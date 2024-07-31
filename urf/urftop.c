/* Copyright (C) 2019-2024 Artifex Software, Inc.
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

/* urftop.c */
/* Top-level API implementation of "URF" Language Interface */

#include "pltop.h"
#include "plmain.h"
#include "gserrors.h"
#include "gxdevice.h"
#include "gsstate.h"
#include "surfx.h"
#include "strimpl.h"
#include "stream.h"
#include "gscoord.h"
#include "gsicc_manage.h"
#include "gspaint.h"

/* Forward decls */

/************************************************************/
/******** Language wrapper implementation (see pltop.h) *****/
/************************************************************/

typedef enum
{
    ii_state_identifying = 0,
    ii_state_unirast,
    ii_state_unirast_header,
    ii_state_unirast_data,
    ii_state_flush
} ii_state;

/*
 * URF interpreter instance
 */
typedef struct urf_interp_instance_s {
    gs_memory_t       *memory;
    gx_device         *dev;
    gx_device         *nulldev;

    gs_color_space    *gray;
    gs_color_space    *rgb;
    gs_color_space    *cmyk;

    /* URF parser state machine */
    ii_state           state;

    int                pages;

    uint8_t            bpp;
    uint8_t            cs;
    uint8_t            duplexMode;
    uint8_t            printQuality;
    uint8_t            mediaType;
    uint8_t            inputSlot;
    uint8_t            outputBin;
    uint8_t            copies;
    uint32_t           finishings;
    uint32_t           width;
    uint32_t           height;
    uint32_t           resolution;

    uint32_t           num_comps;
    uint32_t           byte_width;
    uint32_t           x;
    uint32_t           y;

    gs_image_t         image;
    gs_image_enum     *penum;
    gs_gstate         *pgs;

    stream_URFD_state  urfd_state;
    byte               stream_buffer[2048];

} urf_interp_instance_t;

static int
urf_detect_language(const char *s, int len)
{
    /* For postscript, we look for %! */
    if (len >= 8) {
        if (strncmp(s, "UNIRAST", len) == 0)
            return 100;
    }
    /* FIXME: Other formats go here */

    return 0;
}

static const pl_interp_characteristics_t urf_characteristics = {
    "URF",
    urf_detect_language
};

/* Get implementation's characteristics */
static const pl_interp_characteristics_t * /* always returns a descriptor */
urf_impl_characteristics(const pl_interp_implementation_t *impl)     /* implementation of interpreter to alloc */
{
  return &urf_characteristics;
}

static void
urf_deallocate(urf_interp_instance_t *urf)
{
    if (urf == NULL)
        return;

    rc_decrement_cs(urf->gray, "urf_deallocate");
    rc_decrement_cs(urf->rgb, "urf_deallocate");
    rc_decrement_cs(urf->cmyk, "urf_deallocate");

    if (urf->pgs != NULL)
        gs_gstate_free_chain(urf->pgs);
    gs_free_object(urf->memory, urf, "urf_impl_allocate_interp_instance");
}

/* Deallocate a interpreter instance */
static int
urf_impl_deallocate_interp_instance(pl_interp_implementation_t *impl)
{
    urf_interp_instance_t *urf = (urf_interp_instance_t *)impl->interp_client_data;

    urf_deallocate(urf);
    impl->interp_client_data = NULL;

    return 0;
}

/* Do per-instance interpreter allocation/init. */
static int
urf_impl_allocate_interp_instance(pl_interp_implementation_t *impl, gs_memory_t *mem)
{
    int code;
    urf_interp_instance_t *urf
        = (urf_interp_instance_t *)gs_alloc_bytes(mem,
                                                  sizeof(urf_interp_instance_t),
                                                  "urf_impl_allocate_interp_instance");
    if (!urf)
        return_error(gs_error_VMerror);
    memset(urf, 0, sizeof(*urf));

    urf->memory = mem;
    urf->pgs = gs_gstate_alloc(mem);
    if (urf->pgs == NULL)
        goto failVM;

    /* Push one save level onto the stack to assuage the memory handling */
    code = gs_gsave(urf->pgs);
    if (code < 0)
        goto fail;

    code = gsicc_init_iccmanager(urf->pgs);
    if (code < 0)
        goto fail;

    urf->gray = gs_cspace_new_ICC(mem, urf->pgs, 1);
    urf->rgb  = gs_cspace_new_ICC(mem, urf->pgs, 3);
    urf->cmyk = gs_cspace_new_ICC(mem, urf->pgs, 4);

    impl->interp_client_data = urf;

    return 0;

failVM:
    code = gs_note_error(gs_error_VMerror);
fail:
    (void)urf_deallocate(urf);
    return code;
}

/*
 * Get the allocator with which to allocate a device
 */
static gs_memory_t *
urf_impl_get_device_memory(pl_interp_implementation_t *impl)
{
    urf_interp_instance_t *urf = (urf_interp_instance_t *)impl->interp_client_data;

    return urf->dev ? urf->dev->memory : NULL;
}

#if 0 /* UNUSED */
static int
urf_impl_set_param(pl_interp_implementation_t *impl,
                   pl_set_param_type           type,
                   const char                 *param,
                   const void                 *val)
{
    urf_interp_instance_t *urf = (urf_interp_instance_t *)impl->interp_client_data;

    /* No params set here */
    return 0;
}

static int
urf_impl_add_path(pl_interp_implementation_t *impl,
                  const char                 *path)
{
    urf_interp_instance_t *urf = (urf_interp_instance_t *)impl->interp_client_data;

    /* No paths to add */
    return 0;
}

static int
urf_impl_post_args_init(pl_interp_implementation_t *impl)
{
    urf_interp_instance_t *urf = (urf_interp_instance_t *)impl->interp_client_data;

    /* No post args processing */
    return 0;
}
#endif

/* Prepare interp instance for the next "job" */
static int
urf_impl_init_job(pl_interp_implementation_t *impl,
                  gx_device                  *device)
{
    urf_interp_instance_t *urf = (urf_interp_instance_t *)impl->interp_client_data;

    urf->dev = device;
    urf->state = ii_state_identifying;

    return 0;
}

#if 0 /* UNUSED */
static int
urf_impl_run_prefix_commands(pl_interp_implementation_t *impl,
                             const char                 *prefix)
{
    urf_interp_instance_t *urf = (urf_interp_instance_t *)impl->interp_client_data;

    return 0;
}

static int
urf_impl_process_file(pl_interp_implementation_t *impl, const char *filename)
{
    urf_interp_instance_t *urf = (urf_interp_instance_t *)impl->interp_client_data;

    return 0;
}
#endif

/* Do any setup for parser per-cursor */
static int                      /* ret 0 or +ve if ok, else -ve error code */
urf_impl_process_begin(pl_interp_implementation_t * impl)
{
    return 0;
}

/* Ensure we have 'required' bytes to read, and further ensure
 * that we have no UEL's within those bytes. */
static int
ensure_bytes(urf_interp_instance_t *urf, stream_cursor_read *pr, int required)
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
get32be(stream_cursor_read *pr)
{
    int v = pr->ptr[1] << 24;
    v |= pr->ptr[2] << 16;
    v |= pr->ptr[3] << 8;
    v |= pr->ptr[4];
    pr->ptr += 4;

    return v;
}

static int
get8(stream_cursor_read *pr)
{
    return *++(pr->ptr);
}

static int
urf_impl_process(pl_interp_implementation_t * impl, stream_cursor_read * pr)
{
    urf_interp_instance_t *urf = (urf_interp_instance_t *)impl->interp_client_data;
    int code = 0;
    gs_color_space *cs;

    /* Loop while we have bytes to read */
    while (pr->limit > pr->ptr)
    {
        switch(urf->state)
        {
        case ii_state_identifying:
            /* Try and get us 8 bytes for "UNIRAST\0" */
            code = ensure_bytes(urf, pr, 8);
            if (code < 0)
                return code;
            if (strncmp("UNIRAST", (const char *)pr->ptr+1, 8) == 0)
            {
                pr->ptr += 8;
                urf->state = ii_state_unirast;
                break;
            }
            urf->state = ii_state_flush;
            break;
        case ii_state_unirast:
            /* Try and get us 4 bytes for the page count */
            code = ensure_bytes(urf, pr, 4);
            if (code < 0)
                return code;
            urf->pages = get32be(pr);
            if (urf->pages == 0)
                urf->pages = -1;
            urf->state = ii_state_unirast_header;
            break;
        case ii_state_unirast_header:
            code = ensure_bytes(urf, pr, 32);
            if (code < 0)
                return code;
            urf->bpp          = get8(pr);
            urf->cs           = get8(pr);
            urf->duplexMode   = get8(pr);
            urf->printQuality = get8(pr);
            urf->mediaType    = get8(pr);
            urf->inputSlot    = get8(pr);
            urf->outputBin    = get8(pr);
            urf->copies       = get8(pr);
            if (urf->copies == 0)
                urf->copies = 1;
            urf->finishings   = get32be(pr);
            urf->width        = get32be(pr);
            urf->height       = get32be(pr);
            urf->resolution   = get32be(pr);
            (void)get32be(pr);
            (void)get32be(pr);

            /* Decode the header */
            switch(urf->cs) {
            case 0: /* W */
            case 4: /* DeviceW */
                urf->num_comps = 1;
                cs = urf->gray;
                break;
            case 1: /* sRGB */
            case 5: /* DeviceRGB */
                urf->num_comps = 3;
                cs = urf->rgb;
                break;
            case 6: /* CMYK */
                urf->num_comps = 4;
                cs = urf->cmyk;
                break;
            default:
                goto bad_header;
            }
            if (urf->bpp != 8*urf->num_comps && urf->bpp != 16*urf->num_comps) {
bad_header:
                urf->state = ii_state_flush;
                break;
            }
            urf->byte_width = (urf->bpp>>3)*urf->width;

            if (urf->nulldev == NULL)
            {
                urf->nulldev = gs_currentdevice(urf->pgs);
                rc_increment(urf->nulldev);
                code = gs_setdevice_no_erase(urf->pgs, urf->dev);
                if (code < 0)
                    goto early_flush;
            }
            gs_initmatrix(urf->pgs);

            /* By default the ctm is set to:
             *   xres/72   0
             *   0         -yres/72
             *   0         dev->height * yres/72
             * i.e. it moves the origin from being top right to being bottom left.
             * We want to move it back, as without this, the image will be displayed
             * upside down.
             */
            code = gs_translate(urf->pgs, 0.0, urf->dev->height * 72 / urf->dev->HWResolution[1]);
            if (code >= 0)
                code = gs_scale(urf->pgs, 1, -1);
            /* At this point, the ctm is set to:
             *   xres/72  0
             *   0        yres/72
             *   0        0
             */
            if (code >= 0)
                code = gs_erasepage(urf->pgs);
            if (code < 0)
                goto early_flush;

            memset(&urf->image, 0, sizeof(urf->image));
            gs_image_t_init(&urf->image, cs);
            urf->image.BitsPerComponent = urf->bpp/urf->num_comps;
            urf->image.Width = urf->width;
            urf->image.Height = urf->height;

            urf->image.ImageMatrix.xx = urf->resolution / 72.0f;
            urf->image.ImageMatrix.yy = urf->resolution / 72.0f;

            urf->penum = gs_image_enum_alloc(urf->memory, "urf_impl_process(penum)");
            if (urf->penum == NULL) {
                code = gs_note_error(gs_error_VMerror);
                goto early_flush;
            }

            code = gs_image_init(urf->penum,
                                 &urf->image,
                                 false,
                                 false,
                                 urf->pgs);
            if (code < 0)
                goto early_flush;

            s_init_state((stream_state *)&urf->urfd_state, &s_URFD_template, urf->memory);
            if (s_URFD_template.set_defaults)
                s_URFD_template.set_defaults((stream_state *)&urf->urfd_state);

            urf->urfd_state.width = urf->width;
            urf->urfd_state.bpp = urf->bpp;
            urf->urfd_state.white = urf->num_comps == 4 ? 0xff : 0;

            code = (s_URFD_template.init)((stream_state *)&urf->urfd_state);
            if (code < 0)
                goto early_flush;
            urf->x = urf->y = 0;
            urf->state = ii_state_unirast_data;
            break;
        case ii_state_unirast_data:
        {
            int n = bytes_until_uel(pr);
            stream_cursor_read local_r;
            stream_cursor_write local_w;
            int status;
            unsigned int used;
            int decoded_any = 0;

            local_r.ptr = pr->ptr;
            local_r.limit = local_r.ptr + n;

            do {
                local_w.ptr = urf->stream_buffer-1;
                local_w.limit = local_w.ptr + sizeof(urf->stream_buffer);

                status = (s_URFD_template.process)((stream_state *)&urf->urfd_state,
                                                                   &local_r, &local_w,
                                                                   true);
                (void)status;

                /* status = 0 => need data
                 *          1 => need output space */
                /* Copy the updated pointer back */
                if (pr->ptr != local_r.ptr)
                {
                    /* We may not have actually decoded anything, but we consumed some, so
                     * progress was made! */
                    decoded_any = 1;
                    pr->ptr = local_r.ptr;
                }
                if (local_w.ptr + 1 == urf->stream_buffer)
                    break; /* Failed to decode any data */
                decoded_any = 1;

                /* FIXME: Do we need to byte swap 16bit data? */
                code = gs_image_next(urf->penum, urf->stream_buffer, local_w.ptr + 1 - urf->stream_buffer, &used);
                if (code < 0)
                    goto flush;

                urf->x += used;
                while (urf->x >= urf->byte_width) {
                    urf->x -= urf->byte_width;
                    urf->y++;
                }
                /* Loop while we haven't had all the lines, the decompression
                 * didn't ask for more data, and the decompression didn't give
                 * us more data. */
            } while (urf->y < urf->height);

            if (urf->y == urf->height) {
                code = gs_image_cleanup_and_free_enum(urf->penum, urf->pgs);
                urf->penum = NULL;
                if (code < 0)
                    goto flush;
                code = pl_finish_page(urf->memory->gs_lib_ctx->top_of_system,
                                      urf->pgs, urf->copies, true);
                if (code < 0)
                    goto flush;
                if (urf->pages > 0) {
                    urf->pages--;
                    /* If we've reached the expected end, we should probably flush to UEL */
                    if (urf->pages == 0)
                        urf->state = ii_state_flush;
                }
                urf->urfd_state.templat->release((stream_state *)&urf->urfd_state);
                urf->state = ii_state_unirast_header;
            } else if (decoded_any == 0) {
                /* Failed to make progress. Just give up to avoid livelocking. */
                goto flush;
            }
            break;
        }
        default:
        case ii_state_flush:
            if (0) {
flush:
                urf->urfd_state.templat->release((stream_state *)&urf->urfd_state);
early_flush:
                urf->state = ii_state_flush;
            }
            /* We want to bin any data we get up to, but not including
             * a UEL. */
            return flush_to_uel(pr);
        }
    }

    return code;
}

static int
urf_impl_process_end(pl_interp_implementation_t * impl)
{
    return 0;
}

/* Not implemented */
static int
urf_impl_flush_to_eoj(pl_interp_implementation_t *impl, stream_cursor_read *cursor)
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
urf_impl_process_eof(pl_interp_implementation_t *impl)
{
    return 0;
}

/* Report any errors after running a job */
static int
urf_impl_report_errors(pl_interp_implementation_t *impl,          /* interp instance to wrap up job in */
                       int                         code,          /* prev termination status */
                       long                        file_position, /* file position of error, -1 if unknown */
                       bool                        force_to_cout  /* force errors to cout */
)
{
    return 0;
}

/* Wrap up interp instance after a "job" */
static int
urf_impl_dnit_job(pl_interp_implementation_t *impl)
{
    urf_interp_instance_t *urf = (urf_interp_instance_t *)impl->interp_client_data;

    if (urf->nulldev) {
        int code = gs_setdevice(urf->pgs, urf->nulldev);
        urf->dev = NULL;
        rc_decrement(urf->nulldev, "urf_impl_dnit_job(nulldevice)");
        urf->nulldev = NULL;
        return code;
    }
    return 0;
}

/* Parser implementation descriptor */
const pl_interp_implementation_t urf_implementation = {
  urf_impl_characteristics,
  urf_impl_allocate_interp_instance,
  urf_impl_get_device_memory,
  NULL, /* urf_impl_set_param */
  NULL, /* urf_impl_add_path */
  NULL, /* urf_impl_post_args_init */
  urf_impl_init_job,
  NULL, /* urf_impl_run_prefix_commands */
  NULL, /* urf_impl_process_file */
  urf_impl_process_begin,
  urf_impl_process,
  urf_impl_process_end,
  urf_impl_flush_to_eoj,
  urf_impl_process_eof,
  urf_impl_report_errors,
  urf_impl_dnit_job,
  urf_impl_deallocate_interp_instance,
  NULL
};
