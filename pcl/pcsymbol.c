/* Copyright (C) 1996 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* pcsymbol.c */
/* PCL5 user-defined symbol set commands */
#include "std.h"
#include "plvalue.h"
#include "pcommand.h"
#include "pcstate.h"

/* Define the header of a symbol set definition, */
/* including the storage status that we prefix to it. */
typedef struct pcl_symbol_set_s {
  pcl_entity_common;
	/* The next 18 bytes are defined by PCL5. */
	/* See p. 10-5 of the PCL5 Technical Reference Manual. */
  byte header_size[2];
  byte id[2];
  byte format;
  byte type;
  byte first_code[2];
  byte last_code[2];
  byte character_requirements[8];
} pcl_symbol_set_t;
#define symbol_set_extra offset_of(pcl_symbol_set_t, header_size)

private int /* ESC * c <id> R */ 
pcl_symbol_set_id_code(pcl_args_t *pargs, pcl_state_t *pcls)
{	uint id = uint_arg(pargs);
	id_set_value(pcls->symbol_set_id, id);
	return 0;
}

private int /* ESC ( f <count> W */ 
pcl_define_symbol_set(pcl_args_t *pargs, pcl_state_t *pcls)
{	uint count = uint_arg(pargs);
	const byte *data = arg_data(pargs);
	uint header_size;
	uint first_code, last_code;
	gs_memory_t *mem = pcls->memory;
	byte *header;

	if ( count < 18 )
	  return e_Range;
#define pss ((const pcl_symbol_set_t *)(data - symbol_set_extra))
	header_size = pl_get_uint16(pss->header_size);
	if ( header_size < 18 ||
	     pss->id[0] != id_key(pcls->symbol_set_id)[0] ||
	     pss->id[1] != id_key(pcls->symbol_set_id)[1] ||
	     pss->type > 2
	   )
	  return e_Range;
	switch ( pss->format )
	  {
	  case 1:
	  case 3:
	    break;
	  default:
	    return 0;
	  }
	first_code = pl_get_uint16(pss->first_code);
	last_code = pl_get_uint16(pss->last_code);
	if ( first_code > last_code ||
	     count < 18 + (last_code - first_code + 1) * 2
	   )
	  return e_Range;
	pl_dict_undef(&pcls->symbol_sets, id_key(pcls->symbol_set_id), 2);
	header = gs_alloc_bytes(mem, symbol_set_extra + count,
				"pcl_font_header(header)");
	if ( header == 0 )
	  return_error(e_Memory);
	memcpy(header + symbol_set_extra, data, count);
	((pcl_symbol_set_t *)header)->storage = pcds_temporary;
	pl_dict_put(&pcls->symbol_sets, id_key(pcls->symbol_set_id), 2, header);
	return 0;
#undef pss
}

private int /* ESC * c <ssc_enum> S */ 
pcl_symbol_set_control(pcl_args_t *pargs, pcl_state_t *pcls)
{	gs_const_string key;
	void *value;
	pl_dict_enum_t denum;

	switch ( int_arg(pargs) )
	  {
	  case 0:
	    { /* Delete all user-defined symbol sets. */
	      pl_dict_release(&pcls->symbol_sets);
	    }
	    return 0;
	  case 1:
	    { /* Delete all temporary symbol sets. */
	      pl_dict_enum_stack_begin(&pcls->symbol_sets, &denum, false);
	      while ( pl_dict_enum_next(&denum, &key, &value) )
		if ( ((pcl_symbol_set_t *)value)->storage == pcds_temporary )
		  pl_dict_undef(&pcls->symbol_sets, key.data, key.size);
	    }
	    return 0;
	  case 2:
	    { /* Delete symbol set <symbol_set_id>. */
	      pl_dict_undef(&pcls->symbol_sets, id_key(pcls->symbol_set_id), 2);
	    }
	    return 0;
	  case 4:
	    { /* Make <symbol_set_id> temporary. */
	      if ( pl_dict_find(&pcls->symbol_sets, id_key(pcls->symbol_set_id), 2, &value) )
		((pcl_symbol_set_t *)value)->storage = pcds_temporary;
	    }
	    return 0;
	  case 5:
	    { /* Make <symbol_set_id> permanent. */
	      if ( pl_dict_find(&pcls->symbol_sets, id_key(pcls->symbol_set_id), 2, &value) )
		((pcl_symbol_set_t *)value)->storage = pcds_permanent;
	    }
	    return 0;
	  default:
	    return 0;
	  }
}

/* Initialization */
private int
pcsymbol_do_init(gs_memory_t *mem)
{		/* Register commands */
	DEFINE_CLASS_COMMAND_ARGS('*', 'c', 'R', pcl_symbol_set_id_code, pca_neg_error|pca_big_error)
	DEFINE_CLASS_COMMAND_ARGS('(', 'f', 'W', pcl_define_symbol_set, pca_bytes)
	DEFINE_CLASS_COMMAND_ARGS('*', 'c', 'S', pcl_symbol_set_control, pca_neg_ignore|pca_big_ignore)
	return 0;
}
private void
pcsymbol_do_reset(pcl_state_t *pcls, pcl_reset_type_t type)
{	if ( type & (pcl_reset_initial | pcl_reset_printer) )
	  { id_set_value(pcls->symbol_set_id, 0);
	    if ( type & pcl_reset_initial )
	      pl_dict_init(&pcls->symbol_sets, pcls->memory, NULL);
	    else
	      { pcl_args_t args;
	        arg_set_uint(&args, 1);	/* delete temporary symbol sets */
		pcl_symbol_set_control(&args, pcls);
	      }
	  }
}
private int
pcsymbol_do_copy(pcl_state_t *psaved, const pcl_state_t *pcls,
  pcl_copy_operation_t operation)
{	if ( operation & pcl_copy_after )
	  { /* Don't restore the downloaded symbol set dictionary. */
	    psaved->symbol_sets = pcls->symbol_sets;
	  }
	return 0;
}
const pcl_init_t pcsymbol_init = {
  pcsymbol_do_init, pcsymbol_do_reset, pcsymbol_do_copy
};
