/*
 * fVDI font load and setup
 *
 * Copyright 1997-2000/2003, Johan Klockars
 *                     2005, Standa Opichal
 * This software is licensed under the GNU General Public License.
 * Please, see LICENSE.TXT for further information.
 */

#include "fvdi.h"
#include "relocate.h"

#include <ft2build.h>
#include <freetype/config/ftconfig.h>
#ifdef FT_FREETYPE_H
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_SYNTHESIS_H
#include FT_STROKER_H
#else
#include <freetype/freetype.h>
#include <freetype/ftglyph.h>
#include <freetype/ftsynth.h>	/* FT_GlyphSlot_Embolden */
#include <freetype/ftstroke.h>	/* FT_Stroker, ... */
#endif

#include <mint/osbind.h>

#include "globals.h"
#include "utility.h"
#include "function.h"
#include "list.h"
#include "modules/ft2.h"
#include "ft2.h"

#undef CACHE_YSIZE

#ifndef __STRINGIFY
#ifndef __STRING
#define __STRING(x)	#x
#endif
#define __STRINGIFY(x)	__STRING(x)
#endif
char const ft2_version[] = __STRINGIFY(FREETYPE_MAJOR) "." __STRINGIFY(FREETYPE_MINOR) "." __STRINGIFY(FREETYPE_PATCH);

/* Cached glyph information */
typedef struct cached_glyph
{
    int stored;
    FT_UInt index;
    FT_Bitmap bitmap;
    FT_Bitmap pixmap;
    int minx;
    int maxx;
#ifdef CACHE_YSIZE
    int miny;
    int maxy;
#endif
    int yoffset;
    int advance;
    short cached;
} c_glyph;


/* Handy routines for converting from fixed point */
#define FT_FLOOR(X)	((X & -64) / 64)
#define FT_CEIL(X)	(((X + 63) & -64) / 64)

#define CACHED_METRICS	0x10
#define CACHED_BITMAP	0x01
#define CACHED_PIXMAP	0x02

/* Underline doesn't warrant a new font cache */
#define FT2_EFFECTS_MASK 0x17

#if 1
#define DEBUG_FONTS 1
#endif


/* NVDI/SpeedoGDOS spdchar.map tables */
static Fontspdcharmap spdchar_map;
static short spdchar_map_len = 0;

static FT_Library library;
static LIST fonts;

typedef struct {
    struct Linkable *next;
    struct Linkable *prev;
    Fontheader *font;
} FontheaderListItem;


static FT_Error ft2_find_glyph(Virtual *vwk, Fontheader *font, short ch, int want);
static Fontheader *ft2_dup_font(Virtual *vwk, Fontheader *src, short ptsize);
static void ft2_dispose_font(Fontheader *font);

#define USE_FREETYPE_ERRORS 1

static char *ft2_error(const char *msg, FT_Error error)
{
    static char buffer[1024];

#ifdef USE_FREETYPE_ERRORS
#if 0
#undef FTERRORS_H
#define FT_ERRORDEF(e, v, s)  {e, s},
#else
#undef __FTERRORS_H__
#define FT_ERRORDEF(e, v, s)  {e, s},
#define FT_ERROR_START_LIST     {
#define FT_ERROR_END_LIST       { 0, 0 } };
#endif

    static const struct
    {
        int err_code;
        const char *err_msg;
    } ft_errors[] =
#ifdef FT_ERRORS_H
#include FT_ERRORS_H
#else
#include <freetype/fterrors.h>
#endif
    int i;
    const char *err_msg;

    err_msg = NULL;
    for (i = 0; i < (int) ((sizeof ft_errors) / (sizeof ft_errors[0])); ++i)
    {
        if (FT_ERROR_BASE(error) == ft_errors[i].err_code)
        {
            err_msg = ft_errors[i].err_msg;
            break;
        }
    }
    if (!err_msg)
    {
        err_msg = "unknown FreeType error";
    }
    ksprintf(buffer, "%s: %s (%d)\n", msg, err_msg, FT_ERROR_BASE(error));
#else
    ksprintf(buffer, "%s: (%d)\n", msg, (int)error);
#endif
    return buffer;
}


void ft2_term(void)
{
    /* Cleanup the FT2 font cache */
    FontheaderListItem *x = (FontheaderListItem *) listLast(&fonts);

    while (x)
    {
        FontheaderListItem *tmp = (FontheaderListItem *) listPrev(x);

        listRemove((LINKABLE *) x);
        ft2_dispose_font(x->font);      /* Remove the whole font */
        x = tmp;
    }

    /* Terminate the FT2 library */
    FT_Done_FreeType(library);
}


int ft2_init(void)
{
    FT_Error error;

    error = FT_Init_FreeType(&library);
    if (error)
        return -1;

    /* Initialize Fontheader LRU cache */
    listInit(&fonts);

    return 0;
}


static void ft2_close_face(Fontheader *font)
{
    if (!font->extra.unpacked.data)
        return;

#ifdef DEBUG_FONTS
    if (debug > 1)
    {
        PRINTF(("FT2 close_face: %s\n", font->extra.filename));
        PRINTF(("%s, size: %d\n", ((FT_Face) font->extra.unpacked.data)->family_name, font->size));
    }
#endif

    FT_Done_Face((FT_Face) font->extra.unpacked.data);
    font->extra.unpacked.data = NULL;
}


static Fontheader *ft2_load_metrics(Virtual *vwk, Fontheader *font, FT_Face face, short ptsize)
{
    FT_Error error;

    if (FT_IS_SCALABLE(face))
    {
        FT_Fixed scale;

        /* Set the character size and use default DPI (72) */
#if 1
        if (!ptsize)
        {
            access->funcs.puts("Attempt to load metrics with bad point size!\n");
            ptsize = 10;
        }
#endif

        error = FT_Set_Char_Size(face, 0, ptsize * 64,
                                 25400 / vwk->real_address->screen.pixel.width,
                                 25400 / vwk->real_address->screen.pixel.height);
        if (error)
        {
            access->funcs.puts(ft2_error("FT2  Couldn't set vector font size", error));
            ft2_close_face(font);
            return NULL;
        }

        scale = face->size->metrics.y_scale;
        font->distance.ascent = FT_CEIL(FT_MulFix(face->ascender, scale));
        font->distance.descent = FT_CEIL(FT_MulFix(-face->descender, scale));
        font->distance.top = FT_CEIL(FT_MulFix(face->bbox.yMax, scale));
        font->distance.bottom = FT_CEIL(FT_MulFix(-face->bbox.yMin, scale));
        font->height = FT_CEIL(FT_MulFix(face->bbox.yMax - face->bbox.yMin, scale));

        /* This gives us weird values - perhaps caused by taking care of unusual characters out of Latin-1 charset */
        font->widest.cell = FT_CEIL(FT_MulFix(face->bbox.xMax - face->bbox.xMin, face->size->metrics.x_scale));
        font->widest.character = FT_CEIL(FT_MulFix(face->max_advance_width, face->size->metrics.x_scale));

        font->extra.underline_offset = FT_FLOOR(FT_MulFix(face->underline_position, scale));
        font->underline = FT_FLOOR(FT_MulFix(face->underline_thickness, scale));
    } else if (face->num_fixed_sizes)
    {
        int pick = 0;
        int s;

        /* Find the required font size in the bitmap face (if present) */
        if (debug > 1)
        {
            PRINTF(("FT2 face_sizes(%d) ", face->num_fixed_sizes));
        }

        for (s = 0; s < face->num_fixed_sizes; s++)
        {
            short size = face->available_sizes[s].size / 64;

            if (debug > 1)
            {
                PRINTF((", %d [%ld,%ld] ", size, (long) face->available_sizes[s].width, (long) face->available_sizes[s].height));
            }

            /* Find the closest font size available */
            if (ptsize - face->available_sizes[pick].size / 64 > ptsize - size)
                pick = s;

            if (ptsize >= size / 64)
                break;
        }
        if (debug > 1)
        {
            PRINTF((" => %ld\n", (long) face->available_sizes[pick].size / 64));
        }

#if FREETYPE_VERSION >= 2002000L
        /* FreeType 2.2 and onwards */
        error = FT_Select_Size(face, pick);
#else
        /* FreeType 2.1.x */
        error = FT_Set_Pixel_Sizes(face, face->available_sizes[pick].width, face->available_sizes[pick].height);
#endif
        if (error)
        {
            access->funcs.puts(ft2_error("FT2  Couldn't set bitmap font size", error));
            if (debug > 1)
            {
                PRINTF((" pick: %d\n", pick));
            }
            ft2_close_face(font);
            return NULL;
        }

        font->distance.ascent = face->available_sizes[pick].height;
        font->distance.descent = 0;
        font->distance.top = face->available_sizes[pick].height;
        font->distance.bottom = 0;
        font->height = font->distance.top + font->distance.bottom;

        /* This gives us weird values - perhaps caused by taking care of unusual characters out of Latin-1 charset */
        font->widest.cell = face->available_sizes[pick].width;
        font->widest.character = face->available_sizes[pick].width;

        font->extra.underline_offset = FT_FLOOR(face->underline_position);
        font->underline = FT_FLOOR(face->underline_thickness);

        /* x offset = cos(((90.0-12)/360)*2*M_PI), or 12 degree angle */
        font->skewing = (int) (0.207f * font->height + 1 /* ceiling */ );
    }

    font->size = ptsize;
    font->extra.effects = vwk->text.effects & FT2_EFFECTS_MASK;
    font->lightening = 0xaaaa; /* Set the mask to apply to the glyphs */

    /* Finish the font metrics fill-in */

    /* Font half distance (positive distance from baseline) */
    font->distance.half = font->distance.top - (font->height >> 1);

    /* Fake for vqt_fontinfo() as some apps might rely on this */
    font->code.low = 0;
    font->code.high = 255;

    if (font->underline < 1)
    {
        font->underline = 1;
    }

    if (debug > 1)
    {
        PRINTF(("Font metrics: %s\n", font->name));
        PRINTF(("\tascent = %d, descent = %d, sum = %d\n",
                font->distance.ascent, font->distance.descent, font->distance.ascent + font->distance.descent));
        PRINTF(("\ttop = %d, bottom = %d, height = %d\n",
                font->distance.top, font->distance.bottom, font->height));
        PRINTF(("\ty_ppem = %d, height = %ld\n", face->size->metrics.y_ppem, face->size->metrics.height));
        PRINTF(("\tcell = %d, character = %d\n", font->widest.cell, font->widest.character));
    }

    {
        /* Fixup to handle alignments better way */
        int top = font->distance.top;

        font->extra.distance.base = -top;
        font->extra.distance.half = -top + font->distance.half;
        font->extra.distance.ascent = -top + font->distance.ascent;
        font->extra.distance.bottom = -top - font->distance.bottom;
        font->extra.distance.descent = -top - font->distance.descent;
        font->extra.distance.top = 0;
    }

    return font;
}


static int ft2_load_spdchar_map(Virtual *vwk, const char *filename)
{
    int file;

    (void) vwk;
    if ((file = Fopen(filename, 0)) < 0)
        return -1;

    spdchar_map_len = Fread(file, sizeof(spdchar_map), &spdchar_map);
    Fclose(file);

    if (debug > 1)
    {
        PRINTF(("spdchar.map: len=%d filename=%s\n", spdchar_map_len, filename));
    }

    return 0;
}


static short ft2_get_face_id(Virtual *vwk, FT_Face face)
{
    short id;
    /* Get the family_name + style_name hashcode */
    short id_conflict;
    const char *p;
    short hc = 0;

    for (p = face->family_name; *p; p++)
        hc = (hc << 5) - hc + *p;
    for (p = face->style_name; *p; p++)
        hc = (hc << 5) - hc + *p;

    /* id is >5000 for Vector fonts (from SpeedoGDOS)  */
    id = hc + 5000;
    if (id < 0)
        id = -id;
    if (id < 5000)
        id += 5000;

    /* Find out whether there already is such a font ID */
    do
    {
        Fontheader *f = vwk->real_address->writing.first_font;

        id++;
        id_conflict = 0;

        while (f)
        {
            if (f->id == id)
            {
                id_conflict = 1;
                break;
            }
            f = f->next;
        }
    } while (id_conflict);

    return id;
}


/*
 * Load a font and make it ready for use
 */
Fontheader *ft2_load_font(Virtual *vwk, const char *filename)
{
    Fontheader *font;
    int len = (int) strlen(filename);

    /* FIXME: hack */
    /* all this to just use strcmp() instead of the strcasestr() which
     * is not available atm. */
    if (len >= 11)
    {
        char name[14];
        char *d;
        const char *s;

        for (d = name, s = filename + len - 11; *s; s++)
            *d++ = *s | 0x20;
        *d++ = '\0';
        name[7] = '.';

        if (!strcmp("spdchar.map", name))
        {
            ft2_load_spdchar_map(vwk, filename);
            return NULL;
        }
    }

    font = (Fontheader *) malloc(sizeof(Fontheader));
    if (font)
    {
        FT_Error error;
        FT_Face face;

        ft_keep_open();
        /* Open the font and create ancillary data */
        error = FT_New_Face(library, filename, 0, &face);
        if (error)
        {
            ft_keep_closed();
            free(font);
            return NULL;
        }

        /* Clear the structure */
        memset(font, 0, sizeof(Fontheader));

        /* Construct the font->name = family_name + style_name */
        {
            char buf[255];

            strcpy(buf, face->family_name);
            strcat(buf, " ");
            strcat(buf, face->style_name);		/* FIXME: Concatenate? */
            strncpy(font->name, buf, 32);		/* Family name would be the font name? */
        }

        /* FIXME: store the font type (TTF, Type1) somewhere for vqt_xfntinfo and vq_fontheader functions
         *        e.g. the x.app decides what to call depending on the type of the font. */

        font->id = ft2_get_face_id(vwk, face);
        font->flags = FONTF_EXTERNAL |				/* FT2 handled font */
            (FT_IS_SCALABLE(face) ? FONTF_SCALABLE : 0) |
            (FT_IS_FIXED_WIDTH(face) ? FONTF_MONOSPACED : 0);	/* .FNT compatible flag */
        font->extra.filename = strdup(filename);		/* Font filename to load_glyphs on-demand */
        font->extra.index = 0;				/* Index to load, FIXME: how to we load multiple of them */
        font->extra.effects = 0;

        if (!face->num_fixed_sizes)
        {
            font->size = 0;				/* Vector fonts have size = 0 */
        } else
        {
            font->size = face->available_sizes[0].size / 64;	/* Bitmap font size */
        }

        if (debug > 0)
        {
            PRINTF(("FT2 load_font: %s: size=%ld\n", font->name, (long) font->size));
        }

        /* By default faces should not be kept in memory... (void *)face */
        font->extra.unpacked.data = NULL;
        font->extra.cache = NULL;
        font->extra.scratch = NULL;

#if 0
        if (face->num_fixed_sizes > 1)
        {
            int f;

            /* FIXME!!! NOT TESTED */
            for (f = 1; f < face->num_fixed_sizes; f++)
            {
                Fontheader *new_font = ft2_dup_font(vwk, font, face->available_sizes[f].size / 64);

                if (!new_font)
                    continue;

                /* It's assumed that a device has been initialized (driver exists) */
                if (insert_font(&vwk->real_address->writing.first_font, new_font))
                    vwk->real_address->writing.fonts++;
            }
        }
#endif

        FT_Done_Face(face);
        ft_keep_closed();
    }

    return font;
}


static Fontheader *ft2_open_face(Virtual *vwk, Fontheader *font, short ptsize)
{
    FT_Error error;
    FT_Face face;

#if 0
    if (font->extra.unpacked.data)
        return font;
#endif

#ifdef DEBUG_FONTS
    if (debug > 1)
    {
        PRINTF(("FT2  open_face: %s\n", font->extra.filename));
    }
#endif

    /* Open the font and create ancillary data */
    error = FT_New_Face(library, font->extra.filename, 0, &face);
    if (error)
    {
        access->funcs.puts(ft2_error("FT2  open_face error: ", error));
        return NULL;
    }

#ifdef DEBUG_FONTS
    if (debug > 1)
    {
        PRINTF(("%s, size: %d\n", face->family_name, ptsize));
    }
#endif

    if (font->extra.index != 0)
    {
        if (face->num_faces > font->extra.index)
        {
            FT_Done_Face(face);
            error = FT_New_Face(library, font->extra.filename, font->extra.index, &face);
            if (error)
            {
                access->funcs.puts(ft2_error("FT2  Couldn't get font face", error));
                return NULL;
            }
        } else
        {
            access->funcs.puts(ft2_error("FT2  No such font face", error));
            return NULL;
        }
    }

    if (!font->extra.cache)
    {
        font->extra.cache = malloc(sizeof(c_glyph) * 256);	/* Cache */
        memset(font->extra.cache, 0, sizeof(c_glyph) * 256);	/* Cache */
        font->extra.scratch = malloc(sizeof(c_glyph));		/* Scratch */
        memset(font->extra.scratch, 0, sizeof(c_glyph));
    }

    font = ft2_load_metrics(vwk, font, face, ptsize);
    if (!font)
    {
        access->funcs.puts("FT2  Cannot load metrics\n");
        return NULL;
    }

    /* Face loaded successfully */
    font->extra.unpacked.data = (void *) face;

    return font;
}


static FT_Face ft2_get_face(Virtual *vwk, Fontheader *font)
{
    /* Open the face if needed */
    if (!font->extra.unpacked.data)
    {
        if (font->size)
            font = ft2_open_face(vwk, font, font->size);
        else
            font = ft2_open_face(vwk, font, 10);
    }

    if (!font)
        return NULL;

    return (FT_Face) font->extra.unpacked.data;
}


static Fontheader *ft2_dup_font(Virtual *vwk, Fontheader *src, short ptsize)
{
    Fontheader *font = (Fontheader *) malloc(sizeof(Fontheader));

    if (font)
    {
        memcpy(font, src, sizeof(Fontheader));
        font->extra.filename = strdup(src->extra.filename);
        font->extra.ref_count = 0;

        /* Clean the FT2 data and cache -> initialized below here */
        font->extra.unpacked.data = NULL;
        font->extra.cache = NULL;
        font->extra.scratch = NULL;

        /* underline == 0 -> metrics were not read yet */
        font->underline = 0;

#ifdef DEBUG_FONTS
        if (debug > 1)
        {
            PRINTF(("FT2  dup_font: %s, size: %d\n", font->name, ptsize));
        }
#endif

        font = ft2_open_face(vwk, font, ptsize);
    }

    return font;
}


void ft2_fontheader(Virtual *vwk, Fontheader *font, VQT_FHDR *fhdr)
{
    int i;
    FT_Face face = ft2_get_face(vwk, font);

    /* Strings should not have NUL termination if max size. */
    /* Normally 1000 ORUs per Em square (width of 'M'), but header says. */
    /* 6 byte transformation parameters contain:
     *  short y offset (ORUs)
     *  short x scaling (units of 1/4096)
     *  short y scaling (units of 1/4096)
     */

    memcpy(fhdr->fh_fmver, "D1.0\x0d\x0a\0\0", 8);  /* Format identifier */
    fhdr->fh_fntsz = 0;     /* Font file size */
    fhdr->fh_fbfsz = 0;     /* Minimum font buffer size (non-image data) */
    fhdr->fh_cbfsz = 0;     /* Minimum character buffer size (largest char) */
    fhdr->fh_hedsz = sizeof(VQT_FHDR);  /* Header size */
    fhdr->fh_fntid = 0;     /* Font ID (Bitstream) */
    fhdr->fh_sfvnr = 0;     /* Font version number */
    for (i = 0; i < 32; i++)
    {
        /* Font full name (vqt_name) */
        fhdr->fh_fntnm[i] = font->name[i];
    }
    fhdr->fh_fntnm[i] = 0;
    fhdr->fh_mdate[0] = 0;  /* Manufacturing date (DD Mon YY) */
    fhdr->fh_laynm[0] = 0;  /* Character set name, vendor ID, character set ID */
    /* Last two is char set, usually the second two characters in font filename
     *   Bitstream International Character Set = '00'
     * Two before that is manufacturer, usually first two chars in font filename
     *   Bitstream fonts use 'BX'
     */
    fhdr->fh_cpyrt[0] = 0;  /* Copyright notice */
    fhdr->fh_nchrl = 0;     /* Number of character indices in character set */
    fhdr->fh_nchrf = face->num_glyphs;  /* Total number of character indices in font */
    fhdr->fh_fchrf = 0;     /* Index of first character */
    fhdr->fh_nktks = 0;     /* Number of kerning tracks */
    fhdr->fh_nkprs = 0;     /* Number of kerning pairs */
    fhdr->fh_flags = 0;     /* Font flags, bit 0 - extended mode */
    /* Extended mode is for fonts that require higher quality of rendering,
     * such as chess pieces. Otherwise compact, the default.
     */
    fhdr->fh_cflgs = 0;     /* Classification flags */
    /* bit 0 - Italic
     * bit 1 - Monospace
     * bit 2 - Serif
     * bit 3 - Display
     */
    if (face->style_flags & FT_STYLE_FLAG_ITALIC)
        fhdr->fh_cflgs |= 1;
    if (face->face_flags & FT_FACE_FLAG_FIXED_WIDTH)
        fhdr->fh_cflgs |= 2;
    fhdr->fh_famcl = 0;     /* Family classification */
    /* 0 - Don't care
     * 1 - Serif
     * 2 - Sans serif
     * 3 - Monospace
     * 4 - Script
     * 5 - Decorative
     */
    if (face->face_flags & FT_FACE_FLAG_FIXED_WIDTH)
        fhdr->fh_famcl |= 8;
    fhdr->fh_frmcl = 0x68;  /* Font form classification */
    /* 0x_4 - Condensed
     * 0x_5 - (Reserved for 3/4 condensed)
     * 0x_6 - Semi-condensed
     * 0x_7 - (Reserved for 1/4 condensed)
     * 0x_8 - Normal
     * 0x_9 - (Reserved for 3/4 expanded)
     * 0x_a - Semi-expanded
     * 0x_b - (Reserved for 1/4 expanded)
     * 0x_c - Expanded
     * 0x1_ - Thin
     * 0x2_ - Ultralight
     * 0x3_ - Extralight
     * 0x4_ - Light
     * 0x5_ - Book
     * 0x6_ - Normal
     * 0x7_ - Medium
     * 0x8_ - Semibold
     * 0x9_ - Demibold
     * 0xa_ - Bold
     * 0xb_ - Extrabold
     * 0xc_ - Ultrabold
     * 0xd_ - Heavy
     * 0xe_ - Black
     */
    if (face->style_flags & FT_STYLE_FLAG_BOLD)
        fhdr->fh_frmcl = (fhdr->fh_frmcl & 0x0f) | 0xa0;
    /* The below should likely include "Italic" etc */
    strncpy(fhdr->fh_sfntn, font->name, /* Short font name */
            sizeof(fhdr->fh_sfntn));
    /* Abbreviation of Postscript equivalent font name */
    strncpy(fhdr->fh_sfacn, face->family_name,  /* Short face name */
            sizeof(fhdr->fh_sfacn));
    /* Abbreviation of the typeface family name */
    strncpy(fhdr->fh_fntfm, face->style_name,  /* Font form (as above), style */
            sizeof(fhdr->fh_fntfm));
    fhdr->fh_itang = 0;     /* Italic angle */
    /* Skew in 1/256 of degrees clockwise, if italic font */
    fhdr->fh_orupm = face->units_per_EM;  /* ORUs per Em */
    /* Outline Resolution Units */

    /* There's actually a bunch of more values, but they are not
     * in the struct definition, so skip them
     */
}


void ft2_xfntinfo(Virtual *vwk, Fontheader *font, long flags, XFNT_INFO *info)
{
    int i;
    FT_Face face = ft2_get_face(vwk, font);

    info->format = (font->flags & FONTF_SCALABLE) ? 4 : 1;

    if (flags & XFNT_INFO_FONT_NAME)
    {
        for (i = 0; i < 32; i++)
        {
            info->font_name[i] = font->name[i];
        }
        info->font_name[i] = 0;
    }

    if (flags & XFNT_INFO_FAMILY_NAME)
    {
        strncpy(info->family_name, face->family_name, sizeof(info->family_name) - 1);
        info->family_name[sizeof(info->family_name) - 1] = 0;
    }

    if (flags & XFNT_INFO_STYLE_NAME)
    {
        strncpy(info->style_name, face->style_name, sizeof(info->style_name) - 1);
        info->style_name[sizeof(info->style_name) - 1] = 0;
    }

    if (flags & XFNT_INFO_FILE_NAME1)
    {
        strncpy(info->file_name1, font->extra.filename, sizeof(info->file_name1) - 1);
        info->file_name1[sizeof(info->file_name1) - 1] = 0;
    }

    if (flags & XFNT_INFO_FILE_NAME2)
    {
        info->file_name2[0] = 0;
    }

    if (flags & XFNT_INFO_FILE_NAME3)
    {
        info->file_name3[0] = 0;
    }

    /* 0x100 is without enlargement, 0x200 with */
    if (flags & (XFNT_INFO_SIZES|XFNT_INFO_SIZES2))
    {
        for (i = 0; i < size_count && sizes[i] != 0xffff; i++)
            info->pt_sizes[i] = sizes[i];
        info->pt_cnt = i;
    }
}


static FT_Error ft2_load_glyph(Virtual *vwk, Fontheader *font, short ch, c_glyph *cached, int want)
{
    short bitmap_italics_shear;
    FT_Face face;
    FT_Error error;
    FT_Glyph g;
    FT_GlyphSlot glyph;

    face = ft2_get_face(vwk, font);
    if (!face)
    {
        access->funcs.puts(ft2_error("FT2  Couldn't get face", 0));
        return 1;
    }

    /* Load the glyph */
    if (!cached->index)
    {
        /* vwk->text.charmap  ~  vst_charmap (vst_map_mode()) mapping settings */
        if (vwk->text.charmap == MAP_ATARI /* ASCII */ && ch >= 0 && ch < 256)
        {
            unsigned short cc = 0xffff;

            /* if there is no spdchar.map (or not enough data)
             * -> use built-in mapping */
            if (spdchar_map_len < 450)
            {
                cc = Atari2Bics[ch];
            } else
            {
                if (ch > 31)
                {
                    /* NVDI really seems to be using the _first_ map '00' in the
                     * spdchar.map file no matter what the font format is.
                     *
                     * The TT table seems to contain a lot of crap for non ASCII
                     * chars commonly.
                     */
                    cc = spdchar_map.bs_int.map[ch - 32];
                }

                /* in case we don't have BICS code => fallback to the default mapping */
                if (cc >= BICS_COUNT || ch < 32)
                {
                    cc = Atari2Bics[ch];
                }
            }

            /* in case we don't have BICS -> UNICODE the cc value will be -1 */
            cc = Bics2Unicode[cc];

            /* get the font character index */
            cached->index = FT_Get_Char_Index(face, cc == 0xffff ? (FT_ULong)-1 : cc);

            /* When there is no such character in the font (or cc was -1)
             * we fallback to the default built-in mapping */
            if (!cached->index)
            {
                cc = Atari2Bics[ch];

                /* valid BICS code => to unicode */
                cc = Bics2Unicode[cc];

                /* sanity check */
                if (cc == 0xffff)
                    cc = ch;

                /* get the font character index */
                cached->index = FT_Get_Char_Index(face, cc == 0xffff ? (FT_ULong)-1 : cc);
            }

#if 0
            cached->index = FT_Get_Char_Index(face, Atari2Unicode[ch]);
#endif
        } else if (vwk->text.charmap == MAP_UNICODE)
        {
            /* app use: used at least by the 'Highwire web browser' */
            cached->index = FT_Get_Char_Index(face, ch);
        } else if (vwk->text.charmap == MAP_BITSTREAM)
        {
            /* BICS */ /* no char -> index translation, BICS char index is expected */

            /* app use: might perhaps be used by the 'charmap5' spdchar.map editor */
            unsigned short cc = ch < 0 || ch >= BICS_COUNT ? 0xffff : Bics2Unicode[ch];

            if (cc == 0xffff)
                cc = ch;
            cached->index = FT_Get_Char_Index(face, cc);
        } else
        {
            kprintf("FT2 MAPPING(%d) THAT SHOULD HAVE NEVER HAPPENED!!!\n", vwk->text.charmap);
            cached->index = ch;
        }
    }

    error = FT_Load_Glyph(face, cached->index, FT_LOAD_DEFAULT);
    if (error)
    {
        access->funcs.puts(ft2_error("FT2  Couldn't load glyph", error));
        return error;
    }

    /* Get our glyph shortcuts */
    glyph = face->glyph;

    bitmap_italics_shear = 0;

    /* Handle 'Bold' effect */
    if (font->extra.effects & 0x1)
    {
        /* From 2.1.10 ChangeLog:
         *
         * - A new  API `FT_Outline_Embolden'  (in FT_OUTLINE_H) gives  finer
         * control how  outlines are embolded.
         *
         * - `FT_GlyphSlot_Embolden' (in FT_SYNTHESIS_H)  now handles bitmaps
         * also (code contributed  by Chia I Wu).  Note that this  function
         * is still experimental and may be replaced with a better API.
         */
        FT_GlyphSlot_Embolden(glyph);
    }

    /* Handle 'Italic' style */
    if (font->extra.effects & 0x4)
    {
        if (glyph->format != FT_GLYPH_FORMAT_BITMAP)
        {
            FT_Matrix shear;

            /* FIXME: not always 12 degree angle here for VDI: see vst_skew() */

            /* x offset = cos(((90.0-12)/360)*2*M_PI), or 12 degree angle */
            shear.xx = 1 << 16;
            shear.xy = (int) (0.207f * font->height * (1 << 16)) / font->height;
            shear.yx = 0;
            shear.yy = 1 << 16;

            FT_Outline_Transform(&glyph->outline, &shear);
        } else if (face->num_fixed_sizes)
        {
            bitmap_italics_shear = font->skewing;
        }
    }

    FT_Get_Glyph(glyph, &g);

    /* Outlined style */
    if (font->extra.effects & 0x10 && glyph->format != FT_GLYPH_FORMAT_BITMAP)
    {
        FT_Stroker s;

        error = FT_Stroker_New(library, &s);
        if (!error)
        {
            FT_Stroker_Set(s, 16, FT_STROKER_LINECAP_ROUND, FT_STROKER_LINEJOIN_ROUND, 0);
            FT_Glyph_Stroke(&g, s, 1 /* delete the original glyph */ );

            FT_Stroker_Done(s);
        }
    }


    /* Get the glyph metrics if desired */
    if ((want & CACHED_METRICS) && !(cached->stored & CACHED_METRICS))
    {
        FT_Glyph_Metrics *metrics = &glyph->metrics;

        /* Get the bounding box */
        if (FT_IS_SCALABLE(face))
        {
#if 1
            FT_BBox bbox;
            FT_Glyph_Get_CBox(g, FT_GLYPH_BBOX_PIXELS, &bbox);

            cached->minx = bbox.xMin;
            cached->maxx = bbox.xMax;
            cached->yoffset = font->distance.ascent - bbox.yMax;
#else

            cached->minx = FT_FLOOR(metrics->horiBearingX);
            cached->maxx = cached->minx + FT_CEIL(metrics->width);
#ifdef CACHE_YSIZE
            cached->maxy = FT_FLOOR(metrics->horiBearingY);
            cached->miny = cached->maxy - FT_CEIL(metrics->height);

            cached->yoffset = font->distance.ascent - cached->maxy;
#else
            cached->yoffset = font->distance.ascent - FT_FLOOR(metrics->horiBearingY);
#endif
#endif
            cached->advance = FT_CEIL(metrics->horiAdvance);

        } else
        {
            /* Get the bounding box for non-scalable format.
             * Again, freetype2 fills in many of the font metrics
             * with the value of 0, so some of the values we
             * need must be calculated differently with certain
             * assumptions about non-scalable formats.
             * */
            cached->minx = FT_FLOOR(metrics->horiBearingX);
            cached->maxx = cached->minx + FT_CEIL(metrics->horiAdvance);
#ifdef CACHE_YSIZE
            cached->maxy = FT_FLOOR(metrics->horiBearingY);
            cached->miny = cached->maxy - FT_CEIL(font->distance.top);
#endif
            cached->yoffset = 0;
            cached->advance = FT_CEIL(metrics->horiAdvance);

            cached->maxx += bitmap_italics_shear;
        }

        cached->stored |= CACHED_METRICS;
    }

    if (((want & CACHED_BITMAP) && !(cached->stored & CACHED_BITMAP)) ||
        ((want & CACHED_PIXMAP) && !(cached->stored & CACHED_PIXMAP)))
    {
        FT_Render_Mode render_mode = (want & CACHED_PIXMAP) ? FT_RENDER_MODE_NORMAL : FT_RENDER_MODE_MONO;
        FT_Bitmap *src;
        FT_Bitmap *dst = (render_mode == FT_RENDER_MODE_NORMAL) ? &cached->pixmap : &cached->bitmap;
        int i;

        /* Render the glyph */
        if (glyph->format != FT_GLYPH_FORMAT_BITMAP)
        {
            error = FT_Glyph_To_Bitmap(&g, render_mode, NULL, 0);
            src = &((FT_BitmapGlyph) g)->bitmap;
        } else
        {
            error = FT_Render_Glyph(glyph, render_mode);
            src = &(glyph->bitmap);
        }

        if (error)
        {
            if (debug > 1)
                access->funcs.puts(ft2_error("FT2  Couldn't render glyph", error));

            /* not rendered -> quite fine, just clean it up */
            src->rows = src->width = src->pitch = 0;
            src->buffer = NULL;
        }

        /* Copy over to cache */
        memcpy(dst, src, sizeof(*dst));

#if 0
        if (font->extra.effects & 0x10)
        {
            dst->width += font->thickening << 1 /* FIXME */ ;
        }
#endif
        /* Need to enlarge to fit the italics shear if bitmap font */
        dst->width += bitmap_italics_shear;

        if (want & CACHED_PIXMAP)
        {
            /* NOTE: This all assumes that the ft_render_mode_normal result is 8 bit */
            dst->pitch = (dst->width + 1) & ~1;   /* Even width is the pitch */
        } else
        {
            dst->pitch = ((dst->width + 15) >> 4) << 1;   /* Only whole words */
        }

        if (src->rows != 0)
        {
            dst->buffer = malloc(dst->pitch * dst->rows);
            if (!dst->buffer)
            {
                access->funcs.puts("FT2  Not dst alloc: enough memory\n");
                FT_Done_Glyph(g);
                return 1;
            }
            memset(dst->buffer, 0, dst->pitch * dst->rows);

            /* Outlined style */
            if (font->extra.effects & 0x10 && glyph->format == FT_GLYPH_FORMAT_BITMAP)
            {
#if 0
                CRASHES ...
                /* convert to the word aligned pitch (to dst buffer) */
                long pitch = ((src->width + 15) >> 4) << 1; /* whole words */

                for (i = 0; i < src->rows; i++)
                {
                    int soffset = i * src->pitch;
                    int doffset = i * pitch;

                    memcpy(dst->buffer + doffset, src->buffer + soffset, src->pitch);
                }

                /* set the src pitch to the same */
                src->pitch = pitch;
                src->buffer = realloc(src->buffer, src->pitch * src->rows);
                memset(src->buffer, 0, src->pitch * src->rows);

                /* now word aligned pitch is in dst
                 * -> outline the glyph into src->buffer */
                bitmap_outline(dst->buffer, src->buffer, src->pitch, src->pitch >> 1, src->rows);

                /* clean the temporary dst again */
                memset(dst->buffer, 0, dst->pitch * dst->rows);
#endif
            }


            if ((want & CACHED_PIXMAP) && (src->pixel_mode == FT_PIXEL_MODE_MONO))
            {
                /* This special case wouldn't
                 * be here if the FT_Render_Glyph()
                 * function wasn't buggy when it tried
                 * to render a .fon font with 256
                 * shades of gray.  Instead, it
                 * returns a black and white surface
                 * and we have to translate it back
                 * to a 256 gray shaded surface.
                 * */
                for (i = 0; i < src->rows; i++)
                {
                    int soffset = i * src->pitch;
                    int doffset = i * dst->pitch;
                    unsigned char *srcp = src->buffer + soffset;
                    unsigned char *dstp = dst->buffer + doffset;
                    unsigned char pix;
                    int j, k;

                    for (j = 0; j < src->width; j += 8)
                    {
                        pix = *srcp++;
                        for (k = 0; k < 8; ++k)
                        {
                            if (pix & 0x80)
                            {
                                *dstp++ = 0xff;
                            } else
                            {
                                *dstp++ = 0x00;
                            }
                            pix <<= 1;
                        }
                    }
                }
            } else
            {
                /* convert to the dst pitch */
                for (i = 0; i < src->rows; i++)
                {
                    int soffset = i * src->pitch;
                    int doffset = i * dst->pitch;

                    memcpy(dst->buffer + doffset, src->buffer + soffset, src->pitch);
                }
            }

#ifdef FVDI_DEBUG
            if (debug > 3 && (want & CACHED_PIXMAP))
            {
                /* DEBUG bitmaps */
                unsigned char pix;
                int j;

                PRINTF(("glyph width: %ld advance: %ld\n", (long)dst->width, (long)cached->advance));

                /* print */
                for (i = 0; i < dst->rows; i++)
                {
                    for (j = 0; j < dst->width; j++)
                    {
                        pix = *(unsigned char *) ((long) dst->buffer + (dst->pitch * i) + j);
                        PUTS((pix > 120) ? "*" : ".");
                    }
                    PUTS("\n");
                }
                PUTS("\n");
            }
#endif

        }

        if (bitmap_italics_shear)
        {
            short distance = font->height / (bitmap_italics_shear + 1);
            int row;

            if (want & CACHED_PIXMAP)
            {
                for (row = dst->rows - 1; row >= 0; --row)
                {
                    short cnt = (font->height - cached->yoffset - row) / distance;

                    if (cnt)
                    {
                        unsigned char *pixmap = (unsigned char *) dst->buffer + row * dst->pitch;

                        memmove(pixmap + cnt, pixmap, dst->pitch - cnt);
                        memset(pixmap, 0, cnt);
                    }
                }
            } else
            {
                for (row = dst->rows - 1; row >= 0; --row)
                {
                    short cnt = (font->height - cached->yoffset - row) / distance;

                    if (cnt)
                    {
                        int col;
                        unsigned short maskedPrev = 0;
                        unsigned short mask = (1 << cnt) - 1;
                        unsigned short *bitmap = (unsigned short *) (dst->buffer + row * dst->pitch);

                        for (col = (dst->pitch >> 1) - 1; col >= 0; --col)
                        {
                            unsigned short tmp = *bitmap & mask;

                            *bitmap = (maskedPrev << (16 - cnt)) | (*bitmap >> cnt);
                            bitmap++;
                            maskedPrev = tmp;
                        }
                    }
                }
            }
        }

        /* Light/grey effect */
        if (font->extra.effects & 0x2)
        {
            int row, col;

            if (want & CACHED_PIXMAP)
            {
                unsigned char *pixmap = (unsigned char *) dst->buffer;

                for (row = dst->rows - 1; row >= 0; --row)
                {
                    for (col = 0; col < dst->width; col++)
                    {
                        /* This is rather bitmap grey effect
                         * short lightening = row & 1 ? font->lightening : ~font->lightening;
                         *
                         * *(pixmap + col) = (lightening & (1 << (col % 16)) ) ? *(pixmap + col) : 0;
                         *
                         * Better to just make the color greyer:
                         **/
                        *(pixmap + col) = *(pixmap + col) / 3;
                    }
                    pixmap += dst->pitch;
                }
            } else
            {
                unsigned short *bitmap;

                for (row = dst->rows - 1; row >= 0; --row)
                {
                    short lightening = row & 1 ? font->lightening : ~font->lightening;

                    bitmap = (unsigned short *) ((long) dst->buffer + row * dst->pitch);
                    for (col = (dst->pitch >> 1) - 1; col >= 0; --col)
                    {
                        *bitmap++ &= lightening;
                    }
                }
            }
        }

        /* Mark that we rendered this format */
        cached->stored |= want & (CACHED_BITMAP | CACHED_PIXMAP);
    }

    FT_Done_Glyph(g);

    /* We're done, mark this glyph cached */
    cached->cached = ch;

    return 0;
}


static void ft2_flush_glyph(c_glyph *glyph)
{
    glyph->stored = 0;
    glyph->index = 0;
    if (glyph->bitmap.buffer)
    {
        free(glyph->bitmap.buffer);
        glyph->bitmap.buffer = 0;
    }
    if (glyph->pixmap.buffer)
    {
        free(glyph->pixmap.buffer);
        glyph->pixmap.buffer = 0;
    }
    glyph->cached = 0;
}


static void ft2_flush_cache(Fontheader *font)
{
    int i;
    int size = 256;

    for (i = 0; i < size; ++i)
    {
        if (((c_glyph *) font->extra.cache)[i].cached)
        {
            ft2_flush_glyph(&((c_glyph *) font->extra.cache)[i]);
        }

    }
    if (((c_glyph *) font->extra.scratch)->cached)
    {
        ft2_flush_glyph(font->extra.scratch);
    }
}


static void ft2_dispose_font(Fontheader *font)
{
    /* Close the FreeType2 face */
    ft2_close_face(font);

    /* Remove glyph bitmaps */
    ft2_flush_cache(font);

    /* Dispose of the data */
    free(font->extra.filename);
    free(font->extra.cache);
    free(font->extra.scratch);
    free(font);
}


static FT_Error ft2_find_glyph(Virtual *vwk, Fontheader *font, short ch, int want)
{
    int retval = 0;

    if (ch < 256)
    {
        font->extra.current = &((c_glyph *) font->extra.cache)[ch];
    } else
    {
        if (((c_glyph *) font->extra.scratch)->cached != ch)
        {
            ft2_flush_glyph(font->extra.scratch);
        }
        font->extra.current = font->extra.scratch;
    }
    if ((((c_glyph *) font->extra.current)->stored & want) != want)
    {
        retval = ft2_load_glyph(vwk, font, ch, font->extra.current, want);
    }

    return retval;
}


void *ft2_char_advance(Virtual *vwk, Fontheader *font, long ch, short *advance_info)
{
    if (!ft2_find_glyph(vwk, font, ch, CACHED_METRICS))
    {
        c_glyph *g = (c_glyph *) font->extra.current;

        /* FIXME! Text orientation not taken care of here */

        /* X advance */
        *advance_info++ = g->advance;
        /* Y advance */
        *advance_info++ = 0;

        /* Advance reminders */
        /* remX */
        *advance_info++ = 0;
        /* remY */
        *advance_info++ = 0;

        /* vqt_advance32() - SpeedoGDOS only */
        /* X advance */
        *advance_info++ = g->advance;
        *advance_info++ = 0;
        /* Y advance */
        *advance_info++ = 0;
        *advance_info++ = 0;
    }

    return 0;
}


void *ft2_char_bitmap(Virtual *vwk, Fontheader *font, long ch, short *bitmap_info)
{
    if (!ft2_find_glyph(vwk, font, ch, CACHED_METRICS | CACHED_BITMAP))
    {
        c_glyph *g = (c_glyph *) font->extra.current;

        *bitmap_info++ = g->bitmap.width;	/* Width */
        *bitmap_info++ = g->bitmap.rows;	/* Height */

        /* FIXME! Text orientation not taken care of here */

        /* X advance */
        *bitmap_info++ = g->advance;
        *bitmap_info++ = 0;
        /* Y advance */
        *bitmap_info++ = 0;
        *bitmap_info++ = 0;
        /* X offset */
        *bitmap_info++ = 0;
        *bitmap_info++ = 0;
        /* Y offset */
        *bitmap_info++ = font->height - g->yoffset;
        *bitmap_info++ = 0;

        if (debug > 1)
        {
            PRINTF(("FT2 bitmap_info: w=%ld h=%ld ad=%ld yo=%ld\n", (long) g->maxx, (long) font->height, (long) g->advance, (long) g->yoffset));
        }

        return g->bitmap.buffer;
    }

    return 0;
}


static int ft2_text_size(Virtual *vwk, Fontheader *font, const short *text, int *w, int *h)
{
    const short *ch;
    int x, z;
    int minx = 0, maxx = 0;
#ifdef CACHE_YSIZE
    int miny = 0, maxy = 0;
#endif
    c_glyph *glyph;
    FT_Error error;

    /* Load each character and sum it's bounding box */
    x = 0;
    for (ch = text; *ch; ++ch)
    {
        error = ft2_find_glyph(vwk, font, *ch, CACHED_METRICS);
        if (error)
        {
            continue;
        }
        glyph = font->extra.current;

        z = x + glyph->minx;
        if (minx > z)
        {
            minx = z;
        }
        if (glyph->advance > glyph->maxx)
        {
            z = x + glyph->advance;
        } else
        {
            z = x + glyph->maxx;
        }
        if (maxx < z)
        {
            maxx = z;
        }
        x += glyph->advance;

#ifdef CACHE_YSIZE
        if (glyph->miny < miny)
        {
            miny = glyph->miny;
        }
        if (glyph->maxy > maxy)
        {
            maxy = glyph->maxy;
        }
#endif
    }

    /* Fill the bounds rectangle */
    if (w)
    {
        *w = (maxx - minx);
    }
    if (h)
    {
#ifdef CACHE_YSIZE
#if 0 /* This is correct, but breaks many applications */
        *h = (maxy - miny);
#else
        *h = font->height;
#endif
#else
        *h = font->height;
#endif
    }

#ifdef FVDI_DEBUG
    if (debug > 2)
    {
        char buf[255];

        for (ch = text; *ch; ++ch)
        {
            buf[ch - text] = *ch;
        }
        buf[ch - text] = '\0';

        PRINTF(("txt width: \"%s\" -> %d\n", buf, *w));
    }
#endif

    return 0;
}


static MFDB *ft2_text_render_antialias(Virtual *vwk, Fontheader *font, short x, short y, const short *text, MFDB *textbuf)
{
    int xstart = 0;
    int width;
    const short *ch;
    c_glyph *glyph;

    FT_Bitmap *current;
    FT_Face face;
    FT_Error error;
    FT_Long use_kerning;
    FT_UInt prev_index = 0;

    MFDB tb;
    short colors[2];
    short pxy[8];

    (void) textbuf;
    tb.standard = 0x0100;		/* Chunky! */
    tb.bitplanes = 8;

    colors[1] = vwk->text.colour.background;
    colors[0] = vwk->text.colour.foreground;

    pxy[0] = 0;
    pxy[1] = 0;

    face = (FT_Face)font->extra.unpacked.data;

    /* Check kerning */
    use_kerning = 0; /* FIXME: FT_HAS_KERNING(face); */

    y += ((short *)&font->extra.distance)[vwk->text.alignment.vertical];

    for (ch = text; *ch; ++ch)
    {
        short c = *ch;

        error = ft2_find_glyph(vwk, font, c, CACHED_METRICS | CACHED_PIXMAP);
        if (error)
        {
            continue;
        }
        glyph = font->extra.current;

        current = &glyph->pixmap;

        /* Ensure the width of the pixmap is correct. In some cases,
         * FreeType may report a larger pixmap than possible.
         */
        width = current->width;
        if (width > glyph->maxx - glyph->minx)
        {
            width = glyph->maxx - glyph->minx;
        }
        /* Do kerning, if possible AC-Patch */
        if (use_kerning && prev_index && glyph->index)
        {
            FT_Vector delta;

            FT_Get_Kerning(face, prev_index, glyph->index, ft_kerning_default, &delta);
            xstart += delta.x >> 6;
        }
        /* Compensate for wrap around bug with negative minx's */
        if ((ch == text) && (glyph->minx < 0))
        {
            xstart -= glyph->minx;
        }

        /* FIXME? For now this is char by char */
        {
            /* NOTE:
             * This MFDB is only supported by the aranym driver so far
             *
             * standard = 0x0100  ~  chunky data
             * bitplane = 8       ~  will alpha expand the data
             **/

            /* Fill in the target surface */
            tb.width = width;
            tb.height = current->rows;
            tb.wdwidth = current->pitch >> 1; /* Words per line */
            tb.address = (void *)current->buffer;

            pxy[2] = tb.width - 1;
            pxy[3] = tb.height - 1;
            pxy[4] = x + xstart + glyph->minx;
            pxy[5] = y + glyph->yoffset;
            pxy[6] = pxy[4] + tb.width - 1;
            pxy[7] = pxy[5] + tb.height - 1;
            lib_vdi_spppp(&lib_vrt_cpyfm_nocheck, vwk, vwk->mode, pxy, &tb, NULL, colors);
        }

        xstart += glyph->advance;
        prev_index = glyph->index;
    }

    /* Handle the underline style */
    if (vwk->text.effects & 0x8)
    {
        short row = font->distance.ascent - font->extra.underline_offset - 1;

        if (row + font->underline > font->height)
        {
            row = (font->height - 1) - font->underline;
        }
        y += row;

        /* Fill in the target surface */
        tb.width = xstart;
        tb.height = font->underline;
        tb.wdwidth = (tb.width + 1) >> 1; /* Words per line */
        tb.address = malloc(tb.wdwidth * 2 * tb.height);
        memset(tb.address, (font->extra.effects & 0x2) ? 0xff / 3 : 0xff, tb.wdwidth * 2 * tb.height);

        pxy[2] = tb.width - 1;
        pxy[3] = tb.height - 1;
        pxy[4] = x;
        pxy[5] = y;
        pxy[6] = pxy[4] + tb.width - 1;
        pxy[7] = pxy[5] + tb.height - 1;
        lib_vdi_spppp(&lib_vrt_cpyfm_nocheck, vwk, vwk->mode, pxy, &tb, NULL, colors);

        free(tb.address);
    }

    return NULL;
}


static MFDB *ft2_text_render(Virtual *vwk, Fontheader *font, const short *text, MFDB *textbuf)
{
    int xstart;
    int width;
    int height;
    const short *ch;
    unsigned char *src;
    unsigned char *dst;
    unsigned char *dst_check;
    c_glyph *glyph;

    FT_Bitmap *current;
#if 0
    FT_Error error;
#endif
    FT_Long use_kerning;
    FT_UInt prev_index = 0;
    FT_Face face;

    /* Get the dimensions of the text surface */
#if 0
    if ((ft2_text_size(vwk, font, text, &width, NULL) < 0) || !width)
    {
        /* TTF_SetError("Text has zero width"); */
        return NULL;
    }
#else
    {
        int x, z;
        int minx, maxx;

        minx = maxx = 0;

        /* Load each character and sum its bounding box */
        x = 0;
        for (ch = text; *ch; ++ch)
        {
#if 0
            if (ft2_find_glyph(vwk, font, *ch, CACHED_METRICS))
                continue;
        }
        glyph = font->extra.current;
#else
            short c = *ch;

            /* This should be done via a macro! */
            if (c < 256)
            {
                glyph = &((c_glyph *)font->extra.cache)[c];
            } else
            {
                if (((c_glyph *)font->extra.scratch)->cached != c)
                {
                    ft2_flush_glyph(font->extra.scratch);
                }
                glyph = font->extra.scratch;
            }
            if (!(glyph->stored & CACHED_METRICS))
            {
                if (ft2_load_glyph(vwk, font, c, glyph, CACHED_METRICS))
                {
                    continue;
                }
            }
#endif

            z = x + glyph->minx;
            if (minx > z)
            {
                minx = z;
            }
            if (glyph->advance > glyph->maxx)
            {
                z = x + glyph->advance;
            } else
            {
                z = x + glyph->maxx;
            }
            if (maxx < z)
            {
                maxx = z;
            }
            x += glyph->advance;
        }

        width = maxx - minx;
        if (!width)
            return NULL;
    }
#endif
    height = font->height;

    /* Fill in the target surface */
    textbuf->width = width;
    textbuf->height = height;
    textbuf->standard = 1;
    textbuf->bitplanes = 1;
    /* +1 for end write */
    textbuf->wdwidth = ((width + 15) >> 4) + 1; /* Words per line */
    textbuf->address = malloc(textbuf->wdwidth * 2 * textbuf->height);
    if (textbuf->address == NULL)
    {
        return NULL;
    }
    memset(textbuf->address, 0, textbuf->wdwidth * 2 * textbuf->height);

    /* Adding bounds checking to avoid all kinds of memory
     * corruption errors that may occur.
     */
    dst_check = (unsigned char *) textbuf->address + textbuf->wdwidth * 2 * textbuf->height;

    face = (FT_Face)font->extra.unpacked.data;

    /* Check kerning */
    use_kerning = 0; /* FIXME: FT_HAS_KERNING(face); */

    if (debug > 2)
    {
        PUTS("\n");
        PUTS("Text: ");
    }

    /* Load and render each character */
    xstart = 0;
    for (ch = text; *ch; ++ch)
    {
        short c = *ch;

#ifdef FVDI_DEBUG
        if (debug > 2)
        {
            char buf[2];
            buf[0] = c;
            buf[1] = '\0';
            PUTS(buf);
        }
#endif
#if 0
        int swapped;

        swapped = TTF_byteswapped;
        if (c == UNICODE_BOM_NATIVE)
        {
            swapped = 0;
            if (text == ch)
            {
                ++text;
            }
            continue;
        }
        if (c == UNICODE_BOM_SWAPPED)
        {
            swapped = 1;
            if (text == ch)
            {
                ++text;
            }
            continue;
        }
        if (swapped)
        {
            c = SDL_Swap16(c);
        }
#endif

#if 0
        error = ft2_find_glyph(vwk, font, c, CACHED_METRICS | CACHED_BITMAP);
        if (error)
        {
            continue;
        }
        glyph = font->extra.current;
#else
        /* This should be done via a macro! */
        if (c < 256)
        {
            glyph = &((c_glyph *)font->extra.cache)[c];
        } else
        {
            if (((c_glyph *)font->extra.scratch)->cached != c)
            {
                ft2_flush_glyph(font->extra.scratch);
            }
            glyph = font->extra.scratch;
        }
        if ((glyph->stored & (CACHED_METRICS | CACHED_BITMAP)) != (CACHED_METRICS | CACHED_BITMAP))
        {
            if (ft2_load_glyph(vwk, font, c, glyph, (CACHED_METRICS | CACHED_BITMAP)))
            {
                continue;
            }
        }
#endif
        current = &glyph->bitmap;

        /* Do kerning, if possible AC-Patch */
        if (use_kerning && prev_index && glyph->index)
        {
            FT_Vector delta;

            FT_Get_Kerning(face, prev_index, glyph->index, ft_kerning_default, &delta);
            xstart += delta.x >> 6;
        }
        /* Compensate for wrap around bug with negative minx's */
        if ((ch == text) && (glyph->minx < 0))
        {
            xstart -= glyph->minx;
        }

        {
#if 0
            int offset = xstart + glyph->minx;
            short shift = offset % 8;
            unsigned char rmask = (1 << shift) - 1;
            unsigned char lmask = ~rmask;
            int row, col;

            /* Ensure the width of the bitmap is correct. In some cases,
             * FreeType may report a larger bitmap than possible.
             */
            width = current->width;
            if (width > glyph->maxx - glyph->minx)
            {
                width = glyph->maxx - glyph->minx;
            }

            for (row = 0; row < current->rows; ++row)
            {
                /* Make sure we don't go either over, or under the
                 * limit */
                if (row + glyph->yoffset < 0)
                {
                    continue;
                }
                if (row + glyph->yoffset >= textbuf->height)
                {
                    continue;
                }

                dst = (unsigned char *)textbuf->address +
                      (row + glyph->yoffset) * textbuf->wdwidth * 2 +
                      (offset >> 3);
                src = current->buffer + row * current->pitch;

                for (col = (width + 7) >> 3; col > 0; --col)
                {
                    unsigned char x = *src++;

                    *dst++ |= (x & lmask) >> shift;

                    /* Sanity end of buffer check */
                    if (dst >= dst_check)
                    {
                        break;
                    }

                    *dst |= (x & rmask) << (8 - shift);
                }
            }
#else
            int offset = xstart + glyph->minx;
            short shift = offset % 8;
            int last_row;
            unsigned long byte;
            short row, col, width;
            short dst_inc, src_inc;
#if 0
            unsigned char *src_base, *dst_base;
#else
#define src_base src
#define dst_base dst
#endif

            row = 0;
            src_base = current->buffer;
            dst_base = (unsigned char *)textbuf->address + (offset >> 3);
            if (glyph->yoffset < 0)   /* Under limit? */
            {
                row -= glyph->yoffset;
                src_base += row * current->pitch;
            } else if (glyph->yoffset)
            {
                dst_base += glyph->yoffset * textbuf->wdwidth * 2;
            }
#if 0
            src = src_base;
            dst = dst_base;
#else
#undef src_base
#undef dst_base
#endif

            /* Over limit? */
            last_row = current->rows - 1;
            if (last_row + glyph->yoffset >= textbuf->height)
                last_row = textbuf->height - glyph->yoffset - 1;

            /* Ensure the width of the bitmap is correct. In some cases,
             * FreeType may report a larger bitmap than possible.
             */
            width = current->width;
            if (width > glyph->maxx - glyph->minx)
            {
                width = glyph->maxx - glyph->minx;
            }

            width = ((width + 7) >> 3) - 1;
            dst_inc = textbuf->wdwidth * 2 - (width + 1);
            src_inc = current->pitch - (width + 1);

            /* We need to OR with memory in case previous
             * character "encroached" far into "our" space.
             * Could be special case.
             */
            for (row = last_row - row; row >= 0; --row)
            {
                byte = *dst;
                col = width;
                do
                {
                    unsigned long x = *src++;

                    *dst++ |= byte | (x >> shift);
                    byte = x << (8 - shift);
                } while (--col >= 0);

                *dst |= byte;
                dst += dst_inc;
                src += src_inc;
            }
#endif
        }

        xstart += glyph->advance;

        prev_index = glyph->index;
    }
    if (debug > 2)
    {
        PUTS("\n\n");
    }
    (void) dst_check;

    /* Handle the underline style */
    if (vwk->text.effects & 0x8)
    {
        unsigned char color = 0xff;
        short row = font->distance.ascent - font->extra.underline_offset - 1;

        if (row + font->underline > textbuf->height)
        {
            row = (textbuf->height - 1) - font->underline;
        }
        dst = (unsigned char *)textbuf->address + row * textbuf->wdwidth * 2;
        for (row = font->underline; row > 0; --row)
        {
            /* 1 because 0 is the bg color */
            if (font->extra.effects & 0x2)
                color = row & 1 ? (font->lightening & 0xff) : ~(font->lightening & 0xff);
            memset(dst, color, textbuf->wdwidth * 2);
            dst += textbuf->wdwidth * 2;
        }
    }

    return textbuf;
}


/**
 * Maintains LRU cache of Fontheader instances coresponding to
 * different sizes of FreeType2 fonts loaded in the beginning
 * (which are maintained in the global font list normally).
 **/
static Fontheader *ft2_find_fontsize(Virtual *vwk, Fontheader *font, short ptsize)
{
    static short font_count = 0;
    Fontheader *f;
    FontheaderListItem *i;

    if (!(font->flags & FONTF_SCALABLE))
    {
        /* Not a scalable font:
         *   Fall back to the common add way of finding the right font size */
        f = font->extra.first_size;
        while (f->extra.next_size && (f->extra.next_size->size <= ptsize))
        {
            f = f->extra.next_size;
        }
        /* Set the closest available bitmap font size */
        ptsize = f->size;
    }

    if (debug > 2)
    {
        PRINTF(("FT2 looking for cached_font: id=%ld size=%d eff=%d\n", (long) font->id, ptsize, vwk->text.effects));
    }

    /* LRU: put the selected font to the front of the list */
    {
        LIST *l = &fonts;
        
        listForEach(FontheaderListItem *, i, l)
        {
            if (debug > 2)
            {
                PRINTF(("FT2 cached_font: id==%ld size=%ld eff=%d\n", (long) i->font->id, (long) i->font->size, i->font->extra.effects));
            }
    
            if (i->font->id == font->id &&
                i->font->size == ptsize &&
                i->font->extra.effects == (vwk->text.effects & FT2_EFFECTS_MASK))
            {
                listRemove((LINKABLE *) i);
                listInsert(fonts.head.next, (LINKABLE *) i);
                return i->font;
            }
        }
    }

    if (debug > 1)
    {
        PRINTF(("FT2 find_font: fetch size=%d\n", ptsize));
    }

    /* FIXME: handle maximum number of fonts in the cache here (configurable) */

    /* BUG! Cannot dispose any Fontheader structure when held by a vwk.
     *      Either all functions would update the current_font pointer when
     *      called or all currently used by vwks need to stay in mem.
     */
    if (font_count > 10)
    {
        FontheaderListItem *x = (FontheaderListItem *)listLast(&fonts);

        /* Flush the cache anyway */
        if (x)
            ft2_flush_cache(x->font);

        if (debug > 0)
        {
            PRINTF(("FT2 find_font: flush cache: %s ID=%ld refs=%ld\n",
                x ? x->font->extra.filename : "[null]",
                (long) (x ? x->font->id : -1),
                (long) (x ? x->font->extra.ref_count : -1)));
        }

        /* Find the first one that we can dispose */
        while (x && x->font->extra.ref_count)
            x = (FontheaderListItem *)listPrev(x);

        if (x)
        {
            if (debug > 0)
            {
                PRINTF(("FT2 find_font: found disposable victim: %s refs=%ld\n",
                    x->font->extra.filename,
                    (long) x->font->extra.ref_count));
            }

            listRemove((LINKABLE *) x);
            ft2_dispose_font(x->font); /* Remove the whole font */
            free(x);
        }
        font_count--;
    }

    /* Create additional size/effects face */
    f = ft2_dup_font(vwk, font, ptsize);
    if (f)
    {
        i = malloc(sizeof(FontheaderListItem));
        i->font = f;
        listInsert(fonts.head.next, (LINKABLE *) i);
        font_count++;
    }

    return f;
}


long ft2_text_render_default(Virtual *vwk, unsigned long coords, short *s, long slen)
{
    Fontheader *font = vwk->text.current_font;
    MFDB textbuf, *t;

#ifdef DEBUG_FONTS
    if (debug > 2)
    {
        PRINTF(("Text len: %ld\n", slen));
    }
#endif

    /* FIXME: this should not happen once we have all the font id/size setup routines intercepted */
    if (!font->size)
    {
        PUTS("FT2 RENDERER CALLED FOR 0 size: text_render_default font->size == 0\n");
        /* Create a copy of the font for the particular size */
        font = ft2_find_fontsize(vwk, font, 16);
        if (!font)
        {
            PUTS("Cannot open face\n");
            return 0;
        }
    }

    /* Terminate text */
    s[slen] = 0;

    if (antialiasing)
    {
        short x = coords >> 16;
        short y = coords & 0xffffUL;

        ft2_text_render_antialias(vwk, font, x, y, s, &textbuf);
    } else
    {
        t = ft2_text_render(vwk, font, s, &textbuf);
        if (t && t->address)
        {
            short colors[2];
            short pxy[8];
            short x = coords >> 16;
            short y = coords & 0xffffUL;

            colors[1] = vwk->text.colour.background;
            colors[0] = vwk->text.colour.foreground;

            y += ((short *) &font->extra.distance)[vwk->text.alignment.vertical];

            pxy[0] = 0;
            pxy[1] = 0;
            pxy[2] = t->width - 1;
            pxy[3] = t->height - 1;
            pxy[4] = x;
            pxy[5] = y;
            pxy[6] = x + t->width - 1;
            pxy[7] = y + t->height - 1;

            lib_vdi_spppp(&lib_vrt_cpyfm_nocheck, vwk, vwk->mode, pxy, t, NULL, colors);
            free(t->address);
        }
    }

    /* Dispose of the FreeType2 objects */
    /* ft2_close_face(font); */

    return 1;
}


long ft2_char_width(Virtual *vwk, Fontheader *font, long ch)
{
    short s[] = { ch, 0 };
    int width;

    /* Get the dimensions of the text surface */
    if (ft2_text_size(vwk, font, s, &width, NULL) < 0)
    {
        return 0;
    }

    return width;
}


long ft2_text_width(Virtual *vwk, Fontheader *font, short *s, long slen)
{
    int width;

    /* Terminate text */
    s[slen] = 0;
    /* Get the dimensions of the text surface */
    if (ft2_text_size(vwk, font, s, &width, NULL) < 0)
    {
        return 0;
    }

    return width;
}


long ft2_set_effects(Virtual *vwk, Fontheader *font, long effects)
{
    effects &= vwk->real_address->writing.effects;
    vwk->text.effects = effects;

    /* Update the font metrics after effects change */
    font = ft2_find_fontsize(vwk, font, font->size);

    /* Assign and update the ref_counts */
    if (vwk->text.current_font != font && font)
    {
        if (vwk->text.current_font)
            vwk->text.current_font->extra.ref_count--;
        font->extra.ref_count++;
        vwk->text.current_font = font;
    }

    return effects;
}


Fontheader *ft2_vst_point(Virtual *vwk, long ptsize, short *sizes)
{
    Fontheader *font = vwk->text.current_font;
    short i = 1;

    if (font->size == ptsize)
        return font;

    if (ptsize > 32000)
        ptsize = 32000;

    if (sizes == NULL || sizes[0] == -1)
    {
        PRINTF(("No search for size %ld\n", ptsize));
    }
    else
    {
        PRINTF(("Searching $%08lx for %ld: ", (long) sizes, ptsize));
        while (sizes[i] <= ptsize && sizes[i] != -1)
        {
            i++;
        }
        ptsize = sizes[i - 1];
        PRINTF(("%ld\n", ptsize));
    }

    /* NEED to update metrics to be up-to-date immediately after vst_point() */
    font = ft2_find_fontsize(vwk, font, ptsize);

    /* Dispose of the FreeType2 objects */
    /* ft2_close_face(font); */

    return font;
}


unsigned short CDECL ft2_char_index(Virtual *vwk, Fontheader *font, short *intin)
{
    unsigned short ch = intin[0];
    short src_mode = intin[1];
    short dst_mode = intin[2];
    FT_Face face;
    FT_UInt index;

    face = ft2_get_face(vwk, font);
    if (!face)
    {
        access->funcs.puts(ft2_error("FT2  Couldn't get face", 0));
        return 0xffff;
    }

    /*
     * same logic here as in ft2_load_glyph
     */
    if (src_mode == MAP_ATARI && ch < 256)
    {
        unsigned short cc = 0xffff;

        /* if there is no spdchar.map (or not enough data)
         * -> use built-in mapping */
        if (spdchar_map_len < 450)
        {
            cc = Atari2Bics[ch];
        } else
        {
            if (ch > 31)
            {
                /* NVDI really seems to be using the _first_ map '00' in the
                 * spdchar.map file no matter what the font format is.
                 *
                 * The TT table seems to contain a lot of crap for non ASCII
                 * chars commonly.
                 */
                cc = spdchar_map.bs_int.map[ch - 32];
            }

            /* in case we don't have BICS code => fallback to the default mapping */
            if (cc >= BICS_COUNT || ch < 32)
            {
                cc = Atari2Bics[ch];
            }
        }

        /* in case we don't have BICS -> UNICODE the cc value will be -1 */
        cc = Bics2Unicode[cc];

        /* get the font character index */
        index = FT_Get_Char_Index(face, cc == 0xffff ? (FT_ULong)-1 : cc);

        /* When there is no such character in the font (or cc was -1)
         * we fallback to the default built-in mapping */
        if (index == 0)
        {
            cc = Atari2Bics[ch];

            /* valid BICS code => to unicode */
            cc = Bics2Unicode[cc];

            /* get the font character index */
            index = FT_Get_Char_Index(face, cc == 0xffff ? (FT_ULong)-1 : cc);
        }
        ch = cc;
    } else if (src_mode == MAP_UNICODE)
    {
        /* app use: used at least by the 'Highwire web browser' */
        index = FT_Get_Char_Index(face, ch);
    } else if (src_mode == MAP_BITSTREAM)
    {
        /* BICS */ /* no char -> index translation, BICS char index is expected */

        /* app use: might perhaps be used by the 'charmap5' spdchar.map editor */
        unsigned short cc = ch >= BICS_COUNT ? -1 : Bics2Unicode[ch];

        index = FT_Get_Char_Index(face, cc == 0xffff ? (FT_ULong)-1 : cc);
        ch = cc;
    } else
    {
        kprintf("FT2 SRC MAPPING(%d) THAT SHOULD HAVE NEVER HAPPENED!!!\n", src_mode);
        index = 0;
    }

    if (index == 0)
        return 0xffff;

    if (dst_mode == MAP_UNICODE)
    {
        return ch;
    } else if (dst_mode == MAP_ATARI)
    {
        const short *table;
        int i;
        unsigned short cc;

        if (spdchar_map_len < 450)
        {
            table = Atari2Bics;
            i = 0;
        } else
        {
            table = spdchar_map.bs_int.map - 32;
            i = 32;
        }
        for (; i < 256; i++)
        {
            if (table[i] >= 0)
            {
                cc = table[i];
                if (cc < BICS_COUNT && Bics2Unicode[cc] == ch)
                    return i;
            }
        }
    } else if (dst_mode == MAP_BITSTREAM)
    {
        const unsigned short *table = Bics2Unicode;
        int i;

        for (i = 0; i < 256; i++)
            if (table[i] == ch)
                return i;
    } else
    {
        /* invalid mapping mode */
    }

    return 0xffff;
}


int sprintf(char *str, const char *format, ...)
{
    va_list args;
    long ret;

    va_start(args, format);
    ret = kvsprintf(str, format, args);
    va_end(args);
    return ret;
}


/*
 * since freetype 2.6.5, some calls to atol() were replaced by strtol.
 */
#if FREETYPE_VERSION >= 2006005L
#define ISSPACE(c) ((c) == ' '|| (c) == '\t')
#define ISDIGIT(c) ((c) >= '0' && (c) <= '9')

static unsigned long __strtoul_internal(const char *nptr, char **endptr, int base, int *sign)
{
	long ret = 0;
	const char *ptr = nptr;
	int val;
	short ret_ok = 0;
	char overflow = 0;

	if (base != 0 && 2 > base && base > 36)
		goto error;

	while (*ptr && ISSPACE(*ptr))
		ptr++;							/* skip spaces */

	if ((*sign = (*ptr == '-')))
		ptr++;

	if (!*ptr)
		goto error;

	if (*ptr == '0')
	{
		ret_ok = 1;
		switch (*++ptr & ~0x20)
		{
		case 'B':
			if (base != 0 && base != 2)
				goto error;
			base = 2;
			ptr++;
			break;
		case 'X':
			if (base != 0 && base != 16)
				goto error;
			base = 16;
			ptr++;
			break;
		default:
			if (base == 0)
				base = 8;
			break;
		}
	} else if (base == 0)
		base = 10;

	for (; *ptr; ptr++)
	{
		if (ISDIGIT(*ptr))
			val = *ptr - '0';
		else
		{
			val = 10 + (*ptr & ~0x20 /*TOUPPER*/) - 'A';
			if (val < 10)
				val = 37;
		}
		ret_ok = 1;
		if (val >= base)
			break;
		if (!overflow)
		{
			if (ret)
				ret = ret * base;
			ret = ret + val;
		}
	}
	if (ret_ok)
	{
		if (endptr)
			*endptr = (char *) ptr;
		return overflow ? ULONG_MAX : ret;
	}
  error:
	if (endptr)
		*endptr = (char *) nptr;
	/* TODO set errno */
	return 0;
}

/* redefined in libkern.h */
long _mint_strtol(const char *nptr, char **endptr, long base)
{
	int sign;
	unsigned long ret = __strtoul_internal(nptr, endptr, base, &sign);
	return ret > LONG_MAX ? (sign ? LONG_MIN : LONG_MAX) : (sign ? -ret : ret);
}

/*
 * we may also need getenv
 */
char *getenv(const char *str)
{
	(void) str;
	return 0;
}

#endif


/*
 * In some versions, ftlcdfil.c is not compiled as part of the base module,
 * and we get symbol reference errors if FT_CONFIG_OPTION_SUBPIXEL_RENDERING
 * was defined
 */
#if FREETYPE_VERSION < 2009001 && FREETYPE_VERSION >= 2008000 && defined(FT_CONFIG_OPTION_SUBPIXEL_RENDERING)
#include <freetype/ftlcdfil.h>
  FT_BASE( void )
  ft_lcd_filter_fir( FT_Bitmap*           bitmap,
                     FT_Render_Mode       mode,
                     FT_LcdFiveTapFilter  weights )
  {
    FT_UNUSED( bitmap );
    FT_UNUSED( mode );
    FT_UNUSED( weights );
  }
#endif
