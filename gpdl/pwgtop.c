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

/* pwgtop.c */
/* Top-level API implementation of "PWG" Language Interface */

#include "pltop.h"
#include "gserrors.h"
#include "gxdevice.h"
#include "gsstate.h"
#include "strimpl.h"
#include "gscoord.h"
#include "gsicc_manage.h"
#include "gspaint.h"
#include "plmain.h"
#include "spwgx.h"
#include "stream.h"

/* Forward decls */

/************************************************************/
/******** Language wrapper implementation (see pltop.h) *****/
/************************************************************/

typedef enum
{
    ii_state_identifying = 0,
    ii_state_pwg,
    ii_state_pwg_header,
    ii_state_pwg_data,
    ii_state_flush
} ii_state;

/*
 * PWG interpreter instance
 */
typedef struct pwg_interp_instance_s {
    gs_memory_t       *memory;
    gx_device         *dev;
    gx_device         *nulldev;

    gs_color_space    *gray;
    gs_color_space    *rgb;
    gs_color_space    *cmyk;

    /* PWG parser state machine */
    ii_state           state;

    int                pages;

    uint32_t           bpc;
    uint32_t           bpp;
    uint32_t           bpl;
    uint32_t           colororder;
    uint32_t           cs;
    uint32_t           duplexMode;
    uint32_t           printQuality;
    uint32_t           mediaType;
    uint32_t           copies;
    uint32_t           width;
    uint32_t           height;
    uint32_t           xresolution;
    uint32_t           yresolution;

    uint32_t           num_comps;
    uint32_t           byte_width;
    uint32_t           x;
    uint32_t           y;

    gs_image_t         image;
    gs_image_enum     *penum;
    gs_gstate         *pgs;

    stream_PWGD_state  pwgd_state;
    byte               stream_buffer[2048];

} pwg_interp_instance_t;

static int
pwg_detect_language(const char *s, int len)
{
    /* For postscript, we look for %! */
    if (len >= 4) {
        if (memcmp(s, "RaS2", 4) == 0)
            return 100;
    }

    return 0;
}

static const pl_interp_characteristics_t pwg_characteristics = {
    "PWG",
    pwg_detect_language,
};

/* Get implementation's characteristics */
static const pl_interp_characteristics_t * /* always returns a descriptor */
pwg_impl_characteristics(const pl_interp_implementation_t *impl)     /* implementation of interpreter to alloc */
{
  return &pwg_characteristics;
}

static void
pwg_deallocate(pwg_interp_instance_t *pwg)
{
    if (pwg == NULL)
        return;

    rc_decrement_cs(pwg->gray, "pwg_deallocate");
    rc_decrement_cs(pwg->rgb, "pwg_deallocate");
    rc_decrement_cs(pwg->cmyk, "pwg_deallocate");

    if (pwg->pgs != NULL)
        gs_gstate_free_chain(pwg->pgs);
    gs_free_object(pwg->memory, pwg, "pwg_impl_allocate_interp_instance");
}

/* Deallocate a interpreter instance */
static int
pwg_impl_deallocate_interp_instance(pl_interp_implementation_t *impl)
{
    pwg_interp_instance_t *pwg = (pwg_interp_instance_t *)impl->interp_client_data;

    pwg_deallocate(pwg);
    impl->interp_client_data = NULL;

    return 0;
}

/* Do per-instance interpreter allocation/init. */
static int
pwg_impl_allocate_interp_instance(pl_interp_implementation_t *impl, gs_memory_t *mem)
{
    int code;
    pwg_interp_instance_t *pwg
        = (pwg_interp_instance_t *)gs_alloc_bytes(mem,
                                                  sizeof(pwg_interp_instance_t),
                                                  "pwg_impl_allocate_interp_instance");
    if (!pwg)
        return_error(gs_error_VMerror);
    memset(pwg, 0, sizeof(*pwg));

    pwg->memory = mem;
    pwg->pgs = gs_gstate_alloc(mem);
    if (pwg->pgs == NULL)
        goto failVM;

    /* Push one save level onto the stack to assuage the memory handling */
    code = gs_gsave(pwg->pgs);
    if (code < 0)
        goto fail;

    code = gsicc_init_iccmanager(pwg->pgs);
    if (code < 0)
        goto fail;

    pwg->gray = gs_cspace_new_ICC(mem, pwg->pgs, 1);
    pwg->rgb  = gs_cspace_new_ICC(mem, pwg->pgs, 3);
    pwg->cmyk = gs_cspace_new_ICC(mem, pwg->pgs, 4);

    impl->interp_client_data = pwg;

    return 0;

failVM:
    code = gs_note_error(gs_error_VMerror);
fail:
    (void)pwg_deallocate(pwg);
    return code;
}

/*
 * Get the allocator with which to allocate a device
 */
static gs_memory_t *
pwg_impl_get_device_memory(pl_interp_implementation_t *impl)
{
    pwg_interp_instance_t *pwg = (pwg_interp_instance_t *)impl->interp_client_data;

    return pwg->dev ? pwg->dev->memory : NULL;
}

#if 0 /* UNUSED */
static int
pwg_impl_set_param(pl_interp_implementation_t *impl,
                   pl_set_param_type           type,
                   const char                 *param,
                   const void                 *val)
{
    pwg_interp_instance_t *pwg = (pwg_interp_instance_t *)impl->interp_client_data;

    /* No params set here */
    return 0;
}

static int
pwg_impl_add_path(pl_interp_implementation_t *impl,
                  const char                 *path)
{
    pwg_interp_instance_t *pwg = (pwg_interp_instance_t *)impl->interp_client_data;

    /* No paths to add */
    return 0;
}

static int
pwg_impl_post_args_init(pl_interp_implementation_t *impl)
{
    pwg_interp_instance_t *pwg = (pwg_interp_instance_t *)impl->interp_client_data;

    /* No post args processing */
    return 0;
}
#endif

/* Prepare interp instance for the next "job" */
static int
pwg_impl_init_job(pl_interp_implementation_t *impl,
                  gx_device                  *device)
{
    pwg_interp_instance_t *pwg = (pwg_interp_instance_t *)impl->interp_client_data;

    pwg->dev = device;
    pwg->state = ii_state_identifying;

    return 0;
}

#if 0 /* UNUSED */
static int
pwg_impl_run_prefix_commands(pl_interp_implementation_t *impl,
                             const char                 *prefix)
{
    pwg_interp_instance_t *pwg = (pwg_interp_instance_t *)impl->interp_client_data;

    return 0;
}

static int
pwg_impl_process_file(pl_interp_implementation_t *impl, const char *filename)
{
    pwg_interp_instance_t *pwg = (pwg_interp_instance_t *)impl->interp_client_data;

    return 0;
}
#endif

/* Do any setup for parser per-cursor */
static int                      /* ret 0 or +ve if ok, else -ve error code */
pwg_impl_process_begin(pl_interp_implementation_t * impl)
{
    return 0;
}

/* Ensure we have 'required' bytes to read, and further ensure
 * that we have no UEL's within those bytes. */
static int
ensure_bytes(pwg_interp_instance_t *pwg, stream_cursor_read *pr, int required)
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
pwg_impl_process(pl_interp_implementation_t * impl, stream_cursor_read * pr)
{
    pwg_interp_instance_t *pwg = (pwg_interp_instance_t *)impl->interp_client_data;
    int code = 0;
    gs_color_space *cs;

    /* Loop while we have bytes to read */
    while (pr->limit > pr->ptr)
    {
        switch(pwg->state)
        {
        case ii_state_identifying:
            /* Try and get us 4 bytes for "RaS2" */
            code = ensure_bytes(pwg, pr, 4);
            if (code < 0)
                return code;
            if (memcmp("RaS2", (const char *)pr->ptr+1, 4) == 0)
            {
                pr->ptr += 4;
                pwg->state = ii_state_pwg_header;
                break;
            }
            pwg->state = ii_state_flush;
            break;
        case ii_state_pwg_header:
            /* Try and get us 1796 bytes for the page header */
            code = ensure_bytes(pwg, pr, 1796);
            if (code < 0)
                return code;
	    pr->ptr          += 64;            /* CString PwgRaster */
	    pr->ptr          += 64;            /* CString MediaColor */
	    pr->ptr          += 64;            /* CString MediaType */
	    pr->ptr          += 64;            /* CString PrintContentOptimize */
	    pr->ptr          += 1-(256-267);   /* Reserved */
            (void)get32be(pr);                 /* WhenEnum CutMedia */
            pwg->duplexMode   = !!get32be(pr); /* Boolean Duplex */
	    pwg->xresolution  = get32be(pr);
	    pwg->yresolution  = get32be(pr);
	    pr->ptr          += 1-(284-299);   /* Reserved */
	    (void)!!get32be(pr);               /* Boolean InsertSheet */
	    (void)get32be(pr);                 /* WhenEnum Jog */
	    (void)get32be(pr);                 /* EdgeEnum LeadingEdge FeedDirection */
	    pr->ptr          += 1-(312-323);   /* Reserved */
	    (void)get32be(pr);                 /* MediaPosition */
	    (void)get32be(pr);                 /* MediaWeightMetric */
	    pr->ptr          += 1-(332-339);   /* Reserved */
	    pwg->copies       = get32be(pr);   /* NumCopies */
            if (pwg->copies == 0)
                pwg->copies = 1;
	    (void)get32be(pr);                 /* OrientationEnum Orientation OrientationRequested */
	    pr->ptr          += 1-(348-351);   /* Reserved */
	    (void)get32be(pr);                 /* UnsignedInt PageSize */
	    (void)get32be(pr);                 /* UnsignedInt PageSize */
	    pr->ptr          += 1-(360-367);   /* Reserved */
	    (void)!!get32be(pr);               /* Boolean Tumble */
            pwg->width        = get32be(pr);   /* Width */
            pwg->height       = get32be(pr);   /* Height */
	    pr->ptr          += 1-(380-383);   /* Reserved */
            pwg->bpc          = get32be(pr);   /* Bits Per Color */
            pwg->bpp          = get32be(pr);   /* Bits Per Pixel */
            pwg->bpl          = get32be(pr);   /* Bytes Per Line */
	    pwg->colororder   = get32be(pr);   /* ColorOrderEnum ColorOrder */
	    pwg->cs           = get32be(pr);   /* ColorSpaceEnum ColorSpace */
	    pr->ptr          += 1-(404-419);   /* Reserved */
	    (void)get32be(pr);                 /* NumColors */
	    pr->ptr          += 1-(424-451);   /* Reserved */
	    (void)get32be(pr);                 /* TotalPageCount */
	    (void)get32be(pr);                 /* CrossFeedTransform */
	    (void)get32be(pr);                 /* FeedTransform */
	    (void)get32be(pr);                 /* ImageBoxLeft */
	    (void)get32be(pr);                 /* ImageBoxTop */
	    (void)get32be(pr);                 /* ImageBoxRight */
	    (void)get32be(pr);                 /* ImageBoxBottom */
	    (void)get32be(pr);                 /* SrgbColor AlternatePrimary */
            pwg->printQuality = get32be(pr);   /* PrintQuality */
	    pr->ptr          += 1-(488-507);   /* Reserved */
	    (void)get32be(pr);                 /* VendorIdentifier */
	    (void)get32be(pr);                 /* VendorLength */
	    pr->ptr          += 1-(516-1603);  /* VendorData */
	    pr->ptr          += 1-(1604-1667); /* Reserved */
	    pr->ptr          += 1-(1668-1731); /* RenderingIntent */
	    pr->ptr          += 1-(1732-1795); /* PageSizeName */

            /* Decode the header */
	    if (pwg->colororder != 0)
	      goto bad_header; /* 0 = chunky = only defined one so far */

            switch(pwg->cs) {
            case 3:  /* DeviceBlack */
            case 18: /* Sgray */
            case 48: /* Device1 (Device color, 1 colorant) */
                pwg->num_comps = 1;
                cs = pwg->gray;
                break;
            case 1:  /* DeviceRGB */
            case 19: /* sRGB */
            case 20: /* AdobeRGB */
                pwg->num_comps = 3;
                cs = pwg->rgb;
                break;
            case 6: /* DeviceCMYK */
                pwg->num_comps = 4;
                cs = pwg->cmyk;
                break;
            default:
                goto bad_header;
            }
            if (pwg->bpc != 8)
                goto bad_header;
            if (pwg->bpp != 8*pwg->num_comps && pwg->bpp != 16*pwg->num_comps) {
bad_header:
                pwg->state = ii_state_flush;
                break;
            }
            pwg->byte_width = (pwg->bpp>>3)*pwg->width;
            if (pwg->bpl != pwg->byte_width)
                goto bad_header;

            pwg->nulldev = gs_currentdevice(pwg->pgs);
            rc_increment(pwg->nulldev);
            code = gs_setdevice_no_erase(pwg->pgs, pwg->dev);
            if (code < 0)
                goto early_flush;
            gs_initmatrix(pwg->pgs);

            /* By default the ctm is set to:
             *   xres/72   0
             *   0         -yres/72
             *   0         dev->height * yres/72
             * i.e. it moves the origin from being top right to being bottom left.
             * We want to move it back, as without this, the image will be displayed
             * upside down.
             */
            code = gs_translate(pwg->pgs, 0.0, pwg->dev->height * 72 / pwg->dev->HWResolution[1]);
            if (code >= 0)
                code = gs_scale(pwg->pgs, 1, -1);
            /* At this point, the ctm is set to:
             *   xres/72  0
             *   0        yres/72
             *   0        0
             */
            if (code >= 0)
                code = gs_erasepage(pwg->pgs);
            if (code < 0)
                goto early_flush;

            memset(&pwg->image, 0, sizeof(pwg->image));
            gs_image_t_init(&pwg->image, cs);
            pwg->image.BitsPerComponent = pwg->bpp/pwg->num_comps;
            pwg->image.Width = pwg->width;
            pwg->image.Height = pwg->height;

            pwg->image.ImageMatrix.xx = pwg->xresolution / 72.0f;
            pwg->image.ImageMatrix.yy = pwg->yresolution / 72.0f;

            pwg->penum = gs_image_enum_alloc(pwg->memory, "pwg_impl_process(penum)");
            if (pwg->penum == NULL) {
                code = gs_note_error(gs_error_VMerror);
                goto early_flush;
            }

            code = gs_image_init(pwg->penum,
                                 &pwg->image,
                                 false,
                                 false,
                                 pwg->pgs);
            if (code < 0)
                goto early_flush;

            s_init_state((stream_state *)&pwg->pwgd_state, &s_PWGD_template, pwg->memory);
            if (s_PWGD_template.set_defaults)
                s_PWGD_template.set_defaults((stream_state *)&pwg->pwgd_state);

            pwg->pwgd_state.width = pwg->width;
            pwg->pwgd_state.bpp = pwg->bpp;

            code = (s_PWGD_template.init)((stream_state *)&pwg->pwgd_state);
            if (code < 0)
                goto early_flush;
            pwg->x = pwg->y = 0;
            pwg->state = ii_state_pwg_data;
            break;
        case ii_state_pwg_data:
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
                local_w.ptr = pwg->stream_buffer-1;
                local_w.limit = local_w.ptr + sizeof(pwg->stream_buffer);

                status = (s_PWGD_template.process)((stream_state *)&pwg->pwgd_state,
                                                                   &local_r, &local_w,
                                                                   true);
                /* status = 0 => need data
                 *          1 => need output space
                 * but we don't actually use this currently. */
                (void)status;
                /* Copy the updated pointer back */
                pr->ptr = local_r.ptr;
                if (local_w.ptr + 1 == pwg->stream_buffer)
                    break; /* Failed to decode any data */
                decoded_any = 1;

                code = gs_image_next(pwg->penum, pwg->stream_buffer, local_w.ptr + 1 - pwg->stream_buffer, &used);
                if (code < 0)
                    goto flush;

                pwg->x += used;
                while (pwg->x >= pwg->byte_width) {
                    pwg->x -= pwg->byte_width;
                    pwg->y++;
                }
                /* Loop while we haven't had all the lines, the decompression
                 * didn't ask for more data, and the decompression didn't give
                 * us more data. */
            } while (pwg->y < pwg->height);

            if (pwg->y == pwg->height) {
                code = gs_image_cleanup_and_free_enum(pwg->penum, pwg->pgs);
                pwg->penum = NULL;
                if (code < 0)
                    goto flush;
                code = pl_finish_page(pwg->memory->gs_lib_ctx->top_of_system,
                                      pwg->pgs, pwg->copies, true);
                if (code < 0)
                    goto flush;
                if (pwg->pages > 0) {
                    pwg->pages--;
                    /* If we've reached the expected end, we should probably flush to UEL */
                    if (pwg->pages == 0)
                        pwg->state = ii_state_flush;
                }
                if (pwg->pwgd_state.templat->release)
                    pwg->pwgd_state.templat->release((stream_state *)&pwg->pwgd_state);
                pwg->state = ii_state_pwg_header;
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
                if (pwg->pwgd_state.templat->release)
                    pwg->pwgd_state.templat->release((stream_state *)&pwg->pwgd_state);
early_flush:
                pwg->state = ii_state_flush;
            }
            /* We want to bin any data we get up to, but not including
             * a UEL. */
            return flush_to_uel(pr);
        }
    }

    return code;
}

static int
pwg_impl_process_end(pl_interp_implementation_t * impl)
{
    return 0;
}

/* Not implemented */
static int
pwg_impl_flush_to_eoj(pl_interp_implementation_t *impl, stream_cursor_read *cursor)
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
pwg_impl_process_eof(pl_interp_implementation_t *impl)
{
    return 0;
}

/* Report any errors after running a job */
static int
pwg_impl_report_errors(pl_interp_implementation_t *impl,          /* interp instance to wrap up job in */
                       int                         code,          /* prev termination status */
                       long                        file_position, /* file position of error, -1 if unknown */
                       bool                        force_to_cout  /* force errors to cout */
)
{
    return 0;
}

/* Wrap up interp instance after a "job" */
static int
pwg_impl_dnit_job(pl_interp_implementation_t *impl)
{
    pwg_interp_instance_t *pwg = (pwg_interp_instance_t *)impl->interp_client_data;

    if (pwg->nulldev) {
        int code = gs_setdevice(pwg->pgs, pwg->nulldev);
        pwg->dev = NULL;
        rc_decrement(pwg->nulldev, "pwg_impl_dnit_job(nulldevice)");
        pwg->nulldev = NULL;
        return code;
    }
    return 0;
}

/* Parser implementation descriptor */
const pl_interp_implementation_t pwg_implementation = {
  pwg_impl_characteristics,
  pwg_impl_allocate_interp_instance,
  pwg_impl_get_device_memory,
  NULL, /* pwg_impl_set_param */
  NULL, /* pwg_impl_add_path */
  NULL, /* pwg_impl_post_args_init */
  pwg_impl_init_job,
  NULL, /* pwg_impl_run_prefix_commands */
  NULL, /* pwg_impl_process_file */
  pwg_impl_process_begin,
  pwg_impl_process,
  pwg_impl_process_end,
  pwg_impl_flush_to_eoj,
  pwg_impl_process_eof,
  pwg_impl_report_errors,
  pwg_impl_dnit_job,
  pwg_impl_deallocate_interp_instance,
  NULL, /* pwg_impl_reset */
  NULL  /* interp_client_data */
};
