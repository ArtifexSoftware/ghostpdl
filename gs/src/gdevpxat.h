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

/* gdevpxat.h */
/* Attribute ID definitions for PCL XL */

#ifndef gdevpxat_INCLUDED
#  define gdevpxat_INCLUDED

typedef enum {

  pxaPaletteDepth = 2,
  pxaColorSpace,
  pxaNullBrush,
  pxaNullPen,
  pxaPaletteData,

  pxaPatternSelectID = 8,
  pxaGrayLevel,

  pxaRGBColor = 11,
  pxaPatternOrigin,
  pxaNewDestinationSize,

  pxaDeviceMatrix = 33,
  pxaDitherMatrixDataType,
  pxaDitherOrigin,
  pxaMediaDestination,
  pxaMediaSize,
  pxaMediaSource,
  pxaMediaType,
  pxaOrientation,
  pxaPageAngle,
  pxaPageOrigin,
  pxaPageScale,
  pxaROP3,
  pxaTxMode,

  pxaCustomMediaSize = 47,
  pxaCustomMediaSizeUnits,
  pxaPageCopies,
  pxaDitherMatrixSize,
  pxaDitherMatrixDepth,
  pxaSimplexPageMode,
  pxaDuplexPageMode,
  pxaDuplexPageSide,

  pxaArcDirection = 65,
  pxaBoundingBox,
  pxaDashOffset,
  pxaEllipseDimension,
  pxaEndPoint,
  pxaFillMode,
  pxaLineCapStyle,
  pxaLineJoinStyle,
  pxaMiterLength,
  pxaLineDashStyle,
  pxaPenWidth,
  pxaPoint,
  pxaNumberOfPoints,
  pxaSolidLine,
  pxaStartPoint,
  pxaPointType,
  pxaControlPoint1,
  pxaControlPoint2,
  pxaClipRegion,
  pxaClipMode,

  pxaColorDepth = 98,
  pxaBlockHeight,
  pxaColorMapping,
  pxaCompressMode,
  pxaDestinationBox,
  pxaDestinationSize,
  pxaPatternPersistence,
  pxaPatternDefineID,

  pxaSourceHeight = 107,
  pxaSourceWidth,
  pxaStartLine,

  pxaNumberOfScanLines = 115,

  pxaCommentData = 129,
  pxaDataOrg,

  pxaMeasure = 134,

  pxaSourceType = 136,
  pxaUnitsPerMeasure,

  pxaStreamName = 139,
  pxaStreamDataLength,

  pxaErrorReport = 143,

  pxaCharAngle = 161,
  pxaCharCode,
  pxaCharDataSize,
  pxaCharScale,
  pxaCharShear,
  pxaCharSize,
  pxaFontHeaderLength,
  pxaFontName,
  pxaFontFormat,
  pxaSymbolSet,
  pxaTextData,
  pxaCharSubModeArray,

  pxaXSpacingData = 175,
  pxaYSpacingData,
  pxaCharBoldValue,

  px_attribute_next

} px_attribute_t;

#endif				/* gdevpxat_INCLUDED */
