/*
 * Copyright (C) 1998 Aladdin Enterprises.
 * All rights reserved.
 *
 * This file is part of Aladdin Ghostscript.
 *
 * Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
 * or distributor accepts any responsibility for the consequences of using it,
 * or for whether it serves any particular purpose or works at all, unless he
 * or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
 * License (the "License") for full details.
 *
 * Every copy of Aladdin Ghostscript must include a copy of the License,
 * normally in a plain ASCII text file named PUBLIC.  The License grants you
 * the right to copy, modify and redistribute Aladdin Ghostscript, but only
 * under certain conditions described in the License.  Among other things, the
 * License requires that the copyright notice and this notice be preserved on
 * all copies.
 */

/* pcsymbol.c */
/* PCL5 user-defined symbol set commands */
#include "stdio_.h"			/* std.h + NULL */
#include "plvalue.h"
#include "pcommand.h"
#include "pcstate.h"
#include "pcfont.h"
#include "pcsymbol.h"


private int /* ESC * c <id> R */
pcl_symbol_set_id_code(pcl_args_t *pargs, pcl_state_t *pcls)
{	uint id = uint_arg(pargs);
	id_set_value(pcls->symbol_set_id, id);
	return 0;
}

private int /* ESC ( f <count> W */
pcl_define_symbol_set(pcl_args_t *pargs, pcl_state_t *pcls)
{	uint count = uint_arg(pargs);
	const pl_symbol_map_t *psm = (pl_symbol_map_t *)arg_data(pargs);
	uint header_size;
	uint first_code, last_code;
	gs_memory_t *mem = pcls->memory;
	pl_symbol_map_t *header;
	pcl_symbol_set_t *symsetp;
	pl_glyph_vocabulary_t gv;

	/* argh */
#define psm_header_size offset_of(pl_symbol_map_t, mapping_type)
	if ( count < psm_header_size )
	  return e_Range;
	header_size = pl_get_uint16(psm->header_size);
	if ( header_size < psm_header_size ||
	     psm->id[0] != id_key(pcls->symbol_set_id)[0] ||
	     psm->id[1] != id_key(pcls->symbol_set_id)[1] ||
	     psm->type > 2
	   )
	  return e_Range;
	switch ( psm->format )
	  {
	  case 1:
	  case 3:
	    break;
	  default:
	    return e_Range;
	  }
	first_code = pl_get_uint16(psm->first_code);
	last_code = pl_get_uint16(psm->last_code);
	gv = (psm->character_requirements[7] & 07)==1? plgv_Unicode: plgv_MSL;
	{ int num_codes = last_code - first_code + 1;
	  int i;

	  if ( num_codes <= 0 || count < psm_header_size + num_codes * 2 )
	    return e_Range;
	  header =
	    (pl_symbol_map_t *)gs_alloc_bytes(mem,
					      psm_header_size +
					        num_codes * sizeof(ushort),
					      "pcl_font_header(header)");
	  if ( header == 0 )
	    return_error(e_Memory);
	  memcpy((void *)header, (void *)psm, psm_header_size);
	  /*
	   * Byte swap the codes now, so that we don't have to byte swap
	   * them every time we access them.
	   */
	  for ( i = num_codes; --i >= 0; )
	    header->codes[i] =
	      pl_get_uint16((byte *)psm + psm_header_size + i * 2);
	}
#undef psm_header_size

	/* Symbol set may already exist; if so, we may be replacing one of
	 * its existing maps or adding one for a new glyph vocabulary. */
	if ( pl_dict_find(&pcls->soft_symbol_sets, id_key(pcls->symbol_set_id),
	    2, (void **)&symsetp) )
	  {
	    if ( symsetp->maps[gv] != NULL )
	      gs_free_object(mem, symsetp->maps[gv], "symset map");
	  }
	else
	  { pl_glyph_vocabulary_t gx;
	    symsetp = (pcl_symbol_set_t *)gs_alloc_bytes(mem,
		sizeof(pcl_symbol_set_t), "symset dict value");
	    if ( !symsetp )
	      return_error(e_Memory);
	    for ( gx = plgv_MSL; gx < plgv_next; gx++ )
	      symsetp->maps[gx] = NULL;
	    symsetp->storage = pcds_temporary;
	    pl_dict_put(&pcls->soft_symbol_sets, id_key(pcls->symbol_set_id),
		2, symsetp);
	  }
	symsetp->maps[gv] = header;

	return 0;
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
	      /* Note: When deleting symbol set(s), it is easier (safer?)
	       * to decache and reselect fonts unconditionally.  (Consider,
	       * for example, deleting a downloaded overload of a built-in
	       * which might be the default ID.) */
	      pl_dict_release(&pcls->soft_symbol_sets);
	      pcl_decache_font(pcls, -1);
	    }
	    return 0;
	  case 1:
	    { /* Delete all temporary symbol sets. */
	      pl_dict_enum_stack_begin(&pcls->soft_symbol_sets, &denum, false);
	      while ( pl_dict_enum_next(&denum, &key, &value) )
		if ( ((pcl_symbol_set_t *)value)->storage == pcds_temporary )
		  pl_dict_undef(&pcls->soft_symbol_sets, key.data, key.size);
	      pcl_decache_font(pcls, -1);
	    }
	    return 0;
	  case 2:
	    { /* Delete symbol set <symbol_set_id>. */
	      pl_dict_undef(&pcls->soft_symbol_sets,
		  id_key(pcls->symbol_set_id), 2);
	      pcl_decache_font(pcls, -1);
	    }
	    return 0;
	  case 4:
	    { /* Make <symbol_set_id> temporary. */
	      if ( pl_dict_find(&pcls->soft_symbol_sets,
		  id_key(pcls->symbol_set_id), 2, &value) )
		((pcl_symbol_set_t *)value)->storage = pcds_temporary;
	    }
	    return 0;
	  case 5:
	    { /* Make <symbol_set_id> permanent. */
	      if ( pl_dict_find(&pcls->soft_symbol_sets,
		  id_key(pcls->symbol_set_id), 2, &value) )
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
	DEFINE_CLASS_COMMAND_ARGS('*', 'c', 'R', "Symbol Set ID Code",
				  pcl_symbol_set_id_code,
				  pca_neg_error|pca_big_error)
	DEFINE_CLASS_COMMAND_ARGS('(', 'f', 'W', "Define Symbol Set",
				  pcl_define_symbol_set, pca_bytes)
	DEFINE_CLASS_COMMAND_ARGS('*', 'c', 'S', "Symbol Set Control",
				  pcl_symbol_set_control,
				  pca_neg_ignore|pca_big_ignore)
	return 0;
}

private void	/* free any symbol maps as well as dict value entry */
pcsymbol_dict_value_free(gs_memory_t *mem, void *value, client_name_t cname)
{	pcl_symbol_set_t *ssp = (pcl_symbol_set_t *)value;
	pl_glyph_vocabulary_t gx;

	if ( ssp->storage != pcds_internal )
	  {
	    for ( gx = plgv_MSL; gx < plgv_next; gx++ )
	      {
		if ( ssp->maps[gx] != NULL )
		  gs_free_object(mem, (void*)ssp->maps[gx], cname);
	      }
	  }
	gs_free_object(mem, value, cname);
}

private void
pcsymbol_do_reset(pcl_state_t *pcls, pcl_reset_type_t type)
{	if ( type & (pcl_reset_initial | pcl_reset_printer | pcl_reset_overlay) )
	  { id_set_value(pcls->symbol_set_id, 0);
	    if ( type & pcl_reset_initial )
	      {
	      /* Don't set a parent relationship from soft to built-in
	       * symbol sets.  Although it is arguably useful, it's
	       * better to avoid it and keep anyone who's looking at the
	       * soft symbol sets from mucking up the permanent ones. */
	      pl_dict_init(&pcls->soft_symbol_sets, pcls->memory,
		  pcsymbol_dict_value_free);
	      pl_dict_init(&pcls->built_in_symbol_sets, pcls->memory,
		  pcsymbol_dict_value_free);
	      }
	    else if ( type & pcl_reset_printer )
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
	    psaved->built_in_symbol_sets = pcls->built_in_symbol_sets;
	  }
	return 0;
}


int
pcl_load_built_in_symbol_sets(pcl_state_t *pcls)
{
	const pl_symbol_map_t **maplp;
	pcl_symbol_set_t *symsetp;
	pl_glyph_vocabulary_t gv;

	for ( maplp = &pl_built_in_symbol_maps[0]; *maplp; maplp++ )
	  {
	    const pl_symbol_map_t *mapp = *maplp;
	    /* Create entry for symbol set if this is the first map for
	     * that set. */
	    if ( !pl_dict_find(&pcls->built_in_symbol_sets, mapp->id, 2,
		(void **)&symsetp) )
	      { pl_glyph_vocabulary_t gx;
		symsetp = (pcl_symbol_set_t *)gs_alloc_bytes(pcls->memory,
		    sizeof(pcl_symbol_set_t), "symset init dict value");
		if ( !symsetp )
		  return_error(e_Memory);
		for ( gx = plgv_MSL; gx < plgv_next; gx++ )
		  symsetp->maps[gx] = NULL;
		symsetp->storage = pcds_internal;
	      }
	    gv = (mapp->character_requirements[7] & 07)==1?
		plgv_Unicode: plgv_MSL;
	    pl_dict_put(&pcls->built_in_symbol_sets, mapp->id, 2, symsetp);
	    symsetp->maps[gv] = (pl_symbol_map_t *)mapp;
	  }
	return 0;
}


bool
pcl_check_symbol_support(const byte *symset_req, const byte *font_sup)
{	int i;

	/* if glyph vocabularies match, following will work on the
	 * last 3 bits of last byte.  Note that the font-support bits
	 * are inverted (0 means available). 
	 */
	for ( i = 0; i < 7; i++ )
	  if ( symset_req[i] & font_sup[i] )
	    return false;		/* something needed, not present */
	/* check the last byte but not the glyph vocabularies. */
	if ((symset_req[7] >> 3) & (font_sup[7] >> 3))
	    return false;

	return true;
}


/* Find the symbol map for a particular symbol set and glyph vocabulary,
 * if it exists.
 * There are two dictionaries--one for soft (downloaded) symbol sets and
 * one for built-ins.  These are searched separately.  The actual maps
 * present for a symbol set may overlap between soft and built-in. */
pl_symbol_map_t *
pcl_find_symbol_map(const pcl_state_t *pcls, const byte *id,
  pl_glyph_vocabulary_t gv)
{	
    pcl_symbol_set_t *setp;
	
    if ( pl_dict_find((pl_dict_t *)&pcls->soft_symbol_sets,
		      id, 2, (void **)&setp) &&
	 setp->maps[gv] != NULL )
	return setp->maps[gv];
    if ( pl_dict_find((pl_dict_t *)&pcls->built_in_symbol_sets,
		      id, 2, (void**)&setp) ) {
	/* simple case we found a matching symbol set */
	if ( setp->maps[gv] != NULL )
	    return setp->maps[gv];
	/* we requested a unicode symbol set and found an msl
	   symbol set that can be mapped to unicode */
	if ( (gv == plgv_Unicode)  &&
	     (setp->maps[plgv_MSL]) &&
	     ((setp->maps[plgv_MSL])->mapping_type == PLGV_M2U_MAPPING) )
	    return setp->maps[plgv_MSL];
	/* we requested an msl symbol set and found a unicode
	   symbol set that can be mapped to msl */
	if ( (gv == plgv_MSL) &&
	     (setp->maps[plgv_Unicode]) &&
	     ((setp->maps[plgv_Unicode])->mapping_type == PLGV_U2M_MAPPING) )
	    return setp->maps[plgv_Unicode];
    }
    return NULL;
}

const pcl_init_t pcsymbol_init = {
  pcsymbol_do_init, pcsymbol_do_reset, pcsymbol_do_copy
};
