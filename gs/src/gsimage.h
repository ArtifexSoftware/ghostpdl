/* Copyright (C) 1992, 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
  This file is part of Aladdin Ghostscript.
  
  Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
  or distributor accepts any responsibility for the consequences of using it,
  or for whether it serves any particular purpose or works at all, unless he
  or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
  License (the "License") for full details.
  
  Every copy of Aladdin Ghostscript must include a copy of the License,
  normally in a plain ASCII text file named PUBLIC.  The License grants you
  the right to copy, modify and redistribute Aladdin Ghostscript, but only
  under certain conditions described in the License.  Among other things, the
  License requires that the copyright notice and this notice be preserved on
  all copies.
*/

/* gsimage.h */
/* Client interface to image painting */
/* Requires gsstate.h */
#include "gsiparam.h"

/*
 * The image painting interface uses an enumeration style:
 * the client initializes an enumerator, then supplies data incrementally.
 */
typedef struct gs_image_enum_s gs_image_enum;
gs_image_enum *gs_image_enum_alloc(P2(gs_memory_t *, client_name_t));
/*
 * image_init returns 1 for an empty image, 0 normally, <0 on error.
 * Note that image_init serves for both image and imagemask,
 * depending on the value of ImageMask in the image structure.
 */
int gs_image_init(P4(gs_image_enum *penum, const gs_image_t *pim,
		     bool MultipleDataSources, gs_state *pgs));
int gs_image_next(P4(gs_image_enum *penum, const byte *dbytes,
		     uint dsize, uint *pused));
/*
 * Return the number of bytes of data per row
 * (per plane, if MultipleDataSources is true).
 */
uint gs_image_bytes_per_row(P1(const gs_image_enum *penum));
/* Clean up after processing an image. */
void gs_image_cleanup(P1(gs_image_enum *penum));
