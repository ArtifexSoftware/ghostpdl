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


/* pldict.h */
/* Dictionary interface for PCL parsers */
/* Requires gsmemory.h */

#ifndef pldict_INCLUDED
#  define pldict_INCLUDED

/*
 * We use dictionaries to catalog various kinds of entities.  The keys are
 * strings; the values are 'objects'.  The current implementation is slow
 * but simple.
 *
 * Dictionaries can be stacked.  Lookups search the stack, but additions,
 * deletions, and replacements always work in the top dictionary.
 *
 * The implementation copies keys, but does not copy values.  Instead, it
 * calls a client-supplied freeing procedure whenever a value must be freed
 * (by being deleted or replaced).  The prototype for this procedure is
 * identical to that of gs_free_object (which, however, is a macro.)
 */
typedef void (*pl_dict_value_free_proc_t) (gs_memory_t *, void *,
                                           client_name_t);
#ifndef pl_dict_entry_DEFINED
#  define pl_dict_entry_DEFINED
typedef struct pl_dict_entry_s pl_dict_entry_t;
#endif
typedef struct pl_dict_s pl_dict_t;

struct pl_dict_s
{
    pl_dict_entry_t *entries;
    uint entry_count;
    pl_dict_value_free_proc_t free_proc;
    pl_dict_t *parent;          /* next dictionary up the stack */
    gs_memory_t *memory;
};

#ifdef extern_st
extern_st(st_pl_dict);          /* only for embedders */
#endif
#define public_st_pl_dict()	/* in pldict.c */\
  gs_public_st_ptrs2(st_pl_dict, pl_dict_t, "pl_dict_t",\
    pl_dict_enum_ptrs, pl_dict_reloc_ptrs, entries, parent)
#define st_pl_dict_max_ptrs 2

/*
 * Define the maximum length of keys stored in the dictionary entries
 * themselves.  This is a time/space tradeoff.
 */
#define pl_dict_max_short_key 16

/* Initialize a dictionary. */
void pl_dict_init(pl_dict_t * pdict, gs_memory_t * memory,
                  pl_dict_value_free_proc_t free_proc);

/*
 * Look up an entry in a dictionary, optionally searching the stack, and
 * optionally returning a pointer to the actual dictionary where the
 * entry was found.  Return true, setting *pvalue (and, if ppdict is not
 * NULL, *ppdict), if found.
 */
bool pl_dict_lookup(pl_dict_t * pdict, const byte * kdata, uint ksize,
                    void **pvalue, bool with_stack, pl_dict_t ** ppdict);
#define pl_dict_find(pdict, kdata, ksize, pvalue)\
  pl_dict_lookup(pdict, kdata, ksize, pvalue, true, (pl_dict_t **)0)

#define pl_dict_find_no_stack(pdict, kdata, ksize, pvalue)\
  pl_dict_lookup(pdict, kdata, ksize, pvalue, false, (pl_dict_t **)0)

/*
 * Add an entry to a dictionary.  Return 1 if it replaces an existing entry.
 * Return -1 if we couldn't allocate memory.
 */
int pl_dict_put(pl_dict_t * pdict, const byte * kdata, uint ksize,
                void *value);

/*
 * When a dictionary entry is created, it can be designated as being a
 * synonym or alias of an existing entry, rather than having a value
 * of its own.  All entries in a synonym group are equivalent: there
 * is no distinction between the "original" entry and subsequent ones,
 * and synonymy is commutative and transitive.  All entries in a
 * synonym group have the same value: when the value of one member of
 * a group is changed, the values of all members of the group are
 * changed.  When any member of a synonym group is deleted, all
 * members of the group are deleted.  (Note that the client-supplied
 * freeing procedure is only called once in the case of a change or
 * deletion.)  When deleting an entry that is potentially a synonym or
 * an entry that has associated synonyms the client should use
 * pl_undef_purge_synonyms() (see below).
 */

int pl_dict_put_synonym(pl_dict_t * pdict, const byte * old_kdata,
                        uint old_ksize, const byte * new_kdata,
                        uint new_ksize);

/*
 * Remove an entry from a dictionary.  Return true if the entry was present.
 */
bool pl_dict_undef(pl_dict_t * pdict, const byte * kdata, uint ksize);

/*
 * Get or set the parent of a dictionary.
 */
#define pl_dict_parent(pdict) ((pdict)->parent)
#define pl_dict_set_parent(pdict, pardict) ((pdict)->parent = (pardict))

/*
 * Return the number of entries in a dictionary.
 */
uint pl_dict_length(const pl_dict_t * pdict, bool with_stack);

/*
 * Enumerate a dictionary.  If entries are added during enumeration,
 * they may or may not be enumerated; it is OK to delete entries that have
 * already been enumerated (including the current entry), but not
 * entries that have not been enumerated yet.  Use as follows:
 *	gs_const_string keyvar;
 *	void *valuevar;
 *	pl_dict_enum_t denum;
 *	...
 *	pl_dict_enum_begin(pdict, &denum);
 *	    -or-
 *	pl_dict_enum_stack_begin(pdict, &denum, with_stack);
 *	while ( pl_dict_enum_next(&denum, &keyvar, &valuevar) )
 *	  ... process <keyvar, valuevar> ...
 */
typedef struct pl_dict_enum_s
{
    const pl_dict_t *pdict;
    pl_dict_entry_t *next;
    bool first;
    const pl_dict_t *next_dict;
} pl_dict_enum_t;

void pl_dict_enum_stack_begin(const pl_dict_t * pdict, pl_dict_enum_t * penum,
                              bool with_stack);
#define pl_dict_enum_begin(pdict, penum)\
  pl_dict_enum_stack_begin(pdict, penum, true)
bool pl_dict_enum_next(pl_dict_enum_t * penum, gs_const_string * pkey,
                       void **pvalue);

/*
 * Release a dictionary by freeing all keys, values, and other storage.
 */
void pl_dict_release(pl_dict_t * pdict);

/*
 * Delete an entry that is a synonym or a canonical entry that has
 * related synonyms.  The entire group of entries is deleted.  Note
 * that this routine should be used in liueu of pl_dict_undef() if
 * synonyms are potentially used.
 */
void pl_dict_undef_purge_synonyms(pl_dict_t * pdict, const byte * kdata,
                                  uint ksize);

#endif /* pldict_INCLUDED */
