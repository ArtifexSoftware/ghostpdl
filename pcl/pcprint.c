/* Copyright (C) 1996, 1997 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* pcprint.c */
/* PCL5 print model commands */
#include "stdio_.h"			/* std.h + NULL */
#include "plvalue.h"
#include "pcommand.h"
#include "pcstate.h"
#include "pcdraw.h"
#include "gsbitops.h"
#include "gsmatrix.h"
#include "gscoord.h"
#include "gsrop.h"
#include "gsstate.h"
#include "gxbitmap.h"
#include "gxdevice.h"		/* for gxpcolor.h */
#include "gxpcolor.h"		/* for pattern cache winnowing */

/* Source transparency setting is in rtmisc.c. */

private int /* ESC * v <bool> O */
pcl_pattern_transparency_mode(pcl_args_t *pargs, pcl_state_t *pcls)
{	uint i = uint_arg(pargs);
	if ( i > 1 )
	  return 0;
	pcls->pattern_transparent = !i;
	gs_settexturetransparent(pcls->pgs, pcls->pattern_transparent);
	return 0;
}

private int /* ESC * c <#/id> G */
pcl_pattern_id(pcl_args_t *pargs, pcl_state_t *pcls)
{	uint id = int_arg(pargs);

	if ( id_value(pcls->pattern_id) != id )
	  { id_set_value(pcls->pattern_id, id);
	    pcls->pattern_set = false;
	  }
	return 0;
}

private int /* ESC * v <enum> T */
pcl_select_pattern(pcl_args_t *pargs, pcl_state_t *pcls)
{	uint i = uint_arg(pargs);

	if ( i > 4 )
	  return 0;
	if ( i != pcls->pattern_type ||
	     (i >= 2 &&
	      id_value(pcls->current_pattern_id) != id_value(pcls->pattern_id))
	   )
	  { pcls->pattern_type = i;
	    pcls->current_pattern_id = pcls->pattern_id;
	    pcls->pattern_set = false;
	  }
	return 0;
}

/* build a pattern and create a dictionary entry for it.  Also used by
   hpgl/2 for raster fills. */
int
pcl_store_user_defined_pattern(pcl_state_t *pcls, pl_dict_t *pattern_dict, 
			       pcl_id_t pattern_id, 
			       const byte *data, uint size)
{
	uint height, width;
	uint raster, bytes, start, stored_raster;
	byte *header;

#define ppat ((const pcl_pattern_t *)(data - pattern_extra))
	/*
	 * Amazingly enough, H-P added X and Y resolution fields
	 * in the LJ4, but didn't change the format number!
	 * The only way we can tell the difference is by checking
	 * the byte count.  It appears that in the LJ5, they changed
	 * the format number for resolution-bound patterns to 20,
	 * the same as for bitmap font headers, but this is not documented
	 * in the TRM.
	 */
	if ( (ppat->format != 0 && ppat->format != 20) ||
	     ppat->continuation != 0 ||
	     ppat->encoding != 1 || ppat->reserved != 0
	   )
	  {
	    dprintf4("? format %02x continuation %02x encoding %02x reserved %02x\n",
		     ppat->format, ppat->continuation,
		     ppat->encoding, ppat->reserved);
	    return e_Range;
	  }
	height = pl_get_uint16(ppat->height);
	width = pl_get_uint16(ppat->width);
#undef ppat
	/*
	 * The comment on p. 13-24 of the PCL5 reference manual
	 * that "There must be an even number of bytes in user-defined
	 * pattern data" would seem to imply that each row must be
	 * padded to an even number of bytes; but in fact, it is
	 * possible to have an odd height and an odd width.
	 */
	raster = (width + 7) >> 3;
	bytes = height * raster;
	if ( bytes + 8 > size )
	  return e_Range;
	start = size - bytes;	/* start of downloaded bitmap data */
	stored_raster = bitmap_raster(width);
	pl_dict_undef(pattern_dict, id_key(pattern_id), 2);
	header = gs_alloc_bytes(pcls->memory,
				pattern_data_offset +
				((height + 7) & -8) * stored_raster,
				"pcl pattern");
	if ( header == 0 )
	  return_error(e_Memory);
	memcpy(header + pattern_extra, data,
	       min(sizeof(pcl_pattern_t), start));
#define ppat ((pcl_pattern_t *)header)
	/*
	 * According to p. 13-18 of the PCL5 manual, patterns with
	 * no specified resolution use 300 dpi, rather than the
	 * resolution of the printer.
	 */
	if ( start <= offset_of(pcl_pattern_t, x_resolution) - pattern_extra )
	  { ppat->x_resolution[0] = 300>>8, ppat->x_resolution[1] = 300&0xff;
	    ppat->y_resolution[0] = 300>>8, ppat->y_resolution[1] = 300&0xff;
	  }
	ppat->storage = pcds_temporary;
#undef ppat
	bytes_copy_rectangle(header + pattern_data_offset, stored_raster,
			     data + start, raster, raster, height);
	pl_dict_put(pattern_dict, id_key(pattern_id), 2, header);
	return 0;
}

private int /* ESC * c <nbytes> W */
pcl_user_defined_pattern(pcl_args_t *pargs, pcl_state_t *pcls)
{	uint count = arg_data_size(pargs);
	const byte *data = arg_data(pargs);
	if ( count < 8 )
	  return e_Range;

	{ int code = pcl_store_user_defined_pattern(pcls, &pcls->patterns, 
						    pcls->pattern_id,
						    data, count);

	  if ( code != 0)
	    { lprintf1("Invalid return from storing pattern: %d\n", code);
	      return code;
	    }
	}
	if ( pcls->pattern_type == pcpt_user_defined &&
	     id_value(pcls->current_pattern_id) == id_value(pcls->pattern_id)
	   )
	  pcls->pattern_set = false;
	return 0;
}

private int /* ESC * p <fixed?> R */
pcl_set_pattern_reference_point(pcl_args_t *pargs, pcl_state_t *pcls)
{	int rotate = int_arg(pargs);

	if ( rotate > 1 )
	  return 0;
	pcls->shift_patterns = true;
	pcls->rotate_patterns = rotate == 0;
	return 0;
}

/* Purge deleted entries from the graphics library's Pattern cache. */
private bool
choose_deleted_patterns(gx_color_tile *ctile, void *pcls_data)
{	pcl_state_t *pcls = (pcl_state_t *)pcls_data;
	long id = ctile->uid.id;
	pcl_id_t key;
	void *ignore_value;

	if ( id & ~0xffffL )
	  return false;		/* not a user-defined pattern */
	id_set_value(key, (uint)id);
	return !pl_dict_find(&pcls->patterns, id_key(key), 2, &ignore_value);
}
private void
pcl_purge_pattern_cache(pcl_state_t *pcls)
{	gx_pattern_cache *pcache = gstate_pattern_cache(pcls->pgs);

	if ( pcache != 0 )
	  gx_pattern_cache_winnow(pcache, choose_deleted_patterns,
				  (void *)pcls);
}

private int /* ESC * c <pc_enum> Q */
pcl_pattern_control(pcl_args_t *pargs, pcl_state_t *pcls)
{	gs_const_string key;
	void *value;
	pl_dict_enum_t denum;

	switch ( int_arg(pargs) )
	  {
	  case 0:
	    { /* Delete all user-defined patterns. */
	      if ( pl_dict_length(&pcls->patterns, false) != 0 )
		{ pl_dict_release(&pcls->patterns);
		  pcl_purge_pattern_cache(pcls);
		}
	    }
	    return 0;
	  case 1:
	    { /* Delete all temporary patterns. */
	      bool deleted = false;

	      pl_dict_enum_stack_begin(&pcls->patterns, &denum, false);
	      while ( pl_dict_enum_next(&denum, &key, &value) )
		if ( ((pcl_pattern_t *)value)->storage == pcds_temporary )
		  deleted = true,
		    pl_dict_undef(&pcls->patterns, key.data, key.size);
	      if ( deleted )
		pcl_purge_pattern_cache(pcls);
	    }
	    return 0;
	  case 2:
	    { /* Delete pattern <pattern_id>. */
	      pl_dict_undef(&pcls->patterns, id_key(pcls->pattern_id), 2);
	    }
	    return 0;
	  case 4:
	    { /* Make <pattern_id> temporary. */
	      if ( pl_dict_find(&pcls->patterns, id_key(pcls->pattern_id), 2, &value) )
		((pcl_pattern_t *)value)->storage = pcds_temporary;
	    }
	    return 0;
	  case 5:
	    { /* Make <pattern_id> permanent. */
	      if ( pl_dict_find(&pcls->patterns, id_key(pcls->pattern_id), 2, &value) )
		((pcl_pattern_t *)value)->storage = pcds_permanent;
	    }
	    return 0;
	  default:
	    return 0;
	  }
}

/* Initialization */
private int
pcprint_do_init(gs_memory_t *mem)
{		/* Register commands */
	DEFINE_CLASS('*')
	  {'v', 'O',
	     PCL_COMMAND("Pattern Transparency Mode",
			 pcl_pattern_transparency_mode,
			 pca_neg_ignore|pca_big_ignore)},
	  {'c', 'G',
	     PCL_COMMAND("Pattern ID", pcl_pattern_id,
			 pca_neg_ignore|pca_big_ignore)},
	  {'v', 'T',
	     PCL_COMMAND("Select Pattern", pcl_select_pattern,
			 pca_neg_ignore|pca_big_ignore)},
	  {'c', 'W',
	     PCL_COMMAND("User Defined Pattern", pcl_user_defined_pattern,
			 pca_bytes)},
	  {'p', 'R',
	     PCL_COMMAND("Set Pattern Reference Point",
			 pcl_set_pattern_reference_point,
			 pca_neg_ignore|pca_big_ignore)},
	  {'c', 'Q',
	     PCL_COMMAND("Pattern Control", pcl_pattern_control,
			 pca_neg_ignore|pca_big_ignore)},
	END_CLASS
	return 0;
}
private void
pcprint_do_reset(pcl_state_t *pcls, pcl_reset_type_t type)
{	if ( type & (pcl_reset_initial | pcl_reset_printer) )
	  { id_set_value(pcls->pattern_id, 0);
	    pcls->pattern_type = pcpt_solid_black;
	    pcls->pattern_transparent = true;
	    gs_settexturetransparent(pcls->pgs, true);
	    pcls->shift_patterns = false;
	    pcls->rotate_patterns = true;
	    pcls->pattern_set = false;
	    if ( type & pcl_reset_initial )
	      pl_dict_init(&pcls->patterns, pcls->memory, NULL);
	    else
	      { pcl_args_t args;
	        arg_set_uint(&args, 1);	/* delete temporary patterns */
		pcl_pattern_control(&args, pcls);
	      }
	  }
}
private int
pcprint_do_copy(pcl_state_t *psaved, const pcl_state_t *pcls,
  pcl_copy_operation_t operation)
{	if ( operation & pcl_copy_after )
	  { /* Don't restore the downloaded pattern dictionary. */
	    /**** WHAT IF CURRENT PATTERN WAS DELETED? ****/
	    psaved->patterns = pcls->patterns;
	    gs_settexturetransparent(pcls->pgs, psaved->pattern_transparent);
	    gs_sethalftonephase(pcls->pgs, psaved->pattern_reference_point.x,
				psaved->pattern_reference_point.y);
	  }
	return 0;
}
const pcl_init_t pcprint_init = {
  pcprint_do_init, pcprint_do_reset, pcprint_do_copy
};
