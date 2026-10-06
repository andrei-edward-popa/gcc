/* Copyright (c) 2026 Andrei-Edward Popa.  */
/* { dg-do compile } */
/* { dg-options "-mmodel=large" } */

extern void callback (void);

void
emit_address (void)
{
  __asm__ volatile (".3byte %c0" : : "i" (callback));
}

/* { dg-final { scan-assembler "\\.3byte[ \t]+_callback" } } */
