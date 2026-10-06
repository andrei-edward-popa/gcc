/* Copyright (c) 2026 Andrei-Edward Popa.  */
/* { dg-do compile } */
/* { dg-options "-O2 -g -mmodel=large" } */

#include "unwind.c"

/* { dg-final { scan-assembler "\\.cfi_return_column 11" } } */
/* { dg-final { scan-assembler "\\.cfi_offset 8," } } */
/* { dg-final { scan-assembler "\\.cfi_escape 0x16,11,5,0x11,126,0x22,0x94,3" } } */
