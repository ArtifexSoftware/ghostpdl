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

/*
  CIF output driver

   The `Fake bitmapped device to estimate rendering time'
   slightly modified to produce CIF files from PostScript.
   So anyone can put a nice logo free on its chip!
   Frederic Petrot, petrot@masi.ibp.fr */

#include "gdevprn.h"

/* Define the device parameters. */
#ifndef X_DPI
#  define X_DPI 72
#endif
#ifndef Y_DPI
#  define Y_DPI 72
#endif

/* The device descriptor */
static dev_proc_print_page(cif_print_page);
/* The print_page proc is compatible with allowing bg printing */
const gx_device_printer far_data gs_cif_device =
  prn_device(gdev_prn_initialize_device_procs_mono_bg, "cif",
        DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
        X_DPI, Y_DPI,
        0,0,0,0,
        1, cif_print_page);

/* Send the page to the output. */
static int
cif_print_page(gx_device_printer *pdev, gp_file *prn_stream)
{	int line_size = gdev_mem_bytes_per_scan_line((gx_device *)pdev);
        int lnum;
        byte *in = (byte *)gs_malloc(pdev->memory, line_size, 1, "cif_print_page(in)");
        char *s;
        const char *fname;
        int scanline, scanbyte;
        int length, start; /* length is the number of successive 1 bits, */
                           /* start is the set of 1 bit start position */
        int code = 0;

        if (in == 0)
                return_error(gs_error_VMerror);

#ifdef CLUSTER
        fname = "clusterout";
#else
        fname = (const char *)(pdev->fname);
#endif
        if ((s = strchr(fname, '.')) == NULL)
                length = strlen(fname) + 1;
        else
                length = s - fname;
        s = (char *)gs_malloc(pdev->memory, length+1, sizeof(char), "cif_print_page(s)");
        if (s == NULL)
            return_error(gs_error_VMerror);
        strncpy(s, fname, length);
        *(s + length) = '\0';
        gp_fprintf(prn_stream, "DS1 25 1;\n9 %s;\nLCP;\n", s);
        gs_free(pdev->memory, s, length+1, 1, "cif_print_page(s)");

   for (lnum = 0; lnum < pdev->height; lnum++) {
      code = gdev_prn_copy_scan_lines(pdev, lnum, in, line_size);
      if (code < 0)
          goto xit;
      length = 0;
      for (scanline = 0; scanline < line_size; scanline++)
#ifdef TILE			/* original, simple, inefficient algorithm */
         for (scanbyte = 0; scanbyte < 8; scanbyte++)
            if (((in[scanline] >> scanbyte) & 1) != 0)
               gp_fprintf(prn_stream, "B4 4 %d %d;\n",
                  (scanline * 8 + (7 - scanbyte)) * 4,
                  (pdev->height - lnum) * 4);
#else				/* better algorithm */
         for (scanbyte = 7; scanbyte >= 0; scanbyte--)
            /* cheap linear reduction of rectangles in lines */
            if (((in[scanline] >> scanbyte) & 1) != 0) {
               if (length == 0)
                  start = (scanline * 8 + (7 - scanbyte));
               length++;
            } else {
               if (length != 0)
                  gp_fprintf(prn_stream, "B%d 4 %d %d;\n", length * 4,
                           start * 4 + length * 2,
                           (pdev->height - lnum) * 4);
               length = 0;
            }
#endif
   }
        gp_fprintf(prn_stream, "DF;\nC1;\nE\n");
xit:
        gs_free(pdev->memory, in, line_size, 1, "cif_print_page(in)");
        return code;
}
