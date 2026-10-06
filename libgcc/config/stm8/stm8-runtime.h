/* Shared STM8 runtime declarations.
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

#ifndef LIBGCC_STM8_RUNTIME_H
#define LIBGCC_STM8_RUNTIME_H

typedef int QItype __attribute__ ((mode (QI)));
typedef unsigned int UQItype __attribute__ ((mode (QI)));
typedef int HItype __attribute__ ((mode (HI)));
typedef unsigned int UHItype __attribute__ ((mode (HI)));
typedef int SItype __attribute__ ((mode (SI)));
typedef unsigned int USItype __attribute__ ((mode (SI)));

extern QItype __divqi3 (QItype, QItype);
extern QItype __modqi3 (QItype, QItype);
extern HItype __mulhi3 (HItype, HItype);
extern HItype __divhi3 (HItype, HItype);
extern HItype __modhi3 (HItype, HItype);
extern UHItype __udivhi3 (UHItype, UHItype);
extern UHItype __umodhi3 (UHItype, UHItype);
extern SItype __mulsi3 (SItype, SItype);
extern SItype __divsi3 (SItype, SItype);
extern SItype __modsi3 (SItype, SItype);
extern USItype __udivsi3 (USItype, USItype);
extern USItype __umodsi3 (USItype, USItype);
extern void __stm8_muldi3 (unsigned char *, const unsigned char *,
			   const unsigned char *);
extern void __stm8_udivmoddi4 (unsigned char *, unsigned char *,
			       const unsigned char *, const unsigned char *);
extern void __stm8_divmoddi4 (unsigned char *, unsigned char *,
			      const unsigned char *, const unsigned char *);

typedef unsigned char u_qi;
typedef unsigned short u_hi;
typedef unsigned int __attribute__ ((mode (PSI))) u_psi;
typedef unsigned long u_si;
typedef unsigned long long u_di;

extern int __clzqi2 (u_qi);
extern int __ctzqi2 (u_qi);
extern int __ffsqi2 (u_qi);
extern int __popcountqi2 (u_qi);
extern int __parityqi2 (u_qi);
extern int __clrsbqi2 (u_qi);
extern int __clzhi2 (u_hi);
extern int __ctzhi2 (u_hi);
extern int __ffshi2 (u_hi);
extern int __popcounthi2 (u_hi);
extern int __parityhi2 (u_hi);
extern int __clrsbhi2 (u_hi);
extern int __clzpsi2 (u_psi);
extern int __ctzpsi2 (u_psi);
extern int __ffspsi2 (u_psi);
extern int __popcountpsi2 (u_psi);
extern int __paritypsi2 (u_psi);
extern int __clrsbpsi2 (u_psi);
extern int __clzsi2 (u_si);
extern int __ctzsi2 (u_si);
extern int __ffssi2 (u_si);
extern int __popcountsi2 (u_si);
extern int __paritysi2 (u_si);
extern int __clrsbsi2 (u_si);
extern int __clzdi2 (u_di);
extern int __ctzdi2 (u_di);
extern int __ffsdi2 (u_di);
extern int __popcountdi2 (u_di);
extern int __paritydi2 (u_di);
extern int __clrsbdi2 (u_di);

#endif /* LIBGCC_STM8_RUNTIME_H */
