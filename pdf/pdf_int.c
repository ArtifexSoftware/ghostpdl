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

#include "plmain.h"
#include "pdf_int.h"
#include "pdf_file.h"
#include "pdf_loop_detect.h"
#include "strmio.h"
#include "stream.h"
#include "pdf_path.h"
#include "pdf_colour.h"
#include "pdf_gstate.h"
#include "pdf_stack.h"

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
            pdf_countdown(d->keys[i]);
        if (d->values[i] != NULL)
            pdf_countdown(d->values[i]);
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
        case PDF_ARRAY_MARK:
        case PDF_DICT_MARK:
        case PDF_NULL:
        case PDF_INT:
        case PDF_REAL:
        case PDF_INDIRECT:
        case PDF_BOOL:
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
    gs_offset_t saved_stream_offset;

    if (obj > ctx->xref_table->xref_size)
        return_error(gs_error_rangecheck);

    entry = &ctx->xref_table->xref[obj];

    if(entry->object_num == 0)
        return_error(gs_error_undefined);

    if (ctx->loop_detection) {
        if (pdf_loop_detector_check_object(ctx, obj) == true)
            return_error(gs_error_circular_reference);
    }
    if (entry->cache != NULL){
        pdf_obj_cache_entry *cache_entry = entry->cache;

        *object = cache_entry->o;
        pdf_countup(*object);
        if (ctx->cache_MRU) {
            if ((pdf_obj_cache_entry *)cache_entry->next != NULL)
                ((pdf_obj_cache_entry *)cache_entry->next)->previous = cache_entry->previous;
            if ((pdf_obj_cache_entry *)cache_entry->previous != NULL)
                ((pdf_obj_cache_entry *)cache_entry->previous)->next = cache_entry->next;
            cache_entry->next = NULL;
            cache_entry->previous = ctx->cache_MRU;
            ctx->cache_MRU->next = cache_entry;
            ctx->cache_MRU = cache_entry;
        } else
            return_error(gs_error_unknownerror);
    } else {
        saved_stream_offset = (gs_offset_t)pdf_tell(ctx);

        if (entry->compressed) {
            /* This is an object in a compressed object stream */

            xref_entry *compressed_entry = &ctx->xref_table->xref[entry->u.compressed.compressed_stream_num];
            pdf_dict *compressed_object;
            pdf_stream *compressed_stream;
            char Buffer[256];
            int i = 0;
            int64_t num_entries;
            gs_offset_t offset;

            if (ctx->pdfdebug) {
                dmprintf1(ctx->memory, "%% Reading compressed object %"PRIi64, obj);
                dmprintf1(ctx->memory, " from ObjStm with object number %"PRIi64"\n", compressed_entry->object_num);
            }

            if (compressed_entry->cache == NULL) {
                code = pdf_seek(ctx, ctx->main_stream, compressed_entry->u.uncompressed.offset, SEEK_SET);
                if (code < 0) {
                    (void)pdf_seek(ctx, ctx->main_stream, saved_stream_offset, SEEK_SET);
                    return code;
                }

                code = pdf_read_object_of_type(ctx, ctx->main_stream, PDF_DICT);
                if (code < 0){
                    (void)pdf_seek(ctx, ctx->main_stream, saved_stream_offset, SEEK_SET);
                    return code;
                }

                compressed_object = (pdf_dict *)ctx->stack_top[-1];
                pdf_countup(compressed_object);
                pdf_pop(ctx, 1);
            } else {
                compressed_object = (pdf_dict *)compressed_entry->cache->o;
                pdf_countup(compressed_object);
            }
            /* Check its an ObjStm ! */
            code = pdf_dict_get_type(ctx, compressed_object, "Type", PDF_NAME, &o);
            if (code < 0) {
                pdf_countdown(compressed_object);
                (void)pdf_seek(ctx, ctx->main_stream, saved_stream_offset, SEEK_SET);
                return code;
            }
            if (((pdf_name *)o)->length != 6 || memcmp(((pdf_name *)o)->data, "ObjStm", 6) != 0){
                pdf_countdown(o);
                pdf_countdown(compressed_object);
                (void)pdf_seek(ctx, ctx->main_stream, saved_stream_offset, SEEK_SET);
                return_error(gs_error_syntaxerror);
            }
            pdf_countdown(o);

            /* Need to check the /N entry to see if the object is actually in this stream! */
            code = pdf_dict_get_int(ctx, compressed_object, "N", &num_entries);
            if (code < 0) {
                pdf_countdown(compressed_object);
                (void)pdf_seek(ctx, ctx->main_stream, saved_stream_offset, SEEK_SET);
                return code;
            }
            if (num_entries < 0 || num_entries > ctx->xref_table->xref_size) {
                pdf_countdown(compressed_object);
                (void)pdf_seek(ctx, ctx->main_stream, saved_stream_offset, SEEK_SET);
                return_error(gs_error_rangecheck);
            }

            code = pdf_seek(ctx, ctx->main_stream, compressed_object->stream_offset, SEEK_SET);
            if (code < 0) {
                pdf_countdown(compressed_object);
                (void)pdf_seek(ctx, ctx->main_stream, saved_stream_offset, SEEK_SET);
                return code;
            }

            code = pdf_filter(ctx, compressed_object, ctx->main_stream, &compressed_stream, false);
            if (code < 0) {
                pdf_countdown(compressed_object);
                (void)pdf_seek(ctx, ctx->main_stream, saved_stream_offset, SEEK_SET);
                return code;
            }

            for (i=0;i < num_entries;i++)
            {
                code = pdf_read_token(ctx, compressed_stream);
                if (code < 0) {
                    pdf_close_file(ctx, compressed_stream);
                    pdf_countdown(compressed_object);
                    (void)pdf_seek(ctx, ctx->main_stream, saved_stream_offset, SEEK_SET);
                    return code;
                }
                o = ctx->stack_top[-1];
                if (((pdf_obj *)o)->type != PDF_INT) {
                    pdf_pop(ctx, 1);
                    pdf_close_file(ctx, compressed_stream);
                    pdf_countdown(compressed_object);
                    (void)pdf_seek(ctx, ctx->main_stream, saved_stream_offset, SEEK_SET);
                    return_error(gs_error_typecheck);
                }
                pdf_pop(ctx, 1);
                code = pdf_read_token(ctx, compressed_stream);
                if (code < 0) {
                    pdf_close_file(ctx, compressed_stream);
                    pdf_countdown(compressed_object);
                    (void)pdf_seek(ctx, ctx->main_stream, saved_stream_offset, SEEK_SET);
                    return code;
                }
                o = ctx->stack_top[-1];
                if (((pdf_obj *)o)->type != PDF_INT) {
                    pdf_pop(ctx, 1);
                    pdf_close_file(ctx, compressed_stream);
                    pdf_countdown(compressed_object);
                    (void)pdf_seek(ctx, ctx->main_stream, saved_stream_offset, SEEK_SET);
                    return_error(gs_error_typecheck);
                }
                if (i == entry->u.compressed.object_index)
                    offset = ((pdf_num *)o)->value.i;
                pdf_pop(ctx, 1);
            }

            for (i=0;i < offset;i++)
            {
                code = pdf_read_bytes(ctx, (byte *)&Buffer[0], 1, 1, compressed_stream);
                if (code <= 0) {
                    pdf_close_file(ctx, compressed_stream);
                    return_error(gs_error_ioerror);
                }
            }

            code = pdf_read_token(ctx, compressed_stream);
            if (code < 0) {
                pdf_close_file(ctx, compressed_stream);
                pdf_countdown(compressed_object);
                (void)pdf_seek(ctx, ctx->main_stream, saved_stream_offset, SEEK_SET);
                return code;
            }
            if (ctx->stack_top[-1]->type == PDF_ARRAY_MARK || ctx->stack_top[-1]->type == PDF_DICT_MARK) {
                int start_depth = ctx->stack_top - ctx->stack_bot;

                /* Need to read all the elements from COS objects */
                do {
                    code = pdf_read_token(ctx, compressed_stream);
                    if (code < 0) {
                        pdf_close_file(ctx, compressed_stream);
                        pdf_countdown(compressed_object);
                        (void)pdf_seek(ctx, ctx->main_stream, saved_stream_offset, SEEK_SET);
                        return code;
                    }
                    if (compressed_stream->eof == true) {
                        pdf_close_file(ctx, compressed_stream);
                        pdf_countdown(compressed_object);
                        (void)pdf_seek(ctx, ctx->main_stream, saved_stream_offset, SEEK_SET);
                        return_error(gs_error_ioerror);
                    }
                }while ((ctx->stack_top[-1]->type != PDF_ARRAY && ctx->stack_top[-1]->type != PDF_DICT) || ctx->stack_top - ctx->stack_bot > start_depth);
            }

            pdf_close_file(ctx, compressed_stream);

            *object = ctx->stack_top[-1];
            pdf_countup(*object);
            pdf_pop(ctx, 1);

            pdf_countdown(compressed_object);
        } else {
            code = pdf_seek(ctx, ctx->main_stream, entry->u.uncompressed.offset, SEEK_SET);
            if (code < 0) {
                (void)pdf_seek(ctx, ctx->main_stream, saved_stream_offset, SEEK_SET);
                return code;
            }

            code = pdf_read_object(ctx, ctx->main_stream);
            if (code < 0) {
                (void)pdf_seek(ctx, ctx->main_stream, saved_stream_offset, SEEK_SET);
                return code;
            }

            *object = ctx->stack_top[-1];
            pdf_countup(*object);
            pdf_pop(ctx, 1);
        }
        (void)pdf_seek(ctx, ctx->main_stream, saved_stream_offset, SEEK_SET);
    }

    if (ctx->loop_detection) {
        code = pdf_loop_detector_add_object(ctx, (*object)->object_num);
        if (code < 0)
            return code;
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
static int skip_white(pdf_context *ctx, pdf_stream *s)
{
    uint32_t read = 0;
    int32_t bytes = 0;
    byte c;

    do {
        bytes = pdf_read_bytes(ctx, &c, 1, 1, s);
        if (bytes < 0)
            return_error(gs_error_ioerror);
        if (bytes == 0)
            return 0;
        read += bytes;
    } while (bytes != 0 && iswhite(c));

    if (read > 0)
        pdf_unread(ctx, s, &c, 1);
    return 0;
}

static int skip_eol(pdf_context *ctx, pdf_stream *s)
{
    uint32_t read = 0;
    int32_t bytes = 0;
    byte c;

    do {
        bytes = pdf_read_bytes(ctx, &c, 1, 1, s);
        if (bytes < 0)
            return_error(gs_error_ioerror);
        if (bytes == 0)
            return 0;
        read += bytes;
    } while (bytes != 0 && (c == 0x0A || c == 0x0D));

    if (read > 0)
        pdf_unread(ctx, s, &c, 1);
    return 0;
}

static int pdf_read_num(pdf_context *ctx, pdf_stream *s)
{
    byte Buffer[256];
    unsigned short index = 0;
    short bytes;
    bool real = false;
    pdf_num *num;
    int code = 0, malformed = false;

    skip_white(ctx, s);

    do {
        bytes = pdf_read_bytes(ctx, (byte *)&Buffer[index], 1, 1, s);
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
                pdf_unread(ctx, s, (byte *)&Buffer[index], 1);
                Buffer[index] = 0x00;
                break;
            }
        }
        if (Buffer[index] == '.')
            if (real == true) {
                if (ctx->pdfstoponerror)
                    return_error(gs_error_syntaxerror);
                malformed = true;
            } else
                real = true;
        else {
            if (Buffer[index] == '-' || Buffer[index] == '+') {
                if (index != 0) {
                    if (ctx->pdfstoponerror)
                        return_error(gs_error_syntaxerror);
                    malformed = true;
                }
            } else {
                if (Buffer[index] < 0x30 || Buffer[index] > 0x39) {
                    if (ctx->pdfstoponerror)
                        return_error(gs_error_syntaxerror);
                    dmprintf(ctx->memory, "Ignoring missing white space while parsing number\n");
                    pdf_unread(ctx, s, (byte *)&Buffer[index], 1);
                    Buffer[index] = 0x00;
                    break;
                }
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

    if (malformed) {
        dmprintf1(ctx->memory, "Treating malformed number %s as 0.\n", Buffer);
        num->type = PDF_INT;
        num->value.i = 0;
    } else {
        if (real) {
            float tempf;
            num->type = PDF_REAL;
            if (sscanf((const char *)Buffer, "%f", &tempf) == 0) {
                if (ctx->pdfdebug)
                    dmprintf1(ctx->memory, "failed to read real number : %s\n", Buffer);
                gs_free_object(num->memory, num, "pdf_read_num error");
                return_error(gs_error_syntaxerror);
            }
            num->value.d = tempf;
        } else {
            int tempi;
            num->type = PDF_INT;
            if (sscanf((const char *)Buffer, "%d", &tempi) == 0) {
                if (ctx->pdfdebug)
                    dmprintf1(ctx->memory, "failed to read integer : %s\n", Buffer);
                gs_free_object(num->memory, num, "pdf_read_num error");
                return_error(gs_error_syntaxerror);
            }
            num->value.i = tempi;
        }
    }
    if (ctx->pdfdebug) {
        if (real)
            dmprintf1(ctx->memory, " %f", num->value.d);
        else
            dmprintf1(ctx->memory, " %"PRIi64, num->value.i);
    }

#if REFCNT_DEBUG
    num->UID = ctx->UID++;
    dmprintf1(ctx->memory, "Allocated number object with UID %"PRIi64"\n", num->UID);
#endif
    code = pdf_push(ctx, (pdf_obj *)num);

    if (code < 0)
        pdf_free_object((pdf_obj *)num);

    return code;
}

static int pdf_read_name(pdf_context *ctx, pdf_stream *s)
{
    char *Buffer, *NewBuf = NULL;
    unsigned short index = 0;
    short bytes = 0;
    uint32_t size = 256;
    pdf_name *name = NULL;
    int code;

    Buffer = (char *)gs_alloc_bytes(ctx->memory, size, "pdf_read_name");
    if (Buffer == NULL)
        return_error(gs_error_VMerror);

    do {
        bytes = pdf_read_bytes(ctx, (byte *)&Buffer[index], 1, 1, s);
        if (bytes == 0 && s->eof)
            break;
        if (bytes <= 0)
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
        if (index++ >= size - 1) {
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

#if REFCNT_DEBUG
    name->UID = ctx->UID++;
    dmprintf1(ctx->memory, "Allocated name object with UID %"PRIi64"\n", name->UID);
#endif
    code = pdf_push(ctx, (pdf_obj *)name);

    if (code < 0)
        pdf_free_namestring((pdf_obj *)name);

    return code;
}

static int pdf_read_hexstring(pdf_context *ctx, pdf_stream *s)
{
    char *Buffer, *NewBuf = NULL, HexBuf[2];
    unsigned short index = 0;
    short bytes = 0;
    uint32_t size = 256;
    pdf_string *string = NULL;
    int code;

    Buffer = (char *)gs_alloc_bytes(ctx->memory, size, "pdf_read_hexstring");
    if (Buffer == NULL)
        return_error(gs_error_VMerror);

    if (ctx->pdfdebug)
        dmprintf(ctx->memory, " <");

    do {
        do {
            bytes = pdf_read_bytes(ctx, (byte *)HexBuf, 1, 1, s);
            if (bytes == 0 && s->eof)
                break;
            if (bytes <= 0)
                return_error(gs_error_ioerror);
        } while(iswhite(HexBuf[0]));
        if (bytes == 0 && s->eof)
            break;

        if (HexBuf[0] == '>')
            break;

        if (ctx->pdfdebug)
            dmprintf1(ctx->memory, "%c", HexBuf[0]);

        do {
            bytes = pdf_read_bytes(ctx, (byte *)&HexBuf[1], 1, 1, s);
            if (bytes == 0 && s->eof)
                break;
            if (bytes <= 0)
                return_error(gs_error_ioerror);
        } while(iswhite(HexBuf[1]));
        if (bytes == 0 && s->eof)
            break;

        if (!ishex(HexBuf[0]) || !ishex(HexBuf[1]))
            return_error(gs_error_syntaxerror);

        if (ctx->pdfdebug)
            dmprintf1(ctx->memory, "%c", HexBuf[1]);

        Buffer[index] = (fromhex(HexBuf[0]) << 4) + fromhex(HexBuf[1]);

        if (index++ >= size - 1) {
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

#if REFCNT_DEBUG
    string->UID = ctx->UID++;
    dmprintf1(ctx->memory, "Allocated hexstring object with UID %"PRIi64"\n", string->UID);
#endif
    code = pdf_push(ctx, (pdf_obj *)string);
    if (code < 0)
        pdf_free_namestring((pdf_obj *)string);

    return code;
}

static int pdf_read_string(pdf_context *ctx, pdf_stream *s)
{
    char *Buffer, *NewBuf = NULL, octal[3];
    unsigned short index = 0;
    short bytes = 0;
    uint32_t size = 256;
    pdf_string *string = NULL;
    int code, octal_index = 0;
    bool escape = false, skip_eol = false, exit_loop = false;

    Buffer = (char *)gs_alloc_bytes(ctx->memory, size, "pdf_read_string");
    if (Buffer == NULL)
        return_error(gs_error_VMerror);

    do {
        bytes = pdf_read_bytes(ctx, (byte *)&Buffer[index], 1, 1, s);

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

        if (index++ >= size - 1) {
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

#if REFCNT_DEBUG
    string->UID = ctx->UID++;
    dmprintf1(ctx->memory, "Allocated string object with UID %"PRIi64"\n", string->UID);
#endif
    code = pdf_push(ctx, (pdf_obj *)string);
    if (code < 0)
        pdf_free_namestring((pdf_obj *)string);

    return code;
}

static int pdf_array_from_stack(pdf_context *ctx)
{
    uint64_t index = 0;
    pdf_array *a = NULL;
    pdf_obj *o;
    int code;

    code = pdf_count_to_mark(ctx, &index);
    if (code < 0)
        return code;

    a = (pdf_array *)gs_alloc_bytes(ctx->memory, sizeof(pdf_array), "pdf_array_from_stack");
    if (a == NULL)
        return_error(gs_error_VMerror);

    memset(a, 0x00, sizeof(pdf_array));
    a->memory = ctx->memory;
    a->type = PDF_ARRAY;

    a->size = a->entries = index;

    a->values = (pdf_obj **)gs_alloc_bytes(ctx->memory, index * sizeof(pdf_obj *), "pdf_array_from_stack");
    if (a->values == NULL) {
        gs_free_object(a->memory, a, "pdf_array_from_stack error");
        return_error(gs_error_VMerror);
    }

    while (index) {
        o = ctx->stack_top[-1];
        a->values[--index] = o;
        pdf_countup(o);
        pdf_pop(ctx, 1);
    }

    code = pdf_clear_to_mark(ctx);
    if (code < 0)
        return code;

    if (ctx->pdfdebug)
        dmprintf (ctx->memory, " ]\n");

#if REFCNT_DEBUG
    a->UID = ctx->UID++;
    dmprintf1(ctx->memory, "Allocated array object with UID %"PRIi64"\n", a->UID);
#endif
    code = pdf_push(ctx, (pdf_obj *)a);
    if (code < 0)
        pdf_free_array((pdf_obj *)a);

    return code;
}

int pdf_dict_from_stack(pdf_context *ctx)
{
    uint64_t index = 0;
    pdf_dict *d = NULL;
    uint64_t i = 0;
    int code;

    code = pdf_count_to_mark(ctx, &index);
    if (code < 0)
        return code;

    if (index & 1)
        return_error(gs_error_rangecheck);

    d = (pdf_dict *)gs_alloc_bytes(ctx->memory, sizeof(pdf_dict), "pdf_dict_from_stack");
    if (d == NULL)
        return_error(gs_error_VMerror);

    memset(d, 0x00, sizeof(pdf_dict));
    d->memory = ctx->memory;
    d->type = PDF_DICT;

    d->size = d->entries = index >> 1;

    d->keys = (pdf_obj **)gs_alloc_bytes(ctx->memory, d->size * sizeof(pdf_obj *), "pdf_dict_from_stack");
    if (d->keys == NULL) {
        gs_free_object(d->memory, d, "pdf_read_dict error");
        return_error(gs_error_VMerror);
    }

    d->values = (pdf_obj **)gs_alloc_bytes(ctx->memory, d->size * sizeof(pdf_obj *), "pdf_dict_from_stack");
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

    code = pdf_clear_to_mark(ctx);
    if (code < 0)
        return code;

    if (ctx->pdfdebug)
        dmprintf (ctx->memory, "\n >>\n");

#if REFCNT_DEBUG
    d->UID = ctx->UID++;
    dmprintf1(ctx->memory, "Allocated dictionary object with UID %"PRIi64"\n", d->UID);
#endif
    code = pdf_push(ctx, (pdf_obj *)d);
    if (code < 0)
        pdf_free_dict((pdf_obj *)d);

    return code;
}

static int pdf_read_dict(pdf_context *ctx, pdf_stream *s)
{
    int code;
    pdf_obj *o;

    code = pdf_read_token(ctx, s);
    if (code < 0)
        return code;
    if (ctx->stack_top[-1]->type != PDF_DICT_MARK)
        return_error(gs_error_typecheck);

    do {
        code = pdf_read_token(ctx, s);
        if (code < 0)
            return code;
    } while(ctx->stack_top[-1]->type != PDF_DICT);
    return 0;
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

int pdf_dict_get_type(pdf_context *ctx, pdf_dict *d, const char *Key, pdf_obj_type type, pdf_obj **o)
{
    int code;

    code = pdf_dict_get(d, Key, o);
    if (code < 0)
        return code;

    if ((*o)->type != type) {
        if ((*o)->type == PDF_INDIRECT){
            pdf_name *NewKey = NULL;
            pdf_obj *o1 = NULL;
            pdf_indirect_ref *r = (pdf_indirect_ref *)*o;

            code = pdf_dereference(ctx, r->ref_object_num, r->ref_generation_num, &o1);
            pdf_countdown(*o);
            *o = NULL;
            if (code < 0)
                return code;

            code = pdf_make_name(ctx, (byte *)Key, strlen(Key), (pdf_obj **)&NewKey);
            if (code == 0) {
                (void)pdf_dict_put(d, (pdf_obj *)NewKey, o1);
                pdf_countdown(NewKey);
            }
            if (o1->type != type) {
                pdf_countdown(o1);
                return_error(gs_error_typecheck);
            }
            *o = o1;
        } else {
            pdf_countdown(*o);
            *o = NULL;
            return_error(gs_error_typecheck);
        }
    }
    return 0;
}

int pdf_dict_get_int(pdf_context *ctx, pdf_dict *d, const char *Key, int64_t *i)
{
    int code;
    pdf_num *n;

    code = pdf_dict_get_type(ctx, d, Key, PDF_INT, (pdf_obj **)&n);
    if (code < 0)
        return code;

    *i = n->value.i;
    pdf_countdown(n);
    return 0;
}

int pdf_dict_get_number(pdf_context *ctx, pdf_dict *d, const char *Key, double *f)
{
    int code;
    pdf_num *o;

    code = pdf_dict_get(d, Key, (pdf_obj **)&o);
    if (code < 0)
        return code;
    if (o->type == PDF_INDIRECT) {
        pdf_obj *o1 = NULL;
        pdf_indirect_ref *r = (pdf_indirect_ref *)o;

        code = pdf_dereference(ctx, r->ref_object_num, r->ref_generation_num, &o1);
        pdf_countdown(o);
        o = NULL;
        if (code < 0)
            return code;
        o = (pdf_num *)o1;
    }

    if (o->type == PDF_INT) {
        *f = (double)(o->value.i);
    } else {
        if (o->type == PDF_REAL){
            *f = o->value.d;
        } else {
            pdf_countdown(o);
            return_error(gs_error_typecheck);
        }
    }
    pdf_countdown(o);
    return 0;
}

int pdf_dict_put(pdf_dict *d, pdf_obj *Key, pdf_obj *value)
{
    uint64_t i;
    pdf_obj **new_keys, **new_values;
    pdf_name *n;

    if (Key->type != PDF_NAME)
        return_error(gs_error_typecheck);

    /* First, do we have a Key/value pair already ? */
    for (i=0;i< d->entries;i++) {
        n = (pdf_name *)d->keys[i];
        if (n && n->type == PDF_NAME) {
            if (n->length == ((pdf_name *)Key)->length && memcmp((const char *)n->data, ((pdf_name *)Key)->data, n->length) == 0) {
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
        for (i=0;i< d->size;i++) {
            if (d->keys[i] == NULL) {
                d->keys[i] = Key;
                pdf_countup(Key);
                d->values[i] = value;
                pdf_countup(value);
                d->entries++;
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
    memcpy(new_values, d->values, d->size * sizeof(pdf_obj *));

    gs_free_object(d->memory, d->keys, "pdf_dict_put key reallocation");
    gs_free_object(d->memory, d->values, "pdf_dict_put value reallocation");

    d->keys = new_keys;
    d->values = new_values;

    d->keys[d->size] = Key;
    d->values[d->size] = value;
    d->size++;
    d->entries++;
    pdf_countup(Key);
    pdf_countup(value);

    return 0;
}

int pdf_dict_copy(pdf_dict *target, pdf_dict *source)
{
    int i=0, code = 0;

    for (i=0;i< source->entries;i++) {
        code = pdf_dict_put(target, source->keys[i], source->values[i]);
        if (code < 0)
            return code;
    }
    return 0;
}

int pdf_dict_known(pdf_dict *d, const char *Key, bool *known)
{
    int i;
    pdf_obj *t;

    *known = false;
    for (i=0;i< d->entries;i++) {
        t = d->keys[i];

        if (t && t->type == PDF_NAME) {
            if (((pdf_name *)t)->length == strlen(Key) && memcmp(((pdf_name *)t)->data, Key, ((pdf_name *)t)->length) == 0) {
                *known = true;
                break;
            }
        }
    }
    return 0;
}

int pdf_dict_known_by_key(pdf_dict *d, pdf_name *Key, bool *known)
{
    int i;
    pdf_obj *t;

    *known = false;
    for (i=0;i< d->entries;i++) {
        t = d->keys[i];

        if (t && t->type == PDF_NAME) {
            if (((pdf_name *)t)->length == Key->length && memcmp((const char *)((pdf_name *)t)->data, Key->data, ((pdf_name *)t)->length) == 0) {
                *known = true;
                break;
            }
        }
    }
    return 0;
}

int pdf_merge_dicts(pdf_dict *target, pdf_dict *source)
{
    int i, code;
    bool known = false;

    for (i=0;i< source->entries;i++) {
        code = pdf_dict_known_by_key(target, (pdf_name *)source->keys[i], &known);
        if (code < 0)
            return code;
        if (!known) {
            code = pdf_dict_put(target, source->keys[i], source->values[i]);
            if (code < 0)
                return code;
        }
    }
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

int pdf_array_get_type(pdf_context *ctx, pdf_array *a, uint64_t index, pdf_obj_type type, pdf_obj **o)
{
    int code;

    code = pdf_array_get(a, index, o);
    if (code < 0)
        return code;

    if ((*o)->type != type) {
        if ((*o)->type == PDF_INDIRECT){
            pdf_obj *o1 = NULL;
            pdf_indirect_ref *r = (pdf_indirect_ref *)*o;

            code = pdf_dereference(ctx, r->ref_object_num, r->ref_generation_num, &o1);
            pdf_countdown(*o);
            *o = NULL;
            if (code < 0)
                return code;

            (void)pdf_array_put(a, index, o1);

            if (o1->type != type) {
                pdf_countdown(o1);
                return_error(gs_error_typecheck);
            }
            *o = o1;
        } else {
            pdf_countdown(*o);
            *o = NULL;
            return_error(gs_error_typecheck);
        }
    }
    return 0;
}

int pdf_array_get_int(pdf_context *ctx, pdf_array *a, uint64_t index, int64_t *i)
{
    int code;
    pdf_num *n;

    code = pdf_array_get_type(ctx, a, index, PDF_INT, (pdf_obj **)&n);
    if (code < 0)
        return code;
    *i = n->value.i;
    pdf_countdown(n);
    return 0;
}

int pdf_array_get_number(pdf_context *ctx, pdf_array *a, uint64_t index, double *f)
{
    int code;
    pdf_num *n;

    code = pdf_array_get(a, index, (pdf_obj **)&n);
    if (code < 0)
        return code;
    if (n->type == PDF_INT)
        *f = (double)n->value.i;
    else {
        if (n->type == PDF_REAL)
            *f = n->value.d;
        else {
            pdf_countdown(n);
            return_error(gs_error_typecheck);
        }
    }

    pdf_countdown(n);
    return 0;
}

int pdf_array_put(pdf_array *a, uint64_t index, pdf_obj *o)
{
    if (index > a->size)
        return_error(gs_error_rangecheck);

    pdf_countdown(a->values[index]);
    a->values[index] = o;
    pdf_countup(o);
    return 0;
}

static int pdf_skip_comment(pdf_context *ctx, pdf_stream *s)
{
    byte Buffer;
    short bytes = 0;

    if (ctx->pdfdebug)
        dmprintf (ctx->memory, " %%");

    do {
        bytes = pdf_read_bytes(ctx, (byte *)&Buffer, 1, 1, s);
        if (bytes < 0)
            return_error(gs_error_ioerror);

        if (bytes > 0) {
            if (ctx->pdfdebug)
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
static int pdf_read_keyword(pdf_context *ctx, pdf_stream *s)
{
    byte Buffer[256], b;
    unsigned short index = 0;
    short bytes = 0;
    int code;
    pdf_keyword *keyword;

    skip_white(ctx, s);

    do {
        bytes = pdf_read_bytes(ctx, (byte *)&Buffer[index], 1, 1, s);
        if (bytes < 0)
            return_error(gs_error_ioerror);

        if (bytes > 0) {
            if (iswhite(Buffer[index])) {
                break;
            } else {
                if (isdelimiter(Buffer[index])) {
                    pdf_unread(ctx, s, (byte *)&Buffer[index], 1);
                    break;
                }
            }
            index++;
        }
    } while (bytes && index < 255);

    if (index >= 255 || index == 0) {
        if (ctx->pdfstoponerror)
            return_error(gs_error_syntaxerror);
        strcpy(Buffer, "KEYWORD_TOO_LONG");
        index = 16;
    }

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
    keyword->refcnt = 1;

    memcpy(keyword->data, Buffer, index);
    keyword->length = index;
    keyword->key = PDF_NOT_A_KEYWORD;

#if REFCNT_DEBUG
    keyword->UID = ctx->UID++;
    dmprintf1(ctx->memory, "Allocated keyword object with UID %"PRIi64"\n", keyword->UID);
#endif

    if (ctx->pdfdebug)
        dmprintf1(ctx->memory, " %s\n", Buffer);

    switch(Buffer[0]) {
        case 'R':
            if (keyword->length == 1){
                pdf_indirect_ref *o;
                uint64_t obj_num;
                uint32_t gen_num;

                pdf_countdown(keyword);

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
                o->ref_generation_num = gen_num;
                o->ref_object_num = obj_num;

#if REFCNT_DEBUG
                o->UID = ctx->UID++;
                dmprintf1(ctx->memory, "Allocated indirect reference object with UID %"PRIi64"\n", o->UID);
#endif
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
                code = skip_eol(ctx, s);
                if (code < 0)
                    return code;
            }
            else {
                if (keyword->length == 9 && memcmp((const char *)Buffer, "startxref", 9) == 0)
                    keyword->key = PDF_STARTXREF;
            }
            break;
        case 't':
            if (keyword->length == 4 && memcmp((const char *)Buffer, "true", 4) == 0) {
                pdf_bool *o;

                pdf_countdown(keyword);

                o = (pdf_bool *)gs_alloc_bytes(ctx->memory, sizeof(pdf_bool), "pdf_read_keyword");
                if (o == NULL)
                    return_error(gs_error_VMerror);

                memset(o, 0x00, sizeof(pdf_bool));
                o->memory = ctx->memory;
                o->type = PDF_BOOL;
                o->value = true;

#if REFCNT_DEBUG
                o->UID = ctx->UID++;
                dmprintf1(ctx->memory, "Allocated boolean object with UID %"PRIi64"\n", o->UID);
#endif
                code = pdf_push(ctx, (pdf_obj *)o);
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
                pdf_bool *o;

                pdf_countdown(keyword);

                o = (pdf_bool *)gs_alloc_bytes(ctx->memory, sizeof(pdf_bool), "pdf_read_keyword");
                if (o == NULL)
                    return_error(gs_error_VMerror);

                memset(o, 0x00, sizeof(pdf_obj));
                o->memory = ctx->memory;
                o->type = PDF_BOOL;
                o->value = true;

#if REFCNT_DEBUG
                o->UID = ctx->UID++;
                dmprintf1(ctx->memory, "Allocated boolean object with UID %"PRIi64"\n", o->UID);
#endif
                code = pdf_push(ctx, (pdf_obj *)o);
                if (code < 0)
                    pdf_free_object((pdf_obj *)o);
                return code;
            }
            break;
        case 'n':
            if (keyword->length == 4 && memcmp((const char *)Buffer, "null", 4) == 0){
                pdf_obj *o;

                pdf_countdown(keyword);

                o = (pdf_obj *)gs_alloc_bytes(ctx->memory, sizeof(pdf_obj), "pdf_read_keyword");
                if (o == NULL)
                    return_error(gs_error_VMerror);

                memset(o, 0x00, sizeof(pdf_obj));
                o->memory = ctx->memory;
                o->type = PDF_NULL;

#if REFCNT_DEBUG
                o->UID = ctx->UID++;
                dmprintf1(ctx->memory, "Allocated null object with UID %"PRIi64"\n", o->UID);
#endif
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
    pdf_countdown(keyword);

    return code;
}

/* This function reads from the given stream, at the current offset in the stream,
 * a single PDF 'token' and returns it on the stack.
 */
int pdf_read_token(pdf_context *ctx, pdf_stream *s)
{
    int32_t bytes = 0;
    char Buffer[256];
    int code;

    skip_white(ctx, s);

    bytes = pdf_read_bytes(ctx, (byte *)Buffer, 1, 1, s);
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
            if (bytes <= 0)
                return (gs_error_ioerror);
            if (Buffer[1] == '<') {
                if (ctx->pdfdebug)
                    dmprintf (ctx->memory, " <<\n");
                return pdf_mark_stack(ctx, PDF_DICT_MARK);
            } else {
                if (Buffer[1] == '>') {
                    pdf_unread(ctx, s, (byte *)&Buffer[1], 1);
                    return pdf_read_hexstring(ctx, s);
                } else {
                    if (ishex(Buffer[1])) {
                        pdf_unread(ctx, s, (byte *)&Buffer[1], 1);
                        return pdf_read_hexstring(ctx, s);
                    }
                    else
                        return_error(gs_error_syntaxerror);
                }
            }
            break;
        case '>':
            bytes = pdf_read_bytes(ctx, (byte *)&Buffer[1], 1, 1, s);
            if (bytes <= 0)
                return (gs_error_ioerror);
            if (Buffer[1] == '>')
                return pdf_dict_from_stack(ctx);
            else
                return_error(gs_error_syntaxerror);
            break;
        case '(':
            return pdf_read_string(ctx, s);
            break;
        case '[':
            if (ctx->pdfdebug)
                dmprintf (ctx->memory, "[");
            return pdf_mark_stack(ctx, PDF_ARRAY_MARK);
            break;
        case ']':
            return pdf_array_from_stack(ctx);
            break;
        case '%':
            pdf_skip_comment(ctx, s);
            return pdf_read_token(ctx, s);
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
    int64_t i;
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

    do {
        code = pdf_read_token(ctx, s);
        if (code < 0)
            return code;
        if (s->eof)
            return_error(gs_error_syntaxerror);
    }while (ctx->stack_top[-1]->type != PDF_KEYWORD);

    keyword = ((pdf_keyword *)ctx->stack_top[-1]);
    if (keyword->key == PDF_ENDOBJ) {
        pdf_obj *o;

        if (ctx->stack_top - ctx->stack_bot < 2)
            return_error(gs_error_stackunderflow);

        o = ctx->stack_top[-2];

        pdf_pop(ctx, 1);

        o->object_num = objnum;
        o->generation_num = gen;
        code = pdf_add_to_cache(ctx, o);
        return code;
    }
    if (keyword->key == PDF_STREAM) {
        pdf_dict *d = (pdf_dict *)ctx->stack_top[-2];
        gs_offset_t offset;

        skip_eol(ctx, ctx->main_stream);

        offset = pdf_tell(ctx);

        pdf_pop(ctx, 1);

        if (ctx->stack_top - ctx->stack_bot < 1)
            return_error(gs_error_stackunderflow);

        d = (pdf_dict *)ctx->stack_top[-1];

        if (d->type != PDF_DICT) {
            pdf_pop(ctx, 1);
            return_error(gs_error_syntaxerror);
        }
        d->object_num = objnum;
        d->generation_num = gen;
        d->stream_offset = offset;
        code = pdf_add_to_cache(ctx, (pdf_obj *)d);
        if (code < 0) {
            pdf_pop(ctx, 1);
            return code;
        }

        /* This code may be a performance overhead, it simply skips over the stream contents
         * and checks that the stream ends with a 'endstream endobj' pair. We could add a
         * 'go faster' flag for users who are certain their PDF files are well-formed. This
         * could also allow us to skip all kinds of other checking.....
         */

        code = pdf_dict_get_int(ctx, d, "Length", &i);
        if (code < 0) {
            pdf_pop(ctx, 1);
            return code;
        }
        if (i < 0 || (i + offset)> ctx->main_stream_length) {
            pdf_pop(ctx, 1);
            return_error(gs_error_rangecheck);
        }
        code = pdf_seek(ctx, ctx->main_stream, i, SEEK_CUR);
        if (code < 0) {
            pdf_pop(ctx, 1);
            return code;
        }

        code = pdf_read_token(ctx, s);
        if (ctx->stack_top - ctx->stack_bot < 2)
            return_error(gs_error_stackunderflow);

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
        if (ctx->stack_top - ctx->stack_bot < 2)
            return_error(gs_error_stackunderflow);

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
#if REFCNT_DEBUG
        ctx->stack_top[-1]->UID = ctx->UID++;
        dmprintf1(ctx->memory, "Allocated object with UID %"PRIi64"\n", ctx->stack_top[-1]->UID);
#endif
        return 0;
    }
    pdf_pop(ctx, 2);
    return_error(gs_error_syntaxerror);
}

int pdf_read_object_of_type(pdf_context *ctx, pdf_stream *s, pdf_obj_type type)
{
    int code;

    code = pdf_read_object(ctx, s);
    if (code < 0)
        return code;

    if (ctx->stack_top[-1]->type != type) {
        pdf_pop(ctx, 1);
        return_error(gs_error_typecheck);
    }
    return 0;
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

    *o = NULL;

    name = (pdf_name *)gs_alloc_bytes(ctx->memory, sizeof(pdf_name), "pdf_make_name");
    if (name == NULL)
        return_error(gs_error_VMerror);

    memset(name, 0x00, sizeof(pdf_name));
    name->memory = ctx->memory;
    name->type = PDF_NAME;
    name->refcnt = 1;

    NewBuf = (char *)gs_alloc_bytes(ctx->memory, size, "pdf_make_name");
    if (NewBuf == NULL)
        return_error(gs_error_VMerror);

    memcpy(NewBuf, n, size);

    name->data = (unsigned char *)NewBuf;
    name->length = size;

    *o = (pdf_obj *)name;

#if REFCNT_DEBUG
    (*o)->UID = ctx->UID++;
    dmprintf1(ctx->memory, "Allocated name object with UID %"PRIi64"\n", (*o)->UID);
#endif
    return 0;
}

int pdf_alloc_dict(pdf_context *ctx, uint64_t size, pdf_dict **returned)
{
    pdf_dict *returned_dict;

    *returned = NULL;

    returned_dict = (pdf_dict *)gs_alloc_bytes(ctx->memory, sizeof(pdf_dict), "pdf_alloc_dict");
    if (returned_dict == NULL)
        return_error(gs_error_VMerror);

    memset(returned_dict, 0x00, sizeof(pdf_dict));
    returned_dict->memory = ctx->memory;
    returned_dict->type = PDF_DICT;
    returned_dict->refcnt = 1;

    returned_dict->keys = (pdf_obj **)gs_alloc_bytes(ctx->memory, size * sizeof(pdf_obj *), "pdf_alloc_dict");
    if (returned_dict->keys == NULL) {
        gs_free_object(ctx->memory, returned_dict, "pdf_alloc_dict");
        return_error(gs_error_VMerror);
    }
    returned_dict->values = (pdf_obj **)gs_alloc_bytes(ctx->memory, size * sizeof(pdf_obj *), "pdf_alloc_dict");
    if (returned_dict->keys == NULL) {
        gs_free_object(ctx->memory, returned_dict->keys, "pdf_alloc_dict");
        gs_free_object(ctx->memory, returned_dict, "pdf_alloc_dict");
        return_error(gs_error_VMerror);
    }
    memset(returned_dict->keys, 0x00, size * sizeof(pdf_obj *));
    memset(returned_dict->values, 0x00, size * sizeof(pdf_obj *));
    returned_dict->size = size;
    *returned = returned_dict;
#if REFCNT_DEBUG
    returned_dict->UID = ctx->UID++;
    dmprintf1(ctx->memory, "Allocated dictobject with UID %"PRIi64"\n", returned_dict->UID);
#endif
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
    code = sizeof(pdf_obj *);
    code *= ctx->stack_size;
    ctx->stack_limit = ctx->stack_bot + ctx->stack_size;

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

    /* Initially, prefer the XrefStm in a hybrid file */
    ctx->prefer_xrefstm = true;

#if REFCNT_DEBUG
    ctx->UID = 1;
    ctx->freed_objects = 0;
#endif
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
        pdf_countdown(ctx->Trailer);

    if(ctx->Root)
        pdf_countdown(ctx->Root);

    if (ctx->Info)
        pdf_countdown(ctx->Info);

    if (ctx->Pages)
        pdf_countdown(ctx->Pages);

    if (ctx->xref_table) {
        pdf_countdown(ctx->xref_table);
        ctx->xref_table = NULL;
    }

    if (ctx->stack_bot) {
        pdf_clearstack(ctx);
        gs_free_object(ctx->memory, ctx->stack_bot, "pdf_free_context");
    }

    if (ctx->filename) {
        gs_free_object(ctx->memory, ctx->filename, "free copy of filename");
        ctx->filename = NULL;
    }

    if (ctx->main_stream != NULL) {
        sfclose(ctx->main_stream->s);
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
        sfclose(ctx->main_stream->s);
        ctx->main_stream = NULL;
    }
    ctx->main_stream_length = 0;
}

#define BUF_SIZE 2048

static int pdf_repair_add_object(pdf_context *ctx, uint64_t obj, uint64_t gen, gs_offset_t offset)
{
    if (ctx->xref_table == NULL) {
        ctx->xref_table = (xref_table *)gs_alloc_bytes(ctx->memory, sizeof(xref_table), "repair xref table");
        if (ctx->xref_table == NULL) {
            return_error(gs_error_VMerror);
        }
        memset(ctx->xref_table, 0x00, sizeof(xref_table));
        ctx->xref_table->xref = (xref_entry *)gs_alloc_bytes(ctx->memory, (obj + 1) * sizeof(xref_entry), "repair xref table");
        if (ctx->xref_table->xref == NULL){
            gs_free_object(ctx->memory, ctx->xref_table, "failed to allocate xref table entries for repair");
            ctx->xref_table = NULL;
            return_error(gs_error_VMerror);
        }
        memset(ctx->xref_table->xref, 0x00, (obj + 1) * sizeof(xref_entry));
        ctx->xref_table->memory = ctx->memory;
        ctx->xref_table->type = PDF_XREF_TABLE;
        ctx->xref_table->xref_size = obj + 1;
#if REFCNT_DEBUG
        ctx->xref_table->UID = ctx->UID++;
        dmprintf1(ctx->memory, "Allocated xref table with UID %"PRIi64"\n", ctx->xref_table->UID);
#endif
        pdf_countup(ctx->xref_table);
    } else {
        if (ctx->xref_table->xref_size < (obj + 1)) {
            xref_entry *new_xrefs;

            new_xrefs = (xref_entry *)gs_alloc_bytes(ctx->memory, (obj + 1) * sizeof(xref_entry), "read_xref_stream allocate xref table entries");
            if (new_xrefs == NULL){
                pdf_countdown(ctx->xref_table);
                ctx->xref_table = NULL;
                return_error(gs_error_VMerror);
            }
            memset(new_xrefs, 0x00, (obj + 1) * sizeof(xref_entry));
            memcpy(new_xrefs, ctx->xref_table->xref, ctx->xref_table->xref_size * sizeof(xref_entry));
            gs_free_object(ctx->memory, ctx->xref_table->xref, "reallocated xref entries");
            ctx->xref_table->xref = new_xrefs;
            ctx->xref_table->xref_size = obj + 1;
        }
    }
    ctx->xref_table->xref[obj].compressed = false;
    ctx->xref_table->xref[obj].free = false;
    ctx->xref_table->xref[obj].object_num = obj;
    ctx->xref_table->xref[obj].u.uncompressed.generation_num = gen;
    ctx->xref_table->xref[obj].u.uncompressed.offset = offset;
    return 0;
}

int repair_pdf_file(pdf_context *ctx)
{
    int code;
    gs_offset_t offset;
    uint64_t object_num, generation_num;
    int i;

    pdf_clearstack(ctx);

    if(ctx->pdfdebug)
        dmprintf(ctx->memory, "%% Error encountered in opening PDF file, attempting repair\n");

    /* First pass, identify all the objects of the form x y obj */
    pdf_seek(ctx, ctx->main_stream, 0, SEEK_SET);

    do {
        offset = pdf_tell(ctx);
        do {
            code = pdf_read_token(ctx, ctx->main_stream);
            if (code < 0)
                return code;
            if (ctx->stack_top - ctx->stack_bot > 0) {
                if (ctx->stack_top[-1]->type == PDF_KEYWORD) {
                    pdf_keyword *k = (pdf_keyword *)ctx->stack_top[-1];
                    pdf_num *n;

                    if (k->key == PDF_OBJ) {
                        if (ctx->stack_top - ctx->stack_bot < 3 || ctx->stack_top[-2]->type != PDF_INT || ctx->stack_top[-2]->type != PDF_INT) {
                            pdf_clearstack(ctx);
                            return_error(gs_error_syntaxerror);
                        }
                        n = (pdf_num *)ctx->stack_top[-3];
                        object_num = n->value.i;
                        n = (pdf_num *)ctx->stack_top[-2];
                        generation_num = n->value.i;
                        pdf_clearstack(ctx);

                        do {
                            code = pdf_read_token(ctx, ctx->main_stream);
                            if (code < 0)
                                return code;
                            if (ctx->stack_top[-1]->type == PDF_KEYWORD){
                                pdf_keyword *k = (pdf_keyword *)ctx->stack_top[-1];

                                if (k->key == PDF_ENDOBJ) {
                                    code = pdf_repair_add_object(ctx, object_num, generation_num, offset);
                                    if (code < 0)
                                        return code;
                                    pdf_clearstack(ctx);
                                    break;
                                } else {
                                    if (k->key == PDF_STREAM) {
                                        char Buffer[10], test[] = "endstream";
                                        int index = 0;

                                        do {
                                            code = pdf_read_bytes(ctx, (byte *)&Buffer[index], 1, 1, ctx->main_stream);
                                            if (code < 0)
                                                return code;
                                            if (Buffer[index] == test[index])
                                                index++;
                                            else
                                                index = 0;
                                        } while (index < 9 && ctx->main_stream->eof == false);
                                        do {
                                            code = pdf_read_token(ctx, ctx->main_stream);
                                            if (code < 0)
                                                return code;
                                            if (ctx->stack_top[-1]->type == PDF_KEYWORD){
                                                pdf_keyword *k = (pdf_keyword *)ctx->stack_top[-1];
                                                if (k->key == PDF_ENDOBJ) {
                                                    code = pdf_repair_add_object(ctx, object_num, generation_num, offset);
                                                    if (code < 0)
                                                        return code;
                                                    break;
                                                }
                                            }
                                        }while(ctx->main_stream->eof == false);

                                        pdf_clearstack(ctx);
                                        break;
                                    } else {
                                        pdf_clearstack(ctx);
                                        break;
                                    }
                                }
                            }
                        } while(1);
                        break;
                    } else {
                        if (k->key == PDF_ENDOBJ) {
                            code = pdf_repair_add_object(ctx, object_num, generation_num, offset);
                            if (code < 0)
                                return code;
                            pdf_clearstack(ctx);
                        } else
                            pdf_clearstack(ctx);
                    }
                }
            }
        } while (ctx->main_stream->eof == false);
    } while(ctx->main_stream->eof == false);

    if (ctx->main_stream->eof) {
        sfclose(ctx->main_stream->s);
        ctx->main_stream->s = sfopen(ctx->filename, "r", ctx->memory);
        if (ctx->main_stream->s == NULL)
            return_error(gs_error_ioerror);
        ctx->main_stream->eof = false;
    }

    /* Second pass, examine every object we have located to see if its an ObjStm */
    if (ctx->xref_table == NULL || ctx->xref_table->xref_size < 1)
        return_error(gs_error_syntaxerror);

    for (i=1;i < ctx->xref_table->xref_size;i++) {
        if (ctx->xref_table->xref[i].object_num != 0) {
            /* At this stage, all the objects we've found must be uncompressed */
            pdf_seek(ctx, ctx->main_stream, ctx->xref_table->xref[i].u.uncompressed.offset, SEEK_SET);
            do {
                code = pdf_read_token(ctx, ctx->main_stream);
                if (code < 0)
                    return code;
                if (ctx->stack_top[-1]->type == PDF_KEYWORD) {
                    pdf_keyword *k = (pdf_keyword *)ctx->stack_top[-1];

                    if (k->key == PDF_OBJ){
                        continue;
                    }
                    if (k->key == PDF_ENDOBJ) {
                        if (ctx->stack_top - ctx->stack_bot > 1) {
                            if (ctx->stack_top[-2]->type == PDF_DICT) {
                                 pdf_dict *d = (pdf_dict *)ctx->stack_top[-2];
                                 pdf_obj *o = NULL;

                                 code = pdf_dict_get(d, "Type", &o);
                                 if (code < 0 && code != gs_error_undefined){
                                     pdf_clearstack(ctx);
                                     return code;
                                 }
                                 if (o != NULL) {
                                     pdf_name *n = (pdf_name *)o;

                                     if (n->type == PDF_NAME) {
                                         if (n->length == 7 && memcmp(n->data, "Catalog", 7) == 0) {
                                             ctx->Root = (pdf_dict *)ctx->stack_top[-2];
                                             pdf_countup(ctx->Root);
                                         }
                                     }
                                 }
                            }
                        }
                        pdf_clearstack(ctx);
                        break;
                    }
                    if (k->key == PDF_STREAM) {
                        pdf_dict *d;
                        bool known;

                        d = (pdf_dict *)ctx->stack_top[-2];
                        code = pdf_dict_known(d, "ObjStm", &known);
                        if (code < 0) {
                            pdf_clearstack(ctx);
                            return code;
                        }
                        if (known == true) {
                        }
                        pdf_clearstack(ctx);
                        break;
                    }
                }
            } while (1);
        }
    }

    return 0;
}

static int read_xref_stream_entries(pdf_context *ctx, pdf_stream *s, uint64_t first, uint64_t last, uint64_t *W)
{
    uint i, j = 0;
    uint32_t type = 0;
    uint64_t objnum = 0, gen = 0;
    byte *Buffer;
    int64_t bytes = 0;
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

        entry->compressed = false;
        entry->free = false;
        entry->object_num = i;
        entry->cache = NULL;

        switch(type) {
            case 0:
                entry->free = true;
                entry->u.uncompressed.offset = objnum;         /* For free objects we use the offset to store the object number of the next free object */
                entry->u.uncompressed.generation_num = gen;    /* And the generation number is the numebr to use if this object is used again */
                break;
            case 1:
                entry->u.uncompressed.offset = objnum;
                entry->u.uncompressed.generation_num = gen;
                break;
            case 2:
                entry->compressed = true;
                entry->u.compressed.compressed_stream_num = objnum;   /* The object number of the compressed stream */
                entry->u.compressed.object_index = gen;               /* And the index of the object within the stream */
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
    pdf_name *n;
    pdf_array *a;
    int64_t size;
    int64_t num;
    int64_t W[3];

    code = pdf_dict_get_type(ctx, d, "Type", PDF_NAME, (pdf_obj **)&n);
    if (code < 0)
        return code;

    if (n->length != 4 || memcmp(n->data, "XRef", 4) != 0) {
        pdf_countdown(n);
        return_error(gs_error_syntaxerror);
    }
    pdf_countdown(n);

    code = pdf_dict_get_int(ctx, d, "Size", &size);
    if (code < 0)
        return code;

    if (size < 0)
        return_error(gs_error_rangecheck);

    /* If this is the first xref stream then allocate the xref table and store the trailer */
    if (ctx->xref_table == NULL) {
        ctx->xref_table = (xref_table *)gs_alloc_bytes(ctx->memory, sizeof(xref_table), "read_xref_stream allocate xref table");
        if (ctx->xref_table == NULL) {
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
#if REFCNT_DEBUG
        ctx->xref_table->UID = ctx->UID++;
        dmprintf1(ctx->memory, "Allocated xref table with UID %"PRIi64"\n", ctx->xref_table->UID);
#endif
        pdf_countup(ctx->xref_table);

        ctx->Trailer = d;
        pdf_countup(d);
    }

    pdf_seek(ctx, ctx->main_stream, d->stream_offset, SEEK_SET);
    code = pdf_filter(ctx, d, s, &XRefStrm, false);
    if (code < 0) {
        pdf_countdown(ctx->xref_table);
        ctx->xref_table = NULL;
        return code;
    }

    code = pdf_dict_get_type(ctx, d, "W", PDF_ARRAY, (pdf_obj **)&a);
    if (code < 0) {
        pdf_close_file(ctx, XRefStrm);
        pdf_countdown(ctx->xref_table);
        ctx->xref_table = NULL;
        return code;
    }

    if (a->entries != 3) {
        pdf_countdown(a);
        pdf_close_file(ctx, XRefStrm);
        pdf_countdown(ctx->xref_table);
        ctx->xref_table = NULL;
        return_error(gs_error_rangecheck);
    }
    for (i=0;i<3;i++) {
        code = pdf_array_get_int(ctx, a, (uint64_t)i, (int64_t *)&W[i]);
        if (code < 0) {
            pdf_countdown(a);
            pdf_close_file(ctx, XRefStrm);
            pdf_countdown(ctx->xref_table);
            ctx->xref_table = NULL;
            return code;
        }
    }
    pdf_countdown(a);

    code = pdf_dict_get_type(ctx, d, "Index", PDF_ARRAY, (pdf_obj **)&a);
    if (code == gs_error_undefined) {
        code = read_xref_stream_entries(ctx, XRefStrm, 0, size - 1, (uint64_t *)W);
        if (code < 0) {
            pdf_close_file(ctx, XRefStrm);
            pdf_countdown(ctx->xref_table);
            ctx->xref_table = NULL;
            return code;
        }
    } else {
        int64_t start, end;

        if (code < 0) {
            pdf_close_file(ctx, XRefStrm);
            pdf_countdown(ctx->xref_table);
            ctx->xref_table = NULL;
            return code;
        }

        if (a->entries & 1) {
            pdf_countdown(a);
            pdf_close_file(ctx, XRefStrm);
            pdf_countdown(ctx->xref_table);
            ctx->xref_table = NULL;
            return_error(gs_error_rangecheck);
        }

        for (i=0;i < a->entries;i+=2){
            code = pdf_array_get_int(ctx, a, (uint64_t)i, &start);
            if (code < 0) {
                pdf_countdown(a);
                pdf_close_file(ctx, XRefStrm);
                pdf_countdown(ctx->xref_table);
                ctx->xref_table = NULL;
                return code;
            }

            code = pdf_array_get_int(ctx, a, (uint64_t)i+1, &end);
            if (code < 0) {
                pdf_countdown(a);
                pdf_countdown(start);
                pdf_close_file(ctx, XRefStrm);
                pdf_countdown(ctx->xref_table);
                ctx->xref_table = NULL;
                return code;
            }

            code = read_xref_stream_entries(ctx, XRefStrm, start, start + end - 1, (uint64_t *)W);
            if (code < 0) {
                pdf_countdown(a);
                pdf_close_file(ctx, XRefStrm);
                pdf_countdown(ctx->xref_table);
                ctx->xref_table = NULL;
                return code;
            }
        }
    }
    pdf_countdown(a);

    pdf_close_file(ctx, XRefStrm);

    code = pdf_dict_get_int(ctx, d, "Prev", &num);
    if (code == gs_error_undefined)
        return 0;

    if (code < 0)
        return code;

    if (num < 0 || num > ctx->main_stream_length)
        return_error(gs_error_rangecheck);

    if (pdf_loop_detector_check_object(ctx, num) == true)
        return_error(gs_error_circular_reference);
    else {
        code = pdf_loop_detector_add_object(ctx, num);
        if (code < 0)
            return code;
    }

    if(ctx->pdfdebug)
        dmprintf(ctx->memory, "%% Reading /Prev xref\n");

    pdf_seek(ctx, s, num, SEEK_SET);

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
                        d->stream_offset = pdf_tell(ctx);

                        d->object_num = obj_num;
                        d->generation_num = gen_num;
                        pdf_countup(d);
                        pdf_pop(ctx, 1);

                        code = pdf_process_xref_stream(ctx, d, ctx->main_stream);
                        if (code < 0) {
                            pdf_countdown(d);
                            return (repair_pdf_file(ctx));
                        }
                        pdf_countdown(d);
                        break;
                    }
                    if (keyword->key == PDF_ENDOBJ) {
                        /* Something went wrong, this is not a stream dictionary */
                        pdf_pop(ctx, 3);
                        return(repair_pdf_file(ctx));
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

static int read_xref_section(pdf_context *ctx, pdf_stream *s)
{
    int code = 0, i, j;
    pdf_obj *o = NULL;
    uint64_t start = 0, size = 0;
    int64_t bytes = 0;
    char Buffer[21];

    if (ctx->pdfdebug)
        dmprintf(ctx->memory, "\n%% Reading xref section\n");

    code = pdf_read_token(ctx, ctx->main_stream);

    if (code < 0)
        return code;

    if (ctx->stack_top - ctx->stack_bot < 1)
        return_error(gs_error_stackunderflow);

    o = ctx->stack_top[-1];
    if (o->type == PDF_KEYWORD)
        return 0;

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

    if (ctx->pdfdebug)
        dmprintf2(ctx->memory, "\n%% Section starts at %d and has %d entries\n", (unsigned int) start, (unsigned int)size);

    if (size > 0) {
        if (ctx->xref_table == NULL) {
            ctx->xref_table = (xref_table *)gs_alloc_bytes(ctx->memory, sizeof(xref_table), "read_xref_stream allocate xref table");
            if (ctx->xref_table == NULL)
                return_error(gs_error_VMerror);
            memset(ctx->xref_table, 0x00, sizeof(xref_table));

            ctx->xref_table->xref = (xref_entry *)gs_alloc_bytes(ctx->memory, (start + size) * sizeof(xref_entry), "read_xref_stream allocate xref table entries");
            if (ctx->xref_table->xref == NULL){
                pdf_countdown(ctx->xref_table);
                ctx->xref_table = NULL;
                return_error(gs_error_VMerror);
            }
#if REFCNT_DEBUG
            ctx->xref_table->UID = ctx->UID++;
            dmprintf1(ctx->memory, "Allocated xref table with UID %"PRIi64"\n", ctx->xref_table->UID);
#endif

            memset(ctx->xref_table->xref, 0x00, (start + size) * sizeof(xref_entry));
            ctx->xref_table->memory = ctx->memory;
            ctx->xref_table->type = PDF_XREF_TABLE;
            ctx->xref_table->xref_size = start + size;
            pdf_countup(ctx->xref_table);
        } else {
            if (start + size > ctx->xref_table->xref_size) {
                xref_entry *new_xrefs;

                new_xrefs = (xref_entry *)gs_alloc_bytes(ctx->memory, (start + size) * sizeof(xref_entry), "read_xref_stream allocate xref table entries");
                if (new_xrefs == NULL){
                    pdf_countdown(ctx->xref_table);
                    ctx->xref_table = NULL;
                    return_error(gs_error_VMerror);
                }
                memset(new_xrefs, 0x00, (start + size) * sizeof(xref_entry));
                memcpy(new_xrefs, ctx->xref_table->xref, ctx->xref_table->xref_size * sizeof(xref_entry));
                gs_free_object(ctx->memory, ctx->xref_table->xref, "reallocated xref entries");
                ctx->xref_table->xref = new_xrefs;
                ctx->xref_table->xref_size = start + size;
            }
        }
    }

    skip_white(ctx, s);
    for (i=0;i< size;i++){
        xref_entry *entry = &ctx->xref_table->xref[i + start];
        unsigned char free;

        bytes = pdf_read_bytes(ctx, (byte *)Buffer, 1, 20, s);
        if (bytes < 20)
            return_error(gs_error_ioerror);
        j = 19;
        while (Buffer[j] != 0x0D && Buffer[j] != 0x0A) {
            pdf_unread(ctx, s, (byte *)&Buffer[j], 1);
            j--;
        }
        Buffer[j] = 0x00;
        if (entry->object_num != 0)
            continue;

        sscanf(Buffer, "%ld %d %c", &entry->u.uncompressed.offset, &entry->u.uncompressed.generation_num, &free);
        entry->compressed = false;
        entry->object_num = i + start;
        if (free == 'f')
            entry->free = true;
        if(free == 'n')
            entry->free = false;
    }

    return 0;
}

static int read_xref(pdf_context *ctx, pdf_stream *s)
{
    int code = 0, i, j;
    pdf_obj **o = NULL;
    pdf_keyword *k;
    pdf_dict *d = NULL;
    uint64_t start = 0, size = 0;
    int64_t bytes = 0;
    int64_t num;
    char Buffer[21];

    do {
        o = ctx->stack_top;
        code = read_xref_section(ctx, s);
        if (code < 0)
            return code;
        if (ctx->stack_top - o > 0) {
            k = (pdf_keyword *)ctx->stack_top[-1];
            if(k->type != PDF_KEYWORD || k->key != PDF_TRAILER)
                return_error(gs_error_syntaxerror);
            else {
                pdf_pop(ctx, 1);
                break;
            }
        }
    } while (1);

    code = pdf_read_dict(ctx, ctx->main_stream);
    if (code < 0)
        return code;

    d = (pdf_dict *)ctx->stack_top[-1];
    if (d->type != PDF_DICT) {
        pdf_pop(ctx, 1);
        return_error(gs_error_typecheck);
    }

    if (ctx->Trailer == NULL) {
        ctx->Trailer = d;
        pdf_countup(d);
    }

    /* We have the Trailer dictionary. First up check for hybrid files. These have the initial
     * xref starting at 0 and size of 0. In this case the /Size entry in the trailer dictionary
     * must tell us how large the xref is, and we need to allocate our xref table anyway.
     */
    if (ctx->xref_table == NULL && size == 0) {
        int64_t size;

        code = pdf_dict_get_int(ctx, d, "Size", &size);
        if (code < 0) {
            pdf_pop(ctx, 2);
            return code;
        }
        if (size < 0) {
            pdf_pop(ctx, 2);
            return_error(gs_error_rangecheck);
        }

        ctx->xref_table = (xref_table *)gs_alloc_bytes(ctx->memory, sizeof(xref_table), "read_xref_stream allocate xref table");
        if (ctx->xref_table == NULL) {
            pdf_pop(ctx, 2);
            return_error(gs_error_VMerror);
        }
        memset(ctx->xref_table, 0x00, sizeof(xref_table));
#if REFCNT_DEBUG
        ctx->xref_table->UID = ctx->UID++;
        dmprintf1(ctx->memory, "Allocated xref table with UID %"PRIi64"\n", ctx->xref_table->UID);
#endif

        ctx->xref_table->xref = (xref_entry *)gs_alloc_bytes(ctx->memory, size * sizeof(xref_entry), "read_xref_stream allocate xref table entries");
        if (ctx->xref_table->xref == NULL){
            pdf_pop(ctx, 2);
            pdf_countdown(ctx->xref_table);
            ctx->xref_table = NULL;
            return_error(gs_error_VMerror);
        }

        memset(ctx->xref_table->xref, 0x00, size * sizeof(xref_entry));
        ctx->xref_table->memory = ctx->memory;
        ctx->xref_table->type = PDF_XREF_TABLE;
        ctx->xref_table->xref_size = size;
        pdf_countup(ctx->xref_table);
    }

    /* Now check if this is a hybrid file. */
    if (ctx->Trailer == d) {
        code = pdf_dict_get_int(ctx, d, "XRefStm", &num);
        if (code < 0 && code != gs_error_undefined) {
            pdf_pop(ctx, 2);
            return code;
        }
        if (code == 0)
            ctx->is_hybrid = true;
    } else
        code = gs_error_undefined;

    if (code == 0 && ctx->prefer_xrefstm) {
        if (ctx->pdfdebug)
            dmprintf(ctx->memory, "%% File is a hybrid, containing xref table and xref stream. Using the stream.\n");

        pdf_pop(ctx, 2);

        if (pdf_loop_detector_check_object(ctx, num) == true)
            return_error(gs_error_circular_reference);
        else {
            code = pdf_loop_detector_add_object(ctx, num);
            if (code < 0)
                return code;
        }
        /* Because of the way the code works when we read a file which is a pure
         * xref stream file, we need to read the first integer of 'x y obj'
         * because the xref stream decoding code expects that to be on the stack.
         */
        pdf_seek(ctx, s, num, SEEK_SET);

        code = pdf_read_token(ctx, ctx->main_stream);
        if (code < 0)
            return code;

        code = pdf_read_xref_stream_dict(ctx, ctx->main_stream);
        if (code < 0)
            return code;
    }

    /* Not a hybrid file, so now check if this is a modified file and has
     * previous xref entries.
     */
    code = pdf_dict_get_int(ctx, d, "Prev", &num);
    if (code < 0) {
        pdf_pop(ctx, 2);
        if (code == gs_error_undefined)
            return 0;
        else
            return code;
    }
    pdf_pop(ctx, 1);

    if (num < 0 || num > ctx->main_stream_length)
        return_error(gs_error_rangecheck);

    if (pdf_loop_detector_check_object(ctx, num) == true)
        return_error(gs_error_circular_reference);
    else {
        code = pdf_loop_detector_add_object(ctx, num);
        if (code < 0)
            return code;
    }

    code = pdf_seek(ctx, s, num, SEEK_SET);
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
        pdf_pop(ctx, 1);
        return_error(gs_error_typecheck);
    }
}

int pdf_open_pdf_file(pdf_context *ctx, char *filename)
{
    byte *Buffer = NULL;
    char *s = NULL;
    float version = 0.0;
    gs_offset_t Offset = 0;
    int64_t bytes = 0;
    bool found = false;

//    if (ctx->pdfdebug)
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

    /* Determine file size */
    pdf_seek(ctx, ctx->main_stream, 0, SEEK_END);
    ctx->main_stream_length = stell(ctx->main_stream->s);
    Offset = BUF_SIZE;
    bytes = BUF_SIZE;
    pdf_seek(ctx, ctx->main_stream, 0, SEEK_SET);

    bytes = Offset = min(BUF_SIZE, ctx->main_stream_length);

    if (ctx->pdfdebug)
        dmprintf(ctx->memory, "%% Reading header\n");

    bytes = pdf_read_bytes(ctx, Buffer, 1, Offset, ctx->main_stream);
    if (bytes <= 0) {
        emprintf(ctx->memory, "Failed to read any bytes from file\n");
        cleanup_pdf_open_file(ctx, Buffer);
        return_error(gs_error_ioerror);
    }

    /* First check for existence of header */
    s = strstr((char *)Buffer, "%PDF");
    if (s == NULL) {
        if (ctx->pdfdebug)
            dmprintf1(ctx->memory, "%% File %s does not appear to be a PDF file (no %%PDF in first 2Kb of file)\n", filename);
        ctx->pdf_errors |= E_PDF_NOHEADER;
    } else {
        /* Now extract header version (may be overridden later) */
        if (sscanf(s + 5, "%f", &version) != 1) {
            if (ctx->pdfdebug)
                dmprintf(ctx->memory, "%% Unable to read PDF version from header\n");
            ctx->HeaderVersion = 0;
            ctx->pdf_errors |= E_PDF_NOHEADERVERSION;
        }
        else {
            ctx->HeaderVersion = version;
        }
        if (ctx->pdfdebug)
            dmprintf1(ctx->memory, "%% Found header, PDF version is %f\n", ctx->HeaderVersion);
    }

    /* Jump to EOF and scan backwards looking for startxref */
    pdf_seek(ctx, ctx->main_stream, 0, SEEK_END);

    if (ctx->pdfdebug)
        dmprintf(ctx->memory, "%% Searching for 'startxerf' keyword\n");

    bytes = Offset;
    do {
        byte *last_lineend = NULL;
        uint32_t read;

        if (pdf_seek(ctx, ctx->main_stream, ctx->main_stream_length - Offset, SEEK_SET) != 0) {
            emprintf1(ctx->memory, "File is smaller than %"PRIi64" bytes\n", (int64_t)Offset);
            cleanup_pdf_open_file(ctx, Buffer);
            return_error(gs_error_ioerror);
        }
        read = pdf_read_bytes(ctx, Buffer, 1, bytes, ctx->main_stream);

        if (read <= 0) {
            emprintf1(ctx->memory, "Failed to read %"PRIi64" bytes from file\n", (int64_t)bytes);
            cleanup_pdf_open_file(ctx, Buffer);
            return_error(gs_error_ioerror);
        }

        read = bytes = read + (BUF_SIZE - bytes);

        while(read) {
            if (memcmp(Buffer + read - 9, "startxref", 9) == 0) {
                found = true;
                break;
            } else {
                if (Buffer[read] == 0x0a || Buffer[read] == 0x0d)
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

    gs_free_object(ctx->memory, Buffer, "PDF interpreter - allocate working buffer for file validation");
    return 0;
}

int pdf_read_xref(pdf_context *ctx)
{
    byte *Buffer = NULL;
    int code = 0;

    Buffer = gs_alloc_bytes(ctx->memory, BUF_SIZE, "PDF interpreter - allocate working buffer for file validation");
    if (Buffer == NULL) {
        cleanup_pdf_open_file(ctx, Buffer);
        return_error(gs_error_VMerror);
    }

    code = pdf_init_loop_detector(ctx);
    if (code < 0)
        return code;

    code = pdf_loop_detector_add_object(ctx, ctx->startxref);
    if (code < 0)
        return code;

    if (ctx->startxref != 0) {
        if (ctx->pdfdebug)
            dmprintf(ctx->memory, "%% Trying to read 'xref' token for xref table, or 'int int obj' for an xref stream\n");

        /* Read the xref(s) */
        pdf_seek(ctx, ctx->main_stream, ctx->startxref, SEEK_SET);

        code = pdf_read_token(ctx, ctx->main_stream);
        if (code < 0) {
            dmprintf(ctx->memory, "Failed to read any token at the startxref location\n");
            (void)pdf_free_loop_detector(ctx);
            return(repair_pdf_file(ctx));
        }

        if (ctx->stack_top - ctx->stack_bot < 1)
            return_error(gs_error_undefined);

        if (((pdf_obj *)ctx->stack_top[-1])->type == PDF_KEYWORD && ((pdf_keyword *)ctx->stack_top[-1])->key == PDF_XREF) {
            /* Read old-style xref table */
            pdf_pop(ctx, 1);
            code = read_xref(ctx, ctx->main_stream);
            if (code < 0) {
                (void)pdf_free_loop_detector(ctx);
                return(repair_pdf_file(ctx));
            }
        } else {
            code = pdf_read_xref_stream_dict(ctx, ctx->main_stream);
            if (code < 0){
                (void)pdf_free_loop_detector(ctx);
                return(repair_pdf_file(ctx));
            }
        }
    } else {
        /* Attempt to repair PDF file */
        (void)pdf_free_loop_detector(ctx);
        return(repair_pdf_file(ctx));
    }

    if(ctx->pdfdebug && ctx->xref_table) {
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

    code = pdf_free_loop_detector(ctx);
    if (code < 0)
        return code;

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

        code = pdf_dereference(ctx, ((pdf_indirect_ref *)o1)->ref_object_num,  ((pdf_indirect_ref *)o1)->ref_generation_num, &o);
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

    code = pdf_dict_get_type(ctx, (pdf_dict *)o1, "Type", PDF_NAME, &o);
    if (code < 0) {
        pdf_countdown(o1);
        return code;
    }
    if (((pdf_name *)o)->length != 7 || memcmp(((pdf_name *)o)->data, "Catalog", 7) != 0){
        pdf_countdown(o);
        pdf_countdown(o1);
        return_error(gs_error_syntaxerror);
    }
    pdf_countdown(o);

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

        code = pdf_dereference(ctx, ((pdf_indirect_ref *)o1)->ref_object_num,  ((pdf_indirect_ref *)o1)->ref_generation_num, &o);
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

        code = pdf_dereference(ctx, ((pdf_indirect_ref *)o1)->ref_object_num,  ((pdf_indirect_ref *)o1)->ref_generation_num, &o);
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

    code = pdf_dict_get_int(ctx, (pdf_dict *)o1, "Count", (int64_t *)&ctx->num_pages);
    if (code < 0) {
        pdf_countdown(o1);
        return code;
    }

    /* We don't pdf_countdown(o1) now, because we've transferred our
     * reference to the pointer in the pdf_context structure.
     */
    ctx->Pages = (pdf_dict *)o1;
    return 0;
}

static int pdf_get_page_dict(pdf_context *ctx, pdf_dict *d, uint64_t page_num, uint64_t *page_offset, pdf_dict **target, pdf_dict *inherited)
{
    int i, code = 0;
    pdf_array *Kids = NULL;
    pdf_indirect_ref *node = NULL;
    pdf_dict *child = NULL;
    pdf_name *Type = NULL;
    pdf_dict *inheritable = NULL;
    int64_t num;
    bool known;

    if (ctx->pdfdebug)
        dmprintf1(ctx->memory, "%% Finding page dictionary for page %"PRIi64"\n", page_num + 1);

    /* if we are being passed any inherited values from our parent, copy them now */
    if (inherited != NULL) {
        code = pdf_alloc_dict(ctx, inherited->size, &inheritable);
        if (code < 0)
            return code;
        code = pdf_dict_copy(inheritable, inherited);
        if (code < 0) {
            pdf_countdown(inheritable);
            return code;
        }
    }

    code = pdf_dict_get_int(ctx, d, "Count", &num);
    if (code < 0) {
        pdf_countdown(inheritable);
        return code;
    }
    if (num < 0 || (num + *page_offset) > ctx->num_pages) {
        pdf_countdown(inheritable);
        return_error(gs_error_rangecheck);
    }
    if (num + *page_offset < page_num) {
        pdf_countdown(inheritable);
        *page_offset += num;
        return 1;
    }
    /* The requested page is a descendant of this node */

    /* Check for inheritable keys, if we find any copy them to the 'inheritable' dictionary at this level */
    code = pdf_dict_known(d, "Resources", &known);
    if (code < 0) {
        pdf_countdown(inheritable);
        return code;
    }
    if (known) {
        pdf_name *Key;
        pdf_obj *object;

        if (inheritable == NULL) {
            code = pdf_alloc_dict(ctx, 0, &inheritable);
            if (code < 0)
                return code;
        }

        code = pdf_make_name(ctx, (byte *)"Resources", 9, (pdf_obj **)&Key);
        if (code < 0) {
            pdf_countdown(inheritable);
            return code;
        }
        code = pdf_dict_get(d, "Resources", &object);
        if (code < 0) {
            pdf_countdown(Key);
            pdf_countdown(inheritable);
            return code;
        }
        code = pdf_dict_put(inheritable, (pdf_obj *)Key, object);
        pdf_countdown(Key);
        pdf_countdown(object);
    }
    code = pdf_dict_known(d, "MediaBox", &known);
    if (code < 0) {
        pdf_countdown(inheritable);
        return code;
    }
    if (known) {
        pdf_name *Key;
        pdf_obj *object;

        if (inheritable == NULL) {
            code = pdf_alloc_dict(ctx, 0, &inheritable);
            if (code < 0)
                return code;
        }

        code = pdf_make_name(ctx, (byte *)"MediaBox", 9, (pdf_obj **)&Key);
        if (code < 0) {
            pdf_countdown(inheritable);
            return code;
        }
        code = pdf_dict_get(d, "MediaBox", &object);
        if (code < 0) {
            pdf_countdown(Key);
            pdf_countdown(inheritable);
            return code;
        }
        code = pdf_dict_put(inheritable, (pdf_obj *)Key, object);
        pdf_countdown(Key);
        pdf_countdown(object);
    }
    code = pdf_dict_known(d, "CropBox", &known);
    if (code < 0) {
        pdf_countdown(inheritable);
        return code;
    }
    if (known) {
        pdf_name *Key;
        pdf_obj *object;

        if (inheritable == NULL) {
            code = pdf_alloc_dict(ctx, 0, &inheritable);
            if (code < 0)
                return code;
        }

        code = pdf_make_name(ctx, (byte *)"CropBox", 9, (pdf_obj **)&Key);
        if (code < 0) {
            pdf_countdown(inheritable);
            return code;
        }
        code = pdf_dict_get(d, "CropBox", &object);
        if (code < 0) {
            pdf_countdown(Key);
            pdf_countdown(inheritable);
            return code;
        }
        code = pdf_dict_put(inheritable, (pdf_obj *)Key, object);
        pdf_countdown(Key);
        pdf_countdown(object);
    }
    code = pdf_dict_known(d, "Rotate", &known);
    if (code < 0) {
        pdf_countdown(inheritable);
        return code;
    }
    if (known) {
        pdf_name *Key;
        pdf_obj *object;

        if (inheritable == NULL) {
            code = pdf_alloc_dict(ctx, 0, &inheritable);
            if (code < 0)
                return code;
        }

        code = pdf_make_name(ctx, (byte *)"Rotate", 9, (pdf_obj **)&Key);
        if (code < 0) {
            pdf_countdown(inheritable);
            return code;
        }
        code = pdf_dict_get(d, "Rotate", &object);
        if (code < 0) {
            pdf_countdown(Key);
            pdf_countdown(inheritable);
            return code;
        }
        code = pdf_dict_put(inheritable, (pdf_obj *)Key, object);
        pdf_countdown(Key);
        pdf_countdown(object);
    }

    /* Get the Kids array */
    code = pdf_dict_get_type(ctx, d, "Kids", PDF_ARRAY, (pdf_obj **)&Kids);
    if (code < 0) {
        pdf_countdown(inheritable);
        pdf_countdown(Kids);
        return code;
    }

    /* Check each entry in the Kids array */
    for (i = 0;i < Kids->entries;i++) {
        code = pdf_array_get(Kids, i, (pdf_obj **)&node);
        if (code < 0) {
            pdf_countdown(inheritable);
            pdf_countdown(Kids);
            return code;
        }
        if (node->type != PDF_INDIRECT && node->type != PDF_DICT) {
            pdf_countdown(inheritable);
            pdf_countdown(Kids);
            pdf_countdown(node);
            return_error(gs_error_typecheck);
        }
        if (node->type == PDF_INDIRECT) {
            code = pdf_dereference(ctx, node->ref_object_num, node->ref_generation_num, (pdf_obj **)&child);
            if (code < 0) {
                pdf_countdown(inheritable);
                pdf_countdown(Kids);
                pdf_countdown(node);
                return code;
            }
            if (child->type != PDF_DICT) {
                pdf_countdown(inheritable);
                pdf_countdown(Kids);
                pdf_countdown(node);
                pdf_countdown(child);
                return_error(gs_error_typecheck);
            }
            /* If its an intermediate node, store it in the page_table, if its a leaf node
             * then don't store it. Instead we create a special dictionary of our own which
             * has a /Type of /PageRef and a /PageRef key which is the indirect reference
             * to the page. However in this case we pass on the actual page dictionary to
             * the Kids processing below. If we didn't then we'd fall foul of the loop
             * detection by dereferencing the same object twice.
             * This is tedious, but it means we don't store all the page dictionaries in
             * the Pages tree, because page dictionaries can be large and we generally
             * only use them once. If processed in order we only dereference each page
             * dictionary once, any other order will dereference each page twice. (or more
             * if we render the same page multiple times).
             */
            code = pdf_dict_get_type(ctx, child, "Type", PDF_NAME, (pdf_obj **)&Type);
            if (code < 0) {
                pdf_countdown(inheritable);
                pdf_countdown(Kids);
                pdf_countdown(child);
                pdf_countdown(node);
                return code;
            }
            if (Type->length == 5 && memcmp(Type->data, "Pages", 5) == 0) {
                code = pdf_array_put(Kids, i, (pdf_obj *)child);
                if (code < 0) {
                    pdf_countdown(inheritable);
                    pdf_countdown(Kids);
                    pdf_countdown(child);
                    pdf_countdown(Type);
                    pdf_countdown(node);
                    return code;
                }
            }
            if (Type->length == 4 && memcmp(Type->data, "Page", 4) == 0) {
                /* Make a 'PageRef' entry (just stores an indirect reference to the actual page)
                 * and store that in the Kids array for future reference. But pass on the
                 * dereferenced Page dictionary, in case this is the target page.
                 */
                pdf_dict *leaf_dict = NULL;
                pdf_name *Key = NULL, *Key1 = NULL;

                code = pdf_alloc_dict(ctx, 0, &leaf_dict);
                if (code == 0) {
                    code = pdf_make_name(ctx, (byte *)"PageRef", 7, (pdf_obj **)&Key);
                    if (code == 0) {
                        code = pdf_dict_put(leaf_dict, (pdf_obj *)Key, (pdf_obj *)node);
                        if (code == 0){
                            code = pdf_make_name(ctx, (byte *)"Type", 4, (pdf_obj **)&Key);
                            if (code == 0){
                                code = pdf_make_name(ctx, (byte *)"PageRef", 7, (pdf_obj **)&Key1);
                                if (code == 0) {
                                    code = pdf_dict_put(leaf_dict, (pdf_obj *)Key, (pdf_obj *)Key1);
                                    if (code == 0)
                                        code = pdf_array_put(Kids, i, (pdf_obj *)leaf_dict);
                                }
                            }
                        }
                    }
                }
                pdf_countdown(Key);
                pdf_countdown(Key1);
                pdf_countdown(leaf_dict);
                if (code < 0) {
                    pdf_countdown(inheritable);
                    pdf_countdown(Kids);
                    pdf_countdown(child);
                    pdf_countdown(Type);
                    pdf_countdown(node);
                    return code;
                }
            }
            pdf_countdown(Type);
            pdf_countdown(node);
        } else {
            child = (pdf_dict *)node;
        }
        /* Check the type, if its a Pages entry, then recurse. If its a Page entry, is it the one we want */
        code = pdf_dict_get_type(ctx, child, "Type", PDF_NAME, (pdf_obj **)&Type);
        if (code == 0) {
            if (Type->length == 5 && memcmp(Type->data, "Pages", 5) == 0) {
                code = pdf_dict_get_int(ctx, child, "Count", &num);
                if (code == 0) {
                    if (num < 0 || (num + *page_offset) > ctx->num_pages) {
                        code = gs_error_rangecheck;
                    } else {
                        if (num + *page_offset <= page_num) {
                            pdf_countdown(child);
                            child = NULL;
                            pdf_countdown(Type);
                            Type = NULL;
                            *page_offset += num;
                        } else {
                            code = pdf_get_page_dict(ctx, child, page_num, page_offset, target, inheritable);
                            pdf_countdown(Type);
                            pdf_countdown(Kids);
                            pdf_countdown(child);
                            return 0;
                        }
                    }
                }
            } else {
                if (Type->length == 7 && memcmp(Type->data, "PageRef", 7) == 0) {
                    pdf_countdown(Type);
                    Type = NULL;
                    if ((i + *page_offset) == page_num) {
                        pdf_indirect_ref *o = NULL;
                        pdf_dict *d = NULL;
                        code = pdf_dict_get(child, "PageRef", (pdf_obj **)&o);
                        if (code == 0) {
                            code = pdf_dereference(ctx, o->ref_object_num, o->ref_generation_num, (pdf_obj **)&d);
                            if (code == 0) {
                                if (inheritable != NULL) {
                                    code = pdf_merge_dicts(d, inheritable);
                                    pdf_countdown(inheritable);
                                    inheritable = NULL;
                                }
                                if (code == 0) {
                                    pdf_countdown(Kids);
                                    *target = d;
                                    pdf_countup(*target);
                                    pdf_countdown(d);
                                    return 0;
                                }
                            }
                        }
                    } else
                        pdf_countdown(child);
                } else {
                    if (Type->length == 4 && memcmp(Type->data, "Page", 4) == 0) {
                        pdf_countdown(Type);
                        Type = NULL;
                        if ((i + *page_offset) == page_num) {
                            if (inheritable != NULL) {
                                code = pdf_merge_dicts(child, inheritable);
                            }
                            if (code == 0) {
                                pdf_countdown(inheritable);
                                pdf_countdown(Kids);
                                *target = child;
                                pdf_countup(*target);
                                pdf_countdown(child);
                                return 0;
                            }
                        } else
                            pdf_countdown(child);
                    } else
                        code = gs_error_typecheck;
                }
            }
        }
        if (code < 0) {
            pdf_countdown(inheritable);
            pdf_countdown(Kids);
            pdf_countdown(child);
            pdf_countdown(Type);
            return code;
        }
    }
    /* Positive return value indicates we did not find the target below this node, try the next one */
    pdf_countdown(inheritable);
    return 1;
}

static int split_bogus_operator(pdf_context *ctx)
{
    /* FIXME Need to write this, place holder for now */
    return 0;
}

#define K1(a) (a)
#define K2(a, b) ((a << 8) + b)
#define K3(a, b, c) ((a << 16) + (b << 8) + c)

static int pdf_interpret_stream_operator(pdf_context *ctx, pdf_stream *source)
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
        code = split_bogus_operator(ctx);
        if (code < 0)
            return code;
    } else {
        for (i=0;i < keyword->length;i++) {
            op = (op << 8) + keyword->data[i];
        }
        switch(op) {
            case K1('b'):           /* closepath, fill, stroke */
                pdf_pop(ctx, 1);
                code = pdf_b(ctx);
                break;
            case K1('B'):           /* fill, stroke */
                pdf_pop(ctx, 1);
                code = pdf_B(ctx);
                break;
            case K2('b','*'):       /* closepath, eofill, stroke */
                pdf_pop(ctx, 1);
                code = pdf_b_star(ctx);
                break;
            case K2('B','*'):       /* eofill, stroke */
                pdf_pop(ctx, 1);
                code = pdf_B_star(ctx);
                break;
            case K2('B','I'):       /* begin inline image */
                pdf_pop(ctx, 1);
                code = pdf_BI(ctx);
                break;
            case K3('B','D','C'):   /* begin marked content sequence with property list */
            case K3('B','M','C'):   /* begin marked content sequence */
            case K2('B','T'):       /* begin text */
            case K2('B','X'):       /* begin compatibility section */
                pdf_clearstack(ctx);
                break;
            case K1('c'):           /* curveto */
                pdf_pop(ctx, 1);
                code = pdf_curveto(ctx);
                break;
            case K2('c','m'):       /* concat */
                pdf_pop(ctx, 1);
                code = pdf_concat(ctx);
                break;
            case K2('C','S'):       /* set stroke colour space */
            case K2('c','s'):       /* set non-stroke colour space */
                pdf_clearstack(ctx);
                break;
            case K1('d'):           /* set dash params */
                pdf_pop(ctx, 1);
                code = pdf_setdash(ctx);
                break;
            case K2('E','I'):       /* end inline image */
                pdf_pop(ctx, 1);
                code = pdf_EI(ctx);
                break;
            case K2('d','0'):       /* set type 3 font glyph width */
            case K2('d','1'):       /* set type 3 font glyph width and bounding box */
            case K2('D','o'):       /* invoke named XObject */
            case K2('D','P'):       /* define marked content point with property list */
            case K3('E','M','C'):   /* end marked content sequence */
            case K2('E','T'):       /* end text */
            case K2('E','X'):       /* end compatibility section */
                pdf_clearstack(ctx);
                break;
            case K1('f'):           /* fill */
                pdf_pop(ctx, 1);
                code = pdf_fill(ctx);
                break;
            case K1('F'):           /* fill (obselete operator) */
                pdf_pop(ctx, 1);
                code = pdf_fill(ctx);
                break;
            case K2('f','*'):       /* eofill */
                pdf_pop(ctx, 1);
                code = pdf_eofill(ctx);
                break;
            case K1('G'):           /* setgray for stroke */
                pdf_pop(ctx, 1);
                code = pdf_setgraystroke(ctx);
                break;
            case K1('g'):           /* setgray for non-stroke */
                pdf_pop(ctx, 1);
                code = pdf_setgrayfill(ctx);
                break;
            case K2('g','s'):       /* set graphics state from dictionary */
                pdf_clearstack(ctx);
                break;
            case K1('h'):           /* closepath */
                pdf_pop(ctx, 1);
                code = pdf_closepath(ctx);
                break;
            case K1('i'):           /* setflat */
                pdf_pop(ctx, 1);
                code = pdf_setflat(ctx);
                break;
            case K2('I','D'):       /* begin inline image data */
                pdf_pop(ctx, 1);
                code = pdf_ID(ctx, source);
                break;
            case K1('j'):           /* setlinejoin */
                pdf_pop(ctx, 1);
                code = pdf_setlinejoin(ctx);
                break;
            case K1('J'):           /* setlinecap */
                pdf_pop(ctx, 1);
                code = pdf_setlinecap(ctx);
                break;
            case K1('K'):           /* setcmyk for non-stroke */
                pdf_pop(ctx, 1);
                code = pdf_setcmykstroke(ctx);
                break;
            case K1('k'):           /* setcmyk for non-stroke */
                pdf_pop(ctx, 1);
                code = pdf_setcmykfill(ctx);
                break;
            case K1('l'):           /* lineto */
                pdf_pop(ctx, 1);
                code = pdf_lineto(ctx);
                break;
            case K1('m'):           /* moveto */
                pdf_pop(ctx, 1);
                code = pdf_moveto(ctx);
                break;
            case K1('M'):           /* setmiterlimit */
                pdf_pop(ctx, 1);
                code = pdf_setmiterlimit(ctx);
                break;
            case K2('M','P'):       /* define marked content point */
                pdf_clearstack(ctx);
                break;
            case K1('n'):           /* newpath */
                pdf_pop(ctx, 1);
                code = pdf_newpath(ctx);
                break;
            case K1('q'):           /* gsave */
                pdf_pop(ctx, 1);
                code = pdf_gsave(ctx);
                break;
            case K1('Q'):           /* grestore */
                pdf_pop(ctx, 1);
                code = pdf_grestore(ctx);
                break;
            case K2('r','e'):       /* append rectangle */
                pdf_pop(ctx, 1);
                code = pdf_rectpath(ctx);
                break;
            case K2('R','G'):       /* set rgb colour for stroke */
                pdf_pop(ctx, 1);
                code = pdf_setrgbstroke(ctx);
                break;
            case K2('r','g'):       /* set rgb colour for non-stroke */
                pdf_pop(ctx, 1);
                code = pdf_setrgbfill(ctx);
                break;
            case K2('r','i'):       /* set rendering intent */
                pdf_clearstack(ctx);
                break;
            case K1('s'):           /* closepath, stroke */
                pdf_pop(ctx, 1);
                code = pdf_closepath_stroke(ctx);
                break;
            case K1('S'):           /* stroke */
                pdf_pop(ctx, 1);
                code = pdf_stroke(ctx);
                break;
            case K2('S','C'):       /* set colour for stroke */
            case K2('s','c'):       /* set colour for non-stroke */
            case K3('S','C','N'):   /* set special colour for stroke */
            case K3('s','c','n'):   /* set special colour for non-stroke */
            case K2('s','h'):       /* fill with sahding pattern */
            case K2('T','*'):       /* Move to start of next text line */
            case K2('T','c'):       /* set character spacing */
            case K2('T','d'):       /* move text position */
            case K2('T','D'):       /* Move text position, set leading */
            case K2('T','f'):       /* set font and size */
            case K2('T','j'):       /* show text */
            case K2('T','J'):       /* show text with individual glyph positioning */
            case K2('T','L'):       /* set text leading */
            case K2('T','m'):       /* set text matrix */
            case K2('T','r'):       /* set text rendering mode */
            case K2('T','s'):       /* set text rise */
            case K2('T','w'):       /* set word spacing */
            case K2('T','z'):       /* set text matrix */
                pdf_clearstack(ctx);
                break;
            case K1('v'):           /* append curve (initial point replicated) */
                pdf_pop(ctx, 1);
                code = pdf_v_curveto(ctx);
                break;
            case K1('w'):           /* setlinewidth */
                pdf_pop(ctx, 1);
                code = pdf_setlinewidth(ctx);
                break;
            case K1('W'):           /* clip */
                pdf_pop(ctx, 1);
                code = pdf_clip(ctx);
                break;
            case K2('W','*'):       /* eoclip */
                pdf_pop(ctx, 1);
                code = pdf_eoclip(ctx);
                break;
            case K1('y'):           /* append curve (final point replicated) */
                pdf_pop(ctx, 1);
                code = pdf_y_curveto(ctx);
                break;
            case K1('\''):          /* move to next line and show text */
            case K1('"'):           /* set word and character spacing, move to next line, show text */
                pdf_clearstack(ctx);
                break;
            default:
                code = split_bogus_operator(ctx);
                if (code < 0)
                    return code;
                break;
       }
    }
    return code;
}

static int pdf_interpret_content_stream(pdf_context *ctx, pdf_dict *stream_dict)
{
    int code;
    pdf_stream *compressed_stream;
    pdf_keyword *keyword;

    code = pdf_seek(ctx, ctx->main_stream, stream_dict->stream_offset, SEEK_SET);
    if (code < 0)
        return code;

    code = pdf_filter(ctx, stream_dict, ctx->main_stream, &compressed_stream, false);
    if (code < 0)
        return code;

    do {
        code = pdf_read_token(ctx, compressed_stream);
        if (code < 0) {
            pdf_close_file(ctx, compressed_stream);
            return code;
        }

        if (ctx->stack_top - ctx->stack_bot <= 0) {
            if(compressed_stream->eof == true)
                break;
        }

        if (ctx->stack_top[-1]->type == PDF_KEYWORD) {
            keyword = (pdf_keyword *)ctx->stack_top[-1];

            switch(keyword->key) {
                case PDF_ENDSTREAM:
                    pdf_close_file(ctx, compressed_stream);
                    pdf_clearstack(ctx);
                    return 0;
                    break;
                case PDF_NOT_A_KEYWORD:
                    code = pdf_interpret_stream_operator(ctx, compressed_stream);
                    if (code < 0) {
                        pdf_close_file(ctx, compressed_stream);
                        pdf_clearstack(ctx);
                        return code;
                    }
                    break;
                default:
                    ctx->pdf_errors |= E_PDF_NOENDSTREAM;
                    pdf_close_file(ctx, compressed_stream);
                    pdf_clearstack(ctx);
                    return_error(gs_error_typecheck);
                    break;
            }
        }
        if(compressed_stream->eof == true)
            break;
    }while(1);

    pdf_close_file(ctx, compressed_stream);
    return 0;
}

static int pdf_process_page_contents(pdf_context *ctx, pdf_dict *page_dict)
{
    int i, code = 0;
    pdf_obj *o, *o1;

    code = pdf_dict_get(page_dict, "Contents", &o);
    if (code < 0)
        return code;

    if (o->type == PDF_INDIRECT) {
        if (((pdf_indirect_ref *)o)->ref_object_num == page_dict->object_num)
            return_error(gs_error_circular_reference);

        code = pdf_dereference(ctx, ((pdf_indirect_ref *)o)->ref_object_num, ((pdf_indirect_ref *)o)->ref_generation_num, &o1);
        pdf_countdown(o);
        if (code < 0)
            return code;
        o = o1;
    }

    if (o->type == PDF_ARRAY) {
        pdf_array *a = (pdf_array *)o;

        for (i=0;i < a->size; i++) {
            pdf_indirect_ref *r;
            code = pdf_array_get(a, i, (pdf_obj **)&r);
            if (code < 0) {
                pdf_countdown(o);
                return code;
            }
            if (r->type == PDF_DICT) {
                code = pdf_interpret_content_stream(ctx, (pdf_dict *)r);
                pdf_countdown(r);
                if (code < 0) {
                    pdf_countdown(o);
                    return(code);
                }
            } else {
                if (r->type != PDF_INDIRECT) {
                    pdf_countdown(o);
                    pdf_countdown(r);
                    return_error(gs_error_typecheck);
                } else {
                    if (r->ref_object_num == page_dict->object_num) {
                        pdf_countdown(o);
                        pdf_countdown(r);
                        return_error(gs_error_circular_reference);
                    }
                    code = pdf_dereference(ctx, r->ref_object_num, r->ref_generation_num, &o1);
                    pdf_countdown(r);
                    if (code < 0) {
                        pdf_countdown(o);
                        return code;;
                    }
                    if (o1->type != PDF_DICT) {
                        pdf_countdown(o);
                        return_error(gs_error_typecheck);
                    }
                    code = pdf_interpret_content_stream(ctx, (pdf_dict *)o1);
                    pdf_countdown(o1);
                    if (code < 0) {
                        pdf_countdown(o);
                        return code;
                    }
                }
            }
        }
    } else {
        if (o->type == PDF_DICT) {
            code = pdf_interpret_content_stream(ctx, (pdf_dict *)o);
        } else {
            pdf_countdown(o);
            return_error(gs_error_typecheck);
        }
    }
    pdf_countdown(o);
    return code;
}

static int pdf_check_page_transparency(pdf_context *ctx, pdf_dict *page_dict, bool *transparent)
{
    *transparent = false;
    return 0;
}

int pdf_set_media_size(pdf_context *ctx, pdf_dict *page_dict)
{
    gs_c_param_list list;
    gs_param_float_array fa;
    pdf_array *a = NULL, *default_media = NULL;
    float fv[2];
    double d[4];
    int code;
    uint64_t i;

    gs_c_param_list_write(&list, ctx->memory);

    code = pdf_dict_get_type(ctx, page_dict, "MediaBox", PDF_ARRAY, (pdf_obj **)&default_media);
    if (code < 0)
        return 0;

    if (ctx->usecropbox) {
        if (a != NULL)
            pdf_countdown(a);
        (void)pdf_dict_get_type(ctx, page_dict, "CropBox", PDF_ARRAY, (pdf_obj **)&a);
    }
    if (ctx->useartbox) {
        if (a != NULL)
            pdf_countdown(a);
        (void)pdf_dict_get_type(ctx, page_dict, "ArtBox", PDF_ARRAY, (pdf_obj **)&a);
    }
    if (ctx->usebleedbox) {
        if (a != NULL)
            pdf_countdown(a);
        (void)pdf_dict_get_type(ctx, page_dict, "BBox", PDF_ARRAY, (pdf_obj **)&a);
    }
    if (ctx->usetrimbox) {
        if (a != NULL)
            pdf_countdown(a);
        (void)pdf_dict_get_type(ctx, page_dict, "MediaBox", PDF_ARRAY, (pdf_obj **)&a);
    }
    if (a == NULL)
        a = default_media;

    for (i=0;i<4;i++) {
        code = pdf_array_get_number(ctx, a, i, &d[i]);
    }

    fv[0] = (float)(d[2] - d[0]);
    fv[1] = (float)(d[3] - d[1]);
    fa.persistent = false;
    fa.data = fv;
    fa.size = 2;

    code = param_write_float_array((gs_param_list *)&list, ".MediaSize", &fa);
    if (code >= 0)
    {
        gx_device *dev = gs_currentdevice(ctx->pgs);

        gs_c_param_list_read(&list);
        code = gs_putdeviceparams(dev, (gs_param_list *)&list);
        if (code < 0) {
            gs_c_param_list_release(&list);
            return code;
        }
    }
    gs_c_param_list_release(&list);
    return 0;
}

int pdf_render_page(pdf_context *ctx, uint64_t page_num)
{
    int code;
    uint64_t page_offset = 0;
    pdf_dict *page_dict = NULL;
    bool uses_transparency;

    if (page_num > ctx->num_pages)
        return_error(gs_error_rangecheck);

    if (ctx->pdfdebug)
        dmprintf1(ctx->memory, "%% Processing Page %"PRIi64" content stream\n", page_num + 1);

    code = pdf_init_loop_detector(ctx);
    if (code < 0)
        return code;

    code = pdf_loop_detector_add_object(ctx, ctx->Pages->object_num);
    if (code < 0) {
        pdf_free_loop_detector(ctx);
        return code;
    }

    code = pdf_get_page_dict(ctx, ctx->Pages, page_num, &page_offset, &page_dict, NULL);
    pdf_free_loop_detector(ctx);
    if (code < 0)
        return code;

    if (code > 0)
        return_error(gs_error_unknownerror);

    code = pdf_check_page_transparency(ctx, page_dict, &uses_transparency);
    if (code < 0) {
        pdf_countdown(page_dict);
        return code;
    }

    code = pdf_set_media_size(ctx, page_dict);
    if (code < 0) {
        pdf_countdown(page_dict);
        return code;
    }

    code = pdf_process_page_contents(ctx, page_dict);
    if (code < 0) {
        pdf_countdown(page_dict);
        return code;
    }

    pdf_countdown(page_dict);

    return pl_finish_page(ctx->memory->gs_lib_ctx->top_of_system,
                          ctx->pgs, 1, true);
}

/* These functions are used by the 'PL' implementation, eventually we will */
/* need to have custom PostScript operators to process the file or at      */
/* (least pages from it).                                                  */

int pdf_close_pdf_file(pdf_context *ctx)
{
    if (ctx->main_stream) {
        if (ctx->main_stream->s) {
            sfclose(ctx->main_stream->s);
            ctx->main_stream = NULL;
        }
        gs_free_object(ctx->memory, ctx->main_stream, "Closing main PDF file");
    }
    return 0;
}

int pdf_process_pdf_file(pdf_context *ctx, char *filename)
{
    int code = 0, i;
    pdf_obj *o;

    ctx->filename = (char *)gs_alloc_bytes(ctx->memory, strlen(filename) + 1, "copy of filename");
    if (ctx->filename == NULL)
        return_error(gs_error_VMerror);
    strcpy(ctx->filename, filename);

    code = pdf_open_pdf_file(ctx, filename);
    if (code < 0) {
        return code;
    }

    code = pdf_read_xref(ctx);
    if (code < 0) {
        if (ctx->is_hybrid) {
            /* If its a hybrid file, and we failed to read the XrefStm, try
             * again, but this time read the xref table instead.
             */
            ctx->pdf_errors |= E_PDF_BADXREFSTREAM;
            pdf_countdown(ctx->xref_table);
            ctx->xref_table = NULL;
            ctx->prefer_xrefstm = false;
            code = pdf_read_xref(ctx);
            if (code < 0)
                return code;
        } else {
            ctx->pdf_errors |= E_PDF_BADXREF;
            return code;
        }
    }

    if (ctx->Trailer) {
        code = pdf_dict_get(ctx->Trailer, "Encrypt", &o);
        if (code < 0 && code != gs_error_undefined)
            return code;
        if (code == 0) {
            dmprintf(ctx->memory, "Encrypted PDF files not yet supported.\n");
            return 0;
        }
    }

    if (ctx->Trailer) {
        code = pdf_read_Root(ctx);
        if (code < 0) {
            /* If we couldn#'t find the Root object, and we were using the XrefStm
             * from a hybrid file, then try again, but this time use the xref table
             */
            if (code == gs_error_undefined && ctx->is_hybrid && ctx->prefer_xrefstm) {
                ctx->pdf_errors |= E_PDF_BADXREFSTREAM;
                pdf_countdown(ctx->xref_table);
                ctx->xref_table = NULL;
                ctx->prefer_xrefstm = false;
                code = pdf_read_xref(ctx);
                if (code < 0) {
                    ctx->pdf_errors |= E_PDF_BADXREF;
                    return code;
                }
                code = pdf_read_Root(ctx);
                if (code < 0)
                    return code;
            } else
                return code;
        }
    }

    if (ctx->Trailer) {
        code = pdf_read_Info(ctx);
        if (code < 0 && code != gs_error_undefined) {
            if (ctx->pdfstoponerror)
                return code;
            pdf_clearstack(ctx);
        }
    }

    if (!ctx->Root) {
        dmprintf(ctx->memory, "Catalog dictionary not located in file, unable to proceed\n");
        return_error(gs_error_syntaxerror);
    }

    code = pdf_read_Pages(ctx);
    if (code < 0)
        return code;

    for (i=0;i < ctx->num_pages;i++) {
        code = pdf_render_page(ctx, i);
        if (code < 0)
            return code;
    }

    code = sfclose(ctx->main_stream->s);
    ctx->main_stream = NULL;
    return code;
}

