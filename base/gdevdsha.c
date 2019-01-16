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

/* Default shading drawing device procedures. */

#include "gx.h"
#include "gserrors.h"
#include "gxdevice.h"
#include "gxcindex.h"
#include "gxdevsop.h"

static bool
gx_devn_diff(frac31 devn1[], frac31 devn2[], int num)
{
    int k;

    for (k = 0; k < num; k++) {
        if (devn1[k] != devn2[k]) {
            return true;
        }
    }
    return false;
}

int
gx_hl_fill_linear_color_scanline(gx_device *dev, const gs_fill_attributes *fa,
        int i0, int j, int w, const frac31 *c0, const int32_t *c0f,
        const int32_t *cg_num, int32_t cg_den)
{
    frac31 c[GX_DEVICE_COLOR_MAX_COMPONENTS];
    frac31 curr[GX_DEVICE_COLOR_MAX_COMPONENTS];
    ulong f[GX_DEVICE_COLOR_MAX_COMPONENTS];
    int i, i1 = i0 + w, bi = i0, k;
    const gx_device_color_info *cinfo = &dev->color_info;
    int n = cinfo->num_components;
    int si, ei, di, code;
    gs_fixed_rect rect;
    gx_device_color devc;

    /* Note: All the stepping math is done with frac color values */

    devc.type = gx_dc_type_devn;

    if (j < fixed2int(fa->clip->p.y) ||
            j > fixed2int_ceiling(fa->clip->q.y)) /* Must be compatible to the clipping logic. */
        return 0;
    for (k = 0; k < n; k++) {
        curr[k] = c[k] = c0[k];
        f[k] = c0f[k];
    }
    for (i = i0 + 1, di = 1; i < i1; i += di) {
        if (di == 1) {
            /* Advance colors by 1 pixel. */
            for (k = 0; k < n; k++) {
                if (cg_num[k]) {
                    int32_t m = f[k] + cg_num[k];

                    c[k] += m / cg_den;
                    m -= m / cg_den * cg_den;
                    if (m < 0) {
                        c[k]--;
                        m += cg_den;
                    }
                    f[k] = m;
                }
            }
        } else {
            /* Advance colors by di pixels. */
            for (k = 0; k < n; k++) {
                if (cg_num[k]) {
                    int64_t M = f[k] + (int64_t)cg_num[k] * di;
                    int32_t m;

                    c[k] += (frac31)(M / cg_den);
                    m = (int32_t)(M - M / cg_den * cg_den);
                    if (m < 0) {
                        c[k]--;
                        m += cg_den;
                    }
                    f[k] = m;
                }
            }
        }
        if (gx_devn_diff(c, curr, n)) {
            si = max(bi, fixed2int(fa->clip->p.x));	    /* Must be compatible to the clipping logic. */
            ei = min(i, fixed2int_ceiling(fa->clip->q.x));  /* Must be compatible to the clipping logic. */
            if (si < ei) {
                if (fa->swap_axes) {
                    rect.p.x = int2fixed(j);
                    rect.p.y = int2fixed(si);
                    rect.q.x = int2fixed(j + 1);
                    rect.q.y = int2fixed(ei);
                } else {
                    rect.p.x = int2fixed(si);
                    rect.p.y = int2fixed(j);
                    rect.q.x = int2fixed(ei);
                    rect.q.y = int2fixed(j + 1);
                }
                for (k = 0; k < n; k++) {
                    devc.colors.devn.values[k] = frac312cv(curr[k]);
                }
                code = dev_proc(dev, fill_rectangle_hl_color) (dev, &rect, NULL, &devc, NULL);
                if (code < 0)
                    return code;
            }
            bi = i;
            for (k = 0; k < n; k++) {
                curr[k] = c[k];
            }
            di = 1;
        } else if (i == i1) {
            i++;
            break;
        } else {
            /* Compute a color change pixel analytically. */
            di = i1 - i;
            for (k = 0; k < n; k++) {
                int32_t a;
                int64_t x;
                frac31 v = 1 << (31 - cinfo->comp_bits[k]); /* Color index precision in frac31. */
                frac31 u = c[k] & (v - 1);

                if (cg_num[k] == 0) {
                    /* No change. */
                    continue;
                } if (cg_num[k] > 0) {
                    /* Solve[(f[k] + cg_num[k]*x)/cg_den == v - u, x]  */
                    a = v - u;
                } else {
                    /* Solve[(f[k] + cg_num[k]*x)/cg_den == - u - 1, x]  */
                    a = -u - 1;
                }
                x = ((int64_t)a * cg_den - f[k]) / cg_num[k];
                if (i + x >= i1)
                    continue;
                else if (x < 0)
                    return_error(gs_error_unregistered); /* Must not happen. */
                else if (di > (int)x) {
                    di = (int)x;
                    if (di <= 1) {
                        di = 1;
                        break;
                    }
                }
            }
        }
    }
    si = max(bi, fixed2int(fa->clip->p.x));	    /* Must be compatible to the clipping logic. */
    ei = min(i, fixed2int_ceiling(fa->clip->q.x));  /* Must be compatible to the clipping logic. */
    if (si < ei) {
        if (fa->swap_axes) {
            rect.p.x = int2fixed(j);
            rect.p.y = int2fixed(si);
            rect.q.x = int2fixed(j + 1);
            rect.q.y = int2fixed(ei);
        } else {
            rect.p.x = int2fixed(si);
            rect.p.y = int2fixed(j);
            rect.q.x = int2fixed(ei);
            rect.q.y = int2fixed(j + 1);
        }
        for (k = 0; k < n; k++) {
            devc.colors.devn.values[k] = frac312cv(curr[k]);
        }
        return dev_proc(dev, fill_rectangle_hl_color) (dev, &rect, NULL, &devc, NULL);
    }
    return 0;
}

int
gx_default_fill_linear_color_scanline(gx_device *dev, const gs_fill_attributes *fa,
        int i0, int j, int w,
        const frac31 *c0, const int32_t *c0f, const int32_t *cg_num, int32_t cg_den)
{
    /* This default implementation decomposes the area into constant color rectangles.
       Devices may supply optimized implementations with
       the inversed nesting of the i,k cicles,
       i.e. with enumerating planes first, with a direct writing to the raster,
       and with a fixed bits per component.
     */
    /* First determine if we are doing high level style colors or pure colors */
    bool devn = dev_proc(dev, dev_spec_op)(dev, gxdso_supports_devn, NULL, 0);
    frac31 c[GX_DEVICE_COLOR_MAX_COMPONENTS];
    ulong f[GX_DEVICE_COLOR_MAX_COMPONENTS];
    int i, i1 = i0 + w, bi = i0, k;
    gx_color_index ci0 = 0, ci1;
    const gx_device_color_info *cinfo = &dev->color_info;
    int n = cinfo->num_components;
    int si, ei, di, code;
    /* If the device encodes tags, we expect the comp_shift[num_components] to be valid */
    /* for the tag part of the color (usually the high order bits of the color_index).  */
    gx_color_index tag = device_encodes_tags(dev) ?
                         (gx_color_index)(dev->graphics_type_tag & ~GS_DEVICE_ENCODES_TAGS) << cinfo->comp_shift[n]
                         : 0;

    /* Todo: set this up to vector earlier */
    if (devn)  /* Note, PDF14 could be additive and doing devn */
        return gx_hl_fill_linear_color_scanline(dev, fa, i0, j, w, c0, c0f,
                                                cg_num, cg_den);
    if (j < fixed2int(fa->clip->p.y) ||
            j > fixed2int_ceiling(fa->clip->q.y)) /* Must be compatible to the clipping logic. */
        return 0;
    for (k = 0; k < n; k++) {
        int shift = cinfo->comp_shift[k];
        int bits = cinfo->comp_bits[k];

        c[k] = c0[k];
        f[k] = c0f[k];
        ci0 |= (gx_color_index)(c[k] >> (sizeof(c[k]) * 8 - 1 - bits)) << shift;
    }
    for (i = i0 + 1, di = 1; i < i1; i += di) {
        if (di == 1) {
            /* Advance colors by 1 pixel. */
            ci1 = 0;
            for (k = 0; k < n; k++) {
                int shift = cinfo->comp_shift[k];
                int bits = cinfo->comp_bits[k];

                if (cg_num[k]) {
                    int32_t m = f[k] + cg_num[k];

                    c[k] += m / cg_den;
                    m -= m / cg_den * cg_den;
                    if (m < 0) {
                        c[k]--;
                        m += cg_den;
                    }
                    f[k] = m;
                }
                ci1 |= (gx_color_index)(c[k] >> (sizeof(c[k]) * 8 - 1 - bits)) << shift;
            }
        } else {
            /* Advance colors by di pixels. */
            ci1 = 0;
            for (k = 0; k < n; k++) {
                int shift = cinfo->comp_shift[k];
                int bits = cinfo->comp_bits[k];

                if (cg_num[k]) {
                    int64_t M = f[k] + (int64_t)cg_num[k] * di;
                    int32_t m;

                    c[k] += (frac31)(M / cg_den);
                    m = (int32_t)(M - M / cg_den * cg_den);
                    if (m < 0) {
                        c[k]--;
                        m += cg_den;
                    }
                    f[k] = m;
                }
                ci1 |= (gx_color_index)(c[k] >> (sizeof(c[k]) * 8 - 1 - bits)) << shift;
            }
        }
        if (ci1 != ci0) {
            si = max(bi, fixed2int(fa->clip->p.x));	    /* Must be compatible to the clipping logic. */
            ei = min(i, fixed2int_ceiling(fa->clip->q.x));  /* Must be compatible to the clipping logic. */
            if (si < ei) {
                ci0 |= tag;		/* set tag (may be 0 if the device doesn't use tags) */
                if (fa->swap_axes) {
                    code = dev_proc(dev, fill_rectangle)(dev, j, si, 1, ei - si, ci0);
                } else {
                    code = dev_proc(dev, fill_rectangle)(dev, si, j, ei - si, 1, ci0);
                }
                if (code < 0)
                    return code;
            }
            bi = i;
            ci0 = ci1;
            di = 1;
        } else if (i == i1) {
            i++;
            break;
        } else {
            /* Compute a color change pixel analitically. */
            di = i1 - i;
            for (k = 0; k < n; k++) {
                int32_t a;
                int64_t x;
                frac31 v = 1 << (31 - cinfo->comp_bits[k]); /* Color index precision in frac31. */
                frac31 u = c[k] & (v - 1);

                if (cg_num[k] == 0) {
                    /* No change. */
                    continue;
                } if (cg_num[k] > 0) {
                    /* Solve[(f[k] + cg_num[k]*x)/cg_den == v - u, x]  */
                    a = v - u;
                } else {
                    /* Solve[(f[k] + cg_num[k]*x)/cg_den == - u - 1, x]  */
                    a = -u - 1;
                }
                x = ((int64_t)a * cg_den - f[k]) / cg_num[k];
                if (i + x >= i1)
                    continue;
                else if (x < 0)
                    return_error(gs_error_unregistered); /* Must not happen. */
                else if (di > (int)x) {
                    di = (int)x;
                    if (di <= 1) {
                        di = 1;
                        break;
                    }
                }
            }
        }
    }
    si = max(bi, fixed2int(fa->clip->p.x));	    /* Must be compatible to the clipping logic. */
    ei = min(i, fixed2int_ceiling(fa->clip->q.x));  /* Must be compatible to the clipping logic. */
    if (si < ei) {
        ci0 |= tag;		/* set tag (may be 0 if the device doesn't use tags) */
        if (fa->swap_axes) {
            return dev_proc(dev, fill_rectangle)(dev, j, si, 1, ei - si, ci0);
        } else {
            return dev_proc(dev, fill_rectangle)(dev, si, j, ei - si, 1, ci0);
        }
    }
    return 0;
}
