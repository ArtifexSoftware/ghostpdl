/* Copyright (C) 1989, 1995, 1996, 1997 Aladdin Enterprises.  All rights reserved.

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

/* zimage.c */
/* Image operators */
#include "ghost.h"
#include "errors.h"
#include "oper.h"
#include "estack.h"		/* for image[mask] */
#include "gsstruct.h"
#include "ialloc.h"
#include "igstate.h"
#include "ilevel.h"
#include "store.h"
#include "gscspace.h"
#include "gsmatrix.h"
#include "gsimage.h"
#include "stream.h"
#include "ifilter.h"		/* for stream exception handling */
#include "iimage.h"

/* Define the maximum number of image components. */
/* (We refer to this as M in a number of places below.) */
#ifdef DPNEXT
#  define max_components 5
#else
#  define max_components 4
#endif

/* Forward references */
private int image_setup(P5(gs_image_t * pim, bool multi, os_ptr op, const gs_color_space * pcs, int npop));
private int image_proc_continue(P1(os_ptr));
private int image_file_continue(P1(os_ptr));
private int image_file_buffered_continue(P1(os_ptr));
private int image_string_process(P3(os_ptr, gs_image_enum *, int));
private int image_cleanup(P1(os_ptr));

/* <width> <height> <bits/sample> <matrix> <datasrc> image - */
int
zimage(register os_ptr op)
{
    return zimage_opaque_setup(op, false,
#ifdef DPNEXT
			       false,
#endif
			       NULL, 5);
}

/* <width> <height> <paint_1s> <matrix> <datasrc> imagemask - */
int
zimagemask(register os_ptr op)
{
    gs_image_t image;

    check_type(op[-2], t_boolean);
    gs_image_t_init_mask(&image, op[-2].value.boolval);
    return image_setup(&image, false, op, NULL, 5);
}

/* Common setup for image, colorimage, and alphaimage. */
/* Fills in MultipleDataSources, BitsPerComponent, HasAlpha. */
#ifdef DPNEXT
int
zimage_opaque_setup(os_ptr op, bool multi, bool has_alpha,
		    const gs_color_space * pcs, int npop)
{
    gs_image_t image;

    check_int_leu(op[-2], (level2_enabled ? 12 : 8));	/* bits/sample */
    gs_image_t_init_color(&image);
    image.BitsPerComponent = (int)op[-2].value.intval;
    image.HasAlpha = has_alpha;
    return image_setup(&image, multi, op, pcs, npop);
}
#else
int
zimage_opaque_setup(os_ptr op, bool multi,
		    const gs_color_space * pcs, int npop)
{
    gs_image_t image;

    check_int_leu(op[-2], (level2_enabled ? 12 : 8));	/* bits/sample */
    gs_image_t_init_color(&image);
    image.BitsPerComponent = (int)op[-2].value.intval;
    return image_setup(&image, multi, op, pcs, npop);
}
#endif

/* Common setup for [color]image and imagemask. */
/* Fills in Width, Height, ImageMatrix, ColorSpace. */
private int
image_setup(gs_image_t * pim, bool multi, os_ptr op,
	    const gs_color_space * pcs, int npop)
{
    int code;

    check_type(op[-4], t_integer);	/* width */
    check_type(op[-3], t_integer);	/* height */
    if (op[-4].value.intval < 0 || op[-3].value.intval < 0)
	return_error(e_rangecheck);
    if ((code = read_matrix(op - 1, &pim->ImageMatrix)) < 0)
	return code;
    pim->ColorSpace = pcs;
    pim->Width = (int)op[-4].value.intval;
    pim->Height = (int)op[-3].value.intval;
    return zimage_setup(pim, multi, op, npop);
}

/* Common setup for Level 1 image/imagemask/colorimage, alphaimage, and */
/* the Level 2 dictionary form of image/imagemask. */
int
zimage_setup(const gs_image_t * pim, bool multi, const ref * sources, int npop)
{
    int code;
    gs_image_enum *penum;
    int px;
    const ref *pp;
    int num_sources =
    (multi ? gs_color_space_num_components(pim->ColorSpace) : 1);
    bool must_buffer = false;

    /*
     * We push the following on the estack.  "Optional" values are
     * set to null if not used, so the offsets will be constant.
     *      Control mark,
     *      M data sources (1-M actually used),
     *      M row buffers (only if must_buffer),
     *      current plane index,
     *      current byte in row (only if must_buffer, otherwise 0),
     *      enumeration structure.
     */
#define inumpush (max_components * 2 + 4)
    check_estack(inumpush + 2);	/* stuff above, + continuation + proc */
#ifdef DPNEXT
    if (pim->HasAlpha && multi)
	++num_sources;
#endif
    /*
     * Note that the data sources may be procedures, strings, or (Level
     * 2 only) files.  (The Level 1 reference manual says that Level 1
     * requires procedures, but Adobe Level 1 interpreters also accept
     * strings.)  The sources must all be of the same type.
     *
     * If the sources are files, and two or more are the same file,
     * we must buffer data for each row; otherwise, we can deliver the
     * data directly out of the stream buffers.
     */
    for (px = 0, pp = sources; px < num_sources; px++, pp++) {
	switch (r_type(pp)) {
	    case t_file:
		if (!level2_enabled)
		    return_error(e_typecheck);
		/* Check for aliasing. */
		{
		    int pi;

		    for (pi = 0; pi < px; ++pi)
			if (sources[pi].value.pfile == pp->value.pfile)
			    must_buffer = true;
		}
		/* falls through */
	    case t_string:
		if (r_type(pp) != r_type(sources))
		    return_error(e_typecheck);
		check_read(*pp);
		break;
	    default:
		if (!r_is_proc(sources))
		    return_error(e_typecheck);
		check_proc(*pp);
	}
    }
    if ((penum = gs_image_enum_alloc(imemory, "image_setup")) == 0)
	return_error(e_VMerror);
    code = gs_image_init(penum, pim, multi, igs);
    if (code != 0) {		/* error, or empty image */
	ifree_object(penum, "image_setup");
	if (code >= 0)		/* empty image */
	    pop(npop);
	return code;
    }
    push_mark_estack(es_other, image_cleanup);
    ++esp;
    for (px = 0, pp = sources; px < max_components; esp++, px++, pp++) {
	if (px < num_sources)
	    *esp = *pp;
	else
	    make_null(esp);
	make_null(esp + max_components);	/* buffer */
    }
    esp += max_components + 2;
    make_int(esp - 2, 0);	/* current plane */
    make_int(esp - 1, 0);	/* current byte in row */
    make_istruct(esp, 0, penum);
    switch (r_type(sources)) {
	case t_file:
	    if (must_buffer) {	/* Allocate a buffer for each row. */
		uint size = gs_image_bytes_per_row(penum);

		for (px = 0; px < num_sources; ++px) {
		    byte *sbody = ialloc_string(size, "image_setup");

		    if (sbody == 0) {
			esp -= inumpush;
			image_cleanup(osp);
			return_error(e_VMerror);
		    }
		    make_string(esp - (max_components + 2) + px,
				icurrent_space, size, sbody);
		}
		push_op_estack(image_file_buffered_continue);
	    } else {
		push_op_estack(image_file_continue);
	    }
	    break;
	case t_string:
	    pop(npop);
	    return image_string_process(osp, penum, num_sources);
	default:		/* procedure */
	    push_op_estack(image_proc_continue);
	    *++esp = sources[0];
	    break;
    }
    pop(npop);
    return o_push_estack;
}
/* Continuation for procedure data source. */
private int
image_proc_continue(register os_ptr op)
{
    gs_image_enum *penum = r_ptr(esp, gs_image_enum);
    uint size, used;
    int code;
    int px;
    const ref *pproc;

    if (!r_has_type_attrs(op, t_string, a_read)) {
	check_op(1);
	/* Procedure didn't return a (readable) string.  Quit. */
	esp -= inumpush;
	image_cleanup(op);
	return_error(!r_has_type(op, t_string) ? e_typecheck : e_invalidaccess);
    }
    size = r_size(op);
    if (size == 0)
	code = 1;
    else
	code = gs_image_next(penum, op->value.bytes, size, &used);
    if (code) {			/* Stop now. */
	esp -= inumpush;
	pop(1);
	op = osp;
	image_cleanup(op);
	return (code < 0 ? code : o_pop_estack);
    }
    pop(1);
    px = (int)++(esp[-2].value.intval);
    pproc = esp - (inumpush - 2);
    if (px == max_components || r_has_type(pproc + px, t_null))
	esp[-2].value.intval = px = 0;
    push_op_estack(image_proc_continue);
    *++esp = pproc[px];
    return o_push_estack;
}
/* Continue processing data from an image with file data sources */
/* and no file buffering. */
private int
image_file_continue(os_ptr op)
{
    gs_image_enum *penum = r_ptr(esp, gs_image_enum);
    const ref *pproc = esp - (inumpush - 2);

    for (;;) {
	uint size = max_uint;
	int code;
	int pn, px;
	const ref *pp;

	/*
	 * Do a first pass through the files to ensure that they all
	 * have data available in their buffers, and compute the min
	 * of the available amounts.
	 */

	for (pn = 0, pp = pproc;
	     pn < max_components && !r_has_type(pp, t_null);
	     ++pn, ++pp
	    ) {
	    stream *s = pp->value.pfile;
	    int min_left = sbuf_min_left(s);
	    uint avail;

	    while ((avail = sbufavailable(s)) <= min_left) {
		int next = sgetc(s);

		if (next >= 0) {
		    sputback(s);
		    if (s->end_status == EOFC || s->end_status == ERRC)
			min_left = 0;
		    continue;
		}
		switch (next) {
		    case EOFC:
			break;	/* with avail = 0 */
		    case INTC:
		    case CALLC:
			return
			    s_handle_read_exception(next, pp,
					      NULL, 0, image_file_continue);
		    default:
			/* case ERRC: */
			return_error(e_ioerror);
		}
		break;		/* for EOFC */
	    }
	    /* Note that in the EOF case, we can get here with */
	    /* avail < min_left. */
	    if (avail >= min_left) {
		avail -= min_left;
		if (avail < size)
		    size = avail;
	    } else
		size = 0;
	}

	/* Now pass the min of the available buffered data to */
	/* the image processor. */

	if (size == 0)
	    code = 1;
	else {
	    int pi;
	    uint used;		/* only set for the last plane */

	    for (px = 0, pp = pproc, code = 0; px < pn && !code;
		 ++px, ++pp
		)
		code = gs_image_next(penum, sbufptr(pp->value.pfile),
				     size, &used);
	    /* Now that used has been set, update the streams. */
	    for (pi = 0, pp = pproc; pi < px; ++pi, ++pp)
		sbufskip(pp->value.pfile, used);
	}
	if (code) {
	    esp -= inumpush;
	    image_cleanup(op);
	    return (code < 0 ? code : o_pop_estack);
	}
    }
}
/* Continue processing data from an image with file data sources */
/* and file buffering.  This is similar to the procedure case. */
private int
image_file_buffered_continue(os_ptr op)
{
    gs_image_enum *penum = r_ptr(esp, gs_image_enum);
    const ref *pproc = esp - (inumpush - 2);
    int px = esp[-2].value.intval;
    int dpos = esp[-1].value.intval;
    uint size = gs_image_bytes_per_row(penum);
    int code = 0;

    while (!code) {
	const ref *pp;
	uint avail = size;
	uint used;
	int pi;

	/* Accumulate data until we have a full set of planes. */
	while (px < max_components &&
	       !r_has_type((pp = pproc + px), t_null)
	    ) {
	    const ref *pb = pp + max_components;
	    uint used;
	    int status = sgets(pp->value.pfile, pb->value.bytes,
			       size - dpos, &used);

	    if ((dpos += used) == size)
		dpos = 0, ++px;
	    else
		switch (status) {
		    case EOFC:
			if (dpos < avail)
			    avail = dpos;
			dpos = 0, ++px;
			break;
		    case INTC:
		    case CALLC:
			/* Call out to read from a procedure-based stream. */
			esp[-2].value.intval = px;
			esp[-1].value.intval = dpos;
			return s_handle_read_exception(status, pp,
				     NULL, 0, image_file_buffered_continue);
		    default:
			/*case ERRC: */
			return_error(e_ioerror);
		}
	}
	/* Pass the data to the image processor. */
	if (avail == 0) {
	    code = 1;
	    break;
	}
	for (pi = 0, pp = pproc + max_components;
	     pi < px && !code;
	     ++pi, ++pp
	    )
	    code = gs_image_next(penum, pp->value.bytes, avail, &used);
	/* Reinitialize for the next row. */
	px = dpos = 0;
    }
    esp -= inumpush;
    image_cleanup(op);
    return (code < 0 ? code : o_pop_estack);
}
/* Process data from an image with string data sources. */
/* This never requires callbacks, so it's simpler. */
private int
image_string_process(os_ptr op, gs_image_enum * penum, int num_sources)
{
    int px = 0;

    for (;;) {
	const ref *psrc = esp - (inumpush - 2) + px;
	uint size = r_size(psrc);
	uint used;
	int code;

	if (size == 0)
	    code = 1;
	else
	    code = gs_image_next(penum, psrc->value.bytes, size, &used);
	if (code) {		/* Stop now. */
	    esp -= inumpush;
	    image_cleanup(op);
	    return (code < 0 ? code : o_pop_estack);
	}
	if (++px == num_sources)
	    px = 0;
    }
}
/* Clean up after enumerating an image */
private int
image_cleanup(os_ptr op)
{
    gs_image_enum *penum = r_ptr(esp + inumpush, gs_image_enum);
    const ref *pb;

    /* Free any row buffers, in LIFO order as usual. */
    for (pb = esp + (max_components * 2 + 1);
	 pb >= esp + (max_components + 2);
	 --pb
	)
	if (r_has_type(pb, t_string))
	    gs_free_string(imemory, pb->value.bytes, r_size(pb),
			   "image_cleanup");
    gs_image_cleanup(penum);
    ifree_object(penum, "image_cleanup");
    return 0;
}

/* ------ Initialization procedure ------ */

const op_def zimage_op_defs[] =
{
    {"5image", zimage},
    {"5imagemask", zimagemask},
		/* Internal operators */
    {"1%image_proc_continue", image_proc_continue},
    {"0%image_file_continue", image_file_continue},
    op_def_end(0)
};
