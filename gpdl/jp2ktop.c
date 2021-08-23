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

/* jp2ktop.c */
/* Top-level API implementation of "JP2K" Language Interface */

#include "pltop.h"
#include "gserrors.h"
#include "gxdevice.h"
#include "gsstate.h"
#include "strimpl.h"
#include "gscoord.h"
#include "gsicc_manage.h"
#include "gspaint.h"
#include "plmain.h"
#include "sjpx_openjpeg.h"
#include "stream.h"

/* Forward decls */

/************************************************************/
/******** Language wrapper implementation (see pltop.h) *****/
/************************************************************/

typedef enum
{
    ii_state_identifying = 0,
    ii_state_jp2k,
    ii_state_jp2k_header,
    ii_state_jp2k_data,
    ii_state_flush
} ii_state;

/*
 * JP2K interpreter instance
 */
typedef struct jp2k_interp_instance_s {
    gs_memory_t       *memory;
    gx_device         *dev;
    gx_device         *nulldev;

    gs_color_space    *gray;
    gs_color_space    *rgb;
    gs_color_space    *cmyk;

    /* PWG parser state machine */
    ii_state           state;

    int                pages;

    uint32_t           bpp;
    uint32_t           width;
    uint32_t           height;
    uint32_t           xresolution;
    uint32_t           yresolution;
    uint32_t           copies;

    uint32_t           num_comps;
    uint32_t           byte_width;
    uint32_t           x;
    uint32_t           y;

    gs_image_t         image;
    gs_image_enum     *penum;
    gs_gstate         *pgs;

    stream_jpxd_state  jp2k_state;
    byte               stream_buffer[2048];

} jp2k_interp_instance_t;

static int
jp2k_detect_language(const char *s_, int len)
{
    const unsigned char *s = (const unsigned char *)s_;
    if (len >= 12) {
        if (s[0] == 0x00 &&
            s[1] == 0x00 &&
            s[2] == 0x00 &&
            s[3] == 0x0C &&
            s[4] == 0x6a &&
            s[5] == 0x50 &&
            ((s[6] == 0x1a && s[7] == 0x1a) || (s[6] == 0x20 && s[7] == 0x20)) &&
            s[8] == 0x0d &&
            s[9] == 0x0a &&
            s[10] == 0x87 &&
            s[11] == 0x0a)
            return 100; /* JP2 file */
    }
    if (len >= 4) {
        if (s[0] == 0xff &&
            s[1] == 0x4f &&
            s[2] == 0xff &&
            s[3] == 0x51)
            return 100; /* J2K stream */
    }

    return 0;
}

static const pl_interp_characteristics_t jp2k_characteristics = {
    "JP2K",
    jp2k_detect_language,
};

/* Get implementation's characteristics */
static const pl_interp_characteristics_t * /* always returns a descriptor */
jp2k_impl_characteristics(const pl_interp_implementation_t *impl)     /* implementation of interpreter to alloc */
{
  return &jp2k_characteristics;
}

static void
jp2k_deallocate(jp2k_interp_instance_t *jp2k)
{
    if (jp2k == NULL)
        return;

    rc_decrement_cs(jp2k->gray, "jp2k_deallocate");
    rc_decrement_cs(jp2k->rgb, "jp2k_deallocate");
    rc_decrement_cs(jp2k->cmyk, "jp2k_deallocate");

    if (jp2k->pgs != NULL)
        gs_gstate_free_chain(jp2k->pgs);
    gs_free_object(jp2k->memory, jp2k, "jp2k_impl_allocate_interp_instance");
}

/* Deallocate a interpreter instance */
static int
jp2k_impl_deallocate_interp_instance(pl_interp_implementation_t *impl)
{
    jp2k_interp_instance_t *jp2k = (jp2k_interp_instance_t *)impl->interp_client_data;

    jp2k_deallocate(jp2k);
    impl->interp_client_data = NULL;

    return 0;
}

/* Do per-instance interpreter allocation/init. */
static int
jp2k_impl_allocate_interp_instance(pl_interp_implementation_t *impl, gs_memory_t *mem)
{
    int code;
    jp2k_interp_instance_t *jp2k
        = (jp2k_interp_instance_t *)gs_alloc_bytes(mem,
                                                  sizeof(jp2k_interp_instance_t),
                                                  "jp2k_impl_allocate_interp_instance");
    if (!jp2k)
        return_error(gs_error_VMerror);
    memset(jp2k, 0, sizeof(*jp2k));

    jp2k->memory = mem;
    jp2k->pgs = gs_gstate_alloc(mem);
    if (jp2k->pgs == NULL)
        goto failVM;

    /* Push one save level onto the stack to assuage the memory handling */
    code = gs_gsave(jp2k->pgs);
    if (code < 0)
        goto fail;

    code = gsicc_init_iccmanager(jp2k->pgs);
    if (code < 0)
        goto fail;

    jp2k->gray = gs_cspace_new_ICC(mem, jp2k->pgs, 1);
    jp2k->rgb  = gs_cspace_new_ICC(mem, jp2k->pgs, 3);
    jp2k->cmyk = gs_cspace_new_ICC(mem, jp2k->pgs, 4);

    impl->interp_client_data = jp2k;

    return 0;

failVM:
    code = gs_note_error(gs_error_VMerror);
fail:
    (void)jp2k_deallocate(jp2k);
    return code;
}

/*
 * Get the allocator with which to allocate a device
 */
static gs_memory_t *
jp2k_impl_get_device_memory(pl_interp_implementation_t *impl)
{
    jp2k_interp_instance_t *jp2k = (jp2k_interp_instance_t *)impl->interp_client_data;

    return jp2k->dev ? jp2k->dev->memory : NULL;
}

#if 0 /* UNUSED */
static int
jp2k_impl_set_param(pl_interp_implementation_t *impl,
                   pl_set_param_type           type,
                   const char                 *param,
                   const void                 *val)
{
    jp2k_interp_instance_t *jp2k = (jp2k_interp_instance_t *)impl->interp_client_data;

    /* No params set here */
    return 0;
}

static int
jp2k_impl_add_path(pl_interp_implementation_t *impl,
                  const char                 *path)
{
    jp2k_interp_instance_t *jp2k = (jp2k_interp_instance_t *)impl->interp_client_data;

    /* No paths to add */
    return 0;
}

static int
jp2k_impl_post_args_init(pl_interp_implementation_t *impl)
{
    jp2k_interp_instance_t *jp2k = (jp2k_interp_instance_t *)impl->interp_client_data;

    /* No post args processing */
    return 0;
}
#endif

/* Prepare interp instance for the next "job" */
static int
jp2k_impl_init_job(pl_interp_implementation_t *impl,
                  gx_device                  *device)
{
    jp2k_interp_instance_t *jp2k = (jp2k_interp_instance_t *)impl->interp_client_data;

    jp2k->dev = device;
    jp2k->state = ii_state_identifying;

    return 0;
}

#if 0 /* UNUSED */
static int
jp2k_impl_run_prefix_commands(pl_interp_implementation_t *impl,
                             const char                 *prefix)
{
    jp2k_interp_instance_t *jp2k = (jp2k_interp_instance_t *)impl->interp_client_data;

    return 0;
}

static int
jp2k_impl_process_file(pl_interp_implementation_t *impl, const char *filename)
{
    jp2k_interp_instance_t *jp2k = (jp2k_interp_instance_t *)impl->interp_client_data;

    return 0;
}
#endif

/* Do any setup for parser per-cursor */
static int                      /* ret 0 or +ve if ok, else -ve error code */
jp2k_impl_process_begin(pl_interp_implementation_t * impl)
{
    return 0;
}

/* Ensure we have 'required' bytes to read, and further ensure
 * that we have no UEL's within those bytes. */
static int
ensure_bytes(jp2k_interp_instance_t *jp2k, stream_cursor_read *pr, int required)
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

static int
do_process(jp2k_interp_instance_t *jp2k, stream_cursor_read * pr, bool eof)
{
    int code = 0;
    gs_color_space *cs;
    ii_state ostate;
    size_t bytes_in;
    int advanced;

    /* Loop until we stop 'advancing'. */
    do
    {
        advanced = 0;
        ostate = jp2k->state;
        bytes_in = pr->limit - pr->ptr;
        switch(jp2k->state)
        {
        case ii_state_identifying:
        {
            const byte *s = pr->ptr+1;

            code = ensure_bytes(jp2k, pr, 4);
            if (code < 0)
                return code;
            if (s[0] == 0xff &&
                s[1] == 0x4f &&
                s[2] == 0xff &&
                s[3] == 0x51) {
                jp2k->state = ii_state_jp2k_header;
                break;
            }

            code = ensure_bytes(jp2k, pr, 12);
            if (code < 0)
                return code;
            if (s[0] == 0x00 &&
                s[1] == 0x00 &&
                s[2] == 0x00 &&
                s[3] == 0x0C &&
                s[4] == 0x6a &&
                s[5] == 0x50 &&
                ((s[6] == 0x1a && s[7] == 0x1a) || (s[6] == 0x20 && s[7] == 0x20)) &&
                s[8] == 0x0d &&
                s[9] == 0x0a &&
                s[10] == 0x87 &&
                s[11] == 0x0a) {
                jp2k->state = ii_state_jp2k_header;
                break;
            }
            jp2k->state = ii_state_flush;
            break;
        }
        case ii_state_jp2k_header:
            s_init_state((stream_state *)&jp2k->jp2k_state, &s_jpxd_template, jp2k->memory);
            if (s_jpxd_template.set_defaults)
                s_jpxd_template.set_defaults((stream_state *)&jp2k->jp2k_state);

            code = (s_jpxd_template.init)((stream_state *)&jp2k->jp2k_state);
            if (code < 0)
                goto early_flush;
            jp2k->state = ii_state_jp2k_data;
            break;
        case ii_state_jp2k_data:
        {
            int n = bytes_until_uel(pr);
            stream_cursor_read local_r;
            stream_cursor_write local_w;
            int status;
            unsigned int used;

            local_r.ptr = pr->ptr;
            local_r.limit = local_r.ptr + n;

            do {
                local_w.ptr = jp2k->stream_buffer-1;
                local_w.limit = local_w.ptr + sizeof(jp2k->stream_buffer);

                status = (s_jpxd_template.process)((stream_state *)&jp2k->jp2k_state,
                                                                   &local_r, &local_w,
                                                                   eof);
                /* status = 0 => need data
                 *          1 => need output space
                 * but we don't actually use this currently. */
                (void)status;
                /* Copy the updated pointer back */
                pr->ptr = local_r.ptr;
                if (local_w.ptr + 1 == jp2k->stream_buffer)
                    break; /* Failed to decode any data */

                if (jp2k->width == 0) {
                    float scale, xoffset, yoffset, xext, yext;

                    /* First data we decoded! */
                    jp2k->width = jp2k->jp2k_state.width;
                    if (jp2k->width == 0) {
                        jp2k->state = ii_state_flush;
                        break;
                    }
                    jp2k->height = jp2k->jp2k_state.height;
                    jp2k->bpp = jp2k->jp2k_state.bpp;
                    jp2k->xresolution = 72;
                    jp2k->yresolution = 72;
                    jp2k->copies = 1;
                    /* I would have thought we should use jp2k->jp2k_state.out_numcomps,
                     * but this is wrong! */
                    jp2k->num_comps = jp2k->bpp/8;
                    jp2k->byte_width = (jp2k->width * jp2k->bpp + 7)>>3;

#if 0
                    switch (jp2k->jp2k_state.colorspace) {
                    default:
                        goto early_flush;
                    case gs_jpx_cs_gray:
                        cs = jp2k->gray;
                        break;
                    case gs_jpx_cs_rgb:
                        cs = jp2k->rgb;
                        break;
                    case gs_jpx_cs_cmyk:
                        cs = jp2k->cmyk;
                        break;
                    }
#else
                    switch (jp2k->num_comps) {
                    case 1:
                        cs = jp2k->gray;
                        break;
                    case 3:
                        cs = jp2k->rgb;
                        break;
                    case 4:
                        cs = jp2k->cmyk;
                        break;
                    default:
                        goto early_flush;
                    }
#endif

                    /* Scale to fit, if too large. */
                    scale = 1.0f;
                    if (jp2k->width * jp2k->dev->HWResolution[0] > jp2k->dev->width * jp2k->xresolution)
                        scale = ((float)jp2k->dev->width * jp2k->xresolution) / (jp2k->width * jp2k->dev->HWResolution[0]);
                    if (scale * jp2k->height * jp2k->dev->HWResolution[1] > jp2k->dev->height * jp2k->yresolution)
                        scale = ((float)jp2k->dev->height * jp2k->yresolution) / (jp2k->height * jp2k->dev->HWResolution[1]);

                    jp2k->nulldev = gs_currentdevice(jp2k->pgs);
                    rc_increment(jp2k->nulldev);
                    code = gs_setdevice_no_erase(jp2k->pgs, jp2k->dev);
                    if (code < 0)
                        goto early_flush;
                    gs_initmatrix(jp2k->pgs);

                    /* Centre - Extents and offsets are all calculated in points (1/72 of an inch) */
                    xext = (((float)jp2k->width) * 72 * scale / jp2k->xresolution);
                    xoffset = (jp2k->dev->width * 72 / jp2k->dev->HWResolution[0] - xext)/2;
                    yext = (((float)jp2k->height) * 72 * scale / jp2k->yresolution);
                    yoffset = (jp2k->dev->height * 72 / jp2k->dev->HWResolution[1] - yext)/2;

                    /* By default the ctm is set to:
                     *   xres/72   0
                     *   0         -yres/72
                     *   0         dev->height * yres/72
                     * i.e. it moves the origin from being top right to being bottom left.
                     * We want to move it back, as without this, the image will be displayed
                     * upside down.
                     */
                    code = gs_translate(jp2k->pgs, 0.0, jp2k->dev->height * 72 / jp2k->dev->HWResolution[1]);
                    if (code >= 0)
                        code = gs_translate(jp2k->pgs, xoffset, -yoffset);
                    if (code >= 0)
                        code = gs_scale(jp2k->pgs, scale, -scale);
                    /* At this point, the ctm is set to:
                     *   xres/72  0
                     *   0        yres/72
                     *   0        0
                     */
                    if (code >= 0)
                        code = gs_erasepage(jp2k->pgs);
                    if (code < 0)
                        goto early_flush;

                    memset(&jp2k->image, 0, sizeof(jp2k->image));
                    gs_image_t_init(&jp2k->image, cs);
                    jp2k->image.BitsPerComponent = jp2k->bpp/jp2k->num_comps;
                    jp2k->image.Width = jp2k->width;
                    jp2k->image.Height = jp2k->height;

                    jp2k->image.ImageMatrix.xx = jp2k->xresolution / 72.0f;
                    jp2k->image.ImageMatrix.yy = jp2k->yresolution / 72.0f;

                    jp2k->penum = gs_image_enum_alloc(jp2k->memory, "jp2k_impl_process(penum)");
                    if (jp2k->penum == NULL) {
                        code = gs_note_error(gs_error_VMerror);
                        goto early_flush;
                    }

                    code = gs_image_init(jp2k->penum,
                                         &jp2k->image,
                                         false,
                                         false,
                                         jp2k->pgs);
                    if (code < 0)
                        goto early_flush;
                }

                code = gs_image_next(jp2k->penum, jp2k->stream_buffer, local_w.ptr + 1 - jp2k->stream_buffer, &used);
                if (code < 0)
                    goto flush;

                jp2k->x += used;
                while (jp2k->x >= jp2k->byte_width) {
                    jp2k->x -= jp2k->byte_width;
                    jp2k->y++;
                }
                /* Loop while we haven't had all the lines, the decompression
                 * didn't ask for more data, and the decompression didn't give
                 * us more data. */
            } while (jp2k->y < jp2k->height);

            if (jp2k->penum && jp2k->y == jp2k->height) {
                code = gs_image_cleanup_and_free_enum(jp2k->penum, jp2k->pgs);
                jp2k->penum = NULL;
                if (code < 0)
                    goto flush;
                code = pl_finish_page(jp2k->memory->gs_lib_ctx->top_of_system,
                                      jp2k->pgs, jp2k->copies, true);
                if (code < 0)
                    goto flush;
                if (jp2k->pages > 0) {
                    jp2k->pages--;
                    /* If we've reached the expected end, we should probably flush to UEL */
                    if (jp2k->pages == 0)
                        jp2k->state = ii_state_flush;
                }
                if (jp2k->jp2k_state.templat->release)
                    jp2k->jp2k_state.templat->release((stream_state *)&jp2k->jp2k_state);
                jp2k->state = ii_state_jp2k_header;
            }
            break;
        }
        default:
        case ii_state_flush:
            if (0) {
flush:
                if (jp2k->jp2k_state.templat->release)
                    jp2k->jp2k_state.templat->release((stream_state *)&jp2k->jp2k_state);
early_flush:
                jp2k->state = ii_state_flush;
            }
            /* We want to bin any data we get up to, but not including
             * a UEL. */
            return flush_to_uel(pr);
        }
        advanced |= (ostate != jp2k->state);
        advanced |= (bytes_in != pr->limit - pr->ptr);
    } while (advanced);

    return code;
}

static int
jp2k_impl_process(pl_interp_implementation_t * impl, stream_cursor_read * pr)
{
    jp2k_interp_instance_t *jp2k = (jp2k_interp_instance_t *)impl->interp_client_data;

    return do_process(jp2k, pr, 0);
}

static int
jp2k_impl_process_end(pl_interp_implementation_t * impl)
{
    return 0;
}

/* Not implemented */
static int
jp2k_impl_flush_to_eoj(pl_interp_implementation_t *impl, stream_cursor_read *cursor)
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
jp2k_impl_process_eof(pl_interp_implementation_t *impl)
{
    jp2k_interp_instance_t *jp2k = (jp2k_interp_instance_t *)impl->interp_client_data;
    stream_cursor_read cursor;

    cursor.ptr = NULL;
    cursor.limit = NULL;

    return do_process(jp2k, &cursor, 1);
}

/* Report any errors after running a job */
static int
jp2k_impl_report_errors(pl_interp_implementation_t *impl,          /* interp instance to wrap up job in */
                       int                         code,          /* prev termination status */
                       long                        file_position, /* file position of error, -1 if unknown */
                       bool                        force_to_cout  /* force errors to cout */
)
{
    return 0;
}

/* Wrap up interp instance after a "job" */
static int
jp2k_impl_dnit_job(pl_interp_implementation_t *impl)
{
    jp2k_interp_instance_t *jp2k = (jp2k_interp_instance_t *)impl->interp_client_data;

    if (jp2k->nulldev) {
        int code = gs_setdevice(jp2k->pgs, jp2k->nulldev);
        jp2k->dev = NULL;
        rc_decrement(jp2k->nulldev, "jp2k_impl_dnit_job(nulldevice)");
        jp2k->nulldev = NULL;
        return code;
    }
    return 0;
}

/* Parser implementation descriptor */
const pl_interp_implementation_t jp2k_implementation = {
  jp2k_impl_characteristics,
  jp2k_impl_allocate_interp_instance,
  jp2k_impl_get_device_memory,
  NULL, /* jp2k_impl_set_param */
  NULL, /* jp2k_impl_add_path */
  NULL, /* jp2k_impl_post_args_init */
  jp2k_impl_init_job,
  NULL, /* jp2k_impl_run_prefix_commands */
  NULL, /* jp2k_impl_process_file */
  jp2k_impl_process_begin,
  jp2k_impl_process,
  jp2k_impl_process_end,
  jp2k_impl_flush_to_eoj,
  jp2k_impl_process_eof,
  jp2k_impl_report_errors,
  jp2k_impl_dnit_job,
  jp2k_impl_deallocate_interp_instance,
  NULL, /* jp2k_impl_reset */
  NULL  /* interp_client_data */
};
