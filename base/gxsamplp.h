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


/* Templates for sample lookup and expansion */

/* This module is allowed to include several times into a single .c file.
   The following macros to be defined in advance :
        MULTIPLE_MAPS - 1 if num_components_per_plane > 0 and
                        components use different maps, 0 otherwise.
        TEMPLATE_sample_unpack_1 - a name for the function
        TEMPLATE_sample_unpack_2 - a name for the function
        TEMPLATE_sample_unpack_4 - a name for the function
        TEMPLATE_sample_unpack_8 - a name for the function
 */

#if defined(TEMPLATE_sample_unpack_1) && defined(TEMPLATE_sample_unpack_2) && defined(TEMPLATE_sample_unpack_4) && defined(TEMPLATE_sample_unpack_8)

#include "valgrind.h"

#if MULTIPLE_MAPS
#   define NEXT_MAP map = smap[++smap_index % num_components_per_plane].table.lookup4x1to32
#   define NEXT_MAP8 map = smap[++smap_index % num_components_per_plane].table.lookup8
#   define DEFINE_SMAP_INDEX int smap_index = 0;
#else
#   define NEXT_MAP
#   define NEXT_MAP8
#   define DEFINE_SMAP_INDEX
#endif

const byte *
TEMPLATE_sample_unpack_1(byte * bptr, int *pdata_x, const byte * data, int data_x,
                uint dsize, const sample_map *smap, int spread,
                int num_components_per_plane)
{
    const sample_lookup_t * ptab = &smap->table;
    const byte *psrc = data + (data_x >> 3);
    int left = dsize - (data_x >> 3);
    DEFINE_SMAP_INDEX

#ifdef PACIFY_VALGRIND
    byte *bend = bptr + left*spread;
    byte vbits = 0;

    /* Allow for undefined bits at the end */
    VALGRIND_GET_VBITS(&psrc[left-1], &vbits, 1);
    /* At least the top bit must be defined. If not,
     * don't change anything, and let valgrind complain. */
    if ((vbits & 0x80) == 0) {
        byte zero = 0;
        VALGRIND_SET_VBITS(&psrc[left-1], &zero, 1);
    }
#endif

    if (spread == 1) {
        bits32 *bufp = (bits32 *) bptr;
        const bits32 *map = &ptab->lookup4x1to32[0];
        uint b;

        if (left & 1) {
            b = psrc[0];
            bufp[0] = map[b >> 4];
            NEXT_MAP;
            bufp[1] = map[b & 0xf];
            NEXT_MAP;
            psrc++, bufp += 2;
        }
        left >>= 1;
        while (left--) {
            b = psrc[0];
            bufp[0] = map[b >> 4];
            NEXT_MAP;
            bufp[1] = map[b & 0xf];
            NEXT_MAP;
            b = psrc[1];
            bufp[2] = map[b >> 4];
            NEXT_MAP;
            bufp[3] = map[b & 0xf];
            NEXT_MAP;
            psrc += 2, bufp += 4;
        }
    } else {
        byte *bufp = bptr;
        const byte *map = &ptab->lookup8[0];

        while (left--) {
            uint b = *psrc++;

            *bufp = map[b >> 7];
            NEXT_MAP8;
            bufp += spread;
            *bufp = map[(b >> 6) & 1];
            NEXT_MAP8;
            bufp += spread;
            *bufp = map[(b >> 5) & 1];
            NEXT_MAP8;
            bufp += spread;
            *bufp = map[(b >> 4) & 1];
            NEXT_MAP8;
            bufp += spread;
            *bufp = map[(b >> 3) & 1];
            NEXT_MAP8;
            bufp += spread;
            *bufp = map[(b >> 2) & 1];
            NEXT_MAP8;
            bufp += spread;
            *bufp = map[(b >> 1) & 1];
            NEXT_MAP8;
            bufp += spread;
            *bufp = map[b & 1];
            NEXT_MAP8;
            bufp += spread;
        }
    }
#ifdef PACIFY_VALGRIND
    /* Put any undefinedness back, and carry it over to the output. */
    VALGRIND_SET_VBITS(psrc-1, &vbits, 1);
    {
        byte ones = 0xff;
        if (vbits & 0x40)
            VALGRIND_SET_VBITS(bend - spread*7, &ones, 1);
        if (vbits & 0x20)
            VALGRIND_SET_VBITS(bend - spread*6, &ones, 1);
        if (vbits & 0x10)
            VALGRIND_SET_VBITS(bend - spread*5, &ones, 1);
        if (vbits & 0x08)
            VALGRIND_SET_VBITS(bend - spread*4, &ones, 1);
        if (vbits & 0x04)
            VALGRIND_SET_VBITS(bend - spread*3, &ones, 1);
        if (vbits & 0x02)
            VALGRIND_SET_VBITS(bend - spread*2, &ones, 1);
        if (vbits & 0x01)
            VALGRIND_SET_VBITS(bend - spread*1, &ones, 1);
    }
#endif
    *pdata_x = data_x & 7;
    return bptr;
}

#undef NEXT_MAP

#if MULTIPLE_MAPS
#   define NEXT_MAP  map = smap[++smap_index % num_components_per_plane].table.lookup2x2to16
#else
#   define NEXT_MAP
#endif

const byte *
TEMPLATE_sample_unpack_2(byte * bptr, int *pdata_x, const byte * data, int data_x,
                uint dsize, const sample_map *smap, int spread,
                int num_components_per_plane)
{
    const sample_lookup_t * ptab = &smap->table;
    const byte *psrc = data + (data_x >> 2);
    int left = dsize - (data_x >> 2);
    DEFINE_SMAP_INDEX

#ifdef PACIFY_VALGRIND
    byte *bend = bptr + left*spread;
    byte vbits = 0;

    /* Allow for undefined bits at the end */
    VALGRIND_GET_VBITS(&psrc[left-1], &vbits, 1);
    /* At least the top 2 bits must be defined. If not,
     * don't change anything, and let valgrind complain. */
    if ((vbits & 0xC0) == 0) {
        byte zero = 0;
        VALGRIND_SET_VBITS(&psrc[left-1], &zero, 1);
    }
#endif

    if (spread == 1) {
        bits16 *bufp = (bits16 *) bptr;
        const bits16 *map = &ptab->lookup2x2to16[0];

        while (left--) {
            uint b = *psrc++;

            *bufp++ = map[b >> 4];
            NEXT_MAP;
            *bufp++ = map[b & 0xf];
            NEXT_MAP;
        }
    } else {
        byte *bufp = bptr;
        const byte *map = &ptab->lookup8[0];

        while (left--) {
            unsigned b = *psrc++;

            *bufp = map[b >> 6];
            NEXT_MAP8;
            bufp += spread;
            *bufp = map[(b >> 4) & 3];
            NEXT_MAP8;
            bufp += spread;
            *bufp = map[(b >> 2) & 3];
            NEXT_MAP8;
            bufp += spread;
            *bufp = map[b & 3];
            NEXT_MAP8;
            bufp += spread;
        }
    }
    *pdata_x = data_x & 3;
#ifdef PACIFY_VALGRIND
    /* Put any undefinedness back, and carry it over to the output. */
    VALGRIND_SET_VBITS(psrc-1, &vbits, 1);
    {
        byte ones = 0xff;
        if (vbits & 0x30)
            VALGRIND_SET_VBITS(bend - spread*3, &ones, 1);
        if (vbits & 0x0C)
            VALGRIND_SET_VBITS(bend - spread*2, &ones, 1);
        if (vbits & 0x03)
            VALGRIND_SET_VBITS(bend - spread*1, &ones, 1);
    }
#endif
    return bptr;
}

#undef NEXT_MAP

const byte *
TEMPLATE_sample_unpack_4(byte * bptr, int *pdata_x, const byte * data, int data_x,
                uint dsize, const sample_map *smap, int spread,
                int num_components_per_plane)
{
    const sample_lookup_t * ptab = &smap->table;
    byte *bufp = bptr;
    const byte *psrc = data + (data_x >> 1);
    int left = dsize - (data_x >> 1);
    const byte *map = &ptab->lookup8[0];
    DEFINE_SMAP_INDEX

#ifdef PACIFY_VALGRIND
    byte vbits = 0;

    /* Allow for undefined bits at the end */
    VALGRIND_GET_VBITS(&psrc[left-1], &vbits, 1);
    /* At least the top 4 bits must be defined. If not,
     * don't change anything, and let valgrind complain. */
    if ((vbits & 0xf0) == 0) {
        byte zero = 0;
        VALGRIND_SET_VBITS(&psrc[left-1], &zero, 1);
    }
#endif

    while (left--) {
        uint b = *psrc++;

        *bufp = map[b >> 4];
        NEXT_MAP8;
        bufp += spread;
        *bufp = map[b & 0xf];
        NEXT_MAP8;
        bufp += spread;
    }
#ifdef PACIFY_VALGRIND
    /* Put any undefinedness back, and carry it over to the output. */
    VALGRIND_SET_VBITS(psrc-1, &vbits, 1);
    if ((vbits & 0x0f) != 0) {
        byte ones = 0xFF;
        VALGRIND_SET_VBITS(bufp-spread, &ones, 1);
    }
#endif
    *pdata_x = data_x & 1;
    return bptr;
}

const byte *
TEMPLATE_sample_unpack_8(byte * bptr, int *pdata_x, const byte * data, int data_x,
                uint dsize, const sample_map *smap, int spread,
                int num_components_per_plane)
{
    const sample_lookup_t * ptab = &smap->table;
    byte *bufp = bptr;
    const byte *psrc = data + data_x;
    DEFINE_SMAP_INDEX

    *pdata_x = 0;
    if (spread == 1) {
        if (MULTIPLE_MAPS ||
            ptab->lookup8[0] != 0 ||
            ptab->lookup8[255] != 255
            ) {
            uint left = dsize - data_x;
            const byte *map = &ptab->lookup8[0];

            while (left--) {
                *bufp++ = map[*psrc++];
                NEXT_MAP8;
            }
        } else {		/* No copying needed, and we'll use the data right away. */
            return psrc;
        }
    } else {
        int left = dsize - data_x;
        const byte *map = &ptab->lookup8[0];

        for (; left--; psrc++, bufp += spread) {
            *bufp = map[*psrc];
            NEXT_MAP8;
        }
    }
    return bptr;
}

#undef NEXT_MAP
#undef NEXT_MAP8
#undef DEFINE_SMAP_INDEX

#else
int dummy;
#endif
