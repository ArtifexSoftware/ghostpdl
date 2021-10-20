/* Copyright (C) 2020-2021 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
*/
/* Font re-mapping */

#include "strmio.h"
#include "stream.h"
#include "scanchar.h"

#include "pdf_int.h"
#include "pdf_types.h"
#include "pdf_array.h"
#include "pdf_dict.h"
#include "pdf_stack.h"
#include "pdf_file.h"
#include "pdf_misc.h"
#include "pdf_fmap.h"

typedef struct
{
    const char *keyname;
    const char *mappedname;
} pdfi_custom_fmap_entry;

pdfi_custom_fmap_entry pdfi_custom_fmap_enties[] =
{
  {"Helv", "Helvetica"},
  {NULL, NULL}
};

static inline bool pdfi_fmap_file_exists(pdf_context *ctx, pdf_string *fname)
{
    char nm[gp_file_name_sizeof];
    struct stat buf;
    int code = gs_error_invalidfileaccess;

    if (fname->length < gp_file_name_sizeof) {
        memcpy(nm, fname->data, fname->length);
        nm[fname->length] = '\0';
        code = gp_stat(ctx->memory, (const char *)nm, &buf);
    }
    return code >= 0;
}

static int
pdf_fontmap_open_file(pdf_context *ctx, const char *mapfilename, byte **buf, int *buflen)
{
    int code = 0;
    stream *s;
    char fname[gp_file_name_sizeof];
    const char *path_pfx = "Init/";
    const char *poststring = "\nendstream";
    const int poststringlen = strlen(poststring);
    fname[0] = '\0';

    if (strlen(path_pfx) + strlen(mapfilename) + 1 > gp_file_name_sizeof)
        return_error(gs_error_invalidfileaccess);

    code = pdfi_open_resource_file(ctx, mapfilename, strlen(mapfilename), &s);
    if (code < 0) {
        strncat(fname, path_pfx, strlen(path_pfx));
        strncat(fname, (char *)mapfilename, strlen(mapfilename));
        code = pdfi_open_resource_file(ctx, fname, strlen(fname), &s);
    }

    if (code >= 0) {
        int i;
        sfseek(s, 0, SEEK_END);
        *buflen = sftell(s);
        sfseek(s, 0, SEEK_SET);
        *buf = gs_alloc_bytes(ctx->memory, *buflen + poststringlen, "pdf_cmap_open_file(buf)");
        if (*buf != NULL) {
            sfread((*buf), 1, *buflen, s);
            memcpy((*buf) + *buflen, poststring, poststringlen);
            *buflen += poststringlen;
            /* This is naff, but works for now
               When parsing Fontmap in PS, ";" is defined as "def"
               We don't need either, because the dictionary is built from the stack.
             */
            for (i = 0; i < *buflen - 1; i++) {
                if ((*buf)[i] == ';') {
                    (*buf)[i] = ' ';
                }
            }
        }
        else {
            code = gs_note_error(gs_error_VMerror);
        }
        sfclose(s);
    }
    return code;
}

static int
pdf_make_fontmap(pdf_context *ctx, const char *fmapname, bool cidfmap)
{
    byte *fmapbuf = NULL;
    int code, fmapbuflen;
    pdf_c_stream *fmapstr = NULL;
    pdf_stream fakedict = {0};
    pdfi_custom_fmap_entry *pcfe = pdfi_custom_fmap_enties;
    int i;

    pdf_c_stream fakemainstream = {0};

    code = pdf_fontmap_open_file(ctx, (const char *)fmapname, &fmapbuf, &fmapbuflen);
    if (code < 0)
        return code;

    code = pdfi_open_memory_stream_from_memory(ctx, fmapbuflen, fmapbuf, &fmapstr, true);
    if (code >= 0) {
        int stacksize = pdfi_count_stack(ctx);

        if (ctx->main_stream == NULL) {
            ctx->main_stream = &fakemainstream;
        }
        code = pdfi_mark_stack(ctx, PDF_DICT_MARK);
        if (code < 0)
            goto done;
        code = pdfi_interpret_content_stream(ctx, fmapstr, &fakedict, NULL);
        if (ctx->main_stream == &fakemainstream) {
            ctx->main_stream = NULL;
        }
        if (code >= 0 && pdfi_count_stack(ctx) > stacksize) {
            code = pdfi_dict_from_stack(ctx, 0, 0, true);
            if (code < 0)
                goto done;

            if (cidfmap == true) {
                ctx->pdfcidfmap = (pdf_dict *)ctx->stack_top[-1];
                pdfi_countup(ctx->pdfcidfmap);
            }
            else {
                ctx->pdffontmap = (pdf_dict *)ctx->stack_top[-1];
                pdfi_countup(ctx->pdffontmap);
            }
            pdfi_pop(ctx, 1);
            code = 0;

            /* Add our internal aliases to the fontmap. */
            if (cidfmap == false) {
                for (i = 0; pcfe[i].keyname != NULL; i++) {
                    pdf_obj *value;
                    bool k;

                    /* We don't want to *replace* entries */
                    if (pdfi_dict_known(ctx, ctx->pdffontmap, pcfe[i].keyname, &k) >= 0
                        && k != true) {
                        code = pdfi_name_alloc(ctx, (byte *)pcfe[i].mappedname, strlen(pcfe[i].mappedname), &value);
                        if (code < 0)
                            continue;
                        pdfi_countup(value);
                        /* If dict_put throws an error, we just carry on - hence the (void) */
                        (void)pdfi_dict_put(ctx,  ctx->pdffontmap, pcfe[i].keyname, value);
                        pdfi_countdown(value);
                    }
                }
            }
        }
        else {
            code = gs_note_error(gs_error_syntaxerror);
        }
    }
done:
    gs_free_object(ctx->memory, fmapbuf, "pdf_make_fontmap(fmapbuf)");
    return code;
}


int
pdf_fontmap_lookup_font(pdf_context *ctx, pdf_name *fname, pdf_obj **mapname)
{
    int code = 0;
    pdf_obj *mname;

    if (ctx->pdffontmap == NULL) {
        const char *fmap_default = "Fontmap.GS";
        char *fmapname = (char *)fmap_default;
        code = pdf_make_fontmap(ctx, fmapname, false);
        if (code < 0) {
            return code;
        }
    }
    code = pdfi_dict_get_by_key(ctx, ctx->pdffontmap, fname, &mname);
    if (code < 0)
        return code;
    /* Fontmap can map in multiple "jump" i.e.
       name -> substitute name
       subsitute name -> file name
       So we want to loop until we no more hits.
     */
    while(1) {
        pdf_obj *mname2;
        code = pdfi_dict_get_by_key(ctx, ctx->pdffontmap, (pdf_name *)mname, &mname2);
        if (code < 0) break;
        pdfi_countdown(mname);
        mname = mname2;
    }
    if (mname->type == PDF_STRING && pdfi_fmap_file_exists(ctx, (pdf_string *)mname)) {
        *mapname = mname;
        code = 0;
    }
    else if (mname->type == PDF_NAME) { /* If we map to a name, we assume (for now) we have the font as a "built-in" */
        *mapname = mname;
        code = 0;
    }
    else {
        pdfi_countdown(mname);
        code = gs_note_error(gs_error_undefined);
    }
    return code;
}

int
pdf_fontmap_lookup_cidfont(pdf_context *ctx, pdf_dict *font_dict, pdf_name *name, pdf_obj **mapname, int *findex)
{
    int code = 0;
    pdf_obj *cidname = NULL;
    pdf_obj *mname;

    if (ctx->pdfcidfmap == NULL) {
        const char *cidfmap_default = "cidfmap";
        char *cidfmapname = (char *)cidfmap_default;
        code = pdf_make_fontmap(ctx, cidfmapname, true);
        if (code < 0) {
            return code;
        }
    }
    if (name == NULL || name->type != PDF_NAME) {
        code = pdfi_dict_get(ctx, font_dict, "BaseFont", &cidname);
        if (code < 0 || cidname->type != PDF_NAME) {
            pdfi_countdown(cidname);
            return_error(gs_error_undefined);
        }

    }
    else {
        cidname = (pdf_obj *)name;
    }

    code = pdfi_dict_get_by_key(ctx, ctx->pdfcidfmap, (pdf_name *)cidname, &mname);
    pdfi_countdown(cidname);
    if (code < 0)
        return code;
    /* As above cidfmap can map in multiple "jump" i.e.
       name -> substitute name
       subsitute name -> "record"
       So we want to loop until we no more hits.
     */
    while(1) {
        pdf_obj *mname2;
        code = pdfi_dict_get_by_key(ctx, ctx->pdfcidfmap, (pdf_name *)mname, &mname2);
        if (code < 0) break;
        pdfi_countdown(mname);
        mname = mname2;
    }
    if (mname->type == PDF_DICT) {
        pdf_dict *rec = (pdf_dict *)mname;
        pdf_name *filetype;
        pdf_name *path = NULL;
        pdf_num *ind = NULL;
        pdf_array *mcsi = NULL;
        pdf_dict *ocsi = NULL;
        pdf_string *ord1 = NULL, *ord2 = NULL;
        pdf_num *sup1, *sup2;

        code = pdfi_dict_get(ctx, rec, "FileType", (pdf_obj **)&filetype);
        /* We only handle TTF files, just now */
        if (code < 0 || filetype->type != PDF_NAME || filetype->length != 8 || memcmp(filetype->data, "TrueType", 8) != 0) {
            pdfi_countdown(filetype);
            pdfi_countdown(rec);
            return_error(gs_error_undefined);
        }
        pdfi_countdown(filetype);

        code = pdfi_dict_get(ctx, rec, "CSI", (pdf_obj **)&mcsi);
        if (code < 0 || mcsi->type != PDF_ARRAY) {
            pdfi_countdown(mcsi);
            pdfi_countdown(rec);
            return_error(gs_error_undefined);
        }

        code = pdfi_dict_get(ctx, font_dict, "CIDSystemInfo", (pdf_obj **)&ocsi);
        if (code < 0 || ocsi->type != PDF_DICT) {
            pdfi_countdown(ocsi);
            pdfi_countdown(mcsi);
            pdfi_countdown(rec);
            return_error(gs_error_undefined);
        }
        code = pdfi_dict_get(ctx, ocsi, "Ordering", (pdf_obj **)&ord1);
        if (code < 0 || ord1->type != PDF_STRING) {
            pdfi_countdown(ord1);
            pdfi_countdown(ocsi);
            pdfi_countdown(mcsi);
            pdfi_countdown(rec);
            return_error(gs_error_undefined);
        }
        code = pdfi_array_get(ctx, mcsi, 0, (pdf_obj **)&ord2);
        if (code < 0 || ord2->type != PDF_STRING) {
            pdfi_countdown(ord1);
            pdfi_countdown(ord2);
            pdfi_countdown(ocsi);
            pdfi_countdown(mcsi);
            pdfi_countdown(rec);
            return_error(gs_error_undefined);
        }
        if (pdfi_string_cmp(ord1, ord2) != 0) {
            pdfi_countdown(ord1);
            pdfi_countdown(ord2);
            pdfi_countdown(ocsi);
            pdfi_countdown(mcsi);
            pdfi_countdown(rec);
            return_error(gs_error_undefined);
        }
        pdfi_countdown(ord1);
        pdfi_countdown(ord2);
        code = pdfi_dict_get(ctx, ocsi, "Supplement", (pdf_obj **)&sup1);
        if (code < 0 || sup1->type != PDF_INT) {
            pdfi_countdown(ord1);
            pdfi_countdown(ocsi);
            pdfi_countdown(mcsi);
            pdfi_countdown(rec);
            return_error(gs_error_undefined);
        }
        code = pdfi_array_get(ctx, mcsi, 1, (pdf_obj **)&sup2);
        if (code < 0 || sup2->type != PDF_INT || sup1->value.i != sup2->value.i) {
            pdfi_countdown(sup1);
            pdfi_countdown(sup2);
            pdfi_countdown(ocsi);
            pdfi_countdown(mcsi);
            pdfi_countdown(rec);
            return_error(gs_error_undefined);
        }
        pdfi_countdown(sup1);
        pdfi_countdown(sup2);
        pdfi_countdown(ocsi);
        pdfi_countdown(mcsi);

        code = pdfi_dict_get(ctx, rec, "Path", (pdf_obj **)&path);
        if (code < 0 || path->type != PDF_STRING || !pdfi_fmap_file_exists(ctx, (pdf_string *)path)) {
            pdfi_countdown(rec);
            return_error(gs_error_undefined);
        }

        *mapname = (pdf_obj *)path;
        code = pdfi_dict_get(ctx, rec, "Index", (pdf_obj **)&ind);
        if (code >= 0 && ind->type != PDF_INT) {
            *findex = ind->value.i;
        }
        else {
            *findex = 0;
        }
        pdfi_countdown(ind);
        code = 0;

    }
    else {
        *mapname = (pdf_obj *)mname;
        code = 0;
    }

    return code;
}
