/*
    jbig2dec
    
    Copyright (c) 2001 artofcode LLC.
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    $Id: jbig2_page.c,v 1.1 2002/06/15 14:15:12 giles Exp $
*/

#include <stdio.h>
#include <stdlib.h>

#include "jbig2.h"
#include "jbig2_priv.h"
#include "jbig2_page.h"

/* parse the page info segment data starting at ctx->offset
   a pointer to a new Jbig2PageInfo struct is returned

   the ctx->offset pointer is not advanced; the caller must
   take care of that, using the data_length field of the
   segment header.
*/
static Jbig2PageInfo *
jbig2_read_page_info (Jbig2Ctx *ctx) {
  Jbig2PageInfo *info = (Jbig2PageInfo *)jbig2_alloc(ctx->allocator, sizeof(Jbig2PageInfo));
  int offset = ctx->buf_rd_idx;

	if (info == NULL) {
		printf("unable to allocate memory to parse page info segment\n");
		return NULL;
	}
	
	info->width = get_int32(ctx, offset);
	info->height = get_int32(ctx, offset + 4);
	offset += 8;
	
	info->x_resolution = get_int32(ctx, offset);
	info->y_resolution = get_int32(ctx, offset + 4);
	offset += 8;
	
	get_bytes(ctx, &(info->flags), 1, offset++);
	
	{
	int16 striping = get_int16(ctx, offset);
	if (striping & 0x8000) {
		info->striped = TRUE;
		info->stripe_size = striping & 0x7FFF;
	} else {
		info->striped = FALSE;
		info->stripe_size = 0;	/* would info->height be better? */
	}
	offset += 2;
	}
	
	return info;
}

/* dump the page info struct */
static void
dump_page_info(Jbig2PageInfo *info)
{
    FILE *out = stderr;
    
    fprintf(out, "image is %dx%d ", info->width, info->height);
    if (info->x_resolution == 0) {
        fprintf(out, "(unknown res) ");
    } else if (info->x_resolution == info->y_resolution) {
        fprintf(out, "(%d ppm) ", info->x_resolution);
    } else {
        fprintf(out, "(%dx%d ppm) ", info->x_resolution, info->y_resolution);
    }
    if (info->striped) {
        fprintf(out, "\tmaximum stripe size: %d\n", info->stripe_size);
    } else {
        fprintf(out, "\tno striping\n");
    }
}