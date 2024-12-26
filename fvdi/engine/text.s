*****
* fVDI text drawing functions
*
* Copyright 1997-2003, Johan Klockars 
* This software is licensed under the GNU General Public License.
* Please, see LICENSE.TXT for further information.
*****

transparent	equ	1		; Fall through?

	.include	"vdi.inc"
	.include	"macros.inc"

	xref	clip_point,clip_line
	xref	tos_colour
	xref	line_types
	xref	_lib_vqt_extent,lib_vrt_cpyfm
	xref	asm_allocate_block,asm_free_block
	xref	text_area, _bt
	xref	_vdi_stack_top,_vdi_stack_size,_external_renderer

	xdef	v_gtext,v_ftext,v_justified

	xdef	lib_v_gtext

	xdef	_default_text

	xdef	_draw_text

	xdef	_bitmap_outline


	text

* v_gtext - Standard Trap function
* Todo: ?
* In:	a1	Parameter block
*	a0	VDI struct
v_gtext:
	sub.l	#10,a7
	move.l	intin(a1),a2
	move.l	a2,4(a7)	; String
	move.l	control(a1),a2
	move.w	6(a2),8(a7)	; Length
	beq	.v_gtext_end
	move.l	ptsin(a1),a2
	move.l	(a2),0(a7)	; x,y
	move.l	a7,a1
	bsr	lib_v_gtext
.v_gtext_end:
	add.l	#10,a7
	done_return

* lib_v_gtext - Standard Library function
* Todo: ?
* In:	a1	Parameters (x, y, string, length)
*	a0	VDI struct
lib_v_gtext:
	uses_d1
	move.l	a3,-(a7)
	moveq	#0,d1
	tst.w	vwk_text_alignment_horizontal(a0)
	beq	.left_justified

	movem.l	d2/a0-a2,-(a7)
	sub.w	#8*2,a7
	pea	(a7)
	move.l	4(a1),-(a7)
	move.w	8(a1),d0
	ext.l	d0
	move.l	d0,-(a7)
	move.l	a0,-(a7)
	jsr	_lib_vqt_extent
	move.w	16+4(a7),d1
;	add.w	#1,d1		; Was right coordinate, need width
	add.w	#16+16,a7
	movem.l	(a7)+,d2/a0-a2
	cmp.w	#2,vwk_text_alignment_horizontal(a0)
	beq	.right_justified
	lsr.w	#1,d1 
.right_justified:
	neg.w	d1
	swap	d1
	clr.w	d1
.left_justified:

	add.l	0(a1),d1
	move.w	8(a1),d0
	move.l	4(a1),a1
	sub.l	a2,a2		; No special offset table
	move.l	vwk_real_address(a0),a3
	move.l	wk_r_text(a3),a3
	jsr	(a3)

	move.l	(a7)+,a3
	used_d1
	rts


* Currently not capable of dealing with colour at 20(a1)
* due to driver limitations.
_draw_text:
	movem.l	a2-a3,-(a7)

	lea	4+2*4(a7),a1
	move.l	0(a1),a0
	move.w	2+4(a1),d1
	swap	d1
	move.w	2+8(a1),d1
	move.l	16(a1),d0
	move.l	12(a1),a1
	sub.l	a2,a2		; No special offset table
	move.l	vwk_real_address(a0),a3
	move.l	wk_r_text(a3),a3
	jsr	(a3)

	movem.l	(a7)+,a2-a3
	rts


* v_ftext - Standard Trap function
* Todo: ?
* In:	a1	Parameter block
*	a0	VDI struct
v_ftext:
	sub.l	#14,a7
	move.l	intin(a1),a2
	move.l	a2,4(a7)	; String
	move.l	control(a1),a2
	move.w	2(a2),d0
	move.w	6(a2),8(a7)	; Length
	beq	.v_ftext_end
	move.l	ptsin(a1),a2
	move.l	(a2)+,0(a7)	; x,y
	move.l	a2,10(a7)
	move.l	a7,a1
	cmp.w	#1,d0
	beq	.no_offset
	bsr	lib_v_ftext_offset
.v_ftext_end:
	add.l	#14,a7
	done_return

.no_offset:
	bsr	lib_v_gtext
	bra	.v_ftext_end

* lib_v_ftext_offset - Standard Library function
* Todo: ?
* In:	a1	Parameters (x, y, string, length, offset_table)
*	a0	VDI struct
lib_v_ftext_offset:
	uses_d1
	move.l	a3,-(a7)
	moveq	#0,d1
	tst.w	vwk_text_alignment_horizontal(a0)
	beq	.left_justified_f

	movem.l	d2-d6/a2-a4,-(a7)

	move.w	8(a1),d0		; Number of characters
	move.l	4(a1),a4
	move.l	10(a1),a2
	move.l	vwk_text_current_font(a0),a3
	move.w	font_code_low(a3),d3
	move.w	font_code_high(a3),d4
	move.l	font_table_character(a3),a3

	moveq	#0,d2			; Width total
	moveq	#0,d5			; max width
	lbra	.no_char,2
 label .loop,1
	move.w	(a4)+,d1
	sub.w	d3,d1			; Negative numbers are higher
	cmp.w	d4,d1			;  than code_high
	lbhi	.no_char,2
	add.w	d1,d1
	move.w	2(a3,d1.w),d6
	sub.w	0(a3,d1.w),d6
	add.w	(a2),d2
	add.w	d6,d2
	cmp.w	d2,d5
	lbhi	.no_char,2
	move.w	d2,d5			; Remember widest
 label .no_char,2
	addq.l	#4,a2
	ldbra	d0,.loop,1

	movem.l	(a7)+,d2-d6/a2-a4

	move.w	d2,d1
	cmp.w	#2,vwk_text_alignment_horizontal(a0)
	beq	.right_justified_f
	lsr.w	#1,d1 
.right_justified_f:
	neg.w	d1
	swap	d1
	clr.w	d1
.left_justified_f:

	add.l	0(a1),d1
	move.w	8(a1),d0
	move.l	10(a1),a2	; Offset table
	move.l	4(a1),a1
	move.l	vwk_real_address(a0),a3
	move.l	wk_r_text(a3),a3
	jsr	(a3)

	move.l	(a7)+,a3
	used_d1
	rts


* v_justified - Standard Trap function
* Todo: Actually do justification
* In:	a1	Parameter block
*	a0	VDI struct
v_justified:
	sub.l	#16,a7
	move.l	intin(a1),a2
	move.l	(a2)+,12(a7)	; Flags
	move.l	a2,4(a7)	; String
	move.l	control(a1),a2
	move.w	6(a2),d0
	subq.w	#2,d0
	move.w	d0,8(a7)	; Length
	beq	.v_justified_end
	move.l	ptsin(a1),a2
	move.l	(a2)+,0(a7)	; x,y
	move.w	(a2),10(a7)	; x size
	move.l	a7,a1
	tst.l	12(a7)
	beq	.no_justification
	bsr	lib_v_justified
.v_justified_end:	; .end:
	add.l	#16,a7
	done_return

.no_justification:
	bsr	lib_v_gtext
	bra	.v_justified_end	; .end

* lib_v_justified - Standard Library function
* Todo: ?
* In:	a1	Parameters (x, y, string, length, x_size, word_space, char_space)
*	a0	VDI struct
lib_v_justified:
	uses_d1
	movem.l	d2-d6/a3-a4,-(a7)

	move.l	#4*1024,-(a7)
	bsr	asm_allocate_block
	addq.l	#4,a7
	tst.l	d0
	beq	.lib_v_justified_end
	move.l	d0,a2
	move.l	a2,-(a7)

	move.w	8(a1),d0		; Number of characters
	move.l	4(a1),a4		; String
	move.l	vwk_text_current_font(a0),a3
	move.w	font_code_low(a3),d3
	move.w	font_code_high(a3),d4
	move.l	font_table_character(a3),a3

	clr.l	(a2)+			; First character at 0,0
	moveq	#0,d2			; Width total
	moveq	#0,d5			; Spaces
	lbra	.no_char,2
 label .loop,1
	move.w	(a4)+,d1
	sub.w	d3,d1			; Negative numbers are higher
	cmp.w	d4,d1			;  than code_high
	lbhi	.no_char,2
	cmp.b	#' ',d1
	lbne	.skip,3
	addq.w	#1,d5
 label .skip,3
	add.w	d1,d1
	move.w	2(a3,d1.w),d6
	sub.w	0(a3,d1.w),d6
	add.w	d6,d2
	move.w	d6,(a2)+
	move.w	#0,(a2)+
 label .no_char,2
	ldbra	d0,.loop,1

	move.w	10(a1),d6
	sub.w	d2,d6
	beq	.updated
	ext.l	d6			; d6.l = amount to widen by (could be negative)

	move.w	8(a1),d4
	sub.w	d5,d4
	tst.w	12(a1)			; Any word spacing?
	beq	.spacing_done
	move.w	d5,d4
	tst.w	14(a1)
	beq	.spacing_done		; Any character spacing?
	move.w	8(a1),d4
.spacing_done:
	subq.w	#1,d4			; d4.w = number of positions to vary spacing at
	beq	.updated

	divs	d4,d6			; Ex 50 / 8 = 6 + 2/8	0/6/12/19/25/31/37/44/50
	move.l	d6,d2			;			2 4 6  80 2  4  6  80
	swap	d2
	ext.l	d2
	bpl	.positive
	neg.w	d2
.positive:
	moveq	#0,d3

	move.l	(a7),a2
	clr.l	(a2)+			; First character at 0,0
	move.w	8(a1),d0
	subq.w	#1,d0			; Skip first character
	move.l	4(a1),a4
	addq.l	#2,a4
	bra	.modify_loop_end
.modify_loop:
	move.w	(a2),d5
	move.w	(a4)+,d1
	cmp.b	#' ',d1
	bne	.not_space
	tst.w	14(a1)
	bra	.modify
.not_space:
	tst.w	12(a1)
.modify:
	beq	.modify_done
	add.w	d6,d5
.modify_done:
	add.w	d2,d3
	cmp.w	d4,d3
	blo	.x_done
	sub.w	d4,d3
	addq.w	#1,d5
	tst.l	d2
	bpl	.x_done
	subq.w	#2,d5
.x_done:
	move.w	d5,(a2)
	addq.l	#4,a2
.modify_loop_end:
	dbra	d0,.modify_loop

.updated:
	moveq	#0,d1
	tst.w	vwk_text_alignment_horizontal(a0)
	beq	.left_justified_j

	move.w	10(a1),d1
	cmp.w	#2,vwk_text_alignment_horizontal(a0)
	beq	.right_justified_j
	lsr.w	#1,d1 
.right_justified_j:
	neg.w	d1
	swap	d1
	clr.w	d1
.left_justified_j:

	add.l	0(a1),d1
	move.w	8(a1),d0
	move.l	4(a1),a1
	move.l	(a7),a2		; The character offset table
	move.l	vwk_real_address(a0),a3
	move.l	wk_r_text(a3),a3
	jsr	(a3)

	bsr	asm_free_block	; Address still on stack
	addq.l	#4,a7

.lib_v_justified_end:		; .end:
	movem.l	(a7)+,d2-d6/a3-a4
	used_d1
	rts


* _default_text - Buffer rendering or individual character mono-expand text routine
* Todo:	Add effects of effects to the size
* In:	a0	VDI struct
*	a1	Pointer to string
*	a2	Pointer to character offset table or zero
*	d0	String length
*	d1	Coordinates
* Call:	a0	VDI struct
*	a1	Parameters for lib_vrt_cpyfm
_default_text:
	movem.l	d3-d7/a3-a6,-(a7)

* Some other method should be used for this!
	move.l	vwk_text_current_font(a0),a3
	tst.w	font_flags(a3)
	bmi	.external_renderer

	moveq	#0,d4		; Offset extension of length
	move.l	a2,d3
	beq	.no_offsets
	move.l	a2,a3
	move.w	d0,d3
	subq.w	#1,d3
.test_offsets:
	move.l	(a3)+,d5
	beq	.no_offset_here
	tst.w	d5
	bne	.single_char
	swap	d5
	add.w	d5,d4
.no_offset_here:
	dbra	d3,.test_offsets
.no_offsets:

	move.l	d0,a3
	move.l	#0,-(a7)			; Get a memory block of any size (hopefully large)
	bsr	asm_allocate_block
	addq.l	#4,a7
	tst.l	d0
	exg	d0,a3
	beq	.single_char
	move.l	a3,-(a7)			; For free_block below

	move.l	vwk_text_current_font(a0),a5	; Font structure
	move.w	font_height(a5),d5
	swap	d5

	btst	#3,font_flags(a5)		; Proportional?
	bne	.nonproportional

	movem.l	d0-d2/d4/a1/a3,-(a7)
	move.l	font_table_character(a5),a3
	move.w	font_code_low(a5),d2
	move.w	font_code_high(a5),d4

	moveq	#0,d3			; Width total
;	subq.w	#1,d0
	lbra	.no_char,2
 label .size_loop,1
	move.w	(a1)+,d1
	sub.w	d2,d1			; Negative numbers are higher
	cmp.w	d4,d1			;  than code_high
	lbhi	.no_char,2
	add.w	d1,d1
	add.w	2(a3,d1.w),d3
	sub.w	0(a3,d1.w),d3
 label .no_char,2
	ldbra	d0,.size_loop,1

	movem.l	(a7)+,d0-d2/d4/a1/a3
	bra	.width_done

* It is simply assumed that the external renderer is successful.
.external_renderer:
	move.l	_external_renderer,d2		; (Handle differently?)
	beq	.no_external_renderer		; Not really allowed
	move.l	d2,a3
	
	move.l	a7,d2				; Give external renderer
	move.l	_vdi_stack_top,a7		;  extra stack space!
	move.l	d2,-(a7)			; (Should be improved)

	move.l	_vdi_stack_size,-(a7)	
	move.l	a2,-(a7)			; Character offset table
	ext.l	d0
	move.l	d0,-(a7)			; String len
	move.l	a1,-(a7)			; String pointer
	move.l	d1,-(a7)			; Coordinates (x << 16 | y)
	move.l	a0,-(a7)			; VDI struct
	jsr	(a3)
	add.w	#6*4,a7

	move.l	(a7),a7			; Return to original stack

.no_external_renderer:
	movem.l	(a7)+,d3-d7/a3-a6
	rts

.nonproportional:
	move.w	font_widest_cell(a5),d3
;	btst	#4,vwk_text_effects(a0)		; Outline?
;	beq	1$
;	addq.w	#2,d3
;1$:
	mulu	d0,d3

;	btst	#2,vwk_text_effects(a0)		; Italic?
;	beq	2$
;	add.w	font_offset_left(a5),d3
;	add.w	font_offset_right(a5),d3
;2$:
;	bra	.width_done

.width_done:

	tst.w	d4
	beq	.keep_width
	move.w	d4,d3
	add.w	font_widest_cell(a5),d3
.keep_width:
	move.w	d3,d5

	add.w	#15,d5
	lsr.w	#4,d5
	addq.w	#1,d5
	and.w	#$fffe,d5	; Even number of words wide
	add.w	d5,d5		; d5 - height, bytes wide

	movem.l	d1/d3,-(a7)
	movem.l	d5/a0/a3/a5,-(a7)

	tst.w	d4
	beq	.no_clear
	move.l	d5,d1
	swap	d1
	mulu	d5,d1
	lsr.w	#4,d1
	moveq	#0,d3
	moveq	#0,d4
	moveq	#0,d5
	moveq	#0,d6
 label .loop,1
	movem.l	d3-d6,(a3)
	add.w	#16,a3
	ldbra	d1,.loop,1
	movem.l	(a7),d5/a0/a3/a5
.no_clear:

	move.l	a1,a4				; String address
	sub.l	a1,a1				; Clip rectangle
	moveq	#5,d6				; Fake top line alignment
	moveq	#0,d3				; Coordinates
	moveq	#0,d4
	bsr	text_area
	move.l	4(a7),a0
	move.w	vwk_text_effects(a0),d0

	btst	#0,d0
	beq	.no_bold	
	movem.l	(a7),d5/a0/a3/a5
	move.l	a3,a0
	move.l	a3,a1
	move.w	d5,a6
	move.w	d5,d3
	lsr.w	#1,d3
	swap	d5
	move.w	d5,d7
	move.w	font_thickening(a5),d4
;	moveq	#1,d4
	bsr	bold
	move.l	4(a7),a0
	move.w	vwk_text_effects(a0),d0
.no_bold:

	btst	#3,d0
	beq	.no_underline
	movem.l	(a7),d5/a0/a3/a5
	move.w	d5,d0
	move.w	font_distance_top(a5),d1
	addq.w	#1,d1
	mulu	d0,d1
	add.l	d1,a3
	lsr.w	#2,d0
	subq.w	#1,d0
	move.w	font_underline(a5),d1
	subq.w	#1,d1
	moveq	#-1,d3
 label .loop1,1
	move.l	a3,a0
	move.w	d0,d2
 label .loop2,2
	move.l	d3,(a0)+
	ldbra	d2,.loop2,2
	add.w	d5,a3
	ldbra	d1,.loop1,1
	move.l	4(a7),a0
	move.w	vwk_text_effects(a0),d0
.no_underline:

	btst	#2,d0
	beq	.no_italic
	movem.l	(a7),d5/a0/a3/a5
	move.w	d5,d0
	move.w	font_distance_top(a5),d1
	add.w	font_distance_bottom(a5),d1
	addq.w	#1,d1
	mulu	d0,d1
	add.l	d1,a3
	lsr.w	#1,d0
	subq.w	#2,d0
	move.l	d5,d1
	swap	d1
	subq.w	#1,d1
	move.w	font_skewing(a5),d3
	moveq	#0,d4
 label .loop1b,1
	move.l	a3,a0
	move.w	d0,d2
 label .loop2b,2
	move.l	-4(a0),d6
	lsr.l	d4,d6
	move.w	d6,-(a0)
	ldbra	d2,.loop2b,2
	moveq	#0,d6
	move.w	-(a0),d6
	lsr.l	d4,d6
	move.w	d6,(a0)
	sub.w	d5,a3
	rol.w	#1,d3
	lbcc	.skip,3
	addq.w	#1,d4
 label .skip,3
	ldbra	d1,.loop1b,1
	move.l	4(a7),a0
	move.w	vwk_text_effects(a0),d0
.no_italic:

	btst	#4,d0
	beq	.no_outline
	movem.l	(a7),d5/a0/a3/a5
	move.l	a3,a0
	move.l	a3,a1
	move.w	d5,a6
	move.w	d5,d3
	swap	d5
	move.w	d5,d7
	mulu	d3,d5
	add.l	d5,a1
	move.l	a1,8(a7)		; New bitmap address
	lsr.w	#1,d3
	bsr	outline
	move.l	4(a7),a0
	move.w	vwk_text_effects(a0),d0
.no_outline:

	btst	#1,d0
	beq	.no_light
	movem.l	(a7),d5/a0/a3/a5
	move.w	d5,d0
	lsr.w	#2,d0
	subq.w	#1,d0
	move.l	d5,d1
	swap	d1
	subq.w	#1,d1
	move.w	font_lightening(a5),d2
	move.w	d2,d3
	swap	d3
	move.w	d2,d3
 label .loop1c,1
	move.l	a3,a0
	move.w	d0,d2
 label .loop2c,2
	and.l	d3,(a0)+
	ldbra	d2,.loop2c,2
	add.w	d5,a3
	rol.l	#1,d3
	ldbra	d1,.loop1c,1
.no_light:

	movem.l	(a7)+,d5/a0/a3/a5
	movem.l	(a7)+,d1/d3

	sub.l	#4,a7			; Create pens
;	move.w	vwk_text_colour(a0),0(a7)	; Foreground
;	move.w	#0,2(a7)		; Background
	move.l	vwk_text_colour(a0),d0
	swap	d0
	move.l	d0,0(a7)

	sub.l	#20,a7			; Create source MFDB for rendering buffer
	move.l	a3,mfdb_address(a7)	; Start address of buffer
	move.w	d5,d0
	lsr.w	#1,d0
	move.w	d0,mfdb_wdwidth(a7)	; Width in words
	lsl.w	#4,d0
	move.w	d0,mfdb_width(a7)	; Width in pixels
	swap	d5
	move.w	d5,mfdb_height(a7)	; Height in pixels
	move.w	#1,mfdb_standard(a7)	; Standard format
	move.w	#1,mfdb_bitplanes(a7)	; Monochrome

	sub.l	#16,a7			; Create points
	move.l	#0,0(a7)		; Source left, top
	subq.w	#1,d3
	move.w	d3,4(a7)		; Source right
	subq.w	#1,d5
	move.w	d5,6(a7)		; Source bottom

;	move.l	d1,d3

	move.l	vwk_text_current_font(a0),a5	; Font structure
	move.w	vwk_text_alignment_vertical(a0),d4
	add.w	d4,d4
	add.w	font_extra_distance(a5,d4.w),d1
	move.l	d1,8(a7)		; Destination left,top
	add.w	d5,d1
	move.w	d1,14(a7)		; Destination bottom
	swap	d1
	add.w	d3,d1
	move.w	d1,12(a7)		; Destination right

	sub.l	#18,a7			; Create parameters
	move.w	vwk_mode(a0),0(a7)	; Mode
	lea	18(a7),a3
	move.l	a3,2(a7)		; Points
	lea	16(a3),a3
	move.l	a3,6(a7)		; Source MFDB
	move.l	#0,10(a7)		; Screen as destination
	lea	20(a3),a3
	move.l	a3,14(a7)		; Pens

	move.l	a7,a1
	bsr	lib_vrt_cpyfm

	add.l	#18+16+20+4,a7

	bsr	asm_free_block
	addq.l	#4,a7

	movem.l	(a7)+,d3-d7/a3-a6
	rts


.single_char:
;	movem.l	d3-d7/a3-a6,-(a7)
	sub.l	#4,a7			; Create pens
;	move.w	vwk_text_colour(a0),0(a7)	; Foreground
;	move.w	#0,2(a7)		; Background
	move.l	vwk_text_colour(a0),d5
	swap	d5
	move.l	d5,0(a7)

	sub.l	#20,a7			; Create source MFDB for font
	move.l	vwk_text_current_font(a0),a5	; Font structure
	move.l	font_data(a5),d5
	move.l	d5,mfdb_address(a7)	; Start address of font
	move.w	font_width(a5),d5
	lsr.w	#1,d5
	move.w	d5,mfdb_wdwidth(a7)	; Width in words
	lsl.w	#4,d5
	move.w	d5,mfdb_width(a7)	; Width in pixels
	move.w	font_height(a5),mfdb_height(a7)	; Height in pixels
	move.w	#1,mfdb_standard(a7)	; Standard format
	move.w	#1,mfdb_bitplanes(a7)	; Monochrome

	sub.l	#16,a7			; Create points
	move.w	#0,2(a7)		; Source top
	move.w	font_height(a5),d5
	subq.w	#1,d5
	move.w	d5,6(a7)		; Source bottom

	move.l	d1,d3

	move.w	vwk_text_alignment_vertical(a0),d4
	add.w	d4,d4
	add.w	font_extra_distance(a5,d4.w),d3
	move.l	d3,8(a7)		; Destination left,top
	add.w	d5,d3
	move.w	d3,14(a7)		; Destination bottom

	swap	d3
	subq.w	#1,d3			; Previous character's right edge,
	move.w	d3,12(a7)		;   current width is added below

	sub.l	#18,a7			; Create parameters
	move.w	vwk_mode(a0),0(a7)	; Mode
	lea	18(a7),a3
	move.l	a3,2(a7)		; Points
	lea	16(a3),a3
	move.l	a3,6(a7)		; Source MFDB
	move.l	#0,10(a7)		; Screen as destination
	lea	20(a3),a3
	move.l	a3,14(a7)		; Pens

	move.l	a1,a3			; String
	move.w	d0,d3			; Length
	move.w	font_code_low(a5),d1	; First character

	move.l	font_table_character(a5),a5	; Character offset table

	move.l	a2,d0
	bne	.loopend_offset
	bra	.loopend

.loop_char:			; .loop:
	move.w	(a3)+,d4	; Get character
	sub.w	d1,d4
	add.w	d4,d4
	move.w	0(a5,d4.w),d0
	move.w	d0,0+18(a7)	; Character source left coordinate
	move.w	2(a5,d4.w),d5
	move.w	d5,4+18(a7)	; Character source right coordinate
	sub.w	d0,d5		; Character width
	add.w	d5,12+18(a7)	; Character destination right coordinate
	
	move.l	a7,a1
	bsr	lib_vrt_cpyfm

	add.w	d5,8+18(a7)	; Increase destination x coordinate

.loopend:
	dbra	d3,.loop_char

.default_text_end:
	add.l	#18+16+20+4,a7
	movem.l	(a7)+,d3-d7/a3-a6
	rts

.loop_offset:			; .loop:
	move.w	(a3)+,d4	; Get character
	sub.w	d1,d4
	add.w	d4,d4
	move.w	0(a5,d4.w),d0
	move.w	d0,0+18(a7)	; Character source left coordinate
	move.w	2(a5,d4.w),d5
	move.w	d5,4+18(a7)	; Character source right coordinate
	sub.w	d0,d5		; Character width
	add.w	8+18(a7),d5
	subq.w	#1,d5
	move.w	d5,12+18(a7)	; Character destination right coordinate
	
	move.l	a7,a1
	move.l	a2,-(a7)
	bsr	lib_vrt_cpyfm
	move.l	(a7)+,a2

.loopend_offset:
	move.w	(a2)+,d4
	add.w	d4,8+18(a7)	; Increase destination x/y coordinates
	move.w	(a2)+,d4
	add.w	d4,10+18(a7)

	dbra	d3,.loop_offset
	bra	.default_text_end


* In:	a0	area1
*	a1	area2
*	a6	wrap
*	d4	fattening
*	d3	words
*	d7	lines
bold:
	move.w	d3,d0
	add.w	d0,d0
	sub.w	d0,a6
	subq.w	#1,d4
	subq.w	#1,d3
	subq.w	#1,d7
.lines_b:
	moveq	 #0,d1
	move.w	d3,d6
.words_b:
	move.w	(a0)+,d0
	move.w	d0,d1
	swap	d0
	move.w	d1,d0		; Remember in top word
	move.w	d4,d5
.shifts_b:
	lsr.l	#1,d1
	or.w	d1,d0
	dbra	d5,.shifts_b
	move.w	d0,(a1)+
	move.l	d0,d1		; Restore top word
	dbra	d6,.words_b
	add.l	a6,a0
	add.l	a6,a1
	dbra	d7,.lines_b
	rts


_bitmap_outline:
	move.l	a5,-(a7)
	move.l	a7,a5		; frame pointer
	movem.l	d1-d7/a2-a4/a6,-(a7)

	move.l	8+0(a5),a0	; src
	move.l	8+4(a5),a1	; dst
	move.l	8+8(a5),a6	; wrap (pitch)
	move.l	8+12(a5),d3	; wdwidth
	move.l	8+16(a5),d7	; lines

	bsr	outline
	movem.l	(a7)+,d1-d7/a2-a4/a6
	move.l	(a7)+,a5
	rts


* In:	a0	area1
*	a1	area2
*	a6	wrap
*	d3	words
*	d7	lines
outline:
* Do <-x-> expansion rather than bold(2) (x->->). Easier EOR stage.
	move.l	a6,a3
	sub.w	d3,a3
	sub.w	d3,a3
	subq.w	#1,d7
	move.w	d7,d0
	swap	d7
	move.w	d0,d7
	subq.w	#1,d3
	move.l	a0,a4
	move.l	a1,a5
.lines_o1:
	move.w	d3,d6
	moveq	#0,d1
	move.w	(a0)+,d1
	move.w	d1,d2
.words_o1:
	move.w	d1,d0
	swap	d2
	move.w	(a0)+,d2
	lsr.l	#1,d1
	or.w	d1,d0
	move.l	d2,d1
	add.l	d1,d1
	swap	d1
	or.w	d1,d0
	move.l	d2,d1
	move.w	d0,(a1)+
	dbra	d6,.words_o1
	add.l	a3,a0
	subq.l	#2,a0
	add.l	a3,a1
	dbra	d7,.lines_o1

* Optimization to think about:  combine EOR into vertical bold.

	swap	d7
	addq.w	#2,d3
	and.w	#$fffe,d3
	lsr.w	#1,d3
	subq.w	#1,d3
.longs_o2:
	move.l	a4,a1
	move.l	a5,a0
	move.w	d7,d6
	moveq	#0,d0
	move.l	(a0),d1
	add.l	a6,a0
.lines_o2:
	move.l	(a0),d2
	or.l	d1,d0
	or.l	d2,d0
	move.l	(a1),d5
	eor.l	d5,d0
	move.l	d0,(a0)
	move.l	d1,d0
	move.l	d2,d1	
	add.l	a6,a0
	add.l	a6,a1
	dbra	d6,.lines_o2
	or.l	d1,d0
	move.l	d0,(a0)
	addq.l	#4,a4
	addq.l	#4,a5
	dbra	d3,.longs_o2
	rts

dscale:
	moveq	#15,d7
.not_finished_d:
	move.w	(a0)+,d0
	moveq	 #15,d5
.no_fetch_d:
	add.w	d1,d2
	bvc	.no_write_d
.put_d:
	add.w	d0,d0
	addx.w	d3,d3
	dbra	d7,.no_write_d1
	move.w	d3,(a1)+
	moveq	#15,d7
.no_write_d1:
	dbra	d5,.no_fetch_d
	dbra	d4,.not_finished_d
	bra	.finished_d
.no_write_d:
	add.w	d0,d0
	dbra	d5,.no_fetch_d
	dbra	d4,.not_finished_d
.finished_d:
	cmp.w	#15,d7
	bne	.done_d
	lsl.w	d7,d3
	move.w	d3,(a1)+
.done_d:
	rts

uscale:
	moveq	#15,d7
.not_finished_u:
	move.w	(a0)+,d0
	moveq	#15,d5
.no_fetch_u:
	add.w	d0,d0
	scs	d6
.put_u:
	add.w	d3,d3
	sub.b	d6,d3
	dbra	d7,.no_write_u
	move.w	d3,(a1)+
	moveq	#15,d7
.no_write_u:
	add.w	d1,d2
	bvc	.put_u
	dbra	d5,.no_fetch_u
	dbra	d4,.not_finished_u
	bra	.finished_u
.finished_u:
	cmp.w	#15,d7
	bne	.done_u
	lsl.w	d7,d3
	move.w	d3,(a1)+
.done_u:
	rts

	end
