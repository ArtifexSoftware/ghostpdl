/*
    jbig2dec
    
    Copyright (c) 2001-2002 artofcode LLC.
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    $Id: jbig2_page.c,v 1.3 2002/06/18 13:40:29 giles Exp $
*/

#include <stdio.h>
#include <stdlib.h>

#include "jbig2.h"
#include "jbig2_priv.h"

/* dump the page struct info */
static void
dump_page_info(Jbig2Ctx *ctx, Jbig2SegmentHeader *sh, Jbig2Page *page)
{
    if (page->x_resolution == 0) {
        jbig2_error(ctx, JBIG2_SEVERITY_INFO, sh->segment_number,
            "page %d image is %dx%d (unknown res)", page->number,
            page->width, page->height);
    } else if (page->x_resolution == page->y_resolution) {
        jbig2_error(ctx, JBIG2_SEVERITY_INFO, sh->segment_number,
            "page %d image is %dx%d (%d ppm)", page->number,
            page->width, page->height,
            page->x_resolution);
    } else {
        jbig2_error(ctx, JBIG2_SEVERITY_INFO, sh->segment_number,
            "page %d image is %dx%d (%dx%d ppm)", page->number,
            page->width, page->height,
            page->x_resolution, page->y_resolution);
    }
    if (page->striped) {
        jbig2_error(ctx, JBIG2_SEVERITY_INFO, sh->segment_number,
            "\tmaximum stripe size: %d", page->stripe_size);
    }
}

/**
 * jbig2_read_page_info: parse page info segment
 *
 * parse the page info segment data and fill out a corresponding
 * Jbig2Page struct is returned, including allocating an image
 * buffer for the page (or the first stripe)
 **/
int
jbig2_read_page_info (Jbig2Ctx *ctx, Jbig2SegmentHeader *sh, const byte *segment_data)
{
    Jbig2Page *page;

    /* a new page info segment implies the previous page is finished */
    page = &(ctx->pages[ctx->current_page]);
    if ((page->number != 0) && 
            (page->state == JBIG2_PAGE_NEW) || (page->state == JBIG2_PAGE_FREE)) {
        page->state = JBIG2_PAGE_COMPLETE;
        jbig2_error(ctx, JBIG2_SEVERITY_WARNING, sh->segment_number,
            "unexpected page info segment, marking previous page finished");
    }
        
    /* find a free page */
    {
        int index, j;
        index = ctx->current_page;
        while (ctx->pages[index].state != JBIG2_PAGE_FREE) {
            index++;
            if (index >= ctx->max_page_index) { // FIXME: should also look for freed pages?
                /* grow the list */
                jbig2_realloc(ctx->allocator, ctx->pages, (ctx->max_page_index <<= 2) * sizeof(Jbig2Page));
                for (j=index; j < ctx->max_page_index; j++) {
                    /* note to raph: and look, it gets worse! */
                    ctx->pages[j].state = JBIG2_PAGE_FREE;
                    ctx->pages[j].number = 0;
                    ctx->pages[j].image = NULL;

                }
            }
        }
        page = &(ctx->pages[index]);
        ctx->current_page = index;
        page->state = JBIG2_PAGE_NEW;
        page->number = sh->page_association;
    }
    
    // FIXME: would be nice if we tried to work around this
    if (sh->data_length < 19) {
        jbig2_error(ctx, JBIG2_SEVERITY_FATAL, sh->segment_number,
            "segment too short");
        return NULL;
    }
    
    /* 7.4.8.x */
    page->width = jbig2_get_int32(segment_data);
    page->height = jbig2_get_int32(segment_data + 4);
    
    page->x_resolution = jbig2_get_int32(segment_data + 8);
    page->y_resolution = jbig2_get_int32(segment_data + 12);
    page->flags = segment_data[16];

    /* 7.4.8.6 */
    {
        int16_t striping = jbig2_get_int16(segment_data +17);
        if (striping & 0x8000) {
            page->striped = TRUE;
            page->stripe_size = striping & 0x7FFF;
        } else {
            page->striped = FALSE;
            page->stripe_size = 0;	/* would page->height be better? */
        }
    }
    if (page->height == 0xFFFFFFFF && page->striped == FALSE) {
        jbig2_error(ctx, JBIG2_SEVERITY_WARNING, sh->segment_number,
            "height is unspecified but page is not markes as striped");
        page->striped = TRUE;
    }
    
    if (sh->data_length > 19) {
        jbig2_error(ctx, JBIG2_SEVERITY_WARNING, sh->segment_number,
            "extra data in segment");
    }
    
    dump_page_info(ctx, sh, page);

    /* allocate an approprate page image buffer */
    /* 7.4.8.2 */
    if (page->height == 0xFFFFFFFF) {
        page->image = jbig2_image_new(ctx, page->width, page->stripe_size);
    } else {
        page->image = jbig2_image_new(ctx, page->width, page->height);
    }
    if (page->image == NULL) {
        jbig2_error(ctx, JBIG2_SEVERITY_FATAL, sh->segment_number,
            "failed to allocate buffer for page image");
        jbig2_free(ctx->allocator, page);
        return NULL;
    } else {
        jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, sh->segment_number,
            "allocated %dx%d page image (%d bytes)",
            page->image->width, page->image->height,
            page->image->stride*page->image->height);
    }
    
    return 0;
}

/**
 * jbig2_complete_page: complete a page image
 *
 * called upon seeing an 'end of page' segment, this routine
 * marks a page as completed. final compositing and output
 * of the page image will also happen from here (NYI)
 **/
int
jbig2_complete_page (Jbig2Ctx *ctx, Jbig2SegmentHeader *sh, const byte *segment_data)
{
    uint32_t page_number = ctx->pages[ctx->current_page].number;
    
    if (sh->page_association != page_number) {
        jbig2_error(ctx, JBIG2_SEVERITY_WARNING, sh->segment_number,
            "end of page marker for page %d doesn't match current page number %d",
            sh->page_association, page_number);
    }
    
    ctx->pages[ctx->current_page].state = JBIG2_PAGE_COMPLETE;
    jbig2_error(ctx, JBIG2_SEVERITY_INFO, sh->segment_number,
        "end of page %d", page_number);

    // FIXME: write out the page image
    
    return 0;
}