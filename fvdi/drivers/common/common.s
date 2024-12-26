*****
* fVDI->driver interface (assembly functions), by Johan Klockars
*
* Most fVDI device drivers are expected to make use of this file.
*
* Since it would be difficult to do without this file when
* writing new device drivers, and to make it possible for
* some such drivers to be commercial, this file is put in
* the public domain. It's not copyrighted or under any sort
* of license.
*****

both		equ	1	; Write in both FastRAM and on screen

	.include		"vdi.inc"
	.include		"macros.inc"

	xdef		_line
	xdef		_set_pixel
	xdef		_get_pixel
	xdef		_expand
	xdef		_fill
	xdef		_fillpoly
	xdef		_blit
	xdef		_text
	xdef		_mouse
	xdef		_set_palette
	xdef		_colour
	xdef		_initialize_palette

	xref		_line_draw_r,_write_pixel_r,_read_pixel_r,_expand_area_r
	xref		_fill_area_r,_fill_poly_r,_blit_area_r,_text_area_r,_mouse_draw_r
	xref		_set_colours_r,_get_colour_r
	xref		_fallback_line,_fallback_text,_fallback_fill
	xref		_fallback_fillpoly,_fallback_expand,_fallback_blit
	xref		clip_line
	xref		_mask


	text

*---------
* Set a coloured pixel
* In:	a0	VDI struct, destination MFDB (odd address marks table operation)
*	d0	colour
*	d1	x or table address
*	d2	y or table length (high) and type (0 - coordinates)
* Call:	a1	VDI struct, destination MFDB (odd address marks table operation)
*	d0	colour
*	d1	x or table address
*	d2	y or table length (high) and type (0 - coordinates)
*
* called by:
*     engine/mouse.s: mouse_unshow
*     engine/mouse.s: mouse_show
*     engine/blit.s: setup_blit (various blit routines)
*---------
_set_pixel:
	movem.l		d0-d7/a0-a6,-(a7)	; Used to have -3/4/6 for normal/both

	move.l		a0,a1

	ijsr		_write_pixel_r
	tst.l		d0
	bgt		.write_done

	tst.w		d2
	bne		.write_done		; Only straight coordinate tables available so far
	move.l		8*4(a7),d3		; Fetch a0
	bclr		#0,d3
	move.l		d3,-(a7)
	move.l		4+4(a7),-(a7)		; Fetch d1
	move.w		8+8(a7),-(a7)		; Fetch d2 (high)
.write_loop:
	move.l		2(a7),a2
	move.w		(a2)+,d1
	move.w		(a2)+,d2
	move.l		a2,2(a7)
	move.l		6(a7),a1
	move.l		10+0(a7),d0		; Fetch d0
	ijsr		_write_pixel_r
	subq.w		#1,(a7)
	bne		.write_loop
	add.w		#10,a7
.write_done:

	movem.l		(a7)+,d0-d7/a0-a6
	rts


*---------
* Get a coloured pixel
* In:	a0	VDI struct, source MFDB
*	d1	x
*	d2	y
* Call:	a1	VDI struct, source MFDB
*	d1	x
*	d2	y
* Out:	d0	line colour
*
* called by:
*     engine/mouse.s: mouse_show
*     engine/blit.s: setup_blit (various blit routines)
*---------
_get_pixel:
	movem.l		d1-d7/a0-a6,-(a7)	; Used to have -3/4/6 for normal/both

	move.l		a0,a1
	
	ijsr		_read_pixel_r

	movem.l		(a7)+,d1-d7/a0-a6
	rts


*---------
* Draw a colored line between 2 points
* In:	a0	VDI struct (odd address marks table operation)
*	d0	colour
*	d1	x1 or table address
*	d2	y1 or table length (high) and type (0 - coordinate pairs, 1 - pairs+moves)
*	d3	x2 or move point count
*	d4	y2 or move index address
*	d5	pattern
*	d6	mode
* Call:	a1	VDI struct (odd adress marks table operation)
*	d0	logic operation
*	d1	x1 or table address
*	d2	y1 or table length (high) and type (0 - coordinate pairs, 1 - pairs+moves)
*	d3	x2 or move point count
*	d4	y2 or move index address
*	d5	pattern
*	d6	colour
*
* called by:
*     engine/draw.s: call_draw_line
*     engine/draw.s: v_bez_accel
*     engine/draw.s: _lib_v_pline
*     engine/draw.s: v_pmarker
*     engine/draw.s: lib_v_bez_fill
*---------
_line:
	movem.l		d0-d7/a0-a6,-(a7)	; Used to have -3/4/6 for normal/both

	move.l		a0,a1
	exg		d0,d6

	ijsr		_line_draw_r
	tst.l		d0
	lbgt		.l1,1
	lbmi		.l2,2
	move.l		_fallback_line,d0
	bra		give_up

 label .l1,1
	movem.l		(a7)+,d0-d7/a0-a6
	rts

 label .l2,2					; Transform multiline to single ones
	move.w		8+2(a7),d0
	move.w		d3,d7
	cmp.w		#1,d0
	bhi		.line_done		; Only coordinate pairs and pairs+marks available so far
	beq		.use_marks
	moveq		#0,d7			; Move count
.use_marks:
	swap		d7
	move.w		#1,d7			; Currrent index in high word
	swap		d7

	move.l		8*4(a7),d3		; Fetch a0
	bclr		#0,d3
	move.l		d3,a1
	move.l		1*4(a7),a2		; Table address

	move.l		d4,a6
	tst.w		d7
	beq		.no_start_move
	add.w		d7,a6
	add.w		d7,a6
	subq.l		#2,a6
	cmp.w		#-4,(a6)
	bne		.no_start_movex
	subq.l		#2,a6
	subq.w		#1,d7
.no_start_movex:
	cmp.w		#-2,(a6)
	bne		.no_start_move
	subq.l		#2,a6
	subq.w		#1,d7
.no_start_move:
	bra		.loop_end
.line_loop:
	movem.w		(a2),d1-d4
	move.l		a1,a0
	bsr		clip_line
	bvs		.no_draw
	move.l		0(a7),d6		; Colour
	move.l		5*4(a7),d5		; Pattern
	move.l		6*4(a7),d0		; Mode
	movem.l		d7/a1-a2/a6,-(a7)
	ijsr		_line_draw_r
	movem.l		(a7)+,d7/a1-a2/a6
.no_draw:
	tst.w		d7
	beq		.no_marks
	swap		d7
	addq.w		#1,d7
	move.w		d7,d4
	add.w		d4,d4
	subq.w		#4,d4
	cmp.w		(a6),d4
	bne		.no_move
	subq.l		#2,a6
	addq.w		#1,d7
	swap		d7
	subq.w		#1,d7
	swap		d7
	addq.l		#4,a2
	subq.w		#1,2*4(a7)
.no_move:
	swap		d7
.no_marks:
	addq.l		#4,a2
.loop_end:
	subq.w		#1,2*4(a7)
	bgt		.line_loop
.line_done:
	movem.l		(a7)+,d0-d7/a0-a6
	rts


*---------
* Expand a monochrome area to multiple bitplanes
* In:	a0	VDI struct, destination MFDB, VDI struct, source MFDB
*	d0	colour
*	d1-d2	x1,y1 source
*	d3-d6	x1,y1 x2,y2 destination
*	d7	logic operation
* Call:	d0	height and width to move (high and low word)
*	d1-d2	source coordinates
*	d3-d4	destination coordinates
*	d6	background and foreground colour
*	d7	logic operation
*
* called by:
*     engine/blit.s: lib_vrt_cpyfm
*---------
_expand:
	movem.l		d0-d7/a0-a6,-(a7)	; Used to have -3(/6)/4(/6)/6 for normal/both

	move.l		a0,a1

	exg		d0,d6
	sub.w		d4,d0
	addq.w		#1,d0
	swap		d0
	move.w		d5,d0
	sub.w		d3,d0
	addq.w		#1,d0

	ijsr		_expand_area_r
	tst.l		d0
	lbgt		.l1,1
	move.l		_fallback_expand,d0
	bra		give_up

 label .l1,1
	movem.l		(a7)+,d0-d7/a0-a6
	rts


*---------
* Fill a multiple bitplane area using a monochrome pattern
* In:	a0	VDI struct (odd address marks table operation)
*	d0	colour
*	d1	x1 destination or table address
*	d2	y1    - " -    or table length (high) and type (0 - y/x1/x2 spans)
*	d3-d4	x2,y2 destination
*	d5	pattern address
*	d6	mode
*	d7	interior/style
* Call:	a1	VDI struct (odd address marks table operation)
*	d0	height and width to fill (high and low word)
*	d1	x or table address
*	d2	y or table length (high) and type (0 - y/x1/x2 spans)
*	d3	pattern address
*	d4	colour
*	d6	mode
*	d7	interior/style
**	+colour in a really dumb way...
*
* called by:
*     engine/blit.s: lib_v_bar
*     engine/blit.s: vr_recfl
*     engine/blit.s: fill_area
*     engine/draw.s: hline
*     engine/draw.s: fill_spans
*---------
_fill:
	movem.l		d0-d7/a0-a6,-(a7)	; Used to have -3/4/6 for normal/both

	move.l		a0,a1

; The colour fetching has been expanded to put the address
; of the background colour on the stack.
; That's needed for non-solid replace mode.
; None of this is any kind of good idea except for bitplanes!

	exg		d4,d0
;	move.w		d4,d0
	sub.w		d2,d0
	addq.w		#1,d0
	swap		d0
	move.w		d3,d0
	sub.w		d1,d0
	addq.w		#1,d0

	move.l		d5,d3

	ijsr		_fill_area_r
	tst.l		d0
	lbgt		.l1,1
	lbmi		.l2,2
	move.l		_fallback_fill,d0
	bra		give_up

 label .l1,1
	movem.l		(a7)+,d0-d7/a0-a6
	rts

 label .l2,2					; Transform table fill into ordinary one
	move.w		8+2(a7),d0
	tst.w		d0
	bne		.fill_done		; Only y/x1/x2 spans available so far
	move.l		8*4(a7),d3		; Fetch a0
	bclr		#0,d3
	move.l		d3,a1
	move.l		4(a7),a2		; Fetch d1
.fill_loop:
	moveq		#0,d2
	move.w		(a2)+,d2
	moveq		#0,d1
	move.w		(a2)+,d1
	moveq		#1,d0
	swap		d0
	move.w		(a2)+,d0
	sub.w		d1,d0
	addq.w		#1,d0
	move.l		5*4(a7),d3
	move.l		0(a7),d4
	movem.l		a1-a2,-(a7)
	ijsr		_fill_area_r
	movem.l		(a7)+,a1-a2
	subq.w		#1,2*4(a7)
	bne		.fill_loop
.fill_done:
	movem.l		(a7)+,d0-d7/a0-a6
	rts


*---------
* Fill a multiple bitplane polygon using a monochrome pattern
* In:	a0	VDI struct (odd address marks table operation)
*	d0	colour
*	d1	points address
*	d2	number of points
*	d3	index address
*	d4	number of indices
*	d5	pattern address
*	d6	mode
*	d7	interior/style
* Call:	a1	VDI struct (odd address marks table operation)
*	d0	number of points and indices (high and low word)
*	d1	points address
*	d2	index address
*	d3	pattern address
*	d4	colour
*	d6	mode
*	d7	interior/style
**	+colour in a really dumb way...
*
* called by:
*     engine/draw.s: lib_v_bez_fill
*     engine/draw.s: lib_v_fillarea
*     engine/draw.s: _fill_poly
*---------
_fillpoly:
	movem.l		d0-d7/a0-a6,-(a7)	; Used to have -3/4/6 for normal/both

	move.l		a0,a1

; The colour fetching has been expanded to put the address
; of the background colour on the stack.
; That's needed for non-solid replace mode.
; None of this is any kind of good idea except for bitplanes!

	swap	d2
	move.w	d4,d2
	move.l	d0,d4
	move.l	d2,d0
	move.l	d3,d2
	move.l	d5,d3

	ijsr		_fill_poly_r
	tst.l		d0
	lbgt		.l1,1
	lbmi		.l2,2

 label .l2,2
	move.l		_fallback_fillpoly,d0
	bra		give_up

 label .l1,1
	movem.l		(a7)+,d0-d7/a0-a6
	rts


*---------
* Blit an area
* In:	a0	VDI struct, destination MFDB, VDI struct, source MFDB
*	d0	logic operation
*	d1-d2	x1,y1 source
*	d3-d6	x1,y1 x2,y2 destination
* Call:	a1	VDI struct,destination MFDB, VDI struct, source MFDB
*	d0	height and width to move (high and low word)
*	d1-d2	source coordinates
*	d3-d4	destination coordinates
*	d5	logic operation
*
* called by:
*     engine/blit.s: lib_vro_cpyfm
*---------
_blit:
	movem.l		d0-d7/a0-a6,-(a7)	; Used to have -3/4/6 for normal/both

	move.l		a0,a1

	move.l		d0,d7

	move.w		d6,d0
	sub.w		d4,d0
	addq.w		#1,d0
	swap		d0
	move.w		d5,d0
	sub.w		d3,d0
	addq.w		#1,d0

	ijsr		_blit_area_r
	tst.l		d0
	lbgt		.l1,1
	move.l		_fallback_blit,d0
	bra		give_up

 label .l1,1
	movem.l		(a7)+,d0-d7/a0-a6
	rts


*---------
* Draw some text
* In:	a0	VDI struct
*	a1	string address
*	a2	offset table or zero
*	d0	string length
*	d1	x1,y1 destination
* Call:	a1	VDI struct
*	a2	offset table or zero
*	d0	string length
*	d3-d4	destination coordinates
*	a4	string address
*
* called by:
*     engine/text.s: lib_v_gtext
*     engine/text.s: _draw_text
*     engine/text.s: v_ftext
*     engine/text.s: v_ftext_offset
*---------
_text:
	movem.l		d0-d7/a0-a6,-(a7)	; Was d2-d7/a3-a6

	move.l		a1,a4
	move.l		a0,a1

	move.w		d1,d4
	swap		d1
	move.w		d1,d3

	ijsr		_text_area_r
	tst.l		d0
	lbgt		.l1,1
	move.l		_fallback_text,d0
	bra		give_up

 label .l1,1
	movem.l		(a7)+,d0-d7/a0-a6
	rts


*---------
* Draw the mouse
* In:	a1	Pointer to Workstation struct
*	d0	x (low), old op bits (high)
*	d1	y
*	d2	0 (move), 1 (hide), 2 (show), Mouse* (change)
* Call:	a1	Pointer to Workstation struct
*	d0	x (low), old op bits (high)
*	d1	y
*	d2	0 (move shown), 1 (move hidden), 2 (hide), 3 (show), Mouse* (change)
* Out:	d0	mouse op to try again (low), pointer delay (high)
*
* called by:
*     engine/mouse.s: lib_vsc_form
*     engine/mouse.s: lib_v_show_c
*     engine/mouse.s: lib_v_hide_c
*---------
_mouse:
	ijsr		_mouse_draw_r
	rts


*---------
* Set palette colours
* In:	a0	VDI struct
*	d0	number of entries, start entry
*	a1	requested colour values (3 word/entry)
*	a2	colour palette
*
* called by:
*     engine/colours.s: set_palette
*     engine/support.s: initialize_palette
*---------
_set_palette:
	movem.l		d0-d7/a0-a6,-(a7)	; Overkill

	ijsr		_set_colours_r

	movem.l		(a7)+,d0-d7/a0-a6
	rts


*---------
* Get palette colour
* In:	a0	VDI struct
*	d0	fore- and background colour indices
* Out:	d0	fore- and background colour
*
* called by:
*     engine/draw.s: _default_line
*     engine/draw.s: _default_fill
*     engine/draw.s: _default_expand
*---------
_colour:
	movem.l		d1-d7/a0-a6,-(a7)

	ijsr		_get_colour_r

	movem.l		(a7)+,d1-d7/a0-a6
	rts


*---------
* Set palette colours
* initialize_palette(Virtual *vwk, int start, int n, int requested[][3], Colour palette[])
* To be called from C
*---------
_initialize_palette:
	movem.l		d0-d7/a0-a6,-(a7)	; Overkill

	move.l		15*4+4(a7),a0
	move.l		15*4+8(a7),d1
	move.l		15*4+12(a7),d0
	swap		d0
	move.w		d1,d0
	move.l		15*4+16(a7),a1
	move.l		15*4+20(a7),a2

	ijsr		_set_colours_r

	movem.l		(a7)+,d0-d7/a0-a6
	rts


*---------
* Give up and try other function
* This routine should only be branched to, it's not a subroutine!
* In:	d0	Address to other function
* Call:	d0-a6	Same values as at original call
*---------
give_up:
	pea	.return
	move.l	d0,-(a7)
	movem.l	8(a7),d0-d7/a0-a6
	rts
.return:
	movem.l	(a7)+,d0-d7/a0-a6
	rts

	end
