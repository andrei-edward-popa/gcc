/* Copyright (c) 2026 Andrei-Edward Popa.  */
/* { dg-do run } */
/* { dg-options "-O1" } */

extern void abort (void);
static volatile unsigned char count;

__attribute__((noinline, noipa)) static unsigned short
rotate (unsigned short x, unsigned char n)
{
  return (x << n) | (x >> ((-n) & 15));
}

int
main (void)
{
  static const unsigned short values[]
    = { 0, 1, 255, 256, 257, 32767, 32768, 65535 };
  for (unsigned i = 0; i < sizeof values / sizeof values[0]; i++)
    for (count = 0; count < 16; count++)
      {
	unsigned long x = values[i];
	unsigned n = count;
	unsigned short expected = (x << n) | (x >> (16 - n));
	if (rotate (x, n) != expected)
	  abort ();
      }
  return 0;
}
