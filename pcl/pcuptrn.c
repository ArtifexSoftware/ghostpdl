/* Portions Copyright (C) 2001, 2005 artofcode LLC.
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

/* pcuptrn.c - code for PCL and GL/2 user defined patterns */

#include "string_.h"
#include "gx.h"
#include "gsuid.h"
#include "gscsel.h"
#include "gsdevice.h"
#include "gxdevice.h"
#include "gscspace.h"
#include "gxdcolor.h"
#include "gxcolor2.h"
#include "gxpcolor.h"
#include "pldict.h"
#include "pcindxed.h"
#include "pcpatrn.h"
#include "pcbiptrn.h"
#include "pcuptrn.h"

/*
 * GC routines.
 */
private_st_pattern_data_t();
private_st_pattern_t();

/*
 * Free routine for pattern data structure.
 */
  static void
free_pattern_data(
    gs_memory_t *           pmem,
    void *                  pvpat_data,
    client_name_t           cname
)
{
    pcl_pattern_data_t *    ppat_data = (pcl_pattern_data_t *)pvpat_data;

    if ((ppat_data->storage != pcds_internal) && (ppat_data->pixinfo.data != 0))
        gs_free_object(pmem, ppat_data->pixinfo.data, cname);
    gs_free_object(pmem, pvpat_data, cname);
}

/*
 * Build a pattern data structure. This routine is static as pattern 
 * data structures may only be built as part of a pattern.
 *
 * All pattern data structure are built as "temporary". Routines that build
 * internal patterns should modify this as soon as the pattern is built.
 *
 * Returns 0 on success, < 0 in the event of an error. In the latter case,
 * *pppat_data will be set to NULL.
 */
  static int
build_pattern_data(
    pcl_pattern_data_t **   pppat_data,
    const gs_depth_bitmap * ppixinfo,
    pcl_pattern_type_t      type,
    int                     xres,
    int                     yres,
    gs_memory_t *           pmem
)
{
    pcl_pattern_data_t *    ppat_data = 0;

    *pppat_data = 0;
    rc_alloc_struct_1( ppat_data,
                       pcl_pattern_data_t,
                       &st_pattern_data_t,
                       pmem,
                       return e_Memory,
                       "allocate PCL pattern data"
                       );
    ppat_data->rc.free = free_pattern_data;

    ppat_data->pixinfo = *ppixinfo;
    ppat_data->storage = pcds_temporary;
    ppat_data->type = type;
    ppat_data->xres = xres;
    ppat_data->yres = yres;

    *pppat_data = ppat_data;
    return 0;
}

/*
 * Free the rendered portion of a pattern.
 */
static void
free_pattern_rendering(const gs_memory_t *mem,
		       pcl_pattern_t * pptrn
)
{
    if (pptrn->pcol_ccolor != 0) {
        pcl_ccolor_release(pptrn->pcol_ccolor);
        pptrn->pcol_ccolor = 0;
    }
    if (pptrn->pmask_ccolor != 0) {
        pcl_ccolor_release(pptrn->pmask_ccolor);
        pptrn->pmask_ccolor = 0;
    }
}

/*
 * Free routine for patterns. This is exported for the benefit of the code
 * that handles PCL built-in patterns.
 */
  void
pcl_pattern_free_pattern(
    gs_memory_t *   pmem,
    void *          pvptrn,
    client_name_t   cname
)
{
    pcl_pattern_t * pptrn = (pcl_pattern_t *)pvptrn;

    free_pattern_rendering(pmem, pptrn);
    if (pptrn->ppat_data != 0)
        pcl_pattern_data_release(pptrn->ppat_data);
    gs_free_object(pmem, pvptrn, cname);
}

/*
 * Build a PCL pattern.
 *
 * This is expoorted for use by the routines that create the "built in"
 * patterns.
 *
 * Returns 0 if successful, < 0 in the event of an error. In the latter case,
 * *ppptrn will be set to null.
 */
  int
pcl_pattern_build_pattern(
    pcl_pattern_t **        ppptrn,
    const gs_depth_bitmap * ppixinfo,
    pcl_pattern_type_t      type,
    int                     xres,
    int                     yres,
    gs_memory_t *           pmem
)
{
    pcl_pattern_t *         pptrn = 0;
    int                     code = 0;

    *ppptrn = 0;
    pptrn = gs_alloc_struct( pmem,
                             pcl_pattern_t,
                             &st_pattern_t,
                             "create PCL pattern"
                             );
    if (pptrn == 0)
        return e_Memory;

    pptrn->pcol_ccolor = 0;
    pptrn->pmask_ccolor = 0;
    pptrn->orient = 0;
    /* provide a sentinel to guarantee the initial pattern is
       rendered */
    pptrn->ref_pt.x = pptrn->ref_pt.y = -1.0;
    code = build_pattern_data( &(pptrn->ppat_data),
                               ppixinfo,
                               type,
                               xres,
                               yres,
                               pmem
                               );
    if (code < 0) {
        pcl_pattern_free_pattern(pmem, pptrn, "create PCL pattern");
        return code;
    }

    *ppptrn = pptrn;
    return 0;
}

/*
 * Get a PCL user-defined pattern. A null return indicates the pattern is
 * not defined.
 */
  pcl_pattern_t *
pcl_pattern_get_pcl_uptrn( pcl_state_t *pcs, int id )
{
    if (pcs->last_pcl_uptrn_id != id) {
        pcl_id_t        key;

        pcs->last_pcl_uptrn_id = id;
        id_set_value(key, id);
        if ( !pl_dict_lookup( &pcs->pcl_patterns,
                              id_key(key),
                              2,
                              (void **)&pcs->plast_pcl_uptrn,
                              false,
                              NULL
                              ) )
            pcs->plast_pcl_uptrn = 0;
    }

    return pcs->plast_pcl_uptrn;
}

/*
 * Define a PCL user-defined pattern. This procedure updates the cached
 * information, hence it should be used for all definitions. To undefine
 * an entry, set the second operard to null.
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
  static int
define_pcl_ptrn(
    pcl_state_t *   pcs,
    int             id,
    pcl_pattern_t * pptrn,
    bool            gl2
)
{
    pcl_id_t        key;
    pl_dict_t *     pd = (gl2 ? &pcs->gl_patterns : &pcs->pcl_patterns);

    id_set_value(key, id);
    if (pptrn == 0)
        pl_dict_undef(pd, id_key(key), 2);
    else if (pl_dict_put(pd, id_key(key), 2, pptrn) < 0)
        return e_Memory;

    if (gl2) {
        if (pcs->last_gl2_RF_indx == id)
            pcs->plast_gl2_uptrn = pptrn;
    } else { /* pcl pattern */
        if (pcs->last_pcl_uptrn_id == id)
            pcs->plast_pcl_uptrn = pptrn;
    }
    return 0;
}

/*
 * Delete all temporary patterns or all patterns, based on the value of
 * the operand.
 */
  static void
delete_all_pcl_ptrns(
    bool            renderings,
    bool            tmp_only,
    pcl_state_t *   pcs
)
{
    pcl_pattern_t * pptrn;
    pl_dict_enum_t  denum;
    gs_const_string plkey;
    pl_dict_t *     pdict[2];
    int             i;

    pdict[0] = &pcs->pcl_patterns; 
    pdict[1] = &pcs->gl_patterns;

    for (i = 0; i < 2; i++) {
        pl_dict_enum_begin(pdict[i], &denum);
        while (pl_dict_enum_next(&denum, &plkey, (void **)&pptrn)) {
            if (!tmp_only || (pptrn->ppat_data->storage == pcds_temporary)) {
                pcl_id_t    key;
                id_set_key(key, plkey.data);
                define_pcl_ptrn(pcs, id_value(key), NULL, (pdict[i] == &pcs->gl_patterns));
                /* NB this should be checked - if instead of
                   else-if? */
            } else if (renderings)
                free_pattern_rendering(pcs->memory, pptrn);
        }
    }
}

/*
 * Get a GL/2 user defined pattern. A null return indicates there is no pattern
 * defined for the index.
 */
  pcl_pattern_t *
pcl_pattern_get_gl_uptrn(pcl_state_t *pcs, int indx)
{
    if (pcs->last_gl2_RF_indx != indx) {
        pcl_id_t        key;

        pcs->last_gl2_RF_indx = indx;
        id_set_value(key, indx);
        if ( !pl_dict_lookup( &pcs->gl_patterns,
                              id_key(key),
                              2,
                              (void **)(&pcs->plast_gl2_uptrn),
                              false,
                              NULL
                              ) )
            pcs->plast_gl2_uptrn = 0;
    }

    return pcs->plast_gl2_uptrn;
}

/*
 * Create and define a GL/2 user-define pattern. This is the only pattern-
 * control like facility provided for GL/2. To undefine patterns, use null
 * as the second operand. See pcpatrn.h for further information.
 *
 * Note that RF patterns may be either colored or uncolored. At the GL/2 level
 * the determination is made based on whether or not they contain pen indices
 * other than 0 or 1. At this level the determination is based on the depth
 * of the data pixmap: 1 for uncolored, 8 for colored.
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
  int
pcl_pattern_RF(
    int                     indx,
    const gs_depth_bitmap * ppixmap,
    pcl_state_t *           pcs
)
{
    pcl_id_t                key;
    pcl_pattern_t *         pptrn = 0;

    id_set_value(key, indx);

    if (ppixmap != 0) {
        pcl_pattern_type_t  type = ( ppixmap->pix_depth == 1
                                           ? pcl_pattern_uncolored
				           : pcl_pattern_colored );
	/* RF appears to use the resolution of the device contrary to
           what the pcl documentation implies */
	gx_device *pdev = gs_currentdevice(pcs->pgs);
        int code = pcl_pattern_build_pattern( &pptrn,
					      ppixmap,
					      type,
					      pdev->HWResolution[0],
					      pdev->HWResolution[1],
					      pcs->memory
					      );

        if (code < 0)
            return code;

        if (pl_dict_put(&pcs->gl_patterns, id_key(key), 2, pptrn) < 0) {
            pcl_pattern_free_pattern( pcs->memory,
                                      pptrn,
                                      "create GL/2 RF pattern"
                                      );
            return e_Memory;
        }

    } else
        pl_dict_undef(&pcs->gl_patterns, id_key(key), 2);

    if (pcs->last_gl2_RF_indx == indx)
        pcs->plast_gl2_uptrn = pptrn;

    return 0;
}


/*
 * The PCL user-define pattern type. For memory management reasons, this has
 * a transitory existence.
 *
 * This object comes in two forms, with and without resolution. Which form
 * applies is determined based on the size of the received data array as
 * compared to that indicated by the height, width, and depth fields
 *
 * Note: These structures are defined by HP.
 */
typedef struct pcl_upattern0_s {
    byte     format;        /* actually pcl_pattern_type_t */
    byte     cont;          /* continuation; currently unused */
    byte     depth;         /* bits per pixel; 1 or 8 */
    byte     dummy;         /* reserved - currently unused */
    byte     height[2];     /* height in pixels */
    byte     width[2];      /* width in pixels */
    byte     data[1];       /* actual size derived from hgiht, width, and bits */
} pcl_upattern0_t;

typedef struct pcl_upattern1_s {
    byte     format;        /* actually pcl_pattern_type_t */
    byte     cont;          /* continuation; currently unused */
    byte     depth;         /* bits per pixel; 1 or 8 */
    byte     dummy;         /* reserved - currently unused */
    byte     height[2];     /* height in pixels */
    byte     width[2];      /* width in pixels */
    byte     xres[2];       /* width resolution */
    byte     yres[2];       /* height resolution */
    byte     data[1];       /* actual size derived from hgiht, width, and bits */
} pcl_upattern1_t;

/*
 * ESC * c # W
 *
 * Download Pattern
 */
  static int
download_pcl_pattern(
    pcl_args_t *            pargs,
    pcl_state_t *           pcs
)
{
    uint                    count = arg_data_size(pargs);
    const pcl_upattern0_t * puptrn0 = (pcl_upattern0_t *)arg_data(pargs);
    uint                    format, depth, rsize, patsize, ndsize, dsize;
    gs_depth_bitmap         pixinfo;
    int                     xres = 300, yres = 300;
    pcl_pattern_t *         pptrn = 0;
    int                     code = 0;

    if (count < 8)
	return e_Range;

    format = puptrn0->format;
    /* non data size - the size of the parameters that describe the data */
    ndsize = (format == 20 ? sizeof(pcl_upattern1_t) : sizeof(pcl_upattern0_t)) - 1;
    pixinfo.num_comps = 1;
    pixinfo.size.x = (((uint)puptrn0->width[0]) << 8) + puptrn0->width[1];
    pixinfo.size.y = (((uint)puptrn0->height[0]) << 8) + puptrn0->height[1];
    depth = puptrn0->depth & 0xf;
    pixinfo.pix_depth = depth;
    pixinfo.raster = (pixinfo.size.x * depth + 7) / 8;
    rsize = pixinfo.raster * pixinfo.size.y;
    dsize = min(count - ndsize, rsize);
    patsize = (((pixinfo.size.y) * (pixinfo.size.x) * depth) + 7) / 8;

    /* check for legitimate format */
    if ((format == 0) || (format == 20)) {
        if (depth != 1)
            return e_Range;
    } else if ( (format != 1)                  ||
                ((depth != 1) && (depth != 8)) ||
                (pixinfo.size.x == 0)          ||
                (pixinfo.size.y == 0)            )
        return e_Range;

    if (rsize == 0)
        return e_Range;

    /* allocate space for the array */
    pixinfo.data = gs_alloc_bytes(pcs->memory, rsize, "download PCL pattern");

    if (pixinfo.data == 0)
        return e_Memory;

                
    if (format == 20) {
        pcl_upattern1_t *   puptrn1 = (pcl_upattern1_t *)puptrn0;

        xres = (((uint)puptrn1->xres[0]) << 8) + puptrn1->xres[1];
        yres = (((uint)puptrn1->yres[0]) << 8) + puptrn1->yres[1];
        memcpy(pixinfo.data, puptrn1->data, dsize);
    } else {
        memcpy(pixinfo.data, puptrn0->data, dsize);
    }
    if (dsize < rsize)
        memset(pixinfo.data + dsize, 0, rsize - dsize);

    /* build the pattern */
    code = pcl_pattern_build_pattern( &(pptrn),
                                      &pixinfo,
                                      (format == 1 ? pcl_pattern_colored 
                                                   : pcl_pattern_uncolored),
                                      xres,
                                      yres,
                                      pcs->memory
                                      );

    /* place the pattern into the pattern dictionary */
    if ( (code < 0)                                            ||
         ((code = define_pcl_ptrn(pcs, pcs->pattern_id, pptrn, false)) < 0)  ) {
        if (pptrn != 0)
            pcl_pattern_free_pattern(pcs->memory, pptrn, "download PCL pattern");
        else
            gs_free_object( pcs->memory,
                            (void *)pixinfo.data,
                            "download PCL pattern"
                            );
    }

    return code;
}

/*
 * ESC * c # Q
 *
 * Pattern contorl.
 */
  static int
pattern_control(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    pcl_pattern_t * pptrn = 0;

    switch (int_arg(pargs)) {

        /* delete all patterns */
      case 0:
        delete_all_pcl_ptrns(false, false, pcs);
        break;

        /* delete all temporary patterns */
      case 1:
        delete_all_pcl_ptrns(false, true, pcs);
        break;

        /* delete last specified pattern */
      case 2:
        define_pcl_ptrn(pcs, pcs->pattern_id, NULL, false);

        /* make last specified pattern temporary */
      case 4:
        pptrn = pcl_pattern_get_pcl_uptrn(pcs, pcs->pattern_id);
        if (pptrn != 0)
            pptrn->ppat_data->storage = pcds_temporary;
        break;

        /* make last specified pattern permanent */
      case 5:
        pptrn = pcl_pattern_get_pcl_uptrn(pcs, pcs->pattern_id);
        if (pptrn != 0)
            pptrn->ppat_data->storage = pcds_permanent;
        break;

      default:
        break;
    }

    return 0;
}

static int
upattern_do_copy(pcl_state_t *psaved, const pcl_state_t *pcs,
  pcl_copy_operation_t operation)
{	
    int i;
    /* copy back any patterns created during macro invocation. */
    if ((operation & pcl_copy_after) != 0) {
        for(i = 0; i < countof(pcs->bi_pattern_array); i++)
            psaved->bi_pattern_array[i] = pcs->bi_pattern_array[i];
        psaved->gl_patterns = pcs->gl_patterns;
        psaved->pcl_patterns = pcs->pcl_patterns;
        psaved->last_pcl_uptrn_id = pcs->last_pcl_uptrn_id;
        psaved->plast_pcl_uptrn = pcs->plast_pcl_uptrn;
        psaved->last_gl2_RF_indx = pcs->last_gl2_RF_indx;
        psaved->plast_gl2_uptrn = pcs->plast_gl2_uptrn;
        psaved->punsolid_pattern = pcs->punsolid_pattern;
        psaved->psolid_pattern = pcs->psolid_pattern;
    }
    return 0;
}

/*
 * Initialization and reset routines.
 */ 
  static int
upattern_do_registration(
    pcl_parser_state_t *pcl_parser_state,
    gs_memory_t *   pmem
)
{
    DEFINE_CLASS('*')
    {
        'c', 'W',
        PCL_COMMAND("Download Pattern", download_pcl_pattern, pca_bytes)
    },
    {
        'c', 'Q',
        PCL_COMMAND( "Pattern Control", 
                     pattern_control,
                     pca_neg_ignore | pca_big_ignore
                     )
    },
    END_CLASS
    return 0;
}

  static void
upattern_do_reset(
    pcl_state_t *                   pcs,
    pcl_reset_type_t                type
)
{
    if ((type & pcl_reset_initial) != 0) {
        pl_dict_init(&pcs->pcl_patterns, pcs->memory, pcl_pattern_free_pattern);
        pl_dict_init(&pcs->gl_patterns, pcs->memory, pcl_pattern_free_pattern);
        pcs->last_pcl_uptrn_id = -1;
        pcs->plast_pcl_uptrn = 0;
        pcs->last_gl2_RF_indx = -1;
        pcs->plast_gl2_uptrn = 0;

    } else if ((type & (pcl_reset_cold | pcl_reset_printer | pcl_reset_permanent)) != 0) {
        delete_all_pcl_ptrns(true, !(type & pcl_reset_permanent) , pcs);
        pcl_pattern_clear_bi_patterns(pcs);
        /* GL's IN command takes care of the GL patterns */
    }
}

const pcl_init_t    pcl_upattern_init = { upattern_do_registration, upattern_do_reset, upattern_do_copy };
