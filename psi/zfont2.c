/* Copyright (C) 2001-2023 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  39 Mesa Street, Suite 108A, San Francisco,
   CA 94129, USA, for further information.
*/


/* Type 2 font creation operators */
#include "ghost.h"
#include "string_.h"  /* for CFF parser */
#include "oper.h"
#include "gxfixed.h"
#include "gsmatrix.h"
#include "gxfont.h"
#include "gxfont1.h"
#include "bfont.h"
#include "idict.h"
#include "idparam.h"
#include "ifont1.h"
#include "ifont2.h"
#include "ialloc.h"
#include "iname.h"   /* for CFF parser */
#include "iddict.h"  /* for CFF parser */
#include "store.h"   /* for CFF parser */
#include "iscannum.h"

/* Private utilities */
static uint
subr_bias(const ref * psubrs)
{
    uint size = r_size(psubrs);

    return (size < 1240 ? 107 : size < 33900 ? 1131 : 32768);
}

/*
 * Get the additional parameters for a Type 2 font (or FontType 2 FDArray
 * entry in a CIDFontType 0 font), beyond those common to Type 1 and Type 2
 * fonts.
 */
int
type2_font_params(const_os_ptr op, charstring_font_refs_t *pfr,
                  gs_type1_data *pdata1)
{
    int code;
    float dwx, nwx;
    ref *temp;

    pdata1->interpret = gs_type2_interpret;
    pdata1->lenIV = DEFAULT_LENIV_2;
    pdata1->subroutineNumberBias = subr_bias(pfr->Subrs);
    /* Get information specific to Type 2 fonts. */
    if (dict_find_string(pfr->Private, "GlobalSubrs", &temp) > 0) {
        if (!r_is_array(temp))
            return_error(gs_error_typecheck);
        pfr->GlobalSubrs = temp;
    }
    pdata1->gsubrNumberBias = subr_bias(pfr->GlobalSubrs);
    if ((code = dict_uint_param(pfr->Private, "gsubrNumberBias",
                                0, max_uint, pdata1->gsubrNumberBias,
                                &pdata1->gsubrNumberBias)) < 0 ||
        (code = dict_float_param(pfr->Private, "defaultWidthX", 0.0,
                                 &dwx)) < 0 ||
        (code = dict_float_param(pfr->Private, "nominalWidthX", 0.0,
                                 &nwx)) < 0
        )
        return code;
    pdata1->defaultWidthX = float2fixed(dwx);
    pdata1->nominalWidthX = float2fixed(nwx);
    {
        ref *pirs;

        if (dict_find_string(pfr->Private, "initialRandomSeed", &pirs) <= 0)
            pdata1->initialRandomSeed = 0;
        else if (!r_has_type(pirs, t_integer))
            return_error(gs_error_typecheck);
        else
            pdata1->initialRandomSeed = pirs->value.intval;
    }
    return 0;
}

/* <string|name> <font_dict> .buildfont2 <string|name> <font> */
/* Build a type 2 (compact Adobe encrypted) font. */
static int
zbuildfont2(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    charstring_font_refs_t refs;
    build_proc_refs build;
    int code = build_proc_name_refs(imemory, &build,
                                    "%Type2BuildChar", "%Type2BuildGlyph");
    gs_type1_data data1;

    if (code < 0)
        return code;
    code = charstring_font_get_refs(op, &refs);
    if (code < 0)
        return code;
    code = type2_font_params(op, &refs, &data1);
    if (code < 0)
        return code;
    return build_charstring_font(i_ctx_p, op, &build, ft_encrypted2, &refs,
                                 &data1, bf_notdef_required);
}

/* CFF parser */

#define STR2MEM(s)    s, (sizeof(s) - 1)
#define MAXOP 50   /* Max value according to TN 5176 is 48 */

static const char * const standard_strings[] = {
    /*   0 */ ".notdef",
    /*   1 */ "space",
    /*   2 */ "exclam",
    /*   3 */ "quotedbl",
    /*   4 */ "numbersign",
    /*   5 */ "dollar",
    /*   6 */ "percent",
    /*   7 */ "ampersand",
    /*   8 */ "quoteright",
    /*   9 */ "parenleft",
    /*  10 */ "parenright",
    /*  11 */ "asterisk",
    /*  12 */ "plus",
    /*  13 */ "comma",
    /*  14 */ "hyphen",
    /*  15 */ "period",
    /*  16 */ "slash",
    /*  17 */ "zero",
    /*  18 */ "one",
    /*  19 */ "two",
    /*  20 */ "three",
    /*  21 */ "four",
    /*  22 */ "five",
    /*  23 */ "six",
    /*  24 */ "seven",
    /*  25 */ "eight",
    /*  26 */ "nine",
    /*  27 */ "colon",
    /*  28 */ "semicolon",
    /*  29 */ "less",
    /*  30 */ "equal",
    /*  31 */ "greater",
    /*  32 */ "question",
    /*  33 */ "at",
    /*  34 */ "A",
    /*  35 */ "B",
    /*  36 */ "C",
    /*  37 */ "D",
    /*  38 */ "E",
    /*  39 */ "F",
    /*  40 */ "G",
    /*  41 */ "H",
    /*  42 */ "I",
    /*  43 */ "J",
    /*  44 */ "K",
    /*  45 */ "L",
    /*  46 */ "M",
    /*  47 */ "N",
    /*  48 */ "O",
    /*  49 */ "P",
    /*  50 */ "Q",
    /*  51 */ "R",
    /*  52 */ "S",
    /*  53 */ "T",
    /*  54 */ "U",
    /*  55 */ "V",
    /*  56 */ "W",
    /*  57 */ "X",
    /*  58 */ "Y",
    /*  59 */ "Z",
    /*  60 */ "bracketleft",
    /*  61 */ "backslash",
    /*  62 */ "bracketright",
    /*  63 */ "asciicircum",
    /*  64 */ "underscore",
    /*  65 */ "quoteleft",
    /*  66 */ "a",
    /*  67 */ "b",
    /*  68 */ "c",
    /*  69 */ "d",
    /*  70 */ "e",
    /*  71 */ "f",
    /*  72 */ "g",
    /*  73 */ "h",
    /*  74 */ "i",
    /*  75 */ "j",
    /*  76 */ "k",
    /*  77 */ "l",
    /*  78 */ "m",
    /*  79 */ "n",
    /*  80 */ "o",
    /*  81 */ "p",
    /*  82 */ "q",
    /*  83 */ "r",
    /*  84 */ "s",
    /*  85 */ "t",
    /*  86 */ "u",
    /*  87 */ "v",
    /*  88 */ "w",
    /*  89 */ "x",
    /*  90 */ "y",
    /*  91 */ "z",
    /*  92 */ "braceleft",
    /*  93 */ "bar",
    /*  94 */ "braceright",
    /*  95 */ "asciitilde",
    /*  96 */ "exclamdown",
    /*  97 */ "cent",
    /*  98 */ "sterling",
    /*  99 */ "fraction",
    /* 100 */ "yen",
    /* 101 */ "florin",
    /* 102 */ "section",
    /* 103 */ "currency",
    /* 104 */ "quotesingle",
    /* 105 */ "quotedblleft",
    /* 106 */ "guillemotleft",
    /* 107 */ "guilsinglleft",
    /* 108 */ "guilsinglright",
    /* 109 */ "fi",
    /* 110 */ "fl",
    /* 111 */ "endash",
    /* 112 */ "dagger",
    /* 113 */ "daggerdbl",
    /* 114 */ "periodcentered",
    /* 115 */ "paragraph",
    /* 116 */ "bullet",
    /* 117 */ "quotesinglbase",
    /* 118 */ "quotedblbase",
    /* 119 */ "quotedblright",
    /* 120 */ "guillemotright",
    /* 121 */ "ellipsis",
    /* 122 */ "perthousand",
    /* 123 */ "questiondown",
    /* 124 */ "grave",
    /* 125 */ "acute",
    /* 126 */ "circumflex",
    /* 127 */ "tilde",
    /* 128 */ "macron",
    /* 129 */ "breve",
    /* 130 */ "dotaccent",
    /* 131 */ "dieresis",
    /* 132 */ "ring",
    /* 133 */ "cedilla",
    /* 134 */ "hungarumlaut",
    /* 135 */ "ogonek",
    /* 136 */ "caron",
    /* 137 */ "emdash",
    /* 138 */ "AE",
    /* 139 */ "ordfeminine",
    /* 140 */ "Lslash",
    /* 141 */ "Oslash",
    /* 142 */ "OE",
    /* 143 */ "ordmasculine",
    /* 144 */ "ae",
    /* 145 */ "dotlessi",
    /* 146 */ "lslash",
    /* 147 */ "oslash",
    /* 148 */ "oe",
    /* 149 */ "germandbls",
    /* 150 */ "onesuperior",
    /* 151 */ "logicalnot",
    /* 152 */ "mu",
    /* 153 */ "trademark",
    /* 154 */ "Eth",
    /* 155 */ "onehalf",
    /* 156 */ "plusminus",
    /* 157 */ "Thorn",
    /* 158 */ "onequarter",
    /* 159 */ "divide",
    /* 160 */ "brokenbar",
    /* 161 */ "degree",
    /* 162 */ "thorn",
    /* 163 */ "threequarters",
    /* 164 */ "twosuperior",
    /* 165 */ "registered",
    /* 166 */ "minus",
    /* 167 */ "eth",
    /* 168 */ "multiply",
    /* 169 */ "threesuperior",
    /* 170 */ "copyright",
    /* 171 */ "Aacute",
    /* 172 */ "Acircumflex",
    /* 173 */ "Adieresis",
    /* 174 */ "Agrave",
    /* 175 */ "Aring",
    /* 176 */ "Atilde",
    /* 177 */ "Ccedilla",
    /* 178 */ "Eacute",
    /* 179 */ "Ecircumflex",
    /* 180 */ "Edieresis",
    /* 181 */ "Egrave",
    /* 182 */ "Iacute",
    /* 183 */ "Icircumflex",
    /* 184 */ "Idieresis",
    /* 185 */ "Igrave",
    /* 186 */ "Ntilde",
    /* 187 */ "Oacute",
    /* 188 */ "Ocircumflex",
    /* 189 */ "Odieresis",
    /* 190 */ "Ograve",
    /* 191 */ "Otilde",
    /* 192 */ "Scaron",
    /* 193 */ "Uacute",
    /* 194 */ "Ucircumflex",
    /* 195 */ "Udieresis",
    /* 196 */ "Ugrave",
    /* 197 */ "Yacute",
    /* 198 */ "Ydieresis",
    /* 199 */ "Zcaron",
    /* 200 */ "aacute",
    /* 201 */ "acircumflex",
    /* 202 */ "adieresis",
    /* 203 */ "agrave",
    /* 204 */ "aring",
    /* 205 */ "atilde",
    /* 206 */ "ccedilla",
    /* 207 */ "eacute",
    /* 208 */ "ecircumflex",
    /* 209 */ "edieresis",
    /* 210 */ "egrave",
    /* 211 */ "iacute",
    /* 212 */ "icircumflex",
    /* 213 */ "idieresis",
    /* 214 */ "igrave",
    /* 215 */ "ntilde",
    /* 216 */ "oacute",
    /* 217 */ "ocircumflex",
    /* 218 */ "odieresis",
    /* 219 */ "ograve",
    /* 220 */ "otilde",
    /* 221 */ "scaron",
    /* 222 */ "uacute",
    /* 223 */ "ucircumflex",
    /* 224 */ "udieresis",
    /* 225 */ "ugrave",
    /* 226 */ "yacute",
    /* 227 */ "ydieresis",
    /* 228 */ "zcaron",
    /* 229 */ "exclamsmall",
    /* 230 */ "Hungarumlautsmall",
    /* 231 */ "dollaroldstyle",
    /* 232 */ "dollarsuperior",
    /* 233 */ "ampersandsmall",
    /* 234 */ "Acutesmall",
    /* 235 */ "parenleftsuperior",
    /* 236 */ "parenrightsuperior",
    /* 237 */ "twodotenleader",
    /* 238 */ "onedotenleader",
    /* 239 */ "zerooldstyle",
    /* 240 */ "oneoldstyle",
    /* 241 */ "twooldstyle",
    /* 242 */ "threeoldstyle",
    /* 243 */ "fouroldstyle",
    /* 244 */ "fiveoldstyle",
    /* 245 */ "sixoldstyle",
    /* 246 */ "sevenoldstyle",
    /* 247 */ "eightoldstyle",
    /* 248 */ "nineoldstyle",
    /* 249 */ "commasuperior",
    /* 250 */ "threequartersemdash",
    /* 251 */ "periodsuperior",
    /* 252 */ "questionsmall",
    /* 253 */ "asuperior",
    /* 254 */ "bsuperior",
    /* 255 */ "centsuperior",
    /* 256 */ "dsuperior",
    /* 257 */ "esuperior",
    /* 258 */ "isuperior",
    /* 259 */ "lsuperior",
    /* 260 */ "msuperior",
    /* 261 */ "nsuperior",
    /* 262 */ "osuperior",
    /* 263 */ "rsuperior",
    /* 264 */ "ssuperior",
    /* 265 */ "tsuperior",
    /* 266 */ "ff",
    /* 267 */ "ffi",
    /* 268 */ "ffl",
    /* 269 */ "parenleftinferior",
    /* 270 */ "parenrightinferior",
    /* 271 */ "Circumflexsmall",
    /* 272 */ "hyphensuperior",
    /* 273 */ "Gravesmall",
    /* 274 */ "Asmall",
    /* 275 */ "Bsmall",
    /* 276 */ "Csmall",
    /* 277 */ "Dsmall",
    /* 278 */ "Esmall",
    /* 279 */ "Fsmall",
    /* 280 */ "Gsmall",
    /* 281 */ "Hsmall",
    /* 282 */ "Ismall",
    /* 283 */ "Jsmall",
    /* 284 */ "Ksmall",
    /* 285 */ "Lsmall",
    /* 286 */ "Msmall",
    /* 287 */ "Nsmall",
    /* 288 */ "Osmall",
    /* 289 */ "Psmall",
    /* 290 */ "Qsmall",
    /* 291 */ "Rsmall",
    /* 292 */ "Ssmall",
    /* 293 */ "Tsmall",
    /* 294 */ "Usmall",
    /* 295 */ "Vsmall",
    /* 296 */ "Wsmall",
    /* 297 */ "Xsmall",
    /* 298 */ "Ysmall",
    /* 299 */ "Zsmall",
    /* 300 */ "colonmonetary",
    /* 301 */ "onefitted",
    /* 302 */ "rupiah",
    /* 303 */ "Tildesmall",
    /* 304 */ "exclamdownsmall",
    /* 305 */ "centoldstyle",
    /* 306 */ "Lslashsmall",
    /* 307 */ "Scaronsmall",
    /* 308 */ "Zcaronsmall",
    /* 309 */ "Dieresissmall",
    /* 310 */ "Brevesmall",
    /* 311 */ "Caronsmall",
    /* 312 */ "Dotaccentsmall",
    /* 313 */ "Macronsmall",
    /* 314 */ "figuredash",
    /* 315 */ "hypheninferior",
    /* 316 */ "Ogoneksmall",
    /* 317 */ "Ringsmall",
    /* 318 */ "Cedillasmall",
    /* 319 */ "questiondownsmall",
    /* 320 */ "oneeighth",
    /* 321 */ "threeeighths",
    /* 322 */ "fiveeighths",
    /* 323 */ "seveneighths",
    /* 324 */ "onethird",
    /* 325 */ "twothirds",
    /* 326 */ "zerosuperior",
    /* 327 */ "foursuperior",
    /* 328 */ "fivesuperior",
    /* 329 */ "sixsuperior",
    /* 330 */ "sevensuperior",
    /* 331 */ "eightsuperior",
    /* 332 */ "ninesuperior",
    /* 333 */ "zeroinferior",
    /* 334 */ "oneinferior",
    /* 335 */ "twoinferior",
    /* 336 */ "threeinferior",
    /* 337 */ "fourinferior",
    /* 338 */ "fiveinferior",
    /* 339 */ "sixinferior",
    /* 340 */ "seveninferior",
    /* 341 */ "eightinferior",
    /* 342 */ "nineinferior",
    /* 343 */ "centinferior",
    /* 344 */ "dollarinferior",
    /* 345 */ "periodinferior",
    /* 346 */ "commainferior",
    /* 347 */ "Agravesmall",
    /* 348 */ "Aacutesmall",
    /* 349 */ "Acircumflexsmall",
    /* 350 */ "Atildesmall",
    /* 351 */ "Adieresissmall",
    /* 352 */ "Aringsmall",
    /* 353 */ "AEsmall",
    /* 354 */ "Ccedillasmall",
    /* 355 */ "Egravesmall",
    /* 356 */ "Eacutesmall",
    /* 357 */ "Ecircumflexsmall",
    /* 358 */ "Edieresissmall",
    /* 359 */ "Igravesmall",
    /* 360 */ "Iacutesmall",
    /* 361 */ "Icircumflexsmall",
    /* 362 */ "Idieresissmall",
    /* 363 */ "Ethsmall",
    /* 364 */ "Ntildesmall",
    /* 365 */ "Ogravesmall",
    /* 366 */ "Oacutesmall",
    /* 367 */ "Ocircumflexsmall",
    /* 368 */ "Otildesmall",
    /* 369 */ "Odieresissmall",
    /* 370 */ "OEsmall",
    /* 371 */ "Oslashsmall",
    /* 372 */ "Ugravesmall",
    /* 373 */ "Uacutesmall",
    /* 374 */ "Ucircumflexsmall",
    /* 375 */ "Udieresissmall",
    /* 376 */ "Yacutesmall",
    /* 377 */ "Thornsmall",
    /* 378 */ "Ydieresissmall",
    /* 379 */ "001.000",
    /* 380 */ "001.001",
    /* 381 */ "001.002",
    /* 382 */ "001.003",
    /* 383 */ "Black",
    /* 384 */ "Bold",
    /* 385 */ "Book",
    /* 386 */ "Light",
    /* 387 */ "Medium",
    /* 388 */ "Regular",
    /* 389 */ "Roman",
    /* 390 */ "Semibold"
};

const static unsigned short expert_charset[] = {
    1,   /* space */
    229, /* exclamsmall */
    230, /* Hungarumlautsmall */
    231, /* dollaroldstyle */
    232, /* dollarsuperior */
    233, /* ampersandsmall */
    234, /* Acutesmall */
    235, /* parenleftsuperior */
    236, /* parenrightsuperior */
    237, /* twodotenleader */
    238, /* onedotenleader */
    13,  /* comma */
    14,  /* hyphen */
    15,  /* period */
    99,  /* fraction */
    239, /* zerooldstyle */
    240, /* oneoldstyle */
    241, /* twooldstyle */
    242, /* threeoldstyle */
    243, /* fouroldstyle */
    244, /* fiveoldstyle */
    245, /* sixoldstyle */
    246, /* sevenoldstyle */
    247, /* eightoldstyle */
    248, /* nineoldstyle */
    27,  /* colon */
    28,  /* semicolon */
    249, /* commasuperior */
    250, /* threequartersemdash */
    251, /* periodsuperior */
    252, /* questionsmall */
    253, /* asuperior */
    254, /* bsuperior */
    255, /* centsuperior */
    256, /* dsuperior */
    257, /* esuperior */
    258, /* isuperior */
    259, /* lsuperior */
    260, /* msuperior */
    261, /* nsuperior */
    262, /* osuperior */
    263, /* rsuperior */
    264, /* ssuperior */
    265, /* tsuperior */
    266, /* ff */
    109, /* fi */
    110, /* fl */
    267, /* ffi */
    268, /* ffl */
    269, /* parenleftinferior */
    270, /* parenrightinferior */
    271, /* Circumflexsmall */
    272, /* hyphensuperior */
    273, /* Gravesmall */
    274, /* Asmall */
    275, /* Bsmall */
    276, /* Csmall */
    277, /* Dsmall */
    278, /* Esmall */
    279, /* Fsmall */
    280, /* Gsmall */
    281, /* Hsmall */
    282, /* Ismall */
    283, /* Jsmall */
    284, /* Ksmall */
    285, /* Lsmall */
    286, /* Msmall */
    287, /* Nsmall */
    288, /* Osmall */
    289, /* Psmall */
    290, /* Qsmall */
    291, /* Rsmall */
    292, /* Ssmall */
    293, /* Tsmall */
    294, /* Usmall */
    295, /* Vsmall */
    296, /* Wsmall */
    297, /* Xsmall */
    298, /* Ysmall */
    299, /* Zsmall */
    300, /* colonmonetary */
    301, /* onefitted */
    302, /* rupiah */
    303, /* Tildesmall */
    304, /* exclamdownsmall */
    305, /* centoldstyle */
    306, /* Lslashsmall */
    307, /* Scaronsmall */
    308, /* Zcaronsmall */
    309, /* Dieresissmall */
    310, /* Brevesmall */
    311, /* Caronsmall */
    312, /* Dotaccentsmall */
    313, /* Macronsmall */
    314, /* figuredash */
    315, /* hypheninferior */
    316, /* Ogoneksmall */
    317, /* Ringsmall */
    318, /* Cedillasmall */
    158, /* onequarter */
    155, /* onehalf */
    163, /* threequarters */
    319, /* questiondownsmall */
    320, /* oneeighth */
    321, /* threeeighths */
    322, /* fiveeighths */
    323, /* seveneighths */
    324, /* onethird */
    325, /* twothirds */
    326, /* zerosuperior */
    150, /* onesuperior */
    164, /* twosuperior */
    169, /* threesuperior */
    327, /* foursuperior */
    328, /* fivesuperior */
    329, /* sixsuperior */
    330, /* sevensuperior */
    331, /* eightsuperior */
    332, /* ninesuperior */
    333, /* zeroinferior */
    334, /* oneinferior */
    335, /* twoinferior */
    336, /* threeinferior */
    337, /* fourinferior */
    338, /* fiveinferior */
    339, /* sixinferior */
    340, /* seveninferior */
    341, /* eightinferior */
    342, /* nineinferior */
    343, /* centinferior */
    344, /* dollarinferior */
    345, /* periodinferior */
    346, /* commainferior */
    347, /* Agravesmall */
    348, /* Aacutesmall */
    349, /* Acircumflexsmall */
    350, /* Atildesmall */
    351, /* Adieresissmall */
    352, /* Aringsmall */
    353, /* AEsmall */
    354, /* Ccedillasmall */
    355, /* Egravesmall */
    356, /* Eacutesmall */
    357, /* Ecircumflexsmall */
    358, /* Edieresissmall */
    359, /* Igravesmall */
    360, /* Iacutesmall */
    361, /* Icircumflexsmall */
    362, /* Idieresissmall */
    363, /* Ethsmall */
    364, /* Ntildesmall */
    365, /* Ogravesmall */
    366, /* Oacutesmall */
    367, /* Ocircumflexsmall */
    368, /* Otildesmall */
    369, /* Odieresissmall */
    370, /* OEsmall */
    371, /* Oslashsmall */
    372, /* Ugravesmall */
    373, /* Uacutesmall */
    374, /* Ucircumflexsmall */
    375, /* Udieresissmall */
    376, /* Yacutesmall */
    377, /* Thornsmall */
    378  /* Ydieresissmall */
};

const static unsigned short expert_subset_charset[] = {
    1,   /*space */
    231, /*dollaroldstyle */
    232, /*dollarsuperior */
    235, /*parenleftsuperior */
    236, /*parenrightsuperior */
    237, /*twodotenleader */
    238, /*onedotenleader */
    13,  /*comma */
    14,  /*hyphen */
    15,  /*period */
    99,  /*fraction */
    239, /*zerooldstyle */
    240, /*oneoldstyle */
    241, /*twooldstyle */
    242, /*threeoldstyle */
    243, /*fouroldstyle */
    244, /*fiveoldstyle */
    245, /*sixoldstyle */
    246, /*sevenoldstyle */
    247, /*eightoldstyle */
    248, /*nineoldstyle */
    27,  /*colon */
    28,  /*semicolon */
    249, /*commasuperior */
    250, /*threequartersemdash */
    251, /*periodsuperior */
    253, /*asuperior */
    254, /*bsuperior */
    255, /*centsuperior */
    256, /*dsuperior */
    257, /*esuperior */
    258, /*isuperior */
    259, /*lsuperior */
    260, /*msuperior */
    261, /*nsuperior */
    262, /*osuperior */
    263, /*rsuperior */
    264, /*ssuperior */
    265, /*tsuperior */
    266, /*ff */
    109, /*fi */
    110, /*fl */
    267, /*ffi */
    268, /*ffl */
    269, /*parenleftinferior */
    270, /*parenrightinferior */
    272, /*hyphensuperior */
    300, /*colonmonetary */
    301, /*onefitted */
    302, /*rupiah */
    305, /*centoldstyle */
    314, /*figuredash */
    315, /*hypheninferior */
    158, /*onequarter */
    155, /*onehalf */
    163, /*threequarters */
    320, /*oneeighth */
    321, /*threeeighths */
    322, /*fiveeighths */
    323, /*seveneighths */
    324, /*onethird */
    325, /*twothirds */
    326, /*zerosuperior */
    150, /*onesuperior */
    164, /*twosuperior */
    169, /*threesuperior */
    327, /*foursuperior */
    328, /*fivesuperior */
    329, /*sixsuperior */
    330, /*sevensuperior */
    331, /*eightsuperior */
    332, /*ninesuperior */
    333, /*zeroinferior */
    334, /*oneinferior */
    335, /*twoinferior */
    336, /*threeinferior */
    337, /*fourinferior */
    338, /*fiveinferior */
    339, /*sixinferior */
    340, /*seveninferior */
    341, /*eightinferior */
    342, /*nineinferior */
    343, /*centinferior */
    344, /*dollarinferior */
    345, /*periodinferior */
    346  /*commainferior */
};

static const unsigned short standard_encoding[] = {
  /*   0 */   0, /* .notdef */
  /*   1 */   0, /* .notdef */
  /*   2 */   0, /* .notdef */
  /*   3 */   0, /* .notdef */
  /*   4 */   0, /* .notdef */
  /*   5 */   0, /* .notdef */
  /*   6 */   0, /* .notdef */
  /*   7 */   0, /* .notdef */
  /*   8 */   0, /* .notdef */
  /*   9 */   0, /* .notdef */
  /*  10 */   0, /* .notdef */
  /*  11 */   0, /* .notdef */
  /*  12 */   0, /* .notdef */
  /*  13 */   0, /* .notdef */
  /*  14 */   0, /* .notdef */
  /*  15 */   0, /* .notdef */
  /*  16 */   0, /* .notdef */
  /*  17 */   0, /* .notdef */
  /*  18 */   0, /* .notdef */
  /*  19 */   0, /* .notdef */
  /*  20 */   0, /* .notdef */
  /*  21 */   0, /* .notdef */
  /*  22 */   0, /* .notdef */
  /*  23 */   0, /* .notdef */
  /*  24 */   0, /* .notdef */
  /*  25 */   0, /* .notdef */
  /*  26 */   0, /* .notdef */
  /*  27 */   0, /* .notdef */
  /*  28 */   0, /* .notdef */
  /*  29 */   0, /* .notdef */
  /*  30 */   0, /* .notdef */
  /*  31 */   0, /* .notdef */
  /*  32 */   1, /* space */
  /*  33 */   2, /* exclam */
  /*  34 */   3, /* quotedbl */
  /*  35 */   4, /* numbersign */
  /*  36 */   5, /* dollar */
  /*  37 */   6, /* percent */
  /*  38 */   7, /* ampersand */
  /*  39 */   8, /* quoteright */
  /*  40 */   9, /* parenleft */
  /*  41 */  10, /* parenright */
  /*  42 */  11, /* asterisk */
  /*  43 */  12, /* plus */
  /*  44 */  13, /* comma */
  /*  45 */  14, /* hyphen */
  /*  46 */  15, /* period */
  /*  47 */  16, /* slash */
  /*  48 */  17, /* zero */
  /*  49 */  18, /* one */
  /*  50 */  19, /* two */
  /*  51 */  20, /* three */
  /*  52 */  21, /* four */
  /*  53 */  22, /* five */
  /*  54 */  23, /* six */
  /*  55 */  24, /* seven */
  /*  56 */  25, /* eight */
  /*  57 */  26, /* nine */
  /*  58 */  27, /* colon */
  /*  59 */  28, /* semicolon */
  /*  60 */  29, /* less */
  /*  61 */  30, /* equal */
  /*  62 */  31, /* greater */
  /*  63 */  32, /* question */
  /*  64 */  33, /* at */
  /*  65 */  34, /* A */
  /*  66 */  35, /* B */
  /*  67 */  36, /* C */
  /*  68 */  37, /* D */
  /*  69 */  38, /* E */
  /*  70 */  39, /* F */
  /*  71 */  40, /* G */
  /*  72 */  41, /* H */
  /*  73 */  42, /* I */
  /*  74 */  43, /* J */
  /*  75 */  44, /* K */
  /*  76 */  45, /* L */
  /*  77 */  46, /* M */
  /*  78 */  47, /* N */
  /*  79 */  48, /* O */
  /*  80 */  49, /* P */
  /*  81 */  50, /* Q */
  /*  82 */  51, /* R */
  /*  83 */  52, /* S */
  /*  84 */  53, /* T */
  /*  85 */  54, /* U */
  /*  86 */  55, /* V */
  /*  87 */  56, /* W */
  /*  88 */  57, /* X */
  /*  89 */  58, /* Y */
  /*  90 */  59, /* Z */
  /*  91 */  60, /* bracketleft */
  /*  92 */  61, /* backslash */
  /*  93 */  62, /* bracketright */
  /*  94 */  63, /* asciicircum */
  /*  95 */  64, /* underscore */
  /*  96 */  65, /* quoteleft */
  /*  97 */  66, /* a */
  /*  98 */  67, /* b */
  /*  99 */  68, /* c */
  /* 100 */  69, /* d */
  /* 101 */  70, /* e */
  /* 102 */  71, /* f */
  /* 103 */  72, /* g */
  /* 104 */  73, /* h */
  /* 105 */  74, /* i */
  /* 106 */  75, /* j */
  /* 107 */  76, /* k */
  /* 108 */  77, /* l */
  /* 109 */  78, /* m */
  /* 110 */  79, /* n */
  /* 111 */  80, /* o */
  /* 112 */  81, /* p */
  /* 113 */  82, /* q */
  /* 114 */  83, /* r */
  /* 115 */  84, /* s */
  /* 116 */  85, /* t */
  /* 117 */  86, /* u */
  /* 118 */  87, /* v */
  /* 119 */  88, /* w */
  /* 120 */  89, /* x */
  /* 121 */  90, /* y */
  /* 122 */  91, /* z */
  /* 123 */  92, /* braceleft */
  /* 124 */  93, /* bar */
  /* 125 */  94, /* braceright */
  /* 126 */  95, /* asciitilde */
  /* 127 */   0, /* .notdef */
  /* 128 */   0, /* .notdef */
  /* 129 */   0, /* .notdef */
  /* 130 */   0, /* .notdef */
  /* 131 */   0, /* .notdef */
  /* 132 */   0, /* .notdef */
  /* 133 */   0, /* .notdef */
  /* 134 */   0, /* .notdef */
  /* 135 */   0, /* .notdef */
  /* 136 */   0, /* .notdef */
  /* 137 */   0, /* .notdef */
  /* 138 */   0, /* .notdef */
  /* 139 */   0, /* .notdef */
  /* 140 */   0, /* .notdef */
  /* 141 */   0, /* .notdef */
  /* 142 */   0, /* .notdef */
  /* 143 */   0, /* .notdef */
  /* 144 */   0, /* .notdef */
  /* 145 */   0, /* .notdef */
  /* 146 */   0, /* .notdef */
  /* 147 */   0, /* .notdef */
  /* 148 */   0, /* .notdef */
  /* 149 */   0, /* .notdef */
  /* 150 */   0, /* .notdef */
  /* 151 */   0, /* .notdef */
  /* 152 */   0, /* .notdef */
  /* 153 */   0, /* .notdef */
  /* 154 */   0, /* .notdef */
  /* 155 */   0, /* .notdef */
  /* 156 */   0, /* .notdef */
  /* 157 */   0, /* .notdef */
  /* 158 */   0, /* .notdef */
  /* 159 */   0, /* .notdef */
  /* 160 */   0, /* .notdef */
  /* 161 */  96, /* exclamdown */
  /* 162 */  97, /* cent */
  /* 163 */  98, /* sterling */
  /* 164 */  99, /* fraction */
  /* 165 */ 100, /* yen */
  /* 166 */ 101, /* florin */
  /* 167 */ 102, /* section */
  /* 168 */ 103, /* currency */
  /* 169 */ 104, /* quotesingle */
  /* 170 */ 105, /* quotedblleft */
  /* 171 */ 106, /* guillemotleft */
  /* 172 */ 107, /* guilsinglleft */
  /* 173 */ 108, /* guilsinglright */
  /* 174 */ 109, /* fi */
  /* 175 */ 110, /* fl */
  /* 176 */   0, /* .notdef */
  /* 177 */ 111, /* endash */
  /* 178 */ 112, /* dagger */
  /* 179 */ 113, /* daggerdbl */
  /* 180 */ 114, /* periodcentered */
  /* 181 */   0, /* .notdef */
  /* 182 */ 115, /* paragraph */
  /* 183 */ 116, /* bullet */
  /* 184 */ 117, /* quotesinglbase */
  /* 185 */ 118, /* quotedblbase */
  /* 186 */ 119, /* quotedblright */
  /* 187 */ 120, /* guillemotright */
  /* 188 */ 121, /* ellipsis */
  /* 189 */ 122, /* perthousand */
  /* 190 */   0, /* .notdef */
  /* 191 */ 123, /* questiondown */
  /* 192 */   0, /* .notdef */
  /* 193 */ 124, /* grave */
  /* 194 */ 125, /* acute */
  /* 195 */ 126, /* circumflex */
  /* 196 */ 127, /* tilde */
  /* 197 */ 128, /* macron */
  /* 198 */ 129, /* breve */
  /* 199 */ 130, /* dotaccent */
  /* 200 */ 131, /* dieresis */
  /* 201 */   0, /* .notdef */
  /* 202 */ 132, /* ring */
  /* 203 */ 133, /* cedilla */
  /* 204 */   0, /* .notdef */
  /* 205 */ 134, /* hungarumlaut */
  /* 206 */ 135, /* ogonek */
  /* 207 */ 136, /* caron */
  /* 208 */ 137, /* emdash */
  /* 209 */   0, /* .notdef */
  /* 210 */   0, /* .notdef */
  /* 211 */   0, /* .notdef */
  /* 212 */   0, /* .notdef */
  /* 213 */   0, /* .notdef */
  /* 214 */   0, /* .notdef */
  /* 215 */   0, /* .notdef */
  /* 216 */   0, /* .notdef */
  /* 217 */   0, /* .notdef */
  /* 218 */   0, /* .notdef */
  /* 219 */   0, /* .notdef */
  /* 220 */   0, /* .notdef */
  /* 221 */   0, /* .notdef */
  /* 222 */   0, /* .notdef */
  /* 223 */   0, /* .notdef */
  /* 224 */   0, /* .notdef */
  /* 225 */ 138, /* AE */
  /* 226 */   0, /* .notdef */
  /* 227 */ 139, /* ordfeminine */
  /* 228 */   0, /* .notdef */
  /* 229 */   0, /* .notdef */
  /* 230 */   0, /* .notdef */
  /* 231 */   0, /* .notdef */
  /* 232 */ 140, /* Lslash */
  /* 233 */ 141, /* Oslash */
  /* 234 */ 142, /* OE */
  /* 235 */ 143, /* ordmasculine */
  /* 236 */   0, /* .notdef */
  /* 237 */   0, /* .notdef */
  /* 238 */   0, /* .notdef */
  /* 239 */   0, /* .notdef */
  /* 240 */   0, /* .notdef */
  /* 241 */ 144, /* ae */
  /* 242 */   0, /* .notdef */
  /* 243 */   0, /* .notdef */
  /* 244 */   0, /* .notdef */
  /* 245 */ 145, /* dotlessi */
  /* 246 */   0, /* .notdef */
  /* 247 */   0, /* .notdef */
  /* 248 */ 146, /* lslash */
  /* 249 */ 147, /* oslash */
  /* 250 */ 148, /* oe */
  /* 251 */ 149, /* germandbls */
  /* 252 */   0, /* .notdef */
  /* 253 */   0, /* .notdef */
  /* 254 */   0, /* .notdef */
  /* 255 */   0  /* .notdef */
};

static const unsigned short expert_encoding[] = {
  /*   0 */   0, /* .notdef */
  /*   1 */   0, /* .notdef */
  /*   2 */   0, /* .notdef */
  /*   3 */   0, /* .notdef */
  /*   4 */   0, /* .notdef */
  /*   5 */   0, /* .notdef */
  /*   6 */   0, /* .notdef */
  /*   7 */   0, /* .notdef */
  /*   8 */   0, /* .notdef */
  /*   9 */   0, /* .notdef */
  /*  10 */   0, /* .notdef */
  /*  11 */   0, /* .notdef */
  /*  12 */   0, /* .notdef */
  /*  13 */   0, /* .notdef */
  /*  14 */   0, /* .notdef */
  /*  15 */   0, /* .notdef */
  /*  16 */   0, /* .notdef */
  /*  17 */   0, /* .notdef */
  /*  18 */   0, /* .notdef */
  /*  19 */   0, /* .notdef */
  /*  20 */   0, /* .notdef */
  /*  21 */   0, /* .notdef */
  /*  22 */   0, /* .notdef */
  /*  23 */   0, /* .notdef */
  /*  24 */   0, /* .notdef */
  /*  25 */   0, /* .notdef */
  /*  26 */   0, /* .notdef */
  /*  27 */   0, /* .notdef */
  /*  28 */   0, /* .notdef */
  /*  29 */   0, /* .notdef */
  /*  30 */   0, /* .notdef */
  /*  31 */   0, /* .notdef */
  /*  32 */   1, /* space */
  /*  33 */ 229, /* exclamsmall */
  /*  34 */ 230, /* Hungarumlautsmall */
  /*  35 */   0, /* .notdef */
  /*  36 */ 231, /* dollaroldstyle */
  /*  37 */ 232, /* dollarsuperior */
  /*  38 */ 233, /* ampersandsmall */
  /*  39 */ 234, /* Acutesmall */
  /*  40 */ 235, /* parenleftsuperior */
  /*  41 */ 236, /* parenrightsuperior */
  /*  42 */ 237, /* twodotenleader */
  /*  43 */ 238, /* onedotenleader */
  /*  44 */  13, /* comma */
  /*  45 */  14, /* hyphen */
  /*  46 */  15, /* period */
  /*  47 */  99, /* fraction */
  /*  48 */ 239, /* zerooldstyle */
  /*  49 */ 240, /* oneoldstyle */
  /*  50 */ 241, /* twooldstyle */
  /*  51 */ 242, /* threeoldstyle */
  /*  52 */ 243, /* fouroldstyle */
  /*  53 */ 244, /* fiveoldstyle */
  /*  54 */ 245, /* sixoldstyle */
  /*  55 */ 246, /* sevenoldstyle */
  /*  56 */ 247, /* eightoldstyle */
  /*  57 */ 248, /* nineoldstyle */
  /*  58 */  27, /* colon */
  /*  59 */ 28, /* semicolon */
  /*  60 */ 249, /* commasuperior */
  /*  61 */ 250, /* threequartersemdash */
  /*  62 */ 251, /* periodsuperior */
  /*  63 */ 252, /* questionsmall */
  /*  64 */   0, /* .notdef */
  /*  65 */ 253, /* asuperior */
  /*  66 */ 254, /* bsuperior */
  /*  67 */ 255, /* centsuperior */
  /*  68 */ 256, /* dsuperior */
  /*  69 */ 257, /* esuperior */
  /*  70 */   0, /* .notdef */
  /*  71 */   0, /* .notdef */
  /*  72 */   0, /* .notdef */
  /*  73 */ 258, /* isuperior */
  /*  74 */   0, /* .notdef */
  /*  75 */   0, /* .notdef */
  /*  76 */ 259, /* lsuperior */
  /*  77 */ 260, /* msuperior */
  /*  78 */ 261, /* nsuperior */
  /*  79 */ 262, /* osuperior */
  /*  80 */   0, /* .notdef */
  /*  81 */   0, /* .notdef */
  /*  82 */ 263, /* rsuperior */
  /*  83 */ 264, /* ssuperior */
  /*  84 */ 265, /* tsuperior */
  /*  85 */   0, /* .notdef */
  /*  86 */ 266, /* ff */
  /*  87 */ 109, /* fi */
  /*  88 */ 110, /* fl */
  /*  89 */ 267, /* ffi */
  /*  90 */ 268, /* ffl */
  /*  91 */ 269, /* parenleftinferior */
  /*  92 */   0, /* .notdef */
  /*  93 */ 270, /* parenrightinferior */
  /*  94 */ 271, /* Circumflexsmall */
  /*  95 */ 272, /* hyphensuperior */
  /*  96 */ 273, /* Gravesmall */
  /*  97 */ 274, /* Asmall */
  /*  98 */ 275, /* Bsmall */
  /*  99 */ 276, /* Csmall */
  /* 100 */ 277, /* Dsmall */
  /* 101 */ 278, /* Esmall */
  /* 102 */ 279, /* Fsmall */
  /* 103 */ 280, /* Gsmall */
  /* 104 */ 281, /* Hsmall */
  /* 105 */ 282, /* Ismall */
  /* 106 */ 283, /* Jsmall */
  /* 107 */ 284, /* Ksmall */
  /* 108 */ 285, /* Lsmall */
  /* 109 */ 286, /* Msmall */
  /* 110 */ 287, /* Nsmall */
  /* 111 */ 288, /* Osmall */
  /* 112 */ 289, /* Psmall */
  /* 113 */ 290, /* Qsmall */
  /* 114 */ 291, /* Rsmall */
  /* 115 */ 292, /* Ssmall */
  /* 116 */ 293, /* Tsmall */
  /* 117 */ 294, /* Usmall */
  /* 118 */ 295, /* Vsmall */
  /* 119 */ 296, /* Wsmall */
  /* 120 */ 297, /* Xsmall */
  /* 121 */ 298, /* Ysmall */
  /* 122 */ 299, /* Zsmall */
  /* 123 */ 300, /* colonmonetary */
  /* 124 */ 301, /* onefitted */
  /* 125 */ 302, /* rupiah */
  /* 126 */ 303, /* Tildesmall */
  /* 127 */   0, /* .notdef */
  /* 128 */   0, /* .notdef */
  /* 129 */   0, /* .notdef */
  /* 130 */   0, /* .notdef */
  /* 131 */   0, /* .notdef */
  /* 132 */   0, /* .notdef */
  /* 133 */   0, /* .notdef */
  /* 134 */   0, /* .notdef */
  /* 135 */   0, /* .notdef */
  /* 136 */   0, /* .notdef */
  /* 137 */   0, /* .notdef */
  /* 138 */   0, /* .notdef */
  /* 139 */   0, /* .notdef */
  /* 140 */   0, /* .notdef */
  /* 141 */   0, /* .notdef */
  /* 142 */   0, /* .notdef */
  /* 143 */   0, /* .notdef */
  /* 144 */   0, /* .notdef */
  /* 145 */   0, /* .notdef */
  /* 146 */   0, /* .notdef */
  /* 147 */   0, /* .notdef */
  /* 148 */   0, /* .notdef */
  /* 149 */   0, /* .notdef */
  /* 150 */   0, /* .notdef */
  /* 151 */   0, /* .notdef */
  /* 152 */   0, /* .notdef */
  /* 153 */   0, /* .notdef */
  /* 154 */   0, /* .notdef */
  /* 155 */   0, /* .notdef */
  /* 156 */   0, /* .notdef */
  /* 157 */   0, /* .notdef */
  /* 158 */   0, /* .notdef */
  /* 159 */   0, /* .notdef */
  /* 160 */   0, /* .notdef */
  /* 161 */ 304, /* exclamdownsmall */
  /* 162 */ 305, /* centoldstyle */
  /* 163 */ 306, /* Lslashsmall */
  /* 164 */   0, /* .notdef */
  /* 165 */   0, /* .notdef */
  /* 166 */ 307, /* Scaronsmall */
  /* 167 */ 308, /* Zcaronsmall */
  /* 168 */ 309, /* Dieresissmall */
  /* 169 */ 310, /* Brevesmall */
  /* 170 */ 311, /* Caronsmall */
  /* 171 */   0, /* .notdef */
  /* 172 */ 312, /* Dotaccentsmall */
  /* 173 */   0, /* .notdef */
  /* 174 */   0, /* .notdef */
  /* 175 */ 313, /* Macronsmall */
  /* 176 */   0, /* .notdef */
  /* 177 */   0, /* .notdef */
  /* 178 */ 314, /* figuredash */
  /* 179 */ 315, /* hypheninferior */
  /* 180 */   0, /* .notdef */
  /* 181 */   0, /* .notdef */
  /* 182 */ 316, /* Ogoneksmall */
  /* 183 */ 317, /* Ringsmall */
  /* 184 */ 318, /* Cedillasmall */
  /* 185 */   0, /* .notdef */
  /* 186 */   0, /* .notdef */
  /* 187 */   0, /* .notdef */
  /* 188 */ 158, /* onequarter */
  /* 189 */ 155, /* onehalf */
  /* 190 */ 163, /* threequarters */
  /* 191 */ 319, /* questiondownsmall */
  /* 192 */ 320, /* oneeighth */
  /* 193 */ 321, /* threeeighths */
  /* 194 */ 322, /* fiveeighths */
  /* 195 */ 323, /* seveneighths */
  /* 196 */ 324, /* onethird */
  /* 197 */ 325, /* twothirds */
  /* 198 */   0, /* .notdef */
  /* 199 */   0, /* .notdef */
  /* 200 */ 326, /* zerosuperior */
  /* 201 */ 150, /* onesuperior */
  /* 202 */ 164, /* twosuperior */
  /* 203 */ 169, /* threesuperior */
  /* 204 */ 327, /* foursuperior */
  /* 205 */ 328, /* fivesuperior */
  /* 206 */ 329, /* sixsuperior */
  /* 207 */ 330, /* sevensuperior */
  /* 208 */ 331, /* eightsuperior */
  /* 209 */ 332, /* ninesuperior */
  /* 210 */ 333, /* zeroinferior */
  /* 211 */ 334, /* oneinferior */
  /* 212 */ 335, /* twoinferior */
  /* 213 */ 336, /* threeinferior */
  /* 214 */ 337, /* fourinferior */
  /* 215 */ 338, /* fiveinferior */
  /* 216 */ 339, /* sixinferior */
  /* 217 */ 340, /* seveninferior */
  /* 218 */ 341, /* eightinferior */
  /* 219 */ 342, /* nineinferior */
  /* 220 */ 343, /* centinferior */
  /* 221 */ 344, /* dollarinferior */
  /* 222 */ 345, /* periodinferior */
  /* 223 */ 346, /* commainferior */
  /* 224 */ 347, /* Agravesmall */
  /* 225 */ 348, /* Aacutesmall */
  /* 226 */ 349, /* Acircumflexsmall */
  /* 227 */ 350, /* Atildesmall */
  /* 228 */ 351, /* Adieresissmall */
  /* 229 */ 352, /* Aringsmall */
  /* 230 */ 353, /* AEsmall */
  /* 231 */ 354, /* Ccedillasmall */
  /* 232 */ 355, /* Egravesmall */
  /* 233 */ 356, /* Eacutesmall */
  /* 234 */ 357, /* Ecircumflexsmall */
  /* 235 */ 358, /* Edieresissmall */
  /* 236 */ 359, /* Igravesmall */
  /* 237 */ 360, /* Iacutesmall */
  /* 238 */ 361, /* Icircumflexsmall */
  /* 239 */ 362, /* Idieresissmall */
  /* 240 */ 363, /* Ethsmall */
  /* 241 */ 364, /* Ntildesmall */
  /* 242 */ 365, /* Ogravesmall */
  /* 243 */ 366, /* Oacutesmall */
  /* 244 */ 367, /* Ocircumflexsmall */
  /* 245 */ 368, /* Otildesmall */
  /* 246 */ 369, /* Odieresissmall */
  /* 247 */ 370, /* OEsmall */
  /* 248 */ 371, /* Oslashsmall */
  /* 249 */ 372, /* Ugravesmall */
  /* 250 */ 373, /* Uacutesmall */
  /* 251 */ 374, /* Ucircumflexsmall */
  /* 252 */ 375, /* Udieresissmall */
  /* 253 */ 376, /* Yacutesmall */
  /* 254 */ 377, /* Thornsmall */
  /* 255 */ 378  /* Ydieresissmall */
};

typedef struct tag_cff_data_t {
  ref *blk_ref;
  unsigned int length;
  unsigned int shift;
  unsigned int mask;
} cff_data_t;

typedef struct tag_cff_index_t {
  unsigned int start, end, data;
  unsigned int  offsize, count;
} cff_index_t;

static int
card8(unsigned int *u, const cff_data_t *o, unsigned p, unsigned pe)
{
    if (pe > o->length || p > pe - 1)
        return_error(gs_error_rangecheck); /* out of range access */
    *u = o->blk_ref[p >> o->shift].value.bytes[p & o->mask];
    return 0;
}

static int
card16(unsigned int *u, const cff_data_t *o, unsigned p, unsigned pe)
{
    if (pe > o->length || p > pe - 2)
        return_error(gs_error_rangecheck); /* out of range access */
    *u = (o->blk_ref[ p      >> o->shift].value.bytes[ p      & o->mask]) << 8 |
          o->blk_ref[(p + 1) >> o->shift].value.bytes[(p + 1) & o->mask];
    return 0;
}

static int
card24(unsigned int *u, const cff_data_t *o, unsigned p, unsigned pe)
{
    if (pe > o->length || p > pe - 3)
        return_error(gs_error_rangecheck); /* out of range access */
    *u = (o->blk_ref[ p      >> o->shift].value.bytes[ p      & o->mask]) << 16 |
         (o->blk_ref[(p + 1) >> o->shift].value.bytes[(p + 1) & o->mask]) << 8  |
          o->blk_ref[(p + 2) >> o->shift].value.bytes[(p + 2) & o->mask];
    return 0;
}
static int
card32(unsigned int *u, const cff_data_t *o, unsigned p, unsigned pe)
{
    if (pe > o->length || p > pe - 4)
        return_error(gs_error_rangecheck); /* out of range access */
    *u = (o->blk_ref[ p      >> o->shift].value.bytes[ p      & o->mask]) << 24 |
         (o->blk_ref[(p + 1) >> o->shift].value.bytes[(p + 1) & o->mask]) << 16 |
         (o->blk_ref[(p + 2) >> o->shift].value.bytes[(p + 2) & o->mask]) << 8  |
          o->blk_ref[(p + 3) >> o->shift].value.bytes[(p + 3) & o->mask];
    return 0;
}

static int (* const offset_procs[])(unsigned int *, const cff_data_t *, unsigned, unsigned) = {
    0, card8, card16, card24, card32
};

static int
get_cff_string(unsigned char *dst, const cff_data_t *o, unsigned p, unsigned len)
{
    if (p + len > o->length)
        return_error(gs_error_rangecheck); /* out of range access */
    while (len) {
        unsigned chunk_len = o->mask + 1 - (p & o->mask);
        unsigned char *pos = o->blk_ref[p >> o->shift].value.bytes + (p & o->mask);
        if (chunk_len > len)
            chunk_len = len;
        memcpy(dst, pos, chunk_len);
        dst += chunk_len;
        len -= chunk_len;
        p += chunk_len;
    }
    return 0;
}

static int
parse_index(cff_index_t *x, const cff_data_t *data, unsigned p, unsigned pe)
{  int code;

   if (p == 0) {
       /* Make an empty index when the offset to the index is not defined. */
       memset(x, 0, sizeof(*x));
       return 0;
   }
   x->start = p;
   if ((code = card16(&x->count, data, p, pe)) < 0)
       return code;
   if (x->count) {
       unsigned int eod;

       if ((code = card8(&x->offsize, data, p + 2, pe)) < 0)
           return code;
       if (x->offsize == 0) {  /* skip incorrect index, bug 689854 */
           x->count = 0;
           x->data = 0;
           x->end = p + 3;
       } else if( x->offsize > 4) {
           return_error(gs_error_rangecheck); /* Invalid offest size */
       } else {
           x->data = p + 2 + x->offsize*(x->count+1);
           code = (*offset_procs[x->offsize])(&eod, data, p + 3 + x->offsize*x->count, pe);
           if (code < 0)
               return code;
           x->end = x->data + eod;
       }
   } else {
       x->offsize = 0;
       x->data = 0;
       x->end = p + 2;
   }
   return 0;
}

static int
peek_index(unsigned *pp, unsigned int *len, const cff_index_t *x, const cff_data_t *data, unsigned int i)
{
    int code;
    unsigned int off1, off2;

    if (i >= x->count)
        return_error(gs_error_rangecheck); /* wrong index */
    if((code = (*offset_procs[x->offsize])(&off1, data, x->start + 3 + x->offsize*i, x->end)) < 0)
        return code;
    if((code = (*offset_procs[x->offsize])(&off2, data, x->start + 3 + x->offsize*(i + 1), x->end)) < 0)
        return code;
    if (off2 < off1)
        return_error(gs_error_rangecheck); /* Decreasing offsets */
    if (x->data + off2 > x->end)
        return_error(gs_error_rangecheck); /* Element exceeds index size */
    *len = off2 - off1;
    *pp  = x->data + off1;
    return 0;
}

static int
get_int(int *v,  const cff_data_t *data, unsigned p, unsigned pe)
{
    int code;
    unsigned int c, u;
    const int ext16 = ~0 << 15;  /* sign extension constant */
    const int ext32 = ~0 << 31;  /* sign extension constant */

    if ((code = card8(&c, data, p, pe)) < 0)
        return code;

    if (c == 28) {
        if (pe - p > 2) {
            if ((code = card16(&u, data, p + 1, pe)) < 0)
                return code;
        }
        else {
            if ((code = card8(&u, data, p + 1, pe)) < 0)
                return code;
        }
        *v = ((int)u + ext16) ^ ext16;
        return 3;
    }
    if (c == 29) {
        switch (pe - p) {
            case 2:
              if ((code = card8(&u, data, p + 1, pe)) < 0)
                 return code;
              break;
            case 3:
              if ((code = card16(&u, data, p + 1, pe)) < 0)
                 return code;
              break;
            case 4:
              if ((code = card24(&u, data, p + 1, pe)) < 0)
                 return code;
              break;
            default:
              if ((code = card32(&u, data, p + 1, pe)) < 0)
                 return code;
        }
        *v = ((int)u + ext32) ^ ext32;
        return 5;
    }
    if (c < 32)
        return_error(gs_error_rangecheck); /* out of range */
    if (c < 247) {
        *v = ((int)c - 139);
        return 1;
    }
    if (c < 251) {
        if ((code = card8(&u, data, p + 1, pe)) < 0)
            return code;
        *v = ((int)c - 247)*256 + (int)u + 108;
        return 2;
    }
    if (c < 255) {
        if ((code = card8(&u, data, p + 1, pe)) < 0)
            return code;
        *v = -((int)c - 251)*256 - (int)u - 108;
        return 2;
    }
    return_error(gs_error_rangecheck); /* out of range */
}

static int
get_float(ref *fnum, const cff_data_t *data, unsigned p, unsigned pe)
{
    int code;
    unsigned int c, i;
    char buf[80];
    const unsigned p0 = p;
    char *q = buf;

    for(;;) {
        unsigned int u;

        if ((code = card8(&u, data, p, pe)) < 0)
            return code;
        for (i=0; i<2 && q < buf + sizeof(buf) - 1; i++) {
            c = i ? u & 15 : u >> 4 ;
            switch(c) {
               case 0: case 1: case 2: case 3: case 4: /* 0 .. 9 */
               case 5: case 6: case 7: case 8: case 9:
                   *q++ = '0' + c;
                   break;
               case 10: /* . */
                   *q++ = '.';
                   break;
               case 11: /* E */
                   *q++ = 'e';
                   break;
               case 12: /* E- */
                   *q++ = 'e';
                   *q++ = '-';
                   break;
               case 13: /* invalid, skip */
                   break;
               case 14: /* - */
                   *q++ = '-';
                   break;
               case 15: { /* end */
                   int sign = 0;
                   char *eptr, *bptr = buf;

                   if (q > buf && buf[0] == '-'){
                       sign = -1;
                       bptr = &(buf[1]);
                   }

                   if (q > buf) {
                       code = scan_number ((const byte *)bptr, (const byte *)q, sign, fnum, (const byte **)&eptr, 0);
                   }
                   else {
                       code = 0;
                       make_int(fnum, 0);
                   }
                   if (code < 0) {
                       return(code);
                   }
                   else {
                       return p - p0 + 1;
                   }
                   break;
                }
            }
        }
        p++;
    }
}

static int
idict_put_c_name(i_ctx_t *i_ctx_p, ref *dst, const char *c_name, unsigned int len, const ref *src)
{
    int code;
    ref ps_name;

    if ((code = name_ref(imemory, (const unsigned char *)c_name, len, &ps_name, 0)) < 0)
      return code;
    return idict_put(dst, &ps_name, src);
}

static int
idict_undef_c_name(i_ctx_t *i_ctx_p, ref *dst, const char *c_name, unsigned int len)
{
    int code;
    ref ps_name;

    if ((code = name_ref(imemory, (const unsigned char *)c_name, len, &ps_name, 0)) < 0)
      return code;
    code = idict_undef(dst, &ps_name);
    if (code < 0 && code != gs_error_undefined) /* ignore undefined error */
        return code;
    return 0;
}

static int
idict_move_c_name(i_ctx_t *i_ctx_p, ref *dst, ref *src, const char *c_name, unsigned int len)
{
    int code;
    ref ps_name, *pvalue;

    if ((code = name_ref(imemory, (const unsigned char *)c_name, len, &ps_name, 0)) < 0)
      return code;
    if (dict_find(src, &ps_name, &pvalue) > 0) {
        if ((code = idict_put(dst, &ps_name, pvalue)) < 0)
            return code;
        if ((code = idict_undef(src, &ps_name)) < 0)
            return code;
    }
    return 0;
}

static int
make_string_from_index(i_ctx_t *i_ctx_p, ref *dst, const cff_index_t *index, const cff_data_t *data, unsigned int id, int fd_num)
{
    int code;
    unsigned int len, doff;
    byte *sbody;
    int fdoff = fd_num >= 0;

    if ((code = peek_index(&doff, &len, index, data, id)) < 0)
        return code;
    if (len + fdoff> 65535)
        return_error(gs_error_limitcheck);
    if ((sbody = ialloc_string(len + fdoff, "make_string_from_index")) == 0)
        return_error(gs_error_VMerror);
    make_string(dst, icurrent_space | a_readonly, len + fdoff, sbody);
    if ((code = get_cff_string(sbody + fdoff, data, doff, len)) < 0)
        return code;
    if (fdoff)
        sbody[0] = fd_num;
    return 0;
}

static int
make_string_from_sid(i_ctx_t *i_ctx_p, ref *dst, const cff_index_t *strings, const cff_data_t *data, unsigned int sid)
{
    if (sid < count_of(standard_strings)) {
        make_string(dst, avm_foreign | a_readonly,
          strlen(standard_strings[sid]), (unsigned char *)standard_strings[sid]);  /* break const */
        return 0;
    } else
        return make_string_from_index(i_ctx_p, dst, strings, data, sid - count_of(standard_strings), -1);
}

static int
make_name_from_sid(i_ctx_t *i_ctx_p, ref *dst, const cff_index_t *strings, const cff_data_t *data, unsigned int sid)
{
    if (sid < count_of(standard_strings)) {
        return name_ref(imemory, (const unsigned char *)standard_strings[sid],
                                         strlen(standard_strings[sid]), dst, 0);
    } else {
        int code;
        unsigned int off, len;
        byte buf[200];

        if ((code = peek_index(&off, &len, strings, data, sid - count_of(standard_strings))) < 0)
            return code;
        if (len > sizeof(buf))
            return_error(gs_error_limitcheck);
        if ((code = get_cff_string(buf, data, off, len)) < 0)
            return code;
        return name_ref(imemory, buf, len, dst, 1);
    }
}

static int
make_stringarray_from_index(i_ctx_t *i_ctx_p, ref *dst, const cff_index_t *index, const cff_data_t *data)
{
    int code;
    unsigned int i;

    if ((code = ialloc_ref_array(dst, a_readonly, index->count, "make_stringarray_from_index")) < 0)
        return code;
    for (i = 0; i < index->count; i++) {
        unsigned int subr, len;

        if ((code = peek_index(&subr, &len, index, data, i)) < 0)
            return code;
        if ((code = make_string_from_index(i_ctx_p, dst->value.refs + i, index, data, i, -1)) < 0)
            return code;
    }
    return 0;
}

static void
undelta(ref *ops, unsigned int cnt)
{
    unsigned int i;

    for (i = 0; i < cnt; i++) {
        if (!r_has_type(&ops[i], t_real)) {
            /* Strictly speaking assigning one element of union
             * to another, overlapping element of a different size is
             * undefined behavior, hence assign to an intermediate variable
             */
            float fl = (float)ops[i].value.intval;
            make_real(&ops[i], fl);
        }
    }
    for (i = 1; i < cnt; i++) {
        make_real(&ops[i], ops[i].value.realval + ops[i - 1].value.realval);
    }
}

static int
iso_adobe_charset_proc(const cff_data_t *data, unsigned p, unsigned pe, unsigned i)
{
    if (i < 228)
        return i + 1;
    else
        return_error(gs_error_rangecheck);
}

static int
expert_charset_proc(const cff_data_t *data, unsigned p, unsigned pe, unsigned i)
{
    if (i < sizeof(expert_charset)/sizeof(*expert_charset))
        return expert_charset[i];
    else
        return_error(gs_error_rangecheck);
}

static int
expert_subset_charset_proc(const cff_data_t *data, unsigned p, unsigned pe, unsigned int i)
{
    if (i < sizeof(expert_subset_charset)/sizeof(*expert_subset_charset))
        return expert_subset_charset[i];
    else
        return_error(gs_error_rangecheck);
}

static int
format0_charset_proc(const cff_data_t *data, unsigned p, unsigned pe, unsigned int i)
{
    int code;
    unsigned u;

    if ((code = card16(&u, data, p + 2*i, pe)) < 0)
        return code;
    return (int)u;
}

static int
format1_charset_proc(const cff_data_t *data, unsigned p, unsigned pe, unsigned int i)
{
    int code;
    unsigned int cid = 0;

    while( p < pe - 3) {
        unsigned int first, count;

        if ((code = card16(&first, data, p, pe)) < 0)
            return code;
        if ((code = card8(&count, data, p + 2, pe)) < 0)
            return code;
        ++count;
        if (i < cid + count)
            return first + i - cid;
        p += 3;
        cid += count;
    }
    return_error(gs_error_rangecheck);
}

static int
format2_charset_proc(const cff_data_t *data, unsigned p, unsigned pe, unsigned int i)
{
    int code;
    unsigned int cid = 0;

    while( p < pe - 4) {
        unsigned int first, count;

        if ((code = card16(&first, data, p, pe)) < 0)
            return code;
        if ((code = card16(&count, data, p + 2, pe)) < 0)
            return code;
        ++count;

        if (i < cid + count)
            return first + i - cid;
        p += 4;
        cid += count;
    }
    return_error(gs_error_rangecheck);
}

static int
format0_fdselect_proc(const cff_data_t *data, unsigned p, unsigned pe, unsigned int i)
{
    int code;
    unsigned u;

    if ((code = card8(&u, data, p + i, pe)) < 0)
        return code;
    return (int)u;
}

static int
format3_fdselect_proc(const cff_data_t *data, unsigned p, unsigned pe, unsigned int i)
{
    int code;
    unsigned int u, n_ranges;

    if ((code = card16(&n_ranges, data, p, pe)) < 0)
        return code;
    p += 2;

    while (n_ranges-- &&  p + 5 <= pe) {
        unsigned int first, last;

        if ((code = card16(&first, data, p, pe)) < 0)
            return code;
        if ((code = card16(&last, data, p + 3, pe)) < 0)
            return code;
        if (i >= first && i < last) {
            if ((code = card8(&u, data, p + 2, pe)) < 0)
                return code;
            return (int)u;
        }
        p += 3;
    }
    return_error(gs_error_rangecheck);
}

static int
find_font_dict(i_ctx_t *i_ctx_p, ref *topdict, ref **fontdict, const char *key)
{
    int code;
    ref val;

    if (*fontdict)
        return 0;
    if (dict_find_string(topdict, key, fontdict) > 0)
        return 0;
    if ((code = dict_create(8,  &val)) < 0)
        return code;
    if ((code = idict_put_c_name(i_ctx_p, topdict, key, strlen(key), &val)) < 0)
        return code;
    if ((code = dict_find_string(topdict, key, fontdict)) == 0)
        return_error(gs_error_undefined); /* can't happen */
    return code;
}

#define DEFINE_KEYS(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t,u,v,w,x,y,z, \
                    A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P,Q,R,S,T,U,V) \
static const char * const font_keys[] = { \
  #a,#b,#c,#d,#e,#f,#g,#h,#i,#j,#k,#l,#m,#n,#o,#p,#q,#r,#s,#t,#u,#v,#w,#x,#y,#z, \
  #A,#B,#C,#D,#E,#F,#G,#H,#I,#J,#K,#L,#M,#N,#O,#P,#Q,#R,#S,#T,#U,#V \
}; \
static const short font_keys_sz[] = { \
  sizeof(#a)-1, sizeof(#b)-1, sizeof(#c)-1, sizeof(#d)-1, sizeof(#e)-1, \
  sizeof(#f)-1, sizeof(#g)-1, sizeof(#h)-1, sizeof(#i)-1, sizeof(#j)-1, \
  sizeof(#k)-1, sizeof(#l)-1, sizeof(#m)-1, sizeof(#n)-1, sizeof(#o)-1, \
  sizeof(#p)-1, sizeof(#q)-1, sizeof(#r)-1, sizeof(#s)-1, sizeof(#t)-1, \
  sizeof(#u)-1, sizeof(#v)-1, sizeof(#w)-1, sizeof(#x)-1, sizeof(#y)-1, \
  sizeof(#z)-1, sizeof(#A)-1, sizeof(#B)-1, sizeof(#C)-1, sizeof(#D)-1, \
  sizeof(#E)-1, sizeof(#F)-1, sizeof(#G)-1, sizeof(#H)-1, sizeof(#I)-1, \
  sizeof(#J)-1, sizeof(#K)-1, sizeof(#L)-1, sizeof(#M)-1, sizeof(#N)-1, \
  sizeof(#O)-1, sizeof(#P)-1, sizeof(#Q)-1, sizeof(#R)-1, sizeof(#S)-1, \
  sizeof(#T)-1, sizeof(#U)-1, sizeof(#V)-1 \
}; \
typedef enum { \
  k_##a = 0, k_##b, k_##c, k_##d, k_##e, k_##f, k_##g, k_##h, k_##i, k_##j, k_##k, k_##l, k_##m, \
  k_##n,     k_##o, k_##p, k_##q, k_##r, k_##s, k_##t, k_##u, k_##v, k_##w, k_##x, k_##y, k_##z, \
  k_##A,     k_##B, k_##C, k_##D, k_##E, k_##F, k_##G, k_##H, k_##I, k_##J, k_##K, k_##L, k_##M, \
  k_##N,     k_##O, k_##P, k_##Q, k_##R, k_##S, k_##T, k_##U, k_##V\
} font_keys_dummy_t;

DEFINE_KEYS(
   version,
   Notice,
   FullName,
   FamilyName,
   Weight,
   FontBBox,
   BlueValues,
   OtherBlues,
   FamilyBlues,
   FamilyOtherBlues,
   StdHW,
   StdVW,
   Copyright,
   isFixedPitch,
   ItalicAngle,
   UnderlinePosition,
   UnderlineThickness,
   PaintType,
   CharstringType,
   FontMatrix,
   StrokeWidth,
   BlueScale,
   BlueShift,
   BlueFuzz,
   StemSnapH,
   StemSnapV,
   ForceBold,
   LanguageGroup,
   ExpansionFactor,
   initialRandomSeed,
   SyntheticBase,
   PostScript,
   BaseFontName,
   BaseFontBlend,
   CIDFontVersion,
   CIDFontRevision,
   CIDFontType,
   CIDCount,
   UIDBase,
   FDArray,
   FDSelect,
   FontName,
   UniqueID,
   XUID,
   defaultWidthX,
   nominalWidthX,
   FontType,
   Private
)
#undef DEFINE_KEYS

typedef enum {
    k_topdict = 0, k_fontinfodict = 0x10000, k_privatedict = 0x20000, k_sid = 0x40000,
    k_array = 0x80000, k_delta = 0x100000, k_int = 0x200000, k_bool=0x400000
} font_flags_dummy_t;

#define CONTROL(id, n_op, flags) ((flags) | (n_op) | ((k_##id) << 8))

typedef enum {
  k_0, k_1, k_2, k_7, k_50, k_neg_100, k_8720, k_0_039625, k_0_06, k_false, k_box,
  k_matrix, k_matrix_1, k_emptydict
} font_defaults_dummy_t;

typedef struct tag_font_defaults {
    unsigned short key;
    unsigned short value;
} font_defaults_t;

static const font_defaults_t fontinfo_font_defaults[] = {
    { k_isFixedPitch,       k_false },
    { k_ItalicAngle,        k_0 },
    { k_UnderlinePosition,  k_neg_100 },
    { k_UnderlineThickness, k_50 }
};

static const font_defaults_t simple_font_defaults[] = {
    { k_PaintType,          k_0 },
    { k_CharstringType,     k_2 },
    { k_FontMatrix,         k_matrix }, /*  0.001 0 0 0.001 0 0 */
    { k_FontBBox,           k_box },    /*  0 0 0 0 */
    { k_StrokeWidth,        k_0 }
};

static const font_defaults_t cid_font_defaults[] = {
    { k_CIDFontVersion,     k_0 },
    { k_CIDFontRevision,    k_0 },
    { k_CIDFontType,        k_0 },
    { k_CIDCount,           k_8720 }
};

static const font_defaults_t fd_font_defaults[] = {
    { k_FontMatrix,         k_matrix },  /* 0.001 0 0 0.001 0 0 */
    { k_PaintType,          k_0      },  /* gs needs this */
    { k_FontType,           k_2      },  /* gs needs this */
    { k_Private,            k_emptydict} /* following gs implementation */
};

static const font_defaults_t set_unit_matrix[] = {
    { k_FontMatrix,         k_matrix_1 }   /* 1 0 0 1 0 0 */
};

static const font_defaults_t private_font_defaults[] = {
    { k_BlueScale,          k_0_039625 },
    { k_BlueShift,          k_7 },
    { k_BlueFuzz,           k_1 },
    { k_ForceBold,          k_false },
    { k_LanguageGroup,      k_0 },
    { k_ExpansionFactor,    k_0_06 },
    { k_initialRandomSeed,  k_0 },
    { k_defaultWidthX,      k_0 },
    { k_nominalWidthX,      k_0 }
};

static int
set_defaults(i_ctx_t *i_ctx_p, ref *dest, const font_defaults_t *def, int count)
{
    int i;

    for (i = 0; i < count; i++) {
        ref name, *dummy, value;
        int code;
        float xx;

        if ((code = name_ref(imemory, (const unsigned char *)font_keys[def[i].key],
                                             font_keys_sz[def[i].key], &name, 0)) < 0)
            return code;
        if (dict_find(dest, &name, &dummy) <= 0) {
           switch (def[i].value) {
               default:
               case k_0:
                   make_int(&value, 0);
                   break;
               case k_1:
                   make_int(&value, 1);
                   break;
               case k_2:
                   make_int(&value, 2);
                   break;
               case k_7:
                   make_int(&value, 7);
                   break;
               case k_50:
                   make_int(&value, 50);
                   break;
               case k_neg_100:
                   make_int(&value, -100);
                   break;
               case k_8720:
                   make_int(&value, 8720);
                   break;
               case k_0_039625:
                   make_real(&value, (float)0.039625);
                   break;
               case k_0_06:
                   make_real(&value, (float)0.06);
                   break;
               case k_false:
                   make_bool(&value, 0);
                   break;
               case k_box:
                   if ((code = ialloc_ref_array(&value, a_readonly, 4, "parsecff.default_bbox")) < 0)
                        return code;
                   make_int(&value.value.refs[0], 0);
                   value.value.refs[1] = value.value.refs[2] = value.value.refs[3] = value.value.refs[0];
                   break;
               case k_matrix:
                   xx = (float)0.001;
                   goto make_matrix;
               case k_matrix_1:
                   xx = (float)1.;
                 make_matrix:;
                   if ((code = ialloc_ref_array(&value, a_readonly, 6, "parsecff.default_bbox")) < 0)
                        return code;
                   make_real(&value.value.refs[0], xx);
                   value.value.refs[3] = value.value.refs[0];
                   make_real(&value.value.refs[1], (float)0.);
                   value.value.refs[2] = value.value.refs[4] = value.value.refs[5] = value.value.refs[1];
                   break;
               case k_emptydict:
                   if ((code = dict_create(0, &value)) < 0)
                       return code;
           }
           if ((code = idict_put(dest, &name, &value)) < 0)
               return code;
        }
    }
    return 0;
}

typedef struct tag_font_offsets_t {
    unsigned int fdarray_off;
    unsigned int fdselect_off;
    unsigned int charset_off;
    unsigned int encoding_off;
    unsigned int private_off;
    unsigned int charstrings_off;
    unsigned int local_subrs_off;
    unsigned int private_size;
    bool have_ros;
} font_offsets_t;

static int
parse_dict(i_ctx_t *i_ctx_p,  ref *topdict, font_offsets_t *offsets,
  const cff_index_t *strings, const cff_data_t *data,
  const unsigned p0, const unsigned pe)
{
    ref *fontinfodict = 0, *privatedict = 0, arg, ops[MAXOP];
    int op_i = 0;

    unsigned c;
    unsigned p = p0;
    unsigned control;
    int code;

    while(p < pe) {
        if ((code = card8(&c, data, p++, pe)) < 0)
            return code;
        switch(c) {
            case 0: /* version 0      SID -, FontInfo */
                control = CONTROL(version, 1, k_fontinfodict | k_sid);
                break;
            case 1: /* Notice 1       SID -, FontInfo */
                control = CONTROL(Notice, 1, k_fontinfodict | k_sid);
                break;
            case 2: /* FullName 2     SID -, FontInfo */
                control = CONTROL(FullName, 1, k_fontinfodict | k_sid);
                break;
            case 3: /* FamilyName 3   SID -, FontInfo */
                control = CONTROL(FamilyName, 1, k_fontinfodict | k_sid);
                break;
            case 4: /* Weight 4       SID -, FontInfo */
                control = CONTROL(FamilyName, 1, k_fontinfodict | k_sid);
                break;
            case 5: /* FontBBox 5 array 0 0 0 0 */
                control = CONTROL(FontBBox, 4, k_topdict | k_array);
                break;
            case 6: /* BlueValues 6 delta -, Private */
                control = CONTROL(BlueValues, 0, k_privatedict | k_array | k_delta);
                break;
            case 7: /* OtherBlues 7 delta -, Private */
                control = CONTROL(OtherBlues, 0, k_privatedict | k_array | k_delta);
                break;
            case 8: /* FamilyBlues 8 delta -, Private */
                control = CONTROL(FamilyBlues, 0, k_privatedict | k_array | k_delta);
                break;
            case 9: /* FamilyOtherBlues 9 delta -, Private */
                control = CONTROL(FamilyOtherBlues, 0, k_privatedict | k_array | k_delta);
                break;
            case 10: /* StdHW 10 number -, Private */ /* converts to array */
                control = CONTROL(StdHW, 1, k_privatedict | k_array);
                break;
            case 11: /* StdVW 11 number -, Private */ /* converts to array */
                control = CONTROL(StdVW, 1, k_privatedict | k_array);
                break;
            case 12:
                if (p >= pe)
                    return_error(gs_error_rangecheck);
                if ((code = card8(&c, data, p++, pe)) < 0)
                    return code;
                switch(c) {
                    case 0: /* Copyright    12 0 SID -, FontInfo */
                        control = CONTROL(Copyright, 1, k_fontinfodict | k_sid);
                        break;
                    case 1: /* isFixedPitch 12 1 boolean 0 (false), FontInfo */
                        control = CONTROL(isFixedPitch, 1, k_fontinfodict | k_int | k_bool);
                        break;
                    case 2: /* ItalicAngle  12 2 number 0, FontInfo */
                        control = CONTROL(ItalicAngle, 1, k_fontinfodict);
                        break;
                    case 3: /* UnderlinePosition 12 3 number -100, FontInfo */
                        control = CONTROL(UnderlinePosition, 1, k_fontinfodict);
                        break;
                    case 4: /* UnderlineThickness 12 4 number 50, FontInfo */
                        control = CONTROL(UnderlineThickness, 1, k_fontinfodict);
                        break;
                    case 5: /* PaintType    12 5 number 0 */
                        control = CONTROL(PaintType, 1, k_fontinfodict);
                        break;
                    case 6: /* CharstringType 12 6 number 2 */
                        control = CONTROL(CharstringType, 1, k_fontinfodict | k_int);
                        break;
                    case 7: /* FontMatrix   12 7 array 0.001 0 0 0.001 0 0 */
                        control = CONTROL(FontMatrix, 6, k_topdict | k_array);
                        break;
                    case 8: /* StrokeWidth  12 8 number 0 */
                        control = CONTROL(StrokeWidth, 1, k_topdict);
                        break;
                    case 9: /* BlueScale 12 9 number 0.039625, Private */
                        control = CONTROL(BlueScale, 1, k_privatedict);
                        break;
                    case 10: /* BlueShift 12 10 number 7, Private */
                        control = CONTROL(BlueShift, 1, k_privatedict);
                        break;
                    case 11: /* BlueFuzz 12 11 number 1, Private */
                        control = CONTROL(BlueFuzz, 1, k_privatedict);
                        break;
                    case 12: /* StemSnapH 12 12 delta -, Private */
                        control = CONTROL(StemSnapH, 0, k_privatedict | k_array | k_delta);
                        break;
                    case 13: /* StemSnapV 12 13 delta -, Private */
                        control = CONTROL(StemSnapV, 0, k_privatedict | k_array | k_delta);
                        break;
                    case 14: /* ForceBold 12 14 boolean false, Private */
                        control = CONTROL(ForceBold, 1, k_privatedict | k_int | k_bool);
                        break;
                    case 17: /* LanguageGroup 12 17 number 0, Private */
                        control = CONTROL(LanguageGroup, 1, k_privatedict | k_int);
                        break;
                    case 18: /* ExpansionFactor 12 18 number 0.06, Private */
                        control = CONTROL(ExpansionFactor, 1, k_privatedict);
                        break;
                    case 19: /* initialRandomSeed 12 19 number 0, Private */
                        control = CONTROL(initialRandomSeed, 1, k_privatedict | k_int);
                        break;
                    case 20: /* SyntheticBase 12 20 number -, synthetic base font index */
                        control = CONTROL(SyntheticBase, 1, k_privatedict | k_int);
                        break;
                    case 21: /* PostScript 12 21 SID -, embedded PostScript language code */
                        control = CONTROL(PostScript, 1, k_topdict | k_int | k_sid);
                        break;
                    case 22: /* BaseFontName 12 22 SID -, (added as needed by Adobe-based technology) */
                        control = CONTROL(BaseFontName, 1, k_topdict | k_int | k_sid);
                        break;
                    case 23: /* BaseFontBlend 12 23 delta -, (added as needed by Adobe-based technology) */
                        control = CONTROL(BaseFontBlend, 0, k_topdict | k_array | k_delta);
                        break;

                    /* CIDFont */
                    case 30: {/* ROS 12 30 SID SID number -, Registry Ordering Supplement */
                        ref ros;

                        if ((op_i -= 3) < 0)
                            return_error(gs_error_stackunderflow);
                        if ((code = dict_create(3, &ros)) < 0)
                            return code;
                        check_type(ops[op_i], t_integer);
                        if ((code = make_string_from_sid(i_ctx_p, &arg, strings, data, ops[op_i].value.intval)) < 0)
                            return code;
                        if ((code = idict_put_c_name(i_ctx_p, &ros, STR2MEM("Registry"), &arg)) < 0)
                            return code;
                        if ((code = make_string_from_sid(i_ctx_p, &arg, strings, data, ops[op_i + 1].value.intval)) < 0)
                            return code;
                        if ((code = idict_put_c_name(i_ctx_p, &ros, STR2MEM("Ordering"), &arg)) < 0)
                            return code;
                        if ((code = idict_put_c_name(i_ctx_p, &ros, STR2MEM("Supplement"), &ops[op_i + 2])) < 0)
                            return code;
                        if ((code = idict_put_c_name(i_ctx_p, topdict, STR2MEM("CIDSystemInfo"), &ros)) < 0)
                            return code;
                        offsets->have_ros = true;
                        }
                        continue;
                    case 31: /* CIDFontVersion 12 31 number 0 */
                        control = CONTROL(CIDFontVersion, 1, k_topdict);
                        break;
                    case 32: /* CIDFontRevision 12 32 number 0 */
                        control = CONTROL(CIDFontRevision, 1, k_topdict);
                        break;
                    case 33: /* CIDFontType 12 33 number 0 */
                        control = CONTROL(CIDFontType, 1, k_topdict | k_int);
                        break;
                    case 34: /* CIDCount 12 34 number 8720 */
                        control = CONTROL(CIDCount, 1, k_topdict | k_int);
                        break;
                    case 35: /* UIDBase 12 35 number - */
                        control = CONTROL(UIDBase, 1, k_topdict | k_int);
                        break;
                    case 36: /* FDArray 12 36 number -, Font DICT (FD) INDEX offset (0) */
                        control = 0;
                        if (--op_i < 0)
                            return_error(gs_error_stackunderflow);
                        check_type(ops[op_i], t_integer);
                        offsets->fdarray_off = ops[op_i].value.intval;
                        continue;
                    case 37: /* FDSelect 12 37 number -, FDSelect offset (0) */
                        control = 0;
                        if (--op_i < 0)
                            return_error(gs_error_stackunderflow);
                        check_type(ops[op_i], t_integer);
                        offsets->fdselect_off = ops[op_i].value.intval;
                        continue;
                    case 38: /* FontName 12 38 SID -, FD FontName */
                        control = CONTROL(FontName, 1, k_topdict | k_int | k_sid);
                        break;
                    default:
                        continue;
                }
                break;
            case 13: /* UniqueID 13 number - */
                control = CONTROL(UniqueID, 1, k_topdict | k_int);
                break;
            case 14: /* XUID     14 array - */
                control = CONTROL(XUID, 0, k_topdict | k_array);
                break;
            case 15: /* charset  15 number 0, charset offset (0) */
                if (--op_i < 0)
                    return_error(gs_error_stackunderflow);
                check_type(ops[op_i], t_integer);
                offsets->charset_off = ops[op_i].value.intval;
                continue;
            case 16: /* Encoding 16 number 0, encoding offset (0) */
                if (--op_i < 0)
                    return_error(gs_error_stackunderflow);
                check_type(ops[op_i], t_integer);
                offsets->encoding_off = ops[op_i].value.intval;
                continue;
            case 17: /* CharStrings 17 number -, CharStrings offset (0) */
                if (--op_i < 0)
                    return_error(gs_error_stackunderflow);
                check_type(ops[op_i], t_integer);
                offsets->charstrings_off = ops[op_i].value.intval;
                continue;
            case 18: /* Private 18 number, number -, Private DICT size and offset (0)*/
                if ((op_i -= 2) < 0)
                    return_error(gs_error_stackunderflow);
                check_type(ops[op_i], t_integer);
                check_type(ops[op_i + 1], t_integer);
                offsets->private_size = ops[op_i].value.intval;
                offsets->private_off  = ops[op_i + 1].value.intval;
                continue;
            case 19: /* Local Subrs 19 number -, Offset (self) to local subrs */
                if (--op_i < 0)
                    return_error(gs_error_stackunderflow);
                check_type(ops[op_i], t_integer);
                offsets->local_subrs_off = ops[op_i].value.intval;
                continue;
            case 20: /* defaultWidthX 20 number 0 */
                control = CONTROL(defaultWidthX, 1, k_privatedict);
                break;
            case 21: /* nominalWidthX 21 number 0 */
                control = CONTROL(nominalWidthX, 1, k_privatedict);
                break;
            case 22: case 23: case 24: case 25: case 26: case 27: /* reserved */
                continue;
            case 30: { /* float operand */
                if (op_i >= MAXOP)
                    return_error(gs_error_limitcheck);

                if ((code = get_float(&ops[op_i], data, p, pe)) < 0)
                    return code;

                op_i++;
                p += code;
                }
                continue;
            case 31: case 255: /* reserved */
                continue;
            default:
                {  int n;  /* int operand */

                   if (op_i >= MAXOP)
                       return_error(gs_error_limitcheck);
                   if ((code = get_int(&n, data, p - 1, pe)) < 0)
                       return code;
                   make_int(&ops[op_i], n);
                   op_i++;
                   p += code - 1;
                }
                continue;
        }
        {
            unsigned int n_op = control & 255;
            unsigned int string_id =  (control >> 8) & 255;
            ref arg, *dict;

            if (n_op == 0)
                n_op = op_i;
            else if (op_i < n_op)
                return_error(gs_error_stackunderflow);
            if (control & k_delta)
                undelta(ops + op_i - n_op, n_op);
            op_i -= n_op;
            if (control & (k_sid | k_int | k_bool)) {
                if (!r_has_type(&ops[op_i], t_integer))
                    return_error(gs_error_typecheck);
            }
            if (control & k_bool)
                make_bool(&arg, !!ops[op_i].value.intval);
            if (control & k_sid) {
                if ((code = make_string_from_sid(i_ctx_p, &arg, strings, data, ops[op_i].value.intval)) < 0)
                    return code;
            }
            if (control & k_array) {
                if ((code = ialloc_ref_array(&arg, a_readonly, n_op, "parsecff.array")) < 0)
                    return code;
                memcpy(arg.value.refs, ops + op_i, n_op*sizeof(ref));
            }
            if (control & k_privatedict) {
                if ((code = find_font_dict(i_ctx_p, topdict, &privatedict, "Private")) < 0)
                    return code;
                dict = privatedict;
            } else if (control & k_fontinfodict) {
                if ((code = find_font_dict(i_ctx_p, topdict, &fontinfodict, "FontInfo")) < 0)
                    return code;
                dict = fontinfodict;
            } else
                dict = topdict;
            code = idict_put_c_name(i_ctx_p, dict, font_keys[string_id], font_keys_sz[string_id],
              ((control & (k_bool | k_sid | k_array)) == 0 ? &ops[op_i] : &arg));
            if (code < 0)
                return code;
        }
    }
    return 0;
}

static int
parse_font(i_ctx_t *i_ctx_p,  ref *topdict,
  const cff_index_t *strings, const ref *global_subrs,
  const cff_data_t *data,
  const unsigned p_top, const unsigned pe_top,
  const unsigned p_all, const unsigned pe_all,
  const bool force_cidfont)
{
    int code = 0;
    ref *privatedict = 0, *fontinfodict = 0;
    cff_index_t local_subrs, charstrings_index;
    int (*peek_charset_proc)(const cff_data_t *, unsigned p, unsigned pe, unsigned i);
    font_offsets_t offsets;

    memset(&offsets, 0, sizeof(offsets)); /* set defaults for charset_off, encoding_off */

    if ((code = parse_dict(i_ctx_p, topdict, &offsets, strings, data, p_top, pe_top)) < 0)
        return code;

    /* if the values for the /Private dict are nonsensical, ignore it
     * and hope the font doesn't actually need it!
     */
    if ((offsets.private_off + offsets.private_size) <= pe_all) {
        if ((code = parse_dict(i_ctx_p, topdict, &offsets, strings, data,
                      p_all + offsets.private_off,
                      p_all + offsets.private_off + offsets.private_size)) < 0)
            return code;
    }
    if ((code = parse_index(&local_subrs, data,
               (offsets.local_subrs_off ? p_all + offsets.private_off + offsets.local_subrs_off : 0),
                pe_all)) < 0)
        return code;

    if ((code = find_font_dict(i_ctx_p, topdict, &privatedict, "Private")) < 0)
        return code;
    if ((code = find_font_dict(i_ctx_p, topdict, &fontinfodict, "FontInfo")) < 0)
        return code;
    if (local_subrs.count) {
        ref r;

        if (( code = make_stringarray_from_index(i_ctx_p, &r, &local_subrs, data)) < 0)
            return code;
        if ((code = idict_put_c_name(i_ctx_p, privatedict, STR2MEM("Subrs"), &r)) < 0)
            return code;
    }
    if (global_subrs && !offsets.have_ros) {
        if ((code = idict_put_c_name(i_ctx_p, privatedict, STR2MEM("GlobalSubrs"), global_subrs)) < 0)
            return code;
    }
    if (offsets.charstrings_off <= 0)
        return_error(gs_error_invalidfont);
    if ((code = parse_index(&charstrings_index, data, p_all + offsets.charstrings_off, pe_all)) < 0)
        return code;

    switch (offsets.charset_off) {
        case 0:
            peek_charset_proc = iso_adobe_charset_proc;
            break;
        case 1:
            peek_charset_proc = expert_charset_proc;
            break;
        case 2:
            peek_charset_proc = expert_subset_charset_proc;
            break;
        default: {
            unsigned u;

            if ((code = card8(&u, data, p_all + offsets.charset_off, pe_all)) < 0)
                return code;
            switch (u) {
                case 0:
                    peek_charset_proc = format0_charset_proc;
                    break;
                case 1:
                    peek_charset_proc = format1_charset_proc;
                    break;
                case 2:
                    peek_charset_proc = format2_charset_proc;
                    break;
                default:
                    return_error(gs_error_rangecheck);
            }
        }
    }

    if (offsets.have_ros) {  /* CIDFont */
        unsigned int i;
        cff_index_t fdarray;
        ref int_ref, cstr_ref, fdarray_ref, glyph_directory_ref;
        unsigned int fdselect_off = offsets.fdselect_off;
        unsigned int fdselect_code;
        int (*fdselect_proc)(const cff_data_t *, unsigned p, unsigned pe, unsigned int i);
        ref *array_FontMatrix, name_FontMatrix;
        bool top_has_FontMatrix;

        if (offsets.fdarray_off == 0 || fdselect_off == 0)
            return_error(gs_error_invalidfont);
        if ((code = parse_index(&fdarray, data, p_all + offsets.fdarray_off,  pe_all)) < 0)
            return code;
        if ((code = ialloc_ref_array(&fdarray_ref, a_readonly, fdarray.count, "parsecff.fdarray")) < 0)
            return code;
        if ((code = idict_put_c_name(i_ctx_p, topdict, STR2MEM("FDArray"), &fdarray_ref)) < 0)
            return code;
        if ((code = dict_create(charstrings_index.count + 1, &glyph_directory_ref)) < 0)
            return code;
        if ((code = idict_put_c_name(i_ctx_p, topdict, STR2MEM("GlyphDirectory"), &glyph_directory_ref)) < 0)
            return code;
        if ((code = name_ref(imemory, (const unsigned char *)font_keys[k_FontMatrix], font_keys_sz[k_FontMatrix], &name_FontMatrix, 0)) < 0)
            return code;
        top_has_FontMatrix = dict_find(topdict, &name_FontMatrix, &array_FontMatrix) > 0;

        for (i = 0; i < fdarray.count; i++) {
            unsigned int len;
            unsigned doff;
            cff_index_t fd_subrs;
            ref *fdprivate = 0;
            ref *fdfont = &fdarray_ref.value.refs[i];

            offsets.local_subrs_off = offsets.private_off = 0;
            if ((code = peek_index(&doff, &len, &fdarray, data, i)) < 0)
                return code;
            if ((code = dict_create(5, fdfont)) < 0)
                return code;
            if ((code = parse_dict(i_ctx_p, fdfont, &offsets, strings, data, doff, doff + len)) < 0)
                return code;
            if ((code = parse_dict(i_ctx_p, fdfont, &offsets, strings, data,
              p_all + offsets.private_off, p_all + offsets.private_off + offsets.private_size)) < 0)
                return code;
            if ((code = find_font_dict(i_ctx_p, fdfont, &fdprivate, "Private")) < 0)
                return code;
            if (offsets.local_subrs_off) {
                if ((code = parse_index(&fd_subrs, data,
                  p_all + offsets.private_off + offsets.local_subrs_off, pe_all)) < 0)
                    return code;
                if (fd_subrs.count) {
                    ref r;

                    if (( code = make_stringarray_from_index(i_ctx_p, &r, &fd_subrs, data)) < 0)
                        return code;
                    if ((code = idict_put_c_name(i_ctx_p, fdprivate, STR2MEM("Subrs"), &r)) < 0)
                        return code;
                }
            }
            if (global_subrs) {
                if ((code = idict_put_c_name(i_ctx_p, fdprivate, STR2MEM("GlobalSubrs"), global_subrs)) < 0)
                    return code;
            }
           /* There is no explicit description what to do with FontMatrix of CFF CIDFont in
            * Adobe tech note #5176 (CFF Spec), Ken and Masaki have figured out this is what
            * it should be:
            *
            * 1) If both Top DICT and Font DICT does _not_ have FontMatrix, then Top DICT =
            * [0.001 0 0 0.001 0 0], Font DICT= [1 0 0 1 0 0].  (Or, Top DICT = (absent),
            * Font DICT = [0.001 0 0 0.001 0 0] then let '/CIDFont defineresource'
            * make Top DICT = [0.001 0 0 0.001 0 0], Font DICT = [1 0 0 1 0 0].)
            *
            * 2) If Top DICT has FontMatrix and Font DICT doesn't, then Top DICT = (supplied
            * matrix), Font DICT = [1 0 0 1 0 0].
            *
            * 3) If Top DICT does not have FontMatrix but Font DICT does, then Top DICT = [1
            * 0 0 1 0 0], Font DICT = (supplied matrix).  (Or, Top DICT = (absent),
            * Font DICT = (supplied matrix) then let '/CIDFont defineresource'
            * make Top DICT = [0.001 0 0 0.001 0 0], Font DICT = (supplied matrix 1000 times
            * larger).)
            *
            * 4) If both Top DICT and Font DICT _does_ have FontMatrix, then Top DICT =
            * (supplied matrix), Font DICT = (supplied matrix).
            */
            if (top_has_FontMatrix) {
                if (dict_find(fdfont, &name_FontMatrix, &array_FontMatrix) <= 0) {
                    if ((code = set_defaults(i_ctx_p, fdfont, set_unit_matrix, count_of(set_unit_matrix))) < 0)
                        return code;
                }
            }
            if ((code = set_defaults(i_ctx_p, fdfont, fd_font_defaults, count_of(fd_font_defaults))) < 0)
                return code;
        }

        if ((code = card8(&fdselect_code, data, p_all + fdselect_off, pe_all)) < 0)
          return code;
        if (fdselect_code == 0)
            fdselect_proc = format0_fdselect_proc;
        else if (fdselect_code == 3)
            fdselect_proc = format3_fdselect_proc;
        else
            return_error(gs_error_rangecheck);

        make_int(&int_ref, 0);
        for (i = 0; i < charstrings_index.count; i++) {
            int fd;

            if (fdarray.count <= 1)
               fd = -1;
            else {
               fd = (*fdselect_proc)(data, p_all + fdselect_off + 1, pe_all, i);
               if (fd < 0)
                   return fd;
            }
            if ((code = make_string_from_index(i_ctx_p, &cstr_ref, &charstrings_index, data, i, fd)) < 0)
                return code;
            if (i > 0)
                int_ref.value.intval = (*peek_charset_proc)(data, p_all + offsets.charset_off + 1, pe_all, i - 1);
            if ( int_ref.value.intval < 0)
                return int_ref.value.intval;
            if ((code = idict_put(&glyph_directory_ref, &int_ref, &cstr_ref)) < 0)
                return code;
        }
        int_ref.value.intval = fdarray.count > 1;
        if ((code = idict_put_c_name(i_ctx_p, topdict, STR2MEM("FDBytes"), &int_ref)) < 0)
            return code;
        if ((code = set_defaults(i_ctx_p, topdict, cid_font_defaults, count_of(cid_font_defaults))) < 0)
            return code;
    } else if (force_cidfont) {
        /* Convert simple font to CIDFont */
        ref int_ref, str_ref, glyph_directory_ref, fdarray_ref, cidsysteminfo_ref;

        if ((code = idict_undef_c_name(i_ctx_p, topdict, STR2MEM("UniqueID"))) < 0)
            return code;
        if ((code = idict_undef_c_name(i_ctx_p, topdict, STR2MEM("XUID"))) < 0)
            return code;
        if ((code = ialloc_ref_array(&fdarray_ref, a_readonly, 1, "force_cid.fdarray")) < 0)
            return code;
        if ((code = idict_put_c_name(i_ctx_p, topdict, STR2MEM("FDArray"), &fdarray_ref)) < 0)
            return code;
        if ((code = dict_create(4, &fdarray_ref.value.refs[0])) < 0)
            return code;
        if ((code = idict_move_c_name(i_ctx_p, &fdarray_ref.value.refs[0], topdict, STR2MEM("FontMatrix"))) < 0)
            return code;
        if ((code = idict_move_c_name(i_ctx_p, &fdarray_ref.value.refs[0], topdict, STR2MEM("Private"))) < 0)
            return code;
        if ((code = idict_move_c_name(i_ctx_p, &fdarray_ref.value.refs[0], topdict, STR2MEM("FontType"))) < 0)
            return code;
        if ((code = idict_move_c_name(i_ctx_p, &fdarray_ref.value.refs[0], topdict, STR2MEM("PaintType"))) < 0)
            return code;
        if ((code = set_defaults(i_ctx_p, &fdarray_ref.value.refs[0], fd_font_defaults, count_of(fd_font_defaults))) < 0)
            return code;
        if ((code = dict_create(3, &cidsysteminfo_ref)) < 0)
            return code;
        make_string(&str_ref, avm_foreign | a_readonly, 5, (unsigned char *)"Adobe");
        if ((code = idict_put_c_name(i_ctx_p, &cidsysteminfo_ref, STR2MEM("Registry"), &str_ref)) < 0)
            return code;
        make_string(&str_ref, avm_foreign | a_readonly, 8, (unsigned char *)"Identity");
        if ((code = idict_put_c_name(i_ctx_p, &cidsysteminfo_ref, STR2MEM("Ordering"), &str_ref)) < 0)
            return code;
        make_int(&int_ref, 0);
        if ((code = idict_put_c_name(i_ctx_p, &cidsysteminfo_ref, STR2MEM("Supplement"), &int_ref)) < 0)
            return code;
        if ((code = idict_put_c_name(i_ctx_p, topdict, STR2MEM("CIDSystemInfo"), &cidsysteminfo_ref)) < 0)
            return code;
        if ((code = idict_put_c_name(i_ctx_p, topdict, STR2MEM("FDBytes"), &int_ref)) < 0)
            return code;

        if ((code = dict_create(charstrings_index.count + 1, &glyph_directory_ref)) < 0)
            return code;
        if ((code = idict_put_c_name(i_ctx_p, topdict, STR2MEM("GlyphDirectory"), &glyph_directory_ref)) < 0)
            return code;
        make_int(&int_ref, 0);
        for ( ; int_ref.value.intval < (int)charstrings_index.count; int_ref.value.intval++) {
            ref cstr_ref;

            if ((code = make_string_from_index(i_ctx_p, &cstr_ref, &charstrings_index, data, int_ref.value.intval, -1)) < 0)
                return code;
            if ((code = idict_put(&glyph_directory_ref, &int_ref, &cstr_ref)) < 0)
                return code;
        }
        make_int(&int_ref, charstrings_index.count);
        if ((code = idict_put_c_name(i_ctx_p, topdict, STR2MEM("CIDCount"), &int_ref)) < 0)
            return code;
        if ((code = set_defaults(i_ctx_p, topdict, cid_font_defaults, count_of(cid_font_defaults))) < 0)
            return code;
    } else {
        /* Simple font */
        unsigned int i, gid, enc_format = 0;
        int sid;
        ref ccoderef, name, cstr, cffcharstrings_dict, charstrings_dict, encoding, notdef;
        unsigned char gid2char[256];
        unsigned supp_enc_offset = 0;

        if ((code = name_ref(imemory, (unsigned char *)".notdef", 7, &notdef, 0)) < 0)
            return code;
        if ((code = make_string_from_index(i_ctx_p, &cstr, &charstrings_index, data, 0, -1)) < 0)
            return code;
        if ((code = dict_create(charstrings_index.count + 1, &charstrings_dict)) < 0)
            return code;
        if ((code = dict_create(charstrings_index.count + 1, &cffcharstrings_dict)) < 0)
            return code;
        if ((code = idict_put_c_name(i_ctx_p, topdict, STR2MEM("CharStrings"), &charstrings_dict)) < 0)
            return code;
        if ((code = idict_put_c_name(i_ctx_p, topdict, STR2MEM("CFFCharStrings"), &cffcharstrings_dict)) < 0)
            return code;
        make_int(&ccoderef, 0);
        if ((code = idict_put(&charstrings_dict, &notdef, &ccoderef)) < 0)
            return code;
        if ((code = idict_put(&cffcharstrings_dict, &ccoderef, &cstr)) < 0)
            return code;
        if (offsets.encoding_off <= 1) {
            if ((code = ialloc_ref_array(&encoding, a_readonly, 256, "cff_parser.encoding")) < 0)
                return code;
            for (i = 0; i < 256; i++) {
                if ((code = make_name_from_sid(i_ctx_p, &encoding.value.refs[i],
                  strings, data, (offsets.encoding_off ? expert_encoding : standard_encoding)[i])) < 0)
                    return code;
            }
        } else {
            if ((code = ialloc_ref_array(&encoding, a_readonly, 256, "cff_parser.encoding")) < 0)
                return code;
            for (i = 0; i < 256; i++)
                encoding.value.refs[i] = notdef;
            if ((code = card8(&enc_format, data, p_all + offsets.encoding_off, pe_all)) < 0)
                return code;
            if ((enc_format & 0x7f) == 0) {
                unsigned int n_codes;

                if ((code = card8(&n_codes, data, p_all + offsets.encoding_off + 1, pe_all)) < 0)
                    return code;
                gid2char[0] = 0;
                for (i = 0; i < n_codes; i++) {
                    unsigned int charcode;

                    if ((code = card8(&charcode, data, p_all + offsets.encoding_off + 2 + i, pe_all)) < 0)
                        return code;
                    gid2char[i + 1] = charcode;
                }
                memset(gid2char + n_codes + 1, 0, sizeof(gid2char) - n_codes - 1);
                supp_enc_offset = p_all + offsets.encoding_off + 2 + n_codes;
            } else if ((enc_format & 0x7f) == 1) {
                unsigned int n_ranges, first, left, j, k = 1;

                if ((code = card8(&n_ranges, data, p_all + offsets.encoding_off + 1, pe_all)) < 0)
                    return code;
                gid2char[0] = 0;
                for (i = 0; i < n_ranges; i++) {
                    if ((code = card8(&first, data, p_all + offsets.encoding_off + 2 + 2*i, pe_all)) < 0)
                        return code;
                    if ((code = card8(&left,  data, p_all + offsets.encoding_off + 3 + 2*i, pe_all)) < 0)
                        return code;
                    for (j = 0; j <= left && k < 256; j++)
                         gid2char[k++] = first + j;
                }
                memset(gid2char + k, 0, sizeof(gid2char) - k);
                supp_enc_offset = p_all + offsets.encoding_off + 2*n_ranges + 2;
            } else
                return_error(gs_error_rangecheck);
        }
        if ((code = idict_put_c_name(i_ctx_p, topdict, STR2MEM("Encoding"), &encoding)) < 0)
            return code;
        for (gid = 1; gid < charstrings_index.count; gid++) {
            if ((code = make_string_from_index(i_ctx_p, &cstr, &charstrings_index, data, gid, -1)) < 0)
                return code;
            sid = (*peek_charset_proc)(data, p_all + offsets.charset_off + 1, pe_all, gid - 1);
            if (sid < 0)
                return sid;
            if ((code = make_name_from_sid(i_ctx_p, &name, strings, data, sid)) < 0) {
               char buf[40];
               int len = gs_snprintf(buf, sizeof(buf), "sid-%d", sid);

               if ((code = name_ref(imemory, (unsigned char *)buf, len, &name, 1)) < 0)
                   return code;
            }
            make_int(&ccoderef, gid);
            if ((code = idict_put(&charstrings_dict, &name, &ccoderef)) < 0)
                return code;
            if ((code = idict_put(&cffcharstrings_dict, &ccoderef, &cstr)) < 0)
                return code;
            if (offsets.encoding_off > 1 && gid < 256) {
                encoding.value.refs[gid2char[gid]] = name;
            }
        }
        if (offsets.encoding_off > 1 && (enc_format & 0x80)) {
            unsigned int n_supp, charcode, sid;

            if ((code = card8(&n_supp, data, supp_enc_offset, pe_all)) < 0)
                return code;
            for (i = 0; i < n_supp; i++) {
                if ((code = card8(&charcode, data, supp_enc_offset + 1 + 3*i, pe_all)) < 0)
                    return code;
                if ((code = card16(&sid, data, supp_enc_offset + 2 + 3*i, pe_all)) < 0)
                    return code;
                if ((code = make_name_from_sid(i_ctx_p, &encoding.value.refs[charcode], strings, data, sid)) < 0)
                    return code;
            }
        }
        if ((code = set_defaults(i_ctx_p, fontinfodict, fontinfo_font_defaults, count_of(fontinfo_font_defaults))) < 0)
            return code;
        if ((code = set_defaults(i_ctx_p, privatedict, private_font_defaults, count_of(private_font_defaults))) < 0)
            return code;
        if ((code = set_defaults(i_ctx_p, topdict, simple_font_defaults, count_of(simple_font_defaults))) < 0)
            return code;
    }
    return 0;
}

/* <force_cidfont> <string> .parsecff <dict> */
/* Parse CFF font stored in a single string */
static int
zparsecff(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    unsigned p, pe;
    unsigned int hdr_ver, hdr_size, off_size;
    cff_index_t names, topdicts, strings, gsubrs;
    int code;
    unsigned int i_font;
    ref gsubrs_array, fontset;
    bool force_cidfont;
    cff_data_t data;
    ref blk_wrap[1];

    check_op(2);
    check_read(*op);

    if (r_has_type(op, t_array)) {  /* no packedarrays */
        int i, blk_sz, blk_cnt;

        if (op->value.refs == NULL)
            return_error(gs_error_typecheck);

        data.blk_ref = op->value.refs;
        blk_cnt  = r_size(op);
        blk_sz = r_size(data.blk_ref);
        data.length = 0;
        for (i = 0; i < blk_cnt; i++) {
            int len;

            check_read_type(data.blk_ref[i], t_string);
            len = r_size(&data.blk_ref[i]);
            if (len > blk_sz || (len < blk_sz && i < blk_cnt - 1))
               return_error(gs_error_rangecheck); /* last block can be smaller */
            data.length += len;
        }
        if (data.length == 0)
            return_error(gs_error_rangecheck);

        if (blk_cnt == 1) {
           data.mask   = 0xffff; /* stub */
           data.shift  = 16;     /* stub */
        } else {
            static const int mod2shift[] = {
              0, 0, 1, 26, 2, 23, 27, 0, 3, 16, 24, 30, 28, 11, 0, 13, 4, 7, 17,
              0, 25, 22, 31, 15, 29, 10, 12, 6, 0, 21, 14, 9, 5, 20, 8, 19, 18
            };
            data.mask = blk_sz - 1;
            if (data.mask & blk_sz)
                return_error(gs_error_rangecheck);  /* not a power of 2 */
            data.shift = mod2shift[blk_sz % 37];
        }
    } else if (r_has_type(op, t_string)) {
       data.blk_ref = blk_wrap;
       blk_wrap[0] = *op;
       data.length = r_size(op);
       data.mask   = 0xffff; /* stub */
       data.shift  = 16;     /* stub */
    } else
        return_error(gs_error_typecheck);

    check_type(op[-1], t_boolean);
    force_cidfont = op[-1].value.boolval;
    p  = 0;
    pe = p + data.length;

    if (pe < 8) /* arbitrary, to avoid underflow in pe - 4 */
        return_error(gs_error_rangecheck);
    if ((code = card8(&hdr_ver, &data, p, pe)) < 0)
        return code;
    if (hdr_ver != 1)
        return_error(gs_error_rangecheck); /* Unsupported version. */
    if ((code = card8(&hdr_size, &data, p + 2, pe)) < 0)
        return code;
    if ((code = card8(&off_size, &data, p + 3, pe)) < 0)
        return code;

    if ((code = parse_index(&names, &data, p + hdr_size, pe)) < 0)
        return code;
    if ((code = parse_index(&topdicts, &data, names.end, pe)) < 0)
        return code;
    if ((code = parse_index(&strings, &data, topdicts.end, pe)) < 0)
        return code;
    if ((code = parse_index(&gsubrs, &data, strings.end, pe)) < 0)
        return code;
    if (gsubrs.count) {
        if (( code = make_stringarray_from_index(i_ctx_p, &gsubrs_array, &gsubrs, &data)) < 0)
            return code;
    }
    if (names.count >= 65535)
        return_error(gs_error_limitcheck);
    if ((code = dict_create(names.count, &fontset)) < 0)
        return code;

    for (i_font = 0; i_font < names.count; i_font++) {
        unsigned int name_data;
        unsigned int name_len;
        ref name;
        unsigned int topdict_data;
        unsigned int topdict_len;
        ref topdict;
        unsigned char buf[200];

        if ((code = peek_index(&name_data, &name_len, &names, &data, i_font)) < 0)
            return code;
        if (name_len == 0) /* deleted entry */
            continue;
        if (name_len > sizeof(buf))
            return_error(gs_error_limitcheck);
        if ((code = get_cff_string(buf, &data, name_data, name_len)) < 0)
            return code;
        if (buf[0] == 0)   /* deleted entry */
            continue;
        if ((code = name_ref(imemory, buf, name_len, &name, 1)) < 0)
            return code;
        if ((code = peek_index(&topdict_data, &topdict_len, &topdicts, &data, i_font)) < 0)
            return code;

        if ((code = dict_create(21, &topdict)) < 0)
            return code;

        if ((code = idict_put(&fontset, &name, &topdict)) < 0)
            return code;

        if ((code = parse_font(i_ctx_p, &topdict, &strings, (gsubrs.count ? &gsubrs_array : 0),
                 &data, topdict_data, topdict_data + topdict_len, p, pe, force_cidfont)) < 0)
            return code;
    }
    ref_assign(op - 1, &fontset);
    pop(1);
    return 0;
}

/* ------ Initialization procedure ------ */

const op_def zfont2_op_defs[] =
{
    {"2.buildfont2", zbuildfont2},
    {"2.parsecff", zparsecff},
    op_def_end(0)
};
