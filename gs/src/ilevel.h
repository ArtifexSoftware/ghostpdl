/* Copyright (C) 1992, 1998, 1999 Aladdin Enterprises.  All rights reserved.
  
  This software is licensed to a single customer by Artifex Software Inc.
  under the terms of a specific OEM agreement.
*/

/*$RCSfile$ $Revision$ */
/* Interpreter language level interface */

#ifndef ilevel_INCLUDED
#  define ilevel_INCLUDED

/* The current interpreter language level */
#define LANGUAGE_LEVEL (i_ctx_p->language_level)
#define LL2_ENABLED (LANGUAGE_LEVEL >= 2)
#define LL3_ENABLED (LANGUAGE_LEVEL >= 3)
#define level2_enabled LL2_ENABLED	/* backward compatibility */

#endif /* ilevel_INCLUDED */
