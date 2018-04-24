/* Copyright (C) 2001-2018 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
*/


/* combined internal header for the XPS interpreter */

#include "memory_.h"
#include "math_.h"
#include "string_.h"
#include <stdlib.h>
#include <ctype.h> /* for toupper() */

/* Include zlib early to avoid offsetof redef problems on windows */
#include "zlib.h"

#include "stdint_.h"

#include "gp.h"

#include "gsgc.h"
#include "gstypes.h"
#include "gsstate.h"
#include "gsmatrix.h"
#include "gscoord.h"
#include "gsmemory.h"
#include "gsparam.h"
#include "gsdevice.h"
#include "scommon.h"
#include "gdebug.h"
#include "gserrors.h"
#include "gspaint.h"
#include "gspath.h"
#include "gsimage.h"
#include "gscspace.h"
#include "gsptype1.h"
#include "gscolor2.h"
#include "gscolor3.h"
#include "gsutil.h"
#include "gsicc.h"

#include "gstrans.h"

#include "gxpath.h"     /* gsshade.h depends on it */
#include "gxfixed.h"    /* gsshade.h depends on it */
#include "gxmatrix.h"   /* gxtype1.h depends on it */
#include "gsshade.h"
#include "gsfunc.h"
#include "gsfunc3.h"    /* we use stitching and exponential interp */

#include "gxfont.h"
#include "gxchar.h"
#include "gxcolor2.h" /* Required for definition of gs_pattern1_instance_t */
#include "gxtype1.h"
#include "gxfont1.h"
#include "gxfont42.h"
#include "gxfcache.h"
#include "gxgstate.h"

#include "gzstate.h"
#include "gzpath.h"
#include "gzcpath.h"

#include "gsicc_manage.h"
#include "gscms.h"
#include "gsicc_cache.h"
#include "gxpcolor.h"

#include "gxdevsop.h"       /* For special ops */

#include "pdf_types.h"

#ifndef PDF_CONTEXT
#define PDF_CONTEXT
/*
 * The interpreter context.
 */

typedef enum pdf_error_flag_e {
    E_PDF_NOERROR = 0,
    E_PDF_NOHEADER = 1,
    E_PDF_NOHEADER_VERSION = 2,
    E_PDF_NOSTARTXREF = 4,
    E_PDF_BAD_XREF = 8,
    E_PDF_SHORT_XREF = 16,
} pdf_error_flag;

#define INITIAL_STACK_SIZE 32
#define MAX_STACK_SIZE 32767
#define MAX_OBJECT_CACHE_SIZE 200
#define INITIAL_LOOP_TRACKER_SIZE 32

typedef struct pdf_context_s
{
    void *instance;
    gs_memory_t *memory;

    int first_page;             /* -dFirstPage= */
    int last_page;              /* -dLastPage= */
    bool pdfdebug;
    bool pdfstoponerror;
    bool pdfstoponwarning;
    bool notransparency;
    bool nocidfallback;
    bool no_pdfmark_outlines;
    bool no_pdfmark_dests;
    bool pdffitpage;
    bool usecropbox;
    bool useartbox;
    bool usebleedbox;
    bool usetrimbox;
    bool printed;
    bool showacroform;
    bool showannots;
    bool nouserunit;
    bool renderttnotdef;
    char *PDFPassword;
    char *PageList;

    float HeaderVersion, FinalVersion;

    gs_gstate *pgs;
    gs_font_dir *fontdir;
    int preserve_tr_mode; /* for avoiding charpath with pdfwrite */

    gs_color_space *gray_lin; /* needed for transparency */
    gs_color_space *gray;
    gs_color_space *srgb;
    gs_color_space *scrgb;
    gs_color_space *cmyk;

    char *directory;
    pdf_stream *main_stream;
    gs_offset_t main_stream_length;

    gs_offset_t startxref;

    /* Global toggle for transparency */
    bool use_transparency;
    bool has_transparency;

    xref_table *xref_table;
    pdf_dict *Trailer;
    pdf_dict *Root;
    pdf_dict *Info;
    pdf_dict *Pages;
    uint64_t num_pages;

    uint32_t stack_size;
    pdf_obj **stack_bot;
    pdf_obj **stack_top;
    pdf_obj **stack_limit;

    uint32_t cache_entries;
    pdf_obj_cache_entry *cache_LRU;
    pdf_obj_cache_entry *cache_MRU;

    uint32_t loop_detection_size;
    uint32_t loop_detection_entries;
    uint64_t *loop_detection;
}pdf_context;

pdf_context *pdf_create_context(gs_memory_t *pmem);
int pdf_free_context(gs_memory_t *pmem, pdf_context *ctx);

int pdf_open_pdf_file(pdf_context *ctx, char *filename);
int pdf_process_pdf_file(pdf_context *ctx, char *filename);
int pdf_close_pdf_file(pdf_context *ctx);
#endif
