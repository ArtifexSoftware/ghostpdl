/* Copyright (C) 1996 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* pcfont.h */
/* Definitions for PCL5 fonts */

#include "plfont.h"

/* Define the common structure of downloaded font headers. */
typedef struct pcl_font_header_s {
  byte FontDescriptorSize[2]; /* must be >=64 */
  byte HeaderFormat;
  byte FontType;
  byte StyleMSB;
  byte Reserved;		/* must be 0 */
  byte BaselinePosition[2];
  byte CellWidth[2];
  byte CellHeight[2];
  byte Orientation;
  byte Spacing;
  byte SymbolSet[2];
  byte Pitch[2];
  byte Height[2];
  byte xHeight[2];
  byte WidthType;
  byte StyleLSB;
  byte StrokeWeight;
  byte TypefaceLSB;
  byte TypefaceMSB;
  byte SerifStyle;
  byte Quality;
  byte Placement;
  byte UnderlinePosition;
  byte UnderlineThickness;
  byte TextHeight[2];
  byte TextWidth[2];
  byte FirstCode[2];
  byte LastCode[2];
  byte PitchExtended;
  byte HeightExtended;
  byte CapHeight[2];
  byte FontNumber[4];
  char FontName[16];
} pcl_font_header_t;

/* Define the downloaded font header formats. */
typedef enum {
  pcfh_bitmap = 0,
  pcfh_resolution_bitmap = 20,
  pcfh_intellifont_bound = 10,
  pcfh_intellifont_unbound = 11,
  pcfh_truetype = 15,
  pcfh_truetype_large = 16
} pcl_font_header_format_t;

/* Define the extended of resolution-specified bitmap font header (type 20). */
typedef struct pcl_resolution_bitmap_header_s {
  pcl_font_header_t common;
  byte XResolution[2];
  byte YResolution[2];
  char Copyright[1];
} pcl_resolution_bitmap_header_t;
