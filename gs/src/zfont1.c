/* Copyright (C) 1991, 1995, 1996, 1997, 1998, 1999 Aladdin Enterprises.  All rights reserved.

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


/* Type 1 and Type 4 font creation operators */
#include "ghost.h"
#include "oper.h"
#include "gxfixed.h"
#include "gsmatrix.h"
#include "gxdevice.h"
#include "gschar.h"
#include "gxfont.h"
#include "gxfont1.h"
#include "bfont.h"
#include "ialloc.h"
#include "idict.h"
#include "idparam.h"
#include "ifont1.h"
#include "store.h"

/*#define TEST*/

/* Type 1 auxiliary procedures (defined in zchar1.c) */
extern const gs_type1_data_procs_t z1_data_procs;

/* Default value of lenIV */
#define DEFAULT_LENIV_1 4

/* Private utilities */
private void
find_zone_height(float *pmax_height, int count, const float *values)
{
    int i;
    float zone_height;

    for (i = 0; i < count; i += 2)
	if ((zone_height = values[i + 1] - values[i]) > *pmax_height)
	    *pmax_height = zone_height;
}

/* Extract pointers to internal structures. */
int
charstring_font_get_refs(os_ptr op, charstring_font_refs_t *pfr)
{
    check_type(*op, t_dictionary);
    if (dict_find_string(op, "Private", &pfr->Private) <= 0 ||
	!r_has_type(pfr->Private, t_dictionary)
	)
	return_error(e_invalidfont);
    make_empty_array(&pfr->no_subrs, 0);
    if (dict_find_string(pfr->Private, "OtherSubrs", &pfr->OtherSubrs) > 0) {
	if (!r_is_array(pfr->OtherSubrs))
	    return_error(e_typecheck);
    } else
	pfr->OtherSubrs = &pfr->no_subrs;
    if (dict_find_string(pfr->Private, "Subrs", &pfr->Subrs) > 0) {
	if (!r_is_array(pfr->Subrs))
	    return_error(e_typecheck);
    } else
	pfr->Subrs = &pfr->no_subrs;
    pfr->GlobalSubrs = &pfr->no_subrs;
    return 0;
}

/* Build a Type 1, Type 2, or Type 4 font. */
int
build_charstring_font(i_ctx_t *i_ctx_p, os_ptr op, build_proc_refs *pbuild,
		      font_type ftype, charstring_font_refs_t *pfr,
		      gs_type1_data *pdata1, build_font_options_t options)
{
    const ref *pprivate = pfr->Private;
    gs_font_type1 *pfont;
    font_data *pdata;
    int code;

    /* Get the rest of the information from the Private dictionary. */
    if ((code = dict_int_param(pprivate, "lenIV", -1, 255, pdata1->lenIV,
			       &pdata1->lenIV)) < 0 ||
	(code = dict_uint_param(pprivate, "subroutineNumberBias",
				0, max_uint, pdata1->subroutineNumberBias,
				&pdata1->subroutineNumberBias)) < 0 ||
	(code = dict_int_param(pprivate, "BlueFuzz", 0, 1999, 1,
			       &pdata1->BlueFuzz)) < 0 ||
	(code = dict_float_param(pprivate, "BlueScale", 0.039625,
				 &pdata1->BlueScale)) < 0 ||
	(code = dict_float_param(pprivate, "BlueShift", 7.0,
				 &pdata1->BlueShift)) < 0 ||
	(code = pdata1->BlueValues.count =
	 dict_float_array_param(pprivate, "BlueValues", max_BlueValues * 2,
				&pdata1->BlueValues.values[0], NULL)) < 0 ||
	(code = dict_float_param(pprivate, "ExpansionFactor", 0.06,
				 &pdata1->ExpansionFactor)) < 0 ||
	(code = pdata1->FamilyBlues.count =
	 dict_float_array_param(pprivate, "FamilyBlues", max_FamilyBlues * 2,
				&pdata1->FamilyBlues.values[0], NULL)) < 0 ||
	(code = pdata1->FamilyOtherBlues.count =
	 dict_float_array_param(pprivate,
				"FamilyOtherBlues", max_FamilyOtherBlues * 2,
			    &pdata1->FamilyOtherBlues.values[0], NULL)) < 0 ||
	(code = dict_bool_param(pprivate, "ForceBold", false,
				&pdata1->ForceBold)) < 0 ||
	(code = dict_int_param(pprivate, "LanguageGroup", 0, 1, 0,
			       &pdata1->LanguageGroup)) < 0 ||
	(code = pdata1->OtherBlues.count =
	 dict_float_array_param(pprivate, "OtherBlues", max_OtherBlues * 2,
				&pdata1->OtherBlues.values[0], NULL)) < 0 ||
	(code = dict_bool_param(pprivate, "RndStemUp", true,
				&pdata1->RndStemUp)) < 0 ||
	(code = pdata1->StdHW.count =
	 dict_float_array_param(pprivate, "StdHW", 1,
				&pdata1->StdHW.values[0], NULL)) < 0 ||
	(code = pdata1->StdVW.count =
	 dict_float_array_param(pprivate, "StdVW", 1,
				&pdata1->StdVW.values[0], NULL)) < 0 ||
	(code = pdata1->StemSnapH.count =
	 dict_float_array_param(pprivate, "StemSnapH", max_StemSnap,
				&pdata1->StemSnapH.values[0], NULL)) < 0 ||
	(code = pdata1->StemSnapV.count =
	 dict_float_array_param(pprivate, "StemSnapV", max_StemSnap,
				&pdata1->StemSnapV.values[0], NULL)) < 0 ||
    /* The WeightVector is in the font dictionary, not Private. */
	(code = pdata1->WeightVector.count =
	 dict_float_array_param(op, "WeightVector", max_WeightVector,
				pdata1->WeightVector.values, NULL)) < 0
	)
	return code;
    /*
     * According to section 5.6 of the "Adobe Type 1 Font Format",
     * there is a requirement that BlueScale times the maximum
     * alignment zone height must be less than 1.  Some fonts
     * produced by Fontographer have ridiculously large BlueScale
     * values, so we force BlueScale back into range here.
     */
    {
	float max_zone_height = 1.0;

#define SCAN_ZONE(z)\
    find_zone_height(&max_zone_height, pdata1->z.count, pdata1->z.values);

	SCAN_ZONE(BlueValues);
	SCAN_ZONE(OtherBlues);
	SCAN_ZONE(FamilyBlues);
	SCAN_ZONE(FamilyOtherBlues);

#undef SCAN_ZONE

	if (pdata1->BlueScale * max_zone_height > 1.0)
	    pdata1->BlueScale = 1.0 / max_zone_height;
    }
    /* Do the work common to primitive font types. */
    code = build_gs_primitive_font(i_ctx_p, op, (gs_font_base **)&pfont, ftype,
				   &st_gs_font_type1, pbuild, options);
    if (code != 0)
	return code;
    /* This is a new font, fill it in. */
    pdata = pfont_data(pfont);
    pfont->data = *pdata1;
    ref_assign(&pdata->u.type1.OtherSubrs, pfr->OtherSubrs);
    ref_assign(&pdata->u.type1.Subrs, pfr->Subrs);
    ref_assign(&pdata->u.type1.GlobalSubrs, pfr->GlobalSubrs);
    pfont->data.procs = &z1_data_procs;
    pfont->data.proc_data = (char *)pdata;
    return define_gs_font((gs_font *)pfont);
}

/* Build a Type 1 or Type 4 font. */
private int
buildfont1or4(i_ctx_t *i_ctx_p, os_ptr op, build_proc_refs * pbuild,
	      font_type ftype, build_font_options_t options)
{
    charstring_font_refs_t refs;
    int code = charstring_font_get_refs(op, &refs);
    gs_type1_data data1;

    if (code < 0)
	return code;
    data1.interpret = gs_type1_interpret;
    data1.subroutineNumberBias = 0;
    data1.lenIV = DEFAULT_LENIV_1;
    return build_charstring_font(i_ctx_p, op, pbuild, ftype, &refs, &data1,
				 options);
}

/* <string|name> <font_dict> .buildfont1 <string|name> <font> */
/* Build a type 1 (Adobe encrypted) font. */
private int
zbuildfont1(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    build_proc_refs build;
    int code = build_proc_name_refs(&build,
				    "%Type1BuildChar", "%Type1BuildGlyph");

    if (code < 0)
	return code;
    return buildfont1or4(i_ctx_p, op, &build, ft_encrypted,
			 bf_notdef_required);
}

/* <string|name> <font_dict> .buildfont4 <string|name> <font> */
/* Build a type 4 (disk-based Adobe encrypted) font. */
private int
zbuildfont4(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    build_proc_refs build;
    int code = build_gs_font_procs(op, &build);

    if (code < 0)
	return code;
    return buildfont1or4(i_ctx_p, op, &build, ft_disk_based, bf_options_none);
}

#ifdef TEST

#include "igstate.h"
#include "stream.h"
#include "files.h"

/* <file> .printfont1 - */
private int
zprintfont1(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    const gs_font *pfont = gs_currentfont(igs);
    stream *s;
    int code;

    if (pfont->FontType != ft_encrypted)
	return_error(e_rangecheck);
    check_write_file(s, op);
    code = psdf_embed_type1_font(s, (gs_font_type1 *) pfont);
    if (code >= 0)
	pop(1);
    return code;
}

#endif

/* ------ Initialization procedure ------ */

const op_def zfont1_op_defs[] =
{
    {"2.buildfont1", zbuildfont1},
    {"2.buildfont4", zbuildfont4},
#ifdef TEST
    {"2.printfont1", zprintfont1},
#endif
    op_def_end(0)
};
