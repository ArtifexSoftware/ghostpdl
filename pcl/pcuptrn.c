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

/* pcuptrn.c - code for PCL and GL/2 user defined patterns */

#include "gx.h"
#include "pldict.h"
#include "pcindexed.h"
#include "pcpatrn.h"
#include "pcuptrn.h"

/* dictionaries to hold patterns */
private pl_dict_t   pcl_patterns;
private pl_dict_t   gl_patterns;

/*
 * Optimization - record the last used pattern_id and pattern. This
 * information must be global to this file because redefinition/remvoal
 * of a pattern must cause this to be reset.
 *
 * Separate versions of this parameters are maintained for PCL and GL/2
 * user defined patterns.
 */
private int              last_pcl_uptrn_id;
private pcl_pattern_t *  plast_pcl_uptrn;
private int              last_gl2_RF_indx;
private pcl_pattern_t *  plast_gl2_uptrn;

/*
 * GC routines.
 */
  private
ENUM_PTRS_BEGIN(pattern_enum_ptrs)
        return 0;
    ENUM_PTR(0, pcl_pattern_t, pixinfo.data);
    ENUM_STRING_PTR(1, pcl_pattern_t, palette);
    ENUM_PTR(2, pcl_pattern_t, pcspace);
    ENUM_PTR(3, pcl_pattern_t, prast);
    ENUM_PTR(4, pcl_pattern_t, ccolor.pattern);
ENUM_PTRS_END

  private
RELOC_PTRS_BEGIN(pattern_reloc_ptrs)
    RELOC_PTR(pcl_pattern_t, pixinfo.data);
    RELOC_STRING_PTR(pcl_pattern_t, palette);
    RELOC_PTR(pcl_pattern_t, pcspace);
    RELOC_PTR(pcl_pattern_t, prast);
    RELOC_PTR(pcl_pattern_t, ccolor.pattern);
RELOC_PTRS_END

private_st_pattern_t();

/*
 * Free routine for patterns.
 */
  private void
free_pattern(
    gs_memory_t *   pmem,
    void *          pvptrn,
    client_name_t   cname
)
{
    pcl_pattern_t * pptrn = (pcl_pattern_t *)pvptrn;

    if (pptrn->pixinfo.data != 0)
        gs_free_object(pmem, pptrn->pixinfo.data, cname);
    if (pptrn->palette.data != 0)
        gs_free_string(pmem, pptrn->palette.data, pptrn->palette.size, cname);
    if (pptrn->pcspace != 0) {
        gs_cspace_release(pptrn->pcspace);
        gs_free_object(pmem, (void *)pptrn->pcspace, cname);
    }
    if (pptrn->prast != 0)
        gs_free_object(pmem, (void *)pptrn->prast, cname);
    if (pptrn->ccolor.pattern != 0)
        gs_free_object(pmem, pptrn->ccolor.pattern, cname);
    gs_free_object(pmem, pvptrn, cname);
}

/*
 * Get a PCL user-defined pattern. A null return indicates the pattern is
 * not defined.
 */
  pcl_pattern_t *
pcl_pattern_get_pcl_uptrn(
    int             id
)
{
    if (last_pcl_uptrn_id != id) {
        pcl_id_t        key;

        last_pcl_uptrn_id = id;
        id_set_value(key, id);
        if ( !pl_dict_lookup( &pcl_patterns,
                              id_key(key),
                              2,
                              (void **)&plast_pcl_uptrn,
                              false,
                              NULL
                              ) )
            plast_pcl_uptrn = 0;
    }

    return plast_pcl_uptrn;
}

/*
 * Define a PCL user-defined pattern. This procedure updates the cached
 * information, hence it should be used for all definitions. To undefine
 * an entry, set the second operard to null.
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
  private int
define_pcl_ptrn(
    int             id,
    pcl_pattern_t * pptrn
)
{
    pcl_id_t        key;

    id_set_value(key, id);
    if (pptrn == 0)
        pl_dict_undef(&pcl_patterns, id_key(key), 2);
    else if (pl_dict_put(&pcl_patterns, id_key(key), 2, pptrn) < 0)
        return e_Memory;

    if (last_pcl_uptrn_id == id)
        plast_pcl_uptrn = pptrn;

    return 0;
}

/*
 * Delete all temporary patterns or all patterns, based on the value of
 * the operand.
 *
 * Because the current color in the graphic state will contain an "uncounted"
 * reference to the current patter in the event that a patterned color space
 * is being used, routines that delete the current patter will always set
 * the color space to be DeviceGray before proceeding.
 */
  private void
delete_all_pcl_ptrns(
    bool            tmp_only,
    pcl_state_t *   pcs
)
{
    pcl_pattern_t * pptrn;
    pl_dict_enum_t  denum;
    gs_const_string plkey;

    (pcl_pattern_get_proc_PCL(pcl_pattern_solid_white))(pcs, 0, 0);
    pl_dict_enum_begin(&pcl_patterns, &denum);
    while (pl_dict_enum_next(&denum, &plkey, (void **)&pptrn)) {
        if (!tmp_only || (pptrn->storage == pcds_temporary)) {
            pcl_id_t    key;

            id_set_key(key, plkey.data);
            define_pcl_ptrn(id_value(key), NULL);
        }
    }
}

/*
 * Get a GL/2 user defined pattern. A null return indicates there is no pattern
 * defined for the index.
 */
  pcl_pattern_t *
pcl_pattern_get_gl_uptrn(
    int             indx
)
{
    if (last_gl2_RF_indx != indx) {
        pcl_id_t        key;

        last_gl2_RF_indx = indx;
        id_set_value(key, indx);
        if ( !pl_dict_lookup( &gl_patterns,
                              id_key(key),
                              2,
                              (void **)(&plast_gl2_uptrn),
                              false,
                              NULL
                              ) )
            plast_gl2_uptrn = 0;
    }

    return plast_gl2_uptrn;
}

/*
 * Allocate a pattern object.
 *
 * Returns 0 on success, < 0 in the event of an error.
 */
  private int
allocate_pattern(
    pcl_pattern_t **    ppptrn,
    gs_memory_t *       pmem
)
{
    pcl_pattern_t *     pptrn;

    pptrn = gs_alloc_struct( pmem,
                             pcl_pattern_t,
                             &st_pattern_t,
                             "create pattern"
                             );
    if (pptrn == 0)
        return e_Memory;

    pptrn->pixinfo.data = 0;
    pptrn->pixinfo.raster = 0;
    pptrn->pixinfo.size.x = 0;
    pptrn->pixinfo.size.y = 0;
    pptrn->pixinfo.id = 0;              /* ignored */
    pptrn->pixinfo.pix_depth = 0;
    pptrn->pixinfo.num_comps = 1;
    pptrn->storage = pcds_temporary;
    pptrn->type = pcl_pattern_colored;  /* place holder */
    pptrn->xres = 300;
    pptrn->yres = 300;
    pptrn->orient = -1;
    pptrn->pen_num = -1;
    pptrn->ref_pt.x = 0.0;              /* place holder */
    pptrn->ref_pt.y = 0.0;              /* place holder */
    pptrn->cache_id = 0L;
    pptrn->palette.data = 0;
    pptrn->palette.size = 0;
    pptrn->pcspace = 0;
    pptrn->prast = 0;
    pptrn->ccolor_id = 0L;
    pptrn->ccolor.pattern = 0;

    *ppptrn = pptrn;
    return 0;
}

/*
 * Create and define a GL/2 user-define pattern. This is the only pattern-
 * control like facility provided for GL/2. To undefine patterns, use null
 * as the second operand. See pcpatrn.h for further information.
 *
 * Because the current color in the graphic state will contain an "uncounted"
 * reference to the current patter in the event that a patterned color space
 * is being used, routines that delete the current patter will always set
 * the color space to be DeviceGray before proceeding.
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
    pcl_pattern_t *         pptrn;

    id_set_value(key, indx);

    (pcl_pattern_get_proc_PCL(pcl_pattern_solid_white))(pcs, 0, 0);
    if (ppixmap != 0) {
        int     code = allocate_pattern(&pptrn, pcs->memory);

        if (code < 0)
            return code;

        pptrn->pixinfo = *ppixmap;
        pptrn->type = pcl_pattern_colored;
        if (pl_dict_put(&gl_patterns, id_key(key), 2, pptrn) < 0) {
            gs_free_object(pcs->memory, pptrn, "create GL/2 RF pattern");
            return e_Memory;
        }

    } else {
        pptrn = 0;
        pl_dict_undef(&gl_patterns, id_key(key), 2);
    }
    
    if (last_gl2_RF_indx == indx)
        plast_gl2_uptrn = pptrn;

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
  private int
download_pcl_pattern(
    pcl_args_t *            pargs,
    pcl_state_t *           pcs
)
{
    uint                    count = arg_data_size(pargs);
    const pcl_upattern0_t * puptrn0 = (pcl_upattern0_t *)arg_data(pargs);
    uint                    format, height, width, depth, byte_wid, rsize;
    byte *                  pb = 0;
    pcl_pattern_t *         pptrn = 0;
    int                     code = 0;

    if (count < 8)
	return e_Range;

    format = puptrn0->format;
    depth = puptrn0->depth & 0xf;
    height = (((uint)puptrn0->height[0]) << 8) + puptrn0->height[1];
    width = (((uint)puptrn0->width[0]) << 8) + puptrn0->width[1];

    if (format == 0) {
        if (depth != 1)
            return e_Range;
    } else if ( (format != 1) || ((depth != 1) && (depth != 8)) )
        return e_Range;

    byte_wid = (width * depth + 7) / 8;
    rsize = byte_wid * height;

    /* note: HPS allows count < rsize + 8 */

    pb = gs_alloc_bytes(pcs->memory, byte_wid * height, "download pcl pattern");
    if (pb == 0)
        return e_Memory;
    if ((code = allocate_pattern(&pptrn, pcs->memory)) < 0) {
        gs_free_object(pcs->memory, pb, "download pcl pattern");
        return code;
    }

    pptrn->pixinfo.data = pb;
    pptrn->pixinfo.raster = byte_wid;
    pptrn->pixinfo.size.x = width;
    pptrn->pixinfo.size.y = height;
    pptrn->pixinfo.pix_depth = depth;

    pptrn->type = (format == 1 ? pcl_pattern_colored : pcl_pattern_uncolored);

    if (count >= rsize + 12) {
        pcl_upattern1_t *   puptrn1 = (pcl_upattern1_t *)puptrn0;

        pptrn->xres = (((uint)puptrn1->xres[0]) << 8) + puptrn1->xres[1];
        pptrn->yres = (((uint)puptrn1->yres[0]) << 8) + puptrn1->yres[1];
        memcpy(pb, puptrn1->data, rsize);
    } else {
        uint    tmp_cnt = min(count - 8, rsize);

        memcpy(pb, puptrn0->data, tmp_cnt);
        if (tmp_cnt < rsize)
            memset(pb, 0, rsize - tmp_cnt);
    }

    /* this may release a pattern: clear current color in gstate */
    (pcl_pattern_get_proc_PCL(pcl_pattern_solid_white))(pcs, 0, 0);
    return define_pcl_ptrn(pcs->pattern_id, pptrn);
}

/*
 * ESC * c # Q
 *
 * Pattern contorl.
 */
  private int
pattern_control(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    pcl_pattern_t * pptrn = 0;

    switch (int_arg(pargs)) {

        /* delete all patterns */
      case 0:
        delete_all_pcl_ptrns(false, pcs);
        break;

        /* delete all temporary patterns */
      case 1:
        delete_all_pcl_ptrns(true, pcs);
        break;

        /* delete last specified pattern */
      case 2:
        /* this may release a pattern: clear current color in gstate */
        (pcl_pattern_get_proc_PCL(pcl_pattern_solid_white))(pcs, 0, 0);
        define_pcl_ptrn(pcs->pattern_id, NULL);

        /* make last specified pattern temporary */
      case 4:
        pptrn = pcl_pattern_get_pcl_uptrn(pcs->pattern_id);
        if (pptrn != 0)
            pptrn->storage = pcds_temporary;
        break;

        /* make last specified pattern permanent */
      case 5:
        pptrn = pcl_pattern_get_pcl_uptrn(pcs->pattern_id);
        if (pptrn != 0)
            pptrn->storage = pcds_permanent;
        break;

      default:
        break;
    }

    return 0;
}

/*
 * Initialization and reset routines.
 */ 
  private int
upattern_do_init(
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

  private void
upattern_do_reset(
    pcl_state_t *                   pcs,
    pcl_reset_type_t                type
)
{
    if ((type & pcl_reset_initial) != 0) {
        pl_dict_init(&pcl_patterns, pcs->memory, free_pattern);
        pl_dict_init(&gl_patterns, pcs->memory, free_pattern);
        last_pcl_uptrn_id = -1;
        plast_pcl_uptrn = 0;
        last_gl2_RF_indx = -1;
        plast_gl2_uptrn = 0;
    }
    else if ((type & (pcl_reset_cold | pcl_reset_printer)) != 0)
        delete_all_pcl_ptrns(true, pcs);
        /* GL's IN command takes care of the GL patterns */
}

const pcl_init_t    pcl_upattern_init = { upattern_do_init, upattern_do_reset };
