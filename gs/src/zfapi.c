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
#include "bfont.h"
#include "dstack.h"
#include "ichar.h"
#include "idict.h"
#include "iddict.h"
#include "iname.h"
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

#define CheckRET(a) { int x = a; if(x < 0) return x; }

/* -------------------------------------------------------- */

typedef struct FAPI_outline_handler_s {
    struct gx_path_s *path;
    fixed x0, y0;
} FAPI_outline_handler;

private inline int import_shift(int x, int n)
{   return n > 0 ? x << n : x >> -n;
}

private int add_move(FAPI_path *I, FracInt x, FracInt y)
{   FAPI_outline_handler *olh = (FAPI_outline_handler *)I->olh;
    return gx_path_add_point(olh->path, import_shift(x, I->shift) + olh->x0, -import_shift(y, I->shift) + olh->y0);
}

private int add_line(FAPI_path *I, FracInt x, FracInt y)
{   FAPI_outline_handler *olh = (FAPI_outline_handler *)I->olh;
    return gx_path_add_line_notes(olh->path, import_shift(x, I->shift) + olh->x0, -import_shift(y, I->shift) + olh->y0, 0);
}

private int add_curve(FAPI_path *I, FracInt x0, FracInt y0, FracInt x1, FracInt y1, FracInt x2, FracInt y2)
{   FAPI_outline_handler *olh = (FAPI_outline_handler *)I->olh;
    return gx_path_add_curve_notes(olh->path, import_shift(x0, I->shift) + olh->x0, -import_shift(y0, I->shift) + olh->y0, 
					      import_shift(x1, I->shift) + olh->x0, -import_shift(y1, I->shift) + olh->y0, 
					      import_shift(x2, I->shift) + olh->x0, -import_shift(y2, I->shift) + olh->y0, 0);
}

private int add_closepath(FAPI_path *I)
{   FAPI_outline_handler *olh = (FAPI_outline_handler *)I->olh;
    return gx_path_close_subpath_notes(olh->path, 0);
}

private FAPI_path path_interface_stub = { NULL, 0, add_move, add_line, add_curve, add_closepath };

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
    r->length = r_size(&s);
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
        dict_find_string(pdr, "sfnts", &r->sfnts) < 0)
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
        case FAPI_FONT_FEATURE_FontBBox: return (index < 2 ? 0 : 1024); /* wrong */
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
                if (dict_find_string(pdr, "Private", &Private) < 0)
                    return 0;
                if (dict_find_string(Private, "Subrs", &Subrs) < 0)
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
        case FAPI_FONT_FEATURE_BlueScale: return (ulong)(pfont->data.BlueScale * 65536/* fixme */); 
        case FAPI_FONT_FEATURE_Subrs_total_size :
            {   ref *Private, *Subrs, v;
                int lenIV = max(pfont->data.lenIV, 0);
                ulong size = 0;
                long i = 0;
                if (dict_find_string(pdr, "Private", &Private) < 0)
                    return 0;
                if (dict_find_string(Private, "Subrs", &Subrs) < 0)
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
{   gs_font_type1 *pfont = (gs_font_type1 *)ff->client_font_data;
    switch((int)var_id) {
        case FAPI_FONT_FEATURE_FontMatrix:
            switch(index) {
                case 0 : return pfont->base->FontMatrix.xx;
                case 1 : return pfont->base->FontMatrix.xy;
                case 2 : return pfont->base->FontMatrix.yx;
                case 3 : return pfont->base->FontMatrix.yy;
                case 4 : return pfont->base->FontMatrix.tx;
                case 5 : return pfont->base->FontMatrix.ty;
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
        if (ff->need_decrypt && pfont->data.lenIV > 0)
            decode_bytes(buf, type1string->value.const_bytes, l + lenIV, lenIV);
        else
            memcpy(buf, type1string->value.const_bytes, l);
    }
    return length;
}

private ushort FAPI_FF_get_subr(FAPI_font *ff, int index, byte *buf, ushort buf_length)
{   ref *pdr = (ref *)ff->client_font_data2;
    ref *Private, *Subrs, subr;
    if (dict_find_string(pdr, "Private", &Private) < 0)
        return 0;
    if (dict_find_string(Private, "Subrs", &Subrs) < 0)
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
        ref *CharStrings, char_name, *glyph;
        char_name = *(ref *)ff->client_char_data;
        if (dict_find_string(pdr, "CharStrings", &CharStrings) < 0)
            return -1;
        if (dict_find(CharStrings, &char_name, &glyph) < 0)
            return -1;
        glyph_length = get_type1_data(ff, glyph, buf, buf_length);
    } else { /* type 42 */
        ref *GlyphDirectory, glyph0, *glyph = &glyph0, glyph_index;
        if ((dict_find_string(pdr, "GlyphDirectory", &GlyphDirectory) >= 0 &&
             (r_type(GlyphDirectory) == t_dictionary &&
              ( make_int(&glyph_index, char_code),
                dict_find(GlyphDirectory, &glyph_index, &glyph) >= 0))) ||
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
    0, /* need_decode */
    0, /* font_file_path */
    0, /* subfont */
    0, /* is_type1 */
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
    if (font_file_path != NULL)
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
    ff.is_type1 = (pbfont->FontType == ft_encrypted);
    ff.client_font_data = pbfont;
    ff.client_font_data2 = pdr;
    ff.server_font_data = pbfont->FAPI_font_data; /* Possibly pass it from zFAPIpassfont. */
    CheckRET(renderer_retcode(i_ctx_p, I, I->get_scaled_font(I, &ff, font_file_path, subfont, matrix, HWResolution, xlatmap)));
    pbfont->FAPI_font_data = ff.server_font_data; /* Save it back to GS font. */
    CheckRET(renderer_retcode(i_ctx_p, I, I->get_font_bbox(I, &ff, BBox)));
    CheckRET(renderer_retcode(i_ctx_p, I, I->get_decodingID(I, &ff, &decodingID)));

    /* Refine FontBBox : */
    pbfont->FontBBox.p.x = (double)BBox[0] * size1 / size;
    pbfont->FontBBox.p.y = (double)BBox[1] * size1 / size;
    pbfont->FontBBox.q.x = (double)BBox[2] * size1 / size;
    pbfont->FontBBox.q.y = (double)BBox[3] * size1 / size;
    if (dict_find_string(op, "FontBBox", &v) < 0 || !r_has_type(v, t_array))
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

    /* Assign Decoding : */
    if (decodingID != 0 && *decodingID) {
        CheckRET(name_ref((const byte *)decodingID, strlen(decodingID), &Decoding, 0));
        CheckRET(dict_put_string(pdr, "Decoding", &Decoding, NULL));
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
        if (dict_find_string(op - 1, "FAPI", &v) < 0 || !r_has_type(v, t_name))
	    return_error(e_invalidfont);
        obj_string_data(v, &pchars, &len);
        len = min(len, sizeof(FAPI_ID) - 1);
        strncpy(FAPI_ID, (const char *)pchars, len);
        FAPI_ID[len] = 0;
        CheckRET(FAPI_find_plugin(i_ctx_p, FAPI_ID, &pbfont->FAPI));
    }
    pdata = (font_data *)pfont->client_data;
    CheckRET(build_proc_name_refs(&build, ".FAPIBuildChar", ".FAPIBuildGlyph"));
    ref_assign_new(&pdata->BuildChar, &build.BuildChar);
    ref_assign_new(&pdata->BuildGlyph, &build.BuildGlyph);
    if (dict_find_string(op - 1, "Path", &v) >= 0 && r_has_type(v, t_string))
        font_file_path = ref_to_string(v, imemory_global, "font file path");
    code = FAPI_refine_font(i_ctx_p, op - 1, pbfont, font_file_path);
    if (font_file_path != NULL)
        gs_free_string(imemory_global, (byte *)font_file_path, r_size(v) + 1, "font file path");
    code1 = gs_notify_register(&pfont->notify_list, notify_remove_font, pbfont);
    (void)code1;  /* Recover possible error just ignoring it. */
    pop(1);
    return code;
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

private int fapi_finish_dummy(i_ctx_t *i_ctx_p)
{   return 0;
}

private int outline_char(i_ctx_t *i_ctx_p, FAPI_server *I, FAPI_font *ff, FAPI_char_ref *cr, FAPI_metrics *metrics, int import_shift_v, gs_show_enum *penum_s)
{   FAPI_path path_interface = path_interface_stub;
    FAPI_outline_handler olh;
    olh.path = penum_s->pgs->show_gstate->path;
    olh.x0 = penum_s->pgs->ctm.tx_fixed;
    olh.y0 = penum_s->pgs->ctm.ty_fixed;
    path_interface.olh = &olh;
    path_interface.shift = import_shift_v;
    return renderer_retcode(i_ctx_p, I, I->outline_char(I, ff, cr, &path_interface, metrics));
}

private int FAPI_do_char(i_ctx_t *i_ctx_p, gx_device *dev, char *font_file_path)
{   /* Stack : <font> <code|name> --> - */
    os_ptr op = osp;
    gs_font *pfont;
    int code = font_param(op - 1, &pfont);
    gs_font_base *pbfont = (gs_font_base *) pfont;
    gs_text_enum_t *penum = op_show_find(i_ctx_p);
    gs_show_enum *penum_s = (gs_show_enum *)penum; /* fixme : is this ever correct ? */
    /* 
        fixme: the following code needs to optimize with a maintainence of scaled font objects 
        in graphics library and in interpreter. Now we suppose that the renderer
        uses font cache, so redundant font opening isn't a big expense.
    */
    FAPI_char_ref cr = {0, false, 0, 0};
    const gs_matrix * ctm = &ctm_only(igs), *base_font_matrix = &pfont->base->FontMatrix;
    int scale;
    int HWResolution[2];
    FracInt matrix[6]; /* Font to user space transformation */
    FAPI_metrics metrics;
    FAPI_server *I = pbfont->FAPI;
    const int import_shift_v = _fixed_shift - I->frac_shift;
    const int frac_pixel_shift = 4;
    int client_char_code = 0;
    ref char_name, *SubfontId;
    char *xlatmap = 0; 
    int subfont = 0;
    bool is_TT_from_type42 = (pfont->FontType == ft_TrueType && font_file_path == NULL);
    bool is_embedded_type1 = (pfont->FontType == ft_encrypted && font_file_path == NULL);
    FAPI_font ff = ff_stub;
    int width_status = sws_no_cache;
    gs_log2_scale_point log2_scale = {0, 0};
    int alpha_bits = (*dev_proc(dev, get_alpha_bits)) (dev, go_text);
    int oversampling_x, oversampling_y;
    if (!SHOW_IS(penum, TEXT_DO_NONE) && !igs->in_charpath) {
        gs_currentcharmatrix(igs, NULL, 1); /* make char_tm valid */
        penum_s->fapi_log2_scale.x = -1;
        gx_compute_text_oversampling(penum_s, pfont, alpha_bits, &log2_scale);
        penum_s->fapi_log2_scale = log2_scale;
        //igs->char_tm_valid = false;
    }
    oversampling_x = 1 << log2_scale.x;
    oversampling_y = 1 << log2_scale.y;
    if (code < 0)
        return code;
    if (penum == 0)
	return_error(e_undefined);
    scale = 1 << I->frac_shift;
    /* Note: the ctm mapping here is upside down. */
    matrix[0] =  (FracInt)(ctm->xx / base_font_matrix->xx * 72 / dev->HWResolution[0] * scale);
    matrix[1] = -(FracInt)(ctm->xy / base_font_matrix->yy * 72 / dev->HWResolution[0] * scale);
    matrix[2] =  (FracInt)(ctm->yx / base_font_matrix->xx * 72 / dev->HWResolution[1] * scale);
    matrix[3] = -(FracInt)(ctm->yy / base_font_matrix->yy * 72 / dev->HWResolution[1] * scale);
    matrix[4] =  (FracInt)(ctm->tx / base_font_matrix->xx * 72 / dev->HWResolution[0] * scale);
    matrix[5] =  (FracInt)(ctm->ty / base_font_matrix->yy * 72 / dev->HWResolution[1] * scale);
    HWResolution[0] = (FracInt)((double)dev->HWResolution[0] * oversampling_x * scale);
    HWResolution[1] = (FracInt)((double)dev->HWResolution[1] * oversampling_y * scale);
    if (dict_find_string(osp - 1, "SubfontId", &SubfontId) >= 0 && r_has_type(SubfontId, t_integer))
        subfont = SubfontId->value.intval;
    ff.font_file_path = font_file_path;
    ff.is_type1 = (pbfont->FontType == ft_encrypted);
    ff.client_font_data = pfont;
    ff.client_font_data2 = op - 1;
    ff.server_font_data = pbfont->FAPI_font_data;
    if (!is_TT_from_type42 && pbfont->FAPI_font_data == NULL)
        CheckRET(FAPI_get_xlatmap(i_ctx_p, &xlatmap));
    CheckRET(renderer_retcode(i_ctx_p, I, I->get_scaled_font(I, &ff, font_file_path, subfont, matrix, HWResolution, xlatmap)));
    /* fixme : it would be nice to call get_scaled_font at once for entire 'show' string. */
    if (r_has_type(op, t_integer)) {
	/* Translate from PS encoding to char name : */
	ref *Encoding;
	int_param(op, 0xFF, &client_char_code);
        if (dict_find_string(osp - 1, "Encoding", &Encoding) >= 0 && r_has_type(Encoding, t_array)) {
	    if (array_get(Encoding, client_char_code, &char_name) < 0)
	        CheckRET(name_ref((const byte *)".notdef", 7, &char_name, -1));
	} else
            return_error(e_invalidfont);
    } else 
	char_name = *op;
    if (is_TT_from_type42) {
        /* This font must not use 'cmap', so compute glyph index from CharStrings : */
	ref *CharStrings, *glyph_index;
        if (dict_find_string(osp - 1, "CharStrings", &CharStrings) < 0 || !r_has_type(CharStrings, t_dictionary))
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
            Perhaps since UFST uses char codes as char cache keys (UFST 4.2 cannot use names),
            we provide font char codes equal to document's char codes.
            This trick assumes that Encoding can't point different glyphs
            for same char code. The last should be true due to 
            PLRM3, "5.9.4 Subsetting and Incremental Definition of Glyphs".
        */
        if (r_has_type(op, t_integer))
            cr.char_code = client_char_code;
        else {
            /* Reverse Encoding here, because it is incrementatl. */
            ref *Encoding;
            if (dict_find_string(osp - 1, "Encoding", &Encoding) >= 0)
                cr.char_code = (uint)array_find(Encoding, op);
            else
                return_error(e_invalidfont);
        }
    } else { /* a non-embedded font, i.e. a disk font */
        bool can_retrieve_char_by_name = false;
        CheckRET(renderer_retcode(i_ctx_p, I, I->can_retrieve_char_by_name(I, &ff, &can_retrieve_char_by_name)));
        obj_string_data(&char_name, &cr.char_name, &cr.char_name_length);
        if (!can_retrieve_char_by_name) {
	    /* Translate from char name to encoding used with 3d party font technology : */
	    ref *Decoding, *char_code;
            if (dict_find_string(osp - 1, "Decoding", &Decoding) >= 0 && r_has_type(Decoding, t_dictionary)) {
	        if (dict_find(Decoding, &char_name, &char_code) >= 0 && r_has_type(char_code, t_integer))
		    int_param(char_code, 0xFFFF, &cr.char_code);
	    }
        }
    } 
    ff.client_char_data = &char_name;
    if (SHOW_IS(penum, TEXT_DO_NONE)) {
	CheckRET(renderer_retcode(i_ctx_p, I, I->get_char_width(I, &ff, &cr, &metrics.esc_x, &metrics.esc_y)));
    } else if (igs->in_charpath) {
        CheckRET(outline_char(i_ctx_p, I, &ff, &cr, &metrics, import_shift_v, penum_s));
    } else {
        double sbw[4] = {0, 0}; /* Rather we use only 2 elements, we keep it similar to the old code. */
        gs_rect char_bbox;
        FAPI_retcode code1;
        FAPI_raster rast;
        const gx_drawing_color * pdcolor = penum->pdcolor;
        double em_scale_x, em_scale_y;
	code1 = I->get_char_raster_metrics(I, &ff, &cr, &metrics);
        if (code1 == e_limitcheck) {
            gs_imager_state *pis = (gs_imager_state *)igs;
            CheckRET(outline_char(i_ctx_p, I, &ff, &cr, &metrics, import_shift_v, penum_s));
            CheckRET(gs_imager_setflat(pis, gs_char_flatness(pis, 1.0)));
            CheckRET(gs_fill(igs));
        } else {
            CheckRET(renderer_retcode(i_ctx_p, I, code1));
            if (metrics.bbox_x0 <= metrics.bbox_x1) { /* Isn't a space character. */
                gx_device *dev1;
                gs_state *pgs = penum_s->pgs;
                {   /* optimize : move this stuff to FAPI_refine_font */
                    gs_matrix *m = &pfont->base->FontMatrix;
                    int rounding_x, rounding_y; /* Striking out the 'float' representation error in FontMatrix. */
                    em_scale_x = hypot(m->xx, m->xy) * metrics.em_x;
                    em_scale_y = hypot(m->yx, m->yy) * metrics.em_y;
                    rounding_x = (int)(0x00800000 / em_scale_x); 
                    rounding_y = (int)(0x00800000 / em_scale_y); 
                    em_scale_x = (int)(em_scale_x * rounding_x + 0.5) / rounding_x;
                    em_scale_y = (int)(em_scale_y * rounding_y + 0.5) / rounding_y;
                }
                sbw[2] = metrics.escapement / em_scale_x;
                sbw[3] = 0;
                char_bbox.p.x = metrics.bbox_x0 / em_scale_x;
                char_bbox.p.y = metrics.bbox_y0 / em_scale_y;
                char_bbox.q.x = metrics.bbox_x1 / em_scale_x;
                char_bbox.q.y = metrics.bbox_y1 / em_scale_y;
	        code = zchar_set_cache(i_ctx_p, pbfont, &char_name,
		                       NULL, sbw + 2, &char_bbox,
			               fapi_finish_dummy, fapi_finish_dummy, NULL);
                if (code != 0) {
                    /* fixme : this may be a callout to CDevProc, never tested yet. 
                       Note that the restart after the callout calls
                       I->get_char_raster_metrics at second time. 
                       With the current UFST bridge this generates rater at
                       second time. Need to optimize.
                    */
                    return code;
                }
                width_status = penum_s->width_status;
                dev1 = gs_currentdevice_inline(pgs); /* Possibly changed by zchar_set_cache. */
                CheckRET(renderer_retcode(i_ctx_p, I, I->get_char_raster(I, &ff, &cr, &rast)));
                if (dev != dev1) { /* Using GS cache */
                    /*  GS and renderer may transform coordinates few differently.
                        The best way is to make set_cache_device to take the renderer's bitmap metrics immediately,
                        but we need to account CDevProc, which may truncate the bitmap.
                        Perhaps GS overestimates the bitmap size
                        so now we only add a compensating shift - the dx and dy.
                     */
                    int dx = arith_rshift_slow((penum_s->cc->offset.x >> (_fixed_shift  - frac_pixel_shift - log2_scale.x)) + metrics.orig_x + 8, frac_pixel_shift);
                    int dy = arith_rshift_slow((penum_s->cc->offset.y >> (_fixed_shift  - frac_pixel_shift - log2_scale.y)) - metrics.orig_y + 8, frac_pixel_shift);
	            CheckRET(dev_proc(dev1, copy_mono)(dev1, rast.p, 0, rast.line_step, /*id*/0, dx, dy, rast.width, rast.height, 0, 1));
                    penum_s->cc->offset.x = - (metrics.orig_x << (_fixed_shift - frac_pixel_shift - log2_scale.x)) + (dx << (_fixed_shift - log2_scale.x));
                    penum_s->cc->offset.y =   (metrics.orig_y << (_fixed_shift - frac_pixel_shift - log2_scale.y)) + (dy << (_fixed_shift - log2_scale.y));
                } else { /* Not using GS cache */
	            const gx_clip_path * pcpath = i_ctx_p->pgs->clip_path; 
		    CheckRET(dev_proc(dev, fill_mask)(dev, rast.p, 0, rast.line_step, 0,
				      (int)(penum_s->pgs->ctm.tx + metrics.orig_x / (1 << frac_pixel_shift) + 0.5), 
				      (int)(penum_s->pgs->ctm.ty - metrics.orig_y / (1 << frac_pixel_shift) + 0.5), 
				      rast.width, rast.height,
				      pdcolor, 1, rop3_default, pcpath));
                }
            }
	    code1 = renderer_retcode(i_ctx_p, I, I->release_char_raster(I, &ff, &cr));
            (void)code1; /* Recover possible error just ignoring it. */
        }
    } 
    /* Use the renderer's escapement : */
    penum_s->wxy.x =   import_shift(metrics.esc_x, import_shift_v - log2_scale.x);
    penum_s->wxy.y = - import_shift(metrics.esc_y, import_shift_v - log2_scale.y);
    penum_s->width_status = width_status;
    pop(2);
    return 0;
}

private int FAPI_char(i_ctx_t *i_ctx_p)
{   /* Stack : <font> <code|name> --> - */
    ref *v;
    char *font_file_path = NULL;
    int code;
    gx_device *dev = gs_currentdevice_inline(igs);
    if (dict_find_string(osp - 1, "Path", &v) >= 0 && r_has_type(v, t_string))
        font_file_path = ref_to_string(v, imemory, "font file path");
    code = FAPI_do_char(i_ctx_p, dev, font_file_path);
    if (font_file_path != NULL)
        gs_free_string(imemory, (byte *)font_file_path, r_size(v) + 1, "font file path");
    return code;
}

/* <font> <code> .FAPIBuildChar - */
private int zFAPIBuildChar(i_ctx_t *i_ctx_p)
{   check_type(*osp, t_integer);
    return FAPI_char(i_ctx_p);
}

/* <font> <name> .FAPIBuildGlyph - */
private int zFAPIBuildGlyph(i_ctx_t *i_ctx_p)
{   check_type(*osp, t_name);
    return FAPI_char(i_ctx_p);
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
    if (dict_find_string(pdr, "SubfontId", &SubfontId) >= 0 && r_has_type(SubfontId, t_integer))
        subfont = SubfontId->value.intval;
    *success = false;
    ff.font_file_path = NULL;
    ff.is_type1 = (pbfont->FontType == ft_encrypted);
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
	if(I->get_scaled_font(I, &ff, NULL, subfont, matrix, HWResolution, xlatmap)) {
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
{   {"2.FAPIavailable", zFAPIavailable},
    {"2.FAPIrebuildfont", zFAPIrebuildfont},
    {"2.FAPIBuildChar", zFAPIBuildChar},
    {"2.FAPIBuildGlyph", zFAPIBuildGlyph},
    {"2.FAPIpassfont", zFAPIpassfont},
    op_def_end(0)
};

