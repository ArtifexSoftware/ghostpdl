/* Portions Copyright (C) 2001 artofcode LLC.
   Portions Copyright (C) 1996, 2001 Artifex Software Inc.
   Portions Copyright (C) 1988, 2000 Aladdin Enterprises.
   This software is based in part on the work of the Independent JPEG Group.
   All Rights Reserved.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */
/*$Id$ */

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
pcl_next_id(pcl_state_t *pcs)
{
    if (pcs->next_id == 0)
        pcs->next_id = 16 * 1024 * 1024;
    else if (pcs->next_id == no_UniqueID)
        ++(pcs->next_id);
    return (pcs->next_id)++;
}
