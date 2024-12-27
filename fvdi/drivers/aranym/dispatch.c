/*
 *	Drawing function dispatch
 */

#include "fvdi.h"
#include "relocate.h"

/* NF API constants */
#include "fvdidrv_nfapi.h"

#include "driver.h"
#include "aranym.h"
#include "os.h"
#include <stddef.h>

/* NF fVDI ID value */
static long NF_fVDI;


/* general NatFeat stuff */
static long NF_getid = 0x73004e75L;
static long NF_call  = 0x73014e75L;

#define nfGetID(n)	(((long CDECL (*)(const char *))&NF_getid)n)
#define nfCall(n)	(((long CDECL (*)(long, ...))&NF_call)n)

#define verify(x) typedef int verify ## __LINE__[(x) ? 1 : -1]

int nf_initialize(void)
{
	/* ARAnyMs fVDI driver currently relies on some constants */
	verify(offsetof(Virtual, real_address) == 0);
	verify(offsetof(Workstation, screen.type) == 4);
	verify(offsetof(Workstation, screen.mfdb.address) == 24);
	if ((NF_fVDI = nfGetID(("fVDI"))) == 0)
	{
		access->funcs.error("ARAnyM: fVDI native feature not present", NULL);
		return 0;
	}
	if (nfCall((NF_fVDI + FVDI_GET_VERSION)) != FVDIDRV_NFAPI_VERSION)
	{
		access->funcs.error("ARAnyM: incompatible api version", NULL);
		return 0;
	}

	return 1;
}


static MFDB *simplify(Virtual *vwk, MFDB *mfdb)
{
    Workstation *wk;

    vwk = (Virtual *) ((long) vwk & ~1);
    if (!mfdb)
        return 0;
    if (!mfdb->address)
        return 0;
    wk = vwk->real_address;
    /* for offscreen bitmaps, wk->screen.type will be 0 */
    if (wk->screen.type != 0 && mfdb->address == wk->screen.mfdb.address)
        return 0;
    return mfdb;
}


static int is_offscreen_wk(Workstation *wk)
{
    /* for offscreen bitmaps, wk->screen.type will be 0 */
    if (wk->screen.type != 0)
        return 0;
    if (wk->screen.mfdb.address == 0)
        return 0;
    return 1;
}


#if 0
static int is_offscreen_vwk(Virtual *vwk)
{
    vwk = (Virtual *) ((long) vwk & ~1);
    return is_offscreen_wk(vwk->real_address);
}
#endif


static long *clipping(Virtual *vwk, long *rect)
{
    vwk = (Virtual *) ((long) vwk & ~1);

    rect[0] = vwk->clip.rectangle.x1;
    rect[1] = vwk->clip.rectangle.y1;
    rect[2] = vwk->clip.rectangle.x2;
    rect[3] = vwk->clip.rectangle.y2;

    return rect;
}


long CDECL c_read_pixel(Virtual *vwk, MFDB *mfdb, long x, long y)
{
#if 0
    if (x == 999 && y == 999)
    {
        debug = 1;
        access->funcs.puts("aranym.sys: debug on\n");
    } else if (x == 999 && y == 998)
    {
        access->funcs.puts("aranym.sys: debug off\n");
        debug = 0;
    }
#endif
    return nfCall((NF_fVDI + FVDI_GET_PIXEL, vwk, simplify(vwk, mfdb), x, y));
}


long CDECL c_write_pixel(Virtual *vwk, MFDB *mfdb, long x, long y, long colour)
{
    return nfCall((NF_fVDI + FVDI_PUT_PIXEL, vwk, simplify(vwk, mfdb), x, y, colour));
}


long CDECL c_mouse_draw(Workstation *wk, long x, long y, Mouse *mouse)
{
	if (is_offscreen_wk(wk))
		return 0;
    if ((long) mouse > 7)
    {
        unsigned long foreground;
        unsigned long background;
        long *fgbg = (long *) &mouse->colour;

        get_colours_r(me->default_vwk, *fgbg, &foreground, &background);

        /* Need to mask x since it contains old operation in high bits (use that?) */
        return nfCall((NF_fVDI + FVDI_MOUSE, wk, x & 0xffffL, y, &mouse->mask, &mouse->data,
                       (long) mouse->hotspot.x, (long) mouse->hotspot.y, foreground, background, (long) mouse->type));
    } else
    {
        return nfCall((NF_fVDI + FVDI_MOUSE, wk, x & 0xffffL, y, (long) mouse));
    }
}


long CDECL c_expand_area(Virtual *vwk, MFDB *src, long src_x, long src_y,
    MFDB *dst, long dst_x, long dst_y, long w, long h,
    long operation, long colour)
{
    unsigned long foreground;
    unsigned long background;
    MFDB *simple_dst;

    simple_dst = simplify(vwk, dst);
    if (!simple_dst || (dst->bitplanes > 8))
    {
        get_colours_r((Virtual *) ((long) vwk & ~1), colour, &foreground, &background);
    } else
    {
        foreground = colour & 0xff;
        background = (colour >> 16) & 0xff;
    }

    return nfCall((NF_fVDI + FVDI_EXPAND_AREA, vwk, src, src_x, src_y, simple_dst,
                   dst_x, dst_y, w, h, operation, foreground, background));
}


long CDECL c_fill_area(Virtual *vwk, long x, long y, long w, long h, short *pattern,
    long colour, long mode, long interior_style)
{
    unsigned long foreground;
    unsigned long background;

    get_colours_r((Virtual *) ((long) vwk & ~1), colour, &foreground, &background);

    return nfCall((NF_fVDI + FVDI_FILL_AREA, vwk, x, y, w, h, pattern, foreground, background, mode, interior_style));
}


long CDECL c_blit_area(Virtual *vwk, MFDB *src, long src_x, long src_y, MFDB *dst,
    long dst_x, long dst_y, long w, long h, long operation)
{
    return nfCall((NF_fVDI + FVDI_BLIT_AREA, vwk, simplify(vwk, src), src_x, src_y, simplify(vwk, dst), dst_x, dst_y, w, h, operation));
}


long CDECL c_line_draw(Virtual *vwk, long x1, long y1, long x2, long y2,
    long pattern, long colour, long mode)
{
    long rect[4];
    unsigned long foreground;
    unsigned long background;

    get_colours_r((Virtual *) ((long) vwk & ~1), colour, &foreground, &background);

    return nfCall((NF_fVDI + FVDI_LINE, vwk, x1, y1, x2, y2, pattern, foreground, background, mode, (long) clipping(vwk, rect)));
}


long CDECL c_fill_polygon(Virtual *vwk, short points[], long n,
    short index[], long moves, short *pattern,
    long colour, long mode, long interior_style)
{
    long rect[4];
    unsigned long foreground;
    unsigned long background;

    get_colours_r((Virtual *) ((long) vwk & ~1), colour, &foreground, &background);

    return nfCall((NF_fVDI + FVDI_FILL_POLYGON, vwk, points, n, index, moves, pattern,
                   foreground, background, mode, interior_style, (long) clipping(vwk, rect)));
}


long CDECL c_text_area(Virtual *vwk, short *text, long length, long dst_x, long dst_y, short *offsets)
{
    long rect[4];
    unsigned long foreground;
    unsigned long background;
    long *font;
    long w, h, mode;
    long ret;
    long *fgbg;

    if (vwk->text.effects)
        return 0;

    if (offsets)
        return 0;

    font = (long *) vwk->text.current_font->extra.unpacked.data;
    if (!font)                          /* Must have unpacked data */
        return 0;

    w = vwk->text.current_font->widest.cell;    /* Used to be character, which was wrong */
    if (w != 8)                         /* Only that width allowed for now */
        return 0;

    fgbg = (long *) &vwk->text.colour;
    get_colours_r(vwk, *fgbg, &foreground, &background);

    dst_y += (&vwk->text.current_font->extra.distance.base)[vwk->text.alignment.vertical];

    h = vwk->text.current_font->height;

    mode = vwk->mode;

    ret = nfCall((NF_fVDI + FVDI_TEXT_AREA, vwk, text, length, dst_x, dst_y, font, w, h,
                  foreground, background, mode, (long) clipping(vwk, rect)));

    if (ret == -32)                     /* Unknown NatFeat? */
        return 0;

    return ret;
}


long CDECL c_set_colour(Virtual *vwk, long index, long red, long green, long blue)
{
    return nfCall((NF_fVDI + FVDI_SET_COLOR, index, red, green, blue, vwk));
}


long CDECL c_get_hw_colour(short index, long red, long green, long blue, unsigned long *hw_value)
{
    return nfCall((NF_fVDI + FVDI_GET_HWCOLOR, (long) index, red, green, blue, hw_value));
}


long CDECL c_set_resolution(long width, long height, long depth, long frequency)
{
    return nfCall((NF_fVDI + FVDI_SET_RESOLUTION, width, height, depth, frequency));
}


long CDECL c_get_videoramaddress(void)
{
    return nfCall((NF_fVDI + FVDI_GET_FBADDR));
}


long CDECL c_get_width(void)
{
    return nfCall((NF_fVDI + FVDI_GET_WIDTH));
}


long CDECL c_get_height(void)
{
    return nfCall((NF_fVDI + FVDI_GET_HEIGHT));
}


long CDECL c_get_bpp(void)
{
    return nfCall((NF_fVDI + FVDI_GETBPP));
}


long CDECL c_openwk(Virtual *vwk)
{
    (void) vwk;
    return nfCall((NF_fVDI + FVDI_OPENWK));
}


long CDECL c_closewk(Virtual *vwk)
{
    (void) vwk;
    return nfCall((NF_fVDI + FVDI_CLOSEWK));
}


#if 0
void CDECL c_get_component(long component, long *mask, long *shift, long *loss)
{
    nfCall((NF_fVDI + FVDI_GETCOMPONENT, component, mask, shift, loss));
}
#endif


long CDECL event_query(void)
{
    /* Ask native side if it wants to send events */
    return nfCall((NF_fVDI + FVDI_EVENT, 0L));
}


long CDECL event_init(void)
{
    /* Tell native side to start sending events, if possible */
    return nfCall((NF_fVDI + FVDI_EVENT, 1L));
}


void CDECL event_handler(void)
{
    static long event[8];
    int i;

    nfCall((NF_fVDI + FVDI_EVENT, event));  /* Fetch events */
    for (i = 0; i < (int)(sizeof(event) / sizeof(event[0])); i += 2)
    {
        if (!event[i])
            break;
        if ((event[i] & 0xffff) == 5)
        {
            /* Vertical blank */
            me->default_vwk->real_address->vblank.real = 1;
            me->default_vwk->real_address->vblank.frequency = event[i + 1];
        }
        access->funcs.event(((long) me->module.id << 16) | (event[i] & 0xffff), event[i + 1]);
    }
}
