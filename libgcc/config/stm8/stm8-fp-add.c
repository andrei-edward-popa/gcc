/* Compact IEEE byte arithmetic for the STM8 runtime.
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
   <http://www.gnu.org/licenses/>.  */

/* Fractions are unsigned, big-endian byte arrays with three extra bits
   for guard/round/sticky.  Byte loops share code between binary32/64 and
   avoid expanding soft-fp's two-word normalization branches repeatedly.  */
typedef unsigned char byte;
#define SMALL __attribute__ ((noinline, noclone, noipa))
struct number
{
  byte m[8], sign, kind;
  unsigned e;
};
static SMALL void
left (byte *m)
{
  byte carry = 0;
  for (byte i = 8; i--;)
    {
      byte next = m[i] >> 7;
      m[i] = (byte) (m[i] << 1) | carry;
      carry = next;
    }
}

static SMALL byte
right (byte *m)
{
  byte carry = 0;
  for (byte i = 0; i < 8; i++)
    {
      byte next = m[i] << 7;
      m[i] = (m[i] >> 1) | carry;
      carry = next;
    }
  return carry != 0;
}

static SMALL byte
nonzero (const byte *m)
{
  byte r = 0;
  for (byte i = 0; i < 8; i++)
    r |= m[i];
  return r != 0;
}

static SMALL void
sticky (byte *m, unsigned n)
{
  if (n >= 64)
    {
      byte r = nonzero (m);
      for (byte i = 0; i < 8; i++)
	m[i] = 0;
      m[7] = r;
      return;
    }
  while (n--)
    {
      byte lost = right (m);
      m[7] |= lost;
    }
}

static SMALL void
decode (struct number *r, const byte *a, byte width)
{
  for (byte i = 0; i < 8; i++)
    r->m[i] = 0;
  r->sign = a[0] >> 7;
  unsigned e;
  if (width == 4)
    {
      e = ((unsigned) (a[0] & 127) << 1) | (a[1] >> 7);
      r->m[5] = a[1] & 127;
      r->m[6] = a[2];
      r->m[7] = a[3];
    }
  else
    {
      e = ((unsigned) (a[0] & 127) << 4) | (a[1] >> 4);
      r->m[1] = a[1] & 15;
      for (byte i = 2; i < 8; i++)
	r->m[i] = a[i];
    }
  r->kind = e == (width == 4 ? 255 : 2047) ? (nonzero (r->m) ? 2 : 1) : 0;
  r->e = e ? e : 1;
  if (e && !r->kind)
    r->m[width == 4 ? 5 : 1] |= width == 4 ? 128 : 16;
  for (byte i = 0; i < 3; i++)
    left (r->m);
}

static SMALL int
magnitude (const struct number *a, const struct number *b)
{
  if (a->e != b->e)
    return a->e > b->e ? 1 : -1;
  for (byte i = 0; i < 8; i++)
    if (a->m[i] != b->m[i])
      return a->m[i] > b->m[i] ? 1 : -1;
  return 0;
}

static SMALL void
pack (byte *out, byte *m, unsigned e, byte sign, byte width)
{
  for (byte i = 0; i < 3; i++)
    right (m);
  if (width == 4)
    {
      out[0] = (sign << 7) | (e >> 1);
      out[1] = (e << 7) | (m[5] & 127);
      out[2] = m[6];
      out[3] = m[7];
    }
  else
    {
      out[0] = (sign << 7) | (e >> 4);
      out[1] = (e << 4) | (m[1] & 15);
      for (byte i = 2; i < 8; i++)
	out[i] = m[i];
    }
}

static SMALL void
add (byte *out, const byte *a, const byte *b, byte width, byte subtract)
{
  struct number x, y;
  decode (&x, a, width);
  decode (&y, b, width);
  byte quiet = width == 4 ? 64 : 8;
  if (x.kind == 2 || y.kind == 2)
    {
      const byte *n = x.kind == 2 ? a : b;
      if (x.kind == 2 && y.kind == 2 && (a[1] & quiet) && !(b[1] & quiet))
	n = b;
      for (byte i = 0; i < width; i++)
	out[i] = n[i];
      out[1] |= quiet;
      return;
    }
  y.sign ^= subtract;
  if (x.kind || y.kind)
    {
      if (x.kind && y.kind && x.sign != y.sign)
	{
	  for (byte i = 0; i < width; i++)
	    out[i] = 255;
	  out[0] = 127;
	  return;
	}
      const byte *n = x.kind ? a : b;
      for (byte i = 0; i < width; i++)
	out[i] = n[i];
      out[0] = (out[0] & 127) | ((x.kind ? x.sign : y.sign) << 7);
      return;
    }
  struct number *large = &x, *small = &y;
  int relation = magnitude (&x, &y);
  if (relation < 0)
    {
      large = &y;
      small = &x;
    }
  unsigned e = large->e;
  sticky (small->m, e - small->e);
  byte sign = large->sign;
  unsigned carry = 0;
  if (x.sign == y.sign)
    {
      for (byte i = 8; i--;)
	{
	  unsigned v = (unsigned) large->m[i] + small->m[i] + carry;
	  large->m[i] = v;
	  carry = v >> 8;
	}
    }
  else
    {
      for (byte i = 8; i--;)
	{
	  unsigned v = (unsigned) large->m[i] - small->m[i] - carry;
	  large->m[i] = v;
	  carry = (v >> 8) != 0;
	}
      if (!nonzero (large->m))
	sign = 0;
    }
  byte top = width == 4 ? 4 : 1, topmask = width == 4 ? 4 : 128;
  byte over = width == 4 ? 4 : 0, overmask = width == 4 ? 8 : 1;
  if (large->m[over] & overmask)
    {
      sticky (large->m, 1);
      ++e;
    }
  else if (nonzero (large->m))
    while (!(large->m[top] & topmask) && e > 1)
      {
	left (large->m);
	--e;
      }
  byte tail = large->m[7] & 7;
  if (tail > 4 || (tail == 4 && (large->m[7] & 8)))
    {
      carry = 8;
      for (byte i = 8; i--;)
	{
	  unsigned v = (unsigned) large->m[i] + carry;
	  large->m[i] = v;
	  carry = v >> 8;
	}
      if (large->m[over] & overmask)
	{
	  right (large->m);
	  ++e;
	}
    }
  if (e >= (width == 4 ? 255 : 2047))
    {
      e = width == 4 ? 255 : 2047;
      for (byte i = 0; i < 8; i++)
	large->m[i] = 0;
    }
  else if (!(large->m[top] & topmask))
    e = 0;
  pack (out, large->m, e, sign, width);
}

#ifdef STM8_FP_HOST_TEST
void
stm8_fp_add_bytes (byte *out, const byte *a, const byte *b, byte width,
		   byte subtract)
{
  add (out, a, b, width, subtract);
}

#else
typedef float SFtype;
typedef float DFtype __attribute__ ((mode (DF)));
#define WRAPPER(name, type, width, sub)					       \
  type name (type a, type b);						       \
  type name (type a, type b)						       \
  {									       \
    union								       \
    {									       \
      type f;								       \
      byte bytes[width];						       \
    } x, y, r;								       \
    x.f = a;								       \
    y.f = b;								       \
    add (r.bytes, x.bytes, y.bytes, width, sub);			       \
    return r.f;								       \
  }
WRAPPER (__addsf3, SFtype, 4, 0)
WRAPPER (__subsf3, SFtype, 4, 1)
WRAPPER (__adddf3, DFtype, 8, 0)
WRAPPER (__subdf3, DFtype, 8, 1)
#endif

/* Binary32 multiplication/division use the same byte representation.
   Three rounding bits plus a sticky bit suffice for round-to-nearest,
   including gradual underflow.  No integer wider than 16 bits is needed.  */
static SMALL void
sf_round (byte *out, byte *m, int e, byte sign)
{
  if (m[4] & 8)
    {
      sticky (m, 1);
      ++e;
    }
  if (e <= 0)
    {
      sticky (m, (unsigned) (1 - e));
      e = 1;
    }
  byte tail = m[7] & 7;
  if (tail > 4 || (tail == 4 && (m[7] & 8)))
    {
      unsigned carry = 8;
      for (byte i = 8; i--;)
	{
	  unsigned v = (unsigned) m[i] + carry;
	  m[i] = v;
	  carry = v >> 8;
	}
      if (m[4] & 8)
	{
	  right (m);
	  ++e;
	}
    }
  if (e >= 255)
    {
      e = 255;
      for (byte i = 0; i < 8; i++)
	m[i] = 0;
    }
  else if (!(m[4] & 4))
    e = 0;
  pack (out, m, (unsigned) e, sign, 4);
}

static SMALL int
sf_normalize (struct number *n)
{
  int e = n->e;
  while (!(n->m[4] & 4))
    {
      left (n->m);
      --e;
    }
  for (byte i = 0; i < 3; i++)
    right (n->m);
  return e;
}

static SMALL byte
sf_special (byte *out, const byte *a, const byte *b, struct number *x,
	    struct number *y, byte divide)
{
  byte sign = x->sign ^ y->sign;
  if (x->kind == 2 || y->kind == 2)
    {
      const byte *n = x->kind == 2 ? a : b;
      if (x->kind == 2 && y->kind == 2 && (a[1] & 64) && !(b[1] & 64))
	n = b;
      for (byte i = 0; i < 4; i++)
	out[i] = n[i];
      out[1] |= 64;
      return 1;
    }
  byte xzero = !x->kind && !nonzero (x->m), yzero = !y->kind && !nonzero (y->m);
  byte invalid = divide ? (x->kind && y->kind) || (xzero && yzero)
			: (x->kind && yzero) || (y->kind && xzero);
  if (invalid)
    {
      for (byte i = 0; i < 4; i++)
	out[i] = 255;
      out[0] = 127;
      return 1;
    }
  byte infinity = divide ? x->kind || yzero : x->kind || y->kind;
  byte zero = divide ? xzero || y->kind : xzero || yzero;
  if (infinity || zero)
    {
      out[0] = (sign << 7) | (infinity ? 127 : 0);
      out[1] = infinity ? 128 : 0;
      out[2] = out[3] = 0;
      return 1;
    }
  return 0;
}

static SMALL void
sf_mul (byte *out, const byte *a, const byte *b)
{
  struct number x, y;
  decode (&x, a, 4);
  decode (&y, b, 4);
  if (sf_special (out, a, b, &x, &y, 0))
    return;
  int e = sf_normalize (&x) + sf_normalize (&y) - 127;
  byte product[8] = { 0 };
  for (byte i = 3; i--;)
    {
      unsigned carry = 0;
      for (byte j = 3; j--;)
	{
	  byte k = i + j + 3;
	  unsigned v = (unsigned) x.m[i + 5] * y.m[j + 5] + product[k] + carry;
	  product[k] = v;
	  carry = v >> 8;
	}
      product[i + 2] = carry;
    }
  sticky (product, 20);
  sf_round (out, product, e, x.sign ^ y.sign);
}

static SMALL int
sf_compare (const byte *a, const byte *b)
{
  for (byte i = 0; i < 8; i++)
    if (a[i] != b[i])
      return a[i] > b[i] ? 1 : -1;
  return 0;
}

static SMALL void
sf_div (byte *out, const byte *a, const byte *b)
{
  struct number x, y;
  decode (&x, a, 4);
  decode (&y, b, 4);
  if (sf_special (out, a, b, &x, &y, 1))
    return;
  int e = sf_normalize (&x) - sf_normalize (&y) + 127;
  if (sf_compare (x.m, y.m) < 0)
    {
      left (x.m);
      --e;
    }
  byte quotient[8] = { 0 };
  for (byte bit = 0; bit < 27; bit++)
    {
      left (quotient);
      if (sf_compare (x.m, y.m) >= 0)
	{
	  unsigned borrow = 0;
	  for (byte i = 8; i--;)
	    {
	      unsigned v = (unsigned) x.m[i] - y.m[i] - borrow;
	      x.m[i] = v;
	      borrow = (v >> 8) != 0;
	    }
	  quotient[7] |= 1;
	}
      left (x.m);
    }
  if (nonzero (x.m))
    quotient[7] |= 1;
  sf_round (out, quotient, e, x.sign ^ y.sign);
}

#ifdef STM8_FP_HOST_TEST
void
stm8_fp_muldiv_bytes (byte *out, const byte *a, const byte *b, byte divide)
{
  if (divide)
    sf_div (out, a, b);
  else
    sf_mul (out, a, b);
}

#else
#define SF_WRAPPER(name, operation)					       \
  SFtype name (SFtype a, SFtype b);					       \
  SFtype name (SFtype a, SFtype b)					       \
  {									       \
    union								       \
    {									       \
      SFtype f;								       \
      byte bytes[4];							       \
    } x, y, r;								       \
    x.f = a;								       \
    y.f = b;								       \
    operation (r.bytes, x.bytes, y.bytes);				       \
    return r.f;								       \
  }
SF_WRAPPER (__mulsf3, sf_mul)
SF_WRAPPER (__divsf3, sf_div)
#endif
