/* Copyright (c) 2026 Andrei-Edward Popa.  */
/* { dg-do compile } */
/* { dg-options "-O2" } */

unsigned short
increment (unsigned short x)
{
  return x + 1;
}

unsigned short
clear_word (void)
{
  return 0;
}

unsigned short
high_bit (unsigned short x)
{
  return x >> 15;
}

/* { dg-final { scan-assembler {incw[ \t]+x} } } */
/* { dg-final { scan-assembler {clrw[ \t]+x} } } */
/* { dg-final { scan-assembler-not {srlw[ \t]+x} } } */
