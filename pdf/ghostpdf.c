/* Copyright (C) 2018-2024 Artifex Software, Inc.
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

/* Top level PDF access routines */
#include "ghostpdf.h"
#include "pdf_types.h"
#include "pdf_dict.h"
#include "pdf_array.h"
#include "pdf_int.h"
#include "pdf_misc.h"
#include "pdf_stack.h"
#include "pdf_file.h"
#include "pdf_loop_detect.h"
#include "pdf_trans.h"
#include "pdf_font_types.h"
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
#include "pdf_mark.h"

#include "gsstate.h"        /* For gs_gstate */
#include "gsicc_manage.h"  /* For gsicc_init_iccmanager() */

#if PDFI_LEAK_CHECK
#include "gsmchunk.h"
#endif

#ifndef USE_PDF_PERMISSIONS
#define USE_PDF_PERMISSIONS 0
#endif

extern const char gp_file_name_list_separator;
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

    if (ctx->filename != NULL)
        dmprintf2(ctx->memory, "\n        %s has %"PRIi64" ", ctx->filename, ctx->num_pages);
    else
        dmprintf1(ctx->memory, "\n        File has %"PRIi64" ", ctx->num_pages);

    if (ctx->num_pages > 1)
        dmprintf(ctx->memory, "pages\n\n");
    else
        dmprintf(ctx->memory, "page.\n\n");

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
    dmprintf(ctx->memory, "\n");
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
                    if (pdfi_type_of(a->values[i]) == PDF_INT)
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

static int dump_font(pdf_context *ctx, pdf_dict *font_dict, bool space_name)
{
    pdf_obj *obj = NULL;
    char *str = NULL;
    int len = 0, code = 0, i;
    bool known = false, type0 = false;

    code = pdfi_dict_get_type(ctx, font_dict, "BaseFont", PDF_NAME, &obj);
    if (code >= 0) {
        code = pdfi_string_from_name(ctx, (pdf_name *)obj, &str, &len);
        if (code >= 0) {
            dmprintf1(ctx->memory, "%s", str);
            if (len < 32 && space_name) {
                for (i = 0; i < 32 - len;i++)
                    dmprintf(ctx->memory, " ");
            } else
                dmprintf(ctx->memory, "    ");
            (void)pdfi_free_string_from_name(ctx, str);
        }
        pdfi_countdown(obj);
        obj = NULL;
    }

    code = pdfi_dict_get_type(ctx, font_dict, "Subtype", PDF_NAME, &obj);
    if (code >= 0) {
        code = pdfi_string_from_name(ctx, (pdf_name *)obj, &str, &len);
        if (code >= 0) {
            dmprintf1(ctx->memory, "%s", str);
            for (i = 0; i < 16 - len;i++)
                dmprintf(ctx->memory, " ");
            (void)pdfi_free_string_from_name(ctx, str);
        }
        if (pdfi_name_is((pdf_name *)obj, "Type0"))
            type0 = true;
        pdfi_countdown(obj);
        obj = NULL;
    }

    if (!type0) {
        code = pdfi_dict_get_type(ctx, font_dict, "Embedded", PDF_BOOL, &obj);
        if (code >= 0) {
            if (obj == PDF_FALSE_OBJ)
                dmprintf(ctx->memory, "Not embedded    ");
            else
                dmprintf(ctx->memory, "Embedded        ");
            pdfi_countdown(obj);
            obj = NULL;
        }
        else
            dmprintf(ctx->memory, "Not embedded    ");
    } else
        dmprintf(ctx->memory, "                ");

    code = pdfi_dict_get_type(ctx, font_dict, "ToUnicode", PDF_BOOL, &obj);
    if (code >= 0) {
        if (obj == PDF_TRUE_OBJ)
            dmprintf(ctx->memory, "Has ToUnicode    ");
        else
            dmprintf(ctx->memory, "No ToUnicode     ");
        pdfi_countdown(obj);
        obj = NULL;
    }
    else
        dmprintf(ctx->memory, "No ToUnicode    ");

    code = pdfi_dict_known(ctx, font_dict, "Descendants", &known);
    if (code >= 0 && known) {
        code = pdfi_dict_get_type(ctx, font_dict, "Descendants", PDF_ARRAY, &obj);
        if (code >= 0) {
            pdf_obj *desc = NULL;

            code = pdfi_array_get_type(ctx, (pdf_array *)obj, 0, PDF_DICT, &desc);
            if (code >= 0) {
                dmprintf(ctx->memory, "\n            Descendants: [");
                (void)dump_font(ctx, (pdf_dict *)desc, false);
                dmprintf(ctx->memory, "]");
            }
            pdfi_countdown(obj);
            obj = NULL;
        }
    }
    return 0;
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
int pdfi_output_page_info(pdf_context *ctx, uint64_t page_num)
{
    int code;
    bool known = false;
    double f;
    pdf_dict *page_dict = NULL;
    pdf_array *fonts_array = NULL, *spots_array = NULL;

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

    code = pdfi_check_page(ctx, page_dict, &fonts_array, &spots_array, false);
    if (code < 0) {
        if (ctx->args.pdfstoponerror)
            return code;
    } else {
        if (ctx->page.has_transparency)
            dmprintf(ctx->memory, "     Page uses transparency features");
    }

    code = pdfi_dict_known(ctx, page_dict, "Annots", &known);
    if (code < 0) {
        if (code != gs_error_undefined && ctx->args.pdfstoponerror)
            goto error;
    } else {
        if (known == true)
            dmprintf(ctx->memory, "     Page contains Annotations");
        code = 0;
    }

    if (spots_array != NULL) {
        uint64_t index = 0;
        pdf_name *spot = NULL;
        char *str = NULL;
        int len;

        dmprintf(ctx->memory, "\n    Page Spot colors: \n");
        for (index = 0;index < pdfi_array_size(spots_array);index++) {
            code = pdfi_array_get(ctx, spots_array, index, (pdf_obj **)&spot);
            if (code >= 0) {
                if (pdfi_type_of(spot) == PDF_NAME) {
                    code = pdfi_string_from_name(ctx, spot, &str, &len);
                    if (code >= 0) {
                        dmprintf1(ctx->memory, "        '%s'\n", str);
                        (void)pdfi_free_string_from_name(ctx, str);
                    }
                }
                pdfi_countdown(spot);
                spot = NULL;
            }
        }
        code = 0;
    }

    if (fonts_array != NULL && pdfi_array_size(fonts_array) != 0) {
        uint64_t index = 0;
        pdf_dict *font_dict = NULL;

        dmprintf(ctx->memory, "\n    Fonts used: \n");
        for (index = 0;index < pdfi_array_size(fonts_array);index++) {
            code = pdfi_array_get_type(ctx, fonts_array, index, PDF_DICT, (pdf_obj **)&font_dict);
            if (code >= 0) {
                dmprintf(ctx->memory, "        ");
                (void)dump_font(ctx, font_dict, true);
                dmprintf(ctx->memory, "\n");
                pdfi_countdown(font_dict);
                font_dict = NULL;
            }
        }
        code = 0;
    }

error:
    pdfi_countdown(fonts_array);
    pdfi_countdown(spots_array);
    dmprintf(ctx->memory, "\n\n");
    pdfi_countdown(page_dict);

    return code;
}

/* Error and warning string tables. There should be a string for each error and warning
 * defined in the error (pdf_error_e) and warning (pdf_warning_e) enumerators. Having
 * more strings is harmless, having too few may cause crashes. These need to be kept in sync.
 *
 * The Ghostscript graphics library errors should be kept up to date, but this is less critical
 * if the error code is greater than the last error we know about we just print a generic
 * 'unknown' message.
 */
const char *pdf_error_strings[] = {
#define PARAM(A,B) B
#include "pdf_errors.h"
    ""                               /* last error, should not be used */
};

const char *pdf_warning_strings[] = {
#define PARAM(A,B) B
#include "pdf_warnings.h"
    ""                               /* Last warning should not be used */
};

const char *gs_error_strings[] = {
    "no error",
    "unknownerror",
    "dictfull",
    "dictstackoverflow",
    "dictstackunderflow",
    "execstackoverflow",
    "interrupt",
    "invalidaccess",
    "invalidexit",
    "invalidfileaccess",
    "invalidfont",
    "invalidrestore",
    "ioerror",
    "limitcheck",
    "nocurrentpoint",
    "rangecheck",
    "stackoverflow",
    "stackunderflow",
    "syntaxerror",
    "timeout",
    "typecheck",
    "undefined",
    "undefinedfilename",
    "undefinedresult",
    "unmatchedmark",
    "VMerror",
    "configurationerror",
    "undefinedresource",
    "unregistered",
    "invalidcontext",
    "invalidid",
    "pdf_stackoverflow",
    "circular reference"
};

const char *gs_internal_error_strings[] = {
    "error hit",
    "fatal error",
    "quit",
    "interpreter exit",
    "remap color",
    "exec stack underflow",
    "VMreclaim",
    "Need input",
    "need file",
    "No defined error",
    "No defined error (2)",
    "error info",
    "handled",
};
#define LASTNORMALGSERROR gs_error_circular_reference * -1
#define FIRSTINTERNALERROR gs_error_hit_detected * -1
#define LASTGSERROR gs_error_handled * -1

void pdfi_verbose_error(pdf_context *ctx, int gs_error, const char *gs_lib_function, int pdfi_error, const char *pdfi_function_name, const char *extra_info)
{
    char fallback[] = "unknown graphics library error";

    if (ctx->args.verbose_errors && !ctx->args.QUIET) {
        if (gs_error != 0) {
            char *error_string;
            unsigned int code = gs_error * -1;

            if (code > LASTGSERROR)
                error_string = fallback;
            else {
                if (code > LASTNORMALGSERROR) {
                    if (code < FIRSTINTERNALERROR)
                        error_string = fallback;
                    else
                        error_string = (char *)gs_internal_error_strings[code - FIRSTINTERNALERROR];
                } else
                    error_string = (char *)gs_error_strings[code];
            }
            errprintf(ctx->memory, "Graphics library error %d (%s) in function '%s'", gs_error, error_string, pdfi_function_name);
            if (gs_lib_function != NULL)
                errprintf(ctx->memory, " from lib routine '%s'.\n", gs_lib_function);
            else
                errprintf(ctx->memory, ".\n");

            if (pdfi_error != 0)
                errprintf(ctx->memory, "\tSetting pdfi error %d - %s.\n", pdfi_error, pdf_error_strings[pdfi_error]);
            if (extra_info != NULL)
                errprintf(ctx->memory, "\t%s\n", extra_info);
        } else {
            if (pdfi_error != 0) {
                errprintf(ctx->memory, "Function '%s' set pdfi error %d - %s.\n", pdfi_function_name, pdfi_error, pdf_error_strings[pdfi_error]);
                if (extra_info != NULL)
                    errprintf(ctx->memory, "\t%s\n", extra_info);
            } else {
                if (extra_info != NULL)
                    errprintf(ctx->memory, "%s\n", extra_info);
            }
        }
    }
}

void pdfi_verbose_warning(pdf_context *ctx, int gs_error, const char *gs_lib_function, int pdfi_warning, const char *pdfi_function_name, const char *extra_info)
{
    char fallback[] = "unknown graphics library error";

    if (ctx->args.verbose_warnings && !ctx->args.QUIET) {
        if (gs_error != 0) {
            char *error_string;
            unsigned int code = gs_error * -1;

            if (code > LASTGSERROR)
                error_string = fallback;
            else {
                if (code > LASTNORMALGSERROR) {
                    if (code < FIRSTINTERNALERROR)
                        error_string = fallback;
                    else
                        error_string = (char *)gs_internal_error_strings[code - FIRSTINTERNALERROR];
                } else
                    error_string = (char *)gs_error_strings[code];
            }
            outprintf(ctx->memory, "Graphics library error %d (%s) in function '%s'", gs_error, error_string, pdfi_function_name);
            if (gs_lib_function != NULL)
                outprintf(ctx->memory, " from lib routine '%s'.\n", gs_lib_function);
            else
                outprintf(ctx->memory, ".\n");

            if (pdfi_warning != 0)
                outprintf(ctx->memory, "\tsetting pdfi warning %d - %s.\n", pdfi_warning, pdf_warning_strings[pdfi_warning]);
            if (extra_info != NULL)
                outprintf(ctx->memory, "\t%s\n", extra_info);
        } else {
            if (pdfi_warning != 0) {
                outprintf(ctx->memory, "Function '%s' set pdfi warning %d - %s.\n", pdfi_function_name, pdfi_warning, pdf_warning_strings[pdfi_warning]);
                if (extra_info != NULL)
                    errprintf(ctx->memory, "\t%s\n", extra_info);
            } else {
                if (extra_info != NULL)
                    errprintf(ctx->memory, "\t%s\n", extra_info);
            }
        }
    }
}

void pdfi_set_error_var(pdf_context *ctx, int gs_error, const char *gs_lib_function, pdf_error pdfi_error, const char *pdfi_function_name, const char *fmt, ...)
{
    if (pdfi_error != 0)
        ctx->pdf_errors[pdfi_error / (sizeof(char) * 8)] |= 1 << pdfi_error % (sizeof(char) * 8);
    if (ctx->args.verbose_errors) {
        char extra_info[gp_file_name_sizeof];
        va_list args;

        va_start(args, fmt);
        (void)vsnprintf(extra_info, sizeof(extra_info), fmt, args);
        va_end(args);

        pdfi_verbose_error(ctx, gs_error, gs_lib_function, pdfi_error, pdfi_function_name, extra_info);
    }
}

void pdfi_set_warning_var(pdf_context *ctx, int gs_error, const char *gs_lib_function, pdf_warning pdfi_warning, const char *pdfi_function_name, const char *fmt, ...)
{
    ctx->pdf_warnings[pdfi_warning / (sizeof(char) * 8)] |= 1 << pdfi_warning % (sizeof(char) * 8);
    if (ctx->args.verbose_warnings) {
        char extra_info[gp_file_name_sizeof];
        va_list args;

        va_start(args, fmt);
        (void)vsnprintf(extra_info, sizeof(extra_info), fmt, args);
        va_end(args);

        pdfi_verbose_warning(ctx, gs_error, gs_lib_function, pdfi_warning, pdfi_function_name, extra_info);
    }
}

void pdfi_log_info(pdf_context *ctx, const char *pdfi_function, const char *info)
{
#ifdef DEBUG
    if (!ctx->args.QUIET)
        outprintf(ctx->memory, "%s", info);
#endif
}

void
pdfi_report_errors(pdf_context *ctx)
{
    int code, i, j;
    bool warnings_exist = false, errors_exist = false;

    if (ctx->args.QUIET)
        return;

    for (i = 0; i < PDF_ERROR_BYTE_SIZE; i++) {
        if (ctx->pdf_errors[i] != 0)
            errors_exist = true;
    }

    for (i = 0; i < PDF_WARNING_BYTE_SIZE; i++) {
        if (ctx->pdf_warnings[i] != 0)
            warnings_exist = true;
    }

    if (!errors_exist && !warnings_exist)
        return;

    if (errors_exist)
    {
        dmprintf(ctx->memory, "\nThe following errors were encountered at least once while processing this file:\n");
        for (i = 0; i < PDF_ERROR_BYTE_SIZE; i++) {
            if (ctx->pdf_errors[i] != 0) {
                for (j=0;j < sizeof(char) * 8; j++) {
                    if (ctx->pdf_errors[i] & 1 << j) {
                        int error_num = (i * sizeof(char) * 8) + j;

                        errprintf(ctx->memory, "\t%s\n", pdf_error_strings[error_num]);
                    }
                }
            }
        }
    }

    if (warnings_exist)
    {
        dmprintf(ctx->memory, "\nThe following warnings were encountered at least once while processing this file:\n");
        for (i = 0; i < PDF_WARNING_BYTE_SIZE; i++) {
            if (ctx->pdf_warnings[i] != 0) {
                for (j=0;j < sizeof(char) * 8; j++) {
                    if (ctx->pdf_warnings[i] & 1 << j) {
                        int warning_num = (i * sizeof(char) * 8) + j;

                        outprintf(ctx->memory, "\t%s\n", pdf_warning_strings[warning_num]);
                    }
                }
            }
        }
    }

    dmprintf(ctx->memory, "\n   **** This file had errors that were repaired or ignored.\n");
    if (ctx->Info) {
        pdf_string *s = NULL;

        code = pdfi_dict_knownget_type(ctx, ctx->Info, "Producer", PDF_STRING, (pdf_obj **)&s);
        if (code > 0) {
            char *cs;

            cs = (char *)gs_alloc_bytes(ctx->memory, s->length + 1, "temporary string for error report");
            if (cs == NULL) {
                dmprintf(ctx->memory, "   **** Out of memory while trying to display Producer ****\n");
            } else {
                memcpy(cs, s->data, s->length);
                cs[s->length] = 0x00;
                dmprintf1(ctx->memory, "   **** The file was produced by: \n   **** >>>> %s <<<<\n", cs);
                gs_free_object(ctx->memory, cs, "temporary string for error report");
            }
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
    pdfi_name_entry_t *e = NULL, *last_entry = NULL, *new_entry = NULL;
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

    new_entry = (pdfi_name_entry_t *)gs_alloc_bytes(ctx->memory, sizeof(pdfi_name_entry_t), "Alloc name table entry");
    if (new_entry == NULL)
        return_error(gs_error_VMerror);
    memset(new_entry, 0x00, sizeof(pdfi_name_entry_t));
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
        pdfi_name_entry_t *next = NULL, *e = (pdfi_name_entry_t *)ctx->name_table;

        while (e != NULL) {
            next = (pdfi_name_entry_t *)e->next;
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
    pdfi_name_entry_t *e = (pdfi_name_entry_t *)ctx->name_table;

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
    pdfi_name_entry_t *e = NULL;

    if (igs == NULL)
        return_error(gs_error_undefined);

    ctx = igs->ctx;
    if (ctx == NULL)
        return_error(gs_error_undefined);

    e = (pdfi_name_entry_t *)ctx->name_table;

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

int pdfi_finish_pdf_file(pdf_context *ctx)
{
    if (ctx->Root) {
        if (ctx->device_state.writepdfmarks && ctx->args.preservemarkedcontent) {
            pdf_obj *o = NULL;
            int code = 0;

            code = pdfi_dict_knownget_type(ctx, ctx->Root, "OCProperties", PDF_DICT, &o);
            if (code > 0) {
                // Build and send the OCProperties structure
                code = pdfi_pdfmark_from_objarray(ctx, &o, 1, NULL, "OCProperties");
                pdfi_countdown(o);
                if (code < 0)
                    /* Error message ? */
                ;
            }
        }
    }
    return 0;
}

int pdfi_close_pdf_file(pdf_context *ctx)
{
    if (ctx->Root) {
        pdf_obj *o = NULL;
        int code = 0;

        code = pdfi_dict_knownget(ctx, ctx->Root, "OCProperties", &o);
        if (code > 0) {
            // Build and send the OCProperties structure
            code = pdfi_pdfmark_from_objarray(ctx, &o, 1, NULL, "OCProperties");
            pdfi_countdown(o);
            if (code < 0)
                /* Error message ? */
                ;
        }
    }

    if (ctx->main_stream) {
        if (ctx->main_stream->s) {
            sfclose(ctx->main_stream->s);
        }
        gs_free_object(ctx->memory, ctx->main_stream, "Closing main PDF file");
        ctx->main_stream = NULL;
    }
    ctx->main_stream_length = 0;

    if (ctx->filename) {
        gs_free_object(ctx->memory, ctx->filename, "pdfi_close_pdf_file, free copy of filename");
        ctx->filename = NULL;
    }

    pdfi_clear_context(ctx);
    return 0;
}

static int pdfi_process(pdf_context *ctx)
{
    int code = 0, i;

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

    return code;
}

/* This works by reading each embedded file referenced by the collection. If it
 * has a MIME type indicating it's a PDF file, or somewhere in the first 2KB it
 * has the PDF header (%PDF-) then we treat it as a PDF file. We read the contents
 * of the refrenced stream and write them to disk in a scratch file.
 *
 * We then process each scratch file in turn. Note that we actually return an
 * array of strings; the first string is the temporary filename, the second is
 * the entry from the names tree. Since this can be in UTF16-BE format, it can
 * contain embedded single byte NULL characters, so we can't use a regular C
 * string. Instead we use a triple byte NULL termination.
 *
 * It ought to be possible to do all the processing without creating scratch files, by saving the
 * current file state, and opening a new 'file' on the stream in the original PDF
 * file. But I couldn't immediately get that to work.
 * So this is a FIXME future enhancement.
 */
int pdfi_prep_collection(pdf_context *ctx, uint64_t *TotalFiles, char ***names_array)
{
    int code = 0, i, NumEmbeddedFiles = 0;
    pdf_obj *Names = NULL, *EmbeddedFiles = NULL;
    pdf_array *FileNames = NULL;
    pdf_obj *EF = NULL, *F = NULL;
    char **working_array = NULL;

    if (pdfi_dict_knownget_type(ctx, ctx->Root, "Names", PDF_DICT, &Names)) {
        if(pdfi_dict_knownget_type(ctx, (pdf_dict *)Names, "EmbeddedFiles", PDF_DICT, &EmbeddedFiles)) {
            if (pdfi_dict_knownget_type(ctx, (pdf_dict *)EmbeddedFiles, "Names", PDF_ARRAY, (pdf_obj **)&FileNames)) {
                int ix = 0, index = 0;
                gp_file *scratch_file = NULL;
                char scratch_name[gp_file_name_sizeof];

                NumEmbeddedFiles = pdfi_array_size(FileNames) / 2;

                working_array = (char **)gs_alloc_bytes(ctx->memory, NumEmbeddedFiles * 2 * sizeof(char *), "Collection file working names array");
                if (working_array == NULL) {
                    code = gs_note_error(gs_error_VMerror);
                    goto exit;
                }
                memset(working_array, 0x00, NumEmbeddedFiles * 2 * sizeof(char *));

                for (ix = 0;ix < NumEmbeddedFiles;ix++) {
                    pdf_obj *File = NULL;
                    pdf_obj *Subtype = NULL;

                    code = pdfi_array_get(ctx, FileNames, (ix * 2) + 1, &File);
                    if (code < 0)
                        goto exit;

                    if (pdfi_type_of(File) == PDF_DICT) {
                        if (pdfi_dict_knownget_type(ctx, (pdf_dict *)File, "EF", PDF_DICT, &EF)) {
                            if (pdfi_dict_knownget_type(ctx, (pdf_dict *)EF, "F", PDF_STREAM, &F)) {
                                pdf_dict *stream_dict = NULL;
                                pdf_c_stream *s = NULL;

                                /* pdfi_dict_from_object does not increment the reference count of the stream dictionary
                                 * so we do not need to count it down later.
                                 */
                                code = pdfi_dict_from_obj(ctx, F, &stream_dict);
                                if (code >= 0) {
                                    if (!pdfi_dict_knownget_type(ctx, stream_dict, "Subtype", PDF_NAME, &Subtype)) {
                                        /* No Subtype, (or not a name) we can't check the Mime type, so try to read the first 2Kb
                                         * and look for a %PDF- in that. If not present, assume its not a PDF
                                         */
                                        code = pdfi_seek(ctx, ctx->main_stream, pdfi_stream_offset(ctx, (pdf_stream *)F), SEEK_SET);
                                        if (code >= 0) {
                                            code = pdfi_filter(ctx, (pdf_stream *)F, ctx->main_stream, &s, false);
                                            if (code >= 0) {
                                                char Buffer[2048];
                                                int bytes;

                                                bytes = pdfi_read_bytes(ctx, (byte *)Buffer, 1, 2048, s);
                                                pdfi_close_file(ctx, s);
                                                s = NULL;
                                                /* Assertion; the smallest real PDF file is at least 400 bytes */
                                                if (bytes >= 400) {
                                                    if (strstr(Buffer, "%PDF-") == NULL)
                                                        code = -1;
                                                } else
                                                    code = -1;
                                            }
                                        }
                                    } else {
                                        if (!pdfi_name_is((const pdf_name *)Subtype, "application/pdf"))
                                            code = -1;
                                    }

                                    if (code >= 0) {
                                        /* Appears to be a PDF file. Create a scratch file to hold it, and then
                                         * read the file from the PDF, and write it to the scratch file. Record
                                         * the scratch filename in the working_array for later processing.
                                         */
                                        scratch_file = gp_open_scratch_file(ctx->memory, "gpdf-collection-", scratch_name, "wb");
                                        if (scratch_file != NULL) {
                                            code = pdfi_seek(ctx, ctx->main_stream, pdfi_stream_offset(ctx, (pdf_stream *)F), SEEK_SET);
                                            if (code >= 0) {
                                                double L;
                                                pdf_c_stream *SubFile_stream = NULL;

                                                /* Start by setting up the file to be read. Apply a SubFileDecode so that, if the input stream
                                                 * is not compressed we will stop reading when we get to the end of the stream.
                                                 */
                                                if (pdfi_dict_knownget_number(ctx, stream_dict, "Length", &L) > 0) {

                                                    code = pdfi_apply_SubFileDecode_filter(ctx, (int)L, NULL, ctx->main_stream, &SubFile_stream, false);
                                                    if (code >= 0)
                                                        code = pdfi_filter(ctx, (pdf_stream *)F, SubFile_stream, &s, false);
                                                } else
                                                    code = pdfi_filter(ctx, (pdf_stream *)F, ctx->main_stream, &s, false);

                                                if (code >= 0) {
                                                    char Buffer[2048];
                                                    int bytes;
                                                    pdf_string *Name = NULL;

                                                    /* Read the stream contents and write them to the scratch file */
                                                    do {
                                                        bytes = pdfi_read_bytes(ctx, (byte *)Buffer, 1, 2048, s);
                                                        (void)gp_fwrite(Buffer, 1, bytes, scratch_file);
                                                    } while (bytes > 0);

                                                    /* Create an entry for the Description in the names array */
                                                    code = pdfi_array_get(ctx, FileNames, ix * 2, (pdf_obj **)&Name);
                                                    if (code >= 0) {
                                                        if (pdfi_type_of((pdf_obj *)Name) == PDF_STRING) {
                                                            working_array[(index * 2) + 1] = (char *)gs_alloc_bytes(ctx->memory, Name->length + 3, "Collection file names array entry");
                                                            if (working_array[(index * 2) + 1] != NULL) {
                                                                memset(working_array[(index * 2) + 1], 0x00, Name->length + 3);
                                                                memcpy(working_array[(index * 2) + 1], Name->data, Name->length);
                                                            }
                                                        }
                                                        pdfi_countdown(Name);
                                                        Name = NULL;
                                                    }

                                                    /* And now the scratch file name */
                                                    working_array[index * 2] = (char *)gs_alloc_bytes(ctx->memory, strlen(scratch_name) + 3, "Collection file names array entry");
                                                    if (working_array[index * 2] != NULL) {
                                                        memset(working_array[index * 2], 0x00, strlen(scratch_name) + 3);
                                                        strcpy(working_array[index * 2], scratch_name);
                                                    }

                                                    index++;
                                                    (*TotalFiles)++;
                                                    pdfi_close_file(ctx, s);
                                                    s = NULL;
                                                }
                                                if (SubFile_stream != NULL)
                                                    pdfi_close_file(ctx, SubFile_stream);
                                                SubFile_stream = NULL;
                                            }
                                            gp_fclose(scratch_file);
                                        } else
                                            dmprintf(ctx->memory, "\n   **** Warning: Failed to open a scratch file.\n");
                                    }
                                }
                            }
                        }
                    }
                    pdfi_countdown(Subtype);
                    Subtype = NULL;
                    pdfi_countdown(F);
                    F = NULL;
                    pdfi_countdown(EF);
                    EF = NULL;
                    pdfi_countdown(File);
                    File = NULL;
                }
            } else {
                dmprintf(ctx->memory, "\n   **** Warning: Failed to read EmbeededFiles Names tree.\n");
            }
        } else {
            dmprintf(ctx->memory, "\n   **** Warning: Failed to read EmbeddedFiles.\n");
        }
    } else {
        dmprintf(ctx->memory, "\n   **** Warning: Failed to find Names tree.\n");
    }
    code = 0;

exit:
    if (code >= 0) {
        uint64_t ix = 0;

        (*names_array) = (char **)gs_alloc_bytes(ctx->memory, *TotalFiles * 2 * sizeof(char *), "Collection file namesarray");
        for (i = 0; i < NumEmbeddedFiles;i++) {
            if (working_array[i * 2] != NULL && working_array[(i * 2) + 1] != NULL) {
                (*names_array)[ix * 2] = working_array[i * 2];
                working_array[i * 2] = NULL;
                (*names_array)[(ix * 2) + 1] = working_array[(i * 2) + 1];
                working_array[(i * 2) + 1] = NULL;
                ix++;
            }
        }
    }

    if (working_array != NULL) {
        for (i = 0; i < NumEmbeddedFiles;i++)
            gs_free_object(ctx->memory, working_array[i], "free collection temporary filenames");
        gs_free_object(ctx->memory, working_array, "free collection working array");
    }
    pdfi_countdown(F);
    pdfi_countdown(EF);
    pdfi_countdown(FileNames);
    pdfi_countdown(EmbeddedFiles);
    pdfi_countdown(Names);
    return code;
}

static int pdfi_process_collection(pdf_context *ctx)
{
    int code, i;
    uint64_t TotalFiles = 0, ix = 0;
    char **names_array = NULL;

    code = pdfi_prep_collection(ctx, &TotalFiles, &names_array);
    if (code >= 0 && TotalFiles > 0)
    {
        /* names_array is full of pointers to the scratch file names containing PDF files.
         * Now we need to run each PDF file. First we close down the current file.
         */
        (void)pdfi_close_pdf_file(ctx);

        for (ix = 0;ix < TotalFiles * 2;ix+=2) {
            if (names_array[ix] != NULL) {
                (void)pdfi_process_pdf_file(ctx, names_array[ix]);
                (void)pdfi_close_pdf_file(ctx);
            }
        }
    } else
        /* We didn't find any PDF files in the Embedded Files. So just run the
         * pages in the container file (the original PDF file)
         */
        pdfi_process(ctx);

    for (i = 0; i < TotalFiles * 2;i++)
        gs_free_object(ctx->memory, names_array[i], "free collection temporary filenames");
    gs_free_object(ctx->memory, names_array, "free collection names array");

    return 0;
}

int pdfi_process_pdf_file(pdf_context *ctx, char *filename)
{
    int code = 0;

    code = pdfi_open_pdf_file(ctx, filename);
    if (code < 0) {
        pdfi_report_errors(ctx);
        return code;
    }

    /* Do any custom device configuration */
    pdfi_device_misc_config(ctx);

    if (ctx->Collection != NULL)
        code = pdfi_process_collection(ctx);
    else
        code = pdfi_process(ctx);

    /* Pdfmark_InitialPage is the offset for Page Dests in
     * /Outlines and Link annotations. It is the count of pages
     * processed so far. Update it by the number of pages in
     * this file.
     */
    ctx->Pdfmark_InitialPage += ctx->num_pages;

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
            pdfi_set_error(ctx, 0, NULL, E_PDF_BADXREFSTREAM, "pdfi_init_file", NULL);
            pdfi_countdown(ctx->xref_table);
            ctx->xref_table = NULL;
            ctx->prefer_xrefstm = false;
            code = pdfi_read_xref(ctx);
            if (code < 0)
                goto exit;
        } else {
            pdfi_set_error(ctx, code, NULL, E_PDF_BADXREFSTREAM, "pdfi_init_file", NULL);
            goto exit;
        }
    }

    pdfi_device_set_flags(ctx);

    if (ctx->Trailer) {
        /* See comment in pdfi_read_Root() (pdf_doc.c) for details */
        pdf_dict *d = ctx->Trailer;

        pdfi_countup(d);
        code = pdfi_dict_get(ctx, d, "Encrypt", &o);
        pdfi_countdown(d);
        if (code < 0 && code != gs_error_undefined)
            goto exit;
        if (code == 0) {
            if (pdfi_type_of(o) == PDF_DICT) {
                code = pdfi_initialise_Decryption(ctx);
                if (code < 0)
                    goto exit;

                /* This section is commetned out but deliberately retained. This code
                 * prevents us from processing any PDF file where the 'print' permission
                 * is not set. Additionally, if the 'print' permission is set, but bit 12
                 * 'faitful digital copy' is not set, *and* we are writing to a high level
                 * device, then we will throw an error..
                 */
#if USE_PDF_PERMISSIONS
                code = pdfi_dict_knownget_number(ctx, (pdf_dict *)o, "P", &Permissions);
                if (code < 0)
                    goto exit;
                if (Permissions > ((unsigned long)1 << 31)) {
                    code = gs_note_error(gs_error_rangecheck);
                    goto exit;
                }
                P = (uint32_t)Permissions;
                if ((P & 0x04) == 0) {
                    dmprintf(ctx->memory, "   ****The owner of this file has requested you do not print it.\n");
                    code = gs_note_error(gs_error_invalidfileaccess);
                    goto exit;
                }
                if (ctx->device_state.HighLevelDevice) {
                    if ((P & 0x800) == 0) {
                        dmprintf(ctx->memory, "   ****The owner of this file has requested you do not make a digital copy of it.\n");
                        code = gs_note_error(gs_error_invalidfileaccess);
                        goto exit;
                    }
                }
#endif
            } else {
                if (pdfi_type_of(o) != PDF_NULL)
                    pdfi_set_error(ctx, code, NULL, E_PDF_BADENCRYPT, "pdfi_init_file", NULL);
            }
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
                pdfi_set_error(ctx, 0, NULL, E_PDF_BADXREFSTREAM, "pdfi_init_file", NULL);
                pdfi_countdown(ctx->xref_table);
                ctx->xref_table = NULL;
                ctx->prefer_xrefstm = false;
                code = pdfi_read_xref(ctx);
                if (code < 0) {
                    pdfi_set_error(ctx, 0, NULL, E_PDF_BADXREF, "pdfi_init_file", NULL);
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

    code = pdfi_doc_trailer(ctx);
    if (code < 0)
        goto exit;

    pdfi_read_OptionalRoot(ctx);

    if (ctx->args.pdfinfo) {
        code = pdfi_output_metadata(ctx);
        if (code < 0 && ctx->args.pdfstoponerror)
            goto exit;
    }

exit:
    if (code < 0)
        pdfi_set_error(ctx, code, NULL, 0, "pdfi_init_file", NULL);
    pdfi_countdown(o);
    return code;
}

int pdfi_set_input_stream(pdf_context *ctx, stream *stm)
{
    byte *Buffer = NULL;
    char *s = NULL, *test = NULL;
    float version = 0.0;
    gs_offset_t Offset = 0;
    int64_t bytes = 0, leftover = 0, bytes_left = 0;
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
    pdfi_seek(ctx, ctx->main_stream, 0, SEEK_SET);
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
    test = (char *)Buffer;
    bytes_left = bytes;
    s = strstr((char *)test, "%PDF-");
    if (s == NULL)
        pdfi_set_warning(ctx, 0, NULL, W_PDF_GARBAGE_B4HDR, "pdfi_set_input_stream", "");
    /* Garbage before the header can be anything, including binary and NULL (0x00) characters
     * which can defeat using strstr, so if we fail to find a header, try moving on by the length
     * of the C string + 1 and try again.
     */
    while (s == NULL) {
        bytes_left -= (strlen(test) + 1);
        if (bytes_left < 5)
            break;
        test += strlen(test) + 1;
        s = strstr((char *)test, "%PDF-");
    };
    if (s == NULL) {
        char extra_info[gp_file_name_sizeof];

        if (ctx->filename)
            gs_snprintf(extra_info, sizeof(extra_info), "%% File %s does not appear to be a PDF file (no %%PDF in first 2Kb of file)\n", ctx->filename);
        else
            gs_snprintf(extra_info, sizeof(extra_info), "%% File does not appear to be a PDF stream (no %%PDF in first 2Kb of stream)\n");

        pdfi_set_error(ctx, 0, NULL, E_PDF_NOHEADER, "pdfi_set_input_stream", extra_info);
    } else {
        /* Now extract header version (may be overridden later) */
        if (sscanf(s + 5, "%f", &version) != 1) {
            ctx->HeaderVersion = 0;
            pdfi_set_error(ctx, 0, NULL, E_PDF_NOHEADERVERSION, "pdfi_set_input_stream", (char *)"%% Unable to read PDF version from header\n");
        }
        else {
            ctx->HeaderVersion = version;
            s += 5;
            while (((*s >= '0' && *s <= '9') || *s == '.') && *s != 0x00)
                s++;
            while (*s != 0x00) {
                if (*s == 0x09 || *s== 0x0c || *s == 0x20) {
                    s++;
                    continue;
                }
                if (*s != 0x0A && *s != 0x0D)
                    pdfi_set_warning(ctx, 0, NULL, W_PDF_VERSION_NO_EOL, "pdfi_set_input_stream", (char *)"%% PDF version not immediately followed with EOL\n");
                break;
            }
        }
        if (ctx->args.pdfdebug)
            dmprintf1(ctx->memory, "%% Found header, PDF version is %f\n", ctx->HeaderVersion);
    }

    /* Jump to EOF and scan backwards looking for startxref */
    pdfi_seek(ctx, ctx->main_stream, 0, SEEK_END);

    if (ctx->args.pdfdebug)
        dmprintf(ctx->memory, "%% Searching for 'startxref' keyword\n");

    /* Initially read min(BUF_SIZE, file_length) bytes of data to the buffer */
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

        /* When reading backwards, if we ran out of data in the last buffer while looking
         * for a 'startxref, but we had found a linefeed, then we preserved everything
         * from the beginning of the buffer up to that linefeed, by copying it to the end
         * of the buffer and reducing the number of bytes to read so that it should have filled
         * in the gap. If we didn't read enough bytes, then we have a gap between the end of
         * the data we just read and the leftover data from teh last buffer. Move the preserved
         * data down to meet the end of the data we just read.
         */
        if (bytes != read && leftover != 0)
            memcpy(Buffer + read, Buffer + bytes, leftover);

        /* As above, if we had any leftover data from the last buffer then increase the
         * number of bytes available by that amount. We increase 'bytes' (the number of bytes
         * to read) to the same value, which should mean we read an entire buffer's worth. Of
         * course if we have any data left out of this buffer we'll reduce bytes again...
         */
        read = bytes = read + leftover;

        /* Now search backwards in the buffer for the startxref token */
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

            /* Success! stop now */
            if(sscanf((char *)b, " %"PRIdOFFSET"", &ctx->startxref) != 1) {
                dmprintf(ctx->memory, "Unable to read offset of xref from PDF file\n");
            }
            break;
        } else {
            /* Our file read could conceivably have read back to the point where we read
             * part of the 'startxref' token, but not all of it. So we want to preserve
             * the data in the buffer, but not all of it obviously! The 'startxref' should be followed
             * by a line ending, so above we keep a note of the last line ending. If we found one, then
             * we preserve from the start of the buffer to that point. This could slow us up if the file
             * Is broken, or has a load of junk after the EOF, because we could potentially be saving a
             * lot of data on each pass, but that's only going to happen with bad files.
             * Note we reduce the number of bytes to read so that it just fits into the buffer up to the
             * beginning of the data we preserved.
             */
            if (last_lineend) {
                leftover = last_lineend - Buffer;
                /* Ensure we don't try to copy more than half a buffer, because that will
                 * end up overrunning the buffer end. Since we are only doing this to
                 * ensure we don't drop a partial 'startxref' that's far more than enough.
                 */
                if (leftover < BUF_SIZE / 2) {
                    memmove(Buffer + bytes - leftover, last_lineend, leftover);
                    bytes -= leftover;
                } else
                    leftover = 0;
            } else
                leftover = 0;
        }

        Offset += bytes;
    } while(Offset < ctx->main_stream_length);

    if (!found) {
        pdfi_set_error(ctx, 0, NULL, E_PDF_NOSTARTXREF, "pdfi_set_input_stream", NULL);
        if (ctx->args.pdfstoponerror) {
            code = gs_note_error(gs_error_undefined);
            goto error;
        }
    }

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

static size_t pdfi_grdir_path_string_match(const byte *str, size_t sl0, byte *pat, size_t pl)
{
    bool found = false;
    size_t sl = sl0;

    while (found == false) {
        if (pl > sl)
            break;
        if (*str == *pat && memcmp(str, pat, pl) == 0)
            found = true;
        else {
            str++;
            sl--;
        }
    }
    if (found)
        return (sl0 - sl) + pl;
    else
        return 0;
}

int pdfi_add_paths_to_search_paths(pdf_context *ctx, const char *ppath, int l, bool fontpath)
{
    int i, slen, npaths = (l > 0) ? 1 : 0;
    const char *p = ppath;
    char *ps;
    const char *pe = p + l + 1;
    int code = 0;
    static const char *resstr = "Resource";
    const int restrlen = strlen(resstr);
    const char *dirsepstr = gp_file_name_directory_separator();
    const int dirsepstrlen = strlen(dirsepstr);
    char genresstr[64];

    for (ps = (char *)p; ps < pe; ps++) {
        if (*ps == gp_file_name_list_separator)
           npaths++;
    }

    if (npaths > 0) {
        gs_param_string *pathstrings;
        int new_npaths = ctx->search_paths.num_resource_paths + npaths;

        if (fontpath != true) {
            pathstrings = (gs_param_string *)gs_alloc_bytes(ctx->memory, sizeof(gs_param_string) * new_npaths, "array of paths");
            if (pathstrings == NULL)
                return_error(gs_error_VMerror);

            memset(pathstrings, 0x00, sizeof(gs_param_string) * new_npaths);

            for (i = 1; i <= ctx->search_paths.num_init_resource_paths; i++) {
                pathstrings[new_npaths - i] = ctx->search_paths.resource_paths[ctx->search_paths.num_resource_paths - i];
            }

            for (i = 0; i < ctx->search_paths.num_resource_paths - ctx->search_paths.num_init_resource_paths; i++) {
                pathstrings[i] = ctx->search_paths.resource_paths[i];
            }
            /* NO NOT CHANGE "i" BETWEEN HERE....... */
            gs_free_object(ctx->memory, ctx->search_paths.resource_paths, "old array of paths");
            ctx->search_paths.resource_paths = pathstrings;
            ctx->search_paths.num_resource_paths += npaths;

            /* .....AND HERE */
            for (ps = (char *)p; ps < pe; ps++) {
                if (*ps == gp_file_name_list_separator || ps == pe - 1) {
                    if (*p == gp_file_name_list_separator) p++; /* move past the separator */
                    slen = ps - p;
                    pathstrings[i].data = (byte *)gs_alloc_bytes(ctx->memory, slen, "path string body");

                    if (pathstrings[i].data == NULL) {
                        code = gs_note_error(gs_error_VMerror);
                        break;
                    }

                    memcpy((char *)pathstrings[i].data, p, slen);
                    pathstrings[i].size = slen;
                    pathstrings[i].persistent = false;
                    i++;
                    p = ps++;
                }
            }
            if ((restrlen + 2 * dirsepstrlen) < 64) {
                size_t grdlen;

                memcpy(genresstr, resstr, restrlen  +1); /* +1 So we get the null terminator */
                strncat(genresstr, dirsepstr, dirsepstrlen);

                for (i = 0; i < ctx->search_paths.num_resource_paths; i++) {
                    if ((grdlen = pdfi_grdir_path_string_match(ctx->search_paths.resource_paths[i].data, ctx->search_paths.resource_paths[i].size, (byte *)genresstr, restrlen + dirsepstrlen)) > 0) {
                        ctx->search_paths.genericresourcedir.data = ctx->search_paths.resource_paths[i].data;
                        ctx->search_paths.genericresourcedir.size = grdlen;
                        ctx->search_paths.genericresourcedir.persistent = true;
                        break;
                    }
                }
            }
        }
        else {
            p = ppath;
            pathstrings = (gs_param_string *)gs_alloc_bytes(ctx->memory, sizeof(gs_param_string) * (npaths + ctx->search_paths.num_font_paths), "array of font paths");
            if (pathstrings == NULL)
                return_error(gs_error_VMerror);

            memset(pathstrings, 0x00, sizeof(gs_param_string) * (npaths + ctx->search_paths.num_font_paths));

            for (i = 0; i < ctx->search_paths.num_font_paths; i++) {
                pathstrings[ctx->search_paths.num_font_paths + i] = ctx->search_paths.font_paths[i];
            }
            gs_free_object(ctx->memory, ctx->search_paths.font_paths, "old array of paths");
            ctx->search_paths.font_paths = pathstrings;
            ctx->search_paths.num_font_paths += npaths;

            i = 0;
            for (ps = (char *)p; ps < pe; ps++) {
                if (*ps == gp_file_name_list_separator || ps == pe - 1) {
                    slen = ps - p;
                    pathstrings[i].data = (byte *)gs_alloc_bytes(ctx->memory, slen, "path string body");

                    if (pathstrings[i].data == NULL) {
                        code = gs_note_error(gs_error_VMerror);
                        break;
                    }

                    memcpy((char *)pathstrings[i].data, p, slen);
                    pathstrings[i].size = slen;
                    pathstrings[i].persistent = false;
                    i++;
                    p = ps++;
                }
            }
        }
    }

    return code;
}

int pdfi_add_initial_paths_to_search_paths(pdf_context *ctx, const char *ppath, int l)
{
    int code;
    if (ctx->search_paths.num_resource_paths != 0)
        return_error(gs_error_invalidaccess);

    code = pdfi_add_paths_to_search_paths(ctx, ppath, l, false);
    ctx->search_paths.num_init_resource_paths = ctx->search_paths.num_resource_paths;

    return code;
}

static void pdfi_free_search_paths(pdf_context *ctx)
{
    int i;
    for (i = 0; i < ctx->search_paths.num_resource_paths; i++) {
        if (ctx->search_paths.resource_paths[i].persistent == false)
            gs_free_object(ctx->memory, (byte *)ctx->search_paths.resource_paths[i].data, "path string body");
    }
    for (i = 0; i < ctx->search_paths.num_font_paths; i++) {
        if (ctx->search_paths.font_paths[i].persistent == false)
            gs_free_object(ctx->memory, (byte *)ctx->search_paths.font_paths[i].data, "path string body");
    }
    gs_free_object(ctx->memory, (byte *)ctx->search_paths.resource_paths, "array of paths");
    gs_free_object(ctx->memory, (byte *)ctx->search_paths.font_paths, "array of font paths");

    if (ctx->search_paths.genericresourcedir.persistent == false)
        gs_free_object(ctx->memory, (byte *)ctx->search_paths.genericresourcedir.data, "generic resource directory");
}

static void pdfi_free_fontmapfiles(pdf_context *ctx)
{
    int i;
    for (i = 0; i < ctx->num_fontmapfiles; i++) {
        gs_free_object(ctx->memory, ctx->fontmapfiles[i].data, "fontmapfiles string body");
    }
    gs_free_object(ctx->memory, ctx->fontmapfiles, "fontmapfiles array");
}

/* The fontmap file list doesn't extend, later settings in the command line override earlier ones
   (Unlike the "-I" search paths above).
 */
int pdfi_add_fontmapfiles(pdf_context *ctx, const char *ppath, int l)
{
    int i, nfilenames = (l > 0) ? 1 : 0;
    const char *p = ppath;
    char *ps;
    const char *pe = p + l + 1;
    int code = 0;

    pdfi_free_fontmapfiles(ctx);

    for (ps = (char *)p; ps < pe; ps++) {
        if (*ps == gp_file_name_list_separator)
           nfilenames++;
    }
    if (nfilenames > 0) {
        ctx->fontmapfiles = (gs_string *)gs_alloc_bytes(ctx->memory, sizeof(gs_string) * nfilenames, "array of fontmap files");
        if (ctx->fontmapfiles == NULL) {
            return_error(gs_error_VMerror);
        }
        else {
           memset(ctx->fontmapfiles, 0x00, sizeof(gs_string) * nfilenames);
           ctx->num_fontmapfiles = nfilenames;

           for (i = 0; i < nfilenames; i++) {
               for (ps = (char *)p; ps < pe; ps++) {
                   if (*ps == gp_file_name_list_separator)
                       break;
               }
               ctx->fontmapfiles[i].data = gs_alloc_bytes(ctx->memory, ps - p, "fontmap file name body");
               if (ctx->fontmapfiles[i].data == NULL) {
                    code = gs_note_error(gs_error_VMerror);
                    goto done;
               }
               memcpy(ctx->fontmapfiles[i].data, p, ps - p);
               ctx->fontmapfiles[i].size = ps - p;
               p = ps + 1;
           }
        }
    }
done:
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
pdf_context *pdfi_create_context(gs_memory_t *mem)
{
    pdf_context *ctx = NULL;
    gs_gstate *pgs = NULL;
    int code = 0;
    gs_memory_t *pmem = mem->non_gc_memory;
#if PDFI_LEAK_CHECK
    gs_memory_status_t mstat;
    code = gs_memory_chunk_wrap(&pmem, mem->non_gc_memory);
    if (code < 0)
        return NULL;
    gs_memory_status(pmem, &mstat);
#endif

    ctx = (pdf_context *) gs_alloc_bytes(pmem, sizeof(pdf_context), "pdf_create_context");

    pgs = gs_gstate_alloc(pmem);

    if (!ctx || !pgs)
    {
        if (ctx)
            gs_free_object(pmem, ctx, "pdf_create_context");
        if (pgs)
            gs_gstate_free(pgs);
        return NULL;
    }

    memset(ctx, 0, sizeof(pdf_context));
    ctx->memory = pmem;
    ctx->type = PDF_CTX;
    ctx->flags = 0;
    ctx->refcnt = 1;
    ctx->ctx = ctx;

#if PDFI_LEAK_CHECK
    ctx->memstat = mstat;
#endif

    ctx->stack_bot = (pdf_obj **)gs_alloc_bytes(ctx->memory, INITIAL_STACK_SIZE * sizeof (pdf_obj *), "pdf_imp_allocate_interp_stack");
    if (ctx->stack_bot == NULL) {
        gs_free_object(pmem, ctx, "pdf_create_context");
        gs_gstate_free(pgs);
        return NULL;
    }
    ctx->stack_size = INITIAL_STACK_SIZE;
    ctx->stack_top = ctx->stack_bot - 1;
    code = sizeof(pdf_obj *);
    code *= ctx->stack_size;
    ctx->stack_limit = ctx->stack_bot + ctx->stack_size;

    code = pdfi_init_font_directory(ctx);
    if (code < 0) {
        gs_free_object(pmem, ctx->stack_bot, "pdf_create_context");
        gs_free_object(pmem, ctx, "pdf_create_context");
        gs_gstate_free(pgs);
        return NULL;
    }

    code = gsicc_init_iccmanager(pgs);
    if (code < 0) {
        gs_free_object(ctx->memory, ctx->font_dir, "pdf_create_context");
        gs_free_object(pmem, ctx->stack_bot, "pdf_create_context");
        gs_free_object(pmem, ctx, "pdf_create_context");
        gs_gstate_free(pgs);
        return NULL;
    }

    ctx->pgs = pgs;
    code = pdfi_gstate_set_client(ctx, pgs);
    if (code < 0) {
        gs_free_object(ctx->memory, ctx->font_dir, "pdf_create_context");
        gs_free_object(pmem, ctx->stack_bot, "pdf_create_context");
        gs_free_object(pmem, ctx, "pdf_create_context");
        gs_gstate_free(pgs);
        return NULL;
    }

    /* Some (but not all) path construction operations can either return
     * an error or clamp values when out of range. In order to match Ghostscript's
     * PDF interpreter written in PostScript, we need to clamp them.
     */
    gs_setlimitclamp(pgs, true);

    /* Declare PDL client support for high level patterns, for the benefit
     * of pdfwrite and other high-level devices
     */
    ctx->pgs->have_pattern_streams = true;
    ctx->device_state.preserve_tr_mode = 0;
    ctx->args.notransparency = false;

    ctx->main_stream = NULL;

    /* Setup some flags that don't default to 'false' */
    ctx->args.showannots = true;
    ctx->args.preserveannots = true;
    ctx->args.preservemarkedcontent = true;
    ctx->args.preserveembeddedfiles = true;
    ctx->args.preservedocview = true;
    /* NOTE: For testing certain annotations on cluster, might want to set this to false */
    ctx->args.printed = false; /* True if OutputFile is set, false otherwise see pdftop.c, pdf_impl_set_param() */

    /* Initially, prefer the XrefStm in a hybrid file */
    ctx->prefer_xrefstm = true;

    /* We decrypt strings from encrypted files until we start a page */
    ctx->encryption.decrypt_strings = true;
    ctx->get_glyph_name = pdfi_glyph_name;
    ctx->get_glyph_index = pdfi_glyph_index;

    ctx->job_gstate_level = ctx->pgs->level;
    /* Weirdly the graphics library wants us to always have two gstates, the
     * initial state and at least one saved state. if we don't then when we
     * grestore back to the initial state, it immediately saves another one.
     */
    code = gs_gsave(ctx->pgs);
    if (code < 0) {
        gs_free_object(ctx->memory, ctx->font_dir, "pdf_create_context");
        gs_free_object(pmem, ctx->stack_bot, "pdf_create_context");
        gs_gstate_free(ctx->pgs);
        gs_free_object(pmem, ctx, "pdf_create_context");
        return NULL;
    }
#if REFCNT_DEBUG
    ctx->UID = 1;
#endif
#if CACHE_STATISTICS
    ctx->hits = 0;
    ctx->misses = 0;
    ctx->compressed_hits = 0;
    ctx->compressed_misses = 0;
#endif
#ifdef DEBUG
    ctx->args.verbose_errors = ctx->args.verbose_warnings = 1;
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

#if PURGE_CACHE_PER_PAGE
void
pdfi_purge_obj_cache(pdf_context *ctx)
{
    if (ctx->cache_entries != 0) {
        pdf_obj_cache_entry *entry = ctx->cache_LRU, *next;

        while(entry) {
            next = entry->next;
            if (entry->o->object_num != 0) {
                ctx->xref_table->xref[entry->o->object_num].cache = NULL;
            }
            pdfi_countdown(entry->o);
            ctx->cache_entries--;
            gs_free_object(ctx->memory, entry, "pdfi_clear_context, free LRU");
            entry = next;
#if REFCNT_DEBUG
            ctx->cache_LRU = entry;
#endif
        }
        ctx->cache_LRU = ctx->cache_MRU = NULL;
        ctx->cache_entries = 0;
    }
}
#endif

/* pdfi_clear_context frees all the PDF objects associated with interpreting a given
 * PDF file. Once we've called this we can happily run another file. This function is
 * called by pdf_free_context (in case of errors during the file leaving state around)
 * and by pdfi_close_pdf_file.
 */
int pdfi_clear_context(pdf_context *ctx)
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
    if (ctx->PathSegments != NULL) {
        gs_free_object(ctx->memory, ctx->PathSegments, "pdfi_clear_context");
        ctx->PathSegments = NULL;
    }
    if (ctx->PathPts != NULL) {
        gs_free_object(ctx->memory, ctx->PathPts, "pdfi_clear_context");
        ctx->PathPts = NULL;
    }

    if (ctx->args.PageList) {
        gs_free_object(ctx->memory, ctx->args.PageList, "pdfi_clear_context");
        ctx->args.PageList = NULL;
    }
    if (ctx->Trailer) {
        pdfi_countdown(ctx->Trailer);
        ctx->Trailer = NULL;
    }

    if (ctx->AcroForm) {
        pdfi_countdown(ctx->AcroForm);
        ctx->AcroForm = NULL;
    }

    if(ctx->Root) {
        pdfi_countdown(ctx->Root);
        ctx->Root = NULL;
    }

    if (ctx->Info) {
        pdfi_countdown(ctx->Info);
        ctx->Info = NULL;
    }

    if (ctx->PagesTree) {
        pdfi_countdown(ctx->PagesTree);
        ctx->PagesTree = NULL;
    }

    if (ctx->args.cidfsubstpath.data != NULL) {
        gs_free_object(ctx->memory, ctx->args.cidfsubstpath.data, "cidfsubstpath.data");
        ctx->args.cidfsubstpath.data = NULL;
    }

    if (ctx->args.cidfsubstfont.data != NULL) {
        gs_free_object(ctx->memory, ctx->args.cidfsubstfont.data, "cidfsubstfont.data");
        ctx->args.cidfsubstfont.data = NULL;
    }

    if (ctx->args.defaultfont.data != NULL) {
        gs_free_object(ctx->memory, ctx->args.defaultfont.data, "cidfsubstfont.data");
        ctx->args.defaultfont.data = NULL;
    }

    pdfi_free_cstring_array(ctx, &ctx->args.showannottypes);
    pdfi_free_cstring_array(ctx, &ctx->args.preserveannottypes);

    pdfi_doc_page_array_free(ctx);

    if (ctx->xref_table) {
        pdfi_countdown(ctx->xref_table);
        ctx->xref_table = NULL;
    }

    pdfi_free_OptionalRoot(ctx);

    if (ctx->stack_bot)
        pdfi_clearstack(ctx);

    if (ctx->filename) {
        /* This should already be closed! */
        pdfi_close_pdf_file(ctx);
        gs_free_object(ctx->memory, ctx->filename, "pdfi_clear_context, free copy of filename");
        ctx->filename = NULL;
    }

    if (ctx->main_stream) {
        gs_free_object(ctx->memory, ctx->main_stream, "pdfi_clear_context, free main PDF stream");
        ctx->main_stream = NULL;
    }
    ctx->main_stream_length = 0;

    if(ctx->pgs != NULL) {
        gx_pattern_cache_free(ctx->pgs->pattern_cache);
        ctx->pgs->pattern_cache = NULL;
        if (ctx->pgs->font)
            pdfi_countdown_current_font(ctx);

        /* We use gs_grestore_only() instead of gs_grestore, because gs_grestore
         * will not restore below two gstates and we want to clear the entire
         * stack of saved states, back to the initial state.
         */
        while (ctx->pgs->level != ctx->job_gstate_level && ctx->pgs->saved)
            gs_grestore_only(ctx->pgs);
    }

    pdfi_free_DefaultQState(ctx);
    pdfi_oc_free(ctx);

    if(ctx->encryption.EKey) {
        pdfi_countdown(ctx->encryption.EKey);
        ctx->encryption.EKey = NULL;
    }
    if (ctx->encryption.Password) {
        gs_free_object(ctx->memory, ctx->encryption.Password, "PDF Password from params");
        ctx->encryption.Password = NULL;
    }

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
                    gs_free_object(ctx->memory, entry, "pdfi_clear_context, free LRU");
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
            gs_free_object(ctx->memory, entry, "pdfi_clear_context, free LRU");
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
    if (ctx->font_dir)
        gx_purge_selected_cached_chars(ctx->font_dir, pdfi_fontdir_purge_all, (void *)NULL);

    pdfi_countdown(ctx->pdffontmap);
    ctx->pdffontmap = NULL;
    pdfi_countdown(ctx->pdfnativefontmap);
    ctx->pdfnativefontmap = NULL;
    pdfi_countdown(ctx->pdf_substitute_fonts);
    ctx->pdf_substitute_fonts = NULL;

    return 0;
}

int pdfi_free_context(pdf_context *ctx)
{
#if PDFI_LEAK_CHECK
    gs_memory_status_t mstat, ctxmstat = ctx->memstat;
    gs_memory_t *mem = ctx->memory;
#endif
    pdfi_clear_context(ctx);

    gs_free_object(ctx->memory, ctx->stack_bot, "pdfi_free_context");

    pdfi_free_name_table(ctx);

    /* And here we free the initial graphics state */
    while (ctx->pgs->saved)
        gs_grestore_only(ctx->pgs);

    gs_gstate_free(ctx->pgs);

    ctx->pgs = NULL;

    if (ctx->font_dir)
        gs_free_object(ctx->memory, ctx->font_dir, "pdfi_free_context");

    /* Currently this should never happen, but in future it might if we choose
     * not to keep freeing and reallocating the array.
     */
    if (ctx->loop_detection != NULL) {
        dbgmprintf(ctx->memory, "Loop detection array exists at EOJ\n");
        gs_free_object(ctx->memory, ctx->loop_detection, "pdfi_free_context");
    }

    pdfi_free_search_paths(ctx);
    pdfi_free_fontmapfiles(ctx);

    if (ctx->pdfcidfmap != NULL) {
        pdfi_countdown(ctx->pdfcidfmap);
        ctx->pdfcidfmap = NULL;
    }
    if (ctx->pdffontmap != NULL) {
        pdfi_countdown(ctx->pdffontmap);
        ctx->pdffontmap = NULL;
    }
    rc_decrement(ctx->devbbox, "pdfi_free_context");

    gs_free_object(ctx->memory, ctx, "pdfi_free_context");
#if PDFI_LEAK_CHECK
    gs_memory_status(mem, &mstat);
    if (mstat.allocated > ctxmstat.allocated)
        errprintf(mem, "\nMemory Leak Detected: Pre %d, Post %d\n", (int)ctxmstat.allocated, (int)mstat.allocated);
    (void)gs_memory_chunk_unwrap(mem);
#endif
    return 0;
}

/* These routines are used from the PostScript interpreter inteerface. It is important that
 * the 'interpreter part of the graphics state' should be a pdfi interpreter context while pdfi is running
 * but the PostScript itnerpreter context when the PostScript interpreter is running. If we are going
 * to inherit the PostScript graphics state for pdfi, then we need to turn it into a 'pdfi'
 * graphics state for the duration of the interpretation, and back to a PostScript one when
 * we return to the PostScript interpreter.
 *
 * Bizarrely it appears that the interpreter part of the gstate does not obey grestore, instead we copy
 * the 'current' context to the saved context when we do a grestore. This seems wrong to me, but
 * it seems to be what happens, so we can't rely on grestore to put back the interpreter context, but must
 * do so ourselves.
 *
 * Hence the 'from_PS' routine fills in pointers with the current context and procs, with the expectation that
 * these will be saved and used to restore the data in the 'to_PS' routine.
 */
/* NOTE see the comments in zpdfops.c just under the declaration of the pdfi_switch_t strcuture regarding
 * complications with the ICC profile cache.
 */

int pdfi_gstate_from_PS(pdf_context *ctx, gs_gstate *pgs, pdfi_switch_t *i_switch, gsicc_profile_cache_t *profile_cache)
{
    int code;
    i_switch->pgs = ctx->pgs;
    i_switch->procs = pgs->client_procs;
    i_switch->client_data = (void *)pgs->client_data;
    i_switch->profile_cache = pgs->icc_profile_cache;
    code = pdfi_gstate_set_client(ctx, pgs);
    if (code < 0)
        return code;
    i_switch->psfont = pgs->font;
    pgs->icc_profile_cache = profile_cache;
    rc_increment(pgs->icc_profile_cache);
    pgs->font = NULL;
    ctx->pgs = pgs;
    return code;
}

void pdfi_gstate_to_PS(pdf_context *ctx, gs_gstate *pgs, pdfi_switch_t *i_switch)
{
    pgs->client_procs.free(pgs->client_data, pgs->memory, pgs);
    pgs->client_data = NULL;
    rc_decrement(pgs->icc_profile_cache, "pdfi_gstate_to_PS");
    pgs->icc_profile_cache = i_switch->profile_cache;
    gs_gstate_set_client(pgs, i_switch->client_data, &i_switch->procs, true);
    ctx->pgs->font = NULL;
    ctx->pgs = i_switch->pgs;
    pgs->font = i_switch->psfont;
}
