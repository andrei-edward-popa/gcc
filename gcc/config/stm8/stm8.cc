/* Subroutines used for code generation on STM8 processors.
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

#define IN_TARGET_CODE 1

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "diagnostic-core.h"
#include "target.h"
#include "targhooks.h"
#include "rtl.h"
#include "tree.h"
#include "stringpool.h"
#include "attribs.h"
#include "memmodel.h"
#include "tm_p.h"
#include "regs.h"
#include "insn-codes.h"
#include "emit-rtl.h"
#include "expr.h"
#include "explow.h"
#include "flags.h"
#include "function.h"
#include "calls.h"
#include "stor-layout.h"
#include "output.h"
#include "builtins.h"

/* This file should be included last.  */
#include "target-def.h"

/* A callee-preserved, memory-backed register for dynamic frames.  Common
   linkage provides one slot per program.  Task switching must preserve
   this slot just like a callee-preserved hardware register.  */
static bool stm8_uses_soft_frame_pointer;
static auto_vec<int> stm8_addressed_labels;

/* GNU label values have type void *, hence the near object-pointer mode.
   Represent them by near descriptors containing the complete code PC.
   Direct branches retain their ordinary CODE_LABEL references.  */
static rtx
stm8_label_pointer_constant (rtx x)
{
  if (!TARGET_LARGE)
    return x;
  if (GET_CODE (x) == LABEL_REF && GET_MODE (x) == Pmode)
    {
      int number = CODE_LABEL_NUMBER (label_ref_label (x));
      if (!stm8_addressed_labels.contains (number))
	stm8_addressed_labels.safe_push (number);
      char name[64];
      ASM_GENERATE_INTERNAL_LABEL (name, "stm8_labelptr", number);
      return gen_rtx_SYMBOL_REF (Pmode, ggc_strdup (name));
    }
  if (GET_CODE (x) == CONST)
    return gen_rtx_CONST (GET_MODE (x),
			  stm8_label_pointer_constant (XEXP (x, 0)));
  if (GET_CODE (x) == PLUS || GET_CODE (x) == MINUS)
    return gen_rtx_fmt_ee (GET_CODE (x), GET_MODE (x),
			   stm8_label_pointer_constant (XEXP (x, 0)),
			   stm8_label_pointer_constant (XEXP (x, 1)));
  return x;
}

static void
stm8_asm_file_end (void)
{
  if (stm8_uses_soft_frame_pointer)
    fputs ("\t.comm __stm8_frame_pointer,2,1\n", asm_out_file);
  for (int number : stm8_addressed_labels)
    {
      /* Keep descriptors together so GNU label differences can be
	 resolved by the assembler, including difference-based jump
	 tables used with computed goto.  */
      fprintf (asm_out_file,
	       "\t.section .rodata.stm8_labelptr,\"a\",@progbits\n");
      targetm.asm_out.internal_label (asm_out_file, "stm8_labelptr", number);
      char name[64];
      ASM_GENERATE_INTERNAL_LABEL (name, "L", number);
      fputs ("\t.3byte\t", asm_out_file);
      assemble_name (asm_out_file, name);
      fputc ('\n', asm_out_file);
    }
  stm8_addressed_labels.truncate (0);
}

static void
stm8_asm_file_start (void)
{
  default_file_start ();
  fprintf (asm_out_file, "\t.stm8_model \"%s\"\n",
	   TARGET_LARGE ? "large" : "medium");
}

/* Return the number of consecutive hard registers needed to hold MODE
   starting at REGNO.  */

static unsigned int
stm8_hard_regno_nregs (unsigned int regno, machine_mode mode)
{
  if (regno == STACK_POINTER_REGNUM || regno == ARG_POINTER_REGNUM
      || regno == FRAME_POINTER_REGNUM)
    return 1;

  if (regno == CC_REGNUM && GET_MODE_CLASS (mode) == MODE_CC)
    return 1;

  return CEIL (GET_MODE_SIZE (mode), UNITS_PER_WORD);
}

/* Return true if hard register REGNO can hold a value of MODE.  */

static bool
stm8_hard_regno_mode_ok (unsigned int regno, machine_mode mode)
{
  switch (regno)
    {
    case A_REGNUM:
      return mode == QImode;

    case YH_REGNUM:
      return mode == QImode || mode == HImode || mode == CQImode
	     || mode == CHImode || mode == SImode || mode == SFmode;

    case YL_REGNUM:
      return mode == QImode || mode == PSImode;

    case XH_REGNUM:
      return mode == QImode || mode == HImode || mode == CQImode;

    case XL_REGNUM:
      return mode == QImode;

    case STACK_POINTER_REGNUM:
    case ARG_POINTER_REGNUM:
    case FRAME_POINTER_REGNUM:
      return mode == HImode;

    case CC_REGNUM:
      return GET_MODE_CLASS (mode) == MODE_CC;

    default:
      return false;
    }
}

/* Byte moves between index-register halves require two LD instructions
   through A.  A cost of 2 tells reload that every move between the two
   classes satisfies the movqi constraints, which is false here.  */

static int
stm8_register_move_cost (machine_mode mode, reg_class_t from, reg_class_t to)
{
  if (mode == QImode && from != A_REGS && to != A_REGS)
    return 4;

  return 2;
}

/* Byte transfers involving index-register halves require A.  A late
   caller-save restore may overlap an A-only input reload, so the byte
   copy can instead preserve A in another scratch byte.  Word spills
   use the same preservation when their copy needs A.  */

static reg_class_t
stm8_secondary_reload (bool in_p ATTRIBUTE_UNUSED, rtx x, reg_class_t rclass,
		       machine_mode mode, secondary_reload_info *sri)
{
  if (rclass == NO_REGS)
    return NO_REGS;

  if (mode == QImode
      && reg_class_subset_p ((enum reg_class) rclass, POINTER_REGS))
    {
      if (register_operand (x, mode))
	{
	  if (REG_P (x) && REGNO (x) == A_REGNUM)
	    return NO_REGS;
	  if (reg_renumber == nullptr)
	    return NO_REGS;
	  if (true_regnum (x) == A_REGNUM)
	    return NO_REGS;
	}
      sri->icode = CODE_FOR_stm8_reloadqi;
      if (!register_operand (x, mode) && !MEM_P (x))
	sri->extra_cost = 2;
      return NO_REGS;
    }

  if (mode != HImode
      || !reg_class_subset_p ((enum reg_class) rclass, POINTER_REGS))
    return NO_REGS;

  if (register_operand (x, mode))
    {
      /* Before allocation there is no spill location to inspect.  */
      if (reg_renumber == nullptr || true_regnum (x) >= 0)
	return NO_REGS;
    }
  else if (!MEM_P (x))
    return NO_REGS;

  sri->icode = CODE_FOR_stm8_reloadhi;
  return NO_REGS;
}

/* Return the initial offset between two eliminable frame registers.  */

static unsigned int
stm8_frame_save_size (void)
{
  return frame_pointer_needed ? 2 + (cfun->static_chain_decl ? 2 : 0) : 0;
}

HOST_WIDE_INT
stm8_initial_elimination_offset (int from, int to)
{
  HOST_WIDE_INT frame_size = get_frame_size () + crtl->outgoing_args_size;

  if (from == FRAME_POINTER_REGNUM)
    {
      if (to == STACK_POINTER_REGNUM)
	return frame_size;
    }

  if (from == ARG_POINTER_REGNUM)
    {
      if (to == STACK_POINTER_REGNUM)
	return frame_size + STM8_RETURN_ADDRESS_SIZE + 1
	       + stm8_frame_save_size ();
      if (to == FRAME_POINTER_REGNUM)
	return STM8_RETURN_ADDRESS_SIZE + 1 + stm8_frame_save_size ();
    }

  gcc_unreachable ();
}

/* Return true if FROM may be eliminated in favor of TO.  */

static bool
stm8_can_eliminate (const int from ATTRIBUTE_UNUSED, const int to)
{
  if (to == STACK_POINTER_REGNUM)
    return !frame_pointer_needed;

  return to == FRAME_POINTER_REGNUM;
}

/* Return the offset from the frame pointer to the end of the first
   local stack slot.  */
static HOST_WIDE_INT
stm8_starting_frame_offset (void)
{
  return 1;
}

/* Return the address to which the current function will return.
   Only the current frame is supported.  */

rtx
stm8_return_addr_rtx (int count, rtx frameaddr ATTRIBUTE_UNUSED)
{
  if (count != 0)
    return NULL_RTX;

  /* AP denotes the first argument byte in both fixed and dynamic frames.
     The two-byte near return address immediately precedes it.  */
  return gen_frame_mem (Pmode, plus_constant (Pmode, arg_pointer_rtx, -2));
}

/* Private integer snapshots have one known type per mode.  GCC can reuse
   released slots only when their types must conflict; untyped slots cannot
   satisfy that condition.  Callers keep a slot live until every RTL use,
   including the final result copy, has been emitted.  */
rtx
stm8_assign_stack_temp (machine_mode mode)
{
  gcc_assert (SCALAR_INT_MODE_P (mode));
  tree type = build_nonstandard_integer_type (GET_MODE_BITSIZE (mode), 1);
  return assign_stack_temp_for_type (mode, GET_MODE_SIZE (mode), type);
}

/* PSI occupies three bytes: a HI lowpart at byte one is not a valid
   register subreg.  Snapshot both sides and address individual bytes so
   conversions also work for overlapping operands and volatile sources.  */
void
stm8_expand_integer_convert (rtx dst, rtx src, machine_mode from_mode,
			     bool sign_extend_p)
{
  push_temp_slots ();
  machine_mode to_mode = GET_MODE (dst);
  unsigned int from_size = GET_MODE_SIZE (from_mode);
  unsigned int to_size = GET_MODE_SIZE (to_mode);
  unsigned int copy_size = MIN (from_size, to_size);
  rtx input = stm8_assign_stack_temp (from_mode);
  rtx output = stm8_assign_stack_temp (to_mode);
  emit_move_insn (input, src);
  rtx fill = const0_rtx;
  if (sign_extend_p && to_size > from_size)
    {
      fill = gen_reg_rtx (QImode);
      emit_move_insn (fill, adjust_address (input, QImode, 0));
      emit_insn (gen_ashrqi3 (fill, fill, GEN_INT (7)));
    }
  for (unsigned int i = 0; i < to_size - copy_size; ++i)
    emit_move_insn (adjust_address (output, QImode, i), fill);
  for (unsigned int i = 0; i < copy_size; ++i)
    emit_move_insn (adjust_address (output, QImode, to_size - copy_size + i),
		    adjust_address (input, QImode, from_size - copy_size + i));
  emit_move_insn (dst, output);
  pop_temp_slots ();
}

static bool
stm8_return_in_memory_1 (const_tree type)
{
  if (type == NULL_TREE || VOID_TYPE_P (type))
    return false;

  if (AGGREGATE_TYPE_P (type) || TYPE_MODE (type) == BLKmode)
    return true;

  HOST_WIDE_INT size = int_size_in_bytes (type);

  return size < 0 || size > 4;
}

static bool
stm8_return_in_memory (const_tree type, const_tree fntype ATTRIBUTE_UNUSED)
{
  return stm8_return_in_memory_1 (type);
}

void
stm8_init_cumulative_args (CUMULATIVE_ARGS *cum, tree fntype,
			   rtx libname ATTRIBUTE_UNUSED,
			   tree fndecl ATTRIBUTE_UNUSED,
			   int n_named_args ATTRIBUTE_UNUSED)
{
  cum->arg_number = 0;
  cum->first_arg_reg = STM8_ARG_REG_UNSEEN;
  cum->variadic = false;
  cum->sret_pending = false;

  if (fntype == NULL_TREE)
    return;

  cum->variadic = stdarg_p (fntype);

  tree return_type = TREE_TYPE (fntype);

  cum->sret_pending = stm8_return_in_memory_1 (return_type);
}

/* Generic libcalls also return large scalars through the first stack
   pointer.  It does not consume the first explicit argument position.  */
void
stm8_init_cumulative_libcall_args (CUMULATIVE_ARGS *cum, machine_mode mode)
{
  stm8_init_cumulative_args (cum, NULL_TREE, NULL_RTX, NULL_TREE, 0);
  cum->sret_pending = mode != VOIDmode && GET_MODE_SIZE (mode) > 4;
}

static enum stm8_arg_reg
stm8_function_arg_reg (const CUMULATIVE_ARGS *cum, const function_arg_info &arg)
{
  if (cum->sret_pending)
    return STM8_ARG_REG_STACK;

  if (cum->variadic)
    return STM8_ARG_REG_STACK;

  if (arg.type != NULL_TREE && AGGREGATE_TYPE_P (arg.type))
    return STM8_ARG_REG_STACK;

  HOST_WIDE_INT size;

  if (arg.type != NULL_TREE)
    size = int_size_in_bytes (arg.type);
  else if (arg.mode == QImode)
    size = 1;
  else if (arg.mode == HImode)
    size = 2;
  else
    return STM8_ARG_REG_STACK;

  if (cum->arg_number == 0)
    {
      if (size == 1)
	return STM8_ARG_REG_A;

      if (size == 2)
	return STM8_ARG_REG_X;

      return STM8_ARG_REG_STACK;
    }

  if (cum->arg_number == 1)
    {
      if (cum->first_arg_reg == STM8_ARG_REG_X && size == 1)
	return STM8_ARG_REG_A;

      if (cum->first_arg_reg == STM8_ARG_REG_A && size == 2)
	return STM8_ARG_REG_X;
    }

  return STM8_ARG_REG_STACK;
}

static rtx
stm8_function_arg (cumulative_args_t cum_v, const function_arg_info &arg)
{
  CUMULATIVE_ARGS *cum = get_cumulative_args (cum_v);

  if (arg.mode == VOIDmode)
    return NULL_RTX;

  switch (stm8_function_arg_reg (cum, arg))
    {
    case STM8_ARG_REG_A:
      return gen_rtx_REG (arg.mode, A_REGNUM);

    case STM8_ARG_REG_X:
      return gen_rtx_REG (arg.mode, XH_REGNUM);

    default:
      return NULL_RTX;
    }
}

static void
stm8_function_arg_advance (cumulative_args_t cum_v,
			   const function_arg_info &arg)
{
  CUMULATIVE_ARGS *cum = get_cumulative_args (cum_v);

  if (arg.mode == VOIDmode)
    return;

  if (cum->sret_pending)
    {
      cum->sret_pending = false;
      return;
    }

  enum stm8_arg_reg reg = stm8_function_arg_reg (cum, arg);

  if (cum->arg_number == 0)
    cum->first_arg_reg = reg;

  ++cum->arg_number;
}

static rtx
stm8_value_rtx (machine_mode mode)
{
  if (mode == QImode)
    return gen_rtx_REG (mode, A_REGNUM);

  if (mode == HImode || mode == CQImode)
    return gen_rtx_REG (mode, XH_REGNUM);

  if (mode == PSImode)
    return gen_rtx_REG (mode, YL_REGNUM);

  if (mode == SImode || mode == SFmode || mode == CHImode)
    return gen_rtx_REG (mode, YH_REGNUM);

  gcc_unreachable ();
}

static rtx
stm8_function_value (const_tree type,
		     const_tree fn_decl_or_type ATTRIBUTE_UNUSED,
		     bool outgoing ATTRIBUTE_UNUSED)
{
  return stm8_value_rtx (TYPE_MODE (type));
}

static rtx
stm8_libcall_value (machine_mode mode, const_rtx fun ATTRIBUTE_UNUSED)
{
  return stm8_value_rtx (mode);
}

static bool
stm8_function_value_regno_p (const unsigned int regno)
{
  return (regno == A_REGNUM || regno == XH_REGNUM || regno == YL_REGNUM
	  || regno == YH_REGNUM);
}

static poly_int64
stm8_return_pops_args (tree fundecl, tree funtype, poly_int64 size)
{
  if (TARGET_LARGE || known_eq (size, 0) || funtype == NULL_TREE)
    return 0;

  if (stdarg_p (funtype))
    return 0;

  tree return_type = TREE_TYPE (funtype);

  if (VOID_TYPE_P (return_type))
    return size;

  HOST_WIDE_INT return_size = int_size_in_bytes (return_type);

  if (return_size >= 0 && return_size <= 2)
    return size;

  /* SDCC also uses callee cleanup for the special float -> float
     calling case.  */
  if (TREE_CODE (return_type) == REAL_TYPE)
    {
      /* calls.cc gives libcalls a return type but no argument types.
	 These standard helpers have a floating first argument, and must
	 use the same SDCC cleanup rule as their C implementations.  */
      if (fundecl && TREE_CODE (fundecl) == IDENTIFIER_NODE)
	{
	  const char *name = IDENTIFIER_POINTER (fundecl);
	  static const char *const float_first[]
	      = { "__addsf3",  "__subsf3", "__mulsf3",      "__divsf3",
		  "__negsf2",  "__adddf3", "__subdf3",      "__muldf3",
		  "__divdf3",  "__negdf2", "__extendsfdf2", "__truncdfsf2",
		  "__powisf2", "__powidf2" };
	  for (const char *helper : float_first)
	    if (strcmp (name, helper) == 0)
	      return size;
	}
      tree args = TYPE_ARG_TYPES (funtype);

      if (args != NULL_TREE && TREE_VALUE (args) != NULL_TREE
	  && TREE_CODE (TREE_VALUE (args)) == REAL_TYPE)
	return size;
    }

  return 0;
}

static reg_class_t
stm8_base_reg_class (machine_mode mode ATTRIBUTE_UNUSED,
		     addr_space_t as ATTRIBUTE_UNUSED,
		     enum rtx_code outer_code ATTRIBUTE_UNUSED,
		     enum rtx_code index_code ATTRIBUTE_UNUSED,
		     rtx mem ATTRIBUTE_UNUSED, rtx_insn *insn ATTRIBUTE_UNUSED)
{
  return BASE_REGS;
}

/* Sources can be stack slots, or registers also used for their own
   address.  Read every component before starting the byte pushes.
   With the existing big-endian POST_DEC stack convention, the last
   component is pushed first and the first ends at SP + 1.  */

void
stm8_expand_push (rtx value, machine_mode mode)
{
  unsigned int size = GET_MODE_SIZE (mode);
  auto_vec<rtx, 8> bytes;

  for (unsigned int i = 0; i < size; ++i)
    {
      rtx part = value;
      machine_mode part_mode = mode;
      unsigned int offset = i;
      if (COMPLEX_MODE_P (mode))
	{
	  part_mode = GET_MODE_INNER (mode);
	  unsigned int part_size = GET_MODE_SIZE (part_mode);
	  part = read_complex_part (value, i >= part_size);
	  offset %= part_size;
	}
      rtx byte = operand_subword (part, offset, 1, part_mode);
      if (byte == NULL_RTX)
	{
	  gcc_assert (CONSTANT_P (part));
	  part = force_const_mem (part_mode, part);
	  byte = operand_subword (part, offset, 1, part_mode);
	}
      gcc_assert (byte != NULL_RTX);
      bytes.safe_push (CONST_INT_P (byte) ? byte
					  : copy_to_mode_reg (QImode, byte));
    }
  for (unsigned int i = size; i-- > 0;)
    emit_insn (gen_pushqi1 (bytes[i]));
}

/* Copy with a size_t-width counter.  The generic constant-size fallback
   uses word_mode, which is QI here and cannot count blocks above 255.
   Avoid a call: this also copies arguments into the outgoing argument
   area without overwriting the parameters of a memcpy call itself.  */
void
stm8_expand_cpymem (rtx dest, rtx src, rtx size)
{
  rtx d = copy_addr_to_reg (XEXP (dest, 0));
  rtx s = copy_addr_to_reg (XEXP (src, 0));
  rtx n = copy_to_mode_reg (HImode, size);
  rtx_code_label *loop = gen_label_rtx ();
  rtx_code_label *done = gen_label_rtx ();
  emit_jump_insn (gen_cbranchhi4 (gen_rtx_EQ (VOIDmode, n, const0_rtx), n,
				  const0_rtx, done));
  emit_label (loop);
  rtx dmem = gen_rtx_MEM (QImode, d);
  rtx smem = gen_rtx_MEM (QImode, s);
  MEM_VOLATILE_P (dmem) = MEM_VOLATILE_P (dest);
  MEM_VOLATILE_P (smem) = MEM_VOLATILE_P (src);
  emit_move_insn (dmem, smem);
  emit_insn (gen_addhi3 (d, d, const1_rtx));
  emit_insn (gen_addhi3 (s, s, const1_rtx));
  emit_insn (gen_addhi3 (n, n, constm1_rtx));
  emit_jump_insn (gen_cbranchhi4 (gen_rtx_NE (VOIDmode, n, const0_rtx), n,
				  const0_rtx, loop));
  emit_label (done);
}

/* LD does not change C, so the byte chain retains carry/borrow between
   components.  Early-clobber constraints exclude the destination from
   RHS address registers.  Preserve A if another scratch was allocated.  */
const char *
stm8_output_word_alu (rtx *operands, bool subtract_p)
{
  bool native_p = !MEM_P (operands[2]);
  if (!native_p)
    {
      rtx addr = XEXP (operands[2], 0);
      rtx base = GET_CODE (addr) == PLUS ? XEXP (addr, 0) : addr;
      native_p = CONSTANT_ADDRESS_P (addr)
		 || (REG_P (base) && REGNO (base) == STACK_POINTER_REGNUM);
    }
  if (native_p)
    return subtract_p ? "subw\t%0,%2" : "addw\t%0,%2";

  bool preserve_a = REGNO (operands[3]) != A_REGNUM;
  if (preserve_a)
    output_asm_insn ("ld\t%3,a", operands);
  rtx bytes[4] = { gen_rtx_REG (QImode, REGNO (operands[0]) + 1),
		   gen_rtx_REG (QImode, REGNO (operands[0])),
		   adjust_address (operands[2], QImode, 1),
		   adjust_address (operands[2], QImode, 0) };
  output_asm_insn ("ld\ta,%0", bytes);
  output_asm_insn (subtract_p ? "sub\ta,%2" : "add\ta,%2", bytes);
  output_asm_insn ("ld\t%0,a\n\tld\ta,%1", bytes);
  output_asm_insn (subtract_p ? "sbc\ta,%3" : "adc\ta,%3", bytes);
  output_asm_insn ("ld\t%1,a", bytes);
  if (preserve_a)
    output_asm_insn ("ld\ta,%3", operands);
  return "";
}

/* Return the hard register number corresponding to REG when it is used
   as an address register.  Return -1 if no suitable hard register is
   available yet.  Return FIRST_PSEUDO_REGISTER for a non-strict pseudo.  */

static int
stm8_address_regno (rtx reg, bool strict)
{
  gcc_assert (REG_P (reg));

  int regno = REGNO (reg);

  if (regno >= FIRST_PSEUDO_REGISTER)
    {
      if (!strict)
	return FIRST_PSEUDO_REGISTER;

      if (reg_renumber == nullptr || reg_renumber[regno] < 0)
	return -1;

      regno = reg_renumber[regno];
    }

  return regno;
}

static bool
stm8_address_reg_p (rtx reg, bool strict)
{
  if (!REG_P (reg))
    return false;

  int regno = stm8_address_regno (reg, strict);

  if (regno == FIRST_PSEUDO_REGISTER)
    return true;

  return (regno == XH_REGNUM || regno == YH_REGNUM
	  || regno == STACK_POINTER_REGNUM || regno == ARG_POINTER_REGNUM
	  || regno == FRAME_POINTER_REGNUM);
}

static bool
stm8_16bit_offset_p (HOST_WIDE_INT value)
{
  return value >= -32768 && value <= 65535;
}

static bool
stm8_legitimate_address_p (machine_mode mode, rtx x, bool strict,
			   code_helper ch ATTRIBUTE_UNUSED)
{
  /* Far code addresses are lowered to a private memory descriptor by
     the call expander before register allocation.  Object addresses
     remain HImode, including the address of that descriptor.  */
  if (TARGET_LARGE && mode == FUNCTION_MODE && GET_MODE (x) == PSImode)
    {
      if (GET_CODE (x) == UNSPEC && XINT (x, 1) == UNSPEC_FAR_CALL)
	return true;
      if (REG_P (x) || MEM_P (x))
	return false;
    }

  if (REG_P (x))
    return stm8_address_reg_p (x, strict);

  if (GET_CODE (x) == PLUS)
    {
      rtx base = XEXP (x, 0);
      rtx offset = XEXP (x, 1);

      if (!REG_P (base) && REG_P (offset))
	std::swap (base, offset);

      if (!stm8_address_reg_p (base, strict) || !CONST_INT_P (offset))
	return false;

      HOST_WIDE_INT disp = INTVAL (offset);
      int regno = stm8_address_regno (base, strict);

      /* Virtual stack addresses use negative displacements until frame
	 allocation.  They become positive SP offsets or software-FP
	 accesses, rather than native X/Y indexed instructions.  */
      if (!strict && REGNO (base) >= FIRST_VIRTUAL_REGISTER
	  && REGNO (base) <= LAST_VIRTUAL_REGISTER)
	return stm8_16bit_offset_p (disp);

      /* Before reload, a pseudo address will eventually be allocated
	 to X or Y.  */
      if (regno == FIRST_PSEUDO_REGISTER)
	return IN_RANGE (disp, 0, 65535);

      /* SP only has the short-offset addressing form.  */
      if (regno == STACK_POINTER_REGNUM)
	{
	  /* All components must remain addressable after reload.  A
	     byte-wise word store at (255,SP) would need (256,SP).  */
	  unsigned int size = mode == BLKmode ? 1 : GET_MODE_SIZE (mode);
	  size = MAX (size, 1U);
	  /* Byte/word moves synthesize long SP accesses while saving
	     their temporary index register.  Other instructions use
	     constraint R and reload the address into X/Y.  */
	  return disp >= 0 && disp <= 65536 - size;
	}

      /* The argument pointer is eliminated before final output.  */
      if (regno == ARG_POINTER_REGNUM)
	return stm8_16bit_offset_p (disp);

      /* Software-FP moves normalize their signed offsets with ADDW
	 while preserving the temporary index register.  They do not
	 emit native indexed addressing with a negative displacement.  */
      if (regno == FRAME_POINTER_REGNUM)
	return stm8_16bit_offset_p (disp);

      /* Indexed offsets are unsigned and the sum addresses up to
	 128 KiB (PM0044 section 6.4.4).  Encoding -2 as 0xfffe would
	 access another bank rather than subtracting from a near
	 pointer.  Let GCC form negative offsets with HImode ADDW.  */
      return IN_RANGE (disp, 0, 65535);
    }

  switch (GET_CODE (x))
    {
    case CONST_INT:
      return stm8_16bit_offset_p (INTVAL (x));

    case SYMBOL_REF:
    case LABEL_REF:
    case CONST:
      return true;

    default:
      return false;
    }
}

bool
stm8_far_sp_memory_operand_p (rtx op)
{
  if (!MEM_P (op))
    return false;
  rtx addr = XEXP (op, 0);
  rtx base = addr, offset = const0_rtx;
  if (GET_CODE (addr) == PLUS)
    {
      base = XEXP (addr, 0);
      offset = XEXP (addr, 1);
    }
  if (!REG_P (base) && REG_P (offset))
    std::swap (base, offset);
  if (!REG_P (base) || !CONST_INT_P (offset))
    return false;
  if (REGNO (base) == FRAME_POINTER_REGNUM)
    return true;
  if (REGNO (base) != STACK_POINTER_REGNUM)
    return false;
  unsigned int size
      = GET_MODE (op) == BLKmode ? 1 : GET_MODE_SIZE (GET_MODE (op));
  return INTVAL (offset) + MAX (size, 1U) > 256;
}

/* No allocatable word register may be free while spilling a Y:X result.
   Preserve X inside the atomic byte move instead of making LRA allocate
   a third word register.  The address compensates for the temporary push;
   A and all index-register halves retain exactly the RTL move's values.  */
void
stm8_output_byte_move (rtx dest, rtx src)
{
  rtx operands[2] = { dest, src };
  rtx mem = MEM_P (dest) ? dest : src;
  if (!stm8_far_sp_memory_operand_p (mem))
    {
      output_asm_insn ("ld\t%0,%1", operands);
      return;
    }
  rtx addr = XEXP (mem, 0), base = addr, disp = const0_rtx;
  if (GET_CODE (addr) == PLUS)
    {
      base = XEXP (addr, 0);
      disp = XEXP (addr, 1);
      if (!REG_P (base))
	std::swap (base, disp);
    }
  bool fp_p = REGNO (base) == FRAME_POINTER_REGNUM;
  rtx offset[1] = { GEN_INT ((INTVAL (disp) + (fp_p ? 0 : 2)) & 65535) };
  output_asm_insn ("pushw\tx", operands);
  output_asm_insn (fp_p ? "ldw\tx,__stm8_frame_pointer" : "ldw\tx,sp",
		   operands);
  if (offset[0] != const0_rtx)
    output_asm_insn ("addw\tx,%0", offset);
  output_asm_insn (MEM_P (dest) ? "ld\t(x),a" : "ld\ta,(x)", operands);
  output_asm_insn ("popw\tx", operands);
}

static void
stm8_print_register (FILE *file, unsigned int regno, machine_mode mode)
{
  if (mode == QImode)
    {
      switch (regno)
	{
	case A_REGNUM:
	  fputs ("a", file);
	  return;

	case YH_REGNUM:
	  fputs ("yh", file);
	  return;

	case YL_REGNUM:
	  fputs ("yl", file);
	  return;

	case XH_REGNUM:
	  fputs ("xh", file);
	  return;

	case XL_REGNUM:
	  fputs ("xl", file);
	  return;

	default:
	  break;
	}
    }

  if (mode == HImode)
    {
      switch (regno)
	{
	case YH_REGNUM:
	  fputs ("y", file);
	  return;

	case XH_REGNUM:
	  fputs ("x", file);
	  return;

	case STACK_POINTER_REGNUM:
	  fputs ("sp", file);
	  return;

	case FRAME_POINTER_REGNUM:
	  fputs ("__stm8_frame_pointer", file);
	  return;

	default:
	  break;
	}
    }

  output_operand_lossage ("invalid STM8 register");
}

static void
stm8_print_address_register (FILE *file, rtx reg)
{
  gcc_assert (REG_P (reg));

  switch (REGNO (reg))
    {
    case XH_REGNUM:
      fputs ("x", file);
      return;

    case YH_REGNUM:
      fputs ("y", file);
      return;

    case STACK_POINTER_REGNUM:
      fputs ("sp", file);
      return;

    default:
      output_operand_lossage ("invalid STM8 address register");
    }
}

static void
stm8_print_operand_address (FILE *file, machine_mode mode ATTRIBUTE_UNUSED,
			    rtx addr)
{
  if (REG_P (addr))
    {
      if (REGNO (addr) == STACK_POINTER_REGNUM)
	{
	  fputs ("(0,sp)", file);
	  return;
	}

      fputc ('(', file);
      stm8_print_address_register (file, addr);
      fputc (')', file);
      return;
    }

  if (GET_CODE (addr) == PLUS)
    {
      rtx base = XEXP (addr, 0);
      rtx offset = XEXP (addr, 1);

      if (!REG_P (base) && REG_P (offset))
	std::swap (base, offset);

      gcc_assert (REG_P (base));

      fputc ('(', file);
      output_addr_const (file, offset);
      fputc (',', file);
      stm8_print_address_register (file, base);
      fputc (')', file);
      return;
    }

  switch (GET_CODE (addr))
    {
    case CONST_INT:
    case SYMBOL_REF:
    case LABEL_REF:
    case CONST:
      output_addr_const (file, addr);
      return;

    default:
      output_operand_lossage ("invalid STM8 address");
      return;
    }
}

static void
stm8_print_operand (FILE *file, rtx x, int code)
{
  if (code == 'c' && CONSTANT_P (x))
    {
      output_addr_const (file, stm8_label_pointer_constant (x));
      return;
    }
  if (code == 'J')
    {
      fputs (TARGET_LARGE ? "jpf" : "jp", file);
      return;
    }
  if (code == 'b' && REG_P (x) && GET_MODE (x) == HImode)
    {
      stm8_print_register (file, REGNO (x) + 1, QImode);
      return;
    }
  if (code != 0)
    {
      output_operand_lossage ("invalid STM8 operand modifier");
      return;
    }

  if (REG_P (x))
    {
      stm8_print_register (file, REGNO (x), GET_MODE (x));
      return;
    }

  if (SUBREG_P (x) && REG_P (SUBREG_REG (x)))
    {
      stm8_print_register (file, subreg_regno (x), GET_MODE (x));
      return;
    }

  if (MEM_P (x))
    {
      stm8_print_operand_address (file, GET_MODE (x), XEXP (x, 0));
      return;
    }

  if (CONSTANT_P (x))
    {
      fputc ('#', file);
      output_addr_const (file, stm8_label_pointer_constant (x));
      return;
    }

  output_operand_lossage ("invalid STM8 operand");
}

/* Return the machine mode used for the C floating-point types.  STM8
   follows the SDCC ABI, where float, double and long double are all
   32-bit types.  */

static machine_mode
stm8_c_mode_for_floating_type (enum tree_index ti)
{
  switch (ti)
    {
    case TI_FLOAT_TYPE:
    case TI_DOUBLE_TYPE:
    case TI_LONG_DOUBLE_TYPE:
      return SFmode;

    default:
      gcc_unreachable ();
    }
}

/* PSI is the declared 24-bit integer extension.  DF can be transferred
   in memory even though the established C floating types use SF.  */

static bool
stm8_scalar_mode_supported_p (scalar_mode mode)
{
  return mode == PSImode || mode == DFmode
	 || default_scalar_mode_supported_p (mode);
}

/* convert_modes also has a shortcut before convert_move's trunc optab.
   A HI lowpart of PSI, and a PSI lowpart of SI/DI, are misaligned
   register subregs on this big-endian target.  Use the byte conversions.  */
static bool
stm8_truly_noop_truncation (poly_uint64 outprec, poly_uint64 inprec)
{
  return !((known_eq (inprec, 24) && known_eq (outprec, 16))
	   || (known_eq (outprec, 24) && known_gt (inprec, outprec)));
}

/* Keep a constant SP adjustment in one RTL insn.  expand_binop may retry
   a multi-insn expansion with a pseudo destination when its REG_EQUAL note
   would overlap the input.  Copying that pseudo back to SP prevents LRA
   from tracking its offset and forces a physical frame pointer.  */
void
stm8_emit_stack_adjust (HOST_WIDE_INT amount, bool frame_related_p)
{
  if (amount == 0)
    return;
  rtx_insn *insn = emit_insn (amount > 0 ? gen_stm8_add_sp (GEN_INT (amount))
					 : gen_stm8_sub_sp (GEN_INT (amount)));
  if (frame_related_p)
    RTX_FRAME_RELATED_P (insn) = 1;
}

/* Expand the function epilogue.  */

void
stm8_expand_epilogue (void)
{
  HOST_WIDE_INT frame_size = get_frame_size () + crtl->outgoing_args_size;
  HOST_WIDE_INT pops_args;

  gcc_assert (crtl->args.pops_args.is_constant (&pops_args));
  gcc_assert (pops_args >= 0);

  /*
   * First discard the local frame.  After this, SP has exactly its
   * value from function entry and the return address is at SP + 1.  */
  if (frame_pointer_needed)
    {
      rtx x = gen_rtx_REG (HImode, XH_REGNUM);
      rtx y = gen_rtx_REG (HImode, YH_REGNUM);
      bool x_free = crtl->return_rtx == NULL_RTX
		    || !reg_overlap_mentioned_p (x, crtl->return_rtx);
      bool y_free = crtl->return_rtx == NULL_RTX
		    || !reg_overlap_mentioned_p (y, crtl->return_rtx);
      if (x_free || y_free)
	emit_insn (gen_stm8_restore_frame (x_free ? x : y));
      else
	emit_insn (gen_stm8_restore_frame_wide ());
      if (cfun->static_chain_decl)
	stm8_emit_stack_adjust (2, false);
    }
  else if (frame_size != 0)
    stm8_emit_stack_adjust (frame_size, false);

  if (pops_args != 0)
    {
      rtx x = gen_rtx_REG (HImode, XH_REGNUM);
      rtx y = gen_rtx_REG (HImode, YH_REGNUM);

      bool x_free = crtl->return_rtx == NULL_RTX
		    || !reg_overlap_mentioned_p (x, crtl->return_rtx);

      bool y_free = crtl->return_rtx == NULL_RTX
		    || !reg_overlap_mentioned_p (y, crtl->return_rtx);

      /*
       * SDCC's medium-model callee cleanup removes the return address
       * into a free 16-bit register, discards the stack arguments, then
       * jumps through that register.  */
      if (x_free)
	{
	  emit_insn (gen_stm8_popw_x ());
	  stm8_emit_stack_adjust (pops_args, false);
	  emit_jump_insn (gen_stm8_return_x ());
	  return;
	}

      if (y_free)
	{
	  emit_insn (gen_stm8_popw_y ());
	  stm8_emit_stack_adjust (pops_args, false);
	  emit_jump_insn (gen_stm8_return_y ());
	  return;
	}

      /* The float-to-float case cleans its arguments while both word
	 registers hold the result.  Save X temporarily and use it to
	 relocate the return address above those arguments.  Copy the
	 low byte first, so an overlapping destination is safe too.
	 Indexed addressing also handles argument areas beyond SP's
	 8-bit displacement without changing the return convention.  */
      rtx a = gen_rtx_REG (QImode, A_REGNUM);
      emit_insn (gen_stm8_pushhi (x));
      emit_move_insn (x, stack_pointer_rtx);
      for (int i = 1; i >= 0; --i)
	{
	  rtx src = gen_rtx_MEM (
	      QImode, plus_constant (Pmode, stack_pointer_rtx, 3 + i));
	  rtx dest = gen_rtx_MEM (QImode,
				  plus_constant (Pmode, x, pops_args + 3 + i));
	  emit_move_insn (a, src);
	  emit_move_insn (dest, a);
	}
      emit_insn (gen_stm8_popw_x ());
      stm8_emit_stack_adjust (pops_args, false);
      emit_jump_insn (gen_stm8_return ());
      return;
    }

  emit_jump_insn (gen_stm8_return ());
}

void
stm8_expand_prologue (void)
{
  HOST_WIDE_INT frame_size = get_frame_size () + crtl->outgoing_args_size;

  if (flag_stack_usage_info)
    current_function_static_stack_size = frame_size + stm8_frame_save_size ();

  if (frame_pointer_needed)
    {
      stm8_uses_soft_frame_pointer = true;
      /* Neither A nor X can be clobbered before their argument values
	 have been copied.  Nested functions also receive their static
	 chain in Y; preserve it above the saved predecessor FP.  */
      rtx y = gen_rtx_REG (HImode, YH_REGNUM);
      if (cfun->static_chain_decl)
	RTX_FRAME_RELATED_P (emit_insn (gen_stm8_pushhi (y))) = 1;
      emit_move_insn (y, hard_frame_pointer_rtx);
      RTX_FRAME_RELATED_P (emit_insn (gen_stm8_pushhi (y))) = 1;
      emit_move_insn (y, stack_pointer_rtx);
      rtx_insn *anchor = emit_move_insn (hard_frame_pointer_rtx, y);
      RTX_FRAME_RELATED_P (anchor) = 1;
      /* FP is a fixed, memory-backed logical register.  Its value anchors
	 the caller's SP; this is a CFA definition, not a register save.  */
      add_reg_note (
	  anchor, REG_CFA_DEF_CFA,
	  plus_constant (Pmode, hard_frame_pointer_rtx,
			 stm8_frame_save_size () + STM8_RETURN_ADDRESS_SIZE));
    }

  if (frame_size != 0)
    stm8_emit_stack_adjust (-frame_size, true);

  if (frame_pointer_needed && cfun->static_chain_decl)
    {
      rtx y = gen_rtx_REG (HImode, YH_REGNUM);
      emit_move_insn (y, hard_frame_pointer_rtx);
      emit_insn (gen_stm8_loadhi_same_index (
	  y, gen_frame_mem (HImode, plus_constant (Pmode, y, 3))));
    }
}

static void
stm8_split_movhi_mem_1 (rtx dest, rtx src)
{
  rtx a = gen_rtx_REG (QImode, A_REGNUM);

  gcc_assert (reload_completed);

  if (MEM_P (dest))
    {
      gcc_assert (REG_P (src));

      if (stm8_storehi_cross_index_p (dest, src))
	{
	  emit_insn (gen_stm8_storehi_cross_index (dest, src));
	  return;
	}

      for (unsigned int i = 0; i < 2; ++i)
	{
	  rtx src_byte = simplify_gen_subreg (QImode, src, HImode, i);
	  rtx dest_byte = adjust_address (dest, QImode, i);

	  gcc_assert (src_byte != NULL_RTX);

	  emit_move_insn (a, src_byte);
	  emit_move_insn (dest_byte, a);
	}

      return;
    }

  gcc_assert (REG_P (dest));
  gcc_assert (MEM_P (src));

  if (stm8_loadhi_same_index_p (dest, src))
    {
      emit_insn (gen_stm8_loadhi_same_index (dest, src));
      return;
    }

  if (stm8_loadhi_index_from_sp_p (dest, src))
    {
      emit_insn (gen_stm8_loadhi_index_from_sp (dest, src));
      return;
    }

  /*
   * Byte-wise loads are safe only when changing DEST cannot alter
   * the address used by SRC.  */
  gcc_assert (!reg_overlap_mentioned_p (dest, XEXP (src, 0)));

  for (unsigned int i = 0; i < 2; ++i)
    {
      rtx dest_byte = simplify_gen_subreg (QImode, dest, HImode, i);
      rtx src_byte = adjust_address (src, QImode, i);

      gcc_assert (dest_byte != NULL_RTX);

      emit_move_insn (a, src_byte);
      emit_move_insn (dest_byte, a);
    }
}

/* A word move can be inserted while A holds a byte of a wider value.
   Its early-clobber scratch may be A, or another byte that preserves A
   around the two transfers.  Native LDW forms do not use A.  */

void
stm8_split_movhi_mem (rtx dest, rtx src, rtx scratch)
{
  bool native_p = stm8_storehi_cross_index_p (dest, src)
		  || stm8_loadhi_same_index_p (dest, src)
		  || stm8_loadhi_index_from_sp_p (dest, src);
  bool preserve_a = !native_p && REGNO (scratch) != A_REGNUM;
  rtx a = gen_rtx_REG (QImode, A_REGNUM);

  if (preserve_a)
    emit_move_insn (scratch, a);
  stm8_split_movhi_mem_1 (dest, src);
  if (preserve_a)
    emit_move_insn (a, scratch);
}

/* The byte accumulator instructions cannot read a register RHS.  Make
   the memory copy before constraining their LHS to A, so LRA never has
   to spill the RHS through A while A already contains that LHS.  */

rtx
stm8_prepare_byte_rhs (rtx rhs)
{
  if (!register_operand (rhs, QImode))
    return rhs;

  rtx slot = stm8_assign_stack_temp (QImode);
  emit_move_insn (slot, rhs);
  return slot;
}

bool
stm8_loadhi_index_from_sp_p (rtx dest, rtx src)
{
  if (!REG_P (dest) || !MEM_P (src))
    return false;

  unsigned int regno = REGNO (dest);

  if (regno != XH_REGNUM && regno != YH_REGNUM)
    return false;

  rtx addr = XEXP (src, 0);

  if (REG_P (addr))
    return REGNO (addr) == STACK_POINTER_REGNUM;

  if (GET_CODE (addr) == PLUS)
    {
      rtx op0 = XEXP (addr, 0);
      rtx op1 = XEXP (addr, 1);

      if (REG_P (op0) && REGNO (op0) == STACK_POINTER_REGNUM
	  && CONST_INT_P (op1))
	return IN_RANGE (INTVAL (op1), 0, 254);

      if (REG_P (op1) && REGNO (op1) == STACK_POINTER_REGNUM
	  && CONST_INT_P (op0))
	return IN_RANGE (INTVAL (op0), 0, 254);
    }

  return false;
}

bool
stm8_loadhi_same_index_p (rtx dest, rtx src)
{
  if (!REG_P (dest) || !MEM_P (src))
    return false;

  unsigned int regno = REGNO (dest);

  if (regno != XH_REGNUM && regno != YH_REGNUM)
    return false;

  rtx addr = XEXP (src, 0);

  if (REG_P (addr))
    return REGNO (addr) == regno;

  if (GET_CODE (addr) == PLUS)
    {
      rtx op0 = XEXP (addr, 0);
      rtx op1 = XEXP (addr, 1);

      if (REG_P (op0) && REGNO (op0) == regno && CONST_INT_P (op1))
	return IN_RANGE (INTVAL (op1), 0, 65535);

      if (REG_P (op1) && REGNO (op1) == regno && CONST_INT_P (op0))
	return IN_RANGE (INTVAL (op0), 0, 65535);
    }

  return false;
}

bool
stm8_storehi_cross_index_p (rtx dest, rtx src)
{
  if (!MEM_P (dest) || !REG_P (src))
    return false;

  unsigned int src_regno = REGNO (src);
  unsigned int base_regno;

  if (src_regno == XH_REGNUM)
    base_regno = YH_REGNUM;
  else if (src_regno == YH_REGNUM)
    base_regno = XH_REGNUM;
  else
    return false;

  rtx addr = XEXP (dest, 0);

  if (REG_P (addr))
    return REGNO (addr) == base_regno;

  if (GET_CODE (addr) == PLUS)
    {
      rtx op0 = XEXP (addr, 0);
      rtx op1 = XEXP (addr, 1);

      if (REG_P (op0) && REGNO (op0) == base_regno && CONST_INT_P (op1))
	return IN_RANGE (INTVAL (op1), 0, 65535);

      if (REG_P (op1) && REGNO (op1) == base_regno && CONST_INT_P (op0))
	return IN_RANGE (INTVAL (op0), 0, 65535);
    }

  return false;
}

static void
stm8_option_override (void)
{
  if (!global_options_set.x_flag_omit_frame_pointer)
    flag_omit_frame_pointer = 1;
}

static scalar_int_mode
stm8_function_pointer_mode (void)
{
  return TARGET_LARGE ? PSImode : HImode;
}

/* Snapshot an indirect code pointer before argument registers are filled.
   CALLF has no register addressing form.  The private descriptor's near
   address is carried by RTL until the call, when it is put in Y for a
   tail-transfer helper that preserves A and X.  */
static rtx
stm8_legitimize_address (rtx x, rtx oldx ATTRIBUTE_UNUSED, machine_mode mode)
{
  if (!TARGET_LARGE || mode != FUNCTION_MODE || GET_MODE (x) != PSImode
      || (GET_CODE (x) == UNSPEC && XINT (x, 1) == UNSPEC_FAR_CALL)
      || CONSTANT_P (x))
    return x;

  rtx descriptor = stm8_assign_stack_temp (PSImode);
  emit_move_insn (descriptor, x);
  rtx address = force_reg (Pmode, XEXP (descriptor, 0));
  return gen_rtx_UNSPEC (PSImode, gen_rtvec (1, address), UNSPEC_FAR_CALL);
}

static bool
stm8_function_symbol_constant_p (rtx x)
{
  if (GET_CODE (x) == SYMBOL_REF)
    return SYMBOL_REF_FUNCTION_P (x);
  if (GET_CODE (x) == CONST)
    return stm8_function_symbol_constant_p (XEXP (x, 0));
  if (GET_CODE (x) == PLUS || GET_CODE (x) == MINUS)
    return stm8_function_symbol_constant_p (XEXP (x, 0))
	   || stm8_function_symbol_constant_p (XEXP (x, 1));
  return false;
}

/* Narrowing a code pointer to an object pointer retains the low 16
   bits, as in SDCC large.  Byte relocations express this truncation;
   an ordinary R_STM8_16 correctly rejects an out-of-range address.  */
static bool
stm8_legitimate_constant_p (machine_mode mode, rtx x)
{
  return !(TARGET_LARGE && mode == HImode
	   && stm8_function_symbol_constant_p (x));
}

static bool
stm8_assemble_integer (rtx x, unsigned int size, int aligned_p)
{
  if (size == 2)
    {
      x = stm8_label_pointer_constant (x);
      if (TARGET_LARGE && stm8_function_symbol_constant_p (x))
	{
	  fputs ("\t.byte\thi8(", asm_out_file);
	  output_addr_const (asm_out_file, x);
	  fputs ("),lo8(", asm_out_file);
	  output_addr_const (asm_out_file, x);
	  fputs (")\n", asm_out_file);
	  return true;
	}
    }
  if (size == 3)
    {
      fputs ("\t.3byte\t", asm_out_file);
      output_addr_const (asm_out_file, x);
      fputc ('\n', asm_out_file);
      return true;
    }
  return default_assemble_integer (x, size, aligned_p);
}

/* Far function addresses held in constant pools must be collected with
   their users.  A single shared .rodata section would keep every callback
   in a translation unit alive through its code-address relocations.  */
static section *
stm8_select_rtx_section (machine_mode mode, rtx x, unsigned HOST_WIDE_INT align)
{
  if (TARGET_LARGE && current_function_decl)
    return targetm.asm_out.function_rodata_section (current_function_decl,
						    false);
  return default_elf_select_rtx_section (mode, x, align);
}

#undef TARGET_LEGITIMIZE_ADDRESS
#define TARGET_LEGITIMIZE_ADDRESS stm8_legitimize_address
#undef TARGET_ASM_INTEGER
#define TARGET_ASM_INTEGER stm8_assemble_integer
#undef TARGET_LEGITIMATE_CONSTANT_P
#define TARGET_LEGITIMATE_CONSTANT_P stm8_legitimate_constant_p
#undef TARGET_ASM_SELECT_RTX_SECTION
#define TARGET_ASM_SELECT_RTX_SECTION stm8_select_rtx_section

#undef TARGET_FUNCTION_POINTER_MODE
#define TARGET_FUNCTION_POINTER_MODE stm8_function_pointer_mode
#undef TARGET_FUNCTION_ADDRESS_MODE
#define TARGET_FUNCTION_ADDRESS_MODE stm8_function_pointer_mode

#undef TARGET_ASM_FILE_END
#define TARGET_ASM_FILE_END stm8_asm_file_end
#undef TARGET_ASM_FILE_START
#define TARGET_ASM_FILE_START stm8_asm_file_start

static bool
stm8_push_argument (unsigned int npush ATTRIBUTE_UNUSED)
{
  return !ACCUMULATE_OUTGOING_ARGS;
}

/* Save each physical argument/result register once.  The byte register
   aliases XL and YL are already covered by their complete index registers.  */
static fixed_size_mode
stm8_get_raw_arg_mode (int regno)
{
  if (regno == A_REGNUM)
    return QImode;
  if (regno == XH_REGNUM)
    return HImode;
  return as_a<fixed_size_mode> (VOIDmode);
}

static fixed_size_mode
stm8_get_raw_result_mode (int regno)
{
  if (regno == YH_REGNUM)
    return HImode;
  return stm8_get_raw_arg_mode (regno);
}

#undef TARGET_GET_RAW_ARG_MODE
#define TARGET_GET_RAW_ARG_MODE stm8_get_raw_arg_mode
#undef TARGET_GET_RAW_RESULT_MODE
#define TARGET_GET_RAW_RESULT_MODE stm8_get_raw_result_mode

#undef TARGET_HARD_REGNO_NREGS
#define TARGET_HARD_REGNO_NREGS stm8_hard_regno_nregs

#undef TARGET_HARD_REGNO_MODE_OK
#define TARGET_HARD_REGNO_MODE_OK stm8_hard_regno_mode_ok

#undef TARGET_REGISTER_MOVE_COST
#define TARGET_REGISTER_MOVE_COST stm8_register_move_cost

/* Byte operations require A and word addresses use only X/Y.  Keep
   explicit hard-register lifetimes short so reload can use these small
   classes for intermediate copies and caller-save restores.  */
#undef TARGET_SMALL_REGISTER_CLASSES_FOR_MODE_P
#define TARGET_SMALL_REGISTER_CLASSES_FOR_MODE_P hook_bool_mode_true

#undef TARGET_SECONDARY_RELOAD
#define TARGET_SECONDARY_RELOAD stm8_secondary_reload

#undef TARGET_SCALAR_MODE_SUPPORTED_P
#define TARGET_SCALAR_MODE_SUPPORTED_P stm8_scalar_mode_supported_p

#undef TARGET_TRULY_NOOP_TRUNCATION
#define TARGET_TRULY_NOOP_TRUNCATION stm8_truly_noop_truncation

#undef TARGET_CAN_ELIMINATE
#define TARGET_CAN_ELIMINATE stm8_can_eliminate

#undef TARGET_STARTING_FRAME_OFFSET
#define TARGET_STARTING_FRAME_OFFSET stm8_starting_frame_offset

#undef TARGET_FUNCTION_ARG
#define TARGET_FUNCTION_ARG stm8_function_arg

#undef TARGET_FUNCTION_ARG_ADVANCE
#define TARGET_FUNCTION_ARG_ADVANCE stm8_function_arg_advance

#undef TARGET_FUNCTION_VALUE
#define TARGET_FUNCTION_VALUE stm8_function_value

#undef TARGET_LIBCALL_VALUE
#define TARGET_LIBCALL_VALUE stm8_libcall_value

#undef TARGET_FUNCTION_VALUE_REGNO_P
#define TARGET_FUNCTION_VALUE_REGNO_P stm8_function_value_regno_p

#undef TARGET_RETURN_IN_MEMORY
#define TARGET_RETURN_IN_MEMORY stm8_return_in_memory

#undef TARGET_RETURN_POPS_ARGS
#define TARGET_RETURN_POPS_ARGS stm8_return_pops_args

#undef TARGET_BASE_REG_CLASS
#define TARGET_BASE_REG_CLASS stm8_base_reg_class

#undef TARGET_LEGITIMATE_ADDRESS_P
#define TARGET_LEGITIMATE_ADDRESS_P stm8_legitimate_address_p

#undef TARGET_PRINT_OPERAND
#define TARGET_PRINT_OPERAND stm8_print_operand

#undef TARGET_PRINT_OPERAND_ADDRESS
#define TARGET_PRINT_OPERAND_ADDRESS stm8_print_operand_address

#undef TARGET_C_MODE_FOR_FLOATING_TYPE
#define TARGET_C_MODE_FOR_FLOATING_TYPE stm8_c_mode_for_floating_type

#undef TARGET_OPTION_OVERRIDE
#define TARGET_OPTION_OVERRIDE stm8_option_override

#undef TARGET_PUSH_ARGUMENT
#define TARGET_PUSH_ARGUMENT stm8_push_argument

/* Stack scrubbing uses __builtin_frame_address.  Fixed frames eliminate
   the virtual FP and do not provide a physical frame pointer.  */
#undef TARGET_HAVE_STRUB_SUPPORT_FOR
#define TARGET_HAVE_STRUB_SUPPORT_FOR hook_bool_tree_false

#undef TARGET_DOCUMENTATION_NAME
#define TARGET_DOCUMENTATION_NAME "STM8"

/* CFA notes describe local-variable locations, but the port does not yet
   provide the complete return-PC/epilogue CFI convention.  GDB uses its
   STM8 prologue/FP unwinder rather than incomplete debug unwind rows.  */
static enum unwind_info_type
stm8_debug_unwind_info (void)
{
  return UI_NONE;
}

#undef TARGET_DEBUG_UNWIND_INFO
#define TARGET_DEBUG_UNWIND_INFO stm8_debug_unwind_info

struct gcc_target targetm = TARGET_INITIALIZER;
