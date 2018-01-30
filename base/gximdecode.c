/* Copyright (C) 2014-2018 Artifex Software, Inc.
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

/* Methods for decoding and unpacking image data.  Used for color
monitoring in clist and for creating TIFF files for xpswrite device */

#include "gximdecode.h"
#include "string_.h"

/* We need to have the unpacking proc so that we can monitor the data for color
   or decode during xpswrite */
void
get_unpack_proc(gx_image_enum_common_t *pie, image_decode_t *imd, 
                gs_image_format_t format, const float *decode) {

    static sample_unpack_proc_t procs[2][6] = {
        { sample_unpack_1, sample_unpack_2,
        sample_unpack_4, sample_unpack_8,
        sample_unpack_12, sample_unpackicc_16
        },
        { sample_unpack_1_interleaved, sample_unpack_2_interleaved,
        sample_unpack_4_interleaved, sample_unpack_8_interleaved,
        sample_unpack_12, sample_unpackicc_16
        } };
    int num_planes = pie->num_planes;
    bool interleaved = (num_planes == 1 && pie->plane_depths[0] != imd->bps);
    int i;
    int index_bps = (imd->bps < 8 ? imd->bps >> 1 : (imd->bps >> 2) + 1);
    int log2_xbytes = (imd->bps <= 8 ? 0 : arch_log2_sizeof_frac);

    switch (format) {
    case gs_image_format_chunky:
        imd->spread = 1 << log2_xbytes;
        break;
    case gs_image_format_component_planar:
        imd->spread = (imd->spp) << log2_xbytes;
        break;
    case gs_image_format_bit_planar:
        imd->spread = (imd->spp) << log2_xbytes;
        break;
    default:
        imd->spread = 0;
    }

    if (interleaved) {
        int num_components = pie->plane_depths[0] / imd->bps;

        for (i = 1; i < num_components; i++) {
            if (decode[0] != decode[i * 2 + 0] ||
                decode[1] != decode[i * 2 + 1])
                break;
        }
        if (i == num_components)
            interleaved = false; /* Use single table. */
    }
    imd->unpack = procs[interleaved][index_bps];
}

/* We also need the mapping method for the unpacking proc */
void
get_map(image_decode_t *imd, gs_image_format_t format, const float *decode)
{
    int ci = 0;
    int decode_type;
    int bps = imd->bps;
    int spp = imd->spp;
    static const float default_decode[] = {
        0.0, 1.0, 0.0, 1.0, 0.0, 1.0, 0.0, 1.0, 0.0, 1.0
    };
    const float *this_decode = &decode[ci * 2];
    const float *map_decode;        /* decoding used to */
                              /* construct the expansion map */
    const float *real_decode;       /* decoding for expanded samples */

    decode_type = 3; /* 0=custom, 1=identity, 2=inverted, 3=impossible */
    for (ci = 0; ci < spp; ci += 2) {
        decode_type &= (decode[ci] == 0. && decode[ci + 1] == 1.) |
            (decode[ci] == 1. && decode[ci + 1] == 0.) << 1;
    }

    /* Initialize the maps from samples to intensities. */
    for (ci = 0; ci < spp; ci++) {
        sample_map *pmap = &imd->map[ci];

        if (bps > 8)
            imd->applymap = applymap16;
        else
            imd->applymap = applymap8;

        /* If the decoding is [0 1] or [1 0], we can fold it */
        /* into the expansion of the sample values; */
        /* otherwise, we have to use the floating point method. */

        this_decode = &decode[ci * 2];

        map_decode = real_decode = this_decode;
        if (!(decode_type & 1)) {
            if ((decode_type & 2) && bps <= 8) {
                real_decode = default_decode;
            }
            else {
                map_decode = default_decode;
            }
        }
        if (bps > 2 || format != gs_image_format_chunky) {
            if (bps <= 8)
                image_init_map(&pmap->table.lookup8[0], 1 << bps,
                map_decode);
        }
        else {                /* The map index encompasses more than one pixel. */
            byte map[4];
            register int i;

            image_init_map(&map[0], 1 << bps, map_decode);
            switch (bps) {
            case 1:
            {
                register bits32 *p = &pmap->table.lookup4x1to32[0];

                if (map[0] == 0 && map[1] == 0xff)
                    memcpy((byte *)p, lookup4x1to32_identity, 16 * 4);
                else if (map[0] == 0xff && map[1] == 0)
                    memcpy((byte *)p, lookup4x1to32_inverted, 16 * 4);
                else
                    for (i = 0; i < 16; i++, p++)
                        ((byte *)p)[0] = map[i >> 3],
                        ((byte *)p)[1] = map[(i >> 2) & 1],
                        ((byte *)p)[2] = map[(i >> 1) & 1],
                        ((byte *)p)[3] = map[i & 1];
            }
            break;
            case 2:
            {
                register bits16 *p = &pmap->table.lookup2x2to16[0];

                for (i = 0; i < 16; i++, p++)
                    ((byte *)p)[0] = map[i >> 2],
                    ((byte *)p)[1] = map[i & 3];
            }
            break;
            }
        }
        pmap->decode_base /* = decode_lookup[0] */ = real_decode[0];
        pmap->decode_factor =
            (real_decode[1] - real_decode[0]) /
            (bps <= 8 ? 255.0 : (float)frac_1);
        pmap->decode_max /* = decode_lookup[15] */ = real_decode[1];
        if (decode_type) {
            pmap->decoding = sd_none;
            pmap->inverted = map_decode[0] != 0;
        }
        else if (bps <= 4) {
            int step = 15 / ((1 << bps) - 1);
            int i;

            pmap->decoding = sd_lookup;
            for (i = 15 - step; i > 0; i -= step)
                pmap->decode_lookup[i] = pmap->decode_base +
                i * (255.0 / 15) * pmap->decode_factor;
        }
        else
            pmap->decoding = sd_compute;
    }
}

/* We only provide 8 or 16 bit output with the application of the mapping */
void applymap8(sample_map map[], const void *psrc_in, int spp, void *pdes,
    void *bufend)
{
    byte* psrc = (byte*)psrc_in;
    byte *curr_pos = (byte*) pdes;
    int k;
    float temp;

    while (curr_pos < (byte*) bufend) {
        for (k = 0; k < spp; k++) {
            switch (map[k].decoding) {
            case sd_none:
                *curr_pos = *psrc;
                break;
            case sd_lookup:
                temp = map[k].decode_lookup[(*psrc) >> 4] * 255;
                if (temp > 255) temp = 255;
                if (temp < 0) temp = 0;
                *curr_pos = (byte)temp;
                break;
            case sd_compute:
                temp = map[k].decode_base +
                    *(psrc) * map[k].decode_factor;
                temp *= 255;
                if (temp > 255) temp = 255;
                if (temp < 0) temp = 0;
                *curr_pos = (byte)temp;
            default:
                break;
            }
            curr_pos++;
            psrc++;
        }
    }
}

void applymap16(sample_map map[], const void *psrc_in, int spp, void *pdes,
    void *bufend)
{
    unsigned short *curr_pos = (unsigned short*)pdes;
    unsigned short *psrc = (unsigned short*)psrc_in;
    int k;
    float temp;

    while (curr_pos < (unsigned short*) bufend) {
        for (k = 0; k < spp; k++) {
            switch (map[k].decoding) {
            case sd_none:
                *curr_pos = *psrc;
                break;
            case sd_lookup:
                temp = map[k].decode_lookup[*(psrc) >> 4] * 65535.0;
                if (temp > 65535) temp = 65535;
                if (temp < 0) temp = 0;
                *curr_pos = (unsigned short)temp;
                break;
            case sd_compute:
                temp = map[k].decode_base +
                    *psrc * map[k].decode_factor;
                temp *= 65535;
                if (temp > 65535) temp = 65535;
                if (temp < 0) temp = 0;
                *curr_pos = (unsigned short)temp;
            default:
                break;
            }
            curr_pos++;
            psrc++;
        }
    }
}
