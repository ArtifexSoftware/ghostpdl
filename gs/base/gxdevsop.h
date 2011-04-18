/* Copyright (C) 2001-2011 Artifex Software, Inc.
   All Rights Reserved.
  
   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

/* $Id$ */
/* Definitions for device specific operations */

#ifndef gxdevsop_INCLUDED
#  define gxdevsop_INCLUDED

/* This file enumerates a series of device specific operations, that can be
 * performed using the 'dev_spec_op' procedure in the gs_device structure.
 *
 * This scheme, as its name suggests, allows clients to make device specific
 * calls to any given device in a safe way. This is to gs_devices what an
 * ioctl is to unix file handles.
 *
 * Design features of this scheme ensure that:
 *  * Devices that support a given call call efficiently implement both
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
     *     data = NULL
     *     size = 0
     * Returns 1 if the device is a pdf14 device .
     */
    gxdso_is_pdf14_device,

    /* gxdso_is_native_planar:
     *      data = NULL
     *      size = 0
     * Returns 1 if the device's native format is planar
     */
     gxdso_is_native_planar,

    /* Add new gxdso_ keys above this. */
    gxdso_pattern__LAST
};

#endif /* gxdevsop_INCLUDED */
