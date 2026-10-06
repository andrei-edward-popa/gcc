/* STM8 integer arithmetic using the hardware multiplier and divider.
   Copyright (c) 2026 Andrei-Edward Popa

   This file is part of GCC.

   GCC is free software; you can redistribute it and/or modify it under
   the terms of the GNU General Public License as published by the Free
   Software Foundation; either version 3, or (at your option) any later
   version.

   GCC is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
   for more details.

   Under Section 7 of GPL version 3, you are granted additional
   permissions described in the GCC Runtime Library Exception, version
   3.1, as published by the Free Software Foundation.

   You should have received a copy of the GNU General Public License and
   a copy of the GCC Runtime Library Exception along with this program;
   see the files COPYING3 and COPYING.RUNTIME respectively.  If not, see
   <http://www.gnu.org/licenses/>.  */

#ifdef __STM8_LARGE__
	.stm8_model "large"
#define STM8_ARG 4
#define STM8_RETURN_HI retf
#define STM8_RETURN_SI retf
#define STM8_CALL callf
#else
#define STM8_ARG 3
#define STM8_RETURN_SI ret
#define STM8_CALL call
	.macro STM8_RETURN_HI
	.cfi_remember_state
	popw y
	.cfi_adjust_cfa_offset -2
	.cfi_escape 0x16,11,2,0x7a,0
	addw sp, #2
	.cfi_adjust_cfa_offset -2
	jp (y)
	.cfi_restore_state
	.endm
#endif

/* Read exactly the saved PC width, independently of its debug container.  */
	.macro STM8_CFI_START
	.cfi_sections .debug_frame
	.cfi_startproc
	.cfi_escape 0x16,11,5,0x11,130-STM8_ARG,0x22,0x94,STM8_ARG-1
	.endm

#ifdef L_mulhi3
	.section .text.__mulhi3, "ax", @progbits
	.global ___mulhi3
	.type ___mulhi3, @function
___mulhi3:
	STM8_CFI_START
	/* Keep both bytes of the first operand for its low product.  */
	ldw y, x
	ld a, (STM8_ARG + 1, sp)
	mul x, a
	pushw x
	.cfi_adjust_cfa_offset 2
	/* Only the low bytes of the cross products contribute modulo 2^16.  */
	ld a, yh
	ld xl, a
	ld a, (STM8_ARG + 3, sp)
	mul x, a
	ld a, xl
	add a, (1, sp)
	ld (1, sp), a
	ld a, yl
	ld xl, a
	ld a, (STM8_ARG + 2, sp)
	mul x, a
	ld a, xl
	add a, (1, sp)
	popw x
	.cfi_adjust_cfa_offset -2
	ld xh, a
	STM8_RETURN_HI
	.cfi_endproc
	.size ___mulhi3, .-___mulhi3
#endif

#ifdef L_udivhi3
	.section .text.__udivhi3, "ax", @progbits
	.global ___udivhi3
	.type ___udivhi3, @function
___udivhi3:
	STM8_CFI_START
	ldw y, (STM8_ARG, sp)
	tnzw y
	jrne .Ludivhi3_nonzero
	clrw x
	STM8_RETURN_HI
.Ludivhi3_nonzero:
	divw x, y
	STM8_RETURN_HI
	.cfi_endproc
	.size ___udivhi3, .-___udivhi3
#endif

#ifdef L_umodhi3
	.section .text.__umodhi3, "ax", @progbits
	.global ___umodhi3
	.type ___umodhi3, @function
___umodhi3:
	STM8_CFI_START
	ldw y, (STM8_ARG, sp)
	tnzw y
	jrne .Lumodhi3_nonzero
	clrw x
	STM8_RETURN_HI
.Lumodhi3_nonzero:
	divw x, y
	ldw x, y
	STM8_RETURN_HI
	.cfi_endproc
	.size ___umodhi3, .-___umodhi3
#endif

#ifdef L_divhi3
	.section .text.__divhi3, "ax", @progbits
	.global ___divhi3
	.type ___divhi3, @function
___divhi3:
	STM8_CFI_START
	ldw y, (STM8_ARG, sp)
	tnzw y
	jrne .Ldivhi3_nonzero
	clrw x
	STM8_RETURN_HI
.Ldivhi3_nonzero:
	ld a, xh
	xor a, (STM8_ARG, sp)
	tnzw y
	jrpl .Ldivhi3_divisor_positive
	negw y
.Ldivhi3_divisor_positive:
	tnzw x
	jrpl .Ldivhi3_dividend_positive
	negw x
.Ldivhi3_dividend_positive:
	divw x, y
	tnz a
	jrpl .Ldivhi3_positive
	negw x
.Ldivhi3_positive:
	STM8_RETURN_HI
	.cfi_endproc
	.size ___divhi3, .-___divhi3
#endif

#ifdef L_modhi3
	.section .text.__modhi3, "ax", @progbits
	.global ___modhi3
	.type ___modhi3, @function
___modhi3:
	STM8_CFI_START
	ldw y, (STM8_ARG, sp)
	tnzw y
	jrne .Lmodhi3_nonzero
	clrw x
	STM8_RETURN_HI
.Lmodhi3_nonzero:
	ld a, xh
	tnzw x
	jrpl .Lmodhi3_dividend_positive
	negw x
.Lmodhi3_dividend_positive:
	tnzw y
	jrpl .Lmodhi3_divisor_positive
	negw y
.Lmodhi3_divisor_positive:
	divw x, y
	ldw x, y
	tnz a
	jrpl .Lmodhi3_positive
	negw x
.Lmodhi3_positive:
	STM8_RETURN_HI
	.cfi_endproc
	.size ___modhi3, .-___modhi3
#endif

/* SI arguments are entirely on the stack.  Unlike HI results, SI
   results occupy Y:X and use caller cleanup in both memory models.
   STM8_ARG accounts for the two- or three-byte return address.  */

	.macro STM8_PRODUCT left right
	ld a, (STM8_ARG + 4 + \left, sp)
	ld xl, a
	ld a, (STM8_ARG + 8 + \right, sp)
	mul x, a
	.endm
	.macro STM8_ADD8 left right
	STM8_PRODUCT \left \right
	addw x, (2, sp)
	ldw (2, sp), x
	jrnc .Lcarry\@
	inc (1, sp)
.Lcarry\@:
	.endm
	.macro STM8_ADD16 left right
	STM8_PRODUCT \left \right
	addw x, (1, sp)
	ldw (1, sp), x
	.endm
	.macro STM8_ADD24 left right
	STM8_PRODUCT \left \right
	ld a, xl
	add a, (1, sp)
	ld (1, sp), a
	.endm

#ifdef L_mulsi3
	.section .text.__mulsi3, "ax", @progbits
	.global ___mulsi3
	.type ___mulsi3, @function
___mulsi3:
	STM8_CFI_START
	ld a, (STM8_ARG + 3, sp)
	ld xl, a
	ld a, (STM8_ARG + 7, sp)
	mul x, a
	pushw x
	.cfi_adjust_cfa_offset 2
	clrw y
	pushw y
	.cfi_adjust_cfa_offset 2
	STM8_ADD8 2 3
	STM8_ADD8 3 2
	STM8_ADD16 1 3
	STM8_ADD16 2 2
	STM8_ADD16 3 1
	STM8_ADD24 0 3
	STM8_ADD24 1 2
	STM8_ADD24 2 1
	STM8_ADD24 3 0
	popw y
	.cfi_adjust_cfa_offset -2
	popw x
	.cfi_adjust_cfa_offset -2
	STM8_RETURN_SI
	.cfi_endproc
	.size ___mulsi3, .-___mulsi3

#endif

#ifdef L_stm8_udivmodsi4
	.section .text.__stm8_udivmodsi4, "ax", @progbits
	.global ___stm8_udivmodsi4
	.type ___stm8_udivmodsi4, @function
___stm8_udivmodsi4:
	STM8_CFI_START
	push #32
	.cfi_adjust_cfa_offset 1
	ldw y, (STM8_ARG + 5, sp)
	ldw x, (STM8_ARG + 7, sp)
	tnzw y
	jrne .Ldivsi_slow
	tnzw x
	jreq .Ldivsi_zero
	ld a, xh
	tnz a
	jrne .Ldivsi_word
	/* Divide high word, then two bytes; each partial remainder is < D.  */
	ld a, xl
	ldw x, (STM8_ARG + 1, sp)
	div x, a
	ldw (STM8_ARG + 1, sp), x
	ld xh, a
	ld a, (STM8_ARG + 3, sp)
	ld xl, a
	ld a, (STM8_ARG + 8, sp)
	div x, a
	push a
	.cfi_adjust_cfa_offset 1
	ld a, xl
	ld (STM8_ARG + 4, sp), a
	pop a
	.cfi_adjust_cfa_offset -1
	ld xh, a
	ld a, (STM8_ARG + 4, sp)
	ld xl, a
	ld a, (STM8_ARG + 8, sp)
	div x, a
	push a
	.cfi_adjust_cfa_offset 1
	ld a, xl
	ld (STM8_ARG + 5, sp), a
	pop a
	.cfi_adjust_cfa_offset -1
	clrw x
	ld xl, a
	clrw y
	jra .Ldivsi_store
.Ldivsi_word:
	ldw y, (STM8_ARG + 1, sp)
	tnzw y
	jrne .Ldivsi_slow
	/* The entire dividend and divisor fit DIVW.  */
	ldw y, (STM8_ARG + 3, sp)
	exgw x, y
	divw x, y
	ldw (STM8_ARG + 3, sp), x
	ldw x, y
	clrw y
	jra .Ldivsi_store
.Ldivsi_zero:
	ldw (STM8_ARG + 1, sp), x
	ldw (STM8_ARG + 3, sp), x
	jra .Ldivsi_store
.Ldivsi_slow:
	clrw x
	clrw y
.Ldivsi_loop:
	/* The argument slot holds input followed by quotient bits.  */
	sll (STM8_ARG + 4, sp)
	rlc (STM8_ARG + 3, sp)
	rlc (STM8_ARG + 2, sp)
	rlc (STM8_ARG + 1, sp)
	rlcw x
	rlcw y
	/* CPW has no Y/SP form; EXGW preserves the comparison flags.  */
	exgw x, y
	cpw x, (STM8_ARG + 5, sp)
	exgw x, y
	jrult .Ldivsi_next
	jrugt .Ldivsi_subtract
	cpw x, (STM8_ARG + 7, sp)
	jrult .Ldivsi_next
.Ldivsi_subtract:
	subw x, (STM8_ARG + 7, sp)
	jrnc .Ldivsi_no_borrow
	decw y
.Ldivsi_no_borrow:
	subw y, (STM8_ARG + 5, sp)
	inc (STM8_ARG + 4, sp)
.Ldivsi_next:
	dec (1, sp)
	jrne .Ldivsi_loop
.Ldivsi_store:
	/* Preserve the low remainder while X addresses the output buffer.  */
	pushw x
	.cfi_adjust_cfa_offset 2
	ldw x, (STM8_ARG + 11, sp)
	ldw (x), y
	ldw y, (1, sp)
	ldw (2, x), y
	addw sp, #2
	.cfi_adjust_cfa_offset -2
	ldw y, (STM8_ARG + 1, sp)
	ldw x, (STM8_ARG + 3, sp)
	addw sp, #1
	.cfi_adjust_cfa_offset -1
	STM8_RETURN_SI
	.cfi_endproc
	.size ___stm8_udivmodsi4, .-___stm8_udivmodsi4

#endif

/* Negate the four-byte argument in place.  NEGW sets C for nonzero input;
   LDW preserves it, so only a zero low word needs the high-word increment.  */
	.macro STM8_NEG32 offset
	ldw x, (\offset, sp)
	cplw x
	ldw (\offset, sp), x
	ldw x, (\offset + 2, sp)
	negw x
	ldw (\offset + 2, sp), x
	jrc .Lneg_done\@
	ldw x, (\offset, sp)
	incw x
	ldw (\offset, sp), x
.Lneg_done\@:
	.endm

	.macro STM8_DIVSI name signed remainder
	.section .text.\name, "ax", @progbits
	.global _\name
	.type _\name, @function
_\name:
	STM8_CFI_START
	.if \signed
	ld a, (STM8_ARG, sp)
	.if !\remainder
	xor a, (STM8_ARG + 4, sp)
	.endif
	push a
	.cfi_adjust_cfa_offset 1
	tnz (STM8_ARG + 1, sp)
	jrpl .Lpositive_n\@
	STM8_NEG32 STM8_ARG + 1
.Lpositive_n\@:
	tnz (STM8_ARG + 5, sp)
	jrpl .Lpositive_d\@
	STM8_NEG32 STM8_ARG + 5
.Lpositive_d\@:
	.endif
	/* Four remainder bytes follow the ten-byte outgoing argument area.  */
	sub sp, #14
	.cfi_adjust_cfa_offset 14
	ldw x, (STM8_ARG + 14 + \signed, sp)
	ldw (1, sp), x
	ldw x, (STM8_ARG + 16 + \signed, sp)
	ldw (3, sp), x
	ldw x, (STM8_ARG + 18 + \signed, sp)
	ldw (5, sp), x
	ldw x, (STM8_ARG + 20 + \signed, sp)
	ldw (7, sp), x
	ldw x, sp
	addw x, #11
	ldw (9, sp), x
	STM8_CALL ___stm8_udivmodsi4
	.if \remainder
	ldw y, (11, sp)
	ldw x, (13, sp)
	.endif
	addw sp, #14
	.cfi_adjust_cfa_offset -14
	.if \signed
	pop a
	.cfi_adjust_cfa_offset -1
	tnz a
	jrpl .Lpositive_result\@
	cplw y
	negw x
	jrc .Lpositive_result\@
	incw y
.Lpositive_result\@:
	.endif
	STM8_RETURN_SI
	.cfi_endproc
	.size _\name, .-_\name
	.endm

#ifdef L_udivsi3
	STM8_DIVSI __udivsi3 0 0
#endif
#ifdef L_umodsi3
	STM8_DIVSI __umodsi3 0 1
#endif
#ifdef L_divsi3
	STM8_DIVSI __divsi3 1 0
#endif
#ifdef L_modsi3
	STM8_DIVSI __modsi3 1 1
#endif
