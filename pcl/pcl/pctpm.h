/* Copyright (C) 2001-2020 Artifex Software, Inc.
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


/* pctpm.h - structures of PCL's text parsing methods */

#ifndef pctpm_INCLUDED
#define pctpm_INCLUDED

#include "gx.h"

typedef enum
{
/* http://lprng.sourceforge.net/DISTRIB/RESOURCES/DOCS/pcl5comp.pdf */
    tpm_0_SBCS = 0,
    tpm_21_DBCS7 = 21,          /* e.g. GB_2312-80 (China), JIS_X0208-1983 (Japan), KSC_5601 (Korea) */
    tpm_31_sjis = 31,
    tpm_38_DBCS8 = 38,
    tpm_83_utf8 = 83,           /* http://docs.hp.com/en/5991-7956/5991-7956.pdf */
    /* HP-UX 11i v3 International Printing Features  */
    tpm_1008_utf8 = 1008        /* http://www.lexmark.com/publications/pdfs/TRef_3Q01.pdf */
} pcl_text_parsing_method_t;

int pcl_char_bytelen(byte ch, pcl_text_parsing_method_t tpm);

gs_char pcl_char_get_char(pcl_text_parsing_method_t tpm, const byte ** psrc,
                          int len);

/*
 *  was 0xfffd
 */
#define INVALID_UC 0xffff

#endif /* pctpm_INCLUDED */
