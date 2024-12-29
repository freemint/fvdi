/*
 * fb_spec.c - Specification/initialization file
 * This is part of the FireBee driver for fVDI
 * Derived from Johan Klockars's example in ../16_bit/16b_spec.c
 *
 * Copyright (C) 2017 Vincent Riviere
 * Copyright (C) 2020 Markus Fröschle
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "fvdi.h"
#include "driver.h"
#include "firebee.h"
#include "fb_video.h"
#include "modeline.h"
#include <os.h>
#include "string/memset.h"
#include <stdint.h>

/* rrrrrggg gggbbbbb */
static char const r_16[] = { 5, 11, 12, 13, 14, 15 };
static char const g_16[] = { 6,  5,  6,  7,  8,  9, 10 };
static char const b_16[] = { 5,  0,  1,  2,  3,  4 };
static char const none[] = { 0 };

static Mode const mode[1] = {
    { 16, CHUNKY | CHECK_PREVIOUS | TRUE_COLOUR, { r_16, g_16, b_16, none, none, none }, 0,  2, 2, 1 }
};

char driver_name[] = "FBee";

static struct res
{
    short used; /* Whether the mode option was used or not. */
    short width;
    short height;
    short bpp;
    short freq;
} resolution = {
    0, 640, 480, 16, 71
};

static struct {
    short width;
    short height;
} pixel;

long CDECL (*write_pixel_r)(Virtual *vwk, MFDB *mfdb, long x, long y, long colour) = c_write_pixel;
long CDECL (*read_pixel_r)(Virtual *vwk, MFDB *mfdb, long x, long y) = c_read_pixel;
long CDECL (*line_draw_r)(Virtual *vwk, long x1, long y1, long x2, long y2, long pattern, long colour, long mode) = c_line_draw;
long CDECL (*expand_area_r)(Virtual *vwk, MFDB *src, long src_x, long src_y, MFDB *dst, long dst_x, long dst_y, long w, long h, long operation, long colour) = c_expand_area;
long CDECL (*fill_area_r)(Virtual *vwk, long x, long y, long w, long h, short *pattern, long colour, long mode, long interior_style) = c_fill_area;
long CDECL (*fill_poly_r)(Virtual *vwk, short points[], long n, short index[], long moves, short *pattern, long colour, long mode, long interior_style) = 0;
long CDECL (*blit_area_r)(Virtual *vwk, MFDB *src, long src_x, long src_y, MFDB *dst, long dst_x, long dst_y, long w, long h, long operation) = c_blit_area;
long CDECL (*text_area_r)(Virtual *vwk, short *text, long length, long dst_x, long dst_y, short *offsets) = 0;
long CDECL (*mouse_draw_r)(Workstation *wk, long x, long y, Mouse *mouse) = c_mouse_draw;

long CDECL (*get_colour_r)(Virtual *vwk, long colour) = c_get_colour;
void CDECL (*get_colours_r)(Virtual *vwk, long colour, unsigned long *foreground, unsigned long *background) = 0;
void CDECL (*set_colours_r)(Virtual *vwk, long start, long entries, unsigned short *requested, Colour palette[]) = c_set_colours;

static void fbee_puts(const char* message)
{
    access->funcs.puts("FBee: ");
    access->funcs.puts(message);
    access->funcs.puts("\x0d\x0a");
}

static void panic(const char* message)
{
    /* Unfortunately, fVDI can't recover from driver initialization failure.
     * So we just wait forever. */
    fbee_puts(message);
    for(;;);
}

long wk_extend = 0;

short accel_s = 0;
short accel_c = A_SET_PAL | A_GET_COL | A_SET_PIX | A_GET_PIX | A_BLIT | A_FILL | A_EXPAND | A_LINE |  A_MOUSE;

const Mode *graphics_mode = &mode[0];
struct modeline modeline;

static char *get_num(char *token, short *num)
{
    char buf[10], c;
    int i;

    *num = -1;
    if (!*token)
        return token;
    for(i = 0; i < 10; i ++) {
        c = buf[i] = *token++;
        if ((c < '0') || (c > '9'))
            break;
    }
    if (i > 5)
        return token;

    buf[i] = '\0';
    *num = (short) access->funcs.atol(buf);
    return token;
}

static short set_bpp(short bpp)
{
    switch (bpp) {
        case -1:
        case 16:
            graphics_mode = &mode[0];
            break;
        default:
            panic("Unsupported BPP.");
            break;
    }

    return bpp;
}


static long calc_modeline(struct res *res, struct modeline *ml)
{
    /*
     * round down horizontal resolution to closest multiple of 8. Otherwise we get staircases
     */
    res->width &= ~7;

    /*
     * translate the resolution information we got from FVDI.SYS into proper
     * video timing (a modeline)
     */
    general_timing_formula(res->width, res->height, res->freq, 0.0, ml);

    PRINTF(("pixel clock: %d\r\n", (int) ml->pixel_clock));
    PRINTF(("hres: %d\r\n", ml->h_display));
    PRINTF(("hsync start: %d\r\n", ml->h_sync_start));
    PRINTF(("hsync end: %d\r\n", ml->h_sync_end));
    PRINTF(("htotal: %d\r\n", ml->h_total));
    PRINTF(("vres: %d\r\n", ml->v_display));
    PRINTF(("vsync start: %d\r\n", ml->v_sync_start));
    PRINTF(("vsync end: %d\r\n", ml->v_sync_end));
    PRINTF(("vtotal: %d\r\n", ml->v_total));
    PRINTF(("\r\n"));

    return 1;

}

static long set_mode(const char **ptr)
{
    char token[80], *tokenptr;

    if ((*ptr = access->funcs.skip_space(*ptr)) == NULL)
    {
        ;		/* *********** Error, somehow */
    }
    *ptr = access->funcs.get_token(*ptr, token, 80);

    tokenptr = token;
    tokenptr = get_num(tokenptr, &resolution.width);
    tokenptr = get_num(tokenptr, &resolution.height);
    tokenptr = get_num(tokenptr, &resolution.bpp);
    tokenptr = get_num(tokenptr, &resolution.freq);

    resolution.used = 1;

    resolution.bpp = (short) set_bpp(resolution.bpp);

    return 1;
}


static Option const options[] = {
    { "debug",      { &debug },             2 },  /* debug, turn on debugging aids */
    { "mode",       { (void *) &set_mode },          -1 },  /* mode WIDTHxHEIGHTxDEPTH@FREQ */
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
    switch (token[0]) {
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
    for(i = 0; i < (int)(sizeof(options) / sizeof(Option)); i++) {
        if (access->funcs.equal(xtoken, options[i].name)) {
            switch (options[i].type) {
                case -1:     /* Function call */
                    return (options[i].var.func)(ptr);
                case 0:      /* Default 1, set to 0 */
                    *options[i].var.s = 1 - normal;
                    return 1;
                case 1:     /* Default 0, set to 1 */
                    *options[i].var.s = normal;
                    return 1;
                case 2:     /* Increase */
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

void *screen_address;


/* Allocate screen buffer */
static short *fbee_alloc_vram(short width, short height, short depth)
{
    void *buffer;

    /* FireBee screen buffers live in ST RAM */
    buffer = (void *) Mxalloc((long) width * height * depth + 255, MX_STRAM);
    if (buffer == NULL)
        panic("Mxalloc() failed to allocate screen buffer.");

    screen_address = (void *) ((((uint32_t) buffer + 255UL) & ~255UL));

    PRINTF(("screen buffer allocated at 0x%lx\r\n", (long) buffer));
    return screen_address;
}

/*
 * Do whatever setup work might be necessary on boot up
 * and which couldn't be done directly while loading.
 * Supplied is the default fVDI virtual workstation.
 */
long CDECL initialize(Virtual *vwk)
{
    Workstation *wk;
    int old_palette_size;
    Colour *old_palette_colours;

    /* Display startup banner */
    access->funcs.puts("\r\n");
    access->funcs.puts("\ep FireBee driver for fVDI \eq\r\n");
    access->funcs.puts("\xbd 2019 Markus Fr\x94schle\r\n");
    access->funcs.puts("Free Software distributed under GPLv2\r\n");
    access->funcs.puts("\r\n");

    calc_modeline(&resolution, &modeline);
    PRINTF(("%d x %d x %d@%d\r\n", modeline.h_display, modeline.v_display, resolution.bpp, modeline.pixel_clock));
    screen_address = fbee_alloc_vram(resolution.width, resolution.height, sizeof(short));

    vwk = me->default_vwk;	/* This is what we're interested in */
    wk = vwk->real_address;

    wk->screen.look_up_table = 0;               /* Was 1 (???)  Shouldn't be needed (graphics_mode) */
    wk->screen.mfdb.standard = 0;

    if (wk->screen.pixel.width > 0)             /* Starts out as screen width */
        wk->screen.pixel.width = (wk->screen.pixel.width * 1000L) / wk->screen.mfdb.width;
    else                                        /*   or fixed DPI (negative) */
        wk->screen.pixel.width = 25400 / -wk->screen.pixel.width;

    if (wk->screen.pixel.height > 0)            /* Starts out as screen height */
        wk->screen.pixel.height = (wk->screen.pixel.height * 1000L) / wk->screen.mfdb.height;
    else                                        /*   or fixed DPI (negative) */
        wk->screen.pixel.height = 25400 / -wk->screen.pixel.height;


    /*
     * This code needs more work.
     * Especially if there was no VDI started since before.
     */

    /* replace default VDI colors with loaded palette, if available */
    if (loaded_palette)
        access->funcs.copymem(loaded_palette, default_vdi_colors, 256 * 3 * sizeof(short));

    if ((old_palette_size = wk->screen.palette.size) != 256)    /* Started from different graphics mode? */
    {
        old_palette_colours = wk->screen.palette.colours;

        wk->screen.palette.colours = (Colour *) access->funcs.malloc(256L * sizeof(Colour), 3);	/* Assume malloc won't fail. */
        if (wk->screen.palette.colours)
        {
            wk->screen.palette.size = 256;
            if (old_palette_colours)
                access->funcs.free(old_palette_colours);	/* Release old (small) palette (a workaround) */
        } else
            wk->screen.palette.colours = old_palette_colours;
    }
    c_initialize_palette(vwk, 0, wk->screen.palette.size, default_vdi_colors, wk->screen.palette.colours);

    device.byte_width = wk->screen.wrap;
    device.address = wk->screen.mfdb.address;

    pixel.width = wk->screen.pixel.width;
    pixel.height = wk->screen.pixel.height;

    return 1;
}

/*
 *
 */
long CDECL setup(long type, long value)
{
    long ret;

    ret = -1;
    switch(type) {
        case Q_NAME:
            ret = (long) driver_name;
            break;

        case S_DRVOPTION:
            ret = tokenize((char *)value);
            break;
    }

    return ret;
}

/*
 * Initialize according to parameters (boot and sent).
 * Create new (or use old) Workstation and default Virtual.
 * Supplied is the default fVDI virtual workstation.
 */
Virtual* CDECL opnwk(Virtual *vwk)
{
    Workstation *wk;
    vwk = me->default_vwk;  /* This is what we're interested in */
    wk = vwk->real_address;

    fbee_set_video(screen_address + FB_VRAM_PHYS_OFFSET);

    PRINTF(("selected resolution is %dx%dx%d@%d\r\n", resolution.width, resolution.height, resolution.bpp, resolution.freq));

    wk->screen.mfdb.width = resolution.width;
    wk->screen.mfdb.height = resolution.height;
    wk->screen.mfdb.bitplanes = resolution.bpp;

    /*
     * Some things need to be changed from the
     * default workstation settings.
     */
    wk->screen.mfdb.address =  screen_address;
    PRINTF(("%d x %d x %d screen at 0x%lx\r\n", wk->screen.mfdb.width, wk->screen.mfdb.height,
            wk->screen.mfdb.bitplanes, (long) screen_address));

    wk->screen.mfdb.wdwidth = (wk->screen.mfdb.width + 15) / 16;
    wk->screen.wrap = wk->screen.mfdb.width * (wk->screen.mfdb.bitplanes / 8);

    wk->screen.coordinates.max_x = wk->screen.mfdb.width - 1;
    wk->screen.coordinates.max_y = wk->screen.mfdb.height - 1;

    wk->screen.look_up_table = 0;           /* Was 1 (???)	Shouldn't be needed (graphics_mode) */
    wk->screen.mfdb.standard = 0;

    if (pixel.width > 0)                    /* Starts out as screen width */
        wk->screen.pixel.width = (pixel.width * 1000L) / wk->screen.mfdb.width;
    else                                    /* or fixed DPI (negative) */
        wk->screen.pixel.width = 25400 / -pixel.width;

    if (pixel.height > 0)                   /* Starts out as screen height */
        wk->screen.pixel.height = (pixel.height * 1000L) / wk->screen.mfdb.height;
    else                                    /*	 or fixed DPI (negative) */
        wk->screen.pixel.height = 25400 / -pixel.height;

    wk->mouse.position.x = ((wk->screen.coordinates.max_x - wk->screen.coordinates.min_x + 1) >> 1) + wk->screen.coordinates.min_x;
    wk->mouse.position.y = ((wk->screen.coordinates.max_y - wk->screen.coordinates.min_y + 1) >> 1) + wk->screen.coordinates.min_y;

    /*
     * FIXME
     *
     * for some strange reason, the driver only works if the palette is (re)initialized here again.
     * Otherwise, everything is drawn black on black (not very useful).
     *
     * I did not yet find what I'm doing differently from other drivers (that apparently do not have this problem).
     */
    c_initialize_palette(vwk, 0, wk->screen.palette.size, default_vdi_colors, wk->screen.palette.colours);

    return 0;
}

/*
 * 'Deinitialize'
 */
void CDECL clswk(Virtual *vwk)
{
    (void) vwk;
}
