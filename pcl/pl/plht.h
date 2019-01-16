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


/* plht.h - shared (pcl + pxl) haltone header */

#ifndef plht_INCLUDED
#  define plht_INCLUDED

int pl_set_pcl_halftone(gs_gstate * pgs, gs_mapping_proc transfer_proc,
                        int width, int height,
                        gs_string threshold_data, int phase_x, int phase_y);

#endif /* plht_INCLUDED */
