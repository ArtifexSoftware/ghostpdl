/* Copyright (C) 1996 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* pxptable.c */
/* PCL XL parser tables */

#include "std.h"
#include "pxenum.h"
#include "pxoper.h"
#include "pxvalue.h"
#include "pxptable.h"		/* requires pxenum.h, pxoper.h, pxvalue.h */

/* ---------------- Attribute values ---------------- */

#define sc(sizes) {pxd_scalar|sizes}			/* scalar */
#define scp(sizes,proc) {pxd_scalar|sizes, 0, proc}
#define scub() {pxd_scalar|ub, 255}
#define xy(sizes) {pxd_xy|sizes}			/* XY pair */
#define xyp(sizes,proc) {pxd_xy|sizes, 0, proc}
#define box(sizes) {pxd_box|sizes}			/* box */
#define arr(sizes) {pxd_array|sizes}			/* array */
#define arrp(sizes,proc) {pxd_array|sizes, 0, proc}
#define en(limit) {pxd_scalar|pxd_ubyte, (limit)-1}	/* enumeration */
#define enp(limit,proc) {pxd_scalar|pxd_ubyte, (limit)-1, proc}
#define zero en(1)	/* must be zero */
#define ub pxd_ubyte
#define us pxd_uint16
#define ul pxd_uint32
#define ss pxd_sint16
#define sl pxd_sint32
#define rl pxd_real32

#define none {0}
#define none5 none, none, none, none, none

#define ok_iff(test)\
  ((test) ? 0 : gs_note_error(errorIllegalAttributeValue))

private int
checkCharAngle(const px_value_t *pv)
{	real v = real_value(pv, 0);
	return ok_iff(v >= -360 && v <= 360);
}
private int
checkCharBoldValue(const px_value_t *pv)
{	return ok_iff(pv->value.r >= 0 && pv->value.r <= 1);
}
private int
checkCharScale(const px_value_t *pv)
{	real x = real_value(pv, 0), y = real_value(pv, 1);
	return ok_iff(x >= -32768 && x <= 32767 && y >= -32768 && y <= 32767);
}
#define checkCharShear checkCharScale
private int
checkDestinationSize(const px_value_t *pv)
{	return ok_iff(pv->value.ia[0] != 0 && pv->value.ia[1] != 0);
}
private int
checkDitherMatrixDataType(const px_value_t *pv)
{	return ok_iff(pv->value.i == eUByte);
}
private int
checkDitherMatrixDepth(const px_value_t *pv)
{	return ok_iff(pv->value.i == e8Bit);
}
private int
checkDitherMatrixSize(const px_value_t *pv)
{	return ok_iff(pv->value.i >= 1 && pv->value.i <= 256);
}
private int
checkGrayLevel(const px_value_t *pv)
{	return ok_iff(pv->type & pxd_any_real ?
		      pv->value.r >= 0 && pv->value.r <= 1 :
		      true);
}

private int
checkPageAngle(const px_value_t *pv)
{       
    /* XL 1.0 generates an error for non-ortoganal page angles */
    return 0;
}

private int
checkPageScale(const px_value_t *pv)
{	real x = real_value(pv, 0), y = real_value(pv, 1);
	return ok_iff(x >= 0 && x <= 32767 && y >= 0 && y <= 32767);
}
private int
checkRGBColor(const px_value_t *pv)
{	if ( pv->value.array.size != 3 )
	  return_error(errorIllegalArraySize);
	if ( pv->type & pxd_any_real )
	  { /* Check for values between 0 and 1. */
	    uint i;
	    for ( i = 0; i < pv->value.array.size; ++i )
	      { real v = real_elt(pv, i);
		if ( v < 0.0 || v > 1.0 )
		  return_error(errorIllegalAttributeValue);
	      }
	  }
	return 0;
}
private int
checkSourceHeight(const px_value_t *pv)
{	return ok_iff(pv->value.i >= 1);
}
#define checkSourceWidth checkSourceHeight
private int
checkUnitsPerMeasure(const px_value_t *pv)
{	real x = real_value(pv, 0), y = real_value(pv, 1);
	return ok_iff(x > 0 && x <= 65535 && y > 0 && y <= 65535);
}
#undef ok_iff

const px_attr_value_type_t px_attr_value_types[] = {
  none,
  none,
  en(pxeColorDepth_next),		/* PaletteDepth = 2 */
  en(pxeColorSpace_next),		/* ColorSpace */
  zero,		/* NullBrush */
  zero,		/* NullPen */
  arr(ub),		/* PaletteData */
  none,
  sc(ss),		/* PatternSelectID = 8 */
  scp(ub|rl, checkGrayLevel),		/* GrayLevel */
  none,
  arrp(ub|rl, checkRGBColor),		/* RGBColor = 11 */
  xy(ss),		/* PatternOrigin */
  xyp(us, checkDestinationSize),		/* NewDestinationSize */
  arr(ub),                  /* PrimaryArray = 14 */
  en(pxeColorDepth_next),   /* PrimaryDepth = 15 */
  none,
  en(pxeColorimetricColorSpace_next), /* ColorimetricColorSpace = 17 */
  arr(rl),                            /* XYChromaticities = 18 NB - no limitation range according to the documentation */
  arr(rl),                            /* WhiteReferencePoint = 19 NB - no limitation range according to the documentation */
  arr(rl),                            /* CRGBMinMax = 20 NB - no limitation range according to the documentation */
  arr(rl),                            /* GammaGain = 21 NB - no limitation range according to the documentation */
  none,
  none,
  none,
  none,
  none,
  none,
  none,
  none,
  none,
  none,
  none,
  en(pxeDitherMatrix_next),		/* DeviceMatrix = 33 */
  enp(pxeDataType_next, checkDitherMatrixDataType),	/* DitherMatrixDataType */
  xy(ub|us|ss),		/* DitherOrigin */
  scub(),		/* MediaDestination -- NOT DOCUMENTED, THIS IS A GUESS */
  scub(),		/* MediaSize */
  scub(),		/* MediaSource */
  scub(),		/* MediaType -- NOT DOCUMENTED, THIS IS A GUESS */
  scub(),		/* Orientation -- illegal values only produce a warning! */
  scp(us|ss, checkPageAngle),		/* PageAngle */
  xy(ub|us|ss),		/* PageOrigin */
  xyp(ub|us|rl, checkPageScale),		/* PageScale */
  scub(),		/* ROP3 */
  en(pxeTxMode_next),		/* TxMode */
  none,
  xy(us|rl),		/* CustomMediaSize = 47 */
  en(pxeMeasure_next),		/* CustomMediaSizeUnits */
  sc(us),		/* PageCopies */
  xyp(us, checkDitherMatrixSize),		/* DitherMatrixSize */
  enp(pxeColorDepth_next, checkDitherMatrixDepth),	/* DitherMatrixDepth */
  en(pxeSimplexPageMode_next),		/* SimplexPageMode */
  en(pxeDuplexPageMode_next),		/* DuplexPageMode */
  en(pxeDuplexPageSide_next),		/* DuplexPageSide */
  none5,
  none5,
  en(pxeArcDirection_next),	/* ArcDirection = 65 */
  box(ub|us|ss),		/* BoundingBox */
  sc(ub|us|ss),		/* DashOffset */
  xy(ub|us),		/* EllipseDimension */
  xy(ub|us|ss),		/* EndPoint */
  en(pxeFillMode_next),		/* FillMode */
  en(pxeLineCap_next),		/* LineCapStyle */
  en(pxeLineJoin_next),		/* LineJoinStyle */
  sc(ub|us),		/* MiterLength */
  arr(ub|us|ss),	/* LineDashStyle */
  sc(ub|us),		/* PenWidth */
  xy(ub|us|ss),		/* Point */
  sc(ub|us),		/* NumberOfPoints */
  zero,		/* SolidLine */
  xy(ub|us|ss),		/* StartPoint */
  en(pxeDataType_next),		/* PointType */
  xy(ub|us|ss),		/* ControlPoint1 */
  xy(ub|us|ss),		/* ControlPoint2 */
  en(pxeClipRegion_next),		/* ClipRegion */
  en(pxeClipMode_next),		/* ClipMode */
  none5,
  none5,
  none,
  none,
  none,
  en(pxeColorDepth_next),		/* ColorDepth = 98 */
  sc(us),	/* BlockHeight */
  en(pxeColorMapping_next),		/* ColorMapping */
  en(pxeCompressMode_next),		/* CompressMode */
  box(us),		/* DestinationBox */
  xyp(us, checkDestinationSize),		/* DestinationSize */
  en(pxePatternPersistence_next),		/* PatternPersistence */
  sc(ss),		/* PatternDefineID */
  none,
  scp(us, checkSourceHeight),		/* SourceHeight = 107 */
  scp(us, checkSourceWidth),		/* SourceWidth */
  sc(us),		/* StartLine */
  scub(),                        		/* PadBytesMultiple */
  sc(ul),                                       /* BlockByteLength */
  none,
  none,
  none,
  sc(us),		/* NumberOfScanLines = 115 */
  none,
  none,
  none,
  none,
  none5,
  none,
  none,
  none,
  none,
  arr(ub|us),		/* CommentData = 129 */
  en(pxeDataOrg_next),		/* DataOrg */
  none,
  none,
  none,
  en(pxeMeasure_next),		/* Measure = 134 */
  none,
  en(pxeDataSource_next),		/* SourceType = 136 */
  xyp(us|rl, checkUnitsPerMeasure),		/* UnitsPerMeasure */
  none,
  arr(ub|us),		/* StreamName = 139 */
  sc(ul),		/* StreamDataLength */
  none,
  none,
  en(pxeErrorReport_next),		/* ErrorReport = 143 */
  none,
  none5,
  none5,
  none5,
  none,
  scp(us|ss|rl, checkCharAngle),		/* CharAngle = 161 */
  sc(ub|us),		/* CharCode */
	/* The spec says CharDataSize requires a uint16 argument, */
	/* but the H-P driver (sometimes?) emits a uint32. */
  sc(us|ul),		/* CharDataSize */
  xyp(ub|us|rl, checkCharScale),		/* CharScale */
  xyp(ub|us|ss|rl, checkCharShear),		/* CharShear */
  sc(ub|us|rl),		/* CharSize */
  sc(us),		/* FontHeaderLength */
  arr(ub|us),		/* FontName */
  zero,		/* FontFormat */
  sc(us),		/* SymbolSet */
  arr(ub|us),		/* TextData */
  arr(ub),		/* CharSubModeArray */
  en(pxeWritingMode_next),		/* WritingMode */
  none,
  arr(ub|us|ss),		/* XSpacingData = 175 */
  arr(ub|us|ss),		/* YSpacingData */
  scp(rl, checkCharBoldValue),	/* CharBoldValue */
};

#undef v
#undef vp
#undef vub
#undef xy
#undef xyp
#undef box
#undef array
#undef arrayp
#undef en
#undef enp
#undef zero
#undef ub
#undef us
#undef ul
#undef ss
#undef sl
#undef rl
#undef none
#undef none5

/* ---------------- Attribute names for debugging ---------------- */

#ifdef DEBUG

const char *px_attribute_names[] = {
/*0*/
  0, 0, "PaletteDepth", "ColorSpace", "NullBrush",
  "NullPen", "PaletteData", 0, "PatternSelectID", "GrayLevel",
/*10*/
  0, "RGBColor", "PatternOrigin", "NewDestinationSize", "PrimaryArray",
  "PrimaryDepth", 0, 
  "ColorimetricColorSpace", "XYChromaticities", "WhiteReferencePoint", "CRGBMinMax", "GammaGain",
/*22*/
  0, 0, 0, 0,
  0, 0, 0, 0,
/*30*/
  0, 0, 0, "DeviceMatrix", "DitherMatrixDataType",
  "DitherOrigin", "MediaDestination", "MediaSize", "MediaSource", "MediaType",
/*40*/
  "Orientation", "PageAngle", "PageOrigin", "PageScale", "ROP3",
  "TxMode", 0, "CustomMediaSize", "CustomMediaSizeUnits", "PageCopies",
/*50*/
  "DitherMatrixSize", "DitherMatrixDepth", "SimplexPageMode", "DuplexPageMode",
    "DuplexPageSide",
  0, 0, 0, 0, 0,
/*60*/
  0, 0, 0, 0, 0,
  "ArcDirection", "BoundingBox", "DashOffset", "EllipseDimension", "EndPoint",
/*70*/
  "FillMode", "LineCapStyle", "LineJoinStyle", "MiterLength", "LineDashStyle",
  "PenWidth", "Point", "NumberOfPoints", "SolidLine", "StartPoint",
/*80*/
  "PointType", "ControlPoint1", "ControlPoint2", "ClipRegion", "ClipMode",
  0, 0, 0, 0, 0,
/*90*/
  0, 0, 0, 0, 0,
  0, 0, 0, "ColorDepth", "BlockHeight",
/*100*/
  "ColorMapping", "CompressMode", "DestinationBox", "DestinationSize",
    "PatternPersistence",
  "PatternDefineID", 0, "SourceHeight", "SourceWidth", "StartLine",
/*110*/
  "PadBytesMultiple",
  "BlockByteLength",
  0, 0, 0,
  "NumberOfScanLines", 0, 0, 0, 0,
/*120*/
  0, 0, 0, 0, 0,
  0, 0, 0, 0, "CommentData",
/*130*/
  "DataOrg", 0, 0, 0, "Measure",
  0, "SourceType", "UnitsPerMeasure", 0, "StreamName",
/*140*/
  "StreamDataLength", 0, 0, "ErrorReport", 0,
  0, 0, 0, 0, 0,
/*150*/
  0, 0, 0, 0, 0,
  0, 0, 0, 0, 0,
/*160*/
  0, "CharAngle", "CharCode", "CharDataSize", "CharScale",
  "CharShear", "CharSize", "FontHeaderLength", "FontName", "FontFormat",
/*170*/
  "SymbolSet", "TextData", "CharSubModeArray", 0, 0,
  "XSpacingData", "YSpacingData", "CharBoldValue"
};

#endif

/* ---------------- Tag names for debugging ---------------- */

#ifdef DEBUG

const char *px_tag_0_names[0x40] = {
/*0x*/
  "Null", 0, 0, 0,
  0, 0, 0, 0,
  0, "HT", "LF", "VT",
  "FF", "CR", 0, 0,
/*1x*/
  0, 0, 0, 0,
  0, 0, 0, 0,
  0, 0, 0, "ESC",
  0, 0, 0, 0,
/*2x*/
  "Space", 0, 0, 0,
  0, 0, 0, "_beginASCII",
  "_beginBinaryMSB", "_beginBinaryLSB", 0, 0,
  0, 0, 0, 0,
/*3x*/
  0, 0, 0, 0,
  0, 0, 0, 0,
  0, 0, 0, 0,
  0, 0, 0, 0
};

const char *px_tag_c0_names[0x40] = {
/*cx*/
  "_ubyte", "_uint16", "_uint32", "_sint16",
  "_sint32", "_real32", 0, 0,
  "_ubyte_array", "_uint16_array", "_uint32_array", "_sint16_array",
  "_sint32_array", "_real32_array", 0, 0,
/*dx*/
  "_ubyte_xy", "_uint16_xy", "_uint32_xy", "_sint16_xy",
  "_sint32_xy", "_real32_xy", 0, 0,
  0, 0, 0, 0,
  0, 0, 0, 0,
/*ex*/
  "_ubyte_box", "_uint16_box", "_uint32_box", "_sint16_box",
  "_sint32_box", "_real32_box", 0, 0,
  0, 0, 0, 0,
  0, 0, 0, 0,
/*fx*/
  0, 0, 0, 0,
  0, 0, 0, 0,
  "_attr_ubyte", "_attr_uint16", "_dataLength", "_dataLengthByte",
  0, 0, 0, 0
};

#endif

/* ---------------- Operator names ---------------- */

/* These are needed even in non-debug configurations, */
/* for producing error reports. */

const char *px_operator_names[0x80] = {
/*4x*/
  0, "BeginSession", "EndSession", "BeginPage",
  "EndPage", 0, 0, "Comment",
  "OpenDataSource", "CloseDataSource", 0, 0,
  0, 0, 0, "BeginFontHeader",
/*5x*/
  "ReadFontHeader", "EndFontHeader", "BeginChar", "ReadChar",
  "EndChar", "RemoveFont", 
  "SetCharAttributes",
  0,
  0, 0, 0, "BeginStream",
  "ReadStream", "EndStream", "ExecStream", "RemoveStream",
/*6x*/
  "PopGS", "PushGS", "SetClipReplace", "SetBrushSource",
  "SetCharAngle", "SetCharScale", "SetCharShear", "SetClipIntersect",
  "SetClipRectangle", "SetClipToPage", "SetColorSpace", "SetCursor",
  "SetCursorRel", "SetHalftoneMethod", "SetFillMode", "SetFont",
/*7x*/
  "SetLineDash", "SetLineCap", "SetLineJoin", "SetMiterLimit",
  "SetPageDefaultCTM", "SetPageOrigin", "SetPageRotation", "SetPageScale",
  "SetPaintTxMode", "SetPenSource", "SetPenWidth", "SetROP",
  "SetSourceTxMode", "SetCharBoldValue", 0, "SetClipMode",
/*8x*/
  "SetPathToClip", "SetCharSubMode", 0, 0,
  "CloseSubPath", "NewPath", "PaintPath", 0,
  0, 0, 0, 0,
  0, 0, 0, 0,
/*9x*/
  0, "ArcPath", 0, "BezierPath",
  0, "BezierRelPath", "Chord", "ChordPath",
  "Ellipse", "EllipsePath", 0, "LinePath",
  0, "LineRelPath", "Pie", "PiePath",
/*ax*/
  "Rectangle", "RectanglePath", "RoundRectangle", "RoundRectanglePath",
  0, 0, 0, 0,
  "Text", "TextPath", 0, 0,
  0, 0, 0, 0,
/*bx*/
  "BeginImage", "ReadImage", "EndImage", "BeginRastPattern",
  "ReadRastPattern", "EndRastPattern", "BeginScan", 0,
  "EndScan", "ScanLineRel", 0, 0,
  0, 0, 0, 0
};

/* ---------------- Operator definitions ---------------- */

#define odef(proc, args)\
  extern px_operator_proc(proc);\
  extern const byte /*px_attribute*/ args[]

/* Define the implementation of undefined operators. */
private const byte apxBadOperator[] = {0, 0};
private int
pxBadOperator(px_args_t *par, px_state_t *pxs)
{	return_error(errorIllegalTag);
}

odef(pxBeginSession, apxBeginSession);
odef(pxEndSession, apxEndSession);
odef(pxBeginPage, apxBeginPage);
odef(pxEndPage, apxEndPage);
odef(pxComment, apxComment);
odef(pxOpenDataSource, apxOpenDataSource);
odef(pxCloseDataSource, apxCloseDataSource);
odef(pxBeginFontHeader, apxBeginFontHeader);
odef(pxReadFontHeader, apxReadFontHeader);
odef(pxEndFontHeader, apxEndFontHeader);
odef(pxBeginChar, apxBeginChar);
odef(pxReadChar, apxReadChar);
odef(pxEndChar, apxEndChar);
odef(pxRemoveFont, apxRemoveFont);
odef(pxSetCharAttributes, apxSetCharAttributes);
odef(pxBeginStream, apxBeginStream);
odef(pxReadStream, apxReadStream);
odef(pxEndStream, apxEndStream);
odef(pxExecStream, apxExecStream);
odef(pxRemoveStream, apxRemoveStream);
odef(pxPopGS, apxPopGS);
odef(pxPushGS, apxPushGS);
odef(pxSetClipReplace, apxSetClipReplace);
odef(pxSetBrushSource, apxSetBrushSource);
odef(pxSetCharAngle, apxSetCharAngle);
odef(pxSetCharScale, apxSetCharScale);
odef(pxSetCharShear, apxSetCharShear);
odef(pxSetClipIntersect, apxSetClipIntersect);
odef(pxSetClipRectangle, apxSetClipRectangle);
odef(pxSetClipToPage, apxSetClipToPage);
odef(pxSetColorSpace, apxSetColorSpace);
odef(pxSetCursor, apxSetCursor);
odef(pxSetCursorRel, apxSetCursorRel);
odef(pxSetHalftoneMethod, apxSetHalftoneMethod);
odef(pxSetFillMode, apxSetFillMode);
odef(pxSetFont, apxSetFont);
odef(pxSetLineDash, apxSetLineDash);
odef(pxSetLineCap, apxSetLineCap);
odef(pxSetLineJoin, apxSetLineJoin);
odef(pxSetMiterLimit, apxSetMiterLimit);
odef(pxSetPageDefaultCTM, apxSetPageDefaultCTM);
odef(pxSetPageOrigin, apxSetPageOrigin);
odef(pxSetPageRotation, apxSetPageRotation);
odef(pxSetPageScale, apxSetPageScale);
odef(pxSetPaintTxMode, apxSetPaintTxMode);
odef(pxSetPenSource, apxSetPenSource);
odef(pxSetPenWidth, apxSetPenWidth);
odef(pxSetROP, apxSetROP);
odef(pxSetSourceTxMode, apxSetSourceTxMode);
odef(pxSetCharBoldValue, apxSetCharBoldValue);
odef(pxSetClipMode, apxSetClipMode);
odef(pxSetPathToClip, apxSetPathToClip);
odef(pxSetCharSubMode, apxSetCharSubMode);
odef(pxCloseSubPath, apxCloseSubPath);
odef(pxNewPath, apxNewPath);
odef(pxPaintPath, apxPaintPath);
odef(pxArcPath, apxArcPath);
odef(pxBezierPath, apxBezierPath);
odef(pxBezierRelPath, apxBezierRelPath);
odef(pxChord, apxChord);
odef(pxChordPath, apxChordPath);
odef(pxEllipse, apxEllipse);
odef(pxEllipsePath, apxEllipsePath);
odef(pxLinePath, apxLinePath);
odef(pxLineRelPath, apxLineRelPath);
odef(pxPie, apxPie);
odef(pxPiePath, apxPiePath);
odef(pxRectangle, apxRectangle);
odef(pxRectanglePath, apxRectanglePath);
odef(pxRoundRectangle, apxRoundRectangle);
odef(pxRoundRectanglePath, apxRoundRectanglePath);
odef(pxText, apxText);
odef(pxTextPath, apxTextPath);
odef(pxBeginImage, apxBeginImage);
odef(pxReadImage, apxReadImage);
odef(pxEndImage, apxEndImage);
odef(pxBeginRastPattern, apxBeginRastPattern);
odef(pxReadRastPattern, apxReadRastPattern);
odef(pxEndRastPattern, apxEndRastPattern);
odef(pxBeginScan, apxBeginScan);
odef(pxEndScan, apxEndScan);
odef(pxScanLineRel, apxScanLineRel);

#define none {pxBadOperator, apxBadOperator}

const px_operator_definition_t px_operator_definitions[] = {
/*4x*/
  none,
  {pxBeginSession, apxBeginSession},
  {pxEndSession, apxEndSession},
  {pxBeginPage, apxBeginPage},
  {pxEndPage, apxEndPage},
  none,
  none,
  {pxComment, apxComment},
  {pxOpenDataSource, apxOpenDataSource},
  {pxCloseDataSource, apxCloseDataSource},
  none,
  none,
  none,
  none,
  none,
  {pxBeginFontHeader, apxBeginFontHeader},
/*5x*/
  {pxReadFontHeader, apxReadFontHeader},
  {pxEndFontHeader, apxEndFontHeader},
  {pxBeginChar, apxBeginChar},
  {pxReadChar, apxReadChar},
  {pxEndChar, apxEndChar},
  {pxRemoveFont, apxRemoveFont},
  {pxSetCharAttributes, apxSetCharAttributes},
  none,
  none,
  none,
  none,
  {pxBeginStream, apxBeginStream},
  {pxReadStream, apxReadStream},
  {pxEndStream, apxEndStream},
  {pxExecStream, apxExecStream},
  {pxRemoveStream, apxRemoveStream},
/*6x*/
  {pxPopGS, apxPopGS},
  {pxPushGS, apxPushGS},
  {pxSetClipReplace, apxSetClipReplace},
  {pxSetBrushSource, apxSetBrushSource},
  {pxSetCharAngle, apxSetCharAngle},
  {pxSetCharScale, apxSetCharScale},
  {pxSetCharShear, apxSetCharShear},
  {pxSetClipIntersect, apxSetClipIntersect},
  {pxSetClipRectangle, apxSetClipRectangle},
  {pxSetClipToPage, apxSetClipToPage},
  {pxSetColorSpace, apxSetColorSpace},
  {pxSetCursor, apxSetCursor},
  {pxSetCursorRel, apxSetCursorRel},
  {pxSetHalftoneMethod, apxSetHalftoneMethod},
  {pxSetFillMode, apxSetFillMode},
  {pxSetFont, apxSetFont},
/*7x*/
  {pxSetLineDash, apxSetLineDash},
  {pxSetLineCap, apxSetLineCap},
  {pxSetLineJoin, apxSetLineJoin},
  {pxSetMiterLimit, apxSetMiterLimit},
  {pxSetPageDefaultCTM, apxSetPageDefaultCTM},
  {pxSetPageOrigin, apxSetPageOrigin},
  {pxSetPageRotation, apxSetPageRotation},
  {pxSetPageScale, apxSetPageScale},
  {pxSetPaintTxMode, apxSetPaintTxMode},
  {pxSetPenSource, apxSetPenSource},
  {pxSetPenWidth, apxSetPenWidth},
  {pxSetROP, apxSetROP},
  {pxSetSourceTxMode, apxSetSourceTxMode},
  {pxSetCharBoldValue, apxSetCharBoldValue},
  none,
  {pxSetClipMode, apxSetClipMode},
/*8x*/
  {pxSetPathToClip, apxSetPathToClip},
  {pxSetCharSubMode, apxSetCharSubMode},
  none,
  none,
  {pxCloseSubPath, apxCloseSubPath},
  {pxNewPath, apxNewPath},
  {pxPaintPath, apxPaintPath},
  none,
  none,
  none,
  none,
  none,
  none,
  none,
  none,
  none,
/*9x*/
  none,
  {pxArcPath, apxArcPath},
  none,
  {pxBezierPath, apxBezierPath},
  none,
  {pxBezierRelPath, apxBezierRelPath},
  {pxChord, apxChord},
  {pxChordPath, apxChordPath},
  {pxEllipse, apxEllipse},
  {pxEllipsePath, apxEllipsePath},
  none,
  {pxLinePath, apxLinePath},
  none,
  {pxLineRelPath, apxLineRelPath},
  {pxPie, apxPie},
  {pxPiePath, apxPiePath},
/*ax*/
  {pxRectangle, apxRectangle},
  {pxRectanglePath, apxRectanglePath},
  {pxRoundRectangle, apxRoundRectangle},
  {pxRoundRectanglePath, apxRoundRectanglePath},
  none,
  none,
  none,
  none,
  {pxText, apxText},
  {pxTextPath, apxTextPath},
  none,
  none,
  none,
  none,
  none,
  none,
/*bx*/
  {pxBeginImage, apxBeginImage},
  {pxReadImage, apxReadImage},
  {pxEndImage, apxEndImage},
  {pxBeginRastPattern, apxBeginRastPattern},
  {pxReadRastPattern, apxReadRastPattern},
  {pxEndRastPattern, apxEndRastPattern},
  {pxBeginScan, apxBeginScan},
  none,
  {pxEndScan, apxEndScan},
  {pxScanLineRel, apxScanLineRel},
  none,
  none,
  none,
  none,
  none,
  none
};
