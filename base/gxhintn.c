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


/* Type 1 hinter, a new algorithm */

#include "memory_.h"
#include "math_.h"
#include "gx.h"
#include "gxfixed.h"
#include "gxarith.h"
#include "gstypes.h"
#include "gxmatrix.h"
#include "gxpath.h"
#include "gxfont.h"
#include "gxfont1.h"
#include "gxtype1.h"
#include "gxhintn.h"
#include "gzpath.h"
#include "gserrors.h"

/*  todo :
    - Diagonal stems are not hinted;
    - Some fonts have no StdHW, StdWW. Adobe appears to autohint them.
    - Measure Adobe's flattness parameter.
    - Test Adobe compatibility for rotated/skewed glyphs.
 */

/*  Stem processing basics :
    (See the glyph AE in Times-Roman by Adobe.)

    0. This supposes that glyph is transformed to device space
       with a random matrix.

       All outline poles and all hint commands are stored in arrays
       before staring the exact processing.

       HR pole is pole before which stem replacement happens.

    1. Stem hints may be primary ones (defined in the beginning of charstring),
       and secondary ones (defined at HR poles). Consider that
       secondary stem hints may be redundant (see AE in Times-Roman).
       Secondary stems are HIGHER priority than basic ones.

    2. The range of secondary stem command is from its HR pole to next HR pole.
       The range of primary stem command is entire glyph.

    3. The TT interpreter aligned stem3 with centering the middle stem.

    4. If a stem boundary corresponds to a pole aligned with an alignment zone,
       pass aligned coordinate to the stem command.
       Use the stem boundary longitude middle point for alignment with
       skewed or rotated matrix. Use standard stem width for computing
       opposite coordinates.

    4a. If a stem width rounds to several StemSnap* element,
       choose the element, to which more stems can round.
       See Adobe Technical Note #5049.

    5. If several stems have a same boundary coordinate,
       this boundary gets more priority when aligned.

    6. Considering each set of repeating stem commands as a stem complex, pass
       aligned coordinates to opposite boundaries of stem commands.

    7. Pass aligned boundary coordinate to poles within stem command range.
       Note that this will pass aligned coordinates back to poles,
       from which stem alignment was taken.

    8. Interpolate unaligned poles.

    9. After the alignment is done, it is desirable to check for
       anomalous negative contours and fix them, but we have no
       good algorithm for this. The rasterizer must be tolerant
       to such contours (which may have self-crosses, self-contacts,
       or may change to opposite direction).

*/

/*  Dotsection processing basics :

    If stem replacement occures, dotsection to be ignored.
    To check this properly, we test whether extremal poles of contour
    were actually aligned with stem hints.

    If a contour was aligned with stem hints by both X and Y,
    no special processing required.

    Otherwise if dotsection center falls near vstem axis,
    we align it by X with the axis. Otherwise we align
    it by X to half-pixel. Then we align the center by Y to
    half-pixel, and shift entire contour to satisfy
    the alignment of the center.
*/

/*  vstem3/hstem3 processing basics :
    They are handled by the type 1,2 interpreters (gstype1.c, gstype2.c).
 */

/*  flex processing basics :
    With type 1 it is handled with t1_hinter__flex_* functions.
    With type 2 it is handled by gstype2.c .
 */

#define ADOBE_OVERSHOOT_COMPATIBILIY 0
#define ADOBE_SHIFT_CHARPATH 0

/*  The CONTRAST_STEMS option aligns one of two stem boundaries
    to integral pixel boundary when AlignToPixels = 0.
    It gives more contrast stems, because a bigger part
    of boldness is concentrated in smaller number of pixels.
*/
#define CONTRAST_STEMS 1

static const char *s_pole_array = "t1_hinter pole array";
static const char *s_zone_array = "t1_hinter zone array";
static const char *s_hint_array = "t1_hinter hint array";
static const char *s_contour_array = "t1_hinter contour array";
static const char *s_subglyph_array = "t1_hinter subglyph array";
static const char *s_hint_range_array = "t1_hinter hint_range array";
static const char *s_hint_applying_array = "t1_hinter hint_applying array";
static const char *s_stem_snap_array = "t1_hinter stem_snap array";
static const char *s_stem_snap_vote_array = "t1_hinter stem_snap_vote array";

#define member_prt(type, ptr, offset) (type *)((char *)(ptr) + (offset))

typedef int32_t int24;
#define HAVE_INT64_T

static const unsigned int split_bits = 12;
static const unsigned int max_coord_bits = 24; /* = split_bits * 2 */
static const unsigned int matrix_bits = 19; /* <= sizeof(int) * 8 - 1 - split_bits */
static const unsigned int g2o_bitshift = 12; /* <= matrix_bits + max_coord_bits - (sizeof(int) * 8 + 1) */
#ifndef HAVE_INT64_T
static const int32_t FFFFF000 = ~(int32_t)0xFFF; /* = ~(((int32_t)1 << split_bits) - 1) */
#endif
/* Constants above must satisfy expressions given in comments. */

/* Computes (a*b)>>s, s <= 12 */
static inline int32_t mul_shift(int24 a, int19 b, unsigned int s)
{
#ifdef HAVE_INT64_T
    return ( (int64_t)a * (int64_t)b ) >> s; /* unrounded result */
#else
    { /* 32 bit fallback */
        int32_t aa = a & FFFFF000, a0 = a - aa, a1 = aa >> s;
        return ((a0 * b) >> s) + a1 * b; /* unrounded result */
    }
#endif
}

/* Computes (a*b)>>s, s <= 12, with rounding */
static inline int32_t mul_shift_round(int24 a, int19 b, unsigned int s)
{
#ifdef HAVE_INT64_T
    return (( ( (int64_t)a * (int64_t)b ) >> (s - 1)) + 1) >> 1;
#else
    { /* 32 bit version */
        int32_t aa = a & FFFFF000, a0 = a - aa, a1 = aa >> s;
        return ((((a0 * b) >> (s - 1)) + 1) >> 1) + a1 * b; /* rounded result */
    }
#endif
}

static inline int32_t shift_rounded(int32_t v, unsigned int s)
{   return ((v >> (s - 1)) + 1) >> 1;
}

static inline int32_t Max(int32_t a, int32_t b)
{   return a > b ? a : b;
}

static inline long rshift(long a, int b)
{   return b > 0 ? a << b : a >> -b;
}
/*---------------------- members of matrix classes -------------------------*/

static inline void double_matrix__set(double_matrix * self, const gs_matrix_fixed * m)
{   self->xx = m->xx;
    self->xy = m->xy;
    self->yx = m->yx;
    self->yy = m->yy;
}

static inline int double_matrix__invert_to(const double_matrix * self, double_matrix * m)
{   double det = self->xx * self->yy - self->xy * self->yx;

    if (fabs(det) * 1000000 <= fabs(self->xx) + fabs(self->xy) + fabs(self->yx) + fabs(self->yy))
        return_error(gs_error_rangecheck);
    m->xx =  self->yy / det;
    m->xy = -self->xy / det;
    m->yx = -self->yx / det;
    m->yy =  self->xx / det;
    return 0;
}

static void fraction_matrix__drop_bits(fraction_matrix * self, unsigned int bits)
{   self->xx = shift_rounded(self->xx, bits);
    self->xy = shift_rounded(self->xy, bits);
    self->yx = shift_rounded(self->yx, bits);
    self->yy = shift_rounded(self->yy, bits);
    self->denominator >>= bits;
    self->bitshift -= bits;
}

static void fraction_matrix__set(fraction_matrix * self, const double_matrix * pmat)
{   double axx = fabs(pmat->xx), axy = fabs(pmat->xy);
    double ayx = fabs(pmat->yx), ayy = fabs(pmat->yy);
    double scale = max(axx + axy, ayx + ayy);
    int matrix_exp, m;

    (void)frexp(scale, &matrix_exp);
    self->bitshift = matrix_bits - matrix_exp;
    if (self->bitshift >= sizeof( self->denominator) * 8) {
        self->denominator = 0;
        self->xx = self->xy = self->yx = self->yy = 0;
    } else {
        self->denominator = 1 << self->bitshift;
        /* Round towards zero for a better view of mirrored characters : */
        self->xx = (int32_t)(pmat->xx * self->denominator + 0.5);
        self->xy = (int32_t)(pmat->xy * self->denominator + 0.5);
        self->yx = (int32_t)(pmat->yx * self->denominator + 0.5);
        self->yy = (int32_t)(pmat->yy * self->denominator + 0.5);
        m = Max(Max(any_abs(self->xx), any_abs(self->xy)), Max(any_abs(self->yx), any_abs(self->yy)));
        (void)frexp(m, &matrix_exp);
        if (matrix_exp > matrix_bits)
            fraction_matrix__drop_bits(self, matrix_exp - matrix_bits);
    }
}

static inline int fraction_matrix__to_double(const fraction_matrix * self, double_matrix * pmat)
{
    if (self->denominator == 0)
        return_error(gs_error_rangecheck);
    pmat->xx = (double)self->xx / self->denominator;
    pmat->xy = (double)self->xy / self->denominator;
    pmat->yx = (double)self->yx / self->denominator;
    pmat->yy = (double)self->yy / self->denominator;
    return 0;
}

static int fraction_matrix__invert_to(const fraction_matrix * self, fraction_matrix * pmat)
{   double_matrix m, M;
    int code;

    code = fraction_matrix__to_double(self, &M);
    if (code < 0)
        return code;
    code = double_matrix__invert_to(&M, &m);
    if (code < 0)
        return code;
    fraction_matrix__set(pmat, &m);
    return 0;
}

static inline int32_t fraction_matrix__transform_x(fraction_matrix *self, int24 x, int24 y, unsigned int s)
{   return mul_shift_round(x, self->xx, s) + mul_shift_round(y, self->yx, s);
}
static inline int32_t fraction_matrix__transform_y(fraction_matrix *self, int24 x, int24 y, unsigned int s)
{   return mul_shift_round(x, self->xy, s) + mul_shift_round(y, self->yy, s);
}

/*--------------------------- friends ------------------------------*/

static inline int ranger_step_f(int i, int beg, int end)
{   return (i == end ? beg : i + 1);
}

static inline int ranger_step_b(int i, int beg, int end)
{   return (i == beg ? end : i - 1);
}

static inline fixed o2d(const t1_hinter *h, t1_hinter_space_coord v)
{
    int s = h->g2o_fraction_bits - _fixed_shift;

    if (s >= 1)
        return ((v >> (h->g2o_fraction_bits - _fixed_shift - 1)) + 1) >> 1;
    else if (s == 0)
        return v;
    else
        return v << -s;
}

static inline fixed d2o(const t1_hinter *h, t1_hinter_space_coord v)
{   int s = h->g2o_fraction_bits - _fixed_shift;

    if (s >= 0)
        return v << s;
    else
        return v >> -s;
}

static inline void g2o(t1_hinter * h, t1_glyph_space_coord gx, t1_glyph_space_coord gy, t1_hinter_space_coord *ox, t1_hinter_space_coord *oy)
{   *ox = fraction_matrix__transform_x(&h->ctmf, gx, gy, g2o_bitshift);
    *oy = fraction_matrix__transform_y(&h->ctmf, gx, gy, g2o_bitshift);
}

static inline t1_hinter_space_coord g2o_dist(t1_glyph_space_coord gd, int19 coef)
{   return mul_shift(gd, coef, g2o_bitshift);
}

static inline void g2d(t1_hinter * h, t1_glyph_space_coord gx, t1_glyph_space_coord gy, fixed *dx, fixed *dy)
{
    *dx = fraction_matrix__transform_x(&h->ctmf, gx, gy, g2o_bitshift);
    *dy = fraction_matrix__transform_y(&h->ctmf, gx, gy, g2o_bitshift);
    *dx = o2d(h, *dx);
    *dy = o2d(h, *dy);
    *dx += h->orig_dx;
    *dy += h->orig_dy;
}

static inline void o2g(t1_hinter * h, t1_hinter_space_coord ox, t1_hinter_space_coord oy, t1_glyph_space_coord *gx, t1_glyph_space_coord *gy)
{   *gx = fraction_matrix__transform_x(&h->ctmi, ox, oy, split_bits);
    *gy = fraction_matrix__transform_y(&h->ctmi, ox, oy, split_bits);
    *gx = shift_rounded(*gx, h->g2o_fraction_bits + h->ctmi.bitshift - _fixed_shift - split_bits);
    *gy = shift_rounded(*gy, h->g2o_fraction_bits + h->ctmi.bitshift - _fixed_shift - split_bits);
}

static inline t1_glyph_space_coord o2g_dist(t1_hinter * h, t1_hinter_space_coord od, int19 coef)
{   return shift_rounded(mul_shift(od, coef, split_bits), h->g2o_fraction_bits + h->ctmi.bitshift - _fixed_shift - split_bits);
}

/* --------------------- t1_hint class members ---------------------*/

static void t1_hint__set_aligned_coord(t1_hint * self, t1_glyph_space_coord gc, t1_pole * pole, enum t1_align_type align, int quality)
{   t1_glyph_space_coord g = (self->type == hstem ? pole->gy : pole->gx);

    if (any_abs(self->g0 - g) < any_abs(self->g1 - g)) {
        if (self->aligned0 <= align && self->q0 > quality)
            self->ag0 = gc, self->aligned0 = align, self->q0 = quality;
    } else {
        if (self->aligned1 <= align && self->q1 > quality)
            self->ag1 = gc, self->aligned1 = align, self->q1 = quality;
    }
}

/* --------------------- t1_hinter class members - import --------------------*/

void t1_hinter__init(t1_hinter * self, gx_path *output_path)
{
    self->max_import_coord = (1 << max_coord_bits);
    self->stem_snap_count[0] = self->stem_snap_count[1] = 0;
    self->stem_snap_vote_count = 0;
    self->zone_count = 0;
    self->pole_count = 0;
    self->hint_count = 0;
    self->contour_count = 0;
    self->subglyph_count = 0;
    self->hint_range_count = 0;
    self->flex_count = 0;
    self->have_flex = false;

    self->max_subglyph_count = count_of(self->subglyph0);
    self->max_contour_count = count_of(self->contour0);
    self->max_zone_count = count_of(self->zone0);
    self->max_pole_count = count_of(self->pole0);
    self->max_hint_count = count_of(self->hint0);
    self->max_hint_range_count = count_of(self->hint_range0);
    self->max_hint_applying_count = count_of(self->hint_applying0);
    self->max_stem_snap_count[0] = count_of(self->stem_snap0[0]);
    self->max_stem_snap_count[1] = count_of(self->stem_snap0[1]);
    self->max_stem_snap_vote_count = count_of(self->stem_snap_vote0);

    self->pole = self->pole0;
    self->hint = self->hint0;
    self->zone = self->zone0;
    self->contour = self->contour0;
    self->subglyph = self->subglyph0;
    self->hint_range = self->hint_range0;
    self->hint_applying = self->hint_applying0;
    self->stem_snap[0] = self->stem_snap0[0];
    self->stem_snap[1] = self->stem_snap0[1];
    self->stem_snap_vote = self->stem_snap_vote0;

    self->FontType = 1;
    self->ForceBold = false;
    self->base_font_scale = 0;
    self->resolution = 0;
    self->heigt_transform_coef = self->width_transform_coef = 0;
    self->heigt_transform_coef_rat = self->width_transform_coef_rat = 0;
    self->heigt_transform_coef_inv = self->width_transform_coef_inv = 0;
    self->cx = self->cy = 0;
    self->contour[0] = 0;
    self->subglyph[0] = 0;
    self->keep_stem_width = false;
    self->charpath_flag = false;
    self->grid_fit_x = self->grid_fit_y = true;
    self->output_path = output_path;
    self->memory = (output_path == 0 ? 0 : output_path->memory);
    self->disable_hinting = (self->memory == NULL);
    self->pass_through = self->disable_hinting;
    self->autohinting = false;
    self->fix_contour_sign = false;
    self->path_opened = false;
    self->orig_dx = 0;
    self->orig_dy = 0;
    self->g2o_fraction_bits = 0;

    self->stem_snap[0][0] = self->stem_snap[1][0] = 100; /* default */

    memset(&self->ctmf, 0x00, sizeof(self->ctmf));
    memset(&self->ctmi, 0x00, sizeof(self->ctmi));
}

static inline void t1_hinter__free_arrays(t1_hinter * self)
{   if (self->pole != self->pole0)
        gs_free_object(self->memory, self->pole, s_pole_array);
    if (self->hint != self->hint0)
        gs_free_object(self->memory, self->hint, s_hint_array);
    if (self->zone != self->zone0)
        gs_free_object(self->memory, self->zone, s_zone_array);
    if (self->contour != self->contour0)
        gs_free_object(self->memory, self->contour, s_contour_array);
    if (self->subglyph != self->subglyph0)
        gs_free_object(self->memory, self->subglyph, s_subglyph_array);
    if (self->hint_range != self->hint_range0)
        gs_free_object(self->memory, self->hint_range, s_hint_range_array);
    if (self->hint_applying != self->hint_applying0)
        gs_free_object(self->memory, self->hint_applying, s_hint_applying_array);
    if (self->stem_snap[0] != self->stem_snap0[0])
        gs_free_object(self->memory, self->stem_snap[0], s_stem_snap_array);
    if (self->stem_snap[1] != self->stem_snap0[1])
        gs_free_object(self->memory, self->stem_snap[1], s_stem_snap_array);
    if (self->stem_snap_vote != self->stem_snap_vote0)
        gs_free_object(self->memory, self->stem_snap_vote, s_stem_snap_vote_array);
    self->pole = 0;
    self->hint = 0;
    self->zone = 0;
    self->contour = 0;
    self->hint_range = 0;
    self->hint_applying = 0;
    self->stem_snap[0] = self->stem_snap[1] = 0;
    self->stem_snap_vote = 0;
}

static inline void t1_hinter__init_outline(t1_hinter * self)
{
    self->contour_count = 0;
    self->pole_count = 0;
    self->contour[0] = 0;
    self->hint_count = 0;
    self->primary_hint_count = -1;
    self->suppress_overshoots = false;
    self->path_opened = false;
}

static void t1_hinter__compute_rat_transform_coef(t1_hinter * self)
{
    /* Round towards zero for a better view of mirrored characters : */
    self->heigt_transform_coef_rat = (int19)(self->heigt_transform_coef * self->ctmf.denominator + 0.5);
    self->width_transform_coef_rat = (int19)(self->width_transform_coef * self->ctmf.denominator + 0.5);
    self->heigt_transform_coef_inv = (int19)(self->ctmi.denominator / self->heigt_transform_coef + 0.5);
    self->width_transform_coef_inv = (int19)(self->ctmi.denominator / self->width_transform_coef + 0.5);
}

static inline void t1_hinter__adjust_matrix_precision(t1_hinter * self, fixed xx, fixed yy)
{
    ufixed x = any_abs(xx), y = any_abs(yy);
    ufixed c = (x > y ? x : y);

    while (c >= self->max_import_coord) {
        /* Reduce the precision of ctmf to allow products to fit into 32 bits : */
        self->max_import_coord <<= 1;
        fraction_matrix__drop_bits(&self->ctmf, 1);
        fraction_matrix__drop_bits(&self->ctmi, 1);
        self->g2o_fraction_bits -= 1;
        self->g2o_fraction >>= 1;
        t1_hinter__compute_rat_transform_coef(self);
    }
    if (self->ctmf.denominator == 0) {
        /* ctmf should be degenerate. */
        self->ctmf.denominator = 1;
    }
}

static inline void t1_hinter__set_origin(t1_hinter * self, fixed dx, fixed dy)
{
    fixed align_x = rshift(fixed_1, (self->align_to_pixels ? (int)self->log2_pixels_x : self->log2_subpixels_x));
    fixed align_y = rshift(fixed_1, (self->align_to_pixels ? (int)self->log2_pixels_y : self->log2_subpixels_y));

    self->orig_dx = (dx + align_x / 2) & ~(align_x - 1);
    self->orig_dy = (dy + align_y / 2) & ~(align_y - 1);
    t1_hinter__adjust_matrix_precision(self, self->orig_dx, self->orig_dy);
    self->orig_ox = d2o(self, self->orig_dx);
    self->orig_oy = d2o(self, self->orig_dy);
#   if ADOBE_SHIFT_CHARPATH
        /*  Adobe CPSI rounds coordinates for 'charpath' :
            X to trunc(x+0.5)
            Y to trunc(y)+0.5
        */
        if (self->charpath_flag) {
            self->orig_dx += fixed_half;
            self->orig_dx &= ~(fixed_1 - 1);
            self->orig_dy &= ~(fixed_1 - 1);
            self->orig_dy += fixed_half;
        } else {
            self->orig_dy += fixed_1;
            /* Adobe CPSI does this, not sure why. */
            /* fixme : check bbox of cached bitmap. */
        }
#   endif
}

int t1_hinter__set_mapping(t1_hinter * self, gs_matrix_fixed * ctm,
                    gs_matrix * FontMatrix, gs_matrix * baseFontMatrix,
                    int log2_pixels_x, int log2_pixels_y,
                    int log2_subpixels_x, int log2_subpixels_y,
                    fixed origin_x, fixed origin_y, bool align_to_pixels)
{   float axx = fabs(ctm->xx), axy = fabs(ctm->xy);
    float ayx = fabs(ctm->xx), ayy = fabs(ctm->xy);
    float scale = max(axx + axy, ayx + ayy);
    double_matrix CTM;
    int code;

    self->disable_hinting |= (scale < 1/1024. || scale > 4);
    self->pass_through |= self->disable_hinting;
    self->log2_pixels_x = log2_pixels_x;
    self->log2_pixels_y = log2_pixels_y;
    self->log2_subpixels_x = log2_subpixels_x;
    self->log2_subpixels_y = log2_subpixels_y;
    double_matrix__set(&CTM, ctm);
    fraction_matrix__set(&self->ctmf, &CTM);
    self->g2o_fraction_bits = self->ctmf.bitshift - g2o_bitshift + _fixed_shift;
    if (self->g2o_fraction_bits > max_coord_bits) {
        fraction_matrix__drop_bits(&self->ctmf, self->g2o_fraction_bits - max_coord_bits);
        self->g2o_fraction_bits = max_coord_bits;
    }
    if (self->ctmf.denominator != 0) {
        code = fraction_matrix__invert_to(&self->ctmf, &self->ctmi); /* Note: ctmi is inversion of ctmf, not ctm. */
        if (code == gs_error_rangecheck)
            self->ctmf.denominator = 0;
        else if (code < 0)
            return code;
    }
    if (self->ctmf.denominator != 0) {
        self->g2o_fraction = 1 << self->g2o_fraction_bits;
        /* Note : possibly we'll adjust the matrix precision dynamically
           with adjust_matrix_precision while importing the glyph. */
        if (self->g2o_fraction == 0)
            return_error(gs_error_limitcheck);
    }
    if (self->ctmf.denominator == 0 || self->ctmi.denominator == 0) {
        /* ctmf should be degenerate. */
        self->disable_hinting = true;
        self->pass_through = true;
        self->ctmf.denominator = 1;
    }
    self->transposed = (any_abs(self->ctmf.xy) * 10 > any_abs(self->ctmf.xx));
    {   /* height_transform_coef is scaling factor for the
           distance between horizontal lines while transformation.
           width_transform_coef defines similarly.
        */
        double_matrix m;
        double vp, sp, div_x, div_y;

        code = fraction_matrix__to_double(&self->ctmf, &m);
        if (code < 0)
            return code;
        vp = any_abs(m.xx * m.yy - m.yx * m.xy);
        sp = any_abs(m.xx * m.yx + m.xy * m.yy);
        div_x = hypot(m.xx, m.yx);
        div_y = hypot(m.xy, m.yy);
        if (vp != 0 && div_x != 0 && div_y != 0) {
            if (!self->transposed) {
                self->heigt_transform_coef = vp / div_x;
                self->width_transform_coef = vp / div_y;
            } else {
                self->heigt_transform_coef = vp / div_y;
                self->width_transform_coef = vp / div_x;
            }
            t1_hinter__compute_rat_transform_coef(self);
            self->keep_stem_width = (sp <= vp / 3); /* small skew */
        }
    }
    {   /* Compute font size and resolution : */
        gs_point p0, p1, p2;
        double d0, d1, d2;

        gs_distance_transform(0, 1, baseFontMatrix, &p0);
        gs_distance_transform(0, 1, FontMatrix, &p1);
        gs_distance_transform(0, 1, (gs_matrix *)ctm, &p2);
        d0 = hypot(p0.x, p0.y);
        d1 = hypot(p1.x, p1.y);
        d2 = hypot(p2.x, p2.y);
        self->base_font_scale = d0;
        self->font_size =  floor(d1 / d0 * 10000 + 0.5) / 10000;
        self->resolution = floor(d2 / d1 * 10000000 + 0.5) / 10000000;
        /*
         * fixme: base_font_scale, font_size and resolution are computed wrongly
         * for any of the following cases :
         *
         * 1. CIDFontType0C with FontMatrix=[0.001 0 0 0.001 0 0] gives 1/1000 size.
         * A known example : CIDembedded.pdf . We could obtain the Type 9 FontMatrix
         * in type1_exec_init from penum->fstack.
         *
         * 2. See comment in pdf_font_orig_matrix.
         *
         * Currently we don't use these values with a regular build.
         * The ADOBE_OVERSHOOT_COMPATIBILIY build needs to fix them.
         */
    }
    if (1 || /* Doesn't work - see comment above. */
            self->resolution * self->font_size >= 2) {
        /* Enable the grid fitting separately for axes : */
        self->grid_fit_y = (any_abs(self->ctmf.xy) * 10 < any_abs(self->ctmf.xx) ||
                            any_abs(self->ctmf.xx) * 10 < any_abs(self->ctmf.xy));
        self->grid_fit_x = (any_abs(self->ctmf.yx) * 10 < any_abs(self->ctmf.yy) ||
                            any_abs(self->ctmf.yy) * 10 < any_abs(self->ctmf.yx));
    } else {
        /* Disable the grid fitting for very small fonts. */
        self->grid_fit_x = self->grid_fit_y = false;
    }
    self->align_to_pixels = align_to_pixels;
    t1_hinter__set_origin(self, origin_x, origin_y);
    self->pixel_o_x = rshift(self->g2o_fraction, (self->align_to_pixels ? (int)self->log2_pixels_x : self->log2_subpixels_x));
    self->pixel_o_y = rshift(self->g2o_fraction, (self->align_to_pixels ? (int)self->log2_pixels_y : self->log2_subpixels_y));
    self->pixel_gh = any_abs(o2g_dist(self, self->pixel_o_x, self->heigt_transform_coef_inv));
    self->pixel_gw = any_abs(o2g_dist(self, self->pixel_o_y, self->width_transform_coef_inv));
    return 0;
}

static void t1_hinter__make_zone(t1_hinter * self, t1_zone *zone, float * blues, enum t1_zone_type type, t1_glyph_space_coord blue_fuzz)
{   t1_glyph_space_coord d = 0;

    zone->type = type;
    zone->y           = float2fixed(blues[0] + d);
    zone->overshoot_y = float2fixed(blues[1] + d);
    zone->y_min = min(zone->y, zone->overshoot_y) - blue_fuzz;
    zone->y_max = max(zone->y, zone->overshoot_y) + blue_fuzz;
    if (type == botzone ? zone->overshoot_y > zone->y : zone->overshoot_y < zone->y) {
        int v = zone->overshoot_y; zone->overshoot_y = zone->y; zone->y = v;
    }
    t1_hinter__adjust_matrix_precision(self, zone->y_min, zone->y_max);
}

static bool t1_hinter__realloc_array(gs_memory_t *mem, void **a, void *a0, int *max_count, int elem_size, int enhancement, const char *cname)
{
    void *aa = gs_alloc_bytes(mem, (size_t)(*max_count + enhancement * 2) * elem_size, cname);

    if (aa == NULL)
        return true;
    memcpy(aa, *a, (size_t)*max_count * elem_size);
    if (*a != a0)
        gs_free_object(mem, *a, cname);
    *a = aa;
    *max_count += enhancement * 2;
    return false;
}

static int t1_hinter__set_alignment_zones(gs_memory_t *mem, t1_hinter * self, float * blues, int count, enum t1_zone_type type, bool family)
{   int count2 = count / 2, i, j;

    if (!family) {
        /* Store zones : */
        if (count2 + self->zone_count >= self->max_zone_count)
            if(t1_hinter__realloc_array(mem, (void **)&self->zone, self->zone0, &self->max_zone_count,
                                        sizeof(self->zone0) / count_of(self->zone0),
                                        max(T1_MAX_ALIGNMENT_ZONES, count), s_zone_array))
                return_error(gs_error_VMerror);
        for (i = 0; i < count2; i++)
            t1_hinter__make_zone(self, &self->zone[self->zone_count + i], blues + i + i, type, self->blue_fuzz);
        self->zone_count += count2;
    } else {
        /* Replace with family zones if allowed : */
        t1_zone zone;
        for (i = 0; i < count2; i++) {
            t1_hinter__make_zone(self, &zone, blues + i, type, self->blue_fuzz);
            for (j = 0; j<self->zone_count; j++) {
                t1_zone *zone1 = &self->zone[j];
                if (any_abs(zone.y -           zone1->y          ) * self->heigt_transform_coef <= 1 &&
                    any_abs(zone.overshoot_y - zone1->overshoot_y) * self->heigt_transform_coef <= 1)
                    *zone1 = zone;
            }
        }
    }
    return 0;
}

static int t1_hinter__set_stem_snap(gs_memory_t *mem, t1_hinter * self, float * value, int count, unsigned short hv)
{   int count0 = self->stem_snap_count[hv], i, j;
    t1_glyph_space_coord pixel_g = (!hv ? self->pixel_gh : self->pixel_gw);

    if (pixel_g == 0)
        return 0;
    if (count + count0 >= self->max_stem_snap_count[hv])
        if(t1_hinter__realloc_array(mem, (void **)&self->stem_snap[hv], self->stem_snap0[hv], &self->max_stem_snap_count[hv],
                                        sizeof(self->stem_snap0[0]) / count_of(self->stem_snap0[0]),
                                        max(T1_MAX_STEM_SNAPS, count), s_stem_snap_array))
            return_error(gs_error_VMerror);
    if (count + count0 >= self->max_stem_snap_vote_count)
        if(t1_hinter__realloc_array(mem, (void **)&self->stem_snap_vote, self->stem_snap_vote0, &self->max_stem_snap_vote_count,
                                        sizeof(self->stem_snap_vote0) / count_of(self->stem_snap_vote0),
                                        max(T1_MAX_STEM_SNAPS, count), s_stem_snap_vote_array))
            return_error(gs_error_VMerror);
    if (count == 1 || (count > 0 && float2fixed(value[count - 1] - value[0]) > pixel_g)) {
        for (i = 0; i < count; i++)
            self->stem_snap[hv][i] = float2fixed(value[i]);
        self->stem_snap_count[hv] = count;
        for (i = 0; i < count; i++) {
            for (j = i + 1; j < count; j++)
                if (self->stem_snap[hv][i] > self->stem_snap[hv][j]) {
                    t1_glyph_space_coord v = self->stem_snap[hv][i];

                    self->stem_snap[hv][i] = self->stem_snap[hv][j];
                    self->stem_snap[hv][j] = v;
                }
        }
        for (i = 1, j = 0; i < count; i++) {
            if (self->stem_snap[hv][j] != self->stem_snap[hv][i]) {
                j++;
                self->stem_snap[hv][j] = self->stem_snap[hv][i];
            }
        }
        self->stem_snap_count[hv] = j + 1;
    }
    return 0;
    /* We store unrounded stem snap elements, align stem width
       to an unrounded element, and then round the width to pixels.
       As an alternative we tried to round stem snap elements when storing them,
       and aligh stem width to the closest rounded value. The fist alternative gives
       results closer to Adobe, and therefore we believe that Adobe does the same.
       With the second alternative many glyphs render some wider,
       for example in aaon97_p7.pdf, adesso1.pdf at 300 dpi.

       Another arbitrary solution is ignoring stem snap when
       its variation is lesser than 1 pixel. We believe that a threshold
       must exist because Adobe says that stem snaps work for high resolutions only.
       However we took the 1 pixel value for the threshold from scratch,
       and experiments give good results.

       At last, we ignore Std*V when stem snap is used.
       Doing so because we don't know cases when Std*V
       isn't equal to any stem snap element.
       */
}

int t1_hinter__set_font_data(gs_memory_t *mem, t1_hinter * self, int FontType, gs_type1_data *pdata, bool no_grid_fitting, bool is_resource)
{   int code;

    t1_hinter__init_outline(self);
    self->FontType = FontType;
    self->BlueScale = pdata->BlueScale;
    self->blue_shift = float2fixed(pdata->BlueShift);
    self->blue_fuzz  = float2fixed(pdata->BlueFuzz);
    self->suppress_overshoots = (self->BlueScale > self->heigt_transform_coef / (1 << self->log2_pixels_y) - 0.00020417);
    self->overshoot_threshold = (self->heigt_transform_coef != 0 ? (t1_glyph_space_coord)(fixed_half * (1 << self->log2_pixels_y) / self->heigt_transform_coef) : 0);
    self->ForceBold = pdata->ForceBold;
    self->disable_hinting |= no_grid_fitting;
    self->pass_through |= no_grid_fitting;
    self->charpath_flag = no_grid_fitting;
    self->fix_contour_sign = (!is_resource && self->memory != NULL);
    if (self->fix_contour_sign)
        self->pass_through = false;
    if (self->pass_through)
        return 0;
    code = t1_hinter__set_alignment_zones(mem, self, pdata->OtherBlues.values, pdata->OtherBlues.count, botzone, false);
    if (code >= 0)
        code = t1_hinter__set_alignment_zones(mem, self, pdata->BlueValues.values, min(2, pdata->BlueValues.count), botzone, false);
    if (code >= 0)
        code = t1_hinter__set_alignment_zones(mem, self, pdata->BlueValues.values + 2, pdata->BlueValues.count - 2, topzone, false);
    if (code >= 0)
        code = t1_hinter__set_alignment_zones(mem, self, pdata->FamilyOtherBlues.values, pdata->FamilyOtherBlues.count, botzone, true);
    if (code >= 0)
        code = t1_hinter__set_alignment_zones(mem, self, pdata->FamilyBlues.values, min(2, pdata->FamilyBlues.count), botzone, true);
    if (code >= 0)
        code = t1_hinter__set_alignment_zones(mem, self, pdata->FamilyBlues.values + 2, pdata->FamilyBlues.count - 2, topzone, true);
    if (code >= 0)
        code = t1_hinter__set_stem_snap(mem, self, pdata->StdHW.values, pdata->StdHW.count, 0);
    if (code >= 0)
        code = t1_hinter__set_stem_snap(mem, self, pdata->StdVW.values, pdata->StdVW.count, 1);
    if (code >= 0)
        code = t1_hinter__set_stem_snap(mem, self, pdata->StemSnapH.values, pdata->StemSnapH.count, 0);
    if (code >= 0)
        code = t1_hinter__set_stem_snap(mem, self, pdata->StemSnapV.values, pdata->StemSnapV.count, 1);
    return code;
}

int t1_hinter__set_font42_data(t1_hinter * self, int FontType, gs_type42_data *pdata, bool no_grid_fitting)
{
    t1_hinter__init_outline(self);
    self->FontType = FontType;
    self->BlueScale = 0.039625;	/* A Type 1 spec default. */
    self->blue_shift = 7;	/* A Type 1 spec default. */
    self->blue_fuzz  = 1;	/* A Type 1 spec default. */
    self->suppress_overshoots = (self->BlueScale > self->heigt_transform_coef / (1 << self->log2_pixels_y) - 0.00020417);
    self->overshoot_threshold = (self->heigt_transform_coef != 0 ? (t1_glyph_space_coord)(fixed_half * (1 << self->log2_pixels_y) / self->heigt_transform_coef) : 0);
    self->ForceBold = false;
    self->pass_through |= no_grid_fitting;
    self->charpath_flag = no_grid_fitting;
    self->autohinting = true;
    if (self->pass_through)
        return 0;
    /* Currently we don't provice alignments zones or stem snap. */
    return 0;
}

static inline int t1_hinter__can_add_pole(t1_hinter * self, t1_pole **pole)
{   if (self->pole_count >= self->max_pole_count)
        if(t1_hinter__realloc_array(self->memory, (void **)&self->pole, self->pole0, &self->max_pole_count,
                                    sizeof(self->pole0) / count_of(self->pole0), T1_MAX_POLES, s_pole_array))
            return_error(gs_error_VMerror);
    *pole = &self->pole[self->pole_count];
    return 0;
}

static inline int t1_hinter__add_pole(t1_hinter * self, t1_glyph_space_coord xx, t1_glyph_space_coord yy, enum t1_pole_type type)
{   t1_pole *pole;
    int code = t1_hinter__can_add_pole(self, &pole);

    if (code < 0)
        return code;
    pole->gx = pole->ax = self->cx += xx;
    pole->gy = pole->ay = self->cy += yy;
    pole->ox = pole->oy = 0;
    pole->type = type;
    pole->contour_index = self->contour_count;
    pole->aligned_x = pole->aligned_y = unaligned;
    pole->boundary_length_x = pole->boundary_length_y = 0;
    self->pole_count++;
    return 0;
}

int t1_hinter__sbw(t1_hinter * self, fixed sbx, fixed sby, fixed wx,  fixed wy)
{   self->cx = self->orig_gx = self->subglyph_orig_gx = sbx;
    self->cy = self->orig_gy = self->subglyph_orig_gy = sby;
    self->width_gx = wx;
    self->width_gy = wy;
    return 0;
}

int t1_hinter__sbw_seac(t1_hinter * self, fixed sbx, fixed sby)
{   t1_hinter__adjust_matrix_precision(self, sbx, sby);
    self->cx = self->subglyph_orig_gx = self->orig_gx + sbx;
    self->cy = self->subglyph_orig_gy = self->orig_gy + sby;
    return 0;
}

static bool t1_hinter__find_flex(t1_hinter * self, int k, int contour_beg, int contour_end,
                       t1_glyph_space_coord pixel_g, t1_glyph_space_coord threshold,
                       int i0, int i1, int N, int *j0, int *j1,
                       t1_glyph_space_coord *gm)
{
    int i, j, n = N - 5, m, l;
    t1_glyph_space_coord *p_gc = (!k ? &self->pole[0].gx : &self->pole[0].gy);
    t1_glyph_space_coord *p_gd = (!k ? &self->pole[0].gy : &self->pole[0].gx);
    int offset_gc = (char *)p_gc - (char *)&self->pole[0];
    int offset_gd = (char *)p_gd - (char *)&self->pole[0];
    t1_glyph_space_coord gc0, gc1, gd0, gd1, gcl, gdl, gcp = 0, gdp = 0, gcd, gcm = 0;

    for (i = i0; n; n--, i = i + 1) {
        if (i == contour_end)
            i = contour_beg;
        if (self->pole[i].type == offcurve)
            continue;
        gc0 = *member_prt(t1_glyph_space_coord, &self->pole[i], offset_gc);
        gd0 = *member_prt(t1_glyph_space_coord, &self->pole[i], offset_gd);
        for (j = i1, m = n; m; m--, j--) {
            if (j < contour_beg)
                j = contour_end - 1;
            if (self->pole[j].type == offcurve)
                continue;
            gc1 = *member_prt(t1_glyph_space_coord, &self->pole[j], offset_gc);
            gd1 = *member_prt(t1_glyph_space_coord, &self->pole[j], offset_gd);
            if (any_abs(gd1 - gd0) < pixel_g * 4) /* Arbitrary check for 4 pixels length. */
                continue;
            if (gc0 == gc1) { /* Arbitrary check for strong equality. */
                /* Compute the curvity direction relative to the middle coord. */
                bool gt = false, lt = false;
                double area = 0, area0;
                int dir = 0, prev_dir = 0, dir_change = 0;

                *gm = gc0; /* Safety. */
                /* fixme: optimize: the computaion of gt, lt may be replaced with
                   a longer loop, so that dir_change accounts outer segments.
                   optimize : move the 1st iteratiot outside the loop. */
                for (l = i; ; gcp = gcl, gdp = gdl, prev_dir = dir, l++) {
                    if (l == contour_end)
                        l = contour_beg;
                    gcl = *member_prt(t1_glyph_space_coord, &self->pole[l], offset_gc);
                    gdl = *member_prt(t1_glyph_space_coord, &self->pole[l], offset_gd);
                    if (l != i) {
                        area += (double)(gcp - gc0) * (gdl - gdp) - (double)(gdp - gd0) * (gcl - gcp);
                        gcd = gcl - gc0;
                        gcd = any_abs(gcd);
                        if (gcm < gcd) {
                            *gm = gcl;
                            gcm = gcd;
                        }
                        dir = (gcp < gcl ? 1 : gcp > gcl ? -1 : prev_dir);
                        if (dir * prev_dir < 0)
                            dir_change++;
                    }
                    if (l == j)
                        break;
                    if (gcl < gc0)
                        lt = true;
                    if (gcl > gc0)
                        gt = true;
                }
                if (dir_change > 1)
                    continue;
                if (gcm > threshold)
                    continue;
                area = any_abs(area) / 2; /* Flex area. */
                area0 = (double)(gd1 - gd0) * gcm; /* Surrounding rectangle. */
                area0 = any_abs(area0);
                if (area > area0 * 0.75)
                    continue; /* looks as a rounded rectangle. */
                if (!lt || !gt) {
                    int ii = i - 1, jj = j + 1;
                    t1_glyph_space_coord gii, gjj;

                    if (ii < contour_beg)
                        ii = contour_end - 1;
                    if (jj == contour_end)
                        jj = contour_beg;
                    gii = *member_prt(t1_glyph_space_coord, &self->pole[ii], offset_gc);
                    gjj = *member_prt(t1_glyph_space_coord, &self->pole[jj], offset_gc);
                    if ((lt && gii <= gc0 && gjj <= gc0) ||
                        (gt && gii >= gc0 && gjj >= gc0)) {
                        *j0 = i;
                        *j1 = j;
                        return true;
                    }
                }
            }
        }
        /* Leave the loop here because t1_hinter__fix_missed_flex
           will try the interval starting with the next pole.
           We reserve the 'i' cycle for fonding a "best" flex
           within the interval. */
        break;
    }
    return false;
}

static void t1_hinter__compact_flex(t1_hinter * self, int contour_beg, int contour_end, int i0, int i1, int *pi)
{
    if (i0 > i1) {
        t1_hinter__compact_flex(self, contour_beg, contour_end, i0, contour_end, pi);
        t1_hinter__compact_flex(self, contour_beg, contour_end, contour_beg, i1, pi);
    } else if (i0 < i1) {
        int j;
        int s = i1 - i0 - 1;

        for (j = 0; j < self->hint_range_count; j++) {
            if (self->hint_range[j].beg_pole >= i1)
                self->hint_range[j].beg_pole -= s;
            else if (self->hint_range[j].beg_pole > i0)
                self->hint_range[j].beg_pole = i0;
            if (self->hint_range[j].end_pole >= i1)
                self->hint_range[j].end_pole -= s;
            else if (self->hint_range[j].end_pole > i0)
                self->hint_range[j].end_pole = i0;
        }
        memmove(&self->pole[i0 + 1], &self->pole[i1], sizeof(*self->pole) * (self->pole_count - i1));
        self->contour[self->contour_count] -= s;
        self->pole_count -= s;
        if (*pi >= i1)
            *pi -= s;
        else if (i0 <= *pi)
            *pi = i0;
    }
}

static void t1_hinter__adjust_stem_hints_by_missed_flex(t1_hinter * self, t1_glyph_space_coord g0,
                                                         t1_glyph_space_coord gm, int k)
{
    /* While fixing a missed flex, a part of outline is shifted.
       If there are stem hints pointing to that outline part, we need to move
       their coordinates as well. Here we do so in some hackish way :
       shift any stem that falls into the related coordinate gap.
       It would be nice to have a thinner choice,
       but it appears some complicated, because it could
       multiply stem hints when a hint points to several stems,
       and only some of them are shifted.
       For a simplification we assume that a well designed hint
       must shift all such stems when unbending a flex.
    */
    t1_glyph_space_coord gg = g0;
    int i;

    k = !k;
    if (gm < g0) {
        g0 ^= gm; gm ^= g0; g0 ^= gm;
    }
    for (i = 0; i < self->hint_count; i++)
        if (k == (self->hint[i].type != hstem)) {
            t1_hint *hint = &self->hint[i];

            if (g0 <= hint->g0 && hint->g0 <= gm)
                hint->g0 = hint->ag0 = gg;
            if (g0 <= hint->g1 && hint->g1 <= gm)
                hint->g1 = hint->ag1 = gg;
        }
}

static void t1_hinter__fix_missed_flex(t1_hinter * self)
{
    int contour_beg, contour_end;
    int i, j, k, pj, n, j0, j1;

    if (self->contour_count == 0)
        return;
    contour_beg = self->contour[self->contour_count -1];
    contour_end = self->pole_count - 1; /* the last contour's 'closepath'. */
    if (contour_beg + 8 >= contour_end)
        return;
    for (k = 0; k < 2; k++) {
        t1_glyph_space_coord *p_gc = (!k ? &self->pole[0].gx : &self->pole[0].gy);
        t1_glyph_space_coord *p_gd = (!k ? &self->pole[0].gy : &self->pole[0].gx);
        int offset_gc = (char *)p_gc - (char *)&self->pole[0];
        int offset_gd = (char *)p_gd - (char *)&self->pole[0];
        t1_glyph_space_coord pixel_g = (!k ? self->pixel_gw : self->pixel_gh);
        t1_glyph_space_coord threshold = pixel_g * 5 / 10;
        t1_glyph_space_coord gc0, gc1, gc, gcj, gd = 0, ge, gm;
        int dir = 0, prev_dir;
        bool wrapped = false;

        gc = *member_prt(t1_glyph_space_coord, &self->pole[contour_beg], offset_gc);
        gc0 = gc - threshold;
        gc1 = gc + threshold;
        /* Backward search for a plattue start. */
        for (i = contour_end; i > contour_beg; i--) {
            gcj = *member_prt(t1_glyph_space_coord, &self->pole[i], offset_gc);
            if (self->pole[i].type == offcurve)
                continue;
            if (gcj < gc0 || gcj > gc1)
                break;
        }
        if (i == contour_end) {
            i = contour_beg;
            wrapped = true;
        } else
            i++;
        /* Forward search for all platues. */
        for (;;i++) {
            prev_dir = 0;
            if (i == contour_end) {
                if (wrapped)
                    break;
                wrapped = true;
                i = contour_beg;
            }
            gc = *member_prt(t1_glyph_space_coord, &self->pole[i], offset_gc);
            ge = *member_prt(t1_glyph_space_coord, &self->pole[i], offset_gd);
            gc0 = gc - threshold;
            gc1 = gc + threshold;
            for (pj = i, j = i + 1, n = 0; ; pj = j, j++, n++) {
                if (j == contour_end)
                    j = contour_beg;
                if (j == i)
                    break; /* against bad glyphs. */
                if (self->pole[j].type == offcurve)
                    continue;
                gcj = *member_prt(t1_glyph_space_coord, &self->pole[j], offset_gc);
                if (gcj < gc0 || gcj > gc1)
                    break;
                gd = *member_prt(t1_glyph_space_coord, &self->pole[i], offset_gd);
                dir = (gd > ge ? 1 : -1);
                if (dir * prev_dir < 0)
                    break;
                ge = gd;
                prev_dir = dir;
            }
            if (n < 6)
                continue;
            if (t1_hinter__find_flex(self, k, contour_beg, contour_end, pixel_g, threshold, i, pj, n, &j0, &j1, &gm)) {
                t1_hinter__compact_flex(self, contour_beg, contour_end, j0, j1, &i);
                t1_hinter__adjust_stem_hints_by_missed_flex(self, gc, gm, k);
                contour_end = self->pole_count - 1;
            }
        }
    }
}

int t1_hinter__rmoveto(t1_hinter * self, fixed xx, fixed yy)
{   int code;

    t1_hinter__adjust_matrix_precision(self, xx, yy);
    if (self->flex_count == 0) {
        if (self->pass_through) {
            t1_glyph_space_coord gx = self->cx += xx;
            t1_glyph_space_coord gy = self->cy += yy;
            fixed fx, fy;

            if (self->path_opened) {
                code = gx_path_close_subpath(self->output_path);
                if (code < 0)
                    return code;
                self->path_opened = false;
            }
            g2d(self, gx, gy, &fx, &fy);
            code = gx_path_add_point(self->output_path, fx, fy);
            if (self->flex_count == 0) {
                self->bx = self->cx;
                self->by = self->cy;
            }
            return code;
        }
        if (self->pole_count > 0 && self->pole[self->pole_count - 1].type == moveto)
            self->pole_count--;
        if (self->pole_count > 0 && self->pole[self->pole_count - 1].type != closepath) {
            code = t1_hinter__closepath(self);
            if (code < 0)
                return code;
        }
        if (!self->have_flex)
            t1_hinter__fix_missed_flex(self);
    }
    code = t1_hinter__add_pole(self, xx, yy, moveto);
    if (self->flex_count == 0) {
        self->bx = self->cx;
        self->by = self->cy;
    }
    return code;
}

static inline void t1_hinter__skip_degenerate_segnment(t1_hinter * self, int npoles)
{   /* Degenerate segments amy appear due to import shift with bbox > 4096 */
    int contour_beg = self->contour[self->contour_count], i;

    if (contour_beg >= self->pole_count - npoles)
        return;
    for (i = self->pole_count - npoles - 1; i < self->pole_count - 1; i++)
        if (self->pole[i].ax != self->cx || self->pole[i].ay != self->cy)
            return;
    self->pole_count -= npoles;
}

int t1_hinter__rlineto(t1_hinter * self, fixed xx, fixed yy)
{
    t1_hinter__adjust_matrix_precision(self, xx, yy);
    if (self->pass_through) {
        t1_glyph_space_coord gx = self->cx += xx;
        t1_glyph_space_coord gy = self->cy += yy;
        fixed fx, fy;

        self->path_opened = true;
        g2d(self, gx, gy, &fx, &fy);
        return gx_path_add_line(self->output_path, fx, fy);
    } else {
        int code = t1_hinter__add_pole(self, xx, yy, oncurve);

        if (code < 0)
            return code;
        t1_hinter__skip_degenerate_segnment(self, 1);
        return 0;
    }
}

int t1_hinter__rcurveto(t1_hinter * self, fixed xx0, fixed yy0, fixed xx1, fixed yy1, fixed xx2, fixed yy2)
{
    t1_hinter__adjust_matrix_precision(self, xx0, yy0);
    t1_hinter__adjust_matrix_precision(self, xx1, yy1);
    t1_hinter__adjust_matrix_precision(self, xx2, yy2);
    if (self->pass_through) {
        t1_glyph_space_coord gx0 = self->cx += xx0;
        t1_glyph_space_coord gy0 = self->cy += yy0;
        t1_glyph_space_coord gx1 = self->cx += xx1;
        t1_glyph_space_coord gy1 = self->cy += yy1;
        t1_glyph_space_coord gx2 = self->cx += xx2;
        t1_glyph_space_coord gy2 = self->cy += yy2;
        fixed fx0, fy0, fx1, fy1, fx2, fy2;

        self->path_opened = true;
        g2d(self, gx0, gy0, &fx0, &fy0);
        g2d(self, gx1, gy1, &fx1, &fy1);
        g2d(self, gx2, gy2, &fx2, &fy2);
        return gx_path_add_curve(self->output_path, fx0, fy0, fx1, fy1, fx2, fy2);
    } else {
        int code;

        code = t1_hinter__add_pole(self, xx0, yy0, offcurve);
        if (code < 0)
            return code;
        code = t1_hinter__add_pole(self, xx1, yy1, offcurve);
        if (code < 0)
            return code;
        code = t1_hinter__add_pole(self, xx2, yy2, oncurve);
        if (code < 0)
            return code;
        t1_hinter__skip_degenerate_segnment(self, 3);
        return 0;
    }
}

void t1_hinter__setcurrentpoint(t1_hinter * self, fixed xx, fixed yy)
{
    t1_hinter__adjust_matrix_precision(self, xx, yy);
    if (self->FontType != 2) {
        /* We use this function to set a subglyph origin
           for composite glyphs in Type 2 fonts.
         */
        self->cx = xx;
        self->cy = yy;
    } else if (self->cx != xx || self->cy != yy) {
        /* Type 1 spec reads : "The setcurrentpoint command is used only
           in conjunction with results from OtherSubrs procedures."
           We guess that such cases don't cause a real coordinate change
           (our testbase shows that). But we met a font
           (see comparefiles/type1-ce1_setcurrentpoint.ps) which use
           setcurrentpoint immediately before moveto, with no conjunction
           with OtherSubrs. (The check above is debug purpose only.)
         */
        self->cx = xx;
        self->cy = yy;
    }
}

int t1_hinter__closepath(t1_hinter * self)
{   if (self->pass_through) {
        self->path_opened = false;
        return gx_path_close_subpath(self->output_path);
    } else {
        int contour_beg = self->contour[self->contour_count], code;

        if (contour_beg == self->pole_count)
            return 0; /* maybe a single trailing moveto */
        if (self->bx == self->cx && self->by == self->cy) {
            /* Don't create degenerate segment */
            self->pole[self->pole_count - 1].type = closepath;
        } else {
            t1_glyph_space_coord cx = self->cx, cy = self->cy;

            self->cx = self->bx;
            self->cy = self->by;
            code = t1_hinter__add_pole(self, 0, 0, closepath);
            if (code < 0)
                return code;
            self->cx = cx;
            self->cy = cy;
        }
        self->contour_count++;
        if (self->contour_count >= self->max_contour_count)
            if(t1_hinter__realloc_array(self->memory, (void **)&self->contour, self->contour0, &self->max_contour_count,
                                        sizeof(self->contour0) / count_of(self->contour0), T1_MAX_CONTOURS, s_contour_array))
                return_error(gs_error_VMerror);
        self->contour[self->contour_count] = self->pole_count;
        return 0;
    }
}

int t1_hinter__end_subglyph(t1_hinter * self)
{
    if (self->pass_through)
        return 0;
    self->subglyph_count++;
    if (self->subglyph_count >= self->max_subglyph_count)
        if(t1_hinter__realloc_array(self->memory, (void **)&self->subglyph, self->subglyph0, &self->max_subglyph_count,
                                    sizeof(self->subglyph0) / count_of(self->subglyph0), T1_MAX_SUBGLYPHS, s_subglyph_array))
            return_error(gs_error_VMerror);
    self->subglyph[self->subglyph_count] = self->contour_count;
    return 0;
}

static inline int t1_hinter__can_add_hint(t1_hinter * self, t1_hint **hint)
{   if (self->hint_count >= self->max_hint_count)
        if(t1_hinter__realloc_array(self->memory, (void **)&self->hint, self->hint0, &self->max_hint_count,
                                    sizeof(self->hint0) / count_of(self->hint0), T1_MAX_HINTS, s_hint_array))
            return_error(gs_error_VMerror);
    *hint = &self->hint[self->hint_count];
    return 0;
}

int t1_hinter__flex_beg(t1_hinter * self)
{   if (self->flex_count != 0)
        return_error(gs_error_invalidfont);
    self->flex_count++;
    self->have_flex = true;
    if (self->pass_through)
        return t1_hinter__rmoveto(self, 0, 0);
    return 0;
}

int t1_hinter__flex_point(t1_hinter * self)
{   if (self->flex_count == 0)
        return_error(gs_error_invalidfont);
    self->flex_count++;
    return 0;
}

int t1_hinter__flex_end(t1_hinter * self, fixed flex_height)
{   t1_pole *pole0, *pole1, *pole4;
    t1_hinter_space_coord ox, oy;
    const int32_t div_x = self->g2o_fraction << self->log2_pixels_x;
    const int32_t div_y = self->g2o_fraction << self->log2_pixels_y;

    if (self->flex_count != 8)
        return_error(gs_error_invalidfont);
    /* We've got 8 poles accumulated in pole array. */
    pole0 = &self->pole[self->pole_count - 8];
    pole1 = &self->pole[self->pole_count - 7];
    pole4 = &self->pole[self->pole_count - 4];
    g2o(self, pole4->gx - pole1->gx, pole4->gy - pole1->gy, &ox, &oy);
    if (any_abs(ox) > div_x * fixed2float(flex_height) / 100 ||
        any_abs(oy) > div_y * fixed2float(flex_height) / 100) {
        /* do with curves */
        if (self->pass_through) {
            fixed fx0, fy0, fx1, fy1, fx2, fy2;
            int code;

            g2d(self, pole0[2].gx, pole0[2].gy, &fx0, &fy0);
            g2d(self, pole0[3].gx, pole0[3].gy, &fx1, &fy1);
            g2d(self, pole0[4].gx, pole0[4].gy, &fx2, &fy2);
            code = gx_path_add_curve(self->output_path, fx0, fy0, fx1, fy1, fx2, fy2);
            if (code < 0)
                return code;
            g2d(self, pole0[5].gx, pole0[5].gy, &fx0, &fy0);
            g2d(self, pole0[6].gx, pole0[6].gy, &fx1, &fy1);
            g2d(self, pole0[7].gx, pole0[7].gy, &fx2, &fy2);
            self->flex_count = 0;
            self->pole_count = 0;
            return gx_path_add_curve(self->output_path, fx0, fy0, fx1, fy1, fx2, fy2);
        } else {
            memmove(pole1, pole1 + 1, (sizeof(self->pole0) / count_of(self->pole0)) * 7);
            pole0[1].type = pole0[2].type = offcurve;
            pole0[3].type = oncurve;
            pole0[4].type = pole0[5].type = offcurve;
            pole0[6].type = oncurve;
            self->pole_count--;
        }
    } else {
        /* do with line */
        if (self->pass_through) {
            fixed fx, fy;

            g2d(self, pole0[7].gx, pole0[7].gy, &fx, &fy);
            self->flex_count = 0;
            self->pole_count = 0;
            return gx_path_add_line(self->output_path, fx, fy);
        } else {
            pole0[1] = pole0[7];
            pole0[1].type = oncurve;
            self->pole_count -= 6;
        }
    }
    self->flex_count = 0;
    return 0;
}

static inline int t1_hinter__can_add_hint_range(t1_hinter * self, t1_hint_range **hint_range)
{   if (self->hint_range_count >= self->max_hint_range_count)
        if(t1_hinter__realloc_array(self->memory, (void **)&self->hint_range, self->hint_range0, &self->max_hint_range_count,
                                    sizeof(self->hint_range0) / count_of(self->hint_range0), T1_MAX_HINTS, s_hint_range_array))
            return_error(gs_error_VMerror);
    *hint_range = &self->hint_range[self->hint_range_count];
    return 0;
}

static inline int t1_hinter__can_add_hint_applying(t1_hinter * self, t1_hint_applying **hint_applying)
{   if (self->hint_applying_count >= self->max_hint_applying_count)
        if(t1_hinter__realloc_array(self->memory, (void **)&self->hint_applying, self->hint_applying0, &self->max_hint_applying_count,
                                    sizeof(self->hint_applying0) / count_of(self->hint_applying0), T1_MAX_HINTS, s_hint_applying_array))
            return_error(gs_error_VMerror);
    *hint_applying = &self->hint_applying[self->hint_applying_count];
    return 0;
}

int t1_hinter__hint_mask(t1_hinter * self, byte *mask)
{   int hint_count, i;

    if (self->disable_hinting)
        return 0;
    hint_count = self->hint_count;

    for(i = 0; i < hint_count; i++) {
        bool activate = (mask != NULL && (mask[i >> 3] & (0x80 >> (i & 7))) != 0);
        t1_hint *hint = &self->hint[i];

        if (activate) {
            if (hint->range_index != -1 &&
                (self->hint_range[hint->range_index].end_pole == -1 ||
                 self->hint_range[hint->range_index].end_pole == self->pole_count)) {
                 /* continie the range */
                self->hint_range[hint->range_index].end_pole = -1;
            } else {
                /* add new range */
                t1_hint_range *hint_range;
                int code = t1_hinter__can_add_hint_range(self, &hint_range);

                if (code < 0)
                   return code;
                hint_range->beg_pole = self->pole_count;
                hint_range->end_pole = -1;
                hint_range->next = hint->range_index;
                hint->range_index = self->hint_range_count;
                self->hint_range_count++;
            }
        } else {
            if (hint->range_index != -1 &&
                self->hint_range[hint->range_index].end_pole == -1) {
                /* deactivate */
                self->hint_range[hint->range_index].end_pole = self->pole_count;
            } else
                DO_NOTHING;
        }
    }
    return 0;
}

int t1_hinter__drop_hints(t1_hinter * self)
{   if (self->disable_hinting)
        return 0;
    if (self->primary_hint_count == -1)
        self->primary_hint_count = self->hint_range_count;
    return t1_hinter__hint_mask(self, NULL);
}

static inline int t1_hinter__stem(t1_hinter * self, enum t1_hint_type type, unsigned short stem3_index
                                                  , fixed v0, fixed v1, int side_mask)
{   t1_hint *hint;
    t1_glyph_space_coord s = (type == hstem ? self->subglyph_orig_gy : self->subglyph_orig_gx);
    t1_glyph_space_coord g0 = s + v0;
    t1_glyph_space_coord g1 = s + v0 + v1;
    t1_hint_range *range;
int i, code;

    t1_hinter__adjust_matrix_precision(self, (side_mask & 1 ? g0 : g1), (side_mask & 2 ? g1 : g0));
    for (i = 0; i < self->hint_count; i++)
        if (self->hint[i].type == type &&
                self->hint[i].g0 == g0 && self->hint[i].g1 == g1 &&
                self->hint[i].side_mask == side_mask)
            break;
    if (i < self->hint_count)
        hint = &self->hint[i];
    else {
        code = t1_hinter__can_add_hint(self, &hint);
        if (code < 0)
            return code;
        hint->type = type;
        hint->g0 = hint->ag0 = g0;
        hint->g1 = hint->ag1 = g1;
        hint->aligned0 = hint->aligned1 = unaligned;
        hint->q0 = hint->q1 = max_int;
        hint->b0 = hint->b1 = false;
        hint->stem3_index = stem3_index;
        hint->range_index = -1;
        hint->side_mask = side_mask;
        hint->stem_snap_index0 = hint->stem_snap_index1 = 0;
        hint->boundary_length0 = hint->boundary_length1 = 0;
    }
    code = t1_hinter__can_add_hint_range(self, &range);
    if (code < 0)
        return code;
    range->beg_pole = self->pole_count;
    range->end_pole = -1;
    range->next = hint->range_index;
    hint->range_index = range - self->hint_range;
    if (i >= self->hint_count)
        self->hint_count++;
    self->hint_range_count++;
    return 0;
}

int t1_hinter__dotsection(t1_hinter * self)
{   if (self->pole_count == 0 || self->pole[self->pole_count - 1].type != moveto)
        return 0; /* We store beginning dotsection hints only. */
    if (self->disable_hinting)
        return 0;
    return t1_hinter__stem(self, dot, 0, 0, 0, 0);
}

int t1_hinter__hstem(t1_hinter * self, fixed x0, fixed x1)
{   if (self->disable_hinting)
        return 0;
    return t1_hinter__stem(self, hstem, 0, x0, x1, 3);
}

int t1_hinter__overall_hstem(t1_hinter * self, fixed x0, fixed x1, int side_mask)
{   /* True Type autohinting only. */
    if (self->disable_hinting)
        return 0;
    return t1_hinter__stem(self, hstem, 0, x0, x1, side_mask);
}

int t1_hinter__vstem(t1_hinter * self, fixed y0, fixed y1)
{   if (self->disable_hinting)
        return 0;
    return t1_hinter__stem(self, vstem, 0, y0, y1, 3);
}

int t1_hinter__hstem3(t1_hinter * self, fixed x0, fixed x1, fixed x2, fixed x3, fixed x4, fixed x5)
{   int code;

    if (self->disable_hinting)
        return 0;
    code = t1_hinter__stem(self, hstem, 1, x0, x1, 3);
    if (code < 0)
        return code;
    code = t1_hinter__stem(self, hstem, 2, x2, x3, 3);
    if (code < 0)
        return code;
    return t1_hinter__stem(self, hstem, 3, x4, x5, 3);
}

int t1_hinter__vstem3(t1_hinter * self, fixed y0, fixed y1, fixed y2, fixed y3, fixed y4, fixed y5)
{   int code;

    if (self->disable_hinting)
        return 0;
    code = t1_hinter__stem(self, vstem, 1, y0, y1, 3);
    if (code < 0)
        return code;
    code = t1_hinter__stem(self, vstem, 2, y2, y3, 3);
    if (code < 0)
        return code;
    return t1_hinter__stem(self, vstem, 3, y4, y5, 3);
}

/* --------------------- t1_hinter class members - accessories --------------------*/

int t1_hinter__is_x_fitting(t1_hinter * self)
{   return self->grid_fit_x;
}

/* --------------------- t1_hinter class members - the hinting --------------------*/

static inline int t1_hinter__segment_beg(t1_hinter * self, int pole_index)
{   int contour_index = self->pole[pole_index].contour_index;
    int beg_contour_pole = self->contour[contour_index];
    int end_contour_pole = self->contour[contour_index + 1] - 2;
    int prev = ranger_step_b(pole_index, beg_contour_pole, end_contour_pole);

    while (self->pole[prev].type == offcurve)
        prev = ranger_step_b(prev, beg_contour_pole, end_contour_pole);
    return prev;
}

static inline int t1_hinter__segment_end(t1_hinter * self, int pole_index)
{   int contour_index = self->pole[pole_index].contour_index;
    int beg_contour_pole = self->contour[contour_index];
    int end_contour_pole = self->contour[contour_index + 1] - 2;
    int next = ranger_step_f(pole_index, beg_contour_pole, end_contour_pole);

    while (self->pole[next].type == offcurve)
        next = ranger_step_f(next, beg_contour_pole, end_contour_pole);
    return next;
}

static void t1_hinter__compute_y_span(t1_hinter * self)
{
    int n = self->pole_count - 1;
    int i;

    if (n > 1) {
        /* For non-space characters ignore the trailing moveto.
           Rather it could give a baseline,
           it is not guaranteedly good,
           and doesn't allow a stable recognition
           of the upper side of a dot, comma, etc.. */
        n--;
    } else if (n < 0) {
        return; /* empty glyph */
    }
    self->ymin = self->ymax = self->pole[0].gy;
    for (i = 1; i < n; i++) {
        if (self->ymin > self->pole[i].gy)
            self->ymin = self->pole[i].gy;
        if (self->ymax < self->pole[i].gy)
            self->ymax = self->pole[i].gy;
    }
    self->ymid = (self->ymax + self->ymin) / 2;
}

static void t1_hinter__simplify_representation(t1_hinter * self)
{   int i, j;
    int last_pole = self->pole_count - 1;
    int primary_hint_count = self->primary_hint_count;

    if (last_pole > 1 && self->pole[last_pole -1].type == closepath)
        last_pole -= 2; /* Skip the trailing moveto. */
    if (self->pole_count <= 1)
        return; /* An empty glyph (only a trailing moveto). */
    /* Remove hints which are disabled with !grid_fit_x, !grid_fit_y.
     * We can't do before import is completed due to hint mask commands.
     */
    if (!self->grid_fit_x || !self->grid_fit_y) {
        for (i = j = 0; i < self->hint_count; i++)
            if ((self->hint[i].type == vstem && !self->grid_fit_x) ||
                (self->hint[i].type == hstem && !self->grid_fit_y)) {
                if (i < primary_hint_count)
                    self->primary_hint_count--;
                continue; /* skip it. */
            } else {
                if (i != j)  /* for Valgrind */
                    self->hint[j] = self->hint[i];
                j++;
            }
        self->hint_count = j;
    }
    for (i = 0; i < self->hint_range_count; i++) {
        t1_hint_range *hint_range = &self->hint_range[i];

        j = hint_range->beg_pole;
        if (self->pole[j].type == closepath)
            hint_range->beg_pole = ++j;
        else {
            if (self->pole[j].type == offcurve)
                hint_range->beg_pole = --j;
            if (self->pole[j].type == offcurve)
                hint_range->beg_pole = --j;
        }
        j = hint_range->end_pole;
        if (j == -1)
            hint_range->end_pole = j = last_pole;
        if (self->pole[j].type == offcurve)
            hint_range->end_pole = ++j;
        if (self->pole[j].type == offcurve)
            hint_range->end_pole = ++j;
    }
    /*  moveto's were needed to decode path correctly.
        We don't need them so far.
        Replace 'moveto' with 'oncurve' :
    */
    for (i = 0; i <= self->contour_count; i++)
        if (self->pole[self->contour[i]].type == moveto)
            self->pole[self->contour[i]].type = oncurve;
    /*  After the decoding, hint commands refer to the last pole before HR occures.
        Move pointers to the beginning segment pole.
    */
    for (j = 0; j < self->hint_range_count; j++) {
        int beg_pole = self->hint_range[j].beg_pole;
        int contour_index = self->pole[beg_pole].contour_index;
        int contour_beg_pole = self->contour[contour_index];

        if (beg_pole > contour_beg_pole && beg_pole < last_pole)
            self->hint_range[j].beg_pole = t1_hinter__segment_beg(self, beg_pole);
    }
}

static inline bool t1_hinter__is_small_angle(t1_hinter * self, int pole_index0, int pole_index1,
        long tan_x, long tan_y, int alpha, int alpha_div, int *quality)
{   long gx = self->pole[pole_index1].gx - self->pole[pole_index0].gx;
    long gy = self->pole[pole_index1].gy - self->pole[pole_index0].gy;
    long vp = mul_shift(gx, tan_y, _fixed_shift) - mul_shift(gy, tan_x, _fixed_shift);
    long sp = mul_shift(gx, tan_x, _fixed_shift) + mul_shift(gy, tan_y, _fixed_shift);
    long vp1 = any_abs(vp), sp1 = any_abs(sp);

    if (gx == 0 && gy == 0) {
        *quality = max_int;
        return false;
    }
    if (vp1 >= sp1) {
        *quality = max_int;
        return false;
    }
    if (vp1 / alpha_div > sp1 / alpha) {
        *quality = max_int;
        return false;
    }
    *quality = vp1 * 100 / sp1; /* The best quality is 0. */
    return true;
}

static inline bool t1_hinter__next_contour_pole(t1_hinter * self, int pole_index)
{   int contour_index = self->pole[pole_index].contour_index;
    int beg_contour_pole = self->contour[contour_index];
    int end_contour_pole = self->contour[contour_index + 1] - 2;

    return ranger_step_f(pole_index, beg_contour_pole, end_contour_pole);
}

static inline bool t1_hinter__is_good_tangent(t1_hinter * self, int pole_index, long tan_x, long tan_y, int *quality)
{   int contour_index = self->pole[pole_index].contour_index;
    int beg_contour_pole = self->contour[contour_index];
    int end_contour_pole = self->contour[contour_index + 1] - 2, prev, next;
    int const alpha = 9, alpha_div = 10;
    int quality0, quality1;
    bool good0, good1;

    prev = ranger_step_b(pole_index, beg_contour_pole, end_contour_pole);
    good0 = t1_hinter__is_small_angle(self, prev, pole_index, tan_x, tan_y, alpha, alpha_div, &quality0);
    if (quality0 == 0) {
        *quality = 0;
        return true;
    }
    next = ranger_step_f(pole_index, beg_contour_pole, end_contour_pole);
    good1 = t1_hinter__is_small_angle(self, next, pole_index, tan_x, tan_y, alpha, alpha_div, &quality1);
    *quality = min(quality0, quality1);
    return good0 || good1;
}

static void t1_hinter__compute_type1_stem_ranges(t1_hinter * self)
{   int j;
    int end_range_pole = self->pole_count - 3;
    int primary_hint_count = self->primary_hint_count;

    if (self->hint_count == 0)
        return;
    if (primary_hint_count == -1)
        primary_hint_count = self->hint_range_count;
    /* Process primary hints - ranges are entire glyph : */
    for(j = 0; j < primary_hint_count; j++)      {
        self->hint_range[j].beg_pole = 0;
        self->hint_range[j].end_pole = end_range_pole;
    }
    /*  Note that ranges of primary hints may include a tail of the hint array
        due to multiple contours. Primary hints have a lesser priority,
        so apply them first, and possibly recover later.
    */
}

static void t1_hinter__compute_type2_stem_ranges(t1_hinter * self)
{   int i;

    for (i = 0; i < self->hint_range_count; i++)
        if (self->hint_range[i].end_pole == -1)
            self->hint_range[i].end_pole = self->pole_count - 2;
}

static bool t1_hinter__is_stem_boundary_near(t1_hinter * self, const t1_hint *hint,
                t1_glyph_space_coord g, int boundary)
{
    t1_glyph_space_coord const fuzz = self->blue_fuzz; /* comparefiles/tpc2.ps */

    return any_abs(g - (boundary ? hint->g1 : hint->g0)) <= fuzz;
}

static int t1_hinter__is_stem_hint_applicable(t1_hinter * self, t1_hint *hint, int pole_index, int *quality)
{   /* We don't check hint->side_mask because the unused coord should be outside the design bbox. */
    int k;

    if (hint->type == hstem
            && ((k = 1, t1_hinter__is_stem_boundary_near(self, hint, self->pole[pole_index].gy, 0)) ||
                (k = 2, t1_hinter__is_stem_boundary_near(self, hint, self->pole[pole_index].gy, 1)))
            && t1_hinter__is_good_tangent(self, pole_index, 1, 0, quality))
        return k;
    if (hint->type == vstem
            && ((k = 1, t1_hinter__is_stem_boundary_near(self, hint, self->pole[pole_index].gx, 0)) ||
                (k = 2, t1_hinter__is_stem_boundary_near(self, hint, self->pole[pole_index].gx, 1)))
            && t1_hinter__is_good_tangent(self, pole_index, 0, 1, quality))
        return k;
    return 0;
}

static t1_zone * t1_hinter__find_zone(t1_hinter * self, t1_glyph_space_coord pole_y, bool curve, bool convex, bool concave)
{   bool maybe_top = !curve || convex;
    bool maybe_bot = !curve || concave;
    int i;

    for (i = 0; i < self->zone_count; i++) {
        t1_zone *zone = &self->zone[i];
        if ((maybe_top && zone->type == topzone) || (maybe_bot && zone->type == botzone))
            if (zone->y_min <= pole_y && pole_y <= zone->y_max)
                return zone;
    }
    return NULL;
    /*todo: optimize narrowing the search range */
}

static void t1_hinter__align_to_grid__general(t1_hinter * self, int32_t unit,
            t1_glyph_space_coord gx, t1_glyph_space_coord gy,
            t1_hinter_space_coord *pdx, t1_hinter_space_coord *pdy,
            bool align_to_pixels, bool absolute)
{
    long div_x = rshift(unit, (align_to_pixels ? (int)self->log2_pixels_x : self->log2_subpixels_x));
    long div_y = rshift(unit, (align_to_pixels ? (int)self->log2_pixels_y : self->log2_subpixels_y));
    t1_hinter_space_coord ox, oy, dx, dy;

    g2o(self, gx, gy, &ox, &oy);
    if (absolute) {
        ox += self->orig_ox;
        oy += self->orig_oy;
    }
    dx = ox % div_x;
    dy = oy % div_y; /* So far dx and dy are 19 bits */
    if (dx > div_x / 2 )
        dx = - div_x + dx;
    else if (dx < - div_x / 2)
        dx = div_x + dx;
    if (dy > div_y / 2)
        dy = - div_y + dy;
    else if (dy < - div_y / 2)
        dy = div_y + dy;
    *pdx = dx;
    *pdy = dy;
}

static void t1_hinter__align_to_grid__final(t1_hinter * self,
            t1_glyph_space_coord *x, t1_glyph_space_coord *y,
            t1_hinter_space_coord dx, t1_hinter_space_coord dy)
{
    t1_glyph_space_coord gxd, gyd;

    o2g(self, dx, dy, &gxd, &gyd);
    if (self->grid_fit_x) {
        *x -= gxd;
        *x = (*x + 7) & ~15; /* Round to suppress small noise : */
    }
    if (self->grid_fit_y) {
        *y -= gyd;
        *y = (*y + 7) & ~15; /* Round to suppress small noise : */
    }
}

static void t1_hinter__hint_stem_snap_range(t1_hinter * self,
                                        t1_glyph_space_coord w0, t1_glyph_space_coord w1, bool horiz,
                                        short *index0, short *index1)
{   int k = (horiz ? 0 : 1), i;
    bool index0_set = false;

    *index0 = 0;
    *index1 = -1;
    for (i = 0; i < self->stem_snap_count[k]; i++) {
        if (w0 > self->stem_snap[k][i])
            continue;
        if (!index0_set) {
            index0_set = true;
            *index0 = i;
        }
        if (w1 < self->stem_snap[k][i])
            break;
        *index1 = i;
    }
}

static void t1_hinter__align_to_grid(t1_hinter * self, int32_t unit,
            t1_glyph_space_coord *x, t1_glyph_space_coord *y, bool align_to_pixels)
{   if (unit > 0) {
        t1_hinter_space_coord dx, dy;

        t1_hinter__align_to_grid__general(self, unit, *x, *y, &dx, &dy, align_to_pixels, align_to_pixels);
        t1_hinter__align_to_grid__final(self, x, y, dx, dy);
    }
}

static void t1_hinter_compute_stem_snap_range_hv(t1_hinter * self, int hv)
{
    const enum t1_hint_type T[] = {hstem, vstem};
    int i, j;
    enum t1_hint_type t = T[hv];
    bool horiz = (t == hstem);
    t1_glyph_space_coord pixel_g = (horiz ? self->pixel_gh : self->pixel_gw);
    int stem_snap_count = self->stem_snap_count[hv];

    memset(self->stem_snap_vote, 0, stem_snap_count * sizeof(self->stem_snap_vote[0]));
    for (i = 0; i < self->hint_count; i++) {
        if (self->hint[i].type == t) {
            t1_glyph_space_coord gw = any_abs(self->hint[i].g1 - self->hint[i].g0);

            t1_hinter__hint_stem_snap_range(self, gw - pixel_g + 1, gw + pixel_g - 1, horiz,
                &self->hint[i].stem_snap_index0, &self->hint[i].stem_snap_index1);
            for (j = self->hint[i].stem_snap_index0; j <= self->hint[i].stem_snap_index1; j++)
                self->stem_snap_vote[j]++;
        }
    }
    for (i = 0; i < self->hint_count; i++) {
        if (self->hint[i].type == t) {
            int m = 0, mj = -1, d, md = pixel_g * 2;
            t1_glyph_space_coord gw = any_abs(self->hint[i].g1 - self->hint[i].g0);

            for (j = self->hint[i].stem_snap_index0; j <= self->hint[i].stem_snap_index1; j++) {
                if (m < self->stem_snap_vote[j]) {
                    m = self->stem_snap_vote[j];
                    mj = j;
                    md = any_abs(gw - pixel_g / 5 - self->stem_snap[hv][mj]);
                } else {
                    d = any_abs(gw - pixel_g / 5 - self->stem_snap[hv][j]);
                    if (md > d) {
                        md = d;
                        mj = j;
                    }
                }
            }
            self->hint[i].stem_snap_index0 = mj;
        }
    }
}

static void t1_hinter_compute_stem_snap_range(t1_hinter * self)
{
    if (self->stem_snap_count[0] > 1)
        t1_hinter_compute_stem_snap_range_hv(self, 0);
    if (self->stem_snap_count[1] > 1)
        t1_hinter_compute_stem_snap_range_hv(self, 1);
}

static void t1_hinter__align_stem_width(t1_hinter * self, t1_glyph_space_coord *pgw, const t1_hint *hint)
{
    bool horiz = (hint->type == hstem);
    t1_glyph_space_coord gw = *pgw;
    t1_glyph_space_coord pixel_g = (horiz ? self->pixel_gh : self->pixel_gw);
    t1_glyph_space_coord gwe;

    if (!self->keep_stem_width || pixel_g == 0)
        return;
    if (hint->stem_snap_index0 >= 0 && self->stem_snap_count[horiz ? 0 : 1] > 0) {
        t1_glyph_space_coord w0 = self->stem_snap[horiz ? 0 : 1][hint->stem_snap_index0];
        t1_glyph_space_coord thr0 = pixel_g * 70 / 100, thr1 = pixel_g * 35 / 100;

        if (gw - thr0 <= w0 && w0 <= gw + thr1)
            gw = w0;
    }
    gwe = gw % pixel_g;
    if (gw >= pixel_g && gwe < pixel_g / 2)
        gw -= gwe;
    else
        gw += pixel_g - gwe;
    *pgw = gw;
}

static void t1_hinter__align_stem_to_grid(t1_hinter * self, int32_t unit,
            t1_glyph_space_coord *x0, t1_glyph_space_coord *y0,
            t1_glyph_space_coord  x1, t1_glyph_space_coord  y1,
            bool align_to_pixels, const t1_hint *hint)
{   /* Implemented for Bug 687578 "T1 hinter disturbs stem width". */
    /* fixme: optimize. */
    if (unit > 0) {
        bool horiz = (hint->type == hstem);
        t1_glyph_space_coord gw = (horiz ? y1 - *y0 : x1 - *x0);
        t1_glyph_space_coord GW = any_abs(gw), GW0 = GW;
        bool positive = (gw >= 0);
        int19 cf = (horiz ? self->heigt_transform_coef_rat : self->width_transform_coef_rat);
        t1_hinter_space_coord dx0, dy0, dx1, dy1, dgw;

        t1_hinter__align_to_grid__general(self, unit, *x0, *y0, &dx0, &dy0, align_to_pixels, align_to_pixels);
        t1_hinter__align_to_grid__general(self, unit,  x1,  y1, &dx1, &dy1, align_to_pixels, align_to_pixels);
        t1_hinter__align_stem_width(self, &GW, hint);
        dgw = g2o_dist(GW - GW0, cf);
        if ((horiz ? (!self->transposed ? self->ctmf.yy : self->ctmf.xy)
                   : (!self->transposed ? self->ctmf.xx : self->ctmf.yx)) < 0)
            dgw = - dgw;
        if (horiz) {
            t1_hinter_space_coord ddy1 = (positive ? dy0 - dgw : dy0 + dgw);
            t1_hinter_space_coord ddy0 = (positive ? dy1 + dgw : dy1 - dgw);

            if (any_abs(dy0 + ddy1) > any_abs(dy1 + ddy0))
                dy0 = ddy0;
        } else {
            t1_hinter_space_coord ddx1 = (positive ? dx0 - dgw : dx0 + dgw);
            t1_hinter_space_coord ddx0 = (positive ? dx1 + dgw : dx1 - dgw);

            if (any_abs(dx0 + ddx1) > any_abs(dx1 + ddx0))
                dx0 = ddx0;
        }
        t1_hinter__align_to_grid__final(self, x0, y0, dx0, dy0);
    }
}

#if ADOBE_OVERSHOOT_COMPATIBILIY
static inline t1_hinter_space_coord g2o_dist_blue(t1_hinter * h, t1_glyph_space_coord gw)
{   double W = fixed2float(gw);
    double w = W * (h->resolution * h->font_size * h->base_font_scale - h->BlueScale) + 1;

    return (t1_hinter_space_coord)(w * h->g2o_fraction);
    /* todo : exclude floating point */
}

static void t1_hinter__add_overshoot(t1_hinter * self, t1_zone * zone, t1_glyph_space_coord * x, t1_glyph_space_coord * y)
{   t1_glyph_space_coord gy = *y;
    /* t1_glyph_space_coord gw = any_abs(zone->overshoot_y - zone->y); */
    t1_glyph_space_coord gw = any_abs(gy - zone->y);
    t1_hinter_space_coord ow = g2o_dist_blue(self, gw);
    t1_hinter_space_coord ow1 = ow / self->g2o_fraction * self->g2o_fraction;
    t1_glyph_space_coord gw1 = o2g_dist(self, ow1, self->heigt_transform_coef_inv);

    *y = zone->y + (zone->type == topzone ? gw1 : -gw1);
}
#endif

static enum t1_align_type t1_hinter__compute_aligned_coord(t1_hinter * self,
            t1_glyph_space_coord * gc, int segment_index, fixed t, const t1_hint *hint,
            enum t1_align_type align0)
{   /* Returns true, if alignment zone is applied. */
    /* t is 0 or 0.5, and it is always 0 for curves. */
    bool horiz = (hint->type == hstem);
    enum t1_align_type align = align0;
    t1_glyph_space_coord gx = self->pole[segment_index].gx, gx0;
    t1_glyph_space_coord gy = self->pole[segment_index].gy, gy0;
    t1_glyph_space_coord gc0 = (horiz ? gy : gx);
    bool align_by_stem =
                align0 == unaligned	 /* Force aligning outer boundaries
                                            from the TT spot analyzer. */
                && hint->b0 && hint->b1; /* It's a real stem. Contrary
                                            033-52-5873.pdf uses single hint boundaries
                                            to mark top|bottom sides of a glyph,
                                            but their opposite boundaries are dummy coordinates,
                                            which don't correspond to poles. */

    /*  Compute point of specified segment by parameter t : */
    if (t) {
        int next = t1_hinter__segment_end(self, segment_index);
        t1_glyph_space_coord gx1 = self->pole[next].gx;
        t1_glyph_space_coord gy1 = self->pole[next].gy;

        gx = (gx + gx1) / 2;
        gy = (gy + gy1) / 2;
    }
    gx0 = gx;
    gy0 = gy;
    if (horiz) {
        t1_pole * pole = &self->pole[segment_index];
        int contour_index = pole->contour_index;
        int beg_contour_pole = self->contour[contour_index];
        int end_contour_pole = self->contour[contour_index + 1] - 2;
        int prev1 = ranger_step_b(segment_index, beg_contour_pole, end_contour_pole);
        int prev2 = ranger_step_b(prev1        , beg_contour_pole, end_contour_pole);
        int next1 = ranger_step_f(segment_index, beg_contour_pole, end_contour_pole);
        int next2 = ranger_step_f(next1        , beg_contour_pole, end_contour_pole);
        bool forwd_horiz = (any_abs(self->pole[next1].gy - pole->gy) <=
                max(self->blue_fuzz, any_abs(self->pole[next1].gx - pole->gx) / 10));
        bool bckwd_horiz = (any_abs(self->pole[prev1].gy - pole->gy) <=
                max(self->blue_fuzz, any_abs(self->pole[prev1].gx - pole->gx) / 10));
        bool maximum = (self->pole[next1].gy - pole->gy < 0 &&
                        self->pole[prev1].gy - pole->gy < 0);
        bool minimum = (self->pole[next1].gy - pole->gy > 0 &&
                        self->pole[prev1].gy - pole->gy > 0);

        if (forwd_horiz || bckwd_horiz || maximum || minimum) {
            bool forwd_curve = (self->pole[next1].type == offcurve);
            bool bckwd_curve = (self->pole[prev1].type == offcurve);
            bool curve = (bckwd_curve && forwd_curve);
            bool convex  = (curve && self->pole[prev2].gy <= pole->gy &&
                                     self->pole[next2].gy <= pole->gy);
            bool concave = (curve && self->pole[prev2].gy >= pole->gy &&
                                     self->pole[next2].gy >= pole->gy);
            t1_zone *zone = t1_hinter__find_zone(self, pole->gy, curve || maximum || minimum,
                                                convex || maximum, concave || minimum);

            if (zone != NULL &&
                   (forwd_horiz || bckwd_horiz ||
                    (maximum && zone->type == topzone) ||
                    (minimum && zone->type == botzone))) {
                if (self->suppress_overshoots)
#                   if ADOBE_OVERSHOOT_COMPATIBILIY
                        gy = (zone->type == topzone ? zone->overshoot_y : zone->y);
#                   else
                        gy = zone->y;
#                   endif
                else {
                    t1_glyph_space_coord s = zone->y - pole->gy;
                    if (zone->type == topzone)
                        s = -s;
                    if (!curve && s < self->overshoot_threshold)
                        gy = zone->y;
                    else if (s > self->overshoot_threshold) {
                        t1_glyph_space_coord ss = self->overshoot_threshold * 2;

                        if (s < ss) /* Enforce overshoot : */
                            gy = (zone->type == topzone ? zone->y + ss : zone->y - ss);
                        else {
#                           if ADOBE_OVERSHOOT_COMPATIBILIY
                                t1_hinter__add_overshoot(self, zone, &gx, &gy);
#                           endif
                        }
                    }
                }
                align = (zone->type == topzone ? topzn : botzn);
                align_by_stem = false;
            }
        }
    }
    if (align_by_stem) {
        t1_glyph_space_coord gx1, gy1;

        if (horiz) {
            bool b0 = t1_hinter__is_stem_boundary_near(self, hint, gy, 0);
            bool b1 = t1_hinter__is_stem_boundary_near(self, hint, gy, 1);

            gx1 = gx;
            if (b0 && !b1)
                gy1 = hint->g1, align_by_stem = true;
            else if (!b0 && b1)
                gy1 = hint->g0, align_by_stem = true;
            else
                gy1 = 0; /* Quiet the compiler. */
        } else {
            bool b0 = t1_hinter__is_stem_boundary_near(self, hint, gx, 0);
            bool b1 = t1_hinter__is_stem_boundary_near(self, hint, gx, 1);

            gy1 = gy;
            if (b0 && !b1)
                gx1 = hint->g1, align_by_stem = true;
            else if (!b0 && b1)
                gx1 = hint->g0, align_by_stem = true;
            else
                gx1 = 0; /* Quiet the compiler. */
        }
        if (align_by_stem)
            t1_hinter__align_stem_to_grid(self, self->g2o_fraction, &gx, &gy, gx1, gy1,
                    CONTRAST_STEMS || self->align_to_pixels, hint);
    }
    if (!align_by_stem)
        t1_hinter__align_to_grid(self, self->g2o_fraction, &gx, &gy,
                            CONTRAST_STEMS || self->align_to_pixels);
    *gc = gc0 + (horiz ? gy - gy0 : gx - gx0);
    return (align == unaligned ? aligned : align);
}

#define PRESERVE_STEM_SLANT 1 /*   0 - always diminish
                                   1 - preserve iff slanted in design space
                                   2 - always preserve */

static int t1_hinter__find_stem_middle(t1_hinter * self, fixed *t, int pole_index, bool horiz)
{
    /* *t = 0 preserves slant; *t = fixed_half deminishes slant (don't apply to curves). */
    if (PRESERVE_STEM_SLANT == 2) {
        *t = 0;
        return pole_index;
    } else {
        /* For a better representation of arms with a small slope,
           we align their poles. It appears useful for CJK fonts,
           see comparefiles/japan.ps, Bug687603.ps .
           Otherwise (a slightly rotated glyph, see Openhuis_pdf_zw.pdf)
           we align the arm middle, causing the slope to look smaller
         */
        /* We assume proper glyphs, see Type 1 spec, chapter 4. */
        int next = t1_hinter__next_contour_pole(self, pole_index);
        const int alpha = 10;
        int design_slant;
        bool curve = self->pole[next].type == offcurve;
        bool continuing = (horiz ? t1_hinter__is_small_angle(self, next, pole_index, 1, 0, alpha, 1, &design_slant)
                                 : t1_hinter__is_small_angle(self, next, pole_index, 0, 1, alpha, 1, &design_slant));

        if (!PRESERVE_STEM_SLANT || design_slant == 0)
            *t = (!curve && continuing ? fixed_half : 0);
        else
            *t = 0;
        return pole_index;
    }
}

static int t1_hinter__skip_stem(t1_hinter * self, int pole_index, bool horiz)
{   /* We assume proper glyphs, see Type 1 spec, chapter 4. */
    int i = pole_index;
    int next_pole = t1_hinter__next_contour_pole(self, i);
    int next_segm = t1_hinter__segment_end(self, i);
    long tan_x = (horiz ? 1 : 0);
    long tan_y = (horiz ? 0 : 1);
    int quality;

    while (t1_hinter__is_small_angle(self, i, next_pole, tan_x, tan_y, 1000, 1, &quality) && /* The threshold is taken from scratch. */
           t1_hinter__is_small_angle(self, i, next_segm, tan_x, tan_y, 1000, 1, &quality)) {
        i = t1_hinter__segment_end(self, i);
        if (i == pole_index) {
            /* An invalid glyph with <=2 segments in the contour with no angles. */
            break;
        }
        next_pole = t1_hinter__next_contour_pole(self, i);
        next_segm = t1_hinter__segment_end(self, i);
    }
    return i;
}

static void t1_hinter__mark_existing_stems(t1_hinter * self)
{   /* fixme: Duplicated code with t1_hinter__align_stem_commands. */
    int i, j, jj, k;

    for(i = 0; i < self->hint_count; i++)
        if (self->hint[i].type == vstem || self->hint[i].type == hstem)
            for (k = self->hint[i].range_index; k != -1; k = self->hint_range[k].next) {
                int beg_range_pole = self->hint_range[k].beg_pole;
                int end_range_pole = self->hint_range[k].end_pole;
                int quality;

                if (self->pole[beg_range_pole].type == closepath) {
                    /* A workaround for a buggy font from the Bug 687393,
                       which defines a range with 'closepath' only. */
                    beg_range_pole++;
                    if (beg_range_pole > end_range_pole)
                        continue;
                }
                for (j = beg_range_pole; j <= end_range_pole;) {
                    int k = t1_hinter__is_stem_hint_applicable(self, &self->hint[i], j, &quality);
                    if (k == 1)
                        self->hint[i].b0 = true;
                    else if (k == 2)
                        self->hint[i].b1 = true;
                    {   /* Step to the next pole in the range : */
                        jj = j;
                        j = t1_hinter__segment_end(self, j);
                        if (j <= jj) /* Rolled over contour end ? */
                            j = self->contour[self->pole[j].contour_index + 1]; /* Go to the next contour. */
                    }
                }
            }
}

static void t1_hinter__add_boundary_length(t1_hinter * self, t1_hint *hint,
                                            int pole_index0, int pole_index1)
{   const t1_pole *pole = &self->pole[pole_index0];
    int contour_index = pole->contour_index;
    int beg_contour_pole = self->contour[contour_index];
    int end_contour_pole = self->contour[contour_index + 1] - 2;
    int i0 = ranger_step_b(pole_index0, beg_contour_pole, end_contour_pole);
    int i1 = ranger_step_f(pole_index1, beg_contour_pole, end_contour_pole);
    t1_glyph_space_coord g = (hint->type == hstem ? pole->gy : pole->gx);

    if (self->pole[i0].type == oncurve)
        i0 = pole_index0;
    if (self->pole[i1].type == oncurve)
        i1 = pole_index1;
    *(any_abs(hint->g0 - g) < any_abs(hint->g1 - g) ? &hint->boundary_length0 : &hint->boundary_length1)
        += (hint->type == hstem ? any_abs(self->pole[i0].gx - self->pole[i1].gx)
                                : any_abs(self->pole[i0].gy - self->pole[i1].gy));
}

static void t1_hinter__align_stem_commands(t1_hinter * self)
{   int i, j, jj, k;

    for(i = 0; i < self->hint_count; i++) {
        self->hint[i].boundary_length0 = self->hint[i].boundary_length1 = 0;
        if (self->hint[i].type == vstem || self->hint[i].type == hstem)
            for (k = self->hint[i].range_index; k != -1; k = self->hint_range[k].next) {
                int beg_range_pole = self->hint_range[k].beg_pole;
                int end_range_pole = self->hint_range[k].end_pole;
                bool horiz = (self->hint[i].type == hstem);
                int quality = max_int;

                if (self->pole[beg_range_pole].type == closepath) {
                    /* A workaround for a buggy font from the Bug 687393,
                       which defines a range with 'closepath' only. */
                    beg_range_pole++;
                    if (beg_range_pole > end_range_pole)
                        continue;
                }
                for (j = beg_range_pole; j <= end_range_pole;) {
                    if (self->pole[j].type == closepath) {
                        j++;
                        continue;
                    }
                    if (t1_hinter__is_stem_hint_applicable(self, &self->hint[i], j, &quality)) {
                        fixed t; /* Type 1 spec implies that it is 0 for curves, 0.5 for bars */
                        int segment_index = t1_hinter__find_stem_middle(self, &t, j, horiz);
                        t1_glyph_space_coord gc;
                        enum t1_align_type align = unaligned;

                        if (self->hint[i].side_mask != 3) {
                            /* An overal hint from the True Type autohinter. */
                            align = (self->hint[i].side_mask & 2 ? topzn : botzn);
                        } else if (self->autohinting && horiz) {
                            if (self->pole[segment_index].gy == self->hint[i].g0)
                                align = (self->hint[i].g0 > self->hint[i].g1 ? topzn : botzn);
                        }
                        align = t1_hinter__compute_aligned_coord(self, &gc,
                                    segment_index, t, &self->hint[i], align);
                        /* todo: optimize: primary commands don't need to align, if suppressed by secondary ones. */
                        t1_hint__set_aligned_coord(&self->hint[i], gc, &self->pole[j], align, quality);
                        jj = j;
                        j = t1_hinter__skip_stem(self, j, horiz);
                        t1_hinter__add_boundary_length(self, &self->hint[i], jj, j);
                        if (j < jj) { /* Rolled over contour end ? */
                            j = self->contour[self->pole[j].contour_index + 1]; /* Go to the next contour. */
                            continue;
                        }
                    }
                    {   /* Step to the next pole in the range : */
                        jj = j;
                        j = t1_hinter__segment_end(self, j);
                        if (j <= jj) /* Rolled over contour end ? */
                            j = self->contour[self->pole[j].contour_index + 1]; /* Go to the next contour. */
                    }
                }
            }
    }
}

static void t1_hinter__unfix_opposite_to_common(t1_hinter * self)
{    /* Implemented for Bug 687578 "T1 hinter disturbs stem width". */
    int i, j, k, m, n;
    t1_glyph_space_coord d, md;
    t1_glyph_space_coord *p_ci, *p_cj, *p_agj, agm;
    enum t1_align_type *p_aj, *p_ai, *p_oi, *p_oj, am;

    for (k = 0; k < 2; k++) { /* g0, g1 */
        /* Since the number of stems in a complex is usually small,
           we don't care about redundant computations. */
        for(i = 0; i < self->hint_count; i++) {
            if (self->hint[i].type == vstem || self->hint[i].type == hstem) {
                p_ai = (!k ? &self->hint[i].aligned0 : &self->hint[i].aligned1);
                p_oi = (!k ? &self->hint[i].aligned1 : &self->hint[i].aligned0);
                if (*p_ai > weak && *p_ai == *p_oi) {
                    p_ci = (!k ? &self->hint[i].g0 : &self->hint[i].g1);
                    md = any_abs(self->hint[i].g1 - self->hint[i].g0);
                    m = i;
                    am = *p_ai;
                    agm = (!k ? self->hint[m].ag0 : self->hint[m].ag1);
                    n = 0;
                    for(j = 0; j < self->hint_count; j++) {
                        if (j != i && self->hint[i].type == self->hint[j].type) {
                            p_cj = (!k ? &self->hint[j].g0 : &self->hint[j].g1);
                            if (*p_ci == *p_cj) {
                                n++;
                                p_aj = (!k ? &self->hint[j].aligned0 : &self->hint[j].aligned1);
                                d = any_abs(self->hint[j].g1 - self->hint[j].g0);
                                if (am < *p_aj) {
                                    md = d;
                                    m = j;
                                    am = *p_aj;
                                    agm = (!k ? self->hint[m].ag0 : self->hint[m].ag1);
                                } if (md < d) {
                                    md = d;
                                }
                            }
                        }
                    }
                    if (n) {
                        for(j = 0; j < self->hint_count; j++) {
                            p_cj = (!k ? &self->hint[j].g0 : &self->hint[j].g1);
                            if (*p_ci == *p_cj) {
                                p_aj = (!k ? &self->hint[j].aligned0 : &self->hint[j].aligned1);
                                p_oj = (!k ? &self->hint[j].aligned1 : &self->hint[j].aligned0);
                                p_agj = (!k ? &self->hint[j].ag0 : &self->hint[j].ag1);
                                *p_aj = am;
                                if (*p_oj == aligned)
                                    *p_oj = weak;
                                *p_agj = agm;
                            }
                        }
                    }
                }
            }
        }
    }
}

static void t1_hinter__compute_opposite_stem_coords(t1_hinter * self)
{   int i;

    for (i = 0; i < self->hint_count; i++)
        if ((self->hint[i].type == vstem || self->hint[i].type == hstem)) {
            t1_glyph_space_coord ag0 = self->hint[i].ag0;
            t1_glyph_space_coord ag1 = self->hint[i].ag1;
            enum t1_align_type aligned0 = self->hint[i].aligned0;
            enum t1_align_type aligned1 = self->hint[i].aligned1;
            t1_glyph_space_coord gw;

            gw = any_abs(self->hint[i].g1 - self->hint[i].g0);
            t1_hinter__align_stem_width(self, &gw, &self->hint[i]);
            if (self->hint[i].g1 - self->hint[i].g0 < 0)
                gw = -gw;
            if (aligned0 > aligned1)
                ag1 = ag0 + gw;
            else if (aligned0 < aligned1)
                ag0 = ag1 - gw;
            else {
                t1_glyph_space_coord d0 = any_abs(ag0 - self->hint[i].g0);
                t1_glyph_space_coord d1 = any_abs(ag1 - self->hint[i].g1);

                if (aligned0 == topzn || aligned1 == topzn)
                    if (gw > 0)
                        ag0 = ag1 - gw;
                    else
                        ag1 = ag0 + gw;
                else if (aligned0 == botzn || aligned1 == botzn)
                    if (gw < 0)
                        ag0 = ag1 - gw;
                    else
                        ag1 = ag0 + gw;
                else if (self->hint[i].type == hstem &&
                        min(any_abs(self->hint[i].g0 - self->ymid), any_abs(self->hint[i].g1 - self->ymid)) >
                        (self->ymax - self->ymin) / 5) {
                    if ((self->hint[i].g1 + self->hint[i].g0) / 2 > self->ymid)
                        ag0 = ag1 - gw;
                    else
                        ag1 = ag0 + gw;
                } else {
                    if (d0 < d1)
                        ag1 = ag0 + gw;
                    else
                        ag0 = ag1 - gw;
                }
            }
            self->hint[i].ag0 = ag0;
            self->hint[i].ag1 = ag1;
        }
}

static int t1_hinter__store_hint_applying(t1_hinter * self, t1_hint *hint, int pole_index)
{
    t1_hint_applying *ha;
    int code = t1_hinter__can_add_hint_applying(self, &ha);

    if (code < 0)
        return code;
    ha->pole = pole_index;
    ha->opposite = -1;
    self->hint_applying_count++;
    return 0;
}

static int t1_hinter__align_stem_poles(t1_hinter * self)
{   int i, j, k;
    t1_glyph_space_coord const fuzz = self->blue_fuzz; /* comparefiles/tpc2.ps */
    int code = 0;

    for (i = 0; i < self->hint_count; i++)
        if (self->hint[i].type == vstem || self->hint[i].type == hstem) {
            t1_hint * hint = &self->hint[i];
            t1_glyph_space_coord ag0 = hint->ag0, ag1 = hint->ag1;
            bool horiz = (hint->type == hstem);

            /* fixme: optimize: Reduce hint_applying with storing only one side of the hint. */
            self->hint_applying_count = 0;
            for (k = self->hint[i].range_index; k != -1; k = self->hint_range[k].next) {
                int beg_range_pole = self->hint_range[k].beg_pole;
                int end_range_pole = self->hint_range[k].end_pole;

                for (j = beg_range_pole; j <= end_range_pole; j++) {
                    t1_pole * pole = &self->pole[j];

                    if (pole->type != oncurve)
                        continue;
                    if (!horiz && any_abs(pole->gx - hint->g0) <= fuzz)
                        code = t1_hinter__store_hint_applying(self, hint, j);
                    else if (!horiz && any_abs(pole->gx - hint->g1) <= fuzz)
                        code = t1_hinter__store_hint_applying(self, hint, j);
                    else if ( horiz && any_abs(pole->gy - hint->g0) <= fuzz)
                        code = t1_hinter__store_hint_applying(self, hint, j);
                    else if ( horiz && any_abs(pole->gy - hint->g1) <= fuzz)
                        code = t1_hinter__store_hint_applying(self, hint, j);
                    if (code < 0)
                        return code;
                }
            }
            for (k = 0; k < self->hint_applying_count; k++) {
                t1_hint_applying *ha0 = &self->hint_applying[k];
                int pole_index0 = ha0->pole;
                t1_pole *pole0 = &self->pole[pole_index0];
                t1_glyph_space_coord g0 = (horiz ? pole0->gy : pole0->gx);
                t1_glyph_space_coord t0 = (horiz ? pole0->gx : pole0->gy);
                bool gt0 = any_abs(hint->g0 - g0) > any_abs(hint->g1 - g0);
                t1_glyph_space_coord d, md = any_abs(hint->g1 - hint->g0) * 5 / 4;
                int mj = -1;

                for (j = 0; j < self->hint_applying_count; j++) {
                    t1_hint_applying *ha1 = &self->hint_applying[j];
                    int pole_index1 = ha1->pole;
                    t1_pole *pole1 = &self->pole[pole_index1];
                    t1_glyph_space_coord g1 = (horiz ? pole1->gy : pole1->gx);
                    t1_glyph_space_coord t1 = (horiz ? pole1->gx : pole1->gy);
                    bool gt1 = any_abs(hint->g0 - g1) > any_abs(hint->g1 - g1);

                    if (gt0 != gt1) {
                        d = any_abs(t1 - t0);
                        if (md > d) {
                            mj = j;
                        }
                    }
                }
                if (mj != -1)    {
                    ha0->opposite = mj;
                    self->hint_applying[mj].opposite = j;
                }
            }
            for (k = 0; k < self->hint_applying_count; k++) {
                t1_hint_applying *ha = &self->hint_applying[k];
                int pole_index = ha->pole;
                t1_pole *pole = &self->pole[pole_index];
                t1_glyph_space_coord g0 = (horiz ? pole->gy : pole->gx);
                bool gt0 = any_abs(hint->g0 - g0) > any_abs(hint->g1 - g0);
                enum t1_align_type align = (!gt0 ? hint->aligned0 : hint->aligned1);
                t1_glyph_space_coord ag = (!gt0 ? ag0 : ag1);
                t1_glyph_space_coord bl = (!gt0 ? hint->boundary_length1 : hint->boundary_length0); /* opposite */

                if (ha->opposite == -1)
                    align = weak;
                if (!horiz) {
                    if (pole->aligned_x < align)
                        pole->ax = ag, pole->aligned_x = align, pole->boundary_length_x = bl;
                } else {
                    if (pole->aligned_y < align)
                        pole->ay = ag, pole->aligned_y = align, pole->boundary_length_y = bl;
                }
            }
        }
    return 0;
}

static t1_hint * t1_hinter__find_vstem_by_center(t1_hinter * self, t1_glyph_space_coord gx)
{   /* Find vstem with axis near gx : */
    int i;
    t1_hint * hint = NULL;
    t1_glyph_space_coord dx = fixed_1;

    for (i = 0; i < self->hint_count; i++)
        if (self->hint[i].type == vstem) {
            t1_glyph_space_coord d = any_abs(gx - (self->hint[i].ag0 + self->hint[i].ag1) / 2);

            if (dx > d) {
                dx = d;
                hint = &self->hint[i];
            }
        }
    return hint;
}

static void t1_hinter__process_dotsection(t1_hinter * self, int beg_pole, int end_pole)
{   /*  Since source outline must have oncurve poles at XY extremes,
        we compute bounding box from poles.
    */
    int i;
    t1_glyph_space_coord min_gx = self->pole[beg_pole].gx, min_gy = self->pole[beg_pole].gy;
    t1_glyph_space_coord max_gx = min_gx, max_gy = min_gy;
    t1_glyph_space_coord center_gx, center_gy, center_agx, center_agy;
    t1_glyph_space_coord sx, sy;
    bool aligned_min_x = false, aligned_min_y = false, aligned_max_x = false, aligned_max_y = false;
    bool aligned_x, aligned_y;

    for (i = beg_pole + 1; i <= end_pole; i++) {
        t1_glyph_space_coord gx = self->pole[i].gx, gy = self->pole[i].gy;

        min_gx = min(min_gx, gx);
        min_gy = min(min_gy, gy);
        max_gx = max(max_gx, gx);
        max_gy = max(max_gy, gy);
    }
    for (i = beg_pole; i <= end_pole; i++) {
        if (self->pole[i].gx == min_gx)
            aligned_min_x |= self->pole[i].aligned_x;
        if (self->pole[i].gy == min_gy)
            aligned_min_y |= self->pole[i].aligned_y;
        if (self->pole[i].gx == max_gx)
            aligned_max_x |= self->pole[i].aligned_x;
        if (self->pole[i].gy == max_gy)
            aligned_max_y |= self->pole[i].aligned_y;
    }
    aligned_x = aligned_min_x && aligned_max_x;
    aligned_y = aligned_min_y && aligned_max_y;
    if (aligned_x && aligned_y)
        return; /* The contour was aligned with stem commands - nothing to do. */
    center_gx = center_agx = (min_gx + max_gx) / 2;
    center_gy = center_agy = (min_gy + max_gy) / 2;
    if (!aligned_x) {
        /* Heuristic : apply vstem if it is close to the center : */
        t1_hint * hint = t1_hinter__find_vstem_by_center(self, center_gx);
        if (hint != NULL) {
            center_agx = (hint->ag0 + hint->ag1) / 2; /* Align with vstem */
            aligned_x = true;
        }
    }
    t1_hinter__align_to_grid(self, self->g2o_fraction / 2, &center_agx, &center_agy,
                                CONTRAST_STEMS || self->align_to_pixels);
    sx = center_agx - center_gx;
    sy = center_agy - center_gy;
    if (aligned_x)
        sx = 0;
    if (aligned_y)
        sy = 0;
    /* Shift the contour (sets alignment flags to prevent interpolation) : */
    for (i = beg_pole; i <= end_pole; i++) {
        self->pole[i].ax = self->pole[i].gx + sx;
        self->pole[i].ay = self->pole[i].gy + sy;
        self->pole[i].aligned_x |= !aligned_x; /* Prevent interpolation if we aligned it here. */
        self->pole[i].aligned_y |= !aligned_y;
    }
}

static void t1_hinter__process_dotsections(t1_hinter * self)
{   int i;

    for(i = 0; i < self->hint_count; i++)
        if (self->hint[i].type == dot) {
            int pole_index = self->hint_range[self->hint[i].range_index].beg_pole;
            int contour_index = self->pole[pole_index].contour_index;
            int beg_pole = self->contour[contour_index];
            int end_pole = self->contour[contour_index + 1] - 2;

            t1_hinter__process_dotsection(self, beg_pole, end_pole);
        }
}

static void t1_hinter__interpolate_other_poles(t1_hinter * self)
{   int i, j, k;

    for (k = 0; k<2; k++) { /* X, Y */
        t1_glyph_space_coord *p_gc = (!k ? &self->pole[0].gx : &self->pole[0].gy);
        t1_glyph_space_coord *p_wc = (!k ? &self->pole[0].gy : &self->pole[0].gx);
        t1_glyph_space_coord *p_ac = (!k ? &self->pole[0].ax : &self->pole[0].ay);
        t1_glyph_space_coord *p_bl = (!k ? &self->pole[0].boundary_length_x : &self->pole[0].boundary_length_y);
        enum t1_align_type *p_f = (!k ? &self->pole[0].aligned_x : &self->pole[0].aligned_y);
        int offset_gc = (char *)p_gc - (char *)&self->pole[0];
        int offset_wc = (char *)p_wc - (char *)&self->pole[0];
        int offset_ac = (char *)p_ac - (char *)&self->pole[0];
        int offset_bl = (char *)p_bl - (char *)&self->pole[0];
        int offset_f  = (char *)p_f -  (char *)&self->pole[0];

        for (i = 0; i < self->contour_count; i++) {
            int beg_contour_pole = self->contour[i];
            int end_contour_pole = self->contour[i + 1] - 2;
            int range_beg;

            for (j = beg_contour_pole; j <= end_contour_pole; j++)
                if (*member_prt(enum t1_align_type, &self->pole[j], offset_f))
                    break;
            if (j > end_contour_pole)
                continue;
            range_beg = j;
            do {
                int start_pole = j, stop_pole = -1;
                t1_glyph_space_coord min_a, max_a;
                t1_glyph_space_coord min_g, max_g, g0, g1, a0, a1;
                int min_i = start_pole, max_i = start_pole, cut_l, l;
                bool moved = false;

                do {
                    int min_l = 0, max_l = 0;
                    int min_w, max_w, w0;

                    g0 = *member_prt(t1_glyph_space_coord, &self->pole[start_pole], offset_gc);
                    w0 = *member_prt(t1_glyph_space_coord, &self->pole[start_pole], offset_wc);
                    a0 = *member_prt(t1_glyph_space_coord, &self->pole[start_pole], offset_ac);
                    min_g = g0;
                    max_g = g0;
                    min_w = max_w = w0;
                    for (j = ranger_step_f(start_pole,  beg_contour_pole, end_contour_pole), l = 1;
                         j != start_pole;
                         j = ranger_step_f(j,  beg_contour_pole, end_contour_pole), l++) {
                        t1_glyph_space_coord g = * member_prt(t1_glyph_space_coord, &self->pole[j], offset_gc);
                        t1_glyph_space_coord w = * member_prt(t1_glyph_space_coord, &self->pole[j], offset_wc);

                        if (min_g > g)
                            min_g = g, min_i = j, min_l = l;
                        if (max_g < g)
                            max_g = g, max_i = j, max_l = l;
                        if (min_w > w)
                            min_w = w;
                        if (max_w < w)
                            max_w = w;
                        if (*member_prt(enum t1_align_type, &self->pole[j], offset_f))
                            break;
                        if (j == stop_pole)
                            break;
                    }
                    stop_pole = j;
                    cut_l = l;
                    g1 = * member_prt(t1_glyph_space_coord, &self->pole[stop_pole], offset_gc);
                    a1 = * member_prt(t1_glyph_space_coord, &self->pole[stop_pole], offset_ac);

                    if (start_pole != stop_pole)
                        if (any_abs(g0 - g1) >= any_abs(a0 - a1) / 10)
                            if (any_abs(max_g - min_g) <= any_abs(max_w - min_w) / 4)
                                break; /* OK to interpolate. */
                    /* else break at an extremal pole : */
                    if (min_i != start_pole && min_l < cut_l && min_g != g0 && min_g != g1)
                        stop_pole = min_i, cut_l = min_l;
                    if (max_i != start_pole && max_l < cut_l && max_g != g0 && max_g != g1)
                        stop_pole = max_i, cut_l = max_l;
                } while (cut_l < l);
                    /* Now start_pole and end_pole point to the contour interval to interpolate. */
                if (g0 < g1) {
                    min_g = g0;
                    max_g = g1;
                    min_a = a0;
                    max_a = a1;
                } else {
                    min_g = g1;
                    max_g = g0;
                    min_a = a1;
                    max_a = a0;
                }
                if (min_g == max_g && min_a != max_a) {
                    /* Alignment conflict, choose by boundary_length. */
                    if (* member_prt(t1_glyph_space_coord, &self->pole[start_pole], offset_bl) <
                        * member_prt(t1_glyph_space_coord, &self->pole[stop_pole], offset_bl))
                        min_a = max_a = a1;
                    else
                        min_a = max_a = a0;
                }
                for (j = start_pole; ;
                     j = ranger_step_f(j,  beg_contour_pole, end_contour_pole)) {
                    t1_glyph_space_coord g = * member_prt(t1_glyph_space_coord, &self->pole[j], offset_gc);

                    if (g <= min_g)
                        * member_prt(t1_glyph_space_coord, &self->pole[j], offset_ac) =
                        * member_prt(t1_glyph_space_coord, &self->pole[j], offset_gc) + (min_a - min_g);
                    else if (g >= max_g)
                        * member_prt(t1_glyph_space_coord, &self->pole[j], offset_ac) =
                        * member_prt(t1_glyph_space_coord, &self->pole[j], offset_gc) + (max_a - max_g);
                    if(moved && j == stop_pole)
                        break;
                    moved = true;
                }
                if (min_g < max_g) {
                    int24 div = max_g - min_g;
                    int24 mul = max_a - min_a;
                    /*  Due to glyph coordinate definition, div is not smaller than 2^12.

                        In the following cycle we need to compute x*mul/div for 24-bit integers,
                        We replace this expression with x*u/2^12 where u = mul*2^12/div
                        (note that it's an approximation with relative precision 2^-12).

                        If mul or div are big, we drop 5 bits to fit them into int19.
                        Note that it's another approximation with relative precision 2^-14.
                        Let now they are m0 and d.

                        Then we compute :

                        q1 = m0 / d, r1 = m0 % d, m1 = r1 << 12;   // r1 < 2^19, m0 < 2^12
                        q2 = m1 / d, r2 = m1 % d, m2 = r2 << 12;   // r2 < 2^19, m1 < 2^12
                        q3 = m2 / d, r3 = m2 % d, m3 = r3 << 12;   // r3 < 2^19, m2 < 2^12
                        and so on.

                        We have :

                        u = ((q1 + (q2 >> 12) + (q3 >> 24) + ...) << 12
                          = (q1 << 12) + q2 + (q3 >> 12) + ...
                          = (q1 << 12) + q2 .

                        Thus we got pretty nice formula without iterations. Implementing it below.
                    */
                    int24 m0 = mul, d = div, q1, q2, r1, m1, u;

                    if (m0 >= (1 << 19) || d >= (1 << 19))
                        m0 >>= 5, d >>= 5;
                    q1 = m0 / d, r1 = m0 % d, m1 = r1 << 12;
                    q2 = m1 / d;
                    u = (q1 << 12) + q2;
                    for (j = ranger_step_f(start_pole,  beg_contour_pole, end_contour_pole); j != stop_pole;
                         j = ranger_step_f(j,  beg_contour_pole, end_contour_pole)) {
                        t1_glyph_space_coord g = *member_prt(t1_glyph_space_coord, &self->pole[j], offset_gc);

                        if (min_g < g && g < max_g) {
                            t1_glyph_space_coord *a = member_prt(t1_glyph_space_coord, &self->pole[j], offset_ac);
                            t1_glyph_space_coord x = g - min_g;
                            t1_glyph_space_coord h = mul_shift(x, u, 12); /* It is x*u/2^12 */

                            /* h = (int24)(x * (double)mul / div + 0.5); Uncomment this to disable our tricks. */
                            *a = min_a + h;
                        }
                    }
                }
                j = stop_pole;
            } while (j != range_beg);
        }
    }
}

static int t1_hinter__export(t1_hinter * self)
{   int i, j, code;
    fixed fx, fy;

    for(i = 0; ; i++) {
        int end_pole, beg_pole = self->contour[i];
        t1_pole *pole = & self->pole[beg_pole];

        g2d(self, pole->ax, pole->ay, &fx, &fy);
        code = gx_path_add_point(self->output_path, fx, fy);
        if (code < 0)
            return code;
        if (i >= self->contour_count)
            break;
        end_pole = self->contour[i + 1] - 2;
        for(j = beg_pole + 1; j <= end_pole; j++) {
            pole = & self->pole[j];
            g2d(self, pole->ax, pole->ay, &fx, &fy);
            if (pole->type == oncurve) {
                code = gx_path_add_line(self->output_path, fx, fy);
                if (code < 0)
                    return code;
            } else {
                int j1 = j + 1, j2 = (j + 2 > end_pole ? beg_pole : j + 2);
                fixed fx1, fy1, fx2, fy2;

                g2d(self, self->pole[j1].ax, self->pole[j1].ay, &fx1, &fy1);
                g2d(self, self->pole[j2].ax, self->pole[j2].ay, &fx2, &fy2);
                code = gx_path_add_curve(self->output_path, fx, fy, fx1, fy1, fx2, fy2);
                if (code < 0)
                    return code;
                j+=2;
            }
        }
        code = gx_path_close_subpath(self->output_path);
        if (code < 0)
            return code;
    }
    return 0;
}

static int t1_hinter__add_trailing_moveto(t1_hinter * self)
{   t1_glyph_space_coord gx = self->width_gx, gy = self->width_gy;

#if 0 /* self appears wrong due to several reasons :
         1. With TextAlphaBits=1, AlignToPixels must have no effect.
         2. ashow, awidthshow must add the width before alignment.
         4. In the PDF interpreter, Tc must add before alignment.
         5. Since a character origin is aligned,
            rounding its width doesn't affect subsequent characters.
         6. When the character size is smaller than half pixel width,
            glyph widths rounds to zero, causing overlapped glyphs.
            (Bug 687719 "PDFWRITE corrupts letter spacing/placement").
       */
    if (self->align_to_pixels)
        t1_hinter__align_to_grid(self, self->g2o_fraction, &gx, &gy, self->align_to_pixels);
#endif
    return t1_hinter__rmoveto(self, gx - self->cx, gy - self->cy);
}

int t1_hinter__endglyph(t1_hinter * self)
{   int code = 0;

    code = t1_hinter__add_trailing_moveto(self);
    if (code < 0)
        goto exit;
    code = t1_hinter__end_subglyph(self);
    if (code < 0)
        goto exit;
    t1_hinter__adjust_matrix_precision(self, self->orig_gx, self->orig_gy);
    t1_hinter__compute_y_span(self);
    t1_hinter__simplify_representation(self);
    if (!self->disable_hinting && (self->grid_fit_x || self->grid_fit_y)) {
        if (self->FontType == 1)
            t1_hinter__compute_type1_stem_ranges(self);
        else
            t1_hinter__compute_type2_stem_ranges(self);
        t1_hinter__mark_existing_stems(self);
        t1_hinter_compute_stem_snap_range(self);
        t1_hinter__align_stem_commands(self);
        t1_hinter__unfix_opposite_to_common(self);
        t1_hinter__compute_opposite_stem_coords(self);
        /* stem3 was processed in the Type 1 interpreter. */
        code = t1_hinter__align_stem_poles(self);
        if (code < 0)
            goto exit;
        t1_hinter__process_dotsections(self);
        t1_hinter__interpolate_other_poles(self);
    }
    if (self->pole_count) {
        if (self->fix_contour_sign) {
            t1_hinter__fix_contour_signs(self);
        }
        code = t1_hinter__export(self);
    }
exit:
    t1_hinter__free_arrays(self);
    return code;
}
