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
    {"NimbusMono", {'C','o','u','r','i','e','r',' ',' ',' ',' ',' ',' ',' ',' ',' '},
     {0, 0, {60, 11998}, 0, style_word(0, 0, 0), 0, face_val(3, agfa)},
     cc_alphabetic},
    {"NimbusRomanNo4 Light", {'C','G',' ','T','i','m','e','s',' ',' ',' ',' ',' ',' ',' ',' '},
     {0, 1, {29, 24413}, 0, style_word(0, 0, 0), 0, face_val(5, agfa)},
     cc_alphabetic},
    {"NimbusRomanNo4 Bold", {'C','G',' ','T','i','m','e','s',' ',' ',' ',' ',' ',' ','B','d'},
     {0, 1, {29, 24413}, 0, style_word(0, 0, 0), 3, face_val(5, agfa)},
     cc_alphabetic},
    {"NimbusRomanNo4 Light Italic", {'C','G',' ','T','i','m','e','s',' ',' ',' ',' ',' ',' ','I','t'},
     {0, 1, {29, 24413}, 0, style_word(1, 0, 0), 0, face_val(5, agfa)},
     cc_alphabetic},
    {"NimbusRomanNo4 Bold Italic", {'C','G',' ','T','i','m','e','s',' ',' ',' ',' ','B','d','I','t'},
     {0, 1, {29, 24413}, 0, style_word(1, 0, 0), 3, face_val(5, agfa)},
     cc_alphabetic},
    {"URWClassico", {'C','G',' ','O','m','e','g','a',' ',' ',' ',' ',' ',' ',' ',' '},
     {0, 1, {27, 26052}, 0, style_word(0, 0, 0), 0, face_val(17, agfa)},
     cc_alphabetic},
    {"URWClassico Bold", {'C','G',' ','O','m','e','g','a',' ',' ',' ',' ',' ',' ','B','d'},
     {0, 1, {27, 26052}, 0, style_word(0, 0, 0), 3, face_val(17, agfa)},
     cc_alphabetic},
    {"URWClassico Italic", {'C','G',' ','O','m','e','g','a',' ',' ',' ',' ',' ',' ','I','t'},
     {0, 1, {27, 26052}, 0, style_word(1, 0, 0), 0, face_val(17, agfa)},
     cc_alphabetic},
    {"URWClassico Bold Italic", {'C','G',' ','O','m','e','g','a',' ',' ',' ',' ','B','d','I','t'},
     {0, 1, {27, 26052}, 0, style_word(1, 0, 0), 3, face_val(17, agfa)},
     cc_alphabetic},
    {"Coronet", {'C','o','r','o','n','e','t',' ',' ',' ',' ',' ',' ',' ',' ',' '},
     {0, 1, {20, 35531}, 0, style_word(1, 0, 0), 0, face_val(20, agfa)},
     cc_alphabetic},
    {"ClarendonURWBolCon", {'C','l','a','r','e','n','d','o','n',' ',' ',' ','C','d','B','d'},
     {0, 1, {22, 32550}, 0, style_word(0, 1, 0), 3, face_val(44, agfa)},
     cc_alphabetic},
    {"U001", {'U','n','i','v','e','r','s',' ',' ',' ',' ',' ',' ',' ','M','d'},
     {0, 1, {33, 21684}, 0, style_word(0, 0, 0), 0, face_val(52, agfa)},
     cc_alphabetic},
    {"U001 Bold", {'U','n','i','v','e','r','s',' ',' ',' ',' ',' ',' ',' ','B','d'},
     {0, 1, {33, 21684}, 0, style_word(0, 0, 0), 3, face_val(52, agfa)},
     cc_alphabetic},
    {"U001 Italic", {'U','n','i','v','e','r','s',' ',' ',' ',' ',' ','M','d','I','t'},
     {0, 1, {33, 21684}, 0, style_word(1, 0, 0), 0, face_val(52, agfa)},
     cc_alphabetic},
    {"U001 Bold Italic", {'U','n','i','v','e','r','s',' ',' ',' ',' ',' ','B','d','I','t'},
     {0, 1, {33, 21684}, 0, style_word(1, 0, 0), 3, face_val(52, agfa)},
     cc_alphabetic},
    {"U001Con", {'U','n','i','v','e','r','s',' ',' ',' ',' ',' ','C','d','M','d'},
     {0, 1, {22, 32550}, 0, style_word(0, 1, 0), 0, face_val(52, agfa)},
     cc_alphabetic},
    {"U001Con Bold", {'U','n','i','v','e','r','s',' ',' ',' ',' ',' ','C','d','B','d'},
     {0, 1, {22, 32550}, 0, style_word(0, 1, 0), 3, face_val(52, agfa)},
     cc_alphabetic},
    {"U001Con Italic", {'U','n','i','v','e','r','s',' ',' ',' ','C','d','M','d','I','t'},
     {0, 1, {22, 32550}, 0, style_word(1, 1, 0), 0, face_val(52, agfa)},
     cc_alphabetic},
    {"U001Con Bold Italic", {'U','n','i','v','e','r','s',' ',' ',' ','C','d','B','d','I','t'},
     {0, 1, {22, 32550}, 0, style_word(1, 1, 0), 3, face_val(52, agfa)},
     cc_alphabetic},
    {"AntiqueOlive", {'A','n','t','i','q','O','l','i','v','e',' ',' ',' ',' ',' ',' '},
     {0, 1, {29, 24413}, style_word(0, 0, 0), 0, face_val(72, agfa)},
     cc_alphabetic},
    {"AntiqueOlive Bold", {'A','n','t','i','q','O','l','i','v','e',' ',' ',' ',' ','B','d'},
     {0, 1, {33, 21684}, style_word(0, 0, 0), 3,
      face_val(72, agfa)}, cc_alphabetic},
    {"AntiqueOlive Italic", {'A','n','t','i','q','O','l','i','v','e',' ',' ',' ',' ','I','t'},
     {0, 1, {29, 24413}, 0, style_word(1, 0, 0), 0, face_val(72, agfa)},
     cc_alphabetic},
    {"GaramondNo8", {'G','a','r','a','m','o','n','d',' ','A','n','t','i','q','u','a'},
     {0, 1, {25, 27874}, 0, style_word(0, 0, 0), 0, face_val(101, agfa)}, cc_alphabetic},
    {"GaramondNo8 Medium", {'G','a','r','a','m','o','n','d',' ',' ',' ',' ',' ','H','l','b'},
     {0, 1, {27, 26052}, 0, style_word(0, 0, 0), 3, face_val(101, agfa)}, cc_alphabetic},
    {"GaramondNo8 Italic", {'G','a','r','a','m','o','n','d',' ',' ',' ',' ','K','r','s','v'},
     {0, 1, {23, 30031}, 0, style_word(1, 0, 0), 0,
      face_val(101, agfa)}, cc_alphabetic},
    {"GaramondNo8 Medium Italic", {'G','a','r','a','m','o','n','d',' ','K','r','s','v','H','l','b'},
     {0, 1, {25, 27874}, 0, style_word(1, 0, 0), 3,
      face_val(101, agfa)}, cc_alphabetic},
    {"Mauritius", {'M','a','r','i','g','o','l','d',' ',' ',' ',' ',' ',' ',' ',' '},
     {0, 1, {22, 32550}, 0, style_word(0, 0, 0), 0, face_val(201, agfa)},
     cc_alphabetic},
    {"A028 Medium", {'A','l','b','e','r','t','u','s',' ',' ',' ',' ',' ',' ','M','d'},
     {0, 1, {31, 22968}, 0, style_word(0, 0, 0), 1,
      face_val(266, agfa)}, cc_alphabetic},
    {"A028 Extrabold", {'A','l','b','e','r','t','u','s',' ',' ',' ',' ',' ',' ','X','b'},
     {0, 1, {36, 19530}, 0, style_word(0, 0, 0), 4,
      face_val(266, agfa)}, cc_alphabetic},
    {"A030", {'A','r','i','a','l',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' '},
     {0, 1, {27, 25914}, 0, style_word(0, 0, 0), 0,
      face_val(218, agfa)}, cc_alphabetic},
    {"A030 Bold", {'A','r','i','a','l',' ',' ',' ',' ',' ',' ',' ',' ',' ','B','d'},
     {0, 1, {27, 25914}, 0, style_word(0, 0, 0), 3,
      face_val(218, agfa)}, cc_alphabetic},
    {"A030 Italic", {'A','r','i','a','l',' ',' ',' ',' ',' ',' ',' ',' ',' ','I','t'},
     {0, 1, {27, 25914}, 0, style_word(1, 0, 0), 0, face_val(218, agfa)}, cc_alphabetic},
    {"A030 Bold Italic", {'A','r','i','a','l',' ',' ',' ',' ',' ',' ',' ','B','d','I','t'},
     {0, 1, {27, 25914}, 0, style_word(1, 0, 0), 3, face_val(218, agfa)}, cc_alphabetic},
    {"NimbusRomanNo9", {'T','i','m','e','s','N','e','w','R','m','n',' ',' ',' ',' ',' '},
     {0, 1, {25, 28800}, 0, style_word(0, 0, 0), 0, face_val(517, agfa)}, 
     cc_alphabetic},
    {"NimbusRomanNo9 Medium",{'T','i','m','e','s','N','e','w','R','m','n',' ',' ',' ','B','d'},
     {0, 1, {25, 28800}, 0, style_word(0, 0, 0), 3,
      face_val(517, agfa)}, cc_alphabetic},
    {"NimbusRomanNo9 Italic", {'T','i','m','e','s','N','e','w','R','m','n',' ',' ',' ','I','t'},
     {0, 1, {25, 28800}, 0, style_word(1, 0, 0), 0,
      face_val(517, agfa)}, cc_alphabetic},
    {"NimbusRomanNo9 Medium Italic", {'T','i','m','e','s','N','e','w','R','m','n',' ','B','d','I','t'},
     {0, 1, {25, 28800}, 0, style_word(1, 0, 0), 3,
      face_val(517, agfa)}, cc_alphabetic},
    {"StandardSymL", {'S','y','m','b','o','l',' ',' ',' ',' ',' ',' ',' ',' ',' ',' '},
     {621, 1, {25, 28800}, 0, style_word(0, 0, 0), 0,
      face_val(302, agfa)}, cc_symbol},
    {"Dingbats", {'W','i','n','g','d','i','n','g','s',' ',' ',' ',' ',' ',' ',' '},
     {18540, 1, {27, 25914},0, style_word(0, 0, 0), 0,
      face_val(2730, agfa)}, cc_dingbats},
    {"NimbusMono Bold", {'C','o','u','r','i','e','r',' ',' ',' ',' ',' ',' ',' ','B','d'},
     {0, 0, {60, 11998}, 0, style_word(0, 0, 0), 3, face_val(3, agfa)}, 
     cc_alphabetic },
    {"NimbusMono Italic", {'C','o','u','r','i','e','r',' ',' ',' ',' ',' ',' ',' ','I','t'},
     {0, 0, {60, 11998}, 0, style_word(1, 0, 0), 0, face_val(3, agfa)},
     cc_alphabetic },
    {"NimbusMono Bold Italic", {'C','o','u','r','i','e','r',' ',' ',' ',' ',' ','B','d','I','t'},
     {0, 0, {60, 11998}, 0, style_word(1, 0, 0), 3, face_val(3, agfa)},
     cc_alphabetic},
    {"LetterGothic", {'L','e','t','t','e','r','G','o','t','h','i','c',' ',' ',' ',' '},
     {0, 0, {54, 13332}, 0, style_word(0, 0, 0), 0, face_val(6, agfa)},
     cc_alphabetic},
    {"LetterGothic Bold",
     {'L','e','t','t','e','r','G','o','t','h','i','c',' ',' ','B','d'},
     {0, 0, {50, 14400}, 0, style_word(0, 0, 0), 3, face_val(6, agfa)},
     cc_alphabetic},
    {"LetterGothic Italic", 
     {'L','e','t','t','e','r','G','o','t','h','i','c',' ',' ','I','t'},
     {0, 0, {50, 14400}, 0, style_word(1, 0, 0), 0, face_val(6, agfa)},
     cc_alphabetic},
    {"Lpsb1", {'L','i','n','e',' ','P','r','i','n','t','e','r',' ',' ','0','N'},
     {0, 0, {60, 11998}, 0, style_word(0, 0, 0), 0,
      face_val(218, agfa)}, cc_alphabetic},
    {"Lpsb2", {'L','i','n','e',' ','P','r','i','n','t','e','r',' ','1','0','U'},
     {0, 0, {60, 11998}, 0, style_word(0, 0, 0), 0,
      face_val(218, agfa)}, cc_alphabetic},
    {"Lpsb3", {'L','i','n','e',' ','P','r','i','n','t','e','r',' ','1','1','U'},
     {0, 0, {60, 11998}, 0, style_word(0, 0, 0), 0,
      face_val(218, agfa)}, cc_alphabetic},
    {"Lpsb4", {'L','i','n','e',' ','P','r','i','n','t','e','r',' ','1','2','U'},
     {0, 0, {60, 11998}, 0, style_word(0, 0, 0), 0,
      face_val(218, agfa)}, cc_alphabetic},
    {"Lpsb5", {'L','i','n','e',' ','P','r','i','n','t','e','r',' ',' ','1','U'},
     {0, 0, {60, 11998}, 0, style_word(0, 0, 0), 0,
      face_val(218, agfa)}, cc_alphabetic},
    {"Lpsb6", {'L','i','n','e',' ','P','r','i','n','t','e','r',' ',' ','2','N'},
     {0, 0, {60, 11998}, 0, style_word(0, 0, 0), 0,
      face_val(218, agfa)}, cc_alphabetic},
    {"Lpsb7", {'L','i','n','e',' ','P','r','i','n','t','e','r',' ',' ','5','N'},
     {0, 0, {60, 11998}, 0, style_word(0, 0, 0), 0,
      face_val(218, agfa)}, cc_alphabetic},
    {"Lpsb8", {'L','i','n','e',' ','P','r','i','n','t','e','r',' ',' ','8','U'},
     {0, 0, {60, 11998}, 0, style_word(0, 0, 0), 0,
      face_val(218, agfa)}, cc_alphabetic},
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
    {"Courier", {'C','o','u','r','i','e','r',' ',' ',' ',' ',' ',' ',' ',' ',' '},
     {0, 0, pitch_1, 0, style_word(0, 0, 0), 0, face_val(3, agfa)},
     cc_alphabetic},
    {"CG Times", {'C','G',' ','T','i','m','e','s',' ',' ',' ',' ',' ',' ',' ',' '},
     {0, 1, pitch_1, 0, style_word(0, 0, 0), 0, face_val(5, agfa)},
     cc_alphabetic},
    {"CG Times Bold", {'C','G',' ','T','i','m','e','s',' ',' ',' ',' ',' ',' ','B','d'},
     {0, 1, pitch_1, 0, style_word(0, 0, 0), 3, face_val(5, agfa)},
     cc_alphabetic},
    {"CG Times Italic", {'C','G',' ','T','i','m','e','s',' ',' ',' ',' ',' ',' ','I','t'},
     {0, 1, pitch_1, 0, style_word(1, 0, 0), 0, face_val(5, agfa)},
     cc_alphabetic},
    {"CG Times Bold Italic", {'C','G',' ','T','i','m','e','s',' ',' ',' ',' ','B','d','I','t'},
     {0, 1, pitch_1, 0, style_word(1, 0, 0), 3, face_val(5, agfa)},
     cc_alphabetic},
    {"CG Omega", {'C','G',' ','O','m','e','g','a',' ',' ',' ',' ',' ',' ',' ',' '},
     {0, 1, pitch_1, 0, style_word(0, 0, 0), 0, face_val(17, agfa)},
     cc_alphabetic},
    {"CG Omega Bold", {'C','G',' ','O','m','e','g','a',' ',' ',' ',' ',' ',' ','B','d'},
     {0, 1, pitch_1, 0, style_word(0, 0, 0), 3, face_val(17, agfa)},
     cc_alphabetic},
    {"CG Omega Italic", {'C','G',' ','O','m','e','g','a',' ',' ',' ',' ',' ',' ','I','t'},
     {0, 1, pitch_1, 0, style_word(1, 0, 0), 0, face_val(17, agfa)},
     cc_alphabetic},
    {"CG Omega Bold Italic", {'C','G',' ','O','m','e','g','a',' ',' ',' ',' ','B','d','I','t'},
     {0, 1, pitch_1, 0, style_word(1, 0, 0), 3, face_val(17, agfa)},
     cc_alphabetic},
    {"Coronet", {'C','o','r','o','n','e','t',' ',' ',' ',' ',' ',' ',' ',' ',' '},
     {0, 1, pitch_1, 0, style_word(1, 0, 0), 0, face_val(20, agfa)},
     cc_alphabetic},
    {"Clarendon Condensed Bold", {'C','l','a','r','e','n','d','o','n',' ',' ',' ','C','d','B','d'},
     {0, 1, pitch_1, 0, style_word(0, 1, 0), 3, face_val(44, agfa)},
     cc_alphabetic},
    {"Univers Medium", {'U','n','i','v','e','r','s',' ',' ',' ',' ',' ',' ',' ','M','d'},
     {0, 1, pitch_1, 0, style_word(0, 0, 0), 0, face_val(52, agfa)},
     cc_alphabetic},
    {"Univers Bold", {'U','n','i','v','e','r','s',' ',' ',' ',' ',' ',' ',' ','B','d'},
     {0, 1, pitch_1, 0, style_word(0, 0, 0), 3, face_val(52, agfa)},
     cc_alphabetic},
    {"Univers Medium Italic", {'U','n','i','v','e','r','s',' ',' ',' ',' ',' ','M','d','I','t'},
     {0, 1, pitch_1, 0, style_word(1, 0, 0), 0, face_val(52, agfa)},
     cc_alphabetic},
    {"Univers Bold Italic", {'U','n','i','v','e','r','s',' ',' ',' ',' ',' ','B','d','I','t'},
     {0, 1, pitch_1, 0, style_word(1, 0, 0), 3, face_val(52, agfa)},
     cc_alphabetic},
    {"Univers Condensed Medium", {'U','n','i','v','e','r','s',' ',' ',' ',' ',' ','C','d','M','d'},
     {0, 1, pitch_1, 0, style_word(0, 1, 0), 0, face_val(52, agfa)},
     cc_alphabetic},
    {"Univers Condensed Bold", {'U','n','i','v','e','r','s',' ',' ',' ',' ',' ','C','d','B','d'},
     {0, 1, pitch_1, 0, style_word(0, 1, 0), 3, face_val(52, agfa)},
     cc_alphabetic},
    {"Univers Condensed Medium Italic", {'U','n','i','v','e','r','s',' ',' ',' ','C','d','M','d','I','t'},
     {0, 1, pitch_1, 0, style_word(1, 1, 0), 0, face_val(52, agfa)},
     cc_alphabetic},
    {"Univers Condensed Bold Italic", {'U','n','i','v','e','r','s',' ',' ',' ','C','d','B','d','I','t'},
     {0, 1, pitch_1, 0, style_word(1, 1, 0), 3, face_val(52, agfa)},
     cc_alphabetic},
    {"Antique Olive", {'A','n','t','i','q','O','l','i','v','e',' ',' ',' ',' ',' ',' '},
     {0, 1, pitch_1, 0, style_word(0, 0, 0), 0, face_val(72, agfa)},
     cc_alphabetic},
    {"Antique Olive Bold", {'A','n','t','i','q','O','l','i','v','e',' ',' ',' ',' ','B','d'},
     {0, 1, pitch_1, 0, style_word(0, 0, 0), 3,
      face_val(72, agfa)}, cc_alphabetic},
    {"Antique Olive Italic", {'A','n','t','i','q','O','l','i','v','e',' ',' ',' ',' ','I','t'},
     {0, 1, pitch_1, 0, style_word(1, 0, 0), 0, face_val(72, agfa)},
     cc_alphabetic},
    {"Garamond Antiqua", {'G','a','r','a','m','o','n','d',' ','A','n','t','i','q','u','a'},
     {0, 1, pitch_1, 0, style_word(0, 0, 0), 0, face_val(101, agfa)}, cc_alphabetic},
    {"Garamond Halbfett", {'G','a','r','a','m','o','n','d',' ',' ',' ',' ',' ','H','l','b'},
     {0, 1, pitch_1, 0, style_word(0, 0, 0), 3, face_val(101, agfa)}, cc_alphabetic},
    {"Garamond Kursiv", {'G','a','r','a','m','o','n','d',' ',' ',' ',' ','K','r','s','v'},
     {0, 1, pitch_1, 0, style_word(1, 0, 0), 0,
      face_val(101, agfa)}, cc_alphabetic},
    {"Garamond Kursiv Halbfett", {'G','a','r','a','m','o','n','d',' ','K','r','s','v','H','l','b'},
     {0, 1, pitch_1, 0, style_word(1, 0, 0), 3,
      face_val(101, agfa)}, cc_alphabetic},
    {"Marigold", {'M','a','r','i','g','o','l','d',' ',' ',' ',' ',' ',' ',' ',' '},
     {0, 1, pitch_1, 0, style_word(0, 0, 0), 0, face_val(201, agfa)},
     cc_alphabetic},
    {"Albertus Medium", {'A','l','b','e','r','t','u','s',' ',' ',' ',' ',' ',' ','M','d'},
     {0, 1, pitch_1, 0, style_word(0, 0, 0), 1,
      face_val(266, agfa)}, cc_alphabetic},
    {"Albertus Extra Bold", {'A','l','b','e','r','t','u','s',' ',' ',' ',' ',' ',' ','X','b'},
     {0, 1, pitch_1, 0, style_word(0, 0, 0), 4,
      face_val(266, agfa)}, cc_alphabetic},
    {"Arial", {'A','r','i','a','l',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' '},
     {0, 1, pitch_1, 0, style_word(0, 0, 0), 0,
      face_val(218, agfa)}, cc_alphabetic},
    {"Arial Bold", {'A','r','i','a','l',' ',' ',' ',' ',' ',' ',' ',' ',' ','B','d'},
     {0, 1, pitch_1, 0, style_word(0, 0, 0), 3,
      face_val(218, agfa)}, cc_alphabetic},
    {"Arial Italic", {'A','r','i','a','l',' ',' ',' ',' ',' ',' ',' ',' ',' ','I','t'},
     {0, 1, pitch_1, 0, style_word(1, 0, 0), 0, face_val(218, agfa)}, cc_alphabetic},
    {"Arial Bold Italic", {'A','r','i','a','l',' ',' ',' ',' ',' ',' ',' ','B','d','I','t'},
     {0, 1, pitch_1, 0, style_word(1, 0, 0), 3, face_val(218, agfa)}, cc_alphabetic},
    {"Times New Roman", {'T','i','m','e','s','N','e','w','R','m','n',' ',' ',' ',' ',' '},
     {0, 1, pitch_1, 0, style_word(0, 0, 0), 0, face_val(517, agfa)}, 
     cc_alphabetic},
    {"Times New Roman Bold",{'T','i','m','e','s','N','e','w','R','m','n',' ',' ',' ','B','d'},
     {0, 1, pitch_1, 0, style_word(0, 0, 0), 3,
      face_val(517, agfa)}, cc_alphabetic},
    {"Times New Roman Italic", {'T','i','m','e','s','N','e','w','R','m','n',' ',' ',' ','I','t'},
     {0, 1, pitch_1, 0, style_word(1, 0, 0), 0,
      face_val(517, agfa)}, cc_alphabetic},
    {"Times New Roman Bold Italic", {'T','i','m','e','s','N','e','w','R','m','n',' ','B','d','I','t'},
     {0, 1, pitch_1, 0, style_word(1, 0, 0), 3,
      face_val(517, agfa)}, cc_alphabetic},
    {"Symbol", {'S','y','m','b','o','l',' ',' ',' ',' ',' ',' ',' ',' ',' ',' '},
     {621, 1, pitch_1, 0, style_word(0, 0, 0), 0,
      face_val(302, agfa)}, cc_symbol},
    {"Wingdings", {'W','i','n','g','d','i','n','g','s',' ',' ',' ',' ',' ',' ',' '},
     {18540, 1, pitch_1,0, style_word(0, 0, 0), 0,
      face_val(2730, agfa)}, cc_dingbats},
    {"Courier Bold", {'C','o','u','r','i','e','r',' ',' ',' ',' ',' ',' ',' ','B','d'},
     {0, 0, pitch_1, 0, style_word(0, 0, 0), 3, face_val(3, agfa)}, 
     cc_alphabetic },
    {"Courier Italic", {'C','o','u','r','i','e','r',' ',' ',' ',' ',' ',' ',' ','I','t'},
     {0, 0, pitch_1, 0, style_word(1, 0, 0), 0, face_val(3, agfa)},
     cc_alphabetic },
    {"Courier Bold Italic", {'C','o','u','r','i','e','r',' ',' ',' ',' ',' ','B','d','I','t'},
     {0, 0, pitch_1, 0, style_word(1, 0, 0), 3, face_val(3, agfa)},
     cc_alphabetic},
    {"Letter Gothic", {'L','e','t','t','e','r','G','o','t','h','i','c',' ',' ',' ',' '},
     {0, 0, pitch_1, 0, style_word(0, 0, 0), 0, face_val(6, agfa)},
     cc_alphabetic},
    {"Letter Gothic Bold",
     {'L','e','t','t','e','r','G','o','t','h','i','c',' ',' ','B','d'},
     {0, 0, pitch_1, 0, style_word(0, 0, 0), 3, face_val(6, agfa)},
     cc_alphabetic},
    {"Letter Gothic Italic", 
     {'L','e','t','t','e','r','G','o','t','h','i','c',' ',' ','I','t'},
     {0, 0, pitch_1, 0, style_word(1, 0, 0), 0, face_val(6, agfa)},
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

const int pl_resident_font_table_count = countof(resident_table);
