/* Copyright (C) 2001-2024 Artifex Software, Inc.
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


/* Type 42 (TrueType) font library routines */
#include <stdlib.h>		/* for qsort */

#include "memory_.h"
#include "stdint_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsstruct.h"
#include "gsccode.h"
#include "gsline.h"		/* for gs_gstate_setflat */
#include "gsmatrix.h"
#include "gsutil.h"
#include "gxchrout.h"
#include "gxfixed.h"		/* for gxpath.h */
#include "gxpath.h"
#include "gxfont.h"
#include "gxfont42.h"
#include "gxttf.h"
#include "gxttfb.h"
#include "gxtext.h"
#include "gxchar.h"
#include "gxfcache.h"
#include "gxgstate.h"
#include "gzstate.h"
#include "stream.h"

/* For WOFF Support */
#include "strimpl.h"
#include "stream.h"
#include "strmio.h"
#include "szlibx.h"

/* Structure descriptor */
public_st_gs_font_type42();

/* Forward references */
static int append_outline_fitted(uint glyph_index, const gs_matrix * pmat,
               gx_path * ppath, cached_fm_pair * pair,
               const gs_log2_scale_point * pscale, bool design_grid);
static uint default_get_glyph_index(gs_font_type42 *pfont, gs_glyph glyph);
static int default_get_outline(gs_font_type42 *pfont, uint glyph_index,
                                gs_glyph_data_t *pgd);
font_proc_font_info(gs_type42_font_info); /* Type check. */
font_proc_font_info(gs_truetype_font_info); /* Type check. */

/* Get 2- or 4-byte quantities from a table. */
#define U8(p) ((uint)((p)[0]))
#define S8(p) (int)((U8(p) ^ 0x80) - 0x80)
#define U16(p) (((uint)((p)[0]) << 8) + (p)[1])
#define S16(p) (int)((U16(p) ^ 0x8000) - 0x8000)
#define u32(p) get_u32_msb(p)

#define PUTU16(p, n, offs) {(p + offs)[0] = n >> 8  & 255; (p + offs)[1] = n & 255;}

static byte const ver10[4] = {0x00, 0x01, 0x00, 0x00};
static byte const ver20[4] = {0x00, 0x02, 0x00, 0x00};

/* ---------------- Font level ---------------- */

/* Read data from sfnts. */
int
gs_type42_read_data(gs_font_type42 * pfont, ulong pos, uint length, byte *buf)
{
    int (*string_proc)(gs_font_type42 *, ulong, uint, const byte **) =
        pfont->data.string_proc;
    uint left = length;
    const byte *data;
    int code;

    do {
        code = (*string_proc)(pfont, (ulong)(pos + length - left), left, &data);
        if (code < 0)
            return code;
        if (code == 0)
            code = left;
        memcpy(buf + length - left, data, code);
        left -= code;
    } while (left);
    return 0;
}

/* Get the offset to a glyph using the loca table */
static inline ulong
get_glyph_offset(gs_font_type42 *pfont, uint glyph_index)
{
    ulong result;
    byte buf[4];
    int code;

    /* Hack : when reading fails, we return a huge offset.
       Then gs_type42_font_init will fails because it is greater than loca size.
       Fixme : improve with changing the function prototype.
     */
    if (pfont->data.indexToLocFormat) {
        code = gs_type42_read_data(pfont, pfont->data.loca + glyph_index * 4, 4, buf);
        result = (code < 0 ? 0xffffffff : u32(buf));
    } else {
        code = gs_type42_read_data(pfont, pfont->data.loca + glyph_index * 2, 2, buf);
        result = (code < 0 ? 0xffffffff : (ulong) U16(buf) << 1);
    }
    return result;
}

/* compare fn used by gs_type42_font_init() for sorting the 'loca' table */
typedef struct gs_type42_font_init_sort_s {
    ulong glyph_offset;
    int glyph_num;
    int glyph_length;
} gs_type42_font_init_sort_t;
static int
gs_type42_font_init_compare (const void *a, const void *b)
{
    ulong a_offset = ((const gs_type42_font_init_sort_t *)a)->glyph_offset;
    ulong b_offset = ((const gs_type42_font_init_sort_t *)b)->glyph_offset;
    if (a_offset < b_offset)
        return -1;
    else if (a_offset > b_offset)
        return +1;
    else
        /* make the sort stable */
        return ((const gs_type42_font_init_sort_t *)a)->glyph_num -
               ((const gs_type42_font_init_sort_t *)b)->glyph_num;
}

/*
 * Initialize the cached values in a Type 42 font.
 * Note that this initializes the type42_data procedures other than
 * string_proc, and the font procedures as well.
 */

int
gs_type42_font_init(gs_font_type42 * pfont, int subfontID)
{
    int (*string_proc)(gs_font_type42 *, ulong, uint, const byte **) =
        pfont->data.string_proc;
    byte OffsetTable[12];
    uint numTables;
    byte TableDirectory[MAX_NUM_TT_TABLES * 16];
    uint i;
    int code;
    byte head_box[8] = {0};
    ulong loca_size = 0;
    ulong glyph_start, glyph_offset, glyph_length, glyph_size = 0;
    uint numFonts, version;
    uint OffsetTableOffset;

    static const byte version1_0[4] = {0, 1, 0, 0};
    static const byte version_true[4] = {'t', 'r', 'u', 'e'};
    static const byte version_ttcf[4] = {'t', 't', 'c', 'f'};

    READ_SFNTS(pfont, 0, 12, OffsetTable);
    if (!memcmp(OffsetTable, version_ttcf, 4)) {
        version = u32(OffsetTable + 4);
        if (version != 0x00010000 && version !=0x00020000) {
            emprintf2(pfont->memory,
                      "Unknown TTC header version %08X in the font %s.\n",
                      version, pfont->key_name.chars);
            return_error(gs_error_invalidfont);
        }
        numFonts = u32(OffsetTable + 8);
        if (subfontID < 0 || subfontID >= numFonts)
            return_error(gs_error_rangecheck);
        READ_SFNTS(pfont, 12 + subfontID * 4, 4, OffsetTable);
        OffsetTableOffset = u32(OffsetTable);
        READ_SFNTS(pfont, OffsetTableOffset, 12, OffsetTable);
    } else {
        OffsetTableOffset = 0;
    }

    if (memcmp(OffsetTable, version1_0, 4) &&
            memcmp(OffsetTable, version_true, 4))
        return_error(gs_error_invalidfont);

    numTables = U16(OffsetTable + 4);
    if (numTables > MAX_NUM_TT_TABLES)
        return_error(gs_error_invalidfont);
    READ_SFNTS(pfont, OffsetTableOffset + 12, numTables * 16, TableDirectory);
    /* Clear all non-client-supplied data. */
    {
        void *proc_data = pfont->data.proc_data;

        memset(&pfont->data, 0, sizeof(pfont->data));
        pfont->data.string_proc = string_proc;
        pfont->data.proc_data = proc_data;
        pfont->data.subfontID = (ushort)(subfontID >=0 ? subfontID : 0);
        pfont->data.subfontOffset = (gs_offset_t)OffsetTableOffset;
    }
    for (i = 0; i < numTables; ++i) {
        const byte *tab = TableDirectory + i * 16;
        ulong offset = u32(tab + 8);

        if (!memcmp(tab, "cmap", 4))
            pfont->data.cmap = offset;
        else if (!memcmp(tab, "post", 4)) {
            byte ver[4];
            READ_SFNTS(pfont, offset, 4, ver);
            if (memcmp(ver, ver10, 4) == 0 ||  memcmp(ver, ver20, 4) == 0) {
                pfont->data.post_offset = offset;
            }
            else {
                pfont->data.post_offset = 0;
            }
        }
        else if (!memcmp(tab, "glyf", 4)) {
            pfont->data.glyf = offset;
            glyph_size = (uint)u32(tab + 12);
        } else if (!memcmp(tab, "GSUB", 4)) {
            if (pfont->data.gsub_size != 0)
                return_error(gs_error_invalidfont);
            pfont->data.gsub_size = u32(tab + 12);
            pfont->data.gsub = gs_alloc_byte_array(pfont->memory, pfont->data.gsub_size, 1,
                                                        "gs_type42_font_init(GSUB)");
            if (pfont->data.gsub == 0)
                return_error(gs_error_VMerror);

            code = gs_font_notify_register((gs_font *)pfont, gs_gsub_release, (void *)pfont);
            if (code < 0)
                return code;

            code = gs_type42_read_data(pfont, offset, pfont->data.gsub_size, pfont->data.gsub);
            if ( code < 0 )
                return code;
        } else if (!memcmp(tab, "head", 4)) {
            byte head[54];

            READ_SFNTS(pfont, offset, 54, head);
            pfont->data.unitsPerEm = U16(head + 18);
            memcpy(head_box, head + 36, 8);
            pfont->data.indexToLocFormat = U16(head + 50);
        } else if (!memcmp(tab, "hhea", 4)) {
            byte hhea[36];

            READ_SFNTS(pfont, offset, 36, hhea);
            pfont->data.metrics[0].numMetrics = U16(hhea + 34);
        } else if (!memcmp(tab, "hmtx", 4)) {
            pfont->data.metrics[0].offset = offset;
            pfont->data.metrics[0].length = (uint)u32(tab + 12);
        } else if (!memcmp(tab, "loca", 4)) {
            pfont->data.loca = offset;
            loca_size = u32(tab + 12);
        } else if (!memcmp(tab, "maxp", 4)) {
            byte maxp[30];

            READ_SFNTS(pfont, offset, 30, maxp);
            pfont->data.trueNumGlyphs = U16(maxp + 4);
            pfont->data.maxPoints = U16(maxp + 6);
            pfont->data.maxContours = U16(maxp + 8);
            pfont->data.maxCPoints = U16(maxp + 10);
            pfont->data.maxCContours = U16(maxp + 12);
        } else if (!memcmp(tab, "name", 4)) {
            pfont->data.name_offset = offset;
        } else if (!memcmp(tab, "vhea", 4)) {
            byte vhea[36];

            READ_SFNTS(pfont, offset, 36, vhea);
            pfont->data.metrics[1].numMetrics = U16(vhea + 34);
        } else if (!memcmp(tab, "vmtx", 4)) {
            pfont->data.metrics[1].offset = offset;
            pfont->data.metrics[1].length = (uint)u32(tab + 12);
        } else if (!memcmp(tab, "OS/2", 4)) {
            pfont->data.os2_offset = offset;
        }
    }
    loca_size >>= (pfont->data.indexToLocFormat == 0 ? 1 : 2);
    pfont->data.numGlyphs = loca_size - 1;
    if (pfont->data.numGlyphs > 65535) {
        pfont->data.numGlyphs = 65535;
        loca_size = (65536 << (pfont->data.indexToLocFormat == 0 ? 1 : 2));
    }

    if (pfont->data.numGlyphs > (int)pfont->data.trueNumGlyphs) {
        /* pfont->key_name.chars is ASCIIZ due to copy_font_name. */
        char buf[gs_font_name_max + 2];

        if (pfont->key_name.size) {
            int l = min(sizeof(buf) - 1, pfont->key_name.size);

            memcpy(buf, pfont->key_name.chars, l);
            buf[l] = 0;
        } else if (pfont->font_name.size) {
            int l = min(sizeof(buf) - 1, pfont->font_name.size);

            memcpy(buf, pfont->font_name.chars, l);
            buf[l] = 0;
        } else
            buf[0] = 0;

        emprintf3(pfont->memory,
                  "Warning: 'loca' length %d is greater than numGlyphs %d in the font %s.\n",
                  pfont->data.numGlyphs + 1, pfont->data.trueNumGlyphs, buf);
        if (loca_size > pfont->data.trueNumGlyphs + 1) {
            /* Bug 689516 demonstrates a font, in which numGlyps is smaller than loca size,
               and there are useful glyphs behind maxp.numGlyphs. */
            for (i = loca_size - 1;  i > pfont->data.trueNumGlyphs; i--) {
                glyph_offset = get_glyph_offset(pfont, i);
                if (glyph_offset < glyph_size)
                    break;
            }
            if (i > pfont->data.numGlyphs) {
                /* loca contains more good offsets .
                   Note a code below will fix bad offsets if any. */
                 pfont->data.numGlyphs = loca_size - 1;
            }
        }
        loca_size = pfont->data.numGlyphs + 1;
    }

    pfont->data.warning_patented = false;
    pfont->data.warning_bad_instruction = false;
    pfont->procs.glyph_outline = gs_type42_glyph_outline;
    pfont->procs.glyph_info = gs_type42_glyph_info;
    pfont->procs.enumerate_glyph = gs_type42_enumerate_glyph;
    pfont->procs.font_info = gs_type42_font_info;

    /* default data.get routines require a ram version of a ttf file */
    pfont->data.get_glyph_index = default_get_glyph_index;
    pfont->data.get_outline = default_get_outline;
    pfont->data.get_metrics = gs_type42_default_get_metrics;

    if (pfont->FontType == ft_CID_TrueType && pfont->is_resource) {
        /* This font was load with .load_tt_font_stripped,
           (it's only the case when bf_has_font_file is set because font file
           presents with True Type - see zbuildfont11).
           Can't use get_glyph_offset because 'loca' does not present in sfnt.
           So we skip the unsorted 'loca' check.

           A better way would be to perform the unsorted loca check
           after glyph cache is initialized in zbuildfont11 with gs_glyph_cache__alloc.
           But we're doing this fix under a rush of 8.63 release,
           so have no time for deeper changes.
        */
        pfont->data.len_glyphs = NULL;
    } else {
        if (loca_size == 0) {
            pfont->data.len_glyphs = NULL;
        }
        else {
            /* Now build the len_glyphs array since 'loca' may not be properly sorted */
            pfont->data.len_glyphs = (uint *)gs_alloc_byte_array(pfont->memory, loca_size, sizeof(uint),
                                                            "gs_type42_font_init");
            if (pfont->data.len_glyphs == 0)
                return_error(gs_error_VMerror);
            code = gs_font_notify_register((gs_font *)pfont, gs_len_glyphs_release, (void *)pfont);
            if (code < 0) {
                gs_free_object(pfont->memory, pfont->data.len_glyphs, "gs_len_glyphs_release");
                pfont->data.len_glyphs = NULL;
                return code;
            }
            memset(pfont->data.len_glyphs, 0x00, loca_size * sizeof(uint));
            /* The 'loca' may not be in order, so we construct a glyph length array */
            /* Since 'loca' is usually sorted, first try the simple linear scan to  */
            /* avoid the need to perform the more expensive process. */
            glyph_start = get_glyph_offset(pfont, 0);
            if (loca_size > 1) {
                for (i = 1; i < loca_size; i++) {
                    glyph_offset = get_glyph_offset(pfont, i);
                    glyph_length = glyph_offset - glyph_start;
                    if (glyph_length > 0x80000000)
                        break;
                    if (glyph_offset > glyph_size)
                        break;
                    /* out of order loca */
                    pfont->data.len_glyphs[i - 1] = glyph_length;
                    glyph_start = glyph_offset;
                }
            }
            else {
                /* This is purely so broken fonts don't cause an uninitialized
                   memory access later on.
                 */
                pfont->data.len_glyphs[0] = 0;
            }
            if (i < loca_size) {
                /*
                 * loca was out of order, build the len_glyphs the hard way.
                 * For each glyph, we use the next higher or equal
                 * glyph offset to compute the glyph length.
                 * It assumes no overlapping and no duplicate glyphs.
                 */
                ulong last_glyph_offset = glyph_size;
                ulong num_valid_loca_elm = loca_size;
                long last_offset = 0;
                gs_type42_font_init_sort_t *psort;
                gs_type42_font_init_sort_t *psortary =
                    (gs_type42_font_init_sort_t *)gs_alloc_byte_array(pfont->memory,
                        loca_size, sizeof(gs_type42_font_init_sort_t), "gs_type42_font_init(sort loca)");

                if (psortary == 0)
                    return_error(gs_error_VMerror);
                /* loca_size > 0 due to condition above, so we always have the 0th element. */
                psortary->glyph_num = 0;
                psortary->glyph_offset = get_glyph_offset(pfont, 0);
                for (i = 1, psort = psortary + 1; i < loca_size; i++, psort++) {
                    psort->glyph_num = i;
                    psort->glyph_offset = get_glyph_offset(pfont, i);
                    psort[-1].glyph_length = psort->glyph_offset - last_offset;
                    last_offset = psort->glyph_offset;
                }
                psort[-1].glyph_length = 0; /* Dummy element. */
                qsort(psortary, loca_size, sizeof(gs_type42_font_init_sort_t), gs_type42_font_init_compare);
                while (num_valid_loca_elm > 0 && psortary[num_valid_loca_elm - 1].glyph_offset > glyph_size)
                    num_valid_loca_elm --;
                if (0 == num_valid_loca_elm) {
                    gs_free_object(pfont->memory, psortary, "gs_type42_font_init(sort loca)");
                    return_error(gs_error_invalidfont);
                }
                for (i = num_valid_loca_elm; i--;) {
                    long old_length;

                    psort = psortary + i;
                    old_length = psort->glyph_length;
                    if (old_length < 0 || old_length > 2000 /*  arbitrary */) {
                        pfont->data.len_glyphs[psort->glyph_num] = last_glyph_offset - psort->glyph_offset;
                        /* Note the new length may be so big as old_length. */
                    } else
                        pfont->data.len_glyphs[psort->glyph_num] = old_length;
                    last_glyph_offset = psort->glyph_offset;
                }
                for (i = num_valid_loca_elm; i < loca_size; i++) {
                    psort = psortary + i;
                    pfont->data.len_glyphs[psort->glyph_num] = 0;
                }
                /* Well the last element of len_glyphs is never used.
                   We compute it because we're interesting whether it is not zero sometimes.
                   To know that, set a conditional breakpoint at the next statement.
                 */
                gs_free_object(pfont->memory, psortary, "gs_type42_font_init(sort loca)");
            }
        }
    }
    /*
     * If the font doesn't have a valid FontBBox, compute one from the
     * 'head' information.  Since the Adobe PostScript driver sometimes
     * outputs garbage FontBBox values, we use a "reasonableness" check
     * here.
     */
    if (pfont->FontBBox.p.x >= pfont->FontBBox.q.x ||
        pfont->FontBBox.p.y >= pfont->FontBBox.q.y ||
        pfont->FontBBox.p.x < -0.5 || pfont->FontBBox.p.x > 0.5 ||
        pfont->FontBBox.p.y < -0.5 || pfont->FontBBox.p.y > 0.5
        ) {
        float upem = (float)pfont->data.unitsPerEm;

        pfont->FontBBox.p.x = S16(head_box) / upem;
        pfont->FontBBox.p.y = S16(head_box + 2) / upem;
        pfont->FontBBox.q.x = S16(head_box + 4) / upem;
        pfont->FontBBox.q.y = S16(head_box + 6) / upem;
    }
    pfont->data.warning_patented = false;
    pfont->data.warning_bad_instruction = false;
    pfont->data.get_glyph_index = default_get_glyph_index;
    pfont->data.get_outline = default_get_outline;
    pfont->data.get_metrics = gs_type42_default_get_metrics;
    pfont->procs.glyph_outline = gs_type42_glyph_outline;
    pfont->procs.glyph_info = gs_type42_glyph_info;
    pfont->procs.enumerate_glyph = gs_type42_enumerate_glyph;
    pfont->procs.font_info = gs_type42_font_info;
    return 0;
}

int
gs_len_glyphs_release(void *data, void *event)
{
    gs_font_type42 *pfont = (gs_font_type42 *)data;

    gs_font_notify_unregister((gs_font *)pfont, gs_len_glyphs_release, (void *)data);
    gs_free_object(pfont->memory, pfont->data.len_glyphs, "gs_len_glyphs_release");
    return 0;
}

int
gs_gsub_release(void *data, void *event)
{
    gs_font_type42 *pfont = (gs_font_type42 *)data;

    gs_font_notify_unregister((gs_font *)pfont, gs_gsub_release, (void *)data);
    gs_free_object(pfont->memory, pfont->data.gsub, "gs_gsub_release");
    return 0;
}

/* ---------------- Glyph level ---------------- */

mac_glyph_ordering_t MacintoshOrdering[] =
{
    {0, ".notdef"},
    {1, ".null"},
    {2, "nonmarkingreturn"},
    {3, "space"},
    {4, "exclam"},
    {5, "quotedbl"},
    {6, "numbersign"},
    {7, "dollar"},
    {8, "percent"},
    {9, "ampersand"},
    {10, "quotesingle"},
    {11, "parenleft"},
    {12, "parenright"},
    {13, "asterisk"},
    {14, "plus"},
    {15, "comma"},
    {16, "hyphen"},
    {17, "period"},
    {18, "slash"},
    {19, "zero"},
    {20, "one"},
    {21, "two"},
    {22, "three"},
    {23, "four"},
    {24, "five"},
    {25, "six"},
    {26, "seven"},
    {27, "eight"},
    {28, "nine"},
    {29, "colon"},
    {30, "semicolon"},
    {31, "less"},
    {32, "equal"},
    {33, "greater"},
    {34, "question"},
    {35, "at"},
    {36, "A"},
    {37, "B"},
    {38, "C"},
    {39, "D"},
    {40, "E"},
    {41, "F"},
    {42, "G"},
    {43, "H"},
    {44, "I"},
    {45, "J"},
    {46, "K"},
    {47, "L"},
    {48, "M"},
    {49, "N"},
    {50, "O"},
    {51, "P"},
    {52, "Q"},
    {53, "R"},
    {54, "S"},
    {55, "T"},
    {56, "U"},
    {57, "V"},
    {58, "W"},
    {59, "X"},
    {60, "Y"},
    {61, "Z"},
    {62, "bracketleft"},
    {63, "backslash"},
    {64, "bracketright"},
    {65, "asciicircum"},
    {66, "underscore"},
    {67, "grave"},
    {68, "a"},
    {69, "b"},
    {70, "c"},
    {71, "d"},
    {72, "e"},
    {73, "f"},
    {74, "g"},
    {75, "h"},
    {76, "i"},
    {77, "j"},
    {78, "k"},
    {79, "l"},
    {80, "m"},
    {81, "n"},
    {82, "o"},
    {83, "p"},
    {84, "q"},
    {85, "r"},
    {86, "s"},
    {87, "t"},
    {88, "u"},
    {89, "v"},
    {90, "w"},
    {91, "x"},
    {92, "y"},
    {93, "z"},
    {94, "braceleft"},
    {95, "bar"},
    {96, "braceright"},
    {97, "asciitilde"},
    {98, "Adieresis"},
    {99, "Aring"},
    {100, "Ccedilla"},
    {101, "Eacute"},
    {102, "Ntilde"},
    {103, "Odieresis"},
    {104, "Udieresis"},
    {105, "aacute"},
    {106, "agrave"},
    {107, "acircumflex"},
    {108, "adieresis"},
    {109, "atilde"},
    {110, "aring"},
    {111, "ccedilla"},
    {112, "eacute"},
    {113, "egrave"},
    {114, "ecircumflex"},
    {115, "edieresis"},
    {116, "iacute"},
    {117, "igrave"},
    {118, "icircumflex"},
    {119, "idieresis"},
    {120, "ntilde"},
    {121, "oacute"},
    {122, "ograve"},
    {123, "ocircumflex"},
    {124, "odieresis"},
    {125, "otilde"},
    {126, "uacute"},
    {127, "ugrave"},
    {128, "ucircumflex"},
    {129, "udieresis"},
    {130, "dagger"},
    {131, "degree"},
    {132, "cent"},
    {133, "sterling"},
    {134, "section"},
    {135, "bullet"},
    {136, "paragraph"},
    {137, "germandbls"},
    {138, "registered"},
    {139, "copyright"},
    {140, "trademark"},
    {141, "acute"},
    {142, "dieresis"},
    {143, "notequal"},
    {144, "AE"},
    {145, "Oslash"},
    {146, "infinity"},
    {147, "plusminus"},
    {148, "lessequal"},
    {149, "greaterequal"},
    {150, "yen"},
    {151, "mu"},
    {152, "partialdiff"},
    {153, "summation"},
    {154, "product"},
    {155, "pi"},
    {156, "integral"},
    {157, "ordfeminine"},
    {158, "ordmasculine"},
    {159, "Omega"},
    {160, "ae"},
    {161, "oslash"},
    {162, "questiondown"},
    {163, "exclamdown"},
    {164, "logicalnot"},
    {165, "radical"},
    {166, "florin"},
    {167, "approxequal"},
    {168, "Delta"},
    {169, "guillemotleft"},
    {170, "guillemotright"},
    {171, "ellipsis"},
    {172, "nonbreakingspace"},
    {173, "Agrave"},
    {174, "Atilde"},
    {175, "Otilde"},
    {176, "OE"},
    {177, "oe"},
    {178, "endash"},
    {179, "emdash"},
    {180, "quotedblleft"},
    {181, "quotedblright"},
    {182, "quoteleft"},
    {183, "quoteright"},
    {184, "divide"},
    {185, "lozenge"},
    {186, "ydieresis"},
    {187, "Ydieresis"},
    {188, "fraction"},
    {189, "currency"},
    {190, "guilsinglleft"},
    {191, "guilsinglright"},
    {192, "fi"},
    {193, "fl"},
    {194, "daggerdbl"},
    {195, "periodcentered"},
    {196, "quotesinglbase"},
    {197, "quotedblbase"},
    {198, "perthousand"},
    {199, "Acircumflex"},
    {200, "Ecircumflex"},
    {201, "Aacute"},
    {202, "Edieresis"},
    {203, "Egrave"},
    {204, "Iacute"},
    {205, "Icircumflex"},
    {206, "Idieresis"},
    {207, "Igrave"},
    {208, "Oacute"},
    {209, "Ocircumflex"},
    {210, "apple"},
    {211, "Ograve"},
    {212, "Uacute"},
    {213, "Ucircumflex"},
    {214, "Ugrave"},
    {215, "dotlessi"},
    {216, "circumflex"},
    {217, "tilde"},
    {218, "macron"},
    {219, "breve"},
    {220, "dotaccent"},
    {221, "ring"},
    {222, "cedilla"},
    {223, "hungarumlaut"},
    {224, "ogonek"},
    {225, "caron"},
    {226, "Lslash"},
    {227, "lslash"},
    {228, "Scaron"},
    {229, "scaron"},
    {230, "Zcaron"},
    {231, "zcaron"},
    {232, "brokenbar"},
    {233, "Eth"},
    {234, "eth"},
    {235, "Yacute"},
    {236, "yacute"},
    {237, "Thorn"},
    {238, "thorn"},
    {239, "minus"},
    {240, "multiply"},
    {241, "onesuperior"},
    {242, "twosuperior"},
    {243, "threesuperior"},
    {244, "onehalf"},
    {245, "onequarter"},
    {246, "threequarters"},
    {247, "franc"},
    {248, "Gbreve"},
    {249, "gbreve"},
    {250, "Idotaccent"},
    {251, "Scedilla"},
    {252, "scedilla"},
    {253, "Cacute"},
    {254, "cacute"},
    {255, "Ccaron"},
    {256, "ccaron"},
    {257, "dcroat"},
    {-1, NULL}
};

/* Use the post table to find a glyph name */
int
gs_type42_find_post_name(gs_font_type42 * pfont, gs_glyph glyph, gs_string *gname)
{
    /* READ_SFNTS() uses "code" in the macro, so we need code2 to keep track here */
    int code, code2 = gs_error_undefined;

    if (pfont->FontType == ft_TrueType) {
        if (pfont->data.post_offset != 0) {
            byte ver[4];

            READ_SFNTS(pfont, pfont->data.post_offset, 4, ver);
            if (!memcmp(ver, ver10, 4)){
                if (glyph > 257) glyph = 0;
                gname->data = (byte *)MacintoshOrdering[glyph].name;
                gname->size = strlen(MacintoshOrdering[glyph].name);
                code2 = 0;
            }
            else if (!memcmp(ver, ver20, 4)) {
                byte val[2];
                unsigned short gind;
                READ_SFNTS(pfont, pfont->data.post_offset + 34 + glyph * 2, 2, val);
                gind = U16(val);
                if (gind < 258) {
                    gname->data = (byte *)MacintoshOrdering[gind].name;
                    gname->size = strlen(MacintoshOrdering[gind].name);
                    code2 = 0;
                }
                else {
                    int i;
                    byte val[2];
                    short numglyphs;
                    uint offs;

                    READ_SFNTS(pfont, pfont->data.post_offset + 32, 2, val);
                    numglyphs = U16(val);
                    gind -= 258;
                    if (gind < numglyphs) {
                        offs = pfont->data.post_offset + 34 + numglyphs * 2;
                       for (i = 0; i < numglyphs; i++) {
                           if (i == gind) {
                               READ_SFNTS(pfont, offs, 1, val);
                               code2 = pfont->data.string_proc(pfont, offs + 1, (uint)val[0], (const byte **)&(gname->data));
                               if (code2 >= 0)
                                   gname->size = val[0];
                               break;
                           }
                           else {
                               READ_SFNTS(pfont, offs, 1, val);
                               offs += (uint)val[0] + 1;
                           }
                       }
                    }
                }
            }
            else {
                code2 = gs_error_invalidfont;
            }
        }
    }
    else
        code2 = gs_error_invalidfont;

    if (code2 < 0) code = gs_note_error(code2);
    return code;
}

/*
 * Parse the definition of one component of a composite glyph.  We don't
 * bother to parse the component index, since the caller can do this so
 * easily.
 */
void
gs_type42_parse_component(const byte **pdata, uint *pflags, gs_matrix_fixed *psmat,
                          int *pmp /*[2], may be null*/, const gs_font_type42 *pfont,
                          const gs_matrix_fixed *pmat)
{
    const byte *gdata = *pdata;
    uint flags;
    double factor = 1.0 / pfont->data.unitsPerEm;
    gs_matrix_fixed mat;
    gs_matrix scale_mat;

    flags = U16(gdata);
    gdata += 4;
    mat = *pmat;
    if (flags & TT_CG_ARGS_ARE_XY_VALUES) {
        int arg1, arg2;
        gs_fixed_point pt;

        if (flags & TT_CG_ARGS_ARE_WORDS)
            arg1 = S16(gdata), arg2 = S16(gdata + 2), gdata += 4;
        else
            arg1 = S8(gdata), arg2 = S8(gdata + 1), gdata += 2;
        if (flags & TT_CG_ROUND_XY_TO_GRID) {
            /* We should do something here, but we don't. */
        }
        gs_point_transform2fixed(pmat, arg1 * factor,
                                 arg2 * factor, &pt);
        /****** HACK: WE KNOW ABOUT FIXED MATRICES ******/
        mat.tx = fixed2float(mat.tx_fixed = pt.x);
        mat.ty = fixed2float(mat.ty_fixed = pt.y);
        if (pmp)
            pmp[0] = pmp[1] = -1;
    } else {
        if (flags & TT_CG_ARGS_ARE_WORDS) {
            if (pmp)
                pmp[0] = U16(gdata), pmp[1] = S16(gdata + 2);
            gdata += 4;
        } else {
            if (pmp)
                pmp[0] = U8(gdata), pmp[1] = U8(gdata + 1);
            gdata += 2;
        }
    }
#define S2_14(p) (S16(p) / 16384.0)
    if (flags & TT_CG_HAVE_SCALE) {
        scale_mat.xx = scale_mat.yy = S2_14(gdata);
        scale_mat.xy = scale_mat.yx = 0;
        gdata += 2;
    } else if (flags & TT_CG_HAVE_XY_SCALE) {
        scale_mat.xx = S2_14(gdata);
        scale_mat.yy = S2_14(gdata + 2);
        scale_mat.xy = scale_mat.yx = 0;
        gdata += 4;
    } else if (flags & TT_CG_HAVE_2X2) {
        scale_mat.xx = S2_14(gdata);
        scale_mat.xy = S2_14(gdata + 2);
        scale_mat.yx = S2_14(gdata + 4);
        scale_mat.yy = S2_14(gdata + 6);
        gdata += 8;
    } else
        goto no_scale;
#undef S2_14
    scale_mat.tx = 0;
    scale_mat.ty = 0;
    /* The scale doesn't affect mat.t{x,y}, so we don't */
    /* need to update the fixed components. */
    gs_matrix_multiply(&scale_mat, (const gs_matrix *)&mat,
                       (gs_matrix *)&mat);
no_scale:
    *pdata = gdata;
    *pflags = flags;
    *psmat = mat;
}

#if 0 /* not called */
/* Compute the total number of points in a (possibly composite) glyph. */
static int
total_points(gs_font_type42 *pfont, uint glyph_index)
{
    gs_glyph_data_t glyph_data;
    int code;
    int ocode;
    const byte *gdata;
    int total;

    glyph_data.memory = pfont->memory;
    ocode = pfont->data.get_outline(pfont, glyph_index, &glyph_data);
    if (ocode < 0)
        return ocode;
    if (glyph_data.bits.size == 0)
        return 0;
    gdata = glyph_data.bits.data;
    if (S16(gdata) != -1) {
        /* This is a simple glyph. */
        int numContours = S16(gdata);
        const byte *pends = gdata + 10;
        const byte *pinstr = pends + numContours * 2;

        total = (numContours == 0 ? 0 : U16(pinstr - 2) + 1);
    } else {
        /* This is a composite glyph.  Add up the components. */
        uint flags;
        gs_matrix_fixed mat;

        gdata += 10;
        memset(&mat, 0, sizeof(mat)); /* arbitrary */
        total = 0;
        do {
            code = total_points(pfont, U16(gdata + 2));
            if (code < 0)
                return code;
            total += code;
            gs_type42_parse_component(&gdata, &flags, &mat, NULL, pfont, &mat);
        }
        while (flags & TT_CG_MORE_COMPONENTS);
    }
    gs_glyph_data_free(&glyph_data, "total_points");
    return total;
}
#endif /* not called */

/*
 * Define the default implementation for getting the glyph index from a
 * gs_glyph.  This is trivial for integer ("CID" but representing a GID)
 * gs_glyph values, and not implemented for name glyphs.
 */
static uint
default_get_glyph_index(gs_font_type42 *pfont, gs_glyph glyph)
{
    return (glyph < GS_MIN_CID_GLYPH ? 0 : /* undefined */
            glyph - GS_MIN_CID_GLYPH);
}

/* Define the default implementation for getting the outline data for */
/* a glyph, using indexToLocFormat and the loca and glyf tables. */
/* Set pglyph->data = 0 if the glyph is empty. */
static int
default_get_outline(gs_font_type42 * pfont, uint glyph_index,
                    gs_glyph_data_t *pgd)
{
    int (*string_proc) (gs_font_type42 *, ulong, uint, const byte **) =
        pfont->data.string_proc;
    ulong glyph_start;
    uint glyph_length;
    int code;

     if (glyph_index >= pfont->data.numGlyphs)
        return_error(gs_error_invalidfont);
    glyph_start = get_glyph_offset(pfont, glyph_index);
    if (pfont->data.len_glyphs)
        glyph_length = pfont->data.len_glyphs[glyph_index];
    else
        glyph_length = 0;
    if (glyph_length == 0)
        gs_glyph_data_from_null(pgd);
    else {
        const byte *data;
        byte *buf;

        code = (*string_proc)(pfont, (ulong)(pfont->data.glyf + glyph_start),
                              glyph_length, &data);
        if (code < 0)
            return code;
        if (code == 0)
            gs_glyph_data_from_string(pgd, data, glyph_length, NULL);
        else {
            /*
             * The glyph is segmented in sfnts.
             * It is not allowed with Type 42 specification.
             * Perhaps we can handle it (with a low performance),
             * making a contiguous copy.
             */

            /* 'code' is the returned length */
            buf = (byte *)gs_alloc_string(pgd->memory, glyph_length, "default_get_outline");
            if (buf == 0)
                return_error(gs_error_VMerror);
            gs_glyph_data_from_string(pgd, buf, glyph_length, (gs_font *)pfont);
            memcpy(buf, data, code);
            return gs_type42_read_data(pfont, pfont->data.glyf + glyph_start + code,
                                       glyph_length - code, buf + code);
        }
    }
    return 0;
}

/* Take outline data from a True Type font file. */
int
gs_type42_get_outline_from_TT_file(gs_font_type42 * pfont, stream *s, uint glyph_index,
                gs_glyph_data_t *pgd)
{
    uchar lbuf[8];
    ulong glyph_start;
    uint glyph_length, count;

    /* fixme: Since this function is being called multiple times per glyph,
     * we should cache glyph data in a buffer associated with the font.
     */
    if (pfont->data.indexToLocFormat) {
        sseek(s, pfont->data.loca + glyph_index * 4);
        sgets(s, lbuf, 8, &count);
        if (count < 8)
            return_error(gs_error_invalidfont);
        glyph_start = u32(lbuf);
        glyph_length = u32(lbuf + 4) - glyph_start;
    } else {
        sseek(s, pfont->data.loca + glyph_index * 2);
        sgets(s, lbuf, 4, &count);
        if (count < 4)
            return_error(gs_error_invalidfont);
        glyph_start = (ulong) U16(lbuf) << 1;
        glyph_length = ((ulong) U16(lbuf + 2) << 1) - glyph_start;
    }
    if (glyph_length == 0)
        gs_glyph_data_from_null(pgd);
    else {
        byte *buf;

        sseek(s, pfont->data.glyf + glyph_start);
        buf = (byte *)gs_alloc_string(pgd->memory, glyph_length, "default_get_outline");
        if (buf == 0)
            return_error(gs_error_VMerror);
        gs_glyph_data_from_string(pgd, buf, glyph_length, (gs_font *)pfont);
        sgets(s, buf, glyph_length, &count);
        if (count < glyph_length)
            return_error(gs_error_invalidfont);
    }
    return 0;
}

uint
gs_type42_substitute_glyph_index_vertical(gs_font_type42 *pfont, uint glyph_index,
                                          int WMode, gs_glyph cid)
{   /* A rough trial implementation, possibly needs improvements or optimization. */
    /* Fixme: optimize : Initialize subtable_ptr when the font is defined. */
    byte *gsub_ptr = pfont->data.gsub;
    typedef struct GSUB_s {
        uint32_t Version;
        uint16_t ScriptList;
        uint16_t FeatureList;
        uint16_t LookupList;
    } GSUB;
#if 0 /* Currently unused, but maybe useful for future. */
    ulong gsub_size = pfont->data.gsub_size;
    typedef struct ScriptRecord_s {
        byte Tag[4];
        uint16_t Offset; /* Offset to Script table from beginning of ScriptList */
    } ScriptRecord;
    typedef struct ScriptList_s {
        uint16_t ScriptCount;
        ScriptRecord records[1 /* ScriptCount */];
    } ScriptList;
    typedef struct LangSysRecord_s {
        byte Tag[4];
        uint16_t Offset;  /* Offset to LangSys table from beginning of Script table */
    } LangSysRecord;
    typedef struct ScriptTable_s {
        uint16_t DefaultLangSys;
        uint16_t LangSysCount;
        LangSysRecord records[1/* LangSysCount */];
    } ScriptTable;
    typedef struct LangSysTable_s {
        uint16_t LookupOrder;
        uint16_t ReqFeatureIndex;
        uint16_t FeatureCount;
        uint16_t FeatureIndex[1/* FeatureCount */];
    } LangSysTable;
    typedef struct FeatureRecord_s {
        byte Tag[4];
        uint16_t Feature; /* Offset to Feature table from beginning of FeatureList */
    } FeatureRecord;
    typedef struct FeatureListTable_s {
        uint16_t FeatureCount;
        FeatureRecord records[1/* FeatureCount */];
    } FeatureListTable;
#endif
    typedef struct LookupListTable_s {
        uint16_t LookupCount;
        uint16_t Lookup[1/* LookupCount */]; /* offsets to Lookup tables-from beginning of LookupList */
    } LookupListTable;
    typedef struct LookupTable_s {
        uint16_t LookupType;
        uint16_t LookupFlag;
        uint16_t SubTableCount;
        uint16_t SubTable[1/* SubTableCount */]; /* offsets to Lookup tables-from beginning of LookupList */
    } LookupTable;
    typedef struct SingleSubstFormat1_s {
        uint16_t SubstFormat; /* ==1 */
        uint16_t Coverage; /* Offset to Coverage table-from beginning of Substitution table */
        uint16_t DeltaGlyphId;
    } SingleSubstFormat1;
    typedef uint16_t GlyphID;
    typedef struct SingleSubstFormat2_s {
        uint16_t SubstFormat; /* ==2 */
        uint16_t Coverage; /* Offset to Coverage table-from beginning of Substitution table */
        uint16_t GlyphCount;
        GlyphID Substitute[1/* GlyphCount */];
    } SingleSubstFormat2;
    typedef struct CoverageFormat1_s {
        uint16_t CoverageFormat; /* ==1 */
        uint16_t GlyphCount;
        GlyphID GlyphArray[1/*GlyphCount*/]; /* Array of GlyphIDs-in numerical order */
    } CoverageFormat1;
    typedef struct RangeRecord_s {
        GlyphID Start; /* First GlyphID in the range */
        GlyphID End; /* Last GlyphID in the range */
        uint16_t StartCoverageIndex; /* Coverage Index of first GlyphID in range */
    } RangeRecord;
    typedef struct CoverageFormat2_s {
        uint16_t CoverageFormat; /* ==2 */
        uint16_t RangeCount;
        RangeRecord RangeArray[1/*RangeCount*/]; /* Array of GlyphIDs-in numerical order */
    } CoverageFormat2;
    int i, j;
    GSUB gsub;
    LookupListTable lookup_list_table;
    byte *lookup_list_ptr;

    if (WMode == 0 || gsub_ptr == NULL)
        return glyph_index;

    /* GSUB header */
    gsub.Version = u32(gsub_ptr + offset_of(GSUB, Version));
    gsub.ScriptList = U16(gsub_ptr + offset_of(GSUB, ScriptList));
    gsub.FeatureList = U16(gsub_ptr + offset_of(GSUB, FeatureList));
    gsub.LookupList = U16(gsub_ptr + offset_of(GSUB, LookupList));
    /* LookupList table */
    lookup_list_ptr = gsub_ptr + gsub.LookupList;
    lookup_list_table.LookupCount = U16(lookup_list_ptr + offset_of(LookupListTable, LookupCount));
    for (i = 0; i < lookup_list_table.LookupCount; i++) {
        byte *lookup_table_offset_ptr = lookup_list_ptr + offset_of(LookupListTable, Lookup)
                               + i * sizeof(uint16_t);
        byte *lookup_table_ptr = lookup_list_ptr + U16(lookup_table_offset_ptr);
        LookupTable lookup_table;

        lookup_table.LookupType = U16(lookup_table_ptr + offset_of(LookupTable, LookupType));
        lookup_table.LookupFlag = U16(lookup_table_ptr + offset_of(LookupTable, LookupFlag));
        lookup_table.SubTableCount = U16(lookup_table_ptr + offset_of(LookupTable, SubTableCount));

        if (lookup_table.LookupType == 1) {
            /* Currently we're interesting in Format 1 only, which is only useful
               for single glyph substitution for vertical fonts.
               We copied the logic of this choice from the CJK patch attached to bug 689304,
               and we think that it may need a further improvement. */
            byte *subtable_offset_ptr = lookup_table_ptr + offset_of(LookupTable, SubTable);
            for (j = 0; j < lookup_table.SubTableCount; j++) {
                byte *subtable_ptr = lookup_table_ptr + U16(subtable_offset_ptr + j * sizeof(uint16_t));
                uint16_t format = U16(subtable_ptr);

                if (format == 1) {
                    SingleSubstFormat1 subst;

                    subst.SubstFormat = format; /* Debug purpose. */
                    subst.Coverage = U16(subtable_ptr + offset_of(SingleSubstFormat1, Coverage));
                    subst.DeltaGlyphId = U16(subtable_ptr + offset_of(SingleSubstFormat1, DeltaGlyphId));
                    format = subst.SubstFormat; /* Stops gcc warning */
                } else {
                    SingleSubstFormat2 subst;
                    byte *coverage_ptr;
                    uint16_t coverage_format;

                    subst.SubstFormat = format; /* Debug purpose. */
                    subst.Coverage = U16(subtable_ptr + offset_of(SingleSubstFormat2, Coverage));
                    subst.GlyphCount = U16(subtable_ptr + offset_of(SingleSubstFormat2, GlyphCount));
                    coverage_ptr = subtable_ptr + subst.Coverage;
                    coverage_format = U16(coverage_ptr);
                    if (coverage_format == 1) {
                        CoverageFormat1 cov;

                        cov.CoverageFormat = coverage_format; /* Debug purpose only. */
                        cov.GlyphCount = U16(coverage_ptr + offset_of(CoverageFormat1, GlyphCount));
                        {   /* Binary search. */
                            int k0 = 0, k1 = cov.GlyphCount;

                            for (;;) {
                                int k = (k0 + k1) / 2;
                                GlyphID glyph = U16(coverage_ptr + offset_of(CoverageFormat1, GlyphArray)
                                                         + sizeof(GlyphID) * k);
                                if (glyph_index == glyph) {
                                    /* Found. */
                                    if (k >= cov.GlyphCount)
                                        break; /* Wrong data ? (not sure). */
                                    else {
                                        GlyphID new_glyph = U16(subtable_ptr + offset_of(SingleSubstFormat2, Substitute)
                                                            + sizeof(GlyphID) * k);

                                        return new_glyph;
                                    }
                                } else if (k0 >= k1 - 1) {
                                    break; /* Not found. */
                                } else if (glyph_index < glyph)
                                    k1 = k;
                                else
                                    k0 = k + 1;
                            }
                        }
                    } else if (coverage_format == 2) {
                        CoverageFormat2 cov;

                        cov.CoverageFormat = coverage_format; /* Debug purpose only. */
                        cov.RangeCount = U16(coverage_ptr + offset_of(CoverageFormat2, RangeCount));
                        {   /* Binary search. */
                            int k0 = 0, k1 = cov.RangeCount;

                            for (;;) {
                                int k = (k0 + k1) / 2;
                                RangeRecord rr;

                                rr.Start = U16(coverage_ptr + offset_of(CoverageFormat2, RangeArray)
                                        + sizeof(RangeRecord) * k  + offset_of(RangeRecord, Start));
                                rr.End = U16(coverage_ptr + offset_of(CoverageFormat2, RangeArray)
                                        + sizeof(RangeRecord) * k  + offset_of(RangeRecord, End));
                                rr.StartCoverageIndex = U16(coverage_ptr + offset_of(CoverageFormat2, RangeArray)
                                        + sizeof(RangeRecord) * k  + offset_of(RangeRecord, StartCoverageIndex));

                                if (rr.Start <= glyph_index && glyph_index <= rr.End) {
                                    /* Found. */
                                    if (k >= cov.RangeCount)
                                        break; /* Wrong data ? (not sure). */
                                    else {
                                        uint16_t subst_index = rr.StartCoverageIndex + (glyph_index - rr.Start);
                                        GlyphID new_glyph = U16(subtable_ptr + offset_of(SingleSubstFormat2, Substitute)
                                                            + sizeof(GlyphID) * subst_index);

                                        return new_glyph;
                                    }
                                } else if (k0 >= k1 - 1) {
                                    break; /* Not found. */
                                } else if (glyph_index < rr.Start)
                                    k1 = k;
                                else
                                    k0 = k + 1;
                            }
                        }
                    } else {
                        /* Not implemented yet. */
                    }
                }
            }
        }
    }
    return glyph_index;
}

/* Parse a glyph into pieces, if any. */
static int
parse_pieces(gs_font_type42 *pfont, gs_glyph glyph, gs_glyph *pieces,
             int *pnum_pieces)
{
    uint glyph_index = (glyph >= GS_MIN_GLYPH_INDEX
                        ? glyph - GS_MIN_GLYPH_INDEX
                        : pfont->data.get_glyph_index(pfont, glyph));
    gs_glyph_data_t glyph_data;
    int code;

    glyph_data.memory = pfont->memory;
    code = pfont->data.get_outline(pfont, glyph_index, &glyph_data);
    if (code < 0)
        return code;
    if (glyph_data.bits.size != 0 && S16(glyph_data.bits.data) == -1) {
        /* This is a composite glyph. */
        int i = 0;
        uint flags = TT_CG_MORE_COMPONENTS;
        const byte *gdata = glyph_data.bits.data + 10;
        gs_matrix_fixed mat;

        memset(&mat, 0, sizeof(mat)); /* arbitrary */
        for (i = 0; flags & TT_CG_MORE_COMPONENTS; ++i) {
            if (pieces) {
                pieces[i] = U16(gdata + 2) + GS_MIN_GLYPH_INDEX;
                if (U16(gdata + 2) > pfont->data.numGlyphs) {
                    *pnum_pieces = 0;
                    return_error(gs_error_invalidfont);
                }
            }
            gs_type42_parse_component(&gdata, &flags, &mat, NULL, pfont, &mat);
        }
        *pnum_pieces = i;
    } else
        *pnum_pieces = 0;
    gs_glyph_data_free(&glyph_data, "parse_pieces");
    return 0;
}

/* Define the font procedures for a Type 42 font. */
int
gs_type42_glyph_outline(gs_font *font, int WMode, gs_glyph glyph, const gs_matrix *pmat,
                        gx_path *ppath, double sbw[4])
{
    gs_font_type42 *const pfont = (gs_font_type42 *)font;
    uint glyph_index;
    gs_fixed_point origin;
    int code;
    gs_glyph_info_t info;
    static const gs_matrix imat = { identity_matrix_body };
    bool design_grid = true;
    const gs_log2_scale_point log2_scale = {0, 0};
    /* fixme : The subpixel numbers doesn't pass through the font_proc_glyph_outline interface.
       High level devices can't get a proper grid fitting with AlignToPixels = 1.
       Currently font_proc_glyph_outline is only used by pdfwrite for computing a
       character bbox, which doesn't need a grid fitting.
       We apply design grid here.
     */
    cached_fm_pair *pair;

    if (glyph >= GS_MIN_GLYPH_INDEX)
        glyph_index = glyph - GS_MIN_GLYPH_INDEX;
    else {
        glyph_index = pfont->data.get_glyph_index(pfont, glyph);
        if (pfont->data.gsub_size) {
            if (pfont->data.substitute_glyph_index_vertical != NULL)
                glyph_index = pfont->data.substitute_glyph_index_vertical(pfont, glyph_index, WMode, glyph);
            else
                glyph_index = gs_type42_substitute_glyph_index_vertical(pfont, glyph_index, WMode, glyph);
        }
    }
    code = gx_lookup_fm_pair(font, pmat, &log2_scale, design_grid, &pair);
    if (code < 0)
        return code;
    if (pmat == 0)
        pmat = &imat;
    if (!pair->ttf) {
        void *FAPI_store = ((gs_font_base *)font)->FAPI;
        ((gs_font_base *)font)->FAPI = NULL;
        gx_provide_fm_pair_attributes(font->dir, font, pair, pmat, &log2_scale, false);
        ((gs_font_base *)font)->FAPI = FAPI_store;
    }
    if ((code = gx_path_current_point(ppath, &origin)) < 0 ||
        (code = append_outline_fitted(glyph_index, pmat, ppath, pair,
                                        &log2_scale, design_grid)) < 0 ||
        (code = font->procs.glyph_info(font, glyph, pmat,
                                       GLYPH_INFO_WIDTH0 << WMode, &info)) < 0
        )
        return code;
    return gx_path_add_point(ppath, origin.x + float2fixed(info.width[WMode].x),
                             origin.y + float2fixed(info.width[WMode].y));
}

/* Get glyph info by glyph index. */
int
gs_type42_glyph_info_by_gid(gs_font *font, gs_glyph glyph, const gs_matrix *pmat,
                     int members, gs_glyph_info_t *info, uint glyph_index)
{
    gs_font_type42 *const pfont = (gs_font_type42 *)font;
    int default_members =
        members & ~(GLYPH_INFO_WIDTHS | GLYPH_INFO_NUM_PIECES |
                    GLYPH_INFO_PIECES | GLYPH_INFO_OUTLINE_WIDTHS |
                    GLYPH_INFO_VVECTOR0 | GLYPH_INFO_VVECTOR1);
    gs_glyph_data_t outline;
    int code = 0;

    outline.memory = pfont->memory;
    if (default_members) {
        if (pmat == NULL)
            return gs_note_error(gs_error_undefinedresult);

        code = gs_default_glyph_info(font, glyph, pmat, default_members, info);

        if (code < 0)
            return code;
    } else if ((code = pfont->data.get_outline(pfont, glyph_index, &outline)) < 0)
        return code;		/* non-existent glyph */
    else {
        gs_glyph_data_free(&outline, "gs_type42_glyph_info");
        info->members = 0;
    }
    if (members & GLYPH_INFO_WIDTHS) {
        int i;

        for (i = 0; i < 2; ++i)
            if (members & (GLYPH_INFO_WIDTH0 << i)) {
                float sbw[4];

                code = pfont->data.get_metrics(pfont, glyph_index, i, sbw);
                if (code < 0) {
                    code = 0;
                    continue;
                }
                if (pmat) {
                    code = gs_point_transform(sbw[2], sbw[3], pmat,
                                              &info->width[i]);
                    if (code < 0)
                        return code;
                    code = gs_point_transform(sbw[0], sbw[1], pmat,
                                              &info->v);
                } else {
                    info->width[i].x = sbw[2], info->width[i].y = sbw[3];
                    info->v.x = sbw[0], info->v.y = sbw[1];
                }
                info->members |= (GLYPH_INFO_VVECTOR0 << i);
                info->members |= (GLYPH_INFO_WIDTH << i);
            }

    }
    if (members & (GLYPH_INFO_NUM_PIECES | GLYPH_INFO_PIECES)) {
        gs_glyph *pieces =
            (members & GLYPH_INFO_PIECES ? info->pieces : (gs_glyph *)0);
        int code = parse_pieces(pfont, glyph, pieces, &info->num_pieces);

        if (code < 0)
            return code;
        info->members |= members & (GLYPH_INFO_NUM_PIECES | GLYPH_INFO_PIECES);
    }
    return code;
}
int
gs_type42_glyph_info(gs_font *font, gs_glyph glyph, const gs_matrix *pmat,
                     int members, gs_glyph_info_t *info)
{
    gs_font_type42 *const pfont = (gs_font_type42 *)font;
    uint glyph_index;

    if (glyph >= GS_MIN_GLYPH_INDEX)
        glyph_index = glyph - GS_MIN_GLYPH_INDEX;
    else {
        glyph_index = pfont->data.get_glyph_index(pfont, glyph);
        if (glyph_index == GS_NO_GLYPH)
            return_error(gs_error_undefined);
        if (pfont->data.gsub_size) {
            int WMode = ((members & (GLYPH_INFO_WIDTH1 | GLYPH_INFO_VVECTOR1)) ? 1 : 0);

            if (pfont->data.substitute_glyph_index_vertical != NULL)
                glyph_index = pfont->data.substitute_glyph_index_vertical(pfont, glyph_index, WMode, glyph);
            else
                glyph_index = gs_type42_substitute_glyph_index_vertical(pfont, glyph_index, WMode, glyph);
        }
    }
    return gs_type42_glyph_info_by_gid(font, glyph, pmat, members, info, glyph_index);

}
int
gs_type42_enumerate_glyph(gs_font *font, int *pindex,
                          gs_glyph_space_t glyph_space, gs_glyph *pglyph)
{
    gs_font_type42 *const pfont = (gs_font_type42 *)font;

    while (++*pindex <= pfont->data.numGlyphs) {
        gs_glyph_data_t outline;
        uint glyph_index = *pindex - 1;
        int code;

        outline.memory = pfont->memory;
        code = pfont->data.get_outline(pfont, glyph_index, &outline);
        if (code < 0)
            return code;
        if (outline.bits.data == 0)
            continue;		/* empty (undefined) glyph */
        *pglyph = glyph_index + GS_MIN_GLYPH_INDEX;
        gs_glyph_data_free(&outline, "gs_type42_enumerate_glyph");
        return 0;
    }
    /* We are done. */
    *pindex = 0;
    return 0;
}

/* Get the metrics of a simple glyph. */
static int
simple_glyph_metrics(gs_font_type42 * pfont, uint glyph_index, int wmode,
                     float sbw[4])
{
    double factor = 1.0 / pfont->data.unitsPerEm;
    uint width;
    int lsb;
    int code;

    {
        const gs_type42_mtx_t *pmtx = &pfont->data.metrics[wmode];
        uint num_metrics = pmtx->numMetrics;
        byte pmetrics[4];

        if (pmtx->length == 0)
            return_error(gs_error_rangecheck);
        if (glyph_index < num_metrics) {
            READ_SFNTS(pfont, pmtx->offset + glyph_index * 4, 4, pmetrics);
            width = U16(pmetrics);
            lsb = S16(pmetrics + 2);
        } else {
            uint offset = pmtx->offset + num_metrics * 4;
            uint glyph_offset = (glyph_index - num_metrics) * 2;
            byte plsb[2];

            READ_SFNTS(pfont, offset - 4, 4, pmetrics);
            width = U16(pmetrics);
            if (glyph_offset >= pmtx->length)
                glyph_offset = pmtx->length - 2;
            READ_SFNTS(pfont, offset + glyph_offset, 2, plsb);
            lsb = S16(plsb);
        }
    }
    if (wmode) {
        factor = -factor;	/* lsb and width go down the page */
        sbw[0] = 0, sbw[1] = lsb * factor;
        sbw[2] = 0, sbw[3] = width * factor;
    } else {
        sbw[0] = lsb * factor, sbw[1] = 0;
        sbw[2] = width * factor, sbw[3] = 0;
    }
    return 0;
}

/* Export the default get_metrics procedure.
   The length of sbw is >=4 when bbox in not requested,
   and 8 otherwise.
 */
int
gs_type42_default_get_metrics(gs_font_type42 * pfont, uint glyph_index,
                              gs_type42_metrics_options_t options, float *sbw)
{
    gs_glyph_data_t glyph_data;
    int code;
    int result = 0;
    int wmode = gs_type42_metrics_options_wmode(options);
    int sbw_requested  = gs_type42_metrics_options_sbw_requested(options);
    int bbox_requested = gs_type42_metrics_options_bbox_requested(options);

    glyph_data.memory = pfont->memory;
    code = pfont->data.get_outline(pfont, glyph_index, &glyph_data);
    if (code < 0)
        return code;
    if (bbox_requested) {
        if (glyph_data.bits.size >= 10 && bbox_requested) {
            /* Note: The glyph bbox usn't useful for Dynalab fonts,
               which stretch subglyphs. Therefore we don't
               process subglyphs here. */
            double factor = 1.0 / pfont->data.unitsPerEm;

            sbw[4] = S16(glyph_data.bits.data + 2) * factor;
            sbw[5] = S16(glyph_data.bits.data + 4) * factor;
            sbw[6] = S16(glyph_data.bits.data + 6) * factor;
            sbw[7] = S16(glyph_data.bits.data + 8) * factor;
        } else
            sbw[4] = sbw[5] = sbw[6] = sbw[7] = 0;
    }
    if (sbw_requested) {
        if (glyph_data.bits.size != 0 && S16(glyph_data.bits.data) == -1) {
            /* This is a composite glyph. */
            uint flags;
            const byte *gdata = glyph_data.bits.data + 10;
            gs_matrix_fixed mat;

            memset(&mat, 0, sizeof(mat)); /* arbitrary */
            do {
                uint comp_index = U16(gdata + 2);

                if (comp_index == glyph_index) {
                    result = gs_note_error(gs_error_invalidfont);
                    goto done;
                }

                gs_type42_parse_component(&gdata, &flags, &mat, NULL, pfont, &mat);
                if (flags & TT_CG_USE_MY_METRICS) {
                    result = pfont->data.get_metrics(pfont, comp_index, wmode, sbw);
                    goto done;
                }
            }
            while (flags & TT_CG_MORE_COMPONENTS);
        }
        result = simple_glyph_metrics(pfont, glyph_index, wmode, sbw);
    }
 done:
    gs_glyph_data_free(&glyph_data, "gs_type42_default_get_metrics");
    return result;
}
int
gs_type42_get_metrics(gs_font_type42 * pfont, uint glyph_index,
                      float sbw[4])
{
    return pfont->data.get_metrics(pfont, glyph_index, pfont->WMode, sbw);
}

/* Define the bits in the glyph flags. */
#define gf_OnCurve 1
#define gf_xShort 2
#define gf_yShort 4
#define gf_Repeat 8
#define gf_xPos 16		/* xShort */
#define gf_xSame 16		/* !xShort */
#define gf_yPos 32		/* yShort */
#define gf_ySame 32		/* !yShort */

/* Append a TrueType outline to a path. */
/* Note that this does not append the final moveto for the width. */
int
gs_type42_append(uint glyph_index, gs_gstate * pgs,
                 gx_path * ppath, gs_text_enum_t *penum, gs_font *pfont,
                 bool charpath_flag)
{
    const gs_log2_scale_point * pscale = &penum->log2_scale;
    cached_fm_pair *pair = penum->pair;
    int code = append_outline_fitted(glyph_index, &ctm_only(pgs), ppath,
                        pair, pscale, charpath_flag);

    if (code < 0) {
        if (pgs->in_cachedevice == CACHE_DEVICE_CACHING) {
            /* Perform the cache cleanup, when the cached character data
               has been allocated (gx_alloc_char_bits) but
               the character has not been added to the cache (gx_add_cached_char)
               due to a falure in the character renderer.
             */
            gs_show_enum *const penum_s = (gs_show_enum *)penum;

            if (penum_s->cc != NULL) {
                gx_free_cached_char(pfont->dir, penum_s->cc);
                penum_s->cc = NULL;
            }
        }
        return code;
    }
    code = gx_setcurrentpoint_from_path(pgs, ppath);
    if (code < 0)
        return code;
    /* Set the flatness for curve rendering. */
    return gs_gstate_setflat(pgs, gs_char_flatness(pgs, 1.0));
}
#if 0
/* Used only by add_simple below, which has been removed as unused. */
/* Add 2nd degree Bezier to the path */
static int
add_quadratic_curve(gx_path * const ppath, const gs_fixed_point * const a,
     const gs_fixed_point * const b, const gs_fixed_point * const c)
{
    return gx_path_add_curve(ppath, (a->x + 2*b->x)/3, (a->y + 2*b->y)/3,
        (c->x + 2*b->x)/3, (c->y + 2*b->y)/3, c->x, c->y);
}
#endif
#if 0
/* Used only by check_component below, which was removed as unused. */
/*
 * Append a simple glyph outline to a path (ppath != 0) and/or return
 * its list of points (ppts != 0).
 */
static int
append_simple(const byte *gdata, float sbw[4], const gs_matrix_fixed *pmat,
              gx_path *ppath, gs_fixed_point *ppts, gs_font_type42 * pfont,
              bool subglyph)
{
    int numContours = S16(gdata);
    const byte *pends = gdata + 10;
    const byte *pinstr = pends + numContours * 2;
    const byte *pflags;
    uint npoints;
    const byte *pxc, *pyc;
    int code;

    if (numContours == 0)
        return 0;
    /*
     * It appears that the only way to find the X and Y coordinate
     * tables is to parse the flags.  If this is true, it is an
     * incredible piece of bad design.
     */
    {
        const byte *pf = pflags = pinstr + 2 + U16(pinstr);
        uint xbytes = npoints = U16(pinstr - 2) + 1;
        uint np = npoints;

        while (np > 0) {
            byte flags = *pf++;
            uint reps = (flags & gf_Repeat ? *pf++ + 1 : 1);

            if (!(flags & gf_xShort)) {
                if (flags & gf_xSame)
                    xbytes -= reps;
                else
                    xbytes += reps;
            }
            np -= reps;
        }
        pxc = pf;
        pyc = pxc + xbytes;
    }

    /* Interpret the contours. */

    {
        uint i, np;
        float offset = 0;
        gs_fixed_point pt;
        double factor = 1.0 / pfont->data.unitsPerEm;
        /*
         * Decode the first flag byte outside the loop, to avoid a
         * compiler warning about uninitialized variables.
         */
        byte flags = *pflags++;
        uint reps = (flags & gf_Repeat ? *pflags++ + 1 : 1);

        if (!subglyph) {
            int xmin = S16(gdata + 2); /* We like to see it with debugger. */

            offset = sbw[0] - xmin * factor;
        }
        gs_point_transform2fixed(pmat, offset, 0.0, &pt);
        for (i = 0, np = 0; i < numContours; ++i) {
            bool move = true;
            bool off_curve = false;
            bool is_start_off = false;
            uint last_point = U16(pends + i * 2);
            float dx, dy;
            gs_fixed_point start,pt_start_off;
            gs_fixed_point cpoints[2];

            if_debug1m('1', pfont->memory, "[1t]start %d\n", i);

            for (; np <= last_point; --reps, ++np) {
                gs_fixed_point dpt;

                if (reps == 0) {
                    flags = *pflags++;
                    reps = (flags & gf_Repeat ? *pflags++ + 1 : 1);
                }
                if (flags & gf_xShort) {
                    /*
                     * A bug in the Watcom compiler prevents us from doing
                     * the following with the obvious conditional expression.
                     */
                    if (flags & gf_xPos)
                        dx = *pxc++ * factor;
                    else
                        dx = -(int)*pxc++ * factor;
                } else if (!(flags & gf_xSame))
                    dx = S16(pxc) * factor, pxc += 2;
                else
                    dx = 0;
                if (flags & gf_yShort) {
                    /* See above under dx. */
                    if (flags & gf_yPos)
                        dy = *pyc++ * factor;
                    else
                        dy = -(int)*pyc++ * factor;
                } else if (!(flags & gf_ySame))
                    dy = S16(pyc) * factor, pyc += 2;
                else
                    dy = 0;
                code = gs_distance_transform2fixed(pmat, dx, dy, &dpt);
                if (code < 0)
                    return code;
                pt.x += dpt.x, pt.y += dpt.y;

                if (ppts)	/* return the points */
                    ppts[np] = pt;

                if (ppath) {
                    /* append to a path */
                    if_debug3m('1', pfont->memory, "[1t]%s (%g %g)\n",
                               (flags & gf_OnCurve ? "on " : "off"),
                               fixed2float(pt.x), fixed2float(pt.y));

                    if (move) {
                        if(is_start_off) {
                            if(flags & gf_OnCurve)
                                start = pt;
                            else {
                                start.x = (pt_start_off.x + pt.x)/2;
                                start.y = (pt_start_off.y + pt.y)/2;
                                cpoints[1]=pt;
                                off_curve=true;
                            }
                            move = false;
                            cpoints[0] = start;
                            code = gx_path_add_point(ppath, start.x, start.y);
                        } else {
                            if(flags & gf_OnCurve) {
                                cpoints[0] = start = pt;
                                code = gx_path_add_point(ppath, pt.x, pt.y);
                                move = false;
                            } else {
                                is_start_off = true;
                                pt_start_off = pt;
                            }
                        }
                    } else if (flags & gf_OnCurve) {
                        if (off_curve)
                            code = add_quadratic_curve(ppath, cpoints, cpoints+1, &pt);
                        else
                            code = gx_path_add_line(ppath, pt.x, pt.y);
                        cpoints[0] = pt;
                        off_curve = false;
                    } else {
                        if(off_curve) {
                            gs_fixed_point p;
                            p.x = (cpoints[1].x + pt.x)/2;
                            p.y = (cpoints[1].y + pt.y)/2;
                            code = add_quadratic_curve(ppath, cpoints, cpoints+1, &p);
                            cpoints[0] = p;
                        }
                        off_curve = true;
                        cpoints[1] = pt;
                    }
                    if (code < 0)
                        return code;
                }
            }
            if (ppath) {
                if (is_start_off) {
                    if (off_curve) {
                        gs_fixed_point p;
                        p.x = (cpoints[1].x + pt_start_off.x)/2;
                        p.y = (cpoints[1].y + pt_start_off.y)/2;
                        code = add_quadratic_curve(ppath, cpoints, cpoints+1, &p);
                        if (code < 0)
                            return code;
                        code = add_quadratic_curve(ppath, &p, &pt_start_off, &start);
                        if (code < 0)
                            return code;
                    } else {
                        code = add_quadratic_curve(ppath, cpoints, &pt_start_off, &start);
                        if (code < 0)
                            return code;
                    }
                } else {
                    if (off_curve) {
                        code = add_quadratic_curve(ppath, cpoints, cpoints+1, &start);
                        if (code < 0)
                            return code;
                    }
                }
                code = gx_path_close_subpath(ppath);
                if (code < 0)
                    return code;
            }
        }
    }
    return 0;
}
#endif
#if 0
/* Used only by append_component below, which is also apprently unused and
 * was removed by RJW.
 */
/* Append a glyph outline. */
static int
check_component(uint glyph_index, const gs_matrix_fixed *pmat,
                gx_path *ppath, gs_font_type42 *pfont, gs_fixed_point *ppts,
                gs_glyph_data_t *pgd, bool subglyph)
{
    gs_glyph_data_t glyph_data;
    const byte *gdata;
    float sbw[4];
    int numContours;
    int code;

    glyph_data.memory = pfont->memory;
    code = pfont->data.get_outline(pfont, glyph_index, &glyph_data);
    if (code < 0)
        return code;
    gdata = glyph_data.bits.data;
    if (gdata == 0 || glyph_data.bits.size == 0)	/* empty glyph */
        return 0;
    numContours = S16(gdata);
    if (numContours >= 0) {
        gs_type42_get_metrics(pfont, glyph_index, sbw);
        code = append_simple(gdata, sbw, pmat, ppath, ppts, pfont, subglyph);
        gs_glyph_data_free(&glyph_data, "check_component");
        return (code < 0 ? code : 0); /* simple */
    }
    if (numContours != -1)
        return_error(gs_error_rangecheck);
    *pgd = glyph_data;
    return 1;			/* composite */
}
#endif
#if 0
/* RJW: Does not appear to be used */
static int
append_component(uint glyph_index, const gs_matrix_fixed * pmat,
                 gx_path * ppath, gs_fixed_point *ppts, int point_index,
                 gs_font_type42 * pfont, bool subglyph)
{
    gs_glyph_data_t glyph_data;
    int code;

    glyph_data.memory = pfont->memory;
    code = check_component(glyph_index, pmat, ppath, pfont, ppts + point_index,
                           &glyph_data, subglyph);
    if (code != 1)
        return code;
    /*
     * This is a composite glyph.  Because of the "point matching" feature,
     * we have to do an extra pass over each component to fill in the
     * table of points.
     */
    {
        uint flags;
        const byte *gdata = glyph_data.bits.data + 10;

        do {
            uint comp_index = U16(gdata + 2);
            gs_matrix_fixed mat;
            int mp[2];

            gs_type42_parse_component(&gdata, &flags, &mat, mp, pfont, pmat);
            if (mp[0] >= 0) {
                /* Match up points.  What a nuisance! */
                const gs_fixed_point *const pfrom = ppts + mp[0];
                /*
                 * Contrary to the TrueType documentation, mp[1] is not
                 * relative to the start of the compound glyph, but is
                 * relative to the start of the component.
                 */
                const gs_fixed_point *const pto = ppts + point_index + mp[1];
                gs_fixed_point diff;

                code = append_component(comp_index, &mat, NULL, ppts,
                                        point_index, pfont, true);
                if (code < 0)
                    break;
                diff.x = pfrom->x - pto->x;
                diff.y = pfrom->y - pto->y;
                mat.tx = fixed2float(mat.tx_fixed += diff.x);
                mat.ty = fixed2float(mat.ty_fixed += diff.y);
            }
            code = append_component(comp_index, &mat, ppath, ppts,
                                    point_index, pfont, true);
            if (code < 0)
                break;
            point_index += total_points(pfont, comp_index);
        }
        while (flags & TT_CG_MORE_COMPONENTS);
    }
    gs_glyph_data_free(&glyph_data, "append_component");
    return code;
}
#endif

static int
append_outline_fitted(uint glyph_index, const gs_matrix * pmat,
               gx_path * ppath, cached_fm_pair * pair,
               const gs_log2_scale_point * pscale, bool design_grid)
{
    gs_font_type42 *pfont = (gs_font_type42 *)pair->font;
    int code;

    gx_ttfReader__set_font(pair->ttr, pfont);
    code = gx_ttf_outline(pair->ttf, pair->ttr, pfont, (uint)glyph_index,
        pmat, pscale, ppath, design_grid);
    gx_ttfReader__set_font(pair->ttr, NULL);
    return code;
}

/* ---------------------------------------------- */

static int get_from_names_table(gs_font_type42 *pfont, gs_font_info_t *info,
                           gs_const_string *pmember, int member, uint name_id)
{
    int (*string_proc)(gs_font_type42 *, ulong, uint, const byte **) =
        pfont->data.string_proc;
    byte t[12];
    ushort num_records, strings_offset, i, language_id = 0xffff, length0 = 0, offset0 = 0;
    int code;

    READ_SFNTS(pfont, pfont->data.name_offset + 2, 4, t);
    num_records = U16(t);
    strings_offset = U16(t + 2);
    for (i = 0; i < num_records; i++) {
        ushort platformID, specificID, languageID, nameID, length, offset;

        READ_SFNTS(pfont, pfont->data.name_offset + 6 + i * 12, 12, t);
        platformID = U16(t + 0);
        specificID = U16(t + 2);
        languageID = U16(t + 4);
        nameID = U16(t + 6);
        length = U16(t + 8);
        offset = U16(t + 10);
        if (nameID == name_id) {
            DISCARD(platformID);
            DISCARD(specificID);
            /* Hack : choose the minimal language id. */
            if (language_id > languageID) {
                language_id = languageID;
                length0 = length;
                offset0 = offset;
            }
        }
    }
    if (language_id == 0xffff)
        return 0;
    if ((*string_proc)(pfont, pfont->data.name_offset + strings_offset + offset0,
                                length0, &pmember->data) != 0)
        return_error(gs_error_invalidfont);
    pmember->size = length0;
    info->members |= member;
    return 0;
}

int
gs_truetype_font_info(gs_font *font, const gs_point *pscale, int members,
           gs_font_info_t *info)
{
    gs_font_type42 *pfont = (gs_font_type42 *)font;
    int code;

    if (!(info->members & FONT_INFO_EMBEDDING_RIGHTS) && (members & FONT_INFO_EMBEDDING_RIGHTS)) {
        if(pfont->data.os2_offset != 0) {
            unsigned char fstype[2];

            READ_SFNTS(pfont, pfont->data.os2_offset + 8, 2, fstype);
            info->EmbeddingRights = U16(fstype);
            info->members |= FONT_INFO_EMBEDDING_RIGHTS;
        }
    }
    if (pfont->data.name_offset == 0)
        return 0;
    if (!(info->members & FONT_INFO_COPYRIGHT) && (members & FONT_INFO_COPYRIGHT)) {
        /* One way we can arrive here is from gs_copy_font() -> z42_font_info(), in
         * that case we definitely want to copy the copyright informatoin if there is any,
         * but we don't want to throw an error if we find a Type 42 font which has no
         * copyright information. So get the informaton, but ignore the return code.
         * Bug #696174.
         */
        get_from_names_table(pfont, info, &info->Copyright, FONT_INFO_COPYRIGHT, 0);
    }
    if (!(info->members & FONT_INFO_FAMILY_NAME) && (members & FONT_INFO_FAMILY_NAME)) {
        code = get_from_names_table(pfont, info, &info->FamilyName, FONT_INFO_FAMILY_NAME, 1);
        if (code < 0)
            return code;
    }
    if (!(info->members & FONT_INFO_FULL_NAME) && (members & FONT_INFO_FULL_NAME)) {
        code = get_from_names_table(pfont, info, &info->FullName, FONT_INFO_FULL_NAME, 4);
        if (code < 0)
            return code;
    }

    return 0;
}

int
gs_type42_font_info(gs_font *font, const gs_point *pscale, int members,
           gs_font_info_t *info)
{
    int code = gs_default_font_info(font, pscale, members, info);

    if (code < 0)
        return code;
    return gs_truetype_font_info(font, pscale, members, info);
}

/******************** WOFF Support *************************/
static int
gs_woff_tabdir_compare (const void *a, const void *b)
{
    uint32_t poffs, qoffs;

    poffs = u32((*(byte **)a) + 4);
    qoffs = u32((*(byte **)b) + 4);

    return poffs > qoffs ? 1 : poffs < qoffs ? -1 : 0;
}

static stream *
gs_woff_push_flate_filter(stream *s)
{
    gs_memory_t *mem = s->memory;
    stream *fs, *ffs = NULL;
    byte *buf;
    stream_zlib_state *st;

    fs = s_alloc(mem, "gs_woff_push_flate_filter(fs)");
    buf = gs_alloc_bytes(mem, 4096, "gs_woff_push_flate_filter(buf)");
    st = gs_alloc_struct(mem, stream_zlib_state, s_zlibD_template.stype, "gs_woff_push_flate_filter(st)");
    if (fs == 0 || st == 0 || buf == 0) {
        gs_free_object(mem, fs, "gs_woff_push_flate_filter(fs)");
        gs_free_object(mem, buf, "gs_woff_push_flate_filter(buf)");
        gs_free_object(mem, st, "gs_woff_push_flate_filter(st)");
        goto done;
    }
    s_std_init(fs, buf, 4096, &s_filter_read_procs, s_mode_read);
    st->memory = mem;
    st->templat = &s_zlibD_template;
    fs->state = (stream_state *) st;
    fs->procs.process = s_zlibD_template.process;
    fs->strm = s;
    (*s_zlibD_template.set_defaults) ((stream_state *) st);
    (*s_zlibD_template.init) ((stream_state *) st);
    ffs = fs;
done:
    return ffs;
}

static stream *
gs_woff_pop_flate_filter(stream *s)
{
    gs_memory_t *mem = s->memory;
    stream *strm = s->strm;
    byte *buf = s->cbuf;

    sclose(s);
    gs_free_object(mem, s, "gs_woff_pop_flate_filter(s)");
    gs_free_object(mem, buf, "gs_woff_pop_flate_filter(buf)");

    return strm;
}

#define WOFFHDR_LEN 44
#define WOFFHDR_SFNT_FLAVOR_OFFS 4
#define WOFFHDR_SFNT_NUMTABS_OFFS 12
#define WOFFHDR_SFNT_LEN_OFFS 16

#define WOFF_TABDIR_ENT_LEN 20
#define WOFF_TABDIR_TAG_OFFS 0
#define WOFF_TABDIR_OFFSET_OFFS 4
#define WOFF_TABDIR_CLEN_OFFS 8
#define WOFF_TABDIR_OLEN_OFFS 12
#define WOFF_TABDIR_CSUM_OFFS 16

#define WOFF_TABDIR_FIELD_LEN 4

#define SFNT_TABDIR_ENT_LEN 16
#define SFNT_TABDIR_CSUM_OFFS 4
#define SFNT_TABDIR_OFFSET_OFFS 8
#define SFNT_TABDIR_LEN_OFFS 12


static int
gs_woff2sfnt(gs_memory_t *mem, stream *s, byte *outbuf, int *outbuflen)
{
    int code = 0;
    gs_offset_t start;
    uint32_t sfntlen, ntables, sr, es, rs, i;
    byte woffhdr[44], *tabbuf = NULL;
    byte **tabbufptrs = NULL;
    byte *obuf = outbuf, *tdir;

    if (!sseekable(s)) {
        code = gs_note_error(gs_error_ioerror);
        goto done;
    }
    start = stell(s);
    if (s->bsize < 44){
        code = gs_note_error(gs_error_invalidfont);
        goto done;
    }

    if ((code = sfread(woffhdr, WOFFHDR_LEN, 1, s)) < 0)
        goto done;

    if (memcmp(woffhdr, "wOFF", 4) != 0
     || memcmp(woffhdr + WOFFHDR_SFNT_FLAVOR_OFFS, "OTTO", 4) == 0) {
        (void)sseek(s, start);
        code = gs_note_error(gs_error_invalidfont);
        goto done;
    }
    sfntlen = u32(woffhdr + WOFFHDR_SFNT_LEN_OFFS);
    if (!outbuf || *outbuflen < sfntlen) {
        *outbuflen = sfntlen;
        (void)sseek(s, start);
        goto done;
    }
    ntables = U16(woffhdr + WOFFHDR_SFNT_NUMTABS_OFFS);
    memcpy(obuf, woffhdr + WOFFHDR_SFNT_FLAVOR_OFFS, 4);
    obuf += 4;
    memcpy(obuf, woffhdr + WOFFHDR_SFNT_NUMTABS_OFFS, 2);
    obuf += 2;
    sr = ntables;
    sr |= (sr >> 1);
    sr |= (sr >> 2);
    sr |= (sr >> 4);
    sr |= (sr >> 8);
    sr &= ~(sr >> 1);
    sr *= 16;
    PUTU16(obuf, (ushort)sr, 0)
    rs = ntables * 16 - sr;
    es = 0;
    while (sr > 16) {
        es++;
        sr >>= 1;
    }
    PUTU16(obuf, (ushort)es, 2)
    PUTU16(obuf, (ushort)rs, 4);
    obuf += 6;

    tabbuf = gs_alloc_bytes(mem, WOFF_TABDIR_ENT_LEN * ntables, "gs_woff2sfnt(tabbuf)");
    tabbufptrs = (byte **)gs_alloc_bytes(mem, (ntables + 1) * sizeof(byte *), "gs_woff2sfnt(tabbufptrs)");
    if (!tabbuf || !tabbufptrs) {
        code = gs_note_error(gs_error_VMerror);
        goto done;
    }
    if ((code = sfread(tabbuf, WOFF_TABDIR_ENT_LEN * ntables, 1, s)) < 0)
        goto done;

    for (i = 0; i < ntables; i++)
        tabbufptrs[i] = tabbuf + (i * WOFF_TABDIR_ENT_LEN);
    tabbufptrs[i] = NULL;
    qsort(tabbufptrs, ntables, sizeof(byte *), gs_woff_tabdir_compare);

    /* Two pointers into the output buffer: tdir and obuf.
     * tdir is the beginning of the current entry in the table directory
     * and obuf is the beginning of the current table.
     */
    tdir = obuf;
    obuf += (16 * ntables);
    for (i = 0; i < ntables; i++) {
        byte *tdirent = tabbufptrs[i];
        uint32_t len, clen, pad;
        len = u32(tdirent + WOFF_TABDIR_OLEN_OFFS);
        clen = u32(tdirent + WOFF_TABDIR_CLEN_OFFS);
        /* Build the sfnts table directory entry.
         * For those cases where encoded fields are the same in WOFF and SFNT,
         * we just copy the bytes over
         */
        memcpy(tdir, tdirent, WOFF_TABDIR_FIELD_LEN);
        memcpy(tdir + SFNT_TABDIR_CSUM_OFFS, tdirent + WOFF_TABDIR_CSUM_OFFS, WOFF_TABDIR_FIELD_LEN);
        memcpy(tdir + SFNT_TABDIR_LEN_OFFS, tdirent + WOFF_TABDIR_OLEN_OFFS, WOFF_TABDIR_FIELD_LEN);
        put_u32_msb(tdir, (uint32_t)(obuf - outbuf), SFNT_TABDIR_OFFSET_OFFS);
        tdir += SFNT_TABDIR_ENT_LEN;
        /* Now handle the actual table data */
        sseek(s, u32(tdirent + WOFF_TABDIR_OFFSET_OFFS));
        if (clen != len)
            s = gs_woff_push_flate_filter(s);
        code = sfread(obuf, 1, len, s);
        if (clen != len)
            s = gs_woff_pop_flate_filter(s);
        if (code < 0)
            goto done;
        obuf += len;
        /* TTF requires each table to start on a 4 byte boundary, and
         * padding to be zeroed bytes.
         */
        pad = ((len + 3) & ~3) - len;
        while (pad > 0) {
            *obuf = (byte)0;
            obuf++;
            pad--;
        }
    }
done:
    gs_free_object(mem, tabbuf, "gs_woff2sfnt(tabbuf)");
    gs_free_object(mem, tabbufptrs, "gs_woff2sfnt(tabbufptrs)");
    return code;
}

int
gs_woff2sfnt_stream(gs_memory_t *mem, stream *s, byte *outbuf, int *outbuflen)
{
    return gs_woff2sfnt(mem, s, outbuf, outbuflen);
}

int
gs_woff2sfnt_buffer(gs_memory_t *mem, byte *inbuf, int inbuflen, byte *outbuf, int *outbuflen)
{
    stream *sstrm;
    int code = 0;
    sstrm = file_alloc_stream(mem, "gs_woff2sfnt_buffer(buf stream)");
    if (sstrm) {
        sread_string(sstrm, inbuf, inbuflen);
        code = gs_woff2sfnt(mem,sstrm , outbuf, outbuflen);
        sclose(sstrm);
        gs_free_object(mem, sstrm, "gs_woff2sfnt_buffer(buf stream)");
    }
    else {
        code = gs_note_error(gs_error_VMerror);
    }
    return code;
}
