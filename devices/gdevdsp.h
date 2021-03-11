/* Copyright (C) 2001-2021 Artifex Software, Inc.
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

/* gdevdsp.h - callback structure for DLL based display device */

#ifndef gdevdsp_INCLUDED
#  define gdevdsp_INCLUDED

/*
 * There are 2 mechanisms to provide the callback structure to
 * Ghostscript. A legacy one, and a modern one. The legacy one is
 * deprecated and should not be used in new code - at some point it may
 * be removed.
 *
 * Modern method: Call the Ghostscript APIs in the following order:
 *  gsapi_new_instance(&minst);
 *  gsapi_register_callout(minst, callout, callout_handle);
 *  gsapi_init_with_args(minst, argc, argv);
 *
 * The callout handler should look for a callout from the 'display'
 * device, with id=DISPLAY_CALLOUT_GET_CALLBACK and respond by filling
 * in the supplied gs_display_get_callback_t structure.
 *
 * Supported parameters and default values are:
 * -dDisplayFormat=0                      long
 *    Color format specified using bitfields below.
 *    Included as argument of display_size() and display_presize()
 * These can only be changed when the device is closed.
 *
 * Legacy method: Call the Ghostscript APIs in the following order:
 *  gsapi_new_instance(&minst);
 *  gsapi_set_display_callback(minst, callback);
 *  gsapi_init_with_args(minst, argc, argv);
 *
 * An additional parameter is supported, with default value NULL:
 * -sDisplayHandle=16#04d2 or 1234        string
 *    Caller supplied handle as a decimal or hexadecimal number
 *    in a string.  On 32-bit platforms, it may be set
 *    using -dDisplayHandle=1234 for backward compatibility.
 *    Included as first parameter of all callback functions.
 *
 * The second parameter of all callback functions "void *device"
 * is the address of the Ghostscript display device instance.
 * The arguments "void *handle" and "void *device" together
 * uniquely identify an instance of the display device.
 *
 * A typical sequence of callbacks (when running without a
 * display_choose_mode) would be:
 *  open, presize, memalloc, size, sync, page
 *  presize, memfree, memalloc, size, sync, page
 *  preclose, memfree, close
 * The caller should not access the image buffer:
 *  - before the first sync
 *  - between presize and size
 *  - after preclose
 * If opening the device fails, you might see the following:
 *  open, presize, memalloc, memfree, close
 *
 * A typical sequence of callbacks (when running with a
 * display_choose_mode) will depend upon whether display_choose_mode
 * selects pagemode or request-rectangle mode:
 *
 * In the pagemode case:
 *  open, presize, display_choose_mode, memalloc, size, sync, page
 *  presize, display_choose_mode, memfree, memalloc, size, sync, page
 *  preclose, memfree, close
 * The caller should not access the image buffer:
 *  - before the first sync
 *  - between presize and size
 *  - after preclose
 * If opening the device fails, you might see the following:
 *  open, presize, memalloc, memfree, close
 *
 * In the request-rectangle mode:
 *  open, presize, display_choose_mode, {rectangle_request}*
 *  presize, display_choose_mode, {rectangle_request}*
 *  preclose, close
 *
 * In a run that mixed request-rectangle and pagemode:
 *  open, presize, display_choose_mode, memalloc, size, sync, page
 *  presize, display_choose_mode, memfree, {rectangle_request}*
 *  presize, display_choose_mode, {rectangle_request}*
 *  presize, display_choose_mode, memalloc, size, sync, page
 *  preclose, memfree, close
 */

#define DISPLAY_VERSION_MAJOR 3
#define DISPLAY_VERSION_MINOR 0

#define DISPLAY_VERSION_MAJOR_V1 1 /* before separation format was added */
#define DISPLAY_VERSION_MINOR_V1 0

#define DISPLAY_VERSION_MAJOR_V2 2 /* before planar and banding were added */
#define DISPLAY_VERSION_MINOR_V2 0

/* The display format is set by a combination of the following bitfields */

/* Define the color space alternatives */
typedef enum {
    DISPLAY_COLORS_NATIVE	 = (1<<0),
    DISPLAY_COLORS_GRAY  	 = (1<<1),
    DISPLAY_COLORS_RGB   	 = (1<<2),
    DISPLAY_COLORS_CMYK  	 = (1<<3),
    DISPLAY_COLORS_SEPARATION    = (1<<19)
} DISPLAY_FORMAT_COLOR;
#define DISPLAY_COLORS_MASK 0x8000fL

/* Define whether alpha information, or an extra unused bytes is included */
/* DISPLAY_ALPHA_FIRST and DISPLAY_ALPHA_LAST are not implemented */
typedef enum {
    DISPLAY_ALPHA_NONE   = (0<<4),
    DISPLAY_ALPHA_FIRST  = (1<<4),
    DISPLAY_ALPHA_LAST   = (1<<5),
    DISPLAY_UNUSED_FIRST = (1<<6),	/* e.g. Mac xRGB */
    DISPLAY_UNUSED_LAST  = (1<<7)	/* e.g. Windows BGRx */
} DISPLAY_FORMAT_ALPHA;
#define DISPLAY_ALPHA_MASK 0x00f0L

/* Define the depth per component for DISPLAY_COLORS_GRAY,
 * DISPLAY_COLORS_RGB and DISPLAY_COLORS_CMYK,
 * or the depth per pixel for DISPLAY_COLORS_NATIVE
 * DISPLAY_DEPTH_2 and DISPLAY_DEPTH_12 have not been tested.
 */
typedef enum {
    DISPLAY_DEPTH_1   = (1<<8),
    DISPLAY_DEPTH_2   = (1<<9),
    DISPLAY_DEPTH_4   = (1<<10),
    DISPLAY_DEPTH_8   = (1<<11),
    DISPLAY_DEPTH_12  = (1<<12),
    DISPLAY_DEPTH_16  = (1<<13)
    /* unused (1<<14) */
    /* unused (1<<15) */
} DISPLAY_FORMAT_DEPTH;
#define DISPLAY_DEPTH_MASK 0xff00L

/* Define whether Red/Cyan should come first,
 * or whether Blue/Black should come first
 */
typedef enum {
    DISPLAY_BIGENDIAN    = (0<<16),	/* Red/Cyan first */
    DISPLAY_LITTLEENDIAN = (1<<16)	/* Blue/Black first */
} DISPLAY_FORMAT_ENDIAN;
#define DISPLAY_ENDIAN_MASK 0x00010000L

/* Define whether the raster starts at the top or bottom of the bitmap */
typedef enum {
    DISPLAY_TOPFIRST    = (0<<17),	/* Unix, Mac */
    DISPLAY_BOTTOMFIRST = (1<<17)	/* Windows */
} DISPLAY_FORMAT_FIRSTROW;
#define DISPLAY_FIRSTROW_MASK 0x00020000L

/* Define whether packing RGB in 16-bits should use 555
 * or 565 (extra bit for green)
 */
typedef enum {
    DISPLAY_NATIVE_555 = (0<<18),
    DISPLAY_NATIVE_565 = (1<<18)
} DISPLAY_FORMAT_555;
#define DISPLAY_555_MASK 0x00040000L

/* Define the row alignment, which must be equal to or greater than
 * the size of a pointer.
 * The default (DISPLAY_ROW_ALIGN_DEFAULT) is the size of a pointer,
 * 4 bytes (DISPLAY_ROW_ALIGN_4) on 32-bit systems or 8 bytes
 * (DISPLAY_ROW_ALIGN_8) on 64-bit systems.
 */
typedef enum {
    DISPLAY_ROW_ALIGN_DEFAULT = (0<<20),
    /* DISPLAY_ROW_ALIGN_1 = (1<<20), */ /* not currently possible */
    /* DISPLAY_ROW_ALIGN_2 = (2<<20), */ /* not currently possible */
    DISPLAY_ROW_ALIGN_4 = (3<<20),
    DISPLAY_ROW_ALIGN_8 = (4<<20),
    DISPLAY_ROW_ALIGN_16 = (5<<20),
    DISPLAY_ROW_ALIGN_32 = (6<<20),
    DISPLAY_ROW_ALIGN_64 = (7<<20)
} DISPLAY_FORMAT_ROW_ALIGN;
#define DISPLAY_ROW_ALIGN_MASK 0x00700000L

/* Define whether we are using chunky, planar or planar interleaved
 * representation. */
typedef enum {
    DISPLAY_CHUNKY             = (0<<23),
    DISPLAY_PLANAR             = (1<<23),
    DISPLAY_PLANAR_INTERLEAVED = (2<<23),
} DISPLAY_FORMAT_PLANARNESS;

#ifndef display_callback_DEFINED
#define display_callback_DEFINED
typedef struct display_callback_s display_callback;
#endif

/*
 * Note that for Windows, the display callback functions are
 * cdecl, not stdcall.  This differs from those in iapi.h.
 */

struct display_callback_s {
    /* Size of this structure */
    /* Used for checking if we have been handed a valid structure */
    int size;

    /* Major version of this structure  */
    /* The major version number will change if this structure changes. */
    int version_major;

    /* Minor version of this structure */
    /* The minor version number will change if new features are added
     * without changes to this structure.  For example, a new color
     * format.
     */
    int version_minor;

    /* New device has been opened */
    /* This is the first event from this device. */
    int (*display_open)(void *handle, void *device);

    /* Device is about to be closed. */
    /* Device will not be closed until this function returns. */
    int (*display_preclose)(void *handle, void *device);

    /* Device has been closed. */
    /* This is the last event from this device. */
    int (*display_close)(void *handle, void *device);

    /* Device is about to be resized. */
    /* Resize will only occur if this function returns 0. */
    /* raster is byte count of a row. */
    int (*display_presize)(void *handle, void *device,
        int width, int height, int raster, unsigned int format);

    /* Device has been resized. */
    /* New pointer to raster returned in pimage */
    int (*display_size)(void *handle, void *device, int width, int height,
        int raster, unsigned int format, unsigned char *pimage);

    /* flushpage */
    int (*display_sync)(void *handle, void *device);

    /* showpage */
    /* If you want to pause on showpage, then don't return immediately */
    int (*display_page)(void *handle, void *device, int copies, int flush);

    /* Notify the caller whenever a portion of the raster is updated. */
    /* This can be used for cooperative multitasking or for
     * progressive update of the display.
     * This function pointer may be set to NULL if not required.
     * NOTE: This is actually a really bad thing to work on. It may well
     * end up not being called back at all during the rendering process,
     * in particular if transparency is in use, or if rectangle request
     * mode is used.
     */
    int (*display_update)(void *handle, void *device, int x, int y,
        int w, int h);

    /* Allocate memory for bitmap */
    /* This is provided in case you need to create memory in a special
     * way, e.g. shared.  If this is NULL, the Ghostscript memory device
     * allocates the bitmap. This will only called to allocate the
     * image buffer. The first row will be placed at the address
     * returned by display_memalloc.
     *
     * In the event of this callback returning NULL, Ghostscript will
     * look for a display_rectangle_request callback. If one is not
     * supplied, then this will be reported as memory exhaustion. If
     * one is supplied, then Ghostscript will switch to working in
     * rectangle request mode.
     */
    void *(*display_memalloc)(void *handle, void *device, size_t size);

    /* Free memory for bitmap */
    /* If this is NULL, the Ghostscript memory device will free the bitmap */
    int (*display_memfree)(void *handle, void *device, void *mem);

    /* Added in V2 */
    /* When using separation color space (DISPLAY_COLORS_SEPARATION),
     * give a mapping for one separation component.
     * This is called for each new component found.
     * It may be called multiple times for each component.
     * It may be called at any time between display_size
     * and display_close.
     * The client uses this to map from the separations to CMYK
     * and hence to RGB for display.
     * GS must only use this callback if version_major >= 2.
     * The unsigned short c,m,y,k values are 65535 = 1.0.
     * This function pointer may be set to NULL if not required.
     */
    int (*display_separation)(void *handle, void *device,
        int component, const char *component_name,
        unsigned short c, unsigned short m,
        unsigned short y, unsigned short k);

    /* Added in V3 */
    /* If non NULL, then this gives the callback provider a chance to
     * a) be informed of and b) control the bandheight used by the
     * display device. If a call to allocate the page mode bitmap fails
     * (either an internal allocation or a display_memalloc call), then
     * Ghostscript will look for the presence of a
     * display_rectangle_request callback. If it exists, then it will
     * attempt to use retangle request mode.
     *
     * As part of this, it will pick an appropriate bandheight. If
     * this callback exists, it will be called so the callback provider
     * can know (and, optionally, tweak) the bandheight to be used.
     * This is purely for performance. The callback should only ever
     * *reduce* the bandheight given here.
     *
     * Return the adjusted bandheight (or 0 for no change).
     */
    int (*display_adjust_band_height)(void *handle, void *device,
                                      int bandheight);

    /* Ask the callback for a rectangle to render (and a block to render
     * it in). Each subsequent call tells the caller that any previous
     * call has finished. To signal 'no more rectangles' return with
     * *w or *h = 0.
     *
     * On entry: *raster and *plane_raster are set to the standard
     *   values. All other values are undefined.
     * On return: *memory should point to a block of memory to use.
     *   Pixel (*ox,*oy) is the first pixel represented in that block.
     *   *raster = the number of bytes difference between the address of
     *   component 0 of Pixel(*ox,*oy) and the address of component 0 of
     *   Pixel(*ox,1+*oy).
     *   *plane_raster = the number of bytes difference between the
     *   address of component 0 of Pixel(*ox,*oy) and the address of
     *   component 1 of Pixel(*ox,*oy), if in planar mode, 0 otherwise.
     *   *x, *y, *w, *h = rectangle requested within that memory block.
     *
     */
    int (*display_rectangle_request)(void *handle, void *device,
                                     void **memory, int *ox, int *oy,
                                     int *raster, int *plane_raster,
                                     int *x, int *y, int *w, int *h);
};

/* This is the V2 structure, before banding and planar support was added */
struct display_callback_v2_s {
    int size; /* sizeof(struct display_callback_v2) */
    int version_major; /* DISPLAY_VERSION_MAJOR_V2 */
    int version_minor; /* DISPLAY_VERSION_MINOR_V2 */
    int (*display_open)(void *handle, void *device);
    int (*display_preclose)(void *handle, void *device);
    int (*display_close)(void *handle, void *device);
    int (*display_presize)(void *handle, void *device,
        int width, int height, int raster, unsigned int format);
    int (*display_size)(void *handle, void *device, int width, int height,
        int raster, unsigned int format, unsigned char *pimage);
    int (*display_sync)(void *handle, void *device);
    int (*display_page)(void *handle, void *device, int copies, int flush);
    int (*display_update)(void *handle, void *device, int x, int y,
        int w, int h);
    void *(*display_memalloc)(void *handle, void *device, unsigned long size);
    int (*display_memfree)(void *handle, void *device, void *mem);
    int (*display_separation)(void *handle, void *device,
        int component, const char *component_name,
        unsigned short c, unsigned short m,
        unsigned short y, unsigned short k);
};

/* This is the V1 structure, before separation format was added */
struct display_callback_v1_s {
    int size; /* sizeof(struct display_callback_v1) */
    int version_major; /* DISPLAY_VERSION_MAJOR_V1 */
    int version_minor; /* DISPLAY_VERSION_MINOR_V1 */
    int (*display_open)(void *handle, void *device);
    int (*display_preclose)(void *handle, void *device);
    int (*display_close)(void *handle, void *device);
    int (*display_presize)(void *handle, void *device,
        int width, int height, int raster, unsigned int format);
    int (*display_size)(void *handle, void *device, int width, int height,
        int raster, unsigned int format, unsigned char *pimage);
    int (*display_sync)(void *handle, void *device);
    int (*display_page)(void *handle, void *device, int copies, int flush);
    int (*display_update)(void *handle, void *device, int x, int y,
        int w, int h);
    void *(*display_memalloc)(void *handle, void *device, unsigned long size);
    int (*display_memfree)(void *handle, void *device, void *mem);
};

#define DISPLAY_CALLBACK_V1_SIZEOF sizeof(struct display_callback_v1_s)

#define DISPLAY_CALLOUT_GET_CALLBACK 0
#define DISPLAY_CALLOUT_GET_CALLBACK_LEGACY 1

typedef struct {
    display_callback *callback;
    void *caller_handle;
} gs_display_get_callback_t;

/* The display device calls a callout to find the callback structure
 * and caller_handle from the environment (the DLL caller/user of the
 * API).
 * It passes:
 *   id = DISPLAY_CALLOUT_GET_CALLBACK.
 *   size = sizeof(gs_display_get_callback_t) (or larger);
 *   data = pointer to gs_display_get_callback_t instance for callout
 *          handler to fill in.
 *
 * In order to support the old gsapi_set_display_callback we have a
 * related callout, DISPLAY_CALLOUT_GET_CALLBACK_LEGACY. Do not use
 * this!
 */


#endif /* gdevdsp_INCLUDED */
