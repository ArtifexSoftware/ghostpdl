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


/* Definitions for RunLength filters */
/* Requires scommon.h; strimpl.h if any templates are referenced */

#ifndef srlx_INCLUDED
#  define srlx_INCLUDED

#include "scommon.h"

/* Common state */
#define stream_RL_state_common\
        stream_state_common;\
        bool EndOfData		/* true if 128 = EOD */

/* RunLengthEncode */
typedef struct stream_RLE_state_s {
    stream_RL_state_common;
    /* The following parameters are set by the client. */
    ulong record_size;
    bool omitEOD;
    /* The following change dynamically. */
    ulong record_left;		/* bytes left in current record */
    byte n0;
    byte n1;
    byte n2;
    byte state;
    int run_len;
    byte literals[128];
} stream_RLE_state;

#define private_st_RLE_state()	/* in srle.c */\
  gs_private_st_simple(st_RLE_state, stream_RLE_state, "RunLengthEncode state")
/* We define the initialization procedure here, so that clients */
/* can avoid a procedure call. */
#define s_RLE_set_defaults_inline(ss)\
  ((ss)->EndOfData = true, (ss)->omitEOD = false, (ss)->record_size = 0)
#define s_RLE_init_inline(ss)\
  ((ss)->record_left =\
   ((ss)->record_size == 0 ? ((ss)->record_size = max_uint) :\
    (ss)->record_size),\
   (ss)->n0=(ss)->n1=(ss)->n2=0,\
   (ss)->run_len=0,\
   (ss)->state=0)
extern const stream_template s_RLE_template;

/* RunLengthDecode */
typedef struct stream_RLD_state_s {
    stream_RL_state_common;
    /* The following change dynamically. */
    int copy_left;		/* # of output bytes waiting to be produced */
    int copy_data;		/* -1 if copying, repeated byte if repeating */
} stream_RLD_state;

#define private_st_RLD_state()	/* in srld.c */\
  gs_private_st_simple(st_RLD_state, stream_RLD_state, "RunLengthDecode state")
/* We define the initialization procedure here, so that clients */
/* can avoid a procedure call. */
#define s_RLD_set_defaults_inline(ss)\
  ((ss)->EndOfData = true)
#define s_RLD_init_inline(ss)\
  ((ss)->min_left = ((ss)->EndOfData ? 1 : 0), (ss)->copy_left = 0)
extern const stream_template s_RLD_template;

#endif /* srlx_INCLUDED */
