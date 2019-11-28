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


/* RunLengthEncode filter */
#include "stdio_.h"		/* includes std.h */
#include "memory_.h"
#include "strimpl.h"
#include "srlx.h"

/* ------ RunLengthEncode ------ */

private_st_RLE_state();

/* Set defaults */
static void
s_RLE_set_defaults(stream_state * st)
{
    stream_RLE_state *const ss = (stream_RLE_state *) st;

    s_RLE_set_defaults_inline(ss);
}

/* Initialize */
static int
s_RLE_init(stream_state * st)
{
    stream_RLE_state *const ss = (stream_RLE_state *) st;

    return s_RLE_init_inline(ss);
}

enum {
    /* Initial state - Nothing read (but may be mid run). */
    state_0,

    /* 0 bytes into a run, n0 read. */
    state_eq_0,

    /* 0 bytes into a run, n0 and n1 read. */
    state_eq_01,

    /* n bytes into a literal run, n0 and n1 read. */
    state_gt_01,

    /* n bytes into a literal run, n0,n1,n2 read. */
    state_gt_012,

    /* -n bytes into a repeated run, n0 and n1 read. */
    state_lt_01,

    /* We have reached the end of data, but not written the marker. */
    state_eod_unmarked,

    /* We have reached the end of data, and written the marker. */
    state_eod
};

#ifdef DEBUG_RLE
static void
debug_ate(const byte *p, const byte *plimit,
          const byte *q, const byte *qlimit,
          int ret)
{
    if (p != plimit) {
        dlprintf("CONSUMED");
        while (p != plimit) {
            dlprintf1(" %02x", *++p);
        }
        dlprintf("\n");
    }
    if (q != qlimit) {
        dlprintf("PRODUCED\n");
        while (q != qlimit) {
            int n = *++q;
            if (n == 128) {
                dlprintf1(" EOD(%02x)", n);
            } else if (n < 128) {
                dlprintf2(" %d(%02x)(", n+1, n);
                n++;
                while (n-- && q != qlimit) {
                    dlprintf1(" %02x", *++q);
                }
                if (n != -1) {
                    dlprintf1(" %d missing!", n+1);
                }
                dlprintf(" )\n");
            } else {
                dlprintf2(" %d(%02x) - ", 257-n, n);
                if (q == qlimit) {
                    dlprintf("WTF!");
                }
                dlprintf1("%02x\n", *++q);
            }
        }
        dlprintf("\n");
    }
    dlprintf1("RETURNED %d\n", ret);
}
#else
#define debug_ate(a,b,c,d,e) do { } while (0)
#endif

/* Process a buffer */
static int
s_RLE_process(stream_state * st, stream_cursor_read * pr,
              stream_cursor_write * pw, bool last)
{
    stream_RLE_state *const ss = (stream_RLE_state *) st;
    register const byte *p = pr->ptr;
    register byte *q = pw->ptr;
    const byte *plimit = pr->limit;
    byte *wlimit = pw->limit;
    ulong rleft = ss->record_left;
    int run_len = ss->run_len, ret = 0;
    byte n0 = ss->n0;
    byte n1 = ss->n1;
    byte n2 = ss->n2;
    const byte *rlimit = p + rleft;
#ifdef DEBUG_RLE
    const byte *pinit = p;
    const byte *qinit = q;
    static int entry = -1;

    entry++;
    dlprintf7("ENTERED(%d): avail_in=%d avail_out=%d run_len=%d n0=%02x n1=%02x n2=%02x\n",
              entry, plimit-p, wlimit-q, run_len, n0, n1, n2);
#endif

    switch (ss->state) {
    default:
        dlprintf("Inconsistent state in s_RLE_process!\n");
        /* fall through */
    case state_0:
        while (p != plimit) {
            if (run_len == 0) {
                /* About to start a new run */
                n0 = *++p;
    case state_eq_0:
run_len_0_n0_read:
                if (p == rlimit || (p == plimit && last)) {
                    /* flush the record here, and restart */
                    if (wlimit - q < 2){
                        ss->state = state_eq_0;
                        /* no_output_room_n0_read */
                        goto no_output_room;
                    }
                    *++q = 0; /* Single literal */
                    *++q = n0;
                    rlimit = p + ss->record_size;
                    continue;
                }
                if (p == plimit) {
                    ss->state = state_eq_0;
                    /* no_more_data_n0_read */
                    goto no_more_data;
                }
                n1 = *++p;
    case state_eq_01:
                if (p == rlimit || (p == plimit && last)) {
                    /* flush the record here, and restart */
                    if (wlimit - q < 3 - (n0 == n1)) {
                        ss->state = state_eq_01;
                        /* no_output_room_n0n1_read */
                        goto no_output_room;
                    }
                    if (n0 == n1) {
                        *++q = 0xff; /* Repeat 2 */
                        *++q = n0;
                    } else {
                        *++q = 1; /* Two literals */
                        *++q = n0;
                        *++q = n1;
                    }
                    run_len = 0;
                    rlimit = p + ss->record_size;
                    continue;
                }
                if (n0 == n1) {
                    /* Start of a repeated run */
                    run_len = -2;
                } else {
                    /* A literal run of at least 1. */
                    run_len = 1;
                    ss->literals[0] = n0;
                    n0 = n1;
                }
                if (p == plimit) {
                    ss->state = state_0;
                    goto no_more_data;
                }
            } else if (run_len > 0) {
                /* We are in the middle of a run of literals */
                n1 = *++p;
    case state_gt_01:
                if (p == rlimit || run_len == 126 ||
                    (n0 == n1 && p == plimit && last)) {
                    /* flush the record here, and restart */
                    /* <len> <queue> n0 n1 */
                        if (wlimit - q < run_len+3) {
                            ss->state = state_gt_01;
                            /* no_output_room_gt_n0n1_read */
                            goto no_output_room;
                        }
                    *++q = run_len+1;
                    memcpy(q+1, ss->literals, run_len);
                    q += run_len;
                    *++q = n0;
                    *++q = n1;
                    run_len = 0;
                    if (p == rlimit)
                        rlimit = p + ss->record_size;
                    continue;
                }
                if (n0 == n1) {
                    if (p == plimit) {
                        ss->state = state_gt_01;
                        /* no_more_data_n0n1_read */
                        goto no_more_data;
                    }
                    n2 = *++p;
    case state_gt_012:
                    if (p == rlimit || run_len == 125) {
                        /* flush the record here, and restart */
                        /* <len> <queue> n0 n1 n2 */
                        if (wlimit - q < run_len+4) {
                            ss->state = state_gt_012;
                            /* no_output_room_n0n1n2_read */
                            goto no_output_room;
                        }
                        *++q = run_len+2;
                        memcpy(q+1, ss->literals, run_len);
                        q += run_len;
                        *++q = n0;
                        *++q = n1;
                        *++q = n2;
                        run_len = 0;
                        if (p == rlimit)
                            rlimit = p + ss->record_size;
                        continue;
                    }
                    if (n0 != n2) {
                        /* Stick with a literal run */
                        ss->literals[run_len++] = n0;
                        ss->literals[run_len++] = n1;
                        n0 = n2;
                    } else {
                        /* Flush current run, start a repeated run */
                        /* <len> <queue> */
                        if (wlimit - q < run_len+1) {
                            ss->state = state_gt_012;
                            /* no_output_room_n0n1n2_read */
                            goto no_output_room;
                        }
                        *++q = run_len-1;
                        memcpy(q+1, ss->literals, run_len);
                        q += run_len;
                        run_len = -3; /* Repeated run of length 3 */
                    }
                } else {
                    /* Continue literal run */
                    ss->literals[run_len++] = n0;
                    n0 = n1;
                }
            } else {
                /* We are in the middle of a repeated run */
                /* <n0 repeated -run_len times> */
                n1 = *++p;
                if (n0 == n1)
                    run_len--; /* Repeated run got longer */
    case state_lt_01:
                if (n0 != n1 || p == rlimit || run_len == -128) {
                    /* flush the record here, and restart */
                    if (wlimit - q < 2) {
                        ss->state = state_lt_01;
                        /* no_output_room_lt_n0n1_read */
                        goto no_output_room;
                    }
                    *++q = 257+run_len; /* Repeated run */
                    *++q = n0;
                    run_len = 0;
                    if (p == rlimit)
                        rlimit = p + ss->record_size;
                    if (n0 != n1) {
                        n0 = n1;
                        goto run_len_0_n0_read;
                    }
                }
            }
        }
        /* n1 is never valid here */

        if (last) {
            if (run_len == 0) {
                /* EOD */
                if (wlimit - q < 1) {
                    ss->state = state_0;
                    goto no_output_room;
                }
            } else if (run_len > 0) {
                /* Flush literal run + EOD */
                if (wlimit - q < run_len+2) {
                    ss->state = state_0;
                    goto no_output_room;
                }
                *++q = run_len;
                memcpy(q+1, ss->literals, run_len);
                q += run_len;
                *++q = n0;
            } else if (run_len < 0) {
                /* Flush repeated run + EOD */
                if (wlimit - q < 3) {
                    ss->state = state_0;
                    goto no_output_room;
                }
                *++q = 257+run_len; /* Repeated run */
                *++q = n0;
            }
    case state_eod_unmarked:
            if (!ss->omitEOD) {
                if (wlimit - q < 1) {
                    ss->state = state_eod_unmarked;
                    goto no_output_room;
                }
                *++q = 128; /* EOD */
            }
    case state_eod:
            ss->run_len = 0;
            ss->state = state_0;
            pr->ptr = p;
            pw->ptr = q;
            ss->record_left = rlimit - p;
            debug_ate(pinit, p, qinit, q, EOFC);
            return EOFC;
        }
    }

    /* Normal exit */
    ss->run_len = run_len;
    ss->state = state_0;
    ss->n0 = n0;
    ss->n1 = n1;
    pr->ptr = p;
    pw->ptr = q;
    ss->record_left = rlimit - p;
    debug_ate(pinit, p, qinit, q, 0);
    return 0;

no_output_room:
    ret = 1;
no_more_data:
    ss->n0 = n0;
    ss->n1 = n1;
    ss->n2 = n2;
    ss->run_len = run_len;
    pr->ptr = p;
    pw->ptr = q;
    ss->record_left = rlimit - p;
    debug_ate(pinit, p, qinit, q, ret);
    return ret;
}

/* Stream template */
const stream_template s_RLE_template = {
    &st_RLE_state, s_RLE_init, s_RLE_process, 1, 129, NULL,
    s_RLE_set_defaults, s_RLE_init
};
