/* Copyright (C) 2001-2025 Artifex Software, Inc.
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


/* CCITTFax decoding filter */
#include "stdio_.h"		/* includes std.h */
#include "memory_.h"
#include "gdebug.h"
#include "strimpl.h"
#include "scf.h"
#include "scfx.h"

/* ------ CCITTFaxDecode ------ */

private_st_CFD_state();

#define CFD_BUFFER_SLOP 4

static inline int
get_run(stream_CFD_state *ss, stream_cursor_read *pr, const cfd_node decode[],
    int initial_bits, int min_bits, int *runlen, const char *str);

static inline int
invert_data(stream_CFD_state *ss, stream_cursor_read *pr, int *rlen, byte black_byte);

static inline int
skip_data(stream_CFD_state *ss, stream_cursor_read *pr, int rlen);

/* Set default parameter values. */
static void
s_CFD_set_defaults(register stream_state * st)
{
    stream_CFD_state *const ss = (stream_CFD_state *) st;

    s_CFD_set_defaults_inline(ss);
}

/* Initialize CCITTFaxDecode filter */
static int
s_CFD_init(stream_state * st)
{
    stream_CFD_state *const ss = (stream_CFD_state *) st;
    int raster = ss->raster =
        ROUND_UP((ss->Columns + 7) >> 3, ss->DecodedByteAlign);
    byte white = (ss->BlackIs1 ? 0 : 0xff);

    if (raster < 0)
        return ERRC;

    if (ss->Columns <= 0)
        return ERRC;

    s_hcd_init_inline(ss);
    /* Because skip_white_pixels can look as many as 4 bytes ahead, */
    /* we need to allow 4 extra bytes at the end of the row buffers. */
    ss->lbufstart = gs_alloc_bytes(st->memory, raster + CFD_BUFFER_SLOP * (size_t)2, "CFD lbuf");
    ss->lprev = 0;
    if (ss->lbufstart == 0)
        return ERRC;		/****** WRONG ******/
    ss->lbuf = ss->lbufstart + CFD_BUFFER_SLOP;
    memset(ss->lbufstart, 0xaa, CFD_BUFFER_SLOP);
    memset(ss->lbuf, white, raster);
    memset(ss->lbuf + raster, 0xaa, CFD_BUFFER_SLOP);  /* for Valgrind */
    if (ss->K != 0) {
        ss->lprevstart = gs_alloc_bytes(st->memory, raster + CFD_BUFFER_SLOP * (size_t)2, "CFD lprev");
        if (ss->lprevstart == 0)
            return ERRC;	/****** WRONG ******/
        ss->lprev = ss->lprevstart + CFD_BUFFER_SLOP;
        /* Clear the initial reference line for 2-D encoding. */
        memset(ss->lprev, white, raster);
        /* Ensure that the scan of the reference line will stop. */
        memset(ss->lprev + raster, 0xaa, CFD_BUFFER_SLOP);
        memset(ss->lprevstart, 0xaa, CFD_BUFFER_SLOP);
    }
    ss->k_left = min(ss->K, 0);
    ss->run_color = 0;
    ss->damaged_rows = 0;
    ss->skipping_damage = false;
    ss->cbit = 0;
    ss->uncomp_run = 0;
    ss->rows_left = (ss->Rows <= 0 || ss->EndOfBlock ? -1 : ss->Rows);
    ss->row = 0;
    ss->rpos = ss->wpos = -1;
    ss->eol_count = 0;
    ss->invert = white;
    ss->min_left = 1;
    return 0;
}

/* Release the filter. */
static void
s_CFD_release(stream_state * st)
{
    stream_CFD_state *const ss = (stream_CFD_state *) st;

    gs_free_object(st->memory, ss->lprevstart, "CFD lprev(close)");
    gs_free_object(st->memory, ss->lbufstart, "CFD lbuf(close)");
}

/* Declare the variables that hold the state. */
#define cfd_declare_state\
        hcd_declare_state;\
        register byte *q;\
        int qbit
/* Load the state from the stream. */
#define cfd_load_state()\
        hcd_load_state(),\
        q = ss->lbuf + ss->wpos, qbit = ss->cbit
/* Store the state back in the stream. */
#define cfd_store_state()\
        hcd_store_state(),\
        ss->wpos = q - ss->lbuf, ss->cbit = qbit

/* Macros to get blocks of bits from the input stream. */
/* Invariants: 0 <= bits_left <= bits_size; */
/* bits [bits_left-1..0] contain valid data. */

#define avail_bits(n) hcd_bits_available(n)
#define ensure_bits(n, outl) hcd_ensure_bits(n, outl)
#define peek_bits(n) hcd_peek_bits(n)
#define peek_var_bits(n) hcd_peek_var_bits(n)
#define skip_bits(n) hcd_skip_bits(n)

/* Get a run from the stream. */
#ifdef DEBUG
#  define IF_DEBUG(expr) expr
#else
#  define IF_DEBUG(expr) DO_NOTHING
#endif

static inline int get_run(stream_CFD_state *ss, stream_cursor_read *pr, const cfd_node decode[],
           int initial_bits, int min_bits, int *runlen, const char *str)
{
    cfd_declare_state;
    const cfd_node *np;
    int clen;
    cfd_load_state();

    HCD_ENSURE_BITS_ELSE(initial_bits) {
        /* We might still have enough bits for the specific code. */
        if (bits_left < min_bits)
            goto outl;
        np = &decode[hcd_peek_bits_left() << (initial_bits - bits_left)];
        if ((clen = np->code_length) > bits_left)
            goto outl;
        goto locl;
    }
    np = &decode[peek_bits(initial_bits)];
    if ((clen = np->code_length) > initial_bits) {
            IF_DEBUG(uint init_bits = peek_bits(initial_bits));
            if (!avail_bits(clen)) goto outl;
            clen -= initial_bits;
            skip_bits(initial_bits);
            ensure_bits(clen, outl);                /* can't goto outl */
            np = &decode[np->run_length + peek_var_bits(clen)];
            if_debug4('W', "%s xcode=0x%x,%d rlen=%d\n", str,
                      (init_bits << np->code_length) +
                        peek_var_bits(np->code_length),
                      initial_bits + np->code_length,
                      np->run_length);
            skip_bits(np->code_length);
    } else {
locl:
       if_debug4('W', "%s code=0x%x,%d rlen=%d\n", str,
                      peek_var_bits(clen), clen, np->run_length);
       skip_bits(clen);
    }
    *runlen = np->run_length;

    cfd_store_state();
    return(0);
outl:
    cfd_store_state();
    return(-1);
}


/* Skip data bits for a white run. */
/* rlen is either less than 64, or a multiple of 64. */
static inline int skip_data(stream_CFD_state *ss, stream_cursor_read *pr, int rlen)
{
    cfd_declare_state;
    cfd_load_state();
    (void)rlimit;

    if ( (qbit -= rlen) < 0 )
    {
        q -= qbit >> 3, qbit &= 7;
        if ( rlen >= 64 ) {
            cfd_store_state();
            return(-1);
        }
    }
    cfd_store_state();
    return(0);
}


/* Invert data bits for a black run. */
/* If rlen >= 64, execute makeup_action: this is to handle */
/* makeup codes efficiently, since these are always a multiple of 64. */

static inline int invert_data(stream_CFD_state *ss, stream_cursor_read *pr, int *rlen, byte black_byte)
{
    byte *qlim = ss->lbuf + ss->raster + CFD_BUFFER_SLOP;
    cfd_declare_state;
    cfd_load_state();
    (void)rlimit;

    if (q >= qlim || q < ss->lbufstart) {
        return(-1);
    }

    if ( (*rlen) > qbit )
    {
        if (q + ((*rlen - qbit) >> 3) > qlim) {
            return(-1);
        }

        if (q >= ss->lbuf) {
          *q++ ^= (1 << qbit) - 1;
        }
        else {
            q++;
        }
        (*rlen) -= qbit;

        if (q + ((*rlen) >> 3) >= qlim) {
            return(-1);
        }

        switch ( (*rlen) >> 3 )
        {
          case 7:         /* original rlen possibly >= 64 */
                  if ( (*rlen) + qbit >= 64 ) {
                    goto d;
                  }
                  *q++ = black_byte;
          case 6: *q++ = black_byte;
          case 5: *q++ = black_byte;
          case 4: *q++ = black_byte;
          case 3: *q++ = black_byte;
          case 2: *q++ = black_byte;
          case 1: *q = black_byte;
              (*rlen) &= 7;
              if ( !(*rlen) ) {
                  qbit = 0;
                  break;
              }
              q++;
          case 0:                 /* know rlen != 0 */
              qbit = 8 - (*rlen);
              *q ^= 0xff << qbit;
              break;
          default:        /* original rlen >= 64 */
d:            memset(q, black_byte, (*rlen) >> 3);
              q += (*rlen) >> 3;
              (*rlen) &= 7;
              if ( !(*rlen) ) {
                qbit = 0;
                q--;
              }
              else {
                qbit = 8 - (*rlen);
                *q ^= 0xff << qbit;
              }
              cfd_store_state();
              return(-1);
          }
    }
    else {
            qbit -= (*rlen),
            *q ^= ((1 << (*rlen)) - 1) << qbit;

    }
    cfd_store_state();
    return(0);
}


/* Buffer refill for CCITTFaxDecode filter */
static int cf_decode_eol(stream_CFD_state *, stream_cursor_read *);
static int cf_decode_1d(stream_CFD_state *, stream_cursor_read *);
static int cf_decode_2d(stream_CFD_state *, stream_cursor_read *);
static int cf_decode_uncompressed(stream_CFD_state *, stream_cursor_read *);
static int
s_CFD_process(stream_state * st, stream_cursor_read * pr,
              stream_cursor_write * pw, bool last)
{
    stream_CFD_state *const ss = (stream_CFD_state *) st;
    int wstop = ss->raster - 1;
    int eol_count = ss->eol_count;
    int k_left = ss->k_left;
    int rows_left = ss->rows_left;
    int status = 0;

#ifdef DEBUG
    const byte *rstart = pr->ptr;
    const byte *wstart = pw->ptr;

#endif

    /* Update the pointers we actually use, in case GC moved the buffer */
    ss->lbuf = ss->lbufstart + CFD_BUFFER_SLOP;
    ss->lprev = ss->lprevstart + CFD_BUFFER_SLOP;

top:
#ifdef DEBUG
    {
        hcd_declare_state;
        hcd_load_state();
        (void)rlimit;
        if_debug8m('w', ss->memory,
                   "[w]CFD_process top: eol_count=%d, k_left=%d, rows_left=%d\n"
                   "    bits="PRI_INTPTR", bits_left=%d, read %u, wrote %u%s\n",
                   eol_count, k_left, rows_left,
                   (intptr_t) bits, bits_left,
                   (uint) (p - rstart), (uint) (pw->ptr - wstart),
                   (ss->skipping_damage ? ", skipping damage" : ""));
    }
#endif
    if (ss->skipping_damage) {	/* Skip until we reach an EOL. */
        hcd_declare_state;
        int skip;
        (void)rlimit;

        status = 0;
        do {
            switch ((skip = cf_decode_eol(ss, pr))) {
                default:	/* not EOL */
                    hcd_load_state();
                    skip_bits(-skip);
                    hcd_store_state();
                    continue;
                case 0:	/* need more input */
                    goto out;
                case 1:	/* EOL */
                    {		/* Back up over the EOL. */
                        hcd_load_state();
                        bits_left += run_eol_code_length;
                        hcd_store_state();
                    }
                    ss->skipping_damage = false;
            }
        }
        while (ss->skipping_damage);
        ss->damaged_rows++;
    }
    /*
     * Check for a completed input scan line.  This isn't quite as
     * simple as it seems, because we could have run out of input data
     * between a makeup code and a 0-length termination code, or in a
     * 2-D line before a final horizontal code with a 0-length second
     * run.  There's probably a way to think about this situation that
     * doesn't require a special check, but I haven't found it yet.
     */
    if (ss->wpos == wstop && ss->cbit <= (-ss->Columns & 7) &&
        (k_left == 0 ? !(ss->run_color & ~1) : ss->run_color == 0)
        ) {			/* Check for completed data to be copied to the client. */
        /* (We could avoid the extra copy step for 1-D, but */
        /* it's simpler not to, and it doesn't cost much.) */
        if (ss->rpos < ss->wpos) {
            stream_cursor_read cr;

            cr.ptr = ss->lbuf + ss->rpos;
            cr.limit = ss->lbuf + ss->wpos;
            status = stream_move(&cr, pw);
            ss->rpos = cr.ptr - ss->lbuf;
            if (status)
                goto out;
        }
        ss->row++;
        if (rows_left > 0 && --rows_left == 0)
            goto ck_eol;	/* handle EOD if it is present */
        if (ss->K != 0) {
            byte *prev_bits = ss->lprev;
            byte *prev_start = ss->lprevstart;

            ss->lprev = ss->lbuf;
            ss->lprevstart = ss->lbufstart;
            ss->lbuf = prev_bits;
            ss->lbufstart = prev_start;
            if (ss->K > 0)
                k_left = (k_left == 0 ? ss->K : k_left) - 1;
        }
        ss->rpos = ss->wpos = -1;
        ss->eol_count = eol_count = 0;
        ss->cbit = 0;
        ss->invert = (ss->BlackIs1 ? 0 : 0xff);
        memset(ss->lbuf, ss->invert, wstop + 1);
        ss->run_color = 0;
        /*
         * If EndOfLine is true, we want to include the byte padding
         * in the string of initial zeros in the EOL.  If EndOfLine
         * is false, we aren't sure what we should do....
         */
        if (ss->EncodedByteAlign && !ss->EndOfLine)
            ss->bits_left &= ~7;
    }
    /* If we're between scan lines, scan for EOLs. */
    if (ss->wpos < 0) {
            /*
             * According to Adobe, the decoder should always check for
             * the EOD sequence, regardless of EndOfBlock: the Red Book's
             * documentation of EndOfBlock is wrong.
             */
ck_eol:
        while ((status = cf_decode_eol(ss, pr)) > 0) {
            if_debug0m('w', ss->memory, "[w]EOL\n");
            /* If we are in a Group 3 mixed regime, */
            /* check the next bit for 1- vs. 2-D. */
            if (ss->K > 0) {
                hcd_declare_state;
                hcd_load_state();
                ensure_bits(1, out);	/* can't fail */
                k_left = (peek_bits(1) ? 0 : 1);
                skip_bits(1);
                hcd_store_state();
            }
            ++eol_count;
            if (eol_count == (ss->K < 0 ? 2 : 6)) {
                status = EOFC;
                goto out;
            }
        }
        if (rows_left == 0) {
            status = EOFC;
            goto out;
        }
        if (status == 0)	/* input empty while scanning EOLs */
            goto out;
        switch (eol_count) {
            case 0:
                if (ss->EndOfLine) {	/* EOL is required, but none is present. */
                    status = ERRC;
                    goto check;
                }
            case 1:
                break;
            default:
                status = ERRC;
                goto check;
        }
    }
    /* Now decode actual data. */
    if (k_left < 0) {
        if_debug0m('w', ss->memory, "[w2]new row\n");
        status = cf_decode_2d(ss, pr);
    } else if (k_left == 0) {
        if_debug0m('w', ss->memory, "[w1]new row\n");
        status = cf_decode_1d(ss, pr);
    } else {
        if_debug1m('w', ss->memory, "[w1]new 2-D row, k_left=%d\n", k_left);
        status = cf_decode_2d(ss, pr);
    }
    if_debug3m('w', ss->memory, "[w]CFD status = %d, wpos = %d, cbit = %d\n",
               status, ss->wpos, ss->cbit);
  check:switch (status) {
        case 1:		/* output full */
            goto top;
        case ERRC:
            /* Check for special handling of damaged rows. */
            if (ss->damaged_rows >= ss->DamagedRowsBeforeError ||
                !(ss->EndOfLine && ss->K >= 0)
                )
                break;
            /* Substitute undamaged data if appropriate. */
            /****** NOT IMPLEMENTED YET ******/
            {
                ss->wpos = wstop;
                ss->cbit = -ss->Columns & 7;
                ss->run_color = 0;
            }
            ss->skipping_damage = true;
            goto top;
        default:
            ss->damaged_rows = 0;	/* finished a good row */
    }
  out:ss->k_left = k_left;
    ss->rows_left = rows_left;
    ss->eol_count = eol_count;
    if (ss->ErrsAsEOD && status < 0)
        return EOFC;
    return status;
}

/*
 * Decode a leading EOL, if any.
 * If an EOL is present, skip over it and return 1;
 * if no EOL is present, read no input and return -N, where N is the
 * number of initial bits that can be skipped in the search for an EOL;
 * if more input is needed, return 0.
 * Note that if we detected an EOL, we know that we can back up over it;
 * if we detected an N-bit non-EOL, we know that at least N bits of data
 * are available in the buffer.
 */
static int
cf_decode_eol(stream_CFD_state * ss, stream_cursor_read * pr)
{
    hcd_declare_state;
    int zeros;
    int look_ahead;

    hcd_load_state();
    for (zeros = 0; zeros < run_eol_code_length - 1; zeros++) {
        ensure_bits(1, out);
        if (peek_bits(1))
            return -(zeros + 1);
        skip_bits(1);
    }
    /* We definitely have an EOL.  Skip further zero bits. */
    look_ahead = (ss->K > 0 ? 2 : 1);
    for (;;) {
        ensure_bits(look_ahead, back);
        if (peek_bits(1))
            break;
        skip_bits(1);
    }
    skip_bits(1);
    hcd_store_state();
    return 1;
  back:			/*
                                 * We ran out of data while skipping zeros.
                                 * We know we are at a byte boundary, and have just skipped
                                 * at least run_eol_code_length - 1 zeros.  However,
                                 * bits_left may be 1 if look_ahead == 2.
                                 */
    bits &= (1 << bits_left) - 1;
    bits_left += run_eol_code_length - 1;
    hcd_store_state();
  out:return 0;
}

/* Decode a 1-D scan line. */
static int
cf_decode_1d(stream_CFD_state * ss, stream_cursor_read * pr)
{
    cfd_declare_state;
    byte black_byte = (ss->BlackIs1 ? 0xff : 0);
    int end_bit = -ss->Columns & 7;
    byte *stop = ss->lbuf - 1 + ss->raster;
    int run_color = ss->run_color;
    int status;
    int bcnt;
    (void)rlimit;

    cfd_load_state();
    if_debug1m('w', ss->memory, "[w1]entry run_color = %d\n", ss->run_color);
    if (run_color > 0)
        goto db;
    else
        goto dw;
#define q_at_stop() (q >= stop && (qbit <= end_bit || q > stop))
    while (1)
    {
        run_color = 0;
        if (q_at_stop())
            goto done;

  dw:   /* Decode a white run. */
        do {
            cfd_store_state();
            status = get_run(ss, pr, cf_white_decode, cfd_white_initial_bits,
                             cfd_white_min_bits, &bcnt, "[w1]white");
            cfd_load_state();
            if (status < 0) {
                /* We already set run_color to 0 or -1. */
                status = 0;
                goto out;
            }

            if (bcnt < 0) {		/* exceptional situation */
                switch (bcnt) {
                    case run_uncompressed:	/* Uncompressed data. */
                        cfd_store_state();
                        bcnt = cf_decode_uncompressed(ss, pr);
                        if (bcnt < 0)
                            return bcnt;
                        cfd_load_state();
                        if (bcnt)
                            goto db;
                        else
                            continue; /* Restart decoding white */
                        /*case run_error: */
                        /*case run_zeros: *//* Premature end-of-line. */
                    default:
                        status = ERRC;
                        goto out;
                }
            }

            cfd_store_state();
            status = skip_data(ss, pr, bcnt);
            cfd_load_state();
            if (status < 0) {
                /* If we run out of data after a makeup code, */
                /* note that we are still processing a white run. */
                run_color = -1;
            }
        } while (status < 0);

        if (q_at_stop()) {
            run_color = 0;		/* not inside a run */
  done:
            if (q > stop || qbit < end_bit)
                status = ERRC;
            else
                status = 1;
            break;
        }
        run_color = 1;

  db:   /* Decode a black run. */
        do {
            cfd_store_state();
            status = get_run(ss, pr, cf_black_decode, cfd_black_initial_bits,
                             cfd_black_min_bits, &bcnt, "[w1]black");
            cfd_load_state();
            if (status < 0) {
                /* We already set run_color to 1 or 2. */
                status = 0;
                goto out;
            }

            if (bcnt < 0) {  /* All exceptional codes are invalid here. */
                /****** WRONG, uncompressed IS ALLOWED ******/
                status = ERRC;
                goto out;
            }

            /* Invert bits designated by black run. */
            cfd_store_state();
            status = invert_data(ss, pr, &bcnt, black_byte);
            cfd_load_state();
            if (status < 0) {
                /* If we run out of data after a makeup code, */
                /* note that we are still processing a black run. */
                run_color = 2;
            }
        } while (status < 0);
    }

out:
    cfd_store_state();
    ss->run_color = run_color;
    if_debug1m('w', ss->memory, "[w1]exit run_color = %d\n", run_color);
    return status;
}

/* Decode a 2-D scan line. */
static int
cf_decode_2d(stream_CFD_state * ss, stream_cursor_read * pr)
{
    cfd_declare_state;
    byte invert_white = (ss->BlackIs1 ? 0 : 0xff);
    byte black_byte = ~invert_white;
    byte invert = ss->invert;
    int end_count = -ss->Columns & 7;
    uint raster = ss->raster;
    byte *q0 = ss->lbuf;
    byte *prev_q01 = ss->lprev + 1;
    byte *endptr = q0 - 1 + raster;
    int init_count = raster << 3;
    register int count;
    int rlen;
    int status;

    cfd_load_state();
    count = ((endptr - q) << 3) + qbit;
    endptr[1] = 0xa0;		/* a byte with some 0s and some 1s, */
    /* to ensure run scan will stop */
    if_debug1m('W', ss->memory, "[w2]raster=%d\n", raster);
    switch (ss->run_color) {
        case -2:
            ss->run_color = 0;
            goto hww;
        case -1:
            ss->run_color = 0;
            goto hbw;
        case 1:
            ss->run_color = 0;
            goto hwb;
        case 2:
            ss->run_color = 0;
            goto hbb;
            /*case 0: */
    }

    /* top of decode loop */
    while (1)
    {
        if (count <= end_count) {
            status = (count < end_count ? ERRC : 1);
            goto out;
        }
        /* If invert == invert_white, white and black have their */
        /* correct meanings; if invert == ~invert_white, */
        /* black and white are interchanged. */
        if_debug1m('W', ss->memory, "[w2]%4d:\n", count);
#ifdef DEBUG
        /* Check the invariant between q, qbit, and count. */
        {
            int pcount = (endptr - q) * 8 + qbit;

            if (pcount != count)
                dmlprintf2(ss->memory, "[w2]Error: count=%d pcount=%d\n",
                           count, pcount);
        }
#endif
        /*
         * We could just use get_run here, but we can do better.  However,
         * we must be careful to handle the case where the very last codes
         * in the input stream are 1-bit "vertical 0" codes: we can't just
         * use ensure_bits(3, ...) and go to get more data if it fails.
         */
        ensure_bits(3, out3);
#define vertical_0 (countof(cf2_run_vertical) / 2)
        switch (peek_bits(3)) {
        default /*4..7*/ :	/* vertical(0) */
            if (0) {
 out3:
                /* Unless it's a 1-bit "vertical 0" code, exit. */
                if (!(bits_left > 0 && peek_bits(1)))
                    goto out0;
            }
            skip_bits(1);
            rlen = vertical_0;
            break;
        case 2:		/* vertical(+1) */
            skip_bits(3);
            rlen = vertical_0 + 1;
            break;
        case 3:		/* vertical(-1) */
            skip_bits(3);
            rlen = vertical_0 - 1;
            break;
        case 1:		/* horizontal */
            skip_bits(3);
            if (invert == invert_white) {
                /* We handle horizontal decoding here, so that we can
                 * branch back into it if we run out of input data. */
                /* White, then black. */
  hww:
                do {
                    cfd_store_state();
                    status = get_run(ss, pr, cf_white_decode,
                                     cfd_white_initial_bits,
                                     cfd_white_min_bits, &rlen, " white");
                    cfd_load_state();
                    if (status < 0) {
                        ss->run_color = -2;
                        goto out0;
                    }

                    if ((count -= rlen) < end_count) {
                        status = ERRC;
                        goto out;
                    }
                    if (rlen < 0) goto rlen_lt_zero;

                    cfd_store_state();
                    status = skip_data(ss, pr, rlen);
                    cfd_load_state();
                } while (status < 0);

                /* Handle the second half of a white-black horizontal code. */
  hwb:
                do {
                    cfd_store_state();
                    status = get_run(ss, pr, cf_black_decode,
                                     cfd_black_initial_bits,
                                     cfd_black_min_bits, &rlen, " black");
                    cfd_load_state();
                    if (status < 0){
                        ss->run_color = 1;
                        goto out0;
                    }

                    if ((count -= rlen) < end_count) {
                        status = ERRC;
                        goto out;
                    }
                    if (rlen < 0) goto rlen_lt_zero;

                    cfd_store_state();
                    status = invert_data(ss, pr, &rlen, black_byte);
                    cfd_load_state();
                } while (status < 0);
            } else {
                /* Black, then white. */
  hbb:
                do {
                    cfd_store_state();
                    status = get_run(ss, pr, cf_black_decode,
                                     cfd_black_initial_bits,
                                     cfd_black_min_bits, &rlen, " black");
                    cfd_load_state();
                    if (status < 0) {
                        ss->run_color = 2;
                        goto out0;
                    }

                    if ((count -= rlen) < end_count) {
                        status = ERRC;
                        goto out;
                    }
                    if (rlen < 0) goto rlen_lt_zero;

                    cfd_store_state();
                    status = invert_data(ss, pr, &rlen, black_byte);
                    cfd_load_state();
                }
                while (status < 0);

                /* Handle the second half of a black-white horizontal code. */
  hbw:
                do {
                    cfd_store_state();
                    status = get_run(ss, pr, cf_white_decode,
                                     cfd_white_initial_bits,
                                     cfd_white_min_bits, &rlen, " white");
                    cfd_load_state();
                    if (status < 0) {
                        ss->run_color = -1;
                        goto out0;
                    }

                    if ((count -= rlen) < end_count) {
                        status = ERRC;
                        goto out;
                    }
                    if (rlen < 0) goto rlen_lt_zero;

                    cfd_store_state();
                    status = skip_data(ss, pr, rlen);
                    cfd_load_state();
                } while (status < 0);
            }
            continue; /* jump back to top of decode loop */
        case 0:		/* everything else */
            cfd_store_state();
            status = get_run(ss, pr, cf_2d_decode, cfd_2d_initial_bits,
                             cfd_2d_min_bits, &rlen, "[w2]");
            cfd_load_state();
            if (status < 0) {
                goto out0;
            }

            /* rlen may be run2_pass, run_uncompressed, or */
            /* 0..countof(cf2_run_vertical)-1. */
            if (rlen < 0) {
rlen_lt_zero:
                switch (rlen) {
                case run2_pass:
                    break;
                case run_uncompressed:
                {
                    int which;

                    cfd_store_state();
                    which = cf_decode_uncompressed(ss, pr);
                    if (which < 0) {
                        status = which;
                        goto out;
                    }
                    cfd_load_state();
/****** ADJUST count ******/
                    invert = (which ? ~invert_white : invert_white);
                    continue; /* jump back to top of decode loop */
                }
                default:	/* run_error, run_zeros */
                    status = ERRC;
                    goto out;
                }
            }
        }
        /* Interpreting the run requires scanning the */
        /* previous ('reference') line. */
        {
            int prev_count = count;
            byte prev_data;
            int dlen;
            static const byte count_bit[8] =
                                   {0x80, 1, 2, 4, 8, 0x10, 0x20, 0x40};
            byte *prev_q = prev_q01 + (q - q0);
            int plen;

            if (!(count & 7))
                prev_q++;		/* because of skip macros */
            prev_data = prev_q[-1] ^ invert;
            /* Find the b1 transition. */
            if ((prev_data & count_bit[prev_count & 7]) &&
                (prev_count < init_count || invert != invert_white)
                ) {			/* Look for changing white first. */
                if_debug1m('W', ss->memory, " data=0x%x", prev_data);
                skip_black_pixels(prev_data, prev_q,
                                  prev_count, invert, plen);
                if (prev_count < end_count)		/* overshot */
                    prev_count = end_count;
                if_debug1m('W', ss->memory, " b1 other=%d", prev_count);
            }
            if (prev_count != end_count) {
                if_debug1m('W', ss->memory, " data=0x%x", prev_data);
                skip_white_pixels(prev_data, prev_q,
                                  prev_count, invert, plen);
                if (prev_count < end_count)		/* overshot */
                    prev_count = end_count;
                if_debug1m('W', ss->memory, " b1 same=%d", prev_count);
            }
            /* b1 = prev_count; */
            if (rlen == run2_pass) {	/* Pass mode.  Find b2. */
                if (prev_count != end_count) {
                    if_debug1m('W', ss->memory, " data=0x%x", prev_data);
                    skip_black_pixels(prev_data, prev_q,
                                      prev_count, invert, plen);
                    if (prev_count < end_count)	/* overshot */
                        prev_count = end_count;
                }
                /* b2 = prev_count; */
                if_debug2m('W', ss->memory, " b2=%d, pass %d\n",
                           prev_count, count - prev_count);
            } else {		/* Vertical coding. */
                /* Remember that count counts *down*. */
                prev_count += rlen - vertical_0;	/* a1 */
                if_debug2m('W', ss->memory, " vertical %d -> %d\n",
                           (int)(rlen - vertical_0), prev_count);
            }
            /* Now either invert or skip from count */
            /* to prev_count, and reset count. */
            if (invert == invert_white) {	/* Skip data bits. */
                q = endptr - (prev_count >> 3);
                qbit = prev_count & 7;
            } else {		/* Invert data bits. */
                dlen = count - prev_count;

                cfd_store_state();
                (void)invert_data(ss, pr, &dlen, black_byte);
                cfd_load_state();
            }
            count = prev_count;
            if (rlen >= 0)		/* vertical mode */
                invert = ~invert;	/* polarity changes */
        }
        /* jump back to top of decode loop */
    }

  out0:status = 0;
    /* falls through */
  out:cfd_store_state();
    ss->invert = invert;
    /* Ignore an error (missing EOFB/RTC when EndOfBlock == true) */
    /* if we have finished all rows. */
    if (status == ERRC && ss->Rows > 0 && ss->row > ss->Rows)
        status = EOFC;
    return status;
}

#if 1				/*************** */
static int
cf_decode_uncompressed(stream_CFD_state * ss, stream_cursor_read * pr)
{
    return ERRC;
}
#else /*************** */

/* Decode uncompressed data. */
/* (Not tested: no sample data available!) */
/****** DOESN'T CHECK FOR OVERFLOWING SCAN LINE ******/
static int
cf_decode_uncompressed(stream * s)
{
    cfd_declare_state;
    const cfd_node *np;
    int clen, rlen;

    cfd_load_state();
    while (1) {
        ensure_bits(cfd_uncompressed_initial_bits, NOOUT);
        np = &cf_uncompressed_decode[peek_bits(cfd_uncompressed_initial_bits)];
        clen = np->code_length;
        rlen = np->run_length;
        if (clen > cfd_uncompressed_initial_bits) {	/* Must be an exit code. */
            break;
        }
        if (rlen == cfd_uncompressed_initial_bits) {	/* Longest representable white run */
            if_debug1m('W', s->memory, "[wu]%d\n", rlen);
            if ((qbit -= cfd_uncompressed_initial_bits) < 0)
                qbit += 8, q++;
        } else {
            if_debug1m('W', s->memory, "[wu]%d+1\n", rlen);
            if (qbit -= rlen < 0)
                qbit += 8, q++;
            *q ^= 1 << qbit;
        }
        skip_bits(clen);
    }
    clen -= cfd_uncompressed_initial_bits;
    skip_bits(cfd_uncompressed_initial_bits);
    ensure_bits(clen, NOOUT);
    np = &cf_uncompressed_decode[rlen + peek_var_bits(clen)];
    rlen = np->run_length;
    skip_bits(np->code_length);
    if_debug1m('w', s->memory, "[wu]exit %d\n", rlen);
    if (rlen >= 0) {		/* Valid exit code, rlen = 2 * run length + next polarity */
        if ((qbit -= rlen >> 1) < 0)
            qbit += 8, q++;
        rlen &= 1;
    }
  out:				/******* WRONG ******/
    cfd_store_state();
    return rlen;
}

#endif /*************** */

/* Stream template */
const stream_template s_CFD_template =
{&st_CFD_state, s_CFD_init, s_CFD_process, 1, 1, s_CFD_release,
 s_CFD_set_defaults
};
