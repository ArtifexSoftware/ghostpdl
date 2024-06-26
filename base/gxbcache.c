/* Copyright (C) 2001-2023 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  39 Mesa Street, Suite 108A, San Francisco,
   CA 94129, USA, for further information.
*/


/* Bitmap cache implementation */
#include "memory_.h"
#include "stdint_.h"
#include "gx.h"
#include "gxobj.h"
#include "gsmdebug.h"
#include "gxbcache.h"

/* ------ Entire cache ------ */

/* Initialize a cache.  The caller must allocate and initialize */
/* the first chunk. */
void
gx_bits_cache_init(gx_bits_cache * bc, gx_bits_cache_chunk * bck)
{
    bck->next = bck;
    bc->chunks = bck;
    bc->cnext = 0;
    bc->bsize = 0;
    bc->csize = 0;
}

/* ------ Chunks ------ */

/* Initialize a chunk.  The caller must allocate it and its data. */
void
gx_bits_cache_chunk_init(gx_bits_cache_chunk * bck, byte * data, uint size)
{
    bck->next = 0;
    bck->data = data;
    bck->size = size;
    bck->allocated = 0;
    if (data != 0) {
        gx_cached_bits_head *cbh = (gx_cached_bits_head *) data;

        cbh->size = size;
        cb_head_set_free(cbh);
    }
}

/* ------ Individual entries ------ */

/* Attempt to allocate an entry.  If successful, set *pcbh and return 0. */
/* If there isn't enough room, set *pcbh to an entry requiring freeing, */
/* or to 0 if we are at the end of the chunk, and return -1. */
int
gx_bits_cache_alloc(gx_bits_cache * bc, ulong lsize0, gx_cached_bits_head ** pcbh)
{
    ulong lsize = ROUND_UP(lsize0, obj_align_mod);

#define ssize ((uint)lsize)
    ulong lsize1 = lsize + sizeof(gx_cached_bits_head);

#define ssize1 ((uint)lsize1)
    uint cnext = bc->cnext;
    gx_bits_cache_chunk *bck = bc->chunks;
    uint left = bck->size - cnext;
    gx_cached_bits_head *cbh;
    gx_cached_bits_head *cbh_next;
    uint fsize = 0;

    if (lsize1 > bck->size - cnext && lsize != left) {	/* Not enough room to allocate in this chunk. */
        *pcbh = 0;
        return -1;
    }
    /* Look for and/or free enough space. */
    cbh = cbh_next = (gx_cached_bits_head *) (bck->data + cnext);
    while (fsize < ssize1 && fsize != ssize) {
        if (!cb_head_is_free(cbh_next)) {	/* Ask the caller to free the entry. */
            if (fsize)
                cbh->size = fsize;
            *pcbh = cbh_next;
            return -1;
        }
        fsize += cbh_next->size;
        if_debug2('K', "[K]merging free bits "PRI_INTPTR"(%u)\n",
                  (intptr_t)cbh_next, cbh_next->size);
        cbh_next = (gx_cached_bits_head *) ((byte *) cbh + fsize);
    }
    if (fsize > ssize) {	/* fsize >= ssize1 */
        cbh_next = (gx_cached_bits_head *) ((byte *) cbh + ssize);
        cbh_next->size = fsize - ssize;
        cb_head_set_free(cbh_next);
        if_debug2('K', "[K]shortening bits "PRI_INTPTR" by %u (initial)\n",
                  (intptr_t)cbh, fsize - ssize);
    }
    gs_alloc_fill(cbh, gs_alloc_fill_block, ssize);
    cbh->size = ssize;
    bc->bsize += ssize;
    bc->csize++;
    bc->cnext += ssize;
    bck->allocated += ssize;
    *pcbh = cbh;
    return 0;
#undef ssize
#undef ssize1
}

/* Shorten an entry by a given amount. */
void
gx_bits_cache_shorten(gx_bits_cache * bc, gx_cached_bits_head * cbh,
                      uint diff, gx_bits_cache_chunk * bck)
{
    gx_cached_bits_head *next;

    if ((byte *) cbh + cbh->size == bck->data + bc->cnext &&
        bck == bc->chunks
        )
        bc->cnext -= diff;
    bc->bsize -= diff;
    bck->allocated -= diff;
    cbh->size -= diff;
    next = (gx_cached_bits_head *) ((byte *) cbh + cbh->size);
    cb_head_set_free(next);
    next->size = diff;
}

/* Free an entry.  The caller is responsible for removing the entry */
/* from any other structures (like a hash table). */
void
gx_bits_cache_free(gx_bits_cache * bc, gx_cached_bits_head * cbh,
                   gx_bits_cache_chunk * bck)
{
    uint size = cbh->size;

    bc->csize--;
    bc->bsize -= size;
    bck->allocated -= size;
    gs_alloc_fill(cbh, gs_alloc_fill_deleted, size);
    cbh->size = size;		/* gs_alloc_fill may have overwritten */
    cb_head_set_free(cbh);
}

#ifdef DEBUG
/* A useful bit of code to dump the contents of the bitmap cache. Not
 * currently called. Current position is indicated with a '>'. */
void
gx_bits_cache_dump(gx_bits_cache * bc)
{
    gx_bits_cache_chunk *bck = bc->chunks;
    gx_bits_cache_chunk *first = bck;

    dlprintf2("%d entries making %d bytes\n", bc->csize, bc->bsize);

    do {
        gx_cached_bits_head *cbh;
        uint pos = 0;

        dlprintf2(" chunk of %d bytes (%d allocated)\n", bck->size, bck->allocated);

        cbh = (gx_cached_bits_head *)bck->data;
        while (pos != bck->size) {
            dlprintf3(" %csize=%d depth=%d\n",
                      pos == bc->cnext && bck == bc->chunks ? '>' : ' ',
                      cbh->size, cbh->depth);
            pos += cbh->size;
            cbh = (gx_cached_bits_head *)(((byte *)cbh) + cbh->size);
        }
        bck = bck->next;
    } while (bck != first);

    bck=bck;
}
#endif
