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


/* Definitions for device specific operations */

#ifndef gxdevsop_INCLUDED
#  define gxdevsop_INCLUDED

#include "gxdevcli.h"
#include "gsovrc.h"

/* This file enumerates a series of device specific operations, that can be
 * performed using the 'dev_spec_op' procedure in the gs_device structure.
 *
 * This scheme, as its name suggests, allows clients to make device specific
 * calls to any given device in a safe way. This is to gs_devices what an
 * ioctl is to unix file handles.
 *
 * Design features of this scheme ensure that:
 *  * Devices that support a given call can efficiently implement both
 *    input and output.
 *  * Devices that do not support a given call can efficiently refuse it
 *    (without having to know all the possible calls).
 *  * Devices that are built upon lower level devices can trivially either:
 *     + forward (possibly unknown) calls onto those sub devices
 *     + override known calls locally without passing them on
 *     + pass on known calls, and then modify their returned values.
 *  * New devices can add new device specific operations without requiring
 *    any changes to existing devices using the scheme.
 *
 * Traditionally, when developing Ghostscript, to add new device operations
 * we'd have extended the list of device procedures to include one (or more)
 * new entrypoint(s). With the dev_spec_op entrypoint in place, we have the
 * choice whether to do this or not.
 *
 * Calls which need to be supported by all (or most) devices may well result
 * in the extension of the device proc table as usual. Calls which only need
 * affect a relatively small number of devices may well result in the use
 * of a dev_spec_op call. The two schemes are complementary and which one is
 * most appropriate for any given extension will vary according to the
 * judgement of the programmer.
 *
 * All device_specific_operations are identified by a unique dev_spec_op
 * key, listed in the enumeration below. To add a new one, simply extend the
 * enumeration.
 *
 * The generic call to a dev_spec_op function is as follows:
 *
 * int dev_spec_op(gx_device *dev, int dev_spec_op, void *data, int size);
 *
 * As is common with most of ghostscript, a return value < 0 indicates an
 * error. In particular a return value of gs_error_undefined will be taken
 * to mean that a device did not support the given dev_spec_op. A return
 * value >= 0 indicates that the device operation completed successfully.
 * In general devices should use 0 to indicate successful completion, but
 * for some calls values > 0 may be used to indicate dev_spec_op specific
 * completion values.
 *
 * While this scheme is designed to be general and extensible, there are
 * some common use cases. Where possible, for consistency, implementers
 * should follow the example set by these existing 'families' of calls.
 *
 * Querying device characteristics (or 'Does this device support an
 * operation?')
 *
 *    Called with: data = NULL, size = 0.
 *    Returns: < 0 for 'does not support', > 0 for supports.
 *    Examples:
 *      gxdso_pattern_can_accum    (Does the device support pattern
 *                                  accumulation?)
 *      gxdso_planar_native        (Is the device natively planar?)
 *
 * Generic device function call:
 *
 *   Called with data -> structure full of arguments, size = sizeof that
 *   structure.
 *   Returns: usual gs_error conventions
 *   Examples:
 *     gxdso_foo                    (An example)
 *         data -> struct { int  x;
 *                          y_t *y;
 *                          z_t *z; }
 *         size = sizeof(*data);
 *     gxdso_bar                    (An example)
 *         data -> variable sized buffer of bytes to process
 *         size = sizeof(*data); (number of bytes in the buffer)
 *
 * Generic device function call (short form):
 *
 *   Where we have a limited number of arguments, we can avoid the overhead
 *   of creating a structure on the stack just to pass them.
 *   Returns: usual gs_error conventions
 *   Examples:
 *     gxdso_pattern_start_accum   (Start pattern accumulation)
 *         data = (void *)(gs_pattern1_instance_t *)pinst;
 *         size = (int)(gx_bitmap_is)id;
 *     gxdso_pattern_load     (Load a pattern)
 *         data  = NULL;
 *         size = (int)(gx_bitmap_id)id;
 */

/* Request data block for gxdso_device_child */
typedef struct gxdso_device_child_request_s
{
    gx_device *target;
    int        n;
} gxdso_device_child_request;

/* structure used to request a specific device parameter
 * be returned by a device.
 */
typedef struct dev_param_req_s {
    char *Param;
    void *list;
}dev_param_req_t;

/* structure used to pass the parameters for pattern handling */
typedef struct pattern_accum_param_t {
    void *pinst;
    gs_memory_t *interpreter_memory;
    void *graphics_state;
    int pinst_id;
}pattern_accum_param_s;

/* Structure used for device specific color setting */
typedef struct color_replace_s {
    gx_device_color *pdc;
    const gs_color_space *pcs;
    const gs_client_color *pcc;
    const gs_gstate *pgs;  /* Perhaps needed for named color profile information */
    cmm_profile_t *pdf14_iccprofile;
} color_replace_t;

/* Operation for pdf14 device to perform when in fill stroke
   commands occurring in gdevabuf.c (alphabits) device. This
   lets us get the pdf14 device setup for the stroke fill
   operation, and to clean up following the operation */
typedef enum {
    OP_FS_TRANS_PREFILL,
    OP_FS_TRANS_PRESTROKE,
    OP_FS_TRANS_POSTSTROKE,
    OP_FS_TRANS_CLEANUP
} OP_FS_TRANS;

/* Structure used for overprint communication coming from alpha buffer to owner */
typedef struct overprint_abuf_state_s {
    /* Client filled data that changes each call. */
    OP_FS_TRANS op_trans; /* What are we doing? */
    /* Client filled data that remains constant. */
    const gs_gstate *pgs;          /* From client (abuf) */
    gx_path *ppath;
    const gx_clip_path *pcpath;
    gs_log2_scale_point alpha_buf_path_scale;
    /* Callee storage. */
    unsigned char storage[256];
} overprint_abuf_state_t;

enum {
    /* All gxdso_ keys must be defined in this structure.
     * Do NOT rely on your particular gxdso_ having a particular value.
     * (i.e. do not use:  gxdso_foo = 256,  or similar below). The individual
     * values may change on any compile.)
     */

    /* gxdso_pattern_can_accum:
     *     data = UNUSED
     *     size = UNUSED
     * Returns 1 if device supports start_accum/finish_accum calls, returns 0
     * if it does not.
     * (Replaces the pattern_manage__can_accum pattern_manage call)
     */
    gxdso_pattern_can_accum,

    /* gxdso_pattern_start_accum:
     *     data = gs_pattern1_instance_t *      pattern to accumulate
     *     size = gs_id                         the id of the pattern
     * Called to start pattern accumulation. Returns >= 0 for success.
     * This call is only valid if gxdso_pattern_can_accum returns > 0.
     */
    gxdso_pattern_start_accum,

    /* gxdso_pattern_finish_accum:
     *     data = gs_pattern1_instance_t *      pattern to accumulate
     *     size = gs_id                         the id of the pattern
     * Called to finish pattern accumulation. Returns >= 0 for success.
     * This call is only valid if gxdso_pattern_can_accum returns > 0.
     */
    gxdso_pattern_finish_accum,

    /* gxdso_pattern_load:
     *     data = NULL
     *     size = gs_id                         pattern id to load
     * Called to add a specified pattern to the PDF resources table.
     * Returns >= 0 for success.
     * This call is only valid if gxdso_pattern_can_accum returns > 0.
     */
    gxdso_pattern_load,

    /* gxdso_pattern_shading_area:
     *     data = NULL
     *     size = 0
     * Returns 1 if the device needs the shading area to be calculated.
     */
    gxdso_pattern_shading_area,

    /* gxdso_pattern_is_cpath_accum:
     *     data = NULL
     *     size = 0
     * Returns 1 if the device is a clipping path accumulator (used for
     * converting imagemasks to clipping paths).
     */
    gxdso_pattern_is_cpath_accum,

    /* gxdso_pattern_doesnt_need_path:
     *     data = NULL
     *     size = 0
     * Returns 1 if the device can perform a gx_fill_path without the
     * clipping path being converted to a path.
     */
    gxdso_pattern_shfill_doesnt_need_path,

    /* gxdso_pattern_handles_clip_path:
     *     data = NULL
     *     size = 0
     * Returns 1 if the device supports an optimisation for plotting clipped
     * patterns.
     */
    gxdso_pattern_handles_clip_path,

    /* gxdso_is_std_cmyk_1bit:
     *     data = NULL
     *     size = 0
     * Returns 1 if the device is a 'standard' 1bit per component cmyk device.
     * (standard means, 'maps colors equivalently to cmyk_1bit_map_cmyk_color')
     */
    gxdso_is_std_cmyk_1bit,

    /* gxdso_is_pdf14_device:
     * Either:
     *     data = NULL
     *     size = 0
     *   Returns 1 if the device is a pdf14 device .
     * Or:
     *     data = pointer to a place to store a pdf14_device *
     *     size = sizeof(pdf14_device *).
     *   Returns 1 if the device is a pdf14 device, and fills data with the
     *   pointer to the pdf14 device (may be a child of the original device)
     */
    gxdso_is_pdf14_device,

    gxdso_supports_pattern_transparency,

    /* gxdso_device_child:
     *      data = pointer to gxdso_device_child_request struct
     *      size = sizeof(gxdso_device_child_request)
     * Returns 1 if found.
     * Call with target = device, n = 0 initially, or value returned from
     * previous call.
     * If a device X is called with data->target == X:
     *    If data->n == 0: fills in data->target with the address of the
     *                     first device child target. Fill in data->n with 0
     *                     if no more children, or a continuation value.
     *    If data->n != 0: data->n is a continuation value previously
     *                     returned. Set data->target to the next device
     *                     child target. Set data->n to 0 if no more, or
     *                     another continuation value.
     * Else:
     *    call each child device in turn until one returns a non zero result.
     *    Or return 0 if none do.
     */
    gxdso_device_child,
    /* gxdso_supports_devn:
     *      data = NULL
     *      size = 0
     * Returns 1 if the device supports devicen colors.  example tiffsep.
     */
    gxdso_supports_devn,
    /* gxdso_supports_hlcolor:
     * for devices that can handle pattern and other high level structures
       directly. */
    gxdso_supports_hlcolor,
    /* gxdso_interpolate_threshold:
     * Some devices may wish to suppress using interpolation for scaling
     * unless it is above a given threshold (for example halftoning devices
     * may only want to upscale if the upscale factor is larger than the
     * halftone tile size). data and size are ignored; the return value is
     * 0 for 'no special treatment', or an integer upscaling threshold below
     * which interpolation should be suppressed (for both upscaling and
     * downscaling). */
    gxdso_interpolate_threshold,
    /* gxdso_interpolate_antidropout:
     * Some devices may wish to use a special 'antidropout' downscaler.
     * Return 0 for 'no special treatment', or 1 for the anitdropout
     * downscaler. */
    gxdso_interpolate_antidropout,
    /* gxdso_needs_invariant_palette:
     * The PCL interpreter can set a /Indexed coluor space, and then
     * alter the palette afterwards. For rendering the paletter lookup
     * is done as required, so this works, but for high level devices
     * (eg pdfwrite) we can't deal with this. return '0' if the device
     * doesn't care if the palette changes, and 1 if it does.
     */
    gxdso_needs_invariant_palette,
    /* gxdso_supports_saved_pages:
     * gx_device_printer devices can support this saving pages as clist
     */
    gxdso_supports_saved_pages,
    /* Form handling, we need one to start and one to stop a form
     */
    gxdso_form_begin,
    gxdso_form_end,
    /* These next two relate to high level form handling. After executing a form the
     * PostScript will request an ID for the form. If it gets one, it stores it in the
     * /Implementation in the Form dictioanry. Next time it encoutners 'execform' for that
     * form it will not call gxdso_form_begin and gxdso_form_end, instead it will simply call
     * gxdso_repeat_form with the ID presented earlier. You should not return anything in response
     * to the gxdso_form_ID unless the device is capable of storing the form and repeating it
     * without running the PaintProc again.
     */
    gxdso_get_form_ID,
    gxdso_repeat_form,
    /* gxdso_adjust_bandheight:
     * Adjust the bandheight given in 'size' (normally downwards). Typically
     * to round it to a multiple of a given number.
     */
    gxdso_adjust_bandheight,
    /* Retrieve a *single* device parameter */
    gxdso_get_dev_param,
    /* gxdso_in_pattern_accumulator:
     *     data = NULL
     *     size = 0
     * Returns +ve value if we are rendering into a pattern accumulator
     * device.
     */
    gxdso_in_pattern_accumulator,
    /* Determine if we are in a PDF14 device and the target is a separation
     * device.   In this case, we may want to not use the alternate tint
     * tranform even if the blending color space is RGB or Gray. */
    gxdso_pdf14_sep_device,
    /* Used only by pdfwrite to pass a Form Appearance Name, so that
     * we can use the name in a pdfmark.
     */
    gxdso_pdf_form_name,
    gxdso_pdf_last_form_ID,
    /* Restrict the supplied bbox to that actually used by the underlying device.
     * Used to restrict alphabits drawing to the area defined by compositors etc.*/
    gxdso_restrict_bbox,
    /* JPEG passthrough requests/control. Currently used for the pdfwrite family only.
     */
    gxdso_JPEG_passthrough_query,
    gxdso_JPEG_passthrough_begin,
    gxdso_JPEG_passthrough_data,
    gxdso_JPEG_passthrough_end,
    /* And similarly JPX passthrough requests/control. Currently used for the pdfwrite family only.
     */
    gxdso_JPX_passthrough_query,
    gxdso_JPX_passthrough_begin,
    gxdso_JPX_passthrough_data,
    gxdso_JPX_passthrough_end,

    gxdso_supports_iccpostrender,
    /* Retrieve the last device in a device chain
       (either forwarding or subclass devices).
     */
    gxdso_current_output_device,
    /* Should we call copy_color rather than resolving images to fill_rectangles? */
    gxdso_copy_color_is_fast,
    /* gxdso_is_encoding_direct:
     *     data = NULL
     *     size = 0
     * Returns 1 if the device maps color byte values directly into the device,
     * 0 otherwise.
     */
    gxdso_is_encoding_direct,
    /* gxdso_event_info:
     *     data = dev_param_req_t
     *     size = sizeof(dev_param_req_t)
     * Passes a single name in request->Param, naming the event which occurred.
     * Used to send a warning to pdfwrite that some event has happened we want to know about.
     * Currently this is used in pdf_font.ps to signal that a substittue font has been
     * used. If we are emitting PDF/A then we need to abort it, as the Widths array of
     * the PDF font may not match the widths of the glyphs in the font.
     */
    gxdso_event_info,
    /* gxdso_overprint_active:
    *     data = NULL
    *     size = 0
    * Returns 1 if the overprint device is active,
    * 0 otherwise.
    */
    gxdso_overprint_active,
    /* gxdso_in_smask:
    *     data = NULL
    *     size = 0
    * Returns 1 if we are within an smask (either construction or usage),
    * 0 otherwise.
    */
    gxdso_in_smask,
    /* gxdso_in_smask_construction:
    *     data = NULL
    *     size = 0
    * Returns 1 if we are within an smask construction,
    * 0 otherwise.
    */
    gxdso_in_smask_construction,
    /* Debug only dsos follow here */
#ifdef DEBUG
    /* Private dso used to check that a printer device properly forwards to the default */
    gxdso_debug_printer_check,
#endif
    /* gxdso_supports_alpha:
     *     data = NULL
     *     size = 0
     * Returns 1 if the rendering device supports alpha,
     * 0 otherwise.
     */
    gxdso_supports_alpha,
    /* gxdso_reopen_after_init:
     *     data = NULL
     *     size = 0
     * Returns 1 if the device should be closed/reopened after gs
     * finishes initialisation (e.g. to give it a chance to fetch
     * configuration from registered callout handlers),
     * 0 otherwise.
     */
    gxdso_reopen_after_init,

    /* Normally, when we set a device ICC profile, we validate that the number
     * of components against the number of components the device supports. But
     * in certain cases, we don't want to do that (DeviceN devices and nullpage,
     * for example).
     */
    gxdso_skip_icc_component_validation,

    /* gxdso_copy_alpha_disabled:
     *     data = NULL
     *     size = 0
     * Returns 1 if the command list device sets clist_disable_copy_alpha flag,
     * 0 otherwise.
     */
    gxdso_copy_alpha_disabled,

    /* gxdso_set_HWSize:
     *     data = int[2], [0] is width [1] is height
     *     size = sizeof(int[2])
     * Returns 1 if the device controls dev->width, dev->height
     * 0 otherwise.
     * NB: caller should set dev->wdith and dev->height if return is <= 0
     */
    gxdso_set_HWSize,

    /* gxdso_device_insert_child:
     *     data = pointer to device to insert as child of the device
     *            handling this special op.
     *     size = 0
     * Returns 0 if completed successfully, negative otherwise.
     */
    gxdso_device_insert_child,

    /* Determine if a given device is a clist one. Returns 1 if it is. */
    gxdso_is_clist_device,

    /* Determine if a given device is a null device. Returns 1 if it is. */
    gxdso_is_null_device,

    /* Get information about pdf14 device overprint simulation state
     *     data = unsigned char[2], [0] is overprint_sim_push [1] is num_spot_colors_int
     *     size = sizeof(unsigned char[2])
     * Returns 1 if returned values are valid
     * 0 otherwise.
     */
    gxdso_overprintsim_state,

    /* Used for handing state settings between the abuf device and the
     * transparency and overprint compositor.
     *     data = pointer to overprint_abuf_state_t structure.
     *            op_trans will be OP_FS_TRANS_PREFILL on the first call.
     *            In normal operation it will then be OP_FS_TRANS_PRESTROKE
     *            and then OP_FS_TRANS_POSTSTROKE on subsequent calls. The
     *            contents of the structure will otherwise remain constant
     *            between calls.
     *            In the event of an error return from this call, no further
     *            calls will be made on with this param block (i.e. the
     *            callee must tidy itself up).
     *            In the event of an error in the caller between calls to
     *            this, an OP_FS_TRANS_CLEANUP call will be made.
     *     size = size of structure
     * Returns error. */
    gxdso_abuf_optrans,

    /* Color replacement method.  Intercepts remap color method(s) to
     *  enable the device map source colors to device colors directly
     *  in a method defined in the device.
     *       data = color_replace_t
     *       size = sizeof(color_replace_t)
     *
     *  Returns 0 is no replacment is made.
     *  Returns >0 if replacement occurred. */
    gxdso_replacecolor,

    /* Used solely (for now at least) by pdfwrite. We need to tell pdfwrite that Image and Form
     * XObjects, have an /OC entry and what the object number of the optional content entry is.
     * We should already have defined the optional content dictionary using a pdfmark.
     */
    gxdso_pending_optional_content,

    /* Called to give a device the chance to update the color information after put_params
     * has reset it. This enables device specific changes to not interfere when devices are
     * cloned for NumRenderingThreads etc. */
    gxdso_adjust_colors,

    /* Called to ask if we are an additive device that would like to support separations
     * natively. Should return the number of additive process colors in use.
     */
    gxdso_is_sep_supporting_additive_device,

    /* Communicate the start of text with a 'clip' render mode, and the grestore
     * which restores it away. We send data of 1 for the start of text in a clipping
     * mode and 0 when we grestore back to a non-clip mode.
     */
    gxdso_hilevel_text_clip,

    /* Add new gxdso_ keys above this. */
    gxdso_pattern__LAST
};

#endif /* gxdevsop_INCLUDED */
