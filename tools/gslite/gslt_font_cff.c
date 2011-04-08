/* Copyright (C) 2006 artofcode LLC.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

#include "gslt_font_int.h"

#include "gslt.h"
#include "gslt_font.h"

/*
 * OpenType Tables
 *
 * Required: cmap, head, hhea, hmtx, maxp, name, OS/2, post
 * TrueType: cvt, fpgm, glyf, loca, prep
 * Postscript: CFF, VORG
 * Typographic: BASE, GDEF, GPOS, GSUB, JSTF
 * Other: DSIG, gasp, hdmx, kern, LTSH, PCLT, VDMX, vhea, vmtx
 */

/* Access glyph data through the CharStrings INDEX */

// zfont1.c
// zfont2.c
// zchar1.c
// zcharout.c
// gxtype1.c
// gxfont1.h

static byte * gslt_count_cff_index(byte *p, byte *e, int *countp);
static byte * gslt_find_cff_index(byte *p, byte *e, int idx, byte **pp, byte **ep);

static int subrbias(int count)
{
    return count < 1240 ? 107 : count < 33900 ? 1131 : 32768;
}

static int uofs(byte *p, int offsize)
{
    if (offsize == 1) return p[0];
    if (offsize == 2) return u16(p);
    if (offsize == 3) return u24(p);
    if (offsize == 4) return u32(p);
    return 0;
}

static byte *
gslt_read_cff_real(byte *p, byte *e, float *val)
{
    char buf[64];
    char *txt = buf;

    /* b0 was 30 */

    while (txt < buf + (sizeof buf) - 3 && p < e)
    {
	int b, n;

	b = *p++;

	n = (b >> 4) & 0xf;
	if (n < 0xA) { *txt++ = n + '0'; }
	else if (n == 0xA) { *txt++ = '.'; }
	else if (n == 0xB) { *txt++ = 'E'; }
	else if (n == 0xC) { *txt++ = 'E'; *txt++ = '-'; }
	else if (n == 0xE) { *txt++ = '-'; }
	else if (n == 0xF) { break; }

	n = b & 0xf;
	if (n < 0xA) { *txt++ = n + '0'; }
	else if (n == 0xA) { *txt++ = '.'; }
	else if (n == 0xB) { *txt++ = 'E'; }
	else if (n == 0xC) { *txt++ = 'E'; *txt++ = '-'; }
	else if (n == 0xE) { *txt++ = '-'; }
	else if (n == 0xF) { break; }
    }

    *txt = 0;

    *val = atof(buf);

    return p;
}

static byte *
gslt_read_cff_integer(byte *p, byte *e, int b0, int *val)
{
    int b1, b2, b3, b4;

    if (b0 == 28)
    {
	if (p + 2 > e)
	{
	    gs_throw(-1, "corrupt dictionary (integer)");
	    return 0;
	}
	b1 = *p++;
	b2 = *p++;
	*val = (b1 << 8) | b2;
    }

    else if (b0 == 29)
    {
	if (p + 4 > e)
	{
	    gs_throw(-1, "corrupt dictionary (integer)");
	    return 0;
	}
	b1 = *p++;
	b2 = *p++;
	b3 = *p++;
	b4 = *p++;
	*val = (b1 << 24) | (b2 << 16) | (b3 << 8) | b4;
    }

    else if (b0 < 247)
    {
	*val = b0 - 139;
    }

    else if (b0 < 251)
    {
	if (p + 1 > e)
	{
	    gs_throw(-1, "corrupt dictionary (integer)");
	    return 0;
	}
	b1 = *p++;
	*val = (b0 - 247) * 256 + b1 + 108;
    }

    else
    {
	if (p + 1 > e)
	{
	    gs_throw(-1, "corrupt dictionary (integer)");
	    return 0;
	}
	b1 = *p++;
	*val = -(b0 - 251) * 256 - b1 - 108;
    }

    return p;
}

static int
gslt_read_cff_dict(byte *p, byte *e, gslt_font_t *fontobj, gs_font_type1 *pt1)
{
    struct { int ival; float fval; } args[48];
    int offset;
    int b0, n;
    float f;
    int i;

    int privatelen = 0;
    int privateofs = 0;

    offset = p - fontobj->cffdata;

    n = 0;
    while (p < e)
    {
	b0 = *p++;

	if (b0 < 22)
	{
	    if (b0 == 12)
	    {
		if (p + 1 > e)
		{
		    return gs_throw(-1, "corrupt dictionary (operator)");
		}
		b0 = 0x100 | *p++;
	    }

	    /* some CFF file offsets */

	    if (b0 == 17)
	    {
		fontobj->charstrings = fontobj->cffdata + args[0].ival;
	    }

	    if (b0 == 18)
	    {
		privatelen = args[0].ival;
		privateofs = args[1].ival;
	    }

	    if (b0 == 19)
	    {
		fontobj->subrs = fontobj->cffdata + offset + args[0].ival;
	    }

	    if (b0 == (256 | 36))
		errprintf("warning: cid cff fonts not supported yet");
	    if (b0 == (256 | 37))
		errprintf("warning: cid cff fonts not supported yet");

	    /* Type1 stuff that need to be set for the pt1 struct */

	    if (b0 == (256 | 6))
	    {
		if (args[0].ival == 1)
		{
		    pt1->data.interpret = gs_type1_interpret;
		    pt1->data.lenIV = -1; // FIXME
		}
	    }

	    if (b0 == (256 | 7))
	    {
		pt1->FontMatrix.xx = args[0].fval * 1000;
		pt1->FontMatrix.xy = args[1].fval * 1000;
		pt1->FontMatrix.yx = args[2].fval * 1000;
		pt1->FontMatrix.yy = args[3].fval * 1000;
		pt1->FontMatrix.tx = args[4].fval * 1000;
		pt1->FontMatrix.ty = args[5].fval * 1000;
	    }

	    if (b0 == 5)
	    {
		pt1->FontBBox.p.x = args[0].fval;
		pt1->FontBBox.p.y = args[1].fval;
		pt1->FontBBox.q.x = args[2].fval;
		pt1->FontBBox.q.y = args[3].fval;
	    }

	    if (b0 == 20)
		pt1->data.defaultWidthX = float2fixed(args[0].fval);

	    if (b0 == 21)
		pt1->data.nominalWidthX = float2fixed(args[0].fval);

	    if (b0 == (256 | 19))
		pt1->data.initialRandomSeed = args[0].ival;

	    /* Monday morning blues */

	    if (b0 == 6)
	    {
		pt1->data.BlueValues.count = n / 2;
		for (f = 0, i = 0; i < n; f += args[i].fval, i++)
		    pt1->data.BlueValues.values[i] = f;
	    }

	    if (b0 == 7)
	    {
		pt1->data.OtherBlues.count = n / 2;
		for (f = 0, i = 0; i < n; f += args[i].fval, i++)
		    pt1->data.OtherBlues.values[i] = f;
	    }

	    if (b0 == 8)
	    {
		pt1->data.FamilyBlues.count = n / 2;
		for (f = 0, i = 0; i < n; f += args[i].fval, i++)
		    pt1->data.FamilyBlues.values[i] = f;
	    }

	    if (b0 == 9)
	    {
		pt1->data.FamilyOtherBlues.count = n / 2;
		for (f = 0, i = 0; i < n; f += args[i].fval, i++)
		    pt1->data.FamilyOtherBlues.values[i] = f;
	    }

	    if (b0 == 10)
	    {
		pt1->data.StdHW.count = 1;
		pt1->data.StdHW.values[0] = args[0].fval;
	    }

	    if (b0 == 11)
	    {
		pt1->data.StdVW.count = 1;
		pt1->data.StdVW.values[0] = args[0].fval;
	    }

	    if (b0 == (256 | 9))
		pt1->data.BlueScale = args[0].fval;

	    if (b0 == (256 | 10))
		pt1->data.BlueShift = args[0].fval;

	    if (b0 == (256 | 11))
		pt1->data.BlueFuzz = args[0].fval;

	    if (b0 == (256 | 12))
	    {
		pt1->data.StemSnapH.count = n;
		for (f = 0, i = 0; i < n; f += args[i].fval, i++)
		    pt1->data.StemSnapH.values[i] = f;
	    }

	    if (b0 == (256 | 13))
	    {
		pt1->data.StemSnapV.count = n;
		for (f = 0, i = 0; i < n; f += args[i].fval, i++)
		    pt1->data.StemSnapV.values[i] = f;
	    }

	    if (b0 == (256 | 14))
		pt1->data.ForceBold = args[0].ival;

	    if (b0 == (256 | 17))
		pt1->data.LanguageGroup = args[0].ival;

	    if (b0 == (256 | 18))
		pt1->data.ExpansionFactor = args[0].fval;

	    n = 0;
	}

	else
	{
	    if (b0 == 30)
	    {
		p = gslt_read_cff_real(p, e, &args[n].fval);
		if (!p)
		    return gs_throw(-1, "corrupt dictionary operand");
		args[n].ival = (int) args[n].fval;
		n++;
	    }
	    else if (b0 == 28 || b0 == 29 || (b0 >= 32 && b0 <= 254))
	    {
		p = gslt_read_cff_integer(p, e, b0, &args[n].ival);
		if (!p)
		    return gs_throw(-1, "corrupt dictionary operand");
		args[n].fval = (float) args[n].ival;
		n++;
	    }
	    else
	    {
		return gs_throw1(-1, "corrupt dictionary operand (b0 = %d)", b0);
	    }
	}
    }

    /* recurse for the private dictionary */
    if (privatelen)
    {
	int code = gslt_read_cff_dict(
		fontobj->cffdata + privateofs,
		fontobj->cffdata + privateofs + privatelen,
		fontobj, pt1);
	if (code < 0)
	    return gs_rethrow(code, "cannot read private dictionary");
    }

    return 0;
}

/*
 * Get the number of items in an INDEX, and return
 * a pointer to the end of the INDEX or NULL on
 * failure.
 */
static byte *
gslt_count_cff_index(byte *p, byte *e, int *countp)
{
    int count, offsize, last;

    if (p + 3 > e)
    {
	gs_throw(-1, "not enough data for index header");
	return 0;
    }

    count = u16(p); p += 2;
    *countp = count;

    if (count == 0)
	return p;

    offsize = *p++;

    if (offsize < 1 || offsize > 4)
    {
	gs_throw(-1, "corrupt index header");
	return 0;
    }

    if (p + count * offsize > e)
    {
	gs_throw(-1, "not enough data for index offset table");
	return 0;
    }

    p += count * offsize;
    last = uofs(p, offsize);
    p += offsize;
    p --; /* stupid offsets */

    if (p + last > e)
    {
	gs_throw(-1, "not enough data for index data");
	return 0;
    }

    p += last;

    return p;
}

/*
 * Locate and store pointers to the data of an
 * item in the index that starts at 'p'.
 * Return pointer to the end of the index,
 * or NULL on failure.
 */
static byte *
gslt_find_cff_index(byte *p, byte *e, int idx, byte **pp, byte **ep)
{
    int count, offsize, sofs, eofs, last;

    if (p + 3 > e)
    {
	gs_throw(-1, "not enough data for index header");
	return 0;
    }

    count = u16(p); p += 2;
    if (count == 0)
	return 0;

    offsize = *p++;

    if (offsize < 1 || offsize > 4)
    {
	gs_throw(-1, "corrupt index header");
	return 0;
    }

    if (p + count * offsize > e)
    {
	gs_throw(-1, "not enough data for index offset table");
	return 0;
    }

    if (idx < 0 || idx >= count)
    {
	gs_throw(-1, "tried to access non-existing index item");
	return 0;
    }

    sofs = uofs(p + idx * offsize, offsize);
    eofs = uofs(p + (idx + 1) * offsize, offsize);
    last = uofs(p + count * offsize, offsize);

    p += count * offsize;
    p += offsize;
    p --; /* stupid offsets */

    if (p + last > e)
    {
	gs_throw(-1, "not enough data for index data");
	return 0;
    }

    if (sofs < 0 || eofs < 0 || sofs > eofs || eofs > last)
    {
	gs_throw(-1, "corrupt index offset table");
	return 0;
    }

    *pp = p + sofs;
    *ep = p + eofs;

    return p + last;
}

/*
 * Scan the CFF file structure and extract important data.
 */

static int
gslt_read_cff_file(gslt_font_t *fontobj, gs_font_type1 *pt1)
{
    byte *p = fontobj->cffdata;
    byte *e = fontobj->cffend;
    byte *dictp, *dicte;
    int ngsubrs;
    int nsubrs;
    int count;
    int code;

    /* CFF header */
    {
	int major, minor, hdrsize, offsize;

	if (p + 4 > e)
	    return gs_throw(-1, "not enough data for header");

	major = *p++;
	minor = *p++;
	hdrsize = *p++;
	offsize = *p++;

	if (major != 1 || minor != 0)
	    return gs_throw(-1, "not a CFF 1.0 file");

	if (p + hdrsize - 4 > e)
	    return gs_throw(-1, "not enough data for extended header");
    }

    /* Name INDEX */
    p = gslt_count_cff_index(p, e, &count);
    if (!p)
	return gs_throw(-1, "cannot read name index");
    if (count != 1)
	return gs_throw(-1, "file did not contain exactly one font");

    /* Top Dict INDEX */
    p = gslt_find_cff_index(p, e, 0, &dictp, &dicte);
    if (!p)
	return gs_throw(-1, "cannot read top dict index");

    /* String index */
    p = gslt_count_cff_index(p, e, &count);
    if (!p)
	return gs_throw(-1, "cannot read string index");

    /* Global Subr INDEX */
    fontobj->gsubrs = p;
    p = gslt_count_cff_index(p, e, &ngsubrs);
    if (!p)
	return gs_throw(-1, "cannot read gsubr index");

    /* Read the top and private dictionaries */
    code = gslt_read_cff_dict(dictp, dicte, fontobj, pt1);
    if (code < 0)
	return gs_rethrow(code, "cannot read top dictionary");

    /* Check the subrs index */
    nsubrs = 0;
    if (fontobj->subrs)
    {
	p = gslt_count_cff_index(fontobj->subrs, e, &nsubrs);
	if (!p)
	    return gs_rethrow(-1, "cannot read subrs index");
    }

    /* Check the charstrings index */
    if (fontobj->charstrings)
    {
	p = gslt_count_cff_index(fontobj->charstrings, e, &count);
	if (!p)
	    return gs_rethrow(-1, "cannot read charstrings index");
    }

    pt1->data.subroutineNumberBias = subrbias(nsubrs);
    pt1->data.gsubrNumberBias = subrbias(ngsubrs);
    // nominal and defaultWidthX

    return 0;
}


/*
 *
 */

static gs_glyph
gslt_post_callback_encode_char(gs_font *pfont, gs_char chr, gs_glyph_space_t spc)
{
    gslt_font_t *xf = pfont->client_data;
    int value;
    value = gslt_encode_font_char(xf, chr);
    if (value == 0)
	return gs_no_glyph;
    return value;
}

static gs_char
gslt_post_callback_decode_glyph(gs_font *p42, gs_glyph glyph, int ch)
{
    return GS_NO_CHAR;
}

static int
gslt_post_callback_glyph_name(gs_font *pf, gs_glyph glyph, gs_const_string *pstr)
{
    return -1;
}

static int
gslt_post_callback_glyph_info(gs_font *font, gs_glyph glyph,
	const gs_matrix *pmat, int members, gs_glyph_info_t *info)
{
    return -1;
}

static int
gslt_post_callback_glyph_outline(gs_font *font, int wmode, gs_glyph glyph,
	const gs_matrix *pmat, gx_path *ppath, double sbw[4])
{
    dprintf2("glyph_outline wmode=%d glyph=%d\n", wmode, glyph);
    return -1;
}

typedef struct gs_type1exec_state_s
{
    gs_type1_state cis;         /* must be first */
    /* i_ctx_t *i_ctx_p; */     /* so push/pop can access o-stack */
    double sbw[4];
    gs_rect char_bbox;
    /*
     * The following elements are only used locally to make the stack clean
     * for OtherSubrs: they don't need to be declared for the garbage
     * collector.
     */
    void * save_args[6];
    int num_args;
    bool AlignToPixels;
} gs_type1exec_state;

static int
gslt_post_callback_glyph_data(gs_font_type1 * pfont, gs_glyph glyph, gs_glyph_data_t *pgd)
{
    gslt_font_t *fontobj = pfont->client_data;
    byte *s, *e;
    byte *p;

    // z1_glyph_data
    // zchar_charstring_data
    // gs_glyph_data_from_string

    dprintf1("get glyph data for %d\n", glyph);

    p = gslt_find_cff_index(fontobj->charstrings, fontobj->cffend, glyph, &s, &e);
    if (!p)
	return gs_rethrow(-1, "cannot find charstring");

    gs_glyph_data_from_string(pgd, s, e - s, NULL);

    return 0;
}

static int
gslt_post_callback_subr_data(gs_font_type1 * pfont,
	int subr_num, bool global, gs_glyph_data_t *pgd)
{
    gslt_font_t *fontobj = pfont->client_data;
    byte *s, *e;
    byte *p;

    dprintf2("get %s subr data for %d\n", global?"global":"local", subr_num);

    if (global)
    {
	p = gslt_find_cff_index(fontobj->gsubrs, fontobj->cffend, subr_num, &s, &e);
	if (!p)
	    return gs_rethrow(-1, "cannot find gsubr");
    }
    else
    {
	p = gslt_find_cff_index(fontobj->subrs, fontobj->cffend, subr_num, &s, &e);
	if (!p)
	    return gs_rethrow(-1, "cannot find subr");
    }

    gs_glyph_data_from_string(pgd, s, e - s, NULL);

    return 0;
}

static int
gslt_post_callback_seac_data(gs_font_type1 * pfont, int ccode, gs_glyph * pglyph,
	gs_const_string *gstr, gs_glyph_data_t *pgd)
{
    return gs_throw(-1, "seac is deprecated in CFF fonts");
}

static int
gslt_post_callback_push(void *callback_data, const fixed *values, int count)
{
    return gs_throw(-1, "push not implemented");;
}

static int
gslt_post_callback_pop(void *callback_data, fixed *value)
{
    return gs_throw(-1, "pop not implemented");;
}


static int
gslt_cff_append(gs_state *pgs, gs_font_type1 *pt1, gs_glyph glyph, int donthint)
{
    int code, value;
    gs_type1exec_state cxs;
    gs_glyph_data_t gd;
    gs_type1_state *const pcis = &cxs.cis;
    gs_imager_state *pgis = (gs_imager_state*)pgs;
    gs_glyph_data_t *pgd = &gd;
    double wv[4];
    double sbw[4];
    gs_matrix mtx;

    // get charstring data
    code = gslt_post_callback_glyph_data(pt1, glyph, pgd);
    if (code < 0)
	return gs_rethrow(code, "cannot get glyph data");

    mtx.xx = ctm_only(pgs).xx;
    mtx.xy = ctm_only(pgs).xy;
    mtx.yx = ctm_only(pgs).yx;
    mtx.yy = ctm_only(pgs).yy;
    mtx.tx = 0.0;
    mtx.ty = 0.0;
    gs_matrix_scale(&mtx, 0.001, 0.001, &mtx);
    gs_matrix_fixed_from_matrix(&pgis->ctm, &mtx);
    pgis->flatness = 0;

    code = gs_type1_interp_init(&cxs.cis, pgis, pgs->path, NULL, NULL, donthint, 0, pt1);
    if (code < 0)
	return gs_throw(code, "cannot init type1 interpreter");

    gs_type1_set_callback_data(pcis, &cxs);

    // TODO: check if this is set in the font dict
    // gs_type1_set_lsb(pcis, &mpt);
    // gs_type1_set_width(pcis, &mpt);

    // ...

    while (1)
    {
	code = pt1->data.interpret(pcis, pgd, &value);
	switch (code)
	{
	case type1_result_callothersubr: /* unknown OtherSubr */
	    return_error(-15); /* can't handle it */
	case type1_result_sbw: /* [h]sbw, just continue */
	    type1_cis_get_metrics(pcis, cxs.sbw);
	    type1_cis_get_metrics(pcis, sbw);
	    pgd = 0;
	    break;
	case 0: /* all done */
	default: /* code < 0, error */
	    return code;
	}
    }
}

static int
gslt_post_callback_build_char(gs_text_enum_t *ptextenum, gs_state *pgs,
	gs_font *pfont, gs_char chr, gs_glyph glyph)
{
    gs_show_enum *penum = (gs_show_enum*)ptextenum;
    gs_font_type1 *pt1 = (gs_font_type1*)pfont;
    const gs_rect *pbbox;
    float sbw[4], w2[6];
    int code;

    dprintf2("build_char chr=%d glyph=%d\n", chr, glyph);

    // get the metrics
    w2[0] = 0;
    w2[1] = 1;

    pbbox =  &pt1->FontBBox;
    w2[2] = pbbox->p.x * 0.001;
    w2[3] = pbbox->p.y * 0.001;
    w2[4] = pbbox->q.x * 0.001;
    w2[5] = pbbox->q.y * 0.001;

    /* Expand the bbox when stroking */
    if ( pfont->PaintType )
    {
	float expand = max(1.415, gs_currentmiterlimit(pgs)) * gs_currentlinewidth(pgs) / 2;
	w2[2] -= expand, w2[3] -= expand;
	w2[4] += expand, w2[5] += expand;
    }

    if ( (code = gs_moveto(pgs, 0.0, 0.0)) < 0 )
	return code;
            
    if ( (code = gs_setcachedevice(penum, pgs, w2)) < 0 )
	return code;

    code = gslt_cff_append(pgs, pt1, glyph,
	    gs_show_in_charpath(penum) != cpm_show);
    if (code < 0)
	return code;

    code = (pfont->PaintType ? gs_stroke(pgs) : gs_fill(pgs));
    if (code < 0)
	return code;

    return 0;
}

int
gslt_init_postscript_font(gs_memory_t *mem,
	gs_font_dir *fontdir, gslt_font_t *fontobj)
{
    gs_font_type1 *pt1;
    int cffofs;
    int cfflen;
    int code;

    /* Find the CFF table and parse it to create a charstring based font */
    /* We don't need to support CFF files with multiple fonts */
    /* Find the VORG table for easier vertical metrics */

#if 0
    gs_glyph_data_t;
    gs_type1_data_procs_t {
	z1_glyph_data, z1_subr_data, z1_seac_data, z1_push, z1_pop
    };
    gs_type1_data_s { procs;  ...;
	subroutineNumberBias;
	gsubrNumberBias;
	defaultWidthX;
	nominalWidthX;
    }
    gs_font_type1 { font_base_common; data }
#endif

    cffofs = gslt_find_sfnt_table(fontobj, "CFF ", &cfflen);
    if (cffofs < 0)
	return gs_throw(-1, "cannot find CFF table");

    if (cfflen < 0 || cffofs + cfflen > fontobj->length)
	return gs_throw(-1, "corrupt CFF table location");

    fontobj->cffdata = fontobj->data + cffofs;
    fontobj->cffend = fontobj->data + cffofs + cfflen;

    fontobj->gsubrs = 0;
    fontobj->subrs = 0;
    fontobj->charstrings = 0;

    pt1 = (void*) gs_alloc_struct(mem, gs_font_type1, &st_gs_font_type1, "gslt_font type1");
    if (!pt1)
	return gs_throw(-1, "out of memory");

    fontobj->font = (void*) pt1;

    /*
     * No shortage of things to initialize
     */

    // build_gs_font_procs
    // build_gs_primitive_font
    //	build_gs_outline_font
    //	    build_base_font = build_gs_simple_font
    //		build_gs_font
    //		init_gs_simple_font
    // charstring_font_init
    //	    glyph_outline = zchar1_glyph_outline
    //	    e
    // define_gs_font

    /* Common to all fonts */

    pt1->next = 0;
    pt1->prev = 0;
    pt1->memory = mem;

    pt1->dir = fontdir; /* NB also set by gs_definefont later */
    pt1->base = fontobj->font; /* NB also set by gs_definefont later */
    pt1->is_resource = false;
    gs_notify_init(&pt1->notify_list, gs_memory_stable(mem));
    pt1->id = gs_next_ids(mem, 1);

    pt1->client_data = fontobj; /* that's us */

    gs_make_identity(&pt1->FontMatrix);
    gs_make_identity(&pt1->orig_FontMatrix);

    pt1->FontType = ft_encrypted2;
    pt1->BitmapWidths = true;
    pt1->ExactSize = fbit_use_outlines;
    pt1->InBetweenSize = fbit_use_outlines;
    pt1->TransformedChar = fbit_use_outlines;
    pt1->WMode = 0;
    pt1->PaintType = 0;
    pt1->StrokeWidth = 0;

    pt1->procs.define_font = gs_no_define_font;
    pt1->procs.make_font = gs_no_make_font;
    pt1->procs.font_info = gs_default_font_info;
    // same_font
    pt1->procs.encode_char = gslt_post_callback_encode_char; 
    pt1->procs.decode_glyph = gslt_post_callback_decode_glyph;
    // enumerate_glyph
    pt1->procs.glyph_info = gslt_post_callback_glyph_info;;
    pt1->procs.glyph_outline = gslt_post_callback_glyph_outline;
    pt1->procs.glyph_name = gslt_post_callback_glyph_name;
    pt1->procs.init_fstack = gs_default_init_fstack;
    pt1->procs.next_char_glyph = gs_default_next_char_glyph;
    pt1->procs.build_char = gslt_post_callback_build_char;

    pt1->font_name.size = 0;
    pt1->key_name.size = 0;

    /* Base font specific */

    pt1->FontBBox.p.x = 0; // -0.5;
    pt1->FontBBox.p.y = 0; // -0.5;
    pt1->FontBBox.q.x = 0; // 1.5;
    pt1->FontBBox.q.y = 0; // 1.5;

    uid_set_UniqueID(&pt1->UID, pt1->id);

    pt1->encoding_index = ENCODING_INDEX_UNKNOWN;
    pt1->nearest_encoding_index = ENCODING_INDEX_UNKNOWN;

    pt1->FAPI = 0;
    pt1->FAPI_font_data = 0;

    /* Type 1/2 specific */
    /* defaults from the CFF spec */

    pt1->data.procs.glyph_data = gslt_post_callback_glyph_data;
    pt1->data.procs.subr_data = gslt_post_callback_subr_data;
    pt1->data.procs.seac_data = gslt_post_callback_seac_data;
    pt1->data.procs.push_values = gslt_post_callback_push;
    pt1->data.procs.pop_value = gslt_post_callback_pop;

    pt1->data.interpret = gs_type2_interpret;
    pt1->data.proc_data = fontobj;
    pt1->data.parent = NULL;
    pt1->data.lenIV = -1; // DEFAULT_LENIV_2

    pt1->data.subroutineNumberBias = 0;
    pt1->data.gsubrNumberBias = 0;
    pt1->data.initialRandomSeed = 0;
    pt1->data.defaultWidthX = 0;
    pt1->data.nominalWidthX = 0;

    pt1->data.BlueFuzz = 1;
    pt1->data.BlueScale = 0.039625;
    pt1->data.BlueShift = 7;
    pt1->data.BlueValues.count = 0;
    pt1->data.ExpansionFactor = 0.06;
    pt1->data.ForceBold = 0;
    pt1->data.FamilyBlues.count = 0;
    pt1->data.FamilyOtherBlues.count = 0;
    pt1->data.LanguageGroup = 0;
    pt1->data.OtherBlues.count = 0;

    pt1->data.RndStemUp = 0;
    memset(&pt1->data.StdHW, 0, sizeof(pt1->data.StdHW));
    memset(&pt1->data.StdVW, 0, sizeof(pt1->data.StdVW));
    memset(&pt1->data.StemSnapH, 0, sizeof(pt1->data.StemSnapH));
    memset(&pt1->data.StemSnapV, 0, sizeof(pt1->data.StemSnapH));
    memset(&pt1->data.WeightVector, 0, sizeof(pt1->data.WeightVector));

    code = gslt_read_cff_file(fontobj, pt1);
    if (code < 0)
    {
	// TODO free pt1 here?
	return gs_rethrow(code, "cannot read cff file structure");
    }

    gs_definefont(fontdir, fontobj->font);

    return 0;
}

