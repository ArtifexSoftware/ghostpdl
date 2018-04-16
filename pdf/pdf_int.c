/* Copyright (C) 2001-2018 Artifex Software, Inc.
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

/***********************************************************************************/
/* Functions to free the various kinds of 'PDF objects' and stack manipulations.   */
/* All objects are reference counted. Pushign an object onto the stack increments  */
/* its reference count, popping it from the stack decrements its reference count.  */
/* When an object's reference count is decremented to 0, the relevant 'free'       */
/* function is called to free the object.                                          */

static void pdf_free_namestring(pdf_obj *o)
{
    /* Currently names and strings are the same, so a single cast is OK */
    pdf_name *n = (pdf_name *)o;

    if (n->data != NULL)
        gs_free_object(n->object.memory, n->data, "pdf interpreter free name or string data");
    gs_free_object(n->object.memory, n, "pdf interpreter free name or string");
}

static void pdf_free_keyword(pdf_obj *o)
{
    /* Currently names and strings are the same, so a single cast is OK */
    pdf_keyword *k = (pdf_keyword *)o;

    if (k->data != NULL)
        gs_free_object(k->object.memory, k->data, "pdf interpreter free keyword data");
    gs_free_object(k->object.memory, k, "pdf interpreter free keyword");
}

static void pdf_free_array(pdf_obj *o)
{
    pdf_array *a = (pdf_array *)o;
    int i;

    for (i=0;i < a->entries;i++) {
        if (a->values[i] != NULL)
            pdf_countdown(a->values[i]);
    }
    gs_free_object(a->object.memory, a->values, "pdf interpreter free array contents");
    gs_free_object(a->object.memory, a, "pdf interpreter free array");
}

static void pdf_free_dict(pdf_obj *o)
{
    pdf_dict *d = (pdf_dict *)o;
    int i;

    for (i=0;i < d->entries;i++) {
        if (d->keys[i] != NULL)
            pdf_countdown((pdf_obj *)d->keys[i]);
        if (d->values[i] != NULL)
            pdf_countdown((pdf_obj *)d->values[i]);
    }
    gs_free_object(d->object.memory, d->keys, "pdf interpreter free dictionary keys");
    gs_free_object(d->object.memory, d->values, "pdf interpreter free dictioanry values");
    gs_free_object(d->object.memory, d, "pdf interpreter free dictionary");
}

void pdf_free_object(pdf_obj *o)
{
    switch(o->type) {
        case PDF_NULL:
        case PDF_TRUE:
        case PDF_FALSE:
        case PDF_INT:
        case PDF_REAL:
        case PDF_INDIRECT:
            gs_free_object(o->memory, o, "pdf interpreter object refcount to 0");
            break;
        case PDF_STRING:
        case PDF_NAME:
            pdf_free_namestring(o);
            break;
        case PDF_ARRAY:
            pdf_free_array(o);
            break;
        case PDF_DICT:
            pdf_free_dict(o);
            break;
        case PDF_KEYWORD:
            pdf_free_keyword(o);
            break;
        default:
#ifdef DEBUG
            emprintf(o->memory, "Attempting to free unknown obect type\n");
#endif
            break;
    }
}

static int pdf_pop(pdf_context *ctx, int num)
{
    pdf_obj *o;

    while(num) {
        if (ctx->stack_top > ctx->stack_bot) {
            pdf_countdown(ctx->stack_top[-1]);
            ctx->stack_top--;
        } else {
            return_error(gs_error_stackunderflow);
        }
        num--;
    }
    return 0;
}

static void pdf_clearstack(pdf_context *ctx)
{
    pdf_pop(ctx, ctx->stack_top - ctx->stack_bot);
}

static int pdf_push(pdf_context *ctx, pdf_obj *o)
{
    pdf_obj **new_stack;
    uint32_t entries = 0;

    if (ctx->stack_top < ctx->stack_bot)
        ctx->stack_top = ctx->stack_bot;

    if (ctx->stack_top > ctx->stack_limit) {
        if (ctx->stack_size >= MAX_STACK_SIZE)
            return_error(gs_error_stackoverflow);

        new_stack = (pdf_obj **)gs_alloc_bytes(ctx->memory, (ctx->stack_size + INITIAL_STACK_SIZE) * sizeof (pdf_obj *), "pdf_push_increase_interp_stack");
        if (new_stack == NULL)
            return_error(gs_error_VMerror);

        memcpy(new_stack, ctx->stack_bot, ctx->stack_size * sizeof(pdf_obj *));
        gs_free_object(ctx->memory, ctx->stack_bot, "pdf_push_increase_interp_stack");

        entries = (ctx->stack_top - ctx->stack_bot) / sizeof(pdf_obj *);

        ctx->stack_bot = new_stack;
        ctx->stack_top = ctx->stack_bot + (entries * sizeof(pdf_obj *));
        ctx->stack_size += INITIAL_STACK_SIZE;
        ctx->stack_limit = ctx->stack_bot + (ctx->stack_size * sizeof(pdf_obj *));
    }

    *ctx->stack_top = o;
    ctx->stack_top++;
    o->refcnt++;

    return 0;
}

/***********************************************************************************/
/* 'token' reading functions. Tokens in this sense are PDF logical objects and the */
/* related keywords. So that's numbers, booleans, names, strings, dictionaries,    */
/* arrays, the  null object and indirect references. The keywords are obj/endobj   */
/* stream/endstream, xref, startxref and trailer.                                  */

static void skip_white(pdf_context *ctx, stream *s)
{
    uint32_t bytes = 0, read = 0;
    byte c;

    do {
        bytes = pdf_read_bytes(ctx, &c, 1, 1, s);
        read += bytes;
    } while (bytes != 0 && iswhite(c));

    if (read > 0)
        pdf_unread(ctx, &c, 1);
}

static int pdf_read_num(pdf_context *ctx, stream *s)
{
    byte Buffer[256];
    unsigned short index = 0, bytes = 0;
    bool real = false;
    int64_t i = 0;
    double d = 0;
    pdf_num *num;
    int code = 0;

    skip_white(ctx, s);

    do {
        bytes = pdf_read_bytes(ctx, (byte *)&Buffer[index], 1, 1, s);

        if (iswhite((char)Buffer[index])) {
            Buffer[index] = 0x00;
            break;
        } else {
            if (isdelimiter((char)Buffer[index])) {
                pdf_unread(ctx, (byte *)&Buffer[index], 1);
                Buffer[index] = 0x00;
                break;
            }
        }
        if (Buffer[index] == '.')
            real = true;
        else {
            if (Buffer[index] == '-' || Buffer[index] == '+') {
                if (index != 0)
                    return_error(gs_error_syntaxerror);
            } else {
                if (Buffer[index] < 0x30 || Buffer[index] > 0x39)
                    return_error(gs_error_syntaxerror);
            }
        }
        if (index++ > 256)
            return_error(gs_error_syntaxerror);
    } while(1);

    num = (pdf_num *)gs_alloc_bytes(ctx->memory, sizeof(pdf_num), "pdf_read_num");
    if (num == NULL)
        return_error(gs_error_VMerror);

    memset(num, 0x00, sizeof(pdf_num));
    num->object.memory = ctx->memory;

    if (real) {
        num->object.type = PDF_REAL;
        if (sscanf((const char *)Buffer, "%f", &num->value.d) == 0) {
            gs_free_object(num->object.memory, num, "pdf_read_num error");
            return_error(gs_error_syntaxerror);
        }
    } else {
        num->object.type = PDF_INT;
        if (sscanf((const char *)Buffer, "%d", &num->value.i) == 0) {
            gs_free_object(num->object.memory, num, "pdf_read_num error");
            return_error(gs_error_syntaxerror);
        }
    }

    code = pdf_push(ctx, (pdf_obj *)num);

    if (code < 0)
        pdf_free_object((pdf_obj *)num);

    return code;
}

static int pdf_read_name(pdf_context *ctx, stream *s)
{
    char *Buffer, *NewBuf = NULL;
    unsigned short index = 0, bytes = 0;
    uint32_t size = 256;
    pdf_name *name = NULL;
    int code;

    Buffer = (char *)gs_alloc_bytes(ctx->memory, size, "pdf_read_name");
    if (Buffer == NULL)
        return_error(gs_error_VMerror);

    do {
        bytes = pdf_read_bytes(ctx, (byte *)&Buffer[index], 1, 1, s);

        if (bytes == 0) {
            Buffer[index] = 0x00;
            break;
        }

        if (iswhite((char)Buffer[index])) {
            Buffer[index] = 0x00;
            break;
        } else {
            if (isdelimiter((char)Buffer[index])) {
                pdf_unread(ctx, (byte *)&Buffer[index], 1);
                Buffer[index] = 0x00;
                break;
            }
        }

        /* Check for and convert escaped name characters */
        if (Buffer[index] == '#') {
            byte NumBuf[2];

            bytes = pdf_read_bytes(ctx, (byte *)&NumBuf, 1, 2, s);
            if (bytes < 2) {
                gs_free_object(ctx->memory, Buffer, "pdf_read_name error");
                return_error(gs_error_ioerror);
            }

            if (NumBuf[0] < 0x30 || NumBuf[1] < 0x30 || NumBuf[0] > 0x39 || NumBuf[1] > 0x39) {
                gs_free_object(ctx->memory, Buffer, "pdf_read_name error");
                return_error(gs_error_ioerror);
            }

            Buffer[index] = ((NumBuf[0] - 0x30) * 10) + (NumBuf[1] - 0x30);
        }

        /* If we ran out of memory, increase the buffer size */
        if (index++ >= size) {
            NewBuf = (char *)gs_alloc_bytes(ctx->memory, size + 256, "pdf_read_name");
            if (NewBuf == NULL) {
                gs_free_object(ctx->memory, Buffer, "pdf_read_name error");
                return_error(gs_error_VMerror);
            }
            memcpy(NewBuf, Buffer, size);
            gs_free_object(ctx->memory, Buffer, "pdf_read_name");
            Buffer = NewBuf;
            size += 256;
        }
    } while(1);

    name = (pdf_name *)gs_alloc_bytes(ctx->memory, sizeof(pdf_name), "pdf_read_name");
    if (name == NULL) {
        gs_free_object(ctx->memory, Buffer, "pdf_read_name error");
        return_error(gs_error_VMerror);
    }

    memset(name, 0x00, sizeof(pdf_name));
    name->object.memory = ctx->memory;
    name->object.type = PDF_NAME;

    NewBuf = (char *)gs_alloc_bytes(ctx->memory, index, "pdf_read_name");
    if (NewBuf == NULL) {
        gs_free_object(ctx->memory, Buffer, "pdf_read_name error");
        return_error(gs_error_VMerror);
    }
    memcpy(NewBuf, Buffer, index);
    gs_free_object(ctx->memory, Buffer, "pdf_read_name");

    name->data = (unsigned char *)NewBuf;
    name->length = index;

    code = pdf_push(ctx, (pdf_obj *)name);

    if (code < 0)
        pdf_free_namestring((pdf_obj *)name);

    return code;
}

static int pdf_read_hexstring(pdf_context *ctx, stream *s)
{
    char *Buffer, *NewBuf = NULL, HexBuf[2];
    unsigned short index = 0, bytes = 0;
    uint32_t size = 256;
    pdf_string *string = NULL;
    int code;

    Buffer = (char *)gs_alloc_bytes(ctx->memory, size, "pdf_read_hexstring");
    if (Buffer == NULL)
        return_error(gs_error_VMerror);

    do {
        bytes = pdf_read_bytes(ctx, (byte *)HexBuf, 1, 1, s);
        if (bytes == 0)
            return_error(gs_error_ioerror);

        if (HexBuf[0] == '>')
            break;

        bytes = pdf_read_bytes(ctx, (byte *)&HexBuf[1], 1, 1, s);
        if (bytes == 0)
            return_error(gs_error_ioerror);

        if (!ishex(HexBuf[0]) || !ishex(HexBuf[1]))
            return_error(gs_error_syntaxerror);

        Buffer[index] = (fromhex(HexBuf[0]) << 4) + fromhex(HexBuf[1]);

        if (index++ >= size) {
            NewBuf = (char *)gs_alloc_bytes(ctx->memory, size + 256, "pdf_read_string");
            if (NewBuf == NULL) {
                gs_free_object(ctx->memory, Buffer, "pdf_read_string error");
                return_error(gs_error_VMerror);
            }
            memcpy(NewBuf, Buffer, size);
            gs_free_object(ctx->memory, Buffer, "pdf_read_string");
            Buffer = NewBuf;
            size += 256;
        }
    } while(1);

    string = (pdf_string *)gs_alloc_bytes(ctx->memory, sizeof(pdf_string), "pdf_read_string");
    if (string == NULL) {
        gs_free_object(ctx->memory, Buffer, "pdf_read_string");
        return_error(gs_error_VMerror);
    }

    memset(string, 0x00, sizeof(pdf_string));
    string->object.memory = ctx->memory;
    string->object.type = PDF_STRING;

    NewBuf = (char *)gs_alloc_bytes(ctx->memory, index, "pdf_read_string");
    if (NewBuf == NULL) {
        gs_free_object(ctx->memory, Buffer, "pdf_read_string error");
        return_error(gs_error_VMerror);
    }
    memcpy(NewBuf, Buffer, index);
    gs_free_object(ctx->memory, Buffer, "pdf_read_string");

    string->data = (unsigned char *)NewBuf;
    string->length = index;

    code = pdf_push(ctx, (pdf_obj *)string);
    if (code < 0)
        pdf_free_namestring((pdf_obj *)string);

    return code;
}

static int pdf_read_string(pdf_context *ctx, stream *s)
{
    char *Buffer, *NewBuf = NULL;
    unsigned short index = 0, bytes = 0;
    uint32_t size = 256;
    pdf_string *string = NULL;
    int code;

    Buffer = (char *)gs_alloc_bytes(ctx->memory, size, "pdf_read_string");
    if (Buffer == NULL)
        return_error(gs_error_VMerror);

    do {
        bytes = pdf_read_bytes(ctx, (byte *)&Buffer[index], 1, 1, s);

        if (bytes == 0) {
            Buffer[index] = 0x00;
            break;
        }

        if (Buffer[index] == ')') {
            Buffer[index] = 0x00;
            break;
        }

        if (index++ >= size) {
            NewBuf = (char *)gs_alloc_bytes(ctx->memory, size + 256, "pdf_read_string");
            if (NewBuf == NULL) {
                gs_free_object(ctx->memory, Buffer, "pdf_read_string error");
                return_error(gs_error_VMerror);
            }
            memcpy(NewBuf, Buffer, size);
            gs_free_object(ctx->memory, Buffer, "pdf_read_string");
            Buffer = NewBuf;
            size += 256;
        }
    } while(1);

    string = (pdf_string *)gs_alloc_bytes(ctx->memory, sizeof(pdf_string), "pdf_read_string");
    if (string == NULL) {
        gs_free_object(ctx->memory, Buffer, "pdf_read_string");
        return_error(gs_error_VMerror);
    }

    memset(string, 0x00, sizeof(pdf_string));
    string->object.memory = ctx->memory;
    string->object.type = PDF_STRING;

    NewBuf = (char *)gs_alloc_bytes(ctx->memory, index, "pdf_read_string");
    if (NewBuf == NULL) {
        gs_free_object(ctx->memory, Buffer, "pdf_read_string error");
        return_error(gs_error_VMerror);
    }
    memcpy(NewBuf, Buffer, index);
    gs_free_object(ctx->memory, Buffer, "pdf_read_string");

    string->data = (unsigned char *)NewBuf;
    string->length = index;

    code = pdf_push(ctx, (pdf_obj *)string);
    if (code < 0)
        pdf_free_namestring((pdf_obj *)string);

    return code;
}

static int pdf_read_array(pdf_context *ctx, stream *s)
{
    unsigned short index = 0, bytes = 0;
    byte Buffer;
    pdf_array *a = NULL;
    uint64_t i = 0;
    pdf_obj *o;
    int code;

    do {
        skip_white(ctx, s);

        bytes = pdf_read_bytes(ctx, &Buffer, 1, 1, s);
        
        if (bytes == 0)
            return_error(gs_error_ioerror);

        if (Buffer == ']') {
            break;
        } else {
            pdf_unread(ctx, &Buffer, 1);
            code = pdf_read_token(ctx, s);
            if (code < 0)
                return code;
            index++;
        }
    } while (1);

    a = (pdf_array *)gs_alloc_bytes(ctx->memory, sizeof(pdf_array), "pdf_read_array");
    if (a == NULL)
        return_error(gs_error_VMerror);

    memset(a, 0x00, sizeof(pdf_array));
    a->object.memory = ctx->memory;
    a->object.type = PDF_ARRAY;

    a->size = a->entries = index;

    a->values = (pdf_obj **)gs_alloc_bytes(ctx->memory, index * sizeof(pdf_obj *), "pdf_read_array");
    if (a->values == NULL) {
        gs_free_object(a->object.memory, a, "pdf_read_array error");
        return_error(gs_error_VMerror);
    }

    while (index) {
        o = ctx->stack_top[-1];
        a->values[--index] = o;
        o->refcnt++;
        pdf_pop(ctx, 1);
    }

    code = pdf_push(ctx, (pdf_obj *)a);
    if (code < 0)
        pdf_free_array((pdf_obj *)a);

    return code;
}

static int pdf_read_dict(pdf_context *ctx, stream *s)
{
    unsigned short index = 0, bytes = 0;
    byte Buffer[2];
    pdf_dict *d = NULL;
    uint64_t i = 0;
    pdf_obj *o;
    int code;
    pdf_obj **stack_mark = ctx->stack_top;

    do {
        skip_white(ctx, s);

        bytes = pdf_read_bytes(ctx, &Buffer[0], 1, 1, s);
        
        if (bytes == 0)
            return_error(gs_error_ioerror);

        if (Buffer[0] == '>') {
            bytes = pdf_read_bytes(ctx, &Buffer[1], 1, 1, s);
            
            if (bytes == 0)
                return_error(gs_error_ioerror);

            if (Buffer[1] == '>')
                break;

            pdf_unread(ctx, &Buffer[1], 1);
        } 

        pdf_unread(ctx, &Buffer[0], 1);
        code = pdf_read_token(ctx, s);
        if (code < 0)
            return code;
    } while (1);

    index = ctx->stack_top - stack_mark;

    if (index & 1)
        return_error(gs_error_rangecheck);

    d = (pdf_dict *)gs_alloc_bytes(ctx->memory, sizeof(pdf_dict), "pdf_read_dict");
    if (d == NULL)
        return_error(gs_error_VMerror);

    memset(d, 0x00, sizeof(pdf_dict));
    d->object.memory = ctx->memory;
    d->object.type = PDF_DICT;

    d->size = d->entries = index >> 1;

    d->keys = (pdf_obj **)gs_alloc_bytes(ctx->memory, d->size * sizeof(pdf_obj *), "pdf_read_dict");
    if (d->keys == NULL) {
        gs_free_object(d->object.memory, d, "pdf_read_dict error");
        return_error(gs_error_VMerror);
    }

    d->values = (pdf_obj **)gs_alloc_bytes(ctx->memory, d->size * sizeof(pdf_obj *), "pdf_read_dict");
    if (d->values == NULL) {
        gs_free_object(d->object.memory, d->keys, "pdf_read_dict error");
        gs_free_object(d->object.memory, d, "pdf_read_dict error");
        return_error(gs_error_VMerror);
    }
    
    while (index) {
        i = (index / 2) - 1;

        /* In PDF keys are *required* to be names, so we ought to check that here */
        if (((pdf_obj *)ctx->stack_top[-2])->type == PDF_NAME) {
            d->keys[i] = ctx->stack_top[-2];
            d->keys[i]->refcnt++;
            d->values[i] = ctx->stack_top[-1];
            d->values[i]->refcnt++;
        } else {
            pdf_free_dict((pdf_obj *)d);
            return_error(gs_error_typecheck);
        }

        pdf_pop(ctx, 2);
        index -= 2;
    }

    code = pdf_push(ctx, (pdf_obj *)d);
    if (code < 0)
        pdf_free_dict((pdf_obj *)d);

    return code;
}

int pdf_dict_get(pdf_dict *d, char *Key, pdf_obj **o)
{
    int i=0;
    pdf_obj *t;

    *o = NULL;

    for (i=0;i< d->entries;i++) {
        t = d->keys[i];

        if (t && t->type == PDF_NAME) {
            pdf_name *n = (pdf_name *)t;

            if (((pdf_name *)t)->length == strlen((const char *)Key) && memcmp((const char *)((pdf_name *)t)->data, (const char *)Key, ((pdf_name *)t)->length) == 0) {
                *o = d->values[i];
                return 0;
            }
        }
    }
    return_error(gs_error_undefined);
}

int pdf_dict_put(pdf_dict *d, pdf_obj *key, pdf_obj *value)
{
    return 0;
}

int pdf_array_get(pdf_array *a, uint64_t index, pdf_obj **o)
{
    if (index > a->size)
        return_error(gs_error_rangecheck);

    *o = a->values[index];
    return 0;
}

static int pdf_read_bool(pdf_context *ctx, stream *s)
{
    unsigned short index = 0, bytes = 0;
    byte Buffer[6];
    bool result = false;
    pdf_obj *o;
    int code;

    bytes = pdf_read_bytes(ctx, (byte *)Buffer, 1, 1, s);
    if (bytes == 0)
        return_error(gs_error_ioerror);

    if (Buffer[0] == 't') {
        bytes = pdf_read_bytes(ctx, (byte *)&Buffer[1], 1, 4, s);
        if (bytes < 3)
            return_error(gs_error_ioerror);
        if (bytes == 3) {
        } else {
            if (iswhite(Buffer[4])) {
                Buffer[4] = 0x00;
            } else {
                pdf_unread(ctx, (byte *)&Buffer[4], 1);
                if (isdelimiter(Buffer[4])) {
                    Buffer[4] = 0x00;
                } else {
                    return(gs_error_ioerror);
                }
            }
        }
        if (strcmp((const char *)Buffer, "true") != 0)
            return_error(gs_error_syntaxerror);
        result = true;

    } else {
        if (Buffer[0] == 'f') {
            bytes = pdf_read_bytes(ctx, (byte *)&Buffer[1], 1, 5, s);
            if (bytes < 4)
                return_error(gs_error_ioerror);
            if (bytes == 4) {
            } else {
                if (iswhite(Buffer[5])) {
                    Buffer[5] = 0x00;
                } else {
                    pdf_unread(ctx, (byte *)&Buffer[5], 1);
                    if (isdelimiter(Buffer[5])) {
                        Buffer[5] = 0x00;
                    } else {
                        return(gs_error_ioerror);
                    }
                }
            }
            if (strcmp((const char *)Buffer, "false") != 0)
                return_error(gs_error_syntaxerror);
        } else {
            return_error(gs_error_syntaxerror);
        }
    }

    o = (pdf_obj *)gs_alloc_bytes(ctx->memory, sizeof(pdf_obj), "pdf_read_bool");
    if (o == NULL)
        return_error(gs_error_VMerror);

    memset(0, 0x00, sizeof(pdf_obj));
    o->memory = ctx->memory;

    if (result)
        o->type = PDF_TRUE;
    else
        o->type = PDF_FALSE;

    code = pdf_push(ctx, o);
    if (code < 0)
        pdf_free_object(o);

    return code;
}

static int pdf_read_null(pdf_context *ctx, stream *s)
{
    unsigned short index = 0, bytes = 0;
    byte Buffer[6];
    bool result = false;
    pdf_obj *o;
    int code;

    bytes = pdf_read_bytes(ctx, (byte *)Buffer, 1, 5, s);
    if (bytes == 0 || bytes < 4)
        return_error(gs_error_ioerror);

    if (bytes == 4) {
        Buffer[4] = 0x00;
    } else {
        if (iswhite(Buffer[4])) {
            Buffer[4] = 0x00;
        } else {
            pdf_unread(ctx, (byte *)&Buffer[4], 1);
            if (isdelimiter(Buffer[4])) {
                Buffer[4] = 0x00;
            } else {
                return(gs_error_ioerror);
            }
        }
    }

    o = (pdf_obj *)gs_alloc_bytes(ctx->memory, sizeof(pdf_obj), "pdf_read_bool");
    if (o == NULL)
        return_error(gs_error_VMerror);

    memset(0, 0x00, sizeof(pdf_obj));
    o->memory = ctx->memory;
    o->type = PDF_NULL;

    code = pdf_push(ctx, o);
    if (code < 0)
        pdf_free_object(o);

    return code;
}

static void pdf_skip_comment(pdf_context *ctx, stream *s)
{
    byte Buffer;
    unsigned short index = 0, bytes = 0;

    do {
        bytes = pdf_read_bytes(ctx, (byte *)&Buffer, 1, 1, s);
        if (Buffer = 0x0A || Buffer == 0x0D) {
            break;
        }
    } while (bytes);
}

/* This function is slightly misnamed, for some keywords we do
 * indeed read the keyword and return a PDF_KEYWORD object, but
 * for null, true, false and R we create an appropriate object
 * of that type (PDF_NULL, PDF_TRUE, PDF_FALSE or PDF_INDIRECT_REF)
 * and return it instead.
 */
static int pdf_read_keyword(pdf_context *ctx, stream *s)
{
    byte Buffer[256], b;
    unsigned short index = 0, bytes = 0;
    int code;
    pdf_keyword *keyword;

    skip_white(ctx, s);

    do {
        bytes = pdf_read_bytes(ctx, (byte *)&Buffer[index], 1, 1, s);
        if (iswhite(Buffer[index])) {
            break;
        } else {
            if (isdelimiter(Buffer[index])) {
                pdf_unread(ctx, (byte *)&Buffer[index], 1);
                break;
            }
        }
        index++;
    } while (bytes && index < 255);

    if (index >= 255 || index == 0)
        return_error(gs_error_syntaxerror);

    /* NB The code below uses 'Buffer', not the data stored in keyword->data to compare strings */
    Buffer[index] = 0x00;

    keyword = (pdf_keyword *)gs_alloc_bytes(ctx->memory, sizeof(pdf_keyword), "pdf_read_keyword");
    if (keyword == NULL)
        return_error(gs_error_VMerror);

    keyword->data = (unsigned char *)gs_alloc_bytes(ctx->memory, index, "pdf_read_keyword");
    if (keyword->data == NULL) {
        gs_free_object(ctx->memory, keyword, "pdf_read_keyword error");
        return_error(gs_error_VMerror);
    }

    memset(keyword, 0x00, sizeof(pdf_obj));
    keyword->object.memory = ctx->memory;
    keyword->object.type = PDF_KEYWORD;

    memcpy(keyword->data, Buffer, index);
    keyword->length = index;
    keyword->key = PDF_NOT_A_KEYWORD;

    switch(Buffer[0]) {
        case 'R':
            if (keyword->length == 1){
                pdf_indirect_ref *o;
                uint64_t obj_num;
                uint32_t gen_num;

                pdf_free_keyword((pdf_obj *)keyword);

                if(ctx->stack_top - ctx->stack_bot < 2)
                    return_error(gs_error_stackunderflow);

                if(((pdf_obj *)ctx->stack_top[-1])->type != PDF_INT || ((pdf_obj *)ctx->stack_top[-2])->type != PDF_INT)
                    return_error(gs_error_typecheck);

                gen_num = ((pdf_num *)ctx->stack_top[-1])->value.i;
                pdf_pop(ctx, 1);
                obj_num = ((pdf_num *)ctx->stack_top[-1])->value.i;
                pdf_pop(ctx, 1);

                o = (pdf_indirect_ref *)gs_alloc_bytes(ctx->memory, sizeof(pdf_indirect_ref), "pdf_read_keyword");
                if (o == NULL)
                    return_error(gs_error_VMerror);

                memset(o, 0x00, sizeof(pdf_indirect_ref));
                o->object.memory = ctx->memory;
                o->object.type = PDF_INDIRECT;
                o->generation_num = gen_num;
                o->object_num = obj_num;

                code = pdf_push(ctx, (pdf_obj *)o);
                if (code < 0)
                    pdf_free_object((pdf_obj *)o);
                return code;
            }
            break;
        case 'e':
            if (keyword->length == 9 && memcmp((const char *)Buffer, "endstream", 9) == 0)
                keyword->key = PDF_ENDSTREAM;
            else {
                if (keyword->length == 6 && memcmp((const char *)Buffer, "endobj", 6) == 0)
                    keyword->key = PDF_ENDOBJ;
            }
            break;
        case 'o':
            if (keyword->length == 3 && memcmp((const char *)Buffer, "obj", 3) == 0)
                keyword->key = PDF_OBJ;
            break;
        case 's':
            if (keyword->length == 6 && memcmp((const char *)Buffer, "stream", 6) == 0){
                keyword->key = PDF_STREAM;
                do{
                    bytes = pdf_read_bytes(ctx, &b, 1, 1, s);
                    if (!iswhite(b))
                        break;
                }while (1);
                pdf_seek(ctx, ctx->main_stream, -1, SEEK_CUR);
            }
            else {
                if (keyword->length == 9 && memcmp((const char *)Buffer, "startxref", 9) == 0)
                    keyword->key = PDF_STARTXREF;
            }
            break;
        case 't':
            if (keyword->length == 4 && memcmp((const char *)Buffer, "true", 4) == 0) {
                pdf_obj *o;

                pdf_free_keyword((pdf_obj *)keyword);

                o = (pdf_obj *)gs_alloc_bytes(ctx->memory, sizeof(pdf_obj), "pdf_read_keyword");
                if (o == NULL)
                    return_error(gs_error_VMerror);

                memset(o, 0x00, sizeof(pdf_obj));
                o->memory = ctx->memory;
                o->type = PDF_TRUE;

                code = pdf_push(ctx, o);
                if (code < 0)
                    pdf_free_object((pdf_obj *)o);
                return code;
            }
            else {
                if (keyword->length == 7 && memcmp((const char *)Buffer, "trailer", 7) == 0)
                    keyword->key = PDF_TRAILER;
            }
            break;
        case 'f':
            if (keyword->length == 5 && memcmp((const char *)Buffer, "false", 5) == 0)
            {
                pdf_obj *o;

                pdf_free_keyword((pdf_obj *)keyword);

                o = (pdf_obj *)gs_alloc_bytes(ctx->memory, sizeof(pdf_obj), "pdf_read_keyword");
                if (o == NULL)
                    return_error(gs_error_VMerror);

                memset(o, 0x00, sizeof(pdf_obj));
                o->memory = ctx->memory;
                o->type = PDF_FALSE;

                code = pdf_push(ctx, o);
                if (code < 0)
                    pdf_free_object((pdf_obj *)o);
                return code;
            }
            break;
        case 'n':
            if (keyword->length == 4 && memcmp((const char *)Buffer, "null", 4) == 0){
                pdf_obj *o;

                pdf_free_keyword((pdf_obj *)keyword);

                o = (pdf_obj *)gs_alloc_bytes(ctx->memory, sizeof(pdf_obj), "pdf_read_keyword");
                if (o == NULL)
                    return_error(gs_error_VMerror);

                memset(o, 0x00, sizeof(pdf_obj));
                o->memory = ctx->memory;
                o->type = PDF_NULL;

                code = pdf_push(ctx, o);
                if (code < 0)
                    pdf_free_object((pdf_obj *)o);
                return code;
            }
            break;
        case 'x':
            if (keyword->length == 4 && memcmp((const char *)Buffer, "xref", 4) == 0)
                keyword->key = PDF_XREF;
            break;
    }

    code = pdf_push(ctx, (pdf_obj *)keyword);
    if (code < 0)
        pdf_free_object((pdf_obj *)keyword);

    return code;
}

/* This function reads form the given stream, at the current offset in the stream,
 * a single PDF 'token' and returns it on the stack.
 */
int pdf_read_token(pdf_context *ctx, stream *s)
{
    int32_t bytes = 0;
    char Buffer[256];
    int code;

    skip_white(ctx, s);

    bytes = pdf_read_bytes(ctx, (byte *)Buffer, 1, 1, s);
    if (bytes == 0)
        return (gs_error_ioerror);

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
            pdf_unread(ctx, (byte *)&Buffer[0], 1);
            code = pdf_read_num(ctx, s);
            if (code < 0)
                return code;
            break;
        case '/':
            return pdf_read_name(ctx, s);
            break;
        case '<':
            bytes = pdf_read_bytes(ctx, (byte *)&Buffer[1], 1, 1, s);
            if (bytes == 0)
                return (gs_error_ioerror);
            if (Buffer[1] == '<') {
                return pdf_read_dict(ctx, s);
            } else {
                if (ishex(Buffer[1])) {
                    pdf_unread(ctx, (byte *)&Buffer[1], 1);
                    return pdf_read_hexstring(ctx, s);
                }
                else
                    return_error(gs_error_syntaxerror);
            }
            break;
        case '(':
            return pdf_read_string(ctx, s);
            break;
        case '[':
            return pdf_read_array(ctx, s);
            break;
        case '%':
            pdf_skip_comment(ctx, s);
            break;
        default:
            pdf_unread(ctx, (byte *)&Buffer[0], 1);
            return pdf_read_keyword(ctx, s);
            break;
    }
    return 0;
}


/***********************************************************************************/
/* Highest level functions. The context we create here is returned to the 'PL'     */
/* implementation, in future we plan to return it to PostScript by wrapping a      */
/* gargabe collected object 'ref' around it and returning that to the PostScript   */
/* world. custom PostScript operators will then be able to render pages, annots,   */
/* AcroForms etc by passing the opaque object back to functions here, allowing     */
/* the interpreter access to its context.                                          */

/* We start with routines for creating and destroying the interpreter context */
pdf_context *pdf_create_context(gs_memory_t *pmem)
{
    pdf_context *ctx = NULL;
    gs_gstate *pgs = NULL;
    int code = 0;

    ctx = (pdf_context *) gs_alloc_bytes(pmem->non_gc_memory,
            sizeof(pdf_context), "pdf_imp_allocate_interp_instance");

    pgs = gs_gstate_alloc(pmem);

    if (!ctx || !pgs)
    {
        if (ctx)
            gs_free_object(pmem->non_gc_memory, ctx, "pdf_imp_allocate_interp_instance");
        if (pgs)
            gs_gstate_free(pgs);
        return NULL;
    }

    memset(ctx, 0, sizeof(pdf_context));

    ctx->stack_bot = (pdf_obj **)gs_alloc_bytes(pmem->non_gc_memory, INITIAL_STACK_SIZE * sizeof (pdf_obj *), "pdf_imp_allocate_interp_stack");
    if (ctx->stack_bot == NULL) {
        gs_free_object(pmem->non_gc_memory, ctx, "pdf_imp_allocate_interp_instance");
        gs_gstate_free(pgs);
        return NULL;
    }
    ctx->stack_size = INITIAL_STACK_SIZE;
    ctx->stack_top = ctx->stack_bot - sizeof(pdf_obj *);
    ctx->stack_limit = ctx->stack_bot + (ctx->stack_size * sizeof(pdf_obj *));

    code = gsicc_init_iccmanager(pgs);
    if (code < 0) {
        gs_free_object(pmem->non_gc_memory, ctx->stack_bot, "pdf_imp_allocate_interp_instance");
        gs_free_object(pmem->non_gc_memory, ctx, "pdf_imp_allocate_interp_instance");
        gs_gstate_free(pgs);
        return NULL;
    }

    ctx->memory = pmem->non_gc_memory;
    ctx->pgs = pgs;
    /* Declare PDL client support for high level patterns, for the benefit
     * of pdfwrite and other high-level devices
     */
    ctx->pgs->have_pattern_streams = true;
    ctx->fontdir = NULL;
    ctx->preserve_tr_mode = 0;

    ctx->main_stream = NULL;

    /* Gray, RGB and CMYK profiles set when color spaces installed in graphics lib */
    ctx->gray_lin = gs_cspace_new_ICC(ctx->memory, ctx->pgs, -1);
    ctx->gray = gs_cspace_new_ICC(ctx->memory, ctx->pgs, 1);
    ctx->cmyk = gs_cspace_new_ICC(ctx->memory, ctx->pgs, 4);
    ctx->srgb = gs_cspace_new_ICC(ctx->memory, ctx->pgs, 3);
    ctx->scrgb = gs_cspace_new_ICC(ctx->memory, ctx->pgs, 3);

    return ctx;
}

int pdf_free_context(gs_memory_t *pmem, pdf_context *ctx)
{
    if (ctx->Trailer)
        pdf_countdown((pdf_obj *)ctx->Trailer);

    if(ctx->Root)
        pdf_countdown((pdf_obj *)ctx->Root);

    if (ctx->Info)
        pdf_countdown((pdf_obj *)ctx->Info);

    if (ctx->xref)
        gs_free_object(ctx->memory, ctx->xref, "pdf_free_context");

    if (ctx->stack_bot) {
        pdf_clearstack(ctx);
        gs_free_object(ctx->memory, ctx->stack_bot, "pdf_free_context");
    }

    if (ctx->main_stream != NULL) {
        sfclose(ctx->main_stream);
        ctx->main_stream = NULL;
    }
    if(ctx->pgs != NULL) {
        gs_gstate_free(ctx->pgs);
        ctx->pgs = NULL;
    }

    gs_free_object(ctx->memory, ctx, "pdf_free_context");
    return 0;
}

/* Now routines to open a PDF file (and clean up in the event of an error) */
static void cleanup_pdf_open_file(pdf_context *ctx, byte *Buffer)
{
    if (Buffer != NULL)
        gs_free_object(ctx->memory, Buffer, "PDF interpreter - allocate working buffer for file validation");

    if (ctx->main_stream != NULL) {
        sfclose(ctx->main_stream);
        ctx->main_stream = NULL;
    }
    ctx->main_stream_length = 0;
}

#define BUF_SIZE 2048

int repair_pdf_file(pdf_context *ctx)
{
    pdf_clearstack(ctx);

    return 0;
}

static int read_xref_stream_entries(pdf_context *ctx, stream *s, uint64_t first, uint64_t last, unsigned char *W)
{
    uint i, j = 0;
    uint32_t type = 0;
    uint64_t objnum = 0, gen = 0;
    byte *Buffer;
    int code;
    uint64_t bytes = 0;
    xref_entry *entry;

    if (W[0] > W[1]) {
        if (W[0] > W[2]) {
            j = W[2];
        } else {
            j = W[2];
        }
    } else {
        if (W[1] > W[2]) {
            j = W[1];
        } else {
            j = W[2];
        }
    }

    Buffer = gs_alloc_bytes(ctx->memory, j, "read_xref_stream_entry working buffer");
    for (i=first;i<=last; i++){
        type = objnum = gen = 0;

        bytes = pdf_read_bytes(ctx, Buffer, 1, W[0], s);
        if (bytes < W[0]){
            gs_free_object(ctx->memory, Buffer, "read_xref_stream_entry, free working buffer (error)");
            return_error(gs_error_ioerror);
        }
        for (j=0;j<W[0];j++)
            type = (type << 8) + Buffer[j];
        bytes = pdf_read_bytes(ctx, Buffer, 1, W[1], s);
        if (bytes < W[1]){
            gs_free_object(ctx->memory, Buffer, "read_xref_stream_entry free working buffer (error)");
            return_error(gs_error_ioerror);
        }
        for (j=0;j<W[1];j++)
            objnum = (objnum << 8) + Buffer[j];
        bytes = pdf_read_bytes(ctx, Buffer, 1, W[2], s);
        if (bytes < W[2]){
            gs_free_object(ctx->memory, Buffer, "read_xref_stream_entry, free working buffer (error)");
            return_error(gs_error_ioerror);
        }
        for (j=0;j<W[2];j++)
            gen = (gen << 8) + Buffer[j];

        entry = &ctx->xref[i];
        if (entry->object_num != 0)
            continue;

        switch(j) {
            case 0:
                entry->compressed = false;
                entry->free = true;
                entry->offset = objnum;         /* For free objects we use the offset to store the object number of the next free object */
                entry->generation_num = gen;    /* And the generation number is the numebr to use if this object is used again */
                entry->object_num = i;
                entry->object = NULL;
                break;
            case 1:
                entry->compressed = false;
                entry->free = false;
                entry->offset = objnum;
                entry->generation_num = gen;
                entry->object_num = i;
                entry->object = NULL;
                break;
            case 2:
                entry->compressed = true;
                entry->free = false;
                entry->offset = objnum;         /* In this case we use the offset to store the object number of the compressed stream */
                entry->generation_num = gen;    /* And the generation number is the index of the object within the stream */
                entry->object_num = i;
                entry->object = NULL;
                break;
            default:
                gs_free_object(ctx->memory, Buffer, "read_xref_stream_entry, free working buffer");
                return_error(gs_error_rangecheck);
                break;
        }
    }
    gs_free_object(ctx->memory, Buffer, "read_xref_stream_entry, free working buffer");
    return 0;
}

/* These two routines are recursive.... */
static int pdf_read_xref_stream_dict(pdf_context *ctx, stream *s);

static int pdf_process_xref_stream(pdf_context *ctx, pdf_dict *d, stream *s)
{
    stream *XRefStrm;
    char Buffer[33];
    int code, i;
    pdf_obj *o;
    uint64_t size;
    unsigned char W[3];
    uint64_t prev_obj = 0, prev_gen = 0;

    code = pdf_dict_get(d, "Type", &o);
    if (code < 0)
        return code;

    if (o->type != PDF_NAME)
        return_error(gs_error_typecheck);
    else {
        pdf_name *n = (pdf_name *)o;

        if (n->length != 4 || memcmp(n->data, "XRef", 4) != 0)
            return_error(gs_error_syntaxerror);
    }

    code = pdf_dict_get(d, "Size", &o);
    if (code < 0)
        return code;

    if (o->type != PDF_INT)
        return_error(gs_error_typecheck);
    size = ((pdf_num *)o)->value.i;

    /* If this is the first xref stream then allocate the xref tbale and store the trailer */
    if (ctx->xref == NULL) {
        ctx->xref = (xref_entry *)gs_alloc_bytes(ctx->memory, ((pdf_num *)o)->value.i * sizeof(xref_entry), "read_xref_stream allocate xref table");
        if (ctx->xref == NULL)
            return_error(gs_error_VMerror);

        memset(ctx->xref, 0x00, ((pdf_num *)o)->value.i * sizeof(xref_entry));

        ctx->Trailer = d;
        d->object.refcnt++;
    }

    code = pdf_filter(ctx, d, s, &XRefStrm);
    if (code < 0) {
        gs_free_object(ctx->memory, ctx->xref, "read_xref_stream error");
        ctx->xref = NULL;
        return code;
    }

    code = pdf_dict_get(d, "W", &o);
    if (code < 0) {
        gs_free_object(ctx->memory, ctx->xref, "read_xref_stream error");
        ctx->xref = NULL;
        return code;
    }

    if (o->type != PDF_ARRAY)
        return_error(gs_error_typecheck);
    else {
        pdf_array *a = (pdf_array *)o;

        if (a->entries != 3)
            return_error(gs_error_rangecheck);
        for (i=0;i<3;i++) {
            code = pdf_array_get(a, (uint64_t)i, &o);
            if (code < 0)
                return code;

            if (o->type != PDF_INT) {
                gs_free_object(ctx->memory, ctx->xref, "read_xref_stream error");
                ctx->xref = NULL;
                return_error(gs_error_typecheck);
            }
            W[i] = ((pdf_num *)o)->value.i;
        }
    }

    code = pdf_dict_get(d, "Index", &o);
    if (code == gs_error_undefined) {
        code = read_xref_stream_entries(ctx, XRefStrm, 0, size - 1, W);
        if (code < 0)
            return code;
    } else {
        if (code < 0) {
            gs_free_object(ctx->memory, ctx->xref, "read_xref_stream error");
            ctx->xref = NULL;
            return code;
        }

        if (o->type != PDF_ARRAY)
            return_error(gs_error_typecheck);
        else {
            pdf_array *a = (pdf_array *)o;
            pdf_num *start, *end;

            if (a->entries & 1)
                return_error(gs_error_rangecheck);

            for (i=0;i < a->entries;i+=2){
                code = pdf_array_get(a, (uint64_t)i, (pdf_obj **)&start);
                if (code < 0)
                    return code;
                if (start->object.type != PDF_INT)
                    return_error(gs_error_typecheck);

                code = pdf_array_get(a, (uint64_t)i+1, (pdf_obj **)&end);
                if (code < 0)
                    return code;
                if (end->object.type != PDF_INT)
                    return_error(gs_error_typecheck);

                code = read_xref_stream_entries(ctx, XRefStrm, start->value.i, start->value.i + end->value.i - 1, W);
                if (code < 0)
                    return code;
            }
        }
    }

    code = pdf_dict_get(d, "Prev", &o);
    if (code == gs_error_undefined)
        return 0;

    if (code < 0)
        return code;

    if (o->type != PDF_INT)
        return_error(gs_error_typecheck);

    pdf_seek(ctx, s, ((pdf_num *)o)->value.i, SEEK_SET);

    code = pdf_read_token(ctx, ctx->main_stream);
    if (code < 0)
        return code;

    code = pdf_read_xref_stream_dict(ctx, s);

    return code;
}

static int pdf_read_xref_stream_dict(pdf_context *ctx, stream *s)
{
    int code;

    if (((pdf_obj *)ctx->stack_top[-1])->type == PDF_INT) {
        /* Its an integer, lets try for index gen obj as a XRef stream */
        code = pdf_read_token(ctx, ctx->main_stream);

        if (code < 0)
            return(repair_pdf_file(ctx));

        if (((pdf_obj *)ctx->stack_top[-1])->type != PDF_INT) {
            /* Second element is not an integer, not a valid xref */
            pdf_pop(ctx, 1);
            return(repair_pdf_file(ctx));
        }

        code = pdf_read_token(ctx, ctx->main_stream);
        if (code < 0) {
            pdf_pop(ctx, 1);
            return code;
        }

        if (((pdf_obj *)ctx->stack_top[-1])->type != PDF_KEYWORD) {
            /* Second element is not an integer, not a valid xref */
            pdf_pop(ctx, 2);
            return(repair_pdf_file(ctx));
        } else {
            int obj_num, gen_num;

            pdf_keyword *keyword = (pdf_keyword *)ctx->stack_top[-1];

            if (keyword->key != PDF_OBJ) {
                pdf_pop(ctx, 3);
                return(repair_pdf_file(ctx));
            }
            /* pop the 'obj', generation and object numbers */
            pdf_pop(ctx, 1);
            gen_num = ((pdf_num *)ctx->stack_top[-1])->value.i;
            pdf_pop(ctx, 1);
            obj_num = ((pdf_num *)ctx->stack_top[-1])->value.i;
            pdf_pop(ctx, 1);

            do {
                code = pdf_read_token(ctx, ctx->main_stream);
                if (code < 0)
                    return repair_pdf_file(ctx);

                if (((pdf_obj *)ctx->stack_top[-1])->type == PDF_KEYWORD) {
                    keyword = (pdf_keyword *)ctx->stack_top[-1];
                    if (keyword->key == PDF_STREAM) {
                        pdf_dict *d;
                        pdf_obj *o;

                        /* Remove the 'stream' token from the stack, should leave a dictionary object on the stack */
                        pdf_pop(ctx, 1);
                        if (((pdf_obj *)ctx->stack_top[-1])->type != PDF_DICT) {
                            pdf_pop(ctx, 1);
                            return repair_pdf_file(ctx);
                        }
                        d = (pdf_dict *)ctx->stack_top[-1];
                        d->stream = (gs_offset_t)stell(ctx->main_stream);

                        d->object.object_num = obj_num;
                        d->object.generation_num = gen_num;
                        d->object.refcnt++;
                        pdf_pop(ctx, 1);

                        code = pdf_process_xref_stream(ctx, d, ctx->main_stream);
                        if (code < 0) {
                            d->object.refcnt--;
                            return (repair_pdf_file(ctx));
                        }
                        d->object.refcnt--;
                        break;
                    }
                    if (keyword->key == PDF_ENDOBJ) {
                        break;
                    }
                }
            } while(1);

            /* We should now have pdf_object, endobj on the stack, pop the endobj */
            pdf_pop(ctx, 1);
        }
    } else {
        /* Not an 'xref' and not an integer, so not a valid xref */
        return(repair_pdf_file(ctx));
    }
    return 0;
}

static int read_xref(pdf_context *ctx, stream *s)
{
    int code = 0, i, j;
    pdf_obj *o = NULL;
    pdf_dict *d = NULL;
    uint64_t start = 0, size = 0, bytes = 0;
    char Buffer[21];

    code = pdf_read_token(ctx, ctx->main_stream);

    if (code < 0)
        return code;

    o = ctx->stack_top[-1];
    if (o->type != PDF_INT) {
        /* element is not an integer, not a valid xref */
        pdf_pop(ctx, 1);
        return_error(gs_error_typecheck);
    }
    start = ((pdf_num *)o)->value.i;

    code = pdf_read_token(ctx, ctx->main_stream);
    if (code < 0) {
        pdf_pop(ctx, 1);
        return code;
    }

    o = ctx->stack_top[-1];
    if (o->type != PDF_INT) {
        /* element is not an integer, not a valid xref */
        pdf_pop(ctx, 2);
        return_error(gs_error_typecheck);
    }
    size = ((pdf_num *)o)->value.i;
    pdf_pop(ctx, 2);

    /* If this is the first xref then allocate the xref table and store the trailer */
    if (ctx->xref == NULL && size != 0) {
        ctx->xref = (xref_entry *)gs_alloc_bytes(ctx->memory, (start + size) * sizeof(xref_entry), "read_xref_stream allocate xref table");
        if (ctx->xref == NULL)
            return_error(gs_error_VMerror);

        memset(ctx->xref, 0x00, ((pdf_num *)o)->value.i * sizeof(xref_entry));
    }

    skip_white(ctx, s);
    for (i=0;i< size;i++){
        xref_entry *entry = &ctx->xref[i + start];
        unsigned char free;

        if (entry->object_num != 0)
            continue;

        bytes = pdf_read_bytes(ctx, (byte *)Buffer, 1, 20, s);
        if (bytes < 20)
            return_error(gs_error_ioerror);
        j = 19;
        while (Buffer[j] != 0x0D && Buffer[j] != 0x0A) {
            pdf_unread(ctx, (byte *)&Buffer[j], 1);
            j--;
        }
        Buffer[j] = 0x00;
        sscanf(Buffer, "%ld %d %c", &entry->offset, &entry->generation_num, &free);
        entry->compressed = false;
        entry->object_num = i + start;
        if (free == 'f')
            entry->free = true;
        if(free == 'n')
            entry->free = false;
    }

    /* read trailer dict */
    code = pdf_read_token(ctx, ctx->main_stream);
    if (code < 0)
        return code;

    o = ctx->stack_top[-1];
    if (o->type != PDF_KEYWORD || ((pdf_keyword *)o)->key != PDF_TRAILER) {
        pdf_pop(ctx, 1);
        return_error(gs_error_syntaxerror);
    }
    pdf_pop(ctx, 1);

    code = pdf_read_token(ctx, ctx->main_stream);
    if (code < 0)
        return code;

    d = (pdf_dict *)ctx->stack_top[-1];
    if (d->object.type != PDF_DICT) {
        pdf_pop(ctx, 1);
        return_error(gs_error_typecheck);
    }

    if (ctx->Trailer == NULL) {
        ctx->Trailer = d;
        d->object.refcnt++;
    }

    /* We have the Trailer dictionary. First up check for hybrid files. These have the initial
     * xref starting at 0 and size of 0. In this case the /Size entry in the trailer dictionary
     * must tell us how large the xref is, and we need to allocate our xref table anyway.
     */
    if (ctx->xref == NULL && size == 0) {
        code = pdf_dict_get(d, "Size", &o);
        if (code < 0) {
            pdf_pop(ctx, 2);
            return code;
        }
        if (o->type != PDF_INT) {
            pdf_pop(ctx, 2);
            return_error(gs_error_typecheck);
        }
        ctx->xref = (xref_entry *)gs_alloc_bytes(ctx->memory, ((pdf_num *)o)->value.i * sizeof(xref_entry), "read_xref_stream allocate xref table");
        if (ctx->xref == NULL) {
            pdf_pop(ctx, 2);
            return_error(gs_error_VMerror);
        }

        memset(ctx->xref, 0x00, ((pdf_num *)o)->value.i * sizeof(xref_entry));
    }

    /* Now check if this is a hybrid file. */
    code = pdf_dict_get(d, "XRefStm", &o);
    if (code < 0 && code != gs_error_undefined) {
        pdf_pop(ctx, 2);
        return code;
    }
    if (code == 0) {
        pdf_pop(ctx, 2);
        /* Because of the way the code works when we read a file which is a pure
         * xref stream file, we need to read the first integer of 'x y obj'
         * because the xref stream decoding code expects that to be on the stack.
         */
        if (o->type != PDF_INT)
            return_error(gs_error_typecheck);

        pdf_seek(ctx, s, ((pdf_num *)o)->value.i, SEEK_SET);
        code = pdf_read_token(ctx, ctx->main_stream);
        if (code < 0)
            return code;

        if (o->type != PDF_INT) {
            pdf_pop(ctx, 1);
            return_error(gs_error_typecheck);
        }

        code = pdf_read_xref_stream_dict(ctx, ctx->main_stream);
        return code;
    }

    /* Not a hybrid file, so now check if this is a modified file and has
     * previous xref entries.
     */
    code = pdf_dict_get(d, "Prev", &o);
    if (code < 0) {
        pdf_pop(ctx, 2);
        if (code == gs_error_undefined)
            return 0;
        else
            return code;
    }
    pdf_pop(ctx, 1);

    if (o->type != PDF_INT)
        return_error(gs_error_typecheck);

    code = pdf_seek(ctx, s, ((pdf_num *)o)->value.i , SEEK_SET);
    if (code < 0)
        return code;

    code = pdf_read_token(ctx, ctx->main_stream);
    if (code < 0)
        return(code);

    if (((pdf_obj *)ctx->stack_top[-1])->type == PDF_KEYWORD && ((pdf_keyword *)ctx->stack_top[-1])->key == PDF_XREF) {
        /* Read old-style xref table */
        pdf_pop(ctx, 1);
        return(read_xref(ctx, ctx->main_stream));
    } else {
        return_error(gs_error_typecheck);
    }
}

int open_pdf_file(pdf_context *ctx, char *filename)
{
    byte *Buffer = NULL;
    char *s = NULL;
    float version = 0.0;
    gs_offset_t Offset = 0, bytes = 0;
    int code = 0;

    ctx->main_stream = sfopen(filename, "r", ctx->memory);
    if (ctx->main_stream == NULL)
        return_error(gs_error_ioerror);

    Buffer = gs_alloc_bytes(ctx->memory, BUF_SIZE, "PDF interpreter - allocate working buffer for file validation");
    if (Buffer == NULL) {
        cleanup_pdf_open_file(ctx, Buffer);
        return_error(gs_error_VMerror);
    }

    bytes = pdf_read_bytes(ctx, Buffer, 1, BUF_SIZE, ctx->main_stream);
    if (bytes == 0) {
        cleanup_pdf_open_file(ctx, Buffer);
        return_error(gs_error_ioerror);
    }

    /* First check for existence of header */
    s = strstr((char *)Buffer, "%PDF");
    if (s == NULL) {
        emprintf1(ctx->memory, "File %s does not appear to be a PDF file (no %PDF in first 2Kb of file)\n", filename);
    } else {
        /* Now extract header version (may be overridden later) */
        if (sscanf(s + 5, "%f", &version) != 1) {
            emprintf(ctx->memory, "Unable to read PDF version from header\n");
            ctx->HeaderVersion = 0;
        }
        else {
            ctx->HeaderVersion = version;
        }
    }

    /* Jump to EOF and scan backwards looking for startxref */
    pdf_seek(ctx, ctx->main_stream, 0, SEEK_SET);
    pdf_seek(ctx, ctx->main_stream, 0, SEEK_END);
    ctx->main_stream_length = stell(ctx->main_stream);
    Offset = BUF_SIZE;
    bytes = BUF_SIZE;

    do {
        byte *last_lineend = NULL;
        uint32_t read;

        if (pdf_seek(ctx, ctx->main_stream, ctx->main_stream_length - Offset, SEEK_SET) != 0) {
            cleanup_pdf_open_file(ctx, Buffer);
            return_error(gs_error_ioerror);
        }
        read = pdf_read_bytes(ctx, Buffer, 1, bytes, ctx->main_stream);

        if (read == 0) {
            cleanup_pdf_open_file(ctx, Buffer);
            return_error(gs_error_ioerror);
        }

        read = bytes = read + (BUF_SIZE - bytes);

        while(read) {
            if (memcmp(Buffer + read - 9, "startxref", 9) == 0) {
                break;
            } else {
                if (Buffer[read] == 0x0a || Buffer[read] == 0x0d)
                    last_lineend = Buffer + read;
            }
            read--;
        }
        if (memcmp(Buffer + read - 9, "startxref", 9) == 0) {
            byte *b = Buffer + read;

            if(sscanf((char *)b, " %ld", &ctx->startxref) != 1) {
                emprintf(ctx->memory, "Unable to find token 'startxref' in PDF file\n");
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

    if (ctx->startxref != 0) {
        uint32_t index = 0;

        /* Read the xref(s) */
        pdf_seek(ctx, ctx->main_stream, ctx->startxref, SEEK_SET);

        code = pdf_read_token(ctx, ctx->main_stream);
        if (code < 0)
            return(repair_pdf_file(ctx));

        if (((pdf_obj *)ctx->stack_top[-1])->type == PDF_KEYWORD && ((pdf_keyword *)ctx->stack_top[-1])->key == PDF_XREF) {
            /* Read old-style xref table */
            pdf_pop(ctx, 1);
            code = read_xref(ctx, ctx->main_stream);
            if (code < 0)
                return(repair_pdf_file(ctx));
        } else {
            code = pdf_read_xref_stream_dict(ctx, ctx->main_stream);
            if (code < 0)
                return(repair_pdf_file(ctx));
        }
    } else {
        /* Attempt to repair PDF file */
        return(repair_pdf_file(ctx));
    }

    gs_free_object(ctx->memory, Buffer, "PDF interpreter - allocate working buffer for file validation");
    return 0;
}

/* These functions are used by the 'PL' implementation, eventually we will */
/* need to have custom PostScript operators to process the file or at      */
/* (least pages from it).                                                  */

int close_pdf_file(pdf_context *ctx)
{
    sfclose(ctx->main_stream);
    ctx->main_stream = NULL;
    return 0;
}

int pdf_process_file(pdf_context *ctx, char *filename)
{
    int code = 0;

    code = open_pdf_file(ctx, filename);
    if (code < 0)
        return code;

    code = close_pdf_file(ctx);
    return code;
}

