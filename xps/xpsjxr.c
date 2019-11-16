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


/* JPEG-XR (formerly HD-Photo (formerly Windows Media Photo)) support */

#include "ghostxps.h"

#ifdef _MSC_VER
#undef _MSC_VER
#endif

#include "jpegxr.h"

struct state { xps_context_t *ctx; xps_image_t *output; };

static const char *
jxr_error_string(int code)
{
    switch (code)
    {
    case JXR_EC_OK: return "No error";
    default:
    case JXR_EC_ERROR: return "Unspecified error";
    case JXR_EC_BADMAGIC: return "Stream lacks proper magic number";
    case JXR_EC_FEATURE_NOT_IMPLEMENTED: return "Feature not implemented";
    case JXR_EC_IO: return "Error reading/writing data";
    case JXR_EC_BADFORMAT: return "Bad file format";
    }
}

#define CLAMP(v, mn, mx) (v < mn ? mn : v > mx ? mx : v)

static inline int
scale_bits(int depth, int value)
{
    union { int iv; float fv; } bd32f;

    switch (depth)
    {
    case JXR_BD1WHITE1:
        return value * 255;
    case JXR_BD1BLACK1:
        return value ? 0 : 255;
    case JXR_BD8:
        return value;
    case JXR_BD16:
        return value >> 8;
    case JXR_BD16S: /* -4 .. 4 ; 8192 = 1.0 */
        value = value >> 5;
        return CLAMP(value, 0, 255);
    case JXR_BD32S: /* -128 .. 128 ; 16777216 = 1.0 */
        value = value >> 16;
        return CLAMP(value, 0, 255);
    case JXR_BD32F:
        bd32f.iv = value;
        value = (int)(bd32f.fv * 255);
        return CLAMP(value, 0, 255);
#if 0
    case JXR_BDRESERVED: return value;
    case JXR_BD16F: return value;
    case JXR_BD5: return value;
    case JXR_BD10: return value;
    case JXR_BD565: return value;
#endif
    }
    return value;
}

static void
xps_decode_jpegxr_block(jxr_image_t image, int mx, int my, int *data)
{
    struct state *state = jxr_get_user_data(image);
    xps_context_t *ctx = state->ctx;
    xps_image_t *output = state->output;
    int depth;
    unsigned char *p;
    int x, y, k;

    if (!output->samples)
    {
        gs_color_space *old_cs;
        output->width = jxr_get_IMAGE_WIDTH(image);
        output->height = jxr_get_IMAGE_HEIGHT(image);
        output->comps = jxr_get_IMAGE_CHANNELS(image);
        output->hasalpha = jxr_get_ALPHACHANNEL_FLAG(image);
        output->bits = 8;
        output->stride = output->width * output->comps;
        output->samples = xps_alloc(ctx, output->stride * output->height);
        if (!output->samples) {
            gs_throw(gs_error_VMerror, "out of memory: output->samples.\n");
            return;
        }

        old_cs = output->colorspace;
        switch (output->comps)
        {
        default:
        case 1: output->colorspace = ctx->gray; break;
        case 3: output->colorspace = ctx->srgb; break;
        case 4: output->colorspace = ctx->cmyk; break;
        }
        rc_increment(output->colorspace);
        rc_decrement(old_cs, "xps_decode_jpegxr_block");
    }

    depth = jxr_get_OUTPUT_BITDEPTH(image);

    my = my * 16;
    mx = mx * 16;

    for (y = 0; y < 16; y++)
    {
        if (my + y >= output->height)
            return;
        p = output->samples + (my + y) * output->stride + mx * output->comps;
        for (x = 0; x < 16; x++)
        {
            if (mx + x >= output->width)
                data += output->comps;
            else
                for (k = 0; k < output->comps; k++)
                    *p++ = scale_bits(depth, *data++);
        }
    }
}

static void
xps_decode_jpegxr_alpha_block(jxr_image_t image, int mx, int my, int *data)
{
    struct state *state = jxr_get_user_data(image);
    xps_context_t *ctx = state->ctx;
    xps_image_t *output = state->output;
    int depth;
    unsigned char *p;
    int x, y;

    if (!output->alpha)
    {
        output->alpha = xps_alloc(ctx, output->width * output->height);
        if (!output->alpha) {
            gs_throw(gs_error_VMerror, "out of memory: output->alpha.\n");
            return;
        }
    }

    depth = jxr_get_OUTPUT_BITDEPTH(image);

    my = my * 16;
    mx = mx * 16;

    for (y = 0; y < 16; y++)
    {
        if (my + y >= output->height)
            return;
        p = output->alpha + (my + y) * output->width + mx;
        for (x = 0; x < 16; x++)
        {
            if (mx + x >= output->width)
                data ++;
            else
                *p++ = scale_bits(depth, *data++);
        }
    }
}

int
xps_decode_jpegxr(xps_context_t *ctx, byte *buf, int len, xps_image_t *output)
{
    gp_file *file;
    char *name = xps_alloc(ctx, gp_file_name_sizeof);
    struct state state;
    jxr_container_t container;
    jxr_image_t image;
    int offset, alpha_offset;
    int rc;

    if (!name) {
        return gs_throw(gs_error_VMerror, "cannot allocate scratch file name buffer");
    }

    memset(output, 0, sizeof(*output));

    file = gp_open_scratch_file(ctx->memory, "jpegxr-scratch-", name, "wb+");
    if (!file) {
        xps_free(ctx, name);
        return gs_throw(gs_error_invalidfileaccess, "cannot open scratch file");
    }
    rc = gp_fwrite(buf, 1, len, file);
    if (rc != len) {
        xps_free(ctx, name);
        return gs_throw(gs_error_invalidfileaccess, "cannot write to scratch file");
    }
    rc = xps_fseek(file, 0, SEEK_SET);
    if (rc != 0) {
        xps_free(ctx, name);
        return gs_throw(gs_error_invalidfileaccess, "cannot write to scratch file");
    }

    container = jxr_create_container();
    rc = jxr_read_image_container(container, gp_get_file(file));
    if (rc < 0) {
        xps_free(ctx, name);
        jxr_destroy_container(container);
        return gs_throw1(-1, "jxr_read_image_container: %s", jxr_error_string(rc));
    }

    offset = jxrc_image_offset(container, 0);
    alpha_offset = jxrc_alpha_offset(container, 0);

    output->xres = (int)jxrc_width_resolution(container, 0);
    output->yres = (int)jxrc_height_resolution(container, 0);

    image = jxr_create_input();
    jxr_set_PROFILE_IDC(image, 111);
    jxr_set_LEVEL_IDC(image, 255);
    jxr_set_pixel_format(image, jxrc_image_pixelformat(container, 0));
    jxr_set_container_parameters(image,
        jxrc_image_pixelformat(container, 0),
        jxrc_image_width(container, 0),
        jxrc_image_height(container, 0),
        jxrc_alpha_offset(container, 0),
        jxrc_image_band_presence(container, 0),
        jxrc_alpha_band_presence(container, 0), 0);

    jxr_set_block_output(image, xps_decode_jpegxr_block);
    state.ctx = ctx;
    state.output = output;
    jxr_set_user_data(image, &state);

    rc = xps_fseek(file, offset, SEEK_SET);
    if (rc != 0) {
        xps_free(ctx, name);
        jxr_destroy_container(container);
        jxr_destroy(image);
        return gs_throw1(-1, "jxr_read_image_bitstream: %s", jxr_error_string(rc));
    }

    rc = jxr_read_image_bitstream(image, gp_get_file(file));
    if (rc < 0) {
        xps_free(ctx, name);
        jxr_destroy_container(container);
        jxr_destroy(image);
        return gs_throw1(-1, "jxr_read_image_bitstream: %s", jxr_error_string(rc));
    }

    jxr_destroy(image);

    if (alpha_offset > 0)
    {
        image = jxr_create_input();
        jxr_set_PROFILE_IDC(image, 111);
        jxr_set_LEVEL_IDC(image, 255);
        jxr_set_pixel_format(image, jxrc_image_pixelformat(container, 0));
        jxr_set_container_parameters(image,
            jxrc_image_pixelformat(container, 0),
            jxrc_image_width(container, 0),
            jxrc_image_height(container, 0),
            jxrc_alpha_offset(container, 0),
            jxrc_image_band_presence(container, 0),
            jxrc_alpha_band_presence(container, 0), 0);

        jxr_set_block_output(image, xps_decode_jpegxr_alpha_block);
        state.ctx = ctx;
        state.output = output;
        jxr_set_user_data(image, &state);

        rc = xps_fseek(file, alpha_offset, SEEK_SET);
        if (rc != 0) {
            xps_free(ctx, name);
            jxr_destroy_container(container);
            jxr_destroy(image);
            return gs_throw1(-1, "jxr_read_image_bitstream: %s", jxr_error_string(rc));
        }

        rc = jxr_read_image_bitstream(image, gp_get_file(file));
        if (rc < 0) {
            xps_free(ctx, name);
            jxr_destroy_container(container);
            jxr_destroy(image);
            return gs_throw1(-1, "jxr_read_image_bitstream: %s", jxr_error_string(rc));
        }

        jxr_destroy(image);
    }

    jxr_destroy_container(container);

    gp_fclose(file);
    unlink(name);
    xps_free(ctx, name);

    return gs_okay;
}

int
xps_jpegxr_has_alpha(xps_context_t *ctx, byte *buf, int len)
{
    return 1;
}
