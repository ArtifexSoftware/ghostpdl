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
/*$Id: */

/* plftable.c */
/* resident font table */
#include "ctype_.h"
#include "gstypes.h"
#include "plfont.h"
#include "plftable.h"



#ifdef AGFA_FONT_TABLE
#define fontnames(agfaname, urwname) agfaname
#else
#define fontnames(agfaname, urwname) urwname
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
    /* the actual typeface number is vendor + base value.  Base
       values are found in the pcl 5 Comparison guide - Appendix
       C-6.  We can add a parameter for vendor if necessary, for
       now it is agfa. */
#define face_val(base_value, vendor) (vendor + (base_value))
    /* definition for style word as defined on 11-19 PCLTRM */
#define style_word(posture, width, structure) \
	  ((posture) + (4 * (width)) + (32 * (structure)))
#define REGULAR         (style_word(0, 0, 0))
#define ITALIC          (style_word(1, 0, 0))
#define CONDENSEDITALIC (style_word(1, 1, 0))
#define CONDENSED       (style_word(0, 1, 0))
#define NOBOLD          (0)
#define MEDIUMBOLD      (1)
#define BOLD            (3)
#define EXBOLD          (4)

    {
        fontnames("Courier", "NimbusMono"),
        {'C','o','u','r','i','e','r',' ',' ',' ',' ',' ',' ',' ',' ',' '},
        {0, 0, {60, 11998}, 0, REGULAR, NOBOLD, 4099}, cc_alphabetic
    },

    {
        fontnames("CG Times", "NimbusRomanNo4 Light"),
        {'C','G',' ','T','i','m','e','s',' ',' ',' ',' ',' ',' ',' ',' '},
        {0, 1, {29, 24413}, 0, REGULAR, NOBOLD, 4101},
        cc_alphabetic
    },

    {
        fontnames("CG Times Bold", "NimbusRomanNo4 Bold"),
        {'C','G',' ','T','i','m','e','s',' ',' ',' ',' ',' ',' ','B','d'},
        {0, 1, {29, 24413}, 0, REGULAR, BOLD, 4101},
        cc_alphabetic
    },

    {
        fontnames("CG Times Italic", "NimbusRomanNo4 Light Italic"),
        {'C','G',' ','T','i','m','e','s',' ',' ',' ',' ',' ',' ','I','t'},
        {0, 1, {29, 24413}, 0, ITALIC, NOBOLD, 4101},
        cc_alphabetic
    },

    {
        fontnames("CG Times Bold Italic", "NimbusRomanNo4 Bold Italic"),
        {'C','G',' ','T','i','m','e','s',' ',' ',' ',' ','B','d','I','t'},
        {0, 1, {29, 24413}, 0, ITALIC, BOLD, 4101},
        cc_alphabetic
    },

    {
        fontnames("CG Omega", "URWClassico"),
        {'C','G',' ','O','m','e','g','a',' ',' ',' ',' ',' ',' ',' ',' '},
        {0, 1, {27, 26052}, 0, REGULAR, NOBOLD, 4113},
        cc_alphabetic
    },

    {
        fontnames("CG Omega Bold", "URWClassico Bold"),
        {'C','G',' ','O','m','e','g','a',' ',' ',' ',' ',' ',' ','B','d'},
        {0, 1, {27, 26052}, 0, REGULAR, BOLD, 4113},
        cc_alphabetic
    },

    {
        fontnames("CG Omega Italic", "URWClassico Italic"),
        {'C','G',' ','O','m','e','g','a',' ',' ',' ',' ',' ',' ','I','t'},
        {0, 1, {27, 26052}, 0, ITALIC, NOBOLD, 4113},
        cc_alphabetic
    },

    {
        fontnames("CG Omega Bold Italic", "URWClassico Bold Italic"),
        {'C','G',' ','O','m','e','g','a',' ',' ',' ',' ','B','d','I','t'},
        {0, 1, {27, 26052}, 0, ITALIC, BOLD, 4113},
        cc_alphabetic
    },

    {
        fontnames("Coronet", "Coronet"),
        {'C','o','r','o','n','e','t',' ',' ',' ',' ',' ',' ',' ',' ',' '},
        {0, 1, {20, 35531}, 0, ITALIC, NOBOLD, 4116},
        cc_alphabetic
    },

    {
        fontnames("Clarendon Condensed Bold", "ClarendonURWBolCon"),
        {'C','l','a','r','e','n','d','o','n',' ',' ',' ','C','d','B','d'},
        {0, 1, {22, 32550}, 0, CONDENSED, BOLD, 4140},
        cc_alphabetic
    },

    {
        fontnames("Univers Medium", "U001"), 
        {'U','n','i','v','e','r','s',' ',' ',' ',' ',' ',' ',' ','M','d'},
        {0, 1, {33, 21684}, 0, REGULAR, NOBOLD, 4148},
        cc_alphabetic
    },

    {
        fontnames("Univers Bold", "U001 Bold"), 
        {'U','n','i','v','e','r','s',' ',' ',' ',' ',' ',' ',' ','B','d'},
        {0, 1, {33, 21684}, 0, REGULAR, BOLD, 4148},
        cc_alphabetic
    },

    {
        fontnames("Univers Medium Italic", "U001 Italic"), 
        {'U','n','i','v','e','r','s',' ',' ',' ',' ',' ','M','d','I','t'},
        {0, 1, {33, 21684}, 0, ITALIC, NOBOLD, 4148},
        cc_alphabetic
    },

    {
        fontnames("Univers Bold Italic", "U001 Bold Italic"),
        {'U','n','i','v','e','r','s',' ',' ',' ',' ',' ','B','d','I','t'},
        {0, 1, {33, 21684}, 0, ITALIC, BOLD, 4148},
        cc_alphabetic
    },
    
    {
        fontnames("Univers Condensed Medium", "U001Con"), 
        {'U','n','i','v','e','r','s',' ',' ',' ',' ',' ','C','d','M','d'},
        {0, 1, {22, 32550}, 0, CONDENSED, NOBOLD, 4148},
        cc_alphabetic
    },

    {
        fontnames("Univers Condensed Bold", "U001Con Bold"),
        {'U','n','i','v','e','r','s',' ',' ',' ',' ',' ','C','d','B','d'},
        {0, 1, {22, 32550}, 0, CONDENSED, BOLD, 4148},
        cc_alphabetic
    },

    {
        fontnames("Univers Condensed Medium Italic", "U001Con Italic"), 
        {'U','n','i','v','e','r','s',' ',' ',' ','C','d','M','d','I','t'},
        {0, 1, {22, 32550}, 0, CONDENSEDITALIC, NOBOLD, 4148},
        cc_alphabetic
    },
    
    {
        fontnames("Univers Condensed Bold Italic", "U001Con Bold Italic"), 
        {'U','n','i','v','e','r','s',' ',' ',' ','C','d','B','d','I','t'},
        {0, 1, {22, 32550}, 0, CONDENSEDITALIC, BOLD, 4148},
        cc_alphabetic
    },

    {
        fontnames("Antique Olive", "AntiqueOlive"), 
        {'A','n','t','i','q','O','l','i','v','e',' ',' ',' ',' ',' ',' '},
        {0, 1, {29, 24413}, 0, REGULAR, NOBOLD, 4168},
        cc_alphabetic
    },

    {
        fontnames("Antique Olive Bold", "AntiqueOlive Bold"), 
        {'A','n','t','i','q','O','l','i','v','e',' ',' ',' ',' ','B','d'},
        {0, 1, {33, 21684}, 0, REGULAR, BOLD, 4168}, 
        cc_alphabetic
    },
    
    {
        fontnames("Antique Olive Italic", "AntiqueOlive Italic"),
        {'A','n','t','i','q','O','l','i','v','e',' ',' ',' ',' ','I','t'},
        {0, 1, {29, 24413}, 0, ITALIC, NOBOLD, 4168},
        cc_alphabetic
    },

    {
        fontnames("Garamond Antiqua", "GaramondNo8"), 
        {'G','a','r','a','m','o','n','d',' ','A','n','t','i','q','u','a'},
        {0, 1, {25, 27874}, 0, REGULAR, NOBOLD, 4197}, cc_alphabetic
    },

    {
        fontnames("Garamond Halbfett", "GaramondNo8 Medium"), 
        {'G','a','r','a','m','o','n','d',' ',' ',' ',' ',' ','H','l','b'},
        {0, 1, {27, 26052}, 0, REGULAR, BOLD, 4197}, cc_alphabetic
    },

    {
        fontnames("Garamond Kursiv", "GaramondNo8 Italic"), 
        {'G','a','r','a','m','o','n','d',' ',' ',' ',' ','K','r','s','v'},
        {0, 1, {23, 30031}, 0, ITALIC, NOBOLD, 4197}, cc_alphabetic
    },

    {
        fontnames("Garamond Kursiv Halbfett", "GaramondNo8 Medium Italic"), 
        {'G','a','r','a','m','o','n','d',' ','K','r','s','v','H','l','b'},
        {0, 1, {25, 27874}, 0, ITALIC, BOLD, 4197}, cc_alphabetic
    },

    {
        fontnames("Marigold", "Mauritius"), 
        {'M','a','r','i','g','o','l','d',' ',' ',' ',' ',' ',' ',' ',' '},
        {0, 1, {22, 32550}, 0, REGULAR, NOBOLD, 4297},
        cc_alphabetic
    },

    {
        fontnames("Albertus Medium", "A028 Medium"), 
        {'A','l','b','e','r','t','u','s',' ',' ',' ',' ',' ',' ','M','d'},
        {0, 1, {31, 22968}, 0, REGULAR, MEDIUMBOLD, 4362}, cc_alphabetic
    },

    {
        fontnames("Albertus Extra Bold", "A028 Extrabold"), 
        {'A','l','b','e','r','t','u','s',' ',' ',' ',' ',' ',' ','X','b'},
        {0, 1, {36, 19530}, 0, REGULAR, EXBOLD, 4362}, cc_alphabetic
    },

    {
        fontnames("Arial", "A030"), 
        {'A','r','i','a','l',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' '},
        {0, 1, {27, 25914}, 0, REGULAR, NOBOLD, 16602}, cc_alphabetic
    },

    {
        fontnames("Arial Bold", "A030 Bold"), 
        {'A','r','i','a','l',' ',' ',' ',' ',' ',' ',' ',' ',' ','B','d'},
        {0, 1, {27, 25914}, 0, REGULAR, BOLD, 16602}, cc_alphabetic
    },

    {
        fontnames("Arial Italic", "A030 Italic"), 
        {'A','r','i','a','l',' ',' ',' ',' ',' ',' ',' ',' ',' ','I','t'},
        {0, 1, {27, 25914}, 0, ITALIC, NOBOLD, 16602}, cc_alphabetic
    },

    {
        fontnames("Arial Bold Italic", "A030 Bold Italic"),
        {'A','r','i','a','l',' ',' ',' ',' ',' ',' ',' ','B','d','I','t'},
        {0, 1, {27, 25914}, 0, ITALIC, BOLD, 16602}, cc_alphabetic
    },

    {
        fontnames("Times New Roman", "NimbusRomanNo9"), 
        {'T','i','m','e','s','N','e','w','R','m','n',' ',' ',' ',' ',' '},
        {0, 1, {25, 28800}, 0, REGULAR, NOBOLD, 16901},
        cc_alphabetic
    },

    {
        fontnames("NimbusRomanNo9 Medium", "Times New Roman Bold"),
        {'T','i','m','e','s','N','e','w','R','m','n',' ',' ',' ','B','d'},
        {0, 1, {25, 28800}, 0, REGULAR, BOLD, 16901},
        cc_alphabetic
    },

    {
        fontnames("Times New Roman Italic", "NimbusRomanNo9 Italic"),
        {'T','i','m','e','s','N','e','w','R','m','n',' ',' ',' ','I','t'},
        {0, 1, {25, 28800}, 0, ITALIC, NOBOLD, 16901},
        cc_alphabetic
    },

    {
        fontnames("Times New Roman Italic", "NimbusRomanNo9 Medium Italic"), 
        {'T','i','m','e','s','N','e','w','R','m','n',' ','B','d','I','t'},
        {0, 1, {25, 28800}, 0, ITALIC, BOLD, 16901},
        cc_alphabetic
    },

    {
        fontnames("Helvetica", "NimbusSanL"), 
        {'H','e','l','v','e','t','i','c','a',' ',' ',' ',' ',' ',' ',' '},
        {0, 1, {0, 0}, 0, REGULAR, NOBOLD, 24580}, cc_alphabetic
    },

    {
        fontnames("Helvetica-Bold", "NimbusSanL Bold"), 
        {'H','e','l','v','e','t','i','c','a',' ',' ',' ',' ',' ','B','d'},
        {0, 1, {0, 0}, 0, REGULAR, BOLD, 24580}, cc_alphabetic
    },

    {
        fontnames("Helvetica-BoldOblique", "NimbusSanL Bold Italic"),
        {'H','e','l','v','e','t','i','c','a',' ',' ',' ','B','d','O','b'},
        {0, 1, {0, 0}, 0, ITALIC, BOLD, 24580},
        cc_alphabetic
    },

    {
        fontnames("Helvetica-Narrow", "NimbusSanLCon"),
        {'H','e','l','v','e','t','i','c','a',' ',' ',' ',' ',' ','N','r'},
        {0, 1, {0, 0}, 0, CONDENSED, NOBOLD, 24580},
        cc_alphabetic
    },

    {
        fontnames("Helvetica-Narrow-Bold", "NimbusSanLCon Bold"),
        {'H','e','l','v','e','t','i','c','a',' ',' ',' ','N','r','B','d'},
        {0, 1, {0, 0}, 0, CONDENSED, BOLD, 24580},
        cc_alphabetic
    },

    {
        fontnames("Helvetica-Narrow-BoldOblique", "NimbusSanLCon Bold Italic"), 
        {'H','e','l','v','e','t','i','c','a',' ','N','r','B','d','O','b'},
        {0, 1, {0, 0}, 0, CONDENSEDITALIC, BOLD, 24580}, cc_alphabetic
    },

    {
        fontnames("Helvetica-Narrow-Oblique", "NimbusSanLCon Italic"), 
        {'H','e','l','v','e','t','i','c','a',' ',' ',' ','N','r','O','b'},
        {0, 1, {0, 0}, 0, CONDENSEDITALIC, NOBOLD, 24580}, cc_alphabetic
    },

    {
        fontnames("Helvetica-Oblique", "NimbusSanL Italic"),
        {'H','e','l','v','e','t','i','c','a',' ',' ',' ',' ',' ','O','b'},
        {0, 1, {0, 0}, 0, ITALIC, NOBOLD, 24580}, cc_alphabetic
    },

    {
        fontnames("Palatino-Roman", "URWPalladioL"),
        {'P','a','l','a','t','i','n','o',' ',' ',' ',' ',' ','R','m','n'},
        {0, 1, {0, 0}, 0, REGULAR, NOBOLD, 24591}, cc_alphabetic
    },

    {
        fontnames("Palatino-Italic", "URWPalladioL Italic"), 
        {'P','a','l','a','t','i','n','o',' ',' ',' ',' ',' ',' ','I','t'},
        {0, 1, {0, 0}, 0, ITALIC, NOBOLD, 24591}, cc_alphabetic
    },

    {
        fontnames("Palatino-Bold", "URWPalladioL Bold"), 
        {'P','a','l','a','t','i','n','o',' ',' ',' ',' ',' ',' ','B','d'},
        {0, 1, {0, 0}, 0, REGULAR, BOLD, 24591}, cc_alphabetic
    },

    {
        fontnames("Palatino-BoldItalic", "URWPalladioL Bold Italic"), 
        {'P','a','l','a','t','i','n','o',' ',' ',' ',' ','B','d','I','t'},
        {0, 1, {0, 0}, 0, ITALIC, BOLD, 24591}, cc_alphabetic
    },

    {
        fontnames("AvantGarde-Book", "URWGothicL"),
        {'I','T','C','A','v','a','n','t','G','a','r','d',' ',' ','B','k'},
        {0, 1, {0, 0}, 0, REGULAR, NOBOLD, 24607}, cc_alphabetic
    },

    {
        fontnames("AvantGarde-Demi", "URWGothicLDem"),
        {'I','T','C','A','v','a','n','t','G','a','r','d',' ',' ','D','b'},
        {0, 1, {0, 0}, 0, REGULAR, BOLD, 24607}, cc_alphabetic
    },

    {
        fontnames("AvantGarde-BookOblique", "URWGothicL Oblique"), 
        {'I','T','C','A','v','a','n','t','G','a','r','d','B','k','O','b'},
        {0, 1, {0, 0}, 0, ITALIC, NOBOLD, 24607}, cc_alphabetic
    },

    {
        fontnames("AvantGarde-DemiOblique", "URWGothicLDem Oblique"),
        {'I','T','C','A','v','a','n','t','G','a','r','d','D','b','O','b'},
        {0, 1, {0, 0}, 0, ITALIC, BOLD, 24607}, cc_alphabetic
    },

    {
        fontnames("URWBookmanL", "Bookman-Light"),
        {'I','T','C','B','o','o','k','m','a','n',' ',' ',' ',' ','L','t'},
        {0, 1, {0, 0}, 0, REGULAR, NOBOLD, 24623}, cc_alphabetic
    },

    {
        fontnames("Bookman-Demi", "URWBookmanL Bold"),
        {'I','T','C','B','o','o','k','m','a','n',' ',' ',' ',' ','D','b'},
        {0, 1, {0, 0}, 0, REGULAR, BOLD, 24623}, cc_alphabetic
    },

    {
        fontnames("Bookman-LightItalic", "URWBookmanL Italic"),
        {'I','T','C','B','o','o','k','m','a','n',' ',' ','L','t','I','t'},
        {0, 1, {0, 0}, 0, ITALIC, NOBOLD, 24623}, cc_alphabetic
    },

    {
        fontnames("Bookman-DemiItalic", "URWBookmanL Bold Italic"),
        {'I','T','C','B','o','o','k','m','a','n',' ',' ','D','b','I','t'},
        {0, 1, {0, 0}, 0, ITALIC, BOLD, 24623}, cc_alphabetic
    },

    {
        fontnames("NewCenturySchlbk-Bold", "CenturySchL Bold"), 
        {'N','w','C','e','n','t','S','c','h','l','b','k',' ',' ','B','d'},
        {0, 1, {0, 0}, 0, REGULAR, BOLD, 24703}, cc_alphabetic
    },

    {
        fontnames("NewCenturySchlbk-BoldItalic", "CenturySchL Bold Italic"), 
        {'N','w','C','e','n','t','S','c','h','l','b','k','B','d','I','t'},
        {0, 1, {0, 0}, 0, ITALIC, BOLD, 24703}, cc_alphabetic
    },

    {
        fontnames("NewCenturySchlbk-Italic", "CenturySchL Italic"), 
        {'N','w','C','e','n','t','S','c','h','l','b','k',' ',' ','I','t'},
        {0, 1, {0, 0}, 0, ITALIC, NOBOLD, 24703}, cc_alphabetic
    },

    {
        fontnames("NewCenturySchlbk-Roman", "CenturySchL"), 
        {'N','w','C','e','n','t','S','c','h','l','b','k',' ','R','m','n'},
        {0, 1, {0, 0}, 0, REGULAR, NOBOLD, 24703}, cc_alphabetic
    },

    {
        fontnames("Times-Roman", "NimbusRomanNo9L"), 
        {'T','i','m','e','s',' ',' ',' ',' ',' ',' ',' ',' ','R','m','n'},
        {0, 1, {29, 24413}, 0, REGULAR, NOBOLD, 4101},
        cc_alphabetic
    },

    {
        fontnames("Times-Bold", "NimbusRomanNo9L Bold"),
        {'T','i','m','e','s',' ',' ',' ',' ',' ',' ',' ',' ',' ','B','d'},
        {0, 1, {29, 24413}, 0, REGULAR, BOLD, 4101},
        cc_alphabetic
    },

    {
        fontnames("Times-Italic", "NimbusRomanNo9L Italic"), 
        {'T','i','m','e','s',' ',' ',' ',' ',' ',' ',' ',' ',' ','I','t'},
        {0, 1, {29, 24413}, 0, ITALIC, NOBOLD, 4101},
        cc_alphabetic
    },

    {
        fontnames("Times-BoldItalic", "NimbusRomanNo9L Bold Italic"), 
        {'T','i','m','e','s',' ',' ',' ',' ',' ',' ',' ','B','d','I','t'},
        {0, 1, {29, 24413}, 0, ITALIC, BOLD, 4101},
        cc_alphabetic
    },

    {
        fontnames("ZapfChancery-MediumItalic", "URWChanceryLMed Italic"),
        {'Z','a','p','f','C','h','a','n','c','e','r','y','M','d','I','t'},
        {0, 1, {29, 24413},0, ITALIC, NOBOLD, face_val(43, agfa)}, 
        cc_alphabetic
    },
 
    /* NB needs alias 
    {"StandardSymL", {'S','y','m','b','o','l','P','S',' ',' ',' ',' ',' ',' ',' ',' '},
     {621, 1, {25, 28800}, 0, REGULAR, NOBOLD, 16686}, cc_symbol},
    */

#ifdef AGFA_FONT_TABLE
    {
        fontnames("Wingdings", "noname"),
        {'W','i','n','g','d','i','n','g','s',' ',' ',' ',' ',' ',' ',' '},
        {18540, 1, {27, 25914},0, REGULAR, NOBOLD, face_val(2730, agfa)},
        cc_dingbats
    },
#endif

    {
        fontnames("ZapfDingbats", "Dingbats"), 
        {'I','T','C','Z','a','p','f','D','i','n','g','b','a','t','s',' '},
        {18540, 1, {27, 25914},0, REGULAR, NOBOLD, face_val(45, agfa)},
        cc_dingbats
    },

    {
        fontnames("Courier-Bold", "NimbusMono Bold"), 
        {'C','o','u','r','i','e','r',' ',' ',' ',' ',' ',' ',' ','B','d'},
        {0, 0, {60, 11998}, 0, REGULAR, BOLD, face_val(3, agfa)}, 
        cc_alphabetic
    },


    {
        fontnames("Courier Italic", "NimbusMono Italic"), 
        {'C','o','u','r','i','e','r',' ',' ',' ',' ',' ',' ',' ','I','t'},
        {0, 0, {60, 11998}, 0, ITALIC, NOBOLD, face_val(3, agfa)},
        cc_alphabetic 
    },

    {
        fontnames("Courier Bold Italic", "NimbusMono Bold Italic"), 
        {'C','o','u','r','i','e','r',' ',' ',' ',' ',' ','B','d','I','t'},
        {0, 0, {60, 11998}, 0, ITALIC, BOLD, face_val(3, agfa)},
        cc_alphabetic
    },

    {
        fontnames("Letter Gothic", "LetterGothic"), 
        {'L','e','t','t','e','r','G','o','t','h','i','c',' ',' ',' ',' '},
        {0, 0, {54, 13332}, 0, REGULAR, NOBOLD, face_val(6, agfa)},
        cc_alphabetic
    },
    
    {
        fontnames("Letter Gothic Bold", "LetterGothic Bold"),
        {'L','e','t','t','e','r','G','o','t','h','i','c',' ',' ','B','d'},
        {0, 0, {50, 14400}, 0, REGULAR, BOLD, face_val(6, agfa)},
        cc_alphabetic
    },

    {
        fontnames("Letter Gothic Italic", "LetterGothic Italic"),
        {'L','e','t','t','e','r','G','o','t','h','i','c',' ',' ','I','t'},
        {0, 0, {50, 14400}, 0, ITALIC, NOBOLD, face_val(6, agfa)},
        cc_alphabetic
    },


    {
        /* NB should be CourierPS */
        fontnames("NimbusMonL", "Courier"),
        {'C','o','u','r','i','e','r','P','S',' ',' ',' ',' ',' ',' ',' '},
        {0, 1, {0, 0}, 0, REGULAR, NOBOLD, 24579},
        cc_alphabetic
    },

    {
        fontnames("NimbusMonL Bold", "Courier-Bold"),
        {'C','o','u','r','i','e','r','P','S',' ',' ',' ',' ',' ','B','d'},
        {0, 1, {0, 0}, 0, REGULAR, BOLD, 24579}, cc_alphabetic
    },

    {
        fontnames("Courier-BoldOblique", "NimbusMonL Bold Oblique"),
        {'C','o','u','r','i','e','r','P','S',' ',' ',' ','B','d','O','b'},
        {0, 1, {0, 0}, 0, ITALIC, BOLD, 24579}, cc_alphabetic
    },

    {
        fontnames("Courier-Oblique", "NimbusMonL Oblique"),
        {'C','o','u','r','i','e','r','P','S',' ',' ',' ',' ',' ','O','b'},
        {0, 1, {0, 0}, 0, ITALIC, NOBOLD, 24579}, cc_alphabetic
    },

#ifndef AGFA /* NB - line printer support seams to be completely
                absent in the AGFA build */

    {fontnames("noname", "ArtLinePrinter"), {'L','i','n','e',' ','P','r','i','n','t','e','r',' ',' ','0','N'},
     {0, 0, {432, 1667}, 0, REGULAR, NOBOLD, 0}, cc_alphabetic},
    {fontnames("noname", "ArtLinePrinter"), {'L','i','n','e',' ','P','r','i','n','t','e','r',' ','1','0','U'},
     {0, 0, {432, 1667}, 0, REGULAR, NOBOLD, 0}, cc_alphabetic},
    {fontnames("noname", "ArtLinePrinter"), {'L','i','n','e',' ','P','r','i','n','t','e','r',' ','1','1','U'},
     {0, 0, {432, 1667}, 0, REGULAR, NOBOLD, 0}, cc_alphabetic},
    {fontnames("noname", "ArtLinePrinter"), {'L','i','n','e',' ','P','r','i','n','t','e','r',' ','1','2','U'},
     {0, 0, {432, 1667}, 0, REGULAR, NOBOLD, 0}, cc_alphabetic},
    {fontnames("noname", "ArtLinePrinter"), {'L','i','n','e',' ','P','r','i','n','t','e','r',' ',' ','1','U'},
     {0, 0, {432, 1667}, 0, REGULAR, NOBOLD, 0}, cc_alphabetic},
    {fontnames("noname", "ArtLinePrinter"), {'L','i','n','e',' ','P','r','i','n','t','e','r',' ',' ','2','N'},
     {0, 0, {432, 1667}, 0, REGULAR, NOBOLD, 0}, cc_alphabetic},
    {fontnames("noname", "ArtLinePrinter"), {'L','i','n','e',' ','P','r','i','n','t','e','r',' ',' ','5','N'},
     {0, 0, {432, 1667}, 0, REGULAR, NOBOLD, 0}, cc_alphabetic},
    {fontnames("noname", "ArtLinePrinter"), {'L','i','n','e',' ','P','r','i','n','t','e','r',' ',' ','8','U'},
     {0, 0, {432, 1667}, 0, REGULAR, NOBOLD, 0}, cc_alphabetic},
#endif /* not AGFA and NB */
    {fontnames("", ""), {'0','0'},
     {0, 0, {0, 0}, 0, 0, 0, 0} }
#undef AGFA
#undef C
#undef cc_alphabetic
#undef cc_symbol
#undef cc_dingbats
#undef pitch_1
#undef agfa_value
#undef face_val
};
