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

/* plfont.c */
/* PCL font handling library -- operations on entire fonts */
#include "memory_.h"
#include "stdio_.h"
#include "gdebug.h"
#include "gp.h"
#include "gserror.h"
#include "gserrors.h"
#include "gstypes.h"
#include "gsmemory.h"
#include "gsstruct.h"
#include "gsmatrix.h"
#include "gsstate.h"
#include "gschar.h"
#include "gsutil.h"
#include "gxfont.h"
#include "gxfont42.h"
#include "plfont.h"
#include "plvalue.h"

/* Structure descriptors */
private_st_pl_font();

/* Imported procedures */
int gs_type42_get_metrics(gs_font_type42 *pfont, uint glyph_index,
  float psbw[4]);

/* Define accessors for unaligned, big-endian quantities. */
#define u16(bptr) pl_get_uint16(bptr)
#define s16(bptr) pl_get_int16(bptr)
/**** ASSUME uint >= 32 BITS ****/
#define u32(bptr) (uint)pl_get_uint32(bptr)

/* ---------------- Utilities ---------------- */

/* Free a font.  This is the freeing procedure in the font dictionary. */
void
pl_free_font(gs_memory_t *mem, void *plf, client_name_t cname)
{	pl_font_t *plfont = plf;
	/* Free the characters. */
        if ( !plfont->data_are_permanent )
	  { if ( plfont->glyphs.table )
	     { uint i;
	       for ( i = plfont->glyphs.size; i > 0; )
	         { void *data = plfont->glyphs.table[--i].data;
	           if ( data )
		     gs_free_object(mem, data, cname);
	         }  
	     }
	     gs_free_object(mem, (void *)plfont->header, cname);
	     plfont->header = 0; /* see hack note above */
	  }
	/* Free the font data itself. */
	gs_free_object(mem, (void *)plfont->char_glyphs.table, cname);
	gs_free_object(mem, (void *)plfont->glyphs.table, cname);
	if ( plfont->pfont )	/* might be only partially constructed */
	  { gs_purge_font(plfont->pfont);
	    gs_free_object(mem, plfont->pfont, cname);
	  }
	if ( plfont->font_file ) {
	    gs_free_object(mem, plfont->font_file, cname);
	    plfont->font_file = 0;
	}
	gs_free_object(mem, plf, cname);
}

/* ---------------- Library callbacks ---------------- */

const char *glyph_names[] = {
    /*  Glyf   0 -> Mac Glyph # 0*/ ".notdef",
    /*  Glyf   1 -> Mac Glyph # 0*/ ".notdef",
    /*          Glyf   2 -> Mac Glyph # 0*/ ".notdef",
    /*          Glyf   3 -> Mac Glyph # 3*/ "space",
    /*          Glyf   4 -> Mac Glyph # 4*/ "exclam",
    /*          Glyf   5 -> Mac Glyph # 5*/ "quotedbl",
    /*          Glyf   6 -> Mac Glyph # 6*/ "numbersign",
    /*          Glyf   7 -> Mac Glyph # 7*/ "dollar",
    /*          Glyf   8 -> Mac Glyph # 8*/ "percent",
    /*          Glyf   9 -> Mac Glyph # 9*/ "ampersand",
    /*          Glyf  10 -> Mac Glyph # 10*/ "quotesingle",
    /*          Glyf  11 -> Mac Glyph # 11*/ "parenleft",
    /*          Glyf  12 -> Mac Glyph # 12*/ "parenright",
    /*          Glyf  13 -> Mac Glyph # 13*/ "asterisk",
    /*          Glyf  14 -> Mac Glyph # 14*/ "plus",
    /*          Glyf  15 -> Mac Glyph # 15*/ "comma",
    /*          Glyf  16 -> Mac Glyph # 16*/ "hyphen",
    /*          Glyf  17 -> Mac Glyph # 17*/ "period",
    /*          Glyf  18 -> Mac Glyph # 18*/ "slash",
    /*          Glyf  19 -> Mac Glyph # 19*/ "zero",
    /*          Glyf  20 -> Mac Glyph # 20*/ "one",
    /*          Glyf  21 -> Mac Glyph # 21*/ "two",
    /*          Glyf  22 -> Mac Glyph # 22*/ "three",
    /*          Glyf  23 -> Mac Glyph # 23*/ "four",
    /*          Glyf  24 -> Mac Glyph # 24*/ "five",
    /*          Glyf  25 -> Mac Glyph # 25*/ "six",
    /*          Glyf  26 -> Mac Glyph # 26*/ "seven",
    /*          Glyf  27 -> Mac Glyph # 27*/ "eight",
    /*          Glyf  28 -> Mac Glyph # 28*/ "nine",
    /*          Glyf  29 -> Mac Glyph # 29*/ "colon",
    /*          Glyf  30 -> Mac Glyph # 30*/ "semicolon",
    /*          Glyf  31 -> Mac Glyph # 31*/ "less",
    /*          Glyf  32 -> Mac Glyph # 32*/ "equal",
    /*          Glyf  33 -> Mac Glyph # 33*/ "greater",
    /*          Glyf  34 -> Mac Glyph # 34*/ "question",
    /*          Glyf  35 -> Mac Glyph # 35*/ "at",
    /*          Glyf  36 -> Mac Glyph # 36*/ "A",
    /*          Glyf  37 -> Mac Glyph # 37*/ "B",
    /*          Glyf  38 -> Mac Glyph # 38*/ "C",
    /*          Glyf  39 -> Mac Glyph # 39*/ "D",
    /*          Glyf  40 -> Mac Glyph # 40*/ "E",
    /*          Glyf  41 -> Mac Glyph # 41*/ "F",
    /*          Glyf  42 -> Mac Glyph # 42*/ "G",
    /*          Glyf  43 -> Mac Glyph # 43*/ "H",
    /*          Glyf  44 -> Mac Glyph # 44*/ "I",
    /*          Glyf  45 -> Mac Glyph # 45*/ "J",
    /*          Glyf  46 -> Mac Glyph # 46*/ "K",
    /*          Glyf  47 -> Mac Glyph # 47*/ "L",
    /*          Glyf  48 -> Mac Glyph # 48*/ "M",
    /*          Glyf  49 -> Mac Glyph # 49*/ "N",
    /*          Glyf  50 -> Mac Glyph # 50*/ "O",
    /*          Glyf  51 -> Mac Glyph # 51*/ "P",
    /*          Glyf  52 -> Mac Glyph # 52*/ "Q",
    /*          Glyf  53 -> Mac Glyph # 53*/ "R",
    /*          Glyf  54 -> Mac Glyph # 54*/ "S",
    /*          Glyf  55 -> Mac Glyph # 55*/ "T",
    /*          Glyf  56 -> Mac Glyph # 56*/ "U",
    /*          Glyf  57 -> Mac Glyph # 57*/ "V",
    /*          Glyf  58 -> Mac Glyph # 58*/ "W",
    /*          Glyf  59 -> Mac Glyph # 59*/ "X",
    /*          Glyf  60 -> Mac Glyph # 60*/ "Y",
    /*          Glyf  61 -> Mac Glyph # 61*/ "Z",
    /*          Glyf  62 -> Mac Glyph # 62*/ "bracketleft",
    /*          Glyf  63 -> Mac Glyph # 63*/ "backslash",
    /*          Glyf  64 -> Mac Glyph # 64*/ "bracketright",
    /*          Glyf  65 -> Mac Glyph # 65*/ "asciicircum",
    /*          Glyf  66 -> Mac Glyph # 66*/ "underscore",
    /*          Glyf  67 -> Mac Glyph # 67*/ "grave",
    /*          Glyf  68 -> Mac Glyph # 68*/ "a",
    /*          Glyf  69 -> Mac Glyph # 69*/ "b",
    /*          Glyf  70 -> Mac Glyph # 70*/ "c",
    /*          Glyf  71 -> Mac Glyph # 71*/ "d",
    /*          Glyf  72 -> Mac Glyph # 72*/ "e",
    /*          Glyf  73 -> Mac Glyph # 73*/ "f",
    /*          Glyf  74 -> Mac Glyph # 74*/ "g",
    /*          Glyf  75 -> Mac Glyph # 75*/ "h",
    /*          Glyf  76 -> Mac Glyph # 76*/ "i",
    /*          Glyf  77 -> Mac Glyph # 77*/ "j",
    /*          Glyf  78 -> Mac Glyph # 78*/ "k",
    /*          Glyf  79 -> Mac Glyph # 79*/ "l",
    /*          Glyf  80 -> Mac Glyph # 80*/ "m",
    /*          Glyf  81 -> Mac Glyph # 81*/ "n",
    /*          Glyf  82 -> Mac Glyph # 82*/ "o",
    /*          Glyf  83 -> Mac Glyph # 83*/ "p",
    /*          Glyf  84 -> Mac Glyph # 84*/ "q",
    /*          Glyf  85 -> Mac Glyph # 85*/ "r",
    /*          Glyf  86 -> Mac Glyph # 86*/ "s",
    /*          Glyf  87 -> Mac Glyph # 87*/ "t",
    /*          Glyf  88 -> Mac Glyph # 88*/ "u",
    /*          Glyf  89 -> Mac Glyph # 89*/ "v",
    /*          Glyf  90 -> Mac Glyph # 90*/ "w",
    /*          Glyf  91 -> Mac Glyph # 91*/ "x",
    /*          Glyf  92 -> Mac Glyph # 92*/ "y",
    /*          Glyf  93 -> Mac Glyph # 93*/ "z",
    /*          Glyf  94 -> Mac Glyph # 94*/ "braceleft",
    /*          Glyf  95 -> Mac Glyph # 95*/ "bar",
    /*          Glyf  96 -> Mac Glyph # 96*/ "braceright",
    /*          Glyf  97 -> Mac Glyph # 97*/ "asciitilde",
    /*          Glyf  98 -> Mac Glyph # 98*/ "Adieresis",
    /*          Glyf  99 -> Mac Glyph # 99*/ "Aring",
    /*          Glyf 100 -> Mac Glyph # 100*/ "Ccedilla",
    /*          Glyf 101 -> Mac Glyph # 101*/ "Eacute",
    /*          Glyf 102 -> Mac Glyph # 102*/ "Ntilde",
    /*          Glyf 103 -> Mac Glyph # 103*/ "Odieresis",
    /*          Glyf 104 -> Mac Glyph # 104*/ "Udieresis",
    /*          Glyf 105 -> Mac Glyph # 105*/ "aacute",
    /*          Glyf 106 -> Mac Glyph # 106*/ "agrave",
    /*          Glyf 107 -> Mac Glyph # 107*/ "acircumflex",
    /*          Glyf 108 -> Mac Glyph # 108*/ "adieresis",
    /*          Glyf 109 -> Mac Glyph # 109*/ "atilde",
    /*          Glyf 110 -> Mac Glyph # 110*/ "aring",
    /*          Glyf 111 -> Mac Glyph # 111*/ "ccedilla",
    /*          Glyf 112 -> Mac Glyph # 112*/ "eacute",
    /*          Glyf 113 -> Mac Glyph # 113*/ "egrave",
    /*          Glyf 114 -> Mac Glyph # 114*/ "ecircumflex",
    /*          Glyf 115 -> Mac Glyph # 115*/ "edieresis",
    /*          Glyf 116 -> Mac Glyph # 116*/ "iacute",
    /*          Glyf 117 -> Mac Glyph # 117*/ "igrave",
    /*          Glyf 118 -> Mac Glyph # 118*/ "icircumflex",
    /*          Glyf 119 -> Mac Glyph # 119*/ "idieresis",
    /*          Glyf 120 -> Mac Glyph # 120*/ "ntilde",
    /*          Glyf 121 -> Mac Glyph # 121*/ "oacute",
    /*          Glyf 122 -> Mac Glyph # 122*/ "ograve",
    /*          Glyf 123 -> Mac Glyph # 123*/ "ocircumflex",
    /*          Glyf 124 -> Mac Glyph # 124*/ "odieresis",
    /*          Glyf 125 -> Mac Glyph # 125*/ "otilde",
    /*          Glyf 126 -> Mac Glyph # 126*/ "uacute",
    /*          Glyf 127 -> Mac Glyph # 127*/ "ugrave",
    /*          Glyf 128 -> Mac Glyph # 128*/ "ucircumflex",
    /*          Glyf 129 -> Mac Glyph # 129*/ "udieresis",
    /*          Glyf 130 -> Mac Glyph # 130*/ "dagger",
    /*          Glyf 131 -> Mac Glyph # 131*/ "degree",
    /*          Glyf 132 -> Mac Glyph # 132*/ "cent",
    /*          Glyf 133 -> Mac Glyph # 133*/ "sterling",
    /*          Glyf 134 -> Mac Glyph # 134*/ "section",
    /*          Glyf 135 -> Mac Glyph # 135*/ "bullet",
    /*          Glyf 136 -> Mac Glyph # 136*/ "paragraph",
    /*          Glyf 137 -> Mac Glyph # 137*/ "germandbls",
    /*          Glyf 138 -> Mac Glyph # 138*/ "registered",
    /*          Glyf 139 -> Mac Glyph # 139*/ "copyright",
    /*          Glyf 140 -> Mac Glyph # 140*/ "trademark",
    /*          Glyf 141 -> Mac Glyph # 141*/ "acute",
    /*          Glyf 142 -> Mac Glyph # 142*/ "dieresis",
    /*          Glyf 143 -> Mac Glyph # 143*/ "notequal",
    /*          Glyf 144 -> Mac Glyph # 144*/ "AE",
    /*          Glyf 145 -> Mac Glyph # 145*/ "Oslash",
    /*          Glyf 146 -> Mac Glyph # 146*/ "infinity",
    /*          Glyf 147 -> Mac Glyph # 147*/ "plusminus",
    /*          Glyf 148 -> Mac Glyph # 148*/ "lessequal",
    /*          Glyf 149 -> Mac Glyph # 149*/ "greaterequal",
    /*          Glyf 150 -> Mac Glyph # 150*/ "yen",
    /*          Glyf 151 -> Mac Glyph # 151*/ "mu",
    /*          Glyf 152 -> Mac Glyph # 152*/ "partialdiff",
    /*          Glyf 153 -> Mac Glyph # 153*/ "summation",
    /*          Glyf 154 -> Mac Glyph # 154*/ "product",
    /*          Glyf 155 -> Mac Glyph # 155*/ "pi",
    /*          Glyf 156 -> Mac Glyph # 156*/ "integral",
    /*          Glyf 157 -> Mac Glyph # 157*/ "ordfeminine",
    /*          Glyf 158 -> Mac Glyph # 158*/ "ordmasculine",
    /*          Glyf 159 -> Mac Glyph # 159*/ "Omega",
    /*          Glyf 160 -> Mac Glyph # 160*/ "ae",
    /*          Glyf 161 -> Mac Glyph # 161*/ "oslash",
    /*          Glyf 162 -> Mac Glyph # 162*/ "questiondown",
    /*          Glyf 163 -> Mac Glyph # 163*/ "exclamdown",
    /*          Glyf 164 -> Mac Glyph # 164*/ "logicalnot",
    /*          Glyf 165 -> Mac Glyph # 165*/ "radical",
    /*          Glyf 166 -> Mac Glyph # 166*/ "florin",
    /*          Glyf 167 -> Mac Glyph # 167*/ "approxequal",
    /*          Glyf 168 -> Mac Glyph # 168*/ "increment",
    /*          Glyf 169 -> Mac Glyph # 169*/ "guillemotleft",
    /*          Glyf 170 -> Mac Glyph # 170*/ "guillemotright",
    /*          Glyf 171 -> Mac Glyph # 171*/ "ellipsis",
    /*          Glyf 172 -> Mac Glyph # 0*/ ".notdef",
    /*          Glyf 173 -> Mac Glyph # 173*/ "Agrave",
    /*          Glyf 174 -> Mac Glyph # 174*/ "Atilde",
    /*          Glyf 175 -> Mac Glyph # 175*/ "Otilde",
    /*          Glyf 176 -> Mac Glyph # 176*/ "OE",
    /*          Glyf 177 -> Mac Glyph # 177*/ "oe",
    /*          Glyf 178 -> Mac Glyph # 178*/ "endash",
    /*          Glyf 179 -> Mac Glyph # 179*/ "emdash",
    /*          Glyf 180 -> Mac Glyph # 180*/ "quotedblleft",
    /*          Glyf 181 -> Mac Glyph # 181*/ "quotedblright",
    /*          Glyf 182 -> Mac Glyph # 182*/ "quoteleft",
    /*          Glyf 183 -> Mac Glyph # 183*/ "quoteright",
    /*          Glyf 184 -> Mac Glyph # 184*/ "divide",
    /*          Glyf 185 -> Mac Glyph # 185*/ "lozenge",
    /*          Glyf 186 -> Mac Glyph # 186*/ "ydieresis",
    /*          Glyf 187 -> Mac Glyph # 187*/ "Ydieresis",
    /*          Glyf 188 -> Mac Glyph # 0*/ ".notdef",
    /*          Glyf 189 -> PSGlyf Name # 1*/ "Euro",
    /*          Glyf 190 -> Mac Glyph # 190*/ "guilsinglleft",
    /*          Glyf 191 -> Mac Glyph # 191*/ "guilsinglright",
    /*          Glyf 192 -> PSGlyf Name # 2*/ "fi",
    /*          Glyf 193 -> PSGlyf Name # 3*/ "fl",
    /*          Glyf 194 -> Mac Glyph # 194*/ "daggerdbl",
    /*          Glyf 195 -> Mac Glyph # 195*/ "periodcentered",
    /*          Glyf 196 -> Mac Glyph # 196*/ "quotesinglbase",
    /*          Glyf 197 -> Mac Glyph # 197*/ "quotedblbase",
    /*          Glyf 198 -> Mac Glyph # 198*/ "perthousand",
    /*          Glyf 199 -> Mac Glyph # 199*/ "Acircumflex",
    /*          Glyf 200 -> Mac Glyph # 200*/ "Ecircumflex",
    /*          Glyf 201 -> Mac Glyph # 201*/ "Aacute",
    /*          Glyf 202 -> Mac Glyph # 202*/ "Edieresis",
    /*          Glyf 203 -> Mac Glyph # 203*/ "Egrave",
    /*          Glyf 204 -> Mac Glyph # 204*/ "Iacute",
    /*          Glyf 205 -> Mac Glyph # 205*/ "Icircumflex",
    /*          Glyf 206 -> Mac Glyph # 206*/ "Idieresis",
    /*          Glyf 207 -> Mac Glyph # 207*/ "Igrave",
    /*          Glyf 208 -> Mac Glyph # 208*/ "Oacute",
    /*          Glyf 209 -> Mac Glyph # 209*/ "Ocircumflex",
    /*          Glyf 210 -> Mac Glyph # 0*/ ".notdef",
    /*          Glyf 211 -> Mac Glyph # 211*/ "Ograve",
    /*          Glyf 212 -> Mac Glyph # 212*/ "Uacute",
    /*          Glyf 213 -> Mac Glyph # 213*/ "Ucircumflex",
    /*          Glyf 214 -> Mac Glyph # 214*/ "Ugrave",
    /*          Glyf 215 -> Mac Glyph # 215*/ "dotlessi",
    /*          Glyf 216 -> Mac Glyph # 216*/ "circumflex",
    /*          Glyf 217 -> PSGlyf Name # 4*/ "tilde",
    /*          Glyf 218 -> PSGlyf Name # 5*/ "macron",
    /*          Glyf 219 -> Mac Glyph # 219*/ "breve",
    /*          Glyf 220 -> Mac Glyph # 220*/ "dotaccent",
    /*          Glyf 221 -> Mac Glyph # 221*/ "ring",
    /*          Glyf 222 -> Mac Glyph # 222*/ "cedilla",
    /*          Glyf 223 -> Mac Glyph # 223*/ "hungarumlaut",
    /*          Glyf 224 -> Mac Glyph # 224*/ "ogonek",
    /*          Glyf 225 -> Mac Glyph # 225*/ "caron",
    /*          Glyf 226 -> PSGlyf Name # 6*/ "whitecircle",
    /*          Glyf 227 -> Mac Glyph # 172*/ "nbspace",
    /*          Glyf 228 -> Mac Glyph # 232*/ "brokenbar",
    /*          Glyf 229 -> PSGlyf Name # 7*/ "sfthyphen",
    /*          Glyf 230 -> Mac Glyph # 218*/ "macron",
    /*          Glyf 231 -> Mac Glyph # 242*/ "twosuperior",
    /*          Glyf 232 -> Mac Glyph # 243*/ "threesuperior",
    /*          Glyf 233 -> PSGlyf Name # 8*/ "middot",
    /*          Glyf 234 -> Mac Glyph # 241*/ "onesuperior",
    /*          Glyf 235 -> Mac Glyph # 245*/ "onequarter",
    /*          Glyf 236 -> Mac Glyph # 244*/ "onehalf",
    /*          Glyf 237 -> Mac Glyph # 246*/ "threequarters",
    /*          Glyf 238 -> Mac Glyph # 233*/ "Eth",
    /*          Glyf 239 -> Mac Glyph # 240*/ "multiply",
    /*          Glyf 240 -> Mac Glyph # 235*/ "Yacute",
    /*          Glyf 241 -> Mac Glyph # 237*/ "Thorn",
    /*          Glyf 242 -> Mac Glyph # 234*/ "eth",
    /*          Glyf 243 -> Mac Glyph # 236*/ "yacute",
    /*          Glyf 244 -> Mac Glyph # 238*/ "thorn",
    /*          Glyf 245 -> PSGlyf Name # 9*/ "Amacron",
    /*          Glyf 246 -> PSGlyf Name # 10*/ "amacron",
    /*          Glyf 247 -> PSGlyf Name # 11*/ "Abreve",
    /*          Glyf 248 -> PSGlyf Name # 12*/ "abreve",
    /*          Glyf 249 -> PSGlyf Name # 13*/ "Aogonek",
    /*          Glyf 250 -> PSGlyf Name # 14*/ "aogonek",
    /*          Glyf 251 -> Mac Glyph # 253*/ "Cacute",
    /*          Glyf 252 -> Mac Glyph # 254*/ "cacute",
    /*          Glyf 253 -> Mac Glyph # 255*/ "Ccaron",
    /*          Glyf 254 -> Mac Glyph # 256*/ "ccaron",
    /*          Glyf 255 -> PSGlyf Name # 15*/ "Dcaron",
    /*          Glyf 256 -> PSGlyf Name # 16*/ "dcaron",
    /*          Glyf 257 -> PSGlyf Name # 17*/ "Dslash",
    /*          Glyf 258 -> PSGlyf Name # 18*/ "dmacron",
    /*          Glyf 259 -> PSGlyf Name # 19*/ "Emacron",
    /*          Glyf 260 -> PSGlyf Name # 20*/ "emacron",
    /*          Glyf 261 -> PSGlyf Name # 21*/ "Edot",
    /*          Glyf 262 -> PSGlyf Name # 22*/ "edot",
    /*          Glyf 263 -> PSGlyf Name # 23*/ "Eogonek",
    /*          Glyf 264 -> PSGlyf Name # 24*/ "eogonek",
    /*          Glyf 265 -> PSGlyf Name # 25*/ "Ecaron",
    /*          Glyf 266 -> PSGlyf Name # 26*/ "ecaron",
    /*          Glyf 267 -> Mac Glyph # 248*/ "Gbreve",
    /*          Glyf 268 -> Mac Glyph # 249*/ "gbreve",
    /*          Glyf 269 -> PSGlyf Name # 27*/ "Gcedilla",
    /*          Glyf 270 -> PSGlyf Name # 28*/ "gcedilla",
    /*          Glyf 271 -> PSGlyf Name # 29*/ "Itilde",
    /*          Glyf 272 -> PSGlyf Name # 30*/ "itilde",
    /*          Glyf 273 -> PSGlyf Name # 31*/ "Imacron",
    /*          Glyf 274 -> PSGlyf Name # 32*/ "imacron",
    /*          Glyf 275 -> PSGlyf Name # 33*/ "Iogonek",
    /*          Glyf 276 -> PSGlyf Name # 34*/ "iogonek",
    /*          Glyf 277 -> Mac Glyph # 250*/ "Idot",
    /*          Glyf 278 -> PSGlyf Name # 35*/ "IJ",
    /*          Glyf 279 -> PSGlyf Name # 36*/ "ij",
    /*          Glyf 280 -> PSGlyf Name # 37*/ "Kcedilla",
    /*          Glyf 281 -> PSGlyf Name # 38*/ "kcedilla",
    /*          Glyf 282 -> PSGlyf Name # 39*/ "kgreenlandic",
    /*          Glyf 283 -> PSGlyf Name # 40*/ "Lacute",
    /*          Glyf 284 -> PSGlyf Name # 41*/ "lacute",
    /*          Glyf 285 -> PSGlyf Name # 42*/ "Lcedilla",
    /*          Glyf 286 -> PSGlyf Name # 43*/ "lcedilla",
    /*          Glyf 287 -> PSGlyf Name # 44*/ "Lcaron",
    /*          Glyf 288 -> PSGlyf Name # 45*/ "lcaron",
    /*          Glyf 289 -> PSGlyf Name # 46*/ "Ldot",
    /*          Glyf 290 -> PSGlyf Name # 47*/ "ldot",
    /*          Glyf 291 -> Mac Glyph # 226*/ "Lslash",
    /*          Glyf 292 -> Mac Glyph # 227*/ "lslash",
    /*          Glyf 293 -> PSGlyf Name # 48*/ "Nacute",
    /*          Glyf 294 -> PSGlyf Name # 49*/ "nacute",
    /*          Glyf 295 -> PSGlyf Name # 50*/ "Ncedilla",
    /*          Glyf 296 -> PSGlyf Name # 51*/ "ncedilla",
    /*          Glyf 297 -> PSGlyf Name # 52*/ "Ncaron",
    /*          Glyf 298 -> PSGlyf Name # 53*/ "ncaron",
    /*          Glyf 299 -> PSGlyf Name # 54*/ "napostrophe",
    /*          Glyf 300 -> PSGlyf Name # 55*/ "Eng",
    /*          Glyf 301 -> PSGlyf Name # 56*/ "eng",
    /*          Glyf 302 -> PSGlyf Name # 57*/ "Omacron",
    /*          Glyf 303 -> PSGlyf Name # 58*/ "omacron",
    /*          Glyf 304 -> PSGlyf Name # 59*/ "Odblacute",
    /*          Glyf 305 -> PSGlyf Name # 60*/ "odblacute",
    /*          Glyf 306 -> PSGlyf Name # 61*/ "Racute",
    /*          Glyf 307 -> PSGlyf Name # 62*/ "racute",
    /*          Glyf 308 -> PSGlyf Name # 63*/ "Rcedilla",
    /*          Glyf 309 -> PSGlyf Name # 64*/ "rcedilla",
    /*          Glyf 310 -> PSGlyf Name # 65*/ "Rcaron",
    /*          Glyf 311 -> PSGlyf Name # 66*/ "rcaron",
    /*          Glyf 312 -> PSGlyf Name # 67*/ "Sacute",
    /*          Glyf 313 -> PSGlyf Name # 68*/ "sacute",
    /*          Glyf 314 -> Mac Glyph # 251*/ "Scedilla",
    /*          Glyf 315 -> Mac Glyph # 252*/ "scedilla",
    /*          Glyf 316 -> Mac Glyph # 228*/ "Scaron",
    /*          Glyf 317 -> Mac Glyph # 229*/ "scaron",
    /*          Glyf 318 -> PSGlyf Name # 69*/ "Tcedilla",
    /*          Glyf 319 -> PSGlyf Name # 70*/ "tcedilla",
    /*          Glyf 320 -> PSGlyf Name # 71*/ "Tcaron",
    /*          Glyf 321 -> PSGlyf Name # 72*/ "tcaron",
    /*          Glyf 322 -> PSGlyf Name # 73*/ "Tbar",
    /*          Glyf 323 -> PSGlyf Name # 74*/ "tbar",
    /*          Glyf 324 -> PSGlyf Name # 75*/ "Utilde",
    /*          Glyf 325 -> PSGlyf Name # 76*/ "utilde",
    /*          Glyf 326 -> PSGlyf Name # 77*/ "Umacron",
    /*          Glyf 327 -> PSGlyf Name # 78*/ "umacron",
    /*          Glyf 328 -> PSGlyf Name # 79*/ "Uring",
    /*          Glyf 329 -> PSGlyf Name # 80*/ "uring",
    /*          Glyf 330 -> PSGlyf Name # 81*/ "Udblacute",
    /*          Glyf 331 -> PSGlyf Name # 82*/ "udblacute",
    /*          Glyf 332 -> PSGlyf Name # 83*/ "Uogonek",
    /*          Glyf 333 -> PSGlyf Name # 84*/ "uogonek",
    /*          Glyf 334 -> PSGlyf Name # 85*/ "Zacute",
    /*          Glyf 335 -> PSGlyf Name # 86*/ "zacute",
    /*          Glyf 336 -> PSGlyf Name # 87*/ "Zdot",
    /*          Glyf 337 -> PSGlyf Name # 88*/ "zdot",
    /*          Glyf 338 -> Mac Glyph # 230*/ "Zcaron",
    /*          Glyf 339 -> Mac Glyph # 231*/ "zcaron",
    /*          Glyf 340 -> PSGlyf Name # 89*/ "Gamma",
    /*          Glyf 341 -> PSGlyf Name # 90*/ "Theta",
    /*          Glyf 342 -> PSGlyf Name # 91*/ "Lambda",
    /*          Glyf 343 -> PSGlyf Name # 92*/ "Xi",
    /*          Glyf 344 -> PSGlyf Name # 93*/ "Pi",
    /*          Glyf 345 -> PSGlyf Name # 94*/ "Sigma",
    /*          Glyf 346 -> PSGlyf Name # 95*/ "Upsilon",
    /*          Glyf 347 -> PSGlyf Name # 96*/ "Phi",
    /*          Glyf 348 -> PSGlyf Name # 97*/ "Psi",
    /*          Glyf 349 -> PSGlyf Name # 98*/ "Omega",
    /*          Glyf 350 -> PSGlyf Name # 99*/ "alpha",
    /*          Glyf 351 -> PSGlyf Name # 100*/ "beta",
    /*          Glyf 352 -> PSGlyf Name # 101*/ "gamma",
    /*          Glyf 353 -> PSGlyf Name # 102*/ "delta",
    /*          Glyf 354 -> PSGlyf Name # 103*/ "epsilon",
    /*          Glyf 355 -> PSGlyf Name # 104*/ "zeta",
    /*          Glyf 356 -> PSGlyf Name # 105*/ "eta",
    /*          Glyf 357 -> PSGlyf Name # 106*/ "theta",
    /*          Glyf 358 -> PSGlyf Name # 107*/ "iota",
    /*          Glyf 359 -> PSGlyf Name # 108*/ "kappa",
    /*          Glyf 360 -> PSGlyf Name # 109*/ "lambda",
    /*          Glyf 361 -> PSGlyf Name # 110*/ "mu",
    /*          Glyf 362 -> PSGlyf Name # 111*/ "nu",
    /*          Glyf 363 -> PSGlyf Name # 112*/ "xi",
    /*          Glyf 364 -> PSGlyf Name # 113*/ "omicron",
    /*          Glyf 365 -> PSGlyf Name # 114*/ "rho",
    /*          Glyf 366 -> PSGlyf Name # 115*/ "sigma1",
    /*          Glyf 367 -> PSGlyf Name # 116*/ "sigma",
    /*          Glyf 368 -> PSGlyf Name # 117*/ "tau",
    /*          Glyf 369 -> PSGlyf Name # 118*/ "upsilon",
    /*          Glyf 370 -> PSGlyf Name # 119*/ "phi",
    /*          Glyf 371 -> PSGlyf Name # 120*/ "chi",
    /*          Glyf 372 -> PSGlyf Name # 121*/ "psi",
    /*          Glyf 373 -> PSGlyf Name # 122*/ "omega",
    /*          Glyf 374 -> PSGlyf Name # 123*/ "theta1",
    /*          Glyf 375 -> PSGlyf Name # 124*/ "phi1",
    /*          Glyf 376 -> PSGlyf Name # 125*/ "pi2",
    /*          Glyf 377 -> PSGlyf Name # 126*/ "underscoredbl",
    /*          Glyf 378 -> PSGlyf Name # 127*/ "minute",
    /*          Glyf 379 -> PSGlyf Name # 128*/ "second",
    /*          Glyf 380 -> PSGlyf Name # 129*/ "exclamdbl",
    /*          Glyf 381 -> PSGlyf Name # 130*/ "zerosuperior",
    /*          Glyf 382 -> PSGlyf Name # 131*/ "foursuperior",
    /*          Glyf 383 -> PSGlyf Name # 132*/ "fivesuperior",
    /*          Glyf 384 -> PSGlyf Name # 133*/ "sixsuperior",
    /*          Glyf 385 -> PSGlyf Name # 134*/ "sevensuperior",
    /*          Glyf 386 -> PSGlyf Name # 135*/ "eightsuperior",
    /*          Glyf 387 -> PSGlyf Name # 136*/ "ninesuperior",
    /*          Glyf 388 -> PSGlyf Name # 137*/ "nsuperior",
    /*          Glyf 389 -> PSGlyf Name # 138*/ "peseta",
    /*          Glyf 390 -> PSGlyf Name # 139*/ "afii61248",
    /*          Glyf 391 -> Mac Glyph # 0*/ ".notdef",
    /*          Glyf 392 -> PSGlyf Name # 140*/ "Ifraktur",
    /*          Glyf 393 -> Mac Glyph # 0*/ ".notdef",
    /*          Glyf 394 -> PSGlyf Name # 141*/ "smallscriptl",
    /*          Glyf 395 -> PSGlyf Name # 142*/ "weierstrass",
    /*          Glyf 396 -> PSGlyf Name # 143*/ "Rfraktur",
    /*          Glyf 397 -> Mac Glyph # 0*/ ".notdef",
    /*          Glyf 398 -> PSGlyf Name # 144*/ "uni_2120",
    /*          Glyf 399 -> Mac Glyph # 0*/ ".notdef",
    /*          Glyf 400 -> Mac Glyph # 0*/ ".notdef",
    /*          Glyf 401 -> PSGlyf Name # 145*/ "uni_212F",
    /*          Glyf 402 -> PSGlyf Name # 146*/ "aleph",
    /*          Glyf 403 -> PSGlyf Name # 147*/ "uni_2136",
    /*          Glyf 404 -> PSGlyf Name # 148*/ "uni_2137",
    /*          Glyf 405 -> PSGlyf Name # 149*/ "arrowleft",
    /*          Glyf 406 -> PSGlyf Name # 150*/ "arrowup",
    /*          Glyf 407 -> PSGlyf Name # 151*/ "arrowright",
    /*          Glyf 408 -> PSGlyf Name # 152*/ "arrowdown",
    /*          Glyf 409 -> PSGlyf Name # 153*/ "arrowboth",
    /*          Glyf 410 -> PSGlyf Name # 154*/ "arrowupdn",
    /*          Glyf 411 -> Mac Glyph # 0*/ ".notdef",
    /*          Glyf 412 -> Mac Glyph # 0*/ ".notdef",
    /*          Glyf 413 -> Mac Glyph # 0*/ ".notdef",
    /*          Glyf 414 -> Mac Glyph # 0*/ ".notdef",
    /*          Glyf 415 -> PSGlyf Name # 155*/ "arrowupdnbse",
    /*          Glyf 416 -> PSGlyf Name # 156*/ "carriagereturn",
    /*          Glyf 417 -> Mac Glyph # 0*/ ".notdef",
    /*          Glyf 418 -> Mac Glyph # 0*/ ".notdef",
    /*          Glyf 419 -> PSGlyf Name # 157*/ "arrowdblleft",
    /*          Glyf 420 -> PSGlyf Name # 158*/ "arrowdblup",
    /*          Glyf 421 -> PSGlyf Name # 159*/ "arrowdblright",
    /*          Glyf 422 -> PSGlyf Name # 160*/ "arrowdbldown",
    /*          Glyf 423 -> PSGlyf Name # 161*/ "arrowdblboth",
    /*          Glyf 424 -> Mac Glyph # 0*/ ".notdef",
    /*          Glyf 425 -> PSGlyf Name # 162*/ "universal",
    /*          Glyf 426 -> PSGlyf Name # 163*/ "existential",
    /*          Glyf 427 -> PSGlyf Name # 164*/ "emptyset",
    /*          Glyf 428 -> PSGlyf Name # 165*/ "gradient",
    /*          Glyf 429 -> PSGlyf Name # 166*/ "element",
    /*          Glyf 430 -> PSGlyf Name # 167*/ "notelement",
    /*          Glyf 431 -> Mac Glyph # 0*/ ".notdef",
    /*          Glyf 432 -> PSGlyf Name # 168*/ "suchthat",
    /*          Glyf 433 -> Mac Glyph # 239*/ "minus",
    /*          Glyf 434 -> Mac Glyph # 0*/ ".notdef",
    /*          Glyf 435 -> Mac Glyph # 188*/ "fraction",
    /*          Glyf 436 -> PSGlyf Name # 169*/ "asteriskmath",
    /*          Glyf 437 -> PSGlyf Name # 170*/ "ring",
    /*          Glyf 438 -> PSGlyf Name # 171*/ "proportional",
    /*          Glyf 439 -> PSGlyf Name # 172*/ "orthogonal",
    /*          Glyf 440 -> PSGlyf Name # 173*/ "angle",
    /*          Glyf 441 -> PSGlyf Name # 174*/ "uni_2223",
    /*          Glyf 442 -> Mac Glyph # 0*/ ".notdef",
    /*          Glyf 443 -> PSGlyf Name # 175*/ "uni_2227",
    /*          Glyf 444 -> PSGlyf Name # 176*/ "uni_2228",
    /*          Glyf 445 -> PSGlyf Name # 177*/ "intersection",
    /*          Glyf 446 -> PSGlyf Name # 178*/ "union",
    /*          Glyf 447 -> Mac Glyph # 0*/ ".notdef",
    /*          Glyf 448 -> PSGlyf Name # 179*/ "therefore",
    /*          Glyf 449 -> Mac Glyph # 0*/ ".notdef",
    /*          Glyf 450 -> Mac Glyph # 0*/ ".notdef",
    /*          Glyf 451 -> PSGlyf Name # 180*/ "asymptoticequal",
    /*          Glyf 452 -> PSGlyf Name # 181*/ "congruent",
    /*          Glyf 453 -> PSGlyf Name # 182*/ "equivalence",
    /*          Glyf 454 -> Mac Glyph # 0*/ ".notdef",
    /*          Glyf 455 -> Mac Glyph # 0*/ ".notdef",
    /*          Glyf 456 -> Mac Glyph # 0*/ ".notdef",
    /*          Glyf 457 -> PSGlyf Name # 183*/ "propersubset",
    /*          Glyf 458 -> PSGlyf Name # 184*/ "propersuperset",
    /*          Glyf 459 -> PSGlyf Name # 185*/ "notsubset",
    /*          Glyf 460 -> Mac Glyph # 0*/ ".notdef",
    /*          Glyf 461 -> PSGlyf Name # 186*/ "reflexsubset",
    /*          Glyf 462 -> PSGlyf Name # 187*/ "reflexsuperset",
    /*          Glyf 463 -> Mac Glyph # 0*/ ".notdef",
    /*          Glyf 464 -> Mac Glyph # 0*/ ".notdef",
    /*          Glyf 465 -> PSGlyf Name # 188*/ "circlemultiply",
    /*          Glyf 466 -> Mac Glyph # 0*/ ".notdef",
    /*          Glyf 467 -> Mac Glyph # 0*/ ".notdef",
    /*          Glyf 468 -> Mac Glyph # 0*/ ".notdef",
    /*          Glyf 469 -> Mac Glyph # 0*/ ".notdef",
    /*          Glyf 470 -> Mac Glyph # 0*/ ".notdef",
    /*          Glyf 471 -> Mac Glyph # 0*/ ".notdef",
    /*          Glyf 472 -> PSGlyf Name # 189*/ "uni_22BB",
    /*          Glyf 473 -> PSGlyf Name # 190*/ "house",
    /*          Glyf 474 -> PSGlyf Name # 191*/ "revlogicalnot",
    /*          Glyf 475 -> PSGlyf Name # 192*/ "integraltp",
    /*          Glyf 476 -> PSGlyf Name # 193*/ "integralbt",
    /*          Glyf 477 -> PSGlyf Name # 194*/ "angleleft",
    /*          Glyf 478 -> PSGlyf Name # 195*/ "angleright",
    /*          Glyf 479 -> PSGlyf Name # 196*/ "2500",
    /*          Glyf 480 -> PSGlyf Name # 197*/ "2502",
    /*          Glyf 481 -> PSGlyf Name # 198*/ "250c",
    /*          Glyf 482 -> PSGlyf Name # 199*/ "2510",
    /*          Glyf 483 -> PSGlyf Name # 200*/ "2514",
    /*          Glyf 484 -> PSGlyf Name # 201*/ "2518",
    /*          Glyf 485 -> PSGlyf Name # 202*/ "251c",
    /*          Glyf 486 -> PSGlyf Name # 203*/ "2524",
    /*          Glyf 487 -> PSGlyf Name # 204*/ "252c",
    /*          Glyf 488 -> PSGlyf Name # 205*/ "2534",
    /*          Glyf 489 -> PSGlyf Name # 206*/ "253c",
    /*          Glyf 490 -> PSGlyf Name # 207*/ "2550",
    /*          Glyf 491 -> PSGlyf Name # 208*/ "2551",
    /*          Glyf 492 -> PSGlyf Name # 209*/ "2552",
    /*          Glyf 493 -> PSGlyf Name # 210*/ "2553",
    /*          Glyf 494 -> PSGlyf Name # 211*/ "2554",
    /*          Glyf 495 -> PSGlyf Name # 212*/ "2555",
    /*          Glyf 496 -> PSGlyf Name # 213*/ "2556",
    /*          Glyf 497 -> PSGlyf Name # 214*/ "2557",
    /*          Glyf 498 -> PSGlyf Name # 215*/ "2558",
    /*          Glyf 499 -> PSGlyf Name # 216*/ "2559",
    /*          Glyf 500 -> PSGlyf Name # 217*/ "255a",
    /*          Glyf 501 -> PSGlyf Name # 218*/ "255b",
    /*          Glyf 502 -> PSGlyf Name # 219*/ "255c",
    /*          Glyf 503 -> PSGlyf Name # 220*/ "255d",
    /*          Glyf 504 -> PSGlyf Name # 221*/ "255e",
    /*          Glyf 505 -> PSGlyf Name # 222*/ "255f",
    /*          Glyf 506 -> PSGlyf Name # 223*/ "2560",
    /*          Glyf 507 -> PSGlyf Name # 224*/ "2561",
    /*          Glyf 508 -> PSGlyf Name # 225*/ "2562",
    /*          Glyf 509 -> PSGlyf Name # 226*/ "2563",
    /*          Glyf 510 -> PSGlyf Name # 227*/ "2564",
    /*          Glyf 511 -> PSGlyf Name # 228*/ "2565",
    /*          Glyf 512 -> PSGlyf Name # 229*/ "2566",
    /*          Glyf 513 -> PSGlyf Name # 230*/ "2567",
    /*          Glyf 514 -> PSGlyf Name # 231*/ "2568",
    /*          Glyf 515 -> PSGlyf Name # 232*/ "2569",
    /*          Glyf 516 -> PSGlyf Name # 233*/ "256a",
    /*          Glyf 517 -> PSGlyf Name # 234*/ "256b",
    /*          Glyf 518 -> PSGlyf Name # 235*/ "256c",
    /*          Glyf 519 -> Mac Glyph # 0*/ ".notdef",
    /*          Glyf 520 -> Mac Glyph # 0*/ ".notdef",
    /*          Glyf 521 -> Mac Glyph # 0*/ ".notdef",
    /*          Glyf 522 -> Mac Glyph # 0*/ ".notdef",
    /*          Glyf 523 -> PSGlyf Name # 236*/ "upblock",
    /*          Glyf 524 -> PSGlyf Name # 237*/ "dnblock",
    /*          Glyf 525 -> PSGlyf Name # 238*/ "block",
    /*          Glyf 526 -> PSGlyf Name # 239*/ "lfblock",
    /*          Glyf 527 -> PSGlyf Name # 240*/ "rtblock",
    /*          Glyf 528 -> PSGlyf Name # 241*/ "ltshade",
    /*          Glyf 529 -> PSGlyf Name # 242*/ "shade",
    /*          Glyf 530 -> PSGlyf Name # 243*/ "dkshade",
    /*          Glyf 531 -> PSGlyf Name # 244*/ "filledbox",
    /*          Glyf 532 -> PSGlyf Name # 245*/ "whitesquare",
    /*          Glyf 533 -> PSGlyf Name # 246*/ "blacksmallsquare",
    /*          Glyf 534 -> PSGlyf Name # 247*/ "whitesmallsquare",
    /*          Glyf 535 -> PSGlyf Name # 248*/ "filledrect",
    /*          Glyf 536 -> PSGlyf Name # 249*/ "triagup",
    /*          Glyf 537 -> Mac Glyph # 0*/ ".notdef",
    /*          Glyf 538 -> Mac Glyph # 0*/ ".notdef",
    /*          Glyf 539 -> PSGlyf Name # 250*/ "triagrt",
    /*          Glyf 540 -> PSGlyf Name # 251*/ "triagdn",
    /*          Glyf 541 -> Mac Glyph # 0*/ ".notdef",
    /*          Glyf 542 -> Mac Glyph # 0*/ ".notdef",
    /*          Glyf 543 -> PSGlyf Name # 252*/ "triaglf",
    /*          Glyf 544 -> Mac Glyph # 0*/ ".notdef",
    /*          Glyf 545 -> PSGlyf Name # 253*/ "uni_25C7",
    /*          Glyf 546 -> PSGlyf Name # 254*/ "circle",
    /*          Glyf 547 -> PSGlyf Name # 255*/ "blackcircle",
    /*          Glyf 548 -> PSGlyf Name # 256*/ "invbullet",
    /*          Glyf 549 -> PSGlyf Name # 257*/ "invcircle",
    /*          Glyf 550 -> PSGlyf Name # 258*/ "openbullet",
    /*          Glyf 551 -> PSGlyf Name # 259*/ "smileface",
    /*          Glyf 552 -> PSGlyf Name # 260*/ "invsmileface",
    /*          Glyf 553 -> PSGlyf Name # 261*/ "sun",
    /*          Glyf 554 -> PSGlyf Name # 262*/ "female",
    /*          Glyf 555 -> PSGlyf Name # 263*/ "male",
    /*          Glyf 556 -> PSGlyf Name # 264*/ "spade",
    /*          Glyf 557 -> PSGlyf Name # 265*/ "club",
    /*          Glyf 558 -> PSGlyf Name # 266*/ "heart",
    /*          Glyf 559 -> PSGlyf Name # 267*/ "diamond",
    /*          Glyf 560 -> PSGlyf Name # 268*/ "musicalnote",
    /*          Glyf 561 -> PSGlyf Name # 269*/ "musicalnotedbl",
    /*          Glyf 562 -> PSGlyf Name # 270*/ "uni_301A",
    /*          Glyf 563 -> PSGlyf Name # 271*/ "uni_301B",
    /*          Glyf 564 -> PSGlyf Name # 272*/ "uni_EFBF",
    /*          Glyf 565 -> PSGlyf Name # 273*/ "uni_EFC0",
    /*          Glyf 566 -> PSGlyf Name # 274*/ "uni_EFC1",
    /*          Glyf 567 -> PSGlyf Name # 275*/ "uni_EFC2",
    /*          Glyf 568 -> PSGlyf Name # 276*/ "uni_EFC3",
    /*          Glyf 569 -> PSGlyf Name # 277*/ "uni_EFC4",
    /*          Glyf 570 -> PSGlyf Name # 278*/ "uni_EFC5",
    /*          Glyf 571 -> PSGlyf Name # 279*/ "uni_EFC6",
    /*          Glyf 572 -> PSGlyf Name # 280*/ "uni_EFC7",
    /*          Glyf 573 -> PSGlyf Name # 281*/ "uni_EFC8",
    /*          Glyf 574 -> PSGlyf Name # 282*/ "uni_EFC9",
    /*          Glyf 575 -> PSGlyf Name # 283*/ "uni_EFCA",
    /*          Glyf 576 -> PSGlyf Name # 284*/ "uni_EFCB",
    /*          Glyf 577 -> PSGlyf Name # 285*/ "uni_EFCC",
    /*          Glyf 578 -> PSGlyf Name # 286*/ "uni_EFCD",
    /*          Glyf 579 -> PSGlyf Name # 287*/ "uni_EFCE",
    /*          Glyf 580 -> PSGlyf Name # 288*/ "uni_EFCF",
    /*          Glyf 581 -> PSGlyf Name # 289*/ "uni_EFD0",
    /*          Glyf 582 -> PSGlyf Name # 290*/ "uni_EFD1",
    /*          Glyf 583 -> PSGlyf Name # 291*/ "uni_EFD2",
    /*          Glyf 584 -> PSGlyf Name # 292*/ "uni_EFD3",
    /*          Glyf 585 -> PSGlyf Name # 293*/ "uni_EFD4",
    /*          Glyf 586 -> PSGlyf Name # 294*/ "uni_EFD5",
    /*          Glyf 587 -> PSGlyf Name # 295*/ "uni_EFD6",
    /*          Glyf 588 -> PSGlyf Name # 296*/ "uni_EFD7",
    /*          Glyf 589 -> PSGlyf Name # 297*/ "uni_EFD8",
    /*          Glyf 590 -> PSGlyf Name # 298*/ "uni_EFD9",
    /*          Glyf 591 -> PSGlyf Name # 299*/ "uni_EFDA",
    /*          Glyf 592 -> PSGlyf Name # 300*/ "uni_EFDB",
    /*          Glyf 593 -> PSGlyf Name # 301*/ "uni_EFDC",
    /*          Glyf 594 -> PSGlyf Name # 302*/ "uni_EFDD",
    /*          Glyf 595 -> PSGlyf Name # 303*/ "uni_EFDE",
    /*          Glyf 596 -> PSGlyf Name # 304*/ "uni_EFDF",
    /*          Glyf 597 -> PSGlyf Name # 305*/ "uni_EFE0",
    /*          Glyf 598 -> PSGlyf Name # 306*/ "uni_EFE1",
    /*          Glyf 599 -> PSGlyf Name # 307*/ "uni_EFE2",
    /*          Glyf 600 -> PSGlyf Name # 308*/ "uni_EFE3",
    /*          Glyf 601 -> PSGlyf Name # 309*/ "uni_EFE4",
    /*          Glyf 602 -> PSGlyf Name # 310*/ "uni_EFE5",
    /*          Glyf 603 -> PSGlyf Name # 311*/ "uni_EFE6",
    /*          Glyf 604 -> PSGlyf Name # 312*/ "uni_EFE7",
    /*          Glyf 605 -> PSGlyf Name # 313*/ "uni_EFE8",
    /*          Glyf 606 -> PSGlyf Name # 314*/ "uni_EFE9",
    /*          Glyf 607 -> PSGlyf Name # 315*/ "uni_EFEA",
    /*          Glyf 608 -> PSGlyf Name # 316*/ "uni_EFEB",
    /*          Glyf 609 -> PSGlyf Name # 317*/ "uni_EFEC",
    /*          Glyf 610 -> PSGlyf Name # 318*/ "hp1089",
    /*          Glyf 611 -> PSGlyf Name # 319*/ "hp1087",
    /*          Glyf 612 -> PSGlyf Name # 320*/ "hp1085",
    /*          Glyf 613 -> PSGlyf Name # 321*/ "hp1045",
    /*          Glyf 614 -> PSGlyf Name # 322*/ "hp1030",
    /*          Glyf 615 -> PSGlyf Name # 323*/ "hp0330",
    /*          Glyf 616 -> PSGlyf Name # 324*/ "hp0323",
    /*          Glyf 617 -> PSGlyf Name # 325*/ "hp0322",
    /*          Glyf 618 -> PSGlyf Name # 326*/ "hp0321",
    /*          Glyf 619 -> PSGlyf Name # 327*/ "hp0320",
    /*          Glyf 620 -> PSGlyf Name # 328*/ "hp0319",
    /*          Glyf 621 -> PSGlyf Name # 329*/ "hp0318",
    /*          Glyf 622 -> PSGlyf Name # 330*/ "hp0317",
    /*          Glyf 623 -> PSGlyf Name # 331*/ "uni_EFFA",
    /*          Glyf 624 -> PSGlyf Name # 332*/ "uni_EFFB",
    /*          Glyf 625 -> PSGlyf Name # 333*/ "uni_EFFC",
    /*          Glyf 626 -> PSGlyf Name # 334*/ "uni_EFFD",
    /*          Glyf 627 -> PSGlyf Name # 335*/ "uni_EFFE",
    /*          Glyf 628 -> PSGlyf Name # 336*/ "uni_EFFF",
    /*          Glyf 629 -> PSGlyf Name # 337*/ "ff",
    /*          Glyf 630 -> PSGlyf Name # 338*/ "ffi",
    /*          Glyf 631 -> PSGlyf Name # 339*/ "ffl",
    /*          Glyf 632 -> Mac Glyph # 0*/ ".notdef",
};

// NB global container
gs_const_string pl_mystring;

private const char *
pl_glyph_name(gs_font_type42 *pfont, gs_glyph glyph, gs_const_string *pstr)
{	
    // should check for overflow on bounds of name array.

    if ( glyph >= sizeof(glyph_names) / sizeof(glyph_names[0]))
        glyph = 0;
    pl_mystring.data = glyph_names[glyph];
    pl_mystring.size = strlen(pl_mystring.data);
    pstr = &pl_mystring;
    // NB assume input is zero
    return 0;
}

extern gs_char last_char;
/* Get the unicode valude for a glyph */
private gs_char
pl_decode_glyph(gs_font *font,  gs_glyph glyph)
{	
    // NB glyph is unicode for builtin fonts 
    // this fails for downloaded fonts where there is often 
    // no association other than sequential numbering 
    return last_char;
}

/* Get a glyph from a known encoding.  We don't support this either. */
private gs_glyph
pl_known_encode(gs_char chr, int encoding_index)
{	return gs_no_glyph;
}

/* ---------------- Public procedures ---------------- */

/* character width */
int pl_font_char_width(const pl_font_t *plfont, const void *pgs, uint char_code, gs_point *pwidth)
{
    return (*(plfont)->char_width)(plfont, pgs, char_code, pwidth);
}

/* character width */
int pl_font_char_metrics(const pl_font_t *plfont, const void *pgs, uint char_code, float metrics[4])
{
    return (*(plfont)->char_metrics)(plfont, pgs, char_code, metrics);
}


/* Allocate a font. */
pl_font_t *
pl_alloc_font(gs_memory_t *mem, client_name_t cname)
{	pl_font_t *plfont =
	  gs_alloc_struct(mem, pl_font_t, &st_pl_font, cname);

	if ( plfont )
	  { /* Initialize pointers. */
	    plfont->pfont = 0;
	    plfont->header = 0;
	    plfont->glyphs.table = 0;
	    plfont->char_glyphs.table = 0;
	    /* Initialize other defaults. */
	    plfont->landscape = false;
	    plfont->bold_fraction = 0;
	    plfont->font_file = 0;
	    plfont->resolution.x = plfont->resolution.y = 0;
	    plfont->params.proportional_spacing = true;
	    memset(plfont->character_complement, 0xff, 8);
	    plfont->offsets.GC = plfont->offsets.GT = plfont->offsets.VT = -1;
	  }
	return plfont;
}
/* import from plchar.c - used to determine if this is a downloaded
   true type font or a resident font - yuck. */
extern int pl_tt_get_outline(gs_font_type42 *pfont, uint index, gs_glyph_data_t *pdata);

/* Structure descriptors for cloning fonts */
gs_private_st_ptrs1(st_pl_font_glyph_f, pl_font_glyph_t, "pl_font_glyph_t",
  pl_font_glyph_enum_ptrs_f, pl_font_glyph_reloc_ptrs_f, data);
gs_private_st_element(st_pl_font_glyph_element_f, pl_font_glyph_t,
  "pl_font_glyph_t[]",
  pl_font_glyph_elt_enum_ptrs_f, pl_font_glyph_elt_reloc_ptrs_f, st_pl_font_glyph_f);

pl_font_t *
pl_clone_font(const pl_font_t *src, gs_memory_t *mem, client_name_t cname)
{
	pl_font_t *plfont = 
	  gs_alloc_struct(mem, pl_font_t, &st_pl_font, cname);
	if ( plfont == 0 )
	  return 0;
	/* copy technology common parts */
	plfont->storage = src->storage;
	plfont->header_size = src->header_size;
	plfont->scaling_technology = src->scaling_technology;
	plfont->font_type = src->font_type;
	plfont->char_width = src->char_width;
	plfont->char_metrics = src->char_metrics;
	plfont->large_sizes = src->large_sizes;
	plfont->resolution = src->resolution;
	plfont->params = src->params;
        plfont->pts_per_inch = src->pts_per_inch;
	plfont->font_file_loaded = src->font_file_loaded;
	plfont->landscape = src->landscape;
	plfont->bold_fraction = src->bold_fraction;
	{
	  int i;
	  for (i = 0; i < sizeof(src->character_complement); i++ )
	    plfont->character_complement[i] = src->character_complement[i];
	}
	plfont->offsets = src->offsets;
	plfont->header = gs_alloc_bytes(mem, src->header_size, cname);
	if ( plfont->header == 0 )
	  return 0;
	memcpy(plfont->header, src->header, src->header_size);
	
	if ( src->font_file ) {
	    plfont->font_file = gs_alloc_bytes(mem, strlen(src->font_file) + 1,
					       "pl_clone_font");
	    if ( plfont->font_file == 0 )
		return 0;  /* #NB errors!!! */
	    strcpy(plfont->font_file, src->font_file);
	}
	else
	    plfont->font_file = 0;
	/* technology specific setup */
	switch ( plfont->scaling_technology )
	  {
	  case plfst_bitmap:
	    {
	      gs_font_base *pfont =
		gs_alloc_struct(mem, gs_font_base, &st_gs_font_base, cname);
	      if ( pfont == 0 )
		return 0;
	      pl_fill_in_font((gs_font *)pfont, plfont, src->pfont->dir, mem, "nameless_font");
	      pl_fill_in_bitmap_font(pfont, gs_next_ids(mem, 1));
	      break;
	    }
	  case plfst_Intellifont:
	    {
	      gs_font_base *pfont =
		gs_alloc_struct(mem, gs_font_base, &st_gs_font_base, cname);
	      if ( pfont == 0 )
		return 0;
	      pl_fill_in_font((gs_font *)pfont, plfont, src->pfont->dir, mem, "nameless_font");
	      pl_fill_in_intelli_font(pfont, gs_next_ids(mem, 1));
	      break;
	    }
	  case plfst_TrueType:
	    {
	      {
		gs_font_type42 *pfont =
		  gs_alloc_struct(mem, gs_font_type42, &st_gs_font_type42, cname);
		/* detect if a truetype font is downloaded or
                   internal.  There must be a better way... */
		gs_font_type42 *pfont_src = (gs_font_type42 *)src->pfont;
		bool downloaded = (pfont_src->data.get_outline == pl_tt_get_outline);
		if ( pfont == 0 )
		  return 0;
		pl_fill_in_font((gs_font *)pfont, plfont, src->pfont->dir, mem, "nameless_font");
		pl_fill_in_tt_font(pfont, downloaded ? NULL : src->header, gs_next_ids(mem, 1));
	      }
	      break;
	    }
	  }
	if ( src->char_glyphs.table != 0 )
	  {
	    /* HAS may gs_alloc_struct_array() here but this is
	       consistant with pl_tt_alloc_char_glyphs() */
	    pl_tt_char_glyph_t *char_glyphs =
	      (pl_tt_char_glyph_t *) gs_alloc_byte_array(mem,
							 src->char_glyphs.size,
							 sizeof(pl_tt_char_glyph_t), cname);
	    int i;
	    if ( char_glyphs == 0 )
	      return 0;
	    for ( i = 0; i < src->char_glyphs.size; i++ )
	      char_glyphs[i] = src->char_glyphs.table[i];
	    /* once again a copy struct shortcut and then are restore
               of the char_glyphs.table pointer */
	    plfont->char_glyphs = src->char_glyphs;
	    plfont->char_glyphs.table = char_glyphs;
	  }
	else /* no character glyph table data */
	  plfont->char_glyphs = src->char_glyphs;

	if ( src->glyphs.table != 0 )
	  {
	    int i;
	    plfont->glyphs.table = 
	      gs_alloc_struct_array(mem, src->glyphs.size, pl_font_glyph_t,
				    &st_pl_font_glyph_element_f, cname);
	    plfont->glyphs.used = src->glyphs.used;
	    plfont->glyphs.limit = src->glyphs.limit;
	    plfont->glyphs.size = src->glyphs.size;
	    plfont->glyphs.skip = src->glyphs.skip;
	    for ( i = 0; i < src->glyphs.size; i++ )
	      {
		byte *data = src->glyphs.table[i].data;
		byte *char_data;
		plfont->glyphs.table[i].glyph =
		  src->glyphs.table[i].glyph;
		plfont->glyphs.table[i].data = 0;
		if ( data )
		  { /* ARGH --- */
		    uint size;
		    if ( plfont->scaling_technology == plfst_bitmap )
		      {
			size = 16 +
			  ((pl_get_uint16(data + 10) + 7) >> 3) *
			  pl_get_uint16(data + 12);
		      }
		    else if ( plfont->scaling_technology == plfst_Intellifont ) {
			/* non compound characters */
			if ( data[3] == 3 )
			    size = 6 + pl_get_uint16(data + 4);
			else /* assume data[3] == 4 (compound) */
			    size = 8 + data[6] * 6 + 2;
		    }
		    else /* truetype */
		      size = 2 + 2 + data[2] +
			pl_get_uint16(data + 2 + data[2]);
		    char_data = gs_alloc_bytes(mem, size, cname);
		    if ( char_data == 0 )
		      return 0;
		    memcpy(char_data, data, size);
		    plfont->glyphs.table[i].data = char_data;
		  }

	      }
	  }
	else /* no glyph table */
	  plfont->glyphs = src->glyphs;
	return plfont;
}
	
/* Fill in generic font boilerplate. */
int
pl_fill_in_font(gs_font *pfont, pl_font_t *plfont, gs_font_dir *pdir, gs_memory_t *mem, const char *font_name)
{	
        int i;
	plfont->pfont = pfont;
	/* Initialize generic font data. */
	pfont->next = pfont->prev = 0;
	pfont->memory = mem;
	pfont->dir = pdir;
	pfont->is_resource = false;  
	gs_notify_init(&pfont->notify_list, gs_memory_stable(mem));
	pfont->base = pfont;
	pfont->client_data = plfont;
	pfont->WMode = 0;
	pfont->PaintType = 0;
	pfont->StrokeWidth = 0;
	pfont->procs.init_fstack = gs_default_init_fstack;
	pfont->procs.next_char_glyph = gs_default_next_char_glyph;

	pfont->procs.glyph_name = pl_glyph_name;
	pfont->procs.decode_glyph = pl_decode_glyph;
	// NB
	// pfont->procs.callbacks.known_encode = pl_known_encode;

	pfont->procs.define_font = gs_no_define_font;
	pfont->procs.make_font = gs_no_make_font;
	pfont->procs.font_info = gs_default_font_info;
	pfont->id = gs_next_ids(mem, 1);
	strncpy(pfont->font_name.chars, font_name, sizeof(pfont->font_name.chars));
	pfont->font_name.size = strlen(font_name);
	/* replace spaces with '-', seems acrobat doesn't like spaces. */
	for (i = 0; i < pfont->font_name.size; i++) {
	    if (pfont->font_name.chars[i] == ' ')
		pfont->font_name.chars[i] = '-';
	}
	strncpy(pfont->key_name.chars, font_name, sizeof(pfont->font_name.chars));
	pfont->key_name.size = strlen(font_name);
	return 0;
}

/* Fill in bitmap font boilerplate. */
void
pl_fill_in_bitmap_font(gs_font_base *pfont, long unique_id)
{	/* It appears that bitmap fonts don't need a FontMatrix. */
	gs_make_identity(&pfont->FontMatrix);
	pfont->FontType = ft_user_defined;
	pfont->BitmapWidths = true;
	pfont->ExactSize = fbit_use_bitmaps;
	pfont->InBetweenSize = fbit_use_bitmaps;
	pfont->TransformedChar = fbit_transform_bitmaps;
	pl_bitmap_init_procs(pfont);
	/* We have no idea what the FontBBox should be. */
	pfont->FontBBox.p.x = pfont->FontBBox.p.y =
	  pfont->FontBBox.q.x = pfont->FontBBox.q.y = 0;
	uid_set_UniqueID(&pfont->UID, unique_id);
	pfont->encoding_index = 1;	/****** WRONG ******/
	pfont->nearest_encoding_index = 1;	/****** WRONG ******/
}

/* Fill in TrueType font boilerplate. */
void
pl_fill_in_tt_font(gs_font_type42 *pfont, void *data, long unique_id)
{	gs_make_identity(&pfont->FontMatrix);
	pfont->FontType = ft_TrueType;
	pfont->BitmapWidths = true;
	pfont->ExactSize = fbit_use_outlines;
	pfont->InBetweenSize = fbit_use_outlines;
	pfont->TransformedChar = fbit_use_outlines;
	/* Initialize base font data. */
	/*
	 * We can't set the FontBBox correctly until we've initialized the
	 * Type 42 specific data, but we need to set it to an empty box now
	 * for the sake of gs_type42_font_init.
	 */
	pfont->FontBBox.p.x = pfont->FontBBox.p.y =
	  pfont->FontBBox.q.x = pfont->FontBBox.q.y = 0;
	uid_set_UniqueID(&pfont->UID, unique_id);
	pfont->encoding_index = 1;	/****** WRONG ******/
	pfont->nearest_encoding_index = 1;	/****** WRONG ******/
	/* Initialize Type 42 specific data. */
	pfont->data.proc_data = data;
	pl_tt_init_procs(pfont);
	gs_type42_font_init(pfont);
	pl_tt_finish_init(pfont, !data);
}

/* Fill in Intellifont boilerplate. */
void
pl_fill_in_intelli_font(gs_font_base *pfont, long unique_id)
{	/* Intellifonts have an 8782-unit design space. */
 	{ gs_matrix mat;
	  gs_make_scaling(1.0/8782, 1.0/8782, &mat);
	  gs_matrix_translate(pfont->memory, &mat, -2980.0, -5380.0, &pfont->FontMatrix);
	}
	pfont->FontType = ft_user_defined;
	pfont->BitmapWidths = true;
	pfont->ExactSize = fbit_use_outlines;
	pfont->InBetweenSize = fbit_use_outlines;
	pfont->TransformedChar = fbit_use_outlines;
	/* We have no idea what the FontBBox should be. */
	pfont->FontBBox.p.x = pfont->FontBBox.p.y =
	  pfont->FontBBox.q.x = pfont->FontBBox.q.y = 0;
	uid_set_UniqueID(&pfont->UID, unique_id);
	pfont->encoding_index = 1;	/****** WRONG ******/
	pfont->nearest_encoding_index = 1;	/****** WRONG ******/
	pl_intelli_init_procs(pfont);
}

/*
 * Set large_sizes, scaling_technology, character_complement, offsets
 * (for TrueType fonts), and resolution (for bitmap fonts) by scanning
 * the segments of a segmented downloaded font.
 * This is used for PCL5 Format 15 and 16 fonts and for PCL XL fonts.
 * fst_offset is the offset of the Font Scaling Technology and Variety bytes;
 * the segment data runs from start_offset up to end_offset.
 * large_sizes = false indicates 2-byte segment sizes, true indicates 4-byte.
 */
int
pl_font_scan_segments(const gs_memory_t *mem,
		      pl_font_t *plfont, int fst_offset, int start_offset,
  long end_offset, bool large_sizes, const pl_font_offset_errors_t *pfoe)
{	const byte *header = plfont->header;
	pl_font_scaling_technology_t fst = header[fst_offset];
	int wsize = (large_sizes ? 4 : 2);
	const byte *segment = header + start_offset;
	const byte *end = header + end_offset;
	const byte *null_segment = end - (2 + wsize);
	bool found = false;
	ulong seg_size;
	int illegal_font_data = pfoe->illegal_font_data;

#define return_scan_error(err)\
  return_error(mem, (err) ? (err) : illegal_font_data);

	if ( memcmp(null_segment, "\377\377", 2) /* NULL segment header */ )
	  return_scan_error(pfoe->missing_required_segment);
	if ( memcmp(null_segment + 2, "\0\0\0\0", wsize) /* NULL segment size */ )
	  return_scan_error(pfoe->illegal_null_segment_size);
	switch ( fst )
	  {
	  case plfst_bitmap:
	  case plfst_TrueType:
	    break;
	  default:
	    return_scan_error(pfoe->illegal_font_header_fields);
	  }
	if ( header[fst_offset + 1] ) /* variety, must be 0 */
	  return_scan_error(pfoe->illegal_font_header_fields);
	/* Scan the segments. */
	for ( ; end - segment >= 2 + wsize; segment += 2 + wsize + seg_size )
	    { uint seg_id = u16(segment);
	      const byte *sdata = segment + 2 + wsize;
#define id2(c1,c2) (((uint)(c1) << 8) + (c2))

	      seg_size = (large_sizes ? u32(segment + 2) : u16(segment + 2));
	      if ( seg_size + 2 + wsize > end - segment )
		return_error(mem, illegal_font_data);
	      /* Handle segments common to all fonts. */
	      switch ( seg_id )
		{
		case 0xffff:		/* NULL segment ID */
		  if ( segment != null_segment )
		    return_error(mem, illegal_font_data);
		  continue;
		case id2('V','I'):
		  continue;
		case id2('C', 'C'):
		  if ( seg_size != 8 )
		    return_error(mem, illegal_font_data);
		  memcpy(plfont->character_complement, sdata, 8);
		  continue;
		default:
		  ;
		}
	      /* Handle segments specific to the scaling technology. */
	      if ( fst == plfst_bitmap )
		switch ( seg_id )
		  {
		  case id2('B','R'):
		    if ( seg_size != 4 )
		      return_scan_error(pfoe->illegal_BR_segment);
		    { uint xres = pl_get_uint16(sdata);
		      uint yres = pl_get_uint16(sdata + 2);
		      if ( xres == 0 || yres == 0 )
			return_scan_error(pfoe->illegal_BR_segment);
		      plfont->resolution.x = xres;
		      plfont->resolution.y = yres;
		    }
		    found = true;
		    break;
		  default:
		    if ( pfoe->illegal_font_segment < 0 )
		      return_error(mem, pfoe->illegal_font_segment);
		  }
	      else		/* fst == plfst_TrueType */
		switch ( seg_id )
		  {
		  case id2('G','T'):
		    /*
		     * We don't do much checking here, but we do check that
		     * the segment starts with a table directory that
		     * includes at least 3 elements (gdir, head,
		     * maxp -- but we don't check the actual names).
		     */
		    if ( seg_size < 12 + 5 * 16 ||
			 /* memcmp(sdata, "\000\001\000\000", 4) || */
			 u16(sdata + 4) < 3
		       )
		      return_scan_error(pfoe->illegal_GT_segment);
		    plfont->offsets.GT = segment - header;
		    found = true;
		    break;
		  case id2('G','C'):
		    if ( seg_size < 6 || u16(sdata) != 0 ||
			 seg_size != u16(sdata + 4) * 6 + 6
		       )
		      return_scan_error(pfoe->illegal_GC_segment);
		    plfont->offsets.GC = segment - header;
		    break;
		  case id2('V','T'):
		    /* Check for end of table mark */
		    if ( (seg_size & 3) != 0 || seg_size < 4 ||
			 u16(sdata + seg_size - 4) != 0xffff
		       )
		      return_scan_error(pfoe->illegal_VT_segment);
		    /* Check for table sorted by horizontal glyph ID */
		    { uint i;
		      for ( i = 0; i < seg_size - 4; i += 4 )
			if ( u16(sdata + i) >= u16(sdata + i + 4) )
			  return_scan_error(pfoe->illegal_VT_segment);
		    }
		    plfont->offsets.VT = segment - header;
		    break;
		  default:
		    if ( pfoe->illegal_font_segment < 0 )
		      return_error(mem, pfoe->illegal_font_segment);
		  }
#undef id2
	    }
	  if ( !found )
	    return_scan_error(pfoe->missing_required_segment);
	  if ( segment != end )
	    return_error(mem, illegal_font_data);
	  plfont->large_sizes = large_sizes;
	  plfont->scaling_technology = fst;
	  return 0;
#undef return_scan_error
}

int
pl_free_tt_fontfile_buffer(gs_memory_t *mem, byte *ptt_font_data)
{
    gs_free_object(mem, ptt_font_data, "pl_tt_load_font data");
    return 0;
}

int 
pl_alloc_tt_fontfile_buffer(FILE *in, gs_memory_t *mem, byte **pptt_font_data, ulong *size)
{
    ulong len = (fseek(in, 0L, SEEK_END), ftell(in));
    *size = 6 + len;	/* leave room for segment header */
    if ( *size != (uint)(*size) ) { 
	/*
	 * The font is too big to load in a single piece -- punt.
	 * The error message is bogus, but there isn't any more
	 * appropriate one.
	 */
	fclose(in);
	return_error(mem, gs_error_VMerror);
    }
    rewind(in);
    *pptt_font_data = gs_alloc_bytes(mem, *size, "pl_tt_load_font data");
    if ( *pptt_font_data == 0 ) {
	fclose(in);
	return_error(mem, gs_error_VMerror);
    }
    fread(*pptt_font_data + 6, 1, len, in);
    fclose(in);
    return 0;
}

/* Load a built-in (TrueType) font from external storage. */
int
pl_load_tt_font(FILE *in, gs_font_dir *pdir, gs_memory_t *mem,
  long unique_id, pl_font_t **pplfont, char *font_name)
{	
    byte *tt_font_datap;
    ulong size;
    int code;
    gs_font_type42 *pfont;
    pl_font_t *plfont;
    /* get the data from the file */
    code = pl_alloc_tt_fontfile_buffer(in, mem, &tt_font_datap, &size);
    if ( code < 0 )
	return_error(mem, gs_error_VMerror);
    /* Make a Type 42 font out of the TrueType data. */
    pfont = gs_alloc_struct(mem, gs_font_type42, &st_gs_font_type42,
			    "pl_tt_load_font(gs_font_type42)");
    plfont = pl_alloc_font(mem, "pl_tt_load_font(pl_font_t)");

    if ( pfont == 0 || plfont == 0 )
	code = gs_note_error(mem, gs_error_VMerror);
    else { /* Initialize general font boilerplate. */
	code = pl_fill_in_font((gs_font *)pfont, plfont, pdir, mem, font_name);
	if ( code >= 0 ) { /* Initialize TrueType font boilerplate. */
	    plfont->header = tt_font_datap;
	    plfont->header_size = size;
	    plfont->scaling_technology = plfst_TrueType;
	    plfont->font_type = plft_Unicode;
	    plfont->large_sizes = true;
	    plfont->offsets.GT = 0;
	    pl_fill_in_tt_font(pfont, tt_font_datap, unique_id);
	    code = gs_definefont(pdir, (gs_font *)pfont);
	    if ( code >= 0 ) { 
		/*
		 * Set the nominal design width to the
		 * width of a small 'x' character.  If
		 * there isn't one, set the design
		 * width arbitrarily at 0.6 em.  */
		gs_char space = ' ';
		float sbw[4];
		uint glyph_index =
		    (*pfont->procs.encode_char)
		    ((gs_font *)pfont, space, gs_no_glyph);

		if (glyph_index == gs_no_glyph || glyph_index == 0xffff)
		    glyph_index = 0;
		if ( gs_type42_get_metrics(pfont, glyph_index, sbw) < 0 )
		    sbw[2] = 0.6;
		pl_fp_set_pitch_cp(&plfont->params, sbw[2] * 100);
	    }
	}
    }
    if ( code < 0 ) { 
	gs_free_object(mem, plfont, "pl_tt_load_font(pl_font_t)");
	gs_free_object(mem, pfont, "pl_tt_load_font(gs_font_type42)");
	pl_free_tt_fontfile_buffer(mem, tt_font_datap);
	return code;
    }
    *pplfont = plfont;
    return 0;
}

/* load resident font data to ram */
int
pl_load_resident_font_data_from_file(gs_memory_t *mem, pl_font_t *plfont)
{

    ulong len, size;
    byte *data;
    if (plfont->font_file && !plfont->font_file_loaded) {
	FILE *in = fopen(plfont->font_file, gp_fmode_rb);
	if ( in == NULL )
	    return -1;
	/* note this is exactly the same as the code in pl_load_tt_font */
	len = (fseek(in, 0L, SEEK_END), ftell(in));
	size = 6 + len;	/* leave room for segment header */

	if ( size != (uint)size ) { 
	    /*
	     * The font is too big to load in a single piece -- punt.
	     * The error message is bogus, but there isn't any more
	     * appropriate one.
	     */
	    fclose(in);
	    return_error(mem, gs_error_VMerror);
	}
	rewind(in);
	data = gs_alloc_bytes(mem, size, "pl_tt_load_font data");
	if ( data == 0 ) { 
	    fclose(in);
	    return_error(mem, gs_error_VMerror);
	}
	fread(data + 6, 1, len, in);
	fclose(in);
	plfont->header = data;
	plfont->header_size = size;
	plfont->font_file_loaded = true;
    }
    return 0;
}

/* Keep resident font data in (header) and deallocate the memory */
int
pl_store_resident_font_data_in_file(char *font_file, gs_memory_t *mem, pl_font_t *plfont)
{
    /* Free the header data */
    if ( plfont->header ) {
	gs_free_object(mem, plfont->header, "pl_store_resident_font_data_in_file");
	plfont->header = 0;
	plfont->header_size = 0;
    } else {
	/* nothing to do */
	return 0;
    }
    /* we don't yet have a filename for this font object. create one
       and store it in the font. */
    if ( !plfont->font_file ) {
	plfont->font_file = gs_alloc_bytes(mem, strlen(font_file) + 1, "pl_store_resident_font_data_in_file");
	if ( plfont->font_file == 0 )
	    return -1;
	strcpy(plfont->font_file, font_file);
    }
    /* designate that the font data is not in RAM */
    plfont->font_file_loaded = false;
    return 0;
}
