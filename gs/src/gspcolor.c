/* Copyright (C) 1993, 1995, 1996, 1997 Aladdin Enterprises.  All rights reserved.
  
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

/* gspcolor.c */
/* Pattern color operators and procedures for Ghostscript library */
#include "math_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsrop.h"
#include "gsstruct.h"
#include "gsutil.h"			/* for gs_next_ids */
#include "gxarith.h"
#include "gxfixed.h"
#include "gxmatrix.h"
#include "gxcoord.h"			/* for gs_concat, gx_tr'_to_fixed */
#include "gxcspace.h"			/* for gscolor2.h */
#include "gxcolor2.h"
#include "gxdcolor.h"
#include "gxdevice.h"
#include "gxdevmem.h"
#include "gxclip2.h"
#include "gspath.h"
#include "gxpath.h"
#include "gxpcolor.h"
#include "gzstate.h"
#include "gsimage.h"

private_st_client_pattern();
public_st_pattern_instance();

/* Import the Pattern reloading procedure from gxpcmap.c. */
int gx_pattern_load(P4(gx_device_color *, const gs_imager_state *,
		       gx_device *, gs_color_select_t));

/* Define the Pattern color space. */
extern cs_proc_remap_color(gx_remap_Pattern);
private cs_proc_install_cspace(gx_install_Pattern);
private cs_proc_adjust_cspace_count(gx_adjust_cspace_Pattern);
private cs_proc_init_color(gx_init_Pattern);
private cs_proc_adjust_color_count(gx_adjust_color_Pattern);
private struct_proc_enum_ptrs(gx_enum_ptrs_Pattern);
private struct_proc_reloc_ptrs(gx_reloc_ptrs_Pattern);
const gs_color_space_type
	gs_color_space_type_Pattern =
	 { gs_color_space_index_Pattern, -1, false,
	   gx_init_Pattern, gx_no_concrete_space,
	   gx_no_concretize_color, NULL,
	   gx_remap_Pattern, gx_install_Pattern,
	   gx_adjust_cspace_Pattern, gx_adjust_color_Pattern,
	   gx_enum_ptrs_Pattern, gx_reloc_ptrs_Pattern
	 };

/* makepattern */
private int compute_inst_matrix(P3(gs_pattern_instance *pinst,
				   const gs_state *saved,
				   gs_rect *pbbox));
private rc_free_proc(rc_free_pattern_instance);
int
gs_makepattern(gs_client_color *pcc, const gs_client_pattern *pcp,
  const gs_matrix *pmat, gs_state *pgs, gs_memory_t *mem)
{	gs_pattern_instance inst;
	gs_pattern_instance *pinst;
	gs_state *saved;
	gs_rect bbox;
	gs_fixed_rect cbox;
	int code;

	if ( mem == 0 )
	  mem = gs_state_memory(pgs);
	rc_alloc_struct_1(pinst, gs_pattern_instance, &st_pattern_instance,
			  mem, return_error(gs_error_VMerror),
			  "gs_makepattern");
	pinst->rc.free = rc_free_pattern_instance;
	inst.rc = pinst->rc;
	saved = gs_state_copy(pgs, mem);
	if ( saved == 0 )
	  { code = gs_note_error(gs_error_VMerror);
	    goto finst;
	  }
	gs_concat(saved, pmat);
	gs_newpath(saved);
	switch ( pcp->PaintType )
	  {
	  case 1:		/* colored */
	    gs_set_logical_op(saved, lop_default);
	    break;
	  case 2:		/* uncolored */
	    gx_set_device_color_1(saved);
	    break;
	  default:
	    code = gs_note_error(gs_error_rangecheck);
	    goto fsaved;
	  }
	inst.template = *pcp;
	inst.saved = saved;
	code = compute_inst_matrix(&inst, saved, &bbox);
	if ( code < 0 )
	  goto fsaved;
#define mat inst.step_matrix
	if_debug6('t', "[t]step_matrix=[%g %g %g %g %g %g]\n",
		  mat.xx, mat.xy, mat.yx, mat.yy, mat.tx, mat.ty);
	/* Check for singular stepping matrix. */
	if ( fabs(mat.xx * mat.yy - mat.xy * mat.yx) < 1.0e-6 )
	  { code = gs_note_error(gs_error_rangecheck);
	    goto fsaved;
	  }
	if_debug4('t', "[t]bbox=(%g,%g),(%g,%g)\n",
		  bbox.p.x, bbox.p.y, bbox.q.x, bbox.q.y);
	{	float bbw = bbox.q.x - bbox.p.x;
		float bbh = bbox.q.y - bbox.p.y;
		/* If the step and the size agree to within 1/2 pixel, */
		/* make them the same. */
		inst.size.x = (int)(bbw + 0.8);	/* 0.8 is arbitrary */
		inst.size.y = (int)(bbh + 0.8);
		if ( mat.xy == 0 && mat.yx == 0 &&
		     fabs(fabs(mat.xx) - bbw) < 0.5 &&
		     fabs(fabs(mat.yy) - bbh) < 0.5
		   )
		  {	gs_scale(saved, fabs(inst.size.x / mat.xx),
				 fabs(inst.size.y / mat.yy));
			code = compute_inst_matrix(&inst, saved, &bbox);
			if ( code < 0 )
			  goto fsaved;
			if_debug2('t',
				"[t]adjusted XStep & YStep to size=(%d,%d)\n",
				inst.size.x, inst.size.y);
			if_debug4('t', "[t]bbox=(%g,%g),(%g,%g)\n",
				  bbox.p.x, bbox.p.y, bbox.q.x, bbox.q.y);
		  }
	}
	if ( (code = gs_bbox_transform_inverse(&bbox, &mat, &inst.bbox)) < 0 )
	  goto fsaved;
	if_debug4('t', "[t]ibbox=(%g,%g),(%g,%g)\n",
		  inst.bbox.p.x, inst.bbox.p.y, inst.bbox.q.x, inst.bbox.q.y);
	inst.is_simple = (fabs(mat.xx) == inst.size.x && mat.xy == 0 &&
			  mat.yx == 0 && fabs(mat.yy) == inst.size.y);
	if_debug6('t',
		  "[t]is_simple? xstep=(%g,%g) ystep=(%g,%g) size=(%d,%d)\n",
		  inst.step_matrix.xx, inst.step_matrix.xy,
		  inst.step_matrix.yx, inst.step_matrix.yy,
		  inst.size.x, inst.size.y);
	gx_translate_to_fixed(saved, float2fixed(mat.tx - bbox.p.x),
			      float2fixed(mat.ty - bbox.p.y));
	mat.tx = bbox.p.x;
	mat.ty = bbox.p.y;
#undef mat
	cbox.p.x = fixed_0;
	cbox.p.y = fixed_0;
	cbox.q.x = int2fixed(inst.size.x);
	cbox.q.y = int2fixed(inst.size.y);
	code = gx_clip_to_rectangle(saved, &cbox);
	if ( code < 0 )
	  goto fsaved;
	inst.id = gs_next_ids(1);
	*pinst = inst;
	pcc->pattern = pinst;
	return 0;
#undef mat
fsaved:	gs_state_free(saved);
finst:	gs_free_object(mem, pinst, "gs_makepattern");
	return code;
}
/* Compute the stepping matrix and device space instance bounding box */
/* from the step values and the saved matrix. */
private int
compute_inst_matrix(gs_pattern_instance *pinst, const gs_state *saved,
  gs_rect *pbbox)
{	double xx = pinst->template.XStep * saved->ctm.xx;
	double xy = pinst->template.XStep * saved->ctm.xy;
	double yx = pinst->template.YStep * saved->ctm.yx;
	double yy = pinst->template.YStep * saved->ctm.yy;

	/* Adjust the stepping matrix so all coefficients are >= 0. */
	if ( xx == 0 || yy == 0 )
	  { /* We know that both xy and yx are non-zero. */
	    double temp;
	    temp = xx, xx = yx, yx = temp;
	    temp = xy, xy = yy, yy = temp;
	  }
	if ( xx < 0 )
	  xx = -xx, xy = -xy;
	if ( yy < 0 )
	  yx = -yx, yy = -yy;
	/* Now xx > 0, yy > 0. */
	pinst->step_matrix.xx = xx;
	pinst->step_matrix.xy = xy;
	pinst->step_matrix.yx = yx;
	pinst->step_matrix.yy = yy;
	pinst->step_matrix.tx = saved->ctm.tx;
	pinst->step_matrix.ty = saved->ctm.ty;
	return gs_bbox_transform(&pinst->template.BBox, &ctm_only(saved),
				 pbbox);
}
/* Free the saved gstate when freeing a Pattern instance. */
private void
rc_free_pattern_instance(gs_memory_t *mem, void *pinst_void,
  client_name_t cname)
{	gs_pattern_instance *pinst = pinst_void;

	gs_state_free(pinst->saved);
	rc_free_struct_only(mem, pinst_void, cname);
}

/* setpattern */
int
gs_setpattern(gs_state *pgs, const gs_client_color *pcc)
{	int code = gs_setpatternspace(pgs);
	if ( code < 0 )
		return code;
	return gs_setcolor(pgs, pcc);
}

/* setpatternspace */
/* This does all the work of setpattern except for the final setcolor. */
int
gs_setpatternspace(gs_state *pgs)
{	int code = 0;
	if ( pgs->color_space->type->index != gs_color_space_index_Pattern )
	{	gs_color_space cs;
		cs.params.pattern.base_space =
			*(gs_paint_color_space *)pgs->color_space;
		cs.params.pattern.has_base_space = true;
		cs.type = &gs_color_space_type_Pattern;
		code = gs_setcolorspace(pgs, &cs);
	}
	return code;
}

/* getpattern */
/* This is only intended for the benefit of pattern PaintProcs. */
const gs_client_pattern *
gs_getpattern(const gs_client_color *pcc)
{	return &pcc->pattern->template;
}

/*
 *  Code for generating patterns from bitmaps and pixmaps.
 */

/*
 *  The following structures are realized here only because this is the
 *  first location in which they were needed. Otherwise, there is nothing
 *  about them that is specific to patterns.
 */
public_st_gs_bitmap();
public_st_gs_tile_bitmap();
public_st_gs_depth_bitmap();
public_st_gs_tile_depth_bitmap();

/*
 *  Structure for holding a gs_depth_bitmap and the corresponding depth and
 *  colorspace information. The white index is currently unused, but will be
 *  needed when ImageType 4 images are supported.
 */
typedef struct pixmap_info_s {
    gs_depth_bitmap         bitmap;     /* must be first */
    const gs_color_space *  pcspace;
    uint                    white_index;
} pixmap_info;

gs_private_st_suffix_add1( st_pixmap_info,
                           pixmap_info,
                           "pixmap info. struct",
                           pixmap_enum_ptr,
                           pixmap_reloc_ptr,
                           st_gs_depth_bitmap,
                           pcspace
                           );

#define st_pixmap_info_max_ptrs (1 + st_tile_bitmap_max_ptrs)

/*
 *  The following code is to handle the transition from a single image type
 *  to multiple image types. Once the latter become available, the "NOTYET"
 *  code may be removed.
 *
 *  This modification only affects the pixmap_PaintProc and gx_makepixmappattern
 *  functions.
 */
#ifndef NOTYET

typedef gs_image_t  gs_image4_t;
typedef gs_image_t  gs_image_data_t;
typedef gs_image_t  gs_pixel_image_t;

#define gs_pixel_image_t_init(pimage, pcspace)  \
    {                                           \
        gs_image_t_init_color(pimage);          \
        (pimage)->ColorSpace = (pcspace);       \
    }

#define gs_image4_t_init(pimage, pcspace)       \
    gs_pixel_image_t_init((pimage), (pcspace))

#define set_white_index(pimage, white_index)

#else

#define set_white_index(pimage, white_index)    \
    {                                           \
        (pimage)->MaskColor_is_range = false;   \
        (pimage)->MaskColor[0] = (white_index); \
    }

#endif /* NOTYET */


/*
 *  PaintProc for bitmap and pixmap patterns.
 */
private int
pixmap_PaintProc(
    const gs_client_color *     pcolor,
    gs_state *                  pgs
)
{
    gs_image_enum *             pen = gs_image_enum_alloc( gs_state_memory(pgs),
                                                           "bitmap_PaintProc"
                                                           );
    const gs_client_pattern *   ppat = gs_getpattern(pcolor);
    const pixmap_info *         ppmap = ppat->client_data;
    const gs_depth_bitmap *     pbitmap = &(ppmap->bitmap);
    const byte *                dp = pbitmap->data;
    gs_image4_t                 image;
    int                         n;
    uint                        nbytes, raster, used;

    /*
     * Initialize the image structure.
     *
     * When ImageType 4 becomes available, the image type to be used will
     * be selected based on whether or the pattern image can contain the
     * white_index value. Patterns images that cannot have this value are
     * effectively opaque, irrespective of the current transparency setting.
     *
     * The implementation of ImageType 4 should internally verify if the
     * produce transparency mask is blank, and remove it if this is the case.
     */
    if (ppat->PaintType == 2)
        gs_image_t_init_mask(&image, true);
    else {
        const gs_color_space *  pcspace = ( ppmap->pcspace == 0
                                             ? gs_color_space_DeviceGray()
				             : ppmap->pcspace );

        if (ppmap->white_index >= (1 << pbitmap->pix_depth)) {
            gs_pixel_image_t_init((gs_pixel_image_t *)&image, pcspace);
        } else {
            gs_image4_t_init(&image, pcspace);
            set_white_index(&image, pcspace);
        }
        image.Decode[0] = 0;
        image.Decode[1] = (1 << pbitmap->pix_depth) - 1;
        image.BitsPerComponent = pbitmap->pix_depth;

        /* backwards compatibility */
        if (ppmap->pcspace == 0) {
            image.Decode[0] = 1.0;
            image.Decode[1] = 0.0;
        }
    }
    image.Width = pbitmap->size.x;
    image.Height = pbitmap->size.y;

    raster = pbitmap->raster;
    nbytes = (image.Width * pbitmap->pix_depth + 7) >> 3;
    gs_image_init(pen, (gs_image_data_t *)&image, false, pgs);
    if (nbytes == raster)
        (void)gs_image_next(pen, dp, nbytes * image.Height, &used);
    else for (n = image.Height; n > 0; dp += raster, --n)
	(void)gs_image_next(pen, dp, nbytes, &used);
    gs_image_cleanup(pen);

    gs_free_object(gs_state_memory(pgs), pen, "bitmap_PaintProc");
    return 0;
}

/*
 * Make a pattern from a bitmap or pixmap. The pattern may be colored or
 * uncolored, as determined by the mask operand. This code is intended
 * primarily for use by PCL.
 *
 * See the comment prior to the declaration of this function in gscolor2.h
 * for further information.
 */
int
gs_makepixmappattern(
    gs_client_color *       pcc,
    const gs_depth_bitmap * pbitmap,
    bool                    mask,
    const gs_matrix *       pmat,
    long                    id,
    const gs_color_space *  pcspace,
    uint                    white_index,
    gs_state *              pgs,
    gs_memory_t *           mem
)
{

    gs_client_pattern       pat;
    pixmap_info *           ppmap;
    gs_matrix               mat, smat;
    int                     code;

    /* check that the data is legitimate */
    if ((mask) || (pcspace == 0)) {
        if (pbitmap->pix_depth != 1)
            return_error(gs_error_rangecheck);
        pcspace = 0;
    } else if (gs_color_space_get_index(pcspace) != gs_color_space_index_Indexed)
        return_error(gs_error_rangecheck);
    if (pbitmap->num_comps != 1)
        return_error(gs_error_rangecheck);

    /* allocate and initialize a pixmap_info structure for the paint proc */
    if (mem == 0)
        mem = gs_state_memory(pgs);
    ppmap = gs_alloc_struct( mem,
                             pixmap_info,
                             &st_pixmap_info,
                             "makepximappattern"
                             );
    if (ppmap == 0)
        return_error(gs_error_VMerror);
    ppmap->bitmap = *pbitmap;
    ppmap->pcspace = pcspace;
    ppmap->white_index = white_index;

    /* set up the client pattern structure */
    uid_set_UniqueID(&pat.uid, (id == no_UniqueID) ? gs_next_ids(1) : id );
    pat.PaintType = (mask ? 2 : 1);
    pat.TilingType = 1;
    pat.BBox.p.x = 0;
    pat.BBox.p.y = 0;
    pat.BBox.q.x = pbitmap->size.x;
    pat.BBox.q.y = pbitmap->size.y;
    pat.XStep = pbitmap->size.x;
    pat.YStep = pbitmap->size.y;
    pat.PaintProc = pixmap_PaintProc;
    pat.client_data = ppmap;

    /* set the ctm to be the identity */
    gs_currentmatrix(pgs, &smat);
    gs_make_identity(&mat);
    gs_setmatrix(pgs, &mat);

    /* build the pattern, restore the previous matrix */
    if (pmat == NULL)
        pmat = &mat;
    if ((code = gs_makepattern(pcc, &pat, pmat, pgs, mem)) != 0)
        gs_free_object(mem, ppmap, "makebitmappattern_xform");
    gs_setmatrix(pgs, &smat);
    return code;
}

/*
 *  Backwards compatibility.
 */
int
gs_makebitmappattern_xform(
    gs_client_color *       pcc,
    const gx_tile_bitmap *  ptile,
    bool                    mask,
    const gs_matrix *       pmat,
    long                    id,
    gs_state *              pgs,
    gs_memory_t *           mem
)
{
    gs_depth_bitmap         bitmap;

    /* build the bitmap the size of one repetition */
    bitmap.data = ptile->data;
    bitmap.raster = ptile->raster;
    bitmap.size.x = ptile->rep_width;
    bitmap.size.y = ptile->rep_height;
    bitmap.id = ptile->id;  /* shouldn't matter */
    bitmap.pix_depth = 1;
    bitmap.num_comps = 1;

    return gs_makepixmappattern(pcc, &bitmap, mask, pmat, id, 0, 0, pgs, mem);
}


/* ------ Color space implementation ------ */

/* Pattern device color types. */
/* We need a masked analogue of each of the non-pattern types, */
/* to handle uncolored patterns. */
/* We use 'masked_fill_rect' instead of 'masked_fill_rectangle' */
/* in order to limit identifier lengths to 32 characters. */
private dev_color_proc_load(gx_dc_pattern_load);
private dev_color_proc_fill_rectangle(gx_dc_pattern_fill_rectangle);
private struct_proc_enum_ptrs(dc_pattern_enum_ptrs);
private struct_proc_reloc_ptrs(dc_pattern_reloc_ptrs);
private dev_color_proc_load(gx_dc_pure_masked_load);
private dev_color_proc_fill_rectangle(gx_dc_pure_masked_fill_rect);
private struct_proc_enum_ptrs(dc_masked_enum_ptrs);
private struct_proc_reloc_ptrs(dc_masked_reloc_ptrs);
private dev_color_proc_load(gx_dc_binary_masked_load);
private dev_color_proc_fill_rectangle(gx_dc_binary_masked_fill_rect);
private struct_proc_enum_ptrs(dc_binary_masked_enum_ptrs);
private struct_proc_reloc_ptrs(dc_binary_masked_reloc_ptrs);
private dev_color_proc_load(gx_dc_colored_masked_load);
private dev_color_proc_fill_rectangle(gx_dc_colored_masked_fill_rect);
/* The device color types are exported for gxpcmap.c. */
const gx_device_color_procs
  gx_dc_pattern =
    { gx_dc_pattern_load, gx_dc_pattern_fill_rectangle,
      gx_dc_default_fill_masked,
      dc_pattern_enum_ptrs, dc_pattern_reloc_ptrs
    },
  gx_dc_pure_masked =
    { gx_dc_pure_masked_load, gx_dc_pure_masked_fill_rect,
      gx_dc_default_fill_masked,
      dc_masked_enum_ptrs, dc_masked_reloc_ptrs
    },
  gx_dc_binary_masked =
    { gx_dc_binary_masked_load, gx_dc_binary_masked_fill_rect,
      gx_dc_default_fill_masked,
      dc_binary_masked_enum_ptrs, dc_binary_masked_reloc_ptrs
    },
  gx_dc_colored_masked =
    { gx_dc_colored_masked_load, gx_dc_colored_masked_fill_rect,
      gx_dc_default_fill_masked,
      dc_masked_enum_ptrs, dc_masked_reloc_ptrs
    };
#undef gx_dc_type_pattern
const gx_device_color_procs _ds *gx_dc_type_pattern = &gx_dc_pattern;
#define gx_dc_type_pattern (&gx_dc_pattern)
/* GC procedures */
#define cptr ((gx_device_color *)vptr)
private ENUM_PTRS_BEGIN(dc_pattern_enum_ptrs) {
	return dc_masked_enum_ptrs(vptr, size, index - 1, pep);
	}
	case 0:
	{	gx_color_tile *tile = cptr->colors.pattern.p_tile;
		ENUM_RETURN((tile == 0 ? tile : tile - tile->index));
	}
ENUM_PTRS_END
private RELOC_PTRS_BEGIN(dc_pattern_reloc_ptrs) {
	gx_color_tile *tile = cptr->colors.pattern.p_tile;
	if ( tile != 0 )
	  {	uint index = tile->index;
		RELOC_TYPED_OFFSET_PTR(gx_device_color, colors.pattern.p_tile, index);
	  }
	dc_masked_reloc_ptrs(vptr, size, gcst);
} RELOC_PTRS_END
private ENUM_PTRS_BEGIN(dc_masked_enum_ptrs) ENUM_SUPER(gx_device_color, st_client_color, mask.ccolor, 1);
	case 0:
	{	gx_color_tile *mask = cptr->mask.m_tile;
		ENUM_RETURN((mask == 0 ? mask : mask - mask->index));
	}
ENUM_PTRS_END
private RELOC_PTRS_BEGIN(dc_masked_reloc_ptrs) {
	gx_color_tile *mask = cptr->mask.m_tile;
	RELOC_SUPER(gx_device_color, st_client_color, mask.ccolor);
	if ( mask != 0 )
	  {	uint index = mask->index;
		RELOC_TYPED_OFFSET_PTR(gx_device_color, mask.m_tile, index);
	  }
} RELOC_PTRS_END
private ENUM_PTRS_BEGIN(dc_binary_masked_enum_ptrs) {
	return (*gx_dc_procs_ht_binary.enum_ptrs)(vptr, size, index - 2, pep);
	}
	case 0:
	case 1:
	  return dc_masked_enum_ptrs(vptr, size, index, pep);
ENUM_PTRS_END
private RELOC_PTRS_BEGIN(dc_binary_masked_reloc_ptrs) {
	dc_masked_reloc_ptrs(vptr, size, gcst);
	(*gx_dc_procs_ht_binary.reloc_ptrs)(vptr, size, gcst);
} RELOC_PTRS_END
#undef cptr

/* Macros for pattern loading */
#define FINISH_PATTERN_LOAD\
	while ( !gx_pattern_cache_lookup(pdevc, pis, dev, select) )\
	 { code = gx_pattern_load(pdevc, pis, dev, select);\
	   if ( code < 0 ) break;\
	 }\
	return code;

/* Ensure that a colored Pattern is loaded in the cache. */
private int
gx_dc_pattern_load(gx_device_color *pdevc, const gs_imager_state *pis,
  gx_device *dev, gs_color_select_t select)
{	int code = 0;
	FINISH_PATTERN_LOAD
}
/* Ensure that an uncolored Pattern is loaded in the cache. */
private int
gx_dc_pure_masked_load(gx_device_color *pdevc, const gs_imager_state *pis,
  gx_device *dev, gs_color_select_t select)
{	int code = (*gx_dc_procs_pure.load)(pdevc, pis, dev, select);
	if ( code < 0 )
	  return code;
	FINISH_PATTERN_LOAD
}
private int
gx_dc_binary_masked_load(gx_device_color *pdevc, const gs_imager_state *pis,
  gx_device *dev, gs_color_select_t select)
{	int code = (*gx_dc_procs_ht_binary.load)(pdevc, pis, dev, select);
	if ( code < 0 )
	  return code;
	FINISH_PATTERN_LOAD
}
private int
gx_dc_colored_masked_load(gx_device_color *pdevc, const gs_imager_state *pis,
  gx_device *dev, gs_color_select_t select)
{	int code = (*gx_dc_procs_ht_colored.load)(pdevc, pis, dev, select);
	if ( code < 0 )
	  return code;
	FINISH_PATTERN_LOAD
}

/* Look up a pattern color in the cache. */
bool
gx_pattern_cache_lookup(gx_device_color *pdevc, const gs_imager_state *pis,
  gx_device *dev, gs_color_select_t select)
{	gx_pattern_cache *pcache = pis->pattern_cache;
	gx_bitmap_id id = pdevc->mask.id;

	if ( id == gx_no_bitmap_id )
	  {	color_set_null_pattern(pdevc);
		return true;
	  }
	if ( pcache != 0 )
	  { gx_color_tile *ctile = &pcache->tiles[id % pcache->num_tiles];
	    if ( ctile->id == id &&
		(pdevc->type != &gx_dc_pattern ||
		 ctile->depth == dev->color_info.depth)
	       )
	      { if ( pdevc->type == &gx_dc_pattern ) /* colored */
		  { pdevc->colors.pattern.p_tile = ctile;
		    color_set_phase_mod(pdevc, pis->screen_phase[select].x,
					pis->screen_phase[select].y,
					ctile->tbits.rep_width,
					ctile->tbits.rep_height);
		  }
		pdevc->mask.m_tile =
		  (ctile->tmask.data == 0 ? (gx_color_tile *)0 :
		   ctile);
		return true;
	      }
	  }
	return false;
}

#undef FINISH_PATTERN_LOAD

/*
 * The filling procedures below disregard the phase in the color.
 * We don't see how this can possibly work, but it does; and if we
 * do take the phase into account, we get incorrect output.
 */

/* Macros for filling with a possibly masked pattern. */
#define BEGIN_PATTERN_FILL\
	  {	gx_device_tile_clip cdev;\
		gx_device *pcdev;\
		const gx_strip_bitmap *tmask;\
		int code;\
\
		if ( pdevc->mask.m_tile == 0 )	{ /* no clipping */\
		  code = 0;\
		  pcdev = dev;\
		} else {\
		  tmask = &pdevc->mask.m_tile->tmask;\
		  code = tile_clip_initialize(&cdev, tmask, dev, 0, 0);\
		  if ( code < 0 )\
		    return code;\
		  pcdev = (gx_device *)&cdev;\
		}
#define CLIPPING_FILL (pcdev == (gx_device *)&cdev)
#define END_PATTERN_FILL\
		return code;\
	  }

/* Macros for filling with non-standard X and Y stepping. */
/* Free variables: x, y, w, h, ptile. */
/* tbits_or_tmask is whichever of tbits and tmask is supplying */
/* the tile size. */
/* This implementation could be sped up considerably! */
#define BEGIN_STEPS(tbits_or_tmask)\
	  {	gs_rect bbox, ibbox;\
		gs_point offset;\
		int x0 = x, x1 = x + w, y0 = y, y1 = y + h;\
		int w0 = w, h0 = h;\
		int i0, i1, j0, j1, i, j;\
\
		bbox.p.x = x0, bbox.p.y = y0;\
		bbox.q.x = x1, bbox.q.y = y1;\
		gs_bbox_transform_inverse(&bbox, &ptile->step_matrix, &ibbox);\
		if_debug10('T',\
			  "[T]x,y=(%d,%d) w,h=(%d,%d) => (%g,%g),(%g,%g), offset=(%g,%g)\n",\
			  x, y, w, h,\
			  ibbox.p.x, ibbox.p.y, ibbox.q.x, ibbox.q.y,\
			  ptile->step_matrix.tx, ptile->step_matrix.ty);\
		offset.x = ptile->step_matrix.tx;\
		offset.y = ptile->step_matrix.ty;\
		i0 = (int)floor(ibbox.p.x - ptile->bbox.q.x + 0.000001);\
		i1 = (int)ceil(ibbox.q.x - ptile->bbox.p.x - 0.000001);\
		j0 = (int)floor(ibbox.p.y - ptile->bbox.q.y + 0.000001);\
		j1 = (int)ceil(ibbox.q.y - ptile->bbox.p.y - 0.000001);\
		if_debug4('T', "[T]i=(%d,%d) j=(%d,%d)\n", i0, i1, j0, j1);\
		for ( i = i0; i < i1; i++ )\
		  for ( j = j0; j < j1; j++ )\
		  {	int xoff, yoff;\
			x = (int)(ptile->step_matrix.xx * i +\
				  ptile->step_matrix.yx * j + offset.x);\
			y = (int)(ptile->step_matrix.xy * i +\
				  ptile->step_matrix.yy * j + offset.y);\
			if_debug4('T', "[T]i=%d j=%d x,y=(%d,%d)", i, j, x, y);\
			w = ptile->tbits_or_tmask.size.x;\
			h = ptile->tbits_or_tmask.size.y;\
			if ( x < x0 ) xoff = x0 - x, x = x0, w -= xoff;\
			else xoff = 0;\
			if ( y < y0 ) yoff = y0 - y, y = y0, h -= yoff;\
			else yoff = 0;\
			if ( x + w > x1 ) w = x1 - x;\
			if ( y + h > y1 ) h = y1 - y;\
			if_debug4('T', "=>(%d,%d) w,h=(%d,%d)\n",\
				  x, y, w, h);\
			if ( w > 0 && h > 0 )\
			  {	if ( CLIPPING_FILL )\
				  tile_clip_set_phase(&cdev,\
						      imod(xoff - x, tmask->rep_width),\
						      imod(yoff - y, tmask->rep_height))
#define SOURCE_STEP(src)\
				(src).sdata = source->sdata + (y - y0) * source->sraster;\
				(src).sourcex = source->sourcex + (x - x0);\
				(src).sraster = source->sraster;\
				(src).id = (w == w0 && h == h0 ?\
					    source->id : gx_no_bitmap_id);\
				(src).scolors[0] = source->scolors[0];\
				(src).scolors[1] = source->scolors[1];\
				(src).use_scolors = source->use_scolors
#define END_STEPS\
			  }\
		  }\
	  }

/* Fill a rectangle with a colored Pattern. */
/* Note that we treat this as "texture" for RasterOp. */
private int
gx_dc_pattern_fill_rectangle(const gx_device_color *pdevc, int x, int y,
  int w, int h, gx_device *dev, gs_logical_operation_t lop,
  const gx_rop_source_t *source)
{	gx_color_tile *ptile = pdevc->colors.pattern.p_tile;
	const gx_rop_source_t *rop_source = source;
	gx_rop_source_t no_source;
	gx_strip_bitmap *bits;

	if ( ptile == 0 )	/* null pattern */
	  return 0;
	if ( rop_source == NULL )
	  set_rop_no_source(rop_source, no_source, dev);
	bits = &ptile->tbits;
	BEGIN_PATTERN_FILL
	if ( ptile->is_simple )
	  {	int px = imod(-(int)(ptile->step_matrix.tx + 0.5),
			      bits->rep_width);
	  	int py = imod(-(int)(ptile->step_matrix.ty + 0.5),
			      bits->rep_height);

		if ( pcdev != dev )
		  tile_clip_set_phase(&cdev, px, py);
		if ( source == NULL && lop_no_S_is_T(lop) )
		  code = (*dev_proc(pcdev, strip_tile_rectangle))(pcdev, bits,
			x, y, w, h,
			gx_no_color_index, gx_no_color_index, px, py);
		else
		  code = (*dev_proc(dev, strip_copy_rop))(dev,
			rop_source->sdata, rop_source->sourcex,
			rop_source->sraster, rop_source->id,
			(rop_source->use_scolors ? rop_source->scolors : NULL),
			bits, NULL, x, y, w, h, px, py, lop);
	  }
	else
	  {	BEGIN_STEPS(tbits);
		  { const byte *data = bits->data;
		    bool full_transfer = (w == w0 && h == h0);
		    gx_bitmap_id source_id =
		      (full_transfer ? rop_source->id : gx_no_bitmap_id);
		      
		    if ( source == NULL && lop_no_S_is_T(lop) )
		      code = (*dev_proc(pcdev, copy_color))(pcdev,
				data + bits->raster * yoff, xoff,
				bits->raster,
				(full_transfer ? bits->id : gx_no_bitmap_id),
				x, y, w, h);
		    else
		      { gx_strip_bitmap data_tile;

			data_tile.data = (byte *)data;	/* actually const */
			data_tile.raster = bits->raster;
			data_tile.size.x = data_tile.rep_width =
			  ptile->tbits.size.x;
			data_tile.size.y = data_tile.rep_height =
			  ptile->tbits.size.y;
			data_tile.id = bits->id;
			data_tile.shift = data_tile.rep_shift = 0;
			code = (*dev_proc(dev, strip_copy_rop))(dev,
				rop_source->sdata + (y - y0) * rop_source->sraster,
				rop_source->sourcex + (x - x0),
				rop_source->sraster, source_id,
				(rop_source->use_scolors ?
				 rop_source->scolors : NULL),
				&data_tile, NULL,
				x, y, w, h,
				imod(xoff - x, data_tile.rep_width),
				imod(yoff - y, data_tile.rep_height),
				lop);
		      }
		    if ( code < 0 )
		      return code;
		  }
		END_STEPS
	  }
	END_PATTERN_FILL
}
/* Fill a rectangle with an uncolored Pattern. */
/* Note that we treat this as "texture" for RasterOp. */
private int
gx_dc_pure_masked_fill_rect(const gx_device_color *pdevc, int x, int y,
  int w, int h, gx_device *dev, gs_logical_operation_t lop,
  const gx_rop_source_t *source)
{	gx_color_tile *ptile = pdevc->mask.m_tile;

	BEGIN_PATTERN_FILL
	if ( ptile == 0 || ptile->is_simple )
		code = (*gx_dc_procs_pure.fill_rectangle)(pdevc, x, y, w, h,
					pcdev, lop, source);
	else
	  {	BEGIN_STEPS(tmask);
		if ( source == NULL )
		  code = (*gx_dc_procs_pure.fill_rectangle)(pdevc, x, y, w, h,
					pcdev, lop, source);
		else
		  { gx_rop_source_t step_source;
		    SOURCE_STEP(step_source);
		    code = (*gx_dc_procs_pure.fill_rectangle)(pdevc,
					x, y, w, h,
					pcdev, lop, &step_source);
		  }
		if ( code < 0 )
		  return code;
		END_STEPS
	  }
	END_PATTERN_FILL
}
private int
gx_dc_binary_masked_fill_rect(const gx_device_color *pdevc, int x, int y,
  int w, int h, gx_device *dev, gs_logical_operation_t lop,
  const gx_rop_source_t *source)
{	gx_color_tile *ptile = pdevc->mask.m_tile;

	BEGIN_PATTERN_FILL
	if ( ptile == 0 || ptile->is_simple )
		code = (*gx_dc_procs_ht_binary.fill_rectangle)(pdevc,
					x, y, w, h,
					pcdev, lop, source);
	else
	  {	BEGIN_STEPS(tmask);
		if ( source == NULL )
		  code = (*gx_dc_procs_ht_binary.fill_rectangle)(pdevc,
					x, y, w, h,
					pcdev, lop, source);
		else
		  { gx_rop_source_t step_source;
		    SOURCE_STEP(step_source);
		    code = (*gx_dc_procs_ht_binary.fill_rectangle)(pdevc,
					x, y, w, h,
					pcdev, lop, &step_source);
		  }
		if ( code < 0 )
		  return code;
		END_STEPS
	  }
	END_PATTERN_FILL
}
private int
gx_dc_colored_masked_fill_rect(const gx_device_color *pdevc, int x, int y,
  int w, int h, gx_device *dev, gs_logical_operation_t lop,
  const gx_rop_source_t *source)
{	gx_color_tile *ptile = pdevc->mask.m_tile;

	BEGIN_PATTERN_FILL
	if ( ptile == 0 || ptile->is_simple )
		code = (*gx_dc_procs_ht_colored.fill_rectangle)(pdevc,
					x, y, w, h,
					pcdev, lop, source);
	else
	  {	BEGIN_STEPS(tmask);
		if ( source == NULL )
		  code = (*gx_dc_procs_ht_colored.fill_rectangle)(pdevc,
					x, y, w, h,
					pcdev, lop, source);
		else
		  { gx_rop_source_t step_source;
		    SOURCE_STEP(step_source);
		    code = (*gx_dc_procs_ht_colored.fill_rectangle)(pdevc,
					x, y, w, h,
					pcdev, lop, &step_source);
		  }
		if ( code < 0 )
		  return code;
		END_STEPS
	  }
	END_PATTERN_FILL
}

#undef BEGIN_PATTERN_FILL
#undef END_PATTERN_FILL

/* Initialize a Pattern color. */
private void
gx_init_Pattern(gs_client_color *pcc, const gs_color_space *pcs)
{	if ( pcs->params.pattern.has_base_space )
	{	const gs_color_space *pbcs =
		  (const gs_color_space *)&pcs->params.pattern.base_space;
		cs_init_color(pcc, pbcs);
	}
	/*pcc->pattern = 0;*/		/* cs_full_init_color handles this */
}

/* Install a Pattern color space. */
private int
gx_install_Pattern(gs_color_space *pcs, gs_state *pgs)
{	if ( !pcs->params.pattern.has_base_space )
		return 0;
	return (*pcs->params.pattern.base_space.type->install_cspace)
		((gs_color_space *)&pcs->params.pattern.base_space, pgs);
}

/* Adjust the reference counts for Pattern color spaces or colors. */
private void
gx_adjust_cspace_Pattern(const gs_color_space *pcs, gs_memory_t *mem,
  int delta)
{	if ( pcs->params.pattern.has_base_space )
	  (*pcs->params.pattern.base_space.type->adjust_cspace_count)
	    ((const gs_color_space *)&pcs->params.pattern.base_space, mem, delta);
}

private void
gx_adjust_color_Pattern(const gs_client_color *pcc, const gs_color_space *pcs,
  gs_memory_t *mem, int delta)
{	gs_pattern_instance *pinst = pcc->pattern;

	rc_adjust_only(pinst, delta, "gx_adjust_color_Pattern");
	if ( pcs->params.pattern.has_base_space )
	  (*pcs->params.pattern.base_space.type->adjust_color_count)
	    (pcc, (const gs_color_space *)&pcs->params.pattern.base_space,
	     mem, delta);
}

/* GC procedures */

#define pcs ((gs_color_space *)vptr)

private ENUM_PTRS_BEGIN_PROC(gx_enum_ptrs_Pattern) {
	if ( !pcs->params.pattern.has_base_space )
	  return 0;
	return (*pcs->params.pattern.base_space.type->enum_ptrs)
		 (&pcs->params.pattern.base_space,
		  sizeof(pcs->params.pattern.base_space), index, pep);
} ENUM_PTRS_END_PROC
private RELOC_PTRS_BEGIN(gx_reloc_ptrs_Pattern) {
	if ( !pcs->params.pattern.has_base_space )
	  return;
	(*pcs->params.pattern.base_space.type->reloc_ptrs)
	  (&pcs->params.pattern.base_space, sizeof(gs_paint_color_space), gcst);
} RELOC_PTRS_END

#undef pcs
