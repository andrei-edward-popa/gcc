;; Register constraints for the STM8.
;; Copyright (C) 2026 Free Software Foundation, Inc.
;;
;; This file is part of GCC.
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

(define_register_constraint "a" "A_REGS"
  "The accumulator A.")

(define_register_constraint "x" "X_REGS"
  "The X register or one of its byte halves.")

(define_register_constraint "y" "Y_REGS"
  "The Y register or one of its byte halves.")

(define_register_constraint "v" "POINTER_REGS"
  "The X or Y register, or one of their byte halves.")

(define_register_constraint "q" "SP_REGS"
  "The stack pointer.")

(define_register_constraint "f" "FP_REGS"
  "The memory-backed frame pointer.")

(define_memory_constraint "R"
  "Memory directly addressable by STM8 instructions."
  (and (match_code "mem")
       (match_test "!stm8_far_sp_memory_operand_p (op)")))
