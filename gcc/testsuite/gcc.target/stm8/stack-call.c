/* Copyright (c) 2026 Andrei-Edward Popa.  */
/* { dg-do run } */
/* { dg-options "-O2" } */

extern void abort (void);

__attribute__((noinline)) static int
stack_call (void)
{
  /* LDW X,#0x1234 followed by RET or RETF.  */
  volatile unsigned char code[] = { 0xae, 0x12, 0x34,
#ifdef __STM8_LARGE__
			  0x87
#else
			  0x81
#endif
  };
  return ((int (*) (void)) (void *) code) ();
}

int
main (void)
{
  if (stack_call () != 0x1234)
    abort ();
  return 0;
}
