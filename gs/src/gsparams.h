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
/* Serializer/expander for gs_parm_list's */

#ifndef gsparams_INCLUDED
#  define gsparams_INCLUDED

/* Initial version 2/1/98 by John Desrosiers (soho@crl.com) */
/* 8/8/98 L. Peter Deutsch (ghost@aladdin.com) Completely redesigned
   to use stream rather than buffer API (but retained former API for
   compatibility as well). */

#include "stream.h"
#include "gsparam.h"

#if 0	/****************/

/* ------ Future interface, implemented in gsparam2.c ------ */

/*
 * Serialize the contents of a gs_param_list, including sub-collections,
 * onto a stream.  The list must be in READ mode.
 */
int gs_param_list_puts(stream *dest, gs_param_list *list);

/*
 * Unserialize a parameter list, including sub-collections, from a stream.
 * The list must be in WRITE mode.
 */
int gs_param_list_gets(stream *src, gs_param_list *list, gs_memory_t *mem);

#else	/****************/

/* ------ Present interface, implemented in gsparams.c ------ */

/*
 * Serialize a parameter list into a buffer.  Return the actual number
 * of bytes required to store the list, or a negative error code.
 * The list was stored successfully iff the return value is positive and
 * less than or equal to the buffer size.  Note that the buffer may be
 * NULL, in which case nothing is stored (but the size is still returned).
 */
int gs_param_list_serialize(gs_param_list *list, byte *buf, int buf_size);

/*
 * Unserialize a parameter list from a buffer.  Return the actual number
 * of bytes occupied by the list, or a negative error code.  The buffer
 * must be void * aligned.
 */
int gs_param_list_unserialize(gs_param_list *list, const byte *buf);

#endif	/****************/

#endif /* gsparams_INCLUDED */
