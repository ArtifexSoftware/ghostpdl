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


/* pcident.c - object identification mechanism for PCL */

#include "gx.h"
#include "gsuid.h"
#include "pcident.h"
#include "pcstate.h"

/*
 * Return the next unique identifier.
 *
 * Identifiers start at 8 to try to expose errors that might be masked by an
 * identifier of 0. In the unlikely event that so many identifiers are created
 * that the number wraps back to 0, the next assigned identifier will be
 * 16 * 1024 * 1024. This avoids potential overlap with permanent objects that
 * are assigned identifiers at initialization time.
 *
 * Special handling is also provided to avoid the no_UniqueID value used by
 * the graphic library.
 */
pcl_gsid_t
pcl_next_id(pcl_state_t * pcs)
{
    if (pcs->next_id == 0)
        pcs->next_id = 16 * 1024 * 1024;
    else if (pcs->next_id == no_UniqueID)
        ++(pcs->next_id);
    return (pcs->next_id)++;
}
