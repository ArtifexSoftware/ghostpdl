/* Copyright (C) 1997, 2000 Aladdin Enterprises.  All rights reserved.

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

/*$Id$ */
/* Common utilities for PostScript and PDF writers */
#include "memory_.h"
#include <stdlib.h>		/* for qsort */
#include "gx.h"
#include "gserrors.h"
#include "gdevpsdf.h"
#include "gxfont.h"
#include "scanchar.h"
#include "strimpl.h"
#include "sa85x.h"
#include "scfx.h"
#include "spprint.h"
#include "sstring.h"

/* Structure descriptors */
public_st_device_psdf();
public_st_psdf_binary_writer();

/* Standard color command names. */
const psdf_set_color_commands_t psdf_set_fill_color_commands = {
    "g", "rg", "k", "cs", "sc", "scn"
};
const psdf_set_color_commands_t psdf_set_stroke_color_commands = {
    "G", "RG", "K", "CS", "SC", "SCN"
};

/* ---------------- Vector implementation procedures ---------------- */

int
psdf_setlinewidth(gx_device_vector * vdev, floatp width)
{
    pprintg1(gdev_vector_stream(vdev), "%g w\n", width);
    return 0;
}

int
psdf_setlinecap(gx_device_vector * vdev, gs_line_cap cap)
{
    pprintd1(gdev_vector_stream(vdev), "%d J\n", cap);
    return 0;
}

int
psdf_setlinejoin(gx_device_vector * vdev, gs_line_join join)
{
    pprintd1(gdev_vector_stream(vdev), "%d j\n", join);
    return 0;
}

int
psdf_setmiterlimit(gx_device_vector * vdev, floatp limit)
{
    pprintg1(gdev_vector_stream(vdev), "%g M\n", limit);
    return 0;
}

int
psdf_setdash(gx_device_vector * vdev, const float *pattern, uint count,
	     floatp offset)
{
    stream *s = gdev_vector_stream(vdev);
    int i;

    pputs(s, "[ ");
    for (i = 0; i < count; ++i)
	pprintg1(s, "%g ", pattern[i]);
    pprintg1(s, "] %g d\n", offset);
    return 0;
}

int
psdf_setflat(gx_device_vector * vdev, floatp flatness)
{
    pprintg1(gdev_vector_stream(vdev), "%g i\n", flatness);
    return 0;
}

int
psdf_setlogop(gx_device_vector * vdev, gs_logical_operation_t lop,
	      gs_logical_operation_t diff)
{
/****** SHOULD AT LEAST DETECT SET-0 & SET-1 ******/
    return 0;
}

int
psdf_setfillcolor(gx_device_vector * vdev, const gx_drawing_color * pdc)
{
    return psdf_set_color(vdev, pdc, &psdf_set_fill_color_commands);
}

int
psdf_setstrokecolor(gx_device_vector * vdev, const gx_drawing_color * pdc)
{
    return psdf_set_color(vdev, pdc, &psdf_set_stroke_color_commands);
}

int
psdf_dorect(gx_device_vector * vdev, fixed x0, fixed y0, fixed x1, fixed y1,
	    gx_path_type_t type)
{
    int code = (*vdev_proc(vdev, beginpath)) (vdev, type);

    if (code < 0)
	return code;
    pprintg4(gdev_vector_stream(vdev), "%g %g %g %g re\n",
	     fixed2float(x0), fixed2float(y0),
	     fixed2float(x1 - x0), fixed2float(y1 - y0));
    return (*vdev_proc(vdev, endpath)) (vdev, type);
}

int
psdf_beginpath(gx_device_vector * vdev, gx_path_type_t type)
{
    return 0;
}

int
psdf_moveto(gx_device_vector * vdev, floatp x0, floatp y0, floatp x, floatp y,
	    gx_path_type_t type)
{
    pprintg2(gdev_vector_stream(vdev), "%g %g m\n", x, y);
    return 0;
}

int
psdf_lineto(gx_device_vector * vdev, floatp x0, floatp y0, floatp x, floatp y,
	    gx_path_type_t type)
{
    pprintg2(gdev_vector_stream(vdev), "%g %g l\n", x, y);
    return 0;
}

int
psdf_curveto(gx_device_vector * vdev, floatp x0, floatp y0,
	   floatp x1, floatp y1, floatp x2, floatp y2, floatp x3, floatp y3,
	     gx_path_type_t type)
{
    if (x1 == x0 && y1 == y0)
	pprintg4(gdev_vector_stream(vdev), "%g %g %g %g v\n",
		 x2, y2, x3, y3);
    else if (x3 == x2 && y3 == y2)
	pprintg4(gdev_vector_stream(vdev), "%g %g %g %g y\n",
		 x1, y1, x2, y2);
    else
	pprintg6(gdev_vector_stream(vdev), "%g %g %g %g %g %g c\n",
		 x1, y1, x2, y2, x3, y3);
    return 0;
}

int
psdf_closepath(gx_device_vector * vdev, floatp x0, floatp y0,
	       floatp x_start, floatp y_start, gx_path_type_t type)
{
    pputs(gdev_vector_stream(vdev), "h\n");
    return 0;
}

/* endpath is deliberately omitted. */

/* ---------------- Utilities ---------------- */

gx_color_index
psdf_adjust_color_index(gx_device_vector *vdev, gx_color_index color)
{
    /*
     * Since gx_no_color_index is all 1's, we can't represent
     * a CMYK color consisting of full ink in all 4 components.
     * However, this color must be available for registration marks.
     * gxcmap.c fudges this by changing the K component to 254;
     * undo this fudge here.
     */
    return (color == (gx_no_color_index ^ 1) ? gx_no_color_index : color);
}

int
psdf_set_color(gx_device_vector * vdev, const gx_drawing_color * pdc,
	       const psdf_set_color_commands_t *ppscc)
{
    if (!gx_dc_is_pure(pdc))
	return_error(gs_error_rangecheck);
    {
	stream *s = gdev_vector_stream(vdev);
	gx_color_index color =
	    psdf_adjust_color_index(vdev, gx_dc_pure_color(pdc));
	float
	    v0 = (color >> 24) / 255.0,
	    v1 = ((color >> 16) & 0xff) / 255.0,
	    v2 = ((color >> 8) & 0xff) / 255.0,
	    v3 = (color & 0xff) / 255.0;

	switch (vdev->color_info.num_components) {
	case 4:
	    if (v0 == 0 && v1 == 0 && v2 == 0) {
		v3 = 1.0 - v3;
		goto g;
	    }
	    pprintg4(s, "%g %g %g %g", v0, v1, v2, v3);
	    pprints1(s, " %s\n", ppscc->setcmykcolor);
	    break;
	case 3:
	    if (v1 == v2 && v2 == v3)
		goto g;
	    pprintg3(s, "%g %g %g", v1, v2, v3);
	    pprints1(s, " %s\n", ppscc->setrgbcolor);
	    break;
	case 1:
	g:
	    pprintg1(s, "%g", v3);
	    pprints1(s, " %s\n", ppscc->setgray);
	    break;
	default:		/* can't happen */
	    return_error(gs_error_rangecheck);
	}
    }
    return 0;
}

/* ---------------- Binary data writing ---------------- */

/* Begin writing binary data. */
int
psdf_begin_binary(gx_device_psdf * pdev, psdf_binary_writer * pbw)
{
    gs_memory_t *mem = pbw->memory = pdev->v_memory;

    pbw->target = pdev->strm;
    pbw->dev = pdev;
    /* If not binary, set up the encoding stream. */
    if (!pdev->binary_ok) {
#define BUF_SIZE 100		/* arbitrary */
	byte *buf = gs_alloc_bytes(mem, BUF_SIZE, "psdf_begin_binary(buf)");
	stream_A85E_state *ss = (stream_A85E_state *)
	    s_alloc_state(mem, s_A85E_template.stype,
			  "psdf_begin_binary(stream_state)");
	stream *s = s_alloc(mem, "psdf_begin_binary(stream)");

	if (buf == 0 || ss == 0 || s == 0) {
	    gs_free_object(mem, s, "psdf_begin_binary(stream)");
	    gs_free_object(mem, ss, "psdf_begin_binary(stream_state)");
	    gs_free_object(mem, buf, "psdf_begin_binary(buf)");
	    return_error(gs_error_VMerror);
	}
	ss->template = &s_A85E_template;
	s_init_filter(s, (stream_state *)ss, buf, BUF_SIZE, pdev->strm);
#undef BUF_SIZE
	pbw->strm = pbw->A85E = s;
    } else {
	pbw->A85E = 0;		/* for GC */
	pbw->strm = pdev->strm;
    }
    return 0;
}

/* Add an encoding filter.  The client must have allocated the stream state, */
/* if any, using pdev->v_memory. */
int
psdf_encode_binary(psdf_binary_writer * pbw, const stream_template * template,
		   stream_state * ss)
{
    return (s_add_filter(&pbw->strm, template, ss, pbw->memory) == 0 ?
	    gs_note_error(gs_error_VMerror) : 0);
}

/* Add a 2-D CCITTFax encoding filter. */
/* Set EndOfBlock iff the stream is not ASCII85 encoded. */
int
psdf_CFE_binary(psdf_binary_writer * pbw, int w, int h, bool invert)
{
    gs_memory_t *mem = pbw->memory;
    const stream_template *template = &s_CFE_template;
    stream_CFE_state *st =
	gs_alloc_struct(mem, stream_CFE_state, template->stype,
			"psdf_CFE_binary");
    int code;

    if (st == 0)
	return_error(gs_error_VMerror);
    (*template->set_defaults) ((stream_state *) st);
    st->K = -1;
    st->Columns = w;
    st->Rows = 0;
    st->BlackIs1 = !invert;
    st->EndOfBlock = pbw->strm->state->template != &s_A85E_template;
    code = psdf_encode_binary(pbw, template, (stream_state *) st);
    if (code < 0)
	gs_free_object(mem, st, "psdf_CFE_binary");
    return code;
}

/* Finish writing binary data. */
int
psdf_end_binary(psdf_binary_writer * pbw)
{
    int code = s_close_filters(&pbw->strm, pbw->target);

    /* s_close_filters freed the A85E stream, if any. */
    pbw->A85E = 0;		/* for GC */
    return code;
}
