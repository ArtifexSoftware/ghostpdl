/* Copyright (C) 1997, 1998 Aladdin Enterprises.  All rights reserved.

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


/* Named object pdfmark processing */
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsutil.h"		/* for bytes_compare */
#include "gdevpdfx.h"
#include "strimpl.h"
#include "sstring.h"

/* GC procedures */

private_st_pdf_named_element();
public_st_pdf_named_object();

#define pne ((pdf_named_element *)vptr)
private 
ENUM_PTRS_BEGIN(pdf_named_elt_enum_ptrs) return 0;

ENUM_PTR(0, pdf_named_element, next);
ENUM_STRING_PTR(1, pdf_named_element, key);
/****** WRONG IF data = 0 ******/
ENUM_STRING_PTR(2, pdf_named_element, value);
ENUM_PTRS_END
private RELOC_PTRS_BEGIN(pdf_named_elt_reloc_ptrs)
{
    RELOC_PTR(pdf_named_element, next);
    if (pne->key.data != 0)
	RELOC_STRING_PTR(pdf_named_element, key);
    RELOC_STRING_PTR(pdf_named_element, value);
}
RELOC_PTRS_END
#undef pne

#define pno ((pdf_named_object *)vptr)
private ENUM_PTRS_BEGIN(pdf_named_obj_enum_ptrs) return 0;
ENUM_PTR(0, pdf_named_object, next);
ENUM_STRING_PTR(1, pdf_named_object, key);
ENUM_PTR(2, pdf_named_object, elements);
case 3:
if (pno->type == named_graphics)
    ENUM_RETURN(pno->graphics.enclosing);
else
    return 0;
ENUM_PTRS_END
private RELOC_PTRS_BEGIN(pdf_named_obj_reloc_ptrs)
{
    RELOC_PTR(pdf_named_object, next);
    RELOC_STRING_PTR(pdf_named_object, key);
    RELOC_PTR(pdf_named_object, elements);
    if (pno->type == named_graphics)
	RELOC_PTR(pdf_named_object, graphics.enclosing);
}
RELOC_PTRS_END
#undef pno

/* ---------------- pdfmark processing ---------------- */

/* Define the pdfmark types implemented here. */
private pdfmark_proc(pdfmark_BP);
private pdfmark_proc(pdfmark_EP);
private pdfmark_proc(pdfmark_SP);
private pdfmark_proc(pdfmark_OBJ);
private pdfmark_proc(pdfmark_PUT);
private pdfmark_proc(pdfmark_PUTDICT);
private pdfmark_proc(pdfmark_PUTINTERVAL);
private pdfmark_proc(pdfmark_CLOSE);
const pdfmark_name pdfmark_names_named[] =
{
    {"BP", pdfmark_BP, pdfmark_nameable},
    {"EP", pdfmark_EP, 0},
    {"SP", pdfmark_SP, pdfmark_odd_ok | pdfmark_keep_name},
    {"OBJ", pdfmark_OBJ, pdfmark_nameable},
    {"PUT", pdfmark_PUT, pdfmark_odd_ok | pdfmark_keep_name},
    {".PUTDICT", pdfmark_PUTDICT, pdfmark_odd_ok | pdfmark_keep_name},
  {".PUTINTERVAL", pdfmark_PUTINTERVAL, pdfmark_odd_ok | pdfmark_keep_name},
    {"CLOSE", pdfmark_CLOSE, pdfmark_odd_ok | pdfmark_keep_name},
    {0, 0}
};

/*
 * Predefined objects:
 *      {Catalog}, {DocInfo}
 *      {Page<#>}, {ThisPage}, {PrevPage}, {NextPage}
 */

/* ---------------- Utilities ---------------- */

private int pdfmark_write_named(P2(gx_device_pdf * pdev,
				   const pdf_named_object * pno));
private void pdfmark_free_named(P2(gx_device_pdf * pdev,
				   pdf_named_object * pno));

/* ------ Public ------ */

/*
 * Look up an object name.  Return e_rangecheck if the syntax is invalid.
 * If the object is missing, then:
 *      - If create is false, return gs_error_undefined;
 *      - If create is true, create the object and return 1.
 * Note that there is code in gdevpdf.c that relies on the fact that
 * the named_objects list is sorted by decreasing object number.
 */
private int
pdfmark_find_named(gx_device_pdf * pdev, const gs_param_string * pname,
		   pdf_named_object ** ppno, bool create)
{
    const byte *data = pname->data;
    uint size = pname->size;
    gs_id key_id =
    (size == 0 ? 0 : data[0] + data[size / 2] + data[size - 1]);
    pdf_resource **plist =
    &pdev->resources[resourceNamedObject].
    chains[gs_id_hash(key_id) % num_resource_chains];
    pdf_named_object *pno = (pdf_named_object *) * plist;

    if (!pdfmark_objname_is_valid(data, size))
	return_error(gs_error_rangecheck);
    for (; pno; pno = pno->next)
	if (!bytes_compare(data, size, pno->key.data, pno->key.size)) {
	    *ppno = pno;
	    return 0;
	}
    if (!create)
	return_error(gs_error_undefined);
    {
	gs_memory_t *mem = pdev->pdf_memory;
	byte *key = gs_alloc_string(mem, size, "pdf named object key");
	pdf_resource *pres;
	int code;

	if (key == 0)
	    return_error(gs_error_VMerror);
	code = pdf_alloc_resource(pdev, resourceNamedObject, key_id, &pres);
	if (code < 0) {
	    gs_free_string(mem, key, size, "pdf named object key");
	    return code;
	}
	pno = (pdf_named_object *) pres;
	memcpy(key, data, size);
	pno->id = pdf_obj_ref(pdev);
	pno->type = named_unknown;	/* caller may change */
	pno->key.data = key;
	pno->key.size = size;
	pno->elements = 0;
	pno->open = true;
	/* For now, just link the object onto the private list. */
	pno->next = pdev->named_objects;
	pdev->named_objects = pno;
	*ppno = pno;
	return 1;
    }
}

/* Replace object names with object references in a (parameter) string. */
private const byte *
pdfmark_next_object(const byte * scan, const byte * end, const byte ** pname,
		    pdf_named_object ** ppno, gx_device_pdf * pdev)
{				/*
				 * Starting at scan, find the next object reference, set *pname
				 * to point to it in the string, store the object at *ppno,
				 * and return a pointer to the first character beyond the
				 * reference.  If there are no more object references, set
				 * *pname = end and return end.
				 */
    const byte *left;
    const byte *lit;
    const byte *right;

    *ppno = 0;
  top:left = memchr(scan, '{', end - scan);
    if (left == 0)
	return (*pname = end);
    lit = memchr(scan, '(', left - scan);
    if (lit) {
	/* Skip over the string. */
	byte buf[50];		/* size is arbitrary */
	stream_cursor_read r;
	stream_cursor_write w;
	stream_PSSD_state ss;
	int status;

	s_PSSD_init_inline(&ss);
	r.ptr = lit - 1;
	r.limit = end - 1;
	w.limit = buf + sizeof(buf) - 1;
	do {
	    w.ptr = buf - 1;
	    status = (*s_PSSD_template.process)
		((stream_state *) & ss, &r, &w, true);
	}
	while (status == 1);
	scan = r.ptr + 1;
	goto top;
    }
    right = memchr(left + 1, '}', end - (left + 1));
    if (right == 0)		/* malformed name */
	return (*pname = end);
    *pname = left;
    ++right;
    {
	gs_param_string sname;

	sname.data = left;
	sname.size = right - left;
	pdfmark_find_named(pdev, &sname, ppno, false);
    }
    return right;
}
int
pdfmark_replace_names(gx_device_pdf * pdev, const gs_param_string * from,
		      gs_param_string * to)
{
    const byte *start = from->data;
    const byte *end = start + from->size;
    const byte *scan;
    uint size = 0;
    pdf_named_object *pno;
    bool any = false;
    byte *sto;
    char ref[1 + 10 + 5 + 1];	/* max obj number is 10 digits */

    /* Do a first pass to compute the length of the result. */
    for (scan = start; scan < end;) {
	const byte *sname;
	const byte *next =
	pdfmark_next_object(scan, end, &sname, &pno, pdev);

	size += sname - scan;
	if (pno) {
	    sprintf(ref, " %ld 0 R ", pno->id);
	    size += strlen(ref);
	}
	scan = next;
	any |= next != sname;
    }
    to->persistent = true;	/* ??? */
    if (!any) {
	to->data = start;
	to->size = size;
	return 0;
    }
    sto = gs_alloc_bytes(pdev->pdf_memory, size,
			 "pdfmark_replace_names");
    if (sto == 0)
	return_error(gs_error_VMerror);
    to->data = sto;
    to->size = size;
    /* Do a second pass to do the actual substitutions. */
    for (scan = start; scan < end;) {
	const byte *sname;
	const byte *next =
	pdfmark_next_object(scan, end, &sname, &pno, pdev);
	uint copy = sname - scan;
	int rlen;

	memcpy(sto, scan, copy);
	sto += copy;
	if (pno) {
	    sprintf(ref, " %ld 0 R ", pno->id);
	    rlen = strlen(ref);
	    memcpy(sto, ref, rlen);
	    sto += rlen;
	}
	scan = next;
    }
    return 0;
}

/* Write and free an entire list of named objects. */
int
pdfmark_write_and_free_named(gx_device_pdf * pdev, pdf_named_object ** ppno)
{
    pdf_named_object *pno = *ppno;
    pdf_named_object *next;

    for (; pno; pno = next) {
	next = pno->next;
	pdfmark_write_named(pdev, pno);
	pdfmark_free_named(pdev, pno);
    }
    *ppno = 0;
    return 0;
}

/* ------ Private ------ */

/* Put an element of an array object. */
private int
pdf_named_array_put(gx_device_pdf * pdev, pdf_named_object * pno, int index,
		    const gs_param_string * pvalue)
{
    gs_memory_t *mem = pdev->pdf_memory;
    pdf_named_element **ppne = &pno->elements;
    pdf_named_element *pne;
    pdf_named_element *pnext;
    gs_string value;

    while ((pnext = *ppne) != 0 && pnext->key.size > index)
	ppne = &pnext->next;
    value.data = gs_alloc_string(mem, pvalue->size, "named array value");
    if (value.data == 0)
	return_error(gs_error_VMerror);
    value.size = pvalue->size;
    if (pnext && pnext->key.size == index) {
	/* We're replacing an existing element. */
	gs_free_string(mem, pnext->value.data, pnext->value.size,
		       "named array old value");
	pne = pnext;
    } else {
	/* Create a new element. */
	pne = gs_alloc_struct(mem, pdf_named_element, &st_pdf_named_element,
			      "named array element");
	if (pne == 0) {
	    gs_free_string(mem, value.data, value.size, "named array value");
	    return_error(gs_error_VMerror);
	}
	pne->key.data = 0;
	pne->key.size = index;
	pne->next = pnext;
	*ppne = pne;
    }
    memcpy(value.data, pvalue->data, value.size);
    pne->value = value;
    return 0;
}

/* Put an element of a dictionary object. */
private int
pdf_named_dict_put(gx_device_pdf * pdev, pdf_named_object * pno,
	       const gs_param_string * pkey, const gs_param_string * pvalue)
{
    gs_memory_t *mem = pdev->pdf_memory;
    pdf_named_element **ppne = &pno->elements;
    pdf_named_element *pne;
    pdf_named_element *pnext;
    gs_string value;

    while ((pnext = *ppne) != 0 &&
	   bytes_compare(pnext->key.data, pnext->key.size,
			 pkey->data, pkey->size)
	)
	ppne = &pnext->next;
    value.data = gs_alloc_string(mem, pvalue->size, "named dict value");
    if (value.data == 0)
	return_error(gs_error_VMerror);
    value.size = pvalue->size;
    if (pnext) {
	/* We're replacing an existing element. */
	gs_free_string(mem, pnext->value.data, pnext->value.size,
		       "named array old value");
	pne = pnext;
    } else {
	/* Create a new element. */
	gs_string key;

	key.data = gs_alloc_string(mem, pkey->size, "named dict key");
	key.size = pkey->size;
	pne = gs_alloc_struct(mem, pdf_named_element, &st_pdf_named_element,
			      "named dict element");
	if (key.data == 0 || pne == 0) {
	    gs_free_object(mem, pne, "named dict element");
	    if (key.data)
		gs_free_string(mem, key.data, key.size, "named dict key");
	    gs_free_string(mem, value.data, value.size, "named dict value");
	    return_error(gs_error_VMerror);
	}
	pne->key = key;
	memcpy(key.data, pkey->data, key.size);
	pne->next = pnext;
	*ppne = pne;
    }
    memcpy(value.data, pvalue->data, value.size);
    pne->value = value;
    return 0;
}

/* Write out the definition of a named object. */
private pdf_named_element *
pdf_reverse_elements(pdf_named_element * pne)
{
    pdf_named_element *prev = NULL;
    pdf_named_element *next;

    for (; pne; pne = next)
	next = pne->next, pne->next = prev, prev = pne;
    return prev;
}
private int
pdfmark_write_named(gx_device_pdf * pdev, const pdf_named_object * pno)
{
    stream *s = pdev->strm;
    pdf_named_element *pne = pno->elements;

    switch (pno->type) {
	case named_array:{
		uint last_index = 0;
		pdf_named_element *last = pne = pdf_reverse_elements(pne);

		pdf_open_obj(pdev, pno->id);
		pputs(s, "[");
		for (; pne; ++last_index, pne = pne->next) {
		    for (; pne->key.size > last_index; ++last_index)
			pputs(s, "null\n");
		    pdf_put_value(pdev, pne->value.data, pne->value.size);
		    pputc(s, '\n');
		}
		pdf_reverse_elements(last);
		pputs(s, "]");
	    }
	    break;
	case named_dict:
	    pdf_open_obj(pdev, pno->id);
	    pputs(s, "<<");
	    for (; pne; pne = pne->next) {
		pdf_put_value(pdev, pne->key.data, pne->key.size);
		pputc(s, ' ');
		pdf_put_value(pdev, pne->value.data, pne->value.size);
		pputc(s, '\n');
	    }
	    pputs(s, ">>");
	    break;
	case named_stream:{
		pdf_named_element *last = pne = pdf_reverse_elements(pne);

/****** NYI ******/
#if 0
		pdf_open_stream(pdev, pno->id);
/****** DOESN'T EXIST ******/
		for (; pne; pne = pne->next)
		    pwrite(s, pne->value.data, pne->value.size);
		pdf_close_stream(pdev);
/****** DITTO ******/
#endif
		pdf_reverse_elements(last);
	    }
	    break;
/****** WHAT TO DO WITH GRAPHICS/UNDEFINED? ******/
	default:
	    return 0;
    }
    pdf_end_obj(pdev);
    return 0;
}

/* Free the definition of a named object. */
private void
pdfmark_free_named(gx_device_pdf * pdev, pdf_named_object * pno)
{
    pdf_named_element *pne = pno->elements;
    pdf_named_element *next;

    for (; pne; pne = next) {
	next = pne->next;
	gs_free_string(pdev->pdf_memory, pne->value.data, pne->value.size,
		       "named object element value");
	if (pne->key.data)
	    gs_free_string(pdev->pdf_memory, pne->key.data, pne->key.size,
			   "named object element key");
    }
    gs_free_string(pdev->pdf_memory, pno->key.data, pno->key.size,
		   "named object key");
    gs_free_object(pdev->pdf_memory, pno, "named object");
}

/* ---------------- Individual pdfmarks ---------------- */

/* [ /BBox [llx lly urx ury] /_objdef {obj} /BP pdfmark */
private int
pdfmark_BP(gx_device_pdf * pdev, gs_param_string * pairs, uint count,
	   const gs_matrix * pctm, const gs_param_string * objname)
{
    gs_rect bbox;
    pdf_named_object *pno;
    int code;

    if (objname == 0 || count != 2 || !pdf_key_eq(&pairs[0], "BBox"))
	return_error(gs_error_rangecheck);
    if (sscanf((const char *)pairs[1].data, "[%lg %lg %lg %lg]",
	       &bbox.p.x, &bbox.p.y, &bbox.q.x, &bbox.q.y) != 4
	)
	return_error(gs_error_rangecheck);
    code = pdfmark_find_named(pdev, objname, &pno, true);
    if (code < 0)
	return code;
    if (pno->type != named_unknown)
	return_error(gs_error_rangecheck);
    code = pdf_named_dict_put(pdev, pno, &pairs[0], &pairs[1]);
    if (code < 0)
	return code;
    pno->type = named_graphics;
    pno->graphics.enclosing = pdev->open_graphics;
    pdev->open_graphics = pno;
/****** NYI ******/
    return 0;
}

/* [ /EP pdfmark */
private int
pdfmark_EP(gx_device_pdf * pdev, gs_param_string * pairs, uint count,
	   const gs_matrix * pctm, const gs_param_string * no_objname)
{
    pdf_named_object *pno = pdev->open_graphics;

    if (count != 0 || pno == 0 || pno->type != named_graphics ||
	!pno->open
	)
	return_error(gs_error_rangecheck);
    pno->open = false;
    pdev->open_graphics = pno->graphics.enclosing;
/****** NYI ******/
    return 0;
}

/* [ {obj} /SP pdfmark */
private int
pdfmark_SP(gx_device_pdf * pdev, gs_param_string * pairs, uint count,
	   const gs_matrix * pctm, const gs_param_string * no_objname)
{
    pdf_named_object *pno;
    int code;

    if (count != 1)
	return_error(gs_error_rangecheck);
    if ((code = pdfmark_find_named(pdev, &pairs[0], &pno, false)) < 0)
	return code;
    if (pno->type != named_graphics || pno->open)
	return_error(gs_error_rangecheck);
    code = pdf_open_contents(pdev, pdf_in_stream);
    if (code < 0)
	return code;
    pdf_put_matrix(pdev, "q ", pctm, "cm\n");
    pprintld1(pdev->strm, "/R%ld Do Q\n", pno->id);
    return 0;
}

/* [ /_objdef {array} /type /array /OBJ pdfmark */
/* [ /_objdef {dict} /type /dict /OBJ pdfmark */
/* [ /_objdef {stream} /type /stream /OBJ pdfmark */
private int
pdfmark_OBJ(gx_device_pdf * pdev, gs_param_string * pairs, uint count,
	    const gs_matrix * pctm, const gs_param_string * objname)
{
    pdf_named_object_type type;
    pdf_named_object *pno;
    int code;

    if (objname == 0 || count != 2 || !pdf_key_eq(&pairs[0], "type"))
	return_error(gs_error_rangecheck);
    if (pdf_key_eq(&pairs[1], "/array"))
	type = named_array;
    else if (pdf_key_eq(&pairs[1], "/dict"))
	type = named_dict;
    else if (pdf_key_eq(&pairs[1], "/stream"))
	type = named_stream;
    else
	return_error(gs_error_rangecheck);
    if ((code = pdfmark_find_named(pdev, objname, &pno, true)) < 0)
	return code;
    if (pno->type != named_unknown)
	return_error(gs_error_rangecheck);
    pno->type = type;
    return 0;
}

/* [ {array} index value /PUT pdfmark */
/* [ {stream} string|file /PUT pdfmark */
/* Dictionaries are converted to .PUTDICT */
private int
pdfmark_PUT(gx_device_pdf * pdev, gs_param_string * pairs, uint count,
	    const gs_matrix * pctm, const gs_param_string * no_objname)
{
    pdf_named_object *pno;
    int code;

    if ((code = pdfmark_find_named(pdev, &pairs[0], &pno, false)) < 0)
	return code;
    switch (pno->type) {
	case named_array:{
		int index;

		if (count != 3)
		    return_error(gs_error_rangecheck);
		if ((code = pdfmark_scan_int(&pairs[1], &index)) < 0)
		    return code;
		if (index < 0)
		    return_error(gs_error_rangecheck);
		code = pdf_named_array_put(pdev, pno, index, pairs + 2);
		break;
	    }
	case named_stream:
	    if (count != 2)
		return_error(gs_error_rangecheck);
/****** NYI ******/
	    break;
	default:
	    return_error(gs_error_rangecheck);
    }
    return code;
}

/* [ {dict} key value ... /.PUTDICT pdfmark */
private int
pdfmark_PUTDICT(gx_device_pdf * pdev, gs_param_string * pairs, uint count,
		const gs_matrix * pctm, const gs_param_string * no_objname)
{
    pdf_named_object *pno;
    int code, i;

    if ((code = pdfmark_find_named(pdev, &pairs[0], &pno, false)) < 0)
	return code;
    if (!(count & 1) || pno->type != named_dict)
	return_error(gs_error_rangecheck);
    for (i = 1; code >= 0 && i < count; i += 2)
	code = pdf_named_dict_put(pdev, pno, pairs + i, pairs + i + 1);
    return code;
}

/* [ {array} index value ... /.PUTINTERVAL pdfmark */
private int
pdfmark_PUTINTERVAL(gx_device_pdf * pdev, gs_param_string * pairs, uint count,
		 const gs_matrix * pctm, const gs_param_string * no_objname)
{
    pdf_named_object *pno;
    int code, index, i;

    if ((code = pdfmark_find_named(pdev, &pairs[0], &pno, false)) < 0)
	return code;
    if (count < 2 || pno->type != named_array)
	return_error(gs_error_rangecheck);
    if ((code = pdfmark_scan_int(&pairs[1], &index)) < 0)
	return code;
    if (index < 0)
	return_error(gs_error_rangecheck);
    for (i = 2; code >= 0 && i < count; ++i)
	code = pdf_named_array_put(pdev, pno, index + i - 2, &pairs[i]);
    return code;
}

/* [ {stream} /CLOSE pdfmark */
private int
pdfmark_CLOSE(gx_device_pdf * pdev, gs_param_string * pairs, uint count,
	      const gs_matrix * pctm, const gs_param_string * no_objname)
{
    pdf_named_object *pno;
    int code;

    if (count != 1)
	return_error(gs_error_rangecheck);
    if ((code = pdfmark_find_named(pdev, &pairs[0], &pno, false)) < 0)
	return code;
    if (pno->type != named_stream || !pno->open)
	return_error(gs_error_rangecheck);
    /* Currently we don't do anything special when closing a stream. */
    pno->open = false;
    return 0;
}
