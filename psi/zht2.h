/* Copyright (C) 2001-2023 Artifex Software, Inc.
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


/* Level 2 sethalftone support */

#ifndef zht2_INCLUDED
#  define zht2_INCLUDED

#include "gscspace.h"            /* for gs_separation_name */

/*
 * This routine translates a gs_separation_name value into a character string
 * pointer and a string length.
 */
int gs_get_colorname_string(gs_gstate *pgs,
                            gs_separation_name colorname_index,
                            unsigned char **ppstr,
                            unsigned int *pname_size);

#endif /* zht2_INCLUDED */
