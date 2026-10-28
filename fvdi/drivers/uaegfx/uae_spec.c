/*
 * uae_spec.c - Specification/initialization file
 * This is part of the WinUAE RTG driver for fVDI
 * Derived from Johan Klockars's example in ../16_bit/16b_spec.c
 *
 * Copyright (C) 2017 Vincent Riviere
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

/*#define ENABLE_KDEBUG*/

#include "fvdi.h"
#include "driver.h"
#include "relocate.h"
#include "uaegfx.h"
#include "uaelib.h"
#include "string/memset.h"

static char const r_16[] = { 5, 11, 12, 13, 14, 15 };
static char const g_16[] = { 6,  5,  6,  7,  8,  9, 10 };
static char const b_16[] = { 5,  0,  1,  2,  3,  4 };
static char const none[] = { 0 };

static Mode const mode[1] = {
    { 16, CHUNKY | CHECK_PREVIOUS | TRUE_COLOUR, {r_16, g_16, b_16, none, none, none}, 0,  2, 2, 1 }
};

char driver_name[] = "uaegfx.card";

static struct {
	short used; /* Whether the mode option was used or not. */
	short width;
	short height;
	short bpp;
	short freq;
} resolution = {0, 640, 480, 16, 60};

struct {
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

long wk_extend = 0;

short accel_s = 0;
short accel_c = A_SET_PAL | A_GET_COL | A_SET_PIX | A_GET_PIX | A_BLIT | A_FILL | A_EXPAND | A_LINE | A_MOUSE;

const Mode *graphics_mode = &mode[0];


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
	*num = access->funcs.atol(buf);
	return token;
}


static int set_bpp(int bpp)
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

	resolution.bpp = set_bpp(resolution.bpp);

	return 1;
}

static Option const options[] = {
	{"debug",      { &debug },             2},  /* debug, turn on debugging aids */
	{"mode",       { set_mode },          -1},  /* mode WIDTHxHEIGHTxDEPTH@FREQ */
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

/* This BoardInfo will be passed to all Picasso96 functions */
struct BoardInfo my_BoardInfo;
static struct BitMapExtra bme;

/* We only support this pixel format */
RGBFTYPE my_RGBFormat = RGBFB_R5G6B5;

/* Matching video mode found during enumeration */
static struct LibResolution* my_LibResolution = NULL;

/* Initialize the RTG card with the requested video mode */
static int init_rtg_card(void)
{
	long ret;
	struct MinNode *minnode;

	/* Initialize the UAE Library */
	if (!uaelib_init()) {
		panic_help("Function uaelib_demux() not found.");
		return 0;
	}

	/* Initialize the BoardInfo struct before calling the Picasso96 API */
	BOARDINFO_MEMBER(struct BitMapExtra*, BitMapExtra) = &bme;
	NewMinList(&BOARDINFO_MEMBER(struct MinList, ResolutionsList));

	/* Find the card. This only succeeds if an RTG graphics card is enabled
	 * in WinUAE settings. */
	ret = uaelib_picasso_FindCard(&my_BoardInfo);
	if (ret != -1) {
		panic_help("picasso_FindCard() failed.");
		return 0;
	}

	KINFO(("UAEGFX: BoardInfo=%p MemoryBase=%p MemorySize=%lu\n",
		&my_BoardInfo,
		BOARDINFO_MEMBER(UBYTE*, MemoryBase),
		BOARDINFO_MEMBER(ULONG, MemorySize)
	));

	/* Initialize the card. This populates the ResolutionsList. */
	ret = uaelib_picasso_InitCard(&my_BoardInfo);
	if (ret != -1) {
		panic("picasso_InitCard() failed.");
		return 0;
	}

	/* Warning: The card must have been properly configured by the OS with
	 * AUTOCONFIG. Otherwise, the RTG memory is not mapped, and the loop
	 * below will not find anything. */
	ForEachMinNode(&BOARDINFO_MEMBER(struct MinList, ResolutionsList), minnode)
	{
		struct LibResolution *rez = (struct LibResolution*)minnode;

		KDEBUG(("%p LibResolution %dx%d %s\n", rez, rez->Width, rez->Height, rez->Name));

		if (rez->Width == (UWORD)resolution.width && rez->Height == (UWORD)resolution.height) {
			my_LibResolution = rez;
			KDEBUG(("Requested video mode %dx%d found\n", resolution.width, resolution.height));
			return 1;
		}
	}

	panic("Requested video mode not available.");
	return 0;
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

	KDEBUG(("UAEGFX: initialize vwk=%p\n", vwk));

	/* Display startup banner */
	access->funcs.puts("\r\n");
	access->funcs.puts("\ep WinUAE RTG driver for fVDI \eq\r\n");
	access->funcs.puts("\xbd 2017 Vincent Rivi\x8are\r\n");
	access->funcs.puts("Free Software distributed under GPLv2\r\n");
	access->funcs.puts("\r\n");

	/* Initialize the RTG card with the requested video mode */
	if (!init_rtg_card())
		return 0;

	vwk = me->default_vwk;	/* This is what we're interested in */	
	wk = vwk->real_address;

	wk->screen.look_up_table = 0;			/* Was 1 (???)  Shouldn't be needed (graphics_mode) */
	wk->screen.mfdb.standard = 0;
	/* Unconverted fvdi.sys values, opnwk() converts them for the final resolution */
	pixel.width = wk->screen.pixel.width;
	pixel.height = wk->screen.pixel.height;
	if (wk->screen.pixel.width > 0)        /* Starts out as screen width */
		wk->screen.pixel.width = (wk->screen.pixel.width * 1000L) / wk->screen.mfdb.width;
	else                                   /*   or fixed DPI (negative) */
		wk->screen.pixel.width = 25400 / -wk->screen.pixel.width;
	if (wk->screen.pixel.height > 0)        /* Starts out as screen height */
		wk->screen.pixel.height = (wk->screen.pixel.height * 1000L) / wk->screen.mfdb.height;
	else                                    /*   or fixed DPI (negative) */
		wk->screen.pixel.height = 25400 / -wk->screen.pixel.height;


	/*	
	 * This code needs more work.
	 * Especially if there was no VDI started since before.
	 */

	if (loaded_palette)
		access->funcs.copymem(loaded_palette, default_vdi_colors, 256 * 3 * sizeof(short));
	if ((old_palette_size = wk->screen.palette.size) != 256) {	/* Started from different graphics mode? */
		old_palette_colours = wk->screen.palette.colours;
		wk->screen.palette.colours = (Colour *)access->funcs.malloc(256L * sizeof(Colour), 3);	/* Assume malloc won't fail. */
		if (wk->screen.palette.colours) {
			wk->screen.palette.size = 256;
			if (old_palette_colours)
				access->funcs.free(old_palette_colours);	/* Release old (small) palette (a workaround) */
		} else
			wk->screen.palette.colours = old_palette_colours;
	}
	c_initialize_palette(vwk, 0, wk->screen.palette.size, default_vdi_colors, wk->screen.palette.colours);

	device.byte_width = wk->screen.wrap;
	device.address = wk->screen.mfdb.address;
	KDEBUG(("VDI pixel size = %dx%d\n", wk->screen.pixel.width, wk->screen.pixel.height));

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
		ret = (long)driver_name;
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
	struct ModeInfo* my_ModeInfo = my_LibResolution->Modes[P96_HICOLOR];
	Workstation *wk;
	UBYTE* screen_address = BOARDINFO_MEMBER(UBYTE*, MemoryBase);
	uae_u32 ret;

	KDEBUG(("UAEGFX: opnwk vwk=%p\n", vwk));

	KINFO(("screen_address = %p\n", screen_address));

	vwk = me->default_vwk;  /* This is what we're interested in */
	wk = vwk->real_address;

	/* Set the video mode we found in init_rtg_card() */
	ret = uaelib_picasso_SetGC(&my_BoardInfo, my_ModeInfo, 0);
	if (!ret)
		panic("picasso_SetGC() failed.");

	/* Set proper display area */
	bme.Width = my_LibResolution->Width;
	bme.Height = my_LibResolution->Height;
	ret = uaelib_picasso_SetPanning(&my_BoardInfo, screen_address, my_LibResolution->Width, 0, 0, my_RGBFormat);
	if (!ret)
		panic("picasso_SetPanning() failed.");

	/* update the settings */
	wk->screen.mfdb.width = my_ModeInfo->Width;
	wk->screen.mfdb.height = my_ModeInfo->Height;
	wk->screen.mfdb.bitplanes = my_ModeInfo->Depth;

	/*
	 * Some things need to be changed from the
	 * default workstation settings.
	 */
	wk->screen.mfdb.address = (short *)screen_address;
	wk->screen.mfdb.wdwidth = (wk->screen.mfdb.width + 15) / 16;
	wk->screen.wrap = wk->screen.mfdb.width * (wk->screen.mfdb.bitplanes / 8);

	wk->screen.coordinates.max_x = wk->screen.mfdb.width - 1;
	wk->screen.coordinates.max_y = wk->screen.mfdb.height - 1;

	wk->screen.look_up_table = 0;			/* Was 1 (???)	Shouldn't be needed (graphics_mode) */
	wk->screen.mfdb.standard = 0;

	if (pixel.width > 0)			/* Starts out as screen width */
		wk->screen.pixel.width = (pixel.width * 1000L) / wk->screen.mfdb.width;
	else								   /*	or fixed DPI (negative) */
		wk->screen.pixel.width = 25400 / -pixel.width;

	if (pixel.height > 0)		/* Starts out as screen height */
		wk->screen.pixel.height = (pixel.height * 1000L) / wk->screen.mfdb.height;
	else									/*	 or fixed DPI (negative) */
		wk->screen.pixel.height = 25400 / -pixel.height;

	/* Switch to the RTG screen */
	uaelib_picasso_SetSwitch(&my_BoardInfo, 1);

	return 0;
}

/*
 * 'Deinitialize'
 */
void CDECL clswk(Virtual *vwk)
{
	(void) vwk;
	/* Switch to the standard screen */
	uaelib_picasso_SetSwitch(&my_BoardInfo, 0);
}
