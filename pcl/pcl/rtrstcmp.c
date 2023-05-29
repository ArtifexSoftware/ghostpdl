/* Copyright (C) 2001-2023 Artifex Software, Inc.
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


/* rtrstcmp.c - raster decompression routines */

#include "string_.h"
#include "pcstate.h"
#include "rtrstcmp.h"

/*
 * Each of the decompression methods has the same structure. The operands are
 * are the seed row to be filled in and the buffer from which it is to be
 * filled. A size is provided for the latter.
 *
 * Adpative compression (mode 5) is not handled at this level, though it will
 * make use of these routines for decompressing individual raster rows.
 */

/*
 * Uncompressed data.
 */
static void
uncompress_0(pcl_seed_row_t * pout, const byte * pin, int in_size)
{
    int nbytes = (in_size > pout->size ? pout->size : in_size);

    memcpy(pout->pdata, pin, nbytes);
    if (!pout->is_blank)
        memset(pout->pdata + nbytes, 0, pout->size - nbytes);
    pout->is_blank = (in_size == 0);
}

/*
 * Run-length compression.
 */
static void
uncompress_1(pcl_seed_row_t * pout, const byte * pin, int in_size)
{
    int i = in_size / 2;
    byte *pb = pout->pdata;
    byte *plim = pb + pout->size;

    while (i-- > 0) {
        int cnt = *pin++ + 1;
        byte val = *pin++;

        if (cnt > plim - pb)
            cnt = plim - pb;
        while (cnt-- > 0)
            *pb++ = val;
    }
    if (!pout->is_blank)
        memset(pb, 0, plim - pb);
    pout->is_blank = (in_size == 0);
}

/*
 * TIFF "Packbits" compression.
 */
static void
uncompress_2(pcl_seed_row_t * pout, const byte * pin, int in_size)
{
    int i = in_size;
    byte *pb = pout->pdata;
    byte *plim = pb + pout->size;

    while (i-- > 0) {
        int cntrl = *pin++;

        if (cntrl < 128) {
            uint cnt = min(cntrl + 1, i);
            const byte *ptmp = pin;

            i -= cnt;
            pin += cnt;
            if (cnt > plim - pb)
                cnt = plim - pb;
            while (cnt-- > 0)
                *pb++ = *ptmp++;

        } else if ((cntrl > 128) && (i-- > 0)) {
            int cnt = min(257 - cntrl, plim - pb);
            int val = *pin++;

            memset(pb, val, cnt);
            pb += cnt;
        }
    }
    if (!pout->is_blank)
        memset(pb, 0, plim - pb);
    pout->is_blank = (in_size == 0);
}

/*
 * Delta row compression
 */
static void
uncompress_3(pcl_seed_row_t * pout, const byte * pin, int in_size)
{
    int i = in_size;
    byte *pb = pout->pdata;
    byte *plim = pb + pout->size;

    while (i-- > 0) {
        uint val = *pin++;
        uint cnt = (val >> 5) + 1;
        uint offset = val & 0x1f;
        const byte *ptmp = 0;

        if ((offset == 0x1f) && (i-- > 0)) {
            uint add_offset;
            do
                offset += (add_offset = *pin++);
            while ((add_offset == 0xff) && (i-- > 0));
        }

        if (cnt > i)
            cnt = i;
        i -= cnt;
        ptmp = pin;
        pin += cnt;
        if ((pb += offset) >= plim)
            break;
        if (cnt > plim - pb)
            cnt = plim - pb;
        while (cnt-- > 0)
            *pb++ = *ptmp++;
    }
    pout->is_blank = (pout->is_blank && (in_size == 0));
}

/*
 * Adpative compression (mode 5) is handled at a higher level.
 */

/*
 * Compression mode 9.
 */
static void
uncompress_9(pcl_seed_row_t * pout, const byte * pin, int in_size)
{
    int i = in_size;
    byte *pb = pout->pdata;
    byte *plim = pb + pout->size;
    while (i-- > 0) {
        uint val = *pin++;
        uint cnt = 0;
        uint offset = 0;
        bool more_cnt = false;
        bool more_offset = false;
        bool comp = ((val & 0x80) != 0);

        if (comp) {
            offset = (val >> 5) & 0x3;
            more_offset = (offset == 0x3);
            cnt = (val & 0x1f) + 2;
            more_cnt = (cnt == 0x21);
        } else {
            offset = (val >> 3) & 0xf;
            more_offset = (offset == 0xf);
            cnt = (val & 0x7) + 1;
            more_cnt = (cnt == 0x8);
        }

        while (more_offset && (i > 0)) {
            uint extra = *pin++;
            i -= 1;

            more_offset = (extra == 0xff);
            offset += extra;
        }
        while (more_cnt && (i > 0)) {
            uint extra = *pin++;
            i -= 1;

            more_cnt = (extra == 0xff);
            cnt += extra;
        }

        if ((pb += offset) >= plim)
            break;
        if (i <= 0)
            break;

        if (comp) {
            if(i-- > 0) {
                uint rep_val = *pin++;

                if (cnt > plim - pb)
                    cnt = plim - pb;
                while (cnt-- > 0)
                    *pb++ = rep_val;
            }
        } else {
            if (cnt > i)
                cnt = i;
            i -= cnt;
            if (cnt > plim - pb)
                cnt = plim - pb;
            while (cnt-- > 0)
                *pb++ = *pin++;
        }

    }
    pout->is_blank = (pout->is_blank && (in_size == 0));

}

enum
{
    eeNewPixel = 0x0,
    eeWPixel = 0x20,
    eeNEPixel = 0x40,
    eeCachedColor = 0x60
};

static inline uint32_t
mode10_merge_delta(const uint8_t ** src, uint32_t oldpixel, int *countdown)
{
    uint32_t pixel;

    if (*countdown < 2) {
	return 0xffffff;
    }
    if (**src & 0x80) {
	uint16_t delta;
	int32_t dr, dg, db;
	int r, g, b;

	if_debug2('w', "delta %02X %02X\n", **src, *(*src + 1));

	delta = (**src << 8) | *(*src + 1);
	dr = (delta >> 10) & 0x1f;
	dg = (delta >> 5) & 0x1f;
	db = (delta & 0x1f) << 1;
	if (dr & 0x10) {
	    dr |= 0xffffffe0;
	}
	if (dg & 0x10) {
	    dg |= 0xffffffe0;
	}
	if (db & 0x20) {
	    db |= 0xffffffd0;
	}
	r = ((oldpixel >> 16) + dr) & 0xff;
	g = ((oldpixel >> 8) + dg) & 0xff;
	b = ((oldpixel >> 0) + db) & 0xff;
	pixel = (r << 16) | (g << 8) | (b);
	(*src) += 2;
	*countdown -= 2;
    } else {
	if (*countdown < 3) {
	    return 0xffffff;
	}
	if_debug3('w', "lit %02X %02X %02X\n", **src, *(*src + 1),
		  *(*src + 2));

	pixel = ((**src << 16) | (*(*src + 1) << 8) | (*(*src + 2))) << 1;
	if (pixel & 0x80) {
	    pixel |= 1;
	}
	(*src) += 3;
	*countdown -= 3;
    }
    return pixel;
}

static void
uncompress_10(pcl_seed_row_t * pout, const byte * pin, int in_size)
{
    int i = in_size;
    byte *pb = pout->pdata;
    byte *plim = pb + pout->size;
    uint32_t cachedColor = 0x00ffffff;
    uint32_t pixel = 0xffffff;
    uint8_t val;
    int comp;
    int j;

    while (i-- > 0) {
	int offset;
	int cnt;
	int more_cnt;
	int pixel_source;
	val = *pin++;
	if_debug1('w', "command %02X ", val);
	comp = val & 0x80;
	pixel_source = val & 0x60;

	offset = (val >> 3) & 0x3;
	if (offset == 3) {
	    do {
		if (i <= 0) {
		    if_debug0('w', "source end premature 1\n");
		    goto tidy_up;
		}
		offset += *pin;
		i--;
	    } while (*pin++ == 0xff);
	}
	cnt = (val & 0x7);
	if (cnt == 7)
	    more_cnt = 1;
	else
	    more_cnt = 0;
	if_debug1('w', "offset %d ", offset);
	pb += offset * 3;
	if (pixel_source == eeNewPixel) {
	    /* deal with later */
	} else if (pixel_source == eeWPixel) {
	    if (((pb - pout->pdata) > 2) && (pb <= plim)) {
		pixel = (*(pb - 3) << 16) | (*(pb - 2) << 8) | (*(pb - 1));
	    } else
		break;
	} else if (pixel_source == eeNEPixel) {
	    if (pb + 5 < plim) {
		pixel = (*(pb + 3) << 16) | (*(pb + 4) << 8) | *(pb + 5);
	    } else
		break;
	} else {
	    pixel = cachedColor;
	}
	if (comp) {
	    /* RLE */
	    cnt += 2;
	    if (pixel_source == eeNewPixel) {
		if ((pb + 3) > plim)
		    break;
		pixel = (*pb << 16) | (*(pb + 1) << 8) | (*(pb + 2));
		pixel = mode10_merge_delta(&pin, pixel, &i);
		cachedColor = pixel;
	    }
	    if (more_cnt) {
		do {
		    if (i <= 0) {
			if_debug0('w', "source end premature 2\n");
			goto tidy_up;
		    }
		    cnt += *pin;
		    i--;
		} while (*pin++ == 0xff);
	    }
	    if_debug1('w', "rcnt %d\n", cnt);
	    for (j = 0; j < cnt; j++) {
		if ((pb + 3) > plim) {
		    if_debug0('|', "pixel over run 1\n");
		} else {
		    *pb++ = (pixel >> 16) & 0xff;
		    *pb++ = (pixel >> 8) & 0xff;
		    *pb++ = (pixel >> 0) & 0xff;
		}
	    }
	} else {
	    if ((pb + 3) > plim)
		break;
	    if (pixel_source == eeNewPixel) {
		pixel = (*pb << 16) | (*(pb + 1) << 8) | (*(pb + 2));
		pixel = mode10_merge_delta(&pin, pixel, &i);
		cachedColor = pixel;
	    }
	    *pb++ = (pixel >> 16) & 0xff;
	    *pb++ = (pixel >> 8) & 0xff;
	    *pb++ = (pixel >> 0) & 0xff;
	    if_debug1('w', "lcnt %d\n", cnt + 1);
	    while (1) {
		for (j = 0; j < cnt; j++) {
		    if ((pb + 3) > plim)
			goto tidy_up;
		    pixel = (*pb << 16) | (*(pb + 1) << 8) | (*(pb + 2));
		    pixel = mode10_merge_delta(&pin, pixel, &i);
		    /* cachedColor only updated on first literal pixel */
		    *pb++ = (pixel >> 16) & 0xff;
		    *pb++ = (pixel >> 8) & 0xff;
		    *pb++ = (pixel >> 0) & 0xff;
		}
		if (more_cnt) {
		    cnt = *pin++;
		    i--;
		    if (cnt != 255) {
			more_cnt = 0;
		    }
		} else {
		    break;
		}
	    }
	}
    }
  tidy_up:
    if (in_size)
	if_debug2('w', "data count raw %d out %ld\n", in_size,
		  pb - pout->pdata);
    else
	if_debug0('w', "*");
    pout->is_blank = (pout->is_blank && (in_size == 0));

}

void (*const pcl_decomp_proc[10 + 1]) (pcl_seed_row_t * pout,
                                      const byte * pin, int in_size) = {
    uncompress_0,
    uncompress_1,
    uncompress_2,
    uncompress_3,
    0, 0, 0, 0, 0,      /* modes 4 - 8 handled separately */
    uncompress_9,
    uncompress_10
};
