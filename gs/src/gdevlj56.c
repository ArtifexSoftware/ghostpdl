/* Copyright (C) 1997 Aladdin Enterprises.  All rights reserved.
  
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

/* gdevlj56.c */
/* H-P LaserJet 5 & 6 drivers for Ghostscript */
#include "gdevprn.h"
#include "gdevpcl.h"
#include "gdevpxat.h"
#include "gdevpxen.h"
#include "gdevpxop.h"

/* Define the default resolution. */
#ifndef X_DPI
#  define X_DPI 600
#endif
#ifndef Y_DPI
#  define Y_DPI 600
#endif

/* Define the number of blank lines that make it worthwhile to */
/* start a new image. */
#define MIN_SKIP_LINES 2

/* We round up the LINE_SIZE to a multiple of a ulong for faster scanning. */
#define W sizeof(word)

private dev_proc_open_device(ljet5_open);
private dev_proc_close_device(ljet5_close);
private dev_proc_print_page(ljet5_print_page);

private gx_device_procs ljet5_procs =
  prn_procs(ljet5_open, gdev_prn_output_page, ljet5_close);

gx_device_printer far_data gs_lj5mono_device =
  prn_device(ljet5_procs, "lj5mono",
	DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
	X_DPI, Y_DPI,
	0, 0, 0, 0,
	1, ljet5_print_page);

private gx_device_procs lj5gray_procs =
  prn_color_procs(ljet5_open, gdev_prn_output_page, ljet5_close,
		  gx_default_gray_map_rgb_color,
		  gx_default_gray_map_color_rgb);

gx_device_printer far_data gs_lj5gray_device = {
  prn_device_body(gx_device_printer, lj5gray_procs, "lj5gray",
	DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
	X_DPI, Y_DPI,
	0, 0, 0, 0,
	1, 8, 255, 0, 256, 1, ljet5_print_page)
};

#define ppdev ((gx_device_printer *)pdev)

/* Write a 'canned' data sequence. */
#define fwrite_bytes(bytes, strm) fwrite(bytes, 1, sizeof(bytes), strm)

/* Utilities for writing data values. */
/* H-P printers only support little-endian data, so that's what we emit. */
#define da(a) pxt_attr_ubyte, (a)
private void
put_a(px_attribute_t a, FILE *prn_stream)
{	fputc(pxt_attr_ubyte, prn_stream);
	fputc(a, prn_stream);
}
#define dub(b) pxt_ubyte, (byte)(b)
#define ds(i) (byte)(i), (byte)((i) >> 8)
private void
put_s(uint i, FILE *prn_stream)
{	fputc((byte)i, prn_stream);
	fputc((byte)(i >> 8), prn_stream);
}
#define dus(i) pxt_uint16, ds(i)
private void
put_us(uint i, FILE *prn_stream)
{	fputc(pxt_uint16, prn_stream);
	put_s(i, prn_stream);
}
#define dusp(ix,iy) pxt_uint16_xy, ds(ix), ds(iy)
private void
put_usp(uint ix, uint iy, FILE *prn_stream)
{	fputc(pxt_uint16_xy, prn_stream);
	put_s(ix, prn_stream);
	put_s(iy, prn_stream);
}
#define dss(i) pxt_sint16, ds(i)
private void
put_ss(int i, FILE *prn_stream)
{	fputc(pxt_sint16, prn_stream);
	put_s((uint)i, prn_stream);
}
#define dl(l) ds(l), ds((l) >> 16)
private void
put_l(ulong l, FILE *prn_stream)
{	put_s((uint)l, prn_stream);
	put_s((uint)(l >> 16), prn_stream);
}

/* Open the printer, writing the stream header. */
private int
ljet5_open(gx_device *pdev)
{	int code = gdev_prn_open(pdev);

	if ( code < 0 )
	  return code;
	code = gdev_prn_open_printer(pdev, true);
	if ( code < 0 )
	  return code;
	{ FILE *prn_stream = ppdev->file;
	  static const byte stream_header[] = {
	    da(pxaUnitsPerMeasure),
	    dub(0), da(pxaMeasure),
	    dub(eErrorPage), da(pxaErrorReport),
	    pxtBeginSession,
	    dub(0), da(pxaSourceType),
	    dub(eBinaryLowByteFirst), da(pxaDataOrg),
	    pxtOpenDataSource
	  };

	  fputs("\033%-12345X@PJL ENTER LANGUAGE = PCLXL\n", prn_stream);
	  fputs(") HP-PCL XL;1;1\n", prn_stream);
	  put_usp((uint)(pdev->HWResolution[0] + 0.5),
		  (uint)(pdev->HWResolution[1] + 0.5), prn_stream);
	  fwrite_bytes(stream_header, prn_stream);
	}
	return 0;
}

/* Close the printer, writing the stream trailer. */
private int
ljet5_close(gx_device *pdev)
{	int code = gdev_prn_open_printer(pdev, true);

	if ( code < 0 )
	  return code;
	{ FILE *prn_stream = ppdev->file;
	  static const byte stream_trailer[] = {
	    pxtCloseDataSource,
	    pxtEndSession,
	    033, '%', '-', '1', '2', '3', '4', '5', 'X'
	  };

	  fwrite_bytes(stream_trailer, prn_stream);
	}
	return gdev_prn_close(pdev);
}

/* Send the page to the printer.  For now, just send the whole image. */
private int
ljet5_print_page(gx_device_printer *pdev, FILE *prn_stream)
{	uint line_size = gdev_mem_bytes_per_scan_line((gx_device *)pdev);
	uint line_size_words = (line_size + W - 1) / W;
	uint out_size = line_size + (line_size / 127) + 1;
	word *line = (word *)gs_malloc(line_size_words, W, "ljet5(line)");
	byte *out = gs_malloc(out_size, 1, "ljet5(out)");
	int code = 0;
	int lnum;

	if ( line == 0 || out == 0 )
	  { code = gs_note_error(gs_error_VMerror);
	    goto done;
	  }

	/* Write the page header. */
	{ static const byte page_header[] = {
	    dub(ePortraitOrientation), da(pxaOrientation),
	    dub(eLetterPaper), da(pxaMediaSize),
	    dub(eAutoSelect), da(pxaMediaSource),
	    pxtBeginPage,
	    dusp(0, 0), da(pxaPoint),
	    pxtSetCursor
	  };
	  static const byte mono_header[] = {
	    dub(eGray), da(pxaColorSpace),
	    dub(e8Bit), da(pxaPaletteDepth),
	    pxt_ubyte_array, pxt_ubyte, 2, 0xff, 0x00, da(pxaPaletteData),
	    pxtSetColorSpace
	  };
	  static const byte gray_header[] = {
	    dub(eGray), da(pxaColorSpace),
	    pxtSetColorSpace
	  };

	  fwrite_bytes(page_header, prn_stream);
	  if ( pdev->color_info.depth == 1 )
	    fwrite_bytes(mono_header, prn_stream);
	  else
	    fwrite_bytes(gray_header, prn_stream);
	}

	/* Write the image header. */
	{ static const byte mono_image_header[] = {
	    da(pxaDestinationSize),
	    dub(eIndexedPixel), da(pxaColorMapping),
	    dub(e1Bit), da(pxaColorDepth),
	    pxtBeginImage
	  };
	  static const byte gray_image_header[] = {
	    da(pxaDestinationSize),
	    dub(eDirectPixel), da(pxaColorMapping),
	    dub(e8Bit), da(pxaColorDepth),
	    pxtBeginImage
	  };

	  put_us(pdev->width, prn_stream);
	  put_a(pxaSourceWidth, prn_stream);
	  put_us(pdev->height, prn_stream);
	  put_a(pxaSourceHeight, prn_stream);
	  put_usp(pdev->width, pdev->height, prn_stream);
	  if ( pdev->color_info.depth == 1 )
	    fwrite_bytes(mono_image_header, prn_stream);
	  else
	    fwrite_bytes(gray_image_header, prn_stream);
	}

	/* Write the image data, compressing each line. */
	for ( lnum = 0; lnum < pdev->height; ++lnum )
	  { int ncompr;
	    static const byte line_header[] = {
	      da(pxaStartLine),
	      dus(1), da(pxaBlockHeight),
	      dub(eRLECompression), da(pxaCompressMode),
	      pxtReadImage
	    };

	    code = gdev_prn_copy_scan_lines(pdev, lnum, (byte *)line, line_size);
	    if ( code < 0 )
	      goto fin;
	    put_us(lnum, prn_stream);
	    fwrite_bytes(line_header, prn_stream);
	    ncompr = gdev_pcl_mode2compress(line, line + line_size_words, out);
	    if ( ncompr <= 255 )
	      { fputc(pxt_dataLengthByte, prn_stream);
	        fputc(ncompr, prn_stream);
	      }
	    else
	      { fputc(pxt_dataLength, prn_stream);
	        put_l(ncompr, prn_stream);
	      }
	    fwrite(out, 1, ncompr, prn_stream);
	  }

	/* Finish up. */
fin:
	fputc(pxtEndImage, prn_stream);
	fputc(pxtEndPage, prn_stream);
done:
	if ( out )
	  gs_free(out, out_size, 1, "ljet5(out)");
	if ( line )
	  gs_free(line, line_size_words, W, "ljet5(out)");
	return code;
}
