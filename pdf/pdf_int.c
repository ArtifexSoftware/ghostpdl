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
#include "stream.h"

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
        gs_free_object(n->memory, n->data, "pdf interpreter free name or string data");
    gs_free_object(n->memory, n, "pdf interpreter free name or string");
}

static void pdf_free_keyword(pdf_obj *o)
{
    /* Currently names and strings are the same, so a single cast is OK */
    pdf_keyword *k = (pdf_keyword *)o;

    if (k->data != NULL)
        gs_free_object(k->memory, k->data, "pdf interpreter free keyword data");
    gs_free_object(k->memory, k, "pdf interpreter free keyword");
}

static void pdf_free_array(pdf_obj *o)
{
    pdf_array *a = (pdf_array *)o;
    int i;

    for (i=0;i < a->entries;i++) {
        if (a->values[i] != NULL)
            pdf_countdown(a->values[i]);
    }
    gs_free_object(a->memory, a->values, "pdf interpreter free array contents");
    gs_free_object(a->memory, a, "pdf interpreter free array");
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
    gs_free_object(d->memory, d->keys, "pdf interpreter free dictionary keys");
    gs_free_object(d->memory, d->values, "pdf interpreter free dictioanry values");
    gs_free_object(d->memory, d, "pdf interpreter free dictionary");
}

static void pdf_free_xref_table(pdf_obj *o)
{
    xref_table *xref = (xref_table *)o;

    gs_free_object(xref->memory, xref->xref, "pdf_free_xref_table");
    gs_free_object(xref->memory, xref, "pdf_free_xref_table");
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
        case PDF_XREF_TABLE:
            pdf_free_xref_table(o);
            break;
        default:
#ifdef DEBUG
            dmprintf(o->memory, "!!! Attempting to free unknown obect type !!!\n");
#endif
            break;
    }
}

static int pdf_pop(pdf_context *ctx, int num)
{
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
    pdf_countup(o);

    return 0;
}

/***********************************************************************************/
/* Functions to dereference object references and manage the object cache          */

static int pdf_add_to_cache(pdf_context *ctx, pdf_obj *o)
{
    pdf_obj_cache_entry *entry;

    if (o->object_num > ctx->xref_table->xref_size)
        return_error(gs_error_rangecheck);

    if (ctx->cache_entries == MAX_OBJECT_CACHE_SIZE)
    {
        if (ctx->cache_LRU) {
            entry = ctx->cache_LRU;
            ctx->cache_LRU = entry->next;
            if (entry->next)
                ((pdf_obj_cache_entry *)entry->next)->previous = NULL;
            ctx->xref_table->xref[entry->o->object_num].cache = NULL;
            pdf_countdown(entry->o);
            ctx->cache_entries--;
            gs_free_object(ctx->memory, entry, "pdf_add_to_cache, free LRU");
        } else
            return_error(gs_error_unknownerror);
    }
    entry = (pdf_obj_cache_entry *)gs_alloc_bytes(ctx->memory, sizeof(pdf_obj_cache_entry), "pdf_add_to_cache");
    if (entry == NULL)
        return_error(gs_error_VMerror);

    memset(entry, 0x00, sizeof(pdf_obj_cache_entry));

    entry->o = o;
    pdf_countup(o);
    if (ctx->cache_MRU) {
        entry->previous = ctx->cache_MRU;
        ctx->cache_MRU->next = entry;
    }
    ctx->cache_MRU = entry;
    if (ctx->cache_LRU == NULL)
        ctx->cache_LRU = entry;

    ctx->cache_entries++;
    ctx->xref_table->xref[o->object_num].cache = entry;
    return 0;
}

/* pdf_dereference returns an object with a reference count of at least 1, this represents the
 * reference being held by the caller (in **object) when we return from this function.
 */
int pdf_dereference(pdf_context *ctx, uint64_t obj, uint64_t gen, pdf_obj **object)
{
    xref_entry *entry;
    pdf_obj *o;
    int code;

    if (obj > ctx->xref_table->xref_size)
        return_error(gs_error_rangecheck);

    entry = &ctx->xref_table->xref[obj];

    if (entry->cache != NULL){
        pdf_obj_cache_entry *cache_entry = entry->cache;

        *object = cache_entry->o;
        if (ctx->cache_MRU) {
            ((pdf_obj_cache_entry *)cache_entry->next)->previous = cache_entry->previous;
            ((pdf_obj_cache_entry *)cache_entry->previous)->next = cache_entry->next;
            cache_entry->next = NULL;
            cache_entry->previous = ctx->cache_MRU;
            ctx->cache_MRU->next = cache_entry;
            ctx->cache_MRU = cache_entry;
        } else
            return_error(gs_error_unknownerror);
    } else {
        if (entry->compressed) {
            /* This is an object in a compressed object stream */

            xref_entry *compressed_entry = &ctx->xref_table->xref[entry->u.compressed.compressed_stream_num];
            pdf_dict *compressed_object;
            pdf_stream *compressed_stream;
            char Buffer[256];
            int i = 0;
            uint64_t num_entries;
            gs_offset_t offset;

            if (ctx->pdfdebug) {
                dmprintf1(ctx->memory, "%% Reading compressed object %"PRIi64, obj);
                dmprintf1(ctx->memory, " from ObjStm with object number %"PRIi64"\n", compressed_entry->object_num);
            }

            if (compressed_entry->cache == NULL) {
                code = pdf_seek(ctx, ctx->main_stream, compressed_entry->u.uncompressed.offset, SEEK_SET);
                if (code < 0)
                    return code;

                code = pdf_read_object(ctx, ctx->main_stream);
                if (code < 0)
                    return code;

                compressed_object = (pdf_dict *)ctx->stack_top[-1];
                pdf_countup((pdf_obj *)compressed_object);
                pdf_pop(ctx, 1);
            } else {
                compressed_object = (pdf_dict *)compressed_entry->cache->o;
                pdf_countup((pdf_obj *)compressed_object);
            }
            /* Check its an ObjStm ! */
            code = pdf_dict_get(compressed_object, "Type", &o);
            if (code < 0) {
                pdf_countdown((pdf_obj *)compressed_object);
                return code;
            }
            if (o->type != PDF_NAME) {
                pdf_countdown(o);
                pdf_countdown((pdf_obj *)compressed_object);
                return_error(gs_error_typecheck);
            }
            if (((pdf_name *)o)->length != 6 || memcmp(((pdf_name *)o)->data, "ObjStm", 6) != 0){
                pdf_countdown(o);
                pdf_countdown((pdf_obj *)compressed_object);
                return_error(gs_error_syntaxerror);
            }
            pdf_countdown(o);

            /* Need to check the /N entry to see if the object is actually in this stream! */
            code = pdf_dict_get(compressed_object, "N", &o);
            if (code < 0) {
                pdf_countdown((pdf_obj *)compressed_object);
                return code;
            }
            if (o->type != PDF_INT) {
                pdf_countdown(o);
                pdf_countdown((pdf_obj *)compressed_object);
                return_error(gs_error_typecheck);
            }
            num_entries = ((pdf_num *)o)->value.i;
            if (num_entries <= entry->u.compressed.object_index){
                pdf_countdown(o);
                pdf_countdown((pdf_obj *)compressed_object);
                return_error(gs_error_rangecheck);
            }
            pdf_countdown(o);

            code = pdf_seek(ctx, ctx->main_stream, compressed_object->stream_offset, SEEK_SET);
            if (code < 0) {
                pdf_countdown((pdf_obj *)compressed_object);
                return code;
            }

            code = pdf_filter(ctx, compressed_object, ctx->main_stream, &compressed_stream);
            if (code < 0) {
                pdf_countdown((pdf_obj *)compressed_object);
                return code;
            }

            for (i=0;i < num_entries;i++)
            {
                code = pdf_read_token(ctx, compressed_stream);
                if (code < 0) {
                    sclose(compressed_stream->s);
                    pdf_countdown((pdf_obj *)compressed_object);
                    return code;
                }
                o = ctx->stack_top[-1];
                if (((pdf_obj *)o)->type != PDF_INT) {
                    pdf_pop(ctx, 1);
                    sclose(compressed_stream->s);
                    pdf_countdown((pdf_obj *)compressed_object);
                    return_error(gs_error_typecheck);
                }
                pdf_pop(ctx, 1);
                code = pdf_read_token(ctx, compressed_stream);
                if (code < 0) {
                    sclose(compressed_stream->s);
                    pdf_countdown((pdf_obj *)compressed_object);
                    return code;
                }
                o = ctx->stack_top[-1];
                if (((pdf_obj *)o)->type != PDF_INT) {
                    pdf_pop(ctx, 1);
                    sclose(compressed_stream->s);
                    pdf_countdown((pdf_obj *)compressed_object);
                    return_error(gs_error_typecheck);
                }
                if (i == entry->u.compressed.object_index)
                    offset = ((pdf_num *)o)->value.i;
                pdf_pop(ctx, 1);
            }

            for (i=0;i < offset;i++)
            {
                code = pdf_read_bytes(ctx, (byte *)&Buffer[0], 1, 1, compressed_stream);
            }

            code = pdf_read_token(ctx, compressed_stream);
            if (code < 0) {
                sclose(compressed_stream->s);
                pdf_countdown((pdf_obj *)compressed_object);
                return code;
            }

            sclose(compressed_stream->s);

            *object = ctx->stack_top[-1];
            pdf_countup(*object);
            pdf_pop(ctx, 1);

            pdf_countdown((pdf_obj *)compressed_object);
        } else {
            code = pdf_seek(ctx, ctx->main_stream, entry->u.uncompressed.offset, SEEK_SET);
            if (code < 0)
                return code;

            code = pdf_read_object(ctx, ctx->main_stream);
            if (code < 0)
                return code;

            *object = ctx->stack_top[-1];
            pdf_countup(*object);
            pdf_pop(ctx, 1);
        }
    }

    return 0;
}

/***********************************************************************************/
/* 'token' reading functions. Tokens in this sense are PDF logical objects and the */
/* related keywords. So that's numbers, booleans, names, strings, dictionaries,    */
/* arrays, the  null object and indirect references. The keywords are obj/endobj   */
/* stream/endstream, xref, startxref and trailer.                                  */

/* The 'read' functions all return the newly created object on the context's stack
 * which means these objects are created with a reference count of 0, and only when
 * pushed onto the stack does the reference count become 1, indicating the stack is
 * the only reference.
 */
static void skip_white(pdf_context *ctx,pdf_stream *s)
{
    uint32_t bytes = 0, read = 0;
    byte c;

    do {
        bytes = pdf_read_bytes(ctx, &c, 1, 1, s);
        read += bytes;
    } while (bytes != 0 && iswhite(c));

    if (read > 0)
        pdf_unread(ctx, s, &c, 1);
}

static int pdf_read_num(pdf_context *ctx, pdf_stream *s)
{
    byte Buffer[256];
    unsigned short index = 0, bytes;
    bool real = false;
    pdf_num *num;
    int code = 0;

    skip_white(ctx, s);

    do {
        bytes = pdf_read_bytes(ctx, (byte *)&Buffer[index], 1, 1, s);
        if (bytes < 0)
            return_error(gs_error_ioerror);

        if (iswhite((char)Buffer[index])) {
            Buffer[index] = 0x00;
            break;
        } else {
            if (isdelimiter((char)Buffer[index])) {
                pdf_unread(ctx, s, (byte *)&Buffer[index], 1);
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
    num->memory = ctx->memory;

    if (real) {
        num->type = PDF_REAL;
        if (sscanf((const char *)Buffer, "%f", &num->value.d) == 0) {
            if (ctx->pdfdebug)
                dmprintf1(ctx->memory, "failed to read real number : %s\n", Buffer);
            gs_free_object(num->memory, num, "pdf_read_num error");
            return_error(gs_error_syntaxerror);
        }
    } else {
        num->type = PDF_INT;
        if (sscanf((const char *)Buffer, "%d", &num->value.i) == 0) {
            if (ctx->pdfdebug)
                dmprintf1(ctx->memory, "failed to read integer : %s\n", Buffer);
            gs_free_object(num->memory, num, "pdf_read_num error");
            return_error(gs_error_syntaxerror);
        }
    }

    if (ctx->pdfdebug) {
        if (real)
            dmprintf1(ctx->memory, " %f", num->value.d);
        else
            dmprintf1(ctx->memory, " %"PRIi64, num->value.i);
    }

    code = pdf_push(ctx, (pdf_obj *)num);

    if (code < 0)
        pdf_free_object((pdf_obj *)num);

    return code;
}

static int pdf_read_name(pdf_context *ctx, pdf_stream *s)
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
                pdf_unread(ctx, s, (byte *)&Buffer[index], 1);
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
    name->memory = ctx->memory;
    name->type = PDF_NAME;

    NewBuf = (char *)gs_alloc_bytes(ctx->memory, index, "pdf_read_name");
    if (NewBuf == NULL) {
        gs_free_object(ctx->memory, Buffer, "pdf_read_name error");
        return_error(gs_error_VMerror);
    }
    memcpy(NewBuf, Buffer, index);

    if (ctx->pdfdebug)
        dmprintf1(ctx->memory, " /%s", Buffer);

    gs_free_object(ctx->memory, Buffer, "pdf_read_name");

    name->data = (unsigned char *)NewBuf;
    name->length = index;

    code = pdf_push(ctx, (pdf_obj *)name);

    if (code < 0)
        pdf_free_namestring((pdf_obj *)name);

    return code;
}

static int pdf_read_hexstring(pdf_context *ctx, pdf_stream *s)
{
    char *Buffer, *NewBuf = NULL, HexBuf[2];
    unsigned short index = 0, bytes = 0;
    uint32_t size = 256;
    pdf_string *string = NULL;
    int code;

    Buffer = (char *)gs_alloc_bytes(ctx->memory, size, "pdf_read_hexstring");
    if (Buffer == NULL)
        return_error(gs_error_VMerror);

    if (ctx->pdfdebug)
        dmprintf(ctx->memory, " <");

    do {
        bytes = pdf_read_bytes(ctx, (byte *)HexBuf, 1, 1, s);
        if (bytes == 0)
            return_error(gs_error_ioerror);

        if (HexBuf[0] == '>')
            break;

        if (ctx->pdfdebug)
            dmprintf1(ctx->memory, "%c", HexBuf[0]);

        bytes = pdf_read_bytes(ctx, (byte *)&HexBuf[1], 1, 1, s);
        if (bytes == 0)
            return_error(gs_error_ioerror);

        if (!ishex(HexBuf[0]) || !ishex(HexBuf[1]))
            return_error(gs_error_syntaxerror);

        if (ctx->pdfdebug)
            dmprintf1(ctx->memory, "%c", HexBuf[1]);

        Buffer[index] = (fromhex(HexBuf[0]) << 4) + fromhex(HexBuf[1]);

        if (index++ >= size) {
            NewBuf = (char *)gs_alloc_bytes(ctx->memory, size + 256, "pdf_read_hexstring");
            if (NewBuf == NULL) {
                gs_free_object(ctx->memory, Buffer, "pdf_read_hexstring error");
                return_error(gs_error_VMerror);
            }
            memcpy(NewBuf, Buffer, size);
            gs_free_object(ctx->memory, Buffer, "pdf_read_hexstring");
            Buffer = NewBuf;
            size += 256;
        }
    } while(1);

    if (ctx->pdfdebug)
        dmprintf(ctx->memory, ">");

    string = (pdf_string *)gs_alloc_bytes(ctx->memory, sizeof(pdf_string), "pdf_read_hexstring");
    if (string == NULL) {
        gs_free_object(ctx->memory, Buffer, "pdf_read_hexstring");
        return_error(gs_error_VMerror);
    }

    memset(string, 0x00, sizeof(pdf_string));
    string->memory = ctx->memory;
    string->type = PDF_STRING;

    NewBuf = (char *)gs_alloc_bytes(ctx->memory, index, "pdf_read_hexstring");
    if (NewBuf == NULL) {
        gs_free_object(ctx->memory, Buffer, "pdf_read_hexstring error");
        return_error(gs_error_VMerror);
    }
    memcpy(NewBuf, Buffer, index);
    gs_free_object(ctx->memory, Buffer, "pdf_read_hexstring");

    string->data = (unsigned char *)NewBuf;
    string->length = index;

    code = pdf_push(ctx, (pdf_obj *)string);
    if (code < 0)
        pdf_free_namestring((pdf_obj *)string);

    return code;
}

static int pdf_read_string(pdf_context *ctx, pdf_stream *s)
{
    char *Buffer, *NewBuf = NULL, octal[3];
    unsigned short index = 0, bytes = 0;
    uint32_t size = 256;
    pdf_string *string = NULL;
    int code, octal_index = 0;
    bool escape = false, skip_eol = false, exit_loop = false;

    Buffer = (char *)gs_alloc_bytes(ctx->memory, size, "pdf_read_string");
    if (Buffer == NULL)
        return_error(gs_error_VMerror);

    do {
        bytes = pdf_read_bytes(ctx, (byte *)&Buffer[index], 1, 1, s);

        if (bytes == 0) {
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
                code = pdf_unread(ctx, s, dummy, 2);
                if (code < 0) {
                    gs_free_object(ctx->memory, Buffer, "pdf_read_string");
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
                        Buffer[index] = 0x0a;
                        break;
                    case 't':
                        Buffer[index] = 0x09;
                        break;
                    case 'b':
                        Buffer[index] = 0x07;
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
                        gs_free_object(ctx->memory, Buffer, "pdf_read_string");
                        return_error(gs_error_syntaxerror);
                        break;
                }
            }
        } else {
            switch(Buffer[index]) {
                case 0x0a:
                case 0x0d:
                    if (octal_index != 0) {
                        code = pdf_unread(ctx, s, (byte *)&Buffer[index], 1);
                        if (code < 0) {
                            gs_free_object(ctx->memory, Buffer, "pdf_read_string");
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
                        code = pdf_unread(ctx, s, (byte *)&Buffer[index], 1);
                        if (code < 0) {
                            gs_free_object(ctx->memory, Buffer, "pdf_read_string");
                            return code;
                        }
                        Buffer[index] = octal[0];
                        if (octal_index == 2)
                            Buffer[index] = (Buffer[index] * 8) + octal[1];
                        octal_index = 0;
                    } else {
                        Buffer[index] = 0x00;
                        exit_loop = true;
                    }
                    break;
                case '\\':
                    escape = true;
                    continue;
                default:
                    if (octal_index) {
                        if (Buffer[index] >= 0x30 && Buffer[index] <= 0x37) {
                            octal[octal_index] = Buffer[index] - 0x30;
                            if (++octal_index < 3)
                                continue;
                            Buffer[index] = (octal[0] * 64) + (octal[1] * 8) + octal[2];
                            octal_index = 0;
                        } else {
                            code = pdf_unread(ctx, s, (byte *)&Buffer[index], 1);
                            if (code < 0) {
                                gs_free_object(ctx->memory, Buffer, "pdf_read_string");
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
    string->memory = ctx->memory;
    string->type = PDF_STRING;

    NewBuf = (char *)gs_alloc_bytes(ctx->memory, index, "pdf_read_string");
    if (NewBuf == NULL) {
        gs_free_object(ctx->memory, Buffer, "pdf_read_string error");
        return_error(gs_error_VMerror);
    }
    memcpy(NewBuf, Buffer, index);
    gs_free_object(ctx->memory, Buffer, "pdf_read_string");

    string->data = (unsigned char *)NewBuf;
    string->length = index;

    if (ctx->pdfdebug) {
        int i;
        dmprintf(ctx->memory, " (");
        for (i=0;i<string->length;i++)
            dmprintf1(ctx->memory, "%c", string->data[i]);
        dmprintf(ctx->memory, ")");
    }

    code = pdf_push(ctx, (pdf_obj *)string);
    if (code < 0)
        pdf_free_namestring((pdf_obj *)string);

    return code;
}

static int pdf_read_array(pdf_context *ctx, pdf_stream *s)
{
    unsigned short index = 0, bytes = 0;
    byte Buffer;
    pdf_array *a = NULL;
    pdf_obj *o;
    pdf_obj **stack_mark = ctx->stack_top;
    int code;

    if (ctx->pdfdebug)
        dmprintf (ctx->memory, " [");

    do {
        skip_white(ctx, s);

        bytes = pdf_read_bytes(ctx, &Buffer, 1, 1, s);
        
        if (bytes == 0)
            return_error(gs_error_ioerror);

        if (Buffer == ']') {
            break;
        } else {
            pdf_unread(ctx, s, &Buffer, 1);
            code = pdf_read_token(ctx, s);
            if (code < 0)
                return code;
        }
    } while (1);

    index = ctx->stack_top - stack_mark;

    a = (pdf_array *)gs_alloc_bytes(ctx->memory, sizeof(pdf_array), "pdf_read_array");
    if (a == NULL)
        return_error(gs_error_VMerror);

    memset(a, 0x00, sizeof(pdf_array));
    a->memory = ctx->memory;
    a->type = PDF_ARRAY;

    a->size = a->entries = index;

    a->values = (pdf_obj **)gs_alloc_bytes(ctx->memory, index * sizeof(pdf_obj *), "pdf_read_array");
    if (a->values == NULL) {
        gs_free_object(a->memory, a, "pdf_read_array error");
        return_error(gs_error_VMerror);
    }

    while (index) {
        o = ctx->stack_top[-1];
        a->values[--index] = o;
        pdf_countup(o);
        pdf_pop(ctx, 1);
    }

    if (ctx->pdfdebug)
        dmprintf (ctx->memory, " ]\n");

    code = pdf_push(ctx, (pdf_obj *)a);
    if (code < 0)
        pdf_free_array((pdf_obj *)a);

    return code;
}

static int pdf_read_dict(pdf_context *ctx, pdf_stream *s)
{
    unsigned short index = 0, bytes = 0;
    byte Buffer[2];
    pdf_dict *d = NULL;
    uint64_t i = 0;
    int code;
    pdf_obj **stack_mark = ctx->stack_top;

    if (ctx->pdfdebug)
        dmprintf (ctx->memory, " <<\n");
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

            pdf_unread(ctx, s, &Buffer[1], 1);
        } 

        pdf_unread(ctx, s, &Buffer[0], 1);
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
    d->memory = ctx->memory;
    d->type = PDF_DICT;

    d->size = d->entries = index >> 1;

    d->keys = (pdf_obj **)gs_alloc_bytes(ctx->memory, d->size * sizeof(pdf_obj *), "pdf_read_dict");
    if (d->keys == NULL) {
        gs_free_object(d->memory, d, "pdf_read_dict error");
        return_error(gs_error_VMerror);
    }

    d->values = (pdf_obj **)gs_alloc_bytes(ctx->memory, d->size * sizeof(pdf_obj *), "pdf_read_dict");
    if (d->values == NULL) {
        gs_free_object(d->memory, d->keys, "pdf_read_dict error");
        gs_free_object(d->memory, d, "pdf_read_dict error");
        return_error(gs_error_VMerror);
    }
    
    while (index) {
        i = (index / 2) - 1;

        /* In PDF keys are *required* to be names, so we ought to check that here */
        if (((pdf_obj *)ctx->stack_top[-2])->type == PDF_NAME) {
            d->keys[i] = ctx->stack_top[-2];
            pdf_countup(d->keys[i]);
            d->values[i] = ctx->stack_top[-1];
            pdf_countup(d->values[i]);
        } else {
            pdf_free_dict((pdf_obj *)d);
            return_error(gs_error_typecheck);
        }

        pdf_pop(ctx, 2);
        index -= 2;
    }

    if (ctx->pdfdebug)
        dmprintf (ctx->memory, "\n >>\n");

    code = pdf_push(ctx, (pdf_obj *)d);
    if (code < 0)
        pdf_free_dict((pdf_obj *)d);

    return code;
}

/* The object returned by pdf_dict_get has its reference count incremented by 1 to
 * indicate the reference now held by the caller, in **o.
 */
int pdf_dict_get(pdf_dict *d, const char *Key, pdf_obj **o)
{
    int i=0;
    pdf_obj *t;

    *o = NULL;

    for (i=0;i< d->entries;i++) {
        t = d->keys[i];

        if (t && t->type == PDF_NAME) {
            if (((pdf_name *)t)->length == strlen((const char *)Key) && memcmp((const char *)((pdf_name *)t)->data, (const char *)Key, ((pdf_name *)t)->length) == 0) {
                *o = d->values[i];
                pdf_countup(*o);
                return 0;
            }
        }
    }
    return_error(gs_error_undefined);
}

int pdf_dict_put(pdf_dict *d, pdf_obj *Key, pdf_obj *value)
{
    uint64_t i;
    pdf_obj *t;
    pdf_obj **new_keys, **new_values;

    /* First, do we have a Key/value pair already ? */
    for (i=0;i< d->entries;i++) {
        t = d->keys[i];
        if (t && t->type == PDF_NAME) {
            if (((pdf_name *)t)->length == ((pdf_name *)Key)->length && memcmp((const char *)((pdf_name *)t)->data, ((pdf_name *)Key)->data, ((pdf_name *)t)->length) == 0) {
                if (d->values[i] == value)
                    /* We already have this value stored with this key.... */
                    return 0;
                pdf_countdown(d->values[i]);
                d->values[i] = value;
                pdf_countup(value);
                return 0;
            }
        }
    }

    /* Nope, its a new Key */
    if (d->size > d->entries) {
        /* We have a hole, find and use it */
        for (i=0;i< d->entries;i++) {
            if (d->keys[i] == NULL) {
                d->keys[i] = Key;
                pdf_countup(Key);
                d->values[i] = value;
                pdf_countup(value);
                return 0;
            }
        }
    }

    new_keys = (pdf_obj **)gs_alloc_bytes(d->memory, (d->size + 1) * sizeof(pdf_obj *), "pdf_dict_put reallocate dictionary keys");
    new_values = (pdf_obj **)gs_alloc_bytes(d->memory, (d->size + 1) * sizeof(pdf_obj *), "pdf_dict_put reallocate dictionary values");
    if (new_keys == NULL || new_values == NULL){
        gs_free_object(d->memory, new_keys, "pdf_dict_put memory allocation failure");
        gs_free_object(d->memory, new_values, "pdf_dict_put memory allocation failure");
        return_error(gs_error_VMerror);
    }
    memcpy(new_keys, d->keys, d->size * sizeof(pdf_obj *));
    memcpy(new_keys, d->values, d->size * sizeof(pdf_obj *));

    gs_free_object(d->memory, d->keys, "pdf_dict_put key reallocation");
    gs_free_object(d->memory, d->values, "pdf_dict_put value reallocation");

    d->keys[d->size] = Key;
    d->values[d->size] = value;
    d->size++;
    d->entries++;
    pdf_countup(Key);
    pdf_countup(value);

    return 0;
}

/* The object returned by pdf_array_get has its reference count incremented by 1 to
 * indicate the reference now held by the caller, in **o.
 */
int pdf_array_get(pdf_array *a, uint64_t index, pdf_obj **o)
{
    if (index > a->size)
        return_error(gs_error_rangecheck);

    *o = a->values[index];
    pdf_countup(*o);
    return 0;
}

static void pdf_skip_comment(pdf_context *ctx, pdf_stream *s)
{
    byte Buffer;
    unsigned short bytes = 0;

    if (ctx->pdfdebug)
        dmprintf (ctx->memory, " %%");

    do {
        bytes = pdf_read_bytes(ctx, (byte *)&Buffer, 1, 1, s);
        if (ctx->pdfdebug)
            dmprintf1 (ctx->memory, " %c", Buffer);

        if ((Buffer = 0x0A) || (Buffer == 0x0D)) {
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
static int pdf_read_keyword(pdf_context *ctx, pdf_stream *s)
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
                pdf_unread(ctx, s, (byte *)&Buffer[index], 1);
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
    keyword->memory = ctx->memory;
    keyword->type = PDF_KEYWORD;

    memcpy(keyword->data, Buffer, index);
    keyword->length = index;
    keyword->key = PDF_NOT_A_KEYWORD;

    if (ctx->pdfdebug)
        dmprintf1(ctx->memory, " %s\n", Buffer);

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
                o->memory = ctx->memory;
                o->type = PDF_INDIRECT;
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
int pdf_read_token(pdf_context *ctx, pdf_stream *s)
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
            pdf_unread(ctx, s, (byte *)&Buffer[0], 1);
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
                    pdf_unread(ctx, s, (byte *)&Buffer[1], 1);
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
            pdf_unread(ctx, s, (byte *)&Buffer[0], 1);
            return pdf_read_keyword(ctx, s);
            break;
    }
    return 0;
}

int pdf_read_object(pdf_context *ctx, pdf_stream *s)
{
    int code = 0;
    uint64_t objnum = 0, gen = 0;
    pdf_keyword *keyword = NULL;

    /* An object consists of 'num gen obj' followed by a token, follwed by an endobj
     * A stream dictionary might have a 'stream' instead of an 'endobj', in which case we
     * want to deal with it specially by getting the Length, jumping to the end and checking
     * for an endobj. Or not, possibly, because it would be slow.
     */
    code = pdf_read_token(ctx, s);
    if (code < 0)
        return code;
    if (((pdf_obj *)ctx->stack_top[-1])->type != PDF_INT) {
        pdf_pop(ctx, 1);
        return_error(gs_error_typecheck);
    }
    objnum = ((pdf_num *)ctx->stack_top[-1])->value.i;
    pdf_pop(ctx, 1);

    code = pdf_read_token(ctx, s);
    if (code < 0)
        return code;
    if (((pdf_obj *)ctx->stack_top[-1])->type != PDF_INT) {
        pdf_pop(ctx, 1);
        return_error(gs_error_typecheck);
    }
    gen = ((pdf_num *)ctx->stack_top[-1])->value.i;
    pdf_pop(ctx, 1);

    code = pdf_read_token(ctx, s);
    if (code < 0)
        return code;
    if (((pdf_obj *)ctx->stack_top[-1])->type != PDF_KEYWORD) {
        pdf_pop(ctx, 1);
        return_error(gs_error_typecheck);
    }
    keyword = ((pdf_keyword *)ctx->stack_top[-1]);
    if (keyword->key != PDF_OBJ) {
        pdf_pop(ctx, 1);
        return_error(gs_error_syntaxerror);
    }
    pdf_pop(ctx, 1);

    code = pdf_read_token(ctx, s);
    if (code < 0)
        return code;

    code = pdf_read_token(ctx, s);
    if (((pdf_obj *)ctx->stack_top[-1])->type != PDF_KEYWORD) {
        pdf_pop(ctx, 1);
        return_error(gs_error_typecheck);
    }
    keyword = ((pdf_keyword *)ctx->stack_top[-1]);
    if (keyword->key == PDF_ENDOBJ) {
        pdf_obj *o = ctx->stack_top[-2];

        pdf_pop(ctx, 1);

        o->object_num = objnum;
        o->generation_num = gen;
        pdf_add_to_cache(ctx, o);
        return 0;
    }
    if (keyword->key == PDF_STREAM) {
        pdf_obj *o = NULL;
        pdf_dict *d = (pdf_dict *)ctx->stack_top[-2];
        gs_offset_t offset;

        skip_white(ctx, ctx->main_stream);

        offset = (gs_offset_t)stell(ctx->main_stream->s) - 1;

        pdf_pop(ctx, 1);
        d = (pdf_dict *)ctx->stack_top[-1];

        if (d->type != PDF_DICT) {
            pdf_pop(ctx, 1);
            return_error(gs_error_syntaxerror);
        }
        d->object_num = objnum;
        d->generation_num = gen;
        d->stream_offset = offset;
        code = pdf_add_to_cache(ctx, (pdf_obj *)d);

        /* This code may be a performance overhead, it simply skips over the stream contents
         * and checks that the stream ends with a 'endstream endobj' pair. We could add a
         * 'go faster' flag for users who are certain their PDF files are well-formed. This
         * could also allow us to skip all kinds of other checking.....
         */

        code = pdf_dict_get(d, "Length", &o);
        if (code < 0) {
            pdf_pop(ctx, 1);
            return code;
        }
        if (o->type != PDF_INT) {
            pdf_countdown(o);
            pdf_pop(ctx, 1);
            return_error(gs_error_typecheck);
        }

        code = pdf_seek(ctx, ctx->main_stream, ((pdf_num *)o)->value.i, SEEK_CUR);
        pdf_countdown(o);
        if (code < 0) {
            pdf_pop(ctx, 1);
            return code;
        }

        code = pdf_read_token(ctx, s);
        if (((pdf_obj *)ctx->stack_top[-1])->type != PDF_KEYWORD) {
            pdf_pop(ctx, 2);
            return_error(gs_error_typecheck);
        }
        keyword = ((pdf_keyword *)ctx->stack_top[-1]);
        if (keyword->key != PDF_ENDSTREAM) {
            pdf_pop(ctx, 2);
            return_error(gs_error_typecheck);
        }
        pdf_pop(ctx, 1);

        code = pdf_read_token(ctx, s);
        if (((pdf_obj *)ctx->stack_top[-1])->type != PDF_KEYWORD) {
            pdf_pop(ctx, 2);
            return_error(gs_error_typecheck);
        }
        keyword = ((pdf_keyword *)ctx->stack_top[-1]);
        if (keyword->key != PDF_ENDOBJ) {
            pdf_pop(ctx, 2);
            return_error(gs_error_typecheck);
        }
        pdf_pop(ctx, 1);
        return 0;
    }
    pdf_pop(ctx, 2);
    return_error(gs_error_syntaxerror);
}

/* In contrast to the 'read' functions, the 'make' functions create an object with a
 * reference count of 1. This indicates that the caller holds the reference. Thus the
 * caller need not increment the reference count to the object, but must decrement
 * it (pdf_countdown) before exiting.
 */
int pdf_make_name(pdf_context *ctx, byte *n, uint32_t size, pdf_obj **o)
{
    char *NewBuf = NULL;
    pdf_name *name = NULL;

    name = (pdf_name *)gs_alloc_bytes(ctx->memory, sizeof(pdf_name), "pdf_read_name");
    if (name == NULL)
        return_error(gs_error_VMerror);

    memset(name, 0x00, sizeof(pdf_name));
    name->memory = ctx->memory;
    name->type = PDF_NAME;
    name->refcnt = 1;

    NewBuf = (char *)gs_alloc_bytes(ctx->memory, size, "pdf_read_name");
    if (NewBuf == NULL)
        return_error(gs_error_VMerror);

    memcpy(NewBuf, n, size);

    name->data = (unsigned char *)NewBuf;
    name->length = size;

    *o = (pdf_obj *)name;

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
    if (ctx->cache_entries != 0) {
        pdf_obj_cache_entry *entry = ctx->cache_LRU, *next;

        while(entry) {
            next = entry->next;
            pdf_countdown(entry->o);
            ctx->cache_entries--;
            gs_free_object(ctx->memory, entry, "pdf_add_to_cache, free LRU");
            entry = next;
        }
        ctx->cache_LRU = ctx->cache_MRU = NULL;
        ctx->cache_entries = 0;
    }

    if (ctx->PDFPassword)
        gs_free_object(ctx->memory, ctx->PDFPassword, "pdf_free_context");

    if (ctx->PageList)
        gs_free_object(ctx->memory, ctx->PageList, "pdf_free_context");

    if (ctx->Trailer)
        pdf_countdown((pdf_obj *)ctx->Trailer);

    if(ctx->Root)
        pdf_countdown((pdf_obj *)ctx->Root);

    if (ctx->Info)
        pdf_countdown((pdf_obj *)ctx->Info);

    if (ctx->Pages)
        pdf_countdown((pdf_obj *)ctx->Pages);

    if (ctx->xref_table) {
        pdf_countdown((pdf_obj *)ctx->xref_table);
        ctx->xref_table = NULL;
    }

    if (ctx->stack_bot) {
        pdf_clearstack(ctx);
        gs_free_object(ctx->memory, ctx->stack_bot, "pdf_free_context");
    }

    if (ctx->main_stream != NULL) {
        pdf_close_file(ctx, ctx->main_stream);
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
        gs_free_object(ctx->memory, Buffer, "PDF interpreter - cleanup working buffer for file validation");

    if (ctx->main_stream != NULL) {
        pdf_close_file(ctx, ctx->main_stream);
        ctx->main_stream = NULL;
    }
    ctx->main_stream_length = 0;
}

#define BUF_SIZE 2048

int repair_pdf_file(pdf_context *ctx)
{
    pdf_clearstack(ctx);

    if(ctx->pdfdebug)
        dmprintf(ctx->memory, "%% Error encoutnered in opening PDF file, attempting repair\n");

    return 0;
}

static int read_xref_stream_entries(pdf_context *ctx, pdf_stream *s, uint64_t first, uint64_t last, unsigned char *W)
{
    uint i, j = 0;
    uint32_t type = 0;
    uint64_t objnum = 0, gen = 0;
    byte *Buffer;
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

        entry = &ctx->xref_table->xref[i];
        if (entry->object_num != 0)
            continue;

        switch(type) {
            case 0:
                entry->compressed = false;
                entry->free = true;
                entry->u.uncompressed.offset = objnum;         /* For free objects we use the offset to store the object number of the next free object */
                entry->u.uncompressed.generation_num = gen;    /* And the generation number is the numebr to use if this object is used again */
                entry->object_num = i;
                entry->cache = NULL;
                break;
            case 1:
                entry->compressed = false;
                entry->free = false;
                entry->u.uncompressed.offset = objnum;
                entry->u.uncompressed.generation_num = gen;
                entry->object_num = i;
                entry->cache = NULL;
                break;
            case 2:
                entry->compressed = true;
                entry->free = false;
                entry->u.compressed.compressed_stream_num = objnum;   /* The object number of the compressed stream */
                entry->u.compressed.object_index = gen;               /* And the index of the object within the stream */
                entry->object_num = i;
                entry->cache = NULL;
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
static int pdf_read_xref_stream_dict(pdf_context *ctx, pdf_stream *s);

static int pdf_process_xref_stream(pdf_context *ctx, pdf_dict *d, pdf_stream *s)
{
    pdf_stream *XRefStrm;
    int code, i;
    pdf_obj *o;
    uint64_t size;
    unsigned char W[3];

    code = pdf_dict_get(d, "Type", &o);
    if (code < 0)
        return code;

    if (o->type != PDF_NAME) {
        pdf_countdown(o);
        return_error(gs_error_typecheck);
    }
    else {
        pdf_name *n = (pdf_name *)o;

        if (n->length != 4 || memcmp(n->data, "XRef", 4) != 0) {
            pdf_countdown(o);
            return_error(gs_error_syntaxerror);
        }
    }

    code = pdf_dict_get(d, "Size", &o);
    if (code < 0)
        return code;

    if (o->type != PDF_INT) {
        pdf_countdown(o);
        return_error(gs_error_typecheck);
    }
    size = ((pdf_num *)o)->value.i;
    pdf_countdown(o);

    /* If this is the first xref stream then allocate the xref table and store the trailer */
    if (ctx->xref_table == NULL) {
        ctx->xref_table = (xref_table *)gs_alloc_bytes(ctx->memory, sizeof(xref_table), "read_xref_stream allocate xref table");
        if (ctx->xref_table->xref == NULL) {
            return_error(gs_error_VMerror);
        }
        memset(ctx->xref_table, 0x00, sizeof(xref_table));
        ctx->xref_table->xref = (xref_entry *)gs_alloc_bytes(ctx->memory, size * sizeof(xref_entry), "read_xref_stream allocate xref table entries");
        if (ctx->xref_table->xref == NULL){
            gs_free_object(ctx->memory, ctx->xref_table, "failed to allocate xref table entries");
            ctx->xref_table = NULL;
            return_error(gs_error_VMerror);
        }
        memset(ctx->xref_table->xref, 0x00, size * sizeof(xref_entry));
        ctx->xref_table->memory = ctx->memory;
        ctx->xref_table->type = PDF_XREF_TABLE;
        ctx->xref_table->xref_size = size;
        pdf_countup((pdf_obj *)ctx->xref_table);

        ctx->Trailer = d;
        pdf_countup((pdf_obj *)d);
    }

    code = pdf_filter(ctx, d, s, &XRefStrm);
    if (code < 0) {
        pdf_countdown((pdf_obj *)ctx->xref_table);
        ctx->xref_table = NULL;
        return code;
    }

    code = pdf_dict_get(d, "W", &o);
    if (code < 0) {
        pdf_close_file(ctx, XRefStrm);
        pdf_countdown((pdf_obj *)ctx->xref_table);
        ctx->xref_table = NULL;
        return code;
    }

    if (o->type != PDF_ARRAY) {
        pdf_countdown(o);
        pdf_close_file(ctx, XRefStrm);
        pdf_countdown((pdf_obj *)ctx->xref_table);
        ctx->xref_table = NULL;
        return_error(gs_error_typecheck);
    }
    else {
        pdf_array *a = (pdf_array *)o;

        if (a->entries != 3) {
            pdf_countdown(o);
            pdf_close_file(ctx, XRefStrm);
            pdf_countdown((pdf_obj *)ctx->xref_table);
            ctx->xref_table = NULL;
            return_error(gs_error_rangecheck);
        }
        for (i=0;i<3;i++) {
            code = pdf_array_get(a, (uint64_t)i, &o);
            if (code < 0) {
                pdf_countdown(o);
                pdf_close_file(ctx, XRefStrm);
                pdf_countdown((pdf_obj *)ctx->xref_table);
                ctx->xref_table = NULL;
                return code;
            }
            if (o->type != PDF_INT) {
                pdf_countdown(o);
                pdf_close_file(ctx, XRefStrm);
                pdf_countdown((pdf_obj *)ctx->xref_table);
                ctx->xref_table = NULL;
                return_error(gs_error_typecheck);
            }
            W[i] = ((pdf_num *)o)->value.i;
            pdf_countdown(o);
        }
        pdf_countdown((pdf_obj *)a);
    }

    code = pdf_dict_get(d, "Index", &o);
    if (code == gs_error_undefined) {
        code = read_xref_stream_entries(ctx, XRefStrm, 0, size - 1, W);
        if (code < 0) {
            pdf_close_file(ctx, XRefStrm);
            pdf_countdown((pdf_obj *)ctx->xref_table);
            ctx->xref_table = NULL;
            return code;
        }
    } else {
        if (code < 0) {
            pdf_close_file(ctx, XRefStrm);
            pdf_countdown((pdf_obj *)ctx->xref_table);
            ctx->xref_table = NULL;
            return code;
        }

        if (o->type != PDF_ARRAY) {
            pdf_countdown(o);
            pdf_close_file(ctx, XRefStrm);
            pdf_countdown((pdf_obj *)ctx->xref_table);
            ctx->xref_table = NULL;
            return_error(gs_error_typecheck);
        }
        else {
            pdf_array *a = (pdf_array *)o;
            pdf_num *start, *end;

            if (a->entries & 1) {
                pdf_countdown(o);
                pdf_close_file(ctx, XRefStrm);
                pdf_countdown((pdf_obj *)ctx->xref_table);
                ctx->xref_table = NULL;
                return_error(gs_error_rangecheck);
            }

            for (i=0;i < a->entries;i+=2){
                code = pdf_array_get(a, (uint64_t)i, (pdf_obj **)&start);
                if (code < 0) {
                    pdf_countdown(o);
                    pdf_close_file(ctx, XRefStrm);
                    pdf_countdown((pdf_obj *)ctx->xref_table);
                    ctx->xref_table = NULL;
                    return code;
                }
                if (start->type != PDF_INT) {
                    pdf_countdown(o);
                    pdf_countdown((pdf_obj *)start);
                    pdf_close_file(ctx, XRefStrm);
                    pdf_countdown((pdf_obj *)ctx->xref_table);
                    ctx->xref_table = NULL;
                    return_error(gs_error_typecheck);
                }

                code = pdf_array_get(a, (uint64_t)i+1, (pdf_obj **)&end);
                if (code < 0) {
                    pdf_countdown(o);
                    pdf_countdown((pdf_obj *)start);
                    pdf_close_file(ctx, XRefStrm);
                    pdf_countdown((pdf_obj *)ctx->xref_table);
                    ctx->xref_table = NULL;
                    return code;
                }
                if (end->type != PDF_INT) {
                    pdf_countdown(o);
                    pdf_countdown((pdf_obj *)start);
                    pdf_countdown((pdf_obj *)end);
                    pdf_close_file(ctx, XRefStrm);
                    pdf_countdown((pdf_obj *)ctx->xref_table);
                    ctx->xref_table = NULL;
                    return_error(gs_error_typecheck);
                }

                code = read_xref_stream_entries(ctx, XRefStrm, start->value.i, start->value.i + end->value.i - 1, W);
                pdf_countdown((pdf_obj *)start);
                pdf_countdown((pdf_obj *)end);
                if (code < 0) {
                    pdf_countdown(o);
                    pdf_close_file(ctx, XRefStrm);
                    pdf_countdown((pdf_obj *)ctx->xref_table);
                    ctx->xref_table = NULL;
                    return code;
                }
            }
        }
    }
    if (o)
        pdf_countdown(o);

    pdf_close_file(ctx, XRefStrm);

    code = pdf_dict_get(d, "Prev", &o);
    if (code == gs_error_undefined)
        return 0;

    if (code < 0)
        return code;

    if (o->type != PDF_INT) {
        pdf_countdown(o);
        return_error(gs_error_typecheck);
    }

    if(ctx->pdfdebug)
        dmprintf(ctx->memory, "%% Reading /Prev xref\n");

    pdf_seek(ctx, s, ((pdf_num *)o)->value.i, SEEK_SET);
    pdf_countdown(o);

    code = pdf_read_token(ctx, ctx->main_stream);
    if (code < 0)
        return code;

    code = pdf_read_xref_stream_dict(ctx, s);

    return code;
}

static int pdf_read_xref_stream_dict(pdf_context *ctx, pdf_stream *s)
{
    int code;

    if (ctx->pdfdebug)
        dmprintf(ctx->memory, "\n%% Reading PDF 1.5+ xref stream\n");

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

                        /* Remove the 'stream' token from the stack, should leave a dictionary object on the stack */
                        pdf_pop(ctx, 1);
                        if (((pdf_obj *)ctx->stack_top[-1])->type != PDF_DICT) {
                            pdf_pop(ctx, 1);
                            return repair_pdf_file(ctx);
                        }
                        d = (pdf_dict *)ctx->stack_top[-1];
                        d->stream_offset = (gs_offset_t)stell(ctx->main_stream->s);

                        d->object_num = obj_num;
                        d->generation_num = gen_num;
                        pdf_countup((pdf_obj *)d);
                        pdf_pop(ctx, 1);

                        code = pdf_process_xref_stream(ctx, d, ctx->main_stream);
                        if (code < 0) {
                            pdf_countdown((pdf_obj *)d);
                            return (repair_pdf_file(ctx));
                        }
                        pdf_countdown((pdf_obj *)d);
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

static int read_xref(pdf_context *ctx, pdf_stream *s)
{
    int code = 0, i, j;
    pdf_obj *o = NULL;
    pdf_dict *d = NULL;
    uint64_t start = 0, size = 0, bytes = 0;
    char Buffer[21];

    if (ctx->pdfdebug)
        dmprintf(ctx->memory, "\n%% Reading pre PDF 1.5 xref table\n");

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
    if (ctx->xref_table == NULL && size != 0) {
        ctx->xref_table = (xref_table *)gs_alloc_bytes(ctx->memory, sizeof(xref_table), "read_xref_stream allocate xref table");
        if (ctx->xref_table == NULL)
            return_error(gs_error_VMerror);
        memset(ctx->xref_table, 0x00, sizeof(xref_table));

        ctx->xref_table->xref = (xref_entry *)gs_alloc_bytes(ctx->memory, (start + size) * sizeof(xref_entry), "read_xref_stream allocate xref table entries");
        if (ctx->xref_table->xref == NULL){
            pdf_countdown((pdf_obj *)ctx->xref_table);
            ctx->xref_table = NULL;
            return_error(gs_error_VMerror);
        }

        memset(ctx->xref_table->xref, 0x00, ((pdf_num *)o)->value.i * sizeof(xref_entry));
        ctx->xref_table->memory = ctx->memory;
        ctx->xref_table->type = PDF_XREF_TABLE;
        ctx->xref_table->xref_size = start + size;
        pdf_countup((pdf_obj *)ctx->xref_table);
    }

    skip_white(ctx, s);
    for (i=0;i< size;i++){
        xref_entry *entry = &ctx->xref_table->xref[i + start];
        unsigned char free;

        if (entry->object_num != 0)
            continue;

        bytes = pdf_read_bytes(ctx, (byte *)Buffer, 1, 20, s);
        if (bytes < 20)
            return_error(gs_error_ioerror);
        j = 19;
        while (Buffer[j] != 0x0D && Buffer[j] != 0x0A) {
            pdf_unread(ctx, s, (byte *)&Buffer[j], 1);
            j--;
        }
        Buffer[j] = 0x00;
        sscanf(Buffer, "%ld %d %c", &entry->u.uncompressed.offset, &entry->u.uncompressed.generation_num, &free);
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
    if (d->type != PDF_DICT) {
        pdf_pop(ctx, 1);
        return_error(gs_error_typecheck);
    }

    if (ctx->Trailer == NULL) {
        ctx->Trailer = d;
        pdf_countup((pdf_obj *)d);
    }

    /* We have the Trailer dictionary. First up check for hybrid files. These have the initial
     * xref starting at 0 and size of 0. In this case the /Size entry in the trailer dictionary
     * must tell us how large the xref is, and we need to allocate our xref table anyway.
     */
    if (ctx->xref_table == NULL && size == 0) {
        code = pdf_dict_get(d, "Size", &o);
        if (code < 0) {
            pdf_pop(ctx, 2);
            return code;
        }
        if (o->type != PDF_INT) {
            pdf_pop(ctx, 2);
            return_error(gs_error_typecheck);
        }

        ctx->xref_table = (xref_table *)gs_alloc_bytes(ctx->memory, sizeof(xref_table), "read_xref_stream allocate xref table");
        if (ctx->xref_table == NULL) {
            pdf_pop(ctx, 2);
            return_error(gs_error_VMerror);
        }
        memset(ctx->xref_table, 0x00, sizeof(xref_table));

        ctx->xref_table->xref = (xref_entry *)gs_alloc_bytes(ctx->memory, ((pdf_num *)o)->value.i * sizeof(xref_entry), "read_xref_stream allocate xref table entries");
        if (ctx->xref_table->xref == NULL){
            pdf_pop(ctx, 2);
            pdf_countdown((pdf_obj *)ctx->xref_table);
            ctx->xref_table = NULL;
            return_error(gs_error_VMerror);
        }

        memset(ctx->xref_table->xref, 0x00, ((pdf_num *)o)->value.i * sizeof(xref_entry));
        ctx->xref_table->memory = ctx->memory;
        ctx->xref_table->type = PDF_XREF_TABLE;
        ctx->xref_table->xref_size = ((pdf_num *)o)->value.i;
        pdf_countup((pdf_obj *)ctx->xref_table);
    }

    /* Now check if this is a hybrid file. */
    code = pdf_dict_get(d, "XRefStm", &o);
    if (code < 0 && code != gs_error_undefined) {
        pdf_pop(ctx, 2);
        return code;
    }
    if (code == 0) {
        if (ctx->pdfdebug)
            dmprintf(ctx->memory, "%% File is a hybrid, containing xref table and xref stream. Using the stream.\n");

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

int pdf_open_pdf_file(pdf_context *ctx, char *filename)
{
    byte *Buffer = NULL;
    char *s = NULL;
    float version = 0.0;
    gs_offset_t Offset = 0, bytes = 0;
    int code = 0;

    if (ctx->pdfdebug)
        dmprintf1(ctx->memory, "%% Attempting to open %s as a PDF file\n", filename);

    ctx->main_stream = (pdf_stream *)gs_alloc_bytes(ctx->memory, sizeof(pdf_stream), "PDF interpreter allocate main PDF stream");
    if (ctx->main_stream == NULL)
        return_error(gs_error_VMerror);
    memset(ctx->main_stream, 0x00, sizeof(pdf_stream));

    ctx->main_stream->s = sfopen(filename, "r", ctx->memory);
    if (ctx->main_stream == NULL) {
        emprintf1(ctx->memory, "Failed to open file %s\n", filename);
        return_error(gs_error_ioerror);
    }

    Buffer = gs_alloc_bytes(ctx->memory, BUF_SIZE, "PDF interpreter - allocate working buffer for file validation");
    if (Buffer == NULL) {
        cleanup_pdf_open_file(ctx, Buffer);
        return_error(gs_error_VMerror);
    }

    if (ctx->pdfdebug)
        dmprintf(ctx->memory, "%% Reading header\n");

    bytes = pdf_read_bytes(ctx, Buffer, 1, BUF_SIZE, ctx->main_stream);
    if (bytes == 0) {
        emprintf(ctx->memory, "Failed to read any bytes from file\n");
        cleanup_pdf_open_file(ctx, Buffer);
        return_error(gs_error_ioerror);
    }

    /* First check for existence of header */
    s = strstr((char *)Buffer, "%PDF");
    if (s == NULL) {
        if (ctx->pdfdebug)
            dmprintf1(ctx->memory, "%% File %s does not appear to be a PDF file (no %%PDF in first 2Kb of file)\n", filename);
    } else {
        /* Now extract header version (may be overridden later) */
        if (sscanf(s + 5, "%f", &version) != 1) {
            if (ctx->pdfdebug)
                dmprintf(ctx->memory, "%% Unable to read PDF version from header\n");
            ctx->HeaderVersion = 0;
        }
        else {
            ctx->HeaderVersion = version;
        }
        if (ctx->pdfdebug)
            dmprintf1(ctx->memory, "%% Found header, PDF version is %f\n", ctx->HeaderVersion);
    }

    /* Jump to EOF and scan backwards looking for startxref */
    pdf_seek(ctx, ctx->main_stream, 0, SEEK_SET);
    pdf_seek(ctx, ctx->main_stream, 0, SEEK_END);
    ctx->main_stream_length = stell(ctx->main_stream->s);
    Offset = BUF_SIZE;
    bytes = BUF_SIZE;

    if (ctx->pdfdebug)
        dmprintf(ctx->memory, "%% Searching for 'startxerf' keyword\n");

    do {
        byte *last_lineend = NULL;
        uint32_t read;

        if (pdf_seek(ctx, ctx->main_stream, ctx->main_stream_length - Offset, SEEK_SET) != 0) {
            emprintf1(ctx->memory, "File is smaller than %"PRIi64" bytes\n", (int64_t)Offset);
            cleanup_pdf_open_file(ctx, Buffer);
            return_error(gs_error_ioerror);
        }
        read = pdf_read_bytes(ctx, Buffer, 1, bytes, ctx->main_stream);

        if (read == 0) {
            emprintf1(ctx->memory, "Failed to read %"PRIi64" bytes from file\n", (int64_t)bytes);
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


    if (ctx->startxref != 0) {
        if (ctx->pdfdebug)
            dmprintf(ctx->memory, "%% Trying to read 'xref' token for xref table, or 'int int obj' for an xref stream\n");

        /* Read the xref(s) */
        pdf_seek(ctx, ctx->main_stream, ctx->startxref, SEEK_SET);

        code = pdf_read_token(ctx, ctx->main_stream);
        if (code < 0) {
            dmprintf(ctx->memory, "Failed to read any token at the startxref location\n");
            return(repair_pdf_file(ctx));
        }

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

    if(ctx->pdfdebug) {
        int i, j;
        xref_entry *entry;
        char Buffer[32];

        dmprintf(ctx->memory, "\n%% Dumping xref table\n");
        for (i=0;i < ctx->xref_table->xref_size;i++) {
            entry = &ctx->xref_table->xref[i];
            if(entry->compressed) {
                dmprintf(ctx->memory, "*");
                gs_sprintf(Buffer, "%ld", entry->object_num);
                j = 10 - strlen(Buffer);
                while(j--) {
                    dmprintf(ctx->memory, " ");
                }
                dmprintf1(ctx->memory, "%s ", Buffer);

                gs_sprintf(Buffer, "%ld", entry->u.compressed.compressed_stream_num);
                j = 10 - strlen(Buffer);
                while(j--) {
                    dmprintf(ctx->memory, " ");
                }
                dmprintf1(ctx->memory, "%s ", Buffer);

                gs_sprintf(Buffer, "%ld", entry->u.compressed.object_index);
                j = 10 - strlen(Buffer);
                while(j--) {
                    dmprintf(ctx->memory, " ");
                }
                dmprintf1(ctx->memory, "%s ", Buffer);
            }
            else {
                dmprintf(ctx->memory, " ");

                gs_sprintf(Buffer, "%ld", entry->object_num);
                j = 10 - strlen(Buffer);
                while(j--) {
                    dmprintf(ctx->memory, " ");
                }
                dmprintf1(ctx->memory, "%s ", Buffer);

                gs_sprintf(Buffer, "%ld", entry->u.uncompressed.offset);
                j = 10 - strlen(Buffer);
                while(j--) {
                    dmprintf(ctx->memory, " ");
                }
                dmprintf1(ctx->memory, "%s ", Buffer);

                gs_sprintf(Buffer, "%ld", entry->u.uncompressed.generation_num);
                j = 10 - strlen(Buffer);
                while(j--) {
                    dmprintf(ctx->memory, " ");
                }
                dmprintf1(ctx->memory, "%s ", Buffer);
            }
            if (entry->free)
                dmprintf(ctx->memory, "f\n");
            else
                dmprintf(ctx->memory, "n\n");
        }
    }
    if (ctx->pdfdebug)
        dmprintf(ctx->memory, "\n");
    gs_free_object(ctx->memory, Buffer, "PDF interpreter - allocate working buffer for file validation");
    return 0;
}

static int pdf_read_Root(pdf_context *ctx)
{
    pdf_obj *o, *o1;
    int code;

    if (ctx->pdfdebug)
        dmprintf(ctx->memory, "%% Reading Root dictionary\n");

    code = pdf_dict_get(ctx->Trailer, "Root", &o1);
    if (code < 0)
        return code;

    if (o1->type == PDF_INDIRECT) {
        pdf_obj *name;

        code = pdf_dereference(ctx, ((pdf_indirect_ref *)o1)->object_num,  ((pdf_indirect_ref *)o1)->generation_num, &o);
        pdf_countdown(o1);
        if (code < 0)
            return code;

        if (o->type != PDF_DICT) {
            pdf_countdown(o);
            return_error(gs_error_typecheck);
        }

        code = pdf_make_name(ctx, (byte *)"Root", 4, &name);
        if (code < 0) {
            pdf_countdown(o);
            return code;
        }
        code = pdf_dict_put(ctx->Trailer, name, o);
        /* pdf_make_name created a name with a reference count of 1, the local object
         * is going out of scope, so decrement the reference coutn.
         */
        pdf_countdown(name);
        if (code < 0) {
            pdf_countdown(o);
            return code;
        }
        o1 = o;
    } else {
        if (o1->type != PDF_DICT) {
            pdf_countdown(o1);
            return_error(gs_error_typecheck);
        }
    }

    code = pdf_dict_get((pdf_dict *)o1, "Type", &o);
    if (code < 0) {
        pdf_countdown(o1);
        return code;
    }
    if (o->type != PDF_NAME) {
        pdf_countdown(o1);
        return_error(gs_error_typecheck);
    }
    if (((pdf_name *)o)->length != 7 || memcmp(((pdf_name *)o)->data, "Catalog", 7) != 0){
        pdf_countdown(o1);
        return_error(gs_error_syntaxerror);
    }

    if (ctx->pdfdebug)
        dmprintf(ctx->memory, "\n");
    /* We don't pdf_countdown(o1) now, because we've transferred our
     * reference to the pointer in the pdf_context structure.
     */
    ctx->Root = (pdf_dict *)o1;
    return 0;
}

static int pdf_read_Info(pdf_context *ctx)
{
    pdf_obj *o, *o1;
    int code;

    if (ctx->pdfdebug)
        dmprintf(ctx->memory, "%% Reading Info dictionary\n");

    code = pdf_dict_get(ctx->Trailer, "Info", &o1);
    if (code < 0)
        return code;

    if (o1->type == PDF_INDIRECT) {
        pdf_obj *name;

        code = pdf_dereference(ctx, ((pdf_indirect_ref *)o1)->object_num,  ((pdf_indirect_ref *)o1)->generation_num, &o);
        pdf_countdown(o1);
        if (code < 0)
            return code;

        if (o->type != PDF_DICT) {
            pdf_countdown(o);
            return_error(gs_error_typecheck);
        }

        code = pdf_make_name(ctx, (byte *)"Info", 4, &name);
        if (code < 0) {
            pdf_countdown(o);
            return code;
        }
        code = pdf_dict_put(ctx->Trailer, name, o);
        /* pdf_make_name created a name with a reference count of 1, the local object
         * is going out of scope, so decrement the reference coutn.
         */
        pdf_countdown(name);
        if (code < 0) {
            pdf_countdown(o);
            return code;
        }
        o1 = o;
    } else {
        if (o1->type != PDF_DICT) {
            pdf_countdown(o1);
            return_error(gs_error_typecheck);
        }
    }

    if (ctx->pdfdebug)
        dmprintf(ctx->memory, "\n");
    /* We don't pdf_countdown(o1) now, because we've transferred our
     * reference to the pointer in the pdf_context structure.
     */
    ctx->Info = (pdf_dict *)o1;
    return 0;
}

static int pdf_read_Pages(pdf_context *ctx)
{
    pdf_obj *o, *o1;
    int code;

    if (ctx->pdfdebug)
        dmprintf(ctx->memory, "%% Reading Pages dictionary\n");

    code = pdf_dict_get(ctx->Root, "Pages", &o1);
    if (code < 0)
        return code;

    if (o1->type == PDF_INDIRECT) {
        pdf_obj *name;

        code = pdf_dereference(ctx, ((pdf_indirect_ref *)o1)->object_num,  ((pdf_indirect_ref *)o1)->generation_num, &o);
        pdf_countdown(o1);
        if (code < 0)
            return code;

        if (o->type != PDF_DICT) {
            pdf_countdown(o);
            return_error(gs_error_typecheck);
        }

        code = pdf_make_name(ctx, (byte *)"Pages", 5, &name);
        if (code < 0) {
            pdf_countdown(o);
            return code;
        }
        code = pdf_dict_put(ctx->Root, name, o);
        /* pdf_make_name created a name with a reference count of 1, the local object
         * is going out of scope, so decrement the reference coutn.
         */
        pdf_countdown(name);
        if (code < 0) {
            pdf_countdown(o);
            return code;
        }
        o1 = o;
    } else {
        if (o1->type != PDF_DICT) {
            pdf_countdown(o1);
            return_error(gs_error_typecheck);
        }
    }

    if (ctx->pdfdebug)
        dmprintf(ctx->memory, "\n");
    /* We don't pdf_countdown(o1) now, because we've transferred our
     * reference to the pointer in the pdf_context structure.
     */
    ctx->Pages = (pdf_dict *)o1;
    return 0;
}

/* These functions are used by the 'PL' implementation, eventually we will */
/* need to have custom PostScript operators to process the file or at      */
/* (least pages from it).                                                  */

int pdf_close_pdf_file(pdf_context *ctx)
{
    if (ctx->main_stream) {
        if (ctx->main_stream->s) {
            pdf_close_file(ctx, ctx->main_stream);
            ctx->main_stream = NULL;
        }
        gs_free_object(ctx->memory, ctx->main_stream, "Closing main PDF file");
    }
    return 0;
}

int pdf_process_pdf_file(pdf_context *ctx, char *filename)
{
    int code = 0;

    code = pdf_open_pdf_file(ctx, filename);
    if (code < 0)
        return code;

    code = pdf_read_Root(ctx);
    if (code < 0)
        return code;

    code = pdf_read_Info(ctx);
    if (code < 0)
        return code;

    code = pdf_read_Pages(ctx);
    if (code < 0)
        return code;

    code = pdf_close_pdf_file(ctx);
    return code;
}

