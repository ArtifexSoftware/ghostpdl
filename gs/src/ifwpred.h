/* Copyright (C) 1999 Aladdin Enterprises.  All rights reserved.
  
  This software is licensed to a single customer by Artifex Software Inc.
  under the terms of a specific OEM agreement.
*/

/*$RCSfile$ $Revision$ */
/* filter_read_predictor prototype */

#ifndef ifwpred_INCLUDED
#  define ifwpred_INCLUDED

/* Exported by zfilter2.c for zfzlib.c */
int filter_write_predictor(P4(i_ctx_t *i_ctx_p, int npop,
			      const stream_template * template,
			      stream_state * st));

#endif /* ifwpred_INCLUDED */
