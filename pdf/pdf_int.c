/* Copyright (C) 2018-2021 Artifex Software, Inc.
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
#include "pdf_font.h"
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

static bool ishex(char c)
{
    if (c < 0x30)
        return false;

    if (c > 0x39) {
        if (c > 'F') {
            if (c < 'a')
                return false;
            if (c > 'f')
                return false;
            return true;
        } else {
            if (c < 'A')
                return false;
            return true;
        }
    } else
        return true;
}

/* You must ensure the character is a hex character before calling this, no error trapping here */
static int fromhex(char c)
{
    if (c > 0x39) {
        if (c > 'F') {
            return c - 0x57;
        } else {
            return c - 0x37;
        }
    } else
        return c - 0x30;
}

/* The 'read' functions all return the newly created object on the context's stack
 * which means these objects are created with a reference count of 0, and only when
 * pushed onto the stack does the reference count become 1, indicating the stack is
 * the only reference.
 */
int pdfi_skip_white(pdf_context *ctx, pdf_c_stream *s)
{
    uint32_t read = 0;
    int32_t bytes = 0;
    byte c;

    do {
        bytes = pdfi_read_bytes(ctx, &c, 1, 1, s);
        if (bytes < 0)
            return_error(gs_error_ioerror);
        if (bytes == 0)
            return 0;
        read += bytes;
    } while (bytes != 0 && iswhite(c));

    if (read > 0)
        pdfi_unread(ctx, s, &c, 1);
    return 0;
}

int pdfi_skip_eol(pdf_context *ctx, pdf_c_stream *s)
{
    uint32_t read = 0;
    int32_t bytes = 0;
    byte c;

    do {
        bytes = pdfi_read_bytes(ctx, &c, 1, 1, s);
        if (bytes == 0)
            return 0;
        if (read) {
            if (c == 0x0A)
                return 0;
            pdfi_unread(ctx, s, &c, 1);
            return 0;
        }
        if (c == 0x0D)
            read++;
    } while (c != 0x0A);
    return 0;
}

static int pdfi_read_num(pdf_context *ctx, pdf_c_stream *s, uint32_t indirect_num, uint32_t indirect_gen)
{
    byte Buffer[256];
    char Max[256];
    int MaxLen = 0;
    unsigned short index = 0;
    short bytes;
    bool real = false;
    bool has_decimal_point = false;
    bool has_exponent = false;
    unsigned short exponent_index = 0;
    pdf_num *num;
    int code = 0, malformed = false, doubleneg = false, recovered = false;

    pdfi_skip_white(ctx, s);

    do {
        bytes = pdfi_read_bytes(ctx, (byte *)&Buffer[index], 1, 1, s);
        if (bytes == 0 && s->eof) {
            Buffer[index] = 0x00;
            break;
        }

        if (bytes <= 0)
            return_error(gs_error_ioerror);

        if (iswhite((char)Buffer[index])) {
            Buffer[index] = 0x00;
            break;
        } else {
            if (isdelimiter((char)Buffer[index])) {
                pdfi_unread(ctx, s, (byte *)&Buffer[index], 1);
                Buffer[index] = 0x00;
                break;
            }
        }
        if (Buffer[index] == '.') {
            if (has_decimal_point == true) {
                if (ctx->args.pdfstoponerror)
                    return_error(gs_error_syntaxerror);
                malformed = true;
            } else {
                has_decimal_point = true;
                real = true;
            }
        } else if (Buffer[index] == 'e' || Buffer[index] == 'E') {
            /* TODO: technically scientific notation isn't in PDF spec,
             * but gs seems to accept it, so we should also?
             */
            if (has_exponent == true) {
                if (ctx->args.pdfstoponerror)
                    return_error(gs_error_syntaxerror);
                malformed = true;
            } else {
                pdfi_set_warning(ctx, 0, NULL, W_PDF_NUM_EXPONENT, "pdfi_read_num", NULL);
                has_exponent = true;
                exponent_index = index;
                real = true;
            }
        } else if (Buffer[index] == '-' || Buffer[index] == '+') {
            if (!(index == 0 || (has_exponent && index == exponent_index+1))) {
                if (ctx->args.pdfstoponerror)
                    return_error(gs_error_syntaxerror);
                /* Acrobat weirdness. We need to know if a number starts with two - signs
                 * because Acrobat treats real and integers defined this way differently!
                 * Double-negated integers are treated as 0, and reals are treated as if
                 * they had one negative sign. We can't tell whether the number is a real
                 * or not yet, we do that below.
                 */
                pdfi_set_error(ctx, 0, NULL, E_PDF_MALFORMEDNUMBER, "pdfi_read_num", NULL);
                if (Buffer[index - 1] == '-') {
                    doubleneg = true;
                    index -= 1;
                }
                else {
                    malformed = true;
                    Buffer[index] = 0x00;
                    recovered = true;
                }
            }
        } else if (Buffer[index] < 0x30 || Buffer[index] > 0x39) {
            pdfi_set_error(ctx, 0, NULL, E_PDF_MISSINGWHITESPACE, "pdfi_read_num", (char *)"Ignoring missing white space while parsing number");
            if (ctx->args.pdfstoponerror)
                return_error(gs_error_syntaxerror);
            pdfi_unread(ctx, s, (byte *)&Buffer[index], 1);
            Buffer[index] = 0x00;
            break;
        }
        if (++index > 255)
            return_error(gs_error_syntaxerror);
    } while(1);

    if (!real && index > 7) {
        /* Check for integer overflow, represent as real if so */
        gs_sprintf(Max, "%d", (max_uint >> 1));
        MaxLen = strlen((const char *)Max);

        if (index >= MaxLen) {
            int j = 0;

            if (Buffer[j] == '-')
                j++;

            while (Buffer[j] == '0')
                j++;

            if (index - j > MaxLen)
                real = true;

            if (index - j == MaxLen)
            {
                real = false;

                for (;j< MaxLen; j++) {
                    if (Buffer[j] == Max[j])
                        continue;
                    if (Buffer[j] > Max[j])
                        real=true;
                    break;
                }
            }
        }
    }

    if (real && (!malformed || (malformed && recovered)))
        code = pdfi_object_alloc(ctx, PDF_REAL, 0, (pdf_obj **)&num);
    else
        code = pdfi_object_alloc(ctx, PDF_INT, 0, (pdf_obj **)&num);
    if (code < 0)
        return code;

    if ((malformed && !recovered) || (!real && doubleneg)) {
        char extra_info[gp_file_name_sizeof];

        gs_sprintf(extra_info, "Treating malformed number %s as 0", Buffer);
        pdfi_set_error(ctx, 0, NULL, E_PDF_MALFORMEDNUMBER, "pdfi_read_num", extra_info);
        num->value.i = 0;
    } else {
        if (real) {
            float tempf;
            char *dot = NULL, mfloat[256];   /* We limit numbers to 255 above so it can't exceed this */

            /* Check for overflow */
            gs_sprintf(mfloat, "%f", MAX_FLOAT);
            dot = strstr(mfloat, ".");
            *dot = 0x00;

            dot = strstr((char *)Buffer, ".");
            if (dot != NULL)
                *dot = 0x00;
            if (strlen((char *)Buffer) > strlen(mfloat)) {
                if (dot != NULL)
                    *dot = '.';
                if (ctx->args.pdfdebug)
                    dmprintf1(ctx->memory, "overflow reading real number : %s\n", Buffer);
                pdfi_set_warning(ctx, 0, NULL, W_PDF_OVERFLOW_REAL, "pdfi_read_num", NULL);
                num->value.d = 0.0;
            }
            else {
                if (strlen((char *)Buffer) == strlen(mfloat)) {
                    int fi = 0;

                    for (fi = 0;fi < strlen((const char *)Buffer);fi++) {
                        if (Buffer[fi] > mfloat[fi]) {
                            if (dot != NULL)
                                *dot = '.';
                            if (ctx->args.pdfdebug)
                                dmprintf1(ctx->memory, "overflow reading real number : %s\n", Buffer);
                            pdfi_set_warning(ctx, 0, NULL, W_PDF_OVERFLOW_REAL, "pdfi_read_num", NULL);
                            num->value.d = 0.0;
                        }
                    }
                }
                if (dot != NULL)
                    *dot = '.';
                if (sscanf((const char *)Buffer, "%f", &tempf) == 0) {
                    if (ctx->args.pdfdebug)
                        dmprintf1(ctx->memory, "failed to read real number : %s\n", Buffer);
                    pdfi_set_warning(ctx, 0, NULL, W_PDF_INVALID_REAL, "pdfi_read_num", NULL);
                    num->value.d = 0.0;
                }
                num->value.d = tempf;
            }
        } else {
            int tempi;
            if (sscanf((const char *)Buffer, "%d", &tempi) == 0) {
                if (ctx->args.pdfdebug)
                    dmprintf1(ctx->memory, "failed to read integer : %s\n", Buffer);
                gs_free_object(OBJ_MEMORY(num), num, "pdfi_read_num error");
                return_error(gs_error_syntaxerror);
            }
            num->value.i = tempi;
        }
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
        bytes = pdfi_read_bytes(ctx, (byte *)&Buffer[index], 1, 1, s);
        if (bytes == 0 && s->eof)
            break;
        if (bytes <= 0)
            return_error(gs_error_ioerror);

        if (iswhite((char)Buffer[index])) {
            Buffer[index] = 0x00;
            break;
        } else {
            if (isdelimiter((char)Buffer[index])) {
                pdfi_unread(ctx, s, (byte *)&Buffer[index], 1);
                Buffer[index] = 0x00;
                break;
            }
        }

        /* Check for and convert escaped name characters */
        if (Buffer[index] == '#') {
            byte NumBuf[2];

            bytes = pdfi_read_bytes(ctx, (byte *)&NumBuf, 1, 2, s);
            if (bytes < 2 || (!ishex(NumBuf[0]) || !ishex(NumBuf[1]))) {
                pdfi_set_warning(ctx, 0, NULL, W_PDF_BAD_NAME_ESCAPE, "pdfi_read_name", NULL);
                pdfi_unread(ctx, s, (byte *)NumBuf, bytes);
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
    char *Buffer, *NewBuf = NULL, HexBuf[2];
    unsigned short index = 0;
    short bytes = 0;
    uint32_t size = 256;
    pdf_string *string = NULL;
    int code;

    Buffer = (char *)gs_alloc_bytes(ctx->memory, size, "pdfi_read_hexstring");
    if (Buffer == NULL)
        return_error(gs_error_VMerror);

    if (ctx->args.pdfdebug)
        dmprintf(ctx->memory, " <");

    do {
        do {
            bytes = pdfi_read_bytes(ctx, (byte *)HexBuf, 1, 1, s);
            if (bytes == 0 && s->eof)
                break;
            if (bytes <= 0) {
                code = gs_note_error(gs_error_ioerror);
                goto exit;
            }
        } while(iswhite(HexBuf[0]));
        if (bytes == 0 && s->eof)
            break;

        if (HexBuf[0] == '>')
            break;

        if (ctx->args.pdfdebug)
            dmprintf1(ctx->memory, "%c", HexBuf[0]);

        do {
            bytes = pdfi_read_bytes(ctx, (byte *)&HexBuf[1], 1, 1, s);
            if (bytes == 0 && s->eof)
                break;
            if (bytes <= 0) {
                code = gs_note_error(gs_error_ioerror);
                goto exit;
            }
        } while(iswhite(HexBuf[1]));
        if (bytes == 0 && s->eof)
            break;

        if (!ishex(HexBuf[0]) || !ishex(HexBuf[1])) {
            code = gs_note_error(gs_error_syntaxerror);
            goto exit;
        }

        if (ctx->args.pdfdebug)
            dmprintf1(ctx->memory, "%c", HexBuf[1]);

        Buffer[index] = (fromhex(HexBuf[0]) << 4) + fromhex(HexBuf[1]);

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
    char *Buffer, *NewBuf = NULL, octal[3];
    unsigned short index = 0;
    short bytes = 0;
    uint32_t size = 256;
    pdf_string *string = NULL;
    int code, octal_index = 0, nesting = 0;
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

        bytes = pdfi_read_bytes(ctx, (byte *)&Buffer[index], 1, 1, s);

        if (bytes == 0 && s->eof)
            break;
        if (bytes <= 0) {
            Buffer[index] = 0x00;
            break;
        }

        if (skip_eol) {
            if (Buffer[index] == 0x0a || Buffer[index] == 0x0d)
                continue;
            skip_eol = false;
        }

        if (escape) {
            escape = false;
            if (Buffer[index] == 0x0a || Buffer[index] == 0x0d) {
                skip_eol = true;
                continue;
            }
            if (octal_index) {
                byte dummy[2];
                dummy[0] = '\\';
                dummy[1] = Buffer[index];
                code = pdfi_unread(ctx, s, dummy, 2);
                if (code < 0) {
                    gs_free_object(ctx->memory, Buffer, "pdfi_read_string");
                    return code;
                }
                Buffer[index] = octal[0];
                if (octal_index == 2)
                    Buffer[index] = (Buffer[index] * 8) + octal[1];
                octal_index = 0;
            } else {
                switch (Buffer[index]) {
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
                    default:
                        if (Buffer[index] >= 0x30 && Buffer[index] <= 0x37) {
                            octal[octal_index] = Buffer[index] - 0x30;
                            octal_index++;
                            continue;
                        }
                        /* PDF Reference, literal strings, if the character following a
                         * escape \ character is not recognised, then it is ignored.
                         */
                        escape = false;
                        index++;
                        continue;
                }
            }
        } else {
            switch(Buffer[index]) {
                case 0x0a:
                case 0x0d:
                    if (octal_index != 0) {
                        code = pdfi_unread(ctx, s, (byte *)&Buffer[index], 1);
                        if (code < 0) {
                            gs_free_object(ctx->memory, Buffer, "pdfi_read_string");
                            return code;
                        }
                        Buffer[index] = octal[0];
                        if (octal_index == 2)
                            Buffer[index] = (Buffer[index] * 8) + octal[1];
                        octal_index = 0;
                    } else {
                        Buffer[index] = 0x0a;
                        skip_eol = true;
                    }
                    break;
                case ')':
                    if (octal_index != 0) {
                        code = pdfi_unread(ctx, s, (byte *)&Buffer[index], 1);
                        if (code < 0) {
                            gs_free_object(ctx->memory, Buffer, "pdfi_read_string");
                            return code;
                        }
                        Buffer[index] = octal[0];
                        if (octal_index == 2)
                            Buffer[index] = (Buffer[index] * 8) + octal[1];
                        octal_index = 0;
                    } else {
                        if (nesting == 0) {
                            Buffer[index] = 0x00;
                            exit_loop = true;
                        } else
                            nesting--;
                    }
                    break;
                case '\\':
                    escape = true;
                    continue;
                case '(':
                    pdfi_set_error(ctx, 0, NULL, E_PDF_UNESCAPEDSTRING, "pdfi_read_string", NULL);
                    nesting++;
                    break;
                default:
                    if (octal_index) {
                        if (Buffer[index] >= 0x30 && Buffer[index] <= 0x37) {
                            octal[octal_index] = Buffer[index] - 0x30;
                            if (++octal_index < 3)
                                continue;
                            Buffer[index] = (octal[0] * 64) + (octal[1] * 8) + octal[2];
                            octal_index = 0;
                        } else {
                            code = pdfi_unread(ctx, s, (byte *)&Buffer[index], 1);
                            if (code < 0) {
                                gs_free_object(ctx->memory, Buffer, "pdfi_read_string");
                                return code;
                            }
                            Buffer[index] = octal[0];
                            if (octal_index == 2)
                                Buffer[index] = (Buffer[index] * 8) + octal[1];
                            octal_index = 0;
                        }
                    }
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
    if (ctx->stack_top[-1]->type != PDF_DICT_MARK)
        return_error(gs_error_typecheck);
    depth = pdfi_count_stack(ctx);

    do {
        code = pdfi_read_token(ctx, s, indirect_num, indirect_gen);
        if (code < 0)
            return code;
    } while(pdfi_count_stack(ctx) > depth);
    return 0;
}

int pdfi_skip_comment(pdf_context *ctx, pdf_c_stream *s)
{
    byte Buffer;
    short bytes = 0;

    if (ctx->args.pdfdebug)
        dmprintf (ctx->memory, " %%");

    do {
        bytes = pdfi_read_bytes(ctx, (byte *)&Buffer, 1, 1, s);
        if (bytes < 0)
            return_error(gs_error_ioerror);

        if (bytes > 0) {
            if (ctx->args.pdfdebug)
                dmprintf1 (ctx->memory, " %c", Buffer);

            if ((Buffer == 0x0A) || (Buffer == 0x0D)) {
                break;
            }
        }
    } while (bytes);
    return 0;
}

/* This function is slightly misnamed, for some keywords we do
 * indeed read the keyword and return a PDF_KEYWORD object, but
 * for null, true, false and R we create an appropriate object
 * of that type (PDF_NULL, PDF_BOOL or PDF_INDIRECT_REF)
 * and return it instead.
 */
static int pdfi_read_keyword(pdf_context *ctx, pdf_c_stream *s, uint32_t indirect_num, uint32_t indirect_gen)
{
    byte Buffer[256];
    unsigned short index = 0;
    short bytes = 0;
    int code;
    pdf_keyword *keyword;

    pdfi_skip_white(ctx, s);

    do {
        bytes = pdfi_read_bytes(ctx, (byte *)&Buffer[index], 1, 1, s);
        if (bytes < 0)
            return_error(gs_error_ioerror);

        if (bytes > 0) {
            if (iswhite(Buffer[index])) {
                pdfi_unread(ctx, s, (byte *)&Buffer[index], 1);
                break;
            } else {
                if (isdelimiter(Buffer[index])) {
                    pdfi_unread(ctx, s, (byte *)&Buffer[index], 1);
                    break;
                }
            }
            index++;
        }
    } while (bytes && index < 255);

    if (index >= 255 || index == 0) {
        if (ctx->args.pdfstoponerror)
            return_error(gs_error_syntaxerror);
        strcpy((char *)Buffer, "KEYWORD_TOO_LONG");
        index = 16;
    }

    /* NB The code below uses 'Buffer', not the data stored in keyword->data to compare strings */
    Buffer[index] = 0x00;

    code = pdfi_object_alloc(ctx, PDF_KEYWORD, index, (pdf_obj **)&keyword);
    if (code < 0)
        return code;

    memcpy(keyword->data, Buffer, index);
    pdfi_countup(keyword);

    keyword->indirect_num = indirect_num;
    keyword->indirect_gen = indirect_gen;

    if (ctx->args.pdfdebug)
        dmprintf1(ctx->memory, " %s\n", Buffer);

    switch(Buffer[0]) {
        case 'K':
            if (keyword->length == 16 && memcmp(keyword->data, "KEYWORD_TOO_LONG", 16) == 0) {
                keyword->key = TOKEN_INVALID_KEY;
            }
            break;
        case 'R':
            if (keyword->length == 1){
                pdf_indirect_ref *o;
                uint64_t obj_num;
                uint32_t gen_num;

                pdfi_countdown(keyword);

                if(pdfi_count_stack(ctx) < 2) {
                    pdfi_clearstack(ctx);
                    return_error(gs_error_stackunderflow);
                }

                if(((pdf_obj *)ctx->stack_top[-1])->type != PDF_INT || ((pdf_obj *)ctx->stack_top[-2])->type != PDF_INT) {
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
            break;
        case 'e':
            if (keyword->length == 9 && memcmp((const char *)Buffer, "endstream", 9) == 0)
                keyword->key = TOKEN_ENDSTREAM;
            else {
                if (keyword->length == 6 && memcmp((const char *)Buffer, "endobj", 6) == 0)
                    keyword->key = TOKEN_ENDOBJ;
            }
            break;
        case 'o':
            if (keyword->length == 3 && memcmp((const char *)Buffer, "obj", 3) == 0)
                keyword->key = TOKEN_OBJ;
            break;
        case 's':
            if (keyword->length == 6 && memcmp((const char *)Buffer, "stream", 6) == 0){
                keyword->key = TOKEN_STREAM;
                code = pdfi_skip_eol(ctx, s);
                if (code < 0) {
                    pdfi_countdown(keyword);
                    return code;
                }
            }
            else {
                if (keyword->length == 9 && memcmp((const char *)Buffer, "startxref", 9) == 0)
                    keyword->key = TOKEN_STARTXREF;
            }
            break;
        case 't':
            if (keyword->length == 4 && memcmp((const char *)Buffer, "true", 4) == 0) {
                pdf_bool *o;

                pdfi_countdown(keyword);

                code = pdfi_object_alloc(ctx, PDF_BOOL, 0, (pdf_obj **)&o);
                if (code < 0)
                    return code;

                o->value = true;
                o->indirect_num = indirect_num;
                o->indirect_gen = indirect_gen;

                code = pdfi_push(ctx, (pdf_obj *)o);
                if (code < 0)
                    pdfi_free_object((pdf_obj *)o);
                return code;
            }
            else {
                if (keyword->length == 7 && memcmp((const char *)Buffer, "trailer", 7) == 0)
                    keyword->key = TOKEN_TRAILER;
            }
            break;
        case 'f':
            if (keyword->length == 5 && memcmp((const char *)Buffer, "false", 5) == 0)
            {
                pdf_bool *o;

                pdfi_countdown(keyword);

                code = pdfi_object_alloc(ctx, PDF_BOOL, 0, (pdf_obj **)&o);
                if (code < 0)
                    return code;

                o->value = false;
                o->indirect_num = indirect_num;
                o->indirect_gen = indirect_gen;

                code = pdfi_push(ctx, (pdf_obj *)o);
                if (code < 0)
                    pdfi_free_object((pdf_obj *)o);
                return code;
            }
            break;
        case 'n':
            if (keyword->length == 4 && memcmp((const char *)Buffer, "null", 4) == 0){
                pdf_obj *o;

                pdfi_countdown(keyword);

                code = pdfi_object_alloc(ctx, PDF_NULL, 0, &o);
                if (code < 0)
                    return code;
                o->indirect_num = indirect_num;
                o->indirect_gen = indirect_gen;

                code = pdfi_push(ctx, o);
                if (code < 0)
                    pdfi_free_object((pdf_obj *)o);
                return code;
            }
            break;
        case 'x':
            if (keyword->length == 4 && memcmp((const char *)Buffer, "xref", 4) == 0)
                keyword->key = TOKEN_XREF;
            break;
    }

    code = pdfi_push(ctx, (pdf_obj *)keyword);
    pdfi_countdown(keyword);

    return code;
}

/* This function reads from the given stream, at the current offset in the stream,
 * a single PDF 'token' and returns it on the stack.
 */
int pdfi_read_token(pdf_context *ctx, pdf_c_stream *s, uint32_t indirect_num, uint32_t indirect_gen)
{
    int32_t bytes = 0;
    char Buffer[256];
    int code;

    pdfi_skip_white(ctx, s);

    bytes = pdfi_read_bytes(ctx, (byte *)Buffer, 1, 1, s);
    if (bytes < 0)
        return (gs_error_ioerror);
    if (bytes == 0 && s->eof)
        return 0;

    switch(Buffer[0]) {
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
            pdfi_unread(ctx, s, (byte *)&Buffer[0], 1);
            code = pdfi_read_num(ctx, s, indirect_num, indirect_gen);
            if (code < 0)
                return code;
            break;
        case '/':
            return pdfi_read_name(ctx, s, indirect_num, indirect_gen);
            break;
        case '<':
            bytes = pdfi_read_bytes(ctx, (byte *)&Buffer[1], 1, 1, s);
            if (bytes <= 0)
                return (gs_error_ioerror);
            if (iswhite(Buffer[1])) {
                code = pdfi_skip_white(ctx, s);
                if (code < 0)
                    return code;
                bytes = pdfi_read_bytes(ctx, (byte *)&Buffer[1], 1, 1, s);
            }
            if (Buffer[1] == '<') {
                if (ctx->args.pdfdebug)
                    dmprintf (ctx->memory, " <<\n");
                return pdfi_mark_stack(ctx, PDF_DICT_MARK);
            } else {
                if (Buffer[1] == '>') {
                    pdfi_unread(ctx, s, (byte *)&Buffer[1], 1);
                    return pdfi_read_hexstring(ctx, s, indirect_num, indirect_gen);
                } else {
                    if (ishex(Buffer[1])) {
                        pdfi_unread(ctx, s, (byte *)&Buffer[1], 1);
                        return pdfi_read_hexstring(ctx, s, indirect_num, indirect_gen);
                    }
                    else
                        return_error(gs_error_syntaxerror);
                }
            }
            break;
        case '>':
            bytes = pdfi_read_bytes(ctx, (byte *)&Buffer[1], 1, 1, s);
            if (bytes <= 0)
                return (gs_error_ioerror);
            if (Buffer[1] == '>')
                return pdfi_dict_from_stack(ctx, indirect_num, indirect_gen);
            else {
                pdfi_unread(ctx, s, (byte *)&Buffer[1], 1);
                return_error(gs_error_syntaxerror);
            }
            break;
        case '(':
            return pdfi_read_string(ctx, s, indirect_num, indirect_gen);
            break;
        case '[':
            if (ctx->args.pdfdebug)
                dmprintf (ctx->memory, "[");
            return pdfi_mark_stack(ctx, PDF_ARRAY_MARK);
            break;
        case ']':
            code = pdfi_array_from_stack(ctx, indirect_num, indirect_gen);
            if (code < 0) {
                if (code == gs_error_VMerror || code == gs_error_ioerror || ctx->args.pdfstoponerror)
                    return code;
                pdfi_clearstack(ctx);
                return pdfi_read_token(ctx, s, indirect_num, indirect_gen);
            }
            break;
        case '{':
            if (ctx->args.pdfdebug)
                dmprintf (ctx->memory, "{");
            return pdfi_mark_stack(ctx, PDF_PROC_MARK);
            break;
        case '}':
            pdfi_clear_to_mark(ctx);
            return pdfi_read_token(ctx, s, indirect_num, indirect_gen);
            break;
        case '%':
            pdfi_skip_comment(ctx, s);
            return pdfi_read_token(ctx, s, indirect_num, indirect_gen);
            break;
        default:
            if (isdelimiter(Buffer[0])) {
                if (ctx->args.pdfstoponerror)
                    return_error(gs_error_syntaxerror);
                return pdfi_read_token(ctx, s, indirect_num, indirect_gen);
            }
            pdfi_unread(ctx, s, (byte *)&Buffer[0], 1);
            return pdfi_read_keyword(ctx, s, indirect_num, indirect_gen);
            break;
    }
    return 0;
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

static int search_table_3(pdf_context *ctx, unsigned char *str, pdf_keyword **key)
{
    int i, code = 0;

    for (i = 0; i < 5; i++) {
        if (memcmp(str, op_table_3[i], 3) == 0) {
            code = pdfi_object_alloc(ctx, PDF_KEYWORD, 3, (pdf_obj **)key);
            if (code < 0)
                return code;
            memcpy((*key)->data, str, 3);
            (*key)->key = TOKEN_NOT_A_KEYWORD;
            pdfi_countup(*key);
            return 1;
        }
    }
    return 0;
}

static int search_table_2(pdf_context *ctx, unsigned char *str, pdf_keyword **key)
{
    int i, code = 0;

    for (i = 0; i < 39; i++) {
        if (memcmp(str, op_table_2[i], 2) == 0) {
            code = pdfi_object_alloc(ctx, PDF_KEYWORD, 2, (pdf_obj **)key);
            if (code < 0)
                return code;
            memcpy((*key)->data, str, 2);
            (*key)->key = TOKEN_NOT_A_KEYWORD;
            pdfi_countup(*key);
            return 1;
        }
    }
    return 0;
}

static int search_table_1(pdf_context *ctx, unsigned char *str, pdf_keyword **key)
{
    int i, code = 0;

    for (i = 0; i < 39; i++) {
        if (memcmp(str, op_table_1[i], 1) == 0) {
            code = pdfi_object_alloc(ctx, PDF_KEYWORD, 1, (pdf_obj **)key);
            if (code < 0)
                return code;
            memcpy((*key)->data, str, 1);
            (*key)->key = TOKEN_NOT_A_KEYWORD;
            pdfi_countup(*key);
            return 1;
        }
    }
    return 0;
}

static int split_bogus_operator(pdf_context *ctx, pdf_c_stream *source, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    int code = 0;
    pdf_keyword *keyword = (pdf_keyword *)ctx->stack_top[-1], *key1 = NULL, *key2 = NULL;

    if (keyword->length > 6) {
        /* Longer than 2 3-character operators, we only allow for up to two
         * operators. Check to see if it includes an endstream or endobj.
         */
        if (memcmp(&keyword->data[keyword->length - 6], "endobj", 6) == 0) {
            code = pdfi_object_alloc(ctx, PDF_KEYWORD, keyword->length - 6, (pdf_obj **)&key1);
            if (code < 0)
                goto error_exit;
            memcpy(key1->data, keyword->data, key1->length);
            pdfi_pop(ctx, 1);
            pdfi_push(ctx, (pdf_obj *)key1);
            code = pdfi_interpret_stream_operator(ctx, source, stream_dict, page_dict);
            if (code < 0)
                goto error_exit;
            code = pdfi_object_alloc(ctx, PDF_KEYWORD, 6, (pdf_obj **)&key1);
            if (code < 0)
                goto error_exit;
            memcpy(key1->data, "endobj", 6);
            key1->key = TOKEN_ENDOBJ;
            pdfi_push(ctx, (pdf_obj *)key1);
            return 0;
        } else {
            if (keyword->length > 9 && memcmp(&keyword->data[keyword->length - 9], "endstream", 9) == 0) {
                code = pdfi_object_alloc(ctx, PDF_KEYWORD, keyword->length - 9, (pdf_obj **)&key1);
                if (code < 0)
                    goto error_exit;
                memcpy(key1->data, keyword->data, key1->length);
                pdfi_pop(ctx, 1);
                pdfi_push(ctx, (pdf_obj *)key1);
                code = pdfi_interpret_stream_operator(ctx, source, stream_dict, page_dict);
                if (code < 0)
                    goto error_exit;
                code = pdfi_object_alloc(ctx, PDF_KEYWORD, 9, (pdf_obj **)&key1);
                if (code < 0)
                    goto error_exit;
                memcpy(key1->data, "endstream", 9);
                key1->key = TOKEN_ENDSTREAM;
                pdfi_push(ctx, (pdf_obj *)key1);
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
            switch(keyword->length - 3) {
                case 1:
                    code = search_table_1(ctx, &keyword->data[key1->length], &key2);
                    break;
                case 2:
                    code = search_table_1(ctx, &keyword->data[key1->length], &key2);
                    break;
                case 3:
                    code = search_table_1(ctx, &keyword->data[key1->length], &key2);
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
                code = search_table_1(ctx, &keyword->data[key1->length], &key2);
                break;
            case 2:
                code = search_table_1(ctx, &keyword->data[key1->length], &key2);
                break;
            case 3:
                code = search_table_1(ctx, &keyword->data[key1->length], &key2);
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

    if (code > 0) {
        switch(keyword->length - 1) {
            case 1:
                code = search_table_1(ctx, &keyword->data[key1->length], &key2);
                break;
            case 2:
                code = search_table_1(ctx, &keyword->data[key1->length], &key2);
                break;
            case 3:
                code = search_table_1(ctx, &keyword->data[key1->length], &key2);
                break;
            default:
                goto error_exit;
        }
        if (code <= 0)
            goto error_exit;
        if (code > 0)
            goto match;
    }
    pdfi_countdown(key1);
    pdfi_countdown(key2);
    key1 = NULL;
    key2 = NULL;

match:
    /* If we get here, we have two PDF_KEYWORD objects. We push them on the stack
     * one at a time, and execute them.
     */
    pdfi_push(ctx, (pdf_obj *)key1);
    code = pdfi_interpret_stream_operator(ctx, source, stream_dict, page_dict);
    if (code < 0)
        goto error_exit;

    pdfi_push(ctx, (pdf_obj *)key2);
    code = pdfi_interpret_stream_operator(ctx, source, stream_dict, page_dict);

error_exit:
    pdfi_set_error(ctx, 0, NULL, E_PDF_TOKENERROR, "split_bogus_operator", NULL);
    pdfi_countdown(key1);
    pdfi_countdown(key2);
    pdfi_clearstack(ctx);
    return code;
}

#define K1(a) (a)
#define K2(a, b) ((a << 8) + b)
#define K3(a, b, c) ((a << 16) + (b << 8) + c)

static int pdfi_interpret_stream_operator(pdf_context *ctx, pdf_c_stream *source,
                                          pdf_dict *stream_dict, pdf_dict *page_dict)
{
    pdf_keyword *keyword = (pdf_keyword *)ctx->stack_top[-1];
    uint32_t op = 0;
    int i, code = 0;

    if (keyword->length > 3) {
        /* This means we either have a corrupted or illegal operator. The most
         * usual corruption is two concatented operators (eg QBT instead of Q BT)
         * I plan to tackle this by trying to see if I can make two or more operators
         * out of the mangled one. Note this will also be done below in the 'default'
         * case where we don't recognise a keyword with 3 or fewer characters.
         */
        code = split_bogus_operator(ctx, source, stream_dict, page_dict);
        if (code < 0)
            return code;
        if (pdfi_count_stack(ctx) > 0) {
            keyword = (pdf_keyword *)ctx->stack_top[-1];
            if (keyword->key != TOKEN_NOT_A_KEYWORD)
                return REPAIRED_KEYWORD;
        } else
            return 0;
    } else {
        for (i=0;i < keyword->length;i++) {
            op = (op << 8) + keyword->data[i];
        }
        switch(op) {
            case K1('b'):           /* closepath, fill, stroke */
                pdfi_pop(ctx, 1);
                code = pdfi_b(ctx);
                break;
            case K1('B'):           /* fill, stroke */
                pdfi_pop(ctx, 1);
                code = pdfi_B(ctx);
                break;
            case K2('b','*'):       /* closepath, eofill, stroke */
                pdfi_pop(ctx, 1);
                code = pdfi_b_star(ctx);
                break;
            case K2('B','*'):       /* eofill, stroke */
                pdfi_pop(ctx, 1);
                code = pdfi_B_star(ctx);
                break;
            case K2('B','I'):       /* begin inline image */
                pdfi_pop(ctx, 1);
                code = pdfi_BI(ctx);
                break;
            case K3('B','D','C'):   /* begin marked content sequence with property list */
                pdfi_pop(ctx, 1);
                code = pdfi_op_BDC(ctx, stream_dict, page_dict);
                break;
            case K3('B','M','C'):   /* begin marked content sequence */
                pdfi_pop(ctx, 1);
                code = pdfi_op_BMC(ctx);
                break;
            case K2('B','T'):       /* begin text */
                pdfi_pop(ctx, 1);
                code = pdfi_BT(ctx);
                break;
            case K2('B','X'):       /* begin compatibility section */
                pdfi_pop(ctx, 1);
                break;
            case K1('c'):           /* curveto */
                pdfi_pop(ctx, 1);
                code = pdfi_curveto(ctx);
                break;
            case K2('c','m'):       /* concat */
                pdfi_pop(ctx, 1);
                code = pdfi_concat(ctx);
                break;
            case K2('C','S'):       /* set stroke colour space */
                pdfi_pop(ctx, 1);
                code = pdfi_setstrokecolor_space(ctx, stream_dict, page_dict);
                break;
            case K2('c','s'):       /* set non-stroke colour space */
                pdfi_pop(ctx, 1);
                code = pdfi_setfillcolor_space(ctx, stream_dict, page_dict);
                break;
                break;
            case K1('d'):           /* set dash params */
                pdfi_pop(ctx, 1);
                code = pdfi_setdash(ctx);
                break;
            case K2('E','I'):       /* end inline image */
                pdfi_pop(ctx, 1);
                code = pdfi_EI(ctx);
                break;
            case K2('d','0'):       /* set type 3 font glyph width */
                pdfi_pop(ctx, 1);
                code = pdfi_d0(ctx);
                break;
            case K2('d','1'):       /* set type 3 font glyph width and bounding box */
                pdfi_pop(ctx, 1);
                code = pdfi_d1(ctx);
                break;
            case K2('D','o'):       /* invoke named XObject */
                pdfi_pop(ctx, 1);
                code = pdfi_Do(ctx, stream_dict, page_dict);
                break;
            case K2('D','P'):       /* define marked content point with property list */
                pdfi_pop(ctx, 1);
                if (pdfi_count_stack(ctx) >= 2) {
                    pdfi_pop(ctx, 2);
                } else
                    pdfi_clearstack(ctx);
                break;
            case K2('E','T'):       /* end text */
                pdfi_pop(ctx, 1);
                code = pdfi_ET(ctx);
                break;
            case K3('E','M','C'):   /* end marked content sequence */
                pdfi_pop(ctx, 1);
                code = pdfi_op_EMC(ctx);
                break;
            case K2('E','X'):       /* end compatibility section */
                pdfi_pop(ctx, 1);
                break;
            case K1('f'):           /* fill */
                pdfi_pop(ctx, 1);
                code = pdfi_fill(ctx);
                break;
            case K1('F'):           /* fill (obselete operator) */
                pdfi_pop(ctx, 1);
                code = pdfi_fill(ctx);
                break;
            case K2('f','*'):       /* eofill */
                pdfi_pop(ctx, 1);
                code = pdfi_eofill(ctx);
                break;
            case K1('G'):           /* setgray for stroke */
                pdfi_pop(ctx, 1);
                code = pdfi_setgraystroke(ctx);
                break;
            case K1('g'):           /* setgray for non-stroke */
                pdfi_pop(ctx, 1);
                code = pdfi_setgrayfill(ctx);
                break;
            case K2('g','s'):       /* set graphics state from dictionary */
                pdfi_pop(ctx, 1);
                code = pdfi_setgstate(ctx, stream_dict, page_dict);
                break;
            case K1('h'):           /* closepath */
                pdfi_pop(ctx, 1);
                code = pdfi_closepath(ctx);
                break;
            case K1('i'):           /* setflat */
                pdfi_pop(ctx, 1);
                code = pdfi_setflat(ctx);
                break;
            case K2('I','D'):       /* begin inline image data */
                pdfi_pop(ctx, 1);
                code = pdfi_ID(ctx, stream_dict, page_dict, source);
                break;
            case K1('j'):           /* setlinejoin */
                pdfi_pop(ctx, 1);
                code = pdfi_setlinejoin(ctx);
                break;
            case K1('J'):           /* setlinecap */
                pdfi_pop(ctx, 1);
                code = pdfi_setlinecap(ctx);
                break;
            case K1('K'):           /* setcmyk for non-stroke */
                pdfi_pop(ctx, 1);
                code = pdfi_setcmykstroke(ctx);
                break;
            case K1('k'):           /* setcmyk for non-stroke */
                pdfi_pop(ctx, 1);
                code = pdfi_setcmykfill(ctx);
                break;
            case K1('l'):           /* lineto */
                pdfi_pop(ctx, 1);
                code = pdfi_lineto(ctx);
                break;
            case K1('m'):           /* moveto */
                pdfi_pop(ctx, 1);
                code = pdfi_moveto(ctx);
                break;
            case K1('M'):           /* setmiterlimit */
                pdfi_pop(ctx, 1);
                code = pdfi_setmiterlimit(ctx);
                break;
            case K2('M','P'):       /* define marked content point */
                pdfi_pop(ctx, 1);
                if (pdfi_count_stack(ctx) >= 1)
                    pdfi_pop(ctx, 1);
                break;
            case K1('n'):           /* newpath */
                pdfi_pop(ctx, 1);
                code = pdfi_newpath(ctx);
                break;
            case K1('q'):           /* gsave */
                pdfi_pop(ctx, 1);
                code = pdfi_op_q(ctx);
                break;
            case K1('Q'):           /* grestore */
                pdfi_pop(ctx, 1);
                code = pdfi_op_Q(ctx);
                break;
            case K1('r'):       /* non-standard set rgb colour for non-stroke */
                pdfi_pop(ctx, 1);
                code = pdfi_setrgbfill_array(ctx);
                break;
            case K2('r','e'):       /* append rectangle */
                pdfi_pop(ctx, 1);
                code = pdfi_rectpath(ctx);
                break;
            case K2('R','G'):       /* set rgb colour for stroke */
                pdfi_pop(ctx, 1);
                code = pdfi_setrgbstroke(ctx);
                break;
            case K2('r','g'):       /* set rgb colour for non-stroke */
                pdfi_pop(ctx, 1);
                code = pdfi_setrgbfill(ctx);
                break;
            case K2('r','i'):       /* set rendering intent */
                pdfi_pop(ctx, 1);
                code = pdfi_ri(ctx);
                break;
            case K1('s'):           /* closepath, stroke */
                pdfi_pop(ctx, 1);
                code = pdfi_closepath_stroke(ctx);
                break;
            case K1('S'):           /* stroke */
                pdfi_pop(ctx, 1);
                code = pdfi_stroke(ctx);
                break;
            case K2('S','C'):       /* set colour for stroke */
                pdfi_pop(ctx, 1);
                code = pdfi_setstrokecolor(ctx);
                break;
            case K2('s','c'):       /* set colour for non-stroke */
                pdfi_pop(ctx, 1);
                code = pdfi_setfillcolor(ctx);
                break;
            case K3('S','C','N'):   /* set special colour for stroke */
                pdfi_pop(ctx, 1);
                code = pdfi_setcolorN(ctx, stream_dict, page_dict, false);
                break;
            case K3('s','c','n'):   /* set special colour for non-stroke */
                pdfi_pop(ctx, 1);
                code = pdfi_setcolorN(ctx, stream_dict, page_dict, true);
                break;
            case K2('s','h'):       /* fill with sahding pattern */
                pdfi_pop(ctx, 1);
                code = pdfi_shading(ctx, stream_dict, page_dict);
                break;
            case K2('T','*'):       /* Move to start of next text line */
                pdfi_pop(ctx, 1);
                code = pdfi_T_star(ctx);
                break;
            case K2('T','c'):       /* set character spacing */
                pdfi_pop(ctx, 1);
                code = pdfi_Tc(ctx);
                break;
            case K2('T','d'):       /* move text position */
                pdfi_pop(ctx, 1);
                code = pdfi_Td(ctx);
                break;
            case K2('T','D'):       /* Move text position, set leading */
                pdfi_pop(ctx, 1);
                code = pdfi_TD(ctx);
                break;
            case K2('T','f'):       /* set font and size */
                pdfi_pop(ctx, 1);
                code = pdfi_Tf(ctx, stream_dict, page_dict);
                break;
            case K2('T','j'):       /* show text */
                pdfi_pop(ctx, 1);
                code = pdfi_Tj(ctx);
                break;
            case K2('T','J'):       /* show text with individual glyph positioning */
                pdfi_pop(ctx, 1);
                code = pdfi_TJ(ctx);
                break;
            case K2('T','L'):       /* set text leading */
                pdfi_pop(ctx, 1);
                code = pdfi_TL(ctx);
                break;
            case K2('T','m'):       /* set text matrix */
                pdfi_pop(ctx, 1);
                code = pdfi_Tm(ctx);
                break;
            case K2('T','r'):       /* set text rendering mode */
                pdfi_pop(ctx, 1);
                code = pdfi_Tr(ctx);
                break;
            case K2('T','s'):       /* set text rise */
                pdfi_pop(ctx, 1);
                code = pdfi_Ts(ctx);
                break;
            case K2('T','w'):       /* set word spacing */
                pdfi_pop(ctx, 1);
                code = pdfi_Tw(ctx);
                break;
            case K2('T','z'):       /* set text matrix */
                pdfi_pop(ctx, 1);
                code = pdfi_Tz(ctx);
                break;
            case K1('v'):           /* append curve (initial point replicated) */
                pdfi_pop(ctx, 1);
                code = pdfi_v_curveto(ctx);
                break;
            case K1('w'):           /* setlinewidth */
                pdfi_pop(ctx, 1);
                code = pdfi_setlinewidth(ctx);
                break;
            case K1('W'):           /* clip */
                pdfi_pop(ctx, 1);
                ctx->clip_active = true;
                ctx->do_eoclip = false;
                break;
            case K2('W','*'):       /* eoclip */
                pdfi_pop(ctx, 1);
                ctx->clip_active = true;
                ctx->do_eoclip = true;
                break;
            case K1('y'):           /* append curve (final point replicated) */
                pdfi_pop(ctx, 1);
                code = pdfi_y_curveto(ctx);
                break;
            case K1('\''):          /* move to next line and show text */
                pdfi_pop(ctx, 1);
                code = pdfi_singlequote(ctx);
                break;
            case K1('"'):           /* set word and character spacing, move to next line, show text */
                pdfi_pop(ctx, 1);
                code = pdfi_doublequote(ctx);
                break;
            default:
                code = split_bogus_operator(ctx, source, stream_dict, page_dict);
                if (code < 0)
                    return code;
                if (pdfi_count_stack(ctx) > 0) {
                    keyword = (pdf_keyword *)ctx->stack_top[-1];
                    if (keyword->key != TOKEN_NOT_A_KEYWORD)
                        return REPAIRED_KEYWORD;
                }
                break;
       }
    }
    /* We use a return value of 1 to indicate a repaired keyword (a pair of operators
     * was concatenated, and we split them up). We must not return a value > 0 from here
     * to avoid tripping that test.
     */
    if (code > 0)
        code = 0;
    return code;
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

static int setup_stream_DefaultSpaces(pdf_context *ctx, pdf_dict *stream_dict)
{
    int code = 0;
    pdf_dict *resources_dict = NULL, *colorspaces_dict = NULL;
    pdf_obj *DefaultSpace = NULL;

    /* Create any required DefaultGray, DefaultRGB or DefaultCMYK
     * spaces.
     */

    if (ctx->args.NOSUBSTDEVICECOLORS)
        return 0;

    code = pdfi_dict_knownget(ctx, stream_dict, "Resources", (pdf_obj **)&resources_dict);
    if (code > 0) {
        code = pdfi_dict_knownget(ctx, resources_dict, "ColorSpace", (pdf_obj **)&colorspaces_dict);
        if (code > 0) {
            code = pdfi_dict_knownget(ctx, colorspaces_dict, "DefaultGray", &DefaultSpace);
            if (code > 0) {
                gs_color_space *pcs;
                code = pdfi_create_colorspace(ctx, DefaultSpace, NULL, stream_dict, &pcs, false);
                /* If any given Default* space fails simply ignore it, we wil then use the Device
                 * space (or page level Default) instead, this is as per the spec.
                 */
                if (code >= 0) {
                    if (ctx->page.DefaultGray_cs)
                        rc_decrement_only(ctx->page.DefaultGray_cs, "setup_stream_DefaultSpaces");
                    ctx->page.DefaultGray_cs = pcs;
                    pdfi_set_colour_callback(pcs, ctx, NULL);
                }
            }
            pdfi_countdown(DefaultSpace);
            DefaultSpace = NULL;
            code = pdfi_dict_knownget(ctx, colorspaces_dict, "DefaultRGB", &DefaultSpace);
            if (code > 0) {
                gs_color_space *pcs;
                code = pdfi_create_colorspace(ctx, DefaultSpace, NULL, stream_dict, &pcs, false);
                /* If any given Default* space fails simply ignore it, we wil then use the Device
                 * space (or page level Default) instead, this is as per the spec.
                 */
                if (code >= 0) {
                    if (ctx->page.DefaultRGB_cs)
                        rc_decrement_only(ctx->page.DefaultRGB_cs, "setup_stream_DefaultSpaces");
                    ctx->page.DefaultRGB_cs = pcs;
                    pdfi_set_colour_callback(pcs, ctx, NULL);
                }
            }
            pdfi_countdown(DefaultSpace);
            DefaultSpace = NULL;
            code = pdfi_dict_knownget(ctx, colorspaces_dict, "DefaultCMYK", &DefaultSpace);
            if (code > 0) {
                gs_color_space *pcs;
                code = pdfi_create_colorspace(ctx, DefaultSpace, NULL, stream_dict, &pcs, false);
                /* If any given Default* space fails simply ignore it, we wil then use the Device
                 * space (or page level Default) instead, this is as per the spec.
                 */
                if (code >= 0) {
                    if (ctx->page.DefaultCMYK_cs)
                        rc_decrement_only(ctx->page.DefaultCMYK_cs, "setup_stream_DefaultSpaces");
                    ctx->page.DefaultCMYK_cs = pcs;
                    pdfi_set_colour_callback(pcs, ctx, NULL);
                }
            }
            pdfi_countdown(DefaultSpace);
            DefaultSpace = NULL;
        }
    }

    pdfi_countdown(DefaultSpace);
    pdfi_countdown(resources_dict);
    pdfi_countdown(colorspaces_dict);
    return 0;
}

/* Run a stream in a sub-context (saves/restores DefaultQState) */
int pdfi_run_context(pdf_context *ctx, pdf_stream *stream_obj,
                     pdf_dict *page_dict, bool stoponerror, const char *desc)
{
    int code;
    gs_gstate *DefaultQState;
    /* Save any existing Default* colour spaces */
    gs_color_space *PageDefaultGray = ctx->page.DefaultGray_cs;
    gs_color_space *PageDefaultRGB = ctx->page.DefaultRGB_cs;
    gs_color_space *PageDefaultCMYK = ctx->page.DefaultCMYK_cs;

    /* increment their reference counts because we took a new reference to each */
    rc_increment(ctx->page.DefaultGray_cs);
    rc_increment(ctx->page.DefaultRGB_cs);
    rc_increment(ctx->page.DefaultCMYK_cs);

#if DEBUG_CONTEXT
    dbgmprintf(ctx->memory, "pdfi_run_context BEGIN\n");
#endif
    /* If the stream has any Default* colour spaces, replace the page level ones.
     * This will derement the reference counts to the current spaces if they are replaced.
     */
    setup_stream_DefaultSpaces(ctx, stream_obj->stream_dict);

    pdfi_copy_DefaultQState(ctx, &DefaultQState);
    pdfi_set_DefaultQState(ctx, ctx->pgs);
    code = pdfi_interpret_inner_content_stream(ctx, stream_obj, page_dict, stoponerror, desc);
    pdfi_restore_DefaultQState(ctx, &DefaultQState);

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
    if (!ctx->args.pdfstoponerror)
        code = 0;
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
    pdf_c_stream *stream;
    pdf_keyword *keyword;

    if (content_stream != NULL) {
        stream = content_stream;
    } else {
        code = pdfi_seek(ctx, ctx->main_stream, pdfi_stream_offset(ctx, stream_obj), SEEK_SET);
        if (code < 0)
            return code;

        code = pdfi_filter(ctx, stream_obj, ctx->main_stream, &stream, false);
        if (code < 0)
            return code;
    }

    pdfi_set_stream_parent(ctx, stream_obj, ctx->current_stream);
    ctx->current_stream = stream_obj;

    do {
        code = pdfi_read_token(ctx, stream, stream_obj->object_num, stream_obj->generation_num);
        if (code < 0) {
            if (code == gs_error_ioerror || code == gs_error_VMerror || ctx->args.pdfstoponerror) {
                if (code == gs_error_ioerror) {
                    pdfi_set_error(ctx, 0, NULL, E_PDF_BADSTREAM, "pdfi_interpret_content_stream", (char *)"**** Error reading a content stream.  The page may be incomplete");
                } else if (code == gs_error_VMerror) {
                    pdfi_set_error(ctx, 0, NULL, E_PDF_OUTOFMEMORY, "pdfi_interpret_content_stream", (char *)"**** Error ran out of memory reading a content stream.  The page may be incomplete");
                }
                goto exit;
            }
            continue;
        }

        if (pdfi_count_stack(ctx) <= 0) {
            if(stream->eof == true)
                break;
        }

        if (ctx->stack_top[-1]->type == PDF_KEYWORD) {
repaired_keyword:
            keyword = (pdf_keyword *)ctx->stack_top[-1];

            switch(keyword->key) {
                case TOKEN_ENDSTREAM:
                    pdfi_pop(ctx,1);
                    goto exit;
                    break;
                case TOKEN_ENDOBJ:
                    pdfi_clearstack(ctx);
                    pdfi_set_error(ctx, 0, NULL, E_PDF_MISSINGENDSTREAM, "pdfi_interpret_content_stream", NULL);
                    if (ctx->args.pdfstoponerror)
                        code = gs_note_error(gs_error_syntaxerror);
                    goto exit;
                    break;
                case TOKEN_NOT_A_KEYWORD:
                    {
                        pdf_dict *stream_dict = NULL;

                        code = pdfi_dict_from_obj(ctx, (pdf_obj *)stream_obj, &stream_dict);
                        if (code < 0)
                            goto exit;

                        code = pdfi_interpret_stream_operator(ctx, stream, stream_dict, page_dict);
                        if (code == REPAIRED_KEYWORD)
                            goto repaired_keyword;

                        if (code < 0) {
                            pdfi_set_error(ctx, code, NULL, E_PDF_TOKENERROR, "pdf_interpret_content_stream", NULL);
                            if (ctx->args.pdfstoponerror) {
                                pdfi_clearstack(ctx);
                                goto exit;
                            }
                        }
                    }
                    break;
                case TOKEN_INVALID_KEY:
                    pdfi_set_error(ctx, 0, NULL, E_PDF_KEYWORDTOOLONG, "pdfi_interpret_content_stream", NULL);
                    pdfi_clearstack(ctx);
                    break;
                default:
                    pdfi_set_error(ctx, 0, NULL, E_PDF_MISSINGENDSTREAM, "pdfi_interpret_content_stream", NULL);
                    pdfi_clearstack(ctx);
                    break;
            }
        }
        if(stream->eof == true)
            break;
    }while(1);

exit:
    ctx->current_stream = pdfi_stream_parent(ctx, stream_obj);
    pdfi_clear_stream_parent(ctx, stream_obj);
    pdfi_close_file(ctx, stream);
    return code;
}
