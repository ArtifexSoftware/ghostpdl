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

/* pcsymbol.c */
/* PCL5 user-defined symbol set commands */
#include "stdio_.h"			/* std.h + NULL */
#include "plvalue.h"
#include "pcommand.h"
#include "pcstate.h"
#include "pcfont.h"
#include "pcsymbol.h"



/* HP printers will not convert MSL coded symbol table to unicode.  An
   MSL based font must be used with an MSL symbol set.  We think this
   is a bug.  The following definition can be uncommented to support
   the feature anyway */

/* #define SUPPORT_MSL_TO_UNICODE */

static int /* ESC * c <id> R */
pcl_symbol_set_id_code(pcl_args_t *pargs, pcl_state_t *pcs)
{	uint id = uint_arg(pargs);
	id_set_value(pcs->symbol_set_id, id);
	return 0;
}

#ifdef DEBUG
static void
dump_dl_symbol_set(const pl_symbol_map_t *psm)
{
    dprintf6("header size:%d id:%d format:%s type:%d first code:%d last code:%d\n",
             pl_get_uint16(psm->header_size),
             pl_get_uint16(psm->id),
             (psm->format == 1 ? "MSL" : (psm->format == 3 ? "Unicode" : "Unknown")),
             psm->type,
             pl_get_uint16(psm->first_code),
             pl_get_uint16(psm->last_code));
    {
        int i;
        int num_codes = pl_get_uint16(psm->last_code) - pl_get_uint16(psm->first_code) + 1;
        for (i = 0; i < num_codes; i++) {
            dprintf2("index=%d, code:%d\n", i, psm->codes[i]);
        }
    }
}
#endif

static int /* ESC ( f <count> W */
pcl_define_symbol_set(pcl_args_t *pargs, pcl_state_t *pcs)
{	uint count = uint_arg(pargs);
	const pl_symbol_map_t *psm = (pl_symbol_map_t *)arg_data(pargs);
	uint header_size;
	uint first_code, last_code;
	gs_memory_t *mem = pcs->memory;
	pl_symbol_map_t *header;
	pcl_symbol_set_t *symsetp;
	pl_glyph_vocabulary_t gv;

#define psm_header_size 18
	if ( count < psm_header_size )
	  return e_Range;
	header_size = pl_get_uint16(psm->header_size);
	if ( header_size < psm_header_size ||
	     psm->id[0] != id_key(pcs->symbol_set_id)[0] ||
	     psm->id[1] != id_key(pcs->symbol_set_id)[1] ||
	     psm->type > 2
	   )
	  return e_Range;
	switch ( psm->format )
	  {
	  case 1:
              gv = plgv_MSL;
              break;
	  case 3:
              gv = plgv_Unicode;
              break;
	  default:
	    return e_Range;
	  }
	first_code = pl_get_uint16(psm->first_code);
	last_code = pl_get_uint16(psm->last_code);
        /* NB fixme should check psm->Format to identify the vocabulary. */
	{ int num_codes = last_code - first_code + 1;
	  int i;

	  if ( num_codes <= 0 || last_code > 255 || (count != psm_header_size + num_codes * 2) ) 
	    return e_Range;

	  header =
	    (pl_symbol_map_t *)gs_alloc_bytes(mem,
					      sizeof(pl_symbol_map_t),
					      "pcl_font_header(header)");
	  if ( header == 0 )
	    return_error(e_Memory);
	  memcpy((void *)header, (void *)psm, psm_header_size);
	  /* allow mapping to and from msl and unicode */
          if (psm->format == 1)
#ifdef SUPPORT_MSL_TO_UNICODE
              header->mapping_type = PLGV_M2U_MAPPING;
#else
              header->mapping_type = PLGV_NO_MAPPING;
#endif
          else
              header->mapping_type = PLGV_U2M_MAPPING;

	  /*
	   * Byte swap the codes now, so that we don't have to byte swap
	   * them every time we access them.
	   */
	  for ( i = num_codes; --i >= 0; )
	    header->codes[i] =
	      pl_get_uint16((byte *)psm + psm_header_size + i * 2);
	}
#undef psm_header_size

#ifdef DEBUG
        if ( gs_debug_c('=') )
            dump_dl_symbol_set(psm);
#endif

	/* Symbol set may already exist; if so, we may be replacing one of
	 * its existing maps or adding one for a new glyph vocabulary. */
	if ( pl_dict_find(&pcs->soft_symbol_sets, id_key(pcs->symbol_set_id),
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
	    pl_dict_put(&pcs->soft_symbol_sets, id_key(pcs->symbol_set_id),
		2, symsetp);
	  }
	symsetp->maps[gv] = header;

	return 0;
}

static int /* ESC * c <ssc_enum> S */
pcl_symbol_set_control(pcl_args_t *pargs, pcl_state_t *pcs)
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
	      pl_dict_release(&pcs->soft_symbol_sets);
	      pcl_decache_font(pcs, -1);
	    }
	    return 0;
	  case 1:
	    { /* Delete all temporary symbol sets. */
	      pl_dict_enum_stack_begin(&pcs->soft_symbol_sets, &denum, false);
	      while ( pl_dict_enum_next(&denum, &key, &value) )
		if ( ((pcl_symbol_set_t *)value)->storage == pcds_temporary )
		  pl_dict_undef(&pcs->soft_symbol_sets, key.data, key.size);
	      pcl_decache_font(pcs, -1);
	    }
	    return 0;
	  case 2:
	    { /* Delete symbol set <symbol_set_id>. */
	      pl_dict_undef(&pcs->soft_symbol_sets,
		  id_key(pcs->symbol_set_id), 2);
	      pcl_decache_font(pcs, -1);
	    }
	    return 0;
	  case 4:
	    { /* Make <symbol_set_id> temporary. */
	      if ( pl_dict_find(&pcs->soft_symbol_sets,
		  id_key(pcs->symbol_set_id), 2, &value) )
		((pcl_symbol_set_t *)value)->storage = pcds_temporary;
	    }
	    return 0;
	  case 5:
	    { /* Make <symbol_set_id> permanent. */
	      if ( pl_dict_find(&pcs->soft_symbol_sets,
		  id_key(pcs->symbol_set_id), 2, &value) )
		((pcl_symbol_set_t *)value)->storage = pcds_permanent;
	    }
	    return 0;
	  default:
	    return 0;
	  }
}

static void	/* free any symbol maps as well as dict value entry */
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

static int
pcl_load_built_in_symbol_sets(pcl_state_t *pcs)
{
	const pl_symbol_map_t **maplp;
	pcl_symbol_set_t *symsetp;
	pl_glyph_vocabulary_t gv;

	for ( maplp = &pl_built_in_symbol_maps[0]; *maplp; maplp++ )
	  {
	    const pl_symbol_map_t *mapp = *maplp;
	    /* Create entry for symbol set if this is the first map for
	     * that set. */
	    if ( !pl_dict_find(&pcs->built_in_symbol_sets, mapp->id, 2,
		(void **)&symsetp) )
	      { pl_glyph_vocabulary_t gx;
		symsetp = (pcl_symbol_set_t *)gs_alloc_bytes(pcs->memory,
		    sizeof(pcl_symbol_set_t), "symset init dict value");
		if ( !symsetp )
		  return_error(e_Memory);
		for ( gx = plgv_MSL; gx < plgv_next; gx++ )
		  symsetp->maps[gx] = NULL;
		symsetp->storage = pcds_internal;
	      }
	    gv = (mapp->character_requirements[7] & 07)==1?
		plgv_Unicode: plgv_MSL;
	    pl_dict_put(&pcs->built_in_symbol_sets, mapp->id, 2, symsetp);
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
pcl_find_symbol_map(const pcl_state_t *pcs, const byte *id,
                    pl_glyph_vocabulary_t gv, bool wide16)
{	
    pcl_symbol_set_t *setp;
	

    if ( pl_dict_find((pl_dict_t *)&pcs->soft_symbol_sets,
		      id, 2, (void **)&setp) ||
         pl_dict_find((pl_dict_t *)&pcs->built_in_symbol_sets,
		      id, 2, (void**)&setp) ) {

        /* 16 bit sets are not supported, they aren't strictly
           necessary, yet. */
        if (wide16)
            return NULL;
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

/* Initialization */
static int
pcsymbol_do_registration(
    pcl_parser_state_t *pcl_parser_state,
    gs_memory_t *mem
)
{		/* Register commands */
	DEFINE_CLASS_COMMAND_ARGS('*', 'c', 'R', "Symbol Set ID Code",
				  pcl_symbol_set_id_code,
				  pca_neg_error|pca_big_error)
	DEFINE_CLASS_COMMAND_ARGS('(', 'f', 'W', "Define Symbol Set",
				  pcl_define_symbol_set, pca_byte_data|pca_neg_error|pca_big_clamp)
	DEFINE_CLASS_COMMAND_ARGS('*', 'c', 'S', "Symbol Set Control",
				  pcl_symbol_set_control,
				  pca_neg_ignore|pca_big_ignore)
	return 0;
}

static void
pcsymbol_do_reset(pcl_state_t *pcs, pcl_reset_type_t type)
{	
    if ( type & (pcl_reset_initial | pcl_reset_printer | pcl_reset_overlay) ) {
	id_set_value(pcs->symbol_set_id, 0);
	if ( type & pcl_reset_initial ) {
	    /* Don't set a parent relationship from soft to built-in
	     * symbol sets.  Although it is arguably useful, it's
	     * better to avoid it and keep anyone who's looking at the
	     * soft symbol sets from mucking up the permanent ones. */
	    pl_dict_init(&pcs->soft_symbol_sets, pcs->memory,
			 pcsymbol_dict_value_free);
	    pl_dict_init(&pcs->built_in_symbol_sets, pcs->memory,
			 pcsymbol_dict_value_free);
	    /* NB.  Symbol sets are require for RTL/HPGL/2 mode for
	     * stickfonts but we shouldn't load all of them. */
	    if ( pcl_load_built_in_symbol_sets(pcs) < 0 )
		dprintf("Internal error, no symbol sets found");
	}
	else if ( type & pcl_reset_printer ) { 
	    pcl_args_t args;
	    arg_set_uint(&args, 1);	/* delete temporary symbol sets */
	    pcl_symbol_set_control(&args, pcs);
	}
    }
    if ( type & pcl_reset_permanent ) {
	pl_dict_release(&pcs->soft_symbol_sets);
	pl_dict_release(&pcs->built_in_symbol_sets);
    }
}

static int
pcsymbol_do_copy(pcl_state_t *psaved, const pcl_state_t *pcs,
  pcl_copy_operation_t operation)
{	if ( operation & pcl_copy_after )
	  { /* Don't restore the downloaded symbol set dictionary. */
	    psaved->built_in_symbol_sets = pcs->built_in_symbol_sets;
	  }
	return 0;
}

const pcl_init_t pcsymbol_init = {
  pcsymbol_do_registration, pcsymbol_do_reset, pcsymbol_do_copy
};
