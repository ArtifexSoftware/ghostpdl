/* Copyright (C) 1999 Aladdin Enterprises.  All rights reserved.
  
  This software is provided AS-IS with no warranty, either express or
  implied.
  
  This software is distributed under license and may not be copied,
  modified or distributed except as expressly authorized under the terms
  of the license contained in the file LICENSE in this distribution.
  
  For more information about licensing, please refer to
  http://www.ghostscript.com/licensing/. For information on
  commercial licensing, go to http://www.artifex.com/licensing/ or
  contact Artifex Software, Inc., 101 Lucas Valley Road #110,
  San Rafael, CA  94903, U.S.A., +1(415)492-9861.
*/

/* $Id$ */
/* IMDI Device.
 *
 * This is an RGB contone device, that outputs the raster
 * converted to CMYK using CMM API
 */

/* TODO: this should be configurable */
#define LINK_ICC_NAME	"link.icc"

#include "errno_.h"
#include "string_.h"

#include "gserror.h"
#include "gdevprn.h"
#include "gxfrac.h"
#include "gscms.h"
#include "gsicc_cache.h"
#include "gsicc_manage.h"

#ifndef cmm_gcmmhlink_DEFINED
    #define cmm_gcmmhlink_DEFINED
    typedef void* gcmmhlink_t;
#endif

#ifndef cmm_gcmmhprofile_DEFINED
    #define cmm_gcmmhprofile_DEFINED
    typedef void* gcmmhprofile_t;
#endif
/*
 * Set up the device structures and function tables.
 */

#ifndef X_DPI
#  define X_DPI 72
#endif
#ifndef Y_DPI
#  define Y_DPI 72
#endif

typedef struct gx_device_imdi_s gx_device_imdi;

static dev_proc_open_device(imdi_open_device);
static dev_proc_close_device(imdi_close_device);
static dev_proc_print_page(imdi_print_page);

struct gx_device_imdi_s
{
    gx_device_common;
    gx_prn_device_common;

    gcmmhlink_t icc_link;
    cmm_profile_t *icc_link_profile;

};

static const gx_device_procs imdi_procs =
{
    imdi_open_device, NULL, NULL, gdev_prn_output_page, imdi_close_device,
    gx_default_rgb_map_rgb_color, gx_default_rgb_map_color_rgb,
    NULL, NULL, NULL, NULL, NULL, NULL,
    gdev_prn_get_params, gdev_prn_put_params,
    NULL, NULL, NULL, NULL, gx_page_device_get_page_device
};

const gx_device_imdi gs_imdi_device =
{
    prn_device_body(gx_device_imdi, imdi_procs, "imdi",
	    DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
	    X_DPI, Y_DPI,
	    0, 0, 0, 0,	/* Margins */
	    3, 24, 255, 255, 256, 256, imdi_print_page)
};

static double incurve(void *ctx, int ch, double val)
{
    return val;
}

static double outcurve(void *ctx, int ch, double val)
{
    return val;
}

/*
 * Open IMDI device.
 * Load ICC device link profile (to map sRGB to FOGRA CMYK).
 */

static int
imdi_open_device(gx_device *dev)
{
    gx_device_imdi *idev = (gx_device_imdi*)dev;
    gsicc_rendering_param_t rendering_params;

    /* Open and read profile */

    idev->icc_link_profile = gsicc_get_profile_handle_file(LINK_ICC_NAME, 
                    strlen(LINK_ICC_NAME), dev->memory);

    if (idev->icc_link_profile == NULL)
        return gs_throw(-1, "Could not create link profile for imdi device");

    if (idev->icc_link_profile->num_comps != 3)
	return gs_throw1(-1, "profile must have 3 input channels. got %d.", 
                    idev->icc_link_profile->num_comps);
    if (idev->icc_link_profile->num_comps_out != 4)
	return gs_throw1(-1, "profile must have 4 output channels. got %d.", 
                    idev->icc_link_profile->num_comps_out);


    rendering_params.black_point_comp = false;
    rendering_params.object_type = GS_DEVICE_DOESNT_SUPPORT_TAGS;  /* Already rendered */
    rendering_params.rendering_intent = gsPERCEPTUAL;
    
    idev->icc_link = gscms_get_link(idev->icc_link_profile, 
                    NULL, &rendering_params);

    if (!idev->icc_link)
	return gs_throw(-1, "could not create ICC link handle");

    return gdev_prn_open(dev);

}


/*
 * Close device and clean up ICC structures.
 */

static int
imdi_close_device(gx_device *dev)
{
    gx_device_imdi *idev = (gx_device_imdi*)dev;

    gscms_release_link(idev->icc_link);
    rc_decrement(idev->icc_link_profile, "imdi_close_device");

    return gdev_prn_close(dev);
}


/*
 * Output the page raster.
 */

static int
imdi_print_page(gx_device_printer *pdev, FILE *prn_stream)
{
    gx_device_imdi *idev = (gx_device_imdi*)pdev;

    gsicc_bufferdesc_t input_buff_desc;
    gsicc_bufferdesc_t output_buff_desc;

    byte *srcbuffer = 0;
    byte *dstbuffer = 0;
    FILE *fp[4] = {0};

    int srcstride;
    int dststride;
    int srcplanes;
    int dstplanes;

    byte *srcdata;

    int code = 0;
    int x, y, k;

    int nsame = 0, lsame = 0;
    int ndiff = 0, ldiff = 0;

    /*
     * Open auxiliary CMYK files.
     */

    fprintf(prn_stream, "P6\n%d %d\n255\n", pdev->width, pdev->height);

    for (k = 0; k < 4; k++)
    {
	char name[256];

	sprintf(name, "%s.%c.pgm", pdev->fname, "cmyk"[k]);

	dprintf1("output file: %s\n", name);

	fp[k] = fopen(name, "wb");
	if (!fp[k])
	{
	    code = gs_throw2(-1, "could not open file: %s (%s)", name, strerror(errno));
	    goto cleanup;
	}

	fprintf(fp[k], "P5\n%d %d\n255\n", pdev->width, pdev->height);
    }

    /*
     * Allocate scratch buffers.
     */

    srcplanes = pdev->color_info.num_components;
    srcstride = gdev_mem_bytes_per_scan_line((gx_device*)pdev);
    srcbuffer = gs_malloc(pdev->memory, srcstride, 1, "imdi_print_page(srcbuffer)");
    if (!srcbuffer)
    {
	code = gs_throw1(-1, "outofmem: src buffer %d", srcstride);
	goto cleanup;
    }

    dstplanes = 4;
    dststride = dstplanes * pdev->width;
    dstbuffer = gs_malloc(pdev->memory, dststride, 1, "imdi_print_page(dstbuffer)");
    if (!dstbuffer)
    {
	code = gs_throw1(-1, "outofmem: dst buffer %d", dststride);
	goto cleanup;
    }

    /*
     * Extract pixels, convert colors and write data to files.
     */

    for (y = 0; y < pdev->height; y++)
    {
	gdev_prn_get_bits(pdev, y, srcbuffer, &srcdata);

	/* write rgb to original output file */
	fwrite(srcdata, 1, srcstride, prn_stream);

#if 1 /* Collect runlengths */

	{
	void *inp[1];
	void *outp[1];
	int sx, ex;
	int w = pdev->width;

	sx = ex = 0;

	while (sx < w)
	{
	    inp[0] = srcdata + sx * 3;
	    outp[0] = dstbuffer + sx * 4;

	    while (ex < w && 
		srcdata[ex * 3 + 0] == srcdata[sx * 3 + 0] &&
		srcdata[ex * 3 + 1] == srcdata[sx * 3 + 1] &&
		srcdata[ex * 3 + 2] == srcdata[sx * 3 + 2])
	    {
		ex ++;
	    }

	    /* same-run */
	    if (ex - sx > 1)
	    {
		nsame ++; lsame += ex - sx;

                /* Transform the color */
                gscms_transform_color(idev->icc_link, inp,
                        outp, 1, NULL);

		for (x = sx + 1; x < ex; x++)
		{
		    dstbuffer[x * 4 + 0] = dstbuffer[sx * 4 + 0];
		    dstbuffer[x * 4 + 1] = dstbuffer[sx * 4 + 1];
		    dstbuffer[x * 4 + 2] = dstbuffer[sx * 4 + 2];
		    dstbuffer[x * 4 + 3] = dstbuffer[sx * 4 + 3];
		}
	    }

	    /* diff-run */
	    else
	    {
		ndiff ++;

		while (ex < w && 
			srcdata[ex * 3 + 0] != srcdata[ex * 3 - 3] &&
			srcdata[ex * 3 + 1] != srcdata[ex * 3 - 2] &&
			srcdata[ex * 3 + 2] != srcdata[ex * 3 - 1])
		{
		    ex ++;
		}

		ldiff += ex - sx;

                /* This needs to be done more efficiently */

                gsicc_init_buffer(&input_buff_desc, 3, 1,
                      false, false, false, 0, (ex - sx)*3,
                      1, ex - sx);

                gsicc_init_buffer(&output_buff_desc, 4, 1,
                      false, false, false, 0, (ex - sx)*4,
                      1, ex - sx);

                gscms_transform_color_buffer(idev->icc_link, &input_buff_desc,
                             &output_buff_desc, inp, outp);

	    }

	    sx = ex;
	}
	}

#endif

#if 0 /* Call IMDI for entire scanline */
	void *inp[1];
	void *outp[1];

	inp[0] = srcdata;
	outp[0] = dstbuffer;

        gsicc_init_buffer(&input_buff_desc, 3, 1,
              false, false, false, 0, width*3,
              1, width);

        gsicc_init_buffer(&output_buff_desc, 4, 1,
              false, false, false, 0, width*4,
              1, width);

        gscms_transform_color_buffer(idev->icc_link, &input_buff_desc,
                     &output_buff_desc, inp, outp);



#if 0
	/* output planar data to auxiliary output files */
	for (x = 0; x < pdev->width; x++)
	    for (k = 0; k < 4; k++)
		putc(dstbuffer[x * 4 + k], fp[k]);
#endif
#endif

#if 0 /* Call IMDI for every pixel */
	for (x = 0; x < pdev->width; x++)
	{
	    void *inp[1];
	    void *outp[1];

	    inp[0] = srcdata + x * 3;
	    outp[0] = dstbuffer + x * 4;

            /* Transform the color */
            gscms_transform_color(idev->icc_link, inp,
                    outp, 1, NULL);


	    /* output planar data to auxiliary output files */
	    for (k = 0; k < 4; k++)
		putc(dstbuffer[x * 4 + k], fp[k]);
	}
#endif

#if 0 /* Slow but accurate every pixel */

	for (x = 0; x < pdev->width; x++)
	{
	    srcpixel[0] = srcdata[x * 3 + 0] / 255.0;
	    srcpixel[1] = srcdata[x * 3 + 1] / 255.0;
	    srcpixel[2] = srcdata[x * 3 + 2] / 255.0;

	    code = idev->luo->lookup(idev->luo, dstpixel, srcpixel);
	    if (code > 1)
	    {
		code = gs_throw1(-1, "icc lookup failed: %s", idev->icco->err);
		goto cleanup;
	    }

	    dstbuffer[x * 4 + 0] = dstpixel[0] * 255 + 0.5;
	    dstbuffer[x * 4 + 1] = dstpixel[1] * 255 + 0.5;
	    dstbuffer[x * 4 + 2] = dstpixel[2] * 255 + 0.5;
	    dstbuffer[x * 4 + 3] = dstpixel[3] * 255 + 0.5;

	    /* output planar data to auxiliary output files */
	    for (k = 0; k < 4; k++)
		putc(dstbuffer[x * 4 + k], fp[k]);
	}
#endif
    }

    dprintf4("same=%d/%d diff=%d/%d\n", lsame, nsame, ldiff, ndiff);


    /*
     * Cleanup memory and files.
     */

cleanup:

    for (k = 0; k < 4; k++)
	if (fp[k])
	    fclose(fp[k]);

    if (dstbuffer)
	gs_free(pdev->memory, dstbuffer, dststride, 1, "imdi_print_page(dstbuffer)");

    if (srcbuffer)
	gs_free(pdev->memory, srcbuffer, srcstride, 1, "imdi_print_page(srcbuffer)");

    return code;
}

