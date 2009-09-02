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
/*$Id$*/

/* plftable.c */
/* resident font table */
#include "ctype_.h"
#include "gstypes.h"
#include "plfont.h"
#include "plftable.h"

/* The AGFA_FONT_TABLE definition is defined when the system is
   compiled to use the MonoType font scaler.  It can also be defined
   here locally withe AGFA_SCREENFONTS to use Monotype font names with
   the TrueType screen fonts distributed with the UFST. The latter
   configuration uses the artifex fontscaler. NB configuration is
   awkward. */

/* #define AGFA_FONT_TABLE */
/* #define AGFA_SCREENFONTS */

#ifdef AGFA_FONT_TABLE
#ifdef AGFA_SCREENFONTS
#define fontnames(agfascreenfontname, agfaname, urwname) agfascreenfontname
#else
#define fontnames(agfascreenfontname, agfaname, urwname) agfaname
#endif
#else
#define fontnames(agfascreenfontname, agfaname, urwname) urwname
#endif


const font_resident_t resident_table[] = {
#define C(b) ((byte)((b) ^ 0xff))
#define cc_alphabetic\
	  { C(0), C(0), C(0), C(0), C(0xff), C(0xc0), C(0), C(plgv_Unicode) }
#define cc_symbol\
	  { C(0), C(0), C(0), C(4), C(0), C(0), C(0), C(plgv_MSL) }
#define cc_dingbats\
	  { C(0), C(0), C(0), C(1), C(0), C(0), C(0), C(plgv_MSL) }
    /*
     * Per TRM 23-87, PCL5 printers are supposed to have Univers
     * and CG Times fonts.  Substitute Arial for Univers and
     * Times for CG Times.
     */
    /* hack the vendor value to be agfa's. */
#define agfa (4096)
    /* definition for style word as defined on 11-19 PCLTRM */
#define style_word(posture, width, structure) \
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
        fontnames("Courier", "CourierMT", "NimbusMono-Reg"),
        {'C','o','u','r','i','e','r',' ',' ',' ',' ',' ',' ',' ',' ',' '},
        {0, 0, {600.0, 720000.0/600.0}, 0, REGULAR, NOBOLD, 4099, 0},
        cc_alphabetic, plft_Unicode
    },

    {
        fontnames("CGTimes", "CGTimes", "NimbusRomanNo4-Lig"),
        {'C','G',' ','T','i','m','e','s',' ',' ',' ',' ',' ',' ',' ',' '},
        {0, 1, {295.0, 720000.0/295.0}, 0, REGULAR, NOBOLD, 4101, 1},
        cc_alphabetic, plft_Unicode
    },

    {
        fontnames("CGTimesBold", "CGTimes-Bold", "NimbusRomanNo4-Bol"),
        {'C','G',' ','T','i','m','e','s',' ',' ',' ',' ',' ',' ','B','d'},
        {0, 1, {295.0, 720000.0/295.0}, 0, REGULAR, BOLD, 4101, 2},
        cc_alphabetic, plft_Unicode
    },

    {
        fontnames("CGTimesItalic", "CGTimes-Italic", "NimbusRomanNo4-LigIta"),
        {'C','G',' ','T','i','m','e','s',' ',' ',' ',' ',' ',' ','I','t'},
        {0, 1, {295.0, 720000.0/295.0}, 0, ITALIC, NOBOLD, 4101, 3},
        cc_alphabetic, plft_Unicode
    },

    {
        fontnames("CGTimesBoldItalic", "CGTimes-BoldItalic", "NimbusRomanNo4-BolIta"),
        {'C','G',' ','T','i','m','e','s',' ',' ',' ',' ','B','d','I','t'},
        {0, 1, {295.0, 720000.0/295.0}, 0, ITALIC, BOLD, 4101, 4},
        cc_alphabetic, plft_Unicode
    },

    {
        fontnames("CGOmega", "CGOmega", "URWClassico-Reg"),
        {'C','G',' ','O','m','e','g','a',' ',' ',' ',' ',' ',' ',' ',' '},
        {0, 1, {276.0, 720000.0/276.0}, 0, REGULAR, NOBOLD, 4113, 5},
        cc_alphabetic, plft_Unicode
    },

    {
        fontnames("CGOmegaBold", "CGOmega-Bold", "URWClassico-Bol"),
        {'C','G',' ','O','m','e','g','a',' ',' ',' ',' ',' ',' ','B','d'},
        {0, 1, {276.0, 720000.0/276.0}, 0, REGULAR, BOLD, 4113, 6},
        cc_alphabetic, plft_Unicode
    },

    {
        fontnames("CGOmegaItalic", "CGOmega-Italic", "URWClassico-Ita"),
        {'C','G',' ','O','m','e','g','a',' ',' ',' ',' ',' ',' ','I','t'},
        {0, 1, {276.0, 720000.0/276.0}, 0, ITALIC, NOBOLD, 4113, 7},
        cc_alphabetic, plft_Unicode
    },

    {
        fontnames("CGOmegaBoldItalic", "CGOmega-BoldItalic", "URWClassico-BolIta"),
        {'C','G',' ','O','m','e','g','a',' ',' ',' ',' ','B','d','I','t'},
        {0, 1, {276.0, 720000.0/276.0}, 0, ITALIC, BOLD, 4113, 8},
        cc_alphabetic, plft_Unicode
    },

    {
        fontnames("Coronet", "Coronet", "Coronet"),
        {'C','o','r','o','n','e','t',' ',' ',' ',' ',' ',' ',' ',' ',' '},
        {0, 1, {203.0, 720000.0/203.0}, 0, ITALIC, NOBOLD, 4116, 9},
        cc_alphabetic, plft_Unicode
    },

    {
        fontnames("ClarendonCondensedBold", "Clarendon-Condensed-Bold", "ClarendonURW-BolCon"),
        {'C','l','a','r','e','n','d','o','n',' ',' ',' ','C','d','B','d'},
        {0, 1, {221.0, 720000.0/221.0}, 0, CONDENSED, BOLD, 4140, 10},
        cc_alphabetic, plft_Unicode
    },

    {
        fontnames("UniversMedium", "Univers-Medium", "U001-Reg"), 
        {'U','n','i','v','e','r','s',' ',' ',' ',' ',' ',' ',' ','M','d'},
        {0, 1, {332.0, 720000.0/332.0}, 0, REGULAR, NOBOLD, 4148, 11},
        cc_alphabetic, plft_Unicode
    },

    {
        fontnames("UniversBold", "Univers-Bold", "U001-Bol"), 
        {'U','n','i','v','e','r','s',' ',' ',' ',' ',' ',' ',' ','B','d'},
        {0, 1, {332.0, 720000.0/332.0}, 0, REGULAR, BOLD, 4148, 12},
        cc_alphabetic, plft_Unicode
    },

    {
        fontnames("UniversMediumItalic", "Univers-MediumItalic", "U001-Ita"), 
        {'U','n','i','v','e','r','s',' ',' ',' ',' ',' ','M','d','I','t'},
        {0, 1, {332.0, 720000.0/332.0}, 0, ITALIC, NOBOLD, 4148, 13},
        cc_alphabetic, plft_Unicode
    },

    {
        fontnames("UniversBoldItalic", "Univers-BoldItalic", "U001-BolIta"),
        {'U','n','i','v','e','r','s',' ',' ',' ',' ',' ','B','d','I','t'},
        {0, 1, {332.0, 720000.0/332.0}, 0, ITALIC, BOLD, 4148, 14},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("UniversCondensedMedium", "Univers-Condensed-Medium", "U001Con-Reg"), 
        {'U','n','i','v','e','r','s',' ',' ',' ',' ',' ','C','d','M','d'},
        {0, 1, {221.0, 720000.0/221.0}, 0, CONDENSED, NOBOLD, 4148, 15},
        cc_alphabetic, plft_Unicode
    },

    {
        fontnames("UniversCondensedBold", "Univers-Condensed-Bold", "U001Con-Bol"),
        {'U','n','i','v','e','r','s',' ',' ',' ',' ',' ','C','d','B','d'},
        {0, 1, {221.0, 720000.0/221.0}, 0, CONDENSED, BOLD, 4148, 16},
        cc_alphabetic, plft_Unicode
    },

    {
        fontnames("UniversCondensedMediumItalic", "Univers-Condensed-MediumItalic", "U001Con-Ita"), 
        {'U','n','i','v','e','r','s',' ',' ',' ','C','d','M','d','I','t'},
        {0, 1, {221.0, 720000.0/221.0}, 0, CONDENSEDITALIC, NOBOLD, 4148, 17},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("UniversCondensedBoldItalic", "Univers-Condensed-BoldItalic", "U001Con-BolIta"), 
        {'U','n','i','v','e','r','s',' ',' ',' ','C','d','B','d','I','t'},
        {0, 1, {221.0, 720000.0/221.0}, 0, CONDENSEDITALIC, BOLD, 4148, 18},
        cc_alphabetic, plft_Unicode
    },

    {
        fontnames("AntiqueOlive", "AntiqueOlive", "AntiqueOlive-Reg"), 
        {'A','n','t','i','q','O','l','i','v','e',' ',' ',' ',' ',' ',' '},
        {0, 1, {295.0, 720000.0/295.0}, 0, REGULAR, NOBOLD, 4168, 19},
        cc_alphabetic, plft_Unicode
    },

    {
        fontnames("AntiqueOliveBold", "AntiqueOlive-Bold", "AntiqueOlive-Bol"), 
        {'A','n','t','i','q','O','l','i','v','e',' ',' ',' ',' ','B','d'},
        {0, 1, {332.0, 720000.0/332.0}, 0, REGULAR, BOLD, 4168, 20},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("AntiqueOliveItalic", "AntiqueOlive-Italic", "AntiqueOlive-Ita"),
        {'A','n','t','i','q','O','l','i','v','e',' ',' ',' ',' ','I','t'},
        {0, 1, {294.0, 720000.0/294.0}, 0, ITALIC, NOBOLD, 4168, 21},
        cc_alphabetic, plft_Unicode
    },

    {
        fontnames("GaramondAntiqua", "Garamond-Antiqua", "GaramondNo8-Reg"), 
        {'G','a','r','a','m','o','n','d',' ','A','n','t','i','q','u','a'},
        {0, 1, {258.0, 720000.0/258.0}, 0, REGULAR, NOBOLD, 4197, 22},
        cc_alphabetic, plft_Unicode
    },

    {
        fontnames("GaramondHalbfett", "Garamond-Halbfett", "GaramondNo8-Med"),
        {'G','a','r','a','m','o','n','d',' ',' ',' ',' ',' ','H','l','b'},
        {0, 1, {276.0, 720000.0/276.0}, 0, REGULAR, BOLD, 4197, 23},
        cc_alphabetic, plft_Unicode
    },

    {
        fontnames("GaramondKursiv", "Garamond-Kursiv", "GaramondNo8-Ita"),
        {'G','a','r','a','m','o','n','d',' ',' ',' ',' ','K','r','s','v'},
        {0, 1, {240.0, 720000.0/240.0}, 0, ITALIC, NOBOLD, 4197, 24},
        cc_alphabetic, plft_Unicode
    },

    {
        fontnames("GaramondKursivHalbfett", "Garamond-KursivHalbfett", "GaramondNo8-MedIta"), 
        {'G','a','r','a','m','o','n','d',' ','K','r','s','v','H','l','b'},
        {0, 1, {258.0, 720000.0/258.0}, 0, ITALIC, BOLD, 4197, 25},
        cc_alphabetic, plft_Unicode
    },

    {
        fontnames("Marigold", "Marigold", "Mauritius-Reg"), 
        {'M','a','r','i','g','o','l','d',' ',' ',' ',' ',' ',' ',' ',' '},
        {0, 1, {221.0, 720000.0/221.0}, 0, REGULAR, NOBOLD, 4297, 26},
        cc_alphabetic, plft_Unicode
    },

    {
        fontnames("AlbertusMedium", "Albertus-Medium", "A028-Med"), 
        {'A','l','b','e','r','t','u','s',' ',' ',' ',' ',' ',' ','M','d'},
        {0, 1, {313.0, 720000.0/313.0}, 0, REGULAR, MEDIUMBOLD, 4362, 27},
        cc_alphabetic, plft_Unicode
    },

    {
        fontnames("AlbertusExtraBold", "Albertus-ExtraBold", "A028-Ext"), 
        {'A','l','b','e','r','t','u','s',' ',' ',' ',' ',' ',' ','X','b'},
        {0, 1, {369.0, 720000.0/369.0}, 0, REGULAR, EXBOLD, 4362, 28},
        cc_alphabetic, plft_Unicode
    },

    {
        fontnames("Arial", "Arial", "A030-Reg"), 
        {'A','r','i','a','l',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' '},
        {0, 1, {278.0, 720000.0/278.0}, 0, REGULAR, NOBOLD, 16602, 29},
        cc_alphabetic, plft_Unicode
    },

    {
        fontnames("Arial-BoldMT", "Arial-Bold", "A030-Bol"), 
        {'A','r','i','a','l',' ',' ',' ',' ',' ',' ',' ',' ',' ','B','d'},
        {0, 1, {278.0, 720000.0/278.0}, 0, REGULAR, BOLD, 16602, 30},
        cc_alphabetic, plft_Unicode
    },

    {
        fontnames("Arial-ItalicMT", "Arial-Italic", "A030-Ita"), 
        {'A','r','i','a','l',' ',' ',' ',' ',' ',' ',' ',' ',' ','I','t'},
        {0, 1, {278.0, 720000.0/278.0}, 0, ITALIC, NOBOLD, 16602, 31},
        cc_alphabetic, plft_Unicode
    },

    {
        fontnames("Arial-BoldItalicMT", "Arial-BoldItalic", "A030-BolIta"),
        {'A','r','i','a','l',' ',' ',' ',' ',' ',' ',' ','B','d','I','t'},
        {0, 1, {278.0, 720000.0/278.0}, 0, ITALIC, BOLD, 16602, 32},
        cc_alphabetic, plft_Unicode
    },

    {
        fontnames("TimesNewRoman", "TimesNewRoman", "NimbusRomanNo9-Reg"), 
        {'T','i','m','e','s','N','e','w','R','m','n',' ',' ',' ',' ',' '},
        {0, 1, {250.0, 720000.0/250.0}, 0, REGULAR, NOBOLD, 16901, 33},
        cc_alphabetic, plft_Unicode
    },

    {
        fontnames("TimesNewRoman-Bold", "TimesNewRoman-Bold", "NimbusRomanNo9-Med"),
        {'T','i','m','e','s','N','e','w','R','m','n',' ',' ',' ','B','d'},
        {0, 1, {250.0, 720000.0/250.0}, 0, REGULAR, BOLD, 16901, 34},
        cc_alphabetic, plft_Unicode
    },

    {
        fontnames("TimesNewRoman-Italic", "TimesNewRoman-Italic", "NimbusRomanNo9-Ita"),
        {'T','i','m','e','s','N','e','w','R','m','n',' ',' ',' ','I','t'},
        {0, 1, {250.0, 720000.0/250.0}, 0, ITALIC, NOBOLD, 16901, 36},
        cc_alphabetic, plft_Unicode
    },

    {
        fontnames("TimesNewRoman-BoldItalic", "TimesNewRoman-BoldItalic", "NimbusRomanNo9-MedIta"), 
        {'T','i','m','e','s','N','e','w','R','m','n',' ','B','d','I','t'},
        {0, 1, {250.0, 720000.0/250.0}, 0, ITALIC, BOLD, 16901, 35},
        cc_alphabetic, plft_Unicode
    },

    {
        fontnames("Helvetica", "Helvetica", "NimbusSanL-Regu"), 
        {'H','e','l','v','e','t','i','c','a',' ',' ',' ',' ',' ',' ',' '},
        {0, 1, {278.0, 720000.0/278.0}, 0, REGULAR, NOBOLD, 24580, 37},
        cc_alphabetic, plft_Unicode
    },

    {
        fontnames("Helvetica-Bold", "Helvetica-Bold", "NimbusSanL-Bold"), 
        {'H','e','l','v','e','t','i','c','a',' ',' ',' ',' ',' ','B','d'},
        {0, 1, {278.0, 720000.0/278.0}, 0, REGULAR, BOLD, 24580, 38},
        cc_alphabetic, plft_Unicode
    },

    {
        fontnames("Helvetica-BoldOblique", "Helvetica-BoldOblique", "NimbusSanL-BoldItal"),
        {'H','e','l','v','e','t','i','c','a',' ',' ',' ','B','d','O','b'},
        {0, 1, {278.0, 720000.0/278.0}, 0, ITALIC, BOLD, 24580, 40},
        cc_alphabetic, plft_Unicode
    },

    {
        fontnames("Helvetica-Narrow", "Helvetica-Narrow", "NimbusSanL-ReguCond"),
        {'H','e','l','v','e','t','i','c','a',' ',' ',' ',' ',' ','N','r'},
        {0, 1, {228.0, 720000.0/228.0}, 0, CONDENSED, NOBOLD, 24580, 41},
        cc_alphabetic, plft_Unicode
    },

    {
        fontnames("Helvetica-Narrow-Bold", "Helvetica-Narrow-Bold", "NimbusSanL-BoldCond"),
        {'H','e','l','v','e','t','i','c','a',' ',' ',' ','N','r','B','d'},
        {0, 1, {228.0, 720000.0/228.0}, 0, CONDENSED, BOLD, 24580, 42},
        cc_alphabetic, plft_Unicode
    },

    {
        fontnames("Helvetica-Narrow-BoldOblique", "Helvetica-Narrow-BoldOblique", "NimbusSanL-BoldCondItal"), 
        {'H','e','l','v','e','t','i','c','a',' ','N','r','B','d','O','b'},
        {0, 1, {228.0, 720000.0/228.0}, 0, CONDENSEDITALIC, BOLD, 24580, 44},
        cc_alphabetic, plft_Unicode
    },

    {
        fontnames("Helvetica-Narrow-Oblique", "Helvetica-Narrow-Oblique", "NimbusSanL-ReguCondItal"), 
        {'H','e','l','v','e','t','i','c','a',' ',' ',' ','N','r','O','b'},
        {0, 1, {228.0, 720000.0/228.0}, 0, CONDENSEDITALIC, NOBOLD, 24580, 43},
        cc_alphabetic, plft_Unicode
    },

    {
        fontnames("Helvetica-Oblique", "Helvetica-Oblique", "NimbusSanL-ReguItal"),
        {'H','e','l','v','e','t','i','c','a',' ',' ',' ',' ',' ','O','b'},
        {0, 1, {278.0, 720000.0/278.0}, 0, ITALIC, NOBOLD, 24580, 39},
        cc_alphabetic, plft_Unicode
    },

    {
        fontnames("Palatino-Roman", "Palatino-Roman", "URWPalladioL-Roma"),
        {'P','a','l','a','t','i','n','o',' ',' ',' ',' ',' ','R','m','n'},
        {0, 1, {250.0, 720000.0/250.0}, 0, REGULAR, NOBOLD, 24591, 45},
        cc_alphabetic, plft_Unicode
    },

    {
        fontnames("Palatino-Italic", "Palatino-Italic", "URWPalladioL-Ital"), 
        {'P','a','l','a','t','i','n','o',' ',' ',' ',' ',' ',' ','I','t'},
        {0, 1, {250.0, 720000.0/250.0}, 0, ITALIC, NOBOLD, 24591, 47},
        cc_alphabetic, plft_Unicode
    },

    {
        fontnames("Palatino-Bold", "Palatino-Bold", "URWPalladioL-Bold"), 
        {'P','a','l','a','t','i','n','o',' ',' ',' ',' ',' ',' ','B','d'},
        {0, 1, {250.0, 720000.0/250.0}, 0, REGULAR, BOLD, 24591, 46},
        cc_alphabetic, plft_Unicode
    },

    {
        fontnames("Palatino-BoldItalic", "Palatino-BoldItalic", "URWPalladioL-BoldItal"), 
        {'P','a','l','a','t','i','n','o',' ',' ',' ',' ','B','d','I','t'},
        {0, 1, {250.0, 720000.0/250.0}, 0, ITALIC, BOLD, 24591, 48},
        cc_alphabetic, plft_Unicode
    },

    {
        fontnames("AvantGarde-Book", "AvantGarde-Book", "URWGothicL-Book"),
        {'I','T','C','A','v','a','n','t','G','a','r','d',' ',' ','B','k'},
        {0, 1, {277.0, 720000.0/277.0}, 0, REGULAR, NOBOLD, 24607, 49},
        cc_alphabetic, plft_Unicode
    },

    {
        fontnames("AvantGarde-Demi", "AvantGarde-Demi", "URWGothicL-Demi"),
        {'I','T','C','A','v','a','n','t','G','a','r','d',' ',' ','D','b'},
        {0, 1, {280.0, 720000.0/280.0}, 0, REGULAR, TWOBOLD, 24607, 50},
        cc_alphabetic, plft_Unicode
    },

    {
        fontnames("AvantGarde-BookOblique", "AvantGarde-BookOblique", "URWGothicL-BookObli"),
        {'I','T','C','A','v','a','n','t','G','a','r','d','B','k','O','b'},
        {0, 1, {277.0, 720000.0/277.0}, 0, ITALIC, NOBOLD, 24607, 51},
        cc_alphabetic, plft_Unicode
    },

    {
        fontnames("AvantGarde-DemiOblique", "AvantGarde-DemiOblique", "URWGothicL-DemiObli"),
        {'I','T','C','A','v','a','n','t','G','a','r','d','D','b','O','b'},
        {0, 1, {280.0, 720000.0/280.0}, 0, ITALIC, TWOBOLD, 24607, 52},
        cc_alphabetic, plft_Unicode
    },

    {
        fontnames("Bookman-Light", "Bookman-Light", "URWBookmanL-Ligh"),
        {'I','T','C','B','o','o','k','m','a','n',' ',' ',' ',' ','L','t'},
        {0, 1, {320.0, 720000.0/320.0}, 0, REGULAR, LIGHT, 24623, 53},
        cc_alphabetic, plft_Unicode
    },

    {
        fontnames("Bookman-Demi", "Bookman-Demi", "URWBookmanL-DemiBold"),
        {'I','T','C','B','o','o','k','m','a','n',' ',' ',' ',' ','D','b'},
        {0, 1, {340.0, 720000.0/340.0}, 0, REGULAR, TWOBOLD, 24623, 54},
        cc_alphabetic, plft_Unicode
    },

    {
        fontnames("Bookman-LightItalic", "Bookman-LightItalic", "URWBookmanL-LighItal"),
        {'I','T','C','B','o','o','k','m','a','n',' ',' ','L','t','I','t'},
        {0, 1, {300.0, 720000.0/300.0}, 0, ITALIC, LIGHT, 24623, 55},
        cc_alphabetic, plft_Unicode
    },

    {
        fontnames("Bookman-DemiItalic", "Bookman-DemiItalic", "URWBookmanL-DemiBoldItal"),
        {'I','T','C','B','o','o','k','m','a','n',' ',' ','D','b','I','t'},
        {0, 1, {340.0, 720000.0/340.0}, 0, ITALIC, TWOBOLD, 24623, 56},
        cc_alphabetic, plft_Unicode
    },

    {
        fontnames("NewCenturySchlbk-Bold", "NewCenturySchlbk-Bold", "CenturySchL-Bold"), 
        {'N','w','C','e','n','t','S','c','h','l','b','k',' ',' ','B','d'},
        {0, 1, {287.0, 720000.0/287.0}, 0, REGULAR, BOLD, 24703, 58},
        cc_alphabetic, plft_Unicode
    },

    {
        fontnames("NewCenturySchlbk-BoldItalic", "NewCenturySchlbk-BoldItalic", "CenturySchL-BoldItal"), 
        {'N','w','C','e','n','t','S','c','h','l','b','k','B','d','I','t'},
        {0, 1, {287.0, 720000.0/287.0}, 0, ITALIC, BOLD, 24703, 60},
        cc_alphabetic, plft_Unicode
    },

    {
        fontnames("NewCenturySchlbk-Italic", "NewCenturySchlbk-Italic", "CenturySchL-Ital"), 
        {'N','w','C','e','n','t','S','c','h','l','b','k',' ',' ','I','t'},
        {0, 1, {278.0, 720000.0/278.0}, 0, ITALIC, NOBOLD, 24703, 59},
        cc_alphabetic, plft_Unicode
    },

    {
        fontnames("NewCenturySchlbk-Roman", "NewCenturySchlbk-Roman", "CenturySchL-Roma"), 
        {'N','w','C','e','n','t','S','c','h','l','b','k',' ','R','m','n'},
        {0, 1, {278.0, 720000.0/278.0}, 0, REGULAR, NOBOLD, 24703, 57},
        cc_alphabetic, plft_Unicode
    },

    {
        fontnames("Times-Roman", "Times-Roman", "NimbusRomNo9L-Regu"),
        {'T','i','m','e','s',' ',' ',' ',' ',' ',' ',' ',' ','R','m','n'},
        {0, 1, {250.0, 720000.0/250.0}, 0, REGULAR, NOBOLD, 25093, 61},
        cc_alphabetic, plft_Unicode
    },

    {
        fontnames("Times-Bold", "Times-Bold", "NimbusRomNo9L-Medi"),
        {'T','i','m','e','s',' ',' ',' ',' ',' ',' ',' ',' ',' ','B','d'},
        {0, 1, {250.0, 720000.0/250.0}, 0, REGULAR, BOLD, 25093, 62},
        cc_alphabetic, plft_Unicode
    },

    {
        fontnames("Times-Italic", "Times-Italic", "NimbusRomNo9L-ReguItal"), 
        {'T','i','m','e','s',' ',' ',' ',' ',' ',' ',' ',' ',' ','I','t'},
        {0, 1, {250.0, 720000.0/250.0}, 0, ITALIC, NOBOLD, 25093, 63},
        cc_alphabetic, plft_Unicode
    },

    {
        fontnames("Times-BoldItalic", "Times-BoldItalic", "NimbusRomNo9L-MediItal"), 
        {'T','i','m','e','s',' ',' ',' ',' ',' ',' ',' ','B','d','I','t'},
        {0, 1, {250.0, 720000.0/250.0}, 0, ITALIC, BOLD, 25093, 64},
        cc_alphabetic, plft_Unicode
    },

    {
        fontnames("ZapfChancery-MediumItalic", "ZapfChancery-MediumItalic", "URWChanceryL-MediItal"),
        {'Z','a','p','f','C','h','a','n','c','e','r','y','M','d','I','t'},
        {0, 1, {220.0, 720000.0/220.0},0, ITALIC, NOBOLD, 45099, 65}, 
        cc_alphabetic, plft_Unicode
    },
 
    {
        fontnames("SymbolMT", "Symbol", "StandardSymL"),
        {'S','y','m','b','o','l',' ',' ',' ',' ',' ',' ',' ',' ',' ',' '},
        {621, 1, {250.0, 720000.0/250.0}, 0, REGULAR, NOBOLD, 16686, 66},
        cc_symbol, plft_8bit_printable
    },

    /* NB Symbol - Symbol PS for URW are the same.  Adding the
       confusion AGFA has 2 different fonts presumably Symbol and
       SymbolPS, each called Symbol. */
    {
        fontnames("SymbPS", "SymbPS", "StandardSymL"),
        {'S','y','m','b','o','l','P','S',' ',' ',' ',' ',' ',' ',' ',' '},
        {621, 1, {250.0, 720000.0/250.0}, 0, REGULAR, NOBOLD, 45358, 67},
        cc_symbol, plft_8bit_printable
    },

    {
        fontnames("Wingdings-Regular", "Wingdings-Regular", "WingSub"),
        {'W','i','n','g','d','i','n','g','s',' ',' ',' ',' ',' ',' ',' '},
        {18540, 1, {1000.0, 720000.0/1000.0},0, REGULAR, NOBOLD, 31402, 68},
        cc_dingbats, plft_8bit
    },

    {
        fontnames("ZapfDingbats", "ZapfDingbats", "Dingbats"), 
        {'Z','a','p','f','D','i','n','g','b','a','t','s',' ',' ',' ',' '},
        {460, 1, {280.0, 720000.0/280.0},0, REGULAR, NOBOLD, 45101, 69},
        cc_dingbats, plft_8bit
    },

    {
        fontnames("CourierBold", "CourierMT-Bold", "NimbusMono-Bol"), 
        {'C','o','u','r','i','e','r',' ',' ',' ',' ',' ',' ',' ','B','d'},
        {0, 0, {600.0, 720000.0/600.0}, 0, REGULAR, BOLD, 4099, 70}, 
        cc_alphabetic, plft_Unicode 
    },


    {
        fontnames("CourierItalic", "CourierMT-Italic", "NimbusMono-Ita"),
        {'C','o','u','r','i','e','r',' ',' ',' ',' ',' ',' ',' ','I','t'},
        {0, 0, {600.0, 720000.0/600.0}, 0, ITALIC, NOBOLD, 4099, 71},
        cc_alphabetic, plft_Unicode 
    },

    {
        fontnames("CourierBoldItalic", "CourierMT-BoldItalic", "NimbusMono-BolIta"),
        {'C','o','u','r','i','e','r',' ',' ',' ',' ',' ','B','d','I','t'},
        {0, 0, {600.0, 720000.0/600.0}, 0, ITALIC, BOLD, 4099, 72},
        cc_alphabetic, plft_Unicode
    },

    {
        fontnames("LetterGothic", "LetterGothic", "LetterGothic-Reg"), 
        {'L','e','t','t','e','r','G','o','t','h','i','c',' ',' ',' ',' '},
        {0, 0, {500.0, 720000.0/500.0}, 0, REGULAR, NOBOLD, 4102, 73},
        cc_alphabetic, plft_Unicode
    },
    
    {
        fontnames("LetterGothicBold", "LetterGothic-Bold", "LetterGothic-Bol"),
        {'L','e','t','t','e','r','G','o','t','h','i','c',' ',' ','B','d'},
        {0, 0, {500.0, 720000.0/500.0}, 0, REGULAR, BOLD, 4102, 74},
        cc_alphabetic, plft_Unicode
    },

    {
        fontnames("LetterGothicItalic", "LetterGothic-Italic", "LetterGothic-Ita"),
        {'L','e','t','t','e','r','G','o','t','h','i','c',' ',' ','I','t'},
        {0, 0, {500.0, 720000.0/500.0}, 0, ITALIC, NOBOLD, 4102, 75},
        cc_alphabetic, plft_Unicode
    },


    {
        fontnames("Courier", "Courier", "NimbusMonL-Regu"),
        {'C','o','u','r','i','e','r','P','S',' ',' ',' ',' ',' ',' ',' '},
        {0, 0, {600.0, 720000.0/600.0}, 0, REGULAR, NOBOLD, 24579, 76},
        cc_alphabetic, plft_Unicode
    },

    {
        fontnames("Courier-Bold", "Courier-Bold", "NimbusMonL-Bold"),
        {'C','o','u','r','i','e','r','P','S',' ',' ',' ',' ',' ','B','d'},
        {0, 0, {600.0, 720000.0/600.0}, 0, REGULAR, BOLD, 24579, 77},
        cc_alphabetic, plft_Unicode
    },

    {
        fontnames("Courier-BoldOblique", "Courier-BoldOblique", "NimbusMonL-BoldObli"),
        {'C','o','u','r','i','e','r','P','S',' ',' ',' ','B','d','O','b'},
        {0, 0, {600.0, 720000.0/600.0}, 0, ITALIC, BOLD, 24579, 79},
        cc_alphabetic, plft_Unicode
    },

    {
        fontnames("Courier-Oblique", "Courier-Oblique", "NimbusMonL-ReguObli"),
        {'C','o','u','r','i','e','r','P','S',' ',' ',' ',' ',' ','O','b'},
        {0, 0, {600.0, 720000.0/600.0}, 0, ITALIC, NOBOLD, 24579, 78},
        cc_alphabetic, plft_Unicode
    },

    /************** NB SEMI-WRONG the artifex lineprinter is unbound ****************/
    {fontnames("noname", "noname", "ArtLinePrinter"), {'L','i','n','e',' ','P','r','i','n','t','e','r',' ',' ','0','N'},
     {0, 0, {431.0, 720000.0/431.0}, 34, REGULAR, NOBOLD, 0, 82},
     cc_alphabetic, plft_Unicode},
    {fontnames("noname", "noname", "ArtLinePrinter"), {'L','i','n','e',' ','P','r','i','n','t','e','r',' ',' ','6','N'},
     {0, 0, {431.0, 720000.0/431.0}, 34, REGULAR, NOBOLD, 0, 88}, cc_alphabetic, plft_Unicode},
    {fontnames("noname", "noname", "ArtLinePrinter"), {'L','i','n','e',' ','P','r','i','n','t','e','r',' ',' ','9','N'},
     {0, 0, {431.0, 720000.0/431.0}, 34, REGULAR, NOBOLD, 0, 89}, cc_alphabetic, plft_Unicode},
    {fontnames("noname", "noname", "ArtLinePrinter"), {'L','i','n','e',' ','P','r','i','n','t','e','r',' ','1','0','U'},
     {0, 0, {431.0, 720000.0/431.0}, 34, REGULAR, NOBOLD, 0, 80}, cc_alphabetic, plft_Unicode},
    {fontnames("noname", "noname", "ArtLinePrinter"), {'L','i','n','e',' ','P','r','i','n','t','e','r',' ','1','1','U'},
     {0, 0, {431.0, 720000.0/431.0}, 34, REGULAR, NOBOLD, 0, 83}, cc_alphabetic, plft_Unicode},
    {fontnames("noname", "noname", "ArtLinePrinter"), {'L','i','n','e',' ','P','r','i','n','t','e','r',' ','1','2','U'},
     {0, 0, {431.0, 720000.0/431.0}, 34, REGULAR, NOBOLD, 0, 84}, cc_alphabetic, plft_Unicode},
    {fontnames("noname", "noname", "ArtLinePrinter"), {'L','i','n','e',' ','P','r','i','n','t','e','r',' ',' ','1','U'},
     {0, 0, {431.0, 720000.0/431.0}, 34, REGULAR, NOBOLD, 0, 85}, cc_alphabetic, plft_Unicode},
    {fontnames("noname", "noname", "ArtLinePrinter"), {'L','i','n','e',' ','P','r','i','n','t','e','r',' ',' ','2','N'},
     {0, 0, {431.0, 720000.0/431.0}, 34, REGULAR, NOBOLD, 0, 86}, cc_alphabetic, plft_Unicode},
    {fontnames("noname", "noname", "ArtLinePrinter"), {'L','i','n','e',' ','P','r','i','n','t','e','r',' ',' ','5','N'},
     {0, 0, {431.0, 720000.0/431.0}, 34, REGULAR, NOBOLD, 0, 87}, cc_alphabetic, plft_Unicode},
    {fontnames("noname", "noname", "ArtLinePrinter"), {'L','i','n','e',' ','P','r','i','n','t','e','r',' ',' ','8','U'},
     {0, 0, {431.0, 720000.0/431.0}, 34, REGULAR, NOBOLD, 0, 81}, cc_alphabetic, plft_Unicode},
    {fontnames("","", ""), {'0','0'},
     {0, 0, {0, 0}, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}, 0}
#undef C
#undef cc_alphabetic
#undef cc_symbol
#undef cc_dingbats
#undef pitch_1
#undef agfa_value
};
