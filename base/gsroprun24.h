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


/* This file is repeatedly included by gsroprun.c to 'autogenerate' many
 * different versions of roprun code. DO NOT USE THIS FILE EXCEPT FROM
 * gsroprun.c.
 */

/* Set the following defines as appropriate on entry:
 *   TEMPLATE_NAME (Compulsory)  The name of the function to generate
 *   SPECIFIC_ROP  (Optional)    If set, the function will base its decision
 *                               about whether to provide S and T upon
 *                               this value.
 *   SPECIFIC_CODE (Optional)    If set, this should expand out to code to
 *                               perform the rop. Will be invoked as:
 *                               SPECIFIC_ROP(OUT,D,S,T)
 *   S_CONST       (Optional)    If set, S will be taken to be constant, else
 *                               S will be read from a pointer.
 *   T_CONST       (Optional)    If set, T will be taken to be constant, else
 *                               T will be read from a pointer.
 *   S_1BIT        (Optional)    If set, the code will cope with S maybe
 *                               being a pointer to a 1 bit bitmap to choose
 *                               between scolors[0] and [1]. If set to 1, the
 *                               code will assume that this is the case.
 *   T_1BIT        (Optional)    If set, the code will cope with T maybe
 *                               being a pointer to a 1 bit bitmap to choose
 *                               between scolors[0] and [1]. If set to 1, the
 *                               code will assume that this is the case.
 */

#if defined(TEMPLATE_NAME)

#ifdef SPECIFIC_ROP
#if rop3_uses_S(SPECIFIC_ROP)
#define S_USED
#endif
#if rop3_uses_T(SPECIFIC_ROP)
#define T_USED
#endif
#else /* !SPECIFIC_ROP */
#define S_USED
#define T_USED
#endif /* SPECIFIC_ROP */

#ifndef S_USED
#undef S_1BIT
#endif /* defined(S_USED) */
#ifndef T_USED
#undef T_1BIT
#endif /* defined(T_USED) */

#define GET24(ptr)\
  (((rop_operand)(ptr)[0] << 16) | ((rop_operand)(ptr)[1] << 8) | (ptr)[2])
#define PUT24(ptr, pixel)\
  (ptr)[0] = (byte)((pixel) >> 16),\
  (ptr)[1] = (byte)((uint)(pixel) >> 8),\
  (ptr)[2] = (byte)(pixel)

#if defined(S_USED) && !defined(S_CONST)
#define FETCH_S      do { S = GET24(s); s += 3; } while (0==1)
#else /* !defined(S_USED) || defined(S_CONST) */
#define FETCH_S
#endif /* !defined(S_USED) || defined(S_CONST) */

#if defined(T_USED) && !defined(T_CONST)
#define FETCH_T      do { T = GET24(t); t += 3; } while (0 == 1)
#else /* !defined(T_USED) || defined(T_CONST) */
#define FETCH_T
#endif /* !defined(T_USED) || defined(T_CONST) */

static void TEMPLATE_NAME(rop_run_op *op, byte *d, int len)
{
#ifndef SPECIFIC_CODE
    rop_proc     proc = rop_proc_table[lop_rop(op->rop)];
#define SPECIFIC_CODE(OUT_, D_,S_,T_) OUT_ = proc(D_,S_,T_)
#endif /* !defined(SPECIFIC_CODE) */
#ifdef S_USED
#ifdef S_CONST
    rop_operand  S = op->s.c;
#else /* !defined(S_CONST) */
    const byte  *s = op->s.b.ptr;
#endif /* !defined(S_CONST) */
#ifdef S_1BIT
    int          sroll;
    rop_operand  sc[2];
#endif /* S_1BIT */
#else /* !defined(S_USED) */
#define S 0
#undef S_CONST
#endif /* !defined(S_USED) */
#ifdef T_USED
#ifdef T_CONST
    rop_operand  T = op->t.c;
#else /* !defined(T_CONST) */
    const byte  *t = op->t.b.ptr;
#endif /* !defined(T_CONST) */
#ifdef T_1BIT
    int          troll;
    rop_operand  tc[2];
#endif /* T_1BIT */
#else /* !defined(T_USED) */
#define T 0
#undef T_CONST
#endif /* !defined(T_USED) */

#ifdef S_1BIT
#if S_1BIT == MAYBE
    if (op->flags & rop_s_1bit) {
#endif /* S_1BIT == MAYBE */
        s += op->s.b.pos>>3;
        sroll = 8-(op->s.b.pos & 7);
        sc[0] = ((const gx_color_index *)op->scolors)[0];
        sc[1] = ((const gx_color_index *)op->scolors)[1];
#if S_1BIT == MAYBE
    } else
        sroll = 0;
#endif /* S_1BIT == MAYBE */
#endif /* defined(S_1BIT) */
#ifdef T_1BIT
#if T_1BIT == MAYBE
    if (op->flags & rop_t_1bit) {
#endif /* T_1BIT == MAYBE */
        t += op->t.b.pos>>3;
        troll = 8-(op->t.b.pos & 7);
        tc[0] = ((const gx_color_index *)op->tcolors)[0];
        tc[1] = ((const gx_color_index *)op->tcolors)[1];
#if T_1BIT == MAYBE
    } else
        troll = 0;
#endif /* T_1BIT == MAYBE */
#endif /* defined(T_1BIT) */
    do {
#if defined(S_USED) && !defined(S_CONST)
        rop_operand S;
#endif /* defined(S_USED) && !defined(S_CONST) */
#if defined(T_USED) && !defined(T_CONST)
        rop_operand T;
#endif /* defined(T_USED) && !defined(T_CONST) */
#if defined(S_1BIT) && S_1BIT == MAYBE
        if (sroll == 0) {
#endif /* defined(S_1BIT) && S_1BIT == MAYBE */
#if !defined(S_1BIT) || S_1BIT == MAYBE
            FETCH_S;
#endif /* !defined(S_1BIT) || S_1BIT == MAYBE */
#if defined(S_1BIT) && S_1BIT == MAYBE
        } else
#endif /* defined(S_1BIT) && S_1BIT == MAYBE */
        {
#ifdef S_1BIT
            --sroll;
            S = sc[(*s >> sroll) & 1];
            if (sroll == 0) {
                sroll = 8;
                s++;
            }
#endif /* S_1BIT */
        }
#if defined(T_1BIT) && T_1BIT == MAYBE
        if (troll == 0) {
#endif /* defined(T_1BIT) && T_1BIT == MAYBE */
#if !defined(T_1BIT) || T_1BIT == MAYBE
            FETCH_T;
#endif /* defined(T_1BIT) && T_1BIT == MAYBE */
#if defined(T_1BIT) && T_1BIT == MAYBE
        } else
#endif /* defined(T_1BIT) && T_1BIT == MAYBE */
        {
#ifdef T_1BIT
            --troll;
            T = tc[(*t >> troll) & 1];
            if (troll == 0) {
                troll = 8;
                t++;
            }
#endif /* T_1BIT */
        }
        {
            rop_operand D;
            SPECIFIC_CODE(D, GET24(d), S, T);
            PUT24(d, D);
        }
        d += 3;
    }
    while (--len);
}

#undef GET24
#undef PUT24
#undef FETCH_S
#undef FETCH_T
#undef S
#undef S_1BIT
#undef S_USED
#undef S_CONST
#undef SPECIFIC_CODE
#undef SPECIFIC_ROP
#undef T
#undef T_1BIT
#undef T_USED
#undef T_CONST
#undef TEMPLATE_NAME

#else
int dummy;
#endif
