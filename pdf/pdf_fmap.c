/* Copyright (C) 2020-2025 Artifex Software, Inc.
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
/* Font re-mapping */

#include "strmio.h"
#include "stream.h"
#include "scanchar.h"
#include "gsstrl.h"

#include "pdf_int.h"
#include "pdf_types.h"
#include "pdf_font_types.h"
#include "pdf_array.h"
#include "pdf_dict.h"
#include "pdf_stack.h"
#include "pdf_file.h"
#include "pdf_misc.h"
#include "pdf_font.h"
#include "pdf_fontmt.h"
#include "pdf_fmap.h"

typedef struct
{
    const char *keyname;
    const char *mappedname;
} pdfi_custom_fmap_entry;

pdfi_custom_fmap_entry pdfi_custom_fmap_entries[] =
{
  {"Helv", "Helvetica"},
  {NULL, NULL}
};

static inline bool pdfi_fmap_file_exists(pdf_context *ctx, pdf_string *fname)
{
    return pdfi_font_file_exists(ctx, (const char *)fname->data, fname->length);
}

static int
pdfi_fontmap_open_file(pdf_context *ctx, const char *mapfilename, byte **buf, int *buflen)
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
        int64_t file_size;

        sfseek(s, 0, SEEK_END);
        file_size = sftell(s);
        sfseek(s, 0, SEEK_SET);
        if (file_size < 0 || file_size > max_int - poststringlen) {
            code = gs_note_error(gs_error_ioerror);
        } else {
            byte *dbuf;
            *buflen = (int)file_size;

            (*buf) = gs_alloc_bytes(ctx->memory, *buflen + poststringlen, "pdf_cmap_open_file(buf)");
            if (*buf != NULL) {
                sfread((*buf), 1, *buflen, s);
                memcpy((*buf) + *buflen, poststring, poststringlen);
                *buflen += poststringlen;
                /* This is naff, but works for now
                   When parsing Fontmap in PS, ";" is defined as "def"
                   Also some people use "cvn" to convert a string to a name.
                   We don't need either, because the dictionary is built from the stack.
                 */
                for (i = 0, dbuf = *buf; i < *buflen - 1; i++, dbuf++) {
                    if (*dbuf == ';') {
                        *dbuf = ' ';
                    }
                    if (memcmp(dbuf, "cvn ", 4) == 0) {
                        dbuf[0] = dbuf[1] = dbuf[2] = 0x20;
                    }
                }
            }
            else {
                code = gs_note_error(gs_error_VMerror);
            }
        }
        sfclose(s);
    }
    return code;
}

#ifdef UFST_BRIDGE
/* we know fco_path is null terminated */
static pdf_string *pdfi_make_fco_path_string(pdf_context *ctx, char *fco_path)
{
    pdf_string *fp;
    int code = pdfi_object_alloc(ctx, PDF_STRING, strlen(fco_path), (pdf_obj **)&fp);
    if (code < 0)
        return NULL;
    pdfi_countup(fp);
    memcpy(fp->data, fco_path, strlen(fco_path));

    return fp;
}
#endif /* UFST_BRIDGE */

static inline int pdfi_populate_ufst_fontmap(pdf_context *ctx)
{
#ifdef UFST_BRIDGE
    int status = 0;
    int bSize, i;
    char pthnm[gp_file_name_sizeof];
    char *ufst_root_dir;
    char *fco;
    char *fco_start, *fco_lim;
    size_t ufst_root_dir_len;

    if (pdfi_fapi_ufst_available(ctx->memory) == false) {
        return (0);
    }

    ufst_root_dir = (char *)pdfi_fapi_ufst_get_font_dir(ctx->memory);
    ufst_root_dir_len = strlen(ufst_root_dir);
    if (ufst_root_dir_len >= gp_file_name_sizeof) {
        return gs_error_Fatal;
    }
    fco_start = fco = (char *)pdfi_fapi_ufst_get_fco_list(ctx->memory);
    fco_lim = fco_start + strlen(fco_start) + 1;
    while (fco < fco_lim && status == 0) {
        pdf_string *fco_str;
        status = 0;
        /* build and open (get handle) for the k'th fco file name */
        gs_strlcpy((char *)pthnm, ufst_root_dir, sizeof pthnm);

        for (i = 2; fco[i] != gp_file_name_list_separator && (&fco[i]) < fco_lim - 1; i++)
            ;

        if (i + ufst_root_dir_len >= gp_file_name_sizeof) {
            return gs_error_Fatal;
        }
        strncat(pthnm, fco, i);
        fco += (i + 1);
        fco_str = pdfi_make_fco_path_string(ctx, pthnm);
        if (fco_str == NULL) {
            return gs_error_Fatal;
        }

        /* enumerate the files in this fco */
        for (i = 0; status == 0; i++) {
            char *pname = NULL;
            pdf_font_microtype *pdffont = NULL;
            int font_number = 0;

            status = pdfi_alloc_mt_font(ctx, fco_str, i, &pdffont);
            if (status < 0)
                break;
            status = pdfi_fapi_passfont((pdf_font *)pdffont, i, (char *)"UFST", pthnm, NULL, 0);
            if (status < 0){
#ifdef DEBUG
                dmprintf1(ctx->memory, "CGIFfco_Access error %d\n", status);
#endif
                pdfi_countdown(pdffont);
                break;
            }

            /* For Microtype fonts, once we get here, these
             * pl_fapi_get*() calls cannot fail, so we can
             * safely ignore the return value
             */
            (void)pdfi_fapi_get_mtype_font_name((gs_font *)pdffont->pfont, NULL, &bSize);

            pname = (char *)gs_alloc_bytes(ctx->memory, bSize, "pdfi: mt font name buffer");
            if (!pname) {
                pdfi_countdown(pdffont);
                dmprintf1(ctx->memory, "VM Error for built-in font %d", i);
                continue;
            }

            (void)pdfi_fapi_get_mtype_font_name((gs_font *)pdffont->pfont, (byte *) pname, &bSize);
            (void)pdfi_fapi_get_mtype_font_number((gs_font *)pdffont->pfont, &font_number);

            if (bSize < gs_font_name_max) {
                memcpy(pdffont->pfont->key_name.chars, pname, bSize);
                pdffont->pfont->key_name.chars[bSize] = 0;
                pdffont->pfont->key_name.size = bSize;
                memcpy(pdffont->pfont->font_name.chars, pname, bSize);
                pdffont->pfont->font_name.chars[bSize] = 0;
                pdffont->pfont->font_name.size = bSize;
            }
            status = gs_definefont(ctx->font_dir, (gs_font *) pdffont->pfont);
            if (status < 0) {
                pdfi_countdown(pdffont);
                status = 0;
                continue;
            }
            gs_notify_release(&pdffont->pfont->notify_list);
            status = pdfi_fapi_passfont((pdf_font *)pdffont, i, (char *)"UFST", pthnm, NULL, 0);
            if (status < 0) {
                pdfi_countdown(pdffont);
                status = 0;
                continue;
            }
            status = pdfi_dict_put(ctx, ctx->pdffontmap, pname, (pdf_obj *)pdffont);
            pdfi_countdown(pdffont);
            if (status < 0) {
                status = 0;
                continue;
            }
        }
        pdfi_countdown(fco_str);
    }
#endif /* UFST_BRIDGE */
    return 0;
}

static int
pdf_make_fontmap(pdf_context *ctx, const char *default_fmapname, int cidfmap)
{
    byte *fmapbuf = NULL;
    int code, fmapbuflen;
    pdf_c_stream *fmapstr = NULL;
    pdf_stream fakedict = {0};
    pdfi_custom_fmap_entry *pcfe = pdfi_custom_fmap_entries;
    int i, j = 0;
    char fmapname[gp_file_name_sizeof];
    pdf_c_stream fakemainstream = {0};
    int stacksize = pdfi_count_stack(ctx);

    strncpy(fmapname, default_fmapname, strlen(default_fmapname) + 1);

    code = pdfi_mark_stack(ctx, PDF_DICT_MARK);
    if (code < 0)
        goto done;

    do {
        if (j < ctx->num_fontmapfiles && cidfmap == false) {
            memcpy(fmapname, ctx->fontmapfiles[j].data, ctx->fontmapfiles[j].size);
            fmapname[ctx->fontmapfiles[j].size] = '\0';
        }

        code = pdfi_fontmap_open_file(ctx, (const char *)fmapname, &fmapbuf, &fmapbuflen);
        if (code < 0) {
            if (ctx->args.QUIET != true) {
                (void)outwrite(ctx->memory, "Warning: ", 9);
                if (cidfmap)
                    (void)outwrite(ctx->memory, "cidfmap file ", 13);
                else
                    (void)outwrite(ctx->memory, "Fontmap file \"", 14);
                (void)outwrite(ctx->memory, fmapname, strlen(fmapname));
                (void)outwrite(ctx->memory, "\" not found.\n", 13);
                code = 0;
            }
        }
        else {
            code = pdfi_open_memory_stream_from_memory(ctx, fmapbuflen, fmapbuf, &fmapstr, true);
            if (code >= 0) {

                if (ctx->main_stream == NULL) {
                    ctx->main_stream = &fakemainstream;
                }

                if (fmapbuflen > 0)
                    code = pdfi_interpret_content_stream(ctx, fmapstr, &fakedict, NULL);
                else
                    code = 0;

                if (ctx->main_stream == &fakemainstream) {
                    ctx->main_stream = NULL;
                }
                gs_free_object(ctx->memory, fmapbuf, "pdf_make_fontmap(fmapbuf)");
            }
        }
        j++;
    } while(j < ctx->num_fontmapfiles && cidfmap == false && code >= 0);

    if (code >= 0) {
        if (pdfi_count_stack(ctx) > stacksize) {
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
    /* We always want to leave here with a valid map dictionary
       even if it's empty
     */
    if (cidfmap == true) {
        if (ctx->pdfcidfmap == NULL) {
            code = pdfi_dict_alloc(ctx, 0, &ctx->pdfcidfmap);
            pdfi_countup(ctx->pdfcidfmap);
        }
    }
    else {
        if (ctx->pdffontmap == NULL) {
            code = pdfi_dict_alloc(ctx, 0, &ctx->pdffontmap);
            pdfi_countup(ctx->pdffontmap);
        }
    }
    if (code >= 0) {
        code = pdfi_populate_ufst_fontmap(ctx);
    }
#ifdef DUMP_FONTMAP
    if (ctx->pdffontmap != NULL) {
        uint64_t ind;
        int find = -1;
        pdf_name *key = NULL;
        pdf_obj *v = NULL;
        pdf_string *val = NULL;
        (void)pdfi_dict_key_first(ctx, ctx->pdffontmap, (pdf_obj **) &key, &ind);
        (void)pdfi_dict_get_by_key(ctx, ctx->pdffontmap, key, (pdf_obj **)&v);
        for (j = 0; j < key->length; j++)
            dprintf1("%c", key->data[j]);
        if (pdfi_type_of(v) == PDF_DICT) {
            pdf_num *n;
            pdf_string *val2;
            code = pdfi_dict_get(ctx, (pdf_dict *)v, "Index", (pdf_obj **)&n);
            if (code >= 0 && pdfi_type_of(n) == PDF_INT)
                find = n->value.i;
            else
                code = 0;
            (void)pdfi_dict_get(ctx, (pdf_dict *)v, "Path", (pdf_obj **)&val2);
            val = val2;
        }
        else {
            val = (pdf_string *)v;
        }
        dprintf("	");
        for (j = 0; j < val->length; j++)
            dprintf1("%c", val->data[j]);
        if (find != -1) {
            dprintf1("	Index = %d", find);
            find = -1;
        }

        dprintf("\n");
        pdfi_countdown(key);
        pdfi_countdown(val);

        while (pdfi_dict_key_next(ctx, ctx->pdffontmap, (pdf_obj **) &key, &ind) >= 0 && ind > 0) {
            (void)pdfi_dict_get_by_key(ctx, ctx->pdffontmap, key, (pdf_obj **)&v);
            for (j = 0; j < key->length; j++)
                dprintf1("%c", key->data[j]);
            if (pdfi_type_of(v) == PDF_DICT) {
                pdf_num *n;
                pdf_string *val2;
                code = pdfi_dict_get(ctx, (pdf_dict *)v, "Index", (pdf_obj **)&n);
                if (code >= 0 && pdfi_type_of(n) == PDF_INT)
                    find = n->value.i;
                else
                    code = 0;
                (void)pdfi_dict_get(ctx, (pdf_dict *)v, "Path", (pdf_obj **)&val2);
                val = val2;
            }
            else {
                val = (pdf_string *)v;
            }
            dprintf("       ");
            for (j = 0; j < val->length; j++)
                dprintf1("%c", val->data[j]);
            if (find != -1) {
                dprintf1("	Index = %d", find);
                find = -1;
            }
            pdfi_countdown(key);
            pdfi_countdown(val);
            dprintf("\n");
        }
    }
#endif
    pdfi_clearstack(ctx);
    return code;
}

enum {
  no_type_font = -1,
  type0_font = 0,
  type1_font = 1,
  cff_font = 2,
  type3_font = 3,
  tt_font = 42
};

/* For font file scanning we want to treat OTTO as TTF (rather than, for "normal"
   font loading, treat OTTO as CFF, hence we need a different type picker.
 */
static int pdfi_font_scan_type_picker(byte *buf, int64_t buflen)
{
#define MAKEMAGIC(a, b, c, d) (((a) << 24) | ((b) << 16) | ((c) << 8) | (d))

    if (buflen >= 4) {
        if (MAKEMAGIC(buf[0], buf[1], buf[2], buf[3]) == MAKEMAGIC(0, 1, 0, 0)
            || MAKEMAGIC(buf[0], buf[1], buf[2], buf[3]) == MAKEMAGIC('t', 'r', 'u', 'e')
            || MAKEMAGIC(buf[0], buf[1], buf[2], buf[3]) == MAKEMAGIC('t', 't', 'c', 'f')) {
            return tt_font;
        }
        else if (MAKEMAGIC(buf[0], buf[1], buf[2], buf[3]) == MAKEMAGIC('O', 'T', 'T', 'O')) {
            return tt_font;
        }
        else if (MAKEMAGIC(buf[0], buf[1], buf[2], 0) == MAKEMAGIC('%', '!', 'P', 0)) {
            return type1_font; /* pfa */
        }
        else if (MAKEMAGIC(buf[0], buf[1], buf[2], 0) == MAKEMAGIC(1, 0, 4, 0)) {
            return cff_font; /* 1C/CFF */
        }
        else if (MAKEMAGIC(buf[0], buf[1], 0, 0) == MAKEMAGIC(128, 1, 0, 0)) {
            return type1_font; /* pfb */
        }
    }
    return no_type_font;
#undef MAKEMAGIC
}

static int pdfi_add__to_native_fontmap(pdf_context *ctx, const char *fontname, const char *filepath, const int index)
{
    int code;
    pdf_string *fpstr;

    if (ctx->pdfnativefontmap == NULL) {
        /* 32 is just an arbitrary starting point */
        code = pdfi_dict_alloc(ctx, 32, &ctx->pdfnativefontmap);
        if (code < 0)
            return code;
        pdfi_countup(ctx->pdfnativefontmap);
    }
    /* index == -1 is a file with a single font in it */
    if (index == -1) {
        code = pdfi_object_alloc(ctx, PDF_STRING, strlen(filepath), (pdf_obj **)&fpstr);
        if (code < 0)
            return code;
        pdfi_countup(fpstr);
        memcpy(fpstr->data, filepath, fpstr->length);
        code = pdfi_dict_put(ctx, ctx->pdfnativefontmap, fontname, (pdf_obj *)fpstr);
        pdfi_countdown(fpstr);
    }
    else {
        pdf_dict *recdict;
        pdf_num *ind;

        code = pdfi_object_alloc(ctx, PDF_DICT, 2, (pdf_obj **)&recdict);
        if (code < 0)
            return code;
        pdfi_countup(recdict);

        code = pdfi_object_alloc(ctx, PDF_STRING, strlen(filepath), (pdf_obj **)&fpstr);
        if (code < 0) {
            pdfi_countdown(recdict);
            return code;
        }
        pdfi_countup(fpstr);
        memcpy(fpstr->data, filepath, fpstr->length);
        code = pdfi_dict_put(ctx, recdict, "Path", (pdf_obj *)fpstr);
        pdfi_countdown(fpstr);
        if (code < 0) {
            pdfi_countdown(recdict);
            return code;
        }

        code = pdfi_object_alloc(ctx, PDF_INT, 0, (pdf_obj **)&ind);
        if (code < 0) {
            pdfi_countdown(recdict);
            return code;
        }
        ind->value.i = index;
        pdfi_countup(ind);
        code = pdfi_dict_put(ctx, recdict, "Index", (pdf_obj *)ind);
        pdfi_countdown(ind);
        if (code < 0) {
            pdfi_countdown(recdict);
            return code;
        }
        code = pdfi_dict_put(ctx, ctx->pdfnativefontmap, fontname, (pdf_obj *)recdict);
        pdfi_countdown(recdict);
    }

    return code;
}

static inline int
pdfi_end_ps_token(int c)
{
    return (c == 0x20) || (c == 0x9) || (c == 0xD) || (c == 0xA) || (c == '/')  || (c == '\0');
}

/* Naive way to find a Type 1 /FontName key */
static int pdfi_type1_add_to_native_map(pdf_context *ctx, stream *f, char *fname, char *pname, int pname_size)
{
    gs_string buf;
    uint count = 0;
    int code = gs_error_undefined;
    char *namestr = NULL, *enamestr;
    char *typestr;
    bool pin_eol = false; /* initialised just to placate coverity */
    int type = -1;
    buf.data = (byte *)pname;
    buf.size = pname_size;

    /* It's not an absolute requirement, but it is a de facto standard that
       /FontType and /FontName keys start in column 0 of their lines
     */
    while ((code = sreadline(f, NULL, NULL, NULL, &buf, NULL, &count, &pin_eol, NULL)) >= 0) {
        if (buf.size > 9 && memcmp(buf.data, "/FontName", 9) == 0) {
            namestr = (char *)buf.data + 9;
            while (pdfi_end_ps_token(*namestr))
                namestr++;
            if (*namestr != '\0') {
                while(*namestr == 0x20 || *namestr == 0x9)
                    namestr++;
                if (*namestr == '/')
                    namestr++;
                enamestr = namestr;
                while(!pdfi_end_ps_token((int)*enamestr))
                    enamestr++;
                count = enamestr - namestr > pname_size - 1 ? pname_size - 1 : enamestr - namestr;
                memmove(pname, namestr, count);
                pname[count] = '\0';
                buf.data += count + 1;
                buf.size -= count + 1;
                if (type == 1)
                    break;
            }
        }
        else if (buf.size > 9 && memcmp(buf.data, "/FontType", 9) == 0) {
            typestr = (char *)buf.data + 9;
            while (pdfi_end_ps_token(*typestr))
                typestr++;
            if (*typestr != '\0') {
                while(*typestr == 0x20 || *typestr == 0x9)
                    namestr++;
                if (*typestr == '/')
                    typestr++;
                enamestr = typestr;
                while(!pdfi_end_ps_token((int)*enamestr))
                    enamestr++;
                count = enamestr - typestr > pname_size - 1 ? pname_size - 1 : enamestr - typestr;
                if (count == 1 && *typestr == '1') {
                    type = 1;
                    if (namestr != NULL)
                        break;
                }
            }
        }
        else if (buf.size >= 17 && memcmp(buf.data, "currentfile eexec", 17) == 0)
            break;
        count = 0;
    }
    if (type == 1 && namestr != NULL) {
        code = pdfi_add__to_native_fontmap(ctx, (const char *)pname, (const char *)fname, -1);
    }
    return code < 0 ? code : gs_error_handled;
}

static inline int
u16(const byte *p)
{
    return (p[0] << 8) | p[1];
}

static inline int
sru16(stream *s)
{
    byte p[2];
    int code = sfread(p, 1, 2, s);

    if (code < 0)
        return 0;

    return u16(p);
}

static inline int
u32(const byte *p)
{
    return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

static inline int
sru32(stream *s)
{
    byte p[4];
    int code = sfread(p, 1, 4, s);
    if (code < 0)
        return 0;

    return u32(p);
}

static int pdfi_ttf_add_to_native_map(pdf_context *ctx, stream *f, byte magic[4], char *fname, char *pname, int pname_size)
{
    int ntables, i, j, k, code2, code = gs_error_undefined;
    char table[4];
    int nte, storageOffset;
    bool include_index = false;
    uint32_t nfonts = 1, tableoffs;
    int findex;
    gs_offset_t l;

    code2 = sfseek(f, 0, SEEK_END);
    if (code2 < 0)
        return code;
    l = stell(f);
    /* 4 to skip the magic number */
    code2 = sfseek(f, 4, SEEK_SET);
    if (code2 < 0)
        return code;

    if (memcmp(magic, "ttcf", 4) == 0) {
        uint32_t ver;
        include_index = true;
        ver = sru32(f);
        if (ver != 0x00010000 && ver !=0x00020000) {
            dmprintf1(ctx->memory, "Unknown TTC header version %08X.\n", ver);
            return_error(gs_error_invalidaccess);
        }
        nfonts = sru32(f);
        /* There isn't a specific limit on the number of fonts,
           freetype limits to 255, so we'll do the same here
         */
        if (nfonts > 255)
            return_error(gs_error_invalidfont);
    }

    for (findex = 0; findex < nfonts; findex++) {
        if (include_index == true ) {
            if (findex * 4 > l)
                break;
            code2 = sfseek(f, findex * 4, SEEK_CUR);
            if (code2 < 0)
                return code2;

            tableoffs = sru32(f);
            if (tableoffs + 4 > l)
                break;
            code2 = sfseek(f, tableoffs + 4, SEEK_SET);
            if (code2 < 0)
                return code2;
        }

        ntables = sru16(f);
        /* Similar to above - no spec limit, but we do the same
           as freetype
         */
        if (ntables > 255)
            continue;

        /* Skip the remainder of the invariant header bytes */
        (void)sru16(f);
        (void)sru32(f);

        for (i = 0, code2 = 0; i < ntables && code2 >= 0; i++) {
            if (stell(f) + 4 > l) {
                code2 = gs_error_ioerror;
                continue;
            }
            code2 = sfread(table, 1, 4, f);
            if (code2 < 0)
                return code2;
            if (!memcmp(table, "name", 4)) {
                uint table_pos, table_len;
                byte *namet;
                (void)sru32(f); /* skip checksum */

                table_pos = sru32(f);
                table_len = sru32(f);
                if (table_pos + table_len > l) {
                    code2 = gs_error_ioerror;
                    continue;
                }

                code2 = sfseek(f, table_pos, SEEK_SET);
                if (code2 < 0)
                    continue;

                namet = (byte *)gs_alloc_bytes(ctx->memory, table_len, "pdfi_ttf_add_to_native_map");
                if (namet == NULL)
                    return_error(gs_error_VMerror);
                code2 = sfread(namet, 1, table_len, f);
                if (code2 < 0) {
                    gs_free_object(ctx->memory, namet,"pdfi_ttf_add_to_native_map");
                    continue;
                }
                nte = u16(namet + 2);
                /* Again, arbitrary limit... */
                if (nte > 255) {
                    gs_free_object(ctx->memory, namet,"pdfi_ttf_add_to_native_map");
                    continue;
                }
                storageOffset = u16(namet + 4);

                for (j = 0; j < nte; j++) {
                    byte *rec = namet + 6 + j * 12;
                    if (u16(rec + 6) == 6) {
                        int pid = u16(rec);
                        int eid = u16(rec + 2);
                        int nl = u16(rec + 8);
                        int noffs = u16(rec + 10);

                        if (nl + noffs + storageOffset > table_len || nl >= pname_size) {
                            break;
                        }
                        memcpy(pname, namet + storageOffset + noffs, nl);
                        pname[nl] = '\0';
                        if ((pid == 0 || (pid == 2 && eid == 1) || (pid == 3 && eid == 1)) && (nl % 2) == 0) {
                            for (k = 0; k < nl; k += 2) {
                                if (pname[k] != '\0')
                                    break;
                            }
                            if (k == nl) {
                                int k1, k2;
                                for (k1 = 0, k2 = 1; k2 < nl; k1++, k2 += 2) {
                                    pname[k1] = pname[k2];
                                }
                                nl = nl >> 1;
                                pname[nl] = '\0';
                            }
                        }

                        for (k = 0; k < nl; k++)
                            if (pname[k] < 32 || pname[k] > 126) /* is it a valid PS name? */
                                break;
                        if (k == nl) {
                            code = 0;
                            break;
                       }
                    }
                }
                if (code == gs_error_undefined) {
                    for (j = 0; j < nte; j++) {
                        byte *rec = namet + 6 + j * 12;
                        if (u16(rec + 6) == 4) {
                            int pid = u16(rec);
                            int eid = u16(rec + 2);
                            int nl = u16(rec + 8);
                            int noffs = u16(rec + 10);

                            if (nl + noffs + storageOffset > table_len || nl >= pname_size) {
                                break;
                            }
                            memcpy(pname, namet + storageOffset + noffs, nl);
                            pname[nl] = '\0';
                            if ((pid == 0 || (pid == 2 && eid == 1) || (pid == 3 && eid == 1)) && (nl % 2) == 0) {
                                for (k = 0; k < nl; k += 2) {
                                    if (pname[k] != '\0')
                                        break;
                                }
                                if (k == nl) {
                                    int k1, k2;
                                    for (k1 = 0, k2 = 1; k2 < nl; k1++, k2 += 2) {
                                        pname[k1] = pname[k2];
                                    }
                                    nl = nl >> 1;
                                    pname[nl] = '\0';
                                }
                            }
                            for (k = 0; k < nl; k++)
                                if (pname[k] < 32 || pname[k] > 126) /* is it a valid PS name? */
                                    break;
                            if (k == nl)
                                code = 0;
                            break;
                        }
                    }
                }
                gs_free_object(ctx->memory, namet, "pdfi_ttf_add_to_native_map");
                break;
            }
            else {
                sfseek(f, 12, SEEK_CUR);
            }
        }
        if (code >= 0)
            code = pdfi_add__to_native_fontmap(ctx, (const char *)pname, (const char *)fname, (include_index == true ? findex : -1));
    }
    return code;
}

static const char *font_scan_skip_list[] = {
  ".afm",
  ".bat",
  ".c",
  ".cmd",
  ".com",
  ".dir",
  ".dll",
  ".doc",
  ".drv",
  ".exe",
  ".fon",
  ".fot",
  ".h",
  ".o",
  ".obj",
  ".pfm",
  ".pss",
  ".txt",
  ".gz",
  ".pcf"
};

static bool font_scan_skip_file(const char *fname)
{
    size_t l2, l = strlen(fname);
    bool skip = false;
    int i;

    for (i = 0; i < (sizeof(font_scan_skip_list)/sizeof(*font_scan_skip_list)); i++) {
        l2 = strlen(font_scan_skip_list[i]);
        if (memcmp(font_scan_skip_list[i], fname + l - l2, l2) == 0) {
            skip = true;
            break;
        }
    }
    return skip;
}

static int pdfi_add_font_to_native_map(pdf_context *ctx, const char *fp, char *working)
{
    stream *sf;
    int code = 0;
    uint nread;
    byte magic[4]; /* We only (currently) use up to 4 bytes for type guessing */
    int type;

    if (font_scan_skip_file(fp))
        return 0;

    sf = sfopen(fp, "r", ctx->memory);
    if (sf == NULL)
        return 0;
    code = sgets(sf, magic, 4, &nread);
    if (code < 0 || nread < 4) {
        code = 0;
        sfclose(sf);
        return 0;
    }

    code = sfseek(sf, 0, SEEK_SET);
    if (code < 0) {
        code = 0;
        sfclose(sf);
        return 0;
    }
    /* Slightly naff: in this one case, we want to treat OTTO fonts
       as Truetype, so we lookup the TTF 'name' table - it's more efficient
       than decoding the CFF, and probably will give more expected results
     */
    if (memcmp(magic, "OTTO", 4) == 0 || memcmp(magic, "ttcf", 4) == 0) {
        type = tt_font;
    }
    else {
        type = pdfi_font_scan_type_picker((byte *)magic, 4);
    }
    switch(type) {
        case tt_font:
          code = pdfi_ttf_add_to_native_map(ctx, sf, magic, (char *)fp, working, gp_file_name_sizeof);
          break;
        case cff_font:
              code = gs_error_undefined;
          break;
        case type1_font:
        default:
          code = pdfi_type1_add_to_native_map(ctx, sf, (char *)fp, working, gp_file_name_sizeof);
          break;
    }
    sfclose(sf);
    /* We ignore most errors, on the basis it probably means it wasn't a valid font file */
    if (code != gs_error_VMerror)
        code = 0;

    return code;
}

/* still writes into pdfnativefontmap dictionary */
static int pdfi_generate_platform_fontmap(pdf_context *ctx)
{
    void *enum_state = NULL;
    int code = 0;
    char *fontname, *path;
    char *working = NULL;

    working = (char *)gs_alloc_bytes(ctx->memory, gp_file_name_sizeof, "pdfi_generate_platform_fontmap");
    if (working == NULL) {
        code = gs_note_error(gs_error_VMerror);
        goto done;
    }

    enum_state = gp_enumerate_fonts_init(ctx->memory);
    if (enum_state == NULL) {
        code = gs_note_error(gs_error_VMerror);
        goto done;
    }

    while((code = gp_enumerate_fonts_next(enum_state, &fontname, &path )) > 0) {
        if (fontname == NULL || path == NULL) {
            continue;
        }
        code = pdfi_add_font_to_native_map(ctx, path, working);

        /* We ignore most errors, on the basis it probably means it wasn't a valid font file */
        if (code == gs_error_VMerror)
            break;
        code = 0;
    }
done:
    gp_enumerate_fonts_free(enum_state);
    gs_free_object(ctx->memory, working, "pdfi_generate_platform_fontmap");
    return code;
}

static int pdfi_generate_native_fontmap(pdf_context *ctx)
{
    file_enum *fe;
    int i, j;
    char *patrn= NULL;
    char *result = NULL;
    char *working = NULL;
    int code = 0, l;
    gs_param_string *respaths[2];
    int nrespaths[2];

    if (ctx->pdfnativefontmap != NULL) /* Only run this once */
        return 0;
    if (ctx->args.nonativefontmap == true) {
        /* Basically create an empty dictionary */
        code = pdfi_dict_alloc(ctx, 1, &ctx->pdfnativefontmap);
        if (code < 0)
            return code;
        pdfi_countup(ctx->pdfnativefontmap);
        return 0;
    }

    (void)pdfi_generate_platform_fontmap(ctx);

    patrn = (char *)gs_alloc_bytes(ctx->memory, gp_file_name_sizeof, "pdfi_generate_native_fontmap");
    result = (char *)gs_alloc_bytes(ctx->memory, gp_file_name_sizeof, "pdfi_generate_native_fontmap");
    working = (char *)gs_alloc_bytes(ctx->memory, gp_file_name_sizeof, "pdfi_generate_native_fontmap");
    if (patrn == NULL || result == NULL || working == NULL) {
        gs_free_object(ctx->memory, patrn, "pdfi_generate_native_fontmap");
        gs_free_object(ctx->memory, result, "pdfi_generate_native_fontmap");
        gs_free_object(ctx->memory, working, "pdfi_generate_native_fontmap");
        return_error(gs_error_VMerror);
    }

    respaths[0] = ctx->search_paths.font_paths;
    nrespaths[0] = ctx->search_paths.num_font_paths;
    respaths[1] = ctx->search_paths.resource_paths;
    nrespaths[1] = ctx->search_paths.num_resource_paths;

    for (j = 0; j < sizeof(respaths) / sizeof(respaths[0]); j++) {
        for (i = 0; i < nrespaths[j]; i++) {

            memcpy(patrn, respaths[j][i].data, respaths[j][i].size);
            memcpy(patrn + respaths[j][i].size, "/*", 2);
            patrn[respaths[j][i].size + 2] = '\0';

            fe = gp_enumerate_files_init(ctx->memory, (const char *)patrn, strlen(patrn));
            while ((l = gp_enumerate_files_next(ctx->memory, fe, result, gp_file_name_sizeof - 1)) != ~(uint) 0) {
                result[l] = '\0';

                code = pdfi_add_font_to_native_map(ctx, result, working);

                /* We ignore most errors, on the basis it probably means it wasn't a valid font file */
                if (code == gs_error_VMerror)
                    break;
                code = 0;
            }
            /* We only need to explicitly destroy the enumerator if we exit before enumeration is complete */
            if (code < 0)
                gp_enumerate_files_close(ctx->memory, fe);
        }
    }

#ifdef DUMP_NATIVE_FONTMAP
    if (ctx->pdfnativefontmap != NULL) {
        uint64_t ind;
        int find = -1;
        pdf_name *key = NULL;
        pdf_obj *v = NULL;
        pdf_string *val = NULL;
        (void)pdfi_dict_key_first(ctx, ctx->pdfnativefontmap, (pdf_obj **) &key, &ind);
        (void)pdfi_dict_get_by_key(ctx, ctx->pdfnativefontmap, key, (pdf_obj **)&v);
        for (j = 0; j < key->length; j++)
            dprintf1("%c", key->data[j]);
        if (pdfi_type_of(v) == PDF_DICT) {
            pdf_num *n;
            pdf_string *val2;
            code = pdfi_dict_get(ctx, (pdf_dict *)v, "Index", (pdf_obj **)&n);
            if (code >= 0 && pdfi_type_of(n) == PDF_INT)
                find = n->value.i;
            else
                code = 0;
            (void)pdfi_dict_get(ctx, (pdf_dict *)v, "Path", (pdf_obj **)&val2);
            val = val2;
        }
        else {
            val = (pdf_string *)v;
        }
        dprintf("	");
        for (j = 0; j < val->length; j++)
            dprintf1("%c", val->data[j]);
        if (find != -1) {
            dprintf1("	Index = %d", find);
            find = -1;
        }

        dprintf("\n");
        pdfi_countdown(key);
        pdfi_countdown(val);

        while (pdfi_dict_key_next(ctx, ctx->pdfnativefontmap, (pdf_obj **) &key, &ind) >= 0 && ind > 0) {
            (void)pdfi_dict_get_by_key(ctx, ctx->pdfnativefontmap, key, (pdf_obj **)&v);
            for (j = 0; j < key->length; j++)
                dprintf1("%c", key->data[j]);
            if (pdfi_type_of(v) == PDF_DICT) {
                pdf_num *n;
                pdf_string *val2;
                code = pdfi_dict_get(ctx, (pdf_dict *)v, "Index", (pdf_obj **)&n);
                if (code >= 0 && pdfi_type_of(n) == PDF_INT)
                    find = n->value.i;
                else
                    code = 0;
                (void)pdfi_dict_get(ctx, (pdf_dict *)v, "Path", (pdf_obj **)&val2);
                val = val2;
            }
            else {
                val = (pdf_string *)v;
            }
            dprintf("       ");
            for (j = 0; j < val->length; j++)
                dprintf1("%c", val->data[j]);
            if (find != -1) {
                dprintf1("	Index = %d", find);
                find = -1;
            }
            pdfi_countdown(key);
            pdfi_countdown(val);
            dprintf("\n");
        }
    }
#endif

    gs_free_object(ctx->memory, patrn, "pdfi_generate_native_fontmap");
    gs_free_object(ctx->memory, result, "pdfi_generate_native_fontmap");
    gs_free_object(ctx->memory, working, "pdfi_generate_native_fontmap");
    return 0;
}

int
pdfi_fontmap_lookup_font(pdf_context *ctx, pdf_dict *font_dict, pdf_name *fname, pdf_obj **mapname, int *findex)
{
    int code;
    pdf_obj *mname;

    *findex = 0;

    if (ctx->pdffontmap == NULL) {
        const char *fmap_default = "Fontmap.GS";
        char *fmapname = (char *)fmap_default;
        code = pdf_make_fontmap(ctx, fmapname, false);
        if (code < 0) {
            return code;
        }
    }
    if (ctx->pdfnativefontmap == NULL) {
        code = pdfi_generate_native_fontmap(ctx);
        if (code < 0)
            return code;
    }
    code = pdfi_dict_get_by_key(ctx, ctx->pdffontmap, fname, &mname);
    if (code >= 0) {
        /* Fontmap can map in multiple "jump" i.e.
           name -> substitute name
           subsitute name -> file name
           So we want to loop until we no more hits.
         */
        while(1) {
            pdf_obj *mname2;
            code = pdfi_dict_get_by_key(ctx, ctx->pdffontmap, (pdf_name *)mname, &mname2);
            if (code < 0) {
                code = 0;
                break;
            }
            pdfi_countdown(mname);
            mname = mname2;
        }
    }

    if (code < 0 && ctx->pdfnativefontmap != NULL) {
        pdf_obj *record;
        code = pdfi_dict_get_by_key(ctx, ctx->pdfnativefontmap, fname, &record);
        if (code >= 0) {
            if (pdfi_type_of(record) == PDF_STRING) {
                mname = record;
            }
            else if (pdfi_type_of(record) == PDF_DICT) {
                int64_t i64;
                code = pdfi_dict_get(ctx, (pdf_dict *)record, "Path", &mname);
                if (code >= 0)
                    code = pdfi_dict_get_int(ctx, (pdf_dict *)record, "Index", &i64);
                if (code < 0) {
                    pdfi_countdown(record);
                    return code;
                }
                *findex = (int)i64; /* Rangecheck? */
            }
            else {
                pdfi_countdown(record);
                code = gs_error_undefined;
            }
        }
    }
    else {
        code = gs_error_undefined;
    }

    if (mname != NULL && pdfi_type_of(mname) == PDF_STRING && pdfi_fmap_file_exists(ctx, (pdf_string *)mname)) {
        *mapname = mname;
        (void)pdfi_dict_put(ctx, font_dict, ".Path", mname);
        code = 0;
    }
    else if (mname != NULL && (pdfi_type_of(mname) == PDF_NAME || pdfi_type_of(mname) == PDF_FONT)) { /* If we map to a name, we assume (for now) we have the font as a "built-in" */
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
pdfi_fontmap_lookup_cidfont(pdf_context *ctx, pdf_dict *font_dict, pdf_name *name, pdf_obj **mapname, int *findex)
{
    int code = 0;
    pdf_obj *cidname = NULL;
    pdf_obj *mname;
    bool cidfmapfallback = false;

    *findex = 0;

    if (ctx->pdfcidfmap == NULL) {
        const char *cidfmap_default = "cidfmap";
        char *cidfmapname = (char *)cidfmap_default;
        code = pdf_make_fontmap(ctx, cidfmapname, true);
        if (code < 0) {
            return code;
        }
    }
    if (name == NULL || pdfi_type_of(name) != PDF_NAME) {
        code = pdfi_dict_get(ctx, font_dict, "BaseFont", &cidname);
        if (code < 0 || pdfi_type_of(cidname) != PDF_NAME) {
            pdfi_countdown(cidname);
            return_error(gs_error_undefined);
        }

    }
    else {
        cidname = (pdf_obj *)name;
        pdfi_countup(cidname);
    }

    cidfmapfallback = pdfi_name_is((pdf_name *)cidname, "CIDFallBack");

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
    if (pdfi_type_of(mname) == PDF_DICT) {
        pdf_dict *rec = (pdf_dict *)mname;
        pdf_name *filetype;
        pdf_name *path = NULL;
        pdf_array *mcsi = NULL;
        pdf_dict *ocsi = NULL;
        pdf_string *ord1 = NULL, *ord2 = NULL;
        int64_t i64;

        code = pdfi_dict_get(ctx, rec, "FileType", (pdf_obj **)&filetype);
        /* We only handle TTF files, just now */
        if (code < 0 || pdfi_type_of(filetype) != PDF_NAME || filetype->length != 8 || memcmp(filetype->data, "TrueType", 8) != 0) {
            pdfi_countdown(filetype);
            pdfi_countdown(rec);
            return_error(gs_error_undefined);
        }
        pdfi_countdown(filetype);

        code = pdfi_dict_get(ctx, rec, "CSI", (pdf_obj **)&mcsi);
        if (code < 0 || pdfi_type_of(mcsi) != PDF_ARRAY) {
            pdfi_countdown(mcsi);
            pdfi_countdown(rec);
            return_error(gs_error_undefined);
        }

        if (!cidfmapfallback) {
            code = pdfi_dict_get(ctx, font_dict, "CIDSystemInfo", (pdf_obj **)&ocsi);
            if (code < 0 || pdfi_type_of(ocsi) != PDF_DICT) {
                pdfi_countdown(ocsi);
                pdfi_countdown(mcsi);
                pdfi_countdown(rec);
                return_error(gs_error_undefined);
            }
            code = pdfi_dict_get(ctx, ocsi, "Ordering", (pdf_obj **)&ord1);
            if (code < 0 || pdfi_type_of(ord1) != PDF_STRING) {
                pdfi_countdown(ord1);
                pdfi_countdown(ocsi);
                pdfi_countdown(mcsi);
                pdfi_countdown(rec);
                return_error(gs_error_undefined);
            }
            code = pdfi_array_get(ctx, mcsi, 0, (pdf_obj **)&ord2);
            if (code < 0 || pdfi_type_of(ord2) != PDF_STRING) {
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
        }

        code = pdfi_dict_get(ctx, rec, "Path", (pdf_obj **)&path);
        if (code < 0 || pdfi_type_of(path) != PDF_STRING || !pdfi_fmap_file_exists(ctx, (pdf_string *)path)) {
            pdfi_countdown(rec);
            return_error(gs_error_undefined);
        }

        *mapname = (pdf_obj *)path;
        (void)pdfi_dict_put(ctx, font_dict, ".Path", (pdf_obj *)path);
        code = pdfi_dict_get_int(ctx, rec, "SubfontID", &i64);
        *findex = (code < 0) ? 0 : (int)i64; /* rangecheck? */
        code = 0;

    }
    else {
        *mapname = (pdf_obj *)mname;
        code = 0;
    }

    return code;
}
