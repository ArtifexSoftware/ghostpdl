/* Copyright (C) 2001-2021 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
*/


/* pxerrors.h */
/* Error code definitions for PCL XL parser */

#ifndef pxerrors_INCLUDED
#  define pxerrors_INCLUDED

/* ---------------- Procedural interface ---------------- */

#ifndef px_parser_state_t_DEFINED
#  define px_parser_state_t_DEFINED
typedef struct px_parser_state_s px_parser_state_t;
#endif

#ifndef px_state_DEFINED
#  define px_state_DEFINED
typedef struct px_state_s px_state_t;
#endif

/* Record a warning. */
/* Return 1 if the warning table overflowed. */
/* If save_all is false, only remember the last warning with the same */
/* first word as this one. */
int px_record_warning(const char *message, bool save_all, px_state_t * pxs);

/* Generate a line of an error message starting at internal position N; */
/* return an updated value of N.  When done, return -1. */
#define px_max_error_line 120
int px_error_message_line(char message[px_max_error_line + 1], int N,
                          const char *subsystem, int code,
                          const px_parser_state_t * st,
                          const px_state_t * pxs);

/* Begin an error page.  Returns the initial Y value in *y. */
int px_begin_error_page(px_state_t * pxs, int * y);

/* Print a message on an error page. */
/* Return the updated Y value. */
int px_error_page_show(const char *message, int ytop, px_state_t * pxs);

/* Reset the warning table. */
void px_reset_errors(px_state_t * pxs);

/* ---------------- Error name table ---------------- */

/*
 * The following peculiar structure allows us to include this file wherever
 * error code definitions are needed, and use the same file to generate the
 * table of error names by setting INCLUDE_ERROR_NAMES.
 */

#undef _e_                      /* in case we're including this file twice */

#		ifdef INCLUDE_ERROR_NAMES

const char *px_error_names[] = {
#  define _e_(code, name) name,

#		else /* !INCLUDE_ERROR_NAMES */

extern const char *px_error_names[];

#define px_error_first -1000
#  define _e_(code,name) code,
typedef enum
{

#		endif           /* (!)INCLUDE_ERROR_NAMES */

    _e_(errorIllegalOperatorSequence =
        px_error_first, "IllegalOperatorSequence")
        _e_(errorIllegalTag, "IllegalTag")
        _e_(errorInsufficientMemory, "InsufficientMemory")
        _e_(errorInternalOverflow, "InternalOverflow")

        _e_(errorIllegalArraySize, "IllegalArraySize")
        _e_(errorIllegalAttribute, "IllegalAttribute")
        _e_(errorIllegalAttributeCombination, "IllegalAttributeCombination")
        _e_(errorIllegalAttributeDataType, "IllegalAttributeDataType")
        _e_(errorIllegalAttributeValue, "IllegalAttributeValue")
        _e_(errorMissingAttribute, "MissingAttribute")

        _e_(errorCurrentCursorUndefined, "CurrentCursorUndefined")

        _e_(errorNoCurrentFont, "NoCurrentFont")
        _e_(errorBadFontData, "BadFontData")

        _e_(errorDataSourceNotOpen, "DataSourceNotOpen")
        _e_(errorExtraData, "ExtraData")
        _e_(errorIllegalDataLength, "IllegalDataLength")
        _e_(errorIllegalDataValue, "IllegalDataValue")
        _e_(errorMissingData, "MissingData")

        _e_(errorCannotReplaceCharacter, "CannotReplaceCharacter")
        _e_(errorFontUndefined, "FontUndefined")

        _e_(errorFontNameAlreadyExists, "FontNameAlreadyExists")

        _e_(errorImagePaletteMismatch, "ImagePaletteMismatch")
        _e_(errorMissingPalette, "MissingPalette")

        _e_(errorIllegalMediaSize, "IllegalMediaSize")
        _e_(errorIllegalMediaSource, "IllegalMediaSource")
        _e_(errorIllegalOrientation, "IllegalOrientation")

        _e_(errorStreamUndefined, "StreamUndefined")
        _e_(errorStreamNestingFull, "StreamNestingFull")

        _e_(errorDataSourceNotClosed, "DataSourceNotClosed")

        _e_(errorMaxGSLevelsExceeded, "MaxGSLevelsExceeded")

        _e_(errorFSTMismatch, "FSTMismatch")
        _e_(errorUnsupportedCharacterClass, "UnsupportedCharacterClass")
        _e_(errorUnsupportedCharacterFormat, "UnsupportedCharacterFormat")
        _e_(errorIllegalCharacterData, "IllegalCharacterData")

        _e_(errorIllegalFontData, "IllegalFontData")
        _e_(errorIllegalFontHeaderFields, "IllegalFontHeaderFields")
        _e_(errorIllegalNullSegmentSize, "IllegalNullSegmentSize")
        _e_(errorIllegalFontSegment, "IllegalFontSegment")
        _e_(errorMissingRequiredSegment, "MissingRequiredSegment")
        _e_(errorIllegalGlobalTrueTypeSegment, "IllegalGlobalTrueTypeSegment")
        _e_(errorIllegalGalleyCharacterSegment,
            "IllegalGalleyCharacterSegment")
        _e_(errorIllegalVerticalTxSegment, "IllegalVerticalTxSegment")
        _e_(errorIllegalBitmapResolutionSegment,
            "IllegalBitmapResolutionSegment")

        _e_(errorUndefinedFontNotRemoved, "UndefinedFontNotRemoved")
        _e_(errorInternalFontNotRemoved, "InternalFontNotRemoved")
        _e_(errorMassStorageFontNotRemoved, "MassStorageFontNotRemoved")

        _e_(errorColorSpaceMismatch, "ColorSpaceMismatch")
        _e_(errorRasterPatternUndefined, "RasterPatternUndefined")

        _e_(errorClipModeMismatch, "ClipModeMismatch")

        _e_(errorFontUndefinedNoSubstituteFound,
            "FontUndefinedNoSubstituteFound")
        _e_(errorSymbolSetRemapUndefined, "SymbolSetRemapUndefined")

        _e_(errorUnsupportedBinding, "UnsupportedBinding")
        _e_(errorUnsupportedClassName, "UnsupportedClassName")
        _e_(errorUnsupportedProtocol, "UnsupportedProtocol")
        _e_(errorIllegalStreamHeader, "IllegalStreamHeader")

        /* We define a special "error" that EndSession returns */
        /* to indicate that there were warnings to report. */
        _e_(errorWarningsReported, "WarningsReported")
#		ifdef INCLUDE_ERROR_NAMES
    0
};

#		else /* !INCLUDE_ERROR_NAMES */
    px_error_next
} px_error_t;

#		endif /* (!)INCLUDE_ERROR_NAMES */

#endif /* pxerrors_INCLUDED */
