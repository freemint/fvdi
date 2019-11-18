/*
 * fVDI circle/ellipse/pie/arc code
 *
 * $Id: conic.c,v 1.8 2005-07-10 00:06:24 johan Exp $
 *
 * Copyright 1999/2001-2003, Johan Klockars
 * This software is licensed under the GNU General Public License.
 * Please, see LICENSE.TXT for further information.
 *
 * This is extracted and modified from code with an
 * original copyright as follows.
 */

/*************************************************************************
**       Copyright 1999, Caldera Thin Clients, Inc.                     ** 
**       This software is licenced under the GNU Public License.        **
**       Please see LICENSE.TXT for further information.                ** 
**                                                                      ** 
**                  Historical Copyright                                ** 
**                                                                      **
**  Copyright (c) 1987, Digital Research, Inc. All Rights Reserved.     **
**  The Software Code contained in this listing is proprietary to       **
**  Digital Research, Inc., Monterey, California and is covered by U.S. **
**  and other copyright protection.  Unauthorized copying, adaptation,  **
**  distribution, use or display is prohibited and may be subject to    **
**  civil and criminal penalties.  Disclosure to others is prohibited.  **
**  For the terms and conditions of software code use refer to the      **
**  appropriate Digital Research License Agreement.                     **
**                                                                      **
**************************************************************************/


#include "fvdi.h"
#include "utility.h"
#include "function.h"
#include "globals.h"

#define MAX_ARC_CT 256

#ifdef __GNUC__
#define SMUL_DIV(x,y,z)	((short)(((short)(x)*(long)((short)(y)))/(short)(z)))
#else
#ifdef __PUREC__
#define SMUL_DIV(x,y,z)	((short)(((x)*(long)(y))/(z)))
#else
int SMUL_DIV(int, int, int);   //   d0d1d0d2
#pragma inline d0 = SMUL_DIV(d0, d1, d2) { "c1c181c2"; }
#endif
#endif


void clc_arc(Virtual *vwk, long gdp_code, long xc, long yc, long xrad, long yrad,
             long beg_ang, long end_ang, long del_ang, long n_steps,
             Fgbg fill_colour, Fgbg border_colour,
             short *pattern, short *points, long mode, long interior_style)
{
    short i, j, start, angle;

    if (vwk->clip.on) {
        if (((xc + xrad) < vwk->clip.rectangle.x1)
                || ((xc - xrad) > vwk->clip.rectangle.x2)
                || ((yc + yrad ) < vwk->clip.rectangle.y1)
                || ((yc - yrad) > vwk->clip.rectangle.y2))
            return;
    }
    start = angle = beg_ang;
    *points++ = SMUL_DIV(Icos(angle), xrad, 32767) + xc;
    *points++ = yc - SMUL_DIV(Isin(angle), yrad, 32767);

    for(i = 1, j = 2; i < n_steps; i++, j += 2) {
        angle = SMUL_DIV(del_ang, i, n_steps) + start;
        *points++ = SMUL_DIV(Icos(angle), xrad, 32767) + xc;
        *points++ = yc - SMUL_DIV(Isin(angle), yrad, 32767);
    }
    angle = end_ang;
    *points++ = SMUL_DIV(Icos(angle), xrad, 32767) + xc;
    *points++ = yc - SMUL_DIV(Isin(angle), yrad, 32767);

    /* If pie wedge, draw to center and then close.
     * If arc or circle, do nothing because loop should close circle.
     */

    if ((gdp_code == 3) || (gdp_code == 7)) {	/* Pie wedge */
        n_steps++;
        *points++ = xc;
        *points++ = yc;
    }

    if ((gdp_code == 2) || (gdp_code == 6))	/* Open arc */
        c_pline(vwk, n_steps + 1,
                *(long *)&border_colour, points - (n_steps + 1) * 2);
    else {
#if 0
        filled_poly(vwk, points - (n_steps + 1) * 2, n_steps + 1,
                    *(long *)&fill_colour, pattern, points, mode, interior_style);
#else
        fill_poly(vwk, points - (n_steps + 1) * 2, n_steps + 1,
                  *(long *)&fill_colour, pattern, points, mode, interior_style);
#endif
        if (vwk->fill.perimeter)
#if 0
            c_pline(vwk, n_steps + 1,
                    *(long *)&border_colour, points - (n_steps + 1) * 2);
#else
            c_pline(vwk, n_steps + 1,
                    *(long *)&border_colour, points - (n_steps + 1) * 2);
#endif
    }
}


void col_pat(Virtual *vwk, Fgbg *fill_colour, Fgbg *border_colour, short **pattern)
{
    short interior;
    extern short *pattern_ptrs[];

    interior = vwk->fill.interior;

    *border_colour = vwk->fill.colour;
    if (interior)
        *fill_colour = vwk->fill.colour;
    else {
        fill_colour->background = vwk->fill.colour.foreground;
        fill_colour->foreground = vwk->fill.colour.background;
    }

    if (interior == 4)
        *pattern = vwk->fill.user.pattern.in_use;
    else {
        *pattern = pattern_ptrs[interior];
        if (interior & 2)     /* interior 2 or 3 */
            *pattern += (vwk->fill.style - 1) * 16;
    }
}


long clc_nsteps(long xrad, long yrad)
{
    long n_steps;

    if (xrad > yrad)
        n_steps = xrad;
    else
        n_steps = yrad;

#if 0
    n_steps = n_steps >> 2;

    if (n_steps < 16)
        n_steps = 16;
    else if (n_steps > MAX_ARC_CT)
        n_steps = MAX_ARC_CT;
#else
    n_steps = (n_steps * arc_split) >> 16;

    if (n_steps < arc_min)
        n_steps = arc_min;
    else if (n_steps > arc_max)
        n_steps = arc_max;
#endif

    return n_steps;
}


#if 0
void circle(Virtual *vwk, int xc, int yc, int xrad, short *points)
{
    int yrad, n_steps;

    yrad = SMUL_DIV(xrad, xsize, ysize);
    n_steps = clc_nsteps(xrad, yrad);
    clc_arc(vwk, 4, xc, yc, xrad, yrad, 0, 3600, 3600, n_steps, points);
}


void ellipse(Virtual *vwk, int xc, int yc, int xrad, int yrad, short *points)
{
    int n_steps;
#if 0
    if (xfm_mode < 2) /* if xform != raster then flip */
        yrad = yres - yrad;
#endif
    n_steps = clc_nsteps(xrad, yrad);
    clc_arc(vwk, 5, xc, yc, xrad, yrad, 0, 0, 3600, n_steps, points);
}


void arc(Virtual *vwk, int xc, int yc, int xrad, int beg_ang, int end_ang, short *points)
{
    int del_ang, yrad, n_steps;

    del_ang = end_ang - beg_ang;
    if (del_ang < 0)
        del_ang += 3600;

    yrad = SMUL_DIV(xrad, xsize, ysize);
    n_steps = clc_nsteps(xrad, yrad);
    n_steps = SMUL_DIV(del_ang, n_steps, 3600);
    if (n_steps == 0)
        return;

    clc_arc(vwk, 2, xc, yc, xrad, yrad, beg_ang, end_ang, del_ang, n_steps, points);
}
#endif


void ellipsearc(Virtual *vwk, long gdp_code,
                long xc, long yc, long xrad, long yrad, long beg_ang, long end_ang)
{
    int del_ang, n_steps;
    short *points, *pattern;
    Fgbg fill_colour, border_colour;
    long interior_style;

    del_ang = end_ang - beg_ang;
    if (del_ang <= 0)
        del_ang += 3600;

#if 0
    if (xfm_mode < 2)	/* If xform != raster then flip */
        yrad = yres - yrad;
#endif

    n_steps = clc_nsteps(xrad, yrad);
    n_steps = SMUL_DIV(del_ang, n_steps, 3600);
    if (n_steps == 0)
        return;

    if (!(points = (short *)allocate_block(0)))
        return;

    pattern = 0;
    interior_style = 0;
    border_colour = vwk->line.colour;
    if (gdp_code == 7 || gdp_code == 5) {
        col_pat(vwk, &fill_colour, &border_colour, &pattern);
        interior_style = ((long)vwk->fill.interior << 16) |
                (vwk->fill.style & 0xffffL);
    }

    /* Dummy fill colour, pattern and interior style since not filled */
    clc_arc(vwk, gdp_code, xc, yc, xrad, yrad, beg_ang, end_ang, del_ang,
            n_steps, fill_colour, border_colour, pattern, points, vwk->mode,
            interior_style);

    free_block(points);
}


#if 0
void arb_corner(WORD * corners, WORD type)
{
    /* Local declarations. */
    REG WORD temp, typ;
    REG WORD *xy1, *xy2;

    /* Fix the x coordinate values, if necessary. */

    xy1 = corners;
    xy2 = corners + 2;
    if (*xy1 > *xy2) {
        temp = *xy1;
        *xy1 = *xy2;
        *xy2 = temp;
    }



    /* End if:  "x" values need to be swapped. */
    /* Fix y values based on whether traditional (ll, ur) or raster-op */
    /* (ul, lr) format is desired.                                     */
    xy1++;                      /* they now point to corners[1] and
                                   corners[3] */
    xy2++;

    typ = type;

    if (((typ == LLUR) && (*xy1 < *xy2)) ||
            ((typ == ULLR) && (*xy1 > *xy2))) {
        temp = *xy1;
        *xy1 = *xy2;
        *xy2 = temp;
    }                           /* End if:  "y" values need to be swapped. */
}                               /* End "arb_corner". */
#endif


#if 0
case 7: /* GDP Rounded Box */
ltmp_end = line_beg;
line_beg = SQUARED;
rtmp_end = line_end;
line_end = SQUARED;
#endif
void rounded_box(Virtual *vwk, long gdp_code, short *coords)
/* long x1, long y1, long x2, long y2) */
{
    short i, j;
    short rdeltax, rdeltay;
    short xc, yc, xrad, yrad;
    short x1, y1, x2, y2;
    Workstation *wk = vwk->real_address;
    long n_steps;
    short *points, *pattern;
    Fgbg fill_colour, border_colour;
    long interior_style;

    if (!(points = (short *)allocate_block(0)))
        return;

    pattern = 0;
    interior_style = 0;
    border_colour = vwk->line.colour;
    if (gdp_code == 9) {
        col_pat(vwk, &fill_colour, &border_colour, &pattern);
        interior_style = ((long)vwk->fill.interior << 16) |
                (vwk->fill.style & 0xffffL);
    }

    x1 = coords[0];
    y1 = coords[1];
    if (x1 <= coords[2])
        x2 = coords[2];
    else {
        x2 = x1;
        x1 = coords[2];
    }
    if (y1 <= coords[3])
        y2 = coords[3];
    else {
        y2 = y1;
        y1 = coords[3];
    }

#if 0
    arb_corner(PTSIN, LLUR);
#endif
    rdeltax = (x2 - x1) / 2;
    rdeltay = (y2 - y1) / 2;

    xrad = wk->screen.mfdb.width >> 6;
    if (xrad > rdeltax)
        xrad = rdeltax;

    yrad = SMUL_DIV(xrad, wk->screen.pixel.width, wk->screen.pixel.height);
    if (yrad > rdeltay) {
        yrad = rdeltay;
        xrad = SMUL_DIV(yrad, wk->screen.pixel.height, wk->screen.pixel.width);
    }
    yrad = -yrad;

    n_steps = clc_nsteps(xrad, yrad);

#if 0
    PTSIN[0] = 0;
    PTSIN[1] = yrad;
    PTSIN[2] = SMUL_DIV(Icos(675), xrad, 32767);
    PTSIN[3] = SMUL_DIV(Isin(675), yrad, 32767);
    PTSIN[4] = SMUL_DIV(Icos(450), xrad, 32767);
    PTSIN[5] = SMUL_DIV(Isin(450), yrad, 32767);
    PTSIN[6] = SMUL_DIV(Icos(225), xrad, 32767);
    PTSIN[7] = SMUL_DIV(Isin(225), yrad, 32767);
    PTSIN[8] = xrad;
    PTSIN[9] = 0;
#else
    for(i = 0; i < 5; i++) {
        points[i * 2]     = SMUL_DIV(Icos(900 - 225 * i), xrad, 32767);
        points[i * 2 + 1] = SMUL_DIV(Isin(900 - 225 * i), yrad, 32767);
    }
#endif

    xc = x2 - xrad;
    yc = y1 - yrad;
    j = 10;
    for(i = 9; i >= 0; i--) {
        points[j + 1] = yc + points[i--];
        points[j] = xc + points[i];
        j += 2;
    }
    xc = x1 + xrad;
    j = 20;
    for(i = 0; i < 10; i++) {
        points[j++] = xc - points[i++];
        points[j++] = yc + points[i];
    }
    yc = y2 + yrad;
    j = 30;
    for(i = 9; i >= 0; i--) {
        points[j + 1] = yc - points[i--];
        points[j] = xc - points[i];
        j += 2;
    }
    xc = x2 - xrad;
    j = 0;
    for(i = 0; i < 10; i++) {
        points[j++] = xc + points[i++];
        points[j++] = yc - points[i];
    }
    points[40] = points[0];
    points[41] = points[1];

    if (gdp_code == 8) {
#if 0
        c_pline(vwk, n_steps + 1, *(long *)&border_colour, points - (n_steps + 1) * 2);
#else
        c_pline(vwk, 21, *(long *)&border_colour, points);
#endif
    } else {
#if 0
        filled_poly(vwk, points, 21, *(long *)&fill_colour, pattern, &points[42], vwk->mode, interior_style);
#else
        fill_poly(vwk, points, 21, *(long *)&fill_colour, pattern, &points[42], vwk->mode, interior_style);
#endif
        if (vwk->fill.perimeter)
#if 0
            c_pline(vwk, 21, *(long *)&border_colour, points - (n_steps + 1) * 2);
#else
#if 0
            c_pline(vwk, 21, *(long *)&border_colour, points);
#else
            c_pline(vwk, 21, *(long *)&border_colour, points);
#endif
#endif
    }

    free_block(points);
}


#if 0
/* This is the fill pattern setup */
case 2:
if (fill_index < 8) {
    patmsk = DITHRMSK;
    patptr = &DITHER[fill_index * (patmsk + 1)];
} else {
patmsk = OEMMSKPAT;
patptr = &OEMPAT[(fill_index - 8) * (patmsk + 1)];
}
break;
case 3:
if (fill_index < 6) {
    patmsk = HAT_0_MSK;
    patptr = &HATCH0[fill_index * (patmsk + 1)];
} else {
patmsk = HAT_1_MSK;
patptr = &HATCH1[(fill_index - 6) * (patmsk + 1)];
}
break;
case 4:
patmsk = 0x000f;
patptr = &UD_PATRN;
break;
#endif
