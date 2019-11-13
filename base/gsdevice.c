/* Copyright (C) 2001-2019 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
*/


/* Device operators for Ghostscript library */
#include "ctype_.h"
#include "memory_.h"		/* for memchr, memcpy */
#include "string_.h"
#include "gx.h"
#include "gp.h"
#include "gscdefs.h"		/* for gs_lib_device_list */
#include "gserrors.h"
#include "gsfname.h"
#include "gsstruct.h"
#include "gspath.h"		/* gs_initclip prototype */
#include "gspaint.h"		/* gs_erasepage prototype */
#include "gsmatrix.h"		/* for gscoord.h */
#include "gscoord.h"		/* for gs_initmatrix */
#include "gzstate.h"
#include "gxcmap.h"
#include "gxdevice.h"
#include "gxdevmem.h"
#include "gxiodev.h"
#include "gxcspace.h"
#include "gsicc_manage.h"
#include "gscms.h"

/* Include the extern for the device list. */
extern_gs_lib_device_list();

/*
 * Finalization for devices: do any special finalization first, then
 * close the device if it is open, and finally free the structure
 * descriptor if it is dynamic.
 */
void
gx_device_finalize(const gs_memory_t *cmem, void *vptr)
{
    gx_device * const dev = (gx_device *)vptr;
    (void)cmem; /* unused */

    discard(gs_closedevice(dev));

    if (dev->icc_struct != NULL) {
        rc_decrement(dev->icc_struct, "gx_device_finalize(icc_profile)");
    }

    /* Deal with subclassed devices. Ordinarily these should not be a problem, we
     * will never see them, but if ths is a end of job restore we can end up
     * with the 'child' device(s) being freed before their parents. We need to make
     * sure we don't leave any dangling pointers in that case.
     */
    if (dev->child)
        dev->child->parent = dev->parent;
    if (dev->parent)
        dev->parent->child = dev->child;
    if (dev->PageList) {
        rc_decrement(dev->PageList, "gx_device_finalize(PageList)");
        dev->PageList = 0;
    }

    if (dev->finalize)
        dev->finalize(dev);

    if (dev->stype_is_dynamic)
        gs_free_const_object(dev->memory->non_gc_memory, dev->stype,
                             "gx_device_finalize");

#ifdef DEBUG
    /* Slightly ugly hack: because the garbage collector makes no promises
     * about the order objects can be garbage collected, it is possible for
     * a forwarding device to remain in existence (awaiting garbage collection
     * itself) after it's target marked as free memory by the garbage collector.
     * In such a case, the normal reference counting is fine (since the garbage
     * collector leaves the object contents alone until is has completed its
     * sweep), but the reference counting debugging attempts to access the
     * memory header to output type information - and the header has been
     * overwritten by the garbage collector, causing a crash.
     * Setting the rc memory to NULL here should be safe, since the memory
     * is now in the hands of the garbage collector, and means we can check in
     * debugging code to ensure we don't try to use values that not longer exist
     * in the memmory header.
     * In the non-gc case, finalize is the very last thing to happen before the
     * memory is actually freed, so the rc.memory pointer is moot.
     * See rc_object_type_name()
     */
    if (gs_debug_c('^'))
        dev->rc.memory = NULL;
#endif
}

/* "Free" a device locally allocated on the stack, by finalizing it. */
void
gx_device_free_local(gx_device *dev)
{
    gx_device_finalize(dev->memory, dev);
}

/* GC procedures */
static
ENUM_PTRS_WITH(device_enum_ptrs, gx_device *dev) return 0;
case 0:ENUM_RETURN(gx_device_enum_ptr(dev->parent));
case 1:ENUM_RETURN(gx_device_enum_ptr(dev->child));
ENUM_PTRS_END
static RELOC_PTRS_WITH(device_reloc_ptrs, gx_device *dev)
{
    dev->parent = gx_device_reloc_ptr(dev->parent, gcst);
    dev->child = gx_device_reloc_ptr(dev->child, gcst);
}
RELOC_PTRS_END
static
ENUM_PTRS_WITH(device_forward_enum_ptrs, gx_device_forward *fdev) return 0;
case 0: ENUM_RETURN(gx_device_enum_ptr(fdev->target));
ENUM_PTRS_END
static RELOC_PTRS_WITH(device_forward_reloc_ptrs, gx_device_forward *fdev)
{
    fdev->target = gx_device_reloc_ptr(fdev->target, gcst);
}
RELOC_PTRS_END

/*
 * Structure descriptors.  These must follow the procedures, because
 * we can't conveniently forward-declare the procedures.
 * (See gxdevice.h for details.)
 */
public_st_device();
public_st_device_forward();
public_st_device_null();

/* GC utilities */
/* Enumerate or relocate a device pointer for a client. */
gx_device *
gx_device_enum_ptr(gx_device * dev)
{
    if (dev == 0 || dev->memory == 0)
        return 0;
    return dev;
}
gx_device *
gx_device_reloc_ptr(gx_device * dev, gc_state_t * gcst)
{
    if (dev == 0 || dev->memory == 0)
        return dev;
    return RELOC_OBJ(dev);	/* gcst implicit */
}

/* Set up the device procedures in the device structure. */
/* Also copy old fields to new ones. */
void
gx_device_set_procs(gx_device * dev)
{
    if (dev->static_procs != 0) {	/* 0 if already populated */
        dev->procs = *dev->static_procs;
        dev->static_procs = 0;
    }
}

/* Flush buffered output to the device */
int
gs_flushpage(gs_gstate * pgs)
{
    gx_device *dev = gs_currentdevice(pgs);

    return (*dev_proc(dev, sync_output)) (dev);
}

/* Make the device output the accumulated page description */
int
gs_copypage(gs_gstate * pgs)
{
    return gs_output_page(pgs, 1, 0);
}
int
gs_output_page(gs_gstate * pgs, int num_copies, int flush)
{
    gx_device *dev = gs_currentdevice(pgs);
    cmm_dev_profile_t *dev_profile;
    int code;

    /* for devices that hook 'fill_path' in order to pick up gs_gstate */
    /* values such as dev_ht (such as tiffsep1), make a dummy call here   */
    /* to make sure that it has been called at least once		  */
    code = gs_gsave(pgs);
    if (code < 0)
        return code;
    if (((code = gs_newpath(pgs)) < 0) ||
        ((code = gs_moveto(pgs, 0.0, 0.0)) < 0) ||
    ((code = gs_setgray(pgs, 0.0)) < 0) ||
    ((code = gs_fill(pgs)) < 0))
    {
        gs_grestore(pgs);
	return code;
    }
    code = gs_grestore(pgs);
    if (code < 0)
        return code;

    if (dev->IgnoreNumCopies)
        num_copies = 1;
    if ((code = (*dev_proc(dev, output_page)) (dev, num_copies, flush)) < 0)
        return code;

    code = dev_proc(dev, get_profile)(dev, &(dev_profile));
    if (code < 0)
        return code;
    if (dev_profile->graydetection && !dev_profile->pageneutralcolor) {
        dev_profile->pageneutralcolor = true;             /* start detecting again */
        code = gsicc_mcm_begin_monitor(pgs->icc_link_cache, dev);
    }
    return code;
}

/*
 * Do generic work for output_page.  All output_page procedures must call
 * this as the last thing they do, unless an error has occurred earlier.
 */
int
gx_finish_output_page(gx_device *dev, int num_copies, int flush)
{
    dev->PageCount += num_copies;
    return 0;
}

/* Copy scan lines from an image device */
int
gs_copyscanlines(gx_device * dev, int start_y, byte * data, uint size,
                 int *plines_copied, uint * pbytes_copied)
{
    uint line_size = gx_device_raster(dev, 0);
    uint count = size / line_size;
    uint i;
    byte *dest = data;

    for (i = 0; i < count; i++, dest += line_size) {
        int code = (*dev_proc(dev, get_bits)) (dev, start_y + i, dest, NULL);

        if (code < 0) {
            /* Might just be an overrun. */
            if (start_y + i == dev->height)
                break;
            return_error(code);
        }
    }
    if (plines_copied != NULL)
        *plines_copied = i;
    if (pbytes_copied != NULL)
        *pbytes_copied = i * line_size;
    return 0;
}

/* Get the current device from the graphics state. */
gx_device *
gs_currentdevice(const gs_gstate * pgs)
{
    return pgs->device;
}

/* Get the name of a device. */
const char *
gs_devicename(const gx_device * dev)
{
    return dev->dname;
}

/* Get the initial matrix of a device. */
void
gs_deviceinitialmatrix(gx_device * dev, gs_matrix * pmat)
{
    fill_dev_proc(dev, get_initial_matrix, gx_default_get_initial_matrix);
    (*dev_proc(dev, get_initial_matrix)) (dev, pmat);
}

/* Get the N'th device from the known device list */
const gx_device *
gs_getdevice(int index)
{
    const gx_device *const *list;
    int count = gs_lib_device_list(&list, NULL);

    if (index < 0 || index >= count)
        return 0;		/* index out of range */
    return list[index];
}

/* Get the default device from the known device list */
const gx_device *
gs_getdefaultlibdevice(gs_memory_t *mem)
{
    const gx_device *const *list;
    int count = gs_lib_device_list(&list, NULL);
    const char *name, *end, *fin;
    int i;

    /* Search the compiled in device list for a known device name */
    /* In the case the lib ctx hasn't been initialised */
    if (mem && mem->gs_lib_ctx && mem->gs_lib_ctx->default_device_list) {
        name = mem->gs_lib_ctx->default_device_list;
        fin = name + strlen(name);
    }
    else {
        name = gs_dev_defaults;
        fin = name + strlen(name);
    }

    /* iterate through each name in the string */
    while (name < fin) {

      /* split a name from any whitespace */
      while ((name < fin) && (*name == ' ' || *name == '\t'))
        name++;
      end = name;
      while ((end < fin) && (*end != ' ') && (*end != '\t'))
        end++;

      /* return any matches */
      for (i = 0; i < count; i++)
        if ((end - name) == strlen(list[i]->dname))
          if (!memcmp(name, list[i]->dname, end - name))
            return gs_getdevice(i);

      /* otherwise, try the next device name */
      name = end;
    }

    /* Fall back to the first device in the list. */
    return gs_getdevice(0);
}

const gx_device *
gs_getdefaultdevice(void)
{
    return gs_getdefaultlibdevice(NULL);
}

/* Fill in the GC structure descriptor for a device. */
static void
gx_device_make_struct_type(gs_memory_struct_type_t *st,
                           const gx_device *dev)
{
    if (dev->stype)
        *st = *dev->stype;
    else if (dev_proc(dev, get_xfont_procs) == gx_forward_get_xfont_procs)
        *st = st_device_forward;
    else
        *st = st_device;
    st->ssize = dev->params_size;
}

/* Clone an existing device. */
int
gs_copydevice2(gx_device ** pnew_dev, const gx_device * dev, bool keep_open,
               gs_memory_t * mem)
{
    gx_device *new_dev;
    const gs_memory_struct_type_t *std = dev->stype;
    const gs_memory_struct_type_t *new_std;
    gs_memory_struct_type_t *a_std = 0;
    int code;

    if (dev->stype_is_dynamic) {
        /*
         * We allocated the stype for this device previously.
         * Just allocate a new stype and copy the old one into it.
         */
        a_std = (gs_memory_struct_type_t *)
            gs_alloc_bytes_immovable(mem->non_gc_memory, sizeof(*std),
                                     "gs_copydevice(stype)");
        if (!a_std)
            return_error(gs_error_VMerror);
        *a_std = *std;
        new_std = a_std;
    } else if (std != 0 && std->ssize == dev->params_size) {
        /* Use the static stype. */
        new_std = std;
    } else {
        /* We need to figure out or adjust the stype. */
        a_std = (gs_memory_struct_type_t *)
            gs_alloc_bytes_immovable(mem->non_gc_memory, sizeof(*std),
                                     "gs_copydevice(stype)");
        if (!a_std)
            return_error(gs_error_VMerror);
        gx_device_make_struct_type(a_std, dev);
        new_std = a_std;
    }
    /*
     * Because command list devices have complicated internal pointer
     * structures, we allocate all device instances as immovable.
     */
    new_dev = gs_alloc_struct_immovable(mem, gx_device, new_std,
                                        "gs_copydevice(device)");
    if (new_dev == 0) {
        gs_free_object(mem->non_gc_memory, a_std, "gs_copydevice(stype)");
        return_error(gs_error_VMerror);
    }
    gx_device_init(new_dev, dev, mem, false);
    gx_device_set_procs(new_dev);
    new_dev->stype = new_std;
    new_dev->stype_is_dynamic = new_std != std;
    /*
     * keep_open is very dangerous.  On the other hand, so is copydevice in
     * general, since it just copies the bits without any regard to pointers
     * (including self-pointers) that they may contain.  We handle this by
     * making the default finish_copydevice forbid copying of anything other
     * than the device prototype.
     */
    new_dev->is_open = dev->is_open && keep_open;
    fill_dev_proc(new_dev, finish_copydevice, gx_default_finish_copydevice);
    code = dev_proc(new_dev, finish_copydevice)(new_dev, dev);
    if (code < 0) {
        gs_free_object(mem, new_dev, "gs_copydevice(device)");
#if 0 /* gs_free_object above calls gx_device_finalize,
         which closes the device and releaszes its stype, i.e. a_std. */
        if (a_std)
            gs_free_object(dev->memory->non_gc_memory, a_std, "gs_copydevice(stype)");
#endif
        return code;
    }
    *pnew_dev = new_dev;
    return 0;
}
int
gs_copydevice(gx_device ** pnew_dev, const gx_device * dev, gs_memory_t * mem)
{
    return gs_copydevice2(pnew_dev, dev, false, mem);
}

/* Open a device if not open already.  Return 0 if the device was open, */
/* 1 if it was closed. */
int
gs_opendevice(gx_device *dev)
{
    if (dev->is_open)
        return 0;
    check_device_separable(dev);
    gx_device_fill_in_procs(dev);
    {
        int code = (*dev_proc(dev, open_device))(dev);

        if (code < 0)
            return_error(code);
        dev->is_open = true;
        return 1;
    }
}

static void
gs_gstate_update_device(gs_gstate *pgs, gx_device *dev)
{
    gx_set_cmap_procs(pgs, dev);
    gx_unset_both_dev_colors(pgs);
}

int
gs_gstate_putdeviceparams(gs_gstate *pgs, gx_device *dev, gs_param_list *plist)
{
    int code;
    gx_device *dev2;

    if (dev)
       dev2 = dev;
    else
       dev2 = pgs->device;

    code = gs_putdeviceparams(dev2, plist);
    if (code >= 0)
        gs_gstate_update_device(pgs, dev2);
    return code;
}

/* Set the device in the graphics state */
int
gs_setdevice(gs_gstate * pgs, gx_device * dev)
{
    int code = gs_setdevice_no_erase(pgs, dev);

    if (code == 1)
        code = gs_erasepage(pgs);
    return code;
}
int
gs_setdevice_no_erase(gs_gstate * pgs, gx_device * dev)
{
    int open_code = 0, code;
    gs_lib_ctx_t *libctx = gs_lib_ctx_get_interp_instance(pgs->memory);

    /* If the ICC manager is not yet initialized, set it up now.  But only
       if we have file io capability now */
    if (libctx->io_device_table != NULL) {
        cmm_dev_profile_t *dev_profile;
        if (pgs->icc_manager->lab_profile == NULL) {  /* pick one not set externally */
            code = gsicc_init_iccmanager(pgs);
            if (code < 0)
                return(code);
        }
        /* Also, if the device profile is not yet set then take care of that
           before we start filling pages, if we can */
        /* Although device methods should not be NULL, they are not completely filled in until
         * gx_device_fill_in_procs is called, and its possible for us to get here before this
         * happens, so we *must* make sure the method is not NULL before we use it.
         */
        if (dev->procs.get_profile != NULL) {
            code = dev_proc(dev, get_profile)(dev, &dev_profile);
            if (code < 0) {
                return(code);
            }
            if (dev_profile == NULL ||
                dev_profile->device_profile[gsDEFAULTPROFILE] == NULL) {
                if ((code = gsicc_init_device_profile_struct(dev, NULL,
                                                        gsDEFAULTPROFILE)) < 0)
                    return(code);
                /* set the intent too */
                if ((code = gsicc_set_device_profile_intent(dev, gsRINOTSPECIFIED,
                                                       gsDEFAULTPROFILE)) < 0)
                    return(code);
            }
        }
    }

    /* Initialize the device */
    if (!dev->is_open) {
        gx_device_fill_in_procs(dev);

        /* If we have not yet done so, and if we can, set the device profile
         * Doing so *before* the device is opened means that a device which
         * opens other devices can pass a profile on - for example, pswrite
         * also opens a bbox device
         */
        if (libctx->io_device_table != NULL) {
            cmm_dev_profile_t *dev_profile;
            /* Although device methods should not be NULL, they are not completely filled in until
             * gx_device_fill_in_procs is called, and its possible for us to get here before this
             * happens, so we *must* make sure the method is not NULL before we use it.
             */
            if (dev->procs.get_profile != NULL) {
                code = dev_proc(dev, get_profile)(dev, &dev_profile);
                if (code < 0) {
                    return(code);
                }
                if (dev_profile == NULL ||
                    dev_profile->device_profile[gsDEFAULTPROFILE] == NULL) {
                    if ((code = gsicc_init_device_profile_struct(dev, NULL,
                                                            gsDEFAULTPROFILE)) < 0)
                        return(code);
                }
            }
        }

        if (gs_device_is_memory(dev)) {
            /* Set the target to the current device. */
            gx_device *odev = gs_currentdevice_inline(pgs);

            while (odev != 0 && gs_device_is_memory(odev))
                odev = ((gx_device_memory *)odev)->target;
            gx_device_set_target(((gx_device_forward *)dev), odev);
        }
        code = open_code = gs_opendevice(dev);
        if (code < 0)
            return code;
    }
    gs_setdevice_no_init(pgs, dev);
    pgs->ctm_default_set = false;
    if ((code = gs_initmatrix(pgs)) < 0 ||
        (code = gs_initclip(pgs)) < 0
        )
        return code;
    /* If we were in a charpath or a setcachedevice, */
    /* we aren't any longer. */
    pgs->in_cachedevice = 0;
    pgs->in_charpath = (gs_char_path_mode) 0;
    return open_code;
}
int
gs_setdevice_no_init(gs_gstate * pgs, gx_device * dev)
{
    /*
     * Just set the device, possibly changing color space but no other
     * device parameters.
     *
     * Make sure we don't close the device if dev == pgs->device
     * This could be done by allowing the rc_assign to close the
     * old 'dev' if the rc goes to 0 (via the device structure's
     * finalization procedure), but then the 'code' from the dev
     * closedevice would not be propagated up. We want to allow
     * the code to be handled, particularly for the pdfwrite
     * device.
     */
    if (pgs->device != NULL && pgs->device->rc.ref_count == 1 &&
        pgs->device != dev) {
        int code = gs_closedevice(pgs->device);

        if (code < 0)
            return code;
    }
    rc_assign(pgs->device, dev, "gs_setdevice_no_init");
    gs_gstate_update_device(pgs, dev);
    return 0;
}

/* Initialize a just-allocated device. */
void
gx_device_init(gx_device * dev, const gx_device * proto, gs_memory_t * mem,
               bool internal)
{
    memcpy(dev, proto, proto->params_size);
    dev->memory = mem;
    dev->retained = !internal;
    rc_init(dev, mem, (internal ? 0 : 1));
    rc_increment(dev->icc_struct);
}

void
gx_device_init_on_stack(gx_device * dev, const gx_device * proto,
                        gs_memory_t * mem)
{
    memcpy(dev, proto, proto->params_size);
    dev->memory = mem;
    dev->retained = 0;
    dev->pad = proto->pad;
    dev->log2_align_mod = proto->log2_align_mod;
    dev->is_planar = proto->is_planar;
    rc_init(dev, NULL, 0);
}

/* Make a null device. */
void
gs_make_null_device(gx_device_null *dev_null, gx_device *dev,
                    gs_memory_t * mem)
{
    gx_device_init((gx_device *)dev_null, (const gx_device *)&gs_null_device,
                   mem, true);
    gx_device_set_target((gx_device_forward *)dev_null, dev);
    if (dev) {
        /* The gx_device_copy_color_params() call below should
           probably copy over these new-style color mapping procs, as
           well as the old-style (map_rgb_color and friends). However,
           the change was made here instead, to minimize the potential
           impact of the patch.
        */
        gx_device *dn = (gx_device *)dev_null;
        set_dev_proc(dn, get_color_mapping_procs, gx_forward_get_color_mapping_procs);
        set_dev_proc(dn, get_color_comp_index, gx_forward_get_color_comp_index);
        set_dev_proc(dn, encode_color, gx_forward_encode_color);
        set_dev_proc(dn, decode_color, gx_forward_decode_color);
        set_dev_proc(dn, get_profile, gx_forward_get_profile);
        set_dev_proc(dn, set_graphics_type_tag, gx_forward_set_graphics_type_tag);
        set_dev_proc(dn, begin_transparency_group, gx_default_begin_transparency_group);
        set_dev_proc(dn, end_transparency_group, gx_default_end_transparency_group);
        set_dev_proc(dn, begin_transparency_mask, gx_default_begin_transparency_mask);
        set_dev_proc(dn, end_transparency_mask, gx_default_end_transparency_mask);
        set_dev_proc(dn, discard_transparency_layer, gx_default_discard_transparency_layer);
        set_dev_proc(dn, pattern_manage, gx_default_pattern_manage);
        set_dev_proc(dn, push_transparency_state, gx_default_push_transparency_state);
        set_dev_proc(dn, pop_transparency_state, gx_default_pop_transparency_state);
        set_dev_proc(dn, put_image, gx_default_put_image);
        set_dev_proc(dn, copy_planes, gx_default_copy_planes);
        set_dev_proc(dn, copy_alpha_hl_color, gx_default_no_copy_alpha_hl_color);
        dn->graphics_type_tag = dev->graphics_type_tag;	/* initialize to same as target */
        gx_device_copy_color_params(dn, dev);
    }
}

/* Is a null device ? */
bool gs_is_null_device(gx_device *dev)
{
    /* Assuming null_fill_path isn't used elswhere. */
    return dev->procs.fill_path == gs_null_device.procs.fill_path;
}

/* Mark a device as retained or not retained. */
void
gx_device_retain(gx_device *dev, bool retained)
{
    int delta = (int)retained - (int)dev->retained;

    if (delta) {
        dev->retained = retained; /* do first in case dev is freed */
        rc_adjust_only(dev, delta, "gx_device_retain");
    }
}

/* Select a null device. */
int
gs_nulldevice(gs_gstate * pgs)
{
    int code = 0;
    gs_gstate *spgs;
    bool saveLockSafety = false;
    if (pgs->device == NULL || !gx_device_is_null(pgs->device)) {
        gx_device *ndev;
        code = gs_copydevice(&ndev, (const gx_device *)&gs_null_device,
                                 pgs->memory);

        if (code < 0)
            return code;
        if (gs_currentdevice_inline(pgs) != NULL)
            saveLockSafety = gs_currentdevice_inline(pgs)->LockSafetyParams;
        /*
         * Internal devices have a reference count of 0, not 1,
         * aside from references from graphics states.
         */
        /* There is some strange use of the null device in the code.  I need
           to sort out how the icc profile is best handled with this device.
           It seems to inherit properties from the current device if there
           is one */
        rc_init(ndev, pgs->memory, 0);
        if (pgs->device != NULL) {
            if ((code = dev_proc(pgs->device, get_profile)(pgs->device,
                                               &(ndev->icc_struct))) < 0)
                return code;
            rc_increment(ndev->icc_struct);
            set_dev_proc(ndev, get_profile, gx_default_get_profile);
        }

        if (gs_setdevice_no_erase(pgs, ndev) < 0) {
            gs_free_object(pgs->memory, ndev, "gs_copydevice(device)");
            /* We are out of options: find the device we installed in
               the initial graphics state, and put that in place.
               We just need something so we can end this job cleanly.
             */
            spgs = pgs->saved;
            if (spgs != NULL) {
                while (spgs->saved) spgs = spgs->saved;
                gs_currentdevice_inline(pgs) = gs_currentdevice_inline(spgs);
                rc_increment(gs_currentdevice_inline(pgs));
            }
            code = gs_note_error(gs_error_Fatal);
        }
        if (gs_currentdevice_inline(pgs) != NULL)
            gs_currentdevice_inline(pgs)->LockSafetyParams = saveLockSafety;
    }
    return code;
}

/* Close a device.  The client is responsible for ensuring that */
/* this device is not current in any graphics state. */
int
gs_closedevice(gx_device * dev)
{
    int code = 0;

    if (dev->is_open) {
        code = (*dev_proc(dev, close_device))(dev);
        dev->is_open = false;
        if (code < 0)
            return_error(code);
    }
    return code;
}

/*
 * Just set the device without any reinitializing.
 * (For internal use only.)
 */
void
gx_set_device_only(gs_gstate * pgs, gx_device * dev)
{
    rc_assign(pgs->device, dev, "gx_set_device_only");
}

/* Compute the size of one scan line for a device. */
/* If pad = 0 return the line width in bytes. If pad = 1,
 * return the actual raster value (the number of bytes to offset from
 * a byte on one scanline to the same byte on the scanline below.) */
uint
gx_device_raster(const gx_device * dev, bool pad)
{
    ulong bits = (ulong) dev->width * dev->color_info.depth;
    ulong raster;
    int l2align;

    if (dev->is_planar)
        bits /= dev->color_info.num_components;

    raster = (uint)((bits + 7) >> 3);
    if (!pad)
        return raster;
    l2align = dev->log2_align_mod;
    if (l2align < log2_align_bitmap_mod)
        l2align = log2_align_bitmap_mod;
    return (uint)(((bits + (8 << l2align) - 1) >> (l2align + 3)) << l2align);
}

uint
gx_device_raster_chunky(const gx_device * dev, bool pad)
{
    ulong bits = (ulong) dev->width * dev->color_info.depth;
    ulong raster;
    int l2align;

    raster = (uint)((bits + 7) >> 3);
    if (!pad)
        return raster;
    l2align = dev->log2_align_mod;
    if (l2align < log2_align_bitmap_mod)
        l2align = log2_align_bitmap_mod;
    return (uint)(((bits + (8 << l2align) - 1) >> (l2align + 3)) << l2align);
}
uint
gx_device_raster_plane(const gx_device * dev, const gx_render_plane_t *render_plane)
{
    ulong bpc = (render_plane && render_plane->index >= 0 ?
        render_plane->depth : dev->color_info.depth/(dev->is_planar ? dev->color_info.num_components : 1));
    ulong bits = (ulong) dev->width * bpc;
    int l2align;

    l2align = dev->log2_align_mod;
    if (l2align < log2_align_bitmap_mod)
        l2align = log2_align_bitmap_mod;
    return (uint)(((bits + (8 << l2align) - 1) >> (l2align + 3)) << l2align);
}

/* Adjust the resolution for devices that only have a fixed set of */
/* geometries, so that the apparent size in inches remains constant. */
/* If fit=1, the resolution is adjusted so that the entire image fits; */
/* if fit=0, one dimension fits, but the other one is clipped. */
int
gx_device_adjust_resolution(gx_device * dev,
                            int actual_width, int actual_height, int fit)
{
    double width_ratio = (double)actual_width / dev->width;
    double height_ratio = (double)actual_height / dev->height;
    double ratio =
    (fit ? min(width_ratio, height_ratio) :
     max(width_ratio, height_ratio));

    dev->HWResolution[0] *= ratio;
    dev->HWResolution[1] *= ratio;
    gx_device_set_width_height(dev, actual_width, actual_height);
    return 0;
}

/* Set the HWMargins to values defined in inches. */
/* If move_origin is true, also reset the Margins. */
/* Note that this assumes a printer-type device (Y axis inverted). */
void
gx_device_set_margins(gx_device * dev, const float *margins /*[4] */ ,
                      bool move_origin)
{
    int i;

    for (i = 0; i < 4; ++i)
        dev->HWMargins[i] = margins[i] * 72.0;
    if (move_origin) {
        dev->Margins[0] = -margins[0] * dev->HWResolution[0];
        dev->Margins[1] = -margins[3] * dev->HWResolution[1];
    }
}

static void
gx_device_set_hwsize_from_media(gx_device *dev)
{
    int rot = (dev->LeadingEdge & 1);
    double rot_media_x = rot ? dev->MediaSize[1] : dev->MediaSize[0];
    double rot_media_y = rot ? dev->MediaSize[0] : dev->MediaSize[1];

    dev->width = (int)(rot_media_x * dev->HWResolution[0] / 72.0 + 0.5);
    dev->height = (int)(rot_media_y * dev->HWResolution[1] / 72.0 + 0.5);
}

static void
gx_device_set_media_from_hwsize(gx_device *dev)
{
    int rot = (dev->LeadingEdge & 1);
    double x = dev->width * 72.0 / dev->HWResolution[0];
    double y = dev->height * 72.0 / dev->HWResolution[1];

    if (rot) {
        dev->MediaSize[1] = x;
        dev->MediaSize[0] = y;
    } else {
        dev->MediaSize[0] = x;
        dev->MediaSize[1] = y;
    }
}

/* Set the width and height, updating MediaSize to remain consistent. */
void
gx_device_set_width_height(gx_device * dev, int width, int height)
{
    dev->width = width;
    dev->height = height;
    gx_device_set_media_from_hwsize(dev);
}

/* Set the resolution, updating width and height to remain consistent. */
void
gx_device_set_resolution(gx_device * dev, double x_dpi, double y_dpi)
{
    dev->HWResolution[0] = x_dpi;
    dev->HWResolution[1] = y_dpi;
    gx_device_set_hwsize_from_media(dev);
}

/* Set the MediaSize, updating width and height to remain consistent. */
void
gx_device_set_media_size(gx_device * dev, double media_width, double media_height)
{
    dev->MediaSize[0] = media_width;
    dev->MediaSize[1] = media_height;
    gx_device_set_hwsize_from_media(dev);
}

/*
 * Copy the color mapping procedures from the target if they are
 * standard ones (saving a level of procedure call at mapping time).
 */
void
gx_device_copy_color_procs(gx_device *dev, const gx_device *target)
{
    dev_proc_map_cmyk_color((*from_cmyk)) =
        dev_proc(dev, map_cmyk_color);
    dev_proc_map_rgb_color((*from_rgb)) =
        dev_proc(dev, map_rgb_color);
    dev_proc_map_color_rgb((*to_rgb)) =
        dev_proc(dev, map_color_rgb);

    /* The logic in this function seems a bit stale; it sets the
       old-style color procs, but not the new ones
       (get_color_mapping_procs, get_color_comp_index, encode_color,
       and decode_color). It should probably copy those as well.
    */
    if (from_cmyk == gx_forward_map_cmyk_color ||
        from_cmyk == cmyk_1bit_map_cmyk_color ||
        from_cmyk == cmyk_8bit_map_cmyk_color) {
        from_cmyk = dev_proc(target, map_cmyk_color);
        set_dev_proc(dev, map_cmyk_color,
                     (from_cmyk == cmyk_1bit_map_cmyk_color ||
                      from_cmyk == cmyk_8bit_map_cmyk_color ?
                      from_cmyk : gx_forward_map_cmyk_color));
    }
    if (from_rgb == gx_forward_map_rgb_color ||
        from_rgb == gx_default_rgb_map_rgb_color) {
        from_rgb = dev_proc(target, map_rgb_color);
        set_dev_proc(dev, map_rgb_color,
                     (from_rgb == gx_default_rgb_map_rgb_color ?
                      from_rgb : gx_forward_map_rgb_color));
    }
    if (to_rgb == gx_forward_map_color_rgb ||
        to_rgb == cmyk_1bit_map_color_rgb ||
        to_rgb == cmyk_8bit_map_color_rgb) {
        to_rgb = dev_proc(target, map_color_rgb);
        set_dev_proc(dev, map_color_rgb,
                     (to_rgb == cmyk_1bit_map_color_rgb ||
                      to_rgb == cmyk_8bit_map_color_rgb ?
                      to_rgb : gx_forward_map_color_rgb));
    }
}

#define COPY_PARAM(p) dev->p = target->p

/*
 * Copy the color-related device parameters back from the target:
 * color_info and color mapping procedures.
 */
void
gx_device_copy_color_params(gx_device *dev, const gx_device *target)
{
        COPY_PARAM(color_info);
        COPY_PARAM(cached_colors);
        gx_device_copy_color_procs(dev, target);
}

/*
 * Copy device parameters back from a target.  This copies all standard
 * parameters related to page size and resolution, plus color_info
 * and (if appropriate) color mapping procedures.
 */
void
gx_device_copy_params(gx_device *dev, const gx_device *target)
{
#define COPY_ARRAY_PARAM(p) memcpy(dev->p, target->p, sizeof(dev->p))
        COPY_PARAM(width);
        COPY_PARAM(height);
        COPY_ARRAY_PARAM(MediaSize);
        COPY_ARRAY_PARAM(ImagingBBox);
        COPY_PARAM(ImagingBBox_set);
        COPY_ARRAY_PARAM(HWResolution);
        COPY_ARRAY_PARAM(Margins);
        COPY_ARRAY_PARAM(HWMargins);
        COPY_PARAM(PageCount);
        COPY_PARAM(MaxPatternBitmap);
#undef COPY_ARRAY_PARAM
        gx_device_copy_color_params(dev, target);
}

#undef COPY_PARAM

/*
 * Parse the output file name detecting and validating any %nnd format
 * for inserting the page count.  If a format is present, store a pointer
 * to its last character in *pfmt, otherwise store 0 there.
 * Note that we assume devices have already been scanned, and any % must
 * precede a valid format character.
 *
 * If there was a format, then return the max_width
 */
static int
gx_parse_output_format(gs_parsed_file_name_t *pfn, const char **pfmt)
{
    bool have_format = false, field;
    int width[2], int_width = sizeof(int) * 3, w = 0;
    uint i;

    /* Scan the file name for a format string, and validate it if present. */
    width[0] = width[1] = 0;
    for (i = 0; i < pfn->len; ++i)
        if (pfn->fname[i] == '%') {
            if (i + 1 < pfn->len && pfn->fname[i + 1] == '%') {
                i++;
                continue;
            }
            if (have_format)	/* more than one % */
                return_error(gs_error_undefinedfilename);
            have_format = true;
            field = -1; /* -1..3 for the 5 components of "%[flags][width][.precision][l]type" */
            for (;;)
                if (++i == pfn->len)
                    return_error(gs_error_undefinedfilename);
                else {
                    switch (field) {
                        case -1: /* flags */
                            if (strchr(" #+-", pfn->fname[i]))
                                continue;
                            else
                                field++;
                            /* falls through */
                        default: /* width (field = 0) and precision (field = 1) */
                            if (strchr("0123456789", pfn->fname[i])) {
                                width[field] = width[field] * 10 + pfn->fname[i] - '0';
                                continue;
                            } else if (0 == field && '.' == pfn->fname[i]) {
                                field++;
                                continue;
                            } else
                                field = 2;
                            /* falls through */
                        case 2: /* "long" indicator */
                            field++;
                            if ('l' == pfn->fname[i]) {
                                int_width = sizeof(long) * 3;
                                continue;
                            }
                            /* falls through */
                        case 3: /* type */
                            if (strchr("diuoxX", pfn->fname[i])) {
                                *pfmt = &pfn->fname[i];
                                break;
                            } else
                                return_error(gs_error_undefinedfilename);
                    }
                    break;
                }
        }
    if (have_format) {
        /* Calculate a conservative maximum width. */
        w = max(width[0], width[1]);
        w = max(w, int_width) + 5;
    }
    return w;
}

/*
 * Parse the output file name for a device, recognizing "-" and "|command",
 * and also detecting and validating any %nnd format for inserting the
 * page count.  If a format is present, store a pointer to its last
 * character in *pfmt, otherwise store 0 there.  Note that an empty name
 * is currently allowed.
 */
int
gx_parse_output_file_name(gs_parsed_file_name_t *pfn, const char **pfmt,
                          const char *fname, uint fnlen, gs_memory_t *memory)
{
    int code;

    *pfmt = 0;
    pfn->memory = 0;
    pfn->iodev = NULL;
    pfn->fname = NULL;		/* irrelevant since length = 0 */
    pfn->len = 0;
    if (fnlen == 0)  		/* allow null name */
        return 0;
    /*
     * If the file name begins with a %, it might be either an IODevice
     * or a %nnd format.  Check (carefully) for this case.
     */
    code = gs_parse_file_name(pfn, fname, fnlen, memory);
    if (code < 0) {
        if (fname[0] == '%') {
            /* not a recognized iodev -- may be a leading format descriptor */
            pfn->len = fnlen;
            pfn->fname = fname;
            code = gx_parse_output_format(pfn, pfmt);
        }
        if (code < 0)
            return code;
    }
    if (!pfn->iodev) {
        if ( (pfn->len == 1) && (pfn->fname[0] == '-') ) {
            pfn->iodev = gs_findiodevice(memory, (const byte *)"%stdout", 7);
            pfn->fname = NULL;
        } else if (pfn->fname[0] == '|') {
            pfn->iodev = gs_findiodevice(memory, (const byte *)"%pipe", 5);
            pfn->fname++, pfn->len--;
        } else
            pfn->iodev = iodev_default(memory);
        if (!pfn->iodev)
            return_error(gs_error_undefinedfilename);
    }
    if (!pfn->fname)
        return 0;
    code = gx_parse_output_format(pfn, pfmt);
    if (code < 0)
        return code;
    if (strlen(pfn->iodev->dname) + pfn->len + code >= gp_file_name_sizeof)
        return_error(gs_error_undefinedfilename);
    return 0;
}

/* Check if we write each page into separate file. */
bool
gx_outputfile_is_separate_pages(const char *fname, gs_memory_t *memory)
{
    const char *fmt;
    gs_parsed_file_name_t parsed;
    int code = gx_parse_output_file_name(&parsed, &fmt, fname,
                                         strlen(fname), memory);

    return (code >= 0 && fmt != 0);
}

/* Delete the current output file for a device (file must be closed first) */
int gx_device_delete_output_file(const gx_device * dev, const char *fname)
{
    gs_parsed_file_name_t parsed;
    const char *fmt;
    char *pfname = (char *)gs_alloc_bytes(dev->memory, gp_file_name_sizeof, "gx_device_delete_output_file(pfname)");
    int code;

    if (pfname == NULL) {
        code = gs_note_error(gs_error_VMerror);
	goto done;
    }

    code = gx_parse_output_file_name(&parsed, &fmt, fname, strlen(fname),
                                         dev->memory);
    if (code < 0) {
        goto done;
    }

    if (parsed.iodev && !strcmp(parsed.iodev->dname, "%stdout%"))
        goto done;

    if (fmt) {						/* filename includes "%nnd" */
        long count1 = dev->PageCount + 1;

        while (*fmt != 'l' && *fmt != '%')
            --fmt;
        if (*fmt == 'l')
            gs_sprintf(pfname, parsed.fname, count1);
        else
            gs_sprintf(pfname, parsed.fname, (int)count1);
    } else if (parsed.len && strchr(parsed.fname, '%'))	/* filename with "%%" but no "%nnd" */
        gs_sprintf(pfname, parsed.fname);
    else
        pfname[0] = 0; /* 0 to use "fname", not "pfname" */
    if (pfname[0]) {
        parsed.fname = pfname;
        parsed.len = strlen(parsed.fname);
    }
    if (parsed.iodev)
        code = parsed.iodev->procs.delete_file((gx_io_device *)(&parsed.iodev), (const char *)parsed.fname);
    else
        code = gs_note_error(gs_error_invalidfileaccess);

done:
    if (pfname != NULL)
        gs_free_object(dev->memory, pfname, "gx_device_delete_output_file(pfname)");

    return(code);
}

static int
noclose(FILE *f)
{
    return 0;
}

/* Open the output file for a device. */
int
gx_device_open_output_file(const gx_device * dev, char *fname,
                           bool binary, bool positionable, gp_file ** pfile)
{
    gs_parsed_file_name_t parsed;
    const char *fmt;
    char *pfname = (char *)gs_alloc_bytes(dev->memory, gp_file_name_sizeof, "gx_device_open_output_file(pfname)");
    int code;

    if (pfname == NULL) {
        code = gs_note_error(gs_error_VMerror);
	goto done;
    }

    if (strlen(fname) == 0) {
        code = gs_note_error(gs_error_undefinedfilename);
        emprintf1(dev->memory, "Device '%s' requires an output file but no file was specified.\n", dev->dname);
        goto done;
    }
    code = gx_parse_output_file_name(&parsed, &fmt, fname, strlen(fname), dev->memory);
    if (code < 0) {
        goto done;
    }

    if (parsed.iodev && !strcmp(parsed.iodev->dname, "%stdout%")) {
        if (parsed.fname) {
            code = gs_note_error(gs_error_undefinedfilename);
	    goto done;
	}
        *pfile = gp_file_FILE_alloc(dev->memory);
        if (*pfile == NULL) {
            code = gs_note_error(gs_error_VMerror);
            goto done;
        }
        gp_file_FILE_set(*pfile, dev->memory->gs_lib_ctx->core->fstdout, noclose);
        /* Force stdout to binary. */
        code = gp_setmode_binary_impl(dev->memory->gs_lib_ctx->core->fstdout, true);
	goto done;
    } else if (parsed.iodev && !strcmp(parsed.iodev->dname, "%pipe%")) {
        positionable = false;
    }
    if (fmt) {						/* filename includes "%nnd" */
        long count1 = dev->PageCount + 1;

        while (*fmt != 'l' && *fmt != '%')
            --fmt;
        if (*fmt == 'l')
            gs_sprintf(pfname, parsed.fname, count1);
        else
            gs_sprintf(pfname, parsed.fname, (int)count1);
    } else if (parsed.len && strchr(parsed.fname, '%'))	/* filename with "%%" but no "%nnd" */
        gs_sprintf(pfname, parsed.fname);
    else
        pfname[0] = 0; /* 0 to use "fname", not "pfname" */
    if (pfname[0]) {
        parsed.fname = pfname;
        parsed.len = strlen(parsed.fname);
    }
    if (parsed.iodev &&
        (positionable || parsed.iodev != iodev_default(dev->memory))) {
        char fmode[4];

        if (!parsed.fname) {
            code = gs_note_error(gs_error_undefinedfilename);
	    goto done;
	}
        strcpy(fmode, gp_fmode_wb);
        if (positionable)
            strcat(fmode, "+");
        code = parsed.iodev->procs.gp_fopen(parsed.iodev, parsed.fname, fmode,
                                            pfile, NULL, 0, dev->memory);
        if (code)
            emprintf1(dev->memory,
                      "**** Could not open the file %s .\n",
                      parsed.fname);
    } else {
        *pfile = gp_open_printer(dev->memory, (pfname[0] ? pfname : fname), binary);
        if (!(*pfile)) {
            emprintf1(dev->memory, "**** Could not open the file '%s'.\n", (pfname[0] ? pfname : fname));

            code = gs_note_error(gs_error_invalidfileaccess);
        }
    }

done:
    if (pfname != NULL)
        gs_free_object(dev->memory, pfname, "gx_device_open_output_file(pfname)");

    return(code);
}

/* Close the output file for a device. */
int
gx_device_close_output_file(const gx_device * dev, const char *fname,
                            gp_file *file)
{
    gs_parsed_file_name_t parsed;
    const char *fmt;
    int code = gx_parse_output_file_name(&parsed, &fmt, fname, strlen(fname),
                                         dev->memory);

    if (code < 0)
        return code;
    if (parsed.iodev) {
        if (!strcmp(parsed.iodev->dname, "%stdout%"))
            return 0;
        /* NOTE: fname is unsubstituted if the name has any %nnd formats. */
        if (parsed.iodev != iodev_default(dev->memory))
            return parsed.iodev->procs.fclose(parsed.iodev, file);
    }
    gp_close_printer(file, (parsed.fname ? parsed.fname : fname));
    return 0;
}

bool gx_color_info_equal(const gx_device_color_info * p1, const gx_device_color_info * p2)
{
    if (p1->anti_alias.graphics_bits != p2->anti_alias.graphics_bits)
        return false;
    if (p1->anti_alias.text_bits != p2->anti_alias.text_bits)
        return false;
    if (p1->black_component != p2->black_component)
        return false;
    if (strcmp(p1->cm_name, p2->cm_name) != 0)
        return false;
    if (p1->depth != p2->depth)
        return false;
    if (p1->dither_colors != p2->dither_colors)
        return false;
    if (p1->dither_grays != p2->dither_grays)
        return false;
    if (p1->gray_index != p2->gray_index)
        return false;
    if (p1->max_color != p2->max_color)
        return false;
    if (p1->max_components != p2->max_components)
        return false;
    if (p1->opmode != p2->opmode)
        return false;
    if (p1->polarity != p2->polarity)
        return false;
    if (p1->process_comps != p2->process_comps)
        return false;
    if (p1->separable_and_linear != p2->separable_and_linear)
        return false;
    if (p1->use_antidropout_downscaler != p2->use_antidropout_downscaler)
        return false;
    return true;
}
