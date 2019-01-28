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
/* Ordered Dither Screen Creation Tool. */

#include <stdlib.h>

#ifdef GS_LIB_BUILD
#define LIB_BUILD

#include "std.h"
#include "string_.h"
#include "gsmemory.h"
#include "math_.h"

#   define ALLOC(mem, size) (gs_alloc_bytes((gs_memory_t *)mem, size, "gen_ordered"))
#   define FREE(mem, ptr)   gs_free_object((gs_memory_t *)mem, ptr, "gen_ordered")

#   define PRINTF(mem, str) outprintf((gs_memory_t *)mem, str)
#   define PRINTF2(mem, str, v1, v2) outprintf((gs_memory_t *)mem, str, v1, v2)
#   define PRINTF4(mem, str, v1, v2, v3, v4) outprintf((gs_memory_t *)mem, str, v1, v2, v3, v4)
#   define PRINTF7(mem, str, v1, v2, v3, v4, v5, v6, v7) outprintf((gs_memory_t *)mem, str, v1, v2, v3, v4, v5, v6, v7)
#   define EPRINTF(mem, str) errprintf((gs_memory_t *)mem, str)
#   define EPRINTF1(mem, str, v1) errprintf((gs_memory_t *)mem, str, v1)
#   define EPRINTF3(mem, str, v1, v2, v3) errprintf((gs_memory_t *)mem, str, v1, v2, v3)

#endif /* defined GS_LIB_BUILD */

#ifndef LIB_BUILD

#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <sys/stat.h>
#include <math.h>

typedef unsigned char byte;
#define false 0
#define true 1
#ifndef __cplusplus
    typedef int bool;
#endif      /* __cpluplus */

/* Needed if standalone (main) */

#   define ALLOC(mem, size) (malloc(size))
#   define FREE(mem, ptr)   (free(ptr))

#   define PRINTF(mem, str) printf(str)
#   define PRINTF2(mem, str, v1, v2) printf(str, v1, v2)
#   define PRINTF4(mem, str, v1, v2, v3, v4) printf(str, v1, v2, v3, v4)
#   define PRINTF7(mem, str, v1, v2, v3, v4, v5, v6, v7) printf(str, v1, v2, v3, v4, v5, v6, v7)
#   define EPRINTF(mem, str) fprintf(stderr, str)
#   define EPRINTF1(mem, str, v1) fprintf(stderr, str, v1)
#   define EPRINTF3(mem, str, v1, v2, v3) fprintf(stderr, str, v1, v2, v3)

#endif /* ndef LIB_BUILD */

#include "gen_ordered.h"

typedef struct htsc_point_s {
    double x;
    double y;
} htsc_point_t;

typedef struct htsc_threshpoint {
    int x;
    int y;
    int value;
    int index;
    double dist_to_center;
} htsc_threshpoint_t;

typedef struct htsc_vertices_s {
    htsc_point_t lower_left;
    htsc_point_t upper_left;
    htsc_point_t upper_right;
    htsc_point_t lower_right;
} htsc_vertices_t;

typedef struct htsc_dot_shape_search_s {
    double norm;
    int index_x;
    int index_y;
} htsc_dot_shape_search_t;

typedef struct htsc_matrix_s {
   htsc_vector_t row[2];
} htsc_matrix_t;

typedef struct htsc_dither_pos_s {
    htsc_point_t *point;
    int number_points;
    int *locations;
} htsc_dither_pos_t;

static void htsc_determine_cell_shape(int *x, int *y, int *v, int *u,
                               int *N, htsc_param_t params, void *mem);
static double htsc_spot_value(spottype_t spot_type, double x, double y);
static int htsc_getpoint(htsc_dig_grid_t *dig_grid, int x, int y);
static void htsc_setpoint(htsc_dig_grid_t *dig_grid, int x, int y, int value);
static int  htsc_create_dot_mask(htsc_dig_grid_t *dig_grid, int x, int y, int u, int v,
                       double screen_angle, htsc_vertices_t vertices);
static void htsc_find_bin_center(htsc_dig_grid_t *dot_grid, htsc_vector_t *bin_center);
static int htsc_sumsum(htsc_dig_grid_t dig_grid);
static void htsc_create_dot_profile(htsc_dig_grid_t *dig_grid, int N, int x, int y,
                            int u, int v, double horiz_dpi,
                            double vert_dpi, htsc_vertices_t vertices,
                            htsc_point_t *one_index, spottype_t spot_type,
                            htsc_matrix_t trans_matrix);
static int htsc_allocate_supercell(htsc_dig_grid_t *super_cell, int x, int y, int u,
                           int v, int target_size, bool use_holladay_grid,
                           htsc_dig_grid_t dot_grid, int N, int *S, int *H, int *L);
static void htsc_tile_supercell(htsc_dig_grid_t *super_cell, htsc_dig_grid_t *dot_grid,
                 int x, int y, int u, int v, int N);
void create_2d_gauss_filter(double *filter, int x_size, int y_size,
                            double stdvalx, double stdvaly);
static int htsc_create_holladay_mask(htsc_dig_grid_t super_cell, int H, int L,
                               double gamma, htsc_dig_grid_t *final_mask);
static int htsc_create_dither_mask(htsc_dig_grid_t super_cell,
                             htsc_dig_grid_t *final_mask, int verbose,
                             int num_levels, int y, int x, double vert_dpi,
                             double horiz_dpi, int N, double gamma,
                             htsc_dig_grid_t dot_grid, htsc_point_t one_index);
static int htsc_create_nondithered_mask(htsc_dig_grid_t super_cell, int H, int L,
                          double gamma, htsc_dig_grid_t *final_mask);
static int htsc_gcd(int a, int b);
static int  htsc_lcm(int a, int b);
static int htsc_matrix_inverse(htsc_matrix_t matrix_in, htsc_matrix_t *matrix_out);
static void htsc_matrix_vector_mult(htsc_matrix_t matrix_in, htsc_vector_t vector_in,
                   htsc_vector_t *vector_out);
static int htsc_mask_to_tos(htsc_dig_grid_t *final_mask);
#if RAW_SCREEN_DUMP
static void htsc_dump_screen(htsc_dig_grid_t *dig_grid, char filename[]);
static void htsc_dump_float_image(double *image, int height, int width, double max_val,
                      char filename[]);
static void htsc_dump_byte_image(byte *image, int height, int width, double max_val,
                      char filename[]);
#endif

/* Initialize default values */
void htsc_set_default_params(htsc_param_t *params)
{
    params->scr_ang = 0;
    params->targ_scr_ang = 0;
    params->targ_lpi = 75;
    params->vert_dpi = 300;
    params->horiz_dpi = 300;
    params->targ_quant_spec = false;
    params->targ_quant = 256;
    params->targ_size = 1;
    params->targ_size_spec = false;
    params->spot_type = CIRCLE;
    params->holladay = false;
    params->gamma = 1.0;
    params->output_format = OUTPUT_TOS;
    params->verbose = 0;
}

int
htsc_gen_ordered(htsc_param_t params, int *S, htsc_dig_grid_t *final_mask)
{
    int num_levels;
    int x=0, y=0, v=0, u=0, N=0;
    htsc_vertices_t vertices;
    htsc_vector_t bin_center;
    htsc_point_t one_index = { 0., 0. };
    htsc_dig_grid_t dot_grid;
    htsc_dig_grid_t super_cell;
    int code;
    int H, L;
    htsc_matrix_t trans_matrix, trans_matrix_inv;
    int min_size;

    dot_grid.data = NULL;
    dot_grid.memory = final_mask->memory;
    super_cell.data = NULL;
    super_cell.memory = final_mask->memory;
    final_mask->data = NULL;
    params.targ_scr_ang = params.targ_scr_ang % 90;
    params.scr_ang = params.targ_scr_ang;
    /* Get the vector values that define the small cell shape */
    htsc_determine_cell_shape(&x, &y, &v, &u, &N, params, final_mask->memory);

    /* Figure out how many levels to dither across. */
    if (params.targ_quant_spec) {
        num_levels = ROUND((double) params.targ_quant / (double)N);
    } else {
        num_levels = 1;
    }
    if (num_levels < 1) num_levels = 1;
    if (num_levels == 1) {
        if (params.verbose > 0)
            PRINTF(final_mask->memory, "No additional dithering , creating minimal sized periodic screen\n");
        params.targ_size = 1;
    }

    /* Lower left of the cell is at the origin.  Define the other vertices */
    vertices.lower_left.x = 0;
    vertices.lower_left.y = 0;
    vertices.upper_left.x = x;
    vertices.upper_left.y = y;
    vertices.upper_right.x = x + u;
    vertices.upper_right.y = y + v;
    vertices.lower_right.x = u;
    vertices.lower_right.y = v;

    /* Create the matrix that is used to get us correctly into the dot shape function */
    trans_matrix.row[0].xy[0] = u;
    trans_matrix.row[0].xy[1] = x;
    trans_matrix.row[1].xy[0] = v;
    trans_matrix.row[1].xy[1] = y;
    code = htsc_matrix_inverse(trans_matrix, &trans_matrix_inv);
    if (code < 0) {
        EPRINTF(final_mask->memory, "ERROR! Singular Matrix Inversion!\n");
        return -1;
    }

    /* Create a binary mask that indicates where we need to define the dot turn
       on sequence or dot profile */
    code = htsc_create_dot_mask(&dot_grid, x, y, u, v, params.scr_ang, vertices);
    if (code < 0) {
        return -1;
    }
#if RAW_SCREEN_DUMP
    htsc_dump_screen(&dot_grid, "mask");
#endif
    /* A sanity check */
    if (htsc_sumsum(dot_grid) != -N) {
        EPRINTF(final_mask->memory, "ERROR! grid size problem!\n");
        return -1;
    }

    /* From the binary mask, find the center point.  This is needed to remove
       ambiguity during the TOS calculation from the dot profile.  We want to
       turn on those dots that are closests to the center first when there
       are ties */
    htsc_find_bin_center(&dot_grid, &bin_center);


    /* Now actually determine the turn on sequence */
    htsc_create_dot_profile(&dot_grid, N, x, y, u, v, params.horiz_dpi,
                            params.vert_dpi, vertices, &one_index,
                            params.spot_type, trans_matrix_inv);

#if RAW_SCREEN_DUMP
    htsc_dump_screen(&dot_grid, "dot_profile");
#endif
    /* Allocate super cell */
    code = htsc_allocate_supercell(&super_cell, x, y, u, v, params.targ_size,
                            params.holladay, dot_grid, N, S, &H, &L);
    if (code < 0) {
        EPRINTF(final_mask->memory, "ERROR! grid size problem!\n");
        return -1;
    }

    /* Make a warning about large requested quantization levels with no -s set */
    if (params.targ_size == 1 && num_levels > 1) {
        min_size = (int)ceil((double)params.targ_quant / (double)N);
        EPRINTF1(final_mask->memory, "To achieve %d quantization levels with the desired lpi,\n", params.targ_quant);
        EPRINTF1(final_mask->memory, "it is necessary to specify a SuperCellSize (-s) of at least %d.\n", min_size);
        EPRINTF(final_mask->memory, "Note that an even larger size may be needed to reduce pattern artifacts.\n");
        EPRINTF(final_mask->memory, "Because no SuperCellSize was specified, the minimum possible size\n");
        EPRINTF(final_mask->memory, "that can approximate the requested angle and lpi will be created\n");
        EPRINTF1(final_mask->memory, "with %d quantization levels.\n", N);
    }

    /* Go ahead and fill up the super cell grid with our growth dot values */
    htsc_tile_supercell(&super_cell, &dot_grid, x, y, u, v, N);
#if RAW_SCREEN_DUMP
    htsc_dump_screen(&super_cell, "super_cell_tiled");
#endif
    /* If we are using the Holladay grid (non dithered) then we are done. */
    if (params.holladay) {
        htsc_create_holladay_mask(super_cell, H, L, params.gamma, final_mask);
    } else {
        if ((super_cell.height == dot_grid.height &&
            super_cell.width == dot_grid.width) || num_levels == 1) {
            htsc_create_nondithered_mask(super_cell, H, L, params.gamma, final_mask);
        } else {
            /* Dont allow nonsense settings */
            if (num_levels * N > super_cell.height * super_cell.width) {
                EPRINTF3(final_mask->memory,
                         "Notice, %d quantization levels not possible with super cell of %d by %d\n",
                         num_levels, super_cell.height, super_cell.width);
                num_levels = ROUND((super_cell.height * super_cell.width) / (double)N);
                EPRINTF1(final_mask->memory, "Reducing dithering quantization to %d\n",
                         num_levels);
                EPRINTF1(final_mask->memory, "For an effective quantization of %d\n",
                         super_cell.height * super_cell.width);
            }
            code = htsc_create_dither_mask(super_cell, final_mask, params.verbose, num_levels,
                                           y, x, params.vert_dpi, params.horiz_dpi, N,
                                           params.gamma, dot_grid, one_index);
        }
    }
    final_mask->bin_center = bin_center;

    /* Now if the requested format is turn-on-sequence, convert the "data" */
    if (code == 0 && params.output_format == OUTPUT_TOS) {
        code = htsc_mask_to_tos(final_mask);
    }
    /* result in in final_mask, clean up working arrays allocated. */
    FREE(final_mask->memory, dot_grid.data);
    FREE(final_mask->memory, super_cell.data);
    return code;
}

/* comparison for use in qsort */
static int
compare(const void * a, const void * b)
{
    const htsc_threshpoint_t *val_a = a;
    const htsc_threshpoint_t *val_b = b;
    double cost = val_a->value - val_b->value;

    /* If same value, use distance to center for decision */
    if (cost == 0)
        cost = val_a->dist_to_center - val_b->dist_to_center;
    if (cost == 0)
        return 0;
    if (cost < 0)
        return -1;
    return 1;
}

static int
htsc_mask_to_tos(htsc_dig_grid_t *final_mask)
{
    int width = final_mask->width;
    int height = final_mask->height;
    htsc_vector_t center = final_mask->bin_center;
    int *buff_ptr = final_mask->data;
    int x, y, k = 0;
    int count = height * width;
    htsc_threshpoint_t *values;
    int *tos;

    values = (htsc_threshpoint_t *) ALLOC(final_mask->memory,
                                          sizeof(htsc_threshpoint_t) * width * height);
    if (values == NULL) {
        EPRINTF(final_mask->memory, "ERROR! malloc failure in htsc_mask_to_tos!\n");
        return -1;
    }
    tos = (int *) ALLOC(final_mask->memory, sizeof(int) * 2 * height * width);
    if (tos == NULL) {
        FREE(final_mask->memory, values);
        EPRINTF(final_mask->memory, "ERROR! malloc failure in htsc_mask_to_tos!\n");
        return -1;
    }
    /* Do a sort on the values and then output the coordinates */
    /* First get a list made with the unsorted values and coordinates */
    for (y = 0; y < height; y++) {
        for ( x = 0; x < width; x++ ) {
            values[k].value = *buff_ptr;
            values[k].x = x;
            values[k].y = y;
            values[k].index = k;
            values[k].dist_to_center = (x - center.xy[0]) * (x - center.xy[0]) +
                                       (y - center.xy[1]) * (y - center.xy[1]);
            buff_ptr++;
            k = k + 1;
        }
    }
#if RAW_SCREEN_DUMP
    EPRINTF(final_mask->memory, "Unsorted\n");
    for (k = 0; k < count; k++) {
        EPRINTF(final_mask->memory, "Index %d : x = %d y = %d dist = %4.2lf value = %d \n",
               values[k].index, values[k].x, values[k].y, values[k].dist_to_center, values[k].value);
    }
#endif
        /* Sort */
    qsort(values, height * width, sizeof(htsc_threshpoint_t), compare);
#if RAW_SCREEN_DUMP
    EPRINTF(final_mask->memory, "Sorted\n");
    for (k = 0; k < count; k++) {
        EPRINTF(final_mask->memory, "Index %d : x = %d y = %d dist = %4.2lf value = %d \n",
                values[k].index, values[k].x, values[k].y, values[k].dist_to_center, values[k].value);
    }
#endif

    FREE(final_mask->memory, final_mask->data);
    final_mask->data = tos;
    buff_ptr = tos;

    for (k=0; k < count; k++) {
        *buff_ptr++ = values[count - 1 - k].x;
        *buff_ptr++ = values[count - 1 - k].y;
    }
    FREE(final_mask->memory, values);
    return 0;
}

static void
htsc_determine_cell_shape(int *x_out, int *y_out, int *v_out,
                          int *u_out, int *N_out, htsc_param_t params, void *mem)
{
    double x = 0., y = 0., v = 0., u = 0., N = 0.;
    double frac, scaled_x;
    double ratio;
    const double  pi = 3.14159265358979323846f;
    double true_angle, lpi;
    double prev_lpi, max_lpi = 0.;
    bool use = false;
    double x_use = 0.,y_use = 0.;

    /* Go through and find the rational angle options that gets us to the
       best LPI.  Pick the one that is just over what is requested.
       That is really our limiting factor here.  Pick it and
       then figure out how much dithering we need to do to get to the proper
       number of levels.  */
    frac = tan( params.scr_ang * pi / 180.0 );
    ratio = frac * params.horiz_dpi / params.vert_dpi;
    scaled_x = params.horiz_dpi / params.vert_dpi;
    /* The minimal step is in x */
    prev_lpi = 0;
    if (ratio < 1 && ratio != 0) {
        if (params.verbose > 0) {
            PRINTF(mem, "x\ty\tu\tv\tAngle\tLPI\tLevels\n");
            PRINTF(mem, "-----------------------------------------------------------\n");
        }
        for (x = 1; x < 11; x++) {
            x_use = x;
            y=ROUND((double) x_use / ratio);
            true_angle = 180.0 * atan(((double) x_use / params.horiz_dpi) / ( (double) y / params.vert_dpi) ) / pi;
            lpi = 1.0/( sqrt( ((double) y / params.vert_dpi) * ( (double) y / params.vert_dpi) +
                                    ( (double) x_use / params.horiz_dpi) * ((double) x_use / params.horiz_dpi) ));
            v = -x_use / scaled_x;
            u = y * scaled_x;
            N = y *u - x_use * v;
            if (prev_lpi == 0) {
                prev_lpi = lpi;
                if (params.targ_lpi > lpi) {
                    EPRINTF(mem, "Warning this lpi is not achievable!\n");
                    EPRINTF(mem, "Resulting screen will be poorly quantized\n");
                    EPRINTF(mem, "or completely stochastic!\n");
                    use = true;
                }
                max_lpi = lpi;
            }
            if (prev_lpi >= params.targ_lpi && lpi < params.targ_lpi) {
                if (prev_lpi == max_lpi) {
                    EPRINTF(mem, "Notice lpi is at the maximimum level possible.\n");
                    EPRINTF(mem, "This may result in poor quantization. \n");
                }
                /* Reset these to previous x */
                x_use = x - 1;
                y=ROUND((double) x_use / ratio);
                true_angle =
                    180.0 * atan(((double) x_use / params.horiz_dpi) / ( (double) y / params.vert_dpi) ) / pi;
                lpi =
                    1.0/( sqrt( ((double) y / params.vert_dpi) * ( (double) y / params.vert_dpi) +
                                        ( (double) x_use / params.horiz_dpi) * ((double) x_use / params.horiz_dpi) ));
                v = -x_use / scaled_x;
                u = y * scaled_x;
                N = y *u - x_use * v;
                use = true;
            }
            if (use == true) {
                if (prev_lpi == max_lpi) {
                    /* Print out the final one that we will use */
                    if (params.verbose > 0)
                        PRINTF7(mem, "%3.0lf\t%3.0lf\t%3.0lf\t%3.0lf\t%3.1lf\t%3.1lf\t%3.0lf\n",
                                x_use,y,u,v,true_angle,lpi,N);
                }
                break;
            }
            if (params.verbose > 0)
                PRINTF7(mem, "%3.0lf\t%3.0lf\t%3.0lf\t%3.0lf\t%3.1lf\t%3.1lf\t%3.0lf\n",
                        x_use,y,u,v,true_angle,lpi,N);
            prev_lpi = lpi;
        }
        x = x_use;
    }
    if (ratio >= 1 && ratio!=0) {
        /* The minimal step is in y */
        if (params.verbose > 0) {
            PRINTF(mem, "x\ty\tu\tv\tAngle\tLPI\tLevels\n");
            PRINTF(mem, "-----------------------------------------------------------\n");
        }
        for (y = 1, lpi = 99999999; lpi > params.targ_lpi; y++) {
            y_use = y;
            x = ROUND(y_use * ratio);
            /* compute the true angle */
            true_angle = 180.0 * atan((x / params.horiz_dpi) / (y_use / params.vert_dpi)) / pi;
            lpi = 1.0 / sqrt( (y_use / params.vert_dpi) * (y_use / params.vert_dpi) +
                                (x / params.horiz_dpi) * (x / params.horiz_dpi));
            v = ROUND(-x / scaled_x);
            u = ROUND(y_use * scaled_x);
            N = y_use * u - x * v;
            if (prev_lpi == 0) {
                prev_lpi = lpi;
                if (params.targ_lpi > lpi) {
                    EPRINTF(mem, "Warning this lpi is not achievable!\n");
                    EPRINTF(mem, "Resulting screen will be poorly quantized\n");
                    EPRINTF(mem, "or completely stochastic!\n");
                    use = true;
                }
                max_lpi = lpi;
            }
            if (prev_lpi >= params.targ_lpi && lpi < params.targ_lpi) {
                if (prev_lpi == max_lpi) {
                    EPRINTF(mem, "Warning lpi will be slightly lower than target.\n");
                    EPRINTF(mem, "An increase will result in poor \n");
                    EPRINTF(mem, "quantization or a completely stochastic screen!\n");
                } else {
                    /* Reset these to previous x */
                    y_use = y - 1;
                    x = ROUND(y_use * ratio);
                    /* compute the true angle */
                    true_angle = 180.0 * atan((x / params.horiz_dpi) / (y_use / params.vert_dpi)) / pi;
                    lpi = 1.0 / sqrt( (y_use / params.vert_dpi) * (y_use / params.vert_dpi) +
                                        (x / params.horiz_dpi) * (x / params.horiz_dpi));
                    v = ROUND(-x / scaled_x);
                    u = ROUND(y_use * scaled_x);
                    N = y_use * u - x * v;
                }
                use = true;
            }
            if (use == true) {
                if (prev_lpi == max_lpi) {
                    /* Print out the final one that we will use */
                    PRINTF7(mem, "%3.0lf\t%3.0lf\t%3.0lf\t%3.0lf\t%3.1lf\t%3.1lf\t%3.0lf\n",
                                    x,y_use,u,v,true_angle,lpi,N);
                }
                break;
            }
            if (params.verbose > 0)
                PRINTF7(mem, "%3.0lf\t%3.0lf\t%3.0lf\t%3.0lf\t%3.1lf\t%3.1lf\t%3.0lf\n",
                        x,y_use,u,v,true_angle,lpi,N);
            prev_lpi = lpi;
        }
        y = y_use;
    }
    if (ratio == 0) {
        /* 0 degrees */
        if (scaled_x >= 1) {
            if (params.verbose > 0) {
                PRINTF(mem, "x\ty\tu\tv\tAngle\tLPI\tLevels\n");
                PRINTF(mem, "-----------------------------------------------------------\n");
            }
            for (y = 1, lpi=9999999; lpi > params.targ_lpi; y++) {
                y_use = y;
                x = ROUND( y_use * ratio );
                v = ROUND(-x / scaled_x);
                u = ROUND(y_use * scaled_x);
                N = y_use * u - x * v;
                true_angle = 0;
                lpi = 1.0/(double) sqrt( (double) ((y_use / params.vert_dpi) *
                    (y_use / params.vert_dpi) + (x / params.horiz_dpi) * (x / params.horiz_dpi)) );
                if (prev_lpi == 0) {
                    prev_lpi = lpi;
                    if (params.targ_lpi > lpi) {
                        EPRINTF(mem, "Warning this lpi is not achievable!\n");
                        EPRINTF(mem, "Resulting screen will be poorly quantized\n");
                        EPRINTF(mem, "or completely stochastic!\n");
                        use = true;
                    }
                    max_lpi = lpi;
                }
                if (prev_lpi >= params.targ_lpi && lpi < params.targ_lpi) {
                    if (prev_lpi == max_lpi) {
                        EPRINTF(mem, "Warning lpi will be slightly lower than target.\n");
                        EPRINTF(mem, "An increase will result in poor \n");
                        EPRINTF(mem, "quantization or a completely stochastic screen!\n");
                    } else {
                        /* Reset these to previous x */
                        y_use = y - 1;
                        x = ROUND( y_use * ratio );
                        v = ROUND(-x / scaled_x);
                        u = ROUND(y_use * scaled_x);
                        N = y_use * u - x * v;
                        true_angle = 0;
                        lpi = 1.0/(double) sqrt( (double) ((y_use / params.vert_dpi) *
                            (y_use / params.vert_dpi) + (x / params.horiz_dpi) * (x / params.horiz_dpi)) );
                    }
                    use = true;
                }
                if (use == true) {
                    if (prev_lpi == max_lpi) {
                        /* Print out the final one that we will use */
                        if (params.verbose > 0)
                            PRINTF7(mem, "%3.0lf\t%3.0lf\t%3.0lf\t%3.0lf\t%3.1lf\t%3.1lf\t%3.0lf\n",
                                    x,y_use,u,v,true_angle,lpi,N);
                    }
                    break;
                }
                if (params.verbose > 0)
                    PRINTF7(mem, "%3.0lf\t%3.0lf\t%3.0lf\t%3.0lf\t%3.1lf\t%3.1lf\t%3.0lf\n",
                            x,y_use,u,v,true_angle,lpi,N);
                prev_lpi = lpi;
            }
            y = y_use;
        } else {
        if (params.verbose > 0) {
            PRINTF(mem, "x\ty\tu\tv\tAngle\tLPI\tLevels\n");
            PRINTF(mem, "-----------------------------------------------------------\n");
        }
            for (x = 1; x < 11; x++) {
                x_use = x;
                y = ROUND(x_use * ratio);
                true_angle = 0;
                lpi = 1.0/( sqrt( (y / params.vert_dpi) * (y / params.vert_dpi) +
                            (x_use / params.horiz_dpi) * (x_use / params.horiz_dpi) ));
                v = ROUND( -x_use / scaled_x);
                u = ROUND( y * scaled_x);
                N = y  *u - x_use * v;
                if (prev_lpi == 0) {
                    prev_lpi = lpi;
                    if (params.targ_lpi > lpi) {
                        EPRINTF(mem, "Warning this lpi is not achievable!\n");
                        EPRINTF(mem, "Resulting screen will be poorly quantized\n");
                        EPRINTF(mem, "or completely stochastic!\n");
                        use = true;
                    }
                    max_lpi = lpi;
                }
                if (prev_lpi > params.targ_lpi && lpi < params.targ_lpi) {
                    if (prev_lpi == max_lpi) {
                        EPRINTF(mem, "Warning lpi will be slightly lower than target.\n");
                        EPRINTF(mem, "An increase will result in poor \n");
                        EPRINTF(mem, "quantization or a completely stochastic screen!\n");
                    } else {
                        /* Reset these to previous x */
                        x_use = x - 1;
                        y = ROUND(x_use * ratio);
                        true_angle = 0;
                        lpi = 1.0/( sqrt( (y / params.vert_dpi) * (y / params.vert_dpi) +
                                    (x_use / params.horiz_dpi) * (x_use / params.horiz_dpi) ));
                        v = ROUND( -x_use / scaled_x);
                        u = ROUND( y * scaled_x);
                        N = y  *u - x_use * v;
                    }
                    use = true;
                }
                if (use == true) {
                    if (prev_lpi == max_lpi) {
                        /* Print out the final one that we will use */
                        if (params.verbose > 0)
                            PRINTF7(mem, "%3.0lf\t%3.0lf\t%3.0lf\t%3.0lf\t%3.1lf\t%3.1lf\t%3.0lf\n",
                                    x_use,y,u,v,true_angle,lpi,N);
                    }
                    break;
                }
                if (params.verbose > 0)
                    PRINTF7(mem, "%3.0lf\t%3.0lf\t%3.0lf\t%3.0lf\t%3.1lf\t%3.1lf\t%3.0lf\n",
                            x_use,y,u,v,true_angle,lpi,N);
                prev_lpi = lpi;
            }
            x = x_use;
        }
    }
    *x_out = (int)x;
    *y_out = (int)y;
    *v_out = (int)v;
    *u_out = (int)u;
    *N_out = (int)N;
}

static void
htsc_create_dot_profile(htsc_dig_grid_t *dig_grid, int N, int x, int y, int u, int v,
                        double horiz_dpi, double vert_dpi, htsc_vertices_t vertices,
                        htsc_point_t *one_index, spottype_t spot_type, htsc_matrix_t trans_matrix)
{
    int done, dot_index, hole_index, count, index_x, index_y;
    htsc_dot_shape_search_t dot_search;
    int k, val_min;
    double dist;
    int j;
    htsc_vector_t vector_in, vector_out;
    double x_offset = (x % 2 == 0 ? 0.5 : 0);
    double y_offset = (y % 2 == 0 ? 0.5 : 0);

    done = 0;
    dot_index = 1;
    hole_index = N;
    count = 0;
    val_min=MIN(0,v);

    while (!done) {

        /* First perform a search for largest dot value for those remaining
           dots */
        index_x = 0;
        dot_search.index_x = 0;
        dot_search.index_y = 0;
        dot_search.norm = -100000000; /* Hopefully the dot func is not this small */
        for (k = 0; k < x + u; k++) {
            index_y = 0;
            for (j = val_min; j < y; j++) {
                if ( htsc_getpoint(dig_grid, index_x, index_y) == -1 ) {

                    /* For the spot function we want to make sure that
                       we are properly adjusted to be in the range from
                       -1 to +1.  j and k are moving in the transformed
                       (skewed/rotated) space. First untransform the value */
                    vector_in.xy[0] = k + x_offset;
                    vector_in.xy[1] = j + y_offset;
                    htsc_matrix_vector_mult(trans_matrix, vector_in,
                                            &vector_out);

                    /* And so now we are in the range 0, 1 get us to -.5, .5 */
                    vector_out.xy[0] = 2.0 * vector_out.xy[0] - 1.0;
                    vector_out.xy[1] = 2.0 * vector_out.xy[1] - 1.0;
                    dist = htsc_spot_value(spot_type, vector_out.xy[0],
                                           vector_out.xy[1]);
                    if (dist > dot_search.norm) {
                        dot_search.norm = dist;
                        dot_search.index_x = index_x;
                        dot_search.index_y = index_y;
                    }

                }
                index_y++;
            }
            index_x++;
        }
        /* Assign the index for this position */
        htsc_setpoint(dig_grid, dot_search.index_x, dot_search.index_y,
                      dot_index);
        dot_index++;
        count++;
        if (count == N) {
            done = 1;
            break;
        }

        /* The ones position for the dig_grid is located at the first dot_search
           entry.  We need this later so grab it now */
        if (count == 1) {
            one_index->x = dot_search.index_x;
            one_index->y = dot_search.index_y;
        }

        /* Now search for the closest one to a vertex (of those remaining).
           and assign the current largest index */
        index_x = 0;
        dot_search.index_x = 0;
        dot_search.index_y = 0;
        dot_search.norm = 10000000000;  /* Or this large */
        for (k = 0; k < x + u; k++) {
            index_y = 0;
            for (j = val_min; j < y; j++) {
                if ( htsc_getpoint(dig_grid, index_x, index_y) == -1 ) {

                    /* For the spot function we want to make sure that
                       we are properly adjusted to be in the range from
                       -1 to +1.  j and k are moving in the transformed
                       (skewed/rotated) space. First untransform the value */
                    vector_in.xy[0] = k + x_offset;
                    vector_in.xy[1] = j + y_offset;
                    htsc_matrix_vector_mult(trans_matrix, vector_in,
                                            &vector_out);

                    /* And so now we are in the range 0, 1 get us to -.5, .5 */
                    vector_out.xy[0] = 2.0 * vector_out.xy[0] - 1.0;
                    vector_out.xy[1] = 2.0 * vector_out.xy[1] - 1.0;
                    dist = htsc_spot_value(spot_type, vector_out.xy[0],
                                           vector_out.xy[1]);

                    if (dist < dot_search.norm) {
                        dot_search.norm = dist;
                        dot_search.index_x = index_x;
                        dot_search.index_y = index_y;
                    }
                }
                index_y++;
            }
            index_x++;
        }
        /* Assign the index for this position */
        htsc_setpoint(dig_grid, dot_search.index_x, dot_search.index_y, hole_index);
        hole_index--;
        count++;
        if (count == N) {
            done = 1;
            break;
        }
    }
}

/* This creates a mask for creating the dot shape */
static int
htsc_create_dot_mask(htsc_dig_grid_t *dot_grid, int x, int y, int u, int v,
                 double screen_angle, htsc_vertices_t vertices)
{
    int k,j,val_min,index_x,index_y;
    double slope1, slope2;
    int t1,t2,t3,t4,point_in;
    double b3, b4;
    htsc_point_t test_point;

    if (screen_angle != 0) {
        slope1 = (double) y / (double) x;
        slope2 = (double) v / (double) u;
        val_min=MIN(0,v);
        dot_grid->height = abs(val_min) + y;
        dot_grid->width = x + u;
        dot_grid->data =
            (int *) ALLOC(dot_grid->memory, dot_grid->height * dot_grid->width * sizeof(int));
        if (dot_grid->data == NULL)
            return -1;
        memset(dot_grid->data,0,dot_grid->height * dot_grid->width * sizeof(int));
        index_x = 0;
        for (k = 0; k < (x+u); k++) {
            index_y=0;
            for (j = val_min; j < y; j++) {
                test_point.x = k + 0.5;
                test_point.y = j + 0.5;
                /* test 1 and 2 */
                t1 = (slope1 * test_point.x >= test_point.y);
                t2 = (slope2 * test_point.x <= test_point.y);
                /* test 3 */
                b3 = vertices.upper_left.y - slope2 * vertices.upper_left.x;
                t3 = ((slope2 * test_point.x + b3) > test_point.y);
                /* test 4 */
                b4 = vertices.lower_right.y - slope1 * vertices.lower_right.x;
                t4=((slope1 * test_point.x + b4) < test_point.y);
                point_in = (t1 && t2 && t3 && t4);
                if (point_in) {
                    htsc_setpoint(dot_grid, index_x, index_y, -1);
                }
                index_y++;
            }
            index_x++;
        }
    } else {
        /* All points are valid */
        dot_grid->height = y;
        dot_grid->width = u;
        dot_grid->data = (int *) ALLOC(dot_grid->memory, y * u * sizeof(int));
        if (dot_grid->data == NULL)
            return -1;
        memset(dot_grid->data, -1, y * u * sizeof(int));
        val_min = 0;
    }
    return 0;
}

static void
htsc_find_bin_center(htsc_dig_grid_t *dot_grid, htsc_vector_t *bin_center)
{
    int h = dot_grid->height;
    int w = dot_grid->width;
    int min_y = h + 1;
    int min_x = w + 1;
    int max_y = -1;
    int max_x = -1;
    int x, y;

    for (x = 0; x < w; x++) {
        for (y = 0; y < h; y++) {
            if (htsc_getpoint(dot_grid, x, y) == -1)
            {
                if (x < min_x)
                    min_x = x;
                if (x > max_x)
                    max_x = x;
                if (y < min_y)
                    min_y = y;
                if (y > max_y)
                    max_y = y;
            }
        }
    }
    bin_center->xy[0] = (max_x - min_x) / 2.0;
    bin_center->xy[1] = (max_y - min_y) / 2.0;
}

static int
htsc_getpoint(htsc_dig_grid_t *dig_grid, int x, int y)
{
    return dig_grid->data[ y * dig_grid->width + x];
}

static void
htsc_setpoint(htsc_dig_grid_t *dig_grid, int x, int y, int value)
{
    int kk = 0;;
    if (x < 0 || x > dig_grid->width-1 || y < 0 || y > dig_grid->height-1) {
        kk++;  /* Here to catch issues during debug */
    }
    dig_grid->data[ y * dig_grid->width + x] = value;
}

static int
htsc_sumsum(htsc_dig_grid_t dig_grid)
{

    int pos;
    int grid_size =  dig_grid.width *  dig_grid.height;
    int value = 0;
    int *ptr = dig_grid.data;

    for (pos = 0; pos < grid_size; pos++) {
        value += (*ptr++);
    }
    return value;
}

static int
htsc_gcd(int a, int b)
{
    if ( a == 0 && b == 0 ) return 0;
    if ( b == 0 ) return a;
    if ( a == 0 ) return b;
    while (1) {
        a = a % b;
        if (a == 0) {
            return b;
        }
        b = b % a;
        if (b == 0) {
            return a;
        }
    }
}

static int
htsc_lcm(int a, int b)
{
    int product = a * b;
    int gcd = htsc_gcd(a,b);
    int lcm;

    if (gcd == 0)
        return -1;

    lcm = product/gcd;
    return lcm;
}

static int
htsc_matrix_inverse(htsc_matrix_t matrix_in, htsc_matrix_t *matrix_out)
{
    double determinant;

    determinant = matrix_in.row[0].xy[0] * matrix_in.row[1].xy[1] -
                  matrix_in.row[0].xy[1] * matrix_in.row[1].xy[0];
    if (determinant == 0)
        return -1;
    matrix_out->row[0].xy[0] = matrix_in.row[1].xy[1] / determinant;
    matrix_out->row[0].xy[1] = -matrix_in.row[0].xy[1] / determinant;
    matrix_out->row[1].xy[0] = -matrix_in.row[1].xy[0] / determinant;
    matrix_out->row[1].xy[1] = matrix_in.row[0].xy[0] / determinant;
    return 0;
}

static void
htsc_matrix_vector_mult(htsc_matrix_t matrix_in, htsc_vector_t vector_in,
                   htsc_vector_t *vector_out)
{
    vector_out->xy[0] = matrix_in.row[0].xy[0] * vector_in.xy[0] +
                        matrix_in.row[0].xy[1] * vector_in.xy[1];
    vector_out->xy[1] = matrix_in.row[1].xy[0] * vector_in.xy[0] +
                        matrix_in.row[1].xy[1] * vector_in.xy[1];
}

static int
htsc_allocate_supercell(htsc_dig_grid_t *super_cell, int x, int y, int u,
                        int v, int target_size, bool use_holladay_grid,
                        htsc_dig_grid_t dot_grid, int N, int *S, int *H, int *L)
{
    htsc_matrix_t matrix;
    htsc_matrix_t matrix_inv;
    int code;
    int k, j;
    htsc_vector_t vector_in, m_and_n;
    int Dfinal;
    double diff_val[2];
    double m_and_n_round;
    int lcm_value;
    int super_size_x, super_size_y;
    int min_vert_number;
    int a, b;

    /* Use Holladay Algorithm to create rectangular matrix for screening */
    *H = htsc_gcd((int) abs(y), (int) abs(v));
    if (*H == 0)
        return -1;

    *L = N / *H;
    /* Compute the shift factor */
    matrix.row[0].xy[0] = x;
    matrix.row[0].xy[1] = u;
    matrix.row[1].xy[0] = y;
    matrix.row[1].xy[1] = v;

    code = htsc_matrix_inverse(matrix, &matrix_inv);
    if (code < 0) {
        EPRINTF(dot_grid.memory, "ERROR! matrix singular!\n");
        return -1;
    }
    vector_in.xy[1] = *H;
    Dfinal = 0;
    for (k = 1; k < *L+1; k++) {
        vector_in.xy[0] = k;
        htsc_matrix_vector_mult(matrix_inv, vector_in, &m_and_n);
        for (j = 0; j < 2; j++) {
            m_and_n_round = ROUND(m_and_n.xy[j]);
            diff_val[j] = fabs((double) m_and_n.xy[j] -  (double) m_and_n_round);
        }
        if (diff_val[0] < 0.00000001 && diff_val[1] < 0.00000001) {
            Dfinal = k;
            break;
        }
    }
    if (Dfinal == 0) {
        EPRINTF(dot_grid.memory, "ERROR! computing Holladay Grid\n");
        return -1;
    }
    *S = *L - Dfinal;
  /* Make a large screen of multiple cells and then vary
     the growth rate of the dots to get additional quantization levels
     The macrocell must be H*a by L*b where a and b are integers due to the
     periodicity of the screen. */

    /* To create the Holladay screen (no stocastic stuff),
       select the size H and L to create the matrix i.e. super_size_x
       and super_size_y need to be H by L at least and then just take an
       H by L section. */
    /* Force a periodicity in the screen to avoid the shift factor */
    if (*S != 0) {
        lcm_value = htsc_lcm(*L,*S);
        if (lcm_value < 0)
            return -1;

        min_vert_number = *H * lcm_value / *S;
    } else {
        lcm_value = *L;
        min_vert_number = *H;
    }

    a = (int)ceil((double) target_size / (double) lcm_value);
    b = (int)ceil((double) target_size / (double) min_vert_number);

    /* super_cell Size is  b*min_vert_number by a*lcm_value
       create the large cell */

    if (use_holladay_grid) {
        super_size_x = MAX(*L, dot_grid.width);
        super_size_y = MAX(*H, dot_grid.height);
    } else {
        super_size_x = a * lcm_value;
        super_size_y = b * min_vert_number;
    }
    super_cell->height = super_size_y;
    super_cell->width = super_size_x;
    super_cell->data =
        (int *) ALLOC(dot_grid.memory, super_size_x * super_size_y * sizeof(int));
    if (super_cell->data == NULL)
        return -1;
    memset(super_cell->data, 0, super_size_x * super_size_y * sizeof(int));
    return 0;
}

static void
htsc_supercell_assign_point(int new_k, int new_j, int sc_xsize, int sc_ysize,
                            htsc_dig_grid_t *super_cell, int val, int *num_set )
{
    if (new_k >= 0 && new_j >= 0 && new_k < sc_xsize && new_j < sc_ysize) {
        if (htsc_getpoint(super_cell,new_k,new_j) == 0) {
            htsc_setpoint(super_cell,new_k,new_j,val);
            (*num_set)++;
        }
    }
}

static void
htsc_tile_supercell(htsc_dig_grid_t *super_cell, htsc_dig_grid_t *dot_grid,
                 int x, int y, int u, int v, int N)
{
    int sc_ysize = super_cell->height;
    int sc_xsize = super_cell->width;
    int dot_ysize = dot_grid->height;
    int dot_xsize = dot_grid->width;
    int total_num = sc_ysize * sc_xsize;
    bool done = false;
    int k,j;
    int new_k, new_j;
    int num_set = 0;
    int val;

    for (k = 0; k < dot_xsize; k++) {
        for (j = 0; j < dot_ysize; j++) {
            val = htsc_getpoint(dot_grid,k,j);
            if (val > 0) {
                htsc_setpoint(super_cell,k,j,val);
                num_set++;
            }
        }
    }
    if (num_set == total_num) {
            done = true;
    }
    while (!done) {
        for (k = 0; k < sc_xsize; k++) {
            for (j = 0; j < sc_ysize; j++) {
                val = htsc_getpoint(super_cell,k,j);
                if (val != 0) {
                    new_k = k - x;
                    new_j = j - y;
                    htsc_supercell_assign_point(new_k, new_j, sc_xsize, sc_ysize,
                                                super_cell, val, &num_set);
                    new_k = k + x;
                    new_j = j + y;
                    htsc_supercell_assign_point(new_k, new_j, sc_xsize, sc_ysize,
                                                super_cell, val, &num_set);
                    new_k = k - u;
                    new_j = j - v;
                    htsc_supercell_assign_point(new_k, new_j, sc_xsize, sc_ysize,
                                                super_cell, val, &num_set);
                    new_k = k + u;
                    new_j = j + v;
                    htsc_supercell_assign_point(new_k, new_j, sc_xsize, sc_ysize,
                                                super_cell, val, &num_set);
                }
            }
        }
        if (num_set == total_num) {
            done = true;
        }
    }
}

/* Create 2d gaussian filter that varies with respect to coordinate
   spatial resolution */
void
create_2d_gauss_filter(double *filter, int x_size, int y_size,
    double stdvalx, double stdvaly)
{
    int x_half_size  = (x_size-1)/2;
    int y_half_size = (y_size-1)/2;
    int k,j;
    double arg, val;
    double sum = 0;
    double max_val = 0;
    int total_size = x_size * y_size;
    int index_x, index_y;

    for (j = -y_half_size; j < y_half_size+1; j++) {
        index_y = j + y_half_size;
        for (k = -x_half_size; k < x_half_size+1; k++) {
            arg   = -(k  * k / (stdvalx * stdvalx) +
                      j * j / (stdvaly * stdvaly) ) /2;
            val = exp(arg);
            sum += val;
            if (val > max_val) max_val = val;
            index_x = k + x_half_size;
            filter[index_y * x_size + index_x] = val;
        }
    }
    for (j = 0; j < total_size; j++) {
        filter[j]/=sum;
    }
#if RAW_SCREEN_DUMP
    htsc_dump_float_image(filter, y_size, x_size, max_val/sum, "guass_filt");
#endif
}

/* 2-D convolution (or correlation) with periodic boundary condition */
static void
htsc_apply_filter(byte *screen_matrix, int num_cols_sc,
                  int num_rows_sc, double *filter, int num_cols_filt,
                  int num_rows_filt, double *screen_blur,
    double *max_val, htsc_point_t *max_pos, double *min_val,
                  htsc_point_t *min_pos)
{
    int k,j,kk,jj;
    double sum;
    int half_cols_filt = (num_cols_filt-1)/2;
    int half_rows_filt = (num_rows_filt-1)/2;
    int j_circ,k_circ;
    double fmax_val = -1;
    double fmin_val = 100000000;
    htsc_point_t fmax_pos = { 0.0, 0.0 }, fmin_pos = { 0.0, 0.0 };

    for (j = 0; j < num_rows_sc; j++) {
        for (k = 0; k < num_cols_sc; k++) {
            sum = 0.0;
            for (jj = -half_rows_filt; jj <= half_rows_filt; jj++) {
                j_circ = j + jj;
                if (j_circ < 0) {
                    j_circ =
                        (num_rows_sc - ((-j_circ) % num_rows_sc)) % num_rows_sc;
                }
                if (j_circ > (num_rows_sc - 1)) {
                    j_circ = j_circ % num_rows_sc;
                }
                /* In case modulo is of a negative number */
                if (j_circ < 0)
                    j_circ = j_circ + num_rows_sc;
                for (kk = -half_cols_filt; kk <= half_cols_filt; kk++) {
                    k_circ = k + kk;
                    if (k_circ < 0) {
                        k_circ =
                            (num_cols_sc - ((-k_circ) % num_cols_sc)) % num_cols_sc;
                    }
                    if (k_circ > (num_cols_sc - 1)) {
                        k_circ = k_circ % num_cols_sc;
                    }
                    /* In case modulo is of a negative number */
                    if (k_circ < 0)
                        k_circ = k_circ + num_cols_sc;
                    sum += (double) screen_matrix[k_circ + j_circ * num_cols_sc] *
                            filter[ (jj + half_rows_filt) * num_cols_filt + (kk + half_cols_filt)];
                }
            }
            screen_blur[j * num_cols_sc + k] = sum;
            if (sum > fmax_val) {
                fmax_val = sum;
                fmax_pos.x = k;
                fmax_pos.y = j;
            }
            if (sum < fmin_val) {
                fmin_val = sum;
                fmin_pos.x = k;
                fmin_pos.y = j;
            }
        }
    }
    *max_val = fmax_val;
    *min_val = fmin_val;
    *max_pos = fmax_pos;
    *min_pos = fmin_pos;
}

static int
htsc_add_dots(byte *screen_matrix, int num_cols, int num_rows,
              double horiz_dpi, double vert_dpi, double lpi_act,
              unsigned short *pos_x, unsigned short *pos_y,
              int *locate, int num_dots,
              htsc_dither_pos_t *dot_level_position, int level_num,
              int num_dots_add, void *mem)
{
    double xscale = horiz_dpi / vert_dpi;
    double sigma_y = vert_dpi / lpi_act;
    double sigma_x = sigma_y * xscale;
    int sizefiltx, sizefilty;
    double *filter;
    double *screen_blur;
    int white_pos;
    double max_val, min_val;
    htsc_point_t max_pos, min_pos;
    int k,j;
    int dist, curr_dist;
    htsc_dither_pos_t curr_position;

    sizefiltx = ROUND(sigma_x * 4);
    sizefilty = ROUND(sigma_y * 4);
    if ( ((double) sizefiltx / 2.0) == (sizefiltx >> 1)) {
        sizefiltx += 1;
    }
    if ( ((double) sizefilty / 2.0) == (sizefilty >> 1)) {
        sizefilty += 1;
    }
    filter = (double*) ALLOC(mem, sizeof(double) * sizefilty * sizefiltx);
    if (filter == NULL)
        return -1;
    create_2d_gauss_filter(filter, sizefiltx, sizefilty, (double)sizefiltx, (double)sizefilty);
    screen_blur = (double*)ALLOC(mem, sizeof(double) * num_cols * num_rows);
    if (screen_blur == NULL) {
        FREE(mem, filter);
        return -1;
    }

    for (j = 0; j < num_dots_add; j++) {

        /* Perform the blur */
        htsc_apply_filter(screen_matrix, num_cols, num_rows, filter, sizefiltx,
                  sizefilty, screen_blur, &max_val, &max_pos, &min_val, &min_pos);

        /* Find the closest OFF dot to the min position.  */
        white_pos = 0;
        dist = (num_cols) * (num_cols) + (num_rows) * (num_rows);
        for (k = 0; k < num_dots; k++) {
            curr_dist = (pos_y[k] - (int)min_pos.y) * (pos_y[k] - (int)min_pos.y) +
                        (pos_x[k] - (int)min_pos.x) * (pos_x[k] - (int)min_pos.x);
            if (curr_dist < dist &&
                screen_matrix[pos_x[k] + num_cols * pos_y[k]] == 0) {
                white_pos = k;
                dist = curr_dist;
            }
        }

        /* Set this dot to white */
        screen_matrix[pos_x[white_pos] + num_cols * pos_y[white_pos]] = 1;

        /* Update position information */
        curr_position = dot_level_position[level_num];
        curr_position.point[j].x = pos_x[white_pos];
        curr_position.point[j].y = pos_y[white_pos];
        curr_position.locations[j] = locate[white_pos];
    }
    FREE(mem, filter);
    FREE(mem, screen_blur);
    return 0;
}

static int
htsc_init_dot_position(byte *screen_matrix, int num_cols, int num_rows,
                       double horiz_dpi, double vert_dpi, double lpi_act,
                       unsigned short *pos_x, unsigned short *pos_y, int num_dots,
                       htsc_dither_pos_t *dot_level_position, void *mem)
{
    double xscale = horiz_dpi / vert_dpi;
    double sigma_y = vert_dpi / lpi_act;
    double sigma_x = sigma_y * xscale;
    int sizefiltx, sizefilty;
    double *filter;
    bool done = false;
    double *screen_blur;
    int white_pos, black_pos;
    double max_val, min_val;
    htsc_point_t max_pos, min_pos;
    int k;
    int dist, curr_dist;
    bool found_it;

    sizefiltx = ROUND(sigma_x * 4);
    sizefilty = ROUND(sigma_y * 4);
    if ( ((double) sizefiltx / 2.0) == (sizefiltx >> 1)) {
        sizefiltx += 1;
    }
    if ( ((double) sizefilty / 2.0) == (sizefilty >> 1)) {
        sizefilty += 1;
    }
    filter = (double*) ALLOC(mem, sizeof(double) * sizefilty * sizefiltx);
    if (filter == NULL)
        return -1;
    create_2d_gauss_filter(filter, sizefiltx, sizefilty, (double)sizefiltx, (double)sizefilty);
    screen_blur = (double*) ALLOC(mem, sizeof(double) * num_cols * num_rows);
    if (screen_blur == NULL) {
        FREE(mem, filter);
        return -1;
    }
    /* Start moving dots until the whitest and darkest dot are the same */
    while (!done) {
        /* Blur */
        htsc_apply_filter(screen_matrix, num_cols, num_rows, filter, sizefiltx,
                  sizefilty, screen_blur, &max_val, &max_pos, &min_val, &min_pos);
#if RAW_SCREEN_DUMP
        htsc_dump_float_image(screen_blur, num_cols, num_rows, max_val, "blur_one");
#endif
    /* Find the closest on dot to the max position */
        black_pos = 0;
        dist = (pos_y[0] - (int)max_pos.y) * (pos_y[0] - (int)max_pos.y) +
               (pos_x[0] - (int)max_pos.x) * (pos_x[0] - (int)max_pos.x);
        for ( k = 1; k < num_dots; k++) {
            curr_dist = (pos_y[k] - (int)max_pos.y) * (pos_y[k] - (int)max_pos.y) +
                        (pos_x[k] - (int)max_pos.x) * (pos_x[k] - (int)max_pos.x);
            if (curr_dist < dist &&
                screen_matrix[pos_x[k] + num_cols * pos_y[k]] == 1) {
                black_pos = k;
                dist = curr_dist;
            }
        }
        /* Set this dot to black */
        screen_matrix[pos_x[black_pos] + num_cols * pos_y[black_pos]] = 0;
        /* Blur again */
        htsc_apply_filter(screen_matrix, num_cols, num_rows, filter, sizefiltx,
                  sizefilty, screen_blur, &max_val, &max_pos, &min_val, &min_pos);
        /* Find the closest OFF dot to the min position. */
        white_pos = 0;
        dist = (pos_y[0] - (int)min_pos.y) * (pos_y[0] - (int)min_pos.y) +
               (pos_x[0] - (int)min_pos.x) * (pos_x[0] - (int)min_pos.x);
        for ( k = 1; k < num_dots; k++) {
            curr_dist = (pos_y[k] - (int)min_pos.y) * (pos_y[k] - (int)min_pos.y) +
                        (pos_x[k] - (int)min_pos.x) * (pos_x[k] - (int)min_pos.x);
            if (curr_dist < dist &&
                screen_matrix[pos_x[k] + num_cols * pos_y[k]] == 0) {
                white_pos = k;
                dist = curr_dist;
            }
        }
        /* Set this dot to white */
        screen_matrix[pos_x[white_pos] + num_cols * pos_y[white_pos]] = 1;
        /* If it is the same dot as before, then we are done */
        /* There could be a danger here of cycles longer than 2 */
        if (white_pos == black_pos) {
            done = true;
            FREE(mem, screen_blur);
            FREE(mem, filter);
            return 0;
        } else {
            /* We need to update our dot position information */
            /* find where the old white one was and replace it */
            found_it = false;
            for (k = 0; k < dot_level_position->number_points; k++) {
                if (dot_level_position->point[k].x == pos_x[black_pos] &&
                    dot_level_position->point[k].y == pos_y[black_pos]) {
                    found_it = true;
                    dot_level_position->point[k].x = pos_x[white_pos];
                    dot_level_position->point[k].y = pos_y[white_pos];
                    dot_level_position->locations[k] =
                        pos_x[white_pos] + pos_y[white_pos] * num_cols;
                    break;
                }
            }
            if (!found_it) {
                EPRINTF(mem, "ERROR! bug in dot location accounting\n");
                FREE(mem, filter);
                FREE(mem, screen_blur);
                return -1;
            }
        }
    }
    FREE(mem, filter);
    FREE(mem, screen_blur);
    return 0;
}

static int
htsc_create_dither_mask(htsc_dig_grid_t super_cell, htsc_dig_grid_t *final_mask,
                          int verbose, int num_levels, int y, int x, double vert_dpi,
                          double horiz_dpi, int N, double gamma,
                          htsc_dig_grid_t dot_grid, htsc_point_t dot_grid_one_index)
{
    int *dot_levels = NULL;
    int *locate = NULL;
    double percent, perc_val;
    int code = 0, k, j, h, jj, mm;
    int width_supercell = super_cell.width;
    int height_supercell = super_cell.height;
    int number_points = width_supercell * height_supercell;
    int num_dots = 0;
    double step_size;
    int curr_size;
    int rand_pos, dots_needed;
    int done;
    double lpi_act;
    double vert_scale = ((double) y / vert_dpi);
    double horiz_scale = ((double) x / horiz_dpi);
    byte *screen_matrix = NULL;
    unsigned short *pos_x = NULL, *pos_y = NULL;
    htsc_dither_pos_t *dot_level_pos = NULL, *curr_dot_level = NULL;
    int count;
    int prev_dot_level, curr_num_dots;
    double mag_offset, temp_dbl;
    int *thresholds = NULL, val;
    int j_index, k_index, threshold_value;
    int *dot_level_sort = NULL;
    bool found;

    lpi_act = 1.0/((double) sqrt( vert_scale * vert_scale +
                                  horiz_scale * horiz_scale));
    if (num_levels > 1) {
        curr_size = 2 * MAX(height_supercell, width_supercell);
        locate = (int*) ALLOC(dot_grid.memory, sizeof(int) * curr_size);
        if (locate == NULL) {
            code = -1;
            goto out;
        }
        screen_matrix = (byte*) ALLOC(dot_grid.memory, sizeof(byte) * number_points);
        if (screen_matrix == NULL) {
            code = -1;
            goto out;
        }
        memset(screen_matrix, 0, sizeof(byte) * number_points);

        /* Determine the number of dots in the screen and their index */
        for (j = 0; j < number_points; j++) {
            if (super_cell.data[j] == 1) {
                locate[num_dots] = j;
                num_dots++;
                if (num_dots == (curr_size - 1)) {
                    int *tmp = locate;

                    curr_size = curr_size * 2;
                    locate = (int*) ALLOC(dot_grid.memory, sizeof(int) * curr_size);
                    if (locate == NULL) {
                        code = -1;
                        goto out;
                    }
                    memcpy(locate, tmp, sizeof(int) * (num_dots+1));
                    FREE(dot_grid.memory, tmp);
                }
            }
        }

       /* Convert the 1-D locate positions to 2-D positions so that we can
          use a distance metric to the dot center locations. Also allocate
          the structure for our dot positioning information */
        pos_x = (unsigned short*) ALLOC(dot_grid.memory, sizeof(unsigned short) * num_dots);
        if (pos_x == NULL) {
            code = -1;
            goto out;
        }
        pos_y = (unsigned short*) ALLOC(dot_grid.memory, sizeof(unsigned short) * num_dots);
        if (pos_y == NULL) {
            code = -1;
            goto out;
        }
        for (k = 0; k < num_dots; k++) {
            pos_x[k] = locate[k] % width_supercell;
            pos_y[k] = (locate[k] - pos_x[k]) / width_supercell;
        }

        /* Note that number of quantization levels is not tied to number of dots
           in the macro screen.  */
        dot_level_pos =
            (htsc_dither_pos_t*) ALLOC(dot_grid.memory, sizeof(htsc_dither_pos_t) * num_levels);
        if (dot_level_pos == NULL) {
            code = -1;
            goto out;
        }
        dot_levels = (int*) ALLOC(dot_grid.memory, sizeof(int) * num_levels);
        if (dot_levels == NULL) {
            code = -1;
            goto out;
        }
        percent = 1.0 / ((double)num_levels + 1.0);
        prev_dot_level = 0;
        for (k = 0; k < num_levels; k++) {
            perc_val = (k + 1) * percent;
            dot_levels[k] = ROUND(num_dots * perc_val);
            curr_num_dots = dot_levels[k] -prev_dot_level;
            prev_dot_level = dot_levels[k];
            dot_level_pos[k].locations = (int*) ALLOC(dot_grid.memory, sizeof(int) * curr_num_dots);
            if (dot_level_pos[k].locations == NULL) {
                code = -1;
                goto out;
            }
            dot_level_pos[k].point =
                (htsc_point_t*) ALLOC(dot_grid.memory, sizeof(htsc_point_t) * curr_num_dots);
            if (dot_level_pos[k].point == NULL) {
                code = -1;
                goto out;
            }
            dot_level_pos[k].number_points = curr_num_dots;
        }

        /* An initial random location for the first level  */
        dots_needed = dot_levels[0];
        count = 0;
        if (dots_needed > 0) {
            done = 0;
            while (!done) {
                rand_pos = ROUND((num_dots-1) * (double) rand() / (double) RAND_MAX);
                if (screen_matrix[locate[rand_pos]] != 1) {
                    screen_matrix[locate[rand_pos]] = 1;
                    dot_level_pos->locations[count] = locate[rand_pos];
                    dot_level_pos->point[count].x = pos_x[rand_pos];
                    dot_level_pos->point[count].y = pos_y[rand_pos];
                    dots_needed--;
                    count++;
                }
                if (dots_needed == 0) {
                    done = 1;
                }
            }
        }
#if RAW_SCREEN_DUMP
        htsc_dump_byte_image(screen_matrix, width_supercell, height_supercell,
                             1, "screen0_init");
#endif
        /* Rearrange these dots into a pleasing pattern, but only if there is
        * more than 1.  Otherwise there are none to move */
        if (dot_levels[0] > 1)
            code = htsc_init_dot_position(screen_matrix, width_supercell,
                               height_supercell, horiz_dpi, vert_dpi, lpi_act,
                               pos_x, pos_y, num_dots, dot_level_pos, final_mask->memory);
        if (code < 0)
            goto out;
#if RAW_SCREEN_DUMP
        htsc_dump_byte_image(screen_matrix, width_supercell, height_supercell,
                             1, "screen0_arrange");
#endif
        /* Now we want to introduce more dots at each level */
        for (k = 1; k < num_levels; k++) {
            code = htsc_add_dots(screen_matrix, width_supercell, height_supercell,
                          horiz_dpi, vert_dpi, lpi_act, pos_x, pos_y, locate,
                          num_dots, dot_level_pos, k,
                          dot_level_pos[k].number_points, final_mask->memory);
        if (code < 0)
            goto out;
#if RAW_SCREEN_DUMP
            {
            char str_name[30];
            snprintf(str_name, 30, "screen%d_arrange",k);
            htsc_dump_byte_image(screen_matrix, width_supercell, height_supercell,
                                 1, str_name);
            }
#endif
        }

        if (verbose > 0)
            PRINTF(final_mask->memory, "\n--Dot Positions--\n");
        for (k = 0; k < num_levels; k++) {
            if (verbose > 0)
                PRINTF2(final_mask->memory, "dot_level_pos %d: number_points = %d\n",
                        k, dot_level_pos[k].number_points);
            for (j = 0; j < dot_level_pos[k].number_points; j++) {
                if (verbose > 0)
                    PRINTF4(final_mask->memory, "\tpoint: %d: locations = %d x = %3.2lf y = %3.2lf\n",
                           j, dot_level_pos[k].locations[j], dot_level_pos[k].point[j].x, dot_level_pos[k].point[j].y);
            }
        }

        /* Create the threshold mask. */
        step_size = (MAXVAL + 1.0) / (double) N;
        thresholds = (int*) ALLOC(dot_grid.memory, sizeof(int) * N);
        if (thresholds == NULL) {
            code = -1;
            goto out;
        }
        for (k = 0; k < N; k++) {
            thresholds[N-1-k] = (int)((k + 1) * step_size - (step_size / 2));
        }
        mag_offset =
            (double) (thresholds[0]-thresholds[1]) / (double) (num_levels+1);
        if ( gamma != 1.0) {
            for (k = 0; k < N; k++) {
                temp_dbl =
                    (double) pow((double) thresholds[k] / MAXVAL,
                                 (double) gamma);
                thresholds[k] = ROUND(temp_dbl * MAXVAL);
            }
        }

        /* Now use the indices from the large screen to look up the mask and
           apply the offset to the threshold values to dither the rate at which
           the dots turn on */
        /* allocate the mask */
        final_mask->height = height_supercell;
        final_mask->width = width_supercell;
        final_mask->data =
            (int*) ALLOC(dot_grid.memory, sizeof(int) * height_supercell * width_supercell);
        if (final_mask->data == NULL) {
            code = -1;
            goto out;
        }

        /* We need to associate the locate index with a particular level
           for the when the dot begins to turn on.  Go through the dot_level_pos
           array to get the values.  Probably should create this earlier and avoid
           this */
        dot_level_sort = (int*) ALLOC(dot_grid.memory, sizeof(int) * num_dots);
        if (dot_level_sort == NULL) {
            code = -1;
            goto out;
        }
        for (h = 0; h < num_dots; h++) {
            found = false;
            for (jj = 0; jj < num_levels; jj++) {
                curr_dot_level = &(dot_level_pos[jj]);
                for (mm = 0; mm < curr_dot_level->number_points; mm++) {
                    if (pos_x[h] == curr_dot_level->point[mm].x &&
                        pos_y[h] == curr_dot_level->point[mm].y ) {
                        found = true;
                        dot_level_sort[h] = jj + 1;
                        break;  /* Break from position search(within level) */
                    }
                }
                if (found == true) { /* break from level search */
                    break;
                }
            }
            if (found == false) {
                dot_level_sort[h] = 0;
            }
        }

        for (h = 0; h < num_dots; h++) {
            for (j = 0; j < dot_grid.height; j++) {
                for (k = 0; k < dot_grid.width; k++) {
                    val = htsc_getpoint(&dot_grid, k, j);
                    if (val != 0) {

                        /* Assign a offset threshold values */
                        j_index =
                            (pos_y[h] + j - (int) dot_grid_one_index.y) % height_supercell;

                        /* In case we have modulo of a negative number */
                        if (j_index < 0) j_index = j_index + height_supercell;
                        k_index =
                            (pos_x[h] + k - (int) dot_grid_one_index.x) % width_supercell;

                        /* In case we have modulo of a negative number */
                        if (k_index < 0) k_index = k_index + width_supercell;
                        threshold_value = (int)(thresholds[val-1] +
                                                mag_offset * dot_level_sort[h]);
                        if (threshold_value > MAXVAL) threshold_value = (int)MAXVAL;
                        if (threshold_value < 0) threshold_value = 0;
                        htsc_setpoint(final_mask,k_index,j_index,threshold_value);
                    }
                }
            }
        }
out:
        if (dot_level_pos) {
            for (k = 0; k < num_levels; k++) {
                FREE(dot_grid.memory, dot_level_pos[k].locations);
                FREE(dot_grid.memory, dot_level_pos[k].point);
            }
        }
        FREE(dot_grid.memory, locate);
        FREE(dot_grid.memory, screen_matrix);
        FREE(dot_grid.memory, pos_x);
        FREE(dot_grid.memory, pos_y);
        FREE(dot_grid.memory, dot_level_pos);
        FREE(dot_grid.memory, dot_levels);
        FREE(dot_grid.memory, thresholds);
        FREE(dot_grid.memory, dot_level_sort);
    }
    return code;
}

static int
htsc_create_holladay_mask(htsc_dig_grid_t super_cell, int H, int L,
                          double gamma, htsc_dig_grid_t *final_mask)
{
    double step_size = (MAXVAL + 1.0) /( (double) H * (double) L);
    int k, j, code = 0;
    double *thresholds = NULL;
    int number_points = H * L;
    double half_step = step_size / 2.0;
    double temp;
    int index_point;
    int value;
    double white_scale = 253.0 / 255.0; /* To ensure white is white */

    final_mask->height = H;
    final_mask->width = L;
    final_mask->data = (int *) ALLOC(final_mask->memory, H * L * sizeof(int));
    if (final_mask->data == NULL) {
        code = -1;
        goto out;
    }

    thresholds = (double *) ALLOC(final_mask->memory, H * L * sizeof(double));
    if (final_mask->data == NULL) {
        code = -1;
        goto out;
    }
    for (k = 0; k < number_points; k++) {
         temp = ((k+1) * step_size - half_step) / MAXVAL;
         if ( gamma != 1.0) {
             /* Possible linearization */
            temp = (double) pow((double) temp, (double) gamma);
         }
         thresholds[number_points - k - 1] =
                                    ROUND(temp * MAXVAL * white_scale + 1);
    }
    memset(final_mask->data, 0, H * L * sizeof(int));

    for (j = 0; j < H; j++) {
        for (k = 0; k < L; k++) {
            index_point = htsc_getpoint(&super_cell,k,j) - 1;
            value = (int) floor(thresholds[index_point]);
            htsc_setpoint(final_mask,k,j,value);
        }
    }
out:
    FREE(final_mask->memory, thresholds);
    return code;
}

static int
htsc_create_nondithered_mask(htsc_dig_grid_t super_cell, int H, int L,
                          double gamma, htsc_dig_grid_t *final_mask)
{
    double step_size = (MAXVAL +  1) /( (double) H * (double) L);
    int k, j, code = 0;
    double *thresholds = NULL;
    int number_points = H * L;
    double half_step = step_size / 2.0;
    double temp;
    int index_point;
    int value;
    double white_scale = 253.0 / 255.0; /* To ensure white is white */

    final_mask->height = super_cell.height;
    final_mask->width = super_cell.width;
    final_mask->data = (int *) ALLOC(final_mask->memory, super_cell.height * super_cell.width *
                                      sizeof(int));
    if (final_mask->data == NULL) {
        code = -1;
        goto out;
    }
    thresholds = (double *) ALLOC(final_mask->memory, H * L * sizeof(double));
    if (thresholds == NULL) {
        code = -1;
        goto out;
    }
    for (k = 0; k < number_points; k++) {
         temp = ((k+1) * step_size - half_step) / MAXVAL;
         if ( gamma != 1.0) {
             /* Possible linearization */
            temp = (double) pow((double) temp, (double) gamma);
         }
         thresholds[number_points - k - 1] =
                                    ROUND(temp * MAXVAL * white_scale + 1);
    }
    memset(final_mask->data, 0, super_cell.height * super_cell.width *
                                sizeof(int));
    for (j = 0; j < super_cell.height; j++) {
        for (k = 0; k < super_cell.width; k++) {
            index_point = htsc_getpoint(&super_cell,k,j) - 1;
            value = (int) floor(thresholds[index_point]);
            htsc_setpoint(final_mask,k,j,value);
        }
    }
out:
    FREE(final_mask->memory, thresholds);
    return code;
}

/* Various spot functions */
static double
htsc_spot_circle(double x, double y)
{
    return 1.0 - (x*x + y*y);
}

static double
htsc_spot_redbook(double x, double y)
{
    return (180.0 * (double) cos(x) + 180.0 * (double) cos(y)) / 2.0;
}

static double
htsc_spot_inverted_round(double x, double y)
{
    return (x*x + y*y) - 1.0;
}

static double
htsc_spot_rhomboid(double x, double y)
{
    return 1.0 - ((double) fabs(y) * 0.8 + (double) fabs(x)) / 2.0;
}

static double
htsc_spot_linex(double x, double y)
{
    return 1.0 - (double) fabs(y);
}

static double
htsc_spot_liney(double x, double y)
{
    return 1.0 - (double) fabs(x);
}

static double
htsc_spot_diamond(double x, double y)
{
    double abs_y = (double) fabs(y);
    double abs_x = (double) fabs(x);

    if ((abs_y + abs_x) <= 0.75) {
        return 1.0 - (abs_x * abs_x + abs_y * abs_y);
    } else {
        if ((abs_y + abs_x) <= 1.23) {
            return 1.0 - (0.76  * abs_y + abs_x);
        } else {
            return ((abs_x - 1.0) * (abs_x - 1.0) +
                    (abs_y - 1.0) * (abs_y - 1.0)) - 1.0;
        }
    }
}

static double
htsc_spot_diamond2(double x, double y)
{
    double xy = (double) fabs(x) + (double) fabs(y);

    if (xy <= 1.0) {
        return 1.0 - xy * xy / 2.0;
    } else {
        return 1.0 - (2.0 * xy * xy - 4.0 * (xy - 1.0) * (xy - 1.0)) / 4.0;
    }
}

static double
htsc_spot_roundspot(double x, double y)
{
    double xy = (double)fabs(x) + (double)fabs(y);

    if (xy <= 1.0) {
        return 1.0 - (x*x + y*y);
    } else {
        return ((fabs(x) - 1.0) * (fabs(x) - 1.0)) + ((fabs(y) - 1.0) * (fabs(y) - 1.0)) - 1.0;
    }
}

static double
htsc_spot_value(spottype_t spot_type, double x, double y)
{
    switch (spot_type) {
        case CIRCLE:
            return htsc_spot_circle(x,y);
        case REDBOOK:
            return htsc_spot_redbook(x,y);
        case INVERTED:
            return htsc_spot_inverted_round(x,y);
        case RHOMBOID:
            return htsc_spot_rhomboid(x,y);
        case LINE_X:
            return htsc_spot_linex(x,y);
        case LINE_Y:
            return htsc_spot_liney(x,y);
        case DIAMOND1:
            return htsc_spot_diamond(x,y);
        case DIAMOND2:
            return htsc_spot_diamond2(x,y);
        case ROUNDSPOT:
            return htsc_spot_roundspot(x,y);
        case CUSTOM:  /* A spot (pun intended) for users to define their own */
            return htsc_spot_circle(x,y);
        default:
            return htsc_spot_circle(x,y);
    }
}

#if RAW_SCREEN_DUMP
void
static
htsc_dump_screen(htsc_dig_grid_t *dig_grid, char filename[])
{
    char full_file_name[FULL_FILE_NAME_LENGTH];
    FILE *fid;
    int x,y;
    int *buff_ptr = dig_grid->data;
    int width = dig_grid->width;
    int height = dig_grid->height;
    byte data[3];

    snprintf(full_file_name, FULL_FILE_NAME_LENGTH, "Screen_%s_%dx%dx3.raw",filename,width,height);
    fid = fopen(full_file_name,"wb");

    for (y = 0; y < height; y++) {
        for ( x = 0; x < width; x++ ) {
            if (*buff_ptr < 0) {
                data[0] = 255;
                data[1] = 0;
                data[2] = 0;
            } else if (*buff_ptr > 255) {
                data[0] = 0;
                data[1] = 255;
                data[2] = 0;
            } else {
                data[0] = *buff_ptr;
                data[1] = *buff_ptr;
                data[2] = *buff_ptr;
            }
            fwrite(data,sizeof(unsigned char),3,fid);
            buff_ptr++;
        }
    }
    fclose(fid);
}

static void
htsc_dump_float_image(double *image, int height, int width, double max_val,
                      char filename[])
{
    char full_file_name[FULL_FILE_NAME_LENGTH];
    FILE *fid;
    int x,y;
    int data;
    byte data_byte;

    snprintf(full_file_name, FULL_FILE_NAME_LENGTH, "Float_%s_%dx%d.raw",filename,width,height);
    fid = fopen(full_file_name,"wb");
    for (y = 0; y < height; y++) {
        for ( x = 0; x < width; x++ ) {
            data = (255.0 * image[x + y * width] / max_val);
            if (data > 255) data = 255;
            if (data < 0) data = 0;
            data_byte = data;
            fwrite(&data_byte,sizeof(byte),1,fid);
        }
    }
    fclose(fid);
}

static void
htsc_dump_byte_image(byte *image, int height, int width, double max_val,
                      char filename[])
{
    char full_file_name[FULL_FILE_NAME_LENGTH];
    FILE *fid;
    int x,y;
    int data;
    byte data_byte;

    snprintf(full_file_name, FULL_FILE_NAME_LENGTH, "ByteScaled_%s_%dx%d.raw",filename,width,height);
    fid = fopen(full_file_name,"wb");
    for (y = 0; y < height; y++) {
        for ( x = 0; x < width; x++ ) {
            data = (255.0 * image[x + y * width] / max_val);
            if (data > 255) data = 255;
            if (data < 0) data = 0;
            data_byte = data;
            fwrite(&data_byte,sizeof(byte),1,fid);
        }
    }
    fclose(fid);
}
#endif
