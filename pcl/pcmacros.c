/* Copyright (C) 1996, 1997 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* pcmacros.c */
/* PCL5 macro commands */
#include "stdio_.h"			/* std.h + NULL */
#include "pcommand.h"
#include "pcstate.h"
#include "pcparse.h"

/* ---------------- Macro execution ---------------- */

/* Execute a macro.  We're willing to use C recursion for this because */
/* the PCL5 specification only allows 2 levels of call. */
private int
do_copies(pcl_state_t *psaved, pcl_state_t *pcls,
  pcl_copy_operation_t copy)
{	const pcl_init_t **init = pcl_init_table;
	int code = 0;

	for ( ; *init && code >= 0; ++init )
	  if ( (*init)->do_copy )
	    code = (*(*init)->do_copy)(psaved, pcls, copy);
	return code;
}
int
pcl_execute_macro(const pcl_macro_t *pmac, pcl_state_t *pcls,
  pcl_copy_operation_t before, pcl_reset_type_t reset,
  pcl_copy_operation_t after)
{	pcl_parser_state_t state;
	pcl_state_t saved;
	stream_cursor_read r;
	int code;

	if ( before )
	  { memcpy(&saved, pcls, sizeof(*pcls));
	    do_copies(&saved, pcls, before);
	    pcls->saved = &saved;
	  }
	if ( reset )
	  pcl_do_resets(pcls, reset);
	pcl_process_init(&state);
	r.ptr = (const byte *)(pmac + 1) - 1;
	r.limit =
	  (const byte *)pmac + (gs_object_size(pcls->memory, pmac) - 1);
	pcls->macro_level++;
	code = pcl_process(&state, pcls, &r);
	pcls->macro_level--;
	if ( after )
	  { do_copies(&saved, pcls, after);
	    memcpy(pcls, &saved, sizeof(*pcls));
	  }
	return code;
}

/* ---------------- Commands ---------------- */

/* Define the macro control operations. */
enum {
  macro_end_definition = 1,
  macro_execute = 2,
  macro_call = 3,
  macro_delete_temporary = 7
};

private int /* ESC & f <mc_enum> X */
pcl_macro_control(pcl_args_t *pargs, pcl_state_t *pcls)
{	int i = int_arg(pargs);
	gs_const_string key;
	void *value;
	pl_dict_enum_t denum;

	if ( i == macro_end_definition )
	  { if ( pcls->defining_macro )
	      { /* The parser has included everything through this command */
		/* in the macro_definition, so we can finish things up. */
		int code;
		code = pl_dict_put(&pcls->macros, current_macro_id,
				   current_macro_id_size, pcls->macro_definition);
		pcls->defining_macro = false;
		pcls->macro_definition = 0;
	        return code;
	      }
	  }
	else if ( pcls->defining_macro )
	  return 0;		/* don't execute other macro operations */
	else if ( i == macro_execute || i == macro_call )
	  { /*
	     * TRM 12-9 says that "two levels of nesting are allowed",
	     * which means 3 levels of macros (1 non-nested, 2 nested).
	     */
	    if ( pcls->macro_level > 2 )
	      return 0;
	  }
	else if ( pcls->macro_level )
	  return e_Range;	/* macro operations not allowed inside macro */
	switch ( i )
	  {
	  case 0:
	    { /* Start defining <macro_id>. */
	      pcl_macro_t *pmac;

	      pl_dict_undef_purge_synonyms(&pcls->macros, current_macro_id, current_macro_id_size);
	      pmac = (pcl_macro_t *)
		gs_alloc_bytes(pcls->memory, sizeof(pcl_macro_t),
			       "begin macro definition");
	      if ( pmac == 0 )
		return_error(e_Memory);
	      pmac->storage = pcds_temporary;
	      pcls->macro_definition = (byte *)pmac;
	      pcls->defining_macro = true;
	      return 0;
	    }
	  case macro_end_definition: /* 1 */
	    { /* Stop defining macro.  Outside a macro, this is an error. */
	      return e_Range;
	    }
	  case macro_execute:	/* 2 */
	    { /* Execute <macro_id>. */
	      void *value;
	      if ( !pl_dict_find(&pcls->macros, current_macro_id, current_macro_id_size,
				 &value)
		 )
		return 0;
	      return pcl_execute_macro((const pcl_macro_t *)value, pcls,
			       pcl_copy_none, pcl_reset_none, pcl_copy_none);
	    }
	  case macro_call:	/* 3 */
	    { /* Call <macro_id>, saving and restoring environment. */
	      void *value;
	      if ( !pl_dict_find(&pcls->macros, current_macro_id, current_macro_id_size,
				 &value)
		 )
		return 0;
	      return pcl_execute_macro((const pcl_macro_t *)value, pcls,
				       pcl_copy_before_call, pcl_reset_none,
				       pcl_copy_after_call);
	    }
	  case 4:
	    { /* Define <macro_id> as automatic overlay. */
	      pcls->overlay_macro_id = pcls->macro_id;
	      pcls->overlay_enabled = true;
	      return 0;
	    }
	  case 5:
	    { /* Cancel automatic overlay. */
	      pcls->overlay_enabled = false;
	      return 0;
	    }
	  case 6:
	    { /* Delete all macros. */
	      pl_dict_release(&pcls->macros);
	      return 0;
	    }
	  case macro_delete_temporary: /* 7 */
	    { /* Delete temporary macros. */
	      pl_dict_enum_stack_begin(&pcls->macros, &denum, false);
	      while ( pl_dict_enum_next(&denum, &key, &value) )
		if ( ((pcl_macro_t *)value)->storage == pcds_temporary )
		  pl_dict_undef_purge_synonyms(&pcls->macros, key.data, key.size);
	      return 0;
	    }
	  case 8:
	    { /* Delete <macro_id>. */
	      pl_dict_undef_purge_synonyms(&pcls->macros, current_macro_id, current_macro_id_size);
	      return 0;
	    }
	  case 9:
	    { /* Make <macro_id> temporary. */
	      if ( pl_dict_find(&pcls->macros, current_macro_id, current_macro_id_size, &value) )
		((pcl_macro_t *)value)->storage = pcds_temporary;
	      return 0;
	    }
	  case 10:
	    { /* Make <macro_id> permanent. */
	      if ( pl_dict_find(&pcls->macros, current_macro_id, current_macro_id_size, &value) )
		((pcl_macro_t *)value)->storage = pcds_permanent;
	      return 0;
	    }
	  default:
	    return e_Range;
	  }
}

private int /* ESC & f <id> Y */
pcl_assign_macro_id(pcl_args_t *pargs, pcl_state_t *pcls)
{	uint id = uint_arg(pargs);
	id_set_value(pcls->macro_id, id);
	pcls->macro_id_type = numeric_id;
	return 0;
}

/* Initialization */
private int
pcmacros_do_init(gs_memory_t *mem)
{		/* Register commands */
	DEFINE_CLASS('&')
	  {'f', 'X',
	     PCL_COMMAND("Macro Control", pcl_macro_control,
			 pca_neg_error|pca_big_error|pca_in_macro)},
	  {'f', 'Y',
	     PCL_COMMAND("Assign Macro ID", pcl_assign_macro_id,
			 pca_neg_error|pca_big_error)},
	END_CLASS
	return 0;
}
private void
pcmacros_do_reset(pcl_state_t *pcls, pcl_reset_type_t type)
{	if ( type & (pcl_reset_initial | pcl_reset_printer) )
	  { pcls->overlay_enabled = false;
	    pcls->macro_level = 0;
	    pcls->defining_macro = false;
	    pcls->saved = 0;
	    pcls->macro_definition = 0;

	    if ( type & pcl_reset_initial )
		pl_dict_init(&pcls->macros, pcls->memory, NULL);
	    else
	      { pcl_args_t args;
	        arg_set_uint(&args, macro_delete_temporary);
		pcl_macro_control(&args, pcls);
		if ( pcls->alpha_macro_id.id != 0 )
		  gs_free_object(pcls->memory,
				 pcls->alpha_macro_id.id,
				 "pcmacros_do_reset");
	      }
	  }

	if ( type & (pcl_reset_initial | pcl_reset_printer | pcl_reset_overlay) )
          {
	    pcls->alpha_macro_id.size = 0;
	    pcls->macro_id_type = numeric_id;
	    id_set_value(pcls->macro_id, 0);
	    pcls->alpha_macro_id.id = 0;
          }
}
private int
pcmacros_do_copy(pcl_state_t *psaved, const pcl_state_t *pcls,
  pcl_copy_operation_t operation)
{	if ( operation & pcl_copy_after )
	  { /* Don't restore the macro dictionary. */
	    psaved->macros = pcls->macros;
	  }
	return 0;
}
const pcl_init_t pcmacros_init = {
  pcmacros_do_init, pcmacros_do_reset, pcmacros_do_copy
};
