/* Copyright (C) 2006 artofcode LLC.  All rights reserved.
  
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

/* $Id: gdevwts.c 6300 2005-12-28 19:56:24Z giles $ */

/* TODO: this should be configurable */
#define LINK_ICC_NAME	"link.icc"

#include "errno_.h"
#include "string_.h"

#include "gdevprn.h"
#include "gsdevice.h"
#include "gxfrac.h"
#include "gsht.h"
#include "gxwts.h"
#include "gswts.h"

#include "icc.h"
#include "imdi.h"

/* Memory arg is included in ghostpcl branch but not main branch. */
#define GS_NOTE_ERROR(m, e) gs_note_error(e)

#ifndef X_DPI
#  define X_DPI 72
#endif
#ifndef Y_DPI
#  define Y_DPI 72
#endif

typedef struct {
    wts_screen_t *wts;
    byte *cell;
} wts_cooked_halftone;

typedef struct gx_device_wts_s {
    gx_device_common;
    gx_prn_device_common;
    wts_cooked_halftone wcooked[4];
} gx_device_wts;

private dev_proc_print_page(wtscmyk_print_page);

/* 8-bit-per-plane separated CMYK color. */

private const gx_device_procs wtscmyk_procs = {
    gdev_prn_open, NULL, NULL, gdev_prn_output_page, gdev_prn_close,
    NULL, cmyk_8bit_map_color_cmyk, NULL, NULL, NULL, NULL, NULL, NULL,
    gdev_prn_get_params, gdev_prn_put_params,
    cmyk_8bit_map_cmyk_color, NULL, NULL, NULL, gx_page_device_get_page_device
};

const gx_device_wts gs_wtscmyk_device = {
    prn_device_body(gx_device_wts, wtscmyk_procs, "wtscmyk",
			DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
			X_DPI, Y_DPI,
			0, 0, 0, 0,	/* Margins */
			4, 32, 255, 255, 256, 256, wtscmyk_print_page)
};

/* RGB with imdi conversion to CMYK and wts halftoning */

private dev_proc_open_device(wtsimdi_open_device);
private dev_proc_close_device(wtsimdi_close_device);
private dev_proc_print_page(wtsimdi_print_page);

typedef struct gx_device_wtsimdi_s {
    gx_device_common;
    gx_prn_device_common;
    wts_cooked_halftone wcooked[4];

    icmFile *fp;
    icc *icco;
    icmLuBase *luo;
    imdi *mdo;

} gx_device_wtsimdi;

#include "wtsimdi.c"
private const gx_device_procs wtsimdi_procs =
{
    wtsimdi_open_device, NULL, NULL, gdev_prn_output_page, wtsimdi_close_device,
    gx_default_rgb_map_rgb_color, gx_default_rgb_map_color_rgb,
    NULL, NULL, NULL, NULL, NULL, NULL,
    gdev_prn_get_params, gdev_prn_put_params,
    NULL, NULL, NULL, NULL, gx_page_device_get_page_device
};

const gx_device_wtsimdi gs_wtsimdi_device = {
    prn_device_std_body(gx_device_wtsimdi, wtsimdi_procs, "wtsimdi",
			DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
			X_DPI, Y_DPI,
			0, 0, 0, 0,	/* Margins */
			24, wtsimdi_print_page)
};

private void
wts_halftone_line(void **wts, int y, int width, int n_planes,
		  byte **dst, const byte *src)
{
    int x;
    int plane_ix;

    for (plane_ix = 0; plane_ix < n_planes; plane_ix++) {
	byte *dline = dst[plane_ix];
	for (x = 0; x < width; x += 8) {
	    byte b = 0;
	    if (src[(x + 0) * n_planes + plane_ix] > 0x20) b |= 0x80;
	    if (src[(x + 1) * n_planes + plane_ix] > 0x40) b |= 0x40;
	    if (src[(x + 2) * n_planes + plane_ix] > 0x60) b |= 0x20;
	    if (src[(x + 3) * n_planes + plane_ix] > 0x80) b |= 0x10;
	    if (src[(x + 4) * n_planes + plane_ix] > 0xa0) b |= 0x08;
	    if (src[(x + 5) * n_planes + plane_ix] > 0xc0) b |= 0x04;
	    if (src[(x + 6) * n_planes + plane_ix] > 0xe0) b |= 0x02;
	    if (src[(x + 7) * n_planes + plane_ix] > 0xfe) b |= 0x01;
	    dline[x >> 3] = b;
	}
    }
}

private void
wts_halftone_line_16(wts_cooked_halftone *wch, int y, int width, int n_planes,
		     byte **dst, const byte *src)
{
    int x;
    int plane_ix;
    wts_screen_sample_t *samples;

    for (plane_ix = 0; plane_ix < n_planes; plane_ix++) {
	wts_screen_t *w = wch[plane_ix].wts;
	byte *dline = dst[plane_ix];
	int imax;

	for (x = 0; x < width;) {
	    int i;
	    int n_samples;
	    int cx, cy;

	    wts_get_samples(w, x, y, &cx, &cy, &n_samples);
	    samples = w->samples + cy * w->cell_width + cx;

	    imax = min(width - x, n_samples);
	    for (i = 0; i < imax; i += 8) {
		byte b = 0;
		int src_ix = x * 4 + plane_ix;
#if 0
		if (src[src_ix + 0 * 4] > (samples[i + 0] >> 7)) b |= 0x80;
		if (src[src_ix + 1 * 4] > (samples[i + 1] >> 7)) b |= 0x40;
		if (src[src_ix + 2 * 4] > (samples[i + 2] >> 7)) b |= 0x20;
		if (src[src_ix + 3 * 4] > (samples[i + 3] >> 7)) b |= 0x10;
		if (src[src_ix + 4 * 4] > (samples[i + 4] >> 7)) b |= 0x08;
		if (src[src_ix + 5 * 4] > (samples[i + 5] >> 7)) b |= 0x04;
		if (src[src_ix + 6 * 4] > (samples[i + 6] >> 7)) b |= 0x02;
		if (src[src_ix + 7 * 4] > (samples[i + 7] >> 7)) b |= 0x01;
#else
#if 0
		b |= (src[src_ix + 0 * 4] > (samples[i + 0] >> 7)) << 7;
		b |= (src[src_ix + 1 * 4] > (samples[i + 1] >> 7)) << 6;
		b |= (src[src_ix + 2 * 4] > (samples[i + 2] >> 7)) << 5;
		b |= (src[src_ix + 3 * 4] > (samples[i + 3] >> 7)) << 4;
		b |= (src[src_ix + 4 * 4] > (samples[i + 4] >> 7)) << 3;
		b |= (src[src_ix + 5 * 4] > (samples[i + 5] >> 7)) << 2;
		b |= (src[src_ix + 6 * 4] > (samples[i + 6] >> 7)) << 1;
		b |= (src[src_ix + 7 * 4] > (samples[i + 7] >> 7)) << 0;
#else
		b = (((unsigned int)(((int)(samples[i + 0] >> 7)) - ((int)src[src_ix + 0 * 4]))) >> 24) & 0x80;
		b |= (((unsigned int)(((int)(samples[i + 1] >> 7)) - ((int)src[src_ix + 1 * 4]))) >> 24) & 0x40;
		b |= (((unsigned int)(((int)(samples[i + 2] >> 7)) - ((int)src[src_ix + 2 * 4]))) >> 24) & 0x20;
		b |= (((unsigned int)(((int)(samples[i + 3] >> 7)) - ((int)src[src_ix + 3 * 4]))) >> 24) & 0x10;
		b |= (((unsigned int)(((int)(samples[i + 4] >> 7)) - ((int)src[src_ix + 4 * 4]))) >> 24) & 0x08;
		b |= (((unsigned int)(((int)(samples[i + 5] >> 7)) - ((int)src[src_ix + 5 * 4]))) >> 24) & 0x04;
		b |= (((unsigned int)(((int)(samples[i + 6] >> 7)) - ((int)src[src_ix + 6 * 4]))) >> 24) & 0x02;
		b |= (((unsigned int)(((int)(samples[i + 7] >> 7)) - ((int)src[src_ix + 7 * 4]))) >> 24) & 0x01;
#endif
#endif
		dline[x >> 3] = b;
		x += 8;
	    }
	}
    }
}

private void
wts_halftone_line_8(wts_cooked_halftone *wch, int y, int width, int n_planes,
		    byte **dst, const byte *src)
{
    int x;
    int plane_ix;
    byte *samples;

    for (plane_ix = 0; plane_ix < n_planes; plane_ix++) {
	wts_screen_t *w = wch[plane_ix].wts;
	byte *dline = dst[plane_ix];
	int imax;

	for (x = 0; x < width;) {
	    int i;
	    int n_samples;
	    int cx, cy;

	    wts_get_samples(w, x, y, &cx, &cy, &n_samples);
	    samples = wch[plane_ix].cell + cy * w->cell_width + cx;

	    imax = min(width - x, n_samples);
	    for (i = 0; i < imax; i += 8) {
		byte b = 0;
		int src_ix = x * 4 + plane_ix;
		b = (((unsigned int)(((int)(samples[i + 0])) - ((int)src[src_ix + 0 * 4]))) >> 24) & 0x80;
		b |= (((unsigned int)(((int)(samples[i + 1])) - ((int)src[src_ix + 1 * 4]))) >> 24) & 0x40;
		b |= (((unsigned int)(((int)(samples[i + 2])) - ((int)src[src_ix + 2 * 4]))) >> 24) & 0x20;
		b |= (((unsigned int)(((int)(samples[i + 3])) - ((int)src[src_ix + 3 * 4]))) >> 24) & 0x10;
		b |= (((unsigned int)(((int)(samples[i + 4])) - ((int)src[src_ix + 4 * 4]))) >> 24) & 0x08;
		b |= (((unsigned int)(((int)(samples[i + 5])) - ((int)src[src_ix + 5 * 4]))) >> 24) & 0x04;
		b |= (((unsigned int)(((int)(samples[i + 6])) - ((int)src[src_ix + 6 * 4]))) >> 24) & 0x02;
		b |= (((unsigned int)(((int)(samples[i + 7])) - ((int)src[src_ix + 7 * 4]))) >> 24) & 0x01;
		dline[x >> 3] = b;
		x += 8;
	    }
	}
    }
}

private int
wts_load_halftone(gs_memory_t *mem, wts_cooked_halftone *wch, const char *fn)
{
    FILE *f = fopen(fn, "rb");
    int size;
    byte *buf;
    wts_screen_t *wts;
    int width_padded;
    byte *cooked_cell;
    int y;

    if (f == 0) return gs_error_undefinedfilename;
    fseek(f, 0, 2);
    size = ftell(f);
    fseek(f, 0, 0);
    buf = gs_malloc(mem, size, 1, "wts_load_halftone");
    if (buf == 0) {
	return gs_error_VMerror;
    }
    fread(buf, 1, size, f);
    fclose(f);
    wts = gs_wts_from_buf(buf);
    gs_free(mem, buf, size, 1, "wts_load_halftone");
    wch->wts = wts;
    width_padded = wts->cell_width + 7;
    cooked_cell = gs_malloc(mem, width_padded * wts->cell_height, 1,
			    "wts_load_halftone");
    if (cooked_cell == 0) {
	return gs_error_VMerror;
    }

    wch->cell = cooked_cell;
    for (y = 0; y < wts->cell_height; y++) {
	wts_screen_sample_t *line = &wts->samples[y * wts->cell_width];
	byte *dstline = cooked_cell + y * width_padded;
	int x;
	for (x = 0; x < width_padded; x++) {
	    wts_screen_sample_t sample = line[x % wts->cell_width];
	    dstline[x] = (254 * (int)sample + 0x7fc0) / 0x8ff0;
	}
    }

#if 0
    /* Note: we'll need to fix the free discipline here when we change it
       in gswts.c */
    free(wts->samples);
    wts->samples = NULL;
#endif

    return 0;
}

private int
wts_init_halftones(gx_device_wts *wdev, int n_planes)
{
    int i;
    int code;

    for (i = 0; i < n_planes; i++) {
	if (wdev->wcooked[i].wts == 0) {
	    char wts_fn[256];

	    sprintf(wts_fn, "wts_plane_%d", i);
	    code = wts_load_halftone(wdev->memory, &wdev->wcooked[i], wts_fn);
	    if (code < 0)
		return code;
	}
    }
    return 0;
}

private int
wtscmyk_print_page(gx_device_printer *pdev, FILE *prn_stream)
{
    gx_device_wts *wdev = (gx_device_wts *)pdev;
    int cmyk_bytes = gdev_mem_bytes_per_scan_line((gx_device *) pdev);
    /* Output bytes have to be padded to 16 bits. */
    int y;
    char *cmyk_line = 0;
    byte *data;
    int code = 0;
    int pbm_bytes;
    int n_planes = pdev->color_info.num_components;
    byte *dst[GX_DEVICE_COLOR_MAX_COMPONENTS] = {0};
    FILE *ostream[4] = {0};
    int i;

    wts_init_halftones(wdev, n_planes);

    cmyk_line = gs_malloc(pdev->memory, cmyk_bytes, 1, "wtscmyk_print_page(in)");
    if (cmyk_line == 0) {
	code = GS_NOTE_ERROR(pdev->memory, gs_error_VMerror);
	goto out;
    }
    pbm_bytes = (pdev->width + 7) >> 3;
    for (i = 0; i < n_planes; i++) {
	dst[i] = gs_malloc(pdev->memory, pbm_bytes, 1, "wtscmyk_print_page");
	if (dst[i] == 0) {
	    code = GS_NOTE_ERROR(pdev->memory, gs_error_VMerror);
	    goto out;
	}
    }

    /* Create additional output streams. */
    for (i = 0; i < n_planes; i++) {
	if (i == 0) {
	    ostream[i] = prn_stream;
	} else {
	    char fn[256];
	    char plane_name[4] = "cmyk";
	    int fname_len = strlen(pdev->fname);

	    if (fname_len >= 5 && fname_len < 256) {
		strcpy(fn, pdev->fname);
		if (!strcmp(fn + fname_len - 5, "c.pbm"))
		    fn[fname_len - 5] = plane_name[i];
	    }
	    ostream[i] = fopen(fn, "wb");
	}
	fprintf(ostream[i], "P4\n%d %d\n", pdev->width, pdev->height);
    }
#if 0
    dprintf2(OPTIONAL_MEM(pdev->memory) "Output file name: %s %d\n", pdev->fname, sizeof(dst));
#endif

    /* For each raster line */
    for (y = 0; y < pdev->height; y++) {
	gdev_prn_get_bits(pdev, y, cmyk_line, &data);
	wts_halftone_line_8(wdev->wcooked, y, pdev->width, n_planes, dst, data);
	for (i = 0; i < n_planes; i++)
	    if (ostream[i])
		fwrite(dst[i], 1, pbm_bytes, ostream[i]);
    }
out:
    /* Clean up... */
    gs_free(pdev->memory, cmyk_line, cmyk_bytes, 1, "wtscmyk_print_page(in)");
    for (i = 0; i < n_planes; i++) {
	gs_free(pdev->memory, dst[i], pbm_bytes, 1, "wtscmyk_print_page");
	/* Don't close ostream[0], because gdev_prn_close will. */
	if (i > 0 && ostream[i])
	    fclose(ostream[i]);
    }
    return code;
}

/* Code that follows is adapted from imdi device */

private double incurve(void *ctx, int ch, double val)
{
    return val;
}

private double outcurve(void *ctx, int ch, double val)
{
    return val;
}

private void mdtable(void *ctx, double *outvals, double *invals)
{
    icmLuBase *luo = ctx;
    luo->lookup(luo, outvals, invals);
}

/*
 * Open IMDI device.
 * Load ICC device link profile (to map sRGB to FOGRA CMYK).
 */

private int
wtsimdi_open_device(gx_device *dev)
{
    gx_device_wtsimdi *idev = (gx_device_wtsimdi*)dev;
    int code;

    icColorSpaceSignature ins, outs;
    int inn, outn;
    icmLuAlgType alg;

    icmFile *fp;
    icc *icco;
    icmLuBase *luo;
    imdi *mdo;

    /* Open and read profile */

    fp = new_icmFileStd_name(LINK_ICC_NAME, "rb");
    if (!fp)
	return gs_throw1(-1, "could not open file '%s'", LINK_ICC_NAME);

    icco = new_icc();
    if (!icco)
	return gs_throw(-1, "could not create ICC object");

    code = icco->read(icco, fp, 0);
    if (code != 0)
	return gs_throw1(-1, "could not read ICC profile: %s", icco->err);

    /* Get conversion object */

    luo = icco->get_luobj(icco, icmFwd, icPerceptual, icmSigDefaultData, icmLuOrdNorm);
    if (!luo)
	return gs_throw1(-1, "could not create ICC conversion object: %s", icco->err);
    
    luo->spaces(luo, &ins, &inn, &outs, &outn, &alg, NULL, NULL, NULL);

    dprintf3("%s -> %s [%s]\n",
	    icm2str(icmColorSpaceSignature, ins),
	    icm2str(icmColorSpaceSignature, outs),
	    icm2str(icmLuAlg, alg));

    if (inn != 3)
	return gs_throw1(-1, "profile must have 3 input channels. got %d.", inn);
    if (outn != 4)
	return gs_throw1(-1, "profile must have 4 output channels. got %d.", outn);

    /* Create IMDI optimized lookup object */

    mdo = new_imdi(inn, outn, pixint8, 0, pixint8, 0,
			 33, incurve, mdtable, outcurve, luo);
    if (!mdo)
	return gs_throw(-1, "new_imdi failed");

    idev->fp = fp;
    idev->icco = icco;
    idev->luo = luo;
    idev->mdo = mdo;

    return gdev_prn_open(dev);
}


/*
 * Close device and clean up ICC structures.
 */

private int
wtsimdi_close_device(gx_device *dev)
{
    gx_device_wtsimdi *idev = (gx_device_wtsimdi*)dev;

    idev->mdo->done(idev->mdo);
    idev->luo->del(idev->luo);
    idev->icco->del(idev->icco);
    idev->fp->del(idev->fp);

    return gdev_prn_close(dev);
}

/*
 * Create a row of output data.  The output is the same as the pkmraw
 * device.  This a pseudo 1 bit CMYK output.  Actually the output is
 * 3 byte RGB with each byte being either 0 or 1.
 */
write_pkmraw_row(gx_device_printer * pdev, byte ** data, FILE * pstream)
{
    int x, bit;
    byte * cdata = data[0];
    byte * mdata = data[1];
    byte * ydata = data[2];
    byte * kdata = data[3];
    byte c = *cdata++;
    byte m = *mdata++;
    byte y = *ydata++;
    byte k = *kdata++;

    /*
     * Contrary to what the documentation implies, gcc compiles putc
     * as a procedure call.  This is ridiculous, but since we can't
     * change it, we buffer groups of pixels ourselves and use fwrite.
     */
    for (bit = 7, x = 0; x < pdev->width;) {
	byte raw[80 * 3];	/* 80 is arbitrary, must be a multiple of 8 */
	int end = min(x + sizeof(raw) / 3, pdev->width);
	byte *outp = raw;

	for (; x < end; x++) {

	    if ((k >> bit) & 1) {
		*outp++ = 0;	/* Set output color = black */
		*outp++ = 0;
		*outp++ = 0;
	    } else {
		*outp++ = 1 - ((c >> bit) & 1);
		*outp++ = 1 - ((m >> bit) & 1);
		*outp++ = 1 - ((y >> bit) & 1);
	    }
	    if (bit == 0) {
	        c = *cdata++;
	        m = *mdata++;
	        y = *ydata++;
	        k = *kdata++;
	        bit = 7;
	    } else
	        bit--;
	}
	fwrite(raw, 1, outp - raw, pstream);
    }
    return 0;
}

/*
 * Output the page raster.
 */

private int
wtsimdi_print_page(gx_device_printer *pdev, FILE *prn_stream)
{
    gx_device_wtsimdi *idev = (gx_device_wtsimdi*)pdev;

    byte *srcbuffer = 0;
    byte *dstbuffer = 0;

    int srcstride;
    int dststride;
    int srcplanes;
    int dstplanes;

    int pbm_bytes;
    int n_planes = 4;
    byte *dst[GX_DEVICE_COLOR_MAX_COMPONENTS] = {0};
    byte * cmykpixels;
    int i;

    double srcpixel[GX_DEVICE_COLOR_MAX_COMPONENTS];
    double dstpixel[GX_DEVICE_COLOR_MAX_COMPONENTS];
    byte *srcdata;

    int code = 0;
    int x, y;

    int nsame = 0, lsame = 0;
    int ndiff = 0, ldiff = 0;

    wts_init_halftones((gx_device_wts *)idev, n_planes);

    pbm_bytes = (pdev->width + 7) >> 3;
    for (i = 0; i < n_planes; i++) {
	dst[i] = gs_malloc(pdev->memory, pbm_bytes, 1, "wtsimdi_print_page");
	if (dst[i] == NULL) {
	    code = GS_NOTE_ERROR(pdev->memory, gs_error_VMerror);
	    goto cleanup;
	}
    }
    cmykpixels = gs_malloc(pdev->memory, pbm_bytes, 4, "wtsimdi_print_page");
    if (cmykpixels == NULL) {
	code = GS_NOTE_ERROR(pdev->memory, gs_error_VMerror);
	goto cleanup;
    }

    /* Initialize output file header. */
    fprintf(prn_stream, "P6\n%d %d\n", pdev->width, pdev->height);
    fprintf(prn_stream, "# Image generated by %s %d.%02d (device=wtsimdi)\n",
	   	gs_program_name(), gs_revision_number() / 100,
	   			gs_revision_number() % 100);
    fprintf(prn_stream, "%d\n", 1);

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

#if 0 /* Collect runlengths */

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

		idev->mdo->interp(idev->mdo, outp, inp, 1);
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

		idev->mdo->interp(idev->mdo, outp, inp, ex - sx);
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

	idev->mdo->interp(idev->mdo, outp, inp, pdev->width);

#endif

#if 0 /* Call IMDI for every pixel */
	for (x = 0; x < pdev->width; x++)
	{
	    void *inp[1];
	    void *outp[1];

	    inp[0] = srcdata + x * 3;
	    outp[0] = dstbuffer + x * 4;

	    idev->mdo->interp(idev->mdo, outp, inp, 1);
	}
#endif

#if 1 /* Slow but accurate every pixel */

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

	}
#endif

	wts_halftone_line_8(idev->wcooked, y, pdev->width, n_planes, dst, dstbuffer);
	write_pkmraw_row(pdev, dst, prn_stream);
    }

    dprintf4("same=%d/%d diff=%d/%d\n", lsame, nsame, ldiff, ndiff);


    /*
     * Cleanup memory and files.
     */

cleanup:

    for (i = 0; i < n_planes; i++) {
	gs_free(pdev->memory, dst[i], pbm_bytes, 1, "wtsimdi_print_page");
    }

    if (dstbuffer)
	gs_free(pdev->memory, dstbuffer, dststride, 1, "imdi_print_page(dstbuffer)");

    if (srcbuffer)
	gs_free(pdev->memory, srcbuffer, srcstride, 1, "imdi_print_page(srcbuffer)");

    return code;
}
