/* Copyright (C) 1996, 1997, 1998 Aladdin Enterprises.  All rights
   reserved.  Unauthorized use, copying, and/or distribution
   prohibited.  */

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
