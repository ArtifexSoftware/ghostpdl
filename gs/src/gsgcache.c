/* Copyright (C) 1996, 2000 Aladdin Enterprises.  All rights reserved.
  
  This software is provided AS-IS with no warranty, either express or
  implied.
  
  This software is distributed under license and may not be copied,
  modified or distributed except as expressly authorized under the terms
  of the license contained in the file LICENSE in this distribution.
  
  For more information about licensing, please refer to
  http://www.ghostscript.com/licensing/. For information on
  commercial licensing, go to http://www.artifex.com/licensing/ or
  contact Artifex Software, Inc., 101 Lucas Valley Road #110,
  San Rafael, CA  94903, U.S.A., +1(415)492-9861.
*/

/* $Id$ */
/* Glyph data cache methods. */

#include "gx.h"
#include "gserrors.h"
#include "memory_.h"
#include "gsstruct.h"
#include "gsgdata.h"
#include "gsgcache.h"
#include "gxfont.h"
#include "gxfont42.h"

/* 
 * This implementation hardcodes the type 42 font type.
 * We could generalize it, but since CIDFontType 0 uses
 * a PS procedure for reading glyphs, it is hardly applicable.
 * 
 * The caching is mostly useful for glyphs with multiple components,
 * but CIDFontType 0 has 2 components max, which are relatively seldom.
 * Also it is low useful for fonts, which fully loaded into RAM.
 * FAPI does not need a caching, because renderer pludins access 
 * font data through a file handle by own means.
 *
 * Due to all above, currently the caching is applied
 * only while emulating CIDFontType 2 with a True Type file.
 */

typedef struct gs_glyph_cache_elem_s gs_glyph_cache_elem;
struct gs_glyph_cache_elem_s {
    gs_glyph_data_t gd;
    uint glyph_index;
    uint lock_count;
    gs_glyph_cache_elem *next;
};
gs_public_st_composite(st_glyph_cache_elem, gs_glyph_cache_elem, "gs_glyph_cache_elem",
    gs_glyph_cache_elem_enum_ptrs, gs_glyph_cache_elem_reloc_ptrs);

private 
ENUM_PTRS_WITH(gs_glyph_cache_elem_enum_ptrs, gs_glyph_cache_elem *e)
{
    index --;
    if (index < ST_GLYPH_DATA_NUM_PTRS)
	return ENUM_USING(st_glyph_data, &e->gd, sizeof(e->gd), index);
    return 0;
}
ENUM_PTR(0, gs_glyph_cache_elem, next);
ENUM_PTRS_END
private RELOC_PTRS_WITH(gs_glyph_cache_elem_reloc_ptrs, gs_glyph_cache_elem *e)
{
    RELOC_PTR(gs_glyph_cache_elem, next);
    RELOC_USING(st_glyph_data, &e->gd, sizeof(e->gd));
} RELOC_PTRS_END

struct gs_glyph_cache_s {
    int total_size;
    gs_glyph_cache_elem *list;
    gs_memory_t *memory;
    gs_font_type42 *pfont;
    stream *s;
    get_glyph_data_from_file read_data;
};
gs_private_st_ptrs4(st_glyph_cache, gs_glyph_cache, "gs_glyph_cache",
    gs_glyph_cache_enum_ptrs, gs_glyph_cache_reloc_ptrs, list, memory, pfont, s);



gs_glyph_cache *
gs_glyph_cache__alloc(gs_font_type42 *pfont, stream *s,
			get_glyph_data_from_file read_data)
{ 
    gs_glyph_cache *gdcache = (gs_glyph_cache *)gs_alloc_struct(pfont->memory,
	    gs_glyph_cache, &st_glyph_cache, "gs_glyph_cache");
    if (gdcache == 0)
	return 0;
    gdcache->total_size = 0;
    gdcache->list = NULL;
    gdcache->pfont = pfont;
    gdcache->s = s;
    gdcache->memory = pfont->memory;
    gdcache->read_data = read_data;
    return gdcache;
}

void
gs_glyph_cache__release(gs_glyph_cache *this)
{   gs_glyph_cache_elem *e = this->list;
    
    while (e != NULL) {
	e->gd.procs->free(&e->gd, "gs_glyph_cache__release");
	gs_free_object(this->memory, e, "gs_glyph_cache_elem__release");
    }
    this->list = NULL; 
}

private gs_glyph_cache_elem **
gs_glyph_cache_elem__locate(gs_glyph_cache *this, uint glyph_index)
{   /* If not fond, returns an unlocked element. */ 
    gs_glyph_cache_elem **e = &this->list, **p_unlocked = NULL;
    int count = 0; /* debug purpose only */

    for (; *e != 0; e = &(*e)->next, count++) {
	if ((*e)->glyph_index == glyph_index)
	    return e;
	if ((*e)->lock_count == 0)
	    p_unlocked = e;
    }
    return p_unlocked;
}

private inline void
gs_glyph_cache_elem__move_to_head(gs_glyph_cache *this, gs_glyph_cache_elem **pe)
{   gs_glyph_cache_elem *e = *pe;

    *pe = e->next;
    e->next = this->list;
    this->list = e;
}

/* Manage the glyph data using the font's allocator. */
private void
gs_glyph_cache_elem__free_data(gs_glyph_data_t *pgd, client_name_t cname)
{   gs_glyph_cache_elem *e = (gs_glyph_cache_elem *)pgd->proc_data;

    e->lock_count--;
}
private int
gs_glyph_cache_elem__substring(gs_glyph_data_t *pgd, uint offset, uint size)
{   gs_glyph_cache_elem *e = (gs_glyph_cache_elem *)pgd->proc_data;

    e->lock_count++;
    return_error(gs_error_unregistered); /* Unsupported; should not happen. */
}			       

private const gs_glyph_data_procs_t gs_glyph_cache_elem_procs = {
    gs_glyph_cache_elem__free_data, gs_glyph_cache_elem__substring
};

int 
gs_get_glyph_data_cached(gs_font_type42 *pfont, uint glyph_index, gs_glyph_data_t *pgd)
{   gs_glyph_cache *gdcache = pfont->data.gdcache;
    gs_glyph_cache_elem **pe = gs_glyph_cache_elem__locate(gdcache, glyph_index);
    gs_glyph_cache_elem *e = NULL;

    if (pe == NULL || (*pe)->glyph_index != glyph_index) {
	int code;

	if (pe != NULL && gdcache->total_size > 32767 /* arbitrary */ && 
			  (*pe)->lock_count <= 0) {
	    /* Release the element's data, and move it : */
	    e = *pe;
	    gdcache->total_size -= e->gd.bits.size + sizeof(*e);
	    e->gd.procs->free(&e->gd, "gs_get_glyph_data_cached");
	    gs_glyph_cache_elem__move_to_head(gdcache, pe);
	} else {
	    /* Allocate new head element : */
	    e = (gs_glyph_cache_elem *)gs_alloc_struct(gdcache->memory,
		gs_glyph_cache_elem, &st_glyph_cache_elem, "gs_glyph_cache_elem");
	    if (e == NULL)
		return_error(gs_error_VMerror);
	    memset(e, 0, sizeof(*e));
	    e->next = gdcache->list;
	    gdcache->list = e;
	}
        /* Load the element's data : */
	code = (*gdcache->read_data)(pfont, gdcache->s, glyph_index, &e->gd);
	if (code < 0)
	    return code;
        gdcache->total_size += e->gd.bits.size + sizeof(*e);
	e->glyph_index = glyph_index;
    } else {
        /* Move the element : */
	e = *pe;
	gs_glyph_cache_elem__move_to_head(gdcache, pe);
    }
    /* Copy data and set procs : */
    pgd->bits = e->gd.bits;
    pgd->proc_data = e;
    pgd->procs = &gs_glyph_cache_elem_procs;
    e->lock_count++;
    return 0;
}




