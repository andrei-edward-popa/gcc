/* Copyright (c) 2026 Andrei-Edward Popa.  */

/* { dg-do run } */
/* { dg-options "-O2" } */

union value
{
  unsigned long word;
  float real;
  unsigned char bytes[4];
};

volatile union value input, output;

__attribute__ ((noinline, noclone)) unsigned long
load_word (volatile union value *p)
{
  return p->word;
}

__attribute__ ((noinline, noclone)) float
load_real (volatile union value *p)
{
  return p->real;
}

__attribute__ ((noinline, noclone)) void
copy_word (volatile union value *dst, volatile union value *src)
{
  dst->word = src->word;
}

__attribute__ ((noinline, noclone)) void
zero_word (volatile union value *dst)
{
  dst->word = 0;
}

__attribute__ ((noinline, noclone)) void
negative_zero (volatile union value *dst)
{
  dst->real = -0.0f;
}

int
main (void)
{
  for (unsigned int n = 0; n < 256; ++n)
    {
      unsigned char expected[4];
      for (unsigned int i = 0; i < 4; ++i)
	input.bytes[i] = expected[i] = (unsigned char) (n + 37 * i);
      copy_word (&output, &input);
      for (unsigned int i = 0; i < 4; ++i)
	if (output.bytes[i] != expected[i])
	  __builtin_abort ();
      output.word = load_word (&input);
      for (unsigned int i = 0; i < 4; ++i)
	if (output.bytes[i] != expected[i])
	  __builtin_abort ();
      /* Keep the floating inputs finite, including subnormal values.  */
      input.bytes[0] &= 0x7e;
      expected[0] &= 0x7e;
      output.real = load_real (&input);
      for (unsigned int i = 0; i < 4; ++i)
	if (output.bytes[i] != expected[i])
	  __builtin_abort ();
      zero_word (&output);
      for (unsigned int i = 0; i < 4; ++i)
	if (output.bytes[i] != 0)
	  __builtin_abort ();
      negative_zero (&output);
      for (unsigned int i = 0; i < 4; ++i)
	if (output.bytes[i] != (i == 0 ? 0x80 : 0))
	  __builtin_abort ();
    }
  return 0;
}
