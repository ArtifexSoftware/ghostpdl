/* Copyright (C) 1994-1996, Russell Lang.  All rights reserved.

   This software is licensed to a single customer by Artifex Software Inc.
   under the terms of a specific OEM agreement.
 */

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
