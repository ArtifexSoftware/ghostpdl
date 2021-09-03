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
#include "pdf_dict.h"
#include "pdf_stack.h"
#include "pdf_file.h"
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

static int
pdf_fontmap_open_file(pdf_context *ctx, byte **buf, int *buflen)
{
    int code = 0;
    stream *s;
    char fname[gp_file_name_sizeof];
    const char *path_pfx = "Init/";
    const char *fmap_default = "Fontmap.GS";
    const char *prestring = "<<\n";
    const char *poststring = ">>\nendstream\n";
    const int prestringlen = strlen(prestring);
    const int poststringlen = strlen(poststring);
    fname[0] = '\0';

    if (strlen(path_pfx) + strlen(fmap_default) + 1 > gp_file_name_sizeof)
        return_error(gs_error_invalidfileaccess);

    code = pdfi_open_resource_file(ctx, fmap_default, strlen(fmap_default), &s);
    if (code < 0) {
        strncat(fname, path_pfx, strlen(path_pfx));
        strncat(fname, (char *)fmap_default, strlen(fmap_default));
        code = pdfi_open_resource_file(ctx, fname, strlen(fname), &s);
    }

    if (code >= 0) {
        int i;
        sfseek(s, 0, SEEK_END);
        *buflen = sftell(s);
        sfseek(s, 0, SEEK_SET);
        *buf = gs_alloc_bytes(ctx->memory, *buflen + prestringlen + poststringlen, "pdf_cmap_open_file(buf)");
        if (*buf != NULL) {
            memcpy(*buf, prestring, prestringlen);
            sfread((*buf) + prestringlen, 1, *buflen, s);
            memcpy((*buf) + *buflen + prestringlen, poststring, poststringlen);
            *buflen += prestringlen + poststringlen;
            /* This is naff, but works for now
               When parsing Fontmap in PS, ";" is defined as "def"
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
pdf_make_fontmap(pdf_context *ctx)
{
    byte *fmapbuf = NULL;
    int code, fmapbuflen;
    pdf_c_stream *fmapstr = NULL;
    pdf_stream fakedict = {0};
    pdfi_custom_fmap_entry *pcfe = pdfi_custom_fmap_enties;
    int i;

    pdf_c_stream fakemainstream = {0};

    code = pdf_fontmap_open_file(ctx, &fmapbuf, &fmapbuflen);
    if (code < 0)
        return code;

    code = pdfi_open_memory_stream_from_memory(ctx, fmapbuflen, fmapbuf, &fmapstr, true);
    if (code >= 0) {
        int stacksize = pdfi_count_stack(ctx);

        if (ctx->main_stream == NULL) {
            ctx->main_stream = &fakemainstream;
        }
        code = pdfi_interpret_content_stream(ctx, fmapstr, &fakedict, NULL);
        if (ctx->main_stream == &fakemainstream) {
            ctx->main_stream = NULL;
        }
        if (pdfi_count_stack(ctx) > stacksize && ctx->stack_top[-1]->type == PDF_DICT) {
            ctx->pdffontmap = (pdf_dict *)ctx->stack_top[-1];
            pdfi_countup(ctx->pdffontmap);
            pdfi_pop(ctx, 1);
            code = 0;

            /* Add our internal aliases to the fontmap. */
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
        else {
            code = gs_note_error(gs_error_syntaxerror);
        }
    }
    gs_free_object(ctx->memory, fmapbuf, "pdf_make_fontmap(fmapbuf)");
    return code;
}


int
pdf_fontmap_lookup_font(pdf_context *ctx, pdf_name *fname, pdf_obj **mapname)
{
    int code = 0;
    pdf_obj *mname;

    if (ctx->pdffontmap == NULL) {
        code = pdf_make_fontmap(ctx);
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
    *mapname = mname;
    return 0;
}
