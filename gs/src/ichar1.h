/* Copyright (C) 1998, 1999 Aladdin Enterprises.  All rights reserved.

   This software is licensed to a single customer by Artifex Software Inc.
   under the terms of a specific OEM agreement.
 */

/*$RCSfile$ $Revision$ */
/* Type 1 / Type 2 character rendering operator entry */

#ifndef ichar1_INCLUDED
#  define ichar1_INCLUDED

/* Render a Type 1 or Type 2 outline. */
/* This is the entire implementation of the .type1/2execchar operators. */
int charstring_execchar(P2(i_ctx_t *i_ctx_p, int font_type_mask));

#endif /* ichar1_INCLUDED */
