/* Copyright (C) 2001-2010 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
*/
/* $Id */
/* Support functions for 1bpp Fax/Tiff devices */
#include "minftrsz.h"
#include "gserrors.h"
#include "gserror.h"
#include "gsmalloc.h"
#include "memory_.h"

/************************************************************************
 *
 * These functions implement raster data processing to ensure that the
 * dot size and line width are at least some minimum number of pixels.
 * This is needed for some printing technologies that cannot image pixels
 * (or small groups of pixels) but that rasterize at full resolution to
 * give better quality. The effect is to 'thicken' lines and expand
 * independent pixels (which sets a minimum on the lightest halftoned
 * gray value).
 *
 * While some of this can be done by setting a minimum on linewidth, this
 * would have no effect on features painted with narrow fills or with
 * monochrome images (sometimes pre-halftoned).
 *
 ************************************************************************/

typedef struct min_feature_data_s {
    gs_memory_t *memory;
    int min_size;
    int width;                  /* Pixels per line                      */
    int height;
    int cur_line;
    int bytes_per_line;
    byte *lines;                /* vertical history lines               */
    byte *lines_prev[8];        /* pointers to previous lines           */
                                /* lines_prev[0] is most recent         */
    byte remap_mid8[65536];     /* Horizontal pixel expansion map       */
    /* The h_remap array is indexed by the data byte with the 'extra'   */
    /* min_size bits to the left and right of the byte. Thus 16 bits    */
    /* needed for 8 bits plus the preceding and following 4 bits needed */
    /* for min_size of 4. This is probably adequate for current users   */
        /* the beginning and end of a line are special since they need  */
        /* to darken extra pixels within the line bounds                */
    byte remap_first4[256];
    byte remap_last4[256];
} min_feature_data_t;

static int
next_zero(int startbit, int val)
{
    int i;

    for (i=startbit-1; i>=0; i--)
        if ((val & 1<<i) == 0)
            return i;
    return -1;
}

static int
next_one(int startbit, int val)
{
    int i;

    for (i=startbit-1; i>=0; i--)
        if ((val & 1<<i) != 0)
            return i;
    return -1;
}

/* Note linewidth is number of pixels -- needed for the end of line */
int
min_feature_size_init(gs_memory_t *mem, int min_feature_size,
                      int width, int height, void **min_feature_data)
{
    min_feature_data_t *data;
    int i, bytes_per_line;

    if (min_feature_size > 4)
        return_error(gs_error_limitcheck);
    /* allocate our data structure and vertical tracking buffer */
    if ((data = (min_feature_data_t *)gs_malloc(mem, 1, sizeof(min_feature_data_t),
                                "mem_feature_size(data)")) == NULL)
        return_error(gs_error_VMerror);
    bytes_per_line = (width+7)/8;
    if ((data->lines = (byte *)gs_malloc(mem, bytes_per_line, 2 * min_feature_size,
                                "mem_feature_size(lines)")) == NULL) {
        gs_free(mem, data, 1, sizeof(min_feature_data_t),
                "mem_feature_size(data)");
        return_error(gs_error_VMerror);
    }
    data->memory = mem;
    data->width = width;
    data->height = height;
    data->cur_line = -1;
    data->min_size = min_feature_size;
    data->bytes_per_line = bytes_per_line;
    memset(data->lines, 0, bytes_per_line * 2 * min_feature_size);
    for(i=0; i<2*min_feature_size; i++)
        data->lines_prev[i] = data->lines + (bytes_per_line * i);

#define bm(b)           /* bitmask(b) 0 is lsb */\
   (1<<(b))

    /* build the 'remap' data for the min_feature_size */
    for (i=0; i<256; i++) {
        int f = i, l = i, fd = 8, fw;


        do {
            fd = next_one(fd, f);       /* value == -1 if past bit_0 */
            if (fd < 0)
                break;
            fw = next_zero(fd, f);      /* value == -1 if past bit_0 */
            if ((fd - fw) < min_feature_size) {
                /* darken (set to 0) bits needed depending on min_feature_size  */
                /* 'first' and 'last' are different because we need to expand   */
                /* within the bits, i.e., right of bit 7 for first and left of  */
                /* bit 0 for last                                               */
                /* Note that we will only be using the left most bits of first  */
                /* and the right most bits of last.                             */
                switch (min_feature_size) {
                  case 2:
                    /* current feature size is 1, darken bit to the right       */
                    /* unless it is past the end of the byte in 'last'          */
                    if (fd > 0) {
                        f |= bm(fw);
                        l |= bm(fw);
                    } else /* fd == 0 */
                        l |= 0x03;              /* darken to the left of lsb */
                    break;
                  case 3:
                    /* This case is more complicated -- we want to darken about */
                    /* the center if the current size is 1, else darken a bit   */
                    /* to the right, with the constraints of staying within the */
                    /* byte (bits 7::0). (fd-1) is to the right.                */
                    if ((fd < 7) && (fd > 0)) {
                        /* within byte, left and right */
                        /* darkening is referenced from 'fw' since that is the  */
                        /* white bit to the right of the 1 or 2 pixel wide area */
                        f |= bm(fw+2) | bm(fd-2);
                        l |= bm(fw+2) | bm(fd-2);
                    } else if (fd == 7) {
                        f |= 0xe0;              /* darken top three pixels */
                    } else {                    /* fd == 0 */
                        f |= 0x07;              /* darken bottom three pixels */
                        l |= 0x07;
                    }
                    break;
                  case 4:
                    /* like case 3, except we prefer to darken one to the left  */
                    /* and two to the right.                                    */
                    if ((fd < 7) && (fd > 1)) {
                        /* within byte, left and right */
                        f |= bm(fw+2) | bm(fd-1) | bm(fd-2);
                        l |= bm(fw+2) | bm(fd-1) | bm(fd-2);
                    } else if (fd == 7) {
                        /* darken high 4 bits */
                        f |= 0xfd;
                    } else {    /* fd <= 1 */
                        /* darken final 4 bits */
                        f |= 0x0f;
                        l |= 0x0f;
                    }
                    break;
                  default:      /* don't change the data (shouldn't be here) */
                    break;
                }
            }
            fd = next_zero(fd, f);      /* advance past this group of '1' bits  */
                                        /* we 'seek' again because data may     */
                                        /* have changed due to darkening right  */
        } while (fd >= 0);
        /* finished with the whole byte. Save the results */
        data->remap_first4[i] = f;
        data->remap_last4[i] = l;
    }
    for (i=0; i<65536; i++) {
        int d = i, fd = 16, fw;

        do {
            fd = next_one(fd, d);       /* value == -1 if past bit_0 */
            if (fd < 0)
                break;
            fw = next_zero(fd, d);      /* value == -1 if past bit_0 */
            if ((fd - fw) < min_feature_size) {
                /* darken (set to 0) bits needed depending on min_feature_size  */
                /* 'first' and 'last' are different because we need to expand   */
                /* within the bits, i.e., right of bit 7 for first and left of  */
                /* bit 0 for last                                               */
                /* Note that we will only be using the left most bits of first  */
                /* and the right most bits of last.                             */
                switch (min_feature_size) {
                  case 2:
                    /* current feature size is 1, darken bit to the right       */
                    if (fd > 0) {
                        d |= bm(fw);
                    } else /* fd == 0 */
                        d |= 0x0003;            /* two lsb's darkened   */
                    break;
                  case 3:
                    /* This case is more complicated -- we want to darken about */
                    /* the center but bias darkening to the right               */
                    if ((fd < 15) && (fd > 0)) {
                        /* within value, left and right */
                        d |= bm(fw+2) | bm(fd-1);
                    } else if (fd == 15) {
                        d |= 0xe000;            /* darken top three */
                    } else {    /* fd == 0 */
                        d |= 0x0007;            /* darken three lsb's */
                    }
                    break;
                  case 4:
                    /* like case 3, except we prefer to darken one to the left  */
                    /* and two to the right.                                    */
                    if ((fd < 15) && (fd > 1)) {
                        /* within byte, left and right */
                        d |= bm(fw+2) | bm(fd-1) | bm(fd-2);
                    } else if (fd == 15) {
                        d &= 0xf000;            /* darken top 4 pixels */
                    } else {    /* fd <= 1 */
                        d &= 0x000f;            /* darken last 4 pixels */
                    }
                    break;
                  default:      /* don't change the data (shouldn't be here) */
                    break;
                }
            }
            fd = next_zero(fd, d);      /* advance past this group of '1' bits  */
                                        /* we 'seek' again because data may     */
                                        /* have changed due to darkening right  */
        } while (fd >= 0);
        /* finished with the whole short. Save the byte in the middle */
        data->remap_mid8[i] = (d >> 4) & 0xff;
    }
    *min_feature_data = data;           /* give the caller our pointer */
    return 0;
}

int
min_feature_size_dnit(void *min_feature_data)
{
    min_feature_data_t *data = min_feature_data;

    if (data != NULL) {
        if (data->lines != NULL)
            gs_free(data->memory, data->lines, data->bytes_per_line,
                    2*data->min_size, "mem_feature_size(lines)");
        gs_free(data->memory, data, 1, sizeof(min_feature_data_t),
                "mem_feature_size(data)");
    }
    return 0;
}

/* Return number of byte available */
int
min_feature_size_process(byte *line, void *min_feature_data)
{
    min_feature_data_t *data = min_feature_data;
    ushort      d;
    byte        n, m, *btmp;
    int i, pad_bits = (8 - (data->width & 7)) & 7;      /* range 0 to 7 */
    int bytes_per_line = (data->width+7)/8, end = bytes_per_line - 2;
    int count_out = 0;          /* bytes_per_line if line is output */

    data->cur_line++;
    d = data->remap_first4[line[0]];    /* darken bits toward the right */
    d <<= 4;                            /* shift up to enter loop */

    for (i=0; i<=end; i++) {            /* stop short of 'end' */
        n = line[i+1];
        d |= n >> 4;
        m = data->remap_mid8[d];                /* middle byte with bits darkened */
        line[i] = m;
        d |= m << 4;
        d = ((d<<4) | n) << 4;
    }
    /* Final bits require alignment for the last 4 bits */
    d = (((line[i-1] << 8) | line[i]) >> pad_bits) & 0xff;
    m = data->remap_last4[d];
    line[i-1] |= m >> (8 - pad_bits);
    line[i] |= m << pad_bits;

    /* Now handle the vertical darkening */
    /* shift lines up by adjusting pointers */
    btmp = data->lines_prev[2*(data->min_size)-1];      /* oldest line */
    for (i=2*(data->min_size)-1; i>0; i--)
        data->lines_prev[i] = data->lines_prev[i-1];
    data->lines_prev[0] = btmp;

    /* Copy this input line into the most recent lines_prev: lines_prev[0] */
    memcpy(data->lines_prev[0], line, bytes_per_line);

    switch (data->min_size) {
      case 4:
                /**** TBI ****/
      case 3:
                /**** TBI ****/

      case 2:
        if (data->cur_line >= data->height - 1) {
            if (data->cur_line == data->height - 1) {
                /* end of page must darken next to last line*/
                for (i=0; i<bytes_per_line; i++)
                    line[i] = data->lines_prev[1][i] |= data->lines_prev[0][i];
            } else {
                /* very last line is unchanged, ignore input line in lines_prev[0] */
                for (i=0; i<bytes_per_line; i++)
                    line[i] = data->lines_prev[1][i];
            }
        } else {
            /* darken bits in lines_prev[1] */
            /* Since we are darkening in following lines, we only really need 3 lines */
            for (i=0; i<bytes_per_line; i++) {
                data->lines_prev[0][i] |= data->lines_prev[1][i] & ~(data->lines_prev[2][i]);
                line[i] = data->lines_prev[1][i];
            }
        }
        if (data->cur_line >= 1)
            count_out = bytes_per_line;
        break;
      default:
        break;
    }
    return count_out;
}
