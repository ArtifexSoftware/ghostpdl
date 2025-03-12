/* Copyright (C) 2001-2023 Artifex Software, Inc.
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


/* Font creation utilities */
#include "memory_.h"
#include "string_.h"
#include "ghost.h"
#include "oper.h"
#include "gxfixed.h"
#include "gscencs.h"
#include "gsmatrix.h"
#include "gxdevice.h"
#include "gxfont.h"
#include "bfont.h"
#include "ialloc.h"
#include "idict.h"
#include "idparam.h"
#include "ilevel.h"
#include "iname.h"
#include "inamedef.h"		/* for inlining name_index */
#include "interp.h"		/* for initial_enter_name */
#include "ipacked.h"
#include "istruct.h"
#include "store.h"
#include "gsstate.h" /* for gs_currentcpsimode() */
/* Structure descriptor and GC procedures for font_data */
public_st_font_data();
static
CLEAR_MARKS_PROC(font_data_clear_marks)
{
    ref_struct_clear_marks(cmem, vptr, offset_of(font_data, u.type42.mru_sfnts_index)/*size*/, pstype);
}
static
ENUM_PTRS_BEGIN_PROC(font_data_enum_ptrs)
{
    return ref_struct_enum_ptrs(mem, vptr, offset_of(font_data, u.type42.mru_sfnts_index)/*size*/, index, pep, pstype, gcst);
}
ENUM_PTRS_END_PROC
static
RELOC_PTRS_BEGIN(font_data_reloc_ptrs)
{
    ref_struct_reloc_ptrs(vptr, offset_of(font_data, u.type42.mru_sfnts_index)/*size*/, pstype, gcst);
}
RELOC_PTRS_END

/* <string|name> <font_dict> .buildfont3 <string|name> <font> */
/* Build a type 3 (user-defined) font. */
static int
zbuildfont3(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    int code;
    build_proc_refs build;
    gs_font_base *pfont;

    check_op(2);
    check_type(*op, t_dictionary);
    code = build_gs_font_procs(op, &build);
    if (code < 0)
        return code;
    code = build_gs_simple_font(i_ctx_p, op, &pfont, ft_user_defined,
                                &st_gs_font_base, &build, bf_options_none);
    if (code < 0)
        return code;
    return define_gs_font(i_ctx_p, (gs_font *) pfont);
}

/* Encode a character. */
gs_glyph
zfont_encode_char(gs_font *pfont, gs_char chr, gs_glyph_space_t gspace)
{
    font_data *pdata = pfont_data(pfont);
    const ref *pencoding = &pdata->Encoding;
    ulong index = chr;	/* work around VAX widening bug */
    ref cname;
    int code = array_get(pfont->memory, pencoding, (long)index, &cname);

    if (code < 0 || !r_has_type(&cname, t_name))
        return GS_NO_GLYPH;
    if (pfont->FontType == ft_user_defined && r_type(&pdata->BuildGlyph) == t_null) {
        ref nsref, tname;

        name_string_ref(pfont->memory, &cname, &nsref);
        if (r_size(&nsref) == 7 &&
            !memcmp(nsref.value.const_bytes, ".notdef", r_size(&nsref))) {
            /* A special support for high level devices.
               They need a glyph name but the font doesn't provide one
               due to an instandard BuildChar.
               Such fonts don't conform to PLRM section 5.3.7,
               but we've got real examples that we want to handle (Bug 686982).
               Construct a name here.
               Low level devices don't pass here, because regular PS interpretation
               doesn't need such names.
            */
            char buf[20];
            int code;

            if (gspace == GLYPH_SPACE_NOGEN)
                return GS_NO_GLYPH;
            gs_snprintf(buf, sizeof(buf), "j%ld", chr); /* 'j' is arbutrary. */
            code = name_ref(pfont->memory, (const byte *)buf, strlen(buf), &tname, 1);
            if (code < 0) {
                /* Can't propagate the error due to interface limitation,
                   return with .notdef */
            } else
                cname = tname;
        }
    }
    return (gs_glyph)name_index(pfont->memory, &cname);
}

/* Get the name of a glyph. */
static int
zfont_glyph_name(gs_font *font, gs_glyph index, gs_const_string *pstr)
{
    ref nref, sref;

    if (index >= GS_MIN_CID_GLYPH) {	/* Fabricate a numeric name. */
        char cid_name[sizeof(gs_glyph) * 3 + 1];
        int code;

        gs_snprintf(cid_name, sizeof(cid_name), "%lu", (ulong) index);
        code = name_ref(font->memory, (const byte *)cid_name, strlen(cid_name),
                        &nref, 1);
        if (code < 0)
            return code;
    } else {
        name_index_ref(font->memory, index, &nref);
        if (nref.value.pname == NULL)
            return_error(gs_error_unknownerror);
    }
    name_string_ref(font->memory, &nref, &sref);
    pstr->data = sref.value.const_bytes;
    pstr->size = r_size(&sref);
    return 0;
}

void get_zfont_glyph_name( int (**proc)(gs_font *font, gs_glyph glyph, gs_const_string *pstr) )
{
    *proc = zfont_glyph_name;
}

static gs_char
gs_font_map_glyph_by_dict(const gs_memory_t *mem, const ref *map, gs_glyph glyph, ushort *u, unsigned int length)
{
    ref *v, n;
    uchar *unicode_return = (uchar *)u;
    if (glyph >= GS_MIN_CID_GLYPH) {
        uint cid = glyph - GS_MIN_CID_GLYPH;

        if (dict_find_string(map, "CIDCount", &v) > 0) {
            /* This is a CIDDEcoding resource. */
            make_int(&n, cid / 256);
            if (dict_find(map, &n, &v) > 0) {
                ref vv;

                if (array_get(mem, v, cid % 256, &vv) == 0 && r_type(&vv) == t_integer) {
                    if (v->value.intval > 65535) {
                        if (length < 4)
                            return 4;
                        unicode_return[0] = v->value.intval >> 24;
                        unicode_return[1] = (v->value.intval & 0x00FF0000) >> 16;
                        unicode_return[2] = (v->value.intval & 0x0000FF00) >> 8;
                        unicode_return[3] = v->value.intval & 0xFF;
                        return 4;
                    } else {
                        if (length < 2)
                            return 2;
                        unicode_return[0] = v->value.intval >> 8;
                        unicode_return[1] = v->value.intval & 0xFF;
                        return 2;
                    }
                }
            }
            return 0; /* Absent in the map. */
        }
        /* This is GlyphNames2Unicode dictionary. */
        make_int(&n, cid);
    } else
        name_index_ref(mem, glyph, &n);
     if (dict_find(map, &n, &v) > 0) {
        if (r_has_type(v, t_string)) {
            int l = r_size(v);

            if (length < l)
                return l;
            memcpy(unicode_return, v->value.const_bytes, l);
            return l;
        }
        if (r_type(v) == t_integer) {
            if (v->value.intval > 65535) {
                if (length < 4)
                    return 4;
                unicode_return[0] = v->value.intval >> 24;
                unicode_return[1] = (v->value.intval & 0x00FF0000) >> 16;
                unicode_return[2] = (v->value.intval & 0x0000FF00) >> 8;
                unicode_return[3] = v->value.intval & 0xFF;
                return 4;
            } else {
                if (length < 2)
                    return 2;
                unicode_return[0] = v->value.intval >> 8;
                unicode_return[1] = v->value.intval & 0xFF;
                return 2;
            }
        }
    }
    return 0; /* Absent in the map. */
}

/* Get Unicode UTF-16 code for a glyph. */
int
gs_font_map_glyph_to_unicode(gs_font *font, gs_glyph glyph, int ch, ushort *u, unsigned int length)
{
    font_data *pdata = pfont_data(font);
    const ref *UnicodeDecoding;
    uchar *unicode_return = (uchar *)u;

    if (r_type(&pdata->GlyphNames2Unicode) == t_dictionary) {
        int c = gs_font_map_glyph_by_dict(font->memory,
                                              &pdata->GlyphNames2Unicode, glyph, u, length);

        if (c != 0)
            return c;
        if (ch != -1) { /* -1 indicates a CIDFont */
            /* Its possible that we have a GlyphNames2Unicode dictionary
             * which contains integers and Unicode values, rather than names
             * and Unicode values. This happens if the input was PDF, the font
             * has a ToUnicode Cmap, but no Encoding. In this case we need to
             * use the character code as an index into the dictionary. Try that
             * now before we fall back to the UnicodeDecoding.
             */
            ref *v, n;

            make_int(&n, ch);
            if (dict_find(&pdata->GlyphNames2Unicode, &n, &v) > 0) {
                if (r_has_type(v, t_string)) {
                    int l = r_size(v);

                    if (l > length)
                        return l;

                    memcpy(unicode_return, v->value.const_bytes, l);
                    return l;
                }
                if (r_type(v) == t_integer) {
                    if (v->value.intval > 65535) {
                        if (length < 4)
                            return 4;
                        unicode_return[0] = v->value.intval >> 24;
                        unicode_return[1] = (v->value.intval & 0x00FF0000) >> 16;
                        unicode_return[2] = (v->value.intval & 0x0000FF00) >> 8;
                        unicode_return[3] = v->value.intval & 0xFF;
                        return 4;
                    } else {
                        if (length < 2)
                            return 2;
                        unicode_return[0] = v->value.intval >> 8;
                        unicode_return[1] = v->value.intval & 0xFF;
                        return 2;
                    }
                }
            }
        }
        /*
         * Fall through, because test.ps for SF bug #529103 requres
         * to examine both tables. Due to that the Unicode Decoding resource
         * can't be a default value for FontInfo.GlyphNames2Unicode .
         */
    }
    if (glyph <= GS_MIN_CID_GLYPH && glyph != GS_NO_GLYPH) {
        UnicodeDecoding = zfont_get_to_unicode_map(font->dir);
        if (UnicodeDecoding != NULL && r_type(UnicodeDecoding) == t_dictionary)
            return gs_font_map_glyph_by_dict(font->memory, UnicodeDecoding, glyph, u, length);
    }
    return 0; /* No map. */
}

/* ------ Initialization procedure ------ */

const op_def zbfont_op_defs[] =
{
    {"2.buildfont3", zbuildfont3},
    op_def_end(0)
};

/* ------ Subroutines ------ */

/* Convert strings to executable names for build_proc_refs. */
int
build_proc_name_refs(const gs_memory_t *mem, build_proc_refs * pbuild,
                     const char *bcstr, const char *bgstr)
{
    int code;

    if (!bcstr)
        make_null(&pbuild->BuildChar);
    else {
        if ((code = name_ref(mem, (const byte *)bcstr,
                             strlen(bcstr), &pbuild->BuildChar, 0)) < 0)
            return code;
        r_set_attrs(&pbuild->BuildChar, a_executable);
    }
    if (!bgstr)
        make_null(&pbuild->BuildGlyph);
    else {
        if ((code = name_ref(mem, (const byte *)bgstr,
                             strlen(bgstr), &pbuild->BuildGlyph, 0)) < 0)
            return code;
        r_set_attrs(&pbuild->BuildGlyph, a_executable);
    }
    return 0;
}

/* Get the BuildChar and/or BuildGlyph routines from a (base) font. */
int
build_gs_font_procs(os_ptr op, build_proc_refs * pbuild)
{
    int ccode, gcode;
    ref *pBuildChar;
    ref *pBuildGlyph;

    check_type(*op, t_dictionary);
    ccode = dict_find_string(op, "BuildChar", &pBuildChar);
    gcode = dict_find_string(op, "BuildGlyph", &pBuildGlyph);
    if (ccode <= 0) {
        if (gcode <= 0)
            return_error(gs_error_invalidfont);
        make_null(&pbuild->BuildChar);
    } else {
        check_proc(*pBuildChar);
        pbuild->BuildChar = *pBuildChar;
    }
    if (gcode <= 0)
        make_null(&pbuild->BuildGlyph);
    else {
        check_proc(*pBuildGlyph);
        pbuild->BuildGlyph = *pBuildGlyph;
    }
    return 0;
}

static int font_with_same_UID_and_another_metrics(const gs_font *pfont0, const gs_font *pfont1)
{
    const gs_font_base *pbfont0 = (const gs_font_base *)pfont0;
    const gs_font_base *pbfont1 = (const gs_font_base *)pfont1;

    if (uid_equal(&pbfont0->UID, &pbfont1->UID)) {
        const ref *pfdict0 = &pfont_data(gs_font_parent(pbfont0))->dict;
        const ref *pfdict1 = &pfont_data(gs_font_parent(pbfont1))->dict;
        ref *pmdict0, *pmdict1;

        if (pbfont0->WMode || dict_find_string(pfdict0, "Metrics", &pmdict0) <= 0)
            pmdict0 = NULL;
        if (pbfont1->WMode || dict_find_string(pfdict1, "Metrics", &pmdict1) <= 0)
            pmdict1 = NULL;
        if (!pmdict0 != !pmdict1)
            return 1;
        if (pmdict0 != NULL && !obj_eq(pfont0->memory, pmdict0, pmdict1))
            return 1;
        if (!pbfont0->WMode || dict_find_string(pfdict0, "Metrics2", &pmdict0) <= 0)
            pmdict0 = NULL;
        if (!pbfont0->WMode || dict_find_string(pfdict1, "Metrics2", &pmdict1) <= 0)
            pmdict1 = NULL;
        if (!pmdict0 != !pmdict1)
            return 1;
        if (pmdict0 != NULL && !obj_eq(pfont0->memory, pmdict0, pmdict1))
            return 1;
    }
    return 0;
}

/* Do the common work for building a primitive font -- one whose execution */
/* algorithm is implemented in C (Type 1, Type 2, Type 4, or Type 42). */
/* The caller guarantees that *op is a dictionary. */
int
build_gs_primitive_font(i_ctx_t *i_ctx_p, os_ptr op, gs_font_base ** ppfont,
                        font_type ftype, gs_memory_type_ptr_t pstype,
                        const build_proc_refs * pbuild,
                        build_font_options_t options)
{
    ref *pcharstrings = 0;
    ref CharStrings;
    gs_font_base *pfont;
    font_data *pdata;
    int code;

    if (dict_find_string(op, "CharStrings", &pcharstrings) <= 0) {
        if (!(options & bf_CharStrings_optional))
            return_error(gs_error_invalidfont);
    } else {
        ref *ignore;

        if (!r_has_type(pcharstrings, t_dictionary))
            return_error(gs_error_invalidfont);
        if ((options & bf_notdef_required) != 0 &&
            dict_find_string(pcharstrings, ".notdef", &ignore) <= 0
            )
            return_error(gs_error_invalidfont);
        /*
         * Since build_gs_simple_font may resize the dictionary and cause
         * pointers to become invalid, save CharStrings.
         */
        CharStrings = *pcharstrings;
    }
    code = build_gs_outline_font(i_ctx_p, op, ppfont, ftype, pstype, pbuild,
                                 options, build_gs_simple_font);
    if (code != 0)
        return code;
    pfont = *ppfont;
    pdata = pfont_data(pfont);
    if (pcharstrings)
        ref_assign(&pdata->CharStrings, &CharStrings);
    else
        make_null(&pdata->CharStrings);
    /* Check that the UniqueIDs match.  This is part of the */
    /* Adobe protection scheme, but we may as well emulate it. */
    if (uid_is_valid(&pfont->UID) &&
        !dict_check_uid_param(op, &pfont->UID)
        )
        uid_set_invalid(&pfont->UID);
    if (uid_is_valid(&pfont->UID)) {
        const gs_font *pfont0 = (const gs_font *)pfont;

        code = gs_font_find_similar(ifont_dir, &pfont0,
                       font_with_same_UID_and_another_metrics);
        if (code < 0)
            return code; /* Must not happen. */
        if (code)
            uid_set_invalid(&pfont->UID);
    }
    return 0;
}

/* Build a FDArray entry for a CIDFontType 0 font. */
/* Note that as of Adobe PostScript version 3011, this may be either */
/* a Type 1 or Type 2 font. */
static int
build_FDArray_sub_font(i_ctx_t *i_ctx_p, ref *op,
                       gs_font_base **ppfont,
                       font_type ftype, gs_memory_type_ptr_t pstype,
                       const build_proc_refs * pbuild,
                       build_font_options_t ignore_options)
{
    gs_font *pfont;
    int code = build_gs_sub_font(i_ctx_p, op, &pfont, ftype, pstype, pbuild,
                                 NULL, op);

    if (code >= 0)
        *ppfont = (gs_font_base *)pfont;
    return code;
}
int
build_gs_FDArray_font(i_ctx_t *i_ctx_p, ref *op,
                      gs_font_base **ppfont,
                      font_type ftype, gs_memory_type_ptr_t pstype,
                      const build_proc_refs * pbuild)
{
    gs_font_base *pfont;
    font_data *pdata;
    int code = build_gs_outline_font(i_ctx_p, op, &pfont, ftype, pstype,
                                     pbuild, bf_options_none,
                                     build_FDArray_sub_font);
    static const double bbox[4] = { 0, 0, 0, 0 };
    gs_uid uid;

    if (code < 0)
        return code;
    pdata = pfont_data(pfont);
    /* Fill in members normally set by build_gs_primitive_font. */
    make_null(&pdata->CharStrings);
    /* Fill in members normally set by build_gs_simple_font. */
    uid_set_invalid(&uid);
    init_gs_simple_font(pfont, bbox, &uid);
    pfont->encoding_index = ENCODING_INDEX_UNKNOWN;
    pfont->nearest_encoding_index = ENCODING_INDEX_UNKNOWN;
    /* Fill in members normally set by build_gs_font. */
    pfont->key_name = pfont->font_name;
    *ppfont = pfont;
    return 0;
}

/* Do the common work for building an outline font -- a non-composite font */
/* for which PaintType and StrokeWidth are meaningful. */
/* The caller guarantees that *op is a dictionary. */
int
build_gs_outline_font(i_ctx_t *i_ctx_p, os_ptr op, gs_font_base ** ppfont,
                      font_type ftype, gs_memory_type_ptr_t pstype,
                      const build_proc_refs * pbuild,
                      build_font_options_t options,
                      build_base_font_proc_t build_base_font)
{
    int painttype;
    float strokewidth;
    gs_font_base *pfont;
    int code = dict_int_param(op, "PaintType", 0, 3, 0, &painttype);

    if (code < 0)
        return code;
    code = dict_float_param(op, "StrokeWidth", 0.0, &strokewidth);
    if (code < 0)
        return code;
    code = build_base_font(i_ctx_p, op, ppfont, ftype, pstype, pbuild,
                           options);
    if (code != 0)
        return code;
    pfont = *ppfont;
    pfont->PaintType = painttype;
    pfont->StrokeWidth = strokewidth;
    return 0;
}

void
get_GlyphNames2Unicode(i_ctx_t *i_ctx_p, gs_font *pfont, ref *pdref)
{
    ref *pfontinfo = NULL, *g2u = NULL;
    font_data *pdata;

    if (dict_find_string(pdref, "FontInfo", &pfontinfo) <= 0 ||
            !r_has_type(pfontinfo, t_dictionary) ||
            dict_find_string(pfontinfo, "GlyphNames2Unicode", &g2u) <= 0 ||
            !r_has_type(pfontinfo, t_dictionary))
        return;
    /*
     * Since build_gs_font may resize the dictionary and cause
     * pointers to become invalid, save Glyph2Unicode
     */
    pdata = pfont_data(pfont);
    ref_assign_new(&pdata->GlyphNames2Unicode, g2u);
}

/* Do the common work for building a font of any non-composite FontType. */
/* The caller guarantees that *op is a dictionary. */
int
build_gs_simple_font(i_ctx_t *i_ctx_p, os_ptr op, gs_font_base ** ppfont,
                     font_type ftype, gs_memory_type_ptr_t pstype,
                     const build_proc_refs * pbuild,
                     build_font_options_t options)
{
    double bbox[4];
    gs_uid uid;
    int code;
    gs_font_base *pfont;
    uint space = ialloc_space(idmemory);

    code = font_bbox_param(imemory, op, bbox);
    if (code < 0)
        return code;
    /*
     * Make sure that we allocate uid
     * in the same VM as the font dictionary
     * (see build_gs_sub_font).
     */
    ialloc_set_space(idmemory, r_space(op));
    code = dict_uid_param(op, &uid, 0, imemory, i_ctx_p);
    ialloc_set_space(idmemory, space);
    if (code < 0)
        return code;
    if ((options & bf_UniqueID_ignored) && uid_is_UniqueID(&uid))
        uid_set_invalid(&uid);
    code = build_gs_font(i_ctx_p, op, (gs_font **) ppfont, ftype, pstype,
                         pbuild, options);
    if (code != 0)		/* invalid or scaled font */
        return code;
    pfont = *ppfont;
    pfont->procs.init_fstack = gs_default_init_fstack;
    pfont->procs.define_font = gs_no_define_font;
    pfont->procs.decode_glyph = gs_font_map_glyph_to_unicode;
    pfont->procs.make_font = zbase_make_font;
    pfont->procs.next_char_glyph = gs_default_next_char_glyph;
    pfont->FAPI = 0;
    pfont->FAPI_font_data = 0;
    init_gs_simple_font(pfont, bbox, &uid);
    lookup_gs_simple_font_encoding(pfont);
    get_GlyphNames2Unicode(i_ctx_p, (gs_font *)pfont, op);
    return 0;
}

/* Initialize the FontBBox and UID of a non-composite font. */
void
init_gs_simple_font(gs_font_base *pfont, const double bbox[4],
                    const gs_uid *puid)
{
    pfont->FontBBox.p.x = bbox[0];
    pfont->FontBBox.p.y = bbox[1];
    pfont->FontBBox.q.x = bbox[2];
    pfont->FontBBox.q.y = bbox[3];
    pfont->UID = *puid;
}

/* Compare the encoding of a simple font with the registered encodings. */
void
lookup_gs_simple_font_encoding(gs_font_base * pfont)
{
    const ref *pfe = &pfont_data(pfont)->Encoding;
    int index = -1;

    pfont->encoding_index = index;
    if (r_type(pfe) == t_array && r_size(pfe) <= 256) {
        /* Look for an encoding that's "close". */
        uint esize = r_size(pfe);
        int near_index = -1;
        uint best = esize / 3;	/* must match at least this many */
        gs_const_string fstrs[256];
        int i;

        /* Get the string names of the glyphs in the font's Encoding. */
        for (i = 0; i < esize; ++i) {
            ref fchar;

            if (array_get(pfont->memory, pfe, (long)i, &fchar) < 0 ||
                !r_has_type(&fchar, t_name)
                )
                fstrs[i].data = 0, fstrs[i].size = 0;
            else {
                ref nsref;

                name_string_ref(pfont->memory, &fchar, &nsref);
                fstrs[i].data = nsref.value.const_bytes;
                fstrs[i].size = r_size(&nsref);
            }
        }
        /* Compare them against the known encodings. */
        for (index = 0; index < NUM_KNOWN_REAL_ENCODINGS; ++index) {
            uint match = esize;

            for (i = esize; --i >= 0;) {
                gs_const_string rstr;

                gs_c_glyph_name(gs_c_known_encode((gs_char)i, index), &rstr);
                if (rstr.size == fstrs[i].size &&
                    !memcmp(rstr.data, fstrs[i].data, rstr.size)
                    )
                    continue;
                if (--match <= best)
                    break;
            }
            if (match > best) {
                best = match;
                near_index = index;
                /* If we have a perfect match, stop now. */
                if (best == esize)
                    break;
            }
        }
        index = near_index;
        if (best == esize)
            pfont->encoding_index = index;
    }
    pfont->nearest_encoding_index = index;
}

/* Get FontMatrix and FontName parameters. */
static int
sub_font_params(gs_memory_t *mem, const ref *op, gs_matrix *pmat, gs_matrix *pomat, ref *pfname)
{
    ref *pmatrix, *pfontname, *pfontstyle, *porigfont, *pfontinfo;

    if (dict_find_string(op, "FontMatrix", &pmatrix) <= 0 ||
        read_matrix(mem, pmatrix, pmat) < 0
        )
        return_error(gs_error_invalidfont);
    if (dict_find_string(op, "OrigFont", &porigfont) <= 0)
        porigfont = NULL;
    if (porigfont != NULL && !r_has_type(porigfont, t_dictionary))
        return_error(gs_error_typecheck);

    if (pomat!= NULL) {
        if (porigfont == NULL ||
            dict_find_string(porigfont, "FontMatrix", &pmatrix) <= 0 ||
            read_matrix(mem, pmatrix, pomat) < 0
            )
            memset(pomat, 0, sizeof(*pomat));
    }
    /* Use the FontInfo/OrigFontName key preferrentially (created by MS PSCRIPT driver) */
    if ((dict_find_string((porigfont != NULL ? porigfont : op), "FontInfo", &pfontinfo) > 0) &&
        r_has_type(pfontinfo, t_dictionary) &&
        (dict_find_string(pfontinfo, "OrigFontName", &pfontname) > 0) && (r_has_type(pfontname, t_name) || r_has_type(pfontname, t_string))) {
        if ((dict_find_string(pfontinfo, "OrigFontStyle", &pfontstyle) > 0) && (r_has_type(pfontname, t_name) || r_has_type(pfontname, t_string)) &&
                r_size(pfontstyle) > 0) {
            const byte *tmpStr1 = pfontname->value.const_bytes;
            const byte *tmpStr2 = pfontstyle->value.const_bytes;
            int fssize1 = r_size(pfontname), fssize2 = r_size(pfontstyle), fssize = fssize1 + fssize2 + 1;
            byte *sfname = gs_alloc_string(mem, fssize, "sub_font_params");

            if (sfname == NULL)
                return_error(gs_error_VMerror);
            memcpy(sfname, tmpStr1, fssize1);
            sfname[fssize1]=',' ;
            memcpy(sfname + fssize1 + 1, tmpStr2, fssize2);
            make_string(pfname, a_readonly, fssize, sfname);
        } else
            get_font_name(mem, pfname, pfontname);
    } else if (dict_find_string((porigfont != NULL ? porigfont : op), ".Alias", &pfontname) > 0) {
        /* If we emulate the font, we want the requested name rather than a substitute. */
        get_font_name(mem, pfname, pfontname);
    } else if (dict_find_string((porigfont != NULL ? porigfont : op), "FontName", &pfontname) > 0) {
        get_font_name(mem, pfname, pfontname);
    } else
        make_empty_string(pfname, a_readonly);
    return 0;
}

/* Do the common work for building a font of any FontType. */
/* The caller guarantees that *op is a dictionary. */
/* op[-1] must be the key under which the font is being registered */
/* in FontDirectory, normally a name or string. */
/* Return 0 for a new font, 1 for a font made by makefont or scalefont, */
/* or a negative error code. */
int
build_gs_font(i_ctx_t *i_ctx_p, os_ptr op, gs_font ** ppfont, font_type ftype,
              gs_memory_type_ptr_t pstype, const build_proc_refs * pbuild,
              build_font_options_t options)
{
    ref kname;			/* t_string */
    ref *pftype;
    ref *pencoding = 0;
    bool bitmapwidths;
    int exactsize, inbetweensize, transformedchar;
    int wmode;
    int code;
    gs_font *pfont;
    ref *pfid;
    ref *aop = dict_access_ref(op);
    bool cpsi_mode = gs_currentcpsimode(imemory);

    get_font_name(imemory, &kname, op - 1);
    if (dict_find_string(op, "FontType", &pftype) <= 0 ||
        !r_has_type(pftype, t_integer) ||
        pftype->value.intval != (int)ftype
        )
        return_error(gs_error_invalidfont);
    if (dict_find_string(op, "Encoding", &pencoding) <= 0) {
        if (!(options & bf_Encoding_optional))
            return_error(gs_error_invalidfont);
        pencoding = 0;
    } else {
        if (!r_is_array(pencoding))
            return_error(gs_error_invalidfont);
    }
    if (pencoding) {   /* observed Adobe behavior */
        int count = r_size(pencoding);
        int type = ftype ? t_name : t_integer;
        bool fixit = false;

        while (count--) {
           ref r;
           if ((code = array_get(imemory, pencoding, count, &r)) < 0 ||
             !(r_has_type(&r, type) || r_has_type(&r, t_null))) {
               if (!cpsi_mode && ftype == ft_user_defined) {
                   if (code < 0 || r_has_type(&r, t_null)) {
                       return_error(gs_error_typecheck);
                   }
                   fixit = true;
                   break;
               }
               else {
                   return_error(gs_error_typecheck);
               }
           }
        }

        /* For at least Type 3 fonts, Adobe Distiller will "fix" an Encoding array, as in, for example
         * Bug 692681 where the arrays contain integers rather than names. Once the font is instantiated
         * the integers have been converted to names.
         * It is preferable to to this manipulation here, rather than in Postscript, because we are less
         * restricted by read-only attributes and VM save levels.
         */
        if (fixit) {
            ref penc;
            uint size = 0;
            char buf[32], *bptr;
            avm_space curglob = ialloc_space(idmemory);
            avm_space useglob = r_is_local(pencoding) ? avm_local : avm_global;

            ialloc_set_space(idmemory, useglob);

            count = r_size(pencoding);
            if ((code = ialloc_ref_array(&penc, (r_type_attrs(pencoding) & a_readonly), count, "build_gs_font")) < 0)
                 return code;

            while (count--) {
               ref r;
               if (array_get(imemory, pencoding, count, &r) < 0){
                   return_error(gs_error_typecheck);
               }
               /* For type 3, we know the Encoding entries must be names */
               if (r_has_type(&r, t_name)){
                   ref_assign(&(penc.value.refs[count]), &r);
               }
               else {

                   if ((code = obj_cvs(imemory, &r, (byte *)buf, 32, &size, (const byte **)(&bptr))) < 0) {
                       return(code);
                   }
                   if ((code = name_ref(imemory, (const byte *)bptr, size, &r, true)) < 0)
                        return code;
                   ref_assign(&(penc.value.refs[count]), &r);
               }
            }

            if ((code = dict_put_string(osp, "Encoding", &penc, NULL)) < 0)
               return code;
            ialloc_set_space(idmemory, curglob);
        }
    }
    if ((code = dict_int_param(op, "WMode", 0, 1, 0, &wmode)) < 0 ||
        (code = dict_bool_param(op, "BitmapWidths", false, &bitmapwidths)) < 0 ||
        (code = dict_int_param(op, "ExactSize", 0, 2, fbit_use_bitmaps, &exactsize)) < 0 ||
        (code = dict_int_param(op, "InBetweenSize", 0, 2, fbit_use_outlines, &inbetweensize)) < 0 ||
        (code = dict_int_param(op, "TransformedChar", 0, 2, fbit_use_outlines, &transformedchar)) < 0
        )
        return code;
    code = dict_find_string(op, "FID", &pfid);
    if (code > 0 && r_has_type(pfid, t_fontID)) { /* silently ignore invalid FID per CET 13-05.ps */
        /*
         * If this font has a FID entry already, it might be a scaled font
         * made by makefont or scalefont; in a Level 2 environment, it might
         * be an existing font being registered under a second name, or a
         * re-encoded font (which was invalid in Level 1, but dvips did it
         * anyway).
         */
        pfont = r_ptr(pfid, gs_font);
        /*
         * If the following condition is false this is a re-encoded font,
         * or some other questionable situation in which the FID
         * was preserved.  Pretend the FID wasn't there.
         */
        if (obj_eq(pfont->memory, pfont_dict(pfont), op)) {
            if (pfont->base == pfont) {	/* original font */
                if (!level2_enabled)
                    return_error(gs_error_invalidfont);
                *ppfont = pfont;
                return 1;
            } else {		/* This was made by makefont or scalefont. */
                /* Just insert the new name. */
                gs_matrix mat;
                ref fname;			/* t_string */

                code = sub_font_params(imemory, op, &mat, NULL, &fname);
                if (code < 0)
                    return code;
                code = 1;
                copy_font_name(&pfont->font_name, &fname);
                goto set_name;
            }
        }
    }
    /* This is a new font. */
    if (!r_has_attr(aop, a_write))
        return_error(gs_error_invalidaccess);
    {
        ref encoding;

        /*
         * Since add_FID may resize the dictionary and cause
         * pencoding to become invalid, save the Encoding.
         */
        if (pencoding) {
            encoding = *pencoding;
            pencoding = &encoding;
        }
        code = build_gs_sub_font(i_ctx_p, op, &pfont, ftype, pstype,
                                 pbuild, pencoding, op);
        if (code < 0)
            return code;
    }
    pfont->BitmapWidths = bitmapwidths;
    pfont->ExactSize = (fbit_type)exactsize;
    pfont->InBetweenSize = (fbit_type)inbetweensize;
    pfont->TransformedChar = (fbit_type)transformedchar;
    pfont->WMode = wmode;
    pfont->procs.font_info = zfont_info;
    code = 0;
set_name:
    copy_font_name(&pfont->key_name, &kname);
    *ppfont = pfont;
    return code;
}

/* Create a sub-font -- a font or an entry in the FDArray of a CIDFontType 0 */
/* font.  Default BitmapWidths, ExactSize, InBetweenSize, TransformedChar, */
/* and WMode; do not fill in key_name. */
/* The caller guarantees that *op is a dictionary. */
int
build_gs_sub_font(i_ctx_t *i_ctx_p, const ref *op, gs_font **ppfont,
                  font_type ftype, gs_memory_type_ptr_t pstype,
                  const build_proc_refs * pbuild, const ref *pencoding,
                  ref *fid_op)
{
    gs_matrix mat, omat;
    ref fname;			/* t_string */
    gs_font *pfont;
    font_data *pdata;
    /*
     * Make sure that we allocate the font data
     * in the same VM as the font dictionary.
     */
    uint space = ialloc_space(idmemory);
    int code = sub_font_params(imemory, op, &mat, &omat, &fname);

    if (code < 0)
        return code;
    ialloc_set_space(idmemory, r_space(op));
    pfont = gs_font_alloc(imemory, pstype, &gs_font_procs_default, NULL,
                          "buildfont(font)");
    pdata = ialloc_struct(font_data, &st_font_data,
                          "buildfont(data)");
    if (pfont == 0 || pdata == 0)
        code = gs_note_error(gs_error_VMerror);
    else if (fid_op)
        code = add_FID(i_ctx_p, fid_op, pfont, iimemory);
    if (code < 0) {
        ifree_object(pdata, "buildfont(data)");
        ifree_object(pfont, "buildfont(font)");
        ialloc_set_space(idmemory, space);
        return code;
    }
    refset_null((ref *) pdata, sizeof(font_data) / sizeof(ref));
    ref_assign_new(&pdata->dict, op);
    ref_assign_new(&pdata->BuildChar, &pbuild->BuildChar);
    ref_assign_new(&pdata->BuildGlyph, &pbuild->BuildGlyph);
    if (pencoding)
        ref_assign_new(&pdata->Encoding, pencoding);
    pfont->client_data = pdata;
    pfont->FontType = ftype;
    pfont->FontMatrix = mat;
    pfont->orig_FontMatrix = omat;
    pfont->BitmapWidths = false;
    pfont->ExactSize = fbit_use_bitmaps;
    pfont->InBetweenSize = fbit_use_outlines;
    pfont->TransformedChar = fbit_use_outlines;
    pfont->WMode = 0;
    pfont->procs.encode_char = zfont_encode_char;
    pfont->procs.glyph_name = zfont_glyph_name;
    ialloc_set_space(idmemory, space);
    copy_font_name(&pfont->font_name, &fname);
    *ppfont = pfont;
    return 0;
}

/* Get the string corresponding to a font name. */
/* If the font name isn't a name or a string, return an empty string. */
void
get_font_name(const gs_memory_t *mem, ref * pfname, const ref * op)
{
    switch (r_type(op)) {
        case t_string:
            *pfname = *op;
            break;
        case t_name:
            name_string_ref(mem, op, pfname);
            break;
        default:
            /* This is weird, but legal.... */
            make_empty_string(pfname, a_readonly);
    }
}

/* Copy a font name into the gs_font structure. */
void
copy_font_name(gs_font_name * pfstr, const ref * pfname)
{
    uint size = r_size(pfname);

    if (size > gs_font_name_max)
        size = gs_font_name_max;
    memcpy(&pfstr->chars[0], pfname->value.const_bytes, size);
    /* Following is only for debugging printout. */
    pfstr->chars[size] = 0;
    pfstr->size = size;
}

/* Finish building a font, by calling gs_definefont if needed. */
int
define_gs_font(i_ctx_t *i_ctx_p, gs_font * pfont)
{
    return (pfont->base == pfont && pfont->dir == 0 ?
            /* i.e., unregistered original font */
            gs_definefont(ifont_dir, pfont) :
            0);
}
