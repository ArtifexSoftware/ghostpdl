/* Copyright (C) 2001-2024 Artifex Software, Inc.
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


/* String GC routines for Ghostscript */
#include "memory_.h"
#include "ghost.h"
#include "gsmdebug.h"
#include "gsstruct.h"
#include "iastate.h"
#include "igcstr.h"
#include "igc.h"

/* Forward references */
static bool gc_mark_string(const byte *, uint, bool, const clump_t *);

/* (Un)mark the strings in a clump. */
void
gc_strings_set_marks(clump_t * cp, bool mark)
{
    if (cp->smark != 0) {
        if_debug3('6', "[6]clearing string marks "PRI_INTPTR"[%u] to %d\n",
                  (intptr_t)cp->smark, cp->smark_size, (int)mark);
        memset(cp->smark, 0, cp->smark_size);
        if (mark)
            gc_mark_string(cp->sbase, (cp->climit - cp->sbase), true, cp);
    }
}

/* We mark strings a word at a time. */
typedef string_mark_unit bword;

#define bword_log2_bytes log2_sizeof_string_mark_unit
#define bword_bytes (1 << bword_log2_bytes)
#define bword_log2_bits (bword_log2_bytes + 3)
#define bword_bits (1 << bword_log2_bits)
#define bword_1s (~(bword)0)
/* Compensate for byte order reversal if necessary. */
#if ARCH_IS_BIG_ENDIAN
#  if bword_bytes == 2
#    define bword_swap_bytes(m) m = (m << 8) | (m >> 8)
#  else				/* bword_bytes == 4 */
#    define bword_swap_bytes(m)\
        m = (m << 24) | ((m & 0xff00) << 8) | ((m >> 8) & 0xff00) | (m >> 24)
#  endif
#else
#  define bword_swap_bytes(m) DO_NOTHING
#endif

/* (Un)mark a string in a known clump.  Return true iff any new marks. */
static bool
gc_mark_string(const byte * ptr, uint size, bool set, const clump_t * cp)
{
    uint offset = ptr - cp->sbase;
    bword *bp = (bword *) (cp->smark + ((offset & -bword_bits) >> 3));
    uint bn = offset & (bword_bits - 1);
    bword m = (bword)(((uint64_t)bword_1s << bn) & bword_1s);
    uint left = size;
    bword marks = 0;

    bword_swap_bytes(m);
    if (set) {
        if (left + bn >= bword_bits) {
            marks |= ~*bp & m;
            *bp |= m;
            m = bword_1s, left -= bword_bits - bn, bp++;
            while (left >= bword_bits) {
                marks |= ~*bp;
                *bp = bword_1s;
                left -= bword_bits, bp++;
            }
        }
        if (left) {
            bword_swap_bytes(m);
            m = (bword)(((uint64_t)m - ((uint64_t)m << left)) & bword_1s);
            bword_swap_bytes(m);
            marks |= ~*bp & m;
            *bp |= m;
        }
    } else {
        if (left + bn >= bword_bits) {
            *bp &= ~m;
            m = bword_1s, left -= bword_bits - bn, bp++;
            if (left >= bword_bits * 5) {
                memset(bp, 0, (left & -bword_bits) >> 3);
                bp += left >> bword_log2_bits;
                left &= bword_bits - 1;
            } else
                while (left >= bword_bits) {
                    *bp = 0;
                    left -= bword_bits, bp++;
                }
        }
        if (left) {
            bword_swap_bytes(m);
            m = (bword)(((uint64_t)m - ((uint64_t)m << left)) & bword_1s);
            bword_swap_bytes(m);
            *bp &= ~m;
        }
    }
    return marks != 0;
}

#ifdef DEBUG
/* Print a string for debugging.  We need this because there is no d---
 * equivalent of fwrite.
 */
static void
dmfwrite(const gs_memory_t *mem, const byte *ptr, uint count)
{
    uint i;
    for (i = 0; i < count; ++i)
        dmputc(mem, ptr[i]);
}
#endif

/* Mark a string.  Return true if any new marks. */
bool
gc_string_mark(const byte * ptr, uint size, bool set, gc_state_t * gcst)
{
    const clump_t *cp;
    bool marks;

    if (size == 0)
        return false;
#define dmprintstr(mem)\
  dmputc(mem, '('); dmfwrite(mem, ptr, min(size, 20));\
  dmputs(mem, (size <= 20 ? ")" : "...)"))
    if (!(cp = gc_locate(ptr, gcst))) {		/* not in a clump */
#ifdef DEBUG
        if (gs_debug_c('5')) {
            dmlprintf2(gcst->heap, "[5]"PRI_INTPTR"[%u]", (intptr_t)ptr, size);
            dmprintstr(gcst->heap);
            dmputs(gcst->heap, " not in a clump\n");
        }
#endif
        return false;
    }
    if (cp->smark == 0)		/* not marking strings */
        return false;
#ifdef DEBUG
    if (ptr < cp->ctop) {
        if_debug4('6', "String pointer "PRI_INTPTR"[%u] outside ["PRI_INTPTR".."PRI_INTPTR")\n",
                 (intptr_t)ptr, size, (intptr_t)cp->ctop, (intptr_t)cp->climit);
        return false;
    } else if (ptr + size > cp->climit) {	/*
                                                 * If this is the bottommost string in a clump that has
                                                 * an inner clump, the string's starting address is both
                                                 * cp->ctop of the outer clump and cp->climit of the inner;
                                                 * gc_locate may incorrectly attribute the string to the
                                                 * inner clump because of this.  This doesn't affect
                                                 * marking or relocation, since the machinery for these
                                                 * is all associated with the outermost clump,
                                                 * but it can cause the validity check to fail.
                                                 * Check for this case now.
                                                 */
        const clump_t *scp = cp;

        while (ptr == scp->climit && scp->outer != 0)
            scp = scp->outer;
        if (ptr + size > scp->climit) {
            if_debug4('6', "String pointer "PRI_INTPTR"[%u] outside ["PRI_INTPTR".."PRI_INTPTR")\n",
                     (intptr_t)ptr, size,
                     (intptr_t)scp->ctop, (intptr_t)scp->climit);
            return false;
        }
    }
#endif
    marks = gc_mark_string(ptr, size, set, cp);
#ifdef DEBUG
    if (gs_debug_c('5')) {
        dmlprintf4(gcst->heap, "[5]%s%smarked "PRI_INTPTR"[%u]",
                  (marks ? "" : "already "), (set ? "" : "un"),
                  (intptr_t)ptr, size);
        dmprintstr(gcst->heap);
        dmputc(gcst->heap, '\n');
    }
#endif
    return marks;
}

/* Clear the relocation for strings. */
/* This requires setting the marks. */
void
gc_strings_clear_reloc(clump_t * cp)
{
    if (cp->sreloc != 0) {
        gc_strings_set_marks(cp, true);
        if_debug1('6', "[6]clearing string reloc "PRI_INTPTR"\n",
                  (intptr_t)cp->sreloc);
        gc_strings_set_reloc(cp);
    }
}

/* Count the 0-bits in a byte. */
static const byte count_zero_bits_table[256] =
{
#define o4(n) n,n-1,n-1,n-2
#define o16(n) o4(n),o4(n-1),o4(n-1),o4(n-2)
#define o64(n) o16(n),o16(n-1),o16(n-1),o16(n-2)
    o64(8), o64(7), o64(7), o64(6)
};

#define byte_count_zero_bits(byt)\
  (uint)(count_zero_bits_table[byt])
#define byte_count_one_bits(byt)\
  (uint)(8 - count_zero_bits_table[byt])

/* Set the relocation for the strings in a clump. */
/* The sreloc table stores the relocated offset from climit for */
/* the beginning of each block of string_data_quantum characters. */
void
gc_strings_set_reloc(clump_t * cp)
{
    if (cp->sreloc != 0 && cp->smark != 0) {
        byte *bot = cp->ctop;
        byte *top = cp->climit;
        uint count =
            (top - bot + (string_data_quantum - 1)) >>
            log2_string_data_quantum;
        string_reloc_offset *relp =
            cp->sreloc +
            (cp->smark_size >> (log2_string_data_quantum - 3));
        register const byte *bitp = cp->smark + cp->smark_size;
        register string_reloc_offset reloc = 0;

        /* Skip initial unrelocated strings quickly. */
#if string_data_quantum == bword_bits || string_data_quantum == bword_bits * 2
        {
            /* Work around the alignment aliasing bug. */
            const bword *wp = (const bword *)bitp;

#if string_data_quantum == bword_bits
#  define RELOC_TEST_1S(wp) (wp[-1])
#else /* string_data_quantum == bword_bits * 2 */
#  define RELOC_TEST_1S(wp) (wp[-1] & wp[-2])
#endif
            while (count && RELOC_TEST_1S(wp) == bword_1s) {
                wp -= string_data_quantum / bword_bits;
                *--relp = reloc += string_data_quantum;
                --count;
            }
#undef RELOC_TEST_1S
            bitp = (const byte *)wp;
        }
#endif
        while (count--) {
            bitp -= string_data_quantum / 8;
            reloc += string_data_quantum -
                byte_count_zero_bits(bitp[0]);
            reloc -= byte_count_zero_bits(bitp[1]);
            reloc -= byte_count_zero_bits(bitp[2]);
            reloc -= byte_count_zero_bits(bitp[3]);
#if log2_string_data_quantum > 5
            reloc -= byte_count_zero_bits(bitp[4]);
            reloc -= byte_count_zero_bits(bitp[5]);
            reloc -= byte_count_zero_bits(bitp[6]);
            reloc -= byte_count_zero_bits(bitp[7]);
#endif
            *--relp = reloc;
        }
    }
    cp->sdest = cp->climit;
}

/* Relocate a string pointer. */
void
igc_reloc_string(gs_string * sptr, gc_state_t * gcst)
{
    byte *ptr;
    const clump_t *cp;
    uint offset;
    uint reloc;
    const byte *bitp;
    byte byt;

    if (sptr->size == 0) {
        sptr->data = 0;
        return;
    }
    ptr = sptr->data;

    if (!(cp = gc_locate(ptr, gcst)))	/* not in a clump */
        return;
    if (cp->sreloc == 0 || cp->smark == 0)	/* not marking strings */
        return;
    offset = ptr - cp->sbase;
    reloc = cp->sreloc[offset >> log2_string_data_quantum];
    bitp = &cp->smark[offset >> 3];
    switch (offset & (string_data_quantum - 8)) {
#if log2_string_data_quantum > 5
        case 56:
            reloc -= byte_count_one_bits(bitp[-7]);
        case 48:
            reloc -= byte_count_one_bits(bitp[-6]);
        case 40:
            reloc -= byte_count_one_bits(bitp[-5]);
        case 32:
            reloc -= byte_count_one_bits(bitp[-4]);
#endif
        case 24:
            reloc -= byte_count_one_bits(bitp[-3]);
        case 16:
            reloc -= byte_count_one_bits(bitp[-2]);
        case 8:
            reloc -= byte_count_one_bits(bitp[-1]);
    }
    byt = *bitp & (0xff >> (8 - (offset & 7)));
    reloc -= byte_count_one_bits(byt);
    if_debug2('5', "[5]relocate string "PRI_INTPTR" to 0x%lx\n",
              (intptr_t)ptr, (intptr_t)(cp->sdest - reloc));
    sptr->data = (cp->sdest - reloc);
}
void
igc_reloc_const_string(gs_const_string * sptr, gc_state_t * gcst)
{   /* We assume the representation of byte * and const byte * is */
    /* the same.... */
    igc_reloc_string((gs_string *) sptr, gcst);
}
void
igc_reloc_param_string(gs_param_string * sptr, gc_state_t * gcst)
{
    if (!sptr->persistent) {
        /* We assume that gs_param_string is a subclass of gs_string. */
        igc_reloc_string((gs_string *)sptr, gcst);
    }
}

/* Compact the strings in a clump. */
void
gc_strings_compact(clump_t * cp, const gs_memory_t *mem)
{
    if (cp->smark != 0) {
        byte *hi = cp->climit;
        byte *lo = cp->ctop;
        const byte *from = hi;
        byte *to = hi;
        const byte *bp = cp->smark + cp->smark_size;

#ifdef DEBUG
        if (gs_debug_c('4') || gs_debug_c('5')) {
            byte *base = cp->sbase;
            uint i = (lo - base) & -string_data_quantum;
            uint n = ROUND_UP(hi - base, string_data_quantum);

#define R 16
            for (; i < n; i += R) {
                uint j;

                dmlprintf1(mem, "[4]"PRI_INTPTR": ", (intptr_t) (base + i));
                for (j = i; j < i + R; j++) {
                    byte ch = base[j];

                    if (ch <= 31) {
                        dmputc(mem, '^');
                        dmputc(mem, ch + 0100);
                    } else
                        dmputc(mem, ch);
                }
                dmputc(mem, ' ');
                for (j = i; j < i + R; j++)
                    dmputc(mem, (cp->smark[j >> 3] & (1 << (j & 7)) ?
                           '+' : '.'));
#undef R
                if (!(i & (string_data_quantum - 1)))
                    dmprintf1(mem, " %u", cp->sreloc[i >> log2_string_data_quantum]);
                dmputc(mem, '\n');
            }
        }
#endif
        /*
         * Skip unmodified strings quickly.  We know that cp->smark is
         * aligned to a string_mark_unit.
         */
        {
            /* Work around the alignment aliasing bug. */
            const bword *wp = (const bword *)bp;

            while (to > lo && wp[-1] == bword_1s)
                to -= bword_bits, --wp;
            bp = (const byte *)wp;
            while (to > lo && bp[-1] == 0xff)
                to -= 8, --bp;
        }
        from = to;

        while (from > lo) {
            byte b = *--bp;

            from -= 8;
            switch (b) {
                case 0xff:
                    to -= 8;
                    /*
                     * Since we've seen a byte other than 0xff, we know
                     * to != from at this point.
                     */
                    to[7] = from[7];
                    to[6] = from[6];
                    to[5] = from[5];
                    to[4] = from[4];
                    to[3] = from[3];
                    to[2] = from[2];
                    to[1] = from[1];
                    to[0] = from[0];
                    break;
                default:
                    if (b & 0x80)
                        *--to = from[7];
                    if (b & 0x40)
                        *--to = from[6];
                    if (b & 0x20)
                        *--to = from[5];
                    if (b & 0x10)
                        *--to = from[4];
                    if (b & 8)
                        *--to = from[3];
                    if (b & 4)
                        *--to = from[2];
                    if (b & 2)
                        *--to = from[1];
                    if (b & 1)
                        *--to = from[0];
                    /* falls through */
                case 0:
                    ;
            }
        }
        gs_alloc_fill(cp->ctop, gs_alloc_fill_collected,
                      to - cp->ctop);
        cp->ctop = to;
    }
}
