/*
 * Copyright (C) 1998 Aladdin Enterprises.
 * All rights reserved.
 *
 * This file is part of Aladdin Ghostscript.
 *
 * Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
 * or distributor accepts any responsibility for the consequences of using it,
 * or for whether it serves any particular purpose or works at all, unless he
 * or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
 * License (the "License") for full details.
 *
 * Every copy of Aladdin Ghostscript must include a copy of the License,
 * normally in a plain ASCII text file named PUBLIC.  The License grants you
 * the right to copy, modify and redistribute Aladdin Ghostscript, but only
 * under certain conditions described in the License.  Among other things, the
 * License requires that the copyright notice and this notice be preserved on
 * all copies.
 */

/* rtrstcmp.c - raster decompression routines */

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
  private void
uncompress_0(
    pcl_seed_row_t *    pout,
    const byte *        pin,
    int                 in_size
)
{
    int                 nbytes = (in_size > pout->size ? pout->size : in_size);

    memcpy(pout->pdata, pin, nbytes);
    if (!pout->is_blank)
        memset(pout->pdata + nbytes, 0, pout->size - nbytes);
    pout->is_blank = (in_size == 0);
}

/*
 * Run-length compression.
 */
  private void
uncompress_1(
    pcl_seed_row_t *    pout,
    const byte *        pin,
    int                 in_size
)
{
    int                 i = in_size / 2;
    byte *              pb = pout->pdata;
    byte *              plim = pb + pout->size;

    while (i-- > 0) {
        int     cnt = *pin++ + 1;
        byte    val = *pin++;

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
  private void
uncompress_2(
    pcl_seed_row_t *    pout,
    const byte *        pin,
    int                 in_size
)
{
    int                 i = in_size;
    byte *              pb = pout->pdata;
    byte *              plim = pb + pout->size;

    while (i-- > 0) {
        int             cntrl = *pin++;

        if (cntrl < 128) {
            uint            cnt = min(cntrl + 1, i);
            const byte *    ptmp = pin;

            i -= cnt;
            pin += cnt;
            if (cnt > plim - pb)
                cnt = plim - pb;
            while (cnt-- > 0)
                *pb++ = *ptmp++;

        } else if ((cntrl > 128) && (i-- > 0)) {
            int     cnt = min(257 - cntrl, plim - pb);
            int     val = *pin++;

            while (cnt-- > 0)
                *pb++ = val;
        }
    }
    if (!pout->is_blank)
        memset(pb, 0, plim - pb);
    pout->is_blank = (in_size == 0);
}

/*
 * Delta row compression
 */
  private void
uncompress_3(
    pcl_seed_row_t *    pout,
    const byte *        pin,
    int                 in_size
)
{
    int                 i = in_size;
    byte *              pb = pout->pdata;
    byte *              plim = pb + pout->size;

    while (i-- > 0) {
        uint            val = *pin++;
        uint            cnt = (val >> 5) + 1;
        uint            offset = val & 0x1f;
        const byte *    ptmp = 0;

        if ((offset == 0x1f) && (i-- > 0)) {
            uint    add_offset;

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
 *
 * HP's documentation of this command is not completely clear regarding the 
 * interpretation of the replacement byte count for the run-length compression
 * case. The interpretation used here, based on the documentation in the
 * "PCL 5 Comparison Guide", October 1996 edition, pp. 2.94-2.96, is that the
 * replacement byte count refers to the number of output bytes replaced, and
 * as many input bytes as required are read until at leas this many output
 * bytes have been replaced.
 */
  private void
uncompress_9(
    pcl_seed_row_t *    pout,
    const byte *        pin,
    int                 in_size
)
{
    int                 i = in_size;
    byte *              pb = pout->pdata;
    byte *              plim = pb + pout->size;


    while (i-- > 0) {
        uint            val = *pin++;
        uint            cnt = 0;
        uint            offset = 0;
        bool            more_cnt = false;
        bool            more_offset = false;
        bool            comp = ((val & 0x80) != 0);

        if (comp) {
            offset = (val >> 5) & 0x3;
            more_offset = (offset == 0x3);
            cnt = (val & 0x1f) + 1;
            more_cnt = (cnt == 0x20);
        } else {
            offset = (val >> 3) & 0xf;
            more_offset = (offset == 0xf);
            cnt = (val & 0x7) + 1;
            more_cnt = (cnt == 0x8);
        }

        while (more_offset && (i-- > 0)) {
            uint    extra = *pin++;

            more_offset = (extra == 0xff);
            offset += extra;
        }
        while (more_cnt && (i-- > 0)) {
            uint    extra = *pin++;

            more_cnt = (extra == 0xff);
            offset += extra;
        }

        if ((pb += offset) >= plim)
            break;
        if (comp) {
            uint    j = i / 2;

            while (j-- > 0) {
                uint    rep_cnt = *pin++;
                uint    rep_val = *pin++;

                if (rep_cnt > plim - pb)
                    rep_cnt = plim - pb;
                while (rep_cnt-- > 0)
                    *pb++ = rep_val;
            }
            i -= 2 * j;
            
        } else {
            if (cnt > i)
                cnt = i;
            i -= cnt;
            pin += cnt;
            if (cnt > plim - pb)
                cnt = plim - pb;
            while (cnt-- > 0)
                *pb++ = *pin++;
        }

    }
    pout->is_blank = (pout->is_blank && (in_size == 0));

}


void    (*const pcl_decomp_proc[9 + 1])( pcl_seed_row_t *   pout,
                                         const byte *       pin,
                                         int                in_size 
                                         ) = {
    uncompress_0,
    uncompress_1,
    uncompress_2,
    uncompress_3,
    0, 0, 0, 0, 0,  /* modes 4 and 6 - 8 unused; mode 5 handled separately */
    uncompress_9
};
