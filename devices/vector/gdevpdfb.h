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


/* pdfwrite, ps2write device body template. */

/* This file is allowed to #include several times into a single .c file.
   The following macros to be defined in advance :
   PDF_DEVICE_NAME - a string like "pdfwrite".
   PDF_DEVICE_IDENT - an identifier like gs_pdfwrite_device.
   PDF_DEVICE_MaxInlineImageSize - a value of PDF_DEVICE_MaxInlineImageSize.
   PDF_FOR_OPDFREAD - an integer 0 (false) or 1 (true).
 */

#ifdef PDF_DEVICE_NAME

const gx_device_pdf PDF_DEVICE_IDENT =
{std_device_dci_type_body(gx_device_pdf, pdfwrite_initialize_device_procs,
                          PDF_DEVICE_NAME,
                          &st_device_pdfwrite,
                          DEFAULT_WIDTH_10THS * X_DPI / 10,
                          DEFAULT_HEIGHT_10THS * Y_DPI / 10,
                          X_DPI, Y_DPI,
                          3, 24, 255, 255, 256, 256),
 { 0 },
 psdf_initial_values(PSDF_VERSION_INITIAL, 0 /*false */ ),  /* (!ASCII85EncodePages) */
 0,                     /* pdf_font_dir */
 PDF_FOR_OPDFREAD,		/* is_ps2write */
 PDF_COMPATIBILITY_LEVEL_INITIAL,  /* CompatibilityLevel */
 -1,				/* EndPage */
 1,				/* StartPage */
 1 /*true*/,			/* Optimize */
 1 /*true*/,			/* ParseDSCCommentsForDocInfo */
 1 /*true*/,			/* ParseDSCComments */
 0 /*false*/,			/* EmitDSCWarnings */
 0 /*false*/,			/* CreateJobTicket */
 1 /*true*/,			/* PreserveEPSInfo */
 1 /*true*/,			/* AutoPositionEPSFiles */
 1 /*true*/,			/* PreserveCopyPage */
 0 /*false*/,			/* UsePrologue */
 0,				/* OffOptimizations */
 {0,0},				/* PDFXTrimBoxToMediaBoxOffset */
 {0,0},				/* PDFXBleedBoxToTrimBoxOffset */
 1 /* true */,			/* PDFXSetBleedBoxToMediaBox */
 "",                            /* ocr_language */
 0,                             /* ocr_engine */
 1 /*true*/,			/* ReAssignCharacters */
 1 /*true*/,			/* ReEncodeCharacters */
 1,				/* FirstObjectNumber */
 1 /*true*/,			/* CompressFonts */
 1 /*true*/,			/* CompressStreams */
 0 /*false*/,			/* PrintStatistics */
 {0, 0, 0},			/* DocumentUUID */
 {0, 0, 0},			/* InstanceUUID */
 0,				/* DocumentTimeSeq */
 PDF_FOR_OPDFREAD,		/* ForOPDFRead */
 PDF_FOR_EPS2WRITE,     /* is_eps2write */
 false,				/* CompressEntireFile */
 0 /*false*/,			/* ResourcesBeforeUsage */
 1 /*true*/,			/* HavePDFWidths */
 0 /*false*/,			/* HaveStrokeColor */
 1 /*true*/,			/* ProduceDSC */
 1 /*true*/,			/* HaveTransparency */
 0 /*false*/,			/* PatternImagemask */
 0 /*false*/,			/* PDFX */
 0 /*false*/,			/* PDFA */
 0 /*false*/,			/* Abort generation of PDFA or X, produce PDF */
 12000,				/* MaxClipPathSize */ /* HP LaserJet 1320 hangs with 14000. */
 256000,			/* MaxShadingBitmapSize */
 PDF_DEVICE_MaxInlineImageSize,	/* MaxInlineImageSize */
 {0, 0, 0},			/* OwnerPassword */
 {0, 0, 0},			/* UserPassword */
 0,				/* KeyLength */
 -4,				/* Permissions */
 0,				/* EncryptionR */
 {0, 0, 0},			/* NoEncrypt */
 true,				/* EncryptMetadata */
 true,                          /* ComputeDocumentDigest */
 {0},				/* EncryptionO */
 {0},				/* EncryptionU */
 {0},				/* EncryptionKey */
 0,				/* EncryptionV */
 0 /*false*/,			/* is_EPS */
 0,             /* AccumulatingBBox */
 {{0,0},{0,0}},		/* BBox */
 {-1, -1},			/* doc_dsc_info */
 {-1, -1},			/* page_dsc_info */
 0 /*false*/,			/* fill_overprint */
 0 /*false*/,			/* stroke_overprint */
 1 /* Absolute Colorimetric */, /* rendering intent */
 0 /*false */,                  /* user_icc */
 0 /*false*/,                   /* remap_fill_coilor */
 0 /*false*/,                   /* remap_stroke_coilor */
 gs_no_id,			/* halftone_id */
 {gs_no_id, gs_no_id, gs_no_id, gs_no_id}, /* transfer_ids */
 0,				/* transfer_not_identity */
 gs_no_id,			/* black_generation_id */
 gs_no_id,			/* undercolor_removal_id */
 pdf_compress_none,		/* compression */
 pdf_compress_none,		/* compression_at_page_start */
 {{0}},				/* xref */
 {{0}},				/* asides */
 {{0}},				/* streams */
 {{0}},				/* ObjStm */
 gs_no_id,          /* ObjStm_id */
 NULL,              /* ObjStmOffsets */
 0,                 /* NumObjstmObjects */
 true,             /* doubleXref */
 0,				/* next_id */
 0,				/* Catalog */
 0,				/* Info */
 0,				/* Pages */
 0,				/* outlines_id */
 0,				/* next_page */
 -1,				/* max_referred_page */
 0,				/* contents_id */
 PDF_IN_NONE,			/* context */
 0,				/* contents_length_id */
 0,				/* contents_pos */
 NoMarks,			/* procsets */
 0,				/* text */
 {{0}},				/* text_rotation */
 0,				/* pages */
 0,				/* num_pages */
 1,				/* used_mask */
 {
     {
         {0}}},	/* resources */
 {0},			/* cs_Patterns */
 {0},			/* Identity_ToUnicode_CMaps */
 0,				/* last_resource */
 0,				/* OneByteIdentityH */
 gs_no_id,		/* IdentityCIDSystemInfo_id */
 0,             /* outline_levels */
 -1,			/* outline_depth */
 0,             /* max_outline_depth */
 0,				/* closed_outline_depth */
 0,				/* outlines_open */
 0,				/* articles */
 0,				/* Dests */
 0,				/* EmbeddedFiles */
 0,             /* AF (part of embedded files for PDF/A-3) */
 {0},				/* fileID */
         {0, 0},		/* uuid_time */
 0,				/* global_named_objects */
 0,				/* local_named_objects */
 0,				/* NI_stack */
 0,				/* Namespace_stack */
 0,				/* font_cache */
 {0, 0},			/* char_width */
 0,				/* clip_path */
 0,                             /* PageLabels */
 -1,                            /* PageLabels_current_page */
 0,                             /* PageLabels_current_label */
 0,				/* */
 0,             /* vgstack */
 0,             /* vgstack_size */
 0,				/* vgstack_depth */
 0,				/* vgstack_bottom */
 {0},				/* vg_initial */
 false,				/* vg_initial_set */
 0,				/* sbstack_size */
 0,				/* sbstack_depth */
 0,				/* sbstack */
 0,				/* FormDepth */
 0,             /* HighLevelForm */
 0,				/* PatternDepth */
 {0,0,0,0,0,0},                 /* AccumulatedPatternMatrix */
 0,             /* PatternsSinceForm */
 0,				/* substream_Resources */
 1,				/* pcm_color_info_index == DeviceRGB */
 false,				/* skip_colors */
 false,				/* AR4_save_bug */
 0,				/* font3 */
 0,				/* accumulating_substream_resource */
 {0,0,0,0,0,0,0,0,0},		/* charproc_ctm */
 0,             /* accumulating_charproc */
 {{0, 0}, {0, 0}},  /* Charproc BBox */
 0,				/* charproc_just_accumulated */
 1,             /* PS_accumulator */
 0,             /* Scaled_accumulator */
 0,				/* accumulating_a_global_object */
 0,				/* cgp */
 0,				/* substituted_pattern_count */
 0,				/* substituted_pattern_drop_page */
 0,				/* image_mask_id */
 0,				/* image_mask_is_SMask */
 0,				/* image_mask_skip */
 0,             /* smask_construction */
 0,				/* image_with_SMask */
 {0,0,0,0,0,0}, 		/* gs_matrix converting_image_matrix */
 0,				/* image_mask_scale */
 NULL,				/* pres_soft_mask_dict */
 0,                 /* pdfmark_dup_stream */
 {0, 0},			/* objname */
 0,				/* OPDFRead_procset_length */
 0,				/* find_resource_param */
 0,				/* last_charpath_op */
 0,				/* type3charpath */
 1,				/* SetPageSize */
 0,				/* RotatePages */
 0,				/* Fit Pages */
 0,				/* CenterPages */
 0,				/* DoNumCopies */
 true,				/* PreserveSeparation */
 true,				/* PreserveDeviceN */
 0,				/* PDFACompatibilityPolicy */
 true,				/* DetectDuplicateImages */
 false,				/* AllowIncrementalCFF */
 !PDF_FOR_OPDFREAD,		/* WantsToUnicode */
 !PDF_FOR_OPDFREAD,		/* WantsOptionalContent */
 !PDF_FOR_OPDFREAD,		/* PdfmarkCapable */
 !PDF_FOR_OPDFREAD,		/* WantsPageLabels */
 PDF_FOR_OPDFREAD,		/* AllowPSRepeatFunctions */
 true,				/* IsDistiller (true even for ps2write!) */
 !PDF_FOR_OPDFREAD,		/* PreserveSMask */
 !PDF_FOR_OPDFREAD,		/* PreserveTrMode */
 false,                 /* NoT3CCITT */
 false,                 /* Linearise */
 0,                     /* pointer to resourceusage */
 0,                     /* Size of resourceusage */
 false,                 /* called from output_page */
 false,                 /* FlattenFonts, writes text as outlines instead of fonts */
 -1,                    /* Last Form ID, start with -1 which means 'none' */
 0,                     /* ExtensionMetadata */
 0,                     /* PDFFormName */
 0,                     /* PassThroughWriter */
 1.0,                   /* UserUnit */
 0,                     /* UseOCR */
 NULL,                  /* OCRSaved */
 0,                     /* OCRStage */
 NULL,                  /* OCRUnicode */
 0,                     /* OCR_char_code */
 0,                     /* OCR_glyph */
 NULL,                  /* ocr_glyphs */
 0,                     /* initial_pattern_state */
 false,                 /* OmitInfoDate */
 false,                 /* OmitXMP */
 false,                 /* OmitID */
 false,                 /* ModifiesPageSize */
 false,                 /* ModifiesPageOrder */
 true,                  /* WriteXRefStm */
 true,                  /* WriteObjStms */
 0,                     /* PendingOC */
 true                   /* ToUnicodeForStdEnc */
};

#else
int dummy;
#endif
