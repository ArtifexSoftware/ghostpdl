/* Copyright (C) 1990, 2000 Aladdin Enterprises.  All rights reserved.
  
  This software is provided AS-IS with no warranty, either express or
  implied.
  
  This software is distributed under license and may not be copied,
  modified or distributed except as expressly authorized under the terms
  of the license contained in the file LICENSE in this distribution.
  
  For more information about licensing, please refer to
  http://www.ghostscript.com/licensing/. For information on
  commercial licensing, go to http://www.artifex.com/licensing/ or
  contact Artifex Software, Inc., 101 Lucas Valley Road #110,
  San Rafael, CA  94903, U.S.A., +1(415)492-9861.
*/

/* $Id$ */
/* Type 1 hinter, a new algorithm, prototypes */

#ifndef gxhintn_INCLUDED
#  define gxhintn_INCLUDED

/* fixme : In this version we statically define sizes of all internal arrays.
   It would be nice to implement dynamical re-allocation on overflow.
*/

#define T1_MAX_STEM_SNAPS 12
#define T1_MAX_ALIGNMENT_ZONES 25
#define T1_MAX_CONTOURS 50
#define T1_MAX_POLES (500 + T1_MAX_CONTOURS)
#define T1_MAX_HINTS 200 /* total number of hints in a glyph, not a maximal one at a moiment */

typedef int int32;
typedef fixed t1_glyph_space_coord; /* measured in original glyph space units */
typedef int32 t1_hinter_space_coord; /* measured in temporary outliner's space units */
typedef int32 int19;

enum t1_hint_type
{   hstem, vstem, dot, replace
};

enum t1_pole_type
{   offcurve, oncurve, closepath, moveto
};

enum t1_zone_type
{   topzone, botzone
};

typedef struct {
    double xx, xy, yx, yy;
} double_matrix;

typedef struct {
    int19 xx, xy, yx, yy;
    int32 denominator;
    unsigned int bitshift;
} fraction_matrix;

typedef struct t1_pole_s /* a pole of outline */
{   t1_glyph_space_coord gx,gy; /* source unaligned coords */
    t1_glyph_space_coord ax,ay; /* aligned coords */
    t1_hinter_space_coord ox,oy;
    enum t1_pole_type type;
    int contour_index;
    bool aligned_x, aligned_y;
} t1_pole;

typedef struct t1_hint_s
{   enum t1_hint_type type;
    t1_glyph_space_coord g0, g1; /* starting and ending transversal coord of the stem */
    t1_glyph_space_coord ag0, ag1; /* starting and ending transversal aligned coord of the stem */
    bool aligned0, aligned1; /* ag0, ag1 is aligned */
    short start_pole; /* outline pole to search for stem the stem from */
    short beg_pole, end_pole; /* outline interval of the stem */
    unsigned int stem3_index; /* 1,2,3 for stem3, 0 for other types */
    short complex_link; /* the index of next member of same complex */
    int contour_index;
    bool completed;
} t1_hint;

typedef struct t1_zone_s /* alignment zone */
{   enum t1_zone_type type;
    t1_glyph_space_coord y, overshoot_y;
    t1_glyph_space_coord y_min, y_max;
} t1_zone;

typedef struct t1_hinter_s
{   long italic_x, italic_y; /* tan(ItalicAngle) */
    fraction_matrix ctmf;
    fraction_matrix ctmi;
    unsigned int g2o_fraction_bits;
    unsigned int import_shift;
    int32 g2o_fraction;
    t1_glyph_space_coord orig_gx, orig_gy; /* glyph origin in glyph space */
    t1_glyph_space_coord subglyph_orig_gx, subglyph_orig_gy; /* glyph origin in glyph space */
    fixed orig_dx, orig_dy; /* glyph origin in device space */
    t1_glyph_space_coord width_gx, width_gy; /* glyph space coords of the glyph origin */
    t1_glyph_space_coord cx, cy; /* current point */
    double BlueScale;
    t1_glyph_space_coord blue_shift, blue_fuzz;
    t1_pole pole[T1_MAX_POLES];
    t1_hint hint[T1_MAX_HINTS];
    t1_glyph_space_coord stem_snap[2][T1_MAX_STEM_SNAPS + 1]; /* StdWH + StemSnapH, StdWV + StemSnapV */
    t1_zone zone[T1_MAX_ALIGNMENT_ZONES];
    int contour[T1_MAX_CONTOURS];
    int contour_count;
    int stem_snap_count[2]; /* H, V */
    int zone_count;
    int pole_count;
    int hint_count;
    int primary_hint_count;
    bool ForceBold;
    bool seac_flag;
    bool keep_stem_width;
    bool suppress_overshoots;
    bool disable_hinting;
    bool charpath_flag;
    double font_size;
    double resolution;
    double heigt_transform_coef;
    double width_transform_coef;
    double base_font_scale;
    int19 blue_rounding;
    int19 width_transform_coef_rat;
    int19 heigt_transform_coef_rat;
    int19 width_transform_coef_inv;
    int19 heigt_transform_coef_inv;
    t1_glyph_space_coord overshoot_threshold;
} t1_hinter;

void t1_hinter__reset(t1_hinter * this);
void t1_hinter__reset_outline(t1_hinter * this);
int  t1_hinter__set_transform(t1_hinter * this, gs_matrix_fixed * ctm, gs_rect * FontBBox, gs_matrix * FontMatrix, gs_matrix * baseFontMatrix);
int  t1_hinter__set_alignment_zones(t1_hinter * this, float * blues, int count, enum t1_zone_type type, bool family);
int  t1_hinter__set_stem_snap(t1_hinter * this, float * value, int count, unsigned short hv);
void t1_hinter__set_blue_values(t1_hinter * this, double BlueScale, double BlueShift, double BlueFuzz);
void t1_hinter__set_bold(t1_hinter * this, bool ForceBold);
void t1_hinter__set_origin(t1_hinter * this, fixed dx, fixed dy);
void t1_hinter__set_italic(t1_hinter * this, double ItalicAngle);
void t1_hinter__set_charpath_flag(t1_hinter * this, bool charpath);

int  t1_hinter__sbw(t1_hinter * this, t1_glyph_space_coord sbx, t1_glyph_space_coord sby
                                    , t1_glyph_space_coord wx,  t1_glyph_space_coord wy);
int  t1_hinter__sbw_seac(t1_hinter * this, t1_glyph_space_coord sbx, t1_glyph_space_coord sby);
int  t1_hinter__rmoveto(t1_hinter * this, t1_glyph_space_coord xx, t1_glyph_space_coord yy);
int  t1_hinter__rlineto(t1_hinter *, t1_glyph_space_coord xx, t1_glyph_space_coord yy);
int  t1_hinter__rcurveto(t1_hinter * this, t1_glyph_space_coord xx0, t1_glyph_space_coord yy0
                                         , t1_glyph_space_coord xx1, t1_glyph_space_coord yy1
                                         , t1_glyph_space_coord xx2, t1_glyph_space_coord yy2);
void t1_hinter__setcurrentpoint(t1_hinter * this, t1_glyph_space_coord xx, t1_glyph_space_coord yy);
int  t1_hinter__closepath(t1_hinter * this);

int  t1_hinter__drop_hints(t1_hinter * this);
int  t1_hinter__dotsection(t1_hinter * this);
int  t1_hinter__hstem(t1_hinter * this, t1_glyph_space_coord x0, t1_glyph_space_coord x1);
int  t1_hinter__vstem(t1_hinter * this, t1_glyph_space_coord y0, t1_glyph_space_coord y1);
int  t1_hinter__hstem3(t1_hinter * this, t1_glyph_space_coord x0, t1_glyph_space_coord y1
                                       , t1_glyph_space_coord x2, t1_glyph_space_coord y3
                                       , t1_glyph_space_coord x4, t1_glyph_space_coord y5);
int  t1_hinter__vstem3(t1_hinter * this, t1_glyph_space_coord y0, t1_glyph_space_coord y1
                                       , t1_glyph_space_coord y2, t1_glyph_space_coord y3
                                       , t1_glyph_space_coord y4, t1_glyph_space_coord y5);

void t1_hinter__round_to_pixels(t1_hinter * this, t1_glyph_space_coord * gx, t1_glyph_space_coord * gy);
int  t1_hinter__endchar(t1_hinter * this, bool seac_flag);
int  t1_hinter__endglyph(t1_hinter * this, gs_op1_state * s);

#endif /* gxhintn_INCLUDED */
