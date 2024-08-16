/* Copyright (C) 2020-2024 Artifex Software, Inc.
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
#include "pdf_repair.h"

/* Start with the object caching functions */
/* Disable object caching (for easier debugging with reference counting)
 * by uncommenting the following line
 */
/*#define DISABLE CACHE*/

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
#ifndef DISABLE_CACHE
    pdf_obj_cache_entry *entry;

    if (o < PDF_TOKEN_AS_OBJ(TOKEN__LAST_KEY))
        return 0;

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
#endif
    return 0;
}

/* Given an existing cache entry, promote it to be the most-recently-used
 * cache entry.
 */
static void pdfi_promote_cache_entry(pdf_context *ctx, pdf_obj_cache_entry *cache_entry)
{
#ifndef DISABLE_CACHE
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
#endif
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
#ifndef DISABLE_CACHE
    xref_entry *entry;
    pdf_obj_cache_entry *cache_entry;
    pdf_obj *old_cached_obj = NULL;

    /* Limited error checking here, we assume that things like the
     * validity of the object (eg not a free oobject) have already been handled.
     */

    entry = &ctx->xref_table->xref[o->object_num];
    cache_entry = entry->cache;

    if (cache_entry == NULL) {
        return(pdfi_add_to_cache(ctx, o));
    } else {
        /* NOTE: We grab the object without decrementing, to avoid triggering
         * a warning message for freeing an object that's in the cache
         */
        if (cache_entry->o != NULL)
            old_cached_obj = cache_entry->o;

        /* Put new entry in the cache */
        cache_entry->o = o;
        pdfi_countup(o);
        pdfi_promote_cache_entry(ctx, cache_entry);

        /* Now decrement the old cache entry, if any */
        pdfi_countdown(old_cached_obj);
    }
#endif
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
    pdf_dict *dict = NULL;
    gs_offset_t offset;
    pdf_stream *stream_obj = NULL;

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
    if (s != ctx->main_stream) {
        offset = stell(s->s) - s->unread_size + stream_offset;
        code = pdfi_seek(ctx, ctx->main_stream, offset, SEEK_SET);
        if (code < 0)
            return_error(gs_error_ioerror);
    } else {
        offset = stell(s->s) - s->unread_size;
    }

    if (pdfi_count_stack(ctx) < 1)
        return_error(gs_error_stackunderflow);

    dict = (pdf_dict *)ctx->stack_top[-1];

    if (pdfi_type_of(dict) != PDF_DICT) {
        pdfi_pop(ctx, 1);
        return_error(gs_error_syntaxerror);
    }

    dict->indirect_num = dict->object_num = objnum;
    dict->indirect_gen = dict->generation_num = gen;

    /* Convert the dict into a stream */
    code = pdfi_obj_dict_to_stream(ctx, dict, &stream_obj, true);
    if (code < 0) {
        pdfi_pop(ctx, 1);
        return code;
    }
    /* Pop off the dict and push the stream */
    pdfi_pop(ctx, 1);
    dict = NULL;
    pdfi_push(ctx, (pdf_obj *)stream_obj);

    stream_obj->stream_dict->indirect_num = stream_obj->stream_dict->object_num = objnum;
    stream_obj->stream_dict->indirect_gen = stream_obj->stream_dict->generation_num = gen;
    stream_obj->stream_offset = offset;

    /* Exceptional code. Normally we do not need to worry about detecting circular references
     * when reading objects, because we do not dereference any indirect objects. However streams
     * are a slight exception in that we do get the Length from the stream dictionay and if that
     * is an indirect reference, then we dereference it.
     * OSS-fuzz bug 43247 has a stream where the value associated iwht the /Length is an indirect
     * reference to the same stream object, and leads to infinite recursion. So deal with that
     * possibility here.
     */
    code = pdfi_loop_detector_mark(ctx);
    if (code < 0) {
        pdfi_countdown(stream_obj); /* get rid of extra ref */
        return code;
    }
    if (pdfi_loop_detector_check_object(ctx, stream_obj->object_num)) {
        pdfi_countdown(stream_obj); /* get rid of extra ref */
        pdfi_loop_detector_cleartomark(ctx);
        return_error(gs_error_circular_reference);
    }

    code = pdfi_loop_detector_add_object(ctx, stream_obj->object_num);
    if (code < 0) {
        pdfi_countdown(stream_obj); /* get rid of extra ref */
        pdfi_loop_detector_cleartomark(ctx);
        return code;
    }

    /* This code may be a performance overhead, it simply skips over the stream contents
     * and checks that the stream ends with a 'endstream endobj' pair. We could add a
     * 'go faster' flag for users who are certain their PDF files are well-formed. This
     * could also allow us to skip all kinds of other checking.....
     */

    code = pdfi_dict_get_int(ctx, (pdf_dict *)stream_obj->stream_dict, "Length", &i);
    if (code < 0) {
        char extra_info[gp_file_name_sizeof];

        (void)pdfi_loop_detector_cleartomark(ctx);
        gs_snprintf(extra_info, sizeof(extra_info), "Stream object %u missing mandatory keyword /Length, unable to verify the stream length.\n", objnum);
        code = pdfi_set_error_stop(ctx, gs_note_error(gs_error_ioerror), NULL, E_PDF_BADSTREAM, "pdfi_read_stream_object", extra_info);
        pdfi_countdown(stream_obj); /* get rid of extra ref */
        return code;
    }
    code = pdfi_loop_detector_cleartomark(ctx);
    if (code < 0) {
        pdfi_countdown(stream_obj); /* get rid of extra ref */
        return code;
    }

    if (i < 0 || (i + offset)> ctx->main_stream_length) {
        char extra_info[gp_file_name_sizeof];

        gs_snprintf(extra_info, sizeof(extra_info), "Stream object %u has /Length which, when added to offset of object, exceeds file size.\n", objnum);
        if ((code = pdfi_set_error_stop(ctx, gs_note_error(gs_error_ioerror), NULL, E_PDF_BADSTREAM, "pdfi_read_stream_object", extra_info))< 0) {
            pdfi_pop(ctx, 1);
            pdfi_countdown(stream_obj); /* get rid of extra ref */
            return code;
        }
    } else {
        code = pdfi_seek(ctx, ctx->main_stream, i, SEEK_CUR);
        if (code < 0) {
            pdfi_pop(ctx, 1);
            pdfi_countdown(stream_obj); /* get rid of extra ref */
            return code;
        }

        stream_obj->Length = 0;
        stream_obj->length_valid = false;

        code = pdfi_read_bare_keyword(ctx, ctx->main_stream);
        if (code == 0) {
            char extra_info[gp_file_name_sizeof];

            gs_snprintf(extra_info, sizeof(extra_info), "Failed to find a valid object at end of stream object %u.\n", objnum);
            pdfi_log_info(ctx, "pdfi_read_stream_object", extra_info);
            /* It is possible for pdfi_read_token to clear the stack, losing the stream object. If that
             * happens give up.
             */
            if (pdfi_count_stack(ctx) == 0) {
                pdfi_countdown(stream_obj); /* get rid of extra ref */
                return code;
            }
        } else if (code < 0) {
            char extra_info[gp_file_name_sizeof];

            gs_snprintf(extra_info, sizeof(extra_info), "Failed to find 'endstream' keyword at end of stream object %u.\n", objnum);
            if ((code = pdfi_set_error_stop(ctx, gs_note_error(gs_error_syntaxerror), NULL, E_PDF_MISSINGENDOBJ, "pdfi_read_stream_object", extra_info)) < 0) {
                pdfi_countdown(stream_obj); /* get rid of extra ref */
                return code;
            }
        } else if (code != TOKEN_ENDSTREAM) {
            char extra_info[gp_file_name_sizeof];

            gs_snprintf(extra_info, sizeof(extra_info), "Stream object %u has an incorrect /Length of %"PRIu64"\n", objnum, i);
            if ((code = pdfi_set_error_stop(ctx, gs_note_error(gs_error_syntaxerror), NULL, E_PDF_BAD_LENGTH, "pdfi_read_stream_object", extra_info)) < 0) {
                pdfi_countdown(stream_obj); /* get rid of extra ref */
                return code;
            }
        } else {
            /* Cache the Length in the stream object and mark it valid */
            stream_obj->Length = i;
            stream_obj->length_valid = true;
        }
    }

    /* If we failed to find a valid object, or the object wasn't a keyword, or the
     * keywrod wasn't 'endstream' then the Length is wrong. We need to have the correct
     * Length for streams if we have encrypted files, because we must install a
     * SubFileDecode filter with a Length (EODString is incompatible with AES encryption)
     * Rather than mess about checking for encryption, we'll choose to just correctly
     * calculate the Length of all streams. Although this takes time, it will only
     * happen for files which are invalid.
     */
    if (stream_obj->length_valid != true) {
        char Buffer[10];
        unsigned int bytes, total = 0;
        int c = 0;

        code = pdfi_seek(ctx, ctx->main_stream, stream_obj->stream_offset, SEEK_SET);
        if (code < 0) {
            pdfi_countdown(stream_obj); /* get rid of extra ref */
            pdfi_pop(ctx, 1);
            return code;
        }
        memset(Buffer, 0x00, 10);
        bytes = pdfi_read_bytes(ctx, (byte *)Buffer, 1, 9, ctx->main_stream);
        if (bytes < 9) {
            pdfi_countdown(stream_obj); /* get rid of extra ref */
            return_error(gs_error_ioerror);
        }

        total = bytes;
        do {
            if (memcmp(Buffer, "endstream", 9) == 0) {
                if (Buffer[9] != 0x00)
                    total--;
                stream_obj->Length = total - 9;
                stream_obj->length_valid = true;
                break;
            }
            if (memcmp(Buffer, "endobj", 6) == 0) {
                if (Buffer[9] != 0x00)
                    total--;
                stream_obj->Length = total - 6;
                stream_obj->length_valid = true;
                break;
            }
            memmove(Buffer, Buffer+1, 9);
            c = pdfi_read_byte(ctx, ctx->main_stream);
            if (c < 0)
                break;
            Buffer[9] = (byte)c;
            total++;
        } while(1);
        pdfi_countdown(stream_obj); /* get rid of extra ref */
        if (c < 0)
            return_error(gs_error_ioerror);
        return 0;
    }

    code = pdfi_read_bare_keyword(ctx, ctx->main_stream);
    if (code < 0) {
        pdfi_countdown(stream_obj); /* get rid of extra ref */
        if ((code = pdfi_set_error_stop(ctx, code, NULL, E_PDF_MISSINGENDOBJ, "pdfi_read_stream_object", "")) < 0) {
            return code;
        }
        /* Something went wrong looking for endobj, but we found endstream, so assume
         * for now that will suffice.
         */
        return 0;
    }

    if (code == 0) {
        pdfi_countdown(stream_obj); /* get rid of extra ref */
        return_error(gs_error_stackunderflow);
    }

    if (code != TOKEN_ENDOBJ) {
        pdfi_countdown(stream_obj); /* get rid of extra ref */
        code = pdfi_set_error_stop(ctx, gs_note_error(gs_error_typecheck), NULL, E_PDF_MISSINGENDOBJ, "pdfi_read_stream_object", NULL);
        /* Didn't find an endobj, but we have an endstream, so assume
         * for now that will suffice
         */
        return code;
    }
    pdfi_countdown(stream_obj); /* get rid of extra ref */

    return 0;
}

/* This reads an object *after* the x y obj keyword has been found. Its broken out
 * separately for the benefit of the repair code when reading the dictionary following
 * the 'trailer' keyword, which does not have a 'obj' keyword. Note that it also does
 * not have an 'endobj', we rely on the error handling to take care of that for us.
 */
int pdfi_read_bare_object(pdf_context *ctx, pdf_c_stream *s, gs_offset_t stream_offset, uint32_t objnum, uint32_t gen)
{
    int code = 0, initial_depth = 0;
    pdf_key keyword;
    gs_offset_t saved_offset[3];
    pdf_obj_type type;

    initial_depth = pdfi_count_stack(ctx);
    saved_offset[0] = saved_offset[1] = saved_offset[2] = 0;

    code = pdfi_read_token(ctx, s, objnum, gen);
    if (code < 0)
        return code;

    if (code == 0)
        /* failed to read a token */
        return_error(gs_error_syntaxerror);

    if (pdfi_type_of(ctx->stack_top[-1]) == PDF_FAST_KEYWORD) {
        keyword = (pdf_key)(uintptr_t)(ctx->stack_top[-1]);
        if (keyword == TOKEN_ENDOBJ) {
            ctx->stack_top[-1] = PDF_NULL_OBJ;
            return 0;
        }
    }

    do {
        /* move all the saved offsets up by one */
        saved_offset[0] = saved_offset[1];
        saved_offset[1] = saved_offset[2];
        saved_offset[2] = pdfi_unread_tell(ctx);

        code = pdfi_read_token(ctx, s, objnum, gen);
        if (code < 0) {
            pdfi_clearstack(ctx);
            return code;
        }
        if (s->eof)
            return_error(gs_error_syntaxerror);
        code = 0;
        type = pdfi_type_of(ctx->stack_top[-1]);
        if (type == PDF_KEYWORD)
            goto missing_endobj;
    } while (type != PDF_FAST_KEYWORD);

    keyword = (pdf_key)(uintptr_t)(ctx->stack_top[-1]);
    if (keyword == TOKEN_ENDOBJ) {
        pdf_obj *o;

        if (pdfi_count_stack(ctx) - initial_depth < 2) {
            pdfi_clearstack(ctx);
            return_error(gs_error_stackunderflow);
        }

        o = ctx->stack_top[-2];

        pdfi_pop(ctx, 1);

        if (o >= PDF_TOKEN_AS_OBJ(TOKEN__LAST_KEY)) {
            o->indirect_num = o->object_num = objnum;
            o->indirect_gen = o->generation_num = gen;
        }
        return code;
    }
    if (keyword == TOKEN_STREAM) {
        pdfi_pop(ctx, 1);
        return pdfi_read_stream_object(ctx, s, stream_offset, objnum, gen);
    }
    if (keyword == TOKEN_OBJ) {
        pdf_obj *o;

        if ((code = pdfi_set_error_stop(ctx, gs_note_error(gs_error_syntaxerror), NULL, E_PDF_MISSINGENDOBJ, "pdfi_read_bare_object", NULL)) < 0) {
            return code;
        }

        /* 4 for; the object we want, the object number, generation number and 'obj' keyword */
        if (pdfi_count_stack(ctx) - initial_depth < 4)
            return_error(gs_error_stackunderflow);

        /* If we have that many objects, assume that we can throw away the x y obj and just use the remaining object */
        o = ctx->stack_top[-4];

        pdfi_pop(ctx, 3);

        if (pdfi_type_of(o) != PDF_BOOL && pdfi_type_of(o) != PDF_NULL && pdfi_type_of(o) != PDF_FAST_KEYWORD) {
            o->indirect_num = o->object_num = objnum;
            o->indirect_gen = o->generation_num = gen;
        }
        if (saved_offset[0] > 0)
            (void)pdfi_seek(ctx, s, saved_offset[0], SEEK_SET);
        return 0;
    }

missing_endobj:
    /* Assume that any other keyword means a missing 'endobj' */
    if ((code = pdfi_set_error_stop(ctx, gs_note_error(gs_error_syntaxerror), NULL, E_PDF_MISSINGENDOBJ, "pdfi_read_xref_stream_dict", "")) == 0) {
        pdf_obj *o;

        pdfi_set_error(ctx, 0, NULL, E_PDF_MISSINGENDOBJ, "pdfi_read_bare_object", NULL);

        if (pdfi_count_stack(ctx) - initial_depth < 2)
            return_error(gs_error_stackunderflow);

        o = ctx->stack_top[-2];

        pdfi_pop(ctx, 1);

        if (pdfi_type_of(o) != PDF_BOOL && pdfi_type_of(o) != PDF_NULL && pdfi_type_of(o) != PDF_FAST_KEYWORD) {
            o->indirect_num = o->object_num = objnum;
            o->indirect_gen = o->generation_num = gen;
        }
        return code;
    }
    pdfi_pop(ctx, 2);
    return_error(gs_error_syntaxerror);
}

static int pdfi_read_object(pdf_context *ctx, pdf_c_stream *s, gs_offset_t stream_offset)
{
    int code = 0;
    int objnum = 0, gen = 0;

    /* An object consists of 'num gen obj' followed by a token, follwed by an endobj
     * A stream dictionary might have a 'stream' instead of an 'endobj', in which case we
     * want to deal with it specially by getting the Length, jumping to the end and checking
     * for an endobj. Or not, possibly, because it would be slow.
     */
    code = pdfi_read_bare_int(ctx, s, &objnum);
    if (code < 0)
        return code;
    if (code == 0)
        return_error(gs_error_syntaxerror);

    code = pdfi_read_bare_int(ctx, s, &gen);
    if (code < 0)
        return code;
    if (code == 0)
        return_error(gs_error_syntaxerror);

    code = pdfi_read_bare_keyword(ctx, s);
    if (code < 0)
        return code;
    if (code == 0)
        return gs_note_error(gs_error_ioerror);
    if (code != TOKEN_OBJ) {
        return_error(gs_error_syntaxerror);
    }

    return pdfi_read_bare_object(ctx, s, stream_offset, objnum, gen);
}

static int pdfi_deref_compressed(pdf_context *ctx, uint64_t obj, uint64_t gen, pdf_obj **object,
                                 const xref_entry *entry, bool cache)
{
    int code = 0;
    xref_entry *compressed_entry;
    pdf_c_stream *compressed_stream = NULL;
    pdf_c_stream *SubFile_stream = NULL;
    pdf_c_stream *Object_stream = NULL;
    int i = 0, object_length = 0;
    int64_t num_entries;
    int found_object;
    int64_t Length, First;
    gs_offset_t offset = 0;
    pdf_stream *compressed_object = NULL;
    pdf_dict *compressed_sdict = NULL; /* alias */
    pdf_name *Type = NULL;

    if (entry->u.compressed.compressed_stream_num > ctx->xref_table->xref_size - 1)
        return_error(gs_error_undefined);

    compressed_entry = &ctx->xref_table->xref[entry->u.compressed.compressed_stream_num];

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

        if (pdfi_count_stack(ctx) < 1) {
            code = gs_note_error(gs_error_stackunderflow);
            goto exit;
        }

        if (pdfi_type_of(ctx->stack_top[-1]) != PDF_STREAM) {
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

    if (ctx->loop_detection != NULL) {
        code = pdfi_loop_detector_mark(ctx);
        if (code < 0)
            goto exit;
        if (compressed_sdict->object_num != 0) {
            if (pdfi_loop_detector_check_object(ctx, compressed_sdict->object_num)) {
                code = gs_note_error(gs_error_circular_reference);
            } else {
                code = pdfi_loop_detector_add_object(ctx, compressed_sdict->object_num);
            }
            if (code < 0) {
                (void)pdfi_loop_detector_cleartomark(ctx);
                goto exit;
            }
        }
    }
    /* Check its an ObjStm ! */
    code = pdfi_dict_get_type(ctx, compressed_sdict, "Type", PDF_NAME, (pdf_obj **)&Type);
    if (code < 0) {
        if (ctx->loop_detection != NULL)
            (void)pdfi_loop_detector_cleartomark(ctx);
        goto exit;
    }

    if (!pdfi_name_is(Type, "ObjStm")){
        if (ctx->loop_detection != NULL)
            (void)pdfi_loop_detector_cleartomark(ctx);
        code = gs_note_error(gs_error_syntaxerror);
        goto exit;
    }

    /* Need to check the /N entry to see if the object is actually in this stream! */
    code = pdfi_dict_get_int(ctx, compressed_sdict, "N", &num_entries);
    if (code < 0) {
        if (ctx->loop_detection != NULL)
            (void)pdfi_loop_detector_cleartomark(ctx);
        goto exit;
    }

    if (num_entries < 0 || num_entries > ctx->xref_table->xref_size) {
        if (ctx->loop_detection != NULL)
            (void)pdfi_loop_detector_cleartomark(ctx);
        code = gs_note_error(gs_error_rangecheck);
        goto exit;
    }

    code = pdfi_dict_get_int(ctx, compressed_sdict, "Length", &Length);
    if (code < 0) {
        if (ctx->loop_detection != NULL)
            (void)pdfi_loop_detector_cleartomark(ctx);
        goto exit;
    }

    code = pdfi_dict_get_int(ctx, compressed_sdict, "First", &First);
    if (code < 0) {
        if (ctx->loop_detection != NULL)
            (void)pdfi_loop_detector_cleartomark(ctx);
        goto exit;
    }

    if (ctx->loop_detection != NULL)
        (void)pdfi_loop_detector_cleartomark(ctx);

    code = pdfi_seek(ctx, ctx->main_stream, pdfi_stream_offset(ctx, compressed_object), SEEK_SET);
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
        int new_offset;
        code = pdfi_read_bare_int(ctx, compressed_stream, &found_object);
        if (code < 0)
            goto exit;
        if (code == 0) {
            code = gs_note_error(gs_error_syntaxerror);
            goto exit;
        }
        code = pdfi_read_bare_int(ctx, compressed_stream, &new_offset);
        if (code < 0)
            goto exit;
        if (code == 0) {
            code = gs_note_error(gs_error_syntaxerror);
            goto exit;
        }
        if (i == entry->u.compressed.object_index) {
            if (found_object != obj) {
                code = gs_note_error(gs_error_undefined);
                goto exit;
            }
            offset = new_offset;
        }
        if (i == entry->u.compressed.object_index + 1)
            object_length = new_offset - offset;
    }

    /* Bug #705259 - The first object need not lie immediately after the initial
     * table of object numbers and offsets. The start of the first object is given
     * by the value of First. We don't know how many bytes we consumed getting to
     * the end of the table, unfortunately, so we close the stream, rewind the main
     * stream back to the beginning of the ObjStm, and then read and discard 'First'
     * bytes in order to get to the start of the first object. Then we read the
     * number of bytes required to get from there to the start of the object we
     * actually want.
     * If this ever looks like it's causing performance problems we could read the
     * initial table above manually instead of using the existing code, and track
     * how many bytes we'd read, which would avoid us having to tear down and
     * rebuild the stream.
     */
    if (compressed_stream)
        pdfi_close_file(ctx, compressed_stream);
    if (SubFile_stream)
        pdfi_close_file(ctx, SubFile_stream);

    code = pdfi_seek(ctx, ctx->main_stream, pdfi_stream_offset(ctx, compressed_object), SEEK_SET);
    if (code < 0)
        goto exit;

    /* We already dereferenced this above, so we don't need the loop detection checking here */
    code = pdfi_dict_get_int(ctx, compressed_sdict, "Length", &Length);
    if (code < 0)
        goto exit;

    code = pdfi_apply_SubFileDecode_filter(ctx, Length, NULL, ctx->main_stream, &SubFile_stream, false);
    if (code < 0)
        goto exit;

    code = pdfi_filter(ctx, compressed_object, SubFile_stream, &compressed_stream, false);
    if (code < 0)
        goto exit;

    for (i=0;i < First;i++)
    {
        int c = pdfi_read_byte(ctx, compressed_stream);
        if (c < 0) {
            code = gs_note_error(gs_error_ioerror);
            goto exit;
        }
    }

    /* Skip to the offset of the object we want to read */
    for (i=0;i < offset;i++)
    {
        int c = pdfi_read_byte(ctx, compressed_stream);
        if (c < 0) {
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
    if (code == 0) {
        code = gs_note_error(gs_error_syntaxerror);
        goto exit;
    }
    if (pdfi_type_of(ctx->stack_top[-1]) == PDF_ARRAY_MARK || pdfi_type_of(ctx->stack_top[-1]) == PDF_DICT_MARK) {
        int start_depth = pdfi_count_stack(ctx);

        /* Need to read all the elements from COS objects */
        do {
            code = pdfi_read_token(ctx, Object_stream, obj, gen);
            if (code < 0)
                goto exit;
            if (code == 0) {
                code = gs_note_error(gs_error_syntaxerror);
                goto exit;
            }
            if (compressed_stream->eof == true) {
                code = gs_note_error(gs_error_ioerror);
                goto exit;
            }
        } while ((pdfi_type_of(ctx->stack_top[-1]) != PDF_ARRAY && pdfi_type_of(ctx->stack_top[-1]) != PDF_DICT) || pdfi_count_stack(ctx) > start_depth);
    }

    *object = ctx->stack_top[-1];
    /* For compressed objects we don't get a 'obj gen obj' sequence which is what sets
     * the object number for uncompressed objects. So we need to do that here.
     */
    if (*object >= PDF_TOKEN_AS_OBJ(TOKEN__LAST_KEY)) {
        (*object)->indirect_num = (*object)->object_num = obj;
        (*object)->indirect_gen = (*object)->generation_num = gen;
        pdfi_countup(*object);
    }
    pdfi_pop(ctx, 1);

    if (cache) {
        code = pdfi_add_to_cache(ctx, *object);
        if (code < 0) {
            pdfi_countdown(*object);
            goto exit;
        }
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
static int pdfi_dereference_main(pdf_context *ctx, uint64_t obj, uint64_t gen, pdf_obj **object, bool cache)
{
    xref_entry *entry;
    int code, stack_depth = pdfi_count_stack(ctx);
    gs_offset_t saved_stream_offset;
    bool saved_decrypt_strings = ctx->encryption.decrypt_strings;

    *object = NULL;

    if (ctx->xref_table == NULL)
        return_error(gs_error_typecheck);

    if (ctx->main_stream == NULL || ctx->main_stream->s == NULL)
        return_error(gs_error_ioerror);

    if (obj >= ctx->xref_table->xref_size) {
        char extra_info[gp_file_name_sizeof];

        gs_snprintf(extra_info, sizeof(extra_info), "Error, attempted to dereference object %"PRIu64", which is not present in the xref table\n", obj);
        if ((code = pdfi_set_error_stop(ctx, gs_note_error(gs_error_rangecheck), NULL, E_PDF_BADOBJNUMBER, "pdfi_dereference", extra_info)) < 0) {
            return code;
        }

        code = pdfi_repair_file(ctx);
        if (code < 0) {
            *object = NULL;
            return code;
        }
        if (obj >= ctx->xref_table->xref_size) {
            *object = NULL;
            return_error(gs_error_rangecheck);
        }
    }

    entry = &ctx->xref_table->xref[obj];

    if(entry->object_num == 0) {
        pdfi_set_error(ctx, 0, NULL, E_PDF_BADOBJNUMBER, "pdfi_dereference_main", "Attempt to dereference object 0");
        return_error(gs_error_undefined);
    }

    if (entry->free) {
        char extra_info[gp_file_name_sizeof];

        gs_snprintf(extra_info, sizeof(extra_info), "Attempt to dereference free object %"PRIu64", treating as NULL object.\n", entry->object_num);
        code = pdfi_set_error_stop(ctx, gs_note_error(gs_error_undefined), NULL, E_PDF_DEREF_FREE_OBJ, "pdfi_dereference", extra_info);
        *object = PDF_NULL_OBJ;
        return code;
    }else {
        if (!entry->compressed) {
            if(entry->u.uncompressed.generation_num != gen)
                pdfi_set_warning(ctx, 0, NULL, W_PDF_MISMATCH_GENERATION, "pdfi_dereference_main", "");
        }
    }

    if (ctx->loop_detection) {
        if (pdfi_loop_detector_check_object(ctx, obj) == true)
            return_error(gs_error_circular_reference);
        if (entry->free) {
            code = pdfi_loop_detector_add_object(ctx, obj);
            if (code < 0)
                return code;
        }
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

            code = pdfi_deref_compressed(ctx, obj, gen, object, entry, cache);
            if (code < 0 || *object == NULL)
                goto error;
        } else {
#if CACHE_STATISTICS
            ctx->misses++;
#endif
            ctx->encryption.decrypt_strings = true;

            code = pdfi_seek(ctx, ctx->main_stream, entry->u.uncompressed.offset, SEEK_SET);
            if (code < 0)
                goto error;

            code = pdfi_read_object(ctx, ctx->main_stream, entry->u.uncompressed.offset);

            /* pdfi_read_object() could do a repair, which would invalidate the xref and rebuild it.
             * reload the xref entry to be certain it is valid.
             */
            entry = &ctx->xref_table->xref[obj];
            if (code < 0) {
                int code1 = 0;
                if (entry->free) {
                    dmprintf2(ctx->memory, "Dereference of free object %"PRIu64", next object number as offset failed (code = %d), returning NULL object.\n", entry->object_num, code);
                    *object = PDF_NULL_OBJ;
                    goto free_obj;
                }
                ctx->encryption.decrypt_strings = saved_decrypt_strings;
                (void)pdfi_seek(ctx, ctx->main_stream, saved_stream_offset, SEEK_SET);
                pdfi_pop(ctx, pdfi_count_stack(ctx) - stack_depth);

                code1 = pdfi_repair_file(ctx);
                if (code1 == 0)
                    return pdfi_dereference_main(ctx, obj, gen, object, cache);
                /* Repair failed, just give up and return an error */
                goto error;
            }

            /* We only expect a single object back when dereferencing an indirect reference
             * The only way (I think) we can end up with more than one is if the object initially
             * appears to be a dictionary or array, but the object terminates (with endobj or
             * simply reaching EOF) without terminating the array or dictionary. That's clearly
             * an error. We might, as a future 'improvement' choose to walk back through
             * the stack looking for unterminated dictionary or array markers, and closing them
             * so that (hopefully!) we end up with a single 'repaired' object on the stack.
             * But for now I'm simply going to treat these as errors. We will try a repair on the
             * file to see if we end up using a different (hopefully intact) object from the file.
             */
            if (pdfi_count_stack(ctx) - stack_depth > 1) {
                int code1 = 0;

                code1 = pdfi_repair_file(ctx);
                if (code1 == 0)
                    return pdfi_dereference_main(ctx, obj, gen, object, cache);
                /* Repair failed, just give up and return an error */
                code = gs_note_error(gs_error_syntaxerror);
                goto error;
            }

            if (pdfi_count_stack(ctx) > 0 &&
                ((ctx->stack_top[-1] > PDF_TOKEN_AS_OBJ(TOKEN__LAST_KEY) &&
                (ctx->stack_top[-1])->object_num == obj)
                || ctx->stack_top[-1] == PDF_NULL_OBJ)) {
                *object = ctx->stack_top[-1];
                pdfi_countup(*object);
                pdfi_pop(ctx, 1);
                if (pdfi_type_of(*object) == PDF_INDIRECT) {
                    pdf_indirect_ref *iref = (pdf_indirect_ref *)*object;

                    if (iref->ref_object_num == obj) {
                        code = gs_note_error(gs_error_circular_reference);
                        pdfi_countdown(*object);
                        *object = NULL;
                        goto error;
                    }
                }
                /* There's really no point in caching an indirect reference and
                 * I think it could be potentially confusing to later calls.
                 */
                if (cache && pdfi_type_of(*object) != PDF_INDIRECT) {
                    code = pdfi_add_to_cache(ctx, *object);
                    if (code < 0) {
                        pdfi_countdown(*object);
                        goto error;
                    }
                }
            } else {
                int code1 = 0;

                if (pdfi_count_stack(ctx) > 0)
                    pdfi_pop(ctx, 1);

                if (entry->free) {
                    dmprintf1(ctx->memory, "Dereference of free object %"PRIu64", next object number as offset failed, returning NULL object.\n", entry->object_num);
                    *object = PDF_NULL_OBJ;
                    return 0;
                }
                code1 = pdfi_repair_file(ctx);
                if (code1 == 0)
                    return pdfi_dereference_main(ctx, obj, gen, object, cache);
                /* Repair failed, just give up and return an error */
                code = gs_note_error(gs_error_undefined);
                goto error;
            }
        }
free_obj:
        (void)pdfi_seek(ctx, ctx->main_stream, saved_stream_offset, SEEK_SET);
    }

    if (ctx->loop_detection && pdf_object_num(*object) != 0) {
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
    /* Return the stack to the state at entry */
    pdfi_pop(ctx, pdfi_count_stack(ctx) - stack_depth);
    return code;
}

int pdfi_dereference(pdf_context *ctx, uint64_t obj, uint64_t gen, pdf_obj **object)
{
    return pdfi_dereference_main(ctx, obj, gen, object, true);
}

int pdfi_dereference_nocache(pdf_context *ctx, uint64_t obj, uint64_t gen, pdf_obj **object)
{
    return pdfi_dereference_main(ctx, obj, gen, object, false);
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

int pdfi_deref_loop_detect_nocache(pdf_context *ctx, uint64_t obj, uint64_t gen, pdf_obj **object)
{
    int code;

    code = pdfi_loop_detector_mark(ctx);
    if (code < 0)
        return code;

    code = pdfi_dereference_nocache(ctx, obj, gen, object);
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
        if (ctx->loop_detection != NULL) {
            code = pdfi_loop_detector_mark(ctx);
            if (code < 0)
                return code;
        }

        code = pdfi_array_get_no_store_R(ctx, array, index, &object);

        if (ctx->loop_detection != NULL) {
            int code1 = pdfi_loop_detector_cleartomark(ctx);
            if (code1 < 0)
                return code1;
        }

        if (code == gs_error_circular_reference) {
            /* Previously we just left as an indirect reference, but now we want
             * to return the error so we don't end up replacing indirect references
             * to objects with circular references.
             */
        } else {
            if (code < 0) goto exit;
            if (recurse) {
                code = pdfi_resolve_indirect_loop_detect(ctx, NULL, object, recurse);
                if (code < 0) goto exit;
            }
            /* don't store the object if it's a stream (leave as a ref) */
            if (pdfi_type_of(object) != PDF_STREAM)
                code = pdfi_array_put(ctx, array, index, object);
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
        Key = (pdf_name *)dict->list[index].key;
        if (pdfi_name_is(Key, "Parent"))
            continue;

        if (ctx->loop_detection != NULL) {
            code = pdfi_loop_detector_mark(ctx);
            if (code < 0)
                return code;
        }

        code = pdfi_dict_get_no_store_R_key(ctx, dict, Key, &Value);

        if (ctx->loop_detection != NULL) {
            int code1 = pdfi_loop_detector_cleartomark(ctx);
            if (code1 < 0)
                return code1;
        }

        if (code == gs_error_circular_reference) {
            /* Just leave as an indirect ref */
            code = 0;
        } else {
            if (code < 0) goto exit;
            if (recurse) {
                code = pdfi_resolve_indirect_loop_detect(ctx, NULL, Value, recurse);
                if (code < 0)
                    goto exit;
            }
            /* don't store the object if it's a stream (leave as a ref) */
            if (pdfi_type_of(Value) != PDF_STREAM)
                code = pdfi_dict_put_obj(ctx, dict, (pdf_obj *)Key, Value, true);
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

    switch(pdfi_type_of(value)) {
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

    if (pdf_object_num(value) != 0) {
        if (pdfi_loop_detector_check_object(ctx, value->object_num)) {
            code = gs_note_error(gs_error_circular_reference);
            goto exit;
        }
        code = pdfi_loop_detector_add_object(ctx, value->object_num);
        if (code < 0) goto exit;
    }
    code = pdfi_resolve_indirect(ctx, value, recurse);

 exit:
    (void)pdfi_loop_detector_cleartomark(ctx); /* Clear to the mark for the current loop */
    return code;
}
