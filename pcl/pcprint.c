/* Copyright (C) 1996 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* pcprint.c */
/* PCL5 print model commands */
#include "std.h"
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
	if ( i != pcls->pattern_type )
	  { pcls->pattern_type = i;
	    pcls->current_pattern_id = pcls->pattern_id;
	    pcls->pattern_set = false;
	  }
	return 0;
}

private int /* ESC * c <nbytes> W */
pcl_user_defined_pattern(pcl_args_t *pargs, pcl_state_t *pcls)
{	uint count = arg_data_size(pargs);
	const byte *data = arg_data(pargs);
	uint height, width;
	uint raster, bytes, start, stored_raster;
	byte *header;

	if ( count < 8 )
	  return e_Range;
#define ppat ((const pcl_pattern_t *)(data - pattern_extra))
	/* Amazingly enough, H-P added X and Y resolution fields */
	/* in the LJ4, but didn't change the format number! */
	/* The only way we can tell the difference is by checking */
	/* the byte count. */
	if ( ppat->format != 0 || ppat->continuation != 0 ||
	     ppat->encoding != 1 || ppat->reserved != 0
	   )
	  return e_Range;
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
	if ( bytes + 8 > count )
	  return e_Range;
	start = count - bytes;	/* start of downloaded bitmap data */
	stored_raster = bitmap_raster(width);
	pl_dict_undef(&pcls->patterns, id_key(pcls->pattern_id), 2);
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
	pl_dict_put(&pcls->patterns, id_key(pcls->pattern_id), 2, header);
	if ( pcls->pattern_type == pcpt_user_defined &&
	     id_value(pcls->current_pattern_id) == id_value(pcls->pattern_id)
	   )
	  pcls->pattern_set = false;
	return 0;
#undef ppat
}

private int /* ESC * p <fixed?> R */
pcl_set_pattern_reference_point(pcl_args_t *pargs, pcl_state_t *pcls)
{	int rotate = int_arg(pargs);

	if ( rotate > 1 )
	  return 0;
	pcls->shift_patterns = true;
	{ gs_point origin;
	  gs_transform(pcls->pgs, (float)pcls->cap.x,
		       (float)pcls->cap.y, &origin);
	  pcls->pattern_reference_point.x = (int)origin.x;
	  pcls->pattern_reference_point.y = (int)origin.y;
	  gs_sethalftonephase(pcls->pgs, pcls->pattern_reference_point.x,
			      pcls->pattern_reference_point.y);
	}
	pcls->rotate_patterns = rotate == 0;
	return 0;
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
	      pl_dict_release(&pcls->patterns);
	    }
	    return 0;
	  case 1:
	    { /* Delete all temporary patterns. */
	      pl_dict_enum_stack_begin(&pcls->patterns, &denum, false);
	      while ( pl_dict_enum_next(&denum, &key, &value) )
		if ( ((pcl_pattern_t *)value)->storage == pcds_temporary )
		  pl_dict_undef(&pcls->patterns, key.data, key.size);
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
	  {'v', 'O', {pcl_pattern_transparency_mode, pca_neg_ignore|pca_big_ignore}},
	  {'c', 'G', {pcl_pattern_id, pca_neg_ignore|pca_big_ignore}},
	  {'v', 'T', {pcl_select_pattern, pca_neg_ignore|pca_big_ignore}},
	  {'c', 'W', {pcl_user_defined_pattern, pca_bytes}},
	  {'p', 'R', {pcl_set_pattern_reference_point, pca_neg_ignore|pca_big_ignore}},
	  {'c', 'Q', {pcl_pattern_control, pca_neg_ignore|pca_big_ignore}},
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
