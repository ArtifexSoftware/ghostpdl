/* Copyright (C) 2001-2024 Artifex Software, Inc.
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


/* Output utilities for PDF-writing driver */
#include "memory_.h"
#include "jpeglib_.h"		/* for sdct.h */
#include "gx.h"
#include "gserrors.h"
#include "gscdefs.h"
#include "gsdsrc.h"
#include "gsfunc.h"
#include "gsfunc3.h"
#include "gdevpdfx.h"
#include "gdevpdfo.h"
#include "gdevpdfg.h"
#include "gdevpdtd.h"
#include "strimpl.h"
#include "sa85x.h"
#include "scfx.h"
#include "sdct.h"
#include "slzwx.h"
#include "spngpx.h"
#include "srlx.h"
#include "sarc4.h"
#include "smd5.h"
#include "sstring.h"
#include "strmio.h"
#include "szlibx.h"
#include "gsagl.h"

#include "opdfread.h"
#include "gs_mgl_e.h"
#include "gs_mro_e.h"

extern single_glyph_list_t SingleGlyphList[];

    /* Define the size of internal stream buffers. */
/* (This is not a limitation, it only affects performance.) */
#define sbuf_size 512

/* Optionally substitute other filters for FlateEncode for debugging. */
#if 1
#  define compression_filter_name "FlateDecode"
#  define compression_filter_template s_zlibE_template
#  define compression_filter_state stream_zlib_state
#else
#  define compression_filter_name "LZWDecode"
#  define compression_filter_template s_LZWE_template
#  define compression_filter_state stream_LZW_state
#endif

/* Import procedures for writing filter parameters. */
extern stream_state_proc_get_params(s_DCTE_get_params, stream_DCT_state);
extern stream_state_proc_get_params(s_CF_get_params, stream_CF_state);

#define CHECK(expr)\
  BEGIN if ((code = (expr)) < 0) return code; END

/* GC descriptors */
extern_st(st_pdf_color_space);
extern_st(st_pdf_font_resource);
extern_st(st_pdf_char_proc);
extern_st(st_pdf_font_descriptor);
public_st_pdf_resource();
private_st_pdf_x_object();
private_st_pdf_pattern();

/* ---------------- Utilities ---------------- */

#ifdef PS2WRITE_USES_ROMFS
/*
 * Strip whitespace and comments from a line of PostScript code as possible.
 * Return a pointer to any string that remains, or NULL if none.
 * Note that this may store into the string.
 */
/* This function copied from geninit.c . */
static char *
doit(char *line, bool intact)
{
    char *str = line;
    char *from;
    char *to;
    int in_string = 0;

    if (intact)
        return str;
    while (*str == ' ' || *str == '\t')		/* strip leading whitespace */
        ++str;
    if (*str == 0)		/* all whitespace */
        return NULL;
    if (!strncmp(str, "%END", 4))	/* keep these for .skipeof */
        return str;
    if (str[0] == '%')    /* comment line */
        return NULL;
    /*
     * Copy the string over itself removing:
     *  - All comments not within string literals;
     *  - Whitespace adjacent to '[' ']' '{' '}';
     *  - Whitespace before '/' '(' '<';
     *  - Whitespace after ')' '>'.
     */
    for (to = from = str; (*to = *from) != 0; ++from, ++to) {
        switch (*from) {
            case '%':
                if (!in_string)
                    break;
                continue;
            case ' ':
            case '\t':
                if (to > str && !in_string && strchr(" \t>[]{})", to[-1]))
                    --to;
                continue;
            case '(':
            case '<':
            case '/':
            case '[':
            case ']':
            case '{':
            case '}':
                if (to > str && !in_string && strchr(" \t", to[-1]))
                    *--to = *from;
                if (*from == '(')
                    ++in_string;
                continue;
            case ')':
                --in_string;
                continue;
            case '\\':
                if (from[1] == '\\' || from[1] == '(' || from[1] == ')')
                    *++to = *++from;
                continue;
            default:
                continue;
        }
        break;
    }
    /* Strip trailing whitespace. */
    while (to > str && (to[-1] == ' ' || to[-1] == '\t'))
        --to;
    *to = 0;
    return str;
}

static int
copy_ps_file_stripping_all(stream *s, const char *fname, bool HaveTrueTypes)
{
    stream *f;
    char buf[1024], *p, *q  = buf;
    int n, l = 0, m = sizeof(buf) - 1, outl = 0;
    bool skipping = false;

    f = sfopen(fname, "r", s->memory);
    if (f == NULL)
        return_error(gs_error_undefinedfilename);
    n = sfread(buf, 1, m, f);
    buf[n] = 0;
    do {
        if (*q == '\r' || *q == '\n') {
            q++;
            continue;
        }
        p = strchr(q, '\r');
        if (p == NULL)
            p = strchr(q, '\n');
        if (p == NULL) {
            if (n < m)
                p = buf + n;
            else {
                strcpy(buf, q);
                l = strlen(buf);
                m = sizeof(buf) - 1 - l;
                if (!m) {
                    sfclose(f);
                    emprintf1(s->memory,
                              "The procset %s contains a too long line.",
                              fname);
                    return_error(gs_error_ioerror);
                }
                n = sfread(buf + l, 1, m, f);
                n += l;
                m += l;
                buf[n] = 0;
                q = buf;
                continue;
            }
        }
        *p = 0;
        if (q[0] == '%')
            l = 0;
        else {
            q = doit(q, false);
            if (q == NULL)
                l = 0;
            else
                l = strlen(q);
        }
        if (l) {
            if (!HaveTrueTypes && !strcmp("%%beg TrueType", q))
                skipping = true;
            if (!skipping) {
                outl += l + 1;
                if (outl > 100) {
                    q[l] = '\r';
                    outl = 0;
                } else
                    q[l] = ' ';
                stream_write(s, q, l + 1);
            }
            if (!HaveTrueTypes && !strcmp("%%end TrueType", q))
                skipping = false;
        }
        q = p + 1;
    } while (n == m || q < buf + n);
    if (outl)
        stream_write(s, "\r", 1);
    sfclose(f);
    return 0;
}

static int
copy_ps_file_strip_comments(stream *s, const char *fname, bool HaveTrueTypes)
{
    stream *f;
    char buf[1024], *p, *q  = buf;
    int n, l = 0, m = sizeof(buf) - 1, outl = 0;
    bool skipping = false;

    f = sfopen(fname, "r", s->memory);
    if (f == NULL)
        return_error(gs_error_undefinedfilename);
    n = sfread(buf, 1, m, f);
    buf[n] = 0;
    do {
        if (*q == '\r' || *q == '\n') {
            q++;
            continue;
        }
        p = strchr(q, '\r');
        if (p == NULL)
            p = strchr(q, '\n');
        if (p == NULL) {
            if (n < m)
                p = buf + n;
            else {
                strcpy(buf, q);
                l = strlen(buf);
                m = sizeof(buf) - 1 - l;
                if (!m) {
                    sfclose(f);
                    emprintf1(s->memory,
                              "The procset %s contains a too long line.",
                              fname);
                    return_error(gs_error_ioerror);
                }
                n = sfread(buf + l, 1, m, f);
                n += l;
                m += l;
                buf[n] = 0;
                q = buf;
                continue;
            }
        }
        *p = 0;
        if (q[0] == '%')
            l = 0;
        else {
            q = doit(q, false);
            if (q == NULL)
                l = 0;
            else
                l = strlen(q);
        }
        if (l) {
            if (!HaveTrueTypes && !strcmp("%%beg TrueType", q))
                skipping = true;
            if (!skipping) {
                outl += l + 1;
                if (outl > 100) {
                    q[l] = '\n';
                    outl = 0;
                } else
                    q[l] = '\n';
                stream_write(s, q, l + 1);
            }
            if (!HaveTrueTypes && !strcmp("%%end TrueType", q))
                skipping = false;
        }
        q = p + 1;
    } while (n == m || q < buf + n);
    if (outl)
        stream_write(s, "\r", 1);
    sfclose(f);
    return 0;
}
#endif

static int write_opdfread(stream *s)
{
    int index = 0;

    do {
        if (opdfread_ps[index] == 0x00)
            break;
        stream_write(s, opdfread_ps[index], strlen(opdfread_ps[index]));
        index++;
    } while (1);
    return 0;
}

static int write_tt_encodings(stream *s, bool HaveTrueTypes)
{
    int index = 0;

    do {
        if (gs_mro_e_ps[index] == 0x00)
            break;
        stream_write(s, gs_mro_e_ps[index], strlen(gs_mro_e_ps[index]));
        index++;
    } while (1);

    if (HaveTrueTypes) {
        char Buffer[256];
        single_glyph_list_t *entry = SingleGlyphList;

        gs_snprintf(Buffer, sizeof(Buffer), "/AdobeGlyphList mark\n");
        stream_write(s, Buffer, strlen(Buffer));
        while (entry->Glyph) {
            gs_snprintf(Buffer, sizeof(Buffer), "/%s 16#%04x\n", entry->Glyph, entry->Unicode);
            stream_write(s, Buffer, strlen(Buffer));
            entry++;
        };
        gs_snprintf(Buffer, sizeof(Buffer), ".dicttomark readonly def\n");
        stream_write(s, Buffer, strlen(Buffer));

        index = 0;
        do {
            if (gs_mgl_e_ps[index] == 0x00)
                break;
            stream_write(s, gs_mgl_e_ps[index], strlen(gs_mgl_e_ps[index]));
            index++;
        } while (1);
    }
    return 0;
}

static int
copy_procsets(stream *s, bool HaveTrueTypes, bool stripping)
{
    int code;

    code = write_opdfread(s);
    if (code < 0)
        return code;

    code = write_tt_encodings(s, HaveTrueTypes);
    return code;

}

static int
encode(stream **s, const stream_template *t, gs_memory_t *mem)
{
    stream_state *st = s_alloc_state(mem, t->stype, "pdfwrite_pdf_open_document.encode");

    if (st == 0)
        return_error(gs_error_VMerror);
    if (t->set_defaults)
        t->set_defaults(st);
    if (s_add_filter(s, t, st, mem) == 0) {
        gs_free_object(mem, st, "pdfwrite_pdf_open_document.encode");
        return_error(gs_error_VMerror);
    }
    return 0;
}

/* ------ Document ------ */

/* Write out the arguments used to invoke GS as a comment in the PDF/PS
 * file. We don't write out paths, passwords, or any unrecognised string
 * parameters (all sanitised by the arg code) for privacy/security
 * reasons. This routine is only called by the PDF linearisation code.
 */
int
pdfwrite_fwrite_args_comment(gx_device_pdf *pdev, gp_file *f)
{
    const char * const *argv = NULL;
    const char *arg;
    int towrite, length, i, j, argc;

    argc = gs_lib_ctx_get_args(pdev->memory->gs_lib_ctx, &argv);

    gp_fwrite("%%Invocation:", 13, 1, f);
    length = 12;
    for (i=0;i < argc; i++) {
        arg = argv[i];

        if ((strlen(arg) + length) > 255) {
            gp_fwrite("\n%%+ ", 5, 1, f);
            length = 5;
        } else {
            gp_fwrite(" ", 1, 1, f);
            length++;
        }

        if (strlen(arg) > 250)
            towrite = 250;
        else
            towrite = strlen(arg);

        length += towrite;

        for (j=0;j < towrite;j++) {
            if (arg[j] == 0x0A) {
                gp_fwrite("<0A>", 4, 1, f);
            } else {
                if (arg[j] == 0x0D) {
                    gp_fwrite("<0D>", 4, 1, f);
                } else {
                    gp_fwrite(&arg[j], 1, 1, f);
                }
            }
        }
    }
    gp_fwrite("\n", 1, 1, f);
    return 0;
}

/* Exactly the same as pdfwrite_fwrite_args_comment() above, but uses a stream
 * instead of a gp_file *, because of course we aren't consistent.... This
 * routine is called by the regular PDF or PS file output code.
 */
int
pdfwrite_write_args_comment(gx_device_pdf *pdev, stream *s)
{
    const char * const *argv = NULL;
    const char *arg;
    int towrite, length, i, j, argc;

    argc = gs_lib_ctx_get_args(pdev->memory->gs_lib_ctx, &argv);

    stream_write(s, (byte *)"%%Invocation:", 13);
    length = 12;
    for (i=0;i < argc; i++) {
        arg = argv[i];

        if ((strlen(arg) + length) > 255) {
            stream_write(s, (byte *)"\n%%+ ", 5);
            length = 5;
        } else {
            stream_write(s, (byte *)" ", 1);
            length++;
        }

        if (strlen(arg) > 250)
            towrite = 250;
        else
            towrite = strlen(arg);

        length += towrite;

        for (j=0;j < towrite;j++) {
            if (arg[j] == 0x0A) {
                stream_write(s, (byte *)"<0A>", 4);
            } else {
                if (arg[j] == 0x0D) {
                    stream_write(s, (byte *)"<0D>", 4);
                } else {
                    stream_write(s, (byte *)&arg[j], 1);
                }
            }
        }
    }
    stream_write(s, (byte *)"\n", 1);
    return 0;
}

int ps2write_dsc_header(gx_device_pdf * pdev, int pages)
{
    stream *s = pdev->strm;

    if (pdev->ForOPDFRead) {
        char cre_date_time[41];
        int code, status, cre_date_time_len;
        char BBox[256];

        if (pdev->Eps2Write)
            stream_write(s, (byte *)"%!PS-Adobe-3.0 EPSF-3.0\n", 24);
        else
            stream_write(s, (byte *)"%!PS-Adobe-3.0\n", 15);
        pdfwrite_write_args_comment(pdev, s);
        /* We need to calculate the document BoundingBox which is a 'high water'
         * mark derived from the BoundingBox of all the individual pages.
         */
        {
            int pagecount = 1, j;
            double urx=0, ury=0;

            for (j = 0; j < NUM_RESOURCE_CHAINS; ++j) {
                pdf_resource_t *pres = pdev->resources[resourcePage].chains[j];

                for (; pres != 0; pres = pres->next)
                    if ((!pres->named || pdev->ForOPDFRead)
                        && !pres->object->written) {
                        pdf_page_t *page = &pdev->pages[pagecount - 1];
                        if (ceil(page->MediaBox.x) > urx)
                            urx = ceil(page->MediaBox.x);
                        if (ceil(page->MediaBox.y) > ury)
                            ury = ceil(page->MediaBox.y);
                        pagecount++;
                    }
            }
            if (!pdev->Eps2Write || pdev->BBox.p.x > pdev->BBox.q.x || pdev->BBox.p.y > pdev->BBox.q.y)
                gs_snprintf(BBox, sizeof(BBox), "%%%%BoundingBox: 0 0 %d %d\n", (int)urx, (int)ury);
            else
                gs_snprintf(BBox, sizeof(BBox), "%%%%BoundingBox: %d %d %d %d\n", (int)floor(pdev->BBox.p.x), (int)floor(pdev->BBox.p.y), (int)ceil(pdev->BBox.q.x), (int)ceil(pdev->BBox.q.y));
            stream_write(s, (byte *)BBox, strlen(BBox));
            if (!pdev->Eps2Write || pdev->BBox.p.x > pdev->BBox.q.x || pdev->BBox.p.y > pdev->BBox.q.y)
                gs_snprintf(BBox, sizeof(BBox), "%%%%HiResBoundingBox: 0 0 %.2f %.2f\n", urx, ury);
            else
                gs_snprintf(BBox, sizeof(BBox), "%%%%HiResBoundingBox: %.2f %.2f %.2f %.2f\n", pdev->BBox.p.x, pdev->BBox.p.y, pdev->BBox.q.x, pdev->BBox.q.y);
            stream_write(s, (byte *)BBox, strlen(BBox));
        }
        cre_date_time_len = pdf_get_docinfo_item(pdev, "/CreationDate", cre_date_time, sizeof(cre_date_time) - 1);
        cre_date_time[cre_date_time_len] = 0;
        gs_snprintf(BBox, sizeof(BBox), "%%%%Creator: %s %d (%s)\n", gs_product, (int)gs_revision,
                pdev->dname);
        stream_write(s, (byte *)BBox, strlen(BBox));
        stream_puts(s, "%%LanguageLevel: 2\n");
        gs_snprintf(BBox, sizeof(BBox), "%%%%CreationDate: %s\n", cre_date_time);
        stream_write(s, (byte *)BBox, strlen(BBox));
        gs_snprintf(BBox, sizeof(BBox), "%%%%Pages: %d\n", pages);
        stream_write(s, (byte *)BBox, strlen(BBox));
        gs_snprintf(BBox, sizeof(BBox), "%%%%EndComments\n");
        stream_write(s, (byte *)BBox, strlen(BBox));
        gs_snprintf(BBox, sizeof(BBox), "%%%%BeginProlog\n");
        stream_write(s, (byte *)BBox, strlen(BBox));
        if (pdev->params.CompressPages) {
            /*  When CompressEntireFile is true and ASCII85EncodePages is false,
                the ASCII85Encode filter is applied, rather one may expect the opposite.
                Keeping it so due to no demand for this mode.
                A right implementation should compute the length of the compressed procset,
                write out an invocation of SubFileDecode filter, and write the length to
                there assuming the output file is positionable. */
            stream_write(s, (byte *)"currentfile /ASCII85Decode filter /LZWDecode filter cvx exec\n", 61);
            code = encode(&s, &s_A85E_template, pdev->pdf_memory);
            if (code < 0)
                return code;
            code = encode(&s, &s_LZWE_template, pdev->pdf_memory);
            if (code < 0)
                return code;
        }
        stream_puts(s, "10 dict dup begin\n");
        stream_puts(s, "/DSC_OPDFREAD true def\n");
        if (pdev->Eps2Write) {
            stream_puts(s, "/SetPageSize false def\n");
            stream_puts(s, "/EPS2Write true def\n");
        } else {
            if (pdev->SetPageSize)
                stream_puts(s, "/SetPageSize true def\n");
            stream_puts(s, "/EPS2Write false def\n");
        }
        stream_puts(s, "end\n");

        code = copy_procsets(s, pdev->HaveTrueTypes, false);
        if (code < 0)
            return code;
        status = s_close_filters(&s, pdev->strm);
        if (status < 0)
            return_error(gs_error_ioerror);
        stream_puts(s, "\n");
        pdev->OPDFRead_procset_length = (int)stell(s);
    }
    return 0;
}

/* Open the document if necessary. */
int
pdfwrite_pdf_open_document(gx_device_pdf * pdev)
{
    if (!pdev->strm)
        return_error(gs_error_ioerror);

    if (!is_in_page(pdev) && pdf_stell(pdev) == 0) {
        stream *s = pdev->strm;
        int level = (int)(pdev->CompatibilityLevel * 10 + 0.5);

        pdev->binary_ok = !pdev->params.ASCII85EncodePages;
        if (pdev->ForOPDFRead) {
            int code, status;
            char BBox[256];
            int width = (int)(pdev->width * 72.0 / pdev->HWResolution[0] + 0.5);
            int height = (int)(pdev->height * 72.0 / pdev->HWResolution[1] + 0.5);

            if (pdev->ProduceDSC)
                pdev->CompressEntireFile = 0;
            else {
                stream_write(s, (byte *)"%!\r", 3);
                gs_snprintf(BBox, sizeof(BBox), "%%%%BoundingBox: 0 0 %d %d\n", width, height);
                stream_write(s, (byte *)BBox, strlen(BBox));
                if (pdev->params.CompressPages || pdev->CompressEntireFile) {
                    /*  When CompressEntireFile is true and ASCII85EncodePages is false,
                        the ASCII85Encode filter is applied, rather one may expect the opposite.
                        Keeping it so due to no demand for this mode.
                        A right implementation should compute the length of the compressed procset,
                        write out an invocation of SubFileDecode filter, and write the length to
                        there assuming the output file is positionable. */
                    stream_write(s, (byte *)"currentfile /ASCII85Decode filter /LZWDecode filter cvx exec\n", 61);
                    code = encode(&s, &s_A85E_template, pdev->pdf_memory);
                    if (code < 0)
                        return code;
                    code = encode(&s, &s_LZWE_template, pdev->pdf_memory);
                    if (code < 0)
                        return code;
                }
                stream_puts(s, "10 dict dup begin\n");
                stream_puts(s, "/DSC_OPDFREAD false def\n");
                code = copy_procsets(s, pdev->HaveTrueTypes, true);
                if (code < 0)
                    return code;
                if (!pdev->CompressEntireFile) {
                    status = s_close_filters(&s, pdev->strm);
                    if (status < 0)
                        return_error(gs_error_ioerror);
                } else
                    pdev->strm = s;
                if (!pdev->Eps2Write)
                    stream_puts(s, "/EPS2Write false def\n");
                if(pdev->SetPageSize)
                    stream_puts(s, "/SetPageSize true def\n");
                if(pdev->RotatePages)
                    stream_puts(s, "/RotatePages true def\n");
                if(pdev->FitPages)
                    stream_puts(s, "/FitPages true def\n");
                if(pdev->CenterPages)
                    stream_puts(s, "/CenterPages true def\n");
                stream_puts(s, "end\n");
                pdev->OPDFRead_procset_length = stell(s);
            }
        }
        if (!(pdev->ForOPDFRead)) {
            pprintd2(s, "%%PDF-%d.%d\n", level / 10, level % 10);
            if (pdev->binary_ok)
                stream_puts(s, "%\307\354\217\242\n");
            pdfwrite_write_args_comment(pdev, s);
        }
    }
    /*
     * Determine the compression method.  Currently this does nothing.
     * It also isn't clear whether the compression method can now be
     * changed in the course of the document.
     *
     * Flate compression is available starting in PDF 1.2.  Since we no
     * longer support any older PDF versions, we ignore UseFlateCompression
     * and always use Flate compression.
     */
    if (!pdev->params.CompressPages)
        pdev->compression = pdf_compress_none;
    else
        pdev->compression = pdf_compress_Flate;
    return 0;
}

/* ------ Objects ------ */

/* Allocate an object ID. */
static int64_t
pdf_next_id(gx_device_pdf * pdev)
{
    return (pdev->next_id)++;
}

/*
 * Return the current position in the output.  Note that this may be in the
 * main output file, the asides file, or the ObjStm file.  If the current
 * file is the ObjStm file, positions returned by pdf_stell must only be
 * used locally (for computing lengths or patching), since there is no way
 * to map them later to the eventual position in the output file.
 */
gs_offset_t
pdf_stell(gx_device_pdf * pdev)
{
    stream *s = pdev->strm;
    gs_offset_t pos = stell(s);

    if (s == pdev->asides.strm)
        pos += ASIDES_BASE_POSITION;
    return pos;
}

/* Allocate an ID for a future object.
 * pdf_obj_ref below allocates an object and assigns it a position assuming
 * it will be written at the current location in the PDF file. But we want
 * some way to notice when writing the PDF file if some kinds of objects have
 * never been written out (eg pages allocated for /Dest targets). Setting the
 * position to 0 is good, because we always write the header, which is 15
 * bytes. However, pdf_obj_ref is so wisely used its no longer possible to
 * tell whether writing the object out has been deferred (in which case the
 * pos is updated by pdf_open_obj) or not. Adding this function to allow us
 * to create an object whose writing is definitely deferred, in which case
 * we know it will be updated later. This allows setting the pos to 0,
 * and we can detect that when writing the xref, and set the object to
 * 'unused'.
 */
int64_t pdf_obj_forward_ref(gx_device_pdf * pdev)
{
    int64_t id = pdf_next_id(pdev);
    gs_offset_t pos = 0;

    if (pdev->doubleXref) {
        gp_fwrite(&pos, sizeof(pos), 1, pdev->xref.file);
        gp_fwrite(&pos, sizeof(pos), 1, pdev->xref.file);
    }
    else
        gp_fwrite(&pos, sizeof(pos), 1, pdev->xref.file);
    return id;
}

/* Allocate an ID for a future object. */
int64_t
pdf_obj_ref(gx_device_pdf * pdev)
{
    int64_t id = pdf_next_id(pdev);
    gs_offset_t pos = 0;

    if (pdev->doubleXref) {
        if (pdev->strm == pdev->ObjStm.strm)
            pos = pdev->ObjStm_id;
        else
            pos = 0;
        gp_fwrite(&pos, sizeof(pos), 1, pdev->xref.file);
        if (pdev->strm == pdev->ObjStm.strm)
            pos = pdev->NumObjStmObjects;
        else
            pos = pdf_stell(pdev);
        gp_fwrite(&pos, sizeof(pos), 1, pdev->xref.file);
    }
    else {
        pos = pdf_stell(pdev);
        gp_fwrite(&pos, sizeof(pos), 1, pdev->xref.file);
    }
    return id;
}

/* Set the offset in the xref table for an object to zero. This
 * means that whenwritingthe xref we will mark the object as 'unused'.
 * This is primarily of use when we encounter an error writing an object,
 * we want to elide the entry from the xref in order to not write a
 * broken PDF file. Of course, the missing object may still produce
 * a broken PDF file (more subtly broken), but because the PDF interpreter
 * generally doesn't stop if we signal an error, we try to avoid grossly
 * broken PDF files this way.
 */
int64_t
pdf_obj_mark_unused(gx_device_pdf *pdev, int64_t id)
{
    gp_file *tfile = pdev->xref.file;
    int64_t tpos = gp_ftell(tfile);
    gs_offset_t pos = 0;

    if (pdev->doubleXref) {
        if (gp_fseek(tfile, ((int64_t)(id - pdev->FirstObjectNumber)) * sizeof(pos) * 2,
              SEEK_SET) != 0)
            return_error(gs_error_ioerror);
        gp_fwrite(&pos, sizeof(pos), 1, tfile);
    }
    else {
        if (gp_fseek(tfile, ((int64_t)(id - pdev->FirstObjectNumber)) * sizeof(pos),
              SEEK_SET) != 0)
            return_error(gs_error_ioerror);
    }

    gp_fwrite(&pos, sizeof(pos), 1, tfile);
    if (gp_fseek(tfile, tpos, SEEK_SET) != 0)
        return_error(gs_error_ioerror);
    return 0;
}

/* Begin an object, optionally allocating an ID. */
int64_t
pdf_open_obj(gx_device_pdf * pdev, int64_t id, pdf_resource_type_t type)
{
    stream *s = pdev->strm;

    if (s == NULL)
        return_error(gs_error_ioerror);

    if (id <= 0) {
        id = pdf_obj_ref(pdev);
    } else {
        gs_offset_t pos = pdf_stell(pdev),fake_pos = 0;
        gp_file *tfile = pdev->xref.file;
        int64_t tpos = gp_ftell(tfile);

        if (pdev->doubleXref) {
            if (gp_fseek(tfile, ((int64_t)(id - pdev->FirstObjectNumber)) * sizeof(pos) * 2,
                  SEEK_SET) != 0)
	            return_error(gs_error_ioerror);
            if (pdev->strm == pdev->ObjStm.strm)
                fake_pos = pdev->ObjStm_id;
            gp_fwrite(&fake_pos, sizeof(fake_pos), 1, pdev->xref.file);
            if (pdev->strm == pdev->ObjStm.strm)
                pos = pdev->NumObjStmObjects;
            gp_fwrite(&pos, sizeof(pos), 1, pdev->xref.file);
        } else {
            if (gp_fseek(tfile, ((int64_t)(id - pdev->FirstObjectNumber)) * sizeof(pos),
                  SEEK_SET) != 0)
	            return_error(gs_error_ioerror);
            gp_fwrite(&pos, sizeof(pos), 1, tfile);
        }
        if (gp_fseek(tfile, tpos, SEEK_SET) != 0)
	        return_error(gs_error_ioerror);
    }
    if (pdev->ForOPDFRead && pdev->ProduceDSC) {
        switch(type) {
            case resourceNone:
                /* Used when outputting usage of a previously defined resource
                 * Does not want comments around its use
                 */
                break;
            case resourcePage:
                /* We *don't* want resource comments around pages */
                break;
            case resourceColorSpace:
                pprintld1(s, "%%%%BeginResource: file (PDF Color Space obj_%ld)\n", id);
                break;
            case resourceExtGState:
                pprintld1(s, "%%%%BeginResource: file (PDF Extended Graphics State obj_%ld)\n", id);
                break;
            case resourcePattern:
                pprintld1(s, "%%%%BeginResource: pattern (PDF Pattern obj_%ld)\n", id);
                break;
            case resourceShading:
                pprintld1(s, "%%%%BeginResource: file (PDF Shading obj_%ld)\n", id);
                break;
            case resourceCIDFont:
            case resourceFont:
                /* Ought to write the font name here */
                pprintld1(s, "%%%%BeginResource: procset (PDF Font obj_%ld)\n", id);
                break;
            case resourceCharProc:
                pprintld1(s, "%%%%BeginResource: file (PDF CharProc obj_%ld)\n", id);
                break;
            case resourceCMap:
                pprintld1(s, "%%%%BeginResource: file (PDF CMap obj_%ld)\n", id);
                break;
            case resourceFontDescriptor:
                pprintld1(s, "%%%%BeginResource: file (PDF FontDescriptor obj_%ld)\n", id);
                break;
            case resourceGroup:
                pprintld1(s, "%%%%BeginResource: file (PDF Group obj_%ld)\n", id);
                break;
            case resourceFunction:
                pprintld1(s, "%%%%BeginResource: file (PDF Function obj_%ld)\n", id);
                break;
            case resourceEncoding:
                pprintld1(s, "%%%%BeginResource: encoding (PDF Encoding obj_%ld)\n", id);
                break;
            case resourceCIDSystemInfo:
                pprintld1(s, "%%%%BeginResource: file (PDF CIDSystemInfo obj_%ld)\n", id);
                break;
            case resourceHalftone:
                pprintld1(s, "%%%%BeginResource: file (PDF Halftone obj_%ld)\n", id);
                break;
            case resourceLength:
                pprintld1(s, "%%%%BeginResource: file (PDF Length obj_%ld)\n", id);
                break;
            case resourceSoftMaskDict:
                /* This should not be possible, not valid in PostScript */
                pprintld1(s, "%%%%BeginResource: file (PDF SoftMask obj_%ld)\n", id);
                break;
            case resourceXObject:
                /* This should not be possible, we write these inline */
                pprintld1(s, "%%%%BeginResource: file (PDF XObject obj_%ld)\n", id);
                break;
            case resourceStream:
                /* Possibly we should not add comments to this type */
                pprintld1(s, "%%%%BeginResource: file (PDF stream obj_%ld)\n", id);
                break;
            case resourceOutline:
                /* This should not be possible, not valid in PostScript */
                pprintld1(s, "%%%%BeginResource: file (PDF Outline obj_%ld)\n", id);
                break;
            case resourceArticle:
                /* This should not be possible, not valid in PostScript */
                pprintld1(s, "%%%%BeginResource: file (PDF Article obj_%ld)\n", id);
                break;
            case resourceDests:
                /* This should not be possible, not valid in PostScript */
                pprintld1(s, "%%%%BeginResource: file (PDF Dests obj_%ld)\n", id);
                break;
            case resourceEmbeddedFiles:
                /* This should not be possible, not valid in PostScript */
                pprintld1(s, "%%%%BeginResource: file (PDF EmbeddedFiles obj_%ld)\n", id);
                break;
            case resourceLabels:
                /* This should not be possible, not valid in PostScript */
                pprintld1(s, "%%%%BeginResource: file (PDF Page Labels obj_%ld)\n", id);
                break;
            case resourceThread:
                /* This should not be possible, not valid in PostScript */
                pprintld1(s, "%%%%BeginResource: file (PDF Thread obj_%ld)\n", id);
                break;
            case resourceCatalog:
                /* This should not be possible, not valid in PostScript */
                pprintld1(s, "%%%%BeginResource: file (PDF Catalog obj_%ld)\n", id);
                break;
            case resourceEncrypt:
                /* This should not be possible, not valid in PostScript */
                pprintld1(s, "%%%%BeginResource: file (PDF Encryption obj_%ld)\n", id);
                break;
            case resourcePagesTree:
                /* This should not be possible, not valid in PostScript */
                pprintld1(s, "%%%%BeginResource: file (PDF Pages Tree obj_%ld)\n", id);
                break;
            case resourceMetadata:
                /* This should not be possible, not valid in PostScript */
                pprintld1(s, "%%%%BeginResource: file (PDF Metadata obj_%ld)\n", id);
                break;
            case resourceICC:
                /* This should not be possible, not valid in PostScript */
                pprintld1(s, "%%%%BeginResource: file (PDF ICC Profile obj_%ld)\n", id);
                break;
            case resourceAnnotation:
                /* This should not be possible, not valid in PostScript */
                pprintld1(s, "%%%%BeginResource: file (PDF Annotation obj_%ld)\n", id);
                break;
            case resourceFontFile:
                pprintld1(s, "%%%%BeginResource: file (PDF FontFile obj_%ld)\n", id);
                break;
            default:
                pprintld1(s, "%%%%BeginResource: file (PDF object obj_%ld)\n", id);
                break;
        }
    }
    if (!pdev->WriteObjStms || pdev->strm != pdev->ObjStm.strm)
        pprintld1(s, "%ld 0 obj\n", id);
    return id;
}
int64_t
pdf_begin_obj(gx_device_pdf * pdev, pdf_resource_type_t type)
{
    return pdf_open_obj(pdev, 0L, type);
}

/* End an object. */
int
pdf_end_obj(gx_device_pdf * pdev, pdf_resource_type_t type)
{
    if (!pdev->WriteObjStms || pdev->strm != pdev->ObjStm.strm)
        stream_puts(pdev->strm, "endobj\n");
    if (pdev->ForOPDFRead && pdev->ProduceDSC) {
        switch(type) {
            case resourcePage:
                break;
            default:
            stream_puts(pdev->strm, "%%EndResource\n");
            break;
        }
    }
    return 0;
}

/* ------ Page contents ------ */

/* Handle transitions between contexts. */
static int
    none_to_stream(gx_device_pdf *), stream_to_text(gx_device_pdf *),
    string_to_text(gx_device_pdf *), text_to_stream(gx_device_pdf *),
    stream_to_none(gx_device_pdf *);
typedef int (*context_proc) (gx_device_pdf *);
static const context_proc context_procs[4][4] =
{
    {0, none_to_stream, none_to_stream, none_to_stream},
    {stream_to_none, 0, stream_to_text, stream_to_text},
    {text_to_stream, text_to_stream, 0, 0},
    {string_to_text, string_to_text, string_to_text, 0}
};

/* Compute an object encryption key. */
static int
pdf_object_key(const gx_device_pdf * pdev, gs_id object_id, byte key[16])
{
    gs_md5_state_t md5;
    gs_md5_byte_t zero[2] = {0, 0}, t;
    int KeySize = pdev->KeyLength / 8;

    gs_md5_init(&md5);
    gs_md5_append(&md5, pdev->EncryptionKey, KeySize);
    t = (byte)(object_id >>  0);  gs_md5_append(&md5, &t, 1);
    t = (byte)(object_id >>  8);  gs_md5_append(&md5, &t, 1);
    t = (byte)(object_id >> 16);  gs_md5_append(&md5, &t, 1);
    gs_md5_append(&md5, zero, 2);
    gs_md5_finish(&md5, key);
    return min(KeySize + 5, 16);
}

/* Initialize encryption. */
int
pdf_encrypt_init(const gx_device_pdf * pdev, gs_id object_id, stream_arcfour_state *psarc4)
{
    byte key[16];

    return s_arcfour_set_key(psarc4, key, pdf_object_key(pdev, object_id, key));
}

/* Add the encryption filter. */
int
pdf_begin_encrypt(gx_device_pdf * pdev, stream **s, gs_id object_id)
{
    gs_memory_t *mem = pdev->v_memory;
    stream_arcfour_state *ss;
    gs_md5_byte_t key[16];
    int code, keylength;

    if (!pdev->KeyLength)
        return 0;
    keylength = pdf_object_key(pdev, object_id, key);
    ss = gs_alloc_struct(mem, stream_arcfour_state,
                    s_arcfour_template.stype, "psdf_encrypt");
    if (ss == NULL)
        return_error(gs_error_VMerror);
    code = s_arcfour_set_key(ss, key, keylength);
    if (code < 0)
        return code;
    if (s_add_filter(s, &s_arcfour_template, (stream_state *)ss, mem) == 0)
        return_error(gs_error_VMerror);
    return 0;
    /* IMPORTANT NOTE :
       We don't encrypt streams written into temporary files,
       because they can be used for comparizon
       (for example, for merging equal images).
       Instead that the encryption is applied in pdf_copy_data,
       when the stream is copied to the output file.
     */
}

/* Enter stream context. */
static int
none_to_stream(gx_device_pdf * pdev)
{
    stream *s;
    int code;

    if (pdev->contents_id != 0)
        return_error(gs_error_Fatal);	/* only 1 contents per page */
    pdev->compression_at_page_start = pdev->compression;
    if (pdev->ResourcesBeforeUsage) {
        pdf_resource_t *pres;

        code = pdf_enter_substream(pdev, resourcePage, gs_no_id, &pres,
                    true, pdev->params.CompressPages);
        if (code < 0)
            return code;
        pdev->contents_id = pres->object->id;
        pdev->contents_length_id = gs_no_id; /* inapplicable */
        pdev->contents_pos = -1; /* inapplicable */
        s = pdev->strm;
    } else {
        pdev->contents_id = pdf_begin_obj(pdev, resourceStream);
        pdev->contents_length_id = pdf_obj_ref(pdev);
        s = pdev->strm;
        pprintld1(s, "<</Length %ld 0 R", pdev->contents_length_id);
        if (pdev->compression == pdf_compress_Flate) {
            if (pdev->binary_ok)
                pprints1(s, "/Filter /%s", compression_filter_name);
            else
                pprints1(s, "/Filter [/ASCII85Decode /%s]", compression_filter_name);
        }
        stream_puts(s, ">>\nstream\n");
        pdev->contents_pos = pdf_stell(pdev);
        code = pdf_begin_encrypt(pdev, &s, pdev->contents_id);
        if (code < 0)
            return code;
        pdev->strm = s;
        if (pdev->compression == pdf_compress_Flate) {	/* Set up the Flate filter. */
            const stream_template *templat;
            stream *es;
            byte *buf;
            compression_filter_state *st;

            if (!pdev->binary_ok) {	/* Set up the A85 filter */
                const stream_template *templat2 = &s_A85E_template;
                stream *as = s_alloc(pdev->pdf_memory, "PDF contents stream");
                byte *buf = gs_alloc_bytes(pdev->pdf_memory, sbuf_size,
                                           "PDF contents buffer");
                stream_A85E_state *ast = gs_alloc_struct(pdev->pdf_memory, stream_A85E_state,
                                templat2->stype, "PDF contents state");
                if (as == 0 || ast == 0 || buf == 0)
                    return_error(gs_error_VMerror);
                s_std_init(as, buf, sbuf_size, &s_filter_write_procs,
                           s_mode_write);
                ast->memory = pdev->pdf_memory;
                ast->templat = templat2;
                as->state = (stream_state *) ast;
                as->procs.process = templat2->process;
                as->strm = s;
                (*templat2->init) ((stream_state *) ast);
                pdev->strm = s = as;
            }
            templat = &compression_filter_template;
            es = s_alloc(pdev->pdf_memory, "PDF compression stream");
            buf = gs_alloc_bytes(pdev->pdf_memory, sbuf_size,
                                       "PDF compression buffer");
            st = gs_alloc_struct(pdev->pdf_memory, compression_filter_state,
                                 templat->stype, "PDF compression state");
            if (es == 0 || st == 0 || buf == 0)
                return_error(gs_error_VMerror);
            s_std_init(es, buf, sbuf_size, &s_filter_write_procs,
                       s_mode_write);
            st->memory = pdev->pdf_memory;
            st->templat = templat;
            es->state = (stream_state *) st;
            es->procs.process = templat->process;
            es->strm = s;
            (*templat->set_defaults) ((stream_state *) st);
            code = (*templat->init) ((stream_state *) st);
            if (code < 0) {
                gs_free_object(pdev->pdf_memory, st, "none_to_stream");
                return code;
            }
            pdev->strm = s = es;
        }
    }
    /*
     * Scale the coordinate system.  Use an extra level of q/Q for the
     * sake of poorly designed PDF tools that assume that the contents
     * stream restores the CTM.
     */
    pprintg2(s, "q %g 0 0 %g 0 0 cm\n",
             72.0 / pdev->HWResolution[0], 72.0 / pdev->HWResolution[1]);
    if (pdev->CompatibilityLevel >= 1.3) {
        /* Set the default rendering intent. */
        if (pdev->params.DefaultRenderingIntent != ri_Default) {
            static const char *const ri_names[] = { psdf_ri_names };

            pprints1(s, "/%s ri\n",
                     ri_names[(int)pdev->params.DefaultRenderingIntent]);
        }
    }
    pdev->AR4_save_bug = false;
    return PDF_IN_STREAM;
}
/* Enter text context from stream context. */
static int
stream_to_text(gx_device_pdf * pdev)
{
    int code;

    /*
     * Bizarrely enough, Acrobat Reader cares how the final font size is
     * obtained -- the CTM (cm), text matrix (Tm), and font size (Tf)
     * are *not* all equivalent.  In particular, it seems to use the
     * product of the text matrix and font size to decide how to
     * anti-alias characters.  Therefore, we have to temporarily patch
     * the CTM so that the scale factors are unity.  What a nuisance!
     */
    code = pdf_save_viewer_state(pdev, pdev->strm);
    if (code < 0)
        return 0;
    pprintg2(pdev->strm, "%g 0 0 %g 0 0 cm BT\n",
             pdev->HWResolution[0] / 72.0, pdev->HWResolution[1] / 72.0);
    pdev->procsets |= Text;
    code = pdf_from_stream_to_text(pdev);
    return (code < 0 ? code : PDF_IN_TEXT);
}
/* Exit string context to text context. */
static int
string_to_text(gx_device_pdf * pdev)
{
    int code = pdf_from_string_to_text(pdev);

    return (code < 0 ? code : PDF_IN_TEXT);
}
/* Exit text context to stream context. */
static int
text_to_stream(gx_device_pdf * pdev)
{
    int code;

    stream_puts(pdev->strm, "ET\n");
    code = pdf_restore_viewer_state(pdev, pdev->strm);
    if (code < 0)
        return code;
    pdf_reset_text(pdev);	/* because of Q */
    return PDF_IN_STREAM;
}
/* Exit stream context. */
static int
stream_to_none(gx_device_pdf * pdev)
{
    stream *s = pdev->strm;
    gs_offset_t length;
    int code;
    stream *target;
     char str[21];

    if (pdev->ResourcesBeforeUsage) {
        int code = pdf_exit_substream(pdev);

        if (code < 0)
            return code;
    } else {
        if (pdev->vgstack_depth) {
            code = pdf_restore_viewer_state(pdev, s);
            if (code < 0)
                return code;
        }
        target = pdev->strm;

        if (pdev->compression_at_page_start == pdf_compress_Flate)
            target = target->strm;
        if (!pdev->binary_ok)
            target = target->strm;
        if (pdf_end_encrypt(pdev))
            target = target->strm;
        s_close_filters(&pdev->strm, target);

        s = pdev->strm;
        length = pdf_stell(pdev) - pdev->contents_pos;
        if (pdev->PDFA != 0)
            stream_puts(s, "\n");
        stream_puts(s, "endstream\n");
        pdf_end_obj(pdev, resourceStream);

        if (pdev->WriteObjStms) {
            pdf_open_separate(pdev, pdev->contents_length_id, resourceLength);
            gs_snprintf(str, sizeof(str), "%"PRId64"\n", (int64_t)length);
            stream_puts(pdev->strm, str);
            pdf_end_separate(pdev, resourceLength);
        } else {
            pdf_open_obj(pdev, pdev->contents_length_id, resourceLength);
            gs_snprintf(str, sizeof(str), "%"PRId64"\n", (int64_t)length);
            stream_puts(s, str);
            pdf_end_obj(pdev, resourceLength);
        }
    }
    return PDF_IN_NONE;
}

/* Begin a page contents part. */
int
pdf_open_contents(gx_device_pdf * pdev, pdf_context_t context)
{
    int (*proc) (gx_device_pdf *);

    while ((proc = context_procs[pdev->context][context]) != 0) {
        int code = (*proc) (pdev);

        if (code < 0)
            return code;
        pdev->context = (pdf_context_t) code;
    }
    pdev->context = context;
    return 0;
}

/* Close the current contents part if we are in one. */
int
pdf_close_contents(gx_device_pdf * pdev, bool last)
{
    if (pdev->context == PDF_IN_NONE)
        return 0;
    if (last) {			/* Exit from the clipping path gsave. */
        int code = pdf_open_contents(pdev, PDF_IN_STREAM);

        if (code < 0)
            return code;
        stream_puts(pdev->strm, "Q\n");	/* See none_to_stream. */
        pdf_close_text_contents(pdev);
    }
    return pdf_open_contents(pdev, PDF_IN_NONE);
}

/* ------ Resources et al ------ */

/* Define the allocator descriptors for the resource types. */
const char *const pdf_resource_type_names[] = {
    PDF_RESOURCE_TYPE_NAMES
};
const gs_memory_struct_type_t *const pdf_resource_type_structs[] = {
    PDF_RESOURCE_TYPE_STRUCTS
};

/* Cancel a resource (do not write it into PDF). */
int
pdf_cancel_resource(gx_device_pdf * pdev, pdf_resource_t *pres, pdf_resource_type_t rtype)
{
    /* fixme : Remove *pres from resource chain. */
    pres->where_used = 0;
    if (pres->object) {
        pres->object->written = true;
        if (rtype == resourceXObject || rtype == resourceCharProc || rtype == resourceOther
            || rtype >= NUM_RESOURCE_TYPES) {
            int code = cos_stream_release_pieces(pdev, (cos_stream_t *)pres->object);

            if (code < 0)
                return code;
        }
        cos_release(pres->object, "pdf_cancel_resource");
        gs_free_object(pdev->pdf_memory, pres->object, "pdf_cancel_resources");
        pres->object = 0;
    }
    return 0;
}

/* Remove a resource. */
void
pdf_forget_resource(gx_device_pdf * pdev, pdf_resource_t *pres1, pdf_resource_type_t rtype)
{   /* fixme : optimize. */
    pdf_resource_t **pchain = pdev->resources[rtype].chains;
    pdf_resource_t *pres;
    pdf_resource_t **pprev = &pdev->last_resource;
    int i;

    /* since we're about to free the resource, we can just set
       any of these references to null
    */
    for (i = 0; i < pdev->sbstack_size; i++) {
        if (pres1 == pdev->sbstack[i].font3) {
            pdev->sbstack[i].font3 = NULL;
        }
        else if (pres1 == pdev->sbstack[i].accumulating_substream_resource) {
            pdev->sbstack[i].accumulating_substream_resource = NULL;
        }
        else if (pres1 == pdev->sbstack[i].pres_soft_mask_dict) {
            pdev->sbstack[i].pres_soft_mask_dict = NULL;
        }
    }

    for (; (pres = *pprev) != 0; pprev = &pres->prev)
        if (pres == pres1) {
            *pprev = pres->prev;
            break;
        }

    for (i = (gs_id_hash(pres1->rid) % NUM_RESOURCE_CHAINS); i < NUM_RESOURCE_CHAINS; i++) {
        pprev = pchain + i;
        for (; (pres = *pprev) != 0; pprev = &pres->next)
            if (pres == pres1) {
                *pprev = pres->next;
                if (pres->object) {
                    COS_RELEASE(pres->object, "pdf_forget_resource");
                    gs_free_object(pdev->pdf_memory, pres->object, "pdf_forget_resource");
                    pres->object = 0;
                }
                gs_free_object(pdev->pdf_memory, pres, "pdf_forget_resource");
                return;
            }
    }
}

static int
nocheck(gx_device_pdf * pdev, pdf_resource_t *pres0, pdf_resource_t *pres1)
{
    return 1;
}

/* Substitute a resource with a same one. */
/* NB we cannot substitute resources which have already had an
   id assigned to them, because they already have an entry in the
   xref table. If we want to substiute a resource then it should
   have been allocated with an initial id of -1.
   (see pdf_alloc_resource)
*/
int
pdf_substitute_resource(gx_device_pdf *pdev, pdf_resource_t **ppres,
        pdf_resource_type_t rtype,
        int (*eq)(gx_device_pdf * pdev, pdf_resource_t *pres0, pdf_resource_t *pres1),
        bool write)
{
    pdf_resource_t *pres1 = *ppres;
    int code;

    code = pdf_find_same_resource(pdev, rtype, ppres, (eq ? eq : nocheck));
    if (code < 0)
        return code;
    if (code != 0) {
        code = pdf_cancel_resource(pdev, (pdf_resource_t *)pres1, rtype);
        if (code < 0)
            return code;
        pdf_forget_resource(pdev, pres1, rtype);
        return 0;
    } else {
        if (pres1->object->id < 0)
            pdf_reserve_object_id(pdev, pres1, gs_no_id);
        if (write) {
            code = cos_write_object(pres1->object, pdev, rtype);
            if (code < 0)
                return code;
            pres1->object->written = 1;
        }
        return 1;
    }
}

/* Find a resource of a given type by gs_id. */
pdf_resource_t *
pdf_find_resource_by_gs_id(gx_device_pdf * pdev, pdf_resource_type_t rtype,
                           gs_id rid)
{
    pdf_resource_t **pchain = PDF_RESOURCE_CHAIN(pdev, rtype, rid);
    pdf_resource_t **pprev = pchain;
    pdf_resource_t *pres;

    for (; (pres = *pprev) != 0; pprev = &pres->next)
        if (pres->rid == rid) {
            if (pprev != pchain) {
                *pprev = pres->next;
                pres->next = *pchain;
                *pchain = pres;
            }
            return pres;
        }
    return 0;
}

/* Find resource by resource id. */
pdf_resource_t *
pdf_find_resource_by_resource_id(gx_device_pdf * pdev, pdf_resource_type_t rtype, gs_id id)
{
    pdf_resource_t **pchain = pdev->resources[rtype].chains;
    pdf_resource_t *pres;
    int i;

    for (i = 0; i < NUM_RESOURCE_CHAINS; i++) {
        for (pres = pchain[i]; pres != 0; pres = pres->next) {
            if (pres->object && pres->object->id == id)
                return pres;
        }
    }
    return 0;
}

/* Find same resource. */
int
pdf_find_same_resource(gx_device_pdf * pdev, pdf_resource_type_t rtype, pdf_resource_t **ppres,
        int (*eq)(gx_device_pdf * pdev, pdf_resource_t *pres0, pdf_resource_t *pres1))
{
    pdf_resource_t **pchain = pdev->resources[rtype].chains;
    pdf_resource_t *pres;
    cos_object_t *pco0 = (*ppres)->object;
    int i;

    for (i = 0; i < NUM_RESOURCE_CHAINS; i++) {
        for (pres = pchain[i]; pres != 0; pres = pres->next) {
            if (*ppres != pres) {
                int code;
                cos_object_t *pco1 = pres->object;

                if (pco1 == NULL || cos_type(pco0) != cos_type(pco1))
                    continue;	    /* don't compare different types */
                code = pco0->cos_procs->equal(pco0, pco1, pdev);
                if (code < 0)
                    return code;
                if (code > 0) {
                    code = eq(pdev, *ppres, pres);
                    if (code < 0)
                        return code;
                    if (code > 0) {
                        *ppres = pres;
                        return 1;
                    }
                }
            }
        }
    }
    return 0;
}

void
pdf_drop_resource_from_chain(gx_device_pdf * pdev, pdf_resource_t *pres1, pdf_resource_type_t rtype)
{
    pdf_resource_t **pchain = pdev->resources[rtype].chains;
    pdf_resource_t *pres;
    pdf_resource_t **pprev = &pdev->last_resource;
    int i;

    /* since we're about to free the resource, we can just set
       any of these references to null
    */
    for (i = 0; i < pdev->sbstack_size; i++) {
        if (pres1 == pdev->sbstack[i].font3) {
            pdev->sbstack[i].font3 = NULL;
        }
        else if (pres1 == pdev->sbstack[i].accumulating_substream_resource) {
            pdev->sbstack[i].accumulating_substream_resource = NULL;
        }
        else if (pres1 == pdev->sbstack[i].pres_soft_mask_dict) {
            pdev->sbstack[i].pres_soft_mask_dict = NULL;
        }
    }

    for (; (pres = *pprev) != 0; pprev = &pres->prev)
        if (pres == pres1) {
            *pprev = pres->prev;
            break;
        }

    for (i = (gs_id_hash(pres1->rid) % NUM_RESOURCE_CHAINS); i < NUM_RESOURCE_CHAINS; i++) {
        pprev = pchain + i;
        for (; (pres = *pprev) != 0; pprev = &pres->next)
            if (pres == pres1) {
                *pprev = pres->next;
#if 0
                if (pres->object) {
                    COS_RELEASE(pres->object, "pdf_forget_resource");
                    gs_free_object(pdev->pdf_memory, pres->object, "pdf_forget_resource");
                    pres->object = 0;
                }
                gs_free_object(pdev->pdf_memory, pres, "pdf_forget_resource");
#endif
                return;
            }
    }
}

/* Drop resources by a condition. */
void
pdf_drop_resources(gx_device_pdf * pdev, pdf_resource_type_t rtype,
        int (*cond)(gx_device_pdf * pdev, pdf_resource_t *pres))
{
    pdf_resource_t **pchain = pdev->resources[rtype].chains;
    pdf_resource_t **pprev;
    pdf_resource_t *pres;
    int i;

    for (i = 0; i < NUM_RESOURCE_CHAINS; i++) {
        pprev = pchain + i;
        for (; (pres = *pprev) != 0; ) {
            if (cond(pdev, pres)) {
                *pprev = pres->next;
                pres->next = pres; /* A temporary mark - see below */
            } else
                pprev = &pres->next;
        }
    }
    pprev = &pdev->last_resource;
    for (; (pres = *pprev) != 0; )
        if (pres->next == pres) {
            *pprev = pres->prev;
            if (pres->object) {
                COS_RELEASE(pres->object, "pdf_drop_resources");
                gs_free_object(pdev->pdf_memory, pres->object, "pdf_drop_resources");
                pres->object = 0;
            }
            gs_free_object(pdev->pdf_memory, pres, "pdf_drop_resources");
        } else
            pprev = &pres->prev;
}

/* Print resource statistics. */
void
pdf_print_resource_statistics(gx_device_pdf * pdev)
{

    int rtype;

    for (rtype = 0; rtype < NUM_RESOURCE_TYPES; rtype++) {
        pdf_resource_t **pchain = pdev->resources[rtype].chains;
        pdf_resource_t *pres;
        const char *name = pdf_resource_type_names[rtype];
        int i, n = 0;

        for (i = 0; i < NUM_RESOURCE_CHAINS; i++) {
            for (pres = pchain[i]; pres != 0; pres = pres->next, n++);
        }
        dmprintf3(pdev->pdf_memory, "Resource type %d (%s) has %d instances.\n", rtype,
                (name ? name : ""), n);
    }
}

int FlushObjStm(gx_device_pdf *pdev)
{
    int code = 0, i, len = 0, id = 0, end;
    char offset[21], offsets [(20*MAX_OBJSTM_OBJECTS) + 1];
    pdf_resource_t *pres;
    int options = DATA_STREAM_BINARY;

    if (pdev->ObjStm_id == 0)
        return 0;

    pdev->WriteObjStms = false;

    sflush(pdev->strm);
    sflush(pdev->ObjStm.strm);
    end = stell(pdev->ObjStm.strm);

    if (pdev->CompressStreams)
        options |= DATA_STREAM_COMPRESS;

    code = pdf_open_aside(pdev, resourceStream, pdev->ObjStm_id, &pres, false, options);
    if (code < 0) {
        pdev->WriteObjStms = true;
        return code;
    }
    pdf_reserve_object_id(pdev, pres, pdev->ObjStm_id);

    code = cos_dict_put_c_key_string((cos_dict_t *)pres->object, "/Type", (const byte *)"/ObjStm", 7);
    if (code < 0) {
        pdf_close_aside(pdev);
        pdev->WriteObjStms = true;
        return code;
    }
    code = cos_dict_put_c_key_int((cos_dict_t *)pres->object, "/N", pdev->NumObjStmObjects);
    if (code < 0) {
        pdf_close_aside(pdev);
        pdev->WriteObjStms = true;
        return code;
    }

    memset(offsets, 0x00, (20*MAX_OBJSTM_OBJECTS) + 1);
    for (i=0;i < pdev->NumObjStmObjects;i++) {
        len = pdev->ObjStmOffsets[(i * 2) + 1];
        id = pdev->ObjStmOffsets[(i * 2)];
        gs_snprintf(offset, 21, "%ld %ld ", id, len);
        strcat(offsets, offset);
    }

    code = cos_dict_put_c_key_int((cos_dict_t *)pres->object, "/First", strlen(offsets));
    if (code < 0) {
        pdf_close_aside(pdev);
        pdev->WriteObjStms = true;
        return code;
    }

    stream_puts(pdev->strm, offsets);

    gp_fseek(pdev->ObjStm.file, 0L, SEEK_SET);
    code = pdf_copy_data(pdev->strm, pdev->ObjStm.file, end, NULL);
    if (code < 0) {
        pdf_close_aside(pdev);
        pdev->WriteObjStms = true;
        return code;
    }
    code = pdf_close_aside(pdev);
    if (code < 0)
        return code;
    code = COS_WRITE_OBJECT(pres->object, pdev, resourceNone);
    if (code < 0) {
        pdev->WriteObjStms = true;
        return code;
    }
    pdev->WriteObjStms = true;
    code = pdf_close_temp_file(pdev, &pdev->ObjStm, code);
    if (pdev->ObjStmOffsets != NULL) {
        gs_free_object(pdev->pdf_memory->non_gc_memory, pdev->ObjStmOffsets, "NewObjStm");
        pdev->ObjStmOffsets = NULL;
    }
    pdev->NumObjStmObjects = 0;
    pdev->ObjStm_id = 0;

    pdev->WriteObjStms = true;
    return code;
}

int NewObjStm(gx_device_pdf *pdev)
{
    int code;

    pdev->ObjStm_id = pdf_obj_forward_ref(pdev);

    code = pdf_open_temp_stream(pdev, &pdev->ObjStm);
    if (code < 0)
        return code;

    pdev->NumObjStmObjects = 0;
    if (pdev->ObjStmOffsets != NULL)
        gs_free_object(pdev->pdf_memory->non_gc_memory, pdev->ObjStmOffsets, "NewObjStm");

    pdev->ObjStmOffsets = (gs_offset_t *)gs_alloc_bytes(pdev->pdf_memory->non_gc_memory, MAX_OBJSTM_OBJECTS * sizeof(gs_offset_t) * 2, "NewObjStm");
    if (pdev->ObjStmOffsets == NULL) {
        code = gs_note_error(gs_error_VMerror);
    } else
        memset(pdev->ObjStmOffsets, 0x00, MAX_OBJSTM_OBJECTS * sizeof(int) * 2);
    return code;
}

/* Begin an object logically separate from the contents. */
int64_t
pdf_open_separate_noObjStm(gx_device_pdf * pdev, int64_t id, pdf_resource_type_t type)
{
    int code;

    code = pdfwrite_pdf_open_document(pdev);
    if (code < 0)
        return code;
    pdev->asides.save_strm = pdev->strm;
    pdev->strm = pdev->asides.strm;
    code = pdf_open_obj(pdev, id, type);
    return code;
}

static int is_stream_resource(pdf_resource_type_t type)
{
    if (type == resourceStream)
        return true;
    if (type == resourceCharProc)
        return true;
    if (type == resourcePattern)
        return true;
    if (type == resourceXObject)
        return true;
    return false;
}

int64_t
pdf_open_separate(gx_device_pdf * pdev, int64_t id, pdf_resource_type_t type)
{
    int code;

    if (!pdev->WriteObjStms || is_stream_resource(type)) {
        code = pdfwrite_pdf_open_document(pdev);
        if (code < 0)
            return code;
        pdev->asides.save_strm = pdev->strm;
        pdev->strm = pdev->asides.strm;
        code = pdf_open_obj(pdev, id, type);
    } else {
        if (pdev->ObjStm.strm != NULL && pdev->NumObjStmObjects >= MAX_OBJSTM_OBJECTS) {
            code = FlushObjStm(pdev);
            if (code < 0)
                return code;
        }
        if (!pdev->ObjStm.strm) {
            code = NewObjStm(pdev);
            if (code < 0)
                return code;
        }
        pdev->ObjStm.save_strm = pdev->strm;
        pdev->strm = pdev->ObjStm.strm;
        code = pdf_open_obj(pdev, id, type);
        pdev->ObjStmOffsets[pdev->NumObjStmObjects * 2] = code;
        pdev->ObjStmOffsets[(pdev->NumObjStmObjects * 2) + 1] = pdf_stell(pdev);
    }
    return code;
}
int64_t
pdf_begin_separate(gx_device_pdf * pdev, pdf_resource_type_t type)
{
    return pdf_open_separate(pdev, 0L, type);
}

void
pdf_reserve_object_id(gx_device_pdf * pdev, pdf_resource_t *pres, int64_t id)
{
    pres->object->id = (id == 0 ? pdf_obj_ref(pdev) : id);
    gs_snprintf(pres->rname, sizeof(pres->rname), "R%ld", pres->object->id);
}

/* Begin an aside (resource, annotation, ...). */
int
pdf_alloc_aside(gx_device_pdf * pdev, pdf_resource_t ** plist,
                const gs_memory_struct_type_t * pst, pdf_resource_t **ppres,
                int64_t id)
{
    pdf_resource_t *pres;
    cos_object_t *object;

    if (pst == NULL)
        pst = &st_pdf_resource;
    pres = gs_alloc_struct(pdev->pdf_memory, pdf_resource_t, pst,
                           "pdf_alloc_aside(resource)");
    if (pres == 0)
        return_error(gs_error_VMerror);
    object = cos_object_alloc(pdev, "pdf_alloc_aside(object)");
    if (object == 0)
        return_error(gs_error_VMerror);
    memset(pres, 0, pst->ssize);
    pres->object = object;
    if (id < 0) {
        object->id = -1L;
        pres->rname[0] = 0;
    } else
        pdf_reserve_object_id(pdev, pres, id);
    pres->next = *plist;
    pres->rid = 0;
    *plist = pres;
    pres->prev = pdev->last_resource;
    pdev->last_resource = pres;
    pres->named = false;
    pres->global = false;
    pres->where_used = pdev->used_mask;
    *ppres = pres;
    return 0;
}
int64_t
pdf_begin_aside(gx_device_pdf * pdev, pdf_resource_t ** plist,
                const gs_memory_struct_type_t * pst, pdf_resource_t ** ppres,
                pdf_resource_type_t type)
{
    int64_t id = pdf_begin_separate(pdev, type);
    int code = 0;

    if (id < 0)
        return (int)id;
    code = pdf_alloc_aside(pdev, plist, pst, ppres, id);
    if (code < 0)
        (void)pdf_end_separate(pdev, type);

    return code;
}

/* Begin a resource of a given type. */
int
pdf_begin_resource_body(gx_device_pdf * pdev, pdf_resource_type_t rtype,
                        gs_id rid, pdf_resource_t ** ppres)
{
    int code;

    if (rtype >= NUM_RESOURCE_TYPES)
        rtype = resourceOther;

    code = pdf_begin_aside(pdev, PDF_RESOURCE_CHAIN(pdev, rtype, rid),
                               pdf_resource_type_structs[rtype], ppres, rtype);

    if (code >= 0)
        (*ppres)->rid = rid;
    return code;
}
int
pdf_begin_resource(gx_device_pdf * pdev, pdf_resource_type_t rtype, gs_id rid,
                   pdf_resource_t ** ppres)
{
    int code;

    if (rtype >= NUM_RESOURCE_TYPES)
        rtype = resourceOther;

    code = pdf_begin_resource_body(pdev, rtype, rid, ppres);

    if (code >= 0 && pdf_resource_type_names[rtype] != 0) {
        stream *s = pdev->strm;

        pprints1(s, "<</Type%s", pdf_resource_type_names[rtype]);
        pprintld1(s, "/Name/R%ld", (*ppres)->object->id);
    }
    return code;
}

/* Allocate a resource, but don't open the stream. */
/* If the passed in id 'id' is -1 then in pdf_alloc_aside
   We *don't* reserve an object id (if its 0 or more we do).
   This has important consequences; once an id is created we
   can't 'cancel' it, it will always be written to the xref.
   So if we want to not write duplicates we should create
   the object with an 'id' of -1, and when we finish writing it
   we should call 'pdf_substitute_resource'. If that finds a
   duplicate then it will throw away the new one ands use the old.
   If it doesn't find a duplicate then it will create an object
   id for the new resource.
*/
int
pdf_alloc_resource(gx_device_pdf * pdev, pdf_resource_type_t rtype, gs_id rid,
                   pdf_resource_t ** ppres, int64_t id)
{
    int code;

    if (rtype >= NUM_RESOURCE_TYPES)
        rtype = resourceOther;

    code = pdf_alloc_aside(pdev, PDF_RESOURCE_CHAIN(pdev, rtype, rid),
                               pdf_resource_type_structs[rtype], ppres, id);

    if (code >= 0)
        (*ppres)->rid = rid;
    return code;
}

/* Get the object id of a resource. */
int64_t
pdf_resource_id(const pdf_resource_t *pres)
{
    return pres->object->id;
}

/* End an aside or other separate object. */
int
pdf_end_separate_noObjStm(gx_device_pdf * pdev, pdf_resource_type_t type)
{
    int code = pdf_end_obj(pdev, type);

    pdev->strm = pdev->asides.save_strm;
    pdev->asides.save_strm = 0;
    return code;
}
int
pdf_end_separate(gx_device_pdf * pdev, pdf_resource_type_t type)
{
    int code = pdf_end_obj(pdev, type);

    if (!pdev->WriteObjStms || is_stream_resource(type)) {
        pdev->strm = pdev->asides.save_strm;
        pdev->asides.save_strm = 0;
    } else {
        pdev->strm = pdev->ObjStm.save_strm;
        pdev->ObjStm.save_strm = 0;
        pdev->NumObjStmObjects++;
    }
    return code;
}
int
pdf_end_aside(gx_device_pdf * pdev, pdf_resource_type_t type)
{
    return pdf_end_separate(pdev, type);
}

/* End a resource. */
int
pdf_end_resource(gx_device_pdf * pdev, pdf_resource_type_t type)
{
    return pdf_end_aside(pdev, type);
}

/*
 * Write the Cos objects for resources local to a content stream.  Formerly,
 * this procedure also freed such objects, but this doesn't work, because
 * resources of one type might refer to resources of another type.
 */
int
pdf_write_resource_objects(gx_device_pdf *pdev, pdf_resource_type_t rtype)
{
    int j, code = 0;

    for (j = 0; j < NUM_RESOURCE_CHAINS && code >= 0; ++j) {
        pdf_resource_t *pres = pdev->resources[rtype].chains[j];

        for (; pres != 0; pres = pres->next)
            if ((!pres->named || pdev->ForOPDFRead)
                && pres->object && !pres->object->written) {
                    code = cos_write_object(pres->object, pdev, rtype);
            }
    }
    return code;
}

/*
 * Reverse resource chains.
 * ps2write uses it with page resources.
 * Assuming only the 0th chain contauns something.
 */
void
pdf_reverse_resource_chain(gx_device_pdf *pdev, pdf_resource_type_t rtype)
{
    pdf_resource_t *pres = pdev->resources[rtype].chains[0];
    pdf_resource_t *pres1, *pres0 = pres, *pres2;

    if (pres == NULL)
        return;
    pres1 = pres->next;
    for (;;) {
        if (pres1 == NULL)
            break;
        pres2 = pres1->next;
        pres1->next = pres;
        pres = pres1;
        pres1 = pres2;
    }
    pres0->next = NULL;
    pdev->resources[rtype].chains[0] = pres;
}

/*
 * Free unnamed Cos objects for resources local to a content stream,
 * since they can't be used again.
 */
int
pdf_free_resource_objects(gx_device_pdf *pdev, pdf_resource_type_t rtype)
{
    int j;

    for (j = 0; j < NUM_RESOURCE_CHAINS; ++j) {
        pdf_resource_t **prev = &pdev->resources[rtype].chains[j];
        pdf_resource_t *pres;

        while ((pres = *prev) != 0) {
            if (pres->named) {	/* named, don't free */
                prev = &pres->next;
            } else {
                if (pres->object) {
                    cos_free(pres->object, "pdf_free_resource_objects");
                    pres->object = 0;
                }
                *prev = pres->next;
            }
        }
    }
    return 0;
}

/*
 * Store the resource sets for a content stream (page or XObject).
 * Sets page->{procsets, resource_ids[]}.
 */
int
pdf_store_page_resources(gx_device_pdf *pdev, pdf_page_t *page, bool clear_usage)
{
    int i;

    /* Write any resource dictionaries. */

    for (i = 0; i <= resourceFont; ++i) {
        stream *s = 0;
        int j;

        if (i == resourceOther || i >= NUM_RESOURCE_TYPES)
            continue;
        page->resource_ids[i] = 0;
        for (j = 0; j < NUM_RESOURCE_CHAINS; ++j) {
            pdf_resource_t *pres = pdev->resources[i].chains[j];

            for (; pres != 0; pres = pres->next) {
                if (pres->where_used & pdev->used_mask) {
                    int64_t id = pdf_resource_id(pres);

                    if (id == -1L)
                        continue;
                    if (s == 0) {
                        page->resource_ids[i] = pdf_begin_separate(pdev, i);
                        pdf_record_usage(pdev, page->resource_ids[i], pdev->next_page);
                        s = pdev->strm;
                        stream_puts(s, "<<");
                    }
                    pprints1(s, "/%s\n", pres->rname);
                    pprintld1(s, "%ld 0 R", id);
                    pdf_record_usage(pdev, id, pdev->next_page);
                    if (clear_usage)
                        pres->where_used -= pdev->used_mask;
                }
            }
        }
        if (s) {
            stream_puts(s, ">>\n");
            pdf_end_separate(pdev, i);
        }
        /* If an object isn't used, we still need to emit it :-( This is because
         * we reserved an object number for it, and the xref will have an entry
         * for it. If we don't actually emit it then the xref will be invalid.
         * An alternative would be to modify the xref to mark the object as unused.
         */
        if (i != resourceFont && i != resourceProperties)
            pdf_write_resource_objects(pdev, i);
    }
    page->procsets = pdev->procsets;
    return 0;
}

/* Copy data from a temporary file to a stream. */
int
pdf_copy_data(stream *s, gp_file *file, gs_offset_t count, stream_arcfour_state *ss)
{
    gs_offset_t r, left = count;
    byte buf[sbuf_size];

    while (left > 0) {
        uint copy = min(left, sbuf_size);

        r = gp_fread(buf, 1, copy, file);
        if (r < 1) {
            return gs_note_error(gs_error_ioerror);
        }
        if (ss)
            s_arcfour_process_buffer(ss, buf, copy);
        stream_write(s, buf, copy);
        left -= copy;
    }
    return 0;
}

/* Copy data from a temporary file to a stream,
   which may be targetted to the same file. */
int
pdf_copy_data_safe(stream *s, gp_file *file, gs_offset_t position, int64_t count)
{
    int64_t r, left = count;

    while (left > 0) {
        byte buf[sbuf_size];
        int64_t copy = min(left, (int64_t)sbuf_size);
        int64_t end_pos = gp_ftell(file);

        if (gp_fseek(file, position + count - left, SEEK_SET) != 0) {
            return_error(gs_error_ioerror);
        }
        r = gp_fread(buf, 1, copy, file);
        if (r < 1) {
            return_error(gs_error_ioerror);
        }
        if (gp_fseek(file, end_pos, SEEK_SET) != 0) {
            return_error(gs_error_ioerror);
        }
        stream_write(s, buf, copy);
        sflush(s);
        left -= copy;
    }
    return 0;
}

/* ------ Pages ------ */

/* Get or assign the ID for a page. */
/* Returns 0 if the page number is out of range. */
int64_t
pdf_page_id(gx_device_pdf * pdev, int page_num)
{
    cos_dict_t *Page;

    if (page_num < 1 || pdev->pages == NULL)
        return 0;
    if (page_num >= pdev->num_pages) {	/* Grow the pages array. */
        uint new_num_pages;
        pdf_page_t *new_pages;

        /* Maximum page in PDF is 2^31 - 1. Clamp to that limit here */
        if (page_num > (1LU << 31) - 11)
            page_num = (1LU << 31) - 11;
        new_num_pages = max(page_num + 10, pdev->num_pages << 1);

        new_pages = gs_resize_object(pdev->pdf_memory, pdev->pages, new_num_pages,
                             "pdf_page_id(resize pages)");

        if (new_pages == 0)
            return 0;
        memset(&new_pages[pdev->num_pages], 0,
               (new_num_pages - pdev->num_pages) * sizeof(pdf_page_t));
        pdev->pages = new_pages;
        pdev->num_pages = new_num_pages;
    }
    if ((Page = pdev->pages[page_num - 1].Page) == 0) {
        pdev->pages[page_num - 1].Page = Page = cos_dict_alloc(pdev, "pdf_page_id");
        if (Page == NULL) {
            return 0;
        }
        Page->id = pdf_obj_forward_ref(pdev);
    }
    return Page->id;
}

/* Get the page structure for the current page. */
pdf_page_t *
pdf_current_page(gx_device_pdf *pdev)
{
    return &pdev->pages[pdev->next_page];
}

/* Get the dictionary object for the current page. */
cos_dict_t *
pdf_current_page_dict(gx_device_pdf *pdev)
{
    if (pdf_page_id(pdev, pdev->next_page + 1) <= 0)
        return 0;
    return pdev->pages[pdev->next_page].Page;
}

/* Write saved page- or document-level information. */
int
pdf_write_saved_string(gx_device_pdf * pdev, gs_string * pstr)
{
    if (pstr->data != 0) {
        stream_write(pdev->strm, pstr->data, pstr->size);
        gs_free_string(pdev->pdf_memory, pstr->data, pstr->size,
                       "pdf_write_saved_string");
        pstr->data = 0;
    }
    return 0;
}

/* Open a page for writing. */
int
pdf_open_page(gx_device_pdf * pdev, pdf_context_t context)
{
    if (!is_in_page(pdev)) {
        int code;

        if (pdf_page_id(pdev, pdev->next_page + 1) == 0)
            return_error(gs_error_VMerror);
        code = pdfwrite_pdf_open_document(pdev);
        if (code < 0)
            return code;
    }
    /* Note that context may be PDF_IN_NONE here. */
    return pdf_open_contents(pdev, context);
}

/*  Go to the unclipped stream context. */
int
pdf_unclip(gx_device_pdf * pdev)
{
    const int bottom = (pdev->ResourcesBeforeUsage ? 1 : 0);
    /* When ResourcesBeforeUsage != 0, one sbstack element
       appears from the page contents stream. */

    if (pdev->sbstack_depth <= bottom) {
        int code = pdf_open_page(pdev, PDF_IN_STREAM);

        if (code < 0)
            return code;
    }
    if (pdev->context > PDF_IN_STREAM) {
        int code = pdf_open_contents(pdev, PDF_IN_STREAM);

        if (code < 0)
            return code;
    }
    if (pdev->vgstack_depth > pdev->vgstack_bottom) {
        int code = pdf_restore_viewer_state(pdev, pdev->strm);

        if (code < 0)
            return code;
        code = pdf_remember_clip_path(pdev, NULL);
        if (code < 0)
            return code;
        pdev->clip_path_id = pdev->no_clip_path_id;
    }
    return 0;
}

/* ------ Miscellaneous output ------ */

/* Generate the default Producer string. */
/* This calculation is also performed for Ghostscript generally
 * The code is in ghostpdl/base/gsmisc.c printf_program_ident().
 * Should we change this calculation both sets of code need to be updated.
 */
void
pdf_store_default_Producer(char buf[PDF_MAX_PRODUCER])
{
    int major = (int)(gs_revision / 1000);
    int minor = (int)(gs_revision - (major * 1000)) / 10;
    int patch = gs_revision % 10;

    gs_snprintf(buf, PDF_MAX_PRODUCER, "(%s %d.%02d.%d)", gs_product, major, minor, patch);
}

/* Write matrix values. */
void
pdf_put_matrix(gx_device_pdf * pdev, const char *before,
               const gs_matrix * pmat, const char *after)
{
    stream *s = pdev->strm;

    if (before)
        stream_puts(s, before);
    pprintg6(s, "%g %g %g %g %g %g ",
             pmat->xx, pmat->xy, pmat->yx, pmat->yy, pmat->tx, pmat->ty);
    if (after)
        stream_puts(s, after);
}

/*
 * Write a name, with escapes for unusual characters.  Since we only support
 * PDF 1.2 and above, we can use an escape sequence for anything except a
 * null <00>, and the machinery for selecting the put_name_chars procedure
 * depending on CompatibilityLevel is no longer needed.
 */
static int
pdf_put_name_chars_1_2(stream *s, const byte *nstr, uint size)
{
    uint i;

    for (i = 0; i < size; ++i) {
        uint c = nstr[i];
        char hex[4];

        switch (c) {
            default:
                if (c >= 0x21 && c <= 0x7e) {
                    stream_putc(s, (byte)c);
                    break;
                }
                /* falls through */
            case '#':
            case '%':
            case '(': case ')':
            case '<': case '>':
            case '[': case ']':
            case '{': case '}':
            case '/':
                gs_snprintf(hex, sizeof(hex), "#%02x", c);
                stream_puts(s, hex);
                break;
            case 0:
                stream_puts(s, "BnZr"); /* arbitrary */
        }
    }
    return 0;
}
pdf_put_name_chars_proc_t
pdf_put_name_chars_proc(const gx_device_pdf *pdev)
{
    return &pdf_put_name_chars_1_2;
}
int
pdf_put_name_chars(const gx_device_pdf *pdev, const byte *nstr, uint size)
{
    return pdf_put_name_chars_proc(pdev)(pdev->strm, nstr, size);
}
int
pdf_put_name(const gx_device_pdf *pdev, const byte *nstr, uint size)
{
    stream_putc(pdev->strm, '/');
    return pdf_put_name_chars(pdev, nstr, size);
}

/* Write an encoded string with encryption. */
static int
pdf_encrypt_encoded_string(const gx_device_pdf *pdev, const byte *str, uint size, gs_id object_id)
{
    stream sinp, sstr, sout;
    stream_PSSD_state st;
    stream_state so;
    byte buf[100], bufo[100];
    stream_arcfour_state sarc4;

    if (pdf_encrypt_init(pdev, object_id, &sarc4) < 0) {
        /* The interface can't pass an error. */
        stream_write(pdev->strm, str, size);
        return size;
    }
    s_init(&sinp, NULL);
    sread_string(&sinp, str + 1, size);
    s_init(&sstr, NULL);
    sstr.close_at_eod = false;
    s_init_state((stream_state *)&st, &s_PSSD_template, NULL);
    s_init_filter(&sstr, (stream_state *)&st, buf, sizeof(buf), &sinp);
    s_init(&sout, NULL);
    s_init_state(&so, &s_PSSE_template, NULL);
    s_init_filter(&sout, &so, bufo, sizeof(bufo), pdev->strm);
    stream_putc(pdev->strm, '(');
    for (;;) {
        uint n;
        int code = sgets(&sstr, buf, sizeof(buf), &n);

        if (n > 0) {
            s_arcfour_process_buffer(&sarc4, buf, n);
            stream_write(&sout, buf, n);
        }
        if (code == EOFC)
            break;
        if (code < 0 || n < sizeof(buf)) {
            /* The interface can't pass an error. */
            break;
        }
    }
    /* Another case where we use sclose() and not sclose_filters(), because the
     * buffer we supplied to s_init_filter is a heap based C object, so we
     * must not free it.
     */
    sclose(&sout); /* Writes ')'. */
    return (int)stell(&sinp) + 1;
}

/* Write an encoded string with possible encryption. */
static int
pdf_put_encoded_string(const gx_device_pdf *pdev, const byte *str, uint size, gs_id object_id)
{
    if ((!pdev->KeyLength || pdev->WriteObjStms) || object_id == (gs_id)-1) {
        stream_write(pdev->strm, str, size);
        return 0;
    } else
        return pdf_encrypt_encoded_string(pdev, str, size, object_id);
}
/* Write an encoded string with possible encryption. */
static int
pdf_put_encoded_string_as_hex(const gx_device_pdf *pdev, const byte *str, uint size, gs_id object_id)
{
    if (!pdev->KeyLength || object_id == (gs_id)-1) {
        int i, oct, width = 0;
        char hex[16] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};

        if (pdev->ForOPDFRead && pdev->ProduceDSC)
            stream_write(pdev->strm, "\n", 1);
        stream_write(pdev->strm, "<", 1);
        width++;
        for (i = 1; i < size - 1; i++) {
            if (str[i] == '\\') {
                if (str[i + 1] >= '0' && str[i + 1] <= '9') {
                    oct = (str[i+1] - 0x30) * 64;
                    oct += (str[i+2] - 0x30) *8;
                    oct += str[i+3] - 0x30;
                    i+=3;
                } else {
                    switch (str[++i]) {
                        case 'b' :
                            oct = 8;
                            break;
                        case 't' :
                            oct = 9;
                            break;
                        case 'n' :
                            oct = 10;
                            break;
                        case 'f' :
                            oct = 12;
                            break;
                        case 'r' :
                            oct = 13;
                            break;
                        default:
                            oct = str[i];
                            break;
                    }
                }
                if (width > 252 && pdev->ForOPDFRead && pdev->ProduceDSC) {
                    stream_write(pdev->strm, "\n", 1);
                    width = 0;
                }
                stream_write(pdev->strm, &hex[(oct & 0xf0) >> 4], 1);
                stream_write(pdev->strm, &hex[oct & 0x0f], 1);
                width += 2;
            } else {
                if (width > 252 && pdev->ForOPDFRead && pdev->ProduceDSC) {
                    stream_write(pdev->strm, "\n", 1);
                    width = 0;
                }
                stream_write(pdev->strm, &hex[(str[i] & 0xf0) >> 4], 1);
                stream_write(pdev->strm, &hex[str[i] & 0x0f], 1);
                width += 2;
            }
        }
        stream_write(pdev->strm, ">", 1);
        if (pdev->ForOPDFRead && pdev->ProduceDSC)
            stream_write(pdev->strm, "\n", 1);
        return 0;
    } else
        return pdf_encrypt_encoded_string(pdev, str, size, object_id);
}

/* Write an encoded hexadecimal string with possible encryption. */
static int
pdf_put_encoded_hex_string(const gx_device_pdf *pdev, const byte *str, uint size, gs_id object_id)
{
    emprintf(pdev->memory,
             "Unimplemented function : pdf_put_encoded_hex_string\n");
    stream_write(pdev->strm, str, size);
    return_error(gs_error_unregistered);
}
/*  Scan an item in a serialized array or dictionary.
    This is a very simplified Postscript lexical scanner.
    It assumes the serialization with pdf===only defined in gs/lib/gs_pdfwr.ps .
    We only need to select strings and encrypt them.
    Other items are passed identically.
    Note we don't reconstruct the nesting of arrays|dictionaries.
*/
static int
pdf_scan_item(const gx_device_pdf * pdev, const byte * p, uint l, gs_id object_id)
{
    const byte *q = p;
    int n = l;

    if (*q == ' ' || *q == 't' || *q == '\r' || *q == '\n')
        return (l > 0 ? 1 : 0);
    for (q++, n--; n; q++, n--) {
        if (*q == ' ' || *q == 't' || *q == '\r' || *q == '\n')
            return q - p;
        if (*q == '/' || *q == '[' || *q == ']' || *q == '{' || *q == '}' || *q == '(' || *q == '<')
            return q - p;
        /* Note : immediate names are not allowed in PDF. */
    }
    return l;
}

/* Write a serialized array or dictionary with possible encryption. */
static int
pdf_put_composite(const gx_device_pdf * pdev, const byte * vstr, uint size, gs_id object_id)
{
    if (!pdev->KeyLength || object_id == (gs_id)-1) {
        if (pdev->ForOPDFRead && pdev->ProduceDSC) {
            stream_putc(pdev->strm, (byte)'\n');
            if (size > 255) {
                const byte *start, *p, *end, *save;
                int width = 0;

                end = vstr + size;
                start = p = vstr;
                while (p < end) {
                    if(*p == '\r' || *p == '\n') {
                        width = 0;
                        p++;
                        continue;
                    }
                    if (width > 254) {
                        save = p;
                        /* search backwards for a point to split */
                        while (p > start) {
                            if (*p == '/' || *p == '[' || *p == '{' || *p == '(' || *p == ' ') {
                                stream_write(pdev->strm, start, p - start);
                                stream_putc(pdev->strm, (byte)'\n');
                                width = 0;
                                start = p;
                                break;
                            }
                            else p--;
                        }
                        if (p == start && width != 0) {
                            stream_write(pdev->strm, start, save - start);
                            stream_putc(pdev->strm, (byte)'\n');
                            p = start = save;
                            width = 0;
                        }
                    } else {
                        width++;
                        p++;
                    }
                }
                if (width) {
                    stream_write(pdev->strm, start, p - start);
                    stream_putc(pdev->strm, (byte)'\n');
                }
            } else {
                stream_write(pdev->strm, vstr, size);
            }
        } else {
            stream_write(pdev->strm, vstr, size);
        }
    } else {
        const byte *p = vstr;
        int l = size, n;

        for (;l > 0 ;) {
            if (*p == '(')
                n = pdf_encrypt_encoded_string(pdev, p, l, object_id);
            else {
                n = pdf_scan_item(pdev, p, l, object_id);
                stream_write(pdev->strm, p, n);
            }
            l -= n;
            p += n;
        }
    }
    return 0;
}

/*
 * Write a string in its shortest form ( () or <> ).  Note that
 * this form is different depending on whether binary data are allowed.
 * We wish PDF supported ASCII85 strings ( <~ ~> ), but it doesn't.
 */
int
pdf_put_string(const gx_device_pdf * pdev, const byte * str, uint size)
{
    psdf_write_string(pdev->strm, str, size,
                      (pdev->binary_ok ? PRINT_BINARY_OK : 0));
    return 0;
}

/* Write a value, treating names specially. */
int
pdf_write_value(const gx_device_pdf * pdev, const byte * vstr, uint size, gs_id object_id)
{
    if (size > 0 && vstr[0] == '/')
        return pdf_put_name(pdev, vstr + 1, size - 1);
    else if (size > 5 && vstr[0] == 0 && vstr[1] == 0 && vstr[2] == 0 && vstr[size - 1] == 0 && vstr[size - 2] == 0)
        return pdf_put_name(pdev, vstr + 4, size - 5);
    else if (size > 3 && vstr[0] == 0 && vstr[1] == 0 && vstr[size - 1] == 0)
        return pdf_put_name(pdev, vstr + 3, size - 4);
    else if (size > 1 && (vstr[0] == '[' || vstr[0] == '{'))
        return pdf_put_composite(pdev, vstr, size, object_id);
    else if (size > 2 && vstr[0] == '<' && vstr[1] == '<')
        return pdf_put_composite(pdev, vstr, size, object_id);
    else if (size > 1 && vstr[0] == '(') {
        if (pdev->ForOPDFRead)
            return pdf_put_encoded_string_as_hex(pdev, vstr, size, object_id);
        else
            return pdf_put_encoded_string(pdev, vstr, size, object_id);
    }
    else if (size > 1 && vstr[0] == '<')
        return pdf_put_encoded_hex_string(pdev, vstr, size, object_id);
    stream_write(pdev->strm, vstr, size);
    return 0;
}

/* Store filters for a stream. */
/* Currently this only saves parameters for CCITTFaxDecode. */
int
pdf_put_filters(cos_dict_t *pcd, gx_device_pdf *pdev, stream *s,
                const pdf_filter_names_t *pfn)
{
    const char *filter_name = 0;
    bool binary_ok = true;
    stream *fs = s;
    cos_dict_t *decode_parms = 0;
    int code;

    for (; fs != 0; fs = fs->strm) {
        const stream_state *st = fs->state;
        const stream_template *templat = st->templat;

#define TEMPLATE_IS(atemp)\
  (templat->process == (atemp).process)
        if (TEMPLATE_IS(s_A85E_template))
            binary_ok = false;
        else if (TEMPLATE_IS(s_CFE_template)) {
            cos_param_list_writer_t writer;
            stream_CF_state cfs;

            decode_parms =
                cos_dict_alloc(pdev, "pdf_put_image_filters(decode_parms)");
            if (decode_parms == 0)
                return_error(gs_error_VMerror);
            CHECK(cos_param_list_writer_init(pdev, &writer, decode_parms, 0));
            /*
             * If EndOfBlock is true, we mustn't write a Rows value.
             * This is a hack....
             */
            cfs = *(const stream_CF_state *)st;
            if (cfs.EndOfBlock)
                cfs.Rows = 0;
            CHECK(s_CF_get_params((gs_param_list *)&writer, &cfs, false));
            filter_name = pfn->CCITTFaxDecode;
        } else if (TEMPLATE_IS(s_DCTE_template))
            filter_name = pfn->DCTDecode;
        else if (TEMPLATE_IS(s_zlibE_template))
            filter_name = pfn->FlateDecode;
        else if (TEMPLATE_IS(s_LZWE_template))
            filter_name = pfn->LZWDecode;
        else if (TEMPLATE_IS(s_PNGPE_template)) {
            /* This is a predictor for FlateDecode or LZWEncode. */
            const stream_PNGP_state *const ss =
                (const stream_PNGP_state *)st;

            decode_parms =
                cos_dict_alloc(pdev, "pdf_put_image_filters(decode_parms)");
            if (decode_parms == 0)
                return_error(gs_error_VMerror);
            CHECK(cos_dict_put_c_key_int(decode_parms, "/Predictor",
                                         ss->Predictor));
            CHECK(cos_dict_put_c_key_int(decode_parms, "/Columns",
                                         ss->Columns));
            if (ss->Colors != 1)
                CHECK(cos_dict_put_c_key_int(decode_parms, "/Colors",
                                             ss->Colors));
            if (ss->BitsPerComponent != 8)
                CHECK(cos_dict_put_c_key_int(decode_parms,
                                             "/BitsPerComponent",
                                             ss->BitsPerComponent));
        } else if (TEMPLATE_IS(s_RLE_template))
            filter_name = pfn->RunLengthDecode;
#undef TEMPLATE_IS
    }
    if (filter_name) {
        if (binary_ok) {
            CHECK(cos_dict_put_c_strings(pcd, pfn->Filter, filter_name));
            if (decode_parms)
                CHECK(cos_dict_put_c_key_object(pcd, pfn->DecodeParms,
                                                COS_OBJECT(decode_parms)));
        } else {
            cos_array_t *pca =
                cos_array_alloc(pdev, "pdf_put_image_filters(Filters)");

            if (pca == 0)
                return_error(gs_error_VMerror);
            CHECK(cos_array_add_c_string(pca, pfn->ASCII85Decode));
            CHECK(cos_array_add_c_string(pca, filter_name));
            CHECK(cos_dict_put_c_key_object(pcd, pfn->Filter,
                                            COS_OBJECT(pca)));
            if (decode_parms) {
                pca = cos_array_alloc(pdev,
                                      "pdf_put_image_filters(DecodeParms)");
                if (pca == 0)
                    return_error(gs_error_VMerror);
                CHECK(cos_array_add_c_string(pca, "null"));
                CHECK(cos_array_add_object(pca, COS_OBJECT(decode_parms)));
                CHECK(cos_dict_put_c_key_object(pcd, pfn->DecodeParms,
                                                COS_OBJECT(pca)));
            }
        }
    } else if (!binary_ok)
        CHECK(cos_dict_put_c_strings(pcd, pfn->Filter, pfn->ASCII85Decode));
    return 0;
}

/* Add a Flate compression filter to a binary writer. */
static int
pdf_flate_binary(gx_device_pdf *pdev, psdf_binary_writer *pbw)
{
    const stream_template *templat = (pdev->CompatibilityLevel < 1.3 ?
                    &s_LZWE_template : &s_zlibE_template);
    stream_state *st = s_alloc_state(pdev->pdf_memory, templat->stype,
                                     "pdf_write_function");

    if (st == 0)
        return_error(gs_error_VMerror);
    if (templat->set_defaults)
        templat->set_defaults(st);
    return psdf_encode_binary(pbw, templat, st);
}

/*
 * Begin a data stream.  The client has opened the object and written
 * the << and any desired dictionary keys.
 */
int
pdf_begin_data(gx_device_pdf *pdev, pdf_data_writer_t *pdw)
{
    return pdf_begin_data_stream(pdev, pdw,
                                 DATA_STREAM_BINARY | DATA_STREAM_COMPRESS, 0);
}

int
pdf_append_data_stream_filters(gx_device_pdf *pdev, pdf_data_writer_t *pdw,
                      int orig_options, gs_id object_id)
{
    stream *s = pdev->strm;
    int options = orig_options;
#define USE_ASCII85 1
#define USE_FLATE 2
    static const char *const fnames[4] = {
        "", "/Filter/ASCII85Decode", "/Filter/FlateDecode",
        "/Filter[/ASCII85Decode/FlateDecode]"
    };
    static const char *const fnames1_2[4] = {
        "", "/Filter/ASCII85Decode", "/Filter/LZWDecode",
        "/Filter[/ASCII85Decode/LZWDecode]"
    };
    int filters = 0;
    int code;

    if (options & DATA_STREAM_COMPRESS) {
        filters |= USE_FLATE;
        options |= DATA_STREAM_BINARY;
    }
    if ((options & DATA_STREAM_BINARY) && !pdev->binary_ok)
        filters |= USE_ASCII85;
    if (!(options & DATA_STREAM_NOLENGTH)) {
        stream_puts(s, (pdev->CompatibilityLevel < 1.3 ?
            fnames1_2[filters] : fnames[filters]));
        if (pdev->ResourcesBeforeUsage) {
            pdw->length_pos = stell(s) + 8;
            stream_puts(s, "/Length             >>stream\n");
            pdw->length_id = -1;
        } else {
            pdw->length_pos = -1;
            pdw->length_id = pdf_obj_ref(pdev);
            pprintld1(s, "/Length %ld 0 R>>stream\n", pdw->length_id);
        }
    }
    if (options & DATA_STREAM_ENCRYPT) {
        code = pdf_begin_encrypt(pdev, &s, object_id);
        if (code < 0)
            return code;
        pdev->strm = s;
        pdw->encrypted = true;
    } else
        pdw->encrypted = false;
    if (options & DATA_STREAM_BINARY) {
        code = psdf_begin_binary((gx_device_psdf *)pdev, &pdw->binary);
        if (code < 0)
            return code;
    } else {
        code = 0;
        pdw->binary.target = pdev->strm;
        pdw->binary.dev = (gx_device_psdf *)pdev;
        pdw->binary.strm = pdev->strm;
    }
    pdw->start = stell(s);
    if (filters & USE_FLATE)
        code = pdf_flate_binary(pdev, &pdw->binary);
    return code;
#undef USE_ASCII85
#undef USE_FLATE
}

int
pdf_begin_data_stream(gx_device_pdf *pdev, pdf_data_writer_t *pdw,
                      int options, gs_id object_id)
{   int code;
    /* object_id is an unused rudiment from the old code,
       when the encription was applied when creating the stream.
       The new code encrypts than copying stream from the temporary file. */
    pdw->pdev = pdev;  /* temporary for backward compatibility of pdf_end_data prototype. */
    pdw->binary.target = pdev->strm;
    pdw->binary.dev = (gx_device_psdf *)pdev;
    pdw->binary.strm = 0;		/* for GC in case of failure */
    code = pdf_open_aside(pdev, resourceNone, gs_no_id, &pdw->pres, !object_id,
                options);
    if (object_id != 0)
        pdf_reserve_object_id(pdev, pdw->pres, object_id);
    pdw->binary.strm = pdev->strm;
    return code;
}

/* End a data stream. */
int
pdf_end_data(pdf_data_writer_t *pdw)
{   int code;

    code = pdf_close_aside(pdw->pdev);
    if (code < 0)
        return code;
    code = COS_WRITE_OBJECT(pdw->pres->object, pdw->pdev, resourceNone);
    if (code < 0)
        return code;
    return 0;
}

/* Create a Function object. */
static int pdf_function_array(gx_device_pdf *pdev, cos_array_t *pca,
                               const gs_function_info_t *pinfo);
int
pdf_function_scaled(gx_device_pdf *pdev, const gs_function_t *pfn,
                    const gs_range_t *pranges, cos_value_t *pvalue)
{
    if (pranges == NULL)
        return pdf_function(pdev, pfn, pvalue);
    {
        /*
         * Create a temporary scaled function.  Note that the ranges
         * represent the inverse scaling from what gs_function_make_scaled
         * expects.
         */
        gs_memory_t *mem = pdev->pdf_memory;
        gs_function_t *psfn;
        gs_range_t *ranges = (gs_range_t *)
            gs_alloc_byte_array(mem, pfn->params.n, sizeof(gs_range_t),
                                "pdf_function_scaled");
        int i, code;

        if (ranges == 0)
            return_error(gs_error_VMerror);
        for (i = 0; i < pfn->params.n; ++i) {
            double rbase = pranges[i].rmin;
            double rdiff = pranges[i].rmax - rbase;
            double invbase = -rbase / rdiff;

            ranges[i].rmin = invbase;
            ranges[i].rmax = invbase + 1.0 / rdiff;
        }
        code = gs_function_make_scaled(pfn, &psfn, ranges, mem);
        if (code >= 0) {
            code = pdf_function(pdev, psfn, pvalue);
            gs_function_free(psfn, true, mem);
        }
        gs_free_object(mem, ranges, "pdf_function_scaled");
        return code;
    }
}
static int
pdf_function_aux(gx_device_pdf *pdev, const gs_function_t *pfn,
             pdf_resource_t **ppres)
{
    gs_function_info_t info;
    cos_param_list_writer_t rlist;
    pdf_resource_t *pres;
    cos_object_t *pcfn;
    cos_dict_t *pcd;
    int code = pdf_alloc_resource(pdev, resourceFunction, gs_no_id, &pres, -1);

    if (code < 0) {
        *ppres = 0;
        return code;
    }
    *ppres = pres;
    pcfn = pres->object;
    gs_function_get_info(pfn, &info);
    if (FunctionType(pfn) == function_type_ArrayedOutput) {
        /*
         * Arrayed Output Functions are used internally to represent
         * Shading Function entries that are arrays of Functions.
         * They require special handling.
         */
        cos_array_t *pca;

        cos_become(pcfn, cos_type_array);
        pca = (cos_array_t *)pcfn;
        return pdf_function_array(pdev, pca, &info);
    }
    if (info.DataSource != 0) {
        psdf_binary_writer writer;
        stream *save = pdev->strm;
        cos_stream_t *pcos;
        stream *s;

        cos_become(pcfn, cos_type_stream);
        pcos = (cos_stream_t *)pcfn;
        pcd = cos_stream_dict(pcos);
        s = cos_write_stream_alloc(pcos, pdev, "pdf_function");
        if (s == 0)
            return_error(gs_error_VMerror);
        pdev->strm = s;
        code = psdf_begin_binary((gx_device_psdf *)pdev, &writer);
        if (code >= 0 && info.data_size > 30	/* 30 is arbitrary */
            )
            code = pdf_flate_binary(pdev, &writer);
        if (code >= 0) {
            static const pdf_filter_names_t fnames = {
                PDF_FILTER_NAMES
            };

            code = pdf_put_filters(pcd, pdev, writer.strm, &fnames);
        }
        if (code >= 0) {
            byte buf[100];		/* arbitrary */
            ulong pos;
            uint count;
            const byte *ptr;

            for (pos = 0; pos < info.data_size; pos += count) {
                count = min(sizeof(buf), info.data_size - pos);
                data_source_access_only(info.DataSource, pos, count, buf,
                                        &ptr);
                stream_write(writer.strm, ptr, count);
            }
            code = psdf_end_binary(&writer);
            s_close_filters(&s, s->strm);
        }
        pdev->strm = save;
        if (code < 0)
            return code;
    } else {
        cos_become(pcfn, cos_type_dict);
        pcd = (cos_dict_t *)pcfn;
    }
    if (info.Functions != 0) {
        cos_array_t *functions =
            cos_array_alloc(pdev, "pdf_function(Functions)");
        cos_value_t v;

        if (functions == 0)
            return_error(gs_error_VMerror);
        if ((code = pdf_function_array(pdev, functions, &info)) < 0 ||
            (code = cos_dict_put_c_key(pcd, "/Functions",
                                       COS_OBJECT_VALUE(&v, functions))) < 0
            ) {
            COS_FREE(functions, "pdf_function(Functions)");
            return code;
        }
    }
    code = cos_param_list_writer_init(pdev, &rlist, pcd, PRINT_BINARY_OK);
    if (code < 0)
        return code;
    return gs_function_get_params(pfn, (gs_param_list *)&rlist);
}
static int
functions_equal(gx_device_pdf * pdev, pdf_resource_t *pres0, pdf_resource_t *pres1)
{
    return true;
}
int
pdf_function(gx_device_pdf *pdev, const gs_function_t *pfn, cos_value_t *pvalue)
{
    pdf_resource_t *pres;
    int code = pdf_function_aux(pdev, pfn, &pres);

    if (code < 0)
        return code;
    if (pres->object->md5_valid)
        pres->object->md5_valid = 0;

    code = pdf_substitute_resource(pdev, &pres, resourceFunction, functions_equal, false);
    if (code < 0)
        return code;
    pres->where_used |= pdev->used_mask;
    COS_OBJECT_VALUE(pvalue, pres->object);
    return 0;
}
static int pdf_function_array(gx_device_pdf *pdev, cos_array_t *pca,
                               const gs_function_info_t *pinfo)
{
    int i, code = 0;
    cos_value_t v;

    for (i = 0; i < pinfo->num_Functions; ++i) {
        if ((code = pdf_function(pdev, pinfo->Functions[i], &v)) < 0 ||
            (code = cos_array_add(pca, &v)) < 0
            ) {
            break;
        }
    }
    return code;
}

/* Write a Function object. */
int
pdf_write_function(gx_device_pdf *pdev, const gs_function_t *pfn, int64_t *pid)
{
    cos_value_t value;
    int code = pdf_function(pdev, pfn, &value);

    if (code < 0)
        return code;
    *pid = value.contents.object->id;
    return 0;
}

int
free_function_refs(gx_device_pdf *pdev, cos_object_t *pco)
{
    char key[] = "/Functions";
    cos_value_t *v, v2;

    if (cos_type(pco) == cos_type_dict) {
        v = (cos_value_t *)cos_dict_find((const cos_dict_t *)pco, (const byte *)key, strlen(key));
        if (v && v->value_type == COS_VALUE_OBJECT) {
            if (cos_type(v->contents.object) == cos_type_array){
                int code=0;
                while (code == 0) {
                    code = cos_array_unadd((cos_array_t *)v->contents.object, &v2);
                }
            }
        }
    }
    if (cos_type(pco) == cos_type_array) {
        int64_t index;
        cos_array_t *pca = (cos_array_t *)pco;
        const cos_array_element_t *element = cos_array_element_first(pca);
        cos_value_t *v;

        while (element) {
            element = cos_array_element_next(element, &index, (const cos_value_t **)&v);
            if (v->value_type == COS_VALUE_OBJECT) {
                if (pdf_find_resource_by_resource_id(pdev, resourceFunction, v->contents.object->id)){
                    v->value_type = COS_VALUE_CONST;
                    /* Need to remove the element from the array here */
                }
            }
        }
    }
    return 0;
}

/* Write a FontBBox dictionary element. */
int
pdf_write_font_bbox(gx_device_pdf *pdev, const gs_int_rect *pbox)
{
    stream *s = pdev->strm;
    /*
     * AR 4 doesn't like fonts with empty FontBBox, which
     * happens when the font contains only space characters.
     * Small bbox causes AR 4 to display a hairline. So we use
     * the full BBox.
     */
    int x = pbox->q.x + ((pbox->p.x == pbox->q.x) ? 1000 : 0);
    int y = pbox->q.y + ((pbox->p.y == pbox->q.y) ? 1000 : 0);

    pprintd4(s, "/FontBBox[%d %d %d %d]",
             pbox->p.x, pbox->p.y, x, y);
    return 0;
}

/* Write a FontBBox dictionary element using floats for the values. */
int
pdf_write_font_bbox_float(gx_device_pdf *pdev, const gs_rect *pbox)
{
    stream *s = pdev->strm;
    /*
     * AR 4 doesn't like fonts with empty FontBBox, which
     * happens when the font contains only space characters.
     * Small bbox causes AR 4 to display a hairline. So we use
     * the full BBox.
     */
    float x = pbox->q.x + ((pbox->p.x == pbox->q.x) ? 1000 : 0);
    float y = pbox->q.y + ((pbox->p.y == pbox->q.y) ? 1000 : 0);

    pprintg4(s, "/FontBBox[%g %g %g %g]",
             pbox->p.x, pbox->p.y, x, y);
    return 0;
}
