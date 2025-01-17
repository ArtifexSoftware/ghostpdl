/* Copyright (C) 2001-2025 Artifex Software, Inc.
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


/* Interpreter graphics state definition */

#ifndef igstate_INCLUDED
#  define igstate_INCLUDED

#include "gsstate.h"
#include "gxstate.h"		/* for 'client data' access */
#include "imemory.h"
#include "istruct.h"		/* for gstate obj definition */
#include "gxcindex.h"

/*
 * From the interpreter's point of view, the graphics state is largely opaque,
 * i.e., the interpreter is just another client of the library.
 * The interpreter does require additional items in the graphics state;
 * these are "client data" from the library's point of view.
 * Most of the complexity in this added state comes from
 * the parameters associated with the various Level 2 color spaces.
 * Note that the added information consists entirely of refs.
 */

/*
 * The interpreter represents graphics state objects in a slightly
 * unnatural way, namely, by a t_astruct ref that points to an object
 * of type st_igstate_obj, which is essentially a t_struct ref that in turn
 * points to a real graphics state (object of type st_gs_gstate).
 * We do this so that save and restore can manipulate the intermediate
 * object and not have to worry about copying entire gs_gstates.
 *
 * Because a number of different operators must test whether an object
 * is a gstate, we make an exception to our convention of declaring
 * structure descriptors only in the place where the structure itself
 * is defined (see gsstruct.h for more information on this).
 */
typedef struct igstate_obj_s {
    ref gstate;			/* t_struct / st_gs_gstate */
} igstate_obj;

extern_st(st_igstate_obj);
#define public_st_igstate_obj()	/* in zdps1.c */\
  gs_public_st_ref_struct(st_igstate_obj, igstate_obj, "gstatetype")
#define igstate_ptr(rp) r_ptr(&r_ptr(rp, igstate_obj)->gstate, gs_gstate)

/* DeviceN names and tint transform */
typedef struct ref_device_n_params_s {
    ref layer_names, tint_transform;
} ref_device_n_params;

/* CIE transformation procedures */
typedef struct ref_cie_procs_s {
    union {
        ref DEFG;
        ref DEF;
    } PreDecode;
    union {
        ref ABC;
        ref A;
    } Decode;
    ref DecodeLMN;
} ref_cie_procs;

/* CIE rendering transformation procedures */
typedef struct ref_cie_render_procs_s {
    ref TransformPQR, EncodeLMN, EncodeABC, RenderTableT;
} ref_cie_render_procs;

/* Separation name and tint transform */
typedef struct ref_separation_params_s {
    ref layer_name, tint_transform;
} ref_separation_params;

/* All color space parameters. */
/* All of these are optional. */
/* Note that they may actually be the parameters for an underlying or */
/* alternate space for a special space. */
typedef struct ref_color_procs_s {
    ref_cie_procs cie;
    union {
        ref_device_n_params device_n;
        ref_separation_params separation;
        ref index_proc;
    } special;
} ref_color_procs;
typedef struct ref_colorspace_s {
    ref array;			/* color space (array), only relevant if */
                                /* the current color space has parameters */
                                /* associated with it. */
    ref orig_array_copy;    /* Copy of the original array, in case the program alers it */
    ref_color_procs procs;	/* associated procedures/parameters, */
                                /* only relevant for DeviceN, CIE, */
                                /* Separation, Indexed/CIE, */
                                /* Indexed with procedure, or a Pattern */
                                /* with one of these. */
} ref_colorspace;

typedef struct int_remap_color_info_s int_remap_color_info_t;

typedef struct int_gstate_s {
    ref dash_pattern_array;	/* (array) */
    /* Screen_procs are only relevant if setscreen was */
    /* executed more recently than sethalftone */
    /* (for this graphics context). */
    struct {
        ref red, green, blue, gray;
    } screen_procs,		/* halftone screen procedures */
          transfer_procs;	/* transfer procedures */
    ref black_generation;	/* (procedure) */
    ref undercolor_removal;	/* (procedure) */
    ref_colorspace colorspace[2];
    /*
     * Pattern is relevant only if the current color space
     * is a pattern space.
     */
    ref pattern[2];		/* pattern (dictionary) */
    struct {
        ref dict;		/* CIE color rendering dictionary */
        ref_cie_render_procs procs;	/* (see above) */
    } colorrendering;
    /*
     * Use_cie_color tracks the UseCIEColor parameter of the page
     * device. This parameter may, during initialization, be read
     * through the .getuseciecolor operator, and set (in Level 3)
     * via the .setuseciecolor operator.
     *
     * Previously, the UseCIEColor color space substitution feature
     * was implemented in the graphic library. It is now implemented
     * strictly in the interpreter.
     */
    ref use_cie_color;
    /*
     * Halftone is relevant only if sethalftone was executed
     * more recently than setscreen for this graphics context.
     * setscreen sets it to null.
     */
    ref halftone;		/* halftone (dictionary) */
    /*
     * Pagedevice is only relevant if setpagedevice was executed more
     * recently than nulldevice, setcachedevice, or setdevice with a
     * non-page device (for this graphics context).  If the current device
     * is not a page device, pagedevice is null.
     */
    ref pagedevice;		/* page device (dictionary|null) */
    /*
     * Remap_color_info is used temporarily to communicate the need for
     * Pattern or DeviceNcolor remapping to the interpreter.  See
     * gs_error_Remap_Color in gserrors.h.  The extra level of indirection through a
     * structure is needed because the gstate passed to the PaintProc is
     * different from the current gstate in the graphics state, and because
     * the DeviceN color being remapped is not necessarily the current color
     * in the graphics state (for shading or images): the structure is
     * shared, so that the interpreter can get its hands on the remapping
     * procedure.
     */
    ref remap_color_info;	/* t_struct (int_remap_color_info_t) */
    ref SMask; /* PDF only, null | dictionary | true */
} int_gstate;

#define clear_pagedevice(pigs) make_null(&(pigs)->pagedevice)
/*
 * Even though the interpreter's part of the graphics state actually
 * consists of refs, allocating it as refs tends to create sandbars;
 * since it is always allocated and freed as a unit, we can treat it
 * as an ordinary structure.
 */
#define private_st_int_gstate()	/* in zgstate.c */\
  gs_private_st_ref_struct(st_int_gstate, int_gstate, "int_gstate")

/* Enumerate the refs in an int_gstate. */
/* Since all the elements of an int_gstate are refs, this is simple. */
#define int_gstate_map_refs(p,m)\
 { register ref *rp_ = (ref *)(p);\
   register int i = sizeof(int_gstate) / sizeof(ref);\
   do { m(rp_); ++rp_; } while ( --i );\
 }

/* Create the gstate for a new context. */
/* We export this so that fork can use it. */
gs_gstate *int_gstate_alloc(const gs_dual_memory_t * dmem);

/* Get the int_gstate from a gs_gstate. */
#define gs_int_gstate(pgs) ((int_gstate *)gs_gstate_client_data(pgs))

/* The current instances for operators. */
#define igs (i_ctx_p->pgs)
#define istate gs_int_gstate(igs)

#endif /* igstate_INCLUDED */
