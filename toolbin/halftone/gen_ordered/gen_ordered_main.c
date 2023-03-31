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
/* Ordered Dither Screen Creation Tool. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <sys/stat.h>
#include <math.h>

typedef unsigned char byte;
#define false 0
#define true 1
#ifndef __cplusplus
    typedef int bool;
#endif	/* __cpluplus */

/* included from base/gen_ordered.h */
#include "gen_ordered.h"

static int htsc_save_tos(htsc_dig_grid_t *final_mask);
static int htsc_save_screen(htsc_dig_grid_t *final_mask, bool use_holladay_grid, int S,
                      htsc_param_t params);

/* -------------------------  _snprintf -----------------------------*/

#if defined(_MSC_VER)
#  if _MSC_VER>=1900 /* VS 2014 and later have (finally) snprintf */
#     define STDC99
#  else /* _MSC_VER >= 1900 */
      /* Microsoft Visual C++ 2005  doesn't properly define snprintf,
         which is defined in the C standard ISO/IEC 9899:1999 (E) */

      #include <stdarg.h>

      int snprintf(char *buffer, size_t count, const char *format, ...)
      {
          if (count > 0) {
              va_list args;
              int n;

              va_start(args, format);
              n = _vsnprintf(buffer, count, format, args);
              buffer[count - 1] = 0;
              va_end(args);
              return n;
          } else
              return 0;
      }
#  endif /* _MSC_VER >= 1900 */
#endif /* defined _MSC_VER */

static int
usage (void)
{
    printf ("Usage: gen_ordered [-a target_angle] [-l target_lpi] [-q target_quantization_levels] \n");
    printf ("                 [-r WxH] [-s size_of_supercell] [-d dot_shape_code] -v verbosity\n");
    printf ("                 [ -f [ps | ppm | raw | raw16 | tos] ]\n");
    printf ("a is the desired angle in degrees for the screen\n");
    printf ("b is the desired bit depth of the output threshold\n");
    printf ("  valid only with -ps or the default raw output\n");
    printf ("dot shape codes are as follows: \n");
    printf ("0  CIRCLE \n");
    printf ("1  REDBOOK CIRCLE \n");
    printf ("2  INVERTED \n");
    printf ("3  RHOMBOID \n");
    printf ("4  LINE_X \n");
    printf ("5  LINE_Y \n");
    printf ("6  DIAMOND1 \n");
    printf ("7  DIAMOND2 \n");
    printf ("8  ROUNDSPOT \n");
    printf ("f [tos | ppm | ps | raw] is the output format\n");
    printf ("     If no output format is indicate a turn on sequence output will be created\n");
    printf ("   ppm is an Portable Pixmap image file\n");
    printf ("   ps indicates postscript style output file\n");
    printf ("   raw is a raw 8-bit binary, row major file\n");
    printf ("   tos indicates to output a turn on sequence which can\n");
    printf ("     be fed into thresh_remap to apply a linearization curve.\n");
    printf ("l is the desired lines per inch (lpi)\n");
    printf ("q is the desired number of quantization (gray) levels\n");
    printf ("r is the device resolution in dots per inch (dpi)\n");
    printf ("  use a single number for r if the resolution is symmetric\n");
    printf ("s is the desired size of the super cell\n");
    printf ("v is the verbosity level. Default is 0 which is error messages only.\n");
    return 1;
}

/* Return the pointer to the value for an argument, either at 'arg' or the next argv */
/* Allows for either -ovalue or -o value option syntax */
/* Returns NULL if there is no value (at end of argc arguments) */
static const char *
get_arg (int argc, char **argv, int *pi, const char *arg)
{
    if (arg[0] != 0) {
        return arg;
    } else {
      (*pi)++;
      if (*pi == argc) {
        return NULL;
      } else {
        return argv[*pi];
      }
    }
}

int
main (int argc, char **argv)
{
    int code, i, j, k, m, S;
    htsc_param_t params;
    htsc_dig_grid_t final_mask;

    htsc_set_default_params(&params);

    for (i = 1; i < argc; i++) {
      const char *arg = argv[i];
      const char *arg_value;

      if (arg[0] == '-') {
          switch (arg[1]) {
            case 'a':
              if ((arg_value = get_arg(argc, argv, &i, arg + 2)) == NULL)
                  goto usage_exit;
              params.targ_scr_ang = atoi(arg_value);
              break;
            case 'f':
              if ((arg_value = get_arg(argc, argv, &i, arg + 2)) == NULL)
                  goto usage_exit;
              switch (arg_value[0]) {
                  case 'p':
                    params.output_format = (arg_value[1] == 's') ?
                                            OUTPUT_PS : OUTPUT_PPM;
                    break;
                  case 'r':
                    j = sscanf(arg_value, "raw%d", &k);
                    if (j == 0 || k == 8)
                        params.output_format = OUTPUT_RAW;
                    else
                        params.output_format = OUTPUT_RAW16;
                    break;
                  case 't':
                    params.output_format = OUTPUT_TOS;
                    break;
                  default:
                    goto usage_exit;
              }
              break;
            case 'd':
              if ((arg_value = get_arg(argc, argv, &i, arg + 2)) == NULL)
                  goto usage_exit;
              j = atoi(arg_value);
              if (j < 0 || j > CUSTOM)
                  params.spot_type = CIRCLE;
              else
                  params.spot_type = j;
              break;
            case 'l':
              if ((arg_value = get_arg(argc, argv, &i, arg + 2)) == NULL)
                  goto usage_exit;
              params.targ_lpi = atoi(arg_value);
              break;
            case 'q':
              if ((arg_value = get_arg(argc, argv, &i, arg + 2)) == NULL)
                  goto usage_exit;
              params.targ_quant = atoi(arg_value);
              params.targ_quant_spec = true;
              break;
            case 'r':
              if ((arg_value = get_arg(argc, argv, &i, arg + 2)) == NULL)
                  goto usage_exit;
              j = sscanf(arg_value, "%dx%d", &k, &m);
              if (j < 1)
                  goto usage_exit;
              params.horiz_dpi = k;
              if (j > 1) {
                  params.vert_dpi = m;
              } else {
                  params.vert_dpi = k;
              }
              break;
            case 's':
              if ((arg_value = get_arg(argc, argv, &i, arg + 2)) == NULL)
                  goto usage_exit;
              params.targ_size = atoi(arg_value);
              params.targ_size_spec = true;
              break;
            case 'v':
              if ((arg_value = get_arg(argc, argv, &i, arg + 2)) == NULL)
                  goto usage_exit;
              params.verbose = atoi(arg_value);
              break;
            default:
usage_exit:   return usage();
            }
        }
    }
    final_mask.memory = NULL;				/* quiet compiler */
    code = htsc_gen_ordered(params, &S, &final_mask);

    if (code >= 0)
        code = htsc_save_screen(&final_mask, params.holladay, S, params);

    if (final_mask.data != NULL)
        free(final_mask.data);

    return code;
}

/* Save turn on order list, Assume that htsc_gen_ordered has already converted to TOS */
static int
htsc_save_tos(htsc_dig_grid_t *final_mask)
{
    int width = final_mask->width;
    int height = final_mask->height;
    int *buff_ptr;
    FILE *fid;
    int k = 0;
    int count= height * width;

    fid = fopen("turn_on_seq.out","w");
    fprintf(fid,"# W=%d H=%d\n",width, height);

    /* Write out */
    buff_ptr = final_mask->data;
    for (k = 0; k < count; k++) {
        fprintf(fid,"%d\t%d\n", *buff_ptr++, *buff_ptr++);
    }
    fclose(fid);
    return 0;
}

static int
htsc_save_screen(htsc_dig_grid_t *final_mask, bool use_holladay_grid, int S,
                htsc_param_t params)
{
    char full_file_name[FULL_FILE_NAME_LENGTH];
    FILE *fid;
    int x,y, code = 0;
    int *buff_ptr = final_mask->data;
    int width = final_mask->width;
    int height = final_mask->height;
    byte data;
    unsigned short data_short;
    output_format_type output_format = params.output_format;
    char *output_extension = (output_format == OUTPUT_PS) ? "ps" :
                        ((output_format == OUTPUT_PPM) ? "ppm" :
                        ((output_format == OUTPUT_RAW16 ? "16.raw" : "raw")));

    if (output_format == OUTPUT_TOS) {
        /* We need to figure out the turn-on sequence from the threshold
           array */
        code = htsc_save_tos(final_mask);
    } else {
        if (use_holladay_grid) {
            snprintf(full_file_name, FULL_FILE_NAME_LENGTH, "Screen_Holladay_Shift%d_%dx%d.%s", S, width,
                    height, output_extension);
        } else {
            snprintf(full_file_name, FULL_FILE_NAME_LENGTH, "Screen_Dithered_%dx%d.%s",width,height,
                    output_extension);
        }
        fid = fopen(full_file_name,"wb");

        if (output_format == OUTPUT_PPM)
            fprintf(fid, "P5\n"
                    "# Halftone threshold array, %s, [%d, %d], S=%d\n"
                    "%d %d\n"
                    "255\n",
                    use_holladay_grid ? "Holladay_Shift" : "Dithered", width, height,
                    S, width, height);
        if (output_format != OUTPUT_PS) {
            /* Both BIN and PPM format write the same binary data */
            if (output_format == OUTPUT_RAW || output_format == OUTPUT_PPM) {
                for (y = 0; y < height; y++) {
                    for ( x = 0; x < width; x++ ) {
                        data_short = (unsigned short) (*buff_ptr & 0xffff);
                        data_short = ROUND((double) data_short * 255.0 / MAXVAL);
                        if (data_short > 255) data_short = 255;
                        data = (byte) (data_short & 0xff);
                        fwrite(&data, sizeof(byte), 1, fid);
                        buff_ptr++;
                    }
                }
            } else {	/* raw16 data */
                for (y = 0; y < height; y++) {
                    for ( x = 0; x < width; x++ ) {
                        data_short = (unsigned short) (*buff_ptr & 0xffff);
                        fwrite(&data_short, sizeof(short), 1, fid);
                        buff_ptr++;
                    }
                }
            }
        } else {	/* ps output format */
            if (params.targ_quant <= 256) {
                /* Output PS HalftoneType 3 dictionary */
                fprintf(fid, "%%!PS\n"
                        "<< /HalftoneType 3\n"
                        "   /Width  %d\n"
                        "   /Height %d\n"
                        "   /Thresholds <\n",
                        width, height);

                for (y = 0; y < height; y++) {
                    for ( x = 0; x < width; x++ ) {
                        data_short = (unsigned short) (*buff_ptr & 0xffff);
                        data_short = ROUND((double) data_short * 255.0 / MAXVAL);
                        if (data_short > 255) data_short = 255;
                        data = (byte) (data_short & 0xff);
                        fprintf(fid, "%02x", data);
                        buff_ptr++;
                        if ((x & 0x1f) == 0x1f && (x != (width - 1)))
                            fprintf(fid, "\n");
                    }
                    fprintf(fid, "\n");
                }
                fprintf(fid, "   >\n"
                        ">>\n"
                        );
            } else {
                /* Output PS HalftoneType 16 dictionary. Single array. */
                fprintf(fid, "%%!PS\n"
                        "%% Create a 'filter' from local hex data\n"
                        "{ currentfile /ASCIIHexDecode filter /ReusableStreamDecode filter } exec\n");
                /* hex data follows, 'file' object will be left on stack */
                for (y = 0; y < height; y++) {
                    for ( x = 0; x < width; x++ ) {
                        data_short = (unsigned short) (*buff_ptr & 0xffff);
                        fprintf(fid, "%04x", data_short);
                        buff_ptr++;
                        if ((x & 0x1f) == 0x1f && (x != (width - 1)))
                            fprintf(fid, "\n");
                    }
                    fprintf(fid, "\n");	/* end of one row */
                }
                fprintf(fid, ">\n");	/* ASCIIHexDecode EOF */
                fprintf(fid,
                        "<< /Thresholds 2 index    %% file object for the 16-bit data\n"
                        "   /HalftoneType 16\n"
                        "   /Width  %d\n"
                        "   /Height %d\n"
                        ">>\n"
                        "exch pop     %% discard ResuableStreamDecode file leaving the Halftone dict.\n",
                        width, height);
            }
        }
        fclose(fid);
    }
    return code;
}
