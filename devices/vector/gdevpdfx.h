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
#include "gxdevmem.h"
#include "sarc4.h"
#include "gsfunc.h"

#define FINE_GLYPH_USAGE 1 /* Old code = 0, new code = 1 */

/* ---------------- Acrobat limitations ---------------- */

/*
 * The PDF reference manual, 2nd ed., claims that the limit for coordinates
 * is +/- 32767. However, testing indicates that Acrobat Reader 4 for
 * Windows and Linux fail with coordinates outside +/- 16383. Hence, we
 * limit coordinates to 16k, with a little slop.
 */
/* 28/05/2020 This was only being applied to text and a subset of paths. Since
 * Acrobat 4 is now more than 20 years old, lets just drop support for it. The
 * PDF specification never had this limit, just Adobe's software.
 */
/* 31/05/2021 However, it transpires that the PDF/A-1 specification adopted the
 * implementation limits of Acrobat 5 as part of the specification. So we need
 * to handle that, but we don't need to limit ourselves to the more restrictive
 * Acrobat 4 limit of 16384 ( MAX_USER_COORD 16300), we can use 32767. THis is only
 * used when creating PDF/A-1 files.
 */
#define MAX_USER_COORD 32700

/* ---------------- Statically allocated sizes ---------------- */
/* These should really be dynamic.... */

/* Define the maximum depth of an outline tree. */
/* Note that there is no limit on the breadth of the tree. */
#define INITIAL_MAX_OUTLINE_DEPTH 32

/* Define the maximum size of a destination array string. */
#define MAX_DEST_STRING 80

/* Define the maximum number of objects stored in an ObjStm */
#define MAX_OBJSTM_OBJECTS 200

/* ================ Types and structures ================ */

typedef struct pdf_base_font_s pdf_base_font_t;

/* Define the possible contexts for the output stream. */
typedef enum {
    PDF_IN_NONE,
    PDF_IN_STREAM,
    PDF_IN_TEXT,
    PDF_IN_STRING
} pdf_context_t;

/* ---------------- Cos objects ---------------- */

/*
 * These are abstract types for cos objects.  The concrete types are in
 * gdevpdfo.h.
 */
typedef struct cos_object_s cos_object_t;
typedef struct cos_stream_s cos_stream_t;
typedef struct cos_dict_s cos_dict_t;
typedef struct cos_array_s cos_array_t;
typedef struct cos_value_s cos_value_t;
typedef struct cos_object_procs_s cos_object_procs_t;
typedef const cos_object_procs_t *cos_type_t;
#define cos_types_DEFINED

typedef struct pdf_text_state_s pdf_text_state_t;

typedef struct pdf_char_glyph_pairs_s pdf_char_glyph_pairs_t;

/* ---------------- Resources ---------------- */

typedef enum {
    /*
     * Standard PDF resources.  Font must be last, because resources
     * up to but not including Font are written page-by-page.
     */
    resourceColorSpace,
    resourceExtGState,
    resourcePattern,
    resourceShading,
    resourceXObject,
    resourceProperties,
    resourceOther, /* Anything else that needs to be stored for a time.
                    * Can be any of the types defined below NUM_RESOURCE_TYPES
                    * but this is the type used to identify the object.
                    */
    resourceFont,
    /*
     * Internally used (pseudo-)resources.
     */
    resourceCharProc,
    resourceCIDFont,
    resourceCMap,
    resourceFontDescriptor,
    resourceGroup,
    resourceSoftMaskDict,
    resourceFunction,
    resourcePage,
    NUM_RESOURCE_TYPES,
    /* These resource types were 'resourceOther', but we want to track them
     * for ps2write. They are not stored in the pdf device structure, unlike
     * the reource types above.
     */
    resourceEncoding,
    resourceCIDSystemInfo,
    resourceHalftone,
    resourceLength,
    resourceStream,
    resourceOutline,
    resourceArticle,
    resourceDests,
    resourceLabels,
    resourceThread,
    resourceCatalog,
    resourceEncrypt,
    resourcePagesTree,
    resourceMetadata,
    resourceICC,
    resourceAnnotation,
    resourceEmbeddedFiles,
    resourceFontFile,
    resourceNone        /* Special, used when this isn't a resource at all
                         * eg when we execute a resource we've just written, such as
                         * a Pattern.
                         */
} pdf_resource_type_t;

#define PDF_RESOURCE_TYPE_NAMES\
  "/ColorSpace", "/ExtGState", "/Pattern", "/Shading", "/XObject", "/Properties", 0, "/Font",\
  0, "/Font", "/CMap", "/FontDescriptor", "/Group", "/Mask", 0, 0, 0, 0, 0,\
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
#define PDF_RESOURCE_TYPE_STRUCTS\
  &st_pdf_color_space,                /* gdevpdfg.h / gdevpdfc.c */\
  &st_pdf_resource,                /* see below */\
  &st_pdf_pattern,\
  &st_pdf_resource,\
  &st_pdf_x_object,                /* see below */\
  &st_pdf_resource,\
  &st_pdf_resource,\
  &st_pdf_font_resource,        /* gdevpdff.h / gdevpdff.c */\
  &st_pdf_char_proc,                /* gdevpdff.h / gdevpdff.c */\
  &st_pdf_font_resource,        /* gdevpdff.h / gdevpdff.c */\
  &st_pdf_resource,\
  &st_pdf_font_descriptor,        /* gdevpdff.h / gdevpdff.c */\
  &st_pdf_resource,\
  &st_pdf_resource,\
  &st_pdf_resource,\
  &st_pdf_resource

/*
 * rname is currently R<id> for all resources other than synthesized fonts;
 * for synthesized fonts, rname is A, B, ....  The string is null-terminated.
 */

    /* WARNING WILL ROBINSON!
     * these 2 pointers may *look* like a double linked list but in fact these are pointers
     * to two *different* single-linked lists. 'next' points to the next resource
     * of this type, which is stored in the resource chains pointed to by the array
     * 'resources' in the device structure. 'prev' is a pointer
     * to the list of stored resources of all types, pointed to by the
     * 'last_resource' member of the device structure.
     */
#define pdf_resource_common(typ)\
    typ *next;                        /* next resource of this type */\
    pdf_resource_t *prev;        /* previously allocated resource */\
    gs_id rid;                        /* optional ID key */\
    bool named;\
    bool global;                /* ps2write only */\
    char rname[1/*R*/ + (sizeof(int64_t) * 8 / 3 + 1) + 1/*\0*/];\
    uint64_t where_used;                /* 1 bit per level of content stream */\
    cos_object_t *object
typedef struct pdf_resource_s pdf_resource_t;
struct pdf_resource_s {
    pdf_resource_common(pdf_resource_t);
};

/* The descriptor is public for subclassing. */
extern_st(st_pdf_resource);
#define public_st_pdf_resource()  /* in gdevpdfu.c */\
  gs_public_st_ptrs3(st_pdf_resource, pdf_resource_t, "pdf_resource_t",\
    pdf_resource_enum_ptrs, pdf_resource_reloc_ptrs, next, prev, object)

/*
 * We define XObject resources here because they are used for Image,
 * Form, and PS XObjects, which are implemented in different files.
 */
typedef struct pdf_x_object_s pdf_x_object_t;
struct pdf_x_object_s {
    pdf_resource_common(pdf_x_object_t);
    int width, height;                /* specified width and height for images */
    int data_height;                /* actual data height for images */
};
#define private_st_pdf_x_object()  /* in gdevpdfu.c */\
  gs_private_st_suffix_add0(st_pdf_x_object, pdf_x_object_t,\
    "pdf_x_object_t", pdf_x_object_enum_ptrs, pdf_x_object_reloc_ptrs,\
    st_pdf_resource)

/* Define the mask for which procsets have been used on a page. */
typedef enum {
    NoMarks = 0,
    ImageB = 1,
    ImageC = 2,
    ImageI = 4,
    Text = 8
} pdf_procset_t;

/* ------ Fonts ------ */

/* Define abstract types. */
typedef struct pdf_char_proc_s pdf_char_proc_t;        /* gdevpdff.h */
typedef struct pdf_char_proc_ownership_s pdf_char_proc_ownership_t;        /* gdevpdff.h */
typedef struct pdf_font_s pdf_font_t;  /* gdevpdff.h */
typedef struct pdf_text_data_s pdf_text_data_t;  /* gdevpdft.h */

/* ---------------- Other auxiliary structures ---------------- */

/* Outline nodes and levels */
typedef struct pdf_outline_node_s {
    int64_t id, parent_id, prev_id, first_id, last_id;
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
    int64_t id, article_id, prev_id, next_id, page_id;
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

/* Resource lists */
/* We seem to split the resources up into 'NUM_RESOURCE_CHAINS' linked
 * lists purely as a performance boost. Presumably it save searching very
 * long lists.
 */
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
    int64_t length_id;
    gs_offset_t start_pos;
} pdf_stream_position_t;

/*
 * Define the structure for keeping track of text rotation.
 * There is one for the current page (for AutoRotate /PageByPage)
 * and one for the whole document (for AutoRotate /All).
 */
typedef struct pdf_text_rotation_s {
    int64_t counts[5];                /* 0, 90, 180, 270, other */
    int Rotate;                        /* computed rotation, -1 means none */
} pdf_text_rotation_t;
#define pdf_text_rotation_angle_values 0, 90, 180, 270, -1

/*
 * Define document and page information derived from DSC comments.
 */
typedef struct pdf_page_dsc_info_s {
    int orientation;                /* -1 .. 3 */
    int viewing_orientation;        /* -1 .. 3 */
    gs_rect bounding_box;
} pdf_page_dsc_info_t;

/*
 * Define the stored information for a page.  Because pdfmarks may add
 * information to any page anywhere in the document, we have to wait
 * until the end to write the page dictionaries.
 */
typedef struct pdf_page_s {
    cos_dict_t *Page;
    gs_point MediaBox;
    pdf_procset_t procsets;
    int64_t contents_id;
    int64_t resource_ids[resourceFont + 1]; /* resources thru Font, see above */
    int64_t group_id;
    cos_array_t *Annots;
    pdf_text_rotation_t text_rotation;
    pdf_page_dsc_info_t dsc_info;
    bool NumCopies_set; /* ps2write only. */
    int NumCopies;      /* ps2write only. */
    float UserUnit;     /* pdfwrite only */
} pdf_page_t;
#define private_st_pdf_page()        /* in gdevpdf.c */\
  gs_private_st_ptrs2(st_pdf_page, pdf_page_t, "pdf_page_t",\
    pdf_page_enum_ptrs, pdf_page_reloc_ptrs, Page, Annots)

/*
 * Define the structure for the temporary files used while writing.
 * There are 4 of these, described below.
 */
typedef struct pdf_temp_file_s {
    char file_name[gp_file_name_sizeof];
    gp_file *file;
    stream *strm;
    byte *strm_buf;
    stream *save_strm;                /* save pdev->strm while writing here */
} pdf_temp_file_t;

typedef struct gx_device_pdf_s gx_device_pdf;

/* Structures and definitions for linearisation */
typedef struct linearisation_record_s {
    int PageUsage;
    int NumPagesUsing;
    int *PageList;
    uint NewObjectNumber;
    gs_offset_t OriginalOffset;
    gs_offset_t LinearisedOffset;
    gs_offset_t Length;
} pdf_linearisation_record_t;

#define private_st_pdf_linearisation_record()\
  gs_private_st_ptrs1(st_pdf_linearisation_record, pdf_linearisation_record_t,\
    "pdf_linearisation_record_t", pdf_linearisation_record_enum_ptrs, pdf_linearisation_record_reloc_ptrs,\
    PageList)

typedef struct page_hint_stream_header_s {
    unsigned int LeastObjectsPerPage;    /* Including the page object */
    /* Item 2 is already stored elsewhere */
    unsigned int MostObjectsPerPage;
    unsigned short ObjectNumBits;
    unsigned int LeastPageLength;        /* From beginning of page object to end of last object used by page */
    unsigned int MostPageLength;
    unsigned short PageLengthNumBits;
    unsigned int LeastPageOffset;
    unsigned int MostPageOffset;
    unsigned short PageOffsetNumBits;
    unsigned int LeastContentLength;
    unsigned int MostContentLength;
    unsigned short ContentLengthNumBits;
    unsigned int MostSharedObjects;
    unsigned int LargestSharedObject;
    unsigned short SharedObjectNumBits;
} page_hint_stream_header_t;

typedef struct page_hint_stream_s {
    unsigned int NumUniqueObjects; /* biased by the least number of objects on any page */
    unsigned int PageLength;       /* biased by the least page length*/
    unsigned int NumSharedObjects;
    unsigned int *SharedObjectRef; /* one for each shaed object on the page */
    /* Item 5 we invent */
    gs_offset_t ContentOffset; /* biased by the least offset to the conent stream for any page */
    gs_offset_t ContentLength;/* biased by the least content stream length */
} page_hint_stream_t;

typedef struct shared_hint_stream_header_s {
    unsigned int FirstSharedObject;
    gs_offset_t FirstObjectOffset;
    unsigned int FirstPageEntries;
    unsigned int NumSharedObjects;
    /* Item 5 is always 1 as far as we are concerned */
    unsigned int LeastObjectLength;
    unsigned int MostObjectLength;
    unsigned short LengthNumBits;
} shared_hint_stream_header_t;

typedef struct share_hint_stream_s {
    unsigned int ObjectNumber;
    gs_offset_t ObjectOffset;
    unsigned int ObjectLength;   /* biased by the LeastObjectLength */
    /* item 2 is always 0 */
    /* Which means that item 3 is never present */
    /* Finally item 4 is always 0 (1 less than the number of objects in the group, which is always 1) */
} shared_hint_stream_t;

typedef struct pdf_linearisation_s {
    gp_file *sfile;
    pdf_temp_file_t Lin_File;
    char HintBuffer[256];
    unsigned char HintBits;
    unsigned char HintByte;
    int64_t Catalog_id;
    int64_t Info_id;
    int64_t Pages_id;
    int64_t NumPage1Resources;
    int64_t NumPart1StructureResources;
    int64_t NumSharedResources;
    int64_t NumUniquePageResources;
    int64_t NumPart9Resources;
    int64_t NumNonPageResources;
    int64_t LastResource;
    int64_t MainFileEnd;
    gs_offset_t *Offsets;
    gs_offset_t xref;
    gs_offset_t FirstxrefOffset;
    gs_offset_t FirsttrailerOffset;
    gs_offset_t LDictOffset;
    gs_offset_t FileLength;
    gs_offset_t T;
    gs_offset_t E;
    page_hint_stream_header_t PageHintHeader;
    int NumPageHints;
    page_hint_stream_t *PageHints;
    shared_hint_stream_header_t SharedHintHeader;
    int NumSharedHints;
    shared_hint_stream_t *SharedHints;
} pdf_linearisation_t;

/* These are the values for 'PageUsage' above, values > 0 indicate the page number that uses the resource */
#define resource_usage_not_referenced 0
#define resource_usage_page_shared -1
/* Thses need to be lower than the shared value */
#define resource_usage_part1_structure -2
#define resource_usage_part9_structure -3
#define resource_usage_written -4

/*
 * Define the structure for PDF font cache element.
 */
typedef struct pdf_font_cache_elem_s pdf_font_cache_elem_t;
struct pdf_font_cache_elem_s {
    pdf_font_cache_elem_t *next;
    gs_id font_id;
    int num_chars;                /* safety purpose only */
    int num_widths;                /* safety purpose only */
    struct pdf_font_resource_s *pdfont;
    byte *glyph_usage;
    double *real_widths;        /* [count] (not used for Type 0) */
};

#define private_st_pdf_font_cache_elem()\
    gs_private_st_ptrs4(st_pdf_font_cache_elem, pdf_font_cache_elem_t,\
        "pdf_font_cache_elem_t", pdf_font_cache_elem_enum,\
        pdf_font_cache_elem_reloc, next, pdfont,\
        glyph_usage, real_widths)

/*
 * pdf_viewer_state tracks the graphic state of a viewer,
 * which would interpret the generated PDF file
 * immediately when it is generated.
 */
typedef struct pdf_viewer_state_s {
    int transfer_not_identity;        /* bitmask */
    gs_id transfer_ids[4];
    float strokeconstantalpha;
    float fillconstantalpha;
    bool alphaisshape;
    gs_blend_mode_t blend_mode; /* state.blend_mode */
    gs_id halftone_id;
    gs_id black_generation_id;
    gs_id undercolor_removal_id;
    int overprint_mode;
    float smoothness; /* state.smoothness */
    float flatness;
    bool text_knockout; /* state.text_knockout */
    bool fill_overprint;
    bool stroke_overprint;
    int stroke_adjust; /* state.stroke_adjust */
    bool fill_used_process_color;
    bool stroke_used_process_color;
    gx_hl_saved_color saved_fill_color;
    gx_hl_saved_color saved_stroke_color;
    gx_line_params line_params;
    float *dash_pattern;
    uint dash_pattern_size;
    gs_id soft_mask_id;
} pdf_viewer_state;

/*
 * Define a structure for saving context when entering
 * a contents stream accumulation mode (charproc, Type 1 pattern).
 */
typedef struct pdf_substream_save_s {
    pdf_context_t        context;
    pdf_text_state_t        *text_state;
    gx_path                *clip_path;
    gs_id                clip_path_id;
    int                        vgstack_bottom;
    stream                *strm;
    cos_dict_t                *substream_Resources;
    pdf_procset_t        procsets;
    bool                skip_colors;
    pdf_resource_t      *font3;
    pdf_resource_t        *accumulating_substream_resource;
    bool                charproc_just_accumulated;
    bool                accumulating_a_global_object;
    pdf_resource_t      *pres_soft_mask_dict;
    gs_const_string                objname;
    int                        last_charpath_op;
} pdf_substream_save;

#define private_st_pdf_substream_save()\
    gs_private_st_strings1_ptrs7(st_pdf_substream_save, pdf_substream_save,\
        "pdf_substream_save", pdf_substream_save_enum,\
        pdf_substream_save_reloc, objname, text_state, clip_path, strm, \
        substream_Resources, font3, accumulating_substream_resource, pres_soft_mask_dict)
#define private_st_pdf_substream_save_element()\
  gs_private_st_element(st_pdf_substream_save_element, pdf_substream_save,\
    "pdf_substream_save[]", pdf_substream_save_elt_enum_ptrs,\
    pdf_substream_save_elt_reloc_ptrs, st_pdf_substream_save)

typedef enum {
    pdf_compress_none,
    pdf_compress_LZW,        /* not currently used, thanks to Unisys */
    pdf_compress_Flate
} pdf_compression_type;

typedef enum {
    OCR_UnInit,
    OCR_Rendering,
    OCR_Rendered,
    OCR_UnicodeAvailable,
    OCR_Failed
} pdf_OCR_stage;

typedef enum {
    UseOCRNever,
    UseOCRAsNeeded,
    UseOCRAlways
} pdf_OCR_usage;

typedef struct ocr_glyph_s{
    byte *data;
    int x;
    int y;
    int width;
    int height;
    int raster;
    void *next;
    gs_char char_code;
    gs_glyph glyph;
    bool is_space;
} ocr_glyph_t;

/* Define the device structure. */
struct gx_device_pdf_s {
    gx_device_psdf_common;
    gs_font_dir *pdf_font_dir;  /* Our own font cache, so that PCL can free its own ones, we set our font copeis to use this */
                                /* see 'pdf_free_pdf_font_cache' in gdevpdf.c for more details. */
    bool is_ps2write;          /* ps2write (true) versus pdfwrite (false); never changed */
    /* PDF-specific distiller parameters */
    double CompatibilityLevel;
    int EndPage;
    int StartPage;
    bool Optimize;
    bool ParseDSCCommentsForDocInfo;
    bool ParseDSCComments;
    bool EmitDSCWarnings;
    bool CreateJobTicket;
    bool PreserveEPSInfo;
    bool AutoPositionEPSFiles;
    bool PreserveCopyPage;
    bool UsePrologue;
    int OffOptimizations;
    /* End of distiller parameters */
    /* PDF/X parameters */
    gs_param_float_array PDFXTrimBoxToMediaBoxOffset;
    gs_param_float_array PDFXBleedBoxToTrimBoxOffset;
    bool PDFXSetBleedBoxToMediaBox;
    /* OCR Parameters */
    char ocr_language[1024];
    int ocr_engine;
    /* Other parameters */
    bool ReAssignCharacters;
    bool ReEncodeCharacters;
    int64_t FirstObjectNumber;
    bool CompressFonts;
    bool CompressStreams;
    bool PrintStatistics;
    gs_param_string DocumentUUID;
    gs_param_string InstanceUUID;
    int DocumentTimeSeq;
    bool ForOPDFRead;          /* PS2WRITE only. */
    bool Eps2Write;            /* EPS2WRITE only */
    bool CompressEntireFile;  /* PS2WRITE only. */
    bool ResourcesBeforeUsage; /* PS2WRITE only. */
    bool HavePDFWidths;        /* PS2WRITE only. */
    bool HaveStrokeColor;      /* PS2WRITE only. */
    bool ProduceDSC;               /* PS2WRITE only. */
    bool HaveTransparency;
    bool PatternImagemask; /* The target viewer|printer handles imagemask
                              with pattern color. */
    bool PDFX;                   /* Generate PDF/X */
    int PDFA;                   /* Generate PDF/A 0 = don't produce, otherwise level of PDF/A */
    bool AbortPDFAX;            /* Abort generation of PDFA or X, produce regular PDF */
    int64_t MaxClipPathSize;  /* The maximal number of elements of a clipping path
                              that the target viewer|printer can handle. */
    int64_t MaxShadingBitmapSize; /* The maximal number of bytes in
                              a bitmap representation of a shading.
                              (Bigger shadings to be downsampled). */
    int64_t MaxInlineImageSize;
    /* Encryption parameters */
    gs_param_string OwnerPassword;
    gs_param_string UserPassword;
    uint KeyLength;
    int32_t Permissions;
    uint EncryptionR;
    gs_param_string NoEncrypt;
    bool EncryptMetadata;
    /* End of parameters */
    bool ComputeDocumentDigest; /* Developer needs only; Always true in production. */
    /* Encryption data */
    byte EncryptionO[32];
    byte EncryptionU[32];
    byte EncryptionKey[16];
    uint EncryptionV;
    /* Values derived from DSC comments */
    bool is_EPS;
    int AccumulatingBBox;
    gs_rect BBox;
    pdf_page_dsc_info_t doc_dsc_info; /* document default */
    pdf_page_dsc_info_t page_dsc_info; /* current page */
    /* Additional graphics state */
    bool fill_overprint, stroke_overprint;
    int rendering_intent;
    bool user_icc;
    bool remap_fill_color, remap_stroke_color;
    gs_id halftone_id;
    gs_id transfer_ids[4];
    int transfer_not_identity;        /* bitmask */
    gs_id black_generation_id, undercolor_removal_id;
    /* Following are set when device is opened. */
    pdf_compression_type compression;
    pdf_compression_type compression_at_page_start;
    /* pdf_memory is 'stable' memory, it is not subject to save and restore
     * and is the allocator which should be used for pretty much ewverything
     */
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
     * ObjStm holds a tream of objects being stored in an ObjStm, up to MAX_OBJSTM_OBJECTS,
     * We keep a record of the offset of each object within the stream in ObjStmOffsets, so
     * that later on we can write the offset within the stream for each object into the
     * beginning of the ObjStm. NumObjStmOffsets is used to keep track of how many we have,
     * and ObjStm_id is the id of the stream which we'll eventually store in the 'asides'
     * file, containing the ObjStm.
     */
    pdf_temp_file_t ObjStm;
    int64_t ObjStm_id;
    gs_offset_t *ObjStmOffsets;
    int NumObjStmObjects;
    bool doubleXref;

    /* ................ */
    int64_t next_id;
    /* The following 3 objects, and only these, are allocated */
    /* when the file is opened. */
    cos_dict_t *Catalog;
    cos_dict_t *Info;
    cos_dict_t *Pages;
#define pdf_num_initial_ids 3
    int64_t outlines_id;
    int next_page;
    int max_referred_page;
    int64_t contents_id;
    pdf_context_t context;
    int64_t contents_length_id;
    gs_offset_t contents_pos;
    pdf_procset_t procsets;        /* used on this page */
    pdf_text_data_t *text;
    pdf_text_rotation_t text_rotation;
#define initial_num_pages 50
    pdf_page_t *pages;
    int num_pages;
    uint64_t used_mask;                /* for where_used: page level = 1 */
    pdf_resource_list_t resources[NUM_RESOURCE_TYPES];
    /* cs_Patterns[0] is colored; 1,3,4 are uncolored + Gray,RGB,CMYK */
    pdf_resource_t *cs_Patterns[5];
    pdf_resource_t *Identity_ToUnicode_CMaps[2]; /* WMode = 0,1 */
    pdf_resource_t *last_resource;
    pdf_resource_t *OneByteIdentityH;
    gs_id IdentityCIDSystemInfo_id;
    pdf_outline_level_t *outline_levels;
    int outline_depth;
    int max_outline_depth;
    int closed_outline_depth;
    int outlines_open;
    pdf_article_t *articles;
    cos_dict_t *Dests;
    cos_dict_t *EmbeddedFiles;
    cos_array_t *AF;
    byte fileID[16];
    /* Use a single time moment for all UUIDs to minimize an indeterminizm. */
    /* This needs to be a long because we call the platform code gp_get_realtime
     * to fill it in, and that takes a long *. Be aware that long is 32-bits on
     * 32-bit Windows and 64-bits on 64-bit Windows.
     */
    long uuid_time[2];
    /*
     * global_named_objects holds named objects that are independent of
     * the current namespace: {Catalog}, {DocInfo}, {Page#}, {ThisPage},
     * {PrevPage}, {NextPage}.
     */
    cos_dict_t *global_named_objects;
    /*
     * local_named_objects holds named objects in the current namespace.
     */
    cos_dict_t *local_named_objects;
    /*
     * NI_stack is a last-in, first-out stack in which each element is a
     * (named) cos_stream_t object that eventually becomes the object of an
     * image XObject resource.
     */
    cos_array_t *NI_stack;
    /*
     * Namespace_stack is a last-in, first-out stack in which each pair of
     * elements is, respectively, a saved value of local_named_objects and
     * a saved value of NI_stack.  (The latter is not documented by Adobe,
     * but it was confirmed by them.)
     */
    cos_array_t *Namespace_stack;
    pdf_font_cache_elem_t *font_cache;
    /*
     * char_width is used by pdf_text_set_cache to communicate
     * with assign_char_code around gdev_pdf_fill_mask.
     */
    gs_point char_width;
    /*
     * We need a stable copy of clipping path to prevent writing
     * redundant clipping paths when PS document generates such ones.
     */
    gx_path *clip_path;

    /*
     * Page labels.
     */
    cos_array_t *PageLabels;
    int PageLabels_current_page;
    cos_dict_t *PageLabels_current_label;
    /*
     * The following is a dangerous pointer, which pdf_text_process
     * uses to communicate with assign_char_code.
     * It is a pointer from global memory to local memory.
     * The garbager must not proceess this pointer, and it must
     * not be listed in st_device_pdfwrite.
     * It's life time terminates on garbager invocation.
     */
    gs_text_enum_t *pte;
    /*
     * The viewer's graphic state stack.
     */
    pdf_viewer_state *vgstack;
    int vgstack_size;
    int vgstack_depth;
    int vgstack_bottom;                 /* Stack bottom for the current substream. */
    pdf_viewer_state vg_initial; /* Initial values for viewer's graphic state */
    bool vg_initial_set;

    /* The substream context stack. */
    int sbstack_size;
    int sbstack_depth;
    pdf_substream_save *sbstack;

    /* Temporary workaround. The only way to get forms out of pdfwrite at present
     * is via a transparency group or mask operation. Ordinarily we don't care
     * much about forms, but Patterns within forms need to be scaled to the
     * CTM of the Pattern, not the default page co-ordinate system. We use
     * this value to know if we are nested inside a form or not. If we are
     * we don't undo the resolution co-ordinate transform.
     */
    int FormDepth;

    /* Determine if we have a high level form. We want to do things differently
     * sometimes, if we are capturing a form
     */
    int HighLevelForm;

    /* Nasty hack. OPDFread.ps resets the graphics state to the identity before
     * replaying the Pattern PaintProc, but if the Pattern is nested inside a
     * previous pattern, this doesn't work. We use this to keep track of whether
     * we are nested, and if we are (and are ps2write, not pdfwrite) we track the
     * pattern CTM below.
     */
    int PatternDepth;
    gs_matrix AccumulatedPatternMatrix;

    /* Normally the PDF itnerpreter doesn't call execform to pass a From XObject to
     * pdfwrite, but it *does* create forms (and increment FormDepth) for transparent
     * Groups, which are handled as Form XObjects. Because of the way that Pattterns work,
     * the first pattern after a Form uses the Form co-ordinate system, but if the Pattern
     * itself uses Patterns, then the nested Pattern needs to use the co-ordinate system
     * of the parent Pattern. Unless, of course, we have another Form!
     * So essentially we need to know the Pattern depth, since the last form was executed.
     * If its 0, then we want to apply the current CTM, if its more than that then we want
     * to undo the pdfwrite scaling (see gdevpdfv.c, pdf_store_pattern1_params() at about line
     * 239.
     */
    int PatternsSinceForm;

    /* Accessories */
    cos_dict_t *substream_Resources;     /* Substream resources */
    gs_color_space_index pcm_color_info_index; /* Index of the ProcessColorModel space. */
    bool skip_colors; /* Skip colors while a pattern/charproc accumulation. */
    bool AR4_save_bug; /* See pdf_put_uncolored_pattern */
    pdf_resource_t *font3; /* The owner of the accumulated charstring. */
    pdf_resource_t *accumulating_substream_resource;
    gs_matrix_fixed charproc_ctm;
    bool accumulating_charproc;
    gs_rect charproc_BBox;
    bool charproc_just_accumulated; /* A flag for controlling
                        the glyph variation recognition.
                        Used only with uncached charprocs. */
    bool PS_accumulator; /* A flag to determine whether a given
                         accumulator is for a PostScript type 3 font or not. */
    bool Scaled_accumulator; /* We scale teh CTM when accumulating type 3 fonts */
    bool accumulating_a_global_object; /* ps2write only.
                        Accumulating a global object (such as a named Form,
                        so that resources used in it must also be global.
                        Important for viewers with small memory,
                        which drops resources per page. */
    const pdf_char_glyph_pairs_t *cgp; /* A temporary pointer
                        for pdf_is_same_charproc1.
                        Must be NULL when the garbager is invoked,
                        because it points from global to local memory. */
    int substituted_pattern_count;
    int substituted_pattern_drop_page;
    /* Temporary data for use_image_as_pattern,
       They pass an information about a mask of a masked image,
       (which is being converted into a pattern)
       between 2 consecutive calls to pdf_image_end_image_data. */
    gs_id     image_mask_id;
    bool      image_mask_is_SMask;
    bool      image_mask_skip; /* A flag for pdf_begin_transparency_mask */
    bool      smask_construction; /* True when pdfwrite is constructing a soft mask */
    uint      image_with_SMask; /* A flag for pdf_begin_transparency_group. In order to
                                 * deal with nested groups we set/test the bit according
                                 * to the FormDepth
                                 */
    gs_matrix converting_image_matrix;
    double    image_mask_scale;
    /* Temporary data for soft mask form. */
    pdf_resource_t *pres_soft_mask_dict;
    /* used to track duplicate stream definitions in pdfmarks */
    bool pdfmark_dup_stream;
    /* Temporary data for pdfmark_BP. */
    gs_const_string objname;
    int OPDFRead_procset_length;      /* PS2WRITE only. */
    void *find_resource_param; /* WARNING : not visible for garbager. */
    int last_charpath_op; /* true or false state of last charpath */
    bool type3charpath;
    bool SetPageSize;
    bool RotatePages;
    bool FitPages;
    bool CenterPages;
    bool DoNumCopies;
    bool PreserveSeparation;
    bool PreserveDeviceN;
    int PDFACompatibilityPolicy;
    bool DetectDuplicateImages;
    bool AllowIncrementalCFF;
    bool WantsToUnicode;
    bool PdfmarkCapable;
    bool WantsPageLabels;
    bool WantsOptionalContent;
    bool AllowPSRepeatFunctions;
    bool IsDistiller;
    bool PreserveSMask;
    bool PreserveTrMode;
    bool NoT3CCITT;                 /* A bug in Brother printers causes CCITTFaxDecode
                                     * to fail, especially with small amounts of data.
                                     * This parameter is present only to allow
                                     * ps2write output to work on those pritners.
                                     */
    bool Linearise;                 /* Whether to Linearizse the file, the next 2 parameter
                                     * are only used if this is true.
                                     */
    pdf_linearisation_record_t
        *ResourceUsage;             /* An array, one per resource defined to date, which
                                     * contains either -2 (shared on multiple pages), -1
                                     * (structure object, eg catalog), 0 (not used on a page
                                     * or the page number. This does limit us to a mere 2^31
                                     * pages
                                     */
    int ResourceUsageSize;          /* Size of the above array, currently */
    bool InOutputPage;              /* Used when closing the file, if this is true then we were
                                     * called from output_page and should emit a page even if there
                                     * are no marks. If false, then we probably were called from
                                     * closedevice and, if there are no marks, we should delete
                                     * the last file *if* we are emitting one file per page.
                                     */
    bool FlattenFonts;
    int LastFormID;
    char *ExtensionMetadata;        /* If present the non-standard pdfmark Extension_Metadata has been
                                     * used, the content of ths string is written out as part of the
                                     * metadata referenced by the Catalog.
                                     */
    char *PDFFormName;              /* If present, we are processing (or about to process) a Form XObject
                                     * which we wish to handle as a Form XObject, not flatten. Currently
                                     * this is only the case for Annotation Appearances. This must be NULL
                                     * after the form is processed. The name will be used to create a
                                     * local named object which pdfmark can reference.
                                     */
    stream *PassThroughWriter;      /* A copy of the stream that the image enumerator points to, if we are
                                     * doing JPEG pass through we write the JPEG data here, and don't write
                                     * anything in the image processing routines.
                                     */
    float UserUnit;
    pdf_OCR_usage UseOCR;           /* Never, AsNeeded or Always */
    gs_text_enum_t* OCRSaved;       /* Saved state of the text enumerator before rendering glyph bitmaps for later OCR */
    pdf_OCR_stage OCRStage;         /* Used to control a (sort of) state machine when using OCR to get a Unicode value for a glyph */
    int *OCRUnicode;                /* Used to pass back the Unicode value from the OCR engine to the text processing */
    gs_char OCR_char_code;          /* Passes the current character code from text processing to the image processing code when rendering glyph bitmaps for OCR */
    gs_glyph OCR_glyph;             /* Passes the current glyph code from text processing to the image processing code when rendering glyph bitmaps for OCR */
    ocr_glyph_t *ocr_glyphs;        /* Records bitmaps and other data from text processing when doing OCR */
    gs_gstate **initial_pattern_states;
    bool OmitInfoDate;              /* If true, do not emit CreationDate and ModDate in the Info dictionary and XMP Metadata (must not be true for PDF/X support) */
    bool OmitXMP;                   /* If true, do not emit an XMP /Metadata block and do not reference it from the Catalog (must not be true for PDF/A output) */
    bool OmitID;                    /* If true, do not emit a /ID array in the trailer dicionary (must not be true for encrypted files or PDF 2.0) */
    bool ModifiesPageSize;          /* If true, the new PDF interpreter will not preserve *Box values (the media size has been modified, they will be incorrect) */
    bool ModifiesPageOrder;         /* If true, the new PDF interpreter will not preserve Outlines or Dests, because they will refer to the wrong page number */
    bool WriteXRefStm;              /* If true, (the default) use an XRef stream rather than an xref table */
    bool WriteObjStms;              /* If true, (the default) store candidate objects in ObjStms rather than plain text in the PDF file. */
    int64_t PendingOC;              /* An OptionalContent object is pending */
    bool ToUnicodeForStdEnc;        /* Should we emit ToUnicode CMaps when a simple font has only standard glyph names. Defaults to true */
};

#define is_in_page(pdev)\
  ((pdev)->contents_id != 0)

/* Enumerate the individual pointers in a gx_device_pdf.
   I disliked this macro and so its been removed, the pointers are
   enumerated/relocated in gdevpdf.c now. We still need the gx_device_pdf_num_ptrs
   though, so I'm maintaining this comment just to keep track of the number
   of pointers.
 */
/*#define gx_device_pdf_do_ptrs(m)\
 m(0,asides.strm) m(1,asides.strm_buf) m(2,asides.save_strm)\
 m(3,streams.strm) m(4,streams.strm_buf)\
 m(5,pictures.strm) m(6,pictures.strm_buf) m(7,pictures.save_strm)\
 m(8,Catalog) m(9,Info) m(10,Pages)\
 m(11,text) m(12,pages)\
 m(13,cs_Patterns[0])\
 m(14,cs_Patterns[1]) m(15,cs_Patterns[3]) m(16,cs_Patterns[4])\
 m(17,last_resource)\
 m(18,articles) m(19,Dests) m(20,global_named_objects)\
 m(21, local_named_objects) m(22,NI_stack) m(23,Namespace_stack)\
 m(24,font_cache) m(25,clip_path)\
 m(26,PageLabels) m(27,PageLabels_current_label)\
 m(28,sbstack) m(29,substream_Resources) m(30,font3)\
 m(31,accumulating_substream_resource) \
 m(32,pres_soft_mask_dict) m(33,PDFXTrimBoxToMediaBoxOffset.data)\
 m(34,PDFXBleedBoxToTrimBoxOffset.data)
 m(35,Identity_ToUnicode_CMaps[0]) m(36,Identity_ToUnicode_CMaps[1])\
 m(37,vgstack)\
 m(38, outline_levels)
 m(39, gx_device_pdf, EmbeddedFiles);
 m(40, gx_device_pdf, pdf_font_dir);
 m(41, gx_device_pdf, Extension_Metadata);*/
#define gx_device_pdf_num_ptrs 44
#define gx_device_pdf_do_param_strings(m)\
    m(0, OwnerPassword) m(1, UserPassword) m(2, NoEncrypt)\
    m(3, DocumentUUID) m(4, InstanceUUID)
#define gx_device_pdf_num_param_strings 5
#define gx_device_pdf_do_const_strings(m)\
    m(0, objname)
#define gx_device_pdf_num_const_strings 1

#define private_st_device_pdfwrite()        /* in gdevpdf.c */\
  gs_private_st_composite_final(st_device_pdfwrite, gx_device_pdf,\
    "gx_device_pdf", device_pdfwrite_enum_ptrs, device_pdfwrite_reloc_ptrs,\
    device_pdfwrite_finalize)

/* ================ Driver procedures ================ */

    /* In gdevpdfb.c */
dev_proc_copy_mono(gdev_pdf_copy_mono);
dev_proc_copy_color(gdev_pdf_copy_color);
dev_proc_fill_mask(gdev_pdf_fill_mask);
dev_proc_strip_tile_rectangle(gdev_pdf_strip_tile_rectangle);
    /* In gdevpdfd.c */
extern const gx_device_vector_procs pdf_vector_procs;
dev_proc_fill_rectangle(gdev_pdf_fill_rectangle);
dev_proc_fill_path(gdev_pdf_fill_path);
dev_proc_stroke_path(gdev_pdf_stroke_path);
dev_proc_fill_stroke_path(gdev_pdf_fill_stroke_path);
dev_proc_fillpage(gdev_pdf_fillpage);
    /* In gdevpdfi.c */
dev_proc_begin_typed_image(gdev_pdf_begin_typed_image);
    /* In gdevpdfp.c */
dev_proc_get_params(gdev_pdf_get_params);
dev_proc_put_params(gdev_pdf_put_params);
    /* In gdevpdft.c */
dev_proc_text_begin(gdev_pdf_text_begin);
dev_proc_fill_rectangle_hl_color(gdev_pdf_fill_rectangle_hl_color);
    /* In gdevpdfv.c */
dev_proc_include_color_space(gdev_pdf_include_color_space);
    /* In gdevpdft.c */
dev_proc_composite(gdev_pdf_composite);
dev_proc_begin_transparency_group(gdev_pdf_begin_transparency_group);
dev_proc_end_transparency_group(gdev_pdf_end_transparency_group);
dev_proc_begin_transparency_mask(gdev_pdf_begin_transparency_mask);
dev_proc_end_transparency_mask(gdev_pdf_end_transparency_mask);
dev_proc_dev_spec_op(gdev_pdf_dev_spec_op);

/* ================ Utility procedures ================ */

/* ---------------- Exported by gdevpdf.c ---------------- */

/* Initialize the IDs allocated at startup. */
void pdf_initialize_ids(gx_device_pdf * pdev);

/* Update the color mapping procedures after setting ProcessColorModel. */
void pdf_set_process_color_model(gx_device_pdf * pdev, int index);

/* Reset the text state parameters to initial values. */
void pdf_reset_text(gx_device_pdf *pdev);

/* ---------------- Exported by gdevpdfu.c ---------------- */

/* ------ Document ------ */

/* Utility functions to write args into the file as comments */
int pdfwrite_fwrite_args_comment(gx_device_pdf *pdev, gp_file *f);
int pdfwrite_write_args_comment(gx_device_pdf *pdev, stream *s);

/* Write a DSC compliant header to the file */
int ps2write_dsc_header(gx_device_pdf * pdev, int pages);

/* Open the document if necessary. */
int pdfwrite_pdf_open_document(gx_device_pdf * pdev);

/* ------ Objects ------ */

/* Allocate an ID for a future object, set its pos=0 so we can tell if it is used */
int64_t pdf_obj_forward_ref(gx_device_pdf * pdev);

/* Allocate an ID for a future object. */
int64_t pdf_obj_ref(gx_device_pdf * pdev);

/* Remove an object from the xref table (mark as unused) */
int64_t pdf_obj_mark_unused(gx_device_pdf *pdev, int64_t id);

/* Read the current position in the output stream. */
gs_offset_t pdf_stell(gx_device_pdf * pdev);

/* Begin an object, optionally allocating an ID. */
int64_t pdf_open_obj(gx_device_pdf * pdev, int64_t id, pdf_resource_type_t type);
int64_t pdf_begin_obj(gx_device_pdf * pdev, pdf_resource_type_t type);

/* End an object. */
int pdf_end_obj(gx_device_pdf * pdev, pdf_resource_type_t type);

/* ------ Page contents ------ */

/* Open a page contents part. */
/* Return an error if the page has too many contents parts. */
int pdf_open_contents(gx_device_pdf * pdev, pdf_context_t context);

/* Close the current contents part if we are in one. */
int pdf_close_contents(gx_device_pdf * pdev, bool last);

/* ------ Resources et al ------ */

extern const char *const pdf_resource_type_names[];
extern const gs_memory_struct_type_t *const pdf_resource_type_structs[];

/* Record usage of resoruces by pages */
int pdf_record_usage(gx_device_pdf *const pdev, int64_t resource_id, int page_num);
int pdf_record_usage_by_parent(gx_device_pdf *const pdev, int64_t resource_id, int64_t parent);

/*
 * Define the offset that indicates that a file position is in the
 * asides file rather than the main (contents) file.
 * Must be a power of 2, and larger than the largest possible output file.
 */
#define ASIDES_BASE_POSITION min_int64_t

/* Begin an object logically separate from the contents. */
/* (I.e., an object in the resource file.) */
int64_t pdf_open_separate(gx_device_pdf * pdev, int64_t id, pdf_resource_type_t type);
int64_t pdf_begin_separate(gx_device_pdf * pdev, pdf_resource_type_t type);

/* functions used for ObjStm writing */
int FlushObjStm(gx_device_pdf *pdev);
int NewObjStm(gx_device_pdf *pdev);
int64_t pdf_open_separate_noObjStm(gx_device_pdf * pdev, int64_t id, pdf_resource_type_t type);
int pdf_end_separate_noObjStm(gx_device_pdf * pdev, pdf_resource_type_t type);

/* Reserve object id. */
void pdf_reserve_object_id(gx_device_pdf * pdev, pdf_resource_t *ppres, int64_t id);

/* Begin an aside (resource, annotation, ...). */
int pdf_alloc_aside(gx_device_pdf * pdev, pdf_resource_t ** plist,
                const gs_memory_struct_type_t * pst, pdf_resource_t **ppres,
                int64_t id);
/* Begin an aside (resource, annotation, ...). */
int64_t pdf_begin_aside(gx_device_pdf * pdev, pdf_resource_t **plist,
                    const gs_memory_struct_type_t * pst,
                    pdf_resource_t **ppres, pdf_resource_type_t type);

/* Begin a resource of a given type. */
int pdf_begin_resource(gx_device_pdf * pdev, pdf_resource_type_t rtype,
                       gs_id rid, pdf_resource_t **ppres);

/* Begin a resource body of a given type. */
int pdf_begin_resource_body(gx_device_pdf * pdev, pdf_resource_type_t rtype,
                            gs_id rid, pdf_resource_t **ppres);

/* Allocate a resource, but don't open the stream. */
int pdf_alloc_resource(gx_device_pdf * pdev, pdf_resource_type_t rtype,
                       gs_id rid, pdf_resource_t **ppres, int64_t id);

/* Find same resource. */
int pdf_find_same_resource(gx_device_pdf * pdev,
        pdf_resource_type_t rtype, pdf_resource_t **ppres,
        int (*eq)(gx_device_pdf * pdev, pdf_resource_t *pres0, pdf_resource_t *pres1));

/* Find resource by resource id. */
pdf_resource_t *pdf_find_resource_by_resource_id(gx_device_pdf * pdev,
                                                pdf_resource_type_t rtype, gs_id id);

/* Find a resource of a given type by gs_id. */
pdf_resource_t *pdf_find_resource_by_gs_id(gx_device_pdf * pdev,
                                           pdf_resource_type_t rtype,
                                           gs_id rid);

/* Remove a resource from a chain of resources but do not free it. The resource will
 * have to be listed elsewhere. This is primarily useful for moving existing resources
 * to local named resources.
 */
void
pdf_drop_resource_from_chain(gx_device_pdf * pdev, pdf_resource_t *pres1, pdf_resource_type_t rtype);

void pdf_drop_resources(gx_device_pdf * pdev, pdf_resource_type_t rtype,
        int (*cond)(gx_device_pdf * pdev, pdf_resource_t *pres));

/* Print resource statistics. */
void pdf_print_resource_statistics(gx_device_pdf * pdev);

/* Cancel a resource (do not write it into PDF). */
int pdf_cancel_resource(gx_device_pdf * pdev, pdf_resource_t *pres,
        pdf_resource_type_t rtype);

/* Remove a resource. */
void pdf_forget_resource(gx_device_pdf * pdev, pdf_resource_t *pres1,
        pdf_resource_type_t rtype);

/* Substitute a resource with a same one. */
int pdf_substitute_resource(gx_device_pdf *pdev, pdf_resource_t **ppres,
            pdf_resource_type_t rtype,
            int (*eq)(gx_device_pdf *pdev, pdf_resource_t *pres0, pdf_resource_t *pres1),
            bool write);

/* Get the object id of a resource. */
int64_t pdf_resource_id(const pdf_resource_t *pres);

/* End a separate object. */
int pdf_end_separate(gx_device_pdf * pdev, pdf_resource_type_t type);

/* End an aside. */
int pdf_end_aside(gx_device_pdf * pdev, pdf_resource_type_t type);

/* End a resource. */
int pdf_end_resource(gx_device_pdf * pdev, pdf_resource_type_t type);

/*
 * Write the Cos objects for resources local to a content stream.
 */
int pdf_write_resource_objects(gx_device_pdf *pdev, pdf_resource_type_t rtype);

/*
 * Reverse resource chains.
 * ps2write uses it with page resources.
 * Assuming only the 0th chain contauns something.
 */
void pdf_reverse_resource_chain(gx_device_pdf *pdev, pdf_resource_type_t rtype);

/*
 * Free unnamed Cos objects for resources local to a content stream.
 */
int pdf_free_resource_objects(gx_device_pdf *pdev, pdf_resource_type_t rtype);

/*
 * Store the resource sets for a content stream (page or XObject).
 * Sets page->{procsets, resource_ids[], fonts_id}.
 */
int pdf_store_page_resources(gx_device_pdf *pdev, pdf_page_t *page, bool clear_usage);

/* Copy data from a temporary file to a stream. */
int pdf_copy_data(stream *s, gp_file *file, gs_offset_t count, stream_arcfour_state *ss);
int pdf_copy_data_safe(stream *s, gp_file *file, gs_offset_t position, int64_t count);

/* Add the encryption filter. */
int pdf_begin_encrypt(gx_device_pdf * pdev, stream **s, gs_id object_id);
/* Remove the encryption filter. */
static int inline pdf_end_encrypt(gx_device_pdf *pdev)
{
    if (pdev->KeyLength)
        return 1;
    return 0;
}
/* Initialize encryption. */
int pdf_encrypt_init(const gx_device_pdf * pdev, gs_id object_id, stream_arcfour_state *psarc4);

/* ------ Pages ------ */

/* Get or assign the ID for a page. */
/* Returns 0 if the page number is out of range. */
int64_t pdf_page_id(gx_device_pdf * pdev, int page_num);

/* Get the page structure for the current page. */
pdf_page_t *pdf_current_page(gx_device_pdf *pdev);

/* Get the dictionary object for the current page. */
cos_dict_t *pdf_current_page_dict(gx_device_pdf *pdev);

/* Open a page for writing. */
int pdf_open_page(gx_device_pdf * pdev, pdf_context_t context);

/*  Go to the unclipped stream context. */
int pdf_unclip(gx_device_pdf * pdev);

/* Write saved page- or document-level information. */
int pdf_write_saved_string(gx_device_pdf * pdev, gs_string * pstr);

/* ------ Path drawing ------ */

/* Store a copy of clipping path. */
int pdf_remember_clip_path(gx_device_pdf * pdev, const gx_clip_path * pcpath);

int pdf_check_soft_mask(gx_device_pdf * pdev, gs_gstate * pgs);

/* Test whether the clip path needs updating. */
bool pdf_must_put_clip_path(gx_device_pdf * pdev, const gx_clip_path * pcpath);

/* Write and update the clip path. */
int pdf_put_clip_path(gx_device_pdf * pdev, const gx_clip_path * pcpath);

/* ------ Masked image convertion ------ */

typedef struct pdf_lcvd_s {
    gx_device_memory mdev;
    gx_device_memory *mask;
    gx_device_pdf *pdev;
    dev_t_proc_copy_color((*std_copy_color), gx_device);
    dev_t_proc_copy_mono((*std_copy_mono), gx_device);
    dev_t_proc_fill_rectangle((*std_fill_rectangle), gx_device);
    dev_t_proc_close_device((*std_close_device), gx_device);
    dev_t_proc_get_clipping_box((*std_get_clipping_box), gx_device);
    dev_t_proc_transform_pixel_region((*std_transform_pixel_region), gx_device);
    bool mask_is_empty;
    bool path_is_empty;
    bool mask_is_clean;
    bool filled_trap;
    bool write_matrix;
    bool has_background;
    gs_matrix m;
    gs_point path_offset;
} pdf_lcvd_t;

#define public_st_pdf_lcvd_t()\
  gs_public_st_suffix_add2(st_pdf_lcvd_t, pdf_lcvd_t,\
    "pdf_lcvd_t", pdf_lcvd_t_enum_ptrs,\
    pdf_lcvd_t_reloc_ptrs, st_device_memory, mask, pdev)
#define pdf_lcvd_t_max_ptrs (gx_device_memory_max_ptrs + 2)

int pdf_setup_masked_image_converter(gx_device_pdf *pdev, gs_memory_t *mem, const gs_matrix *m, pdf_lcvd_t **pcvd,
                                 bool need_mask, int x, int y, int w, int h, bool write_on_close);
int pdf_dump_converted_image(gx_device_pdf *pdev, pdf_lcvd_t *cvd, int for_pattern);
void pdf_remove_masked_image_converter(gx_device_pdf *pdev, pdf_lcvd_t *cvd, bool need_mask);

/* ------ Miscellaneous output ------ */

#define PDF_MAX_PRODUCER 200        /* adhoc */
/* Generate the default Producer string. */
void pdf_store_default_Producer(char buf[PDF_MAX_PRODUCER]);

/* Define the strings for filter names and parameters. */
typedef struct pdf_filter_names_s {
    const char *ASCII85Decode;
    const char *ASCIIHexDecode;
    const char *CCITTFaxDecode;
    const char *DCTDecode;
    const char *DecodeParms;
    const char *Filter;
    const char *FlateDecode;
    const char *LZWDecode;
    const char *RunLengthDecode;
    const char *JBIG2Decode;
    const char *JPXDecode;
} pdf_filter_names_t;
#define PDF_FILTER_NAMES\
  "/ASCII85Decode", "/ASCIIHexDecode", "/CCITTFaxDecode",\
  "/DCTDecode",  "/DecodeParms", "/Filter", "/FlateDecode",\
  "/LZWDecode", "/RunLengthDecode", "/JBIG2Decode", "/JPXDecode"
#define PDF_FILTER_NAMES_SHORT\
  "/A85", "/AHx", "/CCF", "/DCT", "/DP", "/F", "/Fl", "/LZW", "/RL", "/???", "/???"

/* Write matrix values. */
void pdf_put_matrix(gx_device_pdf *pdev, const char *before,
                    const gs_matrix *pmat, const char *after);

/* Write a name, with escapes for unusual characters. */
typedef int (*pdf_put_name_chars_proc_t)(stream *, const byte *, uint);
pdf_put_name_chars_proc_t
    pdf_put_name_chars_proc(const gx_device_pdf *pdev);
int pdf_put_name_chars(const gx_device_pdf *pdev, const byte *nstr,
                        uint size);
int pdf_put_name(const gx_device_pdf *pdev, const byte *nstr, uint size);

/* Write a string in its shortest form ( () or <> ). */
int pdf_put_string(const gx_device_pdf *pdev, const byte *str, uint size);

/* Write a value, treating names specially. */
int pdf_write_value(const gx_device_pdf *pdev, const byte *vstr, uint size, gs_id object_id);

/* Store filters for a stream. */
int pdf_put_filters(cos_dict_t *pcd, gx_device_pdf *pdev, stream *s,
                    const pdf_filter_names_t *pfn);

/* Define a possibly encoded and compressed data stream. */
typedef struct pdf_data_writer_s {
    psdf_binary_writer binary;
    gs_offset_t start;
    gs_offset_t length_pos;
    pdf_resource_t *pres;
    gx_device_pdf *pdev; /* temporary for backward compatibility of pdf_end_data prototype. */
    int64_t length_id;
    bool encrypted;
} pdf_data_writer_t;
/*
 * Begin a data stream.  The client has opened the object and written
 * the << and any desired dictionary keys.
 */
#define DATA_STREAM_NOT_BINARY 0  /* data are text, not binary */
#define DATA_STREAM_BINARY 1        /* data are binary */
#define DATA_STREAM_COMPRESS 2        /* OK to compress data */
#define DATA_STREAM_NOLENGTH 4        /* Skip the length reference and filter names writing. */
#define DATA_STREAM_ENCRYPT  8        /* Encrypt data. */
int pdf_begin_data_stream(gx_device_pdf *pdev, pdf_data_writer_t *pdw,
                          int options, gs_id object_id);
int pdf_append_data_stream_filters(gx_device_pdf *pdev, pdf_data_writer_t *pdw,
                      int orig_options, gs_id object_id);
/* begin_data = begin_data_binary with both options = true. */
int pdf_begin_data(gx_device_pdf *pdev, pdf_data_writer_t *pdw);

/* End a data stream. */
int pdf_end_data(pdf_data_writer_t *pdw);

/* ------ Functions ------ */

/* Define the maximum size of a Function reference. */
#define MAX_REF_CHARS ((sizeof(int64_t) * 8 + 2) / 3)

/*
 * Create a Function object with or without range scaling.  Scaling means
 * that if x[i] is the i'th output value from the original Function,
 * the i'th output value from the Function object will be (x[i] -
 * ranges[i].rmin) / (ranges[i].rmax - ranges[i].rmin).  Note that this is
 * the inverse of the scaling convention for Functions per se.
 */
int pdf_function(gx_device_pdf *pdev, const gs_function_t *pfn,
                 cos_value_t *pvalue);
int pdf_function_scaled(gx_device_pdf *pdev, const gs_function_t *pfn,
                        const gs_range_t *pranges, cos_value_t *pvalue);

/* Write a Function object, returning its object ID. */
int pdf_write_function(gx_device_pdf *pdev, const gs_function_t *pfn,
                       int64_t *pid);

/* If a stitching function references an array of other functions, we need
 * to 'unreference' those before freeing the function. otherwise we end up
 * trying to free the referenced functions twice.
 */
int free_function_refs(gx_device_pdf *pdev, cos_object_t *pco);
/* ------ Fonts ------ */

/* Write a FontBBox dictionary element. */
int pdf_write_font_bbox(gx_device_pdf *pdev, const gs_int_rect *pbox);
int pdf_write_font_bbox_float(gx_device_pdf *pdev, const gs_rect *pbox);

/* ---------------- Exported by gdevpdfm.c ---------------- */

/*
 * Define the type for a pdfmark-processing procedure.
 * If nameable is false, the objname argument is always NULL.
 */
#define pdfmark_proc(proc)\
  int proc(gx_device_pdf *pdev, gs_param_string *pairs, uint count,\
           const gs_matrix *pctm, const gs_param_string *objname)

/* Compare a C string and a gs_param_string. */
bool pdf_key_eq(const gs_param_string * pcs, const char *str);

/* Scan an integer out of a parameter string. */
int pdfmark_scan_int(const gs_param_string * pstr, int *pvalue);

/* Process a pdfmark (called from pdf_put_params). */
int pdfmark_process(gx_device_pdf * pdev, const gs_param_string_array * pma);

/* Close the current level of the outline tree. */
int pdfmark_close_outline(gx_device_pdf * pdev);

/* Close the pagelabel numtree. */
int pdfmark_end_pagelabels(gx_device_pdf * pdev);

/* Finish writing an article. */
int pdfmark_write_article(gx_device_pdf * pdev, const pdf_article_t * part);

/* ---------------- Exported by gdevpdfr.c ---------------- */

/* Test whether an object name has valid syntax, {name}. */
bool pdf_objname_is_valid(const byte *data, uint size);

/*
 * Look up a named object.  Return_error(gs_error_rangecheck if the syntax is invalid,
 * gs_error_undefined if no object by that name exists.
 */
int pdf_find_named(gx_device_pdf * pdev, const gs_param_string * pname,
                   cos_object_t **ppco);

/*
 * Create a named object.  id = -1L means do not assign an id.  pname = 0
 * means just create the object, do not name it.
 */
int pdf_create_named(gx_device_pdf *pdev, const gs_param_string *pname,
                     cos_type_t cotype, cos_object_t **ppco, int64_t id);
int pdf_create_named_dict(gx_device_pdf *pdev, const gs_param_string *pname,
                          cos_dict_t **ppcd, int64_t id);

/*
 * Look up a named object as for pdf_find_named.  If the object does not
 * exist, create it (as a dictionary if it is one of the predefined names
 * {ThisPage}, {NextPage}, {PrevPage}, or {Page<#>}, otherwise as a
 * generic object) and return 1.
 */
int pdf_refer_named(gx_device_pdf *pdev, const gs_param_string *pname,
                    cos_object_t **ppco);

/*
 * Look up a named object as for pdf_refer_named.  If the object already
 * exists and is not simply a forward reference, return_error(gs_error_rangecheck);
 * if it exists as a forward reference, set its type and return 0;
 * otherwise, create the object with the given type and return 1.
 * pname = 0 is allowed: in this case, simply create the object.
 */
int pdf_make_named(gx_device_pdf * pdev, const gs_param_string * pname,
                   cos_type_t cotype, cos_object_t **ppco, bool assign_id);
int pdf_make_named_dict(gx_device_pdf * pdev, const gs_param_string * pname,
                        cos_dict_t **ppcd, bool assign_id);

/*
 * Look up a named object as for pdf_refer_named.  If the object does not
 * exist, or is a forward reference, return gs_error_undefined; if the object
 * exists has the wrong type, return gs_error_typecheck.
 */
int pdf_get_named(gx_device_pdf * pdev, const gs_param_string * pname,
                  cos_type_t cotype, cos_object_t **ppco);

/*
 * Push the current local namespace onto the namespace stack, and reset it
 * to an empty namespace.
 */
int pdf_push_namespace(gx_device_pdf *pdev);

/*
 * Pop the top local namespace from the namespace stack.  Return an error if
 * the stack is empty.
 */
int pdf_pop_namespace(gx_device_pdf *pdev);

/*
 * Scan a string for a token.  <<, >>, [, and ] are treated as tokens.
 * Return 1 if a token was scanned, 0 if we reached the end of the string,
 * or an error.  On a successful return, the token extends from *ptoken up
 * to but not including *pscan.
 */
int pdf_scan_token(const byte **pscan, const byte * end, const byte **ptoken);

/*
 * Scan a possibly composite token: arrays and dictionaries are treated as
 * single tokens.
 */
int pdf_scan_token_composite(const byte **pscan, const byte * end,
                             const byte **ptoken);

/* Replace object names with object references in a (parameter) string. */
int pdf_replace_names(gx_device_pdf *pdev, const gs_param_string *from,
                      gs_param_string *to);

/* ================ Text module procedures ================ */

/* ---------------- Exported by gdevpdfw.c ---------------- */

/* For gdevpdf.c */

int write_font_resources(gx_device_pdf *pdev, pdf_resource_list_t *prlist);

/* ---------------- Exported by gdevpdft.c ---------------- */

/* For gdevpdf.c */

pdf_text_data_t *pdf_text_data_alloc(gs_memory_t *mem);
void pdf_set_text_state_default(pdf_text_state_t *pts);
void pdf_text_state_copy(pdf_text_state_t *pts_to, pdf_text_state_t *pts_from);
void pdf_reset_text_page(pdf_text_data_t *ptd);
void pdf_reset_text_state(pdf_text_data_t *ptd);
void pdf_close_text_page(gx_device_pdf *pdev);

/* For gdevpdfb.c */

int pdf_char_image_y_offset(const gx_device_pdf *pdev, int x, int y, int h);

/* Begin a CharProc for an embedded (bitmap) font. */
int pdf_begin_char_proc(gx_device_pdf * pdev, int w, int h, int x_width,
                        int y_offset, int x_offset, gs_id id, pdf_char_proc_t **ppcp,
                        pdf_stream_position_t * ppos);

/* End a CharProc. */
int pdf_end_char_proc(gx_device_pdf * pdev, pdf_stream_position_t * ppos);

/* Put out a reference to an image as a character in an embedded font. */
int pdf_do_char_image(gx_device_pdf * pdev, const pdf_char_proc_t * pcp,
                      const gs_matrix * pimat);

/* Start charproc accumulation for a Type 3 font. */
int pdf_start_charproc_accum(gx_device_pdf *pdev);
/* Install charproc accumulator for a Type 3 font. */
int pdf_set_charproc_attrs(gx_device_pdf *pdev, gs_font *font, double *pw, int narg,
                gs_text_cache_control_t control, gs_char ch, bool scale_100);
/* Complete charproc accumulation for aType 3 font. */
int pdf_end_charproc_accum(gx_device_pdf *pdev, gs_font *font, const pdf_char_glyph_pairs_t *cgp,
                       gs_glyph glyph, gs_char output_char_code, const gs_const_string *gnstr);
/* Open a stream object in the temporary file. */
int pdf_open_aside(gx_device_pdf *pdev, pdf_resource_type_t rtype,
        gs_id id, pdf_resource_t **ppres, bool reserve_object_id, int options);

/* Close a stream object in the temporary file. */
int pdf_close_aside(gx_device_pdf *pdev);

/* Enter the substream accumulation mode. */
int pdf_enter_substream(gx_device_pdf *pdev, pdf_resource_type_t rtype,
                gs_id id, pdf_resource_t **ppres, bool reserve_object_id, bool compress);

/* Exit the substream accumulation mode. */
int pdf_exit_substream(gx_device_pdf *pdev);
/* Add procsets to substream Resources. */
int pdf_add_procsets(cos_dict_t *pcd, pdf_procset_t procsets);
/* Add a resource to substream Resources. */
int pdf_add_resource(gx_device_pdf *pdev, cos_dict_t *pcd, const char *key, pdf_resource_t *pres);

/* For gdevpdfu.c */

int pdf_from_stream_to_text(gx_device_pdf *pdev);
int pdf_from_string_to_text(gx_device_pdf *pdev);
void pdf_close_text_contents(gx_device_pdf *pdev);

int gdev_pdf_get_param(gx_device *dev, char *Param, void *list);

int pdf_open_temp_file(gx_device_pdf *pdev, pdf_temp_file_t *ptf);
int pdf_open_temp_stream(gx_device_pdf *pdev, pdf_temp_file_t *ptf);
int pdf_close_temp_file(gx_device_pdf *pdev, pdf_temp_file_t *ptf, int code);

/* exported by gdevpdfe.c */

int pdf_xmp_write_translated(gx_device_pdf* pdev, stream* s, const byte* data, int data_length,
    void(*write)(stream* s, const byte* data, int data_length));

#endif /* gdevpdfx_INCLUDED */
