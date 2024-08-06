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


/* Font and CMap resource structure and API for pdfwrite */

#ifndef gdevpdtf_INCLUDED
#  define gdevpdtf_INCLUDED

#include "gdevpdtx.h"
#include "gsfcmap.h"

/* ================ Types and structures ================ */

/* ---------------- Font resources ---------------- */

/*
 * pdfwrite manages several different flavors of font resources:
 *
 *  Those that have neither a FontDescriptor nor a base_font:
 *	Type 0 (composite) fonts
 *  Those that have no FontDescriptor, but do have a base_font:
 *	Standard 14 fonts
 *  Those that have a FontDescriptor but no base_font:
 *	Type 3 bitmap fonts
 *  Those that have a FontDescriptor with a base_font:
 *	Type 1 / Type 2 fonts
 *	Type 42 (TrueType) fonts
 *	CIDFontType 0 (Type 1/2) CIDFonts
 *	CIDFontType 2 (TrueType) CIDFonts
 */
/*
 * Font names in PDF files have caused an enormous amount of trouble, so we
 * document specifically how they are handled in each structure.
 *
 * The PDF Reference specifies the BaseFont of a font resource as follows,
 * depending on the font type:
 *
 *   Type 0 - if the descendant font is CIDFontType 0, the descendant font
 *     name followed by a hyphen and the CMap name (the value of Encoding,
 *     if a name, otherwise the CMapName from the CMap); if the descendant
 *     font is CIDFontType 2, the descendant font name.
 *
 *   Type 1 - "usually" the same as the FontName in the base font.
 *
 *   MM Type 1 - if embedded, the same as Type 1; if not embedded, spaces
 *     in the font name are replaced with underscores.
 *
 *   Type 3 - not used.
 *
 *   TrueType - initially, the PostScript name from the 'name' table in
 *     the font; if none, the "name by which the font is known in the host
 *     operating system".  Spaces are removed.  Then, under circumstances
 *     not defined, the string ",Bold", ",Italic", or ",BoldItalic" is
 *     appended if the font has the corresponding style properties.
 *     [We do not do this: we simply use the key_name or font_name.]
 *
 *   CIDFontType 0 - "usually" the same as the CIDFontName in the base font.
 *
 *   CIDFontType 2 - the same as TrueType.
 *
 * In addition, the BaseFont has a XXXXXX+ prefix if the font is a subset
 * (whether embedded or not).
 *
 * We would like to compute the BaseFont at the time that we create the font
 * resource object.  The font descriptor (which is needed to provide
 * information about embedding) and the base font are both available at that
 * time.  Unfortunately, the information as to whether the font will be
 * subsetted is not available.  Therefore, we do compute the BaseFont from
 * the base font name when the font resource is created, to allow checking
 * for duplicate names and for standard font names, but we compute it again
 * after writing out the base font.
 */

#ifndef pdf_font_descriptor_DEFINED
#  define pdf_font_descriptor_DEFINED
typedef struct pdf_font_descriptor_s pdf_font_descriptor_t;
#endif

typedef struct pdf_char_glyph_pair_s pdf_char_glyph_pair_t;

struct pdf_char_glyph_pair_s {
    gs_char chr;
    gs_glyph glyph;
};

/*
 * The write_contents procedure is set by the implementation when the
 * font resource is created.  It is called after generic code has opened
 * the resource object and written the Type, BaseFont (if any),
 * FontDescriptor reference (if any), ToUnicode CMap reference (if any),
 * and additional dictionary entries (if any).
 * The write_contents procedure must write any remaining entries specific
 * to the FontType, followed by the closing ">>", and then call
 * pdf_end_separate.  The reason for this division of function is to allow
 * the write_contents procedure to write additional objects that the
 * resource object references, after closing the resource object.
 */
typedef int (*pdf_font_write_contents_proc_t)
     (gx_device_pdf *, pdf_font_resource_t *);

/*
 * Define an element of an Encoding.  The element is unused if glyph ==
 * GS_NO_GLYPH.
 */
typedef struct pdf_encoding_element_s {
    gs_glyph glyph;
    byte *data;
    uint size;
    bool is_difference;		/* true if must be written in Differences */
} pdf_encoding_element_t;

static inline int pdf_copy_string_to_encoding(gx_device_pdf *pdev, gs_const_string *gnstr, pdf_encoding_element_t *pet)
{
    byte *p = NULL;

    p = gs_alloc_bytes(pdev->pdf_memory->non_gc_memory, gnstr->size, "pdf_copy_string_to_encoding");
    if (p == NULL)
        return_error(gs_error_VMerror);
    memcpy(p, gnstr->data, gnstr->size);
    if (pet->data != NULL)
        gs_free_object(pdev->pdf_memory->non_gc_memory, pet->data, "pdf_copy_string_to_encoding free existing glyph name");
    pet->data = p;
    pet->size = gnstr->size;
    return 0;
}

#define private_st_pdf_encoding1() /* gdevpdtf.c */\
  gs_private_st_const_strings1(st_pdf_encoding1,\
    pdf_encoding_element_t, "pdf_encoding_element_t",\
    pdf_encoding1_enum_ptrs, pdf_encoding1_reloc_ptrs, bah)
#define private_st_pdf_encoding_element() /* gdevpdtf.c */\
  gs_private_st_element(st_pdf_encoding_element, pdf_encoding_element_t,\
    "pdf_encoding_element_t[]", pdf_encoding_elt_enum_ptrs,\
    pdf_encoding_elt_reloc_ptrs, st_pdf_encoding1)

struct pdf_base_font_s {
    /*
     * For the standard 14 fonts, copied == complete is a complete copy
     * of the font, and DO_SUBSET = NO.
     *
     * For fonts that MAY be subsetted, copied is a partial copy,
     * complete is a complete copy, and DO_SUBSET = UNKNOWN until
     * pdf_font_do_subset is called.
     *
     * For fonts that MUST be subsetted, copied == complete is a partial
     * copy, and DO_SUBSET = YES.
     */
    gs_font_base *copied;
    gs_font_base *complete;
    enum {
        DO_SUBSET_UNKNOWN = 0,
        DO_SUBSET_NO,
        DO_SUBSET_YES
    } do_subset;
    bool is_standard;
    /*
     * For CIDFonts, which are always subsetted, num_glyphs is CIDCount.
     * For optionally subsetted fonts, num_glyphs is the count of glyphs
     * in the font when originally copied.  Note that if the font is
     * downloaded incrementally, num_glyphs may be 0.
     */
    int num_glyphs;
    byte *CIDSet;		/* for CIDFonts */
    int CIDSetLength;
    gs_string font_name;
    bool written;
    cos_dict_t *FontFile;
};
#define private_st_pdf_base_font()\
BASIC_PTRS(pdf_base_font_ptrs) {\
    GC_OBJ_ELT(pdf_base_font_t, copied),\
    GC_OBJ_ELT(pdf_base_font_t, complete),\
    GC_OBJ_ELT(pdf_base_font_t, CIDSet),\
    GC_OBJ_ELT(pdf_base_font_t, FontFile),\
    GC_STRING_ELT(pdf_base_font_t, font_name)\
}

typedef struct {
    gs_id id;
    pdf_resource_type_t type;
} pdf_resource_ref_t;

/*
 * Widths are the widths in the outlines: this is what PDF interpreters
 * use, and what will be written in the PDF file.  real_widths are the
 * widths possibly modified by Metrics[2] and CDevProc: these define the
 * actual advance widths of the characters in the PostScript text.
 */
struct pdf_font_resource_s {
    pdf_resource_common(pdf_font_resource_t);
    font_type FontType;		/* copied from font, if any */
    int64_t XUID;
    pdf_font_write_contents_proc_t write_contents;
    gs_string BaseFont;		/* (not used for Type 3) */
    pdf_font_descriptor_t *FontDescriptor; /* (not used for Type 0, Type 3, */
                                /* or standard 14 fonts) */
    /*
     * The base_font member is only used for
     * the standard 14 fonts, which do not have a FontDescriptor.
     */
    pdf_base_font_t *base_font;	/* == FontDescriptor->base_font */
    uint count;			/* # of chars/CIDs */
    double *Widths;		/* [count] (not used for Type 0) */
    byte *used;			/* [ceil(count/8)] bitmap of chars/CIDs used */
                                /* (not used for Type 0 or Type 3) */
    pdf_resource_t *res_ToUnicode; /* CMap (not used for CIDFonts) */
    gs_cmap_t *cmap_ToUnicode;	   /* CMap (not used for CIDFonts) */
    gs_glyph_mark_proc_t mark_glyph;
    void *mark_glyph_data;	/* closure data */

    /* We use this when determining whether we should use an existing ToUnicode
     * CMap or just use the Encoding, for s aimple font. Even if the Encoding
     * only uses named glyphs, with names we can understand, the original
     * ToUnicode may have mapped these in a non-standard way.
     * See Bug #702201 where the ffi ligature is mapped to 3 code points
     */
    int TwoByteToUnicode;

    union {

        struct /*type0*/ {

            pdf_font_resource_t *DescendantFont; /* CIDFont */
            /*
             * The Encoding_name must be long enough to hold either the
             * longest standard CMap name defined in the PDF Reference,
             * or the longest reference to an embedded CMap (# 0 R).
             */
            char Encoding_name[max( /* standard name or <id> 0 R */
                      17,	/* /UniJIS-UCS2-HW-H */
                      sizeof(int64_t) * 8 / 3 + 1 + 4 /* <id> 0 R */
                      ) + 1	/* \0 terminator */
            ];
            byte *CMapName_data;
            uint CMapName_size;
            uint font_index;	/* The index of the descendent font in the source CMap. */
            bool cmap_is_standard;
            int WMode;		/* of CMap */

        } type0;

        struct /*cidfont*/ {

            /* [D]W[2] is Widths. */
            int64_t CIDSystemInfo_id; /* (written when font is allocated) */
            ushort *CIDToGIDMap; /* (CIDFontType 2 only) [count] */
            unsigned int CIDToGIDMapLength;
            gs_id glyphshow_font_id;
            double *Widths2;	/* [count * 2] (x, y) */
            double *v;		/* [count] */
            byte *used2;	/* [(count + 7) / 8] */
            pdf_font_resource_t *parent;

        } cidfont;

        struct /*simple*/ {

            int FirstChar, LastChar; /* 0 <= FirstChar <= LastChar <= 255 */
            /*
             * The BaseEncoding can only be ENCODING_INDEX_WINANSI,
             * ENCODING_INDEX_MACROMAN, ENCODING_INDEX_MACEXPERT, or -1.
             */
            gs_encoding_index_t BaseEncoding;
            gs_encoding_index_t preferred_encoding_index;
            pdf_encoding_element_t *Encoding; /* [256], not for Type 3 */
            gs_point *v; /* [256], glyph origin for WMode 1 */
            int last_reserved_char; /* Except for synthesised Type 3,
                                           which stores such data in LastChar */
            gs_glyph standard_glyph_code_for_notdef;
            union {

                struct /*type1*/ {
                    bool is_MM_instance;
                } type1;

                struct /*truetype*/ {
                    /*
                     * No extra info needed, but the ANSI standard doesn't
                     * allow empty structs.
                     */
                    int _dummy;
                } truetype;

                struct /*type3*/ {
                    gs_rect FontBBox;
                    gs_matrix FontMatrix;
                    pdf_char_proc_ownership_t *char_procs;
                    int max_y_offset;
                    bool bitmap_font;
                    cos_dict_t *Resources;
                    byte *cached;
                } type3;

            } s;

        } simple;

    } u;
};
/* The GC descriptor for resource types must be public. */
#define public_st_pdf_font_resource() /* gdevpdtf.c */\
  gs_public_st_composite(st_pdf_font_resource, pdf_font_resource_t,\
    "pdf_font_resource_t", pdf_font_resource_enum_ptrs,\
    pdf_font_resource_reloc_ptrs)

/*
 * Define the possible embedding statuses of a font.
 */
typedef enum {
    FONT_EMBED_STANDARD,	/* 14 standard fonts */
    FONT_EMBED_NO,
    FONT_EMBED_YES
} pdf_font_embed_t;

/* ---------------- Global structures ---------------- */

/*
 * Define a structure for keeping track of the (unique) resource for
 * each standard font.  Note that standard fonts do not have descriptors:
 * the base_font and copied_font members of the font resource provide the
 * necessary information.
 */
typedef struct pdf_standard_font_s {
    pdf_font_resource_t *pdfont;
    gs_matrix orig_matrix;	/* ****** do we need this? */
} pdf_standard_font_t;
#define private_st_pdf_standard_font() /* gdevpdtf.c */\
  gs_private_st_ptrs1(st_pdf_standard_font, pdf_standard_font_t,\
    "pdf_standard_font_t", pdf_std_font_enum_ptrs, pdf_std_font_reloc_ptrs,\
    pdfont)
#define private_st_pdf_standard_font_element() /* gdevpdtf.c */\
  gs_private_st_element(st_pdf_standard_font_element, pdf_standard_font_t,\
    "pdf_standard_font_t[]", pdf_std_font_elt_enum_ptrs,\
    pdf_std_font_elt_reloc_ptrs, st_pdf_standard_font)

/*
 * There is a single instance (per device) of a structure that tracks global
 * information about outline fonts.  It is defined here, rather than
 * opaquely in the implementation file, because the text processing code
 * needs access to it.
 */

/*typedef struct pdf_outline_fonts_s pdf_outline_fonts_t;*/  /* gdevpdtx.h */
struct pdf_outline_fonts_s {
    pdf_standard_font_t *standard_fonts; /* [PDF_NUM_STANDARD_FONTS] */
};
#define private_st_pdf_outline_fonts() /* gdevpdtf.c */\
  gs_private_st_ptrs1(st_pdf_outline_fonts, pdf_outline_fonts_t,\
    "pdf_outline_fonts_t", pdf_outline_fonts_enum_ptrs,\
    pdf_outline_fonts_reloc_ptrs, standard_fonts)

/* ================ Procedures ================ */

/* ---------------- Font resources ---------------- */

/*
 * Allocate and initialize bookkeeping for outline fonts.
 */
pdf_outline_fonts_t *pdf_outline_fonts_alloc(gs_memory_t *mem);

/*
 * Return the standard fonts array.
 */
pdf_standard_font_t *pdf_standard_fonts(const gx_device_pdf *pdev);

/*
 * Clean the standard fonts array.
 */
void pdf_clean_standard_fonts(const gx_device_pdf *pdev);

/* Free font cache. */
int pdf_free_font_cache(gx_device_pdf *pdev);

/*
 * Allocate specific types of font resource.
 */
int pdf_font_type0_alloc(gx_device_pdf *pdev, pdf_font_resource_t **ppfres,
                         gs_id rid, pdf_font_resource_t *DescendantFont,
                         const gs_const_string *CMapName);
int pdf_font_type3_alloc(gx_device_pdf *pdev, pdf_font_resource_t **ppfres,
                         pdf_font_write_contents_proc_t write_contents);
int pdf_font_std_alloc(gx_device_pdf *pdev, pdf_font_resource_t **ppfres,
                   bool is_original, gs_id rid, gs_font_base *pfont, int index);
int pdf_font_simple_alloc(gx_device_pdf *pdev, pdf_font_resource_t **ppfres,
                          gs_id rid, pdf_font_descriptor_t *pfd);
int pdf_font_cidfont_alloc(gx_device_pdf *pdev, pdf_font_resource_t **ppfres,
                           gs_id rid, pdf_font_descriptor_t *pfd);
int pdf_obtain_cidfont_widths_arrays(gx_device_pdf *pdev, pdf_font_resource_t *pdfont,
                    int wmode, double **w, double **w0, double **v);
int font_resource_encoded_alloc(gx_device_pdf *pdev, pdf_font_resource_t **ppfres,
                            gs_id rid, font_type ftype,
                            pdf_font_write_contents_proc_t write_contents);
int pdf_assign_font_object_id(gx_device_pdf *pdev, pdf_font_resource_t *pdfont);

int font_resource_free(gx_device_pdf *pdev, pdf_font_resource_t *pdfont);

/* Resize font resource arrays. */
int pdf_resize_resource_arrays(gx_device_pdf *pdev, pdf_font_resource_t *pfres,
        uint chars_count);

/*
 * Return the (copied, subset or complete) font associated with a font resource.
 * If this font resource doesn't have a descriptor (Type 0, Type 3, or
 * standard 14), return 0.
 */
gs_font_base *pdf_font_resource_font(const pdf_font_resource_t *pdfont, bool complete);

/*
 * Determine the embedding status of a font.  If the font is in the base
 * 14, store its index (0..13) in *pindex and its similarity to the base
 * font (as determined by the font's same_font procedure) in *psame.
 * (pindex and/or psame may be NULL.)
 */
pdf_font_embed_t pdf_font_embed_status(gx_device_pdf *pdev, gs_font *font,
                                       int *pindex,
                                       pdf_char_glyph_pair_t *pairs, int num_glyphs, font_type *orig_type);

/*
 * Compute the BaseFont of a font according to the algorithm described
 * above.
 */
int pdf_compute_BaseFont(gx_device_pdf *pdev, pdf_font_resource_t *pdfont, bool finish);

/*
 * Convert True Type fonts into CID fonts for PDF/A.
 */
int pdf_convert_truetype_font(gx_device_pdf *pdev,  pdf_resource_t *pres);

/*
 * Convert True Type font descriptor into CID font descriptor for PDF/A.
 */
int pdf_convert_truetype_font_descriptor(gx_device_pdf *pdev, pdf_font_resource_t *pdfont);

/* ---------------- CMap resources ---------------- */

/*
 * Allocate a CMap resource.
 */
int pdf_cmap_alloc(gx_device_pdf *pdev, const gs_cmap_t *pcmap,
                   pdf_resource_t **ppres /* CMap */, int font_index_only);

/*
 * Add a CID-to-GID mapping to a CIDFontType 2 font resource.
 */
int pdf_font_add_cid_to_gid(pdf_font_resource_t *pdfont, uint cid, uint gid);

#endif /* gdevpdtf_INCLUDED */
