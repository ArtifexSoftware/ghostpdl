/* Copyright (C) 2001-2020 Artifex Software, Inc.
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

/* Top level PDF access routines */
#include "ghostpdf.h"
#include "plmain.h"
#include "pdf_types.h"
#include "pdf_dict.h"
#include "pdf_array.h"
#include "pdf_int.h"
#include "pdf_misc.h"
#include "pdf_stack.h"
#include "pdf_file.h"
#include "pdf_loop_detect.h"
#include "pdf_trans.h"
#include "pdf_gstate.h"
#include "stream.h"
#include "strmio.h"
#include "pdf_colour.h"
#include "pdf_font.h"
#include "pdf_text.h"
#include "pdf_page.h"
#include "pdf_check.h"
#include "pdf_optcontent.h"
#include "pdf_sec.h"
#include "pdf_doc.h"
#include "pdf_repair.h"
#include "pdf_xref.h"
#include "pdf_device.h"

/*
 * Convenience routine to check if a given string exists in a dictionary
 * verify its contents and print it in a particular fashion to stdout. This
 * is used to display information about the PDF in response to -dPDFINFO
 */
static int dump_info_string(pdf_context *ctx, pdf_dict *source_dict, const char *Key)
{
    int code;
    pdf_string *s = NULL;
    char *Cstr;

    code = pdfi_dict_knownget_type(ctx, source_dict, Key, PDF_STRING, (pdf_obj **)&s);
    if (code > 0) {
        Cstr = (char *)gs_alloc_bytes(ctx->memory, s->length + 1, "Working memory for string dumping");
        if (Cstr) {
            memcpy(Cstr, s->data, s->length);
            Cstr[s->length] = 0x00;
            dmprintf2(ctx->memory, "%s: %s\n", Key, Cstr);
            gs_free_object(ctx->memory, Cstr, "Working memory for string dumping");
        }
        code = 0;
    }
    pdfi_countdown(s);

    return code;
}

static int pdfi_output_metadata(pdf_context *ctx)
{
    int code = 0;

    if (ctx->num_pages > 1)
        dmprintf2(ctx->memory, "\n        %s has %"PRIi64" pages\n\n", ctx->filename, ctx->num_pages);
    else
        dmprintf2(ctx->memory, "\n        %s has %"PRIi64" page.\n\n", ctx->filename, ctx->num_pages);

    if (ctx->Info != NULL) {
        pdf_name *n = NULL;
        char *Cstr;

        code = dump_info_string(ctx, ctx->Info, "Title");
        if (code < 0) {
            if (ctx->args.pdfstoponerror)
                return code;
        }

        code = dump_info_string(ctx, ctx->Info, "Author");
        if (code < 0) {
            if (ctx->args.pdfstoponerror)
                return code;
        }

        code = dump_info_string(ctx, ctx->Info, "Subject");
        if (code < 0) {
            if (ctx->args.pdfstoponerror)
                return code;
        }

        code = dump_info_string(ctx, ctx->Info, "Keywords");
        if (code < 0) {
            if (ctx->args.pdfstoponerror)
                return code;
        }

        code = dump_info_string(ctx, ctx->Info, "Creator");
        if (code < 0) {
            if (ctx->args.pdfstoponerror)
                return code;
        }

        code = dump_info_string(ctx, ctx->Info, "Producer");
        if (code < 0) {
            if (ctx->args.pdfstoponerror)
                return code;
        }

        code = dump_info_string(ctx, ctx->Info, "CreationDate");
        if (code < 0) {
            if (ctx->args.pdfstoponerror)
                return code;
        }

        code = dump_info_string(ctx, ctx->Info, "ModDate");
        if (code < 0) {
            if (ctx->args.pdfstoponerror)
                return code;
        }


        code = pdfi_dict_knownget_type(ctx, ctx->Info, "Trapped", PDF_NAME, (pdf_obj **)&n);
        if (code > 0) {
            Cstr = (char *)gs_alloc_bytes(ctx->memory, n->length + 1, "Working memory for string dumping");
            if (Cstr) {
                memcpy(Cstr, n->data, n->length);
                Cstr[n->length] = 0x00;
                dmprintf1(ctx->memory, "Trapped: %s\n\n", Cstr);
                gs_free_object(ctx->memory, Cstr, "Working memory for string dumping");
            }
            code = 0;
        }
        pdfi_countdown(n);
        n = NULL;
    }
    return code;
}

/*
 * Convenience routine to check if a given *Box exists in a page dictionary
 * verify its contents and print it in a particular fashion to stdout. This
 * is used to display information about the PDF in response to -dPDFINFO
 */
static int pdfi_dump_box(pdf_context *ctx, pdf_dict *page_dict, const char *Key)
{
    int code, i;
    pdf_array *a = NULL;
    double f;

    code = pdfi_dict_knownget_type(ctx, page_dict, Key, PDF_ARRAY, (pdf_obj **)&a);
    if (code > 0) {
        if (pdfi_array_size(a) != 4) {
            dmprintf1(ctx->memory, "Error - %s does not contain 4 values.\n", Key);
            code = gs_note_error(gs_error_rangecheck);
        } else {
            dmprintf1(ctx->memory, " %s: [", Key);
            for (i = 0; i < pdfi_array_size(a); i++) {
                code = pdfi_array_get_number(ctx, a, (uint64_t)i, &f);
                if (i != 0)
                    dmprintf(ctx->memory, " ");
                if (code == 0) {
                    if (a->values[i]->type == PDF_INT)
                        dmprintf1(ctx->memory, "%"PRIi64"", ((pdf_num *)a->values[i])->value.i);
                    else
                        dmprintf1(ctx->memory, "%f", ((pdf_num *)a->values[i])->value.d);
                } else {
                    dmprintf(ctx->memory, "NAN");
                }
            }
            dmprintf(ctx->memory, "]");
        }
    }
    pdfi_countdown(a);
    return code;
}

/*
 * This routine along with pdfi_output_metadtaa above, dumps certain kinds
 * of metadata from the PDF file, and from each page in the PDF file. It is
 * intended to duplicate the pdf_info.ps functionality of the PostScript-based
 * PDF interpreter in Ghostscript.
 *
 * It is not yet complete, we don't allow an option for dumping media sizes
 * we always emit them, and the switches -dDumpFontsNeeded, -dDumpXML,
 * -dDumpFontsUsed and -dShowEmbeddedFonts are not implemented at all yet.
 */
static int pdfi_output_page_info(pdf_context *ctx, uint64_t page_num)
{
    int code;
    bool known = false;
    double f;
    pdf_dict *page_dict = NULL;

    code = pdfi_page_get_dict(ctx, page_num, &page_dict);
    if (code < 0)
        return code;

    dmprintf1(ctx->memory, "Page %"PRIi64"", page_num + 1);

    code = pdfi_dict_knownget_number(ctx, page_dict, "UserUnit", &f);
    if (code > 0)
        dmprintf1(ctx->memory, " UserUnit: %f ", f);
    if (code < 0) {
        pdfi_countdown(page_dict);
        return code;
    }

    code = pdfi_dump_box(ctx, page_dict, "MediaBox");
    if (code < 0) {
        if (code != gs_error_undefined && ctx->args.pdfstoponerror) {
            pdfi_countdown(page_dict);
            return code;
        }
    }

    code = pdfi_dump_box(ctx, page_dict, "CropBox");
    if (code < 0) {
        if (code != gs_error_undefined && ctx->args.pdfstoponerror) {
            pdfi_countdown(page_dict);
            return code;
        }
    }

    code = pdfi_dump_box(ctx, page_dict, "BleedBox");
    if (code < 0) {
        if (code != gs_error_undefined && ctx->args.pdfstoponerror) {
            pdfi_countdown(page_dict);
            return code;
        }
    }

    code = pdfi_dump_box(ctx, page_dict, "TrimBox");
    if (code < 0) {
        if (code != gs_error_undefined && ctx->args.pdfstoponerror) {
            pdfi_countdown(page_dict);
            return code;
        }
    }

    code = pdfi_dump_box(ctx, page_dict, "ArtBox");
    if (code < 0) {
        if (code != gs_error_undefined && ctx->args.pdfstoponerror) {
            pdfi_countdown(page_dict);
            return code;
        }
    }

    code = pdfi_dict_knownget_number(ctx, page_dict, "Rotate", &f);
    if (code > 0)
        dmprintf1(ctx->memory, "    Rotate = %d ", (int)f);
    if (code < 0) {
        pdfi_countdown(page_dict);
        return code;
    }

    code = pdfi_check_page(ctx, page_dict, false);
    if (code < 0) {
        if (ctx->args.pdfstoponerror)
            return code;
    } else {
        if (ctx->page_has_transparency)
            dmprintf(ctx->memory, "     Page uses transparency features");
    }

    code = pdfi_dict_known(ctx, page_dict, "Annots", &known);
    if (code < 0) {
        if (code != gs_error_undefined && ctx->args.pdfstoponerror)
            return code;
    } else {
        if (known == true)
            dmprintf(ctx->memory, "     Page contains Annotations");
    }

    dmprintf(ctx->memory, "\n\n");
    pdfi_countdown(page_dict);

    return 0;
}

static void
pdfi_report_errors(pdf_context *ctx)
{
    int code;

    if (ctx->pdf_errors == E_PDF_NOERROR && ctx->pdf_warnings == W_PDF_NOWARNING)
        return;

    if (ctx->pdf_errors != E_PDF_NOERROR) {
        dmprintf(ctx->memory, "The following errors were encountered at least once while processing this file:\n");
        if (ctx->pdf_errors & E_PDF_NOHEADER)
            dmprintf(ctx->memory, "\tThe file does not have a valid PDF header.\n");
        if (ctx->pdf_errors & E_PDF_NOHEADERVERSION)
            dmprintf(ctx->memory, "\tThe file header does not contain a version number.\n");
        if (ctx->pdf_errors & E_PDF_NOSTARTXREF)
            dmprintf(ctx->memory, "\tThe file does not contain a 'startxref' token.\n");
        if (ctx->pdf_errors & E_PDF_BADSTARTXREF)
            dmprintf(ctx->memory, "\tThe file contain a 'startxref' token, but it does not point to an xref table.\n");
        if (ctx->pdf_errors & E_PDF_BADXREFSTREAM)
            dmprintf(ctx->memory, "\tThe file uses an XRefStm, but the stream is invalid.\n");
        if (ctx->pdf_errors & E_PDF_BADXREF)
            dmprintf(ctx->memory, "\tThe file uses an xref table, but the table is invalid.\n");
        if (ctx->pdf_errors & E_PDF_SHORTXREF)
            dmprintf(ctx->memory, "\tThe file uses an xref table, but the table has ferwer entires than expected.\n");
        if (ctx->pdf_errors & E_PDF_MISSINGENDSTREAM)
            dmprintf(ctx->memory, "\tA content stream is missing an 'endstream' token.\n");
        if (ctx->pdf_errors & E_PDF_MISSINGENDOBJ)
            dmprintf(ctx->memory, "\tAn object is missing an 'endobj' token.\n");
        if (ctx->pdf_errors & E_PDF_UNKNOWNFILTER)
            dmprintf(ctx->memory, "\tThe file attempted to use an unrecognised decompression filter.\n");
        if (ctx->pdf_errors & E_PDF_MISSINGWHITESPACE)
            dmprintf(ctx->memory, "\tA missing white space was detected while trying to read a number.\n");
        if (ctx->pdf_errors & E_PDF_MALFORMEDNUMBER)
            dmprintf(ctx->memory, "\tA malformed number was detected.\n");
        if (ctx->pdf_errors & E_PDF_UNESCAPEDSTRING)
            dmprintf(ctx->memory, "\tA string used a '(' character without an escape.\n");
        if (ctx->pdf_errors & E_PDF_BADOBJNUMBER)
            dmprintf(ctx->memory, "\tThe file contained a reference to an object number larger than the number of xref entries.\n");
        if (ctx->pdf_errors & E_PDF_TOKENERROR)
            dmprintf(ctx->memory, "\tAn operator in a content stream returned an error.\n");
        if (ctx->pdf_errors & E_PDF_KEYWORDTOOLONG)
            dmprintf(ctx->memory, "\tA keyword (outside a content stream) was too long (> 255).\n");
        if (ctx->pdf_errors & E_PDF_BADPAGETYPE)
            dmprintf(ctx->memory, "\tAn entry in the Pages array was a dictionary with a /Type key whose value was not /Page.\n");
        if (ctx->pdf_errors & E_PDF_CIRCULARREF)
            dmprintf(ctx->memory, "\tAn indirect object caused a circular reference to itself.\n");
        if (ctx->pdf_errors & E_PDF_UNREPAIRABLE)
            dmprintf(ctx->memory, "\tFile could not be repaired.\n");
        if (ctx->pdf_errors & E_PDF_REPAIRED)
            dmprintf(ctx->memory, "\tFile had an error that needed to be repaired.\n");
        if (ctx->pdf_errors & E_PDF_BADSTREAM)
            dmprintf(ctx->memory, "\tFile had an error in a stream.\n");
        if (ctx->pdf_errors & E_PDF_MISSINGOBJ)
            dmprintf(ctx->memory, "\tThe file contained a reference to an object number that is missing.\n");
        if (ctx->pdf_errors & E_PDF_BADPAGEDICT)
            dmprintf(ctx->memory, "\tThe file contained a bad Pages dictionary.  Couldn't process it.\n");
        if (ctx->pdf_errors & E_PDF_OUTOFMEMORY)
            dmprintf(ctx->memory, "\tThe interpeter ran out of memory while processing this file.\n");
        if (ctx->pdf_errors & E_PDF_PAGEDICTERROR)
            dmprintf(ctx->memory, "\tA page had a bad Page dict and was skipped.\n");
        if (ctx->pdf_errors & E_PDF_STACKUNDERFLOWERROR)
            dmprintf(ctx->memory, "\tToo few operands for an operator, operator was skipped.\n");
        if (ctx->pdf_errors & E_PDF_BADSTREAMDICT)
            dmprintf(ctx->memory, "\tA stream dictionary was not followed by a 'stream' keyword.\n");
        if (ctx->pdf_errors & E_PDF_DEREF_FREE_OBJ)
            dmprintf(ctx->memory, "\tAn attempt was made to access an object marked as free in the xref.\n");
    }

    if (ctx->pdf_warnings != W_PDF_NOWARNING) {
        dmprintf(ctx->memory, "The following warnings were encountered at least once while processing this file:\n");
        if (ctx->pdf_warnings & W_PDF_BAD_XREF_SIZE)
            dmprintf(ctx->memory, "\tThe file contains an xref with more entries than the declared /Size in the trailer dictionary.\n");
        if (ctx->pdf_warnings & W_PDF_BAD_INLINEFILTER)
            dmprintf(ctx->memory, "\tThe file attempted to use an inline decompression filter other than on an inline image.\n");
        if (ctx->pdf_warnings & W_PDF_BAD_INLINECOLORSPACE)
            dmprintf(ctx->memory, "\tThe file attempted to use an inline image color space other than on an inline image.\n");
        if (ctx->pdf_warnings & W_PDF_BAD_INLINEIMAGEKEY)
            dmprintf(ctx->memory, "\tThe file attempted to use an inline image dictionary key with an image XObject.\n");
        if (ctx->pdf_warnings & W_PDF_IMAGE_ERROR)
            dmprintf(ctx->memory, "\tThe file has an error when rendering an image.\n");
        if (ctx->pdf_warnings & W_PDF_BAD_IMAGEDICT)
            dmprintf(ctx->memory, "\tThe file attempted to use an image with a bad value in the image dict.\n");
        if (ctx->pdf_warnings & W_PDF_TOOMANYQ)
            dmprintf(ctx->memory, "\tA content stream had unmatched q/Q operations (too many Q's).\n");
        if (ctx->pdf_warnings & W_PDF_TOOMANYq)
            dmprintf(ctx->memory, "\tA content stream had unmatched q/Q operations (too many q's).\n");
        if (ctx->pdf_warnings & W_PDF_STACKGARBAGE)
            dmprintf(ctx->memory, "\tA content stream left entries on the stack.\n");
        if (ctx->pdf_warnings & W_PDF_STACKUNDERFLOW)
            dmprintf(ctx->memory, "\tA content stream consumed too many arguments (stack underflow).\n");
        if (ctx->pdf_warnings & W_PDF_GROUPERROR)
            dmprintf(ctx->memory, "\tA transparency group was not terminated.\n");
        if (ctx->pdf_warnings & W_PDF_OPINVALIDINTEXT)
            dmprintf(ctx->memory, "\tAn operator (eg q/Q) was used in a text block where it is not permitted.\n");
        if (ctx->pdf_warnings & W_PDF_NOTINCHARPROC)
            dmprintf(ctx->memory, "\tA d0 or d1 operator was encountered outside a CharProc.\n");
        if (ctx->pdf_warnings & W_PDF_NESTEDTEXTBLOCK)
            dmprintf(ctx->memory, "\tEncountered a BT while already in a text block.\n");
        if (ctx->pdf_warnings & W_PDF_ETNOTEXTBLOCK)
            dmprintf(ctx->memory, "\tEncountered an ET while not in a text block.\n");
        if (ctx->pdf_warnings & W_PDF_TEXTOPNOBT)
            dmprintf(ctx->memory, "\tEncountered a text position or show operator without a prior BT operator.\n");
        if (ctx->pdf_warnings & W_PDF_BADICC_USE_ALT)
            dmprintf(ctx->memory, "\tCouldn't set ICC profile space, used Alternate space instead.\n");
        if (ctx->pdf_warnings & W_PDF_BADICC_USECOMPS)
            dmprintf(ctx->memory, "\tCouldn't set ICC profile space, used number of profile components to select a space.\n");
        if (ctx->pdf_warnings & W_PDF_BADTRSWITCH)
            dmprintf(ctx->memory, "\tSwitching from a text rendering mode including clip, to a mode which does not, is invalid.\n");
        if (ctx->pdf_warnings & W_PDF_BADSHADING)
            dmprintf(ctx->memory, "\tThe file has an error when interpreting a Shading object.\n");
        if (ctx->pdf_warnings & W_PDF_BADPATTERN)
            dmprintf(ctx->memory, "\tThe file has an error when interpreting a Pattern object.\n");
        if (ctx->pdf_warnings & W_PDF_NONSTANDARD_OP)
            dmprintf(ctx->memory, "\tThe file uses a non-standard PDF operator.\n");
        if (ctx->pdf_warnings & W_PDF_NUM_EXPONENT)
            dmprintf(ctx->memory, "\tThe file uses numbers with exponents, which is not standard PDF.\n");
        if (ctx->pdf_warnings & W_PDF_STREAM_HAS_CONTENTS)
            dmprintf(ctx->memory, "\tA stream dictionary has no stream and instead uses a /Contents entry, which is invalid.\n");
        if (ctx->pdf_warnings & W_PDF_STREAM_BAD_DECODEPARMS)
            dmprintf(ctx->memory, "\tA stream dictionary has an invalid /DecodeParms entry\n");
    }

    dmprintf(ctx->memory, "\n   **** This file had errors that were repaired or ignored.\n");
    if (ctx->Info) {
        pdf_string *s = NULL;

        code = pdfi_dict_knownget_type(ctx, ctx->Info, "Producer", PDF_STRING, (pdf_obj **)&s);
        if (code > 0) {
            char *cs;

            cs = (char *)gs_alloc_bytes(ctx->memory, s->length + 1, "temporary string for error report");
            memcpy(cs, s->data, s->length);
            cs[s->length] = 0x00;
            dmprintf1(ctx->memory, "   **** The file was produced by: \n   **** >>>> %s <<<<\n", cs);
            gs_free_object(ctx->memory, cs, "temporary string for error report");
        }
        pdfi_countdown(s);
    }
    dmprintf(ctx->memory, "   **** Please notify the author of the software that produced this\n");
    dmprintf(ctx->memory, "   **** file that it does not conform to Adobe's published PDF\n");
    dmprintf(ctx->memory, "   **** specification.\n\n");
}

/* Name table
 * I've been trying to avoid this for as long as possible, but it seems it cannot
 * be evaded. We need functions to get an index for a given string (which will
 * add the string to the table if its not present) and to cleear up the table
 * on finishing a PDF file.
 */

int pdfi_get_name_index(pdf_context *ctx, char *name, int len, unsigned int *returned)
{
    pdfi_name_entry *e = NULL, *last_entry = NULL, *new_entry = NULL;
    int index = 0;

    if (ctx->name_table == NULL) {
        e = NULL;
    } else {
        e = ctx->name_table;
    }

    while(e != NULL) {
        if (e->len == len) {
            if (memcmp(e->name, name, e->len) == 0) {
                *returned = e->index;
                return 0;
            }
        }
        last_entry = e;
        index = e->index;
        e = e->next;
    }

    new_entry = (pdfi_name_entry *)gs_alloc_bytes(ctx->memory, sizeof(pdfi_name_entry), "Alloc name table entry");
    if (new_entry == NULL)
        return_error(gs_error_VMerror);
    memset(new_entry, 0x00, sizeof(pdfi_name_entry));
    new_entry->name = (char *)gs_alloc_bytes(ctx->memory, len+1, "Alloc name table name");
    if (new_entry->name == NULL) {
        gs_free_object(ctx->memory, new_entry, "Failed to allocate name entry");
            return_error(gs_error_VMerror);
    }
    memset(new_entry->name, 0x00, len+1);
    memcpy(new_entry->name, name, len);
    new_entry->len = len;
    new_entry->index = ++index;

    if (last_entry)
        last_entry->next = new_entry;
    else
        ctx->name_table = new_entry;

    *returned = new_entry->index;
    return 0;
}

static int pdfi_free_name_table(pdf_context *ctx)
{
    if (ctx->name_table) {
        pdfi_name_entry *next = NULL, *e = (pdfi_name_entry *)ctx->name_table;

        while (e != NULL) {
            next = (pdfi_name_entry *)e->next;
            gs_free_object(ctx->memory, e->name, "free name table entries");
            gs_free_object(ctx->memory, e, "free name table entries");
            e = next;
        }
    }
    ctx->name_table = NULL;
    return 0;
}

int pdfi_name_from_index(pdf_context *ctx, int index, unsigned char **name, unsigned int *len)
{
    pdfi_name_entry *e = (pdfi_name_entry *)ctx->name_table;

    while (e != NULL) {
        if (e->index == index) {
            *name = (unsigned char *)e->name;
            *len = e->len;
            return 0;
        }
        e = e->next;
    }

    return_error(gs_error_undefined);
}

int pdfi_separation_name_from_index(gs_gstate *pgs, gs_separation_name index, unsigned char **name, unsigned int *len)
{
    pdfi_int_gstate *igs = (pdfi_int_gstate *)pgs->client_data;
    pdf_context *ctx = NULL;
    pdfi_name_entry *e = NULL;

    if (igs == NULL)
        return_error(gs_error_undefined);

    ctx = igs->ctx;
    if (ctx == NULL)
        return_error(gs_error_undefined);

    e = (pdfi_name_entry *)ctx->name_table;

    while (e != NULL) {
        if (e->index == index) {
            *name = (unsigned char *)e->name;
            *len = e->len;
            return 0;
        }
        e = e->next;
    }

    return_error(gs_error_undefined);
}

/* These functions are used by the 'PL' implementation, eventually we will */
/* need to have custom PostScript operators to process the file or at      */
/* (least pages from it).                                                  */

int pdfi_close_pdf_file(pdf_context *ctx)
{
    if (ctx->main_stream) {
        if (ctx->main_stream->s) {
            sfclose(ctx->main_stream->s);
        }
        gs_free_object(ctx->memory, ctx->main_stream, "Closing main PDF file");
        ctx->main_stream = NULL;
    }
    ctx->main_stream_length = 0;
    return 0;
}

int pdfi_process_pdf_file(pdf_context *ctx, char *filename)
{
    int code = 0, i;

    code = pdfi_open_pdf_file(ctx, filename);
    if (code < 0) {
        goto exit;
    }

    /* Need to do this here so that ctx->writepdfmarks will be setup
     * It is also called in pdfi_page_render()
     * TODO: Should probably look into that..
     */
    pdfi_device_set_flags(ctx);
    /* Do any custom device configuration */
    pdfi_device_misc_config(ctx);

    /* TODO: Need to handle /Collection around this point, somehow.
     * see pdf_main.ps/pdf_collection_files()
     */

    code = pdfi_doc_trailer(ctx);

    /* Loop over each page and either render it or output the
     * required information.
     */
    for (i=0;i < ctx->num_pages;i++) {
        if (ctx->args.first_page != 0) {
            if (i < ctx->args.first_page - 1)
                continue;
        }
        if (ctx->args.last_page != 0) {
            if (i > ctx->args.last_page - 1)
                break;;
        }
        if (ctx->args.pdfinfo)
            code = pdfi_output_page_info(ctx, i);
        else
            code = pdfi_page_render(ctx, i, true);

        if (code < 0 && ctx->args.pdfstoponerror)
            goto exit;
        code = 0;
    }

 exit:
    pdfi_report_errors(ctx);

    pdfi_close_pdf_file(ctx);
    return code;
}

static int pdfi_init_file(pdf_context *ctx)
{
    int code = 0;
    pdf_obj *o = NULL;

    code = pdfi_read_xref(ctx);
    if (code < 0) {
        if (ctx->is_hybrid) {
            /* If its a hybrid file, and we failed to read the XrefStm, try
             * again, but this time read the xref table instead.
             */
            ctx->pdf_errors |= E_PDF_BADXREFSTREAM;
            pdfi_countdown(ctx->xref_table);
            ctx->xref_table = NULL;
            ctx->prefer_xrefstm = false;
            code = pdfi_read_xref(ctx);
            if (code < 0)
                goto exit;
        } else {
            ctx->pdf_errors |= E_PDF_BADXREF;
            goto exit;
        }
    }

    if (ctx->Trailer) {
        code = pdfi_dict_get(ctx, ctx->Trailer, "Encrypt", &o);
        if (code < 0 && code != gs_error_undefined)
            goto exit;
        if (code == 0) {
            code = pdfi_initialise_Decryption(ctx);
            if (code < 0)
                goto exit;
        }
    }

read_root:
    if (ctx->Trailer) {
        code = pdfi_read_Root(ctx);
        if (code < 0) {
            /* If we couldn#'t find the Root object, and we were using the XrefStm
             * from a hybrid file, then try again, but this time use the xref table
             */
            if (code == gs_error_undefined && ctx->is_hybrid && ctx->prefer_xrefstm) {
                ctx->pdf_errors |= E_PDF_BADXREFSTREAM;
                pdfi_countdown(ctx->xref_table);
                ctx->xref_table = NULL;
                ctx->prefer_xrefstm = false;
                code = pdfi_read_xref(ctx);
                if (code < 0) {
                    ctx->pdf_errors |= E_PDF_BADXREF;
                    goto exit;
                }
                code = pdfi_read_Root(ctx);
                if (code < 0)
                    goto exit;
            } else {
                int code1 = pdfi_repair_file(ctx);
                if (code1 < 0)
                    goto exit;
                goto read_root;
            }
        }
    }

    if (ctx->Trailer) {
        code = pdfi_read_Info(ctx);
        if (code < 0 && code != gs_error_undefined) {
            if (ctx->args.pdfstoponerror)
                goto exit;
            pdfi_clearstack(ctx);
        }
    }

    if (!ctx->Root) {
        dmprintf(ctx->memory, "Catalog dictionary not located in file, unable to proceed\n");
        return_error(gs_error_syntaxerror);
    }

    code = pdfi_read_Pages(ctx);
    if (code < 0)
        goto exit;

    code = pdfi_doc_page_array_init(ctx);
    if (code < 0)
        goto exit;

    if (ctx->num_pages == 0)
        dmprintf(ctx->memory, "\n   **** Warning: PDF document has no pages.\n");

    pdfi_read_OptionalRoot(ctx);

    if (ctx->args.pdfinfo) {
        code = pdfi_output_metadata(ctx);
        if (code < 0 && ctx->args.pdfstoponerror)
            goto exit;
    }

exit:
    pdfi_countdown(o);
    return code;
}

int pdfi_set_input_stream(pdf_context *ctx, stream *stm)
{
    byte *Buffer = NULL;
    char *s = NULL;
    float version = 0.0;
    gs_offset_t Offset = 0;
    int64_t bytes = 0;
    bool found = false;
    int code;

    /* In case of broken PDF files, the repair could run off the end of the
     * file, so make sure that doing so does *not* automagically close the file
     */
    stm->close_at_eod = false;

    ctx->main_stream = (pdf_c_stream *)gs_alloc_bytes(ctx->memory, sizeof(pdf_c_stream), "PDF interpreter allocate main PDF stream");
    if (ctx->main_stream == NULL)
        return_error(gs_error_VMerror);
    memset(ctx->main_stream, 0x00, sizeof(pdf_c_stream));
    ctx->main_stream->s = stm;

    Buffer = gs_alloc_bytes(ctx->memory, BUF_SIZE, "PDF interpreter - allocate working buffer for file validation");
    if (Buffer == NULL) {
        code = gs_error_VMerror;
        goto error;
    }

    /* Determine file size */
    pdfi_seek(ctx, ctx->main_stream, 0, SEEK_END);
    ctx->main_stream_length = pdfi_tell(ctx->main_stream);
    Offset = BUF_SIZE;
    bytes = BUF_SIZE;
    pdfi_seek(ctx, ctx->main_stream, 0, SEEK_SET);

    bytes = Offset = min(BUF_SIZE - 1, ctx->main_stream_length);

    if (ctx->args.pdfdebug)
        dmprintf(ctx->memory, "%% Reading header\n");

    bytes = pdfi_read_bytes(ctx, Buffer, 1, Offset, ctx->main_stream);
    if (bytes <= 0) {
        emprintf(ctx->memory, "Failed to read any bytes from input stream\n");
        code = gs_error_ioerror;
        goto error;
    }
    if (bytes < 8) {
        emprintf(ctx->memory, "Failed to read enough bytes for a valid PDF header from input stream\n");
        code = gs_error_ioerror;
        goto error;
    }
    Buffer[Offset] = 0x00;

    /* First check for existence of header */
    s = strstr((char *)Buffer, "%PDF");
    if (s == NULL) {
        if (ctx->args.pdfdebug) {
            if (ctx->filename)
                dmprintf1(ctx->memory, "%% File %s does not appear to be a PDF file (no %%PDF in first 2Kb of file)\n", ctx->filename);
            else
                dmprintf1(ctx->memory, "%% File %s does not appear to be a PDF stream (no %%PDF in first 2Kb of stream)\n", ctx->filename);
        }
        ctx->pdf_errors |= E_PDF_NOHEADER;
    } else {
        /* Now extract header version (may be overridden later) */
        if (sscanf(s + 5, "%f", &version) != 1) {
            if (ctx->args.pdfdebug)
                dmprintf(ctx->memory, "%% Unable to read PDF version from header\n");
            ctx->HeaderVersion = 0;
            ctx->pdf_errors |= E_PDF_NOHEADERVERSION;
        }
        else {
            ctx->HeaderVersion = version;
        }
        if (ctx->args.pdfdebug)
            dmprintf1(ctx->memory, "%% Found header, PDF version is %f\n", ctx->HeaderVersion);
    }

    /* Jump to EOF and scan backwards looking for startxref */
    pdfi_seek(ctx, ctx->main_stream, 0, SEEK_END);

    if (ctx->args.pdfdebug)
        dmprintf(ctx->memory, "%% Searching for 'startxerf' keyword\n");

    bytes = Offset;
    do {
        byte *last_lineend = NULL;
        uint32_t read;

        if (pdfi_seek(ctx, ctx->main_stream, ctx->main_stream_length - Offset, SEEK_SET) != 0) {
            emprintf1(ctx->memory, "File is smaller than %"PRIi64" bytes\n", (int64_t)Offset);
            code = gs_error_ioerror;
            goto error;
        }
        read = pdfi_read_bytes(ctx, Buffer, 1, bytes, ctx->main_stream);

        if (read <= 0) {
            emprintf1(ctx->memory, "Failed to read %"PRIi64" bytes from file\n", (int64_t)bytes);
            code = gs_error_ioerror;
            goto error;
        }

        read = bytes = read + (BUF_SIZE - bytes);

        while(read) {
            if (memcmp(Buffer + read - 9, "startxref", 9) == 0) {
                found = true;
                break;
            } else {
                if (Buffer[read - 1] == 0x0a || Buffer[read - 1] == 0x0d)
                    last_lineend = Buffer + read;
            }
            read--;
        }
        if (found) {
            byte *b = Buffer + read;

            if(sscanf((char *)b, " %ld", &ctx->startxref) != 1) {
                dmprintf(ctx->memory, "Unable to read offset of xref from PDF file\n");
            }
            break;
        } else {
            if (last_lineend) {
                uint32_t len = last_lineend - Buffer;
                memcpy(Buffer + bytes - len, last_lineend, len);
                bytes -= len;
            }
        }

        Offset += bytes;
    } while(Offset < ctx->main_stream_length);

    if (!found)
        ctx->pdf_errors |= E_PDF_NOSTARTXREF;

    code = pdfi_init_file(ctx);

error:
    gs_free_object(ctx->memory, Buffer, "PDF interpreter - allocate working buffer for file validation");
    return code;
}

int pdfi_open_pdf_file(pdf_context *ctx, char *filename)
{
    stream *s = NULL;
    int code;

    if (ctx->args.pdfdebug)
        dmprintf1(ctx->memory, "%% Attempting to open %s as a PDF file\n", filename);

    ctx->filename = (char *)gs_alloc_bytes(ctx->memory, strlen(filename) + 1, "copy of filename");
    if (ctx->filename == NULL)
        return_error(gs_error_VMerror);
    strcpy(ctx->filename, filename);

    s = sfopen(filename, "r", ctx->memory);
    if (s == NULL) {
        emprintf1(ctx->memory, "Failed to open file %s\n", filename);
        return_error(gs_error_ioerror);
    }
    code = pdfi_set_input_stream(ctx, s);
    return code;
}

/***********************************************************************************/
/* Highest level functions. The context we create here is returned to the 'PL'     */
/* implementation, in future we plan to return it to PostScript by wrapping a      */
/* gargabe collected object 'ref' around it and returning that to the PostScript   */
/* world. custom PostScript operators will then be able to render pages, annots,   */
/* AcroForms etc by passing the opaque object back to functions here, allowing     */
/* the interpreter access to its context.                                          */

/* We start with routines for creating and destroying the interpreter context */
pdf_context *pdfi_create_context(gs_memory_t *pmem)
{
    pdf_context *ctx = NULL;
    gs_gstate *pgs = NULL;
    int code = 0;

    ctx = (pdf_context *) gs_alloc_bytes(pmem->non_gc_memory,
            sizeof(pdf_context), "pdf_create_context");

    pgs = gs_gstate_alloc(pmem);

    if (!ctx || !pgs)
    {
        if (ctx)
            gs_free_object(pmem->non_gc_memory, ctx, "pdf_create_context");
        if (pgs)
            gs_gstate_free(pgs);
        return NULL;
    }

    memset(ctx, 0, sizeof(pdf_context));
    ctx->memory = pmem->non_gc_memory;

    ctx->stack_bot = (pdf_obj **)gs_alloc_bytes(ctx->memory, INITIAL_STACK_SIZE * sizeof (pdf_obj *), "pdf_imp_allocate_interp_stack");
    if (ctx->stack_bot == NULL) {
        gs_free_object(pmem->non_gc_memory, ctx, "pdf_create_context");
        gs_gstate_free(pgs);
        return NULL;
    }
    ctx->stack_size = INITIAL_STACK_SIZE;
    ctx->stack_top = ctx->stack_bot - sizeof(pdf_obj *);
    code = sizeof(pdf_obj *);
    code *= ctx->stack_size;
    ctx->stack_limit = ctx->stack_bot + ctx->stack_size;

    code = pdfi_init_font_directory(ctx);
    if (code < 0) {
        gs_free_object(pmem->non_gc_memory, ctx->stack_bot, "pdf_create_context");
        gs_free_object(pmem->non_gc_memory, ctx, "pdf_create_context");
        gs_gstate_free(pgs);
        return NULL;
    }

    code = gsicc_init_iccmanager(pgs);
    if (code < 0) {
        gs_free_object(ctx->memory, ctx->font_dir, "pdf_create_context");
        gs_free_object(pmem->non_gc_memory, ctx->stack_bot, "pdf_create_context");
        gs_free_object(pmem->non_gc_memory, ctx, "pdf_create_context");
        gs_gstate_free(pgs);
        return NULL;
    }

    ctx->pgs = pgs;
    pdfi_gstate_set_client(ctx, pgs);
    /* Declare PDL client support for high level patterns, for the benefit
     * of pdfwrite and other high-level devices
     */
    ctx->pgs->have_pattern_streams = true;
    ctx->preserve_tr_mode = 0;
    ctx->args.notransparency = false;

    ctx->main_stream = NULL;

    /* Setup some flags that don't default to 'false' */
    ctx->args.showannots = true;
    ctx->args.preserveannots = true;
    /* NOTE: For testing certain annotations on cluster, might want to set this to false */
    ctx->args.printed = true; /* TODO: Should be true if OutputFile is set, false otherwise */

    /* Initially, prefer the XrefStm in a hybrid file */
    ctx->prefer_xrefstm = true;

    /* We decrypt strings from encrypted files until we start a page */
    ctx->decrypt_strings = true;
    ctx->get_glyph_name = pdfi_glyph_name;
    ctx->get_glyph_index = pdfi_glyph_index;

    /* Weirdly the graphics library wants us to always have two gstates, the
     * initial state and at least one saved state. if we don't then when we
     * grestore back to the initial state, it immediately saves another one.
     */
    code = gs_gsave(ctx->pgs);
#if REFCNT_DEBUG
    ctx->UID = 1;
#endif
#if CACHE_STATISTICS
    ctx->hits = 0;
    ctx->misses = 0;
    ctx->compressed_hits = 0;
    ctx->compressed_misses = 0;
#endif
    return ctx;
}

/* Purge all */
static bool
pdfi_fontdir_purge_all(const gs_memory_t * mem, cached_char * cc, void *dummy)
{
    return true;
}

#if DEBUG_CACHE
#if DEBUG_CACHE_FREE
static void
pdfi_print_cache(pdf_context *ctx)
{
    pdf_obj_cache_entry *entry = ctx->cache_LRU, *next;

    dmprintf1(ctx->memory, "CACHE: #entries=%d\n", ctx->cache_entries);
    while(entry) {
        next = entry->next;
#if REFCNT_DEBUG
        dmprintf5(ctx->memory, "UID:%ld, Object:%d, refcnt:%d, next=%p, prev=%p\n",
                  entry->o->UID, entry->o->object_num, entry->o->refcnt,
                  entry->next, entry->previous);
#else
        dmprintf4(ctx->memory, "Object:%d, refcnt:%d, next=%p, prev=%p\n",
                  entry->o->object_num, entry->o->refcnt,
                  entry->next, entry->previous);
#endif
        entry = next;
    }
}
#else
static void
pdfi_print_cache(pdf_context *ctx)
{}
#endif
#endif /* DEBUG */

int pdfi_free_context(gs_memory_t *pmem, pdf_context *ctx)
{
#if CACHE_STATISTICS
    float compressed_hit_rate = 0.0, hit_rate = 0.0;

    if (ctx->compressed_hits > 0 || ctx->compressed_misses > 0)
        compressed_hit_rate = (float)ctx->compressed_hits / (float)(ctx->compressed_hits + ctx->compressed_misses);
    if (ctx->hits > 0 || ctx->misses > 0)
        hit_rate = (float)ctx->hits / (float)(ctx->hits + ctx->misses);

    dmprintf1(ctx->memory, "Number of normal object cache hits: %"PRIi64"\n", ctx->hits);
    dmprintf1(ctx->memory, "Number of normal object cache misses: %"PRIi64"\n", ctx->misses);
    dmprintf1(ctx->memory, "Number of compressed object cache hits: %"PRIi64"\n", ctx->compressed_hits);
    dmprintf1(ctx->memory, "Number of compressed object cache misses: %"PRIi64"\n", ctx->compressed_misses);
    dmprintf1(ctx->memory, "Normal object cache hit rate: %f\n", hit_rate);
    dmprintf1(ctx->memory, "Compressed object cache hit rate: %f\n", compressed_hit_rate);
#endif
    if (ctx->args.PageList)
        gs_free_object(ctx->memory, ctx->args.PageList, "pdfi_free_context");

    if (ctx->Trailer)
        pdfi_countdown(ctx->Trailer);

    if(ctx->Root)
        pdfi_countdown(ctx->Root);

    if (ctx->Info)
        pdfi_countdown(ctx->Info);

    if (ctx->PagesTree)
        pdfi_countdown(ctx->PagesTree);

    pdfi_doc_page_array_free(ctx);

    if (ctx->xref_table) {
        pdfi_countdown(ctx->xref_table);
        ctx->xref_table = NULL;
    }

    pdfi_free_OptionalRoot(ctx);

    if (ctx->stack_bot) {
        pdfi_clearstack(ctx);
        gs_free_object(ctx->memory, ctx->stack_bot, "pdfi_free_context");
    }

    if (ctx->filename) {
        /* This should already be closed, but lets make sure */
        pdfi_close_pdf_file(ctx);
        gs_free_object(ctx->memory, ctx->filename, "pdfi_free_context, free copy of filename");
        ctx->filename = NULL;
    }

    if (ctx->main_stream) {
        gs_free_object(ctx->memory, ctx->main_stream, "pdfi_free_context, free main PDF stream");
        ctx->main_stream = NULL;
    }
    ctx->main_stream_length = 0;

    if(ctx->pgs != NULL) {
        gx_pattern_cache_free(ctx->pgs->pattern_cache);
        if (ctx->pgs->font)
            pdfi_countdown_current_font(ctx);

        /* We use gs_grestore_only() instead of gs_grestore, because gs_grestore
         * will not restore below two gstates and we want to clear the entire
         * stack of saved states, back to the initial state.
         */
        while (ctx->pgs->saved)
            gs_grestore_only(ctx->pgs);

        /* And here we free the initial graphics state */
        gs_gstate_free(ctx->pgs);
        ctx->pgs = NULL;
    }

    pdfi_free_name_table(ctx);

    pdfi_free_DefaultQState(ctx);
    pdfi_oc_free(ctx);

    if(ctx->EKey)
        pdfi_countdown(ctx->EKey);
    if (ctx->Password)
        gs_free_object(ctx->memory, ctx->Password, "PDF Password from params");

    if (ctx->cache_entries != 0) {
        pdf_obj_cache_entry *entry = ctx->cache_LRU, *next;

#if DEBUG_CACHE
        int count;
        bool stop = true;
        pdf_obj_cache_entry *prev;

        do {
            pdfi_print_cache(ctx);
            entry = ctx->cache_LRU;
            stop = true;
            while(entry) {
                pdfi_print_cache(ctx);
                next = entry->next;
                prev = entry->previous;

                /* pass through the cache, count down any objects which are only referenced by the cache (refcnt == 1)
                 * this may cause other objects (referred to by the newly freed object) to decrement their refcnt
                 * until they are also only pointed at by the cache.
                 */
                if (entry->o->refcnt == 1) {
                    stop = false;
                    pdfi_countdown(entry->o);
                    if (prev != NULL)
                        prev->next = next;
                    else
                        ctx->cache_LRU = next;
                    if (next)
                        next->previous = prev;
                    ctx->cache_entries--;
                    gs_free_object(ctx->memory, entry, "pdfi_free_context, free LRU");
                }
                entry = next;
            }
        } while (stop == false);

        entry = ctx->cache_LRU;
        while(entry) {
            next = entry->next;
            prev = entry->previous;
            count = entry->o->refcnt;
            dbgmprintf1(ctx->memory, "CLEANUP cache entry obj %d", entry->o->object_num);
            dbgmprintf1(ctx->memory, " has refcnt %d\n", count);
            entry = next;
        }
#else
        while(entry) {
            next = entry->next;
            pdfi_countdown(entry->o);
            ctx->cache_entries--;
            gs_free_object(ctx->memory, entry, "pdfi_free_context, free LRU");
            entry = next;
#if REFCNT_DEBUG
            ctx->cache_LRU = entry;
#endif
        }
#endif
        ctx->cache_LRU = ctx->cache_MRU = NULL;
        ctx->cache_entries = 0;
    }

    /* We can't free the font directory before the graphics library fonts fonts are freed, as they reference the font_dir.
     * graphics library fonts are refrenced from pdf_font objects, and those may be in the cache, which means they
     * won't be freed until we empty the cache. So we can't free 'font_dir' until after the cache has been cleared.
     */
    if (ctx->font_dir) {
        gx_purge_selected_cached_chars(ctx->font_dir, pdfi_fontdir_purge_all, (void *)NULL);
        gs_free_object(ctx->memory, ctx->font_dir, "pdfi_free_context");
    }
    pdfi_countdown(ctx->pdffontmap);
    gs_free_object(ctx->memory, ctx, "pdfi_free_context");
    return 0;
}
