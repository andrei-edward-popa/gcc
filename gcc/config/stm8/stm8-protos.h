/* Prototypes for the STM8 backend.
   Copyright (C) 2026 Free Software Foundation, Inc.

   This file is part of GCC.

   GCC is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published
   by the Free Software Foundation; either version 3, or (at your
   option) any later version.

   GCC is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GCC; see the file COPYING3.  If not see
   <http://www.gnu.org/licenses/>.  */

#ifndef GCC_STM8_PROTOS_H
#define GCC_STM8_PROTOS_H

extern HOST_WIDE_INT stm8_initial_elimination_offset (int, int);

#ifdef RTX_CODE
extern rtx stm8_return_addr_rtx (int, rtx);
#endif

extern void stm8_init_cumulative_args (CUMULATIVE_ARGS *, tree, rtx, tree, int);
extern void stm8_init_cumulative_libcall_args (CUMULATIVE_ARGS *, machine_mode);

extern void stm8_expand_prologue (void);
extern void stm8_expand_epilogue (void);

extern void stm8_split_movhi_mem (rtx, rtx, rtx);
extern rtx stm8_prepare_byte_rhs (rtx);
extern rtx stm8_assign_stack_temp (machine_mode);

extern bool stm8_storehi_cross_index_p (rtx, rtx);
extern bool stm8_loadhi_index_from_sp_p (rtx, rtx);
extern bool stm8_loadhi_same_index_p (rtx, rtx);

extern void stm8_emit_stack_adjust (HOST_WIDE_INT, bool);
extern void stm8_expand_push (rtx, machine_mode);
extern void stm8_expand_cpymem (rtx, rtx, rtx);
extern void stm8_expand_integer_convert (rtx, rtx, machine_mode, bool);
extern const char *stm8_output_word_alu (rtx *, bool);
extern bool stm8_far_sp_memory_operand_p (rtx);
extern void stm8_output_byte_move (rtx, rtx);

#endif /* GCC_STM8_PROTOS_H.  */
