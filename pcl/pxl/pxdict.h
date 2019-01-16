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


/* pxdict.h */
/* Dictionary interface for PCL XL parser */
/* Requires gsmemory.h */

#ifndef pxdict_INCLUDED
#  define pxdict_INCLUDED

#include "pxvalue.h"
#include "pldict.h"

/*
 * We use dictionaries to catalog 3 kinds of entities: fonts, user-defined
 * streams, and raster patterns.  (For raster patterns, each level of
 * persistence has a separate dictionary.)  The keys are strings (byte or
 * uint16).
 */

typedef pl_dict_value_free_proc_t px_dict_value_free_proc;

#ifndef px_dict_entry_DEFINED
#  define px_dict_entry_DEFINED
typedef pl_dict_entry_t px_dict_entry_t;
#endif
typedef pl_dict_t px_dict_t;

#define st_px_dict st_pl_dict
#define st_px_dict_max_ptrs st_pl_dict_max_ptrs

/* Get the data and size of a byte or uint16 string. */
#define key_data(key)\
  ((key)->value.array.data)
#define key_size(key)\
  ((key)->value.array.size * value_size(key))

/* Initialize a dictionary. */
#define px_dict_init(pdict, memory, free_proc)\
  pl_dict_init(pdict, memory, free_proc)\

/*
 * Look up an entry in a dictionary.  Return true and set *pvalue if found.
 */
#define px_dict_find(pdict, key, pvalue)\
  pl_dict_find(pdict, key_data(key), key_size(key), pvalue)

/*
 * Add an entry to a dictionary.  Return 1 if it replaces an existing entry.
 * Return -1 if we couldn't allocate memory.
 */
#define px_dict_put(pdict, key, value)\
  pl_dict_put(pdict, key_data(key), key_size(key), value)

/*
 * Remove an entry from a dictionary.  Return true if the entry was present.
 */
#define px_dict_undef(pdict, key)\
  pl_dict_undef(pdict, key_data(key), key_size(key))

/*
 * Release a dictionary by freeing all keys, values, and other storage.
 */
#define px_dict_release(pdict)\
  pl_dict_release(pdict)\

#endif /* pxdict_INCLUDED */
