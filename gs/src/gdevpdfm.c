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

/* gdevpdfm.c */
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
#define pdfmark_proc(proc)\
  int proc(P4(gx_device_pdf *pdev, const gs_param_string *pairs, uint count,\
	      const gs_matrix *pctm))
typedef struct pdfmark_name_s {
    const char *mname;
         pdfmark_proc((*proc));
} pdfmark_name;
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
    {"ANN", pdfmark_ANN},
    {"LNK", pdfmark_LNK},
    {"OUT", pdfmark_OUT},
    {"ARTICLE", pdfmark_ARTICLE},
    {"DEST", pdfmark_DEST},
    {"PS", pdfmark_PS},
    {"PAGES", pdfmark_PAGES},
    {"PAGE", pdfmark_PAGE},
    {"DOCINFO", pdfmark_DOCINFO},
    {"DOCVIEW", pdfmark_DOCVIEW},
    {0, 0}
};

/* ---------------- Public entry points ---------------- */

/* Compare a C string and a gs_param_string. */
bool
pdf_key_eq(const gs_param_string * pcs, const char *str)
{
    return (strlen(str) == pcs->size &&
	    !strncmp(str, (const char *)pcs->data, pcs->size));
}

/* Process a pdfmark. */
int
pdfmark_process(gx_device_pdf * pdev, const gs_param_string_array * pma)
{
    const gs_param_string *data = pma->data;
    uint size = pma->size;
    const gs_param_string *pts = &data[size - 1];
    gs_matrix ctm;
    int i;

    if ((size & 1) || size < 2 ||
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
    for (i = 0; mark_names[i].mname != 0; ++i)
	if (pdf_key_eq(pts, mark_names[i].mname))
	    return (*mark_names[i].proc) (pdev, data, size - 2, &ctm);
    return 0;
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

/* Scan an integer out of a parameter string. */
private int
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
pdfmark_write_pair(stream * s, const gs_param_string * pair)
{
    pputc(s, '/');
    pwrite(s, pair->data, pair->size);
    pputc(s, ' ');
    pwrite(s, pair[1].data, pair[1].size);
    pputc(s, '\n');
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
    pprints1(s, "/%s ", key);
    pprintg4(s, "[%g %g %g %g]\n",
	     prect->p.x, prect->p.y, prect->q.x, prect->q.y);
}

/* Copy an annotation dictionary. */
private int
pdfmark_annot(gx_device_pdf * pdev, const gs_param_string * pairs, uint count,
	      const gs_matrix * pctm, const char *subtype)
{
    pdf_resource *pres;
    int code = pdf_begin_aside(pdev, &pdev->annots, NULL, &pres);
    stream *s = pdev->strm;
    bool subtype_present = false;
    bool add_dest = false;
    bool dest_present = false;
    uint i;

    if (code < 0)
	return code;
    pres->rid = pdev->next_page;
    pputs(s, "<< /Type /Annot\n");
    for (i = 0; i < count; i += 2) {
	const gs_param_string *pair = &pairs[i];
	long src_pg;

	if (pdf_key_eq(pair, "SrcPg") &&
	    sscanf((const char *)pair[1].data, "%ld", &src_pg) == 1
	    )
	    pres->rid = src_pg - 1;
	else if (pdf_key_eq(pair, "Page") || pdf_key_eq(pair, "View"))
	    add_dest = true;
	/*
	 * For some unfathomable reason, PDF requires /A instead of
	 * /Action, and /S instead of /Subtype in the Action dictionary.
	 * Handle this here.
	 */
	else if (pdf_key_eq(pair, "Action")) {
	    const byte *astr = pair[1].data;
	    const uint asize = pair[1].size;
	    uint i;

	    pputs(s, "/A ");
	    /* Search for the next occurrence of /Subtype. */
	    for (i = 0; i < asize; ++i)
		if (asize - i > 8 && !memcmp(astr + i, "/Subtype", 8) &&
		    scan_char_decoder[astr[i + 8]] > ctype_name
		    ) {
		    pputs(s, "/S");
		    i += 7;
		} else
		    pputc(s, astr[i]);
	    pputc(s, '\n');
	} else if (pdf_key_eq(pair, "Rect")) {
	    gs_rect rect;

	    code = pdfmark_scan_rect(&rect, pair + 1, pctm);
	    if (code < 0)
		return code;
	    pdfmark_write_rect(s, "Rect", &rect);
	} else {
	    pdfmark_write_pair(s, pair);
	    if (pdf_key_eq(pair, "Dest"))
		dest_present = true;
	    else if (pdf_key_eq(pair, "Subtype"))
		subtype_present = true;
	}
    }
    if (add_dest && !dest_present) {
	char dest[max_dest_string];
	int code = pdfmark_make_dest(dest, pdev, pairs, count);

	if (code >= 0)
	    pprints1(s, "/Dest %s\n", dest);
    }
    if (!subtype_present)
	pprints1(s, "/Subtype /%s ", subtype);
    pputs(s, ">>\n");
    pdf_end_aside(pdev);
    return 0;
}

/* ANN pdfmark */
private int
pdfmark_ANN(gx_device_pdf * pdev, const gs_param_string * pairs, uint count,
	    const gs_matrix * pctm)
{
    return pdfmark_annot(pdev, pairs, count, pctm, "Text");
}

/* LNK pdfmark (obsolescent) */
private int
pdfmark_LNK(gx_device_pdf * pdev, const gs_param_string * pairs, uint count,
	    const gs_matrix * pctm)
{
    return pdfmark_annot(pdev, pairs, count, pctm, "Link");
}

/* Save pairs in a string. */
private bool
pdf_key_member(const gs_param_string * pcs, const char *const keys[])
{
    const char *const *pkey;

    for (pkey = keys; *pkey != 0; ++pkey)
	if (pdf_key_eq(pcs, *pkey))
	    return true;
    return false;
}
#define pdfmark_size_pair(pair)\
  ((pair)[0].size + (pair)[1].size + 3)
private byte *
pdfmark_save_pair(byte * next, const gs_param_string * pair)
{
    uint len;

    *next++ = '/';
    memcpy(next, pair[0].data, len = pair[0].size);
    next += len;
    *next++ = ' ';
    memcpy(next, pair[1].data, len = pair[1].size);
    next += len;
    *next++ = '\n';
    return next;
}
private int
pdfmark_save_edited_pairs(const gx_device_pdf * pdev,
   const gs_param_string * pairs, uint count, const char *const skip_keys[],
	const gs_param_string * add_pairs, uint add_count, gs_string * pstr)
{
    uint i, size;
    byte *data;
    byte *next;

    for (i = 0, size = 0; i < count; i += 2)
	if (!pdf_key_member(&pairs[i], skip_keys))
	    size += pdfmark_size_pair(&pairs[i]);
    for (i = 0; i < add_count; i += 2)
	size += pdfmark_size_pair(&add_pairs[i]);
    if (pstr->data == 0)
	data = gs_alloc_string(pdev->pdf_memory, size, "pdfmark_save_pairs");
    else
	data = gs_resize_string(pdev->pdf_memory, pstr->data, pstr->size,
				size, "pdfmark_save_pairs");
    if (data == 0)
	return_error(gs_error_VMerror);
    next = data;
    for (i = 0; i < count; i += 2)
	if (!pdf_key_member(&pairs[i], skip_keys))
	    next = pdfmark_save_pair(next, &pairs[i]);
    for (i = 0; i < add_count; i += 2)
	next = pdfmark_save_pair(next, &add_pairs[i]);
    pstr->data = data;
    pstr->size = size;
    return 0;
}
static const char *const no_skip_pairs[] =
{0};

#define pdfmark_save_pairs(pdev, pairs, count, pstr)\
  pdfmark_save_edited_pairs(pdev, pairs, count, no_skip_pairs, NULL, 0, pstr)

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
private int
pdfmark_OUT(gx_device_pdf * pdev, const gs_param_string * pairs, uint count,
	    const gs_matrix * pctm)
{
    int depth = pdev->outline_depth;
    pdf_outline_level *plevel = &pdev->outline_levels[depth];
    int sub_count = 0;
    uint i;
    pdf_outline_node node;
    int code;

    for (i = 0; i < count; i += 2) {
	const gs_param_string *pair = &pairs[i];

	if (pdf_key_eq(pair, "Count"))
	    pdfmark_scan_int(pair + 1, &sub_count);
    }
    if (sub_count != 0 && depth == max_outline_depth - 1)
	return_error(gs_error_limitcheck);
    node.action_string.data = 0;
    {
	static const char *const skip_out[] =
	{"Page", "View", "Count", 0};
	char dest[max_dest_string];

	if (pdfmark_make_dest(dest, pdev, pairs, count)) {
	    gs_param_string add_dest[2];

	    param_string_from_string(add_dest[0], "Dest");
	    param_string_from_string(add_dest[1], dest);
	    code = pdfmark_save_edited_pairs(pdev, pairs, count, skip_out,
					     add_dest, 2,
					     &node.action_string);
	} else {
	    code = pdfmark_save_edited_pairs(pdev, pairs, count, skip_out + 2,
					     NULL, 0, &node.action_string);
	}
	if (code < 0)
	    return code;
    }
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
int
pdfmark_write_article(gx_device_pdf * pdev, const pdf_bead * pbead)
{
    stream *s;

    pdf_open_separate(pdev, pbead->id);
    s = pdev->strm;
    pprintld3(s,
	      "<<\n/T %ld 0 R\n/V %ld 0 R\n/N %ld 0 R\n",
	      pbead->article_id, pbead->prev_id, pbead->next_id);
    pprints1(s, "/Dest %s\n", pbead->dest);
    pdfmark_write_rect(s, "R", &pbead->rect);
    pputs(s, ">>\n");
    return pdf_end_separate(pdev);
}

/* ARTICLE pdfmark */
private int
pdfmark_ARTICLE(gx_device_pdf * pdev, const gs_param_string * pairs, uint count,
		const gs_matrix * pctm)
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
/****** Should save other keys for Info dictionary ******/

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
	part->id = pdf_begin_separate(pdev);
	part->first.id = part->last.id = 0;
	pprintld1(pdev->strm, "<</F %ld 0 R>>\n", bead_id);
	pdf_end_separate(pdev);
    }
    /* Add the bead to the article. */
    /* This is similar to what we do for outline nodes. */
    if (part->last.id == 0) {
	part->first.next_id = bead_id;
	part->last.id = part->first.id;
    } else {
	part->last.next_id = bead_id;
	pdfmark_write_article(pdev, &part->last);
    }
    part->last.prev_id = part->last.id;
    part->last.id = bead_id;
    part->last.article_id = part->id;
    part->last.next_id = 0;
    part->last.rect = rect;
    pdfmark_make_dest(part->last.dest, pdev, pairs, count);
    if (part->first.id == 0) {	/* This is the first bead of the article. */
	part->first = part->last;
	part->last.id = 0;
    }
    return 0;
}

/* DEST pdfmark */
private int
pdfmark_DEST(gx_device_pdf * pdev, const gs_param_string * pairs, uint count,
	     const gs_matrix * pctm)
{
    char dest[max_dest_string];
    gs_param_string key;
    pdf_named_dest *pnd;
    byte *str;

    if (!pdfmark_find_key("Dest", pairs, count, &key) ||
	!pdfmark_make_dest(dest, pdev, pairs, count)
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
pdfmark_PS(gx_device_pdf * pdev, const gs_param_string * pairs, uint count,
	   const gs_matrix * pctm)
{
    stream *s;
    gs_param_string source;
    gs_param_string level1;

    if (!pdfmark_find_key("DataSource", pairs, count, &source))
	return_error(gs_error_rangecheck);
    pdfmark_find_key("Level1", pairs, count, &level1);
    if (level1.data == 0 && source.size <= 100 &&
	pdev->CompatibilityLevel >= 1.2
	) {			/* Insert the PostScript code in-line */
	int code = pdf_open_contents(pdev, pdf_in_stream);

	if (code < 0)
	    return code;
	s = pdev->strm;
	pwrite(s, source.data, source.size);
	pputs(s, " PS\n");
    } else {			/* Put the PostScript code in a resource. */
	pdf_resource *pres;
	int code = pdf_begin_resource(pdev, resourceXObject, gs_no_id,
				      &pres);

	if (code < 0)
	    return code;
	s = pdev->strm;
	pputs(s, " /Subtype /PS");
	if (level1.data != 0) {
	    long level1_id = pdf_obj_ref(pdev);

	    pprintld1(s, " /Level1 %ld 0 R", level1_id);
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
pdfmark_PAGES(gx_device_pdf * pdev, const gs_param_string * pairs, uint count,
	      const gs_matrix * pctm)
{
    return pdfmark_save_pairs(pdev, pairs, count, &pdev->pages_string);
}

/* PAGE pdfmark */
private int
pdfmark_PAGE(gx_device_pdf * pdev, const gs_param_string * pairs, uint count,
	     const gs_matrix * pctm)
{
    return pdfmark_save_pairs(pdev, pairs, count, &pdev->page_string);
}

/* DOCINFO pdfmark */
private int
pdfmark_DOCINFO(gx_device_pdf * pdev, const gs_param_string * pairs, uint count,
		const gs_matrix * pctm)
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
	    pdfmark_write_pair(s, &pairs[i]);
    pdf_write_default_info(pdev);
    pputs(s, ">>\n");
    pdf_end_separate(pdev);
    pdev->info_id = info_id;
    return 0;
}

/* DOCVIEW pdfmark */
private int
pdfmark_DOCVIEW(gx_device_pdf * pdev, const gs_param_string * pairs, uint count,
		const gs_matrix * pctm)
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
