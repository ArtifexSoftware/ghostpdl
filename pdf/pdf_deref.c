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

/* Functions to deal with dereferencing indirect objects
 * for the PDF interpreter. In here we also keep the code
 * for dealing with the object cache, because the dereferencing
 * functions are currently the only place that deals with it.
 */

#include "pdf_int.h"
#include "pdf_stack.h"
#include "pdf_loop_detect.h"
#include "strmio.h"
#include "stream.h"
#include "pdf_file.h"
#include "pdf_misc.h"
#include "pdf_dict.h"
#include "pdf_array.h"
#include "pdf_deref.h"

/* Start with the object caching functions */

/* given an object, create a cache entry for it. If we have too many entries
 * then delete the leat-recently-used cache entry. Make the new entry be the
 * most-recently-used entry. The actual entries are attached to the xref table
 * (as well as being a double-linked list), because we detect an existing
 * cache entry by seeing that the xref table for the object number has a non-NULL
 * 'cache' member.
 * So we need to update the xref as well if we add or delete cache entries.
 */
static int pdfi_add_to_cache(pdf_context *ctx, pdf_obj *o)
{
    pdf_obj_cache_entry *entry;

    if (ctx->xref_table->xref[o->object_num].cache != NULL) {
#if DEBUG_CACHE
        dmprintf1(ctx->memory, "Attempting to add object %d to cache when the object is already cached!\n", o->object_num);
#endif
        return_error(gs_error_unknownerror);
    }

    if (o->object_num > ctx->xref_table->xref_size)
        return_error(gs_error_rangecheck);

    if (ctx->cache_entries == MAX_OBJECT_CACHE_SIZE)
    {
#if DEBUG_CACHE
        dbgmprintf(ctx->memory, "Cache full, evicting LRU\n");
#endif
        if (ctx->cache_LRU) {
            entry = ctx->cache_LRU;
            ctx->cache_LRU = entry->next;
            if (entry->next)
                ((pdf_obj_cache_entry *)entry->next)->previous = NULL;
            ctx->xref_table->xref[entry->o->object_num].cache = NULL;
            pdfi_countdown(entry->o);
            ctx->cache_entries--;
            gs_free_object(ctx->memory, entry, "pdfi_add_to_cache, free LRU");
        } else
            return_error(gs_error_unknownerror);
    }
    entry = (pdf_obj_cache_entry *)gs_alloc_bytes(ctx->memory, sizeof(pdf_obj_cache_entry), "pdfi_add_to_cache");
    if (entry == NULL)
        return_error(gs_error_VMerror);

    memset(entry, 0x00, sizeof(pdf_obj_cache_entry));

    entry->o = o;
    pdfi_countup(o);
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

/* Given an existing cache entry, promote it to be the most-recently-used
 * cache entry.
 */
static void pdfi_promote_cache_entry(pdf_context *ctx, pdf_obj_cache_entry *cache_entry)
{
    if (ctx->cache_MRU && cache_entry != ctx->cache_MRU) {
        if ((pdf_obj_cache_entry *)cache_entry->next != NULL)
            ((pdf_obj_cache_entry *)cache_entry->next)->previous = cache_entry->previous;
        if ((pdf_obj_cache_entry *)cache_entry->previous != NULL)
            ((pdf_obj_cache_entry *)cache_entry->previous)->next = cache_entry->next;
        else {
            /* the existing entry is the current least recently used, we need to make the 'next'
             * cache entry into the LRU.
             */
            ctx->cache_LRU = cache_entry->next;
        }
        cache_entry->next = NULL;
        cache_entry->previous = ctx->cache_MRU;
        ctx->cache_MRU->next = cache_entry;
        ctx->cache_MRU = cache_entry;
    }
    return;
}

/* This one's a bit of an oddity, its used for fonts. When we build a PDF font object
 * we want the object cache to reference *that* object, not the dictionary which was
 * read out of the PDF file, so this allows us to replace the font dictionary in the
 * cache with the actual font object, so that later dereferences will get this font
 * object.
 */
int replace_cache_entry(pdf_context *ctx, pdf_obj *o)
{
    xref_entry *entry;
    pdf_obj_cache_entry *cache_entry;

    /* Limited error checking here, we assume that things like the
     * validity of the object (eg not a free oobject) have already been handled.
     */

    entry = &ctx->xref_table->xref[o->object_num];
    cache_entry = entry->cache;

    if (cache_entry == NULL) {
        return(pdfi_add_to_cache(ctx, o));
    } else {
        if (cache_entry->o != NULL)
            pdfi_countdown(cache_entry->o);
        cache_entry->o = o;
        pdfi_countup(o);
        pdfi_promote_cache_entry(ctx, cache_entry);
    }
    return 0;
}

/* Now the dereferencing functions */

/*
 * Technically we can accept a stream other than the main PDF file stream here. This is
 * really for the case of compressed objects where we read tokens from the compressed
 * stream, but it also (with some judicious tinkering) allows us to layer a SubFileDecode
 * on top of the main file stream, which may be useful. Note that this cannot work with
 * objects in compressed object streams! They should always pass a value of 0 for the stream_offset.
 * The stream_offset is the offset from the start of the underlying uncompressed PDF file of
 * the stream we are using. See the comments below when keyword is PDF_STREAM.
 */

/* Determine if a PDF object is in a compressed ObjStm. Returns < 0
 * for an error, 0 if it is not in a compressed ObjStm and 1 if it is.
 * Currently errors are inmpossible. This is only used by the decryption code
 * to determine if a string is in a compressed object stream, if it is then
 * it can't be used for decryption.
 */
int is_compressed_object(pdf_context *ctx, uint32_t obj, uint32_t gen)
{
    xref_entry *entry;

    /* Can't possibly be a compressed object before we have finished reading
     * the xref.
     */
    if (ctx->xref_table == NULL)
        return 0;

    entry = &ctx->xref_table->xref[obj];

    if (entry->compressed)
        return 1;

    return 0;
}

/* We should never read a 'stream' keyword from a compressed object stream
 * so this case should never end up here.
 */
static int pdfi_read_stream_object(pdf_context *ctx, pdf_c_stream *s, gs_offset_t stream_offset,
                                   uint32_t objnum, uint32_t gen)
{
    int code = 0;
    int64_t i;
    pdf_keyword *keyword = NULL;
    pdf_dict *dict = NULL;
    gs_offset_t offset;
    pdf_stream *stream_obj = NULL;

    pdfi_skip_eol(ctx, s);

    /* Strange code time....
     * If we are using a stream which is *not* the PDF uncompressed main file stream
     * then doing stell on it will only tell us how many bytes have been read from
     * that stream, it won't tell us the underlying file position. So we add on the
     * 'unread' bytes, *and* we add on the position of the start of the stream in
     * the actual main file. This is all done so that we can check the /Length
     * of the object. Note that this will *only* work for regular objects it can
     * not be used for compressed object streams, but those don't need checking anyway
     * they have a different mechanism altogether and should never get here.
     */
    offset = stell(s->s) - s->unread_size + stream_offset;
    code = pdfi_seek(ctx, ctx->main_stream, offset, SEEK_SET);

    if (pdfi_count_stack(ctx) < 1)
        return_error(gs_error_stackunderflow);

    dict = (pdf_dict *)ctx->stack_top[-1];
    dict->indirect_num = dict->object_num = objnum;
    dict->indirect_gen = dict->generation_num = gen;

    if (dict->type != PDF_DICT) {
        pdfi_pop(ctx, 1);
        return_error(gs_error_syntaxerror);
    }

    /* Convert the dict into a stream */
    code = pdfi_obj_dict_to_stream(ctx, dict, &stream_obj);
    if (code < 0) {
        pdfi_pop(ctx, 1);
        return code;
    }
    /* Pop off the dict and push the stream */
    pdfi_pop(ctx, 1);
    dict = NULL;
    pdfi_push(ctx, (pdf_obj *)stream_obj);
    pdfi_countdown(stream_obj); /* get rid of extra ref */

    stream_obj->stream_dict->indirect_num = stream_obj->stream_dict->object_num = objnum;
    stream_obj->stream_dict->indirect_gen = stream_obj->stream_dict->generation_num = gen;
    stream_obj->stream_offset = offset;

    /* This code may be a performance overhead, it simply skips over the stream contents
     * and checks that the stream ends with a 'endstream endobj' pair. We could add a
     * 'go faster' flag for users who are certain their PDF files are well-formed. This
     * could also allow us to skip all kinds of other checking.....
     */

    code = pdfi_dict_get_int(ctx, (pdf_dict *)stream_obj->stream_dict, "Length", &i);
    if (code < 0) {
        dmprintf1(ctx->memory, "Stream object %u missing mandatory keyword /Length, unable to verify the stream length.\n", objnum);
        ctx->pdf_errors |= E_PDF_BADSTREAM;
        return 0;
    }

    /* Cache the Length in the stream object and mark it valid */
    stream_obj->Length = i;
    stream_obj->length_valid = true;

    if (i < 0 || (i + offset)> ctx->main_stream_length) {
        dmprintf1(ctx->memory, "Stream object %u has /Length which, when added to offset of object, exceeds file size.\n", objnum);
        ctx->pdf_errors |= E_PDF_BADSTREAM;
        return 0;
    }
    code = pdfi_seek(ctx, ctx->main_stream, i, SEEK_CUR);
    if (code < 0) {
        pdfi_pop(ctx, 1);
        return code;
    }

    code = pdfi_read_token(ctx, ctx->main_stream, objnum, gen);
    if (pdfi_count_stack(ctx) < 2) {
        dmprintf1(ctx->memory, "Failed to find a valid object at end of stream object %u.\n", objnum);
        return 0;
    }

    if (((pdf_obj *)ctx->stack_top[-1])->type != PDF_KEYWORD) {
        dmprintf1(ctx->memory, "Failed to find 'endstream' keyword at end of stream object %u.\n", objnum);
        ctx->pdf_errors |= E_PDF_MISSINGENDOBJ;
        pdfi_pop(ctx, 1);
        return 0;
    }
    keyword = ((pdf_keyword *)ctx->stack_top[-1]);
    if (keyword->key != TOKEN_ENDSTREAM) {
        dmprintf2(ctx->memory, "Stream object %u has an incorrect /Length of %"PRIu64"\n", objnum, i);
        pdfi_pop(ctx, 1);
        return 0;
    }
    pdfi_pop(ctx, 1);

    code = pdfi_read_token(ctx, ctx->main_stream, objnum, gen);
    if (code < 0) {
        if (ctx->args.pdfstoponerror)
            return code;
        else
            /* Something went wrong looking for endobj, but we found endstream, so assume
             * for now that will suffice.
             */
            ctx->pdf_errors |= E_PDF_MISSINGENDOBJ;
        return 0;
    }

    if (pdfi_count_stack(ctx) < 2)
        return_error(gs_error_stackunderflow);

    if (((pdf_obj *)ctx->stack_top[-1])->type != PDF_KEYWORD) {
        pdfi_pop(ctx, 1);
        if (ctx->args.pdfstoponerror)
            return_error(gs_error_typecheck);
        ctx->pdf_errors |= E_PDF_MISSINGENDOBJ;
        /* Didn't find an endobj, but we have an endstream, so assume
         * for now that will suffice
         */
        return 0;
    }
    keyword = ((pdf_keyword *)ctx->stack_top[-1]);
    if (keyword->key != TOKEN_ENDOBJ) {
        pdfi_pop(ctx, 2);
        return_error(gs_error_typecheck);
    }
    pdfi_pop(ctx, 1);
    return 0;
}

/* This reads an object *after* the x y obj keyword has been found. Its broken out
 * separately for the benefit of the repair code when reading the dictionary following
 * the 'trailer' keyword, which does not have a 'obj' keyword. Note that it also does
 * not have an 'endobj', we rely on the error handling to take care of that for us.
 */
int pdfi_read_bare_object(pdf_context *ctx, pdf_c_stream *s, gs_offset_t stream_offset, uint32_t objnum, uint32_t gen)
{
    int code = 0;
    pdf_keyword *keyword = NULL;
    gs_offset_t saved_offset[3];

    saved_offset[0] = saved_offset[1] = saved_offset[2] = 0;

    code = pdfi_read_token(ctx, s, objnum, gen);
    if (code < 0)
        return code;

    do {
        /* move all the saved offsets up by one */
        saved_offset[0] = saved_offset[1];
        saved_offset[1] = saved_offset[2];
        saved_offset[2] = pdfi_unread_tell(ctx);;

        code = pdfi_read_token(ctx, s, objnum, gen);
        if (code < 0) {
            pdfi_clearstack(ctx);
            return code;
        }
        if (s->eof)
            return_error(gs_error_syntaxerror);
    }while (ctx->stack_top[-1]->type != PDF_KEYWORD);

    keyword = ((pdf_keyword *)ctx->stack_top[-1]);
    if (keyword->key == TOKEN_ENDOBJ) {
        pdf_obj *o;

        if (pdfi_count_stack(ctx) < 2) {
            pdfi_clearstack(ctx);
            return_error(gs_error_stackunderflow);
        }

        o = ctx->stack_top[-2];

        pdfi_pop(ctx, 1);

        o->indirect_num = o->object_num = objnum;
        o->indirect_gen = o->generation_num = gen;
        return code;
    }
    if (keyword->key == TOKEN_STREAM) {
        pdfi_pop(ctx, 1);
        return pdfi_read_stream_object(ctx, s, stream_offset, objnum, gen);
    }
    if (keyword->key == TOKEN_OBJ) {
        pdf_obj *o;

        ctx->pdf_errors |= E_PDF_MISSINGENDOBJ;

        /* 4 for; the object we want, the object number, generation number and 'obj' keyword */
        if (pdfi_count_stack(ctx) < 4)
            return_error(gs_error_stackunderflow);

        /* If we have that many objects, assume that we can throw away the x y obj and just use the remaining object */
        o = ctx->stack_top[-4];

        pdfi_pop(ctx, 3);

        o->indirect_num = o->object_num = objnum;
        o->indirect_gen = o->generation_num = gen;
        if (saved_offset[0] > 0)
            (void)pdfi_seek(ctx, s, saved_offset[0], SEEK_SET);
        return 0;
    }

    /* Assume that any other keyword means a missing 'endobj' */
    if (!ctx->args.pdfstoponerror) {
        pdf_obj *o;

        ctx->pdf_errors |= E_PDF_MISSINGENDOBJ;

        if (pdfi_count_stack(ctx) < 2)
            return_error(gs_error_stackunderflow);

        o = ctx->stack_top[-2];

        pdfi_pop(ctx, 1);

        o->indirect_num = o->object_num = objnum;
        o->indirect_gen = o->generation_num = gen;
        return code;
    }
    pdfi_pop(ctx, 2);
    return_error(gs_error_syntaxerror);
}

static int pdfi_read_object(pdf_context *ctx, pdf_c_stream *s, gs_offset_t stream_offset)
{
    int code = 0, stack_size = pdfi_count_stack(ctx);
    uint64_t objnum = 0, gen = 0;
    pdf_keyword *keyword = NULL;

    /* An object consists of 'num gen obj' followed by a token, follwed by an endobj
     * A stream dictionary might have a 'stream' instead of an 'endobj', in which case we
     * want to deal with it specially by getting the Length, jumping to the end and checking
     * for an endobj. Or not, possibly, because it would be slow.
     */
    code = pdfi_read_token(ctx, s, 0, 0);
    if (code < 0)
        return code;
    if (stack_size >= pdfi_count_stack(ctx))
        return gs_note_error(gs_error_ioerror);
    if (((pdf_obj *)ctx->stack_top[-1])->type != PDF_INT) {
        pdfi_pop(ctx, 1);
        return_error(gs_error_typecheck);
    }
    objnum = ((pdf_num *)ctx->stack_top[-1])->value.i;
    pdfi_pop(ctx, 1);

    code = pdfi_read_token(ctx, s, 0, 0);
    if (code < 0)
        return code;
    if (stack_size >= pdfi_count_stack(ctx))
        return gs_note_error(gs_error_ioerror);
    if (((pdf_obj *)ctx->stack_top[-1])->type != PDF_INT) {
        pdfi_pop(ctx, 1);
        return_error(gs_error_typecheck);
    }
    gen = ((pdf_num *)ctx->stack_top[-1])->value.i;
    pdfi_pop(ctx, 1);

    code = pdfi_read_token(ctx, s, 0, 0);
    if (code < 0)
        return code;
    if (stack_size >= pdfi_count_stack(ctx))
        return gs_note_error(gs_error_ioerror);
    if (((pdf_obj *)ctx->stack_top[-1])->type != PDF_KEYWORD) {
        pdfi_pop(ctx, 1);
        return_error(gs_error_typecheck);
    }
    keyword = ((pdf_keyword *)ctx->stack_top[-1]);
    if (keyword->key != TOKEN_OBJ) {
        pdfi_pop(ctx, 1);
        return_error(gs_error_syntaxerror);
    }
    pdfi_pop(ctx, 1);

    return pdfi_read_bare_object(ctx, s, stream_offset, objnum, gen);
}

static int pdfi_deref_compressed(pdf_context *ctx, uint64_t obj, uint64_t gen, pdf_obj **object,
                                 const xref_entry *entry)
{
    int code = 0;
    xref_entry *compressed_entry = &ctx->xref_table->xref[entry->u.compressed.compressed_stream_num];
    pdf_c_stream *compressed_stream = NULL;
    pdf_c_stream *SubFile_stream = NULL;
    pdf_c_stream *Object_stream = NULL;
    char Buffer[256];
    int i = 0, object_length = 0;
    int64_t num_entries, found_object;
    int64_t Length;
    gs_offset_t offset = 0;
    pdf_stream *compressed_object = NULL;
    pdf_dict *compressed_sdict = NULL; /* alias */
    pdf_name *Type = NULL;
    pdf_obj *temp_obj;

    if (ctx->args.pdfdebug) {
        dmprintf1(ctx->memory, "%% Reading compressed object (%"PRIi64" 0 obj)", obj);
        dmprintf1(ctx->memory, " from ObjStm with object number %"PRIi64"\n", compressed_entry->object_num);
    }

    if (compressed_entry->cache == NULL) {
#if CACHE_STATISTICS
        ctx->compressed_misses++;
#endif
        code = pdfi_seek(ctx, ctx->main_stream, compressed_entry->u.uncompressed.offset, SEEK_SET);
        if (code < 0)
            goto exit;

        code = pdfi_read_object(ctx, ctx->main_stream, 0);
        if (code < 0)
            goto exit;

        if ((ctx->stack_top[-1])->type != PDF_STREAM) {
            pdfi_pop(ctx, 1);
            code = gs_note_error(gs_error_typecheck);
            goto exit;
        }
        if (ctx->stack_top[-1]->object_num != compressed_entry->object_num) {
            pdfi_pop(ctx, 1);
            /* Same error (undefined) as when we read an uncompressed object with the wrong number */
            code = gs_note_error(gs_error_undefined);
            goto exit;
        }
        compressed_object = (pdf_stream *)ctx->stack_top[-1];
        pdfi_countup(compressed_object);
        pdfi_pop(ctx, 1);
        code = pdfi_add_to_cache(ctx, (pdf_obj *)compressed_object);
        if (code < 0)
            goto exit;
    } else {
#if CACHE_STATISTICS
        ctx->compressed_hits++;
#endif
        compressed_object = (pdf_stream *)compressed_entry->cache->o;
        pdfi_countup(compressed_object);
        pdfi_promote_cache_entry(ctx, compressed_entry->cache);
    }
    code = pdfi_dict_from_obj(ctx, (pdf_obj *)compressed_object, &compressed_sdict);
    if (code < 0)
        return code;

    /* Check its an ObjStm ! */
    code = pdfi_dict_get_type(ctx, compressed_sdict, "Type", PDF_NAME, (pdf_obj **)&Type);
    if (code < 0)
        goto exit;

    if (!pdfi_name_is(Type, "ObjStm")){
        code = gs_note_error(gs_error_syntaxerror);
        goto exit;
    }

    /* Need to check the /N entry to see if the object is actually in this stream! */
    code = pdfi_dict_get_int(ctx, compressed_sdict, "N", &num_entries);
    if (code < 0)
        goto exit;

    if (num_entries < 0 || num_entries > ctx->xref_table->xref_size) {
        code = gs_note_error(gs_error_rangecheck);
        goto exit;
    }

    code = pdfi_seek(ctx, ctx->main_stream, pdfi_stream_offset(ctx, compressed_object), SEEK_SET);
    if (code < 0)
        goto exit;

    code = pdfi_dict_get_int(ctx, compressed_sdict, "Length", &Length);
    if (code < 0)
        goto exit;

    code = pdfi_apply_SubFileDecode_filter(ctx, Length, NULL, ctx->main_stream, &SubFile_stream, false);
    if (code < 0)
        goto exit;

    code = pdfi_filter(ctx, compressed_object, SubFile_stream, &compressed_stream, false);
    if (code < 0)
        goto exit;

    for (i=0;i < num_entries;i++)
        {
            code = pdfi_read_token(ctx, compressed_stream, obj, gen);
            if (code < 0)
                goto exit;
            temp_obj = ctx->stack_top[-1];
            if (temp_obj->type != PDF_INT) {
                pdfi_pop(ctx, 1);
                goto exit;
            }
            found_object = ((pdf_num *)temp_obj)->value.i;
            pdfi_pop(ctx, 1);
            code = pdfi_read_token(ctx, compressed_stream, obj, gen);
            if (code < 0)
                goto exit;
            temp_obj = ctx->stack_top[-1];
            if (temp_obj->type != PDF_INT) {
                pdfi_pop(ctx, 1);
                goto exit;
            }
            if (i == entry->u.compressed.object_index) {
                if (found_object != obj) {
                    pdfi_pop(ctx, 1);
                    code = gs_note_error(gs_error_undefined);
                    goto exit;
                }
                offset = ((pdf_num *)temp_obj)->value.i;
            }
            if (i == entry->u.compressed.object_index + 1)
                object_length = ((pdf_num *)temp_obj)->value.i - offset;
            pdfi_pop(ctx, 1);
        }

    /* Skip to the offset of the object we want to read */
    for (i=0;i < offset;i++)
        {
            code = pdfi_read_bytes(ctx, (byte *)&Buffer[0], 1, 1, compressed_stream);
            if (code <= 0) {
                code = gs_note_error(gs_error_ioerror);
                goto exit;
            }
        }

    /* If object_length is not 0, then we want to apply a SubFileDecode filter to limit
     * the number of bytes we read to the declared size of the object (difference between
     * the offsets of the object we want to read, and the next object). If it is 0 then
     * we're reading the last object in the stream, so we just rely on the SubFileDecode
     * we set up when we created compressed_stream to limit the bytes to the length of
     * that stream.
     */
    if (object_length > 0) {
        code = pdfi_apply_SubFileDecode_filter(ctx, object_length, NULL, compressed_stream, &Object_stream, false);
        if (code < 0)
            goto exit;
    } else {
        Object_stream = compressed_stream;
    }

    code = pdfi_read_token(ctx, Object_stream, obj, gen);
    if (code < 0)
        goto exit;
    if (ctx->stack_top[-1]->type == PDF_ARRAY_MARK || ctx->stack_top[-1]->type == PDF_DICT_MARK) {
        int start_depth = pdfi_count_stack(ctx);

        /* Need to read all the elements from COS objects */
        do {
            code = pdfi_read_token(ctx, Object_stream, obj, gen);
            if (code < 0)
                goto exit;
            if (compressed_stream->eof == true) {
                code = gs_note_error(gs_error_ioerror);
                goto exit;
            }
        }while ((ctx->stack_top[-1]->type != PDF_ARRAY && ctx->stack_top[-1]->type != PDF_DICT) || pdfi_count_stack(ctx) > start_depth);
    }

    *object = ctx->stack_top[-1];
    /* For compressed objects we don't get a 'obj gen obj' sequence which is what sets
     * the object number for uncompressed objects. So we need to do that here.
     */
    (*object)->indirect_num = (*object)->object_num = obj;
    (*object)->indirect_gen = (*object)->generation_num = gen;
    pdfi_countup(*object);
    pdfi_pop(ctx, 1);

    code = pdfi_add_to_cache(ctx, *object);
    if (code < 0) {
        pdfi_countdown(*object);
        goto exit;
    }

 exit:
    if (Object_stream)
        pdfi_close_file(ctx, Object_stream);
    if (Object_stream != compressed_stream)
        if (compressed_stream)
            pdfi_close_file(ctx, compressed_stream);
    if (SubFile_stream)
        pdfi_close_file(ctx, SubFile_stream);
    pdfi_countdown(compressed_object);
    pdfi_countdown(Type);
    return code;
}

/* pdf_dereference returns an object with a reference count of at least 1, this represents the
 * reference being held by the caller (in **object) when we return from this function.
 */
int pdfi_dereference(pdf_context *ctx, uint64_t obj, uint64_t gen, pdf_obj **object)
{
    xref_entry *entry;
    int code;
    gs_offset_t saved_stream_offset;
    bool saved_decrypt_strings = ctx->encryption.decrypt_strings;

    *object = NULL;

    if (ctx->xref_table == NULL)
        return_error(gs_error_typecheck);

    if (obj >= ctx->xref_table->xref_size) {
        dmprintf1(ctx->memory, "Error, attempted to dereference object %"PRIu64", which is not present in the xref table\n", obj);
        ctx->pdf_errors |= E_PDF_BADOBJNUMBER;

        if(ctx->args.pdfstoponerror)
            return_error(gs_error_rangecheck);

        code = pdfi_object_alloc(ctx, PDF_NULL, 0, object);
        if (code == 0)
            pdfi_countup(*object);
        return code;
    }

    entry = &ctx->xref_table->xref[obj];

    if(entry->object_num == 0)
        return_error(gs_error_undefined);

    if (entry->free) {
        dmprintf1(ctx->memory, "Attempt to dereference free object %"PRIu64", trying next object number as offset.\n", entry->object_num);
        ctx->pdf_errors |= E_PDF_DEREF_FREE_OBJ;
    }

    if (ctx->loop_detection) {
        if (pdfi_loop_detector_check_object(ctx, obj) == true)
            return_error(gs_error_circular_reference);
    }
    if (entry->cache != NULL){
        pdf_obj_cache_entry *cache_entry = entry->cache;

#if CACHE_STATISTICS
        ctx->hits++;
#endif
        *object = cache_entry->o;
        pdfi_countup(*object);

        pdfi_promote_cache_entry(ctx, cache_entry);
    } else {
        saved_stream_offset = pdfi_unread_tell(ctx);

        if (entry->compressed) {
            /* This is an object in a compressed object stream */
            ctx->encryption.decrypt_strings = false;

            code = pdfi_deref_compressed(ctx, obj, gen, object, entry);
            if (code < 0)
                goto error;
        } else {
            pdf_c_stream *SubFile_stream = NULL;
            pdf_string *EODString;
#if CACHE_STATISTICS
            ctx->misses++;
#endif
            code = pdfi_seek(ctx, ctx->main_stream, entry->u.uncompressed.offset, SEEK_SET);
            if (code < 0)
                goto error;

            code = pdfi_name_alloc(ctx, (byte *)"trailer", 6, (pdf_obj **)&EODString);
            if (code < 0)
                goto error;
            pdfi_countup(EODString);

            code = pdfi_apply_SubFileDecode_filter(ctx, 0, EODString, ctx->main_stream, &SubFile_stream, false);
            if (code < 0) {
                pdfi_countdown(EODString);
                goto error;
            }

            code = pdfi_read_object(ctx, SubFile_stream, entry->u.uncompressed.offset);

            pdfi_countdown(EODString);
            pdfi_close_file(ctx, SubFile_stream);
            if (code < 0) {
                if (entry->free) {
                    dmprintf2(ctx->memory, "Dereference of free object %"PRIu64", next object number as offset failed (code = %d), returning NULL object.\n", entry->object_num, code);
                    code = pdfi_object_alloc(ctx, PDF_NULL, 1, object);
                    if (code >= 0) {
                        pdfi_countup(*object);
                        goto free_obj;
                    }
                }
                goto error;
            }

            if ((ctx->stack_top[-1])->object_num == obj) {
                *object = ctx->stack_top[-1];
                pdfi_countup(*object);
                pdfi_pop(ctx, 1);
                code = pdfi_add_to_cache(ctx, *object);
                if (code < 0) {
                    pdfi_countdown(*object);
                    goto error;
                }
            } else {
                pdfi_pop(ctx, 1);
                if (entry->free) {
                    dmprintf1(ctx->memory, "Dereference of free object %"PRIu64", next object number as offset failed, returning NULL object.\n", entry->object_num);
                    code = pdfi_object_alloc(ctx, PDF_NULL, 1, object);
                    if (code >= 0)
                        pdfi_countup(*object);
                    return code;
                }
                code = gs_note_error(gs_error_undefined);
                goto error;
            }
        }
free_obj:
        (void)pdfi_seek(ctx, ctx->main_stream, saved_stream_offset, SEEK_SET);
    }

    if (ctx->loop_detection) {
        code = pdfi_loop_detector_add_object(ctx, (*object)->object_num);
        if (code < 0) {
            ctx->encryption.decrypt_strings = saved_decrypt_strings;
            return code;
        }
    }
    ctx->encryption.decrypt_strings = saved_decrypt_strings;
    return 0;

error:
    ctx->encryption.decrypt_strings = saved_decrypt_strings;
    (void)pdfi_seek(ctx, ctx->main_stream, saved_stream_offset, SEEK_SET);
    pdfi_clearstack(ctx);
    return code;
}

/* do a derefence with loop detection */
int pdfi_deref_loop_detect(pdf_context *ctx, uint64_t obj, uint64_t gen, pdf_obj **object)
{
    int code;

    code = pdfi_loop_detector_mark(ctx);
    if (code < 0)
        return code;

    code = pdfi_dereference(ctx, obj, gen, object);
    (void)pdfi_loop_detector_cleartomark(ctx);
    return code;
}


static int pdfi_resolve_indirect_array(pdf_context *ctx, pdf_obj *obj, bool recurse)
{
    int code = 0;
    uint64_t index, arraysize;
    pdf_obj *object = NULL;
    pdf_array *array = (pdf_array *)obj;

    arraysize = pdfi_array_size(array);
    for (index = 0; index < arraysize; index++) {
        code = pdfi_array_get_no_store_R(ctx, array, index, &object);
        if (code == gs_error_circular_reference) {
            /* Just leave as an indirect ref */
            code = 0;
        } else {
            if (code < 0) goto exit;
            /* don't store the object if it's a stream (leave as a ref) */
            if (object->type != PDF_STREAM)
                code = pdfi_array_put(ctx, array, index, object);
            if (recurse)
                code = pdfi_resolve_indirect(ctx, object, recurse);
        }
        if (code < 0) goto exit;

        pdfi_countdown(object);
        object = NULL;
    }

 exit:
    pdfi_countdown(object);
    return code;
}

static int pdfi_resolve_indirect_dict(pdf_context *ctx, pdf_obj *obj, bool recurse)
{
    int code = 0;
    pdf_dict *dict = (pdf_dict *)obj;
    pdf_name *Key = NULL;
    pdf_obj *Value = NULL;
    uint64_t index, dictsize;

    dictsize = pdfi_dict_entries(dict);

    /* Note: I am not using pdfi_dict_first/next because of needing to handle
     * circular references.
     */
    for (index=0; index<dictsize; index ++) {
        Key = (pdf_name *)dict->keys[index];
        code = pdfi_dict_get_no_store_R_key(ctx, dict, Key, &Value);
        if (code == gs_error_circular_reference) {
            /* Just leave as an indirect ref */
            code = 0;
        } else {
            if (code < 0) goto exit;
            /* don't store the object if it's a stream (leave as a ref) */
            if (Value->type != PDF_STREAM)
                pdfi_dict_put_obj(ctx, dict, (pdf_obj *)Key, Value);
            if (recurse)
                code = pdfi_resolve_indirect(ctx, Value, recurse);
        }
        if (code < 0) goto exit;

        pdfi_countdown(Value);
        Value = NULL;
    }

 exit:
    pdfi_countdown(Value);
    return code;
}

/* Resolve all the indirect references for an object
 * Note: This can be recursive
 */
int pdfi_resolve_indirect(pdf_context *ctx, pdf_obj *value, bool recurse)
{
    int code = 0;

    switch(value->type) {
    case PDF_ARRAY:
        code = pdfi_resolve_indirect_array(ctx, value, recurse);
        break;
    case PDF_DICT:
        code = pdfi_resolve_indirect_dict(ctx, value, recurse);
        break;
    default:
        break;
    }
    return code;
}

/* Resolve all the indirect references for an object
 * Resolve indirect references, either one level or recursively, with loop detect on
 * the parent (can by NULL) and the value.
 */
int pdfi_resolve_indirect_loop_detect(pdf_context *ctx, pdf_obj *parent, pdf_obj *value, bool recurse)
{
    int code = 0;

    code = pdfi_loop_detector_mark(ctx);
    if (code < 0) goto exit;
    if (parent && parent->object_num != 0) {
        code = pdfi_loop_detector_add_object(ctx, parent->object_num);
        if (code < 0) goto exit;
    }
    if (value->object_num != 0) {
        code = pdfi_loop_detector_add_object(ctx, value->object_num);
        if (code < 0) goto exit;
    }
    code = pdfi_resolve_indirect(ctx, value, false);
    if (code < 0) goto exit;
    (void)pdfi_loop_detector_cleartomark(ctx); /* Clear to the mark for the current loop */

 exit:
    return code;
}
