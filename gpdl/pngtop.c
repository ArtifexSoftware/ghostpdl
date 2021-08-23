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

/* pngtop.c */
/* Top-level API implementation of "PNG" Language Interface */

#include "pltop.h"
#include "gserrors.h"
#include "gxdevice.h"
#include "gsstate.h"
#include "strimpl.h"
#include "gscoord.h"
#include "gsicc_manage.h"
#include "gspaint.h"
#include "plmain.h"
#include "png_.h"

/* Forward decls */

/************************************************************/
/******** Language wrapper implementation (see pltop.h) *****/
/************************************************************/

typedef enum
{
    ii_state_identifying = 0,
    ii_state_png,
    ii_state_png_decode,
    ii_state_flush
} ii_state;

/*
 * Png interpreter instance
 */
typedef struct png_interp_instance_s {
    gs_memory_t       *memory;
    gs_memory_t       *cmemory;
    gx_device         *dev;
    gx_device         *nulldev;

    gs_color_space    *gray;
    gs_color_space    *rgb;

    /* Png parser state machine */
    ii_state           state;

    int                pages;

    uint8_t            bpp;
    uint8_t            cs;
    uint32_t           width;
    uint32_t           height;
    uint32_t           xresolution;
    uint32_t           yresolution;
    int                interlaced;

    uint32_t           num_comps;
    uint32_t           byte_width;
    uint32_t           y;
    uint32_t           passes;

    uint32_t           bytes_available_on_entry;

    gs_image_t         image;
    gs_image_enum     *penum;
    gs_gstate         *pgs;

    png_structp        png;
    png_infop          png_info;
    size_t             buffer_full;
    size_t             buffer_max;
    byte              *buffer;
    size_t             file_pos;

    byte              *samples;

} png_interp_instance_t;

static int
png_detect_language(const char *s, int len)
{
    const byte *hdr = (const byte *)s;
    if (len >= 8) {
        if (hdr[0] == 137 &&
            hdr[1] == 80 &&
            hdr[2] == 78 &&
            hdr[3] == 71 &&
            hdr[4] == 13 &&
            hdr[5] == 10 &&
            hdr[6] == 26 &&
            hdr[7] == 10)
            return 100;
    }

    return 0;
}

static const pl_interp_characteristics_t png_characteristics = {
    "PNG",
    png_detect_language,
};

/* Get implementation's characteristics */
static const pl_interp_characteristics_t * /* always returns a descriptor */
png_impl_characteristics(const pl_interp_implementation_t *impl)     /* implementation of interpreter to alloc */
{
  return &png_characteristics;
}

static void
png_deallocate(png_interp_instance_t *png)
{
    if (png == NULL)
        return;

    rc_decrement_cs(png->gray, "png_deallocate");
    rc_decrement_cs(png->rgb, "png_deallocate");

    if (png->pgs != NULL)
        gs_gstate_free_chain(png->pgs);
    gs_free_object(png->memory, png, "png_impl_allocate_interp_instance");
}

/* Deallocate a interpreter instance */
static int
png_impl_deallocate_interp_instance(pl_interp_implementation_t *impl)
{
    png_interp_instance_t *png = (png_interp_instance_t *)impl->interp_client_data;

    png_deallocate(png);
    impl->interp_client_data = NULL;

    return 0;
}

/* Do per-instance interpreter allocation/init. */
static int
png_impl_allocate_interp_instance(pl_interp_implementation_t *impl, gs_memory_t *mem)
{
    int code;
    png_interp_instance_t *png
        = (png_interp_instance_t *)gs_alloc_bytes(mem,
                                                  sizeof(png_interp_instance_t),
                                                  "png_impl_allocate_interp_instance");
    if (!png)
        return_error(gs_error_VMerror);
    memset(png, 0, sizeof(*png));

    png->memory = mem;
    png->pgs = gs_gstate_alloc(mem);
    if (png->pgs == NULL)
        goto failVM;

    /* Push one save level onto the stack to assuage the memory handling */
    code = gs_gsave(png->pgs);
    if (code < 0)
        goto fail;

    code = gsicc_init_iccmanager(png->pgs);
    if (code < 0)
        goto fail;

    png->gray = gs_cspace_new_ICC(mem, png->pgs, 1);
    png->rgb  = gs_cspace_new_ICC(mem, png->pgs, 3);

    impl->interp_client_data = png;

    return 0;

failVM:
    code = gs_note_error(gs_error_VMerror);
fail:
    (void)png_deallocate(png);
    return code;
}

/*
 * Get the allocator with which to allocate a device
 */
static gs_memory_t *
png_impl_get_device_memory(pl_interp_implementation_t *impl)
{
    png_interp_instance_t *png = (png_interp_instance_t *)impl->interp_client_data;

    return png->dev ? png->dev->memory : NULL;
}

/* Prepare interp instance for the next "job" */
static int
png_impl_init_job(pl_interp_implementation_t *impl,
                  gx_device                  *device)
{
    png_interp_instance_t *png = (png_interp_instance_t *)impl->interp_client_data;

    png->dev = device;
    png->state = ii_state_identifying;

    return 0;
}

/* Do any setup for parser per-cursor */
static int                      /* ret 0 or +ve if ok, else -ve error code */
png_impl_process_begin(pl_interp_implementation_t * impl)
{
    return 0;
}

/* Ensure we have 'required' bytes to read, and further ensure
 * that we have no UEL's within those bytes. */
static int
ensure_bytes(png_interp_instance_t *png, stream_cursor_read *pr, int required)
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
my_png_error(png_structp png_ptr, png_const_charp error_msg)
{
    /* png_interp_instance_t *png = (png_interp_instance_t *)png_get_error_ptr(png_ptr); */

    png_longjmp(png_ptr, 1);
}

static void
my_png_warning(png_structp png_ptr, png_const_charp warning_msg)
{
    /* png_interp_instance_t *png = (png_interp_instance_t *)png_get_error_ptr(png_ptr); */
}

static png_voidp
my_png_malloc(png_structp png_ptr, png_alloc_size_t size)
{
    png_interp_instance_t *png = (png_interp_instance_t *)png_get_mem_ptr(png_ptr);

    if (sizeof(void *) == 8) {
        /* gs_alloc_bytes returns blocks aligned to 8 on 64bit platforms.
         * PNG (on Windows at least) requires blocks aligned to 16. */
        unsigned char *block = gs_alloc_bytes(png->memory, size+16, "my_png_malloc");
        intptr_t num_bytes_padded;

        if (block == NULL)
            return NULL;
        num_bytes_padded = 16-(((intptr_t)block) & 15);
        block += num_bytes_padded;
        block[-1] = num_bytes_padded;

        return block;
    }
    return gs_alloc_bytes(png->memory, size, "my_png_malloc");
}

static void
my_png_free(png_structp png_ptr, png_voidp ptr)
{
    png_interp_instance_t *png = (png_interp_instance_t *)png_get_mem_ptr(png_ptr);

    if (sizeof(void *) == 8) {
        unsigned char *block = ptr;
        if (block == NULL)
            return;
        block -= block[-1];
        ptr = (void *)block;
    }
    gs_free_object(png->memory, ptr, "my_png_free");
}

static void
my_png_read(png_structp png_ptr, png_bytep data, png_size_t length)
{
    png_interp_instance_t *png = (png_interp_instance_t *)png_get_io_ptr(png_ptr);
    if (length + png->file_pos > png->buffer_full)
        png_error(png_ptr, "Overread!");

    memcpy(data, &png->buffer[png->file_pos], length);
    png->file_pos += length;
}

static int
do_impl_process(png_interp_instance_t *png, stream_cursor_read * pr, bool eof)
{
    int code = 0;
    ii_state ostate;
    size_t bytes_in;
    int advanced;

    /* Loop until we stop 'advancing'. */
    do
    {
        ostate = png->state;
        bytes_in = pr->limit - pr->ptr;
        advanced = 0;
        switch(png->state)
        {
        case ii_state_identifying:
        {
            const byte *hdr;
            /* Try and get us 8 bytes */
            code = ensure_bytes(png, pr, 8);
            if (code < 0)
                return code;
            hdr = pr->ptr+1;
            if (hdr[0] == 137 &&
                hdr[1] == 80 &&
                hdr[2] == 78 &&
                hdr[3] == 71 &&
                hdr[4] == 13 &&
                hdr[5] == 10 &&
                hdr[6] == 26 &&
                hdr[7] == 10) {
                png->state = ii_state_png;
                break;
            }
            png->state = ii_state_flush;
            break;
        }
        case ii_state_png:
        {
            /* Gather data into a buffer */
            int bytes = bytes_until_uel(pr);

            if (bytes == 0 && pr->limit - pr->ptr > 9) {
                /* No bytes until UEL, and there is space for a UEL in the buffer */
                png->state = ii_state_png_decode;
                png->file_pos = 0;
                break;
            }
            if (bytes == 0 && eof) {
                /* No bytes until UEL, and we are at eof */
                png->state = ii_state_png_decode;
                png->file_pos = 0;
                break;
            }

            if (png->buffer_full + bytes > png->buffer_max) {
                /* Need to expand our buffer */
                size_t proposed = png->buffer_full*2;
                if (proposed == 0)
                    proposed = 32768;
                while (proposed < png->buffer_full + bytes)
                    proposed *= 2;

                if (png->buffer == NULL) {
                    png->buffer = gs_alloc_bytes(png->memory, proposed, "png_buffer");
                    if (png->buffer == NULL) {
                        png->state = ii_state_flush;
                        break;
                    }
                } else {
                    void *new_buf = gs_resize_object(png->memory, png->buffer, proposed, "png_buffer");
                    if (new_buf == NULL) {
                        png->state = ii_state_flush;
                        break;
                    }
                    png->buffer = new_buf;
                }
                png->buffer_max = proposed;
            }

            memcpy(&png->buffer[png->buffer_full], pr->ptr+1, bytes);
            png->buffer_full += bytes;
            pr->ptr += bytes;
            break;
        }
        case ii_state_png_decode:
        {
            gs_color_space *cs;
            float scale, xext, yext, xoffset, yoffset;

            png->png = png_create_read_struct_2(
                               PNG_LIBPNG_VER_STRING,
                               (png_voidp)png,
                               my_png_error,
                               my_png_warning,
                               (png_voidp)png,
                               my_png_malloc,
                               my_png_free);
            if (png->png == NULL) {
                png->state = ii_state_flush;
                break;
            }

            png->png_info = png_create_info_struct(png->png);
            if (!png->png_info) {
                png->state = ii_state_flush;
                break;
            }

            png_set_read_fn(png->png, png, my_png_read);
            png_set_alpha_mode(png->png, PNG_ALPHA_PNG, PNG_DEFAULT_sRGB);

            if (setjmp(png_jmpbuf(png->png))) {
                png->state = ii_state_flush;
                break;
            }

            /* We use the "low level" interface to libpng to allow us to
             * optimise memory usage. Any errors will longjmp. */
            png_read_info(png->png, png->png_info);

            png_set_expand_16(png->png);
            png_set_alpha_mode(png->png, PNG_ALPHA_OPTIMIZED, PNG_DEFAULT_sRGB);
            {
                int bpc, color;
                static const png_color_16 bg = { 0, 65535, 65535, 65535, 65535 };
                png_get_IHDR(png->png, png->png_info,
                             &png->width,
                             &png->height,
                             &bpc,
                             &color,
                             &png->interlaced,
                             NULL /* compression */,
                             NULL /* filter */);
                switch (color) {
                case PNG_COLOR_TYPE_GRAY_ALPHA:
                case PNG_COLOR_TYPE_GRAY:
                    png->num_comps = 1;
                    break;
                case PNG_COLOR_TYPE_PALETTE:
                    png_set_palette_to_rgb(png->png);
                    png->num_comps = 3;
                    break;
                case PNG_COLOR_TYPE_RGB:
                case PNG_COLOR_TYPE_RGB_ALPHA:
                    png->num_comps = 3;
                    break;
                default:
                    png->state = ii_state_flush;
                    png_longjmp(png->png, 1);
                    break;
                }
                png->passes = png_set_interlace_handling(png->png);
                png_set_background(png->png, &bg, PNG_BACKGROUND_GAMMA_SCREEN, 0, 1);
            }

            png->xresolution = png_get_x_pixels_per_inch(png->png, png->png_info);
            png->yresolution = png_get_y_pixels_per_inch(png->png, png->png_info);
            if (png->xresolution == 0)
                png->xresolution = png->yresolution;
            if (png->yresolution == 0)
                png->yresolution = png->xresolution;
            if (png->xresolution == 0)
                png->xresolution = png->yresolution = 72;

            cs = (png->num_comps == 1 ? png->gray : png->rgb);

            /* Read the updated info */
            png_read_update_info(png->png, png->png_info);
            png->bpp = png_get_bit_depth(png->png, png->png_info) * png->num_comps;

            /* Scale to fit, if too large. */
            scale = 1.0f;
            if (png->width * png->dev->HWResolution[0] > png->dev->width * png->xresolution)
                scale = ((float)png->dev->width * png->xresolution) / (png->width * png->dev->HWResolution[0]);
            if (scale * png->height * png->dev->HWResolution[1] > png->dev->height * png->yresolution)
                scale = ((float)png->dev->height * png->yresolution) / (png->height * png->dev->HWResolution[1]);

            /* Centre - Extents and offsets are all calculated in points (1/72 of an inch) */
            xext = ((float)png->width * 72 * scale / png->xresolution);
            xoffset = (png->dev->width * 72 / png->dev->HWResolution[0] - xext)/2;
            yext = ((float)png->height * 72 * scale / png->yresolution);
            yoffset = (png->dev->height * 72 / png->dev->HWResolution[1] - yext)/2;

            /* Now we've got the data from the image header, we can
             * make the gs image instance */
            png->byte_width = png_get_rowbytes(png->png, png->png_info);

            png->nulldev = gs_currentdevice(png->pgs);
            rc_increment(png->nulldev);
            code = gs_setdevice_no_erase(png->pgs, png->dev);
            if (code < 0) {
                png->state = ii_state_flush;
                break;
            }
            gs_initmatrix(png->pgs);

            /* By default the ctm is set to:
             *   xres/72   0
             *   0         -yres/72
             *   0         dev->height * yres/72
             * i.e. it moves the origin from being top right to being bottom left.
             * We want to move it back, as without this, the image will be displayed
             * upside down.
             */
            code = gs_translate(png->pgs, 0.0, png->dev->height * 72 / png->dev->HWResolution[1]);
            if (code >= 0)
                code = gs_translate(png->pgs, xoffset, -yoffset);
            if (code >= 0)
                code = gs_scale(png->pgs, scale, -scale);
            /* At this point, the ctm is set to:
             *   xres/72  0
             *   0        yres/72
             *   0        0
             */
            if (code >= 0)
                code = gs_erasepage(png->pgs);
            if (code < 0) {
                png->state = ii_state_flush;
                break;
            }

            png->samples = gs_alloc_bytes(png->memory,
                                          (size_t)png->byte_width * (png->interlaced ? png->height : 1),
                                          "png_impl_process(samples)");
            if (png->samples == NULL) {
                png->state = ii_state_flush;
                break;
            }

            memset(&png->image, 0, sizeof(png->image));
            gs_image_t_init(&png->image, cs);
            png->image.BitsPerComponent = png->bpp/png->num_comps;
            png->image.Width = png->width;
            png->image.Height = png->height;

            png->image.ImageMatrix.xx = png->xresolution / 72.0f;
            png->image.ImageMatrix.yy = png->yresolution / 72.0f;

            png->penum = gs_image_enum_alloc(png->memory, "png_impl_process(penum)");
            if (png->penum == NULL) {
                code = gs_note_error(gs_error_VMerror);
                png->state = ii_state_flush;
                return code;
            }

            code = gs_image_init(png->penum,
                                 &png->image,
                                 false,
                                 false,
                                 png->pgs);
            if (code < 0) {
                png->state = ii_state_flush;
                return code;
            }

            {
                int i, j;

                if (png->interlaced) {
                    /* Collect the results from all but the last pass */
                    for (j = png->passes-1; j > 0; j--)
                        for (i = 0; i < png->height; i++)
                            png_read_row(png->png, &png->samples[i*png->byte_width], NULL);

                    /* And actually process the last pass */
                    for (i = 0; i < png->height; i++) {
                        uint used;

                        png_read_row(png->png, &png->samples[i*png->byte_width], NULL);

                        code = gs_image_next(png->penum, &png->samples[i*png->byte_width], png->byte_width, &used);
                        if (code < 0) {
                            png->state = ii_state_flush;
                            break;
                        }
                    }
                } else {
                    for (i = 0; i < png->height; i++) {
                        uint used;

                        png_read_row(png->png, png->samples, NULL);

                        code = gs_image_next(png->penum, png->samples, png->byte_width, &used);
                        if (code < 0) {
                            png->state = ii_state_flush;
                            break;
                        }
                    }
                }
            }

            code = gs_image_cleanup_and_free_enum(png->penum, png->pgs);
            png->penum = NULL;
            if (code < 0) {
                png->state = ii_state_flush;
                break;
            }
            code = pl_finish_page(png->memory->gs_lib_ctx->top_of_system,
                                  png->pgs, 1, true);
            if (code < 0) {
                png->state = ii_state_flush;
                break;
            }

            png->state = ii_state_flush;
            break;
        }
        default:
        case ii_state_flush:
            if (png->penum) {
                (void)gs_image_cleanup_and_free_enum(png->penum, png->pgs);
                png->penum = NULL;
            }

            gs_free_object(png->memory, png->samples, "png_impl_process(samples)");
            png->samples = NULL;
            /* We want to bin any data we get up to, but not including
             * a UEL. */
            return flush_to_uel(pr);
        }
        advanced |= (ostate != png->state);
        advanced |= (bytes_in != pr->limit - pr->ptr);
    } while (advanced);

    return code;
}

static int
png_impl_process(pl_interp_implementation_t * impl, stream_cursor_read * pr)
{
    png_interp_instance_t *png = (png_interp_instance_t *)impl->interp_client_data;

    return do_impl_process(png, pr, 0);
}

static int
png_impl_process_end(pl_interp_implementation_t * impl)
{
    return 0;
}

/* Not implemented */
static int
png_impl_flush_to_eoj(pl_interp_implementation_t *impl, stream_cursor_read *cursor)
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
png_impl_process_eof(pl_interp_implementation_t *impl)
{
    png_interp_instance_t *png = (png_interp_instance_t *)impl->interp_client_data;
    stream_cursor_read cursor;

    cursor.ptr = NULL;
    cursor.limit = NULL;

    return do_impl_process(png, &cursor, 1);
}

/* Report any errors after running a job */
static int
png_impl_report_errors(pl_interp_implementation_t *impl,          /* interp instance to wrap up job in */
                       int                         code,          /* prev termination status */
                       long                        file_position, /* file position of error, -1 if unknown */
                       bool                        force_to_cout  /* force errors to cout */
)
{
    return 0;
}

/* Wrap up interp instance after a "job" */
static int
png_impl_dnit_job(pl_interp_implementation_t *impl)
{
    png_interp_instance_t *png = (png_interp_instance_t *)impl->interp_client_data;

    if (png->nulldev) {
        int code = gs_setdevice(png->pgs, png->nulldev);
        png->dev = NULL;
        rc_decrement(png->nulldev, "png_impl_dnit_job(nulldevice)");
        png->nulldev = NULL;
        return code;
    }
    return 0;
}

/* Parser implementation descriptor */
const pl_interp_implementation_t png_implementation = {
  png_impl_characteristics,
  png_impl_allocate_interp_instance,
  png_impl_get_device_memory,
  NULL, /* png_impl_set_param */
  NULL, /* png_impl_add_path */
  NULL, /* png_impl_post_args_init */
  png_impl_init_job,
  NULL, /* png_impl_run_prefix_commands */
  NULL, /* png_impl_process_file */
  png_impl_process_begin,
  png_impl_process,
  png_impl_process_end,
  png_impl_flush_to_eoj,
  png_impl_process_eof,
  png_impl_report_errors,
  png_impl_dnit_job,
  png_impl_deallocate_interp_instance,
  NULL, /* png_impl_reset */
  NULL  /* interp_client_data */
};
