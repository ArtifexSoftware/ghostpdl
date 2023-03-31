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

#include "mex.h"
#include <memory.h>
#include <assert.h>

#ifndef GHOSTPDL
#define GHOSTPDL 1
#endif

#if GHOSTPDL
#include "../../pcl/pl/plapi.h"   /* GSAPI - gpdf version */
#else
#include "psi/iapi.h"       /* GSAPI - ghostscript version */
#endif
#include "../../devices/gdevdsp.h"
/*--------------------------------------------------------------------*/
/* Much of this set-up is stolen from the C-API demo code */

#ifdef HIDE_POINTERS
void *hide_pointer(void *p)
{
    return (p == NULL) ? NULL : (void *)1;
}

#define PTR(p) hide_pointer(p)

#else

#define PTR(p) p

#endif

#define PlanarGray 0x800802
#define PlanarRGB 0x800804
#define PlanarCMYK 0x800808
#define PlanarSpots 0x880800
#define INSTANCE_HANDLE ((void *)1234)
#define SANITY_CHECK_VALUE 0x12345678

#define SANITY_CHECK(ts) assert(ts->sanity_check_value == SANITY_CHECK_VALUE)

/* All the state for a given test is contained within the following
 * structure. */
typedef struct {
    /* This value should always be set to SANITY_CHECK_VALUE. It
     * allows us to check we have a valid (or at least plausible)
     * teststate_t pointer by checking its value. */
    int sanity_check_value;

    int w;
    int h;
    int r;
    int pr;
    int format;

    int n;
    void *mem;

    mxArray **mex_image_data;

} teststate_t;


static int
open(void *handle, void *device)
{
    teststate_t *ts = (teststate_t *)handle;

    SANITY_CHECK(ts);
    mexPrintf("open from C-Mex function\n");

    return 0;
}

static int
preclose(void *handle, void *device)
{
    teststate_t *ts = (teststate_t *)handle;

    SANITY_CHECK(ts);
    mexPrintf("preclose from C-Mex function\n");

    return 0;
}

static int
close(void *handle, void *device)
{
    teststate_t *ts = (teststate_t *)handle;

    SANITY_CHECK(ts);
    mexPrintf("close from C-Mex function\n");

    return 0;
}

static int
presize(void *handle, void *device,
        int width, int height, int raster, unsigned int format)
{
    teststate_t *ts = (teststate_t *)handle;

    SANITY_CHECK(ts);
    mexPrintf("presize: w=%d h=%d r=%d f=%x\n",
           width, height, raster, format);

    ts->w = width;
    ts->h = height;
    ts->r = raster;
    ts->format = format;

    if (ts->format & DISPLAY_COLORS_GRAY)
        ts->n = 1;
    if (ts->format & DISPLAY_COLORS_RGB)
        ts->n = 3;
    if (ts->format & DISPLAY_COLORS_CMYK)
        ts->n = 4;
    if (ts->format & DISPLAY_COLORS_SEPARATION)
        ts->n = 0;
    if ((ts->format & DISPLAY_DEPTH_MASK) != DISPLAY_DEPTH_8)
        return -1; /* Haven't written code for that! */

    return 0;
}

static int
size(void *handle, void *device, int width, int height,
     int raster, unsigned int format, unsigned char *pimage)
{
    teststate_t *ts = (teststate_t *)handle;

    SANITY_CHECK(ts);
    mexPrintf("size: w=%d h=%d r=%d f=%x m=%p\n",
           width, height, raster, format, PTR(pimage));

    ts->w = width;
    ts->h = height;
    ts->r = raster;
    ts->format = format;
    ts->mem = pimage;

    if (ts->format & DISPLAY_PLANAR)
        ts->pr = ts->r * height;
    /* When running with spots, n is not known yet. */
    if (ts->n != 0 && ts->format & DISPLAY_PLANAR_INTERLEAVED)
        ts->pr = ts->r / ts->n;

    return 0;
}

static int
sync(void *handle, void *device)
{
    teststate_t *ts = (teststate_t *)handle;

    SANITY_CHECK(ts);

    mexPrintf("sync\n");

    return 0;
}

static int
page(void *handle, void *device, int copies, int flush)
{
    teststate_t *ts = (teststate_t *)handle;
    mwSize dims[3];
    int num_planes;
    int i,j,k;
    unsigned char *des_ptr, *src_ptr, *matlab_ptr, *row_ptr;
    size_t matlab_planar_raster;

    SANITY_CHECK(ts);

    mexPrintf("page: c=%d f=%d\n", copies, flush);

    /* Transfer to the mex output variables at this time */
    switch (ts->format) {
    case PlanarGray:
        num_planes = 1;
        break;
    case PlanarRGB:
        num_planes = 3;
        break;
    case PlanarCMYK:
        num_planes = 4;
        break;
    case PlanarSpots:
        num_planes = 4;
        break;
    }

    /* Matlab uses a column ordered layout for its storage of
       arrays. So we need to do a little effort here. */
    /* Allocate MATLAB outputs */
    dims[0] = ts->h;
    dims[1] = ts->w;
    dims[2] = num_planes;
    *(ts->mex_image_data) = mxCreateNumericArray(3, dims, mxUINT8_CLASS, mxREAL);

    /* Grab image pointer */
    matlab_ptr = (unsigned char*) mxGetUint8s(*(ts->mex_image_data));
    matlab_planar_raster = ts->w * ts->h;

    /* Copy to MATLAB memory */
    for (k = 0; k < num_planes; k++) {
        des_ptr = matlab_ptr +  k * matlab_planar_raster;
        src_ptr = (unsigned char*) ts->mem + k * ts->pr;
        row_ptr = src_ptr;
        for (i = 0; i < ts->h; i++) {
            for (j = 0; j < ts->w; j++) {
                des_ptr[i + j * ts->h] = *row_ptr++;
            }
            row_ptr = src_ptr + i * ts->r;
        }
    }
    return 0;
}

static int
update(void *handle, void *device, int x, int y, int w, int h)
{
    teststate_t *ts = (teststate_t *)handle;

    SANITY_CHECK(ts);
    /* This print statement just makes too much noise :) */
    /* mexPrintf("update: x=%d y=%d w=%d h=%d\n", x, y, w, h); */

    return 0;
}

static void *
memalloc(void *handle, void *device, size_t size)
{
    void *ret = NULL;
    teststate_t *ts = (teststate_t *)handle;

    SANITY_CHECK(ts);

    ret = malloc(size);

    return ret;
}

static int
memfree(void *handle, void *device, void *mem)
{
    teststate_t *ts = (teststate_t *)handle;

    SANITY_CHECK(ts);
    mexPrintf("memfree: %p\n", PTR(mem));

    free(mem);

    return 0;
}

static int
separation(void *handle, void *device,
           int component, const char *component_name,
           unsigned short c, unsigned short m,
           unsigned short y, unsigned short k)
{
    teststate_t *ts = (teststate_t *)handle;

    SANITY_CHECK(ts);

    mexPrintf("separation: %d %s (%x,%x,%x,%x)\n",
           component, component_name ? component_name : "<NULL>",
           c, m, y, k);
    ts->n++;

    /* Update the plane_raster as n has changed. */
    if (ts->format & DISPLAY_PLANAR_INTERLEAVED)
        ts->pr = ts->r / ts->n;

    return 0;
}

static int
adjust_band_height(void *handle, void *device, int bandheight)
{
    teststate_t *ts = (teststate_t *)handle;

    SANITY_CHECK(ts);
    mexPrintf("adjust_band_height: Unsupported!");

    return 0;
}

static int
rectangle_request(void *handle, void *device,
                  void **memory, int *ox, int *oy,
                  int *raster, int *plane_raster,
                  int *x, int *y, int *w, int *h)
{
    teststate_t *ts = (teststate_t *)handle;

    SANITY_CHECK(ts);

    mexPrintf("rectangle_request: Unsupported!");
    return 0;
}

/*--------------------------------------------------------------------*/
/* All those callback functions live in a display_callback structure
 * that we return to the main code. This can be done using the modern
 * "callout" method, or by using the legacy (deprecated) direct
 * registration method. We strongly prefer the callout method as it
 * avoids the need to pass a pointer using -sDisplayHandle. */
static display_callback callbacks =
{
    sizeof(callbacks),
    DISPLAY_VERSION_MAJOR,
    DISPLAY_VERSION_MINOR,
    open,
    preclose,
    close,
    presize,
    size,
    sync,
    page,
    update,
    memalloc,
    memfree,
    separation,
    adjust_band_height,
    rectangle_request
};

/*--------------------------------------------------------------------*/
/* This is our callout handler. It handles callouts from devices within
 * Ghostscript. It only handles a single callout, from the display
 * device, to return the callback handler and callback handle. */
static int
callout(void *instance,
        void *callout_handle,
        const char *device_name,
        int id,
        int size,
        void *data)
{
    teststate_t *ts = (teststate_t *)callout_handle;

    SANITY_CHECK(ts);

    /* We are only interested in callouts from the display device. */
    if (strcmp(device_name, "display"))
        return -1;

    if (id == DISPLAY_CALLOUT_GET_CALLBACK)
    {
        /* Fill in the supplied block with the details of our callback
         * handler, and the handle to use. In this instance, the handle
         * is the pointer to our test structure. */
        gs_display_get_callback_t *cb = (gs_display_get_callback_t *)data;
        cb->callback = &callbacks;
        cb->caller_handle = ts;
        return 0;
    }
    return -1;
}

/* mexFunction is the gateway routine for the MEX-file.  Like the main
   function in a C project */
void
mexFunction( int nlhs, mxArray *plhs[],
             int nrhs, const mxArray *prhs[] )
{
    /* Set up GS to use the display device. Register the callback
       which will callback to a MATLAB function, whose task it is
       to deal with the rendered image data */
    /* Make the teststate a blank slate for us to work with. */
    teststate_t teststate = { SANITY_CHECK_VALUE };

    /* Construct the argc/argv to pass to ghostscript. */
    int argc = 0;
    char *argv[10];
    char format_arg[64];
    char first_page_arg[64];
    char last_page_arg[64];
    char resolution_arg[64];
    int code = 0;
    void *instance = NULL;
    int format, page_number;
    size_t m, n;
    mxDouble *value;
    int result;
    char *file_name = NULL;
    int len;
    int resolution;

    /* Error checking */
    if (nrhs != 4) {
        mexPrintf("Incorrect number of input args");
        return;
    }

    if (mxGetClassID(prhs[0]) != mxCHAR_CLASS || mxIsComplex(prhs[0]) ||
        mxGetM(prhs[0]) != 1) {
        mexErrMsgTxt("First input must be file name");
        return;
    }
    len = mxGetN(prhs[0]);
    file_name = (char *)malloc(len + 1);
    if (file_name == NULL) {
        mexErrMsgTxt("Filename allocation failed");
        return;
    }
    code = mxGetString(prhs[0], file_name, len + 1);
    if (code != 0) {
        mexErrMsgTxt("Mex string copy failed");
        return;
    }

    if (mxGetClassID(prhs[1]) != mxUINT32_CLASS || mxIsComplex(prhs[1]) ||
        mxGetN(prhs[1]) * mxGetM(prhs[1]) != 1) {
        mexErrMsgTxt("Second input must be data format");
        return;
    }
    format = mxGetScalar(prhs[1]);

    if (!mxIsDouble(prhs[2]) || mxIsComplex(prhs[2]) ||
        mxGetN(prhs[2]) * mxGetM(prhs[2]) != 1) {
        mexErrMsgTxt("Third input must be page number");
        return;
    }
    page_number = mxGetScalar(prhs[2]);

    if (!mxIsDouble(prhs[3]) || mxIsComplex(prhs[3]) ||
        mxGetN(prhs[3]) * mxGetM(prhs[3]) != 1) {
        mexErrMsgTxt("Fourth input must be resolution");
        return;
    }
    resolution = mxGetScalar(prhs[3]);

    if (!(format == PlanarGray || format == PlanarRGB ||
        format == PlanarCMYK || format == PlanarSpots)) {
        mexPrintf("Data format must be planar");
        return;
    }

    if (nlhs != 1) {
        mexPrintf("Incorrect number of output args");
        return;
    }

    /* Set up command */
    argv[argc++] = "gs";
    argv[argc++] = "-sDEVICE=display";  /* Using display device to get callback*/
    argv[argc++] = "-dNOPAUSE";
    argv[argc++] = first_page_arg;
    argv[argc++] = last_page_arg;
    argv[argc++] = format_arg;
    argv[argc++] = resolution_arg;

    sprintf(first_page_arg, "-dFirstPage=%d", page_number);
    sprintf(last_page_arg, "-dLastPage=%d", page_number);
    sprintf(format_arg, "-dDisplayFormat=16#%x", format);
    sprintf(resolution_arg, "-r%d", resolution);

    argv[argc++] = file_name;

    /* Create a GS instance. */
    code = gsapi_new_instance(&instance, INSTANCE_HANDLE);
    if (code < 0) {
        mexPrintf("Error %d in gsapi_new_instance\n", code);
        return;
    }

    /* Assign lhs variables to teststate members */
    teststate.mex_image_data = &plhs[0];

    /* Register our callout handler. This will pass the display
     * device the callback structure and handle when requested. */
    code = gsapi_register_callout(instance, callout, &teststate);
    if (code < 0) {
        mexPrintf("Error %d in gsapi_register_callout\n", code);
        goto fail;
    }

    code = gsapi_init_with_args(instance, argc, argv);
    if (code < 0) {
        mexPrintf("Error %d in gsapi_init_with_args\n", code);
        goto fail;
    }

    /* Close the interpreter down (important, or we will leak!) */
    code = gsapi_exit(instance);
    if (code < 0) {
        mexPrintf("Error %d in gsapi_exit\n", code);
    }

fail:
    /* Delete the gs instance. */
    gsapi_delete_instance(instance);

    if (file_name != NULL)
        free(file_name);
}
