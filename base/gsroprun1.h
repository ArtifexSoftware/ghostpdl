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

/* We work in 'chunks' here; for bigendian machines, we can safely use
 * chunks of 'int' size. For little endian machines where we have a cheap
 * endian swap, we can do likewise. For others, we'll work at the byte
 * level. */
#if !ARCH_IS_BIG_ENDIAN && !defined(ENDIAN_SWAP_INT)
#define CHUNKSIZE 8
#define CHUNK byte
#define CHUNKONES 255

#define ADJUST_TO_CHUNK(d,dpos) do {} while (0)

#else /* ARCH_IS_BIG_ENDIAN || defined(ENDIAN_SWAP_INT) */
#if ARCH_LOG2_SIZEOF_INT == 2
#define CHUNKSIZE 32
#define CHUNK unsigned int
#define CHUNKONES 0xFFFFFFFFU

#if ARCH_SIZEOF_PTR == (1<<ARCH_LOG2_SIZEOF_INT)
#define ROP_PTRDIFF_T int
#else
#define ROP_PTRDIFF_T int64_t
#endif
#define ADJUST_TO_CHUNK(d, dpos)                      \
    do { int offset = ((ROP_PTRDIFF_T)d) & ((CHUNKSIZE>>3)-1);  \
         d = (CHUNK *)(void *)(((byte *)(void *)d)-offset);   \
         dpos += offset<<3;                           \
     } while (0)
#else
/* FIXME: Write more code in here when we find an example. */
#endif
#endif /* ARCH_IS_BIG_ENDIAN || defined(ENDIAN_SWAP_INT) */

/* We define an 'RE' macro that reverses the endianness of a chunk, if we
 * need it, and does nothing otherwise. */
#if !ARCH_IS_BIG_ENDIAN && defined(ENDIAN_SWAP_INT) && (CHUNKSIZE != 8)
#define RE(I) ((CHUNK)ENDIAN_SWAP_INT(I))
#else /* ARCH_IS_BIG_ENDIAN || !defined(ENDIAN_SWAP_INT) || (CHUNKSIZE == 8) */
#define RE(I) (I)
#endif /* ARCH_IS_BIG_ENDIAN || !defined(ENDIAN_SWAP_INT) || (CHUNKSIZE == 8) */

/* In some cases we will need to fetch values from a pointer, and 'skew'
 * them. We need 2 variants of this macro. One that is 'SAFE' to use when
 * SKEW might be 0, and one that can be faster, because we know that SKEW
 * is non zero. */
#define SKEW_FETCH(S,s,SKEW) \
    do { S = RE((RE(s[0])<<SKEW) | (RE(s[1])>>(CHUNKSIZE-SKEW))); s++; } while (0)
#define SAFE_SKEW_FETCH(S,s,SKEW,L,R)                                    \
    do { S = RE(((L) ? 0 : (RE(s[0])<<SKEW)) | ((R) ? 0 : (RE(s[1])>>(CHUNKSIZE-SKEW)))); s++; } while (0)

#if defined(S_USED) && !defined(S_CONST)
#define S_SKEW
#define FETCH_S           SKEW_FETCH(S,s,s_skew)
#define SAFE_FETCH_S(L,R) SAFE_SKEW_FETCH(S,s,s_skew,L,R)
#else /* !defined(S_USED) || defined(S_CONST) */
#define FETCH_S
#define SAFE_FETCH_S(L,R)
#endif /* !defined(S_USED) || defined(S_CONST) */

#if defined(T_USED) && !defined(T_CONST)
#define T_SKEW
#define FETCH_T           SKEW_FETCH(T,t,t_skew)
#define SAFE_FETCH_T(L,R) SAFE_SKEW_FETCH(T,t,t_skew,L,R)
#else /* !defined(T_USED) || defined(T_CONST) */
#define FETCH_T
#define SAFE_FETCH_T(L,R)
#endif /* !defined(T_USED) || defined(T_CONST) */

static void TEMPLATE_NAME(rop_run_op *op, byte *d_, int len)
{
#ifndef SPECIFIC_CODE
    rop_proc     proc = rop_proc_table[op->rop];
#define SPECIFIC_CODE(OUT_, D_,S_,T_) OUT_ = proc(D_,S_,T_)
#endif /* !defined(SPECIFIC_CODE) */
    CHUNK        lmask, rmask;
#ifdef S_USED
#ifdef S_CONST
    CHUNK        S = (CHUNK)op->s.c;
#else /* !defined(S_CONST) */
    const CHUNK *s = (CHUNK *)(void *)op->s.b.ptr;
    CHUNK        S;
    int          s_skew;
#endif /* !defined(S_CONST) */
#else /* !defined(S_USED) */
#define S 0
#undef S_CONST
#endif /* !defined(S_USED) */
#ifdef T_USED
#ifdef T_CONST
    CHUNK        T = (CHUNK)op->t.c;
#else /* !defined(T_CONST) */
    const CHUNK *t = (CHUNK *)(void *)op->t.b.ptr;
    CHUNK        T;
    int          t_skew;
#endif /* !defined(T_CONST) */
#else /* !defined(T_USED) */
#define T 0
#undef T_CONST
#endif /* !defined(T_USED) */
#if defined(S_SKEW) || defined(T_SKEW)
    int skewflags = 0;
#endif
    CHUNK        D;
    int          dpos = op->dpos;
    CHUNK       *d = (CHUNK *)(void *)d_;

    /* Align d to CHUNKSIZE */
    ADJUST_TO_CHUNK(d,dpos);

    /* On entry len = length in 'depth' chunks. Change it to be the length
     * in bits, and add on the number of bits we skip at the start of the
     * run. */
    len    = len * op->depth + dpos;

    /* lmask = the set of bits to alter in the output bitmap on the left
     * hand edge of the run. rmask = the set of bits NOT to alter in the
     * output bitmap on the right hand edge of the run. */
    lmask  = RE((CHUNKONES>>((CHUNKSIZE-1) & dpos)));
    rmask  = RE((CHUNKONES>>((CHUNKSIZE-1) & len)));
    if (rmask == CHUNKONES) rmask = 0;

#if defined(S_CONST) || defined(T_CONST)
    /* S and T should be supplied as 'depth' bits. Duplicate them up to be
     * byte size (if they are supplied byte sized, that's fine too). */
    if (op->depth & 1) {
#ifdef S_CONST
        S |= S<<1;
#endif /* !defined(S_CONST) */
#ifdef T_CONST
        T |= T<<1;
#endif /* !defined(T_CONST) */
    }
    if (op->depth & 3) {
#ifdef S_CONST
        S |= S<<2;
#endif /* !defined(S_CONST) */
#ifdef T_CONST
        T |= T<<2;
#endif /* !defined(T_CONST) */
    }
    if (op->depth & 7) {
#ifdef S_CONST
        S |= S<<4;
#endif /* !defined(S_CONST) */
#ifdef T_CONST
        T |= T<<4;
#endif /* !defined(T_CONST) */
    }
#if CHUNKSIZE > 8
    if (op->depth & 15) {
#ifdef S_CONST
        S |= S<<8;
#endif /* !defined(S_CONST) */
#ifdef T_CONST
        T |= T<<8;
#endif /* !defined(T_CONST) */
    }
#endif /* CHUNKSIZE > 8 */
#if CHUNKSIZE > 16
    if (op->depth & 31) {
#ifdef S_CONST
        S |= S<<16;
#endif /* !defined(S_CONST) */
#ifdef T_CONST
        T |= T<<16;
#endif /* !defined(T_CONST) */
    }
#endif /* CHUNKSIZE > 16 */
#endif /* defined(S_CONST) || defined(T_CONST) */

    /* Note #1: This mirrors what the original code did, but I think it has
     * the risk of moving s and t back beyond officially allocated space. We
     * may be saved by the fact that all blocks have a word or two in front
     * of them due to the allocator. If we ever get valgrind properly marking
     * allocated blocks as readable etc, then this may throw some spurious
     * errors. RJW. */
#ifdef S_SKEW
    {
        int slen, slen2;
        int spos = op->s.b.pos;
        ADJUST_TO_CHUNK(s, spos);
        s_skew = spos - dpos;
        if (s_skew < 0) {
            s_skew += CHUNKSIZE;
            s--;
            skewflags |= 1; /* Suppress reading off left edge */
        }
        /* We are allowed to read all the data bits, so: len - dpos + tpos
         * We're allowed to read in CHUNKS, so: CHUNKUP(len-dpos+tpos).
         * This code will actually read CHUNKUP(len)+CHUNKSIZE bits. If
         * This is larger, then suppress. */
        slen  = (len + s_skew    + CHUNKSIZE-1) & ~(CHUNKSIZE-1);
        slen2 = (len + CHUNKSIZE + CHUNKSIZE-1) & ~(CHUNKSIZE-1);
        if ((s_skew == 0) || (slen < slen2)) {
            skewflags |= 4; /* Suppress reading off the right edge */
        }
    }
#endif /* !defined(S_SKEW) */
#ifdef T_SKEW
    {
        int tlen, tlen2;
        int tpos = op->t.b.pos;
        ADJUST_TO_CHUNK(t, tpos);
        t_skew = tpos - dpos;
        if (t_skew < 0) {
            t_skew += CHUNKSIZE;
            t--;
            skewflags |= 2; /* Suppress reading off left edge */
        }
        /* We are allowed to read all the data bits, so: len - dpos + tpos
         * We're allowed to read in CHUNKS, so: CHUNKUP(len-dpos+tpos).
         * This code will actually read CHUNKUP(len)+CHUNKSIZE bits. If
         * This is larger, then suppress. */
        tlen  = (len + t_skew    + CHUNKSIZE-1) & ~(CHUNKSIZE-1);
        tlen2 = (len + CHUNKSIZE + CHUNKSIZE-1) & ~(CHUNKSIZE-1);
        if ((t_skew == 0) || (tlen < tlen2)) {
            skewflags |= 8; /* Suppress reading off the right edge */
        }
    }
#endif /* !defined(T_SKEW) */

    len -= CHUNKSIZE; /* len = bytes to do - CHUNKSIZE */
    /* len <= 0 means 1 word or less to do */
    if (len <= 0) {
        /* Short case - starts and ends in the same chunk */
        lmask &= ~rmask; /* Combined mask = bits to alter */
        SAFE_FETCH_S(skewflags & 1,skewflags & 4);
        SAFE_FETCH_T(skewflags & 2,skewflags & 8);
        SPECIFIC_CODE(D, *d, S, T);
        *d = (*d & ~lmask) | (D & lmask);
        return;
    }
    if ((lmask != CHUNKONES)
#if defined(S_SKEW) || defined(T_SKEW)
        || (skewflags & 3)
#endif
        ) {
        /* Unaligned left hand case */
        SAFE_FETCH_S(skewflags & 1,s_skew == 0);
        SAFE_FETCH_T(skewflags & 2,t_skew == 0);
        SPECIFIC_CODE(D, *d, S, T);
        *d = (*d & ~lmask) | (D & lmask);
        d++;
        len -= CHUNKSIZE;
    }
    if (len > 0) {
        /* Simple middle case (complete destination chunks). */
#ifdef S_SKEW
        if (s_skew == 0) {
#ifdef T_SKEW
            if (t_skew == 0) {
                do {
                    SPECIFIC_CODE(*d, *d, *s++, *t++);
                    d++;
                    len -= CHUNKSIZE;
                } while (len > 0);
            } else
#endif /* !defined(T_SKEW) */
            {
                do {
                    FETCH_T;
                    SPECIFIC_CODE(*d, *d, *s++, T);
                    d++;
                    len -= CHUNKSIZE;
                } while (len > 0);
            }
        } else
#endif /* !defined(S_SKEW) */
        {
#ifdef T_SKEW
            if (t_skew == 0) {
                do {
                    FETCH_S;
                    SPECIFIC_CODE(*d, *d, S, *t++);
                    d++;
                    len -= CHUNKSIZE;
                } while (len > 0);
            } else
#endif /* !defined(T_SKEW) */
            {
                do {
                    FETCH_S;
                    FETCH_T;
                    SPECIFIC_CODE(*d, *d, S, T);
                    d++;
                    len -= CHUNKSIZE;
                } while (len > 0);
            }
        }
    }
    /* Unaligned right hand case */
    SAFE_FETCH_S(0,skewflags & 4);
    SAFE_FETCH_T(0,skewflags & 8);
    SPECIFIC_CODE(D, *d, S, T);
    *d = (*d & rmask) | (D & ~rmask);
}

#undef ADJUST_TO_CHUNK
#undef CHUNKSIZE
#undef CHUNK
#undef CHUNKONES
#undef FETCH_S
#undef FETCH_T
#undef SAFE_FETCH_S
#undef SAFE_FETCH_T
#undef RE
#undef S
#undef S_USED
#undef S_CONST
#undef S_SKEW
#undef SKEW_FETCH
#undef SAFE_SKEW_FETCH
#undef SPECIFIC_CODE
#undef SPECIFIC_ROP
#undef T
#undef T_USED
#undef T_CONST
#undef T_SKEW
#undef TEMPLATE_NAME
#undef ROP_PTRDIFF_T

#else
int dummy;
#endif
