/* Copyright (C) 2001-2019 Artifex Software, Inc.
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


#ifndef plftable_INCLUDED
#  define plftable_INCLUDED

/* plftable.h */
/* resident font table */
typedef struct font_resident
{
    const char full_font_name[40];      /* name entry 4 in truetype fonts */
    const short unicode_fontname[16];   /* pxl name */
    pl_font_params_t params;
    byte character_complement[8];
    pl_font_type_t font_type;
} font_resident_t;

#endif /* plftable_INCLUDED */

/* #define AGFA_FONT_TABLE */
/* #define AGFA_SCREENFONTS */

#ifdef fontnames

#if 0

#ifdef AGFA_FONT_TABLE
#ifdef AGFA_SCREENFONTS
#define fontnames(agfascreenfontname, agfaname, urwname) agfascreenfontname
#else
#define fontnames(agfascreenfontname, agfaname, urwname) agfaname
#endif
#else
#define fontnames(agfascreenfontname, agfaname, urwname) urwname
#endif
#endif


static const font_resident_t resident_table[] = {
#define C(b) ((byte)((b) ^ 0xff))
#define cc_alphabetic                                                   \
    { C(0), C(0), C(0), C(0), C(0xff), C(0xc0), C(0), C(plgv_Unicode) }
#define cc_symbol                                                       \
    { C(0), C(0), C(0), C(4), C(0), C(0), C(0), C(plgv_Unicode) }
#define cc_dingbats                                                     \
    { C(0), C(0), C(0), C(1), C(0), C(0), C(0), C(plgv_Unicode) }
    /*
     * Per TRM 23-87, PCL5 printers are supposed to have Univers
     * and CG Times fonts.  Substitute Arial for Univers and
     * Times for CG Times.
     */
    /* hack the vendor value to be agfa's. */
#define agfa (4096)
    /* definition for style word as defined on 11-19 PCLTRM */
#define style_word(posture, width, structure)           \
    ((posture) + (4 * (width)) + (32 * (structure)))
#define REGULAR         (style_word(0, 0, 0))
#define ITALIC          (style_word(1, 0, 0))
#define CONDENSEDITALIC (style_word(1, 1, 0))
#define CONDENSED       (style_word(0, 1, 0))
#define LIGHT           (-3)
#define NOBOLD          (0)
#define MEDIUMBOLD      (1)
#define TWOBOLD         (2)
#define BOLD            (3)
#define EXBOLD          (4)
    
    {
        fontnames("Courier", "CourierMT", "NimbusMono-Regular"),
        {'C', 'o', 'u', 'r', 'i', 'e', 'r', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
         ' ', ' '},
        {0, 0, {600.0, 720000.0 / 600.0}, 0, REGULAR, NOBOLD, 4099, 0},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("CGTimes", "CGTimes", "NimbusRomanNo4-Regular"),
        {'C', 'G', ' ', 'T', 'i', 'm', 'e', 's', ' ', ' ', ' ', ' ', ' ', ' ',
         ' ', ' '},
        {0, 1, {295.0, 720000.0 / 295.0}, 0, REGULAR, NOBOLD, 4101, 1},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("CGTimesBold", "CGTimes-Bold", "NimbusRomanNo4-Bold"),
        {'C', 'G', ' ', 'T', 'i', 'm', 'e', 's', ' ', ' ', ' ', ' ', ' ', ' ',
         'B', 'd'},
        {0, 1, {295.0, 720000.0 / 295.0}, 0, REGULAR, BOLD, 4101, 2},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("CGTimesItalic", "CGTimes-Italic", "NimbusRomanNo4-Italic"),
        {'C', 'G', ' ', 'T', 'i', 'm', 'e', 's', ' ', ' ', ' ', ' ', ' ', ' ',
         'I', 't'},
        {0, 1, {295.0, 720000.0 / 295.0}, 0, ITALIC, NOBOLD, 4101, 3},
        cc_alphabetic, plft_Unicode},
    
    {
        fontnames("CGTimesBoldItalic", "CGTimes-BoldItalic",
                  "NimbusRomanNo4-BoldItalic"),
        {'C', 'G', ' ', 'T', 'i', 'm', 'e', 's', ' ', ' ', ' ', ' ', 'B', 'd',
         'I', 't'},
        {0, 1, {295.0, 720000.0 / 295.0}, 0, ITALIC, BOLD, 4101, 4},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("CGOmega", "CGOmega", "URWClassico-Regular"),
        {'C', 'G', ' ', 'O', 'm', 'e', 'g', 'a', ' ', ' ', ' ', ' ', ' ', ' ',
         ' ', ' '},
        {0, 1, {276.0, 720000.0 / 276.0}, 0, REGULAR, NOBOLD, 4113, 5},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("CGOmegaBold", "CGOmega-Bold", "URWClassico-Bold"),
        {'C', 'G', ' ', 'O', 'm', 'e', 'g', 'a', ' ', ' ', ' ', ' ', ' ', ' ',
         'B', 'd'},
        {0, 1, {276.0, 720000.0 / 276.0}, 0, REGULAR, BOLD, 4113, 6},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("CGOmegaItalic", "CGOmega-Italic", "URWClassico-Italic"),
        {'C', 'G', ' ', 'O', 'm', 'e', 'g', 'a', ' ', ' ', ' ', ' ', ' ', ' ',
         'I', 't'},
        {0, 1, {276.0, 720000.0 / 276.0}, 0, ITALIC, NOBOLD, 4113, 7},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("CGOmegaBoldItalic", "CGOmega-BoldItalic",
                  "URWClassico-BoldItalic"),
        {'C', 'G', ' ', 'O', 'm', 'e', 'g', 'a', ' ', ' ', ' ', ' ', 'B', 'd',
         'I', 't'},
        {0, 1, {276.0, 720000.0 / 276.0}, 0, ITALIC, BOLD, 4113, 8},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("Coronet", "Coronet", "C093-Regular"),
        {'C', 'o', 'r', 'o', 'n', 'e', 't', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
         ' ', ' '},
        {0, 1, {203.0, 720000.0 / 203.0}, 0, ITALIC, NOBOLD, 4116, 9},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("ClarendonCondensedBold", "Clarendon-Condensed-Bold",
                  "C011Condensed-Bold"),
        {'C', 'l', 'a', 'r', 'e', 'n', 'd', 'o', 'n', ' ', ' ', ' ', 'C', 'd',
         'B', 'd'},
        {0, 1, {221.0, 720000.0 / 221.0}, 0, CONDENSED, BOLD, 4140, 10},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("UniversMedium", "Univers-Medium", "URWClassicSans-Regular"),
        {'U', 'n', 'i', 'v', 'e', 'r', 's', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
         'M', 'd'},
        {0, 1, {332.0, 720000.0 / 332.0}, 0, REGULAR, NOBOLD, 4148, 11},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("UniversBold", "Univers-Bold", "URWClassicSans-Bold"),
        {'U', 'n', 'i', 'v', 'e', 'r', 's', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
         'B', 'd'},
        {0, 1, {332.0, 720000.0 / 332.0}, 0, REGULAR, BOLD, 4148, 12},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("UniversMediumItalic", "Univers-MediumItalic", "URWClassicSans-RegularItalic"),
        {'U', 'n', 'i', 'v', 'e', 'r', 's', ' ', ' ', ' ', ' ', ' ', 'M', 'd',
         'I', 't'},
        {0, 1, {332.0, 720000.0 / 332.0}, 0, ITALIC, NOBOLD, 4148, 13},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("UniversBoldItalic", "Univers-BoldItalic", "URWClassicSans-BoldItalic"),
        {'U', 'n', 'i', 'v', 'e', 'r', 's', ' ', ' ', ' ', ' ', ' ', 'B', 'd',
         'I', 't'},
        {0, 1, {332.0, 720000.0 / 332.0}, 0, ITALIC, BOLD, 4148, 14},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("UniversCondensedMedium", "Univers-Condensed-Medium",
                  "URWClassicSansCond-Regular"),
        {'U', 'n', 'i', 'v', 'e', 'r', 's', ' ', ' ', ' ', ' ', ' ', 'C', 'd',
         'M', 'd'},
        {0, 1, {221.0, 720000.0 / 221.0}, 0, CONDENSED, NOBOLD, 4148, 15},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("UniversCondensedBold", "Univers-Condensed-Bold",
                  "URWClassicSansCond-Bold"),
        {'U', 'n', 'i', 'v', 'e', 'r', 's', ' ', ' ', ' ', ' ', ' ', 'C', 'd',
         'B', 'd'},
        {0, 1, {221.0, 720000.0 / 221.0}, 0, CONDENSED, BOLD, 4148, 16},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("UniversCondensedMediumItalic",
                  "Univers-Condensed-MediumItalic", "URWClassicSansCond-Italic"),
        {'U', 'n', 'i', 'v', 'e', 'r', 's', ' ', ' ', ' ', 'C', 'd', 'M', 'd',
         'I', 't'},
        {0, 1, {221.0, 720000.0 / 221.0}, 0, CONDENSEDITALIC, NOBOLD, 4148, 17},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("UniversCondensedBoldItalic", "Univers-Condensed-BoldItalic",
                  "URWClassicSansCond-BoldItalic"),
        {'U', 'n', 'i', 'v', 'e', 'r', 's', ' ', ' ', ' ', 'C', 'd', 'B', 'd',
         'I', 't'},
        {0, 1, {221.0, 720000.0 / 221.0}, 0, CONDENSEDITALIC, BOLD, 4148, 18},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("AntiqueOlive", "AntiqueOlive", "AntiqueOlive-Regular"),
        {'A', 'n', 't', 'i', 'q', 'O', 'l', 'i', 'v', 'e', ' ', ' ', ' ', ' ',
         ' ', ' '},
        {0, 1, {295.0, 720000.0 / 295.0}, 0, REGULAR, NOBOLD, 4168, 19},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("AntiqueOliveBold", "AntiqueOlive-Bold", "AntiqueOlive-Bold"),
        {'A', 'n', 't', 'i', 'q', 'O', 'l', 'i', 'v', 'e', ' ', ' ', ' ', ' ',
         'B', 'd'},
        {0, 1, {332.0, 720000.0 / 332.0}, 0, REGULAR, BOLD, 4168, 20},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("AntiqueOliveItalic", "AntiqueOlive-Italic",
                  "AntiqueOlive-Italic"),
        {'A', 'n', 't', 'i', 'q', 'O', 'l', 'i', 'v', 'e', ' ', ' ', ' ', ' ',
         'I', 't'},
        {0, 1, {294.0, 720000.0 / 294.0}, 0, ITALIC, NOBOLD, 4168, 21},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("GaramondAntiqua", "Garamond-Antiqua", "Garamond-Antiqua"),
        {'G', 'a', 'r', 'a', 'm', 'o', 'n', 'd', ' ', 'A', 'n', 't', 'i', 'q',
         'u', 'a'},
        {0, 1, {258.0, 720000.0 / 258.0}, 0, REGULAR, NOBOLD, 4197, 22},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("GaramondHalbfett", "Garamond-Halbfett", "Garamond-Halbfett"),
        {'G', 'a', 'r', 'a', 'm', 'o', 'n', 'd', ' ', ' ', ' ', ' ', ' ', 'H',
         'l', 'b'},
        {0, 1, {276.0, 720000.0 / 276.0}, 0, REGULAR, BOLD, 4197, 23},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("GaramondKursiv", "Garamond-Kursiv", "Garamond-Kursiv"),
        {'G', 'a', 'r', 'a', 'm', 'o', 'n', 'd', ' ', ' ', ' ', ' ', 'K', 'r',
         's', 'v'},
        {0, 1, {240.0, 720000.0 / 240.0}, 0, ITALIC, NOBOLD, 4197, 24},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("GaramondKursivHalbfett", "Garamond-KursivHalbfett",
                  "Garamond-KursivHalbfett"),
        {'G', 'a', 'r', 'a', 'm', 'o', 'n', 'd', ' ', 'K', 'r', 's', 'v', 'H',
         'l', 'b'},
        {0, 1, {258.0, 720000.0 / 258.0}, 0, ITALIC, BOLD, 4197, 25},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("Marigold", "Marigold", "Mauritius-Regular"),
        {'M', 'a', 'r', 'i', 'g', 'o', 'l', 'd', ' ', ' ', ' ', ' ', ' ', ' ',
         ' ', ' '},
        {0, 1, {221.0, 720000.0 / 221.0}, 0, REGULAR, NOBOLD, 4297, 26},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("AlbertusMedium", "Albertus-Medium", "Algiers-Medium"),
        {'A', 'l', 'b', 'e', 'r', 't', 'u', 's', ' ', ' ', ' ', ' ', ' ', ' ',
         'M', 'd'},
        {0, 1, {313.0, 720000.0 / 313.0}, 0, REGULAR, MEDIUMBOLD, 4362, 27},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("AlbertusExtraBold", "Albertus-ExtraBold", "Algiers-ExtraBold"),
        {'A', 'l', 'b', 'e', 'r', 't', 'u', 's', ' ', ' ', ' ', ' ', ' ', ' ',
         'X', 'b'},
        {0, 1, {369.0, 720000.0 / 369.0}, 0, REGULAR, EXBOLD, 4362, 28},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("Arial", "Arial", "NimbusSansNo2-Regular"),
        {'A', 'r', 'i', 'a', 'l', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
         ' ', ' '},
        {0, 1, {278.0, 720000.0 / 278.0}, 0, REGULAR, NOBOLD, 16602, 29},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("Arial-BoldMT", "Arial-Bold", "NimbusSansNo2-Bold"),
        {'A', 'r', 'i', 'a', 'l', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
         'B', 'd'},
        {0, 1, {278.0, 720000.0 / 278.0}, 0, REGULAR, BOLD, 16602, 30},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("Arial-ItalicMT", "Arial-Italic", "NimbusSansNo2-Italic"),
        {'A', 'r', 'i', 'a', 'l', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
         'I', 't'},
        {0, 1, {278.0, 720000.0 / 278.0}, 0, ITALIC, NOBOLD, 16602, 31},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("Arial-BoldItalicMT", "Arial-BoldItalic", "NimbusSansNo2-BoldItalic"),
        {'A', 'r', 'i', 'a', 'l', ' ', ' ', ' ', ' ', ' ', ' ', ' ', 'B', 'd',
         'I', 't'},
        {0, 1, {278.0, 720000.0 / 278.0}, 0, ITALIC, BOLD, 16602, 32},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("TimesNewRoman", "TimesNewRoman", "NimbusRomanNo9-Regular"),
        {'T', 'i', 'm', 'e', 's', 'N', 'e', 'w', 'R', 'm', 'n', ' ', ' ', ' ',
         ' ', ' '},
        {0, 1, {250.0, 720000.0 / 250.0}, 0, REGULAR, NOBOLD, 16901, 33},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("TimesNewRoman-Bold", "TimesNewRoman-Bold",
                  "NimbusRomanNo9-Bold"),
        {'T', 'i', 'm', 'e', 's', 'N', 'e', 'w', 'R', 'm', 'n', ' ', ' ', ' ',
         'B', 'd'},
        {0, 1, {250.0, 720000.0 / 250.0}, 0, REGULAR, BOLD, 16901, 34},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("TimesNewRoman-Italic", "TimesNewRoman-Italic",
                  "NimbusRomanNo9-Italic"),
        {'T', 'i', 'm', 'e', 's', 'N', 'e', 'w', 'R', 'm', 'n', ' ', ' ', ' ',
         'I', 't'},
        {0, 1, {250.0, 720000.0 / 250.0}, 0, ITALIC, NOBOLD, 16901, 36},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("TimesNewRoman-BoldItalic", "TimesNewRoman-BoldItalic",
                  "NimbusRomanNo9-BoldItalic"),
        {'T', 'i', 'm', 'e', 's', 'N', 'e', 'w', 'R', 'm', 'n', ' ', 'B', 'd',
         'I', 't'},
        {0, 1, {250.0, 720000.0 / 250.0}, 0, ITALIC, BOLD, 16901, 35},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("Helvetica", "Helvetica", "NimbusSans-Regular"),
        {'H', 'e', 'l', 'v', 'e', 't', 'i', 'c', 'a', ' ', ' ', ' ', ' ', ' ',
         ' ', ' '},
        {0, 1, {278.0, 720000.0 / 278.0}, 0, REGULAR, NOBOLD, 24580, 37},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("Helvetica-Bold", "Helvetica-Bold", "NimbusSans-Bold"),
        {'H', 'e', 'l', 'v', 'e', 't', 'i', 'c', 'a', ' ', ' ', ' ', ' ', ' ',
         'B', 'd'},
        {0, 1, {278.0, 720000.0 / 278.0}, 0, REGULAR, BOLD, 24580, 38},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("Helvetica-BoldOblique", "Helvetica-BoldOblique",
                  "NimbusSans-BoldOblique"),
        {'H', 'e', 'l', 'v', 'e', 't', 'i', 'c', 'a', ' ', ' ', ' ', 'B', 'd',
         'O', 'b'},
        {0, 1, {278.0, 720000.0 / 278.0}, 0, ITALIC, BOLD, 24580, 40},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("Helvetica-Narrow", "Helvetica-Narrow", "NimbusSansNarrow-Regular"),
        {'H', 'e', 'l', 'v', 'e', 't', 'i', 'c', 'a', ' ', ' ', ' ', ' ', ' ',
         'N', 'r'},
        {0, 1, {228.0, 720000.0 / 228.0}, 0, CONDENSED, NOBOLD, 24580, 41},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("Helvetica-Narrow-Bold", "Helvetica-Narrow-Bold",
                  "NimbusSansNarrow-Bold"),
        {'H', 'e', 'l', 'v', 'e', 't', 'i', 'c', 'a', ' ', ' ', ' ', 'N', 'r',
         'B', 'd'},
        {0, 1, {228.0, 720000.0 / 228.0}, 0, CONDENSED, BOLD, 24580, 42},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("Helvetica-Narrow-BoldOblique", "Helvetica-Narrow-BoldOblique",
                  "NimbusSansNarrow-BoldOblique"),
        {'H', 'e', 'l', 'v', 'e', 't', 'i', 'c', 'a', ' ', 'N', 'r', 'B', 'd',
         'O', 'b'},
        {0, 1, {228.0, 720000.0 / 228.0}, 0, CONDENSEDITALIC, BOLD, 24580, 44},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("Helvetica-Narrow-Oblique", "Helvetica-Narrow-Oblique",
                  "NimbusSansNarrow-Oblique"),
        {'H', 'e', 'l', 'v', 'e', 't', 'i', 'c', 'a', ' ', ' ', ' ', 'N', 'r',
         'O', 'b'},
        {0, 1, {228.0, 720000.0 / 228.0}, 0, CONDENSEDITALIC, NOBOLD, 24580, 43},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("Helvetica-Oblique", "Helvetica-Oblique",
                  "NimbusSans-Oblique"),
        {'H', 'e', 'l', 'v', 'e', 't', 'i', 'c', 'a', ' ', ' ', ' ', ' ', ' ',
         'O', 'b'},
        {0, 1, {278.0, 720000.0 / 278.0}, 0, ITALIC, NOBOLD, 24580, 39},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("Palatino-Roman", "Palatino-Roman", "P052-Roman"),
        {'P', 'a', 'l', 'a', 't', 'i', 'n', 'o', ' ', ' ', ' ', ' ', ' ', 'R',
         'm', 'n'},
        {0, 1, {250.0, 720000.0 / 250.0}, 0, REGULAR, NOBOLD, 24591, 45},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("Palatino-Italic", "Palatino-Italic", "P052-Italic"),
        {'P', 'a', 'l', 'a', 't', 'i', 'n', 'o', ' ', ' ', ' ', ' ', ' ', ' ',
         'I', 't'},
        {0, 1, {250.0, 720000.0 / 250.0}, 0, ITALIC, NOBOLD, 24591, 47},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("Palatino-Bold", "Palatino-Bold", "P052-Bold"),
        {'P', 'a', 'l', 'a', 't', 'i', 'n', 'o', ' ', ' ', ' ', ' ', ' ', ' ',
         'B', 'd'},
        {0, 1, {250.0, 720000.0 / 250.0}, 0, REGULAR, BOLD, 24591, 46},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("Palatino-BoldItalic", "Palatino-BoldItalic",
                  "P052-BoldItalic"),
        {'P', 'a', 'l', 'a', 't', 'i', 'n', 'o', ' ', ' ', ' ', ' ', 'B', 'd',
         'I', 't'},
        {0, 1, {250.0, 720000.0 / 250.0}, 0, ITALIC, BOLD, 24591, 48},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("AvantGarde-Book", "AvantGarde-Book", "URWGothic-Book"),
        {'I', 'T', 'C', 'A', 'v', 'a', 'n', 't', 'G', 'a', 'r', 'd', ' ', ' ',
         'B', 'k'},
        {0, 1, {277.0, 720000.0 / 277.0}, 0, REGULAR, NOBOLD, 24607, 49},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("AvantGarde-Demi", "AvantGarde-Demi", "URWGothic-Demi"),
        {'I', 'T', 'C', 'A', 'v', 'a', 'n', 't', 'G', 'a', 'r', 'd', ' ', ' ',
         'D', 'b'},
        {0, 1, {280.0, 720000.0 / 280.0}, 0, REGULAR, TWOBOLD, 24607, 50},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("AvantGarde-BookOblique", "AvantGarde-BookOblique",
                  "URWGothic-BookOblique"),
        {'I', 'T', 'C', 'A', 'v', 'a', 'n', 't', 'G', 'a', 'r', 'd', 'B', 'k',
         'O', 'b'},
        {0, 1, {277.0, 720000.0 / 277.0}, 0, ITALIC, NOBOLD, 24607, 51},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("AvantGarde-DemiOblique", "AvantGarde-DemiOblique",
                  "URWGothic-DemiOblique"),
        {'I', 'T', 'C', 'A', 'v', 'a', 'n', 't', 'G', 'a', 'r', 'd', 'D', 'b',
         'O', 'b'},
        {0, 1, {280.0, 720000.0 / 280.0}, 0, ITALIC, TWOBOLD, 24607, 52},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("Bookman-Light", "Bookman-Light", "URWBookman-Light"),
        {'I', 'T', 'C', 'B', 'o', 'o', 'k', 'm', 'a', 'n', ' ', ' ', ' ', ' ',
         'L', 't'},
        {0, 1, {320.0, 720000.0 / 320.0}, 0, REGULAR, LIGHT, 24623, 53},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("Bookman-Demi", "Bookman-Demi", "URWBookman-Demi"),
        {'I', 'T', 'C', 'B', 'o', 'o', 'k', 'm', 'a', 'n', ' ', ' ', ' ', ' ',
         'D', 'b'},
        {0, 1, {340.0, 720000.0 / 340.0}, 0, REGULAR, TWOBOLD, 24623, 54},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("Bookman-LightItalic", "Bookman-LightItalic",
                  "URWBookman-LightItalic"),
        {'I', 'T', 'C', 'B', 'o', 'o', 'k', 'm', 'a', 'n', ' ', ' ', 'L', 't',
         'I', 't'},
        {0, 1, {300.0, 720000.0 / 300.0}, 0, ITALIC, LIGHT, 24623, 55},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("Bookman-DemiItalic", "Bookman-DemiItalic",
                  "URWBookman-DemiItalic"),
        {'I', 'T', 'C', 'B', 'o', 'o', 'k', 'm', 'a', 'n', ' ', ' ', 'D', 'b',
         'I', 't'},
        {0, 1, {340.0, 720000.0 / 340.0}, 0, ITALIC, TWOBOLD, 24623, 56},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("NewCenturySchlbk-Bold", "NewCenturySchlbk-Bold",
                  "C059-Bold"),
        {'N', 'w', 'C', 'e', 'n', 't', 'S', 'c', 'h', 'l', 'b', 'k', ' ', ' ',
         'B', 'd'},
        {0, 1, {287.0, 720000.0 / 287.0}, 0, REGULAR, BOLD, 24703, 57},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("NewCenturySchlbk-BoldItalic", "NewCenturySchlbk-BoldItalic",
                  "C059-BdIta"),
        {'N', 'w', 'C', 'e', 'n', 't', 'S', 'c', 'h', 'l', 'b', 'k', 'B', 'd',
         'I', 't'},
        {0, 1, {287.0, 720000.0 / 287.0}, 0, ITALIC, BOLD, 24703, 58},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("NewCenturySchlbk-Italic", "NewCenturySchlbk-Italic",
                  "C059-Italic"),
        {'N', 'w', 'C', 'e', 'n', 't', 'S', 'c', 'h', 'l', 'b', 'k', ' ', ' ',
         'I', 't'},
        {0, 1, {278.0, 720000.0 / 278.0}, 0, ITALIC, NOBOLD, 24703, 59},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("NewCenturySchlbk-Roman", "NewCenturySchlbk-Roman",
                  "C059-Roman"),
        {'N', 'w', 'C', 'e', 'n', 't', 'S', 'c', 'h', 'l', 'b', 'k', ' ', 'R',
         'm', 'n'},
        {0, 1, {278.0, 720000.0 / 278.0}, 0, REGULAR, NOBOLD, 24703, 60},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("Times-Roman", "Times-Roman", "NimbusRoman-Regular"),
        {'T', 'i', 'm', 'e', 's', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', 'R',
         'm', 'n'},
        {0, 1, {250.0, 720000.0 / 250.0}, 0, REGULAR, NOBOLD, 25093, 61},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("Times-Bold", "Times-Bold", "NimbusRoman-Bold"),
        {'T', 'i', 'm', 'e', 's', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
         'B', 'd'},
        {0, 1, {250.0, 720000.0 / 250.0}, 0, REGULAR, BOLD, 25093, 62},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("Times-Italic", "Times-Italic", "NimbusRoman-Italic"),
        {'T', 'i', 'm', 'e', 's', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
         'I', 't'},
        {0, 1, {250.0, 720000.0 / 250.0}, 0, ITALIC, NOBOLD, 25093, 63},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("Times-BoldItalic", "Times-BoldItalic",
                  "NimbusRoman-BoldItalic"),
        {'T', 'i', 'm', 'e', 's', ' ', ' ', ' ', ' ', ' ', ' ', ' ', 'B', 'd',
         'I', 't'},
        {0, 1, {250.0, 720000.0 / 250.0}, 0, ITALIC, BOLD, 25093, 64},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("ZapfChancery-MediumItalic", "ZapfChancery-MediumItalic",
                  "Z003-MediumItalic"),
        {'Z', 'a', 'p', 'f', 'C', 'h', 'a', 'n', 'c', 'e', 'r', 'y', 'M', 'd',
         'I', 't'},
        {0, 1, {220.0, 720000.0 / 220.0}, 0, ITALIC, NOBOLD, 45099, 65},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("SymbolMT", "Symbol", "Symbols"),
        {'S', 'y', 'm', 'b', 'o', 'l', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
         ' ', ' '},
        {621, 1, {250.0, 720000.0 / 250.0}, 0, REGULAR, NOBOLD, 16686, 66},
        cc_symbol, plft_8bit_printable
    },
    
    {
        fontnames("SymbPS", "SymbPS", "StandardSymbolsPS"),
        {'S', 'y', 'm', 'b', 'o', 'l', 'P', 'S', ' ', ' ', ' ', ' ', ' ', ' ',
         ' ', ' '},
        {621, 1, {250.0, 720000.0 / 250.0}, 0, REGULAR, NOBOLD, 45358, 67},
        cc_symbol, plft_8bit_printable
    },
    
    {
        fontnames("Wingdings-Regular", "Wingdings-Regular", "URWDings"),
        {'W', 'i', 'n', 'g', 'd', 'i', 'n', 'g', 's', ' ', ' ', ' ', ' ', ' ',
         ' ', ' '},
        {18540, 1, {1000.0, 720000.0 / 1000.0}, 0, REGULAR, NOBOLD, 31402, 68},
        cc_dingbats, plft_8bit},
    
    {
        fontnames("ZapfDingbats", "ZapfDingbats", "D050000L"),
        {'Z', 'a', 'p', 'f', 'D', 'i', 'n', 'g', 'b', 'a', 't', 's', ' ', ' ',
         ' ', ' '},
        {460, 1, {280.0, 720000.0 / 280.0}, 0, REGULAR, NOBOLD, 45101, 69},
        cc_dingbats, plft_8bit},
    
    {
        fontnames("CourierBold", "CourierMT-Bold", "NimbusMono-Bold"),
        {'C', 'o', 'u', 'r', 'i', 'e', 'r', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
         'B', 'd'},
        {0, 0, {600.0, 720000.0 / 600.0}, 0, REGULAR, BOLD, 4099, 70},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("CourierItalic", "CourierMT-Italic", "NimbusMono-Italic"),
        {'C', 'o', 'u', 'r', 'i', 'e', 'r', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
         'I', 't'},
        {0, 0, {600.0, 720000.0 / 600.0}, 0, ITALIC, NOBOLD, 4099, 71},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("CourierBoldItalic", "CourierMT-BoldItalic",
                  "NimbusMono-BoldItalic"),
        {'C', 'o', 'u', 'r', 'i', 'e', 'r', ' ', ' ', ' ', ' ', ' ', 'B', 'd',
         'I', 't'},
        {0, 0, {600.0, 720000.0 / 600.0}, 0, ITALIC, BOLD, 4099, 72},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("LetterGothic", "LetterGothic", "LetterGothic-Regular"),
        {'L', 'e', 't', 't', 'e', 'r', 'G', 'o', 't', 'h', 'i', 'c', ' ', ' ',
         ' ', ' '},
        {0, 0, {500.0, 720000.0 / 500.0}, 0, REGULAR, NOBOLD, 4102, 73},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("LetterGothicBold", "LetterGothic-Bold", "LetterGothic-Bold"),
        {'L', 'e', 't', 't', 'e', 'r', 'G', 'o', 't', 'h', 'i', 'c', ' ', ' ',
         'B', 'd'},
        {0, 0, {500.0, 720000.0 / 500.0}, 0, REGULAR, BOLD, 4102, 74},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("LetterGothicItalic", "LetterGothic-Italic",
                  "LetterGothic-Italic"),
        {'L', 'e', 't', 't', 'e', 'r', 'G', 'o', 't', 'h', 'i', 'c', ' ', ' ',
         'I', 't'},
        {0, 0, {500.0, 720000.0 / 500.0}, 0, ITALIC, NOBOLD, 4102, 75},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("Courier", "Courier", "NimbusMonoPS-Regular"),
        {'C', 'o', 'u', 'r', 'i', 'e', 'r', 'P', 'S', ' ', ' ', ' ', ' ', ' ',
         ' ', ' '},
        {0, 0, {600.0, 720000.0 / 600.0}, 0, REGULAR, NOBOLD, 24579, 76},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("Courier-Bold", "Courier-Bold", "NimbusMonoPS-Bold"),
        {'C', 'o', 'u', 'r', 'i', 'e', 'r', 'P', 'S', ' ', ' ', ' ', ' ', ' ',
         'B', 'd'},
        {0, 0, {600.0, 720000.0 / 600.0}, 0, REGULAR, BOLD, 24579, 77},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("Courier-BoldOblique", "Courier-BoldOblique",
                  "NimbusMonoPS-BoldItalic"),
        {'C', 'o', 'u', 'r', 'i', 'e', 'r', 'P', 'S', ' ', ' ', ' ', 'B', 'd',
         'O', 'b'},
        {0, 0, {600.0, 720000.0 / 600.0}, 0, ITALIC, BOLD, 24579, 78},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("Courier-Oblique", "Courier-Oblique", "NimbusMonoPS-Italic"),
        {'C', 'o', 'u', 'r', 'i', 'e', 'r', 'P', 'S', ' ', ' ', ' ', ' ', ' ',
         'O', 'b'},
        {0, 0, {600.0, 720000.0 / 600.0}, 0, ITALIC, NOBOLD, 24579, 79},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("noname", "noname", "ArtLinePrinter"),
        {'L', 'i', 'n', 'e', ' ', 'P', 'r', 'i', 'n', 't', 'e', 'r', ' ', ' ',
         '0', 'N'},
        {14, 0, {431.0, 720000.0 / 431.0}, 34, REGULAR, NOBOLD, 0, 80},
        cc_alphabetic, plft_8bit_printable
    },

    {
        fontnames("noname", "noname", "ArtLinePrinter"),
        {'L', 'i', 'n', 'e', ' ', 'P', 'r', 'i', 'n', 't', 'e', 'r', ' ', ' ',
         '6', 'N'},
        {206, 0, {431.0, 720000.0 / 431.0}, 34, REGULAR, NOBOLD, 0, 81},
        cc_alphabetic, plft_8bit_printable
    },

    {
        fontnames("noname", "noname", "ArtLinePrinter"),
        {'L', 'i', 'n', 'e', ' ', 'P', 'r', 'i', 'n', 't', 'e', 'r', ' ', ' ',
         '9', 'N'},
        {302, 0, {431.0, 720000.0 / 431.0}, 34, REGULAR, NOBOLD, 0, 82},
        cc_alphabetic, plft_8bit_printable
    },

    {
        fontnames("noname", "noname", "ArtLinePrinter"),
        {'L', 'i', 'n', 'e', ' ', 'P', 'r', 'i', 'n', 't', 'e', 'r', ' ',
         '1', '0', 'U'},
        {341, 0, {431.0, 720000.0 / 431.0}, 34, REGULAR, NOBOLD, 0, 83},
        cc_alphabetic, plft_8bit_printable
    },


    {
        fontnames("noname", "noname", "ArtLinePrinter"),
        {'L', 'i', 'n', 'e', ' ', 'P', 'r', 'i', 'n', 't', 'e', 'r', ' ', '1',
         '1', 'U'},
        {373, 0, {431.0, 720000.0 / 431.0}, 34, REGULAR, NOBOLD, 0, 84},
        cc_alphabetic, plft_8bit_printable
    },

    {
        fontnames("noname", "noname", "ArtLinePrinter"),
        {'L', 'i', 'n', 'e', ' ', 'P', 'r', 'i', 'n', 't', 'e', 'r', ' ', '1',
         '2', 'U'},
        {405, 0, {431.0, 720000.0 / 431.0}, 34, REGULAR, NOBOLD, 0, 85},
        cc_alphabetic, plft_8bit_printable
    },

    {
        fontnames("noname", "noname", "ArtLinePrinter"),
        {'L', 'i', 'n', 'e', ' ', 'P', 'r', 'i', 'n', 't', 'e', 'r', ' ', ' ',
         '1', 'U'},
        {53, 0, {431.0, 720000.0 / 431.0}, 34, REGULAR, NOBOLD, 0, 86},
        cc_alphabetic, plft_8bit_printable
    },

    {
        fontnames("noname", "noname", "ArtLinePrinter"),
        {'L', 'i', 'n', 'e', ' ', 'P', 'r', 'i', 'n', 't', 'e', 'r', ' ', ' ',
         '2', 'N'},
        {78, 0, {431.0, 720000.0 / 431.0}, 34, REGULAR, NOBOLD, 0, 87},
        cc_alphabetic, plft_8bit_printable
    },

    {
        fontnames("noname", "noname", "ArtLinePrinter"),
        {'L', 'i', 'n', 'e', ' ', 'P', 'r', 'i', 'n', 't', 'e', 'r', ' ', ' ',
         '5', 'N'},
        {174, 0, {431.0, 720000.0 / 431.0}, 34, REGULAR, NOBOLD, 0, 88},
        cc_alphabetic, plft_8bit_printable
    },
    
    {
        fontnames("noname", "noname", "ArtLinePrinter"),
        {'L', 'i', 'n', 'e', ' ', 'P', 'r', 'i', 'n', 't', 'e', 'r', ' ', ' ',
         '8', 'U'},
        {277, 0, {431.0, 720000.0 / 431.0}, 34, REGULAR, NOBOLD, 0, 89},
        cc_alphabetic, plft_8bit_printable
    },

    {
        fontnames("noname", "noname", "ArtLinePrinter"),
        {'L', 'i', 'n', 'e', ' ', 'P', 'r', 'i', 'n', 't', 'e', 'r', ' ', ' ',
         '0', 'U'},
        {21, 0, {431.0, 720000.0 / 431.0}, 34, REGULAR, NOBOLD, 0, 90},
        cc_alphabetic, plft_8bit_printable
    },

    {
        fontnames("noname", "noname", "ArtLinePrinter"),
        {'L', 'i', 'n', 'e', ' ', 'P', 'r', 'i', 'n', 't', 'e', 'r', ' ', ' ',
         '1', 'E'},
        {37, 0, {431.0, 720000.0 / 431.0}, 34, REGULAR, NOBOLD, 0, 91},
        cc_alphabetic, plft_8bit_printable
    },

    {
        fontnames("noname", "noname", "ArtLinePrinter"),
        {'L', 'i', 'n', 'e', ' ', 'P', 'r', 'i', 'n', 't', 'e', 'r', ' ', ' ',
         '1', 'F'},
        {38, 0, {431.0, 720000.0 / 431.0}, 34, REGULAR, NOBOLD, 0, 92},
        cc_alphabetic, plft_8bit_printable
    },

    {
        fontnames("noname", "noname", "ArtLinePrinter"),
        {'L', 'i', 'n', 'e', ' ', 'P', 'r', 'i', 'n', 't', 'e', 'r', ' ', ' ',
         '1', 'G'},
        {39, 0, {431.0, 720000.0 / 431.0}, 34, REGULAR, NOBOLD, 0, 93},
        cc_alphabetic, plft_8bit_printable
    },

    {
        fontnames("noname", "noname", "ArtLinePrinter"),
        {'L', 'i', 'n', 'e', ' ', 'P', 'r', 'i', 'n', 't', 'e', 'r', ' ', ' ',
         '0', 'I'},
        {9, 0, {431.0, 720000.0 / 431.0}, 34, REGULAR, NOBOLD, 0, 94},
        cc_alphabetic, plft_8bit_printable
    },

    {
        fontnames("noname", "noname", "ArtLinePrinter"),
        {'L', 'i', 'n', 'e', ' ', 'P', 'r', 'i', 'n', 't', 'e', 'r', ' ', ' ',
         '0', 'S'},
        {19, 0, {431.0, 720000.0 / 431.0}, 34, REGULAR, NOBOLD, 0, 95},
        cc_alphabetic, plft_8bit_printable
    },

    {
        fontnames("noname", "noname", "ArtLinePrinter"),
        {'L', 'i', 'n', 'e', ' ', 'P', 'r', 'i', 'n', 't', 'e', 'r', ' ', ' ',
         '2', 'S'},
        {83, 0, {431.0, 720000.0 / 431.0}, 34, REGULAR, NOBOLD, 0, 96},
        cc_alphabetic, plft_8bit_printable
    },

    {
        fontnames("noname", "noname", "ArtLinePrinter"),
        {'L', 'i', 'n', 'e', ' ', 'P', 'r', 'i', 'n', 't', 'e', 'r', ' ', ' ',
         '0', 'D'},
        {4, 0, {431.0, 720000.0 / 431.0}, 34, REGULAR, NOBOLD, 0, 97},
        cc_alphabetic, plft_8bit_printable
    },

    {
        fontnames("noname", "noname", "ArtLinePrinter"),
        {'L', 'i', 'n', 'e', ' ', 'P', 'r', 'i', 'n', 't', 'e', 'r', ' ', ' ',
         '1', 'O'},
        {47, 0, {431.0, 720000.0 / 431.0}, 34, REGULAR, NOBOLD, 0, 98},
        cc_alphabetic, plft_8bit_printable
    },

    {
        fontnames("noname", "noname", "ArtLinePrinter"),
        {'L', 'i', 'n', 'e', ' ', 'P', 'r', 'i', 'n', 't', 'e', 'r', ' ', '1',
         '3', 'J'},
        {426, 0, {431.0, 720000.0 / 431.0}, 34, REGULAR, NOBOLD, 0, 99},
        cc_alphabetic, plft_8bit_printable
    },

    {
        fontnames("noname", "noname", "ArtLinePrinter"),
        {'L', 'i', 'n', 'e', ' ', 'P', 'r', 'i', 'n', 't', 'e', 'r', ' ', '1',
         '3', 'U'},
        {437, 0, {431.0, 720000.0 / 431.0}, 34, REGULAR, NOBOLD, 0, 100},
        cc_alphabetic, plft_8bit_printable
    },

    {
        fontnames("noname", "noname", "ArtLinePrinter"),
        {'L', 'i', 'n', 'e', ' ', 'P', 'r', 'i', 'n', 't', 'e', 'r', ' ', ' ',
         '4', 'U'},
        {149, 0, {431.0, 720000.0 / 431.0}, 34, REGULAR, NOBOLD, 0, 101},
        cc_alphabetic, plft_8bit_printable
    },

    {
        fontnames("", "", ""),
        {'0', '0'},
        {0, 0, {0, 0}, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0}, 0
    }
    
#undef C
#undef cc_alphabetic
#undef cc_symbol
#undef cc_dingbats
#undef pitch_1
#undef agfa_value
};

/* a null entry terminates the list, so - 1 */
const int pl_built_in_resident_font_table_count =
    countof(resident_table) - 1;
#endif /* fontnames */
                            
