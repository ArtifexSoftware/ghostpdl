/* Copyright (C) 1996 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

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
  byte short_key[pl_dict_max_short_key];
};
#define entry_key_data(pde)\
  ((pde)->key.size <= pl_dict_max_short_key ? (pde)->short_key : (pde)->key.data)

/* GC descriptors */
public_st_pl_dict();
gs_private_st_composite(st_pl_dict_entry, pl_dict_entry_t, "pl_dict_entry_t",
  pl_dict_entry_enum_ptrs, pl_dict_entry_reloc_ptrs);
#define pde ((pl_dict_entry_t *)vptr)
private ENUM_PTRS_BEGIN(pl_dict_entry_enum_ptrs) return 0;
	ENUM_CONST_STRING_PTR(0, pl_dict_entry_t, key);
	ENUM_PTR(1, pl_dict_entry_t, value);
	ENUM_PTR(2, pl_dict_entry_t, next);
ENUM_PTRS_END
private RELOC_PTRS_BEGIN(pl_dict_entry_reloc_ptrs) {
	RELOC_CONST_STRING_PTR(pl_dict_entry_t, key);
	RELOC_PTR(pl_dict_entry_t, value);
	RELOC_PTR(pl_dict_entry_t, next);
} RELOC_PTRS_END
#undef pde

/* ---------------- Utilities ---------------- */

/* Provide a standard procedure for freeing a value. */
private void
pl_dict_value_free(gs_memory_t *mem, void *value, client_name_t cname)
{	gs_free_object(mem, value, cname);
}

/*
 * Find an entry in a dictionary.  Return a pointer to the pointer to the
 * entry.
 */
private pl_dict_entry_t **
pl_dict_lookup(pl_dict_t *pdict, const byte *kdata, uint ksize)
{	pl_dict_entry_t **ppde = &pdict->entries;
	pl_dict_entry_t *pde;

	for ( ; (pde = *ppde) != 0; ppde = &pde->next )
	  { if ( pde->key.size == ksize &&
		 !memcmp(entry_key_data(pde), kdata, ksize)
	       )
	      return ppde;
	  }
	return false;
}

/* Delete a dictionary entry. */
private void
pl_dict_free(pl_dict_t *pdict, pl_dict_entry_t **ppde, client_name_t cname)
{	pl_dict_entry_t *pde = *ppde;
	gs_memory_t *mem = pdict->memory;

	*ppde = pde->next;
	(*pdict->free_proc)(mem, pde->value, cname);
	if ( pde->key.size > pl_dict_max_short_key )
	  gs_free_string(mem, (byte *)pde->key.data, pde->key.size, cname);
	gs_free_object(mem, pde, cname);
}

/* ---------------- API procedures ---------------- */

/* Initialize a dictionary. */
void
pl_dict_init(pl_dict_t *pdict, gs_memory_t *mem,
  pl_dict_value_free_proc_t free_proc)
{	pdict->memory = mem;
	pdict->free_proc = (free_proc ? free_proc : pl_dict_value_free);
	pdict->entries = 0;
	pdict->parent = 0;
}

/*
 * Look up an entry in a dictionary.  Return true and set *ppvalue if found.
 * This routine, and only this one, searches the stack.
 */
bool
pl_dict_find(pl_dict_t *pdict, const byte *kdata, uint ksize, void **pvalue)
{	pl_dict_t *pdcur = pdict;
	pl_dict_entry_t **ppde;

	while ( (ppde = pl_dict_lookup(pdcur, kdata, ksize)) == 0 )
	  { if ( (pdcur = pdcur->parent) == 0 )
	      return false;
	  }
	*pvalue = (*ppde)->value;
	return true;
}

/*
 * Add an entry to a dictionary.  Return 1 if it replaces an existing entry.
 * Return -1 if we couldn't allocate memory.  pl_dict_put copies the key
 * string, but takes ownership of the value object (i.e., it will free it
 * when the entry is deleted, using the free_proc).
 */
bool
pl_dict_put(pl_dict_t *pdict, const byte *kdata, uint ksize, void *value)
{	pl_dict_entry_t **ppde = pl_dict_lookup(pdict, kdata, ksize);
	gs_memory_t *mem = pdict->memory;
	pl_dict_entry_t *pde;

	if ( !ppde )
	  { /* Make a new entry. */
	    byte *kstr;

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
	    pde->value = value;
	    pde->next = pdict->entries;
	    pdict->entries = pde;
	    return 0;
	  }
	/* Replace the value in an existing entry. */
	pde = *ppde;
	(*pdict->free_proc)(pdict->memory, pde->value,
			    "pl_dict_put(old value)");
	pde->value = value;
	return 1;
}

/*
 * Remove an entry from a dictionary.  Return true if the entry was present.
 */
bool
pl_dict_undef(pl_dict_t *pdict, const byte *kdata, uint ksize)
{	pl_dict_entry_t **ppde = pl_dict_lookup(pdict, kdata, ksize);

	if ( !ppde )
	  return false;
	pl_dict_free(pdict, ppde, "pl_dict_undef");
	return true;
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
