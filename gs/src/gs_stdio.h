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

/*$RCSfile$ $Revision$ */
/* Fake stdin.h header file to access the stream code, for use in icclib. */

#ifndef gs_stdio_INCLUDED
#  define gs_stdio_INCLUDED

#include "stdio_.h"
#include "stream.h"

/* WRONG: this should be 'stream' but this causes compile problems */
/* We really need to clean up the icclib stdio area later */
#define FILE    void

extern  int     sread(void *, unsigned int, unsigned int, FILE *),
                swrite(const void *, unsigned int, unsigned int, FILE *);

#define fread(buf, sz, cnt, strm)   sread((buf), (sz), (cnt), (strm))
#define fwrite(buf, sz, cnt, strm)  swrite((buf), (sz), (cnt), (strm))

/* this definition of fseek is sufficient for icclib */
#define fseek(strm, off, w) spseek((strm), (off))

/* flush is only used for writing, which we do not require */
#define fflush(s)   0

#endif  /* gs_stdio_INCLUDED */
