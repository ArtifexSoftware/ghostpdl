/* Copyright (C) 1989 Aladdin Enterprises.  All rights reserved.
  
  This software is licensed to a single customer by Artifex Software Inc.
  under the terms of a specific OEM agreement.
*/

/*$RCSfile$ $Revision$ */
/* Substitute for math.h on VAX/VMS systems */

#ifndef vmsmath_INCLUDED
#  define vmsmath_INCLUDED

/* DEC VAX/VMS C comes with a math.h file but GNU VAX/VMS C does not. */
/* This file substitutes for math.h when using GNU C. */
#  ifndef __MATH
#    define __MATH
#    if CC$gfloat
#      define HUGE_VAL 8.988465674311578540726371186585e+307
#    else
#      define HUGE_VAL 1.70141183460469229e+38
#    endif
extern double acos(), asin(), atan(), atan2();
extern double sin(), tan(), cos();
extern double cosh(), sinh(), tanh();
extern double exp(), frexp(), ldexp(), log(), log10(), pow();
extern double modf(), fmod(), sqrt(), ceil(), floor();
extern double fabs(), cabs(), hypot();

#  endif

#endif /* vmsmath_INCLUDED */
