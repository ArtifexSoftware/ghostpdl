/* Copyright (C) 1995 Aladdin Enterprises.  All rights reserved.
  
  This file is part of Aladdin Ghostscript.
  
  Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
  or distributor accepts any responsibility for the consequences of using it,
  or for whether it serves any particular purpose or works at all, unless he
  or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
  License (the "License") for full details.
  
  Every copy of Aladdin Ghostscript must include a copy of the License,
  normally in a plain ASCII text file named PUBLIC.  The License grants you
  the right to copy, modify and redistribute Aladdin Ghostscript, but only
  under certain conditions described in the License.  Among other things, the
  License requires that the copyright notice and this notice be preserved on
  all copies.
*/

/* gsparam.c */
/* Default implementation of parameter lists */
#include "memory_.h"
#include "string_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsparam.h"
#include "gsstruct.h"

/* Forward references */
typedef union c_param_value_s c_param_value;
/*typedef struct gs_c_param_s gs_c_param;*/	/* in gsparam.h */

/* Define the GC type for a parameter list. */
private_st_c_param_list();

/* Define a union type for parameter values. */
union gs_c_param_value_s {
	bool b;
	int i;
	long l;
	float f;
	gs_param_string s;
	gs_param_int_array ia;
	gs_param_float_array fa;
	gs_param_string_array sa;
	gs_c_param_list d;
};


/* Lengths corresponding to various cpt_xxx types */
const byte gs_param_type_sizes[] = {GS_PARAM_TYPE_SIZES_ELEMENTS};
#define cpt_size(cpt) (gs_param_type_sizes[(int)cpt])

/* Lengths of *actual* data-containing type pointed to or contained by cpt_xxx's */
const int cpt_base_sizeof[] = {CPT_BASE_SIZEOF_ELEMENTS};

/* Define a parameter list element. */
struct gs_c_param_s {
	gs_c_param *next;
	gs_param_name key;
	union gs_c_param_value_s value;
	gs_param_type type;
	void *alternate_typed_data;
};
/* Parameter values aren't really simple, */
/* but since parameter lists are transient, it doesn't matter. */
gs_private_st_ptrs2(st_c_param, gs_c_param, "gs_c_param",
  c_param_enum_ptrs, c_param_reloc_ptrs, next, alternate_typed_data);

/* ================ Utilities ================ */

#define cplist ((gs_c_param_list *)plist)

/* ================ Generic gs_param_list procs =============== */

/* Reset a gs_param_key_t enumerator to its initial state */
void
param_init_enumerator(gs_param_enumerator_t *enumerator)
{	memset( enumerator, 0, sizeof(*enumerator) );
}

/* ================ Writing parameters to a list ================ */

private param_proc_xmit_null(c_param_write_null);
private param_proc_xmit_typed(c_param_write_typed);
private param_proc_xmit_bool(c_param_write_bool);
private param_proc_xmit_int(c_param_write_int);
private param_proc_xmit_long(c_param_write_long);
private param_proc_xmit_float(c_param_write_float);
private param_proc_xmit_string(c_param_write_string);
private param_proc_xmit_int_array(c_param_write_int_array);
private param_proc_xmit_float_array(c_param_write_float_array);
private param_proc_xmit_string_array(c_param_write_string_array);
private param_proc_begin_xmit_dict(c_param_begin_write_dict);
private param_proc_end_xmit_dict(c_param_end_write_dict);
private param_proc_requested(c_param_requested);
private const gs_param_list_procs c_write_procs = {
	c_param_write_null,
	c_param_write_typed,
	c_param_write_bool,
	c_param_write_int,
	c_param_write_long,
	c_param_write_float,
	c_param_write_string,
	c_param_write_string,		/* name = string */
	c_param_write_int_array,
	c_param_write_float_array,
	c_param_write_string_array,
	c_param_write_string_array,	/* name = string */
	c_param_begin_write_dict,
	c_param_end_write_dict,
	NULL,                        /* get_next_key */
	c_param_requested
};

/* Initialize a list for writing. */
void
gs_c_param_list_write(gs_c_param_list *plist, gs_memory_t *mem)
{	plist->procs = &c_write_procs;
	plist->memory = mem;
	plist->head = 0;
	plist->count = 0;
	plist->has_int_keys = 0;
}

/* Release a list. */
void
gs_c_param_list_release(gs_c_param_list *plist)
{	gs_c_param *pparam;
	while ( (pparam = plist->head) != 0 )
	  { gs_c_param *next = pparam->next;
	    switch (pparam->type)
	      {
	    case cpt_dict:
	    case cpt_dict_int_keys:
	        gs_c_param_list_release(&pparam->value.d);
	        break;
	    case cpt_string:
	    case cpt_name:
	    case cpt_int_array:
	    case cpt_float_array:
	    case cpt_string_array:
	    case cpt_name_array:
	        if (!pparam->value.s.persistent)
	          gs_free_object( plist->memory, (void *)pparam->value.s.data,
	           "gs_c_param_list_release data" );
	        break;
	    default:
	        break;
	      }
	    gs_free_object(plist->memory, pparam->alternate_typed_data,
	     "gs_c_param_list_release alternate data");
	    gs_free_object(plist->memory, pparam, "gs_c_param_list_release entry");
	    plist->head = next;
	    plist->count--;
	  }
}

/* Generic routine for writing a parameter to a list. */
private int
c_param_write(gs_c_param_list *plist, gs_param_name pkey, void *pvalue,
  gs_param_type type)
{	unsigned top_level_sizeof = 0;
	unsigned second_level_sizeof = 0;
	gs_c_param *pparam =
	  gs_alloc_struct(plist->memory, gs_c_param, &st_c_param,
			  "c_param_write entry");
	if ( pparam == 0 )
	  return_error(gs_error_VMerror);
	pparam->next = plist->head;
	pparam->key = pkey;
	memcpy(&pparam->value, pvalue, cpt_size(type));
	pparam->type = type;
	pparam->alternate_typed_data = 0;

	/* Need deeper copies of data if it's not persistent */
	switch (type)
	  {
	    gs_param_string const *curr_string;
	    gs_param_string const *end_string;
	case cpt_string_array:
	case cpt_name_array:
	    /* Determine how much mem needed to hold actual string data */
	    curr_string = pparam->value.sa.data;
	    end_string = curr_string + pparam->value.sa.size;
	    for ( ; curr_string < end_string; ++curr_string)
	      if (!curr_string->persistent)
	        second_level_sizeof += curr_string->size;
	    /* fall thru */

	case cpt_string:
	case cpt_name:
	case cpt_int_array:
	case cpt_float_array:
	    if (!pparam->value.s.persistent)
	      { /* Allocate & copy object pointed to by array or string */
	        byte *top_level_memory;
	        top_level_sizeof = pparam->value.s.size * cpt_base_sizeof[type];
	        top_level_memory = gs_alloc_bytes_immovable(plist->memory,
	         top_level_sizeof + second_level_sizeof, "c_param_write data");
	        if (top_level_memory == 0)
	          { gs_free_object(plist->memory, pparam, "c_param_write entry");
	            return_error(gs_error_VMerror);
	          }
	        memcpy(top_level_memory, pparam->value.s.data, top_level_sizeof);
	        pparam->value.s.data = top_level_memory;

	        /* String/name arrays need to copy actual str data */
	        if (second_level_sizeof > 0)
	          { byte *second_level_memory
		     = top_level_memory + top_level_sizeof;
	            curr_string = pparam->value.sa.data;
	            end_string = curr_string + pparam->value.sa.size;
	            for ( ; curr_string < end_string; ++curr_string)
	              if (!curr_string->persistent)
	                { memcpy(second_level_memory,
		           curr_string->data, curr_string->size);
	                  ( (gs_param_string *)curr_string )->data
			   = second_level_memory;
	                  second_level_memory += curr_string->size;
	                }
	          }
	      }
	    break;
	default:
	    break;
	  }

	plist->head = pparam;
	plist->count++;
	return 0;
}

/* Individual writing routines. */
#define cpw(pvalue, type)\
  c_param_write(cplist, pkey, pvalue, type)
private int
c_param_write_null(gs_param_list *plist, gs_param_name pkey)
{	return cpw(NULL, cpt_null);
}
private int
c_param_write_typed(gs_param_list *plist, gs_param_name pkey,
 gs_param_typed_value *pvalue)
{	switch (pvalue->type)
	  {
	case cpt_dict:
	    return c_param_begin_write_dict
	     ( plist, pkey, &pvalue->value.d, false );
	case cpt_dict_int_keys:
	    return c_param_begin_write_dict
	     ( plist, pkey, &pvalue->value.d, true );
	default:
	    ;
	  }
	return cpw(&pvalue->value, pvalue->type);
}
private int
c_param_write_bool(gs_param_list *plist, gs_param_name pkey, bool *pvalue)
{	return cpw(pvalue, cpt_bool);
}
private int
c_param_write_int(gs_param_list *plist, gs_param_name pkey, int *pvalue)
{
	return cpw(pvalue, cpt_int);
}
private int
c_param_write_long(gs_param_list *plist, gs_param_name pkey, long *pvalue)
{	return cpw(pvalue, cpt_long);
}
private int
c_param_write_float(gs_param_list *plist, gs_param_name pkey, float *pvalue)
{	return cpw(pvalue, cpt_float);
}
private int
c_param_write_string(gs_param_list *plist, gs_param_name pkey,
  gs_param_string *pvalue)
{	return cpw(pvalue, cpt_string);
}
private int
c_param_write_int_array(gs_param_list *plist, gs_param_name pkey,
  gs_param_int_array *pvalue)
{	return cpw(pvalue, cpt_int_array);
}
private int
c_param_write_float_array(gs_param_list *plist, gs_param_name pkey,
  gs_param_float_array *pvalue)
{	return cpw(pvalue, cpt_float_array);
}
private int
c_param_write_string_array(gs_param_list *plist, gs_param_name pkey,
  gs_param_string_array *pvalue)
{	return cpw(pvalue, cpt_string_array);
}
private int
c_param_begin_write_dict(gs_param_list *plist, gs_param_name pkey,
  gs_param_dict *pvalue, bool int_keys)
{	gs_c_param_list *dlist =
	  gs_alloc_struct(cplist->memory, gs_c_param_list, &st_c_param_list,
			  "c_param_begin_write_dict");
	if ( dlist == 0 )
	  return_error(gs_error_VMerror);
	gs_c_param_list_write(dlist, cplist->memory);
	dlist->has_int_keys = int_keys;
	pvalue->list = (gs_param_list *)dlist;
	return 0;
}
private int
c_param_end_write_dict(gs_param_list *plist, gs_param_name pkey,
  gs_param_dict *pvalue)
{	return cpw(  pvalue->list,
	 ( (gs_c_param_list *)(pvalue->list) )->has_int_keys
	 ? cpt_dict_int_keys : cpt_dict  );
}

/* Other procedures */
private bool
c_param_requested(const gs_param_list *plist, gs_param_name pkey)
{	return true;
}

/* ================ Reading from a list to parameters ================ */

private param_proc_xmit_null(c_param_read_null);
private param_proc_xmit_typed(c_param_read_typed);
private param_proc_xmit_bool(c_param_read_bool);
private param_proc_xmit_int(c_param_read_int);
private param_proc_xmit_long(c_param_read_long);
private param_proc_xmit_float(c_param_read_float);
private param_proc_xmit_string(c_param_read_string);
private param_proc_xmit_int_array(c_param_read_int_array);
private param_proc_xmit_float_array(c_param_read_float_array);
private param_proc_xmit_string_array(c_param_read_string_array);
private param_proc_begin_xmit_dict(c_param_begin_read_dict);
private param_proc_next_key(c_param_get_next_key);
private param_proc_end_xmit_dict(c_param_end_read_dict);
private param_proc_get_policy(c_param_read_get_policy);
private param_proc_signal_error(c_param_read_signal_error);
private param_proc_commit(c_param_read_commit);
private const gs_param_list_procs c_read_procs = {
	c_param_read_null,
	c_param_read_typed,
	c_param_read_bool,
	c_param_read_int,
	c_param_read_long,
	c_param_read_float,
	c_param_read_string,
	c_param_read_string,		/* name = string */
	c_param_read_int_array,
	c_param_read_float_array,
	c_param_read_string_array,
	c_param_read_string_array,	/* name = string */
	c_param_begin_read_dict,
	c_param_end_read_dict,
	c_param_get_next_key,
	NULL,				/* requested */
	c_param_read_get_policy,
	c_param_read_signal_error,
	c_param_read_commit
};

/* Switch a list from writing to reading. */
void
gs_c_param_list_read(gs_c_param_list *plist)
{	plist->procs = &c_read_procs;
}

/* Generic routine for reading a parameter from a list. */
private gs_c_param *
c_param_find(gs_c_param_list *plist, gs_param_name pkey)
{	gs_c_param *pparam = plist->head;
	for ( ; pparam != 0; pparam = pparam->next )
	  if ( !strcmp(pparam->key, pkey) )
	    return pparam;
	return 0;
}
private int
c_param_read(gs_c_param_list *plist, gs_param_name pkey, void *pvalue,
  gs_param_type type)
{	gs_c_param *pparam = c_param_find(plist, pkey);
	bool match;
	if ( pparam == 0 )
	  return 1;
	/* This module's implementation aliases some types, so force those comparisons true */
	if ( !(match = pparam->type == type) )
	  switch (pparam->type)
	    {
	  case cpt_int:
	    if (type == cpt_long)
	     { *(long *)pvalue = pparam->value.i;
	       return  0;
	     }
	    if (type == cpt_float)
	     { *(float *)pvalue = (float)pparam->value.i;
	       return 0;
	     }
	    break;
	  case cpt_long:
	    if (type == cpt_int)
	     {
	       *(int *)pvalue = (int)pparam->value.l;
#if arch_sizeof_int < arch_sizeof_long
	       if ( pparam->value.l != *(int *)pvalue )
	         return_error(e_rangecheck);
#endif
	       return  0;
	     }
	    if (type == cpt_float)
	     { *(float *)pvalue = (float)pparam->value.l;
	       return 0;
	     }
	    break;
	  case cpt_string:
	    match = (type == cpt_name);
	    break;
	  case cpt_name:
	    match = (type == cpt_string);
	    break;
	  case cpt_int_array:
	    if (type == cpt_float_array)
	      {
	      /* Convert int array to float dest */
	      gs_param_float_array fa;
	      int element;
	      fa.size = pparam->value.ia.size;
	      fa.persistent = false;
	      if (pparam->alternate_typed_data == 0)
	        { if (   (  pparam->alternate_typed_data
	           = (void *)gs_alloc_bytes_immovable( plist->memory,
	           fa.size * sizeof(float),
	           "gs_c_param_read alternate float array" )  ) == 0   )
	            return_error(gs_error_VMerror);
	          for (element = 0; element < fa.size; ++element)
	            ( (float*)(pparam->alternate_typed_data) )[element]
	             = (float)pparam->value.ia.data[element];
	        }
	      fa.data = (float *)pparam->alternate_typed_data;

	      memcpy( pvalue, &fa, sizeof(gs_param_float_array) );
	      return 0;
	      }
	    break;
	  case cpt_string_array:
	    match = (type == cpt_name_array);
	    break;
	  case cpt_name_array:
	    match = (type == cpt_string_array);
	    break;
	  default:
	    break;
	    }
	if (!match)
	  return_error(gs_error_typecheck);
	memcpy(pvalue, &pparam->value, cpt_size(type));
	return 0;
}

/* Individual reading routines. */
#define cpr(pvalue, type)\
  c_param_read(cplist, pkey, pvalue, type)
private int
c_param_read_null(gs_param_list *plist, gs_param_name pkey)
{	return cpr(NULL, cpt_null);
}
private int
c_param_read_typed(gs_param_list *plist, gs_param_name pkey,
  gs_param_typed_value *pvalue)
{	gs_c_param *pparam = c_param_find(cplist, pkey);
	if ( pparam == 0 )
	  return 1;
	pvalue->type = pparam->type;
	if (pvalue->type == cpt_dict || pvalue->type == cpt_dict_int_keys)
	  { gs_c_param_list_read(&pparam->value.d);
	    pvalue->value.d.list = (gs_param_list *)&pparam->value.d;
	    pvalue->value.d.size = pparam->value.d.count;
	  }
	else
	  memcpy(&pvalue->value, &pparam->value, cpt_size(pvalue->type));
	return 0;
}
private int
c_param_read_bool(gs_param_list *plist, gs_param_name pkey, bool *pvalue)
{	return cpr(pvalue, cpt_bool);
}
private int
c_param_read_int(gs_param_list *plist, gs_param_name pkey, int *pvalue)
{	return cpr(pvalue, cpt_int);
}
private int
c_param_read_long(gs_param_list *plist, gs_param_name pkey, long *pvalue)
{	return cpr(pvalue, cpt_long);
}
private int
c_param_read_float(gs_param_list *plist, gs_param_name pkey, float *pvalue)
{	return cpr(pvalue, cpt_float);
}
private int
c_param_read_string(gs_param_list *plist, gs_param_name pkey,
  gs_param_string *pvalue)
{	return cpr(pvalue, cpt_string);
}
private int
c_param_read_int_array(gs_param_list *plist, gs_param_name pkey,
  gs_param_int_array *pvalue)
{	return cpr(pvalue, cpt_int_array);
}
private int
c_param_read_float_array(gs_param_list *plist, gs_param_name pkey,
  gs_param_float_array *pvalue)
{	return cpr(pvalue, cpt_float_array);
}
private int
c_param_read_string_array(gs_param_list *plist, gs_param_name pkey,
  gs_param_string_array *pvalue)
{	return cpr(pvalue, cpt_string_array);
}
private int
c_param_begin_read_dict(gs_param_list *plist, gs_param_name pkey,
  gs_param_dict *pvalue, bool int_keys)
{	gs_c_param *pparam = c_param_find(cplist, pkey);
	if ( pparam == 0 )
	  return 1;
	if ( pparam->type != (int_keys ? cpt_dict_int_keys : cpt_dict) )
	  return_error(gs_error_typecheck);
	gs_c_param_list_read(&pparam->value.d);
	pvalue->list = (gs_param_list *)&pparam->value.d;
	pvalue->size = pparam->value.d.count;
	return 0;
}
private int
c_param_end_read_dict(gs_param_list *plist, gs_param_name pkey,
  gs_param_dict *pvalue)
{	return 0;
}

/* Other procedures */
private int
c_param_read_get_policy(gs_param_list *plist, gs_param_name pkey)
{	return gs_param_policy_ignore;
}
private int
c_param_read_signal_error(gs_param_list *plist, gs_param_name pkey, int code)
{	return code;
}
private int
c_param_read_commit(gs_param_list *plist)
{	return 0;
}
private int	/* ret 0 ok, 1 if EOF, or -ve err */
c_param_get_next_key(gs_param_list *plist, gs_param_key_t *key)
{	gs_c_param *pparam = key->enumerator.pvoid
	 ? ( (gs_c_param *)(key->enumerator.pvoid) )->next : cplist->head;
	if (pparam == 0)
	  return 1;
	key->enumerator.pvoid = pparam;
	key->key = pparam->key;
	key->key_sizeof= strlen(key->key);
	return 0;
}

