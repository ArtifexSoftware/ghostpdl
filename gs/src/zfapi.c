/* Copyright (C) 1991, 2000 Aladdin Enterprises.  All rights reserved.
  
  This file is part of AFPL Ghostscript.
  
  AFPL Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author or
  distributor accepts any responsibility for the consequences of using it, or
  for whether it serves any particular purpose or works at all, unless he or
  she says so in writing.  Refer to the Aladdin Free Public License (the
  "License") for full details.
  
  Every copy of AFPL Ghostscript must include a copy of the License, normally
  in a plain ASCII text file named PUBLIC.  The License grants you the right
  to copy, modify and redistribute AFPL Ghostscript, but only under certain
  conditions described in the License.  Among other things, the License
  requires that the copyright notice and this notice be preserved on all
  copies.
*/

/*$Id$ */
/* Font API client */

#include "memory_.h"
#include "math_.h"
#include "ghost.h"
#include "gp.h"
#include "oper.h"
#include "gxdevice.h"
#include "gxfont.h"
#include "gxfont1.h"
#include "gxchar.h"
#include "gxpath.h"
#include "gxfcache.h"
#include "gxchrout.h"
#include "gscoord.h"
#include "gspaint.h"
#include "gsfont.h"
#include "gspath.h"
#include "bfont.h"
#include "dstack.h"
#include "ichar.h"
#include "idict.h"
#include "iddict.h"
#include "iname.h"
#include "ifont.h"
#include "igstate.h"
#include "icharout.h"
#include "ifapi.h"
#include "iplugin.h"
#include "store.h"
#include "gzstate.h"
#include "gdevpsf.h"
#include "stream.h"		/* for files.h */
#include "files.h"
#include "gscrypt1.h"
#include "gxfcid.h"

#define CheckRET(a) { int x = a; if(x < 0) return x; }

/* -------------------------------------------------------- */

typedef struct FAPI_outline_handler_s {
    struct gx_path_s *path;
    fixed x0, y0;
    bool close_path, need_close; /* This stuff fixes unclosed paths being rendered with UFST */
} FAPI_outline_handler;

private inline int import_shift(int x, int n)
{   return n > 0 ? x << n : x >> -n;
}

private int add_closepath(FAPI_path *I)
{   FAPI_outline_handler *olh = (FAPI_outline_handler *)I->olh;
    olh->need_close = false;
    return gx_path_close_subpath_notes(olh->path, 0);
}

private int add_move(FAPI_path *I, FracInt x, FracInt y)
{   FAPI_outline_handler *olh = (FAPI_outline_handler *)I->olh;
    if (olh->need_close && olh->close_path)
        CheckRET(add_closepath(I));
    olh->need_close = false;
    return gx_path_add_point(olh->path, import_shift(x, I->shift) + olh->x0, -import_shift(y, I->shift) + olh->y0);
}

private int add_line(FAPI_path *I, FracInt x, FracInt y)
{   FAPI_outline_handler *olh = (FAPI_outline_handler *)I->olh;
    olh->need_close = true;
    return gx_path_add_line_notes(olh->path, import_shift(x, I->shift) + olh->x0, -import_shift(y, I->shift) + olh->y0, 0);
}

private int add_curve(FAPI_path *I, FracInt x0, FracInt y0, FracInt x1, FracInt y1, FracInt x2, FracInt y2)
{   FAPI_outline_handler *olh = (FAPI_outline_handler *)I->olh;
    olh->need_close = true;
    return gx_path_add_curve_notes(olh->path, import_shift(x0, I->shift) + olh->x0, -import_shift(y0, I->shift) + olh->y0, 
					      import_shift(x1, I->shift) + olh->x0, -import_shift(y1, I->shift) + olh->y0, 
					      import_shift(x2, I->shift) + olh->x0, -import_shift(y2, I->shift) + olh->y0, 0);
}

private FAPI_path path_interface_stub = { NULL, 0, add_move, add_line, add_curve, add_closepath };

private inline bool IsCIDFont(const gs_font_base *pbfont)
{   return (pbfont->FontType == ft_CID_encrypted ||
            pbfont->FontType == ft_CID_user_defined ||
            pbfont->FontType == ft_CID_TrueType);
    /* The font type 10 (ft_CID_user_defined) must not pass to FAPI. */
}

private inline bool IsType1GlyphData(const gs_font_base *pbfont)
{   return pbfont->FontType == ft_encrypted ||
           pbfont->FontType == ft_CID_encrypted;
}

/* -------------------------------------------------------- */

typedef struct sfnts_reader_s sfnts_reader;
struct sfnts_reader_s { 
    ref *sfnts;
    const byte *p;
    long index;
    uint offset;
    uint length;
    bool error;
    byte (*rbyte)(sfnts_reader *r);
    ushort (*rword)(sfnts_reader *r);
    ulong (*rlong)(sfnts_reader *r);
    void (*rstring)(sfnts_reader *r, byte *v, int length);
    void (*seek)(sfnts_reader *r, ulong pos);
};

private void sfnts_next_elem(sfnts_reader *r)
{   ref s;
    if (r->error)
        return;
    r->index++;
    r->error |= (array_get(r->sfnts, r->index, &s) < 0);
    if (r->error)
        return;
    r->p = s.value.const_bytes;
    r->length = r_size(&s) & ~(uint)1; /* See Adobe Technical Note # 5012, section 4.2. */
    r->offset = 0;
}

private inline byte sfnts_reader_rbyte_inline(sfnts_reader *r)
{   if (r->offset >= r->length)
        sfnts_next_elem(r);
    return (r->error ? 0 : r->p[r->offset++]);
}

private byte sfnts_reader_rbyte(sfnts_reader *r) /* old compiler compatibility */
{   return sfnts_reader_rbyte_inline(r);
}

private ushort sfnts_reader_rword(sfnts_reader *r)
{   return (sfnts_reader_rbyte_inline(r) << 8) + sfnts_reader_rbyte_inline(r);
}

private ulong sfnts_reader_rlong(sfnts_reader *r)
{   return (sfnts_reader_rbyte_inline(r) << 24) + (sfnts_reader_rbyte_inline(r) << 16) + 
           (sfnts_reader_rbyte_inline(r) << 8) + sfnts_reader_rbyte_inline(r);
}

private void sfnts_reader_rstring(sfnts_reader *r, byte *v, int length)
{   if (length < 0)
        return;
    while (!r->error) {
        int l = min(length, r->length - r->offset);
        memcpy(v, r->p + r->offset, l);
        r->p += l;
        length -= l;
        if (length <= 0)
            return;
        sfnts_next_elem(r);
    }
}

private void sfnts_reader_seek(sfnts_reader *r, ulong pos)
{   /* fixme : optimize */
    ulong skipped = 0;
    r->index = -1;
    sfnts_next_elem(r);
    while (skipped + r->length < pos && !r->error) {
        skipped += r->length;
        sfnts_next_elem(r);
    }
    r->offset = pos - skipped;
}

private void sfnts_reader_init(sfnts_reader *r, ref *pdr)
{   r->rbyte = sfnts_reader_rbyte;
    r->rword = sfnts_reader_rword;
    r->rlong = sfnts_reader_rlong;
    r->rstring = sfnts_reader_rstring;
    r->seek = sfnts_reader_seek;
    r->index = -1;
    r->error = false;
    if (r_type(pdr) != t_dictionary ||
        dict_find_string(pdr, "sfnts", &r->sfnts) <= 0)
        r->error = true;
    sfnts_next_elem(r);
}

/* -------------------------------------------------------- */

typedef struct sfnts_writer_s sfnts_writer;
struct sfnts_writer_s {
    byte *buf, *p;
    int buf_size;
    void (*wbyte)(sfnts_writer *w, byte v);
    void (*wword)(sfnts_writer *w, ushort v);
    void (*wlong)(sfnts_writer *w, ulong v);
    void (*wstring)(sfnts_writer *w, byte *v, int length);
};

private void sfnts_writer_wbyte(sfnts_writer *w, byte v)
{   if (w->buf + w->buf_size < w->p + 1)
        return; /* safety */
    w->p[0] = v;
    w->p++;
}

private void sfnts_writer_wword(sfnts_writer *w, ushort v)
{   if (w->buf + w->buf_size < w->p + 2)
        return; /* safety */
    w->p[0] = v / 256;
    w->p[1] = v % 256;
    w->p += 2;
}

private void sfnts_writer_wlong(sfnts_writer *w, ulong v)
{   if (w->buf + w->buf_size < w->p + 4)
        return; /* safety */
    w->p[0] = v >> 24;
    w->p[1] = (v >> 16) & 0xFF;
    w->p[2] = (v >>  8) & 0xFF;
    w->p[3] = v & 0xFF;
    w->p += 4;
}

private void sfnts_writer_wstring(sfnts_writer *w, byte *v, int length)
{   if (w->buf + w->buf_size < w->p + length)
        return; /* safety */
    memcpy(w->p, v, length);
    w->p += length;
}

private const sfnts_writer sfnts_writer_stub = {
    0, 0, 0,
    sfnts_writer_wbyte,
    sfnts_writer_wword,
    sfnts_writer_wlong,
    sfnts_writer_wstring
};

/* -------------------------------------------------------- */

private inline bool sfnts_need_copy_table(byte *tag)
{ return memcmp(tag, "glyf", 4) &&
         memcmp(tag, "glyx", 4) && /* Presents in files created by AdobePS5.dll Version 5.1.2 */
         memcmp(tag, "loca", 4) &&
         memcmp(tag, "locx", 4) && /* Presents in files created by AdobePS5.dll Version 5.1.2 */
         memcmp(tag, "cmap", 4);
}

private void sfnt_copy_table(sfnts_reader *r, sfnts_writer *w, int length)
{   byte buf[1024];
    while (length > 0 && !r->error) {
        int l = min(length, sizeof(buf));
        r->rstring(r, buf, l);
        w->wstring(w, buf, l);
        length -= l;
    }
}

private ulong sfnts_copy_except_glyf(sfnts_reader *r, sfnts_writer *w)
{   /* Note : TTC is not supported and probably is unuseful for Type 42. */
    /* This skips glyf, loca and cmap from copying. */
    struct {
        byte tag[4];
        ulong checkSum, offset, offset_new, length;
    } tables[30];
    const ushort alignment = 4; /* Not sure, maybe 2 */
    ulong version = r->rlong(r);
    ushort num_tables = r->rword(r);
    ushort i, num_tables_new = 0;
    ushort searchRange, entrySelector = 0, rangeShift, v;
    ulong size_new = 12;
    r->rword(r); /* searchRange */
    r->rword(r); /* entrySelector */
    r->rword(r); /* rangeShift */
    for (i = 0; i < num_tables; i++) {
        if (r->error)
            return 0;
        r->rstring(r, tables[i].tag, 4);
        tables[i].checkSum = r->rlong(r);
        tables[i].offset = r->rlong(r);
        tables[i].length = r->rlong(r);
        tables[i].offset_new = size_new;
        if (sfnts_need_copy_table(tables[i].tag)) {
            num_tables_new ++;
            size_new += (tables[i].length + alignment - 1) / alignment * alignment;
        }
    }
    size_new += num_tables_new * 16;
    if (w == 0)
        return size_new;

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
    for (i = 0; i < num_tables; i++)
        if (sfnts_need_copy_table(tables[i].tag)) {
            w->wstring(w, tables[i].tag, 4);
            w->wlong(w, tables[i].checkSum);
            w->wlong(w, tables[i].offset_new + num_tables_new * 16);
            w->wlong(w, tables[i].length);
        }
    for (i = 0; i < num_tables; i++)
        if (sfnts_need_copy_table(tables[i].tag)) {
            int k = tables[i].length;
            r->seek(r, tables[i].offset);
            if (r->error)
                return 0;
            if (w->p - w->buf != tables[i].offset_new + num_tables_new * 16)
                return 0; /* the algorithm consistency check */
            sfnt_copy_table(r, w, tables[i].length);
            for (; k & (alignment - 1); k++)
                w->wbyte(w, 0);
        }
    return size_new;
}

private ulong true_type_size(ref *pdr)
{   sfnts_reader r;
    sfnts_reader_init(&r, pdr);
    return sfnts_copy_except_glyf(&r, 0);
}

private ushort FAPI_FF_serialize_tt_font(FAPI_font *ff, void *buf, int buf_size)
{   ref *pdr = (ref *)ff->client_font_data2;
    sfnts_reader r;
    sfnts_writer w = sfnts_writer_stub;
    w.buf_size = buf_size;
    w.buf = w.p = buf;
    sfnts_reader_init(&r, pdr);
    if(!sfnts_copy_except_glyf(&r, &w))
        return 1;
    return r.error;
}

private inline ushort float_to_ushort(float v)
{   return (ushort)(v * 16); /* fixme : the scale may depend on renderer */
}

private ushort FAPI_FF_get_word(FAPI_font *ff, fapi_font_feature var_id, int index)
{   gs_font_type1 *pfont = (gs_font_type1 *)ff->client_font_data;
    ref *pdr = (ref *)ff->client_font_data2;
    switch((int)var_id) {
        case FAPI_FONT_FEATURE_Weight: return 0; /* wrong */
        case FAPI_FONT_FEATURE_ItalicAngle: return 0; /* wrong */
        case FAPI_FONT_FEATURE_IsFixedPitch: return 0; /* wrong */
        case FAPI_FONT_FEATURE_UnderLinePosition: return 0; /* wrong */
        case FAPI_FONT_FEATURE_UnderlineThickness: return 0; /* wrong */
        case FAPI_FONT_FEATURE_FontType: return 1;
        case FAPI_FONT_FEATURE_FontBBox: 
            switch (index) {
                case 0 : return (ushort)pfont->FontBBox.p.x;
                case 1 : return (ushort)pfont->FontBBox.p.y;
                case 2 : return (ushort)pfont->FontBBox.q.x;
                case 3 : return (ushort)pfont->FontBBox.q.y;
            }
            return 0;
        case FAPI_FONT_FEATURE_BlueValues_count: return pfont->data.BlueValues.count;
        case FAPI_FONT_FEATURE_BlueValues: return float_to_ushort(pfont->data.BlueValues.values[index]);
        case FAPI_FONT_FEATURE_OtherBlues_count: return pfont->data.OtherBlues.count;
        case FAPI_FONT_FEATURE_OtherBlues: return float_to_ushort(pfont->data.OtherBlues.values[index]);
        case FAPI_FONT_FEATURE_FamilyBlues_count: return pfont->data.FamilyBlues.count;
        case FAPI_FONT_FEATURE_FamilyBlues: return float_to_ushort(pfont->data.FamilyBlues.values[index]);
        case FAPI_FONT_FEATURE_FamilyOtherBlues_count: return pfont->data.FamilyOtherBlues.count;
        case FAPI_FONT_FEATURE_FamilyOtherBlues: return float_to_ushort(pfont->data.FamilyOtherBlues.values[index]);
        case FAPI_FONT_FEATURE_BlueShift: return float_to_ushort(pfont->data.BlueShift);
        case FAPI_FONT_FEATURE_BlueFuzz: return float_to_ushort(pfont->data.BlueShift);
        case FAPI_FONT_FEATURE_StdHW: return (pfont->data.StdHW.count == 0 ? 0 : float_to_ushort(pfont->data.StdHW.values[0])); /* UFST bug ? */
        case FAPI_FONT_FEATURE_StdVW: return (pfont->data.StdVW.count == 0 ? 0 : float_to_ushort(pfont->data.StdVW.values[0])); /* UFST bug ? */
        case FAPI_FONT_FEATURE_StemSnapH_count: return pfont->data.StemSnapH.count;
        case FAPI_FONT_FEATURE_StemSnapH: return float_to_ushort(pfont->data.StemSnapH.values[index]);
        case FAPI_FONT_FEATURE_StemSnapV_count: return pfont->data.StemSnapV.count;
        case FAPI_FONT_FEATURE_StemSnapV: return float_to_ushort(pfont->data.StemSnapV.values[index]);
        case FAPI_FONT_FEATURE_ForceBold: return pfont->data.ForceBold;
        case FAPI_FONT_FEATURE_LanguageGroup: return pfont->data.LanguageGroup;
        case FAPI_FONT_FEATURE_lenIV: return (ff->need_decrypt ? 0 : pfont->data.lenIV);
        case FAPI_FONT_FEATURE_Subrs_count: 
            {   ref *Private, *Subrs;
                if (dict_find_string(pdr, "Private", &Private) <= 0)
                    return 0;
                if (dict_find_string(Private, "Subrs", &Subrs) <= 0)
                    return 0;
                return r_size(Subrs);
            }
            return 0;
    }
    return 0;
}

private ulong FAPI_FF_get_long(FAPI_font *ff, fapi_font_feature var_id, int index)
{   gs_font_type1 *pfont = (gs_font_type1 *)ff->client_font_data;
    ref *pdr = (ref *)ff->client_font_data2;
    switch((int)var_id) {
        case FAPI_FONT_FEATURE_UniqueID: return pfont->UID.id;
        case FAPI_FONT_FEATURE_BlueScale: return (ulong)(pfont->data.BlueScale * 65536); 
        case FAPI_FONT_FEATURE_Subrs_total_size :
            {   ref *Private, *Subrs, v;
                int lenIV = max(pfont->data.lenIV, 0);
                ulong size = 0;
                long i = 0;
                if (dict_find_string(pdr, "Private", &Private) <= 0)
                    return 0;
                if (dict_find_string(Private, "Subrs", &Subrs) <= 0)
                    return 0;
                for (i = r_size(Subrs) - 1; i >= 0; i--) {
                    array_get(Subrs, i, &v);
                    size += r_size(&v) - (ff->need_decrypt ? 0 : lenIV);
                }
                return size;
            }
            return 0;
        case FAPI_FONT_FEATURE_TT_size:
            return true_type_size(pdr);
    }
    return 0;
}

private float FAPI_FF_get_float(FAPI_font *ff, fapi_font_feature var_id, int index)
{   gs_font_base *pbfont = (gs_font_base *)ff->client_font_data;
    switch((int)var_id) {
        case FAPI_FONT_FEATURE_FontMatrix:
            {   double FontMatrix_div = (ff->is_cid && !IsCIDFont(pbfont) ? 1000 : 1);
                switch(index) {
                    case 0 : return pbfont->base->FontMatrix.xx / FontMatrix_div;
                    case 1 : return pbfont->base->FontMatrix.xy / FontMatrix_div;
                    case 2 : return pbfont->base->FontMatrix.yx / FontMatrix_div;
                    case 3 : return pbfont->base->FontMatrix.yy / FontMatrix_div;
                    case 4 : return pbfont->base->FontMatrix.tx / FontMatrix_div;
                    case 5 : return pbfont->base->FontMatrix.ty / FontMatrix_div;
                }
            } 
    }
    return 0;
}

private inline void decode_bytes(byte *p, const byte *s, int l, int lenIV)
{   ushort state = 4330;
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

private ushort get_type1_data(FAPI_font *ff, ref *type1string, byte *buf, ushort buf_length)
{   gs_font_type1 *pfont = (gs_font_type1 *)ff->client_font_data;
    int lenIV = max(pfont->data.lenIV, 0);
    int length = r_size(type1string) - (ff->need_decrypt ? lenIV : 0);
    if (buf != 0) {
        int l = min(length, buf_length); /*safety */
        if (ff->need_decrypt && pfont->data.lenIV >= 0)
            decode_bytes(buf, type1string->value.const_bytes, l + lenIV, lenIV);
        else
            memcpy(buf, type1string->value.const_bytes, l);
    }
    return length;
}

private ushort FAPI_FF_get_subr(FAPI_font *ff, int index, byte *buf, ushort buf_length)
{   ref *pdr = (ref *)ff->client_font_data2;
    ref *Private, *Subrs, subr;
    if (dict_find_string(pdr, "Private", &Private) <= 0)
        return 0;
    if (dict_find_string(Private, "Subrs", &Subrs) <= 0)
        return 0;
    if (array_get(Subrs, index, &subr) < 0)
        return 0;
    return get_type1_data(ff, &subr, buf, buf_length);
}

private bool sfnt_get_glyph_offset(ref *pdr, int index, ulong *offset0, ulong *offset1)
{   /* Note : TTC is not supported and probably is unuseful for Type 42. */
    sfnts_reader r;
    struct {
        byte tag[4];
        ulong checkSum, offset, length;
    } tables[30];
    ushort num_tables, i, iloca = 0, iglyf = 0, k = 2;
    int glyf_elem_size = 2; /* fixme : take from 'head' */
    sfnts_reader_init(&r, pdr);
    r.rlong(&r); /* version */
    num_tables = r.rword(&r);
    r.rword(&r); /* searchRange */
    r.rword(&r); /* entrySelector */
    r.rword(&r); /* rangeShift */
    for (i = 0; i < num_tables && k; i++) {
        r.rstring(&r, tables[i].tag, 4);
        tables[i].checkSum = r.rlong(&r);
        tables[i].offset = r.rlong(&r);
        tables[i].length = r.rlong(&r);
        if (!memcmp(tables[i].tag, "loca", 4))
            iloca = i, k--;
        else if (!memcmp(tables[i].tag, "glyf", 4))
            iglyf = i, k--;
    }
    r.seek(&r, tables[iloca].offset + index * glyf_elem_size);
    *offset0 = tables[iglyf].offset + (glyf_elem_size == 2 ? r.rword(&r) : r.rlong(&r)) * 2;
    if (index + 1 >= tables[iloca].length / glyf_elem_size)
        *offset1 = tables[iglyf].offset + tables[iglyf].length;
    else
        *offset1 = tables[iglyf].offset + (glyf_elem_size == 2 ? r.rword(&r) : r.rlong(&r)) * 2;
    return r.error;
}

private ushort FAPI_FF_get_glyph(FAPI_font *ff, int char_code, byte *buf, ushort buf_length)
{   /* note : 2-step request */
    ref *pdr = (ref *)ff->client_font_data2;
    ushort glyph_length;
    if (ff->is_type1) {
        if (ff->is_cid) {
            ref *glyph = (ref *)ff->client_char_data;
            glyph_length = get_type1_data(ff, glyph, buf, buf_length);
        } else {
            ref *CharStrings, char_name, *glyph;
            char_name = *(ref *)ff->client_char_data;
            if (dict_find_string(pdr, "CharStrings", &CharStrings) <= 0)
                return -1;
            if (dict_find(CharStrings, &char_name, &glyph) <= 0)
                return -1;
            glyph_length = get_type1_data(ff, glyph, buf, buf_length);
        }
    } else { /* type 42 */
        ref *GlyphDirectory, glyph0, *glyph = &glyph0, glyph_index;
        if ((dict_find_string(pdr, "GlyphDirectory", &GlyphDirectory) > 0 &&
             (r_type(GlyphDirectory) == t_dictionary &&
              ( make_int(&glyph_index, char_code),
                dict_find(GlyphDirectory, &glyph_index, &glyph) > 0))) ||
            ((r_type(GlyphDirectory) == t_array &&
              array_get(GlyphDirectory, char_code, &glyph0) >= 0) &&
             r_type(glyph) == t_string)) {
            glyph_length = r_size(glyph);
            if (buf != 0 && glyph_length > 0)
                memcpy(buf, glyph->value.const_bytes, min(glyph_length, buf_length)/* safety */);
        } else {
            ulong offset0, offset1;
            bool error = sfnt_get_glyph_offset(pdr, char_code, &offset0, &offset1);
            glyph_length = (error ? -1 : offset1 - offset0);
            if (buf != 0 && !error) {
                sfnts_reader r;
                sfnts_reader_init(&r, pdr);
                r.seek(&r, offset0);
                r.rstring(&r, buf, min(glyph_length, buf_length)/* safety */);
                if (r.error)
                    glyph_length = -1;
            }
        }
    }
    return glyph_length;
}

private const FAPI_font ff_stub = {
    0, /* server_font_data */
    0, /* need_decrypt */
    0, /* font_file_path */
    0, /* subfont */
    0, /* is_type1 */
    0, /* is_cid */
    0, /* client_font_data */
    0, /* client_font_data2 */
    0, /* client_char_data */
    FAPI_FF_get_word,
    FAPI_FF_get_long,
    FAPI_FF_get_float,
    FAPI_FF_get_subr,
    FAPI_FF_get_glyph,
    FAPI_FF_serialize_tt_font
};

private int FAPI_get_xlatmap(i_ctx_t *i_ctx_p, char **xlatmap)
{   ref *pref;
    CheckRET(dict_find_string(systemdict, ".xlatmap", &pref));
    if (r_type(pref) != t_string)
        return_error(e_typecheck);
    *xlatmap = (char *)pref->value.bytes;
    /*  Note : this supposes that xlatmap doesn't move in virtual memory.
        Garbager must not be called while plugin executes get_scaled_font, get_decodingID.
        Fix some day with making copy of xlatmap in system memory.
    */
    return 0;
}

private int renderer_retcode(i_ctx_t *i_ctx_p, FAPI_server *I, FAPI_retcode rc)
{   stream *s;
    if (rc == 0)
	return 0;
    if (zget_stdout(i_ctx_p, &s) == 0) {
	uint wlen;
	const char *s0 = "Error: Font Renderer Plugin ( ", *s1 = " ) return code = ";
        char buf[10];
        sprintf(buf, "%d", rc);
	sputs(s, (const byte *)s0, strlen(s0), &wlen);
	sputs(s, (const byte *)I->ig.d->subtype, strlen(I->ig.d->subtype), &wlen);
	sputs(s, (const byte *)s1, strlen(s1), &wlen);
	sputs(s, (const byte *)buf, strlen(buf), &wlen);
	sputs(s, (const byte *)"\n", 1, &wlen);
    }
    return rc < 0 ? rc : e_invalidfont;
}

private int zFAPIavailable(i_ctx_t *i_ctx_p)
{   i_plugin_holder *h = i_plugin_get_list(i_ctx_p);
    bool available = true;
    os_ptr op = osp;
    for (; h != 0; h = h->next)
       	if (!strcmp(h->I->d->type,"FAPI"))
            goto found;
    available = false;
    found :
    push(1);
    make_bool(op, available);
    return 0;
} 


private int FAPI_find_plugin(i_ctx_t *i_ctx_p, const char *subtype, FAPI_server **pI)
{   i_plugin_holder *h = i_plugin_get_list(i_ctx_p);
    for (; h != 0; h = h->next)
       	if (!strcmp(h->I->d->type,"FAPI"))
	    if (!strcmp(h->I->d->subtype, subtype)) {
	        FAPI_server *I = *pI = (FAPI_server *)h->I;
	        CheckRET(renderer_retcode(i_ctx_p, I, I->ensure_open(I)));
	        return 0;
	    }
    return_error(e_invalidfont);
}

private int FAPI_refine_font(i_ctx_t *i_ctx_p, os_ptr op, gs_font_base *pbfont, const char *font_file_path)
{   ref *pdr = op;  /* font dict */
    FracInt matrix[6] = {1, 0, 0, 1, 0, 0};
    FAPI_server *I = pbfont->FAPI;
    double size, size1;
    ref *v, x0, y0, x1, y1, Decoding, *SubfontId;
    int HWResolution[2], BBox[4], scale, subfont = 0;
    const char *decodingID = NULL;
    char *xlatmap = NULL;
    FAPI_font ff = ff_stub;
    if (font_file_path != NULL && pbfont->FAPI_font_data == NULL)
        CheckRET(FAPI_get_xlatmap(i_ctx_p, &xlatmap));
    scale = 1 << I->frac_shift;
    size1 = size = 1 / hypot(pbfont->FontMatrix.xx, pbfont->FontMatrix.xy);
    if (size < 1000)
	size = 1000;
    if (size1 > 100)
        size1 = (int)(size1 + 0.5);
    matrix[0] = matrix[3] = (int)(size * scale + 0.5);
    HWResolution[0] = (FracInt)(72 * scale);
    HWResolution[1] = (FracInt)(72 * scale);
    if (dict_find_string(pdr, "SubfontId", &SubfontId) >= 0 && r_has_type(SubfontId, t_integer))
        subfont = SubfontId->value.intval;
    ff.font_file_path = font_file_path;
    ff.is_type1 = IsType1GlyphData(pbfont);
    ff.client_font_data = pbfont;
    ff.client_font_data2 = pdr;
    ff.server_font_data = pbfont->FAPI_font_data; /* Possibly pass it from zFAPIpassfont. */
    ff.is_cid = IsCIDFont(pbfont);
    CheckRET(renderer_retcode(i_ctx_p, I, I->get_scaled_font(I, &ff, subfont, matrix, HWResolution, xlatmap)));
    pbfont->FAPI_font_data = ff.server_font_data; /* Save it back to GS font. */
    CheckRET(renderer_retcode(i_ctx_p, I, I->get_font_bbox(I, &ff, BBox)));
    CheckRET(renderer_retcode(i_ctx_p, I, I->get_decodingID(I, &ff, &decodingID)));

    /* Refine FontBBox : */
    pbfont->FontBBox.p.x = (double)BBox[0] * size1 / size;
    pbfont->FontBBox.p.y = (double)BBox[1] * size1 / size;
    pbfont->FontBBox.q.x = (double)BBox[2] * size1 / size;
    pbfont->FontBBox.q.y = (double)BBox[3] * size1 / size;
    if (dict_find_string(op, "FontBBox", &v) <= 0 || !r_has_type(v, t_array))
	return_error(e_invalidfont);
    if (r_size(v) < 4)
	return_error(e_invalidfont);
    make_real(&x0, pbfont->FontBBox.p.x);
    make_real(&y0, pbfont->FontBBox.p.y);
    make_real(&x1, pbfont->FontBBox.q.x);
    make_real(&y1, pbfont->FontBBox.q.y);
    ref_assign_old(v, v->value.refs + 0, &x0, "FAPI_refine_font_BBox");
    ref_assign_old(v, v->value.refs + 1, &y0, "FAPI_refine_font_BBox");
    ref_assign_old(v, v->value.refs + 2, &x1, "FAPI_refine_font_BBox");
    ref_assign_old(v, v->value.refs + 3, &y1, "FAPI_refine_font_BBox");

    /* Assign a Decoding : */
    if (decodingID != 0 && *decodingID) {
       if (IsCIDFont(pbfont)) {
            ref *CIDSystemInfo, *Ordering, SubstNWP;
            byte buf[30];
            int ordering_length, decodingID_length = min(strlen(decodingID), sizeof(buf) - 2);
            if (dict_find_string(pdr, "CIDSystemInfo", &CIDSystemInfo) <= 0 || !r_has_type(CIDSystemInfo, t_dictionary))
                return_error(e_invalidfont);
            if (dict_find_string(CIDSystemInfo, "Ordering", &Ordering) <= 0 || !r_has_type(Ordering, t_string))
                return_error(e_invalidfont);
            ordering_length = min(r_size(Ordering), sizeof(buf) - 2 - decodingID_length);
            memcpy(buf, Ordering->value.const_bytes, ordering_length);
            CheckRET(name_ref(buf, ordering_length, &SubstNWP, 0));
            CheckRET(dict_put_string(pdr, "SubstNWP", &SubstNWP, NULL));
            buf[ordering_length] = '.';
            memcpy(buf + ordering_length + 1, decodingID, decodingID_length);
            buf[decodingID_length + 1 + ordering_length] = 0; /* Debug purpose only */
            CheckRET(name_ref(buf, decodingID_length + 1 + ordering_length, &Decoding, 0));
        } else
            CheckRET(name_ref((const byte *)decodingID, strlen(decodingID), &Decoding, 0));
        CheckRET(dict_put_string(pdr, "Decoding", &Decoding, NULL));
    }

    /* Refine descendents : */
    if (font_file_path == NULL && pbfont->FontType == ft_CID_encrypted) {
        /*  fixme : Do we really need this ? (besides FontBBox)
        */
        gs_font_cid0 *pfcid = (gs_font_cid0 *)pbfont;
        gs_font_type1 **FDArray = pfcid->cidata.FDArray;
        int i, n = pfcid->cidata.FDArray_size;
        ref *rFDArray, f;
        if (dict_find_string(pdr, "FDArray", &rFDArray) <= 0 || r_type(rFDArray) != t_array)
            return_error(e_invalidfont);
        ff = ff_stub;
        ff.is_type1 = true;
        for (i = 0; i < n; i++) {
            gs_font_type1 *pbfont1 = FDArray[i];
            pbfont1->FontBBox = pbfont->FontBBox; /* Inherit FontBBox from the type 9 font. */
            if(array_get(rFDArray, i, &f) < 0 || r_type(&f) != t_dictionary)
                return_error(e_invalidfont);
            ff.client_font_data = pbfont1;
            pbfont1->FAPI = pbfont->FAPI;
            ff.client_font_data2 = &f;
            ff.server_font_data = pbfont1->FAPI_font_data;
            ff.is_cid = true;
            CheckRET(renderer_retcode(i_ctx_p, I, I->get_scaled_font(I, &ff, 0, matrix, HWResolution, NULL)));
            pbfont1->FAPI_font_data = ff.server_font_data; /* Save it back to GS font. */
        }
    }
    return 0;
}

private int notify_remove_font(void *proc_data, void *event_data)
{   /* gs_font_finalize passes event_data == NULL, so check it here. */
    if (event_data == NULL) {
        gs_font_base *pbfont = proc_data;
        if (pbfont->FAPI_font_data != 0)
            pbfont->FAPI->release_typeface(pbfont->FAPI, pbfont->FAPI_font_data);
    }
    return 0;
}

/*  <string|name> <font> <is_disk_font> .rebuildfontFAPI <string|name> <font> */
/*  Rebuild a font for handling it with an external renderer. 
    
    The font was built as a native GS font to allow easy access
    to font features. Then zFAPIrebuildfont sets FAPI entry
    into gx_font_base and replaces BuildGlyph and BuildChar 
    to enforce the FAPI handling.

*/
private int zFAPIrebuildfont(i_ctx_t *i_ctx_p)
{   os_ptr op = osp;
    build_proc_refs build;
    gs_font *pfont;
    int code = font_param(op - 1, &pfont), code1;
    gs_font_base *pbfont = (gs_font_base *)pfont;
    ref *v;
    char *font_file_path = NULL, FAPI_ID[20];
    const byte *pchars;
    uint len;
    font_data *pdata;
    CheckRET(code);
    check_type(*op, t_boolean);
    if (pbfont->FAPI != 0) {
        /*  If the font was processed with zFAPIpassfont,
            it already has an attached FAPI and server_font_data.
            Don't change them here.
        */
    } else {
        if (dict_find_string(op - 1, "FAPI", &v) <= 0 || !r_has_type(v, t_name))
	    return_error(e_invalidfont);
        obj_string_data(v, &pchars, &len);
        len = min(len, sizeof(FAPI_ID) - 1);
        strncpy(FAPI_ID, (const char *)pchars, len);
        FAPI_ID[len] = 0;
        CheckRET(FAPI_find_plugin(i_ctx_p, FAPI_ID, &pbfont->FAPI));
    }
    pdata = (font_data *)pfont->client_data;
    if (dict_find_string(op - 1, "Path", &v) <= 0 || !r_has_type(v, t_string))
        v = NULL;
    if (pfont->FontType == ft_CID_encrypted && v == NULL) {
        CheckRET(build_proc_name_refs(&build, ".FAPIBuildGlyph9", ".FAPIBuildGlyph9"));
    } else
        CheckRET(build_proc_name_refs(&build, ".FAPIBuildChar", ".FAPIBuildGlyph"));
    if (name_index(&pdata->BuildChar) == name_index(&build.BuildChar)) {
        /* Already rebuilt - maybe a substituted font. */
    } else {
        ref_assign_new(&pdata->BuildChar, &build.BuildChar);
        ref_assign_new(&pdata->BuildGlyph, &build.BuildGlyph);
        if (v != NULL)
            font_file_path = ref_to_string(v, imemory_global, "font file path");
        code = FAPI_refine_font(i_ctx_p, op - 1, pbfont, font_file_path);
        if (font_file_path != NULL)
            gs_free_string(imemory_global, (byte *)font_file_path, r_size(v) + 1, "font file path");
        code1 = gs_notify_register(&pfont->notify_list, notify_remove_font, pbfont);
        (void)code1;  /* Recover possible error just ignoring it. */
    }
    pop(1);
    return code;
}

private bool TT_char_code_from_CID(ref *Decoding, int nCID, FAPI_char_ref *cr)
{   ref *DecodingArray, char_code, ih;
    make_int(&ih, nCID / 256);
    if (dict_find(Decoding, &ih, &DecodingArray) <= 0 || !r_has_type(DecodingArray, t_array) ||
        array_get(DecodingArray, nCID % 256, &char_code) < 0 || !r_has_type(&char_code, t_integer)) {
        cr->is_glyph_index = true;
        cr->char_code = 0; /* .notdef */
        return false;
    } else {
        cr->char_code = char_code.value.intval;
        return true;
    }
}

private ulong array_find(ref *Encoding, ref *char_name) {
    ulong n = r_size(Encoding), i;
    ref v;
    for (i = 0; i < n; i++)
        if (array_get(Encoding, i, &v) < 0)
            break;
        else if(r_type(char_name) == r_type(&v) && char_name->value.const_pname == v.value.const_pname)
            return i;
    return 0;
}

private int outline_char(i_ctx_t *i_ctx_p, FAPI_server *I, int import_shift_v, gs_show_enum *penum_s, struct gx_path_s *path, bool close_path)
{   FAPI_path path_interface = path_interface_stub;
    FAPI_outline_handler olh;
    olh.path = path;
    olh.x0 = penum_s->pgs->ctm.tx_fixed;
    olh.y0 = penum_s->pgs->ctm.ty_fixed;
    olh.close_path = close_path;
    olh.need_close = false;
    path_interface.olh = &olh;
    path_interface.shift = import_shift_v;
    CheckRET(renderer_retcode(i_ctx_p, I, I->get_char_outline(I, &path_interface)));
    if (olh.need_close && olh.close_path)
        CheckRET(add_closepath(&path_interface));
    return 0;
}

private void compute_em_scale(const gs_font_base *pbfont, FAPI_metrics *metrics, double FontMatrix_div, double *em_scale_x, double *em_scale_y)
{   /* optimize : move this stuff to FAPI_refine_font */
    gs_matrix *m = &pbfont->base->FontMatrix;
    int rounding_x, rounding_y; /* Striking out the 'float' representation error in FontMatrix. */
    double sx, sy;
    sx = hypot(m->xx, m->xy) * metrics->em_x / FontMatrix_div;
    sy = hypot(m->yx, m->yy) * metrics->em_y / FontMatrix_div;
    rounding_x = (int)(0x00800000 / sx);
    rounding_y = (int)(0x00800000 / sy);
    *em_scale_x = (int)(sx * rounding_x + 0.5) / (double)rounding_x;
    *em_scale_y = (int)(sy * rounding_y + 0.5) / (double)rounding_y;
}


private const int frac_pixel_shift = 4;

private int fapi_finish_render_aux(i_ctx_t *i_ctx_p, gs_font_base *pbfont, FAPI_server *I)
{   gs_text_enum_t *penum = op_show_find(i_ctx_p);
    gs_show_enum *penum_s = (gs_show_enum *)penum;
    gs_state *pgs = penum_s->pgs;
    gx_device *dev1 = gs_currentdevice_inline(pgs); /* Possibly changed by zchar_set_cache. */
    gx_device *dev = penum_s->dev;
    const int import_shift_v = _fixed_shift - I->frac_shift;
    FAPI_raster rast;
    if (SHOW_IS(penum, TEXT_DO_NONE)) {
        /* do nothing */
    } else if (igs->in_charpath) {
        CheckRET(outline_char(i_ctx_p, I, import_shift_v, penum_s, pgs->show_gstate->path, !pbfont->PaintType));
    } else {
        int code = I->get_char_raster(I, &rast);
        if (code == e_limitcheck) {
            /* The rerver provides an outline instead the raster. */
            gs_imager_state *pis = (gs_imager_state *)pgs->show_gstate;
            gs_point pt;
            CheckRET(gs_currentpoint(pgs->show_gstate, &pt));
            CheckRET(outline_char(i_ctx_p, I, import_shift_v, penum_s, pgs->show_gstate->path, !pbfont->PaintType));
            CheckRET(gs_imager_setflat(pis, gs_char_flatness(pis, 1.0)));
            if (pbfont->PaintType) {
                CheckRET(gs_stroke(pgs->show_gstate));
            } else
                CheckRET(gs_fill(pgs->show_gstate));
            CheckRET(gs_moveto(pgs->show_gstate, pt.x, pt.y));
        } else {
            CheckRET(renderer_retcode(i_ctx_p, I, code));
            if (pgs->in_cachedevice == CACHE_DEVICE_CACHING) { /* Using GS cache */
                /*  GS and renderer may transform coordinates few differently.
                    The best way is to make set_cache_device to take the renderer's bitmap metrics immediately,
                    but we need to account CDevProc, which may truncate the bitmap.
                    Perhaps GS overestimates the bitmap size,
                    so now we only add a compensating shift - the dx and dy.
                */              
                int shift_rd = _fixed_shift  - frac_pixel_shift;
                int rounding = 1 << (frac_pixel_shift - 1);
                int dx = arith_rshift_slow((pgs->ctm.tx_fixed >> shift_rd) + rast.orig_x + rounding, frac_pixel_shift);
                int dy = arith_rshift_slow((pgs->ctm.ty_fixed >> shift_rd) - rast.orig_y + rounding, frac_pixel_shift);
	        CheckRET(dev_proc(dev1, copy_mono)(dev1, rast.p, 0, rast.line_step, 0, dx, dy, rast.width, rast.height, 0, 1));
            } else { /* Not using GS cache */
	        const gx_clip_path * pcpath = i_ctx_p->pgs->clip_path; 
                const gx_drawing_color * pdcolor = penum->pdcolor;
	        CheckRET(dev_proc(dev, fill_mask)(dev, rast.p, 0, rast.line_step, 0,
			          (int)(penum_s->pgs->ctm.tx + (double)rast.orig_x / (1 << frac_pixel_shift) + 0.5), 
			          (int)(penum_s->pgs->ctm.ty - (double)rast.orig_y / (1 << frac_pixel_shift) + 0.5), 
			          rast.width, rast.height,
			          pdcolor, 1, rop3_default, pcpath));
            }
        }
    }
    pop(2);
    return 0;
}

private int fapi_finish_render(i_ctx_t *i_ctx_p)
{   os_ptr op = osp;
    gs_font *pfont;
    int code = font_param(op - 1, &pfont);
    if (code == 0) {
        gs_font_base *pbfont = (gs_font_base *) pfont;
        FAPI_server *I = pbfont->FAPI;
        code = fapi_finish_render_aux(i_ctx_p, pbfont, I);
        I->release_char_data(I);
    }
    return code;
}

private int FAPI_do_char(i_ctx_t *i_ctx_p, gs_font_base *pbfont, gx_device *dev, char *font_file_path, bool bBuildGlyph, ref *charstring)
{   /* Stack : <font> <code|name> --> - */
    os_ptr op = osp;
    gs_text_enum_t *penum = op_show_find(i_ctx_p);
    gs_show_enum *penum_s = (gs_show_enum *)penum;
    /* 
        fixme: the following code needs to optimize with a maintainence of scaled font objects 
        in graphics library and in interpreter. Now we suppose that the renderer
        uses font cache, so redundant font opening isn't a big expense.
    */
    FAPI_char_ref cr = {0, false, NULL, 0};
    const gs_matrix * ctm = &ctm_only(igs);
    int scale;
    int HWResolution[2];
    FracInt matrix[6]; /* Font to user space transformation */
    FAPI_metrics metrics;
    FAPI_server *I = pbfont->FAPI;
    int client_char_code = 0;
    ref char_name, *SubfontId;
    int subfont = 0;
    bool is_TT_from_type42 = (pbfont->FontType == ft_TrueType && font_file_path == NULL);
    bool is_embedded_type1 = (pbfont->FontType == ft_encrypted && font_file_path == NULL);
    bool bCID = (IsCIDFont(pbfont) || charstring != NULL);
    bool bIsType1GlyphData = IsType1GlyphData(pbfont);
    FAPI_font ff = ff_stub;
    gs_log2_scale_point log2_scale = {0, 0};
    int alpha_bits = (*dev_proc(dev, get_alpha_bits)) (dev, go_text);
    int oversampling_x, oversampling_y;
    double FontMatrix_div = (bCID && bIsType1GlyphData && font_file_path == NULL ? 1000 : 1);
    bool bVertical = (gs_rootfont(igs)->WMode != 0);
    double sbw[2], vsbw[4], *pvsbw = NULL;
    double em_scale_x, em_scale_y;
    gs_rect char_bbox;
    int code;
    if(bBuildGlyph && !bCID) {
        check_type(*op, t_name);
    } else
        check_type(*op, t_integer);

    /* Compute the sacle : */
    if (!SHOW_IS(penum, TEXT_DO_NONE) && !igs->in_charpath) {
        gs_currentcharmatrix(igs, NULL, 1); /* make char_tm valid */
        penum_s->fapi_log2_scale.x = -1;
        gx_compute_text_oversampling(penum_s, (gs_font *)pbfont, alpha_bits, &log2_scale);
        penum_s->fapi_log2_scale = log2_scale;
    }
retry_oversampling:
    oversampling_x = 1 << log2_scale.x;
    oversampling_y = 1 << log2_scale.y;
    if (penum == 0)
	return_error(e_undefined);
    scale = 1 << I->frac_shift;
    {   gs_matrix *base_font_matrix = &pbfont->base->FontMatrix;
        double dx = hypot(base_font_matrix->xx, base_font_matrix->xy);
        double dy = hypot(base_font_matrix->yx, base_font_matrix->yy);
        /*  Trick : we need to restore the font scale from ctm, pbfont->FontMatrix,
            and base_font_matrix. We assume that base_font_matrix is
            a multiple of pbfont->FontMatrix with a constant from scalefont.
            But we cannot devide ctm by pbfont->FontMatrix for getting
            a proper result: the base_font_matrix may be XY transposition,
            but we must not cut out the transposition from ctm.
            Therefore we use the norm of base_font_matrix columns as the divisors
            for X and Y. It is not clear what to do when base_font_matrix is anisotropic
            (i.e. dx != dy), but we did not meet such fonts before now.
        */
        matrix[0] =  (FracInt)(ctm->xx * FontMatrix_div / dx * 72 / dev->HWResolution[0] * scale);
        matrix[1] = -(FracInt)(ctm->xy * FontMatrix_div / dy * 72 / dev->HWResolution[0] * scale);
        matrix[2] =  (FracInt)(ctm->yx * FontMatrix_div / dx * 72 / dev->HWResolution[1] * scale);
        matrix[3] = -(FracInt)(ctm->yy * FontMatrix_div / dy * 72 / dev->HWResolution[1] * scale);
        matrix[4] =  (FracInt)(ctm->tx * FontMatrix_div / dx * 72 / dev->HWResolution[0] * scale);
        matrix[5] =  (FracInt)(ctm->ty * FontMatrix_div / dy * 72 / dev->HWResolution[1] * scale);
        /* Note: the ctm mapping here is upside down. */
    }
    HWResolution[0] = (FracInt)((double)dev->HWResolution[0] * oversampling_x * scale);
    HWResolution[1] = (FracInt)((double)dev->HWResolution[1] * oversampling_y * scale);

    /* Prepare font data : */
    if (dict_find_string(osp - 1, "SubfontId", &SubfontId) > 0 && r_has_type(SubfontId, t_integer))
        subfont = SubfontId->value.intval;
    ff.font_file_path = font_file_path;
    ff.is_type1 = bIsType1GlyphData;
    ff.is_cid = bCID;
    ff.client_font_data = pbfont;
    ff.client_font_data2 = op - 1;
    ff.server_font_data = pbfont->FAPI_font_data;
    CheckRET(renderer_retcode(i_ctx_p, I, I->get_scaled_font(I, &ff, subfont, matrix, HWResolution, NULL)));
    /* fixme : it would be nice to call get_scaled_font at once for entire 'show' string. */

    /* Obtain the character name : */
    if (bCID) {
	int_param(op, 0xFFFF, &client_char_code);
        make_null(&char_name);
    } else if (r_has_type(op, t_integer)) {
	/* Translate from PS encoding to char name : */
	ref *Encoding;
	int_param(op, 0xFF, &client_char_code);
        if (dict_find_string(osp - 1, "Encoding", &Encoding) > 0 && r_has_type(Encoding, t_array)) {
	    if (array_get(Encoding, client_char_code, &char_name) < 0)
	        CheckRET(name_ref((const byte *)".notdef", 7, &char_name, -1));
	} else
            return_error(e_invalidfont);
    } else 
	char_name = *op;

    /* Obtain the character code or glyph index : */
    if (bCID) {
        if (font_file_path != NULL) {
            ref *Decoding, *SubstNWP;
            int SubstNWP_length, i;
            if (dict_find_string(osp - 1, "Decoding", &Decoding) <= 0 || !r_has_type(Decoding, t_dictionary))
                return_error(e_invalidfont);
            if (dict_find_string(osp - 1, "SubstNWP", &SubstNWP) <= 0 || !r_has_type(SubstNWP, t_array))
                return_error(e_invalidfont);
            SubstNWP_length = r_size(SubstNWP);
            if (!TT_char_code_from_CID(Decoding, client_char_code, &cr)) {
                for (i = 0; i < SubstNWP_length; i += 5) {
                    /* fixme : process the narrow/wide/proportional mapping type -
                       need to adjust the 'matrix' above.
                       Call get_font_proportional_feature for proper choice.
                    */
                    ref rb, re, rs;
                    int nb, ne, ns;
                    CheckRET(array_get(SubstNWP, i + 1, &rb));
                    CheckRET(array_get(SubstNWP, i + 2, &re));
                    CheckRET(array_get(SubstNWP, i + 3, &rs));
                    nb = rb.value.intval;
                    ne = re.value.intval;
                    ns = rs.value.intval;
                    if (client_char_code >= nb && client_char_code <= ne)
                        if (TT_char_code_from_CID(Decoding, ns + (client_char_code - nb), &cr))
                            break;
                    if (client_char_code >= ns && client_char_code <= ns + (ne - nb))
                        if (TT_char_code_from_CID(Decoding, nb + (client_char_code - ns), &cr))
                            break;
                }
            }
        } else
            cr.char_code = client_char_code;
    } else if (is_TT_from_type42) {
        /* This font must not use 'cmap', so compute glyph index from CharStrings : */
	ref *CharStrings, *glyph_index;
        if (dict_find_string(osp - 1, "CharStrings", &CharStrings) <= 0 || !r_has_type(CharStrings, t_dictionary))
            return_error(e_invalidfont);
        if (dict_find(CharStrings, &char_name, &glyph_index) < 0) {
            cr.char_code = 0; /* .notdef */
            CheckRET(name_ref((const byte *)".notdef", 7, &char_name, -1));
        } else if (r_has_type(glyph_index, t_integer))
            cr.char_code = glyph_index->value.intval;
        else
            return_error(e_invalidfont);
        cr.is_glyph_index = true;
    } else if (is_embedded_type1) {
        /*  Since the client passes charstring by callback using ff.client_char_data,
            the client doesn't need to provide a good cr here.
            Perhaps since UFST uses char codes as glyph cache keys (UFST 4.2 cannot use names),
            we provide font char codes equal to document's char codes.
            This trick assumes that Encoding can't point different glyphs
            for same char code. The last should be true due to 
            PLRM3, "5.9.4 Subsetting and Incremental Definition of Glyphs".
        */
        if (r_has_type(op, t_integer))
            cr.char_code = client_char_code;
        else {
            /* Reverse Encoding here, because it is incremental. */
            ref *Encoding;
            if (dict_find_string(osp - 1, "Encoding", &Encoding) > 0)
                cr.char_code = (uint)array_find(Encoding, op);
            else
                return_error(e_invalidfont);
        }
    } else { /* a non-embedded font, i.e. a disk font */
        bool can_retrieve_char_by_name = false;
        obj_string_data(&char_name, &cr.char_name, &cr.char_name_length);
        CheckRET(renderer_retcode(i_ctx_p, I, I->can_retrieve_char_by_name(I, &ff, &cr, &can_retrieve_char_by_name)));
        if (!can_retrieve_char_by_name) {
	    /* Translate from char name to encoding used with 3d party font technology : */
	    ref *Decoding, *char_code;
            if (dict_find_string(osp - 1, "Decoding", &Decoding) > 0 && r_has_type(Decoding, t_dictionary)) {
	        if (dict_find(Decoding, &char_name, &char_code) >= 0 && r_has_type(char_code, t_integer))
		    int_param(char_code, 0xFFFF, &cr.char_code);
	    }
        }
    } 

    /* Render : */
    if (ff.is_type1 && ff.is_cid)
        ff.client_char_data = charstring;
    else
        ff.client_char_data = &char_name;
    if (SHOW_IS(penum, TEXT_DO_NONE)) {
	CheckRET(renderer_retcode(i_ctx_p, I, I->get_char_width(I, &ff, &cr, &metrics.escapement)));
    } else if (igs->in_charpath) {
        CheckRET(renderer_retcode(i_ctx_p, I, I->get_char_outline_metrics(I, &ff, &cr, &metrics)));
    } else {
        code = I->get_char_raster_metrics(I, &ff, &cr, &metrics);
        if (code == e_limitcheck) {
            if(log2_scale.x > 0 || log2_scale.y > 0) {
                penum_s->fapi_log2_scale.x = log2_scale.x = penum_s->fapi_log2_scale.y = log2_scale.y = 0;
                I->release_char_data(I);
                goto retry_oversampling;
            }
            CheckRET(renderer_retcode(i_ctx_p, I, I->get_char_outline_metrics(I, &ff, &cr, &metrics)));
        } else {
            CheckRET(renderer_retcode(i_ctx_p, I, code));
        }
    }
    compute_em_scale(pbfont, &metrics, FontMatrix_div, &em_scale_x, &em_scale_y);
    sbw[0] = metrics.escapement / em_scale_x;
    sbw[1] = 0;
    char_bbox.p.x = metrics.bbox_x0 / em_scale_x;
    char_bbox.p.y = metrics.bbox_y0 / em_scale_y;
    char_bbox.q.x = metrics.bbox_x1 / em_scale_x;
    char_bbox.q.y = metrics.bbox_y1 / em_scale_y;
    if (bVertical) {
        vsbw[0] = sbw[0] / 2;
        vsbw[1] = pbfont->FontBBox.q.y;
        vsbw[2] = 0;
        vsbw[3] = - pbfont->FontBBox.q.x; /* Sic ! */
        pvsbw = vsbw;
    }
    code = zchar_set_cache(i_ctx_p, pbfont, &char_name,
		           NULL, sbw, &char_bbox,
			   fapi_finish_render, fapi_finish_render, pvsbw);
    if (code != 0) {
        if (code < 0) {
            /* An error */
            I->release_char_data(I);
        } else { 
            /* Callout to CDevProc, zsetcachedevice2, fapi_finish_render. */
        }
    }
    return code;
}

private int FAPI_char(i_ctx_t *i_ctx_p, bool bBuildGlyph, ref *charstring)
{   /* Stack : <font> <code|name> --> - */
    ref *v;
    char *font_file_path = NULL;
    gx_device *dev = gs_currentdevice_inline(igs);
    gs_font *pfont;
    int code = font_param(osp - 1, &pfont);
    if (code == 0) {
        gs_font_base *pbfont = (gs_font_base *) pfont;
        if (dict_find_string(osp - 1, "Path", &v) > 0 && r_has_type(v, t_string))
            font_file_path = ref_to_string(v, imemory, "font file path");
        code = FAPI_do_char(i_ctx_p, pbfont, dev, font_file_path, bBuildGlyph, charstring);
        if (font_file_path != NULL)
            gs_free_string(imemory, (byte *)font_file_path, r_size(v) + 1, "font file path");
    }
    return code;
}

private int FAPIBuildGlyph9aux(i_ctx_t *i_ctx_p)
{   os_ptr op = osp;                  /* <font0> <cid> <font9> <cid> */
    ref font9 = *pfont_dict(gs_currentfont(igs));
    ref *rFDArray, f;
    int font_index;
    CheckRET(ztype9mapcid(i_ctx_p));  /* <font0> <cid> <charstring> <font_index> */
    /* fixme: what happens if the charstring is absent ? 
       Can FDArray contain 'null' (see %Type9BuildGlyph in gs_cidfn.ps)? */
    font_index = op[0].value.intval;
    if (dict_find_string(&font9, "FDArray", &rFDArray) <= 0 || r_type(rFDArray) != t_array)
        return_error(e_invalidfont);
    if(array_get(rFDArray, font_index, &f) < 0 || r_type(&f) != t_dictionary)
        return_error(e_invalidfont);
    op[0] = op[-2];                   
    op[-2] = op[-1]; /* Keep the charstring on ostack for the garbager. */
    op[-1] = f;                       /* <font0> <charstring> <subfont> <cid> */
    CheckRET(FAPI_char(i_ctx_p, true, op - 2));
                                      /* <font0> <charstring> */
    return 0;
}

/* <font> <code> .FAPIBuildChar - */
private int zFAPIBuildChar(i_ctx_t *i_ctx_p)
{   return FAPI_char(i_ctx_p, false, NULL);
}

/* non-CID : <font> <code> .FAPIBuildGlyph - */
/*     CID : <font> <name> .FAPIBuildGlyph - */
private int zFAPIBuildGlyph(i_ctx_t *i_ctx_p)
{   return FAPI_char(i_ctx_p, true, NULL);
}

/* <font> <cid> .FAPIBuildGlyph9 - */
private int zFAPIBuildGlyph9(i_ctx_t *i_ctx_p)
{   /*  The alghorithm is taken from %Type9BuildGlyph - see gs_cidfn.ps .  */
    os_ptr op = osp;
    int cid, code;
    avm_space s = ialloc_space(idmemory);
    check_type(op[ 0], t_integer);
    check_type(op[-1], t_dictionary);
    cid = op[0].value.intval;
    push(2);
    op[-1] = *pfont_dict(gs_currentfont(igs));
    op[0] = op[-2];                   /* <font0> <cid> <font9> <cid> */
    ialloc_set_space(idmemory, (r_is_local(op - 3) ? avm_global : avm_local)); /* for ztype9mapcid */
    code = FAPIBuildGlyph9aux(i_ctx_p);
    if (code != 0) {                  /* <font0> <dirty> <dirty> <dirty> */
        /* Adjust ostack for the correct error handling : */
        make_int(op - 2, cid);        
        pop(2);                       /* <font0> <cid> */
    } else {                          /* <font0> <dirty> */
        pop(2);                       /* */
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

private int do_FAPIpassfont(i_ctx_t *i_ctx_p, bool *success)
{   ref *pdr = osp;  /* font dict */
    gs_font *pfont;
    int code = font_param(osp, &pfont);
    gs_font_base *pbfont;
    int HWResolution[2], BBox[4];
    FracInt matrix[6] = {1, 0, 0, 1, 0, 0}; /* to be scaled */
    i_plugin_holder *h = i_plugin_get_list(i_ctx_p);
    int subfont = 0;
    char *xlatmap = NULL; /* Not needed for embedded fonts. */
    ref *SubfontId;
    FAPI_font ff = ff_stub;
    CheckRET(code);
    pbfont = (gs_font_base *)pfont;
    if (dict_find_string(pdr, "SubfontId", &SubfontId) > 0 && r_has_type(SubfontId, t_integer))
        subfont = SubfontId->value.intval;
    *success = false;
    ff.font_file_path = NULL;
    ff.is_type1 = IsType1GlyphData(pbfont);
    ff.is_cid = IsCIDFont(pbfont);
    ff.client_font_data = pfont;
    ff.client_font_data2 = pdr;
    ff.server_font_data = 0;
    for (; h != 0; h = h->next) {
	ref FAPI_ID;
        FAPI_server *I;
       	if (strcmp(h->I->d->type, "FAPI"))
            continue;
        I = (FAPI_server *)h->I;
        CheckRET(renderer_retcode(i_ctx_p, I, I->ensure_open(I)));
	HWResolution[0] = HWResolution[1] = 72 << I->frac_shift;
	matrix[0] = matrix[3] = 1 << I->frac_shift;
	if(I->get_scaled_font(I, &ff, subfont, matrix, HWResolution, xlatmap)) {
            if (pbfont->FAPI_font_data != 0)
                I->release_typeface(I, ff.server_font_data);
            ff.server_font_data = 0;
	    continue;
        }
        pbfont->FAPI_font_data = ff.server_font_data;
        ff.server_font_data = 0;
	if (I->get_font_bbox(I, &ff, BBox)) { /* Try to do something to check whether the font is valid. */
            /* Failed, skip this renderer : */
            if (pbfont->FAPI_font_data != 0)
                I->release_typeface(I, pbfont->FAPI_font_data);
            pbfont->FAPI_font_data = 0;
            continue;
        }
        /* fixme : with CID fonts we need to test something more. */
        pbfont->FAPI = I; /* We found a good renderer, so go with it */
        CheckRET(name_ref((const byte *)I->ig.d->subtype, strlen(I->ig.d->subtype), &FAPI_ID, false));
	CheckRET(dict_put_string(pdr, "FAPI", &FAPI_ID, NULL)); /* Insert FAPI entry to font dictionary. */
	*success = true;
	return 0;
    }
    /* Could not find renderer, return with false success. */
    return 0;
}

/* <font_dict> .FAPIpassfont bool <font_dict> */
/* must insert /FAPI to font dictionary */
private int zFAPIpassfont(i_ctx_t *i_ctx_p)
{   os_ptr op = osp;
    int code;
    bool found;
    check_type(*op, t_dictionary);
    code = do_FAPIpassfont(i_ctx_p, &found);
    if(code != 0)
	return code;
    push(1);
    make_bool(op, found);
    return 0;
}

const op_def zfapi_op_defs[] =
{   {"2.FAPIavailable",   zFAPIavailable},
    {"2.FAPIpassfont",    zFAPIpassfont},
    {"2.FAPIrebuildfont", zFAPIrebuildfont},
    {"2.FAPIBuildChar",   zFAPIBuildChar},
    {"2.FAPIBuildGlyph",  zFAPIBuildGlyph},
    {"2.FAPIBuildGlyph9", zFAPIBuildGlyph9},
    op_def_end(0)
};

