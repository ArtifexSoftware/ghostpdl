/* Copyright (C) 1989, 1995, 1996, 1997 Aladdin Enterprises.  All rights reserved.
  
  This file is part of Aladdin Ghostscript.
  
  Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
  or distributor accepts any responsibility for the consequences of using it,
  or for whether it serves any particular purpose or works at all, unless he
  or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
  License (the "License") for full details.
  
  Every copy of Aladdin Ghostscript must include a copy of the License,
  normally in a plain ASCII text file named PUBLIC.  The License grants you
  the right to copy, modify and redistribute Aladdin Ghostscript, but only
  under certain conditions described in the License.  Among other things, the
  License requires that the copyright notice and this notice be preserved on
  all copies.
*/

/* gxfixed.h */
/* Fixed-point arithmetic for Ghostscript */

#ifndef gxfixed_INCLUDED
#  define gxfixed_INCLUDED

/*
 * Coordinates are generally represented internally by fixed-point
 * quantities: integers lose accuracy in crucial places,
 * and floating point arithmetic is slow.
 */
typedef long fixed;
typedef ulong ufixed;		/* only used in a very few places */
#define max_fixed max_long
#define min_fixed min_long
#define fixed_0 0L
#define fixed_epsilon 1L
/*
 * 12 bits of fraction provides both the necessary accuracy and
 * a sufficiently large range of coordinates.
 */
#define _fixed_shift 12
#define fixed_fraction_bits _fixed_shift
#define fixed_int_bits (sizeof(fixed) * 8 - _fixed_shift)
#define fixed_scale (1<<_fixed_shift)
#define _fixed_rshift(x) arith_rshift(x,_fixed_shift)
#define _fixed_round_v (fixed_scale>>1)
#define _fixed_fraction_v (fixed_scale-1)
/*
 * We use a center-of-pixel filling rule; Adobe specifies that coordinates
 * designate half-open regions.  Because of this, we need special rounding
 * to go from a coordinate to the pixel it falls in.  We use the term
 * "pixel rounding" for this kind of rounding.
 */
#define _fixed_pixround_v (_fixed_round_v - fixed_epsilon)

/*
 * Most operations can be done directly on fixed-point quantities:
 * addition, subtraction, shifting, multiplication or division by
 * (integer) constants; assignment, assignment with zero;
 * comparison, comparison against zero.
 * Multiplication and division by floats is OK if the result is
 * explicitly cast back to fixed.
 * Conversion to and from int and float types must be done explicitly.
 * Note that if we are casting a fixed to a float in a context where
 * only ratios and not actual values are involved, we don't need to take
 * the scale factor into account: we can simply cast to float directly.
 */
#define int2fixed(i) ((fixed)(i)<<_fixed_shift)
/* Define some useful constants. */
/* Avoid casts, so strict ANSI compilers will accept them in #ifs. */
#define fixed_1 (fixed_epsilon << _fixed_shift)
#define fixed_half (fixed_1 >> 1)
/*
 * On 16-bit systems, we can convert fixed variables to ints more efficiently
 * than general fixed quantities.  For this reason, we define two separate
 * sets of conversion macros.
 */
#define fixed2int(x) ((int)_fixed_rshift(x))
#define fixed2int_rounded(x) ((int)_fixed_rshift((x)+_fixed_round_v))
#define fixed2int_ceiling(x) ((int)_fixed_rshift((x)+_fixed_fraction_v))
#define fixed_pre_pixround(x) ((x)+_fixed_pixround_v)
#define fixed2int_pixround(x) fixed2int(fixed_pre_pixround(x))
#define fixed_is_int(x) !((x)&_fixed_fraction_v)
#if arch_ints_are_short & !arch_is_big_endian
/* Do some of the shifting and extraction ourselves. */
#  define _fixed_hi(x) *((uint *)&(x)+1)
#  define _fixed_lo(x) *((uint *)&(x))
#  define fixed2int_var(x)\
	((int)((_fixed_hi(x) << (16-_fixed_shift)) +\
	       (_fixed_lo(x) >> _fixed_shift)))
#  define fixed2int_var_rounded(x)\
	((int)((_fixed_hi(x) << (16-_fixed_shift)) +\
	       (((_fixed_lo(x) >> (_fixed_shift-1))+1)>>1)))
#  define fixed2int_var_ceiling(x)\
	(fixed2int_var(x) -\
	 arith_rshift((int)-(_fixed_lo(x) & _fixed_fraction_v), _fixed_shift))
#else
/* Use reasonable definitions. */
#  define fixed2int_var(x) fixed2int(x)
#  define fixed2int_var_rounded(x) fixed2int_rounded(x)
#  define fixed2int_var_ceiling(x) fixed2int_ceiling(x)
#endif
#define fixed2int_var_pixround(x) fixed2int_pixround(x)
#define fixed2long(x) ((long)_fixed_rshift(x))
#define fixed2long_rounded(x) ((long)_fixed_rshift((x)+_fixed_round_v))
#define fixed2long_ceiling(x) ((long)_fixed_rshift((x)+_fixed_fraction_v))
#define fixed2long_pixround(x) ((long)_fixed_rshift((x)+_fixed_pixround_v))
#define float2fixed(f) ((fixed)((f)*(float)fixed_scale))
/* Note that fixed2float actually produces a double result. */
#define fixed2float(x) ((x)*(1.0/fixed_scale))

/* Rounding and truncation on fixeds */
#define fixed_floor(x) ((x)&(-1L<<_fixed_shift))
#define fixed_rounded(x) (((x)+_fixed_round_v)&(-1L<<_fixed_shift))
#define fixed_ceiling(x) (((x)+_fixed_fraction_v)&(-1L<<_fixed_shift))
#define fixed_pixround(x) (((x)+_fixed_pixround_v)&(-1L<<_fixed_shift))
#define fixed_fraction(x) ((int)(x)&_fixed_fraction_v)
/* I don't see how to do truncation towards 0 so easily.... */
#define fixed_truncated(x) ((x) < 0 ? fixed_ceiling(x) : fixed_floor(x))

#ifdef USE_FPU
#  define USE_FPU_FIXED (USE_FPU < 0 && arch_floats_are_IEEE && arch_sizeof_long == 4)
#else
#  define USE_FPU_FIXED 0
#endif

/*
 * Define a procedure for computing a * b / c when b and c are non-negative,
 * b < c, and a * b exceeds (or might exceed) the capacity of a long.
 * It's really annoying that C doesn't provide any way to get at
 * the double-length multiply/divide instructions that almost all hardware
 * provides....
 */

#if USE_FPU_FIXED
fixed fixed_mult_quo(P3(fixed A, fixed B, fixed C));
#else
#  define fixed_mult_quo(fixed_a, fixed_b, fixed_c)\
    ((fixed)floor((double)(fixed_a) * (fixed_b) / (fixed_c)))
#endif

/*
 * Transforming coordinates involves multiplying two floats, or a float
 * and a double, and then converting the result to a fixed.  Since this
 * operation is so common, we provide an alternative implementation of it
 * on machines that use IEEE floating point representation but don't have
 * floating point hardware.  The implementation may be in either C or
 * assembler.
 */

/*
 * set_fmul2fixed_vars(R, FA, FB, dtemp) computes R = FA * FB:
 *	R is a fixed, FA and FB are floats (not doubles), and
 *	  dtemp is a temporary double.
 * set_dfmul2fixed_vars(R, DA, FB, dtemp) computes R = DA * FB:
 *	R is a fixed, DA is a double, FB is a float, and
 *	  dtemp is a temporary double.
 * R, FA, FB, and DA must be variables, not expressions.
 */
#if USE_FPU_FIXED && arch_sizeof_short == 2
int set_fmul2fixed_(P3(fixed *, long, long));
#define set_fmul2fixed_vars(vr,vfa,vfb,dtemp)\
  set_fmul2fixed_(&vr, *(long *)&vfa, *(long *)&vfb)
int set_dfmul2fixed_(P4(fixed *, ulong, long, long));
#  if arch_is_big_endian
#  define set_dfmul2fixed_vars(vr,vda,vfb,dtemp)\
     set_dfmul2fixed_(&vr, ((ulong *)&vda)[1], *(long *)&vfb, *(long *)&vda)
#  else
#  define set_dfmul2fixed_vars(vr,vda,vfb,dtemp)\
     set_dfmul2fixed_(&vr, *(ulong *)&vda, *(long *)&vfb, ((long *)&vda)[1])
#  endif
#else			/* don't bother */
#  define set_fmul2fixed_vars(vr,vfa,vfb,dtemp)\
     (dtemp = (vfa) * (vfb),\
      (f_fits_in_bits(dtemp, fixed_int_bits) ? (vr = float2fixed(dtemp), 0) :\
       gs_note_error(gs_error_limitcheck)))
#  define set_dfmul2fixed_vars(vr,vda,vfb,dtemp)\
     (dtemp = (vda) * (vfb),\
      (f_fits_in_bits(dtemp, fixed_int_bits) ? (vr = float2fixed(dtemp), 0) :\
       gs_note_error(gs_error_limitcheck)))
#endif
/*
 * set_float2fixed_vars(R, F) does the equivalent of R = float2fixed(F):
 *	R is a fixed, F is a float or a double.
 * set_fixed2float_var(R, V) does the equivalent of R = fixed2float(V):
 *	R is a float or a double, V is a fixed.
 * set_ldexp_fixed2double(R, V, E) does the equivalent of R=ldexp((double)V,E):
 *	R is a double, V is a fixed, E is an int.
 * R and F must be variables, not expressions; V and E may be expressions.
 */
#if USE_FPU_FIXED
int set_float2fixed_(P3(fixed *, long, int));
int set_double2fixed_(P4(fixed *, ulong, long, int));
# define set_float2fixed_vars(vr,vf)\
    (sizeof(vf) == sizeof(float) ?\
     set_float2fixed_(&vr, *(long *)&vf, fixed_fraction_bits) :\
     set_double2fixed_(&vr, ((ulong *)&vf)[arch_is_big_endian],\
		       ((long *)&vf)[1 - arch_is_big_endian],\
		       fixed_fraction_bits))
long fixed2float_(P2(fixed, int));
void set_fixed2double_(P3(double *, fixed, int));
# define set_fixed2float_var(vf,x)\
    (sizeof(vf) == sizeof(float) ?\
     (*(long *)&vf = fixed2float_(x, fixed_fraction_bits), 0) :\
     (set_fixed2double_(&vf, x, fixed_fraction_bits), 0))
#define set_ldexp_fixed2double(vd, x, exp)\
  set_fixed2double_(&vd, x, -(exp))
#else
# define set_float2fixed_vars(vr,vf)\
    (f_fits_in_bits(vf, fixed_int_bits) ? (vr = float2fixed(vf), 0) :\
     gs_note_error(gs_error_limitcheck))
# define set_fixed2float_var(vf,x)\
    (vf = fixed2float(x), 0)
# define set_ldexp_fixed2double(vd, x, exp)\
    discard(vd = ldexp((double)(x), exp))
#endif

/* A point with fixed coordinates */
typedef struct gs_fixed_point_s {
	fixed x, y;
} gs_fixed_point;

/* A rectangle with fixed coordinates */
typedef struct gs_fixed_rect_s {
	gs_fixed_point p, q;
} gs_fixed_rect;

#endif					/* gxfixed_INCLUDED */
