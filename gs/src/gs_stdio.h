/* Copyright (C) 2001 1999 Aladdin Enterprises.  All rights reserved.
  
  This file is part of AFPL Ghostscript.
  
  AFPL Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author or
  distributor accepts any responsibility for the consequences of using it, or
  for whether it serves any particular purpose or works at all, unless he or
  she says so in writing.  Refer to the Aladdin Free Public License (the
  "License") for full details.
  
  Every copy of AFPL Ghostscript must include a copy of the License, normally
  in a plain ASCII text file named PUBLIC.  The License grants you the right
  to copy, modify and redistribute AFPL Ghostscript, but only under certain
  conditions described in the License.  Among other things, the License
  requires that the copyright notice and this notice be preserved on all
  copies.
*/

/* $Id$ */
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
