/* Copyright (C) 1996 Aladdin Enterprises.  All rights reserved.

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

/* zfont42.c */
/* Type 42 font creation operator */
#include "ghost.h"
#include "errors.h"
#include "oper.h"
#include "gsccode.h"
#include "gsmatrix.h"
#include "gxfont.h"
#include "gxfont42.h"
#include "bfont.h"
#include "idict.h"
#include "idparam.h"
#include "store.h"

/* Forward references */
private int z42_string_proc(P4(gs_font_type42 *, ulong, uint, const byte **));

/* <string|name> <font_dict> .buildfont11/42 <string|name> <font> */
/* Build a type 11 (TrueType CID-keyed) or 42 (TrueType) font. */
int
build_gs_TrueType_font(os_ptr op, font_type ftype, const char _ds * bcstr,
		       const char _ds * bgstr, build_font_options_t options)
{
    build_proc_refs build;
    ref *psfnts;
    ref sfnts0;

#define sfd (sfnts0.value.const_bytes)
    gs_font_type42 *pfont;
    font_data *pdata;
    int code;

    code = build_proc_name_refs(&build, bcstr, bgstr);
    if (code < 0)
	return code;
    check_type(*op, t_dictionary);
    if (dict_find_string(op, "sfnts", &psfnts) <= 0)
	return_error(e_invalidfont);
    if ((code = array_get(psfnts, 0L, &sfnts0)) < 0)
	return code;
    if (!r_has_type(&sfnts0, t_string))
	return_error(e_typecheck);
    code = build_gs_primitive_font(op, (gs_font_base **) & pfont, ftype,
				   &st_gs_font_type42, &build, options);
    if (code != 0)
	return code;
    pdata = pfont_data(pfont);
    ref_assign(&pdata->u.type42.sfnts, psfnts);
    pfont->data.string_proc = z42_string_proc;
    pfont->data.proc_data = (char *)pdata;
    code = gs_type42_font_init(pfont);
    if (code < 0)
	return code;
    return define_gs_font((gs_font *) pfont);
}
private int
zbuildfont42(os_ptr op)
{
    return build_gs_TrueType_font(op, ft_TrueType, "%Type42BuildChar",
				  "%Type42BuildGlyph", bf_options_none);
}

/* ------ Initialization procedure ------ */

const op_def zfont42_op_defs[] =
{
    {"2.buildfont42", zbuildfont42},
    op_def_end(0)
};

/* Procedure for accessing the sfnts array. */
private int
z42_string_proc(gs_font_type42 * pfont, ulong offset, uint length,
		const byte ** pdata)
{
    const font_data *pfdata = pfont_data(pfont);
    ulong left = offset;
    uint index = 0;

    for (;; ++index) {
	ref rstr;
	int code = array_get(&pfdata->u.type42.sfnts, index, &rstr);
	uint size;

	if (code < 0)
	    return code;
	if (!r_has_type(&rstr, t_string))
	    return_error(e_typecheck);
	/*
	 * NOTE: According to the Adobe documentation, each sfnts
	 * string should have even length.  If the length is odd,
	 * the additional byte is padding and should be ignored.
	 */
	size = r_size(&rstr) & ~1;
	if (left < size) {
	    if (left + length > size)
		return_error(e_rangecheck);
	    *pdata = rstr.value.const_bytes + left;
	    return 0;
	}
	left -= size;
    }
}
