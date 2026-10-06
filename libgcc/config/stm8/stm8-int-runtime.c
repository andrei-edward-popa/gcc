/* Software integer helpers for the STM8 GCC port.
   Copyright (C) 2026 Free Software Foundation, Inc.

   This file is part of GCC.

   GCC is free software; you can redistribute it and/or modify it under
   the terms of the GNU General Public License as published by the Free
   Software Foundation; either version 3, or (at your option) any later
   version.

   GCC is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
   for more details.

   Under Section 7 of GPL version 3, you are granted additional
   permissions described in the GCC Runtime Library Exception, version
   3.1, as published by the Free Software Foundation.

   You should have received a copy of the GNU General Public License and
   a copy of the GCC Runtime Library Exception along with this program;
   see the files COPYING3 and COPYING.RUNTIME respectively.  If not, see
   <http://www.gnu.org/licenses/>.

   This file intentionally implements only operations that are not profitable
   or possible as direct STM8 instructions.  The QI/HI/SI helpers use the
   normal GCC libcall ABI because these modes have established return
   locations.  DImode is different: STM8 has no eight-byte hard-register
   return location, so the compiler-side DImode expanders call the
   __stm8_* pointer-result helpers at the end of this file.

   Scalar helpers avoid multiplication, division and remainder internally.
   The DI product uses byte products accumulated in 16 bits; these use
   native MUL or the separate HI helper, never the DI helper itself.  */

#include "stm8-runtime.h"

/* Unsigned restoring division in the operand's own width.  Keeping the
   carry bit from the left shift separately avoids requiring the next wider
   integer mode.  If the pre-shift MSB is set, the mathematical shifted
   remainder is >= 2^N and therefore certainly >= the N-bit divisor.  The
   wrapped subtraction then yields exactly the low-N-bit mathematical
   remainder.  */

static UQItype
stm8_udivmodqi_core (UQItype dividend, UQItype divisor, UQItype *remainder)
{
  UQItype quotient = 0;
  UQItype rem = 0;

  if (divisor == 0)
    {
      *remainder = 0;
      return 0;
    }

  for (unsigned int bit = 0; bit < 8; ++bit)
    {
      UQItype carry = (UQItype) (rem >> 7);
      rem = (UQItype) ((UQItype) (rem << 1)
		       | (UQItype) ((dividend >> (7 - bit)) & 1));
      if (carry || rem >= divisor)
	{
	  rem = (UQItype) (rem - divisor);
	  quotient = (UQItype) (quotient | (UQItype) (1u << (7 - bit)));
	}
    }

  *remainder = rem;
  return quotient;
}

static UHItype
stm8_udivmodhi_core (UHItype dividend, UHItype divisor, UHItype *remainder)
{
  UHItype quotient = 0;
  UHItype rem = 0;

  if (divisor == 0)
    {
      *remainder = 0;
      return 0;
    }

  for (unsigned int bit = 0; bit < 16; ++bit)
    {
      UHItype carry = (UHItype) (rem >> 15);
      rem = (UHItype) ((UHItype) (rem << 1)
		       | (UHItype) ((dividend >> (15 - bit)) & 1));
      if (carry || rem >= divisor)
	{
	  rem = (UHItype) (rem - divisor);
	  quotient = (UHItype) (quotient | (UHItype) (1u << (15 - bit)));
	}
    }

  *remainder = rem;
  return quotient;
}

static USItype
stm8_udivmodsi_core (USItype dividend, USItype divisor, USItype *remainder)
{
  USItype quotient = 0;
  USItype rem = 0;

  if (divisor == 0)
    {
      *remainder = 0;
      return 0;
    }

  for (unsigned int bit = 0; bit < 32; ++bit)
    {
      USItype carry = rem >> 31;
      rem = (rem << 1) | ((dividend >> (31 - bit)) & 1u);
      if (carry || rem >= divisor)
	{
	  rem -= divisor;
	  quotient |= (USItype) 1u << (31 - bit);
	}
    }

  *remainder = rem;
  return quotient;
}

/* QImode signed division is not normally visible from C because integer
   promotions turn signed-char arithmetic into HImode on STM8.  Keep the
   routines nevertheless: RTL transformations and non-C front ends may still
   request the standard QI libcalls.  */

QItype
__divqi3 (QItype lhs, QItype rhs)
{
  UQItype a = (UQItype) lhs;
  UQItype b = (UQItype) rhs;
  UQItype rem;
  int neg_a = lhs < 0;
  int neg_b = rhs < 0;

  if (neg_a)
    a = (UQItype) (0u - a);
  if (neg_b)
    b = (UQItype) (0u - b);

  UQItype q = stm8_udivmodqi_core (a, b, &rem);
  if (neg_a ^ neg_b)
    q = (UQItype) (0u - q);
  return (QItype) q;
}

QItype
__modqi3 (QItype lhs, QItype rhs)
{
  UQItype a = (UQItype) lhs;
  UQItype b = (UQItype) rhs;
  UQItype rem;
  int neg_a = lhs < 0;

  if (neg_a)
    a = (UQItype) (0u - a);
  if (rhs < 0)
    b = (UQItype) (0u - b);

  (void) stm8_udivmodqi_core (a, b, &rem);
  if (neg_a)
    rem = (UQItype) (0u - rem);
  return (QItype) rem;
}

/* The low half of a product is independent of signedness, so one standard
   libcall implements both signed and unsigned HImode multiplication.  */

HItype
__mulhi3 (HItype lhs, HItype rhs)
{
  UHItype a = (UHItype) lhs;
  UHItype b = (UHItype) rhs;
  UHItype result = 0;

  for (unsigned int i = 0; i < 16; ++i)
    {
      if (b & 1u)
	result = (UHItype) (result + a);
      b = (UHItype) (b >> 1);
      a = (UHItype) (a << 1);
    }

  return (HItype) result;
}

HItype
__divhi3 (HItype lhs, HItype rhs)
{
  UHItype a = (UHItype) lhs;
  UHItype b = (UHItype) rhs;
  UHItype rem;
  int neg_a = lhs < 0;
  int neg_b = rhs < 0;

  if (neg_a)
    a = (UHItype) (0u - a);
  if (neg_b)
    b = (UHItype) (0u - b);

  UHItype q = stm8_udivmodhi_core (a, b, &rem);
  if (neg_a ^ neg_b)
    q = (UHItype) (0u - q);
  return (HItype) q;
}

HItype
__modhi3 (HItype lhs, HItype rhs)
{
  UHItype a = (UHItype) lhs;
  UHItype b = (UHItype) rhs;
  UHItype rem;
  int neg_a = lhs < 0;

  if (neg_a)
    a = (UHItype) (0u - a);
  if (rhs < 0)
    b = (UHItype) (0u - b);

  (void) stm8_udivmodhi_core (a, b, &rem);
  if (neg_a)
    rem = (UHItype) (0u - rem);
  return (HItype) rem;
}

/* These unsigned entry points are normally bypassed by native DIVW, but are
   cheap insurance for code paths that explicitly request the standard
   libfuncs.  */

UHItype
__udivhi3 (UHItype lhs, UHItype rhs)
{
  UHItype rem;
  return stm8_udivmodhi_core (lhs, rhs, &rem);
}

UHItype
__umodhi3 (UHItype lhs, UHItype rhs)
{
  UHItype rem;
  (void) stm8_udivmodhi_core (lhs, rhs, &rem);
  return rem;
}

SItype
__mulsi3 (SItype lhs, SItype rhs)
{
  USItype a = (USItype) lhs;
  USItype b = (USItype) rhs;
  USItype result = 0;

  for (unsigned int i = 0; i < 32; ++i)
    {
      if (b & 1u)
	result += a;
      b >>= 1;
      a <<= 1;
    }

  return (SItype) result;
}

USItype
__udivsi3 (USItype lhs, USItype rhs)
{
  USItype rem;
  return stm8_udivmodsi_core (lhs, rhs, &rem);
}

USItype
__umodsi3 (USItype lhs, USItype rhs)
{
  USItype rem;
  (void) stm8_udivmodsi_core (lhs, rhs, &rem);
  return rem;
}

SItype
__divsi3 (SItype lhs, SItype rhs)
{
  USItype a = (USItype) lhs;
  USItype b = (USItype) rhs;
  USItype rem;
  int neg_a = lhs < 0;
  int neg_b = rhs < 0;

  if (neg_a)
    a = (USItype) (0u - a);
  if (neg_b)
    b = (USItype) (0u - b);

  USItype q = stm8_udivmodsi_core (a, b, &rem);
  if (neg_a ^ neg_b)
    q = (USItype) (0u - q);
  return (SItype) q;
}

SItype
__modsi3 (SItype lhs, SItype rhs)
{
  USItype a = (USItype) lhs;
  USItype b = (USItype) rhs;
  USItype rem;
  int neg_a = lhs < 0;

  if (neg_a)
    a = (USItype) (0u - a);
  if (rhs < 0)
    b = (USItype) (0u - b);

  (void) stm8_udivmodsi_core (a, b, &rem);
  if (neg_a)
    rem = (USItype) (0u - rem);
  return (SItype) rem;
}

/* -------------------------------------------------------------------------
   DImode pointer-result helpers.
   ------------------------------------------------------------------------- */

static void
stm8_u64_zero (unsigned char *p)
{
  for (unsigned int i = 0; i < 8; ++i)
    p[i] = 0;
}

static void
stm8_u64_copy (unsigned char *d, const unsigned char *s)
{
  for (unsigned int i = 0; i < 8; ++i)
    d[i] = s[i];
}

static int
stm8_u64_is_zero (const unsigned char *p)
{
  unsigned char v = 0;
  for (unsigned int i = 0; i < 8; ++i)
    v |= p[i];
  return v == 0;
}

static int
stm8_u64_cmp (const unsigned char *a, const unsigned char *b)
{
  for (unsigned int i = 0; i < 8; ++i)
    {
      if (a[i] < b[i])
	return -1;
      if (a[i] > b[i])
	return 1;
    }
  return 0;
}

static void
stm8_u64_sub_inplace (unsigned char *a, const unsigned char *b)
{
  unsigned int borrow = 0;

  for (int i = 7; i >= 0; --i)
    {
      unsigned int av = a[i];
      unsigned int bv = (unsigned int) b[i] + borrow;
      a[i] = (unsigned char) (av - bv);
      borrow = av < bv;
    }
}

static void
stm8_u64_neg_inplace (unsigned char *a)
{
  unsigned int carry = 1;

  for (int i = 7; i >= 0; --i)
    {
      unsigned int v = (unsigned int) ((unsigned char) ~a[i]) + carry;
      a[i] = (unsigned char) v;
      carry = v >> 8;
    }
}

static void
stm8_u64_shl1 (unsigned char *a)
{
  unsigned int carry = 0;

  for (int i = 7; i >= 0; --i)
    {
      unsigned int v = ((unsigned int) a[i] << 1) | carry;
      a[i] = (unsigned char) v;
      carry = (v >> 8) & 1;
    }
}

void
__stm8_muldi3 (unsigned char *dst, const unsigned char *lhs,
	       const unsigned char *rhs)
{
  stm8_u64_zero (dst);

  /* Base-256 schoolbook product, retaining only the low 64 bits.  */
  for (int ia = 7; ia >= 0; --ia)
    {
      unsigned int carry = 0;

      for (int ib = 7; ib >= 0; --ib)
	{
	  int k = ia + ib - 7;
	  if (k < 0)
	    break;

	  unsigned int v = (unsigned int) dst[k]
			   + (unsigned int) lhs[ia] * (unsigned int) rhs[ib]
			   + carry;

	  dst[k] = (unsigned char) v;
	  carry = v >> 8;
	}
      /* Any carry left after byte 0 is overflow modulo 2^64.  */
    }
}

void
__stm8_udivmoddi4 (unsigned char *quotient, unsigned char *remainder,
		   const unsigned char *dividend, const unsigned char *divisor)
{
  stm8_u64_zero (quotient);
  stm8_u64_zero (remainder);

  /* C division by zero is undefined; keep deterministic output without
     adding a target-specific trap policy here.  */
  if (stm8_u64_is_zero (divisor))
    return;

  /* Restoring binary long division, consuming dividend bits MSB first.  */
  for (unsigned int bit = 0; bit < 64; ++bit)
    {
      stm8_u64_shl1 (remainder);

      unsigned int byte = bit >> 3;
      unsigned int bit_in_byte = bit & 7;
      unsigned char mask = (unsigned char) (0x80u >> bit_in_byte);
      if (dividend[byte] & mask)
	remainder[7] |= 1;

      if (stm8_u64_cmp (remainder, divisor) >= 0)
	{
	  stm8_u64_sub_inplace (remainder, divisor);
	  quotient[byte] |= mask;
	}
    }
}

void
__stm8_divmoddi4 (unsigned char *quotient, unsigned char *remainder,
		  const unsigned char *dividend, const unsigned char *divisor)
{
  unsigned char a[8];
  unsigned char b[8];
  unsigned int neg_a = (dividend[0] & 0x80u) != 0;
  unsigned int neg_b = (divisor[0] & 0x80u) != 0;

  stm8_u64_copy (a, dividend);
  stm8_u64_copy (b, divisor);

  if (neg_a)
    stm8_u64_neg_inplace (a);
  if (neg_b)
    stm8_u64_neg_inplace (b);

  __stm8_udivmoddi4 (quotient, remainder, a, b);

  if (neg_a ^ neg_b)
    stm8_u64_neg_inplace (quotient);
  if (neg_a)
    stm8_u64_neg_inplace (remainder);
}
