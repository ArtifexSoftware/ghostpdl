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
#ifndef AGFA_FONT_TABLE
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

    {"NimbusMono", {'C','o','u','r','i','e','r',' ',' ',' ',' ',' ',' ',' ',' ',' '},
     {0, 0, {60, 11998}, 0, REGULAR, NOBOLD, 4099}, cc_alphabetic},

    {"URWGothicL", {'I','T','C','A','v','a','n','t','G','a','r','d',' ',' ','B','k'},
     {0, 1, {0, 0}, 0, REGULAR, NOBOLD, 24607}, cc_alphabetic},
    {"URWGothicL Oblique", {'I','T','C','A','v','a','n','t','G','a','r','d','B','k','O','b'},
     {0, 1, {0, 0}, 0, ITALIC, NOBOLD, 24607}, cc_alphabetic},
    {"URWGothicLDem", {'I','T','C','A','v','a','n','t','G','a','r','d',' ',' ','D','b'},
     {0, 1, {0, 0}, 0, REGULAR, BOLD, 24607}, cc_alphabetic},
    {"URWGothicLDem Oblique", {'I','T','C','A','v','a','n','t','G','a','r','d','D','b','O','b'},
     {0, 1, {0, 0}, 0, ITALIC, BOLD, 24607}, cc_alphabetic},

    {"NimbusMonL", {'C','o','u','r','i','e','r','P','S',' ',' ',' ',' ',' ',' ',' '},
     {0, 1, {0, 0}, 0, REGULAR, NOBOLD, 24579}, cc_alphabetic},
    {"NimbusMonL Bold", {'C','o','u','r','i','e','r','P','S',' ',' ',' ',' ',' ','B','d'},
     {0, 1, {0, 0}, 0, REGULAR, BOLD, 24579}, cc_alphabetic},
    {"NimbusMonL Bold Oblique", {'C','o','u','r','i','e','r','P','S',' ',' ',' ','B','d','O','b'},
     {0, 1, {0, 0}, 0, ITALIC, BOLD, 24579}, cc_alphabetic},
    {"NimbusMonL Oblique", {'C','o','u','r','i','e','r','P','S',' ',' ',' ',' ',' ','O','b'},
     {0, 1, {0, 0}, 0, ITALIC, NOBOLD, 24579}, cc_alphabetic},

    {"NimbusSanL", {'H','e','l','v','e','t','i','c','a',' ',' ',' ',' ',' ',' ',' '},
     {0, 1, {0, 0}, 0, REGULAR, NOBOLD, 24580}, cc_alphabetic},
    {"NimbusSanL Bold", {'H','e','l','v','e','t','i','c','a',' ',' ',' ',' ',' ','B','d'},
     {0, 1, {0, 0}, 0, REGULAR, BOLD, 24580}, cc_alphabetic},
    {"NimbusSanL Bold Italic", {'H','e','l','v','e','t','i','c','a',' ',' ',' ','B','d','O','b'},
     {0, 1, {0, 0}, 0, ITALIC, BOLD, 24580}, cc_alphabetic},
    {"NimbusSanLCon", {'H','e','l','v','e','t','i','c','a',' ',' ',' ',' ',' ','N','r'},
     {0, 1, {0, 0}, 0, CONDENSED, NOBOLD, 24580}, cc_alphabetic},
    {"NimbusSanLCon Bold", {'H','e','l','v','e','t','i','c','a',' ',' ',' ','N','r','B','d'},
     {0, 1, {0, 0}, 0, CONDENSED, BOLD, 24580}, cc_alphabetic},
    {"NimbusSanLCon Bold Italic", {'H','e','l','v','e','t','i','c','a',' ','N','r','B','d','O','b'},
     {0, 1, {0, 0}, 0, CONDENSEDITALIC, BOLD, 24580}, cc_alphabetic},
    {"NimbusSanLCon Italic", {'H','e','l','v','e','t','i','c','a',' ',' ',' ','N','r','O','b'},
     {0, 1, {0, 0}, 0, CONDENSEDITALIC, NOBOLD, 24580}, cc_alphabetic},
    {"NimbusSanL Italic", {'H','e','l','v','e','t','i','c','a',' ',' ',' ',' ',' ','O','b'},
     {0, 1, {0, 0}, 0, ITALIC, NOBOLD, 24580}, cc_alphabetic},

    {"CenturySchL Bold", {'N','w','C','e','n','t','S','c','h','l','b','k',' ',' ','B','d'},
     {0, 1, {0, 0}, 0, REGULAR, BOLD, 24703}, cc_alphabetic},
    {"CenturySchL Bold Italic", {'N','w','C','e','n','t','S','c','h','l','b','k','B','d','I','t'},
     {0, 1, {0, 0}, 0, ITALIC, BOLD, 24703}, cc_alphabetic},
    {"CenturySchL Italic", {'N','w','C','e','n','t','S','c','h','l','b','k',' ',' ','I','t'},
     {0, 1, {0, 0}, 0, ITALIC, NOBOLD, 24703}, cc_alphabetic},
    {"CenturySchL", {'N','w','C','e','n','t','S','c','h','l','b','k',' ','R','m','n'},
     {0, 1, {0, 0}, 0, REGULAR, NOBOLD, 24703}, cc_alphabetic},

    {"URWPalladioL Bold", {'P','a','l','a','t','i','n','o',' ',' ',' ',' ',' ',' ','B','d'},
     {0, 1, {0, 0}, 0, REGULAR, BOLD, 24591}, cc_alphabetic},
    {"URWPalladioL Bold Italic", {'P','a','l','a','t','i','n','o',' ',' ',' ',' ','B','d','I','t'},
     {0, 1, {0, 0}, 0, ITALIC, BOLD, 24591}, cc_alphabetic},
    {"URWPalladioL Italic", {'P','a','l','a','t','i','n','o',' ',' ',' ',' ',' ',' ','I','t'},
     {0, 1, {0, 0}, 0, ITALIC, NOBOLD, 24591}, cc_alphabetic},
    {"URWPalladioL", {'P','a','l','a','t','i','n','o',' ',' ',' ',' ',' ','R','m','n'},
     {0, 1, {0, 0}, 0, REGULAR, NOBOLD, 24591}, cc_alphabetic},

    {"NimbusRomanNo4 Light", {'C','G',' ','T','i','m','e','s',' ',' ',' ',' ',' ',' ',' ',' '},
     {0, 1, {29, 24413}, 0, REGULAR, NOBOLD, 4101},
     cc_alphabetic},
    {"NimbusRomanNo4 Bold", {'C','G',' ','T','i','m','e','s',' ',' ',' ',' ',' ',' ','B','d'},
     {0, 1, {29, 24413}, 0, REGULAR, BOLD, 4101},
     cc_alphabetic},
    {"NimbusRomanNo4 Light Italic", {'C','G',' ','T','i','m','e','s',' ',' ',' ',' ',' ',' ','I','t'},
     {0, 1, {29, 24413}, 0, ITALIC, NOBOLD, 4101},
     cc_alphabetic},
    {"NimbusRomanNo4 Bold Italic", {'C','G',' ','T','i','m','e','s',' ',' ',' ',' ','B','d','I','t'},
     {0, 1, {29, 24413}, 0, ITALIC, BOLD, 4101},
     cc_alphabetic},

    {"NimbusRomanNo9L", {'T','i','m','e','s',' ',' ',' ',' ',' ',' ',' ',' ','R','m','n'},
     {0, 1, {29, 24413}, 0, REGULAR, NOBOLD, 4101},
     cc_alphabetic},
    {"NimbusRomanNo9L Bold", {'T','i','m','e','s',' ',' ',' ',' ',' ',' ',' ',' ',' ','B','d'},
     {0, 1, {29, 24413}, 0, REGULAR, BOLD, 4101},
     cc_alphabetic},
    {"NimbusRomanNo9L Italic", {'T','i','m','e','s',' ',' ',' ',' ',' ',' ',' ',' ',' ','I','t'},
     {0, 1, {29, 24413}, 0, ITALIC, NOBOLD, 4101},
     cc_alphabetic},
    {"NimbusRomanNo9L Bold Italic", {'T','i','m','e','s',' ',' ',' ',' ',' ',' ',' ','B','d','I','t'},
     {0, 1, {29, 24413}, 0, ITALIC, BOLD, 4101},
     cc_alphabetic},

    {"URWClassico", {'C','G',' ','O','m','e','g','a',' ',' ',' ',' ',' ',' ',' ',' '},
     {0, 1, {27, 26052}, 0, REGULAR, NOBOLD, 4113},
     cc_alphabetic},
    {"URWClassico Bold", {'C','G',' ','O','m','e','g','a',' ',' ',' ',' ',' ',' ','B','d'},
     {0, 1, {27, 26052}, 0, REGULAR, BOLD, 4113},
     cc_alphabetic},
    {"URWClassico Italic", {'C','G',' ','O','m','e','g','a',' ',' ',' ',' ',' ',' ','I','t'},
     {0, 1, {27, 26052}, 0, ITALIC, NOBOLD, 4113},
     cc_alphabetic},
    {"URWClassico Bold Italic", {'C','G',' ','O','m','e','g','a',' ',' ',' ',' ','B','d','I','t'},
     {0, 1, {27, 26052}, 0, ITALIC, BOLD, 4113},
     cc_alphabetic},

    {"Coronet", {'C','o','r','o','n','e','t',' ',' ',' ',' ',' ',' ',' ',' ',' '},
     {0, 1, {20, 35531}, 0, ITALIC, NOBOLD, 4116},
     cc_alphabetic},

    {"ClarendonURWBolCon", {'C','l','a','r','e','n','d','o','n',' ',' ',' ','C','d','B','d'},
     {0, 1, {22, 32550}, 0, CONDENSED, BOLD, 4140},
     cc_alphabetic},

    {"U001", {'U','n','i','v','e','r','s',' ',' ',' ',' ',' ',' ',' ','M','d'},
     {0, 1, {33, 21684}, 0, REGULAR, NOBOLD, 4148},
     cc_alphabetic},
    {"U001 Bold", {'U','n','i','v','e','r','s',' ',' ',' ',' ',' ',' ',' ','B','d'},
     {0, 1, {33, 21684}, 0, REGULAR, BOLD, 4148},
     cc_alphabetic},
    {"U001 Italic", {'U','n','i','v','e','r','s',' ',' ',' ',' ',' ','M','d','I','t'},
     {0, 1, {33, 21684}, 0, ITALIC, NOBOLD, 4148},
     cc_alphabetic},
    {"U001 Bold Italic", {'U','n','i','v','e','r','s',' ',' ',' ',' ',' ','B','d','I','t'},
     {0, 1, {33, 21684}, 0, ITALIC, BOLD, 4148},
     cc_alphabetic},
    {"U001Con", {'U','n','i','v','e','r','s',' ',' ',' ',' ',' ','C','d','M','d'},
     {0, 1, {22, 32550}, 0, CONDENSED, NOBOLD, 4148},
     cc_alphabetic},
    {"U001Con Bold", {'U','n','i','v','e','r','s',' ',' ',' ',' ',' ','C','d','B','d'},
     {0, 1, {22, 32550}, 0, CONDENSED, BOLD, 4148},
     cc_alphabetic},
    {"U001Con Italic", {'U','n','i','v','e','r','s',' ',' ',' ','C','d','M','d','I','t'},
     {0, 1, {22, 32550}, 0, CONDENSEDITALIC, NOBOLD, 4148},
     cc_alphabetic},
    {"U001Con Bold Italic", {'U','n','i','v','e','r','s',' ',' ',' ','C','d','B','d','I','t'},
     {0, 1, {22, 32550}, 0, CONDENSEDITALIC, BOLD, 4148},
     cc_alphabetic},

    {"AntiqueOlive", {'A','n','t','i','q','O','l','i','v','e',' ',' ',' ',' ',' ',' '},
     {0, 1, {29, 24413}, REGULAR, NOBOLD, 4168},
     cc_alphabetic},
    {"AntiqueOlive Bold", {'A','n','t','i','q','O','l','i','v','e',' ',' ',' ',' ','B','d'},
     {0, 1, {33, 21684}, REGULAR, BOLD, 4168}, cc_alphabetic},
    {"AntiqueOlive Italic", {'A','n','t','i','q','O','l','i','v','e',' ',' ',' ',' ','I','t'},
     {0, 1, {29, 24413}, 0, ITALIC, NOBOLD, 4168},
     cc_alphabetic},

    {"GaramondNo8", {'G','a','r','a','m','o','n','d',' ','A','n','t','i','q','u','a'},
     {0, 1, {25, 27874}, 0, REGULAR, NOBOLD, 4197}, cc_alphabetic},
    {"GaramondNo8 Medium", {'G','a','r','a','m','o','n','d',' ',' ',' ',' ',' ','H','l','b'},
     {0, 1, {27, 26052}, 0, REGULAR, BOLD, 4197}, cc_alphabetic},
    {"GaramondNo8 Italic", {'G','a','r','a','m','o','n','d',' ',' ',' ',' ','K','r','s','v'},
     {0, 1, {23, 30031}, 0, ITALIC, NOBOLD, 4197}, cc_alphabetic},
    {"GaramondNo8 Medium Italic", {'G','a','r','a','m','o','n','d',' ','K','r','s','v','H','l','b'},
     {0, 1, {25, 27874}, 0, ITALIC, BOLD, 4197}, cc_alphabetic},

    {"Mauritius", {'M','a','r','i','g','o','l','d',' ',' ',' ',' ',' ',' ',' ',' '},
     {0, 1, {22, 32550}, 0, REGULAR, NOBOLD, 4297},
     cc_alphabetic},

    {"A028 Medium", {'A','l','b','e','r','t','u','s',' ',' ',' ',' ',' ',' ','M','d'},
     {0, 1, {31, 22968}, 0, REGULAR, MEDIUMBOLD, 4362}, cc_alphabetic},
    {"A028 Extrabold", {'A','l','b','e','r','t','u','s',' ',' ',' ',' ',' ',' ','X','b'},
     {0, 1, {36, 19530}, 0, REGULAR, EXBOLD, 4362}, cc_alphabetic},

    {"A030", {'A','r','i','a','l',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' '},
     {0, 1, {27, 25914}, 0, REGULAR, NOBOLD, 16602}, cc_alphabetic},
    {"A030 Bold", {'A','r','i','a','l',' ',' ',' ',' ',' ',' ',' ',' ',' ','B','d'},
     {0, 1, {27, 25914}, 0, REGULAR, BOLD, 16602}, cc_alphabetic},
    {"A030 Italic", {'A','r','i','a','l',' ',' ',' ',' ',' ',' ',' ',' ',' ','I','t'},
     {0, 1, {27, 25914}, 0, ITALIC, NOBOLD, 16602}, cc_alphabetic},
    {"A030 Bold Italic", {'A','r','i','a','l',' ',' ',' ',' ',' ',' ',' ','B','d','I','t'},
     {0, 1, {27, 25914}, 0, ITALIC, BOLD, 16602}, cc_alphabetic},

    {"NimbusRomanNo9", {'T','i','m','e','s','N','e','w','R','m','n',' ',' ',' ',' ',' '},
     {0, 1, {25, 28800}, 0, REGULAR, NOBOLD, 16901}, 
     cc_alphabetic},
    {"NimbusRomanNo9 Medium",{'T','i','m','e','s','N','e','w','R','m','n',' ',' ',' ','B','d'},
     {0, 1, {25, 28800}, 0, REGULAR, BOLD, 16901}, cc_alphabetic},
    {"NimbusRomanNo9 Italic", {'T','i','m','e','s','N','e','w','R','m','n',' ',' ',' ','I','t'},
     {0, 1, {25, 28800}, 0, ITALIC, NOBOLD, 16901}, cc_alphabetic},
    {"NimbusRomanNo9 Medium Italic", {'T','i','m','e','s','N','e','w','R','m','n',' ','B','d','I','t'},
     {0, 1, {25, 28800}, 0, ITALIC, BOLD, 16901}, cc_alphabetic},

    {"StandardSymL", {'S','y','m','b','o','l',' ',' ',' ',' ',' ',' ',' ',' ',' ',' '},
     {621, 1, {25, 28800}, 0, REGULAR, NOBOLD, 16686}, cc_symbol},

    {"URWChanceryLMed Italic", {'Z','a','p','f','C','h','a','n','c','e','r','y','M','d','I','t'},
     {18540, 1, {27, 45099},0, REGULAR, NOBOLD, face_val(2730, agfa)}, cc_dingbats},
    {"Dingbats", {'W','i','n','g','d','i','n','g','s',' ',' ',' ',' ',' ',' ',' '},
     {18540, 1, {27, 25914},0, REGULAR, NOBOLD, face_val(2730, agfa)}, cc_dingbats},

    {"NimbusMono Bold", {'C','o','u','r','i','e','r',' ',' ',' ',' ',' ',' ',' ','B','d'},
     {0, 0, {60, 11998}, 0, REGULAR, BOLD, face_val(3, agfa)}, 
     cc_alphabetic },
    {"NimbusMono Italic", {'C','o','u','r','i','e','r',' ',' ',' ',' ',' ',' ',' ','I','t'},
     {0, 0, {60, 11998}, 0, ITALIC, NOBOLD, face_val(3, agfa)},
     cc_alphabetic },
    {"NimbusMono Bold Italic", {'C','o','u','r','i','e','r',' ',' ',' ',' ',' ','B','d','I','t'},
     {0, 0, {60, 11998}, 0, ITALIC, BOLD, face_val(3, agfa)},
     cc_alphabetic},

    {"LetterGothic", {'L','e','t','t','e','r','G','o','t','h','i','c',' ',' ',' ',' '},
     {0, 0, {54, 13332}, 0, REGULAR, NOBOLD, face_val(6, agfa)},
     cc_alphabetic},
    {"LetterGothic Bold",
     {'L','e','t','t','e','r','G','o','t','h','i','c',' ',' ','B','d'},
     {0, 0, {50, 14400}, 0, REGULAR, BOLD, face_val(6, agfa)},
     cc_alphabetic},
    {"LetterGothic Italic", 
     {'L','e','t','t','e','r','G','o','t','h','i','c',' ',' ','I','t'},
     {0, 0, {50, 14400}, 0, ITALIC, NOBOLD, face_val(6, agfa)},
     cc_alphabetic},

    {"NimbusMono", {'L','i','n','e',' ','P','r','i','n','t','e','r',' ',' ','0','N'},
     {0, 0, {60, 11998}, 0, REGULAR, NOBOLD, face_val(218, agfa)}, cc_alphabetic},
    {"NimbusMono", {'L','i','n','e',' ','P','r','i','n','t','e','r',' ','1','0','U'},
     {0, 0, {60, 11998}, 0, REGULAR, NOBOLD, face_val(218, agfa)}, cc_alphabetic},
    {"NimbusMono", {'L','i','n','e',' ','P','r','i','n','t','e','r',' ','1','1','U'},
     {0, 0, {60, 11998}, 0, REGULAR, NOBOLD, face_val(218, agfa)}, cc_alphabetic},
    {"NimbusMono", {'L','i','n','e',' ','P','r','i','n','t','e','r',' ','1','2','U'},
     {0, 0, {60, 11998}, 0, REGULAR, NOBOLD, face_val(218, agfa)}, cc_alphabetic},
    {"NimbusMono", {'L','i','n','e',' ','P','r','i','n','t','e','r',' ',' ','1','U'},
     {0, 0, {60, 11998}, 0, REGULAR, NOBOLD, face_val(218, agfa)}, cc_alphabetic},
    {"NimbusMono", {'L','i','n','e',' ','P','r','i','n','t','e','r',' ',' ','2','N'},
     {0, 0, {60, 11998}, 0, REGULAR, NOBOLD, face_val(218, agfa)}, cc_alphabetic},
    {"NimbusMono", {'L','i','n','e',' ','P','r','i','n','t','e','r',' ',' ','5','N'},
     {0, 0, {60, 11998}, 0, REGULAR, NOBOLD, face_val(218, agfa)}, cc_alphabetic},
    {"NimbusMono", {'L','i','n','e',' ','P','r','i','n','t','e','r',' ',' ','8','U'},
     {0, 0, {60, 11998}, 0, REGULAR, NOBOLD, face_val(218, agfa)}, cc_alphabetic},
    {"", {'0','0'},
     {0, 0, {0, 0}, 0, 0, 0, 0} }
#undef C
#undef cc_alphabetic
#undef cc_symbol
#undef cc_dingbats
#undef pitch_1
#undef agfa_value
#undef face_val
};

#else /* agfa table */
const font_resident_t resident_table[] = {
    /*
     * Symbol sets, typeface family values, and character complements
     * are faked; they do not (necessarily) match the actual fonts.  */
#define C(b) ((byte)((b) ^ 0xff))
#define cc_alphabetic\
	  { C(0), C(0), C(0), C(0), C(0xff), C(0xc0), C(0), C(plgv_Unicode) }
#define cc_symbol\
	  { C(0), C(0), C(0), C(4), C(0), C(0), C(0), C(plgv_MSL) }
#define cc_dingbats\
	  { C(0), C(0), C(0), C(1), C(0), C(0), C(0), C(plgv_MSL) }
#define pitch_1 fp_pitch_value_cp(1)
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

    {"Courier", {'C','o','u','r','i','e','r',' ',' ',' ',' ',' ',' ',' ',' ',' '},
     {0, 0, pitch_1, 0, REGULAR, NOBOLD, face_val(3, agfa)},
     cc_alphabetic},
    {"CG Times", {'C','G',' ','T','i','m','e','s',' ',' ',' ',' ',' ',' ',' ',' '},
     {0, 1, pitch_1, 0, REGULAR, NOBOLD, face_val(5, agfa)},
     cc_alphabetic},
    {"CG Times Bold", {'C','G',' ','T','i','m','e','s',' ',' ',' ',' ',' ',' ','B','d'},
     {0, 1, pitch_1, 0, REGULAR, BOLD, face_val(5, agfa)},
     cc_alphabetic},
    {"CG Times Italic", {'C','G',' ','T','i','m','e','s',' ',' ',' ',' ',' ',' ','I','t'},
     {0, 1, pitch_1, 0, ITALIC, NOBOLD, face_val(5, agfa)},
     cc_alphabetic},
    {"CG Times Bold Italic", {'C','G',' ','T','i','m','e','s',' ',' ',' ',' ','B','d','I','t'},
     {0, 1, pitch_1, 0, ITALIC, BOLD, face_val(5, agfa)},
     cc_alphabetic},
    {"CG Omega", {'C','G',' ','O','m','e','g','a',' ',' ',' ',' ',' ',' ',' ',' '},
     {0, 1, pitch_1, 0, REGULAR, NOBOLD, face_val(17, agfa)},
     cc_alphabetic},
    {"CG Omega Bold", {'C','G',' ','O','m','e','g','a',' ',' ',' ',' ',' ',' ','B','d'},
     {0, 1, pitch_1, 0, REGULAR, BOLD, face_val(17, agfa)},
     cc_alphabetic},
    {"CG Omega Italic", {'C','G',' ','O','m','e','g','a',' ',' ',' ',' ',' ',' ','I','t'},
     {0, 1, pitch_1, 0, ITALIC, NOBOLD, face_val(17, agfa)},
     cc_alphabetic},
    {"CG Omega Bold Italic", {'C','G',' ','O','m','e','g','a',' ',' ',' ',' ','B','d','I','t'},
     {0, 1, pitch_1, 0, ITALIC, BOLD, face_val(17, agfa)},
     cc_alphabetic},
    {"Coronet", {'C','o','r','o','n','e','t',' ',' ',' ',' ',' ',' ',' ',' ',' '},
     {0, 1, pitch_1, 0, ITALIC, NOBOLD, face_val(20, agfa)},
     cc_alphabetic},
    {"Clarendon Condensed Bold", {'C','l','a','r','e','n','d','o','n',' ',' ',' ','C','d','B','d'},
     {0, 1, pitch_1, 0, CONDENSED, BOLD, face_val(44, agfa)},
     cc_alphabetic},
    {"Univers Medium", {'U','n','i','v','e','r','s',' ',' ',' ',' ',' ',' ',' ','M','d'},
     {0, 1, pitch_1, 0, REGULAR, NOBOLD, face_val(52, agfa)},
     cc_alphabetic},
    {"Univers Bold", {'U','n','i','v','e','r','s',' ',' ',' ',' ',' ',' ',' ','B','d'},
     {0, 1, pitch_1, 0, REGULAR, BOLD, face_val(52, agfa)},
     cc_alphabetic},
    {"Univers Medium Italic", {'U','n','i','v','e','r','s',' ',' ',' ',' ',' ','M','d','I','t'},
     {0, 1, pitch_1, 0, ITALIC, NOBOLD, face_val(52, agfa)},
     cc_alphabetic},
    {"Univers Bold Italic", {'U','n','i','v','e','r','s',' ',' ',' ',' ',' ','B','d','I','t'},
     {0, 1, pitch_1, 0, ITALIC, BOLD, face_val(52, agfa)},
     cc_alphabetic},
    {"Univers Condensed Medium", {'U','n','i','v','e','r','s',' ',' ',' ',' ',' ','C','d','M','d'},
     {0, 1, pitch_1, 0, CONDENSED, NOBOLD, face_val(52, agfa)},
     cc_alphabetic},
    {"Univers Condensed Bold", {'U','n','i','v','e','r','s',' ',' ',' ',' ',' ','C','d','B','d'},
     {0, 1, pitch_1, 0, CONDENSED, BOLD, face_val(52, agfa)},
     cc_alphabetic},
    {"Univers Condensed Medium Italic", {'U','n','i','v','e','r','s',' ',' ',' ','C','d','M','d','I','t'},
     {0, 1, pitch_1, 0, CONDENSEDITALIC, NOBOLD, face_val(52, agfa)},
     cc_alphabetic},
    {"Univers Condensed Bold Italic", {'U','n','i','v','e','r','s',' ',' ',' ','C','d','B','d','I','t'},
     {0, 1, pitch_1, 0, CONDENSEDITALIC, BOLD, face_val(52, agfa)},
     cc_alphabetic},
    {"Antique Olive", {'A','n','t','i','q','O','l','i','v','e',' ',' ',' ',' ',' ',' '},
     {0, 1, pitch_1, 0, REGULAR, NOBOLD, face_val(72, agfa)},
     cc_alphabetic},
    {"Antique Olive Bold", {'A','n','t','i','q','O','l','i','v','e',' ',' ',' ',' ','B','d'},
     {0, 1, pitch_1, 0, REGULAR, BOLD, face_val(72, agfa)}, cc_alphabetic},
    {"Antique Olive Italic", {'A','n','t','i','q','O','l','i','v','e',' ',' ',' ',' ','I','t'},
     {0, 1, pitch_1, 0, ITALIC, NOBOLD, face_val(72, agfa)},
     cc_alphabetic},
    {"Garamond Antiqua", {'G','a','r','a','m','o','n','d',' ','A','n','t','i','q','u','a'},
     {0, 1, pitch_1, 0, REGULAR, NOBOLD, face_val(101, agfa)}, cc_alphabetic},
    {"Garamond Halbfett", {'G','a','r','a','m','o','n','d',' ',' ',' ',' ',' ','H','l','b'},
     {0, 1, pitch_1, 0, REGULAR, BOLD, face_val(101, agfa)}, cc_alphabetic},
    {"Garamond Kursiv", {'G','a','r','a','m','o','n','d',' ',' ',' ',' ','K','r','s','v'},
     {0, 1, pitch_1, 0, ITALIC, NOBOLD, face_val(101, agfa)}, cc_alphabetic},
    {"Garamond Kursiv Halbfett", {'G','a','r','a','m','o','n','d',' ','K','r','s','v','H','l','b'},
     {0, 1, pitch_1, 0, ITALIC, BOLD, face_val(101, agfa)}, cc_alphabetic},
    {"Marigold", {'M','a','r','i','g','o','l','d',' ',' ',' ',' ',' ',' ',' ',' '},
     {0, 1, pitch_1, 0, REGULAR, NOBOLD, face_val(201, agfa)},
     cc_alphabetic},
    {"Albertus Medium", {'A','l','b','e','r','t','u','s',' ',' ',' ',' ',' ',' ','M','d'},
     {0, 1, pitch_1, 0, REGULAR, MEDIUMBOLD, face_val(266, agfa)}, cc_alphabetic},
    {"Albertus Extra Bold", {'A','l','b','e','r','t','u','s',' ',' ',' ',' ',' ',' ','X','b'},
     {0, 1, pitch_1, 0, REGULAR, EXBOLD, face_val(266, agfa)}, cc_alphabetic},
    {"Arial", {'A','r','i','a','l',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' '},
     {0, 1, pitch_1, 0, REGULAR, 0,
      face_val(218, agfa)}, cc_alphabetic},
    {"Arial Bold", {'A','r','i','a','l',' ',' ',' ',' ',' ',' ',' ',' ',' ','B','d'},
     {0, 1, pitch_1, 0, REGULAR, BOLD, face_val(218, agfa)}, cc_alphabetic},
    {"Arial Italic", {'A','r','i','a','l',' ',' ',' ',' ',' ',' ',' ',' ',' ','I','t'},
     {0, 1, pitch_1, 0, ITALIC, NOBOLD, face_val(218, agfa)}, cc_alphabetic},
    {"Arial Bold Italic", {'A','r','i','a','l',' ',' ',' ',' ',' ',' ',' ','B','d','I','t'},
     {0, 1, pitch_1, 0, ITALIC, BOLD, face_val(218, agfa)}, cc_alphabetic},
    {"Times New Roman", {'T','i','m','e','s','N','e','w','R','m','n',' ',' ',' ',' ',' '},
     {0, 1, pitch_1, 0, REGULAR, NOBOLD, face_val(517, agfa)}, 
     cc_alphabetic},
    {"Times New Roman Bold",{'T','i','m','e','s','N','e','w','R','m','n',' ',' ',' ','B','d'},
     {0, 1, pitch_1, 0, REGULAR, BOLD, face_val(517, agfa)}, cc_alphabetic},
    {"Times New Roman Italic", {'T','i','m','e','s','N','e','w','R','m','n',' ',' ',' ','I','t'},
     {0, 1, pitch_1, 0, ITALIC, NOBOLD, face_val(517, agfa)}, cc_alphabetic},
    {"Times New Roman Bold Italic", {'T','i','m','e','s','N','e','w','R','m','n',' ','B','d','I','t'},
     {0, 1, pitch_1, 0, ITALIC, BOLD, face_val(517, agfa)}, cc_alphabetic},
    {"Symbol", {'S','y','m','b','o','l',' ',' ',' ',' ',' ',' ',' ',' ',' ',' '},
     {621, 1, pitch_1, 0, REGULAR, NOBOLD, face_val(302, agfa)}, cc_symbol},
    {"Wingdings", {'W','i','n','g','d','i','n','g','s',' ',' ',' ',' ',' ',' ',' '},
     {18540, 1, pitch_1,0, REGULAR, NOBOLD, face_val(2730, agfa)}, cc_dingbats},
    {"Courier Bold", {'C','o','u','r','i','e','r',' ',' ',' ',' ',' ',' ',' ','B','d'},
     {0, 0, pitch_1, 0, REGULAR, BOLD, face_val(3, agfa)}, 
     cc_alphabetic },
    {"Courier Italic", {'C','o','u','r','i','e','r',' ',' ',' ',' ',' ',' ',' ','I','t'},
     {0, 0, pitch_1, 0, ITALIC, NOBOLD, face_val(3, agfa)},
     cc_alphabetic },
    {"Courier Bold Italic", {'C','o','u','r','i','e','r',' ',' ',' ',' ',' ','B','d','I','t'},
     {0, 0, pitch_1, 0, ITALIC, BOLD, face_val(3, agfa)},
     cc_alphabetic},
    {"Letter Gothic", {'L','e','t','t','e','r','G','o','t','h','i','c',' ',' ',' ',' '},
     {0, 0, pitch_1, 0, REGULAR, NOBOLD, face_val(6, agfa)},
     cc_alphabetic},
    {"Letter Gothic Bold",
     {'L','e','t','t','e','r','G','o','t','h','i','c',' ',' ','B','d'},
     {0, 0, pitch_1, 0, REGULAR, BOLD, face_val(6, agfa)},
     cc_alphabetic},
    {"Letter Gothic Italic", 
     {'L','e','t','t','e','r','G','o','t','h','i','c',' ',' ','I','t'},
     {0, 0, pitch_1, 0, ITALIC, NOBOLD, face_val(6, agfa)},
     cc_alphabetic},
    /* Note that "bound" TrueType fonts are indexed starting at 0xf000, */
    /* not at 0. */
    {"", {'0','0'},
     {0, 0, pitch_1, 0, 0, 0, 0} }
#undef C
#undef cc_alphabetic
#undef cc_symbol
#undef cc_dingbats
#undef pitch_1
#undef agfa_value
#undef face_val
};
#endif /* AGFA TABLE */

#ifdef NEVERCOMPILED


This is additional font documentation.  Most printer fonts have a
corresponding screen font which ships with HP printers.  The following
table lists the Postscript internal font number (derived from the HP
4600 Color Laserjet), the postscript font name the screen font file
name, the screen filename, and the corresponding urw filename which is
the the urw postscript file name + .ttf.

   HP Name                         screen file   urw fontfilename              pclxl name from test file
1 Albertus-ExtraBold              albr85w.ttf    A028-Ext.ttf                    Albertus      Xb                       A028 Extrabold            
2 Albertus-Medium                 albr55w.ttf    A028-Med.ttf                    Albertus      Md                       A028 Medium               
3 AntiqueOlive                    olvr55w.ttf    AntiqueOlive-Reg.ttf            AntiqOlive                             AntiqueOlive              
4 AntiqueOlive-Bold               olvr75w.ttf    AntiqueOlive-Bol.ttf            AntiqOlive    Bd                       AntiqueOlive Bold         
5 AntiqueOlive-Italic             olvr56w.ttf    AntiqueOlive-Ita.ttf            AntiqOlive    It                       AntiqueOlive Italic       
6 Arial*                                 N/A     A030-Reg.ttf                    Arial                                  A030                      
7 Arial BD*                              N/A     A030-Bol.ttf                    Arial         Bd                       A030 Bold                 
8 Arial BD It.*                          N/A     A030-BolIta.ttf                 Arial       BdIt                       A030 Bold Italic          
9 Arial It.*                             N/A     A030-Ita.ttf                    Arial         It                       A030 Italic               
10 AvantGarde-Book                 avgr45w.ttf    URWGothicL-Book.ttf +          ITCAvantGard  Bk                       URWGothicL                
11 AvantGarde-BookOblique          avgr46w.ttf    URWGothicL-BookObli.ttf +      ITCAvantGardBkOb                       URWGothicL Oblique
12 AvantGarde-Demi                 avgr65w.ttf    URWGothicL-Demi.ttf +              ITCAvantGard  Db                    URWGothicLDem
13 AvantGarde-DemiOblique          avgr66w.ttf    URWGothicL-DemiObli.ttf +       ITCAvantGardDbOb                       URWGothicLDem Oblique     
14 Bookman-Demi                    bokr75w.ttf    URWBookmanL-DemiBold.ttf +     missing from test                      URWBookmanL Bold          
15 Bookman-DemiItalic              bokr76w.ttf    URWBookmanL-DemiBoldItal.ttf + missing from test                      URWBookmanL Bold Italic   
16 Bookman-Light                   bokr35w.ttf    URWBookmanL-Ligh.ttf +         missing from test                      URWBookmanL               
17 Bookman-LightItalic             bokr36w.ttf    URWBookmanL-LighItal.ttf +     missing from test                      URWBookmanL Italic        
18 CGOmega                         cgor45w.ttf    URWClassico-Reg.ttf            CG Omega                               URWClassico               
19 CGOmega-Bold                    cgor65w.ttf    URWClassico-Bol.ttf            CG Omega      Bd                       URWClassico Bold          
20 CGOmega-BoldItalic              cgor66w.ttf    URWClassico-BolIta.ttf         CG Omega    BdIt                       URWClassico Bold Italic   
21 CGOmega-Italic                  cgor46w.ttf    URWClassico-Ita.ttf            CG Omega      It                       URWClassico Italic        
22 CGTimes                         cgtr45w.ttf    NimbusRomanNo4-Lig.ttf         CG Times                               NimbusRomanNo4 Light      
23 CGTimes-Bold                    cgtr65w.ttf    NimbusRomanNo4-Bol.ttf         CG Times      Bd                       NimbusRomanNo4 Bold       
24 CGTimes-BoldItalic              cgtr66w.ttf    NimbusRomanNo4-BolIta.ttf      CG Times    BdIt                       NimbusRomanNo4 Bold Italic
25 CGTimes-Italic                  cgtr46w.ttf    NimbusRomanNo4-LigIta.ttf      CG Times      It                       NimbusRomanNo4 Light Itali
26 Clarendon-Condensed-Bold        clar67w.ttf    ClarendonURW-BolCon.ttf        Clarendon   CdBd                       ClarendonURWBolCon        
27 Coronet                         coronet.ttf    Coronet.ttf                    Coronet                                Coronet                   
28 Courier                         cpsr45w.ttf    NimbusMonL-Regu.ttf +          CourierPS                              NimbusMonL                
29 Courier-Bold                    cpsr65w.ttf    NimbusMonL-Bold.ttf +          CourierPS     Bd                       NimbusMonL Bold           
30 Courier-BoldOblique             cpsr66w.ttf    NimbusMonL-BoldObli.ttf +      CourierPS   BdOb                       NimbusMonL Bold Oblique   
31 Courier-Oblique                 cpsr46w.ttf    NimbusMonL-ReguObli.ttf +      CourierPS     Ob                       NimbusMonL Oblique        
32 courier*                               N/A     NimbusMono-Reg.ttf             Courier                                NimbusMono                
33 courier bd.*                           N/A     NimbusMono-Bol.ttf             missing from test                      NimbusMono Bold           
34 courier bd it*                         N/A     NimbusMono-BolIta.ttf          missing from test                      NimbusMono Bold Italic    
35 courier it.*                           N/A     NimbusMono-Ita.ttf             missing from test                      NimbusMono Italic         
36 Garamond-Antiqua                garr45w.ttf    GaramondNo8-Reg.ttf            Downloaded                             GaramondNo8               
37 Garamond-Halbfett               garr65w.ttf    GaramondNo8-MedIta.ttf         Downloaded                             GaramondNo8 Medium Italic 
38 Garamond-Kursiv                 garr46w.ttf    GaramondNo8-Ita.ttf            Downloaded                             GaramondNo8 Italic        
39 Garamond-KursivHalbfett         garr66w.ttf    GaramondNo8-Med.ttf            Downloaded                             GaramondNo8 Medium        
40 Helvetica                       helr45w.ttf    NimbusSanL-Regu.ttf +          Helvetica                              NimbusSanL                
41 Helvetica-Bold                  helr65w.ttf    NimbusSanL-Bold.ttf +          Helvetica     Bd                       NimbusSanL Bold           
42 Helvetica-BoldOblique           helr66w.ttf    NimbusSanL-BoldItal.ttf +      Helvetica   BdOb                       NimbusSanL Bold Italic    
43 Helvetica-Narrow                helr47w.ttf    NimbusSanL-ReguCond.ttf +      Helvetica     Nr                       NimbusSanLCon             
44 Helvetica-Narrow-Bold           helr67w.ttf    NimbusSanL-BoldCond.ttf +      Helvetica   NrBd                       NimbusSanLCon Bold        
45 Helvetica-Narrow-BoldOblique    helr68w.ttf    NimbusSanL-BoldCondItal.ttf +  Helvetica NrBdOb                       NimbusSanLCon Bold Italic 
46 Helvetica-Narrow-Oblique        helr48w.ttf    NimbusSanL-ReguCondItal.ttf +  Helvetica   NrOb                       NimbusSanLCon Italic      
47 Helvetica-Oblique               helr46w.ttf    NimbusSanL-ReguItal.ttf +      Helvetica     Ob                       NimbusSanL Italic         
48 LetterGothic-Bold               letr65w.ttf    LetterGothic-Bol.ttf           LetterGothic  Bd                       LetterGothic Bold         
49 LetterGothic-Italic             letr46w.ttf    LetterGothic-Ita.ttf           LetterGothic  It                       LetterGothic Italic       
50 LetterGothic                    letr45w.ttf    LetterGothic-Reg.ttf           LetterGothic                           LetterGothic              
51 Marigold                        marigold.ttf   Mauritius-Reg.ttf              Marigold                               Mauritius                 
52 NewCenturySchlbk-Bold           ncsr75w.ttf    CenturySchL-Bold.ttf +         NwCentSchlbk  Bd                       CenturySchL Bold          
53 NewCenturySchlbk-BoldItalic     ncsr76w.ttf    CenturySchL-BoldItal.ttf +     NwCentSchlbkBdIt                       CenturySchL Bold Italic   
54 NewCenturySchlbk-Italic         ncsr56w.ttf    CenturySchL-Ital.ttf +         NwCentSchlbk  It                       CenturySchL Italic        
55 NewCenturySchlbk-Roman          ncsr55w.ttf    CenturySchL-Roma.ttf +         NwCentSchlbk Rmn                       CenturySchL               
56 Palatino-Bold                   palr65w.ttf    URWPalladioL-Bold.ttf +        Palatino      Bd                       URWPalladioL Bold         
57 Palatino-BoldItalic             palr66w.ttf    URWPalladioL-BoldItal.ttf +    Palatino    BdIt                       URWPalladioL Bold Italic  
58 Palatino-Italic                 palr46w.ttf    URWPalladioL-Ital.ttf +        Palatino      It                       URWPalladioL Italic       
59 Palatino-Roman                  palr45w.ttf    URWPalladioL-Roma.ttf +        Palatino     Rmn                       URWPalladioL              
60 Symbol                          symps__.ttf    StandardSymL.ttf               Symbol                                 StandardSymL              
61 Symbolmt                          n/a          missing.                       SymbolPS                               file missing. not found   
62 Times-Bold                      timr65w.ttf    NimbusRomNo9L-Medi.ttf  +       Times         Bd                       NimbusRomNo9L Bold        
63 Times-BoldItalic                timr66w.ttf    NimbusRomNo9L-MediItal.ttf +    Times       BdIt                       NimbusRomNo9L Bold Italic 
64 Times-Italic                    timr46w.ttf    NimbusRomNo9L-ReguItal.ttf +    Times         It                       NimbusRomNo9L Italic      
65 Times-Roman                     timr45w.ttf    NimbusRomNo9L-Regu.ttf  +       Times        Rmn                       NimbusRomNo9L             
66 times new roman*                       N/A     NimbusRomanNo9-Reg.ttf        TimesNewRmn                            NimbusRomanNo9            
67 times new roman bd.*                   N/A     NimbusRomanNo9-Med.ttf        TimesNewRmn   Bd                       NimbusRomanNo9 Medium     
68 times new roman bd. it.*               N/A     NimbusRomanNo9-MedIta.ttf     TimesNewRmn BdIt                       NimbusRomanNo9 Medium Ital
69 times new roman it.*                   N/A     NimbusRomanNo9-Ita.ttf         TimesNewRmn   It                       NimbusRomanNo9 Italic     
70 Univers-Bold                    unvr65w.ttf    U001-Bol.ttf                   Univers       Bd                       U001 Bold                 
71 Univers-BoldItalic              unvr66w.ttf    U001-BolIta.ttf                Univers     BdIt                       U001 Bold Italic          
72 Univers-Condensed-Bold          unvr67w.ttf    U001Con-Bol.ttf                Univers     CdBd                       U001Con Bold              
73 Univers-Condensed-BoldItalic    unvr68w.ttf    U001Con-BolIta.ttf             Univers   CdBdIt                       U001Con Bold Italic       
74 Univers-Condensed-Medium        unvr57w.ttf    U001Con-Reg.ttf                Univers     CdMd                       U001Con                   
75 Univers-Condensed-MediumItalic  unvr58w.ttf    U001Con-Ita.ttf                Univers   CdMdIt                       U001Con Italic            
76 Univers-Medium                  unvr55w.ttf    U001-Reg.ttf                   Univers       Md                       U001                      
77 Univers-MediumItalic            unvr56w.ttf    U001-Ita.ttf                   Univers     MdIt                       U001 Italic               
78 Wingdings                         N/A           missing (patent)              missing from test                      
79 ZapfChancery-MediumItalic       chanc___.ttf   URWChanceryL-MediItal.ttf +    ZapfChanceryMdIt                       URWChanceryLMed Italic    
80 ZapfDingbats                    dings___.ttf   Dingbats.ttf                   Dingbats

* These did not ship with hp screen fonts.

+ New with the 35 fonts.

- original 45 fonts.

Notes:

LetterGothic-BolIta was shipped as a screen font that does not
correspond with a printer font.

#endif /* NEVERCOMPILED */
