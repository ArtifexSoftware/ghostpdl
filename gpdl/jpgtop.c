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

/* jpgtop.c */
/* Top-level API implementation of "JPG" Language Interface */

#include "pltop.h"
#include "gserrors.h"
#include "gxdevice.h"
#include "gsstate.h"
#include "strimpl.h"
#include "gscoord.h"
#include "gsicc_manage.h"
#include "gspaint.h"
#include "plmain.h"
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
    jmp_buf *setjmp_buffer;
} jpg_error_mgr;

/*
 * Jpg interpreter instance
 */
typedef struct jpg_interp_instance_s {
    gs_memory_t       *memory;
    gs_memory_t       *cmemory;
    gx_device         *dev;
    gx_device         *nulldev;

    gs_color_space    *gray;
    gs_color_space    *rgb;
    gs_color_space    *cmyk;

    /* Jpg parser state machine */
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
    jpg_error_mgr      jerr;

    struct
    {
        jmp_buf setjmp_buffer;
        unsigned char pad[16];
    } aligned_jmpbuf;


    byte              *samples;

} jpg_interp_instance_t;

static int
jpg_detect_language(const char *s, int len)
{
    const byte *hdr = (const byte *)s;
    if (len >= 11) {
        if (hdr[0] == 0xFF && hdr[1] == 0xd8 && hdr[2] == 0xff && hdr[3] == 0xe0 &&
            strncmp("JFIF", s+6, 5) == 0)
            return 100;
    }

    return 0;
}

static const pl_interp_characteristics_t jpg_characteristics = {
    "JPG",
    jpg_detect_language,
};

/* GS's fakakta jpeg integration insists on putting a
 * memory structure pointer in the decompress structs client data.
 * This is no good to find our instance from. We therefore find
 * it by offsetting back from the address of the cinfo. This is
 * a nasty bit of casting, so wrap it in a macro. */
#define JPG_FROM_CINFO(CINFO) \
    (jpg_interp_instance_t *)(((char *)(CINFO))-offsetof(jpg_interp_instance_t,cinfo))


/* Get implementation's characteristics */
static const pl_interp_characteristics_t * /* always returns a descriptor */
jpg_impl_characteristics(const pl_interp_implementation_t *impl)     /* implementation of interpreter to alloc */
{
  return &jpg_characteristics;
}

static void
jpg_deallocate(jpg_interp_instance_t *jpg)
{
    if (jpg == NULL)
        return;

    rc_decrement_cs(jpg->gray, "jpg_deallocate");
    rc_decrement_cs(jpg->rgb, "jpg_deallocate");
    rc_decrement_cs(jpg->cmyk, "jpg_deallocate");

    if (jpg->pgs != NULL)
        gs_gstate_free_chain(jpg->pgs);
    gs_free_object(jpg->memory, jpg, "jpg_impl_allocate_interp_instance");
}

/* Deallocate a interpreter instance */
static int
jpg_impl_deallocate_interp_instance(pl_interp_implementation_t *impl)
{
    jpg_interp_instance_t *jpg = (jpg_interp_instance_t *)impl->interp_client_data;

    jpg_deallocate(jpg);
    impl->interp_client_data = NULL;

    return 0;
}

/* Do per-instance interpreter allocation/init. */
static int
jpg_impl_allocate_interp_instance(pl_interp_implementation_t *impl, gs_memory_t *mem)
{
    int code;
    jpg_interp_instance_t *jpg
        = (jpg_interp_instance_t *)gs_alloc_bytes(mem,
                                                  sizeof(jpg_interp_instance_t),
                                                  "jpg_impl_allocate_interp_instance");
    if (!jpg)
        return_error(gs_error_VMerror);
    memset(jpg, 0, sizeof(*jpg));

    jpg->memory = mem;
    jpg->pgs = gs_gstate_alloc(mem);
    if (jpg->pgs == NULL)
        goto failVM;

    /* Push one save level onto the stack to assuage the memory handling */
    code = gs_gsave(jpg->pgs);
    if (code < 0)
        goto fail;

    code = gsicc_init_iccmanager(jpg->pgs);
    if (code < 0)
        goto fail;

    jpg->gray = gs_cspace_new_ICC(mem, jpg->pgs, 1);
    jpg->rgb  = gs_cspace_new_ICC(mem, jpg->pgs, 3);
    jpg->cmyk = gs_cspace_new_ICC(mem, jpg->pgs, 4);

    impl->interp_client_data = jpg;

    return 0;

failVM:
    code = gs_note_error(gs_error_VMerror);
fail:
    (void)jpg_deallocate(jpg);
    return code;
}

/*
 * Get the allocator with which to allocate a device
 */
static gs_memory_t *
jpg_impl_get_device_memory(pl_interp_implementation_t *impl)
{
    jpg_interp_instance_t *jpg = (jpg_interp_instance_t *)impl->interp_client_data;

    return jpg->dev ? jpg->dev->memory : NULL;
}

#if 0 /* UNUSED */
static int
jpg_impl_set_param(pl_interp_implementation_t *impl,
                   pl_set_param_type           type,
                   const char                 *param,
                   const void                 *val)
{
    jpg_interp_instance_t *jpg = (jpg_interp_instance_t *)impl->interp_client_data;

    /* No params set here */
    return 0;
}

static int
jpg_impl_add_path(pl_interp_implementation_t *impl,
                  const char                 *path)
{
    jpg_interp_instance_t *jpg = (jpg_interp_instance_t *)impl->interp_client_data;

    /* No paths to add */
    return 0;
}

static int
jpg_impl_post_args_init(pl_interp_implementation_t *impl)
{
    jpg_interp_instance_t *jpg = (jpg_interp_instance_t *)impl->interp_client_data;

    /* No post args processing */
    return 0;
}
#endif

/* Prepare interp instance for the next "job" */
static int
jpg_impl_init_job(pl_interp_implementation_t *impl,
                  gx_device                  *device)
{
    jpg_interp_instance_t *jpg = (jpg_interp_instance_t *)impl->interp_client_data;

    jpg->dev = device;
    jpg->state = ii_state_identifying;

    return 0;
}

#if 0 /* UNUSED */
static int
jpg_impl_run_prefix_commands(pl_interp_implementation_t *impl,
                             const char                 *prefix)
{
    jpg_interp_instance_t *jpg = (jpg_interp_instance_t *)impl->interp_client_data;

    return 0;
}

static int
jpg_impl_process_file(pl_interp_implementation_t *impl, const char *filename)
{
    jpg_interp_instance_t *jpg = (jpg_interp_instance_t *)impl->interp_client_data;

    return 0;
}
#endif

/* Do any setup for parser per-cursor */
static int                      /* ret 0 or +ve if ok, else -ve error code */
jpg_impl_process_begin(pl_interp_implementation_t * impl)
{
    return 0;
}

/* Ensure we have 'required' bytes to read, and further ensure
 * that we have no UEL's within those bytes. */
static int
ensure_bytes(jpg_interp_instance_t *jpg, stream_cursor_read *pr, int required)
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

static void
jpg_jpeg_init_source(j_decompress_ptr cinfo)
{
    /* We've already inited the source */
}

static boolean
jpg_fill_input_buffer(j_decompress_ptr cinfo)
{
    return FALSE; /* We've filled the buffer already */
}

static void
jpg_skip_input_data(j_decompress_ptr cinfo, long num_bytes)
{
    jpg_interp_instance_t *jpg = JPG_FROM_CINFO(cinfo);
    size_t n = cinfo->src->bytes_in_buffer;

    if (n > num_bytes)
        n = num_bytes;
    jpg->jsrc.next_input_byte += num_bytes;
    jpg->jsrc.bytes_in_buffer -= n;
}

static boolean
jpg_resync_to_restart(j_decompress_ptr cinfo, int desired)
{
    return FALSE;
}

static void
jpg_term_source(j_decompress_ptr cinfo)
{
}

static void
jpg_error_exit(j_common_ptr cinfo)
{
    jpg_error_mgr *jerr = (jpg_error_mgr *)cinfo->err;

    longjmp(*jerr->setjmp_buffer, 1);
}

static int
fill_jpeg_source(jpg_interp_instance_t *jpg, stream_cursor_read * pr)
{
    size_t n = bytes_until_uel(pr);
    size_t skip = jpg->bytes_to_skip;

    /* Skip any bytes we are supposed to be skipping. */
    if (skip > 0) {
        if (skip > n)
            skip = n;
        pr->ptr += skip;
        n -= skip;
        jpg->bytes_to_skip -= skip;
        if (jpg->bytes_to_skip != 0)
            return 1; /* Still more to skip */
    }

    /* Set up for the call into the jpeg lib */
    jpg->jsrc.next_input_byte = pr->ptr+1;
    jpg->jsrc.bytes_in_buffer = pr->limit - pr->ptr;
    jpg->bytes_available_on_entry = jpg->jsrc.bytes_in_buffer;

    return 0;
}

static int
consume_jpeg_data(jpg_interp_instance_t *jpg, stream_cursor_read *pr)
{
    size_t bytes_read = jpg->jsrc.next_input_byte - (pr->ptr+1);
    size_t n = pr->limit - pr->ptr;

    if (n > bytes_read)
        n = bytes_read;
    pr->ptr += n;
    bytes_read -= n;

    /* We need to skip the next bytes_read bytes */
    jpg->bytes_to_skip = bytes_read;

    return (pr->limit - pr->ptr == jpg->bytes_available_on_entry);
}

/* Horrible hack. Windows appears to expect the jmpbuf to be 16 byte
 * aligned. Most 64bit platforms appear to return blocks from malloc
 * that are aligned to 16 byte boundaries. Our malloc routines only
 * return things to be 8 byte aligned on 64bit platforms, so we may
 * fail. Accordingly, we allocate the jmpbuf in a larger structure,
 * and align it.
 *
 * This is only a calculation we have to do once, so we just do it on
 * all platforms. */
static jmp_buf *
align_setjmp_buffer(void *ptr)
{
    intptr_t offset = (intptr_t)ptr;
    char *aligned;
    offset &= 15;
    offset = (16-offset) & 15;

    aligned = ((char *)ptr) + offset;

    return (jmp_buf *)(void *)aligned;
}

static int
jpg_impl_process(pl_interp_implementation_t * impl, stream_cursor_read * pr)
{
    jpg_interp_instance_t *jpg = (jpg_interp_instance_t *)impl->interp_client_data;
    int code = 0;
    int need_more_data;

    do
    {
        need_more_data = 0;
        switch(jpg->state)
        {
        case ii_state_identifying:
        {
            const byte *hdr;
            /* Try and get us 11 bytes */
            code = ensure_bytes(jpg, pr, 11);
            if (code < 0)
                return code;
            hdr = pr->ptr+1;
            if (hdr[0] == 0xFF && hdr[1] == 0xd8 && hdr[2] == 0xff && hdr[3] == 0xe0 &&
                memcmp("JFIF", hdr+6, 5) == 0)
            {
                jpg->state = ii_state_jpeg;
                break;
            }
            jpg->state = ii_state_flush;
            break;
        }
        case ii_state_jpeg:
            code = gs_jpeg_mem_init(jpg->memory, (j_common_ptr)&jpg->cinfo);
            if (code < 0) {
                jpg->state = ii_state_flush;
                break;
            }

            jpg->jerr.setjmp_buffer = align_setjmp_buffer(&jpg->aligned_jmpbuf);

            jpg->cinfo.err = jpeg_std_error(&jpg->jerr.pub);
            jpg->jerr.pub.error_exit = jpg_error_exit;
            if (setjmp(*jpg->jerr.setjmp_buffer)) {
                jpg->state = ii_state_flush;
                break;
            }

            jpeg_create_decompress(&jpg->cinfo);

            jpg->cinfo.src = &jpg->jsrc;
            jpg->jsrc.init_source = jpg_jpeg_init_source;
            jpg->jsrc.fill_input_buffer = jpg_fill_input_buffer;
            jpg->jsrc.skip_input_data = jpg_skip_input_data;
            jpg->jsrc.resync_to_restart = jpg_resync_to_restart;
            jpg->jsrc.term_source = jpg_term_source;

            jpg->state = ii_state_jpeg_header;
            break;
        case ii_state_jpeg_header:
        {
            int ok;
            gs_color_space *cs;
            float scale, xext, yext, xoffset, yoffset;

            if (fill_jpeg_source(jpg, pr)) {
                need_more_data = 1;
                break; /* No bytes left after skipping */
            }
            if (setjmp(*jpg->jerr.setjmp_buffer)) {
                jpeg_destroy_decompress(&jpg->cinfo);
                jpg->state = ii_state_flush;
                break;
            }
            ok = jpeg_read_header(&jpg->cinfo, TRUE);
            need_more_data = consume_jpeg_data(jpg, pr);
            if (ok == JPEG_SUSPENDED)
                break;
            need_more_data = 0;

            jpg->width = jpg->cinfo.image_width;
            jpg->height = jpg->cinfo.image_height;
            jpg->num_comps = jpg->cinfo.num_components;
            jpg->bpp = 8 * jpg->num_comps;
            switch(jpg->num_comps) {
            default:
            case 1:
                cs = jpg->gray;
                jpg->cinfo.out_color_space = JCS_GRAYSCALE;
                break;
            case 3:
                cs = jpg->rgb;
                jpg->cinfo.out_color_space = JCS_RGB;
                break;
            case 4:
                cs = jpg->cmyk;
                jpg->cinfo.out_color_space = JCS_CMYK;
                break;
            }

            /* Find us some X and Y resolutions */
            jpg->xresolution = jpg->cinfo.X_density;
            jpg->yresolution = jpg->cinfo.Y_density;
            if (jpg->xresolution == 0)
                jpg->xresolution = jpg->yresolution;
            if (jpg->yresolution == 0)
                jpg->yresolution = jpg->xresolution;
            if (jpg->xresolution == 0)
                jpg->xresolution = 72;
            if (jpg->yresolution == 0)
                jpg->yresolution = 72;

            /* Scale to fit, if too large. */
            scale = 1.0f;
            if (jpg->width * jpg->dev->HWResolution[0] > jpg->dev->width * jpg->xresolution)
                scale = ((float)jpg->dev->width * jpg->xresolution) / (jpg->width * jpg->dev->HWResolution[0]);
            if (scale * jpg->height * jpg->dev->HWResolution[1] > jpg->dev->height * jpg->yresolution)
                scale = ((float)jpg->dev->height * jpg->yresolution) / (jpg->height * jpg->dev->HWResolution[1]);

            /* Centre - Extents and offsets are all calculated in points (1/72 of an inch) */
            xext = ((float)jpg->width * 72 * scale / jpg->xresolution);
            xoffset = (jpg->dev->width * 72 / jpg->dev->HWResolution[0] - xext)/2;
            yext = ((float)jpg->height * 72 * scale / jpg->yresolution);
            yoffset = (jpg->dev->height * 72 / jpg->dev->HWResolution[1] - yext)/2;

            /* Now we've got the data from the image header, we can
             * make the gs image instance */
            jpg->byte_width = (jpg->bpp>>3)*jpg->width;

            jpg->nulldev = gs_currentdevice(jpg->pgs);
            rc_increment(jpg->nulldev);
            code = gs_setdevice_no_erase(jpg->pgs, jpg->dev);
            if (code < 0) {
                jpg->state = ii_state_flush;
                break;
            }
            gs_initmatrix(jpg->pgs);

            /* By default the ctm is set to:
             *   xres/72   0
             *   0         -yres/72
             *   0         dev->height * yres/72
             * i.e. it moves the origin from being top right to being bottom left.
             * We want to move it back, as without this, the image will be displayed
             * upside down.
             */
            code = gs_translate(jpg->pgs, 0.0, jpg->dev->height * 72 / jpg->dev->HWResolution[1]);
            if (code >= 0)
                code = gs_translate(jpg->pgs, xoffset, -yoffset);
            if (code >= 0)
                code = gs_scale(jpg->pgs, scale, -scale);
            /* At this point, the ctm is set to:
             *   xres/72  0
             *   0        yres/72
             *   0        0
             */
            if (code >= 0)
                code = gs_erasepage(jpg->pgs);
            if (code < 0) {
                jpg->state = ii_state_flush;
                break;
            }

            jpg->samples = gs_alloc_bytes(jpg->memory, jpg->byte_width, "jpg_impl_process(samples)");
            if (jpg->samples == NULL) {
                jpg->state = ii_state_flush;
                break;
            }

            memset(&jpg->image, 0, sizeof(jpg->image));
            gs_image_t_init(&jpg->image, cs);
            jpg->image.BitsPerComponent = jpg->bpp/jpg->num_comps;
            jpg->image.Width = jpg->width;
            jpg->image.Height = jpg->height;

            jpg->image.ImageMatrix.xx = jpg->xresolution / 72.0f;
            jpg->image.ImageMatrix.yy = jpg->yresolution / 72.0f;

            jpg->penum = gs_image_enum_alloc(jpg->memory, "jpg_impl_process(penum)");
            if (jpg->penum == NULL) {
                code = gs_note_error(gs_error_VMerror);
                jpg->state = ii_state_flush;
                return code;
            }

            code = gs_image_init(jpg->penum,
                                 &jpg->image,
                                 false,
                                 false,
                                 jpg->pgs);
            if (code < 0) {
                jpg->state = ii_state_flush;
                return code;
            }

            jpg->state = ii_state_jpeg_start;
            break;
        }
        case ii_state_jpeg_start:
        {
            int ok;
            if (fill_jpeg_source(jpg, pr)) {
                need_more_data = 1;
                break; /* No bytes left after skipping */
            }
            if (setjmp(*jpg->jerr.setjmp_buffer)) {
                jpeg_destroy_decompress(&jpg->cinfo);
                jpg->state = ii_state_flush;
                break;
            }
            ok = jpeg_start_decompress(&jpg->cinfo);
            (void)consume_jpeg_data(jpg, pr);
            if (ok == FALSE)
                break;
            jpg->state = ii_state_jpeg_rows;
            break;
        }
        case ii_state_jpeg_rows:
        {
            int rows_decoded;
            unsigned int used;

            if (fill_jpeg_source(jpg, pr)) {
                need_more_data = 1;
                break; /* No bytes left after skipping */
            }
            if (setjmp(*jpg->jerr.setjmp_buffer)) {
                jpeg_destroy_decompress(&jpg->cinfo);
                jpg->state = ii_state_flush;
                break;
            }
            rows_decoded = jpeg_read_scanlines(&jpg->cinfo, &jpg->samples, 1);
            need_more_data = consume_jpeg_data(jpg, pr);
            if (rows_decoded == 0)
                break; /* Not enough data for a scanline yet */
            need_more_data = 0;

            code = gs_image_next(jpg->penum, jpg->samples, jpg->byte_width, &used);
            if (code < 0) {
                jpg->state = ii_state_flush;
                break;
            }
            jpg->y++;
            if (jpg->y == jpg->height) {
                code = gs_image_cleanup_and_free_enum(jpg->penum, jpg->pgs);
                jpg->penum = NULL;
                if (code < 0) {
                    jpg->state = ii_state_flush;
                    break;
                }
                code = pl_finish_page(jpg->memory->gs_lib_ctx->top_of_system,
                                      jpg->pgs, 1, true);
                if (code < 0) {
                    jpg->state = ii_state_flush;
                    break;
                }
                jpg->state = ii_state_jpeg_finish;
            }
            break;
        }
        case ii_state_jpeg_finish:
        {
            int ok;

            if (fill_jpeg_source(jpg, pr)) {
                need_more_data = 1;
                break; /* No bytes left after skipping */
            }
            if (setjmp(*jpg->jerr.setjmp_buffer)) {
                jpeg_destroy_decompress(&jpg->cinfo);
                jpg->state = ii_state_flush;
                break;
            }
            ok = jpeg_finish_decompress(&jpg->cinfo);
            need_more_data = consume_jpeg_data(jpg, pr);
            if (ok == FALSE)
                break;
            need_more_data = 0;
            jpg->state = ii_state_flush;
            break;
        }
        default:
        case ii_state_flush:
            if (setjmp(*jpg->jerr.setjmp_buffer))
                break;
            jpeg_destroy_decompress(&jpg->cinfo);

            gs_jpeg_mem_term((j_common_ptr)&jpg->cinfo);

            if (jpg->penum) {
                (void)gs_image_cleanup_and_free_enum(jpg->penum, jpg->pgs);
                jpg->penum = NULL;
            }

            gs_free_object(jpg->memory, jpg->samples, "jpg_impl_process(samples)");
            jpg->samples = NULL;
            /* We want to bin any data we get up to, but not including
             * a UEL. */
            return flush_to_uel(pr);
        }
    } while (!need_more_data);

    return code;
}

static int
jpg_impl_process_end(pl_interp_implementation_t * impl)
{
    return 0;
}

/* Not implemented */
static int
jpg_impl_flush_to_eoj(pl_interp_implementation_t *impl, stream_cursor_read *cursor)
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
jpg_impl_process_eof(pl_interp_implementation_t *impl)
{
    return 0;
}

/* Report any errors after running a job */
static int
jpg_impl_report_errors(pl_interp_implementation_t *impl,          /* interp instance to wrap up job in */
                       int                         code,          /* prev termination status */
                       long                        file_position, /* file position of error, -1 if unknown */
                       bool                        force_to_cout  /* force errors to cout */
)
{
    return 0;
}

/* Wrap up interp instance after a "job" */
static int
jpg_impl_dnit_job(pl_interp_implementation_t *impl)
{
    jpg_interp_instance_t *jpg = (jpg_interp_instance_t *)impl->interp_client_data;

    if (jpg->nulldev) {
        int code = gs_setdevice(jpg->pgs, jpg->nulldev);
        jpg->dev = NULL;
        rc_decrement(jpg->nulldev, "jpg_impl_dnit_job(nulldevice)");
        jpg->nulldev = NULL;
        return code;
    }
    return 0;
}

/* Parser implementation descriptor */
const pl_interp_implementation_t jpg_implementation = {
  jpg_impl_characteristics,
  jpg_impl_allocate_interp_instance,
  jpg_impl_get_device_memory,
  NULL, /* jpg_impl_set_param */
  NULL, /* jpg_impl_add_path */
  NULL, /* jpg_impl_post_args_init */
  jpg_impl_init_job,
  NULL, /* jpg_impl_run_prefix_commands */
  NULL, /* jpg_impl_process_file */
  jpg_impl_process_begin,
  jpg_impl_process,
  jpg_impl_process_end,
  jpg_impl_flush_to_eoj,
  jpg_impl_process_eof,
  jpg_impl_report_errors,
  jpg_impl_dnit_job,
  jpg_impl_deallocate_interp_instance,
  NULL, /* jpg_impl_reset */
  NULL  /* interp_client_data */
};
