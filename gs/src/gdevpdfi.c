/* Copyright (C) 1996, 1997 Aladdin Enterprises.  All rights reserved.

   This file is part of Aladdin Ghostscript.

   Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
   or distributor accepts any responsibility for the consequences of using it,
   or for whether it serves any particular purpose or works at all, unless he
   or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
   License (the "License") for full details.

   Every copy of Aladdin Ghostscript must include a copy of the License,
   normally in a plain ASCII text file named PUBLIC.  The License grants you
   the right to copy, modify and redistribute Aladdin Ghostscript, but only
   under certain conditions described in the License.  Among other things, the
   License requires that the copyright notice and this notice be preserved on
   all copies.
 */

/* gdevpdfi.c */
/* Image handling for PDF-writing driver */
#include "math_.h"
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsflip.h"
#include "gdevpdfx.h"
#include "gxcspace.h"
#include "gscolor2.h"		/* for gscie.h */
#include "gscie.h"		/* requires gscspace.h */
#include "gxistate.h"
#include "strimpl.h"
#include "sa85x.h"
#include "scfx.h"
#include "srlx.h"

/* We need color space types for constructing temporary color spaces. */
extern const gs_color_space_type
      gs_color_space_type_DeviceGray, gs_color_space_type_DeviceRGB, gs_color_space_type_DeviceCMYK,
      gs_color_space_type_Indexed;

/* ---------------- Utilities ---------------- */

/* ------ Binary data ------ */

/* Define the structure for the filters for writing binary data. */
typedef struct pdf_binary_writer_s {
    stream *strm;
    stream es;			/* no state for A85E */
    byte encode_buf[256];
    stream cs;			/* client provides (initialized) state */
    byte compress_buf[256];
} pdf_binary_writer;

private const stream_procs filter_write_procs =
{s_std_noavailable, s_std_noseek, s_std_write_reset,
 s_std_write_flush, s_filter_close
};

/* Begin writing binary data. */
/* If css is not NULL, it is the compressor stream state. */
private int
pdf_begin_binary(gx_device_pdf * pdev, pdf_binary_writer * pbw,
		 stream_state * css)
{
    stream *s = pdev->strm;

    /* If not binary, set up the encoding stream. */
    if (!pdev->binary_ok) {
	stream *es = &pbw->es;

	s_std_init(es, pbw->encode_buf, sizeof(pbw->encode_buf),
		   &filter_write_procs, s_mode_write);
	es->template = &s_A85E_template;
	es->procs.process = es->template->process;
	es->strm = s;
	s = es;
    }
    /* If compressing, set up the compression stream. */
    if (css) {
	stream *cs = (stream *) & pbw->cs;
	const stream_template *template = css->template;

	s_std_init(cs, pbw->compress_buf, sizeof(pbw->compress_buf),
		   &filter_write_procs, s_mode_write);
	css->memory = pdev->pdf_memory;
	cs->state = css;
	cs->procs.process = template->process;
	if (template->init)
	    (*template->init) (css);
	cs->strm = s;
	s = cs;
    }
    pbw->strm = s;
    return 0;
}

/* Finish writing binary data. */
private int
pdf_end_binary(gx_device_pdf * pdev, pdf_binary_writer * pbw)
{
    stream *s = pbw->strm;

    /* Close the filters in reverse order. */
    /* Stop before we try to close the file stream. */
    while (s != pdev->strm) {
	stream *next = s->strm;

	/* We have to open-code sclose, because we want to release */
	/* the stream state but not try to free it. */
	stream_state *st = s->state;

	stream_proc_release((*release)) = st->template->release;
	(*s->procs.close) (s);
	if (release != 0)
	    (*release) (st);
	s = next;
    }
    sflush(s);			/* flush the file stream buffer */
    return 0;
}

/* ------ Images ------ */

/* Test whether a cached CIE procedure is the identity function. */
#define cie_cache_is_identity(pc)\
  ((pc)->floats.params.is_identity)
#define cie_cache3_is_identity(pca)\
  (cie_cache_is_identity(&(pca)[0]) &&\
   cie_cache_is_identity(&(pca)[1]) &&\
   cie_cache_is_identity(&(pca)[2]))

/*
 * Test whether a cached CIE procedure is an exponential.  A cached
 * procedure is exponential iff f(x) = k*(x^p).  We make a very cursory
 * check for this: we require that f(0) = 0, set k = f(1), set p =
 * log[a](f(a)/k), and then require that f(b) = k*(b^p), where a and b are
 * two arbitrarily chosen values between 0 and 1.  Naturally all this is
 * done with some slop.
 */
#define ia (gx_cie_cache_size / 3)
#define ib (gx_cie_cache_size * 2 / 3)
#define iv(i) ((i) / (double)(gx_cie_cache_size - 1))
#define a iv(ia)
#define b iv(ib)

private bool
cie_values_are_exponential(floatp va, floatp vb, floatp k,
			   float *pexpt)
{
    double p;

    if (fabs(k) < 0.001)
	return false;
    if (va == 0 || (va > 0) != (k > 0))
	return false;
    p = log(va / k) / log(a);
    if (fabs(vb - k * pow(b, p)) >= 0.001)
	return false;
    *pexpt = p;
    return true;
}

private bool
cie_scalar_cache_is_exponential(const gx_cie_scalar_cache * pc, float *pexpt)
{
    double k, va, vb;

    if (fabs(pc->floats.values[0]) >= 0.001)
	return false;
    k = pc->floats.values[gx_cie_cache_size - 1];
    va = pc->floats.values[ia];
    vb = pc->floats.values[ib];
    return cie_values_are_exponential(va, vb, k, pexpt);
}
#define cie_scalar3_cache_is_exponential(pca, expts)\
  (cie_scalar_cache_is_exponential(&(pca)[0], &(expts)[0]) &&\
   cie_scalar_cache_is_exponential(&(pca)[1], &(expts)[1]) &&\
   cie_scalar_cache_is_exponential(&(pca)[2], &(expts)[2]))

private bool
cie_vector_cache_is_exponential(const gx_cie_vector_cache * pc, float *pexpt)
{
    double k, va, vb;

    if (fabs(pc->vecs.values[0].u) >= 0.001)
	return false;
    k = pc->vecs.values[gx_cie_cache_size - 1].u;
    va = pc->vecs.values[ia].u;
    vb = pc->vecs.values[ib].u;
    return cie_values_are_exponential(va, vb, k, pexpt);
}
#define cie_vector3_cache_is_exponential(pca, expts)\
  (cie_vector_cache_is_exponential(&(pca)[0], &(expts)[0]) &&\
   cie_vector_cache_is_exponential(&(pca)[1], &(expts)[1]) &&\
   cie_vector_cache_is_exponential(&(pca)[2], &(expts)[2]))

#undef ia
#undef ib
#undef iv
#undef a
#undef b

/* Define the long and short versions of the keys in an image dictionary, */
/* and other strings for images. */
typedef struct pdf_image_names_s {
    const char *ASCII85Decode;
    const char *BitsPerComponent;
    const char *CalCMYK;
    const char *CalGray;
    const char *CalRGB;
    const char *CCITTFaxDecode;
    const char *ColorSpace;
    const char *Decode;
    const char *DecodeParms;
    const char *DeviceCMYK;
    const char *DeviceGray;
    const char *DeviceRGB;
    const char *Filter;
    const char *Height;
    const char *ImageMask;
    const char *Indexed;
    const char *Interpolate;
    const char *Width;
} pdf_image_names;
private const pdf_image_names image_names_full =
{
    "/ASCII85Decode", "/BitsPerComponent",
    "/CalCMYK", "/CalGray", "/CalRGB", "/CCITTFaxDecode", "/ColorSpace",
    "/Decode", "/DecodeParms", "/DeviceCMYK", "/DeviceGray", "/DeviceRGB",
    "/Filter", "/Height", "/ImageMask", "/Indexed", "/Interpolate", "/Width",
};
private const pdf_image_names image_names_short =
{
    "/A85", "/BPC",
	/* We need CalRGB to work around a bug in some Adobe products. */
    "/CC", "/CG", /*"/CR" */ "/CalRGB", "/CCF", "/CS",
    "/D", "/DP", "/CMYK", "/G", "/RGB",
    "/F", "/H", "/IM", "/I", "/I", "/W",
};

/* Write out image parameters for either an in-line image or image resource. */
/* decode_parms, if supplied, must start with /, [, or <<. */
private int
pdf_write_image_params(gx_device_pdf * pdev, const gs_image_t * pim,
		       const char *filter_name, const char *decode_parms,
		       const pdf_image_names * pin)
{
    stream *s = pdev->strm;
    const gs_color_space *pcs = pim->ColorSpace;
    const char *cs_name;
    int num_components;
    float indexed_decode[2];
    const float *default_decode = NULL;

    if (pim->ImageMask) {
	pprints1(s, "%s true", pin->ImageMask);
	pdev->procsets |= ImageB;
	num_components = 1;
    } else {
	const gs_color_space *pbcs = pcs;
	const gs_indexed_params *pip = 0;
	const gs_cie_common *pciec;

	pputs(s, pin->ColorSpace);
      csw:switch (gs_color_space_get_index(pbcs)) {
	    case gs_color_space_index_DeviceGray:
		pdev->procsets |= ImageB;
		cs_name = pin->DeviceGray;
		break;
	    case gs_color_space_index_DeviceRGB:
		pdev->procsets |= ImageC;
		cs_name = pin->DeviceRGB;
		break;
	    case gs_color_space_index_DeviceCMYK:
		pdev->procsets |= ImageC;
		cs_name = pin->DeviceCMYK;
		break;
	    case gs_color_space_index_CIEA:
		pdev->procsets |= ImageB;
		pprints1(s, "[%s<<", pin->CalGray);
		{
		    const gs_cie_a *pcie = pbcs->params.a;
		    float expts[3];

		    if (cie_cache3_is_identity(pcie->common.caches.DecodeLMN))
			cie_vector_cache_is_exponential(&pcie->caches.DecodeA, &expts[0]);
		    else
			discard(cie_scalar3_cache_is_exponential(pcie->common.caches.DecodeLMN, expts));
		    if (expts[0] != 1)
			pprintg1(s, "/Gamma %g", expts[0]);
		    pciec = (const gs_cie_common *)pcie;
		}
	      cal:pprintg3(s, "/WhitePoint[%g %g %g]",
			 pciec->points.WhitePoint.u,
			 pciec->points.WhitePoint.v,
			 pciec->points.WhitePoint.w);
		if (pciec->points.BlackPoint.u != 0 ||
		    pciec->points.BlackPoint.v != 0 ||
		    pciec->points.BlackPoint.w != 0
		    )
		    pprintg3(s, "/BlackPoint[%g %g %g]",
			     pciec->points.BlackPoint.u,
			     pciec->points.BlackPoint.v,
			     pciec->points.BlackPoint.w);
		pputs(s, ">>]");
		cs_name = 0;
		break;
	    case gs_color_space_index_CIEABC:
		pdev->procsets |= ImageC;
		pprints1(s, "[%s<<", pin->CalRGB);
		{
		    const gs_cie_abc *pcie = pbcs->params.abc;
		    const gs_matrix3 *pmat;
		    float expts[3];

		    if (pcie->common.MatrixLMN.is_identity &&
			cie_cache3_is_identity(pcie->common.caches.DecodeLMN)
			) {
			discard(cie_vector3_cache_is_exponential(pcie->caches.DecodeABC, expts));
			pmat = &pcie->MatrixABC;
		    } else {
			discard(cie_scalar3_cache_is_exponential(pcie->common.caches.DecodeLMN, expts));
			pmat = &pcie->common.MatrixLMN;
		    }
		    if (expts[0] != 1 || expts[1] != 1 || expts[2] != 1)
			pprintg3(s, "/Gamma[%g %g %g]", expts[0], expts[1],
				 expts[2]);
		    if (!pmat->is_identity) {
			pprintg3(s, "/Matrix[%g %g %g",
				 pmat->cu.u, pmat->cu.v, pmat->cu.w);
			pprintg6(s, " %g %g %g %g %g %g]",
				 pmat->cv.u, pmat->cv.v, pmat->cv.w,
				 pmat->cw.u, pmat->cw.v, pmat->cw.w);
		    }
		    pciec = (const gs_cie_common *)pcie;
		}
		goto cal;
	    case gs_color_space_index_Indexed:
		pdev->procsets |= ImageI;
		pprints1(s, "[%s", pin->Indexed);
		pip = &pcs->params.indexed;
		pbcs = (const gs_color_space *)&pip->base_space;
		indexed_decode[0] = 0;
		indexed_decode[1] = (1 << pim->BitsPerComponent) - 1;
		default_decode = indexed_decode;
		goto csw;
	    default:		/* shouldn't happen */
		return_error(gs_error_rangecheck);
	}
	if (cs_name)
	    pprints1(s, " %s", cs_name);
	num_components = gs_color_space_num_components(pbcs);
	if (pip) {
	    register const char _ds *hex_digits = "0123456789abcdef";
	    int i;

	    pprintd1(s, " %d\n<", pip->hival);
	    for (i = 0; i < (pip->hival + 1) * num_components; ++i) {
		byte b = pip->lookup.table.data[i];

		pputc(s, hex_digits[b >> 4]);
		pputc(s, hex_digits[b & 0xf]);
	    }
	    pputs(s, ">\n]");
	    num_components = 1;
	}
    }
/* Some compilers try to substitute macro args in string literals! */
#define pprintsd(strm, str, v)\
  (pputs(strm, str), pprintd1(strm, " %d", v))
    pprintsd(s, pin->Width, pim->Width);
    pprintsd(s, pin->Height, pim->Height);
    pprintsd(s, pin->BitsPerComponent, pim->BitsPerComponent);
#undef pprintsd
    {
	int i;

	for (i = 0; i < num_components * 2; ++i)
	    if (pim->Decode[i] !=
		(default_decode ? default_decode[i] : i & 1)
		)
		break;
	if (i < num_components * 2) {
	    char sepr = '[';

	    pputs(s, pin->Decode);
	    for (i = 0; i < num_components * 2; sepr = ' ', ++i) {
		pputc(s, sepr);
		pprintg1(s, "%g", pim->Decode[i]);
	    }
	    pputc(s, ']');
	}
    }
    if (pim->Interpolate)
	pprints1(s, "%s true", pin->Interpolate);
    if (filter_name) {
	if (pdev->binary_ok)
	    pprints2(s, "%s%s", pin->Filter, filter_name);
	else
	    pprints3(s, "%s[%s%s]", pin->Filter, pin->ASCII85Decode,
		     filter_name);
	if (decode_parms)
	    pprints2(s,
		     (pdev->binary_ok ? "%s%s" : "%s[null%s]"),
		     pin->DecodeParms, decode_parms);
    } else if (!pdev->binary_ok)
	pprints2(s, "%s%s", pin->Filter, pin->ASCII85Decode);
    return 0;
}

/* Fill in the image parameters for a device space bitmap. */
/* PDF images are always specified top-to-bottom. */
private void
pdf_make_bitmap_matrix(gs_matrix * pmat, int x, int y, int w, int h)
{
    pmat->xx = w;
    pmat->xy = 0;
    pmat->yx = 0;
    pmat->yy = -h;
    pmat->tx = x;
    pmat->ty = y + h;
}
private void
pdf_make_bitmap_image(gs_image_t * pim, int x, int y, int w, int h)
{
    pim->Width = w;
    pim->Height = h;
    pdf_make_bitmap_matrix(&pim->ImageMatrix, x, y, w, h);
}

/* Put out the gsave and matrix for an image. */
private void
pdf_put_image_matrix(gx_device_pdf * pdev, const gs_matrix * pmat)
{
    pdf_put_matrix(pdev, "q\n", pmat, "cm\n");
}

/* ------ Image writing ------ */

/* Define the structure for writing an image. */
typedef struct pdf_image_writer_s {
    pdf_binary_writer binary;
    const pdf_image_names *pin;
    const char *begin_data;
    pdf_resource *pres;		/* XObject resource iff not in-line */
    long length_id;		/* id of length object (forward reference) */
    long start_pos;		/* starting file position of data */
} pdf_image_writer;

/* Begin writing an image. */
private int
pdf_begin_write_image(gx_device_pdf * pdev, pdf_image_writer * piw, bool in_line)
{
    if (in_line) {
	stream *s = pdev->strm;

	piw->pres = 0;
	pputs(s, "BI\n");
	piw->pin = &image_names_short;
	piw->begin_data = (pdev->binary_ok ? "ID " : "ID\n");
    } else {
	int code = pdf_begin_resource(pdev, resourceXObject, gs_no_id,
				      &piw->pres);
	stream *s = pdev->strm;

	if (code < 0)
	    return code;
	piw->length_id = pdf_obj_ref(pdev);
	pprintld1(s, " /Subtype /Image /Length %ld 0 R\n",
		  piw->length_id);
	piw->pin = &image_names_full;
	piw->begin_data = ">>\nstream\n";
    }
    return 0;
}

/* Begin writing the image data. */
private int
pdf_begin_image_data(gx_device_pdf * pdev, pdf_image_writer * piw,
  const gs_image_t * pim, const char *filter_name, const char *decode_parms)
{
    stream *s = pdev->strm;
    int code = pdf_write_image_params(pdev, pim, filter_name, decode_parms,
				      piw->pin);

    if (code < 0)
	return code;
    pprints1(s, "\n%s", piw->begin_data);
    piw->start_pos = pdf_stell(pdev);
    return 0;
}

/* Finish writing an image. */
/* Return 0 if resource, 1 if in-line, or an error code. */
private int
pdf_end_write_image(gx_device_pdf * pdev, pdf_image_writer * piw)
{
    stream *s = pdev->strm;

    if (piw->pres) {		/* image resource */
	long length;

	pputs(s, "\n");
	length = pdf_stell(pdev) - piw->start_pos;
	pputs(s, "endstream\n");
	pdf_end_resource(pdev);
	pdf_open_separate(pdev, piw->length_id);
	s = pdev->strm;
	pprintld1(s, "%ld\n", length);
	pdf_end_separate(pdev);
	return 0;
    } else {			/* in-line image */
	pputs(s, "\nEI\nQ\n");
	return 1;
    }
}

/* Put out a reference to an image resource. */
private int
pdf_do_image(gx_device_pdf * pdev, const pdf_resource * pres,
	     const gs_matrix * pimat)
{
    int code = pdf_open_contents(pdev, pdf_in_stream);

    if (code < 0)
	return code;
    if (pimat)
	pdf_put_image_matrix(pdev, pimat);
    pprintld1(pdev->strm, "/R%ld Do\nQ\n", pres->id);
    return 0;
}

/* ---------------- Driver procedures ---------------- */

/* ------ Low-level calls ------ */

/* Copy a monochrome bitmap or mask. */
int
gdev_pdf_copy_mono(gx_device * dev,
		const byte * base, int sourcex, int raster, gx_bitmap_id id,
	int x, int y, int w, int h, gx_color_index zero, gx_color_index one)
{
    gx_device_pdf *pdev = (gx_device_pdf *) dev;
    int code;
    gs_color_space cs;
    byte palette[6];
    gs_image_t image;
    int yi;
    pdf_image_writer writer;
    pdf_stream_position ipos;
    pdf_resource *pres = 0;
    byte invert = 0;

    if (w <= 0 || h <= 0)
	return 0;
    /* Make sure we aren't being clipped. */
    if (pdf_must_put_clip_path(pdev, NULL)) {
	code = pdf_open_page(pdev, pdf_in_stream);
	if (code < 0)
	    return code;
	pdf_put_clip_path(pdev, NULL);
    }
    /* We have 3 cases: mask, inverse mask, and solid. */
    if (zero == gx_no_color_index) {
	if (one == gx_no_color_index)
	    return 0;
	/* If a mask has an id, assume it's a character. */
	if (id != gx_no_bitmap_id && sourcex == 0) {
	    pdf_set_color(pdev, one, &pdev->fill_color, "rg");
	    pres = pdf_find_resource_by_gs_id(pdev, resourceCharProc, id);
	    if (pres == 0) {	/* Define the character in an embedded font. */
		pdf_char_proc *pcp;
		int y_offset;
		int max_y_offset =
		(pdev->open_font == 0 ? 0 :
		 pdev->open_font->max_y_offset);

		gs_image_t_init_mask(&image, false);
		invert = 0xff;
		pdf_make_bitmap_image(&image, x, y, w, h);
		y_offset =
		    image.ImageMatrix.ty - (int)(pdev->text.current.y + 0.5);
		if (x < pdev->text.current.x ||
		    y_offset < -max_y_offset || y_offset > max_y_offset
		    )
		    y_offset = 0;
		/*
		 * The Y axis of the text matrix is inverted,
		 * so we need to negate the Y offset appropriately.
		 */
		code = pdf_begin_char_proc(pdev, w, h, 0, y_offset, id,
					   &pcp, &ipos);
		if (code < 0)
		    return code;
		y_offset = -y_offset;
		pprintd3(pdev->strm, "0 0 0 %d %d %d d1\n", y_offset,
			 w, h + y_offset);
		pprintd3(pdev->strm, "%d 0 0 %d 0 %d cm\n", w, h,
			 y_offset);
		code = pdf_begin_write_image(pdev, &writer, true);
		if (code < 0)
		    return code;
		pcp->rid = id;
		pres = (pdf_resource *) pcp;
		goto wr;
	    }
	    pdf_make_bitmap_matrix(&image.ImageMatrix, x, y, w, h);
	    goto rx;
	}
	pdf_set_color(pdev, one, &pdev->fill_color, "rg");
	gs_image_t_init_mask(&image, false);
	invert = 0xff;
    } else if (one == gx_no_color_index) {
	gs_image_t_init_mask(&image, false);
	pdf_set_color(pdev, zero, &pdev->fill_color, "rg");
    } else if (zero == 0 && one == 0xffffff) {
	gs_image_t_init_gray(&image);
    } else if (zero == 0xffffff && one == 0) {
	gs_image_t_init_gray(&image);
	invert = 0xff;
    } else {
	gs_image_t_init_color(&image);
	cs.type = &gs_color_space_type_Indexed;
	cs.params.indexed.hival = 1;
	palette[0] = (byte) (zero >> 16);
	palette[1] = (byte) (zero >> 8);
	palette[2] = (byte) (zero);
	palette[3] = (byte) (one >> 16);
	palette[4] = (byte) (one >> 8);
	palette[5] = (byte) (one);
	cs.params.indexed.lookup.table.data = palette;
	cs.params.indexed.lookup.table.size = 6;
	cs.params.indexed.use_proc = false;
	image.BitsPerComponent = 1;
	image.ColorSpace = &cs;
    }
    pdf_make_bitmap_image(&image, x, y, w, h);
    {
	ulong nbytes = (ulong) ((w + 7) >> 3) * h;
	bool in_line = nbytes <= 4000;

	if (in_line)
	    pdf_put_image_matrix(pdev, &image.ImageMatrix);
	code = pdf_open_page(pdev, pdf_in_stream);
	if (code < 0)
	    return code;
	code = pdf_begin_write_image(pdev, &writer, in_line);
	if (code < 0)
	    return code;
    }
  wr:				/*
				 * There are 3 different cases at this point:
				 *      - Writing an in-line image (pres == 0, writer.pres == 0);
				 *      - Writing an XObject image (pres == 0, writer.pres != 0);
				 *      - Writing the image for a CharProc (pres != 0).
				 * We handle them with in-line code followed by a switch,
				 * rather than making the shared code into a procedure,
				 * simply because there would be an awful lot of parameters
				 * that would need to be passed.
				 */
    {
	char decode_parms[80];

	sprintf(decode_parms,
		"<</K -1/Columns %d%s>>",
		w, (invert ? "" : "/BlackIs1 true"));
	pdf_begin_image_data(pdev, &writer, &image,
			     writer.pin->CCITTFaxDecode, decode_parms);
    }
    {
	stream_CFE_state csstate;

	csstate.template = &s_CFE_template;
	(*csstate.template->set_defaults) ((stream_state *) & csstate);
	csstate.K = -1;
	csstate.Columns = w;
	csstate.Rows = h;
	csstate.BlackIs1 = invert == 0;
	pdf_begin_binary(pdev, &writer.binary, (stream_state *) & csstate);
	for (yi = 0; yi < h; ++yi) {
	    const byte *data = base + yi * raster + (sourcex >> 3);
	    int sbit = sourcex & 7;

	    if (sbit == 0) {
		int nbytes = (w + 7) >> 3;
		int i;

		for (i = 0; i < nbytes; ++data, ++i)
		    sputc(writer.binary.strm, *data ^ invert);
	    } else {
		int wleft = w;
		int rbit = 8 - sbit;

		for (; wleft + sbit > 8; ++data, wleft -= 8)
		    sputc(writer.binary.strm,
			  ((*data << sbit) + (data[1] >> rbit)) ^ invert);
		if (wleft > 0)
		    sputc(writer.binary.strm,
			  ((*data << sbit) ^ invert) &
			  (byte) (0xff00 >> wleft));
	    }
	}
	pdf_end_binary(pdev, &writer.binary);
    }
    if (!pres) {
	switch (pdf_end_write_image(pdev, &writer)) {
	    default:		/* error */
		return code;
	    case 1:
		return 0;
	    case 0:
		return pdf_do_image(pdev, writer.pres, &image.ImageMatrix);
	}
    }
    pputs(pdev->strm, "\nEI\n");
    code = pdf_end_char_proc(pdev, &ipos);
    if (code < 0)
	return code;
  rx:{
	gs_matrix imat;

	imat = image.ImageMatrix;
	imat.xx /= w;
	imat.xy /= h;
	imat.yx /= w;
	imat.yy /= h;
	return pdf_do_char_image(pdev, (const pdf_char_proc *)pres, &imat);
    }
}

/* Copy a color bitmap. */
int
gdev_pdf_copy_color(gx_device * dev,
		const byte * base, int sourcex, int raster, gx_bitmap_id id,
		    int x, int y, int w, int h)
{
    gx_device_pdf *pdev = (gx_device_pdf *) dev;
    int depth = dev->color_info.depth;
    int bytes_per_pixel = depth >> 3;
    int code = pdf_open_page(pdev, pdf_in_stream);
    int yi;
    gs_image_t image;
    gs_color_space cs;
    pdf_image_writer writer;
    ulong nbytes;

    if (code < 0)
	return code;
    if (w <= 0 || h <= 0)
	return 0;
    /* Make sure we aren't being clipped. */
    pdf_put_clip_path(pdev, NULL);
    gs_image_t_init_color(&image);
    pdf_make_bitmap_image(&image, x, y, w, h);
    image.BitsPerComponent = 8;
    cs.type = (bytes_per_pixel == 3 ? &gs_color_space_type_DeviceRGB :
	       bytes_per_pixel == 4 ? &gs_color_space_type_DeviceCMYK :
	       &gs_color_space_type_DeviceGray);
    image.ColorSpace = &cs;
    nbytes = (ulong) w *bytes_per_pixel * h;

    pdf_put_image_matrix(pdev, &image.ImageMatrix);
    code = pdf_begin_write_image(pdev, &writer, nbytes <= 4000);
    if (code < 0)
	return code;
    code = pdf_begin_image_data(pdev, &writer, &image, NULL, NULL);
    if (code < 0)
	return code;
    pdf_begin_binary(pdev, &writer.binary, NULL);
    for (yi = 0; yi < h; ++yi) {
	uint ignore;

	sputs(writer.binary.strm,
	      base + sourcex * bytes_per_pixel + yi * raster,
	      w * bytes_per_pixel, &ignore);
    }
    pdf_end_binary(pdev, &writer.binary);
    code = pdf_end_write_image(pdev, &writer);
    switch (code) {
	default:
	    return code;	/* error */
	case 1:
	    return 0;
	case 0:;
    }
    return pdf_do_image(pdev, writer.pres, &image.ImageMatrix);
}

/* Fill a mask. */
int
gdev_pdf_fill_mask(gx_device * dev,
		 const byte * data, int data_x, int raster, gx_bitmap_id id,
		   int x, int y, int width, int height,
		   const gx_drawing_color * pdcolor, int depth,
		   gs_logical_operation_t lop, const gx_clip_path * pcpath)
{
    gx_device_pdf *pdev = (gx_device_pdf *) dev;
    int code;

    if (width <= 0 || height <= 0)
	return 0;
    if (depth > 1 || !gx_dc_is_pure(pdcolor) != 0)
	return gx_default_fill_mask(dev, data, data_x, raster, id,
				    x, y, width, height, pdcolor, depth, lop,
				    pcpath);
    if (pdf_must_put_clip_path(pdev, pcpath)) {
	code = pdf_open_page(pdev, pdf_in_stream);
	if (code < 0)
	    return code;
	pdf_put_clip_path(pdev, pcpath);
    }
    return gdev_pdf_copy_mono(dev, data, data_x, raster, id,
			      x, y, width, height,
			      gx_no_color_index,
			      gx_dc_pure_color(pdcolor));
}

/* ------ High-level calls ------ */

/* Define the structure for keeping track of progress through an image. */
typedef struct pdf_image_enum_s {
    gs_memory_t *memory;
    void *default_info;
    int width;
    int num_planes;
    int bits_per_pixel;		/* bits per pixel (per plane) */
    int rows_left;
    pdf_image_writer writer;
} pdf_image_enum;

/* We can disregard the pointers in the writer by allocating */
/* the image enumerator as immovable.  This is a hack, of course. */
gs_private_st_ptrs1(st_pdf_image_enum, pdf_image_enum, "pdf_image_enum",
	 pdf_image_enum_enum_ptrs, pdf_image_enum_reloc_ptrs, default_info);

/* Test whether we can handle a given color space. */
private bool
pdf_can_handle_color_space(const gs_color_space * pcs)
{
    gs_color_space_index index = gs_color_space_get_index(pcs);

    if (index == gs_color_space_index_Indexed) {
	if (pcs->params.indexed.use_proc)
	    return false;
	index =
	    gs_color_space_get_index(gs_color_space_indexed_base_space(pcs));
    }
    switch (index) {
	case gs_color_space_index_DeviceGray:
	case gs_color_space_index_DeviceRGB:
	case gs_color_space_index_DeviceCMYK:
	    return true;
	case gs_color_space_index_Separation:
	case gs_color_space_index_Pattern:
	    return false;
/****** OK in PDF 1.2 ******/
	case gs_color_space_index_CIEA:
	    {			/* Check that we can represent this as a CalGray space. */
		const gs_cie_a *pcie = pcs->params.a;
		float expts[3];

		return (pcie->MatrixA.u == 1 && pcie->MatrixA.v == 1 &&
			pcie->MatrixA.w == 1 &&
			pcie->common.MatrixLMN.is_identity &&
			((cie_cache_is_identity(&pcie->caches.DecodeA) &&
			  cie_scalar3_cache_is_exponential(pcie->common.caches.DecodeLMN, expts) &&
			  expts[1] == expts[0] && expts[2] == expts[0]) ||
			 (cie_vector_cache_is_exponential(&pcie->caches.DecodeA, &expts[0]) &&
		     cie_cache3_is_identity(pcie->common.caches.DecodeLMN)))
		    );
	    }
	case gs_color_space_index_CIEABC:
	    {			/* Check that we can represent this as a CalRGB space. */
		const gs_cie_abc *pcie = pcs->params.abc;
		float expts[3];

		return ((cie_cache3_is_identity(pcie->caches.DecodeABC) &&
			 pcie->MatrixABC.is_identity &&
			 cie_scalar3_cache_is_exponential(pcie->common.caches.DecodeLMN, expts)) ||
			(cie_vector3_cache_is_exponential(pcie->caches.DecodeABC, expts) &&
		    cie_cache3_is_identity(pcie->common.caches.DecodeLMN) &&
			 pcie->common.MatrixLMN.is_identity)
		    );
	    }
	default:		/* CIEBasedDEF[G], LL3 spaces */
	    return false;
    }
}

/* Start processing an image. */
int
gdev_pdf_begin_image(gx_device * dev,
		     const gs_imager_state * pis, const gs_image_t * pim,
		     gs_image_format_t format, const gs_int_rect * prect,
	      const gx_drawing_color * pdcolor, const gx_clip_path * pcpath,
		     gs_memory_t * mem, void **pinfo)
{
    gx_device_pdf *pdev = (gx_device_pdf *) dev;
    int code = pdf_open_page(pdev, pdf_in_stream);
    pdf_image_enum *pie;
    const gs_color_space *pcs = pim->ColorSpace;
    int num_components =
    (pim->ImageMask ? 1 : gs_color_space_num_components(pcs));
    gs_int_rect rect;
    ulong nbytes;

    if (code < 0)
	return code;
    if (prect)
	rect = *prect;
    else {
	rect.p.x = rect.p.y = 0;
	rect.q.x = pim->Width, rect.q.y = pim->Height;
    }
    /* See above for why we allocate the enumerator as immovable. */
    pie = gs_alloc_struct_immovable(mem, pdf_image_enum,
				    &st_pdf_image_enum,
				    "pdf_begin_image");
    if (pie == 0)
	return_error(gs_error_VMerror);
    pie->memory = mem;
    *pinfo = pie;
    if ((pim->ImageMask ?
	 (!gx_dc_is_pure(pdcolor) || pim->CombineWithColor) :
	 !pdf_can_handle_color_space(pim->ColorSpace)) ||
	prect
	) {
	int code = gx_default_begin_image(dev, pis, pim, format, prect,
					  pdcolor, pcpath, mem,
					  &pie->default_info);

	if (code < 0)
	    gs_free_object(mem, pie, "pdf_begin_image");
	return code;
    }
    pie->default_info = 0;
    pie->width = rect.q.x - rect.p.x;
    switch (format) {
	case gs_image_format_chunky:
	    pie->num_planes = 1;
	    break;
	case gs_image_format_component_planar:
	    pie->num_planes = num_components;
	    break;
	case gs_image_format_bit_planar:
	    pie->num_planes = num_components * pim->BitsPerComponent;
	    break;
    }
    pie->bits_per_pixel =
	pim->BitsPerComponent * num_components / pie->num_planes;
    pie->rows_left = rect.q.y - rect.p.y;
    pdf_put_clip_path(pdev, pcpath);
    if (pim->ImageMask)
	pdf_set_color(pdev, gx_dc_pure_color(pdcolor), &pdev->fill_color,
		      "rg");
/****** DOESN'T DO COMPRESSION YET ******/
    {
	gs_matrix mat;
	gs_matrix bmat;
	int code;

	pdf_make_bitmap_matrix(&bmat, -rect.p.x, -rect.p.y,
			       pim->Width, pim->Height);
	if ((code = gs_matrix_invert(&pim->ImageMatrix, &mat)) < 0 ||
	    (code = gs_matrix_multiply(&bmat, &mat, &mat)) < 0 ||
	    (code = gs_matrix_multiply(&mat, &ctm_only(pis), &mat)) < 0
	    ) {
	    gs_free_object(mem, pie, "pdf_begin_image");
	    return code;
	}
	pdf_put_image_matrix(pdev, &mat);
    }
    nbytes = (((ulong) pie->width * pie->bits_per_pixel + 7) >> 3) *
	pie->rows_left;
    code = pdf_begin_write_image(pdev, &pie->writer, nbytes <= 4000);
    if (code < 0)
	return code;
    code = pdf_begin_image_data(pdev, &pie->writer, pim, NULL, NULL);
    if (code < 0)
	return code;
    pdf_begin_binary(pdev, &pie->writer.binary, NULL);
    return 0;
}

/* Process the next piece of an image. */
int
gdev_pdf_image_data(gx_device * dev,
      void *info, const byte ** planes, int data_x, uint raster, int height)
{
    pdf_image_enum *pie = info;
    int h = height;
    uint y_offset = 0;
    uint bcount;
    uint ignore;
    int nplanes = pie->num_planes;

#define row_bytes 180		/* must be 0 mod 3, 4, 6, 9 */
    byte row[row_bytes];

    if (pie->default_info)
	return gx_default_image_data(dev, pie->default_info, planes, data_x,
				     raster, height);
    if (h > pie->rows_left)
	h = pie->rows_left;
    pie->rows_left -= h;
    bcount = ((data_x + pie->width) * pie->bits_per_pixel + 7) >> 3;
    for (; h > 0; y_offset += raster, --h) {
	if (nplanes > 1) {	/* Flip the data in blocks before writing. */
	    uint offset = y_offset;
	    uint count = bcount;

	    while (count) {
		uint flip_count = min(count, row_bytes / nplanes);

		image_flip_planes(row, planes, offset, flip_count, nplanes,
				  pie->bits_per_pixel);
		sputs(pie->writer.binary.strm, row, flip_count * nplanes,
		      &ignore);
		count -= flip_count;
		offset += flip_count;
	    }
	} else
	    sputs(pie->writer.binary.strm, planes[0] + y_offset, bcount,
		  &ignore);
    }
    return !pie->rows_left;
#undef row_bytes
}

/* Clean up by releasing the buffers. */
int
gdev_pdf_end_image(gx_device * dev, void *info, bool draw_last)
{
    gx_device_pdf *pdev = (gx_device_pdf *) dev;
    pdf_image_enum *pie = info;
    int code;

    if (pie->default_info)
	code = gx_default_end_image(dev, pie->default_info, draw_last);
    else {
	code = pdf_end_binary(pdev, &pie->writer.binary);
	if (code < 0)
	    return code;
	code = pdf_end_write_image(pdev, &pie->writer);
	switch (code) {
	    default:
		return code;	/* error */
	    case 1:
		return 0;
	    case 0:;
	}
	code = pdf_do_image(pdev, pie->writer.pres, NULL);
    }
    gs_free_object(pie->memory, pie, "pdf_end_image");
    return code;
}
