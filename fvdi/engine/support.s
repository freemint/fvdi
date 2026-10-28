*****
* fVDI support routines
*
* Copyright 1997-2003, Johan Klockars
* This software is licensed under the GNU General Public License.
* Please, see LICENSE.TXT for further information.
*****

transparent	equ	1		; Fall through?

	.include	"vdi.inc"
	.include	"macros.inc"

	xref	_old_wk_handle
	xref	_cpu
	xref	_stand_alone
	xref	_allocate_block,_free_block

	xdef	_flip_words,_flip_longs
	xdef	redirect,redirect_d0
	xdef	_call_other
	xdef	_initialize_palette
	xdef	asm_allocate_block,asm_free_block
	xdef	_cache_flush


	text

* flip_words(short *addr, long n)
* Byte swap a number of consecutive words
_flip_words:
	move.l	4(a7),a0
	move.l	8(a7),d0
	bra	.loopend
.loop:
	move.w	(a0),d1
	rol.w	#8,d1
	move.w	d1,(a0)+
.loopend:
	dbra	d0,.loop
	rts


* flip_longs(long *addr, long n)
* Byte swap a number of consecutive longs
_flip_longs:
	move.l	4(a7),a0
	move.l	8(a7),d0
	bra	.loopend_l
.loop_l:
	move.l	(a0),d1
	rol.w	#8,d1
	swap	d1
	rol.w	#8,d1
	move.l	d1,(a0)+
.loopend_l:
	dbra	d0,.loop_l
	rts


* redirect    - Remaps VDI call to default physical workstation
* redirect_d0 =      -   "   -     specified handle
* Todo:	?
* In:	a1	Parameter block   **** Should perhaps put this in d1? ****
redirect:
	move.w	_old_wk_handle,d0
;redirect_d0:
	move.l	control(a1),a0
	cmp.w	handle(a0),d0		; Already correct?
	bne	.call
	move.l	a1,d1			; That's where the VDI wants it
	restore_regs
	tst.w	_stand_alone
	bne	.real_rte
	moveq	#0x73,d0
	move.l	_vdi_address(pc),-(a7)
	rts
.real_rte:
	rte
.call:
	tst.w	_stand_alone
	bne	.no_redirect
	bsr	asm_call_other
.no_redirect:
	restore_regs
	rte

redirect_d0:
	bsr	asm_call_other
	real_return


* call_other - Do a VDI call to the previous VDI instead
* Todo:	?
* In:	a1	Parameter block   **** Should perhaps put this in d1? ****
*	d0	Handle of the other workstation
* Out:	d0	Possibly new handle created
_call_other:
	move.l	4(a7),a1
	move.l	8(a7),d0
asm_call_other:
	move.l	a1,d1			; That's where the VDI wants it
	move.l	control(a1),a0
	move.w	handle(a0),-(a7)	; Remember original handle
	move.w	d0,handle(a0)		; Point to handle from above (normally default physical workstation)
	moveq	#$73,d0

* Pretend the call was from this code
* We need to check various values when the normal VDI returns
 ifeq mcoldfire
	move.w	#$ffff,-(a7)		; Mark old stack
	move.w	#$88,-(a7)		; In case we're on >='020
	pea	.vdi_ret(pc)
	move.w	sr,-(a7)
	move.l	_vdi_address(pc),-(a7)
	rts

.vdi_ret:
	cmp.w	#$ffff,(a7)+
	beq	.was_020
	addq.l	#2,a7			; Skip $88 if not >='020
.was_020:

	move.l	control(a1),a0
	move.w	handle(a0),d0
	move.w	(a7)+,handle(a0)
	rts
 else
	pea	.vdi_ret(pc)
	move.w	sr,-(sp)
	move.w	#0x4000,-(sp)
	move.l	_vdi_address(pc),-(sp)
	rts
.vdi_ret:
	move.l	control(a1),a0
	move.w	handle(a0),d0
	move.w	(sp)+,handle(a0)
	rts
 endc

* initialize_palette(Virtual *vwk, long start, long n, short requested[][3], Colour palette[])
* Set palette colours
_initialize_palette:
	movem.l		d0-d7/a0-a6,-(sp)	; Overkill

	move.l		15*4+4(a7),a0
	move.l		15*4+8(a7),d1
	move.l		15*4+12(a7),d0
	swap		d0
	move.w		d1,d0
	move.l		15*4+16(a7),a1
	move.l		15*4+20(a7),a2

	move.l		vwk_real_address(a0),a3
	move.l		wk_r_set_palette(a3),a3
	jsr		(a3)

	movem.l		(sp)+,d0-d7/a0-a6
	rts


* long allocate_block(long size)
* Allocate a block from the internal memory pool
asm_allocate_block:
	movem.l	d1-d2/a0-a2,-(a7)
	move.l	4+5*4(a7),-(a7)
	bsr	_allocate_block
	addq.l	#4,a7
	movem.l	(a7)+,d1-d2/a0-a2
	rts


* free_block(void *addr)
* Free a block and return it to the internal memory pool
asm_free_block:
	movem.l	d0-d2/a0-a2,-(a7)
	move.l	4+6*4(a7),-(a7)
	bsr	_free_block
	addq.l	#4,a7
	movem.l	(a7)+,d0-d2/a0-a2
	rts


* void DRIVER_EXPORT cache_flush(void)
* Flush both caches
_cache_flush:
	movem.l	d0-d1,-(a7)
  ifeq	mcoldfire
	move.l	_cpu,d0
	cmp.w	#20,d0
	blo	cache_end
	cmp.w	#30,d0
	bls	is_030		; 68020 and 68030


	dc.w	$f4f8		; cpusha bc
  else
	move.l	a0,-(sp)	; d0/d1 already saved above

	; flush_and_invalidate_caches() stolen from BaS_gcc
	nop			; synchronize/flush store buffer
	clr.l	d0
	clr.l	d1
	move.l	d0,a0
1:
	; push cache line in a0
	.word	0xf4e8		; cpushl bc,(a0) - portasm does not accept this instruction
	lea	0x10(a0),a0	; increment index by 1
	addq.l	#1,d1		; increment counter
	cmpi.w	#512,d1		; sets done?
	bne.s	1b		; no
	clr.l	d1		; set counter to zero again
	addq.l	#1,d0		; increment to next way
	move.l	d0,a0		;
	cmpi.w	#4,d0		; finished flushing all 4 ways?
	bne.s	1b

	move.l	(sp)+,a0
  endc				; mcoldfire

cache_end:
	movem.l	(a7)+,d0-d1
	rts

is_030:
	dc.w	$4e7a		; movec cacr,d0
	dc.w	$0002
	move.l	d0,d1
	or.w	#$808,d1
	dc.w	$4e7b		; movec d1,cacr
	dc.w	$1002
	dc.w	$4e7b		; movec d0,cacr
	dc.w	$0002
	bra	cache_end

	end
