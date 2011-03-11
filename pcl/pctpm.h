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

/* pctpm.h - structures of PCL's text parsing methods */

#ifndef pctpm_INCLUDED
#define pctpm_INCLUDED

#include "gx.h"

typedef enum  {
/* http://lprng.sourceforge.net/DISTRIB/RESOURCES/DOCS/pcl5comp.pdf */
    tpm_0_SBCS    = 0,
    tpm_21_DBCS7  = 21,   /* e.g. GB_2312-80 (China), JIS_X0208-1983 (Japan), KSC_5601 (Korea) */
    tpm_31_sjis   = 31,
    tpm_38_DBCS8  = 38,	  
    tpm_83_utf8   = 83,	  /* http://docs.hp.com/en/5991-7956/5991-7956.pdf */
                          /* HP-UX 11i v3 International Printing Features  */
    tpm_1008_utf8 = 1008  /* http://www.lexmark.com/publications/pdfs/TRef_3Q01.pdf */
} pcl_text_parsing_method_t;

int pcl_char_bytelen(byte ch, pcl_text_parsing_method_t tpm);
gs_char pcl_char_get_char(pcl_text_parsing_method_t tpm, const byte ** psrc, int len);

/*
 *  was 0xfffd  
 */
#define INVALID_UC 0xffff

#endif			/* pctpm_INCLUDED */
