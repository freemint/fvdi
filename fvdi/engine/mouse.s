*****
* fVDI mouse functions
*
* Copyright 1997-2000, Johan Klockars
* This software is licensed under the GNU General Public License.
* Please, see LICENSE.TXT for further information.
*****

transparent	equ	1		; Fall through?

mouse_size	equ	4		; Mostly for testing
show_delay	equ	1		; 5

etv_timer   equ 0x400
vbl_queue   equ 0x456
timer_ms    equ 0x442

mouse_interval	equ	1		; Interval between mouse updates

	.include	"vdi.inc"
	.include	"macros.inc"
	.include	"linea.inc"

	xref	redirect
	xref	_stand_alone
	xref	_malloc,_free
	xref	_screen_wk

	xdef	vsc_form,v_show_c,v_hide_c
	xdef	vq_mouse,vq_key_s,vrq_string
	xdef	vex_butv,vex_motv,vex_curv,vex_wheelv,vex_timv
	xdef	_user_cur
	xdef	_do_nothing,_vector_call
	xref	_linea_vars
	xdef    _init_interrupts
	xdef    _reset_interrupts

	xdef	lib_vsc_form,lib_v_show_c,lib_v_hide_c


	text

* vsc_form - Standard Trap function
* Todo: Redraw mouse through vector
* In:   a1      Parameter block
*       a0      VDI struct
vsc_form:
	move.l	a1,-(a7)
	move.l	intin(a1),a1
	bsr	lib_vsc_form
	move.l	(a7)+,a1
	move.l	vwk_real_address(a0),a2		; If no mouse type, the original VDI is called too
	tst.w	wk_mouse_type(a2)
	beq     redirect
	done_return

* lib_vsc_form - Standard Library function
* Todo: ?
* In:	a1	Parameters   lib_vsc_form(pt_data)
*	a0	VDI struct
lib_vsc_form:
	movem.l	a0-a1,-(a7)
	move.l	a1,a2
	move.l	vwk_real_address(a0),a1		; a0 no longer -> VDI
	move.l  _linea_vars,a0
	addq.b  #1,MOUSE_FLAG(a0)           ; disable mouse drawing
	bsr     transform_mouse
	move.w  sr,d0
	ori.w   #0x0700,sr
	move.l  GCURX(a0),CUR_X(a0)
	clr.b   CUR_FLAG(a0)
	move.w  d0,sr
	subq.b  #1,MOUSE_FLAG(a0)
	movem.l	(a7)+,a0-a1
	rts


*
* trasnform mouse shape
* Todo: Redraw mouse through vector
* In:   a2      mouse definition block
*       a1      workstation struct
*       a0      linea variables
transform_mouse:
	move.l	(a2)+,wk_mouse_hotspot(a1)	; X and y coodinates
	addq.l	#2,a2                       ; skip planes (FIXME: does not support multi-pattern mouse yet)
	move.l	(a2)+,wk_mouse_colour(a1)	; Mask and data colours

	move.l	a1,-(a7)
	lea	wk_mouse_mask(a1),a1
	moveq	#15,d0				; Mask and data rows
 label .loop,1
	move.l	(a2)+,(a1)+
	ldbra	d0,.loop,1
	move.l	(a7)+,a1

	move.l	wk_r_mouse(a1),a2
	move.l  d1,-(a7)
	move.l	d2,-(a7)
	move.l	a1,d2
	add.l	#wk_mouse,d2			; Change
	move.w	mouse_op,d0
	swap	d0
	move.l	a0,-(a7)
	move.w	GCURX(a0),d0
	move.w	GCURY(a0),d1
	jsr	(a2)
	move.w	d0,mouse_op		; What to try again
	swap	d0			;  and in how long
	move.w	d0,pointer_delay
	move.l	(a7)+,a0
	move.l	(a7)+,d2
	move.l	(a7)+,d1
 label .done,2
	rts

* v_show_c - Standard Trap function
* Todo: Redraw mouse through vector when necessary
* In:   a1      Parameter block
*       a0      VDI struct
v_show_c:
	move.l	intin(a1),a2
	move.l	a1,-(a7)
	move.w	(a2),-(a7)
	move.l	a7,a1
	bsr	lib_v_show_c
	addq.l	#2,a7
	move.l	(a7)+,a1
	move.l	vwk_real_address(a0),a2		; If no mouse type, the original VDI is called too
	tst.w	wk_mouse_type(a2)
	beq     redirect
	done_return

* lib_v_show_c - Standard Library function
* Todo: ?
* In:	a1	Parameters   lib_v_show_c(reset)
*	a0	VDI struct
lib_v_show_c:
	move.w	(a1),d0				; Always show?
	move.l	vwk_real_address(a0),a1
	move.l  a0,-(a7)
	move.l	_linea_vars,a0
	tst.w	d0
	lbeq	.set_,2
	move.w	M_HID_CNT(a0),d0
	lbeq	.end,1				; Already shown?
	subq.w	#1,d0
 label .set_,2
	move.w	d0,M_HID_CNT(a0)
	lbhi	.end,1				; Still not shown?

	tst.w	wk_mouse_type(a1)		; If no mouse type, leave to old VDI
	lbeq	.end,1


	move.l	wk_r_mouse(a1),a2
	move.l	d1,-(a7)
	move.l	d2,-(a7)
	move.w  sr,d2
	ori.w   #0x0700,sr               ; disable interrupts
	clr.b   CUR_FLAG(a0)
	move.w	mouse_op,d0
	swap	d0
	move.w	GCURX(a0),d0
	move.w	GCURY(a0),d1
	move.w  d2,sr                    ; restore sr
	moveq	#3,d2				; Show
	jsr	(a2)
	move.w	d0,mouse_op
	swap	d0
	move.w	d0,pointer_delay
	move.l	(a7)+,d2
	move.l	(a7)+,d1

 label .end,1
	move.l  (a7)+,a0
	rts

* v_hide_c - Standard Trap function
* Todo: Remove mouse through vector when necessary
* In:   a1      Parameter block
*       a0      VDI struct
v_hide_c:
	move.l	a1,-(a7)
	bsr	lib_v_hide_c
	move.l	(a7)+,a1
	move.l	vwk_real_address(a0),a2
	tst.w	wk_mouse_type(a2)		; If no mouse type, call the old VDI too
	beq     redirect
	done_return

* lib_v_hide_c - Standard Library function
* Todo: ?
* In:	a1	Parameters   lib_v_hide_c()
*	a0	VDI struct
lib_v_hide_c:
	move.l	vwk_real_address(a0),a1
	move.l  a0,-(a7)
	move.l	_linea_vars,a0
	move.w	M_HID_CNT(a0),d0
	addq.w	#1,d0
	move.w	d0,M_HID_CNT(a0)

	cmp.w	#1,d0				; Already hidden?
	lbhi	.not_shown,1

	tst.w	wk_mouse_type(a1)		; If no mouse type, leave to old VDI
	lbeq	.not_shown,1

	move.l	wk_r_mouse(a1),a2
	move.l	d1,-(a7)
	move.l	d2,-(a7)
	move.w	mouse_op,d0
	swap	d0
	move.w	GCURX(a0),d0
	move.w	GCURY(a0),d1
	moveq	#2,d2				; Hide
	jsr	(a2)
	move.w	d0,mouse_op
	swap	d0
	move.w	d0,pointer_delay
	move.l	(a7)+,d2
	move.l	(a7)+,d1

 label .not_shown,1
	move.l	(a7)+,a0
	rts

* vq_mouse - Standard Trap function
* Todo:
* In:   a1      Parameter block
*       a0      VDI struct
vq_mouse:
	move.w  sr,d0
	ori.w   #0x0700,sr
	move.l  _linea_vars,a0
	move.l	ptsout(a1),a2
	move.l  GCURX(a0),(a2)
	move.l	intout(a1),a2
	move.w  MOUSE_BT(a0),(a2)
	move.w  d0,sr
	done_return


* vrq_string - Standard Trap function
* Todo: This could use the p_kbshift system variable instead
* In:   a1      Parameter block
*       a0      VDI struct
vrq_string:
	uses_d1
	movem.l	d2/a0-a1,-(a7)

	move.w	#2,-(a7)
	move.w	#1,-(a7)
	trap	#13
	addq.l	#4,a7
	tst.w	d0
	beq	.no_key

	move.w	#2,-(a7)
	move.w	#2,-(a7)
	trap	#13
	addq.l	#4,a7
	and.w	#$00ff,d0
	move.l	d0,d1
	swap	d1
	lsl.w	#8,d1
	or.w	d1,d0
.no_key:

	movem.l	(a7)+,d2/a0-a1
	used_d1

	tst.w	d0
	beq	.no_result
	move.l	intin(a1),a2
	tst.w	(a2)
	bmi	.keep_scancode
	and.w	#$00ff,d0
.keep_scancode:
	move.l	intout(a1),a2
	move.w	d0,(a2)
	move.l	control(a1),a2
	move.w	#1,L_intout(a2)
.no_result:

	done_return


* vq_key_s - Standard Trap function
* Todo: This could use the p_kbshift system variable instead
* In:   a1      Parameter block
*       a0      VDI struct
vq_key_s:
	uses_d1
	movem.l	d2/a0-a2,-(a7)
	move.w	#-1,-(a7)	; Kbshift(-1)
	move.w	#$0b,-(a7)
	trap	#13
	addq.l	#4,a7
	movem.l	(a7)+,d2/a0-a2
	used_d1
	and.w	#$000f,d0
	move.l	intout(a1),a2
	move.w	d0,(a2)
	done_return


* vex_butv - Standard Trap function
* Todo:
* In:   a1      Parameter block
*       a0      VDI struct
vex_butv:
	uses_d1

	move.l	control(a1),a1
	move.l	14(a1),d0
	move.l	vwk_real_address(a0),a2
	move.l	wk_vector_button(a2),d1
	move.l	d0,wk_vector_button(a2)
	move.l	d1,18(a1)

	used_d1
	; mimic the original VDI
	move.l _linea_vars,a0
	move.l d0,USER_BUT(a0)
vex_butv_ex:
	done_return


* vex_motv - Standard Trap function
* Todo:
* In:   a1      Parameter block
*       a0      VDI struct
vex_motv:
	uses_d1

	move.l	control(a1),a1
	move.l	14(a1),d0
	move.l	vwk_real_address(a0),a2
	move.l	wk_vector_motion(a2),d1
	move.l	d0,wk_vector_motion(a2)
	move.l	d1,18(a1)

	used_d1
	; mimic the original VDI
	move.l _linea_vars,a0
	move.l d0,USER_MOT(a0)
vex_motv_ex:
	done_return


* vex_curv - Standard Trap function
* Todo:
* In:   a1      Parameter block
*       a0      VDI struct
vex_curv:
	uses_d1

	move.l	control(a1),a1
	move.l	14(a1),d0
	move.l	vwk_real_address(a0),a2
	move.l	wk_vector_draw(a2),d1
	move.l	d0,wk_vector_draw(a2)
	move.l	d1,18(a1)

	used_d1
	; mimic the original VDI
	move.l _linea_vars,a0
	move.l d0,USER_CUR(a0)
vex_curv_ex:
	done_return

* vex_wheelv - Standard Trap function
* Todo:
* In:   a1      Parameter block
*       a0      VDI struct
vex_wheelv:
	uses_d1

	move.l	a1,-(sp)
	move.l	control(a1),a1
	move.l	14(a1),d0
	move.l	vwk_real_address(a0),a2
	move.l	wk_vector_wheel(a2),d1
	move.l	d0,wk_vector_wheel(a2)
	move.l	d1,18(a1)

	move.l	(sp)+,a1

	used_d1

	; If no mouse type, the original VDI is called too
	tst.w	_stand_alone
	beq     redirect
	done_return


* vex_timv - Standard Trap function
* Todo:
* In:   a1      Parameter block
*       a0      VDI struct
vex_timv:
	move.l	control(a1),a2

	; mimic the original VDI
	move.w  sr,d0
	ori.w   #0x0700,sr               ; disable interrupts
	move.l  _linea_vars,a0
	move.l  USER_TIM(a0),18(a2)
	move.l  14(a2),USER_TIM(a0)
	move.w  d0,sr

	move.l	intout(a1),a2
	move.w	(timer_ms).w,(a2)

vex_timv_ex:
	done_return


* vb_draw - Support function
* Todo: ?
* In:	-
	dc.b	"XBRA"
	dc.b	"fVDI"
	dc.l    0
vb_draw:
    move.l  _linea_vars,a0
	tst.w   M_HID_CNT(a0)            ; allowed to draw mouse ?
	bne     .fast_return
	tst.b   MOUSE_FLAG(a0)           ; cntrl access to mouse form
	bne	.fast_return		; Already drawing - abort!
	bclr    #0,CUR_FLAG(a0)          ; control access to cursor position
	beq	.fast_return

	; Do a proper redraw
	move.w	mouse_op,d0
	swap	d0
	move.w  CUR_X(a0),d0
	move.w  CUR_Y(a0),d1

	move.l	_screen_wk,a1
	move.l	wk_driver(a1),a0
	move.l	driver_default_vwk(a0),a0

	move.l	wk_r_mouse(a1),a2
	moveq	#0,d2			; move shown
	tst.w	wk_mouse_forced(a1)
	beq	.not_forced
	addq.w	#4,d2
	clr.w	wk_mouse_forced(a1)
.not_forced:
	jsr	(a2)
.no_error:
	move.w	d0,mouse_op
	swap	d0
	move.w	d0,pointer_delay
.fast_return:
	rts

* do_nothing - Support function
* Todo: ?
* In:	-
_do_nothing:
	rts


* vector_call - Support function
* Calls out to function passing loword of data in d1, and hiword in d0
* In: void *vector (on stack)
*     long data    (on stack)
_vector_call:
	movem.l	d1-d7/a0-a6,-(a7)
	move.l	14*4+8(a7),d0
	moveq	#0,d1
	move.w	d0,d1
	clr.w	d0
	swap	d0
	move.l	14*4+4(a7),a0
	jsr	(a0)
	swap	d0
	move.w	d1,d0
	movem.l	(a7)+,d1-d7/a0-a6
	rts


*
* Interrupt service routine called by etv_timer
*
	dc.b	"XBRA"
	dc.b	"fVDI"
_old_etv_timer:
	dc.l    0
sys_timer:
	move.l     _linea_vars,a0
	move.l     NEXT_TIM(a0),-(a7)       ; USER_TIM will call NEXT_TIM
	move.l     USER_TIM(a0),-(a7)
	rts

* init_interrupts
* In:
_init_interrupts:
	movem.l    d0-d3/a0-a2,-(a7)
	move.w     sr,d3
	ori.w      #0x0700,sr               ; disable interrupts
	pea.l      sys_timer(pc)            ; new etv_timer function
	move.w     #etv_timer/4,-(a7)
	move.w     #5,-(a7)
	trap       #13
	addq.l     #8,a7
	move.l     _linea_vars,a0
	move.l     d0,_old_etv_timer
	move.l     d0,NEXT_TIM(a0)
	lea        _do_nothing(pc),a1
	move.l     a1,USER_TIM(a0)
	move.l     a1,USER_BUT(a0)
	move.l     a1,USER_MOT(a0)
	lea        _user_cur(pc),a1
	move.l     a1,USER_CUR(a0)
	lea.l      mouse_form(pc),a2
	move.l     _screen_wk,a1
	bsr        transform_mouse          ; mouse shape

	clr.w      MOUSE_BT(a0)             ; no button pressed
	clr.b      CUR_MS_STAT(a0)          ; no mouse move
	clr.b      MOUSE_FLAG(a0)           ; mouse drawing possible
	moveq.l    #1,d0
	move.w     d0,M_HID_CNT(a0)         ; mouse is hidden
	move.b     d0,CUR_FLAG(a0)          ; enable VBL mouse drawing
	move.w     d0,V_HID_CNT(a0)         ; alpha cursor is hidden
	move.w     #0x1E1E,V_PERIOD(a0)
	move.w     #1<<(CURSOR_BL+8),V_STAT_0(a0)
	move.l     DEV_TAB(a0),d0
	lsr.l      #1,d0
	bclr       #15,d0                   ; clear overflow bit
	move.l     d0,GCURX(a0)
	move.l     d0,CUR_X(a0)
	movea.l    (vbl_queue).w,a0
	move.l     #vb_draw,(a0)            ; VBL mouse routine
	pea.l      mouse_int(pc)            ; mouse interrupt routine
	pea.l      mouse_param(pc)          ; parameters for Initmouse
	moveq.l    #1,d0                    ; Initmouse(1) (relative mode)
	move.l     d0,-(a7)
	trap       #14
	lea.l      12(a7),a7
	move.w     d3,sr
	movem.l    (a7)+,d0-d3/a0-a2
	rts

mouse_param:
	.dc.b 0                  ; topmode
	.dc.b 0                  ; buttons
	.dc.b 1                  ; xparam
	.dc.b 1                  ; yparam

mouse_form:
	.dc.w 1                  ; mf_xhot
	.dc.w 1                  ; mf_yhot
	.dc.w 1                  ; mf_nplanes - immer 1
	.dc.w 0                  ; mf_fg - Maskenfarbe
	.dc.w 1                  ; mf_bg - Cursorfarbe
	; mask
	.dc.w 0xc000
	.dc.w 0xe000
	.dc.w 0xf000
	.dc.w 0xf800
	.dc.w 0xfc00
	.dc.w 0xfe00
	.dc.w 0xff00
	.dc.w 0xff80
	.dc.w 0xffc0
	.dc.w 0xffe0
	.dc.w 0xfe00
	.dc.w 0xef00
	.dc.w 0xcf00
	.dc.w 0x8780
	.dc.w 0x0780
	.dc.w 0x0380
	; data
	.dc.w 0x0000
	.dc.w 0x4000
	.dc.w 0x6000
	.dc.w 0x7000
	.dc.w 0x7800
	.dc.w 0x7c00
	.dc.w 0x7e00
	.dc.w 0x7f00
	.dc.w 0x7f80
	.dc.w 0x7c00
	.dc.w 0x6c00
	.dc.w 0x4600
	.dc.w 0x0600
	.dc.w 0x0300
	.dc.w 0x0300
	.dc.w 0x0000

_reset_interrupts:
	movem.l    d0-d3/a0-a2,-(a7)
	move.w     sr,d3
	ori.w      #0x0700,sr
	move.l     _linea_vars,a0
	lea.l      USER_TIM(a0),a0          ; clear linea vectors
	clr.l      (a0)+                    ; USER_TIM
	clr.l      (a0)+                    ; NEXT_TIM
	clr.l      (a0)+                    ; USER_BUT
	clr.l      (a0)+                    ; USER_CUR
	clr.l      (a0)+                    ; USER_MOT
	move.l     _old_etv_timer(pc),(etv_timer).w
	clr.l      -(a7)
	clr.l      -(a7)
	clr.l      -(a7)                    ; turn off mouse Initmouse(0)
	trap       #14
	lea.l      12(a7),a7
	movea.l    (vbl_queue).w,a0
	clr.l      (a0)                     ; reset VBL routine
	move.w     d3,sr
	movem.l    (a7)+,d0-d3/a0-a2
	rts

mouse_int:
	movem.l    d0-d3/a0-a1/a3,-(a7)
	move.b     (a0)+,d0
	move.b     d0,d1
	moveq.l    #-8,d2
	and.b      d2,d1
	sub.b      d2,d1                    ; mouse data paket?
	bne        mouse_exit
	moveq.l    #3,d2
	and.w      d2,d0                    ; mouse button state (button 0 and 1 are swapped)
	lsr.w      #1,d0                    ; right mouse button pressed?
	bcc.s      mouse_but
	addq.w     #2,d0                    ; yes, set bit 1
mouse_but:
	move.l     _linea_vars,a3
	move.b     CUR_MS_STAT(a3),d1
	and.w      d2,d1                    ; mask buttons
	cmp.w      d1,d0                    ; key state chaneged?
	beq.s      mouse_no_but
	movea.l    USER_BUT(a3),a1
	move.w     d1,-(a7)
	jsr        (a1)
	move.w     (a7)+,d1
	move.w     d0,MOUSE_BT(a3)
	eor.b      d0,d1
	ror.b      #2,d1
	or.b       d0,d1                    ; change of key state (bit 6 & 7)
mouse_no_but:
	move.b     d1,CUR_MS_STAT(a3)
	move.b     (a0)+,d2                 ; relative x motion
	move.b     (a0)+,d3                 ; relative y motion
	move.b     d2,d0
	or.b       d3,d0                    ; mouse was moved?
	beq        mouse_exit
	ext.w      d2
	ext.w      d3
	movem.w    GCURX(a3),d0-d1          ; last mouse position
	add.w      d2,d0
	add.w      d3,d1
	bsr.s      clip_mouse               ; clip mouse coordinates
	cmp.w      GCURX(a3),d0             ; x position changed?
	bne.s      mouse_user_mot
	cmp.w      GCURY(a3),d1             ; y position changed?
	beq.s      mouse_exit
mouse_user_mot:
	bset       #5,CUR_MS_STAT(A3)       ; mouse has been moved
	movem.w    d0-d1,-(a7)
	movea.l    USER_MOT(a3),a1
	jsr        (a1)
	movem.w    (a7)+,d2-d3
	sub.w      d0,d2
	sub.w      d1,d3
	or.w       d2,d3
	beq.s      mouse_savexy
	bsr.s      clip_mouse               ; clip mouse coordinates
mouse_savexy:
	movem.w    d0-d1,GCURX(a3)
	move.l     _screen_wk,a1
	movem.w    d0-d1,wk_mouse_position(a1)
	movea.l    USER_CUR(a3),a1
	jsr        (a1)
mouse_exit:
	movem.l    (a7)+,d0-d3/a0-a1/a3
	rts

clip_mouse:
	tst.w      d0
	bpl.s      clip_mouse_x2
	moveq.l    #0,d0
	bra.s      clip_mouse_y1
clip_mouse_x2:
	cmp.w      V_REZ_HZ(a3),d0
	blt.s      clip_mouse_y1
	move.w     V_REZ_HZ(a3),d0
	subq.w     #1,d0
clip_mouse_y1:
	tst.w      d1
	bpl.s      clip_mouse_y2
	moveq.l    #0,d1
	rts
clip_mouse_y2:
	cmp.w      V_REZ_VT(a3),d1
	blt.s      clip_mouse_exit
	move.w     V_REZ_VT(a3),d1
	subq.w     #1,d1
clip_mouse_exit:
	rts

*
* USER_CUR
*
_user_cur:
	move.w     sr,-(a7)
	ori.w      #0x0700,sr
	move.l     _linea_vars,a1
	move.w     d0,CUR_X(a1)             ; mouse x coordinate
	move.w     d1,CUR_Y(a1)             ; mouse y coordinate
	bset       #0,CUR_FLAG(a1)          ; mouse must be redrawn on next VBL
	move.w     (a7)+,sr
	rts

pointer_delay:
	dc.w	0
mouse_op:
	dc.w	0

	end
