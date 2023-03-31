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

/* $Id: $ */
/*
    thresh_remap [-c] [-p] [-q] [-o outputfile] threshold.tos [transfer.dat][> threshold.bin]

    threshold.tos is the file produced by either gen_stochastic or
        gen_ordered consisting of a header line that has the width and
        height (format "# W=%d H=%d" followed by a list of pixels in
        order of painting, one line per pixel format: %d %d

    transfer.dat is optional. If missing, the thresold array will have
    1/256 of the total at each level. The format of the transfer.dat
    file is: %d %f     where the first integer is the level and the
    second float is the brightness (0 == dark, similar to L* value).
    The 'level' is in the range 0 to 256. The final entry should be
    256 WHITEVALUE. If the final measurement is not level 256, then
    the assumed WHITEVALUE will be 1.0000

    -c outputs the count of pixels at each of 256 levels
    -p generates output in PostScript HalftoneType 3 format
    -q suppresses output (except -c)
    -o specifies the file for the binary or PostScript remapped threshold array
*/

#include <stdio.h>
#include <math.h>

#define MAX_WIDTH 256
#define MAX_HEIGHT 256

int
main( int argc, char *argv[]) {

    FILE *tfile, *ofile = stdout, *lfile = NULL;
    int code, optargc, i, level, width, height, num_pix, num_lin, cur_pix;
    int counts=0, quiet=0;
    int thresh_order [MAX_WIDTH*MAX_HEIGHT][2], cur_level, lin_levels[257];
    double cur_value, lin_values[257], delta_value;
    unsigned char thresh[MAX_WIDTH*MAX_HEIGHT];
    char *ofilename, format = 'b';
    int pixcounts[256];

    if (argc < 2)
        goto usage;
    /* process options */
    optargc = 1;
    while (argv[optargc][0] == '-') {
        switch (argv[optargc][1]) {
          case 'c':
            counts = 1;
            break;
          case 'o':
            if (argv[optargc][2] == 0)
                ofilename = argv[++optargc];
            else
                ofilename = &argv[optargc][2];
                   ofile = fopen(ofilename, "w");
            if (ofile == NULL) {
                fprintf(stderr, "\nUnable to open outputfile: '%s'\n", ofilename);
                exit(1);
            }
            break;
          case 'p':
            format = 'p';
            break;
          case 'q':
            quiet = 1;
          default:
            break;
        }
        optargc++;
    }
    if (argc-optargc < 1)
        goto usage;

    /* read the threshold array */
    tfile = fopen(argv[optargc],"r");
    if (tfile == NULL) {
        fprintf(stderr, "\n    Threshold data file not found: '%s'\n", argv[optargc]);
        exit(1);
    }
    /* Read the header to get the width and height */
    i = fscanf(tfile, "# W=%d H=%d", &width, &height);
    if (i < 2) {
        printf("Missing header line. Should be: # W=### H=###\n");
        exit(1);
    }

    for (i=0; i<width*height; i++) {
        code = fscanf(tfile,"%d %d", &thresh_order[i][0], &thresh_order[i][1]);
        if (code < 2) break;
    }
    num_pix = i;

    if (argc-optargc < 2) {
        num_lin = 1;
        lin_levels[0] = 256;
        lin_values[0] = 1.0;
    } else {
        lfile = fopen( argv[optargc+1], "r");
        if (lfile == NULL) {
            fprintf(stderr, "\n    Linearization file not found: '%s'\n", argv[optargc+1]);
            exit(1);
        }
        num_lin = 0;
        while ((i = fscanf(lfile, "%d %f", &lin_levels[num_lin], &lin_values[num_lin])) == 2)
            num_lin++;
        if (lin_levels[num_lin] != 256) {
            lin_levels[num_lin] = 256;
            lin_values[num_lin++] = 1.0;		/* assumption, could extrapolate instead */
            fprintf(stderr, "\ntransfer function data did not include final level 256 value\n");
        }
    }

    if (quiet == 0) {
        fprintf(stderr, "mapping '%s' from %d levels to 256 levels.\n"
            "number of mapped data points is %d.\n",
            argv[optargc], num_pix, num_lin);
    }

    cur_level = 0;
    cur_value = 0.0;
    cur_pix = 0;
    cur_value = 0.0;
    delta_value = lin_values[num_lin-1] / (double)(num_pix);
    for (i=0; i<256; i++)
        pixcounts[i] = 0;

    for (i=0; i<num_lin; i++) {
        int range_levels, base_level = cur_level;
        float base_value = cur_value;
        int next_level = lin_levels[i];
        float next_value = lin_values[i];
        float end_value, range_values;

        range_values = next_value - cur_value;
        range_levels = next_level - cur_level;
        if (range_levels == 0)
            continue;
        /* emit the levels from cur_level to next_level */
        for (level=cur_level; level<next_level; level++) {
            end_value = base_value + range_values * (float)(1+level-base_level) / (float)range_levels;
            if (end_value > next_value)
                end_value = next_value;		/* clamp in case of rounding errors */
            while (end_value - cur_value > 0.00001) {
                thresh[thresh_order[cur_pix][0] + (width*thresh_order[cur_pix][1])] = level;
                pixcounts[level]++;
                cur_pix++;
                if (cur_pix >= num_pix)
                    break;
                cur_value += delta_value;
            }
            if (cur_pix >= num_pix)
                break;
        }
        cur_level = next_level;
    }
    /* now fill any remaining cells */
    for (; cur_pix < num_pix; cur_pix++) {
        thresh[thresh_order[cur_pix][0] + (width*thresh_order[cur_pix][1])] = 255;
        pixcounts[255]++;
    }
    /* Now put out the mapped threshold order */
    if (format == 'p') {
      /* PostScript HalftoneType 3 Threshold Array for testing */
      if (num_pix >= 65536) {
            fprintf(stderr, "\nThreshold array too large for PostScript format HalftoneType 3.\n");
            exit(1);
        }
        fprintf(ofile,"%%!PS\n%% Created from '%s'\n",argv[optargc]);
        fprintf(ofile, "<< /HalftoneType 3\n   /Width %d\n   /Height %d\n   /Thresholds <",
            width, height);
        for (i=0; i<num_pix; i++)
            fprintf(ofile, ((i % 32) == 0) ? "\n     %02x " : "%02x ", 255 - thresh[i]);
        fprintf(ofile, ">\n>>\n");
        fprintf(ofile,"/Default exch /Halftone defineresource sethalftone\n{ } settransfer\n");
        fprintf(ofile,"%%%%EOF\n");
    } else {
        /* Just dump the binary data (suitable for Scanvec */
        fprintf(ofile, "%c%c", width/256, width % 256);
        for (i=0; i<num_pix; i++)
            fprintf(ofile, "%c", thresh[i]);
    }
    fclose(ofile);
    if (counts) {
        for (i=0; i<256; i++)
            printf("%03d: %d\n", i, pixcounts[i]);
    }

    return 0;

usage:
    fprintf(stderr,
        "usage: "
        "thresh_remap [-c] [-p] [-q] [-o outputfile] threshold.tos [transfer.dat][> threshold.bin]\n"
        "\n"
        "\t-c outputs the count of pixels at each of 256 levels\n"
        "\t-p generates output in PostScript HalftoneType 3 format\n"
        "\t-q suppresses output (except -c)\n"
        "\t-o specifies the file for the binary or PostScript remapped threshold array\n"
        "\n"
        "\tthreshold.tos is the file produced by either gen_stochastic or\n"
        "\tgen_ordered consisting of a header line that has the width and\n"
        "\theight (format '# W=%%d H=%%d' followed by a list of pixels in\n"
        "\torder of painting, one line per pixel format: %%d %%d\n"

        "\ttransfer.dat is optional. If missing, the thresold array will have\n"
        "\t1/256 of the total at each level. The format of the transfer.dat\n"
        "\tfile is: %%d %%f     where the first integer is the level and the\n"
        "\tsecond float is the brightness (0 == dark, similar to L* value).\n"
        "\tThe 'level' is in the range 0 to 256. The final entry should be\n"
        "\t256 WHITEVALUE. If the final measurement is not level 256, then\n"
        "\tthe assumed WHITEVALUE will be 1.0000\n"
        "\n"
        );
    return(1);
}
