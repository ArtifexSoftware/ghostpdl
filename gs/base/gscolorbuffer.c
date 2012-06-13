/* Copyright (C) 2001-2012 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134, San Rafael,
   CA  94903, U.S.A., +1(415)492-9861, for further information.
*/


/* Simple operators and place holding support for doing conversions on buffers
   of data.  These functions perform device (non CIE or ICC color conversions)
   on buffers of data. Eventually these will be replaced with functions for
   using the ICC based linked mappings with the external CMS. This is far from
   efficient at this point, but gets the job done */

#include "string_.h"
#include "stdpre.h"
#include "gstypes.h"
#include "gsmemory.h"
#include "gxblend.h"
#include "gscolorbuffer.h"

#define float_color_to_byte_color(float_color) ( \
    (0.0 < (float_color) && (float_color) < 1.0) ? \
        ((unsigned char) ((float_color) * 255.0)) : \
        (((float_color) <= 0.0) ? 0x00 : 0xFF) \
    )

/* We could use the conversions that are defined in gxdcconv.c,
   however for now we will use something even easier. This is
   temporary until we get the proper ICC flows in place */

static void
rgb_to_cmyk(byte rgb[],byte cmyk[])
{

    /* Real sleazy min black generation */

    cmyk[0] = 255 - rgb[0];
    cmyk[1] = 255 - rgb[1];
    cmyk[2] = 255 - rgb[2];

    cmyk[3] = (cmyk[0] < cmyk[1]) ?
        min(cmyk[0], cmyk[2]) : min(cmyk[1], cmyk[2]);

}

static void
rgb_to_gray(byte rgb[], byte gray[])
{

    float temp_value;

    /* compute a luminance component */
    temp_value = rgb[0]*0.3 + rgb[1]*0.59 + rgb[2]*0.11;
    temp_value = temp_value * (1.0 / 255.0 );  /* May need to be optimized */
    gray[0] = float_color_to_byte_color(temp_value);

}

static void
cmyk_to_rgb(byte cmyk[], byte rgb[])
{

    /* real ugly, but temporary */

    rgb[0] = 255 - min(cmyk[0] + cmyk[3],255);
    rgb[1] = 255 - min(cmyk[1] + cmyk[3],255);
    rgb[2] = 255 - min(cmyk[2] + cmyk[3],255);

}

static void
cmyk_to_gray(byte cmyk[], byte gray[])
{

    float temp_value;

    temp_value = ((255 - cmyk[0])*0.3 +
                  (255 - cmyk[1])*0.59 +
                  (255 - cmyk[2]) * 0.11) * (255 - cmyk[3]);
    temp_value = temp_value * (1.0 / 65025.0 );

    gray[0] = float_color_to_byte_color(temp_value);

}

static void
gray_to_cmyk(byte gray[], byte cmyk[])
{

    /* Just do black. */
    cmyk[0] = 0;
    cmyk[1] = 0;
    cmyk[2] = 0;
    cmyk[3] = 255 - gray[0];

}

static void
gray_to_rgb(byte gray[], byte rgb[])
{

    rgb[0] = gray[0];
    rgb[1] = gray[0];
    rgb[2] = gray[0];

}

void
gs_transform_color_buffer_generic(byte *inputbuffer,
            int row_stride, int plane_stride,
            int input_num_color, gs_int_rect rect, byte *outputbuffer,
            int output_num_color, int num_noncolor_planes)

{
    int num_rows, num_cols, x, y, z;
    void (* color_remap)(byte input[],byte output[]) = NULL;
    byte input_vector[4],output_vector[4];
    int plane_offset[PDF14_MAX_PLANES],alpha_offset_in,max_num_channels;

    num_rows = rect.q.y - rect.p.y;
    num_cols = rect.q.x - rect.p.x;

    /* Check for spot + cmyk case */

    if (output_num_color > 4)
    {

        /* To CMYK always */
        switch (input_num_color) {

            case 1:
                color_remap = gray_to_cmyk;
                break;

            case 3:
                color_remap = rgb_to_cmyk;
                break;

            case 4:
                color_remap = NULL;        /* Copy data */
                break;

            default:

                /* Should never be here.  Groups must
                   be gray, rgb or CMYK.   Exception
                   may be ICC with XPS */

                break;

        }

    } else {

        /* Pick the mapping to use */

        switch (input_num_color) {

            case 1:
                if (output_num_color == 3)
                    color_remap = gray_to_rgb;
                else
                    color_remap = gray_to_cmyk;
                break;

            case 3:
                if (output_num_color == 1)
                    color_remap = rgb_to_gray;
                else
                    color_remap = rgb_to_cmyk;
                break;

            case 4:
                if (output_num_color == 1)
                    color_remap = cmyk_to_gray;
                else
                    color_remap = cmyk_to_rgb;
                break;

            default:

                /* Should never be here.  Groups must
                   be gray, rgb or CMYK.   Until we
                   have ICC working here with XPS */

                break;

        }

    }

    /* data is planar */

    max_num_channels = max(input_num_color, output_num_color) +
        num_noncolor_planes;

    for (z = 0; z < max_num_channels; z++)
       plane_offset[z] = z * plane_stride;

    if (color_remap == NULL) {

        /* Blending group was CMYK, output is CMYK + spot */

        memcpy(outputbuffer, inputbuffer, 4*plane_stride);

        /* Add any data that are beyond the standard color data (e.g. alpha) */

        if (num_noncolor_planes > 0)
            memcpy(&(outputbuffer[plane_offset[output_num_color]]),
                &(inputbuffer[plane_offset[input_num_color]]),
                num_noncolor_planes*plane_stride);

    } else {

        /* Have to remap */

        alpha_offset_in = input_num_color*plane_stride;

        for (y = 0; y < num_rows; y++) {

           for (x = 0; x < num_cols; x++) {

                /* If the source alpha is transparent, then move on */

                if (inputbuffer[x + alpha_offset_in] != 0x00) {

                    /* grab the input */

                    for (z = 0; z<input_num_color; z++)
                        input_vector[z] = inputbuffer[x+plane_offset[z]];

                    /* convert */

                   color_remap(input_vector,output_vector);

                   /* store the output */

                   for (z = 0; z < output_num_color; z++)
                        outputbuffer[x+plane_offset[z]] = output_vector[z];

                   /* Add any that are beyond the standard color data */

                    for(z = 0; z < num_noncolor_planes; z++)
                        outputbuffer[x + plane_offset[output_num_color+z]] =
                            inputbuffer[x + plane_offset[input_num_color+z]];

                }

            }

            /* update our positions */

            for (z = 0; z < max_num_channels; z++)
                plane_offset[z] += row_stride;

            alpha_offset_in += row_stride;

        }

    }

}
