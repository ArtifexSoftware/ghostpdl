/* Copyright (C) 2001-2012 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134, San Rafael,
   CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

#include "gslt_font_int.h"

#include "gslt.h"
#include "gslt_font.h"

/*
 * Locate the 'cmap' table and count the number of subtables.
 */

int
gslt_load_sfnt_cmap(gslt_font_t *xf)
{
    byte *cmapdata;
    int offset, length;
    int nsubtables;

    offset = gslt_find_sfnt_table(xf, "cmap", &length);
    if (offset < 0)
        return -1;

    if (length < 4)
        return -1;

    cmapdata = xf->data + offset;

    nsubtables = u16(cmapdata + 2);
    if (nsubtables < 0)
        return -1;
    if (length < 4 + nsubtables * 8)
        return -1;

    xf->cmaptable = offset;
    xf->cmapsubcount = nsubtables;
    xf->cmapsubtable = 0;

    return 0;
}

/*
 * Return the number of cmap subtables.
 */

int
gslt_count_font_encodings(gslt_font_t *xf)
{
    return xf->cmapsubcount;
}

/*
 * Extract PlatformID and EncodingID for a cmap subtable.
 */

int
gslt_identify_font_encoding(gslt_font_t *xf, int idx, int *pid, int *eid)
{
    byte *cmapdata, *entry;
    if (idx < 0 || idx >= xf->cmapsubcount)
        return -1;
        cmapdata = xf->data + xf->cmaptable;
    entry = cmapdata + 4 + idx * 8;
    *pid = u16(entry + 0);
    *eid = u16(entry + 2);
    return 0;
}

/*
 * Select a cmap subtable for use with encoding functions.
 */

int
gslt_select_font_encoding(gslt_font_t *xf, int idx)
{
    byte *cmapdata, *entry;
    int pid, eid;
    if (idx < 0 || idx >= xf->cmapsubcount)
        return -1;
    cmapdata = xf->data + xf->cmaptable;
    entry = cmapdata + 4 + idx * 8;
    pid = u16(entry + 0);
    eid = u16(entry + 2);
    xf->cmapsubtable = xf->cmaptable + u32(entry + 4);
    xf->usepua = (pid == 3 && eid == 0);
    return 0;
}

/*
 * Encode a character using the selected cmap subtable.
 * TODO: extend this to cover more cmap formats.
 */

static int
gslt_encode_font_char_int(gslt_font_t *xf, int code)
{
    byte *table;

    /* no cmap selected: return identity */
    if (xf->cmapsubtable <= 0)
        return code;

    table = xf->data + xf->cmapsubtable;

    switch (u16(table))
    {
    case 0: /* Apple standard 1-to-1 mapping. */
        return table[code + 6];

    case 4: /* Microsoft/Adobe segmented mapping. */
        {
            int segCount2 = u16(table + 6);
            byte *endCount = table + 14;
            byte *startCount = endCount + segCount2 + 2;
            byte *idDelta = startCount + segCount2;
            byte *idRangeOffset = idDelta + segCount2;
            int i2;

            for (i2 = 0; i2 < segCount2 - 3; i2 += 2)
            {
                int delta, roff;
                int start = u16(startCount + i2);
                int glyph;

                if ( code < start )
                    return 0;
                if ( code > u16(endCount + i2) )
                    continue;
                delta = s16(idDelta + i2);
                roff = s16(idRangeOffset + i2);
                if ( roff == 0 )
                {
                    return ( code + delta ) & 0xffff; /* mod 65536 */
                    return 0;
                }
                glyph = u16(idRangeOffset + i2 + roff + ((code - start) << 1));
                return (glyph == 0 ? 0 : glyph + delta);
            }

            /*
             * The TrueType documentation says that the last range is
             * always supposed to end with 0xffff, so this shouldn't
             * happen; however, in some real fonts, it does.
             */
            return 0;
        }

    case 6: /* Single interval lookup. */
        {
            int firstCode = u16(table + 6);
            int entryCount = u16(table + 8);
            if ( code < firstCode || code >= firstCode + entryCount )
                return 0;
            return u16(table + 10 + ((code - firstCode) << 1));
        }

    case 10: /* Trimmed array (like 6) */
        {
            int startCharCode = u32(table + 12);
            int numChars = u32(table + 16);
            if ( code < startCharCode || code >= startCharCode + numChars )
                return 0;
            return u32(table + 20 + (code - startCharCode) * 4);
        }

    case 12: /* Segmented coverage. (like 4) */
        {
            int nGroups = u32(table + 12);
            byte *group = table + 16;
            int i;

            for (i = 0; i < nGroups; i++)
            {
                int startCharCode = u32(group + 0);
                int endCharCode = u32(group + 4);
                int startGlyphID = u32(group + 8);
                if ( code < startCharCode )
                    return 0;
                if ( code <= endCharCode )
                    return startGlyphID + (code - startCharCode);
                group += 12;
            }

            return 0;
        }

    case 2: /* High-byte mapping through table. */
    case 8: /* Mixed 16-bit and 32-bit coverage (like 2) */
    default:
        errprintf("error: unknown cmap format: %d\n", u16(table));
        return 0;
    }

    return 0;
}

int
gslt_encode_font_char(gslt_font_t *xf, int code)
{
    int gid = gslt_encode_font_char_int(xf, code);
    if (gid == 0 && xf->usepua)
        gid = gslt_encode_font_char_int(xf, 0xF000 | code);
    return gid;
}
