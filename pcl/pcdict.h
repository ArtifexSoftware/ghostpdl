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
