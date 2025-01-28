/* Copyright (C) 2001-2025 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  39 Mesa Street, Suite 108A, San Francisco,
   CA 94129, USA, for further information.
*/


/* pdfmark processing for PDF-writing driver */
#include "math_.h"
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsutil.h"		/* for bytes_compare */
#include "gdevpdfx.h"
#include "gdevpdfo.h"
#include "szlibx.h"
#include "slzwx.h"

/* GC descriptors */
private_st_pdf_article();

/*
 * The pdfmark pseudo-parameter indicates the occurrence of a pdfmark
 * operator in the input file.  Its "value" is the arguments of the operator,
 * passed through essentially unchanged:
 *      (key, value)*, CTM, type
 */

/*
 * Define an entry in a table of pdfmark-processing procedures.
 * (The actual table is at the end of this file, to avoid the need for
 * forward declarations for the procedures.)
 */
#define PDFMARK_NAMEABLE 1	/* allows _objdef */
#define PDFMARK_ODD_OK 2	/* OK if odd # of parameters */
#define PDFMARK_KEEP_NAME 4	/* don't substitute reference for name */
                                /* in 1st argument */
#define PDFMARK_NO_REFS 8	/* don't substitute references for names */
                                /* anywhere */
#define PDFMARK_TRUECTM 16	/* pass the true CTM to the procedure, */
                                /* not the one transformed to reflect the default user space */
typedef struct pdfmark_name_s {
    const char *mname;
    pdfmark_proc((*proc));
    byte options;
} pdfmark_name;

/* ---------------- Public utilities ---------------- */

/* Compare a C string and a gs_param_string. */
bool
pdf_key_eq(const gs_param_string * pcs, const char *str)
{
    return (strlen(str) == pcs->size && pcs->data &&
            !strncmp(str, (const char *)pcs->data, pcs->size));
}

/* Scan an integer out of a parameter string. */
int
pdfmark_scan_int(const gs_param_string * pstr, int *pvalue)
{
#define MAX_INT_STR 20
    uint size = pstr->size;
    char str[MAX_INT_STR + 1];

    if (size > MAX_INT_STR)
        return_error(gs_error_limitcheck);
    memcpy(str, pstr->data, size);
    str[size] = 0;
    return (sscanf(str, "%d", pvalue) == 1 ? 0 :
            gs_note_error(gs_error_rangecheck));
#undef MAX_INT_STR
}

/* ---------------- Private utilities ---------------- */

/* Find a key in a dictionary. */
static bool
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
static int
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

/*
 * This routine checks the page number is inside the valid range FirstPage->LastPage
 * and if it is, and FirstPage is not 0, it updates the page number by rebasing it to
 * the new FirstPage. If all conditions are met we also update the 'max_referred_page'
 * which we check during closedown, and emit a warning if a reference should somehow
 * point outside the valid range of pages in the document. This can happen if we reference
 * a destination page that we haven't created yet, and when we get to the end of the
 * job we discover we never did create it.
 */
static int
update_max_page_reference(gx_device_pdf * pdev, int *page)
{
    if (*page < pdev->FirstPage || (pdev->LastPage != 0 && *page > pdev->LastPage)) {
        emprintf1(pdev->memory, "Destination page %d lies outside the valid page range.\n", *page);
        return -1;
    }
    else {
        if (pdev->FirstPage != 0)
            *page = (*page - pdev->FirstPage) + 1;

        if (pdev->max_referred_page < *page)
            pdev->max_referred_page = *page;
    }
    return 0;
}

/* Construct a destination string specified by /Page and/or /View. */
/* Return 0 if none (but still fill in a default), 1 or 2 if present */
/* (1 if only one of /Page or /View, 2 if both), <0 if error. */
static int
pdfmark_make_dest(char dstr[MAX_DEST_STRING], gx_device_pdf * pdev,
                  const char *Page_key, const char *View_key,
                  const gs_param_string * pairs, uint count, uint RequirePage)
{
    gs_param_string page_string, view_string;
    int present =
        pdfmark_find_key(Page_key, pairs, count, &page_string) +
        pdfmark_find_key(View_key, pairs, count, &view_string);
    int page=0;
    gs_param_string action;
    int len, code = 0;

    if (present || RequirePage)
        page = pdfmark_page_number(pdev, &page_string);

    if (view_string.size == 0)
        param_string_from_string(view_string, "[/XYZ null null null]");
    if (page == 0)
        strcpy(dstr, "[null ");
    else if (pdfmark_find_key("/Action", pairs, count, &action) &&
             pdf_key_eq(&action, "/GoToR")
        )
        gs_snprintf(dstr, MAX_DEST_STRING, "[%d ", page - 1);
    else {
        code = update_max_page_reference(pdev, &page);
        if (code < 0)
            return code;
        gs_snprintf(dstr, MAX_DEST_STRING, "[%"PRId64" 0 R ", pdf_page_id(pdev, page));
    }
    len = strlen(dstr);
    if (len + view_string.size > MAX_DEST_STRING)
        return_error(gs_error_limitcheck);
    if (view_string.data[0] != '[' ||
        view_string.data[view_string.size - 1] != ']'
        )
        return_error(gs_error_rangecheck);
    memcpy(dstr + len, view_string.data + 1, view_string.size - 1);
    dstr[len + view_string.size - 1] = 0;
    return present;
}

/*
 * If a named destination is specified by a string, convert it to a name,
 * update *dstr, and return 1; otherwise return 0.
 */
static int
pdfmark_coerce_dest(gs_param_string *dstr, char dest[MAX_DEST_STRING])
{
    const byte *data = dstr->data;
    uint size = dstr->size;
    if (size == 0 || data[0] != '(')
        return 0;
    /****** HANDLE ESCAPES ******/
    memcpy(dest, data, size - 1);
    dest[0] = '/';
    dest[size - 1] = 0;
    dstr->data = (byte *)dest;
    dstr->size = size - 1;
    return 1;
}

/* Put pairs in a dictionary. */
static int
pdfmark_put_c_pair(cos_dict_t *pcd, const char *key,
                   const gs_param_string * pvalue)
{
    return cos_dict_put_c_key_string(pcd, key, pvalue->data, pvalue->size);
}
static int
pdfmark_put_pair(cos_dict_t *pcd, const gs_param_string * pair)
{
    return cos_dict_put_string(pcd, pair->data, pair->size,
                               pair[1].data, pair[1].size);
}

/* Scan a Rect value. */
static int
pdfmark_scan_rect(gs_rect * prect, const gs_param_string * str,
                  const gs_matrix * pctm)
{
    uint size = str->size;
    double v[4];
#define MAX_RECT_STRING 100
    char chars[MAX_RECT_STRING + 3];
    int end_check;

    if (str->size > MAX_RECT_STRING)
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

/* Make a Rect value. */
static void
pdfmark_make_rect(char str[MAX_RECT_STRING], const gs_rect * prect)
{
    /*
     * We have to use a stream and pprintf, rather than sprintf,
     * because printf formats can't express the PDF restrictions on
     * the form of the output.
     */
    stream s;

    s_init(&s, NULL);
    swrite_string(&s, (byte *)str, MAX_RECT_STRING - 1);
    pprintg4(&s, "[%g %g %g %g]",
             prect->p.x, prect->p.y, prect->q.x, prect->q.y);
    str[stell(&s)] = 0;
}

#define MAX_BORDER_STRING 100
/* Write a transformed Border value on a stream. */
static int
pdfmark_write_border(stream *s, const gs_param_string *str,
                     const gs_matrix *pctm)
{
    int i;

    for (i = 0; i < str->size; i++)
        stream_putc(s, str->data[i]);
    return 0;
}

/* Put an element in a stream's dictionary. */
static int
cos_stream_put_c_strings(cos_stream_t *pcs, const char *key, const char *value)
{
    return cos_dict_put_c_strings(cos_stream_dict(pcs), key, value);
}

static int
setup_pdfmark_stream_no_compression(gx_device_psdf *pdev0,
                        cos_stream_t *pco)
{
    /* This function is for pdfwrite only. */
    gx_device_pdf *pdev = (gx_device_pdf *)pdev0;
    gs_memory_t *mem = pdev->pdf_memory;

    pco->input_strm = cos_write_stream_alloc(pco, pdev,
                                  "setup_pdfmark_stream_compression");
    if (pco->input_strm == 0)
        return_error(gs_error_VMerror);
    if (!pdev->binary_ok) {
        stream_state *ss = s_alloc_state(mem, s_A85E_template.stype,
                          "setup_pdfmark_stream_compression");
        if (ss == 0)
            return_error(gs_error_VMerror);
        if (s_add_filter(&pco->input_strm, &s_A85E_template, ss, mem) == 0) {
            gs_free_object(mem, ss, "setup_image_compression");
            return_error(gs_error_VMerror);
        }
    }
    return 0;
}

/* Setup pdfmak stream compression. */
static int
setup_pdfmark_stream_compression(gx_device_psdf *pdev0,
                        cos_stream_t *pco)
{
    /* This function is for pdfwrite only. */
    gx_device_pdf *pdev = (gx_device_pdf *)pdev0;
    gs_memory_t *mem = pdev->pdf_memory;
    static const pdf_filter_names_t fnames = {
        PDF_FILTER_NAMES
    };
    const stream_template *templat =
        (pdev->params.UseFlateCompression &&
         pdev->version >= psdf_version_ll3 ?
         &s_zlibE_template : &s_LZWE_template);
    stream_state *st;

    pco->input_strm = cos_write_stream_alloc(pco, pdev,
                                  "setup_pdfmark_stream_compression");
    if (pco->input_strm == 0)
        return_error(gs_error_VMerror);
    if (!pdev->binary_ok) {
        stream_state *ss = s_alloc_state(mem, s_A85E_template.stype,
                          "setup_pdfmark_stream_compression");
        if (ss == 0)
            return_error(gs_error_VMerror);
        if (s_add_filter(&pco->input_strm, &s_A85E_template, ss, mem) == 0) {
            gs_free_object(mem, ss, "setup_image_compression");
            return_error(gs_error_VMerror);
        }
    }
    st = s_alloc_state(mem, templat->stype,
                            "setup_pdfmark_stream_compression");
    if (st == 0)
        return_error(gs_error_VMerror);
    if (templat->set_defaults)
        (*templat->set_defaults) (st);
    if (s_add_filter(&pco->input_strm, templat, st, mem) == 0) {
        gs_free_object(mem, st, "setup_image_compression");
        return_error(gs_error_VMerror);
    }
    return pdf_put_filters(cos_stream_dict(pco), pdev, pco->input_strm, &fnames);
}

static int
pdfmark_bind_named_object(gx_device_pdf *pdev, const gs_const_string *objname,
                          pdf_resource_t **pres)
{
    int code;

    if (objname != NULL && objname->size) {
        const cos_value_t *v = cos_dict_find(pdev->local_named_objects, objname->data, objname->size);

        if (v != NULL) {
            if (v->value_type == COS_VALUE_OBJECT) {
                if (cos_type(v->contents.object) == &cos_generic_procs) {
                    /* The object was referred but not defined.
                       Use the old object id.
                       The old object stub to be dropped. */
                    pdf_reserve_object_id(pdev, *pres, v->contents.object->id);
                } else if (!v->contents.object->written) {
                    /* We can't know whether the old object was referred or not.
                       Write it out for a consistent result in any case. */
                    code = cos_write_object(v->contents.object, pdev, resourceOther);

                    if (code < 0)
                        return code;
                    v->contents.object->written = true;
                }
            } else
                return_error(gs_error_rangecheck); /* Must not happen. */
        }
    }
    if ((*pres)->object->id == -1) {
        if(objname != NULL && objname->size)
            code = pdf_substitute_resource(pdev, pres, resourceXObject, NULL, false);
        else
            code = pdf_substitute_resource(pdev, pres, resourceXObject, NULL, true);
        (*pres)->where_used |= pdev->used_mask;
        if (code < 0)
            return code;
    } else {
        /* Unfortunately we can't apply pdf_substitute_resource,
           because the object may already be referred by its id.
           Redundant objects may happen in this case.
           For better results users should define objects before usage.
         */
    }
    if (objname != NULL && objname->size) {
        cos_value_t value;

        code = cos_dict_put(pdev->local_named_objects, objname->data,
                            objname->size, cos_object_value(&value, (cos_object_t *)(*pres)->object));
        if (code < 0)
            return code;
    }
    return 0;
}

/* ---------------- Miscellaneous pdfmarks ---------------- */

/*
 * Create the dictionary for an annotation or outline.  For some
 * unfathomable reason, PDF requires the following key substitutions
 * relative to pdfmarks:
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
 *     /Action /GoTo => drop the Action key
 * Also, \n in Contents strings must be replaced with \r.
 * Also, an outline dictionary with no action, Dest, Page, or View has an
 * implied GoTo action with Dest = [{ThisPage} /XYZ null null null].
 * Note that for Thread actions, the Dest is not a real destination,
 * and must not be processed as one.
 *
 * We always treat /A and /F as equivalent to /Action and /File
 * respectively.  The pdfmark and PDF documentation is so confused on the
 * issue of when the long and short names should be used that we only give
 * this a 50-50 chance of being right.
 *
 * Note that we must transform Rect and Border coordinates.
 */

typedef struct ao_params_s {
    gx_device_pdf *pdev;	/* for pdfmark_make_dest */
    const char *subtype;	/* default Subtype in top-level dictionary */
    int src_pg;		/* set to SrcPg - 1 if any */
} ao_params_t;
static int
pdfmark_put_ao_pairs(gx_device_pdf * pdev, cos_dict_t *pcd,
                     const gs_param_string * pairs, uint count,
                     const gs_matrix * pctm, ao_params_t * params,
                     bool for_outline)
{
    const gs_param_string *Action = 0;
    const gs_param_string *File = 0;
    const gs_param_string *URI = 0;
    gs_param_string Dest;
    gs_param_string Subtype;
    uint i;
    int code;
    char dest[MAX_DEST_STRING];
    bool coerce_dest = false;

    Subtype.data = 0;
    Subtype.size = 0;
    Dest.data = 0;
    Dest.size = 0;
    if (params->subtype)
        param_string_from_string(Subtype, params->subtype);
    else
        Subtype.data = 0;
    for (i = 0; i < count; i += 2) {
        const gs_param_string *pair = &pairs[i];
        int src_pg;

        if (pdf_key_eq(pair, "/SrcPg")){
            unsigned char *buf0 = (unsigned char *)gs_alloc_bytes(pdev->memory, (pair[1].size + 1) * sizeof(unsigned char),
                        "pdf_xmp_write_translated");
            memcpy(buf0, pair[1].data, pair[1].size);
            buf0[pair[1].size] = 0x00;
            if (sscanf((char *)buf0, "%ld", &src_pg) == 1)
                params->src_pg = src_pg - 1;
            gs_free_object(pdev->memory, buf0, "pdf_xmp_write_translated");
        }
        else if (!for_outline && pdf_key_eq(pair, "/Color"))
            pdfmark_put_c_pair(pcd, "/C", pair + 1);
        else if (!for_outline && pdf_key_eq(pair, "/Title"))
            pdfmark_put_c_pair(pcd, "/T", pair + 1);
        else if (pdf_key_eq(pair, "/Action") || pdf_key_eq(pair, "/A"))
            Action = pair;
        /* Previously also catered for '/F', but at the top level (outside an
         * Action dict which is handled below), a /F can only be the Flags for
         * the annotation, not a File or JavaScript action.
         */
        else if (pdf_key_eq(pair, "/File") /* || pdf_key_eq(pair, "/F")*/)
            File = pair;
        else if (pdf_key_eq(pair, "/Dest")) {
            Dest = pair[1];
            coerce_dest = true;
        }
        else if (pdf_key_eq(pair, "/URI")) {
            URI = pair;		/* save it for placing into the Action dict */
        }
        else if (pdf_key_eq(pair, "/Page") || pdf_key_eq(pair, "/View")) {
            /* Make a destination even if this is for an outline. */
            if (Dest.data == 0) {
                code = pdfmark_make_dest(dest, params->pdev, "/Page", "/View",
                                         pairs, count, 0);
                if (code >= 0) {
                    param_string_from_string(Dest, dest);
                    if (for_outline)
                        coerce_dest = false;
                } else {
                    emprintf(pdev->memory, "   **** Warning: Outline has invalid link that was discarded.\n");
                    return gs_note_error(gs_error_rangecheck);
                }
            }
        } else if (pdf_key_eq(pair, "/Subtype"))
            Subtype = pair[1];
        /*
         * We also have to replace all occurrences of \n in Contents
         * strings with \r.  Unfortunately, they probably have already
         * been converted to \012....
         */
        else if (pdf_key_eq(pair, "/Contents")) {
            byte *cstr;
            uint csize = pair[1].size;
            cos_value_t *pcv;
            uint i, j;

            /*
             * Copy the string into value storage, then update it in place.
             */
            pdfmark_put_pair(pcd, pair);
            /* Break const so we can update the (copied) string. */
            pcv = (cos_value_t *)cos_dict_find_c_key(pcd, "/Contents");
            if (pcv == NULL)
                return_error(gs_error_ioerror); /* shouldn't be possible */
            cstr = pcv->contents.chars.data;
            /* Loop invariant: j <= i < csize. */
            for (i = j = 0; i < csize;)
                if (csize - i >= 2 && !memcmp(cstr + i, "\\n", 2) &&
                    (i == 0 || cstr[i - 1] != '\\')
                    ) {
                    cstr[j] = '\\', cstr[j + 1] = 'r';
                    i += 2, j += 2;
                } else if (csize - i >= 4 && !memcmp(cstr + i, "\\012", 4) &&
                           (i == 0 || cstr[i - 1] != '\\')
                    ) {
                    cstr[j] = '\\', cstr[j + 1] = 'r';
                    i += 4, j += 2;
                } else
                    cstr[j++] = cstr[i++];
            if (j != i) {
                pcv->contents.chars.data =
                gs_resize_string(pdev->pdf_memory, cstr, csize, j,
                                 "pdfmark_put_ao_pairs");
                pcv->contents.chars.size = j;
            }
        } else if (pdf_key_eq(pair, "/L")) {
            gs_rect rect;
            char rstr[MAX_RECT_STRING];
            int code = pdfmark_scan_rect(&rect, pair + 1, pctm);
            if (code < 0)
                return code;
            pdfmark_make_rect(rstr, &rect);
            cos_dict_put_c_key_string(pcd, "/L", (byte *)rstr,
                                      strlen(rstr));
        } else if (pdf_key_eq(pair, "/Rect")) {
            gs_rect rect;
            char rstr[MAX_RECT_STRING];
            int code = pdfmark_scan_rect(&rect, pair + 1, pctm);

            if (code < 0)
                return code;
            pdfmark_make_rect(rstr, &rect);
            cos_dict_put_c_key_string(pcd, "/Rect", (byte *)rstr,
                                      strlen(rstr));
        } else if (pdf_key_eq(pair, "/Border")) {
            stream s;
            char bstr[MAX_BORDER_STRING + 1];
            int code;

            s_init(&s, NULL);
            swrite_string(&s, (byte *)bstr, MAX_BORDER_STRING + 1);
            code = pdfmark_write_border(&s, pair + 1, pctm);
            if (code < 0)
                return code;
            if (stell(&s) > MAX_BORDER_STRING)
                return_error(gs_error_limitcheck);
            bstr[stell(&s)] = 0;
            cos_dict_put_c_key_string(pcd, "/Border", (byte *)bstr,
                                      strlen(bstr));
        } else if (for_outline && pdf_key_eq(pair, "/Count"))
            DO_NOTHING;
        else {
            int i, j=0;
            unsigned char *temp, *buf0 = (unsigned char *)gs_alloc_bytes(pdev->memory, pair[1].size * 2 * sizeof(unsigned char),
                        "pdf_xmp_write_translated");
            if (buf0 == NULL)
                return_error(gs_error_VMerror);
            for (i = 0; i < pair[1].size; i++) {
                byte c = pair[1].data[i];

                buf0[j++] = c;
                /* Acrobat doesn't like the 'short' escapes. and wants them as octal....
                 * I beliwvw this should be considered an Acrtobat bug, either escapes
                 * can be used, or not, we shouldn't have to force them to octal.
                 */
                if (c == '\\') {
                    switch(pair[1].data[i + 1]) {
                        case 'b':
                            buf0[j++] = '0';
                            buf0[j++] = '1';
                            buf0[j++] = '0';
                            i++;
                            break;
                        case 'f':
                            buf0[j++] = '0';
                            buf0[j++] = '1';
                            buf0[j++] = '4';
                            i++;
                            break;
                        case 'n':
                            buf0[j++] = '0';
                            buf0[j++] = '1';
                            buf0[j++] = '2';
                            i++;
                            break;
                        case 'r':
                            buf0[j++] = '0';
                            buf0[j++] = '1';
                            buf0[j++] = '5';
                            i++;
                            break;
                        case 't':
                            buf0[j++] = '0';
                            buf0[j++] = '1';
                            buf0[j++] = '1';
                            i++;
                            break;
                        case '\\':
                            buf0[j++] = '1';
                            buf0[j++] = '3';
                            buf0[j++] = '4';
                            i++;
                            break;
                        default:
                            break;
                    }
                }
            }
            i = pair[1].size;
            temp = (unsigned char *)pair[1].data;
            ((gs_param_string *)pair)[1].data = buf0;
            ((gs_param_string *)pair)[1].size = j;

            pdfmark_put_pair(pcd, pair);

            ((gs_param_string *)pair)[1].data = temp;
            ((gs_param_string *)pair)[1].size = i;
            gs_free_object(pdev->memory, buf0, "pdf_xmp_write_translated");
        }
    }
    if (!for_outline && pdf_key_eq(&Subtype, "/Link")) {
        if (Action) {
            /* Don't delete the Dest for GoTo or file-GoToR. */
            if (pdf_key_eq(Action + 1, "/GoTo") ||
                (File && pdf_key_eq(Action + 1, "/GoToR"))
                )
                DO_NOTHING;
            else
                Dest.data = 0;
        }
    }

    /* Now handle the deferred keys. */
    if (Action) {
        const byte *astr = Action[1].data;
        const uint asize = Action[1].size;

        if ((File != 0 || Dest.data != 0 || URI != 0) &&
            (pdf_key_eq(Action + 1, "/Launch") ||
             (pdf_key_eq(Action + 1, "/GoToR") && File) ||
             pdf_key_eq(Action + 1, "/Article"))
            ) {
            cos_dict_t *adict;
            cos_value_t avalue;

            if (pdf_key_eq(Action + 1, "/Launch") && pdev->PDFA != 0) {
                switch (pdev->PDFACompatibilityPolicy) {
                    /* Default behaviour matches Adobe Acrobat, warn and continue,
                     * output file will not be PDF/A compliant
                     */
                    case 0:
                        emprintf(pdev->memory,
                                 "Launch annotations not permitted in PDF/A, reverting to normal PDF output\n");
                        pdev->AbortPDFAX = true;
                        pdev->PDFA = 0;
                        break;
                        /* Since the annotation would break PDF/A compatibility, do not
                         * include it, but warn the user that it has been dropped.
                         */
                    case 1:
                        emprintf(pdev->memory,
                                 "Launch annotations not permitted in PDF/A,  cannot drop annotation, aborting conversion\n");
                        return_error (gs_error_typecheck);
                        break;
                    case 2:
                        emprintf(pdev->memory,
                                 "Launch annotations not permitted in PDF/A,  aborting conversion\n");
                        return_error (gs_error_typecheck);
                        break;
                    default:
                        emprintf(pdev->memory,
                                 "Launch annotations not permitted in PDF/A, unrecognised PDFACompatibilityLevel,\nreverting to normal PDF output\n");
                        pdev->AbortPDFAX = true;
                        pdev->PDFA = 0;
                        break;
                }
            }

            adict = cos_dict_alloc(pdev, "action dict");
            if (adict == 0)
                return_error(gs_error_VMerror);
            if (!for_outline) {
                /* We aren't sure whether this is really needed.... */
                code = cos_dict_put_c_strings(adict, "/Type", "/Action");
                if (code < 0)
                    return code;
            }
            if (pdf_key_eq(Action + 1, "/Article")) {
                code = cos_dict_put_c_strings(adict, "/S", "/Thread");
                if (code < 0)
                    return code;
                coerce_dest = false; /* Dest is not a real destination */
            }
            else
                pdfmark_put_c_pair(adict, "/S", Action + 1);
            if (Dest.data) {
                if (coerce_dest)
                    pdfmark_coerce_dest(&Dest, dest);
                pdfmark_put_c_pair(adict, "/D", &Dest);
                Dest.data = 0;	/* so we don't write it again */
            }
            if (File) {
                pdfmark_put_c_pair(adict, "/F", File + 1);
                File = 0;	/* so we don't write it again */
            }
            if (URI) {
                /* Adobe Distiller puts a /URI key from pdfmark into the */
                /* Action dict with /S /URI as Subtype */
                pdfmark_put_pair(adict, URI);
                code = cos_dict_put_c_strings(adict, "/S", "/URI");
                if (code < 0)
                    return code;
            }
            cos_dict_put(pcd, (const byte *)"/A", 2,
                         COS_OBJECT_VALUE(&avalue, adict));
        } else if (asize >= 4 && !memcmp(astr, "<<", 2)) {
            /* Replace occurrences of /Dest, /File, and /Subtype. */
            const byte *scan = astr + 2;
            const byte *end = astr + asize;
            gs_param_string key, value;
            cos_dict_t *adict = cos_dict_alloc(pdev, "action dict");
            cos_value_t avalue;
            int code = 0;

            if (adict == 0)
                return_error(gs_error_VMerror);
            if (URI) {
                /* Adobe Distiller puts a /URI key from pdfmark into the */
                /* Action dict with /S /URI as Subtype */
                pdfmark_put_pair(adict, URI);
                if(cos_dict_put_c_strings(adict, "/S", "/URI") < 0)
                    return code;
            }
            while ((code = pdf_scan_token(&scan, end, &key.data)) > 0) {
                key.size = scan - key.data;
                if (key.data[0] != '/' ||
                    (code = pdf_scan_token_composite(&scan, end, &value.data)) != 1)
                    break;
                value.size = scan - value.data;
                if (pdev->PDFA != 0 && (pdf_key_eq(&key, "/Subtype") || pdf_key_eq(&key, "/S"))) {
                    if (pdf_key_eq(&value, "/Launch")) {
                        switch (pdev->PDFACompatibilityPolicy) {
                            /* Default behaviour matches Adobe Acrobat, warn and continue,
                             * output file will not be PDF/A compliant
                             */
                            case 0:
                                emprintf(pdev->memory,
                                         "Launch annotations not permitted in PDF/A, reverting to normal PDF output\n");
                                pdev->AbortPDFAX = true;
                                pdev->PDFA = 0;
                                break;
                                /* Since the annotation would break PDF/A compatibility, do not
                                 * include it, but warn the user that it has been dropped.
                                 */
                            case 1:
                                emprintf(pdev->memory,
                                         "Launch annotations not permitted in PDF/A,  cannot drop annotation, aborting conversion\n");
                                gs_free_object(pdev->memory->stable_memory, adict,  "action dict");
                                return_error (gs_error_typecheck);
                                break;
                            case 2:
                                emprintf(pdev->memory,
                                         "Launch annotations not permitted in PDF/A,  aborting conversion\n");
                                gs_free_object(pdev->memory->stable_memory, adict,  "action dict");
                                return_error (gs_error_typecheck);
                                break;
                            default:
                                emprintf(pdev->memory,
                                         "Launch annotations not permitted in PDF/A, unrecognised PDFACompatibilityLevel,\nreverting to normal PDF output\n");
                                pdev->AbortPDFAX = true;
                                pdev->PDFA = 0;
                                break;
                        }
                    }
                }
                if (pdf_key_eq(&key, "/Dest") || pdf_key_eq(&key, "/D")) {
                    param_string_from_string(key, "/D");
                    if (value.data[0] == '(' && pdev->CompatibilityLevel < 1.2) {
                        int i;

                        for (i = 0;i < value.size; i++) {
                            if (value.data[i] == '\\') {
                                emprintf(pdev->memory,
                                         "Link Destination contains characters which cannot be represented in a name.\nDestinations cannot be strings in versions prior to PDF 1.2. Annotation removed in the output.\n");
                                return gs_error_typecheck;
                            }
                        }
                        /****** FIXME: DETECT ESCAPES ******/
                        pdfmark_coerce_dest(&value, dest);
                    }
                } else if (pdf_key_eq(&key, "/File"))
                    param_string_from_string(key, "/F");
                else if (pdf_key_eq(&key, "/Subtype"))
                    param_string_from_string(key, "/S");
                cos_dict_put_string(adict, key.data, key.size,
                                    value.data, value.size);
            }
            if (code <= 0 || !pdf_key_eq(&key, ">>")) {
                cos_free((cos_object_t *)adict, "action dict");
                return_error(gs_error_rangecheck);
            }
            cos_dict_put(pcd, (const byte *)"/A", 2,
                         COS_OBJECT_VALUE(&avalue, adict));
        } else if (pdf_key_eq(Action + 1, "/GoTo"))
            pdfmark_put_pair(pcd, Action);
        else if (Action[1].size < 30) {
            /* Hack: we could substitute names in pdfmark_process,
               now should recognize whether it was done.
               Not a perfect method though.
               Go with it for a while. */
            char buf[30];
            int d0, d1;

            memcpy(buf, Action[1].data, Action[1].size);
            buf[Action[1].size] = 0;
            if (sscanf(buf, "%d %d R", &d0, &d1) == 2)
                pdfmark_put_pair(pcd, Action);
        }
    }
    /*
     * If we have /Dest or /File without the right kind of action,
     * simply write it at the top level.  This doesn't seem right,
     * but I'm not sure what else to do.
     */
    if (Dest.data) {
        if (coerce_dest)
            pdfmark_coerce_dest(&Dest, dest);
        /*
         * For PDF 1.2 or better  we write a Names tree, but in this case the names
         * are required to be (counter-intuitively) strings, NOT name objects....
         */
        if (pdev->CompatibilityLevel > 1.1) {
            gs_param_string DestString;
            int i = 0, j;
            char *D;

            /*
             * If the name has any 'unusual' characters, it is 'escaped' by starting with NULLs
             * I suspect we no longer need that, but here we remove the escaping NULLs
             */
            if (Dest.size > 3 && Dest.data[0] == 0x00 && Dest.data[1] == 0x00 && Dest.data[Dest.size - 1] == 0x00) {
                i = 2;
                if (Dest.size > 5 && Dest.data[2] == 0x00 && Dest.data[3] == 0x00)
                    i += 2;
                /* If it has preceeding NULLs, then it has a terminating NULL as well, get rid of that too */
                Dest.size--;
            }

            if (Dest.data[i] == '/') {
                i++;

                DestString.data = gs_alloc_bytes(pdev->memory->stable_memory, (Dest.size * 2) + 1, "DEST pdfmark temp string");
                if (DestString.data == 0)
                    return_error(gs_error_VMerror);
                DestString.size = (Dest.size * 2) + 1;
                DestString.persistent = 0;
                D = (char *)DestString.data;
                D[0] = '(';
                for (j=1;i<Dest.size;i++, j++) {
                    if (Dest.data[i] == '(' || Dest.data[i] == ')') {
                        D[j++] = '\\';
                    }
                    D[j] = Dest.data[i];
                }
                D[j++] = ')';
                DestString.size = j;
                pdfmark_put_c_pair(pcd, "/Dest", &DestString);
                gs_free_object(pdev->memory->stable_memory, D, "DEST pdfmark temp string");
            } else
                pdfmark_put_c_pair(pcd, "/Dest", &Dest);
        } else
            pdfmark_put_c_pair(pcd, "/Dest", &Dest);
    } else if (for_outline && !Action) {
        /* Make an implicit destination. */
        char dstr[1 + (sizeof(int64_t) * 8 / 3 + 1) + 25 + 1];
        int64_t page_id = pdf_page_id(pdev, pdev->next_page + 1);

        gs_snprintf(dstr, MAX_DEST_STRING, "[%"PRId64" 0 R /XYZ null null null]", page_id);
        cos_dict_put_c_key_string(pcd, "/Dest", (const unsigned char*) dstr,
                                  strlen(dstr));
    }
    if (File)
        pdfmark_put_pair(pcd, File);
    if (Subtype.data)
        pdfmark_put_c_pair(pcd, "/Subtype", &Subtype);
    return 0;
}

/* Copy an annotation dictionary. */
static int
pdfmark_annot(gx_device_pdf * pdev, gs_param_string * pairs, uint count,
              const gs_matrix * pctm, const gs_param_string *objname,
              const char *subtype)
{
    ao_params_t params;
    cos_dict_t *pcd;
    int page_index = pdev->next_page;
    cos_array_t *annots;
    cos_value_t value;
    int code;

    /* Annotations are only permitted in PDF/A if they have the
     * Print flag enabled, so we need to prescan for that here.
     */
    if(pdev->PDFA != 0) {
        int i;
        unsigned int Flags = 0;
        /* Check all the keys to see if we have a /F (Flags) key/value pair defined */
        for (i = 0; i < count; i += 2) {
            const gs_param_string *pair = &pairs[i];

            if (pdf_key_eq(pair, "/F")) {
                char Buffer[32];

                pair = &pairs[i+1];
                if (pair->size >= sizeof(Buffer))
                    code = 0;
                else {
                    memcpy(Buffer, pair->data, pair->size);
                    Buffer[pair->size] = 0x00;
                    code = sscanf(Buffer, "%u", &Flags);
                }
                if (code != 1)
                    emprintf(pdev->memory,
                             "Annotation has an invalid /Flags attribute\n");
                break;
            }
        }
        /* Check the Print flag, PDF/A annotations *must* be set to print */
        if ((Flags & 4) == 0){
            switch (pdev->PDFACompatibilityPolicy) {
                /* Default behaviour matches Adobe Acrobat, warn and continue,
                 * output file will not be PDF/A compliant
                 */
                case 0:
                    emprintf(pdev->memory,
                             "Annotation set to non-printing,\n not permitted in PDF/A, reverting to normal PDF output\n");
                    pdev->AbortPDFAX = true;
                    pdev->PDFA = 0;
                    break;
                    /* Since the annotation would break PDF/A compatibility, do not
                     * include it, but warn the user that it has been dropped.
                     */
                case 1:
                    emprintf(pdev->memory,
                             "Annotation set to non-printing,\n not permitted in PDF/A, annotation will not be present in output file\n");
                    return 0;
                    break;
                case 2:
                    emprintf(pdev->memory,
                             "Annotation set to non-printing,\n not permitted in PDF/A, aborting conversion\n");
                    return_error(gs_error_invalidfont);
                    break;
                default:
                    emprintf(pdev->memory,
                             "Annotation set to non-printing,\n not permitted in PDF/A, unrecognised PDFACompatibilityLevel,\nreverting to normal PDF output\n");
                    pdev->AbortPDFAX = true;
                    pdev->PDFA = 0;
                    break;
            }
        }
    }
    if (pdev->PDFX != 0) {
        gs_param_string Subtype;
        bool discard = true;
        pdf_page_t *page = 0L;

        page = &pdev->pages[pdev->next_page];

        if (subtype) {
            param_string_from_string(Subtype, subtype);
            if (pdf_key_eq(&Subtype, "/TrapNet") ||
                pdf_key_eq(&Subtype, "/PrinterMark"))
                discard = false;
        }
        if (discard) {
            int i;
            for (i = 0; i < count; i += 2) {
                const gs_param_string *pair = &pairs[i];
                if (pdf_key_eq(pair, "/Rect")) {
                    gs_rect rect;
                    const cos_value_t *v_trimbox, *v_bleedbox, *v_artbox, *v_cropbox;
                    const byte *p;
                    char buf[100];
                    int size, code;
                    float temp[4]; /* the type is float for sscanf. */
                    double pagebox[4] = {0, 0};

                    pagebox[2] = pdev->MediaSize[0];
                    pagebox[3] = pdev->MediaSize[1];

                    v_cropbox = v_bleedbox = v_trimbox = v_artbox = 0x0L;

                    code = pdfmark_scan_rect(&rect, pair + 1, pctm);

                    if (code < 0)
                        return code;

                    if (page && page->Page) {
                        v_trimbox = cos_dict_find_c_key(page->Page, "/TrimBox");
                        v_bleedbox = cos_dict_find_c_key(page->Page, "/BleedBox");
                        v_artbox = cos_dict_find_c_key(page->Page, "/ArtBox");
                        v_cropbox = cos_dict_find_c_key(page->Page, "/CropBox");
                    }

                    if (v_cropbox != NULL && v_cropbox->value_type == COS_VALUE_SCALAR) {
                        p = v_cropbox->contents.chars.data;
                        size = min (v_cropbox->contents.chars.size, sizeof(buf) - 1);
                        memcpy(buf, p, size);
                        buf[size] = 0;
                        if (sscanf(buf, "[ %g %g %g %g ]",
                                &temp[0], &temp[1], &temp[2], &temp[3]) == 4) {
                            if (temp[0] > pagebox[0]) pagebox[0] = temp[0];
                            if (temp[1] > pagebox[1]) pagebox[1] = temp[1];
                        }
                    }
                    if (v_bleedbox != NULL && v_bleedbox->value_type == COS_VALUE_SCALAR) {
                        p = v_bleedbox->contents.chars.data;
                        size = min (v_bleedbox->contents.chars.size, sizeof(buf) - 1);
                        memcpy(buf, p, size);
                        buf[size] = 0;
                        if (sscanf(buf, "[ %g %g %g %g ]",
                                &temp[0], &temp[1], &temp[2], &temp[3]) == 4) {
                            if (temp[0] > pagebox[0]) pagebox[0] = temp[0];
                            if (temp[1] > pagebox[1]) pagebox[1] = temp[1];
                        }
                    }
                    if (v_trimbox != NULL && v_trimbox->value_type == COS_VALUE_SCALAR) {
                        p = v_trimbox->contents.chars.data;
                        size = min (v_trimbox->contents.chars.size, sizeof(buf) - 1);
                        memcpy(buf, p, size);
                        buf[size] = 0;
                        if (sscanf(buf, "[ %g %g %g %g ]",
                                &temp[0], &temp[1], &temp[2], &temp[3]) == 4) {
                            if (temp[0] > pagebox[0]) pagebox[0] = temp[0];
                            if (temp[1] > pagebox[1]) pagebox[1] = temp[1];
                        }
                    }
                    if (v_artbox != NULL && v_artbox->value_type == COS_VALUE_SCALAR) {
                        p = v_artbox->contents.chars.data;
                        size = min (v_artbox->contents.chars.size, sizeof(buf) - 1);
                        memcpy(buf, p, size);
                        buf[size] = 0;
                        if (sscanf(buf, "[ %g %g %g %g ]",
                                &temp[0], &temp[1], &temp[2], &temp[3]) == 4) {
                            if (temp[0] > pagebox[0]) pagebox[0] = temp[0];
                            if (temp[1] > pagebox[1]) pagebox[1] = temp[1];
                        }
                    }
                    if (v_cropbox == 0 && v_trimbox == 0 && v_artbox == 0 && v_bleedbox == 0) {
                        if (pdev->PDFXTrimBoxToMediaBoxOffset.size >= 4 &&
                                    pdev->PDFXTrimBoxToMediaBoxOffset.data[0] >= 0 &&
                                    pdev->PDFXTrimBoxToMediaBoxOffset.data[1] >= 0 &&
                                    pdev->PDFXTrimBoxToMediaBoxOffset.data[2] >= 0 &&
                                    pdev->PDFXTrimBoxToMediaBoxOffset.data[3] >= 0) {
                            pagebox[0] += pdev->PDFXTrimBoxToMediaBoxOffset.data[0];
                            pagebox[1] += pdev->PDFXTrimBoxToMediaBoxOffset.data[3];
                            pagebox[2] -= pdev->PDFXTrimBoxToMediaBoxOffset.data[1];
                            pagebox[3] -= pdev->PDFXTrimBoxToMediaBoxOffset.data[2];
                        } else if (pdev->PDFXBleedBoxToTrimBoxOffset.size >= 4 &&
                                pdev->PDFXBleedBoxToTrimBoxOffset.data[0] >= 0 &&
                                pdev->PDFXBleedBoxToTrimBoxOffset.data[1] >= 0 &&
                                pdev->PDFXBleedBoxToTrimBoxOffset.data[2] >= 0 &&
                                pdev->PDFXBleedBoxToTrimBoxOffset.data[3] >= 0) {
                            pagebox[0] -= pdev->PDFXBleedBoxToTrimBoxOffset.data[0];
                            pagebox[1] -= pdev->PDFXBleedBoxToTrimBoxOffset.data[3];
                            pagebox[2] += pdev->PDFXBleedBoxToTrimBoxOffset.data[1];
                            pagebox[3] += pdev->PDFXBleedBoxToTrimBoxOffset.data[2];
                        }
                    }

                    if (rect.p.x > pagebox[2] || rect.q.x < pagebox[0] ||
                        rect.p.y > pagebox[3] || rect.q.y < pagebox[1])
                        break;
                    switch (pdev->PDFACompatibilityPolicy) {
                        /* Default behaviour matches Adobe Acrobat, warn and continue,
                         * output file will not be PDF/A compliant
                         */
                        case 0:
                            emprintf(pdev->memory,
                                     "Annotation (not TrapNet or PrinterMark) on page,\n not permitted in PDF/X, reverting to normal PDF output\n");
                            pdev->AbortPDFAX = true;
                            pdev->PDFX = 0;
                            break;
                            /* Since the annotation would break PDF/A compatibility, do not
                             * include it, but warn the user that it has been dropped.
                             */
                        case 1:
                            emprintf(pdev->memory,
                                     "Annotation (not TrapNet or PrinterMark) on page,\n not permitted in PDF/X, annotation will not be present in output file\n");
                            return 0;
                            break;
                        case 2:
                            emprintf(pdev->memory,
                                     "Annotation (not TrapNet or PrinterMark) on page,\n not permitted in PDF/X, aborting conversion\n");
                            return_error(gs_error_invalidfont);
                            break;
                        default:
                            emprintf(pdev->memory,
                                     "Annotation s(not TrapNet or PrinterMark) on page,\n not permitted in PDF/A, unrecognised PDFACompatibilityLevel,\nreverting to normal PDF output\n");
                            pdev->AbortPDFAX = true;
                            pdev->PDFX = 0;
                            break;
                    }
                    break;
                }
            }
            if (i > count) {
                switch (pdev->PDFACompatibilityPolicy) {
                    /* Default behaviour matches Adobe Acrobat, warn and continue,
                     * output file will not be PDF/X compliant
                     */
                    case 0:
                        emprintf(pdev->memory,
                                 "Annotation (not TrapNet or PrinterMark) potentially on page (no /Rect in dict),\n not permitted in PDF/X, reverting to normal PDF output\n");
                        pdev->AbortPDFAX = true;
                        pdev->PDFX = 0;
                        break;
                        /* Since the annotation would break PDF/X compatibility, do not
                         * include it, but warn the user that it has been dropped.
                         */
                    case 1:
                        emprintf(pdev->memory,
                                 "Annotation (not TrapNet or PrinterMark) potentially on page (no /Rect in dict),\n not permitted in PDF/X, annotation will not be present in output file\n");
                        return 0;
                        break;
                    case 2:
                        emprintf(pdev->memory,
                                 "Annotation (not TrapNet or PrinterMark) potentially on page (no /Rect in dict),\n not permitted in PDF/X, aborting conversion\n");
                        return_error(gs_error_invalidfont);
                        break;
                    default:
                        emprintf(pdev->memory,
                                 "Annotation s(not TrapNet or PrinterMark) potentially on page (no /Rect in dict),\n not permitted in PDF/A, unrecognised PDFACompatibilityLevel,\nreverting to normal PDF output\n");
                        pdev->AbortPDFAX = true;
                        pdev->PDFX = 0;
                        break;
                }
            }
        }
    }
    params.pdev = pdev;
    params.subtype = subtype;
    params.src_pg = -1;
    code = pdf_make_named_dict(pdev, objname, &pcd, true);
    if (code < 0)
        return code;
    code = cos_dict_put_c_strings(pcd, "/Type", "/Annot");
    if (code < 0) {
        (void)pdf_obj_mark_unused(pdev, pcd->id);
        cos_free((cos_object_t *)pcd, "pdfmark_annot");
        return code;
    }
    code = pdfmark_put_ao_pairs(pdev, pcd, pairs, count, pctm, &params, false);
    if (code < 0) {
        (void)pdf_obj_mark_unused(pdev, pcd->id);
        cos_free((cos_object_t *)pcd, "pdfmark_annot");
        return code;
    }
    if (params.src_pg >= 0)
        page_index = params.src_pg;
    if (pdf_page_id(pdev, page_index + 1) <= 0) {
        (void)pdf_obj_mark_unused(pdev, pcd->id);
        cos_free((cos_object_t *)pcd, "pdfmark_annot");
        return_error(gs_error_rangecheck);
    }
    annots = pdev->pages[page_index].Annots;
    if (annots == 0) {
        annots = cos_array_alloc(pdev, "pdfmark_annot");
        if (annots == 0) {
            (void)pdf_obj_mark_unused(pdev, pcd->id);
            cos_free((cos_object_t *)pcd, "pdfmark_annot");
            return_error(gs_error_VMerror);
        }
        pdev->pages[page_index].Annots = annots;
    }
    if (!objname) {
        /* Write the annotation now. */
        COS_WRITE_OBJECT(pcd, pdev, resourceAnnotation);
        COS_RELEASE(pcd, "pdfmark_annot");
    }
    return cos_array_add(annots,
                         cos_object_value(&value, COS_OBJECT(pcd)));
}

/* ANN pdfmark */
static int
pdfmark_ANN(gx_device_pdf * pdev, gs_param_string * pairs, uint count,
            const gs_matrix * pctm, const gs_param_string * objname)
{
    return pdfmark_annot(pdev, pairs, count, pctm, objname, "/Text");
}

/* LNK pdfmark (obsolescent) */
static int
pdfmark_LNK(gx_device_pdf * pdev, gs_param_string * pairs, uint count,
            const gs_matrix * pctm, const gs_param_string * objname)
{
    return pdfmark_annot(pdev, pairs, count, pctm, objname, "/Link");
}

/* Write and release one node of the outline tree. */
static int
pdfmark_write_outline(gx_device_pdf * pdev, pdf_outline_node_t * pnode,
                      int64_t next_id)
{
    stream *s;
    int code = 0;

    pdf_open_separate(pdev, pnode->id, resourceOutline);
    if (pnode->action != NULL)
        pnode->action->id = pnode->id;
    else {
        emprintf1(pdev->memory,
                  "pdfmark error: Outline node %ld has no action or destination.\n",
                  (unsigned long)pnode->id);
        code = gs_note_error(gs_error_undefined);
    }
    s = pdev->strm;
    stream_puts(s, "<< ");
    if (pnode->action != NULL)
        cos_dict_elements_write(pnode->action, pdev);
    if (pnode->count)
        pprintd1(s, "/Count %d ", pnode->count);
    pprinti64d1(s, "/Parent %"PRId64" 0 R\n", pnode->parent_id);
    if (pnode->prev_id)
        pprinti64d1(s, "/Prev %"PRId64" 0 R\n", pnode->prev_id);
    if (next_id)
        pprinti64d1(s, "/Next %"PRId64" 0 R\n", next_id);
    if (pnode->first_id)
        pprinti64d2(s, "/First %"PRId64" 0 R /Last %"PRId64" 0 R\n",
                  pnode->first_id, pnode->last_id);
    stream_puts(s, ">>\n");
    pdf_end_separate(pdev, resourceOutline);
    if (pnode->action != NULL)
        COS_FREE(pnode->action, "pdfmark_write_outline");
    pnode->action = 0;
    return code;
}

/* Adjust the parent's count when writing an outline node. */
static void
pdfmark_adjust_parent_count(pdf_outline_level_t * plevel)
{
    pdf_outline_level_t *parent = plevel - 1;
    int count = plevel->last.count;

    if (count > 0) {
        if (parent->last.count < 0)
            parent->last.count -= count;
        else
            parent->last.count += count;
    }
}

/*
 * Close the current level of the outline tree.  Note that if we are at
 * the end of the document, some of the levels may be incomplete if the
 * Count values were incorrect.
 */
int
pdfmark_close_outline(gx_device_pdf * pdev)
{
    int depth = pdev->outline_depth;
    pdf_outline_level_t *plevel = &pdev->outline_levels[depth];
    int code = 0;

    if (plevel->last.id) {	/* check for incomplete tree */
        code = pdfmark_write_outline(pdev, &plevel->last, 0);
    }
    if (depth > 0) {
        plevel[-1].last.last_id = plevel->last.id;
        pdfmark_adjust_parent_count(plevel);
        --plevel;
        if (plevel->last.count < 0)
            pdev->closed_outline_depth--;
        pdev->outline_depth--;
    }
    return code;
}

/* OUT pdfmark */
static int
pdfmark_OUT(gx_device_pdf * pdev, gs_param_string * pairs, uint count,
            const gs_matrix * pctm, const gs_param_string * no_objname)
{
    int depth = pdev->outline_depth;
    pdf_outline_level_t *plevel = &pdev->outline_levels[depth];
    int sub_count = 0;
    uint i;
    pdf_outline_node_t node;
    ao_params_t ao;
    int code;

    for (i = 0; i < count; i += 2) {
        const gs_param_string *pair = &pairs[i];

        if (pdf_key_eq(pair, "/Count"))
            pdfmark_scan_int(pair + 1, &sub_count);
    }
    if (sub_count != 0 && depth == pdev->max_outline_depth - 1) {
        pdf_outline_level_t *new_ptr;

        new_ptr = (pdf_outline_level_t *)gs_alloc_bytes(pdev->pdf_memory, (pdev->max_outline_depth + INITIAL_MAX_OUTLINE_DEPTH) * sizeof(pdf_outline_level_t) * sizeof(pdf_outline_level_t), "outline_levels array");
        if (!new_ptr)
            return_error(gs_error_VMerror);
        memcpy (new_ptr, pdev->outline_levels, pdev->max_outline_depth * sizeof(pdf_outline_level_t));
        gs_free_object(pdev->pdf_memory, pdev->outline_levels, "outline_levels array");
        pdev->outline_levels = new_ptr;
        pdev->max_outline_depth += INITIAL_MAX_OUTLINE_DEPTH;
        plevel = &pdev->outline_levels[depth];
    }
    node.action = cos_dict_alloc(pdev, "pdfmark_OUT");
    if (node.action == 0)
        return_error(gs_error_VMerror);
    ao.pdev = pdev;
    ao.subtype = 0;
    ao.src_pg = -1;
    code = pdfmark_put_ao_pairs(pdev, node.action, pairs, count, pctm, &ao,
                                true);
    if (code < 0) {
        cos_free((cos_object_t *)node.action, "pdfmark_OUT");
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
        plevel->first.action = 0; /* never used */
    } else {			/* Write the previous node. */
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
        plevel->last.count = plevel->last.id = 0;
        plevel->first.action = plevel->last.action = 0;	/* for GC */
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
static int
pdfmark_write_bead(gx_device_pdf * pdev, const pdf_bead_t * pbead)
{
    stream *s;
    char rstr[MAX_RECT_STRING];

    pdf_open_separate(pdev, pbead->id, resourceArticle);
    s = pdev->strm;
    pprinti64d3(s, "<</T %"PRId64" 0 R/V %"PRId64" 0 R/N %"PRId64" 0 R",
              pbead->article_id, pbead->prev_id, pbead->next_id);
    if (pbead->page_id != 0)
        pprinti64d1(s, "/P %"PRId64" 0 R", pbead->page_id);
    pdfmark_make_rect(rstr, &pbead->rect);
    pprints1(s, "/R%s>>\n", rstr);
    return pdf_end_separate(pdev, resourceArticle);
}

/* Finish writing an article, and release its data. */
int
pdfmark_write_article(gx_device_pdf * pdev, const pdf_article_t * part)
{
    pdf_article_t art;
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
    pdf_open_separate(pdev, art.contents->id, resourceArticle);
    s = pdev->strm;
    pprinti64d1(s, "<</F %"PRId64" 0 R/I<<", art.first.id);
    cos_dict_elements_write(art.contents, pdev);
    stream_puts(s, ">> >>\n");
    return pdf_end_separate(pdev, resourceArticle);
}

/* ARTICLE pdfmark */
static int
pdfmark_ARTICLE(gx_device_pdf * pdev, gs_param_string * pairs, uint count,
                const gs_matrix * pctm, const gs_param_string * no_objname)
{
    gs_memory_t *mem = pdev->pdf_memory;
    gs_param_string title;
    gs_param_string rectstr;
    gs_rect rect;
    int64_t bead_id;
    pdf_article_t *part;
    int code;

    if (!pdfmark_find_key("/Title", pairs, count, &title) ||
        !pdfmark_find_key("/Rect", pairs, count, &rectstr)
        )
        return_error(gs_error_rangecheck);
    if ((code = pdfmark_scan_rect(&rect, &rectstr, pctm)) < 0)
        return code;
    bead_id = pdf_obj_ref(pdev);

    /* Find the article with this title, or create one. */
    for (part = pdev->articles; part != 0; part = part->next) {
        const cos_value_t *a_title =
            cos_dict_find_c_key(part->contents, "/Title");

        if (a_title != 0 && !COS_VALUE_IS_OBJECT(a_title) &&
            !bytes_compare(a_title->contents.chars.data,
                           a_title->contents.chars.size,
                           title.data, title.size))
            break;
    }
    if (part == 0) {		/* Create the article. */
        cos_dict_t *contents =
            cos_dict_alloc(pdev, "pdfmark_ARTICLE(contents)");

        if (contents == 0)
            return_error(gs_error_VMerror);
        part = gs_alloc_struct(mem, pdf_article_t, &st_pdf_article,
                               "pdfmark_ARTICLE(article)");
        if (part == 0 || contents == 0) {
            gs_free_object(mem, part, "pdfmark_ARTICLE(article)");
            if (contents)
                COS_FREE(contents, "pdfmark_ARTICLE(contents)");
            return_error(gs_error_VMerror);
        }
        contents->id = pdf_obj_ref(pdev);
        part->next = pdev->articles;
        pdev->articles = part;
        cos_dict_put_string(contents, (const byte *)"/Title", 6,
                            title.data, title.size);
        part->first.id = part->last.id = 0;
        part->contents = contents;
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
    part->last.article_id = part->contents->id;
    part->last.next_id = 0;
    part->last.rect = rect;
    {
        gs_param_string page_string;
        int page = 0;
        uint i;

        pdfmark_find_key("/Page", pairs, count, &page_string);
        page = pdfmark_page_number(pdev, &page_string);
        code = update_max_page_reference(pdev, &page);
        if (code < 0)
            return code;
        part->last.page_id = pdf_page_id(pdev, page);
        for (i = 0; i < count; i += 2) {
            if (pdf_key_eq(&pairs[i], "/Rect") || pdf_key_eq(&pairs[i], "/Page"))
                continue;
            pdfmark_put_pair(part->contents, &pairs[i]);
        }
    }
    if (part->first.id == 0) {	/* This is the first bead of the article. */
        part->first = part->last;
        part->last.id = 0;
    }
    return 0;
}

/* EMBED pdfmark */
static int
pdfmark_EMBED(gx_device_pdf * pdev, gs_param_string * pairs, uint count,
             const gs_matrix * pctm, const gs_param_string * objname)
{
    gs_param_string key;
    int i;

    if (pdev->CompatibilityLevel < 1.4)
        return_error(gs_error_undefined);
    if (pdev->PDFA > 0 && pdev->PDFA < 2) {
        switch(pdev->PDFACompatibilityPolicy) {
            default:
            case 0:
                emprintf(pdev->memory,
                "The PDF/A-1 specifcation prohibits the embedding of files, reverting to normal PDF output.\n");
                pdev->AbortPDFAX = true;
                pdev->PDFA = 0;
                return 0;
            case 1:
                emprintf(pdev->memory,
                "The PDF/A-1 specifcation prohibits the embedding of files, pdfamrk operatoin ignored.\n");
                break;
            case 2:
                return_error(gs_error_undefined);
        }
    }
    if (pdev->PDFA > 0 && pdev->PDFA < 3) {
        emprintf(pdev->memory,
        "The PDF/A-2 specifcation only permits the embedding of PDF/A-1 or PDF/A-2 files.\n");
        emprintf(pdev->memory,
        "The pdfwrite device has not validated this embedded file, output may not conform to PDF/A-2.\n");
    }
    if (!pdfmark_find_key("/FS", pairs, count, &key))
        return_error(gs_error_rangecheck);
    if (!pdfmark_find_key("/Name", pairs, count, &key))
        return_error(gs_error_rangecheck);
    if (!pdev->EmbeddedFiles) {
        pdev->EmbeddedFiles = cos_dict_alloc(pdev, "pdfmark_EMBED(EmbeddedFiles)");
        if (pdev->EmbeddedFiles == 0)
            return_error(gs_error_VMerror);
        pdev->EmbeddedFiles->id = pdf_obj_ref(pdev);
    }
    if (pdev->PDFA == 3 && !pdev->AF && !cos_dict_find_c_key(pdev->Catalog, "/AF")) {
        pdev->AF = cos_array_alloc(pdev, "pdfmark_EMBED(EmbeddedFiles)");
        if (pdev->AF == 0)
            return_error(gs_error_VMerror);
        pdev->AF->id = pdf_obj_ref(pdev);
    }

    for (i = 0; i < count; i += 2) {
        if (pdf_key_eq(&pairs[i], "/FS")) {
            if (!cos_dict_find_c_key(pdev->Catalog, "/AF")) {
                cos_value_t v;
                cos_object_t *object;
                int64_t id;
                int code;

                {
                    char written = pairs[i+1].data[pairs[i+1].size];
                    char *data = (char *)pairs[i+1].data;

                    data[pairs[i+1].size] = 0x00;
                    code = sscanf(data, "%"PRId64" 0 R", &id);
                    data[pairs[i+1].size] = written;
                    if (code < 1)
                        return_error(gs_error_rangecheck);
                }

                object = cos_reference_alloc(pdev, "embedded file");
                object->id = id;
                COS_OBJECT_VALUE(&v, object);
                code = cos_dict_put(pdev->EmbeddedFiles, key.data, key.size, &v);
                if (code < 0)
                    return code;
                if (pdev->PDFA == 3) {
                    object = cos_reference_alloc(pdev, "embedded file");
                    object->id = id;
                    COS_OBJECT_VALUE(&v, object);
                    code = cos_array_add(pdev->AF, &v);
                    if (code < 0)
                        return code;
                }
            }
            else
                return cos_dict_put_string(pdev->EmbeddedFiles, key.data, key.size, pairs[i+1].data, pairs[i+1].size);
        }
    }
    return 0;
}

/* DEST pdfmark */
static int
pdfmark_DEST(gx_device_pdf * pdev, gs_param_string * pairs, uint count,
             const gs_matrix * pctm, const gs_param_string * objname)
{
    int present;
    char dest[MAX_DEST_STRING];
    gs_param_string key;
    cos_value_t value;
    cos_dict_t *ddict;
    int i, code;

    if (!pdfmark_find_key("/Dest", pairs, count, &key) ||
        (present =
         pdfmark_make_dest(dest, pdev, "/Page", "/View", pairs, count, 1)) < 0
        )
        return_error(gs_error_rangecheck);
    cos_string_value(&value, (byte *)dest, strlen(dest));
    if (!pdev->Dests) {
        pdev->Dests = cos_dict_alloc(pdev, "pdfmark_DEST(Dests)");
        if (pdev->Dests == 0)
            return_error(gs_error_VMerror);
        pdev->Dests->id = pdf_obj_ref(pdev);
    }

    /*
     * Create the destination as a dictionary with a D key.
     */
    code = pdf_make_named_dict(pdev, objname, &ddict, false);
    ddict->id = pdf_obj_ref(pdev);

    if (code < 0)
        return code;
    code = cos_dict_put_c_key_string(ddict, "/D", (byte *)dest,
                                     strlen(dest));
    for (i = 0; code >= 0 && i < count; i += 2)
        if (!pdf_key_eq(&pairs[i], "/Dest") &&
            !pdf_key_eq(&pairs[i], "/Page") &&
            !pdf_key_eq(&pairs[i], "/View")
            )
            code = pdfmark_put_pair(ddict, &pairs[i]);
    if (code < 0)
        return code;
    COS_WRITE_OBJECT(ddict, pdev, resourceOther);
    COS_OBJECT_VALUE(&value, ddict);
    COS_RELEASE(ddict, "pdfmark_DEST(Dests dict)");

    return cos_dict_put(pdev->Dests, key.data, key.size, &value);
}

/* Check that pass-through PostScript code is a string. */
static bool
ps_source_ok(const gs_memory_t *mem, const gs_param_string * psource)
{
    if (psource->size >= 2 && psource->data[0] == '(' &&
        psource->data[psource->size - 1] == ')'
        )
        return true;
    else {
        int i;
        lprintf("bad PS passthrough: ");
        for (i=0; i<psource->size; i++)
            errprintf(mem, "%c", psource->data[i]);
        errprintf(mem, "\n");
        return false;
    }
}

/* Write the contents of pass-through PostScript code. */
/* Return the size written on the file. */
static uint
pdfmark_write_ps(stream *s, const gs_param_string * psource)
{
    /****** REMOVE ESCAPES WITH PSSDecode, SEE gdevpdfr p. 2 ******/
    uint size = psource->size - 2;

    stream_write(s, psource->data + 1, size);
    stream_putc(s, '\n');
    return size + 1;
}

/* Start a XObject. */
static int
start_XObject(gx_device_pdf * pdev, bool compress, cos_stream_t **ppcs)
{   pdf_resource_t *pres;
    cos_stream_t *pcs;
    int code;

    code = pdf_open_page(pdev, PDF_IN_STREAM);
    if (code < 0)
        return code;
    code = pdf_enter_substream(pdev, resourceXObject, gs_no_id, &pres, false,
                pdev->CompressStreams);
    if (code < 0)
        return code;
    pdev->accumulating_a_global_object = true;
    pcs = (cos_stream_t *)pres->object;
    pdev->substream_Resources = cos_dict_alloc(pdev, "start_XObject");
    if (!pdev->substream_Resources)
        return_error(gs_error_VMerror);
    if (pdev->ForOPDFRead) {
        code = cos_dict_put_c_key_bool((cos_dict_t *)pres->object, "/.Global", true);
        if (code < 0)
            return code;
    }
    pres->named = true;
    pres->where_used = 0;	/* initially not used */
    pcs->pres = pres;
    *ppcs = pcs;
    return 0;
}

/* PS pdfmark */
#define MAX_PS_INLINE 100
static int
pdfmark_PS(gx_device_pdf * pdev, gs_param_string * pairs, uint count,
           const gs_matrix * pctm, const gs_param_string * objname)
{
    gs_param_string source;
    gs_param_string level1;

    if (!pdfmark_find_key("/DataSource", pairs, count, &source) ||
        !ps_source_ok(pdev->memory, &source) ||
        (pdfmark_find_key("/Level1", pairs, count, &level1) &&
         !ps_source_ok(pdev->memory, &level1))
        )
        return_error(gs_error_rangecheck);
    if (level1.data == 0 && source.size <= MAX_PS_INLINE && objname == 0) {
        /* Insert the PostScript code in-line */
        int code = pdf_open_contents(pdev, PDF_IN_STREAM);
        stream *s;

        if (code < 0)
            return code;
        s = pdev->strm;
        stream_write(s, source.data, source.size);
        stream_puts(s, " PS\n");
    } else {
        /* Put the PostScript code in a resource. */
        cos_stream_t *pcs;
        int code;
        gs_id level1_id = gs_no_id;
        pdf_resource_t *pres;

        if (level1.data != 0) {
            pdf_resource_t *pres;

            code = pdf_enter_substream(pdev,
                        resourceXObject,
                        gs_no_id, &pres, true,
                        pdev->CompressStreams);
            if (code < 0)
                return code;
            pcs = (cos_stream_t *)pres->object;
            if (pdev->ForOPDFRead && objname != 0) {
                code = cos_dict_put_c_key_bool((cos_dict_t *)pres->object, "/.Global", true);
                if (code < 0)
                    return code;
            }
            pres->named = (objname != 0);
            pres->where_used = 0;
            pcs->pres = pres;
            DISCARD(pdfmark_write_ps(pdev->strm, &level1));
            code = pdf_exit_substream(pdev);
            if (code < 0)
                return code;
            code = cos_write_object(pres->object, pdev, resourceOther);
            if (code < 0)
                return code;
            level1_id = pres->object->id;
        }
        code = start_XObject(pdev, pdev->params.CompressPages, &pcs);
        if (code < 0)
            return code;
        pres = pdev->accumulating_substream_resource;
        code = cos_stream_put_c_strings(pcs, "/Type", "/XObject");
        if (code < 0)
            return code;
        code = cos_stream_put_c_strings(pcs, "/Subtype", "/PS");
        if (code < 0)
            return code;
        if (level1_id != gs_no_id) {
            char r[MAX_DEST_STRING];

            gs_snprintf(r, sizeof(r), "%ld 0 R", level1_id);
            code = cos_dict_put_c_key_string(cos_stream_dict(pcs), "/Level1",
                                             (byte *)r, strlen(r));
            if (code < 0)
                return code;
        }
        DISCARD(pdfmark_write_ps(pdev->strm, &source));
        code = pdf_exit_substream(pdev);
        if (code < 0)
            return code;
        {   gs_const_string objname1, *pon = NULL;

            if (objname != NULL) {
                objname1.data = objname->data;
                objname1.size = objname->size;
                pon = &objname1;
            }
            code = pdfmark_bind_named_object(pdev, pon, &pres);
            if (code < 0)
                return code;
        }
        code = pdf_open_contents(pdev, PDF_IN_STREAM);
        if (code < 0)
            return code;
        pcs->pres->where_used |= pdev->used_mask;
        pprinti64d1(pdev->strm, "/R%"PRId64" Do\n", pcs->id);
    }
    return 0;
}

/* Common code for pdfmarks that do PUT into a specific object. */
static int
pdfmark_put_pairs(cos_dict_t *pcd, gs_param_string * pairs, uint count)
{
    int code = 0, i;

    if (count & 1)
        return_error(gs_error_rangecheck);
    for (i = 0; code >= 0 && i < count; i += 2)
        code = pdfmark_put_pair(pcd, pairs + i);
    return code;
}

/* PAGES pdfmark */
static int
pdfmark_PAGES(gx_device_pdf * pdev, gs_param_string * pairs, uint count,
              const gs_matrix * pctm, const gs_param_string * no_objname)
{
    return pdfmark_put_pairs(pdev->Pages, pairs, count);
}

/* PAGE pdfmark */
static int
pdfmark_PAGE(gx_device_pdf * pdev, gs_param_string * pairs, uint count,
             const gs_matrix * pctm, const gs_param_string * no_objname)
{
    return pdfmark_put_pairs(pdf_current_page_dict(pdev), pairs, count);
}

/* Add a page label for the current page. The last label on a page
 * overrides all previous labels for this page. Unlabeled pages will get
 * empty page labels. label == NULL flushes the last label */
static int
pdfmark_add_pagelabel(gx_device_pdf * pdev, const gs_param_string *label)
{
    cos_value_t value;
    cos_dict_t *dict = 0;
    int code = 0;

    /* create label dict (and page label array if not present yet) */
    if (label != 0) {
        if (!pdev->PageLabels) {
            pdev->PageLabels = cos_array_alloc(pdev,
                    "pdfmark_add_pagelabel(PageLabels)");
            if (pdev->PageLabels == 0)
                return_error(gs_error_VMerror);
            pdev->PageLabels->id = pdf_obj_ref(pdev);

            /* empty label for unlabled pages before first labled page */
            pdev->PageLabels_current_page = 0;
            pdev->PageLabels_current_label = cos_dict_alloc(pdev,
                                           "pdfmark_add_pagelabel(first)");
            if (pdev->PageLabels_current_label == 0)
                return_error(gs_error_VMerror);
        }

        dict = cos_dict_alloc(pdev, "pdfmark_add_pagelabel(dict)");
        if (dict == 0)
            return_error(gs_error_VMerror);

        code = cos_dict_put_c_key(dict, "/P", cos_string_value(&value,
            label->data, label->size));
        if (code < 0) {
            COS_FREE(dict, "pdfmark_add_pagelabel(dict)");
            return code;
        }
    }

    /* flush current label */
    if (label == 0 || pdev->next_page != pdev->PageLabels_current_page) {
        /* handle current label */
        if (pdev->PageLabels_current_label) {
            if (code >= 0) {
                code = cos_array_add_int(pdev->PageLabels,
                        pdev->PageLabels_current_page);
                if (code >= 0)
                    code = cos_array_add(pdev->PageLabels,
                            COS_OBJECT_VALUE(&value,
                                pdev->PageLabels_current_label));
            }
            pdev->PageLabels_current_label = 0;
        }

        /* handle unlabled pages between current labeled page and
         * next labeled page */
        if (pdev->PageLabels) {
            if (pdev->next_page - pdev->PageLabels_current_page > 1) {
                cos_dict_t *tmp = cos_dict_alloc(pdev,
                        "pdfmark_add_pagelabel(tmp)");
                if (tmp == 0)
                    return_error(gs_error_VMerror);

                code = cos_array_add_int(pdev->PageLabels,
                        pdev->PageLabels_current_page + 1);
                if (code >= 0)
                    code = cos_array_add(pdev->PageLabels,
                            COS_OBJECT_VALUE(&value, tmp));
            }
        }
    }

    /* new current label */
    if (pdev->PageLabels_current_label)
        COS_FREE(pdev->PageLabels_current_label,
                "pdfmark_add_pagelabel(current_label)");
    pdev->PageLabels_current_label = dict;
    pdev->PageLabels_current_page = pdev->next_page;

    return code;
}

/* Close the pagelabel numtree.*/
int
pdfmark_end_pagelabels(gx_device_pdf * pdev)
{
    return pdfmark_add_pagelabel(pdev, 0);
}

/* [ /Label string /PlateColor string pdfmark */
/* FIXME: /PlateColor is ignored */
static int
pdfmark_PAGELABEL(gx_device_pdf * pdev, gs_param_string * pairs, uint count,
             const gs_matrix * pctm, const gs_param_string * no_objname)
{
    gs_param_string key;

    if (pdev->CompatibilityLevel >= 1.3) {
        if (pdfmark_find_key("/Label", pairs, count, &key)) {
            return pdfmark_add_pagelabel(pdev, &key);
        }
    }
    return 0;
}

static int is_XMP_Key(const gs_param_string *param)
{
    if (pdf_key_eq(param, "/Title"))
        return 1;
    if (pdf_key_eq(param, "/Author"))
        return 1;
    if (pdf_key_eq(param, "/Subject"))
        return 1;
    if (pdf_key_eq(param, "/Keywords"))
        return 1;
    if (pdf_key_eq(param, "/Creator"))
        return 1;
    if (pdf_key_eq(param, "/Producer"))
        return 1;
    /* These two aren't string data types and so won't affect anything
     * in the DOCINFO pdfmark, which is the only client for this code currently
     * but we may want to use this in future for other purposed.
     */
    if (pdf_key_eq(param, "/CreationDate"))
        return 1;
    if (pdf_key_eq(param, "/ModDate"))
        return 1;
    return 0;
}

/* DOCINFO pdfmark */
static int
pdfmark_DOCINFO(gx_device_pdf * pdev, gs_param_string * pairs, uint count,
                const gs_matrix * pctm, const gs_param_string * no_objname)
{
    /*
     * We could use pdfmark_put_pairs(pdev->Info, pairs, count), except
     * that we want to replace "Distiller" with our own name as the
     * Producer.
     */
    cos_dict_t *const pcd = pdev->Info;
    int code = 0, i;

    if (count & 1)
        return_error(gs_error_rangecheck);
    for (i = 0; code >= 0 && i < count; i += 2) {
        const gs_param_string *pair = pairs + i;

        if (pdev->CompatibilityLevel >= 2.0) {
            if (!pdf_key_eq(pairs + i, "/ModDate") && !pdf_key_eq(pairs + i, "/CreationDate"))
                continue;
        }

        if (pdev->PDFA !=0 && is_XMP_Key(pair)) {
            const gs_param_string *p = pairs + i + 1;
            bool abort = false;

            /* Ensure that we can write the XMP string. If not, handle this according to PDFACompatibilityPolicy */
            code = pdf_xmp_write_translated(pdev, NULL, p->data + 1, p->size - 2, NULL);
            abort = code < 0;

            if (abort == true)
            {
                /* Can't handle UTF16BE in PDF/A1, so abort this pair or abort PDF/A or just abort,
                 * depending on PDFACompatibilityPolicy
                 */
                switch (pdev->PDFACompatibilityPolicy) {
                    case 0:
                        emprintf(pdev->memory,
                         "Text string detected in DOCINFO cannot be represented in XMP for PDF/A1, reverting to normal PDF output\n");
                        pdev->AbortPDFAX = true;
                        pdev->PDFA = 0;
                        break;
                    case 1:
                        emprintf(pdev->memory,
                         "Text string detected in DOCINFO cannot be represented in XMP for PDF/A1, discarding DOCINFO\n");
                        continue;
                        break;
                    case 2:
                        emprintf(pdev->memory,
                         "Text string detected in DOCINFO cannot be represented in XMP for PDF/A1, aborting conversion.\n");
                        /* If we don't return a fatal error then zputdeviceparams simply ignores it (!) */
                        return_error(gs_error_Fatal);
                        break;
                    default:
                        break;
                }
            }
        }
        if (pdf_key_eq(pairs + i, "/Producer")) {
            string_match_params params;
            params = string_match_params_default;
            params.ignore_case = true;

            if (!string_match((const byte *)GS_PRODUCTFAMILY, strlen(GS_PRODUCTFAMILY), (const byte *)"GPL Ghostscript", 15, &params))
                code = pdfmark_put_pair(pcd, pair);
        } else
            code = pdfmark_put_pair(pcd, pair);
        if (code < 0)
            break;
    }
    return code;
}

/* DOCVIEW pdfmark */
static int
pdfmark_DOCVIEW(gx_device_pdf * pdev, gs_param_string * pairs, uint count,
                const gs_matrix * pctm, const gs_param_string * no_objname)
{
    char dest[MAX_DEST_STRING];
    int code = 0;

    if (count & 1)
        return_error(gs_error_rangecheck);
    code = pdfmark_make_dest(dest, pdev, "/Page", "/View", pairs, count, 0);
    if (code < 0)
        return gs_note_error(gs_error_rangecheck);

    if (code > 0) {
        int i;

        code = cos_dict_put_c_key_string(pdev->Catalog, "/OpenAction",
                                         (byte *)dest, strlen(dest));
        for (i = 0; code >= 0 && i < count; i += 2)
            if (!(pdf_key_eq(&pairs[i], "/Page") ||
                  pdf_key_eq(&pairs[i], "/View"))
                )
                code = pdfmark_put_pair(pdev->Catalog, pairs + i);
        return code;
    } else
        return pdfmark_put_pairs(pdev->Catalog, pairs, count);
}

/* ---------------- Named object pdfmarks ---------------- */

/* [ /BBox [llx lly urx ury] /_objdef {obj} /BP pdfmark */
static int
pdfmark_BP(gx_device_pdf * pdev, gs_param_string * pairs, uint count,
           const gs_matrix * pctm, const gs_param_string * objname)
{
    gs_rect bbox;
    cos_stream_t *pcs;
    int code;
    gs_matrix ictm;
    byte bbox_str[6 + 6 * 15], matrix_str[6 + 6 * 15];
    char chars[MAX_RECT_STRING + 1];
    int bbox_str_len, matrix_str_len;
    stream s;

    if (objname == 0 || count != 2 || !pdf_key_eq(&pairs[0], "/BBox"))
        return_error(gs_error_rangecheck);
    code = gs_matrix_invert(pctm, &ictm);
    if (code < 0)
        return code;
    if (pairs[1].size > MAX_RECT_STRING)
        return_error(gs_error_limitcheck);
    memcpy(chars, pairs[1].data, pairs[1].size);
    chars[pairs[1].size] = 0;
    if (sscanf(chars, "[%lg %lg %lg %lg]",
               &bbox.p.x, &bbox.p.y, &bbox.q.x, &bbox.q.y) != 4)
        return_error(gs_error_rangecheck);
    if ((pdev->used_mask << 1) == 0)
        return_error(gs_error_limitcheck);
    code = start_XObject(pdev, pdev->params.CompressPages, &pcs);
    if (code < 0)
        return code;
    {	byte *s = gs_alloc_string(pdev->memory, objname->size, "pdfmark_PS");

        if (s == NULL)
            return_error(gs_error_VMerror);
        memcpy(s, objname->data, objname->size);
        pdev->objname.data = s;
        pdev->objname.size = objname->size;
    }
    pcs->is_graphics = true;
    gs_bbox_transform(&bbox, pctm, &bbox);
    s_init(&s, NULL);
    swrite_string(&s, bbox_str, sizeof(bbox_str));
    pprintg4(&s, "[%g %g %g %g]",
            bbox.p.x, bbox.p.y, bbox.q.x, bbox.q.y);
    bbox_str_len = stell(&s);
    swrite_string(&s, matrix_str, sizeof(bbox_str));
    pprintg6(&s, "[%g %g %g %g %g %g]",
            ictm.xx, ictm.xy, ictm.yx, ictm.yy, ictm.tx, ictm.ty);
    matrix_str_len = stell(&s);
    if ((code = cos_stream_put_c_strings(pcs, "/Type", "/XObject")) < 0 ||
        (code = cos_stream_put_c_strings(pcs, "/Subtype", "/Form")) < 0 ||
        (code = cos_stream_put_c_strings(pcs, "/FormType", "1")) < 0 ||
        (code = cos_dict_put_c_key_string(cos_stream_dict(pcs), "/BBox",
                                          bbox_str, bbox_str_len)) < 0 ||
        (code = cos_dict_put_c_key_string(cos_stream_dict(pcs), "/Matrix",
                                      matrix_str, matrix_str_len)) < 0 ||
        (code = cos_dict_put_c_key_object(cos_stream_dict(pcs), "/Resources",
                                          COS_OBJECT(pdev->substream_Resources))) < 0
        )
        return code;
    /* Don't add to local_named_objects untill the object is created
       to prevent pending references, which may appear
       if /PUT pdfmark executes before pdf_substitute_resource in pdfmark_EP
       drops this object.
    */
    pdev->FormDepth++;
    return 0;
}

/* [ /EP pdfmark */
static int
pdfmark_EP(gx_device_pdf * pdev, gs_param_string * pairs, uint count,
           const gs_matrix * pctm, const gs_param_string * no_objname)
{
    int code;
    pdf_resource_t *pres = pdev->accumulating_substream_resource;
    gs_const_string objname = pdev->objname;

    /* We are not currently accumulating a resource, this suggests an /EP
     * without an opening /BP. This is (obviously) an error, Distiller throws
     * an 'undefined' error, so we will too.
     */
    if (pres == NULL)
        return_error(gs_error_undefined);

    if (pdev->CompatibilityLevel <= 1.7) {
        code = pdf_add_procsets(pdev->substream_Resources, pdev->procsets);
        if (code < 0)
            return code;
    }
    code = pdf_exit_substream(pdev);
    if (code < 0)
        return code;
    code = pdfmark_bind_named_object(pdev, &objname, &pres);
    if (code < 0)
        return 0;
    gs_free_const_string(pdev->memory, objname.data, objname.size, "pdfmark_EP");
    pdev->FormDepth--;
    return 0;
}

/* [ {obj} /SP pdfmark */
static int
pdfmark_SP(gx_device_pdf * pdev, gs_param_string * pairs, uint count,
           const gs_matrix * pctm, const gs_param_string * no_objname)
{
    cos_object_t *pco;		/* stream */
    int code;

    if (count != 1)
        return_error(gs_error_rangecheck);
    if ((code = pdf_get_named(pdev, &pairs[0], cos_type_stream, &pco)) < 0)
        return code;
    if (pco->is_open || !pco->is_graphics)
        return_error(gs_error_rangecheck);
    code = pdf_open_contents(pdev, PDF_IN_STREAM);
    if (code < 0)
        return code;
    pdf_put_matrix(pdev, "q ", pctm, "cm");
    pprinti64d1(pdev->strm, "/R%"PRId64" Do Q\n", pco->id);
    pco->pres->where_used |= pdev->used_mask;

    code = pdf_add_resource(pdev, pdev->substream_Resources, "/XObject", pco->pres);
    if (code < 0)
        return code;

    return 0;
}

/* [ /_objdef {array} /type /array /OBJ pdfmark */
/* [ /_objdef {dict} /type /dict /OBJ pdfmark */
/* [ /_objdef {stream} /type /stream /OBJ pdfmark */
static int
pdfmark_OBJ(gx_device_pdf * pdev, gs_param_string * pairs, uint count,
            const gs_matrix * pctm, const gs_param_string * objname)
{
    cos_type_t cotype;
    cos_object_t *pco;
    bool stream = false;
    int code;

    if (objname == 0 || count != 2 || !pdf_key_eq(&pairs[0], "/type"))
        return_error(gs_error_rangecheck);
    if (pdf_key_eq(&pairs[1], "/array"))
        cotype = cos_type_array;
    else if (pdf_key_eq(&pairs[1], "/dict"))
        cotype = cos_type_dict;
    else if ((stream = pdf_key_eq(&pairs[1], "/stream")))
        cotype = cos_type_stream;
    else
        return_error(gs_error_rangecheck);
    if ((code = pdf_make_named(pdev, objname, cotype, &pco, true)) < 0) {
        /*
         * For Distiller compatibility, allows multiple /OBJ pdfmarks with
         * the same name and type, even though the pdfmark specification
         * doesn't say anything about this being legal.
         */
        if (code == gs_error_rangecheck &&
            pdf_refer_named(pdev, objname, &pco) >= 0 &&
            cos_type(pco) == cotype
            )
            return 0;		/* already exists, but OK */
        return code;
    }
    if (stream) {
        if (pdev->CompressStreams)
            return setup_pdfmark_stream_compression((gx_device_psdf *)pdev,
                                                     (cos_stream_t *)pco);
        else
            return setup_pdfmark_stream_no_compression((gx_device_psdf *)pdev,
                                                     (cos_stream_t *)pco);
    }
    return 0;
}

/* [ {array} index value /PUT pdfmark */
/* Dictionaries are converted to .PUTDICT */
/* Streams are converted to .PUTSTREAM */
static int
pdfmark_PUT(gx_device_pdf * pdev, gs_param_string * pairs, uint count,
            const gs_matrix * pctm, const gs_param_string * no_objname)
{
    cos_object_t *pco;
    cos_value_t value;
    int code, index;

    if (count != 3)
        return_error(gs_error_rangecheck);
    if ((code = pdf_get_named(pdev, &pairs[0], cos_type_array, &pco)) < 0)
        return code;
    if ((code = pdfmark_scan_int(&pairs[1], &index)) < 0)
        return code;
    if (index < 0)
        return_error(gs_error_rangecheck);
    if (pco->written)
        return_error(gs_error_rangecheck);
    return cos_array_put((cos_array_t *)pco, index,
                cos_string_value(&value, pairs[2].data, pairs[2].size));
}

/* [ {dict} key value ... /.PUTDICT pdfmark */
/* [ {stream} key value ... /.PUTDICT pdfmark */
/*
 * Adobe's pdfmark documentation doesn't allow PUTDICT with a stream,
 * but it's reasonable and unambiguous, and Acrobat Distiller accepts it,
 * so we do too.
 */
static int
pdfmark_PUTDICT(gx_device_pdf * pdev, gs_param_string * pairs, uint count,
                const gs_matrix * pctm, const gs_param_string * no_objname)
{
    cos_object_t *pco;
    int code, i;

    if ((code = pdf_refer_named(pdev, &pairs[0], &pco)) < 0)
        return code;
    if (cos_type(pco) != cos_type_dict && cos_type(pco) != cos_type_stream)
        return_error(gs_error_typecheck);
    if (pco->written)
        return_error(gs_error_rangecheck);

    /* If this is a stream, and we are doing PDF/A output, and the stream
     * is a Metadata stream, then we must not write it compressed. Bizarrely PDF/A
     * excludes this.
     * Actually, we cna't write Metadata streams at all. Because we don't know how to extend
     * the XMP Metadata to include them.
     */
    if (cos_type(pco) == cos_type_stream && pdev->PDFA) {
        for (i=0;i<count;i++) {
            if (pairs[i].size == 9 && strncmp((const char *)pairs[i].data, "/Metadata", 9) == 0) {
                cos_dict_t *pcd = (cos_dict_t *)pco;

                if (pdev->PDFA) {
                    switch (pdev->PDFACompatibilityPolicy) {
                        case 0:
                            emprintf(pdev->memory,
                                     "Cannot preserve Marked Content in PDF/A, reverting to normal PDF output\n\n");
                            pdev->AbortPDFAX = true;
                            pdev->PDFA = 0;
                            break;
                        case 1:
                            emprintf(pdev->memory,
                                     "Cannot preserve Marked Content in PDF/A, dropping feature to preserve PDF/A compatibility\n");
                            return 0;
                            break;
                        case 2:
                            emprintf(pdev->memory,
                                     "Cannot preserve Marked Content in PDF/A, aborting conversion\n");
                            return_error (gs_error_typecheck);
                            break;
                        default:
                            emprintf(pdev->memory,
                                     "Cannot preserve Marked Content in PDF/A, unrecognised PDFACompatibilityLevel,\nreverting to normal PDF output\n");
                            pdev->AbortPDFAX = true;
                            pdev->PDFA = 0;
                            break;
                    }
                }

                /* Close the compressed stream */
                gs_free_object(pdev->pdf_memory, pco->input_strm, "free old stream, replacing with new stream");
                /* And create a new uncompressed stream */
                code = setup_pdfmark_stream_no_compression((gx_device_psdf *)pdev,
                                                     (cos_stream_t *)pco);
                if (code < 0)
                    return code;

                /* We also need to remove any compression filters from the stream dictionary
                 * The only possible error here is that the key isn't found, which is possible
                 * and we don't care, so ignore the return code.
                 */
                cos_dict_delete_c_key(pcd, "/Filter");
                cos_dict_delete_c_key(pcd, "/DecodeParams");

            }
        }
    }
    return pdfmark_put_pairs((cos_dict_t *)pco, pairs + 1, count - 1);
}

/* [ {stream} string ... /.PUTSTREAM pdfmark */
static int
pdfmark_PUTSTREAM(gx_device_pdf * pdev, gs_param_string * pairs, uint count,
                  const gs_matrix * pctm, const gs_param_string * no_objname)
{
    cos_object_t *pco;
    int code, i;
    uint l;

    if (count < 2)
        return_error(gs_error_rangecheck);
    if ((code = pdf_get_named(pdev, &pairs[0], cos_type_stream, &pco)) < 0)
        return code;
    if (!pco->is_open) {
        pdev->pdfmark_dup_stream = true;
        return 0;
    }
    for (i = 1; i < count; ++i)
        if (sputs(pco->input_strm, pairs[i].data, pairs[i].size, &l) != 0)
            return_error(gs_error_ioerror);
    if (pco->written)
        return_error(gs_error_rangecheck);
    pdev->pdfmark_dup_stream = false;
    return code;
}

/* [ {array} value /APPEND pdfmark */
static int
pdfmark_APPEND(gx_device_pdf * pdev, gs_param_string * pairs, uint count,
               const gs_matrix * pctm, const gs_param_string * objname)
{
    cos_object_t *pco;
    cos_value_t value;
    int code;

    if (count != 2)
        return_error(gs_error_rangecheck);
    if ((code = pdf_get_named(pdev, &pairs[0], cos_type_array, &pco)) < 0)
        return code;
    return cos_array_add((cos_array_t *)pco,
                cos_string_value(&value, pairs[1].data, pairs[1].size));
}

/* [ {array} index value ... /.PUTINTERVAL pdfmark */
static int
pdfmark_PUTINTERVAL(gx_device_pdf * pdev, gs_param_string * pairs, uint count,
                 const gs_matrix * pctm, const gs_param_string * no_objname)
{
    cos_object_t *pco;
    cos_value_t value;
    int code, index, i;

    if (count < 2)
        return_error(gs_error_rangecheck);
    if ((code = pdf_get_named(pdev, &pairs[0], cos_type_array, &pco)) < 0)
        return code;
    if ((code = pdfmark_scan_int(&pairs[1], &index)) < 0)
        return code;
    if (index < 0)
        return_error(gs_error_rangecheck);
    for (i = 2; code >= 0 && i < count; ++i)
        code = cos_array_put((cos_array_t *)pco, index + i - 2,
                cos_string_value(&value, pairs[i].data, pairs[i].size));
    return code;
}

/* [ {stream} /CLOSE pdfmark */
static int
pdfmark_CLOSE(gx_device_pdf * pdev, gs_param_string * pairs, uint count,
              const gs_matrix * pctm, const gs_param_string * no_objname)
{
    cos_object_t *pco;
    int code;

    if (count != 1)
        return_error(gs_error_rangecheck);
    if ((code = pdf_get_named(pdev, &pairs[0], cos_type_stream, &pco)) < 0)
        return code;
    if (!pco->is_open && !pdev->pdfmark_dup_stream)
        return_error(gs_error_rangecheck);
    pdev->pdfmark_dup_stream = false;
    /* Currently we don't do anything special when closing a stream. */
    pco->is_open = false;
    return 0;
}

/* [ /NamespacePush pdfmark */
static int
pdfmark_NamespacePush(gx_device_pdf *pdev, gs_param_string *pairs, uint count,
                      const gs_matrix *pctm, const gs_param_string *objname)
{
    if (count != 0)
        return_error(gs_error_rangecheck);
    return pdf_push_namespace(pdev);
}

/* [ /NamespacePop pdfmark */
static int
pdfmark_NamespacePop(gx_device_pdf *pdev, gs_param_string *pairs, uint count,
                     const gs_matrix *pctm, const gs_param_string *objname)
{
    if (count != 0)
        return_error(gs_error_rangecheck);
    cos_dict_objects_write(pdev->local_named_objects, pdev);
    return pdf_pop_namespace(pdev);
}

/* [ /_objdef {image} /NI pdfmark */
static int
pdfmark_NI(gx_device_pdf *pdev, gs_param_string *pairs, uint count,
           const gs_matrix *pctm, const gs_param_string *objname)
{
    cos_object_t *pco;
    int code;

    if (objname == 0 || count != 0)
        return_error(gs_error_rangecheck);
    code = pdf_make_named(pdev, objname, cos_type_dict, &pco, true);
    if (code < 0)
        return code;
    return cos_array_add_object(pdev->NI_stack, pco);
}

/* ---------------- Named content pdfmarks ---------------- */

/* [ tag /MP pdfmark */
static int
pdfmark_MP(gx_device_pdf *pdev, gs_param_string *pairs, uint count,
           const gs_matrix *pctm, const gs_param_string *objname)
{
    int code;
    char *tag;

    if (count != 1) return_error(gs_error_rangecheck);

    tag = (char *)gs_alloc_bytes(pdev->memory, (pairs[0].size + 1) * sizeof(unsigned char),
                "pdfmark_MP");
    memcpy(tag, pairs[0].data, pairs[0].size);
    tag[pairs[0].size] = 0x00;

    code = pdf_open_contents(pdev, PDF_IN_STREAM);
    if (code < 0) return code;

    pprints1(pdev->strm, "%s MP\n", tag);

    gs_free_object(pdev->memory, tag, "pdfmark_MP");
    return 0;
}

/* [ tag propdict /DP pdfmark */
static int
pdfmark_DP(gx_device_pdf *pdev, gs_param_string *pairs, uint count,
           const gs_matrix *pctm, const gs_param_string *objname)
{
    int code;
    cos_object_t *pco;
    char *cstring;
    pdf_resource_t *pres;

    if (count != 2) return_error(gs_error_rangecheck);

    /* check tag for /Name object syntax */
    if ((pairs[0].data)[0] != '/') return_error(gs_error_rangecheck);

    /* check propdict for {object name} syntax */
    if (pdf_objname_is_valid(pairs[1].data, pairs[1].size))
    {
        code = pdf_refer_named(pdev, &pairs[1], &pco);
        if(code < 0) return code;
    }
    else /* << inline prop dict >> */
    {
        /* strip << and >> */
        if ((pairs[1].data)[0]=='<'&&(pairs[1].data)[1]=='<')
        {
            int ix = 0;
            byte *p = (byte *)pairs[1].data;

            /* Fortunately we don't use the 'size' when freeing the memory
             * so we can quickly copy the string content up two places and reduce
             * the size by 2 to remove the '<<'. This saves an alloc and free of the
             * string data.
             */
            for (ix = 0; ix < pairs[1].size - 2;ix++)
                p[ix] = pairs[1].data[ix + 2];
            pairs[1].size-=2;
        }
        else
            return_error(gs_error_rangecheck);

        if ((pairs[1].data)[pairs[1].size-1]=='>'&&(pairs[1].data)[pairs[1].size-2]=='>')
            pairs[1].size-=2;

        /* convert inline propdict to C string with object names replaced by refs */
        code = pdf_replace_names(pdev, &pairs[1], &pairs[1]);
        if (code<0) return code;
        cstring = (char *)gs_alloc_bytes(pdev->memory, (pairs[1].size + 1) * sizeof(unsigned char),
            "pdfmark_DP");
        memcpy(cstring, pairs[1].data, pairs[1].size);
        cstring[pairs[1].size] = 0x00;

        code = pdf_make_named_dict(pdev, NULL, (cos_dict_t**) &pco, true);
        if (code<0) return code;

        /* copy inline propdict to new object */
        code = cos_dict_put_c_strings((cos_dict_t*) pco, cstring, "");
        if(code < 0) return code;
        COS_WRITE_OBJECT(pco, pdev, resourceProperties);
        COS_RELEASE(pco, "pdfmark_DP");
        gs_free_object(pdev->memory, cstring, "pdfmark_DP");
    }

    pres = pdf_find_resource_by_resource_id(pdev, resourceProperties, pco->id);
    if (pres==0){
        if ((code = pdf_alloc_resource(pdev, resourceProperties, pco->id, &(pco->pres), pco->id))<0)
            return code;
    }

    cstring = (char *)gs_alloc_bytes(pdev->memory, (pairs[0].size + 1) * sizeof(unsigned char),
                "pdfmark_DP");
    memcpy(cstring, pairs[0].data, pairs[0].size);
    cstring[pairs[0].size] = 0x00;

    /* make sure we write to the correct stream */
    code = pdf_open_contents(pdev, PDF_IN_STREAM);
    if (code < 0) return code;

    pprints1(pdev->strm, "%s", cstring); /* write tag */
    pprinti64d1(pdev->strm, "/R%"PRId64" DP\n", pco->id);
    pco->pres->where_used |= pdev->used_mask;
    if ((code = pdf_add_resource(pdev, pdev->substream_Resources, "/Properties", pco->pres))<0)
        return code;

    gs_free_object(pdev->memory, cstring, "pdfmark_DP");
    return 0;
}

/* [ tag /BMC pdfmark */
static int
pdfmark_BMC(gx_device_pdf *pdev, gs_param_string *pairs, uint count,
            const gs_matrix *pctm, const gs_param_string *objname)
{
    int code;
    char *tag;

    if (count != 1) return_error(gs_error_rangecheck);

    tag = (char *)gs_alloc_bytes(pdev->memory, (pairs[0].size + 1) * sizeof(unsigned char),
                "pdfmark_BMC");
    memcpy(tag, pairs[0].data, pairs[0].size);
    tag[pairs[0].size] = 0x00;

    code = pdf_open_contents(pdev, PDF_IN_STREAM);
    if (code < 0) return code;

    pprints1(pdev->strm, "%s BMC\n", tag);

    gs_free_object(pdev->memory, tag, "pdfmark_BMC");
    return 0;
}

/* [ tag propdict /BDC pdfmark */
static int
pdfmark_BDC(gx_device_pdf *pdev, gs_param_string *pairs, uint count,
            const gs_matrix *pctm, const gs_param_string *objname)
{
    int code, id = 0, i, esc_size = 0;
    cos_object_t *pco = NULL;
    char *cstring;
    pdf_resource_t *pres;

    if (count != 2) return_error(gs_error_rangecheck);

    /* check tag for /Name object syntax */
    if ((pairs[0].data)[0] != '/') return_error(gs_error_rangecheck);

    /* Check for /OC (Optional Content), if it is Optional Content, check the output PDF level is at least 1.5 */
    if (pairs[0].size == 3 && memcmp(pairs[0].data, "/OC", 3) == 0) {
        if (pdev->CompatibilityLevel < 1.4999) {
            if (pdev->PDFA) {
                switch (pdev->PDFACompatibilityPolicy) {
                    case 0:
                        emprintf(pdev->memory,
                                 "Optional (Marked) Content not valid in this version of PDF, reverting to normal PDF output\n\n");
                        pdev->AbortPDFAX = true;
                        pdev->PDFA = 0;
                        break;
                    case 1:
                        emprintf(pdev->memory,
                                 "Optional (Marked) Content not valid in this version of PDF. Dropping feature to preserve PDF/A compatibility\n");
                        return 0;
                        break;
                    case 2:
                        emprintf(pdev->memory,
                                 "Optional (Marked) Content not valid in this version of PDF,  aborting conversion\n");
                        return_error (gs_error_typecheck);
                        break;
                    default:
                        emprintf(pdev->memory,
                                 "Optional (Marked) Content not valid in this version of PDF, unrecognised PDFACompatibilityLevel,\nreverting to normal PDF output\n");
                        pdev->AbortPDFAX = true;
                        pdev->PDFA = 0;
                        break;
                }
            } else {
                emprintf(pdev->memory,
                         "Optional (Marked) Content not valid in this version of PDF. Dropping feature to preserve compatibility\n");
                return 0;
            }
        }
    }

    /* check propdict for {object name} syntax */
    if (pdf_objname_is_valid(pairs[1].data, pairs[1].size))
    {
        code = pdf_refer_named(pdev, &pairs[1], &pco);
        if(code < 0) return code;
        id = pco->id;
    }
    else /* << inline prop dict >> */
    {
        /* strip << and >> */
        if ((pairs[1].data)[0]=='<'&&(pairs[1].data)[1]=='<')
        {
            int ix = 0;
            byte *p = (byte *)pairs[1].data;

            /* Fortunately we don't use the 'size' when freeing the memory
             * so we can quickly copy the string content up two places and reduce
             * the size by 2 to remove the '<<'. This saves an alloc and free of the
             * string data.
             */
            for (ix = 0; ix < pairs[1].size - 2;ix++)
                p[ix] = pairs[1].data[ix + 2];
            pairs[1].size-=2;

            if ((pairs[1].data)[pairs[1].size-1]=='>'&&(pairs[1].data)[pairs[1].size-2]=='>')
            pairs[1].size-=2;

            /* convert inline propdict to C string with object names replaced by refs */
            code = pdf_replace_names(pdev, &pairs[1], &pairs[1]);
            if (code<0) return code;
            cstring = (char *)gs_alloc_bytes(pdev->memory, (pairs[1].size + 1) * sizeof(unsigned char),
                "pdfmark_BDC");
            memcpy(cstring, pairs[1].data, pairs[1].size);
            cstring[pairs[1].size] = 0x00;

            code = pdf_make_named_dict(pdev, NULL, (cos_dict_t**) &pco, true);
            if (code<0) return code;

            /* copy inline propdict to new object */
            code = cos_dict_put_c_strings((cos_dict_t*) pco, cstring, "");
            if(code < 0) return code;

            COS_WRITE_OBJECT(pco, pdev, resourceProperties);

            COS_RELEASE(pco, "pdfmark_BDC");
            gs_free_object(pdev->memory, cstring, "pdfmark_BDC");
            id = pco->id;
        }
        else {
            if ((pairs[1].data)[pairs[1].size-1]!='R') {
                if ((pairs[1].data)[pairs[1].size-2]==' ' && (pairs[1].data)[pairs[1].size-1]!='R')
                    return_error(gs_error_rangecheck);
                if (sscanf((const char *)pairs[1].data, "%d 0 R", &id) != 1)
                    return_error(gs_error_unknownerror);
            }

        }
    }

    pres = pdf_find_resource_by_resource_id(pdev, resourceProperties, id);
    if (pres==0){
        if (pco != NULL) {
            if ((code = pdf_alloc_resource(pdev, resourceProperties, pco->id, &(pco->pres), pco->id))<0) {
                return code;
            }
        }
        else
            if ((code = pdf_alloc_resource(pdev, resourceProperties, id, &pres, id))<0)
                return code;
    }

    /* We need to make sure we escape any white space in the tag */
    for (i = 0;i < pairs[0].size;i++) {
        switch(pairs[0].data[i]) {
            case 0x00:
            case 0x09:
            case 0x0a:
            case 0x0c:
            case 0x0d:
            case 0x20:
                esc_size += 3;
                break;
            default:
                esc_size++;
                break;
        }
    }

    cstring = (char *)gs_alloc_bytes(pdev->memory, (esc_size + 1) * sizeof(unsigned char),
                "pdfmark_BDC");
    esc_size = 0;
    for (i = 0;i < pairs[0].size;i++) {
        switch(pairs[0].data[i]) {
            case 0x00:
            case 0x09:
            case 0x0a:
            case 0x0c:
            case 0x0d:
            case 0x20:
                cstring[esc_size++] = '#';
                cstring[esc_size++] = (pairs[0].data[i] >> 4) + 0x30;
                cstring[esc_size++] = (pairs[0].data[i] & 0x0f) + 0x30;
                break;
            default:
                cstring[esc_size++] = pairs[0].data[i];
                break;
        }
    }
    cstring[esc_size] = 0x00;

    /* make sure we write to the correct stream */
    code = pdf_open_contents(pdev, PDF_IN_STREAM);
    if (code < 0) return code;

    pprints1(pdev->strm, "%s", cstring); /* write tag */
    pprinti64d1(pdev->strm, "/R%"PRId64" BDC\n", id);
    if (pco != NULL) {
        pco->pres->where_used |= pdev->used_mask;
        if ((code = pdf_add_resource(pdev, pdev->substream_Resources, "/Properties", pco->pres))<0)
            return code;
    }
    else {
        pres->where_used |= pdev->used_mask;
        if ((code = pdf_add_resource(pdev, pdev->substream_Resources, "/Properties", pres))<0)
            return code;
    }


    gs_free_object(pdev->memory, cstring, "pdfmark_BDC");
    return 0;
}

/* [ /EMC pdfmark */
static int
pdfmark_EMC(gx_device_pdf *pdev, gs_param_string *pairs, uint count,
            const gs_matrix *pctm, const gs_param_string *objname)
{
    int code;

    code = pdf_open_contents(pdev, PDF_IN_STREAM);
    if (code < 0) return code;
    stream_puts(pdev->strm, "EMC\n");

    return 0;
}

/* ---------------- Document structure pdfmarks ---------------- */

/* [ newsubtype1 stdsubtype1 ... /StRoleMap pdfmark */
static int
pdfmark_StRoleMap(gx_device_pdf *pdev, gs_param_string *pairs, uint count,
                  const gs_matrix *pctm, const gs_param_string *objname)
{
    return 0;			/****** NOT IMPLEMENTED YET ******/
}

/* [ class1 {attrobj1} ... /StClassMap pdfmark */
static int
pdfmark_StClassMap(gx_device_pdf *pdev, gs_param_string *pairs, uint count,
                   const gs_matrix *pctm, const gs_param_string *objname)
{
    return 0;			/****** NOT IMPLEMENTED YET ******/
}

/*
 * [ [/_objdef {objname}] /Subtype name [/Title string] [/Alt string]
 *   [/ID string] [/Class name] [/At index] [/Bookmark dict] [action_pairs...]
 *   /StPNE pdfmark
 */
static int
pdfmark_StPNE(gx_device_pdf *pdev, gs_param_string *pairs, uint count,
              const gs_matrix *pctm, const gs_param_string *objname)
{
    return 0;			/****** NOT IMPLEMENTED YET ******/
}

/* [ [/Title string] [/Open bool] [action_pairs...] /StBookmarkRoot pdfmark */
static int
pdfmark_StBookmarkRoot(gx_device_pdf *pdev, gs_param_string *pairs, uint count,
                       const gs_matrix *pctm, const gs_param_string *objname)
{
    return 0;			/****** NOT IMPLEMENTED YET ******/
}

/* [ [/E {elt}] /StPush pdfmark */
static int
pdfmark_StPush(gx_device_pdf *pdev, gs_param_string *pairs, uint count,
               const gs_matrix *pctm, const gs_param_string *objname)
{
    return 0;			/****** NOT IMPLEMENTED YET ******/
}

/* [ /StPop pdfmark */
static int
pdfmark_StPop(gx_device_pdf *pdev, gs_param_string *pairs, uint count,
              const gs_matrix *pctm, const gs_param_string *objname)
{
    return 0;			/****** NOT IMPLEMENTED YET ******/
}

/* [ /StPopAll pdfmark */
static int
pdfmark_StPopAll(gx_device_pdf *pdev, gs_param_string *pairs, uint count,
                 const gs_matrix *pctm, const gs_param_string *objname)
{
    return 0;			/****** NOT IMPLEMENTED YET ******/
}

/* [ [/T tagname] [/At index] /StBMC pdfmark */
static int
pdfmark_StBMC(gx_device_pdf *pdev, gs_param_string *pairs, uint count,
              const gs_matrix *pctm, const gs_param_string *objname)
{
    return 0;			/****** NOT IMPLEMENTED YET ******/
}

/* [ [/P propdict] [/T tagname] [/At index] /StBDC pdfmark */
static int
pdfmark_StBDC(gx_device_pdf *pdev, gs_param_string *pairs, uint count,
              const gs_matrix *pctm, const gs_param_string *objname)
{
    return 0;			/****** NOT IMPLEMENTED YET ******/
}

/* [ /Obj {obj} [/At index] /StOBJ pdfmark */
static int
pdfmark_StOBJ(gx_device_pdf *pdev, gs_param_string *pairs, uint count,
              const gs_matrix *pctm, const gs_param_string *objname)
{
    return 0;			/****** NOT IMPLEMENTED YET ******/
}

/* [ /Obj {obj} /StAttr pdfmark */
static int
pdfmark_StAttr(gx_device_pdf *pdev, gs_param_string *pairs, uint count,
               const gs_matrix *pctm, const gs_param_string *objname)
{
    return 0;			/****** NOT IMPLEMENTED YET ******/
}

/* [ /StoreName name /StStore pdfmark */
static int
pdfmark_StStore(gx_device_pdf *pdev, gs_param_string *pairs, uint count,
                const gs_matrix *pctm, const gs_param_string *objname)
{
    return 0;			/****** NOT IMPLEMENTED YET ******/
}

/* [ /StoreName name /StRetrieve pdfmark */
static int
pdfmark_StRetrieve(gx_device_pdf *pdev, gs_param_string *pairs, uint count,
                   const gs_matrix *pctm, const gs_param_string *objname)
{
    return 0;			/****** NOT IMPLEMENTED YET ******/
}

static int
pdfmark_Metadata(gx_device_pdf * pdev, gs_param_string * pairs, uint count,
             const gs_matrix * pctm, const gs_param_string * objname)
{
    int i;
    gs_param_string key;
    char data[10] = "/Metadata";

    if (pdev->CompatibilityLevel < 1.4) {
        dmprintf(pdev->pdf_memory, "Cannot add Metadata to PDF files with version earlier than 1.4.\n");
        return 0;
    }
    if (pdev->PDFA != 0)
        dmprintf(pdev->pdf_memory, "Warning: PDF/A output requires specific metadata, this pdfmark has overridden that,\n         output conformance cannot be guaranteed\n");
    if (pdev->PDFX != 0)
        dmprintf(pdev->pdf_memory, "Warning: PDF/X output requires specific metadata, this pdfmark has overridden that,\n         output conformance cannot be guaranteed\n");

    if(pdev->ExtensionMetadata) {
        dmprintf(pdev->pdf_memory, "Extension metadata exists when /Metadata pdfmark executed, discarding extension metadata.\n");
        gs_free_object(pdev->pdf_memory->stable_memory, pdev->ExtensionMetadata, "Extension metadata discarded on /Metadata pdfmark");
    }

    if (!pdev->Catalog) {
        gs_param_string nstr;

        param_string_from_string(nstr, "{Catalog}");
        pdf_create_named_dict(pdev, &nstr, &pdev->Catalog, 0L);
    }

    key.data = (const byte *)&data;
    key.size = 9;

    for (i = 0; i < count; i += 2) {
        if (pdf_key_eq(&pairs[i], "{Catalog}")) {
            return cos_dict_put_string(pdev->Catalog, key.data, key.size,
                               pairs[i+1].data, pairs[i+1].size);
        }
    }
    return 0;
}

static int
pdfmark_Ext_Metadata(gx_device_pdf * pdev, gs_param_string * pairs, uint count,
             const gs_matrix * pctm, const gs_param_string * objname)
{
    int i, j=0;

    if (pdev->CompatibilityLevel < 1.4) {
        dmprintf(pdev->pdf_memory, "Cannot add Metadata to PDF files with version earlier than 1.4.\n");
        return 0;
    }

    if (cos_dict_find_c_key(pdev->Catalog, "/Metadata")) {
        dmprintf(pdev->pdf_memory, "Cannot add extension to Metadata specified with the /Metadata pdfmark\n");
        return 0;
    }

    if(pdev->ExtensionMetadata) {
        dmprintf(pdev->pdf_memory, "Extension metadata already defined, discarding old data.\n");
        gs_free_object(pdev->pdf_memory->stable_memory, pdev->ExtensionMetadata, "Extension metadata");
    }
    pdev->ExtensionMetadata = (char *)gs_alloc_bytes(pdev->pdf_memory->stable_memory, pairs[1].size - 1, "Extension metadata");
    memset(pdev->ExtensionMetadata, 0x00, pairs[1].size - 1);
    for (i=1;i<pairs[1].size - 1;i++) {
        if (pairs[1].data[i] == '\\') {
            switch(pairs[1].data[i+1]) {
                case '(':
                case ')':
                case '\\':
                    pdev->ExtensionMetadata[j++] = pairs[1].data[i+1];
                    i++;
                    break;
                case 'r':
                    pdev->ExtensionMetadata[j++] = 0x0D;
                    i++;
                    break;
                case 'n':
                    pdev->ExtensionMetadata[j++] = 0x0A;
                    i++;
                    break;
                case 't':
                    pdev->ExtensionMetadata[j++] = 0x09;
                    i++;
                    break;
                case 'b':
                    pdev->ExtensionMetadata[j++] = 0x08;
                    i++;
                    break;
                case 'f':
                    pdev->ExtensionMetadata[j++] = 0x0C;
                    i++;
                    break;
                default:
                    if((pairs[1].data[i+1]) >= 0x30 && (pairs[1].data[i+1]) <= 0x39) {
                        pdev->ExtensionMetadata[j++] = (pairs[1].data[i+1] - 0x30) * 64 + (pairs[1].data[i+2] - 0x30) * 8 + (pairs[1].data[i+3] - 0x30);
                        i += 3;
                    } else
                        pdev->ExtensionMetadata[j++] = pairs[1].data[i];
                    break;
            }
        } else
            pdev->ExtensionMetadata[j++] = pairs[1].data[i];
    }
    return 0;
}

static int
pdfmark_OCProperties(gx_device_pdf * pdev, gs_param_string * pairs, uint count,
             const gs_matrix * pctm, const gs_param_string * objname)
{
    char *str;

    if (pdev->CompatibilityLevel < 1.4999) {
        if (pdev->PDFA) {
            switch (pdev->PDFACompatibilityPolicy) {
                case 0:
                    emprintf(pdev->memory,
                             "Optional Content Properties not valid in this version of PDF, reverting to normal PDF output\n\n");
                    pdev->AbortPDFAX = true;
                    pdev->PDFA = 0;
                    break;
                case 1:
                    emprintf(pdev->memory,
                             "Optional Content Properties not valid in this version of PDF. Dropping feature to preserve PDF/A compatibility\n");
                    break;
                case 2:
                    emprintf(pdev->memory,
                             "Optional Content Properties not valid in this version of PDF,  aborting conversion\n");
                    return_error (gs_error_typecheck);
                    break;
                default:
                    emprintf(pdev->memory,
                             "Optional Content Properties not valid in this version of PDF, unrecognised PDFACompatibilityLevel,\nreverting to normal PDF output\n");
                    pdev->AbortPDFAX = true;
                    pdev->PDFA = 0;
                    break;
            }
        } else {
            emprintf(pdev->memory,
                     "Optional Content Properties not valid in this version of PDF. Dropping feature to preserve compatibility\n");
        }
    } else {
        str = (char *)gs_alloc_bytes(pdev->memory, pairs[0].size + 1, "pdfmark_OCProperties");
        memset(str, 0x00, pairs[0].size + 1);
        memcpy(str, pairs[0].data, pairs[0].size);

        (void)cos_dict_put_c_key_string(pdev->Catalog, "/OCProperties",
                                         (byte *)str, strlen(str));

        gs_free_object(pdev->memory, str, "pdfmark_OCProperties");
    }
    return 0;
}

/* ---------------- Dispatch ---------------- */

/*
 * Define the pdfmark types we know about.
 */
static const pdfmark_name mark_names[] =
{
        /* Miscellaneous. */
    {"ANN",          pdfmark_ANN,         PDFMARK_NAMEABLE},
    {"LNK",          pdfmark_LNK,         PDFMARK_NAMEABLE},
    {"OUT",          pdfmark_OUT,         0},
    {"ARTICLE",      pdfmark_ARTICLE,     0},
    {"DEST",         pdfmark_DEST,        PDFMARK_NAMEABLE},
    {"EMBED",        pdfmark_EMBED,       PDFMARK_NAMEABLE},
    {"PS",           pdfmark_PS,          PDFMARK_NAMEABLE},
    {"PAGES",        pdfmark_PAGES,       0},
    {"PAGE",         pdfmark_PAGE,        0},
    {"PAGELABEL",    pdfmark_PAGELABEL,   0},
    {"DOCINFO",      pdfmark_DOCINFO,     0},
    {"DOCVIEW",      pdfmark_DOCVIEW,     0},
        /* Named objects. */
    {"BP",           pdfmark_BP,          PDFMARK_NAMEABLE | PDFMARK_TRUECTM},
    {"EP",           pdfmark_EP,          0},
    {"SP",           pdfmark_SP,          PDFMARK_ODD_OK | PDFMARK_KEEP_NAME | PDFMARK_TRUECTM},
    {"OBJ",          pdfmark_OBJ,         PDFMARK_NAMEABLE},
    {"PUT",          pdfmark_PUT,         PDFMARK_ODD_OK | PDFMARK_KEEP_NAME},
    {".PUTDICT",     pdfmark_PUTDICT,     PDFMARK_ODD_OK | PDFMARK_KEEP_NAME},
    {".PUTINTERVAL", pdfmark_PUTINTERVAL, PDFMARK_ODD_OK | PDFMARK_KEEP_NAME},
    {".PUTSTREAM",   pdfmark_PUTSTREAM,   PDFMARK_ODD_OK | PDFMARK_KEEP_NAME |
                                          PDFMARK_NO_REFS},
    {"APPEND",       pdfmark_APPEND,      PDFMARK_KEEP_NAME},
    {"CLOSE",        pdfmark_CLOSE,       PDFMARK_ODD_OK | PDFMARK_KEEP_NAME},
    {"NamespacePush", pdfmark_NamespacePush, 0},
    {"NamespacePop", pdfmark_NamespacePop, 0},
    {"NI",           pdfmark_NI,          PDFMARK_NAMEABLE},
        /* Marked content. */
    {"MP",           pdfmark_MP,          PDFMARK_ODD_OK},
    {"DP",           pdfmark_DP,          0},
    {"BMC",          pdfmark_BMC,         PDFMARK_ODD_OK},
    {"BDC",          pdfmark_BDC,         PDFMARK_NO_REFS},
    {"EMC",          pdfmark_EMC,         0},
        /* Document structure. */
    {"StRoleMap",    pdfmark_StRoleMap,   0},
    {"StClassMap",   pdfmark_StClassMap,  0},
    {"StPNE",        pdfmark_StPNE,       PDFMARK_NAMEABLE},
    {"StBookmarkRoot", pdfmark_StBookmarkRoot, 0},
    {"StPush",       pdfmark_StPush,       0},
    {"StPop",        pdfmark_StPop,        0},
    {"StPopAll",     pdfmark_StPopAll,     0},
    {"StBMC",        pdfmark_StBMC,        0},
    {"StBDC",        pdfmark_StBDC,        0},
    /* EMC is listed under "Marked content" above. */
    {"StOBJ",        pdfmark_StOBJ,        0},
    {"StAttr",       pdfmark_StAttr,       0},
    {"StStore",      pdfmark_StStore,      0},
    {"StRetrieve",   pdfmark_StRetrieve,   0},
    /* Metadata and extension */
    {"Metadata",     pdfmark_Metadata,     0},
    {"Ext_Metadata", pdfmark_Ext_Metadata, 0},
    {"OCProperties", pdfmark_OCProperties, PDFMARK_ODD_OK},
        /* End of list. */
    {0, 0}
};

/* Process a pdfmark. */
int
pdfmark_process(gx_device_pdf * pdev, const gs_param_string_array * pma)
{
    const gs_param_string *data = pma->data;
    uint size = pma->size;
    const gs_param_string *pts = &data[size - 1];
    const gs_param_string *objname = 0;
    gs_matrix ctm;
    const pdfmark_name *pmn;
    int code = 0;

    if (size < 2)
        return_error(gs_error_stackunderflow);

    {	int cnt, len = pts[-1].size;
        char buf[200]; /* 6 doubles should fit (%g == -0.14285714285714285e-101 = 25 chars) */

        if (len > sizeof(buf) - 1)
            return_error(gs_error_rangecheck);
        memcpy(buf, pts[-1].data, len);
        buf[len] = 0;
        cnt = sscanf(buf, "[%g %g %g %g %g %g]",
                   &ctm.xx, &ctm.xy, &ctm.yx, &ctm.yy, &ctm.tx, &ctm.ty);
        if (cnt != 6)
            return_error(gs_error_rangecheck);
    }
    size -= 2;			/* remove CTM & pdfmark name */
    for (pmn = mark_names; pmn->mname != 0; ++pmn)
        if (pdf_key_eq(pts, pmn->mname)) {
            gs_memory_t *mem = pdev->pdf_memory;
            int odd_ok = (pmn->options & PDFMARK_ODD_OK) != 0;
            gs_param_string *pairs;
            int j, index;

            /*
             * Our coordinate system is scaled so that user space is always
             * default user space.  Adjust the CTM to match this, except if this
             * particular pdfmark requires the "true" CTM.
             */
            if (pmn->options & PDFMARK_TRUECTM)
                DO_NOTHING;
            else {
                double xscale = 72.0 / pdev->HWResolution[0],
                       yscale = 72.0 / pdev->HWResolution[1];
                ctm.xx *= xscale, ctm.xy *= yscale;
                ctm.yx *= xscale, ctm.yy *= yscale;
                ctm.tx *= xscale, ctm.ty *= yscale;
            }
            if (size & !odd_ok)
                return_error(gs_error_rangecheck);
            if (pmn->options & PDFMARK_NAMEABLE) {
                /* Look for an object name. */
                for (j = 0; j < size; j += 2) {
                    if (pdf_key_eq(&data[j], "/_objdef")) {
                        objname = &data[j + 1];
                        if (!pdf_objname_is_valid(objname->data,
                                                  objname->size)
                            )
                            return_error(gs_error_rangecheck);
                        /* Save the pairs without the name. */
                        size -= 2;
                        pairs = (gs_param_string *)
                            gs_alloc_byte_array(mem, size,
                                                sizeof(gs_param_string),
                                                "pdfmark_process(pairs)");
                        if (!pairs)
                            return_error(gs_error_VMerror);

                        for (index=0;index < size;index++)
                            pairs[index].data = NULL;
                        for (index=0;index < j;index++) {
                            pairs[index].data = gs_alloc_bytes(mem, data[index].size, "pdfmark_process(pairs)");
                            if (pairs[index].data == NULL)
                                goto error;
                            memcpy((byte *)pairs[index].data, data[index].data, data[index].size);
                            pairs[index].size = data[index].size;
                            pairs[index].persistent = 1;
                        }
                        for (index=j+2;index < size + 2;index++) {
                            pairs[index - 2].data = gs_alloc_bytes(mem, data[index].size, "pdfmark_process(pairs)");
                            if (pairs[index - 2].data == NULL)
                                goto error;
                            memcpy((byte *)pairs[index - 2].data, data[index].data, data[index].size);
                            pairs[index - 2].size = data[index].size;
                            pairs[index - 2].persistent = 1;
                        }
                        goto copied;
                    }
                }
            }
            /* Save all the pairs. */
            pairs = (gs_param_string *)
                gs_alloc_byte_array(mem, size, sizeof(gs_param_string),
                                    "pdfmark_process(pairs)");
            if (!pairs)
                return_error(gs_error_VMerror);
            for (j=0;j < size;j++)
                pairs[j].data = NULL;
            for (j=0;j < size;j++) {
                pairs[j].data = gs_alloc_bytes(mem, data[j].size, "pdfmark_process(pairs)");
                if (pairs[j].data == NULL)
                    goto error;
                memcpy((byte *)pairs[j].data, data[j].data, data[j].size);
                pairs[j].size = data[j].size;
                pairs[j].persistent = 1;
            }
copied:		/* Substitute object references for names. */
            if (!(pmn->options & PDFMARK_NO_REFS)) {
                for (j = (pmn->options & PDFMARK_KEEP_NAME ? 1 : 1 - odd_ok);
                     j < size; j += 2 - odd_ok
                     ) {
                    code = pdf_replace_names(pdev, &pairs[j], &pairs[j]);
                    if (code < 0) {
                        gs_free_object(mem, pairs, "pdfmark_process(pairs)");
                        return code;
                    }
                }
            }
            code = (*pmn->proc) (pdev, pairs, size, &ctm, objname);
error:
            for (j=0;j < size;j++)
                gs_free_object(mem, (byte *)pairs[j].data, "pdfmark_process(pairs)");
            gs_free_object(mem, pairs, "pdfmark_process(pairs)");
            break;
        }
    return code;
}
