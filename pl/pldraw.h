/* Copyright (C) 1997 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* pldraw.h */
/* Interface to pldraw.c */
#include "gsiparam.h"
#ifndef gs_state_DEFINED
#  define gs_state_DEFINED
typedef struct gs_state_s gs_state;
#endif

/* Begin an image with parameters derived from a graphics state. */
int pl_begin_image(P3(gs_state *pgs, const gs_image_t *pim,
		      void **pinfo));
