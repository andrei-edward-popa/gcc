/* GCC backend definitions for the STM8 processor.
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

/* Run-time target specification.  */

#define TARGET_CPU_CPP_BUILTINS()					       \
  do									       \
    {									       \
      builtin_define ("__STM8__");					       \
      if (TARGET_LARGE)							       \
	builtin_define ("__STM8_LARGE__");				       \
      builtin_assert ("cpu=stm8");					       \
    }									       \
  while (0)

/* Storage layout.  */

#define BITS_BIG_ENDIAN 0
#define BYTES_BIG_ENDIAN 1
#define WORDS_BIG_ENDIAN 1

#ifdef IN_LIBGCC2
#define UNITS_PER_WORD 4
#else
#define UNITS_PER_WORD 1
#endif

#define SHORT_TYPE_SIZE 16
#define INT_TYPE_SIZE 16
#define LONG_TYPE_SIZE 32
#define LONG_LONG_TYPE_SIZE 64

#define DEFAULT_SIGNED_CHAR 0

#define STRICT_ALIGNMENT 0
#define FUNCTION_BOUNDARY 8
#define BIGGEST_ALIGNMENT 8
#define STACK_BOUNDARY 8
#define PARM_BOUNDARY 8

#define STACK_GROWS_DOWNWARD 1
#define STACK_PUSH_CODE POST_DEC
#define FRAME_GROWS_DOWNWARD 1

#define STACK_POINTER_OFFSET 1
#define ACCUMULATE_OUTGOING_ARGS 1
#define FIRST_PARM_OFFSET(FUNDECL) 0
/* CFA is the caller's SP, while AP points one byte beyond the return PC
   because STM8 pushes post-decrement.  */
#define INCOMING_FRAME_SP_OFFSET STM8_RETURN_ADDRESS_SIZE
#define ARG_POINTER_CFA_OFFSET(FUNDECL) (-1)

#define Pmode HImode
#define STACK_SIZE_MODE Pmode
#define POINTER_SIZE 16

#undef SIZE_TYPE
#define SIZE_TYPE "unsigned int"

#undef PTRDIFF_TYPE
#define PTRDIFF_TYPE "int"

#undef USER_LABEL_PREFIX
#define USER_LABEL_PREFIX "_"

#define TARGET_LARGE (stm8_memory_model == STM8_MODEL_LARGE)
#define FUNCTION_MODE (TARGET_LARGE ? PSImode : HImode)
#define STM8_RETURN_ADDRESS_SIZE (TARGET_LARGE ? 3 : 2)
/* DWARF code addresses must cover the complete far address space, while
   DW_AT_byte_size still describes 16-bit object and 24-bit code pointers.  */
#define DWARF2_ADDR_SIZE (TARGET_LARGE ? 4 : 2)
/* Direct far calls must retain the executable symbol, rather than
   manufacturing an indirect call to save a repeated address load.  */
#define NO_FUNCTION_CSE TARGET_LARGE

#define MOVE_MAX 2
#define SLOW_BYTE_ACCESS 0

/* Register usage.  */

#define A_REGNUM 0
#define YH_REGNUM 1
#define YL_REGNUM 2
#define XH_REGNUM 3
#define XL_REGNUM 4
#define STACK_POINTER_REGNUM 5
#define CC_REGNUM 6
#define ARG_POINTER_REGNUM 7
#define FRAME_POINTER_REGNUM 8

/* Fixed frames eliminate FP to SP.  Dynamic frames use a memory-backed
   FP, leaving Y:X available for the SDCC 24/32-bit return convention.  */
#define HARD_FRAME_POINTER_REGNUM FRAME_POINTER_REGNUM

#define HARD_FRAME_POINTER_IS_FRAME_POINTER 1
#define HARD_FRAME_POINTER_IS_ARG_POINTER 0

#define FIRST_PSEUDO_REGISTER 9

#define REGISTER_NAMES { "a", "yh", "yl", "xh", "xl", "sp", "cc", "argp", "fp" }

#define FIXED_REGISTERS { 0, 0, 0, 0, 0, 1, 1, 1, 1 }

#define CALL_REALLY_USED_REGISTERS { 1, 1, 1, 1, 1, 0, 1, 0, 0 }

enum reg_class
{
  NO_REGS,
  A_REGS,
  X_REGS,
  Y_REGS,
  POINTER_REGS,
  BASE_REGS,
  GENERAL_REGS,
  SP_REGS,
  CC_REGS,
  AP_REGS,
  FP_REGS,
  ALL_REGS,
  LIM_REG_CLASSES
};

#define N_REG_CLASSES ((int) LIM_REG_CLASSES)

#define REG_CLASS_NAMES							       \
  { "NO_REGS",      "A_REGS",    "X_REGS",       "Y_REGS",		       \
    "POINTER_REGS", "BASE_REGS", "GENERAL_REGS", "SP_REGS",		       \
    "CC_REGS",      "AP_REGS",   "FP_REGS",      "ALL_REGS" }

#define REG_CLASS_CONTENTS						       \
  {									       \
    { 0x000 }, /* NO_REGS.  */						       \
    { 0x001 }, /* A_REGS.  */						       \
    { 0x018 }, /* X_REGS.  */						       \
    { 0x006 }, /* Y_REGS.  */						       \
    { 0x01e }, /* POINTER_REGS.  */					       \
    { 0x13e }, /* BASE_REGS.  */					       \
    { 0x01f }, /* GENERAL_REGS.  */					       \
    { 0x020 }, /* SP_REGS.  */						       \
    { 0x040 }, /* CC_REGS.  */						       \
    { 0x080 }, /* AP_REGS.  */						       \
    { 0x100 }, /* FP_REGS.  */						       \
    { 0x1ff }  /* ALL_REGS.  */						       \
  }

#define REGNO_REG_CLASS(REGNO)						       \
  ((REGNO) == A_REGNUM				    ? A_REGS		       \
   : ((REGNO) == XH_REGNUM || (REGNO) == XL_REGNUM) ? X_REGS		       \
   : ((REGNO) == YH_REGNUM || (REGNO) == YL_REGNUM) ? Y_REGS		       \
   : (REGNO) == STACK_POINTER_REGNUM		    ? SP_REGS		       \
   : (REGNO) == CC_REGNUM			    ? CC_REGS		       \
   : (REGNO) == ARG_POINTER_REGNUM		    ? AP_REGS		       \
   : (REGNO) == FRAME_POINTER_REGNUM		    ? FP_REGS		       \
						    : NO_REGS)

#define ELIMINABLE_REGS							       \
  {									       \
    { ARG_POINTER_REGNUM, STACK_POINTER_REGNUM },			       \
    { ARG_POINTER_REGNUM, FRAME_POINTER_REGNUM },			       \
    { FRAME_POINTER_REGNUM, STACK_POINTER_REGNUM },			       \
  }
#define INITIAL_ELIMINATION_OFFSET(FROM, TO, OFFSET)			       \
  ((OFFSET) = stm8_initial_elimination_offset ((FROM), (TO)))

#define RETURN_ADDR_RTX(COUNT, FRAMEADDR)				       \
  stm8_return_addr_rtx ((COUNT), (FRAMEADDR))

enum stm8_arg_reg
{
  STM8_ARG_REG_UNSEEN,
  STM8_ARG_REG_STACK,
  STM8_ARG_REG_A,
  STM8_ARG_REG_X
};

typedef struct
{
  unsigned int arg_number;
  enum stm8_arg_reg first_arg_reg;
  bool variadic;
  bool sret_pending;
} CUMULATIVE_ARGS;

#define INIT_CUMULATIVE_ARGS(CUM, FNTYPE, LIBNAME, FNDECL, N_NAMED_ARGS)       \
  stm8_init_cumulative_args (&(CUM), (FNTYPE), (LIBNAME), (FNDECL),	       \
			     (N_NAMED_ARGS))

#define INIT_CUMULATIVE_LIBCALL_ARGS(CUM, MODE, LIBNAME)		       \
  stm8_init_cumulative_libcall_args (&(CUM), (MODE))

#define FUNCTION_ARG_REGNO_P(REGNO)					       \
  ((REGNO) == A_REGNUM || (REGNO) == XH_REGNUM || (REGNO) == XL_REGNUM)

#define DEFAULT_PCC_STRUCT_RETURN 0

/* Every dynamic frame uses the software FP.  Its epilogue restores SP
   from that stable base, so a redundant SAVE_FUNCTION stack restore
   must not consume a return register before the epilogue.  */
#define EXIT_IGNORE_STACK 1

/* Addressing modes.  */

#define MAX_REGS_PER_ADDRESS 1
#define INDEX_REG_CLASS NO_REGS
#define REGNO_OK_FOR_INDEX_P(REGNO) 0

#define STM8_REGNO_OK_FOR_BASE_P(REGNO)					       \
  ((REGNO) == XH_REGNUM || (REGNO) == YH_REGNUM				       \
   || (REGNO) == STACK_POINTER_REGNUM || (REGNO) == ARG_POINTER_REGNUM	       \
   || (REGNO) == FRAME_POINTER_REGNUM)

#ifdef REG_OK_STRICT

#define REGNO_OK_FOR_BASE_P(REGNO)					       \
  (STM8_REGNO_OK_FOR_BASE_P (REGNO)					       \
   || ((REGNO) >= FIRST_PSEUDO_REGISTER && reg_renumber != NULL		       \
       && reg_renumber[(REGNO)] >= 0					       \
       && STM8_REGNO_OK_FOR_BASE_P (reg_renumber[(REGNO)])))

#else

#define REGNO_OK_FOR_BASE_P(REGNO)					       \
  (STM8_REGNO_OK_FOR_BASE_P (REGNO) || (REGNO) >= FIRST_PSEUDO_REGISTER)

#endif

/* Nested functions.  */

#define STATIC_CHAIN_REGNUM YH_REGNUM

#define TRAMPOLINE_SIZE 7

/* Assembler output.  */

#define GLOBAL_ASM_OP "\t.global\t"

#define TEXT_SECTION_ASM_OP "\t.text"
#define DATA_SECTION_ASM_OP "\t.data"
#define BSS_SECTION_ASM_OP "\t.section\t.bss"
#define READONLY_DATA_SECTION_ASM_OP "\t.section\t.rodata"

#define ASM_APP_ON "#APP\n"
#define ASM_APP_OFF "#NOAPP\n"

#define ASM_OUTPUT_ALIGN(STREAM, LOG)					       \
  do									       \
    {									       \
      if ((LOG) == 0)							       \
	break;								       \
      fprintf ((STREAM), "\t.balign %d\n", 1 << (LOG));			       \
    }									       \
  while (0)

/* Switch tables.  */

#define CASE_VECTOR_MODE Pmode

/* Function profiling is not implemented yet.  */

#define FUNCTION_PROFILER(FILE, LABELNO)				       \
  do									       \
    {									       \
    }									       \
  while (0)

#define PUSH_ROUNDING(BYTES) (BYTES)
