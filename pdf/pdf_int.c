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

/* The PDF interpreter written in C */

#include "plmain.h"
#include "pdf_int.h"
#include "pdf_file.h"
#include "pdf_loop_detect.h"
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

/* Prototype for the benefit of pdfi_read_object */
static int skip_eol(pdf_context *ctx, pdf_stream *s);

/***********************************************************************************/
/* Functions to create the various kinds of 'PDF objects', Created objects have a  */
/* reference count of 0. Composite objects (dictionaries, arrays, strings) use the */
/* 'size' argument to create an object with the correct numbr of entries or of the */
/* requested size. Simple objects (integers etc) ignore this parameter.            */
/* Objects do not get their data assigned, that's up to the caller, but we do      */
/* set the lngth or size fields for composite objects.                             */

int pdfi_alloc_object(pdf_context *ctx, pdf_obj_type type, unsigned int size, pdf_obj **obj)
{
    int bytes = 0;

    switch(type) {
        case PDF_ARRAY_MARK:
        case PDF_DICT_MARK:
        case PDF_PROC_MARK:
        case PDF_NULL:
            bytes = sizeof(pdf_obj);
            break;
        case PDF_INT:
        case PDF_REAL:
            bytes = sizeof(pdf_num);
            break;
        case PDF_STRING:
        case PDF_NAME:
            bytes = sizeof(pdf_string);
            break;
        case PDF_ARRAY:
            bytes = sizeof(pdf_array);
            break;
        case PDF_DICT:
            bytes = sizeof(pdf_dict);
            break;
        case PDF_INDIRECT:
            bytes = sizeof(pdf_indirect_ref);
            break;
        case PDF_BOOL:
            bytes = sizeof(pdf_bool);
            break;
        case PDF_KEYWORD:
            bytes = sizeof(pdf_keyword);
            break;
        /* The following aren't PDF object types, but are objects we either want to
         * reference count, or store on the stack.
         */
        case PDF_XREF_TABLE:
            bytes = sizeof(xref_table);
            break;
        default:
            return_error(gs_error_typecheck);
    }
    *obj = (pdf_obj *)gs_alloc_bytes(ctx->memory, bytes, "pdfi_alloc_object");
    if (*obj == NULL)
        return_error(gs_error_VMerror);

    memset(*obj, 0x00, bytes);
    (*obj)->ctx = ctx;
    (*obj)->type = type;

    switch(type) {
        case PDF_NULL:
        case PDF_INT:
        case PDF_REAL:
        case PDF_INDIRECT:
        case PDF_BOOL:
        case PDF_ARRAY_MARK:
        case PDF_DICT_MARK:
        case PDF_PROC_MARK:
            break;
        case PDF_KEYWORD:
        case PDF_STRING:
        case PDF_NAME:
            {
                unsigned char *data = NULL;
                data = (unsigned char *)gs_alloc_bytes(ctx->memory, size, "pdfi_alloc_object");
                if (data == NULL) {
                    gs_free_object(ctx->memory, *obj, "pdfi_alloc_object");
                    *obj = NULL;
                    return_error(gs_error_VMerror);
                }
                ((pdf_string *)*obj)->data = data;
                ((pdf_string *)*obj)->length = size;
            }
            break;
        case PDF_ARRAY:
            {
                pdf_obj **values = NULL;

                ((pdf_array *)*obj)->size = size;
                if (size > 0) {
                    values = (pdf_obj **)gs_alloc_bytes(ctx->memory, size * sizeof(pdf_obj *), "pdfi_alloc_object");
                    if (values == NULL) {
                        gs_free_object(ctx->memory, *obj, "pdfi_alloc_object");
                        gs_free_object(ctx->memory, values, "pdfi_alloc_object");
                        *obj = NULL;
                        return_error(gs_error_VMerror);
                    }
                    ((pdf_array *)*obj)->values = values;
                    memset(((pdf_array *)*obj)->values, 0x00, size * sizeof(pdf_obj *));
                }
            }
            break;
        case PDF_DICT:
            {
                pdf_obj **keys = NULL, **values = NULL;

                ((pdf_dict *)*obj)->size = size;
                if (size > 0) {
                    keys = (pdf_obj **)gs_alloc_bytes(ctx->memory, size * sizeof(pdf_obj *), "pdfi_alloc_object");
                    values = (pdf_obj **)gs_alloc_bytes(ctx->memory, size * sizeof(pdf_obj *), "pdfi_alloc_object");
                    if (keys == NULL || values == NULL) {
                        gs_free_object(ctx->memory, *obj, "pdfi_alloc_object");
                        gs_free_object(ctx->memory, keys, "pdfi_alloc_object");
                        gs_free_object(ctx->memory, values, "pdfi_alloc_object");
                        *obj = NULL;
                        return_error(gs_error_VMerror);
                    }
                    ((pdf_dict *)*obj)->values = values;
                    ((pdf_dict *)*obj)->keys = keys;
                    memset(((pdf_dict *)*obj)->values, 0x00, size * sizeof(pdf_obj *));
                    memset(((pdf_dict *)*obj)->keys, 0x00, size * sizeof(pdf_obj *));
                }
            }
            break;
        /* The following aren't PDF object types, but are objects we either want to
         * reference count, or store on the stack.
         */
        case PDF_XREF_TABLE:
            break;
        default:
            break;
    }
#if REFCNT_DEBUG
    (*obj)->UID = ctx->UID++;
    dmprintf2(ctx->memory, "Allocated object of type %c with UID %"PRIi64"\n", (*obj)->type, (*obj)->UID);
#endif
    return 0;
}

/***********************************************************************************/
/* Functions to free the various kinds of 'PDF objects' and stack manipulations.   */
/* All objects are reference counted. Pushign an object onto the stack increments  */
/* its reference count, popping it from the stack decrements its reference count.  */
/* When an object's reference count is decremented to 0, the relevant 'free'       */
/* function is called to free the object.                                          */

static void pdfi_free_namestring(pdf_obj *o)
{
    /* Currently names and strings are the same, so a single cast is OK */
    pdf_name *n = (pdf_name *)o;

    if (n->data != NULL)
        gs_free_object(OBJ_MEMORY(n), n->data, "pdf interpreter free name or string data");
    gs_free_object(OBJ_MEMORY(n), n, "pdf interpreter free name or string");
}

static void pdfi_free_keyword(pdf_obj *o)
{
    /* Currently names and strings are the same, so a single cast is OK */
    pdf_keyword *k = (pdf_keyword *)o;

    if (k->data != NULL)
        gs_free_object(OBJ_MEMORY(k), k->data, "pdf interpreter free keyword data");
    gs_free_object(OBJ_MEMORY(k), k, "pdf interpreter free keyword");
}

static void pdfi_free_xref_table(pdf_obj *o)
{
    xref_table *xref = (xref_table *)o;

    gs_free_object(OBJ_MEMORY(xref), xref->xref, "pdfi_free_xref_table");
    gs_free_object(OBJ_MEMORY(xref), xref, "pdfi_free_xref_table");
}

void pdfi_free_object(pdf_obj *o)
{
    switch(o->type) {
        case PDF_ARRAY_MARK:
        case PDF_DICT_MARK:
        case PDF_PROC_MARK:
        case PDF_NULL:
        case PDF_INT:
        case PDF_REAL:
        case PDF_INDIRECT:
        case PDF_BOOL:
            gs_free_object(OBJ_MEMORY(o), o, "pdf interpreter object refcount to 0");
            break;
        case PDF_STRING:
        case PDF_NAME:
            pdfi_free_namestring(o);
            break;
        case PDF_ARRAY:
            pdfi_array_free(o);
            break;
        case PDF_DICT:
            pdfi_free_dict(o);
            break;
        case PDF_KEYWORD:
            pdfi_free_keyword(o);
            break;
        case PDF_XREF_TABLE:
            pdfi_free_xref_table(o);
            break;
        case PDF_FONT:
            pdfi_free_font(o);
            break;
        case PDF_CMAP:
            pdfi_free_cmap(o);
            break;
        default:
            dbgmprintf(OBJ_MEMORY(o), "!!! Attempting to free unknown obect type !!!\n");
            break;
    }
}

/***********************************************************************************/
/* Functions to dereference object references and manage the object cache          */

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

/* Determine if a PDF object is in a compressed ObjStm. Returns < 0
 * for an error, 0 if it is not in a compressed ObjStm and 1 if it is.
 * Currently errors are inmpossible.
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

/*
 * Technically we can accept a stream other than the main PDF file stream here. This is
 * really for the case of compressed objects where we read tokens from the compressed
 * stream, but it also (with some judicious tinkering) allows us to layer a SubFileDecode
 * on top of the main file stream, which may be useful. Note that this cannot work with
 * objects in compressed object streams! They should always pass a value of 0 for the stream_offset.
 * The stream_offset is the offset from the start of the underlying uncompressed PDF file of
 * the stream we are using. See the comments below when keyword is PDF_STREAM.
 */

/* This reads an object *after* the x y obj keyword has been found. Its broken out
 * separately for the benefit of the repair code when reading the dictionary following
 * the 'trailer' keyword, which does not have a 'obj' keyword. Note that it also does
 * not have an 'endobj', we rely on the error handling to take care of that for us.
 */
static int pdfi_read_bare_object(pdf_context *ctx, pdf_stream *s, gs_offset_t stream_offset, uint32_t objnum, uint32_t gen)
{
    int code = 0;
    int64_t i;
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
    if (keyword->key == PDF_ENDOBJ) {
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
    if (keyword->key == PDF_STREAM) {
        /* We should never read a 'stream' keyword from a compressed object stream
         * so this case should never end up here.
         */
        pdf_dict *d = (pdf_dict *)ctx->stack_top[-2];
        gs_offset_t offset;

        skip_eol(ctx, s);

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

        pdfi_pop(ctx, 1);

        if (pdfi_count_stack(ctx) < 1)
            return_error(gs_error_stackunderflow);

        d = (pdf_dict *)ctx->stack_top[-1];

        if (d->type != PDF_DICT) {
            pdfi_pop(ctx, 1);
            return_error(gs_error_syntaxerror);
        }
        d->indirect_num = d->object_num = objnum;
        d->indirect_gen = d->generation_num = gen;
        d->stream_offset = offset;
        if (code < 0) {
            pdfi_pop(ctx, 1);
            return code;
        }

        /* This code may be a performance overhead, it simply skips over the stream contents
         * and checks that the stream ends with a 'endstream endobj' pair. We could add a
         * 'go faster' flag for users who are certain their PDF files are well-formed. This
         * could also allow us to skip all kinds of other checking.....
         */

        code = pdfi_dict_get_int(ctx, d, "Length", &i);
        if (code < 0) {
            dmprintf1(ctx->memory, "Stream object %u missing mandatory keyword /Length, unable to verify the stream length.\n", objnum);
            ctx->pdf_errors |= E_PDF_BADSTREAM;
            return 0;
        }
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
        if (keyword->key != PDF_ENDSTREAM) {
            dmprintf2(ctx->memory, "Stream object %u has an incorrect /Length of %"PRIu64"\n", objnum, i);
            pdfi_pop(ctx, 1);
            return 0;
        }
        pdfi_pop(ctx, 1);

        code = pdfi_read_token(ctx, ctx->main_stream, objnum, gen);
        if (code < 0) {
            if (ctx->pdfstoponerror)
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
            if (ctx->pdfstoponerror)
                return_error(gs_error_typecheck);
            ctx->pdf_errors |= E_PDF_MISSINGENDOBJ;
            /* Didn't find an endobj, but we have an endstream, so assume
             * for now that will suffice
             */
            return 0;
        }
        keyword = ((pdf_keyword *)ctx->stack_top[-1]);
        if (keyword->key != PDF_ENDOBJ) {
            pdfi_pop(ctx, 2);
            return_error(gs_error_typecheck);
        }
        pdfi_pop(ctx, 1);
        return 0;
    }
    if (keyword->key == PDF_OBJ) {
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
    if (!ctx->pdfstoponerror) {
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

static int pdfi_read_object(pdf_context *ctx, pdf_stream *s, gs_offset_t stream_offset)
{
    int code = 0;
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
    if (((pdf_obj *)ctx->stack_top[-1])->type != PDF_INT) {
        pdfi_pop(ctx, 1);
        return_error(gs_error_typecheck);
    }
    objnum = ((pdf_num *)ctx->stack_top[-1])->value.i;
    pdfi_pop(ctx, 1);

    code = pdfi_read_token(ctx, s, 0, 0);
    if (code < 0)
        return code;
    if (((pdf_obj *)ctx->stack_top[-1])->type != PDF_INT) {
        pdfi_pop(ctx, 1);
        return_error(gs_error_typecheck);
    }
    gen = ((pdf_num *)ctx->stack_top[-1])->value.i;
    pdfi_pop(ctx, 1);

    code = pdfi_read_token(ctx, s, 0, 0);
    if (code < 0)
        return code;
    if (((pdf_obj *)ctx->stack_top[-1])->type != PDF_KEYWORD) {
        pdfi_pop(ctx, 1);
        return_error(gs_error_typecheck);
    }
    keyword = ((pdf_keyword *)ctx->stack_top[-1]);
    if (keyword->key != PDF_OBJ) {
        pdfi_pop(ctx, 1);
        return_error(gs_error_syntaxerror);
    }
    pdfi_pop(ctx, 1);

    return pdfi_read_bare_object(ctx, s, stream_offset, objnum, gen);
}

/* pdf_dereference returns an object with a reference count of at least 1, this represents the
 * reference being held by the caller (in **object) when we return from this function.
 */
int pdfi_dereference(pdf_context *ctx, uint64_t obj, uint64_t gen, pdf_obj **object)
{
    xref_entry *entry;
    pdf_obj *o;
    int code;
    gs_offset_t saved_stream_offset;
    bool saved_decrypt_strings = ctx->decrypt_strings;
    pdf_dict *compressed_object = NULL;

    *object = NULL;

    if (obj >= ctx->xref_table->xref_size) {
        dmprintf1(ctx->memory, "Error, attempted to dereference object %"PRIu64", which is not present in the xref table\n", obj);
        ctx->pdf_errors |= E_PDF_BADOBJNUMBER;

        if(ctx->pdfstoponerror)
            return_error(gs_error_rangecheck);

        code = pdfi_alloc_object(ctx, PDF_NULL, 0, object);
        if (code == 0)
            pdfi_countup(*object);
        return code;
    }

    entry = &ctx->xref_table->xref[obj];

    if(entry->object_num == 0)
        return_error(gs_error_undefined);

    if (entry->free) {
        dmprintf1(ctx->memory, "Attempt to dereference free object %"PRIu64", returning a null object.\n", entry->object_num);
        code = pdfi_alloc_object(ctx, PDF_NULL, 1, object);
        if (code >= 0)
            pdfi_countup(*object);
        return code;
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

        ctx->decrypt_strings = false;

        if (entry->compressed) {
            /* This is an object in a compressed object stream */

            xref_entry *compressed_entry = &ctx->xref_table->xref[entry->u.compressed.compressed_stream_num];
            pdf_stream *compressed_stream, *SubFile_stream, *Object_stream;
            char Buffer[256];
            int i = 0, object_length = 0;
            int64_t num_entries, found_object;
            gs_offset_t offset = 0;

            if (ctx->pdfdebug) {
                dmprintf1(ctx->memory, "%% Reading compressed object (%"PRIi64" 0 obj)", obj);
                dmprintf1(ctx->memory, " from ObjStm with object number %"PRIi64"\n", compressed_entry->object_num);
            }

            if (compressed_entry->cache == NULL) {
#if CACHE_STATISTICS
                ctx->compressed_misses++;
#endif
                code = pdfi_seek(ctx, ctx->main_stream, compressed_entry->u.uncompressed.offset, SEEK_SET);
                if (code < 0)
                    goto error;

                code = pdfi_read_object(ctx, ctx->main_stream, 0);
                if (code < 0)
                    goto error;

                compressed_object = (pdf_dict *)ctx->stack_top[-1];
                if (compressed_object->type != PDF_DICT) {
                    pdfi_pop(ctx, 1);
                    code = gs_note_error(gs_error_typecheck);
                    goto error;
                }
                if (compressed_object->object_num != compressed_entry->object_num) {
                    pdfi_pop(ctx, 1);
                    /* Same error (undefined) as when we read an uncompressed object with the wrong number */
                    code = gs_note_error(gs_error_undefined);
                    goto error;
                }
                pdfi_countup(compressed_object);
                pdfi_pop(ctx, 1);
                code = pdfi_add_to_cache(ctx, (pdf_obj *)compressed_object);
                if (code < 0) {
                    goto error;
                }
            } else {
#if CACHE_STATISTICS
                ctx->compressed_hits++;
#endif
                compressed_object = (pdf_dict *)compressed_entry->cache->o;
                pdfi_countup(compressed_object);
                pdfi_promote_cache_entry(ctx, compressed_entry->cache);
            }
            /* Check its an ObjStm ! */
            code = pdfi_dict_get_type(ctx, compressed_object, "Type", PDF_NAME, &o);
            if (code < 0)
                goto error;

            if (((pdf_name *)o)->length != 6 || memcmp(((pdf_name *)o)->data, "ObjStm", 6) != 0){
                pdfi_countdown(o);
                code = gs_note_error(gs_error_syntaxerror);
                goto error;
            }
            pdfi_countdown(o);

            /* Need to check the /N entry to see if the object is actually in this stream! */
            code = pdfi_dict_get_int(ctx, compressed_object, "N", &num_entries);
            if (code < 0)
                goto error;

            if (num_entries < 0 || num_entries > ctx->xref_table->xref_size) {
                code = gs_note_error(gs_error_rangecheck);
                goto error;
            }

            code = pdfi_seek(ctx, ctx->main_stream, compressed_object->stream_offset, SEEK_SET);
            if (code < 0)
                goto error;

            code = pdfi_dict_get_type(ctx, compressed_object, "Length", PDF_INT, &o);
            if (code < 0)
                goto error;

            code = pdfi_apply_SubFileDecode_filter(ctx, ((pdf_num *)o)->value.i, NULL, ctx->main_stream, &SubFile_stream, false);
            pdfi_countdown(o);
            if (code < 0)
                goto error;

            code = pdfi_filter(ctx, compressed_object, SubFile_stream, &compressed_stream, false);
            if (code < 0)
                goto error;

            for (i=0;i < num_entries;i++)
            {
                code = pdfi_read_token(ctx, compressed_stream, obj, gen);
                if (code < 0) {
                    pdfi_close_file(ctx, compressed_stream);
                    pdfi_close_file(ctx, SubFile_stream);
                    goto error;
                }
                o = ctx->stack_top[-1];
                if (((pdf_obj *)o)->type != PDF_INT) {
                    pdfi_pop(ctx, 1);
                    pdfi_close_file(ctx, compressed_stream);
                    pdfi_close_file(ctx, SubFile_stream);
                    goto error;
                }
                found_object = ((pdf_num *)o)->value.i;
                pdfi_pop(ctx, 1);
                code = pdfi_read_token(ctx, compressed_stream, obj, gen);
                if (code < 0) {
                    pdfi_close_file(ctx, compressed_stream);
                    pdfi_close_file(ctx, SubFile_stream);
                    goto error;
                }
                o = ctx->stack_top[-1];
                if (((pdf_obj *)o)->type != PDF_INT) {
                    pdfi_pop(ctx, 1);
                    pdfi_close_file(ctx, compressed_stream);
                    pdfi_close_file(ctx, SubFile_stream);
                    goto error;
                }
                if (i == entry->u.compressed.object_index) {
                    if (found_object != obj) {
                        pdfi_pop(ctx, 1);
                        pdfi_close_file(ctx, compressed_stream);
                        pdfi_close_file(ctx, SubFile_stream);
                        code = gs_note_error(gs_error_undefined);
                        goto error;
                    }
                    offset = ((pdf_num *)o)->value.i;
                }
                if (i == entry->u.compressed.object_index + 1)
                    object_length = ((pdf_num *)o)->value.i - offset;
                pdfi_pop(ctx, 1);
            }

            /* Skip to the offset of the object we want to read */
            for (i=0;i < offset;i++)
            {
                code = pdfi_read_bytes(ctx, (byte *)&Buffer[0], 1, 1, compressed_stream);
                if (code <= 0) {
                    pdfi_close_file(ctx, compressed_stream);
                    pdfi_close_file(ctx, SubFile_stream);
                    ctx->decrypt_strings = saved_decrypt_strings;
                    return_error(gs_error_ioerror);
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
                if (code < 0) {
                    pdfi_close_file(ctx, SubFile_stream);
                    goto error;
                }
            } else {
                Object_stream = compressed_stream;
            }

            code = pdfi_read_token(ctx, Object_stream, obj, gen);
            if (code < 0) {
                pdfi_close_file(ctx, Object_stream);
                pdfi_close_file(ctx, SubFile_stream);
                goto error;
            }
            if (ctx->stack_top[-1]->type == PDF_ARRAY_MARK || ctx->stack_top[-1]->type == PDF_DICT_MARK) {
                int start_depth = pdfi_count_stack(ctx);

                /* Need to read all the elements from COS objects */
                do {
                    code = pdfi_read_token(ctx, Object_stream, obj, gen);
                    if (code < 0) {
                        pdfi_close_file(ctx, Object_stream);
                        pdfi_close_file(ctx, SubFile_stream);
                        goto error;
                    }
                    if (compressed_stream->eof == true) {
                        pdfi_close_file(ctx, Object_stream);
                        pdfi_close_file(ctx, SubFile_stream);
                        code = gs_note_error(gs_error_ioerror);
                        goto error;
                    }
                }while ((ctx->stack_top[-1]->type != PDF_ARRAY && ctx->stack_top[-1]->type != PDF_DICT) || pdfi_count_stack(ctx) > start_depth);
            }

            pdfi_close_file(ctx, Object_stream);
            if (Object_stream != compressed_stream)
                pdfi_close_file(ctx, compressed_stream);
            pdfi_close_file(ctx, SubFile_stream);

            *object = ctx->stack_top[-1];
            /* For compressed objects we don't get a 'obj gen obj' sequence which is what sets
             * the object number for uncompressed objects. So we need to do that here.
             */
            (*object)->indirect_num = (*object)->object_num = obj;
            (*object)->indirect_gen = (*object)->generation_num = gen;
            pdfi_countup(*object);
            pdfi_pop(ctx, 1);

            pdfi_countdown(compressed_object);
            code = pdfi_add_to_cache(ctx, *object);
            if (code < 0) {
                pdfi_countdown(*object);
                goto error;
            }
        } else {
            pdf_stream *SubFile_stream = NULL;
            pdf_string *EODString;
#if CACHE_STATISTICS
            ctx->misses++;
#endif
            code = pdfi_seek(ctx, ctx->main_stream, entry->u.uncompressed.offset, SEEK_SET);
            if (code < 0)
                goto error;

            code = pdfi_make_name(ctx, (byte *)"trailer", 6, (pdf_obj **)&EODString);
            if (code < 0)
                goto error;

            code = pdfi_apply_SubFileDecode_filter(ctx, 0, EODString, ctx->main_stream, &SubFile_stream, false);
            if (code < 0) {
                pdfi_countdown(EODString);
                goto error;
            }

            code = pdfi_read_object(ctx, SubFile_stream, entry->u.uncompressed.offset);

            pdfi_countdown(EODString);
            pdfi_close_file(ctx, SubFile_stream);
            if (code < 0)
                goto error;

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
                code = gs_note_error(gs_error_undefined);
                goto error;
            }
        }
        (void)pdfi_seek(ctx, ctx->main_stream, saved_stream_offset, SEEK_SET);
    }

    if (ctx->loop_detection) {
        code = pdfi_loop_detector_add_object(ctx, (*object)->object_num);
        if (code < 0) {
            ctx->decrypt_strings = saved_decrypt_strings;
            return code;
        }
    }
    ctx->decrypt_strings = saved_decrypt_strings;
    return 0;

error:
    ctx->decrypt_strings = saved_decrypt_strings;
    pdfi_countdown(compressed_object);
    (void)pdfi_seek(ctx, ctx->main_stream, saved_stream_offset, SEEK_SET);
    pdfi_clearstack(ctx);
    return code;
}

/* do a derefence with loop detection */
int
pdfi_deref_loop_detect(pdf_context *ctx, uint64_t obj, uint64_t gen, pdf_obj **object)
{
    int code;

    code = pdfi_loop_detector_mark(ctx);
    if (code < 0)
        return code;

    code = pdfi_dereference(ctx, obj, gen, object);
    (void)pdfi_loop_detector_cleartomark(ctx);
    return code;
}

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
int pdfi_skip_white(pdf_context *ctx, pdf_stream *s)
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

static int skip_eol(pdf_context *ctx, pdf_stream *s)
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
    } while (bytes != 0 && (c == 0x0A || c == 0x0D));

    if (read > 0)
        pdfi_unread(ctx, s, &c, 1);
    return 0;
}

static int pdfi_read_num(pdf_context *ctx, pdf_stream *s, uint32_t indirect_num, uint32_t indirect_gen)
{
    byte Buffer[256];
    unsigned short index = 0;
    short bytes;
    bool real = false;
    bool has_decimal_point = false;
    bool has_exponent = false;
    unsigned short exponent_index = 0;
    pdf_num *num;
    int code = 0, malformed = false;

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
                if (ctx->pdfstoponerror)
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
                if (ctx->pdfstoponerror)
                    return_error(gs_error_syntaxerror);
                malformed = true;
            } else {
                ctx->pdf_warnings |= W_PDF_NUM_EXPONENT;
                has_exponent = true;
                exponent_index = index;
                real = true;
            }
        } else if (Buffer[index] == '-' || Buffer[index] == '+') {
            if (!(index == 0 || (has_exponent && index == exponent_index+1))) {
                if (ctx->pdfstoponerror)
                    return_error(gs_error_syntaxerror);
                malformed = true;
            }
        } else if (Buffer[index] < 0x30 || Buffer[index] > 0x39) {
            if (ctx->pdfstoponerror)
                return_error(gs_error_syntaxerror);
            if (!(ctx->pdf_errors & E_PDF_MISSINGWHITESPACE))
                dmprintf(ctx->memory, "Ignoring missing white space while parsing number\n");
            ctx->pdf_errors |= E_PDF_MISSINGWHITESPACE;
            pdfi_unread(ctx, s, (byte *)&Buffer[index], 1);
            Buffer[index] = 0x00;
            break;
        }
        if (++index > 255)
            return_error(gs_error_syntaxerror);
    } while(1);

    if (real && !malformed)
        code = pdfi_alloc_object(ctx, PDF_REAL, 0, (pdf_obj **)&num);
    else
        code = pdfi_alloc_object(ctx, PDF_INT, 0, (pdf_obj **)&num);
    if (code < 0)
        return code;

    if (malformed) {
        if (!(ctx->pdf_errors & E_PDF_MALFORMEDNUMBER))
            dmprintf1(ctx->memory, "Treating malformed number %s as 0.\n", Buffer);
        ctx->pdf_errors |= E_PDF_MALFORMEDNUMBER;
        num->value.i = 0;
    } else {
        if (real) {
            float tempf;
            if (sscanf((const char *)Buffer, "%f", &tempf) == 0) {
                if (ctx->pdfdebug)
                    dmprintf1(ctx->memory, "failed to read real number : %s\n", Buffer);
                gs_free_object(OBJ_MEMORY(num), num, "pdfi_read_num error");
                return_error(gs_error_syntaxerror);
            }
            num->value.d = tempf;
        } else {
            int tempi;
            if (sscanf((const char *)Buffer, "%d", &tempi) == 0) {
                if (ctx->pdfdebug)
                    dmprintf1(ctx->memory, "failed to read integer : %s\n", Buffer);
                gs_free_object(OBJ_MEMORY(num), num, "pdfi_read_num error");
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
    num->indirect_num = indirect_num;
    num->indirect_gen = indirect_gen;

    code = pdfi_push(ctx, (pdf_obj *)num);

    if (code < 0)
        pdfi_free_object((pdf_obj *)num);

    return code;
}

static int pdfi_read_name(pdf_context *ctx, pdf_stream *s, uint32_t indirect_num, uint32_t indirect_gen)
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
            if (bytes < 2) {
                gs_free_object(ctx->memory, Buffer, "pdfi_read_name error");
                return_error(gs_error_ioerror);
            }

            if (!ishex(NumBuf[0]) || !ishex(NumBuf[1])) {
                gs_free_object(ctx->memory, Buffer, "pdfi_read_name error");
                return_error(gs_error_ioerror);
            }

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

    code = pdfi_alloc_object(ctx, PDF_NAME, index, (pdf_obj **)&name);
    if (code < 0) {
        gs_free_object(ctx->memory, Buffer, "pdfi_read_name error");
        return code;
    }
    memcpy(name->data, Buffer, index);
    name->indirect_num = indirect_num;
    name->indirect_gen = indirect_gen;

    if (ctx->pdfdebug)
        dmprintf1(ctx->memory, " /%s", Buffer);

    gs_free_object(ctx->memory, Buffer, "pdfi_read_name");

    code = pdfi_push(ctx, (pdf_obj *)name);

    if (code < 0)
        pdfi_free_namestring((pdf_obj *)name);

    return code;
}

static int pdfi_read_hexstring(pdf_context *ctx, pdf_stream *s, uint32_t indirect_num, uint32_t indirect_gen)
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

    if (ctx->pdfdebug)
        dmprintf(ctx->memory, " <");

    do {
        do {
            bytes = pdfi_read_bytes(ctx, (byte *)HexBuf, 1, 1, s);
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
            bytes = pdfi_read_bytes(ctx, (byte *)&HexBuf[1], 1, 1, s);
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
            NewBuf = (char *)gs_alloc_bytes(ctx->memory, size + 256, "pdfi_read_hexstring");
            if (NewBuf == NULL) {
                gs_free_object(ctx->memory, Buffer, "pdfi_read_hexstring error");
                return_error(gs_error_VMerror);
            }
            memcpy(NewBuf, Buffer, size);
            gs_free_object(ctx->memory, Buffer, "pdfi_read_hexstring");
            Buffer = NewBuf;
            size += 256;
        }
    } while(1);

    if (ctx->pdfdebug)
        dmprintf(ctx->memory, ">");

    code = pdfi_alloc_object(ctx, PDF_STRING, index, (pdf_obj **)&string);
    if (code < 0) {
        gs_free_object(ctx->memory, Buffer, "pdfi_read_name error");
        return code;
    }
    memcpy(string->data, Buffer, index);
    string->indirect_num = indirect_num;
    string->indirect_gen = indirect_gen;

    gs_free_object(ctx->memory, Buffer, "pdfi_read_hexstring");

    code = pdfi_push(ctx, (pdf_obj *)string);
    if (code < 0)
        pdfi_free_namestring((pdf_obj *)string);

    return code;
}

static int pdfi_read_string(pdf_context *ctx, pdf_stream *s, uint32_t indirect_num, uint32_t indirect_gen)
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
                    ctx->pdf_errors |= E_PDF_UNESCAPEDSTRING;
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

    code = pdfi_alloc_object(ctx, PDF_STRING, index, (pdf_obj **)&string);
    if (code < 0) {
        gs_free_object(ctx->memory, Buffer, "pdfi_read_name error");
        return code;
    }
    memcpy(string->data, Buffer, index);
    string->indirect_num = indirect_num;
    string->indirect_gen = indirect_gen;

    gs_free_object(ctx->memory, Buffer, "pdfi_read_string");

    if (ctx->is_encrypted && ctx->decrypt_strings) {
        code = pdfi_decrypt_string(ctx, string);
        if (code < 0)
            return code;
    }

    if (ctx->pdfdebug) {
        int i;
        dmprintf(ctx->memory, " (");
        for (i=0;i<string->length;i++)
            dmprintf1(ctx->memory, "%c", string->data[i]);
        dmprintf(ctx->memory, ")");
    }

    code = pdfi_push(ctx, (pdf_obj *)string);
    if (code < 0)
        pdfi_free_namestring((pdf_obj *)string);

    return code;
}

int pdfi_read_dict(pdf_context *ctx, pdf_stream *s, uint32_t indirect_num, uint32_t indirect_gen)
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

static int pdfi_skip_comment(pdf_context *ctx, pdf_stream *s)
{
    byte Buffer;
    short bytes = 0;

    if (ctx->pdfdebug)
        dmprintf (ctx->memory, " %%");

    do {
        bytes = pdfi_read_bytes(ctx, (byte *)&Buffer, 1, 1, s);
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
static int pdfi_read_keyword(pdf_context *ctx, pdf_stream *s, uint32_t indirect_num, uint32_t indirect_gen)
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
        if (ctx->pdfstoponerror)
            return_error(gs_error_syntaxerror);
        strcpy((char *)Buffer, "KEYWORD_TOO_LONG");
        index = 16;
    }

    /* NB The code below uses 'Buffer', not the data stored in keyword->data to compare strings */
    Buffer[index] = 0x00;

    code = pdfi_alloc_object(ctx, PDF_KEYWORD, index, (pdf_obj **)&keyword);
    if (code < 0)
        return code;

    memcpy(keyword->data, Buffer, index);
    pdfi_countup(keyword);

    keyword->indirect_num = indirect_num;
    keyword->indirect_gen = indirect_gen;

    if (ctx->pdfdebug)
        dmprintf1(ctx->memory, " %s\n", Buffer);

    switch(Buffer[0]) {
        case 'K':
            if (keyword->length == 16 && memcmp(keyword->data, "KEYWORD_TOO_LONG", 16) == 0) {
                keyword->key = PDF_INVALID_KEY;
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

                code = pdfi_alloc_object(ctx, PDF_INDIRECT, 0, (pdf_obj **)&o);
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
                if (code < 0) {
                    pdfi_countdown(keyword);
                    return code;
                }
            }
            else {
                if (keyword->length == 9 && memcmp((const char *)Buffer, "startxref", 9) == 0)
                    keyword->key = PDF_STARTXREF;
            }
            break;
        case 't':
            if (keyword->length == 4 && memcmp((const char *)Buffer, "true", 4) == 0) {
                pdf_bool *o;

                pdfi_countdown(keyword);

                code = pdfi_alloc_object(ctx, PDF_BOOL, 0, (pdf_obj **)&o);
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
                    keyword->key = PDF_TRAILER;
            }
            break;
        case 'f':
            if (keyword->length == 5 && memcmp((const char *)Buffer, "false", 5) == 0)
            {
                pdf_bool *o;

                pdfi_countdown(keyword);

                code = pdfi_alloc_object(ctx, PDF_BOOL, 0, (pdf_obj **)&o);
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

                code = pdfi_alloc_object(ctx, PDF_NULL, 0, &o);
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
                keyword->key = PDF_XREF;
            break;
    }

    code = pdfi_push(ctx, (pdf_obj *)keyword);
    pdfi_countdown(keyword);

    return code;
}

/* This function reads from the given stream, at the current offset in the stream,
 * a single PDF 'token' and returns it on the stack.
 */
int pdfi_read_token(pdf_context *ctx, pdf_stream *s, uint32_t indirect_num, uint32_t indirect_gen)
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
                if (ctx->pdfdebug)
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
            if (ctx->pdfdebug)
                dmprintf (ctx->memory, "[");
            return pdfi_mark_stack(ctx, PDF_ARRAY_MARK);
            break;
        case ']':
            code = pdfi_array_from_stack(ctx, indirect_num, indirect_gen);
            if (code < 0) {
                if (code == gs_error_VMerror || code == gs_error_ioerror || ctx->pdfstoponerror)
                    return code;
                pdfi_clearstack(ctx);
                return pdfi_read_token(ctx, s, indirect_num, indirect_gen);
            }
            break;
        case '{':
            if (ctx->pdfdebug)
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
                if (ctx->pdfstoponerror)
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
int pdfi_make_name(pdf_context *ctx, byte *n, uint32_t size, pdf_obj **o)
{
    int code;
    *o = NULL;

    code = pdfi_alloc_object(ctx, PDF_NAME, size, o);
    if (code < 0)
        return code;
    pdfi_countup(*o);

    memcpy(((pdf_name *)*o)->data, n, size);

    return 0;
}

/* Now routines to open a PDF file (and clean up in the event of an error) */

static int pdfi_repair_add_object(pdf_context *ctx, uint64_t obj, uint64_t gen, gs_offset_t offset)
{
    /* Although we can handle object numbers larger than this, on some systems (32-bit Windows)
     * memset is limited to a (signed!) integer for the size of memory to clear. We could deal
     * with this by clearing the memory in blocks, but really, this is almost certainly a
     * corrupted file or something.
     */
    if (obj >= 0x7ffffff / sizeof(xref_entry))
        return_error(gs_error_rangecheck);

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
        ctx->xref_table->ctx = ctx;
        ctx->xref_table->type = PDF_XREF_TABLE;
        ctx->xref_table->xref_size = obj + 1;
#if REFCNT_DEBUG
        ctx->xref_table->UID = ctx->UID++;
        dmprintf1(ctx->memory, "Allocated xref table with UID %"PRIi64"\n", ctx->xref_table->UID);
#endif
        pdfi_countup(ctx->xref_table);
    } else {
        if (ctx->xref_table->xref_size < (obj + 1)) {
            xref_entry *new_xrefs;

            new_xrefs = (xref_entry *)gs_alloc_bytes(ctx->memory, (obj + 1) * sizeof(xref_entry), "read_xref_stream allocate xref table entries");
            if (new_xrefs == NULL){
                pdfi_countdown(ctx->xref_table);
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

int pdfi_repair_file(pdf_context *ctx)
{
    int code;
    gs_offset_t offset;
    uint64_t object_num = 0, generation_num = 0;
    int i;
    gs_offset_t outer_saved_offset[3];

    if (ctx->repaired) {
        dmprintf(ctx->memory, "%% Trying to repair file for second time -- unrepairable\n");
        ctx->pdf_errors |= E_PDF_UNREPAIRABLE;
        return_error(gs_error_undefined);
    }

    ctx->repaired = true;
    ctx->pdf_errors |= E_PDF_REPAIRED;

    pdfi_clearstack(ctx);

    if(ctx->pdfdebug)
        dmprintf(ctx->memory, "%% Error encountered in opening PDF file, attempting repair\n");

    /* First try to locate a %PDF header. If we can't find one, abort this, the file is too broken
     * and may not even be a PDF file.
     */
    pdfi_seek(ctx, ctx->main_stream, 0, SEEK_SET);
    {
        char Buffer[10], test[] = "%PDF";
        int index = 0;

        do {
            code = pdfi_read_bytes(ctx, (byte *)&Buffer[index], 1, 1, ctx->main_stream);
            if (code < 0)
                return code;
            if (Buffer[index] == test[index])
                index++;
            else
                index = 0;
        } while (index < 4 && ctx->main_stream->eof == false);
        if (memcmp(Buffer, test, 4) != 0)
            return_error(gs_error_undefined);
        pdfi_unread(ctx, ctx->main_stream, (byte *)Buffer, 4);
        pdfi_skip_comment(ctx, ctx->main_stream);
    }
    if (ctx->main_stream->eof == true)
        return_error(gs_error_ioerror);

    /* First pass, identify all the objects of the form x y obj */

    do {
        code = pdfi_skip_white(ctx, ctx->main_stream);
        if (code < 0) {
            if (code != gs_error_VMerror && code != gs_error_ioerror) {
                pdfi_clearstack(ctx);
                offset = pdfi_unread_tell(ctx);
                continue;
            } else
                return code;
        }
        offset = pdfi_unread_tell(ctx);
        outer_saved_offset[0] = outer_saved_offset[1] = outer_saved_offset[2] = 0;
        do {
            outer_saved_offset[0] = outer_saved_offset[1];
            outer_saved_offset[1] = outer_saved_offset[2];
            outer_saved_offset[2] = pdfi_unread_tell(ctx);;

            code = pdfi_read_token(ctx, ctx->main_stream, 0, 0);
            if (code < 0) {
                if (code != gs_error_VMerror && code != gs_error_ioerror) {
                    pdfi_clearstack(ctx);
                    offset = pdfi_unread_tell(ctx);
                    continue;
                } else
                    return code;
            }
            if (pdfi_count_stack(ctx) > 0) {
                if (ctx->stack_top[-1]->type == PDF_KEYWORD) {
                    pdf_keyword *k = (pdf_keyword *)ctx->stack_top[-1];
                    pdf_num *n;

                    if (k->key == PDF_OBJ) {
                        gs_offset_t saved_offset[3];

                        offset = outer_saved_offset[0];

                        saved_offset[0] = saved_offset[1] = saved_offset[2] = 0;

                        if (pdfi_count_stack(ctx) < 3 || ctx->stack_top[-2]->type != PDF_INT || ctx->stack_top[-2]->type != PDF_INT) {
                            pdfi_clearstack(ctx);
                            continue;
                        }
                        n = (pdf_num *)ctx->stack_top[-3];
                        object_num = n->value.i;
                        n = (pdf_num *)ctx->stack_top[-2];
                        generation_num = n->value.i;
                        pdfi_clearstack(ctx);

                        do {
                            /* move all the saved offsets up by one */
                            saved_offset[0] = saved_offset[1];
                            saved_offset[1] = saved_offset[2];
                            saved_offset[2] = pdfi_unread_tell(ctx);;

                            code = pdfi_read_token(ctx, ctx->main_stream, 0, 0);
                            if (code < 0) {
                                if (code != gs_error_VMerror && code != gs_error_ioerror)
                                    continue;
                                return code;
                            }
                            if (code == 0 && ctx->main_stream->eof)
                                break;

                            if (ctx->stack_top[-1]->type == PDF_KEYWORD){
                                pdf_keyword *k = (pdf_keyword *)ctx->stack_top[-1];

                                if (k->key == PDF_OBJ) {
                                    /* Found obj while looking for endobj, store the existing 'obj'
                                     * and start afresh.
                                     */
                                    code = pdfi_repair_add_object(ctx, object_num, generation_num, offset);
                                    if (pdfi_count_stack(ctx) < 3 || ctx->stack_top[-2]->type != PDF_INT || ctx->stack_top[-2]->type != PDF_INT) {
                                        pdfi_clearstack(ctx);
                                        break;
                                    }
                                    n = (pdf_num *)ctx->stack_top[-3];
                                    object_num = n->value.i;
                                    n = (pdf_num *)ctx->stack_top[-2];
                                    generation_num = n->value.i;
                                    pdfi_clearstack(ctx);
                                    offset = saved_offset[0];
                                    continue;
                                }

                                if (k->key == PDF_ENDOBJ) {
                                    code = pdfi_repair_add_object(ctx, object_num, generation_num, offset);
                                    if (code < 0)
                                        return code;
                                    pdfi_clearstack(ctx);
                                    break;
                                } else {
                                    if (k->key == PDF_STREAM) {
                                        char Buffer[10], test[] = "endstream";
                                        int index = 0;

                                        do {
                                            code = pdfi_read_bytes(ctx, (byte *)&Buffer[index], 1, 1, ctx->main_stream);
                                            if (code < 0) {
                                                if (code != gs_error_VMerror && code != gs_error_ioerror)
                                                    continue;
                                                return code;
                                            }
                                            if (Buffer[index] == test[index])
                                                index++;
                                            else
                                                index = 0;
                                        } while (index < 9 && ctx->main_stream->eof == false);
                                        do {
                                            code = pdfi_read_token(ctx, ctx->main_stream, 0, 0);
                                            if (code < 0) {
                                                if (code != gs_error_VMerror && code != gs_error_ioerror)
                                                    continue;
                                                return code;
                                            }
                                            if (ctx->stack_top[-1]->type == PDF_KEYWORD){
                                                pdf_keyword *k = (pdf_keyword *)ctx->stack_top[-1];
                                                if (k->key == PDF_ENDOBJ) {
                                                    code = pdfi_repair_add_object(ctx, object_num, generation_num, offset);
                                                    if (code < 0) {
                                                        if (code != gs_error_VMerror && code != gs_error_ioerror)
                                                            break;
                                                        return code;
                                                    }
                                                    break;
                                                }
                                            }
                                        }while(ctx->main_stream->eof == false);

                                        pdfi_clearstack(ctx);
                                        break;
                                    } else {
                                        pdfi_clearstack(ctx);
                                    }
                                }
                            }
                        } while(1);
                        break;
                    } else {
                        if (k->key == PDF_ENDOBJ) {
                            code = pdfi_repair_add_object(ctx, object_num, generation_num, offset);
                            if (code < 0)
                                return code;
                            pdfi_clearstack(ctx);
                        } else
                            if (k->key == PDF_STARTXREF) {
                                code = pdfi_read_token(ctx, ctx->main_stream, 0, 0);
                                if (code < 0 && code != gs_error_VMerror && code != gs_error_ioerror)
                                    continue;
                                if (code < 0)
                                    return code;
                                pdfi_clearstack(ctx);
                            } else {
                                if (k->key == PDF_TRAILER) {
                                    code = pdfi_read_bare_object(ctx, ctx->main_stream, 0, 0, 0);
                                    if (code == 0 && ctx->stack_top[-1]->type == PDF_DICT) {
                                        if (ctx->Trailer) {
                                            pdf_dict *d = (pdf_dict *)ctx->stack_top[-1];
                                            bool known = false;

                                            code = pdfi_dict_known(d, "Root", &known);
                                            if (code == 0 && known) {
                                                pdfi_countdown(ctx->Trailer);
                                                ctx->Trailer = (pdf_dict *)ctx->stack_top[-1];
                                                pdfi_countup(ctx->Trailer);
                                            }
                                        } else {
                                            ctx->Trailer = (pdf_dict *)ctx->stack_top[-1];
                                            pdfi_countup(ctx->Trailer);
                                        }
                                    }
                                }
                                pdfi_clearstack(ctx);
                            }
                    }
                    code = pdfi_skip_white(ctx, ctx->main_stream);
                    if (code < 0) {
                        if (code != gs_error_VMerror && code != gs_error_ioerror) {
                            pdfi_clearstack(ctx);
                            offset = pdfi_unread_tell(ctx);
                            continue;
                        } else
                            return code;
                    }
                    offset = pdfi_unread_tell(ctx);
                }
                if (pdfi_count_stack(ctx) > 0 && ctx->stack_top[-1]->type != PDF_INT)
                    pdfi_clearstack(ctx);
            }
        } while (ctx->main_stream->eof == false);
    } while(ctx->main_stream->eof == false);

    if (ctx->main_stream->eof && ctx->filename) {
        sfclose(ctx->main_stream->s);
        ctx->main_stream->s = sfopen(ctx->filename, "r", ctx->memory);
        if (ctx->main_stream->s == NULL)
            return_error(gs_error_ioerror);
        ctx->main_stream->eof = false;
    } else {
        pdfi_seek(ctx, ctx->main_stream, 0, SEEK_SET);
        ctx->main_stream->eof = false;
    }

    /* Second pass, examine every object we have located to see if its an ObjStm */
    if (ctx->xref_table == NULL || ctx->xref_table->xref_size < 1)
        return_error(gs_error_syntaxerror);

    for (i=1;i < ctx->xref_table->xref_size;i++) {
        if (ctx->xref_table->xref[i].object_num != 0) {
            /* At this stage, all the objects we've found must be uncompressed */
            if (ctx->xref_table->xref[i].u.uncompressed.offset > ctx->main_stream_length)
                return_error(gs_error_rangecheck);

            pdfi_seek(ctx, ctx->main_stream, ctx->xref_table->xref[i].u.uncompressed.offset, SEEK_SET);
            do {
                code = pdfi_read_token(ctx, ctx->main_stream, 0, 0);
                if (ctx->main_stream->eof == true || (code < 0 && code != gs_error_ioerror && code != gs_error_VMerror))
                    break;
                if (code < 0)
                    return code;
                if (ctx->stack_top[-1]->type == PDF_KEYWORD) {
                    pdf_keyword *k = (pdf_keyword *)ctx->stack_top[-1];

                    if (k->key == PDF_OBJ){
                        continue;
                    }
                    if (k->key == PDF_ENDOBJ) {
                        if (pdfi_count_stack(ctx) > 1) {
                            if (ctx->stack_top[-2]->type == PDF_DICT) {
                                pdf_dict *d = (pdf_dict *)ctx->stack_top[-2];
                                pdf_obj *o = NULL;

                                code = pdfi_dict_knownget_type(ctx, d, "Type", PDF_NAME, &o);
                                if (code < 0) {
                                    pdfi_clearstack(ctx);
                                    return code;
                                }
                                if (code > 0) {
                                    pdf_name *n = (pdf_name *)o;

                                    if (pdfi_name_is(n, "Catalog")) {
                                        pdfi_countdown(ctx->Root); /* In case it was already set */
                                        ctx->Root = (pdf_dict *)ctx->stack_top[-2];
                                        pdfi_countup(ctx->Root);
                                    }
                                }
                                pdfi_countdown(o);
                            }
                        }
                        pdfi_clearstack(ctx);
                        break;
                    }
                    if (k->key == PDF_STREAM) {
                        pdf_dict *d;
                        pdf_name *n = NULL;

                        if (pdfi_count_stack(ctx) <= 1) {
                            pdfi_clearstack(ctx);
                            break;;
                        }
                        d = (pdf_dict *)ctx->stack_top[-2];
                        if (d->type != PDF_DICT) {
                            pdfi_clearstack(ctx);
                            break;;
                        }
                        code = pdfi_dict_knownget_type(ctx, d, "Type", PDF_NAME, (pdf_obj **)&n);
                        if (code < 0) {
                            if (ctx->pdfstoponerror || code == gs_error_VMerror) {
                                pdfi_clearstack(ctx);
                                return code;
                            }
                        }
                        if (code > 0) {
                            if (pdfi_name_is(n, "ObjStm")) {
                                int64_t N, obj_num, offset;
                                int j;
                                pdf_stream *compressed_stream;
                                pdf_obj *o;

                                offset = pdfi_unread_tell(ctx);
                                pdfi_seek(ctx, ctx->main_stream, offset, SEEK_SET);
                                code = pdfi_filter(ctx, d, ctx->main_stream, &compressed_stream, false);
                                if (code == 0) {
                                    code = pdfi_dict_get_int(ctx, d, "N", &N);
                                    if (code == 0) {
                                        for (j=0;j < N; j++) {
                                            code = pdfi_read_token(ctx, compressed_stream, 0, 0);
                                            if (code == 0) {
                                                o = ctx->stack_top[-1];
                                                if (((pdf_obj *)o)->type == PDF_INT) {
                                                    obj_num = ((pdf_num *)o)->value.i;
                                                    pdfi_pop(ctx, 1);
                                                    code = pdfi_read_token(ctx, compressed_stream, 0, 0);
                                                    if (code == 0) {
                                                        o = ctx->stack_top[-1];
                                                        if (((pdf_obj *)o)->type == PDF_INT) {
                                                            offset = ((pdf_num *)o)->value.i;
                                                            if (obj_num < 1) {
                                                                pdfi_close_file(ctx, compressed_stream);
                                                                pdfi_clearstack(ctx);
                                                                return_error(gs_error_rangecheck);
                                                            }
                                                            ctx->xref_table->xref[obj_num].compressed = true;
                                                            ctx->xref_table->xref[obj_num].free = false;
                                                            ctx->xref_table->xref[obj_num].object_num = obj_num;
                                                            ctx->xref_table->xref[obj_num].u.compressed.compressed_stream_num = i;
                                                            ctx->xref_table->xref[obj_num].u.compressed.object_index = j;
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                    pdfi_close_file(ctx, compressed_stream);
                                }
                                if (code < 0) {
                                    if (ctx->pdfstoponerror || code == gs_error_VMerror) {
                                        pdfi_clearstack(ctx);
                                        return code;
                                    }
                                }
                            }
                        }
                        pdfi_countdown(n);
                        pdfi_clearstack(ctx);
                        break;
                    }
                }
            } while (1);
        }
    }

    return 0;
}


int pdfi_read_Root(pdf_context *ctx)
{
    pdf_obj *o, *o1;
    int code;

    if (ctx->pdfdebug)
        dmprintf(ctx->memory, "%% Reading Root dictionary\n");

    code = pdfi_dict_get(ctx, ctx->Trailer, "Root", &o1);
    if (code < 0)
        return code;

    if (o1->type == PDF_INDIRECT) {
        code = pdfi_dereference(ctx, ((pdf_indirect_ref *)o1)->ref_object_num,  ((pdf_indirect_ref *)o1)->ref_generation_num, &o);
        pdfi_countdown(o1);
        if (code < 0)
            return code;

        if (o->type != PDF_DICT) {
            pdfi_countdown(o);
            return_error(gs_error_typecheck);
        }

        code = pdfi_dict_put(ctx, ctx->Trailer, "Root", o);
        if (code < 0) {
            pdfi_countdown(o);
            return code;
        }
        o1 = o;
    } else {
        if (o1->type != PDF_DICT) {
            pdfi_countdown(o1);
            return_error(gs_error_typecheck);
        }
    }

    code = pdfi_dict_get_type(ctx, (pdf_dict *)o1, "Type", PDF_NAME, &o);
    if (code < 0) {
        pdfi_countdown(o1);
        return code;
    }
    if (pdfi_name_strcmp((pdf_name *)o, "Catalog") != 0){
        pdfi_countdown(o);
        pdfi_countdown(o1);
        return_error(gs_error_syntaxerror);
    }
    pdfi_countdown(o);

    if (ctx->pdfdebug)
        dmprintf(ctx->memory, "\n");
    /* We don't pdfi_countdown(o1) now, because we've transferred our
     * reference to the pointer in the pdf_context structure.
     */
    pdfi_countdown(ctx->Root); /* If file was repaired it might be set already */
    ctx->Root = (pdf_dict *)o1;
    return 0;
}

int pdfi_read_Info(pdf_context *ctx)
{
    pdf_obj *o, *o1;
    int code;

    if (ctx->pdfdebug)
        dmprintf(ctx->memory, "%% Reading Info dictionary\n");

    code = pdfi_dict_get(ctx, ctx->Trailer, "Info", &o1);
    if (code < 0)
        return code;

    if (o1->type == PDF_INDIRECT) {
        code = pdfi_dereference(ctx, ((pdf_indirect_ref *)o1)->ref_object_num,  ((pdf_indirect_ref *)o1)->ref_generation_num, &o);
        pdfi_countdown(o1);
        if (code < 0)
            return code;

        if (o->type != PDF_DICT) {
            pdfi_countdown(o);
            return_error(gs_error_typecheck);
        }

        code = pdfi_dict_put(ctx, ctx->Trailer, "Info", o);
        if (code < 0) {
            pdfi_countdown(o);
            return code;
        }
        o1 = o;
    } else {
        if (o1->type != PDF_DICT) {
            pdfi_countdown(o1);
            return_error(gs_error_typecheck);
        }
    }

    if (ctx->pdfdebug)
        dmprintf(ctx->memory, "\n");
    /* We don't pdfi_countdown(o1) now, because we've transferred our
     * reference to the pointer in the pdf_context structure.
     */
    ctx->Info = (pdf_dict *)o1;
    return 0;
}

int pdfi_read_Pages(pdf_context *ctx)
{
    pdf_obj *o, *o1;
    int code;
    double d;

    if (ctx->pdfdebug)
        dmprintf(ctx->memory, "%% Reading Pages dictionary\n");

    code = pdfi_dict_get(ctx, ctx->Root, "Pages", &o1);
    if (code < 0)
        return code;

    if (o1->type == PDF_INDIRECT) {
        code = pdfi_dereference(ctx, ((pdf_indirect_ref *)o1)->ref_object_num,  ((pdf_indirect_ref *)o1)->ref_generation_num, &o);
        pdfi_countdown(o1);
        if (code < 0)
            return code;

        if (o->type != PDF_DICT) {
            pdfi_countdown(o);
            ctx->pdf_errors |= E_PDF_BADPAGEDICT;
            dmprintf(ctx->memory, "*** Error: Something is wrong with the Pages dictionary.  Giving up.\n");
            if (o->type == PDF_INDIRECT)
                dmprintf(ctx->memory, "           Double indirect reference.  Loop in Pages tree?\n");
            return_error(gs_error_typecheck);
        }

        code = pdfi_dict_put(ctx, ctx->Root, "Pages", o);
        if (code < 0) {
            pdfi_countdown(o);
            return code;
        }
        o1 = o;
    } else {
        if (o1->type != PDF_DICT) {
            pdfi_countdown(o1);
            return_error(gs_error_typecheck);
        }
    }

    if (ctx->pdfdebug)
        dmprintf(ctx->memory, "\n");

    /* Acrobat allows the Pages Count to be a flaoting point nuber (!) */
    code = pdfi_dict_get_number(ctx, (pdf_dict *)o1, "Count", &d);
    if (code < 0)
        return code;

    if (floor(d) != d) {
        pdfi_countdown(o1);
        return_error(gs_error_rangecheck);
    } else {
        ctx->num_pages = (int)floor(d);
    }

    /* We don't pdfi_countdown(o1) now, because we've transferred our
     * reference to the pointer in the pdf_context structure.
     */
    ctx->Pages = (pdf_dict *)o1;
    return 0;
}

/* Read optional things in from Root */
void
pdfi_read_OptionalRoot(pdf_context *ctx)
{
    pdf_obj *obj = NULL;
    int code;

    if (ctx->pdfdebug)
        dmprintf(ctx->memory, "%% Reading other Root contents\n");

    if (ctx->pdfdebug)
        dmprintf(ctx->memory, "%% OCProperties\n");
    code = pdfi_dict_get_type(ctx, ctx->Root, "OCProperties", PDF_DICT, &obj);
    if (code == 0) {
        ctx->OCProperties = (pdf_dict *)obj;
    } else {
        ctx->OCProperties = NULL;
        if (ctx->pdfdebug)
            dmprintf(ctx->memory, "%% (None)\n");
    }

}

void
pdfi_free_OptionalRoot(pdf_context *ctx)
{
    if (ctx->OCProperties) {
        pdfi_countdown(ctx->OCProperties);
    }
}

/* Handle child node processing for page_dict */
static int
pdfi_get_child(pdf_context *ctx, pdf_array *Kids, int i, pdf_dict **pchild)
{
    pdf_indirect_ref *node = NULL;
    pdf_dict *child = NULL;
    pdf_name *Type = NULL;
    pdf_dict *leaf_dict = NULL;
    pdf_name *Key = NULL;
    int code = 0;

    code = pdfi_array_get_no_deref(ctx, Kids, i, (pdf_obj **)&node);
    if (code < 0)
        goto errorExit;

    if (node->type != PDF_INDIRECT && node->type != PDF_DICT) {
        code = gs_note_error(gs_error_typecheck);
        goto errorExit;
    }

    if (node->type == PDF_INDIRECT) {
        code = pdfi_dereference(ctx, node->ref_object_num, node->ref_generation_num,
                                (pdf_obj **)&child);
        if (code < 0) {
            int code1 = pdfi_repair_file(ctx);
            if (code1 < 0)
                goto errorExit;
            code = pdfi_dereference(ctx, node->ref_object_num,
                                    node->ref_generation_num, (pdf_obj **)&child);
            if (code < 0)
                goto errorExit;
        }
        if (child->type != PDF_DICT) {
            code = gs_note_error(gs_error_typecheck);
            goto errorExit;
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
        code = pdfi_dict_get_type(ctx, child, "Type", PDF_NAME, (pdf_obj **)&Type);
        if (code < 0)
            goto errorExit;
        if (pdfi_name_is(Type, "Pages")) {
            code = pdfi_array_put(ctx, Kids, i, (pdf_obj *)child);
            if (code < 0)
                goto errorExit;
        } else {
            /* Bizarrely, one of the QL FTS files (FTS_07_0704.pdf) has a page diciotnary with a /Type of /Template */
            if (!pdfi_name_is(Type, "Page"))
                ctx->pdf_errors |= E_PDF_BADPAGETYPE;
            /* Make a 'PageRef' entry (just stores an indirect reference to the actual page)
             * and store that in the Kids array for future reference. But pass on the
             * dereferenced Page dictionary, in case this is the target page.
             */

            code = pdfi_alloc_object(ctx, PDF_DICT, 0, (pdf_obj **)&leaf_dict);
            if (code < 0)
                goto errorExit;
            code = pdfi_make_name(ctx, (byte *)"PageRef", 7, (pdf_obj **)&Key);
            if (code < 0)
                goto errorExit;
            code = pdfi_dict_put_obj(leaf_dict, (pdf_obj *)Key, (pdf_obj *)node);
            if (code < 0)
                goto errorExit;
            code = pdfi_dict_put(ctx, leaf_dict, "Type", (pdf_obj *)Key);
            if (code < 0)
                goto errorExit;
            code = pdfi_array_put(ctx, Kids, i, (pdf_obj *)leaf_dict);
            if (code < 0)
                goto errorExit;
        }
    } else {
        child = (pdf_dict *)node;
        pdfi_countup(child);
    }

    *pchild = child;
    child = NULL;

 errorExit:
    pdfi_countdown(child);
    pdfi_countdown(node);
    pdfi_countdown(Type);
    pdfi_countdown(Key);
    return code;
}

/* Check if key is in the dictionary, and if so, copy it into the inheritable dict.
 */
static int
pdfi_check_inherited_key(pdf_context *ctx, pdf_dict *d, const char *keyname, pdf_dict *inheritable)
{
    int code = 0;
    pdf_obj *object = NULL;
    bool known;

    /* Check for inheritable keys, if we find any copy them to the 'inheritable' dictionary at this level */
    code = pdfi_dict_known(d, keyname, &known);
    if (code < 0)
        goto exit;
    if (known) {
        code = pdfi_loop_detector_mark(ctx);
        if (code < 0){
            goto exit;
        }
        code = pdfi_dict_get(ctx, d, keyname, &object);
        if (code < 0) {
            (void)pdfi_loop_detector_cleartomark(ctx);
            goto exit;
        }
        code = pdfi_loop_detector_cleartomark(ctx);
        if (code < 0) {
            goto exit;
        }
        code = pdfi_dict_put(ctx, inheritable, keyname, object);
    }

 exit:
    pdfi_countdown(object);
    return code;
}

int
pdfi_get_page_dict(pdf_context *ctx, pdf_dict *d, uint64_t page_num, uint64_t *page_offset,
                   pdf_dict **target, pdf_dict *inherited)
{
    int i, code = 0;
    pdf_array *Kids = NULL;
    pdf_dict *child = NULL;
    pdf_name *Type = NULL;
    pdf_dict *inheritable = NULL;
    int64_t num;
    double dbl;

    if (ctx->pdfdebug)
        dmprintf1(ctx->memory, "%% Finding page dictionary for page %"PRIi64"\n", page_num + 1);

    /* Allocated inheritable dict (it might stay empty) */
    code = pdfi_alloc_object(ctx, PDF_DICT, 0, (pdf_obj **)&inheritable);
    if (code < 0)
        return code;
    pdfi_countup(inheritable);

    /* if we are being passed any inherited values from our parent, copy them now */
    if (inherited != NULL) {
        code = pdfi_dict_copy(inheritable, inherited);
        if (code < 0)
            goto exit;
    }

    code = pdfi_dict_get_number(ctx, d, "Count", &dbl);
    if (code < 0)
        goto exit;
    if (dbl != floor(dbl)) {
        code = gs_note_error(gs_error_rangecheck);
        goto exit;
    }
    num = (int)dbl;

    if (num < 0 || (num + *page_offset) > ctx->num_pages) {
        code = gs_note_error(gs_error_rangecheck);
        goto exit;
    }
    if (num + *page_offset < page_num) {
        *page_offset += num;
        code = 1;
        goto exit;
    }
    /* The requested page is a descendant of this node */

    /* Check for inheritable keys, if we find any copy them to the 'inheritable' dictionary at this level */
    code = pdfi_check_inherited_key(ctx, d, "Resources", inheritable);
    if (code < 0)
        goto exit;
    code = pdfi_check_inherited_key(ctx, d, "MediaBox", inheritable);
    if (code < 0)
        goto exit;
    code = pdfi_check_inherited_key(ctx, d, "CropBox", inheritable);
    if (code < 0)
        goto exit;
    code = pdfi_check_inherited_key(ctx, d, "Rotate", inheritable);
    if (code < 0) {
        goto exit;
    }

    /* Get the Kids array */
    code = pdfi_dict_get_type(ctx, d, "Kids", PDF_ARRAY, (pdf_obj **)&Kids);
    if (code < 0) {
        goto exit;
    }

    /* Check each entry in the Kids array */
    for (i = 0;i < pdfi_array_size(Kids);i++) {
        pdfi_countdown(child);
        child = NULL;
        pdfi_countdown(Type);
        Type = NULL;

        code = pdfi_get_child(ctx, Kids, i, &child);
        if (code < 0) {
            goto exit;
        }

        /* Check the type, if its a Pages entry, then recurse. If its a Page entry, is it the one we want */
        code = pdfi_dict_get_type(ctx, child, "Type", PDF_NAME, (pdf_obj **)&Type);
        if (code == 0) {
            if (pdfi_name_is(Type, "Pages")) {
                code = pdfi_dict_get_number(ctx, child, "Count", &dbl);
                if (code == 0) {
                    if (dbl != floor(dbl)) {
                        code = gs_note_error(gs_error_rangecheck);
                        goto exit;
                    }
                    num = (int)dbl;
                    if (num < 0 || (num + *page_offset) > ctx->num_pages) {
                        code = gs_note_error(gs_error_rangecheck);
                        goto exit;
                    } else {
                        if (num + *page_offset <= page_num) {
                            *page_offset += num;
                        } else {
                            code = pdfi_get_page_dict(ctx, child, page_num, page_offset, target, inheritable);
                            goto exit;
                        }
                    }
                }
            } else {
                if (pdfi_name_is(Type, "PageRef")) {
                    if ((*page_offset) == page_num) {
                        pdf_dict *d = NULL;

                        code = pdfi_dict_get(ctx, child, "PageRef", (pdf_obj **)&d);
                        if (code < 0)
                            goto exit;
                        code = pdfi_merge_dicts(d, inheritable);
                        *target = d;
                        pdfi_countup(*target);
                        pdfi_countdown(d);
                        goto exit;
                    } else {
                        *page_offset += 1;
                    }
                } else {
                    if (!pdfi_name_is(Type, "Page"))
                        ctx->pdf_errors |= E_PDF_BADPAGETYPE;
                    if ((*page_offset) == page_num) {
                        code = pdfi_merge_dicts(child, inheritable);
                        *target = child;
                        pdfi_countup(*target);
                        goto exit;
                    } else {
                        *page_offset += 1;
                    }
                }
            }
        }
        if (code < 0)
            goto exit;
    }
    /* Positive return value indicates we did not find the target below this node, try the next one */
    code = 1;

 exit:
    pdfi_countdown(inheritable);
    pdfi_countdown(Kids);
    pdfi_countdown(child);
    pdfi_countdown(Type);
    return code;
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
static int pdfi_interpret_stream_operator(pdf_context *ctx, pdf_stream *source, pdf_dict *stream_dict, pdf_dict *page_dict);

static int search_table_3(pdf_context *ctx, unsigned char *str, pdf_keyword **key)
{
    int i, code = 0;

    for (i = 0; i < 5; i++) {
        if (memcmp(str, op_table_3[i], 3) == 0) {
            code = pdfi_alloc_object(ctx, PDF_KEYWORD, 3, (pdf_obj **)key);
            if (code < 0)
                return code;
            memcpy((*key)->data, str, 3);
            (*key)->key = PDF_NOT_A_KEYWORD;
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
            code = pdfi_alloc_object(ctx, PDF_KEYWORD, 2, (pdf_obj **)key);
            if (code < 0)
                return code;
            memcpy((*key)->data, str, 2);
            (*key)->key = PDF_NOT_A_KEYWORD;
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
            code = pdfi_alloc_object(ctx, PDF_KEYWORD, 1, (pdf_obj **)key);
            if (code < 0)
                return code;
            memcpy((*key)->data, str, 1);
            (*key)->key = PDF_NOT_A_KEYWORD;
            pdfi_countup(*key);
            return 1;
        }
    }
    return 0;
}

static int split_bogus_operator(pdf_context *ctx, pdf_stream *source, pdf_dict *stream_dict, pdf_dict *page_dict)
{
    int code = 0;
    pdf_keyword *keyword = (pdf_keyword *)ctx->stack_top[-1], *key1 = NULL, *key2 = NULL;

    if (keyword->length > 6) {
        /* Longer than 2 3-character operators, we only allow for up to two
         * operators. Check to see if it includes an endstream or endobj.
         */
        if (memcmp(&keyword->data[keyword->length - 6], "endobj", 6) == 0) {
            code = pdfi_alloc_object(ctx, PDF_KEYWORD, keyword->length - 6, (pdf_obj **)&key1);
            if (code < 0)
                goto error_exit;
            memcpy(key1->data, keyword->data, key1->length);
            pdfi_pop(ctx, 1);
            pdfi_push(ctx, (pdf_obj *)key1);
            code = pdfi_interpret_stream_operator(ctx, source, stream_dict, page_dict);
            if (code < 0)
                goto error_exit;
            code = pdfi_alloc_object(ctx, PDF_KEYWORD, 6, (pdf_obj **)&key1);
            if (code < 0)
                goto error_exit;
            memcpy(key1->data, "endobj", 6);
            key1->key = PDF_ENDOBJ;
            pdfi_push(ctx, (pdf_obj *)key1);
            return 0;
        } else {
            if (keyword->length > 9 && memcmp(&keyword->data[keyword->length - 9], "endstream", 9) == 0) {
                code = pdfi_alloc_object(ctx, PDF_KEYWORD, keyword->length - 9, (pdf_obj **)&key1);
                if (code < 0)
                    goto error_exit;
                memcpy(key1->data, keyword->data, key1->length);
                pdfi_pop(ctx, 1);
                pdfi_push(ctx, (pdf_obj *)key1);
                code = pdfi_interpret_stream_operator(ctx, source, stream_dict, page_dict);
                if (code < 0)
                    goto error_exit;
                code = pdfi_alloc_object(ctx, PDF_KEYWORD, 9, (pdf_obj **)&key1);
                if (code < 0)
                    goto error_exit;
                memcpy(key1->data, "endstream", 9);
                key1->key = PDF_ENDSTREAM;
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
        if (code <= 0)
            goto error_exit;
    }

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
        if (code <= 0)
            goto error_exit;
    }

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
    }

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
    ctx->pdf_errors |= E_PDF_TOKENERROR;
    pdfi_countdown(key1);
    pdfi_countdown(key2);
    pdfi_clearstack(ctx);
    return code;
}

#define K1(a) (a)
#define K2(a, b) ((a << 8) + b)
#define K3(a, b, c) ((a << 16) + (b << 8) + c)

static int pdfi_interpret_stream_operator(pdf_context *ctx, pdf_stream *source, pdf_dict *stream_dict, pdf_dict *page_dict)
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
            if (keyword->key != PDF_NOT_A_KEYWORD)
                return gs_error_repaired_keyword;
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
                    if (keyword->key != PDF_NOT_A_KEYWORD)
                        return gs_error_repaired_keyword;
                }
                break;
       }
    }
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
        ctx->pdf_warnings |= W_PDF_GROUPERROR;
        while (ctx->current_stream_save.group_depth > local_save->group_depth)
            pdfi_trans_end_group(ctx);
    }
    if (ctx->pgs->level > ctx->current_stream_save.gsave_level)
        ctx->pdf_warnings |= W_PDF_TOOMANYq;
    if (pdfi_count_stack(ctx) > ctx->current_stream_save.stack_count)
        ctx->pdf_warnings |= W_PDF_STACKGARBAGE;
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
int pdfi_run_context(pdf_context *ctx, pdf_dict *stream_dict,
                     pdf_dict *page_dict, bool stoponerror, const char *desc)
{
    int code;
    gs_gstate *DefaultQState;

#if DEBUG_CONTEXT
    dbgmprintf(ctx->memory, "pdfi_run_context BEGIN\n");
#endif
    pdfi_copy_DefaultQState(ctx, &DefaultQState);
    pdfi_set_DefaultQState(ctx, ctx->pgs);
    code = pdfi_interpret_inner_content_stream(ctx, stream_dict, page_dict, stoponerror, desc);
    pdfi_restore_DefaultQState(ctx, &DefaultQState);
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
pdfi_interpret_inner_content(pdf_context *ctx, pdf_stream *content_stream, pdf_dict *stream_dict,
                             pdf_dict *page_dict, bool stoponerror, const char *desc)
{
    int code = 0;
    bool saved_stoponerror = ctx->pdfstoponerror;
    stream_save local_entry_save;

    local_save_stream_state(ctx, &local_entry_save);
    initialise_stream_save(ctx);

    /* Stop on error in substream, and also be prepared to clean up the stack */
    if (stoponerror)
        ctx->pdfstoponerror = true;

#if DEBUG_CONTEXT
    dbgmprintf1(ctx->memory, "BEGIN %s stream\n", desc);
#endif
    code = pdfi_interpret_content_stream(ctx, content_stream, stream_dict, page_dict);
#if DEBUG_CONTEXT
    dbgmprintf1(ctx->memory, "END %s stream\n", desc);
#endif

    if (code < 0)
        dbgmprintf1(ctx->memory, "ERROR: inner_stream: code %d when rendering stream\n", code);

    ctx->pdfstoponerror = saved_stoponerror;

    /* Put our state back the way it was on entry */
#if PROBE_STREAMS
    if (ctx->pgs->level > ctx->current_stream_save.gsave_level ||
        pdfi_count_stack(ctx) > ctx->current_stream_save.stack_count)
        code = ((pdf_context *)0)->first_page;
#endif

    cleanup_context_interpretation(ctx, &local_entry_save);
    local_restore_stream_state(ctx, &local_entry_save);
    if (!ctx->pdfstoponerror)
        code = 0;
    return code;
}

/* Interpret inner content from a string
 */
int
pdfi_interpret_inner_content_string(pdf_context *ctx, pdf_string *content_string,
                                    pdf_dict *stream_dict, pdf_dict *page_dict,
                                    bool stoponerror, const char *desc)
{
    int code = 0;
    pdf_stream *stream = NULL;

    code = pdfi_open_memory_stream_from_memory(ctx, content_string->length,
                                               content_string->data, &stream);
    if (code < 0) goto exit;

    /* NOTE: stream gets closed in here */
    code = pdfi_interpret_inner_content(ctx, stream, stream_dict, page_dict, stoponerror, desc);
 exit:
    return code;
}

/* Interpret inner content from a stream_dict
 */
int
pdfi_interpret_inner_content_stream(pdf_context *ctx, pdf_dict *stream_dict,
                                    pdf_dict *page_dict, bool stoponerror, const char *desc)
{
    return pdfi_interpret_inner_content(ctx, NULL, stream_dict, page_dict, stoponerror, desc);
}

/*
 * Interpret a content stream.
 * content_stream -- content to parse.  If NULL, get it from the stream_dict
 * stream_dict -- dict containing the stream
 */
int
pdfi_interpret_content_stream(pdf_context *ctx, pdf_stream *content_stream,
                              pdf_dict *stream_dict, pdf_dict *page_dict)
{
    int code;
    pdf_stream *stream;
    pdf_keyword *keyword;

    if (content_stream != NULL) {
        stream = content_stream;
    } else {
        code = pdfi_seek(ctx, ctx->main_stream, stream_dict->stream_offset, SEEK_SET);
        if (code < 0)
            return code;

        code = pdfi_filter(ctx, stream_dict, ctx->main_stream, &stream, false);
        if (code < 0)
            return code;
    }

    do {
        code = pdfi_read_token(ctx, stream, stream_dict->object_num, stream_dict->generation_num);
        if (code < 0) {
            if (code == gs_error_ioerror || code == gs_error_VMerror || ctx->pdfstoponerror) {
                if (code == gs_error_ioerror) {
                    ctx->pdf_errors |= E_PDF_BADSTREAM;
                    dmprintf(ctx->memory, "**** Error reading a content stream.  The page may be incomplete.\n");
                } else if (code == gs_error_VMerror) {
                    ctx->pdf_errors |= E_PDF_OUTOFMEMORY;
                    dmprintf(ctx->memory, "**** Error ran out of memory reading a content stream.  The page may be incomplete.\n");
                }
                pdfi_close_file(ctx, stream);
                return code;
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
                case PDF_ENDSTREAM:
                    pdfi_close_file(ctx, stream);
                    pdfi_pop(ctx,1);
                    return 0;
                    break;
                case PDF_ENDOBJ:
                    pdfi_close_file(ctx, stream);
                    pdfi_clearstack(ctx);
                    ctx->pdf_errors |= E_PDF_MISSINGENDSTREAM;
                    if (ctx->pdfstoponerror)
                        return_error(gs_error_syntaxerror);
                    return 0;
                    break;
                case PDF_NOT_A_KEYWORD:
                    code = pdfi_interpret_stream_operator(ctx, stream, stream_dict, page_dict);
                    if (code < 0) {
                        if (code == gs_error_repaired_keyword)
                            goto repaired_keyword;

                        ctx->pdf_errors |= E_PDF_TOKENERROR;
                        if (ctx->pdfstoponerror) {
                            pdfi_close_file(ctx, stream);
                            pdfi_clearstack(ctx);
                            return code;
                        }
                    }
                    break;
                case PDF_INVALID_KEY:
                    ctx->pdf_errors |= E_PDF_KEYWORDTOOLONG;
                    pdfi_clearstack(ctx);
                    break;
                default:
                    ctx->pdf_errors |= E_PDF_MISSINGENDSTREAM;
                    pdfi_close_file(ctx, stream);
                    pdfi_clearstack(ctx);
                    return_error(gs_error_typecheck);
                    break;
            }
        }
        if(stream->eof == true)
            break;
    }while(1);

    pdfi_close_file(ctx, stream);
    return 0;
}

/*
 * Checks for both "Resource" and "RD" in the specified dict.
 * And then gets the typedict of Type (e.g. Font or XObject).
 * Returns 0 if undefined, >0 if found, <0 if error
 */
static int pdfi_resource_knownget_typedict(pdf_context *ctx, unsigned char *Type,
                                           pdf_dict *dict, pdf_dict **typedict)
{
    int code;
    pdf_dict *Resources = NULL;

    code = pdfi_dict_knownget_type(ctx, dict, "Resources", PDF_DICT, (pdf_obj **)&Resources);
    if (code == 0)
        code = pdfi_dict_knownget_type(ctx, dict, "DR", PDF_DICT, (pdf_obj **)&Resources);
    if (code < 0) goto exit;
    if (code > 0)
        code = pdfi_dict_knownget_type(ctx, Resources, (const char *)Type, PDF_DICT, (pdf_obj **)typedict);
 exit:
    pdfi_countdown(Resources);
    return code;
}

int pdfi_find_resource(pdf_context *ctx, unsigned char *Type, pdf_name *name,
                       pdf_dict *dict, pdf_dict *page_dict, pdf_obj **o)
{
    pdf_dict *typedict = NULL;
    pdf_dict *Parent = NULL;
    int code;

    *o = NULL;

    /* Check the provided dict */
    code = pdfi_resource_knownget_typedict(ctx, Type, dict, &typedict);
    if (code < 0) goto exit;
    if (code > 0) {
        code = pdfi_dict_get_no_store_R(ctx, typedict, name, o);
        if (code != gs_error_undefined)
            goto exit;
    }

    /* Check the Parents, if any */
    code = pdfi_dict_knownget_type(ctx, dict, "Parent", PDF_DICT, (pdf_obj **)&Parent);
    if (code < 0) goto exit;
    if (code > 0) {
        if (Parent->object_num != ctx->CurrentPageDict->object_num) {
            code = pdfi_find_resource(ctx, Type, name, Parent, page_dict, o);
            if (code != gs_error_undefined)
                goto exit;
        }
    }

    pdfi_countdown(typedict);
    typedict = NULL;

    /* Normally page_dict can't be (or shouldn't be) NULL. However, if we are processing
     * a TYpe 3 font, then the 'page dict' is the Resources dictionary of that font. If
     * the font inherits Resources from its page (which it should not) then its possible
     * that the 'page dict' could be NULL here. We need to guard against that. Its possible
     * there may be other, similar, cases (eg Patterns within Patterns). In addition we
     * do need to be able to check the real page dictionary for inhereited resources, and
     * in the case of a type 3 font BuildChar at least there is no easy way to do that.
     * So we'll store the page dictionary for the current page in the context as a
     * last-ditch resource to check.
     */
    if (page_dict != NULL) {
        code = pdfi_resource_knownget_typedict(ctx, Type, page_dict, &typedict);
        if (code < 0) goto exit;

        if (code > 0) {
            code = pdfi_dict_get_no_store_R(ctx, typedict, name, o);
            goto exit;
        }
    }

    pdfi_countdown(typedict);
    typedict = NULL;

    if (ctx->CurrentPageDict != NULL) {
        code = pdfi_resource_knownget_typedict(ctx, Type, ctx->CurrentPageDict, &typedict);
        if (code < 0) goto exit;

        if (code > 0) {
            code = pdfi_dict_get_no_store_R(ctx, typedict, name, o);
            goto exit;
        }
    }

    /* If we got all the way down there, we didn't find it */
    code = gs_error_undefined;

exit:
    pdfi_countdown(typedict);
    pdfi_countdown(Parent);
    return code;
}
