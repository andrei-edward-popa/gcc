/* Copyright (c) 2026 Andrei-Edward Popa.  */

/* { dg-do run } */
/* { dg-options "-O2 -fgimple -fno-inline" } */

/* The GIMPLE front end retains byte operations without C promotions.  */
#define BYTE_SHIFT(NAME, TYPE, OP, COUNT) \
  TYPE __GIMPLE () NAME##COUNT (TYPE value) \
  { \
    TYPE result; \
    result = value OP COUNT; \
    return result; \
  }

#define WORD_SHIFT(NAME, TYPE, OP, COUNT) \
  __attribute__ ((noinline, noclone)) \
  TYPE NAME##COUNT (TYPE value, unsigned char tag) \
  { \
    TYPE result = value OP COUNT; \
    return result ^ tag; \
  }

BYTE_SHIFT (byte_left, unsigned char, <<, 0)
BYTE_SHIFT (byte_left, unsigned char, <<, 1)
BYTE_SHIFT (byte_left, unsigned char, <<, 2)
BYTE_SHIFT (byte_left, unsigned char, <<, 3)
BYTE_SHIFT (byte_left, unsigned char, <<, 4)
BYTE_SHIFT (byte_left, unsigned char, <<, 5)
BYTE_SHIFT (byte_left, unsigned char, <<, 6)
BYTE_SHIFT (byte_left, unsigned char, <<, 7)

BYTE_SHIFT (byte_right, unsigned char, >>, 0)
BYTE_SHIFT (byte_right, unsigned char, >>, 1)
BYTE_SHIFT (byte_right, unsigned char, >>, 2)
BYTE_SHIFT (byte_right, unsigned char, >>, 3)
BYTE_SHIFT (byte_right, unsigned char, >>, 4)
BYTE_SHIFT (byte_right, unsigned char, >>, 5)
BYTE_SHIFT (byte_right, unsigned char, >>, 6)
BYTE_SHIFT (byte_right, unsigned char, >>, 7)

BYTE_SHIFT (byte_signed, signed char, >>, 0)
BYTE_SHIFT (byte_signed, signed char, >>, 1)
BYTE_SHIFT (byte_signed, signed char, >>, 2)
BYTE_SHIFT (byte_signed, signed char, >>, 3)
BYTE_SHIFT (byte_signed, signed char, >>, 4)
BYTE_SHIFT (byte_signed, signed char, >>, 5)
BYTE_SHIFT (byte_signed, signed char, >>, 6)
BYTE_SHIFT (byte_signed, signed char, >>, 7)

WORD_SHIFT (word_left, unsigned int, <<, 0)
WORD_SHIFT (word_left, unsigned int, <<, 1)
WORD_SHIFT (word_left, unsigned int, <<, 2)
WORD_SHIFT (word_left, unsigned int, <<, 3)
WORD_SHIFT (word_left, unsigned int, <<, 4)
WORD_SHIFT (word_left, unsigned int, <<, 5)
WORD_SHIFT (word_left, unsigned int, <<, 6)
WORD_SHIFT (word_left, unsigned int, <<, 7)
WORD_SHIFT (word_left, unsigned int, <<, 8)
WORD_SHIFT (word_left, unsigned int, <<, 9)
WORD_SHIFT (word_left, unsigned int, <<, 10)
WORD_SHIFT (word_left, unsigned int, <<, 11)
WORD_SHIFT (word_left, unsigned int, <<, 12)
WORD_SHIFT (word_left, unsigned int, <<, 13)
WORD_SHIFT (word_left, unsigned int, <<, 14)
WORD_SHIFT (word_left, unsigned int, <<, 15)

WORD_SHIFT (word_right, unsigned int, >>, 0)
WORD_SHIFT (word_right, unsigned int, >>, 1)
WORD_SHIFT (word_right, unsigned int, >>, 2)
WORD_SHIFT (word_right, unsigned int, >>, 3)
WORD_SHIFT (word_right, unsigned int, >>, 4)
WORD_SHIFT (word_right, unsigned int, >>, 5)
WORD_SHIFT (word_right, unsigned int, >>, 6)
WORD_SHIFT (word_right, unsigned int, >>, 7)
WORD_SHIFT (word_right, unsigned int, >>, 8)
WORD_SHIFT (word_right, unsigned int, >>, 9)
WORD_SHIFT (word_right, unsigned int, >>, 10)
WORD_SHIFT (word_right, unsigned int, >>, 11)
WORD_SHIFT (word_right, unsigned int, >>, 12)
WORD_SHIFT (word_right, unsigned int, >>, 13)
WORD_SHIFT (word_right, unsigned int, >>, 14)
WORD_SHIFT (word_right, unsigned int, >>, 15)

static unsigned char (*const byte_left_functions[]) (unsigned char) =
{
  byte_left0, byte_left1, byte_left2, byte_left3,
  byte_left4, byte_left5, byte_left6, byte_left7
};

static unsigned char (*const byte_right_functions[]) (unsigned char) =
{
  byte_right0, byte_right1, byte_right2, byte_right3,
  byte_right4, byte_right5, byte_right6, byte_right7
};

static signed char (*const byte_signed_functions[]) (signed char) =
{
  byte_signed0, byte_signed1, byte_signed2, byte_signed3,
  byte_signed4, byte_signed5, byte_signed6, byte_signed7
};

static unsigned int (*const word_left_functions[]) (unsigned int, unsigned char) =
{
  word_left0, word_left1, word_left2, word_left3,
  word_left4, word_left5, word_left6, word_left7,
  word_left8, word_left9, word_left10, word_left11,
  word_left12, word_left13, word_left14, word_left15
};

static unsigned int (*const word_right_functions[]) (unsigned int, unsigned char) =
{
  word_right0, word_right1, word_right2, word_right3,
  word_right4, word_right5, word_right6, word_right7,
  word_right8, word_right9, word_right10, word_right11,
  word_right12, word_right13, word_right14, word_right15
};

int
main (void)
{
  for (unsigned int value = 0; value < 256; ++value)
    {
      unsigned int word = value * 257U;
      unsigned char tag = value ^ 0x5aU;
      for (unsigned char count = 0; count < 16; ++count)
	{
	  unsigned int divisor = 1U << count;
	  if (word_left_functions[count] (word, tag)
	      != ((unsigned int) (word * divisor) ^ tag)
	      || word_right_functions[count] (word, tag)
		 != ((word / divisor) ^ tag))
	    __builtin_abort ();
	  if (count < 8)
	    {
	      int signed_value = (signed char) value;
	      int expected = signed_value / (int) divisor;
	      if (signed_value < 0 && signed_value % (int) divisor != 0)
		--expected;
	      if (byte_left_functions[count] (value)
		  != (unsigned char) (value * divisor)
		  || byte_right_functions[count] (value) != value / divisor
		  || byte_signed_functions[count] (signed_value) != expected)
		__builtin_abort ();
	    }
	}
    }
  return 0;
}
