/* Copyright (C) 2001-2025 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  39 Mesa Street, Suite 108A, San Francisco,
   CA 94129, USA, for further information.
*/


/* Generic image enumeration and cleanup */
#include "gx.h"
#include "memory_.h"
#include "gserrors.h"
#include "gxdevice.h"
#include "gxcpath.h"
#include "gximage.h"
#include "gsicc_cache.h"
#include "gxgstate.h"

#ifdef WITH_CAL
#include "cal.h"
#endif

/* Forward declarations */
static void update_strip(gx_image_enum *penum);
static void repack_bit_planes(const gx_image_plane_t *src_planes,
                               const ulong *offsets, int num_planes,
                               byte *buffer, int width,
                               const sample_lookup_t * ptab, int spread);
static gx_device *setup_image_device(const gx_image_enum *penum);

/* Process the next piece of an ImageType 1 image. */
int
gx_image1_plane_data(gx_image_enum_common_t * info,
                     const gx_image_plane_t * planes, int height,
                     int *rows_used)
{
    gx_image_enum *penum = (gx_image_enum *) info;
    gx_device *dev;
    const int y = penum->y;
    int y_end = min(y + height, penum->rect.h);
    int width_spp = penum->rect.w * penum->spp;
    int num_planes = penum->num_planes;
    int num_components_per_plane = 1;

#define BCOUNT(plane)		/* bytes per data row */\
  (((penum->rect.w + (plane).data_x) * penum->spp * penum->bps / num_planes\
    + 7) >> 3)

    fixed adjust = penum->adjust;
    ulong offsets[GS_IMAGE_MAX_COMPONENTS];
    int ignore_data_x;
    bool bit_planar = penum->num_planes > penum->spp;
    int code;

    /* Sanity check */
    if (penum->pgs != NULL && penum->pgs->level < penum->pgs_level) {
        code = gs_note_error(gs_error_undefinedresult);
        goto out;
    }
    if (height == 0) {
        *rows_used = 0;
        return 0;
    }
    dev = setup_image_device(penum);

    /* Now render complete rows. */

    if (penum->used.y) {
        /*
         * Processing was interrupted by an error.  Skip over rows
         * already processed.
         */
        int px;

        for (px = 0; px < num_planes; ++px)
            offsets[px] = (size_t)planes[px].raster * penum->used.y;
        penum->used.y = 0;
    } else
        memset(offsets, 0, num_planes * sizeof(offsets[0]));
    if (num_planes == 1 && penum->plane_depths[0] != penum->bps) {
        /* A single plane with multiple components. */
        num_components_per_plane = penum->plane_depths[0] / penum->bps;
    }
    for (; penum->y < y_end; penum->y++) {
        int px;
        const byte *buffer;
        int sourcex;
        int x_used = penum->used.x;
        int skip = 0;

        /* Bump DDA's if it doesn't cause overflow */
        penum->cur.x = dda_current(penum->dda.row.x);
        if (max_int - any_abs(penum->dda.row.x.step.dQ) > any_abs(penum->cur.x))
            dda_next(penum->dda.row.x);
        penum->cur.y = dda_current(penum->dda.row.y);
        if (max_int - any_abs(penum->dda.row.y.step.dQ) > any_abs(penum->cur.y))
            dda_next(penum->dda.row.y);

        if (penum->interpolate == interp_off && penum->skip_next_line && penum->skip_next_line(penum, dev)) {
            goto mt;
        } else if (penum->skip_next_line == NULL) {
            /* Previously we skipped these calculations if we were interpolating.
             * Bug 706881 shows us that we can't skip this in all cases, because
             * the values are needed when halftoning for landscape files at least.
             * Easiest to always perform the calculations. */
            switch (penum->posture) {
                case image_portrait:
                    {    /* Precompute integer y and height, */
                        /* and check for clipping. */
                        fixed yc = penum->cur.y,
                              yn = dda_current(penum->dda.row.y);

                        if (yn < yc) {
                            fixed temp = yn;

                            yn = yc;
                            yc = temp;
                        }
                        yc -= adjust;
                        yn += adjust;
                        if (penum->interpolate == interp_off)
                        {
                            if (yc >= penum->clip_outer.q.y)
                                goto mt;
                            if (yn <= penum->clip_outer.p.y)
                                goto mt;
                        }
                        penum->yci = fixed2int_pixround_perfect(yc);
                        penum->hci = fixed2int_pixround_perfect(yn) - penum->yci;
                        if (penum->interpolate == interp_off && penum->hci == 0)
                            goto mt;
                        if_debug2m('b', penum->memory, "[b]yci=%d, hci=%d\n",
                                   penum->yci, penum->hci);
                    }
                    break;
                case image_landscape:
                    {    /* Check for no pixel centers in x. */
                        fixed xc = penum->cur.x,
                              xn = dda_current(penum->dda.row.x);

                        if (xn < xc) {
                            fixed temp = xn;

                            xn = xc;
                            xc = temp;
                        }
                        xc -= adjust;
                        xn += adjust;
                        if (penum->interpolate == interp_off)
                        {
                            if (xc >= penum->clip_outer.q.x)
                                goto mt;
                            if (xn <= penum->clip_outer.p.x)
                                goto mt;
                        }
                        penum->xci = fixed2int_pixround_perfect(xc);
                        penum->wci = fixed2int_pixround_perfect(xn) - penum->xci;
                        if (penum->interpolate == interp_off && penum->wci == 0)
                            goto mt;
                        if_debug2m('b', penum->memory, "[b]xci=%d, wci=%d\n",
                                   penum->xci, penum->wci);
                    }
                    break;
                case image_skewed:
                    ;
            }
        }
        if (0)
        {
        mt:
            skip = 1;
        }
        if (bit_planar) {
            /* Repack the bit planes into byte-wide samples. */

            buffer = penum->buffer;
            sourcex = 0;
            if (!skip)
                for (px = 0; px < num_planes; px += penum->bps)
                    repack_bit_planes(planes, offsets, penum->bps, penum->buffer,
                                      penum->rect.w, &penum->map[px].table,
                                      penum->spread);
            for (px = 0; px < num_planes; ++px)
                offsets[px] += planes[px].raster;
        } else {
            /*
             * Normally, we unpack the data into the buffer, but if
             * there is only one plane and we don't need to expand the
             * input samples, we may use the data directly.
             */
            sourcex = planes[0].data_x;
            if (!skip)
                buffer =
                    (*penum->unpack)(penum->buffer, &sourcex,
                                     planes[0].data + offsets[0],
                                     planes[0].data_x, BCOUNT(planes[0]),
                                     &penum->map[0], penum->spread, num_components_per_plane);
            else
                buffer = NULL;

            offsets[0] += planes[0].raster;
            for (px = 1; px < num_planes; ++px) {
                if (!skip)
                    (*penum->unpack)(penum->buffer + (px << penum->log2_xbytes),
                                     &ignore_data_x,
                                     planes[px].data + offsets[px],
                                     planes[px].data_x, BCOUNT(planes[px]),
                                     &penum->map[px], penum->spread, 1);
                offsets[px] += planes[px].raster;
            }
        }
#ifdef DEBUG
        if (gs_debug_c('b'))
            dmprintf1(dev->memory, "[b]image1 y=%d\n", y);
        if (gs_debug_c('B')) {
            int i, n = width_spp;
            byte *buftemp = (buffer == NULL) ? penum->buffer : (byte *)buffer;

            if (penum->bps > 8)
                n *= 2;
            else if (penum->bps == 1 && penum->unpack_bps == 8)
                n = (n + 7) / 8;
            dmlputs(dev->memory, "[B]row:");
            for (i = 0; i < n; i++)
                dmprintf1(dev->memory, " %02x", buftemp[i]);
            dmputs(dev->memory, "\n");
        }
#endif
        if (!skip)
        {
            update_strip(penum);
            if (x_used) {
                /*
                 * Processing was interrupted by an error.  Skip over pixels
                 * already processed.
                 */
                dda_advance(penum->dda.pixel0.x, x_used);
                dda_advance(penum->dda.pixel0.y, x_used);
                penum->used.x = 0;
            }
            if_debug2m('b', penum->memory, "[b]pixel0 x=%g, y=%g\n",
                       fixed2float(dda_current(penum->dda.pixel0.x)),
                       fixed2float(dda_current(penum->dda.pixel0.y)));
            code = (*penum->render)(penum, buffer, sourcex + x_used,
                                    width_spp - x_used * penum->spp, 1, dev);
            if (code < 0) {
                /* Error or interrupt, restore original state. */
                penum->used.x += x_used;
                if (!penum->used.y) {
                    dda_previous(penum->dda.row.x);
                    dda_previous(penum->dda.row.y);
                    dda_translate(penum->dda.strip.x,
                                  penum->prev.x - penum->cur.x);
                    dda_translate(penum->dda.strip.y,
                                  penum->prev.y - penum->cur.y);
                }
                goto out;
            }
            penum->prev = penum->cur;
        }
    }
    if (penum->y < penum->rect.h) {
        code = 0;
    } else {
        /* End of input data.  Render any left-over buffered data. */
        code = gx_image1_flush(info);
        if (code >= 0)
            code = 1;
    }
out:
    /* Note that caller must call end_image */
    /* for both error and normal termination. */
    *rows_used = penum->y - y;
    return code;
}

/* Flush any buffered data. */
int
gx_image1_flush(gx_image_enum_common_t * info)
{
    gx_image_enum *penum = (gx_image_enum *)info;
    int width_spp = penum->rect.w * penum->spp;
    fixed adjust = penum->adjust;

    penum->cur.x = dda_current(penum->dda.row.x);
    penum->cur.y = dda_current(penum->dda.row.y);
    switch (penum->posture) {
        case image_portrait:
            {
                fixed yc = penum->cur.y;

                penum->yci = fixed2int_rounded(yc - adjust);
                penum->hci = fixed2int_rounded(yc + adjust) - penum->yci;
            }
            break;
        case image_landscape:
            {
                fixed xc = penum->cur.x;

                penum->xci = fixed2int_rounded(xc - adjust);
                penum->wci = fixed2int_rounded(xc + adjust) - penum->xci;
            }
            break;
        case image_skewed:	/* pacify compilers */
            ;
    }
    update_strip(penum);
    penum->prev = penum->cur;
    return (*penum->render)(penum, NULL, 0, width_spp, 0,
                            setup_image_device(penum));
}

/* Update the strip DDA when moving to a new row. */
static void
update_strip(gx_image_enum *penum)
{

#if 1
    /* Old code. */
    dda_translate(penum->dda.strip.x, penum->cur.x - penum->prev.x);
    dda_translate(penum->dda.strip.y, penum->cur.y - penum->prev.y);
    penum->dda.pixel0 = penum->dda.strip;
#else
    /* A better precision with stromng dda_advance -
       doesn't work becauae gx_image1_plane_data
       doesn't call it at each step. */
    gx_dda_fixed_point temp;

    temp.x.state = penum->dda.strip.x.state;
    temp.y.state = penum->dda.strip.y.state;
    temp.x.step = penum->dda.row.x.step;
    temp.y.step = penum->dda.row.y.step;
    dda_next(temp.x);
    dda_next(temp.y);
    penum->dda.strip.x.state = temp.x.state;
    penum->dda.strip.y.state = temp.y.state;
    penum->dda.pixel0 = penum->dda.strip;
#endif
}

/*
 * Repack 1 to 8 individual bit planes into 8-bit samples.
 * buffer is aligned, and includes padding to an 8-byte boundary.
 * This procedure repacks one row, so the only relevant members of
 * src_planes are data and data_x (not raster).
 */
static void
repack_bit_planes(const gx_image_plane_t *src_planes, const ulong *offsets,
                  int num_planes, byte *buffer, int width,
                  const sample_lookup_t * ptab, int spread)
{
    gx_image_plane_t planes[8];
    byte *zeros = 0;
    byte *dest = buffer;
    int any_data_x = 0;
    bool direct = (spread == 1 && ptab->lookup8[0] == 0 &&
                   ptab->lookup8[255] == 255);
    int pi, x;
    gx_image_plane_t *pp;

    /*
     * Set up the row pointers, taking data_x and null planes into account.
     * If there are any null rows, we need to create a block of zeros in
     * order to avoid tests in the loop.
     */
    for (pi = 0, pp = planes; pi < num_planes; ++pi, ++pp)
        if (src_planes[pi].data == 0) {
            if (!zeros) {
                zeros = buffer + width - ((width + 7) >> 3);
            }
            pp->data = zeros;
            pp->data_x = 0;
        } else {
            int dx = src_planes[pi].data_x;

            pp->data = src_planes[pi].data + (dx >> 3) + offsets[pi];
            any_data_x |= (pp->data_x = dx & 7);
        }
    if (zeros)
        memset(zeros, 0, buffer + width - zeros);

    /*
     * Now process the data, in blocks of one input byte column
     * (8 output bytes).
     */
    for (x = 0; x < width; x += 8) {
        bits32 w0 = 0, w1 = 0;
#if ARCH_IS_BIG_ENDIAN
        static const bits32 expand[16] = {
            0x00000000, 0x00000001, 0x00000100, 0x00000101,
            0x00010000, 0x00010001, 0x00010100, 0x00010101,
            0x01000000, 0x01000001, 0x01000100, 0x01000101,
            0x01010000, 0x01010001, 0x01010100, 0x01010101
        };
#else
        static const bits32 expand[16] = {
            0x00000000, 0x01000000, 0x00010000, 0x01010000,
            0x00000100, 0x01000100, 0x00010100, 0x01010100,
            0x00000001, 0x01000001, 0x00010001, 0x01010001,
            0x00000101, 0x01000101, 0x00010101, 0x01010101
        };
#endif

        if (any_data_x) {
            for (pi = 0, pp = planes; pi < num_planes; ++pi, ++pp) {
                uint b = *(pp->data++);
                int dx = pp->data_x;

                if (dx) {
                    b <<= dx;
                    if (x + 8 - dx < width)
                        b += *pp->data >> (8 - dx);
                }
                w0 = (w0 << 1) | expand[b >> 4];
                w1 = (w1 << 1) | expand[b & 0xf];
            }
        } else {
            for (pi = 0, pp = planes; pi < num_planes; ++pi, ++pp) {
                uint b = *(pp->data++);

                w0 = (w0 << 1) | expand[b >> 4];
                w1 = (w1 << 1) | expand[b & 0xf];
            }
        }
        /*
         * We optimize spread == 1 and identity ptab together, although
         * we could subdivide these 2 cases into 4 if we wanted.
         */
        if (direct) {
            ((bits32 *)dest)[0] = w0;
            ((bits32 *)dest)[1] = w1;
            dest += 8;
        } else {
#define MAP_BYTE(v) (ptab->lookup8[(byte)(v)])
            dest[0] = MAP_BYTE(w0 >> 24); dest += spread;
            dest[1] = MAP_BYTE(w0 >> 16); dest += spread;
            dest[2] = MAP_BYTE(w0 >> 8); dest += spread;
            dest[3] = MAP_BYTE(w0); dest += spread;
            dest[4] = MAP_BYTE(w1 >> 24); dest += spread;
            dest[5] = MAP_BYTE(w1 >> 16); dest += spread;
            dest[6] = MAP_BYTE(w1 >> 8); dest += spread;
            dest[7] = MAP_BYTE(w1); dest += spread;
#undef MAP_BYTE
        }
    }
}

/* Set up the device for drawing an image. */
static gx_device *
setup_image_device(const gx_image_enum *penum)
{
    gx_device *dev = penum->dev;

    if (penum->clip_dev) {
        gx_device_clip *cdev = penum->clip_dev;

        gx_device_set_target((gx_device_forward *)cdev, dev);
        dev = (gx_device *) cdev;
    }
    if (penum->rop_dev) {
        gx_device_rop_texture *rtdev = penum->rop_dev;

        gx_device_set_target((gx_device_forward *)rtdev, dev);
        dev = (gx_device *) rtdev;
    }
    return dev;
}

/* Clean up by releasing the buffers. */
/* Currently we ignore draw_last. */
int
gx_image1_end_image(gx_image_enum_common_t * info, bool draw_last)
{
    gx_image_enum *penum = (gx_image_enum *) info;
    gs_memory_t *mem = penum->memory;
    stream_image_scale_state *scaler = penum->scaler;

    if_debug2m('b', penum->memory, "[b]%send_image, y=%d\n",
               (penum->y < penum->rect.h ? "premature " : ""), penum->y);
    if (draw_last) {
        int code = gx_image_flush(info);

        if (code < 0)
            return code;
    }

    if (penum->tpr_state != NULL) {
        transform_pixel_region_data data;
        gx_device *dev = penum->dev;
        if (penum->clip_dev)
            dev = (gx_device *)penum->clip_dev;
        if (penum->rop_dev)
            dev = (gx_device *)penum->rop_dev;
        data.state = penum->tpr_state;
        dev_proc(dev, transform_pixel_region)(dev, transform_pixel_region_end, &data);
    }
    /* release the reference to the target */
    if ( penum->rop_dev )
        gx_device_set_target((gx_device_forward *)penum->rop_dev, NULL);
    /* it is not clear (to me) why these are freed explicitly instead
       of using reference counting */
    gs_free_object(mem, penum->rop_dev, "image RasterOp");

    /* We do now reference count the clip device, see bug #706771 */
    rc_decrement(penum->clip_dev, "gx_image1_end_image");
    penum->clip_dev = NULL;

    if (scaler != 0) {
        (*scaler->templat->release) ((stream_state *) scaler);
        gs_free_object(mem, scaler, "image scaler state");
    }
    if (penum->icc_link != NULL) {
        gsicc_release_link(penum->icc_link);
    }
    if (penum->color_cache != NULL) {
        gs_free_object(mem, penum->color_cache->device_contone,
                        "device_contone");
        gs_free_object(mem, penum->color_cache->is_transparent,
                       "image is_transparent");
        gs_free_object(mem, penum->color_cache, "image color cache");
    }
    if (penum->thresh_buffer != NULL) {
        gs_free_object(mem, penum->thresh_buffer, "image thresh_buffer");
    }
    if (penum->ht_buffer != NULL) {
        gs_free_object(mem, penum->ht_buffer, "image ht_buffer");
    }
    if (penum->clues != NULL) {
        gs_free_object(mem,penum->clues, "image clues");
    }

    /* decrement this ref that was incremented in gx_image_enum_begin() */
    rc_decrement_only(penum->pcs, "pcs");
    penum->pcs = NULL;

    gs_free_object(mem, penum->line, "image line");
    gs_free_object(mem, penum->buffer, "image buffer");

#ifdef WITH_CAL
    if (penum->cal_ht != NULL) {
        cal_halftone_fin(penum->cal_ht, mem->non_gc_memory);
    }
#endif
    gx_image_free_enum(&info);
    return 0;
}
