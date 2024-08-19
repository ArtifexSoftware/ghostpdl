/* Copyright (C) 2018-2024 Artifex Software, Inc.
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


#ifndef PDF_CONTEXT
#define PDF_CONTEXT

#include "gserrors.h"   /* Most files use this to set errors of the gs_error_ form */
#include "gxgstate.h"

#define BUF_SIZE 2048

/* Limit nesting of arrays and dictionaries. We don't want to allow this
 * to be unbounded, because on exit we could end up exceeding the C execution stack
 * if we get too deeply nested.
 */
#define MAX_NESTING_DEPTH 100

#include "pdf_types.h"

#if defined(MEMENTO)
#define PDFI_LEAK_CHECK 0
#endif

#ifndef PDFI_LEAK_CHECK
#define PDFI_LEAK_CHECK 0
#endif

/* A structure for setting/resetting the interpreter graphics state
 * and some graphics state content when switching between Ghostscript
 * and pdfi, when running under GS.
 */
typedef struct pdf_context_switch {
    gs_gstate *pgs;
    gs_font *psfont;
    gs_gstate_client_procs procs;
    void *client_data;
    void *profile_cache;
} pdfi_switch_t;

/*
 * The interpreter context.
 */
/* Warnings and errors. The difference between a warning and an error is that we use a warning where
 * the file is technically illegal but we can be certain as to the real intent. At the time of writing
 * the only case is also a perfect example; the use of an inline image filter abbreviation (eg A85)
 * on a stream or object which is not an inline image. Although technically incorrect, its obvious
 * how to deal with this.
 */
typedef enum pdf_error_e {
#include "pdf_errors.h"
    E_PDF_MAX_ERROR       /* last entry */
}pdf_error;

typedef enum pdf_warning_e {
#include "pdf_warnings.h"
    W_PDF_MAX_WARNING     /* last entry */
} pdf_warning;

#define PDF_ERROR_BYTE_SIZE ((E_PDF_MAX_ERROR - 1) / (sizeof(char) * 8) + 1)
#define PDF_WARNING_BYTE_SIZE ((W_PDF_MAX_WARNING - 1) / (sizeof(char) * 8) + 1)

typedef enum pdf_crypt_filter_e {
    CRYPT_NONE,     /* Not an encrypted file */
    CRYPT_IDENTITY, /* Encrypted file, but no encryption on this object type */
    CRYPT_V1,     /* 40-bit RC4 */
    CRYPT_V2,     /* 128-bit RC4 */
    CRYPT_AESV2,  /* 128-bit AES */
    CRYPT_AESV3,  /* 256-bit AES */
} pdf_crypt_filter;

typedef enum pdf_type3_d_type_e {
    pdf_type3_d_none,
    pdf_type3_d0,
    pdf_type3_d1
} pdf_type3_d_type;

#define INITIAL_STACK_SIZE 32
#define MAX_STACK_SIZE 524288
#define MAX_OBJECT_CACHE_SIZE 200
#define INITIAL_LOOP_TRACKER_SIZE 32

typedef struct pdf_transfer_s {
    gs_mapping_proc proc;	/* typedef is in gxtmap.h */
    frac values[transfer_map_size];
} pdf_transfer_t;

/* Items we want preserved around content stream executions */
typedef struct stream_save_s {
    gs_offset_t stream_offset;
    int gsave_level;
    int stack_count;
    gs_matrix intial_CTM;
    int group_depth;
} stream_save;

/* resource font object cache - this is a simple lookup table
   to match a FontDescriptor containing a FontFile* entry with
   a pdfi font object derived from the FontFile stream
 */

#define RESOURCE_FONT_CACHE_BLOCK_SIZE 32
typedef struct resource_font_cache_s resource_font_cache_t;

struct resource_font_cache_s
{
    int desc_obj_num;
    pdf_obj *pdffont;
};

typedef struct name_entry_s {
    char *name;
    int len;
    unsigned int index;
    void *next;
} pdfi_name_entry_t;

typedef struct cmd_args_s {
    /* These are various command line switches, the list is not yet complete */
    int first_page;             /* -dFirstPage= */
    int last_page;              /* -dLastPage= */
    bool pdfdebug;
    bool pdfstoponerror;
    bool pdfstoponwarning;
    bool notransparency;
    bool nocidfallback;
    int PDFA;
    int PDFX;
    bool no_pdfmark_outlines; /* can be overridden to true if multi-page output */
    bool no_pdfmark_dests;    /* can be overridden to true if multi-page output */
    bool pdffitpage;
    bool usecropbox;
    bool useartbox;
    bool usebleedbox;
    bool usetrimbox;
    bool printed;
    bool showacroform;
    bool showannots;
    char **showannottypes; /* Null terminated array of strings, NULL if none */
    bool dopdfmarks;
    bool preserveannots;
    char **preserveannottypes; /* Null terminated array of strings, NULL if none */
    bool preservemarkedcontent;
    bool preserveembeddedfiles;
    bool preservedocview;
    bool nouserunit;
    bool renderttnotdef;
    bool pdfinfo;
    bool UsePDFX3Profile;
    bool NOSUBSTDEVICECOLORS;
    bool ditherppi;
    int PDFX3Profile_num;
    char *UseOutputIntent;
    char *PageList;
    bool QUIET;
    bool verbose_errors;
    bool verbose_warnings;
    gs_string cidfsubstpath;
    gs_string cidfsubstfont;
    gs_string defaultfont;
    bool defaultfont_is_name;

    bool ignoretounicode;
    bool nonativefontmap;
} cmd_args_t;

typedef struct encryption_state_s {
    /* Encryption, passwords and filter details */
    bool is_encrypted;
    int V;
    int Length;
    char *Password;
    int PasswordLen;
    int R;
    /* Revision 1-4 have O and E being 32 bytes, revision 5 and 6 48 bytes */
    char O[48];
    char U[48];
    /* OE and UE are used for revision 5 and 6 encryption */
    char OE[32];
    char UE[32];
    int P;
    pdf_string *EKey;
    bool EncryptMetadata;
    pdf_crypt_filter StrF;
    pdf_crypt_filter StmF;
    /* decrypting strings is complicated :-(
     * Streams are easy, because they can't be in compressed ObjStms, and they
     * have to be indirect objects. Strings can be indirect references or directly
     * defined, can be in compressed ObjStms and can appear inside content streams.
     * When they are in content streams we don't decrypt them, because the *stream*
     * was already decrypted. So when strings are directly or indirectly defined,
     * and *NOT* defined as part of a content stream, and not in an Objstm, we
     * need to decrypt them. We can handle the checking for ObjStm in the actual
     * decryption routine, where we also handle picking out the object number of the
     * enclosing parent, if its a directly defined string, but we cannot tell
     * whether we are executing a content stream or not, so we need to know that. This
     * flag is set whenever we are executing a content stream, it is temporarily reset
     * by pdfi_dereference() because indirect references can't appear in a content stream
     * so we know we need to decrypt any strings that are indirectly referenced. Note that
     * Form handling needs to set this flag for the duration of a Form content stream,
     * because we can execute Forms outside a page context (eg Annotations).
     */
    bool decrypt_strings;
} encryption_state_t;

typedef struct page_state_s {
    /* Page level PDF objects */
    /* DefaultGray, RGB and CMYK spaces */
    gs_color_space *DefaultGray_cs;
    gs_color_space *DefaultRGB_cs;
    gs_color_space *DefaultCMYK_cs;
    /* Last-ditch resource lookup */
    pdf_dict *CurrentPageDict;
    /* Page leve 'Default' transfer functions, black generation and under colour removal */
    pdf_transfer_t DefaultTransfers[4];
    pdf_transfer_t DefaultBG;
    pdf_transfer_t DefaultUCR;
    /* This tracks whether the current page uses transparency features */
    bool has_transparency;
    /* This tracks how many spots are on the current page */
    int num_spots;
    /* Does this page need overprint support? */
    bool needs_OP;
    /* Does this page have OP=true in ExtGState? */
    bool has_OP;
    /* Are we simulating overprint on this page? */
    bool simulate_op;
    double Size[4];
    double Crop[4];
    double UserUnit;
} page_state_t;

typedef struct text_state_s {
    /* we need the text enumerator in order to call gs_text_setcharwidth() for d0 and d1 */
    gs_text_enum_t *current_enum;
    /* Detect if we are inside a text block at any time. Nested text blocks are illegal and certain
     * marking operations are illegal inside text blocks. We also manipulate this when rendering
     * type 3 BuildChar procedures, as those marking operations are legal in a BuildChar, even
     * when we are in a text block.
     */
    int BlockDepth;
    /* We set this when in a clipping text rendering mode when we draw the first text
     * We use this (and the BlockDepth) to detect whether switching to a non-clipping
     * text mode is an error or not.
     */
    bool TextClip;
    /* This is to determine if we get Type 3 Charproc operators (d0 and d1) outside
     * a Type 3 BuildChar.
     */
    bool inside_CharProc;
    /* We need to know if we're in a type 3 CharProc which has executed a 'd1' operator.
     * Colour operators are technically invalid if we are in a 'd1' context and we must
     * ignore them.
     * OSS-fuzz #45320 has a type 3 font with a BuildChar which has a 'RG' before the
     * d1. This is (obviously) illegal because the spec says the first operation must
     * be either a d0 or d1, in addition because of the graphics state depth hackery
     * (see comments in pdf_d0() in pdf_font.c) this messes up the reference counting
     * of the colour spaces, leading to a crash. So what was a boolean flag is now an
     * enumerated type; pdf_type3_d_none, pdf_type3_d0 or pdf_type3_d1.
     */
    pdf_type3_d_type CharProc_d_type;
    /* If there is no current point when we do a BT we start by doing a 0 0 moveto in order
     * to establish an initial point. However, this also starts a path. When we finish
     * off with a BT we need to clear that path by doing a newpath, otherwise we might
     * end up drawing it. See /tests_private/comparefiles/Bug692867.pdf
     * We store the initial current poitn validity and if t was not initially valid
     * (ie no path) then we do a newpath on a ET.
     * BT/ET are not supposed to be nested, and path construction is not permitted inside
     * a BT/ET block.
     */
    bool initial_current_point_valid;
} text_state_t;

typedef struct device_state_s {
    /* Parameters/capabilities of the selected device */
    /* Needed to determine whether we need to reset the device to handle any spots
     * and whether we need to prescan the PDF file to determine how many spot colourants
     * (if any) are used in the file.
     */
    bool spot_capable;
    /* for avoiding charpath with pdfwrite */
    bool preserve_tr_mode;
    /* Are SMask's preserved by device (pdfwrite) */
    bool preserve_smask;
    bool ForOPDFRead;
    bool pdfmark;
    bool HighLevelDevice;
    /* These are derived from the device parameters rather than extracted from the device */
    /* But this is a convenient place to keep them. */
    /* Does current output device handle pdfmark */
    bool writepdfmarks;
    /* Should annotations be preserved or marked for current output device? */
    bool annotations_preserved;
    /* Should we pass on PageLabels (using a device param, not a pdfmark) */
    bool WantsPageLabels;
    /* Is the device capable of handling optional content */
    bool WantsOptionalContent;
    bool PassUserUnit;
    bool ModifiesPageSize;
    bool ModifiesPageOrder;
} device_state_t;

/*
 * resource_paths: for CMaps, iccprofiles, fonts... mainly build time settings and from
 * "-I" command line options.
 * font_paths: Specific to fonts: from the -sFONTPATH=<> option
 * We keep a running count (num_resource_paths) of all, and a one off count of paths that
 * came from the build (num_init_resource_paths) so we can keep the (weird) search order
 * that gs uses.
 */
typedef struct search_paths_s
{
    gs_param_string *resource_paths;
    int             num_resource_paths; /* total */
    int             num_init_resource_paths; /* number of paths that came from the build */
    gs_param_string *font_paths;
    int             num_font_paths;
    gs_param_string genericresourcedir;
    bool search_here_first;
} search_paths_t;

typedef struct pdf_context_s
{
    pdf_obj_common;
    void *instance;
    gs_memory_t *memory;

    /* command line argument storage */
    cmd_args_t args;

    /* Encryption state */
    encryption_state_t encryption;

    /* Text and text state parameters */
    text_state_t text;

    /* The state of the current page being processed */
    page_state_t page;

    device_state_t device_state;


    /* PDF interpreter state */

    /* State for handling the wacky W and W* operators */
    bool clip_active;
    bool do_eoclip;

    /* Doing a high level form for pdfwrite (annotations) */
    bool PreservePDFForm;
    /* If processing multiple files, the number of pages to add to /Page Destinations
     * when handling Outlines and Annotations. This is the number of pages in all
     * files completely processed so far.
     */
    int Pdfmark_InitialPage;

    /* Optional things from Root */
    pdf_dict *OCProperties;
    pdf_dict *Collection;

    /* Optional/Marked Content stuff */
    void *OFFlevels;
    uint64_t BMClevel;
    bool BDCWasOC;

    /* Bitfields recording whether any errors or warnings were encountered */
    char pdf_errors[PDF_ERROR_BYTE_SIZE];
    char pdf_warnings[PDF_WARNING_BYTE_SIZE];

    /* We need a gs_font_dir for gs_definefotn() */
    gs_font_dir * font_dir;
    /* Obviously we need a graphics state */
    gs_gstate *pgs;

    /* PDF really doesn't have a path in the graphics state. This is different to
     * PostScript and has implications; changing the CTM partway through path
     * construction affects path segments already accumulated. The path is
     * unaffected by gsvae and grestore. Previously we've unwound any pending
     * path and rerun it, this is causing problems so instead we'll do what
     * Acrobat obviously does and build the path outside the graphics state
     */
    /* We make allocations in chunks for the path to avoid lots of little
     * allocations, but we need to know where the end of the current allocation
     * is so that we can tell if we would overflow and increase it.
     */
    char *PathSegments;
    /* The current insertion point. */
    char *PathSegmentsCurrent;
    /* The current limit of the block */
    char *PathSegmentsTop;
    double *PathPts;
    double *PathPtsCurrent;
    double *PathPtsTop;

    /* set up by pdf_impl_set_device, this is the 'high water mark' for
     * restoring back to when we close a PDF file. This ensures the device
     * is correctly set up for any subesquent file to be run.
     */
    int job_gstate_level;
    /* This is currently used for Patterns, but I suspect needs to be changed to use
     * 'the enclosing context'
     */
    gs_gstate *DefaultQState;


    /* The input PDF filename and the stream for it */
    char *filename;
    pdf_c_stream *main_stream;

    /* Length of the main file */
    gs_offset_t main_stream_length;
    /* offset to the xref table */
    gs_offset_t startxref;

    /* Track whether file is a hybrid. Initially prefer XRefStm but
     * if we fail to read the structure using an XRefStm, try again
     * using the xref
     */
    bool prefer_xrefstm;
    bool is_hybrid;
    /* If we've already repaired the file once, and it still fails, don't try to repair it again */
    bool repaired;
    /* Repairing is true while the repair code is running, during this we ignore errors and warnings */
    bool repairing;

    /* The HeaderVersion is the declared version from the PDF header, but this
     * can be overridden by later trailer dictionaries, so the FinalVersion is
     * the version as finally read from the file. Note we don't currently use
     * these for anything, we might in future emit warnings if PDF files use features
     * inconsistent with the FinalVersion.
     */
    float HeaderVersion, FinalVersion;

    /* Document level PDF objects */
    xref_table_t *xref_table;
    /* Warning! Do not use ctx->Trailer directly as it may be replaced if the file is repaired.
     * See pdf_doc.c, pdf_read_Root()
     */
    pdf_dict *Trailer;
    pdf_dict *Root;
    pdf_dict *Info;
    pdf_dict *PagesTree;
    uint64_t num_pages;
    uint32_t *page_array; /* cache of page dict object_num's for pdfmark Dest */
    pdf_dict *AcroForm;
    bool NeedAppearances; /* From AcroForm, if any */

    /* The interpreter operand stack */
    uint32_t stack_size;
    pdf_obj **stack_bot;
    pdf_obj **stack_top;
    pdf_obj **stack_limit;

    /* The object cache */
    uint32_t cache_entries;
    pdf_obj_cache_entry *cache_LRU;
    pdf_obj_cache_entry *cache_MRU;

    /* The loop detection state */
    uint32_t loop_detection_size;
    uint32_t loop_detection_entries;
    uint64_t *loop_detection;

    /* A counter for nesting of arrays and dictionaries. We don't want to allow this
     * to be unbounded, because on exit we could end up exceeding the C execution stack
     * if we get too deeply nested.
     */
    uint32_t object_nesting;

    /* Used to set the 'parent' stream of a stream that gets created by dereferencing
     * We should not need this but badly fromed PDF files can use Resources defined in
     * an earlier (non-Page) stream object, and Acrobat handles this, so we need to.
     * We could haev done this more neatly if we'd known this during design :-(
     */
    pdf_stream *current_stream;
    stream_save current_stream_save;

    /* A name table :-( */
    pdfi_name_entry_t *name_table;

    gs_string *fontmapfiles;
    int num_fontmapfiles;

    search_paths_t search_paths;
    pdf_dict *pdffontmap;
    pdf_dict *pdfnativefontmap; /* Explicit mappings take precedence, hence we need separate dictionaries */
    pdf_dict *pdf_substitute_fonts;
    pdf_dict *pdfcidfmap;
    resource_font_cache_t *resource_font_cache;
    uint32_t resource_font_cache_size;

    gx_device *devbbox; /* Cached for use in pdfi_string_bbox */
    /* These function pointers can be replaced by ones intended to replicate
     * PostScript functionality when running inside the Ghostscript PostScript
     * interpreter.
     */
    int (*finish_page) (struct pdf_context_s *ctx);
    int (*get_glyph_name)(gs_font *font, gs_glyph index, gs_const_string *pstr);
    int (*get_glyph_index)(gs_font *font, byte *str, uint size, uint *glyph);

#if REFCNT_DEBUG
    uint64_t ref_UID;
#endif
#if CACHE_STATISTICS
    uint64_t hits;
    uint64_t misses;
    uint64_t compressed_hits;
    uint64_t compressed_misses;
#endif
#if PDFI_LEAK_CHECK
    gs_memory_status_t memstat;
#endif
}pdf_context;

#define OBJ_CTX(o) ((pdf_context *)(o->ctx))
#define OBJ_MEMORY(o) OBJ_CTX(o)->memory

int pdfi_add_paths_to_search_paths(pdf_context *ctx, const char *ppath, int l, bool fontpath);
int pdfi_add_initial_paths_to_search_paths(pdf_context *ctx, const char *ppath, int l);
int pdfi_add_fontmapfiles(pdf_context *ctx, const char *ppath, int l);

pdf_context *pdfi_create_context(gs_memory_t *pmem);
int pdfi_clear_context(pdf_context *ctx);
int pdfi_free_context(pdf_context *ctx);

int pdfi_get_name_index(pdf_context *ctx, char *name, int len, unsigned int *returned);
int pdfi_name_from_index(pdf_context *ctx, int index, unsigned char **name, unsigned int *len);
int pdfi_separation_name_from_index(gs_gstate *pgs, gs_separation_name index, unsigned char **name, unsigned int *len);
int pdfi_open_pdf_file(pdf_context *ctx, char *filename);
int pdfi_set_input_stream(pdf_context *ctx, stream *stm);
int pdfi_process_pdf_file(pdf_context *ctx, char *filename);
int pdfi_prep_collection(pdf_context *ctx, uint64_t *TotalFiles, char ***names_array);
int pdfi_finish_pdf_file(pdf_context *ctx);
int pdfi_close_pdf_file(pdf_context *ctx);
int pdfi_gstate_from_PS(pdf_context *ctx, gs_gstate *pgs, pdfi_switch_t *i_switch, gsicc_profile_cache_t *profile_cache);
void pdfi_gstate_to_PS(pdf_context *ctx, gs_gstate *pgs, pdfi_switch_t *i_switch);
int pdfi_output_page_info(pdf_context *ctx, uint64_t page_num);

void pdfi_report_errors(pdf_context *ctx);
void pdfi_verbose_error(pdf_context *ctx, int gs_error, const char *gs_lib_function, int pdfi_error, const char *pdfi_function_name, const char *extra_info, const char *file_line);
void pdfi_verbose_warning(pdf_context *ctx, int gs_error, const char *gs_lib_function, int pdfi_warning, const char *pdfi_function_name, const char *extra_info, const char *file_line);

static inline void pdfi_set_error_file_line(pdf_context *ctx, int gs_error, const char *gs_lib_function, pdf_error pdfi_error, const char *pdfi_function_name, const char *extra_info, const char *file_line)
{
    /* ignore problems while repairing a file */
    if (!ctx->repairing) {
        if (pdfi_error != 0)
            ctx->pdf_errors[pdfi_error / (sizeof(char) * 8)] |= 1 << pdfi_error % (sizeof(char) * 8);
        if (ctx->args.verbose_errors)
            pdfi_verbose_error(ctx, gs_error, gs_lib_function, pdfi_error, pdfi_function_name, extra_info, file_line);
    }
}

static inline int pdfi_set_error_stop_file_line(pdf_context *ctx, int gs_error, const char *gs_lib_function, pdf_error pdfi_error, const char *pdfi_function_name, const char *extra_info, const char *file_line)
{
    pdfi_set_error_file_line(ctx, gs_error, gs_lib_function, pdfi_error, pdfi_function_name, extra_info, file_line);
    if (ctx->args.pdfstoponerror || gs_error == gs_error_Fatal || gs_error == gs_error_VMerror) {
        if (gs_error < 0)
            return gs_error;
        else
            return gs_error_unknownerror;
    }
    return 0;
}

static inline void pdfi_set_warning_file_line(pdf_context *ctx, int gs_error, const char *gs_lib_function, pdf_warning pdfi_warning, const char *pdfi_function_name, const char *extra_info, const char *file_line)
{
    /* ignore problems while repairing a file */
    if (!ctx->repairing) {
        ctx->pdf_warnings[pdfi_warning / (sizeof(char) * 8)] |= 1 << pdfi_warning % (sizeof(char) * 8);
        if (ctx->args.verbose_warnings)
            pdfi_verbose_warning(ctx, gs_error, gs_lib_function, pdfi_warning, pdfi_function_name, extra_info, file_line);
    }
}

static inline int pdfi_set_warning_stop_file_line(pdf_context *ctx, int gs_error, const char *gs_lib_function, pdf_warning pdfi_warning, const char *pdfi_function_name, const char *extra_info, const char *file_line)
{
    pdfi_set_warning_file_line(ctx, gs_error, gs_lib_function, pdfi_warning, pdfi_function_name, extra_info, file_line);
    if (ctx->args.pdfstoponwarning || gs_error == gs_error_Fatal || gs_error == gs_error_VMerror) {
        if (gs_error < 0)
            return gs_error;
        else
            return gs_error_unknownerror;
    }
    return 0;
}

static inline void pdfi_log_info_file_line(pdf_context *ctx, const char *pdfi_function, const char *info)
{
    if (!ctx->args.QUIET)
        outprintf(ctx->memory, "%s", info);
}

#if defined(DEBUG) && defined(__FILE__) && defined(__LINE__)
#define DEBUG_FILE_LINE 1
#define pdfi_log_info(ctx, pdfi_function, info) pdfi_log_info_file_line(ctx, pdfi_function, info)
#define pdfi_set_warning_stop(ctx, gs_error, gs_lib_function, pdfi_warning, pdfi_function_name, extra_info) pdfi_set_warning_stop_file_line(ctx, gs_error, gs_lib_function, pdfi_warning, pdfi_function_name, extra_info, __FILE__"("GS_STRINGIZE(__LINE__)")")
#define pdfi_set_warning(ctx, gs_error, gs_lib_function, pdfi_warning, pdfi_function_name, extra_info) pdfi_set_warning_file_line(ctx, gs_error, gs_lib_function, pdfi_warning, pdfi_function_name, extra_info, __FILE__"("GS_STRINGIZE(__LINE__)")")
#define pdfi_set_error_stop(ctx, gs_error, gs_lib_function, pdfi_warning, pdfi_function_name, extra_info) pdfi_set_error_stop_file_line(ctx, gs_error, gs_lib_function, pdfi_warning, pdfi_function_name, extra_info, __FILE__"("GS_STRINGIZE(__LINE__)")")
#define pdfi_set_error(ctx, gs_error, gs_lib_function, pdfi_warning, pdfi_function_name, extra_info) pdfi_set_error_file_line(ctx, gs_error, gs_lib_function, pdfi_warning, pdfi_function_name, extra_info, __FILE__"("GS_STRINGIZE(__LINE__)")")
#else
#define DEBUG_FILE_LINE 0
#define pdfi_log_info(ctx, pdfi_function, info) pdfi_log_info_file_line(ctx, pdfi_function, info)
#define pdfi_set_warning_stop(ctx, gs_error, gs_lib_function, pdfi_warning, pdfi_function_name, extra_info) pdfi_set_warning_stop_file_line(ctx, gs_error, gs_lib_function, pdfi_warning, pdfi_function_name, extra_info, NULL)
#define pdfi_set_warning(ctx, gs_error, gs_lib_function, pdfi_warning, pdfi_function_name, extra_info) pdfi_set_warning_file_line(ctx, gs_error, gs_lib_function, pdfi_warning, pdfi_function_name, extra_info, NULL)
#define pdfi_set_error_stop(ctx, gs_error, gs_lib_function, pdfi_warning, pdfi_function_name, extra_info) pdfi_set_error_stop_file_line(ctx, gs_error, gs_lib_function, pdfi_warning, pdfi_function_name, extra_info, NULL)
#define pdfi_set_error(ctx, gs_error, gs_lib_function, pdfi_warning, pdfi_function_name, extra_info) pdfi_set_error_file_line(ctx, gs_error, gs_lib_function, pdfi_warning, pdfi_function_name, extra_info, NULL)
#endif

/* Variants of the above that work in a printf style. */
int pdfi_set_error_var(pdf_context *ctx, int gs_error, const char *gs_lib_function, pdf_error pdfi_error, const char *pdfi_function_name, const char *fmt, ...);
int pdfi_set_warning_var(pdf_context *ctx, int gs_error, const char *gs_lib_function, pdf_warning pdfi_warning, const char *pdfi_function_name, const char *fmt, ...);

#define PURGE_CACHE_PER_PAGE 0

#if PURGE_CACHE_PER_PAGE
void pdfi_purge_obj_cache(pdf_context *ctx);
#endif

#endif
