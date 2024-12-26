*****
* fVDI v0.96, 020710
*   Mainly function dispatcher related things
*
* Copyright 1997-2002, Johan Klockars
* This software is licensed under the GNU General Public License.
* Please, see LICENSE.TXT for further information.
*****

stoplog		equ	0		; Log until no more room (not circular)

transparent	equ	1		; Fall through?
return_version	equ	1		; Should fVDI identify itself?

HANDLES		equ	32		; Important things regarding this
MAX_HANDLE	equ	HANDLES		;  also in setup.c

SCREENDEV	equ	7		; Any better ideas?

xbra_chain	equ	1		; Don't want xref for vdi_address

fvdi_magic	equ	1969

	.include	"vdi.inc"
	.include	"macros.inc"

 ifne mcoldfire
STACK_SIZE	equ	8192		; allow more stack for pacf converted routines
 else
STACK_SIZE	equ	4096		; Used to be 2048
 endc

	xref	_startup
	xref	_basepage
	xref	_super
	xref	default_functions,default_opcode5,default_opcode11
	xref	clip_table
	xref	_handle,_handle_link
	xref	_booted
	xref	_screen_virtual,_default_virtual,_screen_wk
	xref	redirect_d0
	xref	start_unimpl,end_unimpl
	xref	_recheck_mtask
	xref	_debug,_xbiosfix
	xref	_vdi_debug,_trap2_debug,_lineA_debug
	xref	_vq_gdos_value

	xdef	_init
	xdef	_trap2_address,_trap2_temp
	xdef	_vdi_dispatch
	xdef	_eddi_dispatch
	xdef	_data_start,_bss_start
	xdef	_vdi_address
	xdef	opcode5,opcode11
	xdef	_bad_or_non_fvdi_handle
	xdef	_sub_call
	xdef	_trap14_address,_trap14
	xdef	_lineA_address,_lineA


	text

init:
_init:
	move.l	4(a7),a5	; Address of BP
	move.l	$c(a5),d0	; Length of TEXT
	add.l	$14(a5),d0	; Length of DATA
	add.l	$1c(a5),d0	; Length of BSS
	add.l	#STACK_SIZE,d0	; Stack size
	add.l	#$100,d0	; BP
	move.l	d0,a6		; Program size
	move.l	a5,d1
	add.l	d0,d1

	and.w	#$fffe,d1	; Even stack address
	move.l	d1,a7

	move.l	d0,-(a7)	; Keep d0 bytes
	move.l	a5,-(a7)	; From address a5
	move.w	d0,-(a7)	; dummy
	move.w	#$4a,-(a7)	; Mshrink
	trap	#1
	lea	12(a7),a7

	move.l	a5,_basepage

	bsr	_startup	; Initialize
	tst.l	d0
	beq	.error

	move.w	#0,-(a7)
	move.l	a6,-(a7)        ; Program size
	move.w	#$31,-(a7)	; Ptermres
	trap	#1

.error:
	move.w	#$0,-(a7)	; Pterm0
	trap	#1


* XBRA chain for Trap #2
	dc.b	"XBRA"
	dc.b	"fVDI"
_trap2_address:
	dc.l	0

* trap2_temp - Startup support
* Todo: Efficiency? Dangers?
* In:	d0	$73 for VDI, else jump to old routine
*	d1	Parameter block
_trap2_temp:
	tst.l	_vdi_address		; If we get here after the real fVDI
	bne	.tt_no_vdi		;  dispatcher is linked in, just leave.

  ifne FVDI_DEBUG
	cmp.w	#2,_debug
	bls	.no_debug1

	movem.l	d0-d2/a0-a2,-(a7)

	move.l	a7,a0
	add.w	#6*4,a0
	move.l	a0,-(a7)
	move.l	d1,-(a7)
	move.l	d0,-(a7)
	jsr	_trap2_debug
	add.w	#12,a7

	movem.l	(a7)+,d0-d2/a0-a2
.no_debug1:
  endc


	cmp.w	#$73,d0
	bne	.tt_no_vdi


	move.l	a2,-(a7)		; Some always needed

	move.l	d1,a2			; a2 - parameter block
	move.l	control(a2),a2		; a2 - control

	move.w	function(a2),d0		; d0 - function number

	cmp.w	#1,d0			; Don't do anything until the
	bne	.no_interest		;  screen workstation is opened.

	move.l	$88,a2
	cmp.l	#_trap2_temp,a2
	bne	.not_alone
	move.l	_trap2_address,a2
.not_alone:
	move.l	a2,_vdi_address
	move.l	#_vdi_dispatch,$88	; Link in the real dispatcher

	movem.l	d1-d2/a0-a1,-(a7)
	bsr	_recheck_mtask		; MiNT/MagiC started after fVDI?
	movem.l	(a7)+,d1-d2/a0-a1

	move.l	(a7)+,a2
	moveq	#$73,d0
	bra	_vdi_dispatch

.no_interest:
	move.l	(a7)+,a2
.tt_no_vdi:
	move.l	_trap2_address,-(a7)
	rts


* XBRA chain for Trap #2
	dc.b	"XBRA"
	dc.b	"fVDI"
_vdi_address:
	dc.l	0

* vdi_dispatch - Trap #2 interceptor/dispatcher
* Todo: Efficiency? Dangers?
* In:	d0	$73 for VDI, else jump to old routine
*	d1	Parameter block
_vdi_dispatch:
	cmp.w	#fvdi_magic,d0		; Changed to $73 if fVDI is enabled
	bne	no_vdi

dispatch_entry:
	save_regs			; Some always needed

	move.l	d1,a2			; a2 - parameter block
	move.l	control(a2),a1		; a1 - control

	lea	_handle,a0
	move.w	handle(a1),d0		; d0 - handle
	cmp.w	#MAX_HANDLE,d0
	bhs	large_handle
handle_ok:
	add.w	d0,d0
	add.w	d0,d0
	move.l	0(a0,d0.w),a0		; a0 - vdi structure
vwk_ok:

	move.w	function(a1),d0		; d0 - function number
	move.l	vwk_real_address(a0),a2	; a2 - workstation
non_fvdi_ok:
	add.w	d0,d0			; Dangerous? What about >255?
	add.w	d0,d0
	add.w	d0,d0
	lea	wk_function(a2),a2
	lea	0(a2,d0.w),a2		; a2 - function address

	move.w	(a2)+,L_ptsout(a1)
	move.w	(a2)+,L_intout(a1)
	move.l	(a2),a2			; a2 - function address

  ifne FVDI_DEBUG
	cmp.l	#_bad_or_non_fvdi_handle,a2
	beq	.special
	cmp.l	#start_unimpl,a2
	blo	.normal
	cmp.l	#end_unimpl,a2
	bcc	.normal			; >=
.special:
  ifne FVDI_DEBUG
	cmp.w	#2,_debug
	bls	.no_debug_special
	movem.l	d0-d2/a0-a2,-(a7)
	ifne mcoldfire
	move.l  8*4+4(a7),-(a7)
	else
	move.l  8*4+2(a7),-(a7)
	endc
	or.l    #1,d1
	move.l	d1,-(a7)
	jsr	_vdi_debug
	addq.l	#8,a7
	movem.l	(a7)+,d0-d2/a0-a2
.no_debug_special:
  endc
	move.l	d1,a1			; a1 - parameter block
	jmp	(a2)			; Only reached for unimplemented functions


.normal:
  endc
  ifne FVDI_DEBUG
	move.l	a2,d0
	move.l	_super,a2
	tst.w	(a2)			; Log function call if that has (super->fvdi_log.active)
	exg	d0,a2			;  been requested (only debug version)
	beq	.no_log			; Currently only logs implemented functions (see above)
	move.l	a2,-(a7)
	move.l	d0,a2
	move.l	6(a2),d0		; super->fvdi_log.current
	cmp.l	10(a2),d0		; super->fvdi_log.end
	bls	.continue_log
  ifne stoplog
	bra	.no_log
  else
	move.l	2(a2),d0		; super->fvdi_log.start
  endc
.continue_log:
	exg	a2,d0
	move.w	function(a1),(a2)+
	move.w	subfunction(a1),(a2)+
	ifne mcoldfire
	move.l  3*4+4(a7),(a2)+		; Address of calling Trap #2
	else
	move.l  3*4+2(a7),(a2)+		; Address of calling Trap #2
	endc
	exg	a2,d0
	move.l	d0,6(a2)
	move.l	(a7)+,a2
.no_log:
  endc

  ifne FVDI_DEBUG
	cmp.w	#2,_debug
	bls	.no_debug
	movem.l	d0-d2/a0-a2,-(a7)
	ifne mcoldfire
	move.l  8*4+4(a7),-(a7)
	else
	move.l  8*4+2(a7),-(a7)
	endc
	move.l	d1,-(a7)
	jsr	_vdi_debug
	addq.l	#8,a7
	movem.l	(a7)+,d0-d2/a0-a2
.no_debug:
  endc

	move.l	d1,a1			; a1 - parameter block
	jmp	(a2)


* The call didn't seem to really be to the VDI,
* but check for a few other possibilities
no_vdi:
    ; Just to be safe, check that fVDI is 'on top'
    lea _vdi_dispatch+2(pc),a0
	cmp.w	#fvdi_magic,(a0)
	beq	.disabled
  ifne return_version
	cmp.w	#$fffe,d0
	beq	.version
  endc
	cmp.w	#$ffff,d0
	beq	.query
.disabled:
	move.l	_vdi_address,-(a7)	; Continue to next in chain
	rts

.version:
	move.l	_vq_gdos_value,d0	 ; A vq_gdos, report name
	rte

.query:
	move.l	#subroutine_call,d0	; Someone doesn't like Trap #2
	rte


* The supplied handle was too large,
* but it might still be valid or simply not needed.
large_handle:
	lea	_handle_link,a0		; Search through linked tables of handles
	sub.w	#MAX_HANDLE,d0
	bra	.test_handle
.search_handle:
	move.l	(a0),a0
	cmp.w	2(a0),d0		; In this handle table?
	blo	handle_found
	sub.w	2(a0),d0
	addq.l	#4,a0
.test_handle:
	tst.l	(a0)
	bne	.search_handle

bad_handle:				; The handle definitely was bad
	move.w	function(a1),d0		; d0 - function number
	cmp.w	#1,d0			; Check for the functions which are OK
	beq	.opnwk_ok		;  without a handle
	cmp.w	#100,d0
	beq	.opnvwk_ok
	cmp.w	#248,d0			; This is a vq_devinfo call
	beq	.opnvwk_ok
.bad_call:
	restore_regs
	tst.w	_stand_alone
	bne	.bad_rte
	moveq	#0x73,d0
	move.l	_vdi_address(pc),-(a7)
	rts
.bad_rte:
	rte
handle_found:
	addq.l #8,a0
	bra handle_ok

.opnwk_ok:				; Fake handle/vwk when necessary
	tst.w	_booted
	bne	.first_opnwk
.really_ok:
.opnvwk_ok:
	moveq	#1,d0			; Set handle to first workstation
	lea	_handle,a0
	bra	handle_ok

.first_opnwk:				; Special treatment for the first opened
	move.l	_default_virtual,a0	;  wk when booted
	bra	vwk_ok


* Entry point for unallocated or redirected handles
* The 'real' handle from the VDI struct means:
*	-1	Bad handle
*	$8000|n	Not screen, fall through on handle n
*	n	Screen, fall through unless v_opnvwk (which fVDI handles)
* In:	a0	VDI struct
*	d1	Parameter block
_bad_or_non_fvdi_handle:
	move.w	vwk_standard_handle(a0),d0
	move.l	d1,a1			; a1 - parameter block
	move.l	control(a1),a2		; a2 - control
	cmp.w	#-1,d0
	bne	.not_bad_handle
	move.l	a2,a1			; a1 - control
	lea	_handle,a0
	bra	bad_handle
.not_bad_handle:
	bclr	#15,d0			; Non-screen handle?
	beq	.screen_handle
	move.w	function(a2),d0		; You only get here for non-fVDI handles
	cmp.w	#1,d0			;  and open/close must be handled for those
	beq	.non_fvdi_oc
	cmp.w	#2,d0
	beq	.non_fvdi_oc
	cmp.w	#100,d0
	beq	.non_fvdi_oc
	cmp.w	#101,d0
	beq	.non_fvdi_oc
	move.w	vwk_standard_handle(a0),d0	; Ordinary non-fVDI call,
	bclr	#15,d0				;  so send it on
	bra	redirect_d0

.screen_handle:				; Can only happen with a fakeboot fVDI
	cmp.w	#100,function(a2)	; Not v_opnvwk?
	bne	redirect_d0
	move.l	a2,a1			; a1 - control
	move.l	_screen_virtual,a0	; Pretend fVDI opened the screen
	bra	vwk_ok

.non_fvdi_oc:				; Non-fVDI open/close
	move.l	a2,a1			; a1 - control
	move.l	_default_virtual,a2	; We need access to the normal routines
	move.l	vwk_real_address(a2),a2	;  somehow, but not to valid data from
	bra	non_fvdi_ok		;  any structure


* XBRA chain for Trap #2 with d0 == $ffff
	dc.b	"XBRA"
	dc.b	"fVDI"
_sub_call:
	dc.l	0

* Fake a subroutine call into fVDI (which assumes a Trap)
* The address of this routine is returned if d0 == $ffff on Trap #2 entry.
* Mainly used by NVDI
subroutine_call:
	move.w	#$ffff,-(a7)		; Mark old stack
	move.w	#$88,-(a7)		; Set up a 'Trap #2' on the stack
	pea	.return_here
	move.w	sr,-(a7)
	ifne	mcoldfire
	move.w	#$4000,-(sp)		; to create a valid ColdFire stack frame,
					; push a format word on the stack
	endc
	bra	dispatch_entry
.return_here:
	cmp.w	#$ffff,(a7)+
	beq	.was_020
	addq.l	#2,a7			; Skip $88 if not >= '020
.was_020:
	rts


* opcode5 - Subfunction Trap dispatcher
* Todo: Dangers?
* In:	a0	VDI struct
*	a1	Parameter block
opcode5:
	move.l	control(a1),a2		; a2 - control
	move.w	subfunction(a2),d0	; d0 - subfunction number
	move.l	vwk_real_address(a0),a2
	lea	wk_opcode5(a2),a2

	cmp.w	-2(a2),d0		; Only allow correct codes
	bls	.ok_5
	moveq	#0,d0			; Unknown function
.ok_5:
	add.w	d0,d0
	add.w	d0,d0
	move.l	0(a2,d0.w),a2		; a2 - subfunction address

	jmp	(a2)


* opcode11 - Subfunction Trap dispatcher
* Todo: Dangers?
* In:	a0	VDI struct
*	a1	Parameter block
opcode11:
	move.l	control(a1),a2		; a2 - control
	move.w	subfunction(a2),d0	; d0 - subfunction number
	move.l	vwk_real_address(a0),a2
	lea	wk_opcode11(a2),a2

	cmp.w	-2(a2),d0		; Only allow correct codes
	bls	.ok_11
	moveq	#0,d0			; Unknown function
.ok_11:
	add.w	d0,d0
	add.w	d0,d0
	move.l	0(a2,d0.w),a2		; a2 - subfunction address

	jmp	(a2)


* EdDI dispatch
* Only used to return version number
_eddi_dispatch:
	tst.w	d0
	bne	unknown_eddi
	move.w	#$0100,d0
	rts

unknown_eddi:
	moveq	#-1,d0
	rts


* XBRA chain for Trap #14
	dc.b	"XBRA"
	dc.b	"fVDI"
_trap14_address:
	ds.l	1

* trap14 - Trap #14 interceptor
* Handles screen related XBIOS calls
* Todo: ?
_trap14:
	tst.w	_xbiosfix
	beq	.continue_trap14
  ifne mcoldfire
	; move	usp,a0			; PortAsm doesn't swallow this although it's valid
	.short	0x4e68
	; mvz.w	2(sp),d0
	.short	0x71ef
	btst	#13,d0			; called from supervisor mode?
  else
	move	usp,a0
	btst	#5,(a7)			; called from supervisor mode?
  endc
	beq	.correct_a0		; no, continue
  ifne mcoldfire
	lea	4(sp),a0		; a0 now points to the program counter field in the exception stack frame
	; mvz.b	(sp),d0			; extract format field of exception stack frame
	.short	0x7197
	lsr.l	#4,d0			; shift bits in place (these are now 4, 5, 6 or 7) depending on original frame offset
	andi.w	#0x7,d0			; mask out format
	lea	0(a0,d0.w),a0		; and compute final address
	bra	.correct_a0
  else
	lea	6(a7),a0		; stack offset for 68000
  endc
	tst.w	$59e			; > 68000?
	beq	.correct_a0		; no, we are correct
	addq.l	#2,a0			; adjust for long stack frame
.correct_a0:
	move.w	(a0),d0
	cmp.w	#2,d0			; is it a Physbase() (2), Logbase() (3) or Getrez() (4) call?
	blo	.continue_trap14
	cmp.w	#4,d0
	bhi	.continue_trap14
	beq	.getrez
	move.l	_screen_wk,d0
	beq	.continue_trap14
	move.l	d0,a0
	move.l	wk_screen_mfdb_address(a0),d0
	rte

.getrez:
	moveq	#SCREENDEV,d0		; Any better ideas?
	rte

.continue_trap14:
	move.l	_trap14_address,a0
	jmp	(a0)


* XBRA chain for LineA
	dc.b	"XBRA"
	dc.b	"fVDI"
_lineA_address:
	ds.l	1

* lineA - LineA interceptor
* Todo:	?
_lineA:
 ifne mcoldfire
	move.l	4(a7),a1
 else
	move.l 	2(a7),a1		; get linea opcode address
 endc
	move.w	(a1),d0
	and.l	#$f,d0			; map out function code

  ifne FVDI_DEBUG
	cmp.w	#2,_debug
	bls	.no_debug2
	movem.l	d0-d2/a0-a2,-(a7)
	move.l	a1,-(a7)
	move.l	d0,-(a7)
	jsr	_lineA_debug
	addq.l	#8,a7
	movem.l	(a7)+,d0-d2/a0-a2
.no_debug2:
	tst.w	d0
  endc

	beq	.continue_lineA		; Need for font address in a1
	move.l	_screen_wk,d0
	beq	.continue_lineA
	move.l	d0,a0
	move.l	wk_screen_linea(a0),a0
	addq.l	#2,a1
 ifne mcoldfire
	move.l 	a1,4(a7)
 else
	move.l	a1,2(a7)
 endc
	rte

.continue_lineA:
	move.l	_lineA_address,a0
	jmp	(a0)


	data
_data_start:				; Just markers

	bss
_bss_start:

	end
