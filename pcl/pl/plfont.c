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


/* plfont.c */
/* PCL font handling library -- operations on entire fonts */
#include "memory_.h"
#include "stdio_.h"
#include "gdebug.h"
#include "gp.h"
#include "gserrors.h"
#include "gstypes.h"
#include "gsmemory.h"
#include "gsstruct.h"
#include "gsmatrix.h"
#include "gsstate.h"
#include "gschar.h"
#include "gsimage.h"
#include "gsutil.h"
#include "gxfcache.h"
#include "gxfont.h"
#include "gxfont42.h"
#include "gzstate.h"
#include "plfont.h"
#include "plvalue.h"
#include "plchar.h"
#include "strmio.h"
#include "stream.h"

#include "plfapi.h"

/* Structure descriptors */
private_st_pl_font();

/* Define accessors for unaligned, big-endian quantities. */
#define u16(bptr) pl_get_uint16(bptr)
#define s16(bptr) pl_get_int16(bptr)
/**** ASSUME uint >= 32 BITS ****/
#define u32(bptr) (uint)pl_get_uint32(bptr)

/* ---------------- Utilities ---------------- */
/* Free a font.  This is the freeing procedure in the font dictionary. */
void
pl_free_font(gs_memory_t * mem, void *plf, client_name_t cname)
{
    pl_font_t *plfont = plf;

    /* Free the characters. */
    if (!plfont->data_are_permanent) {
        if (plfont->glyphs.table) {
            uint i;

            for (i = plfont->glyphs.size; i > 0;) {
                void *data = (void *)plfont->glyphs.table[--i].data;

                if (data)
                    gs_free_object(mem, data, cname);
            }
        }
        gs_free_object(mem, (void *)plfont->header, cname);
        plfont->header = 0;     /* see hack note above */
    }

    /* free any nodes in the widths cache */
    pl_font_glyph_width_cache_remove_nodes(plfont);

    /* Free the font data itself. */
    gs_free_object(mem, (void *)plfont->char_glyphs.table, cname);
    gs_free_object(mem, (void *)plfont->glyphs.table, cname);
    if (plfont->pfont) {        /* might be only partially constructed */
        gs_purge_font_from_char_caches_completely(plfont->pfont);
        (void)gs_purge_font(plfont->pfont);
        gs_free_object(mem, plfont->pfont, cname);
    }
    if (plfont->font_file) {
        gs_free_object(mem, plfont->font_file, cname);
        plfont->font_file = 0;
    }
    if (plfont->names != NULL) {
        int i = 0;
        for (i = 0;i < plfont->next_name_index; i++)
            gs_free_object(mem, plfont->names[i], "freeing names table");
        gs_free_object(mem, plfont->names, "free names table");
        plfont->names = NULL;
        plfont->max_name_index = plfont->next_name_index = 0;
    }
    gs_free_object(mem, plf, cname);
}

/* ---------------- Library callbacks ---------------- */

const char *pl_mac_names[258] = {
    ".notdef",
    ".null",
    "nonmarkingreturn",
    "space",
    "exclam",
    "quotedbl",
    "numbersign",
    "dollar",
    "percent",
    "ampersand",
    "quotesingle",
    "parenleft",
    "parenright",
    "asterisk",
    "plus",
    "comma",
    "hyphen",
    "period",
    "slash",
    "zero",
    "one",
    "two",
    "three",
    "four",
    "five",
    "six",
    "seven",
    "eight",
    "nine",
    "colon",
    "semicolon",
    "less",
    "equal",
    "greater",
    "question",
    "at",
    "A",
    "B",
    "C",
    "D",
    "E",
    "F",
    "G",
    "H",
    "I",
    "J",
    "K",
    "L",
    "M",
    "N",
    "O",
    "P",
    "Q",
    "R",
    "S",
    "T",
    "U",
    "V",
    "W",
    "X",
    "Y",
    "Z",
    "bracketleft",
    "backslash",
    "bracketright",
    "asciicircum",
    "underscore",
    "grave",
    "a",
    "b",
    "c",
    "d",
    "e",
    "f",
    "g",
    "h",
    "i",
    "j",
    "k",
    "l",
    "m",
    "n",
    "o",
    "p",
    "q",
    "r",
    "s",
    "t",
    "u",
    "v",
    "w",
    "x",
    "y",
    "z",
    "braceleft",
    "bar",
    "braceright",
    "asciitilde",
    "Adieresis",
    "Aring",
    "Ccedilla",
    "Eacute",
    "Ntilde",
    "Odieresis",
    "Udieresis",
    "aacute",
    "agrave",
    "acircumflex",
    "adieresis",
    "atilde",
    "aring",
    "ccedilla",
    "eacute",
    "egrave",
    "ecircumflex",
    "edieresis",
    "iacute",
    "igrave",
    "icircumflex",
    "idieresis",
    "ntilde",
    "oacute",
    "ograve",
    "ocircumflex",
    "odieresis",
    "otilde",
    "uacute",
    "ugrave",
    "ucircumflex",
    "udieresis",
    "dagger",
    "degree",
    "cent",
    "sterling",
    "section",
    "bullet",
    "paragraph",
    "germandbls",
    "registered",
    "copyright",
    "trademark",
    "acute",
    "dieresis",
    "notequal",
    "AE",
    "Oslash",
    "infinity",
    "plusminus",
    "lessequal",
    "greaterequal",
    "yen",
    "mu",
    "partialdiff",
    "summation",
    "product",
    "pi",
    "integral",
    "ordfeminine",
    "ordmasculine",
    "Omega",
    "ae",
    "oslash",
    "questiondown",
    "exclamdown",
    "logicalnot",
    "radical",
    "florin",
    "approxequal",
    "Delta",
    "guillemotleft",
    "guillemotright",
    "ellipsis",
    "nonbreakingspace",
    "Agrave",
    "Atilde",
    "Otilde",
    "OE",
    "oe",
    "endash",
    "emdash",
    "quotedblleft",
    "quotedblright",
    "quoteleft",
    "quoteright",
    "divide",
    "lozenge",
    "ydieresis",
    "Ydieresis",
    "fraction",
    "currency",
    "guilsinglleft",
    "guilsinglright",
    "fi",
    "fl",
    "daggerdbl",
    "periodcentered",
    "quotesinglbase",
    "quotedblbase",
    "perthousand",
    "Acircumflex",
    "Ecircumflex",
    "Aacute",
    "Edieresis",
    "Egrave",
    "Iacute",
    "Icircumflex",
    "Idieresis",
    "Igrave",
    "Oacute",
    "Ocircumflex",
    "apple",
    "Ograve",
    "Uacute",
    "Ucircumflex",
    "Ugrave",
    "dotlessi",
    "circumflex",
    "tilde",
    "macron",
    "breve",
    "dotaccent",
    "ring",
    "cedilla",
    "hungarumlaut",
    "ogonek",
    "caron",
    "Lslash",
    "lslash",
    "Scaron",
    "scaron",
    "Zcaron",
    "zcaron",
    "brokenbar",
    "Eth",
    "eth",
    "Yacute",
    "yacute",
    "Thorn",
    "thorn",
    "minus",
    "multiply",
    "onesuperior",
    "twosuperior",
    "threesuperior",
    "onehalf",
    "onequarter",
    "threequarters",
    "franc",
    "Gbreve",
    "gbreve",
    "Idotaccent",
    "Scedilla",
    "scedilla",
    "Cacute",
    "cacute",
    "Ccaron",
    "ccaron",
    "dcroat"
};

static int
pl_glyph_name(gs_font * pfont, gs_glyph glyph, gs_const_string * pstr)
{
    uint table_length;
    ulong table_offset;
    pl_font_t * plfont = (pl_font_t *)pfont->client_data;

    if (glyph >= GS_MIN_GLYPH_INDEX)
        glyph -= GS_MIN_GLYPH_INDEX;

    /* guess if the font type is not truetype */
    if (pfont->FontType != ft_TrueType) {
        glyph -= 29;
        if (glyph < 258) {
            pstr->data = (const byte *)pl_mac_names[glyph];
            pstr->size = strlen((const char *)pstr->data);
            return 0;
        } else {
            if_debug1m('=', pfont->memory,
                       "[=]glyph index %lx out of range\n", (ulong) glyph);
            return -1;
        }
    }

    table_offset =
        tt_find_table((gs_font_type42 *) pfont, "post", &table_length);
    /* no post table */
    if (table_offset == 0)
        return -1;
    /* this shoudn't happen but... */
    if (table_length == 0)
        return -1;

    {
        ulong format;
        int numGlyphs;
        uint glyph_name_index;
        const byte *postp;      /* post table pointer */

        ((gs_font_type42 *) pfont)->data.string_proc((gs_font_type42 *) pfont,
                                                     table_offset,
                                                     table_length, &postp);
        format = u32(postp);
        if (format != 0x20000) {
            /* format 1.0 (mac encoding) is a simple table see the TT
               spec.  We don't implement this because we don't see it
               in practice */
            dmprintf1(pfont->memory, "unknown post table format %lX\n",
                      format);
            return -1;
        }
        /* skip over the post header */
        numGlyphs = (int)u16(postp + 32);
        if ((int)glyph > numGlyphs - 1) {
            if_debug1m('=', pfont->memory,
                       "[=]glyph index %lx out of range\n", glyph);
            return -1;
        }
        /* glyph name index starts at post + 34 each entry is 2 bytes */
        glyph_name_index = u16(postp + 34 + (glyph * 2));
        /* this shouldn't happen */
        if (glyph_name_index > 0x7fff)
            return -1;
        /* mac easy */
        if (glyph_name_index < 258) {

            pstr->data = (const byte *)pl_mac_names[glyph_name_index];
            pstr->size = strlen((const char *)pstr->data);
            return 0;
            /* not mac */
        } else {
            byte *mydata;
            /* and here's the tricky part */
            const byte *pascal_stringp = postp + 34 + (numGlyphs * 2);
            /* 0 - 257 lives in the mac table above */
            glyph_name_index -= 258;
            /* The string we want is the index'th pascal string,
               so we "hop" to each length byte "index" times. */
            while (glyph_name_index > 0) {
                pascal_stringp += ((int)(*pascal_stringp) + 1);
                glyph_name_index--;
            }
            /* length byte */
            pstr->size = (int)(*pascal_stringp);
            /* + 1 is for the length byte */
            pstr->data = pascal_stringp + 1;
            /* sanity check */
            if (pstr->data + pstr->size > postp + table_length ||
                pstr->data - 1 < postp) {
                dmprintf(pfont->memory, "data out of range\n");
                return -1;
            }
            /* sigh - we have to allocate a copy of the data - by the
               time a high level device makes use of it the font data
               may be freed.  Track the allocated memory in our
               font 'wrapper' so we can free it when we free tha font wrapper.
             */
            mydata =
                gs_alloc_bytes(pfont->memory, pstr->size + 1,
                               "glyph to name");
            if (mydata == 0)
                return -1;
            memcpy(mydata, pascal_stringp + 1, pstr->size);
            pstr->data = mydata;
            if (plfont->names == NULL) {
                plfont->names = (char **)gs_alloc_bytes(pfont->memory, 256 * sizeof (char *), "names storage");
                if (plfont->names == NULL) {
                    gs_free_object(pfont->memory, (byte *)pstr->data, "free string on error");
                    pstr->data = NULL;
                    pstr->size = 0;
                    return -1;
                }
                plfont->max_name_index = 255;
                plfont->next_name_index = 0;
                memset(plfont->names, 0x00, 256 * sizeof (char *));
            }
            if (plfont->next_name_index > plfont->max_name_index) {
                char **temp = NULL;
                temp = (char **)gs_alloc_bytes(pfont->memory, (plfont->max_name_index + 256) * sizeof (char *), "names storage");
                if (temp == NULL) {
                    gs_free_object(pfont->memory, (byte *)pstr->data, "free string on error");
                    pstr->data = NULL;
                    pstr->size = 0;
                    return -1;
                }
                memset(temp, 0x00, (plfont->max_name_index + 256) * sizeof (char *));
                memcpy(temp, plfont->names, plfont->max_name_index * sizeof(char *));
                gs_free_object(pfont->memory, (void *)plfont->names, "realloc names storage");
                plfont->names = temp;
                plfont->max_name_index += 256;
            }
            plfont->names[plfont->next_name_index++] = (char *)pstr->data;
            return 0;
        }
    }
    return 0;
}

/* Get the unicode valude for a glyph */
static int
pl_decode_glyph(gs_font * font, gs_glyph glyph, int ch, ushort *unicode_return, unsigned int length)
{
    unsigned char *ucode = (unsigned char *)unicode_return;

    if (ch < 0 || ch > 255)
        return (int) GS_NO_CHAR;

    if (length == 0)
        return 2;

#if ARCH_IS_BIG_ENDIAN
    *unicode_return = (ushort)ch;
#else
    ucode[0] = 0x00;
    ucode[1] = ch & 0xff;
#endif
    return 2;
}

/* ---------------- Width cache ---------------- */
static int
pl_font_glyph_width_cache_node_add(pl_font_t *plfont,
                              uint char_code, gs_point * pwidth)
{
    pl_glyph_width_node_t *node;

    /* We can't safely cache widths for bitmap fonts */
    if (plfont->scaling_technology == plfst_bitmap) {
        return(0);
    }

    if (plfont->widths_cache_nitems > PL_MAX_WIDTHS_CACHE_NITEMS) {
        pl_font_glyph_width_cache_remove_nodes(plfont);
    }

    node = (pl_glyph_width_node_t *) gs_alloc_bytes(plfont->pfont->memory,
                                                 sizeof
                                                 (pl_glyph_width_node_t),
                                                 "pl_glyph_width_cache_node_add");

    if (node == NULL) {
        /* if we couldn't allocate a node, it probably doesn't hurt
         * to get rid of all the nodes we have.
         */
        pl_font_glyph_width_cache_remove_nodes(plfont);
        return -1;
    }

    node->next = plfont->widths_cache;
    plfont->widths_cache = node;
    plfont->widths_cache_nitems++;

    node->char_code = char_code;
    node->font_id = plfont->pfont->id;
    node->width = *pwidth;

    return 0;
}


static int
pl_font_glyph_width_cache_node_search(const pl_font_t *plfont, uint char_code,
                                 gs_point * pwidth)
{
    pl_glyph_width_node_t *current = plfont->widths_cache;

    while (current) {
        if (char_code == current->char_code) {
            *pwidth = current->width;
            return 0;
        }
        current = current->next;
    }
    return -1;
}


/* ---------------- Public procedures ---------------- */
/* character width */

void
pl_font_glyph_width_cache_remove_nodes(pl_font_t *plfont)
{
    pl_glyph_width_node_t *current = plfont->widths_cache;

    while (current) {
        pl_glyph_width_node_t *next = current->next;

        gs_free_object(plfont->pfont->memory, current, "pl_glyph_width_list_remove");
        current = next;
    }
    plfont->widths_cache = NULL;
    plfont->widths_cache_nitems = 0;
    return;
}

int
pl_font_char_width(const pl_font_t * plfont, const void *pgs,
                   gs_char char_code, gs_point * pwidth)
{
    int code = 0;

    if (pl_font_glyph_width_cache_node_search(plfont, char_code, pwidth) >= 0) {
        return(code);
    }

    if ((code = (*(plfont)->char_width) (plfont, pgs, char_code, pwidth)) == 0) {

        /* at least here, ignore the return value - if we fail to add a node
         * to the cache, we can reasonably attempt to carry on without it
         */
        (void)pl_font_glyph_width_cache_node_add((pl_font_t *)plfont, char_code, pwidth);
    }
    return code;
}

/* character width */
int
pl_font_char_metrics(const pl_font_t * plfont, const void *pgs,
                     gs_char char_code, float metrics[4])
{
    return (*(plfont)->char_metrics) (plfont, pgs, char_code, metrics);
}

/* Allocate a font. */
pl_font_t *
pl_alloc_font(gs_memory_t * mem, client_name_t cname)
{
    pl_font_t *plfont = gs_alloc_struct(mem, pl_font_t, &st_pl_font, cname);

    if (plfont) {               /* Initialize pointers. */
        plfont->pfont = 0;
        plfont->header = 0;
        plfont->glyphs.table = 0;
        plfont->char_glyphs.table = 0;
        /* Initialize other defaults. */
        plfont->orient = 0;
        plfont->allow_vertical_substitutes = false;
        plfont->bold_fraction = 0;
        plfont->font_file = 0;
        plfont->resolution.x = plfont->resolution.y = 0;
        plfont->params.proportional_spacing = true;
        memset(plfont->character_complement, 0xff, 8);
        plfont->offsets.GC = plfont->offsets.GT = plfont->offsets.VT = -1;
        plfont->pts_per_inch = 72.0;    /* normal value */
        plfont->widths_cache = NULL;
        plfont->widths_cache_nitems = 0;
        plfont->names = NULL;
        plfont->max_name_index = 0;
        plfont->next_name_index = 0;
    }
    return plfont;
}

/* Structure descriptors for cloning fonts */
gs_private_st_ptrs1(st_pl_font_glyph_f, pl_font_glyph_t, "pl_font_glyph_t",
                    pl_font_glyph_enum_ptrs_f, pl_font_glyph_reloc_ptrs_f,
                    data);
gs_private_st_element(st_pl_font_glyph_element_f, pl_font_glyph_t,
                      "pl_font_glyph_t[]", pl_font_glyph_elt_enum_ptrs_f,
                      pl_font_glyph_elt_reloc_ptrs_f, st_pl_font_glyph_f);

pl_font_t *
pl_clone_font(const pl_font_t * src, gs_memory_t * mem, client_name_t cname)
{
    pl_font_t *plfont = gs_alloc_struct(mem, pl_font_t, &st_pl_font, cname);

    if (plfont == 0)
        return 0;
    /* copy technology common parts */
    plfont->storage = src->storage;
    plfont->header_size = src->header_size;
    plfont->scaling_technology = src->scaling_technology;
    plfont->is_xl_format = src->is_xl_format;
    plfont->allow_vertical_substitutes = src->allow_vertical_substitutes;
    plfont->font_type = src->font_type;
    plfont->char_width = src->char_width;
    plfont->char_metrics = src->char_metrics;
    plfont->large_sizes = src->large_sizes;
    plfont->resolution = src->resolution;
    plfont->params = src->params;
    plfont->pts_per_inch = src->pts_per_inch;
    plfont->font_file_loaded = src->font_file_loaded;
    plfont->orient = src->orient;
    plfont->bold_fraction = src->bold_fraction;
    plfont->widths_cache = NULL;
    plfont->widths_cache_nitems = 0;
    {
        int i;

        for (i = 0; i < sizeof(src->character_complement); i++)
            plfont->character_complement[i] = src->character_complement[i];
    }
    plfont->offsets = src->offsets;
    plfont->header = gs_alloc_bytes(mem, src->header_size, cname);
    if (plfont->header == 0)
        return 0;
    memcpy(plfont->header, src->header, src->header_size);

    plfont->names = NULL;
    plfont->max_name_index = 0;
    plfont->next_name_index = 0;

    if (src->font_file) {
        plfont->font_file =
            (char *)gs_alloc_bytes(mem, strlen(src->font_file) + 1,
                                   "pl_clone_font");
        if (plfont->font_file == 0)
            return 0;           /* #NB errors!!! */
        strcpy(plfont->font_file, src->font_file);
    } else
        plfont->font_file = 0;
    /* technology specific setup */
    switch (plfont->scaling_technology) {
        case plfst_bitmap:
            {
                gs_font_base *pfont =
                    gs_alloc_struct(mem, gs_font_base, &st_gs_font_base,
                                    cname);
                if (pfont == 0)
                    return 0;
                pl_fill_in_font((gs_font *) pfont, plfont, src->pfont->dir,
                                mem, "nameless_font");
                pl_fill_in_bitmap_font(pfont, gs_next_ids(mem, 1));
                break;
            }
        case plfst_Intellifont:
            {
                gs_font_base *pfont =
                    gs_alloc_struct(mem, gs_font_base, &st_gs_font_base,
                                    cname);
                if (pfont == 0)
                    return 0;
                pl_fill_in_font((gs_font *) pfont, plfont, src->pfont->dir,
                                mem, "nameless_font");
                pl_fill_in_intelli_font(pfont, gs_next_ids(mem, 1));
                break;
            }
        case plfst_TrueType:
            {
                {
                    gs_font_type42 *pfont =
                        gs_alloc_struct(mem, gs_font_type42,
                                        &st_gs_font_type42, cname);
                    /* detect if a truetype font is downloaded or
                       internal.  There must be a better way... */
                    gs_font_type42 *pfont_src = (gs_font_type42 *) src->pfont;
                    bool downloaded =
                        (pfont_src->data.get_outline == pl_tt_get_outline);
                    if (pfont == 0)
                        return 0;
                    pl_fill_in_font((gs_font *) pfont, plfont,
                                    src->pfont->dir, mem, "nameless_font");
                    pl_fill_in_tt_font(pfont, downloaded ? NULL : src->header,
                                       gs_next_ids(mem, 1));
                }
                break;
            }
        default:
            return 0;
    }
    if (src->char_glyphs.table != 0) {
        /* HAS may gs_alloc_struct_array() here but this is
           consistant with pl_tt_alloc_char_glyphs() */
        pl_tt_char_glyph_t *char_glyphs =
            (pl_tt_char_glyph_t *) gs_alloc_byte_array(mem,
                                                       src->char_glyphs.size,
                                                       sizeof
                                                       (pl_tt_char_glyph_t),
                                                       cname);
        int i;

        if (char_glyphs == 0)
            return 0;
        for (i = 0; i < src->char_glyphs.size; i++)
            char_glyphs[i] = src->char_glyphs.table[i];
        /* once again a copy struct shortcut and then are restore
           of the char_glyphs.table pointer */
        plfont->char_glyphs = src->char_glyphs;
        plfont->char_glyphs.table = char_glyphs;
    } else                      /* no character glyph table data */
        plfont->char_glyphs = src->char_glyphs;

    if (src->glyphs.table != 0) {
        int i;

        plfont->glyphs.table =
            gs_alloc_struct_array(mem, src->glyphs.size, pl_font_glyph_t,
                                  &st_pl_font_glyph_element_f, cname);
        if (plfont->glyphs.table == NULL)
            return 0;
        plfont->glyphs.used = src->glyphs.used;
        plfont->glyphs.limit = src->glyphs.limit;
        plfont->glyphs.size = src->glyphs.size;
        plfont->glyphs.skip = src->glyphs.skip;
        for (i = 0; i < src->glyphs.size; i++) {
            const byte *data = src->glyphs.table[i].data;
            byte *char_data;

            plfont->glyphs.table[i].glyph = src->glyphs.table[i].glyph;
            plfont->glyphs.table[i].data = 0;
            if (data) {
                uint size = src->glyphs.table[i].data_len;
                char_data = gs_alloc_bytes(mem, size, cname);
                if (char_data == 0)
                    return 0;
                memcpy(char_data, data, size);
                plfont->glyphs.table[i].data = char_data;
                plfont->glyphs.table[i].data_len = size;
            }

        }
    } else                      /* no glyph table */
        plfont->glyphs = src->glyphs;
    return plfont;
}

/* Fill in generic font boilerplate. NB TODO examine duplication with
   gs_font_alloc().  The font name must not contain spaces.  It is
   used for PDF output and Acrobat (some versions) do not process the
   file correctly with spaces in the name. */
int
pl_fill_in_font(gs_font * pfont, pl_font_t * plfont, gs_font_dir * pdir,
                gs_memory_t * mem, const char *font_name)
{
    gs_font_base *pbfont = (gs_font_base *) pfont;

    plfont->pfont = pfont;
    /* Initialize generic font data. */
    gs_make_identity(&pbfont->orig_FontMatrix);
    gs_make_identity(&pbfont->FontMatrix);
    pbfont->next = pbfont->prev = 0;
    pbfont->memory = mem;
    pbfont->dir = pdir;
    pbfont->is_resource = false;
    gs_notify_init(&pbfont->notify_list, gs_memory_stable(mem));
    pbfont->base = (gs_font *) pbfont;
    pbfont->client_data = plfont;
    pbfont->WMode = 0;
    pbfont->PaintType = 0;
    pbfont->StrokeWidth = 0;
    pbfont->is_cached = 0;
    pbfont->procs.init_fstack = gs_default_init_fstack;
    pbfont->procs.next_char_glyph = gs_default_next_char_glyph;
    pbfont->FAPI = NULL;
    pbfont->FAPI_font_data = NULL;

    pbfont->procs.glyph_name = pl_glyph_name;
    pbfont->procs.decode_glyph = pl_decode_glyph;
    /* NB pbfont->procs.callbacks.known_encode = pl_known_encode; */
    pbfont->procs.define_font = gs_no_define_font;
    pbfont->procs.make_font = gs_no_make_font;
    pbfont->procs.font_info = gs_default_font_info;
    pbfont->procs.glyph_info = gs_default_glyph_info;
    pbfont->procs.glyph_outline = gs_no_glyph_outline;
    pbfont->id = gs_next_ids(mem, 1);
    {
        size_t sz = strlen(font_name);
        gs_font_name *fnm = &pbfont->font_name;
        gs_font_name *knm = &pbfont->key_name;

        if (sz > gs_font_name_max)
            sz = gs_font_name_max;
        fnm->size = knm->size = sz;

        memcpy(fnm->chars, font_name, sz);
        fnm->chars[sz] = 0;

        memcpy(knm->chars, font_name, sz);
        knm->chars[sz] = 0;
    }
    return 0;
}

/* Fill in bitmap font boilerplate. */
void
pl_fill_in_bitmap_font(gs_font_base * pfont, long unique_id)
{
    pfont->FontType = ft_PCL_user_defined;
    pfont->BitmapWidths = true;
    pfont->ExactSize = fbit_use_bitmaps;
    pfont->InBetweenSize = fbit_use_bitmaps;
    pfont->TransformedChar = fbit_transform_bitmaps;
    pl_bitmap_init_procs(pfont);
    /* We have no idea what the FontBBox should be. */
    pfont->FontBBox.p.x = pfont->FontBBox.p.y =
        pfont->FontBBox.q.x = pfont->FontBBox.q.y = 0;
    uid_set_UniqueID(&pfont->UID, unique_id);
    pfont->encoding_index = 1;          /****** WRONG ******/
    pfont->nearest_encoding_index = 1;          /****** WRONG ******/
}

/* Fill in TrueType font boilerplate. */
int
pl_fill_in_tt_font(gs_font_type42 * pfont, void *data, long unique_id)
{
    pfont->FontType = ft_TrueType;
    pfont->BitmapWidths = true;
    pfont->ExactSize = fbit_use_outlines;
    pfont->InBetweenSize = fbit_use_outlines;
    pfont->TransformedChar = fbit_use_outlines;
    /* Initialize base font data. */
    /*
     * We can't set the FontBBox correctly until we've initialized the
     * Type 42 specific data, but we need to set it to an empty box now
     * for the sake of gs_type42_font_init.
     */
    pfont->FontBBox.p.x = pfont->FontBBox.p.y =
        pfont->FontBBox.q.x = pfont->FontBBox.q.y = 0;
    uid_set_UniqueID(&pfont->UID, unique_id);
    pfont->encoding_index = 1;          /****** WRONG ******/
    pfont->nearest_encoding_index = 1;          /****** WRONG ******/
    /* Initialize Type 42 specific data. */
    pfont->data.proc_data = data;
    pl_tt_init_procs(pfont);
    {
        int code = gs_type42_font_init(pfont, 0);

        if (code < 0)
            return code;
    }
    /* disable unused FAPI */
    pfont->FAPI = 0;
    pfont->FAPI_font_data = 0;
    pl_tt_finish_init(pfont, !data);
    return 0;
}

/* Fill in Intellifont boilerplate. */
void
pl_fill_in_intelli_font(gs_font_base * pfont, long unique_id)
{                               /* Intellifonts have an 8782-unit design space. */
    {
        gs_matrix mat;

        gs_make_scaling(1.0 / 8782, 1.0 / 8782, &mat);
        gs_matrix_translate(&mat, -2980.0, -5380.0, &pfont->orig_FontMatrix);
    }
    pfont->FontType = ft_MicroType;
    pfont->BitmapWidths = true;
    pfont->ExactSize = fbit_use_outlines;
    pfont->InBetweenSize = fbit_use_outlines;
    pfont->TransformedChar = fbit_use_outlines;
    /* We have no idea what the FontBBox should be. */
    pfont->FontBBox.p.x = pfont->FontBBox.p.y =
        pfont->FontBBox.q.x = pfont->FontBBox.q.y = 0;
    uid_set_UniqueID(&pfont->UID, unique_id);
    pfont->encoding_index = 1;          /****** WRONG ******/
    pfont->nearest_encoding_index = 1;          /****** WRONG ******/
    pl_intelli_init_procs(pfont);
}

/*
 * Set large_sizes, scaling_technology, character_complement, offsets
 * (for TrueType fonts), and resolution (for bitmap fonts) by scanning
 * the segments of a segmented downloaded font.
 * This is used for PCL5 Format 15 and 16 fonts and for PCL XL fonts.
 * fst_offset is the offset of the Font Scaling Technology and Variety bytes;
 * the segment data runs from start_offset up to end_offset.
 * large_sizes = false indicates 2-byte segment sizes, true indicates 4-byte.
 */
int
pl_font_scan_segments(const gs_memory_t * mem,
                      pl_font_t * plfont, int fst_offset, int start_offset,
                      long end_offset, bool large_sizes,
                      const pl_font_offset_errors_t * pfoe)
{
    const byte *header = plfont->header;
    pl_font_scaling_technology_t fst = header[fst_offset];
    int wsize = (large_sizes ? 4 : 2);
    const byte *segment = header + start_offset;
    const byte *end = header + end_offset;
    const byte *null_segment = end - (2 + wsize);
    bool found = false;
    ulong seg_size;
    int illegal_font_data = pfoe->illegal_font_data;

#define return_scan_error(err)\
  return_error((err) ? (err) : illegal_font_data);

    if (memcmp(null_segment, "\377\377", 2) /* NULL segment header */ )
        return_scan_error(pfoe->missing_required_segment);
    if (memcmp(null_segment + 2, "\0\0\0\0", wsize) /* NULL segment size */ )
        return_scan_error(pfoe->illegal_null_segment_size);
    switch (fst) {
        case plfst_bitmap:
        case plfst_TrueType:
            break;
        default:
            return_scan_error(pfoe->illegal_font_header_fields);
    }
    if (header[fst_offset + 1]) /* variety, must be 0 */
        return_scan_error(pfoe->illegal_font_header_fields);
    /* Scan the segments. */
    for (; end - segment >= 2 + wsize; segment += 2 + wsize + seg_size) {
        uint seg_id = u16(segment);
        const byte *sdata = segment + 2 + wsize;

#define id2(c1,c2) (((uint)(c1) << 8) + (c2))

        seg_size = (large_sizes ? u32(segment + 2) : u16(segment + 2));
        if (seg_size + 2 + wsize > end - segment)
            return_error(illegal_font_data);
        /* Handle segments common to all fonts. */
        switch (seg_id) {
            case 0xffff:       /* NULL segment ID */
                if (segment != null_segment)
                    return_error(illegal_font_data);
                continue;
            case id2('V', 'I'):
                continue;
            case id2('C', 'C'):
                if (seg_size != 8)
                    return_error(illegal_font_data);
                memcpy(plfont->character_complement, sdata, 8);
                continue;
            default:
                ;
        }
        /* Handle segments specific to the scaling technology. */
        if (fst == plfst_bitmap)
            switch (seg_id) {
                case id2('B', 'R'):
                    if (seg_size != 4)
                        return_scan_error(pfoe->illegal_BR_segment);
                    {
                        uint xres = pl_get_uint16(sdata);
                        uint yres = pl_get_uint16(sdata + 2);

                        if (xres == 0 || yres == 0)
                            return_scan_error(pfoe->illegal_BR_segment);
                        plfont->resolution.x = xres;
                        plfont->resolution.y = yres;
                    }
                    found = true;
                    break;
                default:
                    if (pfoe->illegal_font_segment < 0)
                        return_error(pfoe->illegal_font_segment);
        } else                  /* fst == plfst_TrueType */
            switch (seg_id) {
                case id2('G', 'T'):
                    /*
                     * We don't do much checking here, but we do check that
                     * the segment starts with a table directory that
                     * includes at least 3 elements (gdir, head,
                     * maxp -- but we don't check the actual names).
                     */
                    if (seg_size < 12 + 5 * 16 ||
                        /* memcmp(sdata, "\000\001\000\000", 4) || */
                        u16(sdata + 4) < 3)
                        return_scan_error(pfoe->illegal_GT_segment);
                    plfont->offsets.GT = segment - header;
                    found = true;
                    break;
                case id2('G', 'C'):
                    if (seg_size < 6 || u16(sdata) != 0 ||
                        seg_size != u16(sdata + 4) * 6 + 6)
                        return_scan_error(pfoe->illegal_GC_segment);
                    plfont->offsets.GC = segment - header;
                    break;
                case id2('V', 'T'):
                    /* Check for end of table mark */
                    if ((seg_size & 3) != 0 || seg_size < 4 ||
                        u16(sdata + seg_size - 4) != 0xffff)
                        return_scan_error(pfoe->illegal_VT_segment);
                    /* Check for table sorted by horizontal glyph ID */
                    {
                        uint i;

                        for (i = 0; i < seg_size - 4; i += 4)
                            if (u16(sdata + i) > u16(sdata + i + 4))
                                return_scan_error(pfoe->illegal_VT_segment);
                    }
                    plfont->offsets.VT = segment - header;
                    break;
                case id2('V', 'E'):    /* nb unimplemented */
                    break;
                case id2('V', 'R'):    /* nb unimplemented */
                    break;
                case id2('C', 'E'):    /* nb unimplemented */
                    break;
                default:
                    if (pfoe->illegal_font_segment < 0)
                        return_error(pfoe->illegal_font_segment);
            }
#undef id2
    }
    if (!found)
        return_scan_error(pfoe->missing_required_segment);
    if (segment != end)
        return_error(illegal_font_data);
    plfont->large_sizes = large_sizes;
    plfont->scaling_technology = fst;
    return 0;
#undef return_scan_error
}

int
pl_free_tt_fontfile_buffer(gs_memory_t * mem, byte * ptt_font_data)
{
    gs_free_object(mem, ptt_font_data, "pl_tt_load_font data");
    return 0;
}

int
pl_alloc_tt_fontfile_buffer(stream * in, gs_memory_t * mem,
                            byte ** pptt_font_data, ulong * size)
{
    ulong len = (sfseek(in, 0L, SEEK_END), sftell(in));

    *size = 6 + len;            /* leave room for segment header */
    if (*size != (uint) (*size)) {
        /*
         * The font is too big to load in a single piece -- punt.
         * The error message is bogus, but there isn't any more
         * appropriate one.
         */
        sfclose(in);
        return_error(gs_error_VMerror);
    }
    srewind(in);
    *pptt_font_data = gs_alloc_bytes(mem, *size, "pl_tt_load_font data");
    if (*pptt_font_data == 0) {
        sfclose(in);
        return_error(gs_error_VMerror);
    }
    sfread(*pptt_font_data + 6, 1, len, in);
    sfclose(in);
    return 0;
}

/* Load a built-in (TrueType) font from external storage. */
int
pl_load_tt_font(stream * in, gs_font_dir * pdir, gs_memory_t * mem,
                long unique_id, pl_font_t ** pplfont, char *font_name)
{
    byte *tt_font_datap = NULL;
    ulong size;
    int code;
    gs_font_type42 *pfont = NULL;
    pl_font_t *plfont = NULL;
    byte *file_name = NULL;
    gs_const_string pfname;

    if (sfilename(in, &pfname) == 0) {
        file_name =
            gs_alloc_bytes(mem, pfname.size + 1, "pl_load_tt_font file_name");
        if (!file_name) {
            sfclose(in);
            return_error(gs_error_VMerror);
        }
        /* the stream code guarantees the string is null terminated */
        memcpy(file_name, pfname.data, pfname.size + 1);
    }

    /* get the data from the file */
    code = pl_alloc_tt_fontfile_buffer(in, mem, &tt_font_datap, &size);
    if (code < 0)
        goto error;
    /* Make a Type 42 font out of the TrueType data. */
    pfont = gs_alloc_struct(mem, gs_font_type42, &st_gs_font_type42,
                            "pl_tt_load_font(gs_font_type42)");
    if (pfont == NULL) {
        code = gs_error_VMerror;
        goto error;
    }
    memset(pfont, 0, sizeof(*pfont));
    plfont = pl_alloc_font(mem, "pl_tt_load_font(pl_font_t)");
    if (plfont == NULL) {
        code = gs_error_VMerror;
        goto error;
    }

    /* Initialize general font boilerplate. */
    code = pl_fill_in_font((gs_font *) pfont, plfont, pdir, mem, font_name);
    if (code < 0)
        goto error;

    /* Initialize TrueType font boilerplate. */
    plfont->header = tt_font_datap;
    plfont->header_size = size;
    plfont->scaling_technology = plfst_TrueType;
    plfont->font_type = plft_Unicode;
    plfont->large_sizes = true;
    plfont->offsets.GT = 0;
    plfont->is_xl_format = false;
    code = pl_fill_in_tt_font(pfont, tt_font_datap, unique_id);
    if (code < 0)
        goto error;
    code = gs_definefont(pdir, (gs_font *) pfont);
    if (code < 0)
        goto error;

    code = pl_fapi_passfont(plfont, 0, NULL, (char *)file_name, NULL, 0);
    if (code < 0)
        goto error;
    if (file_name)
        gs_free_object(mem, file_name, "pl_load_tt_font file_name");

    *pplfont = plfont;
    return 0;

error:
    gs_free_object(mem, plfont, "pl_tt_load_font(pl_font_t)");
    gs_free_object(mem, pfont, "pl_tt_load_font(gs_font_type42)");
    pl_free_tt_fontfile_buffer(mem, tt_font_datap);
    gs_free_object(mem, file_name, "pl_load_tt_font file_name");
    return_error(code);
}

/* load resident font data to ram */
int
pl_load_resident_font_data_from_file(gs_memory_t * mem, pl_font_t * plfont)
{

    ulong len, size;
    byte *data;

    if (plfont->font_file && !plfont->font_file_loaded) {
        stream *in = sfopen(plfont->font_file, "r", mem);

        if (in == NULL)
            return -1;
        /* note this is exactly the same as the code in pl_load_tt_font */
        len = (sfseek(in, 0L, SEEK_END), sftell(in));
        size = 6 + len;         /* leave room for segment header */

        if (size != (uint) size) {
            /*
             * The font is too big to load in a single piece -- punt.
             * The error message is bogus, but there isn't any more
             * appropriate one.
             */
            sfclose(in);
            return_error(gs_error_VMerror);
        }
        srewind(in);
        data = gs_alloc_bytes(mem, size, "pl_tt_load_font data");
        if (data == 0) {
            sfclose(in);
            return_error(gs_error_VMerror);
        }
        sfread(data + 6, 1, len, in);
        sfclose(in);
        plfont->header = data;
        plfont->header_size = size;
        plfont->font_file_loaded = true;
    }
    return 0;
}

/* Keep resident font data in (header) and deallocate the memory */
int
pl_store_resident_font_data_in_file(char *font_file, gs_memory_t * mem,
                                    pl_font_t * plfont)
{
    /* Free the header data */
    if (plfont->header) {
        gs_free_object(mem, plfont->header,
                       "pl_store_resident_font_data_in_file");
        plfont->header = 0;
        plfont->header_size = 0;
    } else {
        /* nothing to do */
        return 0;
    }
    /* we don't yet have a filename for this font object. create one
       and store it in the font. */
    if (!plfont->font_file) {
        plfont->font_file =
            (char *)gs_alloc_bytes(mem, strlen(font_file) + 1,
                                   "pl_store_resident_font_data_in_file");
        if (plfont->font_file == 0)
            return -1;
        strcpy(plfont->font_file, font_file);
    }
    /* designate that the font data is not in RAM */
    plfont->font_file_loaded = false;
    return 0;
}

pl_font_t *
pl_lookup_font_by_pjl_number(pl_dict_t * pfontdict, int pjl_font_number)
{
    pl_dict_enum_t dictp;
    gs_const_string key;
    void *value;

    pl_dict_enum_begin(pfontdict, &dictp);
    while (pl_dict_enum_next(&dictp, &key, &value)) {
        pl_font_t *plfont = value;

        if (plfont->params.pjl_font_number == pjl_font_number)
            return value;
    }
    return (pl_font_t *) NULL;
}
