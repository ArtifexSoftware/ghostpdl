/* Copyright (C) 1996, 1997, 1998 Aladdin Enterprises.  All rights
   reserved.  Unauthorized use, copying, and/or distribution
   prohibited.  */

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
