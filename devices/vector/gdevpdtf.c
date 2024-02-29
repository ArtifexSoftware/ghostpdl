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


/* Font and CMap resource implementation for pdfwrite text */
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsutil.h"		/* for bytes_compare */
#include "gxfcache.h"		/* for orig_fonts list */
#include "gxfcid.h"
#include "gxfcmap.h"
#include "gxfcopy.h"
#include "gxfont.h"
#include "gxfont1.h"
#include "gdevpsf.h"
#include "gdevpdfx.h"
#include "gdevpdtb.h"
#include "gdevpdtd.h"
#include "gdevpdtf.h"
#include "gdevpdtw.h"
#include "gdevpdti.h"
#include "gdevpdfo.h"       /* for cos_free() */
#include "whitelst.h"		/* Checks whether protected fonta cna be embedded */

#include "gscencs.h"

/* GC descriptors */
public_st_pdf_font_resource();
private_st_pdf_standard_font();
private_st_pdf_standard_font_element();
private_st_pdf_outline_fonts();

static
ENUM_PTRS_WITH(pdf_font_resource_enum_ptrs, pdf_font_resource_t *pdfont)
ENUM_PREFIX(st_pdf_resource, 12);
case 0: return ENUM_STRING(&pdfont->BaseFont);
case 1: ENUM_RETURN(pdfont->FontDescriptor);
case 2: ENUM_RETURN(pdfont->base_font);
case 3: ENUM_RETURN(pdfont->Widths);
case 4: ENUM_RETURN(pdfont->used);
case 5: ENUM_RETURN(pdfont->res_ToUnicode);
case 6: ENUM_RETURN(pdfont->cmap_ToUnicode);
case 7: switch (pdfont->FontType) {
 case ft_composite:
     ENUM_RETURN(pdfont->u.type0.DescendantFont);
 case ft_CID_encrypted:
 case ft_CID_TrueType:
     ENUM_RETURN(pdfont->u.cidfont.Widths2);
 default:
     pdf_mark_glyph_names(pdfont, mem);
     ENUM_RETURN(pdfont->u.simple.Encoding);
}
case 8: switch (pdfont->FontType) {
 case ft_encrypted:
 case ft_encrypted2:
 case ft_TrueType:
 case ft_PCL_user_defined:
 case ft_MicroType:
 case ft_GL2_stick_user_defined:
 case ft_user_defined:
 case ft_PDF_user_defined:
 case ft_GL2_531:
     ENUM_RETURN(pdfont->u.simple.v);
 case ft_CID_encrypted:
 case ft_CID_TrueType:
     ENUM_RETURN(pdfont->u.cidfont.v);
 case ft_composite:
 default:
     ENUM_RETURN(0);
}
case 9: switch (pdfont->FontType) {
 case ft_PCL_user_defined:
 case ft_MicroType:
 case ft_GL2_stick_user_defined:
 case ft_user_defined:
 case ft_PDF_user_defined:
 case ft_GL2_531:
     ENUM_RETURN(pdfont->u.simple.s.type3.char_procs);
 case ft_CID_encrypted:
 case ft_CID_TrueType:
     ENUM_RETURN(pdfont->u.cidfont.CIDToGIDMap);
 default:
     ENUM_RETURN(0);
}
case 10: switch (pdfont->FontType) {
 case ft_PCL_user_defined:
 case ft_MicroType:
 case ft_GL2_stick_user_defined:
 case ft_user_defined:
 case ft_PDF_user_defined:
 case ft_GL2_531:
     ENUM_RETURN(pdfont->u.simple.s.type3.cached);
 case ft_CID_encrypted:
 case ft_CID_TrueType:
     ENUM_RETURN(pdfont->u.cidfont.parent);
 default:
     ENUM_RETURN(0);
}
case 11: switch (pdfont->FontType) {
 case ft_PCL_user_defined:
 case ft_MicroType:
 case ft_GL2_stick_user_defined:
 case ft_user_defined:
 case ft_PDF_user_defined:
 case ft_GL2_531:
     ENUM_RETURN(pdfont->u.simple.s.type3.Resources);
 case ft_CID_encrypted:
 case ft_CID_TrueType:
     ENUM_RETURN(pdfont->u.cidfont.used2);
 default:
     ENUM_RETURN(0);
}
ENUM_PTRS_END
static
RELOC_PTRS_WITH(pdf_font_resource_reloc_ptrs, pdf_font_resource_t *pdfont)
{
    RELOC_PREFIX(st_pdf_resource);
    RELOC_STRING_VAR(pdfont->BaseFont);
    RELOC_VAR(pdfont->FontDescriptor);
    RELOC_VAR(pdfont->base_font);
    RELOC_VAR(pdfont->Widths);
    RELOC_VAR(pdfont->used);
    RELOC_VAR(pdfont->res_ToUnicode);
    RELOC_VAR(pdfont->cmap_ToUnicode);
    switch (pdfont->FontType) {
    case ft_composite:
        RELOC_VAR(pdfont->u.type0.DescendantFont);
        break;
    case ft_PCL_user_defined:
    case ft_MicroType:
    case ft_GL2_stick_user_defined:
    case ft_user_defined:
    case ft_PDF_user_defined:
    case ft_GL2_531:
        RELOC_VAR(pdfont->u.simple.Encoding);
        RELOC_VAR(pdfont->u.simple.v);
        RELOC_VAR(pdfont->u.simple.s.type3.char_procs);
        RELOC_VAR(pdfont->u.simple.s.type3.cached);
        RELOC_VAR(pdfont->u.simple.s.type3.Resources);
        break;
    case ft_CID_encrypted:
    case ft_CID_TrueType:
        RELOC_VAR(pdfont->u.cidfont.Widths2);
        RELOC_VAR(pdfont->u.cidfont.v);
        RELOC_VAR(pdfont->u.cidfont.CIDToGIDMap);
        RELOC_VAR(pdfont->u.cidfont.parent);
        RELOC_VAR(pdfont->u.cidfont.used2);
        break;
    default:
        RELOC_VAR(pdfont->u.simple.Encoding);
        RELOC_VAR(pdfont->u.simple.v);
        break;
    }
}
RELOC_PTRS_END

/* ---------------- Standard fonts ---------------- */

/* ------ Private ------ */

/* Define the 14 standard built-in fonts. */
#define PDF_NUM_STANDARD_FONTS 14
typedef struct pdf_standard_font_info_s {
    const char *fname;
    int size;
    gs_encoding_index_t base_encoding;
} pdf_standard_font_info_t;
static const pdf_standard_font_info_t standard_font_info[] = {
    {"Courier",                7, ENCODING_INDEX_STANDARD},
    {"Courier-Bold",          12, ENCODING_INDEX_STANDARD},
    {"Courier-Oblique",       15, ENCODING_INDEX_STANDARD},
    {"Courier-BoldOblique",   19, ENCODING_INDEX_STANDARD},
    {"Helvetica",              9, ENCODING_INDEX_STANDARD},
    {"Helvetica-Bold",        14, ENCODING_INDEX_STANDARD},
    {"Helvetica-Oblique",     17, ENCODING_INDEX_STANDARD},
    {"Helvetica-BoldOblique", 21, ENCODING_INDEX_STANDARD},
    {"Symbol",                 6, ENCODING_INDEX_SYMBOL},
    {"Times-Roman",           11, ENCODING_INDEX_STANDARD},
    {"Times-Bold",            10, ENCODING_INDEX_STANDARD},
    {"Times-Italic",          12, ENCODING_INDEX_STANDARD},
    {"Times-BoldItalic",      16, ENCODING_INDEX_STANDARD},
    {"ZapfDingbats",          12, ENCODING_INDEX_DINGBATS},
    {0}
};

/* Return the index of a standard font name, or -1 if missing. */
static int
pdf_find_standard_font_name(const byte *str, uint size)
{
    const pdf_standard_font_info_t *ppsf;

    for (ppsf = standard_font_info; ppsf->fname; ++ppsf)
        if (ppsf->size == size &&
            !memcmp(ppsf->fname, (const char *)str, size)
            )
            return ppsf - standard_font_info;
    return -1;
}

/*
 * If there is a standard font with the same appearance (CharStrings,
 * Private, WeightVector) as the given font, set *psame to the mask of
 * identical properties, and return the standard-font index; otherwise,
 * set *psame to 0 and return -1.
 */
static int
find_std_appearance(const gx_device_pdf *pdev, gs_font_base *bfont,
                    int mask, pdf_char_glyph_pair_t *pairs, int num_glyphs)
{
    bool has_uid = uid_is_UniqueID(&bfont->UID) && bfont->UID.id != 0;
    const pdf_standard_font_t *psf = pdf_standard_fonts(pdev);
    int i;

    switch (bfont->FontType) {
    case ft_encrypted:
    case ft_encrypted2:
    case ft_TrueType:
        break;
    default:
        return -1;
    }

    for (i = 0; i < PDF_NUM_STANDARD_FONTS; ++psf, ++i) {
        gs_font_base *cfont;
        int code;

        if (!psf->pdfont)
            continue;
        cfont = pdf_font_resource_font(psf->pdfont, false);
        if (has_uid) {
            /*
             * Require the UIDs to match.  The PostScript spec says this
             * is the case iff the outlines are the same.
             */
            if (!uid_equal(&bfont->UID, &cfont->UID))
                continue;
        }
        /*
         * Require the actual outlines to match (within the given subset).
         */
        code = gs_copied_can_copy_glyphs((const gs_font *)cfont,
                                         (const gs_font *)bfont,
                                         &pairs[0].glyph, num_glyphs,
                                         sizeof(pdf_char_glyph_pair_t), true);
        if (code == gs_error_unregistered) /* Debug purpose only. */
            return code;
        /* Note: code < 0 means an error. Skip it here. */
        if (code > 0)
            return i;
    }
    return -1;
}

/*
 * Scan a font directory for standard fonts.  Return true if any new ones
 * were found.  A font is recognized as standard if it was loaded as a
 * resource, it has a UniqueId, and it has a standard name.
 */
static bool
scan_for_standard_fonts(gx_device_pdf *pdev, const gs_font_dir *dir)
{
    bool found = false;
    gs_font *orig = dir->orig_fonts;

    for (; orig; orig = orig->next) {
        gs_font_base *obfont;

        if (orig->FontType == ft_composite || !orig->is_resource)
            continue;
        obfont = (gs_font_base *)orig;
        if (uid_is_UniqueID(&obfont->UID)) {
            /* Is it one of the standard fonts? */
            int i = pdf_find_standard_font_name(orig->key_name.chars,
                                                orig->key_name.size);

            if (i >= 0 && pdf_standard_fonts(pdev)[i].pdfont == 0) {
                pdf_font_resource_t *pdfont;
                int code = pdf_font_std_alloc(pdev, &pdfont, true, orig->id, obfont,
                                              i);

                if (code < 0)
                    continue;
                found = true;
            }
        }
    }
    return found;
}

/* ---------------- Initialization ---------------- */

/*
 * Allocate and initialize bookkeeping for outline fonts.
 */
pdf_outline_fonts_t *
pdf_outline_fonts_alloc(gs_memory_t *mem)
{
    pdf_outline_fonts_t *pofs =
        gs_alloc_struct(mem, pdf_outline_fonts_t, &st_pdf_outline_fonts,
                        "pdf_outline_fonts_alloc(outline_fonts)");
    pdf_standard_font_t *ppsf =
        gs_alloc_struct_array(mem, PDF_NUM_STANDARD_FONTS,
                              pdf_standard_font_t,
                              &st_pdf_standard_font_element,
                              "pdf_outline_fonts_alloc(standard_fonts)");

    if (pofs == 0 || ppsf == 0)
        return 0;
    memset(ppsf, 0, PDF_NUM_STANDARD_FONTS * sizeof(*ppsf));
    memset(pofs, 0, sizeof(*pofs));
    pofs->standard_fonts = ppsf;
    return pofs;
}

/*
 * Return the standard fonts array.
 */
pdf_standard_font_t *
pdf_standard_fonts(const gx_device_pdf *pdev)
{
    if (pdev->text != NULL && pdev->text->outline_fonts != NULL)
        return pdev->text->outline_fonts->standard_fonts;
    return NULL;
}

/*
 * Clean the standard fonts array.
 */
void
pdf_clean_standard_fonts(const gx_device_pdf *pdev)
{
    pdf_standard_font_t *ppsf = pdf_standard_fonts(pdev);

    if (ppsf != NULL)
        memset(ppsf, 0, PDF_NUM_STANDARD_FONTS * sizeof(*ppsf));
}

/* ---------------- Font resources ---------------- */

/* ------ Private ------ */

static int
pdf_resize_array(gs_memory_t *mem, void **p, int elem_size, int old_size, int new_size)
{
    void *q = gs_alloc_byte_array(mem, new_size, elem_size, "pdf_resize_array");

    if (q == NULL)
        return_error(gs_error_VMerror);
    memset((char *)q + elem_size * old_size, 0, elem_size * (new_size - old_size));
    memcpy(q, *p, elem_size * old_size);
    gs_free_object(mem, *p, "pdf_resize_array");
    *p = q;
    return 0;
}

/*
 * Allocate and (minimally) initialize a font resource.
 */
static int
font_resource_alloc(gx_device_pdf *pdev, pdf_font_resource_t **ppfres,
                    pdf_resource_type_t rtype, gs_id rid, font_type ftype,
                    int chars_count,
                    pdf_font_write_contents_proc_t write_contents)
{
    gs_memory_t *mem = pdev->pdf_memory;
    pdf_font_resource_t *pfres;
    double *widths = 0;
    byte *used = 0;
    int code;
    bool is_CID_font = (ftype == ft_CID_encrypted || ftype == ft_CID_TrueType);

    if (chars_count != 0) {
        uint size = (chars_count + 7) / 8;

        if (!is_CID_font) {
            widths = (void *)gs_alloc_byte_array(mem, chars_count, sizeof(*widths),
                                                "font_resource_alloc(Widths)");
        } else {
            /*  Delay allocation because we don't know which WMode will be used. */
        }
        used = gs_alloc_bytes(mem, size, "font_resource_alloc(used)");
        if ((!is_CID_font && widths == 0) || used == 0) {
            code = gs_note_error(gs_error_VMerror);
            goto fail;
        }
        if (!is_CID_font)
            memset(widths, 0, chars_count * sizeof(*widths));
        memset(used, 0, size);
    }
    code = pdf_alloc_resource(pdev, rtype, rid, (pdf_resource_t **)&pfres, -1L);
    if (code < 0)
        goto fail;
    memset((byte *)pfres + sizeof(pdf_resource_t), 0,
           sizeof(*pfres) - sizeof(pdf_resource_t));
    pfres->FontType = ftype;
    pfres->count = chars_count;
    pfres->Widths = widths;
    pfres->used = used;
    pfres->write_contents = write_contents;
    pfres->res_ToUnicode = NULL;
    pfres->cmap_ToUnicode = NULL;
    pfres->mark_glyph = 0;
    pfres->mark_glyph_data = 0;
    pfres->u.simple.standard_glyph_code_for_notdef = gs_c_name_glyph((const byte *)".notdef", 7) - gs_c_min_std_encoding_glyph;
    *ppfres = pfres;
    return 0;
 fail:
    gs_free_object(mem, used, "font_resource_alloc(used)");
    gs_free_object(mem, widths, "font_resource_alloc(Widths)");
    return code;
}

int font_resource_free(gx_device_pdf *pdev, pdf_font_resource_t *pdfont)
{
    if(pdfont->BaseFont.size
       && (pdfont->base_font == NULL || !pdfont->base_font->is_standard)) {

        gs_free_string(pdev->pdf_memory, pdfont->BaseFont.data, pdfont->BaseFont.size, "Free BaseFont string");
        pdfont->BaseFont.data = (byte *)0L;
        pdfont->BaseFont.size = 0;
    }
    if(pdfont->Widths) {
        gs_free_object(pdev->pdf_memory, pdfont->Widths, "Free Widths array");
        pdfont->Widths = 0;
    }
    if(pdfont->used) {
        gs_free_object(pdev->pdf_memory, pdfont->used, "Free used array");
        pdfont->used = 0;
    }
    if(pdfont->res_ToUnicode) {
        /* ToUnicode resources are tracked amd released separately */
        pdfont->res_ToUnicode = 0;
    }
    if(pdfont->cmap_ToUnicode) {
        gs_cmap_ToUnicode_free(pdev->pdf_memory, pdfont->cmap_ToUnicode);
        pdfont->cmap_ToUnicode = 0;
    }
    switch(pdfont->FontType) {
        case ft_composite:
            if (pdfont->u.type0.CMapName_data != NULL) {
                gs_free_object(pdev->memory->non_gc_memory, (byte *)pdfont->u.type0.CMapName_data, "font_resource_free(CMapName)");
                pdfont->u.type0.CMapName_data = NULL;
                pdfont->u.type0.CMapName_size = 0;
            }
            break;
        case ft_PCL_user_defined:
        case ft_MicroType:
        case ft_GL2_stick_user_defined:
        case ft_user_defined:
        case ft_PDF_user_defined:
        case ft_GL2_531:
            if(pdfont->u.simple.Encoding) {
                int ix;
                for (ix = 0; ix <= 255;ix++)
                    gs_free_object(pdev->pdf_memory->non_gc_memory, pdfont->u.simple.Encoding[ix].data, "Free copied glyph name string");
                gs_free_object(pdev->pdf_memory, pdfont->u.simple.Encoding, "Free simple Encoding");
                pdfont->u.simple.Encoding = 0;
            }
            if(pdfont->u.simple.v) {
                gs_free_object(pdev->pdf_memory, pdfont->u.simple.v, "Free simple v");
                pdfont->u.simple.v = 0;
            }
            if (pdfont->u.simple.s.type3.char_procs) {
                pdf_free_charproc_ownership(pdev, (pdf_resource_t *)pdfont->u.simple.s.type3.char_procs);
                pdfont->u.simple.s.type3.char_procs = 0;
            }
            if (pdfont->u.simple.s.type3.cached) {
                gs_free_object(pdev->pdf_memory, pdfont->u.simple.s.type3.cached, "Free type 3 cached array");
                pdfont->u.simple.s.type3.cached = NULL;
            }
            if (pdfont->u.simple.s.type3.Resources != NULL) {
                cos_free((cos_object_t *)pdfont->u.simple.s.type3.Resources, "Free type 3 Resources dictionary");
                pdfont->u.simple.s.type3.Resources = NULL;
            }
            break;
        case ft_CID_encrypted:
        case ft_CID_TrueType:
            if(pdfont->u.cidfont.Widths2) {
                gs_free_object(pdev->pdf_memory, pdfont->u.cidfont.Widths2, "Free CIDFont Widths2 array");
                pdfont->u.cidfont.Widths2 = NULL;
            }
            if(pdfont->u.cidfont.v) {
                gs_free_object(pdev->pdf_memory, pdfont->u.cidfont.v, "Free CIDFont v array");
                pdfont->u.cidfont.v = NULL;
            }
            if(pdfont->u.cidfont.used2) {
                gs_free_object(pdev->pdf_memory, pdfont->u.cidfont.used2, "Free CIDFont used2");
                pdfont->u.cidfont.used2 = 0;
            }
            if(pdfont->u.cidfont.CIDToGIDMap) {
                gs_free_object(pdev->pdf_memory, pdfont->u.cidfont.CIDToGIDMap, "Free CIDToGID map");
                pdfont->u.cidfont.CIDToGIDMap = 0;
            }
            break;
        default:
            if(pdfont->u.simple.Encoding) {
                int ix;
                for (ix = 0; ix <= 255;ix++)
                    gs_free_object(pdev->pdf_memory->non_gc_memory, pdfont->u.simple.Encoding[ix].data, "Free copied glyph name string");
                gs_free_object(pdev->pdf_memory, pdfont->u.simple.Encoding, "Free simple Encoding");
                pdfont->u.simple.Encoding = 0;
            }
            if(pdfont->u.simple.v) {
                gs_free_object(pdev->pdf_memory, pdfont->u.simple.v, "Free simple v");
                pdfont->u.simple.v = 0;
            }
            break;
    }
    if (pdfont->object) {
        gs_free_object(pdev->pdf_memory, pdfont->object, "Free font resource object");
        pdfont->object = 0;
    }
    /* We free FontDescriptor resources separately */
    if(pdfont->FontDescriptor)
        pdfont->FontDescriptor = NULL;
    else {
        if (pdfont->base_font) {
            /* Normally we free the 'base font' when we free the Font Descriptor,
             * but if we have no font descriptor (we are not embedding the font),
             * we won't free the copies of the font, or any other associated memory,
             * so do it now.
             */
            pdf_base_font_t *pbfont = pdfont->base_font;
            gs_font *copied = (gs_font *)pbfont->copied, *complete = (gs_font *)pbfont->complete;

            if (copied)
                gs_free_copied_font(copied);
            if (complete && copied != complete) {
                gs_free_copied_font(complete);
                pbfont->complete = 0;
            }
            pbfont->copied = 0;
            if (pbfont && pbfont->font_name.size) {
                gs_free_string(pdev->pdf_memory, pbfont->font_name.data, pbfont->font_name.size, "Free BaseFont FontName string");
                pbfont->font_name.data = (byte *)0L;
                pbfont->font_name.size = 0;
            }
            if (pbfont) {
                gs_free_object(pdev->pdf_memory, pbfont, "Free base font from FontDescriptor)");
                pdfont->base_font = 0;
            }
        }
    }
    return 0;
}

int
pdf_assign_font_object_id(gx_device_pdf *pdev, pdf_font_resource_t *pdfont)
{
    if (pdf_resource_id((pdf_resource_t *)pdfont) == -1) {
        int code;

        pdf_reserve_object_id(pdev, (pdf_resource_t *)pdfont, 0);
        code = pdf_mark_font_descriptor_used(pdev, pdfont->FontDescriptor);
        if (code < 0)
            return code;
        if (pdfont->FontType == 0) {
            pdf_font_resource_t *pdfont1 = pdfont->u.type0.DescendantFont;

            if (pdf_font_id(pdfont1) == -1) {
                pdf_reserve_object_id(pdev, (pdf_resource_t *)pdfont1, 0);
                code = pdf_mark_font_descriptor_used(pdev, pdfont1->FontDescriptor);
                if (code < 0)
                    return code;
            }
        }
    }
    return 0;
}

static int
font_resource_simple_alloc(gx_device_pdf *pdev, pdf_font_resource_t **ppfres,
                           gs_id rid, font_type ftype, int chars_count,
                           pdf_font_write_contents_proc_t write_contents)
{
    pdf_font_resource_t *pfres;
    int code = font_resource_alloc(pdev, &pfres, resourceFont, rid, ftype,
                                   chars_count, write_contents);

    if (code < 0)
        return code;
    pfres->u.simple.FirstChar = 256;
    pfres->u.simple.LastChar = -1;
    pfres->u.simple.BaseEncoding = -1;
    pfres->u.simple.preferred_encoding_index = -1;
    pfres->u.simple.last_reserved_char = -1;
    pfres->TwoByteToUnicode = 1;
    *ppfres = pfres;
    return 0;
}
int
font_resource_encoded_alloc(gx_device_pdf *pdev, pdf_font_resource_t **ppfres,
                            gs_id rid, font_type ftype,
                            pdf_font_write_contents_proc_t write_contents)
{
    pdf_encoding_element_t *Encoding = (pdf_encoding_element_t *)gs_alloc_bytes(pdev->pdf_memory, 256 * sizeof(pdf_encoding_element_t), "font_resource_encoded_alloc");
    gs_point *v = (gs_point *)gs_alloc_byte_array(pdev->pdf_memory,
                    256, sizeof(gs_point), "pdf_font_simple_alloc");
    pdf_font_resource_t *pdfont;
    int code, i;

    if (v == 0 || Encoding == 0) {
        gs_free_object(pdev->pdf_memory, Encoding,
                       "font_resource_encoded_alloc");
        gs_free_object(pdev->pdf_memory, v,
                       "font_resource_encoded_alloc");
        return_error(gs_error_VMerror);
    }
    code = font_resource_simple_alloc(pdev, &pdfont, rid, ftype,
                                      256, write_contents);
    if (code < 0) {
        gs_free_object(pdev->pdf_memory, Encoding,
                       "font_resource_encoded_alloc");
        gs_free_object(pdev->pdf_memory, v,
                       "font_resource_encoded_alloc");
        return_error(gs_error_VMerror);
    }
    memset(v, 0, 256 * sizeof(*v));
    memset(Encoding, 0, 256 * sizeof(*Encoding));
    for (i = 0; i < 256; ++i)
        Encoding[i].glyph = GS_NO_GLYPH;
    pdfont->u.simple.Encoding = Encoding;
    pdfont->u.simple.v = v;
    *ppfres = pdfont;
    return 0;
}

/*
 * Record whether a Type 1 or Type 2 font is a Multiple Master instance.
 */
static void
set_is_MM_instance(pdf_font_resource_t *pdfont, const gs_font_base *pfont)
{
    switch (pfont->FontType) {
    case ft_encrypted:
    case ft_encrypted2:
        pdfont->u.simple.s.type1.is_MM_instance =
            ((const gs_font_type1 *)pfont)->data.WeightVector.count > 0;
    default:
        break;
    }
}

/* ------ Generic public ------ */

/* Resize font resource arrays. */
int
pdf_resize_resource_arrays(gx_device_pdf *pdev, pdf_font_resource_t *pfres, int chars_count)
{
    /* This function fixes CID fonts that provide a lesser CIDCount than
       CIDs used in a document. Rather PS requires to print CID=0,
       we need to provide a bigger CIDCount since we don't
       re-encode the text. The text should look fine if the
       viewer application substitutes the font. */
    gs_memory_t *mem = pdev->pdf_memory;
    int code;

    if (chars_count < pfres->count)
        return 0;
    if (pfres->Widths != NULL) {
        code = pdf_resize_array(mem, (void **)&pfres->Widths, sizeof(*pfres->Widths),
                    pfres->count, chars_count);
        if (code < 0)
            return code;
    }
    code = pdf_resize_array(mem, (void **)&pfres->used, sizeof(*pfres->used),
                    (pfres->count + 7) / 8, (chars_count + 7) / 8);
    if (code < 0)
        return code;
    if (pfres->FontType == ft_CID_encrypted || pfres->FontType == ft_CID_TrueType) {
        if (pfres->u.cidfont.v != NULL) {
            code = pdf_resize_array(mem, (void **)&pfres->u.cidfont.v,
                    sizeof(*pfres->u.cidfont.v), pfres->count * 2, chars_count * 2);
            if (code < 0)
                return code;
        }
        if (pfres->u.cidfont.Widths2 != NULL) {
            code = pdf_resize_array(mem, (void **)&pfres->u.cidfont.Widths2,
                    sizeof(*pfres->u.cidfont.Widths2), pfres->count, chars_count);
            if (code < 0)
                return code;
        }
    }
    if (pfres->FontType == ft_CID_TrueType) {
        if (pfres->u.cidfont.CIDToGIDMap != NULL) {
            code = pdf_resize_array(mem, (void **)&pfres->u.cidfont.CIDToGIDMap,
                    sizeof(*pfres->u.cidfont.CIDToGIDMap), pfres->count, chars_count);
            if (code < 0)
                return code;
            pfres->u.cidfont.CIDToGIDMapLength = chars_count;
        }
    }
    if (pfres->FontType == ft_CID_encrypted || pfres->FontType == ft_CID_TrueType) {
        if (pfres->u.cidfont.used2 != NULL) {
            code = pdf_resize_array(mem, (void **)&pfres->u.cidfont.used2,
                    sizeof(*pfres->u.cidfont.used2),
                    (pfres->count + 7) / 8, (chars_count + 7) / 8);
            if (code < 0)
                return code;
        }
    }
    pfres->count = chars_count;
    return 0;
}

/* Get the object ID of a font resource. */
long
pdf_font_id(const pdf_font_resource_t *pdfont)
{
    return pdf_resource_id((const pdf_resource_t *)pdfont);
}

/*
 * Return the (copied, subset) font associated with a font resource.
 * If this font resource doesn't have one (Type 0 or Type 3), return 0.
 */
gs_font_base *
pdf_font_resource_font(const pdf_font_resource_t *pdfont, bool complete)
{
    if (pdfont->base_font != NULL)
        return pdf_base_font_font(pdfont->base_font, complete);
    if (pdfont->FontDescriptor == 0)
        return 0;
    return pdf_font_descriptor_font(pdfont->FontDescriptor, complete);
}

/*
 * Determine the embedding status of a font.  If the font is in the base
 * 14, store its index (0..13) in *pindex and its similarity to the base
 * font (as determined by the font's same_font procedure) in *psame.
 */
static bool
font_is_symbolic(const gs_font *font)
{
    if (font->FontType == ft_composite)
        return true;		/* arbitrary */
    switch (((const gs_font_base *)font)->nearest_encoding_index) {
    case ENCODING_INDEX_STANDARD:
    case ENCODING_INDEX_ISOLATIN1:
    case ENCODING_INDEX_WINANSI:
    case ENCODING_INDEX_MACROMAN:
        return false;
    default:
        return true;
    }
}
static bool
embed_list_includes(const gs_param_string_array *psa, const byte *chars,
                    uint size)
{
    uint i;

    for (i = 0; i < psa->size; ++i)
        if (!bytes_compare(psa->data[i].data, psa->data[i].size, chars, size))
            return true;
    return false;
}
static bool
embed_as_standard(gx_device_pdf *pdev, gs_font *font, int index,
                  pdf_char_glyph_pair_t *pairs, int num_glyphs)
{
    if (font->is_resource) {
        return true;
    }
    if (find_std_appearance(pdev, (gs_font_base *)font, -1,
                            pairs, num_glyphs) == index)
        return true;
    if (!scan_for_standard_fonts(pdev, font->dir))
        return false;
    return (find_std_appearance(pdev, (gs_font_base *)font, -1,
                                pairs, num_glyphs) == index);
}
static bool
has_extension_glyphs(gs_font *pfont)
{
    psf_glyph_enum_t genum;
    gs_glyph glyph;
    gs_const_string str;
    int code, j = 0, l;
    const int sl = strlen(gx_extendeg_glyph_name_separator);

    psf_enumerate_glyphs_begin(&genum, (gs_font *)pfont, NULL, 0, GLYPH_SPACE_NAME);
    for (glyph = GS_NO_GLYPH; (psf_enumerate_glyphs_next(&genum, &glyph)) != 1; ) {
        code = pfont->procs.glyph_name(pfont, glyph, &str);
        if (code < 0)
            return code;
        l = str.size - sl;
        for (j = 0; j < l; j ++)
            if (!memcmp(gx_extendeg_glyph_name_separator, str.data + j, sl))
                return true;
    }
    psf_enumerate_glyphs_reset(&genum);
    return false;
}

pdf_font_embed_t
pdf_font_embed_status(gx_device_pdf *pdev, gs_font *font, int *pindex,
                      pdf_char_glyph_pair_t *pairs, int num_glyphs)
{
    const gs_font_name *fn = &font->font_name;
    const byte *chars = fn->chars;
    uint size = fn->size;
    int index = pdf_find_standard_font_name(chars, size);
    bool embed_as_standard_called = false;
    bool do_embed_as_standard = false; /* Quiet compiler. */
    int code;
    gs_font_info_t info;

    memset(&info, 0x00, sizeof(gs_font_info_t));
    code = font->procs.font_info(font, NULL, FONT_INFO_EMBEDDING_RIGHTS, &info);
    if (code == 0 && (info.members & FONT_INFO_EMBEDDING_RIGHTS)) {
        if (((info.EmbeddingRights == 0x0002) || (info.EmbeddingRights & 0x0200))
            && !IsInWhiteList ((const char *)chars, size)) {
            /* See the OpenType specification, "The 'OS/2' and Windows Metrics Table" for details
               of the fstype parameter. This is a bitfield, currently we forbid embedding of fonts
               with these bits set:
               bit 1	0x0002	Fonts that have only this bit set must not be modified, embedded or
                                exchanged in any manner.
               bit 9	0x0200	Bitmap embedding only.

               Note for Restricted License embedding (bit 1), this must be the only level of embedding
               selected (see the OpenType spec).
             */
            char name[gs_font_name_max + 1];
            int len;

            len = min(gs_font_name_max, font->font_name.size);
            memcpy(name, font->font_name.chars, len);
            name[len] = 0;
            emprintf1(pdev->pdf_memory,
                      "\nWarning: Font %s cannot be embedded because of licensing restrictions\n",
                      name);
            return FONT_EMBED_NO;
        }
    }
    /*
     * The behavior of Acrobat Distiller changed between 3.0 (PDF 1.2),
     * which will never embed the base 14 fonts, and 4.0 (PDF 1.3), which
     * doesn't treat them any differently from any other fonts.  However,
     * if any of the base 14 fonts is not embedded, it still requires
     * special treatment.
     */
    if (pindex)
        *pindex = index;
    if (pdev->PDFX || pdev->PDFA != 0)
        return FONT_EMBED_YES;
    if (pdev->CompatibilityLevel < 1.3) {
        if (index >= 0 &&
            (embed_as_standard_called = true,
                do_embed_as_standard = embed_as_standard(pdev, font, index, pairs, num_glyphs))) {
            if (pdev->ForOPDFRead && has_extension_glyphs(font))
                return FONT_EMBED_YES;
            return FONT_EMBED_STANDARD;
        }
    }
    /* Check the Embed lists. */
    if (!embed_list_includes(&pdev->params.NeverEmbed, chars, size) ||
        (index >= 0 &&
            !(embed_as_standard_called ? do_embed_as_standard :
             (embed_as_standard_called = true,
              (do_embed_as_standard = embed_as_standard(pdev, font, index, pairs, num_glyphs)))))
        /* Ignore NeverEmbed for a non-standard font with a standard name */
        ) {
        if (pdev->params.EmbedAllFonts || font_is_symbolic(font) ||
            embed_list_includes(&pdev->params.AlwaysEmbed, chars, size))
            return FONT_EMBED_YES;
    }
    if (index >= 0 &&
        (embed_as_standard_called ? do_embed_as_standard :
         embed_as_standard(pdev, font, index, pairs, num_glyphs)))
        return FONT_EMBED_STANDARD;
    return FONT_EMBED_NO;
}

/*
 * Compute the BaseFont of a font according to the algorithm described
 * in gdevpdtf.h.
 */
int
pdf_compute_BaseFont(gx_device_pdf *pdev, pdf_font_resource_t *pdfont, bool finish)
{
    pdf_font_resource_t *pdsubf = pdfont;
    gs_string fname;
    uint size;
    byte *data;

    if (pdfont->FontType == ft_composite) {
        int code;

        pdsubf = pdfont->u.type0.DescendantFont;
        code = pdf_compute_BaseFont(pdev, pdsubf, finish);
        if (code < 0)
            return code;
        fname = pdsubf->BaseFont;
    }
    else if (pdfont->FontDescriptor == 0) {
        /* Type 3 font, or has its BaseFont computed in some other way. */
        return 0;
    } else
        fname = *pdf_font_descriptor_base_name(pdsubf->FontDescriptor);
    size = fname.size;
    data = gs_alloc_string(pdev->pdf_memory, size,
                           "pdf_compute_BaseFont");
    if (data == 0)
        return_error(gs_error_VMerror);
    memcpy(data, fname.data, size);
    switch (pdfont->FontType) {
    case ft_composite:
        /* Previously we copied the name of the original CMap onto the font name
         * but this doesn't make any sense.
         */
        break;
    case ft_encrypted:
    case ft_encrypted2:
        if (pdfont->u.simple.s.type1.is_MM_instance &&
            !pdf_font_descriptor_embedding(pdfont->FontDescriptor)
            ) {
            /* Replace spaces by underscores in the base name. */
            uint i;

            for (i = 0; i < size; ++i)
                if (data[i] == ' ')
                    data[i] = '_';
        }
        break;
    case ft_TrueType:
    case ft_CID_TrueType: {
        /* Remove spaces from the base name. */
        uint i, j;

        for (i = j = 0; i < size; ++i)
            if (data[i] != ' ')
                data[j++] = data[i];
        data = gs_resize_string(pdev->pdf_memory, data, i, j,
                                "pdf_compute_BaseFont");
        size = j;
        break;
    }
    default:
        break;
    }
    if (pdfont->BaseFont.size)
        gs_free_string(pdev->pdf_memory, pdfont->BaseFont.data, pdfont->BaseFont.size, "Replacing BaseFont string");
    pdfont->BaseFont.data = fname.data = data;
    pdfont->BaseFont.size = fname.size = size;
    /* Compute names for subset fonts. */
    if (finish && pdfont->FontDescriptor != NULL &&
        pdf_font_descriptor_is_subset(pdfont->FontDescriptor) &&
        !pdf_has_subset_prefix(fname.data, fname.size) &&
        pdf_font_descriptor_embedding(pdfont->FontDescriptor)
        ) {
        int code;
        gs_font_base *pbfont = pdf_font_resource_font(pdfont, false);

        if (pdfont->FontDescriptor)
            code = pdf_add_subset_prefix(pdev, &fname, pdfont->used, pdfont->count, pdf_fontfile_hash(pdfont->FontDescriptor));
        else
            code = pdf_add_subset_prefix(pdev, &fname, pdfont->used, pdfont->count, 0);

        if (code < 0)
            return code;
        pdfont->BaseFont = fname;

        /* Don't write a UID for subset fonts. */
        if (uid_is_XUID(&pbfont->UID)) {
            uid_free(&pbfont->UID, pbfont->memory, "gs_font_finalize");
        }
        uid_set_invalid(&pbfont->UID);
    }
    if (pdfont->FontType != ft_composite && pdsubf->FontDescriptor)
        *pdf_font_descriptor_name(pdsubf->FontDescriptor) = fname;
    return 0;
}

/* ------ Type 0 ------ */

/* Allocate a Type 0 font resource. */
int
pdf_font_type0_alloc(gx_device_pdf *pdev, pdf_font_resource_t **ppfres,
                     gs_id rid, pdf_font_resource_t *DescendantFont,
                     const gs_const_string *CMapName)
{
    int code = font_resource_alloc(pdev, ppfres, resourceFont, rid,
                                   ft_composite, 0, pdf_write_contents_type0);

    if (code >= 0) {
        byte *chars = NULL;

        (*ppfres)->u.type0.DescendantFont = DescendantFont;

        chars = gs_alloc_bytes(pdev->pdf_memory->non_gc_memory, CMapName->size, "pdf_font_resource_t(CMapName)");
        if (chars == 0)
            return_error(gs_error_VMerror);
        memcpy(chars, CMapName->data, CMapName->size);
        (*ppfres)->u.type0.CMapName_data = chars;
        (*ppfres)->u.type0.CMapName_size = CMapName->size;

        (*ppfres)->u.type0.font_index = 0;
        code = pdf_compute_BaseFont(pdev, *ppfres, false);
    }
    return code;
}

/* ------ Type 3 ------ */

/* Allocate a Type 3 font resource for sinthesyzed bitmap fonts. */
int
pdf_font_type3_alloc(gx_device_pdf *pdev, pdf_font_resource_t **ppfres,
                     pdf_font_write_contents_proc_t write_contents)
{
    return font_resource_simple_alloc(pdev, ppfres, gs_no_id, ft_user_defined,
                                      256, write_contents);
}

/* ------ Standard (base 14) Type 1 or TrueType ------ */

/* Allocate a standard (base 14) font resource. */
int
pdf_font_std_alloc(gx_device_pdf *pdev, pdf_font_resource_t **ppfres,
                   bool is_original, gs_id rid, gs_font_base *pfont, int index)
{
    pdf_font_resource_t *pdfont;
    int code = font_resource_encoded_alloc(pdev, &pdfont, rid, pfont->FontType,
                                           pdf_write_contents_std);
    const pdf_standard_font_info_t *psfi = &standard_font_info[index];
    pdf_standard_font_t *psf = &pdf_standard_fonts(pdev)[index];
    gs_matrix *orig_matrix = (is_original ? &pfont->FontMatrix : &psf->orig_matrix);

    if (code < 0 ||
        (code = pdf_base_font_alloc(pdev, &pdfont->base_font, pfont, orig_matrix, true)) < 0
        )
        return code;
    pdfont->BaseFont.data = (byte *)psfi->fname; /* break const */
    pdfont->BaseFont.size = strlen(psfi->fname);
    pdfont->mark_glyph = pfont->dir->ccache.mark_glyph;
    set_is_MM_instance(pdfont, pfont);
    if (is_original) {
        psf->pdfont = pdfont;
        psf->orig_matrix = pfont->FontMatrix;
    }
    *ppfres = pdfont;
    return 0;
}

/* ------ Other Type 1 or TrueType ------ */

/* Allocate a Type 1 or TrueType font resource. */
int
pdf_font_simple_alloc(gx_device_pdf *pdev, pdf_font_resource_t **ppfres,
                      gs_id rid, pdf_font_descriptor_t *pfd)
{
    pdf_font_resource_t *pdfont;
    int code;

    code = font_resource_encoded_alloc(pdev, &pdfont, rid,
                                           pdf_font_descriptor_FontType(pfd),
                                           pdf_write_contents_simple);

    if (code < 0)
        return(gs_note_error(code));
    pdfont->FontDescriptor = pfd;
    set_is_MM_instance(pdfont, pdf_font_descriptor_font(pfd, false));
    *ppfres = pdfont;
    return pdf_compute_BaseFont(pdev, pdfont, false);
}

/* ------ CID-keyed ------ */

/* Allocate a CIDFont resource. */
int
pdf_font_cidfont_alloc(gx_device_pdf *pdev, pdf_font_resource_t **ppfres,
                       gs_id rid, pdf_font_descriptor_t *pfd)
{
    font_type FontType = pdf_font_descriptor_FontType(pfd);
    gs_font_base *font = pdf_font_descriptor_font(pfd, false);
    int chars_count;
    int code;
    pdf_font_write_contents_proc_t write_contents;
    const gs_cid_system_info_t *pcidsi;
    ushort *map = 0;
    pdf_font_resource_t *pdfont;

    switch (FontType) {
    case ft_CID_encrypted:
        chars_count = ((const gs_font_cid0 *)font)->cidata.common.CIDCount;
        pcidsi = &((const gs_font_cid0 *)font)->cidata.common.CIDSystemInfo;
        write_contents = pdf_write_contents_cid0;
        break;
    case ft_CID_TrueType:
        chars_count = ((const gs_font_cid2 *)font)->cidata.common.CIDCount;
        pcidsi = &((const gs_font_cid2 *)font)->cidata.common.CIDSystemInfo;
        map = (void *)gs_alloc_byte_array(pdev->pdf_memory, chars_count,
                                          sizeof(*map), "CIDToGIDMap");
        if (map == 0)
            return_error(gs_error_VMerror);
        memset(map, 0, chars_count * sizeof(*map));
        write_contents = pdf_write_contents_cid2;
        break;
    default:
        return_error(gs_error_rangecheck);
    }
    code = font_resource_alloc(pdev, &pdfont, resourceCIDFont, rid, FontType,
                               chars_count, write_contents);
    if (code < 0)
        return code;
    pdfont->FontDescriptor = pfd;
    pdfont->u.cidfont.CIDToGIDMap = map;
    pdfont->u.cidfont.CIDToGIDMapLength = chars_count;
    /* fixme : Likely pdfont->u.cidfont.CIDToGIDMap duplicates
       pdfont->FontDescriptor->base_font->copied->client_data->CIDMap.
       Only difference is 0xFFFF designates unmapped CIDs.
     */
    pdfont->u.cidfont.Widths2 = NULL;
    pdfont->u.cidfont.v = NULL;
    pdfont->u.cidfont.parent = NULL;
    /* Don' know whether the font will use WMode 1,
       so reserve it now. */
    pdfont->u.cidfont.used2 = gs_alloc_bytes(pdev->pdf_memory,
                (chars_count + 7) / 8, "pdf_font_cidfont_alloc");
    if (pdfont->u.cidfont.used2 == NULL)
        return_error(gs_error_VMerror);
    memset(pdfont->u.cidfont.used2, 0, (chars_count + 7) / 8);
    /*
     * Write the CIDSystemInfo now, so we don't try to access it after
     * the font may no longer be available.
     */
    code = pdf_write_cid_systemInfo_separate(pdev, pcidsi, &pdfont->u.cidfont.CIDSystemInfo_id);
    if (code < 0)
        return code;
    *ppfres = pdfont;
    return pdf_compute_BaseFont(pdev, pdfont, false);
}

int
pdf_obtain_cidfont_widths_arrays(gx_device_pdf *pdev, pdf_font_resource_t *pdfont,
                                 int wmode, double **w, double **w0, double **v)
{
    gs_memory_t *mem = pdev->pdf_memory;
    double *ww, *vv = 0, *ww0 = 0;
    int chars_count = pdfont->count;

    *w0 = (wmode ? pdfont->Widths : NULL);
    *v = (wmode ? pdfont->u.cidfont.v : NULL);
    *w = (wmode ? pdfont->u.cidfont.Widths2 : pdfont->Widths);
    if (*w == NULL) {
        ww = (double *)gs_alloc_byte_array(mem, chars_count, sizeof(*ww),
                                                    "pdf_obtain_cidfont_widths_arrays");
        if (wmode) {
            vv = (double *)gs_alloc_byte_array(mem, chars_count, sizeof(*vv) * 2,
                                                    "pdf_obtain_cidfont_widths_arrays");
            if (pdfont->Widths == 0) {
                ww0 = (double *)gs_alloc_byte_array(mem, chars_count, sizeof(*ww0),
                                                    "pdf_obtain_cidfont_widths_arrays");
                pdfont->Widths = *w0 = ww0;
                if (ww0 != 0)
                    memset(ww0, 0, chars_count * sizeof(*ww));
            } else
                *w0 = ww0 = pdfont->Widths;
        }
        if (ww == 0 || (wmode && vv == 0) || (wmode && ww0 == 0)) {
            gs_free_object(mem, ww, "pdf_obtain_cidfont_widths_arrays");
            gs_free_object(mem, vv, "pdf_obtain_cidfont_widths_arrays");
            gs_free_object(mem, ww0, "pdf_obtain_cidfont_widths_arrays");
            return_error(gs_error_VMerror);
        }
        if (wmode)
            memset(vv, 0, chars_count * 2 * sizeof(*vv));
        memset(ww, 0, chars_count * sizeof(*ww));
        if (wmode) {
            pdfont->u.cidfont.Widths2 = *w = ww;
            pdfont->u.cidfont.v = *v = vv;
        } else {
            pdfont->Widths = *w = ww;
            *v = NULL;
        }
    }
    return 0;
}

/*
 * Convert True Type fonts into CID fonts for PDF/A.
 */
int
pdf_convert_truetype_font(gx_device_pdf *pdev, pdf_resource_t *pres)
{
    if (pdev->PDFA == 0)
        return 0;
    else {
        pdf_font_resource_t *pdfont = (pdf_font_resource_t *)pres;

        if (pdfont->FontType != ft_TrueType)
            return 0;
        else if (pdf_resource_id(pres) == -1)
            return 0; /* An unused font. */
        else {
            int code = pdf_different_encoding_index(pdfont, 0);

            if (code < 0)
                return code;
            if (code == 256 && pdfont->u.simple.BaseEncoding != ENCODING_INDEX_UNKNOWN)
                return 0;
            {	/* The encoding have a difference - do convert. */
                pdf_font_resource_t *pdfont0;
                gs_const_string CMapName = {(const byte *)"OneByteIdentityH", 16};

                code = pdf_convert_truetype_font_descriptor(pdev, pdfont);
                if (code < 0)
                    return code;
                code = pdf_font_type0_alloc(pdev, &pdfont0, pres->rid + 1, pdfont, &CMapName);
                if (code < 0)
                    return code;
                /* Pass the font object ID to the type 0 font resource. */
                pdf_reserve_object_id(pdev, (pdf_resource_t *)pdfont0, pdf_resource_id(pres));
                pdf_reserve_object_id(pdev, (pdf_resource_t *)pdfont, gs_no_id);
                /* Set Encoding_name because we won't call attach_cmap_resource for it : */
                code = pdf_write_OneByteIdentityH(pdev);
                if (code < 0)
                    return 0;
                pdfont->u.cidfont.CIDSystemInfo_id = pdev->IdentityCIDSystemInfo_id;
                gs_snprintf(pdfont0->u.type0.Encoding_name, sizeof(pdfont0->u.type0.Encoding_name),
                            "%ld 0 R", pdf_resource_id(pdev->OneByteIdentityH));
                /* Move ToUnicode : */
                pdfont0->res_ToUnicode = pdfont->res_ToUnicode; pdfont->res_ToUnicode = 0;
                pdfont0->cmap_ToUnicode = pdfont->cmap_ToUnicode; pdfont->cmap_ToUnicode = 0;
                /* Change the font type to CID font : */
                pdfont->FontType = ft_CID_TrueType;
                pdfont->write_contents = pdf_write_contents_cid2;
                return 0;
            }
        }
    }
}

/* ---------------- CMap resources ---------------- */

/*
 * Allocate a CMap resource.
 */
int
pdf_cmap_alloc(gx_device_pdf *pdev, const gs_cmap_t *pcmap,
               pdf_resource_t **ppres, int font_index_only)
{
    return pdf_write_cmap(pdev, pcmap, ppres, font_index_only);
}
