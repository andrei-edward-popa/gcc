/* Copyright (c) 2026 Andrei-Edward Popa.  */
/* { dg-do compile } */
/* { dg-options "-mmodel=large -g -Os" } */

extern void consume (void (*fn) (void));

static void
callback (void)
{
}

static inline void
forward (void (*fn) (void))
{
  consume (fn);
}

void
caller (void)
{
  forward (callback);
}
