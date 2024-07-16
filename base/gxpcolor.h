/* Copyright (C) 2001-2024 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  39 Mesa Street, Suite 108A, San Francisco,
   CA 94129, USA, for further information.
*/


/* Pattern color and tile structures and procedures */
/* Requires gsmatrix.h, gxcolor2.h, gxdcolor.h */

#ifndef gxpcolor_INCLUDED
#  define gxpcolor_INCLUDED

#include "gspcolor.h"
#include "gxcspace.h"
#include "gxdevice.h"
#include "gxdevmem.h"
#include "gxpcache.h"
#include "gxblend.h"
#include "gxcpath.h"
#include "gxdcolor.h"
#include "gxiclass.h"

#define RAW_PATTERN_DUMP 0

typedef union gx_device_clist_s gx_device_clist;

struct gs_pattern_type_s {
    int PatternType;
    struct pp_ {

        /*
         * Define whether a Pattern uses the base color space in its color
         * space, requiring setcolor to provide values for the base color
         * space.  Currently this is true for uncolored PatternType 1
         * patterns, false for all others.
         */

#define pattern_proc_uses_base_space(proc)\
  bool proc(const gs_pattern_template_t *)

        pattern_proc_uses_base_space((*uses_base_space));

        /*
         * Make an instance of a Pattern.
         */

#define pattern_proc_make_pattern(proc)\
  int proc(gs_client_color *, const gs_pattern_template_t *,\
           const gs_matrix *, gs_gstate *, gs_memory_t *)

        pattern_proc_make_pattern((*make_pattern));

        /*
         * Get the template from a Pattern instance.
         */

#define pattern_proc_get_pattern(proc)\
  const gs_pattern_template_t *proc(const gs_pattern_instance_t *)

        pattern_proc_get_pattern((*get_pattern));

        /*
         * Remap a Pattern color to a device color.
         * cs_proc_remap_color is defined in gxcspace.h.
         */

#define pattern_proc_remap_color(proc)\
  cs_proc_remap_color(proc)

        pattern_proc_remap_color((*remap_color));

        /*
         * Perform any special actions required when a pattern instance
         * is made the current color "color" (i.e.: at setcolor time).
         * This is primarily useful for PatternType2 patterns, where the
         * pattern instance specifies a color space.
         */
#define pattern_proc_set_color(proc)\
  int proc(const gs_client_color *, gs_gstate *)

        pattern_proc_set_color((*set_color));

    } procs;
};

/*
 * Initialize the common part of a pattern template.  This procedure is for
 * the use of gs_pattern*_init implementations, not clients.
 */
void gs_pattern_common_init(gs_pattern_template_t *,
                            const gs_pattern_type_t *);

/*
 * Do the generic work for makepattern: allocate the instance and the
 * saved graphics state, and fill in the common members.
 */
int gs_make_pattern_common(gs_client_color *, const gs_pattern_template_t *,
                           const gs_matrix *, gs_gstate *, gs_memory_t *,
                           gs_memory_type_ptr_t);

/* Declare the freeing procedure for Pattern instances. */
extern rc_free_proc(rc_free_pattern_instance);

/* Declare the Pattern color space type. */
extern const gs_color_space_type gs_color_space_type_Pattern;

/*
 * Define the (PatternType 1) Pattern device color types.  There is one type
 * for colored patterns, and one uncolored pattern type for each non-Pattern
 * device color type.
 */
extern const gx_device_color_type_t
    gx_dc_pattern, gx_dc_pure_masked, gx_dc_binary_masked,
    gx_dc_colored_masked, gx_dc_devn_masked;

#ifndef gx_dc_type_pattern
#define gx_dc_type_pattern (&gx_dc_pattern)
#endif

/*
 * These device color methods are shared amongst pattern types.
 */
extern dev_color_proc_save_dc(gx_dc_pattern_save_dc);
extern dev_color_proc_write(gx_dc_pattern_write);
extern dev_color_proc_read(gx_dc_pattern_read);

/*
 * For shading and colored tiling patterns, it is not possible to say
 * which color components have non-zero values.
 */
extern dev_color_proc_get_nonzero_comps(gx_dc_pattern_get_nonzero_comps);

/* Used for keeping a buffer of the pattern that containes an alpha channel
  in a planar form.  Since we must be going from the pdf14 compositor through
  the pattern accumulator and then back to a pdf14 compositor it makes sense
  to keep the data in planar form for the pdf14 compositor action */

typedef struct gx_pattern_trans_s gx_pattern_trans_t;

struct gx_pattern_trans_s {

    const gx_device *pdev14;
    byte *transbytes;
    gs_memory_t *mem;
    gx_pattern_trans_t *fill_trans_buffer;  /* buffer to fill */
    gs_int_rect rect;
    int rowstride;
    int planestride;
    int n_chan; /* number of pixel planes including alpha */
    bool has_shape;  /* extra plane inserted */
    bool has_tags;	/* and yet another plane for the tag */
    int width; /* Complete plane width/height; rect may be a subset of this */
    int height;
    const pdf14_nonseparable_blending_procs_t *blending_procs;
    bool is_additive;
    bool deep;
    void *buf;
    void (* pat_trans_fill)(int xmin, int ymin, int xmax, int ymax, int px,
                            int py, const gx_color_tile *ptile,
                            gx_pattern_trans_t *fill_trans_buffer,
                            int native16);
    int (* image_render)(gx_image_enum * penum, const byte * buffer,
                            int data_x, uint w, int h, gx_device * dev);
};

#define private_st_pattern_trans() /* in gxpcmap.c */\
gs_private_st_ptrs4(st_pattern_trans, gx_pattern_trans_t, "gx_pattern_trans",\
                    pattern_trans_enum_ptrs, pattern_trans_reloc_ptrs,\
                    pdev14, transbytes, fill_trans_buffer, buf)
/*
 * Define a color tile, an entry in the rendered Pattern cache (and
 * eventually in the colored halftone cache).  Note that the depth is
 * not sufficient to ensure that the rendering matches a given device;
 * however, we don't currently have an object that represents the
 * abstraction of a 'color representation'.
 */
struct gx_color_tile_s {
    /* ------ The following are the 'key' in the cache. ------ */
    /* Note that the id is a generated instance ID, */
    /* and has no relation to the template's gs_uid. */
    gx_bitmap_id id;
    int depth;
    /* We do, however, copy the template's gs_uid, */
    /* for use in selective cache purging. */
    gs_uid uid;
    /* ------ The following are the cache 'value'. ------ */
    int bits_used;              /* The number of bits this uses in the cache */
    /* Note that if tbits and tmask both have data != 0, */
    /* both must have the same rep_shift. */
/****** NON-ZERO shift VALUES ARE NOT SUPPORTED YET. ******/
    int tiling_type;		/* TilingType */
    gs_matrix step_matrix;	/* tiling space -> device space, */
    /* see gxcolor2.h for details */
    gs_rect bbox;		/* bbox of tile in tiling space */
    gx_strip_bitmap tbits;	/* data = 0 if uncolored */
    gx_strip_bitmap tmask;	/* data = 0 if no mask */
    /* (i.e., the mask is all 1's) */

    gx_pattern_trans_t *ttrans;  /* !=0 if has trans. in this case tbits == 0 */
    gs_blend_mode_t blending_mode;  /* used if the pattern has transparency */

    gx_device_clist *cdev;	/* not NULL if the graphics is a command list. */
    byte is_simple;		/* true if xstep/ystep = tile size */
    byte has_overlap;           /* true if step size is smaller than bounding box */
    byte is_dummy;		/* if true, the device manages the pattern,
                                   and the content of the tile is empty. */
    byte trans_group_popped;    /* Used to avoid multiple group pops in image mask fills */
    byte num_planar_planes;     /* Has to be stored here due to the device
                                   change that can occur when the tile is
                                   created and when it is written in the clist
                                   when we are writing to a transparency
                                   device which, is not planar but the target
                                   is */
    byte is_locked;		/* stroke patterns cannot be freed during fill_stroke_path */
    byte pad[2];		/* structure members alignment. */
    /* The following is neither key nor value. */
    uint index;			/* the index of the tile within the cache (for GC) */
};

#define private_st_color_tile()	/* in gxpcmap.c */\
  gs_private_st_ptrs4(st_color_tile, gx_color_tile, "gx_color_tile",\
    color_tile_enum_ptrs, color_tile_reloc_ptrs, tbits.data, tmask.data, cdev,\
    ttrans)

#define private_st_color_tile_element()	/* in gxpcmap.c */\
  gs_private_st_element(st_color_tile_element, gx_color_tile,\
    "gx_color_tile[]", color_tile_elt_enum_ptrs, color_tile_elt_reloc_ptrs,\
    st_color_tile)

/* Define the Pattern cache. */
/*#include "gxpcache.h" *//* (above) */

/* Allocate a Pattern cache. */
/* We shorten the procedure names because some VMS compilers */
/* truncate names to 23 characters. */
uint gx_pat_cache_default_tiles(void);
ulong gx_pat_cache_default_bits(void);
gx_pattern_cache *gx_pattern_alloc_cache(gs_memory_t *, uint, ulong);
/* Free pattern cache and its components. */
void gx_pattern_cache_free(gx_pattern_cache *pcache);

/* Get or set the Pattern cache in a gstate. */
gx_pattern_cache *gstate_pattern_cache(gs_gstate *);
void gstate_set_pattern_cache(gs_gstate *, gx_pattern_cache *);

/*
 * Define a device for accumulating the rendering of a Pattern.
 * This is actually a wrapper for two other devices: one that accumulates
 * the actual pattern image (if this is a colored pattern), and one that
 * accumulates a mask defining which pixels in the image are set.
 */
typedef struct gx_device_pattern_accum_s {
    gx_device_forward_common;
    /* Client sets these before opening */
    gs_memory_t *bitmap_memory;
    const gs_pattern1_instance_t *instance;
    /* open sets these */
    gx_device_memory *bits;	/* target also points to bits */
    gx_device_memory *mask;

    /* If we have transparency, then we will just
    use the PDF14 buffer directly instead
    of the memory device.  That is
    what this pointer is for */

    gx_pattern_trans_t *transbuff;

} gx_device_pattern_accum;

#define private_st_device_pattern_accum() /* in gxpcmap.c */\
  gs_private_st_suffix_add4_final(st_device_pattern_accum,\
    gx_device_pattern_accum, "pattern accumulator", pattern_accum_enum,\
    pattern_accum_reloc, gx_device_finalize, st_device_forward,\
    instance, bits, mask, transbuff)

/* Allocate a pattern accumulator. */
gx_device_forward * gx_pattern_accum_alloc(gs_memory_t * mem,
                       gs_memory_t * stoarge_memory,
                       gs_pattern1_instance_t *pinst, client_name_t cname);

/* Return true if pattern accumulator device (not pattern-clist) */
bool gx_device_is_pattern_accum(gx_device *dev);

/* Given the size of a new pattern tile, free entries from the cache until  */
/* enough space is available (or nothing left to free).			    */
/* This will allow 1 oversized entry					    */
void gx_pattern_cache_ensure_space(gs_gstate * pgs, size_t needed);

gx_color_tile *
gx_pattern_cache_find_tile_for_id(gx_pattern_cache *pcache, gs_id id);

void gx_pattern_cache_update_used(gs_gstate *pgs, size_t used);

/* Update cache tile space */
void gx_pattern_cache_update_space(gs_gstate * pgs, size_t used);

/* Add an accumulated pattern to the cache. */
/* Note that this does not free any of the data in the accumulator */
/* device, but it may zero out the bitmap_memory pointers to prevent */
/* the accumulated bitmaps from being freed when the device is closed. */
int gx_pattern_cache_add_entry(gs_gstate *, gx_device_forward *,
                               gx_color_tile **);
/* Add a dummy Pattern cache entry.  Stubs a pattern tile for interpreter when
   device handles high level patterns. */
int gx_pattern_cache_add_dummy_entry(gs_gstate *pgs, gs_pattern1_instance_t *pinst,
                                int depth);

/* set or clear the lock for a tile in the cache. Returns error if tile not in cache */
int gx_pattern_cache_entry_set_lock(gs_gstate * pgs, gs_id id, bool new_lock_value);

/* Get entry for reading a pattern from clist. */
int gx_pattern_cache_get_entry(gs_gstate * pgs, gs_id id, gx_color_tile ** pctile);

/* Look up a pattern color in the cache. */
bool gx_pattern_cache_lookup(gx_device_color *, const gs_gstate *,
                             gx_device *, gs_color_select_t);

/* Purge selected entries from the pattern cache. */
void gx_pattern_cache_winnow(gx_pattern_cache *,
                             bool (*)(gx_color_tile *, void *),
                             void *);

void gx_pattern_cache_flush(gx_pattern_cache * pcache);

bool gx_pattern_tile_is_clist(gx_color_tile *ptile);

/* Return true if pattern-clist device (not pattern accumulator) */
bool gx_device_is_pattern_clist(gx_device *dev);

dev_proc_open_device(pattern_clist_open_device);
dev_proc_dev_spec_op(pattern_accum_dev_spec_op);

/* Code to fill a pdf14 transparency rectangles with a pattern trans buffer object */
int gx_trans_pattern_fill_rect(int xmin, int ymin, int xmax, int ymax,
                               gx_color_tile *ptile,
                               gx_pattern_trans_t *fill_trans_buffer,
                               gs_int_point phase, gx_device *dev,
                               const gx_device_color * pdevc,
                               int native16);

gx_pattern_trans_t* new_pattern_trans_buff(gs_memory_t *mem);

void tile_rect_trans_simple(int xmin, int ymin, int xmax, int ymax, int px,
                            int py, const gx_color_tile *ptile,
                            gx_pattern_trans_t *fill_trans_buffer,
                            int native16);

/* This is used for the case when we may have overlapping tiles.
   We need to get better detection for this as
   it would be best to avoid doing it if not needed. */
void tile_rect_trans_blend(int xmin, int ymin, int xmax, int ymax, int px,
                           int py, const gx_color_tile *ptile,
                           gx_pattern_trans_t *fill_trans_buffer,
                           int native16);

/* File a colored pattern with white */
int gx_erase_colored_pattern(gs_gstate *pgs);

#endif /* gxpcolor_INCLUDED */
