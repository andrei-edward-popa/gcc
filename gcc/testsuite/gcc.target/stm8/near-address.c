/* Copyright (c) 2026 Andrei-Edward Popa.  */
/* { dg-do run } */
/* { dg-options "-O1 -fno-builtin" } */
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Reassociation in the reference loops may create a negative base and
   a positive indexed offset.  Near pointers must still wrap to 16 bits.  */
static unsigned char source[1100], buffer[1100], reference[1100];
volatile unsigned short result;
volatile unsigned long checks;

#define CHECK(e)                                                               \
  do                                                                           \
    {                                                                          \
      if (!(e))                                                                \
	{                                                                      \
	  result = __LINE__;                                                   \
	  return 1;                                                            \
	}                                                                      \
      checks++;                                                                \
    }                                                                          \
  while (0)

static unsigned char
pattern (unsigned i)
{
  return (unsigned char) (i * 37u + 11u);
}

int
main (void)
{
  static const unsigned lengths[]
      = { 0,   1,   2,	 3,   7,   127, 128,  255,  256, 257,
	  511, 512, 513, 767, 768, 769, 1023, 1024, 1025 };
  static const int deltas[] = { -31, -1, 0, 1, 31 };
  static const int values[] = { -1, 0, 1, 255, 0x5500, 0x55a5 };
  unsigned i, k, offset, j;

  CHECK (memcpy (buffer, source, 0) == buffer);
  CHECK (memmove (buffer, source, 0) == buffer);
  CHECK (memset (buffer, 123, 0) == buffer);
  CHECK (memcmp (buffer, source, 0) == 0);
  for (i = 0; i < sizeof source; i++)
    source[i] = pattern (i);
  for (k = 0; k < sizeof lengths / sizeof lengths[0]; k++)
    {
      unsigned n = lengths[k];
      for (offset = 0; offset < 4; offset++)
	{
	  for (i = 0; i < sizeof buffer; i++)
	    buffer[i] = reference[i] = 0x5a;
	  for (i = 0; i < n; i++)
	    reference[offset + i] = source[3 - offset + i];
	  CHECK (memcpy (buffer + offset, source + 3 - offset, n)
		 == buffer + offset);
	  for (i = 0; i < sizeof buffer; i++)
	    CHECK (buffer[i] == reference[i]);
	  CHECK (memcmp (buffer + offset, source + 3 - offset, n) == 0);
	  if (n)
	    {
	      unsigned char old = buffer[offset + n - 1];
	      buffer[offset + n - 1] = old ^ 0x80;
	      int comparison = memcmp (buffer + offset, source + 3 - offset, n);
	      CHECK ((comparison < 0) == (buffer[offset + n - 1] < old));
	      CHECK ((comparison > 0) == (buffer[offset + n - 1] > old));
	    }
	  for (j = 0; j < sizeof values / sizeof values[0]; j++)
	    {
	      for (i = 0; i < sizeof buffer; i++)
		buffer[i] = reference[i] = 0x35;
	      for (i = 0; i < n; i++)
		reference[offset + i] = values[j];
	      CHECK (memset (buffer + offset, values[j], n) == buffer + offset);
	      for (i = 0; i < sizeof buffer; i++)
		CHECK (buffer[i] == reference[i]);
	    }
	}
      for (j = 0; j < sizeof deltas / sizeof deltas[0]; j++)
	{
	  unsigned dest = 33 + deltas[j];
	  for (i = 0; i < sizeof buffer; i++)
	    buffer[i] = reference[i] = pattern (i);
	  for (i = 0; i < n; i++)
	    reference[dest + i] = pattern (33 + i);
	  CHECK (memmove (buffer + dest, buffer + 33, n) == buffer + dest);
	  for (i = 0; i < sizeof buffer; i++)
	    CHECK (buffer[i] == reference[i]);
	}
      for (i = 0; i < n; i++)
	buffer[i] = source[i] = pattern (i) | 1;
      buffer[n] = source[n] = 0;
      CHECK (strlen (source) == n);
      CHECK (strcmp (source, buffer) == 0);
      if (n)
	{
	  unsigned char old = buffer[n - 1];
	  buffer[n - 1] = old ^ 0x80;
	  CHECK ((strcmp (source, buffer) < 0) == (old < buffer[n - 1]));
	  CHECK ((strcmp (source, buffer) > 0) == (old > buffer[n - 1]));
	}
      for (i = 0; i < sizeof source; i++)
	source[i] = pattern (i);
    }
  result = 0xa5;
  return 0;
}
