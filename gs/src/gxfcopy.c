/* Copyright (C) 2002 Aladdin Enterprises.  All rights reserved.
  
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
/* Font copying for high-level output */
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsline.h"		/* for BuildChar */
#include "gspaint.h"		/* for BuildChar */
#include "gspath.h"		/* for gs_moveto in BuildChar */
#include "gsstruct.h"
#include "stream.h"
#include "gxfont.h"
#include "gxfont1.h"
#include "gxfont42.h"
#include "gxfcid.h"
#include "gxfcopy.h"
#include "gxistate.h"		/* for Type 1 glyph_outline */
#include "gxtext.h"		/* for BuildChar */
#include "gxtype1.h"		/* for Type 1 glyph_outline */
#include "gzstate.h"		/* for path for BuildChar */
#include "gdevpsf.h"

/* ================ Types and structures ================ */

typedef struct gs_copied_glyph_s gs_copied_glyph_t;
typedef struct gs_copied_font_data_s gs_copied_font_data_t;

typedef struct gs_copied_font_procs_s {
    int (*finish_copy_font)(gs_font *font, gs_font *copied);
    int (*copy_glyph)(gs_font *font, gs_glyph glyph, gs_font *copied);
    int (*add_encoding)(gs_font *copied, gs_char chr, gs_glyph glyph);
    int (*named_glyph_slot)(gs_copied_font_data_t *cfdata, gs_glyph glyph,
			    gs_copied_glyph_t **pslot);
    /* Font procedures */
    font_proc_encode_char((*encode_char));
    font_proc_glyph_info((*glyph_info));
    font_proc_glyph_outline((*glyph_outline));
} gs_copied_font_procs_t;

/*
 * We need to store the Subrs data for Type 1/2 and CIDFontType 0 fonts,
 * and the GlobalSubrs data for all but Type 1.
 */
typedef struct gs_subr_info_s {
    byte *data;		/* Subr data */
    int count;
    uint *starts;	/* [count+1] Subr[i] = data[starts[i]..starts[i+1]] */
} gs_subr_info_t;

/*
 * The glyphs for copied fonts are stored explicitly in a table indexed by
 * glyph number.
 * For Type 1 fonts, the glyph numbers are parallel to the hashed name table.
 * For TrueType fonts, the glyph number is the TrueType GID.
 * For CIDFontType 0 fonts, the glyph number is the CID.
 * For CIDFontType 2 fonts, the glyph number is the TrueType GID;
 * a separate CIDMap array maps CIDs to GIDs.
 */
struct gs_copied_glyph_s {
    gs_const_string gdata;	/* vector data */
#define HAS_DATA 1		/* entry is in use */
				/* HAS_SBW* are only used for TT-based fonts */
#define HAS_SBW0 2		/* has hmtx */
#define HAS_SBW1 4		/* has vmtx */
    byte used;			/* non-zero iff this entry is in use */
				/* (if not, gdata.{data,size} = 0) */
};
gs_private_st_const_strings1(st_gs_copied_glyph, gs_copied_glyph_t,
			     "gs_copied_glyph_t", gs_copied_glyph_enum_ptrs,
			     gs_copied_glyph_reloc_ptrs, gdata);
gs_private_st_element(st_gs_copied_glyph_element, gs_copied_glyph_t,
		      "gs_copied_glyph_t[]",
		      copied_glyph_element_enum_ptrs,
		      copied_glyph_element_reloc_ptrs, st_gs_copied_glyph);

/*
 * Type 1 and TrueType fonts also have a 'names' table, parallel to the
 * 'glyphs' table.
 * For Type 1 fonts, this is a hash table; glyph numbers are assigned
 * arbitrarily, according to the hashed placement of the names.
 * For TrueType fonts, this is indexed by GID.
 * The strings in this table are those returned by the font's glyph_name
 * procedure: we assume they are garbage-collected.
 */
typedef struct gs_copied_glyph_name_s {
    gs_glyph glyph;		/* key (for comparison and glyph_name only) */
    gs_const_string str;	/* glyph name */
} gs_copied_glyph_name_t;
gs_private_st_const_strings1(st_gs_copied_glyph_name, gs_copied_glyph_name_t,
			     "gs_copied_glyph_name_t",
			     gs_copied_glyph_name_enum_ptrs,
			     gs_copied_glyph_name_reloc_ptrs, str);
gs_private_st_element(st_gs_copied_glyph_name_element, gs_copied_glyph_name_t,
		      "gs_copied_glyph_name_t[]",
		      copied_glyph_name_element_enum_ptrs,
		      copied_glyph_name_element_reloc_ptrs,
		      st_gs_copied_glyph_name);

/*
 * The client_data of copied fonts points to an instance of
 * gs_copied_font_data_t.
 */
struct gs_copied_font_data_s {
    gs_font_info_t info;	/* from the original font, must be first */
    const gs_copied_font_procs_t *procs;
    gs_copied_glyph_t *glyphs;	/* [glyphs_size] */
    uint glyphs_size;		/* (a power of 2 for Type 1/2) */
    gs_glyph notdef;		/* CID 0 or .notdef glyph */
    /*
     * We don't use a union for the rest of the data, because some of the
     * cases overlap and it's just not worth the trouble.
     */
    gs_copied_glyph_name_t *names; /* (Type 1/2) [glyphs_size] */
    byte *data;			/* (TrueType and CID fonts) copied data */
    uint data_size;		/* (TrueType and CID fonts) */
    gs_glyph *Encoding;		/* (Type 1/2 and Type 42) [256] */
    ushort *CIDMap;		/* (CIDFontType 2) [CIDCount] */
    gs_subr_info_t subrs;	/* (Type 1/2 and CIDFontType 0) */
    gs_subr_info_t global_subrs; /* (Type 2 and CIDFontType 0) */
    gs_font_cid0 *parent;	/* (Type 1 subfont) => parent CIDFontType 0 */
};
extern_st(st_gs_font_info);
gs_private_st_suffix_add10(st_gs_copied_font_data, gs_copied_font_data_t,
  "gs_copied_font_data_t", gs_copied_font_data_enum_ptrs,
  gs_copied_font_data_reloc_ptrs, st_gs_font_info, glyphs, names,
  data, Encoding, CIDMap, subrs.data, subrs.starts, global_subrs.data,
  global_subrs.starts, parent);

inline private gs_copied_font_data_t *
cf_data(const gs_font *font)
{
    return (gs_copied_font_data_t *)font->client_data;
}

/* ================ Procedures ================ */

/* ---------------- Private utilities ---------------- */

/* Copy a string.  Return 0 or gs_error_VMerror. */
private int
copy_string(gs_memory_t *mem, gs_const_string *pstr, client_name_t cname)
{
    const byte *data = pstr->data;
    uint size = pstr->size;
    byte *str;

    if (data == 0)
	return 0;		/* empty string */
    str = gs_alloc_string(mem, size, cname);
    pstr->data = str;
    if (str == 0)
	return_error(gs_error_VMerror);
    memcpy(str, data, size);
    return 0;
}

/* Free a copied string. */
private void
uncopy_string(gs_memory_t *mem, gs_const_string *pstr, client_name_t cname)
{
    if (pstr->data)
	gs_free_const_string(mem, pstr->data, pstr->size, cname);
}

/*
 * Allocate an Encoding for a Type 1 or Type 42 font.
 */
private int
copied_Encoding_alloc(gs_font *copied)
{
    gs_copied_font_data_t *const cfdata = cf_data(copied);
    gs_glyph *Encoding = (gs_glyph *)
	gs_alloc_byte_array(copied->memory, 256, sizeof(*cfdata->Encoding),
			    "copy_font_type1(Encoding)");
    int i;

    if (Encoding == 0)
	return_error(gs_error_VMerror);
    for (i = 0; i < 256; ++i)
	Encoding[i] = cfdata->notdef;
    cfdata->Encoding = Encoding;
    return 0;
}

/*
 * Allocate and set up data copied from a TrueType or CID font.
 * stell(*s) + extra is the length of the data.
 */
private int
copied_data_alloc(gs_font *copied, stream *s, uint extra, int code)
{
    gs_copied_font_data_t *const cfdata = cf_data(copied);
    uint len = stell(s);
    byte *fdata;

    if (code < 0)
	return code;
    fdata = gs_alloc_bytes(copied->memory, len + extra, "copied_data_alloc");
    if (fdata == 0)
	return_error(gs_error_VMerror);
    swrite_string(s, fdata, len);
    cfdata->data = fdata;
    cfdata->data_size = len + extra;
    return 0;
}

/*
 * Copy Subrs or GSubrs from a font.
 */
private int
copy_subrs(gs_font_type1 *pfont, bool global, gs_subr_info_t *psi,
	   gs_memory_t *mem)
{
    int i, code;
    uint size;
    gs_glyph_data_t gdata;
    byte *data;
    uint *starts;

    /* Scan the font to determine the size of the subrs. */
    for (i = 0, size = 0;
	 (code = pfont->data.procs.subr_data(pfont, i, global, &gdata)) !=
	     gs_error_rangecheck;
	 ++i) {
	size += gdata.bits.size;
	gs_glyph_data_free(&gdata, "copy_subrs");
    }
    if (size == 0)
	data = 0, starts = 0, i = 0;
    else {
	/* Allocate the copy. */
	data = gs_alloc_bytes(mem, size, "copy_subrs(data)");
	starts = (uint *)gs_alloc_byte_array(mem, i + 1, sizeof(*starts),
					     "copy_subrs(starts)");
	if (data == 0 || starts == 0) {
	    gs_free_object(mem, starts, "copy_subrs(starts)");
	    gs_free_object(mem, data, "copy_subrs(data)");
	    return_error(gs_error_VMerror);
	}

	/* Copy the data. */
	for (i = 0, size = 0;
	     (code = pfont->data.procs.subr_data(pfont, i, global, &gdata)) !=
		 gs_error_rangecheck;
	     ++i) {
	    starts[i] = size;
	    memcpy(data + size, gdata.bits.data, gdata.bits.size);
	    size += gdata.bits.size;
	    gs_glyph_data_free(&gdata, "copy_subrs");
	}
	starts[i] = size;
    }

    psi->data = data;
    psi->starts = starts;
    psi->count = i;
    return 0;
}

/*
 * Return a pointer to the definition of a copied glyph, accessed either by
 * name or by glyph number.  If the glyph is out of range, return
 * gs_error_rangecheck; if the glyph is in range but undefined, store a
 * pointer to the slot where it would be added, which will have gdata.data
 * == 0, and return gs_error_undefined; if the glyph is defined, store the
 * pointer and return 0.
 */
private int
copied_glyph_slot(gs_copied_font_data_t *cfdata, gs_glyph glyph,
		  gs_copied_glyph_t **pslot)
{
    uint gsize = cfdata->glyphs_size;

    *pslot = 0;
    if (glyph >= GS_MIN_CID_GLYPH) {
	if (glyph - GS_MIN_CID_GLYPH >= gsize)
	    return_error(gs_error_rangecheck);
	*pslot = &cfdata->glyphs[glyph - GS_MIN_CID_GLYPH];
    } else if (cfdata->names == 0)
	return_error(gs_error_rangecheck);
    else {
	int code = cfdata->procs->named_glyph_slot(cfdata, glyph, pslot);

	if (code < 0)
	    return code;
    }
    if (!(*pslot)->used)
	return_error(gs_error_undefined);
    return 0;
}
private int
named_glyph_slot_none(gs_copied_font_data_t *cfdata, gs_glyph glyph,
			gs_copied_glyph_t **pslot)
{
    return_error(gs_error_rangecheck);
}
private int
named_glyph_slot_hashed(gs_copied_font_data_t *cfdata, gs_glyph glyph,
			gs_copied_glyph_t **pslot)
{
    uint gsize = cfdata->glyphs_size;
    gs_copied_glyph_name_t *names = cfdata->names;
    uint hash = (uint)glyph % gsize;
    /*
     * gsize is a power of 2, so an odd reprobe interval guarantees that we
     * will touch every slot.
     */
    uint hash2 = ((uint)glyph / gsize * 2 + 1) % gsize;

    while (names[hash].str.data != 0 && names[hash].glyph != glyph)
	hash = (hash + hash2) % gsize;
    *pslot = &cfdata->glyphs[hash];
    return 0;
}
private int
named_glyph_slot_linear(gs_copied_font_data_t *cfdata, gs_glyph glyph,
			gs_copied_glyph_t **pslot)
{
    gs_copied_glyph_name_t *names = cfdata->names;
    int i;

    for (i = 0; i < cfdata->glyphs_size; ++i)
	if (names[i].glyph == glyph) {
	    *pslot = &cfdata->glyphs[i];
	    return 0;
	}
    return_error(gs_error_rangecheck);
}

/*
 * Add glyph data to the glyph table.  This handles copying the vector
 * data, detecting attempted redefinitions, and freeing temporary glyph
 * data.  The glyph must be an integer, an index in the glyph table.
 * Return 1 if the glyph was already defined, 0 if newly added.
 */
private int
copy_glyph_data(gs_font *font, gs_glyph glyph, gs_font *copied,
		gs_glyph_data_t *pgdata, const byte *prefix, int prefix_bytes)
{
    gs_copied_font_data_t *const cfdata = cf_data(copied);
    uint size = pgdata->bits.size;
    gs_copied_glyph_t *pcg;
    int code = copied_glyph_slot(cfdata, glyph, &pcg);

    switch (code) {
    case 0:			/* already defined */
	if (pcg->gdata.size != prefix_bytes + size ||
	    memcmp(pcg->gdata.data, prefix, prefix_bytes) ||
	    memcmp(pcg->gdata.data + prefix_bytes,
		   pgdata->bits.data, size)
	    )
	    code = gs_note_error(gs_error_invalidaccess);
	else
	    code = 1;
	break;
    case gs_error_undefined: {
	uint str_size = prefix_bytes + size;
	byte *str = gs_alloc_string(copied->memory, str_size,
				    "copy_glyph_data(data)");

	if (str == 0)
	    code = gs_note_error(gs_error_VMerror);
	else {
	    if (prefix_bytes)
		memcpy(str, prefix, prefix_bytes);
	    memcpy(str + prefix_bytes, pgdata->bits.data, size);
	    pcg->gdata.data = str;
	    pcg->gdata.size = str_size;
	    pcg->used = HAS_DATA;
	    code = 0;
	}
    }
    default:
	break;
    }
    gs_glyph_data_free(pgdata, "copy_glyph_data");
    return code;
}

/*
 * Copy a glyph name into the names table.
 */
private int
copy_glyph_name(gs_font *font, gs_glyph glyph, gs_font *copied,
		gs_glyph copied_glyph)
{
    gs_copied_font_data_t *const cfdata = cf_data(copied);
    gs_copied_glyph_t *pcg;
    int code = copied_glyph_slot(cfdata, copied_glyph, &pcg);
    gs_copied_glyph_name_t *pcgn;
    gs_const_string str;

    if (code < 0 ||
	(code = font->procs.glyph_name(font, glyph, &str)) < 0 ||
	(code = copy_string(copied->memory, &str, "copy_glyph_name")) < 0
	)
	return code;
    pcgn = cfdata->names + (pcg - cfdata->glyphs);
    pcgn->glyph = glyph;
    pcgn->str = str;
    return 0;
}

/*
 * Find the .notdef glyph in a font.
 */
private gs_glyph
find_notdef(gs_font_base *font)
{
    int index = 0;
    gs_glyph glyph;

    while (font->procs.enumerate_glyph((gs_font *)font, &index,
				       GLYPH_SPACE_NAME, &glyph),
	   index != 0)
	if (gs_font_glyph_is_notdef(font, glyph))
	    return glyph;
    return GS_NO_GLYPH;		/* best we can do */
}

/*
 * Add an Encoding entry to a character-indexed font (Type 1/2/42).
 */
private int
copied_char_add_encoding(gs_font *copied, gs_char chr, gs_glyph glyph)
{
    gs_copied_font_data_t *const cfdata = cf_data(copied);
    gs_glyph *Encoding = cfdata->Encoding;
    gs_copied_glyph_t *pslot;
    int code;

    if (Encoding == 0)
	return_error(gs_error_invalidaccess);
    if (chr < 0 || chr >= 256 || glyph >= GS_MIN_CID_GLYPH)
	return_error(gs_error_rangecheck);
    code = copied_glyph_slot(cfdata, glyph, &pslot);
    if (code < 0)
	return code;
    if (Encoding[chr] != glyph && Encoding[chr] != cfdata->notdef)
	return_error(gs_error_invalidaccess);
    Encoding[chr] = glyph;
    return 0;
}

/* Don't allow adding an Encoding entry. */
private int
copied_no_add_encoding(gs_font *copied, gs_char chr, gs_glyph glyph)
{
    return_error(gs_error_invalidaccess);
}

/* ---------------- Font procedures ---------------- */

private int
copied_font_info(gs_font *font, const gs_point *pscale, int members,
		 gs_font_info_t *info)
{
    if (pscale != 0)
	return_error(gs_error_rangecheck);
    *info = cf_data(font)->info;
    return 0;
}

private gs_glyph
copied_encode_char(gs_font *copied, gs_char chr, gs_glyph_space_t glyph_space)
{
    gs_copied_font_data_t *const cfdata = cf_data(copied);
    const gs_glyph *Encoding = cfdata->Encoding;

    if (chr < 0 || chr >= 256 || Encoding == 0)
	return GS_NO_GLYPH;
    return Encoding[chr];
}

private int
copied_enumerate_glyph(gs_font *font, int *pindex,
		       gs_glyph_space_t glyph_space, gs_glyph *pglyph)
{
    gs_copied_font_data_t *const cfdata = cf_data(font);

    for (; *pindex < cfdata->glyphs_size; ++*pindex)
	if (cfdata->glyphs[*pindex].used) {
	    *pglyph =
		(glyph_space == GLYPH_SPACE_NAME && cfdata->names != 0 ?
		 cfdata->names[*pindex].glyph :
		 *pindex + GS_MIN_CID_GLYPH);
	    ++(*pindex);
	    return 0;
	}
    *pindex = 0;
    return 0;
}

private int
copied_glyph_name(gs_font *font, gs_glyph glyph, gs_const_string *pstr)
{
    gs_copied_font_data_t *const cfdata = cf_data(font);
    gs_copied_glyph_t *pcg;

    if (glyph >= GS_MIN_CID_GLYPH)
	return_error(gs_error_rangecheck);
    if (copied_glyph_slot(cfdata, glyph, &pcg) < 0)
	return_error(gs_error_undefined);
    *pstr = cfdata->names[pcg - cfdata->glyphs].str;
    return 0;
}

private int
copied_build_char(gs_text_enum_t *pte, gs_state *pgs, gs_font *font,
		  gs_char chr, gs_glyph glyph)
{
    int wmode = font->WMode;
    int code;
    gs_glyph_info_t info;
    double wxy[2];

    if (glyph == GS_NO_GLYPH) {
	glyph = font->procs.encode_char(font, chr, GLYPH_SPACE_INDEX);
	if (glyph == GS_NO_GLYPH) {
	    glyph = cf_data(font)->notdef;
	}
    }
    /*
     * Type 1/2 outlines don't require a current point, but TrueType
     * outlines do.  We might want to fix this someday....
     */
    if ((code = gs_moveto(pgs, 0.0, 0.0)) < 0 ||
	(code = font->procs.glyph_info(font, glyph, NULL,
				       (GLYPH_INFO_WIDTH << wmode) |
				       GLYPH_INFO_BBOX |
				       GLYPH_INFO_OUTLINE_WIDTHS,
				       &info)) < 0
	)
	return code;
    wxy[0] = info.width[wmode].x;
    wxy[1] = info.width[wmode].y;
    wxy[2] = info.bbox.p.x;
    wxy[3] = info.bbox.p.y;
    wxy[4] = info.bbox.q.x;
    wxy[5] = info.bbox.q.y;
    if ((code = gs_text_setcachedevice(pte, wxy)) < 0 ||
	(code = font->procs.glyph_outline(font, glyph, &ctm_only(pgs),
					  pgs->path)) < 0
	)
	return code;
    if (font->PaintType != 0) {
	gs_setlinewidth(pgs, font->StrokeWidth);
	return gs_stroke(pgs);
    } else {
	return gs_fill(pgs);
    }
}

/* ---------------- Individual FontTypes ---------------- */

/* ------ Type 1 ------ */

private int
copied_type1_glyph_data(gs_font_type1 * pfont, gs_glyph glyph,
			gs_glyph_data_t *pgd)
{
    gs_copied_font_data_t *const cfdata = cf_data((gs_font *)pfont);
    gs_copied_glyph_t *pslot;
    int code = copied_glyph_slot(cfdata, glyph, &pslot);

    if (code < 0)
	return code;
    gs_glyph_data_from_string(pgd, pslot->gdata.data, pslot->gdata.size,
			      NULL);
    return 0;
}

private int
copied_type1_subr_data(gs_font_type1 * pfont, int subr_num, bool global,
		       gs_glyph_data_t *pgd)
{
    gs_copied_font_data_t *const cfdata = cf_data((gs_font *)pfont);
    const gs_subr_info_t *psi =
	(global ? &cfdata->global_subrs : &cfdata->subrs);

    if (subr_num < 0 || subr_num >= psi->count)
	return_error(gs_error_rangecheck);
    gs_glyph_data_from_string(pgd, psi->data + psi->starts[subr_num],
			      psi->starts[subr_num + 1] -
			        psi->starts[subr_num],
			      NULL);
    return 0;
}

private int
copied_type1_seac_data(gs_font_type1 * pfont, int ccode,
		       gs_glyph * pglyph, gs_glyph_data_t *pgd)
{
    /*
     * This can only be invoked if the components have already been
     * copied to their proper positions, so it is simple.
     */
    gs_glyph glyph = copied_encode_char((gs_font *)pfont, (gs_char)ccode,
					GLYPH_SPACE_NAME);

    if (glyph == GS_NO_GLYPH)
	return_error(gs_error_rangecheck);
    if (pglyph)
	*pglyph = glyph;
    if (pgd)
	return copied_type1_glyph_data(pfont, glyph, pgd);
    else
	return 0;
}

private int
copied_type1_push_values(void *callback_data, const fixed *values, int count)
{
    return_error(gs_error_unregistered);
}

private int
copied_type1_pop_value(void *callback_data, fixed *value)
{
    return_error(gs_error_unregistered);
}

private int
copy_font_type1(gs_font *font, gs_font *copied)
{
    gs_font_type1 *font1 = (gs_font_type1 *)font;
    gs_font_type1 *copied1 = (gs_font_type1 *)copied;
    gs_copied_font_data_t *const cfdata = cf_data(copied);
    int code;

    cfdata->notdef = find_notdef((gs_font_base *)font);
    code = copied_Encoding_alloc(copied);
    if (code < 0)
	return code;
    if ((code = copy_subrs(font1, false, &cfdata->subrs, copied->memory)) < 0 ||
	(code = copy_subrs(font1, true, &cfdata->global_subrs, copied->memory)) < 0
	) {
	gs_free_object(copied->memory, cfdata->Encoding,
		       "copy_font_type1(Encoding)");
	return code;
    }
    /*
     * We don't need real push/pop procedures, because we can't do anything
     * useful with fonts that have non-standard OtherSubrs anyway.
     */
    copied1->data.procs.glyph_data = copied_type1_glyph_data;
    copied1->data.procs.subr_data = copied_type1_subr_data;
    copied1->data.procs.seac_data = copied_type1_seac_data;
    copied1->data.procs.push_values = copied_type1_push_values;
    copied1->data.procs.pop_value = copied_type1_pop_value;
    copied1->data.proc_data = 0;
    return 0;
}

private int
copy_glyph_type1(gs_font *font, gs_glyph glyph, gs_font *copied)
{
    gs_glyph_data_t gdata;
    gs_font_type1 *font1 = (gs_font_type1 *)font;
    int code = font1->data.procs.glyph_data(font1, glyph, &gdata);
    int rcode;

    if (code < 0)
	return code;
    code = copy_glyph_data(font, glyph, copied, &gdata, NULL, 0);
    if (code < 0)
	return code;
    rcode = code;
    if (code == 0)
	code = copy_glyph_name(font, glyph, copied, glyph);
    return (code < 0 ? code : rcode);
}

private int
copied_type1_glyph_outline(gs_font *font, gs_glyph glyph,
			   const gs_matrix *pmat, gx_path *ppath)
{
    /*
     * This code duplicates much of zcharstring_outline in zchar1.c.
     * This is unfortunate, but we don't see a simple way to refactor the
     * code to avoid it.
     */
    gs_glyph_data_t gdata;
    gs_font_type1 *const pfont1 = (gs_font_type1 *)font;
    int code = pfont1->data.procs.glyph_data(pfont1, glyph, &gdata);
    const gs_glyph_data_t *pgd = &gdata;
    gs_type1_state cis;
    static const gs_log2_scale_point no_scale = {0, 0};
    gs_imager_state gis;

    if (code < 0)
	return code;
    if (pgd->bits.size <= max(pfont1->data.lenIV, 0))
	return_error(gs_error_invalidfont);
    /* Initialize just enough of the imager state. */
    if (pmat)
	gs_matrix_fixed_from_matrix(&gis.ctm, pmat);
    else {
	gs_matrix imat;

	gs_make_identity(&imat);
	gs_matrix_fixed_from_matrix(&gis.ctm, &imat);
    }
    gis.flatness = 0;
    code = gs_type1_interp_init(&cis, &gis, ppath, &no_scale, true, 0,
				pfont1);
    if (code < 0)
	return code;
    cis.charpath_flag = true;	/* suppress hinting */
    /* Continue interpreting. */
    for (;;) {
	int value;

	code = pfont1->data.interpret(&cis, pgd, &value);
	switch (code) {
	case 0:		/* all done */
	    /* falls through */
	default:		/* code < 0, error */
	    return code;
	case type1_result_callothersubr:	/* unknown OtherSubr */
	    return_error(gs_error_rangecheck); /* can't handle it */
	case type1_result_sbw:	/* [h]sbw, just continue */
	    pgd = 0;
	}
    }
}

private const gs_copied_font_procs_t copied_procs_type1 = {
    copy_font_type1, copy_glyph_type1, copied_char_add_encoding,
    named_glyph_slot_hashed,
    copied_encode_char, gs_type1_glyph_info, copied_type1_glyph_outline
};

/* ------ Type 42 ------ */

private int
copied_type42_string_proc(gs_font_type42 *font, ulong offset, uint len,
			  const byte **pstr)
{
    gs_copied_font_data_t *const cfdata = font->data.proc_data;

    if (offset + len > cfdata->data_size)
	return_error(gs_error_rangecheck);
    *pstr = cfdata->data + offset;
    return 0;
}

private int
copied_type42_get_outline(gs_font_type42 *font, uint glyph_index,
			  gs_glyph_data_t *pgd)
{
    gs_copied_font_data_t *const cfdata = font->data.proc_data;
    gs_copied_glyph_t *pcg;

    if (glyph_index >= cfdata->glyphs_size)
	return_error(gs_error_rangecheck);
    pcg = &cfdata->glyphs[glyph_index];
    if (!pcg->used)
	gs_glyph_data_from_null(pgd);
    else
	gs_glyph_data_from_string(pgd, pcg->gdata.data, pcg->gdata.size, NULL);
    return 0;
}

private int
copied_type42_get_metrics(gs_font_type42 * pfont, uint glyph_index,
			  int wmode, float sbw[4])
{
    /* Check whether we have metrics for this (glyph,wmode) pair. */
    gs_copied_font_data_t *const cfdata = pfont->data.proc_data;
    gs_copied_glyph_t *pcg;

    if (glyph_index >= cfdata->glyphs_size)
	return_error(gs_error_rangecheck);
    pcg = &cfdata->glyphs[glyph_index];
    if (!(pcg->used & (HAS_SBW0 << wmode)))
	return_error(gs_error_undefined);
    return gs_type42_default_get_metrics(pfont, glyph_index, wmode, sbw);
}

private uint
copied_type42_get_glyph_index(gs_font_type42 *font, gs_glyph glyph)
{
    gs_copied_font_data_t *const cfdata = cf_data((gs_font *)font);
    gs_copied_glyph_t *pcg;
    int code = copied_glyph_slot(cfdata, glyph, &pcg);

    if (code < 0)
	return 0;		/* undefined */
    return pcg - cfdata->glyphs;
}

private int
copy_font_type42(gs_font *font, gs_font *copied)
{
    gs_font_type42 *const font42 = (gs_font_type42 *)font;
    gs_font_type42 *const copied42 = (gs_font_type42 *)copied;
    gs_copied_font_data_t *const cfdata = cf_data(copied);
    /*
     * We "write" the font, aside from the glyphs, into an in-memory
     * structure, and access it from there.
     * We allocate room at the end of the copied data for fake hmtx/vmtx.
     */
    uint extra = font42->data.numGlyphs * 8;
    stream fs;
    int code;

    cfdata->notdef = find_notdef((gs_font_base *)font);
    code = copied_Encoding_alloc(copied);
    if (code < 0)
	return code;
    swrite_position_only(&fs);
    code = psf_write_truetype_stripped(&fs, font42);
    code = copied_data_alloc(copied, &fs, extra, code);
    if (code < 0)
	goto fail;
    psf_write_truetype_stripped(&fs, font42);
    copied42->data.string_proc = copied_type42_string_proc;
    copied42->data.proc_data = cfdata;
    code = gs_type42_font_init(copied42);
    if (code < 0)
	goto fail2;
    copied42->data.get_outline = copied_type42_get_outline;
    copied42->data.metrics[0].numMetrics =
	copied42->data.metrics[1].numMetrics =
	extra / 8;
    copied42->data.metrics[0].offset = cfdata->data_size - extra;
    copied42->data.metrics[1].offset = cfdata->data_size - extra / 2;
    copied42->data.metrics[0].length =
	copied42->data.metrics[1].length =
	extra / 2;
    memset(cfdata->data + cfdata->data_size - extra, 0, extra);
    copied42->data.get_metrics = copied_type42_get_metrics;
    copied42->data.get_glyph_index = copied_type42_get_glyph_index;
    copied42->data.numGlyphs = font42->data.numGlyphs;
    return 0;
 fail2:
    gs_free_object(copied->memory, cfdata->data,
		   "copy_font_type42(data)");
 fail:
    gs_free_object(copied->memory, cfdata->Encoding,
		   "copy_font_type42(Encoding)");
    return code;
}

private int
copy_glyph_type42(gs_font *font, gs_glyph glyph, gs_font *copied)
{
    gs_glyph_data_t gdata;
    gs_font_type42 *font42 = (gs_font_type42 *)font;
    gs_font_type42 *const copied42 = (gs_font_type42 *)copied;
    uint gid = font42->data.get_glyph_index(font42, glyph);
    int code = font42->data.get_outline(font42, gid, &gdata);
    int rcode;
    gs_copied_font_data_t *const cfdata = cf_data(copied);
    gs_copied_glyph_t *pcg;
    float sbw[4];
    double factor = font42->data.unitsPerEm;
    int i;

    if (code < 0)
	return code;
    code = copy_glyph_data(font, gid + GS_MIN_CID_GLYPH, copied, &gdata, NULL, 0);
    if (code < 0)
	return code;
    rcode = code;
    if (glyph < GS_MIN_CID_GLYPH)
	code = copy_glyph_name((gs_font *)font, glyph, copied,
			       gid + GS_MIN_CID_GLYPH);
    DISCARD(copied_glyph_slot(cfdata, glyph, &pcg)); /* can't fail */
    for (i = 0; i < 2; ++i) {
	if (font42->data.get_metrics(font42, gid, i, sbw) >= 0) {
	    int sb = (int)(sbw[i] * factor);
	    uint width = (uint)(sbw[2 + i] * factor);
	    byte *pmetrics =
		cfdata->data + copied42->data.metrics[i].offset + gid * 4;

	    pmetrics[0] = (byte)(width >> 8);
	    pmetrics[1] = (byte)width;
	    pmetrics[2] = (byte)(sb >> 8);
	    pmetrics[3] = (byte)sb;
	    pcg->used |= HAS_SBW0 << i;
	}
	factor = -factor;	/* values are negated for WMode = 1 */
    }
    return (code < 0 ? code : rcode);
}

private const gs_copied_font_procs_t copied_procs_type42 = {
    copy_font_type42, copy_glyph_type42, copied_char_add_encoding,
    named_glyph_slot_linear,
    copied_encode_char, gs_type42_glyph_info, gs_type42_glyph_outline
};

/* ------ CIDFont shared ------ */

private int
copy_font_cid_common(gs_font *font, gs_font *copied, gs_font_cid_data *pcdata)
{
    return (copy_string(copied->memory, &pcdata->CIDSystemInfo.Registry,
			"Registry") |
	    copy_string(copied->memory, &pcdata->CIDSystemInfo.Ordering,
			"Ordering"));
}

/* ------ CIDFontType 0 ------ */

private int
copied_cid0_glyph_data(gs_font_base *font, gs_glyph glyph,
		       gs_glyph_data_t *pgd, int *pfidx)
{
    gs_font_cid0 *fcid0 = (gs_font_cid0 *)font;
    gs_copied_font_data_t *const cfdata = cf_data((gs_font *)font);
    gs_copied_glyph_t *pcg;
    int code = copied_glyph_slot(cfdata, glyph, &pcg);
    int fdbytes = fcid0->cidata.FDBytes;
    int i;

    if (pfidx)
	*pfidx = 0;
    if (code < 0) {
	if (pgd)
	    gs_glyph_data_from_null(pgd);
	return_error(gs_error_undefined);
    }
    if (pfidx)
	for (i = 0; i < fdbytes; ++i)
	    *pfidx = (*pfidx << 8) + pcg->gdata.data[i];
    if (pgd)
	gs_glyph_data_from_string(pgd, pcg->gdata.data + fdbytes,
				  pcg->gdata.size - fdbytes, NULL);
    return 0;
}
private int
copied_sub_type1_glyph_data(gs_font_type1 * pfont, gs_glyph glyph,
			    gs_glyph_data_t *pgd)
{
    return
      copied_cid0_glyph_data((gs_font_base *)cf_data((gs_font *)pfont)->parent,
			     glyph, pgd, NULL);
}

private int
cid0_subfont(gs_font *copied, gs_glyph glyph, gs_font_type1 **pfont1)
{
    int fidx;
    int code = copied_cid0_glyph_data((gs_font_base *)copied, glyph, NULL,
				      &fidx);

    if (code >= 0)
	*pfont1 = ((gs_font_cid0 *)copied)->cidata.FDArray[fidx];
    return code;
}

private int
copied_cid0_glyph_info(gs_font *font, gs_glyph glyph, const gs_matrix *pmat,
		       int members, gs_glyph_info_t *info)
{
    gs_font_type1 *subfont1;
    int code = cid0_subfont(font, glyph, &subfont1);

    if (code < 0)
	return code;
    return subfont1->procs.glyph_info((gs_font *)subfont1, glyph, pmat,
				      members, info);
}

private int
copied_cid0_glyph_outline(gs_font *font, gs_glyph glyph,
			  const gs_matrix *pmat, gx_path *ppath)
{
    gs_font_type1 *subfont1;
    int code = cid0_subfont(font, glyph, &subfont1);

    if (code < 0)
	return code;
    return subfont1->procs.glyph_outline((gs_font *)subfont1, glyph, pmat,
					 ppath);
}

private int
copy_font_cid0(gs_font *font, gs_font *copied)
{
    gs_font_cid0 *copied0 = (gs_font_cid0 *)copied;
    gs_copied_font_data_t *const cfdata = cf_data(copied);
    gs_font_type1 **FDArray =
	gs_alloc_struct_array(copied->memory, copied0->cidata.FDArray_size,
			      gs_font_type1 *,
			      &st_gs_font_type1_ptr_element, "FDArray");
    int i = 0, code;

    if (FDArray == 0)
	return_error(gs_error_VMerror);
    code = copy_font_cid_common(font, copied, &copied0->cidata.common);
    if (code < 0)
	goto fail;
    for (; i < copied0->cidata.FDArray_size; ++i) {
	gs_font *subfont = (gs_font *)copied0->cidata.FDArray[i];
	gs_font_type1 *subfont1 = (gs_font_type1 *)subfont;
	gs_font *subcopy;
	gs_font_type1 *subcopy1;
	gs_copied_font_data_t *subdata;

	if (i == 0) {
	    /* copy_subrs requires a Type 1 font, even for GSubrs. */
	    code = copy_subrs(subfont1, true, &cfdata->global_subrs,
			      copied->memory);
	    if (code < 0)
		goto fail;
	}
	code = gs_copy_font(subfont, copied->memory, &subcopy);
	if (code < 0)
	    goto fail;
	subcopy1 = (gs_font_type1 *)subcopy;
	subdata = cf_data(subcopy);
	subdata->parent = copied0;
	gs_free_object(copied->memory, subdata->Encoding,
		       "copy_font_cid0(Encoding)");
	subdata->Encoding = 0;
	/*
	 * Share the glyph data and global_subrs with the parent.  This
	 * allows copied_type1_glyph_data in the subfont to do the right
	 * thing.
	 */
	gs_free_object(copied->memory, subdata->names,
		       "copy_font_cid0(subfont names)");
	gs_free_object(copied->memory, subdata->glyphs,
		       "copy_font_cid0(subfont glyphs)");
	subcopy1->data.procs.glyph_data = copied_sub_type1_glyph_data;
	subdata->glyphs = cfdata->glyphs;
	subdata->glyphs_size = cfdata->glyphs_size;
	subdata->names = 0;
	subdata->global_subrs = cfdata->global_subrs;
	FDArray[i] = subcopy1;
    }
    cfdata->notdef = GS_MIN_CID_GLYPH;
    copied0->cidata.FDArray = FDArray;
    copied0->cidata.FDBytes =
	(copied0->cidata.FDArray_size <= 1 ? 0 :
	 copied0->cidata.FDArray_size <= 256 ? 1 : 2);
    copied0->cidata.glyph_data = copied_cid0_glyph_data;
    return 0;
 fail:
    while (--i >= 0)
	gs_free_object(copied->memory, FDArray[i], "copy_font_cid0(subfont)");
    gs_free_object(copied->memory, FDArray, "FDArray");
    return code;
}

private int
copy_glyph_cid0(gs_font *font, gs_glyph glyph, gs_font *copied)
{
    gs_glyph_data_t gdata;
    gs_font_cid0 *fcid0 = (gs_font_cid0 *)font;
    gs_font_cid0 *copied0 = (gs_font_cid0 *)copied;
    int fdbytes = copied0->cidata.FDBytes;
    int fidx;
    int code = fcid0->cidata.glyph_data((gs_font_base *)font, glyph,
					&gdata, &fidx);
    byte prefix[MAX_FDBytes];
    int i;

    if (code < 0)
	return code;
    for (i = fdbytes - 1; i >= 0; --i, fidx >>= 8)
	prefix[i] = (byte)fidx;
    if (fidx != 0)
	return_error(gs_error_rangecheck);
    return copy_glyph_data(font, glyph, copied, &gdata, prefix, fdbytes);
}

private const gs_copied_font_procs_t copied_procs_cid0 = {
    copy_font_cid0, copy_glyph_cid0, copied_no_add_encoding,
    named_glyph_slot_none,
    gs_no_encode_char, copied_cid0_glyph_info, copied_cid0_glyph_outline
};

/* ------ CIDFontType 2 ------ */

private int
copied_cid2_CIDMap_proc(gs_font_cid2 *fcid2, gs_glyph glyph)
{
    uint cid = glyph - GS_MIN_CID_GLYPH;
    gs_copied_font_data_t *const cfdata = cf_data((gs_font *)fcid2);
    const ushort *CIDMap = cfdata->CIDMap;

    if (glyph < GS_MIN_CID_GLYPH || cid >= fcid2->cidata.common.CIDCount)
	return_error(gs_error_rangecheck);
    return CIDMap[cid];
}

private int
copy_font_cid2(gs_font *font, gs_font *copied)
{
    gs_font_cid2 *copied2 = (gs_font_cid2 *)copied;
    gs_copied_font_data_t *const cfdata = cf_data(copied);
    int code;
    int CIDCount = copied2->cidata.common.CIDCount;
    ushort *CIDMap = (ushort *)
	gs_alloc_byte_array(copied->memory, CIDCount, sizeof(ushort),
			    "copy_font_cid2(CIDMap");

    if (CIDMap == 0)
	return_error(gs_error_VMerror);
    code = copy_font_cid_common(font, copied, &copied2->cidata.common);
    if (code < 0 ||
	(code = copy_font_type42(font, copied)) < 0
	) {
	gs_free_object(copied->memory, CIDMap, "copy_font_cid2(CIDMap");
	return code;
    }
    cfdata->notdef = GS_MIN_CID_GLYPH;
    memset(CIDMap, 0, CIDCount * sizeof(*CIDMap));
    cfdata->CIDMap = CIDMap;
    copied2->cidata.MetricsCount = 0;
    copied2->cidata.CIDMap_proc = copied_cid2_CIDMap_proc;
    return 0;
}

private int
copy_glyph_cid2(gs_font *font, gs_glyph glyph, gs_font *copied)
{
    gs_font_cid2 *fcid2 = (gs_font_cid2 *)font;
    gs_copied_font_data_t *const cfdata = cf_data(copied);
    uint cid = glyph - GS_MIN_CID_GLYPH;
    int gid;
    int code;

    gid = fcid2->cidata.CIDMap_proc(fcid2, glyph);
    if (gid < 0 || gid >= cfdata->glyphs_size)
	return_error(gs_error_rangecheck);
    if (cfdata->CIDMap[cid] != 0 && cfdata->CIDMap[cid] != gid)
	return_error(gs_error_invalidaccess);
    code = copy_glyph_type42(font, GS_MIN_CID_GLYPH + gid, copied);
    if (code < 0)
	return code;
    cfdata->CIDMap[cid] = gid;
    return code;
}

private const gs_copied_font_procs_t copied_procs_cid2 = {
    copy_font_cid2, copy_glyph_cid2, copied_no_add_encoding,
    named_glyph_slot_none,
    gs_no_encode_char, gs_type42_glyph_info, gs_type42_glyph_outline
};

/* ---------------- Public ---------------- */

/*
 * Procedure vector for copied fonts.
 */
private font_proc_font_info(copied_font_info);
private font_proc_enumerate_glyph(copied_enumerate_glyph);
private const gs_font_procs copied_font_procs = {
    0,				/* define_font, not supported */
    0,				/* make_font, not supported */
    copied_font_info,
    gs_default_same_font,
    0,				/* encode_char, varies by FontType */
    copied_enumerate_glyph,
    0,				/* glyph_info, varies by FontType */
    0,				/* glyph_outline, varies by FontType */
    copied_glyph_name,
    gs_default_init_fstack,
    gs_default_next_char_glyph,
    copied_build_char
};

/*
 * Copy a font, aside from its glyphs.
 */
int
gs_copy_font(gs_font *font, gs_memory_t *mem, gs_font **pfont_new)
{
    gs_memory_type_ptr_t fstype = gs_object_type(font->memory, font);
    uint fssize = gs_struct_type_size(fstype);
    gs_font *copied = 0;
    gs_copied_font_data_t *cfdata = 0;
    gs_font_info_t info;
    gs_copied_glyph_t *glyphs = 0;
    uint glyphs_size;
    gs_copied_glyph_name_t *names = 0;
    bool have_names = false;
    const gs_copied_font_procs_t *procs;
    int code;

    /*
     * Check for a supported FontType, and compute the size of its
     * copied glyph table.
     */
    switch (font->FontType) {
    case ft_TrueType:
	procs = &copied_procs_type42;
	glyphs_size = ((gs_font_type42 *)font)->data.numGlyphs;
	have_names = true;
	break;
    case ft_encrypted:
    case ft_encrypted2:
	procs = &copied_procs_type1;
	/* Count the glyphs. */
	glyphs_size = 0;
	{
	    int index = 0;
	    gs_glyph glyph;

	    while (font->procs.enumerate_glyph(font, &index, GLYPH_SPACE_NAME,
					       &glyph), index != 0)
		++glyphs_size;
	}
	/*
	 * Make names_size a power of 2 to ensure termination of the loop in
	 * named_glyphs_slot_hashed, q.v.
	 */
	glyphs_size = glyphs_size * 3 / 2;
	while (glyphs_size & (glyphs_size - 1))
	    glyphs_size = (glyphs_size | (glyphs_size - 1)) + 1;
	if (glyphs_size < 256)	/* probably incremental font */
	    glyphs_size = 256;
	have_names = true;
	break;
    case ft_CID_encrypted:
	procs = &copied_procs_cid0;
	glyphs_size = ((gs_font_cid0 *)font)->cidata.common.CIDCount;
	break;
    case ft_CID_TrueType:
	procs = &copied_procs_cid2;
	/* Glyphs are indexed by GID, not by CID. */
	glyphs_size = ((gs_font_cid2 *)font)->data.numGlyphs;
	break;
    default:
	return_error(gs_error_rangecheck);
    }

    /* Get the font_info for copying. */

    memset(&info, 0, sizeof(info));
    info.Flags_requested = ~0;
    code = font->procs.font_info(font, NULL, ~0, &info);
    if (code < 0)
	return code;

    /* Allocate the generic copied information. */

    glyphs = gs_alloc_struct_array(mem, glyphs_size, gs_copied_glyph_t,
				   &st_gs_copied_glyph,
				   "gs_copy_font(glyphs)");
    if (have_names != 0)
	names = gs_alloc_struct_array(mem, glyphs_size, gs_copied_glyph_name_t,
				      &st_gs_copied_glyph_name,
				      "gs_copy_font(names)");
    copied = gs_alloc_struct(mem, gs_font, fstype,
			     "gs_copy_font(copied font)");
    cfdata = gs_alloc_struct(mem, gs_copied_font_data_t,
			    &st_gs_copied_font_data,
			    "gs_copy_font(wrapper data)");
    if (cfdata)
	memset(cfdata, 0, sizeof(*cfdata));
    if (glyphs == 0 || (names == 0 && have_names) || copied == 0 ||
	cfdata == 0
	) {
	code = gs_note_error(gs_error_VMerror);
	goto fail;
    }
    cfdata->info = info;
    if ((code = (copy_string(mem, &cfdata->info.Copyright,
			     "gs_copy_font(Copyright)") |
		 copy_string(mem, &cfdata->info.Notice,
			     "gs_copy_font(Notice)") |
		 copy_string(mem, &cfdata->info.FamilyName,
			     "gs_copy_font(FamilyName)") |
		 copy_string(mem, &cfdata->info.FullName,
			     "gs_copy_font(FullName)"))) < 0
	)
	goto fail;

    /* Initialize the copied font. */

    memcpy(copied, font, fssize);
    copied->next = copied->prev = 0;
    copied->memory = mem;
    copied->is_resource = false;
    gs_notify_init(&copied->notify_list, mem);
    copied->base = copied;
    copied->client_data = cfdata;
    copied->procs = copied_font_procs;
    copied->procs.encode_char = procs->encode_char;
    copied->procs.glyph_info = procs->glyph_info;
    copied->procs.glyph_outline = procs->glyph_outline;
    {
	gs_font_base *bfont = (gs_font_base *)copied;

	bfont->FAPI = 0;
	bfont->FAPI_font_data = 0;
	code = uid_copy(&bfont->UID, mem, "gs_copy_font(UID)");
	if (code < 0)
	    goto fail;
    }

    cfdata->procs = procs;
    memset(glyphs, 0, glyphs_size * sizeof(*glyphs));
    cfdata->glyphs = glyphs;
    cfdata->glyphs_size = glyphs_size;
    if (names)
	memset(names, 0, glyphs_size * sizeof(*names));
    cfdata->names = names;

    /* Do FontType-specific initialization. */

    code = procs->finish_copy_font(font, copied);
    if (code < 0)
	goto fail;

    *pfont_new = copied;
    if (cfdata->notdef != GS_NO_GLYPH)
	code = gs_copy_glyph(font, cfdata->notdef, copied);
    return code;

 fail:
    /* Free storage and exit. */
    if (cfdata) {
	uncopy_string(mem, &cfdata->info.FullName,
		      "gs_copy_font(FullName)");
	uncopy_string(mem, &cfdata->info.FamilyName,
		      "gs_copy_font(FamilyName)");
	uncopy_string(mem, &cfdata->info.Notice,
		      "gs_copy_font(Notice)");
	uncopy_string(mem, &cfdata->info.Copyright,
		      "gs_copy_font(Copyright)");
	gs_free_object(mem, cfdata, "gs_copy_font(wrapper data)");
    }
    gs_free_object(mem, copied, "gs_copy_font(copied font)");
    gs_free_object(mem, names, "gs_copy_font(names)");
    gs_free_object(mem, glyphs, "gs_copy_font(glyphs)");
    return code;
}

/*
 * Copy a glyph, including any sub-glyphs.
 */
int
gs_copy_glyph(gs_font *font, gs_glyph glyph, gs_font *copied)
{
    int code;
#define MAX_GLYPH_PIECES 32	/* arbitrary, but 10 is too small */
    gs_glyph glyphs[MAX_GLYPH_PIECES];
    uint count = 0, i;

    if (copied->procs.font_info != copied_font_info)
	return_error(gs_error_rangecheck);
    code = cf_data(copied)->procs->copy_glyph(font, glyph, copied);
    if (code != 0)
	return code;
    /* Copy any sub-glyphs. */
    psf_add_subset_pieces(glyphs, &count, MAX_GLYPH_PIECES, MAX_GLYPH_PIECES,
			  font);
    if (count > MAX_GLYPH_PIECES)
	return_error(gs_error_limitcheck);
    for (i = 0; i < count; ++i) {
	code = gs_copy_glyph(font, glyphs[i], copied);
	if (code < 0)
	    return code;
    }
    return 0;
#undef MAX_GLYPH_PIECES
}

/*
 * Add an Encoding entry to a copied font.  The glyph need not already have
 * been copied.
 */
int
gs_copied_font_add_encoding(gs_font *copied, gs_char chr, gs_glyph glyph)
{
    gs_copied_font_data_t *const cfdata = cf_data(copied);

    if (copied->procs.font_info != copied_font_info)
	return_error(gs_error_rangecheck);
    return cfdata->procs->add_encoding(copied, chr, glyph);
}

/*
 * Copy all the glyphs and, if relevant, Encoding entries from a font.  This
 * is equivalent to copying the glyphs and Encoding entries individually,
 * and returns errors under the same conditions.
 */
int
gs_copy_font_complete(gs_font *font, gs_font *copied)
{
    int index, code = 0;
    gs_glyph_space_t space = GLYPH_SPACE_NAME;
    gs_glyph glyph;

    /*
     * For Type 1 fonts and CIDFonts, enumerating the glyphs using
     * GLYPH_SPACE_NAME will cover all the glyphs.  (The "names" of glyphs
     * in CIDFonts are CIDs, but that is not a problem.)  For Type 42 fonts,
     * however, we have to copy by name once, so that we also copy the
     * name-to-GID mapping (the CharStrings dictionary in PostScript), and
     * then copy again by GID, to cover glyphs that don't have names.
     */
    for (;;) {
	for (index = 0;
	     code >= 0 &&
		 (font->procs.enumerate_glyph(font, &index, space, &glyph),
		  index != 0);
	     )
	    code = gs_copy_glyph(font, glyph, copied);
	/* For Type 42 fonts, if we copied by name, now copy again by index. */
	if (space == GLYPH_SPACE_NAME && font->FontType == ft_TrueType)
	    space = GLYPH_SPACE_INDEX;
	else
	    break;
    }
    if (cf_data(copied)->Encoding != 0)
	for (index = 0; code >= 0 && index < 256; ++index) {
	    glyph = font->procs.encode_char(font, (gs_char)index,
					    GLYPH_SPACE_NAME);
	    if (glyph != GS_NO_GLYPH)
		code = gs_copied_font_add_encoding(copied, (gs_char)index,
						   glyph);
	}
    return code;
}
