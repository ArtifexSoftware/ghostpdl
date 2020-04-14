/* Copyright (C) 2001-2020 Artifex Software, Inc.
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


/* pcdict.h - PCL utilities for interfacing the PL's dictionary mechanism */

#ifndef pcdict_INCLUDED
#define pcdict_INCLUDED

#include "gx.h"

/* Define the type for an ID key used in a dictionary. */
typedef struct pcl_id_s
{
    uint value;
    byte key[2];                /* key for dictionaries */
} pcl_id_t;

#define id_key(id)      ((id).key)
#define id_value(id)    ((id).value)

#define id_set_key(id, bytes)                           \
    ( (id).key[0] = (bytes)[0],                         \
      (id).key[1] = (bytes)[1],                         \
      (id).value = ((id).key[0] << 8) + (id).key[1] )

#define id_set_value(id, val)   \
    ( (id).value = (val), (id).key[0] = (val) >> 8, (id).key[1] = (byte)(val) )

#endif /* pcdict_INCLUDED */
