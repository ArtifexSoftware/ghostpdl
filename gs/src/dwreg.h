/* Copyright (C) 2001, Ghostgum Software Pty Ltd.  All rights reserved.
  
   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */

/* $Id$ */

#ifndef dwreg_INCLUDED
#  define dwreg_INCLUDED

/* Get and set named registry values for Ghostscript application. */
int win_get_reg_value(const char *name, char *ptr, int *plen);
int win_set_reg_value(const char *name, const char *value);

#endif /* dwreg_INCLUDED */
