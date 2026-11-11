/*
 * sag_mous.c - Mouse cursor functions
 * This is part of SAGA driver for fVDI
 *
 * Copyright (C) 2017 Vincent Riviere
 * Copyright (C) 2019 Thorsten Otto
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
#include "saga.h"

/* We must remember if the mouse is visible or not */
static int mouse_visible = 0;

/* Current mouse shape */
static Mouse *pmouse;

/* MFDB used to draw the mouse */
static MFDB mouse_mfdb = {NULL, 16, 16, 1, 1, 1, { 0, 0, 0 }};

/* We must save the mouse background */
static short backup_data[16*16];
static MFDB mouse_backup_mfdb = {backup_data, 16, 16, 1, 0, 16, { 0, 0, 0 }};

/* Visible part of the mouse: screen position, size and offset inside the shape */
static short backup_x, backup_y, backup_w, backup_h;
static short shape_x, shape_y;

/*
 * Clip the 16x16 mouse rectangle with its top-left corner at (*px, *py)
 * to the screen. On return, (*px, *py) and (*pw, *ph) give the visible
 * part and (*psx, *psy) its offset inside the mouse shape.
 * Returns 0 if no part of the mouse is visible.
 */
static int clip_mouse(Virtual *vwk, short *px, short *py, short *psx, short *psy, short *pw, short *ph)
{
	short x = *px;
	short y = *py;
	short sx = 0;
	short sy = 0;
	short w = 16;
	short h = 16;
	short screen_w = vwk->real_address->screen.mfdb.width;
	short screen_h = vwk->real_address->screen.mfdb.height;

	if (x < 0)
	{
		sx = -x;
		w += x;
		x = 0;
	}
	if (y < 0)
	{
		sy = -y;
		h += y;
		y = 0;
	}
	if (x + w > screen_w)
		w = screen_w - x;
	if (y + h > screen_h)
		h = screen_h - y;

	*px = x;
	*py = y;
	*psx = sx;
	*psy = sy;
	*pw = w;
	*ph = h;

	return w > 0 && h > 0;
}

static void draw_mouse(Virtual *vwk)
{
	/* Draw the mask, transparent mode */
	mouse_mfdb.address = pmouse->mask;
	c_expand_area(vwk, &mouse_mfdb, shape_x, shape_y, NULL, backup_x, backup_y, backup_w, backup_h, 2, pmouse->colour.background);

	/* Draw the data, transparent mode */
	mouse_mfdb.address = pmouse->data;
	c_expand_area(vwk, &mouse_mfdb, shape_x, shape_y, NULL, backup_x, backup_y, backup_w, backup_h, 2, pmouse->colour.foreground);
}

static void hide_mouse(Virtual *vwk)
{
	if (mouse_visible)
	{
		if (hwmouse)
		{
			/* put position outside window */
			saga_set_mouse_position(-100, -100);
		} else if (backup_w > 0 && backup_h > 0)
		{
			/* Restore the backup */
			c_blit_area(vwk, &mouse_backup_mfdb, 0, 0, NULL, backup_x, backup_y, backup_w, backup_h, 3);
		}
		mouse_visible = 0;
	}
}

static void show_mouse(Virtual *vwk, short x, short y)
{
	/* If a mouse shape has not yet been set, just ignore */
	if (!pmouse)
		return;

	if (mouse_visible)
		hide_mouse(vwk);

	x -= pmouse->hotspot.x;
	y -= pmouse->hotspot.y;

	if (hwmouse)
	{
		saga_set_mouse_position(x, y);
	} else
	{
		backup_x = x;
		backup_y = y;
		if (clip_mouse(vwk, &backup_x, &backup_y, &shape_x, &shape_y, &backup_w, &backup_h))
		{
			/* Make a new backup */
			c_blit_area(vwk, NULL, backup_x, backup_y, &mouse_backup_mfdb, 0, 0, backup_w, backup_h, 3);

			draw_mouse(vwk);
		}
	}

	mouse_visible = 1;
}


long CDECL c_mouse_draw(Workstation *wk, long x, long y, Mouse *mouse)
{
	/* See mouse_timer and wk_r_mouse in engine/mouse.s for parameters meaning */
	Virtual *vwk = me->default_vwk;

	(void) wk;

	if ((long)mouse > 7) /* Set new mouse cursor shape */
	{
		int mouse_was_visible = mouse_visible;

		if (mouse_was_visible)
			hide_mouse(vwk);

		pmouse = mouse;
		if (hwmouse)
			saga_set_mouse_sprite(wk, mouse);
		
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
