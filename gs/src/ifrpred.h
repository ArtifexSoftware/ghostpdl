/* Copyright (C) 1999 Aladdin Enterprises.  All rights reserved.

   This software is licensed to a single customer by Artifex Software Inc.
   under the terms of a specific OEM agreement.
 */

/*$RCSfile$ $Revision$ */
/* filter_read_predictor prototype */

#ifndef ifrpred_INCLUDED
#  define ifrpred_INCLUDED

/* Exported by zfdecode.c for zfzlib.c */
int filter_read_predictor(P4(i_ctx_t *i_ctx_p, int npop,
			     const stream_template * template,
			     stream_state * st));

#endif /* ifrpred_INCLUDED */
