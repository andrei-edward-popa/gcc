/* Copyright (c) 2026 Andrei-Edward Popa.  */

/* { dg-do run } */
/* { dg-options "-O2 -fgimple" } */

/* Integer promotions normally widen C shifts to HI.  Use the GIMPLE
   front end to exercise the byte RTL patterns directly.  */
unsigned char __GIMPLE ()
left (unsigned char value, unsigned char count)
{
  unsigned char result;
  result = value << count;
  return result;
}

unsigned char __GIMPLE ()
right (unsigned char value, unsigned char count)
{
  unsigned char result;
  result = value >> count;
  return result;
}

signed char __GIMPLE ()
signed_right (signed char value, unsigned char count)
{
  signed char result;
  result = value >> count;
  return result;
}

int
main (void)
{
  for (unsigned int value = 0; value < 256; ++value)
    for (unsigned char count = 0; count < 8; ++count)
      {
	unsigned int divisor = 1U << count;
	int signed_value = (signed char) value;
	int expected = signed_value / (int) divisor;
	if (signed_value < 0 && signed_value % (int) divisor != 0)
	  --expected;
	if (left (value, count) != (unsigned char) (value * divisor)
	    || right (value, count) != value / divisor
	    || signed_right (signed_value, count) != expected)
	  __builtin_abort ();
      }
  return 0;
}
