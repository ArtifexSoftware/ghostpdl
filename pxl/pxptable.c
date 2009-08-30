/* Portions Copyright (C) 2001 artofcode LLC.
   Portions Copyright (C) 1996, 2001 Artifex Software Inc.
   Portions Copyright (C) 1988, 2000 Aladdin Enterprises.
   This software is based in part on the work of the Independent JPEG Group.
   All Rights Reserved.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */
/*$Id$ */

/* pxptable.c */
/* PCL XL parser tables */

#include "std.h"
#include "pxenum.h"
#include "pxoper.h"
#include "pxvalue.h"
#include "pxptable.h"           /* requires pxenum.h, pxoper.h, pxvalue.h */
#include "pxstate.h"

/* ---------------- Attribute values ---------------- */

#define sc(sizes) {pxd_scalar|sizes}                    /* scalar */
#define scp(sizes,proc) {pxd_scalar|sizes, 0, proc}
#define scub() {pxd_scalar|ub, 255}
#define xy(sizes) {pxd_xy|sizes}                        /* XY pair */
#define xyp(sizes,proc) {pxd_xy|sizes, 0, proc}
#define box(sizes) {pxd_box|sizes}                      /* box */
#define arr(sizes) {pxd_array|sizes}                    /* array */
#define arrp(sizes,proc) {pxd_array|sizes, 0, proc}
#define en(limit) {pxd_scalar|pxd_ubyte, (limit)-1}     /* enumeration */
#define enp(limit,proc) {pxd_scalar|pxd_ubyte, (limit)-1, proc}
#define zero en(1)      /* must be zero */
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

static int
checkCharAngle(const px_value_t *pv)
{       real v = real_value(pv, 0);
        return ok_iff(v >= -360 && v <= 360);
}
static int
checkCharBoldValue(const px_value_t *pv)
{       return ok_iff(pv->value.r >= 0 && pv->value.r <= 1);
}
static int
checkCharScale(const px_value_t *pv)
{       real x = real_value(pv, 0), y = real_value(pv, 1);
        return ok_iff(x >= -32768 && x <= 32767 && y >= -32768 && y <= 32767);
}
#define checkCharShear checkCharScale
static int
checkDestinationSize(const px_value_t *pv)
{       return ok_iff(pv->value.ia[0] != 0 && pv->value.ia[1] != 0);
}
static int
checkDitherMatrixDataType(const px_value_t *pv)
{       return ok_iff(pv->value.i == eUByte);
}
static int
checkDitherMatrixDepth(const px_value_t *pv)
{       return ok_iff(pv->value.i == e8Bit);
}
static int
checkDitherMatrixSize(const px_value_t *pv)
{       return ok_iff(pv->value.i >= 1 && pv->value.i <= 256);
}
static int
checkGrayLevel(const px_value_t *pv)
{       return ok_iff(pv->type & pxd_any_real ?
                      pv->value.r >= 0 && pv->value.r <= 1 :
                      true);
}

static int
checkPageAngle(const px_value_t *pv)
{       
    /* XL 1.0 generates an error for non-orthogonal page angles */
    return 0;
}

static int
checkPageScale(const px_value_t *pv)
{       real x = real_value(pv, 0), y = real_value(pv, 1);
        return ok_iff(x >= 0 && x <= 32767 && y >= 0 && y <= 32767);
}
static int
checkRGBColor(const px_value_t *pv)
{       if ( pv->value.array.size != 3 )
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
static int
checkSourceHeight(const px_value_t *pv)
{       return ok_iff(pv->value.i >= 1);
}
#define checkSourceWidth checkSourceHeight
static int
checkUnitsPerMeasure(const px_value_t *pv)
{       real x = real_value(pv, 0), y = real_value(pv, 1);
        return ok_iff(x > 0 && x <= 65535 && y > 0 && y <= 65535);
}
#undef ok_iff

const px_attr_value_type_t px_attr_value_types[] = {
/* 0 */    none,                    
/* 1 */    none,
/* 2 */    en(pxeColorDepth_next),       /* PaletteDepth = 2 */
/* 3 */    en(pxeColorSpace_next),              /* ColorSpace */
/* 4 */    zero,                /* NullBrush */
/* 5 */    zero,                /* NullPen */
/* 6 */    arr(ub),             /* PaletteData */
/* 7 */    none,
/* 8 */    sc(ss),              /* PatternSelectID = 8 */
/* 9 */    scp(ub|rl, checkGrayLevel),          /* GrayLevel */
/* 10 */   none,
/* 11 */   arrp(ub|rl, checkRGBColor),          /* RGBColor = 11 */
/* 12 */   xy(ss),              /* PatternOrigin */
/* 13 */   xyp(us, checkDestinationSize),               /* NewDestinationSize */
/* 14 */   arr(ub),                  /* PrimaryArray = 14 */
/* 15 */   en(pxeColorDepth_next),   /* PrimaryDepth = 15 */
/* 16 */   none,
/* 17 */   en(pxeColorimetricColorSpace_next), /* ColorimetricColorSpace = 17 */
/* 18 */   arr(rl),  /* XYChromaticities = 18 */
/* 19 */   arr(rl),  /* WhiteReferencePoint = 19 */
/* 20 */   arr(rl),  /* CRGBMinMax = 20 */
/* 21 */   arr(rl),  /* GammaGain = 21 */
/* 22 */   none,
/* 23 */   none,
/* 24 */   none,
/* 25 */   none,
/* 26 */   none,
/* 27 */   none,
/* 28 */   none,
/* 29 */   en(pxeColorTrapping_next), /* AllObjects NB ColorTrapping is largest enum */
/* 30 */   en(pxeColorTrapping_next), /* TextObjects = 30 */
/* 31 */   en(pxeColorTrapping_next), /* VectorObjects = 31 */
/* 32 */   en(pxeColorTrapping_next), /* RasterObjects = 32 */
/* 33 */   en(pxeDitherMatrix_next),  /* DeviceMatrix = 33 */
/* 34 */   enp(pxeDataType_next, checkDitherMatrixDataType), /* DitherMatrixDataType */
/* 35 */   xy(ub|us|ss),        /* DitherOrigin */
/* 36 */   scub(),              /* MediaDestination */
/* 37 */   {pxd_scalar|pxd_array|pxd_ubyte, 255}, /* MediaSize */
/* 38 */   scub(),              /* MediaSource */
/* 39 */   arr(ub),             /* MediaType */
/* 40 */   scub(),              /* Orientation -- illegal values only produce a warning! */
/* 41 */   scp(us|ss, checkPageAngle),          /* PageAngle */
/* 42 */   xy(ub|us|ss),                /* PageOrigin */
/* 43 */   xyp(ub|us|rl, checkPageScale),               /* PageScale */
/* 44 */   scub(),              /* ROP3 */
/* 45 */   en(pxeTxMode_next),          /* TxMode */
/* 46 */   none,
/* 47 */   xy(us|rl),           /* CustomMediaSize = 47 */
/* 48 */   en(pxeMeasure_next),         /* CustomMediaSizeUnits */
/* 49 */   sc(us),              /* PageCopies */
/* 50 */   xyp(us, checkDitherMatrixSize),              /* DitherMatrixSize */
/* 51 */   enp(pxeColorDepth_next, checkDitherMatrixDepth),     /* DitherMatrixDepth */
/* 52 */   en(pxeSimplexPageMode_next),         /* SimplexPageMode */
/* 53 */   en(pxeDuplexPageMode_next),          /* DuplexPageMode */
/* 54 */   en(pxeDuplexPageSide_next),          /* DuplexPageSide */
/* 55 */   none,
/* 56 */   none,
/* 57 */   none,
/* 58 */   none,
/* 59 */   none,
/* 60 */   none,
/* 61 */   none,
/* 62 */   none,
/* 63 */   none,
/* 64 */   none,
/* 65 */   en(pxeArcDirection_next),    /* ArcDirection = 65 */
/* 66 */   box(ub|us|ss),               /* BoundingBox */
/* 67 */   sc(ub|us|ss),                /* DashOffset */
/* 68 */   xy(ub|us),           /* EllipseDimension */
/* 69 */   xy(ub|us|ss),                /* EndPoint */
/* 70 */   en(pxeFillMode_next),                /* FillMode */
/* 71 */   en(pxeLineCap_next),         /* LineCapStyle */
/* 72 */   en(pxeLineJoin_next),                /* LineJoinStyle */
/* 73 */   sc(ub|us),           /* MiterLength */
/* 74 */   arr(ub|us|ss),       /* LineDashStyle */
/* 75 */   sc(ub|us),           /* PenWidth */
/* 76 */   xy(ub|us|ss),                /* Point */
/* 77 */   sc(ub|us),           /* NumberOfPoints */
/* 78 */   zero,                /* SolidLine */
/* 79 */   xy(ub|us|ss),                /* StartPoint */
/* 80 */   en(pxeDataType_next),                /* PointType */
/* 81 */   xy(ub|us|ss),                /* ControlPoint1 */
/* 82 */   xy(ub|us|ss),                /* ControlPoint2 */
/* 83 */   en(pxeClipRegion_next),              /* ClipRegion */
/* 84 */   en(pxeClipMode_next),                /* ClipMode */
/* 85 */   none,
/* 86 */   none,
/* 87 */   none,
/* 88 */   none,
/* 89 */   none,
/* 90 */   none,
/* 91 */   none,
/* 92 */   none,
/* 93 */   none,
/* 94 */   none,
/* 95 */   none,
/* 96 */   none,
/* 97 */   none,
/* 98 */   en(pxeColorDepth_next),              /* ColorDepth = 98 */
/* 99 */   sc(us),      /* BlockHeight */
/* 100 */  en(pxeColorMapping_next),            /* ColorMapping */
/* 101 */  en(pxeCompressMode_next),            /* CompressMode */
/* 102 */  box(us),             /* DestinationBox */
/* 103 */  xyp(us, checkDestinationSize),               /* DestinationSize */
/* 104 */  en(pxePatternPersistence_next),              /* PatternPersistence */
/* 105 */  sc(ss),              /* PatternDefineID */
/* 106 */  none,
/* 107 */  scp(us, checkSourceHeight),          /* SourceHeight = 107 */
/* 108 */  scp(us, checkSourceWidth),           /* SourceWidth */
/* 109 */  sc(us),              /* StartLine */
/* 110 */  scub(),                                      /* PadBytesMultiple */
/* 111 */  sc(ul),                                       /* BlockByteLength */
/* 112 */  none,
/* 113 */  none,
/* 114 */  none,
/* 115 */  sc(us),              /* NumberOfScanLines = 115 */
/* 116 */  none,
/* 117 */  none,
/* 118 */  none,
/* 119 */  none,
/* 120 */  en(pxeColorTreatment_next),
/* 121 */  none,
/* 122 */  none,
/* 123 */  none,
/* 124 */  none,
/* 125 */  none,
/* 126 */  none,
/* 127 */  none,
/* 128 */  none,
/* 129 */  arr(ub|us),          /* CommentData = 129 */
/* 130 */  en(pxeDataOrg_next),         /* DataOrg */
/* 131 */  none,
/* 132 */  none,
/* 133 */  none,
/* 134 */  en(pxeMeasure_next),         /* Measure = 134 */
/* 135 */  none,
/* 136 */  en(pxeDataSource_next),              /* SourceType = 136 */
/* 137 */  xyp(us|rl, checkUnitsPerMeasure),            /* UnitsPerMeasure */
/* 138 */  none,
/* 139 */  arr(ub|us),          /* StreamName = 139 */
/* 140 */  sc(ul),              /* StreamDataLength */
/* 141 */  none,
/* 142 */  none,
/* 143 */  en(pxeErrorReport_next),             /* ErrorReport = 143 */
/* 144 */  none,
/* 145 */  sc(ul),              /* VUExtension = 145 */
/* 146 */  none,
/* 147 */  scub(),              /* VUAttr1 = 147 */
/* 148 */  scub(),
/* 149 */  scub(),
/* 150 */  none,
/* 151 */  none,
/* 152 */  none,
/* 153 */  none,
/* 154 */  none,
/* 155 */  none,
/* 156 */  none,
/* 157 */  none,
/* 158 */  none,
/* 159 */  none,
/* 160 */  none,
/* 161 */  scp(us|ss|rl, checkCharAngle),               /* CharAngle = 161 */
/* 162 */  sc(ub|us),           /* CharCode */
/* 163 */  sc(us|ul),           /* CharDataSize HP spec says us - driver sometimes emits ul */
/* 164 */  xyp(ub|us|rl, checkCharScale),               /* CharScale */
/* 165 */  xyp(ub|us|ss|rl, checkCharShear),            /* CharShear */
/* 166 */  sc(ub|us|rl),                /* CharSize */
/* 167 */  sc(us),              /* FontHeaderLength */
/* 168 */  arr(ub|us),          /* FontName */
/* 169 */  zero,                /* FontFormat */
/* 170 */  sc(us),              /* SymbolSet */
/* 171 */  arr(ub|us),          /* TextData */
/* 172 */  arr(ub),             /* CharSubModeArray */
/* 173 */  en(pxeWritingMode_next),             /* WritingMode */
/* 174 */  none,
/* 175 */  arr(ub|us|ss),               /* XSpacingData = 175 */
/* 176 */  arr(ub|us|ss),               /* YSpacingData */
/* 177 */  scp(rl, checkCharBoldValue), /* CharBoldValue */
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
  0, 0, 0, "AllObjectTypes",
/*30*/
  "TextObjects", "VectorObjects", "RasterObjects", "DeviceMatrix", "DitherMatrixDataType",
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
  "ColorTreatment", 0, 0, 0, 0,
  0, 0, 0, 0, "CommentData",
/*130*/
  "DataOrg", 0, 0, 0, "Measure",
  0, "SourceType", "UnitsPerMeasure", 0, "StreamName",
/*140*/
  "StreamDataLength", 0, 0, "ErrorReport", 0,
  "VUExtension", 0, "VUAttr1", "VUAttr2", "VUAttr3",
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
  "EndPage", 0, "VendorUnique", "Comment",
  "OpenDataSource", "CloseDataSource", 0, 0,
  0, 0, 0, "BeginFontHeader",
/*5x*/
  "ReadFontHeader", "EndFontHeader", "BeginChar", "ReadChar",
  "EndChar", "RemoveFont", 
  "SetCharAttributes",
  "SetDefaultGS", "SetColorTreatment",
  0, 0, "BeginStream",
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
  "SetSourceTxMode", "SetCharBoldValue", "SetNeutralAxis", "SetClipMode",
/*8x*/
  "SetPathToClip", "SetCharSubMode", "BeginUserDefinedLineCap", "pxtEndUserDefinedLineCap",
  "CloseSubPath", "NewPath", "PaintPath", 0,
  0, 0, 0, 0,
  0, 0, 0, 0,
/*9x*/
  0, "ArcPath", "SetColorTrapping", "BezierPath",
  "SetAdaptiveHalftoning", "BezierRelPath", "Chord", "ChordPath",
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
  0, 0, 0, "Passthrough"
};

/* ---------------- Operator definitions ---------------- */

/* Define the implementation of undefined operators. */
static const byte apxBadOperator[] = {0, 0};
static int
pxBadOperator(px_args_t *par, px_state_t *pxs)
{       return_error(errorIllegalTag);
}

#define none {pxBadOperator, apxBadOperator}

const px_operator_definition_t px_operator_definitions[] = {
/*4x*/
  none,
  {pxBeginSession, apxBeginSession},
  {pxEndSession, apxEndSession},
  {pxBeginPage, apxBeginPage},
  {pxEndPage, apxEndPage},
  none,
  {pxVendorUnique, apxVendorUnique},
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
  {pxSetDefaultGS, apxSetDefaultGS},
  {pxSetColorTreatment, apxSetColorTreatment},
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
  {pxSetNeutralAxis, apxSetNeutralAxis},
  {pxSetClipMode, apxSetClipMode},
/*8x*/
  {pxSetPathToClip, apxSetPathToClip},
  {pxSetCharSubMode, apxSetCharSubMode},
  {pxBeginUserDefinedLineCap, apxBeginUserDefinedLineCap},
  {pxEndUserDefinedLineCap, apxEndUserDefinedLineCap},
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
  {pxSetColorTrapping, apxSetColorTrapping},
  {pxBezierPath, apxBezierPath},
  {pxSetAdaptiveHalftoning, apxSetAdaptiveHalftoning},
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
  {pxPassthrough, apxPassthrough}
};
