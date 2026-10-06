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

   Scalar bit counts avoid recursive calls to compiler bit builtins.  */

/* Bit-count helpers return native int, as required by optabs.cc.  */
#include "stm8-runtime.h"
int
__clzqi2 (u_qi value)
{
  u_qi mask = (u_qi) 1 << 7;
  int count = 0;
  while (mask && !(value & mask))
    {
      mask >>= 1;
      ++count;
    }
  return count;
}

int
__ctzqi2 (u_qi value)
{
  u_qi mask = (u_qi) 1;
  int count = 0;
  while (mask && !(value & mask))
    {
      mask <<= 1;
      ++count;
    }
  return count;
}

int
__ffsqi2 (u_qi value)
{
  if (!value)
    return 0;
  u_qi mask = (u_qi) 1;
  int count = 0;
  while (mask && !(value & mask))
    {
      mask <<= 1;
      ++count;
    }
  return count + 1;
}

int
__popcountqi2 (u_qi value)
{
  int count = 0;
  for (unsigned char i = 0; i < 8; ++i)
    {
      count += value & 1;
      value >>= 1;
    }
  return count;
}

int
__parityqi2 (u_qi value)
{
  int count = 0;
  for (unsigned char i = 0; i < 8; ++i)
    {
      count += value & 1;
      value >>= 1;
    }
  return count & 1;
}

int
__clrsbqi2 (u_qi value)
{
  if (value >> 7)
    value = ~value;
  return __clzqi2 (value) - 1;
}

int
__clzhi2 (u_hi value)
{
  u_hi mask = (u_hi) 1 << 15;
  int count = 0;
  while (mask && !(value & mask))
    {
      mask >>= 1;
      ++count;
    }
  return count;
}

int
__ctzhi2 (u_hi value)
{
  u_hi mask = (u_hi) 1;
  int count = 0;
  while (mask && !(value & mask))
    {
      mask <<= 1;
      ++count;
    }
  return count;
}

int
__ffshi2 (u_hi value)
{
  if (!value)
    return 0;
  u_hi mask = (u_hi) 1;
  int count = 0;
  while (mask && !(value & mask))
    {
      mask <<= 1;
      ++count;
    }
  return count + 1;
}

int
__popcounthi2 (u_hi value)
{
  int count = 0;
  for (unsigned char i = 0; i < 16; ++i)
    {
      count += value & 1;
      value >>= 1;
    }
  return count;
}

int
__parityhi2 (u_hi value)
{
  int count = 0;
  for (unsigned char i = 0; i < 16; ++i)
    {
      count += value & 1;
      value >>= 1;
    }
  return count & 1;
}

int
__clrsbhi2 (u_hi value)
{
  if (value >> 15)
    value = ~value;
  return __clzhi2 (value) - 1;
}

int
__clzpsi2 (u_psi value)
{
  u_psi mask = (u_psi) 1 << 23;
  int count = 0;
  while (mask && !(value & mask))
    {
      mask >>= 1;
      ++count;
    }
  return count;
}

int
__ctzpsi2 (u_psi value)
{
  u_psi mask = (u_psi) 1;
  int count = 0;
  while (mask && !(value & mask))
    {
      mask <<= 1;
      ++count;
    }
  return count;
}

int
__ffspsi2 (u_psi value)
{
  if (!value)
    return 0;
  u_psi mask = (u_psi) 1;
  int count = 0;
  while (mask && !(value & mask))
    {
      mask <<= 1;
      ++count;
    }
  return count + 1;
}

int
__popcountpsi2 (u_psi value)
{
  int count = 0;
  for (unsigned char i = 0; i < 24; ++i)
    {
      count += value & 1;
      value >>= 1;
    }
  return count;
}

int
__paritypsi2 (u_psi value)
{
  int count = 0;
  for (unsigned char i = 0; i < 24; ++i)
    {
      count += value & 1;
      value >>= 1;
    }
  return count & 1;
}

int
__clrsbpsi2 (u_psi value)
{
  if (value >> 23)
    value = ~value;
  return __clzpsi2 (value) - 1;
}

int
__clzsi2 (u_si value)
{
  u_si mask = (u_si) 1 << 31;
  int count = 0;
  while (mask && !(value & mask))
    {
      mask >>= 1;
      ++count;
    }
  return count;
}

int
__ctzsi2 (u_si value)
{
  u_si mask = (u_si) 1;
  int count = 0;
  while (mask && !(value & mask))
    {
      mask <<= 1;
      ++count;
    }
  return count;
}

int
__ffssi2 (u_si value)
{
  if (!value)
    return 0;
  u_si mask = (u_si) 1;
  int count = 0;
  while (mask && !(value & mask))
    {
      mask <<= 1;
      ++count;
    }
  return count + 1;
}

int
__popcountsi2 (u_si value)
{
  int count = 0;
  for (unsigned char i = 0; i < 32; ++i)
    {
      count += value & 1;
      value >>= 1;
    }
  return count;
}

int
__paritysi2 (u_si value)
{
  int count = 0;
  for (unsigned char i = 0; i < 32; ++i)
    {
      count += value & 1;
      value >>= 1;
    }
  return count & 1;
}

int
__clrsbsi2 (u_si value)
{
  if (value >> 31)
    value = ~value;
  return __clzsi2 (value) - 1;
}

int
__clzdi2 (u_di value)
{
  u_di mask = (u_di) 1 << 63;
  int count = 0;
  while (mask && !(value & mask))
    {
      mask >>= 1;
      ++count;
    }
  return count;
}

int
__ctzdi2 (u_di value)
{
  u_di mask = (u_di) 1;
  int count = 0;
  while (mask && !(value & mask))
    {
      mask <<= 1;
      ++count;
    }
  return count;
}

int
__ffsdi2 (u_di value)
{
  if (!value)
    return 0;
  u_di mask = (u_di) 1;
  int count = 0;
  while (mask && !(value & mask))
    {
      mask <<= 1;
      ++count;
    }
  return count + 1;
}

int
__popcountdi2 (u_di value)
{
  int count = 0;
  for (unsigned char i = 0; i < 64; ++i)
    {
      count += value & 1;
      value >>= 1;
    }
  return count;
}

int
__paritydi2 (u_di value)
{
  int count = 0;
  for (unsigned char i = 0; i < 64; ++i)
    {
      count += value & 1;
      value >>= 1;
    }
  return count & 1;
}

int
__clrsbdi2 (u_di value)
{
  if (value >> 63)
    value = ~value;
  return __clzdi2 (value) - 1;
}
