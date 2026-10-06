/* STM8 large-model indirect call helper.
   Copyright (C) 2026 Free Software Foundation, Inc.

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

/* Y points to a big-endian 24-bit PC.  Preserve A/X argument registers
   and replace the helper transfer with the callee address, so RETF
   returns directly to the original caller.  */

#ifdef __STM8_LARGE__
	.stm8_model "large"
	.section .text.__stm8_call_indirect, "ax", @progbits
	.global ___stm8_call_indirect
___stm8_call_indirect:
	push a
	ld a, (y)
	ldw y, (1, y)
	pushw y
	push a
	ld a, (4, sp)
	ld yh, a
	ld a, (3, sp)
	ld (4, sp), a
	ld a, (2, sp)
	ld (3, sp), a
	ld a, (1, sp)
	ld (2, sp), a
	ld a, yh
	addw sp, #1
	retf
#endif
