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


/* Font copying for high-level output */
#include "memory_.h"
#include "gx.h"
#include <stdlib.h>		/* for qsort */
#include "gscencs.h"
#include "gserrors.h"
#include "gsline.h"		/* for BuildChar */
#include "gspaint.h"		/* for BuildChar */
#include "gspath.h"		/* for gs_moveto in BuildChar */
#include "gsstruct.h"
#include "gsutil.h"
#include "gschar.h"
#include "stream.h"
#include "gxfont.h"
#include "gxfont1.h"
#include "gxfont42.h"
#include "gxchar.h"
#include "gxfcid.h"
#include "gxfcopy.h"
#include "gxfcache.h"		/* for gs_font_dir_s */
#include "gxgstate.h"		/* for Type 1 glyph_outline */
#include "gxtext.h"		/* for BuildChar */
#include "gxtype1.h"		/* for Type 1 glyph_outline */
#include "gzstate.h"		/* for path for BuildChar */
#include "gdevpsf.h"
#include "smd5.h"		/* Create MD5 hash of Subrs when comparing fonts */

#define GLYPHS_SIZE_IS_PRIME 1 /* Old code = 0, new code = 1. */

/* ================ Types and structures ================ */

typedef struct gs_copied_glyph_s gs_copied_glyph_t;
typedef struct gs_copied_font_data_s gs_copied_font_data_t;

typedef struct gs_copied_font_procs_s {
    int (*finish_copy_font)(gs_font *font, gs_font *copied);
    int (*copy_glyph)(gs_font *font, gs_glyph glyph, gs_font *copied,
                      int options);
    int (*add_encoding)(gs_font *copied, gs_char chr, gs_glyph glyph);
    int (*named_glyph_slot)(gs_copied_font_data_t *cfdata, gs_glyph glyph,
                            gs_copied_glyph_t **pslot);
    /* Font procedures */
    font_proc_encode_char((*encode_char));
    font_proc_glyph_info((*glyph_info));
    font_proc_glyph_outline((*glyph_outline));
    int (*uncopy_glyph)(gs_font *font, gs_glyph glyph, gs_font *copied,
                      int options);
} gs_copied_font_procs_t;

/*
 * We need to store the Subrs data for Type 1/2 and CIDFontType 0 fonts,
 * and the GlobalSubrs data for all but Type 1.
 */
typedef struct gs_subr_info_s {
    byte *data;		/* Subr data */
    int count;
    uint *starts;	/* [count+1] Subr[i] = data[starts[i]..starts[i+1]] */
} gs_subr_info_t;

/*
 * The glyphs for copied fonts are stored explicitly in a table indexed by
 * glyph number.
 * For Type 1 fonts, the glyph numbers are parallel to the hashed name table.
 * For TrueType fonts, the glyph number is the TrueType GID.
 * For CIDFontType 0 fonts, the glyph number is the CID.
 * For CIDFontType 2 fonts, the glyph number is the TrueType GID;
 * a separate CIDMap array maps CIDs to GIDs.
 */
struct gs_copied_glyph_s {
    gs_const_string gdata;	/* vector data */
#define HAS_DATA 1		/* entry is in use */
                                /* HAS_SBW* are only used for TT-based fonts */
#define HAS_SBW0 2		/* has hmtx */
#define HAS_SBW1 4		/* has vmtx */
    byte used;			/* non-zero iff this entry is in use */
                                /* (if not, gdata.{data,size} = 0) */
    int order_index;            /* Index for the ordered glyph set. */
};
/*
 * We use a special GC descriptor to avoid large GC overhead.
 */
gs_private_st_composite(st_gs_copied_glyph_element, gs_copied_glyph_t,
                        "gs_copied_glyph_t[]", copied_glyph_element_enum_ptrs,
                        copied_glyph_element_reloc_ptrs);
static ENUM_PTRS_WITH(copied_glyph_element_enum_ptrs, gs_copied_glyph_t *pcg)
     if (index < size / (uint)sizeof(gs_copied_glyph_t))
         return ENUM_CONST_STRING(&pcg[index].gdata);
     return 0;
ENUM_PTRS_END
static RELOC_PTRS_WITH(copied_glyph_element_reloc_ptrs, gs_copied_glyph_t *pcg)
{
    uint count = size / (uint)sizeof(gs_copied_glyph_t);
    gs_copied_glyph_t *p = pcg;

    for (; count > 0; --count, ++p)
        if (p->gdata.size > 0)
            RELOC_CONST_STRING_VAR(p->gdata);
}
RELOC_PTRS_END

/*
 * Type 1 and TrueType fonts also have a 'names' table, parallel to the
 * 'glyphs' table.
 * For Type 1 fonts, this is a hash table; glyph numbers are assigned
 * arbitrarily, according to the hashed placement of the names.
 * For TrueType fonts, this is indexed by GID.
 * The strings in this table are either those returned by the font's
 * glyph_name procedure, which we assume are garbage-collected, or those
 * associated with the known encodings, which we assume are immutable.
 */
typedef struct gs_copied_glyph_name_s {
    gs_glyph glyph;		/* key (for comparison and glyph_name only) */
    gs_const_string str;	/* glyph name */
} gs_copied_glyph_name_t;
/*
 * We use the same special GC descriptor as above for 'names'.
 */
gs_private_st_composite(st_gs_copied_glyph_name_element,
                        gs_copied_glyph_name_t,
                        "gs_copied_glyph_name_t[]",
                        copied_glyph_name_enum_ptrs,
                        copied_glyph_name_reloc_ptrs);
static ENUM_PTRS_WITH(copied_glyph_name_enum_ptrs, gs_copied_glyph_name_t *pcgn)
     if (index < size / (uint)sizeof(gs_copied_glyph_name_t)) {
         const gs_copied_glyph_name_t *const p = &pcgn[index];

         return (p->str.size == 0 ||
                 gs_is_c_glyph_name(p->str.data, p->str.size) ?
                 ENUM_CONST_STRING2(0, 0) :
                 ENUM_CONST_STRING(&p->str));
     }
     return 0;
     /* We should mark glyph name here, but we have no access to
        the gs_font_dir instance. Will mark in gs_copied_font_data_enum_ptrs.
      */
ENUM_PTRS_END
static RELOC_PTRS_WITH(copied_glyph_name_reloc_ptrs, gs_copied_glyph_name_t *pcgn)
{
    uint count = size / (uint)sizeof(gs_copied_glyph_name_t);
    gs_copied_glyph_name_t *p = pcgn;

    for (; count > 0; --count, ++p)
        if (p->str.size > 0 && !gs_is_c_glyph_name(p->str.data, p->str.size))
            RELOC_CONST_STRING_VAR(p->str);
}
RELOC_PTRS_END

/*
 * To accommodate glyphs with multiple names, there is an additional
 * 'extra_names' table.  Since this is rare, this table uses linear search.
 */
typedef struct gs_copied_glyph_extra_name_s gs_copied_glyph_extra_name_t;
struct gs_copied_glyph_extra_name_s {
    gs_copied_glyph_name_t name;
    uint gid;			/* index in glyphs table */
    gs_copied_glyph_extra_name_t *next;
};
BASIC_PTRS(gs_copied_glyph_extra_name_ptrs) {
    GC_STRING_ELT(gs_copied_glyph_extra_name_t, name.str),
    GC_OBJ_ELT(gs_copied_glyph_extra_name_t, next)
};
gs_private_st_basic(st_gs_copied_glyph_extra_name,
                    gs_copied_glyph_extra_name_t,
                    "gs_copied_glyph_extra_name_t",
                    gs_copied_glyph_extra_name_ptrs,
                    gs_copied_glyph_extra_name_data);

/*
 * The client_data of copied fonts points to an instance of
 * gs_copied_font_data_t.
 */
struct gs_copied_font_data_s {
    gs_font_info_t info;	/* from the original font, must be first */
    const gs_copied_font_procs_t *procs;
    gs_copied_glyph_t *glyphs;	/* [glyphs_size] */
    uint glyphs_size;		/* (a power of 2 or a prime number for Type 1/2) */
    uint num_glyphs;		/* The number of glyphs copied. */
    gs_glyph notdef;		/* CID 0 or .notdef glyph */
    /*
     * We don't use a union for the rest of the data, because some of the
     * cases overlap and it's just not worth the trouble.
     */
    gs_copied_glyph_name_t *names; /* (Type 1/2, TrueType) [glyphs_size] */
    gs_copied_glyph_extra_name_t *extra_names; /* (TrueType) */
    byte *data;			/* (TrueType and CID fonts) copied data */
    uint data_size;		/* (TrueType and CID fonts) */
    gs_glyph *Encoding;		/* (Type 1/2 and Type 42) [256] */
    ushort *CIDMap;		/* (CIDFontType 2) [CIDCount] */
    gs_subr_info_t subrs;	/* (Type 1/2 and CIDFontType 0) */
    gs_subr_info_t global_subrs; /* (Type 2 and CIDFontType 0) */
    gs_font_cid0 *parent;	/* (Type 1 subfont) => parent CIDFontType 0 */
    gs_font_dir *dir;
    bool ordered;
};
extern_st(st_gs_font_info);
static
ENUM_PTRS_WITH(gs_copied_font_data_enum_ptrs, gs_copied_font_data_t *cfdata)
    /* See comments in gs_copy_font() below regarding the enumeration of names
     * and the font's 'dir' member
     */
    if (index == 12 && cfdata->dir != NULL) {
        gs_copied_glyph_name_t *names = cfdata->names;
        gs_copied_glyph_extra_name_t *en = cfdata->extra_names;
        int i;

        if (names != NULL)
            for (i = 0; i < cfdata->glyphs_size; ++i)
                if (names[i].glyph < gs_c_min_std_encoding_glyph)
                    cfdata->dir->ccache.mark_glyph(mem, names[i].glyph, NULL);
        for (; en != NULL; en = en->next)
            if (en->name.glyph < gs_c_min_std_encoding_glyph)
                cfdata->dir->ccache.mark_glyph(mem, en->name.glyph, NULL);
    }
    return ENUM_USING(st_gs_font_info, &cfdata->info, sizeof(gs_font_info_t), index - 12);
    ENUM_PTR3(0, gs_copied_font_data_t, glyphs, names, extra_names);
    ENUM_PTR3(3, gs_copied_font_data_t, data, Encoding, CIDMap);
    ENUM_PTR3(6, gs_copied_font_data_t, subrs.data, subrs.starts, global_subrs.data);
    ENUM_PTR3(9, gs_copied_font_data_t, global_subrs.starts, parent, dir);
ENUM_PTRS_END

static RELOC_PTRS_WITH(gs_copied_font_data_reloc_ptrs, gs_copied_font_data_t *cfdata)
{
    RELOC_PTR3(gs_copied_font_data_t, glyphs, names, extra_names);
    RELOC_PTR3(gs_copied_font_data_t, data, Encoding, CIDMap);
    RELOC_PTR3(gs_copied_font_data_t, subrs.data, subrs.starts, global_subrs.data);
    RELOC_PTR3(gs_copied_font_data_t, global_subrs.starts, parent, dir);
    RELOC_USING(st_gs_font_info, &cfdata->info, sizeof(gs_font_info_t));
}
RELOC_PTRS_END

gs_private_st_composite(st_gs_copied_font_data, gs_copied_font_data_t, "gs_copied_font_data_t",\
    gs_copied_font_data_enum_ptrs, gs_copied_font_data_reloc_ptrs);

static inline gs_copied_font_data_t *
cf_data(const gs_font *font)
{
    return (gs_copied_font_data_t *)font->client_data;
}

/* ================ Procedures ================ */

/* ---------------- Private utilities ---------------- */

/* Copy a string.  Return 0 or gs_error_VMerror. */
static int
copy_string(gs_memory_t *mem, gs_const_string *pstr, client_name_t cname)
{
    const byte *data = pstr->data;
    uint size = pstr->size;
    byte *str;

    if (data == 0)
        return 0;		/* empty string */
    str = gs_alloc_string(mem, size, cname);
    pstr->data = str;
    if (str == 0)
        return_error(gs_error_VMerror);
    memcpy(str, data, size);
    return 0;
}

/* Free a copied string. */
static void
uncopy_string(gs_memory_t *mem, gs_const_string *pstr, client_name_t cname)
{
    if (pstr->data)
        gs_free_const_string(mem, pstr->data, pstr->size, cname);
}

/*
 * Allocate an Encoding for a Type 1 or Type 42 font.
 */
static int
copied_Encoding_alloc(gs_font *copied)
{
    gs_copied_font_data_t *const cfdata = cf_data(copied);
    gs_glyph *Encoding = (gs_glyph *)
        gs_alloc_byte_array(copied->memory, 256, sizeof(*cfdata->Encoding),
                            "copy_font_type1(Encoding)");
    int i;

    if (Encoding == 0)
        return_error(gs_error_VMerror);
    for (i = 0; i < 256; ++i)
        Encoding[i] = GS_NO_GLYPH;
    cfdata->Encoding = Encoding;
    return 0;
}

/*
 * Allocate and set up data copied from a TrueType or CID font.
 * stell(*s) + extra is the length of the data.
 */
static int
copied_data_alloc(gs_font *copied, stream *s, uint extra, int code)
{
    gs_copied_font_data_t *const cfdata = cf_data(copied);
    uint len = stell(s);
    byte *fdata;

    if (code < 0)
        return code;
    fdata = gs_alloc_bytes(copied->memory, len + extra, "copied_data_alloc");
    if (fdata == 0)
        return_error(gs_error_VMerror);
    s_init(s, copied->memory);
    swrite_string(s, fdata, len);
    cfdata->data = fdata;
    cfdata->data_size = len + extra;
    return 0;
}

/*
 * Copy Subrs or GSubrs from a font.
 */
static int
copy_subrs(gs_font_type1 *pfont, bool global, gs_subr_info_t *psi,
           gs_memory_t *mem)
{
    int i, code;
    uint size;
    gs_glyph_data_t gdata;
    byte *data;
    uint *starts;

    gdata.memory = pfont->memory;
    /* Scan the font to determine the size of the subrs. */
    for (i = 0, size = 0;
         (code = pfont->data.procs.subr_data(pfont, i, global, &gdata)) !=
             gs_error_rangecheck;
         ++i) {
        if (code >= 0) {
            size += gdata.bits.size;
            gs_glyph_data_free(&gdata, "copy_subrs");
        }
    }
    if (size == 0)
        data = 0, starts = 0, i = 0;
    else {
        /* Allocate the copy. */
        data = gs_alloc_bytes(mem, size, "copy_subrs(data)");
        starts = (uint *)gs_alloc_byte_array(mem, i + 1, sizeof(*starts),
                                             "copy_subrs(starts)");
        if (data == 0 || starts == 0) {
            gs_free_object(mem, starts, "copy_subrs(starts)");
            gs_free_object(mem, data, "copy_subrs(data)");
            return_error(gs_error_VMerror);
        }

        /* Copy the data. */
        for (i = 0, size = 0;
             (code = pfont->data.procs.subr_data(pfont, i, global, &gdata)) !=
                 gs_error_rangecheck;
             ++i) {
            starts[i] = size;
            if (code >= 0) {
                memcpy(data + size, gdata.bits.data, gdata.bits.size);
                size += gdata.bits.size;
                gs_glyph_data_free(&gdata, "copy_subrs");
            }
        }
        starts[i] = size;
    }

    psi->data = data;
    psi->starts = starts;
    psi->count = i;
    return 0;
}

/*
 * Return a pointer to the definition of a copied glyph, accessed either by
 * name or by glyph number.  If the glyph is out of range, return
 * gs_error_rangecheck; if the glyph is in range but undefined, store a
 * pointer to the slot where it would be added, which will have gdata.data
 * == 0, and return gs_error_undefined; if the glyph is defined, store the
 * pointer and return 0.
 *
 * NOTE:
 * The interim variable (idx) is used here as a workaround for what appears
 * to be a compiler optimiser bug in VS2019 - using "glyph - GS_MIN_CID_GLYPH"
 * directly to index into the cfdata->glyphs array results in a pointer value
 * in *pslot which is complete nonsense. Using the variable to store the
 * calculated value results in working code.
 */
static int
copied_glyph_slot(gs_copied_font_data_t *cfdata, gs_glyph glyph,
                  gs_copied_glyph_t **pslot)
{
    uint gsize = cfdata->glyphs_size;
    unsigned int idx;

    *pslot = 0;
    if (glyph >= GS_MIN_GLYPH_INDEX) {
        /* CIDFontType 2 uses glyph indices for slots.  */
        idx = (unsigned int)(glyph - GS_MIN_GLYPH_INDEX);
        if (idx >= gsize)
            return_error(gs_error_rangecheck);
        *pslot = &cfdata->glyphs[idx];
    } else if (glyph >= GS_MIN_CID_GLYPH) {
        /* CIDFontType 0 uses CIDS for slots.  */
        idx = (unsigned int)(glyph - GS_MIN_CID_GLYPH);
        if (idx >= gsize)
            return_error(gs_error_rangecheck);
        *pslot = &cfdata->glyphs[idx];
    } else if (cfdata->names == 0)
        return_error(gs_error_rangecheck);
    else {
        int code = cfdata->procs->named_glyph_slot(cfdata, glyph, pslot);

        if (code < 0)
            return code;
    }
    if (!(*pslot)->used)
        return_error(gs_error_undefined);
    return 0;
}
static int
named_glyph_slot_none(gs_copied_font_data_t *cfdata, gs_glyph glyph,
                        gs_copied_glyph_t **pslot)
{
    return_error(gs_error_rangecheck);
}
static int
named_glyph_slot_hashed(gs_copied_font_data_t *cfdata, gs_glyph glyph,
                        gs_copied_glyph_t **pslot)
{
    uint gsize = cfdata->glyphs_size;
    gs_copied_glyph_name_t *names = cfdata->names;
    uint hash = (uint)glyph % gsize;
    /*
     * gsize is either a prime number or a power of 2.
     * If it is prime, any positive reprobe below gsize guarantees that we
     * will touch every slot.
     * If it is a power of 2, any odd reprobe guarantees that we
     * will touch every slot.
     */
    uint hash2 = ((uint)glyph / gsize * 2 + 1) % gsize;
    uint tries = gsize;

    while (names[hash].str.data != 0 && names[hash].glyph != glyph) {
        hash = (hash + hash2) % gsize;
        if (!tries)
	  return_error(gs_error_undefined);
        tries--;
    }
    *pslot = &cfdata->glyphs[hash];
    return 0;
}
static int
named_glyph_slot_linear(gs_copied_font_data_t *cfdata, gs_glyph glyph,
                        gs_copied_glyph_t **pslot)
{
    {
        gs_copied_glyph_name_t *names = cfdata->names;
        int i;

        for (i = 0; i < cfdata->glyphs_size; ++i)
            if (names[i].glyph == glyph) {
                *pslot = &cfdata->glyphs[i];
                return 0;
            }
    }
    /* This might be a glyph with multiple names.  Search extra_names. */
    {
        gs_copied_glyph_extra_name_t *extra_name = cfdata->extra_names;

        for (; extra_name != 0; extra_name = extra_name->next)
            if (extra_name->name.glyph == glyph) {
                *pslot = &cfdata->glyphs[extra_name->gid];
                return 0;
            }
    }
    return_error(gs_error_rangecheck);
}

/*
 * Add glyph data to the glyph table.  This handles copying the vector
 * data, detecting attempted redefinitions, and freeing temporary glyph
 * data.  The glyph must be an integer, an index in the glyph table.
 * Return 1 if the glyph was already defined, 0 if newly added (or an
 * error per options).
 */
static int
copy_glyph_data(gs_font *font, gs_glyph glyph, gs_font *copied, int options,
                gs_glyph_data_t *pgdata, const byte *prefix, int prefix_bytes)
{
    gs_copied_font_data_t *const cfdata = cf_data(copied);
    uint size = pgdata->bits.size;
    gs_copied_glyph_t *pcg = 0;
    int code = copied_glyph_slot(cfdata, glyph, &pcg);

    if (cfdata->ordered)
        return_error(gs_error_unregistered); /* Must not happen. */
    switch (code) {
    case 0:			/* already defined */
        if ((options & COPY_GLYPH_NO_OLD) ||
            pcg->gdata.size != prefix_bytes + size ||
            (prefix_bytes > 0 && memcmp(pcg->gdata.data, prefix, prefix_bytes)) ||
            (size > 0 && memcmp(pcg->gdata.data + prefix_bytes,
                   pgdata->bits.data, size))
            )
            code = gs_note_error(gs_error_invalidaccess);
        else
            code = 1;
        break;
    case gs_error_undefined:
        if (options & COPY_GLYPH_NO_NEW)
            code = gs_note_error(gs_error_undefined);
        else if (pcg == NULL)
            code = gs_note_error(gs_error_undefined);
        else {
            uint str_size = prefix_bytes + size;

            code = 0;
            if (str_size > 0) {
                byte *str = gs_alloc_string(copied->memory, str_size,
                                            "copy_glyph_data(data)");

                if (str == 0)
                    code = gs_note_error(gs_error_VMerror);
                else {
                    if (prefix_bytes)
                        memcpy(str, prefix, prefix_bytes);
                    memcpy(str + prefix_bytes, pgdata->bits.data, size);
                    pcg->gdata.data = str;
                }
            }
            if (code >= 0) {
                pcg->gdata.size = str_size;
                pcg->used = HAS_DATA;
                pcg->order_index = -1;
                code = 0;
                cfdata->num_glyphs++;
            }
        }
    default:
        break;
    }
    gs_glyph_data_free(pgdata, "copy_glyph_data");
    return code;
}

/*
 * Copy a glyph name into the names table.
 */
static int
copy_glyph_name(gs_font *font, gs_glyph glyph, gs_font *copied,
                gs_glyph copied_glyph)
{
    gs_glyph known_glyph;
    gs_copied_font_data_t *const cfdata = cf_data(copied);
    gs_copied_glyph_t *pcg;
    int code = copied_glyph_slot(cfdata, copied_glyph, &pcg);
    gs_copied_glyph_name_t *pcgn;
    gs_const_string str;

    if (cfdata->ordered)
        return_error(gs_error_unregistered); /* Must not happen. */
    if (code < 0 ||
        (code = font->procs.glyph_name(font, glyph, &str)) < 0
        )
        return code;
    /* Try to share a permanently allocated known glyph name. */
    if ((known_glyph = gs_c_name_glyph(str.data, str.size)) != GS_NO_GLYPH)
        gs_c_glyph_name(known_glyph, &str);
    else if ((code = copy_string(copied->memory, &str, "copy_glyph_name")) < 0)
        return code;
    pcgn = cfdata->names + (pcg - cfdata->glyphs);
    if (pcgn->glyph != GS_NO_GLYPH &&
        (pcgn->str.size != str.size ||
         memcmp(pcgn->str.data, str.data, str.size))
        ) {
        /* This is a glyph with multiple names.  Add an extra_name entry. */
        gs_copied_glyph_extra_name_t *extra_name =
            gs_alloc_struct(copied->memory, gs_copied_glyph_extra_name_t,
                            &st_gs_copied_glyph_extra_name,
                            "copy_glyph_name(extra_name)");

        if (extra_name == 0)
            return_error(gs_error_VMerror);
        memset(extra_name, 0x00, sizeof(gs_copied_glyph_extra_name_t));
        extra_name->next = cfdata->extra_names;
        extra_name->gid = pcg - cfdata->glyphs;
        cfdata->extra_names = extra_name;
        pcgn = &extra_name->name;
    }
    if (pcgn->str.size != 0 && !gs_is_c_glyph_name(pcgn->str.data, pcgn->str.size))
        gs_free_string(copied->memory, (byte *)pcgn->str.data, pcgn->str.size, "Free copied glyph name");
    pcgn->glyph = glyph;
    pcgn->str = str;
    return 0;
}

/*
 * Find the .notdef glyph in a font.
 */
static gs_glyph
find_notdef(gs_font_base *font)
{
    int index = 0;
    gs_glyph glyph;

    while (font->procs.enumerate_glyph((gs_font *)font, &index,
                                       GLYPH_SPACE_NAME, &glyph),
           index != 0)
        if (gs_font_glyph_is_notdef(font, glyph))
            return glyph;
    return GS_NO_GLYPH;		/* best we can do */
}

/*
 * Add an Encoding entry to a character-indexed font (Type 1/2/42).
 */
static int
copied_char_add_encoding(gs_font *copied, gs_char chr, gs_glyph glyph)
{
    gs_copied_font_data_t *const cfdata = cf_data(copied);
    gs_glyph *Encoding = cfdata->Encoding;
    gs_copied_glyph_t *pslot;
    int code;

    if (cfdata->ordered)
        return_error(gs_error_unregistered); /* Must not happen. */
    if (Encoding == 0)
        return_error(gs_error_invalidaccess);
    if (chr >= 256 || glyph >= GS_MIN_CID_GLYPH)
        return_error(gs_error_rangecheck);
    code = copied_glyph_slot(cfdata, glyph, &pslot);
    if (code < 0)
        return code;
    if (Encoding[chr] != glyph && Encoding[chr] != GS_NO_GLYPH)
        return_error(gs_error_invalidaccess);
    Encoding[chr] = glyph;
    return 0;
}

/* Don't allow adding an Encoding entry. */
static int
copied_no_add_encoding(gs_font *copied, gs_char chr, gs_glyph glyph)
{
    return_error(gs_error_invalidaccess);
}

/* ---------------- Font procedures ---------------- */

static int
copied_font_info(gs_font *font, const gs_point *pscale, int members,
                 gs_font_info_t *info)
{
    if (pscale != 0)
        return_error(gs_error_rangecheck);
    *info = cf_data(font)->info;
    return 0;
}

static gs_glyph
copied_encode_char(gs_font *copied, gs_char chr, gs_glyph_space_t glyph_space)
{
    gs_copied_font_data_t *const cfdata = cf_data(copied);
    const gs_glyph *Encoding = cfdata->Encoding;

    if (chr >= 256 || Encoding == 0)
        return GS_NO_GLYPH;
    return Encoding[chr];
}

static int
copied_enumerate_glyph(gs_font *font, int *pindex,
                       gs_glyph_space_t glyph_space, gs_glyph *pglyph)
{
    gs_copied_font_data_t *const cfdata = cf_data(font);

    if (cfdata->ordered) {
        if (*pindex >= cfdata->num_glyphs)
            *pindex = 0;
        else {
            int i = cfdata->glyphs[*pindex].order_index;

            *pglyph = cfdata->names[i].glyph;
            ++(*pindex);
        }
        return 0;
    }
    for (; *pindex < cfdata->glyphs_size; ++*pindex)
        if (cfdata->glyphs[*pindex].used) {
            *pglyph =
                (glyph_space == GLYPH_SPACE_NAME && cfdata->names != 0 ?
                 cfdata->names[*pindex].glyph :
                 /* CIDFontType 0 uses CIDS as slot indices; CIDFontType 2 uses GIDs. */
                 *pindex + (glyph_space == GLYPH_SPACE_NAME
                            ? GS_MIN_CID_GLYPH : GS_MIN_GLYPH_INDEX));
            ++(*pindex);
            return 0;
        }
    *pindex = 0;
    return 0;
}

static int
copied_glyph_name(gs_font *font, gs_glyph glyph, gs_const_string *pstr)
{
    gs_copied_font_data_t *const cfdata = cf_data(font);
    gs_copied_glyph_t *pcg;

    if (glyph >= GS_MIN_CID_GLYPH)
        return_error(gs_error_rangecheck);
    if (copied_glyph_slot(cfdata, glyph, &pcg) < 0)
        return_error(gs_error_undefined);
    *pstr = cfdata->names[pcg - cfdata->glyphs].str;
    return 0;
}

static int
copied_build_char(gs_show_enum *pte, gs_gstate *pgs, gs_font *font,
                  gs_char chr, gs_glyph glyph)
{
    int wmode = font->WMode;
    int code;
    gs_glyph_info_t info;
    double wxy[6];
    double sbw_stub[4]; /* Currently glyph_outline retrieves sbw only with type 1,2,9 fonts. */

    if (glyph == GS_NO_GLYPH) {
        glyph = font->procs.encode_char(font, chr, GLYPH_SPACE_INDEX);
        if (glyph == GS_NO_GLYPH)
            glyph = cf_data(font)->notdef;
    }
    /*
     * Type 1/2 outlines don't require a current point, but TrueType
     * outlines do.  We might want to fix this someday....
     */
    if ((code = gs_moveto(pgs, 0.0, 0.0)) < 0 ||
        (code = font->procs.glyph_info(font, glyph, NULL,
                                       (GLYPH_INFO_WIDTH << wmode) |
                                       GLYPH_INFO_BBOX |
                                       GLYPH_INFO_OUTLINE_WIDTHS,
                                       &info)) < 0
        )
        return code;
    wxy[0] = info.width[wmode].x;
    wxy[1] = info.width[wmode].y;
    wxy[2] = info.bbox.p.x;
    wxy[3] = info.bbox.p.y;
    wxy[4] = info.bbox.q.x;
    wxy[5] = info.bbox.q.y;
    if ((code = gs_setcachedevice_double(pte, pte->pgs, wxy)) < 0 ||
        (code = font->procs.glyph_outline(font, wmode, glyph, &ctm_only(pgs),
                                          pgs->path, sbw_stub)) < 0
        )
        return code;
    if (font->PaintType != 0) {
        gs_setlinewidth(pgs, font->StrokeWidth);
        return gs_stroke(pgs);
    } else {
        return gs_fill(pgs);
    }
}

static inline bool
compare_arrays(const float *v0, int l0, const float *v1, int l1)
{
    if (l0 != l1)
        return false;
    if (memcmp(v0, v1, l0 * sizeof(v0[0])))
        return false;
    return true;
}

#define compare_tables(a, b) compare_arrays(a.values, a.count, b.values, b.count)

static int
compare_glyphs(const gs_font *cfont, const gs_font *ofont, gs_glyph *glyphs,
                           int num_glyphs, int glyphs_step, int level)
{
    /*
     * Checking widths because we can synthesize fonts from random fonts
     * having same FontName and FontType.
     * We must request width explicitely because Type 42 stores widths
     * separately from outline data. We could skip it for Type 1, which doesn't.
     * We don't care of Metrics, Metrics2 because copied font never has them.
     */
    int i, WMode = ofont->WMode;
    int members = (GLYPH_INFO_WIDTH0 << WMode) | GLYPH_INFO_OUTLINE_WIDTHS | GLYPH_INFO_NUM_PIECES;
    gs_matrix mat;
    gs_copied_font_data_t *const cfdata = cf_data(cfont);
    int num_new_glyphs = 0;

    gs_make_identity(&mat);
    for (i = 0; i < num_glyphs; i++) {
        gs_glyph glyph = *(gs_glyph *)((byte *)glyphs + i * glyphs_step);
        gs_glyph pieces0[40], *pieces = pieces0;
        gs_glyph_info_t info0, info1;
        int code0, code1, code2, code;

        memset(&info0, 0x00, sizeof(gs_glyph_info_t));
        code0 = ofont->procs.glyph_info((gs_font *)ofont, glyph, &mat, members, &info0);
        memset(&info1, 0x00, sizeof(gs_glyph_info_t));
        code1 = cfont->procs.glyph_info((gs_font *)cfont, glyph, &mat, members, &info1);

        if (code0 == gs_error_undefined)
            continue;
        if (code1 == gs_error_undefined) {
            num_new_glyphs++;
            if (num_new_glyphs > cfdata->glyphs_size - cfdata->num_glyphs)
                return 0;
            continue;
        }
        if (code0 < 0)
            return code0;
        if (code1 < 0)
            return code1;
        if (info0.num_pieces != info1.num_pieces)
            return 0;
        if (info0.num_pieces > 0) {
            if(level > 5)
                return_error(gs_error_rangecheck); /* abnormal glyph recursion */
            if (info0.num_pieces > countof(pieces0) / 2) {
                pieces = (gs_glyph *)gs_alloc_bytes(cfont->memory,
                    sizeof(gs_glyph) * info0.num_pieces * 2, "compare_glyphs");
                if (pieces == 0)
                    return_error(gs_error_VMerror);
            }
            info0.pieces = pieces;
            info1.pieces = pieces + info0.num_pieces;
            code0 = ofont->procs.glyph_info((gs_font *)ofont, glyph, &mat,
                                    GLYPH_INFO_PIECES, &info0);
            code1 = cfont->procs.glyph_info((gs_font *)cfont, glyph, &mat,
                                    GLYPH_INFO_PIECES, &info1);
            if (code0 >= 0 && code1 >= 0) {
                code2 = memcmp(info0.pieces, info1.pieces, info0.num_pieces * sizeof(*pieces));
                if (!code2)
                    code = compare_glyphs(cfont, ofont, pieces, info0.num_pieces, glyphs_step, level + 1);
                else
                    code = 0; /* Quiet compiler. */
            } else
                code2 = code = 0;
            if (pieces != pieces0)
                gs_free_object(cfont->memory, pieces, "compare_glyphs");
            if (code0 == gs_error_undefined)
                continue;
            if (code1 == gs_error_undefined) {
                num_new_glyphs++;
                if (num_new_glyphs > cfdata->glyphs_size - cfdata->num_glyphs)
                    return 0;
                continue;
            }
            if (code0 < 0)
                return code0;
            if (code1 < 0)
                return code1;
            if (code2 || code == 0) {
                return 0;
            }
        } else {
            gs_glyph_data_t gdata0, gdata1;

            switch(cfont->FontType) {
                case ft_encrypted:
                case ft_encrypted2: {
                    gs_font_type1 *font0 = (gs_font_type1 *)cfont;
                    gs_font_type1 *font1 = (gs_font_type1 *)ofont;

                    gdata0.memory = font0->memory;
                    gdata1.memory = font1->memory;
                    code0 = font0->data.procs.glyph_data(font0, glyph, &gdata0);
                    code1 = font1->data.procs.glyph_data(font1, glyph, &gdata1);
                    break;
                }
                case ft_TrueType:
                case ft_CID_TrueType: {
                    gs_font_type42 *font0 = (gs_font_type42 *)cfont;
                    gs_font_type42 *font1 = (gs_font_type42 *)ofont;
                    uint glyph_index0 = font0->data.get_glyph_index(font0, glyph);
                    uint glyph_index1 = font1->data.get_glyph_index(font1, glyph);

                    gdata0.memory = font0->memory;
                    gdata1.memory = font1->memory;
                    code0 = font0->data.get_outline(font0, glyph_index0, &gdata0);
                    code1 = font1->data.get_outline(font1, glyph_index1, &gdata1);
                    break;
                }
                case ft_CID_encrypted: {
                    gs_font_cid0 *font0 = (gs_font_cid0 *)cfont;
                    gs_font_cid0 *font1 = (gs_font_cid0 *)ofont;
                    int fidx0, fidx1;

                    gdata0.memory = font0->memory;
                    gdata1.memory = font1->memory;
                    code0 = font0->cidata.glyph_data((gs_font_base *)font0, glyph, &gdata0, &fidx0);
                    code1 = font1->cidata.glyph_data((gs_font_base *)font1, glyph, &gdata1, &fidx1);
                    break;
                }
                default:
                    return_error(gs_error_unregistered); /* unimplemented */
            }
            if (code0 < 0) {
                if (code1 >= 0)
                    gs_glyph_data_free(&gdata1, "compare_glyphs");
                return code0;
            }
            if (code1 < 0) {
                if (code0 >= 0)
                    gs_glyph_data_free(&gdata0, "compare_glyphs");
                return code1;
            }
            if (gdata0.bits.size != gdata1.bits.size)
                return 0;
            if (memcmp(gdata0.bits.data, gdata0.bits.data, gdata0.bits.size))
                return 0;
            gs_glyph_data_free(&gdata0, "compare_glyphs");
            gs_glyph_data_free(&gdata1, "compare_glyphs");
        }
    }
    return 1;
}

/* ---------------- Individual FontTypes ---------------- */

/* ------ Type 1 ------ */

static int
copied_type1_glyph_data(gs_font_type1 * pfont, gs_glyph glyph,
                        gs_glyph_data_t *pgd)
{
    gs_copied_font_data_t *const cfdata = cf_data((gs_font *)pfont);
    gs_copied_glyph_t *pslot;
    int code = copied_glyph_slot(cfdata, glyph, &pslot);

    if (code < 0)
        return code;
    gs_glyph_data_from_string(pgd, pslot->gdata.data, pslot->gdata.size,
                              NULL);
    return 0;
}

static int
copied_type1_subr_data(gs_font_type1 * pfont, int subr_num, bool global,
                       gs_glyph_data_t *pgd)
{
    gs_copied_font_data_t *const cfdata = cf_data((gs_font *)pfont);
    const gs_subr_info_t *psi =
        (global ? &cfdata->global_subrs : &cfdata->subrs);

    if (subr_num < 0 || subr_num >= psi->count)
        return_error(gs_error_rangecheck);
    gs_glyph_data_from_string(pgd, psi->data + psi->starts[subr_num],
                              psi->starts[subr_num + 1] -
                                psi->starts[subr_num],
                              NULL);
    return 0;
}

static int
copied_type1_seac_data(gs_font_type1 * pfont, int ccode,
                       gs_glyph * pglyph, gs_const_string *gstr, gs_glyph_data_t *pgd)
{
    /*
     * This can only be invoked if the components have already been
     * copied to their proper positions, so it is simple.
     */
    gs_glyph glyph = gs_c_known_encode((gs_char)ccode, ENCODING_INDEX_STANDARD);
    gs_glyph glyph1;
    int code;

    if (glyph == GS_NO_GLYPH)
        return_error(gs_error_rangecheck);
    code = gs_c_glyph_name(glyph, gstr);
    if (code < 0)
        return code;
    code = pfont->dir->global_glyph_code((gs_font *)pfont, gstr, &glyph1);
    if (code < 0)
        return code;
    if (pglyph)
        *pglyph = glyph1;
    if (pgd)
        return copied_type1_glyph_data(pfont, glyph1, pgd);
    else
        return 0;
}

static int
copied_type1_push_values(void *callback_data, const fixed *values, int count)
{
    return_error(gs_error_unregistered);
}

static int
copied_type1_pop_value(void *callback_data, fixed *value)
{
    return_error(gs_error_unregistered);
}

static int
copy_font_type1(gs_font *font, gs_font *copied)
{
    gs_font_type1 *font1 = (gs_font_type1 *)font;
    gs_font_type1 *copied1 = (gs_font_type1 *)copied;
    gs_copied_font_data_t *const cfdata = cf_data(copied);
    int code;

    cfdata->notdef = find_notdef((gs_font_base *)font);
    code = copied_Encoding_alloc(copied);
    if (code < 0)
        return code;
    if ((code = copy_subrs(font1, false, &cfdata->subrs, copied->memory)) < 0 ||
        (code = copy_subrs(font1, true, &cfdata->global_subrs, copied->memory)) < 0
        ) {
        gs_free_object(copied->memory, cfdata->Encoding,
                       "copy_font_type1(Encoding)");
        return code;
    }
    /*
     * We don't need real push/pop procedures, because we can't do anything
     * useful with fonts that have non-standard OtherSubrs anyway.
     */
    copied1->data.procs.glyph_data = copied_type1_glyph_data;
    copied1->data.procs.subr_data = copied_type1_subr_data;
    copied1->data.procs.seac_data = copied_type1_seac_data;
    copied1->data.procs.push_values = copied_type1_push_values;
    copied1->data.procs.pop_value = copied_type1_pop_value;
    copied1->data.proc_data = 0;
    return 0;
}

static int
uncopy_glyph_type1(gs_font *font, gs_glyph glyph, gs_font *copied, int options)
{
    gs_copied_glyph_t *pcg = NULL;
    gs_copied_font_data_t *cfdata = cf_data(copied);

    (void)copied_glyph_slot(cfdata, glyph, &pcg);
    if (pcg != NULL) {
        if (pcg->gdata.data != NULL) {
            gs_free_string(copied->memory, (byte *)pcg->gdata.data, pcg->gdata.size, "Free copied glyph name");
            pcg->gdata.size = 0;
            pcg->gdata.data = NULL;
        }
        pcg->used = 0;
        cfdata->num_glyphs--;
    }
    return 0;
}

static int
copy_glyph_type1(gs_font *font, gs_glyph glyph, gs_font *copied, int options)
{
    gs_glyph_data_t gdata;
    gs_font_type1 *font1 = (gs_font_type1 *)font;
    int code;
    int rcode;

    gdata.memory = font->memory;
    code = font1->data.procs.glyph_data(font1, glyph, &gdata);
    if (code < 0)
        return code;
    code = copy_glyph_data(font, glyph, copied, options, &gdata, NULL, 0);
    if (code < 0)
        return code;
    rcode = code;
    if (code == 0)
        code = copy_glyph_name(font, glyph, copied, glyph);
    return (code < 0 ? code : rcode);
}

static int
copied_type1_glyph_outline(gs_font *font, int WMode, gs_glyph glyph,
                           const gs_matrix *pmat, gx_path *ppath, double sbw[4])
{   /*
     * 'WMode' may be inherited from an upper font.
     * We ignore in because Type 1,2 charstrings do not depend on it.
     */

    /*
     * This code duplicates much of zcharstring_outline in zchar1.c.
     * This is unfortunate, but we don't see a simple way to refactor the
     * code to avoid it.
     */
    gs_glyph_data_t gdata;
    gs_font_type1 *const pfont1 = (gs_font_type1 *)font;
    int code;
    const gs_glyph_data_t *pgd = &gdata;
    gs_type1_state cis;
    gs_gstate ggs;

    memset(&cis, 0x00, sizeof(cis));

    gdata.memory = pfont1->memory;
    code = pfont1->data.procs.glyph_data(pfont1, glyph, &gdata);
    if (code < 0)
        return code;
    if (pgd->bits.size <= max(pfont1->data.lenIV, 0))
        return_error(gs_error_invalidfont);
    /* Initialize just enough of the gs_gstate. */
    if (pmat)
        gs_matrix_fixed_from_matrix(&ggs.ctm, pmat);
    else {
        gs_matrix imat;

        gs_make_identity(&imat);
        gs_matrix_fixed_from_matrix(&ggs.ctm, &imat);
    }
    ggs.flatness = 0;
    code = gs_type1_interp_init(&cis, &ggs, ppath, NULL, NULL, true, 0,
                                pfont1);
    if (code < 0)
        return code;
    cis.no_grid_fitting = true;
    /* Continue interpreting. */
    for (;;) {
        int value;

        code = pfont1->data.interpret(&cis, pgd, &value);
        switch (code) {
        case 0:		/* all done */
            /* falls through */
        default:		/* code < 0, error */
            return code;
        case type1_result_callothersubr:	/* unknown OtherSubr */
            return_error(gs_error_rangecheck); /* can't handle it */
        case type1_result_sbw:	/* [h]sbw, just continue */
            pgd = 0;
            type1_cis_get_metrics(&cis, sbw);
        }
    }
}

static const gs_copied_font_procs_t copied_procs_type1 = {
    copy_font_type1, copy_glyph_type1, copied_char_add_encoding,
    named_glyph_slot_hashed,
    copied_encode_char, gs_type1_glyph_info, copied_type1_glyph_outline,
    uncopy_glyph_type1
};

static void hash_subrs(gs_font_type1 *pfont)
{
    gs_type1_data *d0 = &pfont->data;
    gs_glyph_data_t gdata0;
    gs_md5_state_t md5;
    int i, exit = 0;

    gs_md5_init(&md5);
    gdata0.memory = pfont->memory;
    /* Scan the font to hash the global subrs. */
    for (i = 0; !exit; i++) {
        int code0 = pfont->data.procs.subr_data((gs_font_type1 *)pfont,
                                                i, true, &gdata0);
        if (code0 == gs_error_rangecheck)
            /* rangecheck means we ran out of /Subrs */
            exit = true;
        if (code0 == gs_error_typecheck)
            /* typecheck means that we may have encoutnered a null object
             * for a Subr, we ignore this subr, but carry on hashing, as there
             * may be more Subrs.
             */
            continue;
        if (code0 < 0)
            break;
        else {
            gs_md5_append(&md5, gdata0.bits.data, gdata0.bits.size);
            gs_glyph_data_free(&gdata0, "hash_type1_subrs");
        }
    }
    /* For a 'belt and braces' approach, we also record the number of local
     * and global /Subrs, and compare these below as well. Shifting the global
     * subrs up means that we can avoid an accidental co-incidence where simply
     * summing the two sets together might give the same result for different fonts.
     */
    d0->num_subrs = i << 16;
    exit = 0;
    /* Scan the font to hash the local subrs. */
    for (i = 0; !exit; i++) {
        int code0 = pfont->data.procs.subr_data((gs_font_type1 *)pfont,
                                                i, false, &gdata0);
        if (code0 == gs_error_rangecheck)
            /* rangecheck means we ran out of /Subrs */
            exit = true;
        if (code0 == gs_error_typecheck)
            /* typecheck means that we may have encoutnered a null object
             * for a Subr, we ignore this subr, but carry on hashing, as there
             * may be more Subrs.
             */
            continue;
        if (code0 < 0)
            break;
        else {
            gs_md5_append(&md5, gdata0.bits.data, gdata0.bits.size);
            gs_glyph_data_free(&gdata0, "hash_type1_subrs");
        }
    }
    gs_md5_finish(&md5, d0->hash_subrs);
    d0->num_subrs += i;
}

static bool
same_type1_hinting(const gs_font_type1 *cfont, const gs_font_type1 *ofont)
{
    const gs_type1_data *d0 = &cfont->data, *d1 = &ofont->data;
    int *hash0 = (int *)&d0->hash_subrs;
    int *hash1 = (int *)&d1->hash_subrs;

    if (d0->lenIV != d1->lenIV)
        return false;
    /*
    if (d0->defaultWidthX != d1->defaultWidthX)
        return false;
    if (d0->nominalWidthX != d1->nominalWidthX)
        return false;
    */
    if (d0->BlueFuzz != d1->BlueFuzz)
        return false;
    if (d0->BlueScale != d1->BlueScale)
        return false;
    if (d0->BlueShift != d1->BlueShift)
        return false;
    if (d0->ExpansionFactor != d1->ExpansionFactor)
        return false;
    if (d0->ForceBold != d1->ForceBold)
        return false;
    if (!compare_tables(d0->FamilyBlues, d1->FamilyBlues))
        return false;
    if (!compare_tables(d0->FamilyOtherBlues, d1->FamilyOtherBlues))
        return false;
    if (d0->LanguageGroup != d1->LanguageGroup)
        return false;
    if (!compare_tables(d0->OtherBlues, d1->OtherBlues))
        return false;
    if (d0->RndStemUp != d1->RndStemUp)
        return false;
    if (!compare_tables(d0->StdHW, d1->StdHW))
        return false;
    if (!compare_tables(d0->StemSnapH, d1->StemSnapH))
        return false;
    if (!compare_tables(d0->StemSnapV, d1->StemSnapV))
        return false;
    if (!compare_tables(d0->WeightVector, d1->WeightVector))
        return false;
    if (hash0[0] == 0x00 && hash0[1] == 0x00 && hash0[2] == 0x00 && hash0[3] == 0x00)
        hash_subrs((gs_font_type1 *)cfont);
    if (hash1[0] == 0x00 && hash1[1] == 0x00 && hash1[2] == 0x00 && hash1[3] == 0x00)
        hash_subrs((gs_font_type1 *)ofont);
    if (memcmp(d0->hash_subrs, d1->hash_subrs, 16) != 0 || d0->num_subrs != d1->num_subrs)
        return false;

    /*
     *	We ignore differences in OtherSubrs because pdfwrite
     *	must build without PS interpreter and therefore copied font
     *	have no storage for them.
     */
    return true;
}

/* ------ Type 42 ------ */

static int
copied_type42_string_proc(gs_font_type42 *font, ulong offset, uint len,
                          const byte **pstr)
{
    gs_copied_font_data_t *const cfdata = font->data.proc_data;

    if (offset + len > cfdata->data_size)
        return_error(gs_error_rangecheck);
    *pstr = cfdata->data + offset;
    return 0;
}

static int
copied_type42_get_outline(gs_font_type42 *font, uint glyph_index,
                          gs_glyph_data_t *pgd)
{
    gs_copied_font_data_t *const cfdata = font->data.proc_data;
    gs_copied_glyph_t *pcg;

    if (glyph_index >= cfdata->glyphs_size)
        return_error(gs_error_rangecheck);
    pcg = &cfdata->glyphs[glyph_index];
    if (!pcg->used)
        gs_glyph_data_from_null(pgd);
    else
        gs_glyph_data_from_string(pgd, pcg->gdata.data, pcg->gdata.size, NULL);
    return 0;
}

static int
copied_type42_get_metrics(gs_font_type42 * pfont, uint glyph_index,
                          gs_type42_metrics_options_t options, float *sbw)
{
    /* Check whether we have metrics for this (glyph,wmode) pair. */
    gs_copied_font_data_t *const cfdata = pfont->data.proc_data;
    gs_copied_glyph_t *pcg;
    int wmode = gs_type42_metrics_options_wmode(options);

    if (glyph_index >= cfdata->glyphs_size)
        return_error(gs_error_rangecheck);
    pcg = &cfdata->glyphs[glyph_index];
    if (!(pcg->used & (HAS_SBW0 << wmode)))
        return_error(gs_error_undefined);
    return gs_type42_default_get_metrics(pfont, glyph_index, options, sbw);
}

static uint
copied_type42_get_glyph_index(gs_font_type42 *font, gs_glyph glyph)
{
    gs_copied_font_data_t *const cfdata = cf_data((gs_font *)font);
    gs_copied_glyph_t *pcg;
    int code = copied_glyph_slot(cfdata, glyph, &pcg);

    if (code < 0)
        return GS_NO_GLYPH;
    return pcg - cfdata->glyphs;
}

static int
copy_font_type42(gs_font *font, gs_font *copied)
{
    gs_font_type42 *const font42 = (gs_font_type42 *)font;
    gs_font_type42 *const copied42 = (gs_font_type42 *)copied;
    gs_copied_font_data_t *const cfdata = cf_data(copied);
    /*
     * We "write" the font, aside from the glyphs, into an in-memory
     * structure, and access it from there.
     * We allocate room at the end of the copied data for fake hmtx/vmtx.
     */
    uint extra = font42->data.trueNumGlyphs * 8;
    stream fs;
    int code;

    cfdata->notdef = find_notdef((gs_font_base *)font);
    code = copied_Encoding_alloc(copied);
    if (code < 0)
        return code;
    s_init(&fs, font->memory);
    swrite_position_only(&fs);
    code = (font->FontType == ft_TrueType ? psf_write_truetype_stripped(&fs, font42)
                                          : psf_write_cid2_stripped(&fs, (gs_font_cid2 *)font42));
    code = copied_data_alloc(copied, &fs, extra, code);
    if (code < 0)
        goto fail;
    if (font->FontType == ft_TrueType)
        psf_write_truetype_stripped(&fs, font42);
    else
        psf_write_cid2_stripped(&fs, (gs_font_cid2 *)font42);
    copied42->data.string_proc = copied_type42_string_proc;
    copied42->data.proc_data = cfdata;
    code = gs_type42_font_init(copied42, 0);
    if (code < 0)
        goto fail2;
    /* gs_type42_font_init overwrites font_info. */
    copied->procs.font_info = copied_font_info;
    /* gs_type42_font_init overwrites enumerate_glyph. */
    copied42->procs.enumerate_glyph = copied_enumerate_glyph;
    copied42->data.get_glyph_index = copied_type42_get_glyph_index;
    copied42->data.get_outline = copied_type42_get_outline;
    copied42->data.get_metrics = copied_type42_get_metrics;
    copied42->data.metrics[0].numMetrics =
        copied42->data.metrics[1].numMetrics =
        extra / 8;
    copied42->data.metrics[0].offset = cfdata->data_size - extra;
    copied42->data.metrics[1].offset = cfdata->data_size - extra / 2;
    copied42->data.metrics[0].length =
        copied42->data.metrics[1].length =
        extra / 2;
    memset(cfdata->data + cfdata->data_size - extra, 0, extra);
    copied42->data.numGlyphs = font42->data.numGlyphs;
    copied42->data.trueNumGlyphs = font42->data.trueNumGlyphs;
    return 0;
 fail2:
    gs_free_object(copied->memory, cfdata->data,
                   "copy_font_type42(data)");
 fail:
    gs_free_object(copied->memory, cfdata->Encoding,
                   "copy_font_type42(Encoding)");
    return code;
}

static int uncopy_glyph_type42(gs_font *font, gs_glyph glyph, gs_font *copied, int options)
{
    gs_copied_glyph_t *pcg = NULL;
    gs_font_type42 *font42 = (gs_font_type42 *)font;
    gs_font_cid2 *fontCID2 = (gs_font_cid2 *)font;
    gs_copied_font_data_t *cfdata = cf_data(copied);
    uint gid = (options & COPY_GLYPH_BY_INDEX ? glyph - GS_MIN_GLYPH_INDEX :
                font->FontType == ft_CID_TrueType
                    ? fontCID2->cidata.CIDMap_proc(fontCID2, glyph)
                    : font42->data.get_glyph_index(font42, glyph));

    (void)copied_glyph_slot(cfdata, gid + GS_MIN_GLYPH_INDEX, &pcg);
    if (pcg != NULL) {
        if (pcg->gdata.data != NULL) {
            gs_free_string(copied->memory, (byte *)pcg->gdata.data, pcg->gdata.size, "Free copied glyph name");
            pcg->gdata.size = 0;
            pcg->gdata.data = NULL;
        }
        pcg->used = 0;
        cfdata->num_glyphs--;
    }

    return 0;
}

static int
copy_glyph_type42(gs_font *font, gs_glyph glyph, gs_font *copied, int options)
{
    gs_glyph_data_t gdata;
    gs_font_type42 *font42 = (gs_font_type42 *)font;
    gs_font_cid2 *fontCID2 = (gs_font_cid2 *)font;
    gs_font_type42 *const copied42 = (gs_font_type42 *)copied;
    uint gid = (options & COPY_GLYPH_BY_INDEX ? glyph - GS_MIN_GLYPH_INDEX :
                font->FontType == ft_CID_TrueType
                    ? fontCID2->cidata.CIDMap_proc(fontCID2, glyph)
                    : font42->data.get_glyph_index(font42, glyph));
    int code;
    int rcode;
    gs_copied_font_data_t *const cfdata = cf_data(copied);
    gs_copied_glyph_t *pcg;
    float sbw[4];
    double factor = font42->data.unitsPerEm;
    int i;

    /* If we've been told to, use the TrueType GSUB table to find a possible replacement
     * glyph for the one which was supplied by the CMAP subtable. This is slightly useful
     * when using a TrueType as a replacement for a missing CIDFont, the CMap defines
     * vertical writing and there is a suitable vertical glyph available to use as a
     * replacement for a horizontal glyph (punctuation, basically). Not a common
     * situation, of rather limited value, but....
     */
    if (!(options & COPY_GLYPH_BY_INDEX) && (options & COPY_GLYPH_USE_GSUB) && font->FontType == ft_CID_TrueType)
        gid = fontCID2->data.substitute_glyph_index_vertical((gs_font_type42 *)font, gid, font->WMode, glyph);

    gdata.memory = font42->memory;
    code = font42->data.get_outline(font42, gid, &gdata);
    /* If the glyph is a /.notdef, and the GID is not 0, and we failed to find
     * the /.notdef, try again with GID 0. We have seen fonts from GraphPad
     * Prism which end up putting the /.notdef in the CharStrings dictionary
     * with the wrong GID value (Bug #691573)
     */
    if (code < 0 && gid != 0) {
        gs_const_string gnstr;
        if (font->procs.glyph_name(font, glyph, &gnstr) >= 0 && gnstr.size == 7
            && !memcmp(gnstr.data, ".notdef", 7)) {
            gid = 0;
            code = font42->data.get_outline(font42, gid, &gdata);
        }
    }
    if (code < 0)
        return code;

    code = copy_glyph_data(font, gid + GS_MIN_GLYPH_INDEX, copied, options,
                           &gdata, NULL, 0);
    if (code < 0)
        return code;
    rcode = code;
    if (glyph < GS_MIN_CID_GLYPH)
        code = copy_glyph_name(font, glyph, copied,
                               gid + GS_MIN_GLYPH_INDEX);
    DISCARD(copied_glyph_slot(cfdata, gid + GS_MIN_GLYPH_INDEX, &pcg)); /* can't fail */
    for (i = 0; i < 2; ++i) {
        if (font42->data.get_metrics(font42, gid, i, sbw) >= 0) {
            int sb = (int)(sbw[i] * factor + 0.5);
            uint width = (uint)(sbw[2 + i] * factor + 0.5);
            byte *pmetrics =
                cfdata->data + copied42->data.metrics[i].offset + gid * 4;

            pmetrics[0] = (byte)(width >> 8);
            pmetrics[1] = (byte)width;
            pmetrics[2] = (byte)(sb >> 8);
            pmetrics[3] = (byte)sb;
            pcg->used |= HAS_SBW0 << i;
        }
        factor = -factor;	/* values are negated for WMode = 1 */
    }
    return (code < 0 ? code : rcode);
}

static gs_glyph
copied_type42_encode_char(gs_font *copied, gs_char chr,
                          gs_glyph_space_t glyph_space)
{
    gs_copied_font_data_t *const cfdata = cf_data(copied);
    const gs_glyph *Encoding = cfdata->Encoding;
    gs_glyph glyph;

    if (chr >= 256 || Encoding == 0)
        return GS_NO_GLYPH;
    glyph = Encoding[chr];
    if (glyph_space == GLYPH_SPACE_INDEX) {
        /* Search linearly for the glyph by name. */
        gs_copied_glyph_t *pcg;
        int code = named_glyph_slot_linear(cfdata, glyph, &pcg);

        if (code < 0 || !pcg->used)
            return GS_NO_GLYPH;
        return GS_MIN_GLYPH_INDEX + (pcg - cfdata->glyphs);
    }
    return glyph;
}

static const gs_copied_font_procs_t copied_procs_type42 = {
    copy_font_type42, copy_glyph_type42, copied_char_add_encoding,
    named_glyph_slot_linear,
    copied_type42_encode_char, gs_type42_glyph_info, gs_type42_glyph_outline,
    uncopy_glyph_type42
};

static inline int
access_type42_data(gs_font_type42 *pfont, ulong base, ulong length,
                   const byte **vptr)
{
    return pfont->data.string_proc(pfont, base, length, vptr);
}

static inline uint
U16(const byte *p)
{
    return ((uint)p[0] << 8) + p[1];
}

static int
same_maxp_values(gs_font_type42 *font0, gs_font_type42 *font1)
{
    gs_type42_data *d0 = &font0->data, *d1 = &font1->data;

    if (d0->maxPoints < d1->maxPoints)
        return 0;
    if (d0->maxContours < d1->maxContours)
        return 0;
    if (d0->maxCPoints < d1->maxCPoints)
        return 0;
    if (d0->maxCContours < d1->maxCContours)
        return 0;
    return 1;
}

static int
same_type42_hinting(gs_font_type42 *font0, gs_font_type42 *font1)
{
    gs_type42_data *d0 = &font0->data, *d1 = &font1->data;
    gs_font_type42 *font[2];
    uint pos[2][3];
    uint len[2][3] = {{0,0,0}, {0,0,0}};
    int i, j, code;

    if (d0->unitsPerEm != d1->unitsPerEm)
        return 0;
    font[0] = font0;
    font[1] = font1;
    memset(pos, 0, sizeof(pos));
    for (j = 0; j < 2; j++) {
        const byte *OffsetTable;
        uint numTables;

        code = access_type42_data(font[j], font[j]->data.subfontOffset, 12, &OffsetTable);
        if (code < 0)
            return code;
        numTables = U16(OffsetTable + 4);
        for (i = 0; i < numTables; ++i) {
            const byte *tab;
            ulong start;
            uint length;

            code = access_type42_data(font[j], font[j]->data.subfontOffset + 12 + i * 16, 16, &tab);
            if (code < 0)
                return code;
            start = get_u32_msb(tab + 8);
            length = get_u32_msb(tab + 12);
            if (!memcmp("prep", tab, 4))
                pos[j][0] = start, len[j][0] = length;
            else if (!memcmp("cvt ", tab, 4))
                pos[j][1] = start, len[j][1] = length;
            else if (!memcmp("fpgm", tab, 4))
                pos[j][2] = start, len[j][2] = length;
        }
    }
    for (i = 0; i < 3; i++) {
        if (len[0][i] != len[1][i])
            return 0;
    }
    for (i = 0; i < 3; i++) {
        if (len[0][i] != 0) {
            const byte *data0, *data1;
            ulong length = len[0][i], size0, size1, size;
            ulong pos0 = pos[0][i], pos1 = pos[1][i];

            while (length > 0) {
                code = access_type42_data(font0, pos0, length, &data0);
                if (code < 0)
                    return code;
                size0 = (code == 0 ? length : code);
                code = access_type42_data(font1, pos1, length, &data1);
                if (code < 0)
                    return code;
                size1 = (code == 0 ? length : code);
                size = min(size0, size1);
                if (memcmp(data0, data1, size))
                    return 0;
                pos0 += size;
                pos1 += size;
                length -= size;
            }
        }
    }
    return 1;
}

/* ------ CIDFont shared ------ */

static int
copy_font_cid_common(gs_font *font, gs_font *copied, gs_font_cid_data *pcdata)
{
    return (copy_string(copied->memory, &pcdata->CIDSystemInfo.Registry,
                        "Registry") |
            copy_string(copied->memory, &pcdata->CIDSystemInfo.Ordering,
                        "Ordering"));
}

/* ------ CIDFontType 0 ------ */

static int
copied_cid0_glyph_data(gs_font_base *font, gs_glyph glyph,
                       gs_glyph_data_t *pgd, int *pfidx)
{
    gs_font_cid0 *fcid0 = (gs_font_cid0 *)font;
    gs_copied_font_data_t *const cfdata = cf_data((gs_font *)font);
    gs_copied_glyph_t *pcg;
    int code = copied_glyph_slot(cfdata, glyph, &pcg);
    int fdbytes = fcid0->cidata.FDBytes;
    int i;

    if (pfidx)
        *pfidx = 0;
    if (code < 0) {
        if (pgd)
            gs_glyph_data_from_null(pgd);
        return_error(gs_error_undefined);
    }
    if (pfidx)
        for (i = 0; i < fdbytes; ++i)
            *pfidx = (*pfidx << 8) + pcg->gdata.data[i];
    if (pgd)
        gs_glyph_data_from_string(pgd, pcg->gdata.data + fdbytes,
                                  pcg->gdata.size - fdbytes, NULL);
    return 0;
}
static int
copied_sub_type1_glyph_data(gs_font_type1 * pfont, gs_glyph glyph,
                            gs_glyph_data_t *pgd)
{
    return
      copied_cid0_glyph_data((gs_font_base *)cf_data((gs_font *)pfont)->parent,
                             glyph, pgd, NULL);
}

static int
cid0_subfont(gs_font *copied, gs_glyph glyph, gs_font_type1 **pfont1)
{
    int fidx;
    int code = copied_cid0_glyph_data((gs_font_base *)copied, glyph, NULL,
                                      &fidx);

    if (code >= 0) {
        gs_font_cid0 *font0 = (gs_font_cid0 *)copied;

        if (fidx >= font0->cidata.FDArray_size)
            return_error(gs_error_unregistered); /* Must not happen. */
        *pfont1 = font0->cidata.FDArray[fidx];
    }
    return code;
}

static int
copied_cid0_glyph_info(gs_font *font, gs_glyph glyph, const gs_matrix *pmat,
                       int members, gs_glyph_info_t *info)
{
    gs_font_type1 *subfont1;
    int code = cid0_subfont(font, glyph, &subfont1);

    if (code < 0)
        return code;
    if (members & GLYPH_INFO_WIDTH1) {
        /* Hack : There is no way to pass WMode from font to glyph_info,
         * and usually CID font has no metrics for WMode 1.
         * Therefore we use FontBBox as default size.
         * Warning : this incompletely implements the request :
         * other requested members are not retrieved.
         */
        gs_font_info_t finfo;
        int code = subfont1->procs.font_info(font, NULL, FONT_INFO_BBOX, &finfo);

        if (code < 0)
            return code;
        info->width[0].x = 0;
        info->width[0].y = 0;
        info->width[1].x = 0;
        info->width[1].y = -finfo.BBox.q.x; /* Sic! */
        info->v.x = finfo.BBox.q.x / 2;
        info->v.y = finfo.BBox.q.y;
        info->members = GLYPH_INFO_WIDTH1;
        return 0;
    }
    return subfont1->procs.glyph_info((gs_font *)subfont1, glyph, pmat,
                                      members, info);
}

static int
copied_cid0_glyph_outline(gs_font *font, int WMode, gs_glyph glyph,
                          const gs_matrix *pmat, gx_path *ppath, double sbw[4])
{
    gs_font_type1 *subfont1;
    int code = cid0_subfont(font, glyph, &subfont1);

    if (code < 0)
        return code;
    return subfont1->procs.glyph_outline((gs_font *)subfont1, WMode, glyph, pmat,
                                         ppath, sbw);
}

static int
copy_font_cid0(gs_font *font, gs_font *copied)
{
    gs_font_cid0 *copied0 = (gs_font_cid0 *)copied;
    gs_copied_font_data_t *const cfdata = cf_data(copied);
    gs_font_type1 **FDArray =
        gs_alloc_struct_array(copied->memory, copied0->cidata.FDArray_size,
                              gs_font_type1 *,
                              &st_gs_font_type1_ptr_element, "FDArray");
    int i = 0, code;

    if (FDArray == 0)
        return_error(gs_error_VMerror);
    code = copy_font_cid_common(font, copied, &copied0->cidata.common);
    if (code < 0)
        goto fail;
    for (; i < copied0->cidata.FDArray_size; ++i) {
        gs_font *subfont = (gs_font *)copied0->cidata.FDArray[i];
        gs_font_type1 *subfont1 = (gs_font_type1 *)subfont;
        gs_font *subcopy;
        gs_font_type1 *subcopy1;
        gs_copied_font_data_t *subdata;

        if (i == 0) {
            /* copy_subrs requires a Type 1 font, even for GSubrs. */
            code = copy_subrs(subfont1, true, &cfdata->global_subrs,
                              copied->memory);
            if (code < 0)
                goto fail;
        }
        code = gs_copy_font(subfont, &subfont->FontMatrix, copied->memory, &subcopy, -1);
        if (code < 0)
            goto fail;
        subcopy1 = (gs_font_type1 *)subcopy;
        subcopy1->data.parent = NULL;
        subdata = cf_data(subcopy);
        subdata->parent = copied0;
        gs_free_object(copied->memory, subdata->Encoding,
                       "copy_font_cid0(Encoding)");
        subdata->Encoding = 0;
        /*
         * Share the glyph data and global_subrs with the parent.  This
         * allows copied_type1_glyph_data in the subfont to do the right
         * thing.
         */
        gs_free_object(copied->memory, subdata->names,
                       "copy_font_cid0(subfont names)");
        gs_free_object(copied->memory, subdata->glyphs,
                       "copy_font_cid0(subfont glyphs)");
        subcopy1->data.procs.glyph_data = copied_sub_type1_glyph_data;
        subdata->glyphs = cfdata->glyphs;
        subdata->glyphs_size = cfdata->glyphs_size;
        subdata->names = 0;
        if (subdata->global_subrs.data != NULL)
            gs_free_object(copied->memory, subdata->global_subrs.data, "copy parent global subrs to child, free child global subrs");
        if (subdata->global_subrs.starts != NULL)
            gs_free_object(copied->memory, subdata->global_subrs.starts, "copy parent global subrs to child, free child global subrs");
        subdata->global_subrs = cfdata->global_subrs;
        FDArray[i] = subcopy1;
    }
    cfdata->notdef = GS_MIN_CID_GLYPH;
    copied0->cidata.FDArray = FDArray;
    copied0->cidata.FDBytes =
        (copied0->cidata.FDArray_size <= 1 ? 0 :
         copied0->cidata.FDArray_size <= 256 ? 1 : 2);
    copied0->cidata.glyph_data = copied_cid0_glyph_data;
    return 0;
 fail:
    while (--i >= 0)
        gs_free_object(copied->memory, FDArray[i], "copy_font_cid0(subfont)");
    gs_free_object(copied->memory, FDArray, "FDArray");
    return code;
}

static int
uncopy_glyph_cid0(gs_font *font, gs_glyph glyph, gs_font *copied, int options)
{
    gs_copied_glyph_t *pcg = NULL;
    gs_copied_font_data_t *cfdata = cf_data(copied);

    (void)copied_glyph_slot(cfdata, glyph, &pcg);
    if (pcg != NULL) {
        if (pcg->gdata.data != NULL) {
            gs_free_string(copied->memory, (byte *)pcg->gdata.data, pcg->gdata.size, "Free copied glyph name");
            pcg->gdata.size = 0;
            pcg->gdata.data = NULL;
        }
        pcg->used = 0;
        cfdata->num_glyphs--;
    }
    return 0;
}

static int
copy_glyph_cid0(gs_font *font, gs_glyph glyph, gs_font *copied, int options)
{
    gs_glyph_data_t gdata;
    gs_font_cid0 *fcid0 = (gs_font_cid0 *)font;
    gs_font_cid0 *copied0 = (gs_font_cid0 *)copied;
    int fdbytes = copied0->cidata.FDBytes;
    int fidx;
    int code;
    byte prefix[MAX_FDBytes];
    int i;

    gdata.memory = font->memory;
    code = fcid0->cidata.glyph_data((gs_font_base *)font, glyph,
                &gdata, &fidx);
    if (code < 0)
        return code;
    for (i = fdbytes - 1; i >= 0; --i, fidx >>= 8)
        prefix[i] = (byte)fidx;
    if (fidx != 0)
        return_error(gs_error_rangecheck);
    return copy_glyph_data(font, glyph, copied, options, &gdata, prefix, fdbytes);
}

static const gs_copied_font_procs_t copied_procs_cid0 = {
    copy_font_cid0, copy_glyph_cid0, copied_no_add_encoding,
    named_glyph_slot_none,
    gs_no_encode_char, copied_cid0_glyph_info, copied_cid0_glyph_outline,
    uncopy_glyph_cid0
};

static int
same_cid0_hinting(const gs_font_cid0 *cfont, const gs_font_cid0 *ofont)
{
    int i;

    if (cfont->cidata.FDArray_size != ofont->cidata.FDArray_size)
        return 0;

    for (i = 0; i < cfont->cidata.FDArray_size; i++) {
        gs_font_type1 *subfont0 = cfont->cidata.FDArray[i];
        gs_font_type1 *subfont1 = ofont->cidata.FDArray[i];
        if (!same_type1_hinting(subfont0, subfont1))
            return 0;
    }
    return 1;
}

/* ------ CIDFontType 2 ------ */

static int
copied_cid2_CIDMap_proc(gs_font_cid2 *fcid2, gs_glyph glyph)
{
    uint cid = glyph - GS_MIN_CID_GLYPH;
    gs_copied_font_data_t *const cfdata = cf_data((gs_font *)fcid2);
    const ushort *CIDMap = cfdata->CIDMap;

    if (glyph < GS_MIN_CID_GLYPH || cid >= fcid2->cidata.common.CIDCount)
        return_error(gs_error_rangecheck);
    if (CIDMap[cid] == 0xffff)
        return -1;
    return CIDMap[cid];
}

static uint
copied_cid2_get_glyph_index(gs_font_type42 *font, gs_glyph glyph)
{
    int glyph_index = copied_cid2_CIDMap_proc((gs_font_cid2 *)font, glyph);

    if (glyph_index < 0)
        return GS_NO_GLYPH;
    return glyph_index;
}

extern_st(st_subst_CID_on_WMode);

static int
copy_font_cid2(gs_font *font, gs_font *copied)
{
    gs_font_cid2 *copied2 = (gs_font_cid2 *)copied;
    gs_copied_font_data_t *const cfdata = cf_data(copied);
    int code;
    int CIDCount = copied2->cidata.common.CIDCount;
    ushort *CIDMap = (ushort *)
        gs_alloc_byte_array(copied->memory, CIDCount, sizeof(ushort),
                            "copy_font_cid2(CIDMap");

    if (CIDMap == 0)
        return_error(gs_error_VMerror);
    code = copy_font_cid_common(font, copied, &copied2->cidata.common);
    if (code < 0 ||
        (code = copy_font_type42(font, copied)) < 0
        ) {
        gs_free_object(copied->memory, CIDMap, "copy_font_cid2(CIDMap");
        return code;
    }
    cfdata->notdef = GS_MIN_CID_GLYPH;
    memset(CIDMap, 0xff, CIDCount * sizeof(*CIDMap));
    cfdata->CIDMap = CIDMap;
    copied2->cidata.MetricsCount = 0;
    copied2->cidata.CIDMap_proc = copied_cid2_CIDMap_proc;
    {
        gs_font_type42 *const copied42 = (gs_font_type42 *)copied;

        copied42->data.get_glyph_index = copied_cid2_get_glyph_index;
    }
    if (copied2->subst_CID_on_WMode) {
        gs_subst_CID_on_WMode_t *subst = NULL;

        rc_alloc_struct_1(subst, gs_subst_CID_on_WMode_t, &st_subst_CID_on_WMode,
                            copied2->memory, return_error(gs_error_VMerror), "copy_font_cid2");
        subst->data[0] = subst->data[1] = 0;
        copied2->subst_CID_on_WMode = subst;
    }

    return 0;
}

static int expand_CIDMap(gs_font_cid2 *copied2, uint CIDCount)
{
    ushort *CIDMap;
    gs_copied_font_data_t *const cfdata = cf_data((gs_font *)copied2);

    if (CIDCount <= copied2->cidata.common.CIDCount)
        return 0;
    CIDMap = (ushort *)
        gs_alloc_byte_array(copied2->memory, CIDCount, sizeof(ushort),
                            "expand_CIDMap(new CIDMap)");
    if (CIDMap == 0)
        return_error(gs_error_VMerror);
    memcpy(CIDMap, cfdata->CIDMap, copied2->cidata.common.CIDCount * sizeof(*CIDMap));
    memset(CIDMap + copied2->cidata.common.CIDCount, 0xFF,
            (CIDCount - copied2->cidata.common.CIDCount) * sizeof(*CIDMap));
    gs_free_object(copied2->memory, cfdata->CIDMap, "expand_CIDMap(old CIDMap)");
    cfdata->CIDMap = CIDMap;
    copied2->cidata.common.CIDCount = CIDCount;
    return 0;
}

static int
uncopy_glyph_cid2(gs_font *font, gs_glyph glyph, gs_font *copied, int options)
{
    gs_copied_glyph_t *pcg = NULL;
    gs_copied_font_data_t *cfdata = cf_data(copied);

    (void)copied_glyph_slot(cf_data(copied), glyph, &pcg);
    if (pcg != NULL) {
        if (pcg->gdata.data != NULL) {
            gs_free_string(copied->memory, (byte *)pcg->gdata.data, pcg->gdata.size, "Free copied glyph name");
            pcg->gdata.size = 0;
            pcg->gdata.data = NULL;
        }
        pcg->used = 0;
        cfdata->num_glyphs--;
    }
    return 0;
}

static int
copy_glyph_cid2(gs_font *font, gs_glyph glyph, gs_font *copied, int options)
{
    gs_font_cid2 *fcid2 = (gs_font_cid2 *)font;
    gs_copied_font_data_t *const cfdata = cf_data(copied);
    gs_font_cid2 *copied2 = (gs_font_cid2 *)copied;
    int gid;
    int code;

    if (!(options & COPY_GLYPH_BY_INDEX)) {
        uint cid = glyph - GS_MIN_CID_GLYPH;
        int CIDCount;

        code = expand_CIDMap(copied2, cid + 1);
        if (code < 0)
            return code;
        CIDCount = copied2->cidata.common.CIDCount;
        gid = fcid2->cidata.CIDMap_proc(fcid2, glyph);
        /* If we've been told to, use the TrueType GSUB table to find a possible replacement
         * glyph for the one which was supplied by the CMAP subtable. This is slightly useful
         * when using a TrueType as a replacement for a missing CIDFont, the CMap defines
         * vertical writing and there is a suitable vertical glyph available to use as a
         * replacement for a horizontal glyph (punctuation, basically). Not a common
         * situation, of rather limited value, but....
         */
        if (options & COPY_GLYPH_USE_GSUB)
            gid = ((gs_font_cid2 *)font)->data.substitute_glyph_index_vertical((gs_font_type42 *)font, gid, font->WMode, glyph);
        if (gid < 0 || gid >= cfdata->glyphs_size)
            return_error(gs_error_rangecheck);
        if (cid > CIDCount)
            return_error(gs_error_invalidaccess);
        if (cfdata->CIDMap[cid] != 0xffff && cfdata->CIDMap[cid] != gid)
            return_error(gs_error_invalidaccess);
        code = copy_glyph_type42(font, glyph, copied, options);
        if (code < 0)
            return code;
        cfdata->CIDMap[cid] = gid;
    } else {
        gid = glyph - GS_MIN_GLYPH_INDEX;
        if (gid < 0 || gid >= cfdata->glyphs_size)
            return_error(gs_error_rangecheck);
        code = copy_glyph_type42(font, glyph, copied, options);
        if (code < 0)
            return code;
    }
    return code;
}

static const gs_copied_font_procs_t copied_procs_cid2 = {
    copy_font_cid2, copy_glyph_cid2, copied_no_add_encoding,
    named_glyph_slot_none,
    gs_no_encode_char, gs_type42_glyph_info, gs_type42_glyph_outline,
    uncopy_glyph_cid2
};

static int
same_cid2_hinting(const gs_font_cid2 *cfont, const gs_font_cid2 *ofont)
{
    return same_type42_hinting((gs_font_type42 *)cfont, (gs_font_type42 *)ofont);
}

/* ---------------- Public ---------------- */

/*
 * Procedure vector for copied fonts.
 */
static font_proc_font_info(copied_font_info);
static font_proc_enumerate_glyph(copied_enumerate_glyph);
static const gs_font_procs copied_font_procs = {
    0,				/* define_font, not supported */
    0,				/* make_font, not supported */
    copied_font_info,
    gs_default_same_font,
    0,				/* encode_char, varies by FontType */
    0,				/* decode_char, not supported */
    copied_enumerate_glyph,
    0,				/* glyph_info, varies by FontType */
    0,				/* glyph_outline, varies by FontType */
    copied_glyph_name,
    gs_default_init_fstack,
    gs_default_next_char_glyph,
    copied_build_char
};

#if GLYPHS_SIZE_IS_PRIME
static const int some_primes[] = {
    /* Arbitrary choosen prime numbers, being reasonable for a Type 1|2 font size.
       We start with 257 to fit 256 glyphs and .notdef .
       Smaller numbers aren't useful, because we don't know whether a font
       will add more glyphs incrementally when we allocate its stable copy.
    */
    257, 359, 521, 769, 1031, 2053,
    3079, 4099, 5101, 6101, 7109, 8209, 10007, 12007, 14009,
    16411, 20107, 26501, 32771, 48857, 65537, 85229, 127837};
#endif

/*
 * Copy a font, aside from its glyphs.
 */
int
gs_copy_font(gs_font *font, const gs_matrix *orig_matrix, gs_memory_t *mem, gs_font **pfont_new, int max_reserved_glyphs)
{
    gs_memory_type_ptr_t fstype = gs_object_type(font->memory, font);
    uint fssize = gs_struct_type_size(fstype);
    gs_font *copied = 0;
    gs_copied_font_data_t *cfdata = 0;
    gs_font_info_t info;
    gs_copied_glyph_t *glyphs = 0;
    uint glyphs_size;
    gs_copied_glyph_name_t *names = 0;
    bool have_names = false;
    const gs_copied_font_procs_t *procs;
    int code;

    /*
     * Check for a supported FontType, and compute the size of its
     * copied glyph table.
     */
    switch (font->FontType) {
    case ft_TrueType:
        procs = &copied_procs_type42;
        glyphs_size = ((gs_font_type42 *)font)->data.trueNumGlyphs;
        have_names = true;
        break;
    case ft_encrypted:
    case ft_encrypted2:
        procs = &copied_procs_type1;
        /* Count the glyphs. */
        glyphs_size = 0;
        {
            int index = 0;
            gs_glyph glyph;

            while (font->procs.enumerate_glyph(font, &index, GLYPH_SPACE_NAME,
                                               &glyph), index != 0)
                ++glyphs_size;
        }
        if(glyphs_size > max_reserved_glyphs && max_reserved_glyphs != -1)
            glyphs_size = max_reserved_glyphs;

#if GLYPHS_SIZE_IS_PRIME
        if (glyphs_size < 257)
            glyphs_size = 257;
        /*
         * Make glyphs_size a prime number to ensure termination of the loop in
         * named_glyphs_slot_hashed, q.v.
         * Also reserve additional slots for the case of font merging and
         * for possible font increments.
         */
        glyphs_size = glyphs_size * 3 / 2;

        { int i;
            for (i = 0; i < count_of(some_primes); i++)
                if (glyphs_size <= some_primes[i])
                    break;
            if (i >= count_of(some_primes))
                return_error(gs_error_rangecheck);
            glyphs_size = some_primes[i];
        }
#else
        /*
         * Make names_size a power of 2 to ensure termination of the loop in
         * named_glyphs_slot_hashed, q.v.
         */
        glyphs_size = glyphs_size * 3 / 2;
        while (glyphs_size & (glyphs_size - 1))
            glyphs_size = (glyphs_size | (glyphs_size - 1)) + 1;
        if (glyphs_size < 256)	/* probably incremental font */
            glyphs_size = 256;
#endif
        have_names = true;
        break;
    case ft_CID_encrypted:
        procs = &copied_procs_cid0;
        /* We used to use the CIDCount here, but for CIDFonts with a GlyphDirectory
         * (dictionary form) the number of CIDs is not the same as the highest CID.
         * Because we use the CID as the slot, we need to assign the highest possible
         * CID, not the number of CIDs. Don't forget to add one because CIDs
         * count from 0.
         */
        glyphs_size = ((gs_font_cid0 *)font)->cidata.common.MaxCID + 1;
        break;
    case ft_CID_TrueType:
        procs = &copied_procs_cid2;
        /* Glyphs are indexed by GID, not by CID. */
        glyphs_size = ((gs_font_cid2 *)font)->data.trueNumGlyphs;
        break;
    default:
        return_error(gs_error_rangecheck);
    }

    /* Get the font_info for copying. */

    memset(&info, 0, sizeof(info));
    info.Flags_requested = ~0;
    code = font->procs.font_info(font, NULL, ~0, &info);

    /* We can ignore a lack of FontInfo for TrueType fonts which
     * are descendants of CID fonts
     */
    if (code < 0 && !(font->FontType == ft_CID_TrueType))
        return code;

    /* Allocate the generic copied information. */

    glyphs = gs_alloc_struct_array(mem, glyphs_size, gs_copied_glyph_t,
                                   &st_gs_copied_glyph_element,
                                   "gs_copy_font(glyphs)");
    if (have_names != 0)
        names = gs_alloc_struct_array(mem, glyphs_size, gs_copied_glyph_name_t,
                                      &st_gs_copied_glyph_name_element,
                                      "gs_copy_font(names)");
    copied = gs_alloc_struct(mem, gs_font, fstype,
                             "gs_copy_font(copied font)");
    if (copied) {
        gs_font_base *bfont = (gs_font_base *)copied;

        /* Initialize the copied font - minumum we need
         * so we can safely free it in the "fail:" case
         * below
         */
        memcpy(copied, font, fssize);
        copied->next = copied->prev = 0;
        copied->memory = mem;
        copied->is_resource = false;
        gs_notify_init(&copied->notify_list, mem);
        copied->base = copied;

        bfont->FAPI = 0;
        bfont->FAPI_font_data = 0;
        bfont->encoding_index = ENCODING_INDEX_UNKNOWN;
        code = uid_copy(&bfont->UID, mem, "gs_copy_font(UID)");
        if (code < 0) {
            uid_set_invalid(&bfont->UID);
            goto fail;
        }
    }
    cfdata = gs_alloc_struct(mem, gs_copied_font_data_t,
                            &st_gs_copied_font_data,
                            "gs_copy_font(wrapper data)");
    if (cfdata)
        memset(cfdata, 0, sizeof(*cfdata));
    if (glyphs == 0 || (names == 0 && have_names) || copied == 0 ||
        cfdata == 0
        ) {
        code = gs_note_error(gs_error_VMerror);
        goto fail;
    }
    cfdata->info = info;

    /* This is somewhat unpleasant. We use the 'glyph' as the unique ID for a number
     * of purposes, but in particular for determining which CharStrings need to be written
     * out by pdfwrite. The 'glyph' appears to be (sometimes) given by the index of the glyph name
     * in the *interpreter* name table. For names in one of the standard encodings
     * we find the name there and use its ID. However, if the glyph name is non-standard
     * then it is added to the interpreter name table and the name index is used to
     * identify the glyph. The problem arises if the font is restored away, and a
     * vmreclaim causes the (now unreferenced) glyph names to be flushed. If we
     * should then use the same font and glyph, its possible that the name table
     * might be different, resulting in a different name index. We would then write
     * duplicate CharStrings to the output, see Bug 687172.
     * The GC enumeration (see top of file) marks the names in the name table to prevent
     * them being flushed. As long as everything is in the same memory allocator this
     * works (ugly though it is). However, if we are using the pdfi PDF interpreter
     * inside the PostScript interpreter, then a problem arises. The pdfwrite device
     * holds on to the font copies until the device is destroyed, by which time the
     * PDF interpreter has already gone. The vmreclaim prior to the device destruction
     * enumerates the name pointers. Because the font was allocated by pdfi these
     * are no longer valid. They are also not needed, since the pdfi name table is
     * not garbage collected..
     * To cater for both conditions we test the memory allocator the font was using.
     * If its a GC'ed allocater then we keep a pointer to the font 'dir' and we enumerate
     * the names and mark them in the interpreter table. Otherwise we don't attempt to
     * mark them. We use dir being NULL to control whether we mark the names.
     */
    if (font->memory != font->memory->non_gc_memory)
        cfdata->dir = font->dir;
    else
        cfdata->dir = NULL;

    if ((code = (copy_string(mem, &cfdata->info.Copyright,
                             "gs_copy_font(Copyright)") |
                 copy_string(mem, &cfdata->info.Notice,
                             "gs_copy_font(Notice)") |
                 copy_string(mem, &cfdata->info.FamilyName,
                             "gs_copy_font(FamilyName)") |
                 copy_string(mem, &cfdata->info.FullName,
                             "gs_copy_font(FullName)"))) < 0
        )
        goto fail;
    /* set the remainder of the copied font contents */
    copied->FontMatrix = *orig_matrix;
    copied->client_data = cfdata;
    copied->procs = copied_font_procs;
    copied->procs.encode_char = procs->encode_char;
    copied->procs.glyph_info = procs->glyph_info;
    copied->procs.glyph_outline = procs->glyph_outline;

    cfdata->procs = procs;
    memset(glyphs, 0, glyphs_size * sizeof(*glyphs));
    cfdata->glyphs = glyphs;
    cfdata->glyphs_size = glyphs_size;
    cfdata->num_glyphs = 0;
    cfdata->ordered = false;
    if (names)
        memset(names, 0, glyphs_size * sizeof(*names));
    cfdata->names = names;
    if (names != 0) {
        uint i;

        for (i = 0; i < glyphs_size; ++i)
            names[i].glyph = GS_NO_GLYPH;
    }

    /* Do FontType-specific initialization. */

    code = procs->finish_copy_font(font, copied);
    if (code < 0)
        goto fail;

    if (cfdata->notdef != GS_NO_GLYPH)
        code = gs_copy_glyph(font, cfdata->notdef, copied);
    if (code < 0)
        gs_free_copied_font(copied);
    else
        *pfont_new = copied;

    return code;

 fail:
    /* Free storage and exit. */
    if (cfdata) {
        uncopy_string(mem, &cfdata->info.FullName,
                      "gs_copy_font(FullName)");
        uncopy_string(mem, &cfdata->info.FamilyName,
                      "gs_copy_font(FamilyName)");
        uncopy_string(mem, &cfdata->info.Notice,
                      "gs_copy_font(Notice)");
        uncopy_string(mem, &cfdata->info.Copyright,
                      "gs_copy_font(Copyright)");
        gs_free_object(mem, cfdata, "gs_copy_font(wrapper data)");
    }
    gs_free_object(mem, copied, "gs_copy_font(copied font)");
    gs_free_object(mem, names, "gs_copy_font(names)");
    gs_free_object(mem, glyphs, "gs_copy_font(glyphs)");
    return code;
}

/* We only need this because the descendant(s) share the parent
 * CIDFont glyph space, so we can't free that if we are a descendant.
 */
static int gs_free_copied_descendant_font(gs_font *font)
{
    gs_copied_font_data_t *cfdata = font->client_data;
    gs_memory_t *mem = font->memory;

    if (cfdata) {
        uncopy_string(mem, &cfdata->info.FullName,
                      "gs_free_copied_font(FullName)");
        uncopy_string(mem, &cfdata->info.FamilyName,
                      "gs_free_copied_font(FamilyName)");
        uncopy_string(mem, &cfdata->info.Notice,
                      "gs_free_copied_font(Notice)");
        uncopy_string(mem, &cfdata->info.Copyright,
                      "gs_free_copied_font(Copyright)");
        if (cfdata->Encoding)
            gs_free_object(mem, cfdata->Encoding, "gs_free_copied_font(Encoding)");
        gs_free_object(mem, cfdata->names, "gs_free_copied_font(names)");
        gs_free_object(mem, cfdata->data, "gs_free_copied_font(data)");
        if (cfdata->subrs.data != NULL)
            gs_free_object(mem, cfdata->subrs.data, "gs_free_copied_font(subrs->data)");
        if (cfdata->subrs.starts != NULL)
            gs_free_object(mem, cfdata->subrs.starts, "gs_free_copied_font(subrs->starts)");
        /* global subrs are 'shared with the parent', see copy_font_cid0()
         * so we don't want to free them here, they are freed by the parent font.
         */
        gs_free_object(mem, cfdata, "gs_free_copied_font(wrapper data)");
    }
    gs_free_object(mem, font, "gs_free_copied_font(copied font)");
    return 0;
}

int gs_free_copied_font(gs_font *font)
{
    gs_copied_font_data_t *cfdata = font->client_data;
    gs_memory_t *mem = font->memory;
    int i, code;
    gs_copied_glyph_t *pcg = 0;
    gs_copied_glyph_name_t *pcgn = 0;

    /* For CID fonts, we must also free the descendants, which we copied
     * at the time we copied the actual CIDFont itself
     */
    if (font->FontType == ft_CID_encrypted) {
        gs_font_cid0 *copied0 = (gs_font_cid0 *)font;

        for (i = 0; i < copied0->cidata.FDArray_size; ++i) {
            code = gs_free_copied_descendant_font((gs_font *)copied0->cidata.FDArray[i]);
            if (code < 0)
                return code;
        }
        gs_free_object(mem, copied0->cidata.FDArray, "free copied CIDFont FDArray");
        copied0->cidata.FDArray = 0;
        gs_free_string(mem, (byte *)copied0->cidata.common.CIDSystemInfo.Registry.data, copied0->cidata.common.CIDSystemInfo.Registry.size, "Free copied Registry");
        gs_free_string(mem, (byte *)copied0->cidata.common.CIDSystemInfo.Ordering.data, copied0->cidata.common.CIDSystemInfo.Ordering.size, "Free copied Registry");
        copied0->cidata.common.CIDSystemInfo.Registry.data = copied0->cidata.common.CIDSystemInfo.Ordering.data = NULL;
        copied0->cidata.common.CIDSystemInfo.Registry.size = copied0->cidata.common.CIDSystemInfo.Ordering.size = 0;
    }

    if (font->FontType == ft_CID_TrueType) {
        gs_font_cid2 *copied2 = (gs_font_cid2 *)font;

        if (copied2->subst_CID_on_WMode)
            rc_decrement(copied2->subst_CID_on_WMode, "gs_free_copied_font(subst_CID_on_WMode");
        gs_free_string(mem, (byte *)copied2->cidata.common.CIDSystemInfo.Registry.data, copied2->cidata.common.CIDSystemInfo.Registry.size, "Free copied Registry");
        gs_free_string(mem, (byte *)copied2->cidata.common.CIDSystemInfo.Ordering.data, copied2->cidata.common.CIDSystemInfo.Ordering.size, "Free copied Registry");
        copied2->cidata.common.CIDSystemInfo.Registry.data = copied2->cidata.common.CIDSystemInfo.Ordering.data = NULL;
        copied2->cidata.common.CIDSystemInfo.Registry.size = copied2->cidata.common.CIDSystemInfo.Ordering.size = 0;
    }

    if (cfdata) {
        /* free copied glyph data */
        for (i=0;i < cfdata->glyphs_size;i++) {
            pcg = &cfdata->glyphs[i];
            if(pcg->gdata.data != NULL) {
                gs_free_string(font->memory, (byte *)pcg->gdata.data, pcg->gdata.size, "Free copied glyph");
            }
            if (cfdata->names) {
                pcgn = &cfdata->names[i];
                if (pcgn->str.data != NULL) {
                    if (!gs_is_c_glyph_name(pcgn->str.data, pcgn->str.size))
                        gs_free_string(font->memory, (byte *)pcgn->str.data, pcgn->str.size, "Free copied glyph name");
                }
            }
        }
        if (cfdata->extra_names) {
            gs_copied_glyph_extra_name_t *extra_name = cfdata->extra_names, *next;

            while (extra_name != NULL) {
                next = extra_name->next;
                if (!gs_is_c_glyph_name(extra_name->name.str.data, extra_name->name.str.size))
                    gs_free_string(font->memory, (byte *)extra_name->name.str.data, extra_name->name.str.size, "Free extra name string");
                gs_free_object(font->memory, extra_name, "free copied font(extra_names)");
                extra_name = next;
            }
            cfdata->extra_names = NULL;
        }

        uncopy_string(mem, &cfdata->info.FullName,
                      "gs_free_copied_font(FullName)");
        uncopy_string(mem, &cfdata->info.FamilyName,
                      "gs_free_copied_font(FamilyName)");
        uncopy_string(mem, &cfdata->info.Notice,
                      "gs_free_copied_font(Notice)");
        uncopy_string(mem, &cfdata->info.Copyright,
                      "gs_free_copied_font(Copyright)");
        if (cfdata->subrs.data != NULL)
            gs_free_object(mem, cfdata->subrs.data, "gs_free_copied_font(subrs.data)");
        if (cfdata->subrs.starts != NULL)
            gs_free_object(mem, cfdata->subrs.starts, "gs_free_copied_font(subrs.dtarts)");
        if (cfdata->global_subrs.data !=  NULL)
            gs_free_object(mem, cfdata->global_subrs.data, "gs_free_copied_font(gsubrs.data)");
        if (cfdata->global_subrs.starts !=  NULL)
            gs_free_object(mem, cfdata->global_subrs.starts, "gs_free_copied_font(gsubrs.starts)");
        if (cfdata->Encoding)
            gs_free_object(mem, cfdata->Encoding, "gs_free_copied_font(Encoding)");
        if (cfdata->CIDMap)
            gs_free_object(mem, cfdata->CIDMap, "gs_free_copied_font(CIDMap)");
        gs_free_object(mem, cfdata->glyphs, "gs_free_copied_font(glyphs)");
        gs_free_object(mem, cfdata->names, "gs_free_copied_font(names)");
        gs_free_object(mem, cfdata->data, "gs_free_copied_font(data)");
        gs_free_object(mem, cfdata, "gs_free_copied_font(wrapper data)");
    }
    gs_free_object(mem, font, "gs_free_copied_font(copied font)");
    return 0;
}

/*
 * Copy a glyph, including any sub-glyphs.
 */
int
gs_copy_glyph(gs_font *font, gs_glyph glyph, gs_font *copied)
{
    return gs_copy_glyph_options(font, glyph, copied, 0);
}
int
gs_copy_glyph_options(gs_font *font, gs_glyph glyph, gs_font *copied,
                      int options)
{
    int code;
#define MAX_GLYPH_PIECES 64	/* arbitrary, but 32 is too small - bug 687698. */
    gs_glyph glyphs[MAX_GLYPH_PIECES];
    uint count = 1, i;
    gs_copied_font_data_t *cfdata = NULL;

    if (copied->procs.font_info != copied_font_info)
        return_error(gs_error_rangecheck);
    cfdata = cf_data(copied);
    code = cfdata->procs->copy_glyph(font, glyph, copied, options);
    if (code != 0)
        return code;
    /* Copy any sub-glyphs. */
    glyphs[0] = glyph;
    code = psf_add_subset_pieces(glyphs, &count, MAX_GLYPH_PIECES, MAX_GLYPH_PIECES,
                          font);
    if (code < 0)
        return code;
    if (count > MAX_GLYPH_PIECES)
        return_error(gs_error_limitcheck);
    for (i = 1; i < count; ++i) {
        code = gs_copy_glyph_options(font, glyphs[i], copied,
                                     (options & ~COPY_GLYPH_NO_OLD) | COPY_GLYPH_BY_INDEX);
        if (code < 0) {
            int j = 0;

            for (j = 0; j < i; j++) {
                if (cfdata->procs->uncopy_glyph != NULL)
                    (void)cfdata->procs->uncopy_glyph(font, glyph, copied, options);
            }
            return code;
        }
        /* if code > 0 then we already have the glyph, so no need to process further.
         * If the original glyph was not a CID then we are copying by name, not by index.
         * But the copy above copies by index which means we don't have an entry for
         * the glyp-h component in the name table. If we are using names then we
         * absolutely *must* have an entry in the name table, so go ahead and add
         * one here. Note that the array returned by psf_add_subset_pieces has the
         * GIDs with an offset of GS_MIN_GLYPH_INDEX added. Previously we removed this
         * offset, but if the resulting GID referenced a name already in use (or later used)
         * then the generated CMAP was incorrect. By leaving the offset in place we get
         * a name generated (numeric name based on GID) which gurantees no name collisions.
         * (Bug #693444).
         */
        if (code == 0 && glyph < GS_MIN_CID_GLYPH && glyphs[i] > GS_MIN_GLYPH_INDEX) {
            code = copy_glyph_name(font, glyphs[i], copied,
                               glyphs[i]);
            if (code < 0)
                return code;
        }
    }
    /*
     * Because 'seac' accesses the Encoding of the font as well as the
     * glyphs, we have to copy the Encoding entries as well.
     */
    if (count == 1)
        return 0;
    switch (font->FontType) {
    case ft_encrypted:
    case ft_encrypted2:
        break;
    default:
        return 0;
    }
#if 0 /* No need to add subglyphs to the Encoding because they always are
         taken from StandardEncoding (See the Type 1 spec about 'seac').
         Attempt to add them to the encoding can cause a conflict,
         if the encoding specifies different glyphs for these char codes
         (See the bug #687172). */
    {
        gs_copied_glyph_t *pcg;
        gs_glyph_data_t gdata;
        gs_char chars[2];

        gdata.memory = font->memory;
        /* Since we just copied the glyph, copied_glyph_slot can't fail. */
        DISCARD(copied_glyph_slot(cf_data(copied), glyph, &pcg));
        gs_glyph_data_from_string(&gdata, pcg->gdata.data, pcg->gdata.size,
                                  NULL);
        code = gs_type1_piece_codes((gs_font_type1 *)font, &gdata, chars);
        if (code <= 0 ||	/* 0 is not possible here */
            (code = gs_copied_font_add_encoding(copied, chars[0], glyphs[1])) < 0 ||
            (code = gs_copied_font_add_encoding(copied, chars[1], glyphs[2])) < 0
            )
            return code;
    }
#endif
    return 0;
#undef MAX_GLYPH_PIECES
}

/*
 * Add an Encoding entry to a copied font.  The glyph need not already have
 * been copied.
 */
int
gs_copied_font_add_encoding(gs_font *copied, gs_char chr, gs_glyph glyph)
{
    gs_copied_font_data_t *const cfdata = cf_data(copied);

    if (copied->procs.font_info != copied_font_info)
        return_error(gs_error_rangecheck);
    return cfdata->procs->add_encoding(copied, chr, glyph);
}

/*
 * Copy all the glyphs and, if relevant, Encoding entries from a font.  This
 * is equivalent to copying the glyphs and Encoding entries individually,
 * and returns errors under the same conditions.
 */
int
gs_copy_font_complete(gs_font *font, gs_font *copied)
{
    int index, code = 0;
    gs_glyph_space_t space = GLYPH_SPACE_NAME;
    gs_glyph glyph;

    /*
     * For Type 1 fonts and CIDFonts, enumerating the glyphs using
     * GLYPH_SPACE_NAME will cover all the glyphs.  (The "names" of glyphs
     * in CIDFonts are CIDs, but that is not a problem.)  For Type 42 fonts,
     * however, we have to copy by name once, so that we also copy the
     * name-to-GID mapping (the CharStrings dictionary in PostScript), and
     * then copy again by GID, to cover glyphs that don't have names.
     */
    for (;;) {
        for (index = 0;
             code >= 0 &&
                 (font->procs.enumerate_glyph(font, &index, space, &glyph),
                  index != 0);
            ) {
            if (font->FontType == ft_TrueType &&
                    ((glyph >= GS_MIN_CID_GLYPH && glyph < GS_MIN_GLYPH_INDEX) || glyph == GS_NO_GLYPH ||
                    (space == GLYPH_SPACE_INDEX && glyph < GS_MIN_GLYPH_INDEX)))
                return_error(gs_error_invalidfont); /* bug 688370. */
            code = gs_copy_glyph(font, glyph, copied);
        }
        /* For Type 42 fonts, if we copied by name, now copy again by index. */
        if (space == GLYPH_SPACE_NAME && font->FontType == ft_TrueType)
            space = GLYPH_SPACE_INDEX;
        else
            break;
    }
    if (cf_data(copied)->Encoding != 0)
        for (index = 0; code >= 0 && index < 256; ++index) {
            glyph = font->procs.encode_char(font, (gs_char)index,
                                            GLYPH_SPACE_NAME);
            if (glyph != GS_NO_GLYPH) {
                code = gs_copied_font_add_encoding(copied, (gs_char)index,
                                                   glyph);
                if (code == gs_error_undefined) {
                    /* Skip Encoding entries, which point to undefiuned glyphs -
                       happens with 033-52-5873.pdf. */
                    code = 0;
                }
                if (code == gs_error_rangecheck) {
                    /* Skip Encoding entries, which point to undefiuned glyphs -
                       happens with 159.pdf. */
                    code = 0;
                }
            }
        }
    if (copied->FontType != ft_composite) {
        gs_font_base *bfont = (gs_font_base *)font;
        gs_font_base *bcopied = (gs_font_base *)copied;

        bcopied->encoding_index = bfont->encoding_index;
        bcopied->nearest_encoding_index = bfont->nearest_encoding_index;
    }
    return code;
}

/*
 * Check whether specified glyphs can be copied from another font.
 * It means that (1) fonts have same hinting parameters and
 * (2) font subsets for the specified glyph set don't include different
 * outlines or metrics. Possible returned values :
 * 0 (incompatible), 1 (compatible), < 0 (error)
 */
int
gs_copied_can_copy_glyphs(const gs_font *cfont, const gs_font *ofont,
                          gs_glyph *glyphs, int num_glyphs, int glyphs_step,
                          bool check_hinting)
{
    int code = 0;

    if (cfont == ofont)
        return 1;
    if (cfont->FontType != ofont->FontType)
        return 0;
    if (cfont->WMode != ofont->WMode)
        return 0;
    if (cfont->font_name.size == 0 || ofont->font_name.size == 0) {
        if (cfont->key_name.size != ofont->key_name.size ||
            memcmp(cfont->key_name.chars, ofont->key_name.chars,
                        cfont->font_name.size))
            return 0; /* Don't allow to merge random fonts. */
    } else {
        if (cfont->font_name.size != ofont->font_name.size ||
            memcmp(cfont->font_name.chars, ofont->font_name.chars,
                            cfont->font_name.size))
            return 0; /* Don't allow to merge random fonts. */
    }
    if (check_hinting) {
        switch(cfont->FontType) {
            case ft_encrypted:
            case ft_encrypted2:
                if (!same_type1_hinting((const gs_font_type1 *)cfont,
                                        (const gs_font_type1 *)ofont))
                    return 0;
                code = 1;
                break;
            case ft_TrueType:
                code = same_type42_hinting((gs_font_type42 *)cfont,
                                        (gs_font_type42 *)ofont);
                if (code > 0)
                    code = same_maxp_values((gs_font_type42 *)cfont,
                                        (gs_font_type42 *)ofont);
                break;
            case ft_CID_encrypted:
                if (!gs_is_CIDSystemInfo_compatible(
                                gs_font_cid_system_info(cfont),
                                gs_font_cid_system_info(ofont)))
                    return 0;
                code = same_cid0_hinting((const gs_font_cid0 *)cfont,
                                         (const gs_font_cid0 *)ofont);
                break;
            case ft_CID_TrueType:
                if (!gs_is_CIDSystemInfo_compatible(
                                gs_font_cid_system_info(cfont),
                                gs_font_cid_system_info(ofont)))
                    return 0;
                code = same_cid2_hinting((const gs_font_cid2 *)cfont,
                                         (const gs_font_cid2 *)ofont);
                if (code > 0)
                    code = same_maxp_values((gs_font_type42 *)cfont,
                                        (gs_font_type42 *)ofont);
                break;
            default:
                return_error(gs_error_unregistered); /* Must not happen. */
        }
        if (code <= 0) /* an error or false */
            return code;
    }
    return compare_glyphs(cfont, ofont, glyphs, num_glyphs, glyphs_step, 0);
}

/* Extension glyphs may be added to a font to resolve
   glyph name conflicts while conwerting a PDF Widths into Metrics.
   This function drops them before writing out an embedded font. */
int
copied_drop_extension_glyphs(gs_font *copied)
{
    /* When we encounter a glyph used at multiple encoding positions, and
     * the encoding positions have different Widths, we end up defining
     * a new glyph name, because we can't have a PostScript glyph which has
     * two sets of metrics. Here we are supposed to find such duplicates
     * and 'drop' them. It appears that the original intention was to mark
     * the 'slot'->used member as false, with the expectation that this
     * would drop the glyph from the font.
     */
    /* 	Note : This function drops 'used' flags for some glyphs
        and truncates glyph names. Can't use the font
        for outlining|rasterization|width after applying it.
     */
    gs_copied_font_data_t *const cfdata = cf_data(copied);
    uint gsize = cfdata->glyphs_size, ext_name;
    const int sl = strlen(gx_extendeg_glyph_name_separator);

    for (ext_name = 0; ext_name < gsize; ext_name++) {
        gs_copied_glyph_t *pslot = &cfdata->glyphs[ext_name];
        gs_copied_glyph_name_t *name;
        int l, j, k, non_ext_name;

        if (!pslot->used)
            continue;
        name = &cfdata->names[ext_name];
        l = name->str.size - sl;

        for (j = 0; j < l; j ++)
            if (!memcmp(gx_extendeg_glyph_name_separator, name->str.data + j, sl))
                break;
        if (j >= l)
            continue;
        /* Found an extension name.
           Find the corresponding non-extended one. */
        non_ext_name = ext_name;
        for (k = 0; k < gsize; k++)
            if (cfdata->glyphs[k].used &&
                    cfdata->names[k].str.size == j &&
                    !memcmp(cfdata->names[k].str.data, name->str.data, j) &&
                    !bytes_compare(pslot->gdata.data, pslot->gdata.size,
                            cfdata->glyphs[k].gdata.data, cfdata->glyphs[k].gdata.size)) {
                non_ext_name = k;
                break;
            }
        /* Drop others with same prefix. */
        for (k = 0; k < gsize; k++)
            if (k != non_ext_name && cfdata->glyphs[k].used &&
                    cfdata->names[k].str.size >= j + sl &&
                    !memcmp(cfdata->names[k].str.data, name->str.data, j) &&
                    !memcmp(gx_extendeg_glyph_name_separator, name->str.data + j, sl) &&
                    !bytes_compare(pslot->gdata.data, pslot->gdata.size,
                    cfdata->glyphs[k].gdata.data, cfdata->glyphs[k].gdata.size)) {
                cfdata->glyphs[k].used = false;
                cfdata->names[k].str.size = j;
            }
        /* Truncate the extended glyph name. */
        cfdata->names[ext_name].str.size = j;
    }
    return 0;
}

static int
compare_glyph_names(const void *pg1, const void *pg2)
{
    const gs_copied_glyph_name_t * gn1 = *(const gs_copied_glyph_name_t **)pg1;
    const gs_copied_glyph_name_t * gn2 = *(const gs_copied_glyph_name_t **)pg2;

    return bytes_compare(gn1->str.data, gn1->str.size, gn2->str.data, gn2->str.size);
}

/* Order font data to avoid a serialization indeterminism. */
static int
order_font_data(gs_copied_font_data_t *cfdata, gs_memory_t *memory)
{
    int i, j = 0;

    gs_copied_glyph_name_t **a = (gs_copied_glyph_name_t **)gs_alloc_byte_array(memory, cfdata->num_glyphs,
        sizeof(gs_copied_glyph_name_t *), "order_font_data");
    if (a == NULL)
        return_error(gs_error_VMerror);
    j = 0;
    for (i = 0; i < cfdata->glyphs_size; i++) {
        if (cfdata->glyphs[i].used) {
            if (j >= cfdata->num_glyphs) {
                gs_free_object(memory, a, "order_font_data");
                return_error(gs_error_unregistered); /* Must not happen */
            }
            a[j++] = &cfdata->names[i];
        }
    }
    qsort(a, j, sizeof(*a), compare_glyph_names);
    for (j--; j >= 0; j--)
        cfdata->glyphs[j].order_index = a[j] - cfdata->names;
    gs_free_object(memory, a, "order_font_data");
    return 0;
}

/* Order font to avoid a serialization indeterminism. */
int
copied_order_font(gs_font *font)
{

    if (font->procs.enumerate_glyph != copied_enumerate_glyph)
        return_error(gs_error_unregistered); /* Must not happen */
    if (font->FontType != ft_encrypted && font->FontType != ft_encrypted2) {
         /* Don't need to order, because it is ordered by CIDs or glyph indices. */
        return 0;
    }
    {	gs_copied_font_data_t * cfdata = cf_data(font);
        cfdata->ordered = true;
        return order_font_data(cfdata, font->memory);
    }
}

/* Get .nmotdef glyph. */
gs_glyph
copied_get_notdef(const gs_font *font)
{
    gs_copied_font_data_t * cfdata = cf_data(font);

    return cfdata->notdef;
}
