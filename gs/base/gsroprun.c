/* Copyright (C) 2001-2006 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

/* $Id$ */
/* Runs of RasterOps */
#include "std.h"
#include "stdpre.h"
#include "gsropt.h"

#define get24(ptr)\
  (((rop_operand)(ptr)[0] << 16) | ((rop_operand)(ptr)[1] << 8) | (ptr)[2])
#define put24(ptr, pixel)\
  (ptr)[0] = (byte)((pixel) >> 16),\
  (ptr)[1] = (byte)((uint)(pixel) >> 8),\
  (ptr)[2] = (byte)(pixel)

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
        S = (s[0]<<s_skew) | s[1]>>(8-s_skew);
        T = (t[0]<<t_skew) | t[1]>>(8-t_skew);
        D = proc(*d, S, T);
        *d = (*d & ~lmask) | (D & lmask);
        return;
    }
    if (lmask != 0xFF) {
        /* Unaligned left hand case */
        S = (s[0]<<s_skew) | s[1]>>(8-s_skew);
        s++;
        T = (t[0]<<t_skew) | t[1]>>(8-t_skew);
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
                    T = (t[0]<<t_skew) | t[1]>>(8-t_skew);
                    t++;
                    *d = proc(*d, *s++, T);
                    d++;
                    len -= 8;
                } while (len >= 0);
            }
        } else {
            if (t_skew == 0) {
                do {
                    S = (s[0]<<s_skew) | s[1]>>(8-s_skew);
                    s++;
                    *d = proc(*d, S, *t++);
                    d++;
                    len -= 8;
                } while (len >= 0);
            } else {
                do {
                    S = (s[0]<<s_skew) | s[1]>>(8-s_skew);
                    s++;
                    T = (t[0]<<t_skew) | t[1]>>(8-t_skew);
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
        S = (s[0]<<s_skew) | s[1]>>(8-s_skew);
        T = (t[0]<<t_skew) | t[1]>>(8-t_skew);
        D = proc(*d, S, T);
        *d = (D & ~rmask) | (*d & rmask);
    }
}

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

static void generic_rop_run1_const_s(rop_run_op *op, byte *d, int len)
{
    rop_proc    proc = rop_proc_table[op->rop];
    byte        lmask, rmask;
    byte        S = (byte)op->s.c;
    byte        T, D;
    const byte *t = op->t.b.ptr;
    int         t_skew;

    /* S should be supplied as 'depth' bits. Duplicate that up to be byte
     * size (if it's supplied byte sized, that's fine too). */
    if (op->depth & 1)
        S |= S<<1;
    if (op->depth & 3)
        S |= S<<2;
    if (op->depth & 7)
        S |= S<<4;

    len    = len*op->depth + op->dpos;
    /* lmask = the set of bits to alter in the output bitmap on the left
     * hand edge of the run. rmask = the set of bits NOT to alter in the
     * output bitmap on the right hand edge of the run. */
    lmask  = 255>>(7 & op->dpos);
    rmask  = 255>>(7 & len);

    /* See note #1 above. RJW. */
    t_skew = op->t.b.pos - op->dpos;
    if (t_skew < 0) {
        t_skew += 8;
        t--;
    }

    len -= 8;
    if (len < 0) {
        /* Short case - starts and ends in the same byte */
        lmask &= ~rmask; /* Combined mask = bits to alter */
        T = (t[0]<<t_skew) | t[1]>>(8-t_skew);
        D = proc(*d, S, T);
        *d = (*d & ~lmask) | (D & lmask);
        return;
    }
    if (lmask != 0xFF) {
        /* Unaligned left hand case */
        T = (t[0]<<t_skew) | t[1]>>(8-t_skew);
        t++;
        D = proc(*d, S, T);
        *d = (*d & ~lmask) | (D & lmask);
        d++;
        len -= 8;
    }
    if (len >= 0) {
        /* Simple middle case (complete destination bytes). */
        if (t_skew == 0) {
            do {
                *d = proc(*d, S, *t++);
                d++;
                len -= 8;
            } while (len >= 0);
        } else {
            do {
                T = (t[0]<<t_skew) | t[1]>>(8-t_skew);
                t++;
                *d = proc(*d, S, T);
                d++;
                len -= 8;
            } while (len >= 0);
        }
    }
    if (rmask != 0xFF) {
        /* Unaligned right hand case */
        T = (t[0]<<t_skew) | t[1]>>(8-t_skew);
        D = proc(*d, S, T);
        *d = (D & ~rmask) | (*d & rmask);
    }
}

static void generic_rop_run8_const_s(rop_run_op *op, byte *d, int len)
{
    rop_proc    proc = rop_proc_table[op->rop];
    byte        s = op->s.c;
    const byte *t = op->t.b.ptr;
    do
    {
        *d = proc(*d, s, *t++);
        d++;
    }
    while (--len);
}

static void generic_rop_run24_const_s(rop_run_op *op, byte *d, int len)
{
    rop_proc     proc = rop_proc_table[op->rop];
    rop_operand  s = op->s.c;
    const byte  *t = op->t.b.ptr;
    do
    {
        rop_operand D = proc(get24(d), s, get24(t));
        put24(d, D);
        t += 3;
        d += 3;
    }
    while (--len);
}

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

static void rop_run_swapped(rop_run_op *op, byte *d, int len)
{
    rop_run_op local;

    local.run     = op->runswap;
    local.s       = op->t;
    local.t       = op->s;
    local.rop     = op->rop;
    local.depth   = op->depth;
    local.flags   = op->flags;
    local.dpos    = op->dpos;
    local.release = op->release;
    local.opaque  = op->opaque;

    op->runswap(&local, d, len);
}

void rop_get_run_op(rop_run_op *op, byte rop, byte depth, int flags)
{
    int key;
    int swap = 0;

    if (!rop3_uses_S(rop))
        flags |= rop_s_constant;
    if (!rop3_uses_T(rop))
        flags |= rop_t_constant;

    swap = ((flags & (rop_s_constant | rop_t_constant)) == rop_t_constant);
    if (swap) {
        flags = (flags & ~rop_t_constant) | rop_s_constant;
        rop = rop3_swap_S_T(rop);
    }

    op->flags   = (flags & (rop_s_constant | rop_t_constant));
    op->depth   = (byte)depth;
    op->rop     = rop;
    op->release = NULL;
    key = (rop<<9) + 256 + (depth<<2) + flags;
retry:
    switch (key)
    {
    case (1<<2):
        op->run     = generic_rop_run1;
        op->s.b.pos = 0;
        op->t.b.pos = 0;
        op->dpos    = 0;
        break;
    case (8<<2):
        op->run   = generic_rop_run8;
        break;
    case (24<<2):
        op->run   = generic_rop_run24;
        break;
    case (1<<2) + rop_s_constant:
        op->run   = generic_rop_run1_const_s;
        op->s.b.pos = 0;
        op->t.b.pos = 0;
        op->dpos    = 0;
        break;
    case (8<<2) + rop_s_constant:
        op->run   = generic_rop_run8_const_s;
        break;
    case (24<<2) + rop_s_constant:
        op->run   = generic_rop_run24_const_s;
        break;
    case (1<<2) + rop_s_constant + rop_t_constant:
        op->run   = generic_rop_run1_const_st;
        op->s.b.pos = 0;
        op->t.b.pos = 0;
        op->dpos    = 0;
        break;
    case (8<<2) + rop_s_constant + rop_t_constant:
        op->run   = generic_rop_run8_const_st;
        break;
    case (24<<2) + rop_s_constant + rop_t_constant:
        op->run   = generic_rop_run24_const_st;
        break;
    default:
        /* If we failed to find a specific one for this rop value, try again
         * for a generic one. */
        if (key & 256)
        {
            key &= 255;
            goto retry;
        }
        eprintf("This should never happen!\n");
        break;
    }

    if (swap)
    {
        op->runswap = op->run;
        op->run = rop_run_swapped;
    }
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

