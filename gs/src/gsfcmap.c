/* Copyright (C) 1997, 2000 Aladdin Enterprises.  All rights reserved.
  
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
/* CMap character decoding */
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsstruct.h"
#include "gsutil.h"		/* for gs_next_ids */
#include "gxfcmap.h"

typedef struct gs_cmap_identity_s {
    GS_CMAP_COMMON;
    int num_bytes;
    int varying_bytes;
    int code;			/* 0 or num_bytes */
} gs_cmap_identity_t;

/* GC descriptors */
public_st_cmap();
gs_private_st_suffix_add0_local(st_cmap_identity, gs_cmap_identity_t,
				"gs_cmap_identity_t", cmap_ptrs, cmap_data,
				st_cmap);

/* ---------------- Client procedures ---------------- */

    /* ------ Initialization/creation ------ */

/*
 * Create an Identity CMap.
 */
private uint
get_integer_bytes(const byte *src, int count)
{
    uint v = 0;
    int i;

    for (i = 0; i < count; ++i)
	v = (v << 8) + src[i];
    return v;
}
private int
identity_decode_next(const gs_cmap_t *pcmap, const gs_const_string *str,
		     uint *pindex, uint *pfidx,
		     gs_char *pchr, gs_glyph *pglyph)
{
    const gs_cmap_identity_t *const pcimap =
	(const gs_cmap_identity_t *)pcmap;
    int num_bytes = pcimap->num_bytes;
    uint value;

    if (str->size < *pindex + num_bytes) {
	*pglyph = gs_no_glyph;
	return (*pindex == str->size ? 2 : -1);
    }
    value = get_integer_bytes(str->data + *pindex, num_bytes);
    *pglyph = gs_min_cid_glyph + value;
    *pchr = value;
    *pindex += num_bytes;
    *pfidx = 0;
    return pcimap->code;
}
private int
identity_next_range(gs_cmap_ranges_enum_t *penum)
{
    if (penum->index == 0) {
	const gs_cmap_identity_t *const pcimap =
	    (const gs_cmap_identity_t *)penum->cmap;

	memset(penum->range.first, 0, pcimap->num_bytes);
	memset(penum->range.last, 0xff, pcimap->num_bytes);
	penum->index = 1;
	return 0;
    }
    return 1;
}
private const gs_cmap_ranges_enum_procs_t identity_range_procs = {
    identity_next_range
};
private void
identity_enum_ranges(const gs_cmap_t *pcmap, gs_cmap_ranges_enum_t *pre)
{
    gs_cmap_ranges_enum_setup(pre, pcmap, &identity_range_procs);
}
private int
identity_next_lookup(gs_cmap_lookups_enum_t *penum)
{
    if (penum->index[0] == 0) {
	const gs_cmap_identity_t *const pcimap =
	    (const gs_cmap_identity_t *)penum->cmap;
	int num_bytes = pcimap->num_bytes;

	memset(penum->entry.key[0], 0, num_bytes);
	memset(penum->entry.key[1], 0xff, num_bytes);
	memset(penum->entry.key[1], 0, num_bytes - pcimap->varying_bytes);
	penum->entry.key_size = num_bytes;
	penum->entry.key_is_range = true;
	penum->entry.value_type =
	    (pcimap->code ? CODE_VALUE_CHARS : CODE_VALUE_CID);
	penum->entry.value.size = num_bytes;
	penum->entry.font_index = 0;
	penum->index[0] = 1;
	return 0;
    }
    return 1;
}
private int
no_next_lookup(gs_cmap_lookups_enum_t *penum)
{
    return 1;
}
private int
identity_next_entry(gs_cmap_lookups_enum_t *penum)
{
    const gs_cmap_identity_t *const pcimap =
	(const gs_cmap_identity_t *)penum->cmap;
    int num_bytes = pcimap->num_bytes;
    int i = num_bytes - pcimap->varying_bytes;

    memcpy(penum->temp_value, penum->entry.key[0], num_bytes);
    memcpy(penum->entry.key[0], penum->entry.key[1], i);
    while (--i >= 0)
	if (++(penum->entry.key[1][i]) != 0) {
	    penum->entry.value.data = penum->temp_value;
	    return 0;
	}
    return 1;
}

private const gs_cmap_lookups_enum_procs_t identity_lookup_procs = {
    identity_next_lookup, identity_next_entry
};
const gs_cmap_lookups_enum_procs_t gs_cmap_no_lookups_procs = {
    no_next_lookup, 0
};
private void
identity_enum_lookups(const gs_cmap_t *pcmap, int which,
		      gs_cmap_lookups_enum_t *pre)
{
    gs_cmap_lookups_enum_setup(pre, pcmap,
			       (which ? &gs_cmap_no_lookups_procs :
				&identity_lookup_procs));
}

private const gs_cmap_procs_t identity_procs = {
    identity_decode_next, identity_enum_ranges, identity_enum_lookups
};

private int
gs_cmap_identity_alloc(gs_cmap_t **ppcmap, int num_bytes, int varying_bytes,
		       int return_code, const char *cmap_name, int wmode,
		       gs_memory_t *mem)
{
    /*
     * We could allow any value of num_bytes between 1 and
     * min(MAX_CMAP_CODE_SIZE, 4), but if num_bytes != 2, we can't name
     * the result "Identity-[HV]".
     */
    static const gs_cid_system_info_t identity_cidsi = {
	{ (const byte *)"Adobe", 5 },
	{ (const byte *)"Identity", 8 },
	0
    };
    int code;
    gs_cmap_identity_t *pcimap;

    if (num_bytes != 2)
	return_error(gs_error_rangecheck);
    code = gs_cmap_alloc(ppcmap, &st_cmap_identity, wmode,
			 (const byte *)cmap_name, strlen(cmap_name),
			 &identity_cidsi, 1, &identity_procs, mem);
    if (code < 0)
	return code;
    pcimap = (gs_cmap_identity_t *)*ppcmap;
    pcimap->num_bytes = num_bytes;
    pcimap->varying_bytes = varying_bytes;
    pcimap->code = return_code;
    return 0;
}
int
gs_cmap_create_identity(gs_cmap_t **ppcmap, int num_bytes, int wmode,
			gs_memory_t *mem)
{
    return gs_cmap_identity_alloc(ppcmap, num_bytes, num_bytes, 0,
				  (wmode ? "Identity-V" : "Identity-H"),
				  wmode, mem);
}
int
gs_cmap_create_char_identity(gs_cmap_t **ppcmap, int num_bytes, int wmode,
			     gs_memory_t *mem)
{
    return gs_cmap_identity_alloc(ppcmap, num_bytes, 1, num_bytes,
				  (wmode ? "Identity-BF-V" : "Identity-BF-H"),
				  wmode, mem);
}

    /* ------ Decoding ------ */

/*
 * Decode and map a character from a string using a CMap.
 * See gsfcmap.h for details.
 */
int
gs_cmap_decode_next(const gs_cmap_t *pcmap, const gs_const_string *str,
		    uint *pindex, uint *pfidx,
		    gs_char *pchr, gs_glyph *pglyph)
{
    return pcmap->procs->decode_next(pcmap, str, pindex, pfidx, pchr, pglyph);
}

    /* ------ Enumeration ------ */

/*
 * Initialize the enumeration of the code space ranges, and enumerate
 * the next range.  See gxfcmap.h for details.
 */
void
gs_cmap_ranges_enum_init(const gs_cmap_t *pcmap, gs_cmap_ranges_enum_t *penum)
{
    pcmap->procs->enum_ranges(pcmap, penum);
}
int
gs_cmap_enum_next_range(gs_cmap_ranges_enum_t *penum)
{
    return penum->procs->next_range(penum);
}

/*
 * Initialize the enumeration of the lookups, and enumerate the next
 * the next lookup or entry.  See gxfcmap.h for details.
 */
void
gs_cmap_lookups_enum_init(const gs_cmap_t *pcmap, int which,
			  gs_cmap_lookups_enum_t *penum)
{
    pcmap->procs->enum_lookups(pcmap, which, penum);
}
int
gs_cmap_enum_next_lookup(gs_cmap_lookups_enum_t *penum)
{
    return penum->procs->next_lookup(penum);
}
int
gs_cmap_enum_next_entry(gs_cmap_lookups_enum_t *penum)
{
    return penum->procs->next_entry(penum);
}

/* ---------------- Implementation procedures ---------------- */

    /* ------ Initialization/creation ------ */

/*
 * Initialize a just-allocated CMap, to ensure that all pointers are clean
 * for the GC.  Note that this only initializes the common part.
 */
void
gs_cmap_init(gs_cmap_t *pcmap)
{
    memset(pcmap, 0, sizeof(*pcmap));
    pcmap->id = gs_next_ids(1);
    uid_set_invalid(&pcmap->uid);
}

/*
 * Allocate and initialize (the common part of) a CMap.
 */
int
gs_cmap_alloc(gs_cmap_t **ppcmap, const gs_memory_struct_type_t *pstype,
	      int wmode, const byte *map_name, uint name_size,
	      const gs_cid_system_info_t *pcidsi_in, int num_fonts,
	      const gs_cmap_procs_t *procs, gs_memory_t *mem)
{
    gs_cmap_t *pcmap =
	gs_alloc_struct(mem, gs_cmap_t, pstype, "gs_cmap_alloc(CMap)");
    gs_cid_system_info_t *pcidsi =
	gs_alloc_struct_array(mem, num_fonts, gs_cid_system_info_t,
			      &st_cid_system_info,
			      "gs_cmap_alloc(CIDSystemInfo)");

    if (pcmap == 0 || pcidsi == 0) {
	gs_free_object(mem, pcidsi, "gs_cmap_alloc(CIDSystemInfo)");
	gs_free_object(mem, pcmap, "gs_cmap_alloc(CMap)");
	return_error(gs_error_VMerror);
    }
    gs_cmap_init(pcmap);	/* id, uid */
    pcmap->CMapType = 1;
    pcmap->CMapName.data = map_name;
    pcmap->CMapName.size = name_size;
    if (pcidsi_in)
	memcpy(pcidsi, pcidsi_in, sizeof(*pcidsi) * num_fonts);
    else
	memset(pcidsi, 0, sizeof(*pcidsi) * num_fonts);
    pcmap->CIDSystemInfo = pcidsi;
    pcmap->num_fonts = num_fonts;
    pcmap->CMapVersion = 1.0;
    /* uid = 0, UIDOffset = 0 */
    pcmap->WMode = wmode;
    /* from_Unicode = 0 */
    /* not glyph_name, glyph_name_data */
    pcmap->procs = procs;
    *ppcmap = pcmap;
    return 0;
}

/*
 * Initialize an enumerator with convenient defaults (index = 0).
 */
void
gs_cmap_ranges_enum_setup(gs_cmap_ranges_enum_t *penum,
			  const gs_cmap_t *pcmap,
			  const gs_cmap_ranges_enum_procs_t *procs)
{
    penum->cmap = pcmap;
    penum->procs = procs;
    penum->index = 0;
}
void
gs_cmap_lookups_enum_setup(gs_cmap_lookups_enum_t *penum,
			   const gs_cmap_t *pcmap,
			   const gs_cmap_lookups_enum_procs_t *procs)
{
    penum->cmap = pcmap;
    penum->procs = procs;
    penum->index[0] = penum->index[1] = 0;
}
