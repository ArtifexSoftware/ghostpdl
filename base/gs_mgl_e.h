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


/* $Id: gs_mgl_e.h 11951 2010-12-15 08:22:58Z ken */
/* Originally stored in gs/Resource/Init as gs_mgl_e.ps and built into
 * the ROM file system, this file was referenced by ps2write when
 * writing PostScript output. With the inclusion of ps2write in the
 * PCL and XPS interpreters we can no longer rely on the PostScript
 * resources being present, and so this file has been converted to
 * 'C' and included as a header. The original file is now stored
 * in gs/lib with the comments intact.
 */
const char *gs_mgl_e_ps [] = {
"/currentglobal where\n",
"{pop currentglobal{setglobal}true setglobal}\n",
"{{}}\n",
"ifelse\n",
"/MacRomanEncoding .findencoding\n",
"/MacGlyphEncoding\n",
"/.notdef/.null/CR\n",
"4 index 32 95 getinterval aload pop\n",
"99 index 128 45 getinterval aload pop\n",
"/notequal/AE\n",
"/Oslash/infinity/plusminus/lessequal/greaterequal\n",
"/yen/mu1/partialdiff/summation/product\n",
"/pi/integral/ordfeminine/ordmasculine/Ohm\n",
"/ae/oslash/questiondown/exclamdown/logicalnot\n",
"/radical/florin/approxequal/increment/guillemotleft\n",
"/guillemotright/ellipsis/nbspace\n",
"174 index 203 12 getinterval aload pop\n",
"/lozenge\n",
"187 index 216 24 getinterval aload pop\n",
"/applelogo\n",
"212 index 241 7 getinterval aload pop\n",
"/overscore\n",
"220 index 249 7 getinterval aload pop\n",
"/Lslash/lslash/Scaron/scaron\n",
"/Zcaron/zcaron/brokenbar/Eth/eth\n",
"/Yacute/yacute/Thorn/thorn/minus\n",
"/multiply/onesuperior/twosuperior/threesuperior/onehalf\n",
"/onequarter/threequarters/franc/Gbreve/gbreve\n",
"/Idotaccent/Scedilla/scedilla/Cacute/cacute\n",
"/Ccaron/ccaron/dmacron\n",
"260 -1 roll pop\n",
"258 packedarray\n",
"7 1 index .registerencoding\n",
".defineencoding\n",
"exec\n",
0x00
};
