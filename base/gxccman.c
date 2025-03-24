/* Copyright (C) 2001-2025 Artifex Software, Inc.
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


/* Character cache management routines for Ghostscript library */
#include "gx.h"
#include "memory_.h"
#include "gpcheck.h"
#include "gserrors.h"
#include "gsstruct.h"
#include "gsbitops.h"
#include "gsutil.h"		/* for gs_next_ids */
#include "gxfixed.h"
#include "gxmatrix.h"
#include "gzstate.h"
#include "gxpath.h"
#include "gxdevice.h"
#include "gxdevmem.h"
#include "gxchar.h"
#include "gxfont.h"
#include "gxfcache.h"
#include "gxxfont.h"
#include "gxttfb.h"
#include "gxfont42.h"
#include "gxobj.h"

/* Define the descriptors for the cache structures. */
private_st_cached_fm_pair();
private_st_cached_fm_pair_elt();
/*private_st_cached_char(); *//* unused */
private_st_cached_char_ptr();	/* unused */
private_st_cached_char_ptr_elt();
/*
 * Define a descriptor for the cache data.  This is equivalent to st_bytes,
 * but it identifies the cache data as such in a memory dump.
 */
gs_private_st_simple(st_font_cache_bytes, byte, "font cache bytes");
/* GC procedures */
/* We do all the work in font_dir_enum/reloc_ptrs in gsfont.c. */
/* See gxfcache.h for details. */
static
ENUM_PTRS_BEGIN(cc_ptr_enum_ptrs) return 0;

ENUM_PTRS_END
static RELOC_PTRS_BEGIN(cc_ptr_reloc_ptrs)
{
}
RELOC_PTRS_END

/* Forward references */
static int alloc_char(gs_font_dir *, ulong, cached_char **);
static int alloc_char_in_chunk(gs_font_dir *, ulong, cached_char **);
static void hash_remove_cached_char(gs_font_dir *, uint);
static void shorten_cached_char(gs_font_dir *, cached_char *, uint);

/* ====== Initialization ====== */

/* Allocate and initialize the character cache elements of a font directory. */
int
gx_char_cache_alloc(gs_memory_t * struct_mem, gs_memory_t * bits_mem,
            gs_font_dir * pdir, uint bmax, uint mmax, uint cmax, uint upper)
{				/* Since we use open hashing, we must increase cmax somewhat. */
    uint chsize = (cmax + (cmax >> 1)) | 31;
    cached_fm_pair *mdata;
    cached_char **chars;

    /* the table size must be adjusted upward such that we overflow
       cache character memory before filling the table.  The searching
       code uses an empty table entry as a sentinel. */
    chsize = max(chsize, ROUND_UP(bmax, sizeof_cached_char) / sizeof_cached_char + 1);

    /* Round up chsize to a power of 2. */
    while (chsize & (chsize + 1))
        chsize |= chsize >> 1;
    chsize++;
    mdata = gs_alloc_struct_array(struct_mem, mmax, cached_fm_pair,
                                  &st_cached_fm_pair_element,
                                  "font_dir_alloc(mdata)");
    chars = gs_alloc_struct_array(struct_mem, chsize, cached_char *,
                                  &st_cached_char_ptr_element,
                                  "font_dir_alloc(chars)");
    if (mdata == 0 || chars == 0) {
        gs_free_object(struct_mem, chars, "font_dir_alloc(chars)");
        gs_free_object(struct_mem, mdata, "font_dir_alloc(mdata)");
        return_error(gs_error_VMerror);
    }
    pdir->fmcache.mmax = mmax;
    pdir->fmcache.mdata = mdata;
    memset(mdata, 0, mmax * sizeof(*mdata));
    memset(chars, 0, chsize * sizeof(*chars));
    pdir->ccache.struct_memory = struct_mem;
    pdir->ccache.bits_memory = bits_mem;
    pdir->ccache.bmax = bmax;
    pdir->ccache.cmax = cmax;
    pdir->ccache.lower = upper / 10;
    pdir->ccache.upper = upper;
    pdir->ccache.table = chars;
    pdir->ccache.table_mask = chsize - 1;
    return gx_char_cache_init(pdir);
}

/* Initialize the character cache. */
int
gx_char_cache_init(register gs_font_dir * dir)
{
    int i;
    cached_fm_pair *pair;
    char_cache_chunk *cck = (char_cache_chunk *)
    gs_alloc_bytes_immovable(dir->ccache.bits_memory,
                             sizeof(char_cache_chunk),
                             "initial_chunk");
    if (cck == NULL)
        return_error(gs_error_VMerror);

    dir->fmcache.msize = 0;
    dir->fmcache.used = dir->fmcache.mmax;
    dir->fmcache.free = dir->fmcache.mmax;
    dir->fmcache.unused = 0;
    gx_bits_cache_chunk_init(cck, NULL, 0);
    gx_bits_cache_init((gx_bits_cache *) & dir->ccache, cck);
    dir->ccache.bspace = 0;
    memset((char *)dir->ccache.table, 0,
           (dir->ccache.table_mask + 1) * sizeof(cached_char *));
    for (i = 0, pair = dir->fmcache.mdata;
         i < dir->fmcache.mmax; i++, pair++) {
        pair->index = i;
        fm_pair_init(pair);
        pair->ttf = 0;
        pair->ttr = 0;
    }
    return 0;
}

/* ====== Purging ====== */

/* Purge from the character cache all entries selected by */
/* a client-supplied procedure. */
void
gx_purge_selected_cached_chars(gs_font_dir * dir,
                               bool(*proc) (const gs_memory_t *mem,
                                            cached_char *, void *),
                               void *proc_data)
{
    int chi;
    int cmax = dir->ccache.table_mask;

    for (chi = 0; chi <= cmax;) {
        cached_char *cc = dir->ccache.table[chi];

        if (cc != 0 &&
                (*proc) (dir->memory, cc, proc_data)) {
            hash_remove_cached_char(dir, chi);
            gx_free_cached_char(dir, cc);
        } else
            chi++;
    }
}

/* ====== font-matrix pair lists ====== */

static int
fm_pair_remove_from_list(gs_font_dir * dir, cached_fm_pair *pair, uint *head)
{
    if (dir->fmcache.mdata + pair->index != pair)
        return_error(gs_error_unregistered); /* Must not happen. */
    if (pair->next == pair->index) {
        /* The list consists of single element. */
        if (pair->prev != pair->index)
            return_error(gs_error_unregistered); /* Must not happen. */
        *head = dir->fmcache.mmax;
    } else {
        cached_fm_pair *next = dir->fmcache.mdata + pair->next;
        cached_fm_pair *prev = dir->fmcache.mdata + pair->prev;

        if (next->prev != pair->index)
            return_error(gs_error_unregistered); /* Must not happen. */
        if (prev->next != pair->index)
            return_error(gs_error_unregistered); /* Must not happen. */
        if (*head == pair->index)
            *head = next->index;
        next->prev = prev->index;
        prev->next = next->index;
    }
    return 0;
}

static int
fm_pair_insert_into_list(gs_font_dir * dir, cached_fm_pair *pair, uint *head)
{
    if (dir->fmcache.mdata + pair->index != pair)
        return_error(gs_error_unregistered); /* Must not happen. */
    if (*head >= dir->fmcache.mmax) {
        *head = pair->next = pair->prev = pair->index;
    } else {
        cached_fm_pair *first = dir->fmcache.mdata + *head;
        cached_fm_pair *last = dir->fmcache.mdata + first->prev;

        if (first->prev != last->index)
            return_error(gs_error_unregistered); /* Must not happen. */
        if (last->next != first->index)
            return_error(gs_error_unregistered); /* Must not happen. */
        pair->next = first->index;
        pair->prev = last->index;
        first->prev = last->next = pair->index;
        *head = pair->index;
    }
    return 0;
}

/* ====== Font-level routines ====== */

static int
gx_attach_tt_interpreter(gs_font_dir * dir,
               gs_font_type42 *font, cached_fm_pair *pair,
               const gs_matrix * char_tm, const gs_log2_scale_point *log2_scale,
               bool design_grid)
{
    float cxx, cxy, cyx, cyy;
    gs_matrix m;
    int code;

    gx_compute_char_matrix(char_tm, log2_scale, &cxx, &cxy, &cyx, &cyy);
    pair->design_grid = design_grid;
    m.xx = cxx;
    m.xy = cxy;
    m.yx = cyx;
    m.yy = cyy;
    m.tx = m.ty = 0;
    pair->ttr = gx_ttfReader__create(dir->memory->stable_memory);
    if (!pair->ttr)
        return_error(gs_error_VMerror);
    /*  We could use a single the reader instance for all fonts ... */
    pair->ttf = ttfFont__create(dir);
    if (!pair->ttf)
        return_error(gs_error_VMerror);
    gx_ttfReader__set_font(pair->ttr, (gs_font_type42 *)font);
    code = ttfFont__Open_aux(pair->ttf, dir->tti, pair->ttr,
                (gs_font_type42 *)font, &m, log2_scale, design_grid);
    gx_ttfReader__set_font(pair->ttr, NULL);
    return code;
}

static inline bool
does_font_need_tt_interpreter(gs_font *font)
{
    if (font->FontType == ft_TrueType || font->FontType == ft_CID_TrueType) {
        gs_font_type42 *pfont = (gs_font_type42 *)font;

        if (pfont->FAPI==NULL)
            return true;
    }
    return false;
}

int
gx_provide_fm_pair_attributes(gs_font_dir * dir,
               gs_font *font, cached_fm_pair *pair,
               const gs_matrix * char_tm, const gs_log2_scale_point *log2_scale,
               bool design_grid)
{
    if (does_font_need_tt_interpreter(font)) {
        if (pair->ttf != NULL)
            return 0; /* Already attached. */
        return gx_attach_tt_interpreter(dir, (gs_font_type42 *)font, pair,
                        char_tm, log2_scale, design_grid);
    }
    return 0;
}

/* Add a font/matrix pair to the cache. */
/* (This is only exported for gxccache.c.) */
int
gx_add_fm_pair(register gs_font_dir * dir, gs_font * font, const gs_uid * puid,
               const gs_matrix * char_tm, const gs_log2_scale_point *log2_scale,
               bool design_grid, cached_fm_pair **ppair)
{
    float mxx, mxy, myx, myy;
    register cached_fm_pair *pair;
    int code;

    gx_compute_ccache_key(font, char_tm, log2_scale, design_grid,
                            &mxx, &mxy, &myx, &myy);
    if (dir->fmcache.msize == dir->fmcache.mmax) {
        /* cache is full, drop the older entry. */
        /* gx_touch_fm_pair must be called whenever
           a pair is used to move it to the top of the list.
           Since we drop a pair from the list bottom,
           and since the list is long enough,
           with a high probability it won't drop a pair,
           which currently is pointed by an active text enumerator.

           Note that with Type 3 fonts multiple text enumerators
           may be active (exist on estack) in same time,
           therefore the list length sets a constraint for
           the number of font-matrix pairs used within a charproc.
           If it uses too many ones, the outer text enumerator
           will fail with 'invalidfont' in gx_add_cached_char.
        */
        pair = dir->fmcache.mdata + dir->fmcache.used;
        pair = dir->fmcache.mdata + pair->prev; /* last touched. */
        code = gs_purge_fm_pair(dir, pair, 0);
        if (code < 0)
            return code;
    }
    if (dir->fmcache.free < dir->fmcache.mmax) {
        /* use a free entry. */
        pair = dir->fmcache.mdata + dir->fmcache.free;
        code = fm_pair_remove_from_list(dir, pair, &dir->fmcache.free);
        if (code < 0)
            return code;
    } else {
        /* reserve a new entry. */
        pair = dir->fmcache.mdata + dir->fmcache.unused;
        dir->fmcache.unused++;
    }
    font->is_cached = true; /* Set this early to ensure
            gs_purge_font_from_char_caches works for it in case of errors. */
    dir->fmcache.msize++;
    code = fm_pair_insert_into_list(dir, pair, &dir->fmcache.used);
    if (code < 0)
        return code;
    pair->font = font;
    pair->UID = *puid;
    /* Copy UID into a stable memory,
       so that 'restore' may keep this pair. */
    code = uid_copy(&pair->UID, dir->memory->stable_memory, "gx_add_fm_pair");
    if (code < 0) {
        uid_set_invalid(&pair->UID);
        return code;
    }
    pair->FontType = font->FontType;
    pair->hash = (uint) (dir->hash % 549);	/* scramble bits */
    dir->hash += 371;
    pair->mxx = mxx, pair->mxy = mxy;
    pair->myx = myx, pair->myy = myy;
    pair->num_chars = 0;
    pair->xfont_tried = false;
    pair->xfont = 0;
    pair->ttf = 0;
    pair->ttr = 0;
    pair->design_grid = false;
    if (does_font_need_tt_interpreter(font)) {
            code = gx_attach_tt_interpreter(dir, (gs_font_type42 *)font, pair,
                                char_tm, log2_scale, design_grid);
            if (code < 0)
                return code;
    }
    else {
       if (font->FontType == ft_TrueType) {
           pair->design_grid = design_grid;
       }
    }
    pair->memory = 0;
    if_debug8m('k', dir->memory,
               "[k]adding pair "PRI_INTPTR": font="PRI_INTPTR" [%g %g %g %g] UID %ld, "PRI_INTPTR"\n",
               (intptr_t)pair, (intptr_t)font,
               pair->mxx, pair->mxy, pair->myx, pair->myy,
               (long)pair->UID.id, (intptr_t) pair->UID.xvalues);
    *ppair = pair;
    return 0;
}

/* Update the pointer to the last used font/matrix pair. */
int
gx_touch_fm_pair(gs_font_dir *dir, cached_fm_pair *pair)
{
    if (pair->index != dir->fmcache.used) {
        int code;

        code = fm_pair_remove_from_list(dir, pair, &dir->fmcache.used);
        if (code < 0)
            return code;
        return fm_pair_insert_into_list(dir, pair, &dir->fmcache.used);
    }
    return 0;
}

/* ------ Internal routines ------ */

/* Purge from the caches all references to a given font/matrix pair, */
/* or just characters that depend on its xfont. */
#define cpair ((cached_fm_pair *)vpair)
static bool
purge_fm_pair_char(const gs_memory_t *mem, cached_char * cc, void *vpair)
{
    return cc_pair(cc) == cpair;
}
#undef cpair

static inline void
gs_clean_fm_pair_attributes(gs_font_dir * dir, cached_fm_pair * pair)
{
    if (pair->ttr)
        gx_ttfReader__destroy(pair->ttr);
    pair->ttr = 0;
    if (pair->ttf)
        ttfFont__destroy(pair->ttf, dir);
    pair->ttf = 0;
}

void
gs_clean_fm_pair(gs_font_dir * dir, cached_fm_pair * pair)
{
    if_debug1m('k', dir->memory, "[k]cleaning pair "PRI_INTPTR"\n", (intptr_t) pair);
    pair->font = NULL;
    gs_clean_fm_pair_attributes(dir, pair);
}

int
gs_purge_fm_pair(gs_font_dir * dir, cached_fm_pair * pair, int xfont_only)
{
    if_debug2m('k', dir->memory, "[k]purging pair "PRI_INTPTR"%s\n",
               (intptr_t)pair, (xfont_only ? " (xfont only)" : ""));
    if (pair->xfont != 0) {
        (*pair->xfont->common.procs->release) (pair->xfont,
                                               pair->memory);
        pair->xfont_tried = false;
        pair->xfont = 0;
    }
    gx_purge_selected_cached_chars(dir,
                                    purge_fm_pair_char,
                                   pair);
    gs_clean_fm_pair_attributes(dir, pair);
    if (!xfont_only) {
        int code;

#ifdef DEBUG
        if (pair->num_chars != 0) {
            lprintf1("Error in gs_purge_fm_pair: num_chars =%d\n",
                     pair->num_chars);
        }
#endif
        if (uid_is_XUID(&pair->UID)) {
            gs_free_object(dir->memory->stable_memory, pair->UID.xvalues, "gs_purge_fm_pair");
            pair->UID.id = 0;
            pair->UID.xvalues = NULL;
        }

        fm_pair_set_free(pair);
        code = fm_pair_remove_from_list(dir, pair, &dir->fmcache.used);
        if (code < 0)
            return code;
        code = fm_pair_insert_into_list(dir, pair, &dir->fmcache.free);
        if (code < 0)
            return code;
        dir->fmcache.msize--;
    }
    return 0;
}


/* ====== Character-level routines ====== */

/*
 * Allocate storage for caching a rendered character with possible
 * oversampling and/or alpha.  Return the cached_char if OK, 0 if too big.
 * If the character is being oversampled, make the size decision
 * on the basis of the final (scaled-down) size.
 *
 * The iwidth and iheight parameters include scaling up for oversampling
 * (multiplication by 1 << pscale->{x,y}.)
 * The depth parameter is the final number of alpha bits;
 * depth <= x scale * y scale.
 */
int
gx_alloc_char_bits(gs_font_dir * dir, gx_device_memory * pdev,
                   ushort iwidth, ushort iheight,
                   const gs_log2_scale_point * pscale, int depth, cached_char **pcc)
{
    int log2_xscale = pscale->x;
    int log2_yscale = pscale->y;
    int log2_depth = ilog2(depth);
    size_t nwidth_bits = ((size_t)iwidth >> log2_xscale) << log2_depth;
    size_t isize, icdsize;
#ifdef ENABLE_IMPOSSIBLE_ALPHA_CODE
    ulong isize2;
#endif
    size_t iraster;
    cached_char *cc;
    float HWResolution0 = 72, HWResolution1 = 72;  /* default for dev == NULL */
    int code;

    *pcc = 0;
    HWResolution0 = pdev->HWResolution[0];
    HWResolution1 = pdev->HWResolution[1];

    /* Compute the scaled-down bitmap size, and test against */
    /* the maximum cachable character size. */

    iraster = bitmap_raster(nwidth_bits);
    if (iraster != 0 && iheight >> log2_yscale > dir->ccache.upper / iraster) {
        if_debug5m('k', pdev->memory, "[k]no cache bits: scale=%dx%d, raster/scale=%u, height/scale=%u, upper=%u\n",
                   1 << log2_xscale, 1 << log2_yscale,
                   (unsigned int)iraster, iheight, dir->ccache.upper);
        return 0;		/* too big */
    }
    /* Compute the actual bitmap size(s) and allocate the bits. */
    {
        /*
         * Render to a full (possibly oversampled) bitmap; compress
         * (if needed) when done.
         *
         * HACK: Preserve the reference count and retained flag.
         */
        rc_header rc;
        bool retained = pdev->retained;
        gx_device *target = pdev->target;

        rc = pdev->rc;
        /* Pass the correct target, but decrement its refct afterwards. */
        gs_make_mem_mono_device(pdev, pdev->memory, target);
        rc_decrement_only(target, "gx_alloc_char_bits"); /* can't go to 0 */
        pdev->rc = rc;
        pdev->retained = retained;
        pdev->width = iwidth;
        pdev->height = iheight;
        pdev->raster = gx_device_raster((gx_device *)pdev, 1);
        gdev_mem_bitmap_size(pdev, &isize);	/* Assume less than max_ulong */
        pdev->HWResolution[0] = HWResolution0;
        pdev->HWResolution[1] = HWResolution1;
    }
    icdsize = isize + sizeof_cached_char;
    code = alloc_char(dir, icdsize, &cc);
    if (code < 0)
        return code;
    *pcc = cc;
    if (cc == 0)
        return 0;
    if_debug4m('k', pdev->memory, "[k]adding char "PRI_INTPTR":%u(%u,%u)\n",
               (intptr_t)cc, (uint)icdsize, iwidth, iheight);

    /* Fill in the entry. */

    cc_set_depth(cc, depth);
    cc->xglyph = gx_no_xglyph;
    /* Set the width and height to those of the device. */
    /* Note that if we are oversampling without an alpha buffer. */
    /* these are not the final unscaled dimensions. */
    cc->width = pdev->width;
    cc->height = pdev->height;
    cc->shift = 0;
    cc_set_raster(cc, gdev_mem_raster(pdev));
    cc_set_pair_only(cc, 0);	/* not linked in yet */
    cc->id = gx_no_bitmap_id;
    cc->subpix_origin.x = cc->subpix_origin.y = 0;
    cc->linked = false;

    /* Open the cache device(s). */
    gx_open_cache_device(pdev, cc);

    return 0;
}

/* Open the cache device. */
void
gx_open_cache_device(gx_device_memory * dev, cached_char * cc)
{
    byte *bits = cc_bits(cc);
    size_t bsize;

    gdev_mem_bitmap_size(dev, &bsize);

    dev->width = cc->width;
    dev->height = cc->height;
    memset((char *)bits, 0, bsize);
    dev->base = bits;
    (*dev_proc(dev, open_device)) ((gx_device *) dev);	/* initialize */
}

/* Remove a character from the cache. */
void
gx_free_cached_char(gs_font_dir * dir, cached_char * cc)
{
    char_cache_chunk *cck = cc->chunk;

    dir->ccache.chunks = cck;
    dir->ccache.cnext = (byte *) cc - cck->data;
    if (cc->linked)
        cc_pair(cc)->num_chars--;
    if_debug2m('k', dir->memory, "[k]freeing char "PRI_INTPTR", pair="PRI_INTPTR"\n",
               (intptr_t)cc, (intptr_t)cc_pair(cc));
    gx_bits_cache_free((gx_bits_cache *) & dir->ccache, &cc->head, cck);
}

/* Add a character to the cache */
int
gx_add_cached_char(gs_font_dir * dir, gx_device_memory * dev,
cached_char * cc, cached_fm_pair * pair, const gs_log2_scale_point * pscale)
{
    if_debug5m('k', dev->memory,
               "[k]chaining char "PRI_INTPTR": pair="PRI_INTPTR", glyph=0x%lx, wmode=%d, depth=%d\n",
               (intptr_t)cc, (intptr_t)pair, (ulong)cc->code,
               cc->wmode, cc_depth(cc));
    if (dev != NULL) {
        static const gs_log2_scale_point no_scale =
        {0, 0};

        /* Close the device, to flush the alpha buffer if any. */
        (*dev_proc(dev, close_device)) ((gx_device *) dev);
        gx_add_char_bits(dir, cc,
                         (gs_device_is_abuf((gx_device *) dev) ?
                          &no_scale : pscale));
    }
    /* Add the new character to the hash table. */
    {
        uint chi = chars_head_index(cc->code, pair);

        while (dir->ccache.table[chi &= dir->ccache.table_mask] != 0)
            chi++;
        dir->ccache.table[chi] = cc;
        if (cc->pair == NULL) {
            /* gx_show_text_retry could reset it when bbox_draw
               discovered an insufficient FontBBox and enlarged it.
               Glyph raster params could change then. */
            cc->pair = pair;
        } else if (cc->pair != pair) {
            /* gx_add_fm_pair could drop the active font-matrix pair
               due to cache overflow during a charproc interpretation.
               Likely a single charproc renders too many characters
               for generating the character image.
               We have no mechanizm for locking font-matrix pairs in cache
               to avoud their dissipation. Therefore we consider this failure
               as an implementation limitation. */
            return_error(gs_error_invalidfont);
        }
        cc->linked = true;
        cc_set_pair(cc, pair);
        pair->num_chars++;
    }
    return 0;
}

/* Adjust the bits of a newly-rendered character, by unscaling */
/* and compressing or converting to alpha values if necessary. */
void
gx_add_char_bits(gs_font_dir * dir, cached_char * cc,
                 const gs_log2_scale_point * plog2_scale)
{
    int log2_x = plog2_scale->x, log2_y = plog2_scale->y;
    uint raster = cc_raster(cc);
    byte *bits = cc_bits(cc);
    int depth = cc_depth(cc);
    int log2_depth = ilog2(depth);
    uint nwidth_bits, nraster;
    gs_int_rect bbox;

#ifdef DEBUG
    if (cc->width % (1 << log2_x) != 0 ||
        cc->height % (1 << log2_y) != 0
        ) {
        lprintf4("size %d,%d not multiple of scale %d,%d!\n",
                 cc->width, cc->height,
                 1 << log2_x, 1 << log2_y);
        cc->width &= -1 << log2_x;
        cc->height &= -1 << log2_y;
    }
#endif

    /*
     * Compute the bounding box before compressing.
     * We may have to scan more bits, but this is a lot faster than
     * compressing the white space.  Note that all bbox values are
     * in bits, not pixels.
     */

    bits_bounding_box(bits, cc->height, raster, &bbox);

    /*
     * If the character was oversampled, compress it now.
     * In this case we know that log2_depth <= log2_x.
     * If the character was not oversampled, or if we converted
     * oversampling to alpha dynamically (using an alpha buffer
     * intermediate device), log2_x and log2_y are both zero,
     * but in the latter case we may still have depth > 1.
     */

    if ((log2_x | log2_y) != 0) {
        if_debug5m('k', dir->memory,
                   "[k]compressing %dx%d by %dx%d to depth=%d\n",
                   cc->width, cc->height, 1 << log2_x, 1 << log2_y,
                   depth);
#ifdef DEBUG
        if (gs_debug_c('K'))
            debug_dump_bitmap(dir->memory, bits, raster, cc->height,
                              "[K]uncompressed bits");
#endif
        /* Truncate/round the bbox to a multiple of the scale. */
        {
            int scale_x = 1 << log2_x;

            bbox.p.x &= -scale_x;
            bbox.q.x = (bbox.q.x + scale_x - 1) & -scale_x;
        }
        {
            int scale_y = 1 << log2_y;

            bbox.p.y &= -scale_y;
            bbox.q.y = (bbox.q.y + scale_y - 1) & -scale_y;
        }
        cc->width = (bbox.q.x - bbox.p.x) >> log2_x;
        cc->height = (bbox.q.y - bbox.p.y) >> log2_y;
        nwidth_bits = cc->width << log2_depth;
        nraster = bitmap_raster(nwidth_bits);
        bits_compress_scaled(bits + raster * bbox.p.y, bbox.p.x,
                             cc->width << log2_x,
                             cc->height << log2_y,
                             raster,
                             bits, nraster, plog2_scale, log2_depth);
        bbox.p.x >>= log2_x;
        bbox.p.y >>= log2_y;
    } else {
        /* No oversampling, just remove white space on all 4 sides. */
        const byte *from = bits + raster * bbox.p.y + (bbox.p.x >> 3);

        cc->height = bbox.q.y - bbox.p.y;
        bbox.p.x &= ~7;		/* adjust to byte boundary */
        bbox.p.x >>= log2_depth;	/* bits => pixels */
        bbox.q.x = (bbox.q.x + depth - 1) >> log2_depth;	/* ditto */
        cc->width = bbox.q.x - bbox.p.x;
        nwidth_bits = cc->width << log2_depth;
        nraster = bitmap_raster(nwidth_bits);
        if (bbox.p.x != 0 || nraster != raster) {
            /* Move the bits down and over. */
            byte *to = bits;
            uint n = cc->height;

            /* We'd like to move only
               uint nbytes = (nwidth_bits + 7) >> 3;
               * bytes per scan line, but unfortunately this drops
               * the guaranteed zero padding at the end.
             */

            for (; n--; from += raster, to += nraster)
                memmove(to, from, /*nbytes */ nraster);
        } else if (bbox.p.y != 0) {	/* Just move the bits down. */
            memmove(bits, from, raster * cc->height);
        }
    }

    /* Adjust the offsets to account for removed white space. */

    cc->offset.x -= int2fixed(bbox.p.x);
    cc->offset.y -= int2fixed(bbox.p.y);

    /* Discard the memory device overhead that follows the bits, */
    /* and any space reclaimed from unscaling or compression. */

    cc_set_raster(cc, nraster);
    {
        uint diff = ROUND_DOWN(cc->head.size - sizeof_cached_char -
                               nraster * cc->height,
                               align_cached_char_mod);

        if (diff >= sizeof(cached_char_head)) {
            shorten_cached_char(dir, cc, diff);
            if_debug2m('K', dir->memory, "[K]shortening char "PRI_INTPTR" by %u (adding)\n",
                       (intptr_t)cc, diff);
        }
    }

    /* Assign a bitmap id. */

    cc->id = gs_next_ids(dir->memory, 1);
}

/* Purge from the caches all references to a given font. */
static int
gs_purge_font_from_char_caches_forced(gs_font * font, bool force)
{
    gs_font_dir * dir;
    cached_fm_pair *pair;
    int count;

    if (font->dir == NULL)
        return 0; /* The font was not properly build due to errors. */
    if (!font->is_cached)
        return 0;
    dir = font->dir;
    pair = dir->fmcache.mdata;
    count = dir->fmcache.mmax;
    font->is_cached = false; /* Prevent redundant execution. */
    if_debug1m('k', font->memory, "[k]purging font "PRI_INTPTR"\n",
               (intptr_t)font);
    for (; count--; pair++) {
        if (pair->font == font) {
            if (!force && uid_is_valid(&pair->UID)) {	/* Keep the entry. */
                gs_clean_fm_pair(dir, pair);
            } else {
                int code = gs_purge_fm_pair(dir, pair, 0);

                if (code < 0)
                    return code;
            }
        }
    }
    return 0;
}

/* Purge from the caches all references to a given font,
   with leaving persistent chars in the cache. */
int
gs_purge_font_from_char_caches(gs_font * font)
{
    /* This function is called when a font is being released.
       The purpose is to remove all cache attributes,
       which may point to the font data.
       Note : when a font has a valid XUID,
       it doesn't release cache entries and cached chars,
       so that they may be used in future
       if a font with same XUID appears again.
       All this improves the performance when
       a document executes a sequence like this :

       n {
          save /fontname findfont 10 scalefont
          (xyz) show
          restore
       } repeat
     */
    return gs_purge_font_from_char_caches_forced(font, false);
}

/* Purge from the caches all references to a given font,
   without leaving persistent chars in the cache. */
int
gs_purge_font_from_char_caches_completely(gs_font * font)
{
    /* A client should call this finction
       when it frees a font,
       and the client doesn't need to leave
       persistent cache entries for this font
       even if the font has a valid XUID.
     */
    return gs_purge_font_from_char_caches_forced(font, true);
}

/* ------ Internal routines ------ */

/* Allocate data space for a cached character, adding a new chunk if needed. */
static int
alloc_char(gs_font_dir * dir, ulong icdsize, cached_char **pcc)
{				/* Try allocating at the current position first. */
    cached_char *cc;
    int code = alloc_char_in_chunk(dir, icdsize, &cc);

    *pcc = cc;
    if (code < 0)
        return code;
    if (cc == 0) {
        if (dir->ccache.bspace < dir->ccache.bmax) {	/* Allocate another chunk. */
            gs_memory_t *mem = dir->ccache.bits_memory;
            char_cache_chunk *cck_prev = dir->ccache.chunks;
            char_cache_chunk *cck;
            uint cksize = ROUND_UP(dir->ccache.bmax / 5 + 1, obj_align_mod);
            uint tsize = ROUND_UP(dir->ccache.bmax - dir->ccache.bspace, obj_align_mod);
            byte *cdata;

            if (cksize > tsize)
                cksize = tsize;
            if (icdsize + sizeof(cached_char_head) > cksize) {
                if_debug2m('k', mem,
                           "[k]no cache bits: cdsize+head=%lu, cksize=%u\n",
                           icdsize + sizeof(cached_char_head),
                           cksize);
                return 0;	/* wouldn't fit */
            }
            cck = (char_cache_chunk *)
                gs_alloc_bytes_immovable(mem, sizeof(*cck),
                                         "char cache chunk");
            if (cck == 0)
                return 0;
            cdata =
                gs_alloc_struct_array_immovable(mem, cksize, byte,
                                                &st_font_cache_bytes,
                                                "char cache chunk(data)");
            if (cdata == 0) {
                gs_free_object(mem, cck, "char cache chunk");
                return 0;
            }
            gx_bits_cache_chunk_init(cck, cdata, cksize);
            cck->next = cck_prev->next;
            cck_prev->next = cck;
            dir->ccache.bspace += cksize;
            dir->ccache.chunks = cck;
        } else {		/* Cycle through existing chunks. */
            char_cache_chunk *cck_init = dir->ccache.chunks;
            char_cache_chunk *cck = cck_init;

            while ((dir->ccache.chunks = cck = cck->next) != cck_init) {
                dir->ccache.cnext = 0;
                code = alloc_char_in_chunk(dir, icdsize, &cc);
                if (code < 0)
                    return code;
                if (cc != 0) {
                    *pcc = cc;
                    return 0;
                }
            }
        }
        dir->ccache.cnext = 0;
        code = alloc_char_in_chunk(dir, icdsize, &cc);
        if (code < 0)
            return code;
        *pcc = cc;
    }
    return 0;
}

/* Allocate a character in the current chunk. */
static int
alloc_char_in_chunk(gs_font_dir * dir, ulong icdsize, cached_char **pcc)
{
    char_cache_chunk *cck = dir->ccache.chunks;
    cached_char_head *cch;

#define cc ((cached_char *)cch)

    *pcc = 0;
    while (gx_bits_cache_alloc((gx_bits_cache *) & dir->ccache,
                               icdsize, &cch) < 0
        ) {
        if (cch == 0) {		/* Not enough room to allocate in this chunk. */
            return 0;
        }
        else {			/* Free the character */
            cached_fm_pair *pair = cc_pair(cc);

            if (pair != 0) {
                uint chi = chars_head_index(cc->code, pair);
                uint cnt = dir->ccache.table_mask + 1;

                while (dir->ccache.table[chi & dir->ccache.table_mask] != cc) {
                    chi++;
                    if (cnt-- == 0)
                        return_error(gs_error_unregistered); /* Must not happen. */
                }
                hash_remove_cached_char(dir, chi);
            }

            gx_free_cached_char(dir, cc);
        }
    }

    cc->chunk = cck;
    cc->loc = (byte *) cc - cck->data;
    *pcc = cc;
    return 0;

#undef cc
}

/* Remove the cached_char at a given index in the hash table. */
/* In order not to slow down lookup, we relocate following entries. */
static void
hash_remove_cached_char(gs_font_dir * dir, uint chi)
{
    uint mask = dir->ccache.table_mask;
    uint from = ((chi &= mask) + 1) & mask;
    cached_char *cc;

    dir->ccache.table[chi] = 0;
    while ((cc = dir->ccache.table[from]) != 0) {	/* Loop invariants: chars[chi] == 0; */
        /* chars[chi+1..from] != 0. */
        uint fchi = chars_head_index(cc->code, cc_pair(cc));

        /* If chi <= fchi < from, we relocate the character. */
        /* Note that '<=' must take wraparound into account. */
        if ((chi < from ? chi <= fchi && fchi < from :
             chi <= fchi || fchi < from)
            ) {
            dir->ccache.table[chi] = cc;
            dir->ccache.table[from] = 0;
            chi = from;
        }
        from = (from + 1) & mask;
    }
}

/* Shorten a cached character. */
/* diff >= sizeof(cached_char_head). */
static void
shorten_cached_char(gs_font_dir * dir, cached_char * cc, uint diff)
{
    gx_bits_cache_shorten((gx_bits_cache *) & dir->ccache, &cc->head,
                          diff, cc->chunk);
    if_debug2m('K', dir->memory, "[K]shortening creates free block "PRI_INTPTR"(%u)\n",
              (intptr_t)((byte *) cc + cc->head.size), diff);
}
