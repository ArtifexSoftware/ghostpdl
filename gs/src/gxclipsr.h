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
/* Internals of clipsave/cliprestore */

#ifndef gxclipsr_INCLUDED
#  define gxclipsr_INCLUDED

#include "gsrefct.h"

/*
 * Unlike the graphics state stack, which is threaded through the actual
 * gstate objects, the clipping path stack is implemented with separate,
 * small objects.  These are reference-counted, because they may be
 * shared by off-stack graphics states.
 */

#ifndef gx_clip_path_DEFINED
#  define gx_clip_path_DEFINED
typedef struct gx_clip_path_s gx_clip_path;
#endif
#ifndef gx_clip_stack_DEFINED
#  define gx_clip_stack_DEFINED
typedef struct gx_clip_stack_s gx_clip_stack_t;
#endif

struct gx_clip_stack_s {
    rc_header rc;
    gx_clip_path *clip_path;
    gx_clip_stack_t *next;
};

#define private_st_clip_stack()	/* in gsclipsr.c */\
  gs_private_st_ptrs2(st_clip_stack, gx_clip_stack_t,\
    "gx_clip_stack_t", clip_stack_enum_ptrs, clip_stack_reloc_ptrs,\
    clip_path, next)

#endif /* gxclipsr_INCLUDED */
