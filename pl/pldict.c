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

/* pldict.h */
/* Dictionary implementation for PCL parsers */

#include "memory_.h"
#include "gstypes.h"
#include "gsmemory.h"
#include "gsstruct.h"
#include "pldict.h"

/*
 * The current implementation of dictionaries is a simple linked list.  The
 * only concession to efficiency is that we store keys of up to
 * pl_dict_max_short_key characters in the node itself rather than a separately
 * allocated string.
 */
struct pl_dict_entry_s {
  gs_const_string key;		/* data pointer = 0 if short key */
  void *value;
  pl_dict_entry_t *next;
  pl_dict_entry_t *link;       /* a link to the actual entry (supports aliases). */
  byte short_key[pl_dict_max_short_key];
};
#define entry_key_data(pde)\
  ((pde)->key.size <= pl_dict_max_short_key ? (pde)->short_key : (pde)->key.data)

/* GC descriptors */
public_st_pl_dict();
gs_private_st_composite(st_pl_dict_entry, pl_dict_entry_t, "pl_dict_entry_t",
  pl_dict_entry_enum_ptrs, pl_dict_entry_reloc_ptrs);
#define pde ((pl_dict_entry_t *)vptr)
static ENUM_PTRS_BEGIN(pl_dict_entry_enum_ptrs) return 0;
     /*	ENUM_CONST_STRING_PTR(0, pl_dict_entry_t, key); */
	ENUM_PTR(1, pl_dict_entry_t, value);
	ENUM_PTR(2, pl_dict_entry_t, next);
	ENUM_PTR(3, pl_dict_entry_t, link);
ENUM_PTRS_END
static RELOC_PTRS_BEGIN(pl_dict_entry_reloc_ptrs) {
    /*	RELOC_CONST_STRING_PTR(pl_dict_entry_t, key); */
	RELOC_PTR(pl_dict_entry_t, value);
	RELOC_PTR(pl_dict_entry_t, next);
	RELOC_PTR(pl_dict_entry_t, link);
} RELOC_PTRS_END
#undef pde

/* ---------------- Utilities ---------------- */

/* Provide a standard procedure for freeing a value. */
static void
pl_dict_value_free(gs_memory_t *mem, void *value, client_name_t cname)
{	gs_free_object(mem, value, cname);
}

/*
 * Look up an entry in a dictionary.  Return a pointer to the pointer to the
 * entry.
 */
static pl_dict_entry_t **
pl_dict_lookup_entry(pl_dict_t *pdict, const byte *kdata, uint ksize)
{	pl_dict_entry_t **ppde = &pdict->entries;
	pl_dict_entry_t *pde;
	for ( ; (pde = *ppde) != 0; ppde = &pde->next )
	  { if ( pde->key.size == ksize &&
		 !memcmp(entry_key_data(pde), kdata, ksize)
	       )
	      return ppde;
	  }
	return 0;
}

/* Delete a dictionary entry. */
static void
pl_dict_free(pl_dict_t *pdict, pl_dict_entry_t **ppde, client_name_t cname)
{	pl_dict_entry_t *pde = *ppde;
	gs_memory_t *mem = pdict->memory;

	*ppde = pde->next;
	if ( !pde->link ) /* values are not freed for links */
	  (*pdict->free_proc)(mem, pde->value, cname);
	if ( pde->key.size > pl_dict_max_short_key )
	  gs_free_string(mem, (byte *)pde->key.data, pde->key.size, cname);
	gs_free_object(mem, pde, cname);
	pdict->entry_count--;
}

/* ---------------- API procedures ---------------- */

/* Initialize a dictionary. */
void
pl_dict_init(pl_dict_t *pdict, gs_memory_t *mem,
  pl_dict_value_free_proc_t free_proc)
{	pdict->memory = mem;
	pdict->free_proc = (free_proc ? free_proc : pl_dict_value_free);
	pdict->entries = 0;
	pdict->entry_count = 0;
	pdict->parent = 0;
}

/*
 * Look up an entry in a dictionary, optionally searching the stack, and
 * optionally returning a pointer to the actual dictionary where the
 * entry was found.  Return true, setting *pvalue (and, if ppdict is not
 * NULL, *ppdict), if found.  Note that this is the only routine that
 * searches the stack.
 */
bool
pl_dict_lookup(pl_dict_t *pdict, const byte *kdata, uint ksize, void **pvalue,
  bool with_stack, pl_dict_t **ppdict)
{	pl_dict_t *pdcur = pdict;
	pl_dict_entry_t **ppde;

	while ( (ppde = pl_dict_lookup_entry(pdcur, kdata, ksize)) == 0 )
	  { if ( !with_stack || (pdcur = pdcur->parent) == 0 )
	      return false;
	  }
	*pvalue = (*ppde)->value;
	if ( ppdict )
	  *ppdict = pdcur;
	return true;
}

/*
 * make a new dictionary entry.
 */
static int
pl_dict_build_new_entry(pl_dict_t *pdict, const byte *kdata, uint ksize,
			void *value, pl_dict_entry_t *link)
{	/* Make a new entry. */
        byte *kstr;
	gs_memory_t *mem = pdict->memory;
	pl_dict_entry_t *pde;
	pde = gs_alloc_struct(mem, pl_dict_entry_t, &st_pl_dict_entry,
			      "pl_dict_put(entry)");
	kstr = (ksize <= pl_dict_max_short_key ? pde->short_key :
		gs_alloc_string(mem, ksize, "pl_dict_put(key)"));
	if ( pde == 0 || kstr == 0 )
	  { if ( kstr && kstr != pde->short_key )
	      gs_free_string(mem, kstr, ksize, "pl_dict_put(key)");
	      gs_free_object(mem, pde, "pl_dict_put(entry)");
	      return -1;
	  }
	memcpy(kstr, kdata, ksize);
	pde->key.data = (ksize <= pl_dict_max_short_key ? 0 : kstr);
	pde->key.size = ksize;
	pde->link = link;
	pde->value = value;
	pde->next = pdict->entries;
	pdict->entries = pde;
	pdict->entry_count++;
	return 0;
}

/*
 * Add an entry to a dictionary.  Return 1 if it replaces an existing entry.
 * Return -1 if we couldn't allocate memory.  pl_dict_put copies the key
 * string, but takes ownership of the value object (i.e., it will free it
 * when the entry is deleted, using the free_proc).
 */
int
pl_dict_put(pl_dict_t *pdict, const byte *kdata, uint ksize, void *value)
{	pl_dict_entry_t **ppde = pl_dict_lookup_entry(pdict, kdata, ksize);
	if ( !ppde )
	  { void *link = 0;
	    return pl_dict_build_new_entry(pdict, kdata, ksize, value, link);
	  }
	else
	  { /* Replace the value in an existing entry. */
	    pl_dict_entry_t *pde;
	    pde = *ppde;
	    (*pdict->free_proc)(pdict->memory, pde->value,
				"pl_dict_put(old value)");
	    pde->value = value;
	    return 1;
	  }
}

/*
 * link entry or alias
 */
int
pl_dict_put_synonym(pl_dict_t *pdict, const byte *old_kdata, uint old_ksize,
		 const byte *new_kdata, uint new_ksize)
{	pl_dict_entry_t **old_ppde = pl_dict_lookup_entry(pdict, old_kdata, old_ksize);
	pl_dict_entry_t *old_pde;
	pl_dict_entry_t **new_ppde = pl_dict_lookup_entry(pdict, new_kdata, new_ksize);
	/* old value doesn't exist or new value does exist */
	if ( !old_ppde || new_ppde )
	     return -1;
	/* find the original data if this is a link to a link */
	old_pde = *old_ppde;
	if ( old_pde->link != 0 )
	  old_pde = old_pde->link;

	return pl_dict_build_new_entry(pdict, new_kdata, new_ksize,
				       old_pde->value, old_pde);
}

/* 
 * Purge alias entries.  A bit tricky but this doesn't fowl the
 * enumeration code since links are always prior to their entries.  We
 * insert at the head of the list and a real entry must be present to
 * insert a link.  Also deleting an entry deletes *all* associated
 * links and deleting a link deletes the corresponding entry.
 */
void
pl_dict_undef_purge_synonyms(pl_dict_t *pdict, const byte *kdata, uint ksize)
{	pl_dict_entry_t **ppde = &pdict->entries;
	pl_dict_entry_t **pptarget = pl_dict_lookup_entry(pdict, kdata, ksize);
	pl_dict_entry_t *pde;
	pl_dict_entry_t *ptarget;

	if ( !pptarget )
	  return;
	ptarget = *pptarget;
	/* get the real entry if this is a link. */
	if ( ptarget->link )
	  ptarget = ptarget->link;
#define dict_get_key_data(entry)  ((entry)->key.size > pl_dict_max_short_key ?\
  (entry)->key.data : (entry)->short_key)
	pl_dict_undef(pdict, dict_get_key_data(ptarget), ptarget->key.size);
	/* delete links to the target */
	pde = *ppde;
	while ( pde )
	  {
	    pl_dict_entry_t *npde = pde->next; /* next entry */
	    if ( pde->link && pde->link == ptarget )
	      pl_dict_undef(pdict, dict_get_key_data(pde), pde->key.size);
	    pde = npde;
	  }
#undef dict_get_key_data
}

/*
 * Remove an entry from a dictionary.  Return true if the entry was present.
 */
bool
pl_dict_undef(pl_dict_t *pdict, const byte *kdata, uint ksize)
{	pl_dict_entry_t **ppde = pl_dict_lookup_entry(pdict, kdata, ksize);

	if ( !ppde )
	  return false;
	pl_dict_free(pdict, ppde, "pl_dict_undef");
	return true;
}

/*
 * Return the number of entries in a dictionary.
 */
uint
pl_dict_length(const pl_dict_t *pdict, bool with_stack)
{	uint count = pdict->entry_count;

	if ( with_stack )
	  { const pl_dict_t *pdcur;
	    for ( pdcur = pdict->parent; pdcur != 0; pdcur = pdcur->parent )
	      count += pdcur->entry_count;
	  }
	return count;
}

/*
 * Enumerate a dictionary with or without its stack.
 * See pldict.h for details.
 */
void
pl_dict_enum_stack_begin(const pl_dict_t *pdict, pl_dict_enum_t *penum,
  bool with_stack)
{	penum->pdict = pdict;
	penum->next = 0;
	penum->first = true;
	penum->next_dict = (with_stack ? pdict->parent : 0);
}

bool
pl_dict_enum_next(pl_dict_enum_t *penum, gs_const_string *pkey,
  void **pvalue)
{	pl_dict_entry_t *pde;
	while ( (pde = (penum->first ? penum->pdict->entries : penum->next)) == 0 )
	  { if ( penum->next_dict == 0 )
	      return false;
	    penum->next_dict = (penum->pdict = penum->next_dict)->parent;
	    penum->first = true;
	  }
	pkey->data = entry_key_data(pde);
	pkey->size = pde->key.size;
	*pvalue = pde->value;
	penum->next = pde->next;
	penum->first = false;
	return true;
}

/*
 * Release a dictionary by freeing all keys, values, and other storage.
 */
void
pl_dict_release(pl_dict_t *pdict)
{	while ( pdict->entries )
	  pl_dict_free(pdict, &pdict->entries, "pl_dict_release");
}
