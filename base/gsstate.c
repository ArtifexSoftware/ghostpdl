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


/* Miscellaneous graphics state operators for Ghostscript library */
#include "gx.h"
#include "memory_.h"
#include "gserrors.h"
#include "gsstruct.h"
#include "gsutil.h"             /* for gs_next_ids */
#include "gzstate.h"
#include "gxcspace.h"           /* here for gscolor2.h */
#include "gscolor2.h"
#include "gscoord.h"            /* for gs_initmatrix */
#include "gscie.h"
#include "gxclipsr.h"
#include "gxcmap.h"
#include "gxdevice.h"
#include "gxpcache.h"
#include "gzht.h"
#include "gzline.h"
#include "gspath.h"
#include "gzpath.h"
#include "gzcpath.h"
#include "gsovrc.h"
#include "gxcolor2.h"
#include "gscolor3.h" /* for gs_smoothness() */
#include "gxpcolor.h"
#include "gsicc_manage.h"
#include "gxdevsop.h"

/* Forward references */
static gs_gstate *gstate_alloc(gs_memory_t *, client_name_t,
                               const gs_gstate *);
static gs_gstate *gstate_clone_for_gsave(gs_gstate *,
                                         client_name_t);
static gs_gstate *gstate_clone_for_gstate(const gs_gstate *, gs_memory_t *,
                                          client_name_t);
static void gstate_free_contents(gs_gstate *);
static int gstate_copy(gs_gstate *, const gs_gstate *,
                        gs_gstate_copy_reason_t, client_name_t);
static void clip_stack_rc_adjust(gx_clip_stack_t *cs, int delta, client_name_t cname);

/*
 * Graphics state storage management is complicated.  There are many
 * different classes of storage associated with a graphics state:
 *
 * (1) The gstate object itself.  This includes some objects physically
 *      embedded within the gstate object, but because of garbage collection
 *      requirements, there are no embedded objects that can be
 *      referenced by non-transient pointers.  We assume that the gstate
 *      stack "owns" its gstates and that we can free the top gstate when
 *      doing a restore.
 *
 * (2) Objects that are referenced directly by the gstate and whose lifetime
 *      is independent of the gstate.  These are garbage collected, not
 *      reference counted, so we don't need to do anything special with them
 *      when manipulating gstates.  Currently this includes:
 *              font
 *
 * (3) Objects that are referenced directly by the gstate, may be shared
 *      among gstates, and should disappear when no gstates reference them.
 *      These fall into two groups:
 *
 *   (3a) Objects that are logically connected to individual gstates.
 *      We use reference counting to manage these.  Currently these are:
 *              halftone, dev_ht(4), cie_render, black_generation,
 *              undercolor_removal, set_transfer.*, cie_joint_caches,
 *              clip_stack, {opacity,shape}.mask
 *      effective_transfer.* may point to some of the same objects as
 *      set_transfer.*, but don't contribute to the reference count.
 *      Similarly, dev_color may point to the dev_ht object.  For
 *      simplicity, we initialize all of these pointers to 0 and then
 *      allocate the object itself when needed.
 *
 *   (3b) Objects whose lifetimes are associated with something else.
 *      Currently these are:
 *              pattern_cache, which is associated with the entire
 *                stack, is allocated when first needed, and currently
 *                is never freed;
 *              view_clip, which is associated with the current
 *                save level (effectively, with the gstate sub-stack
 *                back to the save) and is managed specially;
 *
 * (4) Objects that are referenced directly by exactly one gstate and that
 *      are not referenced (except transiently) from any other object.
 *      These fall into two groups:
 *
 *   (4b) Objects allocated individually, for the given reason:
 *              line_params.dash.pattern (variable-length),
 *              color_space, path, clip_path, effective_clip.path,
 *              ccolor, dev_color
 *                  (may be referenced from image enumerators or elsewhere)
 *
 *   (4b) The "client data" for a gstate.  For the interpreter, this is
 *      the refs associated with the gstate, such as the screen procedures.
 *      Client-supplied procedures manage client data.
 *
 * (5) Objects referenced indirectly from gstate objects of category (4),
 *      including objects that may also be referenced directly by the gstate.
 *      The individual routines that manipulate these are responsible
 *      for doing the right kind of reference counting or whatever.
 *      Currently:
 *              devices, path, clip_path, and (if different from both clip_path
 *                and view_clip) effective_clip.path require
 *                gx_path_assign/free, which uses a reference count;
 *              color_space and ccolor require cs_adjust_color/cspace_count
 *                or cs_adjust_counts, which use a reference count;
 *              dev_color has no references to storage that it owns.
 *      We count on garbage collection or restore to deallocate
 *        sub-objects of halftone.
 *
 * Note that when after a gsave, the existing gstate references the related
 * objects that we allocate at the same time, and the newly allocated gstate
 * references the old related objects.  Similarly, during a grestore, we
 * free the related objects referenced by the current gstate, but after the
 * grestore, we free the saved gstate, not the current one.  However, when
 * we allocate gstates off-stack, the newly allocated gstate does reference
 * the newly allocated component objects.  Note also that setgstate /
 * currentgstate may produce gstates in which different allocators own
 * different sub-objects; this is OK, because restore guarantees that there
 * won't be any dangling pointers (as long as we don't allow pointers from
 * global gstates to local objects).
 */

/*
 * Define these elements of the graphics state that are allocated
 * individually for each state, except for line_params.dash.pattern.
 * Note that effective_clip_shared is not on the list.
 */
typedef struct gs_gstate_parts_s {
    gx_path *path;
    gx_clip_path *clip_path;
    gx_clip_path *effective_clip_path;
    struct {
        gs_client_color *ccolor;
        gx_device_color *dev_color;
    } color[2];
} gs_gstate_parts;

#define GSTATE_ASSIGN_PARTS(pto, pfrom)\
  ((pto)->path = (pfrom)->path, (pto)->clip_path = (pfrom)->clip_path,\
   (pto)->effective_clip_path = (pfrom)->effective_clip_path,\
   (pto)->color[0].ccolor = (pfrom)->color[0].ccolor,\
   (pto)->color[0].dev_color = (pfrom)->color[0].dev_color,\
   (pto)->color[1].ccolor = (pfrom)->color[1].ccolor,\
   (pto)->color[1].dev_color = (pfrom)->color[1].dev_color)

extern_st(st_gs_gstate); /* for gstate_alloc() */

/* Copy client data, using the copy_for procedure if available, */
/* the copy procedure otherwise. */
static int
gstate_copy_client_data(const gs_gstate * pgs, void *dto, void *dfrom,
                        gs_gstate_copy_reason_t reason)
{
    return (pgs->client_procs.copy_for != 0 ?
            (*pgs->client_procs.copy_for) (dto, dfrom, reason) :
            (*pgs->client_procs.copy) (dto, dfrom));
}

/* ------ Operations on the entire graphics state ------ */

/*
 * Allocate a path for the graphics state.  We use stable memory because
 * some PostScript files have Type 3 fonts whose BuildChar procedure
 * uses the sequence save ... setcachedevice ... restore, and the path
 * built between the setcachedevice and the restore must not be freed.
 * If it weren't for this, we don't think stable memory would be needed.
 */
static gs_memory_t *
gstate_path_memory(gs_memory_t *mem)
{
    return gs_memory_stable(mem);
}

/* Allocate and initialize a graphics state. */
gs_gstate *
gs_gstate_alloc(gs_memory_t * mem)
{
    gs_gstate *pgs = gstate_alloc(mem, "gs_gstate_alloc", NULL);
    gs_memory_t *path_mem = gstate_path_memory(mem);
    int code;

    if (pgs == 0)
        return 0;
    GS_STATE_INIT_VALUES(pgs, 1.0);
    /* Need to set up at least enough to make gs_gstate_free happy */
    pgs->saved = 0;
    pgs->clip_stack = NULL;
    pgs->view_clip = NULL;
    pgs->font = NULL;
    pgs->root_font = NULL;
    pgs->show_gstate = NULL;
    pgs->device = NULL;

    /*
     * Just enough of the state is initialized at this point
     * that it's OK to call gs_gstate_free if an allocation fails.
     */

    code = gs_gstate_initialize(pgs, mem);
    if (code < 0)
        goto fail;

    /* Finish initializing the color rendering state. */

    rc_alloc_struct_1(pgs->halftone, gs_halftone, &st_halftone, mem,
                      goto fail, "gs_gstate_alloc(halftone)");
    pgs->halftone->type = ht_type_none;

    /* Initialize other things not covered by initgraphics */

    pgs->clip_stack = 0;
    pgs->view_clip = gx_cpath_alloc(path_mem, "gs_gstate_alloc(view_clip)");
    if (pgs->view_clip == NULL)
        goto fail;
    pgs->view_clip->rule = 0;   /* no clipping */
    pgs->effective_clip_id = pgs->clip_path->id;
    pgs->effective_view_clip_id = gs_no_id;
    pgs->in_cachedevice = 0;
    pgs->device = 0;            /* setting device adjusts refcts */
    code = gs_nulldevice(pgs);
    if (code < 0)
        goto fail;
    gs_setfillconstantalpha(pgs, 1.0);
    gs_setstrokeconstantalpha(pgs, 1.0);
    gs_setalphaisshape(pgs, false);
    gs_settransfer(pgs, gs_identity_transfer);
    gs_setflat(pgs, 1.0);
    gs_setfilladjust(pgs, 0.3, 0.3);
    gs_setlimitclamp(pgs, false);
    gs_setstrokeadjust(pgs, true);
    pgs->font = 0;              /* Not right, but acceptable until the */
    /* PostScript code does the first setfont. */
    pgs->root_font = 0;         /* ditto */
    pgs->in_charpath = (gs_char_path_mode) 0;
    pgs->show_gstate = 0;
    pgs->level = 0;
    if (gs_initgraphics(pgs) >= 0)
        return pgs;
    /* Something went very wrong. */
fail:
    gs_gstate_free(pgs);
    return 0;
}

/* Set the client data in a graphics state. */
/* This should only be done to a newly created state. */
void
gs_gstate_set_client(gs_gstate * pgs, void *pdata,
                    const gs_gstate_client_procs * pprocs, bool client_has_pattern_streams)
{
    pgs->client_data = pdata;
    pgs->client_procs = *pprocs;
    pgs->have_pattern_streams = client_has_pattern_streams;
}

/* Get the client data from a graphics state. */
#undef gs_gstate_client_data     /* gzstate.h makes this a macro */
void *
gs_gstate_client_data(const gs_gstate * pgs)
{
    return pgs->client_data;
}

/* Free the chain of gstates.*/
void
gs_gstate_free_chain(gs_gstate * pgs)
{
   gs_gstate *saved = pgs, *tmp;

   while(saved != 0) {
       tmp = saved->saved;
       gs_gstate_free(saved);
       saved = tmp;
   }
}

/* Free a graphics state. */
void
gs_gstate_free(gs_gstate * pgs)
{
    if (pgs == NULL)
        return;
    gstate_free_contents(pgs);
    gs_free_object(pgs->memory, pgs, "gs_gstate_free");
}

/* Save the graphics state. */
int
gs_gsave(gs_gstate * pgs)
{
    gs_gstate *pnew = gstate_clone_for_gsave(pgs, "gs_gsave");

    if (pnew == NULL)
        return_error(gs_error_VMerror);
    /* As of PLRM3, the interaction between gsave and the clip stack is
     * now clear. gsave stores the clip stack into the saved graphics
     * state, but then clears it in the current graphics state.
     *
     * Ordinarily, reference count rules would indicate an rc_decrement()
     * on pgs->clip_stack, but gstate_clone() has an exception for
     * the clip_stack field.
     */
    pgs->clip_stack = NULL;
    pgs->saved = pnew;
    if (pgs->show_gstate == pgs)
        pgs->show_gstate = pnew->show_gstate = pnew;
    pgs->trans_flags.xstate_change = false;
    pgs->level++;
    if_debug2m('g', pgs->memory, "[g]gsave -> "PRI_INTPTR", level = %d\n",
              (intptr_t)pnew, pgs->level);
    return 0;
}

/*
 * Save the graphics state for a 'save'.
 * We cut the stack below the new gstate, and return the old one.
 * In addition to an ordinary gsave, we create a new view clip path.
 */
int
gs_gsave_for_save(gs_gstate * pgs, gs_gstate ** psaved)
{
    int code;
    gx_clip_path *old_cpath = pgs->view_clip;
    gx_clip_path *new_cpath;

    if (old_cpath) {
        new_cpath =
            gx_cpath_alloc_shared(old_cpath, pgs->memory,
                                  "gs_gsave_for_save(view_clip)");
        if (new_cpath == 0)
            return_error(gs_error_VMerror);
    } else {
        new_cpath = 0;
    }
    code = gs_gsave(pgs);
    if (code < 0)
        goto fail;
    if (pgs->effective_clip_path == pgs->view_clip)
        pgs->effective_clip_path = new_cpath;
    pgs->view_clip = new_cpath;
    /* Cut the stack so we can't grestore past here. */
    *psaved = pgs->saved;
    pgs->saved = 0;

    code = gs_gsave(pgs);
    if (code < 0) {
        pgs->saved = *psaved;
        *psaved = NULL;
        gs_grestore(pgs);
        return code;
    }
    return code;
fail:
    if (new_cpath)
        gx_cpath_free(new_cpath, "gs_gsave_for_save(view_clip)");
    return code;
}

/* Restore the graphics state. Can fully empty graphics stack */
int     /* return 0 if ok, 1 if stack was empty */
gs_grestore_only(gs_gstate * pgs)
{
    gs_gstate *saved = pgs->saved;
    gs_gstate tmp_gstate;
    void *pdata = pgs->client_data;
    void *sdata;

    if_debug2m('g', pgs->memory, "[g]grestore "PRI_INTPTR", level was %d\n",
               (intptr_t)saved, pgs->level);
    if (!saved)
        return 1;
    sdata = saved->client_data;
    if (saved->pattern_cache == 0)
        saved->pattern_cache = pgs->pattern_cache;
    /* Swap back the client data pointers. */
    pgs->client_data = sdata;
    saved->client_data = pdata;
    if (pdata != 0 && sdata != 0)
        gstate_copy_client_data(pgs, pdata, sdata, copy_for_grestore);
    gstate_free_contents(pgs);
    tmp_gstate = *pgs;              /* temp after contents freed (with pointers zeroed) */
    *pgs = *saved;
    if (pgs->show_gstate == saved)
        pgs->show_gstate = pgs;
    *saved = tmp_gstate;            /* restore "freed" state (pointers zeroed after contents freed) */
    gs_free_object(pgs->memory, saved, "gs_grestore");

    return 0;
}

/* Restore the graphics state per PostScript semantics */
int
gs_grestore(gs_gstate * pgs)
{
    int code;
    if (!pgs->saved)
        return gs_gsave(pgs);   /* shouldn't ever happen */
    code = gs_grestore_only(pgs);
    if (code < 0)
        return code;

    /* Wraparound: make sure there are always >= 1 saves on stack */
    if (pgs->saved)
        return 0;
    return gs_gsave(pgs);
}

/* Restore the graphics state for a 'restore', splicing the old stack */
/* back on.  Note that we actually do a grestoreall + 2 grestores. */
int
gs_grestoreall_for_restore(gs_gstate * pgs, gs_gstate * saved)
{
    int code;

    while (pgs->saved->saved) {
        code = gs_grestore(pgs);
        if (code < 0)
            return code;
    }
    /* Make sure we don't leave dangling pointers in the caches. */
    if (pgs->pattern_cache)
        (*pgs->pattern_cache->free_all) (pgs->pattern_cache);
    pgs->saved->saved = saved;
    code = gs_grestore(pgs);
    if (code < 0)
        return code;
    if (pgs->view_clip) {
        gx_cpath_free(pgs->view_clip, "gs_grestoreall_for_restore");
        pgs->view_clip = 0;
    }
    return gs_grestore(pgs);
}

/* Restore to the bottommost graphics state (at this save level). */
int
gs_grestoreall(gs_gstate * pgs)
{
    if (!pgs->saved)            /* shouldn't happen */
        return gs_gsave(pgs);
    while (pgs->saved->saved) {
        int code = gs_grestore(pgs);

        if (code < 0)
            return code;
    }
    return gs_grestore(pgs);
}

/* Allocate and return a new graphics state. */
gs_gstate *
gs_gstate_copy(const gs_gstate * pgs, gs_memory_t * mem)
{
    gs_gstate *pnew;

    pnew = gstate_clone_for_gstate(pgs, mem, "gs_gstate");
    if (pnew == NULL)
        return NULL;
    clip_stack_rc_adjust(pnew->clip_stack, 1, "gs_gstate_copy");
    pnew->saved = NULL;
    /*
     * Prevent dangling references from the show_gstate pointer.  If
     * this context is its own show_gstate, set the pointer in the clone
     * to point to the clone; otherwise, set the pointer in the clone to
     * NULL, and let gs_setgstate fix it up.
     */
    pnew->show_gstate =
        (pgs->show_gstate == pgs ? pnew : NULL);
    return pnew;
}

/* Copy one previously allocated graphics state to another. */
int
gs_copygstate(gs_gstate * pto, const gs_gstate * pfrom)
{
    return gstate_copy(pto, pfrom, copy_for_copygstate, "gs_copygstate");
}

/* Copy the current graphics state to a previously allocated one. */
int
gs_currentgstate(gs_gstate * pto, const gs_gstate * pgs)
{
    int code =
        gstate_copy(pto, pgs, copy_for_currentgstate, "gs_currentgstate");

    if (code >= 0)
        pto->view_clip = 0;
    return code;
}

/* Restore the current graphics state from a previously allocated one. */
int
gs_setgstate(gs_gstate * pgs, const gs_gstate * pfrom)
{
    /*
     * The implementation is the same as currentgstate,
     * except we must preserve the saved pointer, the level,
     * the view clip, and possibly the show_gstate.
     */
    gs_gstate *saved_show = pgs->show_gstate;
    int level = pgs->level;
    gx_clip_path *view_clip = pgs->view_clip;
    int code;

    pgs->view_clip = 0;         /* prevent refcount decrementing */
    code = gstate_copy(pgs, pfrom, copy_for_setgstate, "gs_setgstate");
    if (code < 0)
        return code;
    pgs->level = level;
    pgs->view_clip = view_clip;
    pgs->show_gstate =
        (pgs->show_gstate == pfrom ? pgs : saved_show);
    return 0;
}

/* Get the allocator pointer of a graphics state. */
/* This is provided only for the interpreter */
/* and for color space implementation. */
gs_memory_t *
gs_gstate_memory(const gs_gstate * pgs)
{
    return pgs->memory;
}

/* Get the saved pointer of the graphics state. */
/* This is provided only for Level 2 grestore. */
gs_gstate *
gs_gstate_saved(const gs_gstate * pgs)
{
    return pgs->saved;
}

/* Swap the saved pointer of the graphics state. */
/* This is provided only for save/restore. */
gs_gstate *
gs_gstate_swap_saved(gs_gstate * pgs, gs_gstate * new_saved)
{
    gs_gstate *saved = pgs->saved;

    pgs->saved = new_saved;
    return saved;
}

/* Swap the memory pointer of the graphics state. */
/* This is provided only for the interpreter. */
gs_memory_t *
gs_gstate_swap_memory(gs_gstate * pgs, gs_memory_t * mem)
{
    gs_memory_t *memory = pgs->memory;

    pgs->memory = mem;
    return memory;
}

/* ------ Operations on components ------ */

/*
 * Push an overprint compositor onto the current device. Note that if
 * the current device already is an overprint compositor, the
 * composite will update its parameters but not create a new
 * compositor device.
 */
int
gs_gstate_update_overprint(gs_gstate * pgs, const gs_overprint_params_t * pparams)
{
    gs_composite_t *    pct = 0;
    int                 code;
    gx_device *         dev = pgs->device;
    gx_device *         ovptdev;

    code = gs_create_overprint(&pct, pparams, pgs->memory);
    if (code >= 0) {
        code = dev_proc(dev, composite)( dev,
                                                   &ovptdev,
                                                   pct,
                                                   pgs,
                                                   pgs->memory,
                                                   NULL);
        if (code >= 0 || code == gs_error_handled){
            if (code == 1) {
                gx_set_device_only(pgs, ovptdev);
                /* Get rid of extra reference */
                rc_decrement(ovptdev, "gs_gstate_update_overprint(ovptdev)");
            }
            code = 0;
        }
    }
    if (pct != 0)
        gs_free_object(pgs->memory, pct, "gs_gstate_update_overprint");

    /* the following hack handles devices that don't support compositors */
    if (code == gs_error_unknownerror && !pparams->retain_any_comps)
        code = 0;
    return code;
}

/*
 * Reset the overprint mode for the current color space and color. This
 * routine should be called  whenever the current device (i.e.: color
 * model), overprint, overprint mode, color space, or color are modified.
 *
 * The need reason this routine must be called for changes in the current
 * color and must consider the current color involves the Pattern color
 * space. In that space, the "color" (pattern) can determine if the base
 * color space is used (PatternType 1 with PaintType 2), or may provide
 * is own color space (PatternType 1 with PaintType 1, PatternType 2).
 *
 * The most general situation (PatternType 1 with PaintType 1) cannot be
 * handled properly due to limitations of the pattern cache mechanism,
 * so in this case overprint is effectively disable by making all color
 * components "drawn".
 */
int
gs_do_set_overprint(gs_gstate * pgs)
{
    const gs_color_space *  pcs = gs_currentcolorspace_inline(pgs);
    const gs_client_color * pcc = gs_currentcolor_inline(pgs);
    int                     code = 0;

    if (cs_num_components(pcs) < 0 && pcc->pattern != 0)
        code = pcc->pattern->type->procs.set_color(pcc, pgs);
    else {
        gx_device* dev = pgs->device;
        cmm_dev_profile_t* dev_profile;
        gs_color_space_index pcs_index = gs_color_space_get_index(pcs);

        dev_proc(dev, get_profile)(dev, &dev_profile);
        if (dev_profile->overprint_control == gs_overprint_control_disable)
            return code;

        /* Transparency device that supports spots and where we have
           sep or devicen colors needs special consideration if the device
           is in a additive blend mode.  This could
           be written more compactly, but it would be unreadable. */
        if (dev_proc(dev, dev_spec_op)(dev, gxdso_pdf14_sep_device, NULL, 0) &&
            (dev->color_info.polarity != GX_CINFO_POLARITY_SUBTRACTIVE) &&
            (pcs_index == gs_color_space_index_DeviceN ||
             pcs_index == gs_color_space_index_Separation)) {
            if (pcs_index == gs_color_space_index_Separation) {
                if (!(pcs->params.separation.color_type == SEP_MIX ||
                      pcs->params.separation.color_type == SEP_ENUM)) {
                    /* Sep color is not a spot color.  We can't do OP and trans */
                    return code;
                }
            }
            if (pcs_index == gs_color_space_index_DeviceN) {
                if (pcs->params.device_n.color_type != SEP_PURE_SPOT) {
                    /* DeviceN has process colors  We can't do OP and trans. */
                    return code;
                }
            }
        }

        /* If we have a CIE-based space, use the ICC equivalent space */
        if (gs_color_space_is_PSCIE(pcs) && pcs->icc_equivalent != NULL)
            pcs = pcs->icc_equivalent;

        /* The spaces that do not allow opm (e.g. ones that are not ICC or DeviceCMYK)
           will blow away any true setting later. But we have to be prepared
           in case this is a CMYK ICC space for example. Hence we set effective mode
           to mode here (Bug 698721)*/
        pgs->color[0].effective_opm = pgs->overprint_mode;

        if_debug2m(gs_debug_flag_overprint, pgs->memory,
            "[overprint] gs_do_set_overprint. Preset effective mode. pgs->color[0].effective_opm = %d pgs->color[1].effective_opm = %d\n",
            pgs->color[0].effective_opm, pgs->color[1].effective_opm);

        pcs->type->set_overprint(pcs, pgs);
    }
    return code;
}

/* setoverprint (non-stroke case) interpreter code
   ensures that this is called when appropriate. This
   should only be coming when we are doing PS files.
   As they don't have separate stroke and fill overprint
   controls */
void
gs_setoverprint(gs_gstate * pgs, bool ovp)
{
    pgs->overprint = ovp;
    pgs->stroke_overprint = ovp;
}

/* currentoverprint */
bool
gs_currentoverprint(const gs_gstate * pgs)
{
    return pgs->overprint;
}

/* setstrokeoverprint */
void
gs_setstrokeoverprint(gs_gstate * pgs, bool ovp)
{
    pgs->stroke_overprint = ovp;
}

/* currentstrokeoverprint */
bool
gs_currentstrokeoverprint(const gs_gstate * pgs)
{
    return pgs->stroke_overprint;
}

/* setstrokeoverprint */
void
gs_setfilloverprint(gs_gstate * pgs, bool ovp)
{
    pgs->overprint = ovp;
}

/* currentstrokeoverprint */
bool
gs_currentfilloverprint(const gs_gstate * pgs)
{
    return pgs->overprint;
}

/* setoverprintmode */
int
gs_setoverprintmode(gs_gstate * pgs, int mode)
{
    if (mode < 0 || mode > 1)
        return_error(gs_error_rangecheck);
    pgs->overprint_mode = mode;

    return 0;
}

/* currentoverprintmode */
int
gs_currentoverprintmode(const gs_gstate * pgs)
{
    return pgs->overprint_mode;
}

void
gs_setcpsimode(gs_memory_t *mem, bool mode)
{
    gs_lib_ctx_t *libctx = gs_lib_ctx_get_interp_instance(mem);

    libctx->core->CPSI_mode = mode;
}

/* currentcpsimode */
bool
gs_currentcpsimode(const gs_memory_t * mem)
{
    gs_lib_ctx_t *libctx = gs_lib_ctx_get_interp_instance(mem);

    return libctx->core->CPSI_mode;
}

/* The edgebuffer based scanconverter can only cope with values of 0
 * or 0.5 (i.e. 'center of pixel' or 'any part of pixel'). These
 * are the only values required for correct behaviour according to
 * the PDF and PS specs. Therefore, if we are using the edgebuffer
 * based scan converter, force these values. */
static void
sanitize_fill_adjust(gs_gstate * pgs)
{
    int scanconverter = gs_getscanconverter(pgs->memory);
    if (scanconverter >= GS_SCANCONVERTER_EDGEBUFFER || (GS_SCANCONVERTER_DEFAULT_IS_EDGEBUFFER && scanconverter == GS_SCANCONVERTER_DEFAULT)) {
        fixed adjust = (pgs->fill_adjust.x >= float2fixed(0.25) || pgs->fill_adjust.y >= float2fixed(0.25) ? fixed_half : 0);
        pgs->fill_adjust.x = adjust;
        pgs->fill_adjust.y = adjust;
    }
}

void
gs_setscanconverter(gs_gstate * gs, int converter)
{
    gs_lib_ctx_t *libctx = gs_lib_ctx_get_interp_instance(gs->memory);

    libctx->core->scanconverter = converter;

    sanitize_fill_adjust(gs);
}

/* getscanconverter */
int
gs_getscanconverter(const gs_memory_t * mem)
{
    gs_lib_ctx_t *libctx = gs_lib_ctx_get_interp_instance(mem);

    return libctx->core->scanconverter;
}

/* setrenderingintent
 *
 *  Use ICC numbers from Table 18 (section 6.1.11) rather than the PDF order
 *  to reduce re-coding and confusion.
 *    Perceptual            0
 *    Relative Colorimetric 1
 *    Saturation            2
 *    AbsoluteColorimetric  3
 */
int
gs_setrenderingintent(gs_gstate *pgs, int ri) {
    if (ri < 0 || ri > 3)
        return_error(gs_error_rangecheck);
    pgs->renderingintent = ri;
    return 0;
}

/* currentrenderingintent */
int
gs_currentrenderingintent(const gs_gstate * pgs)
{
    return pgs->renderingintent;
}

int
gs_setblackptcomp(gs_gstate *pgs, bool bkpt) {
    pgs->blackptcomp = bkpt;
    return 0;
}

/* currentrenderingintent */
bool
gs_currentblackptcomp(const gs_gstate * pgs)
{
    return pgs->blackptcomp;
}

/*
 * Reset most of the graphics state.
 */
int
gs_initgraphics(gs_gstate * pgs)
{
    int code;
    const gs_gstate gstate_initial = {
            gs_gstate_initial(1.0)
        };
    gs_matrix m;
    gs_make_identity(&m);

    gs_initmatrix(pgs);
    if ((code = gs_newpath(pgs)) < 0 ||
        (code = gs_initclip(pgs)) < 0 ||
        (code = gs_setlinewidth(pgs, 1.0)) < 0 ||
        (code = gs_setlinestartcap(pgs, gstate_initial.line_params.start_cap)) < 0 ||
        (code = gs_setlineendcap(pgs, gstate_initial.line_params.end_cap)) < 0 ||
        (code = gs_setlinedashcap(pgs, gstate_initial.line_params.dash_cap)) < 0 ||
        (code = gs_setlinejoin(pgs, gstate_initial.line_params.join)) < 0 ||
        (code = gs_setcurvejoin(pgs, gstate_initial.line_params.curve_join)) < 0 ||
        (code = gs_setdash(pgs, (float *)0, 0, 0.0)) < 0 ||
        (gs_setdashadapt(pgs, false),
         (code = gs_setdotlength(pgs, 0.0, false))) < 0 ||
        (code = gs_setdotorientation(pgs)) < 0 ||
        (code = gs_setmiterlimit(pgs, gstate_initial.line_params.miter_limit)) < 0
        )
        return code;
    gs_init_rop(pgs);
    /* Initialize things so that gx_remap_color won't crash. */
    if (pgs->icc_manager->default_gray == 0x00) {
        gs_color_space  *pcs1, *pcs2;

        pcs1 = gs_cspace_new_DeviceGray(pgs->memory);
        if (pcs1 == NULL)
            return_error(gs_error_unknownerror);

        if (pgs->color[0].color_space != NULL) {
            gs_setcolorspace(pgs, pcs1);
            rc_decrement_cs(pcs1, "gs_initgraphics");
        } else {
            pgs->color[0].color_space = pcs1;
            gs_setcolorspace(pgs, pcs1);
        }
        code = gx_set_dev_color(pgs);
        if (code < 0)
            return code;

        gs_swapcolors_quick(pgs); /* To color 1 */

        pcs2 = gs_cspace_new_DeviceGray(pgs->memory);
        if (pcs2 == NULL)
            return_error(gs_error_unknownerror);

        if (pgs->color[0].color_space != NULL) {
            gs_setcolorspace(pgs, pcs2);
            rc_decrement_cs(pcs2, "gs_initgraphics");
        } else {
            pgs->color[0].color_space = pcs2;
            gs_setcolorspace(pgs, pcs2);
        }
        code = gx_set_dev_color(pgs);

        gs_swapcolors_quick(pgs); /* To color 0 */

        if (code < 0)
            return code;

    } else {
        gs_color_space  *pcs1, *pcs2;

        pcs1 = gs_cspace_new_ICC(pgs->memory, pgs, 1);
        if (pcs1 == NULL)
            return_error(gs_error_unknownerror);

        if (pgs->color[0].color_space != NULL) {
            gs_setcolorspace(pgs, pcs1);
            rc_decrement_cs(pcs1, "gs_initgraphics");
        } else {
            pgs->color[0].color_space = pcs1;
            gs_setcolorspace(pgs, pcs1);
        }
        code = gx_set_dev_color(pgs);
        if (code < 0)
            return code;

        gs_swapcolors_quick(pgs); /* To color 1 */
        pcs2 = gs_cspace_new_ICC(pgs->memory, pgs, 1);
        if (pcs2 == NULL)
            return_error(gs_error_unknownerror);

        if (pgs->color[0].color_space != NULL) {
            gs_setcolorspace(pgs, pcs2);
            rc_decrement_cs(pcs2, "gs_initgraphics");
        } else {
            pgs->color[0].color_space = pcs2;
            gs_setcolorspace(pgs, pcs2);
        }
        code = gx_set_dev_color(pgs);

        gs_swapcolors_quick(pgs); /* To color 0 */

        if (code < 0)
            return code;
    }
    pgs->in_cachedevice = 0;

    code = gs_settextspacing(pgs, (double)0.0);
    if (code < 0)
        goto exit;
    code = gs_settextleading(pgs, (double)0.0);
    if (code < 0)
        goto exit;

    gs_settextrenderingmode(pgs, 0);

    code = gs_setwordspacing(pgs, (double)0.0);
    if (code < 0)
        goto exit;
    code = gs_settexthscaling(pgs, (double)100.0);
    if (code < 0)
        goto exit;

    gs_setaccuratecurves(pgs, true);

    code = gs_setstrokeconstantalpha(pgs, 1.0);
    if (code < 0)
        goto exit;
    code = gs_setfillconstantalpha(pgs, 1.0);
    if (code < 0)
        goto exit;
    code = gs_setalphaisshape(pgs, 0);
    if (code < 0)
        goto exit;
    code = gs_setblendmode(pgs, BLEND_MODE_Compatible);
    if (code < 0)
        goto exit;
    code = gs_settextknockout(pgs, true);
    if (code < 0)
        goto exit;
    code = gs_setsmoothness(pgs, 0.02); /* Match gs code */
    if (code < 0)
        goto exit;

    code = gs_settextmatrix(pgs, &m);
    if (code < 0)
        goto exit;

    code = gs_settextlinematrix(pgs, &m);
    if (code < 0)
        goto exit;
exit:
    return code;
}

/* setfilladjust */
int
gs_setfilladjust(gs_gstate * pgs, double adjust_x, double adjust_y)
{
#define CLAMP_TO_HALF(v)\
    ((v) <= 0 ? fixed_0 : (v) >= 0.5 ? fixed_half : float2fixed(v));

    pgs->fill_adjust.x = CLAMP_TO_HALF(adjust_x);
    pgs->fill_adjust.y = CLAMP_TO_HALF(adjust_y);

    sanitize_fill_adjust(pgs);

    return 0;
#undef CLAMP_TO_HALF
}

/* currentfilladjust */
int
gs_currentfilladjust(const gs_gstate * pgs, gs_point * adjust)
{
    adjust->x = fixed2float(pgs->fill_adjust.x);
    adjust->y = fixed2float(pgs->fill_adjust.y);
    return 0;
}

/* setlimitclamp */
void
gs_setlimitclamp(gs_gstate * pgs, bool clamp)
{
    pgs->clamp_coordinates = clamp;
}

/* currentlimitclamp */
bool
gs_currentlimitclamp(const gs_gstate * pgs)
{
    return pgs->clamp_coordinates;
}

/* settextrenderingmode */
void
gs_settextrenderingmode(gs_gstate * pgs, uint trm)
{
    pgs->text_rendering_mode = trm;
}

/* currenttextrenderingmode */
uint
gs_currenttextrenderingmode(const gs_gstate * pgs)
{
    return pgs->text_rendering_mode;
}

double
gs_currenttextspacing(const gs_gstate *pgs)
{
    return pgs->textspacing;
}

int
gs_settextspacing(gs_gstate *pgs, double Tc)
{
    int code = 0;
    gs_fixed_point dxy;

    code = gs_distance_transform2fixed(&pgs->ctm, Tc, 1, &dxy);
    if (code < 0)
        return code;

    pgs->textspacing = (float)Tc;
    return 0;
}

double
gs_currenttextleading(const gs_gstate *pgs)
{
    return pgs->textleading;
}

int
gs_settextleading(gs_gstate *pgs, double TL)
{
    pgs->textleading = (float)TL;
    return 0;
}

double
gs_currenttextrise(const gs_gstate *pgs)
{
    return pgs->textrise;
}

int
gs_settextrise(gs_gstate *pgs, double Ts)
{
    pgs->textrise = (float)Ts;
    return 0;
}

double
gs_currentwordspacing(const gs_gstate *pgs)
{
    return pgs->wordspacing;
}

int
gs_setwordspacing(gs_gstate *pgs, double Tw)
{
    pgs->wordspacing = (float)Tw;
    return 0;
}

int
gs_settexthscaling(gs_gstate *pgs, double Tz)
{
    pgs->texthscaling = (float)Tz;
    return 0;
}

double
gs_currenttexthscaling(const gs_gstate *pgs)
{
    return pgs->texthscaling;
}

int
gs_setPDFfontsize(gs_gstate *pgs, double Tf)
{
    pgs->PDFfontsize = (float)Tf;
    return 0;
}

double
gs_currentPDFfontsize(const gs_gstate *pgs)
{
    return pgs->PDFfontsize;
}

int
gs_settextlinematrix(gs_gstate *pgs, gs_matrix *m)
{
    pgs->textlinematrix.xx = m->xx;
    pgs->textlinematrix.xy = m->xy;
    pgs->textlinematrix.yx = m->yx;
    pgs->textlinematrix.yy = m->yy;
    pgs->textlinematrix.tx = m->tx;
    pgs->textlinematrix.ty = m->ty;
    return 0;
}
int
gs_gettextlinematrix(gs_gstate *pgs, gs_matrix *m)
{
    m->xx = pgs->textlinematrix.xx;
    m->xy = pgs->textlinematrix.xy;
    m->yx = pgs->textlinematrix.yx;
    m->yy = pgs->textlinematrix.yy;
    m->tx = pgs->textlinematrix.tx;
    m->ty = pgs->textlinematrix.ty;
    return 0;
}

int
gs_settextmatrix(gs_gstate *pgs, gs_matrix *m)
{
    pgs->textmatrix.xx = m->xx;
    pgs->textmatrix.xy = m->xy;
    pgs->textmatrix.yx = m->yx;
    pgs->textmatrix.yy = m->yy;
    pgs->textmatrix.tx = m->tx;
    pgs->textmatrix.ty = m->ty;
    return 0;
}
int
gs_gettextmatrix(gs_gstate *pgs, gs_matrix *m)
{
    m->xx = pgs->textmatrix.xx;
    m->xy = pgs->textmatrix.xy;
    m->yx = pgs->textmatrix.yx;
    m->yy = pgs->textmatrix.yy;
    m->tx = pgs->textmatrix.tx;
    m->ty = pgs->textmatrix.ty;
    return 0;
}


/* sethpglpathmode */
void
gs_sethpglpathmode(gs_gstate * pgs, bool path)
{
    pgs->hpgl_path_mode = path;
}

/* currenthpglpathmode */
bool
gs_currenthpglpathmode(const gs_gstate * pgs)
{
    return pgs->hpgl_path_mode;
}

/* ------ Internal routines ------ */

/* Free the privately allocated parts of a gstate. */
static void
gstate_free_parts(gs_gstate * parts, gs_memory_t * mem, client_name_t cname)
{
    gs_free_object(mem, parts->color[1].dev_color, cname);
    gs_free_object(mem, parts->color[1].ccolor, cname);
    gs_free_object(mem, parts->color[0].dev_color, cname);
    gs_free_object(mem, parts->color[0].ccolor, cname);
    parts->color[1].dev_color = 0;
    parts->color[1].ccolor = 0;
    parts->color[0].dev_color = 0;
    parts->color[0].ccolor = 0;
    if (!parts->effective_clip_shared && parts->effective_clip_path) {
        gx_cpath_free(parts->effective_clip_path, cname);
        parts->effective_clip_path = 0;
    }
    gx_cpath_free(parts->clip_path, cname);
    parts->clip_path = 0;
    if (parts->path) {
        gx_path_free(parts->path, cname);
        parts->path = 0;
    }
}

static inline void
gstate_parts_init_dev_color(gx_device_color *dc)
{
    gx_device_color_type dct = dc->type;
    gs_graphics_type_tag_t gtt = dc->tag;
    memset(dc, 0x00, sizeof(gx_device_color));
    dc->type = dct;
    dc->tag = gtt;
}

/* Allocate the privately allocated parts of a gstate. */
static int
gstate_alloc_parts(gs_gstate * parts, const gs_gstate * shared,
                   gs_memory_t * mem, client_name_t cname)
{
    gs_memory_t *path_mem = gstate_path_memory(mem);

    parts->path =
        (shared ?
         gx_path_alloc_shared(shared->path, path_mem,
                              "gstate_alloc_parts(path)") :
         gx_path_alloc(path_mem, "gstate_alloc_parts(path)"));
    parts->clip_path =
        (shared ?
         gx_cpath_alloc_shared(shared->clip_path, mem,
                               "gstate_alloc_parts(clip_path)") :
         gx_cpath_alloc(mem, "gstate_alloc_parts(clip_path)"));
    if (!shared || shared->effective_clip_shared) {
        parts->effective_clip_path = parts->clip_path;
        parts->effective_clip_shared = true;
    } else {
        parts->effective_clip_path =
            gx_cpath_alloc_shared(shared->effective_clip_path, mem,
                                  "gstate_alloc_parts(effective_clip_path)");
        parts->effective_clip_shared = false;
    }
    parts->color[0].color_space = NULL;
    parts->color[1].color_space = NULL;
    parts->color[0].ccolor =
        gs_alloc_struct(mem, gs_client_color, &st_client_color, cname);
    parts->color[1].ccolor =
        gs_alloc_struct(mem, gs_client_color, &st_client_color, cname);
    parts->color[0].dev_color =
        gs_alloc_struct(mem, gx_device_color, &st_device_color, cname);
    parts->color[1].dev_color =
        gs_alloc_struct(mem, gx_device_color, &st_device_color, cname);
    if (parts->path == 0 || parts->clip_path == 0 ||
        parts->effective_clip_path == 0 ||
        parts->color[0].ccolor == 0 || parts->color[0].dev_color == 0 ||
        parts->color[1].ccolor == 0 || parts->color[1].dev_color == 0
        ) {
        gstate_free_parts(parts, mem, cname);
        return_error(gs_error_VMerror);
    }
    gstate_parts_init_dev_color(parts->color[0].dev_color);
    gstate_parts_init_dev_color(parts->color[1].dev_color);
    return 0;
}

/*
 * Allocate a gstate and its contents.
 * If pfrom is not NULL, the path, clip_path, and (if distinct from both
 * clip_path and view_clip) effective_clip_path share the segments of
 * pfrom's corresponding path(s).
 */
static gs_gstate *
gstate_alloc(gs_memory_t * mem, client_name_t cname, const gs_gstate * pfrom)
{
    gs_gstate *pgs =
        gs_alloc_struct(mem, gs_gstate, &st_gs_gstate, cname);

    if (pgs == NULL)
        return NULL;
    memset(pgs, 0x00, sizeof(gs_gstate));
    if (gstate_alloc_parts(pgs, pfrom, mem, cname) < 0) {
        gs_free_object(mem, pgs, cname);
        return NULL;
    }
    pgs->memory = mem;
    return pgs;
}

/* Copy the dash pattern from one gstate to another. */
static int
gstate_copy_dash(gs_memory_t *mem, gx_dash_params *dash , const gs_gstate * pfrom)
{
    return gx_set_dash(dash, pfrom->line_params.dash.pattern,
                      pfrom->line_params.dash.pattern_size,
                      pfrom->line_params.dash.offset, mem);
}

typedef struct {
    gs_gstate_parts  parts;
    gx_dash_params   dash;
} gs_gstate_clone_data;

static gs_gstate *
gstate_clone_core(const gs_gstate               *pfrom,
                        gs_memory_t             *mem,
                        client_name_t            cname,
                        gs_gstate_clone_data    *clone_data,
                        gs_gstate_copy_reason_t  reason)
{
    gs_gstate *pgs = gstate_alloc(mem, cname, pfrom);
    void *pdata = NULL;

    if (pgs == NULL)
        return NULL;
    if (pfrom->client_data != NULL) {
        pdata = (*pfrom->client_procs.alloc) (mem);

        if (pdata == NULL ||
            gstate_copy_client_data(pfrom, pdata, pfrom->client_data,
                                    reason) < 0)
            goto failEarly;
    }
    /* Copy the dash and dash pattern if necessary. */
    clone_data->dash = gs_currentlineparams_inline(pfrom)->dash;
    if (clone_data->dash.pattern) {
        int code;

        clone_data->dash.pattern = NULL; /* Ensures a fresh allocation */
        code = gstate_copy_dash(mem, &clone_data->dash, pfrom);
        if (code < 0)
            goto fail;
    }
    /* Some records within pgs are allocated. We copy pfrom into pgs
     * wholesale (to avoid problems with the structure being updated and
     * us having to keep it in sync), so we copy those allocated regions
     * out first. The caller of this routine will then put them back
     * into either pgs or pfrom as appropriate. */
    GSTATE_ASSIGN_PARTS(&clone_data->parts, pgs);
    *pgs = *pfrom;
    pgs->client_data = pdata;

    gs_gstate_copied(pgs);
    /* Don't do anything to clip_stack. */

    rc_increment(pgs->device);
    *clone_data->parts.color[0].ccolor    = *pgs->color[0].ccolor;
    *clone_data->parts.color[0].dev_color = *pgs->color[0].dev_color;
    *clone_data->parts.color[1].ccolor    = *pgs->color[1].ccolor;
    *clone_data->parts.color[1].dev_color = *pgs->color[1].dev_color;
    cs_adjust_counts_icc(pgs, 1);
    cs_adjust_swappedcounts_icc(pgs, 1);

    return pgs;

  fail:
    gs_free_object(mem, clone_data->dash.pattern, cname);
    if (pdata != NULL)
        (*pfrom->client_procs.free) (pdata, mem, pgs);
  failEarly:
    gstate_free_parts(pgs, mem, cname);
    gs_free_object(mem, pgs, cname);

    return NULL;
}


/* Clone an existing graphics state for use in gsave. The clone refers
 * to the old contents, and the old state refers to the new contents. */
/* Return NULL if the allocation fails. */
static gs_gstate *
gstate_clone_for_gsave(gs_gstate     *pfrom,
                       client_name_t  cname)
{
    gs_gstate_clone_data clone_data;
    gs_gstate *pgs = gstate_clone_core(pfrom, pfrom->memory, cname,
                                       &clone_data, copy_for_gsave);

    if (pgs == NULL)
        return NULL;

    /* Newly allocated parts go back into pfrom, not pgs! */
    GSTATE_ASSIGN_PARTS(pfrom, &clone_data.parts);
    gs_currentlineparams_inline(pfrom)->dash = clone_data.dash;

    return pgs;
}

/* Clone an existing graphics state. The view_clip is not copied. */
/* Return NULL if the allocation fails. */
static gs_gstate *
gstate_clone_for_gstate(const gs_gstate     *pfrom,
                              gs_memory_t   *mem,
                              client_name_t  cname)
{
    gs_gstate_clone_data clone_data;
    gs_gstate *pgs = gstate_clone_core(pfrom, mem, cname, &clone_data,
                                       copy_for_gstate);

    if (pgs == NULL)
        return NULL;
    GSTATE_ASSIGN_PARTS(pgs, &clone_data.parts);
    pgs->view_clip = NULL;
    gs_currentlineparams_inline(pgs)->dash = clone_data.dash;
    pgs->memory = mem;

    return pgs;
}

/* Adjust reference counters for the whole clip stack */
/* accessible from the given point */
static void
clip_stack_rc_adjust(gx_clip_stack_t *cs, int delta, client_name_t cname)
{
    gx_clip_stack_t *p = cs;

    while(p) {
        gx_clip_stack_t *q = p;
        p = p->next;
        rc_adjust(q, delta, cname);
    }
}

/*
 * Finalization for graphics states. This is where we handle RC for those
 * elements.
 */
void
gs_gstate_finalize(const gs_memory_t *cmem,void *vptr)
{
    gs_gstate *pgs = (gs_gstate *)vptr;
    (void)cmem;	/* unused */

    if (cmem == NULL)
        return;			/* place for breakpoint */
    gstate_free_contents(pgs);
}

/* Release the composite parts of a graphics state, */
/* but not the state itself. */
static void
gstate_free_contents(gs_gstate * pgs)
{
    gs_memory_t *mem = pgs->memory;
    const char *const cname = "gstate_free_contents";

    rc_decrement(pgs->device, cname);
    pgs->device = 0;
    clip_stack_rc_adjust(pgs->clip_stack, -1, cname);
    pgs->clip_stack = 0;
    if (pgs->view_clip != NULL && pgs->level == 0) {
        gx_cpath_free(pgs->view_clip, cname);
        pgs->view_clip = NULL;
    }
    if (pgs->client_data != 0)
        (*pgs->client_procs.free) (pgs->client_data, mem, pgs);
    pgs->client_data = 0;
    cs_adjust_counts_icc(pgs, -1);
    cs_adjust_swappedcounts_icc(pgs, -1);
    pgs->color[0].color_space = 0;
    pgs->color[1].color_space = 0;
    gs_free_object(mem, pgs->line_params.dash.pattern, cname);
    pgs->line_params.dash.pattern = 0;
    gstate_free_parts(pgs, mem, cname);     /* this also clears pointers to freed elements */
    gs_gstate_release(pgs);
}

/* Copy one gstate to another. */
static int
gstate_copy(gs_gstate * pto, const gs_gstate * pfrom,
            gs_gstate_copy_reason_t reason, client_name_t cname)
{
    gs_gstate_parts parts;

    GSTATE_ASSIGN_PARTS(&parts, pto);
    /* Copy the dash pattern if necessary. */
    if (pfrom->line_params.dash.pattern || pto->line_params.dash.pattern) {
        int code = gstate_copy_dash(pto->memory,
                             &(gs_currentlineparams_inline(pto)->dash), pfrom);

        if (code < 0)
            return code;
    }
    /*
     * It's OK to decrement the counts before incrementing them,
     * because anything that is going to survive has a count of
     * at least 2 (pto and somewhere else) initially.
     * Handle references from contents.
     */
    cs_adjust_counts_icc(pto, -1);
    cs_adjust_swappedcounts_icc(pto, -1);
    gx_path_assign_preserve(pto->path, pfrom->path);
    gx_cpath_assign_preserve(pto->clip_path, pfrom->clip_path);
    /*
     * effective_clip_shared will be copied, but we need to do the
     * right thing with effective_clip_path.
     */
    if (pfrom->effective_clip_shared) {
        /*
         * pfrom->effective_clip_path is either pfrom->view_clip or
         * pfrom->clip_path.
         */
        parts.effective_clip_path =
            (pfrom->effective_clip_path == pfrom->view_clip ?
             pto->view_clip : parts.clip_path);
    } else
        gx_cpath_assign_preserve(pto->effective_clip_path,
                                 pfrom->effective_clip_path);
    *parts.color[0].ccolor    = *pfrom->color[0].ccolor;
    *parts.color[0].dev_color = *pfrom->color[0].dev_color;
    *parts.color[1].ccolor    = *pfrom->color[1].ccolor;
    *parts.color[1].dev_color = *pfrom->color[1].dev_color;
    /* Handle references from gstate object. */
    rc_pre_assign(pto->device, pfrom->device, cname);
    if (pto->clip_stack != pfrom->clip_stack) {
        clip_stack_rc_adjust(pfrom->clip_stack, 1, cname);
        clip_stack_rc_adjust(pto->clip_stack, -1, cname);
    }
    {
        struct gx_pattern_cache_s *pcache = pto->pattern_cache;
        void *pdata = pto->client_data;
        gs_memory_t *mem = pto->memory;
        gs_gstate *saved = pto->saved;
        float *pattern = pto->line_params.dash.pattern;

        gs_gstate_pre_assign(pto, (const gs_gstate *)pfrom);
        *pto = *pfrom;
        pto->client_data = pdata;
        pto->memory = mem;
        pto->saved = saved;
        pto->line_params.dash.pattern = pattern;
        if (pto->pattern_cache == 0)
            pto->pattern_cache = pcache;
        if (pfrom->client_data != 0) {
            /* We need to break 'const' here. */
            gstate_copy_client_data((gs_gstate *) pfrom, pdata,
                                    pfrom->client_data, reason);
        }
    }
    GSTATE_ASSIGN_PARTS(pto, &parts);
    cs_adjust_counts_icc(pto, 1);
    cs_adjust_swappedcounts_icc(pto, 1);
    pto->show_gstate =
        (pfrom->show_gstate == pfrom ? pto : 0);
    return 0;
}

/* Accessories. */
gs_id gx_get_clip_path_id(gs_gstate *pgs)
{
    return pgs->clip_path->id;
}

void gs_swapcolors_quick(const gs_gstate *cpgs)
{
    union {
        const gs_gstate *cpgs;
        gs_gstate *pgs;
    } const_breaker;
    gs_gstate *pgs;
    struct gx_cie_joint_caches_s *tmp_cie;
    gs_devicen_color_map          tmp_ccm;
    gs_client_color              *tmp_cc;
    int                           tmp;
    gx_device_color              *tmp_dc;
    gs_color_space               *tmp_cs;

    /* Break const just once, neatly, here rather than
     * hackily in every caller. */
    const_breaker.cpgs = cpgs;
    pgs = const_breaker.pgs;

    tmp_cc               = pgs->color[0].ccolor;
    pgs->color[0].ccolor = pgs->color[1].ccolor;
    pgs->color[1].ccolor = tmp_cc;

    tmp_dc                  = pgs->color[0].dev_color;
    pgs->color[0].dev_color = pgs->color[1].dev_color;
    pgs->color[1].dev_color = tmp_dc;

    tmp_cs                    = pgs->color[0].color_space;
    pgs->color[0].color_space = pgs->color[1].color_space;
    pgs->color[1].color_space = tmp_cs;

    /* Overprint and effective_op vary with stroke/fill and cs */
    tmp                         = pgs->color[0].effective_opm;
    pgs->color[0].effective_opm = pgs->color[1].effective_opm;
    pgs->color[1].effective_opm = tmp;

    /* Swap the bits of the gs_gstate that depend on the current color */
    tmp_cie                   = pgs->cie_joint_caches;
    pgs->cie_joint_caches     = pgs->cie_joint_caches_alt;
    pgs->cie_joint_caches_alt = tmp_cie;

    tmp_ccm                      = pgs->color_component_map;
    pgs->color_component_map     = pgs->color_component_map_alt;
    pgs->color_component_map_alt = tmp_ccm;

    pgs->is_fill_color = !(pgs->is_fill_color);	/* used by overprint for fill_stroke */
}
