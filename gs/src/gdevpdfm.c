/* Copyright (C) 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.

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

/*Id: gdevpdfm.c  */
/* pdfmark processing for PDF-writing driver */
#include "memory_.h"
#include "string_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsutil.h"		/* for bytes_compare */
#include "gdevpdfx.h"
#include "scanchar.h"

/* GC descriptors */
private_st_pdf_article();
private_st_pdf_named_dest();

/*
 * The pdfmark pseudo-parameter indicates the occurrence of a pdfmark
 * operator in the input file.  Its "value" is the arguments of the operator,
 * passed through essentially unchanged:
 *      (key, value)*, CTM, type
 */

/*
 * Define the pdfmark types we know about.
 */
private pdfmark_proc(pdfmark_ANN);
private pdfmark_proc(pdfmark_LNK);
private pdfmark_proc(pdfmark_OUT);
private pdfmark_proc(pdfmark_ARTICLE);
private pdfmark_proc(pdfmark_DEST);
private pdfmark_proc(pdfmark_PS);
private pdfmark_proc(pdfmark_PAGES);
private pdfmark_proc(pdfmark_PAGE);
private pdfmark_proc(pdfmark_DOCINFO);
private pdfmark_proc(pdfmark_DOCVIEW);
private const pdfmark_name mark_names[] =
{
    {"ANN", pdfmark_ANN, pdfmark_nameable},
    {"LNK", pdfmark_LNK, pdfmark_nameable},
    {"OUT", pdfmark_OUT, 0},
    {"ARTICLE", pdfmark_ARTICLE, 0},
    {"DEST", pdfmark_DEST, pdfmark_nameable},
    {"PS", pdfmark_PS, pdfmark_nameable},
    {"PAGES", pdfmark_PAGES, 0},
    {"PAGE", pdfmark_PAGE, 0},
    {"DOCINFO", pdfmark_DOCINFO, 0},
    {"DOCVIEW", pdfmark_DOCVIEW, 0},
    {0, 0}
};
private const pdfmark_name *const mark_name_tables[2] =
{
    mark_names,
    pdfmark_names_named
};

/* ---------------- Public entry points ---------------- */

/* Compare a C string and a gs_param_string. */
bool
pdf_key_eq(const gs_param_string * pcs, const char *str)
{
    return (strlen(str) == pcs->size &&
	    !strncmp(str, (const char *)pcs->data, pcs->size));
}

/* Scan an integer out of a parameter string. */
int
pdfmark_scan_int(const gs_param_string * pstr, int *pvalue)
{
#define max_int_str 20
    uint size = pstr->size;
    char str[max_int_str + 1];

    if (size > max_int_str)
	return_error(gs_error_limitcheck);
    memcpy(str, pstr->data, size);
    str[size] = 0;
    return (sscanf(str, "%d", pvalue) == 1 ? 0 :
	    gs_note_error(gs_error_rangecheck));
#undef max_int_str
}

/* Process a pdfmark. */
int
pdfmark_process(gx_device_pdf * pdev, const gs_param_string_array * pma)
{
    const gs_param_string *data = pma->data;
    uint size = pma->size;
    const gs_param_string *pts = &data[size - 1];
    const gs_param_string *objname = 0;
    gs_matrix ctm;
    int n;
    int code = 0;

    if (size < 2 ||
	sscanf((const char *)pts[-1].data, "[%g %g %g %g %g %g]",
	       &ctm.xx, &ctm.xy, &ctm.yx, &ctm.yy, &ctm.tx, &ctm.ty) != 6
	)
	return_error(gs_error_rangecheck);
    /*
     * Our coordinate system is scaled so that user space is always
     * default user space.  Adjust the CTM to match this.
     */
    {
	double xscale = 72.0 / pdev->HWResolution[0], yscale = 72.0 / pdev->HWResolution[1];

	ctm.xx *= xscale, ctm.xy *= yscale;
	ctm.yx *= xscale, ctm.yy *= yscale;
	ctm.tx *= xscale, ctm.ty *= yscale;
    }
    size -= 2;			/* remove CTM & pdfmark name */
    for (n = 0; n < countof(mark_name_tables); ++n) {
	const pdfmark_name *pmn;

	for (pmn = mark_name_tables[n]; pmn->mname != 0; ++pmn)
	    if (pdf_key_eq(pts, pmn->mname)) {
		int odd_ok = (pmn->options & pdfmark_odd_ok) != 0;
		int j;

		if (size & !odd_ok)
		    return_error(gs_error_rangecheck);
		if (pmn->options & pdfmark_nameable) {
		    /* Look for an object name. */
		    for (j = 0; j < size; j += 2) {
			if (pdf_key_eq(&data[j], "_objdef")) {
			    objname = &data[j + 1];
			    if (!pdfmark_objname_is_valid(objname->data,
							  objname->size)
				)
				return_error(gs_error_rangecheck);
			    break;
			}
		    }
		}
		/* Copy the pairs to a writable array. */
		{
		    uint count = (objname ? size - 2 : size);
		    gs_memory_t *mem = pdev->pdf_memory;
		    gs_param_string *pairs = (gs_param_string *)
		    gs_alloc_byte_array(mem, count, sizeof(gs_param_string),
					"pdfmark_process(pairs)");

		    if (objname) {
			memcpy(pairs, data, j * sizeof(*data));
			memcpy(pairs + j, data + j + 2, (count - j) * sizeof(*data));
			size -= 2;
		    } else {
			memcpy(pairs, data, count * sizeof(*data));
		    }
		    /* Substitute object references for names. */
		    for (j = (pmn->options & pdfmark_keep_name ? 1 : 1 - odd_ok);
			 j < size; j += 2 - odd_ok
			) {
			code = pdfmark_replace_names(pdev, &pairs[j], &pairs[j]);
			if (code < 0) {
			    gs_free_object(mem, pairs, "pdfmark_process(pairs)");
			    goto out;
			}
		    }
		    code = (*pmn->proc) (pdev, pairs, size, &ctm, objname);
		    gs_free_object(mem, pairs, "pdfmark_process(pairs)");
		}
		goto out;
	    }
    }
  out:return code;
}

/* ---------------- Utilities ---------------- */

/* Find a key in a dictionary. */
private bool
pdfmark_find_key(const char *key, const gs_param_string * pairs, uint count,
		 gs_param_string * pstr)
{
    uint i;

    for (i = 0; i < count; i += 2)
	if (pdf_key_eq(&pairs[i], key)) {
	    *pstr = pairs[i + 1];
	    return true;
	}
    pstr->data = 0;
    pstr->size = 0;
    return false;
}

/*
 * Get the page number for a page referenced by number or as /Next or /Prev.
 * The result may be 0 if the page number is 0 or invalid.
 */
private int
pdfmark_page_number(gx_device_pdf * pdev, const gs_param_string * pnstr)
{
    int page = pdev->next_page + 1;

    if (pnstr->data == 0);
    else if (pdf_key_eq(pnstr, "/Next"))
	++page;
    else if (pdf_key_eq(pnstr, "/Prev"))
	--page;
    else if (pdfmark_scan_int(pnstr, &page) < 0)
	page = 0;
    return page;
}

/* Construct a destination string specified by /Page and/or /View. */
/* Return 0 if none (but still fill in a default), 1 if present, */
/* <0 if error. */
private int
pdfmark_make_dest(char dstr[max_dest_string], gx_device_pdf * pdev,
		  const gs_param_string * pairs, uint count)
{
    gs_param_string page_string, view_string;
    bool present =
    pdfmark_find_key("Page", pairs, count, &page_string) |
    pdfmark_find_key("View", pairs, count, &view_string);
    int page = pdfmark_page_number(pdev, &page_string);
    gs_param_string action;
    int len;

    if (view_string.size == 0)
	param_string_from_string(view_string, "[/XYZ 0 0 1]");
    if (page == 0)
	strcpy(dstr, "[null ");
    else if (pdfmark_find_key("Action", pairs, count, &action) &&
	     pdf_key_eq(&action, "/GoToR")
	)
	sprintf(dstr, "[%d ", page - 1);
    else
	sprintf(dstr, "[%ld 0 R ", pdf_page_id(pdev, page));
    len = strlen(dstr);
    if (len + view_string.size > max_dest_string)
	return_error(gs_error_limitcheck);
    if (view_string.data[0] != '[' ||
	view_string.data[view_string.size - 1] != ']'
	)
	return_error(gs_error_rangecheck);
    memcpy(dstr + len, view_string.data + 1, view_string.size - 1);
    dstr[len + view_string.size - 1] = 0;
    return present;
}

/* Write pairs for a dictionary. */
private void
pdfmark_write_c_pair(const gx_device_pdf * pdev, const char *key,
		     const gs_param_string * pvalue)
{
    stream *s = pdev->strm;

    pputc(s, '/');
    pputs(s, key);
    switch (pvalue->data[0]) {
	default:
	    pputc(s, ' ');
	case '/':
	case '(':
	case '[':
	case '<':
	    break;
    }
    pdf_put_value(pdev, pvalue->data, pvalue->size);
    pputc(s, '\n');
}
private void
pdfmark_write_pair(const gx_device_pdf * pdev, const gs_param_string * pair)
{
    stream *s = pdev->strm;

    pdf_put_name(pdev, pair->data, pair->size);
    pputc(s, ' ');
    pdf_put_value(pdev, pair[1].data, pair[1].size);
    pputc(s, '\n');
}
private void
pdfmark_write_pairs_except(const gx_device_pdf * pdev,
    const gs_param_string * pairs, uint count, const char *const *skip_keys)
{
    uint i;
    static const char *const no_skip_keys[] =
    {0};

    if (skip_keys == 0)
	skip_keys = no_skip_keys;
    for (i = 0; i < count; i += 2) {
	const char *const *pkey;

	for (pkey = skip_keys; *pkey != 0; ++pkey)
	    if (pdf_key_eq(&pairs[i], *pkey))
		break;
	if (!*pkey)
	    pdfmark_write_pair(pdev, &pairs[i]);
    }
}
#define pdfmark_write_pairs(pdev, pairs, count)\
  pdfmark_write_pairs_except(pdev, pairs, count, NULL)

/* Append data to a saved string. */
int
pdfmark_save(gx_device_pdf * pdev, gs_string * pstr,
	     int (*proc) (P2(const gx_device_pdf *, void *)), void *pdata,
	     client_name_t cname)
{
    gs_memory_t *mem = pdev->pdf_memory;
    stream *s;
    int code = psdf_alloc_position_stream(&s, mem);
    stream *save = pdev->strm;

    if (code < 0)
	return code;
    pdev->strm = s;
    code = (*proc) (pdev, pdata);
    if (code >= 0) {
	uint pos = stell(s);
	uint size = pstr->size;
	byte *str;

	if (pstr->data == 0)
	    size = 0,
		str = gs_alloc_string(mem, pos, cname);
	else
	    str = gs_resize_string(mem, pstr->data, size, size + pos, cname);
	if (str == 0)
	    code = gs_note_error(gs_error_VMerror);
	else {
	    pstr->data = str;
	    swrite_string(s, str + size, pos);
	    code = (*proc) (pdev, pdata);
	    if (code >= 0)
		pstr->size = size + pos;
	}
    }
    pdev->strm = save;
    gs_free_object(mem, s, "pdfmark_save");
    return code;
}

/* Scan a Rect value. */
private int
pdfmark_scan_rect(gs_rect * prect, const gs_param_string * str,
		  const gs_matrix * pctm)
{
    uint size = str->size;
    double v[4];

#define max_rect_string_size 100
    char chars[max_rect_string_size + 3];
    int end_check;

    if (str->size > max_rect_string_size)
	return_error(gs_error_limitcheck);
    memcpy(chars, str->data, size);
    strcpy(chars + size, " 0");
    if (sscanf(chars, "[%lg %lg %lg %lg]%d",
	       &v[0], &v[1], &v[2], &v[3], &end_check) != 5
	)
	return_error(gs_error_rangecheck);
    gs_point_transform(v[0], v[1], pctm, &prect->p);
    gs_point_transform(v[2], v[3], pctm, &prect->q);
    return 0;
}

/* Write a Rect value. */
private void
pdfmark_write_rect(stream * s, const char *key, const gs_rect * prect)
{
    pprints1(s, "/%s", key);
    pprintg4(s, "[%g %g %g %g]\n",
	     prect->p.x, prect->p.y, prect->q.x, prect->q.y);
}

/*
 * Write the dictionary for an annotation or outline.  For some unfathomable
 * reason, PDF requires the following key substitutions relative to
 * pdfmarks:
 *   In annotation and link dictionaries:
 *     /Action => /A, /Color => /C, /Title => /T
 *   In outline directionaries:
 *     /Action => /A, but *not* /Color or /Title
 *   In Action subdictionaries:
 *     /Dest => /D, /File => /F, /Subtype => /S
 * and also the following substitutions:
 *     /Action /Launch /File xxx =>
 *       /A << /S /Launch /F xxx >>
 *     /Action /GoToR /File xxx /Dest yyy =>
 *       /A << /S /GoToR /F xxx /D yyy' >>
 *     /Action /Article /Dest yyy =>
 *       /A << /S /Thread /D yyy' >>
 * Also, \n in Contents strings must be replaced with \r.
 */

typedef struct ao_params_s {
    gx_device_pdf *pdev;	/* for pdfmark_make_dest */
    const char *subtype;	/* default Subtype in top-level dictionary */
    pdf_resource *pres;		/* resource for saving source page */
} ao_params_t;
private int
pdfmark_write_ao_pairs(const gx_device_pdf * pdev,
		       const gs_param_string * pairs, uint count,
	     const gs_matrix * pctm, ao_params_t * params, bool for_outline)
{
    stream *s = pdev->strm;

    const gs_param_string *Action = 0;
    const gs_param_string *File = 0;
    gs_param_string Dest;
    gs_param_string Subtype;
    uint i;
    int code;
    char dest[max_dest_string];

    Dest.data = 0;
    if (!for_outline) {
	code = pdfmark_make_dest(dest, params->pdev, pairs, count);
	if (code < 0)
	    return code;
	else if (code == 0)
	    Dest.data = 0;
	else
	    param_string_from_string(Dest, dest);
    }
    if (params->subtype)
	param_string_from_string(Subtype, params->subtype);
    else
	Subtype.data = 0;
    for (i = 0; i < count; i += 2) {
	const gs_param_string *pair = &pairs[i];
	long src_pg;

	if (params->pres != 0 && pdf_key_eq(pair, "SrcPg") &&
	    sscanf((const char *)pair[1].data, "%ld", &src_pg) == 1
	    )
	    params->pres->rid = src_pg - 1;
	else if (!for_outline && pdf_key_eq(pair, "Color"))
	    pdfmark_write_c_pair(pdev, "C", pair + 1);
	else if (!for_outline && pdf_key_eq(pair, "Title"))
	    pdfmark_write_c_pair(pdev, "T", pair + 1);
	else if (pdf_key_eq(pair, "Action"))
	    Action = pair;
	else if (pdf_key_eq(pair, "File"))
	    File = pair;
	else if (pdf_key_eq(pair, "Dest"))
	    Dest = pair[1];
	else if (pdf_key_eq(pair, "Page") || pdf_key_eq(pair, "View")) {
	    /* Make a destination even if this is for an outline. */
	    if (Dest.data == 0) {
		code = pdfmark_make_dest(dest, params->pdev, pairs, count);
		if (code < 0)
		    return code;
		param_string_from_string(Dest, dest);
	    }
	} else if (pdf_key_eq(pair, "Subtype"))
	    Subtype = pair[1];
	/*
	 * We also have to replace all occurrences of \n in Contents
	 * strings with \r.  Unfortunately, they probably have already
	 * been converted to \012....
	 */
	else if (pdf_key_eq(pair, "Contents")) {
	    const byte *astr = pair[1].data;
	    const uint asize = pair[1].size;
	    uint i;

	    pputs(s, "/Contents ");
	    for (i = 0; i < asize; ++i)
		if (asize - i >= 2 && !memcmp(astr + i, "\\n", 2) &&
		    (i == 0 || astr[i - 1] != '\\')
		    ) {
		    pputs(s, "\\r");
		    ++i;
		} else if (asize - i >= 4 && !memcmp(astr + i, "\\012", 4) &&
			   (i == 0 || astr[i - 1] != '\\')
		    ) {
		    pputs(s, "\\r");
		    i += 3;
		} else
		    pputc(s, astr[i]);
	    pputc(s, '\n');
	} else if (pdf_key_eq(pair, "Rect")) {
	    gs_rect rect;
	    int code = pdfmark_scan_rect(&rect, pair + 1, pctm);

	    if (code < 0)
		return code;
	    pdfmark_write_rect(s, "Rect", &rect);
	} else if (for_outline && pdf_key_eq(pair, "Count"))
	    DO_NOTHING;
	else
	    pdfmark_write_pair(pdev, pair);
    }
    if (!for_outline && pdf_key_eq(&Subtype, "Link")) {
	if (Action)
	    Dest.data = 0;
    }
    /* Now handle the deferred keys. */
    if (Action) {
	const byte *astr = Action[1].data;
	const uint asize = Action[1].size;

	pputs(s, "/A ");
	if ((File != 0 || Dest.data != 0) &&
	    (pdf_key_eq(Action + 1, "/Launch") ||
	     (pdf_key_eq(Action + 1, "/GoToR") && File) ||
	     pdf_key_eq(Action + 1, "/Article"))
	    ) {
	    pputs(s, "<<");
	    if (!for_outline) {
		/* We aren't sure whether this is really needed.... */
		pputs(s, "/Type/Action");
	    }
	    if (pdf_key_eq(Action + 1, "/Article"))
		pputs(s, "/S/Thread");
	    else
		pdfmark_write_c_pair(pdev, "S", Action + 1);
	    if (Dest.data) {
		pdfmark_write_c_pair(pdev, "D", &Dest);
		Dest.data = 0;	/* so we don't write it again */
	    }
	    if (File) {
		pdfmark_write_c_pair(pdev, "F", File + 1);
		File = 0;	/* so we don't write it again */
	    }
	    pputs(s, ">>");
	} else {
	    /* Replace occurrences of /Dest, /File, /Subtype. */
	    uint i;

#define key_is(key, len)\
  (asize - i > len && !memcmp(astr + i, key, len) &&\
   scan_char_decoder[astr[i + len]] > ctype_name)
#define replace_key(old_len, new_key)\
  BEGIN pputs(s, new_key); i += old_len - 1; END
	    for (i = 0; i < asize; ++i) {
		if (key_is("/Dest", 4))
		    replace_key(4, "/D");
		else if (key_is("/File", 4))
		    replace_key(4, "/F");
		else if (key_is("/Subtype", 8))
		    replace_key(8, "/S");
		else
		    pputc(s, astr[i]);
	    }
	}
#undef key_is
#undef replace_key
	pputc(s, '\n');
    }
    /*
     * If we have /Dest or /File without the right kind of action,
     * simply write it at the top level.  This doesn't seem right,
     * but I'm not sure what else to do.
     */
    if (Dest.data)
	pdfmark_write_c_pair(pdev, "Dest", &Dest);
    if (File)
	pdfmark_write_pair(pdev, File);
    if (Subtype.data)
	pdfmark_write_c_pair(pdev, "Subtype", &Subtype);
    return 0;
}

/* Copy an annotation dictionary. */
private int
pdfmark_annot(gx_device_pdf * pdev, gs_param_string * pairs, uint count,
	      const gs_matrix * pctm, const char *subtype)
{
    ao_params_t params;
    int code = pdf_begin_aside(pdev, &pdev->annots, NULL, &params.pres);
    stream *s = pdev->strm;

    if (code < 0)
	return code;
    params.pdev = pdev;
    params.subtype = subtype;
    params.pres->rid = pdev->next_page;
    pputs(s, "<</Type/Annot\n");
    code = pdfmark_write_ao_pairs(pdev, pairs, count, pctm, &params,
				  false);
    if (code < 0)
	return code;
    pputs(s, ">>\n");
    pdf_end_aside(pdev);
    return 0;
}

/* ANN pdfmark */
private int
pdfmark_ANN(gx_device_pdf * pdev, gs_param_string * pairs, uint count,
	    const gs_matrix * pctm, const gs_param_string * objname)
{
    return pdfmark_annot(pdev, pairs, count, pctm, "/Text");
}

/* LNK pdfmark (obsolescent) */
private int
pdfmark_LNK(gx_device_pdf * pdev, gs_param_string * pairs, uint count,
	    const gs_matrix * pctm, const gs_param_string * objname)
{
    return pdfmark_annot(pdev, pairs, count, pctm, "/Link");
}

/* Save pairs in a string. */
typedef struct pairs_params_s {
    const gs_param_string *pairs;
    uint count;
    const char *const *skip_keys;
    const gs_param_string *add_pairs;
    uint add_count;
} pairs_params_t;
private int
pairs_write(const gx_device_pdf * pdev, void *pdata)
{
    const pairs_params_t *ppp = (const pairs_params_t *)pdata;

    pdfmark_write_pairs_except(pdev, ppp->pairs, ppp->count,
			       ppp->skip_keys);
    pdfmark_write_pairs(pdev, ppp->add_pairs, ppp->add_count);
    return 0;
}
private int
pdfmark_save_edited_pairs(gx_device_pdf * pdev,
    const gs_param_string * pairs, uint count, const char *const *skip_keys,
	const gs_param_string * add_pairs, uint add_count, gs_string * pstr)
{
    pairs_params_t params;

    params.pairs = pairs;
    params.count = count;
    params.skip_keys = skip_keys;
    params.add_pairs = add_pairs;
    params.add_count = add_count;

    return pdfmark_save(pdev, pstr, pairs_write, &params,
			"pdfmark_save_edited_pairs");
}
#define pdfmark_save_pairs(pdev, pairs, count, pstr)\
  pdfmark_save_edited_pairs(pdev, pairs, count, NULL, NULL, 0, pstr)

/* Write out one node of the outline tree. */
private int
pdfmark_write_outline(gx_device_pdf * pdev, pdf_outline_node * pnode,
		      long next_id)
{
    stream *s;

    pdf_open_separate(pdev, pnode->id);
    s = pdev->strm;
    pputs(s, "<< ");
    pdf_write_saved_string(pdev, &pnode->action_string);
    if (pnode->count)
	pprintd1(s, "/Count %d ", pnode->count);
    pprintld1(s, "/Parent %ld 0 R\n", pnode->parent_id);
    if (pnode->prev_id)
	pprintld1(s, "/Prev %ld 0 R\n", pnode->prev_id);
    if (next_id)
	pprintld1(s, "/Next %ld 0 R\n", next_id);
    if (pnode->first_id)
	pprintld2(s, "/First %ld 0 R /Last %ld 0 R\n",
		  pnode->first_id, pnode->last_id);
    pputs(s, ">>\n");
    pdf_end_separate(pdev);
    return 0;
}

/* Adjust the parent's count when writing an outline node. */
private void
pdfmark_adjust_parent_count(pdf_outline_level * plevel)
{
    pdf_outline_level *parent = plevel - 1;
    int count = plevel->last.count;

    if (count > 0) {
	if (parent->last.count < 0)
	    parent->last.count -= count;
	else
	    parent->last.count += count;
    }
}

/* Close the current level of the outline tree. */
int
pdfmark_close_outline(gx_device_pdf * pdev)
{
    int depth = pdev->outline_depth;
    pdf_outline_level *plevel = &pdev->outline_levels[depth];
    int code;

    code = pdfmark_write_outline(pdev, &plevel->last, 0);
    if (code < 0)
	return code;
    if (depth > 0) {
	plevel[-1].last.last_id = plevel->last.id;
	pdfmark_adjust_parent_count(plevel);
	--plevel;
	if (plevel->last.count < 0)
	    pdev->closed_outline_depth--;
	pdev->outline_depth--;
    }
    return 0;
}

/* OUT pdfmark */
typedef struct out_pairs_params_s {
    const gs_param_string *pairs;
    uint count;
    const gs_matrix *pctm;
    ao_params_t ao;
} out_pairs_params_t;
private int
pdfmark_OUT_write(const gx_device_pdf * pdev, void *pdata)
{
    out_pairs_params_t *popp = (out_pairs_params_t *) pdata;

    return pdfmark_write_ao_pairs(pdev, popp->pairs, popp->count,
				  popp->pctm, &popp->ao, true);
}
private int
pdfmark_OUT(gx_device_pdf * pdev, gs_param_string * pairs, uint count,
	    const gs_matrix * pctm, const gs_param_string * no_objname)
{
    int depth = pdev->outline_depth;
    pdf_outline_level *plevel = &pdev->outline_levels[depth];
    int sub_count = 0;
    uint i;
    pdf_outline_node node;
    out_pairs_params_t params;
    int code;

    for (i = 0; i < count; i += 2) {
	const gs_param_string *pair = &pairs[i];

	if (pdf_key_eq(pair, "Count"))
	    pdfmark_scan_int(pair + 1, &sub_count);
    }
    if (sub_count != 0 && depth == max_outline_depth - 1)
	return_error(gs_error_limitcheck);
    node.action_string.data = 0;
    params.pctm = pctm;
    params.pairs = pairs;
    params.count = count;
    params.ao.pdev = pdev;
    params.ao.subtype = 0;
    params.ao.pres = 0;
    code = pdfmark_save(pdev, &node.action_string, pdfmark_OUT_write,
			&params, "pdfmark_OUT");
    if (code < 0)
	return code;
    if (pdev->outlines_id == 0)
	pdev->outlines_id = pdf_obj_ref(pdev);
    node.id = pdf_obj_ref(pdev);
    node.parent_id =
	(depth == 0 ? pdev->outlines_id : plevel[-1].last.id);
    node.prev_id = plevel->last.id;
    node.first_id = node.last_id = 0;
    node.count = sub_count;
    /* Add this node to the outline at the current level. */
    if (plevel->first.id == 0) {	/* First node at this level. */
	if (depth > 0)
	    plevel[-1].last.first_id = node.id;
	node.prev_id = 0;
	plevel->first = node;
    } else {			/* Write out the previous node. */
	if (depth > 0)
	    pdfmark_adjust_parent_count(plevel);
	pdfmark_write_outline(pdev, &plevel->last, node.id);
    }
    plevel->last = node;
    plevel->left--;
    if (!pdev->closed_outline_depth)
	pdev->outlines_open++;
    /* If this node has sub-nodes, descend one level. */
    if (sub_count != 0) {
	pdev->outline_depth++;
	++plevel;
	plevel->left = (sub_count > 0 ? sub_count : -sub_count);
	plevel->first.id = 0;
	if (sub_count < 0)
	    pdev->closed_outline_depth++;
    } else {
	while ((depth = pdev->outline_depth) > 0 &&
	       pdev->outline_levels[depth].left == 0
	    )
	    pdfmark_close_outline(pdev);
    }
    return 0;
}

/* Write an article bead. */
private int
pdfmark_write_bead(gx_device_pdf * pdev, const pdf_bead * pbead)
{
    stream *s;

    pdf_open_separate(pdev, pbead->id);
    s = pdev->strm;
    pprintld3(s, "<</T %ld 0 R/V %ld 0 R/N %ld 0 R",
	      pbead->article_id, pbead->prev_id, pbead->next_id);
    if (pbead->page_id != 0)
	pprintld1(s, "/P %ld 0 R", pbead->page_id);
    pdfmark_write_rect(s, "R", &pbead->rect);
    pputs(s, ">>\n");
    return pdf_end_separate(pdev);
}

/* Finish writing an article. */
int
pdfmark_write_article(gx_device_pdf * pdev, const pdf_article * part)
{
    pdf_article art;
    stream *s;

    art = *part;
    if (art.last.id == 0) {
	/* Only one bead in the article. */
	art.first.prev_id = art.first.next_id = art.first.id;
    } else {
	/* More than one bead in the article. */
	art.first.prev_id = art.last.id;
	art.last.next_id = art.first.id;
	pdfmark_write_bead(pdev, &art.last);
    }
    pdfmark_write_bead(pdev, &art.first);
    pdf_open_separate(pdev, art.id);
    s = pdev->strm;
    pprintld1(s, "<</F %ld 0 R/I<</Title ", art.first.id);
    pdf_put_value(pdev, art.title.data, art.title.size);
    if (art.info.data != 0) {
	pputs(s, "\n");
	pdf_put_value(pdev, art.info.data, art.info.size);
    }
    pputs(s, ">> >>\n");
    return pdf_end_separate(pdev);
}

/* ARTICLE pdfmark */
private int
pdfmark_ARTICLE(gx_device_pdf * pdev, gs_param_string * pairs, uint count,
		const gs_matrix * pctm, const gs_param_string * no_objname)
{
    gs_param_string title;
    gs_param_string rectstr;
    gs_rect rect;
    long bead_id;
    pdf_article *part;
    int code;

    if (!pdfmark_find_key("Title", pairs, count, &title) ||
	!pdfmark_find_key("Rect", pairs, count, &rectstr)
	)
	return_error(gs_error_rangecheck);
    if ((code = pdfmark_scan_rect(&rect, &rectstr, pctm)) < 0)
	return code;

    /* Find the article with this title, or create one. */
    bead_id = pdf_obj_ref(pdev);
    for (part = pdev->articles; part != 0; part = part->next)
	if (!bytes_compare(part->title.data, part->title.size,
			   title.data, title.size))
	    break;
    if (part == 0) {		/* Create the article. */
	byte *str;

	part = gs_alloc_struct(pdev->pdf_memory, pdf_article,
			       &st_pdf_article, "pdfmark_ARTICLE");
	str = gs_alloc_string(pdev->pdf_memory, title.size,
			      "article title");
	if (part == 0 || str == 0)
	    return_error(gs_error_VMerror);
	part->next = pdev->articles;
	pdev->articles = part;
	memcpy(str, title.data, title.size);
	part->title.data = str;
	part->title.size = title.size;
	part->id = pdf_obj_ref(pdev);
	part->first.id = part->last.id = 0;
	part->info.data = 0;
	part->info.size = 0;
    }
    /*
     * Add the bead to the article.  This is similar to what we do for
     * outline nodes, except that articles have only a page number and
     * not View/Dest.
     */
    if (part->last.id == 0) {
	part->first.next_id = bead_id;
	part->last.id = part->first.id;
    } else {
	part->last.next_id = bead_id;
	pdfmark_write_bead(pdev, &part->last);
    }
    part->last.prev_id = part->last.id;
    part->last.id = bead_id;
    part->last.article_id = part->id;
    part->last.next_id = 0;
    part->last.rect = rect;
    {
	gs_param_string page_string;
	int page = 0;
	static const char *const skip_article[] =
	{
	    "Title", "Rect", "Page", 0
	};

	if (pdfmark_find_key("Page", pairs, count, &page_string))
	    page = pdfmark_page_number(pdev, &page_string);
	part->last.page_id = pdf_page_id(pdev, page);
	pdfmark_save_edited_pairs(pdev, pairs, count, skip_article,
				  NULL, 0, &part->info);
    }
    if (part->first.id == 0) {	/* This is the first bead of the article. */
	part->first = part->last;
	part->last.id = 0;
    }
    return 0;
}

/* DEST pdfmark */
private int
pdfmark_DEST(gx_device_pdf * pdev, gs_param_string * pairs, uint count,
	     const gs_matrix * pctm, const gs_param_string * objname)
{
    char dest[max_dest_string];
    gs_param_string key;
    pdf_named_dest *pnd;
    byte *str;

    if (!pdfmark_find_key("Dest", pairs, count, &key) ||
	!pdfmark_make_dest(dest, pdev, pairs, count) < 0
	)
	return_error(gs_error_rangecheck);
    pnd = gs_alloc_struct(pdev->pdf_memory, pdf_named_dest,
			  &st_pdf_named_dest, "pdfmark_DEST");
    str = gs_alloc_string(pdev->pdf_memory, key.size,
			  "named_dest key");
    if (pnd == 0 || str == 0)
	return_error(gs_error_VMerror);
    pnd->next = pdev->named_dests;
    memcpy(str, key.data, key.size);
    pnd->key.data = str;
    pnd->key.size = key.size;
    strcpy(pnd->dest, dest);
    pdev->named_dests = pnd;
    return 0;
}

/* Write the contents of pass-through code. */
/* We are inside the stream dictionary, in a 'separate' object. */
private int
pdfmark_write_ps(gx_device_pdf * pdev, const gs_param_string * psource)
{
    stream *s = pdev->strm;
    long length_id = pdf_obj_ref(pdev);
    long start_pos, length;

    pprintld1(s, " /Length %ld 0 R >> stream\n", length_id);
    start_pos = pdf_stell(pdev);
    if (psource->size < 2 || psource->data[0] != '(' ||
	psource->data[psource->size - 1] != ')'
	)
	lprintf1("bad PS passthrough: %s\n", psource->data);
    else {
/****** SHOULD REMOVE ESCAPES ******/
	pwrite(s, psource->data + 1, psource->size - 2);
    }
    pputs(s, "\n");
    length = pdf_stell(pdev) - start_pos;
    pputs(s, "endstream\n");
    pdf_end_separate(pdev);
    pdf_open_separate(pdev, length_id);
    pprintld1(s, "%ld\n", length);
    pdf_end_separate(pdev);
    return 0;
}

/* PS pdfmark */
private int
pdfmark_PS(gx_device_pdf * pdev, gs_param_string * pairs, uint count,
	   const gs_matrix * pctm, const gs_param_string * objname)
{
    stream *s;
    gs_param_string source;
    gs_param_string level1;

    if (!pdfmark_find_key("DataSource", pairs, count, &source))
	return_error(gs_error_rangecheck);
    pdfmark_find_key("Level1", pairs, count, &level1);
    if (level1.data == 0 && source.size <= 100 &&
	pdev->CompatibilityLevel >= 1.2 && objname == 0
	) {			/* Insert the PostScript code in-line */
	int code = pdf_open_contents(pdev, pdf_in_stream);

	if (code < 0)
	    return code;
	s = pdev->strm;
	pwrite(s, source.data, source.size);
	pputs(s, " PS\n");
    } else {			/* Put the PostScript code in a resource. */
	pdf_resource *pres;

/****** SHOULD BE A NamedObject ******/
	int code = pdf_begin_resource(pdev, resourceImageXObject, gs_no_id,
				      &pres);

	if (code < 0)
	    return code;
	s = pdev->strm;
	pputs(s, "/Subtype/PS");
	if (level1.data != 0) {
	    long level1_id = pdf_obj_ref(pdev);

	    pprintld1(s, "/Level1 %ld 0 R", level1_id);
	    pdfmark_write_ps(pdev, &source);
	    pdf_open_separate(pdev, level1_id);
	    pputs(pdev->strm, "<<");
	    pdfmark_write_ps(pdev, &level1);
	} else
	    pdfmark_write_ps(pdev, &source);
	code = pdf_open_contents(pdev, pdf_in_stream);
	if (code < 0)
	    return code;
	pprintld1(pdev->strm, "/R%ld Do\n", pres->id);
    }
    return 0;
}

/* PAGES pdfmark */
private int
pdfmark_PAGES(gx_device_pdf * pdev, gs_param_string * pairs, uint count,
	      const gs_matrix * pctm, const gs_param_string * no_objname)
{
    return pdfmark_save_pairs(pdev, pairs, count, &pdev->pages_string);
}

/* PAGE pdfmark */
private int
pdfmark_PAGE(gx_device_pdf * pdev, gs_param_string * pairs, uint count,
	     const gs_matrix * pctm, const gs_param_string * no_objname)
{
    return pdfmark_save_pairs(pdev, pairs, count, &pdev->page_string);
}

/* DOCINFO pdfmark */
private int
pdfmark_DOCINFO(gx_device_pdf * pdev, gs_param_string * pairs, uint count,
		const gs_matrix * pctm, const gs_param_string * no_objname)
{
    long info_id = pdf_begin_separate(pdev);
    stream *s = pdev->strm;
    uint i;

    if (info_id < 0)
	return info_id;
    pputs(s, "<<\n");
    for (i = 0; i < count; i += 2)
	if (!pdf_key_eq(&pairs[i], "CreationDate") &&
	    !pdf_key_eq(&pairs[i], "Producer")
	    )
	    pdfmark_write_pair(pdev, &pairs[i]);
    pdf_write_default_info(pdev);
    pputs(s, ">>\n");
    pdf_end_separate(pdev);
    pdev->info_id = info_id;
    return 0;
}

/* DOCVIEW pdfmark */
private int
pdfmark_DOCVIEW(gx_device_pdf * pdev, gs_param_string * pairs, uint count,
		const gs_matrix * pctm, const gs_param_string * no_objname)
{
    char dest[max_dest_string];

    if (pdfmark_make_dest(dest, pdev, pairs, count)) {
	gs_param_string add_dest[2];
	static const char *const skip_dest[] =
	{"Page", "View", 0};

	param_string_from_string(add_dest[0], "OpenAction");
	param_string_from_string(add_dest[1], dest);
	return pdfmark_save_edited_pairs(pdev, pairs, count, skip_dest,
					 add_dest, 2,
					 &pdev->catalog_string);
    } else
	return pdfmark_save_pairs(pdev, pairs, count, &pdev->catalog_string);
}
