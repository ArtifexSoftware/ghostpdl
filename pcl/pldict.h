/* Copyright (C) 1996 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
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
typedef void (*pl_dict_value_free_proc_t)(P3(gs_memory_t *, void *,
					     client_name_t));
#ifndef pl_dict_entry_DEFINED
#  define pl_dict_entry_DEFINED
typedef struct pl_dict_entry_s pl_dict_entry_t;
#endif
typedef struct pl_dict_s pl_dict_t;
struct pl_dict_s {
  pl_dict_entry_t *entries;
  pl_dict_value_free_proc_t free_proc;
  pl_dict_t *parent;		/* next dictionary up the stack */
  gs_memory_t *memory;
};
#ifdef extern_st
extern_st(st_pl_dict);		/* only for embedders */
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
void pl_dict_init(P3(pl_dict_t *pdict, gs_memory_t *memory,
		     pl_dict_value_free_proc_t free_proc));

/*
 * Look up an entry in a dictionary.  Return true and set *pvalue if found.
 */
bool pl_dict_find(P4(pl_dict_t *pdict, const byte *kdata, uint ksize,
		     void **pvalue));

/*
 * Add an entry to a dictionary.  Return 1 if it replaces an existing entry.
 * Return -1 if we couldn't allocate memory.
 */
int pl_dict_put(P4(pl_dict_t *pdict, const byte *kdata, uint ksize,
		   void *value));

/*
 * Remove an entry from a dictionary.  Return true if the entry was present.
 */
bool pl_dict_undef(P3(pl_dict_t *pdict, const byte *kdata, uint ksize));

/*
 * Get or set the parent of a dictionary.
 */
#define pl_dict_parent(pdict) ((pdict)->parent)
#define pl_dict_set_parent(pdict, pardict) ((pdict)->parent = (pardict))

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
typedef struct pl_dict_enum_s {
  const pl_dict_t *pdict;
  pl_dict_entry_t *next;
  bool first;
  const pl_dict_t *next_dict;
} pl_dict_enum_t;
void pl_dict_enum_stack_begin(P3(const pl_dict_t *pdict, pl_dict_enum_t *penum,
				 bool with_stack));
#define pl_dict_enum_begin(pdict, penum)\
  pl_dict_enum_stack_begin(pdict, penum, true)
bool pl_dict_enum_next(P3(pl_dict_enum_t *penum, gs_const_string *pkey,
			  void **pvalue));

/*
 * Release a dictionary by freeing all keys, values, and other storage.
 */
void pl_dict_release(P1(pl_dict_t *pdict));

#endif				/* pldict_INCLUDED */
