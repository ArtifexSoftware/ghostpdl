/* Copyright (C) 2001-2018 Artifex Software, Inc.
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


/* Client interface to Display PostScript facilities. */

#ifndef gsdps_INCLUDED
#  define gsdps_INCLUDED

/* Device-source images */
#include "gsiparm2.h"
#include "gsgstate.h"

/* View clipping */
int gs_initviewclip(gs_gstate *);
int gs_eoviewclip(gs_gstate *);
int gs_viewclip(gs_gstate *);
int gs_viewclippath(gs_gstate *);

#endif /* gsdps_INCLUDED */
