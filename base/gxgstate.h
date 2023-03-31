/* Copyright (C) 2001-2023 Artifex Software, Inc.
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


/* graphics state definition */

#ifndef gxistate_INCLUDED
#  define gxistate_INCLUDED

#include "gscsel.h"
#include "gsrefct.h"
#include "gsropt.h"
#include "gstparam.h"
#include "gxcvalue.h"
#include "gxcmap.h"
#include "gxfixed.h"
#include "gxline.h"
#include "gxmatrix.h"
#include "gxtmap.h"
#include "gscspace.h"
#include "gstrans.h"
#include "gsnamecl.h"
#include "gscms.h"
#include "gscpm.h"
#include "gscspace.h"
#include "gxdcolor.h"
#include "gxstate.h"
#include "gsfont.h"
#include "gxpath.h"
#include "gsccolor.h"
#include "gsht1.h"
#include "gxclipsr.h"
#include "gsicc_blacktext.h"


/*
 * Define the color rendering state information.
 * This should be a separate object (or at least a substructure),
 * but making this change would require editing too much code.
 */

/*
 * We need some special memory management for the components of a
 * c.r. state, as indicated by the following notations on the elements:
 *      (RC) means the element is reference-counted.
 *      (Shared) means the element is shared among an arbitrary number of
 *        c.r. states and is never freed.
 *      (Owned) means exactly one c.r. state references the element,
 *        and it is guaranteed that no references to it will outlive
 *        the c.r. state itself.
 */

/* Define the interior structure of a transfer function. */
typedef struct gx_transfer_s {
    int red_component_num;
    gx_transfer_map *red;		/* (RC) */
    int green_component_num;
    gx_transfer_map *green;		/* (RC) */
    int blue_component_num;
    gx_transfer_map *blue;		/* (RC) */
    int gray_component_num;
    gx_transfer_map *gray;		/* (RC) */
} gx_transfer;

#define gs_color_rendering_state_common\
\
                /* Halftone screen: */\
\
        gs_halftone *halftone;			/* (RC) */\
        gs_int_point screen_phase[gs_color_select_count];\
                /* dev_ht depends on halftone and device resolution. */\
        gx_device_halftone *dev_ht[HT_OBJTYPE_COUNT];		/* (RC) */\
\
                /* Color (device-dependent): */\
\
        struct gs_cie_render_s *cie_render;	/* (RC) may be 0 */\
        bool cie_to_xyz;			/* flag for conversion to XYZ, no CRD req'd */\
        gx_transfer_map *black_generation;	/* (RC) may be 0 */\
        gx_transfer_map *undercolor_removal;	/* (RC) may be 0 */\
                /* set_transfer holds the transfer functions specified by */\
                /* set[color]transfer; effective_transfer includes the */\
                /* effects of overrides by TransferFunctions in halftone */\
                /* dictionaries.  (In Level 1 systems, set_transfer and */\
                /* effective_transfer are always the same.) */\
        gx_transfer set_transfer;		/* members are (RC) */\
        int effective_transfer_non_identity_count;\
        gx_transfer_map *effective_transfer[GX_DEVICE_COLOR_MAX_COMPONENTS]; /* see below */\
\
                /* Color caches: */\
\
                /* cie_joint_caches depend on cie_render and */\
                /* the color space. */\
        struct gx_cie_joint_caches_s *cie_joint_caches;		/* (RC) */\
                /* cmap_procs depend on the device's color_info. */\
        const struct gx_color_map_procs_s *cmap_procs;		/* static */\
                /* DeviceN component map for current color space */\
        gs_devicen_color_map color_component_map;\
                /* The contents of pattern_cache depend on the */\
                /* the color space and the device's color_info and */\
                /* resolution. */\
        struct gx_pattern_cache_s *pattern_cache;	/* (Shared) by all gstates */\
\
        /* Simple color spaces, stored here for easy access from */ 	\
        /* gx_concrete_space_CIE */ \
        gs_color_space *devicergb_cs;\
        gs_color_space *devicecmyk_cs;\
\
        /* Stores for cached values which correspond to whichever */\
        /* color isn't in force at the moment */\
        struct gx_cie_joint_caches_s *cie_joint_caches_alt;\
        gs_devicen_color_map          color_component_map_alt


/* Current colors (non-stroking, and stroking) */
typedef struct gs_gstate_color_s {
    gs_color_space *color_space; /* after substitution */
    gs_client_color *ccolor;
    gx_device_color *dev_color;
    bool effective_opm;
} gs_gstate_color;

/*
 * Enumerate the reference-counted pointers in a c.r. state.  Note that
 * effective_transfer doesn't contribute to the reference count: it points
 * either to the same objects as set_transfer, or to objects in a halftone
 * structure that someone else worries about.
 */
#define gs_cr_state_do_rc_ptrs(m)\
  m(halftone) \
  m(dev_ht[HT_OBJTYPE_DEFAULT]) \
  m(dev_ht[HT_OBJTYPE_VECTOR]) \
  m(dev_ht[HT_OBJTYPE_IMAGE]) \
  m(dev_ht[HT_OBJTYPE_TEXT]) \
  m(cie_render) \
  m(black_generation) \
  m(undercolor_removal) \
  m(set_transfer.red) \
  m(set_transfer.green) \
  m(set_transfer.blue) \
  m(set_transfer.gray) \
  m(cie_joint_caches) \
  m(devicergb_cs) \
  m(devicecmyk_cs)\
  m(cie_joint_caches_alt)

/* Enumerate the pointers in a c.r. state. */
#define gs_cr_state_do_ptrs(m)\
  m(0,halftone) \
  m(1,dev_ht[HT_OBJTYPE_DEFAULT]) \
  m(2, dev_ht[HT_OBJTYPE_VECTOR]) \
  m(3, dev_ht[HT_OBJTYPE_IMAGE]) \
  m(4, dev_ht[HT_OBJTYPE_TEXT]) \
  m(5,cie_render) \
  m(6,black_generation) \
  m(7,undercolor_removal) \
  m(8,set_transfer.red) \
  m(9,set_transfer.green) \
  m(10,set_transfer.blue) \
  m(11,set_transfer.gray)\
  m(12,cie_joint_caches) \
  m(13,pattern_cache) \
  m(14,devicergb_cs) \
  m(15,devicecmyk_cs)\
  m(16,cie_joint_caches_alt)
  /*
   * We handle effective_transfer specially in gsistate.c since its pointers
   * are not enumerated for garbage collection but they are are relocated.
   */
/*
 * This count does not include the effective_transfer pointers since they
 * are not enumerated for GC.
 */
#define st_cr_state_num_ptrs 17

struct gs_devicen_color_map_s {
    bool use_alt_cspace;
    separation_type sep_type;
    uint num_components;	/* Input - Duplicate of value in gs_device_n_params */
    uint num_colorants;		/* Number of colorants - output */
    gs_id cspace_id;		/* Used to verify color space and map match */
    int color_map[GS_CLIENT_COLOR_MAX_COMPONENTS];
};


/* These flags are used to keep track of qQ
   combinations surrounding a graphic state
   change that includes a softmask setting.
   The transparency compositor must be notified
   when a Q event occurs following a softmask */

typedef struct gs_xstate_trans_flags {
    bool xstate_pending;
    bool xstate_change;
} gs_xstate_trans_flags_t;

#define gs_currentdevicecolor_inline(pgs) ((pgs)->color[0].dev_color)
#define gs_currentcolor_inline(pgs)       ((pgs)->color[0].ccolor)
#define gs_currentcolorspace_inline(pgs)  ((pgs)->color[0].color_space)
#define gs_swappeddevicecolor_inline(pgs) ((pgs)->color[1].dev_color)
#define gs_swappedcolor_inline(pgs)       ((pgs)->color[1].ccolor)
#define gs_swappedcolorspace_inline(pgs)  ((pgs)->color[1].color_space)
#define gs_currentcolor_eopm(pgs)         ((pgs)->color[0].effective_opm)

#define char_tm_only(pgs) *(gs_matrix *)&(pgs)->char_tm

#undef gs_currentdevice_inline /* remove definition in gsdevice.h?? */
#define gs_currentdevice_inline(pgs) ((pgs)->device)

#define gs_gstate_client_data(pgs) ((pgs)->client_data)

/* Define the graphics state structure itself. */
/*
 * Note that the ctm member is a gs_matrix_fixed.  As such, it cannot be
 * used directly as the argument for procedures like gs_point_transform.
 * Instead, one must use the ctm_only macro, e.g., &ctm_only(pgs) rather
 * than &pgs->ctm.
 */

/* Access macros */
#define ctm_only(pgs) (*(const gs_matrix *)&(pgs)->ctm)
#define ctm_only_writable(pgs) (*(gs_matrix *)&(pgs)->ctm)
#define set_ctm_only(pgs, mat) (*(gs_matrix *)&(pgs)->ctm = (mat))
#define gs_init_rop(pgs) ((pgs)->log_op = lop_default)
#define gs_currentflat_inline(pgs) ((pgs)->flatness)
#define gs_currentlineparams_inline(pgs) (&(pgs)->line_params)
#define gs_current_logical_op_inline(pgs) ((pgs)->log_op)
#define gs_set_logical_op_inline(pgs, lop) ((pgs)->log_op = (lop))

struct gs_gstate_s {
    gs_memory_t *memory;
    void *client_data;
    gx_line_params line_params;
    bool hpgl_path_mode;
    gs_matrix_fixed ctm;
    bool current_point_valid;
    gs_point current_point;
    gs_point subpath_start;
    bool clamp_coordinates;
    gs_logical_operation_t log_op;
    gs_blend_mode_t blend_mode;
    gs_xstate_trans_flags_t trans_flags;
    gs_id soft_mask_id;
    bool text_knockout;
    uint text_rendering_mode;
    bool has_transparency;   /* used to keep from doing shading fills in device color space */
    gx_device *trans_device;  /* trans device has all mappings to group color space */
    bool overprint;
    int overprint_mode;
    bool stroke_overprint;
    float flatness;
    gs_fixed_point fill_adjust; /* A path expansion for fill; -1 = dropout prevention*/
    bool stroke_adjust;
    bool accurate_curves;
    bool have_pattern_streams;
    float smoothness;
    int renderingintent; /* See gsstate.c */
    bool blackptcomp;
    gsicc_manager_t *icc_manager; /* ICC color manager, profile */
    gsicc_link_cache_t *icc_link_cache; /* ICC linked transforms */
    gsicc_profile_cache_t *icc_profile_cache;  /* ICC profiles from PS. */
    gsicc_blacktextvec_state_t *black_textvec_state;  /* Used to store and restore cs for black text */

    CUSTOM_COLOR_PTR        /* Pointer to custom color callback struct */
    const gx_color_map_procs *
      (*get_cmap_procs)(const gs_gstate *, const gx_device *);
    gs_color_rendering_state_common;

    gs_gstate *saved;	        /* previous state from gsave */

    /* Transformation: */
    gs_matrix ctm_inverse;
    bool ctm_inverse_valid;     /* true if ctm_inverse = ctm^-1 */
    gs_matrix ctm_default;
    bool ctm_default_set;       /* if true, use ctm_default; */
                                /* if false, ask device */
    gs_matrix ctm_initial;  /* The value of the device initial matrix at the time the default was set. */
    bool ctm_initial_set;   /* if true, modification set. If not, assume identity. */

    /* Paths: */

    gx_path *path;
    gx_clip_path *clip_path;
    gx_clip_stack_t *clip_stack;  /* (LanguageLevel 3 only) */
    gx_clip_path *view_clip;	  /* (may be 0, or have rule = 0) */

    /* Effective clip path cache */
    gs_id effective_clip_id;            /* (key) clip path id */
    gs_id effective_view_clip_id;	/* (key) view clip path id */
    gx_clip_path *effective_clip_path;	/* (value) effective clip path, */
                                /* possibly = clip_path or view_clip */
    bool effective_clip_shared;	/* true iff e.c.p. = c.p. or v.c. */

    /* PDF graphics state parameters */
    float strokeconstantalpha, fillconstantalpha;
                                /* *SMask is stored in int_gstate as its a ref object */
    bool alphaisshape;
    float textspacing;
    float textleading;
    float textrise;
    float wordspacing;
    float texthscaling;
    float PDFfontsize;
    gs_matrix textlinematrix;
    gs_matrix textmatrix;
    /* Current colors (non-stroking, and stroking) */
    gs_gstate_color color[2];
    int is_fill_color;
    /* Font: */
    gs_font *font;
    gs_font *root_font;
    gs_matrix_fixed char_tm;     /* font matrix * ctm */
    bool char_tm_valid;	         /* true if char_tm is valid */
    gs_in_cache_device_t in_cachedevice;    /* (see gscpm.h) */
    gs_char_path_mode in_charpath;          /* (see gscpm.h) */
    gs_gstate *show_gstate;      /* gstate when show was invoked */
                                /* (so charpath can append to path) */
    /* Other stuff: */
    int level;			/* incremented by 1 per gsave */
    gx_device *device;
    gs_gstate_client_procs client_procs;
};

/* Initialization for gs_gstate */
#define gs_gstate_initial(scale)\
  0, 0, { gx_line_params_initial }, 0,\
   { (float)(scale), 0.0, 0.0, (float)(-(scale)), 0.0, 0.0 },\
  false, {0, 0}, {0, 0}, false, \
  lop_default, BLEND_MODE_Compatible,\
  {0, 0}, 0, 1/*true*/, 0, 0/*false*/, 0, 0/*false*/, 0, 0/*false*/, 1.0,  \
   { fixed_half, fixed_half }, 0/*false*/, 1/*true*/, 0/*false*/, (float)0.02,\
  1, 1/* bpt true */, 0, 0, 0, 0, INIT_CUSTOM_COLOR_PTR	/* 'Custom color' callback pointer */  \
  gx_default_get_cmap_procs

#define GS_STATE_INIT_VALUES(s, scale) \
  do { \
    static const struct gs_gstate_s __state_init = {gs_gstate_initial(scale)}; \
    s->memory = __state_init.memory; \
    s->client_data = __state_init.client_data; \
    s->line_params = __state_init.line_params; \
    s->hpgl_path_mode = __state_init.hpgl_path_mode; \
    s->ctm = __state_init.ctm; \
    s->current_point_valid = __state_init.current_point_valid; \
    s->current_point = __state_init.current_point; \
    s->subpath_start = __state_init.subpath_start; \
    s->clamp_coordinates = __state_init.clamp_coordinates; \
    s->log_op = __state_init.log_op; \
    s->blend_mode = __state_init.blend_mode; \
    s->trans_flags = __state_init.trans_flags; \
    s->soft_mask_id = __state_init.soft_mask_id; \
    s->text_knockout = __state_init.text_knockout; \
    s->text_rendering_mode = __state_init.text_rendering_mode; \
    s->has_transparency = __state_init.has_transparency; \
    s->trans_device = __state_init.trans_device; \
    s->overprint = __state_init.overprint; \
    s->overprint_mode = __state_init.overprint_mode; \
    s->stroke_overprint = __state_init.stroke_overprint; \
    s->flatness = __state_init.flatness; \
    s->fill_adjust = __state_init.fill_adjust; \
    s->stroke_adjust = __state_init.stroke_adjust; \
    s->accurate_curves = __state_init.accurate_curves; \
    s->have_pattern_streams = __state_init.have_pattern_streams; \
    s->smoothness = __state_init.smoothness; \
    s->renderingintent = __state_init.renderingintent; \
    s->blackptcomp = __state_init.blackptcomp; \
    s->icc_manager = __state_init.icc_manager; \
    s->icc_link_cache = __state_init.icc_link_cache; \
    s->icc_profile_cache = __state_init.icc_profile_cache; \
    s->get_cmap_procs = __state_init.get_cmap_procs; \
    s->black_textvec_state = NULL; \
    s->show_gstate = NULL; \
    s->is_fill_color = 1; \
    s->strokeconstantalpha = 1.0; \
    s->fillconstantalpha = 1.0; \
    s->alphaisshape = 0; \
    s->texthscaling = 100.0; \
    s->textspacing = 0.0; \
    s->textleading = 0.0; \
    s->wordspacing = 0.0; \
    s->textrise = 0.0; \
  } while (0)

struct_proc_finalize(gs_gstate_finalize);
#define public_st_gs_gstate()	/* in gsstate.c */\
  gs_public_st_composite_use_final(st_gs_gstate, gs_gstate, "gs_gstate",\
    gs_gstate_enum_ptrs, gs_gstate_reloc_ptrs, gs_gstate_finalize)

/*
 * Enumerate the pointers in a graphics state
 * except device which must
 * be handled specially.
 */
#define gs_gstate_do_ptrs(m)\
  m(0,  client_data) \
  m(1,  trans_device) \
  m(2,  icc_manager) \
  m(3,  icc_link_cache) \
  m(4,  icc_profile_cache) \
  m(5,  saved) \
  m(6,  path) \
  m(7,  clip_path) \
  m(8,  clip_stack) \
  m(9,  view_clip) \
  m(10, effective_clip_path) \
  m(11, color[0].color_space) \
  m(12, color[0].ccolor) \
  m(13, color[0].dev_color) \
  m(14, color[1].color_space) \
  m(15, color[1].ccolor) \
  m(16, color[1].dev_color)\
  m(17, font) \
  m(18, root_font) \
  m(19, show_gstate) \
  m(20, black_textvec_state)

#define gs_gstate_num_ptrs 21

/* The '+1' in the following is because gs_gstate.device
 * is handled specially
 */
#define st_gs_gstate_num_ptrs\
  (st_line_params_num_ptrs + st_cr_state_num_ptrs + gs_gstate_num_ptrs + 1)

/* Initialize an graphics state, other than the parts covered by */
/* gs_gstate_initial. */
int gs_gstate_initialize(gs_gstate * pgs, gs_memory_t * mem);

/* Increment reference counts to note that a graphics state has been copied. */
void gs_gstate_copied(gs_gstate * pgs);

/* Adjust reference counts before assigning one gs_gstate to another. */
void gs_gstate_pre_assign(gs_gstate *to, const gs_gstate *from);

/* Release an gs_gstate. */
void gs_gstate_release(gs_gstate * pgs);
int gs_currentscreenphase_pgs(const gs_gstate *, gs_int_point *, gs_color_select_t);

/* The following macro is used for development purpose for designating places
   where current point is changed. Clients must not use it. */
#define gx_setcurrentpoint(pgs, xx, yy)\
    (pgs)->current_point.x = xx;\
    (pgs)->current_point.y = yy;

/* The const is a lie. The swapcolors stuff is a bit of a hack; a late
 * addition to the graphics library, which has to be used in many places
 * where the device interface only has a const gs_gstate available to it
 * rather than a non-const one. In all these cases, we flip the colors
 * during a call to a function, then swap it back again before we leave,
 * so the net effect of the call remains that the gstate is not changed.
 * Rather than breaking const at every single callsite, we move the const
 * breaking into the function itself. */
void gs_swapcolors_quick(const gs_gstate *);

/* Set the graphics_type_tag iff the requested tag bit is not set in the dev_color and	*/
/* unset the dev_color so that gx_set_dev_color will remap (encode) with the new tag.	*/
/* Also make sure the tag is set in the device so the two remain in sync.               */
static inline void ensure_tag_is_set(gs_gstate *pgs, gx_device *dev, gs_graphics_type_tag_t tag)
{
    if ((dev->graphics_type_tag & tag) == 0)
        dev_proc(dev, set_graphics_type_tag)(dev, tag);
    if (device_encodes_tags(dev)) {
        if ((pgs->color[0].dev_color->tag & tag) == 0) {
            gx_unset_dev_color(pgs);	/* current dev_color needs update to new tag */
            pgs->color[0].dev_color->tag = tag;	/* after unset, now set it */
        }
    }
}

/* Adjust the color reference counts for the current space. */
static inline void
cs_adjust_color_count(gs_gstate *pgs, int delta)
{
    gs_color_space *pcs = gs_currentcolorspace_inline(pgs);

    (pcs->type->adjust_color_count)(gs_currentcolor_inline(pgs),
                                    pcs, delta);
}

/* Adjust the color reference counts for the swapped space (i.e.
 * the one that is not current). */
static inline void
cs_adjust_swappedcolor_count(gs_gstate *pgs, int delta)
{
    gs_color_space *pcs = gs_swappedcolorspace_inline(pgs);

    (pcs->type->adjust_color_count)(gs_swappedcolor_inline(pgs),
                                    pcs, delta);
}

#endif /* gxistate_INCLUDED */
