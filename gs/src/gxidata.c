/* Copyright (C) 1995, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.

   This file is part of Aladdin Ghostscript.

   Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
   or distributor accepts any responsibility for the consequences of using it,
   or for whether it serves any particular purpose or works at all, unless he
   or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
   License (the "License") for full details.

   Every copy of Aladdin Ghostscript must include a copy of the License,
   normally in a plain ASCII text file named PUBLIC.  The License grants you
   the right to copy, modify and redistribute Aladdin Ghostscript, but only
   under certain conditions described in the License.  Among other things, the
   License requires that the copyright notice and this notice be preserved on
   all copies.
 */


/* Generic image enumeration and cleanup */
#include "gx.h"
#include "memory_.h"
#include "gserrors.h"
#include "gxdevice.h"
#include "gxcpath.h"
#include "gximage.h"

/* Forward declarations */
private void repack_bit_planes(P7(const gx_image_plane_t *src_planes,
				  const ulong *offsets, int num_planes,
				  byte *buffer, int width,
				  const sample_lookup_t * ptab, int spread));

/* Process the next piece of an ImageType 1 image. */
int
gx_image1_plane_data(gx_device * dev, gx_image_enum_common_t * info,
		     const gx_image_plane_t * planes, int height)
{
    gx_image_enum *penum = (gx_image_enum *) info;
    int y = penum->y;
    int y_end = min(y + height, penum->rect.h);
    int width_spp = penum->rect.w * penum->spp;
    int num_planes = penum->num_planes;

#define BCOUNT(plane)		/* bytes per data row */\
  (((penum->rect.w + (plane).data_x) * penum->spp * penum->bps / num_planes\
    + 7) >> 3)

    fixed adjust = penum->adjust;
    ulong offsets[gs_image_max_planes];
    int ignore_data_x;
    bool bit_planar = penum->num_planes > penum->spp;
    int code;

    if (height == 0)
	return 0;

    /* Set up the clipping and/or RasterOp device if needed. */

    if (penum->clip_dev) {
	gx_device_clip *cdev = penum->clip_dev;

	cdev->target = dev;
	dev = (gx_device *) cdev;
    }
    if (penum->rop_dev) {
	gx_device_rop_texture *rtdev = penum->rop_dev;

	((gx_device_forward *) rtdev)->target = dev;
	dev = (gx_device *) rtdev;
    }
    /* Now render complete rows. */

    memset(offsets, 0, num_planes * sizeof(offsets[0]));
    for (; penum->y < y_end; penum->y++) {
	int px;
	const byte *buffer;
	int sourcex;

	if (bit_planar) {
	    /* Repack the bit planes into byte-wide samples. */
	    
	    buffer = penum->buffer;
	    sourcex = 0;
	    for (px = 0; px < num_planes; px += penum->bps)
		repack_bit_planes(planes, offsets, penum->bps, penum->buffer,
				  penum->rect.w, &penum->map[0].table,
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
	    buffer =
		(*penum->unpack)(penum->buffer, &sourcex,
				 planes[0].data + offsets[0],
				 planes[0].data_x, BCOUNT(planes[0]),
				 &penum->map[0].table, penum->spread);

	    offsets[0] += planes[0].raster;
	    for (px = 1; px < num_planes; ++px) {
		(*penum->unpack)(penum->buffer + (px << penum->log2_xbytes),
				 &ignore_data_x,
				 planes[px].data + offsets[px],
				 planes[px].data_x, BCOUNT(planes[px]),
				 &penum->map[px].table, penum->spread);
		offsets[px] += planes[px].raster;
	    }
	}
#ifdef DEBUG
	if (gs_debug_c('B')) {
	    int i, n = width_spp;

	    dlputs("[B]row:");
	    for (i = 0; i < n; i++)
		dprintf1(" %02x", buffer[i]);
	    dputs("\n");
	}
#endif
	penum->cur.x = dda_current(penum->dda.row.x);
	dda_next(penum->dda.row.x);
	penum->cur.y = dda_current(penum->dda.row.y);
	dda_next(penum->dda.row.y);
	if (!penum->interpolate)
	    switch (penum->posture) {
		case image_portrait:
		    {		/* Precompute integer y and height, */
			/* and check for clipping. */
			fixed yc = penum->cur.y, yn = dda_current(penum->dda.row.y);

			if (yn < yc) {
			    fixed temp = yn;

			    yn = yc;
			    yc = temp;
			}
			yc -= adjust;
			if (yc >= penum->clip_outer.q.y)
			    goto mt;
			yn += adjust;
			if (yn <= penum->clip_outer.p.y)
			    goto mt;
			penum->yci = fixed2int_pixround(yc);
			penum->hci = fixed2int_pixround(yn) - penum->yci;
			if (penum->hci == 0)
			    goto mt;
		    }
		    break;
		case image_landscape:
		    {		/* Check for no pixel centers in x. */
			fixed xc = penum->cur.x, xn = dda_current(penum->dda.row.x);

			if (xn < xc) {
			    fixed temp = xn;

			    xn = xc;
			    xc = temp;
			}
			xc -= adjust;
			if (xc >= penum->clip_outer.q.x)
			    goto mt;
			xn += adjust;
			if (xn <= penum->clip_outer.p.x)
			    goto mt;
			penum->xci = fixed2int_pixround(xc);
			penum->wci = fixed2int_pixround(xn) - penum->xci;
			if (penum->wci == 0)
			    goto mt;
		    }
		    break;
		case image_skewed:
		    ;
	    }
	dda_translate(penum->dda.pixel0.x,
		      penum->cur.x - penum->prev.x);
	dda_translate(penum->dda.pixel0.y,
		      penum->cur.y - penum->prev.y);
	penum->prev = penum->cur;
	code = (*penum->render) (penum, buffer, sourcex, width_spp, 1,
				 dev);
	if (code < 0)
	    goto err;
      mt:;
    }
    if (penum->y < penum->rect.h) {
	code = 0;
	goto out;
    }
    /* End of data.  Render any left-over buffered data. */
    code = gx_image1_flush(info);
    if (code < 0) {
	penum->y--;
	goto err;
    }
    code = 1;
    goto out;
  err:				/* Error or interrupt, restore original state. */
    while (penum->y > y) {
	dda_previous(penum->dda.row.x);
	dda_previous(penum->dda.row.y);
	--(penum->y);
    }
    /* Note that caller must call end_image */
    /* for both error and normal termination. */
  out:return code;
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
    dda_translate(penum->dda.pixel0.x, penum->cur.x - penum->prev.x);
    dda_translate(penum->dda.pixel0.y, penum->cur.y - penum->prev.y);
    penum->prev = penum->cur;
    return (*penum->render)(penum, NULL, 0, width_spp, 0, penum->dev);
}

/*
 * Repack 1 to 8 individual bit planes into 8-bit samples.
 * buffer is aligned, and includes padding to an 8-byte boundary.
 * This procedure repacks one row, so the only relevant members of
 * src_planes are data and data_x (not raster).
 */
private void
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
#if arch_is_big_endian
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

/* Clean up by releasing the buffers. */
/* Currently we ignore draw_last. */
int
gx_image1_end_image(gx_device *ignore_dev, gx_image_enum_common_t * info,
		    bool draw_last)
{
    gx_image_enum *penum = (gx_image_enum *) info;
    gs_memory_t *mem = penum->memory;
    stream_IScale_state *scaler = penum->scaler;

    if_debug2('b', "[b]%send_image, y=%d\n",
	      (penum->y < penum->rect.h ? "premature " : ""), penum->y);
    gs_free_object(mem, penum->rop_dev, "image RasterOp");
    gs_free_object(mem, penum->clip_dev, "image clipper");
    if (scaler != 0) {
	(*s_IScale_template.release) ((stream_state *) scaler);
	gs_free_object(mem, scaler, "image scaler state");
    }
    gs_free_object(mem, penum->line, "image line");
    gs_free_object(mem, penum->buffer, "image buffer");
    gs_free_object(mem, penum, "gx_default_end_image");
    return 0;
}
