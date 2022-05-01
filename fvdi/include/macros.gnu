  .ifdef	xbra_chain
*	.external	_vdi_address
  .endc

  .ifdef	_stand_alone
*	xref	_stand_alone
  .endc


	.macro	save_regs
	movem.l	a1-a2,-(a7)
	.endm

	.macro	uses_d1
  .if	transparent
	move.l	d1,-(a7)
  .endif
	.endm

	.macro	used_d1
  .if	transparent
	move.l	(a7)+,d1
  .endif
	.endm

	.macro	restore_regs
	movem.l	(a7)+,a1-a2
	.endm

	.macro	real_return
	restore_regs
	rte
	.endm

	.macro	done_return
  .ifne	only_fvdi
	real_return
  .else
	return
  .endif
	.endm

* Convert from a 16 bit size to fix31
	.macro  short_to_fix31 src dst
  .ifc	\src,\dst
	swap	\dst
	clr.w	\dst
  .else
	moveq	#0,\dst
	move.w	\src,\dst
	swap	\dst
  .endif
	.endm

* Convert from fix31 to a 16 bit size, rounding up
	.macro fix31_to_short src dst
	addi.l	#0x8000,\src
	swap	\src
  .ifnc	\src,\dst
	move.w	\src,\dst
  .endif
	.endm

	.macro	ijsr indirect
  .if mc68000 == 1
	pea	ret\@
	move.l	\indirect,-(a7)
	rts
ret\@:
  .else
	jsr	([\indirect])
  .endif
	.endm

	.macro	ijmp indirect
  .ifne mc68000 == 1
	move.l	\indirect,-(a7)
	rts
  .else
	jmp	([\indirect])
  .endif
	.endm
