/* Copyright (C) 2001-2011 Artifex Software, Inc.
   All Rights Reserved.
  
   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

/* $Id: gs_mor_e.h 11951 2010-12-15 08:22:58Z ken */
/* Originally stored in gs/Resource/Init as gs_mro_e.ps and built into
 * the ROM file system, this file was referenced by ps2write when
 * writing PostScript output. With the inclusion of ps2write in the 
 * PCL and XPS interpreters we can no longer rely on the PostScript
 * resources being present, and so this file has been converted to 
 * 'C' and included as a header. The original file is now stored
 * in gs/lib with the comments intact.
 */
const char *gs_mro_e_ps [] = {
"/currentglobal where\n",
"{pop currentglobal{setglobal}true setglobal}\n",
"{{}}\n",
"ifelse\n",
"/MacRomanEncoding\n",
"StandardEncoding 0 39 getinterval aload pop\n",
"/quotesingle\n",
"StandardEncoding 40 56 getinterval aload pop\n",
"/grave\n",
"StandardEncoding 97 31 getinterval aload pop\n",
"/Adieresis/Aring/Ccedilla/Eacute/Ntilde/Odieresis/Udieresis/aacute\n",
"/agrave/acircumflex/adieresis/atilde/aring/ccedilla/eacute/egrave\n",
"/ecircumflex/edieresis/iacute/igrave\n",
"/icircumflex/idieresis/ntilde/oacute\n",
"/ograve/ocircumflex/odieresis/otilde\n",
"/uacute/ugrave/ucircumflex/udieresis\n",
"/dagger/degree/cent/sterling/section/bullet/paragraph/germandbls\n",
"/registered/copyright/trademark/acute/dieresis/.notdef/AE/Oslash\n",
"/.notdef/plusminus/.notdef/.notdef/yen/mu/.notdef/.notdef\n",
"/.notdef/.notdef/.notdef/ordfeminine/ordmasculine/.notdef/ae/oslash\n",
"/questiondown/exclamdown/logicalnot/.notdef\n",
"/florin/.notdef/.notdef/guillemotleft\n",
"/guillemotright/ellipsis/space/Agrave/Atilde/Otilde/OE/oe\n",
"/endash/emdash/quotedblleft/quotedblright\n",
"/quoteleft/quoteright/divide/.notdef\n",
"/ydieresis/Ydieresis/fraction/currency\n",
"/guilsinglleft/guilsinglright/fi/fl\n",
"/daggerdbl/periodcentered/quotesinglbase/quotedblbase\n",
"/perthousand/Acircumflex/Ecircumflex/Aacute\n",
"/Edieresis/Egrave/Iacute/Icircumflex\n",
"/Idieresis/Igrave/Oacute/Ocircumflex\n",
"/.notdef/Ograve/Uacute/Ucircumflex\n",
"/Ugrave/dotlessi/circumflex/tilde\n",
"/macron/breve/dotaccent/ring/cedilla/hungarumlaut/ogonek/caron\n",
"256 packedarray\n",
"5 1 index .registerencoding\n",
".defineencoding\n",
"exec\n",
0x00
};
