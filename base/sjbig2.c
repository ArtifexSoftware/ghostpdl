/* Copyright (C) 2001-2019 Artifex Software, Inc.
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


/* jbig2decode filter implementation -- hooks in libjbig2dec */

#include "stdint_.h"
#include "memory_.h"
#include "stdio_.h" /* sprintf() for debug output */

#include "gserrors.h"
#include "gdebug.h"
#include "strimpl.h"
#include "sjbig2.h"
#include <limits.h>                     /* UINT_MAX */

/* stream implementation */

/* The /JBIG2Decode filter is a fairly memory intensive one to begin with,
   particularly in the initial jbig2dec library implementation. Furthermore,
   as a PDF 1.4 feature, we can assume a fairly large (host-level) machine.
   We therefore dispense with the normal Ghostscript memory discipline and
   let the library allocate all its resources on the heap. The pointers to
   these are not enumerated and so will not be garbage collected. We rely
   on our release() proc being called to deallocate state.
 */

private_st_jbig2decode_state();	/* creates a gc object for our state, defined in sjbig2.h */

/* error callback for jbig2 decoder */
static void
s_jbig2decode_error(void *callback_data, const char *msg, Jbig2Severity severity,
               int32_t seg_idx)
{
    s_jbig2_callback_data_t *error_data = (s_jbig2_callback_data_t *)callback_data;
    const char *type;
    char segment[22];

    switch (severity) {
        case JBIG2_SEVERITY_DEBUG:
            type = "DEBUG"; break;;
        case JBIG2_SEVERITY_INFO:
            type = "info"; break;;
        case JBIG2_SEVERITY_WARNING:
            type = "WARNING"; break;;
        case JBIG2_SEVERITY_FATAL:
            type = "FATAL ERROR decoding image:";
            /* pass the fatal error upstream if possible */
            if (error_data != NULL) error_data->error = gs_error_ioerror;
            break;;
        default: type = "unknown message:"; break;;
    }
    if (seg_idx == -1) segment[0] = '\0';
    else gs_sprintf(segment, "(segment 0x%02x)", seg_idx);

    if (error_data)
    {
        char *message;
        int len;

        len = snprintf(NULL, 0, "jbig2dec %s %s %s", type, msg, segment);
        if (len < 0)
            return;

        message = (char *)gs_alloc_bytes(error_data->memory, len + 1, "sjbig2decode_error(message)");
        if (message == NULL)
            return;

        len = snprintf(message, len + 1, "jbig2dec %s %s %s", type, msg, segment);
        if (len < 0)
        {
            gs_free_object(error_data->memory, message, "s_jbig2decode_error(message)");
            return;
        }

        if (error_data->last_message != NULL && strcmp(message, error_data->last_message)) {
            if (error_data->repeats > 1)
            {
                if (error_data->severity == JBIG2_SEVERITY_FATAL || error_data->severity == JBIG2_SEVERITY_WARNING) {
                    dmlprintf1(error_data->memory, "jbig2dec last message repeated %ld times\n", error_data->repeats);
                } else {
                    if_debug1m('w', error_data->memory, "[w] jbig2dec last message repeated %ld times\n", error_data->repeats);
                }
            }
            gs_free_object(error_data->memory, error_data->last_message, "s_jbig2decode_error(last_message)");
            if (severity == JBIG2_SEVERITY_FATAL || severity == JBIG2_SEVERITY_WARNING) {
                dmlprintf1(error_data->memory, "%s\n", message);
            } else {
                if_debug1m('w', error_data->memory, "[w] %s\n", message);
            }
            error_data->last_message = message;
            error_data->severity = severity;
            error_data->type = type;
            error_data->repeats = 0;
        }
        else if (error_data->last_message != NULL) {
            error_data->repeats++;
            if (error_data->repeats % 1000000 == 0)
            {
                if (error_data->severity == JBIG2_SEVERITY_FATAL || error_data->severity == JBIG2_SEVERITY_WARNING) {
                    dmlprintf1(error_data->memory, "jbig2dec last message repeated %ld times so far\n", error_data->repeats);
                } else {
                    if_debug1m('w', error_data->memory, "[w] jbig2dec last message repeated %ld times so far\n", error_data->repeats);
                }
            }
            gs_free_object(error_data->memory, message, "s_jbig2decode_error(message)");
        }
        else if (error_data->last_message == NULL) {
            if (severity == JBIG2_SEVERITY_FATAL || severity == JBIG2_SEVERITY_WARNING) {
                dmlprintf1(error_data->memory, "%s\n", message);
            } else {
                if_debug1m('w', error_data->memory, "[w] %s\n", message);
            }
            error_data->last_message = message;
            error_data->severity = severity;
            error_data->type = type;
            error_data->repeats = 0;
        }
    }
    else
    {
/*
        FIXME s_jbig2_callback_data_t should be updated so that jbig2_ctx_new is not called
        with a NULL argument (see jbig2.h) and we never reach here with a NULL state
*/
        if (severity == JBIG2_SEVERITY_FATAL) {
            dlprintf3("jbig2dec %s %s %s\n", type, msg, segment);
        } else {
            if_debug3('w', "[w] jbig2dec %s %s %s\n", type, msg, segment);
        }
    }
}

static void
s_jbig2decode_flush_errors(void *callback_data)
{
    s_jbig2_callback_data_t *error_data = (s_jbig2_callback_data_t *)callback_data;

    if (error_data == NULL)
        return;

    if (error_data->last_message != NULL) {
        if (error_data->repeats > 1)
        {
            if (error_data->severity == JBIG2_SEVERITY_FATAL || error_data->severity == JBIG2_SEVERITY_WARNING) {
                dmlprintf1(error_data->memory, "jbig2dec last message repeated %ld times\n", error_data->repeats);
            } else {
                if_debug1m('w', error_data->memory, "[w] jbig2dec last message repeated %ld times\n", error_data->repeats);
            }
        }
        gs_free_object(error_data->memory, error_data->last_message, "s_jbig2decode_error(last_message)");
        error_data->last_message = NULL;
        error_data->repeats = 0;
    }
}

/* invert the bits in a buffer */
/* jbig2 and postscript have different senses of what pixel
   value is black, so we must invert the image */
static void
s_jbig2decode_invert_buffer(unsigned char *buf, int length)
{
    int i;

    for (i = 0; i < length; i++)
        *buf++ ^= 0xFF;
}

typedef struct {
        Jbig2Allocator allocator;
        gs_memory_t *mem;
} s_jbig2decode_allocator_t;

static void *s_jbig2decode_alloc(Jbig2Allocator *_allocator, size_t size)
{
        s_jbig2decode_allocator_t *allocator = (s_jbig2decode_allocator_t *) _allocator;
        if (size > UINT_MAX)
            return NULL;
        return gs_alloc_bytes(allocator->mem, size, "s_jbig2decode_alloc");
}

static void s_jbig2decode_free(Jbig2Allocator *_allocator, void *p)
{
        s_jbig2decode_allocator_t *allocator = (s_jbig2decode_allocator_t *) _allocator;
        gs_free_object(allocator->mem, p, "s_jbig2decode_free");
}

static void *s_jbig2decode_realloc(Jbig2Allocator *_allocator, void *p, size_t size)
{
        s_jbig2decode_allocator_t *allocator = (s_jbig2decode_allocator_t *) _allocator;
        if (size > UINT_MAX)
            return NULL;
        return gs_resize_object(allocator->mem, p, size, "s_jbig2decode_realloc");
}

/* parse a globals stream packed into a gs_bytestring for us by the postscript
   layer and stuff the resulting context into a pointer for use in later decoding */
int
s_jbig2decode_make_global_data(gs_memory_t *mem, byte *data, uint length, void **result)
{
    Jbig2Ctx *ctx = NULL;
    int code;
    s_jbig2decode_allocator_t *allocator;

    /* the cvision encoder likes to include empty global streams */
    if (length == 0) {
        if_debug0('w', "[w] ignoring zero-length jbig2 global stream.\n");
        *result = NULL;
        return 0;
    }

    allocator = (s_jbig2decode_allocator_t *) gs_alloc_bytes(mem,
            sizeof (s_jbig2decode_allocator_t), "s_jbig2_make_global_data");
    if (allocator == NULL) {
        *result = NULL;
        return_error(gs_error_VMerror);
    }

    allocator->allocator.alloc = s_jbig2decode_alloc;
    allocator->allocator.free = s_jbig2decode_free;
    allocator->allocator.realloc = s_jbig2decode_realloc;
    allocator->mem = mem;

    /* allocate a context with which to parse our global segments */
    ctx = jbig2_ctx_new((Jbig2Allocator *) allocator, JBIG2_OPTIONS_EMBEDDED,
                            NULL, s_jbig2decode_error, NULL);
    if (ctx == NULL) {
        gs_free_object(mem, allocator, "s_jbig2_make_global_data");
        return_error(gs_error_VMerror);
    }

    /* parse the global bitstream */
    code = jbig2_data_in(ctx, data, length);
    if (code) {
        /* error parsing the global stream */
        allocator = (s_jbig2decode_allocator_t *) jbig2_ctx_free(ctx);
        gs_free_object(allocator->mem, allocator, "s_jbig2_make_global_data");
        *result = NULL;
        return_error(gs_error_ioerror);
    }

    /* canonize and store our global state */
    *result = jbig2_make_global_ctx(ctx);

    return 0; /* todo: check for allocation failure */
}

/* release a global ctx pointer */
void
s_jbig2decode_free_global_data(void *data)
{
    Jbig2GlobalCtx *global_ctx = (Jbig2GlobalCtx*)data;
    s_jbig2decode_allocator_t *allocator;

    allocator = (s_jbig2decode_allocator_t *) jbig2_global_ctx_free(global_ctx);

    gs_free_object(allocator->mem, allocator, "s_jbig2decode_free_global_data");
}

/* store a global ctx pointer in our state structure.
 * If "gd" is NULL, then this library must free the global context.
 * If not-NULL, then it will be memory managed by caller, for example,
 * garbage collected in the case of the PS interpreter.
 * Currently gpdf will use NULL, and the PDF implemented in the gs interpreter would use
 * the garbage collection.
 */
int
s_jbig2decode_set_global_data(stream_state *ss, s_jbig2_global_data_t *gd, void *global_ctx)
{
    stream_jbig2decode_state *state = (stream_jbig2decode_state*)ss;
    state->global_struct = gd;
    state->global_ctx = global_ctx;
    return 0;
}

/* initialize the steam.
   this involves allocating the context structures, and
   initializing the global context from the /JBIG2Globals object reference
 */
static int
s_jbig2decode_init(stream_state * ss)
{
    stream_jbig2decode_state *const state = (stream_jbig2decode_state *) ss;
    Jbig2GlobalCtx *global_ctx = state->global_ctx; /* may be NULL */
    int code = 0;
    s_jbig2decode_allocator_t *allocator = NULL;

    state->callback_data = (s_jbig2_callback_data_t *)gs_alloc_bytes(
                                                ss->memory->non_gc_memory,
                                                sizeof(s_jbig2_callback_data_t),
						"s_jbig2decode_init(callback_data)");
    if (state->callback_data) {
        state->callback_data->memory = ss->memory->non_gc_memory;
        state->callback_data->error = 0;
        state->callback_data->last_message = NULL;
        state->callback_data->repeats = 0;

        allocator = (s_jbig2decode_allocator_t *) gs_alloc_bytes(ss->memory->non_gc_memory, sizeof (s_jbig2decode_allocator_t), "s_jbig2decode_init(allocator)");
        if (allocator == NULL) {
                s_jbig2decode_error(state->callback_data, "failed to allocate custom jbig2dec allocator", JBIG2_SEVERITY_FATAL, -1);
        }
        else {
                allocator->allocator.alloc = s_jbig2decode_alloc;
                allocator->allocator.free = s_jbig2decode_free;
                allocator->allocator.realloc = s_jbig2decode_realloc;
                allocator->mem = ss->memory->non_gc_memory;

                /* initialize the decoder with the parsed global context if any */
                state->decode_ctx = jbig2_ctx_new((Jbig2Allocator *) allocator, JBIG2_OPTIONS_EMBEDDED,
                             global_ctx, s_jbig2decode_error, state->callback_data);

                if (state->decode_ctx == NULL) {
                        gs_free_object(allocator->mem, allocator, "s_jbig2decode_release");
                }

        }

        code = state->callback_data->error;
    }
    else {
        code = gs_error_VMerror;
    }
    state->image = 0;


    return_error (code);
}

/* process a section of the input and return any decoded data.
   see strimpl.h for return codes.
 */
static int
s_jbig2decode_process(stream_state * ss, stream_cursor_read * pr,
                  stream_cursor_write * pw, bool last)
{
    stream_jbig2decode_state *const state = (stream_jbig2decode_state *) ss;
    Jbig2Image *image = state->image;
    long in_size = pr->limit - pr->ptr;
    long out_size = pw->limit - pw->ptr;
    int status = 0;

    /* there will only be a single page image,
       so pass all data in before looking for any output.
       note that the gs stream library expects offset-by-one
       indexing of the buffers, while jbig2dec uses normal 0 indexes */
    if (in_size > 0) {
        /* pass all available input to the decoder */
        jbig2_data_in(state->decode_ctx, pr->ptr + 1, in_size);
        pr->ptr += in_size;
        /* simulate end-of-page segment */
        if (last == 1) {
            jbig2_complete_page(state->decode_ctx);
        }
        /* handle fatal decoding errors reported through our callback */
        if (state->callback_data->error) return state->callback_data->error;
    }
    if (out_size > 0) {
        if (image == NULL) {
            /* see if a page image in available */
            image = jbig2_page_out(state->decode_ctx);
            if (image != NULL) {
                state->image = image;
                state->offset = 0;
            }
        }
        if (image != NULL) {
            /* copy data out of the decoded image, if any */
            long image_size = image->height*image->stride;
            long usable = min(image_size - state->offset, out_size);
            memcpy(pw->ptr + 1, image->data + state->offset, usable);
            s_jbig2decode_invert_buffer(pw->ptr + 1, usable);
            state->offset += usable;
            pw->ptr += usable;
            status = (state->offset < image_size) ? 1 : 0;
        }
    }

    return status;
}

/* stream release.
   free all our decoder state.
 */
static void
s_jbig2decode_release(stream_state *ss)
{
    stream_jbig2decode_state *const state = (stream_jbig2decode_state *) ss;

    if (state->decode_ctx) {
        s_jbig2decode_allocator_t *allocator = NULL;

        if (state->image) jbig2_release_page(state->decode_ctx, state->image);
	state->image = NULL;
        s_jbig2decode_flush_errors(state->callback_data);
        allocator = (s_jbig2decode_allocator_t *) jbig2_ctx_free(state->decode_ctx);
	state->decode_ctx = NULL;

        gs_free_object(allocator->mem, allocator, "s_jbig2decode_release");
    }
    if (state->callback_data) {
        gs_memory_t *mem = state->callback_data->memory;
        gs_free_object(state->callback_data->memory, state->callback_data->last_message, "s_jbig2decode_release(message)");
        gs_free_object(mem, state->callback_data, "s_jbig2decode_release(callback_data)");
	state->callback_data = NULL;
    }
    if (state->global_struct != NULL) {
        /* the interpreter calls jbig2decode_free_global_data() separately */
    } else {
        /* We are responsible for freeing global context */
        if (state->global_ctx) {
            s_jbig2decode_free_global_data(state->global_ctx);
            state->global_ctx = NULL;
        }
    }
}

void
s_jbig2decode_finalize(const gs_memory_t *cmem, void *vptr)
{
    (void)cmem;

    s_jbig2decode_release((stream_state *)vptr);
}

/* set stream defaults.
   this hook exists to avoid confusing the gc with bogus
   pointers. we use it similarly just to NULL all the pointers.
   (could just be done in _init?)
 */
static void
s_jbig2decode_set_defaults(stream_state *ss)
{
    stream_jbig2decode_state *const state = (stream_jbig2decode_state *) ss;

    /* state->global_ctx is not owned by us */
    state->global_struct = NULL;
    state->global_ctx = NULL;
    state->decode_ctx = NULL;
    state->image = NULL;
    state->offset = 0;
    state->callback_data = NULL;
}

/* stream template */
const stream_template s_jbig2decode_template = {
    &st_jbig2decode_state,
    s_jbig2decode_init,
    s_jbig2decode_process,
    1, 1, /* min in and out buffer sizes we can handle --should be ~32k,64k for efficiency? */
    s_jbig2decode_release,
    s_jbig2decode_set_defaults
};
