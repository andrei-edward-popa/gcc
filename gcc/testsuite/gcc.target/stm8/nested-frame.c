/* Copyright (c) 2026 Andrei-Edward Popa.  */
/* { dg-do run } */
/* { dg-options "-O1" } */

extern void abort (void);
static volatile unsigned long checksum;

__attribute__((noinline, noipa)) static void
put_char (unsigned char c)
{
  checksum = checksum * 33 + c;
}

__attribute__((noinline, noipa)) static void
set_pos (void)
{
  checksum += 7;
}

__attribute__((noinline, noipa)) static unsigned
process (unsigned length)
{
  unsigned char buffer[length + 1];
  unsigned position = 0;
  unsigned long offset = 1;

  __attribute__((noinline)) void print (int pos, int c)
  {
    unsigned char *p;
    for (p = buffer + pos; *p; p++)
      {
	position++;
	put_char (*p + c);
      }
  }

  __attribute__((noinline)) void remove_chars (unsigned n)
  {
    set_pos ();
    print (offset, n);
    set_pos ();
    print (offset, n + 1);
    set_pos ();
  }

  for (unsigned i = 0; i < length; i++)
    buffer[i] = i % 127 + 1;
  buffer[length] = 0;
  remove_chars (3);
  return position;
}

int
main (void)
{
  static const unsigned lengths[] = { 2, 13, 127, 255, 256, 300, 512 };
  unsigned long expected = 0;
  for (unsigned k = 0; k < sizeof lengths / sizeof lengths[0]; k++)
    {
      unsigned n = lengths[k];
      expected += 7;
      for (unsigned i = 1; i < n; i++)
	expected = expected * 33 + (i % 127 + 1) + 3;
      expected += 7;
      for (unsigned i = 1; i < n; i++)
	expected = expected * 33 + (i % 127 + 1) + 4;
      expected += 7;
      if (process (n) != 2 * (n - 1) || checksum != expected)
	abort ();
    }
  return 0;
}
