/* Copyright (C) 2001-2023 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  39 Mesa Street, Suite 108A, San Francisco,
   CA 94129, USA, for further information.
*/


/* Definitions for DDAs */
/* Requires gxfixed.h */

#ifndef gxdda_INCLUDED
#  define gxdda_INCLUDED

#include "stdpre.h"
#include "gxfixed.h"

/* We use the familiar Bresenham DDA algorithm for several purposes:
 *      - tracking the edges when filling trapezoids;
 *      - tracking the current pixel corner coordinates when rasterizing
 *      skewed or rotated images;
 *      - converting curves to sequences of lines (this is a 3rd-order
 *      DDA, the others are 1st-order);
 *      - perhaps someday for drawing single-pixel lines.
 * In the case of trapezoids, lines, and curves, we need to use
 * the DDA to find the integer X values at integer+0.5 values of Y;
 * in the case of images, we use DDAs to compute the (fixed)
 * X and Y values at (integer) source pixel corners.
 *
 * The purpose of the DDA is to compute the exact values Q(i) = floor(i*D/N)
 * for increasing integers i, 0 <= i <= N.  D is considered to be an
 * integer, although it may actually be a fixed.
 *
 * In the original formulation of the algorithm, we maintained i*D/N as
 * Q + (N-R)/N where Q and R are integers, where 0 < R <= N, with the
 * following auxiliary values:
 *      dQ = floor(D/N)
 *      dR = D mod N (0 <= dR < N)
 *      NdR = N - dR
 * And at each iteration the code did:
 *      Q += dQ;
 *      if ( R > dR ) R -= dR; else ++Q, R += NdR;
 *
 * In the new formulation here, we hold i*D/N as Q + (N-1-R)/N where Q and R
 * are integers, 0 <= R < N.
 *      Q += dQ;
 *      R -= dR
 *      if ( R < 0) R += N, ++Q;
 * These formulas work regardless of the sign of D, and never let R go
 * out of range.
 *
 * Why is the new formulation better? Well, it's simpler for one thing - the
 * values stored in the structure are the obvious ones, without the strange
 * NdR one. Also, in the original we test R > dR, which takes as long as
 * doing R-=dR in the first place; in the new code we test R against 0, which
 * we get for free on most architectures.
 *
 * In architectures that use branches for alternation, the first (should)
 * compiles to something like (excuse the pseudo code):
 *
 *   Q += dQ
 *   if (R > dR)
 *      goto A
 *   R -= dR
 *   goto B
 * A:
 *   Q += 1
 *   R += NdR
 * B:
 *
 * So 7 'instructions', 5 on each possible route through the code, including
 * 1 branch regardless of the values.
 *
 * In the new form, it compiles to:
 *
 *   R -= dR
 *   if (R >= 0)     <- this instruction for free due to preceeding one
 *       goto A:
 *   R += N
 *   Q++
 * A:
 *   Q += dQ
 *
 * So 5 'instructions', 3 on the no step case, 5 on the step case, including
 * 1 branch on the no-step case.
 *
 * If the compiler is smart enough to use the carry flag, it becomes:
 *
 *   R -= dR
 *   if (R >= 0)     <- this instruction for free due to preceeding one
 *       goto A:
 *   R += N
 * A:
 *   Q += dQ + C     <- Add with carry
 *
 * 4 instructions total, 3 on the no step case, 4 on the step case, including
 * 1 branch on the no-step case.
 *
 * This is an even better win on architectures (like ARM) where alternation
 * can be done without branches; the original gives:
 *
 *   ADD   Q, Q, dQ
 *   CMP   R, dR
 *   SUBGT R, R, dR
 *   ADDLE Q, Q, #1
 *   ADDLE R, R, nDR
 *
 * (assuming all values in registers) and the new formulation becomes:
 *
 *   SUBS  R, R, dR
 *   ADDLT R, R, N
 *   ADDLT Q, Q, #1
 *   ADD   Q, Q, dQ
 *
 * If the compiler is smart enough to use the carry flag, we can do even
 * better:
 *
 *   SUBS  R, R, dR
 *   ADDLT R, R, N
 *   ADC   Q, Q, dQ
 *
 * (Actually looking at the compilation given by MSVC 2005 confirms a 1
 * instruction (and one branch) win for the above results. Sadly compilers
 * don't look to be smart enough to exploit carry flag tricks.)
 */

/* In the following structure definitions, ntype must be an unsigned type. */
#define dda_state_struct(sname, dtype, ntype)\
  struct sname { dtype Q; ntype R; }
#define dda_step_struct(sname, dtype, ntype)\
  struct sname { dtype dQ; ntype dR, N; }

/* DDA with int Q and uint N */
typedef struct gx_dda_int_s {
    dda_state_struct(ia_, int, uint) state;
    dda_step_struct(ie_, int, uint) step;
} gx_dda_int_t;

/* DDA with fixed Q and (unsigned) integer N */
typedef
dda_state_struct(_a, fixed, uint) gx_dda_state_fixed;
     typedef dda_step_struct(_e, fixed, uint) gx_dda_step_fixed;
     typedef struct gx_dda_fixed_s {
         gx_dda_state_fixed state;
         gx_dda_step_fixed step;
     } gx_dda_fixed;
/*
 * Define a pair of DDAs for iterating along an arbitrary line.
 */
     typedef struct gx_dda_fixed_point_s {
         gx_dda_fixed x, y;
     } gx_dda_fixed_point;
/*
 * Initialize a DDA.  The sign test is needed only because C doesn't
 * provide reliable definitions of / and % for integers (!!!).
 */
#define dda_init_state(dstate, init, N_)\
  (dstate).Q = (init), (dstate).R = ((N_)-1)
#define dda_init_step(dstep, D, N_)\
  do {\
    if ( (N_) == 0 )\
      (dstep).dQ = 0, (dstep).dR = 0;\
    else if ( (D) < 0 )\
     { (dstep).dQ = -(int)((uint)-(D) / (N_));\
       if ( ((dstep).dR = -(D) % (N_)) != 0 )\
         --(dstep).dQ, (dstep).dR = (N_) - (dstep).dR;\
     }\
    else\
     { (dstep).dQ = (D) / (N_); (dstep).dR = (D) % (N_); }\
    (dstep).N = (N_);\
  } while (0)
#define dda_init(dda, init, D, N)\
  do {\
    dda_init_state((dda).state, init, N);\
    dda_init_step((dda).step, D, N);\
  } while (0)

static inline
int dda_will_overflow(gx_dda_fixed dda)
{
    /* Every step, R decrements by dR. If it becomes negative, we add 1 to Q, and add N to R. */
    /* So on average we add (N-dR)/N to Q each step. So in N steps we add (N-dR) to Q. */
    uint N = dda.step.N;
    fixed delta = dda.step.dQ * N + N - dda.step.dR;

    if (delta > 0 && delta + dda.state.Q < dda.state.Q)
            return 1;
    if (delta < 0 && delta + dda.state.Q > dda.state.Q)
            return 1;
    return 0;
}

/*
 * Initialise a DDA, and do a half step.
 */
#define dda_init_state_half(dstate, dstep, init, N_)\
  (dstate).Q = (init) + ((dstep).dQ>>1);\
  (dstate).R = ((N_)-1) - (((dstep).dR + ((dstep).dQ & 1 ? (dstep).N : 0))>>1);\
  if ((signed)(dstate).R < 0) {\
    (dstate).Q++;\
    (dstate).R += (dstep).N;\
  }
#define dda_init_half(dda, init, D, N)\
  do {\
    dda_init_step((dda).step, D, N);\
    dda_init_state_half((dda).state, (dda).step, init, N);\
  } while (0)

/*
 * Compute the sum of two DDA steps with the same D and N.
 * Note that since dR + NdR = N, this quantity must be the same in both
 * fromstep and tostep.
 */
#define dda_step_add(tostep, fromstep)\
    BEGIN\
        (tostep).dR += (fromstep).dR;\
        if ((tostep).dR >= (tostep).N) {\
            (tostep).dQ ++;\
            (tostep).dR -= (tostep).N;\
        }\
        (tostep).dQ += (fromstep).dQ;\
    END
/*
 * Return the current value in a DDA.
 */
#define dda_state_current(dstate) (dstate).Q
#define dda_current(dda) dda_state_current((dda).state)
#define dda_current_fixed2int(dda)\
  fixed2int_var(dda_state_current((dda).state))
/*
 * Increment a DDA to the next point.
 * Returns the updated current value.
 */
#define dda_state_next(dstate, dstep)\
    do {\
        (dstate).R -= (dstep).dR;\
        if ((signed)(dstate).R < 0) {\
            (dstate).Q++;\
            (dstate).R += (dstep).N;\
        };\
        (dstate).Q += (dstep).dQ;\
    } while (0)
#define dda_next(dda) dda_state_next((dda).state, (dda).step)
#define dda_next_assign(dda,v) BEGIN dda_next(dda);(v)=(dda).state.Q; END

/*
 * Back up a DDA to the previous point.
 * Returns the updated current value.
 */
#define dda_state_previous(dstate, dstep)\
    BEGIN\
        (dstate).R += (dstep).dR;\
        if ((dstate).R >= (dstep).N) {\
            (dstate).Q--;\
            (dstate).R -= (dstep).N;\
        }\
        (dstate).Q -= (dstep).dQ;\
    END
#define dda_previous(dda) dda_state_previous((dda).state, (dda).step)
#define dda_previous_assign(dda,v) BEGIN dda_previous(dda);(v)=(dda).state.Q;END
/*
 * Advance a DDA by an arbitrary number of steps.
 * This implementation is very inefficient; we'll improve it if needed.
 */
#define dda_state_advance(dstate, dstep, nsteps)\
  BEGIN\
      uint n_ = (nsteps);\
      (dstate).Q += (dstep).dQ * (nsteps);\
      while ( n_-- ) {\
          (dstate).R -= (dstep).dR;\
          if ((signed int)(dstate).R < 0) {\
              (dstate).Q ++;\
              (dstate).R += (dstep).N;\
          }\
      }\
  END
#define dda_advance(dda, nsteps)\
  dda_state_advance((dda).state, (dda).step, nsteps)
/*
 * Translate the position of a DDA by a given amount.
 */
#define dda_state_translate(dstate, delta)\
  ((dstate).Q += (delta))
#define dda_translate(dda, delta)\
  dda_state_translate((dda).state, delta)

#endif /* gxdda_INCLUDED */
