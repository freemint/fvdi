/*
 * fVDI device driver specific setup
 *
 * Copyright 1998-2002, Johan Klockars
 * This software is licensed under the GNU General Public License.
 * Please, see LICENSE.TXT for further information.
 */

#include "fvdi.h"
#include "relocate.h"

#include "os.h"
#include "driver.h"
#include "bitplane.h"
#include "string/memset.h"

#if 0
#define FAST		/* Write in FastRAM buffer */
#define BOTH		/* Write in both FastRAM and on screen */
#else
#undef FAST
#undef BOTH
#endif


static char const red[] = { 6 };
static char const green[] = { 6 };
static char const blue[] = { 6 };
static char const none[] = { 0 };

static Mode const mode[4] = {
    { 1, CHECK_PREVIOUS, { red, green, blue, none, none, none }, 0, 0, 1, 1 },
    { 2, CHECK_PREVIOUS, { red, green, blue, none, none, none }, 0, 0, 1, 1 },
    { 4, CHECK_PREVIOUS, { red, green, blue, none, none, none }, 0, 0, 1, 1 },
    { 8, CHECK_PREVIOUS, { red, green, blue, none, none, none }, 0, 0, 1, 1 }
};

char driver_name[] = "Bitplane (shadow)";

long CDECL(*write_pixel_r) (Virtual *vwk, MFDB * mfdb, long x, long y, long colour) = c_write_pixel;
long CDECL(*read_pixel_r) (Virtual *vwk, MFDB * mfdb, long x, long y) = c_read_pixel;
long CDECL(*line_draw_r) (Virtual *vwk, long x1, long y1, long x2, long y2, long pattern, long colour, long mode) = c_line_draw;
long CDECL(*expand_area_r) (Virtual *vwk, MFDB * src, long src_x, long src_y, MFDB * dst, long dst_x, long dst_y, long w, long h, long operation, long colour) = c_expand_area;
long CDECL(*fill_area_r) (Virtual *vwk, long x, long y, long w, long h, short *pattern, long colour, long mode, long interior_style) = c_fill_area;
long CDECL(*fill_poly_r) (Virtual *vwk, short points[], long n, short index[], long moves, short *pattern, long colour, long mode, long interior_style) = 0;
long CDECL(*blit_area_r) (Virtual *vwk, MFDB * src, long src_x, long src_y, MFDB * dst, long dst_x, long dst_y, long w, long h, long operation) = c_blit_area;
long CDECL(*text_area_r) (Virtual *vwk, short *text, long length, long dst_x, long dst_y, short *offsets) = 0;
long CDECL(*mouse_draw_r) (Workstation *wk, long x, long y, Mouse * mouse) = c_mouse_draw;

long CDECL(*get_colour_r) (Virtual *vwk, long colour) = c_get_colour;
void CDECL(*get_colours_r) (Virtual *vwk, long colour, unsigned long *foreground, unsigned long *background) = 0;
void CDECL(*set_colours_r) (Virtual *vwk, long start, long entries, unsigned short *requested, Colour palette[]) = c_set_colours;

long wk_extend = 0;
short accel_s = 0;
short accel_c = A_SET_PAL | A_GET_COL | A_SET_PIX | A_GET_PIX | A_BLIT | A_FILL | A_EXPAND | A_LINE | A_MOUSE;

const Mode *graphics_mode = &mode[0];

short shadow = 0;
short fix_shape = 0;
short no_restore = 0;
short depth = 0;


static Option const options[] = {
    { "debug",      { &debug },             2 },  /* debug, turn on debugging aids */
    { "shadow",     { &shadow },            0 },  /* Use a separate buffer of the screen in RAM */
    { "fixshape",   { &fix_shape },         0 },  /* fixed shape; do not allow mouse shape changes */
    { "norestore",  { &no_restore },        0 },
};

/*
 * Handle any driver specific parameters
 */
long check_token(char *token, const char **ptr)
{
    int i;
    int normal;
    char *xtoken;

    xtoken = token;
    switch (token[0])
    {
    case '+':
        xtoken++;
        normal = 1;
        break;
    case '-':
        xtoken++;
        normal = 0;
        break;
    default:
        normal = 1;
        break;
    }
    for (i = 0; i < (int)(sizeof(options) / sizeof(Option)); i++)
    {
        if (access->funcs.equal(xtoken, options[i].name))
        {
            switch (options[i].type)
            {
            case -1:      /* Function call */
                return (options[i].var.func)(ptr);
            case 0:        /* Default 1, set to 0 */
                *options[i].var.s = 1 - normal;
                return 1;
            case 1:      /* Default 0, set to 1 */
                *options[i].var.s = normal;
                return 1;
            case 2:      /* Increase */
                *options[i].var.s += -1 + 2 * normal;
                return 1;
            case 3:
                if ((*ptr = access->funcs.skip_space(*ptr)) == NULL)
                {
                    ;  /* *********** Error, somehow */
                }
                *ptr = access->funcs.get_token(*ptr, token, 80);
                *options[i].var.s = token[0];
                return 1;
            }
        }
    }

    return 0;
}


/*
 * Do whatever setup work might be necessary on boot up
 * and which couldn't be done directly while loading.
 * Supplied is the default fVDI virtual workstation.
 */
long initialize(Virtual *vwk)
{
#ifdef __mcoldfire__
    /* use screenpt system variable on Firebee as Physbase() doesn't work there (see below FIXME) */
    short** screenpt = (short **) 0x45e;
#endif
    Workstation *wk;
#ifdef FAST
    char *buf;
    int fast_w_bytes;
#endif

    debug = access->funcs.misc(0, 1, 0);

    vwk = me->default_vwk;              /* This is what we're interested in */
    wk = vwk->real_address;

#ifdef FAST
    if (shadow)
    {
#if 0                                   /* It's not clear that this is a good idea */
        fast_w_bytes = (wk->screen.wrap + 15) & 0xfffffff0;
#else
        fast_w_bytes = wk->screen.wrap;
#endif
        buf = (char *) access->funcs.malloc(fast_w_bytes * wk->screen.mfdb.height + 255, 1);
        if (buf)
        {
            wk->screen.shadow.buffer = buf;
            wk->screen.shadow.address = (void *) (((long) buf + 255) & 0xffffff00);
            wk->screen.shadow.wrap = fast_w_bytes;
        } else
        {
            access->funcs.error("Can't allocate FastRAM!", 0);
            wk->screen.shadow.buffer = 0;
            wk->screen.shadow.address = 0;
        }
#ifndef BOTH
        wk->screen.mfdb.address = wk->screen.shadow.address;
#endif
    }
#endif
    if (!wk->screen.shadow.address)
        driver_name[10] = 0;

    if (wk->screen.pixel.width > 0)     /* Starts out as screen width */
        wk->screen.pixel.width = (wk->screen.pixel.width * 1000L) / wk->screen.mfdb.width;
    else                                /*   or fixed DPI (negative) */
        wk->screen.pixel.width = 25400 / -wk->screen.pixel.width;
    if (wk->screen.pixel.height > 0)    /* Starts out as screen height */
        wk->screen.pixel.height = (wk->screen.pixel.height * 1000L) / wk->screen.mfdb.height;
    else                                /*   or fixed DPI (negative) */
        wk->screen.pixel.height = 25400 / -wk->screen.pixel.height;

    device.byte_width = wk->screen.wrap;

#ifdef __mcoldfire__
    /*
     * FIXME: work around the problem that the FireBee screen base registers can't be
     * read currently (FPGA bug).
     * This has the (fatal) effect that a Physbase() call returns NULL on the FireBee.
     * The workaround is to use the corresponding system variable
     */
    wk->screen.mfdb.address = *screenpt;
#else
    wk->screen.mfdb.address = (short *) Physbase();
#endif /* __mcoldfire__ */

    device.address = wk->screen.mfdb.address;

    switch (wk->screen.mfdb.bitplanes)
    {
    case 1:
        graphics_mode = &mode[0];
        break;
    case 2:
        graphics_mode = &mode[1];
        break;
    case 4:
        graphics_mode = &mode[2];
        break;
    case 8:
        graphics_mode = &mode[3];
        break;
    default:
        access->funcs.puts("Unsupported BPP.\n");
        return 0;
    }
    setup_scrninfo(me->device, graphics_mode);

    PRINTF(("%dx%dx%d screen at %08lx\n", wk->screen.mfdb.width, wk->screen.mfdb.height, wk->screen.mfdb.bitplanes,
            (long) wk->screen.mfdb.address));

    return 1;
}

/*
 *
 */
long setup(long type, long value)
{
    long ret;

    (void) value;
    ret = -1;
    switch (type)
    {
#if 0
    case S_SCREEN:
        if (value != -1)
            old_screen = value;
        ret = old_screen;
        break;
    case S_AESBUF:
        if (value != -1)
            aes_buffer = value;
        ret = aes_buffer;
        break;
    case S_DOBLIT:
        if (value != -1)
            blit_fall_back = value;
        ret = blit_fall_back;
        break;
    case S_CACHEIMG:
        if (value != -1)
            cache_img = value;
        ret = cache_img;
        break;
#endif
    case Q_NAME:
        ret = (long) driver_name;
        break;
    }

    return ret;
}

/*
 * Initialize according to parameters (boot and sent).
 * Create new (or use old) Workstation and default Virtual.
 * Supplied is the default fVDI virtual workstation.
 */
Virtual *CDECL opnwk(Virtual *vwk)
{
    Workstation *wk;
    unsigned short *linea;

    (void) vwk;
    wk = me->default_vwk->real_address;
    wk->screen.mfdb.address = (short *) Logbase();

    linea = wk->screen.linea;
    wk->mouse.position.x = linea[-0x25a / 2]; /* GCURX */
    wk->mouse.position.y = linea[-0x258 / 2]; /* GCURY */

    return 0;
}

/*
 * 'Deinitialize'
 */
void CDECL clswk(Virtual *vwk)
{
    (void) vwk;
}
