/* Copyright (C) 1993, 1996, 1997 Aladdin Enterprises.  All rights reserved.

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

/* zchar1.c */
/* Type 1 character display operator */
#include "ghost.h"
#include "errors.h"
#include "oper.h"
#include "gsstruct.h"
#include "gxfixed.h"
#include "gxmatrix.h"
#include "gxchar.h"		/* for gs_type1_init in gstype1.h */
				/* (should only be gschar.h) */
#include "gxdevice.h"		/* for gxfont.h */
#include "gxfont.h"
#include "gxfont1.h"
#include "gxtype1.h"
#include "gzstate.h"		/* for path for gs_type1_init */
				/* (should only be gsstate.h) */
#include "gspaint.h"		/* for gs_fill, gs_stroke */
#include "gspath.h"
#include "estack.h"
#include "ialloc.h"
#include "ichar.h"
#include "icharout.h"
#include "idict.h"
#include "ifont.h"
#include "igstate.h"
#include "store.h"

/* Test whether a font is Type 1 compatible. */
#define font_is_type1_compatible(pfont)\
  ((pfont)->FontType == ft_encrypted || (pfont)->FontType == ft_disk_based)

/* ---------------- .type1execchar ---------------- */

/*
 * This is the workhorse for %Type1BuildChar, %Type1BuildGlyph,
 * CCRun, and CID fonts.  Eventually this will appear in the C API;
 * even now, its normal control path doesn't use any continuations.
 */

/*
 * Define the state record for this operator, which must save the metrics
 * separately as well as the Type 1 interpreter state.
 */
typedef struct gs_type1exec_state_s {
    gs_type1_state cis;		/* must be first */
    double sbw[4];
    int /*metrics_present */ present;
    gs_rect char_bbox;
} gs_type1exec_state;

gs_private_st_suffix_add0(st_gs_type1exec_state, gs_type1exec_state,
			  "gs_type1exec_state", gs_type1exec_state_enum_ptrs,
			  gs_type1exec_state_reloc_ptrs, st_gs_type1_state);

/* Forward references */
private int bbox_continue(P1(os_ptr));
private int nobbox_continue(P1(os_ptr));
private int type1_call_OtherSubr(P3(const gs_type1exec_state *,
				    int (*)(P1(os_ptr)), const ref *));
private int type1_continue_dispatch(P3(gs_type1_state *, const ref *, ref *));
private int type1_callout_dispatch(P2(os_ptr, int (*)(P1(os_ptr))));
private int op_type1_cleanup(P1(os_ptr));
private void op_type1_free(P1(os_ptr));
private void
     type1_cis_get_metrics(P2(const gs_type1_state * pcis, double psbw[4]));
private int bbox_getsbw_continue(P1(os_ptr));
private int type1exec_bbox(P3(os_ptr, gs_type1exec_state *, gs_font *));
private int bbox_fill(P1(os_ptr));
private int bbox_stroke(P1(os_ptr));
private int nobbox_finish(P2(os_ptr, gs_type1exec_state *));
private int nobbox_fill(P1(os_ptr));
private int nobbox_stroke(P1(os_ptr));
private int
/* <font> <code|name> <name> <charstring> .type1execchar - */
ztype1execchar(register os_ptr op)
{
    gs_font *pfont;

#define pbfont ((gs_font_base *)pfont)
#define pfont1 ((gs_font_type1 *)pfont)
    const gs_type1_data *pdata;
    int code = font_param(op - 3, &pfont);
    gs_show_enum *penum = op_show_find();
    gs_type1exec_state cxs;

#define pcis (&cxs.cis)

    if (code < 0)
	return code;
    if (penum == 0 || !font_is_type1_compatible(pfont))
	return_error(e_undefined);
    pdata = &pfont1->data;
    /*
     * Any reasonable implementation would execute something like
     *      1 setmiterlimit 0 setlinejoin 0 setlinecap
     * here, but apparently the Adobe implementations aren't reasonable.
     *
     * If this is a stroked font, set the stroke width.
     */
    if (pfont->PaintType)
	gs_setlinewidth(igs, pfont->StrokeWidth);
    check_estack(3);		/* for continuations */
    /*
     * Execute the definition of the character.
     */
    if (r_is_proc(op))
	return zchar_exec_char_proc(op);
    /*
     * The definition must be a Type 1 CharString.
     * Note that we do not require read access: this is deliberate.
     */
    check_type(*op, t_string);
    if (r_size(op) <= max(pdata->lenIV, 0))
	return_error(e_invalidfont);
    /*
     * In order to make character oversampling work, we must
     * set up the cache before calling .type1addpath.
     * To do this, we must get the bounding box from the FontBBox,
     * and the width from the CharString or the Metrics.
     * If the FontBBox isn't valid, we can't do any of this.
     */
    code = zchar_get_metrics(pbfont, op - 1, cxs.sbw);
    if (code < 0)
	return code;
    cxs.present = code;
    /* Establish a current point. */
    code = gs_moveto(igs, 0.0, 0.0);
    if (code < 0)
	return code;
    code = gs_type1_init(pcis, penum, NULL,
			 gs_show_in_charpath(penum) != cpm_show,
			 pfont1->PaintType, pfont1);
    if (code < 0)
	return code;
    if (pfont1->FontBBox.q.x > pfont1->FontBBox.p.x &&
	pfont1->FontBBox.q.y > pfont1->FontBBox.p.y
	) {			/* The FontBBox is valid. */
	cxs.char_bbox = pfont1->FontBBox;
	return type1exec_bbox(op, &cxs, pfont);
    } else {			/*
				 * The FontBBox is not valid.  In this case,
				 * we create the path first, then do the setcachedevice.
				 * If we are oversampling (in this case, only for anti-
				 * aliasing, not just to improve quality), we have to
				 * create the path twice, since we can't know the
				 * oversampling factor until after setcachedevice.
				 */
	const ref *opstr = op;
	ref other_subr;

	if (cxs.present == metricsSideBearingAndWidth) {
	    gs_point sbpt;

	    sbpt.x = cxs.sbw[0], sbpt.y = cxs.sbw[1];
	    gs_type1_set_lsb(pcis, &sbpt);
	}
	/* Continue interpreting. */
      icont:code = type1_continue_dispatch(pcis, opstr, &other_subr);
	switch (code) {
	    case 0:		/* all done */
		return nobbox_finish(op, &cxs);
	    default:		/* code < 0, error */
		return code;
	    case type1_result_callothersubr:	/* unknown OtherSubr */
		return type1_call_OtherSubr(&cxs, nobbox_continue,
					    &other_subr);
	    case type1_result_sbw:	/* [h]sbw, just continue */
		if (cxs.present != metricsSideBearingAndWidth)
		    type1_cis_get_metrics(pcis, cxs.sbw);
		opstr = 0;
		goto icont;
	}
    }
#undef pcis
#undef pfont1
#undef pbfont
}
/* Do all the work for the case where we have a bounding box. */
private int
type1exec_bbox(os_ptr op, gs_type1exec_state * pcxs, gs_font * pfont)
{
#define pcis (&pcxs->cis)
#define pbfont ((gs_font_base *)pfont)
    /*
     * We have a valid bounding box.  If we don't have Metrics
     * for this character, start interpreting the CharString;
     * do the setcachedevice as soon as we know the
     * (side bearing and) width.
     */
    if (pcxs->present == metricsNone) {		/* Get the width from the CharString, */
	/* then set the cache device. */
	ref cnref;
	ref other_subr;
	int code;

	/* Since an OtherSubr callout might change osp, */
	/* save the character name now. */
	ref_assign(&cnref, op - 1);
	code = type1_continue_dispatch(pcis, op, &other_subr);
	switch (code) {
	    default:		/* code < 0 or done, error */
		return ((code < 0 ? code :
			 gs_note_error(e_invalidfont)));
	    case type1_result_callothersubr:	/* unknown OtherSubr */
		return type1_call_OtherSubr(pcxs,
					    bbox_getsbw_continue,
					    &other_subr);
	    case type1_result_sbw:	/* [h]sbw, done */
		break;
	}
	type1_cis_get_metrics(pcis, pcxs->sbw);
	return zchar_set_cache(osp, pbfont, &cnref,
			       NULL, pcxs->sbw + 2,
			       &pcxs->char_bbox,
			       bbox_fill, bbox_stroke);
    } else {			/* We have the width and bounding box: */
	/* set up the cache device now. */
	return zchar_set_cache(op, pbfont, op - 1,
			       (pcxs->present ==
				metricsSideBearingAndWidth ?
				pcxs->sbw : NULL),
			       pcxs->sbw + 2,
			       &pcxs->char_bbox,
			       bbox_fill, bbox_stroke);
    }
#undef pcis
#undef pbfont
}


/* Handle the results of gs_type1_interpret. */
/* pcref points to a t_string ref. */
private int
type1_continue_dispatch(gs_type1_state * pcis, const ref * pcref, ref * pos)
{
    int value;
    int code;
    gs_const_string charstring;
    gs_const_string *pchars;

    if (pcref == 0) {
	pchars = 0;
    } else {
	charstring.data = pcref->value.const_bytes;
	charstring.size = r_size(pcref);
	pchars = &charstring;
    }
    code = gs_type1_interpret(pcis, pchars, &value);
    switch (code) {
	case type1_result_callothersubr:
	    {			/* The Type 1 interpreter handles all known OtherSubrs, */
		/* so this must be an unknown one. */
		const font_data *pfdata = pfont_data(gs_currentfont(igs));

		code = array_get(&pfdata->u.type1.OtherSubrs,
				 (long)value, pos);
		return (code < 0 ? code : type1_result_callothersubr);
	    }
    }
    return code;
}

/* Do a callout to an OtherSubr implemented in PostScript. */
/* The caller must have done a check_estack(4). */
private int
type1_call_OtherSubr(const gs_type1exec_state * pcxs, int (*cont) (P1(os_ptr)),
		     const ref * pos)
{				/* Move the Type 1 interpreter state to the heap. */
    gs_type1exec_state *hpcxs = ialloc_struct(gs_type1exec_state,
					      &st_gs_type1exec_state,
					      "type1_call_OtherSubr");

    if (hpcxs == 0)
	return_error(e_VMerror);
    *hpcxs = *pcxs;
    push_mark_estack(es_show, op_type1_cleanup);
    ++esp;
    make_istruct(esp, 0, hpcxs);
    push_op_estack(cont);
    ++esp;
    *esp = *pos;
    return o_push_estack;
}

/* Continue from an OtherSubr callout while getting metrics. */
private int
bbox_getsbw_continue(os_ptr op)
{
    ref other_subr;
    gs_type1exec_state *pcxs = r_ptr(esp, gs_type1exec_state);

#define pcis (&pcxs->cis)
    int code;

    code = type1_continue_dispatch(pcis, NULL, &other_subr);
    op = osp;			/* in case z1_push/pop_proc was called */
    switch (code) {
	default:		/* code < 0 or done, error */
	    op_type1_free(op);
	    return ((code < 0 ? code : gs_note_error(e_invalidfont)));
	case type1_result_callothersubr:	/* unknown OtherSubr */
	    push_op_estack(bbox_getsbw_continue);
	    ++esp;
	    *esp = other_subr;
	    return o_push_estack;
	case type1_result_sbw:	/* [h]sbw, done */
	    {
		double sbw[4];
		const gs_font_base *pbfont =
		(const gs_font_base *)pcis->pfont;
		gs_rect bbox;

		/* Get the metrics before freeing the state. */
		type1_cis_get_metrics(pcis, sbw);
		bbox = pcxs->char_bbox;
		op_type1_free(op);
		return zchar_set_cache(op, pbfont, op, sbw, sbw + 2, &bbox,
				       bbox_fill, bbox_stroke);
	    }
    }
#undef pcis
}

/* <font> <code|name> <name> <charstring> <sbx> <sby> %bbox_{fill|stroke} - */
/* <font> <code|name> <name> <charstring> %bbox_{fill|stroke} - */
private int bbox_finish(P2(os_ptr, int (*)(P1(os_ptr))));
private int
bbox_fill(os_ptr op)
{
    return bbox_finish(op, nobbox_fill);
}
private int
bbox_stroke(os_ptr op)
{
    return bbox_finish(op, nobbox_stroke);
}
private int
bbox_finish(os_ptr op, int (*cont) (P1(os_ptr)))
{
    gs_font *pfont;

#define pfont1 ((gs_font_type1 *)pfont)
    int code;
    gs_show_enum *penum = op_show_find();
    gs_type1exec_state cxs;	/* stack allocate to avoid sandbars */

#define pcis (&cxs.cis)
    double sbxy[2];
    gs_point sbpt;
    gs_point *psbpt = 0;
    os_ptr opc = op;
    const ref *opstr;
    ref other_subr;

    if (!r_has_type(opc, t_string)) {
	check_op(3);
	code = num_params(op, 2, sbxy);
	if (code < 0)
	    return code;
	sbpt.x = sbxy[0];
	sbpt.y = sbxy[1];
	psbpt = &sbpt;
	opc -= 2;
	check_type(*opc, t_string);
    }
    code = font_param(opc - 3, &pfont);
    if (code < 0)
	return code;
    if (penum == 0 || !font_is_type1_compatible(pfont))
	return_error(e_undefined);
    {
	int lenIV = pfont1->data.lenIV;

	if (lenIV > 0 && r_size(opc) <= lenIV)
	    return_error(e_invalidfont);
    }
    check_estack(5);		/* in case we need to do a callout */
    code = gs_type1_init(pcis, penum, psbpt,
			 gs_show_in_charpath(penum) != cpm_show,
			 pfont1->PaintType, pfont1);
    if (code < 0)
	return code;
    opstr = opc;
  icont:code = type1_continue_dispatch(pcis, opstr, &other_subr);
    switch (code) {
	case 0:		/* all done */
	    /* Call the continuation now. */
	    if (psbpt)
		pop(2);
	    return (*cont) (osp);
	case type1_result_callothersubr:	/* unknown OtherSubr */
	    push_op_estack(cont);	/* call later */
	    return type1_call_OtherSubr(&cxs, bbox_continue,
					&other_subr);
	case type1_result_sbw:	/* [h]sbw, just continue */
	    opstr = 0;
	    goto icont;
	default:		/* code < 0, error */
	    return code;
    }
#undef pfont1
#undef pcis
}

/* Continue from an OtherSubr callout while building the path. */
private int
type1_callout_dispatch(os_ptr op, int (*cont) (P1(os_ptr)))
{
    ref other_subr;
    gs_type1exec_state *pcxs = r_ptr(esp, gs_type1exec_state);

#define pcis (&pcxs->cis)
    int code;

  icont:code = type1_continue_dispatch(pcis, NULL, &other_subr);
    op = osp;			/* in case z1_push/pop_proc was called */
    switch (code) {
	case 0:		/* callout done, cont is on e-stack */
	    return 0;
	default:		/* code < 0 or done, error */
	    op_type1_free(op);
	    return ((code < 0 ? code : gs_note_error(e_invalidfont)));
	case type1_result_callothersubr:	/* unknown OtherSubr */
	    push_op_estack(cont);
	    ++esp;
	    *esp = other_subr;
	    return o_push_estack;
	case type1_result_sbw:	/* [h]sbw, just continue */
	    goto icont;
    }
#undef pcis
}
private int
bbox_continue(os_ptr op)
{
    int code = type1_callout_dispatch(op, bbox_continue);

    if (code == 0) {		/* Assume the OtherSubr(s) didn't mess with the o-stack.... */
	int npop = (r_has_type(op, t_string) ? 4 : 6);

	pop(npop);
	op -= npop;
	op_type1_free(op);
    }
    return code;
}
private int
nobbox_continue(os_ptr op)
{
    int code = type1_callout_dispatch(op, nobbox_continue);

    if (code)
	return code;
    {
	gs_type1exec_state cxs;
	gs_type1exec_state *pcxs = r_ptr(esp, gs_type1exec_state);

	cxs = *pcxs;
	op_type1_free(op);
	return nobbox_finish(op, &cxs);
    }
}

/* Clean up after a Type 1 callout. */
private int
op_type1_cleanup(os_ptr op)
{
    ifree_object(r_ptr(esp + 2, void), "op_type1_cleanup");

    return 0;
}
private void
op_type1_free(os_ptr op)
{
    ifree_object(r_ptr(esp, void), "op_type1_free");

    /*
     * In order to avoid popping from the e-stack and then pushing onto
     * it, which would violate an interpreter invariant, we simply
     * overwrite the two e-stack items being discarded (hpcxs and the
     * cleanup operator) with empty procedures.
     */
    make_empty_const_array(esp - 1, a_readonly + a_executable);
    make_empty_const_array(esp, a_readonly + a_executable);
}

/* Finish the no-FontBBox case after constructing the path. */
/* If we are oversampling for anti-aliasing, we have to go around again. */
/* <font> <code|name> <name> <charstring> %nobbox_continue - */
private int
nobbox_finish(os_ptr op, gs_type1exec_state * pcxs)
{
    int code;
    gs_show_enum *penum = op_show_find();
    gs_font *pfont;

#define pbfont ((gs_font_base *)pfont)
#define pfont1 ((gs_font_type1 *)pfont)

    if ((code = gs_pathbbox(igs, &pcxs->char_bbox)) < 0 ||
	(code = font_param(op - 3, &pfont)) < 0
	)
	return code;
    if (penum == 0 || !font_is_type1_compatible(pfont))
	return_error(e_undefined);
    if (pcxs->present == metricsNone) {
	gs_point endpt;

	if ((code = gs_currentpoint(igs, &endpt)) < 0)
	    return code;
	pcxs->sbw[2] = endpt.x, pcxs->sbw[3] = endpt.y;
	pcxs->present = metricsSideBearingAndWidth;
    }
    /*
     * We only need to rebuild the path from scratch if we might
     * oversample for anti-aliasing.
     */
    if ((*dev_proc(igs->device, get_alpha_bits))
	(igs->device, go_text) > 1
	) {
	gs_newpath(igs);
	gs_moveto(igs, 0.0, 0.0);
	code = gs_type1_init(&pcxs->cis, penum, NULL,
			     gs_show_in_charpath(penum) != cpm_show,
			     pfont1->PaintType, pfont1);
	if (code < 0)
	    return code;
	return type1exec_bbox(op, pcxs, pfont);
    }
    return zchar_set_cache(op, pbfont, op, NULL, pcxs->sbw + 2,
			   &pcxs->char_bbox,
			   nobbox_fill, nobbox_stroke);
#undef pbfont
}
/* Finish by popping the operands and filling or stroking. */
private int
nobbox_fill(os_ptr op)
{
    pop(4);
    /*
     * Properly designed fonts, which have no self-intersecting outlines
     * and in which outer and inner outlines are drawn in opposite
     * directions, aren't affected by choice of filling rule; but some
     * badly designed fonts in the Genoa test suite seem to require
     * using the even-odd rule to match Adobe interpreters.
     */
    return gs_eofill(igs);
}
private int
nobbox_stroke(os_ptr op)
{
    pop(4);
    return gs_stroke(igs);
}

/* ------ Internal procedures ------ */

/* Get the metrics (l.s.b. and width) from the Type 1 interpreter. */
private void
type1_cis_get_metrics(const gs_type1_state * pcis, double psbw[4])
{
    psbw[0] = fixed2float(pcis->lsb.x);
    psbw[1] = fixed2float(pcis->lsb.y);
    psbw[2] = fixed2float(pcis->width.x);
    psbw[3] = fixed2float(pcis->width.y);
}

/* ------ Initialization procedure ------ */

const op_def zchar1_op_defs[] =
{
    {"4.type1execchar", ztype1execchar},
		/* Internal operators */
    {"4%nobbox_continue", nobbox_continue},
    {"4%nobbox_fill", nobbox_fill},
    {"4%nobbox_stroke", nobbox_stroke},
    {"4%bbox_getsbw_continue", bbox_getsbw_continue},
    {"4%bbox_continue", bbox_continue},
    {"4%bbox_fill", bbox_fill},
    {"4%bbox_stroke", bbox_stroke},
    op_def_end(0)
};

/* ------ Auxiliary procedures for type 1 fonts ------ */

/* These are exported for zfont1.c. */

int
z1_subr_proc(gs_font_type1 * pfont, int index, bool global,
	     gs_const_string * pstr)
{
    const font_data *pfdata = pfont_data(pfont);
    ref subr;
    int code;

    code = array_get((global ? &pfdata->u.type1.GlobalSubrs :
		      &pfdata->u.type1.Subrs),
		     index, &subr);
    if (code < 0)
	return code;
    check_type_only(subr, t_string);
    pstr->data = subr.value.const_bytes;
    pstr->size = r_size(&subr);
    return 0;
}

int
z1_seac_proc(gs_font_type1 * pfont, int index, gs_const_string * pstr)
{
    const font_data *pfdata = pfont_data(pfont);
    ref *pcstr;
    ref enc_entry;
    int code = array_get(&StandardEncoding, (long)index, &enc_entry);

    if (code < 0)
	return code;
    if (dict_find(&pfdata->CharStrings, &enc_entry, &pcstr) <= 0)
	return_error(e_undefined);
    check_type_only(*pcstr, t_string);
    pstr->data = pcstr->value.const_bytes;
    pstr->size = r_size(pcstr);
    return 0;
}

int
z1_push_proc(gs_font_type1 * ignore, const fixed * pf, int count)
{
    const fixed *p = pf + count - 1;
    int i;

    check_ostack(count);
    for (i = 0; i < count; i++, p--) {
	osp++;
	make_real(osp, fixed2float(*p));
    }
    return 0;
}

int
z1_pop_proc(gs_font_type1 * ignore, fixed * pf)
{
    double val;
    int code = real_param(osp, &val);

    if (code < 0)
	return code;
    *pf = float2fixed(val);
    osp--;
    return 0;
}
