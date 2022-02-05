#include "fvdi.h"
#include "relocate.h"
#include "driver.h"
#include "aranym.h"
#include "string/memset.h"

/* Should try to get rid of this! */
#include "os.h"

/* color bit organization */
static char const none[] = { 0 };

static char const r_8[] = { 8 };
static char const g_8[] = { 8 };
static char const b_8[] = { 8 };

static char const r_16[] = { 5, 3, 4, 5, 6, 7 };
static char const g_16[] = { 6, 13, 14, 15, 0, 1, 2 };
static char const b_16[] = { 5, 8, 9, 10, 11, 12 };

static char const r_16f[] = { 5, 11, 12, 13, 14, 15 };
static char const g_16f[] = { 6, 5, 6, 7, 8, 9, 10 };
static char const b_16f[] = { 5, 0, 1, 2, 3, 4 };

static char const r_32[] = { 8, 16, 17, 18, 19, 20, 21, 22, 23 };
static char const g_32[] = { 8,  8,  9, 10, 11, 12, 13, 14, 15 };
static char const b_32[] = { 8,  0,  1,  2,  3,  4,  5,  6,  7 };

static char const r_32f[] = { 8,  8,  9, 10, 11, 12, 13, 14, 15 };
static char const g_32f[] = { 8, 16, 17, 18, 19, 20, 21, 22, 23 };
static char const b_32f[] = { 8, 24, 25, 26, 27, 28, 29, 30, 31 };


/**
 * Mode *graphics_mode
 *
 * bpp     The number of bits per pixel
 *
 * flags   Various information (OR together the appropriate ones)
 *           CHECK_PREVIOUS - Ask fVDI to look at the previous graphics mode
 *                            set by the ROM VDI (I suppose.. *standa*)
 *           CHUNKY         - Pixels are chunky
 *           TRUE_COLOUR    - Pixel value is colour value (no palette)
 *
 * bits    Poperly set up MBits structure:
 *           red, green, blue,  - Pointers to arrays containing the number of
 *           alpa, genlock,       of bits and the corresponding bit numbers
 *           unused               (the latter only for true colour modes)
 *
 * code    Driver dependent value
 *
 * format  Type of graphics mode
 *           0 - interleaved
 *           2 - packed pixels
 *
 * clut    Type of colour look up table
 *           1 - hardware
 *           2 - software
 *
 * org     Pixel bit organization (OR together the appropriate ones)
 *           0x01 - usual bit order
 *           0x80 - Intel byte order
 **/
static Mode const mode[] =                          /* FIXME: big and little endian differences. */
{
    /* ... 0, interleaved, hardware clut, usual bit order */
    {  1, CHECK_PREVIOUS, { r_8, g_8, b_8, none, none, none }, 0, 0, 1, 1 },
    {  2, CHECK_PREVIOUS, { r_8, g_8, b_8, none, none, none }, 0, 0, 1, 1 },
    {  4, CHECK_PREVIOUS, { r_8, g_8, b_8, none, none, none }, 0, 0, 1, 1 },
    {  8, CHECK_PREVIOUS, { r_8, g_8, b_8, none, none, none }, 0, 0, 1, 1 },

    /* ... 0, packed pixels, software clut (none), usual bit order */
    { 16, CHECK_PREVIOUS | CHUNKY | TRUE_COLOUR, { r_16f, g_16f, b_16f, none, none, none }, 0, 2, 2, 1 },
    { 24, CHECK_PREVIOUS | CHUNKY | TRUE_COLOUR, { r_32f, g_32f, b_32f, none, none, none }, 0, 2, 2, 1 },
    { 32, CHECK_PREVIOUS | CHUNKY | TRUE_COLOUR, { r_32, g_32, b_32, none, none, none }, 0, 2, 2, 1 },

    /* ... 0, packed pixels, software clut (none), fb layout */
    { 16, CHECK_PREVIOUS | CHUNKY | TRUE_COLOUR, { r_16, g_16, b_16, none, none, none }, 0, 2, 2, 0x81 },
    { 24, CHECK_PREVIOUS | CHUNKY | TRUE_COLOUR, { r_32, g_32, b_32, none, none, none }, 0, 2, 2, 0x81 },
    { 32, CHECK_PREVIOUS | CHUNKY | TRUE_COLOUR, { r_32, g_32, b_32, none, none, none }, 0, 2, 2, 0x81 }
};

char driver_name[] = "NatFeat/ARAnyM 2005-01-24 (xx bit)";

static struct {
    short used;                         /* Whether the mode option was used or not. */
    short width;
    short height;
    short bpp;
    short freq;
} resolution = {
    0, 640, 480, 16, 85
};

static struct
{
    short width;
    short height;
} pixel;



long CDECL (*write_pixel_r)(Virtual *vwk, MFDB *mfdb, long x, long y, long colour) = c_write_pixel;
long CDECL (*read_pixel_r)(Virtual *vwk, MFDB *mfdb, long x, long y) = c_read_pixel;
long CDECL (*line_draw_r)(Virtual *vwk, long x1, long y1, long x2, long y2, long pattern, long colour, long mode) = c_line_draw;
long CDECL (*expand_area_r)(Virtual *vwk, MFDB *src, long src_x, long src_y, MFDB *dst, long dst_x, long dst_y, long w, long h, long operation, long colour) = c_expand_area;
long CDECL (*fill_area_r)(Virtual *vwk, long x, long y, long w, long h, short *pattern, long colour, long mode, long interior_style) = c_fill_area;
long CDECL (*fill_poly_r)(Virtual *vwk, short points[], long n, short index[], long moves, short *pattern, long colour, long mode, long interior_style) = c_fill_polygon;
long CDECL (*blit_area_r)(Virtual *vwk, MFDB *src, long src_x, long src_y, MFDB *dst, long dst_x, long dst_y, long w, long h, long operation) = c_blit_area;
long CDECL (*text_area_r)(Virtual *vwk, short *text, long length, long dst_x, long dst_y, short *offsets) = c_text_area;
long CDECL (*mouse_draw_r)(Workstation *wk, long x, long y, Mouse *mouse) = c_mouse_draw;

long CDECL (*get_colour_r)(Virtual *vwk, long colour) = c_get_colour_16;
void CDECL (*get_colours_r)(Virtual *vwk, long colour, unsigned long *foreground, unsigned long *background) = c_get_colours_16;
void CDECL (*set_colours_r)(Virtual *vwk, long start, long entries, unsigned short *requested, Colour palette[]) = c_set_colours_16;

long wk_extend = 0;
short accel_s = 0;
short accel_c = A_SET_PIX | A_GET_PIX | A_MOUSE | A_LINE | A_BLIT | A_FILL | A_EXPAND | A_FILLPOLY | A_SET_PAL | A_GET_COL | A_TEXT;
const Mode *graphics_mode = &mode[1];

static short irq = 0;
static short fb_scrninfo;

static long set_mode(const char **ptr);
static long set_scrninfo(const char **ptr);


static Option const options[] = {
    {"mode", { set_mode }, -1 },            /* mode WIDTHxHEIGHTxDEPTH@FREQ */
    {"scrninfo", { set_scrninfo }, -1 },    /* scrninfo fb, make vq_scrninfo return values regarding actual fb layout */
    {"debug", { &debug }, 2 },              /* debug, turn on debugging aids */
    {"irq", { &irq }, 1 },                  /* irq, turn on IRQ handling of events */
};


static char *get_num(char *token, short *num)
{
    char buf[10], c;
    int i;

    *num = -1;
    if (!*token)
        return token;
    for (i = 0; i < 10; i++)
    {
        c = buf[i] = *token++;
        if ((c < '0') || (c > '9'))
            break;
    }
    if (i > 5)
        return token;

    buf[i] = '\0';
    *num = access->funcs.atol(buf);
    return token;
}


static int set_bpp(int bpp)
{
    switch (bpp)
    {
    case 1:
        driver_name[27] = ' ';
        driver_name[28] = '1';
        graphics_mode = &mode[0];
        break;
    case 2:
        driver_name[27] = ' ';
        driver_name[28] = '2';
        graphics_mode = &mode[1];
        break;
    case 4:
        driver_name[27] = ' ';
        driver_name[28] = '4';
        graphics_mode = &mode[2];
        break;
    case 8:
        driver_name[27] = ' ';
        driver_name[28] = '8';
        graphics_mode = &mode[3];
        break;
    default:
        bpp = 16;                       /* Default as 16 bit */
        /* fall through */
    case 16:
        driver_name[27] = '1';
        driver_name[28] = '6';
        graphics_mode = fb_scrninfo ? &mode[7] : &mode[4];
        break;
    case 24:
        driver_name[27] = '2';
        driver_name[28] = '4';
        graphics_mode = fb_scrninfo ? &mode[8] : &mode[5];
        break;
    case 32:
        driver_name[27] = '3';
        driver_name[28] = '2';
        graphics_mode = fb_scrninfo ? &mode[9] : &mode[6];
        break;
    }

#if 0
    /* Update various bitmasks */
    if (bpp > 8)
    {
        long r_mask, g_mask, b_mask;
        long r_shift, g_shift, b_shift;
        long r_loss, g_loss, b_loss;
        int i;

        /* Update R */
        c_get_component(0, &r_mask, &r_shift, &r_loss);
        for (i = 0; i < graphics_mode->bits.red[0]; i++)
        {
            graphics_mode->bits.red[i + 1] = i + r_shift;
        }
        /* Update G */
        c_get_component(1, &g_mask, &g_shift, &g_loss);
        for (i = 0; i < graphics_mode->bits.green[0]; i++)
        {
            graphics_mode->bits.green[i + 1] = i + g_shift;
        }
        /* Update B */
        c_get_component(2, &b_mask, &b_shift, &b_loss);
        for (i = 0; i < graphics_mode->bits.blue[0]; i++)
        {
            graphics_mode->bits.blue[i + 1] = i + b_shift;
        }
    }
#endif

    switch (bpp)
    {
    case 16:
        set_colours_r = c_set_colours_16;
        get_colours_r = c_get_colours_16;
        get_colour_r = c_get_colour_16;
        break;
    case 24:
    case 32:
        set_colours_r = c_set_colours_32;
        get_colours_r = c_get_colours_32;
        get_colour_r = c_get_colour_32;
        break;
    default:
        /* indexed color modes */
        set_colours_r = c_set_colours_8;
        get_colours_r = c_get_colours_8;
        get_colour_r = c_get_colour_8;
        break;
    }

    return bpp;
}


static long set_mode(const char **ptr)
{
    char token[80], *tokenptr;

    if ((*ptr = access->funcs.skip_space(*ptr)) == 0)
    {
        access->funcs.error("missing parameter for mode option", NULL);
    }
    *ptr = access->funcs.get_token(*ptr, token, 80);

    tokenptr = token;
    tokenptr = get_num(tokenptr, &resolution.width);
    tokenptr = get_num(tokenptr, &resolution.height);
    tokenptr = get_num(tokenptr, &resolution.bpp);
    tokenptr = get_num(tokenptr, &resolution.freq);

    resolution.used = 1;

    resolution.bpp = set_bpp(resolution.bpp);

    return 1;
}


static long set_scrninfo(const char **ptr)
{
    char token[80];

    if ((*ptr = access->funcs.skip_space(*ptr)) == 0)
    {
        access->funcs.error("missing parameter for scrninfo option", NULL);
    }
    *ptr = access->funcs.get_token(*ptr, token, 80);

    if (access->funcs.equal(token, "fb"))
    {
        fb_scrninfo = 1;
    } else
    {
        fb_scrninfo = 0;
    }

    resolution.bpp = set_bpp(resolution.bpp);

    if (me && me->device)
        setup_scrninfo(me->device, graphics_mode);

    return 1;
}


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
            case -1:                /* Function call */
                return (options[i].var.func) (ptr);
            case 0:                 /* Default 1, set to 0 */
                *options[i].var.s = 1 - normal;
                return 1;
            case 1:                 /* Default 0, set to 1 */
                *options[i].var.s = normal;
                return 1;
            case 2:                 /* Increase */
                *options[i].var.s += -1 + 2 * normal;
                return 1;
            case 3:
                if ((*ptr = access->funcs.skip_space(*ptr)) == 0)
                {
                    access->funcs.error("missing parameter for ", token);
                }
                *ptr = access->funcs.get_token(*ptr, token, 80);
                *options[i].var.s = token[0];
                return 1;
            }
        }
    }

    return 0;
}


static void setup_wk(Virtual *vwk)
{
    Workstation *wk = vwk->real_address;
    unsigned short *linea;

    /* update the settings */
    wk->screen.mfdb.width = resolution.width;
    wk->screen.mfdb.height = resolution.height;
    wk->screen.mfdb.bitplanes = resolution.bpp;

    /*
     * Some things need to be changed from the
     * default workstation settings.
     */
    wk->screen.mfdb.address = (void *) c_get_videoramaddress();
    wk->screen.mfdb.wdwidth = (wk->screen.mfdb.width + 15) / 16;
    wk->screen.wrap = wk->screen.mfdb.width * (wk->screen.mfdb.bitplanes / 8);

    wk->screen.coordinates.max_x = wk->screen.mfdb.width - 1;
    wk->screen.coordinates.max_y = (wk->screen.mfdb.height & 0xfff0) - 1;   /* Desktop can't deal with non-16N heights */

    wk->screen.look_up_table = 0;       /* Was 1 (???)  Shouldn't be needed (graphics_mode) */
    wk->screen.mfdb.standard = 0;

    if (pixel.width > 0)                /* Starts out as screen width */
        wk->screen.pixel.width = (pixel.width * 1000L) / wk->screen.mfdb.width;
    else                                /*   or fixed DPI (negative) */
        wk->screen.pixel.width = 25400 / -pixel.width;

    if (pixel.height > 0)               /* Starts out as screen height */
        wk->screen.pixel.height = (pixel.height * 1000L) / wk->screen.mfdb.height;
    else                                /*   or fixed DPI (negative) */
        wk->screen.pixel.height = 25400 / -pixel.height;

    device.address = wk->screen.mfdb.address;
    device.byte_width = wk->screen.wrap;

    linea = wk->screen.linea;
    wk->mouse.position.x = linea[-0x25a / 2]; /* GCURX */
    wk->mouse.position.y = linea[-0x258 / 2]; /* GCURY */

    /**
     * The following needs to be here due to bpp > 8 modes where the SDL
     * palette needs the appropriate SDL_surface->format to be set prior
     * use i.e. _after_ the resolution change
     **/
    c_initialize_palette(vwk, 0, wk->screen.palette.size, default_vdi_colors, wk->screen.palette.colours);
}


static void initialize_wk(Virtual *vwk)
{
    Workstation *wk = vwk->real_address;

    if (loaded_palette)
        access->funcs.copymem(loaded_palette, default_vdi_colors, 256 * 3 * sizeof(short));

    /*
     * This code needs more work.
     * Especially if there was no VDI started since before.
     */

    if (wk->screen.palette.size != 256)         /* Started from different graphics mode? */
    {
        Colour *old_palette_colours = wk->screen.palette.colours;

        wk->screen.palette.colours = (Colour *) access->funcs.malloc(256L * sizeof(Colour), 3); /* Assume malloc won't fail. */
        if (wk->screen.palette.colours)
        {
            wk->screen.palette.size = 256;
            if (old_palette_colours)
                access->funcs.free(old_palette_colours);    /* Release old (small) palette (a workaround) */
        } else
        {
            wk->screen.palette.colours = old_palette_colours;
        }
    }

    pixel.width = wk->screen.pixel.width;
    pixel.height = wk->screen.pixel.height;
}


/*
 * Do whatever setup work might be necessary on boot up
 * and which couldn't be done directly while loading.
 * Supplied is the default fVDI virtual workstation.
 */
long CDECL initialize(Virtual *vwk)
{
    vwk = me->default_vwk;              /* This is what we're interested in */

    if (!nf_initialize())
    {
        access->funcs.puts("  No or incompatible NatFeat fVDI!\n");
        return 0;
    }

    initialize_wk(vwk);

    setup_wk(vwk);

    if (debug > 2)
    {
        PRINTF(("  fb_base = $%08lx\n", c_get_videoramaddress()));
    }

    return 1;
}


/*
 *
 */
long CDECL setup(long type, long value)
{
    long ret;

    ret = -1;
    switch ((int) type)
    {
    case Q_NAME:
        ret = (long) driver_name;
        break;
    case S_DRVOPTION:
        ret = tokenize((char *) value);
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
    vwk = me->default_vwk;              /* This is what we're interested in */

    /* switch off VIDEL */
    c_openwk(vwk);

    if (resolution.used)
    {
        resolution.bpp = graphics_mode->bpp;    /* Table value (like rounded down) --- e.g. no 23bit but 16 etc */

        c_set_resolution(resolution.width, resolution.height, resolution.bpp, resolution.freq);
    } else
    {
        /* FIXME: Hack to get it working after boot in less than 16bit */
        resolution.bpp = graphics_mode->bpp;    /* 16 bit by default */
    }

    /* update the width/height if restricted by the native part */
    resolution.width = c_get_width();
    resolution.height = c_get_height();
    resolution.bpp = set_bpp((int) c_get_bpp());

    setup_wk(vwk);

    return 0;
}


/*
 * 'Deinitialize'
 */
void CDECL clswk(Virtual *vwk)
{
    c_closewk(vwk);

    /* Get rid of the event_trampoline! */
}
