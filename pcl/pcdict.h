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

/* pcdict.h - PCL utilities for interfacing the PL's dictionary mechanism */

#ifndef pcdict_INCLUDED
#define pcdict_INCLUDED

#include "gx.h"


/* Define the type for an ID key used in a dictionary. */
typedef struct pcl_id_s {
    uint    value;
    byte    key[2];	/* key for dictionaries */
} pcl_id_t;

#define id_key(id)      ((id).key)
#define id_value(id)    ((id).value)

#define id_set_key(id, bytes)                           \
    ( (id).key[0] = (bytes)[0],                         \
      (id).key[1] = (bytes)[1],                         \
      (id).value = ((id).key[0] << 8) + (id).key[1] )

#define id_set_value(id, val)   \
    ( (id).value = (val), (id).key[0] = (val) >> 8, (id).key[1] = (byte)(val) )

#endif			/* pcdict_INCLUDED */
