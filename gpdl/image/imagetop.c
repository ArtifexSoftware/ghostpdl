/* Copyright (C) 2019 Artifex Software, Inc.
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

/* imagetop.c */
/* Top-level API implementation of "Image" Language Interface */

#include "pltop.h"
#include "gserrors.h"
#include "gxdevice.h"
#include "gsstate.h"
#include "strimpl.h"
#include "gscoord.h"
#include "jpeglib.h"
#include "setjmp_.h"
#include "sjpeg.h"

/* Forward decls */

/************************************************************/
/******** Language wrapper implementation (see pltop.h) *****/
/************************************************************/

typedef enum
{
    ii_state_identifying = 0,
    ii_state_jpeg,
    ii_state_jpeg_header,
    ii_state_jpeg_start,
    ii_state_jpeg_rows,
    ii_state_jpeg_finish,
    ii_state_flush
} ii_state;

typedef struct {
    struct jpeg_error_mgr pub;
    jmp_buf setjmp_buffer;
} img_error_mgr;

/*
 * Image interpreter instance
 */
typedef struct img_interp_instance_s {
    gs_memory_t       *memory;
    gs_memory_t       *cmemory;
    gx_device         *dev;
    gx_device         *nulldev;

    gs_color_space    *gray;
    gs_color_space    *rgb;
    gs_color_space    *cmyk;

    /* Image parser state machine */
    ii_state           state;

    int                pages;

    uint8_t            bpp;
    uint8_t            cs;
    uint32_t           width;
    uint32_t           height;
    uint32_t           xresolution;
    uint32_t           yresolution;

    uint32_t           num_comps;
    uint32_t           byte_width;
    uint32_t           y;

    uint32_t           bytes_available_on_entry;

    gs_image_t         image;
    gs_image_enum     *penum;
    gs_gstate         *pgs;

    struct jpeg_decompress_struct cinfo;
    struct jpeg_source_mgr        jsrc;
    size_t             bytes_to_skip;
    img_error_mgr      jerr;

    byte              *samples;

} img_interp_instance_t;

static int
img_detect_language(const char *s, int len)
{
    const byte *hdr = (const byte *)s;
    if (len >= 11) {
        if (hdr[0] == 0xFF && hdr[1] == 0xd8 && hdr[2] == 0xff && hdr[3] == 0xe0 &&
            strncmp("JFIF", s+6, 5) == 0)
            return 100;
    }
    /* FIXME: Other formats go here */

    return 0;
}

static const pl_interp_characteristics_t img_characteristics = {
    "IMAGE",
    img_detect_language,
    1 /* minimum input size */
};

/* GS's fakakta jpeg integration insists on putting a
 * memory structure pointer in the decompress structs client data.
 * This is no good to find our instance from. We therefore find
 * it by offsetting back from the address of the cinfo. This is
 * a nasty bit of casting, so wrap it in a macro. */
#define IMG_FROM_CINFO(CINFO) \
    (img_interp_instance_t *)(((char *)(CINFO))-offsetof(img_interp_instance_t,cinfo))


/* Get implementation's characteristics */
static const pl_interp_characteristics_t * /* always returns a descriptor */
img_impl_characteristics(const pl_interp_implementation_t *impl)     /* implementation of interpreter to alloc */
{
  return &img_characteristics;
}

static void
img_deallocate(img_interp_instance_t *img)
{
    if (img == NULL)
        return;

    rc_decrement_cs(img->gray, "img_deallocate");
    rc_decrement_cs(img->rgb, "img_deallocate");
    rc_decrement_cs(img->cmyk, "img_deallocate");

    if (img->pgs != NULL)
        gs_gstate_free_chain(img->pgs);
    gs_free_object(img->memory, img, "img_impl_allocate_interp_instance");
}

/* Deallocate a interpreter instance */
static int
img_impl_deallocate_interp_instance(pl_interp_implementation_t *impl)
{
    img_interp_instance_t *img = (img_interp_instance_t *)impl->interp_client_data;

    img_deallocate(img);
    impl->interp_client_data = NULL;

    return 0;
}

/* Do per-instance interpreter allocation/init. */
static int
img_impl_allocate_interp_instance(pl_interp_implementation_t *impl, gs_memory_t *mem)
{
    int code;
    img_interp_instance_t *img
        = (img_interp_instance_t *)gs_alloc_bytes(mem,
                                                  sizeof(img_interp_instance_t),
                                                  "img_impl_allocate_interp_instance");
    if (!img)
        return_error(gs_error_VMerror);
    memset(img, 0, sizeof(*img));

    img->memory = mem;
    img->pgs = gs_gstate_alloc(mem);
    if (img->pgs == NULL)
        goto failVM;

    /* Push one save level onto the stack to assuage the memory handling */
    code = gs_gsave(img->pgs);
    if (code < 0)
        goto fail;

    code = gsicc_init_iccmanager(img->pgs);
    if (code < 0)
        goto fail;

    img->gray = gs_cspace_new_ICC(mem, img->pgs, 1);
    img->rgb  = gs_cspace_new_ICC(mem, img->pgs, 3);
    img->cmyk = gs_cspace_new_ICC(mem, img->pgs, 4);

    impl->interp_client_data = img;

    return 0;

failVM:
    code = gs_note_error(gs_error_VMerror);
fail:
    (void)img_deallocate(img);
    return code;
}

/*
 * Get the allocator with which to allocate a device
 */
static gs_memory_t *
img_impl_get_device_memory(pl_interp_implementation_t *impl)
{
    img_interp_instance_t *img = (img_interp_instance_t *)impl->interp_client_data;

    return img->dev ? img->dev->memory : NULL;
}

#if 0 /* UNUSED */
static int
img_impl_set_param(pl_interp_implementation_t *impl,
                   pl_set_param_type           type,
                   const char                 *param,
                   const void                 *val)
{
    img_interp_instance_t *img = (img_interp_instance_t *)impl->interp_client_data;

    /* No params set here */
    return 0;
}

static int
img_impl_add_path(pl_interp_implementation_t *impl,
                  const char                 *path)
{
    img_interp_instance_t *img = (img_interp_instance_t *)impl->interp_client_data;

    /* No paths to add */
    return 0;
}

static int
img_impl_post_args_init(pl_interp_implementation_t *impl)
{
    img_interp_instance_t *img = (img_interp_instance_t *)impl->interp_client_data;

    /* No post args processing */
    return 0;
}
#endif

/* Prepare interp instance for the next "job" */
static int
img_impl_init_job(pl_interp_implementation_t *impl,
                  gx_device                  *device)
{
    img_interp_instance_t *img = (img_interp_instance_t *)impl->interp_client_data;

    img->dev = device;
    img->state = ii_state_identifying;

    return 0;
}

#if 0 /* UNUSED */
static int
img_impl_run_prefix_commands(pl_interp_implementation_t *impl,
                             const char                 *prefix)
{
    img_interp_instance_t *img = (img_interp_instance_t *)impl->interp_client_data;

    return 0;
}

static int
img_impl_process_file(pl_interp_implementation_t *impl, const char *filename)
{
    img_interp_instance_t *img = (img_interp_instance_t *)impl->interp_client_data;

    return 0;
}
#endif

/* Do any setup for parser per-cursor */
static int                      /* ret 0 or +ve if ok, else -ve error code */
img_impl_process_begin(pl_interp_implementation_t * impl)
{
    img_interp_instance_t *img = (img_interp_instance_t *)impl->interp_client_data;

    return 0;
}

/* Ensure we have 'required' bytes to read, and further ensure
 * that we have no UEL's within those bytes. */
static int
ensure_bytes(img_interp_instance_t *img, stream_cursor_read *pr, int required)
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

static void
img_jpeg_init_source(j_decompress_ptr cinfo)
{
    /* We've already inited the source */
}

static boolean
img_fill_input_buffer(j_decompress_ptr cinfo)
{
    return FALSE; /* We've filled the buffer already */
}

static void
img_skip_input_data(j_decompress_ptr cinfo, long num_bytes)
{
    img_interp_instance_t *img = IMG_FROM_CINFO(cinfo);
    size_t n = cinfo->src->bytes_in_buffer;

    if (n > num_bytes)
        n = num_bytes;
    img->jsrc.next_input_byte += num_bytes;
    img->jsrc.bytes_in_buffer -= n;
}

static boolean
img_resync_to_restart(j_decompress_ptr cinfo, int desired)
{
    return FALSE;
}

static void
img_term_source(j_decompress_ptr cinfo)
{
}

static void
img_error_exit(j_common_ptr cinfo)
{
    img_error_mgr *jerr = (img_error_mgr *)cinfo->err;

    longjmp(jerr->setjmp_buffer, 1);
}

static int
fill_jpeg_source(img_interp_instance_t *img, stream_cursor_read * pr)
{
    size_t n = pr->limit - pr->ptr;
    size_t skip = img->bytes_to_skip;

    /* Skip any bytes we are supposed to be skipping. */
    if (skip > 0) {
        if (skip > n)
            skip = n;
        pr->ptr += skip;
        n -= skip;
        img->bytes_to_skip -= skip;
        if (img->bytes_to_skip != 0)
            return 1; /* Still more to skip */
    }

    /* Set up for the call into the jpeg lib */
    img->jsrc.next_input_byte = pr->ptr+1;
    img->jsrc.bytes_in_buffer = pr->limit - pr->ptr;
    img->bytes_available_on_entry = img->jsrc.bytes_in_buffer;

    return 0;
}

static int
consume_jpeg_data(img_interp_instance_t *img, stream_cursor_read *pr)
{
    size_t bytes_read = img->jsrc.next_input_byte - (pr->ptr+1);
    size_t n = pr->limit - pr->ptr;

    if (n > bytes_read)
        n = bytes_read;
    pr->ptr += n;
    bytes_read -= n;

    /* We need to skip the next bytes_read bytes */
    img->bytes_to_skip = bytes_read;

    return (pr->limit - pr->ptr == img->bytes_available_on_entry);
}

static int
img_impl_process(pl_interp_implementation_t * impl, stream_cursor_read * pr)
{
    img_interp_instance_t *img = (img_interp_instance_t *)impl->interp_client_data;
    int code = 0;
    int need_more_data;

    do
    {
        need_more_data = 0;
        switch(img->state)
        {
        case ii_state_identifying:
        {
            const byte *hdr;
            /* Try and get us 11 bytes */
            code = ensure_bytes(img, pr, 11);
            if (code < 0)
                return code;
            hdr = pr->ptr+1;
            if (hdr[0] == 0xFF && hdr[1] == 0xd8 && hdr[2] == 0xff && hdr[3] == 0xe0 &&
                memcmp("JFIF", hdr+6, 5) == 0)
            {
                img->state = ii_state_jpeg;
                break;
            }
            img->state = ii_state_flush;
            break;
        }
        case ii_state_jpeg:
            code = gs_jpeg_mem_init(img->memory, (j_common_ptr)&img->cinfo);
            if (code < 0) {
                img->state = ii_state_flush;
                break;
            }

            img->cinfo.err = jpeg_std_error(&img->jerr.pub);
            img->jerr.pub.error_exit = img_error_exit;
            if (setjmp(img->jerr.setjmp_buffer)) {
                img->state = ii_state_flush;
                break;
            }

            jpeg_create_decompress(&img->cinfo);

            img->cinfo.src = &img->jsrc;
            img->jsrc.init_source = img_jpeg_init_source;
            img->jsrc.fill_input_buffer = img_fill_input_buffer;
            img->jsrc.skip_input_data = img_skip_input_data;
            img->jsrc.resync_to_restart = img_resync_to_restart;
            img->jsrc.term_source = img_term_source;

            img->state = ii_state_jpeg_header;
            break;
        case ii_state_jpeg_header:
        {
            int ok;
            gs_color_space *cs;
            float scale, xext, yext, xoffset, yoffset;

            if (fill_jpeg_source(img, pr)) {
                need_more_data = 1;
                break; /* No bytes left after skipping */
            }
            if (setjmp(img->jerr.setjmp_buffer)) {
                jpeg_destroy_decompress(&img->cinfo);
                img->state = ii_state_flush;
                break;
            }
            ok = jpeg_read_header(&img->cinfo, TRUE);
            need_more_data = consume_jpeg_data(img, pr);
            if (ok == JPEG_SUSPENDED)
                break;
            need_more_data = 0;

            img->width = img->cinfo.image_width;
            img->height = img->cinfo.image_height;
            img->num_comps = img->cinfo.num_components;
            img->bpp = 8 * img->num_comps;
            switch(img->num_comps) {
            default:
            case 1:
                cs = img->gray;
                img->cinfo.out_color_space = JCS_GRAYSCALE;
                break;
            case 3:
                cs = img->rgb;
                img->cinfo.out_color_space = JCS_RGB;
                break;
            case 4:
                cs = img->cmyk;
                img->cinfo.out_color_space = JCS_CMYK;
                break;
            }

            /* Find us some X and Y resolutions */
            img->xresolution = img->cinfo.X_density;
            img->yresolution = img->cinfo.Y_density;
            if (img->xresolution == 0)
                img->xresolution = img->yresolution;
            if (img->yresolution == 0)
                img->yresolution = img->xresolution;
            if (img->xresolution == 0)
                img->xresolution = 72;
            if (img->yresolution == 0)
                img->yresolution = 72;

            /* Scale to fit, if too large. */
            scale = 1.0f;
            if (img->width * 72 > img->dev->width * img->xresolution)
                scale = ((float)img->dev->width * img->xresolution) / (img->width * 72);
            if (scale * img->height * 72 > img->dev->height * img->yresolution)
                scale = ((float)img->dev->height * img->yresolution) / (img->height * 72);

            /* Centre */
            xext = ((float)img->width * 72 * scale / img->xresolution);
            xoffset = (img->dev->width - xext)/2;
            yext = ((float)img->height * 72 * scale / img->yresolution);
            yoffset = (img->dev->height - yext)/2;


            /* Now we've got the data from the image header, we can
             * make the gs image instance */
            img->byte_width = (img->bpp>>3)*img->width;

            img->nulldev = gs_currentdevice(img->pgs);
            rc_increment(img->nulldev);
            code = gs_setdevice_no_erase(img->pgs, img->dev);
            if (code < 0) {
                img->state = ii_state_flush;
                break;
            }
            gs_initmatrix(img->pgs);

            code = gs_translate(img->pgs, xoffset, img->dev->height-yoffset);
            if (code >= 0)
                code = gs_scale(img->pgs, scale, -scale);
            if (code >= 0)
                code = gs_erasepage(img->pgs);
            if (code < 0) {
                img->state = ii_state_flush;
                break;
            }

            img->samples = gs_alloc_bytes(img->memory, img->byte_width, "img_impl_process(samples)");
            if (img->samples == NULL) {
                img->state = ii_state_flush;
                break;
            }

            memset(&img->image, 0, sizeof(img->image));
            gs_image_t_init(&img->image, cs);
            img->image.BitsPerComponent = img->bpp/img->num_comps;
            img->image.Width = img->width;
            img->image.Height = img->height;

            img->image.ImageMatrix.xx = img->xresolution / 72.0;
            img->image.ImageMatrix.yy = img->yresolution / 72.0;

            img->penum = gs_image_enum_alloc(img->memory, "img_impl_process(penum)");
            if (img->penum == NULL) {
                code = gs_note_error(gs_error_VMerror);
                img->state = ii_state_flush;
                return code;
            }

            code = gs_image_init(img->penum,
                                 &img->image,
                                 false,
                                 false,
                                 img->pgs);
            if (code < 0) {
                img->state = ii_state_flush;
                return code;
            }

            img->state = ii_state_jpeg_start;
            break;
        }
        case ii_state_jpeg_start:
        {
            int ok;
            if (fill_jpeg_source(img, pr)) {
                need_more_data = 1;
                break; /* No bytes left after skipping */
            }
            if (setjmp(img->jerr.setjmp_buffer)) {
                jpeg_destroy_decompress(&img->cinfo);
                img->state = ii_state_flush;
                break;
            }
            ok = jpeg_start_decompress(&img->cinfo);
            (void)consume_jpeg_data(img, pr);
            if (ok == FALSE)
                break;
            img->state = ii_state_jpeg_rows;
            break;
        }
        case ii_state_jpeg_rows:
        {
            int rows_decoded;
            unsigned int used;

            if (fill_jpeg_source(img, pr)) {
                need_more_data = 1;
                break; /* No bytes left after skipping */
            }
            if (setjmp(img->jerr.setjmp_buffer)) {
                jpeg_destroy_decompress(&img->cinfo);
                img->state = ii_state_flush;
                break;
            }
            rows_decoded = jpeg_read_scanlines(&img->cinfo, &img->samples, 1);
            need_more_data = consume_jpeg_data(img, pr);
            if (rows_decoded == 0)
                break; /* Not enough data for a scanline yet */
            need_more_data = 0;

            code = gs_image_next(img->penum, img->samples, img->byte_width, &used);
            if (code < 0) {
                img->state = ii_state_flush;
                break;
            }
            img->y++;
            if (img->y == img->height) {
                code = gs_image_cleanup_and_free_enum(img->penum, img->pgs);
                img->penum = NULL;
                if (code < 0) {
                    img->state = ii_state_flush;
                    break;
                }
                code = pl_finish_page(img->memory->gs_lib_ctx->top_of_system,
                                      img->pgs, 1, true);
                if (code < 0) {
                    img->state = ii_state_flush;
                    break;
                }
                img->state = ii_state_jpeg_finish;
            }
            break;
        }
        case ii_state_jpeg_finish:
        {
            int ok;

            if (fill_jpeg_source(img, pr)) {
                need_more_data = 1;
                break; /* No bytes left after skipping */
            }
            if (setjmp(img->jerr.setjmp_buffer)) {
                jpeg_destroy_decompress(&img->cinfo);
                img->state = ii_state_flush;
                break;
            }
            ok = jpeg_finish_decompress(&img->cinfo);
            need_more_data = consume_jpeg_data(img, pr);
            if (ok == FALSE)
                break;
            need_more_data = 0;
            img->state = ii_state_flush;
            break;
        }
        default:
        case ii_state_flush:
            if (setjmp(img->jerr.setjmp_buffer))
                break;
            jpeg_destroy_decompress(&img->cinfo);

            gs_jpeg_mem_term((j_common_ptr)&img->cinfo);

            if (img->penum) {
                (void)gs_image_cleanup_and_free_enum(img->penum, img->pgs);
                img->penum = NULL;
            }

            gs_free_object(img->memory, img->samples, "img_impl_process(samples)");
            img->samples = NULL;
            /* We want to bin any data we get up to, but not including
             * a UEL. */
            return flush_to_uel(pr);
        }
    } while (!need_more_data);

    return code;
}

static int
img_impl_process_end(pl_interp_implementation_t * impl)
{
    img_interp_instance_t *img = (img_interp_instance_t *)impl->interp_client_data;
    int code = 0;

    /* FIXME: */
    if (code == gs_error_InterpreterExit || code == gs_error_NeedInput)
        code = 0;

    return code;
}

/* Not implemented */
static int
img_impl_flush_to_eoj(pl_interp_implementation_t *impl, stream_cursor_read *cursor)
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
img_impl_process_eof(pl_interp_implementation_t *impl)
{
    return 0;
}

/* Report any errors after running a job */
static int
img_impl_report_errors(pl_interp_implementation_t *impl,          /* interp instance to wrap up job in */
                       int                         code,          /* prev termination status */
                       long                        file_position, /* file position of error, -1 if unknown */
                       bool                        force_to_cout  /* force errors to cout */
)
{
    return 0;
}

/* Wrap up interp instance after a "job" */
static int
img_impl_dnit_job(pl_interp_implementation_t *impl)
{
    img_interp_instance_t *img = (img_interp_instance_t *)impl->interp_client_data;

    if (img->nulldev) {
        int code = gs_setdevice(img->pgs, img->nulldev);
        img->dev = NULL;
        rc_decrement(img->nulldev, "img_impl_dnit_job(nulldevice)");
        img->nulldev = NULL;
        return code;
    }
    return 0;
}

/* Parser implementation descriptor */
const pl_interp_implementation_t img_implementation = {
  img_impl_characteristics,
  img_impl_allocate_interp_instance,
  img_impl_get_device_memory,
  NULL, /* img_impl_set_param */
  NULL, /* img_impl_add_path */
  NULL, /* img_impl_post_args_init */
  img_impl_init_job,
  NULL, /* img_impl_run_prefix_commands */
  NULL, /* img_impl_process_file */
  img_impl_process_begin,
  img_impl_process,
  img_impl_process_end,
  img_impl_flush_to_eoj,
  img_impl_process_eof,
  img_impl_report_errors,
  img_impl_dnit_job,
  img_impl_deallocate_interp_instance,
  NULL
};
