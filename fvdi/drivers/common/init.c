/*
 * fVDI generic device driver initialization, by Johan Klockars
 *
 * Since it would be difficult to do without this file when
 * writing new device drivers, and to make it possible for
 * some such drivers to be commercial, this file is put in
 * the public domain. It's not copyrighted or under any sort
 * of license.
 *
 * I don't expect this file to need many changes for different modes
 * and hardware.
 *
 * This file will of course be extended when new acceleration options
 * and such are added to the fVDI kernel.
 */

#include "os.h"
#include "fvdi.h"
#include "stdio.h"
#include "relocate.h"
#include "driver.h"

short debug = 0;


#define MAX_PALETTE	256
#define Min(x,y)	(((x) <= (y)) ? (x) : (y))



/*
 * Default 'accelerator' functions
 * Will be called automatically from
 * common.s/c_common.s if an accelerator
 * function returns with d0=0.
 */

long CDECL (*fallback_line)(Virtual *vwk, DrvLine *pars);
void CDECL (*fallback_text)(Virtual *vwk, short *text, long length, long dst_x, long dst_y, short *offsets);
void CDECL (*fallback_fill)(Virtual *vwk, long x, long y, long w, long h, short *pattern, long colour, long mode, long interior_style);
void CDECL (*fallback_fillpoly)(Virtual *vwk, short points[], long n, short index[], long moves, short *pattern, long colour, long mode, long interior_style);
void CDECL (*fallback_expand)(Virtual *vwk, MFDB *src, long src_x, long src_y, MFDB *dst, long dst_x, long dst_y, long w, long h, long operation, long colour);
long CDECL (*fallback_blit)(Virtual *vwk, MFDB *src, long src_x, long src_y, MFDB *dst, long dst_x, long dst_y, long w, long h, long operation);

/*
 * Global variables
 */


/*
 * not directly referenced anywhere;
 * but don't declare as static to prevent
 * it from being optimized away
 */
/* static */ Locator locator = { MAGIC, MODULE_IF_VER, init };

Access *access;
short mask[16][4];		/* Allocate instead? */
Driver *me = 0;			/* Access to this seems to be needed */
Device device;
short *loaded_palette = 0;
static unsigned char tos_colours[] = { 0, 255, 1, 2, 4, 6, 3, 5, 7, 8, 9, 10, 12, 14, 11, 13 };
static int accelerate;


long tokenize(const char *ptr)
{
    char token[80], *tmp, ch;

    ptr = access->funcs.skip_space(ptr);
    while (ptr)
    {
        ptr = access->funcs.get_token(ptr, token, 80);
        check_token(token, &ptr);	/* Check driver specific parameters */
        if (access->funcs.equal(token, "accelerate"))
        {
            if ((ptr = access->funcs.skip_space(ptr)) == NULL)
            {
                access->funcs.error("missing parameter for %s", token);
            } else
            {
                ptr = access->funcs.get_token(ptr, token, 80);
                accelerate = 0;
                tmp = token;
                while ((ch = *tmp++) != 0)
                {
                    /* Figure out what things should be accelerated */
                    accelerate <<= 1;
                    if ((ch != '0') && (ch != '-'))
                        accelerate++;
                }
                accelerate &= ACCEL_ALL;
            }
        }
        ptr = access->funcs.skip_space(ptr);
    }

    return 1;
}


void setup_scrninfo(Device *device, const Mode *graphics_mode)
{
    int i;
    const MBits *gmbits;

    device->format = graphics_mode->format;
    device->clut = graphics_mode->clut;
    device->bit_depth = graphics_mode->bpp;
#if 0
    device->byte_width = wk->screen.wrap;
    device->address = wk->screen.mfdb.address;
#endif
    gmbits = &graphics_mode->bits;
    device->bits.red = gmbits->red[0];
    device->bits.green = gmbits->green[0];
    device->bits.blue = gmbits->blue[0];
    device->bits.alpha = gmbits->alpha[0];
    device->bits.genlock = gmbits->genlock[0];
    device->bits.unused = gmbits->unused[0];
    device->bits.organization = graphics_mode->org;
    device->dummy2 = 0;
    if (device->clut == 2)
    {
        int bits = gmbits->red[0] + gmbits->green[0] + gmbits->blue[0];

        device->dummy1 = (1L << bits) >> 16;
        device->colours = (1L << bits) & 0xffff;

        for (i = 0; i < gmbits->red[0]; i++)
            device->scrmap.bitnumber.red[i] = gmbits->red[i + 1];
        for (i = gmbits->red[0]; i < 16; i++)
            device->scrmap.bitnumber.red[i] = -1;		/* Not used */
        for (i = 0; i < gmbits->green[0]; i++)
            device->scrmap.bitnumber.green[i] = gmbits->green[i + 1];
        for (i = gmbits->green[0]; i < 16; i++)
            device->scrmap.bitnumber.green[i] = -1;		/* Not used */
        for (i = 0; i < gmbits->blue[0]; i++)
            device->scrmap.bitnumber.blue[i] = gmbits->blue[i + 1];
        for (i = gmbits->blue[0]; i < 16; i++)
            device->scrmap.bitnumber.blue[i] = -1;		/* Not used */
        for (i = 0; i < gmbits->alpha[0]; i++)
            device->scrmap.bitnumber.alpha[i] = gmbits->alpha[i + 1];
        for (i = gmbits->alpha[0]; i < 16; i++)
            device->scrmap.bitnumber.alpha[i] = -1;		/* Not used */
        for (i = 0; i < gmbits->genlock[0]; i++)
            device->scrmap.bitnumber.genlock[i] = gmbits->genlock[i + 1];
        for (i = gmbits->genlock[0]; i < 16; i++)
            device->scrmap.bitnumber.genlock[i] = -1;	/* Not used */
        for (i = 0; i < gmbits->unused[0]; i++)
            device->scrmap.bitnumber.unused[i] = gmbits->unused[i + 1];
        for (i = gmbits->unused[0]; i < 32; i++)
            device->scrmap.bitnumber.unused[i] = -1;	/* Not used */
        for (i = 0; i < 144; i++)
            device->scrmap.bitnumber.reserved[i] = 0;
    } else
    {
        device->dummy1 = (1L << graphics_mode->bpp) >> 16;
        device->colours = (1L << graphics_mode->bpp) & 0xffff;

        for (i = 0; i < (int) (sizeof(tos_colours) / sizeof(tos_colours[0])); i++)
            device->scrmap.vdi2pix[i] = tos_colours[i];
        if (graphics_mode->bpp == 8)
        {
            for (; i < 255; i++)
                device->scrmap.vdi2pix[i] = i;
            device->scrmap.vdi2pix[255] = 15;
        } else
        {
            for (; i < 256; i++)
                device->scrmap.vdi2pix[i] = 0;
        }
        device->scrmap.vdi2pix[1] = (1 << (graphics_mode->bpp > 8 ? 8 : graphics_mode->bpp)) - 1;
    }
}


extern char _bss_start[];
extern char _edata[];
extern char _end[];


#ifdef __GNUC__
static void *linea_addr(void)
{
	register void *addr __asm__("a0");
	
	__asm__ __volatile(
#ifdef __mcoldfire__
		"\t.dc.w 0xa920\n"
#else
		"\t.dc.w 0xa000\n"
#endif
		: "=r"(addr)
		:
		: "d0", "d1", "d2", "a1", "a2", "cc" AND_MEMORY);
	return addr;
}
#endif


#ifdef __PUREC__
/*
 * note: we have to use d0 here, not a0,
 * because this whole project is compiled with cdecl calling
 */
static void push_a2(void) 0x2F0A;
static void pop_a2(void) 0x245F;
static long get_a0(void) 0x2008; /* move.l a0,d0 */
static void *linea0(void) 0xa000;

static void *CDECL linea_addr(void)
{
	long vars;
	push_a2();
	linea0();
	vars = get_a0();
	pop_a2();
	return (void *)vars;
}
#endif


/*
 * Do all initialization that can be done while loading.
 * Supplied is an access structure for fVDI internals,
 *   the default fVDI virtual workstation and
 *   a pointer to the command line arguments.
 * Return 1 if no error occured.
 */
long CDECL init(Access *_access, Driver *driver, Virtual *vwk, char *opts)
{
    Workstation *wk;
    Virtual *default_vwk = 0;
    Workstation *default_wk = 0;
    Colour *default_palette = 0;

    /*
     * Clear bss segment.
     * Recent versions of the GNU compiler chain allocate initialized variables
     * with a zero value in the BSS segment rather than the data segment,
     * and previous versions of the loader had a bug that failed to clear it.
     * Although that bug is fixed, clearing might still be neccesarry in case
     * this module is loaded by an older version of fvdi.prg.
     * _end and _bss_start are symbols that are generated by the linker.
     * Note that this function is directly called by the loader, therefore
     * we are using the stack of fvdi, so our stack (if any) and thus the
     * local variables of this function are not part of the bss. 
     */
    memset(_bss_start, 0, _end - _bss_start);
    
    access = _access;

    /*
     * Initialize the device structure
     */

    me = driver;                        /* Seems to be needed */

    /*
     * Initialize the device structure
     */

    driver->module.name = driver_name;
    driver->module.initialize = initialize;
    driver->module.setup = setup;
    driver->opnwk = opnwk;
    driver->clswk = clswk;
    driver->default_vwk = 0;            /* Set below */
    driver->device = &device;
    driver->module.priv = 0;


    /*
     * Check device driver options
     */

    accelerate = ACCEL_ALL;             /* Default to everything on */
    tokenize(opts);


    /*
     * Allocate and do initial setup (by copying)
     * of the default device specific workstation/virtual
     */

    if ((default_wk = (Workstation *) access->funcs.malloc(sizeof(Workstation) + wk_extend, 3)) == NULL)
        return 0;

    if ((default_vwk = (Virtual *)access->funcs.malloc(sizeof(Virtual), 3)) == NULL)
    {
        access->funcs.free(default_wk);
        return 0;
    }

    access->funcs.copymem(vwk->real_address, default_wk, sizeof(Workstation));
    access->funcs.copymem(vwk, default_vwk, sizeof(Virtual));

    wk = default_wk;
    default_vwk->real_address = wk;
    driver->default_vwk = default_vwk;


    /*
     * Do some initialization using LineA etc
     * if it can be of use to the driver.
     */
    wk->screen.linea = linea_addr();

    if (graphics_mode->flags & CHECK_PREVIOUS)
    {
        unsigned short *linea;

        /* Sets linea/wrap/width/height/bitplanes */
        linea = wk->screen.linea;
        wk->screen.mfdb.bitplanes = linea[0];        /* PLANES */
        wk->screen.mfdb.width = linea[-0x00c / 2];   /* V_REZ_HZ */
        wk->screen.mfdb.height = linea[-0x004 / 2];  /* V_REZ_HT */
        wk->screen.wrap = linea[-0x002 / 2];         /* BYTES_LIN */

        wk->screen.palette.size = Min(1L << wk->screen.mfdb.bitplanes, MAX_PALETTE);
        wk->screen.mfdb.address = (void *)Physbase();
        wk->screen.mfdb.wdwidth = wk->screen.mfdb.width / 16;

        if ((default_palette = (Colour *) access->funcs.malloc(wk->screen.palette.size * sizeof(Colour), 3)) == NULL)
        {
            access->funcs.free(default_vwk);
            access->funcs.free(default_wk);
            return 0;
        }
        if (wk->screen.palette.colours)
            loaded_palette = (short *) wk->screen.palette.colours;
        wk->screen.palette.colours = default_palette;
    }


    /*
     * Initialize more of the default workstation
     */

    wk->driver = driver;
    wk->screen.mfdb.standard = 0;
    wk->screen.type = 4;
    wk->screen.colour = 1;
    wk->screen.bkg_colours = 0;         /* ? */
    wk->screen.look_up_table = 1;       /* Why?!? */
    wk->screen.palette.possibilities = 0;   /* More than 32767 colours available */
    /* Values and transformation table */
    /* Pixel width/height */
    /* Coordinates (what's 'course'?  max/min should be more sophisticated) */
    wk->screen.coordinates.max_x = wk->screen.mfdb.width - 1;
    wk->screen.coordinates.max_y = wk->screen.mfdb.height - 1;
    /* 16x16 op/s */
    wk->various.buttons = 2;
    wk->various.cursor_movement = 2;
    wk->various.number_entry = 1;
    wk->various.selection = 1;
    wk->various.typing = 1;
    wk->various.workstation_type = 2;


    /*
     * Set up the required and accelerator functions
     */

    if (accel_s & A_SET_PAL)
        wk->r.set_palette = set_palette;
    else
        wk->r.set_palette = c_set_palette;
    if (accel_s & A_GET_COL)
        wk->r.get_colour = colour;
    else
        wk->r.get_colour = c_colour;
    if (accel_s & A_SET_PIX)
        wk->r.set_pixel = set_pixel;
    else
        wk->r.set_pixel = c_set_pixel;
    if (accel_s & A_GET_PIX)
        wk->r.get_pixel = get_pixel;
    else
        wk->r.get_pixel = c_get_pixel;

    if ((accelerate & A_LINE) && line_draw_r)
    {
        fallback_line = wk->r.line;     /* Remember the original (internal) function */
        if (accel_s & A_LINE)           /* Look for assembly... */
            wk->r.line = line;
        else if (accel_c & A_LINE)      /* ...and C accelerator functions */
            wk->r.line = c_line;
    }
    if ((accelerate & A_EXPAND) && expand_area_r)
    {
        fallback_expand = wk->r.expand;
        if (accel_s & A_EXPAND)
            wk->r.expand = expand;
        else if (accel_c & A_EXPAND)
            wk->r.expand = c_expand;
    }
    if ((accelerate & A_FILL) && fill_area_r)
    {
        fallback_fill = wk->r.fill;
        if (accel_s & A_FILL)
            wk->r.fill = fill;
        else if (accel_c & A_FILL)
            wk->r.fill = c_fill;
    }
    if ((accelerate & A_FILLPOLY) && fill_poly_r)
    {
        fallback_fillpoly = wk->r.fillpoly;
        if (accel_s & A_FILLPOLY)
            wk->r.fillpoly = fillpoly;
        else if (accel_c & A_FILLPOLY)
            wk->r.fillpoly = c_fillpoly;
    }
    if ((accelerate & A_BLIT) && blit_area_r)
    {
        fallback_blit = wk->r.blit;
        if (accel_s & A_BLIT)
            wk->r.blit = blit;
        else if (accel_c & A_BLIT)
            wk->r.blit = c_blit;
    }
    if ((accelerate & A_TEXT) && text_area_r)
    {
        fallback_text = wk->r.text;
        if (accel_s & A_TEXT)
            wk->r.text = &text;
        else if (accel_c & A_TEXT)
            wk->r.text = &c_text;
    }
    if (!mouse_draw_r || !((accel_s | accel_c) & A_MOUSE))
    {
        PUTS("driver without mouse drawing no longer supported\n");
        return 0;
    }
    {
        wk->mouse.type = 1;
        if (accel_s & A_MOUSE)
            wk->r.mouse = mouse;
        else if (accel_c & A_MOUSE)
            wk->r.mouse = c_mouse;
    }


    /*
     * Initialize colour number to mask conversion table
     * if it's needed (that is, for bitplane modes).
     */

    if (!(graphics_mode->flags & CHUNKY))
    {
        int i, j, v;

        for (i = 0; i < 16; i++)
        {
            switch (wk->screen.mfdb.bitplanes)
            {
            case 1:
                v = (i & 0x01) ? 0x000f : 0;
                break;
            case 2:
                v = i & 0x03;
                v |= v << 2;
                break;
            default:
                v = i;
                break;
            }
            for (j = 0; j < 4; j++)
            {
                if (v & 0x01)
                    mask[i][j] = 0xffff;
                else
                    mask[i][j] = 0;
                v >>= 1;
            }
        }
    }

    setup_scrninfo(&device, graphics_mode);

    /* Perhaps set up default clipping? */

    return 1;
}
