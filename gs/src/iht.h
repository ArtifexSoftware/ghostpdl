/* Copyright (C) 1997 Aladdin Enterprises.  All rights reserved.
  
  This software is licensed to a single customer by Artifex Software Inc.
  under the terms of a specific OEM agreement.
*/

/*$RCSfile$ $Revision$ */
/* Procedures exported by zht.c for zht1.c and zht2.c */

#ifndef iht_INCLUDED
#  define iht_INCLUDED

int zscreen_params(P2(os_ptr op, gs_screen_halftone * phs));

int zscreen_enum_init(P7(i_ctx_t *i_ctx_p, const gx_ht_order * porder,
			 gs_screen_halftone * phs, ref * pproc, int npop,
			 op_proc_t finish_proc, gs_memory_t * mem));

#endif /* iht_INCLUDED */
