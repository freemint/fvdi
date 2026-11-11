/*
 * uae_mous.c - Mouse cursor functions
 * This is part of the WinUAE RTG driver for fVDI
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
#include "radeon.h"

/* We must remember if the mouse is visible or not */
static int mouse_visible = 0;

/* Current mouse shape */
static Mouse *pmouse;

/* MFDB used to draw the mouse */
static MFDB mouse_mfdb = { NULL, 16, 16, 1, 1, 1, { 0, 0, 0 } };

/* We must save the mouse background */
static short backup_data[16*16];
static MFDB mouse_backup_mfdb = { backup_data, 16, 16, 1, 0, 16, { 0, 0, 0 } };
static short backup_x, backup_y, backup_w, backup_h;

static void clip_mouse(Virtual *vwk, short x, short y, short *pw, short *ph)
{
    short w = 16;
    short h = 16;
    short screen_w = vwk->real_address->screen.mfdb.width;
    short screen_h = vwk->real_address->screen.mfdb.height;

    *pw = x + w > screen_w ? screen_w - x : w;
    *ph = y + h > screen_h ? screen_h - y : h;
}

static long draw_mouse(Virtual *vwk, short x, short y)
{
    short w, h;

    x -= pmouse->hotspot.x;
    y -= pmouse->hotspot.y;
    clip_mouse(vwk, x, y, &w, &h);

    /* Draw the mask, transparent mode */
    mouse_mfdb.address = pmouse->mask;
    c_expand_area(vwk, &mouse_mfdb, 0, 0, NULL, x, y, w, h, 2, pmouse->colour.background);

    /* Draw the data, transparent mode */
    mouse_mfdb.address = pmouse->data;
    c_expand_area(vwk, &mouse_mfdb, 0, 0, NULL, x, y, w, h, 2, pmouse->colour.foreground);

    return 1;
}

static void hide_mouse(Virtual *vwk)
{
    if (mouse_visible)
    {
        /* Restore the backup */
        c_blit_area(vwk, &mouse_backup_mfdb, 0, 0, NULL, backup_x, backup_y, backup_w, backup_h, 3);

        mouse_visible = 0;
    }
}

static void show_mouse(Virtual *vwk, short x, short y)
{
    if (mouse_visible)
        hide_mouse(vwk);

    /* Make a new backup */
    backup_x = x - pmouse->hotspot.x;
    backup_y = y - pmouse->hotspot.y;
    clip_mouse(vwk, backup_x, backup_y, &backup_w, &backup_h);
    c_blit_area(vwk, NULL, backup_x, backup_y, &mouse_backup_mfdb, 0, 0, backup_w, backup_h, 3);

    draw_mouse(vwk, x, y);

    mouse_visible = 1;
}

long CDECL
c_mouse_draw(Workstation *wk, long x, long y, Mouse *mouse)
{
    /* See mouse_timer and wk_r_mouse in engine/mouse.s for parameters meaning */
    Virtual *vwk = me->default_vwk;

    (void) wk;
    /* KDEBUG(("c_mouse_draw %ld,%ld %p (old=%lu)\n", x & 0xffff, y, mouse, (ULONG)x >> 16)); */

    if ((long)mouse > 7) /* Set new mouse cursor shape */
    {
        int mouse_was_visible = mouse_visible;

        if (mouse_was_visible)
            hide_mouse(vwk);

        pmouse = mouse;

        if (mouse_was_visible)
            show_mouse(vwk, (short)x, (short)y);

        return 1;
    }
    else
    {
        switch ((long)mouse)
        {
            case 0: /* Move visible */
            case 4: /* Move visible forced (wk_mouse_forced) */
                show_mouse(vwk, (short)x, (short)y);
                return 1;

            case 1: /* Move hidden */
            case 5: /* Move hidden forced (wk_mouse_forced) */
                return 1;

            case 2: /* Hide */
                hide_mouse(vwk);
                return 1;

            case 3: /* Show */
                show_mouse(vwk, (short)x, (short)y);
                return 1;
        }
    }

    return 0;
}
