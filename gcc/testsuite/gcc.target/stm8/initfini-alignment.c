/* Copyright (c) 2026 Andrei-Edward Popa.  */

/* { dg-do compile } */
/* { dg-options "-O2" } */
/* { dg-final { scan-assembler-not "\\.align" } } */

volatile unsigned char order;

static void __attribute__ ((constructor (101)))
early (void)
{
  order = 1;
}

static void __attribute__ ((constructor))
normal (void)
{
  order = 2;
}

static void __attribute__ ((destructor))
finish (void)
{
  order = 3;
}
