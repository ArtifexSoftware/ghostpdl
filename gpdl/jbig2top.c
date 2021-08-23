/* Copyright (C) 2019-2021 Artifex Software, Inc.
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

/* jbig2top.c */
/* Top-level API implementation of "JBIG2" Language Interface */

#include "pltop.h"
#include "gserrors.h"
#include "gxdevice.h"
#include "gsstate.h"
#include "strimpl.h"
#include "gscoord.h"
#include "gsicc_manage.h"
#include "gspaint.h"
#include "plmain.h"
#include "jbig2.h"

/* Forward decls */

/************************************************************/
/******** Language wrapper implementation (see pltop.h) *****/
/************************************************************/

typedef enum
{
    ii_state_identifying = 0,
    ii_state_jbig2,
    ii_state_jbig2_start,
    ii_state_jbig2_decode,
    ii_state_flush
} ii_state;

/*
 * JBig2 interpreter instance
 */
typedef struct jbig2_interp_instance_s {
    gs_memory_t       *memory;
    gs_memory_t       *cmemory;
    gx_device         *dev;
    gx_device         *nulldev;

    gs_color_space    *gray;

    /* JBig2 parser state machine */
    ii_state           state;

    gs_image_t         image;
    gs_image_enum     *penum;
    gs_gstate         *pgs;

    Jbig2Ctx          *jbig_ctx;
    struct _Jbig2Allocator allocator;

    byte              *samples;

} jbig2_interp_instance_t;

static int
jbig2_detect_language(const char *s, int len)
{
    const byte *hdr = (const byte *)s;
    if (len >= 8) {
        if (hdr[0] == 0x97 &&
            hdr[1] == 'J' &&
            hdr[2] == 'B' &&
            hdr[3] == '2' &&
            hdr[4] == 0x0d &&
            hdr[5] == 0x0a &&
            hdr[6] == 0x1a &&
            hdr[7] == 0x0a)
            return 100;
    }

    return 0;
}

static const pl_interp_characteristics_t jbig2_characteristics = {
    "JBIG2",
    jbig2_detect_language,
};

/* Get implementation's characteristics */
static const pl_interp_characteristics_t * /* always returns a descriptor */
jbig2_impl_characteristics(const pl_interp_implementation_t *impl)     /* implementation of interpreter to alloc */
{
  return &jbig2_characteristics;
}

static void
jbig2_deallocate(jbig2_interp_instance_t *jbig2)
{
    if (jbig2 == NULL)
        return;

    rc_decrement_cs(jbig2->gray, "jbig2_deallocate");

    if (jbig2->pgs != NULL)
        gs_gstate_free_chain(jbig2->pgs);
    gs_free_object(jbig2->memory, jbig2, "jbig2_impl_allocate_interp_instance");
}

/* Deallocate a interpreter instance */
static int
jbig2_impl_deallocate_interp_instance(pl_interp_implementation_t *impl)
{
    jbig2_interp_instance_t *jbig2 = (jbig2_interp_instance_t *)impl->interp_client_data;

    jbig2_deallocate(jbig2);
    impl->interp_client_data = NULL;

    return 0;
}

/* Do per-instance interpreter allocation/init. */
static int
jbig2_impl_allocate_interp_instance(pl_interp_implementation_t *impl, gs_memory_t *mem)
{
    int code;
    jbig2_interp_instance_t *jbig2
        = (jbig2_interp_instance_t *)gs_alloc_bytes(mem,
                                                  sizeof(jbig2_interp_instance_t),
                                                  "jbig2_impl_allocate_interp_instance");
    if (!jbig2)
        return_error(gs_error_VMerror);
    memset(jbig2, 0, sizeof(*jbig2));

    jbig2->memory = mem;
    jbig2->pgs = gs_gstate_alloc(mem);
    if (jbig2->pgs == NULL)
        goto failVM;

    /* Push one save level onto the stack to assuage the memory handling */
    code = gs_gsave(jbig2->pgs);
    if (code < 0)
        goto fail;

    code = gsicc_init_iccmanager(jbig2->pgs);
    if (code < 0)
        goto fail;

    jbig2->gray = gs_cspace_new_ICC(mem, jbig2->pgs, 1);

    impl->interp_client_data = jbig2;

    return 0;

failVM:
    code = gs_note_error(gs_error_VMerror);
fail:
    (void)jbig2_deallocate(jbig2);
    return code;
}

/*
 * Get the allocator with which to allocate a device
 */
static gs_memory_t *
jbig2_impl_get_device_memory(pl_interp_implementation_t *impl)
{
    jbig2_interp_instance_t *jbig2 = (jbig2_interp_instance_t *)impl->interp_client_data;

    return jbig2->dev ? jbig2->dev->memory : NULL;
}

/* Prepare interp instance for the next "job" */
static int
jbig2_impl_init_job(pl_interp_implementation_t *impl,
                  gx_device                  *device)
{
    jbig2_interp_instance_t *jbig2 = (jbig2_interp_instance_t *)impl->interp_client_data;

    jbig2->dev = device;
    jbig2->state = ii_state_identifying;

    return 0;
}

/* Do any setup for parser per-cursor */
static int                      /* ret 0 or +ve if ok, else -ve error code */
jbig2_impl_process_begin(pl_interp_implementation_t * impl)
{
    return 0;
}

/* Ensure we have 'required' bytes to read, and further ensure
 * that we have no UEL's within those bytes. */
static int
ensure_bytes(jbig2_interp_instance_t *jpg, stream_cursor_read *pr, int required)
{
    int n;
    const uint8_t *p = pr->ptr+1;
    const uint8_t *q;
    int avail;

    /* Find out how many bytes we need to check */
    n = pr->limit - pr->ptr;
    if (n > required)
        n = required;

    /* Make sure there are no UELs in that block */
    q = p + n;
    while (p != q) {
        while (p != q && *p != '\033')
            p++;
        if (p == q)
            break;
        avail = pr->limit - pr->ptr;
        if (memcmp(p, "\033%-12345X", min(avail, 9)) == 0) {
            /* At least a partial match to a UEL */
            return avail < 9 ? gs_error_NeedInput : gs_error_InterpreterExit;
        }
        p++;
    }

    /* If we have enough bytes, great, if not, get some more */
    return (n < required) ? gs_error_NeedInput : 0;
}

static int
flush_to_uel(stream_cursor_read *pr)
{
    const uint8_t *p = pr->ptr+1;
    const uint8_t *q = pr->limit+1;
    int avail;

    while (p != q) {
        while (p != q && *p != '\033')
            p++;
        if (p == q)
            break;
        avail = pr->limit - pr->ptr;
        if (memcmp(p, "\033%-12345X", min(avail, 9)) == 0) {
            /* At least a partial match to a UEL. Bin everything to
             * the start of the match. */
            pr->ptr = p-1;
            if (avail == 9) /* Complete match. Exit! */
                return gs_error_InterpreterExit;
            /* Partial match. Get more data. */
            return gs_error_NeedInput;
        }
        p++;
    }

    pr->ptr = pr->limit;

    return 0;
}

static int
bytes_until_uel(const stream_cursor_read *pr)
{
    const uint8_t *p = pr->ptr+1;
    const uint8_t *q = pr->limit+1;
    int avail;

    while (p != q) {
        while (p != q && *p != '\033')
            p++;
        if (p == q)
            break;
        avail = pr->limit - pr->ptr;
        if (memcmp(p, "\033%-12345X", min(avail, 9)) == 0) {
            /* At least a partial match to a UEL. Everything up to
             * the start of the match is up for grabs. */
            return p - (pr->ptr+1);
        }
        p++;
    }

    return pr->limit - pr->ptr;
}

static void my_errors(void *data, const char *msg, Jbig2Severity severity, uint32_t seg_idx)
{
    /* Do nothing */
}

static void *my_alloc(Jbig2Allocator *allocator, size_t size)
{
    jbig2_interp_instance_t *jbig2 = (jbig2_interp_instance_t *)(((char *)allocator)-offsetof(jbig2_interp_instance_t, allocator));

    return gs_alloc_bytes(jbig2->memory, size, "jbig2(my_alloc)");
}

static void my_free(Jbig2Allocator *allocator, void *p)
{
    jbig2_interp_instance_t *jbig2 = (jbig2_interp_instance_t *)(((char *)allocator)-offsetof(jbig2_interp_instance_t, allocator));

    gs_free_object(jbig2->memory, p, "jbig2(my_free)");
}

static void *my_realloc(Jbig2Allocator *allocator, void *p, size_t size)
{
    jbig2_interp_instance_t *jbig2 = (jbig2_interp_instance_t *)(((char *)allocator)-offsetof(jbig2_interp_instance_t, allocator));

    return gs_resize_object(jbig2->memory, p, size, "jbig2(my_realloc)");
}

static int
do_impl_process(pl_interp_implementation_t * impl, stream_cursor_read * pr, int eof)
{
    jbig2_interp_instance_t *jbig2 = (jbig2_interp_instance_t *)impl->interp_client_data;
    int code = 0;
    ii_state ostate = (ii_state)-1;
    int bytes_left = 0;

    while (jbig2->state != ostate || pr->limit - pr->ptr != bytes_left)
    {
        ostate = jbig2->state;
        bytes_left = pr->limit - pr->ptr;
        switch(jbig2->state)
        {
        case ii_state_identifying:
        {
            const byte *hdr;
            /* Try and get us 8 bytes */
            code = ensure_bytes(jbig2, pr, 8);
            if (code < 0)
                return code;
            hdr = pr->ptr+1;
            if (hdr[0] == 0x97 &&
                hdr[1] == 'J' &&
                hdr[2] == 'B' &&
                hdr[3] == '2' &&
                hdr[4] == 0x0d &&
                hdr[5] == 0x0a &&
                hdr[6] == 0x1a &&
                hdr[7] == 0x0a) {
                jbig2->state = ii_state_jbig2;
                break;
            }
            jbig2->state = ii_state_flush;
            break;
        }
        case ii_state_jbig2:
        {
            /* Gather data into a buffer */
            int bytes = bytes_until_uel(pr);

            if (bytes == 0 && pr->limit - pr->ptr > 9) {
                /* No bytes until UEL, and there is space for a UEL in the buffer */
                jbig2->state = ii_state_jbig2_start;
                break;
            }
            if (bytes == 0 && eof) {
                /* No bytes until UEL, and we are at eof */
                jbig2->state = ii_state_jbig2_start;
                break;
            }

            if (jbig2->jbig_ctx == NULL) {
                jbig2->allocator.alloc = &my_alloc;
                jbig2->allocator.free = &my_free;
                jbig2->allocator.realloc = &my_realloc;
                jbig2->jbig_ctx = jbig2_ctx_new(&jbig2->allocator,
                                                0, /* Options */
                                                NULL, /* Global ctx */
                                                &my_errors,
                                                jbig2);
                if (jbig2->jbig_ctx == NULL) {
                    jbig2->state = ii_state_flush;
                    break;
                }
            }
            if (jbig2_data_in(jbig2->jbig_ctx, pr->ptr+1, bytes)) {
                jbig2->state = ii_state_flush;
                break;
            }
            pr->ptr += bytes;
            break;
        }
        case ii_state_jbig2_start:
            /* This state exists so we can change back to it after
             * a successful decode. It avoids the enclosing loop
             * exiting after the first image of a jbig2 due to the
             * state not having changed. We could avoid this by using
             * a while loop in the "decode" state below, but that would
             * make breaking harder. */
            jbig2->state = ii_state_jbig2_decode;
            break;
        case ii_state_jbig2_decode:
        {
            float xext, yext, xoffset, yoffset, scale;
            unsigned long y, w, h;
            unsigned int used;
            Jbig2Image *img = jbig2_page_out(jbig2->jbig_ctx);
            if (img == NULL) {
                jbig2->state = ii_state_flush;
                break;
            }
            w = img->width;
            h = img->height;

            /* Scale to fit, if too large. */
            scale = 1.0f;
            if (w * jbig2->dev->HWResolution[0] > jbig2->dev->width * 200)
                scale = ((float)jbig2->dev->width * 200) / (w * jbig2->dev->HWResolution[0]);
            if (scale * h * jbig2->dev->HWResolution[1] > jbig2->dev->height * 200)
                scale = ((float)jbig2->dev->height * 200) / (h * jbig2->dev->HWResolution[1]);

            jbig2->nulldev = gs_currentdevice(jbig2->pgs);
            rc_increment(jbig2->nulldev);
            code = gs_setdevice_no_erase(jbig2->pgs, jbig2->dev);
            if (code < 0)
                goto fail_during_decode;

            code = gs_erasepage(jbig2->pgs);
            if (code < 0)
                goto fail_during_decode;

            jbig2->penum = gs_image_enum_alloc(jbig2->memory, "jbig2_impl_process(penum)");
            if (jbig2->penum == NULL) {
                code = gs_note_error(gs_error_VMerror);
                goto fail_during_decode;
            }

            /* Centre - Extents and offsets are all calculated in points (1/72 of an inch) */
            xext = (((float)w) * 72 * scale / 200);
            xoffset = (jbig2->dev->width * 72 / jbig2->dev->HWResolution[0] - xext)/2;
            yext = (((float)h) * 72 * scale / 200);
            yoffset = (jbig2->dev->height * 72 / jbig2->dev->HWResolution[1] - yext)/2;

            gs_initmatrix(jbig2->pgs);

            /* By default the ctm is set to:
             *   xres/72   0
             *   0         -yres/72
             *   0         dev->height * yres/72
             * i.e. it moves the origin from being top right to being bottom left.
             * We want to move it back, as without this, the image will be displayed
             * upside down.
             */
            code = gs_translate(jbig2->pgs, 0.0, jbig2->dev->height * 72 / jbig2->dev->HWResolution[1]);
            if (code >= 0)
                code = gs_translate(jbig2->pgs, xoffset, -yoffset);
            if (code >= 0)
                code = gs_scale(jbig2->pgs, scale, -scale);
            if (code < 0)
                goto fail_during_decode;

            memset(&jbig2->image, 0, sizeof(jbig2->image));
            gs_image_t_init(&jbig2->image, jbig2->gray);
            jbig2->image.BitsPerComponent = 1;
            jbig2->image.Width = w;
            jbig2->image.Height = h;

            jbig2->image.ImageMatrix.xx = 200.0f/72.0f;
            jbig2->image.ImageMatrix.yy = 200.0f/72.0f;
            jbig2->image.Decode[0] = 1.0f;
            jbig2->image.Decode[1] = 0.0f;

            code = gs_image_init(jbig2->penum,
                                 &jbig2->image,
                                 false,
                                 false,
                                 jbig2->pgs);
            if (code < 0)
                goto fail_during_decode;

            for (y = 0; y < img->height; y++) {
                code = gs_image_next(jbig2->penum,
                                     &img->data[y*img->stride],
                                     (img->width+7)>>3,
                                     &used);
                if (code < 0)
                    goto fail_during_decode;
            }
            jbig2_release_page(jbig2->jbig_ctx, img);
            code = gs_image_cleanup_and_free_enum(jbig2->penum, jbig2->pgs);
            jbig2->penum = NULL;
            if (code < 0) {
                jbig2->state = ii_state_flush;
                break;
            }
            (void)pl_finish_page(jbig2->memory->gs_lib_ctx->top_of_system,
                                 jbig2->pgs, 1, true);
            jbig2->state = ii_state_jbig2_start;
            break;
fail_during_decode:
            jbig2_release_page(jbig2->jbig_ctx, img);
            jbig2->state = ii_state_flush;
            break;
        }
        default:
        case ii_state_flush:
            if (jbig2->jbig_ctx) {
                jbig2_ctx_free(jbig2->jbig_ctx);
                jbig2->jbig_ctx = NULL;
            }

            if (jbig2->penum) {
                (void)gs_image_cleanup_and_free_enum(jbig2->penum, jbig2->pgs);
                jbig2->penum = NULL;
            }

            /* We want to bin any data we get up to, but not including
             * a UEL. */
            return flush_to_uel(pr);
        }
    }

    return code;
}

static int
jbig2_impl_process(pl_interp_implementation_t * impl, stream_cursor_read * pr) {
    return do_impl_process(impl, pr, 0);
}

static int
jbig2_impl_process_end(pl_interp_implementation_t * impl)
{
    return 0;
}

/* Not implemented */
static int
jbig2_impl_flush_to_eoj(pl_interp_implementation_t *impl, stream_cursor_read *cursor)
{
    const byte *p = cursor->ptr;
    const byte *rlimit = cursor->limit;

    /* Skip to, but leave UEL in buffer for PJL to find later */
    for (; p < rlimit; ++p)
        if (p[1] == '\033') {
            uint avail = rlimit - p;

            if (memcmp(p + 1, "\033%-12345X", min(avail, 9)))
                continue;
            if (avail < 9)
                break;
            cursor->ptr = p;
            return 1;           /* found eoj */
        }
    cursor->ptr = p;
    return 0;                   /* need more */
}

/* Parser action for end-of-file */
static int
jbig2_impl_process_eof(pl_interp_implementation_t *impl)
{
    stream_cursor_read r;

    r.ptr = NULL;
    r.limit = NULL;
    return do_impl_process(impl, &r, 1);
}

/* Report any errors after running a job */
static int
jbig2_impl_report_errors(pl_interp_implementation_t *impl,          /* interp instance to wrap up job in */
                        int                         code,          /* prev termination status */
                        long                        file_position, /* file position of error, -1 if unknown */
                        bool                        force_to_cout  /* force errors to cout */
)
{
    return 0;
}

/* Wrap up interp instance after a "job" */
static int
jbig2_impl_dnit_job(pl_interp_implementation_t *impl)
{
    jbig2_interp_instance_t *jbig2 = (jbig2_interp_instance_t *)impl->interp_client_data;

    if (jbig2->nulldev) {
        int code = gs_setdevice(jbig2->pgs, jbig2->nulldev);
        jbig2->dev = NULL;
        rc_decrement(jbig2->nulldev, "jbig2_impl_dnit_job(nulldevice)");
        jbig2->nulldev = NULL;
        return code;
    }
    return 0;
}

/* Parser implementation descriptor */
const pl_interp_implementation_t jbig2_implementation = {
  jbig2_impl_characteristics,
  jbig2_impl_allocate_interp_instance,
  jbig2_impl_get_device_memory,
  NULL, /* jbig2_impl_set_param */
  NULL, /* jbig2_impl_add_path */
  NULL, /* jbig2_impl_post_args_init */
  jbig2_impl_init_job,
  NULL, /* jbig2_impl_run_prefix_commands */
  NULL, /* jbig2_impl_process_file */
  jbig2_impl_process_begin,
  jbig2_impl_process,
  jbig2_impl_process_end,
  jbig2_impl_flush_to_eoj,
  jbig2_impl_process_eof,
  jbig2_impl_report_errors,
  jbig2_impl_dnit_job,
  jbig2_impl_deallocate_interp_instance,
  NULL, /* jbig2_impl_reset */
  NULL  /* interp_client_data */
};
