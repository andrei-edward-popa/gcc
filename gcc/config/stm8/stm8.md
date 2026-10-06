;; Machine description for the STM8 processor.
;; Copyright (C) 2026 Free Software Foundation, Inc.
;;
;; This file is part of GCC.
;;
;;
;; GCC is free software; you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation; either version 3, or (at your option)
;; any later version.
;;
;; GCC is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU General Public License for more details.
;;
;; You should have received a copy of the GNU General Public License
;; along with GCC; see the file COPYING3.  If not see
;; <http://www.gnu.org/licenses/>.

(include "constraints.md")

;; Hard register numbers used by the machine description.

(define_constants
  [
   (A_REG   0)
   (YH_REG  1)
   (YL_REG  2)
   (XH_REG  3)
   (XL_REG  4)
   (SP_REG  5)
   (CC_REG  6)
   (AP_REG  7)
   (FP_REG  8)
  ])

(define_c_enum "unspec"
  [UNSPEC_RELOAD_HI UNSPEC_RELOAD_QI UNSPEC_LONGJMP UNSPEC_FAR_CALL])

;; No operation.

(define_insn "nop"
  [(const_int 0)]
  ""
  "nop")

;; The generic multi-word fallback can form a SUBREG of a symbolic PSI
;; address.  Such an operand is not an integer byte and loses relocation
;; information.  Load symbolic PCs from a 24-bit constant pool entry and
;; snapshot all bytes before writing any possibly overlapping destination.
(define_expand "movpsi"
  [(set (match_operand:PSI 0 "nonimmediate_operand")
	(match_operand:PSI 1 "general_operand"))]
  "TARGET_LARGE"
{
  rtx source = operands[1];
  if (CONSTANT_P (source) && !CONST_INT_P (source))
    source = force_const_mem (PSImode, source);
  rtx bytes[3];
  for (int i = 0; i < 3; ++i)
    {
      bytes[i] = gen_reg_rtx (QImode);
      emit_move_insn (bytes[i], operand_subword (source, i, 1, PSImode));
    }
  if (REG_P (operands[0]))
    emit_clobber (operands[0]);
  for (int i = 0; i < 3; ++i)
    emit_move_insn (operand_subword (operands[0], i, 1, PSImode), bytes[i]);
  DONE;
})

;; -------------------------------------------------------------------------
;; Jumps
;; -------------------------------------------------------------------------

;; Unconditional jump.

(define_insn "jump"
  [(set (pc)
	(label_ref (match_operand 0 "" "")))]
  ""
  "%J0\t%l0")

;; Near computed goto and nonlocal jumps use the same 16-bit code pointers
;; as calls.  Recognizing this as a JUMP_INSN also lets GCC attach the
;; REG_NON_LOCAL_GOTO note when expanding __builtin_longjmp.
(define_expand "indirect_jump"
  [(set (pc) (match_operand:HI 0 "register_operand"))]
  ""
{
  emit_jump_insn (TARGET_LARGE ? gen_stm8_far_indirect_jump (operands[0])
			       : gen_stm8_near_indirect_jump (operands[0]));
  DONE;
})

(define_insn "stm8_near_indirect_jump"
  [(set (pc)
	(match_operand:HI 0 "register_operand" "v"))]
  "!TARGET_LARGE"
  "jp\t(%0)")

(define_insn "stm8_far_indirect_jump"
  [(set (pc) (mem:PSI (match_operand:HI 0 "register_operand" "v")))
   (clobber (match_scratch:HI 1 "=&v"))]
  "TARGET_LARGE"
  "ld\t%b1,a\n\tld\ta,(2,%0)\n\tpush\ta\n\tld\ta,(1,%0)\n\tpush\ta\n\tld\ta,\
(%0)\n\tpush\ta\n\tld\ta,%b1\n\tretf")

;; A generic longjmp can spill its saved PC/FP into the old frame and then
;; reload those spills after replacing our memory-backed FP.  Restore the
;; whole context in one instruction so reload finishes before either frame
;; address changes.  GCC's builtin buffer contains FP, PC and SP at 0, 2, 4.
(define_expand "builtin_longjmp"
  [(use (match_operand:HI 0 "register_operand"))]
  ""
{
  emit_jump_insn (gen_stm8_builtin_longjmp (operands[0]));
  emit_barrier ();
  DONE;
})

;; Nested-function gotos use a two-word FP/SP save area and pass the PC
;; separately.  Snapshot all three values before the atomic restoration;
;; no reload is then allowed to refer to the frame after it is replaced.
(define_expand "nonlocal_goto"
  [(use (match_operand 0 "general_operand"))
   (use (match_operand:HI 1 "general_operand"))
   (use (match_operand:HI 2 "general_operand"))
   (use (match_operand:HI 3 "general_operand"))]
  ""
{
  push_temp_slots ();
  rtx context = stm8_assign_stack_temp (DImode);
  emit_clobber (gen_rtx_MEM (BLKmode, gen_rtx_SCRATCH (VOIDmode)));
  emit_move_insn (adjust_address (context, HImode, 0), operands[3]);
  emit_move_insn (adjust_address (context, HImode, 2), operands[1]);
  emit_move_insn (adjust_address (context, HImode, 4), operands[2]);
  rtx address = force_reg (Pmode, XEXP (context, 0));
  emit_jump_insn (gen_stm8_builtin_longjmp (address));
  emit_barrier ();
  pop_temp_slots ();
  DONE;
})

(define_insn "stm8_builtin_longjmp"
  [(set (pc)
	(unspec_volatile:HI
	  [(mem:HI (plus:HI (match_operand:HI 0 "register_operand" "v")
			    (const_int 2)))] UNSPEC_LONGJMP))
   (set (reg:HI SP_REG)
	(mem:HI (plus:HI (match_dup 0) (const_int 4))))
   (set (reg:HI FP_REG) (mem:HI (match_dup 0)))
   (clobber (match_dup 0))
   (clobber (match_scratch:HI 1 "=&v"))]
  ""
{
  output_asm_insn (
      "ldw\t%1,%0\n\tldw\t%1,(%1)\n\tldw\t__stm8_frame_pointer,%1\n\tldw\t%1,%"
      "0\n\tldw\t%1,(4,%1)\n\tldw\tsp,%1\n\tldw\t%0,(2,%0)",
      operands);
  return TARGET_LARGE
	     ? "ld\t%b1,a\n\tld\ta,(2,%0)\n\tpush\ta\n\tld\ta,(1,%0)"
	       "\n\tpush\ta\n\tld\ta,(%0)\n\tpush\ta\n\tld\ta,%b1\n\tretf"
	     : "jp\t(%0)";
})

;; Normal near call.  Operand 0 is the MEM containing the function
;; address.  The remaining public call operands are not needed by STM8.
(define_expand "call"
  [(call (match_operand 0 "memory_operand")
	 (match_operand 1 "" ""))]
  ""
{
  if (TARGET_LARGE)
    {
      rtx address = XEXP (operands[0], 0);
      if (GET_CODE (address) == UNSPEC)
	{
	  rtx y = gen_rtx_REG (HImode, YH_REG);
	  emit_move_insn (y, XVECEXP (address, 0, 0));
	  emit_call_insn (gen_stm8_far_call_indirect (y, operands[1]));
	}
      else
	emit_call_insn (gen_stm8_far_call (operands[0], operands[1]));
    }
  else
    emit_call_insn (gen_stm8_near_call (operands[0], operands[1]));
  DONE;
})

(define_insn "stm8_near_call"
  [(call (match_operand 0 "memory_operand" "R")
	 (match_operand 1 "" ""))]
  "!TARGET_LARGE"
  "call\t%0")

(define_insn "stm8_far_call"
  [(call (match_operand 0 "memory_operand" "R")
	 (match_operand 1 "" ""))]
  "TARGET_LARGE && CONSTANT_P (XEXP (operands[0], 0))"
  "callf\t%0")

(define_insn "stm8_far_call_indirect"
  [(call (mem:PSI (unspec:PSI
		   [(match_operand:HI 0 "register_operand" "y")]
		   UNSPEC_FAR_CALL))
	 (match_operand 1 "" ""))]
  "TARGET_LARGE"
  "callf\t___stm8_call_indirect")

;; Normal near call returning a value.
(define_expand "untyped_call"
  [(parallel [(call (match_operand 0 "memory_operand") (const_int 0))
	      (match_operand 1 "")
	      (match_operand 2 "")])]
  ""
{
  emit_call_insn (gen_call (operands[0], const0_rtx));
  for (int i = 0; i < XVECLEN (operands[2], 0); ++i)
    {
      rtx set = XVECEXP (operands[2], 0, i);
      emit_move_insn (SET_DEST (set), SET_SRC (set));
    }
  /* These stores observe untyped return registers, whose definitions
     cannot be described by an ordinary call_value.  */
  emit_insn (gen_blockage ());
  DONE;
})

;; Normal near call returning a value.
(define_expand "call_value"
  [(set (match_operand 0 "register_operand")
	(call (match_operand 1 "memory_operand")
	      (match_operand 2 "" "")))]
  ""
{
  if (TARGET_LARGE)
    {
      rtx address = XEXP (operands[1], 0);
      if (GET_CODE (address) == UNSPEC)
	{
	  rtx y = gen_rtx_REG (HImode, YH_REG);
	  emit_move_insn (y, XVECEXP (address, 0, 0));
	  emit_call_insn (
	      gen_stm8_far_call_value_indirect (operands[0], y, operands[2]));
	}
      else
	emit_call_insn (
	    gen_stm8_far_call_value (operands[0], operands[1], operands[2]));
    }
  else
    emit_call_insn (
	gen_stm8_near_call_value (operands[0], operands[1], operands[2]));
  DONE;
})

(define_insn "stm8_near_call_value"
  [(set (match_operand 0 "register_operand" "=r")
	(call (match_operand 1 "memory_operand" "R")
	      (match_operand 2 "" "")))]
  "!TARGET_LARGE"
  "call\t%1")

(define_insn "stm8_far_call_value"
  [(set (match_operand 0 "register_operand" "=r")
	(call (match_operand 1 "memory_operand" "R")
	      (match_operand 2 "" "")))]
  "TARGET_LARGE && CONSTANT_P (XEXP (operands[1], 0))"
  "callf\t%1")

(define_insn "stm8_far_call_value_indirect"
  [(set (match_operand 0 "register_operand" "=r")
	(call (mem:PSI (unspec:PSI
		       [(match_operand:HI 1 "register_operand" "y")]
		       UNSPEC_FAR_CALL))
	      (match_operand 2 "" "")))]
  "TARGET_LARGE"
  "callf\t___stm8_call_indirect")

(define_expand "call_pop"
  [(match_operand 0 "memory_operand")
   (match_operand 1 "" "")
   (match_operand 2 "" "")
   (match_operand 3 "" "")]
  ""
{
  emit_call_insn (gen_stm8_call_pop (operands[0], operands[1], operands[3]));
  DONE;
})


(define_insn "stm8_call_pop"
  [(call (match_operand 0 "memory_operand" "R")
	 (match_operand 1 "" ""))
   (set (reg:HI SP_REG)
	(plus:HI (reg:HI SP_REG)
		 (match_operand:HI 2 "const_int_operand" "i")))]
  ""
  "call\t%0")

(define_expand "call_value_pop"
  [(match_operand 0 "register_operand")
   (match_operand 1 "memory_operand")
   (match_operand 2 "" "")
   (match_operand 3 "" "")
   (match_operand 4 "" "")]
  ""
{
  emit_call_insn (gen_stm8_call_value_pop (operands[0], operands[1],
					   operands[2], operands[4]));
  DONE;
})


(define_insn "stm8_call_value_pop"
  [(set (match_operand 0 "register_operand" "=r")
	(call (match_operand 1 "memory_operand" "R")
	      (match_operand 2 "" "")))
   (set (reg:HI SP_REG)
	(plus:HI (reg:HI SP_REG)
		 (match_operand:HI 3 "const_int_operand" "i")))]
  ""
  "call\t%1")

;; -------------------------------------------------------------------------
;; Function entry and exit
;; -------------------------------------------------------------------------

(define_expand "allocate_stack"
  [(match_operand:HI 0 "register_operand")
   (match_operand:HI 1 "general_operand")]
  ""
{
  rtx next_sp = gen_reg_rtx (Pmode);
  emit_insn (gen_subhi3 (next_sp, stack_pointer_rtx, operands[1]));
  emit_move_insn (stack_pointer_rtx, next_sp);
  /* Its final offset includes the accumulated outgoing argument area,
     whose size is not yet known while expanding this allocation.  */
  emit_move_insn (operands[0], virtual_stack_dynamic_rtx);
  DONE;
})

(define_expand "prologue"
  [(const_int 0)]
  ""
  {
    stm8_expand_prologue ();
    DONE;
  })

(define_expand "epilogue"
  [(const_int 0)]
  ""
  {
    stm8_expand_epilogue ();
    DONE;
  })


(define_insn "stm8_return"
  [(return)]
  ""
  { return TARGET_LARGE ? "retf" : "ret"; })

;; Reset SP from the stable FP anchor, then restore the predecessor.
;; Describe the whole operation as one insn: SP may move by a dynamic
;; amount and the scratch must not overlap the function's return value.
(define_insn "stm8_restore_frame"
  [(set (reg:HI SP_REG)
	(plus:HI (reg:HI FP_REG) (const_int 2)))
   (set (reg:HI FP_REG)
	(mem:HI (plus:HI (reg:HI FP_REG) (const_int 1))))
   (clobber (match_operand:HI 0 "register_operand" "=&v"))]
  ""
  "ldw\t%0,__stm8_frame_pointer\n\tldw\tsp,%0\n\tpopw\t%0\n\tldw\t__stm8_fra\
me_pointer,%0")

;; Both X and Y hold an ABI return value.  Exchange X with the current
;; FP through A, reset SP, and reload X before replacing the saved FP.
;; No extra shared scratch storage or clobbered return register is needed.
(define_insn "stm8_restore_frame_wide"
  [(set (reg:HI SP_REG)
	(plus:HI (reg:HI FP_REG) (const_int 2)))
   (set (reg:HI FP_REG)
	(mem:HI (plus:HI (reg:HI FP_REG) (const_int 1))))
   (clobber (reg:QI A_REG))]
  ""
  "ld\ta,xl\n\texg\ta,__stm8_frame_pointer+1\n\tld\txl,a\n\tld\ta,xh\n\texg\
\ta,__stm8_frame_pointer\n\tld\txh,a\n\tldw\tsp,x\n\tldw\tx,__stm8_frame_poi\
nter\n\tld\ta,(1,sp)\n\tld\t__stm8_frame_pointer,a\n\tld\ta,(2,sp)\n\tld\t__\
stm8_frame_pointer+1,a\n\taddw\tsp,#2")

(define_predicate "stm8_word_rhs_operand"
  (ior (match_operand 0 "memory_operand")
       (match_operand 0 "immediate_operand")))

;; ADDW/SUBW accept immediates, absolute memory and short SP offsets.
;; Other memory addresses use an atomic byte chain with an A scratch.
;; The public expanders snapshot non-constant RHS values on the stack before
;; tying the destination to X/Y.  This also keeps overlapping operands safe.

(define_insn "stm8_addhi_index_imm"
  [(set (match_operand:HI 0 "register_operand" "=v")
	(plus:HI (match_operand:HI 1 "register_operand" "0")
		  (match_operand:HI 2 "immediate_operand" "i")))]
  "REGNO (operands[0]) >= FIRST_PSEUDO_REGISTER
   || REGNO (operands[0]) == XH_REGNUM
   || REGNO (operands[0]) == YH_REGNUM"
  "addw\t%0,%2")

(define_insn "stm8_addhi_index_rhs"
  [(set (match_operand:HI 0 "register_operand" "=&v")
	(plus:HI
	  (match_operand:HI 1 "register_operand" "0")
	  (match_operand:HI 2 "memory_operand" "R")))
   (clobber (match_scratch:QI 3 "=&r"))]
  "REGNO (operands[0]) >= FIRST_PSEUDO_REGISTER
   || REGNO (operands[0]) == XH_REGNUM
   || REGNO (operands[0]) == YH_REGNUM"
{
  return stm8_output_word_alu (operands, false);
})

(define_insn "stm8_subhi_index_imm"
  [(set (match_operand:HI 0 "register_operand" "=v")
	(minus:HI (match_operand:HI 1 "register_operand" "0")
		  (match_operand:HI 2 "immediate_operand" "i")))]
  "REGNO (operands[0]) >= FIRST_PSEUDO_REGISTER
   || REGNO (operands[0]) == XH_REGNUM
   || REGNO (operands[0]) == YH_REGNUM"
  "subw\t%0,%2")

(define_insn "stm8_subhi_index_rhs"
  [(set (match_operand:HI 0 "register_operand" "=&v")
	(minus:HI
	  (match_operand:HI 1 "register_operand" "0")
	  (match_operand:HI 2 "memory_operand" "R")))
   (clobber (match_scratch:QI 3 "=&r"))]
  "REGNO (operands[0]) >= FIRST_PSEUDO_REGISTER
   || REGNO (operands[0]) == XH_REGNUM
   || REGNO (operands[0]) == YH_REGNUM"
{
  return stm8_output_word_alu (operands, true);
})

(define_insn "stm8_addhi_double"
  [(set (match_operand:HI 0 "register_operand" "=v")
	(plus:HI (match_operand:HI 1 "register_operand" "0")
		 (match_dup 1)))]
  ""
  "sllw\t%0")

(define_expand "addhi3"
  [(set (match_operand:HI 0 "register_operand")
	(plus:HI
	  (match_operand:HI 1 "general_operand")
	  (match_operand:HI 2 "general_operand")))]
  ""
{
  push_temp_slots ();
  if (rtx_equal_p (operands[0], stack_pointer_rtx)
      && rtx_equal_p (operands[1], stack_pointer_rtx)
      && CONST_INT_P (operands[2]))
    {
      stm8_emit_stack_adjust (INTVAL (operands[2]), false);
      pop_temp_slots ();
      DONE;
    }

  if (register_operand (operands[1], HImode)
      && rtx_equal_p (operands[1], operands[2]))
    {
      if (!rtx_equal_p (operands[0], operands[1]))
	emit_move_insn (operands[0], operands[1]);
      emit_insn (gen_stm8_addhi_double (operands[0], operands[0]));
      pop_temp_slots ();
      DONE;
    }

  if (CONST_INT_P (operands[1]) && !CONST_INT_P (operands[2]))
    std::swap (operands[1], operands[2]);

  operands[1] = force_reg (HImode, operands[1]);

  if (!CONST_INT_P (operands[2]))
    {
      rtx slot = stm8_assign_stack_temp (HImode);
      emit_move_insn (slot, operands[2]);
      operands[2] = slot;
    }

  if (!rtx_equal_p (operands[0], operands[1]))
    emit_move_insn (operands[0], operands[1]);

  if (MEM_P (operands[2]))
    emit_insn (
	gen_stm8_addhi_index_rhs (operands[0], operands[0], operands[2]));
  else
    emit_insn (
	gen_stm8_addhi_index_imm (operands[0], operands[0], operands[2]));
  pop_temp_slots ();
  DONE;
})

(define_expand "subhi3"
  [(set (match_operand:HI 0 "register_operand")
	(minus:HI
	  (match_operand:HI 1 "general_operand")
	  (match_operand:HI 2 "general_operand")))]
  ""
{
  push_temp_slots ();
  if (rtx_equal_p (operands[0], stack_pointer_rtx)
      && rtx_equal_p (operands[1], stack_pointer_rtx)
      && CONST_INT_P (operands[2]))
    {
      stm8_emit_stack_adjust (-INTVAL (operands[2]), false);
      pop_temp_slots ();
      DONE;
    }

  operands[1] = force_reg (HImode, operands[1]);

  if (!CONST_INT_P (operands[2]))
    {
      rtx slot = stm8_assign_stack_temp (HImode);
      emit_move_insn (slot, operands[2]);
      operands[2] = slot;
    }

  if (!rtx_equal_p (operands[0], operands[1]))
    emit_move_insn (operands[0], operands[1]);

  if (MEM_P (operands[2]))
    emit_insn (
	gen_stm8_subhi_index_rhs (operands[0], operands[0], operands[2]));
  else
    emit_insn (
	gen_stm8_subhi_index_imm (operands[0], operands[0], operands[2]));
  pop_temp_slots ();
  DONE;
})

;; Model each constant adjustment in one RTL insn for SP elimination.
;; Emit chunks that fit the ISA short immediate at final output.

(define_insn "stm8_sub_sp"
  [(set (reg:HI SP_REG)
	(plus:HI (reg:HI SP_REG)
		  (match_operand:HI 0 "const_int_operand" "i")))]
  "INTVAL (operands[0]) < 0"
{
  HOST_WIDE_INT amount = -INTVAL (operands[0]);
  while (amount != 0)
    {
      HOST_WIDE_INT chunk = amount > 255 ? 255 : amount;
      rtx step[1] = { GEN_INT (chunk) };
      output_asm_insn ("sub\tsp,%0", step);
      amount -= chunk;
    }
  return "";
})


;; Adjust the stack pointer upward using short immediate chunks.

(define_insn "stm8_add_sp"
  [(set (reg:HI SP_REG)
	(plus:HI (reg:HI SP_REG)
		 (match_operand:HI 0 "const_int_operand" "i")))]
  "INTVAL (operands[0]) > 0"
{
  HOST_WIDE_INT amount = INTVAL (operands[0]);
  while (amount != 0)
    {
      HOST_WIDE_INT chunk = amount > 255 ? 255 : amount;
      rtx step[1] = { GEN_INT (chunk) };
      output_asm_insn ("addw\tsp,%0", step);
      amount -= chunk;
    }
  return "";
})

;; -------------------------------------------------------------------------
;; Moves
;; -------------------------------------------------------------------------

;; 8-bit move.
;;
;; STM8 can move an arbitrary byte register to/from A, but there is no
;; general byte-register-to-byte-register instruction.  Memory accesses
;; likewise use A.

(define_expand "cpymemhi"
  [(match_operand:BLK 0 "memory_operand")
   (match_operand:BLK 1 "memory_operand")
   (match_operand:HI 2 "general_operand")
   (match_operand 3 "const_int_operand")]
  ""
{
  stm8_expand_cpymem (operands[0], operands[1], operands[2]);
  DONE;
})

(define_insn "movqi"
  [(set (match_operand:QI 0 "nonimmediate_operand"
			  "=a,v,a,m,a,v,a")
	(match_operand:QI 1 "general_operand"
			  "v,a,m,a,i,0,0"))]
  ""
  {
    if (rtx_equal_p (operands[0], operands[1]))
      return "";

    stm8_output_byte_move (operands[0], operands[1]);
    return "";
  })


;; 16-bit register move.
;;
;; Memory operands are intentionally not handled here yet.  STM8 LDW has
;; asymmetric restrictions for X/Y indexed memory operands, so those moves
;; need target-specific expansion rather than an unrestricted r/m pattern.

(define_expand "movhi"
  [(set (match_operand:HI 0 "nonimmediate_operand")
	(match_operand:HI 1 "general_operand"))]
  ""
  {
    if (MEM_P (operands[0]) || MEM_P (operands[1]))
      {
	/* Avoid MEM <- MEM.  */
	if (MEM_P (operands[0])
	    && !register_operand (operands[1], HImode))
	  operands[1] = force_reg (HImode, operands[1]);

	rtx set = gen_rtx_SET (operands[0], operands[1]);
	rtx clobber
	  = gen_rtx_CLOBBER (VOIDmode, gen_rtx_SCRATCH (QImode));

	emit_insn (gen_rtx_PARALLEL (VOIDmode,
				     gen_rtvec (2, set, clobber)));
	DONE;
      }
  })

(define_insn "*movhi_reg"
  [(set (match_operand:HI 0 "register_operand"
			  "=v,v,v,q,v,f")
	(match_operand:HI 1 "nonmemory_operand"
			  "v,i,q,v,f,v"))]
  ""
  {
    if (rtx_equal_p (operands[0], operands[1]))
      return "";

    return "ldw\t%0,%1";
  })

(define_insn "stm8_storehi_cross_index"
  [(set (match_operand:HI 0 "memory_operand" "=R")
	(match_operand:HI 1 "register_operand" "v"))]
  "stm8_storehi_cross_index_p (operands[0], operands[1])"
  "ldw\t%0,%1")

(define_insn "stm8_loadhi_index_from_sp"
  [(set (match_operand:HI 0 "register_operand" "=v")
	(match_operand:HI 1 "memory_operand" "R"))]
  "stm8_loadhi_index_from_sp_p (operands[0], operands[1])"
  "ldw\t%0,%1")

(define_insn "stm8_loadhi_same_index"
  [(set (match_operand:HI 0 "register_operand" "=v")
	(match_operand:HI 1 "memory_operand" "R"))]
  "stm8_loadhi_same_index_p (operands[0], operands[1])"
  "ldw\t%0,%1")

(define_insn_and_split "*movhi_mem"
  [(set (match_operand:HI 0 "nonimmediate_operand" "=v,m")
	(match_operand:HI 1 "general_operand"      "m,v"))
   (clobber (match_scratch:QI 2 "=&r,&r"))]
  ""
  "#"
  "reload_completed"
  [(const_int 0)]
  {
    stm8_split_movhi_mem (operands[0], operands[1], operands[2]);
    DONE;
  })

;; LRA may spill a pseudo after movhi has already expanded a register
;; move.  The reload hook supplies a scratch for these memory copies.
;; Use a target-specific copy so LRA does not treat the scratch-bearing
;; replacement as another ordinary move requiring secondary reload.

(define_insn_and_split "stm8_reloadhi"
  [(set (match_operand:HI 0 "nonimmediate_operand" "=v,m")
	(unspec:HI [(match_operand:HI 1 "general_operand" "m,v")]
		   UNSPEC_RELOAD_HI))
   (clobber (match_operand:QI 2 "register_operand" "=&r,&r"))]
  ""
  "#"
  "reload_completed"
  [(const_int 0)]
{
  stm8_split_movhi_mem (operands[0], operands[1], operands[2]);
  DONE;
})

;; An index-byte caller-save restore can occur while A already contains
;; a comparison input.  Use A directly when it is free; otherwise preserve
;; it in the allocated scratch byte.  The early clobber keeps that byte
;; separate from the source, destination and all address registers.
(define_insn "stm8_reloadqi"
  [(set (match_operand:QI 0 "nonimmediate_operand" "=v,m,v")
	(unspec:QI [(match_operand:QI 1 "general_operand" "mi,v,v")]
		   UNSPEC_RELOAD_QI))
   (clobber (match_operand:QI 2 "register_operand" "=&r,&r,&r"))]
  ""
{
  if (REGNO (operands[2]) != A_REG)
    output_asm_insn ("ld\t%2,a", operands);
  stm8_output_byte_move (gen_rtx_REG (QImode, A_REG), operands[1]);
  stm8_output_byte_move (operands[0], gen_rtx_REG (QImode, A_REG));
  if (REGNO (operands[2]) != A_REG)
    output_asm_insn ("ld\ta,%2", operands);
  return "";
})

(define_insn "stm8_popw_x"
  [(set (reg:HI XH_REG)
	(mem:HI
	  (plus:HI (reg:HI SP_REG)
		   (const_int 1))))
   (set (reg:HI SP_REG)
	(plus:HI (reg:HI SP_REG)
		 (const_int 2)))]
  ""
  "popw\tx")


(define_insn "stm8_popw_y"
  [(set (reg:HI YH_REG)
	(mem:HI
	  (plus:HI (reg:HI SP_REG)
		   (const_int 1))))
   (set (reg:HI SP_REG)
	(plus:HI (reg:HI SP_REG)
		 (const_int 2)))]
  ""
  "popw\ty")

(define_insn "stm8_return_x"
  [(return)
   (use (reg:HI XH_REG))]
  ""
  "jp\t(x)")


(define_insn "stm8_return_y"
  [(return)
   (use (reg:HI YH_REG))]
  ""
  "jp\t(y)")

(define_expand "pushqi1"
  [(set (mem:QI (post_dec:HI (reg:HI SP_REG)))
	(match_operand:QI 0 "general_operand"))]
  ""
{
  if (!REG_P (operands[0]) && !CONST_INT_P (operands[0]))
    operands[0] = force_reg (QImode, operands[0]);

  emit_insn (gen_stm8_pushqi (operands[0]));
  DONE;
})


(define_insn "stm8_pushqi"
  [(set (mem:QI (post_dec:HI (reg:HI SP_REG)))
	(match_operand:QI 0 "nonmemory_operand" "ai"))]
  ""
  "push\t%0")

(define_expand "pushhi1"
  [(set (mem:HI (post_dec:HI (reg:HI SP_REG)))
	(match_operand:HI 0 "general_operand"))]
  ""
{
  if (!REG_P (operands[0]))
    operands[0] = force_reg (HImode, operands[0]);

  emit_insn (gen_stm8_pushhi (operands[0]));
  DONE;
})


(define_insn "stm8_pushhi"
  [(set (mem:HI (post_dec:HI (reg:HI SP_REG)))
	(match_operand:HI 0 "register_operand" "v"))]
  ""
  "pushw\t%0")

;; The generic wide POST_DEC move writes upward from the old SP, which
;; does not describe STM8's multi-byte push layout.  Snapshot components
;; before changing SP and push the least significant byte first.
(define_mode_iterator PUSH_MOVE [PSI SI DI SF DF CQI CHI CPSI CSI CDI SC DC])
;; gen_reg_rtx represents complex pseudos as CONCAT before allocation.
;; A general_operand alone would reject them and silently use the generic
;; POST_DEC fallback instead of this expander.
(define_predicate "stm8_push_operand"
  (ior (match_operand 0 "general_operand")
       (match_code "concat")))
(define_expand "push<mode>1"
  [(set (mem:PUSH_MOVE (post_dec:HI (reg:HI SP_REG)))
	(match_operand:PUSH_MOVE 0 "stm8_push_operand"))]
  ""
{
  stm8_expand_push (operands[0],<MODE>mode);
  DONE;
})

;; -------------------------------------------------------------------------
;; Integer operations
;; -------------------------------------------------------------------------

;; Constant and variable byte shifts.  STM8 shifts A by one bit per
;; instruction.  A variable count is zero-extended into X and counted down.

(define_code_iterator QI_SHIFT [ashift ashiftrt lshiftrt])
(define_code_attr qi_shift_name
  [(ashift "ashl") (ashiftrt "ashr") (lshiftrt "lshr")])
(define_code_attr qi_shift_insn
  [(ashift "sll") (ashiftrt "sra") (lshiftrt "srl")])

(define_predicate "stm8_qi_shift_count_operand"
  (and (match_code "const_int")
       (match_test "IN_RANGE (INTVAL (op), 0, 7)")))

(define_expand "<qi_shift_name>qi3"
  [(set (match_operand:QI 0 "register_operand")
	(QI_SHIFT:QI (match_operand:QI 1 "general_operand")
		     (match_operand:QI 2 "general_operand")))]
  ""
{
  push_temp_slots ();
  if (CONST_INT_P (operands[2]) && IN_RANGE (INTVAL (operands[2]), 0, 7))
    {
      operands[1] = force_reg (QImode, operands[1]);
      emit_insn (gen_stm8_<qi_shift_name>qi_const (operands[0], operands[1],
						    operands[2]));
      pop_temp_slots ();
      DONE;
    }

  rtx value = operands[1];
  if (!CONST_INT_P (value))
    {
      rtx slot = stm8_assign_stack_temp (QImode);
      emit_move_insn (slot, value);
      value = slot;
    }

  rtx count = force_reg (QImode, operands[2]);
  rtx a = gen_rtx_REG (QImode, A_REG);
  rtx x = gen_rtx_REG (HImode, XH_REG);

  emit_move_insn (a, count);
  emit_insn (gen_stm8_zero_extendqihi (x, a));
  emit_move_insn (a, value);
  emit_insn (gen_stm8_<qi_shift_name>qi_var ());
  emit_move_insn (operands[0], a);
  pop_temp_slots ();
  DONE;
})

(define_insn "stm8_<qi_shift_name>qi_const"
  [(set (match_operand:QI 0 "register_operand" "=a")
	(QI_SHIFT:QI (match_operand:QI 1 "register_operand" "0")
		     (match_operand:QI 2 "stm8_qi_shift_count_operand" "n")))]
  ""
{
  for (int i = 0; i < INTVAL (operands[2]); ++i)
    output_asm_insn ("<qi_shift_insn>\t%0", operands);
  return "";
})

(define_insn "stm8_<qi_shift_name>qi_var"
  [(set (reg:QI A_REG)
	(QI_SHIFT:QI (reg:QI A_REG) (reg:QI XL_REG)))
   (set (reg:HI XH_REG) (const_int 0))]
  ""
  "tnzw\tx\n\tjreq\t.Lstm8_qishift_done%=\n.Lstm8_qishift_loop%=:\n\t<qi_shi\
ft_insn>\ta\n\tdecw\tx\n\tjrne\t.Lstm8_qishift_loop%=\n.Lstm8_qishift_done%=:")

(define_insn "one_cmplqi2"
  [(set (match_operand:QI 0 "register_operand" "=a")
	(not:QI (match_operand:QI 1 "register_operand" "0")))]
  ""
  "cpl\t%0")

(define_insn "negqi2"
  [(set (match_operand:QI 0 "register_operand" "=a")
	(neg:QI (match_operand:QI 1 "register_operand" "0")))]
  ""
  "neg\t%0")

;; Native word shifts use X/Y.  Variable shifts reserve X for the value and
;; Y for the zero-extended QI count.

(define_code_iterator HI_SHIFT [ashift ashiftrt lshiftrt])
(define_code_attr hi_shift_name
  [(ashift "ashl") (ashiftrt "ashr") (lshiftrt "lshr")])
(define_code_attr hi_shift_insn
  [(ashift "sllw") (ashiftrt "sraw") (lshiftrt "srlw")])

(define_predicate "stm8_hi_shift_count_operand"
  (and (match_code "const_int")
       (match_test "IN_RANGE (INTVAL (op), 0, 15)")))

(define_expand "<hi_shift_name>hi3"
  [(set (match_operand:HI 0 "register_operand")
	(HI_SHIFT:HI (match_operand:HI 1 "general_operand")
		     (match_operand:QI 2 "general_operand")))]
  ""
{
  push_temp_slots ();
  if (CONST_INT_P (operands[2]) && IN_RANGE (INTVAL (operands[2]), 0, 15))
    {
      operands[1] = force_reg (HImode, operands[1]);
      emit_insn (gen_stm8_<hi_shift_name>hi_const (operands[0], operands[1],
						    operands[2]));
      pop_temp_slots ();
      DONE;
    }

  rtx value = operands[1];
  if (!CONST_INT_P (value))
    {
      rtx slot = stm8_assign_stack_temp (HImode);
      emit_move_insn (slot, value);
      value = slot;
    }

  rtx count = force_reg (QImode, operands[2]);
  rtx a = gen_rtx_REG (QImode, A_REG);
  rtx x = gen_rtx_REG (HImode, XH_REG);
  rtx y = gen_rtx_REG (HImode, YH_REG);

  emit_move_insn (a, count);
  emit_insn (gen_stm8_zero_extendqihi (y, a));
  emit_move_insn (x, value);
  emit_insn (gen_stm8_<hi_shift_name>hi_var ());
  emit_move_insn (operands[0], x);
  pop_temp_slots ();
  DONE;
})

(define_insn "stm8_<hi_shift_name>hi_const"
  [(set (match_operand:HI 0 "register_operand" "=v")
	(HI_SHIFT:HI (match_operand:HI 1 "register_operand" "0")
		     (match_operand:QI 2 "stm8_hi_shift_count_operand" "n")))]
  ""
{
  for (int i = 0; i < INTVAL (operands[2]); ++i)
    output_asm_insn ("<hi_shift_insn>\t%0", operands);
  return "";
})

(define_insn "stm8_<hi_shift_name>hi_var"
  [(set (reg:HI XH_REG)
	(HI_SHIFT:HI (reg:HI XH_REG) (reg:QI YL_REG)))
   (set (reg:HI YH_REG) (const_int 0))]
  ""
  "tnzw\ty\n\tjreq\t.Lstm8_hishift_done%=\n.Lstm8_hishift_loop%=:\n\t<hi_shi\
ft_insn>\tx\n\tdecw\ty\n\tjrne\t.Lstm8_hishift_loop%=\n.Lstm8_hishift_done%=:")

(define_insn "one_cmplhi2"
  [(set (match_operand:HI 0 "register_operand" "=v")
	(not:HI (match_operand:HI 1 "register_operand" "0")))]
  ""
  "cplw\t%0")

(define_insn "neghi2"
  [(set (match_operand:HI 0 "register_operand" "=v")
	(neg:HI (match_operand:HI 1 "register_operand" "0")))]
  ""
  "negw\t%0")

;; -------------------------------------------------------------------------
;; Integer conversions
;; -------------------------------------------------------------------------

;; GCC uses PSI for bitsizetype with 16-bit size_t, including VLA metadata.
;; Use byte-addressed conversions instead of misaligned HI subregs of PSI.

(define_expand "zero_extendqipsi2"
  [(set (match_operand:PSI 0 "nonimmediate_operand")
	(zero_extend:PSI (match_operand:QI 1 "general_operand")))]
  ""
{
  stm8_expand_integer_convert (operands[0], operands[1], QImode, false);
  DONE;
})

(define_expand "extendqipsi2"
  [(set (match_operand:PSI 0 "nonimmediate_operand")
	(sign_extend:PSI (match_operand:QI 1 "general_operand")))]
  ""
{
  stm8_expand_integer_convert (operands[0], operands[1], QImode, true);
  DONE;
})

(define_expand "zero_extendhipsi2"
  [(set (match_operand:PSI 0 "nonimmediate_operand")
	(zero_extend:PSI (match_operand:HI 1 "general_operand")))]
  ""
{
  stm8_expand_integer_convert (operands[0], operands[1], HImode, false);
  DONE;
})

(define_expand "extendhipsi2"
  [(set (match_operand:PSI 0 "nonimmediate_operand")
	(sign_extend:PSI (match_operand:HI 1 "general_operand")))]
  ""
{
  stm8_expand_integer_convert (operands[0], operands[1], HImode, true);
  DONE;
})

(define_expand "zero_extendpsisi2"
  [(set (match_operand:SI 0 "nonimmediate_operand")
	(zero_extend:SI (match_operand:PSI 1 "general_operand")))]
  ""
{
  stm8_expand_integer_convert (operands[0], operands[1], PSImode, false);
  DONE;
})

(define_expand "extendpsisi2"
  [(set (match_operand:SI 0 "nonimmediate_operand")
	(sign_extend:SI (match_operand:PSI 1 "general_operand")))]
  ""
{
  stm8_expand_integer_convert (operands[0], operands[1], PSImode, true);
  DONE;
})

(define_expand "zero_extendpsidi2"
  [(set (match_operand:DI 0 "nonimmediate_operand")
	(zero_extend:DI (match_operand:PSI 1 "general_operand")))]
  ""
{
  stm8_expand_integer_convert (operands[0], operands[1], PSImode, false);
  DONE;
})

(define_expand "extendpsidi2"
  [(set (match_operand:DI 0 "nonimmediate_operand")
	(sign_extend:DI (match_operand:PSI 1 "general_operand")))]
  ""
{
  stm8_expand_integer_convert (operands[0], operands[1], PSImode, true);
  DONE;
})

(define_expand "truncpsiqi2"
  [(set (match_operand:QI 0 "nonimmediate_operand")
	(truncate:QI (match_operand:PSI 1 "general_operand")))]
  ""
{
  stm8_expand_integer_convert (operands[0], operands[1], PSImode, false);
  DONE;
})

(define_expand "truncpsihi2"
  [(set (match_operand:HI 0 "nonimmediate_operand")
	(truncate:HI (match_operand:PSI 1 "general_operand")))]
  ""
{
  stm8_expand_integer_convert (operands[0], operands[1], PSImode, false);
  DONE;
})

(define_expand "truncsipsi2"
  [(set (match_operand:PSI 0 "nonimmediate_operand")
	(truncate:PSI (match_operand:SI 1 "general_operand")))]
  ""
{
  stm8_expand_integer_convert (operands[0], operands[1], SImode, false);
  DONE;
})

(define_expand "truncdipsi2"
  [(set (match_operand:PSI 0 "nonimmediate_operand")
	(truncate:PSI (match_operand:DI 1 "general_operand")))]
  ""
{
  stm8_expand_integer_convert (operands[0], operands[1], DImode, false);
  DONE;
})

(define_expand "zero_extendqihi2"
  [(set (match_operand:HI 0 "register_operand")
	(zero_extend:HI (match_operand:QI 1 "general_operand")))]
  ""
{
  operands[1] = force_reg (QImode, operands[1]);
  emit_insn (gen_stm8_zero_extendqihi (operands[0], operands[1]));
  DONE;
})

(define_insn "stm8_zero_extendqihi"
  [(set (match_operand:HI 0 "register_operand" "=v")
	(zero_extend:HI (match_operand:QI 1 "register_operand" "a")))]
  ""
{
  if (REGNO (operands[0]) == XH_REGNUM)
    return "ldw\tx,#0\n\tld\txl,a";

  gcc_assert (REGNO (operands[0]) == YH_REGNUM);
  return "ldw\ty,#0\n\tld\tyl,a";
})

(define_expand "extendqihi2"
  [(set (match_operand:HI 0 "register_operand")
	(sign_extend:HI (match_operand:QI 1 "general_operand")))]
  ""
{
  operands[1] = force_reg (QImode, operands[1]);
  emit_insn (gen_stm8_extendqihi (operands[0], operands[1]));
  DONE;
})

(define_insn "stm8_extendqihi"
  [(set (match_operand:HI 0 "register_operand" "=v")
	(sign_extend:HI (match_operand:QI 1 "register_operand" "a")))]
  ""
{
  if (REGNO (operands[0]) == XH_REGNUM)
    return "ldw\tx,#0\n\ttnz\ta\n\tjrpl\t.Lstm8_sext%=\n\tcplw\tx\n.Lstm8_sext%"
	   "=:\n\tld\txl,a";

  gcc_assert (REGNO (operands[0]) == YH_REGNUM);
  return "ldw\ty,#0\n\ttnz\ta\n\tjrpl\t.Lstm8_sext%=\n\tcplw\ty\n.Lstm8_sext%=:"
	 "\n\tld\tyl,a";
})

(define_expand "trunchiqi2"
  [(set (match_operand:QI 0 "register_operand")
	(truncate:QI (match_operand:HI 1 "general_operand")))]
  ""
{
  operands[1] = force_reg (HImode, operands[1]);
  emit_insn (gen_stm8_trunchiqi (operands[0], operands[1]));
  DONE;
})

(define_insn "stm8_trunchiqi"
  [(set (match_operand:QI 0 "register_operand" "=a")
	(truncate:QI (match_operand:HI 1 "register_operand" "v")))]
  ""
{
  if (REGNO (operands[1]) == XH_REGNUM)
    return "ld\ta,xl";

  gcc_assert (REGNO (operands[1]) == YH_REGNUM);
  return "ld\ta,yl";
})

;; SI is returned in Y:X.  Build extensions in a stable stack slot and let
;; the wide move support transfer the finished value to its final home.

(define_expand "zero_extendhisi2"
  [(set (match_operand:SI 0 "nonimmediate_operand")
	(zero_extend:SI (match_operand:HI 1 "general_operand")))]
  ""
{
  push_temp_slots ();
  rtx slot = stm8_assign_stack_temp (SImode);
  rtx value = force_reg (HImode, operands[1]);
  emit_move_insn (adjust_address (slot, HImode, 0), const0_rtx);
  emit_move_insn (adjust_address (slot, HImode, 2), value);
  emit_move_insn (operands[0], slot);
  pop_temp_slots ();
  DONE;
})

(define_expand "extendhisi2"
  [(set (match_operand:SI 0 "nonimmediate_operand")
	(sign_extend:SI (match_operand:HI 1 "general_operand")))]
  ""
{
  push_temp_slots ();
  rtx slot = stm8_assign_stack_temp (SImode);
  rtx value = force_reg (HImode, operands[1]);
  rtx sign = gen_reg_rtx (HImode);
  emit_move_insn (sign, value);
  emit_insn (gen_stm8_ashrhi_const (sign, sign, GEN_INT (15)));
  emit_move_insn (adjust_address (slot, HImode, 0), sign);
  emit_move_insn (adjust_address (slot, HImode, 2), value);
  emit_move_insn (operands[0], slot);
  pop_temp_slots ();
  DONE;
})

(define_expand "zero_extendqisi2"
  [(set (match_operand:SI 0 "nonimmediate_operand")
	(zero_extend:SI (match_operand:QI 1 "general_operand")))]
  ""
{
  rtx tmp = gen_reg_rtx (HImode);
  emit_insn (gen_zero_extendqihi2 (tmp, operands[1]));
  emit_insn (gen_zero_extendhisi2 (operands[0], tmp));
  DONE;
})

(define_expand "extendqisi2"
  [(set (match_operand:SI 0 "nonimmediate_operand")
	(sign_extend:SI (match_operand:QI 1 "general_operand")))]
  ""
{
  rtx tmp = gen_reg_rtx (HImode);
  emit_insn (gen_extendqihi2 (tmp, operands[1]));
  emit_insn (gen_extendhisi2 (operands[0], tmp));
  DONE;
})

(define_expand "truncsihi2"
  [(set (match_operand:HI 0 "nonimmediate_operand")
	(truncate:HI (match_operand:SI 1 "general_operand")))]
  ""
{
  push_temp_slots ();
  rtx slot = stm8_assign_stack_temp (SImode);
  emit_move_insn (slot, operands[1]);
  emit_move_insn (operands[0], adjust_address (slot, HImode, 2));
  pop_temp_slots ();
  DONE;
})

(define_expand "truncsiqi2"
  [(set (match_operand:QI 0 "nonimmediate_operand")
	(truncate:QI (match_operand:SI 1 "general_operand")))]
  ""
{
  push_temp_slots ();
  rtx slot = stm8_assign_stack_temp (SImode);
  emit_move_insn (slot, operands[1]);
  emit_move_insn (operands[0], adjust_address (slot, QImode, 3));
  pop_temp_slots ();
  DONE;
})

;; DImode has no hard-register home.  All extensions/truncations therefore
;; use memory explicitly and never invent a non-existent 64-bit return
;; register class.

(define_expand "zero_extendhidi2"
  [(set (match_operand:DI 0 "nonimmediate_operand")
	(zero_extend:DI (match_operand:HI 1 "general_operand")))]
  ""
{
  push_temp_slots ();
  rtx slot = stm8_assign_stack_temp (DImode);
  rtx value = force_reg (HImode, operands[1]);
  emit_move_insn (adjust_address (slot, HImode, 0), const0_rtx);
  emit_move_insn (adjust_address (slot, HImode, 2), const0_rtx);
  emit_move_insn (adjust_address (slot, HImode, 4), const0_rtx);
  emit_move_insn (adjust_address (slot, HImode, 6), value);
  emit_move_insn (operands[0], slot);
  pop_temp_slots ();
  DONE;
})

(define_expand "extendhidi2"
  [(set (match_operand:DI 0 "nonimmediate_operand")
	(sign_extend:DI (match_operand:HI 1 "general_operand")))]
  ""
{
  push_temp_slots ();
  rtx slot = stm8_assign_stack_temp (DImode);
  rtx value = force_reg (HImode, operands[1]);
  rtx sign = gen_reg_rtx (HImode);
  emit_move_insn (sign, value);
  emit_insn (gen_stm8_ashrhi_const (sign, sign, GEN_INT (15)));
  emit_move_insn (adjust_address (slot, HImode, 0), sign);
  emit_move_insn (adjust_address (slot, HImode, 2), sign);
  emit_move_insn (adjust_address (slot, HImode, 4), sign);
  emit_move_insn (adjust_address (slot, HImode, 6), value);
  emit_move_insn (operands[0], slot);
  pop_temp_slots ();
  DONE;
})

(define_expand "zero_extendqidi2"
  [(set (match_operand:DI 0 "nonimmediate_operand")
	(zero_extend:DI (match_operand:QI 1 "general_operand")))]
  ""
{
  rtx tmp = gen_reg_rtx (HImode);
  emit_insn (gen_zero_extendqihi2 (tmp, operands[1]));
  emit_insn (gen_zero_extendhidi2 (operands[0], tmp));
  DONE;
})

(define_expand "extendqidi2"
  [(set (match_operand:DI 0 "nonimmediate_operand")
	(sign_extend:DI (match_operand:QI 1 "general_operand")))]
  ""
{
  rtx tmp = gen_reg_rtx (HImode);
  emit_insn (gen_extendqihi2 (tmp, operands[1]));
  emit_insn (gen_extendhidi2 (operands[0], tmp));
  DONE;
})

(define_expand "zero_extendsidi2"
  [(set (match_operand:DI 0 "nonimmediate_operand")
	(zero_extend:DI (match_operand:SI 1 "general_operand")))]
  ""
{
  push_temp_slots ();
  rtx src = stm8_assign_stack_temp (SImode);
  rtx dest = stm8_assign_stack_temp (DImode);
  emit_move_insn (src, operands[1]);
  emit_move_insn (adjust_address (dest, HImode, 0), const0_rtx);
  emit_move_insn (adjust_address (dest, HImode, 2), const0_rtx);
  emit_move_insn (adjust_address (dest, SImode, 4), src);
  emit_move_insn (operands[0], dest);
  pop_temp_slots ();
  DONE;
})

(define_expand "extendsidi2"
  [(set (match_operand:DI 0 "nonimmediate_operand")
	(sign_extend:DI (match_operand:SI 1 "general_operand")))]
  ""
{
  push_temp_slots ();
  rtx src = stm8_assign_stack_temp (SImode);
  rtx dest = stm8_assign_stack_temp (DImode);
  rtx top = gen_reg_rtx (HImode);
  rtx sign = gen_reg_rtx (HImode);
  emit_move_insn (src, operands[1]);
  emit_move_insn (top, adjust_address (src, HImode, 0));
  emit_move_insn (sign, top);
  emit_insn (gen_stm8_ashrhi_const (sign, sign, GEN_INT (15)));
  emit_move_insn (adjust_address (dest, HImode, 0), sign);
  emit_move_insn (adjust_address (dest, HImode, 2), sign);
  emit_move_insn (adjust_address (dest, SImode, 4), src);
  emit_move_insn (operands[0], dest);
  pop_temp_slots ();
  DONE;
})

(define_expand "truncdihi2"
  [(set (match_operand:HI 0 "nonimmediate_operand")
	(truncate:HI (match_operand:DI 1 "general_operand")))]
  ""
{
  push_temp_slots ();
  rtx slot = stm8_assign_stack_temp (DImode);
  emit_move_insn (slot, operands[1]);
  emit_move_insn (operands[0], adjust_address (slot, HImode, 6));
  pop_temp_slots ();
  DONE;
})

(define_expand "truncdiqi2"
  [(set (match_operand:QI 0 "nonimmediate_operand")
	(truncate:QI (match_operand:DI 1 "general_operand")))]
  ""
{
  push_temp_slots ();
  rtx slot = stm8_assign_stack_temp (DImode);
  emit_move_insn (slot, operands[1]);
  emit_move_insn (operands[0], adjust_address (slot, QImode, 7));
  pop_temp_slots ();
  DONE;
})

(define_expand "truncdisi2"
  [(set (match_operand:SI 0 "nonimmediate_operand")
	(truncate:SI (match_operand:DI 1 "general_operand")))]
  ""
{
  push_temp_slots ();
  rtx slot = stm8_assign_stack_temp (DImode);
  emit_move_insn (slot, operands[1]);
  emit_move_insn (operands[0], adjust_address (slot, SImode, 4));
  pop_temp_slots ();
  DONE;
})

;; -------------------------------------------------------------------------
;; HI logical operations
;; -------------------------------------------------------------------------

(define_code_iterator HI_LOGICAL [and ior xor])
(define_code_attr hi_logical_name
  [(and "and") (ior "ior") (xor "xor")])
(define_code_attr hi_logical_insn
  [(and "and") (ior "or") (xor "xor")])

(define_expand "<hi_logical_name>hi3"
  [(set (match_operand:HI 0 "nonimmediate_operand")
	(HI_LOGICAL:HI (match_operand:HI 1 "general_operand")
		       (match_operand:HI 2 "general_operand")))]
  ""
{
  push_temp_slots ();
  rtx rhs = stm8_assign_stack_temp (HImode);
  rtx out = stm8_assign_stack_temp (HImode);
  emit_move_insn (out, operands[1]);
  emit_move_insn (rhs, operands[2]);
  emit_insn (gen_stm8_<hi_logical_name>hi_mem (out, rhs));
  emit_move_insn (operands[0], out);
  pop_temp_slots ();
  DONE;
})

(define_insn "stm8_<hi_logical_name>hi_mem"
  [(set (match_operand:HI 0 "memory_operand" "+R")
	(HI_LOGICAL:HI (match_dup 0)
		       (match_operand:HI 1 "memory_operand" "R")))
   (clobber (reg:QI A_REG))]
  ""
{
  for (unsigned int i = 0; i < 2; ++i)
    {
      rtx xops[3];
      xops[0] = adjust_address (operands[0], QImode, i);
      xops[1] = adjust_address (operands[0], QImode, i);
      xops[2] = adjust_address (operands[1], QImode, i);
      output_asm_insn ("ld\ta,%1", xops);
      output_asm_insn ("<hi_logical_insn>\ta,%2", xops);
      output_asm_insn ("ld\t%0,a", xops);
    }
  return "";
})

;; -------------------------------------------------------------------------
;; Native QI/HI multiply and unsigned divide/modulo
;; -------------------------------------------------------------------------

(define_expand "mulqi3"
  [(set (match_operand:QI 0 "register_operand")
	(mult:QI (match_operand:QI 1 "general_operand")
		 (match_operand:QI 2 "general_operand")))]
  ""
{
  rtx product = gen_reg_rtx (HImode);
  emit_insn (gen_umulqihi3 (product, operands[1], operands[2]));
  emit_insn (gen_trunchiqi2 (operands[0], product));
  DONE;
})

(define_expand "umulqihi3"
  [(set (match_operand:HI 0 "register_operand")
	(mult:HI
	  (zero_extend:HI (match_operand:QI 1 "general_operand"))
	  (zero_extend:HI (match_operand:QI 2 "general_operand"))))]
  ""
{
  push_temp_slots ();
  rtx x = gen_rtx_REG (HImode, XH_REG);
  rtx xl = gen_rtx_REG (QImode, XL_REG);
  rtx a = gen_rtx_REG (QImode, A_REG);

  rtx rhs = operands[2];
  if (!CONST_INT_P (rhs))
    {
      rtx slot = stm8_assign_stack_temp (QImode);
      emit_move_insn (slot, rhs);
      rhs = slot;
    }

  emit_move_insn (a, operands[1]);
  emit_move_insn (xl, a);
  emit_move_insn (a, rhs);
  emit_insn (gen_stm8_umulqihi ());
  emit_move_insn (operands[0], x);
  pop_temp_slots ();
  DONE;
})

(define_insn "stm8_umulqihi"
  [(set (reg:HI XH_REG)
	(mult:HI (zero_extend:HI (reg:QI XL_REG))
		 (zero_extend:HI (reg:QI A_REG))))]
  ""
  "mul\tx,a")

(define_expand "udivmodhi4"
  [(parallel
     [(set (match_operand:HI 0 "register_operand")
	   (udiv:HI (match_operand:HI 1 "general_operand")
		    (match_operand:HI 2 "general_operand")))
      (set (match_operand:HI 3 "register_operand")
	   (umod:HI (match_dup 1) (match_dup 2)))])]
  ""
{
  push_temp_slots ();
  rtx x = gen_rtx_REG (HImode, XH_REG);
  rtx y = gen_rtx_REG (HImode, YH_REG);
  rtx divisor = stm8_assign_stack_temp (HImode);
  emit_move_insn (divisor, operands[2]);
  emit_move_insn (x, operands[1]);
  emit_move_insn (y, divisor);
  emit_insn (gen_stm8_udivmodhi ());
  emit_move_insn (operands[0], x);
  emit_move_insn (operands[3], y);
  pop_temp_slots ();
  DONE;
})

(define_insn "stm8_udivmodhi"
  [(set (reg:HI XH_REG)
	(udiv:HI (reg:HI XH_REG) (reg:HI YH_REG)))
   (set (reg:HI YH_REG)
	(umod:HI (reg:HI XH_REG) (reg:HI YH_REG)))]
  ""
  "divw\tx,y")

(define_expand "udivmodqi4"
  [(parallel
     [(set (match_operand:QI 0 "register_operand")
	   (udiv:QI (match_operand:QI 1 "general_operand")
		    (match_operand:QI 2 "general_operand")))
      (set (match_operand:QI 3 "register_operand")
	   (umod:QI (match_dup 1) (match_dup 2)))])]
  ""
{
  push_temp_slots ();
  rtx x = gen_rtx_REG (HImode, XH_REG);
  rtx xl = gen_rtx_REG (QImode, XL_REG);
  rtx a = gen_rtx_REG (QImode, A_REG);
  rtx divisor = stm8_assign_stack_temp (QImode);
  emit_move_insn (divisor, operands[2]);
  rtx dividend = force_reg (QImode, operands[1]);
  emit_insn (gen_stm8_zero_extendqihi (x, dividend));
  emit_move_insn (a, divisor);
  emit_insn (gen_stm8_udivmodqi ());

  rtx quotient = gen_reg_rtx (QImode);
  rtx remainder = gen_reg_rtx (QImode);
  emit_move_insn (remainder, a);
  emit_move_insn (quotient, xl);
  emit_move_insn (operands[0], quotient);
  emit_move_insn (operands[3], remainder);
  pop_temp_slots ();
  DONE;
})

(define_insn "stm8_udivmodqi"
  [(set (reg:HI XH_REG)
	(udiv:HI (reg:HI XH_REG)
		 (zero_extend:HI (reg:QI A_REG))))
   (set (reg:QI A_REG)
	(truncate:QI
	  (umod:HI (reg:HI XH_REG)
		   (zero_extend:HI (reg:QI A_REG)))))]
  ""
  "div\tx,a")


(define_expand "udivhi3"
  [(set (match_operand:HI 0 "register_operand")
	(udiv:HI (match_operand:HI 1 "general_operand")
		 (match_operand:HI 2 "general_operand")))]
  ""
{
  push_temp_slots ();
  rtx x = gen_rtx_REG (HImode, XH_REG);
  rtx y = gen_rtx_REG (HImode, YH_REG);
  rtx divisor = stm8_assign_stack_temp (HImode);
  emit_move_insn (divisor, operands[2]);
  emit_move_insn (x, operands[1]);
  emit_move_insn (y, divisor);
  emit_insn (gen_stm8_udivmodhi ());
  emit_move_insn (operands[0], x);
  pop_temp_slots ();
  DONE;
})

(define_expand "umodhi3"
  [(set (match_operand:HI 0 "register_operand")
	(umod:HI (match_operand:HI 1 "general_operand")
		 (match_operand:HI 2 "general_operand")))]
  ""
{
  push_temp_slots ();
  rtx x = gen_rtx_REG (HImode, XH_REG);
  rtx y = gen_rtx_REG (HImode, YH_REG);
  rtx divisor = stm8_assign_stack_temp (HImode);
  emit_move_insn (divisor, operands[2]);
  emit_move_insn (x, operands[1]);
  emit_move_insn (y, divisor);
  emit_insn (gen_stm8_udivmodhi ());
  emit_move_insn (operands[0], y);
  pop_temp_slots ();
  DONE;
})

(define_expand "udivqi3"
  [(set (match_operand:QI 0 "register_operand")
	(udiv:QI (match_operand:QI 1 "general_operand")
		 (match_operand:QI 2 "general_operand")))]
  ""
{
  push_temp_slots ();
  rtx x = gen_rtx_REG (HImode, XH_REG);
  rtx xl = gen_rtx_REG (QImode, XL_REG);
  rtx a = gen_rtx_REG (QImode, A_REG);
  rtx divisor = stm8_assign_stack_temp (QImode);
  emit_move_insn (divisor, operands[2]);
  rtx dividend = force_reg (QImode, operands[1]);
  emit_insn (gen_stm8_zero_extendqihi (x, dividend));
  emit_move_insn (a, divisor);
  emit_insn (gen_stm8_udivmodqi ());
  emit_move_insn (operands[0], xl);
  pop_temp_slots ();
  DONE;
})

(define_expand "umodqi3"
  [(set (match_operand:QI 0 "register_operand")
	(umod:QI (match_operand:QI 1 "general_operand")
		 (match_operand:QI 2 "general_operand")))]
  ""
{
  push_temp_slots ();
  rtx x = gen_rtx_REG (HImode, XH_REG);
  rtx a = gen_rtx_REG (QImode, A_REG);
  rtx divisor = stm8_assign_stack_temp (QImode);
  emit_move_insn (divisor, operands[2]);
  rtx dividend = force_reg (QImode, operands[1]);
  emit_insn (gen_stm8_zero_extendqihi (x, dividend));
  emit_move_insn (a, divisor);
  emit_insn (gen_stm8_udivmodqi ());
  emit_move_insn (operands[0], a);
  pop_temp_slots ();
  DONE;
})

;; Software operations whose scalar return modes have established ABI homes.
;; Use explicit standard libcall names rather than relying on generic libgcc2:
;; a 16-bit target does not get all QI/HI/SI multiply/divide routines from
;; libgcc2 automatically.  The companion stm8-int-runtime.c supplies them.

(define_expand "divqi3"
  [(set (match_operand:QI 0 "register_operand")
	(div:QI (match_operand:QI 1 "general_operand")
		(match_operand:QI 2 "general_operand")))]
  ""
{
  rtx fun = gen_rtx_SYMBOL_REF (Pmode, "__divqi3");
  rtx value
      = emit_library_call_value (fun, NULL_RTX, LCT_NORMAL, QImode, operands[1],
				 QImode, operands[2], QImode);
  emit_move_insn (operands[0], value);
  DONE;
})

(define_expand "modqi3"
  [(set (match_operand:QI 0 "register_operand")
	(mod:QI (match_operand:QI 1 "general_operand")
		(match_operand:QI 2 "general_operand")))]
  ""
{
  rtx fun = gen_rtx_SYMBOL_REF (Pmode, "__modqi3");
  rtx value
      = emit_library_call_value (fun, NULL_RTX, LCT_NORMAL, QImode, operands[1],
				 QImode, operands[2], QImode);
  emit_move_insn (operands[0], value);
  DONE;
})

(define_expand "mulhi3"
  [(set (match_operand:HI 0 "register_operand")
	(mult:HI (match_operand:HI 1 "general_operand")
		 (match_operand:HI 2 "general_operand")))]
  ""
{
  rtx fun = gen_rtx_SYMBOL_REF (Pmode, "__mulhi3");
  rtx value
      = emit_library_call_value (fun, NULL_RTX, LCT_NORMAL, HImode, operands[1],
				 HImode, operands[2], HImode);
  emit_move_insn (operands[0], value);
  DONE;
})

(define_expand "divhi3"
  [(set (match_operand:HI 0 "register_operand")
	(div:HI (match_operand:HI 1 "general_operand")
		(match_operand:HI 2 "general_operand")))]
  ""
{
  rtx fun = gen_rtx_SYMBOL_REF (Pmode, "__divhi3");
  rtx value
      = emit_library_call_value (fun, NULL_RTX, LCT_NORMAL, HImode, operands[1],
				 HImode, operands[2], HImode);
  emit_move_insn (operands[0], value);
  DONE;
})

(define_expand "modhi3"
  [(set (match_operand:HI 0 "register_operand")
	(mod:HI (match_operand:HI 1 "general_operand")
		(match_operand:HI 2 "general_operand")))]
  ""
{
  rtx fun = gen_rtx_SYMBOL_REF (Pmode, "__modhi3");
  rtx value
      = emit_library_call_value (fun, NULL_RTX, LCT_NORMAL, HImode, operands[1],
				 HImode, operands[2], HImode);
  emit_move_insn (operands[0], value);
  DONE;
})

;; SImode has the established Y:X return ABI, so normal value-returning
;; libcalls are valid for the operations that STM8 cannot implement compactly.

;; Generic optab widening initializes a PSI lowpart of an SI register.
;; Its big-endian byte offset is one, which validate_subreg rejects.
;; Convert explicitly: the low 24 product bits are independent of the
;; extension, and truncation modulo 2^24 preserves PSI multiplication.
(define_expand "mulpsi3"
  [(set (match_operand:PSI 0 "nonimmediate_operand")
	(mult:PSI (match_operand:PSI 1 "general_operand")
		  (match_operand:PSI 2 "general_operand")))]
  ""
{
  rtx lhs = gen_reg_rtx (SImode);
  rtx rhs = gen_reg_rtx (SImode);
  rtx product = gen_reg_rtx (SImode);
  emit_insn (gen_zero_extendpsisi2 (lhs, operands[1]));
  emit_insn (gen_zero_extendpsisi2 (rhs, operands[2]));
  emit_insn (gen_mulsi3 (product, lhs, rhs));
  emit_insn (gen_truncsipsi2 (operands[0], product));
  DONE;
})

(define_expand "mulsi3"
  [(set (match_operand:SI 0 "register_operand")
	(mult:SI (match_operand:SI 1 "general_operand")
		 (match_operand:SI 2 "general_operand")))]
  ""
{
  rtx fun = gen_rtx_SYMBOL_REF (Pmode, "__mulsi3");
  rtx value
      = emit_library_call_value (fun, NULL_RTX, LCT_NORMAL, SImode, operands[1],
				 SImode, operands[2], SImode);
  emit_move_insn (operands[0], value);
  DONE;
})

(define_expand "udivsi3"
  [(set (match_operand:SI 0 "register_operand")
	(udiv:SI (match_operand:SI 1 "general_operand")
		 (match_operand:SI 2 "general_operand")))]
  ""
{
  rtx fun = gen_rtx_SYMBOL_REF (Pmode, "__udivsi3");
  rtx value
      = emit_library_call_value (fun, NULL_RTX, LCT_NORMAL, SImode, operands[1],
				 SImode, operands[2], SImode);
  emit_move_insn (operands[0], value);
  DONE;
})

(define_expand "umodsi3"
  [(set (match_operand:SI 0 "register_operand")
	(umod:SI (match_operand:SI 1 "general_operand")
		 (match_operand:SI 2 "general_operand")))]
  ""
{
  rtx fun = gen_rtx_SYMBOL_REF (Pmode, "__umodsi3");
  rtx value
      = emit_library_call_value (fun, NULL_RTX, LCT_NORMAL, SImode, operands[1],
				 SImode, operands[2], SImode);
  emit_move_insn (operands[0], value);
  DONE;
})

(define_expand "divsi3"
  [(set (match_operand:SI 0 "register_operand")
	(div:SI (match_operand:SI 1 "general_operand")
		(match_operand:SI 2 "general_operand")))]
  ""
{
  rtx fun = gen_rtx_SYMBOL_REF (Pmode, "__divsi3");
  rtx value
      = emit_library_call_value (fun, NULL_RTX, LCT_NORMAL, SImode, operands[1],
				 SImode, operands[2], SImode);
  emit_move_insn (operands[0], value);
  DONE;
})

(define_expand "modsi3"
  [(set (match_operand:SI 0 "register_operand")
	(mod:SI (match_operand:SI 1 "general_operand")
		(match_operand:SI 2 "general_operand")))]
  ""
{
  rtx fun = gen_rtx_SYMBOL_REF (Pmode, "__modsi3");
  rtx value
      = emit_library_call_value (fun, NULL_RTX, LCT_NORMAL, SImode, operands[1],
				 SImode, operands[2], SImode);
  emit_move_insn (operands[0], value);
  DONE;
})

;; -------------------------------------------------------------------------
;; Comparisons
;; -------------------------------------------------------------------------

(define_predicate "stm8_comparison_operator"
  (match_code "eq,ne,lt,le,gt,ge,ltu,leu,gtu,geu"))

(define_expand "cbranchhi4"
  [(set (pc)
	(if_then_else
	  (match_operator 0 "stm8_comparison_operator"
	    [(match_operand:HI 1 "general_operand")
	     (match_operand:HI 2 "general_operand")])
	  (label_ref (match_operand 3 "" ""))
	  (pc)))]
  ""
{
  push_temp_slots ();
  rtx rhs = operands[2];
  if (!CONST_INT_P (rhs))
    {
      rtx slot = stm8_assign_stack_temp (HImode);
      emit_move_insn (slot, rhs);
      rhs = slot;
    }

  rtx x = gen_rtx_REG (HImode, XH_REG);
  emit_move_insn (x, operands[1]);
  emit_jump_insn (gen_stm8_cbranchhi (operands[0], x, rhs, operands[3]));
  pop_temp_slots ();
  DONE;
})

(define_insn "stm8_cbranchhi"
  [(set (pc)
	(if_then_else
	  (match_operator 0 "stm8_comparison_operator"
	    [(match_operand:HI 1 "register_operand" "x")
	     (match_operand:HI 2 "stm8_word_rhs_operand" "Ri")])
	  (label_ref (match_operand 3 "" ""))
	  (pc)))]
  ""
{
  enum rtx_code code = GET_CODE (operands[0]);

  if (operands[2] == const0_rtx && (code == EQ || code == NE))
    output_asm_insn ("tnzw\t%1", operands);
  else
    output_asm_insn ("cpw\t%1,%2", operands);

  switch (code)
    {
    case EQ:
      return "jrne\t.Lstm8_cbranchhi%=\n\t%J3\t%l3\n.Lstm8_cbranchhi%=:";
    case NE:
      return "jreq\t.Lstm8_cbranchhi%=\n\t%J3\t%l3\n.Lstm8_cbranchhi%=:";
    case LT:
      return "jrsge\t.Lstm8_cbranchhi%=\n\t%J3\t%l3\n.Lstm8_cbranchhi%=:";
    case LE:
      return "jrsgt\t.Lstm8_cbranchhi%=\n\t%J3\t%l3\n.Lstm8_cbranchhi%=:";
    case GT:
      return "jrsle\t.Lstm8_cbranchhi%=\n\t%J3\t%l3\n.Lstm8_cbranchhi%=:";
    case GE:
      return "jrslt\t.Lstm8_cbranchhi%=\n\t%J3\t%l3\n.Lstm8_cbranchhi%=:";
    case LTU:
      return "jruge\t.Lstm8_cbranchhi%=\n\t%J3\t%l3\n.Lstm8_cbranchhi%=:";
    case LEU:
      return "jrugt\t.Lstm8_cbranchhi%=\n\t%J3\t%l3\n.Lstm8_cbranchhi%=:";
    case GTU:
      return "jrule\t.Lstm8_cbranchhi%=\n\t%J3\t%l3\n.Lstm8_cbranchhi%=:";
    case GEU:
      return "jrult\t.Lstm8_cbranchhi%=\n\t%J3\t%l3\n.Lstm8_cbranchhi%=:";
    default:
      gcc_unreachable ();
    }
})

(define_predicate "stm8_byte_rhs_operand"
  (ior (match_operand 0 "memory_operand")
       (match_operand 0 "immediate_operand")))

(define_code_iterator BYTE_BINARY [plus minus and ior xor])
(define_code_attr byte_binary_name
  [(plus "add") (minus "sub") (and "and") (ior "ior") (xor "xor")])
(define_code_attr byte_binary_insn
  [(plus "add") (minus "sub") (and "and") (ior "or") (xor "xor")])

(define_expand "<byte_binary_name>qi3"
  [(set (match_operand:QI 0 "register_operand")
	(BYTE_BINARY:QI (match_operand:QI 1 "general_operand")
			(match_operand:QI 2 "general_operand")))]
  ""
{
  push_temp_slots ();
  if (<CODE> == PLUS && register_operand (operands[1], QImode)
      && rtx_equal_p (operands[1], operands[2]))
    {
      emit_insn (gen_stm8_addqi_double (operands[0], operands[1]));
      pop_temp_slots ();
      DONE;
    }

  if (<CODE> != MINUS && CONST_INT_P (operands[1])
      && !CONST_INT_P (operands[2]))
    std::swap (operands[1], operands[2]);

  operands[2] = stm8_prepare_byte_rhs (operands[2]);
  operands[1] = force_reg (QImode, operands[1]);
  emit_insn (
      gen_stm8_<byte_binary_name>qi (operands[0], operands[1], operands[2]));
  pop_temp_slots ();
  DONE;
})

(define_insn "stm8_<byte_binary_name>qi"
  [(set (match_operand:QI 0 "register_operand" "=a")
	(BYTE_BINARY:QI (match_operand:QI 1 "register_operand" "0")
			(match_operand:QI 2 "stm8_byte_rhs_operand" "Ri")))]
  ""
  "<byte_binary_insn>\t%0,%2")

(define_insn "stm8_addqi_double"
  [(set (match_operand:QI 0 "register_operand" "=a")
	(plus:QI (match_operand:QI 1 "register_operand" "0")
		 (match_dup 1)))]
  ""
  "sll\t%0")

(define_expand "cbranchqi4"
  [(set (pc)
	(if_then_else
	  (match_operator 0 "stm8_comparison_operator"
	    [(match_operand:QI 1 "register_operand")
	     (match_operand:QI 2 "general_operand")])
	  (label_ref (match_operand 3 "" ""))
	  (pc)))]
  ""
{
  push_temp_slots ();
  operands[2] = stm8_prepare_byte_rhs (operands[2]);
  emit_jump_insn (
      gen_stm8_cbranchqi (operands[0], operands[1], operands[2], operands[3]));
  pop_temp_slots ();
  DONE;
})

(define_insn "stm8_cbranchqi"
  [(set (pc)
	(if_then_else
	  (match_operator 0 "stm8_comparison_operator"
	    [(match_operand:QI 1 "register_operand" "a")
	     (match_operand:QI 2 "stm8_byte_rhs_operand" "Ri")])
	  (label_ref (match_operand 3 "" ""))
	  (pc)))]
  ""
{
  enum rtx_code code = GET_CODE (operands[0]);

  if (operands[2] == const0_rtx && (code == EQ || code == NE))
    output_asm_insn ("tnz\t%1", operands);
  else
    output_asm_insn ("cp\t%1,%2", operands);

  switch (code)
    {
    case EQ:
      return "jrne\t.Lstm8_cbranch%=\n\t%J3\t%l3\n.Lstm8_cbranch%=:";
    case NE:
      return "jreq\t.Lstm8_cbranch%=\n\t%J3\t%l3\n.Lstm8_cbranch%=:";
    case LT:
      return "jrsge\t.Lstm8_cbranch%=\n\t%J3\t%l3\n.Lstm8_cbranch%=:";
    case LE:
      return "jrsgt\t.Lstm8_cbranch%=\n\t%J3\t%l3\n.Lstm8_cbranch%=:";
    case GT:
      return "jrsle\t.Lstm8_cbranch%=\n\t%J3\t%l3\n.Lstm8_cbranch%=:";
    case GE:
      return "jrslt\t.Lstm8_cbranch%=\n\t%J3\t%l3\n.Lstm8_cbranch%=:";
    case LTU:
      return "jruge\t.Lstm8_cbranch%=\n\t%J3\t%l3\n.Lstm8_cbranch%=:";
    case LEU:
      return "jrugt\t.Lstm8_cbranch%=\n\t%J3\t%l3\n.Lstm8_cbranch%=:";
    case GTU:
      return "jrule\t.Lstm8_cbranch%=\n\t%J3\t%l3\n.Lstm8_cbranch%=:";
    case GEU:
      return "jrult\t.Lstm8_cbranch%=\n\t%J3\t%l3\n.Lstm8_cbranch%=:";
    default:
      gcc_unreachable ();
    }
})

;; -------------------------------------------------------------------------
;; 32/64-bit arithmetic and logical operations
;; -------------------------------------------------------------------------

;; SI/DI values are snapshotted to stack memory.  A single target insn then
;; emits the whole byte chain atomically, so ADD/ADC and SUB/SBC carry state
;; cannot be separated by scheduling.  This also avoids pretending that DI
;; has a hard-register home.  match_dup keeps the operation in place even
;; after combine: only two memory addresses can occupy X/Y together.

(define_mode_iterator WIDE_INT [PSI SI DI])
(define_code_iterator WIDE_BINARY [plus minus and ior xor])
(define_code_attr wide_binary_name
  [(plus "add") (minus "sub") (and "and") (ior "ior") (xor "xor")])
(define_code_attr wide_binary_insn
  [(plus "add") (minus "sub") (and "and") (ior "or") (xor "xor")])

(define_expand "<wide_binary_name><mode>3"
  [(set (match_operand:WIDE_INT 0 "nonimmediate_operand")
	(WIDE_BINARY:WIDE_INT
	  (match_operand:WIDE_INT 1 "general_operand")
	  (match_operand:WIDE_INT 2 "general_operand")))]
  ""
{
  push_temp_slots ();
  rtx rhs = stm8_assign_stack_temp (<MODE>mode);
  rtx out = stm8_assign_stack_temp (<MODE>mode);
  emit_move_insn (out, operands[1]);
  emit_move_insn (rhs, operands[2]);
  emit_insn (gen_stm8_<wide_binary_name><mode>_mem (out, rhs));
  emit_move_insn (operands[0], out);
  pop_temp_slots ();
  DONE;
})

(define_insn "stm8_<wide_binary_name><mode>_mem"
  [(set (match_operand:WIDE_INT 0 "memory_operand" "+R")
	(WIDE_BINARY:WIDE_INT
	  (match_dup 0)
	  (match_operand:WIDE_INT 1 "memory_operand" "R")))
   (clobber (reg:QI A_REG))]
  ""
{
  unsigned int size = GET_MODE_SIZE (<MODE>mode);

  if (<CODE> == PLUS || <CODE> == MINUS)
    {
      for (unsigned int n = 0; n < size; ++n)
	{
	  unsigned int i = size - 1 - n;
	  rtx xops[3];
	  xops[0] = adjust_address (operands[0], QImode, i);
	  xops[1] = adjust_address (operands[0], QImode, i);
	  xops[2] = adjust_address (operands[1], QImode, i);
	  output_asm_insn ("ld\ta,%1", xops);
	  if (<CODE> == PLUS)
	    output_asm_insn (n == 0 ? "add\ta,%2" : "adc\ta,%2", xops);
	  else
	    output_asm_insn (n == 0 ? "sub\ta,%2" : "sbc\ta,%2", xops);
	  output_asm_insn ("ld\t%0,a", xops);
	}
    }
  else
    {
      for (unsigned int i = 0; i < size; ++i)
	{
	  rtx xops[3];
	  xops[0] = adjust_address (operands[0], QImode, i);
	  xops[1] = adjust_address (operands[0], QImode, i);
	  xops[2] = adjust_address (operands[1], QImode, i);
	  output_asm_insn ("ld\ta,%1", xops);
	  output_asm_insn ("<wide_binary_insn>\ta,%2", xops);
	  output_asm_insn ("ld\t%0,a", xops);
	}
    }
  return "";
})

(define_expand "one_cmpl<mode>2"
  [(set (match_operand:WIDE_INT 0 "nonimmediate_operand")
	(not:WIDE_INT (match_operand:WIDE_INT 1 "general_operand")))]
  ""
{
  push_temp_slots ();
  rtx src = stm8_assign_stack_temp (<MODE>mode);
  rtx out = stm8_assign_stack_temp (<MODE>mode);
  emit_move_insn (src, operands[1]);
  emit_insn (gen_stm8_one_cmpl<mode>_mem (out, src));
  emit_move_insn (operands[0], out);
  pop_temp_slots ();
  DONE;
})

(define_insn "stm8_one_cmpl<mode>_mem"
  [(set (match_operand:WIDE_INT 0 "memory_operand" "=R")
	(not:WIDE_INT (match_operand:WIDE_INT 1 "memory_operand" "R")))
   (clobber (reg:QI A_REG))]
  ""
{
  unsigned int size = GET_MODE_SIZE (<MODE>mode);
  for (unsigned int i = 0; i < size; ++i)
    {
      rtx xops[2];
      xops[0] = adjust_address (operands[0], QImode, i);
      xops[1] = adjust_address (operands[1], QImode, i);
      output_asm_insn ("ld\ta,%1", xops);
      output_asm_insn ("cpl\ta", xops);
      output_asm_insn ("ld\t%0,a", xops);
    }
  return "";
})

(define_expand "neg<mode>2"
  [(set (match_operand:WIDE_INT 0 "nonimmediate_operand")
	(neg:WIDE_INT (match_operand:WIDE_INT 1 "general_operand")))]
  ""
{
  push_temp_slots ();
  rtx src = stm8_assign_stack_temp (<MODE>mode);
  rtx out = stm8_assign_stack_temp (<MODE>mode);
  emit_move_insn (src, operands[1]);
  emit_insn (gen_stm8_neg<mode>_mem (out, src));
  emit_move_insn (operands[0], out);
  pop_temp_slots ();
  DONE;
})

(define_insn "stm8_neg<mode>_mem"
  [(set (match_operand:WIDE_INT 0 "memory_operand" "=R")
	(neg:WIDE_INT (match_operand:WIDE_INT 1 "memory_operand" "R")))
   (clobber (reg:QI A_REG))]
  ""
{
  unsigned int size = GET_MODE_SIZE (<MODE>mode);
  for (unsigned int n = 0; n < size; ++n)
    {
      unsigned int i = size - 1 - n;
      rtx xops[2];
      xops[0] = adjust_address (operands[0], QImode, i);
      xops[1] = adjust_address (operands[1], QImode, i);
      output_asm_insn ("clr\ta", xops);
      output_asm_insn (n == 0 ? "sub\ta,%1" : "sbc\ta,%1", xops);
      output_asm_insn ("ld\t%0,a", xops);
    }
  return "";
})

;; -------------------------------------------------------------------------
;; 32/64-bit shifts
;; -------------------------------------------------------------------------

(define_code_iterator WIDE_SHIFT [ashift ashiftrt lshiftrt])
(define_code_attr wide_shift_name
  [(ashift "ashl") (ashiftrt "ashr") (lshiftrt "lshr")])

(define_expand "<wide_shift_name><mode>3"
  [(set (match_operand:WIDE_INT 0 "nonimmediate_operand")
	(WIDE_SHIFT:WIDE_INT
	  (match_operand:WIDE_INT 1 "general_operand")
	  (match_operand:QI 2 "general_operand")))]
  ""
{
  push_temp_slots ();
  rtx out = stm8_assign_stack_temp (<MODE>mode);
  emit_move_insn (out, operands[1]);

  if (CONST_INT_P (operands[2]))
    {
      emit_insn (gen_stm8_<wide_shift_name><mode>_const (out, operands[2]));
    }
  else
    {
      rtx a = gen_rtx_REG (QImode, A_REG);
      emit_move_insn (a, operands[2]);
      emit_insn (gen_stm8_<wide_shift_name><mode>_var (out));
    }

  emit_move_insn (operands[0], out);
  pop_temp_slots ();
  DONE;
})

(define_insn "stm8_<wide_shift_name><mode>_const"
  [(set (match_operand:WIDE_INT 0 "memory_operand" "+R")
	(WIDE_SHIFT:WIDE_INT
	  (match_dup 0)
	  (match_operand:QI 1 "const_int_operand" "n")))
   (clobber (reg:QI A_REG))]
  ""
{
  unsigned int size = GET_MODE_SIZE (<MODE>mode);
  unsigned int count = INTVAL (operands[1]) & 0xff;

  /* Move whole bytes first.  Besides reducing code size, this bounds
     the number of bit-shift chains independently of the mode width.  */
  unsigned int bytes = MIN (count / 8, size);
  if (bytes != 0)
    {
      rtx x[2];
      if (<CODE> == ASHIFT)
	{
	  for (unsigned int i = 0; i < size - bytes; ++i)
	    {
	      x[0] = adjust_address (operands[0], QImode, i);
	      x[1] = adjust_address (operands[0], QImode, i + bytes);
	      output_asm_insn ("ld\ta,%1\n\tld\t%0,a", x);
	    }
	  for (unsigned int i = size - bytes; i < size; ++i)
	    {
	      x[0] = adjust_address (operands[0], QImode, i);
	      output_asm_insn ("clr\t%0", x);
	    }
	}
      else
	{
	  for (unsigned int i = size; i-- > bytes;)
	    {
	      x[0] = adjust_address (operands[0], QImode, i);
	      x[1] = adjust_address (operands[0], QImode, i - bytes);
	      output_asm_insn ("ld\ta,%1\n\tld\t%0,a", x);
	    }
	  x[0] = adjust_address (operands[0], QImode, 0);
	  if (<CODE> == ASHIFTRT)
	    {
	      output_asm_insn ("ld\ta,%0", x);
	      for (unsigned int i = 0; i < 7; ++i)
		output_asm_insn ("sra\ta", x);
	    }
	  else
	    output_asm_insn ("clr\ta", x);
	  for (unsigned int i = 0; i < bytes; ++i)
	    {
	      x[0] = adjust_address (operands[0], QImode, i);
	      output_asm_insn ("ld\t%0,a", x);
	    }
	}
    }
  count %= 8;

  for (unsigned int c = 0; c < count; ++c)
    {
      if (<CODE> == ASHIFT)
	{
	  rtx x[1];
	  x[0] = adjust_address (operands[0], QImode, size - 1);
	  output_asm_insn ("sll\t%0", x);
	  for (unsigned int n = 1; n < size; ++n)
	    {
	      x[0] = adjust_address (operands[0], QImode, size - 1 - n);
	      output_asm_insn ("rlc\t%0", x);
	    }
	}
      else
	{
	  rtx x[1];
	  x[0] = adjust_address (operands[0], QImode, 0);
	  output_asm_insn (<CODE> == ASHIFTRT ? "sra\t%0" : "srl\t%0", x);
	  for (unsigned int i = 1; i < size; ++i)
	    {
	      x[0] = adjust_address (operands[0], QImode, i);
	      output_asm_insn ("rrc\t%0", x);
	    }
	}
    }
  return "";
})

(define_insn "stm8_<wide_shift_name><mode>_var"
  [(set (match_operand:WIDE_INT 0 "memory_operand" "+R")
	(WIDE_SHIFT:WIDE_INT (match_dup 0) (reg:QI A_REG)))
   (set (reg:QI A_REG) (const_int 0))]
  ""
{
  unsigned int size = GET_MODE_SIZE (<MODE>mode);
  output_asm_insn (
      "tnz\ta\n\tjreq\t.Lstm8_wshift_done%=\n.Lstm8_wshift_loop%=:", operands);

  if (<CODE> == ASHIFT)
    {
      rtx x[1];
      x[0] = adjust_address (operands[0], QImode, size - 1);
      output_asm_insn ("sll\t%0", x);
      for (unsigned int n = 1; n < size; ++n)
	{
	  x[0] = adjust_address (operands[0], QImode, size - 1 - n);
	  output_asm_insn ("rlc\t%0", x);
	}
    }
  else
    {
      rtx x[1];
      x[0] = adjust_address (operands[0], QImode, 0);
      output_asm_insn (<CODE> == ASHIFTRT ? "sra\t%0" : "srl\t%0", x);
      for (unsigned int i = 1; i < size; ++i)
	{
	  x[0] = adjust_address (operands[0], QImode, i);
	  output_asm_insn ("rrc\t%0", x);
	}
    }

  output_asm_insn (
      "dec\ta\n\tjrne\t.Lstm8_wshift_loop%=\n.Lstm8_wshift_done%=:", operands);
  return "";
})

;; -------------------------------------------------------------------------
;; 32/64-bit comparisons
;; -------------------------------------------------------------------------

;; Wide comparisons are done directly on byte snapshots.  Signed relations
;; use a signed comparison for the most-significant byte and unsigned
;; lexicographic comparison for the remaining bytes.

(define_expand "cbranch<mode>4"
  [(set (pc)
	(if_then_else
	  (match_operator 0 "stm8_comparison_operator"
	    [(match_operand:WIDE_INT 1 "general_operand")
	     (match_operand:WIDE_INT 2 "general_operand")])
	  (label_ref (match_operand 3 "" ""))
	  (pc)))]
  ""
{
  push_temp_slots ();
  rtx lhs = stm8_assign_stack_temp (<MODE>mode);
  rtx rhs = stm8_assign_stack_temp (<MODE>mode);
  emit_move_insn (lhs, operands[1]);
  emit_move_insn (rhs, operands[2]);
  emit_jump_insn (
      gen_stm8_cbranch<mode>_mem (operands[0], lhs, rhs, operands[3]));
  pop_temp_slots ();
  DONE;
})

(define_insn "stm8_cbranch<mode>_mem"
  [(set (pc)
	(if_then_else
	  (match_operator 0 "stm8_comparison_operator"
	    [(match_operand:WIDE_INT 1 "memory_operand" "R")
	     (match_operand:WIDE_INT 2 "memory_operand" "R")])
	  (label_ref (match_operand 3 "" ""))
	  (pc)))
   (clobber (reg:QI A_REG))]
  ""
{
  enum rtx_code code = GET_CODE (operands[0]);
  unsigned int size = GET_MODE_SIZE (<MODE>mode);

  if (code == EQ || code == NE)
    {
      for (unsigned int i = 0; i < size; ++i)
	{
	  rtx xops[4];
	  xops[0] = NULL_RTX;
	  xops[1] = adjust_address (operands[1], QImode, i);
	  xops[2] = adjust_address (operands[2], QImode, i);
	  xops[3] = operands[3];
	  output_asm_insn ("ld\ta,%1\n\tcp\ta,%2", xops);
	  output_asm_insn (code == EQ ? "jrne\t.Lstm8_wcmp_done%="
				      : "jrne\t.Lstm8_wcmp_true%=",
			   xops);
	}

      if (code == EQ)
	output_asm_insn ("%J3\t%l3\n.Lstm8_wcmp_done%=:", operands);
      else
	output_asm_insn ("%J3\t.Lstm8_wcmp_done%=\n.Lstm8_wcmp_true%=:\n\t%"
			 "J3\t%l3\n.Lstm8_wcmp_done%=:",
			 operands);
      return "";
    }

  bool signed_p = (code == LT || code == LE || code == GT || code == GE);

  if (signed_p)
    {
      rtx xops[4];
      xops[0] = NULL_RTX;
      xops[1] = adjust_address (operands[1], QImode, 0);
      xops[2] = adjust_address (operands[2], QImode, 0);
      xops[3] = operands[3];
      output_asm_insn ("ld\ta,%1\n\tcp\ta,%2\n\tjreq\t.Lstm8_wcmp_lower%=",
		       xops);

      switch (code)
	{
	case LT:
	  output_asm_insn ("jrsge\t.Lstm8_wcmp_done%=", xops);
	  break;
	case LE:
	  output_asm_insn ("jrsgt\t.Lstm8_wcmp_done%=", xops);
	  break;
	case GT:
	  output_asm_insn ("jrsle\t.Lstm8_wcmp_done%=", xops);
	  break;
	case GE:
	  output_asm_insn ("jrslt\t.Lstm8_wcmp_done%=", xops);
	  break;
	default:
	  gcc_unreachable ();
	}
      output_asm_insn ("%J3\t%l3\n.Lstm8_wcmp_lower%=:", xops);

      for (unsigned int i = 1; i < size; ++i)
	{
	  xops[1] = adjust_address (operands[1], QImode, i);
	  xops[2] = adjust_address (operands[2], QImode, i);
	  output_asm_insn ("ld\ta,%1\n\tcp\ta,%2", xops);
	  if (i + 1 != size)
	    output_asm_insn ("jrne\t.Lstm8_wcmp_lower_decide%=", xops);
	}
      output_asm_insn (".Lstm8_wcmp_lower_decide%=:", xops);

      switch (code)
	{
	case LT:
	  output_asm_insn ("jruge\t.Lstm8_wcmp_done%=", xops);
	  break;
	case LE:
	  output_asm_insn ("jrugt\t.Lstm8_wcmp_done%=", xops);
	  break;
	case GT:
	  output_asm_insn ("jrule\t.Lstm8_wcmp_done%=", xops);
	  break;
	case GE:
	  output_asm_insn ("jrult\t.Lstm8_wcmp_done%=", xops);
	  break;
	default:
	  gcc_unreachable ();
	}
      output_asm_insn ("%J3\t%l3\n.Lstm8_wcmp_done%=:", xops);
      return "";
    }

  for (unsigned int i = 0; i < size; ++i)
    {
      rtx xops[4];
      xops[0] = NULL_RTX;
      xops[1] = adjust_address (operands[1], QImode, i);
      xops[2] = adjust_address (operands[2], QImode, i);
      xops[3] = operands[3];
      output_asm_insn ("ld\ta,%1\n\tcp\ta,%2", xops);
      if (i + 1 != size)
	output_asm_insn ("jrne\t.Lstm8_wcmp_decide%=", xops);
    }
  output_asm_insn (".Lstm8_wcmp_decide%=:", operands);

  switch (code)
    {
    case LTU:
      output_asm_insn ("jruge\t.Lstm8_wcmp_done%=", operands);
      break;
    case LEU:
      output_asm_insn ("jrugt\t.Lstm8_wcmp_done%=", operands);
      break;
    case GTU:
      output_asm_insn ("jrule\t.Lstm8_wcmp_done%=", operands);
      break;
    case GEU:
      output_asm_insn ("jrult\t.Lstm8_wcmp_done%=", operands);
      break;
    default:
      gcc_unreachable ();
    }
  output_asm_insn ("%J3\t%l3\n.Lstm8_wcmp_done%=:", operands);
  return "";
})

;; -------------------------------------------------------------------------
;; DImode multiply/divide/modulo
;; -------------------------------------------------------------------------

;; The current libcall argument setup does not describe the hidden pointer
;; for an eight-byte result.  These expanders therefore use explicit
;; destination/input pointers to STM8-specific void helpers, preserving
;; the established C ABI.  The integer runtime supplies their definitions
;; using byte/word arithmetic.

(define_expand "muldi3"
  [(set (match_operand:DI 0 "nonimmediate_operand")
	(mult:DI (match_operand:DI 1 "general_operand")
		 (match_operand:DI 2 "general_operand")))]
  ""
{
  push_temp_slots ();
  rtx lhs = stm8_assign_stack_temp (DImode);
  rtx rhs = stm8_assign_stack_temp (DImode);
  rtx out = stm8_assign_stack_temp (DImode);
  emit_move_insn (lhs, operands[1]);
  emit_move_insn (rhs, operands[2]);
  rtx fun = gen_rtx_SYMBOL_REF (Pmode, "__stm8_muldi3");
  emit_library_call (fun, LCT_NORMAL, VOIDmode, XEXP (out, 0), Pmode,
		     XEXP (lhs, 0), Pmode, XEXP (rhs, 0), Pmode);
  emit_move_insn (operands[0], out);
  pop_temp_slots ();
  DONE;
})

(define_expand "udivmoddi4"
  [(parallel
     [(set (match_operand:DI 0 "nonimmediate_operand")
	   (udiv:DI (match_operand:DI 1 "general_operand")
		    (match_operand:DI 2 "general_operand")))
      (set (match_operand:DI 3 "nonimmediate_operand")
	   (umod:DI (match_dup 1) (match_dup 2)))])]
  ""
{
  push_temp_slots ();
  rtx lhs = stm8_assign_stack_temp (DImode);
  rtx rhs = stm8_assign_stack_temp (DImode);
  rtx quo = stm8_assign_stack_temp (DImode);
  rtx rem = stm8_assign_stack_temp (DImode);
  emit_move_insn (lhs, operands[1]);
  emit_move_insn (rhs, operands[2]);
  rtx fun = gen_rtx_SYMBOL_REF (Pmode, "__stm8_udivmoddi4");
  emit_library_call (fun, LCT_NORMAL, VOIDmode, XEXP (quo, 0), Pmode,
		     XEXP (rem, 0), Pmode, XEXP (lhs, 0), Pmode, XEXP (rhs, 0),
		     Pmode);
  emit_move_insn (operands[0], quo);
  emit_move_insn (operands[3], rem);
  pop_temp_slots ();
  DONE;
})

(define_expand "divmoddi4"
  [(parallel
     [(set (match_operand:DI 0 "nonimmediate_operand")
	   (div:DI (match_operand:DI 1 "general_operand")
		   (match_operand:DI 2 "general_operand")))
      (set (match_operand:DI 3 "nonimmediate_operand")
	   (mod:DI (match_dup 1) (match_dup 2)))])]
  ""
{
  push_temp_slots ();
  rtx lhs = stm8_assign_stack_temp (DImode);
  rtx rhs = stm8_assign_stack_temp (DImode);
  rtx quo = stm8_assign_stack_temp (DImode);
  rtx rem = stm8_assign_stack_temp (DImode);
  emit_move_insn (lhs, operands[1]);
  emit_move_insn (rhs, operands[2]);
  rtx fun = gen_rtx_SYMBOL_REF (Pmode, "__stm8_divmoddi4");
  emit_library_call (fun, LCT_NORMAL, VOIDmode, XEXP (quo, 0), Pmode,
		     XEXP (rem, 0), Pmode, XEXP (lhs, 0), Pmode, XEXP (rhs, 0),
		     Pmode);
  emit_move_insn (operands[0], quo);
  emit_move_insn (operands[3], rem);
  pop_temp_slots ();
  DONE;
})

(define_expand "udivdi3"
  [(set (match_operand:DI 0 "nonimmediate_operand")
	(udiv:DI (match_operand:DI 1 "general_operand")
		 (match_operand:DI 2 "general_operand")))]
  ""
{
  push_temp_slots ();
  rtx lhs = stm8_assign_stack_temp (DImode);
  rtx rhs = stm8_assign_stack_temp (DImode);
  rtx quo = stm8_assign_stack_temp (DImode);
  rtx rem = stm8_assign_stack_temp (DImode);
  emit_move_insn (lhs, operands[1]);
  emit_move_insn (rhs, operands[2]);
  rtx fun = gen_rtx_SYMBOL_REF (Pmode, "__stm8_udivmoddi4");
  emit_library_call (fun, LCT_NORMAL, VOIDmode, XEXP (quo, 0), Pmode,
		     XEXP (rem, 0), Pmode, XEXP (lhs, 0), Pmode, XEXP (rhs, 0),
		     Pmode);
  emit_move_insn (operands[0], quo);
  pop_temp_slots ();
  DONE;
})

(define_expand "umoddi3"
  [(set (match_operand:DI 0 "nonimmediate_operand")
	(umod:DI (match_operand:DI 1 "general_operand")
		 (match_operand:DI 2 "general_operand")))]
  ""
{
  push_temp_slots ();
  rtx lhs = stm8_assign_stack_temp (DImode);
  rtx rhs = stm8_assign_stack_temp (DImode);
  rtx quo = stm8_assign_stack_temp (DImode);
  rtx rem = stm8_assign_stack_temp (DImode);
  emit_move_insn (lhs, operands[1]);
  emit_move_insn (rhs, operands[2]);
  rtx fun = gen_rtx_SYMBOL_REF (Pmode, "__stm8_udivmoddi4");
  emit_library_call (fun, LCT_NORMAL, VOIDmode, XEXP (quo, 0), Pmode,
		     XEXP (rem, 0), Pmode, XEXP (lhs, 0), Pmode, XEXP (rhs, 0),
		     Pmode);
  emit_move_insn (operands[0], rem);
  pop_temp_slots ();
  DONE;
})

(define_expand "divdi3"
  [(set (match_operand:DI 0 "nonimmediate_operand")
	(div:DI (match_operand:DI 1 "general_operand")
		(match_operand:DI 2 "general_operand")))]
  ""
{
  push_temp_slots ();
  rtx lhs = stm8_assign_stack_temp (DImode);
  rtx rhs = stm8_assign_stack_temp (DImode);
  rtx quo = stm8_assign_stack_temp (DImode);
  rtx rem = stm8_assign_stack_temp (DImode);
  emit_move_insn (lhs, operands[1]);
  emit_move_insn (rhs, operands[2]);
  rtx fun = gen_rtx_SYMBOL_REF (Pmode, "__stm8_divmoddi4");
  emit_library_call (fun, LCT_NORMAL, VOIDmode, XEXP (quo, 0), Pmode,
		     XEXP (rem, 0), Pmode, XEXP (lhs, 0), Pmode, XEXP (rhs, 0),
		     Pmode);
  emit_move_insn (operands[0], quo);
  pop_temp_slots ();
  DONE;
})

(define_expand "moddi3"
  [(set (match_operand:DI 0 "nonimmediate_operand")
	(mod:DI (match_operand:DI 1 "general_operand")
		(match_operand:DI 2 "general_operand")))]
  ""
{
  push_temp_slots ();
  rtx lhs = stm8_assign_stack_temp (DImode);
  rtx rhs = stm8_assign_stack_temp (DImode);
  rtx quo = stm8_assign_stack_temp (DImode);
  rtx rem = stm8_assign_stack_temp (DImode);
  emit_move_insn (lhs, operands[1]);
  emit_move_insn (rhs, operands[2]);
  rtx fun = gen_rtx_SYMBOL_REF (Pmode, "__stm8_divmoddi4");
  emit_library_call (fun, LCT_NORMAL, VOIDmode, XEXP (quo, 0), Pmode,
		     XEXP (rem, 0), Pmode, XEXP (lhs, 0), Pmode, XEXP (rhs, 0),
		     Pmode);
  emit_move_insn (operands[0], rem);
  pop_temp_slots ();
  DONE;
})
