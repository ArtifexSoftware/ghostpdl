/* Copyright (C) 1996, 1997, 1998, 1999 Aladdin Enterprises.  All rights reserved.

   This file is part of Aladdin Ghostscript.

   Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
   or distributor accepts any responsibility for the consequences of using it,
   or for whether it serves any particular purpose or works at all, unless he
   or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
   License (the "License") for full details.

   Every copy of Aladdin Ghostscript must include a copy of the License,
   normally in a plain ASCII text file named PUBLIC.  The License grants you
   the right to copy, modify and redistribute Aladdin Ghostscript, but only
   under certain conditions described in the License.  Among other things, the
   License requires that the copyright notice and this notice be preserved on
   all copies.
 */


/* Internal definitions for PDF-writing driver. */

#ifndef gdevpdfx_INCLUDED
#  define gdevpdfx_INCLUDED

#include "gsparam.h"
#include "gsuid.h"
#include "gxdevice.h"
#include "gxfont.h"
#include "gxline.h"
#include "stream.h"
#include "spprint.h"
#include "gdevpsdf.h"
#include "gdevpdfo.h"

/* ---------------- Statically allocated sizes ---------------- */
/* These should really be dynamic.... */

/* Define the maximum depth of an outline tree. */
/* Note that there is no limit on the breadth of the tree. */
#define MAX_OUTLINE_DEPTH 8

/* Define the maximum size of a destination array string. */
#define MAX_DEST_STRING 80

/* ================ Types and structures ================ */

/* Define the possible contexts for the output stream. */
typedef enum {
    PDF_IN_NONE,
    PDF_IN_STREAM,
    PDF_IN_TEXT,
    PDF_IN_STRING
} pdf_context_t;

/* ---------------- Resources ---------------- */

typedef enum {
    /*
     * Standard PDF resources.  Font must be last, because resources
     * up to but not including Font are written page-by-page.
     */
    resourceColorSpace,
    /*resourceExtGState,*/	/* not needed */
    resourcePattern,
    /*resourceShading,*/	/* not needed */
    resourceXObject,
    resourceFont,
    /*
     * Internally used (pseudo-)resources.
     */
    resourceCharProc,
    NUM_RESOURCE_TYPES
} pdf_resource_type_t;

#define pdf_resource_type_names\
  "ColorSpace", "Pattern", "XObject", "Font",\
  0
#define pdf_resource_type_structs\
  &st_pdf_resource, &st_pdf_resource, &st_pdf_resource, &st_pdf_font,\
  &st_pdf_char_proc

#define pdf_resource_common(typ)\
    typ *next;			/* next resource of this type */\
    pdf_resource_t *prev;	/* previously allocated resource */\
    gs_id rid;			/* optional ID key */\
    bool named;\
    bool used_on_page;\
    cos_object_t *object
typedef struct pdf_resource_s pdf_resource_t;
struct pdf_resource_s {
    pdf_resource_common(pdf_resource_t);
};

#define private_st_pdf_resource()\
  gs_private_st_ptrs3(st_pdf_resource, pdf_resource_t, "pdf_resource_t",\
    pdf_resource_enum_ptrs, pdf_resource_reloc_ptrs, next, prev, object)

/* ------ Fonts ------ */

/*
 * The PDF writer creates 3 different kinds of font resources:
 *
 *	- Synthesized Type 3 bitmap fonts, identified by num_chars != 0
 *	(or equivalently PDF_FONT_IS_SYNTHESIZED = true).
 *
 *	- Non-embedded Type 1 or TrueType fonts, identified by num_chars == 0,
 *	descriptor != 0, descriptor->FontFile_id == 0.  A font is in the
 *	base 14 iff index >= 0.
 *
 *	- Embedded Type 1 or TrueType fonts, identified by num_chars == 0,
 *	descriptor != 0, descriptor->FontFile_id != 0.
 */

#define PDF_NUM_STD_FONTS 14
#define pdf_do_std_fonts(m)\
  m("Courier", ENCODING_INDEX_STANDARD)\
  m("Courier-Bold", ENCODING_INDEX_STANDARD)\
  m("Courier-Oblique", ENCODING_INDEX_STANDARD)\
  m("Courier-BoldOblique", ENCODING_INDEX_STANDARD)\
  m("Helvetica", ENCODING_INDEX_STANDARD)\
  m("Helvetica-Bold", ENCODING_INDEX_STANDARD)\
  m("Helvetica-Oblique", ENCODING_INDEX_STANDARD)\
  m("Helvetica-BoldOblique", ENCODING_INDEX_STANDARD)\
  m("Symbol", ENCODING_INDEX_SYMBOL)\
  m("Times-Roman", ENCODING_INDEX_STANDARD)\
  m("Times-Bold", ENCODING_INDEX_STANDARD)\
  m("Times-Italic", ENCODING_INDEX_STANDARD)\
  m("Times-BoldItalic", ENCODING_INDEX_STANDARD)\
  m("ZapfDingbats", ENCODING_INDEX_DINGBATS)

/* Font descriptors (not handled as separate resources) */
typedef struct pdf_font_descriptor_s {
    long id;
    /* Required elements */
    int Ascent, CapHeight, Descent, ItalicAngle, StemV;
    gs_int_rect FontBBox;
    uint Flags;
    /* Optional elements (default to 0) */
    long FontFile_id;
    int AvgWidth, Leading, MaxWidth, MissingWidth, StemH, XHeight;
} pdf_font_descriptor_t;
/* Flag bits */
/*#define FONT_IS_FIXED_WIDTH (1<<0)*/  /* define in gxfont.h */
#define FONT_IS_SERIF (1<<1)
#define FONT_IS_SYMBOLIC (1<<2)
#define FONT_IS_SCRIPT (1<<3)
#define FONT_IS_ADOBE_ROMAN (1<<5)
#define FONT_IS_ITALIC (1<<6)
#define FONT_IS_ALL_CAPS (1<<16)
#define FONT_IS_SMALL_CAPS (1<<17)
#define FONT_IS_FORCE_BOLD (1<<18)
#define private_st_pdf_font_descriptor()\
  gs_private_st_simple(st_pdf_font_descriptor, pdf_font_descriptor_t,\
    "pdf_font_descriptor_t")

/* Font resources */
typedef struct pdf_char_proc_s pdf_char_proc_t;	/* forward reference */
typedef struct pdf_font_s pdf_font_t;
/*
 * PDF font names must be large enough for the 14 built-in fonts,
 * and also large enough for any reasonable font name + 7 characters
 * for the subsetting prefix.
 */
#define MAX_PDF_FONT_NAME (7 + gs_font_name_max + 1)
typedef struct pdf_font_name_s {
    byte chars[MAX_PDF_FONT_NAME];
    uint size;
} pdf_font_name_t;
typedef struct pdf_encoding_element_s {
    gs_glyph glyph;
    gs_const_string str;
} pdf_encoding_element_t;
#define private_st_pdf_encoding_element()\
  gs_private_st_composite(st_pdf_encoding_element, pdf_encoding_element_t,\
    "pdf_encoding_element_t[]", pdf_encoding_elt_enum_ptrs,\
    pdf_encoding_elt_reloc_ptrs)
struct pdf_font_s {
    pdf_resource_common(pdf_font_t);
    pdf_font_name_t fname;
    font_type FontType;
    gs_font *font;		/* non-0 iff font will notify us; */
				/* should be a weak pointer */
    int index;			/* in pdf_standard_fonts */
    gs_matrix orig_matrix;	/* FontMatrix of unscaled font for embedding */
    /*
     * For synthesized fonts, frname is A, B, ...; for other fonts,
     * frname is R<id>.  The string is null-terminated.
     */
    char frname[1/*R*/ + (sizeof(long) * 8 / 3 + 1) + 1/*\0*/];
    /* Encoding differences for base fonts. */
    byte chars_used[32];	/* 1 bit per character code */
    pdf_encoding_element_t *differences; /* [256] */
    long diff_id;
    /* Bookkeeping for non-synthesized fonts. */
    pdf_font_descriptor_t *descriptor;
    int Widths[256];
    byte widths_known[32];	/* 1 bit per character code */
    bool skip;			/* font was already written, skip it */
    /* Bookkeeping for synthesized fonts. */
    int num_chars;
#define PDF_FONT_IS_SYNTHESIZED(pdfont) ((pdfont)->num_chars != 0)
    pdf_char_proc_t *char_procs;
    int max_y_offset;
    /* Pseudo-characters for spacing. */
    /* The range should be determined by the device resolution.... */
#define X_SPACE_MIN 24
#define X_SPACE_MAX 150
    byte spaces[X_SPACE_MAX - X_SPACE_MIN + 1];
};

#define private_st_pdf_font()\
  gs_private_st_suffix_add4(st_pdf_font, pdf_font_t, "pdf_font_t",\
    pdf_font_enum_ptrs, pdf_font_reloc_ptrs, st_pdf_resource,\
    font, differences, descriptor, char_procs)

/* CharProc pseudo-resources for synthesized fonts */
struct pdf_char_proc_s {
    pdf_resource_common(pdf_char_proc_t);
    pdf_font_t *font;
    pdf_char_proc_t *char_next;	/* next char_proc for same font */
    int width, height;
    int x_width;		/* X escapement */
    int y_offset;		/* of character (0,0) */
    byte char_code;
};

#define private_st_pdf_char_proc()\
  gs_private_st_suffix_add2(st_pdf_char_proc, pdf_char_proc_t,\
    "pdf_char_proc_t", pdf_char_proc_enum_ptrs,\
    pdf_char_proc_reloc_ptrs, st_pdf_resource, font, char_next)

/* ------ Named objects ------ */

/* Define an element of the graphics object accumulation (BP/EP) stack. */
typedef struct pdf_graphics_save_s pdf_graphics_save_t;
struct pdf_graphics_save_s {
    pdf_graphics_save_t *prev;
    cos_stream_t *object;
    long position;
    pdf_context_t save_context;
    long save_contents_id;
};

#define private_st_pdf_graphics_save()	/* in gdevpdfm.c */\
  gs_private_st_ptrs2(st_pdf_graphics_save, pdf_graphics_save_t,\
    "pdf_graphics_save_t", pdf_graphics_save_enum_ptrs,\
    pdf_graphics_save_reloc_ptrs, prev, object)

/* ---------------- Other auxiliary structures ---------------- */

/* Outline nodes and levels */
typedef struct pdf_outline_node_s {
    long id, parent_id, prev_id, first_id, last_id;
    int count;
    cos_dict_t *action;
} pdf_outline_node_t;
typedef struct pdf_outline_level_s {
    pdf_outline_node_t first;
    pdf_outline_node_t last;
    int left;
} pdf_outline_level_t;
/*
 * The GC descriptor is implicit, since outline levels occur only in an
 * embedded array in the gx_device_pdf structure.
 */

/* Articles */
typedef struct pdf_bead_s {
    long id, article_id, prev_id, next_id, page_id;
    gs_rect rect;
} pdf_bead_t;
typedef struct pdf_article_s pdf_article_t;
struct pdf_article_s {
    pdf_article_t *next;
    cos_dict_t *contents;
    pdf_bead_t first;
    pdf_bead_t last;
};

#define private_st_pdf_article()\
  gs_private_st_ptrs2(st_pdf_article, pdf_article_t,\
    "pdf_article_t", pdf_article_enum_ptrs, pdf_article_reloc_ptrs,\
    next, contents)

/* ---------------- The device structure ---------------- */

/* Text state */
typedef struct pdf_std_font_s {
    gs_matrix orig_matrix;
    gs_uid uid;
} pdf_std_font_t;
typedef struct pdf_text_state_s {
    /* State parameters */
    float character_spacing;
    pdf_font_t *font;
    floatp size;
    float word_spacing;
    float horizontal_scaling;
    /* Bookkeeping */
    gs_matrix matrix;		/* relative to device space, not user space */
    gs_point line_start;
    gs_point current;
#define max_text_buffer 200	/* arbitrary, but overflow costs 5 chars */
    byte buffer[max_text_buffer];
    int buffer_count;
    pdf_std_font_t std_fonts[PDF_NUM_STD_FONTS];  /* must be last (can't initialize) */
} pdf_text_state_t;

#define pdf_text_state_default\
  0, NULL, 0, 0, 100,\
  { identity_matrix_body }, { 0, 0 }, { 0, 0 }, { 0 }, 0

/* Resource lists */
#define NUM_RESOURCE_CHAINS 16
typedef struct pdf_resource_list_s {
    pdf_resource_t *chains[NUM_RESOURCE_CHAINS];
} pdf_resource_list_t;

/* Define the hash function for gs_ids. */
#define gs_id_hash(rid) ((rid) + ((rid) / NUM_RESOURCE_CHAINS))
/* Define the accessor for the proper hash chain. */
#define PDF_RESOURCE_CHAIN(pdev, type, rid)\
  (&(pdev)->resources[type].chains[gs_id_hash(rid) % NUM_RESOURCE_CHAINS])

/* Define the bookkeeping for an open stream. */
typedef struct pdf_stream_position_s {
    long length_id;
    long start_pos;
} pdf_stream_position_t;

/* Define the mask for which procsets have been used on a page. */
typedef enum {
    NoMarks = 0,
    ImageB = 1,
    ImageC = 2,
    ImageI = 4,
    Text = 8
} pdf_procset;

/*
 * Define the stored information for a page.  Because pdfmarks may add
 * information to any page anywhere in the document, we have to wait
 * until the end to write out the page dictionaries.
 */
typedef struct pdf_page_s {
    cos_dict_t *Page;
    gs_int_point MediaBox;
    pdf_procset procsets;
    long contents_id;
    long resource_ids[resourceFont]; /* resources up to Font, see above */
    long fonts_id;
    cos_array_t *Annots;
} pdf_page_t;
#define private_st_pdf_page()	/* in gdevpdf.c */\
  gs_private_st_ptrs2(st_pdf_page, pdf_page_t, "pdf_page_t",\
    pdf_page_enum_ptrs, pdf_page_reloc_ptrs, Page, Annots)

/*
 * Define the structure for the temporary files used while writing.
 * There are 4 of these, described below.
 */
typedef struct pdf_temp_file_s {
    char file_name[gp_file_name_sizeof];
    FILE *file;
    stream *strm;
    byte *strm_buf;
    stream *save_strm;		/* save pdev->strm while writing here */
} pdf_temp_file_t;

/* Define the device structure. */
#ifndef gx_device_pdf_DEFINED
#  define gx_device_pdf_DEFINED
typedef struct gx_device_pdf_s gx_device_pdf_t;
#endif
struct gx_device_pdf_s {
    gx_device_psdf_common;
    /* PDF-specific distiller parameters */
    float CompatibilityLevel;
    /* End of distiller parameters */
    /* Other parameters */
    bool ReAssignCharacters;
    bool ReEncodeCharacters;
    long FirstObjectNumber;
    /* End of parameters */
    /* Following are set when device is opened. */
    enum {
	pdf_compress_none,
	pdf_compress_LZW,	/* not currently used, thanks to Unisys */
	pdf_compress_Flate
    } compression;
#define pdf_memory v_memory
    /*
     * The xref temporary file is logically an array of longs.
     * xref[id - FirstObjectNumber] is the position in the output file
     * of the object with the given id.
     *
     * Note that xref, unlike the other temporary files, does not have
     * an associated stream or stream buffer.
     */
    pdf_temp_file_t xref;
    /*
     * asides holds resources and other "aside" objects.  It is
     * copied verbatim to the output file at the end of the document.
     */
    pdf_temp_file_t asides;
    /*
     * streams holds data for stream-type Cos objects.  The data is
     * copied to the output file at the end of the document.
     *
     * Note that streams.save_strm is not used, since we don't interrupt
     * normal output when saving stream data.
     */
    pdf_temp_file_t streams;
    /*
     * pictures holds graphic objects being accumulated between BP and EP.
     * The object is moved to streams when the EP is reached: since BP and
     * EP nest, we delete the object from the pictures file at that time.
     */
    pdf_temp_file_t pictures;
    pdf_font_t *open_font;
    long embedded_encoding_id;
    /* ................ */
    long next_id;
    /* The following 3 objects, and only these, are allocated */
    /* when the file is opened. */
    cos_dict_t *Catalog;
    cos_dict_t *Info;
    cos_dict_t *Pages;
#define pdf_num_initial_ids 3
    long outlines_id;
    int next_page;
    long contents_id;
    pdf_context_t context;
    long contents_length_id;
    long contents_pos;
    pdf_procset procsets;	/* used on this page */
    float flatness;
/****** SHOULD USE state ******/
    /* The line width, dash offset, and dash pattern */
    /* are in default user space units. */
    gx_line_params line_params;	/* current values */
/****** SHOULD USE state ******/
    pdf_text_state_t text;
    long space_char_ids[X_SPACE_MAX - X_SPACE_MIN + 1];
#define initial_num_pages 50
    pdf_page_t *pages;
    int num_pages;
    pdf_resource_list_t resources[NUM_RESOURCE_TYPES];
    pdf_resource_t *cs_Pattern;
    pdf_resource_t *last_resource;
    pdf_outline_level_t outline_levels[MAX_OUTLINE_DEPTH];
    int outline_depth;
    int closed_outline_depth;
    int outlines_open;
    pdf_article_t *articles;
    cos_dict_t *Dests;
    cos_dict_t *named_objects;
    pdf_graphics_save_t *open_graphics;
};

#define is_in_page(pdev)\
  ((pdev)->contents_id != 0)
#define is_in_document(pdev)\
  (is_in_page(pdev) || (pdev)->last_resource != 0)

/* Enumerate the individual pointers in a gx_device_pdf. */
#define gx_device_pdf_do_ptrs(m)\
 m(0,asides.strm) m(1,asides.strm_buf) m(2,asides.save_strm)\
 m(3,streams.strm) m(4,streams.strm_buf)\
 m(5,pictures.strm) m(6,pictures.strm_buf) m(7,pictures.save_strm)\
 m(8,open_font)\
 m(9,Catalog) m(10,Info) m(11,Pages)\
 m(12,line_params.dash.pattern) m(13,text.font) m(14,pages)\
 m(15,cs_Pattern) m(16,last_resource)\
 m(17,articles) m(18,Dests) m(19,named_objects) m(20,open_graphics)
#define gx_device_pdf_num_ptrs 21
#define gx_device_pdf_do_strings(m) /* do nothing */
#define gx_device_pdf_num_strings 0
#define st_device_pdf_max_ptrs\
  (st_device_psdf_max_ptrs + gx_device_pdf_num_ptrs +\
   gx_device_pdf_num_strings + NUM_RESOURCE_TYPES * NUM_RESOURCE_CHAINS +\
   MAX_OUTLINE_DEPTH * 2)

#define private_st_device_pdfwrite()	/* in gdevpdf.c */\
  gs_private_st_composite_final(st_device_pdfwrite, gx_device_pdf,\
    "gx_device_pdf", device_pdfwrite_enum_ptrs, device_pdfwrite_reloc_ptrs,\
    device_pdfwrite_finalize)

/* ================ Driver procedures ================ */

    /* In gdevpdfd.c */
dev_proc_fill_rectangle(gdev_pdf_fill_rectangle);
dev_proc_fill_path(gdev_pdf_fill_path);
dev_proc_stroke_path(gdev_pdf_stroke_path);
    /* In gdevpdfi.c */
dev_proc_copy_mono(gdev_pdf_copy_mono);
dev_proc_copy_color(gdev_pdf_copy_color);
dev_proc_fill_mask(gdev_pdf_fill_mask);
dev_proc_begin_image(gdev_pdf_begin_image);
dev_proc_strip_tile_rectangle(gdev_pdf_strip_tile_rectangle);
    /* In gdevpdfp.c */
dev_proc_get_params(gdev_pdf_get_params);
dev_proc_put_params(gdev_pdf_put_params);
    /* In gdevpdft.c */
dev_proc_text_begin(gdev_pdf_text_begin);

/* ================ Utility procedures ================ */

/* ---------------- Exported by gdevpdf.c ---------------- */

/* Initialize the IDs allocated at startup. */
void pdf_initialize_ids(P1(gx_device_pdf * pdev));

/* ---------------- Exported by gdevpdfu.c ---------------- */

/* ------ Document ------ */

/* Open the document if necessary. */
void pdf_open_document(P1(gx_device_pdf * pdev));

/* ------ Objects ------ */

/* Allocate an ID for a future object. */
long pdf_obj_ref(P1(gx_device_pdf * pdev));

/* Read the current position in the output stream. */
long pdf_stell(P1(gx_device_pdf * pdev));

/* Begin an object, optionally allocating an ID. */
long pdf_open_obj(P2(gx_device_pdf * pdev, long id));
long pdf_begin_obj(P1(gx_device_pdf * pdev));

/* End an object. */
int pdf_end_obj(P1(gx_device_pdf * pdev));

/* ------ Graphics ------ */

/* Reset the graphics state parameters to initial values. */
void pdf_reset_graphics(P1(gx_device_pdf * pdev));

/* Set the fill or stroke color. */
int pdf_set_color(P4(gx_device_pdf * pdev, gx_color_index color,
		     gx_drawing_color * pdcolor, const char *rgs));

/* Write matrix values. */
void pdf_put_matrix(P4(gx_device_pdf * pdev, const char *before,
		       const gs_matrix * pmat, const char *after));

/* Write a name, with escapes for unusual characters. */
void pdf_put_name(P3(const gx_device_pdf * pdev, const byte * nstr, uint size));

/* Write a string in its shortest form ( () or <> ). */
void pdf_put_string(P3(const gx_device_pdf * pdev, const byte * str, uint size));

/* Write a value, treating names specially. */
void pdf_write_value(P3(const gx_device_pdf * pdev, const byte * vstr, uint size));

/* ------ Page contents ------ */

/* Open a page contents part. */
/* Return an error if the page has too many contents parts. */
int pdf_open_contents(P2(gx_device_pdf * pdev, pdf_context_t context));

/* Close the current contents part if we are in one. */
int pdf_close_contents(P2(gx_device_pdf * pdev, bool last));

/* ------ Resources et al ------ */

/*
 * Define the offset that indicates that a file position is in the
 * asides file rather than the main (contents) file.
 * Must be a power of 2, and larger than the largest possible output file.
 */
#define ASIDES_BASE_POSITION min_long

/* Begin an object logically separate from the contents. */
/* (I.e., an object in the resource file.) */
long pdf_open_separate(P2(gx_device_pdf * pdev, long id));
long pdf_begin_separate(P1(gx_device_pdf * pdev));

/* Begin an aside (resource, annotation, ...). */
int pdf_begin_aside(P4(gx_device_pdf * pdev, pdf_resource_t **plist,
		       const gs_memory_struct_type_t * pst,
		       pdf_resource_t **ppres));

/* Begin a resource of a given type. */
int pdf_begin_resource(P4(gx_device_pdf * pdev, pdf_resource_type_t rtype,
			  gs_id rid, pdf_resource_t **ppres));

/* Begin a resource body of a given type. */
int pdf_begin_resource_body(P4(gx_device_pdf * pdev, pdf_resource_type_t rtype,
			       gs_id rid, pdf_resource_t **ppres));

/* Allocate a resource, but don't open the stream. */
int pdf_alloc_resource(P5(gx_device_pdf * pdev, pdf_resource_type_t rtype,
			  gs_id rid, pdf_resource_t **ppres, long id));

/* Find a resource of a given type by gs_id. */
pdf_resource_t *pdf_find_resource_by_gs_id(P3(gx_device_pdf * pdev,
					      pdf_resource_type_t rtype,
					      gs_id rid));

/* End a separate object. */
int pdf_end_separate(P1(gx_device_pdf * pdev));

/* End an aside. */
int pdf_end_aside(P1(gx_device_pdf * pdev));

/* End a resource. */
int pdf_end_resource(P1(gx_device_pdf * pdev));

/* Copy data from a temporary file to a stream. */
void pdf_copy_data(P3(stream *s, FILE *file, long count));

/* ------ Pages ------ */

/* Get or assign the ID for a page. */
/* Returns 0 if the page number is out of range. */
long pdf_page_id(P2(gx_device_pdf * pdev, int page_num));

/* Get the dictionary object for the current page. */
cos_dict_t *pdf_current_page_dict(P1(gx_device_pdf *pdev));

/* Open a page for writing. */
int pdf_open_page(P2(gx_device_pdf * pdev, pdf_context_t context));

/* Write saved page- or document-level information. */
int pdf_write_saved_string(P2(gx_device_pdf * pdev, gs_string * pstr));

/* ------ Path drawing ------ */

/* Test whether the clip path needs updating. */
bool pdf_must_put_clip_path(P2(gx_device_pdf * pdev, const gx_clip_path * pcpath));

/* Write and update the clip path. */
int pdf_put_clip_path(P2(gx_device_pdf * pdev, const gx_clip_path * pcpath));

/* ---------------- Exported by gdevpdfm.c ---------------- */

/*
 * Define the type for a pdfmark-processing procedure.
 * If nameable is false, the objname argument is always NULL.
 */
#define pdfmark_proc(proc)\
  int proc(P5(gx_device_pdf *pdev, gs_param_string *pairs, uint count,\
	      const gs_matrix *pctm, const gs_param_string *objname))

/* Compare a C string and a gs_param_string. */
bool pdf_key_eq(P2(const gs_param_string * pcs, const char *str));

/* Scan an integer out of a parameter string. */
int pdfmark_scan_int(P2(const gs_param_string * pstr, int *pvalue));

/* Process a pdfmark (called from pdf_put_params). */
int pdfmark_process(P2(gx_device_pdf * pdev, const gs_param_string_array * pma));

/* Close the current level of the outline tree. */
int pdfmark_close_outline(P1(gx_device_pdf * pdev));

/* Finish writing an article. */
int pdfmark_write_article(P2(gx_device_pdf * pdev, const pdf_article_t * part));

/* ---------------- Exported by gdevpdfr.c ---------------- */

/* Test whether an object name has valid syntax, {name}. */
bool pdf_objname_is_valid(P2(const byte *data, uint size));

/*
 * Look up a named object.  Return e_rangecheck if the syntax is invalid,
 * e_undefined if no object by that name exists.
 */
int pdf_find_named(P3(gx_device_pdf * pdev, const gs_param_string * pname,
		      cos_object_t **ppco));

/*
 * Create a named object.  id = -1L means do not assign an id.  pname = 0
 * means just create the object, do not name it.
 */
int pdf_create_named(P5(gx_device_pdf *pdev, const gs_param_string *pname,
			cos_type_t cotype, cos_object_t **ppco, long id));
int pdf_create_named_dict(P4(gx_device_pdf *pdev, const gs_param_string *pname,
			     cos_dict_t **ppcd, long id));

/*
 * Look up a named object as for pdf_find_named.  If the object does not
 * exist, create it (as a dictionary if it is one of the predefined names
 * {ThisPage}, {NextPage}, {PrevPage}, or {Page<#>}, otherwise as a
 * generic object) and return 1.
 */
int pdf_refer_named(P3(gx_device_pdf *pdev, const gs_param_string *pname,
		       cos_object_t **ppco));

/*
 * Look up a named object as for pdf_refer_named.  If the object already
 * exists and is not simply a forward reference, return e_rangecheck;
 * if it exists as a forward reference, set its type and return 0;
 * otherwise, create the object with the given type and return 1.
 * pname = 0 is allowed: in this case, simply create the object.
 */
int pdf_make_named(P5(gx_device_pdf * pdev, const gs_param_string * pname,
		      cos_type_t cotype, cos_object_t **ppco, bool assign_id));
int pdf_make_named_dict(P4(gx_device_pdf * pdev, const gs_param_string * pname,
			   cos_dict_t **ppcd, bool assign_id));

/*
 * Look up a named object as for pdf_refer_named.  If the object does not
 * exist, or is a forward reference, return e_undefined; if the object
 * exists has the wrong type, return e_typecheck.
 */
int pdf_get_named(P4(gx_device_pdf * pdev, const gs_param_string * pname,
		     cos_type_t cotype, cos_object_t **ppco));

/*
 * Scan a string for a token.  <<, >>, [, and ] are treated as tokens.
 * Return 1 if a token was scanned, 0 if we reached the end of the string,
 * or an error.  On a successful return, the token extends from *ptoken up
 * to but not including *pscan.
 */
int pdf_scan_token(P3(const byte **pscan, const byte * end,
		      const byte **ptoken));

/*
 * Scan a possibly composite token: arrays and dictionaries are treated as
 * single tokens.
 */
int pdf_scan_token_composite(P3(const byte **pscan, const byte * end,
				const byte **ptoken));

/* Replace object names with object references in a (parameter) string. */
int pdf_replace_names(P3(gx_device_pdf *pdev, const gs_param_string *from,
			 gs_param_string *to));

/* ---------------- Exported by gdevpdft.c ---------------- */

/* Begin a CharProc for an embedded (bitmap) font. */
int pdf_begin_char_proc(P8(gx_device_pdf * pdev, int w, int h, int x_width,
			   int y_offset, gs_id id, pdf_char_proc_t **ppcp,
			   pdf_stream_position_t * ppos));

/* End a CharProc. */
int pdf_end_char_proc(P2(gx_device_pdf * pdev, pdf_stream_position_t * ppos));

/* Put out a reference to an image as a character in an embedded font. */
int pdf_do_char_image(P3(gx_device_pdf * pdev, const pdf_char_proc_t * pcp,
			 const gs_matrix * pimat));

/* ---------------- Exported by gdevpdff.c ---------------- */

typedef enum {
    FONT_EMBED_BASE14,
    FONT_EMBED_NO,
    FONT_EMBED_YES
} pdf_font_embed_t;

typedef struct pdf_standard_font_s {
    const char *fname;
    gs_encoding_index_t base_encoding;
} pdf_standard_font_t;
extern const pdf_standard_font_t pdf_standard_fonts[];

/* Find an original font corresponding to an arbitrary font, if any. */
bool pdf_find_orig_font(P4(gx_device_pdf *pdev, gs_font *font,
			   gs_const_string *pfname, gs_matrix *pfmat));

/*
 * Determine the embedding status of a font.  If the font is in the base
 * 14, store its index (0..13) in *pindex.
 */
pdf_font_embed_t pdf_font_embed_status(P3(gx_device_pdf *pdev, gs_font *font,
					  int *pindex));

/* Allocate a font resource. */
int pdf_alloc_font(P4(gx_device_pdf *pdev, gs_id rid, pdf_font_t **ppfres,
		      bool with_descriptor));

/* Add an encoding difference to a font. */
int pdf_add_encoding_difference(P5(gx_device_pdf *pdev, pdf_font_t *ppf, int chr,
				   const gs_font_base *bfont, gs_glyph glyph));

/* Get the width of a given character in a (base) font. */
int pdf_char_width(P5(pdf_font_t *ppf, int ch, gs_font *font,
		      const gs_matrix *pmat, int *pwidth /* may be NULL */));

/* Compute the FontDescriptor for a font or a font subset. */
int pdf_compute_font_descriptor(P4(gx_device_pdf *pdev,
				   pdf_font_descriptor_t *pfd, gs_font *font,
				   const byte *used /*[32]*/));

/* Register a font for eventual writing (embedded or not). */
int pdf_register_font(P3(gx_device_pdf *pdev, gs_font *font, pdf_font_t *ppf));

/* Write out the font resources when wrapping up the output. */
int pdf_write_font_resources(P1(gx_device_pdf *pdev));

#endif /* gdevpdfx_INCLUDED */
