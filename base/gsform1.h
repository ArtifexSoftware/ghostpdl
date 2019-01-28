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


/* Client interface to FormType 1 Forms */

#ifndef gsform1_h_INCLUDED
#  define gsform1_h_INCLUDED

#include "gstypes.h"
#include "gsmatrix.h"
#include "gxpath.h"
#include "gsgstate.h"


typedef struct gs_form_template_s {
    gs_rect BBox;
    gs_matrix form_matrix;
    gs_matrix CTM;
    gx_clip_path * pcpath;
    int FormID;
    gs_gstate *pgs;
} gs_form_template_t;

#endif
