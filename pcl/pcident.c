/*
 * Copyright (C) 1998 Aladdin Enterprises.
 * All rights reserved.
 *
 * This file is part of Aladdin Ghostscript.
 *
 * Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
 * or distributor accepts any responsibility for the consequences of using it,
 * or for whether it serves any particular purpose or works at all, unless he
 * or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
 * License (the "License") for full details.
 *
 * Every copy of Aladdin Ghostscript must include a copy of the License,
 * normally in a plain ASCII text file named PUBLIC.  The License grants you
 * the right to copy, modify and redistribute Aladdin Ghostscript, but only
 * under certain conditions described in the License.  Among other things, the
 * License requires that the copyright notice and this notice be preserved on
 * all copies.
 */

/* pcident.c - object identification mechanism for PCL */

#include "gx.h"
#include "gsuid.h"
#include "pcident.h"

private pcl_gsid_t  next_id;

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
pcl_next_id(void)
{
    if (next_id == 0)
        next_id = 16 * 1024 * 1024;
    else if (next_id == no_UniqueID)
        ++next_id;
    return next_id++;
}

/*
 * Initialize the next_id value.
 */
  void
pcl_init_id(void)
{
    next_id = 8UL;
}
