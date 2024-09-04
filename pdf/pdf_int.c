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

/* The PDF interpreter written in C */

#include "pdf_int.h"
#include "pdf_file.h"
#include "strmio.h"
#include "stream.h"
#include "pdf_misc.h"
#include "pdf_path.h"
#include "pdf_colour.h"
#include "pdf_image.h"
#include "pdf_shading.h"
#include "pdf_font.h"
#include "pdf_font_types.h"
#include "pdf_cmap.h"
#include "pdf_text.h"
#include "pdf_gstate.h"
#include "pdf_stack.h"
#include "pdf_xref.h"
#include "pdf_dict.h"
#include "pdf_array.h"
#include "pdf_trans.h"
#include "pdf_optcontent.h"
#include "pdf_sec.h"
#include <stdlib.h>

#include "gsstate.h"    /* for gs_gstate_free */

/* we use -ve returns for error, 0 for success and +ve for 'take an action' */
/* Defining tis return so we do not need to define a new error */
#define REPAIRED_KEYWORD 1

/***********************************************************************************/
/* 'token' reading functions. Tokens in this sense are PDF logical objects and the */
/* related keywords. So that's numbers, booleans, names, strings, dictionaries,    */
/* arrays, the  null object and indirect references. The keywords are obj/endobj   */
/* stream/endstream, xref, startxref and trailer.                                  */

/***********************************************************************************/
/* Some simple functions to find white space, delimiters and hex bytes             */
static bool iswhite(char c)
{
    if (c == 0x00 || c == 0x09 || c == 0x0a || c == 0x0c || c == 0x0d || c == 0x20)
        return true;
    else
        return false;
}

static bool isdelimiter(char c)
{
    if (c == '/' || c == '(' || c == ')' || c == '[' || c == ']' || c == '<' || c == '>' || c == '{' || c == '}' || c == '%')
        return true;
    else
        return false;
}

/* The 'read' functions all return the newly created object on the context's stack
 * which means these objects are created with a reference count of 0, and only when
 * pushed onto the stack does the reference count become 1, indicating the stack is
 * the only reference.
 */
int pdfi_skip_white(pdf_context *ctx, pdf_c_stream *s)
{
    int c;

    do {
        c = pdfi_read_byte(ctx, s);
        if (c < 0)
            return 0;
    } while (iswhite(c));

    pdfi_unread_byte(ctx, s, (byte)c);
    return 0;
}

int pdfi_skip_eol(pdf_context *ctx, pdf_c_stream *s)
{
    int c;

    do {
        c = pdfi_read_byte(ctx, s);
        if (c < 0 || c == 0x0a)
            return 0;
    } while (c != 0x0d);
    c = pdfi_read_byte(ctx, s);
    if (c == 0x0a)
        return 0;
    if (c >= 0)
        pdfi_unread_byte(ctx, s, (byte)c);
    pdfi_set_warning(ctx, 0, NULL, W_PDF_STREAM_BAD_KEYWORD, "pdfi_skip_eol", NULL);
    return 0;
}

/* Fast(ish) but inaccurate strtof, with Adobe overflow handling,
 * lifted from MuPDF. */
static float acrobat_compatible_atof(char *s)
{
    int neg = 0;
    int i = 0;

    while (*s == '-') {
        neg = 1;
        ++s;
    }
    while (*s == '+') {
        ++s;
    }

    while (*s >= '0' && *s <= '9') {
        /* We deliberately ignore overflow here.
         * Tests show that Acrobat handles * overflows in exactly the same way we do:
         * 123450000000000000000678 is read as 678.
         */
        i = i * 10 + (*s - '0');
        ++s;
    }

    if (*s == '.') {
        float MAX = (MAX_FLOAT-9)/10;
        float v = (float)i;
        float n = 0;
        float d = 1;
        ++s;
        /* Bug 705211: Ensure that we don't overflow n here - just ignore any
         * trailing digits after this. This will be plenty accurate enough. */
        while (*s >= '0' && *s <= '9' && n <= MAX) {
            n = 10 * n + (*s - '0');
            d = 10 * d;
            ++s;
        }
        v += n / d;
        return neg ? -v : v;
    } else {
        return (float)(neg ? -i : i);
    }
}

int pdfi_read_bare_int(pdf_context *ctx, pdf_c_stream *s, int *parsed_int)
{
    int index = 0;
    int int_val = 0;
    int negative = 0;

restart:
    pdfi_skip_white(ctx, s);

    do {
        int c = pdfi_read_byte(ctx, s);
        if (c == EOFC)
            break;

        if (c < 0)
            return_error(gs_error_ioerror);

        if (iswhite(c)) {
            break;
        } else if (c == '%' && index == 0) {
            pdfi_skip_comment(ctx, s);
            goto restart;
        } else if (isdelimiter(c)) {
            pdfi_unread_byte(ctx, s, (byte)c);
            break;
        }

        if (c >= '0' && c <= '9') {
            int_val = int_val*10 + c - '0';
        } else if (c == '.') {
            goto error;
        } else if (c == 'e' || c == 'E') {
            pdfi_log_info(ctx, "pdfi_read_bare_int", (char *)"Invalid number format: scientific notation\n");
            goto error;
        } else if (c == '-') {
            /* Any - sign not at the start of the string indicates a malformed number. */
            if (index != 0 || negative) {
                pdfi_log_info(ctx, "pdfi_read_bare_int", (char *)"Invalid number format: sign not at the start\n");
                goto error;
            }
            negative = 1;
        } else if (c == '+') {
            if (index == 0) {
                /* Just drop the + it's pointless, and it'll get in the way
                 * of our negation handling for floats. */
                continue;
            } else {
                pdfi_log_info(ctx, "pdfi_read_bare_int", (char *)"Invalid number format: sign not at the start\n");
                goto error;
            }
        } else {
            if (index > 0) {
                pdfi_log_info(ctx, "pdfi_read_bare_int", (char *)"Invalid number format: Ignoring missing white space while parsing number\n");
                goto error;
            }
            pdfi_unread_byte(ctx, s, (byte)c);
            goto error;
        }
        if (++index > 255)
            goto error;
    } while(1);

    *parsed_int = negative ? -int_val : int_val;
    if (ctx->args.pdfdebug)
        dmprintf1(ctx->memory, " %d", *parsed_int);
    return (index > 0);

error:
    *parsed_int = 0;
    return_error(gs_error_syntaxerror);
}

static int pdfi_read_num(pdf_context *ctx, pdf_c_stream *s, uint32_t indirect_num, uint32_t indirect_gen)
{
    byte Buffer[256];
    unsigned short index = 0;
    bool real = false;
    bool has_decimal_point = false;
    bool has_exponent = false;
    unsigned short exponent_index = 0;
    pdf_num *num;
    int code = 0, malformed = false, doubleneg = false, recovered = false, negative = false, overflowed = false;
    unsigned int int_val = 0;
    int tenth_max_int = max_int / 10, tenth_max_uint = max_uint / 10;

    pdfi_skip_white(ctx, s);

    do {
        int c = pdfi_read_byte(ctx, s);
        if (c == EOFC) {
            Buffer[index] = 0x00;
            break;
        }

        if (c < 0)
            return_error(gs_error_ioerror);

        if (iswhite(c)) {
            Buffer[index] = 0x00;
            break;
        } else if (isdelimiter(c)) {
            pdfi_unread_byte(ctx, s, (byte)c);
            Buffer[index] = 0x00;
            break;
        }
        Buffer[index] = (byte)c;

        if (c >= '0' && c <= '9') {
            if  (!(malformed && recovered) && !overflowed && !real) {
                if ((negative && int_val <= tenth_max_int) || (!negative && int_val <= tenth_max_uint))
                    int_val = int_val*10 + c - '0';
                else {
                    if ((code = pdfi_set_error_stop(ctx, gs_note_error(gs_error_syntaxerror), NULL, E_PDF_NUMBEROVERFLOW, "pdfi_read_num", NULL)) < 0) {
                        return code;
                    }
                    overflowed = true;
                }
            }
        } else if (c == '.') {
            if (has_decimal_point == true) {
                if ((code = pdfi_set_error_stop(ctx, gs_note_error(gs_error_syntaxerror), NULL, E_PDF_MALFORMEDNUMBER, "pdfi_read_num", NULL)) < 0) {
                    return code;
                }
                malformed = true;
            } else {
                has_decimal_point = true;
                real = true;
            }
        } else if (c == 'e' || c == 'E') {
            /* TODO: technically scientific notation isn't in PDF spec,
             * but gs seems to accept it, so we should also?
             */
            if (has_exponent == true) {
                if ((code = pdfi_set_error_stop(ctx, gs_note_error(gs_error_syntaxerror), NULL, E_PDF_MALFORMEDNUMBER, "pdfi_read_num", NULL)) < 0) {
                    return code;
                }
                malformed = true;
            } else {
                pdfi_set_warning(ctx, 0, NULL, W_PDF_NUM_EXPONENT, "pdfi_read_num", NULL);
                has_exponent = true;
                exponent_index = index;
                real = true;
            }
        } else if (c == '-') {
            /* Any - sign not at the start of the string, or just after an exponent
             * indicates a malformed number. */
            if (!(index == 0 || (has_exponent && index == exponent_index+1))) {
                if ((code = pdfi_set_error_stop(ctx, gs_note_error(gs_error_syntaxerror), NULL, E_PDF_MALFORMEDNUMBER, "pdfi_read_num", NULL)) < 0) {
                    return code;
                }
                if (Buffer[index - 1] != '-') {
                    /* We are parsing a number line 123-56. We should continue parsing, but
                     * ignore anything from the second -. */
                    malformed = true;
                    Buffer[index] = 0;
                    recovered = true;
                }
            }
            if (!has_exponent && !(malformed && recovered)) {
                doubleneg = negative;
                negative = 1;
            }
        } else if (c == '+') {
            if (index == 0 || (has_exponent && index == exponent_index+1)) {
                /* Just drop the + it's pointless, and it'll get in the way
                 * of our negation handling for floats. */
                index--;
            } else {
                if ((code = pdfi_set_error_stop(ctx, gs_note_error(gs_error_syntaxerror), NULL, E_PDF_MALFORMEDNUMBER, "pdfi_read_num", NULL)) < 0) {
                    return code;
                }
                if (Buffer[index - 1] != '-') {
                    /* We are parsing a number line 123-56. We should continue parsing, but
                     * ignore anything from the second -. */
                    malformed = true;
                    Buffer[index] = 0;
                    recovered = true;
                }
            }
        } else {
            if ((code = pdfi_set_error_stop(ctx, gs_note_error(gs_error_syntaxerror), NULL, E_PDF_MISSINGWHITESPACE, "pdfi_read_num", NULL)) < 0) {
                return code;
            }
            pdfi_unread_byte(ctx, s, (byte)c);
            Buffer[index] = 0x00;
            break;
        }
        if (++index > 255)
            return_error(gs_error_syntaxerror);
    } while(1);

    if (real && (!malformed || (malformed && recovered)))
        code = pdfi_object_alloc(ctx, PDF_REAL, 0, (pdf_obj **)&num);
    else
        code = pdfi_object_alloc(ctx, PDF_INT, 0, (pdf_obj **)&num);
    if (code < 0)
        return code;

    if ((malformed && !recovered) || (!real && doubleneg)) {
        if ((code = pdfi_set_warning_var(ctx, gs_note_error(gs_error_syntaxerror), NULL, E_PDF_MALFORMEDNUMBER, "pdfi_read_num", "Treating malformed number %s as 0", Buffer)) < 0) {
            goto exit;
        }
        num->value.i = 0;
    } else if (has_exponent) {
        float f, exp;
        char *p = strstr((const char *)Buffer, "e");

        if (p == NULL)
            p = strstr((const char *)Buffer, "E");

        if (p == NULL) {
            if ((code = pdfi_set_warning_var(ctx, gs_note_error(gs_error_syntaxerror), NULL, E_PDF_MALFORMEDNUMBER, "pdfi_read_num", "Treating malformed float %s as 0", Buffer)) < 0) {
                goto exit;
            }
            num->value.d = 0;
        } else {
            p++;

            if (sscanf((char *)p, "%g", &exp) != 1 || exp > 38) {
                if ((code = pdfi_set_warning_var(ctx, gs_note_error(gs_error_syntaxerror), NULL, E_PDF_MALFORMEDNUMBER, "pdfi_read_num", "Treating malformed float %s as 0", Buffer)) < 0) {
                    goto exit;
                }
                num->value.d = 0;
            } else {
                if (sscanf((char *)Buffer, "%g", &f) == 1) {
                    num->value.d = f;
                } else {
                    if ((code = pdfi_set_warning_var(ctx, gs_note_error(gs_error_syntaxerror), NULL, E_PDF_MALFORMEDNUMBER, "pdfi_read_num", "Treating malformed float %s as 0", Buffer)) < 0) {
                        goto exit;
                    }
                    num->value.d = 0;
                }
            }
        }
    } else if (real) {
        num->value.d = acrobat_compatible_atof((char *)Buffer);
    } else {
        /* The doubleneg case is taken care of above. */
        num->value.i = negative ? (int64_t)int_val * -1 : (int64_t)int_val;
    }
    if (ctx->args.pdfdebug) {
        if (real)
            dmprintf1(ctx->memory, " %f", num->value.d);
        else
            dmprintf1(ctx->memory, " %"PRIi64, num->value.i);
    }
    num->indirect_num = indirect_num;
    num->indirect_gen = indirect_gen;

    code = pdfi_push(ctx, (pdf_obj *)num);

exit:
    if (code < 0)
        pdfi_free_object((pdf_obj *)num);

    return code;
}

static int pdfi_read_name(pdf_context *ctx, pdf_c_stream *s, uint32_t indirect_num, uint32_t indirect_gen)
{
    char *Buffer, *NewBuf = NULL;
    unsigned short index = 0;
    short bytes = 0;
    uint32_t size = 256;
    pdf_name *name = NULL;
    int code;

    Buffer = (char *)gs_alloc_bytes(ctx->memory, size, "pdfi_read_name");
    if (Buffer == NULL)
        return_error(gs_error_VMerror);

    do {
        int c = pdfi_read_byte(ctx, s);
        if (c < 0)
            break;

        if (iswhite((char)c)) {
            Buffer[index] = 0x00;
            break;
        } else if (isdelimiter((char)c)) {
            pdfi_unread_byte(ctx, s, (char)c);
            Buffer[index] = 0x00;
            break;
        }
        Buffer[index] = (char)c;

        /* Check for and convert escaped name characters */
        if (c == '#') {
            byte NumBuf[2];

            bytes = pdfi_read_bytes(ctx, (byte *)&NumBuf, 1, 2, s);
            if (bytes < 2 || (!ishex(NumBuf[0]) || !ishex(NumBuf[1]))) {
                pdfi_set_warning(ctx, 0, NULL, W_PDF_BAD_NAME_ESCAPE, "pdfi_read_name", NULL);
                pdfi_unread(ctx, s, (byte *)NumBuf, bytes);
                /* This leaves the name buffer with a # in it, rather than anything sane! */
            }
            else
                Buffer[index] = (fromhex(NumBuf[0]) << 4) + fromhex(NumBuf[1]);
        }

        /* If we ran out of memory, increase the buffer size */
        if (index++ >= size - 1) {
            NewBuf = (char *)gs_alloc_bytes(ctx->memory, size + 256, "pdfi_read_name");
            if (NewBuf == NULL) {
                gs_free_object(ctx->memory, Buffer, "pdfi_read_name error");
                return_error(gs_error_VMerror);
            }
            memcpy(NewBuf, Buffer, size);
            gs_free_object(ctx->memory, Buffer, "pdfi_read_name");
            Buffer = NewBuf;
            size += 256;
        }
    } while(1);

    code = pdfi_object_alloc(ctx, PDF_NAME, index, (pdf_obj **)&name);
    if (code < 0) {
        gs_free_object(ctx->memory, Buffer, "pdfi_read_name error");
        return code;
    }
    memcpy(name->data, Buffer, index);
    name->indirect_num = indirect_num;
    name->indirect_gen = indirect_gen;

    if (ctx->args.pdfdebug)
        dmprintf1(ctx->memory, " /%s", Buffer);

    gs_free_object(ctx->memory, Buffer, "pdfi_read_name");

    code = pdfi_push(ctx, (pdf_obj *)name);

    if (code < 0)
        pdfi_free_object((pdf_obj *)name);

    return code;
}

static int pdfi_read_hexstring(pdf_context *ctx, pdf_c_stream *s, uint32_t indirect_num, uint32_t indirect_gen)
{
    char *Buffer, *NewBuf = NULL;
    unsigned short index = 0;
    uint32_t size = 256;
    pdf_string *string = NULL;
    int code, hex0, hex1;

    Buffer = (char *)gs_alloc_bytes(ctx->memory, size, "pdfi_read_hexstring");
    if (Buffer == NULL)
        return_error(gs_error_VMerror);

    if (ctx->args.pdfdebug)
        dmprintf(ctx->memory, " <");

    do {
        do {
            hex0 = pdfi_read_byte(ctx, s);
            if (hex0 < 0)
                break;
        } while(iswhite(hex0));
        if (hex0 < 0)
            break;

        if (hex0 == '>')
            break;

        if (ctx->args.pdfdebug)
            dmprintf1(ctx->memory, "%c", (char)hex0);

        do {
            hex1 = pdfi_read_byte(ctx, s);
            if (hex1 < 0)
                break;
        } while(iswhite(hex1));
        if (hex1 < 0)
            break;

        if (hex1 == '>') {
            /* PDF Reference 1.7 page 56:
             * "If the final digit of a hexadecimal string is missing that is,
             * if there is an odd number of digits the final digit is assumed to be 0."
             */
            hex1 = 0x30;
            if (!ishex(hex0) || !ishex(hex1)) {
                code = gs_note_error(gs_error_syntaxerror);
                goto exit;
            }
            Buffer[index] = (fromhex(hex0) << 4) + fromhex(hex1);
            if (ctx->args.pdfdebug)
                dmprintf1(ctx->memory, "%c", hex1);
            break;
        }

        if (!ishex(hex0) || !ishex(hex1)) {
            code = gs_note_error(gs_error_syntaxerror);
            goto exit;
        }

        if (ctx->args.pdfdebug)
            dmprintf1(ctx->memory, "%c", (char)hex1);

        Buffer[index] = (fromhex(hex0) << 4) + fromhex(hex1);

        if (index++ >= size - 1) {
            NewBuf = (char *)gs_alloc_bytes(ctx->memory, size + 256, "pdfi_read_hexstring");
            if (NewBuf == NULL) {
                code = gs_note_error(gs_error_VMerror);
                goto exit;
            }
            memcpy(NewBuf, Buffer, size);
            gs_free_object(ctx->memory, Buffer, "pdfi_read_hexstring");
            Buffer = NewBuf;
            size += 256;
        }
    } while(1);

    if (ctx->args.pdfdebug)
        dmprintf(ctx->memory, ">");

    code = pdfi_object_alloc(ctx, PDF_STRING, index, (pdf_obj **)&string);
    if (code < 0)
        goto exit;
    memcpy(string->data, Buffer, index);
    string->indirect_num = indirect_num;
    string->indirect_gen = indirect_gen;

    if (ctx->encryption.is_encrypted && ctx->encryption.decrypt_strings) {
        code = pdfi_decrypt_string(ctx, string);
        if (code < 0)
            return code;
    }

    code = pdfi_push(ctx, (pdf_obj *)string);
    if (code < 0)
        pdfi_free_object((pdf_obj *)string);

 exit:
    gs_free_object(ctx->memory, Buffer, "pdfi_read_hexstring");
    return code;
}

static int pdfi_read_string(pdf_context *ctx, pdf_c_stream *s, uint32_t indirect_num, uint32_t indirect_gen)
{
    char *Buffer, *NewBuf = NULL;
    unsigned short index = 0;
    uint32_t size = 256;
    pdf_string *string = NULL;
    int c, code, nesting = 0;
    bool escape = false, skip_eol = false, exit_loop = false;

    Buffer = (char *)gs_alloc_bytes(ctx->memory, size, "pdfi_read_string");
    if (Buffer == NULL)
        return_error(gs_error_VMerror);

    do {
        if (index >= size - 1) {
            NewBuf = (char *)gs_alloc_bytes(ctx->memory, size + 256, "pdfi_read_string");
            if (NewBuf == NULL) {
                gs_free_object(ctx->memory, Buffer, "pdfi_read_string error");
                return_error(gs_error_VMerror);
            }
            memcpy(NewBuf, Buffer, size);
            gs_free_object(ctx->memory, Buffer, "pdfi_read_string");
            Buffer = NewBuf;
            size += 256;
        }

        c = pdfi_read_byte(ctx, s);

        if (c < 0) {
            if (nesting > 0 && (code = pdfi_set_error_stop(ctx, gs_note_error(gs_error_syntaxerror), NULL, E_PDF_UNESCAPEDSTRING, "pdfi_read_string", NULL) < 0)) {
                gs_free_object(ctx->memory, Buffer, "pdfi_read_string error");
                return code;
            }
            Buffer[index] = 0x00;
            break;
        }

        if (skip_eol) {
            if (c == 0x0a || c == 0x0d)
                continue;
            skip_eol = false;
        }
        Buffer[index] = (char)c;

        if (escape) {
            escape = false;
            switch (Buffer[index]) {
                case 0x0a:
                case 0x0d:
                    skip_eol = true;
                    continue;
                case 'n':
                    Buffer[index] = 0x0a;
                    break;
                case 'r':
                    Buffer[index] = 0x0d;
                    break;
                case 't':
                    Buffer[index] = 0x09;
                    break;
                case 'b':
                    Buffer[index] = 0x08;
                    break;
                case 'f':
                    Buffer[index] = 0x0c;
                    break;
                case '(':
                case ')':
                case '\\':
                    break;
                case '0':
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                {
                    /* Octal chars can be 1, 2 or 3 chars in length, terminated either
                     * by being 3 chars long, EOFC, or a non-octal char. We do not allow
                     * line breaks in the middle of octal chars. */
                    int c1 = pdfi_read_byte(ctx, s);
                    c -= '0';
                    if (c1 < 0) {
                        /* Nothing to do, or unread */
                    } else if (c1 < '0' || c1 > '7') {
                        pdfi_unread_byte(ctx, s, (char)c1);
                    } else {
                        c = c*8 + c1 - '0';
                        c1 = pdfi_read_byte(ctx, s);
                        if (c1 < 0) {
                            /* Nothing to do, or unread */
                        } else if (c1 < '0' || c1 > '7') {
                            pdfi_unread_byte(ctx, s, (char)c1);
                        } else
                            c = c*8 + c1 - '0';
                    }
                    Buffer[index] = c;
                    break;
                }
                default:
                    /* PDF Reference, literal strings, if the character following a
                     * escape \ character is not recognised, then it is ignored.
                     */
                    escape = false;
                    index++;
                    continue;
            }
        } else {
            switch(Buffer[index]) {
                case 0x0d:
                    Buffer[index] = 0x0a;
                    /*fallthrough*/
                case 0x0a:
                    skip_eol = true;
                    break;
                case ')':
                    if (nesting == 0) {
                        Buffer[index] = 0x00;
                        exit_loop = true;
                    } else
                        nesting--;
                    break;
                case '\\':
                    escape = true;
                    continue;
                case '(':
                    nesting++;
                    break;
                default:
                    break;
            }
        }

        if (exit_loop)
            break;

        index++;
    } while(1);

    code = pdfi_object_alloc(ctx, PDF_STRING, index, (pdf_obj **)&string);
    if (code < 0) {
        gs_free_object(ctx->memory, Buffer, "pdfi_read_name error");
        return code;
    }
    memcpy(string->data, Buffer, index);
    string->indirect_num = indirect_num;
    string->indirect_gen = indirect_gen;

    gs_free_object(ctx->memory, Buffer, "pdfi_read_string");

    if (ctx->encryption.is_encrypted && ctx->encryption.decrypt_strings) {
        code = pdfi_decrypt_string(ctx, string);
        if (code < 0)
            return code;
    }

    if (ctx->args.pdfdebug) {
        int i;
        dmprintf(ctx->memory, " (");
        for (i=0;i<string->length;i++)
            dmprintf1(ctx->memory, "%c", string->data[i]);
        dmprintf(ctx->memory, ")");
    }

    code = pdfi_push(ctx, (pdf_obj *)string);
    if (code < 0) {
        pdfi_free_object((pdf_obj *)string);
        pdfi_set_error(ctx, code, NULL, 0, "pdfi_read_string", NULL);
    }

    return code;
}

int pdfi_read_dict(pdf_context *ctx, pdf_c_stream *s, uint32_t indirect_num, uint32_t indirect_gen)
{
    int code, depth;

    code = pdfi_read_token(ctx, s, indirect_num, indirect_gen);
    if (code < 0)
        return code;
    if (code == 0)
        return_error(gs_error_syntaxerror);

    if (pdfi_type_of(ctx->stack_top[-1]) != PDF_DICT_MARK)
        return_error(gs_error_typecheck);
    depth = pdfi_count_stack(ctx);

    do {
        code = pdfi_read_token(ctx, s, indirect_num, indirect_gen);
        if (code < 0)
            return code;
        if (code == 0)
            return_error(gs_error_syntaxerror);
    } while(pdfi_count_stack(ctx) > depth);
    return 0;
}

int pdfi_skip_comment(pdf_context *ctx, pdf_c_stream *s)
{
    int c;

    if (ctx->args.pdfdebug)
        dmprintf (ctx->memory, " %%");

    do {
        c = pdfi_read_byte(ctx, s);
        if (c < 0)
            break;

        if (ctx->args.pdfdebug)
            dmprintf1 (ctx->memory, "%c", (char)c);

    } while (c != 0x0a && c != 0x0d);

    return 0;
}

#define PARAM1(A) # A,
#define PARAM2(A,B) A,
static const char pdf_token_strings[][10] = {
#include "pdf_tokens.h"
};

#define nelems(A) (sizeof(A)/sizeof(A[0]))

typedef int (*bsearch_comparator)(const void *, const void *);

int pdfi_read_bare_keyword(pdf_context *ctx, pdf_c_stream *s)
{
    byte Buffer[256];
    int code, index = 0;
    int c;
    void *t;

    pdfi_skip_white(ctx, s);

    do {
        c = pdfi_read_byte(ctx, s);
        if (c < 0)
            break;

        if (iswhite(c) || isdelimiter(c)) {
            pdfi_unread_byte(ctx, s, (byte)c);
            break;
        }
        Buffer[index] = (byte)c;
        index++;
    } while (index < 255);

    if (index >= 255 || index == 0) {
        if ((code = pdfi_set_error_stop(ctx, gs_note_error(gs_error_syntaxerror), NULL, E_PDF_NOERROR, "pdfi_read_bare_keyword", "")) < 0) {
            return code;
        }
        return TOKEN_INVALID_KEY;
    }

    Buffer[index] = 0x00;
    t = bsearch((const void *)Buffer,
                (const void *)pdf_token_strings[TOKEN_INVALID_KEY+1],
                nelems(pdf_token_strings)-(TOKEN_INVALID_KEY+1),
                sizeof(pdf_token_strings[0]),
                (bsearch_comparator)&strcmp);
    if (t == NULL)
        return TOKEN_INVALID_KEY;

    if (ctx->args.pdfdebug)
        dmprintf1(ctx->memory, " %s\n", Buffer);

    return (((const char *)t) - pdf_token_strings[0]) / sizeof(pdf_token_strings[0]);
}

static pdf_key lookup_keyword(const byte *Buffer)
{
    void *t = bsearch((const void *)Buffer,
                      (const void *)pdf_token_strings[TOKEN_INVALID_KEY+1],
                      nelems(pdf_token_strings)-(TOKEN_INVALID_KEY+1),
                      sizeof(pdf_token_strings[0]),
                      (bsearch_comparator)&strcmp);
    if (t == NULL)
        return TOKEN_NOT_A_KEYWORD;

    return (pdf_key)((((const char *)t) - pdf_token_strings[0]) /
                     sizeof(pdf_token_strings[0]));
}

/* This function is slightly misnamed. We read 'keywords' from
 * the stream (including null, true, false and R), and will usually
 * return them directly as TOKENs cast to be pointers. In the event
 * that we can't match what we parse to a known keyword, we'll
 * instead return a PDF_KEYWORD object. In the even that we parse
 * an 'R', we will return a PDF_INDIRECT object.
 */
static int pdfi_read_keyword(pdf_context *ctx, pdf_c_stream *s, uint32_t indirect_num, uint32_t indirect_gen)
{
    byte Buffer[256];
    unsigned short index = 0;
    int c, code;
    pdf_keyword *keyword;
    pdf_key key;

    pdfi_skip_white(ctx, s);

    do {
        c = pdfi_read_byte(ctx, s);
        if (c < 0)
            break;

        if (iswhite(c) || isdelimiter(c)) {
            pdfi_unread_byte(ctx, s, (byte)c);
            break;
        }
        Buffer[index] = (byte)c;
        index++;
    } while (index < 255);

    if (index >= 255 || index == 0) {
        if ((code = pdfi_set_error_stop(ctx, gs_note_error(gs_error_syntaxerror), NULL, 0, "pdfi_read_keyword", NULL) < 0)) {
            return code;
        }
        key = (index >= 255 ? TOKEN_TOO_LONG : TOKEN_INVALID_KEY);
        index = 0;
        Buffer[0] = 0;
    } else {
        Buffer[index] = 0x00;
        key = lookup_keyword(Buffer);

        if (ctx->args.pdfdebug)
            dmprintf1(ctx->memory, " %s\n", Buffer);

        switch (key) {
            case TOKEN_R:
            {
                pdf_indirect_ref *o;
                uint64_t obj_num;
                uint32_t gen_num;

                if(pdfi_count_stack(ctx) < 2) {
                    pdfi_clearstack(ctx);
                    return_error(gs_error_stackunderflow);
                }

                if(pdfi_type_of(ctx->stack_top[-1]) != PDF_INT || pdfi_type_of(ctx->stack_top[-2]) != PDF_INT) {
                    pdfi_clearstack(ctx);
                    return_error(gs_error_typecheck);
                }

                gen_num = ((pdf_num *)ctx->stack_top[-1])->value.i;
                pdfi_pop(ctx, 1);
                obj_num = ((pdf_num *)ctx->stack_top[-1])->value.i;
                pdfi_pop(ctx, 1);

                code = pdfi_object_alloc(ctx, PDF_INDIRECT, 0, (pdf_obj **)&o);
                if (code < 0)
                    return code;

                o->ref_generation_num = gen_num;
                o->ref_object_num = obj_num;
                o->indirect_num = indirect_num;
                o->indirect_gen = indirect_gen;

                code = pdfi_push(ctx, (pdf_obj *)o);
                if (code < 0)
                    pdfi_free_object((pdf_obj *)o);

                return code;
            }
            case TOKEN_NOT_A_KEYWORD:
                 /* Unexpected keyword found. We'll allocate an object for the buffer below. */
                 break;
            case TOKEN_STREAM:
                code = pdfi_skip_eol(ctx, s);
                if (code < 0)
                    return code;
                /* fallthrough */
            case TOKEN_PDF_TRUE:
            case TOKEN_PDF_FALSE:
            case TOKEN_null:
            default:
                /* This is the fast, common exit case. We just push the key
                 * onto the stack. No allocation required. No deallocation
                 * in the case of error. */
                return pdfi_push(ctx, (pdf_obj *)(intptr_t)key);
        }
    }

    /* Unexpected keyword. We can't handle this with the fast no-allocation case. */
    code = pdfi_object_alloc(ctx, PDF_KEYWORD, index, (pdf_obj **)&keyword);
    if (code < 0)
        return code;

    if (index)
        memcpy(keyword->data, Buffer, index);

    /* keyword->length set as part of allocation. */
    keyword->indirect_num = indirect_num;
    keyword->indirect_gen = indirect_gen;

    code = pdfi_push(ctx, (pdf_obj *)keyword);
    if (code < 0)
        pdfi_free_object((pdf_obj *)keyword);

    return code;
}

/* This function reads from the given stream, at the current offset in the stream,
 * a single PDF 'token' and returns it on the stack.
 */
int pdfi_read_token(pdf_context *ctx, pdf_c_stream *s, uint32_t indirect_num, uint32_t indirect_gen)
{
    int c, code;

rescan:
    pdfi_skip_white(ctx, s);

    c = pdfi_read_byte(ctx, s);
    if (c == EOFC)
        return 0;
    if (c < 0)
        return_error(gs_error_ioerror);

    switch(c) {
        case 0x30:
        case 0x31:
        case 0x32:
        case 0x33:
        case 0x34:
        case 0x35:
        case 0x36:
        case 0x37:
        case 0x38:
        case 0x39:
        case '+':
        case '-':
        case '.':
            pdfi_unread_byte(ctx, s, (byte)c);
            code = pdfi_read_num(ctx, s, indirect_num, indirect_gen);
            if (code < 0)
                return code;
            break;
        case '/':
            code = pdfi_read_name(ctx, s, indirect_num, indirect_gen);
            if (code < 0)
                return code;
            return 1;
            break;
        case '<':
            c = pdfi_read_byte(ctx, s);
            if (c < 0)
                return (gs_error_ioerror);
            if (iswhite(c)) {
                code = pdfi_skip_white(ctx, s);
                if (code < 0)
                    return code;
                c = pdfi_read_byte(ctx, s);
            }
            if (c == '<') {
                if (ctx->args.pdfdebug)
                    dmprintf (ctx->memory, " <<\n");
                if (ctx->object_nesting < MAX_NESTING_DEPTH) {
                    ctx->object_nesting++;
                    code = pdfi_mark_stack(ctx, PDF_DICT_MARK);
                    if (code < 0)
                        return code;
                }
                else if ((code = pdfi_set_error_stop(ctx, gs_note_error(gs_error_limitcheck), NULL, E_PDF_NESTEDTOODEEP, "pdfi_read_token", NULL) < 0)) {
                    return code;
                }
                return 1;
            } else if (c == '>') {
                pdfi_unread_byte(ctx, s, (byte)c);
                code = pdfi_read_hexstring(ctx, s, indirect_num, indirect_gen);
                if (code < 0)
                    return code;
                return 1;
            } else if (ishex(c)) {
                pdfi_unread_byte(ctx, s, (byte)c);
                code = pdfi_read_hexstring(ctx, s, indirect_num, indirect_gen);
                if (code < 0)
                    return code;
            }
            else
                return_error(gs_error_syntaxerror);
            break;
        case '>':
            c = pdfi_read_byte(ctx, s);
            if (c < 0)
                return (gs_error_ioerror);
            if (c == '>') {
                if (ctx->object_nesting > 0) {
                    ctx->object_nesting--;
                    code = pdfi_dict_from_stack(ctx, indirect_num, indirect_gen, false);
                    if (code < 0)
                        return code;
                } else {
                    if ((code = pdfi_set_error_stop(ctx, gs_note_error(gs_error_unmatchedmark), NULL, E_PDF_UNMATCHEDMARK, "pdfi_read_token", NULL) < 0)) {
                        return code;
                    }
                    goto rescan;
                }
                return 1;
            } else {
                pdfi_unread_byte(ctx, s, (byte)c);
                return_error(gs_error_syntaxerror);
            }
            break;
        case '(':
            code = pdfi_read_string(ctx, s, indirect_num, indirect_gen);
            if (code < 0)
                return code;
            return 1;
            break;
        case '[':
            if (ctx->args.pdfdebug)
                dmprintf (ctx->memory, "[");
            if (ctx->object_nesting < MAX_NESTING_DEPTH) {
                ctx->object_nesting++;
                code = pdfi_mark_stack(ctx, PDF_ARRAY_MARK);
                if (code < 0)
                    return code;
            } else
                if ((code = pdfi_set_error_stop(ctx, gs_note_error(gs_error_syntaxerror), NULL, E_PDF_NESTEDTOODEEP, "pdfi_read_token", NULL)) < 0)
                    return code;
            return 1;
            break;
        case ']':
            if (ctx->object_nesting > 0) {
                ctx->object_nesting--;
                code = pdfi_array_from_stack(ctx, indirect_num, indirect_gen);
                if (code < 0)
                    return code;
            } else {
                if ((code = pdfi_set_error_stop(ctx, gs_note_error(gs_error_unmatchedmark), NULL, E_PDF_UNMATCHEDMARK, "pdfi_read_token", NULL) < 0)) {
                    return code;
                }
                goto rescan;
            }
            break;
        case '{':
            if (ctx->args.pdfdebug)
                dmprintf (ctx->memory, "{");
            code = pdfi_mark_stack(ctx, PDF_PROC_MARK);
            if (code < 0)
                return code;
            return 1;
            break;
        case '}':
            pdfi_clear_to_mark(ctx);
            goto rescan;
            break;
        case '%':
            pdfi_skip_comment(ctx, s);
            goto rescan;
            break;
        default:
            if (isdelimiter(c)) {
                if ((code = pdfi_set_error_stop(ctx, gs_note_error(gs_error_syntaxerror), NULL, 0, "pdfi_read_token", NULL) < 0)) {
                    return code;
                }
                goto rescan;
            }
            pdfi_unread_byte(ctx, s, (byte)c);
            code = pdfi_read_keyword(ctx, s, indirect_num, indirect_gen);
            if (code < 0)
                return code;
            return 1;
            break;
    }
    return 1;
}

/* In contrast to the 'read' functions, the 'make' functions create an object with a
 * reference count of 1. This indicates that the caller holds the reference. Thus the
 * caller need not increment the reference count to the object, but must decrement
 * it (pdf_countdown) before exiting.
 */
int pdfi_name_alloc(pdf_context *ctx, byte *n, uint32_t size, pdf_obj **o)
{
    int code;
    *o = NULL;

    code = pdfi_object_alloc(ctx, PDF_NAME, size, o);
    if (code < 0)
        return code;

    memcpy(((pdf_name *)*o)->data, n, size);

    return 0;
}

static char op_table_3[5][3] = {
    "BDC", "BMC", "EMC", "SCN", "scn"
};

static char op_table_2[39][2] = {
    "b*", "BI", "BT", "BX", "cm", "CS", "cs", "EI", "d0", "d1", "Do", "DP", "ET", "EX", "f*", "gs", "ID", "MP", "re", "RG",
    "rg", "ri", "SC", "sc", "sh", "T*", "Tc", "Td", "TD", "Tf", "Tj", "TJ", "TL", "Tm", "Tr", "Ts", "Tw", "Tz", "W*",
};

static char op_table_1[27][1] = {
    "b", "B", "c", "d", "f", "F", "G", "g", "h", "i", "j", "J", "K", "k", "l", "m", "n", "q", "Q", "s", "S", "v", "w", "W",
    "y", "'", "\""
};

/* forward definition for the 'split_bogus_operator' function to use */
static int pdfi_interpret_stream_operator(pdf_context *ctx, pdf_c_stream *source,
                                          pdf_dict *stream_dict, pdf_dict *page_dict);

static int
make_keyword_obj(pdf_context *ctx, const byte *data, int length, pdf_keyword **pkey)
{
    byte Buffer[256];
    pdf_key key;
    int code;

    memcpy(Buffer, data, length);
    Buffer[length] = 0;
    key = lookup_keyword(Buffer);
    if (key != TOKEN_INVALID_KEY) {
        /* The common case. We've found a real key, just cast the token to
         * a pointer, and return that. */
        *pkey = (pdf_keyword *)PDF_TOKEN_AS_OBJ(key);
        return 1;
    }
    /* We still haven't found a real keyword. Allocate a new object and
     * return it. */
    code = pdfi_object_alloc(ctx, PDF_KEYWORD, length, (pdf_obj **)pkey);
    if (code < 0)
        return code;
    if (length)
        memcpy((*pkey)->data, Buffer, length);
    pdfi_countup(*pkey);

    return 1;
}

static int search_table_3(pdf_context *ctx, unsigned char *str, pdf_keyword **key)
{
    int i;

    for (i = 0; i < 5; i++) {
        if (memcmp(str, op_table_3[i], 3) == 0)
            return make_keyword_obj(ctx, str, 3, key);
    }
    return 0;
}

static int search_table_2(pdf_context *ctx, unsigned char *str, pdf_keyword **key)
{
    int i;

    for (i = 0; i < 39; i++) {
        if (memcmp(str, op_table_2[i], 2) == 0)
            return make_keyword_obj(ctx, str, 2, key);
    }
    return 0;
}

static int search_table_1(pdf_context *ctx, unsigned char *str, pdf_keyword **key)
{
    int i;

    for (i = 0; i < 27; i++) {
        if (memcmp(str, op_table_1[i], 1) == 0)
            return make_keyword_obj(ctx, str, 1, key);
    }
    return 0;
}

static int split_bogus_operator(pdf_context *ctx, pdf_c_stream *source, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    int code = 0;
    pdf_keyword *keyword = (pdf_keyword *)ctx->stack_top[-1], *key1 = NULL, *key2 = NULL;
    int length = keyword->length - 6;

    if (length > 0) {
        /* Longer than 2 3-character operators, we only allow for up to two
         * operators. Check to see if it includes an endstream or endobj.
         */
        if (memcmp(&keyword->data[length], "endobj", 6) == 0) {
            /* Keyword is "<something>endobj". So make a keyword just from
             * <something>, push that, execute it, then push endobj. */
            code = make_keyword_obj(ctx, keyword->data, length, &key1);
            if (code < 0)
                goto error_exit;
            pdfi_pop(ctx, 1);
            pdfi_push(ctx, (pdf_obj *)key1);
            pdfi_countdown(key1); /* Drop the reference returned by make_keyword_obj. */
            code = pdfi_interpret_stream_operator(ctx, source, stream_dict, page_dict);
            if (code < 0)
                goto error_exit;
            pdfi_push(ctx, PDF_TOKEN_AS_OBJ(TOKEN_ENDOBJ));
            return 0;
        } else {
            length = keyword->length - 9;
            if (length > 0 && memcmp(&keyword->data[length], "endstream", 9) == 0) {
                /* Keyword is "<something>endstream". So make a keyword just from
                 * <something>, push that, execute it, then push endstream. */
                code = make_keyword_obj(ctx, keyword->data, length, &key1);
                if (code < 0)
                    goto error_exit;
                pdfi_pop(ctx, 1);
                pdfi_push(ctx, (pdf_obj *)key1);
                pdfi_countdown(key1); /* Drop the reference returned by make_keyword_obj. */
                code = pdfi_interpret_stream_operator(ctx, source, stream_dict, page_dict);
                if (code < 0)
                    goto error_exit;
                pdfi_push(ctx, PDF_TOKEN_AS_OBJ(TOKEN_ENDSTREAM));
                return 0;
            } else {
                pdfi_clearstack(ctx);
                return 0;
            }
        }
    }

    if (keyword->length > 3) {
        code = search_table_3(ctx, keyword->data, &key1);
        if (code < 0)
            goto error_exit;

        if (code > 0) {
            switch (keyword->length - 3) {
                case 1:
                    code = search_table_1(ctx, &keyword->data[3], &key2);
                    break;
                case 2:
                    code = search_table_2(ctx, &keyword->data[3], &key2);
                    break;
                case 3:
                    code = search_table_3(ctx, &keyword->data[3], &key2);
                    break;
                default:
                    goto error_exit;
            }
        }
        if (code < 0)
            goto error_exit;
        if (code > 0)
            goto match;
    }
    pdfi_countdown(key1);
    pdfi_countdown(key2);
    key1 = NULL;
    key2 = NULL;

    if (keyword->length > 5 || keyword->length < 2)
        goto error_exit;

    code = search_table_2(ctx, keyword->data, &key1);
    if (code < 0)
        goto error_exit;

    if (code > 0) {
        switch(keyword->length - 2) {
            case 1:
                code = search_table_1(ctx, &keyword->data[2], &key2);
                break;
            case 2:
                code = search_table_2(ctx, &keyword->data[2], &key2);
                break;
            case 3:
                code = search_table_3(ctx, &keyword->data[2], &key2);
                break;
            default:
                goto error_exit;
        }
        if (code < 0)
            goto error_exit;
        if (code > 0)
            goto match;
    }
    pdfi_countdown(key1);
    pdfi_countdown(key2);
    key1 = NULL;
    key2 = NULL;

    if (keyword->length > 4)
        goto error_exit;

    code = search_table_1(ctx, keyword->data, &key1);
    if (code <= 0)
        goto error_exit;

    switch(keyword->length - 1) {
        case 1:
            code = search_table_1(ctx, &keyword->data[1], &key2);
            break;
        case 2:
            code = search_table_2(ctx, &keyword->data[1], &key2);
            break;
        case 3:
            code = search_table_3(ctx, &keyword->data[1], &key2);
            break;
        default:
            goto error_exit;
    }
    if (code <= 0)
        goto error_exit;

match:
    pdfi_set_warning(ctx, 0, NULL, W_PDF_MISSING_WHITE_OPS, "split_bogus_operator", NULL);
    /* If we get here, we have two PDF_KEYWORD objects. We push them on the stack
     * one at a time, and execute them.
     */
    pdfi_push(ctx, (pdf_obj *)key1);
    code = pdfi_interpret_stream_operator(ctx, source, stream_dict, page_dict);
    if (code < 0)
        goto error_exit;

    pdfi_push(ctx, (pdf_obj *)key2);
    code = pdfi_interpret_stream_operator(ctx, source, stream_dict, page_dict);

    pdfi_countdown(key1);
    pdfi_countdown(key2);
    pdfi_clearstack(ctx);
    return code;

error_exit:
    pdfi_set_error(ctx, code, NULL, E_PDF_TOKENERROR, "split_bogus_operator", NULL);
    pdfi_countdown(key1);
    pdfi_countdown(key2);
    pdfi_clearstack(ctx);
    return code;
}

static int pdfi_interpret_stream_operator(pdf_context *ctx, pdf_c_stream *source,
                                          pdf_dict *stream_dict, pdf_dict *page_dict)
{
    pdf_obj *keyword = ctx->stack_top[-1];
    int code = 0;

    if (keyword < PDF_TOKEN_AS_OBJ(TOKEN__LAST_KEY))
    {
        switch((uintptr_t)keyword) {
            case TOKEN_b:           /* closepath, fill, stroke */
                pdfi_pop(ctx, 1);
                code = pdfi_b(ctx);
                break;
            case TOKEN_B:           /* fill, stroke */
                pdfi_pop(ctx, 1);
                code = pdfi_B(ctx);
                break;
            case TOKEN_bstar:       /* closepath, eofill, stroke */
                pdfi_pop(ctx, 1);
                code = pdfi_b_star(ctx);
                break;
            case TOKEN_Bstar:       /* eofill, stroke */
                pdfi_pop(ctx, 1);
                code = pdfi_B_star(ctx);
                break;
            case TOKEN_BI:       /* begin inline image */
                pdfi_pop(ctx, 1);
                code = pdfi_BI(ctx);
                break;
            case TOKEN_BDC:   /* begin marked content sequence with property list */
                pdfi_pop(ctx, 1);
                code = pdfi_op_BDC(ctx, stream_dict, page_dict);
                break;
            case TOKEN_BMC:   /* begin marked content sequence */
                pdfi_pop(ctx, 1);
                code = pdfi_op_BMC(ctx);
                break;
            case TOKEN_BT:       /* begin text */
                pdfi_pop(ctx, 1);
                code = pdfi_BT(ctx);
                break;
            case TOKEN_BX:       /* begin compatibility section */
                pdfi_pop(ctx, 1);
                break;
            case TOKEN_c:           /* curveto */
                pdfi_pop(ctx, 1);
                code = pdfi_curveto(ctx);
                break;
            case TOKEN_cm:       /* concat */
                pdfi_pop(ctx, 1);
                code = pdfi_concat(ctx);
                break;
            case TOKEN_CS:       /* set stroke colour space */
                pdfi_pop(ctx, 1);
                code = pdfi_setstrokecolor_space(ctx, stream_dict, page_dict);
                break;
            case TOKEN_cs:       /* set non-stroke colour space */
                pdfi_pop(ctx, 1);
                code = pdfi_setfillcolor_space(ctx, stream_dict, page_dict);
                break;
            case TOKEN_d:           /* set dash params */
                pdfi_pop(ctx, 1);
                code = pdfi_setdash(ctx);
                break;
            case TOKEN_d0:       /* set type 3 font glyph width */
                pdfi_pop(ctx, 1);
                code = pdfi_d0(ctx);
                break;
            case TOKEN_d1:       /* set type 3 font glyph width and bounding box */
                pdfi_pop(ctx, 1);
                code = pdfi_d1(ctx);
                break;
            case TOKEN_Do:       /* invoke named XObject */
                pdfi_pop(ctx, 1);
                code = pdfi_Do(ctx, stream_dict, page_dict);
                break;
            case TOKEN_DP:       /* define marked content point with property list */
                pdfi_pop(ctx, 1);
                code = pdfi_op_DP(ctx, stream_dict, page_dict);
                break;
            case TOKEN_EI:       /* end inline image */
                pdfi_pop(ctx, 1);
                code = pdfi_EI(ctx);
                break;
            case TOKEN_ET:       /* end text */
                pdfi_pop(ctx, 1);
                code = pdfi_ET(ctx);
                break;
            case TOKEN_EMC:   /* end marked content sequence */
                pdfi_pop(ctx, 1);
                code = pdfi_op_EMC(ctx);
                break;
            case TOKEN_EX:       /* end compatibility section */
                pdfi_pop(ctx, 1);
                break;
            case TOKEN_f:           /* fill */
                pdfi_pop(ctx, 1);
                code = pdfi_fill(ctx);
                break;
            case TOKEN_F:           /* fill (obselete operator) */
                pdfi_pop(ctx, 1);
                code = pdfi_fill(ctx);
                break;
            case TOKEN_fstar:       /* eofill */
                pdfi_pop(ctx, 1);
                code = pdfi_eofill(ctx);
                break;
            case TOKEN_G:           /* setgray for stroke */
                pdfi_pop(ctx, 1);
                code = pdfi_setgraystroke(ctx);
                break;
            case TOKEN_g:           /* setgray for non-stroke */
                pdfi_pop(ctx, 1);
                code = pdfi_setgrayfill(ctx);
                break;
            case TOKEN_gs:       /* set graphics state from dictionary */
                pdfi_pop(ctx, 1);
                code = pdfi_setgstate(ctx, stream_dict, page_dict);
                break;
            case TOKEN_h:           /* closepath */
                pdfi_pop(ctx, 1);
                code = pdfi_closepath(ctx);
                break;
            case TOKEN_i:           /* setflat */
                pdfi_pop(ctx, 1);
                code = pdfi_setflat(ctx);
                break;
            case TOKEN_ID:       /* begin inline image data */
                pdfi_pop(ctx, 1);
                code = pdfi_ID(ctx, stream_dict, page_dict, source);
                break;
            case TOKEN_j:           /* setlinejoin */
                pdfi_pop(ctx, 1);
                code = pdfi_setlinejoin(ctx);
                break;
            case TOKEN_J:           /* setlinecap */
                pdfi_pop(ctx, 1);
                code = pdfi_setlinecap(ctx);
                break;
            case TOKEN_K:           /* setcmyk for non-stroke */
                pdfi_pop(ctx, 1);
                code = pdfi_setcmykstroke(ctx);
                break;
            case TOKEN_k:           /* setcmyk for non-stroke */
                pdfi_pop(ctx, 1);
                code = pdfi_setcmykfill(ctx);
                break;
            case TOKEN_l:           /* lineto */
                pdfi_pop(ctx, 1);
                code = pdfi_lineto(ctx);
                break;
            case TOKEN_m:           /* moveto */
                pdfi_pop(ctx, 1);
                code = pdfi_moveto(ctx);
                break;
            case TOKEN_M:           /* setmiterlimit */
                pdfi_pop(ctx, 1);
                code = pdfi_setmiterlimit(ctx);
                break;
            case TOKEN_MP:       /* define marked content point */
                pdfi_pop(ctx, 1);
                code = pdfi_op_MP(ctx);
                break;
            case TOKEN_n:           /* newpath */
                pdfi_pop(ctx, 1);
                code = pdfi_newpath(ctx);
                break;
            case TOKEN_q:           /* gsave */
                pdfi_pop(ctx, 1);
                code = pdfi_op_q(ctx);
                break;
            case TOKEN_Q:           /* grestore */
                pdfi_pop(ctx, 1);
                code = pdfi_op_Q(ctx);
                break;
            case TOKEN_r:       /* non-standard set rgb colour for non-stroke */
                pdfi_pop(ctx, 1);
                code = pdfi_setrgbfill_array(ctx);
                break;
            case TOKEN_re:       /* append rectangle */
                pdfi_pop(ctx, 1);
                code = pdfi_rectpath(ctx);
                break;
            case TOKEN_RG:       /* set rgb colour for stroke */
                pdfi_pop(ctx, 1);
                code = pdfi_setrgbstroke(ctx);
                break;
            case TOKEN_rg:       /* set rgb colour for non-stroke */
                pdfi_pop(ctx, 1);
                code = pdfi_setrgbfill(ctx);
                break;
            case TOKEN_ri:       /* set rendering intent */
                pdfi_pop(ctx, 1);
                code = pdfi_ri(ctx);
                break;
            case TOKEN_s:           /* closepath, stroke */
                pdfi_pop(ctx, 1);
                code = pdfi_closepath_stroke(ctx);
                break;
            case TOKEN_S:           /* stroke */
                pdfi_pop(ctx, 1);
                code = pdfi_stroke(ctx);
                break;
            case TOKEN_SC:       /* set colour for stroke */
                pdfi_pop(ctx, 1);
                code = pdfi_setstrokecolor(ctx);
                break;
            case TOKEN_sc:       /* set colour for non-stroke */
                pdfi_pop(ctx, 1);
                code = pdfi_setfillcolor(ctx);
                break;
            case TOKEN_SCN:   /* set special colour for stroke */
                pdfi_pop(ctx, 1);
                code = pdfi_setcolorN(ctx, stream_dict, page_dict, false);
                break;
            case TOKEN_scn:   /* set special colour for non-stroke */
                pdfi_pop(ctx, 1);
                code = pdfi_setcolorN(ctx, stream_dict, page_dict, true);
                break;
            case TOKEN_sh:       /* fill with sahding pattern */
                pdfi_pop(ctx, 1);
                code = pdfi_shading(ctx, stream_dict, page_dict);
                break;
            case TOKEN_Tstar:       /* Move to start of next text line */
                pdfi_pop(ctx, 1);
                code = pdfi_T_star(ctx);
                break;
            case TOKEN_Tc:       /* set character spacing */
                pdfi_pop(ctx, 1);
                code = pdfi_Tc(ctx);
                break;
            case TOKEN_Td:       /* move text position */
                pdfi_pop(ctx, 1);
                code = pdfi_Td(ctx);
                break;
            case TOKEN_TD:       /* Move text position, set leading */
                pdfi_pop(ctx, 1);
                code = pdfi_TD(ctx);
                break;
            case TOKEN_Tf:       /* set font and size */
                pdfi_pop(ctx, 1);
                code = pdfi_Tf(ctx, stream_dict, page_dict);
                break;
            case TOKEN_Tj:       /* show text */
                pdfi_pop(ctx, 1);
                code = pdfi_Tj(ctx);
                break;
            case TOKEN_TJ:       /* show text with individual glyph positioning */
                pdfi_pop(ctx, 1);
                code = pdfi_TJ(ctx);
                break;
            case TOKEN_TL:       /* set text leading */
                pdfi_pop(ctx, 1);
                code = pdfi_TL(ctx);
                break;
            case TOKEN_Tm:       /* set text matrix */
                pdfi_pop(ctx, 1);
                code = pdfi_Tm(ctx);
                break;
            case TOKEN_Tr:       /* set text rendering mode */
                pdfi_pop(ctx, 1);
                code = pdfi_Tr(ctx);
                break;
            case TOKEN_Ts:       /* set text rise */
                pdfi_pop(ctx, 1);
                code = pdfi_Ts(ctx);
                break;
            case TOKEN_Tw:       /* set word spacing */
                pdfi_pop(ctx, 1);
                code = pdfi_Tw(ctx);
                break;
            case TOKEN_Tz:       /* set text matrix */
                pdfi_pop(ctx, 1);
                code = pdfi_Tz(ctx);
                break;
            case TOKEN_v:           /* append curve (initial point replicated) */
                pdfi_pop(ctx, 1);
                code = pdfi_v_curveto(ctx);
                break;
            case TOKEN_w:           /* setlinewidth */
                pdfi_pop(ctx, 1);
                code = pdfi_setlinewidth(ctx);
                break;
            case TOKEN_W:           /* clip */
                pdfi_pop(ctx, 1);
                ctx->clip_active = true;
                ctx->do_eoclip = false;
                break;
            case TOKEN_Wstar:       /* eoclip */
                pdfi_pop(ctx, 1);
                ctx->clip_active = true;
                ctx->do_eoclip = true;
                break;
            case TOKEN_y:           /* append curve (final point replicated) */
                pdfi_pop(ctx, 1);
                code = pdfi_y_curveto(ctx);
                break;
            case TOKEN_APOSTROPHE:          /* move to next line and show text */
                pdfi_pop(ctx, 1);
                code = pdfi_singlequote(ctx);
                break;
            case TOKEN_QUOTE:           /* set word and character spacing, move to next line, show text */
                pdfi_pop(ctx, 1);
                code = pdfi_doublequote(ctx);
                break;
            default:
                /* Shouldn't we return an error here? Original code didn't seem to. */
                break;
        }
        /* We use a return value of 1 to indicate a repaired keyword (a pair of operators
         * was concatenated, and we split them up). We must not return a value > 0 from here
         * to avoid tripping that test.
         */
        if (code > 0)
            code = 0;
        return code;
    } else {
        /* This means we either have a corrupted or illegal operator. The most
         * usual corruption is two concatented operators (eg QBT instead of Q BT)
         * I plan to tackle this by trying to see if I can make two or more operators
         * out of the mangled one.
         */
        code = split_bogus_operator(ctx, source, stream_dict, page_dict);
        if (code < 0)
            return code;
        if (pdfi_count_stack(ctx) > 0) {
            keyword = ctx->stack_top[-1];
            if (keyword != PDF_TOKEN_AS_OBJ(TOKEN_NOT_A_KEYWORD))
                return REPAIRED_KEYWORD;
        }
    }
    return 0;
}

void local_save_stream_state(pdf_context *ctx, stream_save *local_save)
{
    /* copy the 'save_stream' data from the context to a local structure */
    local_save->stream_offset = ctx->current_stream_save.stream_offset;
    local_save->gsave_level = ctx->current_stream_save.gsave_level;
    local_save->stack_count = ctx->current_stream_save.stack_count;
    local_save->group_depth = ctx->current_stream_save.group_depth;
}

void cleanup_context_interpretation(pdf_context *ctx, stream_save *local_save)
{
    pdfi_seek(ctx, ctx->main_stream, ctx->current_stream_save.stream_offset, SEEK_SET);
    /* The transparency group implenetation does a gsave, so the end group does a
     * grestore. Therefore we need to do this before we check the saved gstate depth
     */
    if (ctx->current_stream_save.group_depth != local_save->group_depth) {
        pdfi_set_warning(ctx, 0, NULL, W_PDF_GROUPERROR, "pdfi_cleanup_context_interpretation", NULL);
        while (ctx->current_stream_save.group_depth > local_save->group_depth)
            pdfi_trans_end_group(ctx);
    }
    if (ctx->pgs->level > ctx->current_stream_save.gsave_level)
        pdfi_set_warning(ctx, 0, NULL, W_PDF_TOOMANYq, "pdfi_cleanup_context_interpretation", NULL);
    if (pdfi_count_stack(ctx) > ctx->current_stream_save.stack_count)
        pdfi_set_warning(ctx, 0, NULL, W_PDF_STACKGARBAGE, "pdfi_cleanup_context_interpretation", NULL);
    while (ctx->pgs->level > ctx->current_stream_save.gsave_level)
        pdfi_grestore(ctx);
    pdfi_clearstack(ctx);
}

void local_restore_stream_state(pdf_context *ctx, stream_save *local_save)
{
    /* Put the entries stored in the context back to what they were on entry
     * We shouldn't really need to do this, the cleanup above should mean all the
     * entries are properly reset.
     */
    ctx->current_stream_save.stream_offset = local_save->stream_offset;
    ctx->current_stream_save.gsave_level = local_save->gsave_level;
    ctx->current_stream_save.stack_count = local_save->stack_count;
    ctx->current_stream_save.group_depth = local_save->group_depth;
}

void initialise_stream_save(pdf_context *ctx)
{
    /* Set up the values in the context to the current values */
    ctx->current_stream_save.stream_offset = pdfi_tell(ctx->main_stream);
    ctx->current_stream_save.gsave_level = ctx->pgs->level;
    ctx->current_stream_save.stack_count = pdfi_count_total_stack(ctx);
}

/* Run a stream in a sub-context (saves/restores DefaultQState) */
int pdfi_run_context(pdf_context *ctx, pdf_stream *stream_obj,
                     pdf_dict *page_dict, bool stoponerror, const char *desc)
{
    int code = 0, code1 = 0;
    gs_gstate *DefaultQState = NULL;
    /* Save any existing Default* colour spaces */
    gs_color_space *PageDefaultGray = ctx->page.DefaultGray_cs;
    gs_color_space *PageDefaultRGB = ctx->page.DefaultRGB_cs;
    gs_color_space *PageDefaultCMYK = ctx->page.DefaultCMYK_cs;

    ctx->page.DefaultGray_cs = NULL;
    ctx->page.DefaultRGB_cs = NULL;
    ctx->page.DefaultCMYK_cs = NULL;

#if DEBUG_CONTEXT
    dbgmprintf(ctx->memory, "pdfi_run_context BEGIN\n");
#endif
    /* If the stream has any Default* colour spaces, replace the page level ones.
     * This will derement the reference counts to the current spaces if they are replaced.
     */
    code = pdfi_setup_DefaultSpaces(ctx, stream_obj->stream_dict);
    if (code < 0)
        goto exit;

    /* If no Default* space found, try using the Page level ones (if any) */
    if (ctx->page.DefaultGray_cs == NULL) {
        ctx->page.DefaultGray_cs = PageDefaultGray;
        rc_increment(PageDefaultGray);
    }
    if (ctx->page.DefaultRGB_cs == NULL) {
        ctx->page.DefaultRGB_cs = PageDefaultRGB;
        rc_increment(PageDefaultRGB);
    }
    if (ctx->page.DefaultCMYK_cs == NULL) {
        ctx->page.DefaultCMYK_cs = PageDefaultCMYK;
        rc_increment(PageDefaultCMYK);
    }

    code = pdfi_copy_DefaultQState(ctx, &DefaultQState);
    if (code < 0)
        goto exit;

    code = pdfi_set_DefaultQState(ctx, ctx->pgs);
    if (code < 0)
        goto exit;

    code = pdfi_interpret_inner_content_stream(ctx, stream_obj, page_dict, stoponerror, desc);

    code1 = pdfi_restore_DefaultQState(ctx, &DefaultQState);
    if (code >= 0)
        code = code1;

exit:
    if (DefaultQState != NULL) {
        gs_gstate_free(DefaultQState);
        DefaultQState = NULL;
    }

    /* Count down any Default* colour spaces */
    rc_decrement(ctx->page.DefaultGray_cs, "pdfi_run_context");
    rc_decrement(ctx->page.DefaultRGB_cs, "pdfi_run_context");
    rc_decrement(ctx->page.DefaultCMYK_cs, "pdfi_run_context");

    /* And restore the page level ones (if any) */
    ctx->page.DefaultGray_cs = PageDefaultGray;
    ctx->page.DefaultRGB_cs = PageDefaultRGB;
    ctx->page.DefaultCMYK_cs = PageDefaultCMYK;

#if DEBUG_CONTEXT
    dbgmprintf(ctx->memory, "pdfi_run_context END\n");
#endif
    return code;
}


/* Interpret a sub-content stream, with some handling of error recovery, clearing stack, etc.
 * This temporarily turns on pdfstoponerror if requested.
 * It will make sure the stack is cleared and the gstate is matched.
 */
static int
pdfi_interpret_inner_content(pdf_context *ctx, pdf_c_stream *content_stream, pdf_stream *stream_obj,
                             pdf_dict *page_dict, bool stoponerror, const char *desc)
{
    int code = 0;
    bool saved_stoponerror = ctx->args.pdfstoponerror;
    stream_save local_entry_save;

    local_save_stream_state(ctx, &local_entry_save);
    initialise_stream_save(ctx);

    /* This causes several files to render 'incorrectly', even though they are in some sense
     * invalid. It doesn't seem to provide any benefits so I have, for now, removed it. If
     * there is a good reason for it we can put it back again.
     * FIXME - either restore or remove these lines
     * /tests_private/pdf/PDF_2.0_FTS/fts_23_2310.pdf
     * /tests_private/pdf/PDF_2.0_FTS/fts_23_2311.pdf
     * /tests_private/pdf/PDF_2.0_FTS/fts_23_2312.pdf
     * /tests_private/pdf/sumatra/recursive_colorspace.pdf
     * /tests_private/pdf/uploads/Bug696410.pdf
     * /tests_private/pdf/sumatra/1900_-_cairo_transparency_inefficiency.pdf (with pdfwrite)
     */
#if 0
    /* Stop on error in substream, and also be prepared to clean up the stack */
    if (stoponerror)
        ctx->args.pdfstoponerror = true;
#endif

#if DEBUG_CONTEXT
    dbgmprintf1(ctx->memory, "BEGIN %s stream\n", desc);
#endif
    code = pdfi_interpret_content_stream(ctx, content_stream, stream_obj, page_dict);
#if DEBUG_CONTEXT
    dbgmprintf1(ctx->memory, "END %s stream\n", desc);
#endif

    if (code < 0)
        dbgmprintf1(ctx->memory, "ERROR: inner_stream: code %d when rendering stream\n", code);

    ctx->args.pdfstoponerror = saved_stoponerror;

    /* Put our state back the way it was on entry */
#if PROBE_STREAMS
    if (ctx->pgs->level > ctx->current_stream_save.gsave_level ||
        pdfi_count_stack(ctx) > ctx->current_stream_save.stack_count)
        code = ((pdf_context *)0)->first_page;
#endif

    cleanup_context_interpretation(ctx, &local_entry_save);
    local_restore_stream_state(ctx, &local_entry_save);
    if (code < 0)
        code = pdfi_set_error_stop(ctx, code, NULL, 0, "pdfi_interpret_inner_content", NULL);
    return code;
}

/* Interpret inner content from a buffer
 */
int
pdfi_interpret_inner_content_buffer(pdf_context *ctx, byte *content_data,
                                      uint32_t content_length,
                                      pdf_dict *stream_dict, pdf_dict *page_dict,
                                      bool stoponerror, const char *desc)
{
    int code = 0;
    pdf_c_stream *stream = NULL;
    pdf_stream *stream_obj = NULL;

    if (content_length == 0)
        return 0;

    code = pdfi_open_memory_stream_from_memory(ctx, content_length,
                                               content_data, &stream, true);
    if (code < 0)
        goto exit;

    code = pdfi_obj_dict_to_stream(ctx, stream_dict, &stream_obj, false);
    if (code < 0)
        return code;

    /* NOTE: stream gets closed in here */
    code = pdfi_interpret_inner_content(ctx, stream, stream_obj, page_dict, stoponerror, desc);
    pdfi_countdown(stream_obj);
 exit:
    return code;
}

/* Interpret inner content from a C string
 */
int
pdfi_interpret_inner_content_c_string(pdf_context *ctx, char *content_string,
                                      pdf_dict *stream_dict, pdf_dict *page_dict,
                                      bool stoponerror, const char *desc)
{
    uint32_t length = (uint32_t)strlen(content_string);
    bool decrypt_strings;
    int code;

    if (length == 0)
        return 0;

    /* Underlying buffer limit is uint32, so handle the extremely unlikely case that
     * our string is too big.
     */
    if (length != strlen(content_string))
        return_error(gs_error_limitcheck);

    /* Since this is a constructed string content, not part of the file, it can never
     * be encrypted. So disable decryption during this call.
     */
    decrypt_strings = ctx->encryption.decrypt_strings;
    ctx->encryption.decrypt_strings = false;
    code = pdfi_interpret_inner_content_buffer(ctx, (byte *)content_string, length,
                                               stream_dict, page_dict, stoponerror, desc);
    ctx->encryption.decrypt_strings = decrypt_strings;

    return code;
}

/* Interpret inner content from a string
 */
int
pdfi_interpret_inner_content_string(pdf_context *ctx, pdf_string *content_string,
                                    pdf_dict *stream_dict, pdf_dict *page_dict,
                                    bool stoponerror, const char *desc)
{
    return pdfi_interpret_inner_content_buffer(ctx, content_string->data, content_string->length,
                                               stream_dict, page_dict, stoponerror, desc);
}

/* Interpret inner content from a stream_dict
 */
int
pdfi_interpret_inner_content_stream(pdf_context *ctx, pdf_stream *stream_obj,
                                    pdf_dict *page_dict, bool stoponerror, const char *desc)
{
    return pdfi_interpret_inner_content(ctx, NULL, stream_obj, page_dict, stoponerror, desc);
}

/*
 * Interpret a content stream.
 * content_stream -- content to parse.  If NULL, get it from the stream_dict
 * stream_dict -- dict containing the stream
 */
int
pdfi_interpret_content_stream(pdf_context *ctx, pdf_c_stream *content_stream,
                              pdf_stream *stream_obj, pdf_dict *page_dict)
{
    int code;
    pdf_c_stream *stream = NULL, *SubFile_stream = NULL;
    pdf_keyword *keyword;
    pdf_stream *s = ctx->current_stream;
    pdf_obj_type type;
    char EODString[] = "endstream";

    /* Check this stream, and all the streams currently being executed, to see
     * if the stream we've been given is already in train. If it is, then we
     * have encountered recursion. This can happen if a non-page stream such
     * as a Form or Pattern uses a Resource, but does not declare it in it's
     * Resources, and instead inherits it from the parent. We cannot detect that
     * before the Resource is used, so all we can do is check here.
     */
    while (s != NULL && pdfi_type_of(s) == PDF_STREAM) {
        if (s->object_num > 0) {
            if (s->object_num == stream_obj->object_num) {
                pdf_dict *d = NULL;
                bool known = false;

                code = pdfi_dict_from_obj(ctx, (pdf_obj *)stream_obj, &d);
                if (code >= 0) {
                    code = pdfi_dict_known(ctx, d, "Parent", &known);
                    if (code >= 0 && known)
                        (void)pdfi_dict_delete(ctx, d, "Parent");
                }
                pdfi_set_error(ctx, 0, NULL, E_PDF_CIRCULARREF, "pdfi_interpret_content_stream", "Aborting stream");
                return_error(gs_error_circular_reference);
            }
        }
        s = (pdf_stream *)s->parent_obj;
    }

    if (content_stream != NULL) {
        stream = content_stream;
    } else {
        code = pdfi_seek(ctx, ctx->main_stream, pdfi_stream_offset(ctx, stream_obj), SEEK_SET);
        if (code < 0)
            return code;

        if (stream_obj->length_valid) {
            if (stream_obj->Length == 0)
                return 0;
            code = pdfi_apply_SubFileDecode_filter(ctx, stream_obj->Length, NULL, ctx->main_stream, &SubFile_stream, false);
        }
        else
            code = pdfi_apply_SubFileDecode_filter(ctx, 0, EODString, ctx->main_stream, &SubFile_stream, false);
        if (code < 0)
            return code;

        code = pdfi_filter(ctx, stream_obj, SubFile_stream, &stream, false);
        if (code < 0) {
            pdfi_close_file(ctx, SubFile_stream);
            return code;
        }
    }

    pdfi_set_stream_parent(ctx, stream_obj, ctx->current_stream);
    ctx->current_stream = stream_obj;

    do {
        code = pdfi_read_token(ctx, stream, stream_obj->object_num, stream_obj->generation_num);
        if (code < 0) {
            if (code == gs_error_ioerror || code == gs_error_VMerror || ctx->args.pdfstoponerror) {
                if (code == gs_error_ioerror) {
                    pdfi_set_error(ctx, code, NULL, E_PDF_BADSTREAM, "pdfi_interpret_content_stream", (char *)"**** Error reading a content stream.  The page may be incomplete");
                } else if (code == gs_error_VMerror) {
                    pdfi_set_error(ctx, code, NULL, E_PDF_OUTOFMEMORY, "pdfi_interpret_content_stream", (char *)"**** Error ran out of memory reading a content stream.  The page may be incomplete");
                }
                goto exit;
            }
            continue;
        }

        if (pdfi_count_stack(ctx) <= 0) {
            if(stream->eof == true)
                break;
        }

repaired_keyword:
        type = pdfi_type_of(ctx->stack_top[-1]);
        if (type == PDF_FAST_KEYWORD) {
            keyword = (pdf_keyword *)ctx->stack_top[-1];

            switch((uintptr_t)keyword) {
                case TOKEN_ENDSTREAM:
                    pdfi_pop(ctx,1);
                    goto exit;
                    break;
                case TOKEN_ENDOBJ:
                    pdfi_clearstack(ctx);
                    code = pdfi_set_error_stop(ctx, gs_note_error(gs_error_syntaxerror), NULL, E_PDF_MISSINGENDSTREAM, "pdfi_interpret_content_stream", NULL);
                    goto exit;
                    break;
                case TOKEN_INVALID_KEY:
                    pdfi_clearstack(ctx);
                    if ((code = pdfi_set_error_stop(ctx, gs_note_error(gs_error_limitcheck), NULL, E_PDF_KEYWORDTOOLONG, "pdfi_interpret_content_stream", NULL)) < 0)
                        goto exit;
                    break;
                case TOKEN_TOO_LONG:
                    pdfi_clearstack(ctx);
                    if ((code = pdfi_set_error_stop(ctx, gs_note_error(gs_error_syntaxerror), NULL, E_PDF_MISSINGENDSTREAM, "pdfi_interpret_content_stream", NULL)) < 0)
                        goto exit;
                    break;
                default:
                    goto execute;
            }
        }
        else if (type == PDF_KEYWORD)
        {
execute:
            {
                pdf_dict *stream_dict = NULL;

                code = pdfi_dict_from_obj(ctx, (pdf_obj *)stream_obj, &stream_dict);
                if (code < 0)
                    goto exit;

                code = pdfi_interpret_stream_operator(ctx, stream, stream_dict, page_dict);
                if (code == REPAIRED_KEYWORD)
                    goto repaired_keyword;

                if (code < 0) {
                    if ((code = pdfi_set_error_stop(ctx, code, NULL, E_PDF_TOKENERROR, "pdf_interpret_content_stream", NULL)) < 0) {
                        pdfi_clearstack(ctx);
                        goto exit;
                    }
                }
            }
        }
        if(stream->eof == true)
            break;
    }while(1);

exit:
    ctx->current_stream = pdfi_stream_parent(ctx, stream_obj);
    pdfi_clear_stream_parent(ctx, stream_obj);
    pdfi_close_file(ctx, stream);
    if (SubFile_stream != NULL)
        pdfi_close_file(ctx, SubFile_stream);
    return code;
}
