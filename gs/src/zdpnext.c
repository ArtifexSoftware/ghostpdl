/* Copyright (C) 1997 Aladdin Enterprises.  All rights reserved.

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

/* zdpnext.c */
/* NeXT Display PostScript extensions */
#include "ghost.h"
#include "errors.h"
#include "oper.h"
#include "gscspace.h"		/* for iimage.h */
#include "gsmatrix.h"
#include "gsiparam.h"		/* for iimage.h */
#include "gxcvalue.h"
#include "gxsample.h"
#include "ialloc.h"
#include "igstate.h"
#include "iimage.h"

/*
 * Miscellaneous notes:
 *
 * composite / dissolve respect destination clipping (both clip & viewclip),
 *   but ignore source clipping.
 * composite / dissolve must handle overlapping source/destination correctly.
 * compositing converts the source to the destination's color model
 *   (including halftoning if needed).
 */

/*
 * Define the compositing operations.  Eventually this will be moved to
 * a header file in the library.  These values must match the ones in
 * dpsNeXT.h.
 */
typedef enum {
    NX_CLEAR = 0,
    NX_COPY,
    NX_SOVER,
    NX_SIN,
    NX_SOUT,
    NX_SATOP,
    NX_DOVER,
    NX_DIN,
    NX_DOUT,
    NX_DATOP,
    NX_XOR,
    NX_PLUSD,
    NX_HIGHLIGHT,		/* only for compositerect */
    NX_PLUSL,
#define NX_composite_last NX_PLUSL
#define NX_compositerect_last NX_PLUSL
    NX_DISSOLVE			/* fake value used internally for dissolve */
} gs_composite_op_t;

/*
 * Define the parameters for a compositing operation at the library level.
 * Of course this too belongs in the library.
 */
typedef struct gs_composite_params_s {
    gs_composite_op_t cop;
    gx_color_value delta;	/* only for dissolve */
    byte src_value[5 * 2];	/* only if src == 0 or !src_has_alpha */
} gs_composite_params_t;

/* Define a local structure for holding compositing operands. */
typedef struct gs_composite_operands_s {
    double src_rect[4];		/* x, y, width, height */
    gs_state *src_gs;
    double dest_pt[2];		/* x, y */
} gs_composite_operands_t;

/* Imported procedures */
int zimage_multiple(P2(os_ptr op, bool has_alpha));	/* in zcolor1.c */

/* Forward references */
private int composite_operands(P2(gs_composite_operands_t *, os_ptr));
private int rect_param(P2(os_ptr, double[4]));

/* <width> <height> <bits/comp> <matrix> */
/*      <datasrc_0> ... <datasrc_ncomp-1> true <ncomp> alphaimage - */
/*      <datasrc> false <ncomp> alphaimage - */
private int
zalphaimage(register os_ptr op)
{				/* Essentially the whole implementation is shared with colorimage. */
    return zimage_multiple(op, true);
}

/* <srcx> <srcy> <width> <height> <srcgstate|null> <destx> <desty> <op> */
/*   composite - */
private int
zcomposite(register os_ptr op)
{
    gs_composite_operands_t operands;
    int code = composite_operands(&operands, op);
    gs_composite_params_t params;

    if (code < 0)
	return code;
    check_int_leu(*op, NX_composite_last);
    params.cop = (gs_composite_op_t) op->value.intval;
    if (params.cop == NX_HIGHLIGHT)
	return_error(e_rangecheck);
/****** NYI ******/
    return_error(e_undefined);
}

/* <destx> <desty> <width> <height> <op> compositerect - */
private int
zcompositerect(register os_ptr op)
{
    double dest_rect[4];
    gs_composite_params_t params;
    int code = rect_param(op - 1, dest_rect);

    if (code < 0)
	return code;
    check_int_leu(*op, NX_compositerect_last);
    params.cop = (gs_composite_op_t) op->value.intval;
/****** SET src_value ******/
/****** NYI ******/
    return_error(e_undefined);
}

/* <srcx> <srcy> <width> <height> <srcgstate|null> <destx> <desty> <delta> */
/*   dissolve - */
private int
zdissolve(register os_ptr op)
{
    gs_composite_operands_t operands;
    int code = composite_operands(&operands, op);
    gs_composite_params_t params;
    double delta;

    if (code < 0)
	return code;
    code = real_param(op, &delta);
    if (code < 0)
	return code;
    if (delta < 0 || delta > 1)
	return_error(e_rangecheck);
    params.cop = NX_DISSOLVE;
    params.delta = (gx_color_value) (delta * gx_max_color_value);
/****** NYI ******/
    return_error(e_undefined);
}

/* ------ Initialization procedure ------ */

const op_def zdpnext_op_defs[] =
{
    {"7alphaimage", zalphaimage},
    {"8composite", zcomposite},
    {"5compositerect", zcompositerect},
    {"8dissolve", zdissolve},
    op_def_end(0)
};

/* ------ Internal routines ------ */

/* Collect a rect operand. */
private int
rect_param(os_ptr op, double rect[4])
{
    int code = num_params(op, 4, rect);

    if (code < 0)
	return code;
    if (rect[2] < 0)
	rect[0] += rect[2], rect[2] = -rect[2];
    if (rect[3] < 0)
	rect[1] += rect[3], rect[3] = -rect[3];
    return code;
}

/* Collect parameters for a compositing operation. */
private int
composite_operands(gs_composite_operands_t * pco, os_ptr op)
{
    int code = rect_param(op - 4, pco->src_rect);

    if (code < 0 ||
	(code = num_params(op - 1, 2, pco->dest_pt)) < 0
	)
	return code;
    if (r_has_type(op - 3, t_null))
	pco->src_gs = igs;
    else {
	check_stype(op[-3], st_igstate_obj);
	check_read(op[-3]);
	pco->src_gs = igstate_ptr(op - 3);
    }
    return 0;
}

/* ------ Library procedures ------ */

/*
 * Composite two arrays of (premultiplied) pixel values.
 * Legal values of bits_per_value are 1, 2, 4, 8, and 16.
 * src_has_alpha indicates whether or not the source has alpha values;
 * if not, the alpha value is constant (in the parameter structure).
 * Legal values of values_per_pixel are 1-5, including alpha.
 * (The source has one fewer value per pixel if it doesn't have alpha.)
 * src_data = 0 is legal and indicates that the source value is constant,
 * specified in the parameter structure.
 *
 * The current implementation is simple but inefficient.  We'll speed it up
 * later if necessary.
 */
private int
composite_values(byte * dest_data, int dest_x, const byte * src_data, int src_x,
	       int bits_per_value, int values_per_pixel, bool src_has_alpha,
      uint num_pixels, const gs_composite_params_t * pcp, gs_memory_t * mem)
{
    int src_vpp = values_per_pixel - (src_has_alpha ? 0 : 1);
    int bytes_per_value = bits_per_value >> 3;
    int dest_bpp, src_bpp;
    uint highlight_value = (1 << bits_per_value) - 1;
    byte *dbuf = 0;
    byte *sbuf = 0;
    int dx;
    byte *dptr;
    const byte *sptr;

    if (bytes_per_value == 0) {
	/*
	 * Unpack the operands now, and repack the results after the
	 * operation.
	 */
	sample_unpack_proc((*unpack_proc));
	static const bits16 lookup2x2[16] =
	{
#if arch_is_big_endian
#  define map2(a,b) (a * 0x5500 + b * 0x0055)
#else
#  define map2(a,b) (a * 0x0055 + b * 0x5500)
#endif
#define map4x2(a) map2(a,0), map2(a,1), map2(a,2), map2(a,3)
	    map4x2(0), map4x2(1), map4x2(2), map4x2(3)
#undef map2
#undef map4x2
	};
	static const byte lookup4[16] =
	{
	    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
	    0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff
	};
	const sample_lookup_t *lookup;
	int sx;

	switch (bits_per_value) {
	    case 1:
		unpack_proc = sample_unpack_1;
		lookup = sample_lookup_1_identity;
		break;
	    case 2:
		unpack_proc = sample_unpack_2;
		lookup = (const sample_lookup_t *)lookup2x2;
		break;
	    case 4:
		unpack_proc = sample_unpack_4;
		lookup = (const sample_lookup_t *)lookup4;
		break;
	    default:
		return_error(e_rangecheck);
	}
	/*
	 * Because of partial input bytes due to dest_/src_x,
	 * we may need as many as 3 extra pixels in the buffers.
	 */
	dbuf = gs_alloc_bytes(mem, (num_pixels + 3) * values_per_pixel,
			      "composite_values(dbuf)");
	if (src_data)
	    sbuf = gs_alloc_bytes(mem, (num_pixels + 3) * src_vpp,
				  "composite_values(sbuf)");
	if ((sbuf == 0 && src_data) || dbuf == 0) {
	    gs_free_object(mem, sbuf, "composite_values(sbuf)");
	    gs_free_object(mem, dbuf, "composite_values(dbuf)");
	    return_error(e_VMerror);
	}
	dptr = (*unpack_proc) (dbuf, &dx, dest_data, dest_x,
			       ((dest_x + num_pixels) * bits_per_value *
				values_per_pixel + 7) >> 3,
			       lookup, 1);
	if (src_data)
	    sptr = (*unpack_proc) (sbuf, &sx, src_data, src_x,
				   ((src_x + num_pixels) * bits_per_value *
				    src_vpp + 7) >> 3,
				   lookup, 1);
	src_vpp = bytes_per_value = 1;
	dest_bpp = values_per_pixel;
	src_bpp = src_vpp;
	dptr += dx * dest_bpp;
	sptr += sx * src_bpp;
    } else {
	dest_bpp = bytes_per_value * values_per_pixel;
	src_bpp = bytes_per_value * src_vpp;
	dptr = dest_data + dest_x * dest_bpp;
	sptr = src_data + src_x * src_bpp;
    }
    {
#define bits16_get(p) ( ((p)[0] << 8) + (p)[1] )
#define bits16_put(p,v) ( (p)[0] = (byte)((v) >> 8), (p)[1] = (byte)(v) )
	uint max_value = (1 << bits_per_value) - 1;
	uint delta_v = pcp->delta * max_value / gx_max_color_value;
	int src_no_alpha_j;
	uint src_alpha;
	uint x;

	if (src_has_alpha)
	    src_no_alpha_j = -1;
	else {
	    src_alpha =
		(bytes_per_value == 1 ?
		 pcp->src_value[values_per_pixel - 1] :
		 bits16_get(&pcp->src_value[src_bpp - 2]));
	    src_no_alpha_j = 0;
	}
	for (x = 0; x < num_pixels; ++x) {
	    uint dest_alpha;
	    int j;

	    if (!src_data)
		sptr = pcp->src_value;
	    if (bytes_per_value == 1) {
		dest_alpha = dptr[values_per_pixel - 1];
		if (src_has_alpha)
		    src_alpha = sptr[values_per_pixel - 1];
	    } else {
		dest_alpha =
		    bits16_get(&dptr[dest_bpp - 2]);
		if (src_has_alpha)
		    src_alpha =
			bits16_get(&sptr[src_bpp - 2]);
	    }
#define fr(v, a) ((v) * (a) / max_value)
#define nfr(v, a) ((v) * (max_value - (a)) / max_value)
	    for (j = values_per_pixel; --j != 0;) {
		uint dest_v, src_v, result;

#define set_clamped(r, v) if ( (r = (v)) > max_value ) r = max_value

		if (bytes_per_value == 1) {
		    dest_v = *dptr;
		    src_v = (j == src_no_alpha_j ? src_alpha : *sptr++);
		} else {
		    dest_v = bits16_get(dptr);
		    if (j == src_no_alpha_j)
			src_v = src_alpha;
		    else
			src_v = bits16_get(sptr), sptr += 2;
		}
		switch (pcp->cop) {
		    case NX_CLEAR:
			result = 0;
			break;
		    case NX_COPY:
			result = src_v;
			break;
		    case NX_PLUSD:
			/*
			 * This is the only case where we have to worry about
			 * clamping a possibly negative result.
			 */
			result = src_v + dest_v;
			result = (result < max_value ? 0 : result - max_value);
			break;
		    case NX_PLUSL:
			set_clamped(result, src_v + dest_v);
			break;
		    case NX_SOVER:
			set_clamped(result, src_v + nfr(dest_v, src_alpha));
			break;
		    case NX_DOVER:
			set_clamped(result, nfr(src_v, dest_alpha) + dest_v);
			break;
		    case NX_SIN:
			result = fr(src_v, dest_alpha);
			break;
		    case NX_DIN:
			result = fr(dest_v, src_alpha);
			break;
		    case NX_SOUT:
			result = nfr(src_v, dest_alpha);
			break;
		    case NX_DOUT:
			result = nfr(dest_v, src_alpha);
			break;
		    case NX_SATOP:
			set_clamped(result, fr(src_v, dest_alpha) +
				    nfr(dest_v, src_alpha));
			break;
		    case NX_DATOP:
			set_clamped(result, nfr(src_v, dest_alpha) +
				    fr(dest_v, src_alpha));
			break;
		    case NX_XOR:
			set_clamped(result, nfr(src_v, dest_alpha) +
				    nfr(dest_v, src_alpha));
			break;
		    case NX_HIGHLIGHT:
			/*
			 * Bizarre but true: this operation converts white and
			 * light gray into each other, and leaves all other values
			 * unchanged.  We only implement it properly for gray-scale
			 * devices.
			 */
			if (j != 0 && !((src_v ^ highlight_value) & ~1))
			    result = src_v ^ 1;
			else
			    result = src_v;
			break;
		    case NX_DISSOLVE:
			result = fr(src_v, delta_v) + nfr(dest_v, delta_v);
			break;
		    default:
			return_error(e_rangecheck);
		}
		if (bytes_per_value == 1)
		    *dptr++ = (byte) result;
		else
		    bits16_put(dptr, result), dptr += 2;
	    }
	}
    }
    if (dbuf) {
	/*
	 * Pack pixels back into the destination.  This is modeled on
	 * the line_accum macros in gxcindex.h.
	 */
/****** TBC ******/
	gs_free_object(mem, sbuf, "composite_values(sbuf)");
	gs_free_object(mem, dbuf, "composite_values(dbuf)");
    }
    return 0;
}
