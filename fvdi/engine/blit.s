*****
* fVDI blit type functions
*
* Copyright 1997-2002, Johan Klockars 
* This software is licensed under the GNU General Public License.
* Please, see LICENSE.TXT for further information.
*****

transparent	equ	1		; Fall through?

	.include	"vdi.inc"
	.include	"macros.inc"

	xref	clip_rect
	xref	setup_plot,tos_colour
	xref	_expand_area
	xref	_pattern_ptrs
	xref	_default_line
	xref	_vr_transfer_bits,_colour_entry
	xref	_set_colour_table,_colour_table,_inverse_table

	xdef	v_bar,vr_recfl,vrt_cpyfm,vro_cpyfm
	xdef	vr_trnfm
	xdef	vr_transfer_bits,colour_entry
	xdef	set_colour_table,colour_table,inverse_table
	xdef	v_get_pixel

	xdef	lib_v_bar,lib_vr_recfl,lib_vrt_cpyfm,lib_vro_cpyfm
	xdef	lib_v_get_pixel

	xdef	_lib_vrt_cpyfm,_lib_vrt_cpyfm_nocheck,_lib_vro_cpyfm
	xdef	_lib_vr_trnfm
	xdef	_default_fill,_default_expand,_default_blit

	xdef	_fill_area


	text

* v_get_pixel - Standard Trap function
* Todo: -
* In:   a1      Parameter block
*       a0      VDI struct
v_get_pixel:
	uses_d1
	sub.w	#12,a7
	move.l	ptsin(a1),a2
	move.l	(a2),0(a7)		; Coordinates
	move.l	intout(a1),a2
	move.l	a2,4(a7)
	addq.l	#2,a2
	move.l	a2,8(a7)
	move.l	a7,a1
	bsr	lib_v_get_pixel
	add.w	#12,a7
	used_d1
	done_return			; Should be real_return

* lib_v_get_pixel - Standard Library function
* Todo: Convert when in bitplane modes
* In:   a1      Parameters   lib_v_get_pixel(x, y, &colour, &index)
*       a0      VDI struct
lib_v_get_pixel:
	move.l	d2,-(a7)
	clr.l	-(a7)
	move.l	a0,-(a7)
	move.w	(a1)+,d1
	move.w	(a1)+,d2
	move.l	vwk_real_address(a0),a2
	move.l	wk_r_get_pixel(a2),a2
	move.l	a7,a0
	jsr	(a2)
	move.l	(a7),a0
	addq.l	#8,a7
	move.l	(a1)+,a2
	move.w	d0,(a2)
	move.l	vwk_real_address(a0),a2
	move.l	wk_driver(a2),a2
	move.l	driver_device(a2),a2
	move.w	dev_clut(a2),d1
	move.l	vwk_real_address(a0),a2
	cmp.w	#1,d1			; Hardware CLUT? (used to test look_up_table)
	beq	.convert_to_index
	swap	d0
	cmp.w	#16,wk_screen_mfdb_bitplanes(a2)
	bhi	.more_than_16_bit
	moveq	#-1,d0
.more_than_16_bit:
	move.l	(a1)+,a2
	move.w	d0,(a2)
.end_lib_v_get_pixel:
	move.l	(a7)+,d2
	rts

.convert_to_index:
	moveq	#-1,d0			; This should of course convert!
	bra	.end_lib_v_get_pixel


* v_bar - Standard Trap function
* Todo: -
* In:   a1      Parameter block
*       a0      VDI struct
v_bar:
	uses_d1
	move.l	ptsin(a1),a1
	bsr	lib_v_bar
	used_d1
	done_return			; Should be real_return

* lib_v_bar - Standard Library function
* Todo: -
* In:   a1      Parameters   lib_v_bar(points)
*       a0      VDI struct
lib_v_bar:
	tst.w	vwk_fill_interior(a0)
	bne	.do_fill		; Not hollow, so draw
	tst.w	vwk_fill_perimeter(a0)
	bne	.no_fill		; Hollow and perimeter, so don't draw (right?)
.do_fill:
	movem.l	a0-a1,-(a7)
	bsr	lib_vr_recfl
	movem.l	(a7)+,a0-a1
	tst.w	vwk_fill_perimeter(a0)
	beq	.bar_end	; 1$ .end
.no_fill:
	movem.l	d2-d7,-(a7)
	move.l	vwk_real_address(a0),a2
	move.l	wk_r_fill(a2),a2
	move.l	_pattern_ptrs,d5

	moveq	#0,d6
	move.w	vwk_mode(a0),d6		; Mode
	move.l	#$00010000,d7		; Solid

	movem.w	(a1),d1-d4		; Draw top/bottom perimeter
	move.w	d2,d4
	move.l	vwk_fill_colour(a0),d0
	bsr	clip_rect
	lblt	.skip1,3
	jsr	(a2)
 label .skip1,3

	movem.w	(a1),d1-d4		; Draw bottom/top perimeter
	cmp.w	d2,d4
	lbeq	.end,2
	move.w	d4,d2
	move.l	vwk_fill_colour(a0),d0
	bsr	clip_rect
	lblt	.skip2,4
	jsr	(a2)
 label .skip2,4

	movem.w	(a1),d1-d4		; Draw left perimeter
	cmp.w	d2,d4			; Bug compatibility
	lbge	.no_swap1,5
	exg	d2,d4
 label .no_swap1,5
	addq.w	#1,d2
	subq.w	#1,d4
	move.w	d1,d3
	move.l	vwk_fill_colour(a0),d0
	bsr	clip_rect
	lblt	.skip3,6
	jsr	(a2)
 label .skip3,6

	movem.w	(a1),d1-d4		; Draw right perimeter
	cmp.w	d2,d4			; Bug compatibility
	lbge	.no_swap2,7
	exg	d2,d4
 label .no_swap2,7
	cmp.w	d1,d3
	lbeq	.end,2
	addq.w	#1,d2
	subq.w	#1,d4
	move.w	d3,d1
	move.l	vwk_fill_colour(a0),d0
	bsr	clip_rect
	lblt	.skip4,8
	jsr	(a2)
 label .skip4,8

 label .end,2
	movem.l	(a7)+,d2-d7

.bar_end:	; 1$:
	rts


* vr_recfl - Standard Trap function
* Todo:
* In:   a1      Parameter block
*       a0      VDI struct
vr_recfl:
	uses_d1
	move.l	ptsin(a1),a1
	bsr	lib_vr_recfl
	used_d1
	done_return			; Should be real_return

* lib_vr_recfl - Standard Library function
* Todo: -
* In:   a1      Parameters   lib_vr_recfl(points)
*       a0      VDI struct
lib_vr_recfl:
;	move.w	#0,d0			; Background colour
;	swap	d0
	move.l	vwk_fill_colour(a0),d0

	movem.l	d2-d7,-(a7)
;	movem.w	(a1),d1-d4		; Get rectangle coordinates
	moveq	#0,d1			; Need clear upper word for
	move.w	(a1)+,d1		;  compatibility with current
	moveq	#0,d2			;  span fill implementation (990704)
	move.w	(a1)+,d2
	moveq	#0,d3
	move.w	(a1)+,d3
	moveq	#0,d4
	move.w	(a1)+,d4

	cmp.w	d2,d4			; Bug compatibility
	bge	.no_swapy
	exg	d2,d4
.no_swapy:
	cmp.w	d1,d3			; (only seen in NVDI polygon code)
	bge	.no_swapx
	exg	d1,d3
.no_swapx:
	bsr	clip_rect
	blt	.end			; Empty rectangle?

	move.l	vwk_real_address(a0),a2
	move.l	wk_r_fill(a2),a1

	move.w	vwk_fill_interior(a0),d5

	moveq	#0,d7
	move.w	d5,d7
	swap	d7			; Interior/0 (style set below, if any)

	tst.w	d5
	bne	.solid
	swap	d0			; Hollow, so background colour
;	bra	.end			; Hollow, so no drawing (right?)
.solid:

	cmp.w	#4,d5
	bne	.no_user
	move.l	vwk_fill_user_pattern_in_use(a0),a2
	bra	.got_pattern
.no_user:
	add.w	d5,d5
	add.w	d5,d5
	lea	_pattern_ptrs,a2
	move.l	0(a2,d5.w),a2
	and.w	#$08,d5			; Check former bit 1 (interior 2 or 3)
	beq	.got_pattern
	move.w	vwk_fill_style(a0),d5

	move.w	d5,d7			; Interior/Style

	subq.w	#1,d5
	lsl.w	#5,d5			; Add style index
	add.w	d5,a2
.got_pattern:
	move.l	a2,d5

	moveq	#0,d6
	move.w	vwk_mode(a0),d6		; Mode

	jsr	(a1)
.end:
	movem.l	(a7)+,d2-d7
	rts


_fill_area:
	movem.l	d2-d7/a2,-(a7)
	lea	4+4*4(a7),a1
	move.l	0(a1),a0
	moveq	#0,d1
	move.w	2+4(a1),d1
	moveq	#0,d2
	move.w	2+8(a1),d2
	moveq	#0,d3
	move.w	2+12(a1),d3
	moveq	#0,d4
	move.w	2+16(a1),d4
	move.l	20(a1),d0
	bsr	clip_rect
	blt	.end_fill_area		; Empty rectangle?

	move.l	vwk_real_address(a0),a2
	move.l	wk_r_fill(a2),a1
	move.l	_pattern_ptrs,d5

	moveq	#1,d6			; Replace mode
	move.l	#$00010000,d7		; Solid

	jsr	(a1)
.end_fill_area:
	movem.l	(a7)+,d2-d7/a2
	rts


* _default_fill - Line by line (or pixel by pixel) fill routine
* In:	a0	VDI struct (odd address marks table operation)
*	d0	Colours
*	d1	x1 destination or table address
*	d2	y1   - " -     or table length (high) and type (0 - y/x1/x2 spans)
*	d3-d4.w	x2,y2 destination
*	d5	Pointer to pattern
*	d6	Mode
*	d7	Interior/style
* Call:	a0	VDI struct, 0 (destination MFDB)
*	d0	Colours
*	d1-d2.w	Start coordinates
*	d3-d4.w	End coordinates
*	d5	Pattern
_default_fill:
	movem.l	d1-d7/a0-a2,-(a7)

	move.w	a0,d7
	and.w	#1,d7
	sub.w	d7,a0

	move.l	d5,a2

	cmp.w	#1,d6			; Not replace?
	bne	.pattern

	move.l	a2,a1
	moveq	#8-1,d5
	move.l	d6,-(a7)
.check_pattern:
	move.l	(a1)+,d6		; All ones?
	addq.l	#1,d6
	dbne	d5,.check_pattern
	beq	.no_pattern
	move.l	(a7)+,d6
	bra	.pattern
.no_pattern:
	move.l	(a7)+,d6
	moveq	#-1,d5

	move.l	vwk_real_address(a0),a1
	move.l	wk_r_line(a1),a1
	cmp.l	#_default_line,a1	; No real acceleration?
	beq	.pattern

	tst.w	d7
	bne	.table_lfill

	move.w	d4,d7
	moveq	#0,d6
	move.w	vwk_mode(a0),d6
.loopy_sl:
	move.w	d2,d4
	jsr	(a1)
	addq.w	#1,d2
	cmp.w	d7,d2
	ble	.loopy_sl

.end_default_fill:
	movem.l	(a7)+,d1-d7/a0-a2
	rts

.table_lfill:
	tst.w	d2
	bne	.end_default_fill	; Only y/x1/x2 spans available so far
	move.l	d2,d6
	swap	d6
	subq.w	#1,d6
	blt	.end_default_fill
	move.l	d1,a2
.tlfill_loop:
	moveq	#0,d2
	move.w	(a2)+,d2
	move.w	d2,d4
	moveq	#0,d1
	move.w	(a2)+,d1
	move.w	(a2)+,d3
	jsr	(a1)
	dbra	d6,.tlfill_loop
	bra	.end_default_fill

* Call:	a0	VDI struct, 0 (destination MFDB)
*	d0	Colour values
*	d1-d2.w	Coordinates
*	d6	Mode
*	a3-a4	Set/get pixel
.pattern:
	movem.l	a3-a4,-(a7)

	move.l	vwk_real_address(a0),a1
	move.l	wk_r_get_colour(a1),a1	; Index to real colour
	jsr	(a1)

	move.w	d6,-(a7)
	bsr	setup_plot
	addq.l	#2,a7

;	move.l	d5,a2

	clr.l	-(a7)			; No MFDB => draw on screen
	move.l	a0,-(a7)
	move.l	a7,a0			; a0 no longer -> VDI struct!

	tst.w	d7
	bne	.table_pfill

	move.w	d1,d6
.loopy_pp:
	move.w	d6,d1			; x
	move.w	d2,d5
	and.w	#$000f,d5
	add.w	d5,d5
	move.w	0(a2,d5.w),d5
	rol.w	d1,d5
.loopx_pp:
	rol.w	#1,d5
	jsr	(a1)
	addq.w	#1,d1
	cmp.w	d3,d1
	ble	.loopx_pp
	addq.w	#1,d2
	cmp.w	d4,d2
	ble	.loopy_pp
.end_pfill:
	move.l	(a7),a0
	addq.l	#8,a7
	movem.l	(a7)+,a3-a4
	bra	.end_default_fill

.table_pfill:
	tst.w	d2
	bne	.end_pfill		; Only y/x1/x2 spans available so far
	move.l	d2,d6
	swap	d6
	subq.w	#1,d6
	blt	.end_pfill

	move.l	a5,-(a7)
	move.l	d1,a5
.tploopy_pp:
	moveq	#0,d2
	move.w	(a5)+,d2
	moveq	#0,d1
	move.w	(a5)+,d1
	move.w	(a5)+,d3

	move.w	d2,d5
	and.w	#$000f,d5
	add.w	d5,d5
	move.w	0(a2,d5.w),d5
	rol.w	d1,d5
.tploopx_pp:
	rol.w	#1,d5
	jsr	(a1)
	addq.w	#1,d1
	cmp.w	d3,d1
	ble	.tploopx_pp
	dbra	d6,.tploopy_pp
	move.l	(a7)+,a5
	bra	.end_pfill


* vrt_cpyfm - Standard Trap function
* Todo: Should jump via indirection table
* In:   a1      Parameter block
*       a0      VDI struct
vrt_cpyfm:
	sub.l	#18,a7
	move.l	intin(a1),a2
	move.w	(a2)+,0(a7)	; Mode
	move.l	a2,14(a7)	; Pens
	move.l	ptsin(a1),2(a7)	; Points
	move.l	control(a1),a2
	move.l	14(a2),6(a7)	; Source
	move.l	18(a2),10(a7)	; Destination
	move.l	a7,a1
	bsr	lib_vrt_cpyfm
	add.l	#18,a7
	done_return


* lib_vrt_cpyfm - Standard Library function
* Todo: ?
*	Clipping should probably not be done for non-screen destinations
* In:   a1      Parameters (mode, points, source, destination, pens)
*       a0      VDI struct
_lib_vrt_cpyfm:
lib_vrt_cpyfm:
	move.l	14(a1),a2
	move.l	(a2),d0		; Background colour (top word)
	swap	d0		; Foreground colour (bottom word)

	move.l	vwk_real_address(a0),a2
	cmp.w	wk_screen_palette_size(a2),d0
	blo	.okf
	move.w	#BLACK,d0
.okf:
	swap	d0
	cmp.w	wk_screen_palette_size(a2),d0
	blo	.okb
	move.w	#WHITE,d0
.okb:
	bra	nocheck

_lib_vrt_cpyfm_nocheck:
	move.l	14(a1),a2
	move.l	(a2),d0		; Colours

nocheck:
	swap	d0

	uses_d1
	movem.l	d2-d5/a3-a5,-(a7)

	sub.l	#16,a7		; VDI struct, destination MFDB, VDI struct, source MFDB
	move.l	a0,(a7)
	move.l	a0,8(a7)

	move.l	vwk_real_address(a0),a3
	moveq	#-1,d5		; Default to clipping

	move.l	10(a1),a2	; a2 - destination MFDB
	move.l	a2,d3
	lbeq	.screen_dest,1
	move.l	mfdb_address(a2),d3
	lbeq	.screen_dest,1
	cmp.l	wk_screen_mfdb_address(a3),d3
	lbeq	.screen_dest,1
	moveq	#0,d5		; Not screen as destination
 label .screen_dest,1
	
	move.l	2(a1),a2	; Get rectangle coordinates
	movem.w	8(a2),d1-d4

	; This should not be done if bitmap scaling is possible!
	move.w	d1,d3		; Do not use lower right destination coordinates at all
	move.w	d2,d4
	sub.w	0(a2),d3
	add.w	4(a2),d3
	sub.w	2(a2),d4
	add.w	6(a2),d4

	tst.w	d5
	lbeq	.no_clip,2
	bsr	clip_rect	; Clipping necessary?
	blt	.end_vrt_cpyfm	; .end
 label .no_clip,2
	
	movem.w	d1-d4,-(a7)
	sub.w	8(a2),d1	; Calculate clipped source coordinates
	sub.w	10(a2),d2
	add.w	0(a2),d1
	add.w	2(a2),d2

	move.l	10(a1),a2	; a2 - destination MFDB
	move.l	a2,8+4(a7)

	move.l	6(a1),a2	; a2 - source MFDB
	move.l	a2,8+12(a7)

	move.l	wk_r_expand(a3),d3

	movem.l	d5-d7,-(a7)
	move.w	0(a1),d7	; Mode
	move.l	d3,a1
	movem.w	12(a7),d3-d6	; Destination coordinates
	lea	12+8(a7),a0	; a0 no longer -> VDI struct!
	jsr	(a1)
	move.l	(a0),a0
	movem.l	(a7)+,d5-d7
	addq.l	#8,a7

.end_vrt_cpyfm:			; .end:
	add.l	#16,a7
	movem.l	(a7)+,d2-d5/a3-a5
	used_d1
	moveq	#1,d0		; Successful
	rts


* _default_expand - Pixel by pixel mono-expand routine
* In:	a0	VDI struct, destination MFDB, VDI struct, source MFDB
*	d0	Colours
*	d1-d2.w	Source coordinates
*	d3-d4.w	Destination start coordinates
*	d5-d6.w	Destination end coordinates
*	d7	Mode
* Call:	a0	VDI struct, destination MFDB
*	d0	Colour values
*	d1-d2.w	Coordinates
*	a3-a4	Set/get pixel
_default_expand:
	movem.l	d0-d7/a0-a6,-(a7)
	movem.w	d3-d4,-(a7)

	move.l	4(a0),d3
	beq	.not2mono
	move.l	d3,a2
	move.l	mfdb_address(a2),d4
	beq	.not2mono
	cmp.w	#1,mfdb_bitplanes(a2)
	bne	.not2mono
	move.l	(a0),a2
	move.l	vwk_real_address(a2),a2
	cmp.l	wk_screen_mfdb_address(a2),d4
	bne	.to_mono
.not2mono:

	move.l	a0,-(a7)
	move.l	(a0),a0
	move.l	vwk_real_address(a0),a2
	move.l	wk_r_get_colour(a2),a2	; Index to real colour
	jsr	(a2)
	move.l	(a7)+,a0

	move.l	a0,a2
	move.l	(a0),a0
	move.w	d7,-(a7)
	bsr	setup_plot
	addq.l	#2,a7
	move.l	a2,a0

	move.l	12(a0),a2	; Source MFDB

	move.l	0(a2),a5	; Point to bitmap
	mulu	8(a2),d2
	add.l	d2,d2
	add.l	d2,a5
	sub.w	8(a2),a5	; Added back below
	sub.w	8(a2),a5	; Added back below
	move.w	d1,d2
	lsr.w	#4,d2
	add.w	d2,d2
	add.w	d2,a5
	and.w	#$0f,d1
	move.l	a5,-(a7)
	move.w	d1,-(a7)

	move.w	6+2(a7),d2
        
.loopy_pp_vrt:		; .loopy_pp:
	move.w	6+0(a7),d1
	move.l	2(a7),a5	; To start of next row
	add.w	8(a2),a5
	add.w	8(a2),a5
	move.l	a5,2(a7)
	move.w	(a5)+,d3	; Scroll to reach start pixel
	move.w	0(a7),d4
	lsl.w	d4,d3
	neg.w	d4		; Number of pixels left in word
	add.w	#16,d4

.loopx_pp_vrt:		; .loopx_pp:
	lsl.w	#1,d3
	jsr	(a1)
	subq.w	#1,d4		; Fetch new word when needed
	bne	.bits_left
	move.w	(a5)+,d3
	moveq	#16,d4
.bits_left:
	addq.w	#1,d1		; Increase x
	cmp.w	d5,d1
	ble	.loopx_pp_vrt	; .loopx_pp
	addq.w	#1,d2		; Increase y
	cmp.w	d6,d2
	ble	.loopy_pp_vrt	; .loopy_pp
	add.l	#10,a7

.dexp_end:
	movem.l	(a7)+,d0-d7/a0-a6
	rts

.to_mono:
	movem.w	(a7)+,d3-d4
	move.l	a0,a1
	exg	d0,d6
	sub.w	d4,d0
	addq.w	#1,d0
	swap	d0
	move.w	d5,d0
	sub.w	d3,d0
	addq.w	#1,d0
	bsr	_expand_area
	bra	.dexp_end


* vro_cpyfm - Standard Trap function
* Todo: Should jump via indirection table
* In:   a1      Parameter block
*       a0      VDI struct
vro_cpyfm:
	sub.l	#14,a7
	move.l	intin(a1),a2
	move.w	(a2),0(a7)	; Mode
	move.l	ptsin(a1),2(a7)	; Points
	move.l	control(a1),a2
	move.l	14(a2),6(a7)	; Source
	move.l	18(a2),10(a7)	; Destination
	move.l	a7,a1
	bsr	lib_vro_cpyfm
	add.l	#14,a7
	done_return

* lib_vro_cpyfm - Standard Library function
* Todo: ?
* In:   a1      Parameters (mode, points, source, destination)
*       a0      VDI struct
_lib_vro_cpyfm:
lib_vro_cpyfm:
	uses_d1
	movem.l	d2-d5/a3-a5,-(a7)

	sub.l	#16,a7		; VDI struct, destination MFDB, VDI struct, source MFDB
	move.l	a0,(a7)
	move.l	a0,8(a7)

	move.l	vwk_real_address(a0),a3
	moveq	#-1,d5		; Default to clipping

	move.l	10(a1),a2	; a2 - destination MFDB
	move.l	a2,d3
	beq	.screen_dest
	move.l	mfdb_address(a2),d3
	beq	.screen_dest
	cmp.l	wk_screen_mfdb_address(a3),d3
	beq	.screen_dest
	moveq	#0,d5		; Not screen as destination
.screen_dest:
	move.l	a2,4(a7)

	move.l	6(a1),a2	; a2 - source MFDB
	move.l	a2,12(a7)

	move.l	2(a1),a2	; Get rectangle coordinates
	movem.w	8(a2),d1-d4

	; This should not be done if bitmap scaling is possible!
	move.w	d1,d3		; Do not use lower right destination coordinates at all
	move.w	d2,d4
	sub.w	0(a2),d3
	add.w	4(a2),d3
	sub.w	2(a2),d4
	add.w	6(a2),d4

	tst.w	d5
	beq	.no_clip
	bsr	clip_rect	; Clipping necessary?
	blt	.end_vro_cpyfm	; .end
.no_clip:
	movem.w	d1-d4,-(a7)
	sub.w	8(a2),d1	; Calculate clipped source coordinates
	sub.w	10(a2),d2
	add.w	0(a2),d1
	add.w	2(a2),d2

	move.l	wk_r_blit(a3),d3

	movem.l	d6,-(a7)
	move.w	0(a1),d0
	move.l	d3,a1
	movem.w	4(a7),d3-d6
	lea	4+8(a7),a0	; a0 no longer -> VDI struct!
	jsr	(a1)
	move.l	(a0),a0
	movem.l	(a7)+,d6
	addq.l	#8,a7

.end_vro_cpyfm:			; .end:
	add.l	#16,a7

	movem.l	(a7)+,d2-d5/a3-a5
	used_d1
	rts


* setup_blit - Internal function
*
* Sets up pointers to pixel blit functions
* In:	a0	VDI struct
*	d0	drawing mode
* Out:	a1	blit function
*	a3	set pixel function
*	a4	get pixel function
setup_blit:
	move.l	vwk_real_address(a0),a1
	move.l	wk_r_set_pixel(a1),a3
	move.l	wk_r_get_pixel(a1),a4
	ifne mc68000
	move.w d0,a1
	add.l  a1,a1
	add.l  a1,a1
	move.l blit_routines(pc,a1),a1
	else
	ifne mcoldfire
	ext.l d0
	move.l blit_routines(pc,d0.l*4),a1
	else
	move.l blit_routines(pc,d0.w*4),a1
	endc
	endc
	rts

blit_routines:
	dc.l	blit_0,blit_1,blit_2,blit_3,blit_4,blit_5,blit_6,blit_7
	dc.l	blit_8,blit_9,blit_a,blit_b,blit_c,blit_d,blit_e,blit_f

* _default_blit - Pixel by pixel blit routine
* In:	a0	VDI struct, destination MFDB, VDI struct, source MFDB
*	d0	Mode
*	d1-d2.w	Source coordinates
*	d3-d4.w	Destination start coordinates
*	d5-d6.w	Destination end coordinates
* Call:	a0	VDI struct, destination MFDB, VDI struct, source MFDB
*	d1	Source x (high), destination x (low)
*	d2	Source y (high), destination y (low)
*	a3-a4	Set/get pixel
_default_blit:
	move.l	a0,a2
	move.l	(a0),a0
	bsr	setup_blit
	move.l	a2,a0

	cmp.w	d4,d2
	blo	.move_down
	bhi	.move_up
	cmp.w	d3,d1
	blo	.move_down

.move_up:
	swap	d2
	move.w	d4,d2
	swap	d1
	move.w	d3,d1
	move.l	d1,-(a7)
        
.loopy_upp:
	move.l	0(a7),d1
.loopx_upp:
	jsr	(a1)
	add.l	#$10001,d1	; Increase x
	cmp.w	d5,d1
	ble	.loopx_upp
	add.l	#$10001,d2	; Increase y
	cmp.w	d6,d2
	ble	.loopy_upp
	add.l	#4,a7
	rts

.move_down:
	add.w	d6,d2
	sub.w	d4,d2
	swap	d2
	move.w	d6,d2
	add.w	d5,d1
	sub.w	d3,d1
	swap	d1
	move.w	d5,d1
	move.l	d1,-(a7)

.loopy_dpp:
	move.l	0(a7),d1
.loopx_dpp:
	jsr	(a1)
	sub.l	#$10001,d1	; Decrease x
	cmp.w	d3,d1
	bge	.loopx_dpp
	sub.l	#$10001,d2	; Decrease y
	cmp.w	d4,d2
	bge	.loopy_dpp
	add.l	#4,a7
	rts


* vr_transfer_bits - Standard Trap function
* Todo: ?
* In:   a1      Parameter block
*       a0      VDI struct
vr_transfer_bits:
	uses_d1
	move.l	d2,-(a7)
	
	move.l	intin(a1),a2
	moveq	#0,d0
	move.w	0(a2),d0
	move.l	d0,-(a7)
	move.l	ptsin(a1),a2
	pea	8(a2)
	pea	0(a2)
	move.l	control(a1),a2
	move.l	18(a2),-(a7)
	move.l	14(a2),-(a7)
	move.l	a0,-(a7)
	jsr	_vr_transfer_bits
	add.w	#24,a7

	move.l	(a7)+,d2
	used_d1
	done_return


* *color* - Standard Trap function
* Todo: ?
* In:   a1      Parameter block
*       a0      VDI struct
colour_entry:
	uses_d1
	movem.l	d2/a1,-(a7)

	move.l	intout(a1),-(a7)
	move.l	intin(a1),-(a7)
	move.l	control(a1),a2
	moveq	#0,d0
	move.w	subfunction(a2),d0
	move.l	d0,-(a7)
	move.l	a0,-(a7)
	jsr	_colour_entry
	add.w	#16,a7

	movem.l	(a7)+,d2/a1
	move.l	control(a1),a2
	move.w	d0,L_intout(a2)
	used_d1
	done_return


* vs_*ctab* - Standard Trap function
* Todo: ?
* In:   a1      Parameter block
*       a0      VDI struct
set_colour_table:
	uses_d1
	movem.l	d2/a1,-(a7)

	move.l	intin(a1),-(a7)
	move.l	control(a1),a2
	moveq	#0,d0
	move.w	subfunction(a2),d0
	move.l	d0,-(a7)
	move.l	a0,-(a7)
	jsr	_set_colour_table
	add.w	#12,a7

	movem.l	(a7)+,d2/a1
	move.l	intout(a1),a2
	move.w	d0,(a2)
	used_d1
	done_return


* *ctab* - Standard Trap function
* Todo: ?
* In:   a1      Parameter block
*       a0      VDI struct
colour_table:
	uses_d1
	movem.l	d2/a1,-(a7)

	move.l	intout(a1),-(a7)
	move.l	intin(a1),-(a7)
	move.l	control(a1),a2
	moveq	#0,d0
	move.w	subfunction(a2),d0
	move.l	d0,-(a7)
	move.l	a0,-(a7)
	jsr	_colour_table
	add.w	#16,a7

	movem.l	(a7)+,d2/a1
	move.l	control(a1),a2
	move.w	d0,L_intout(a2)
	used_d1
	done_return


* *itab - Standard Trap function
* Todo: ?
* In:   a1      Parameter block
*       a0      VDI struct
inverse_table:
	uses_d1
	movem.l	d2/a1,-(a7)

	move.l	intout(a1),-(a7)
	move.l	intin(a1),-(a7)
	move.l	control(a1),a2
	moveq	#0,d0
	move.w	subfunction(a2),d0
	move.l	d0,-(a7)
	move.l	a0,-(a7)
	jsr	_inverse_table
	add.w	#16,a7

	movem.l	(a7)+,d2/a1
	move.l	control(a1),a2
	move.w	d0,L_intout(a2)
	used_d1
	done_return

	
* vr_trnfm - Standard Trap function
* Todo: ?
* In:   a1      Parameter block
*       a0      VDI struct
vr_trnfm:
	move.l	control(a1),a2
	lea	14(a2),a1
	bsr	_lib_vr_trnfm
	done_return

* lib_vr_trnfm - Standard Library function
* Todo: Should use C2P from MGIF instead of the current crap!
* In:   a1      Parameters   lib_vr_trnfm(source, dest)
*       a0      VDI struct
_lib_vr_trnfm:
	uses_d1
	movem.l	d2-d7/a3-a6,-(a7)
	move.l	(a1)+,a2
	move.l	(a1),a1
	exg	a1,a2		; a1 - source, a2 - dest

	move.l	mfdb_address(a1),a3
	move.l	mfdb_address(a2),a4
	move.w	mfdb_height(a1),d0
	move.w	mfdb_wdwidth(a1),d1
	move.w	mfdb_bitplanes(a1),d2

	cmp.l	a3,a4
	beq	in_place

;	cmp.w	#1,d2			; Single plane is the same
;	beq	end_vr_trnfm		;  but needs to copy

	tst.w	mfdb_standard(a1)
	beq	to_standard

* Reorganize standard bitplanes into device specific ones.
* Does not work in place.
* In:	d0.w	mfdb_height
*	d1.w	mfdb_wdwidth (pixels / 16)
*	d2.w	bitplanes
*	a3	mfdb_address source
*	a4	mfdb_address destination
	move.w	d0,d3
	mulu	d1,d3
	add.l	d3,d3			; d3 = pix/16 * height * 2 = words/plane

	move.l	a3,a5

	move.w	d0,d7
	subq.w	#1,d7

 label .loop1,1
	move.w	d1,d6
	subq.w	#1,d6

 label .loop2,2
	move.w	d2,d5
	subq.w	#1,d5

 label .loop3,3
	move.w	(a3),(a4)+
	add.l	d3,a3
	ldbra	d5,.loop3,3
	addq.w	#2,a5
	move.l	a5,a3
	ldbra	d6,.loop2,2
	ldbra	d7,.loop1,1
	bra	finish_up


to_standard:
	move.l	vwk_real_address(a0),a5
	move.l	wk_driver(a5),a5
	move.l	driver_device(a5),a5
	move.w	dev_format(a5),d6
	and.w	#2,d6
	beq	not_chunky_ts

	move.w	d0,d7
	mulu	d1,d7
	add.l	d7,d7			; d7 = pix/16 * height * 2 = words/plane
	moveq	#2,d6

	cmp.w	#16,d2
	bne	.not_16bit_ts
	bsr	to_standard_16
	bra	end_vr_trnfm

.not_16bit_ts:
	cmp.w	#8,d2
	bne	.not_8bit_ts
	bsr	to_standard_8
	bra	end_vr_trnfm

.not_8bit_ts:
;	move.w	d1,d6			; Copy source to destination since the
;	mulu	d2,d6			;  current dechunk routine is in-place
;	mulu	d0,d6			; d6 = pix/16 * planes * height * 2 = total words
;
;	subq.l	#1,d6
;	move.l	a3,a5
;	move.l	a4,a6
;5$:
;	move.w	(a5)+,(a6)+
;	dbra	d6,5$

	cmp.w	#32,d2
	bne	not_32bit_ts
	bsr	to_standard_32
	bra	end_vr_trnfm


* Transform device specific to standard format.
* Works in place due to source copying.
* In:	d0.w	mfdb_height
*	d1.w	mfdb_wdwidth (pixels / 16)
*	d6.w	group result step (2 or 16/32/64)
*	d7.l	single result step (bytes per plane or 2)
*	a3	mfdb_address source
*	a4	mfdb_address destination
to_standard_8:
	sub.w	#16,a7			; This is specialized for 8 bit!
	move.l	a7,a6			; Doesn't seem to work correctly (Jinnee)
	move.l	a4,a5

	move.w	d6,d1

	subq.w	#1,d0

 label .loop0b,0
	swap	d0
	move.w	mfdb_wdwidth(a1),d0
	subq.w	#1,d0

 label .loop1b,1
	movem.l	(a3)+,d2-d5
	movem.l	d2-d5,(a6)
	moveq	#7,d3		; Was 15

 label .loop2b,2
	moveq	#15,d4		; Was 14

 label .loop3b,3
	move.b	0(a6,d4),d5	; Was .w
	add.b	d5,d5		; Was .w
	addx.w	d6,d6		; Was .b
;	roxr.b	#1,d6		; This _is_ the right one!???
	move.b	d5,0(a6,d4)	; Was .w
	subq.l	#1,d4		; Was 2
	lbpl	.loop3b,3
	move.w	d6,(a4)		; Was .b
	add.l	d7,a4

	ldbra	d3,.loop2b,2
	add.w	d1,a5
	move.l	a5,a4
	ldbra	d0,.loop1b,1
	swap	d0
	ldbra	d0,.loop0b,0

	add.w	#16,a7
	rts


to_standard_16:
	sub.w	#32,a7			; This is specialized for 16 bit!
	move.l	a7,a6
	move.l	a4,a5

	move.w	d6,d1

	subq.w	#1,d0			; d0 = height - 1

 label .loop0,0
	swap	d0
	move.w	mfdb_wdwidth(a1),d0
	subq.w	#1,d0			; d1 = width - 1

 label .loop1,1
	movem.l	(a3)+,d2-d5		; Copy 16 words due to bad dechunky routine
	movem.l	d2-d5,(a6)
	movem.l	(a3)+,d2-d5
	movem.l	d2-d5,16(a6)
	moveq	#15,d3			; d3 = bitplanes - 1 

 label .loop2,2
	moveq	#30,d4			; d4 = last word index

 label .loop3,3
	move.w	0(a6,d4),d5
	add.w	d5,d5
;	addx.w	d6,d6
	roxr.w	#1,d6		; This _is_ the right one!
	move.w	d5,0(a6,d4)
	subq.w	#2,d4
	lbpl	.loop3,3
	move.w	d6,(a4)
	add.l	d7,a4

	ldbra	d3,.loop2,2
	add.w	d1,a5
	move.l	a5,a4
	ldbra	d0,.loop1,1
	swap	d0
	ldbra	d0,.loop0,0

	add.w	#32,a7
	rts


to_standard_32:
	sub.w	#64,a7			; This is specialized for 32 bit!
	move.l	a7,a6			; Doesn't seem to work correctly (Jinnee)
	move.l	a4,a5

	move.w	d6,d1

	subq.w	#1,d0

 label .loop0c,0
	swap	d0
	move.w	mfdb_wdwidth(a1),d0
	subq.w	#1,d0

 label .loop1c,1
	movem.l	(a3)+,d2-d5
	movem.l	d2-d5,(a6)
	movem.l	(a3)+,d2-d5
	movem.l	d2-d5,16(a6)
	movem.l	(a3)+,d2-d5
	movem.l	d2-d5,32(a6)
	movem.l	(a3)+,d2-d5
	movem.l	d2-d5,48(a6)
	moveq	#31,d3		; Was 15

 label .loop2c,2
	moveq	#60,d4		; Was 62

 label .loop3c,3
	move.l	0(a6,d4),d5	; Was .w
	add.l	d5,d5		; Was .w
;;	addx.w	d6,d6		; Was .l
;	roxr.l	#1,d6		; This _is_ the right one!
	roxr.w	#1,d6		; This _is_ the right one!
	move.l	d5,0(a6,d4)	; Was .w
	subq.l	#4,d4		; Was 2
	lbpl	.loop3c,3
	move.w	d6,(a4)		; Was .l
	add.l	d7,a4

	ldbra	d3,.loop2c,2
	add.w	d1,a5
	move.l	a5,a4
	ldbra	d0,.loop1c,1
	swap	d0
	ldbra	d0,.loop0c,0

	add.w	#64,a7
	rts


not_32bit_ts:
not_chunky_ts:
	move.w	d2,d3			; Reorganize device specific
	add.w	d3,d3			;  bitplanes into standard ones

	move.l	a3,a5

	move.w	d2,d7
	subq.w	#1,d7

 label .loop1d,1
	move.w	d0,d6
	subq.w	#1,d6

 label .loop2d,2
	move.w	d1,d5
	subq.w	#1,d5

 label .loop3d,3
	move.w	(a3),(a4)+
	add.w	d3,a3
	ldbra	d5,.loop3d,3
	ldbra	d6,.loop2d,2
	addq.w	#2,a5
	move.l	a5,a3
	ldbra	d7,.loop1d,1
	bra	end_vr_trnfm


in_place:
	cmp.w	#1,d2			; Single plane is the same
	beq	end_vr_trnfm
	tst.w	mfdb_standard(a1)
	beq	to_standard_ip

* Reorganize standard bitplanes into device specific ones.
* In:	d0.w	mfdb_height
*	d1.w	mfdb_wdwidth (pixels / 16)
*	d2.w	bitplanes
*	a3	mfdb_address source
*	a4	mfdb_address destination
	move.w	d0,d7			; Reorganize standard bitplanes
	move.w	d1,d6			;  into device specific ones

	move.w	d1,d3
	mulu	d2,d3
	mulu	d0,d3
	add.l	d3,d3			; d0 = pix/16 * planes * height * 2 = total bytes

	mulu	d0,d1
	add.w	d1,d1			; d1 = bytes per standard line * height

	move.l	d3,d0
	subq.l	#2,d0
	subq.l	#2,d1
	addq.l	#2,a4

	move.l	a0,-(a7)

	subq.w	#1,d7

 label .loop1,1
	move.w	d6,d5
	subq.w	#1,d5

 label .loop2,2
	move.w	d2,d4
	subq.w	#1,d4

 label .loop3,3
	move.l	a4,a0
	bsr	rotate_mem
	addq.l	#2,a4
	subq.l	#2,d0
	ldbra	d4,.loop3,3
	subq.w	#2,d1
	ldbra	d5,.loop2,2
	ldbra	d7,.loop1,1

	move.l	(a7)+,a0
	bra	finish_up


to_standard_ip:			; Should have code for chunky modes (working on it)
	move.l	vwk_real_address(a0),a5
	move.l	wk_driver(a5),a5
	move.l	driver_device(a5),a5
	move.w	dev_format(a5),d6
	and.w	#2,d6
	beq	.not_chunky_ts_ip

	move.w	d0,d7
	mulu	d1,d7
	add.l	d7,d7			; d7 = pix/16 * height * 2 = words/plane
	moveq	#2,d6

	cmp.w	#16,d2
	bne	.not_16bit_ts_ip
	bsr	to_standard_16
	bra	end_vr_trnfm

.not_16bit_ts_ip:
	cmp.w	#8,d2
	bne	.not_8bit_ts_ip
	bsr	to_standard_8
	bra	end_vr_trnfm

.not_8bit_ts_ip:

	cmp.w	#32,d2
	bne	.not_32bit_ts_ip
	bsr	to_standard_32
	bra	end_vr_trnfm


.not_32bit_ts_ip:
.not_chunky_ts_ip:
	exg	d1,d2

	move.w	d0,d7
	move.w	d1,d6

	move.w	d1,d3
	mulu	d2,d3
	mulu	d0,d3
	add.l	d3,d3			; d3 = pix/16 * planes * height * 2 = total bytes

	add.w	d1,d1			; d1 = bytes per 16 pixel block

	move.l	d3,d0
	subq.l	#2,d0
	subq.w	#2,d1
	ext.l	d1
	addq.l	#2,a4

	move.l	a0,-(a7)

	subq.w	#1,d6

 label .loop1,1
	move.w	d7,d5
	subq.w	#1,d5

 label .loop2,2
	move.w	d2,d4
	subq.w	#1,d4

 label .loop3,3
	move.l	a4,a0
	bsr	rotate_mem
	addq.l	#2,a4
	subq.l	#2,d0
	ldbra	d4,.loop3,3
	ldbra	d5,.loop2,2
	subq.w	#2,d1
	ldbra	d6,.loop1,1

	move.l	(a7)+,a0
	bra	end_vr_trnfm


finish_up:
	move.l	vwk_real_address(a0),a4	; Should this really check driver?
	move.l	wk_driver(a4),a4
	move.l	driver_device(a4),a4
	move.w	dev_format(a4),d0
	and.w	#2,d0
	beq	.not_chunky

	move.w	mfdb_bitplanes(a1),d0
	cmp.w	#16,d0
	bne	.not_16bit
	move.l	mfdb_address(a2),a4
	move.l	a4,a3
	move.w	mfdb_height(a1),d0
	moveq	#32,d6
	moveq	#2,d7
	bsr	to_standard_16		; Is its own inverse!
	bra	end_vr_trnfm


.not_16bit:
	cmp.w	#8,d0
	bne	.not_8bit

	sub.w	#16,a7			; This is specialized for 8 bit!
	move.l	a7,a3
	move.l	mfdb_address(a2),a4
	move.w	mfdb_height(a1),d0

	subq.w	#1,d0

 label .loop0,0
	move.w	mfdb_wdwidth(a1),d1
	subq.w	#1,d1

 label .loop1,1
	movem.l	(a4),d2-d5
	movem.l	d2-d5,(a3)
	moveq	#15,d3

 label .loop2,2
	moveq	#14,d4

 label .loop3,3
	move.w	0(a3,d4),d5
	add.w	d5,d5
	addx.b	d6,d6
;	roxr.b	#1,d6		; This _is_ the right one!???
	move.w	d5,0(a3,d4)
	subq.w	#2,d4
	lbpl	.loop3,3
	move.b	d6,(a4)+

	ldbra	d3,.loop2,2
	ldbra	d1,.loop1,1
	ldbra	d0,.loop0,0

	add.w	#16,a7
	bra	end_vr_trnfm


.not_8bit:
	cmp.w	#32,d0
	bne	.not_32bit

	move.l	vwk_real_address(a0),a5
;	move.l	wk_driver(a5),a6
;	move.l	driver_device(a6),a6
;	lea	dev_vdi2pix(a6),a6
	lea	vdi_colours,a6

	move.l	vwk_palette(a0),d0
	bne	.local_palette
	move.l	wk_screen_palette_colours(a5),d0
.local_palette:
	move.l	d0,a5

	sub.w	#64,a7			; This is specialized for 32 bit!
	move.l	a7,a3
	move.l	mfdb_address(a2),a4
	move.w	mfdb_height(a1),d0

	subq.w	#1,d0

 label .loop0b,0
	move.w	mfdb_wdwidth(a1),d1
	subq.w	#1,d1

 label .loop1b,1
	movem.l	(a4),d2-d5
	movem.l	d2-d5,(a3)
	movem.l	16(a4),d2-d5
	movem.l	d2-d5,16(a3)
	movem.l	32(a4),d2-d5
	movem.l	d2-d5,32(a3)
	movem.l	48(a4),d2-d5
	movem.l	d2-d5,48(a3)
	moveq	#15,d3

 label .loop2b,2
	moveq	#62,d4

 label .loop3b,3
	move.w	0(a3,d4),d5
	add.w	d5,d5

	roxr.l	#1,d6		; This _is_ the right one!
	move.w	d5,0(a3,d4)
	subq.w	#2,d4
	lbpl	.loop3b,3

	move.l	d6,(a4)+

 label .skipb,4
	ldbra	d3,.loop2b,2
	ldbra	d1,.loop1b,1
	ldbra	d0,.loop0b,0

	add.w	#64,a7
	bra	end_vr_trnfm

.not_32bit:
.not_chunky:		

end_vr_trnfm:
	moveq	#1,d0
	tst.w	mfdb_standard(a1)
	beq	.was_not_standard
	moveq	#0,d0
.was_not_standard:
	move.w	d0,mfdb_standard(a2)
	movem.l	(a7)+,d2-d7/a3-a6
	used_d1
	rts


* rotate_mem - Support function
*              Rotates a memory area in a reasonably smart way
* Todo: Use stack buffer for small amounts
* In:   a0      Pointer to data
*       d0	Size
*       d1      Shift
rotate_mem:
	movem.l	d0-d3/a2-a3,-(a7)
	move.l	a0,a2
	cmp.l	d0,d1
	bge	.end_rotate
	tst.l	d1
	beq	.end_rotate
.rotate:
	move.l	a2,a3
	add.l	d1,a3
	move.l	d0,d2
	sub.l	d1,d2
	subq.l	#1,d2

 label .loop,1
	move.b	(a2),d3
	move.b	(a3),(a2)+
	move.b	d3,(a3)+
	ldbra	d2,.loop,1
	sub.l	#$10000,d2
	lbpl	.loop,1
	move.l	d0,d2
	move.l	d1,d0
	divu	d1,d2
	swap	d2
	ext.l	d2
	sub.l	d2,d1
;	bpl	.positive
;	neg.l	d1
;.positive:
	cmp.l	d0,d1
	blt	.rotate
.end_rotate:
	movem.l	(a7)+,d0-d3/a2-a3
	rts

* blit_0 - Internal function
*
* D1 = 0 (clear destination block)
* In:	a0	-> VDI struct, destination MFDB, VDI struct, source MFDB
*	d1	Source x (high), destination x (low)
*	d2	Source y (high), destination y (low)
*	a3	set pixel function
*	a4	get pixel function
blit_0:
	moveq	#0,d0
	jsr	(a3)
	rts

* blit_1 - Internal function
*
* D1 = S and D
blit_1:
	move.l	d7,-(a7)
	swap	d1
	swap	d2
	addq.l	#8,a0
	jsr	(a4)
	subq.l	#8,a0
	swap	d1
	swap	d2
	move.w	d0,d7
	jsr	(a4)
	and.w	d7,d0
	jsr	(a3)
	move.l	(a7)+,d7
	rts

* blit_2 - Internal function
*
* D1 = S and (not D)
blit_2:
	move.l	d7,-(a7)
	swap	d1
	swap	d2
	addq.l	#8,a0
	jsr	(a4)
	subq.l	#8,a0
	swap	d1
	swap	d2
	move.w	d0,d7
	jsr	(a4)
	not.w	d0
	and.w	d7,d0
	jsr	(a3)
	move.l	(a7)+,d7
	rts

* blit_3 - Internal function
*
* D1 = S (replace mode)
blit_3:
	swap	d1
	swap	d2
	addq.l	#8,a0
	jsr	(a4)
	subq.l	#8,a0
	swap	d1
	swap	d2
	jsr	(a3)
	rts

* blit_4 - Internal function
*
* D1 = (not S) and D (erase mode)
blit_4:
	move.l	d7,-(a7)
	swap	d1
	swap	d2
	addq.l	#8,a0
	jsr	(a4)
	subq.l	#8,a0
	swap	d1
	swap	d2
	move.w	d0,d7
	not.w	d7
	jsr	(a4)
	and.w	d7,d0
	jsr	(a3)
	move.l	(a7)+,d7
	rts

* blit_5 - Internal function
*
* D1 = D (no operation)
blit_5:
	rts

* blit_6 - Internal function
*
* D1 = S xor D (XOR mode)
blit_6:
	move.l	d7,-(a7)
	swap	d1
	swap	d2
	addq.l	#8,a0
	jsr	(a4)
	subq.l	#8,a0
	swap	d1
	swap	d2
	move.w	d0,d7
	jsr	(a4)
	eor.w	d7,d0
	jsr	(a3)
	move.l	(a7)+,d7
	rts

* blit_7 - Internal function
*
* D1 = S or D (transparent mode)
blit_7:
	move.l	d7,-(a7)
	swap	d1
	swap	d2
	addq.l	#8,a0
	jsr	(a4)
	subq.l	#8,a0
	swap	d1
	swap	d2
	move.w	d0,d7
	jsr	(a4)
	or.w	d7,d0
	jsr	(a3)
	move.l	(a7)+,d7
	rts

* blit_8 - Internal function
*
* D1 = not (S or D)
blit_8:
	move.l	d7,-(a7)
	swap	d1
	swap	d2
	addq.l	#8,a0
	jsr	(a4)
	subq.l	#8,a0
	swap	d1
	swap	d2
	move.w	d0,d7
	jsr	(a4)
	or.w	d7,d0
	not.w	d0
	jsr	(a3)
	move.l	(a7)+,d7
	rts

* blit_9 - Internal function
*
* D1 = not (S xor D)
blit_9:
	move.l	d7,-(a7)
	swap	d1
	swap	d2
	addq.l	#8,a0
	jsr	(a4)
	subq.l	#8,a0
	swap	d1
	swap	d2
	move.w	d0,d7
	jsr	(a4)
	eor.w	d7,d0
	not.w	d0
	jsr	(a3)
	move.l	(a7)+,d7
	rts

* blit_a - Internal function
*
* D1 = not D
blit_a:
	jsr	(a4)
	not.w	d0
	jsr	(a3)
	rts

* blit_b - Internal function
*
* D1 = S or (not D)
blit_b:
	move.l	d7,-(a7)
	swap	d1
	swap	d2
	addq.l	#8,a0
	jsr	(a4)
	subq.l	#8,a0
	swap	d1
	swap	d2
	move.w	d0,d7
	jsr	(a4)
	not.w	d0
	or.w	d7,d0
	jsr	(a3)
	move.l	(a7)+,d7
	rts

* blit_c - Internal function
*
* D1 = not S
blit_c:
	swap	d1
	swap	d2
	addq.l	#8,a0
	jsr	(a4)
	subq.l	#8,a0
	swap	d1
	swap	d2
	not.w	d0
	jsr	(a3)
	rts

* blit_d - Internal function
*
* D1 = (not S) or D (reverse transparent mode)
blit_d:
	move.l	d7,-(a7)
	swap	d1
	swap	d2
	addq.l	#8,a0
	jsr	(a4)
	subq.l	#8,a0
	swap	d1
	swap	d2
	move.w	d0,d7
	not.w	d7
	jsr	(a4)
	or.w	d7,d0
	jsr	(a3)
	move.l	(a7)+,d7
	rts

* blit_e - Internal function
*
* D1 = not (S and D)
blit_e:
	move.l	d7,-(a7)
	swap	d1
	swap	d2
	addq.l	#8,a0
	jsr	(a4)
	subq.l	#8,a0
	swap	d1
	swap	d2
	move.w	d0,d7
	jsr	(a4)
	and.w	d7,d0
	not.w	d0
	jsr	(a3)
	move.l	(a7)+,d7
	rts

* blit_f - Internal function
*
* D1 = 1 (fill destination block)
blit_f:
	moveq	#-1,d0
	jsr	(a3)
	rts

	data

vdi_colours:
	dc.b	0,2,3,6,4,7,5,8,9,10,11,14,12,15,13,255
