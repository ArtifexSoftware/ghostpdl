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

/* pcbiptrn.c - code for PCL built-in patterns */

#include "pcpatrn.h"
#include "pcbiptrn.h"

/*
 * Macro for convenient creation of static patterns.
 *
 * Note that the built-in patterns cannot be const because their render
 * information may be changed. To support environments in which all
 * initialized globals are in ROM, two copies of each built-in pattern
 * are created: a prototype (qualified as const), and the actual copy.
 */
#define make_static_pattern(name, name_proto, data)                     \
    private const pcl_pattern_t name_proto = {                          \
        { (byte *)data, 2, {16, 16}, 0, 1, 1 }, /* pixmap information */\
        pcds_permanent,                         /* storage - ignored */ \
        pcl_pattern_uncolored,                  /* type */              \
        300, 300,                               /* resolution */        \
        -1,                                     /* orient */            \
        -1,                                     /* pen number */        \
        { 0.0, 0.0 },                           /* reference point */   \
        0UL,                                    /* cache_id */          \
        { 0, 0 },                               /* palette */           \
        0,                                      /* pcspace */           \
        0,                                      /* prast */             \
        0UL,                                    /* ccolor_id */         \
        { { { 0.0, 0.0, 0.0, 0.0 } }, 0 }       /* ccolor structure */  \
    };                                                                  \
    private pcl_pattern_t   name

/*
 * PCL shade patterns.
 */
private const byte  shade_01_02_data[2 * 16] = {
    0x80, 0x80,   0x00, 0x00,   0x00, 0x00,   0x00, 0x00,
    0x00, 0x00,   0x00, 0x00,   0x00, 0x00,   0x00, 0x00,
    0x08, 0x08,   0x00, 0x00,   0x00, 0x00,   0x00, 0x00,
    0x00, 0x00,   0x00, 0x00,   0x00, 0x00,   0x00, 0x00
};
make_static_pattern(shade_01_02, shade_01_02_proto, shade_01_02_data);

private const byte  shade_03_10_data[2 * 16] = {
    0x80, 0x80,   0x00, 0x00,   0x00, 0x00,   0x00, 0x00,
    0x08, 0x08,   0x00, 0x00,   0x00, 0x00,   0x00, 0x00,
    0x80, 0x80,   0x00, 0x00,   0x00, 0x00,   0x00, 0x00,
    0x08, 0x08,   0x00, 0x00,   0x00, 0x00,   0x00, 0x00
};
make_static_pattern(shade_03_10, shade_03_10_proto, shade_03_10_data);

private const byte  shade_11_20_data[2 * 16] = {
    0xc0, 0xc0,   0xc0, 0xc0,   0x00, 0x00,   0x00, 0x00,
    0x00, 0x00,   0x00, 0x00,   0xc0, 0xc0,   0xc0, 0xc0,
    0x00, 0x00,   0x00, 0x00,   0x00, 0x00,   0x00, 0x00,
    0x00, 0x00,   0x00, 0x00,   0x00, 0x00,   0x00, 0x00
};
make_static_pattern(shade_11_20, shade_11_20_proto, shade_11_20_data);

private const byte  shade_21_35_data[2 * 16] = {
    0xc1, 0xc1,   0xc1, 0xc1,   0x80, 0x80,   0x08, 0x08,
    0x1c, 0x1c,   0x1c, 0x1c,   0x08, 0x08,   0x80, 0x80,
    0xc1, 0xc1,   0xc1, 0xc1,   0x80, 0x80,   0x08, 0x08,
    0x1c, 0x1c,   0x1c, 0x1c,   0x08, 0x08,   0x80, 0x80
};
make_static_pattern(shade_21_35, shade_21_35_proto, shade_21_35_data);

private const byte  shade_36_55_data[2 * 16] = {
    0xc1, 0xc1,   0xeb, 0xeb,   0xc1, 0xc1,   0x88, 0x88,
    0x1c, 0x1c,   0xbe, 0xbe,   0x1c, 0x1c,   0x88, 0x88,
    0xc1, 0xc1,   0xeb, 0xeb,   0xc1, 0xc1,   0x88, 0x88,
    0x1c, 0x1c,   0xbe, 0xbe,   0x1c, 0x1c,   0x88, 0x88
};
make_static_pattern(shade_36_55, shade_36_55_proto, shade_36_55_data);

private const byte  shade_56_80_data[2 * 16] = {
    0xe3, 0xe3,   0xe3, 0xe3,   0xe3, 0xe3,   0xdd, 0xdd,
    0x3e, 0x3e,   0x3e, 0x3e,   0x3e, 0x3e,   0xdd, 0xdd,
    0xe3, 0xe3,   0xe3, 0xe3,   0xe3, 0xe3,   0xdd, 0xdd,
    0x3e, 0x3e,   0x3e, 0x3e,   0x3e, 0x3e,   0xdd, 0xdd
};
make_static_pattern(shade_56_80, shade_56_80_proto, shade_56_80_data);

private const byte  shade_81_99_data[2 * 16] = {
    0xf7, 0xf7,   0xe3, 0xe3,   0xf7, 0xf7,   0xff, 0xff,
    0x7f, 0x7f,   0x3e, 0x3e,   0x7f, 0x7f,   0xff, 0xff,
    0xf7, 0xf7,   0xe3, 0xe3,   0xf7, 0xf7,   0xff, 0xff,
    0x7f, 0x7f,   0x3e, 0x3e,   0x7f, 0x7f,   0xff, 0xff
};
make_static_pattern(shade_81_99, shade_81_99_proto, shade_81_99_data);

/*
 * Data for cross-hatch patterns
 */
private const byte  cross_1_data[2 * 16] = {
    0x00, 0x00,   0x00, 0x00,   0x00, 0x00,   0x00, 0x00,
    0x00, 0x00,   0x00, 0x00,   0x00, 0x00,   0xff, 0xff,
    0xff, 0xff,   0x00, 0x00,   0x00, 0x00,   0x00, 0x00,
    0x00, 0x00,   0x00, 0x00,   0x00, 0x00,   0x00, 0x00
};
make_static_pattern(cross_1, cross_1_proto, cross_1_data);

private const byte cross_2_data[2 * 16] = {
    0x01, 0x80,   0x01, 0x80,   0x01, 0x80,   0x01, 0x80,
    0x01, 0x80,   0x01, 0x80,   0x01, 0x80,   0x01, 0x80,
    0x01, 0x80,   0x01, 0x80,   0x01, 0x80,   0x01, 0x80,
    0x01, 0x80,   0x01, 0x80,   0x01, 0x80,   0x01, 0x80
};
make_static_pattern(cross_2, cross_2_proto, cross_2_data);

private const byte cross_3_data[2 * 16] = {
    0x80, 0x03,   0x00, 0x07,   0x00, 0x0e,   0x00, 0x1c,
    0x00, 0x38,   0x00, 0x70,   0x00, 0xe0,   0x01, 0xc0,
    0x03, 0x80,   0x07, 0x00,   0x0e, 0x00,   0x1c, 0x00,
    0x38, 0x00,   0x70, 0x00,   0xe0, 0x00,   0xc0, 0x01
};
make_static_pattern(cross_3, cross_3_proto, cross_3_data);

private const byte cross_4_data[2 * 16] = {
    0xc0, 0x01,   0xe0, 0x00,   0x70, 0x00,   0x38, 0x00,
    0x1c, 0x00,   0x0e, 0x00,   0x07, 0x00,   0x03, 0x80,
    0x01, 0xc0,   0x00, 0xe0,   0x00, 0x70,   0x00, 0x38,
    0x00, 0x1c,   0x00, 0x0e,   0x00, 0x07,   0x80, 0x03
};
make_static_pattern(cross_4, cross_4_proto, cross_4_data);

private const byte cross_5_data[2 * 16] = {
    0x01, 0x80,   0x01, 0x80,   0x01, 0x80,   0x01, 0x80,
    0x01, 0x80,   0x01, 0x80,   0x01, 0x80,   0xff, 0xff,
    0xff, 0xff,   0x01, 0x80,   0x01, 0x80,   0x01, 0x80,
    0x01, 0x80,   0x01, 0x80,   0x01, 0x80,   0x01, 0x80
};
make_static_pattern(cross_5, cross_5_proto, cross_5_data);

private const byte cross_6_data[2 * 16] = {
    0xc0, 0x03,   0xe0, 0x07,   0x70, 0x0e,   0x38, 0x1c,
    0x1c, 0x38,   0x0e, 0x70,   0x07, 0xe0,   0x03, 0xc0,
    0x03, 0xc0,   0x07, 0xe0,   0x0e, 0x70,   0x1c, 0x38,
    0x38, 0x1c,   0x70, 0x0e,   0xe0, 0x07,   0xc0, 0x03
};
make_static_pattern(cross_6, cross_6_proto, cross_6_data);


private const pcl_pattern_t *const  bi_pattern_proto_array[] = {
    &shade_01_02_proto, &shade_03_10_proto,
    &shade_11_20_proto, &shade_21_35_proto,
    &shade_36_55_proto, &shade_56_80_proto,
    &shade_81_99_proto,
    &cross_1_proto,     &cross_2_proto,
    &cross_3_proto,     &cross_4_proto,
    &cross_5_proto,     &cross_6_proto
};

private pcl_pattern_t *const    bi_pattern_array[] = {
    &shade_01_02, &shade_03_10, &shade_11_20, &shade_21_35,
    &shade_36_55, &shade_56_80, &shade_81_99,
    &cross_1,     &cross_2,     &cross_3,     &cross_4,
    &cross_5,     &cross_6
};

#define bi_cross_array  (bi_pattern_array + 7)

/*
 * A special array, used for rendering images that interact with solid
 * foregrounds.
 *
 * Handling the interaction of rasters and foregrounds in PCL is tricky. PCL
 * foregounds carry a full set of rendering information, including color
 * correction and halftoning information. These may differe from the color
 * correction information and halftone mechanism used to process the raster
 * itself (which is always taken from the current palette at the time the
 * raster is output). The graphic library can accommodate only a single
 * color rendering dictionary and halftone mechanism at one time, hence the
 * problem.
 *
 * The solution is to invoke a second graphic state. Patterns in the graphic
 * library are provided with their own graphic state, so method for handling
 * solid foreground colors is to create a solid, uncolored pattern that is
 * is "on" (assumes the foreground color) everywhere.
 *
 * The following 1x1 pixel texture is used for this prupose. As with the
 * pattern above, two copies of this structure are required: a prototype
 * (qualified as const) and the pattern actually used.
 */
private const byte solid_pattern_data = 0xff;

private const pcl_pattern_t solid_pattern_proto = {
    { (byte *)&solid_pattern_data, 1, {1, 1}, 0, 1, 1 },
    pcds_permanent,                         /* storage - ignored */
    pcl_pattern_uncolored,                  /* type */
    300, 300,                               /* resolution - NA */
    -1,                                     /* orient */
    -1,                                     /* pen number */
    { 0.0, 0.0 },                           /* reference point */
    0UL,                                    /* cache_id */
    { 0, 0 },                               /* palette */
    0,                                      /* pcspace */
    0,                                      /* prast */
    0UL,                                    /* ccolor_id */
    { { { 0.0, 0.0, 0.0, 0.0 } }, 0 }       /* ccolor structure */
};

private pcl_pattern_t   solid_pattern;


/*
 * Initialize the built-in patterns
 */
  void
pcl_pattern_init_bi_patterns(void)
{
    int     i;

    for (i = 0; i < countof(bi_pattern_proto_array); i++)
        *(bi_pattern_array[i]) = *(bi_pattern_proto_array[i]);
    solid_pattern = solid_pattern_proto;
}

/*
 * For a given intensity value, return the corresponding shade pattern. A
 * null return indicates that a solid pattern should be used - the caller
 * must look at the intensity to determine if it is black or white.
 */
  pcl_pattern_t *
pcl_pattern_get_shade(
    int                     inten
)
{
    static int              last_inten;
    static pcl_pattern_t *  plast_shade;

    if (inten != last_inten) {
        last_inten = inten;
        if (inten <= 0)
            plast_shade = 0;
        else if (inten <= 2)
            plast_shade = &shade_01_02;
        else if (inten <= 10)
            plast_shade = &shade_03_10;
        else if (inten <= 20)
            plast_shade = &shade_11_20;
        else if (inten <= 35)
            plast_shade = &shade_21_35;
        else if (inten <= 55)
            plast_shade = &shade_36_55;
        else if (inten <= 80)
            plast_shade = &shade_56_80;
        else if (inten <= 99)
            plast_shade = &shade_81_99;
        else 
            plast_shade = 0;
    }
    return plast_shade;
}

/*
 * For a given index value, return the corresponding cross-hatch pattern. A
 * null return indicates that the pattern is out of range. The caller must
 * determine what to do in this case.
 */
  pcl_pattern_t *
pcl_pattern_get_cross(
    int     indx
)
{
    if ((indx < 1) || (indx > 6))
        return 0;
    else
        return bi_cross_array[indx - 1];
}

/*
 * Return the solid uncolored patterns, to be used with rasters (see above).
 */
  pcl_pattern_t *
pcl_pattern_get_solid_pattern(void)
{
    return &solid_pattern;
}
