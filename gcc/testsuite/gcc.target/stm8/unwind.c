/* Copyright (c) 2026 Andrei-Edward Popa.  */
/* { dg-do compile } */
/* { dg-options "-O2 -g -mmodel=medium" } */

unsigned char
byte_return (unsigned char a, unsigned b, unsigned char c)
{
  return a + b + c;
}

unsigned
word_return (unsigned a, unsigned b)
{
  return a + b;
}

float
wide_return (float a, unsigned n)
{
  volatile unsigned char frame[n];
  frame[0] = n;
  return a;
}

/* { dg-final { scan-assembler "\\.cfi_return_column 11" } } */
/* { dg-final { scan-assembler "\\.cfi_escape 0x16,11,2,0x79,0" } } */
/* { dg-final { scan-assembler "\\.cfi_escape 0x16,11,2,0x7a,0" } } */
/* { dg-final { scan-assembler "\\.cfi_offset 8," } } */
/* { dg-final { scan-assembler "\\.cfi_escape 0x16,11,5,0x11,127,0x22,0x94,2" } } */
