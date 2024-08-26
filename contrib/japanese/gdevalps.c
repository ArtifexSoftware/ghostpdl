/* Copyright (C) 1990, 1995, 1997 Aladdin Enterprises.  All rights reserved.

  This file is part of Aladdin Ghostscript.

  Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
  or distributor accepts any responsibility for the consequences of using it,
  or for whether it serves any particular purpose or works at all, unless he
  or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
  License (the "License") for full details.

  Every copy of Aladdin Ghostscript must include a copy of the License,
  normally in a plain ASCII text file named PUBLIC.  The License grants you
  the right to copy, modify and redistribute Aladdin Ghostscript, but only
  under certain conditions described in the License.  Among other things, the
  License requires that the copyright notice and this notice be preserved on
  all copies.
*/

/*
  This driver supports ALPS MD Series printer 600dpi monochrome mode only.

   Feb.  3 1999   Ver. 0.1      MD5000 monochrome mode support.
   Mar. 19 1999   Ver. 0.2      old MD series monochrome compressed mode
                                support. contributed by Kousuke Ikeda.

  There is no printer command refernces, so the sequence of initializing,
  inc cartridge selecting, paper feeding and many specs are not clear yet.

  ESC 0x2a 0x62 n2 n1 0x59          raster line skip command.
                                    skip (n1 * 0x100 + n2) lines.
  ESC 0x2a 0x62 n2 n1 0x54 s2 s1    raster line print command.
                                    skip (s1 * 0x100 + s2) * 8 dots from
                                    paper side, then (n1 * 0x100 + n2) bytes
                                    of MSB first data streams following.
  ESC 0x2a 0x62 n2 n1 0x57          raster line print command for MD1300.
                                    (applicable to MD1xxx and MD2xxx ?)
                                    (n1 * 0x100 + n2) bytes of MSB first data
                                    streams following,
                                    but the data must be compressed.
*/

/* gdevalps.c */
/* Alps Micro Dry 600dpi monochrome printer driver */
#include "gdevprn.h"

#define MD_TOP_MARGIN		0.47f
#define MD_BOTTOM_MARGIN	0.59f
#define MD_SIDE_MARGIN		0.13f

#define X_DPI 600
#define Y_DPI 600
#define LINE_SIZE ((X_DPI * 84 / 10 + 7) / 8)	/* bytes per line for letter */

static int md50_print_page(gx_device_printer *, gp_file *, const char *, int);
static dev_proc_open_device(md_open);
static dev_proc_print_page(md50m_print_page);
static dev_proc_print_page(md50e_print_page);
static dev_proc_print_page(md1xm_print_page);

static void
md_initialize_device_procs(gx_device *dev)
{
    gdev_prn_initialize_device_procs_mono(dev);

    set_dev_proc(dev, open_device, md_open);
}

gx_device_printer far_data gs_md50Mono_device =
  prn_device(md_initialize_device_procs, "md50Mono",
        DEFAULT_WIDTH_10THS,
        DEFAULT_HEIGHT_10THS,
        600,				/* x_dpi */
        600,				/* y_dpi */
        0, 0, 0, 0,			/* margins */
        1, md50m_print_page);

gx_device_printer far_data gs_md50Eco_device =
  prn_device(md_initialize_device_procs, "md50Eco",
        DEFAULT_WIDTH_10THS,
        DEFAULT_HEIGHT_10THS,
        600,				/* x_dpi */
        600,				/* y_dpi */
        0, 0, 0, 0,			/* margins */
        1, md50e_print_page);

gx_device_printer far_data gs_md1xMono_device =
  prn_device(md_initialize_device_procs, "md1xMono",
        DEFAULT_WIDTH_10THS,
        DEFAULT_HEIGHT_10THS,
        600,				/* x_dpi */
        600,				/* y_dpi */
        0, 0, 0, 0,			/* margins */
        1, md1xm_print_page);

/* Normal black 600 x 600 dpi mode. */
static const char init_50mono[] = {
0x1b, 0x65,
0x1b, 0x25, 0x80, 0x41,
0x1b, 0x1a, 0x00, 0x00, 0x4c,
0x1b, 0x26, 0x6c, 0x01, 0x00, 0x48,
0x1b, 0x26, 0x6c, 0x07, 0x00, 0x4d,
0x1b, 0x26, 0x6c, 0x04, 0x00, 0x41,
0x1b, 0x2a, 0x72, 0x00, 0x55,
0x1b, 0x2a, 0x74, 0x03, 0x52,
0x1b, 0x26, 0x6c, 0xe5, 0x18, 0x50,
0x1b, 0x1a, 0x00, 0x00, 0x41,
0x1b, 0x26, 0x6c, 0x01, 0x00, 0x43, 0x00,
0x1b, 0x1a, 0x00, 0x00, 0x55,
0x1b, 0x2a, 0x72, 0x01, 0x41,
0x1b, 0x2a, 0x62, 0x00, 0x00, 0x4d,
0x1b, 0x1a, 0x00, 0x80, 0x72,
};

/* ECO black 600 x 600 dpi mode. */
/* If you wanto to use ECO black casette, use this sequence for initialize. */
static const char init_50eco[] = {
0x1b, 0x65,
0x1b, 0x25, 0x80, 0x41,
0x1b, 0x1a, 0x00, 0x00, 0x4c,
0x1b, 0x26, 0x6c, 0x01, 0x00, 0x48,
0x1b, 0x26, 0x6c, 0x07, 0x00, 0x4d,
0x1b, 0x26, 0x6c, 0x04, 0x00, 0x41,
0x1b, 0x2a, 0x72, 0x01, 0x55,
0x1b, 0x2a, 0x74, 0x03, 0x52,
0x1b, 0x26, 0x6c, 0xe5, 0x18, 0x50,
0x1b, 0x1a, 0x00, 0x00, 0x41,
0x1b, 0x1a, 0x01, 0x00, 0x43,
0x1b, 0x26, 0x6c, 0x01, 0x00, 0x43, 0x17,
0x1b, 0x1a, 0x00, 0x00, 0x55,
0x1b, 0x2a, 0x72, 0x01, 0x41,
0x1b, 0x2a, 0x62, 0x00, 0x00, 0x4d,
0x1b, 0x1a, 0x16, 0x80, 0x72,
};

/* Mono Black 600x600 mode for MD1300 */
static const char init_md13[] = {
0x1b, 0x65,
0x1b, 0x25, 0x80, 0x41,
0x1b, 0x1a, 0x00, 0x00, 0x4c,
0x1b, 0x26, 0x6c, 0x01, 0x00, 0x48,
0x1b, 0x26, 0x6c, 0x00, 0x00, 0x4d,
0x1b, 0x26, 0x6c, 0x04, 0x00, 0x41,
0x1b, 0x2a, 0x72, 0x00, 0x55,
0x1b, 0x2a, 0x74, 0x03, 0x52,
0x1b, 0x26, 0x6c, 0xe5, 0x18, 0x50,
0x1b, 0x1a, 0x00, 0x00, 0x41,
0x1b, 0x2a, 0x72, 0x00, 0x41,
0x1b, 0x2a, 0x62, 0x02, 0x00, 0x4d,
0x1b, 0x1a, 0x00, 0x00, 0x72,
};

static const char end_md[] = {
0x0c,
0x1b, 0x2a, 0x72, 0x43,
0x1b, 0x25, 0x00, 0x58
};
/* ------ Internal routines ------ */

/* Open the printer, and set the margins. */
static int
md_open(gx_device *pdev)
{
    static const float md_margins[4] =
    {
        MD_SIDE_MARGIN, MD_BOTTOM_MARGIN,
        MD_SIDE_MARGIN, MD_TOP_MARGIN
    };

    if (pdev->HWResolution[0] != 600)
    {
        emprintf(pdev->memory, "device must have an X resolution of 600dpi\n");
        return_error(gs_error_rangecheck);
    }

    gx_device_set_margins(pdev, md_margins, true);
    return gdev_prn_open(pdev);
}

/* MD5000 monochrome mode entrance. */
static int
md50m_print_page(gx_device_printer *pdev, gp_file *prn_stream)
{
  return(md50_print_page(pdev, prn_stream, init_50mono, sizeof(init_50mono)));
}

/* MD5000 Eco mode monochrome mode entrance. */
static int
md50e_print_page(gx_device_printer *pdev, gp_file *prn_stream)
{
  return(md50_print_page(pdev, prn_stream, init_50eco, sizeof(init_50eco)));
}

/* MD5000 monochrome mode print. */
static int
md50_print_page(gx_device_printer *pdev, gp_file *prn_stream,
              const char *init_str, int init_size)
{
  int lnum;
  int line_size = gdev_mem_bytes_per_scan_line((gx_device *)pdev);
  byte *data = (byte *)gs_malloc(pdev->memory->non_gc_memory, 8, line_size, "md50_print_page(data)" );
  int skipping = 0, code = 0;
  int nbyte;
  int nskip;
  int n;

  if (data == NULL) {
      code = gs_note_error(gs_error_VMerror);
      goto cleanup;
  }

    /* Load Paper & Select Inc Cartridge */
  gp_fwrite(init_str, sizeof(char), init_size, prn_stream);
  gp_fflush(prn_stream);

  for ( lnum = 0; lnum <= pdev->height; lnum++ ) {
    byte *end_data = data + line_size;
    byte *start_data = data;

    n = gdev_prn_copy_scan_lines(pdev, lnum,
                             (byte *)data, line_size);
    if (n != 1) {
        code = n;
        goto cleanup;
    }

    /* Remove trailing 0s. */
    while ( end_data > data && end_data[-1] == 0 )
      end_data--;
    /* Count pre print skip octets */
    while ( start_data < end_data && *start_data == 0 )
      start_data++;
    nbyte = end_data - start_data;
    nskip = start_data - data;

    if(nbyte == 0)
      {
        skipping++;
        continue;
      }
    else
      {
        if(skipping)
          {
            gp_fprintf(prn_stream, "%c%c%c%c%c%c", 0x1b, 0x2a, 0x62,
                       skipping & 0xff, (skipping & 0xff00) / 0x100, 0x59);
            skipping = 0;
          }
        gp_fprintf(prn_stream, "%c%c%c%c%c%c%c%c", 0x1b, 0x2a, 0x62,
                   nbyte & 0xff, (nbyte & 0xff00) / 0x100, 0x54,
                   nskip & 0xff, (nskip & 0xff00) / 0x100);
        gp_fwrite(start_data, sizeof(char), nbyte, prn_stream);
      }
  }

  /* Eject Page */
  gp_fwrite(end_md, sizeof(char), sizeof(end_md), prn_stream);
  gp_fflush(prn_stream);

cleanup:
  gs_free(pdev->memory->non_gc_memory, data, 8, line_size, "md50_print_page(data)" );

  return code;
}

/* all? MD series monochrome mode print with data compression. */
static int
md1xm_print_page(gx_device_printer *pdev, gp_file *prn_stream)
{
  int lnum;
  int line_size = gdev_mem_bytes_per_scan_line((gx_device *)pdev);
  byte *data = (byte *)gs_malloc(pdev->memory->non_gc_memory, 8, line_size, "md1xm_print_page(data)");
  byte *out_start = (byte *)gs_malloc(pdev->memory->non_gc_memory, 8, line_size, "md1xm_print_page(data)");
  int skipping = 0, code = 0;
  int nbyte;

  if (data == NULL || out_start == NULL) {
      code = gs_note_error(gs_error_VMerror);
      goto cleanup;
  }

  /* Load Paper & Select Inc Cartridge */
  gp_fwrite(&init_md13[0], sizeof(char), sizeof(init_md13), prn_stream);
  gp_fflush(prn_stream);

  for ( lnum = 0; lnum <= pdev->height; lnum++ ) {
    byte *end_data = data + line_size;
    byte *data_p = data;
    byte *out_data = out_start;
    byte *p, *q;
    int count;

    gdev_prn_copy_scan_lines(pdev, lnum, data, line_size);
    /* Remove trailing 0s. */
    while ( end_data > data && end_data[-1] == 0 )
      end_data--;

     nbyte = end_data - data_p;

    if(nbyte == 0)
      {
        skipping++;
        continue;
      }
    else
      {
        if(skipping)
          {
            gp_fprintf(prn_stream, "%c%c%c%c%c%c", 0x1b, 0x2a, 0x62,
                       skipping & 0xff, (skipping & 0xff00) / 0x100, 0x59);
            skipping = 0;
          }

        /* Following codes are borrowed from gdevescp.c */

        for ( p = data_p, q = data_p + 1; q < end_data; ){

          if( *p != *q ) {

            p += 2;
            q += 2;

          } else {
            /*
            ** Check behind us, just in case:
            */

            if( p > data_p && *p == *(p-1) )
              p--;

            /*
            ** walk forward, looking for matches:
            */

            for( q++ ; *q == *p && q < end_data ; q++ ) {
              if( (q-p) >= 128 ) {
                if( p > data_p ) {
                  count = p - data_p;
                  while( count > 128 ) {
                    *out_data++ = '\177';
                    memcpy(out_data, data_p, 128);	/* data */
                    data_p += 128;
                    out_data += 128;
                    count -= 128;
                  }
                  *out_data++ = (char) (count - 1); /* count */
                  memcpy(out_data, data_p, count);	/* data */
                  out_data += count;
                }
                *out_data++ = '\201';	/* Repeat 128 times */
                *out_data++ = *p;
                p += 128;
                data_p = p;
              }
            }

            if( (q - p) > 2 ) {	/* output this sequence */
              if( p > data_p ) {
                count = p - data_p;
                while( count > 128 ) {
                  *out_data++ = '\177';
                  memcpy(out_data, data_p, 128);	/* data */
                  data_p += 128;
                  out_data += 128;
                  count -= 128;
                }
                *out_data++ = (char) (count - 1);	/* byte count */
                memcpy(out_data, data_p, count);	/* data */
                out_data += count;
              }
              count = q - p;
              *out_data++ = (char) (256 - count + 1);
              *out_data++ = *p;
              p += count;
              data_p = p;
            } else	/* add to non-repeating data list */
              p = q;
            if( q < end_data )
              q++;
          }
        }
        /*
        ** copy remaining part of line:
        */

        if( data_p < end_data ) {

          count = end_data - data_p;

          /*
          ** If we've had a long run of varying data followed by a
          ** sequence of repeated data and then hit the end of line,
          ** it's possible to get data counts > 128.
          */

          while( count > 128 ) {
            *out_data++ = '\177';
            memcpy(out_data, data_p, 128);	/* data */
            data_p += 128;
            out_data += 128;
            count -= 128;
          }

          *out_data++ = (char) (count - 1);	/* byte count */
          memcpy(out_data, data_p, count);	/* data */
          out_data += count;
        }

        nbyte = out_data - out_start;

        gp_fprintf(prn_stream, "%c%c%c%c%c%c", 0x1b, 0x2a, 0x62,
                   nbyte & 0xff, (nbyte & 0xff00) / 0x100, 0x57);
        gp_fwrite(out_start, sizeof(char), nbyte, prn_stream);
      }
  }

  /* Eject Page */
  gp_fwrite(end_md, sizeof(char), sizeof(end_md), prn_stream);
  gp_fflush(prn_stream);

cleanup:
  gs_free(pdev->memory->non_gc_memory, data, 8, line_size, "md1xm_print_page(data)");
  gs_free(pdev->memory->non_gc_memory, out_start, 8, line_size, "md1xm_print_page(data)");

  return code;
}
