/* Soft-FP definitions for STM8
   Copyright (C) 2015-2026 Free Software Foundation, Inc.

This file is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 3, or (at your option) any
later version.

This file is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

Under Section 7 of GPL version 3, you are granted additional
permissions described in the GCC Runtime Library Exception, version
3.1, as published by the Free Software Foundation.

You should have received a copy of the GNU General Public License and
a copy of the GCC Runtime Library Exception along with this program;
see the files COPYING3 and COPYING.RUNTIME respectively.  If not, see
<http://www.gnu.org/licenses/>.  */

/* One 64-bit fraction avoids replicated carry/borrow and normalization
   branches in binary64.  Binary32 uses a 32-bit fraction.  This is an
   internal soft-fp representation; exported argument/result ABI is fixed.  */
#ifdef STM8_SOFTFP_WORD64
#define _FP_W_TYPE_SIZE 64
#define _FP_W_TYPE unsigned long long
#define _FP_WS_TYPE signed long long
#else
#define _FP_W_TYPE_SIZE 32
#define _FP_W_TYPE unsigned long
#define _FP_WS_TYPE signed long
#endif
#define _FP_I_TYPE int

typedef int CMPtype __attribute__ ((mode (__libgcc_cmp_return__)));
#define CMPtype CMPtype

#define _FP_MUL_MEAT_S(R, X, Y)						       \
  _FP_MUL_MEAT_1_wide (_FP_WFRACBITS_S, R, X, Y, umul_ppmm)
#ifdef STM8_SOFTFP_WORD64
#define _FP_MUL_MEAT_D(R, X, Y)						       \
  _FP_MUL_MEAT_1_wide (_FP_WFRACBITS_D, R, X, Y, umul_ppmm)
#define _FP_DIV_MEAT_D(R, X, Y) _FP_DIV_MEAT_1_loop (D, R, X, Y)
#define _FP_NANFRAC_D ((_FP_QNANBIT_D << 1) - 1)
#else
#define _FP_MUL_MEAT_D(R, X, Y)						       \
  _FP_MUL_MEAT_2_wide (_FP_WFRACBITS_D, R, X, Y, umul_ppmm)
#define _FP_DIV_MEAT_D(R, X, Y) _FP_DIV_MEAT_2_udiv (D, R, X, Y)
#define _FP_NANFRAC_D ((_FP_QNANBIT_D << 1) - 1), -1
#endif
#define _FP_MUL_MEAT_Q(R, X, Y)						       \
  _FP_MUL_MEAT_4_wide (_FP_WFRACBITS_Q, R, X, Y, umul_ppmm)

#define _FP_DIV_MEAT_S(R, X, Y) _FP_DIV_MEAT_1_loop (S, R, X, Y)
#define _FP_DIV_MEAT_Q(R, X, Y) _FP_DIV_MEAT_4_udiv (Q, R, X, Y)

#define _FP_NANFRAC_S ((_FP_QNANBIT_S << 1) - 1)
#define _FP_NANFRAC_Q ((_FP_QNANBIT_Q << 1) - 1), -1, -1, -1
#define _FP_NANSIGN_S 0
#define _FP_NANSIGN_D 0
#define _FP_NANSIGN_Q 0

#define _FP_KEEPNANFRACP 1
#define _FP_QNANNEGATEDP 0

#define _FP_CHOOSENAN(fs, wc, R, X, Y, OP)				       \
  do									       \
    {									       \
      if ((_FP_FRAC_HIGH_RAW_##fs (X) & _FP_QNANBIT_##fs)		       \
	  && !(_FP_FRAC_HIGH_RAW_##fs (Y) & _FP_QNANBIT_##fs))		       \
	{								       \
	  R##_s = Y##_s;						       \
	  _FP_FRAC_COPY_##wc (R, Y);					       \
	}								       \
      else								       \
	{								       \
	  R##_s = X##_s;						       \
	  _FP_FRAC_COPY_##wc (R, X);					       \
	}								       \
      R##_c = FP_CLS_NAN;						       \
    }									       \
  while (0)

#define _FP_TININESS_AFTER_ROUNDING 1

#define __BIG_ENDIAN 4321

#define __BYTE_ORDER __BIG_ENDIAN

/* Define ALIASNAME as a strong alias for NAME.  */
#define strong_alias(name, aliasname) _strong_alias (name, aliasname)
#define _strong_alias(name, aliasname)					       \
  extern __typeof (name) aliasname __attribute__ ((alias (#name)));

/* IEEE formats require 32-bit bitfield containers even with 16-bit int.  */
#define _FP_BITFIELD_TYPE USItype
