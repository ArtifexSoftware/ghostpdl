/* Copyright (C) 1996, 1997, 1998 Aladdin Enterprises.  All rights
   reserved.  Unauthorized use, copying, and/or distribution
   prohibited.  */

/* pcbiptrn.c - code for PCL built-in patterns */

#include "string_.h"
#include "pcpatrn.h"
#include "pcuptrn.h"
#include "pcbiptrn.h"

/*
 * Bitmap arrays for the built-in patterns.
 */
private const byte  bi_data_array[ (7 + 6) * 2 * 16 ] = {

    /* shade 1% to 2% */
    0x80, 0x80,   0x00, 0x00,   0x00, 0x00,   0x00, 0x00,
    0x00, 0x00,   0x00, 0x00,   0x00, 0x00,   0x00, 0x00,
    0x08, 0x08,   0x00, 0x00,   0x00, 0x00,   0x00, 0x00,
    0x00, 0x00,   0x00, 0x00,   0x00, 0x00,   0x00, 0x00,

    /* shade 3% to 10% */
    0x80, 0x80,   0x00, 0x00,   0x00, 0x00,   0x00, 0x00,
    0x08, 0x08,   0x00, 0x00,   0x00, 0x00,   0x00, 0x00,
    0x80, 0x80,   0x00, 0x00,   0x00, 0x00,   0x00, 0x00,
    0x08, 0x08,   0x00, 0x00,   0x00, 0x00,   0x00, 0x00,

    /* shade 11% to 20% */
    0xc0, 0xc0,   0xc0, 0xc0,   0x00, 0x00,   0x00, 0x00,
    0x0c, 0x0c,   0x0c, 0x0c,   0x00, 0x00,   0x00, 0x00,
    0xc0, 0xc0,   0xc0, 0xc0,   0x00, 0x00,   0x00, 0x00,
    0x0c, 0x0c,   0x0c, 0x0c,   0x00, 0x00,   0x00, 0x00,

    /* shade 21% to 35% */
    0xc1, 0xc1,   0xc1, 0xc1,   0x80, 0x80,   0x08, 0x08,
    0x1c, 0x1c,   0x1c, 0x1c,   0x08, 0x08,   0x80, 0x80,
    0xc1, 0xc1,   0xc1, 0xc1,   0x80, 0x80,   0x08, 0x08,
    0x1c, 0x1c,   0x1c, 0x1c,   0x08, 0x08,   0x80, 0x80,

    /* shade 36% to 55% */
    0xc1, 0xc1,   0xeb, 0xeb,   0xc1, 0xc1,   0x88, 0x88,
    0x1c, 0x1c,   0xbe, 0xbe,   0x1c, 0x1c,   0x88, 0x88,
    0xc1, 0xc1,   0xeb, 0xeb,   0xc1, 0xc1,   0x88, 0x88,
    0x1c, 0x1c,   0xbe, 0xbe,   0x1c, 0x1c,   0x88, 0x88,

    /* shade 56% to 80% */
    0xe3, 0xe3,   0xe3, 0xe3,   0xe3, 0xe3,   0xdd, 0xdd,
    0x3e, 0x3e,   0x3e, 0x3e,   0x3e, 0x3e,   0xdd, 0xdd,
    0xe3, 0xe3,   0xe3, 0xe3,   0xe3, 0xe3,   0xdd, 0xdd,
    0x3e, 0x3e,   0x3e, 0x3e,   0x3e, 0x3e,   0xdd, 0xdd,

    /* shade 81% to 99% */
    0xf7, 0xf7,   0xe3, 0xe3,   0xf7, 0xf7,   0xff, 0xff,
    0x7f, 0x7f,   0x3e, 0x3e,   0x7f, 0x7f,   0xff, 0xff,
    0xf7, 0xf7,   0xe3, 0xe3,   0xf7, 0xf7,   0xff, 0xff,
    0x7f, 0x7f,   0x3e, 0x3e,   0x7f, 0x7f,   0xff, 0xff,

    /* cross-hatch 1 (horizontal stripes) */
    0x00, 0x00,   0x00, 0x00,   0x00, 0x00,   0x00, 0x00,
    0x00, 0x00,   0x00, 0x00,   0x00, 0x00,   0xff, 0xff,
    0xff, 0xff,   0x00, 0x00,   0x00, 0x00,   0x00, 0x00,
    0x00, 0x00,   0x00, 0x00,   0x00, 0x00,   0x00, 0x00,

    /* cross-hatch 2 (vertical stripes) */
    0x01, 0x80,   0x01, 0x80,   0x01, 0x80,   0x01, 0x80,
    0x01, 0x80,   0x01, 0x80,   0x01, 0x80,   0x01, 0x80,
    0x01, 0x80,   0x01, 0x80,   0x01, 0x80,   0x01, 0x80,
    0x01, 0x80,   0x01, 0x80,   0x01, 0x80,   0x01, 0x80,

    /* cross-hatch 3 (upper right/lower left diagonal stripes) */
    0x80, 0x03,   0x00, 0x07,   0x00, 0x0e,   0x00, 0x1c,
    0x00, 0x38,   0x00, 0x70,   0x00, 0xe0,   0x01, 0xc0,
    0x03, 0x80,   0x07, 0x00,   0x0e, 0x00,   0x1c, 0x00,
    0x38, 0x00,   0x70, 0x00,   0xe0, 0x00,   0xc0, 0x01,

    /* cross-hatch 4 (upper left/lower right diagonal stripes) */
    0xc0, 0x01,   0xe0, 0x00,   0x70, 0x00,   0x38, 0x00,
    0x1c, 0x00,   0x0e, 0x00,   0x07, 0x00,   0x03, 0x80,
    0x01, 0xc0,   0x00, 0xe0,   0x00, 0x70,   0x00, 0x38,
    0x00, 0x1c,   0x00, 0x0e,   0x00, 0x07,   0x80, 0x03,

    /* cross-hatch 5 (aligned cross-hatch) */
    0x01, 0x80,   0x01, 0x80,   0x01, 0x80,   0x01, 0x80,
    0x01, 0x80,   0x01, 0x80,   0x01, 0x80,   0xff, 0xff,
    0xff, 0xff,   0x01, 0x80,   0x01, 0x80,   0x01, 0x80,
    0x01, 0x80,   0x01, 0x80,   0x01, 0x80,   0x01, 0x80,

    /* cross-hatch 6 (diagnoal cross-hatch) */
    0xc0, 0x03,   0xe0, 0x07,   0x70, 0x0e,   0x38, 0x1c,
    0x1c, 0x38,   0x0e, 0x70,   0x07, 0xe0,   0x03, 0xc0,
    0x03, 0xc0,   0x07, 0xe0,   0x0e, 0x70,   0x1c, 0x38,
    0x38, 0x1c,   0x70, 0x0e,   0xe0, 0x07,   0xc0, 0x03
};


#define make_pixmap(indx)                                           \
    { (byte *)(bi_data_array + indx * 2 * 16), 2, {16, 16}, 0, 1, 1 }

private const gs_depth_bitmap   bi_pixmap_array[7 + 6] = {
    make_pixmap(0),
    make_pixmap(1),
    make_pixmap(2),
    make_pixmap(3),
    make_pixmap(4),
    make_pixmap(5),
    make_pixmap(6),
    make_pixmap(7),
    make_pixmap(8),
    make_pixmap(9),
    make_pixmap(10),
    make_pixmap(11),
    make_pixmap(12)
};

private pcl_pattern_t * bi_pattern_array[countof(bi_pixmap_array)];

#define bi_cross_offset 7

/*
 * A special pattern, used for rendering images that interact with solid
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
private const gs_depth_bitmap   solid_pattern_pixmap = {
    (byte *)&solid_pattern_data, 1, {1, 1}, 0, 1, 1
};

private pcl_pattern_t * psolid_pattern;

/*
 * An "un-solid" pattern, similar to the solid pattern described above, but
 * with only background. This is used primarily to handle the case of an
 * uncolored patter with a white foreground color in GL/2, which such patterns
 * are completely transparent (if pattern transparency is on; note that what
 * the GL/2 documentation describes as source transparency is actually pattern
 * transparency).
 */
private const byte unsolid_pattern_data = 0x0;
private const gs_depth_bitmap   unsolid_pattern_pixmap = {
    (byte *)&unsolid_pattern_data, 1, {1, 1}, 0, 1, 1
};

private pcl_pattern_t * punsolid_pattern;


/*
 * The following where originally local statics, but were moved to top level
 * so as to work on systems that do not re-initialize BSS at each startup.
 */
private int             last_inten;
private pcl_pattern_t * plast_shade;

/* a pointer to the memory structure to be used for building built-in patterns */
private gs_memory_t *   pbi_mem;


/*
 * Initialize the built-in patterns
 */
  void
pcl_pattern_init_bi_patterns(
    gs_memory_t *   pmem
)
{
    memset(bi_pattern_array, 0, sizeof(bi_pattern_array));
    psolid_pattern = 0;
    punsolid_pattern = 0;
    last_inten = 0;
    plast_shade = 0;
    pbi_mem = pmem;
}

/*
 * Clear all built-in patterns. This is normally called during a reset, to
 * conserve memory.
 */
  void
pcl_pattern_clear_bi_patterns(void)
{
    int     i;

    for (i = 0; i < countof(bi_pattern_array); i++) {
        if (bi_pattern_array[i] != 0) {
            pcl_pattern_free_pattern( pbi_mem,
                                      bi_pattern_array[i],
                                      "clear PCL built-in patterns"
                                      );
            bi_pattern_array[i] = 0;
        }
    }

    last_inten = 0;
    plast_shade = 0;

    if (psolid_pattern != 0) {
        pcl_pattern_free_pattern( pbi_mem,
                                  psolid_pattern,
                                  "clear PCL built-in patterns"
                                  );

        psolid_pattern = 0;
    }
    if (punsolid_pattern != 0) {
        pcl_pattern_free_pattern( pbi_mem,
                                  punsolid_pattern,
                                  "clear PCL built-in patterns"
                                  );

        punsolid_pattern = 0;
    }
}

/*
 * Return the pointer to a built-in pattern, building it if inecessary.
 */
  private pcl_pattern_t *
get_bi_pattern(
    int             indx
)
{
    if (bi_pattern_array[indx] == 0) {
        (void)pcl_pattern_build_pattern( &(bi_pattern_array[indx]),
                                         &(bi_pixmap_array[indx]),
                                         pcl_pattern_uncolored,
                                         300,
                                         300,
                                         pbi_mem
                                         );
        bi_pattern_array[indx]->ppat_data->storage = pcds_internal;
    }
    return bi_pattern_array[indx];
}

/*
 * For a given intensity value, return the corresponding shade pattern. A
 * null return indicates that a solid pattern should be used - the caller
 * must look at the intensity to determine if it is black or white.
 */
  pcl_pattern_t *
pcl_pattern_get_shade(
    int             inten
)
{
    if (inten != last_inten) {
        last_inten = inten;
        if (inten <= 0)
            plast_shade = 0;
        else if (inten <= 2)
            plast_shade = get_bi_pattern(0);
        else if (inten <= 10)
            plast_shade = get_bi_pattern(1);
        else if (inten <= 20)
            plast_shade = get_bi_pattern(2);
        else if (inten <= 35)
            plast_shade = get_bi_pattern(3);
        else if (inten <= 55)
            plast_shade = get_bi_pattern(4);
        else if (inten <= 80)
            plast_shade = get_bi_pattern(5);
        else if (inten <= 99)
            plast_shade = get_bi_pattern(6);
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
    int             indx
)
{
    if ((indx < 1) || (indx > 6))
        return 0;
    else
        return get_bi_pattern(indx + bi_cross_offset - 1);
}

/*
 * Return the solid uncolored pattern, to be used with rasters (see above).
 */
  pcl_pattern_t *
pcl_pattern_get_solid_pattern(void)
{
    if (psolid_pattern == 0) {
        (void)pcl_pattern_build_pattern( &(psolid_pattern),
                                         &solid_pattern_pixmap,
                                         pcl_pattern_uncolored,
                                         300,
                                         300,
                                         pbi_mem
                                         );
        psolid_pattern->ppat_data->storage = pcds_internal;
    }
    return psolid_pattern;
}

/*
 * Return the "unsolid" uncolored patterns, to be used with GL/2.
 */
  pcl_pattern_t *
pcl_pattern_get_unsolid_pattern(void)
{
    if (punsolid_pattern == 0) {
        (void)pcl_pattern_build_pattern( &(punsolid_pattern),
                                         &unsolid_pattern_pixmap,
                                         pcl_pattern_uncolored,
                                         300,
                                         300,
                                         pbi_mem
                                         );
        punsolid_pattern->ppat_data->storage = pcds_internal;
    }
    return punsolid_pattern;
}
