/* Copyright (C) 1994-1996, Russell Lang.
   Portions Copyright (C) 2001 artofcode LLC.
   Portions Copyright (C) 1996, 2001 Artifex Software Inc.
   Portions Copyright (C) 1988, 2000 Aladdin Enterprises.
   This software is based in part on the work of the Independent JPEG Group.  
   All Rights Reserved.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */
  
/*$RCSfile$ $Revision$ */
/* gsdll extension for Microsoft Windows platforms */

#ifndef gsdllwin_INCLUDED
#  define gsdllwin_INCLUDED

/* DLL exported functions */
/* for load time dynamic linking */
HGLOBAL GSDLLAPI gsdll_copy_dib(unsigned char GSFAR * device);
HPALETTE GSDLLAPI gsdll_copy_palette(unsigned char GSFAR * device);
void GSDLLAPI gsdll_draw(unsigned char GSFAR * device, HDC hdc, LPRECT dest,
			 LPRECT src);
int GSDLLAPI gsdll_get_bitmap_row(unsigned char *device,
				  LPBITMAPINFOHEADER pbmih,
				  LPRGBQUAD prgbquad, LPBYTE * ppbyte,
				  unsigned int row);

/* Function pointer typedefs */
/* for run time dynamic linking */
typedef HGLOBAL (GSDLLAPI * PFN_gsdll_copy_dib)(unsigned char GSFAR *);
typedef HPALETTE (GSDLLAPI * PFN_gsdll_copy_palette)(unsigned char GSFAR *);
typedef void (GSDLLAPI * PFN_gsdll_draw) (unsigned char GSFAR *, HDC, LPRECT,
					  LPRECT);
typedef int (GSDLLAPI * PFN_gsdll_get_bitmap_row)
     (unsigned char *device, LPBITMAPINFOHEADER pbmih, LPRGBQUAD prgbquad,
      LPBYTE * ppbyte, unsigned int row);

#endif /* gsdllwin_INCLUDED */
