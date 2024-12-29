/*
 * fVDI default drawing function code
 *
 * Copyright 2003, Johan Klockars
 * This software is licensed under the GNU General Public License.
 * Please, see LICENSE.TXT for further information.
 *
 * The Bresenham line drawing variation with perfect clipping is
 * based on code from the Graphics Gems series of books.
 */

#include "fvdi.h"
#include "function.h"
#include "relocate.h"
#include "utility.h"

void CDECL retry_line(Virtual *vwk, DrvLine *pars);
void CDECL vr_transfer_bits(Virtual *vwk, GCBITMAP *src_bm, GCBITMAP *dst_bm, RECT16 *src_rect, RECT16 *dst_rect, long mode);

void call_draw_line(Virtual *vwk, DrvLine *line);


void CDECL retry_line(Virtual *vwk, DrvLine *pars)
{
    short *table, length, *index, moves;
    short n;
    short init_x, init_y, x2, y2;
    short movepnt;
    DrvLine line;

    table = index = 0;
    length = moves = 0;
    if ((long) vwk & 1)
    {
        if ((unsigned long) (pars->y1 & 0xffff) > 1)
            return;                     /* Don't know about this kind of table operation */
        table = (short *) pars->x1;
        length = (pars->y1 >> 16) & 0xffff;
        if ((pars->y1 & 0xffff) == 1)
        {
            index = (short *) pars->y2;
            moves = pars->x2 & 0xffff;
        }
        vwk = (Virtual *) ((long) vwk - 1);
    } else
        return;

    /* Clear high words of coordinate unions? */
    line.pattern = pars->pattern;
    line.colour = pars->colour;
    line.mode = pars->mode;

    init_x = line.x1 = *table++;
    init_y = line.y1 = *table++;

    movepnt = -1;
    if (index)
    {
        moves--;
        if (index[moves] == -4)
            moves--;
        if (index[moves] == -2)
            moves--;
        if (moves >= 0)
            movepnt = (index[moves] + 4) / 2;
    }

    for (n = 1; n < length; n++)
    {
        x2 = *table++;
        y2 = *table++;
        if (n == movepnt)
        {
            if (--moves >= 0)
                movepnt = (index[moves] + 4) / 2;
            else
                movepnt = -1;           /* Never again equal to n */
            init_x = line.x1 = x2;
            init_y = line.y1 = y2;
            continue;
        }

        if (((n == length - 1) || (n == movepnt - 1)) && ((x2 != init_x) || (y2 != init_y)))
            line.draw_last = 1;         /* Draw last pixel in the line */
        else
            line.draw_last = 0;         /* Do not draw last pixel in the line */

#if 0
        c_draw_line(vwk, x1, y1, x2, y2, pattern, colour, mode, draw_last);
#else
        line.x2 = x2;
        line.y2 = y2;
        call_draw_line(vwk, &line);
#endif

        line.x1 = x2;
        line.y1 = y2;
    }
}


/* This is in a rather adhoc state at the moment, unfortunately.
 * But it does seem to work, for what it does, and is not complicated.
 */
void CDECL vr_transfer_bits(Virtual *vwk, GCBITMAP *src_bm, GCBITMAP *dst_bm, RECT16 *src_rect, RECT16 *dst_rect, long mode)
{
    int error = 0;

    do
    {
        if ((src_rect->x2 - src_rect->x1 != dst_rect->x2 - dst_rect->x1) ||
            (src_rect->y2 - src_rect->y1 != dst_rect->y2 - dst_rect->y1))
        {
            PUTS("No support yet for scaling\n");
            error = 1;
            break;
        }

        if (!src_bm)
        {
            PUTS("No support yet for screen->memory/screen\n");
            error = 1;
            break;
        }

        if (dst_bm)
        {
            if (src_bm->px_format != dst_bm->px_format)
            {
                if (src_bm->px_format == 0x01020101L && dst_bm->px_format == 0x01020808L)
                {
                    int x, y;

                    for (y = src_rect->y1; y <= src_rect->y2; y++)
                    {
                        long *src;
                        unsigned char *dst;
                        unsigned long v, mask;

                        src = (long *)(src_bm->addr + src_bm->width * y + (src_rect->x1 / 32) * 4);
                        mask = 1 << (31 - src_rect->x1 % 32);
                        dst = dst_bm->addr + dst_bm->width * (dst_rect->y1 - src_rect->y1 + y) + dst_rect->x1;
                        v = *src++;
                        if (mode == 33)
                        {
                            for (x = src_rect->x2 - src_rect->x1; x >= 0; x--)
                            {
                                if (v & mask)
                                    *dst = ~0;
                                dst++;
                                mask = (mask >> 1) | (mask << 31);
                                if ((long)mask < 0)
                                    v = *src++;
                            }
                        } else
                        {
                            for (x = src_rect->x2 - src_rect->x1; x >= 0; x--)
                            {
                                if (v & mask)
                                    *dst++ = ~0;
                                else
                                    *dst++ = 0;
                                mask = (mask >> 1) | (mask << 31);
                                if ((long)mask < 0)
                                    v = *src++;
                            }
                        }
                    }
                    mode = 0;  /* Just to skip error printout at the end */
                } else if (src_bm->px_format == 0x01020101L && dst_bm->px_format == 0x03421820L)
                {
                    int x, y;

                    for (y = src_rect->y1; y <= src_rect->y2; y++)
                    {
                        long *src;
                        long *dst;
                        unsigned long v, mask;

                        src = (long *)(src_bm->addr + src_bm->width * y + (src_rect->x1 / 32) * 4);
                        mask = 1 << (31 - src_rect->x1 % 32);
                        dst = (long *)(dst_bm->addr + dst_bm->width * (dst_rect->y1 - src_rect->y1 + y)) + dst_rect->x1;
                        v = *src++;
                        if (mode == 33)
                        {
                            for (x = src_rect->x2 - src_rect->x1; x >= 0; x--)
                            {
                                if (v & mask)
                                    *dst = ~0;
                                dst++;
                                mask = (mask >> 1) | (mask << 31);
                                if ((long)mask < 0)
                                    v = *src++;
                            }
                        } else
                        {
                            for (x = src_rect->x2 - src_rect->x1; x >= 0; x--)
                            {
                                if (v & mask)
                                    *dst++ = ~0;
                                else
                                    *dst++ = 0;
                                mask = (mask >> 1) | (mask << 31);
                                if ((long) mask < 0)
                                    v = *src++;
                            }
                        }
                    }
                    mode = 0;  /* Just to skip error printout at the end */
                } else
                {
                    PUTS("No support yet for memory->memory between these different pixmap formats\n");
                    error = 1;
                    break;
                }
            } else
            {
                if (src_bm->px_format == 0x01020101L)
                {
                    /* PX_PREF1 */
                    PUTS("No support yet for 1 bit memory->memory\n");
                    error = 1;
                    break;
                } else if (src_bm->px_format == 0x01020808L)
                {
                    /* PX_PREF8 */
                    int x, y;

                    for (y = src_rect->y1; y <= src_rect->y2; y++)
                    {
                        unsigned char *src, *dst;

                        src = src_bm->addr + src_bm->width * y + src_rect->x1;
                        dst = dst_bm->addr + dst_bm->width * (dst_rect->y1 - src_rect->y1 + y) + dst_rect->x1;
                        for (x = src_rect->x2 - src_rect->x1; x >= 0; x--)
                            *dst++ = *src++;
                    }
                } else if (src_bm->px_format == 0x03421820L)
                {
                    /* PX_PREF32 */
                    int x, y;

                    for (y = src_rect->y1; y <= src_rect->y2; y++)
                    {
                        long *src, *dst;

                        src = (long *)(src_bm->addr + src_bm->width * y) + src_rect->x1;
                        dst = (long *)(dst_bm->addr + dst_bm->width * (dst_rect->y1 - src_rect->y1 + y)) + dst_rect->x1;
                        for (x = src_rect->x2 - src_rect->x1; x >= 0; x--)
                            *dst++ = *src++;
                    }
                } else
                {
                    PRINTF(("Unsupported pixel format ($%lx) for memory->memory\n", src_bm->px_format));
                    error = 1;
                    break;
                }
            }
        } else
        {
            long *palette;

            if (src_bm->px_format == 0x01020101L)
            {
                /* PX_PREF1 */
                int x, y, i;
                char *block;
                MFDB mfdb;
                short coords[8];

#if 0
                if (!src_bm->ctab)
                {
                    PUTS("Need a colour table for 1 bit->TC\n");
                    error = 1;
                    break;
                }
                if (src_bm->ctab->color_space != 1)
                {
                    PUTS("Need an RGB colour table for 1 bit->TC\n");
                    error = 1;
                    break;
                }
#endif

                if ((block = (char *)allocate_block(0)) == 0)
                {
                    PUTS("Could not allocate memory block\n");
                    error = 1;
                    break;
                }
#if 0
                palette = (long *)block;
                for (i = 0; i < src_bm->ctab->no_colors; i++)
                {
                    palette[i] = ((long)(src_bm->ctab->colors[i].rgb.red & 0xff) << 16) |
                                 ((long)(src_bm->ctab->colors[i].rgb.green & 0xff) << 8) |
                                 ((long)(src_bm->ctab->colors[i].rgb.blue & 0xff));
                }
#endif

                mfdb.address = (short *)&block[src_bm->ctab->no_colors * sizeof(*palette)];
                mfdb.width = src_rect->x2 - src_rect->x1 + 1;
                mfdb.height = 1;
                mfdb.wdwidth = (mfdb.width + 15) / 16;
                mfdb.standard = 0;
                mfdb.bitplanes = 32;

                coords[0] = 0;
                coords[1] = 0;
                coords[2] = src_rect->x2 - src_rect->x1 + 1;
                coords[3] = 0;

                coords[4] = dst_rect->x1;
                coords[5] = dst_rect->y1;
                coords[6] = dst_rect->x2;
                coords[7] = dst_rect->y1;

                for (y = src_rect->y1; y <= src_rect->y2; y++)
                {
                    long *src, *dst;
                    unsigned long v, mask;

                    src = (long *)(src_bm->addr + src_bm->width * y) + (src_rect->x1 / 32);
                    mask = 1 << (31 - src_rect->x1 % 32);
                    dst = (long *)&block[src_bm->ctab->no_colors * sizeof(*palette)];
                    v = *src++;
                    if (mode == 33) /* T_TRANSPARENT */
                    {
                        short coords2[8];

                        for (i = 0; i < 4; i++)
                        {
                            coords2[i] = coords[i + 4];
                            coords2[i + 4] = coords[i];
                        }
                        lib_vdi_spppp(lib_vro_cpyfm, vwk, 3, coords2, 0, &mfdb, 0);
                        for (x = src_rect->x2 - src_rect->x1; x >= 0; x--)
                        {
                            if (v & mask)
                                *dst = ~0;
                            dst++;
                            mask = (mask >> 1) | (mask << 31);
                            if ((long)mask < 0)
                                v = *src++;
                        }
                    } else
                    {
                        for (x = src_rect->x2 - src_rect->x1; x >= 0; x--)
                        {
                            if (v & mask)
                                *dst++ = ~0;
                            else
                                *dst++ = 0;
                            mask = (mask >> 1) | (mask << 31);
                            if ((long)mask < 0)
                                v = *src++;
                        }
                    }
                    lib_vdi_spppp(lib_vro_cpyfm, vwk, 3, coords, &mfdb, 0, 0);
                    coords[5]++;
                    coords[7]++;
                }

                free_block(block);
                mode = 0;  /* Just to skip error printout at the end */
            } else if (src_bm->px_format == 0x01020808L)
            {
                /* PX_PREF8 */
                char *block;
                unsigned char *src;
                long *dst;
                MFDB mfdb;
                short coords[8];
                int x, y, i;

                if (!src_bm->ctab)
                {
                    PUTS("Need a colour table for 8 bit->TC\n");
                    error = 1;
                    break;
                }
                if (src_bm->ctab->color_space != 1)
                {
                    PUTS("Need an RGB colour table for 8 bit->TC\n");
                    error = 1;
                    break;
                }

                if ((block = (char *)allocate_block(0)) == NULL)
                {
                    PUTS("Could not allocate memory block\n");
                    error = 1;
                    break;
                }

                palette = (long *)block;
                for (i = 0; i < src_bm->ctab->no_colors; i++)
                {
                    palette[i] = ((long)(src_bm->ctab->colors[i].rgb.red & 0xff) << 16) |
                                 ((long)(src_bm->ctab->colors[i].rgb.green & 0xff) << 8) |
                                 ((long)(src_bm->ctab->colors[i].rgb.blue & 0xff));
                }

                mfdb.address = (short *)&block[src_bm->ctab->no_colors * sizeof(*palette)];
                mfdb.width = src_rect->x2 - src_rect->x1 + 1;
                mfdb.height = 1;
                mfdb.wdwidth = (mfdb.width + 15) / 16;
                mfdb.standard = 0;
                mfdb.bitplanes = 32;

                coords[0] = 0;
                coords[1] = 0;
                coords[2] = src_rect->x2 - src_rect->x1 + 1;
                coords[3] = 0;

                coords[4] = dst_rect->x1;
                coords[5] = dst_rect->y1;
                coords[6] = dst_rect->x2;
                coords[7] = dst_rect->y1;

                for (y = src_rect->y1; y <= src_rect->y2; y++)
                {
                    src = src_bm->addr + src_bm->width * y + src_rect->x1;
                    dst = (long *)&block[src_bm->ctab->no_colors * sizeof(*palette)];
                    for (x = src_rect->x2 - src_rect->x1; x >= 0; x--)
                        *dst++ = palette[*src++];
                    lib_vdi_spppp(lib_vro_cpyfm, vwk, 3, coords, &mfdb, 0, 0);
                    coords[5]++;
                    coords[7]++;
                }

                free_block(block);
            } else if (src_bm->px_format == 0x03421820L)
            {
                /* PX_PREF32 */
                MFDB mfdb;
                short coords[8];

                mfdb.address = (short *)src_bm->addr;
                mfdb.width = src_bm->xmax - src_bm->xmin;
                mfdb.height = src_bm->ymax - src_bm->ymin;
                mfdb.wdwidth = (mfdb.width + 15) / 16;
                mfdb.standard = 0;
                mfdb.bitplanes = 32;

                coords[0] = src_rect->x1;
                coords[1] = src_rect->y1;
                coords[2] = src_rect->x2;
                coords[3] = src_rect->y2;

                coords[4] = dst_rect->x1;
                coords[5] = dst_rect->y1;
                coords[6] = dst_rect->x2;
                coords[7] = dst_rect->y2;

                lib_vdi_spppp(lib_vro_cpyfm, vwk, 3, coords, &mfdb, 0, 0);
            } else
            {
                PRINTF(("Unsupported source pixel format ($%lx) for !TC->TC\n", src_bm->px_format));
                error = 1;
                break;
            }
        }
    } while (0);

    if (error)
    {
#ifdef FVDI_DEBUG
        int i;

        PRINTF(("vr_transfer_bits mode %ld\n", mode));

        for (i = 0; i < 2; i++)
        {
            GCBITMAP *bm = (i == 0) ? src_bm : dst_bm;

            if (bm)
            {
                PRINTF(("%s $%08lx  Width: %ld  Bits: %ld  Format: $%lx",
                    i == 0 ? "SRC" : "DST",
                    (long) bm->addr,
                    bm->width,
                    bm->bits,
                    bm->px_format));
                if (bm->xmin || bm->ymin)
                {
                    PRINTF(("  Base: %ld,%ld", bm->xmin, bm->ymin));
                }
                PRINTF(("  Dim: %ld,%ld\n", bm->xmax - bm->xmin, bm->ymax - bm->ymin));

                PRINTF(("    C/ITAB: $%08lx%08lx/$", (long) bm->ctab, (long) bm->itab));

                if (bm->ctab)
                {
                    PRINTF(("  ID: $%lx  Space: %ld  Flags: $%lx  Colours: %ld", bm->ctab->map_id, bm->ctab->color_space, bm->ctab->flags, bm->ctab->no_colors));
                }

                PUTS("\n");
            }
        }

        PUTS("--------\n");
    } else if (mode != 0 && mode != 32)
    {
        PRINTF(("\nvr_transform_bits mode %ld\n\n", mode));
#endif
    }
}
