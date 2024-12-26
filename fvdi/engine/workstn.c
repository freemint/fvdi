/*
 * fVDI workstation functions
 *
 * Copyright 2000/2003, Johan Klockars
 * This software is licensed under the GNU General Public License.
 * Please, see LICENSE.TXT for further information.
 */

#include "fvdi.h"
#include "stdio.h"
#include "function.h"
#include "relocate.h"
#include "utility.h"
#include "globals.h"

#define NEG_PAL_N	9	/* Number of negative palette entries */

/*
 * attention: also accessed in fvdi.s
 */
struct extra_handles {
	long count;
	struct extra_handles *next;
	Virtual *handles[62];
};
struct extra_handles *handle_link = 0;

int lib_vq_extnd(Virtual *vwk, long subfunction, long flag, short *intout, short *ptsout);


void linea_setup(Workstation *wk)
{
    unsigned short *linea, width, height, bitplanes;
    static unsigned short linea_orig[12];
    static short init = 1;

    linea = wk->screen.linea;

    if (init)
    {
        init = 0;
        linea_orig[0] = linea[-0x2b4 / 2];  /* DEV_TAB[0] */
        linea_orig[1] = linea[-0x2b2 / 2];  /* DEV_TAB[1] */
        linea_orig[2] = linea[-0x00c / 2];  /* V_REZ_HZ */
        linea_orig[3] = linea[-0x004 / 2];  /* V_REZ_VT */
        linea_orig[4] = linea[-0x306 / 2];  /* INQ_TAB[4] */
        linea_orig[5] = linea[0];           /* PLANES */
        linea_orig[6] = linea[1];           /* WIDTH */
        linea_orig[7] = linea[-1];          /* BYTES_LIN */
        linea_orig[8] = linea[-0x304 / 2];  /* INQ_TAB[5] */
        linea_orig[9] = linea[-0x266 / 2];  /* DEV_TAB[39] */
        linea_orig[10] = linea[-0x26e / 2]; /* DEV_TAB[35] */
        linea_orig[11] = linea[-0x29a / 2]; /* DEV_TAB[13] */
    }

    /* Copy a few things into the lineA variables */
    width = wk->screen.mfdb.width;
    height = wk->screen.mfdb.height;
    linea[-0x2b4 / 2] = width - 1;    /* DEV_TAB[0] */
    linea[-0x2b2 / 2] = height - 1;   /* DEV_TAB[1] */
    linea[-0x00c / 2] = width;        /* V_REZ_HZ */
    linea[-0x004 / 2] = height;       /* V_REZ_VT */
    if (lineafix)
    {
        /* Should cover more really */
        bitplanes = wk->screen.mfdb.bitplanes;
        linea[-0x306 / 2] = bitplanes;  /* INQ_TAB[4] */
        linea[0] = bitplanes;
        width = (width >> 3) * bitplanes;
        linea[1] = width;   /* WIDTH */
        linea[-1] = width;  /* BYTES_LIN */
    } else if (!init)
    {
        linea[-0x306 / 2] = linea_orig[4];  /* INQ_TAB[4] */
        linea[0] = linea_orig[5];   /* PLANES */
        linea[1] = linea_orig[6];   /* WIDTH */
        linea[-1] = linea_orig[7];  /* BYTES_LIN */
    }

    linea[-0x304 / 2] = wk->screen.look_up_table;          /* INQ_TAB[5] */
    linea[-0x266 / 2] = wk->screen.palette.possibilities;  /* DEV_TAB[39] */
    linea[-0x26e / 2] = wk->screen.colour;                 /* DEV_TAB[35] */
    linea[-0x29a / 2] = wk->screen.palette.size;           /* DEV_TAB[13] */
}


/* Find virtual workstation entry for a handle */
static Virtual **find_handle_entry(short hnd)
{
    short handles;
    struct extra_handles *link;

    handles = HANDLES;
    if (hnd < handles)
        return &handle[hnd];

	/* attention: keep in sync with large_handle in fvdi.s */
    link = handle_link;
    while (link)
    {
        hnd -= handles;
        if (debug)
        {
            PUTS("Looking for handle in extra table\n");
        }
        handles = link->count;
        if (hnd < handles)
            return &link->handles[hnd];
        link = link->next;
    }

    return NULL;
}


/* Find (or create, if necessary) a free handle */
static short find_free_handle(Virtual ***handle_entry)
{
    short hnd, handles;
    struct extra_handles *link, **last;
    Virtual **handle_table;

    handles = HANDLES;
    for (hnd = 1; hnd < handles; hnd++)
    {
        if (handle[hnd] == non_fvdi_vwk)
        {
            *handle_entry = &handle[hnd];
            return hnd;
        }
    }

    last = &handle_link;
    link = handle_link;
    while (link != NULL)
    {
        if (debug)
        {
            PUTS("Looking for free handle in extra table\n");
        }
        handles += link->count;
        handle_table = link->handles;
        for (; hnd < handles; hnd++, handle_table++)
        {
            if (*handle_table == non_fvdi_vwk)
            {
                *handle_entry = handle_table;
                return hnd;
            }
        }
        last = &link->next;
        link = link->next;
    }

    handles = sizeof(link->handles) / sizeof(link->handles[0]);
    /* be paranoid: don't allow VDI handles to overflow range of signed short */
    if (hnd >= (short)((32767 - handles) / sizeof(void *)))
        return 0;

    link = (struct extra_handles *)malloc(sizeof(*link));
    if (link)
    {
        if (debug)
        {
            PRINTF(("Allocated space for %d extra handles\n", handles));
        }
        link->count = handles;
        link->next = NULL;
        for (; --handles >= 0; )
            link->handles[handles] = non_fvdi_vwk;
        *last = link;
        *handle_entry = &link->handles[0];
        return hnd;
    }

    return 0;
}


/* Needs to deal with virtuals on non-screen workstations! */
void CDECL v_opnvwk(Virtual *vwk, VDIpars *pars)
{
    short *intin;
    short hnd, width, height, bitplanes, lwidth, dummy;
    long extra_size;
    long size;
    Workstation *wk, *new_wk;
    Virtual *new_vwk, **handle_entry;
    MFDB *mfdb;
    long colors;
    short format;
    short bit_order;
    unsigned short c;

    pars->control->handle = 0;	/* Assume failure */
    if ((hnd = find_free_handle(&handle_entry)) == 0)
    {
        PRINTF(("v_opnvwk: no free handle\n"));
        return;
    }

    wk = vwk->real_address;
    /* 32 - user fill pattern */
    extra_size = 32;
    extra_size += sizeof(*vwk);

    /* Check if really v_opnbm */
    if (pars->control->subfunction != 1 || pars->control->l_intin < 20)
    {
        if ((new_vwk = malloc(extra_size)) == NULL)
        {
            PRINTF(("v_opnbm[v_opnvwk]: out of memory\n"));
            return;
        }
        copymem(wk->driver->default_vwk, new_vwk, sizeof(Virtual));
        vwk = new_vwk;
    } else
    {
        mfdb = (MFDB *)pars->control->addr1;
        intin = pars->intin;

        if (!mfdb)
        {
            PRINTF(("v_opnbm: NULL mfdb\n"));
            return;
        }
        /* Doesn't allow the EdDI v1.1 variant yet */
        colors = *((long *)&intin[15]);
        if (colors != 0 && colors != 2 && colors != (1L << wk->screen.mfdb.bitplanes))
        {
            PRINTF(("v_opnbm: unsupported colors %ld\n", colors));
            return;
        }
        bitplanes = mfdb->bitplanes;
        if (bitplanes == 0)
            bitplanes = wk->screen.mfdb.bitplanes;
        /* Only same as physical or one allowed */
        if (bitplanes != 1 && bitplanes != wk->screen.mfdb.bitplanes)
        {
            PRINTF(("v_opnbm: unsupported planes %d\n", bitplanes));
            return;
        }
        format = intin[18];
        if (format != 0 && format != 1)
        {
            PRINTF(("v_opnbm: unsupported format %d\n", format));
            return;
        }
        bit_order = intin[19];
        if (bit_order != 0 && bit_order != 1)
        {
            PRINTF(("v_opnbm: unsupported bit order %d\n", bit_order));
            return;
        }
        
        extra_size += sizeof(*wk);

        if (mfdb->address || intin[11] || intin[12])
        {
            width = intin[11] ? (intin[11] + 1) : wk->screen.mfdb.width;
            height = intin[12] ? (intin[12] + 1) : wk->screen.mfdb.height;
        } else
        {
            width = wk->screen.mfdb.width;
            height = wk->screen.mfdb.height;
        }
        width = (width + 15) & 0xfff0;

        lwidth = (width >> 3) * bitplanes;	/* >> 4 in assembly file */
        size = (long)lwidth * height;

        if (!mfdb->address)
            extra_size += size;

        /* New vwk, but it should really not always be for this driver! */
        if ((new_vwk = malloc(extra_size)) == NULL)
        {
            PRINTF(("v_opnbm: out of memory (%ld)\n", extra_size));
            return;
        }

        new_wk = (Workstation *)((long)new_vwk + sizeof(Virtual) + 32);
        copymem(wk, new_wk, sizeof(Workstation));
        copymem(wk->driver->default_vwk, new_vwk, sizeof(Virtual));

        if (!mfdb->address)
        {
            mfdb->standard = 0;
            mfdb->address = (short *)((long)new_wk + sizeof(Workstation));
            memset(mfdb->address, bitplanes > 8 ? -1 : 0, size);
        }

        new_wk->screen.mfdb.address = mfdb->address;
        mfdb->width = new_wk->screen.mfdb.width = width;
        mfdb->height = new_wk->screen.mfdb.height = height;
        mfdb->wdwidth = new_wk->screen.mfdb.wdwidth = (width >> 4);	/* Right? */
        mfdb->bitplanes = new_wk->screen.mfdb.bitplanes = bitplanes;
        mfdb->reserved[0] = new_wk->screen.mfdb.reserved[0] = 0;
        mfdb->reserved[1] = new_wk->screen.mfdb.reserved[1] = 0;
        mfdb->reserved[2] = new_wk->screen.mfdb.reserved[2] = 0;
        new_wk->screen.mfdb.standard = 0;
        if (mfdb->standard)	/* Need to convert input MFDB to device dependent format? */
            lib_vdi_pp(lib_vr_trnfm, new_vwk, mfdb, mfdb);

        new_wk->screen.type = 0;
        new_wk->screen.wrap = lwidth;		/* Right? */
        new_wk->screen.shadow.buffer = 0;
        new_wk->screen.shadow.address = 0;
        new_wk->screen.shadow.wrap = 0;

        new_wk->screen.pixel.width = intin[13];
        new_wk->screen.pixel.height = intin[14];

        new_wk->screen.coordinates.course = 0;	/* ? */
        new_wk->screen.coordinates.min_x = 0;
        new_wk->screen.coordinates.min_y = 0;
        new_wk->screen.coordinates.max_x = width - 1;
        new_wk->screen.coordinates.max_y = height - 1;

        /* Probably OK to mark all these as unavailable */
        new_wk->various.input_type = 0;
        new_wk->various.inking = 0;
        new_wk->various.buttons = 0;
        new_wk->various.cursor_movement = 0;
        new_wk->various.number_entry = 0;
        new_wk->various.selection = 0;
        new_wk->various.typing = 0;
        new_wk->various.workstation_type = 0;
        new_wk->mouse.type = 0;				/* Enough? */

        new_vwk->real_address = new_wk;
        vwk = new_vwk;
        wk = new_wk;
    }

    vwk->fill.user.pattern.in_use = (short *)((long)vwk + sizeof(Virtual));
    vwk->fill.user.pattern.extra = 0;
    vwk->fill.user.multiplane = 0;

    /* Return information about workstation */
    lib_vq_extnd(vwk, 0, 0, pars->intout, pars->ptsout);

    pars->control->handle = vwk->standard_handle = hnd;
    *handle_entry = vwk;

    /* Call various setup functions (most with supplied data) */
    c = pars->intin[1];
    if (c < 1 || c > wk->drawing.line.types)
        c = 1;
    vwk->line.type = c;
    c = pars->intin[2];
    if (c >= wk->screen.palette.size)
        c = BLACK;
    vwk->line.colour.foreground = c;
    c = pars->intin[3];
    if (c < 1 || c > wk->drawing.marker.types)
        c = 3; /* Asterisk */
    vwk->marker.type = c;
    c = pars->intin[4];
    if (c >= wk->screen.palette.size)
        c = BLACK;
    vwk->marker.colour.foreground = c;
    lib_vst_font(vwk, pars->intin[5]);
    /* Default to 10 or 9 point font */
    lib_vst_point(vwk, wk->screen.mfdb.height >= 400 ? 10 : 9, &dummy, &dummy, &dummy, &dummy);
    c = pars->intin[6];
    if (c >= wk->screen.palette.size)
        c = BLACK;
    vwk->text.colour.foreground = c;
    c = pars->intin[7];
    if (c > 4)
        c = 0; /* Hollow */
    vwk->fill.interior = c;
    c = pars->intin[8];
    if (c < 1 || c > 24)
        c = 1;
    vwk->fill.style = c;
    c = pars->intin[9];
    if (c >= wk->screen.palette.size)
        c = BLACK;
    vwk->fill.colour.foreground = c;
    lib_vs_clip(vwk, 0, NULL);  /* No clipping (set to max size) */
    /* Should also take care of the coordinate values that come now */
}


void CDECL v_opnwk(VDIpars *pars)
{
    Driver *driver;
    Virtual *vwk, **handle_entry;
    Workstation *wk;
    unsigned short hnd, oldhnd;

    /* For now, just assume that any
     * workstation handle >10 is non-fVDI
     */
    if (pars->intin[0] > LAST_SCREEN_DEVICE)
    {
        int failed = 1;

        pars->control->handle = 0;	/* Assume failure */
        vwk = 0;
        if (old_gdos)
        {
            /* No pass-through without old GDOS */
            if ((hnd = find_free_handle(&handle_entry)) != 0)
            {
#if __GNUC__ >= 14
#pragma GCC diagnostic ignored "-Walloc-size"
#endif
                if ((vwk = malloc(sizeof(Workstation *) + sizeof(short))) != NULL)
#if __GNUC__ >= 14
#pragma GCC diagnostic warning "-Walloc-size"
#endif
                {
                    if ((oldhnd = call_other(pars, 0)) != 0)
                    {
                        /* Dummy handle for call */
                        failed = 0;
#if __GNUC__ >= 11
#pragma GCC diagnostic ignored "-Warray-bounds"
#endif
                        vwk->real_address = non_fvdi_wk;
                        /* Mark as pass-through handle */
                        vwk->standard_handle = oldhnd | 0x8000;
#if __GNUC__ >= 11
#pragma GCC diagnostic warning "-Warray-bounds"
#endif
                        *handle_entry = vwk;
                        pars->control->handle = hnd;
                    } else
                    {
                        PRINTF(("call_other failed (%d)\n", pars->intin[0]));
                    }
                } else
                {
                    PUTS("malloc failed\n");
                }
            } else
            {
                PUTS("find_free_handle failed\n");
            }
        } else
        {
            PUTS("no old GDOS\n");
        }

        if (failed)
        {
            if (vwk)
                free(vwk);		/* Couldn't open */
        }

        return;
    }

#if 0
    /* Experimenting, 001217/010109 */
    if (!old_wk_handle && !stand_alone)
    {
        short intout[45], ptsout[12];

        old_wk_handle = scall_v_opnwk(1, intout, ptsout);
    }
#endif

    driver = (Driver *)driver_list->value;
    if ((vwk = driver->opnwk(default_virtual)) != NULL)
        ;				/* Should probably do something */
    else
        vwk = driver->default_vwk;

    /* To accomodate mouse drawing (only for one screen wk) */
    screen_wk = wk = vwk->real_address;

    linea_setup(wk);
    init_interrupts();

    v_opnvwk(vwk, pars);

    old_wk_handle = pars->control->handle;

    screen_vwk = *find_handle_entry(pars->control->handle);
}


void CDECL v_clsvwk(Virtual *vwk, VDIpars *pars)
{
    Virtual **handle_entry;
    short hnd;

    hnd = vwk->standard_handle;
    if (hnd == 0)	/* Check if default VDI structure */
        return;
    if (hnd < 0)
    {
        call_other(pars, hnd & 0x7fff);
        hnd = pars->control->handle;
    } else
    {
        if (vwk->fill.user.pattern.extra)
            free(vwk->fill.user.pattern.extra);
        if (vwk->palette != NULL)
            free((void *)(((long)vwk->palette & ~1) - NEG_PAL_N * sizeof(Colour)));
    }

    if (vwk->text.current_font)
        vwk->text.current_font->extra.ref_count--; /* Allow the font to be freed if appropriate */
    free(vwk);	/* This will work for off-screen bitmaps too, fortunately */

    /* Reset VDI structure address to default */
    handle_entry = find_handle_entry(hnd);
    if (!handle_entry)
    {
        PUTS("Handle could not be found in tables!\n");
    } else if (*handle_entry != vwk)
    {
        PUTS("Wrong handle entry!\n");
    } else
    {
        *handle_entry = non_fvdi_vwk;
    }
}


/* Needs to be able to deal with multiple fVDI workstations! */
void CDECL v_clswk(Virtual *vwk, VDIpars *pars)
{
    Driver *driver;
    Workstation *wk;

    wk = vwk->real_address;
    v_clsvwk(vwk, pars);

    if (wk != non_fvdi_wk && (driver = wk->driver) != NULL && driver->module.id <= LAST_SCREEN_DEVICE)
    {
        screen_wk = 0;

        driver->clswk(vwk);

#if 0
        if (old_wk_handle && !stand_alone)
            scall_v_clswk(old_wk_handle);
#endif
        reset_interrupts();
    }
}


void CDECL vq_devinfo(VDIpars *pars)
{
    /* For now, just assume that any
     * workstation handle >10 is non-fVDI
     */
    if (pars->intin[0] > LAST_SCREEN_DEVICE)
    {
        if (old_gdos)
        {
            /* No pass-through without old GDOS */
            call_other(pars, 0);	/* Dummy handle for call */
#ifdef FVDI_DEBUG
            {
                int i;
                char buf[2];
                
                PUTS("vq_devinfo:\n");
                display_output(pars);
                buf[1] = '\0';
                PUTS("filename: ");
                for (i = 0; i < pars->control->l_intout; i++)
                {
                    buf[0] = pars->intout[i];
                    PUTS(buf);
                }
                PUTS("\n");
                PUTS("device name: ");
                for (i = 1; i < pars->control->l_ptsout; i++)
                {
                    buf[0] = pars->ptsout[i];
                    PUTS(buf);
                }
                PUTS("\n");
            }
#endif
        } else
        {
            PUTS("no old GDOS (vq_devinfo)\n");
        }
        return;
    }

    pars->intout[0] = 0;
    pars->ptsout[0] = 1;
    pars->ptsout[1] = 0;
    pars->control->l_intout = 0;
    pars->control->l_ptsout = 1;
    PUTS("fVDI handles currently don't support vq_devinfo\n");
}


int lib_vq_extnd(Virtual *vwk, long subfunction, long flag, short *intout, short *ptsout)
{
    Workstation *wk = vwk->real_address;

    if (flag == 2 && subfunction == 1)
    {
        copymem_aligned(wk->driver->device, intout, sizeof(Device));
        ((Device *)intout)->address = wk->screen.mfdb.address;
        ((Device *)intout)->byte_width = wk->screen.wrap;
        return 1;
    } else if (flag)
    {
        *intout++ = wk->screen.type;
        *intout++ = wk->screen.bkg_colours;
        *intout++ = wk->writing.effects;
        *intout++ = wk->raster.scaling;
        *intout++ = wk->screen.mfdb.bitplanes;
        *intout++ = wk->screen.look_up_table;
        *intout++ = wk->raster.performance;
        *intout++ = wk->drawing.flood_fill;
        *intout++ = wk->writing.rotation.type;
        *intout++ = wk->drawing.writing_modes;
        *intout++ = wk->various.input_type;
        *intout++ = wk->writing.justification;
        *intout++ = wk->various.inking;
        *intout++ = wk->drawing.rubber_banding;
        *intout++ = wk->various.max_ptsin;
        *intout++ = wk->various.max_intin;
        *intout++ = wk->various.buttons;
        *intout++ = wk->drawing.line.wide.types_possible;
        *intout++ = wk->drawing.line.wide.writing_modes;
        *intout++ = vwk->clip.on;  /* 19 - originally reserved from here on */
        *intout++ = 0;   /* No pixel sizes below (1 - 0.1 um, 2 - 0.01, 3 - 0.001 */
        *intout++ = 0;   /* Pixel width */
        *intout++ = 0;   /* Pixel height */
        *intout++ = 0;   /* Horizontal dpi */
        *intout++ = 0;   /* Vertical dpi */
        *intout++ = 0;   /* Bit image rotation on printer (PC-GEM/3) */
        *intout++ = 0;   /* AES buffer address (PC-GEM/3) */
        *intout++ = 0;   /*         -    "    -           */
        *intout++ = 2;   /* Beziers! */
        *intout++ = 0;   /* Unused */
        *intout++ = 0;   /* 1 - bitmap scale, 2 - new raster functions, 4 - vr_clip_rects_xxx */
        *intout++ = 0;   /* 9 from here used to be reserved */
        *intout++ = 1;   /* New style colour routines (at least some of them) */
        *intout++ = 0;
        *intout++ = 0;
        *intout++ = 0;
        *intout++ = 0;
        *intout++ = 0;
        *intout++ = 0;
        *intout++ = 0;
        *intout++ = 0;   /* Unusable left border */
        *intout++ = 0;   /*          upper       */
        *intout++ = 0;   /*          right       */
        *intout++ = 0;   /*          lower       */
        *intout++ = 0;   /* Paper format (default/A3/A4/A5/B5/letter/half/legal/double/broad */

        *ptsout++ = vwk->clip.rectangle.x1;
        *ptsout++ = vwk->clip.rectangle.y1;
        *ptsout++ = vwk->clip.rectangle.x2;
        *ptsout++ = vwk->clip.rectangle.y2;
        *ptsout++ = 0;   /* Reserved from here on */
        *ptsout++ = 0;
        *ptsout++ = 0;
        *ptsout++ = 0;
        *ptsout++ = 0;
        *ptsout++ = 0;
        *ptsout++ = 0;
        *ptsout++ = 0;

        return 0;
    } else
    {
        long attributes;
        int i, n;

        *intout++ = wk->screen.coordinates.max_x;
        *intout++ = wk->screen.coordinates.max_y;
        *intout++ = wk->screen.coordinates.course;
        *intout++ = wk->screen.pixel.width;
        *intout++ = wk->screen.pixel.height;
        *intout++ = wk->writing.size.possibilities;
        *intout++ = wk->drawing.line.types;
        *intout++ = wk->drawing.line.wide.width.possibilities;
        *intout++ = wk->drawing.marker.types;
        *intout++ = wk->drawing.marker.size.possibilities;
        *intout++ = 1;     /* Fonts */
        *intout++ = wk->drawing.fill.patterns;
        *intout++ = wk->drawing.fill.hatches;
        *intout++ = wk->screen.palette.size;
        *intout++ = wk->drawing.primitives.supported;

        attributes = wk->drawing.primitives.attributes;
        n = 0;
        for (i = 9; i >= 0; i--)
        {
            if (attributes & 0x07)
            {
                n++;
                intout[i] = 10 - i;
                intout[i + 10] = (attributes & 0x07) - 1;
            }
            attributes >>= 3;
        }
        if (n < 10)
            intout[i + n] = -1;
        intout += 2 * 10;

        *intout++ = wk->screen.colour;
        *intout++ = wk->writing.rotation.possible;
        *intout++ = wk->drawing.fill.possible;
        *intout++ = wk->drawing.cellarray.available;
        *intout++ = wk->screen.palette.possibilities;
        *intout++ = wk->various.cursor_movement;
        *intout++ = wk->various.number_entry;
        *intout++ = wk->various.selection;
        *intout++ = wk->various.typing;
        *intout++ = wk->various.workstation_type;

        *ptsout++ = wk->writing.size.width.min;
        *ptsout++ = wk->writing.size.height.min;
        *ptsout++ = wk->writing.size.width.max;
        *ptsout++ = wk->writing.size.height.max;
        *ptsout++ = wk->drawing.line.wide.width.min;
        *ptsout++ = 0;
        *ptsout++ = wk->drawing.line.wide.width.max;
        *ptsout++ = 0;
        *ptsout++ = wk->drawing.marker.size.width.min;
        *ptsout++ = wk->drawing.marker.size.height.min;
        *ptsout++ = wk->drawing.marker.size.width.max;
        *ptsout++ = wk->drawing.marker.size.height.max;
        return 0;
    }
}


void lib_vs_clip(Virtual *vwk, short on, short *rect)
{
    Workstation *wk = vwk->real_address;

    vwk->clip.on = on;
    
    if (on)
    {
        short x;
        
        x = *rect++;
        if (x < 0)
            x = 0;
        vwk->clip.rectangle.x1 = x;
        x = *rect++;
        if (x < 0)
            x = 0;
        vwk->clip.rectangle.y1 = x;
        x = *rect++;
        if (x >= wk->screen.coordinates.max_x)
            x = wk->screen.coordinates.max_x;
        vwk->clip.rectangle.x2 = x;
        x = *rect++;
        if (x >= wk->screen.coordinates.max_y)
            x = wk->screen.coordinates.max_y;
        vwk->clip.rectangle.y2 = x;
    } else
    {
        vwk->clip.rectangle.x1 = vwk->clip.rectangle.y1 = 0;
        vwk->clip.rectangle.x2 = wk->screen.coordinates.max_x;
        vwk->clip.rectangle.y2 = wk->screen.coordinates.max_y;
    }
}
