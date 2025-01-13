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


/* Font API client */

#include "stdlib.h"             /* abs() */

#include "memory_.h"
#include "math_.h"
#include "stat_.h"              /* include before definition of esp macro, bug 691123 */
#include "string_.h"
#include "ghost.h"
#include "gp.h"
#include "oper.h"
#include "gxdevice.h"
#include "gxfont.h"
#include "gxfont1.h"
#include "gxchar.h"
#include "gzpath.h"
#include "gxpath.h"
#include "gxfcache.h"
#include "gxchrout.h"
#include "gximask.h"
#include "gscoord.h"
#include "gspaint.h"
#include "gsfont.h"
#include "gspath.h"
#include "bfont.h"
#include "dstack.h"
#include "estack.h"
#include "ichar.h"
#include "idict.h"
#include "iname.h"
#include "ifont.h"
#include "icid.h"
#include "igstate.h"
#include "icharout.h"

#include "ifapi.h"
#include "iplugin.h"
#include "store.h"
#include "gzstate.h"
/* #include "gdevpsf.h" */
#include "stream.h"             /* for files.h */
#include "gscrypt1.h"
#include "gxfcid.h"
#include "gsstype.h"
#include "gxchar.h"             /* for st_gs_show_enum */
#include "ipacked.h"            /* for packed_next */
#include "iddict.h"
#include "ifont42.h"            /* for string_array_access_proc */
#include "gdebug.h"
#include "gsimage.h"
#include "gxcldev.h"
#include "gxdevmem.h"

#include "gxfapi.h"

/* -------------------------------------------------------- */

typedef struct sfnts_reader_s sfnts_reader;
struct sfnts_reader_s
{
    ref *sfnts;
    const gs_memory_t *memory;
    const byte *p;
    long index;
    uint offset;
    uint length;
    int error;
         byte(*rbyte) (sfnts_reader *r);
         ushort(*rword) (sfnts_reader *r);
         ulong(*rlong) (sfnts_reader *r);
    int (*rstring) (sfnts_reader *r, byte *v, int length);
    void (*seek) (sfnts_reader *r, ulong pos);
};

static void
sfnts_next_elem(sfnts_reader *r)
{
    ref s;
    int code;

    if (r->error < 0)
        return;
    do {
    	r->index++;
    	code = array_get(r->memory, r->sfnts, r->index, &s);
        if (code < 0) {
            r->error = code;
            return;
        }
        if (!r_has_type(&s, t_string)) {
            r->error = gs_note_error(gs_error_typecheck);
            return;
        }
    	r->p = s.value.const_bytes;
    	r->length = r_size(&s) & ~(uint) 1; /* See Adobe Technical Note # 5012, section 4.2. */
    } while (r->length == 0);
    r->offset = 0;
}

static inline byte
sfnts_reader_rbyte_inline(sfnts_reader *r)
{
    if (r->offset >= r->length)
        sfnts_next_elem(r);
    return ((r->error < 0) ? 0 : r->p[r->offset++]);
}

static byte
sfnts_reader_rbyte(sfnts_reader *r)
{                               /* old compiler compatibility */
    return (sfnts_reader_rbyte_inline(r));
}

#define SFNTS_READER_RBYTE_TO_USHORT(r) ((ulong)sfnts_reader_rbyte_inline(r))

static ushort
sfnts_reader_rword(sfnts_reader *r)
{
    ushort retval;

    retval = SFNTS_READER_RBYTE_TO_USHORT(r) << 8;
    retval += SFNTS_READER_RBYTE_TO_USHORT(r);

    return retval;
}

#undef SFNTS_READER_RBYTE_TO_USHORT

#define SFNTS_READER_RBYTE_TO_ULONG(r) ((ulong)sfnts_reader_rbyte_inline(r))

static ulong
sfnts_reader_rlong(sfnts_reader *r)
{
    ulong retval;
    retval = SFNTS_READER_RBYTE_TO_ULONG(r) << 24;
    retval += SFNTS_READER_RBYTE_TO_ULONG(r) << 16;
    retval += SFNTS_READER_RBYTE_TO_ULONG(r) << 8;
    retval += SFNTS_READER_RBYTE_TO_ULONG(r);
    return retval;
}

#undef SFNTS_READER_RWORD_TO_LONG

static int
sfnts_reader_rstring(sfnts_reader *r, byte *v, int length)
{
    int rlength = length;

    if (length <= 0)
        return (0);
    while (r->error >= 0) {
        int l = min(length, r->length - r->offset);

        memcpy(v, r->p + r->offset, l);
        length -= l;
        r->offset += l;
        if (length <= 0)
            return (rlength);
        v += l;
        sfnts_next_elem(r);
    }
    return (rlength - length);
}

static void
sfnts_reader_seek(sfnts_reader *r, ulong pos)
{                               /* fixme : optimize */
    ulong skipped = 0;

    r->index = -1;
    sfnts_next_elem(r);
    while (skipped + r->length < pos && r->error >= 0) {
        skipped += r->length;
        sfnts_next_elem(r);
    }
    r->offset = pos - skipped;
}

static void
sfnts_reader_init(const gs_memory_t *mem, sfnts_reader *r, ref *pdr)
{
    r->memory = mem;
    r->rbyte = sfnts_reader_rbyte;
    r->rword = sfnts_reader_rword;
    r->rlong = sfnts_reader_rlong;
    r->rstring = sfnts_reader_rstring;
    r->seek = sfnts_reader_seek;
    r->index = -1;
    r->error = 0;
    if (r_type(pdr) != t_dictionary ||
        dict_find_string(pdr, "sfnts", &r->sfnts) <= 0)
        r->error = gs_error_undefined;
    sfnts_next_elem(r);
}

/* -------------------------------------------------------- */

typedef struct sfnts_writer_s sfnts_writer;
struct sfnts_writer_s
{
    byte *buf, *p;
    int buf_size;
    void (*wbyte) (sfnts_writer *w, byte v);
    void (*wword) (sfnts_writer *w, ushort v);
    void (*wlong) (sfnts_writer *w, ulong v);
    void (*wstring) (sfnts_writer *w, byte *v, int length);
};

static void
sfnts_writer_wbyte(sfnts_writer *w, byte v)
{
    if (w->buf + w->buf_size < w->p + 1)
        return;                 /* safety */
    w->p[0] = v;
    w->p++;
}

static void
sfnts_writer_wword(sfnts_writer *w, ushort v)
{
    if (w->buf + w->buf_size < w->p + 2)
        return;                 /* safety */
    w->p[0] = v / 256;
    w->p[1] = v % 256;
    w->p += 2;
}

static void
sfnts_writer_wlong(sfnts_writer *w, ulong v)
{
    if (w->buf + w->buf_size < w->p + 4)
        return;                 /* safety */
    w->p[0] = v >> 24;
    w->p[1] = (v >> 16) & 0xFF;
    w->p[2] = (v >> 8) & 0xFF;
    w->p[3] = v & 0xFF;
    w->p += 4;
}

static void
sfnts_writer_wstring(sfnts_writer *w, byte *v, int length)
{
    if (w->buf + w->buf_size < w->p + length)
        return;                 /* safety */
    memcpy(w->p, v, length);
    w->p += length;
}

static const sfnts_writer sfnts_writer_stub = {
    0, 0, 0,
    sfnts_writer_wbyte,
    sfnts_writer_wword,
    sfnts_writer_wlong,
    sfnts_writer_wstring
};

/* -------------------------------------------------------- */

static inline bool
sfnts_need_copy_table(byte *tag)
{
    return (memcmp(tag, "glyf", 4) && memcmp(tag, "glyx", 4) && /* Presents in files created by AdobePS5.dll Version 5.1.2 */
            memcmp(tag, "loca", 4) && memcmp(tag, "locx", 4) && /* Presents in files created by AdobePS5.dll Version 5.1.2 */
            memcmp(tag, "cmap", 4));
}

static void
sfnt_copy_table(sfnts_reader *r, sfnts_writer *w, int length)
{
    byte buf[1024];

    while (length > 0 && r->error >= 0) {
        int l = min(length, sizeof(buf));

        (void)r->rstring(r, buf, l);
        w->wstring(w, buf, l);
        length -= l;
    }
}

static int
sfnts_copy_except_glyf(sfnts_reader *r, sfnts_writer *w)
{                               /* Note : TTC is not supported and probably is unuseful for Type 42. */
    /* This skips glyf, loca and cmap from copying. */
    struct
    {
        byte tag[4];
        ulong checkSum, offset, offset_new, length;
    } tables[40];
    const ushort alignment = 4; /* Not sure, maybe 2 */
    ulong version = r->rlong(r);
    ushort num_tables = r->rword(r);
    ushort i, num_tables_new = 0;
    ushort searchRange, entrySelector = 0, rangeShift, v;
    ulong size_new = 12;

    if (num_tables > countof(tables)) {
        r->error = gs_note_error(gs_error_invalidfont);
        return r->error;
    }

    r->rword(r);                /* searchRange */
    if (r->error < 0)
        return r->error;

    r->rword(r);                /* entrySelector */
    if (r->error < 0)
        return r->error;

    r->rword(r);                /* rangeShift */
    if (r->error < 0)
        return r->error;

    for (i = 0; i < num_tables && r->error >= 0; i++) {
        (void)r->rstring(r, tables[i].tag, 4);
        if (r->error < 0)
            continue;
        tables[i].checkSum = r->rlong(r);
        tables[i].offset = r->rlong(r);
        tables[i].length = r->rlong(r);
        tables[i].offset_new = size_new;
        if (sfnts_need_copy_table(tables[i].tag)) {
            num_tables_new++;
            size_new +=
                (tables[i].length + alignment - 1) / alignment * alignment;
        }
    }
    if (r->error < 0)
        return r->error;
    size_new += num_tables_new * 16;
    if (w == 0) {
        return size_new;
    }
    searchRange = v = num_tables_new * 16;
    for (i = 0; v; i++) {
        v >>= 1;
        searchRange |= v;
        entrySelector++;
    }
    searchRange -= searchRange >> 1;
    rangeShift = num_tables_new * 16 - searchRange;

    w->wlong(w, version);
    w->wword(w, num_tables_new);
    w->wword(w, searchRange);
    w->wword(w, entrySelector);
    w->wword(w, rangeShift);
    for (i = 0; i < num_tables; i++) {
        if (sfnts_need_copy_table(tables[i].tag)) {
            w->wstring(w, tables[i].tag, 4);
            w->wlong(w, tables[i].checkSum);
            w->wlong(w, tables[i].offset_new + num_tables_new * 16);
            w->wlong(w, tables[i].length);
        }
    }
    for (i = 0; i < num_tables && r->error >= 0; i++) {
        if (sfnts_need_copy_table(tables[i].tag)) {
            int k = tables[i].length;

            r->seek(r, tables[i].offset);

            if (w->p - w->buf != tables[i].offset_new + num_tables_new * 16) {
                r->error = gs_error_invalidfont;       /* the algorithm consistency check */
                continue;
            }
            sfnt_copy_table(r, w, tables[i].length);
            for (; k & (alignment - 1); k++)
                w->wbyte(w, 0);
        }
    }
    if (r->error < 0)
        return r->error;

    return (size_new);
}

static int
true_type_size(const gs_memory_t *mem, ref *pdr, unsigned long int *length)
{
    sfnts_reader r;

    sfnts_reader_init(mem, &r, pdr);
    *length = sfnts_copy_except_glyf(&r, 0);

    return r.error;
}

static int
FAPI_FF_serialize_tt_font(gs_fapi_font *ff, void *buf, int buf_size)
{
    ref *pdr = pfont_dict(((gs_font_base *) ff->client_font_data2));
    sfnts_reader r;
    sfnts_writer w = sfnts_writer_stub;

    w.buf_size = buf_size;
    w.buf = w.p = buf;
    sfnts_reader_init(ff->memory, &r, pdr);
    return sfnts_copy_except_glyf(&r, &w);
}

static inline ushort
float_to_ushort(float v)
{
    return ((ushort) (v * 16)); /* fixme : the scale may depend on renderer */
}

/* In general, we assumed that the entries we use below have been validated (at least for type)
 * at definefont time. This means validating each entry only once, rather than on every read
 * here. Better, for example, for BlendDesignMap which is an array, of arrays, of arrays of
 * numbers.
 */
static int
FAPI_FF_get_word(gs_fapi_font *ff, gs_fapi_font_feature var_id, int index, unsigned short *ret)
{
    gs_font_type1 *pfont = (gs_font_type1 *) ff->client_font_data;
    ref *pdr = pfont_dict(((gs_font_base *) ff->client_font_data2));
    int code = 0;

    switch ((int)var_id) {
        case gs_fapi_font_feature_Weight:
            *ret = 0;           /* wrong */
            break;
        case gs_fapi_font_feature_ItalicAngle:
            *ret = 0;           /* wrong */
            break;
        case gs_fapi_font_feature_IsFixedPitch:
            *ret = 0;           /* wrong */
            break;
        case gs_fapi_font_feature_UnderLinePosition:
            *ret = 0;           /* wrong */
            break;
        case gs_fapi_font_feature_UnderlineThickness:
            *ret = 0;           /* wrong */
            break;
        case gs_fapi_font_feature_FontType:
            *ret = (pfont->FontType == 2 ? 2 : 1);
            break;
        case gs_fapi_font_feature_FontBBox:
            switch (index) {
                case 0:
                    *ret = ((ushort) pfont->FontBBox.p.x);
                    break;
                case 1:
                    *ret = ((ushort) pfont->FontBBox.p.y);
                    break;
                case 2:
                    *ret = ((ushort) pfont->FontBBox.q.x);
                    break;
                case 3:
                    *ret = ((ushort) pfont->FontBBox.q.y);
                    break;
                default:
                    code = gs_note_error(gs_error_rangecheck);
            }
            break;
        case gs_fapi_font_feature_BlueValues_count:
            *ret = pfont->data.BlueValues.count;
            break;
        case gs_fapi_font_feature_BlueValues:
            *ret =  (float_to_ushort(pfont->data.BlueValues.values[index]));
            break;
        case gs_fapi_font_feature_OtherBlues_count:
            *ret = pfont->data.OtherBlues.count;
            break;
        case gs_fapi_font_feature_OtherBlues:
            *ret = (float_to_ushort(pfont->data.OtherBlues.values[index]));
            break;
        case gs_fapi_font_feature_FamilyBlues_count:
            *ret = pfont->data.FamilyBlues.count;
            break;
        case gs_fapi_font_feature_FamilyBlues:
            *ret = (float_to_ushort(pfont->data.FamilyBlues.values[index]));
            break;
        case gs_fapi_font_feature_FamilyOtherBlues_count:
            *ret = pfont->data.FamilyOtherBlues.count;
            break;
        case gs_fapi_font_feature_FamilyOtherBlues:
            *ret = (float_to_ushort(pfont->data.FamilyOtherBlues.values[index]));
            break;
        case gs_fapi_font_feature_BlueShift:
            *ret = float_to_ushort(pfont->data.BlueShift);
            break;
        case gs_fapi_font_feature_BlueFuzz:
            *ret = float_to_ushort(pfont->data.BlueShift);
            break;
        case gs_fapi_font_feature_StdHW:
            *ret = (pfont->data.StdHW.count == 0 ? 0 : float_to_ushort(pfont->data.StdHW.values[0]));   /* UFST bug ? */
            break;
        case gs_fapi_font_feature_StdVW:
            *ret = (pfont->data.StdVW.count == 0 ? 0 : float_to_ushort(pfont->data.StdVW.values[0]));   /* UFST bug ? */
            break;
        case gs_fapi_font_feature_StemSnapH_count:
            *ret = pfont->data.StemSnapH.count;
            break;
        case gs_fapi_font_feature_StemSnapH:
            *ret = float_to_ushort(pfont->data.StemSnapH.values[index]);
            break;
        case gs_fapi_font_feature_StemSnapV_count:
            *ret = pfont->data.StemSnapV.count;
            break;
        case gs_fapi_font_feature_StemSnapV:
            *ret = float_to_ushort(pfont->data.StemSnapV.values[index]);
            break;
        case gs_fapi_font_feature_ForceBold:
            *ret = pfont->data.ForceBold;
            break;
        case gs_fapi_font_feature_LanguageGroup:
            *ret = pfont->data.LanguageGroup;
            break;
        case gs_fapi_font_feature_lenIV:
            *ret = ff->need_decrypt ? 0 : pfont->data.lenIV;
            break;
        case gs_fapi_font_feature_GlobalSubrs_count:
            {
                ref *Private, *GlobalSubrs;

                if (pfont->FontType == ft_encrypted2) {
                    if (dict_find_string(pdr, "Private", &Private) <= 0) {
                        *ret = 0;
                        break;
                    }
                    if (dict_find_string(Private, "GlobalSubrs", &GlobalSubrs) <= 0) {
                        *ret = 0;
                        break;
                    }
                    *ret = r_size(GlobalSubrs);
                    break;
                }
                *ret = 0;
                break;
            }
        case gs_fapi_font_feature_Subrs_count:
            {
                ref *Private, *Subrs;

                if (dict_find_string(pdr, "Private", &Private) <= 0) {
                    *ret = 0;
                    break;
                }
                if (dict_find_string(Private, "Subrs", &Subrs) <= 0) {
                    *ret = 0;
                    break;
                }
                *ret = r_size(Subrs);
                break;
            }
        case gs_fapi_font_feature_CharStrings_count:
            {
                ref *CharStrings;

                if (dict_find_string(pdr, "CharStrings", &CharStrings) <= 0)
                    *ret = 0;
                else
                    *ret = dict_maxlength(CharStrings);
                break;
            }
            /* Multiple Master specific */
        case gs_fapi_font_feature_DollarBlend:
            {
                ref *DBlend;

                if (dict_find_string(pdr, "$Blend", &DBlend) <= 0)
                    *ret = 0;
                else
                    *ret = 1;
                break;
            }
        case gs_fapi_font_feature_BlendAxisTypes_count:
            {
                ref *Info, *Axes;

                if (dict_find_string(pdr, "FontInfo", &Info) <= 0) {
                    *ret = 0;
                    break;
                }
                if (dict_find_string(Info, "BlendAxisTypes", &Axes) <= 0) {
                    *ret = 0;
                    break;
                }
                *ret = r_size(Axes);
                break;
            }
        case gs_fapi_font_feature_BlendFontInfo_count:
            {
                ref *Info, *FontInfo;

                if (dict_find_string(pdr, "Blend", &Info) <= 0) {
                    *ret = 0;
                    break;
                }
                if (dict_find_string(Info, "FontInfo", &FontInfo) <= 0) {
                    *ret = 0;
                    break;
                }
                *ret = dict_length(FontInfo);
                break;
            }
        case gs_fapi_font_feature_BlendPrivate_count:
            {
                ref *Info, *Private;

                if (dict_find_string(pdr, "Blend", &Info) <= 0) {
                    *ret = 0;
                    break;
                }
                if (dict_find_string(Info, "Private", &Private) <= 0) {
                    *ret = 0;
                    break;
                }
                *ret = dict_length(Private);
                break;
            }
        case gs_fapi_font_feature_WeightVector_count:
            {
                *ret = pfont->data.WeightVector.count;
                break;
            }
        case gs_fapi_font_feature_BlendDesignPositionsArrays_count:
            {
                ref *Info, *Array;

                if (dict_find_string(pdr, "FontInfo", &Info) <= 0) {
                    *ret = 0;
                    break;
                }
                if (dict_find_string(Info, "BlendDesignPositions", &Array) <= 0) {
                    *ret = 0;
                    break;
                }
                *ret = r_size(Array);
                break;
            }
        case gs_fapi_font_feature_BlendDesignMapArrays_count:
            {
                ref *Info, *Array;

                if (dict_find_string(pdr, "FontInfo", &Info) <= 0) {
                    *ret = 0;
                    break;
                }
                if (dict_find_string(Info, "BlendDesignMap", &Array) <= 0) {
                    *ret = 0;
                    break;
                }
                *ret = r_size(Array);
                break;
            }
        case gs_fapi_font_feature_BlendDesignMapSubArrays_count:
            {
                ref *Info, *Array, SubArray;

                if (dict_find_string(pdr, "FontInfo", &Info) <= 0) {
                    *ret = 0;
                    break;
                }
                if (dict_find_string(Info, "BlendDesignMap", &Array) <= 0) {
                    *ret = 0;
                    break;
                }
                if (array_get(ff->memory, Array, index, &SubArray) < 0) {
                    *ret = 0;
                    break;
                }
                *ret = r_size(&SubArray);
                break;
            }
        case gs_fapi_font_feature_DollarBlend_length:
            {
                ref *DBlend, Element, string;
                int i, length = 0;
                char Buffer[32];

                if (dict_find_string(pdr, "$Blend", &DBlend) <= 0) {
                    *ret = 0;
                    break;
                }
                for (i = 0; i < r_size(DBlend); i++) {
                    /* When reading the real proc, we add a space between each entry */
                    length++;
                    if (array_get(ff->memory, DBlend, i, &Element) < 0) {
                        length = 0;
                        break;
                    }
                    switch (r_btype(&Element)) {
                        case t_name:
                            name_string_ref(ff->memory, &Element, &string);
                            length += r_size(&string);
                            break;
                        case t_real:
                            gs_snprintf(Buffer, sizeof(Buffer), "%f", Element.value.realval);
                            length += strlen(Buffer);
                            break;
                        case t_integer:
                            gs_snprintf(Buffer, sizeof(Buffer), "%"PRIpsint, Element.value.intval);
                            length += strlen(Buffer);
                            break;
                        case t_operator:
                            {
                                op_def const *op;

                                op = op_index_def(r_size(&Element));
                                length += strlen(op->oname + 1);
                            }
                            break;
                        default:
                            break;
                    }

                    if (length > max_ushort) {
                        length = 0;
                        break;
                    }
                 }
                *ret = length;
                break;
            }
        case gs_fapi_font_feature_BlendFontBBox_length:
            {
                ref *Blend, *bfbbox, bfbbox0;
                if (dict_find_string(pdr, "Blend", &Blend) <= 0) {
                    *ret = 0;
                    break;
                }

                if (dict_find_string(Blend, "FontBBox", &bfbbox) <= 0) {
                    *ret = 0;
                    break;
                }
                if (!r_has_type(bfbbox, t_array) || array_get(ff->memory, bfbbox, 0, &bfbbox0) < 0) {
                    *ret = 0;
                    break;
                }
                *ret = (ushort)r_size(&bfbbox0);
                break;
            }
        case gs_fapi_font_feature_BlendFontBBox:
            {
                ref *Blend, *bfbbox, subbfbbox, val;
                int aind, ind;
                if (dict_find_string(pdr, "Blend", &Blend) <= 0) {
                    *ret = 0;
                    break;
                }
                if (dict_find_string(Blend, "FontBBox", &bfbbox) <= 0) {
                    *ret = 0;
                    break;
                }
                ind = index % 4;
                aind = (index - ind) /4;
                if (array_get(ff->memory, bfbbox, aind, &subbfbbox) < 0) {
                    *ret = 0;
                    break;
                }
                if (array_get(ff->memory, &subbfbbox, ind, &val) < 0) {
                    *ret = 0;
                    break;
                }

                *ret = (ushort)val.value.intval;
                break;
            }
        case gs_fapi_font_feature_BlendBlueValues_length:
            {
                ref *Priv, *Blend, *bbv;
                if (dict_find_string(pdr, "Blend", &Blend) <= 0) {
                    *ret = 0;
                    break;
                }
                if (dict_find_string(Blend, "Private", &Priv) <= 0) {
                    *ret = 0;
                    break;
                }
                if (dict_find_string(Priv, "BlueValues", &bbv) <= 0) {
                    *ret = 0;
                    break;
                }
                *ret = (ushort)r_size(bbv);
                break;
            }
        case gs_fapi_font_feature_BlendBlueValues_count:
            {
                ref *Priv, *Blend, *bbv, sub;

                if (dict_find_string(pdr, "Blend", &Blend) <= 0) {
                    *ret = 0;
                    break;
                }
                if (dict_find_string(Blend, "Private", &Priv) <= 0) {
                    *ret = 0;
                    break;
                }
                if (dict_find_string(Priv, "BlueValues", &bbv) <= 0) {
                    *ret = 0;
                    break;
                }
                if (array_get(ff->memory, bbv, index, &sub) < 0) {
                    *ret = 0;
                    break;
                }
                *ret = (ushort)r_size(&sub);
                break;
            }
        case gs_fapi_font_feature_BlendBlueValues:
            {
                ref *Priv, *Blend, *bbv, sub, r;
                int aind = 0;
                if (dict_find_string(pdr, "Blend", &Blend) <= 0) {
                    *ret = 0;
                    break;
                }
                if (dict_find_string(Blend, "Private", &Priv) <= 0) {
                    *ret = 0;
                    break;
                }
                if (dict_find_string(Priv, "BlueValues", &bbv) <= 0) {
                    *ret = 0;
                    break;
                }

                while (1) {
                    if ((code = array_get(ff->memory, bbv, aind++, &sub)) < 0) {
                        *ret = 0;
                        break;
                    }
                    if (index - (int)r_size(&sub) < 0) {
                        break;
                    }
                    index -= r_size(&sub);
                }
                if (code < 0)
                    break;

                if (array_get(ff->memory, &sub, index, &r) < 0) {
                    *ret = 0;
                    break;
                }

                *ret = (ushort)r.value.intval;
                break;
            }
        case gs_fapi_font_feature_BlendOtherBlues_length:
            {
                ref *Priv, *Blend, *bob;
                if (dict_find_string(pdr, "Blend", &Blend) <= 0) {
                    *ret = 0;
                    break;
                }
                if (dict_find_string(Blend, "Private", &Priv) <= 0) {
                    *ret = 0;
                    break;
                }
                if (dict_find_string(Priv, "OtherBlues", &bob) <= 0) {
                    *ret = 0;
                    break;
                }
                *ret = (ushort)r_size(bob);
                break;
            }
        case gs_fapi_font_feature_BlendOtherBlues_count:
            {
                ref *Priv, *Blend, *bob, sub;
                if (dict_find_string(pdr, "Blend", &Blend) <= 0) {
                    *ret = 0;
                    break;
                }
                if (dict_find_string(Blend, "Private", &Priv) <= 0) {
                    *ret = 0;
                    break;
                }
                if (dict_find_string(Priv, "OtherBlues", &bob) <= 0) {
                    *ret = 0;
                    break;
                }

                 if (array_get(ff->memory, bob, index, &sub) < 0) {
                    *ret = 0;
                    break;
                }
                *ret = (ushort)r_size(&sub);
                break;
            }
        case gs_fapi_font_feature_BlendOtherBlues:
            {
                ref *Priv, *Blend, *bob, sub, r;
                int aind = 0;
                if (dict_find_string(pdr, "Blend", &Blend) <= 0) {
                    *ret = 0;
                    break;
                }
                if (dict_find_string(Blend, "Private", &Priv) <= 0) {
                    *ret = 0;
                    break;
                }
                if (dict_find_string(Priv, "OtherBlues", &bob) <= 0) {
                    *ret = 0;
                    break;
                }

                while (1) {
                    if ((code = array_get(ff->memory, bob, aind++, &sub)) < 0)
                        break;
                    if (index - (int)r_size(&sub) < 0) {
                        break;
                    }
                    index -= r_size(&sub);
                }
                if (code < 0) {
                    *ret = 0;
                    break;
                }
                if (array_get(ff->memory, &sub, index, &r) < 0) {
                    *ret = 0;
                    break;
                }

                *ret = (ushort)r.value.intval;
                break;
            }
        case gs_fapi_font_feature_BlendBlueScale_count:
            {
                ref *Priv, *Blend, *bbs;
                if (dict_find_string(pdr, "Blend", &Blend) <= 0) {
                    *ret = 0;
                    break;
                }
                if (dict_find_string(Blend, "Private", &Priv) <= 0) {
                    *ret = 0;
                    break;
                }
                if (dict_find_string(Priv, "BlueScale", &bbs) <= 0) {
                    *ret = 0;
                    break;
                }
                *ret = (ushort)r_size(bbs);
                break;
            }
        case gs_fapi_font_feature_BlendBlueShift_count:
            {
                ref *Priv, *Blend, *bbs;
                if (dict_find_string(pdr, "Blend", &Blend) <= 0) {
                    *ret = 0;
                    break;
                }
                if (dict_find_string(Blend, "Private", &Priv) <= 0) {
                    *ret = 0;
                    break;
                }
                if (dict_find_string(Priv, "BlueShift", &bbs) <= 0) {
                    *ret = 0;
                    break;
                }
                *ret = (ushort)r_size(bbs);
                break;
            }
        case gs_fapi_font_feature_BlendBlueShift:
            {
                ref *Priv, *Blend, *bbs, r;
                if (dict_find_string(pdr, "Blend", &Blend) <= 0) {
                    *ret = 0;
                    break;
                }
                if (dict_find_string(Blend, "Private", &Priv) <= 0) {
                    *ret = 0;
                    break;
                }
                if (dict_find_string(Priv, "BlueShift", &bbs) <= 0) {
                    *ret = 0;
                    break;
                }
                if (array_get(ff->memory, bbs, index, &r) < 0) {
                    *ret = 0;
                    break;
                }

                *ret = (ushort)r.value.intval;
                break;
            }
        case gs_fapi_font_feature_BlendBlueFuzz_count:
            {
                ref *Priv, *Blend, *bbf;
                if (dict_find_string(pdr, "Blend", &Blend) <= 0) {
                    *ret = 0;
                    break;
                }
                if (dict_find_string(Blend, "Private", &Priv) <= 0) {
                    *ret = 0;
                    break;
                }
                if (dict_find_string(Priv, "BlueFuzz", &bbf) <= 0) {
                    *ret = 0;
                    break;
                }
                *ret = (ushort)r_size(bbf);
                break;
            }
        case gs_fapi_font_feature_BlendBlueFuzz:
            {
                ref *Priv, *Blend, *bbf, r;
                if (dict_find_string(pdr, "Blend", &Blend) <= 0) {
                    *ret = 0;
                    break;
                }
                if (dict_find_string(Blend, "Private", &Priv) <= 0) {
                    *ret = 0;
                    break;
                }
                if (dict_find_string(Priv, "BlueFuzz", &bbf) <= 0) {
                    *ret = 0;
                    break;
                }
                if (array_get(ff->memory, bbf, index, &r) < 0) {
                    *ret = 0;
                    break;
                }

                *ret = (ushort)r.value.intval;
                break;
            }
        case gs_fapi_font_feature_BlendForceBold_count:
            {
                ref *Priv, *Blend, *bfb;
                if (dict_find_string(pdr, "Blend", &Blend) <= 0) {
                    *ret = 0;
                    break;
                }
                if (dict_find_string(Blend, "Private", &Priv) <= 0) {
                    *ret = 0;
                    break;
                }
                if (dict_find_string(Priv, "ForceBold", &bfb) <= 0) {
                    *ret = 0;
                    break;
                }
                *ret = (ushort)r_size(bfb);
                break;
            }
        case gs_fapi_font_feature_BlendForceBold:
            {
                ref *Priv, *Blend, *bfb, r;
                if (dict_find_string(pdr, "Blend", &Blend) <= 0) {
                    *ret = 0;
                    break;
                }
                if (dict_find_string(Blend, "Private", &Priv) <= 0) {
                    *ret = 0;
                    break;
                }
                if (dict_find_string(Priv, "BlueFuzz", &bfb) <= 0) {
                    *ret = 0;
                    break;
                }
                if (array_get(ff->memory, bfb, index, &r) < 0) {
                    *ret = 0;
                    break;
                }

                *ret = (ushort)r.value.boolval;
            }
        case gs_fapi_font_feature_BlendStdHW_length:
            {
                ref *Priv, *Blend, *stdhw;
                if (dict_find_string(pdr, "Blend", &Blend) <= 0) {
                    *ret = 0;
                    break;
                }
                if (dict_find_string(Blend, "Private", &Priv) <= 0) {
                    *ret = 0;
                    break;
                }
                if (dict_find_string(Priv, "StdHW", &stdhw) <= 0) {
                    *ret = 0;
                    break;
                }
                *ret = (ushort)r_size(stdhw);
                break;
            }
        case gs_fapi_font_feature_BlendStdHW_count:
            {
                ref *Priv, *Blend, *stdhw, sub;
                if (dict_find_string(pdr, "Blend", &Blend) <= 0) {
                    *ret = 0;
                    break;
                }
                if (dict_find_string(Blend, "Private", &Priv) <= 0) {
                    *ret = 0;
                    break;
                }
                if (dict_find_string(Priv, "StdHW", &stdhw) <= 0) {
                    *ret = 0;
                    break;
                }

                if (array_get(ff->memory, stdhw, index, &sub) < 0) {
                    *ret = 0;
                    break;
                }
                *ret = (ushort)r_size(&sub);
                break;
            }
        case gs_fapi_font_feature_BlendStdHW:
            {
                ref *Priv, *Blend, *stdhw, sub, r;
                int aind = 0;
                if (dict_find_string(pdr, "Blend", &Blend) <= 0) {
                    *ret = 0;
                    break;
                }
                if (dict_find_string(Blend, "Private", &Priv) <= 0) {
                    *ret = 0;
                    break;
                }
                if (dict_find_string(Priv, "StdHW", &stdhw) <= 0) {
                    *ret = 0;
                    break;
                }

                while (1) {
                    if ((code = array_get(ff->memory, stdhw, aind++, &sub)) < 0)
                        break;
                    if (index - (int)r_size(&sub) < 0) {
                        break;
                    }
                    index -= r_size(&sub);
                }
                if (code < 0) {
                    *ret = 0;
                    break;
                }
                if (array_get(ff->memory, &sub, index, &r) < 0) {
                    *ret = 0;
                    break;
                }

                *ret = (ushort)r.value.intval;
                break;
            }
        case gs_fapi_font_feature_BlendStdVW_length:
            {
                ref *Priv, *Blend, *stdvw;
                if (dict_find_string(pdr, "Blend", &Blend) <= 0) {
                    *ret = 0;
                    break;
                }
                if (dict_find_string(Blend, "Private", &Priv) <= 0) {
                    *ret = 0;
                    break;
                }
                if (dict_find_string(Priv, "StdVW", &stdvw) <= 0) {
                    *ret = 0;
                    break;
                }
                *ret = (ushort)r_size(stdvw);
                break;
            }
        case gs_fapi_font_feature_BlendStdVW_count:
            {
                ref *Priv, *Blend, *stdvw, sub;
                if (dict_find_string(pdr, "Blend", &Blend) <= 0) {
                    *ret = 0;
                    break;
                }
                if (dict_find_string(Blend, "Private", &Priv) <= 0) {
                    *ret = 0;
                    break;
                }

                if (dict_find_string(Priv, "StdVW", &stdvw) <= 0) {
                    *ret = 0;
                    break;
                }

                if (array_get(ff->memory, stdvw, index, &sub) < 0) {
                    *ret = 0;
                    break;
                }
                *ret = (ushort)r_size(&sub);
                break;
            }
        case gs_fapi_font_feature_BlendStdVW:
            {
                ref *Priv, *Blend, *stdvw, sub, r;
                int aind = 0;
                if (dict_find_string(pdr, "Blend", &Blend) <= 0) {
                    *ret = 0;
                    break;
                }
                if (dict_find_string(Blend, "Private", &Priv) <= 0) {
                    *ret = 0;
                    break;
                }
                if (dict_find_string(Priv, "StdVW", &stdvw) <= 0) {
                    *ret = 0;
                    break;
                }

                while (1) {
                    if ((code = array_get(ff->memory, stdvw, aind++, &sub)) < 0)
                        break;
                    if (index - (int)r_size(&sub) < 0) {
                        break;
                    }
                    index -= r_size(&sub);
                }
                if (code < 0) {
                    *ret = 0;
                    break;
                }
                if (array_get(ff->memory, &sub, index, &r) < 0) {
                    *ret = 0;
                    break;
                }

                *ret = (ushort)r.value.intval;
                break;
            }
        case gs_fapi_font_feature_BlendStemSnapH_length:
            {
                ref *Priv, *Blend, *ssh;
                if (dict_find_string(pdr, "Blend", &Blend) <= 0) {
                    *ret = 0;
                    break;
                }
                if (dict_find_string(Blend, "Private", &Priv) <= 0) {
                    *ret = 0;
                    break;
                }
                if (dict_find_string(Priv, "StemSnapH", &ssh) <= 0) {
                    *ret = 0;
                    break;
                }
                *ret = (ushort)r_size(ssh);
                break;
            }
        case gs_fapi_font_feature_BlendStemSnapH_count:
            {
                ref *Priv, *Blend, *bssh, sub;
                if (dict_find_string(pdr, "Blend", &Blend) <= 0) {
                    *ret = 0;
                    break;
                }
                if (dict_find_string(Blend, "Private", &Priv) <= 0) {
                    *ret = 0;
                    break;
                }
                if (dict_find_string(Priv, "StemSnapH", &bssh) <= 0) {
                    *ret = 0;
                    break;
                }

                if (array_get(ff->memory, bssh, index, &sub) < 0) {
                    *ret = 0;
                    break;
                }
                *ret = (ushort)r_size(&sub);
                break;
            }
        case gs_fapi_font_feature_BlendStemSnapH:
            {
                ref *Priv, *Blend, *bssh, sub, r;
                int aind = 0;
                if (dict_find_string(pdr, "Blend", &Blend) <= 0) {
                    *ret = 0;
                    break;
                }
                if (dict_find_string(Blend, "Private", &Priv) <= 0) {
                    *ret = 0;
                    break;
                }
                if (dict_find_string(Priv, "StemSnapH", &bssh) <= 0) {
                    *ret = 0;
                    break;
                }

                while (1) {
                    if ((code = array_get(ff->memory, bssh, aind++, &sub)) < 0)
                        break;
                    if (index - (int)r_size(&sub) < 0) {
                        break;
                    }
                    index -= r_size(&sub);
                }
                if (code < 0) {
                    *ret = 0;
                    break;
                }
                if (array_get(ff->memory, &sub, index, &r) < 0) {
                    *ret = 0;
                    break;
                }

                *ret = (ushort)r.value.intval;
                break;
            }
        case gs_fapi_font_feature_BlendStemSnapV_length:
            {
                ref *Priv, *Blend, *ssv;
                if (dict_find_string(pdr, "Blend", &Blend) <= 0) {
                    *ret = 0;
                    break;
                }
                if (dict_find_string(Blend, "Private", &Priv) <= 0) {
                    *ret = 0;
                    break;
                }
                if (dict_find_string(Priv, "StdHW", &ssv) <= 0) {
                    *ret = 0;
                    break;
                }
                *ret = (ushort)r_size(ssv);
                break;
            }
        case gs_fapi_font_feature_BlendStemSnapV_count:
            {
                ref *Priv, *Blend, *bssv, sub;
                if (dict_find_string(pdr, "Blend", &Blend) <= 0) {
                    *ret = 0;
                    break;
                }
                if (dict_find_string(Blend, "Private", &Priv) <= 0) {
                    *ret = 0;
                    break;
                }
                if (dict_find_string(Priv, "StemSnapV", &bssv) <= 0) {
                    *ret = 0;
                    break;
                }

                if (array_get(ff->memory, bssv, index, &sub) < 0) {
                    *ret = 0;
                    break;
                }
                *ret = (ushort)r_size(&sub);
                break;
            }
        case gs_fapi_font_feature_BlendStemSnapV:
            {
                ref *Priv, *Blend, *bssv, sub, r;
                int aind = 0;
                if (dict_find_string(pdr, "Blend", &Blend) <= 0) {
                    *ret = 0;
                    break;
                }
                if (dict_find_string(Blend, "Private", &Priv) <= 0) {
                    *ret = 0;
                    break;
                }
                if (dict_find_string(Priv, "StemSnapV", &bssv) <= 0) {
                    *ret = 0;
                    break;
                }

                while (1) {
                    if ((code = array_get(ff->memory, bssv, aind++, &sub)) < 0)
                        break;
                    if (index - (int)r_size(&sub) < 0) {
                        break;
                    }
                    index -= r_size(&sub);
                }
                if (code < 0) {
                    *ret = 0;
                    break;
                }
                if (array_get(ff->memory, &sub, index, &r) < 0) {
                    *ret = 0;
                    break;
                }

                *ret = (ushort)r.value.intval;
                break;
            }

            /* End MM specifics */
    }
    return code;
}

static int
FAPI_FF_get_long(gs_fapi_font *ff, gs_fapi_font_feature var_id, int index, unsigned long *ret)
{
    gs_font_type1 *pfont = (gs_font_type1 *) ff->client_font_data;
    int code = 0;

    ref *pdr = pfont_dict(((gs_font_base *) ff->client_font_data2));

    switch ((int)var_id) {
        case gs_fapi_font_feature_UniqueID:
            *ret = pfont->UID.id;
            break;
        case gs_fapi_font_feature_BlueScale:
            *ret = (ulong) (pfont->data.BlueScale * 65536);
            break;
        case gs_fapi_font_feature_Subrs_total_size:
            {
                ref *Private, *Subrs, v;
                int lenIV = max(pfont->data.lenIV, 0), k;
                ulong size = 0;
                long i;
                const char *name[2] = { "Subrs", "GlobalSubrs" };
                if (dict_find_string(pdr, "Private", &Private) <= 0) {
                    *ret = 0;
                    break;
                }
                for (k = 0; k < 2; k++) {
                    if (dict_find_string(Private, name[k], &Subrs) > 0)
                        for (i = r_size(Subrs) - 1; i >= 0; i--) {
                            array_get(pfont->memory, Subrs, i, &v);
                            if (r_type(&v) == t_string) {
                                size += r_size(&v) - (ff->need_decrypt ? 0 : lenIV);
                            }
                        }
                }
                *ret = size;
            }
            break;
        case gs_fapi_font_feature_TT_size:
            code = true_type_size(ff->memory, pdr, ret);
            break;
    }
    return code;
}

static int
FAPI_FF_get_float(gs_fapi_font *ff, gs_fapi_font_feature var_id, int index, float *ret)
{
    gs_font_type1 *pfont1 = (gs_font_type1 *) ff->client_font_data;
    gs_font_base *pbfont = (gs_font_base *) ff->client_font_data2;
    ref *pdr = pfont_dict(pbfont);
    int code = 0;
    gs_fapi_server *I = pbfont->FAPI;

    switch ((int)var_id) {
        case gs_fapi_font_feature_FontMatrix:
            {
                double FontMatrix_div;
                gs_matrix m, *mptr;

                if (I && I->get_fontmatrix) {
                    FontMatrix_div = 1;
                    mptr = &m;
                    I->get_fontmatrix(I, mptr);
                }
                else {
                    FontMatrix_div =
                        ((ff->is_cid
                          && (!FAPI_ISCIDFONT(pbfont))) ? 1000 : 1);
                    mptr = &(pbfont->base->FontMatrix);
                }
                switch (index) {
                    case 0:
                    default:
                        *ret = (mptr->xx / FontMatrix_div);
                        break;
                    case 1:
                        *ret = (mptr->xy / FontMatrix_div);
                        break;
                    case 2:
                        *ret = (mptr->yx / FontMatrix_div);
                        break;
                    case 3:
                        *ret = (mptr->yy / FontMatrix_div);
                        break;
                    case 4:
                        *ret = (mptr->tx / FontMatrix_div);
                        break;
                    case 5:
                        *ret = (mptr->ty / FontMatrix_div);
                        break;
                }
                break;
            }

        case gs_fapi_font_feature_WeightVector:
            {
                if (index < pfont1->data.WeightVector.count) {
                    *ret = pfont1->data.WeightVector.values[index];
                }
                else {
                    *ret = 0;
                }
            }
            break;
        case gs_fapi_font_feature_BlendDesignPositionsArrayValue:
            {
                ref *Info, *Array, SubArray, value;
                int array_index = index / 8;

                index %= 8;
                if (dict_find_string(pdr, "FontInfo", &Info) <= 0) {
                    *ret = 0;
                    break;
                }
                if (dict_find_string(Info, "BlendDesignPositions", &Array) <= 0) {
                    *ret = 0;
                    break;
                }
                if (array_get(ff->memory, Array, array_index, &SubArray) < 0) {
                    *ret = 0;
                    break;
                }
                if (array_get(ff->memory, &SubArray, index, &value) < 0)
                    return 0;
                if (!r_has_type(&value, t_integer)) {
                    if (r_has_type(&value, t_real)) {
                        *ret = value.value.realval;
                    }
                    else
                        *ret = 0;
                }
                else
                    *ret = ((float)value.value.intval);
            }
            break;
        case gs_fapi_font_feature_BlendDesignMapArrayValue:
            {
                ref *Info, *Array, SubArray, SubSubArray, value;
                int array_index = index / 64;

                index %= 8;
                if (dict_find_string(pdr, "FontInfo", &Info) <= 0) {
                    *ret = 0;
                    break;
                 }
                if (dict_find_string(Info, "BlendDesignMap", &Array) <= 0) {
                    *ret = 0;
                    break;
                }
                if (array_get(ff->memory, Array, array_index, &SubArray) < 0) {
                    *ret = 0;
                    break;
                }
                if (array_get(ff->memory, &SubArray, index, &SubSubArray) < 0) {
                    *ret = 0;
                    break;
                }
                if (array_get(ff->memory, &SubSubArray, index, &value) < 0) {
                    *ret = 0;
                    break;
                }
                if (!r_has_type(&value, t_integer)) {
                    if (r_has_type(&value, t_real)) {
                        *ret = (value.value.realval);
                    }
                    else
                        *ret = 0;
                }
                else
                    *ret = ((float)value.value.intval);
            }
            break;
        case gs_fapi_font_feature_BlendBlueScale:
            {
                ref *Priv, *Blend, *bbs, r;

                if (dict_find_string(pdr, "Blend", &Blend) <= 0) {
                    *ret = 0;
                    break;
                }
                if (dict_find_string(Blend, "Private", &Priv) <= 0) {
                    *ret = 0;
                    break;
                }
                if (dict_find_string(Priv, "BlueScale", &bbs) <= 0) {
                    *ret = 0;
                    break;
                }
                if (array_get(ff->memory, bbs, index, &r) < 0) {
                    *ret = 0;
                    break;
                }
               if (r_has_type(&r, t_real))
                   *ret = r.value.realval;
               else if (r_has_type(&r, t_integer))
                   *ret = (float)r.value.intval;
            }
            break;
    }
    return code;
}

static int
FAPI_FF_get_name(gs_fapi_font *ff, gs_fapi_font_feature var_id, int index,
                 char *Buffer, int len)
{
    ref name, string;
    ref *pdr = pfont_dict(((gs_font_base *) ff->client_font_data2));
    int code = 0;

    switch ((int)var_id) {
        case gs_fapi_font_feature_BlendAxisTypes:
            {
                ref *Info, *Axes;

                if (dict_find_string(pdr, "FontInfo", &Info) <= 0) {
                    code = gs_note_error(gs_error_undefined);
                    break;
                }
                if (dict_find_string(Info, "BlendAxisTypes", &Axes) <= 0) {
                    code = gs_note_error(gs_error_undefined);
                    break;
                }
                if (!r_has_type(Axes, t_array)) {
                    code = gs_note_error(gs_error_undefined);
                    break;
                }
                if (array_get(ff->memory, Axes, index, &name) < 0) {
                    code = gs_note_error(gs_error_undefined);
                    break;
                }
            }
    }
    if (code >= 0) {
        name_string_ref(ff->memory, &name, &string);
        if (r_size(&string) < len) {
            memcpy(Buffer, string.value.const_bytes, r_size(&string));
            Buffer[r_size(&string)] = 0x00;
        }
        else {
            code = gs_note_error(gs_error_unknownerror);
        }
    }
    return code;
}

/* NOTE: we checked the type of $Blend at definefont time, so we know it is a
 * procedure and don't need to check it again here
 */
static int
FAPI_FF_get_proc(gs_fapi_font *ff, gs_fapi_font_feature var_id, int index,
                 char *Buffer)
{
    ref *pdr = pfont_dict(((gs_font_base *) ff->client_font_data2));
    char *ptr = Buffer;
    int code = 0;

    if (!Buffer)
        return_error(gs_error_unknownerror);

    switch ((int)var_id) {
        case gs_fapi_font_feature_DollarBlend:
            {
                ref *DBlend, Element, string;
                int i;
                char Buf[32];

                if (dict_find_string(pdr, "$Blend", &DBlend) <= 0) {
                    code = gs_note_error(gs_error_undefined);
                    break;
                }
                for (i = 0; i < r_size(DBlend); i++) {
                    *ptr++ = 0x20;
                    if (array_get(ff->memory, DBlend, i, &Element) < 0) {
                        code = gs_note_error(gs_error_undefined);
                        break;
                    }
                    switch (r_btype(&Element)) {
                        case t_name:
                            name_string_ref(ff->memory, &Element, &string);

                            strncpy(ptr, (char *)string.value.const_bytes,
                                    r_size(&string));
                            ptr += r_size(&string);
                            break;
                        case t_real:
                            gs_snprintf(Buf, sizeof(Buf), "%f", Element.value.realval);
                            strcpy(ptr, Buf);
                            ptr += strlen(Buf);
                            break;
                        case t_integer:
                            gs_snprintf(Buf, sizeof(Buf), "%"PRIpsint, Element.value.intval);
                            strcpy(ptr, Buf);
                            ptr += strlen(Buf);
                            break;
                        case t_operator:
                            {
                                op_def const *op;

                                op = op_index_def(r_size(&Element));
                                strcpy(ptr, op->oname + 1);
                                ptr += strlen(op->oname + 1);
                            }
                            break;
                        default:
                            break;
                    }
                }
            }
    }
    return code < 0 ? code : (ptr - Buffer);
}

static inline void
decode_bytes(byte *p, const byte *s, int l, int lenIV)
{
    ushort state = 4330;

    for (; l; s++, l--) {
        uchar c = (*s ^ (state >> 8));

        state = (*s + state) * crypt_c1 + crypt_c2;
        if (lenIV > 0)
            lenIV--;
        else {
            *p = c;
            p++;
        }
    }
}

static int
get_type1_data(gs_fapi_font *ff, const ref *type1string,
               byte *buf, int buf_length)
{
    gs_font_type1 *pfont = (gs_font_type1 *) ff->client_font_data;
    int lenIV = max(pfont->data.lenIV, 0);
    int length = r_size(type1string) - (ff->need_decrypt ? lenIV : 0);

    if (buf != 0) {
        int l = min(length, buf_length);        /*safety */

        if (ff->need_decrypt && pfont->data.lenIV >= 0)
            decode_bytes(buf, type1string->value.const_bytes, l + lenIV,
                         lenIV);
        else
            memcpy(buf, type1string->value.const_bytes, l);
    }
    return length;
}

static int
FAPI_FF_get_gsubr(gs_fapi_font *ff, int index, byte *buf, int buf_length)
{
    ref *pdr = pfont_dict(((gs_font_base *) ff->client_font_data2));
    ref *Private, *GlobalSubrs, subr;

    if (dict_find_string(pdr, "Private", &Private) <= 0)
        return 0;
    if (dict_find_string(Private, "GlobalSubrs", &GlobalSubrs) <= 0)
        return 0;
    if (array_get(ff->memory,
                  GlobalSubrs, index, &subr) < 0 || r_type(&subr) != t_string)
        return 0;
    return (get_type1_data(ff, &subr, buf, buf_length));
}

static int
FAPI_FF_get_subr(gs_fapi_font *ff, int index, byte *buf, int buf_length)
{
    ref *pdr = pfont_dict(((gs_font_base *) ff->client_font_data2));
    ref *Private, *Subrs, subr;

    if (dict_find_string(pdr, "Private", &Private) <= 0)
        return 0;
    if (dict_find_string(Private, "Subrs", &Subrs) <= 0)
        return 0;
    if (array_get(ff->memory, Subrs, index, &subr) < 0
        || r_type(&subr) != t_string)
        return 0;
    return (get_type1_data(ff, &subr, buf, buf_length));
}

static int
FAPI_FF_get_raw_subr(gs_fapi_font *ff, int index, byte *buf,
                     int buf_length)
{
    ref *pdr = pfont_dict(((gs_font_base *) ff->client_font_data2));
    ref *Private, *Subrs, subr;
    int code = 0;

    do {
        if (dict_find_string(pdr, "Private", &Private) <= 0) {
            code = gs_note_error(gs_error_undefined);
            break;
        }
        if (dict_find_string(Private, "Subrs", &Subrs) <= 0) {
            code = gs_note_error(gs_error_undefined);
            break;
        }
        if (array_get(ff->memory, Subrs, index, &subr) < 0) {
            code = gs_note_error(gs_error_undefined);
            break;
        }
        if (r_type(&subr) != t_string) {
            code = gs_note_error(gs_error_undefined);
            break;
        }
        if (buf && buf_length && buf_length >= r_size(&subr)) {
            memcpy(buf, subr.value.const_bytes, r_size(&subr));
        }
    } while(0);

    return code < 0 ? code : r_size(&subr);
}

/* FAPI_FF_get_charstring_name() and FAPI_FF_get_charstring()
 *
 * Generally we'd want to use the dictionary content
 * enumeration API rather than dict_index_entry(), but
 * the FAPI interface doesn't enforce sequential accessing
 * of the indices.
 * Setting up enumeration and enumerating through the entries
 * until we reach the requested valid index is a performance
 * hit we don't want to pay.
 *
 * Luckily, the checks we need for invalid CharString contents
 * also handle empty "slots" in the dictionary.
 */

static int
FAPI_FF_get_charstring_name(gs_fapi_font *ff, int index, byte *buf,
                            ushort buf_length)
{
    int code = 0;
    ref *pdr = pfont_dict(((gs_font_base *) ff->client_font_data2));
    ref *CharStrings, eltp[2], string;

    do {
        if (dict_find_string(pdr, "CharStrings", &CharStrings) <= 0) {
            code = gs_note_error(gs_error_undefined);
            break;
        }
        if (dict_index_entry(CharStrings, index, eltp) < 0) {
            code = gs_note_error(gs_error_undefined);
            break;
        }
        if (r_type(&eltp[0]) != t_name) {
            code = gs_note_error(gs_error_undefined);
            break;
        }
        name_string_ref(ff->memory, &eltp[0], &string);
        if (r_size(&string) <= buf_length) {
            memcpy(buf, string.value.const_bytes, r_size(&string));
            buf[r_size(&string)] = 0x00;
        }
    } while(0);
    return code < 0 ? code : r_size(&string);
}

static int
FAPI_FF_get_charstring(gs_fapi_font *ff, int index, byte *buf,
                       ushort buf_length)
{
    int code = 0;
    ref *pdr = pfont_dict(((gs_font_base *) ff->client_font_data2));
    ref *CharStrings, eltp[2];

    do {
        if (dict_find_string(pdr, "CharStrings", &CharStrings) <= 0) {
            code = gs_note_error(gs_error_undefined);
            break;
        }
        if (dict_index_entry(CharStrings, index, eltp) < 0) {
            code = gs_note_error(gs_error_undefined);
            break;
        }
        if (r_type(&eltp[1]) != t_string) {
            code = gs_note_error(gs_error_typecheck);
            break;
        }
        if (buf && buf_length && buf_length >= r_size(&eltp[1])) {
            memcpy(buf, eltp[1].value.const_bytes, r_size(&eltp[1]));
        }
    } while(0);

    return code < 0 ? code : r_size(&eltp[1]);
}

static int
sfnt_get_sfnt_length(ref *pdr, ulong *len)
{
    int code = 0;
    ref *sfnts, sfnt_elem;
    const gs_memory_t *mem = dict_mem(pdr->value.pdict);

    *len = 0;
    if (r_type(pdr) != t_dictionary ||
        dict_find_string(pdr, "sfnts", &sfnts) <= 0) {
        code = gs_error_invalidfont;
    }
    else {
        if (r_type(sfnts) != t_array && r_type(sfnts) != t_string) {
            code = gs_error_invalidfont;
        }
        else {
            if (r_type(sfnts) == t_string) {
                *len = r_size(sfnts);
            }
            else {
                int i;
                for (i = 0; i < r_size(sfnts); i++) {
                    code = array_get(mem, sfnts, i, &sfnt_elem);
                    if (code < 0) break;
                    *len += r_size(&sfnt_elem);
                }
            }
        }
    }
    return code;
}

static int
sfnt_get_glyph_offset(ref *pdr, gs_font_type42 *pfont42, int index,
                      ulong *offset0)
{                               /* Note : TTC is not supported and probably is unuseful for Type 42. */
    sfnts_reader r;
    int glyf_elem_size = (pfont42->data.indexToLocFormat) ? 4 : 2;
    ulong fullsize;

    if (index < pfont42->data.trueNumGlyphs) {
        sfnts_reader_init(pfont42->memory, &r, pdr);
        r.seek(&r, pfont42->data.loca + index * (ulong)glyf_elem_size);
        *offset0 =
            pfont42->data.glyf + (glyf_elem_size ==
                              2 ? r.rword(&r) * 2 : r.rlong(&r));
        r.error = sfnt_get_sfnt_length(pdr, &fullsize);
        if (r.error < 0 || *offset0 > fullsize) {
            r.error = gs_note_error(gs_error_invalidaccess);
        }
    }
    else {
        r.error = gs_note_error(gs_error_rangecheck);
    }
    return (r.error);
}

static int
ps_get_GlyphDirectory_data_ptr(gs_fapi_font *ff, int char_code,
                               const byte **ptr)
{
    ref *pdr = pfont_dict(((gs_font_base *) ff->client_font_data2));
    ref *GlyphDirectory, glyph0, *glyph = &glyph0, glyph_index;

    if (dict_find_string(pdr, "GlyphDirectory", &GlyphDirectory) > 0) {
        if (((r_type(GlyphDirectory) == t_dictionary &&
              (make_int(&glyph_index, char_code),
               dict_find(GlyphDirectory, &glyph_index, &glyph) > 0)) ||
             (r_type(GlyphDirectory) == t_array &&
              array_get(ff->memory, GlyphDirectory, char_code, &glyph0) >= 0)
            )
            && r_type(glyph) == t_string) {
            *ptr = glyph->value.const_bytes;
            return (r_size(glyph));
        }
        else
            /* We have a GlyphDirectory, but couldn't find the glyph. If we
             * return -1 then we will attempt to use glyf and loca which
             * will fail. Instead return 0, so we execute an 'empty' glyph.
             */
            return 0;
    }
    return -1;
}

static int
get_charstring(gs_fapi_font *ff, int char_code, ref **proc, ref *char_name)
{
    ref *CharStrings;
    ref *pdr = pfont_dict(((gs_font_base *) ff->client_font_data2));

    if (ff->is_type1) {
        if (ff->is_cid)
            return -1;
        if (dict_find_string(pdr, "CharStrings", &CharStrings) <= 0)
            return -1;

        if (ff->char_data != NULL) {
            /*
             * Can't use char_code in this case because hooked Type 1 fonts
             * with 'glyphshow' may render a character which has no
             * Encoding entry.
             */
            if (name_ref
                (ff->memory, ff->char_data, ff->char_data_len, char_name,
                 -1) < 0)
                return -1;
        }
        else {                  /* seac */
            i_ctx_t *i_ctx_p = (i_ctx_t *) ff->client_ctx_p;
            ref *StandardEncoding;

            if (dict_find_string
                (systemdict, "StandardEncoding", &StandardEncoding) <= 0
                || array_get(ff->memory, StandardEncoding, char_code,
                             char_name) < 0) {
                if (name_ref
                    (ff->memory, (const byte *)".notdef", 7, char_name,
                     -1) < 0)
                    return -1;
            }
        }
        if (dict_find(CharStrings, char_name, (ref **) proc) <= 0)
            return -1;
    }
    return 0;
}

static int
FAPI_FF_get_glyph(gs_fapi_font *ff, gs_glyph char_code, byte *buf, int buf_length)
{
    /*
     * We assume that renderer requests glyph data with multiple
     * consecutive calls to this function.
     *
     * For a simple glyph it calls this function exactly twice: first
     * with buf == NULL for requesting the necessary buffer length, and
     * second with buf != NULL for requesting the data (the second call
     * may be skipped if the renderer discontinues the rendering).
     *
     * For a composite glyph it calls this function 2 * (N + 1)
     * times: 2 calls for the main glyph (same as above) followed with
     * 2 * N calls for subglyphs, where N is less or equal to the number
     * of subglyphs (N may be less if the renderer caches glyph data,
     * or discontinues rendering on an exception).
     */
    ref *pdr = pfont_dict(((gs_font_base *) ff->client_font_data2));

    int glyph_length;
    i_ctx_t *i_ctx_p = (i_ctx_t *) ff->client_ctx_p;

    if (ff->is_type1) {
        if (ff->is_cid) {
            const gs_string *char_str = (const gs_string *)ff->char_data;
            ref glyph;

            make_string(&glyph, avm_foreign | a_readonly, char_str->size,
                        char_str->data);

            glyph_length = get_type1_data(ff, &glyph, buf, buf_length);
        }
        else {
            ref *CharStrings, *CFFCharStrings, char_name, *glyph;

            if (ff->char_data != NULL) {
                /*
                 * Can't use char_code in this case because hooked Type 1 fonts
                 * with 'glyphshow' may render a character which has no
                 * Encoding entry.
                 */
                if (name_ref(ff->memory, ff->char_data,
                             ff->char_data_len, &char_name, -1) < 0)
                    return gs_fapi_glyph_invalid_format;
                if (buf != NULL) {
                    /*
                     * Trigger the next call to the 'seac' case below.
                     * Here we use the assumption about call sequence
                     * being documented above.
                     */
                    ff->char_data = NULL;
                }
            }
            else {              /* seac */
                ref *StandardEncoding;

                if (dict_find_string
                    (systemdict, "StandardEncoding", &StandardEncoding) <= 0
                    || array_get(ff->memory, StandardEncoding, char_code,
                                 &char_name) < 0)
                    if (name_ref
                        (ff->memory, (const byte *)".notdef", 7, &char_name, -1) < 0)
                        return gs_fapi_glyph_invalid_format;
            }
            if (dict_find_string(pdr, "CharStrings", &CharStrings) <= 0)
                return gs_fapi_glyph_invalid_format;

            if (dict_find(CharStrings, &char_name, &glyph) <= 0) {
                if (name_ref
                    (ff->memory, (const byte *)".notdef", 7, &char_name, -1) < 0) {
                    return gs_fapi_glyph_invalid_format;
                }
                if (dict_find(CharStrings, &char_name, &glyph) <= 0) {
                    return gs_fapi_glyph_invalid_format;
                }
            }
            if (r_has_type(glyph, t_array) || r_has_type(glyph, t_mixedarray))
                return gs_fapi_glyph_invalid_format;

            if (r_has_type(glyph, t_integer)
                && dict_find_string(pdr, "CFFCharStrings", &CFFCharStrings) > 0) {
                ref *g2;
                if (dict_find(CFFCharStrings, glyph, &g2) <= 0) {
                    ref nd;
                    make_int(&nd, 0);
                    if (dict_find(CFFCharStrings, &nd, &g2) <= 0) {
                        return gs_fapi_glyph_invalid_format;
                    }
                }
                glyph = g2;
            }

            if (!r_has_type(glyph, t_string))
                return 0;
            glyph_length = get_type1_data(ff, glyph, buf, buf_length);
        }
    }
    else {                      /* type 42 */
        const byte *data_ptr;
        int l = ff->get_glyphdirectory_data(ff, char_code, &data_ptr);
        ref *render_notdef_ref;
        bool render_notdef = true;

        if (dict_find_string(pdr, ".render_notdef", &render_notdef_ref) > 0
            && r_has_type(render_notdef_ref, t_boolean)) {
            render_notdef = render_notdef_ref->value.boolval;
        }
        else {
            render_notdef = i_ctx_p->RenderTTNotdef;
        }

        /* We should only render the TT notdef if we've been told to - logic lifted from zchar42.c */
        if (!render_notdef
            &&
            ((ff->char_data_len == 7
              && strncmp((const char *)ff->char_data, ".notdef", 7) == 0)
             || (ff->char_data_len > 9
                 && strncmp((const char *)ff->char_data, ".notdef~GS",
                            10) == 0))) {
            glyph_length = 0;
        }
        else {
            if (l >= 0) {
                int MetricsCount = gs_fapi_get_metrics_count(ff), mc =
                    MetricsCount << 1;

                glyph_length = max((ushort) (l - mc), 0);       /* safety */
                if (buf != 0 && glyph_length > 0)
                    memcpy(buf, data_ptr + mc,
                           min(glyph_length, buf_length) /* safety */ );
            }
            else {
                gs_font_type42 *pfont42 = (gs_font_type42 *) ff->client_font_data;
                ulong offset0, length_read;
                int error = sfnt_get_glyph_offset(pdr, pfont42, char_code, &offset0);

                if (error < 0) {
                    glyph_length = gs_fapi_glyph_invalid_index;
                }
                else if (pfont42->data.len_glyphs) {
                    if (char_code <= pfont42->data.numGlyphs)
                        glyph_length = pfont42->data.len_glyphs[char_code];
                    else
                        glyph_length = gs_fapi_glyph_invalid_index;
                }
                else {
                    ulong noffs, endoffs;
                    /* If we haven't got a len_glyphs array, try using the offset of the next glyph offset
                     * to work out the length
                     */
                    error = sfnt_get_glyph_offset(pdr, pfont42, char_code + 1, &noffs);
                    if (error == 0) {
                        glyph_length = noffs - offset0;
                        error = sfnt_get_sfnt_length(pdr, &endoffs);
                        if (error < 0) {
                            glyph_length = gs_fapi_glyph_invalid_index;
                        }
                        else {
                            if (glyph_length + offset0 > endoffs) {
                                glyph_length = gs_fapi_glyph_invalid_index;
                            }
                        }
                    }
                    else {
                        /* And if we can't get the next glyph offset, use the end of the sfnt data
                         * to work out the length.
                         */
                        error = sfnt_get_sfnt_length(pdr, &noffs);
                        if (error < 0) {
                            glyph_length = gs_fapi_glyph_invalid_index;
                        }
                        else {
                            glyph_length = noffs - offset0;
                        }
                    }
                }

                if (buf != 0 && !error) {
                    sfnts_reader r;

                    sfnts_reader_init(pfont42->memory, &r, pdr);

                    r.seek(&r, offset0);
                    length_read =
                        r.rstring(&r, buf,
                                  min(glyph_length,
                                      buf_length) /* safety */ );
                    if (r.error == 1) {
                        glyph_length = gs_fapi_glyph_invalid_index;
                    }
                    /* r.error == 2 means a rangecheck, and probably means that the
                     * font is broken, and the final glyph length is longer than the data available for it.
                     * In which case we need to return the number of bytes read.
                     */
                    if (r.error == 2) {
                        glyph_length = length_read;
                    }
                }
            }
        }
    }
    return glyph_length;
}

static int
ps_fapi_get_metrics(gs_fapi_font *ff, gs_string *char_name, gs_glyph cid, double *m, bool vertical)
{
    ref glyph;
    int code;
    gs_font_base *pbfont = ((gs_font_base *) ff->client_font_data2);

    if (char_name->data != NULL) {
        make_string(&glyph, avm_foreign | a_readonly, char_name->size, char_name->data);
    }
    else {
        make_int(&glyph, cid);
    }

    if (vertical) {
        code = zchar_get_metrics2(pbfont, &glyph, m);
    }
    else {
        code = zchar_get_metrics(pbfont, &glyph, m);
    }

    make_null(&glyph);

    return (code);
}


/* forward declaration for the ps_ff_stub assignment */
static int ps_get_glyphname_or_cid(gs_text_enum_t *penum,
                                   gs_font_base *pbfont,
                                   gs_string *charstring, gs_string *name,
                                   gs_glyph ccode, gs_string *enc_char_name,
                                   char *font_file_path,
                                   gs_fapi_char_ref *cr, bool bCID);

static int ps_fapi_set_cache(gs_text_enum_t *penum,
                             const gs_font_base *pbfont,
                             const gs_string *char_name, gs_glyph cid,
                             const double pwidth[2], const gs_rect *pbbox,
                             const double Metrics2_sbw_default[4],
                             bool *imagenow);

static const gs_fapi_font ps_ff_stub = {
    0,                          /* server_font_data */
    0,                          /* need_decrypt */
    NULL,                       /* const gs_memory_t */
    0,                          /* font_file_path */
    0,                          /* full_font_buf */
    0,                          /* full_font_buf_len */
    0,                          /* subfont */
    false,                      /* is_type1 */
    false,                      /* is_cid */
    false,                      /* is_outline_font */
    false,                      /* is_mtx_skipped */
    false,                      /* is_vertical */
    false,                      /* metrics_only */
    {{-1, -1}},                 /* ttf_cmap_req */
    {-1, -1},                   /* ttf_cmap_selected */
    0,                          /* client_ctx_p */
    0,                          /* client_font_data */
    0,                          /* client_font_data2 */
    0,                          /* char_data */
    0,                          /* char_data_len */
    0,                          /* embolden */
    FAPI_FF_get_word,
    FAPI_FF_get_long,
    FAPI_FF_get_float,
    FAPI_FF_get_name,
    FAPI_FF_get_proc,
    FAPI_FF_get_gsubr,
    FAPI_FF_get_subr,
    FAPI_FF_get_raw_subr,
    FAPI_FF_get_glyph,
    FAPI_FF_serialize_tt_font,
    NULL,                       /* retrieve_tt_font */
    FAPI_FF_get_charstring,
    FAPI_FF_get_charstring_name,
    ps_get_GlyphDirectory_data_ptr,
    ps_get_glyphname_or_cid,
    ps_fapi_get_metrics,
    ps_fapi_set_cache
};

static int
FAPI_get_xlatmap(i_ctx_t *i_ctx_p, char **xlatmap)
{
    ref *pref;
    int code;

    if ((code = dict_find_string(systemdict, ".xlatmap", &pref)) < 0)
        return code;
    if (code == 0)
        return_error(gs_error_undefined);

    if (r_type(pref) != t_string)
        return_error(gs_error_typecheck);
    *xlatmap = (char *)pref->value.bytes;
    /*  Note : this supposes that xlatmap doesn't move in virtual memory.
       Garbager must not be called while plugin executes get_scaled_font, get_decodingID.
       Fix some day with making copy of xlatmap in system memory.
     */
    return 0;
}

static int
renderer_retcode(gs_memory_t *mem, gs_fapi_server *I, gs_fapi_retcode rc)
{
    if (rc == 0)
        return 0;
    emprintf2(mem,
              "Error: Font Renderer Plugin ( %s ) return code = %d\n",
              I->ig.d->subtype, rc);
    return rc < 0 ? rc : gs_error_invalidfont;
}

/* <server name>/<null> object .FAPIavailable bool */
static int
zFAPIavailable(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    char *serv_name = NULL;
    ref name_ref;

    check_op(1);
    if (r_has_type(op, t_name)) {
        name_string_ref(imemory, op, &name_ref);

        serv_name =
            (char *) ref_to_string(&name_ref, imemory, "zFAPIavailable");
        if (!serv_name) {
            return_error(gs_error_VMerror);
        }
    }

    make_bool(op, gs_fapi_available(imemory, serv_name));

    if (serv_name) {
        gs_free_string(imemory, (byte *) serv_name,
                       strlen((char *)serv_name) + 1, "zFAPIavailable");
    }
    return (0);
}

static void
ps_get_server_param(gs_fapi_server *I, const byte *subtype,
                    byte **server_param, int *server_param_size)
{
    ref *FAPIconfig, *options, *server_options;
    i_ctx_t *i_ctx_p = (i_ctx_t *) I->client_ctx_p;

    if (dict_find_string(systemdict, ".FAPIconfig", &FAPIconfig) > 0
        && r_has_type(FAPIconfig, t_dictionary)) {
        if (dict_find_string(FAPIconfig, "ServerOptions", &options) > 0
            && r_has_type(options, t_dictionary)) {
            if (dict_find_string(options, (char *)subtype, &server_options) >
                0 && r_has_type(server_options, t_string)) {
                *server_param = (byte *) server_options->value.const_bytes;
                *server_param_size = r_size(server_options);
            }
        }
    }
}

static int
FAPI_refine_font(i_ctx_t *i_ctx_p, os_ptr op, gs_font *pfont,
                 int subfont, const char *font_file_path)
{
    ref *pdr = op;              /* font dict */
    const char *decodingID = NULL;
    char *xlatmap = NULL;
    gs_font_base *pbfont = (gs_font_base *)pfont;
    gs_fapi_server *I = pbfont->FAPI;
    ref *Decoding_old;
    int code;

    if (font_file_path != NULL && pbfont->FAPI_font_data == NULL)
        if ((code = FAPI_get_xlatmap(i_ctx_p, &xlatmap)) < 0)
            return code;

    gs_fapi_set_servers_client_data(imemory, NULL, i_ctx_p);

    code =
        gs_fapi_prepare_font(pfont, I, subfont, font_file_path,
                             NULL, xlatmap, &decodingID);
    if (code < 0)
        return code;

    if (code > 0) {
        /* save refined FontBBox back to PS world */
        ref *v, mat[4], arr;
        int attrs;

        if (dict_find_string(op, "FontBBox", &v) > 0) {
            if (!r_has_type(v, t_array) && !r_has_type(v, t_shortarray)
                && !r_has_type(v, t_mixedarray))
                return_error(gs_error_invalidfont);
            make_real(&mat[0], pbfont->FontBBox.p.x);
            make_real(&mat[1], pbfont->FontBBox.p.y);
            make_real(&mat[2], pbfont->FontBBox.q.x);
            make_real(&mat[3], pbfont->FontBBox.q.y);
            if (r_has_type(v, t_shortarray) || r_has_type(v, t_mixedarray)
                || r_size(v) < 4) {
                /* Create a new full blown array in case the values are reals */
                code = ialloc_ref_array(&arr, a_all, 4, "array");
                if (code < 0)
                    return code;
                v = &arr;
                code = idict_put_string(op, "FontBBox", &arr);
                if (code < 0)
                    return code;
                ref_assign_new(v->value.refs + 0, &mat[0]);
                ref_assign_new(v->value.refs + 1, &mat[1]);
                ref_assign_new(v->value.refs + 2, &mat[2]);
                ref_assign_new(v->value.refs + 3, &mat[3]);
            }
            else {
                ref_assign_old(v, v->value.refs + 0, &mat[0],
                               "FAPI_refine_font_BBox");
                ref_assign_old(v, v->value.refs + 1, &mat[1],
                               "FAPI_refine_font_BBox");
                ref_assign_old(v, v->value.refs + 2, &mat[2],
                               "FAPI_refine_font_BBox");
                ref_assign_old(v, v->value.refs + 3, &mat[3],
                               "FAPI_refine_font_BBox");
            }
            attrs = v->tas.type_attrs;
            r_clear_attrs(v, a_all);
            r_set_attrs(v, attrs | a_execute);
        }
    }

    /* Assign a Decoding : */
    if (decodingID != 0 && *decodingID
        && dict_find_string(pdr, "Decoding", &Decoding_old) <= 0) {
        ref Decoding;

        if (FAPI_ISCIDFONT(pbfont)) {
            ref *CIDSystemInfo, *Ordering, SubstNWP;
            byte buf[30];
            int ordering_length, decodingID_length =
                min(strlen(decodingID), sizeof(buf) - 2);

            if (dict_find_string(pdr, "CIDSystemInfo", &CIDSystemInfo) <= 0
                || !r_has_type(CIDSystemInfo, t_dictionary))
                return_error(gs_error_invalidfont);

            if (dict_find_string(CIDSystemInfo, "Ordering", &Ordering) <= 0
                || !r_has_type(Ordering, t_string)) {
                return_error(gs_error_invalidfont);
            }

            ordering_length =
                min(r_size(Ordering), sizeof(buf) - 2 - decodingID_length);
            memcpy(buf, Ordering->value.const_bytes, ordering_length);
            if ((code =
                 name_ref(imemory, buf, ordering_length, &SubstNWP, 0)) < 0)
                return code;
            if ((code =
                 dict_put_string(pdr, "SubstNWP", &SubstNWP, NULL)) < 0)
                return code;
            buf[ordering_length] = '.';
            memcpy(buf + ordering_length + 1, decodingID, decodingID_length);
            buf[decodingID_length + 1 + ordering_length] = 0;   /* Debug purpose only */
            if ((code = name_ref(imemory, buf,
                                 decodingID_length + 1 + ordering_length,
                                 &Decoding, 0)) < 0)
                return code;
        }
        else if ((code = name_ref(imemory, (const byte *)decodingID,
                                  strlen(decodingID), &Decoding, 0)) < 0)
            return code;
        if ((code = dict_put_string(pdr, "Decoding", &Decoding, NULL)) < 0)
            return code;
    }
    return 0;
}

/*  <string|name> <font> <is_disk_font> .rebuildfontFAPI <string|name> <font> */
/*  Rebuild a font for handling it with an external renderer.

    The font was built as a native GS font to allow easy access
    to font features. Then zFAPIrebuildfont sets FAPI entry
    into gx_font_base and replaces BuildGlyph and BuildChar
    to enforce the FAPI handling.

    This operator must not be called with devices which embed fonts.

*/
static int
zFAPIrebuildfont(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    build_proc_refs build;
    gs_font *pfont;
    int code;
    gs_font_base *pbfont;
    ref *v;
    char *font_file_path = NULL;
    char FAPI_ID[20];
    const byte *pchars;
    uint len;
    font_data *pdata;
    gs_fapi_server *I;
    bool has_buildglyph;
    bool has_buildchar;
    int subfont;

    check_op(3);
    code = font_param(op - 1, &pfont);
    if (code < 0)
        return code;

    pbfont = (gs_font_base *) pfont;

    check_type(*op, t_boolean);
    /* If someone has copied the font dictionary, we may still
     * have the FAPI entry in the dict, but not have the FAPI
     * server assigned in the font object.
     */
    if (pbfont->FAPI == NULL) {
        if (dict_find_string(op - 1, "FAPI", &v) <= 0
            || !r_has_type(v, t_name))
            return_error(gs_error_invalidfont);
        obj_string_data(imemory, v, &pchars, &len);
        len = min(len, sizeof(FAPI_ID) - 1);
        strncpy((char *)FAPI_ID, (const char *)pchars, len);
        FAPI_ID[len] = 0;

        gs_fapi_set_servers_client_data(imemory, &ps_ff_stub, i_ctx_p);

        code =
            gs_fapi_find_server(imemory, FAPI_ID,
                                (gs_fapi_server **) & (pbfont->FAPI),
                                (gs_fapi_get_server_param_callback)
                                ps_get_server_param);
        if (!pbfont->FAPI || code < 0) {
            return_error(gs_error_invalidfont);
        }
    }

    pdata = (font_data *) pfont->client_data;
    I = pbfont->FAPI;

    if (dict_find_string((op - 1), "SubfontId", &v) > 0
        && r_has_type(v, t_integer))
        subfont = v->value.intval;
    else
        subfont = 0;


    if (r_type(&(pdata->BuildGlyph)) != t_null) {
        has_buildglyph = true;
    }
    else {
        has_buildglyph = false;
    }

    if (r_type(&(pdata->BuildChar)) != t_null) {
        has_buildchar = true;
    }
    else {
        has_buildchar = false;
    }

    /* This shouldn't happen, but just in case */
    if (has_buildglyph == false && has_buildchar == false) {
        has_buildglyph = true;
    }

    if (dict_find_string(op - 1, "Path", &v) <= 0 || !r_has_type(v, t_string)) {
        v = NULL;
    }

    if (pfont->FontType == ft_CID_encrypted && v == NULL) {
        if ((code = build_proc_name_refs(imemory, &build, ".FAPIBuildGlyph9",
                                         ".FAPIBuildGlyph9")) < 0) {
            return code;
        }
    }
    else {
        if ((code = build_proc_name_refs(imemory, &build, ".FAPIBuildChar",
                                         ".FAPIBuildGlyph")) < 0) {
            return code;
        }
    }

    if (!
        ((r_type(&(pdata->BuildChar)) != t_null
          && pdata->BuildChar.value.pname && build.BuildChar.value.pname
          && name_index(imemory, &pdata->BuildChar) == name_index(imemory,
                                                                  &build.
                                                                  BuildChar))
         || (r_type(&(pdata->BuildGlyph)) != t_null
             && pdata->BuildGlyph.value.pname && build.BuildGlyph.value.pname
             && name_index(imemory, &pdata->BuildGlyph) == name_index(imemory,
                                                                      &build.
                                                                      BuildGlyph))))
    {

        if (has_buildchar == true) {
            ref_assign_new(&pdata->BuildChar, &build.BuildChar);
        }
        else {
            make_null(&pdata->BuildChar);
        }

        if (has_buildglyph == true) {
            ref_assign_new(&pdata->BuildGlyph, &build.BuildGlyph);
        }
        else {
            make_null(&pdata->BuildGlyph);
        }
        if (v != NULL) {
            font_file_path =
                ref_to_string(v, imemory_global, "font file path");
        }

        code =
            FAPI_refine_font(i_ctx_p, op - 1, pfont, subfont,
                             font_file_path);

        memcpy(&I->initial_FontMatrix, &pbfont->FontMatrix,
               sizeof(gs_matrix));

        if (font_file_path != NULL) {
            gs_free_string(imemory_global, (byte *) font_file_path,
                           r_size(v) + 1, "font file path");
        }
    }
    pop(1);
    return code;
}

static ulong
array_find(const gs_memory_t *mem, ref *Encoding, ref *char_name)
{
    ulong n = r_size(Encoding), i;
    ref v;

    for (i = 0; i < n; i++)
        if (array_get(mem, Encoding, i, &v) < 0)
            break;
        else if (r_type(char_name) == r_type(&v)
                 && char_name->value.const_pname == v.value.const_pname)
            return i;
    return 0;
}

static int
zfapi_finish_render(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    gs_font *pfont;
    int code = font_param(op - 1, &pfont);

    if (code == 0) {
        gs_font_base *pbfont = (gs_font_base *) pfont;
        gs_fapi_server *I = pbfont->FAPI;
        gs_text_enum_t *penum = op_show_find(i_ctx_p);

        gs_fapi_set_servers_client_data(imemory, NULL, i_ctx_p);

        code = gs_fapi_finish_render(pfont, igs, penum, I);
        pop(2);
        I->release_char_data(I);
    }
    return code;
}

static int
ps_fapi_set_cache(gs_text_enum_t *penum, const gs_font_base *pbfont,
                  const gs_string *char_name, gs_glyph cid,
                  const double pwidth[2], const gs_rect *pbbox,
                  const double Metrics2_sbw_default[4], bool *imagenow)
{
    i_ctx_t *i_ctx_p = (i_ctx_t *) pbfont->FAPI->client_ctx_p;
    op_proc_t exec_cont = 0;    /* dummy - see below */
    int code = 0;

    if (cid < GS_MIN_CID_GLYPH) {
        ref cname;

        make_string(&cname, avm_foreign | a_readonly, char_name->size,
                    char_name->data);
        code =
            zchar_set_cache(i_ctx_p, pbfont, &cname, NULL, pwidth, pbbox,
                            zfapi_finish_render, &exec_cont,
                            Metrics2_sbw_default);
    }
    else {
        ref cidref;

        make_int(&cidref, (cid - GS_MIN_CID_GLYPH));
        code = zchar_set_cache(i_ctx_p, pbfont, &cidref, NULL, pwidth, pbbox,
                               zfapi_finish_render, &exec_cont,
                               Metrics2_sbw_default);
    }

    if (code >= 0 && exec_cont != NULL) {
        *imagenow = true;
    }
    else {
        *imagenow = false;
    }
    /* We ignore the value of exec_cont here, and leave it up to
     * gs_fapi_do_char() to do the "right" thing based on the
     * return value
     */
    return (code);
}


static const byte *
find_substring(const byte *where, int length, const char *what)
{
    int l = strlen(what);
    int n = length - l;
    const byte *p = where;

    for (; n >= 0; n--, p++)
        if (!memcmp(p, what, l))
            return p;
    return NULL;
}

static int
ps_get_glyphname_or_cid(gs_text_enum_t *penum,
                        gs_font_base *pbfont, gs_string *charstring,
                        gs_string *name, gs_glyph ccode,
                        gs_string *enc_char_name, char *font_file_path,
                        gs_fapi_char_ref *cr, bool bCID)
{
    ref *pdr = pfont_dict(pbfont);
    int client_char_code = ccode;
    ref char_name, cname_str;
    int code = 0;
    gs_fapi_server *I = pbfont->FAPI;
    bool is_TT_from_type42 = (pbfont->FontType == ft_TrueType && font_file_path == NULL);
    bool is_glyph_index = false;
    bool is_embedded_type1 =
        ((pbfont->FontType == ft_encrypted
          || pbfont->FontType == ft_encrypted2) && font_file_path == NULL);
    i_ctx_t *i_ctx_p = (i_ctx_t *) I->client_ctx_p;
    bool unicode_cp = false;

    /* Obtain the character name : */
    if (bCID) {
        if (pbfont->FontType == ft_CID_TrueType && font_file_path) {
            ref *pdr2, *fidr, *dummy;
            pdr2 = pfont_dict(gs_rootfont(igs));
            if (dict_find_string(pdr2, "FontInfo", &fidr) > 0 &&
                dict_find_string(fidr, "GlyphNames2Unicode", &dummy) > 0)
            {
                unsigned char uc[4] = {0};
                unsigned int cc = 0;
                int i, l;
                if (penum->text.operation & TEXT_FROM_SINGLE_CHAR) {
                    cc = penum->text.data.d_char;
                } else if (penum->text.operation & TEXT_FROM_SINGLE_GLYPH) {
                    cc = penum->text.data.d_glyph - GS_MIN_CID_GLYPH;
                }
                else {
                    byte *c = (byte *)&penum->text.data.bytes[penum->index - penum->bytes_decoded];
                    for (i = 0; i < penum->bytes_decoded ; i++) {
                      cc |= c[i] << ((penum->bytes_decoded - 1) - i) * 8;
                    }
                }
                l = ((gs_font_base *)gs_rootfont(igs))->procs.decode_glyph(gs_rootfont(igs), cc + GS_MIN_CID_GLYPH, ccode, (unsigned short *)uc, sizeof(uc));
                if (l > 0 && l < sizeof(uc)) {
                    cc = 0;
                    for (i = 0; i < l; i++) {
                        cc |= uc[l - 1 - i] << (i * 8);
                    }
                    ccode = cc;
                    unicode_cp = true;
                }
            }
        }
        client_char_code = ccode;
        make_null(&char_name);
        enc_char_name->data = NULL;
        enc_char_name->size = 0;
    }
    else {
        if (ccode != GS_NO_CHAR) {
            /* Translate from PS encoding to char name : */
            ref *Encoding;

            client_char_code = ccode;
            if (dict_find_string(pdr, "Encoding", &Encoding) > 0 &&
                (r_has_type(Encoding, t_array) ||
                 r_has_type(Encoding, t_shortarray)
                 || r_has_type(Encoding, t_mixedarray))) {
                if (array_get(imemory, Encoding, client_char_code, &char_name)
                    < 0)
                    if ((code =
                         name_ref(imemory, (const byte *)".notdef", 7,
                                  &char_name, -1)) < 0)
                        return code;
            }
            else {
                return_error(gs_error_invalidfont);
            }
        }
        else {
            code =
                names_ref(imemory->gs_lib_ctx->gs_name_table,
                          (const byte *)name->data, name->size, &char_name,
                          0);

        }
        /* We need to store the name as we get it (from the Encoding array), in case it's
         * had the name extended (with "~GS~xx"), we'll remove the extension before passing
         * it to the renderer for a disk based font. But the metrics dictionary may have
         * been constructed using the extended name....
         */
        if (!r_has_type(&char_name, t_name))
            return_error(gs_error_invalidfont);
        name_string_ref(imemory, &char_name, &cname_str);
        enc_char_name->data = cname_str.value.bytes;
        enc_char_name->size = r_size(&cname_str);
    }

    /* Obtain the character code or glyph index : */
    cr->char_codes_count = 1;
    if (bCID) {
        if (font_file_path != NULL) {
            ref *Decoding, *TT_cmap = NULL, *SubstNWP;
            ref src_type, dst_type;
            uint c = 0;

            is_glyph_index = true;

            if (dict_find_string(pdr, "Decoding", &Decoding) <= 0
                || !r_has_type(Decoding, t_dictionary))
                return_error(gs_error_invalidfont);
            if (dict_find_string(pdr, "SubstNWP", &SubstNWP) <= 0
                || !r_has_type(SubstNWP, t_array))
                return_error(gs_error_invalidfont);
            if (dict_find_string(pdr, "TT_cmap", &TT_cmap) <= 0
                || !r_has_type(TT_cmap, t_dictionary)) {
                ref *DecodingArray, char_code, char_code1, ih;
                int i = client_char_code % 256, n;

                make_int(&ih, client_char_code / 256);
                /* Check the Decoding array for this block of CIDs */
                if (dict_find(Decoding, &ih, &DecodingArray) <= 0
                    || !r_has_type(DecodingArray, t_array)
                    || array_get(imemory, DecodingArray, i, &char_code) < 0) {
                    return_error(gs_error_invalidfont);
                }

                /* Check the Decoding entry */
                if (r_has_type(&char_code, t_integer)) {
                    n = 1;
                }
                else if (r_has_type(&char_code, t_array)) {
                    DecodingArray = &char_code;
                    i = 0;
                    n = r_size(DecodingArray);
                }
                else {
                    return_error(gs_error_invalidfont);
                }

                for (; n--; i++) {
                    if (array_get(imemory, DecodingArray, i, &char_code1) < 0
                        || !r_has_type(&char_code1, t_integer)) {
                        return_error(gs_error_invalidfont);
                    }

                    c = char_code1.value.intval;
                    I->check_cmap_for_GID(I, &c);
                    if (c != 0)
                        break;
                }
            }
            else {
                ref *CIDSystemInfo;
                ref *Ordering;
                ref *fdict, *CMapDict, *CMapName, *WMode, CMapNameStr;
                char *cmapnm = NULL;
                int cmapnmlen = 0;
                int wmode = 0;
                /* leave off the -H or -V */
                const char * const utfcmap = "Identity-UTF16";
                int utfcmaplen = strlen(utfcmap);

                fdict = pfont_dict(gs_rootfont(igs));
                code = dict_find_string(fdict, "CMap", &CMapDict);
                if (code > 0 && r_has_type(CMapDict, t_dictionary)) {
                    code = dict_find_string(CMapDict, "WMode", &WMode);
                    if (code > 0 && r_has_type(WMode, t_integer)) {
                        wmode = WMode->value.intval;
                    }
                    code = dict_find_string(CMapDict, "CMapName", &CMapName);
                    if (code > 0 && r_has_type(CMapName, t_name)) {
                        name_string_ref(imemory, CMapName, &CMapNameStr);
                        cmapnm = (char *)CMapNameStr.value.bytes;
                        cmapnmlen = r_size(&CMapNameStr);
                    }
                }
                /* We only have to lookup the char code if we're *not* using an identity ordering
                   with the exception of Identity-UTF16 which is a different beast altogether */
                if (unicode_cp || (cmapnmlen > 0 && !strncmp(cmapnm, utfcmap, cmapnmlen > utfcmaplen ? utfcmaplen : cmapnmlen))
                    || (dict_find_string(pdr, "CIDSystemInfo", &CIDSystemInfo) > 0
                    && r_has_type(CIDSystemInfo, t_dictionary)
                    && dict_find_string(CIDSystemInfo, "Ordering",
                                        &Ordering) > 0
                    && r_has_type(Ordering, t_string)
                    && strncmp((const char *)Ordering->value.bytes,
                               "Identity", 8) != 0)) {

                    if ((code =
                         cid_to_TT_charcode(imemory, Decoding, TT_cmap,
                                            SubstNWP, client_char_code, &c,
                                            &src_type, &dst_type)) < 0) {
                        return code;
                    }
                }
                else {
                    if (pbfont->FontType == ft_CID_TrueType) {
                        c = ((gs_font_cid2 *)pbfont)->cidata.CIDMap_proc(((gs_font_cid2 *)pbfont),
                                                  client_char_code + GS_MIN_CID_GLYPH);
                    }
                    else {
                        c = client_char_code;
                    }
                }
                if (pbfont->FontType == ft_CID_TrueType)
                    c = ((gs_font_cid2 *)pbfont)->data.substitute_glyph_index_vertical((gs_font_type42 *)pbfont, c, wmode, ccode);
            }
            if (pbfont->FontType == ft_CID_TrueType && c == 0 && TT_cmap) {
                ref cc32;
                ref *gid;
                make_int(&cc32, 32);
                if (dict_find(TT_cmap, &cc32, &gid) > 0)
                    c = gid->value.intval;
            }
            cr->char_codes[0] = c;
            /* fixme : process the narrow/wide/proportional mapping type,
               using src_type, dst_type. Should adjust the 'matrix' above.
               Call get_font_proportional_feature for proper choice.
             */
        }
        else {
            ref *CIDMap;
            byte *Map;
            int c_code = client_char_code;
            int gdb = 2;
            int i;
            ref *GDBytes = NULL;

            if ((dict_find_string(pdr, "GDBytes", &GDBytes) > 0)
                && r_has_type(GDBytes, t_integer)) {
                gdb = GDBytes->value.intval;
            }

            /* The PDF Reference says that we should use a CIDToGIDMap, but the PDF
             * interpreter converts this into a CIDMap (see pdf_font.ps, processCIDToGIDMap)
             */
            if (dict_find_string(pdr, "CIDMap", &CIDMap) > 0
                && !r_has_type(CIDMap, t_name) && (r_has_type(CIDMap, t_array)
                                                   || r_has_type(CIDMap,
                                                                 t_string))) {

                if (r_has_type(CIDMap, t_array)) {

                    /* Too big for single string, so its an array of 2 strings */
                    code = string_array_access_proc(pbfont->memory, CIDMap, 1,
                                                    client_char_code * (ulong)gdb,
                                                    gdb, NULL, NULL,
                                                    (const byte **)&Map);
                }
                else {
                    if (CIDMap->tas.rsize <= c_code * gdb) {
                        c_code = 0;
                    }
                    Map = &CIDMap->value.bytes[c_code * gdb];
                }
                cr->char_codes[0] = 0;
                is_glyph_index = true;
                if (code >= 0) {
                    for (i = 0; i < gdb; i++) {
                        cr->char_codes[0] = (cr->char_codes[0] << 8) + Map[i];
                    }
                }
                else {
                    ref *cstr, *refcode;
                    code = dict_find_string(pdr, "CharStrings", &cstr);
                    if (code > 0) {
                        code = dict_find_string(cstr, ".notdef", &refcode);
                        if (code > 0) {
                            cr->char_codes[0] = refcode->value.intval;
                        }
                    }
                }
            }
            else
                cr->char_codes[0] = client_char_code;
        }
    }
    else if (is_TT_from_type42) {
        /* This font must not use 'cmap', so compute glyph index from CharStrings : */
        ref *CharStrings, *glyph_index, *cmaptab;

        if (dict_find_string(pdr, "TT_cmap", &cmaptab) > 0 &&
           r_has_type(cmaptab, t_dictionary)) {
           const char *nd = ".notdef";

           if (enc_char_name->size >= strlen(nd) &&
               enc_char_name->data[0] == nd[0] &&
               !memcmp(enc_char_name->data, nd, strlen(nd))) {
               ref ccref, *gidref, boolref;
               make_int(&ccref, ccode);
               if (dict_find(cmaptab, &ccref, &gidref) > 0 &&
                   r_has_type(gidref, t_integer) &&
                   gidref->value.intval == 0) {
                   make_bool(&boolref, true);
               }
               else {
                   make_bool(&boolref, false);
               }
               dict_put_string(pdr, ".render_notdef", &boolref, NULL);
           }
        }

        if (dict_find_string(pdr, "CharStrings", &CharStrings) <= 0
            || !r_has_type(CharStrings, t_dictionary))
            return_error(gs_error_invalidfont);
        if ((dict_find(CharStrings, &char_name, &glyph_index) <= 0)
            || r_has_type(glyph_index, t_null)) {
#ifdef DEBUG
            ref *pvalue;

            if (gs_debug_c('1')
                && (dict_find_string(systemdict, "QUIET", &pvalue)) > 0
                && (r_has_type(pvalue, t_boolean)
                    && pvalue->value.boolval == false)) {
                char *glyphn;

                name_string_ref(imemory, &char_name, &char_name);

                glyphn =
                    ref_to_string(&char_name, imemory,
                                  "ps_get_glyphname_or_cid");
                if (glyphn) {
                    dmprintf2(imemory, " Substituting .notdef for %s in the font %s \n",
                             glyphn, pbfont->font_name.chars);
                    gs_free_string(imemory, (byte *) glyphn,
                                   strlen(glyphn) + 1,
                                   "ps_get_glyphname_or_cid");
                }
            }
#endif

            cr->char_codes[0] = 0;      /* .notdef */
            if ((code =
                 name_ref(imemory, (const byte *)".notdef", 7, &char_name,
                          -1)) < 0)
                return code;
        }
        else if (r_has_type(glyph_index, t_integer)) {
            cr->char_codes[0] = glyph_index->value.intval;
        }
        else {
#if 1                           /* I can't find this ever being used, no idea what it's for..... */
            os_ptr op = osp;

            /* Check execution stack has space for BuldChar proc and finish_render */
            check_estack(2);
            /* check space and duplicate the glyph index for BuildChar */
            check_op(1);
            push(1);
            ref_assign_inline(op, op - 1);
            /* Come back to fapi_finish_render after running the BuildChar */
            push_op_estack(zfapi_finish_render);
            ++esp;
            ref_assign(esp, glyph_index);
            return o_push_estack;
#else
            return (gs_error_invalidfont);
#endif
        }
        is_glyph_index = true;
    }
    else if (is_embedded_type1) {
        /*  Since the client passes charstring by callback using I->ff.char_data,
           the client doesn't need to provide a good cr here.
           Perhaps since UFST uses char codes as glyph cache keys (UFST 4.2 cannot use names),
           we provide font char codes equal to document's char codes.
           This trick assumes that Encoding can't point different glyphs
           for same char code. The last should be true due to
           PLRM3, "5.9.4 Subsetting and Incremental Definition of Glyphs".
         */
        if (ccode != GS_NO_CHAR) {
            cr->char_codes[0] = client_char_code;
        }
        else {
            /*
             * Reverse Encoding here, because it can be an incremental one.
             * Note that this can cause problems with UFST (see the comment above),
             * if the encoding doesn't contain the glyph name rendered with glyphshow.
             */
            ref *Encoding;
            ref glyph;

            if ((code = name_ref(pbfont->memory, name->data, name->size, &glyph, false)) < 0)
                return code;

            if (dict_find_string(osp - 1, "Encoding", &Encoding) > 0) {
                cr->char_codes[0] =
                    (uint) array_find(imemory, Encoding, &glyph);
            }
            else
                return_error(gs_error_invalidfont);
        }
    }
    else {                      /* a non-embedded font, i.e. a disk font */
        bool can_retrieve_char_by_name = false;
        const byte *p;

        obj_string_data(imemory, &char_name, &cr->char_name,
                        &cr->char_name_length);
        p = find_substring(cr->char_name, cr->char_name_length,
                           gx_extendeg_glyph_name_separator);
        if (p != NULL) {
            cr->char_name_length = p - cr->char_name;
            if ((code = name_ref(pbfont->memory, cr->char_name,
                             cr->char_name_length, &char_name, true)) < 0)
                return code;
        }
        if ((code =
             renderer_retcode(imemory, I,
                              I->can_retrieve_char_by_name(I, &I->ff, cr,
                                                           &can_retrieve_char_by_name)))
            < 0)
            return code;

        if (!can_retrieve_char_by_name) {
            /* Translate from char name to encoding used with 3d party font technology : */
            ref *Decoding, *char_code;

            if (dict_find_string(osp - 1, "Decoding", &Decoding) > 0
                && r_has_type(Decoding, t_dictionary)) {
                if (dict_find(Decoding, &char_name, &char_code) > 0) {
                    code = 0;
                    if (r_has_type(char_code, t_integer)) {
                        int c_code;
                        int_param(char_code, 0xFFFF, &c_code);
                        cr->char_codes[0] = (gs_glyph)c_code;
                    }
                    else if (r_has_type(char_code, t_array)
                             || r_has_type(char_code, t_shortarray)) {
                        int i;
                        ref v;

                        cr->char_codes_count = r_size(char_code);
                        if (cr->char_codes_count > count_of(cr->char_codes))
                            code = gs_note_error(gs_error_rangecheck);
                        if (code >= 0) {
                            for (i = 0; i < cr->char_codes_count; i++) {
                                code = array_get(imemory, char_code, i, &v);
                                if (code < 0)
                                    break;
                                if (!r_has_type(char_code, t_integer)) {
                                    code = gs_note_error(gs_error_rangecheck);
                                    break;
                                }
                                cr->char_codes[i] = v.value.intval;
                            }
                        }
                    }
                    else {
                        code = gs_note_error(gs_error_rangecheck);
                    }
                    if (code < 0) {
                        char buf[16];
                        int l = cr->char_name_length;

                        if (l > sizeof(buf) - 1) {
                            l = sizeof(buf) - 1;
                        }
                        memcpy(buf, cr->char_name, l);
                        buf[l] = 0;
                        emprintf1(imemory,
                                  "Wrong decoding entry for the character '%s'.\n",
                                  buf);
                        return_error(gs_error_rangecheck);
                    }
                }
            }
        }
    }

    /* Provide glyph data for renderer : */
    /* Occasionally, char_name is already a glyph index to pass to the rendering engine
     * so don't treat it as a name object.
     * I believe this will only happen with a TTF/Type42, but checking the object type
     * is cheap, and covers all font type eventualities.
     */
    if (!I->ff.is_cid && r_has_type(&char_name, t_name)) {
        ref sname;

        name_string_ref(imemory, &char_name, &sname);
        I->ff.char_data = sname.value.const_bytes;
        I->ff.char_data_len = r_size(&sname);
    }
    else if (I->ff.is_type1) {
        I->ff.char_data = charstring;
    }

    cr->is_glyph_index = is_glyph_index;
    cr->client_char_code = client_char_code;

    return (code);
}


static int
FAPI_char(i_ctx_t *i_ctx_p, bool bBuildGlyph, ref *charstring)
{                               /* Stack : <font> <code|name> --> - */
    os_ptr op = osp;
    ref *pdr = op - 1;
    ref *v;
    char *font_file_path = NULL;
    gs_font *pfont;
    int code;

    check_op(2);
    code = font_param(osp - 1, &pfont);

    if (code == 0) {
        gs_font_base *pbfont = (gs_font_base *) pfont;
        bool bCID = (FAPI_ISCIDFONT(pbfont) || charstring != NULL);
        int subfont;
        gs_fapi_server *I = pbfont->FAPI;
        gs_text_enum_t *penum = op_show_find(i_ctx_p);
        gs_string char_string, *c_string_p = NULL;
        gs_string char_name, *c_name_p = NULL;
        gs_glyph cindex = GS_NO_CHAR;
        ref gname;

        if (I == NULL)
            return_error(gs_error_invalidfont);

        /* initialise the FAPI font, this includes language specific stuff */
        I->ff = ps_ff_stub;

        I->client_ctx_p = i_ctx_p;

        if (bBuildGlyph && !bCID) {
            if (r_type(op) != t_name) {
                name_enter_string(imemory, ".notdef", op);
            }
            check_type(*op, t_name);

            name_string_ref(imemory, op, &gname);
            c_name_p = &char_name;
            c_name_p->data = gname.value.bytes;
            c_name_p->size = r_size(&gname);

        }
        else {
            int chint;
            if (bBuildGlyph && pbfont->FontType == ft_CID_TrueType
                && r_has_type(op, t_name)) {
                ref *chstrs, *chs;

                /* This logic is lifted from %Type11BuildGlyph in gs_cidfn.ps
                 * Note we only have to deal with mistakenly being given a name object
                 * here, the out of range CID is handled later
                 */
                if ((dict_find_string(op - 1, "CharStrings", &chstrs)) <= 0) {
                    return_error(gs_error_undefined);
                }

                if ((dict_find_string(chstrs, ".notdef", &chs)) <= 0) {
                    return_error(gs_error_undefined);
                }
                ref_assign_inline(op, chs);
            }

            make_null(&gname);
            check_type(*op, t_integer);
            int_param(op, 0xFFFF, (int *)&chint);
            cindex = chint;
        }

        if (dict_find_string(pdr, "SubfontId", &v) > 0
            && r_has_type(v, t_integer))
            subfont = v->value.intval;
        else
            subfont = 0;

        if (dict_find_string(osp - 1, "Path", &v) > 0
            && r_has_type(v, t_string)) {
            font_file_path = ref_to_string(v, imemory, "font file path");
        }

        if (charstring) {
            c_string_p = &char_string;
            c_string_p->data = charstring->value.bytes;
            c_string_p->size = r_size(charstring);
        }

        code =
            gs_fapi_do_char(pfont, igs, penum, font_file_path,
                            bBuildGlyph, c_string_p, c_name_p, (gs_char)cindex, cindex,
                            subfont);
        if (font_file_path != NULL) {
            gs_free_string(imemory, (byte *) font_file_path, r_size(v) + 1,
                           "font file path");
        }
        /* This handles the situation where a charstring has been replaced with a PS procedure.
         * against the rules, but not *that* rare.
         * It's also something that GS does internally to simulate font styles.
         */
        if (code == gs_error_unregistered) {
            os_ptr op = osp;
            ref *proc = NULL, gname;

            if (I->ff.is_type1
                && (get_charstring(&I->ff, cindex, &proc, &gname) >= 0)
                && proc != NULL && (r_has_type(proc, t_array)
                    || r_has_type(proc, t_mixedarray))) {
                push(2);
                ref_assign(op - 1, &gname);
                ref_assign(op, proc);
                return (zchar_exec_char_proc(i_ctx_p));
            }
            else {
                return_error(gs_error_invalidfont);
            }
        }
    }
    /* We've already imaged teh glyph, pop the operands */
    if (code == 0)
        pop(2);
    return code;
}

static int
zFAPIBuildGlyph9(i_ctx_t *i_ctx_p)
{
    /*  The alghorithm is taken from %Type9BuildGlyph - see gs_cidfn.ps .  */
    os_ptr lop, op = osp;
    int cid, code;
    avm_space s = ialloc_space(idmemory);
    ref font9 = *pfont_dict(gs_currentfont(igs));
    ref *rFDArray, f;
    int font_index;

    check_op(2);
    check_type(op[0], t_integer);
    check_type(op[-1], t_dictionary);
    cid = op[0].value.intval;
    push(2);
    op[-1] = *pfont_dict(gs_currentfont(igs));
    op[0] = op[-2];             /* <font0> <cid> <font9> <cid> */
    ialloc_set_space(idmemory, (r_is_local(op - 3) ? avm_global : avm_local));  /* for ztype9mapcid */

    /* stack: <font0> <cid> <font9> <cid> */
    if ((code = ztype9mapcid(i_ctx_p)) < 0)
        return code;            /* <font0> <cid> <charstring> <font_index> */
    /* fixme: what happens if the charstring is absent ?
       Can FDArray contain 'null' (see %Type9BuildGlyph in gs_cidfn.ps)? */
    font_index = op[0].value.intval;
    if (dict_find_string(&font9, "FDArray", &rFDArray) <= 0
        || r_type(rFDArray) != t_array)
        return_error(gs_error_invalidfont);
    if (array_get(imemory, rFDArray, font_index, &f) < 0
        || r_type(&f) != t_dictionary)
        return_error(gs_error_invalidfont);

    op[0] = op[-2];
    op[-2] = op[-1];            /* Keep the charstring on ostack for the garbager. */
    op[-1] = f;                 /* <font0> <charstring> <subfont> <cid> */
    if ((code = FAPI_char(i_ctx_p, true, op - 2)) < 0)
        return code;
    /* stack: <font0> <charstring> */

    lop = osp;
    if (code == 5) {
        int i, ind = (lop - op);

        op = osp;

        for (i = ind; i >= 0; i--) {
            op[-i - 2] = op[-i];
        }
        pop(2);
    }
    else if (code < 0) {        /* <font0> <dirty> <dirty> <dirty> */
        /* Adjust ostack for the correct error handling : */
        make_int(op - 2, cid);
        pop(2);                 /* <font0> <cid> */
    }
    else if (code != 5) {       /* <font0> <dirty> */


        pop(2);                 /* */
        /*  Note that this releases the charstring, and it may be garbage-collected
           before the interpreter calls fapi_finish_render. This requires the server
           to keep glyph raster internally between calls to get_char_raster_metrics
           and get_char_raster. Perhaps UFST cannot provide metrics without
           building a raster, so this constraint actually goes from UFST.
         */
    }
    ialloc_set_space(idmemory, s);
    return code;
}

/* <font> <code> .FAPIBuildChar - */
static int
zFAPIBuildChar(i_ctx_t *i_ctx_p)
{
    return FAPI_char(i_ctx_p, false, NULL);
}

/* non-CID : <font> <code> .FAPIBuildGlyph - */
/*     CID : <font> <name> .FAPIBuildGlyph - */
static int
zFAPIBuildGlyph(i_ctx_t *i_ctx_p)
{
    return FAPI_char(i_ctx_p, true, NULL);
}


/* <font_dict> .FAPIpassfont bool <font_dict> */
/* must insert /FAPI to font dictionary */
static int
zFAPIpassfont(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    gs_font *pfont;
    int code;
    char *font_file_path = NULL;
    ref *v;
    char *xlatmap = NULL;
    char *fapi_request = NULL;
    char *fapi_id = NULL;
    ref reqstr;
    int subfont;

    check_op(1);
    /* Normally embedded fonts have no Path, but if a CID font is
     * emulated with a TT font, and it is hooked with FAPI,
     * the path presents and is neccessary to access the full font data.
     */
    check_type(*op, t_dictionary);

    code = font_param(osp, &pfont);
    if (code < 0)
        return code;

    if (dict_find_string(op, "SubfontId", &v) > 0
        && r_has_type(v, t_integer))
        subfont = v->value.intval;
    else
        subfont = 0;

    code = FAPI_get_xlatmap(i_ctx_p, &xlatmap); /* Useful for emulated fonts hooked with FAPI. */
    if (code < 0)
        return code;

    /* If the font dictionary contains a FAPIPlugInReq key, the the PS world wants us
     * to try to use a specific FAPI plugin, so find it, and try it....
     */
    if (dict_find_string(op, "FAPIPlugInReq", &v) > 0 && r_type(v) == t_name) {

        name_string_ref(imemory, v, &reqstr);

        fapi_request = ref_to_string(&reqstr, imemory, "zFAPIpassfont");
    }

    if (dict_find_string(op, "Path", &v) > 0 && r_has_type(v, t_string))
        font_file_path = ref_to_string(v, imemory_global, "font file path");

    gs_fapi_set_servers_client_data(imemory, &ps_ff_stub, i_ctx_p);

    code =
        gs_fapi_passfont(pfont, subfont, font_file_path, NULL, fapi_request, xlatmap,
                         &fapi_id, NULL, (gs_fapi_get_server_param_callback)ps_get_server_param);

    if (font_file_path != NULL)
        gs_free_string(imemory_global, (byte *) font_file_path, r_size(v) + 1,
                       "font file path");

    if (fapi_request != NULL)
        gs_free_string(imemory, (byte *) fapi_request,
                       strlen(fapi_request) + 1, "do_FAPIpassfont");
    if (code < 0 && code != gs_error_invalidaccess)
        return code;

    if (code >= 0 && fapi_id != NULL) {
        ref FAPI_ID;

        if ((code =
             name_ref(imemory, (const byte *)fapi_id,
                      strlen(fapi_id), &FAPI_ID, false)) < 0)
            return code;
        if ((code = dict_put_string(op, "FAPI", &FAPI_ID, NULL)) < 0)
            return code;        /* Insert FAPI entry to font dictionary. */
    }
    push(1);
    make_bool(op, (fapi_id != NULL));
    return 0;
}

const op_def zfapi_op_defs[] = {
    {"1.FAPIavailable", zFAPIavailable},
    {"2.FAPIpassfont", zFAPIpassfont},
    {"2.FAPIrebuildfont", zFAPIrebuildfont},
    {"2.FAPIBuildChar", zFAPIBuildChar},
    {"2.FAPIBuildGlyph", zFAPIBuildGlyph},
    {"2.FAPIBuildGlyph9", zFAPIBuildGlyph9},
    op_def_end(0)
};
