/* Copyright (C) 2001-2019 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
*/


/* Runs of RasterOps */
#include "std.h"
#include "stdpre.h"
#include "gsropt.h"
#include "gp.h"
#include "gxcindex.h"

/* Enable the following define to use 'template'd code (code formed by
 * repeated #inclusion of a header file to generate differen versions).
 * This code should be faster as it uses native ints where possible.
 * Eventually this should be the only type of code left in here and we can
 * remove this define. */
#define USE_TEMPLATES

/* Enable the following define to disable all rops (at least, all the ones
 * done using the rop run mechanism. For debugging only. */
#undef DISABLE_ROPS

/* A hack. Define this, and we will update the rop usage within a file. */
#undef RECORD_ROP_USAGE

/* Values used for defines used when including 'template' headers. */
#define MAYBE 0
#define YES   1

#ifdef RECORD_ROP_USAGE
#define MAX (1024<<7)
static int usage[MAX*3];

static int inited = 0;

static void write_usage(void)
{
    int i;
    for (i = 0; i < MAX; i++)
        if (usage[3*i] != 0) {
            int depth = ((i>>4)&3)<<3;
            if (depth == 0) depth = 1;
            if (i & (1<<6))
               (fprintf)(stderr, "ROP: rop=%x ", i>>7);
            else
               (fprintf)(stderr, "ROP: rop=ANY ");
           (fprintf)(stderr, "depth=%d flags=%d gets=%d inits=%d pixels=%d\n",
                     depth, i&15, usage[3*i], usage[3*i+1], usage[3*i+2]);
        }
#ifdef RECORD_BINARY
    {
    FILE *out = fopen("ropusage2.tmp", "wb");
    if (!out)
        return;
    fwrite(usage, sizeof(int), (1024<<7), out);
    fclose(out);
    }
#endif
}

static void record(int rop)
{
    if (inited == 0) {
#ifdef RECORD_BINARY
        FILE *in = fopen("ropusage2.tmp", "r");
        if (!in)
            memset(usage, 0, MAX*sizeof(int));
        else {
            fread(usage, sizeof(int), MAX, in);
            fclose(in);
        }
#endif
        memset(usage, 0, MAX*3*sizeof(int));
        atexit(write_usage);
        inited = 1;
    }

    usage[3*rop]++;
}
#endif

#define get24(ptr)\
  (((rop_operand)(ptr)[0] << 16) | ((rop_operand)(ptr)[1] << 8) | (ptr)[2])
#define put24(ptr, pixel)\
  (ptr)[0] = (byte)((pixel) >> 16),\
  (ptr)[1] = (byte)((uint)(pixel) >> 8),\
  (ptr)[2] = (byte)(pixel)

/* Rop specific code */
/* Rop 0x55 = Invert   dep=1  (all cases) */
#ifdef USE_TEMPLATES
#define TEMPLATE_NAME          invert_rop_run1
#define SPECIFIC_ROP           0x55
#define SPECIFIC_CODE(O,D,S,T) do { O = ~D; } while (0)
#define MM_SETUP() __m128i mm_constant_ones = _mm_cmpeq_epi32(mm_constant_ones, mm_constant_ones);
#define MM_SPECIFIC_CODE(O,D,S,T) do { _mm_storeu_si128(O,_mm_xor_si128(_mm_loadu_si128(D),mm_constant_ones)); } while (0 == 1)
#define S_CONST
#define T_CONST
#include "gsroprun1.h"
#else
static void invert_rop_run1(rop_run_op *op, byte *d, int len)
{
    byte lmask, rmask;

    len    = len * op->depth + op->dpos;
    /* lmask = the set of bits to alter in the output bitmap on the left
     * hand edge of the run. rmask = the set of bits NOT to alter in the
     * output bitmap on the right hand edge of the run. */
    lmask  = 255>>(7 & op->dpos);
    rmask  = 255>>(7 & len);

    len -= 8;
    if (len < 0) {
        /* Short case - starts and ends in the same byte */
        lmask &= ~rmask; /* Combined mask = bits to alter */
        *d = (*d & ~lmask) | ((~*d) & lmask);
        return;
    }
    if (lmask != 0xFF) {
        *d = (*d & ~lmask) | ((~*d) & lmask);
        d++;
        len -= 8;
    }
    if (len >= 0) {
        /* Simple middle case (complete destination bytes). */
        do {
            *d = ~*d;
            d++;
            len -= 8;
        } while (len >= 0);
    }
    if (rmask != 0xFF) {
        /* Unaligned right hand case */
        *d = ((~*d) & ~rmask) | (*d & rmask);
    }
}
#endif

/* Rop 0x55 = Invert   dep=8  (all cases) */
#ifdef USE_TEMPLATES
#define TEMPLATE_NAME          invert_rop_run8
#define SPECIFIC_ROP           0x55
#define SPECIFIC_CODE(O,D,S,T) do { O = ~D; } while (0)
#define MM_SETUP() __m128i mm_constant_ones = _mm_cmpeq_epi32(mm_constant_ones, mm_constant_ones);
#define MM_SPECIFIC_CODE(O,D,S,T) do { _mm_storeu_si128(O,_mm_xor_si128(_mm_loadu_si128(D),mm_constant_ones)); } while (0 == 1)
#define S_CONST
#define T_CONST
#include "gsroprun8.h"
#else
static void invert_rop_run8(rop_run_op *op, byte *d, int len)
{
    do {
        *d = ~*d;
        d++;
    }
    while (--len);
}
#endif

/* Rop 0x33 = ~s */

/* 0x33 = ~s  dep=1 t_constant */
#ifdef USE_TEMPLATES
#define TEMPLATE_NAME          notS_rop_run1_const_t
#define SPECIFIC_ROP           0x33
#define SPECIFIC_CODE(O,D,S,T) do { O = ~S; } while (0)
#define T_CONST
#include "gsroprun1.h"
#else
static void notS_rop_run1_const_s(rop_run_op *op, byte *d, int len)
{
    byte        lmask, rmask;
    const byte *s = op->s.b.ptr;
    byte        S;
    int         s_skew;

    len    = len * op->depth + op->dpos;
    /* lmask = the set of bits to alter in the output bitmap on the left
     * hand edge of the run. rmask = the set of bits NOT to alter in the
     * output bitmap on the right hand edge of the run. */
    lmask  = 255>>(7 & op->dpos);
    rmask  = 255>>(7 & len);

    /* Note #1: This mirrors what the original code did, but I think it has
     * the risk of moving s and t back beyond officially allocated space. We
     * may be saved by the fact that all blocks have a word or two in front
     * of them due to the allocator. If we ever get valgrind properly marking
     * allocated blocks as readable etc, then this may throw some spurious
     * errors. RJW. */
    s_skew = op->t.b.pos - op->dpos;
    if (s_skew < 0) {
        s_skew += 8;
        s--;
    }

    len -= 8;
    if (len < 0) {
        /* Short case - starts and ends in the same byte */
        lmask &= ~rmask; /* Combined mask = bits to alter */
        S = (s[0]<<s_skew) | (s[1]>>(8-s_skew));
        *d = (*d & ~lmask) | (~S & lmask);
        return;
    }
    if (lmask != 0xFF) {
        /* Unaligned left hand case */
        S = (s[0]<<s_skew) | (s[1]>>(8-s_skew));
        s++;
        *d = (*d & ~lmask) | (~S & lmask);
        d++;
        len -= 8;
    }
    if (len >= 0) {
        /* Simple middle case (complete destination bytes). */
        if (s_skew == 0) {
            do {
                *d++ = ~*s++;
                len -= 8;
            } while (len >= 0);
        } else {
            do {
                S = (s[0]<<s_skew) | (s[1]>>(8-s_skew));
                s++;
                *d++ = ~S;
                len -= 8;
            } while (len >= 0);
        }
    }
    if (rmask != 0xFF) {
        /* Unaligned right hand case */
        S = (s[0]<<s_skew) | (s[1]>>(8-s_skew));
        *d = (~S & ~rmask) | (*d & rmask);
    }
}
#endif

/* Rop 0xEE = d|s */

/* 0xEE = d|s  dep=1 t_constant */
#ifdef USE_TEMPLATES
#define TEMPLATE_NAME          dors_rop_run1_const_t
#define SPECIFIC_ROP           0xEE
#define SPECIFIC_CODE(O,D,S,T) do { O = D|S; } while (0)
#define T_CONST
#include "gsroprun1.h"
#else
static void dors_rop_run1_const_t(rop_run_op *op, byte *d, int len)
{
    byte        lmask, rmask;
    const byte *s = op->s.b.ptr;
    byte        S, D;
    int         s_skew;

    len    = len * op->depth + op->dpos;
    /* lmask = the set of bits to alter in the output bitmap on the left
     * hand edge of the run. rmask = the set of bits NOT to alter in the
     * output bitmap on the right hand edge of the run. */
    lmask  = 255>>(7 & op->dpos);
    rmask  = 255>>(7 & len);

    /* Note #1: This mirrors what the original code did, but I think it has
     * the risk of moving s and t back beyond officially allocated space. We
     * may be saved by the fact that all blocks have a word or two in front
     * of them due to the allocator. If we ever get valgrind properly marking
     * allocated blocks as readable etc, then this may throw some spurious
     * errors. RJW. */
    s_skew = op->s.b.pos - op->dpos;
    if (s_skew < 0) {
        s_skew += 8;
        s--;
    }

    len -= 8;
    if (len < 0) {
        /* Short case - starts and ends in the same byte */
        lmask &= ~rmask; /* Combined mask = bits to alter */
        S = (s[0]<<s_skew) | (s[1]>>(8-s_skew));
        D = *d | S;
        *d = (*d & ~lmask) | (D & lmask);
        return;
    }
    if (lmask != 0xFF) {
        /* Unaligned left hand case */
        S = (s[0]<<s_skew) | (s[1]>>(8-s_skew));
        s++;
        D = *d | S;
        *d = (*d & ~lmask) | (D & lmask);
        d++;
        len -= 8;
    }
    if (len >= 0) {
        /* Simple middle case (complete destination bytes). */
        if (s_skew == 0) {
            do {
                *d++ |= *s++;
                len -= 8;
            } while (len >= 0);
        } else {
            do {
                S = (s[0]<<s_skew) | (s[1]>>(8-s_skew));
                s++;
                *d |= S;
                d++;
                len -= 8;
            } while (len >= 0);
        }
    }
    if (rmask != 0xFF) {
        /* Unaligned right hand case */
        S = (s[0]<<s_skew) | (s[1]>>(8-s_skew));
        D = *d | S;
        *d = (D & ~rmask) | (*d & rmask);
    }
}
#endif

/* Rop 0x66 = d^s (and 0x5A = d^t) */

/* 0x66 = d^s  dep=1 t_constant */
#ifdef USE_TEMPLATES
#define TEMPLATE_NAME          xor_rop_run1_const_t
#define SPECIFIC_ROP           0x66
#define SPECIFIC_CODE(O,D,S,T) do { O = D^S; } while (0)
#define T_CONST
#include "gsroprun1.h"
#else
static void xor_rop_run1_const_t(rop_run_op *op, byte *d, int len)
{
    byte        lmask, rmask;
    const byte *s = op->s.b.ptr;
    byte        S, D;
    int         s_skew;

    len    = len * op->depth + op->dpos;
    /* lmask = the set of bits to alter in the output bitmap on the left
     * hand edge of the run. rmask = the set of bits NOT to alter in the
     * output bitmap on the right hand edge of the run. */
    lmask  = 255>>(7 & op->dpos);
    rmask  = 255>>(7 & len);

    /* Note #1: This mirrors what the original code did, but I think it has
     * the risk of moving s and t back beyond officially allocated space. We
     * may be saved by the fact that all blocks have a word or two in front
     * of them due to the allocator. If we ever get valgrind properly marking
     * allocated blocks as readable etc, then this may throw some spurious
     * errors. RJW. */
    s_skew = op->s.b.pos - op->dpos;
    if (s_skew < 0) {
        s_skew += 8;
        s--;
    }

    len -= 8;
    if (len < 0) {
        /* Short case - starts and ends in the same byte */
        lmask &= ~rmask; /* Combined mask = bits to alter */
        S = (s[0]<<s_skew) | (s[1]>>(8-s_skew));
        D = *d ^ S;
        *d = (*d & ~lmask) | (D & lmask);
        return;
    }
    if (lmask != 0xFF) {
        /* Unaligned left hand case */
        S = (s[0]<<s_skew) | (s[1]>>(8-s_skew));
        s++;
        D = *d ^ S;
        *d = (*d & ~lmask) | (D & lmask);
        d++;
        len -= 8;
    }
    if (len >= 0) {
        /* Simple middle case (complete destination bytes). */
        if (s_skew == 0) {
            do {
                *d++ ^= *s++;
                len -= 8;
            } while (len >= 0);
        } else {
            do {
                S = (s[0]<<s_skew) | (s[1]>>(8-s_skew));
                s++;
                *d = *d ^ S;
                d++;
                len -= 8;
            } while (len >= 0);
        }
    }
    if (rmask != 0xFF) {
        /* Unaligned right hand case */
        S = (s[0]<<s_skew) | (s[1]>>(8-s_skew));
        D = *d ^ S;
        *d = (D & ~rmask) | (*d & rmask);
    }
}
#endif

/* rop = 0x66 = d^s  dep=8  s_constant t_constant */
#ifdef USE_TEMPLATES
#define TEMPLATE_NAME          xor_rop_run8_const_st
#define SPECIFIC_ROP           0x66
#define SPECIFIC_CODE(O,D,S,T) do { O = D^S; } while (0)
#define MM_SPECIFIC_CODE(O,D,S,T) do { _mm_storeu_si128(O,_mm_xor_si128(_mm_loadu_si128(D),S)); } while (0 == 1)
#define S_CONST
#define T_CONST
#include "gsroprun8.h"
#else
static void xor_rop_run8_const_st(rop_run_op *op, byte *d, int len)
{
    const byte S = op->s.c;
    if (S == 0)
        return;
    do {
        *d ^= S;
        d++;
    }
    while (--len);
}
#endif

/* rop = 0x66 = d^s  dep=24  s_constant t_constant */
#ifdef USE_TEMPLATES
#define TEMPLATE_NAME          xor_rop_run24_const_st
#define SPECIFIC_ROP           0x66
#define SPECIFIC_CODE(O,D,S,T) do { O = D^S; } while (0)
#define S_CONST
#define T_CONST
#include "gsroprun24.h"
#else
static void xor_rop_run24_const_st(rop_run_op *op, byte *d, int len)
{
    rop_operand S = op->s.c;
    if (S == 0)
        return;
    do
    {
        rop_operand D = get24(d) ^ S;
        put24(d, D);
        d += 3;
    }
    while (--len);
}
#endif

/* rop = 0xAA = d  dep=?  s_constant t_constant */
static void nop_rop_const_st(rop_run_op *op, byte *d, int len)
{
}

/* rop = 0xCC = s dep=1 t_constant */
#ifdef USE_TEMPLATES
#define TEMPLATE_NAME          sets_rop_run1
#define SPECIFIC_ROP           0xCC
#define SPECIFIC_CODE(O,D,S,T) do { O = S; } while (0)
#define T_CONST
#include "gsroprun1.h"
#else
static void sets_rop_run1(rop_run_op *op, byte *d, int len)
{
    rop_proc    proc = rop_proc_table[op->rop];
    byte        lmask, rmask;
    byte        S, D;
    const byte *s = op->s.b.ptr;
    int         s_skew;

    len    = len*op->depth + op->dpos;
    /* lmask = the set of bits to alter in the output bitmap on the left
     * hand edge of the run. rmask = the set of bits NOT to alter in the
     * output bitmap on the right hand edge of the run. */
    lmask  = 255>>(7 & op->dpos);
    rmask  = 255>>(7 & len);

    /* See note #1 above. RJW. */
    s_skew = op->s.b.pos - op->dpos;
    if (s_skew < 0) {
        s_skew += 8;
        s--;
    }

    len -= 8;
    if (len < 0) {
        /* Short case - starts and ends in the same byte */
        lmask &= ~rmask; /* Combined mask = bits to alter */
        S = (s[0]<<s_skew) | (s[1]>>(8-s_skew));
        D = proc(*d, S, 0);
        *d = (*d & ~lmask) | (D & lmask);
        return;
    }
    if (lmask != 0xFF) {
        /* Unaligned left hand case */
        S = (s[0]<<s_skew) | (s[1]>>(8-s_skew));
        s++;
        D = proc(*d, S, 0);
        *d = (*d & ~lmask) | (D & lmask);
        d++;
        len -= 8;
    }
    if (len >= 0) {
        /* Simple middle case (complete destination bytes). */
        if (s_skew == 0) {
            do {
                *d = proc(*d, *s++, 0);
                d++;
                len -= 8;
            } while (len >= 0);
        } else {
            do {
                S = (s[0]<<s_skew) | (s[1]>>(8-s_skew));
                s++;
                *d = proc(*d, S, 0);
                d++;
                len -= 8;
            } while (len >= 0);
        }
    }
    if (rmask != 0xFF) {
        /* Unaligned right hand case */
        S = (s[0]<<s_skew) | (s[1]>>(8-s_skew));
        D = proc(*d, S, 0);
        *d = (D & ~rmask) | (*d & rmask);
    }
}
#endif

/* rop = 0xCC = s dep=8 s_constant | t_constant */
#ifdef USE_TEMPLATES
#define TEMPLATE_NAME          sets_rop_run8
#define SPECIFIC_ROP           0xCC
#define SPECIFIC_CODE(O,D,S,T) do { O = S; } while (0)
#define MM_SPECIFIC_CODE(O,D,S,T) do { _mm_storeu_si128(O,S); } while (0 == 1)
#define S_CONST
#define T_CONST
#include "gsroprun8.h"
#else
static void sets_rop_run8(rop_run_op *op, byte *d, int len)
{
    const byte S = op->s.c;
    do {
        *d++ = S;
    }
    while (--len);
}
#endif

/* rop = 0xCC = s dep=24 s_constant | t_constant */
#ifdef USE_TEMPLATES
#define TEMPLATE_NAME          sets_rop_run24
#define SPECIFIC_ROP           0xCC
#define SPECIFIC_CODE(O,D,S,T) do { O = S; } while (0)
#define S_CONST
#define T_CONST
#include "gsroprun24.h"
#else
static void copys_rop_run24(rop_run_op *op, byte *d, int len)
{
    rop_operand S = op->s.c;
    {
        put24(d, S);
        d += 3;
    }
    while (--len);
}
#endif

/* Generic ROP run code */
#ifdef USE_TEMPLATES
#define TEMPLATE_NAME          generic_rop_run1
#include "gsroprun1.h"
#else
static void generic_rop_run1(rop_run_op *op, byte *d, int len)
{
    rop_proc    proc = rop_proc_table[op->rop];
    byte        lmask, rmask;
    const byte *s = op->s.b.ptr;
    const byte *t = op->t.b.ptr;
    byte        S, T, D;
    int         s_skew, t_skew;

    len    = len * op->depth + op->dpos;
    /* lmask = the set of bits to alter in the output bitmap on the left
     * hand edge of the run. rmask = the set of bits NOT to alter in the
     * output bitmap on the right hand edge of the run. */
    lmask  = 255>>(7 & op->dpos);
    rmask  = 255>>(7 & len);

    /* Note #1: This mirrors what the original code did, but I think it has
     * the risk of moving s and t back beyond officially allocated space. We
     * may be saved by the fact that all blocks have a word or two in front
     * of them due to the allocator. If we ever get valgrind properly marking
     * allocated blocks as readable etc, then this may throw some spurious
     * errors. RJW. */
    s_skew = op->s.b.pos - op->dpos;
    if (s_skew < 0) {
        s_skew += 8;
        s--;
    }
    t_skew = op->t.b.pos - op->dpos;
    if (t_skew < 0) {
        t_skew += 8;
        t--;
    }

    len -= 8;
    if (len < 0) {
        /* Short case - starts and ends in the same byte */
        lmask &= ~rmask; /* Combined mask = bits to alter */
        S = (s[0]<<s_skew) | (s[1]>>(8-s_skew));
        T = (t[0]<<t_skew) | (t[1]>>(8-t_skew));
        D = proc(*d, S, T);
        *d = (*d & ~lmask) | (D & lmask);
        return;
    }
    if (lmask != 0xFF) {
        /* Unaligned left hand case */
        S = (s[0]<<s_skew) | (s[1]>>(8-s_skew));
        s++;
        T = (t[0]<<t_skew) | (t[1]>>(8-t_skew));
        t++;
        D = proc(*d, S, T);
        *d = (*d & ~lmask) | (D & lmask);
        d++;
        len -= 8;
    }
    if (len >= 0) {
        /* Simple middle case (complete destination bytes). */
        if (s_skew == 0) {
            if (t_skew == 0) {
                do {
                    *d = proc(*d, *s++, *t++);
                    d++;
                    len -= 8;
                } while (len >= 0);
            } else {
                do {
                    T = (t[0]<<t_skew) | (t[1]>>(8-t_skew));
                    t++;
                    *d = proc(*d, *s++, T);
                    d++;
                    len -= 8;
                } while (len >= 0);
            }
        } else {
            if (t_skew == 0) {
                do {
                    S = (s[0]<<s_skew) | (s[1]>>(8-s_skew));
                    s++;
                    *d = proc(*d, S, *t++);
                    d++;
                    len -= 8;
                } while (len >= 0);
            } else {
                do {
                    S = (s[0]<<s_skew) | (s[1]>>(8-s_skew));
                    s++;
                    T = (t[0]<<t_skew) | (t[1]>>(8-t_skew));
                    t++;
                    *d = proc(*d, S, T);
                    d++;
                    len -= 8;
                } while (len >= 0);
            }
        }
    }
    if (rmask != 0xFF) {
        /* Unaligned right hand case */
        S = (s[0]<<s_skew) | (s[1]>>(8-s_skew));
        T = (t[0]<<t_skew) | (t[1]>>(8-t_skew));
        D = proc(*d, S, T);
        *d = (D & ~rmask) | (*d & rmask);
    }
}
#endif

#ifdef USE_TEMPLATES
#define TEMPLATE_NAME          generic_rop_run8
#include "gsroprun8.h"
#else
static void generic_rop_run8(rop_run_op *op, byte *d, int len)
{
    rop_proc    proc = rop_proc_table[op->rop];
    const byte *s = op->s.b.ptr;
    const byte *t = op->t.b.ptr;
    do {
        *d = proc(*d, *s++, *t++);
        d++;
    }
    while (--len);
}
#endif

#ifdef USE_TEMPLATES
#define TEMPLATE_NAME          generic_rop_run8_1bit
#define S_TRANS MAYBE
#define T_TRANS MAYBE
#define S_1BIT  MAYBE
#define T_1BIT  MAYBE
#include "gsroprun8.h"
#else
static void generic_rop_run8_1bit(rop_run_op *op, byte *d, int len)
{
    rop_proc     proc = rop_proc_table[lop_rop(op->rop)];
    const byte  *s = op->s.b.ptr;
    const byte  *t = op->t.b.ptr;
    int          sroll, troll;
    const gx_color_index *scolors = op->scolors;
    const gx_color_index *tcolors = op->tcolors;
    if (op->flags & rop_s_1bit) {
        s = op->s.b.ptr + (op->s.b.pos>>3);
        sroll = 8-(op->s.b.pos & 7);
    } else
        sroll = 0;
    if (op->flags & rop_t_1bit) {
        t = op->t.b.ptr + (op->t.b.pos>>3);
        troll = 8-(op->t.b.pos & 7);
    } else
        troll = 0;
    do {
        rop_operand S, T;
        if (sroll == 0)
            S = *s++;
        else {
            --sroll;
            S = scolors[(*s >> sroll) & 1];
            if (sroll == 0) {
                sroll = 8;
                s++;
            }
        }
        if (troll == 0)
            T = *t++;
        else {
            --troll;
            T = tcolors[(*t >> troll) & 1];
            if (troll == 0) {
                troll = 8;
                t++;
            }
        }
        *d = proc(*d, S, T);
        d++;
    }
    while (--len);
}
#endif

#ifdef USE_TEMPLATES
#define TEMPLATE_NAME          generic_rop_run24
#include "gsroprun24.h"
#else
static void generic_rop_run24(rop_run_op *op, byte *d, int len)
{
    rop_proc    proc = rop_proc_table[op->rop];
    const byte *s = op->s.b.ptr;
    const byte *t = op->t.b.ptr;
    do
    {
        rop_operand D = proc(get24(d), get24(s), get24(t));
        put24(d, D);
        s += 3;
        t += 3;
        d += 3;
    }
    while (--len);
}
#endif

#ifdef USE_TEMPLATES
#define TEMPLATE_NAME          generic_rop_run24_1bit
#define S_TRANS MAYBE
#define T_TRANS MAYBE
#define S_1BIT MAYBE
#define T_1BIT MAYBE
#include "gsroprun24.h"
#else
static void generic_rop_run24_1bit(rop_run_op *op, byte *d, int len)
{
    rop_proc     proc = rop_proc_table[lop_rop(op->rop)];
    const byte  *s = op->s.b.ptr;
    const byte  *t = op->t.b.ptr;
    int          sroll, troll;
    const gx_color_index *scolors = op->scolors;
    const gx_color_index *tcolors = op->tcolors;
    if (op->flags & rop_s_1bit) {
        s = op->s.b.ptr + (op->s.b.pos>>3);
        sroll = 8-(op->s.b.pos & 7);
    } else
        sroll = 0;
    if (op->flags & rop_t_1bit) {
        t = op->t.b.ptr + (op->t.b.pos>>3);
        troll = 8-(op->t.b.pos & 7);
    } else
        troll = 0;
    do {
        rop_operand S, T;
        if (sroll == 0) {
            S = get24(s);
            s += 3;
        } else {
            --sroll;
            S = scolors[(*s >> sroll) & 1];
            if (sroll == 0) {
                sroll = 8;
                s++;
            }
        }
        if (troll == 0) {
            T = get24(t);
            t += 3;
        } else {
            --troll;
            T = tcolors[(*t >> troll) & 1];
            if (troll == 0) {
                troll = 8;
                t++;
            }
        }
        {
            rop_operand D = proc(get24(d), S, T);
            put24(d, D);
        }
        d += 3;
    }
    while (--len);
}
#endif

#ifdef USE_TEMPLATES
#define TEMPLATE_NAME          generic_rop_run1_const_t
#define T_CONST
#include "gsroprun1.h"
#else
static void generic_rop_run1_const_t(rop_run_op *op, byte *d, int len)
{
    rop_proc    proc = rop_proc_table[op->rop];
    byte        lmask, rmask;
    byte        T = (byte)op->t.c;
    byte        S, D;
    const byte *s = op->s.b.ptr;
    int         s_skew;

    /* T should be supplied as 'depth' bits. Duplicate that up to be byte
     * size (if it's supplied byte sized, that's fine too). */
    if (op->depth & 1)
        T |= T<<1;
    if (op->depth & 3)
        T |= T<<2;
    if (op->depth & 7)
        T |= T<<4;

    len    = len*op->depth + op->dpos;
    /* lmask = the set of bits to alter in the output bitmap on the left
     * hand edge of the run. rmask = the set of bits NOT to alter in the
     * output bitmap on the right hand edge of the run. */
    lmask  = 255>>(7 & op->dpos);
    rmask  = 255>>(7 & len);

    /* See note #1 above. RJW. */
    s_skew = op->s.b.pos - op->dpos;
    if (s_skew < 0) {
        s_skew += 8;
        s--;
    }

    len -= 8;
    if (len < 0) {
        /* Short case - starts and ends in the same byte */
        lmask &= ~rmask; /* Combined mask = bits to alter */
        S = (s[0]<<s_skew) | (s[1]>>(8-s_skew));
        D = proc(*d, S, T);
        *d = (*d & ~lmask) | (D & lmask);
        return;
    }
    if (lmask != 0xFF) {
        /* Unaligned left hand case */
        S = (s[0]<<s_skew) | (s[1]>>(8-s_skew));
        s++;
        D = proc(*d, S, T);
        *d = (*d & ~lmask) | (D & lmask);
        d++;
        len -= 8;
    }
    if (len >= 0) {
        /* Simple middle case (complete destination bytes). */
        if (s_skew == 0) {
            do {
                *d = proc(*d, *s++, T);
                d++;
                len -= 8;
            } while (len >= 0);
        } else {
            do {
                S = (s[0]<<s_skew) | (s[1]>>(8-s_skew));
                s++;
                *d = proc(*d, S, T);
                d++;
                len -= 8;
            } while (len >= 0);
        }
    }
    if (rmask != 0xFF) {
        /* Unaligned right hand case */
        S = (s[0]<<s_skew) | (s[1]>>(8-s_skew));
        D = proc(*d, S, T);
        *d = (D & ~rmask) | (*d & rmask);
    }
}
#endif

#ifdef USE_TEMPLATES
#define TEMPLATE_NAME          generic_rop_run8_const_t
#define T_CONST
#include "gsroprun8.h"
#else
static void generic_rop_run8_const_t(rop_run_op *op, byte *d, int len)
{
    rop_proc    proc = rop_proc_table[op->rop];
    const byte *s = op->s.b.ptr;
    byte        t = op->t.c;
    do
    {
        *d = proc(*d, s, *t++);
        d++;
    }
    while (--len);
}
#endif

#ifdef USE_TEMPLATES
#define TEMPLATE_NAME          generic_rop_run8_1bit_const_t
#define S_TRANS MAYBE
#define S_1BIT YES
#define T_CONST
#include "gsroprun8.h"
#else
static void generic_rop_run8_1bit_const_t(rop_run_op *op, byte *d, int len)
{
    rop_proc     proc = rop_proc_table[lop_rop(op->rop)];
    byte         T = op->t.c;
    const byte  *s = op->s.b.ptr;
    int          sroll;
    const byte  *scolors = op->scolors;
    s = op->s.b.ptr + (op->s.b.pos>>3);
    sroll = 8-(op->s.b.pos & 7);
    do {
        rop_operand S;
        --sroll;
        S = scolors[(*s >> sroll) & 1];
        if (sroll == 0) {
            sroll = 8;
            s++;
        }
        *d++ = proc(*d, S, T);
    }
    while (--len);
}
#endif

#ifdef USE_TEMPLATES
#define TEMPLATE_NAME          generic_rop_run24_const_t
#define T_CONST
#include "gsroprun24.h"
#else
static void generic_rop_run24_const_t(rop_run_op *op, byte *d, int len)
{
    rop_proc     proc = rop_proc_table[op->rop];
    const byte  *s = op->s.b.ptr;
    rop_operand  T = op->t.c;
    do
    {
        rop_operand D = proc(get24(d), get24(s), T);
        put24(d, D);
        s += 3;
        d += 3;
    }
    while (--len);
}
#endif

#ifdef USE_TEMPLATES
#define TEMPLATE_NAME          generic_rop_run24_1bit_const_t
#define S_1BIT YES
#define S_TRANS MAYBE
#define T_CONST
#define T_TRANS MAYBE
#include "gsroprun24.h"
#else
static void generic_rop_run24_1bit_const_t(rop_run_op *op, byte *d, int len)
{
    rop_proc     proc = rop_proc_table[lop_rop(op->rop)];
    rop_operand  T = op->t.c;
    const byte  *s = op->s.b.ptr;
    int          sroll;
    const byte  *scolors = op->scolors;
    rop_operand  sc[2];
    s = op->s.b.ptr + (op->s.b.pos>>3);
    sroll = 8-(op->s.b.pos & 7);
    sc[0] = ((const gx_color_index *)op->scolors)[0];
    sc[1] = ((const gx_color_index *)op->scolors)[3];
    do {
        rop_operand S, D;
        --sroll;
        S = sc[(*s >> sroll) & 1];
        if (sroll == 0) {
            sroll = 8;
            s++;
        }
        D = proc(get24(d), S, T);
        put24(d, D);
        d += 3;
    }
    while (--len);
}
#endif

#ifdef USE_TEMPLATES
#define TEMPLATE_NAME          generic_rop_run1_const_st
#define T_CONST
#define S_CONST
#include "gsroprun1.h"
#else
static void generic_rop_run1_const_st(rop_run_op *op, byte *d, int len)
{
    rop_proc proc = rop_proc_table[op->rop];
    byte     lmask, rmask;
    byte     S = (byte)op->s.c;
    byte     T = (byte)op->t.c;
    byte     D;

    /* S and T should be supplied as 'depth' bits. Duplicate them up to be
     * byte size (if they are supplied byte sized, that's fine too). */
    if (op->depth & 1) {
        S |= S<<1;
        T |= T<<1;
    }
    if (op->depth & 3) {
        S |= S<<2;
        T |= T<<2;
    }
    if (op->depth & 7) {
        S |= S<<4;
        T |= T<<4;
    }

    len    = len*op->depth + op->dpos;
    /* lmask = the set of bits to alter in the output bitmap on the left
     * hand edge of the run. rmask = the set of bits NOT to alter in the
     * output bitmap on the right hand edge of the run. */
    lmask  = 255>>(7 & op->dpos);
    rmask  = 255>>(7 & len);

    len -= 8;
    if (len < 0) {
        /* Short case - starts and ends in the same byte */
        lmask &= ~rmask; /* Combined mask = bits to alter */
        D = proc(*d, S, T);
        *d = (*d & ~lmask) | (D & lmask);
        return;
    }
    if (lmask != 0xFF) {
        /* Unaligned left hand case */
        D = proc(*d, S, T);
        *d = (*d & ~lmask) | (D & lmask);
        d++;
        len -= 8;
    }
    while (len >= 0) {
        /* Simple middle case (complete destination bytes). */
        *d = proc(*d, S, T);
        d++;
        len -= 8;
    }
    if (rmask != 0xFF) {
        /* Unaligned right hand case */
        D = proc(*d, S, T);
        *d = (D & ~rmask) | (*d & rmask);
    }
}
#endif

#ifdef USE_TEMPLATES
#define TEMPLATE_NAME          generic_rop_run8_const_st
#define S_CONST
#define T_CONST
#include "gsroprun8.h"
#else
static void generic_rop_run8_const_st(rop_run_op *op, byte *d, int len)
{
    rop_proc proc = rop_proc_table[op->rop];
    byte     s = op->s.c;
    byte     t = op->t.c;
    do
    {
        *d = proc(*d, s, t);
        d++;
    }
    while (--len);
}
#endif

#ifdef USE_TEMPLATES
#define TEMPLATE_NAME          generic_rop_run24_const_st
#define S_CONST
#define T_CONST
#include "gsroprun24.h"
#else
static void generic_rop_run24_const_st(rop_run_op *op, byte *d, int len)
{
    rop_proc    proc = rop_proc_table[op->rop];
    rop_operand s    = op->s.c;
    rop_operand t    = op->t.c;
    do
    {
        rop_operand D = proc(get24(d), s, t);
        put24(d, D);
        d += 3;
    }
    while (--len);
}
#endif

#ifdef RECORD_ROP_USAGE
static void record_run(rop_run_op *op, byte *d, int len)
{
    rop_run_op local;

    usage[(int)op->opaque*3 + 1]++;
    usage[(int)op->opaque*3 + 2] += len;

    local.run     = op->runswap;
    local.s       = op->s;
    local.t       = op->t;
    local.scolors = op->scolors;
    local.tcolors = op->tcolors;
    local.rop     = op->rop;
    local.depth   = op->depth;
    local.flags   = op->flags;
    local.dpos    = op->dpos;
    local.release = op->release;
    local.opaque  = op->opaque;
    local.mul     = op->mul;

    op->runswap(&local, d, len);
}
#endif

static void rop_run_swapped(rop_run_op *op, byte *d, int len)
{
    rop_run_op local;

#ifdef RECORD_ROP_USAGE
    usage[(int)op->opaque*3 + 1]++;
    usage[(int)op->opaque*3 + 2] += len;
#endif

    local.run     = op->runswap;
    local.s       = op->t;
    local.t       = op->s;
    local.scolors = op->tcolors;
    local.tcolors = op->scolors;
    local.rop     = op->rop;
    local.depth   = op->depth;
    local.flags   = op->flags;
    local.dpos    = op->dpos;
    local.release = op->release;
    local.opaque  = op->opaque;
    local.mul     = op->mul;

    op->runswap(&local, d, len);
}

int rop_get_run_op(rop_run_op *op, int lop, int depth, int flags)
{
    int key;
    int swap = 0;

#ifdef DISABLE_ROPS
    lop = 0xAA;
#endif

    lop = lop_sanitize(lop);

    /* This is a simple initialisation to quiet a Coverity warning. It complains that
     * the 'if (swap)' at the end of the function uses 'run' uninitialised. While its
     * true that some ROPs do not instantly set the run member, they go through code
     * which, I believe, swaps the lop source and texture, then executes the switch
     * statement once more and the second execution sets run. So in summary I don't
     * think this is a real problem, and this fixes the Coverity warning.
     */
    op->run = 0;

    /* If the lop ignores either S or T, then we might as well set them to
     * be constants; will save us slaving through memory. */
    if (!rop3_uses_S(lop)) {
        flags |= rop_s_constant;
        flags &= ~rop_s_1bit;
    }
    if (!rop3_uses_T(lop)) {
        flags |= rop_t_constant;
        flags &= ~rop_t_1bit;
    }

    /* Cut down the number of cases. */
    /* S or T can either be constant, bitmaps, or '1-bitmaps'
     * (arrays of single bits to choose between 2 preset colors).
     * If S or T is unused, then it will appear as constant.
     */
    switch (flags)
    {
    case rop_s_constant:              /* Map 'S constant,T bitmap' -> 'S bitmap,T constant' */
    case rop_s_constant | rop_t_1bit: /* Map 'S constant,T 1-bitmap' -> 'S 1-bitmap,T constant' */
    case rop_t_1bit:                  /* Map 'S bitmap,T 1-bitmap' -> 'S 1-bitmap,T bitmap' */
        swap = 1;
        break;
    case rop_s_constant | rop_t_constant: /* Map 'S unused, T used' -> 'S used, T unused' */
        swap = ((rop_usage_table[lop & 0xff] & (rop_usage_S | rop_usage_T)) == rop_usage_T);
        break;
    }
    if (swap) {
        flags = ((flags & rop_t_constant ? rop_s_constant : 0) |
                 (flags & rop_s_constant ? rop_t_constant : 0) |
                 (flags & rop_t_1bit     ? rop_s_1bit     : 0) |
                 (flags & rop_s_1bit     ? rop_t_1bit     : 0));
        lop = lop_swap_S_T(lop);
    }
    /* At this point, we know that in the ordering:
     *   'unused' < 'constant' < 'bitmap' < '1-bitmap',
     * that S >= T.
     */

    /* Can we fold down from 24bit to 8bit? */
    op->mul = 1;
    if (depth == 24) {
        switch (flags & (rop_s_constant | rop_s_1bit))
        {
        case 0: /* s is a bitmap. No good. */
        case rop_s_1bit: /* s is 1 bit data. No good. */
            goto no_fold_24_to_8;
        case rop_s_constant: /* constant or unused */
        {
            rop_operand s = swap ? op->t.c : op->s.c;
            if ((rop_usage_table[lop & 0xff] & rop_usage_S) &&
                ((s & 0xff) != ((s>>8) & 0xff) ||
                 (s & 0xff) != ((s>>16) & 0xff)))
                /* USED, and a colour that doesn't work out the same in 8bits */
                goto no_fold_24_to_8;
            break;
        }
        }
        switch (flags & (rop_t_constant | rop_t_1bit))
        {
        case 0: /* t is a bitmap. No good. */
        case rop_t_1bit: /* t is 1 bit data. No good. */
            goto no_fold_24_to_8;
        case rop_t_constant: /* constant or unused */
        {
            rop_operand t = swap ? op->s.c : op->t.c;
            if ((rop_usage_table[lop & 0xff] & rop_usage_T) &&
                ((t & 0xff) != ((t>>8) & 0xff) ||
                 (t & 0xff) != ((t>>16) & 0xff)))
                /* USED, and a colour that doesn't work out the same in 8bits */
                goto no_fold_24_to_8;
            break;
        }
        }
        /* We can safely fold down from 24 to 8 */
        op->mul = 3;
        depth = 8;
    no_fold_24_to_8:{}
    }

    op->flags   = (flags & (rop_s_constant | rop_t_constant | rop_s_1bit | rop_t_1bit));
    op->depth   = (byte)depth;
    op->release = NULL;

    /* If S and T are constant, and the lop uses both of them, we can combine them.
     * Currently this only works for cases where D is unused.
     */
    if (op->flags == (rop_s_constant | rop_t_constant) &&
        rop_usage_table[lop & 0xff] == rop_usage_ST) {
        switch (lop & (rop3_D>>rop3_D_shift)) /* Ignore the D bits */
        {
        /* Skip 0000 as doesn't use S or T */
        case ((0<<6) | (0<<4) | (0<<2) | 1):
            op->s.c = ~(op->s.c | op->t.c);
            break;
        case ((0<<6) | (0<<4) | (1<<2) | 0):
            op->s.c = op->s.c & ~op->t.c;
            break;
        /* Skip 0011 as doesn't use S */
        case ((0<<6) | (1<<4) | (0<<2) | 0):
            op->s.c = ~op->s.c & op->t.c;
            break;
        /* Skip 0101 as doesn't use T */
        case ((0<<6) | (1<<4) | (1<<2) | 0):
            op->s.c = op->s.c ^ op->t.c;
            break;
        case ((0<<6) | (1<<4) | (1<<2) | 1):
            op->s.c = ~(op->s.c & op->t.c);
            break;
        case ((1<<6) | (0<<4) | (0<<2) | 0):
            op->s.c = op->s.c & op->t.c;
            break;
        case ((1<<6) | (0<<4) | (0<<2) | 1):
            op->s.c = ~(op->s.c ^ op->t.c);
            break;
        /* Skip 1010 as doesn't use T */
        case ((1<<6) | (0<<4) | (1<<2) | 1):
            op->s.c = op->s.c | ~op->t.c;
            break;
        /* Skip 1100 as doesn't use S */
        case ((1<<6) | (1<<4) | (0<<2) | 1):
            op->s.c = ~op->s.c | op->t.c;
            break;
        case ((1<<6) | (1<<4) | (1<<2) | 0):
            op->s.c = op->s.c | op->t.c;
            break;
        /* Skip 1111 as doesn't use S or T */
        default:
            /* Never happens */
            break;
        }
        lop = (lop & ~0xff) | rop3_S;
    }
    op->rop     = lop & 0xFF;

#define ROP_SPECIFIC_KEY(lop, depth, flags) (((lop)<<7)+(1<<6)+((depth>>3)<<4)+(flags))
#define KEY_IS_ROP_SPECIFIC(key)            (key & (1<<6))
#define STRIP_ROP_SPECIFICITY(key)          (key &= ((1<<6)-1))
#define KEY(depth, flags)                   (((depth>>3)<<4)+(flags))

    key = ROP_SPECIFIC_KEY(lop, depth, flags);
#ifdef RECORD_ROP_USAGE
    op->opaque = (void*)(key & (MAX-1));
    record((int)op->opaque);
#endif
retry:
    switch (key)
    {
    /* First, the rop specific ones */
    /* 0x33 = ~S */
    case ROP_SPECIFIC_KEY(0x33, 1, rop_t_constant):
        op->run     = notS_rop_run1_const_t;
        break;
    /* 0x55 = Invert */
    case ROP_SPECIFIC_KEY(0x55, 1, rop_s_constant | rop_t_constant):
        op->run     = invert_rop_run1;
        op->s.b.pos = 0;
        op->t.b.pos = 0;
        op->dpos    = 0;
        break;
    case ROP_SPECIFIC_KEY(0x55, 8, rop_s_constant | rop_t_constant): /* S & T UNUSED */
        op->run     = invert_rop_run8;
        break;
    /* 0x66 = D xor S */
    case ROP_SPECIFIC_KEY(0x66, 1, rop_t_constant):
        op->run     = xor_rop_run1_const_t;
        break;
    case ROP_SPECIFIC_KEY(0x66, 8, rop_s_constant | rop_t_constant): /* T_UNUSED */
        op->run     = xor_rop_run8_const_st;
        break;
    case ROP_SPECIFIC_KEY(0x66, 24, rop_s_constant | rop_t_constant): /* T_UNUSED */
        op->run     = xor_rop_run24_const_st;
        break;
    case ROP_SPECIFIC_KEY(0xAA, 1, rop_s_constant | rop_t_constant): /* S & T UNUSED */
    case ROP_SPECIFIC_KEY(0xAA, 8, rop_s_constant | rop_t_constant): /* S & T UNUSED */
    case ROP_SPECIFIC_KEY(0xAA, 24, rop_s_constant | rop_t_constant):/* S & T UNUSED */
        op->run     = nop_rop_const_st;
        return 0;
    /* 0xCC = S */
    case ROP_SPECIFIC_KEY(0xCC, 1, rop_t_constant): /* T_UNUSED */
        op->run     = sets_rop_run1;
        break;
    case ROP_SPECIFIC_KEY(0xCC, 8, rop_s_constant | rop_t_constant): /* T_UNUSED */
        op->run     = sets_rop_run8;
        break;
    case ROP_SPECIFIC_KEY(0xCC, 24, rop_s_constant | rop_t_constant): /* T_UNUSED */
        op->run     = sets_rop_run24;
        break;
    /* 0xEE = D or S */
    case ROP_SPECIFIC_KEY(0xEE, 1, rop_t_constant):
        op->run     = dors_rop_run1_const_t;
        break;
    /* Then the generic ones */
    case KEY(1, 0):
        op->run     = generic_rop_run1;
        op->s.b.pos = 0;
        op->t.b.pos = 0;
        op->dpos    = 0;
        break;
    case KEY(8, 0):
        op->run = generic_rop_run8;
        break;
    case KEY(8, rop_s_1bit):
    case KEY(8, rop_s_1bit | rop_t_1bit ):
        op->run = generic_rop_run8_1bit;
        break;
    case KEY(24, 0):
        op->run   = generic_rop_run24;
        break;
    case KEY(24, rop_s_1bit):
    case KEY(24, rop_s_1bit | rop_t_1bit ):
        op->run = generic_rop_run24_1bit;
        break;
    case KEY(1, rop_t_constant):
        op->run   = generic_rop_run1_const_t;
        op->s.b.pos = 0;
        op->t.b.pos = 0;
        op->dpos    = 0;
        break;
    case KEY(8, rop_t_constant):
        op->run   = generic_rop_run8_const_t;
        break;
    case KEY(8, rop_s_1bit | rop_t_constant):
        op->run = generic_rop_run8_1bit_const_t;
        break;
    case KEY(24, rop_t_constant):
        op->run   = generic_rop_run24_const_t;
        break;
    case KEY(24, rop_s_1bit | rop_t_constant):
        op->run = generic_rop_run24_1bit_const_t;
        break;
    case KEY(1, rop_s_constant | rop_t_constant):
        op->run   = generic_rop_run1_const_st;
        op->s.b.pos = 0;
        op->t.b.pos = 0;
        op->dpos    = 0;
        break;
    case KEY(8, rop_s_constant | rop_t_constant):
        op->run   = generic_rop_run8_const_st;
        break;
    case KEY(24, rop_s_constant | rop_t_constant):
        op->run   = generic_rop_run24_const_st;
        break;
    default:
        /* If we failed to find a specific one for this rop value, try again
         * for a generic one. */
        if (KEY_IS_ROP_SPECIFIC(key))
        {
            STRIP_ROP_SPECIFICITY(key);
            goto retry;
        }
        eprintf1("This should never happen! key=%x\n", key);
        break;
    }

    if (swap)
    {
        op->runswap = op->run;
        op->run = rop_run_swapped;
    }
#ifdef RECORD_ROP_USAGE
    else
    {
        op->runswap = op->run;
        op->run = record_run;
    }
#endif
    return 1;
}

void (rop_set_s_constant)(rop_run_op *op, int s)
{
    rop_set_s_constant(op, s);
}

void (rop_set_s_bitmap)(rop_run_op *op, const byte *s)
{
    rop_set_s_bitmap(op, s);
}

void (rop_set_s_bitmap_subbyte)(rop_run_op *op, const byte *s, int pos)
{
    rop_set_s_bitmap_subbyte(op, s, pos);
}

void (rop_set_s_colors)(rop_run_op *op, const byte *scolors)
{
    rop_set_s_colors(op, scolors);
}

void (rop_set_t_constant)(rop_run_op *op, int t)
{
    rop_set_t_constant(op, t);
}

void (rop_set_t_bitmap)(rop_run_op *op, const byte *t)
{
    rop_set_t_bitmap(op, t);
}

void (rop_set_t_bitmap_subbyte)(rop_run_op *op, const byte *t, int pos)
{
    rop_set_t_bitmap_subbyte(op, t, pos);
}

void (rop_set_t_colors)(rop_run_op *op, const byte *tcolors)
{
    rop_set_t_colors(op, tcolors);
}

void (rop_run)(rop_run_op *op, byte *d, int len)
{
    rop_run(op, d, len);
}

void (rop_run_subbyte)(rop_run_op *op, byte *d, int start, int len)
{
    rop_run_subbyte(op, d, start, len);
}

void (rop_release_run_op)(rop_run_op *op)
{
    rop_release_run_op(op);
}
