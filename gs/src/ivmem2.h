/* Copyright (C) 1999 Aladdin Enterprises.  All rights reserved.
  
  This software is licensed to a single customer by Artifex Software Inc.
  under the terms of a specific OEM agreement.
*/

/*$RCSfile$ $Revision$ */
/* VM control user parameter procedures */

#ifndef ivmem2_INCLUDED
#  define ivmem2_INCLUDED

/* Exported by zvmem2.c for zusparam.c */
int set_vm_reclaim(P2(i_ctx_t *, long));
int set_vm_threshold(P2(i_ctx_t *, long));

#endif /* ivmem2_INCLUDED */
