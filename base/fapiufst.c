/* Copyright (C) 2001-2025 Artifex Software, Inc.
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


/* Agfa UFST plugin */

/* GS includes : */
#include "stdio_.h"
#include "string_.h"
#include "stream.h"             /* for files.h */
#include "strmio.h"

#include "memory_.h"
#include "gsmemory.h"
#include "math_.h"
#include "stat_.h"              /* include before definition of esp macro, bug 691123 */
#include "gserrors.h"

#include "gp.h"

#include "gxdevice.h"
#include "gxfont.h"
#include "gxfont1.h"
#include "gxchar.h"
#include "gxpath.h"
#include "gxfcache.h"
#include "gxchrout.h"
#include "gximask.h"
#include "gscoord.h"
#include "gspaint.h"
#include "gsfont.h"
#include "gspath.h"

#include "gxfapi.h"

#include "gzstate.h"

#include "gscrypt1.h"
#include "gxfcid.h"
#include "gsstype.h"
#include "gxchar.h"             /* for st_gs_show_enum */

#include "gsmalloc.h"
#include "gsmchunk.h"
#include "gdebug.h"             /* for gs_debug_c() */


#undef frac_bits

#ifndef _MSC_VER
#define _MSC_VER 0
#endif

/* UFST includes : */
#include "cgconfig.h"
#include "ufstport.h"
#include "dbg_ufst.h"
#include "shareinc.h"
#include "t1isfnt.h"
#include "cgmacros.h"
#include "sfntenum.h"
#define DOES_ANYONE_USE_THIS_STRUCTURE  /* see TTPCLEO.H, UFST 4.2 */
#include "ttpcleo.h"
#undef  DOES_ANYONE_USE_THIS_STRUCTURE

#include "cgmacros.h"           /* for SWAPW() and SWAPL() */

#include "gxfapiu.h"

#if (UFST_VERSION_MAJOR >= 6 && UFST_VERSION_MINOR >= 2) || UFST_VERSION_MAJOR >= 7
#include "t1itype1.h"
#endif

#define ufst_emprintf(m,s) { outflush(m); emprintf(m, s); outflush(m); }
#define ufst_emprintf1(m,s,d) { outflush(m); emprintf1(m, s, d); outflush(m); }

#if (UFST_VERSION_MAJOR >= 6 && UFST_VERSION_MINOR >= 2) || UFST_VERSION_MAJOR >= 7
#undef true
#undef false

#define false FALSE
#define true TRUE
#define UNICODE UFST_UNICODE
#endif

/* Prior to 6.2, prototypes had empty parameter lists, causing problems
 * when passing 64 bit pointers into the UFST functions.
 * Prototype "properly" here.
 */
#if UFST_VERSION_MAJOR < 6
#if defined (ANSI_DEFS)
UW16 CGENTRY CGIFFfont(FSP PFONTCONTEXT fc);
#else
UW16 CGENTRY CGIFFfont(fc);
PFONTCONTEXT fc;
#endif /* ANSI_DEFS */
#endif

typedef struct fapi_ufst_server_s fapi_ufst_server;

static unsigned int font_ids = 0x7fffffff;

#if UFST_REENTRANT
#define FSA_FROM_SERVER IF_STATE *pIFS = &r->IFS
#else
EXTERN IF_STATE if_state;

#define FSA_FROM_SERVER IF_STATE *pIFS = &if_state;    (void)r; (void)pIFS;
static fapi_ufst_server *static_server_ptr_for_ufst_callback = 0;
#endif

#if PCLEO_RDR  &&  BYTEORDER==LOHI
GLOBAL UW16 PCLswapHdr(FSP LPUB8 p, UW16 gifct);        /* UFST header doesn't define it. */
#define UFST_PCLswapHdr(p, gifct) PCLswapHdr(FSP p, gifct)
#else
#define UFST_PCLswapHdr(p, gifct)
#endif

typedef struct pcleo_glyph_list_elem_s pcleo_glyph_list_elem;
struct pcleo_glyph_list_elem_s
{
    UW16 chId;
    pcleo_glyph_list_elem *next;
    /* more data follows here depending on font type */
};

typedef struct
{
    SL32 font_id;
    uint tt_font_body_offset;
    UW16 is_disk_font;
    UW16 font_type;
    UW16 platformId;
    UW16 specificId;
    pcleo_glyph_list_elem *glyphs;
    char decodingID[40];
    TTFONTINFOTYPE *ufstfontinfo;
    UW16 ufstfontinfosize;
} ufst_common_font_data;

typedef struct
{
    PCLETTO_CHR_HDR h;
    UW16 add_data;
    UW16 charDataSize;
    UW16 glyphID;
} PCLETTO_CHDR;

struct fapi_ufst_server_s
{
    gs_fapi_server If;
    int bInitialized;
    gs_fapi_font *ff;
    gs_memory_t *mem;
    byte *param;
    int param_size;
    IF_STATE IFS;
    FONTCONTEXT fc;
    void *char_data;
    bool bRaster;
    bool ufst_is_singleton;
    double tran_xx, tran_xy, tran_yx, tran_yy;
    fco_list_elem *fco_list;
    gs_fapi_retcode callback_error;
    gs_fapi_metrics_type metrics_type;
    fracint sb_x, aw_x;         /* replaced PS metrics. */
};

static inline void gs_fapi_ufst_release_char_data_inline(fapi_ufst_server * r);
static void release_glyphs(fapi_ufst_server * r, ufst_common_font_data * d);

/* Type casts : */

static inline fapi_ufst_server *
If_to_I(gs_fapi_server * If)
{
    return (fapi_ufst_server *) If;
}

static inline fapi_ufst_server *
IFS_to_I(IF_STATE * pIFS)
{
    return (fapi_ufst_server *) ((char *)pIFS -
                                 offset_of(fapi_ufst_server, IFS));
}

/* assign font_id not based on fchandle value, nor read from font */
static inline int
assign_font_id(void)
{
    /* We're only allowed 5 open FCOs at one time
     * so make sure we don't clash with those
     */
    if ((--font_ids) <= (5 << 16)) {
        /* When we clash with the expected values for FCO's
         * wrap around. No will ever have enough concurrent
         * fonts for this to be a problem!
         */
        font_ids = 0x7fffffff;
    }
    return (font_ids);
}

/*------------------ gs_fapi_server members ------------------------------------*/

static inline void
gs_fapi_ufst_release_char_data_inline(fapi_ufst_server * r)
{                               /*  The server keeps character raster between calls to gs_fapi_ufst_get_char_raster_metrics
                                   and gs_fapi_ufst_get_char_raster, as well as between calls to gs_fapi_ufst_get_char_outline_metrics
                                   and gs_fapi_ufst_get_char_outline. Meanwhile this regular
                                   sequence of calls may be interrupted by an error in CDefProc or setchachedevice2,
                                   which may be invoked between them. In this case Ghostscript
                                   is unable to provide a signal to FAPI that the data are not
                                   longer needed. This would cause memory leaks in UFST heap.
                                   To work around this, appropriate server's entries check whether
                                   raster data were left after a previous call, and ultimately release them.
                                   This function provides it.
                                 */
    if (r->char_data != NULL) {
        FSA_FROM_SERVER;

        CHARfree(FSA(MEM_HANDLE) r->char_data);
        r->char_data = 0;
    }
}

/*
 * In the language switch build (which we detect because PSI_INCLUDED
 * is defined), we use the gx_UFST_init() call to initialize the UFST,
 * rather than calling into the UFST directly. That has the advantage
 * that the same UFST configuration is used for PCL and PS, but the
 * disadvantage that the dynamic parameters set in server_param are
 * not available. Thus, it is switched through this compile time option.
*/
static gs_fapi_retcode
open_UFST(fapi_ufst_server * r, const byte * server_param,
          int server_param_size)
{
    int code;
    SW16 fcHandle;
    int l;
    char ufst_root_dir[1024] = "";
    char sPlugIn[1024] = "";
#if !NO_SYMSET_MAPPING
    bool bSSdir = FALSE;
#endif
    bool bPlugIn = FALSE;
    const char *keySSdir = "UFST_SSdir=";
    const int keySSdir_length = strlen(keySSdir);
    const char *keyPlugIn = "UFST_PlugIn=";
    const int keyPlugIn_length = strlen(keyPlugIn);
    const char sep = gp_file_name_list_separator;
    const byte *p = server_param, *e = server_param + server_param_size, *q;

    FSA_FROM_SERVER;

    if (server_param_size > 0 && server_param) {
        r->param =
            (byte *) gs_malloc(r->mem, server_param_size, 1, "server_params");
        if (!r->param) {
            return_error(gs_error_VMerror);
        }
        memcpy(r->param, server_param, server_param_size);
        r->param_size = server_param_size;
    }

    for (; p < e; p = q + 1) {
        for (q = p; q < e && *q != sep; q++)
            /* DO_NOTHING */ ;
        l = q - p;
        if (l > keySSdir_length && !memcmp(p, keySSdir, keySSdir_length)) {
            l = q - p - keySSdir_length;
            if (l > sizeof(ufst_root_dir) - 1)
                l = sizeof(ufst_root_dir) - 1;
            memcpy(ufst_root_dir, p + keySSdir_length, l);
            ufst_root_dir[l] = 0;
#if !NO_SYMSET_MAPPING
            bSSdir = TRUE;
#endif
        }
        else if (l > keyPlugIn_length
                 && !memcmp(p, keyPlugIn, keyPlugIn_length)) {
            l = q - p - keyPlugIn_length;
            if (l > sizeof(sPlugIn) - 1)
                l = sizeof(sPlugIn) - 1;
            memcpy(sPlugIn, p + keyPlugIn_length, l);
            sPlugIn[l] = 0;
            bPlugIn = TRUE;
        }
        else if (gs_debug_c('1'))
            ufst_emprintf(r->mem, "Warning: Unknown UFST parameter ignored.\n");
    }
#if !NO_SYMSET_MAPPING
    if (!bSSdir) {
        strcpy(ufst_root_dir, ".");
        if (gs_debug_c('1'))
            ufst_emprintf(r->mem,
                     "Warning: UFST_SSdir is not specified, will search *.ss files in the curent directory.\n");
    }
#endif
    code = gx_UFST_init(r->mem, (const UB8 *)ufst_root_dir);
    if (code < 0)
        return code;
    r->ufst_is_singleton = 0;
    CGIFfont_access(FSA DISK_ACCESS);
    if (bPlugIn) {
        if ((code = gx_UFST_open_static_fco(sPlugIn, &fcHandle)) != 0)
            return code;
        if ((code = CGIFfco_Plugin(FSA fcHandle)) != 0)
            return code;
    }
    else {
#ifdef FCO_RDR
        ufst_emprintf(r->mem,
                 "Warning: UFST_PlugIn is not specified, some characters may be missing.\n");
#endif
    }
    return 0;
}

static LPUB8 impl_PCLchId2ptr(FSP UW16 chId);

static gs_fapi_retcode
gs_fapi_ufst_ensure_open(gs_fapi_server *server, const char *server_param, int server_param_size)
{
    fapi_ufst_server *r = If_to_I(server);
    int code;

    if (r->bInitialized)
        return 0;
    r->bInitialized = 1;
    {
        code = open_UFST(r, (byte *)server_param, server_param_size);
        if (code < 0) {
            ufst_emprintf(r->mem, "Error opening the UFST font server.\n");
            return code;
        }
    }
    gx_set_UFST_Callbacks(NULL, impl_PCLchId2ptr, impl_PCLchId2ptr);
    return 0;
}

static UW16
get_font_type(stream * f)
{
    char buf[20], mark_PS[] = "%!";
    int i;

    if (sfread(buf, 1, sizeof(buf), f) != sizeof(buf))
        return 0;
    if (buf[0] == 0x15 || buf[0] == 0x00)       /* fixme : don't know how to do correctly. */
        return FC_FCO_TYPE;
    for (i = 0; i < sizeof(buf) - sizeof(mark_PS); i++)
        if (!memcmp(buf + i, mark_PS, sizeof(mark_PS) - 1))
            return FC_PST1_TYPE;
    if (buf[0] == '\0' && buf[1] == '\1')
        return FC_TT_TYPE;
    if (buf[0] == 't' && buf[1] == 't')
        return FC_TT_TYPE;
    return 0;                   /* fixme : unknown type - actually an error. */
}

static int
choose_decoding_general(fapi_ufst_server * r, ufst_common_font_data * d,
                        const char *cmapId)
{
    if (!d->decodingID[0])
        strncpy(d->decodingID, "Unicode", sizeof(d->decodingID));
    /*    fixme : must depend on charset used in the font.  */
    return 1;
}

static int
choose_decoding_TT(fapi_ufst_server * r, ufst_common_font_data * d,
                   const char *cmapId)
{
#if TT_ROM || TT_DISK
    int platId, specId, i;
    CMAP_QUERY q;
    UW16 font_access;
    bool failed;
    void *p =
        (d->is_disk_font ? (void *)(d + 1) : (void *)((UB8 *) d +
                                                      d->
                                                      tt_font_body_offset));
    FSA_FROM_SERVER;

    if (sscanf(cmapId, "%d.%d", &platId, &specId) != 2)
        return 0;
    font_access = pIFS->font_access;
    pIFS->font_access = (d->is_disk_font ? DISK_ACCESS : ROM_ACCESS);
    failed = CGIFtt_cmap_query(FSA p, r->fc.ttc_index, &q);
    pIFS->font_access = font_access;
    if (failed)
        return 0;
    for (i = 0; i < q.numCmap; i++)
        if (q.entry[i].platId == platId && q.entry[i].specId == specId) {
            d->platformId = platId;
            d->specificId = specId;
            return 1;
        }
    return 0;
#else
    if (!d->decodingID[0])
        strncpy(d->decodingID, "Unicode", sizeof(d->decodingID));
    return 1;
#endif
}

static void
scan_xlatmap(fapi_ufst_server * r, ufst_common_font_data * d,
             const char *xlatmap, const char *font_kind,
             int (*choose_proc) (fapi_ufst_server * r,
                                 ufst_common_font_data * d,
                                 const char *cmapId))
{
    const char *p = xlatmap;

    while (*p) {
        int good_kind = !strcmp(p, font_kind);

        p += strlen(p) + 2;
        while (*p) {
            const char *cmapId = p, *decodingID = p + strlen(p) + 1;

            strncpy(d->decodingID, decodingID, sizeof(d->decodingID));
            if (!decodingID[0])
                break;
            p = decodingID + strlen(decodingID) + 1;
            if (good_kind)
                if (choose_proc(r, d, cmapId))
                    return;
        }
    }
    d->decodingID[0] = 0;
}

static void
choose_decoding(fapi_ufst_server * r, ufst_common_font_data * d,
                const char *xlatmap)
{
    if (xlatmap != 0)
        switch (d->font_type) {
            case FC_IF_TYPE:   /* fixme */
                break;
            case FC_PST1_TYPE:
                scan_xlatmap(r, d, xlatmap, "PostScript",
                             choose_decoding_general);
                break;
            case FC_TT_TYPE:
                scan_xlatmap(r, d, xlatmap, "TrueType", choose_decoding_TT);
                break;
            case FC_FCO_TYPE:
                scan_xlatmap(r, d, xlatmap, "Microtype",
                             choose_decoding_general);
                break;
        }
}

static inline void
store_word(byte ** p, ushort w)
{
    *((*p)++) = w / 256;
    *((*p)++) = w % 256;

}

static LPUB8
get_TT_glyph(fapi_ufst_server * r, gs_fapi_font * ff, UW16 chId)
{
    pcleo_glyph_list_elem *g;
    PCLETTO_CHDR *h;
    ufst_common_font_data *d = (ufst_common_font_data *) r->fc.font_hdr - 1;
    LPUB8 q;
    ushort glyph_length = ff->get_glyph(ff, chId, 0, 0);
    bool use_XL_format = ff->is_mtx_skipped;

    /*
     * The client must set ff->is_mtx_skipped iff
     * it requests a replaced lsb for True Type.
     * If it is set, a replaced width to be supplied.
     * This constraint is derived from UFST restriction :
     * the font header format must be compatible with
     * glyph header format.
     */

    if (glyph_length == (ushort) - 1) {
        r->callback_error = gs_error_invalidfont;
        return 0;
    }
    g = (pcleo_glyph_list_elem *) gs_malloc(r->mem,
                                            sizeof(pcleo_glyph_list_elem) +
                                            (use_XL_format ? 12 :
                                             sizeof(PCLETTO_CHDR)) +
                                            glyph_length + 2, 1,
                                            "PCLETTO char");
    if (g == 0) {
        r->callback_error = gs_error_VMerror;
        return 0;
    }
    g->chId = chId;
    g->next = d->glyphs;
    d->glyphs = g;
    h = (PCLETTO_CHDR *) (g + 1);
    h->h.format = 15;
    if (use_XL_format) {
        h->h.continuation = 2;
        q = (LPUB8) h + 2;
        store_word(&q, (ushort) (glyph_length + 10));
        store_word(&q, (ushort) (r->sb_x >> r->If.frac_shift)); /* see gs_fapi_ufst_can_replace_metrics */
        store_word(&q, (ushort) (r->aw_x >> r->If.frac_shift));
        store_word(&q, 0);
        store_word(&q, chId);
    }
    else {
        h->h.continuation = 0;
        h->h.descriptorsize = 4;
        h->h.ch_class = 15;
        h->add_data = 0;
        q = (LPUB8) & h->charDataSize;
        store_word(&q, (ushort) (glyph_length + 4));
        store_word(&q, chId);
    }
    if (ff->get_glyph(ff, chId, (LPUB8) q, glyph_length) == (ushort) - 1) {
        r->callback_error = gs_error_invalidfont;
        return 0;
    }
    q += glyph_length;
    store_word(&q, 0);          /* checksum */
    return (LPUB8) h;
    /*
     * The metrics replacement here is done only for the case
     * corresponding to non-disk TT fonts with MetricsCount != 0;
     * Other cases are not supported because UFST cannot handle them.
     * Here we don't take care of cases which gs_fapi_ufst_can_replace_metrics rejects.
     *
     * We don't care of metrics for subglyphs, because
     * it is ignored by TT interpreter.
     */
}

static LPUB8
get_T1_glyph(fapi_ufst_server * r, gs_fapi_font * ff, UW16 chId)
{
#if PST1_SFNTI
#if UFST_VERSION_MAJOR >= 6 && UFST_VERSION_MINOR >= 2
    ushort glyph_length = ff->get_glyph(ff, chId, 0, 0);
    CDATASTR charstring;
    PCDATASTR_DEMO pcharstring;
    byte *cstring;
    ufst_common_font_data *d = (ufst_common_font_data *) r->fc.font_hdr - 1;
    MEM_HANDLE hndl;

    FSA_FROM_SERVER;

    d->glyphs = NULL;

    charstring.len = glyph_length;
    charstring.hdata = BUFalloc(charstring.len);
    if (!charstring.hdata) {
        r->callback_error = gs_error_VMerror;
        return (NULL);
    }

    cstring = (unsigned char *)MEMptr(charstring.hdata);

    if (ff->get_glyph(ff, chId, cstring, glyph_length) != glyph_length) {
        r->callback_error = gs_error_VMerror;
        BUFfree(charstring.hdata);
        return (NULL);
    }

    hndl = BUFalloc(FSA(SL32) (sizeof(CDATASTR)));
    if (!hndl) {
        r->callback_error = gs_error_VMerror;
        BUFfree(charstring.hdata);
        return (NULL);
    }

    pcharstring = (PCDATASTR_DEMO) MEMptr(hndl);

    pcharstring->len = charstring.len;  /* Datalen */
    pcharstring->hdata = charstring.hdata;
    pcharstring->decrypted = 1;

    return ((LPUB8) pcharstring);
#else
    ushort glyph_length = ff->get_glyph(ff, chId, 0, 0);
    LPUB8 q;
    pcleo_glyph_list_elem *g = (pcleo_glyph_list_elem *) gs_malloc(r->mem,
                                                                   sizeof
                                                                   (pcleo_glyph_list_elem)
                                                                   +
                                                                   sizeof
                                                                   (PS_CHAR_HDR)
                                                                   + 2 + 2 +
                                                                   glyph_length
                                                                   + 1, 1,
                                                                   "PSEO char");
    PS_CHAR_HDR *h;
    ufst_common_font_data *d = (ufst_common_font_data *) r->fc.font_hdr - 1;

    FSA_FROM_SERVER;

    if (g == 0 || glyph_length == (ushort) - 1) {
        r->callback_error = gs_error_invalidfont;
        return 0;
    }
    g->chId = chId;
    g->next = d->glyphs;
    d->glyphs = g;
    h = (PS_CHAR_HDR *) (g + 1);
    h->format = 30;             /* raster=4, DJ=5, IF=10, TT=15, PS=30 */
    h->continuation = 0;        /* always 0 */
    h->descriptorsize = 2;      /* always 2 */
    h->ch_class = 11;           /* contour=3, compound=4, tt=10, ps=11 */
    h->len = 0;                 /* # of bytes to follow (not including csum) */
    q = (byte *) h + sizeof(*h);
    /* A workaround for UFST4.6 bug in t1idecod.c (UNPACK_WORD uses pIFS->ph instead ph) :
       setting Namelen=4, rather normally it must be 0. */
    q[0] = 0;                   /* Namelen */
    q[1] = 4;                   /* Namelen */
    q[2] = (glyph_length) / 256;        /* Datalen */
    q[3] = (glyph_length) % 256;        /* Datalen */
    /* Glyph name goes here, but we don't use it. */
    q += 4;
    glyph_length = ff->get_glyph(ff, chId, q, glyph_length);
    q += glyph_length;
    *q = 1;                     /* Decrypt flag */
    pIFS->ph = (byte *) h + sizeof(*h); /* A workaround for UFST4.6 bug in t1idecod.c (UNPACK_WORD uses pIFS->ph instead ph) */
    return (LPUB8) h;
#endif
#else
    return 0;
#endif
}

static pcleo_glyph_list_elem *
find_glyph(ufst_common_font_data * d, UW16 chId)
{
    pcleo_glyph_list_elem *e;

    for (e = d->glyphs; e != 0; e = e->next)
        if (e->chId == chId)
            return e;
    return 0;
}

/* UFST callback : */
static LPUB8
impl_PCLchId2ptr(FSP UW16 chId)
{
#if UFST_REENTRANT
    fapi_ufst_server *r = IFS_to_I(pIFS);
#else
    fapi_ufst_server *r = static_server_ptr_for_ufst_callback;
#endif
    gs_fapi_font *ff = r->ff;
    ufst_common_font_data *d = (ufst_common_font_data *) r->fc.font_hdr - 1;
    pcleo_glyph_list_elem *g = find_glyph(d, chId);
    LPUB8 result = 0;

    if (g != 0)
        result = (LPUB8) (g + 1);
    if ((r->fc.format & FC_FONTTYPE_MASK) == FC_PST1_TYPE)
        result = get_T1_glyph(r, ff, chId);
    if ((r->fc.format & FC_FONTTYPE_MASK) == FC_TT_TYPE)
        result = get_TT_glyph(r, ff, chId);
    return result;
}

static inline void
pack_word(LPUB8 * p, UW16 v)
{
    LPUB8 q = (LPUB8) & v;

#if (BYTEORDER == LOHI)         /* defied in UFST includes */
    (*p)[1] = q[0];
    (*p)[0] = q[1];
#else
    *(UW16 *) (*p) = v;
#endif
    *p += 2;
}

static inline void
pack_long(LPUB8 * p, UL32 v)
{
    LPUB8 q = (LPUB8) & v;

#if (BYTEORDER == LOHI)         /* defied in UFST includes */
    (*p)[3] = q[0];
    (*p)[2] = q[1];
    (*p)[1] = q[2];
    (*p)[0] = q[3];
#else
    *(UL32 *) (*p) = v;
#endif
    *p += 4;
}

static inline void
pack_float(LPUB8 * p, LPUB8 pe, float v)
{
    int plen = (int)(((char *)(*p)) - ((char *)(pe)));
    gs_snprintf((char *)(*p), plen, "%f", v);
    *p += strlen((const char *)*p) + 1;
}

#define PACK_ZERO(p) *(p++) = 0
#define PACK_BYTE(p, c) *(p++) = c

static inline int
pack_feature_word(gs_fapi_font * ff, LPUB8 * p, gs_fapi_font_feature var, int ind)
{
    UW16 val;
    int code = ff->get_word(ff, var, ind, (unsigned short *)&val);
    if (code < 0)
        return code;
    pack_word(p, val);

    return code;
}

static inline int
pack_feature_long(gs_fapi_font * ff, LPUB8 * p, gs_fapi_font_feature var, int ind)
{
    UL32 val;
    unsigned long lv;
    int code = ff->get_long(ff, var, ind, &lv);
    if (code < 0)
        return code;
    val = (UL32)lv;
    pack_word(p, val);

    return code;
}

static int
pack_pseo_word_array(fapi_ufst_server * r, gs_fapi_font * ff, UB8 ** p,
                     UW16 max_count, gs_fapi_font_feature count_id,
                     gs_fapi_font_feature array_id)
{
    UW16 k, k2, j;
    int code;

    code = ff->get_word(ff, count_id, 0, (unsigned short *)&k2);
    if (code < 0)
        return code;
    k = min(k2, max_count);

    pack_word(p, k);
    for (j = 0; j < k; j++)
        code = pack_feature_word(ff, p, j, array_id);
    for (; j < max_count; j++)
        pack_word(p, 0);

    return code;
}

static int
pack_pseo_fhdr(fapi_ufst_server * r, gs_fapi_font * ff, UB8 * p, UB8 * pe)
{
    ushort j, n, skip = 0;
    int code;

    while (((uint64_t) p) & 0x03)       /* align to QUADWORD */
        PACK_ZERO(p);

    pack_long(&p, 1);           /* format = 1 */
    for (j = 0; j < 6; j++) {
        float f;
        code = ff->get_float(ff, gs_fapi_font_feature_FontMatrix, j, &f);
        if (code < 0)
            return code;
        pack_float(&p, pe, f);
    }
    while (((uint64_t) p) & 0x03)       /* align to QUADWORD */
        PACK_ZERO(p);
    /* UFST has no definition for PSEO structure, so implement serialization : */
    /* NOTE: PACK_LONG and PACK_WORD macros can do a return on error */
    code = pack_feature_long(ff, &p, 0, gs_fapi_font_feature_UniqueID);
    if (code < 0)
        return code;
    code = pack_feature_long(ff, &p, 0, gs_fapi_font_feature_BlueScale);
    if (code < 0)
        return code;
    code = pack_feature_word(ff, &p, 0, gs_fapi_font_feature_Weight);
    if (code < 0)
        return code;
    code = pack_feature_word(ff, &p, 0, gs_fapi_font_feature_ItalicAngle);
    if (code < 0)
        return code;
    code = pack_feature_word(ff, &p, 0, gs_fapi_font_feature_IsFixedPitch);
    if (code < 0)
        return code;
    code = pack_feature_word(ff, &p, 0, gs_fapi_font_feature_UnderLinePosition);
    if (code < 0)
        return code;
    code = pack_feature_word(ff, &p, 0, gs_fapi_font_feature_UnderlineThickness);
    if (code < 0)
        return code;
    code = pack_feature_word(ff, &p, 0, gs_fapi_font_feature_FontType);
    if (code < 0)
        return code;
    code = pack_feature_word(ff, &p, 0, gs_fapi_font_feature_FontBBox);
    if (code < 0)
        return code;
    code = pack_feature_word(ff, &p, 1, gs_fapi_font_feature_FontBBox);
    if (code < 0)
        return code;
    code = pack_feature_word(ff, &p, 2, gs_fapi_font_feature_FontBBox);
    if (code < 0)
        return code;
    code = pack_feature_word(ff, &p, 3, gs_fapi_font_feature_FontBBox);
    if (code < 0)
        return code;
    code = pack_pseo_word_array(r, ff, &p, 14, gs_fapi_font_feature_BlueValues_count,
                         gs_fapi_font_feature_BlueValues);
    if (code < 0)
        return code;
    code = pack_pseo_word_array(r, ff, &p, 10, gs_fapi_font_feature_OtherBlues_count,
                         gs_fapi_font_feature_OtherBlues);
    if (code < 0)
        return code;

    code = pack_pseo_word_array(r, ff, &p, 14,
                         gs_fapi_font_feature_FamilyBlues_count,
                         gs_fapi_font_feature_FamilyBlues);
    if (code < 0)
        return code;

    code = pack_pseo_word_array(r, ff, &p, 10,
                         gs_fapi_font_feature_FamilyOtherBlues_count,
                         gs_fapi_font_feature_FamilyOtherBlues);
    if (code < 0)
        return code;

    code = pack_feature_word(ff, &p, 0, gs_fapi_font_feature_BlueShift);
    if (code < 0)
        return code;
    code = pack_feature_word(ff, &p, 0, gs_fapi_font_feature_BlueFuzz);
    if (code < 0)
        return code;
    code = pack_feature_word(ff, &p, 0, gs_fapi_font_feature_StdHW);
    if (code < 0)
        return code;
    code = pack_feature_word(ff, &p, 0, gs_fapi_font_feature_StdVW);
    if (code < 0)
        return code;
    code = pack_pseo_word_array(r, ff, &p, 12, gs_fapi_font_feature_StemSnapH_count,
                         gs_fapi_font_feature_StemSnapH);
    if (code < 0)
        return code;

    code = pack_pseo_word_array(r, ff, &p, 12, gs_fapi_font_feature_StemSnapV_count,
                         gs_fapi_font_feature_StemSnapV);
    if (code < 0)
        return code;

    code = pack_feature_word(ff, &p, 0, gs_fapi_font_feature_ForceBold);
    if (code < 0)
        return code;
    code = pack_feature_word(ff, &p, 0, gs_fapi_font_feature_LanguageGroup);
    if (code < 0)
        return code;
    code = pack_feature_word(ff, &p, 0, gs_fapi_font_feature_lenIV);
    if (code < 0)
        return code;
    for (j = 0; j < 12; j++)
        PACK_ZERO(p), PACK_ZERO(p);     /* Reserved2 */
    /* max data size = 107 words + 6 floats in ASCII */
    code = ff->get_word(ff, gs_fapi_font_feature_Subrs_count, 0, &n);
    if (code < 0)
        return code;
    pack_word(&p, n);
    for (j = 0; j < n; j++) {
        ushort subr_len = ff->get_subr(ff, j, 0, 0);

        if (subr_len != 0) {
            pack_word(&p, j);
            pack_word(&p, subr_len);
            PACK_BYTE(p, 1);    /* is_decrypted */
            ff->get_subr(ff, j, p, subr_len);
            p += subr_len;
        }
        else
            skip = 1;
    }
    code = ff->get_word(ff, gs_fapi_font_feature_GlobalSubrs_count, 0, &n);
    if (code < 0)
        return code;

    pack_word(&p, n);
    for (j = 0; j < n; j++) {
        ushort subr_len = ff->get_gsubr(ff, j, 0, 0);

        if (subr_len != 0) {
            pack_word(&p, j);
            pack_word(&p, subr_len);
            PACK_BYTE(p, 1);        /* is_decrypted */
            ff->get_gsubr(ff, j, p, subr_len);
            p += subr_len;
        }
        else
            skip = 1;
    }
    if (skip)
        pack_word(&p, 0xFFFF);
    return -1;
}

static char *
my_strdup(fapi_ufst_server * r, const char *s, const char *cname)
{
    int len = strlen(s) + 1;
    char *p = (char *)gs_malloc(r->mem, len, 1, cname);

    if (p != 0)
        memcpy(p, s, len);
    return p;
}

static gs_fapi_retcode
fco_open(fapi_ufst_server * r, const char *font_file_path,
         fco_list_elem ** result)
{
    int code;
    fco_list_elem *e = gx_UFST_find_static_fco(font_file_path);

    if (e != NULL) {
        *result = e;
        return 0;
    }
    for (e = r->fco_list; e != 0; e = e->next) {
        if (!strcmp(e->file_path, font_file_path))
            break;
    }
    if (e == 0) {
        SW16 fcHandle;

        FSA_FROM_SERVER;

        if ((code = CGIFfco_Open(FSA(UB8 *) font_file_path, &fcHandle)) != 0)
            return code;
        e = (fco_list_elem *) gs_malloc(r->mem, sizeof(*e), 1,
                                        "fco_list_elem");
        if (e == 0) {
            CGIFfco_Close(FSA fcHandle);
            return_error(gs_error_VMerror);
        }
        e->open_count = 0;
        e->fcHandle = fcHandle;
        e->file_path = my_strdup(r, font_file_path, "fco_file_path");
        if (e->file_path == 0) {
            CGIFfco_Close(FSA fcHandle);
            gs_free(r->mem, e, 0, 0, "fco_list_elem");
            return_error(gs_error_VMerror);
        }
        e->next = r->fco_list;
        r->fco_list = e;
    }
    e->open_count++;
    *result = e;
    return 0;
}

static gs_fapi_retcode
ufst_make_font_data(fapi_ufst_server * r, const char *font_file_path,
                    gs_fapi_font * ff, ufst_common_font_data ** return_data)
{
    ulong area_length = sizeof(ufst_common_font_data), tt_size = 0;
    LPUB8 buf;
    PCLETTO_FHDR *h;
    ufst_common_font_data *d;
    bool use_XL_format = ff->is_mtx_skipped;
    int code;

    FSA_FROM_SERVER;

    *return_data = 0;
    r->fc.ttc_index = ff->subfont;
    if (ff->font_file_path == NULL) {
#if UFST_VERSION_MAJOR < 6
        return (gs_error_invalidaccess);
#else
        /* If we have the Freetype server available, always use it for non-FCO fonts */
        if (gs_fapi_available(r->mem, (char *)"FreeType")) {
            return (gs_error_invalidaccess);
        }

        area_length += PCLETTOFONTHDRSIZE;
        if (ff->is_type1) {
            int subrs_count;
            int subrs_length;
            int subrs_area_size;
            int gsubrs_count;
            unsigned short usval;
            unsigned long ulval;

            code = ff->get_word(ff, gs_fapi_font_feature_Subrs_count, 0, &usval);
            if (code < 0)
                return code;
            subrs_count = usval;
            code = ff->get_word(ff, gs_fapi_font_feature_GlobalSubrs_count, 0, &usval);
            if (code < 0)
                return code;
            gsubrs_count = usval;

            subrs_count += gsubrs_count;

            code = ff->get_long(ff, gs_fapi_font_feature_Subrs_total_size, 0, &ulval);
            if (code < 0)
                return code;
            subrs_length = (int)ulval;

            subrs_area_size = subrs_count * 5 + subrs_length + 2;
            area_length += 360 + subrs_area_size;       /* some inprecise - see pack_pseo_fhdr */
        }
        else {
            code = ff->get_long(ff, gs_fapi_font_feature_TT_size, 0, &tt_size);
            if (code < 0)
                return code;

            if (tt_size == 0)
                return_error(gs_error_invalidfont);
/*            area_length += tt_size + (use_XL_format ? 6 : 4) + 4 + 2;*/
            area_length += tt_size + 6 + 4 + 2;
        }
#endif
    }
    else {
        int sind = strlen(font_file_path);

        if (strncasecmp(font_file_path + sind - 4, ".fco", 4) != 0) {
            return (gs_error_invalidaccess);
        }
        area_length += strlen(font_file_path) + 1;
    }
    buf = gs_malloc(r->mem, area_length, 1, "ufst font data");
    if (buf == 0)
        return_error(gs_error_VMerror);

    memset(buf, 0x00, area_length);

    d = (ufst_common_font_data *) buf;
    d->tt_font_body_offset = 0;
    d->platformId = 0;
    d->specificId = 0;
    d->decodingID[0] = 0;
    d->glyphs = 0;
    d->is_disk_font = (ff->font_file_path != NULL);
    d->ufstfontinfo = NULL;
    d->ufstfontinfosize = 0;

    if (d->is_disk_font) {
        fco_list_elem *e = gx_UFST_find_static_fco(font_file_path);

        if (e != NULL) {
            memcpy(d + 1, font_file_path, strlen(font_file_path) + 1);
            d->font_id = (e->fcHandle << 16) | ff->subfont;
            d->font_type = FC_FCO_TYPE;
        }
        else {
            stream *f = sfopen(font_file_path, "r", r->mem);

            if (f == NULL) {
                ufst_emprintf1(r->mem,
                          "fapiufst: Can't open %s\n", font_file_path);
                return_error(gs_error_undefinedfilename);
            }
            memcpy(d + 1, font_file_path, strlen(font_file_path) + 1);
            d->font_type = get_font_type(f);
            sfclose(f);
            if (d->font_type == FC_FCO_TYPE) {
                fco_list_elem *e;

                if ((code = fco_open(r, font_file_path, &e)) != 0)
                    return code;
                d->font_id = (e->fcHandle << 16) | ff->subfont;
            }
        }
    }
    else {
        unsigned long ulval;

        d->font_type = (ff->is_type1 ? FC_PST1_TYPE : FC_TT_TYPE);
        code = ff->get_long(ff, gs_fapi_font_feature_UniqueID, 0, &ulval);
        if (code < 0)
            return code;
        d->font_id = ulval;
        if (d->font_id < 0) {
            d->font_id = assign_font_id();
        }
        h = (PCLETTO_FHDR *) (buf + sizeof(ufst_common_font_data));

        if (ff->is_type1) {
            h->fontDescriptorSize = PCLETTOFONTHDRSIZE;
        }
        else {
            h->fontDescriptorSize = PCLETTOFONTHDRSIZE;
        }

        if (d->font_type == FC_TT_TYPE && !use_XL_format)
            h->descriptorFormat = 16;
        else
            h->descriptorFormat = 15;
        h->fontType = 11;       /* wrong *//*  3- 11=Unicode; 0,1,2 also possible */
        h->style_msb = 0;       /* wrong *//*  4- from PCLT table in TrueType font */
        h->reserved1 = 0;
        h->baselinePosition = 0;        /* wrong *//*  6- from head table in TT font; = 0 */
        h->cellWidth = 1024;    /* wrong *//*  8- head, use xMax - xMin */
        h->cellHeight = 1024;   /* wrong *//* 10- head, use yMax - yMin */
        h->orientation = 0;     /* 12- 0 */
        h->spacing = 1;         /* wrong *//* 13- 1=proportional, 0-fixed pitch */
        h->characterSet = 56;   /* wrong *//* 14- same as symSetCode; =56 if unbound. */
        h->pitch = 1024;        /* wrong *//* 16- PCLT */
        h->height = 0;          /* 18- 0 if TrueType */
        h->xHeight = 512;       /* wrong *//* 20- PCLT */
        h->widthType = 0;       /* wrong *//* 22- PCLT */
        h->style_lsb = 0;       /* wrong *//* 23- PCLT */
        h->strokeWeight = 0;    /* wrong *//* 24- PCLT */
        h->typeface_lsb = 0;    /* wrong *//* 25- PCLT */
        h->typeface_msb = 0;    /* wrong *//* 26- PCLT */
        h->serifStyle = 0;      /* wrong *//* 27- PCLT */
        h->quality = 0;         /* wrong *//* 28- 2 if TrueType */
        h->placement = 0;       /* wronfg *//* 29- 0 if TrueType */
        h->underlinePosition = 0;       /* 30- 0 */
        h->underlineHeight = 0; /* 31- 0 */
        h->textHeight = 102;    /* wrong *//* 32- from OS/2 table in TT font */
        h->textWidth = 1024;    /* wrong *//* 34- OS/2 */
        h->firstCode = 0;       /* 36- set to 0 if unbound */
        h->lastCode = 255;      /* wrong *//* 38- max number of downloadable chars if unbound */
        h->pitch_ext = 0;       /* 40- 0 if TrueType */
        h->height_ext = 0;      /* 41- 0 if TrueType */
        h->capHeight = 1024;    /* wrong *//* 42- PCLT */
        h->fontNumber = d->font_id;     /* 44- PCLT */
        h->fontName[0] = 0;     /* wrong *//* 48- PCLT */
        h->scaleFactor = 1024;  /* wrong *//* 64- head:unitsPerEm */
        h->masterUnderlinePosition = 0; /* wrong *//* 66- post table, or -20% of em */
        h->masterUnderlineHeight = 0;   /* wrong *//* 68- post table, or 5% of em */

        h->fontScalingTechnology = 1;   /* 70- 1=TrueType; 0=Intellifont */
        h->variety = 0;         /* 71- 0 if TrueType */

        memset((LPUB8) h + PCLETTOFONTHDRSIZE, 0, 8);   /* work around bug in PCLswapHdr : it wants format 10 */
        /*  fixme : Most fields above being marked "wrong" look unused by UFST.
           Need to check for sure.
         */
        /*  fixme : This code assumes 1-byte alignment for PCLETTO_FHDR structure.
           Use PACK_* macros to improve.
         */
        UFST_PCLswapHdr((UB8 *) h, 0);

        if (ff->is_type1) {
            LPUB8 fontdata = (LPUB8) h + PCLETTOFONTHDRSIZE;

            code = pack_pseo_fhdr(r, ff, fontdata, (LPUB8)(buf + area_length));
            if (code < 0)
                return code;
        }
        else {
            LPUB8 pseg = (LPUB8) h + PCLETTOFONTHDRSIZE;
            LPUB8 fontdata = pseg + 6 /* (use_XL_format ? 6 : 4) */ ;


            pseg[0] = 'G';
            pseg[1] = 'T';

            *((ulong *) (&(pseg[2]))) = SWAPL(tt_size);

            d->tt_font_body_offset = (LPUB8) fontdata - (LPUB8) d;

            code = ff->serialize_tt_font(ff, fontdata, tt_size);
            if (code < 0)
                return code;

            *(fontdata + tt_size) = 255;
            *(fontdata + tt_size + 1) = 255;
            *(fontdata + tt_size + 2) = 0;
            *(fontdata + tt_size + 3) = 0;
            *(fontdata + tt_size + 4) = 0;
            *(fontdata + tt_size + 5) = 0;      /* checksum */
        }
    }
    *return_data = d;
    return 0;
}

static void
prepare_typeface(fapi_ufst_server * r, ufst_common_font_data * d)
{
    r->fc.format = d->font_type;
    r->fc.font_id = d->font_id;
    r->fc.font_hdr = (UB8 *) (d + 1);
    if (!d->is_disk_font)
        r->fc.format |= FC_EXTERN_TYPE;
}

static gs_fapi_retcode
gs_fapi_ufst_get_scaled_font_aux(gs_fapi_server * server, gs_fapi_font * ff,
                    const gs_fapi_font_scale * font_scale,
                    const char *xlatmap, gs_fapi_descendant_code dc)
{
    fapi_ufst_server *r = If_to_I(server);
    FONTCONTEXT *fc = &r->fc;

    /*  Note : UFST doesn't provide handles for opened fonts,
       but copies FONTCONTEXT to IFSTATE and caches it.
       Due to this the plugin cannot provide a handle for the font.
       This assumes that only one font context is active at a moment.
     */
    ufst_common_font_data *d = (ufst_common_font_data *) ff->server_font_data;
    const double scale = F_ONE;
    double hx, hy;
    gs_fapi_retcode code = 0;
    bool use_XL_format = ff->is_mtx_skipped;
    int world_scale = 0;
    FONT_METRICS fm;

    FSA_FROM_SERVER;

    if (ff->is_cid && ff->is_type1 && ff->font_file_path == NULL &&
        (dc == gs_fapi_toplevel_begin || dc == gs_fapi_toplevel_complete)) {
        /* Don't need any processing for the top level font of a non-disk CIDFontType 0.
           See comment in FAPI_prepare_font.
           Will do with its subfonts individually.
         */
        return 0;
    }

    ff->need_decrypt = 1;
    if (d == 0) {
        if ((code = ufst_make_font_data(r, ff->font_file_path, ff, &d)) != 0)
            return code;
        ff->server_font_data = d;
        prepare_typeface(r, d);
        if (ff->font_file_path != NULL || ff->is_type1) /* such fonts don't use RAW_GLYPH */
            choose_decoding(r, d, xlatmap);
    }
    else {
        prepare_typeface(r, d);
        if (ff->font_file_path != NULL || ff->is_type1) /* such fonts don't use RAW_GLYPH */
            choose_decoding(r, d, xlatmap);
    }

    r->tran_xx = font_scale->matrix[0] / scale, r->tran_xy =
        font_scale->matrix[1] / scale;
    r->tran_yx = font_scale->matrix[2] / scale, r->tran_yy =
        font_scale->matrix[3] / scale;
    hx = hypot(r->tran_xx, r->tran_xy), hy = hypot(r->tran_yx, r->tran_yy);
    fc->xspot = F_ONE;
    fc->yspot = F_ONE;
    fc->fc_type = FC_MAT2_TYPE;

    fc->s.m2.m[0] = (int)((double)font_scale->matrix[0] / hx + 0.5);
    fc->s.m2.m[1] = (int)((double)font_scale->matrix[1] / hx + 0.5);
    fc->s.m2.m[2] = (int)((double)font_scale->matrix[2] / hy + 0.5);
    fc->s.m2.m[3] = (int)((double)font_scale->matrix[3] / hy + 0.5);
    fc->s.m2.matrix_scale = 16;
    fc->s.m2.xworld_res = font_scale->HWResolution[0] >> 16;
    fc->s.m2.yworld_res = font_scale->HWResolution[1] >> 16;

    if ((hx > 0 && hy > 0) && (hx < 1.5 || hy < 1.5)) {
        world_scale = 8;
        hx *= 256;
        hy *= 256;

        while (hx < 1.5 || hy < 1.5) {
            world_scale += 1;
            hx *= 10;
            hy *= 10;
        }
    }
    fc->s.m2.world_scale = world_scale;
    fc->s.m2.point_size = (int)(hy * 8 + 0.5);  /* 1/8ths of pixels */
    fc->s.m2.set_size = (int)(hx * 8 + 0.5);

#if GRAYSCALING
    fc->numXsubpixels = font_scale->subpixels[0];
    fc->numYsubpixels = font_scale->subpixels[1];
    fc->alignment = (font_scale->align_to_pixels ? GAGG : GAPP);
#endif

    fc->ExtndFlags = 0;
    if (d->font_type == FC_TT_TYPE) {
#if UFST_VERSION_MAJOR >= 6
        fc->ExtndFlags |= EF_TT_ERRORCHECK_ON;
#endif
        fc->ssnum = USER_CMAP;
    }
    else if (d->font_type == FC_FCO_TYPE) {
        fc->ssnum = UNICODE;
    }
    else if (d->font_type == FC_PST1_TYPE) {
        fc->ssnum = T1ENCODING;
    }

    if (d->font_type != FC_TT_TYPE && d->font_type != FC_FCO_TYPE) {
        fc->ExtndFlags |= EF_NOSYMSETMAP;
    }
    /* always use format 16 for TrueType fonts */
    if (d->font_type == FC_TT_TYPE && !use_XL_format) {
        fc->ExtndFlags |= EF_FORMAT16_TYPE;
    }
/*    fc->ExtndFlags |= EF_SUBSTHOLLOWBOX_TYPE;*/
    fc->format |= FC_NON_Z_WIND;        /* NON_ZERO Winding required for TrueType */
    fc->format |= FC_INCHES_TYPE;       /* output in units per inch */
    fc->user_platID = d->platformId;
    fc->user_specID = d->specificId;
    if (use_XL_format) {
        fc->ExtndFlags |= EF_XLFONT_TYPE;
        /* FOR XL format TTFs, we have to disable hinting */
        fc->ExtndFlags |= EF_TT_NOHINT;
    }
    if (ff->is_vertical)
        fc->ExtndFlags |= EF_UFSTVERT_TYPE;
    fc->dl_ssnum = (d->specificId << 4) | d->platformId;
    fc->ttc_index = ff->subfont;
    r->callback_error = 0;

    code = CGIFfont(FSA fc);
    if (r->callback_error != 0) {
        gs_free(r->mem, ff->server_font_data, 0, 0, "gs_fapi_ufst_get_scaled_font_aux");
        return r->callback_error;
    }

    if (d->font_type == FC_FCO_TYPE) {
        code = CGIFfont_metrics(&fm);
        if (r->callback_error != 0)
            return r->callback_error;
    }
    if (code != 0) {
        gs_free(r->mem, ff->server_font_data, 0, 0, "gs_fapi_ufst_get_scaled_font_aux");
        code = gs_error_invalidfont;
    }

    if (code >= 0 && d->font_type == FC_FCO_TYPE && ff->font_file_path) {
        UW16 bufsize = 0;
        LPSB8 buffer = NULL;

        code =
            CGIFfco_Access(FSA (LPUB8)ff->font_file_path, ff->subfont, TFATRIB_KEY,
                           &bufsize, NULL);
        if (code == 0) {
            if (bufsize > d->ufstfontinfosize) {
                gs_free(r->mem, d->ufstfontinfo, 0, 0, "ufst font info data");

                buffer = gs_malloc(r->mem, bufsize, 1, "ufst font info data");
                if (buffer == NULL) {
                    code = gs_error_VMerror;
                }
            }
            else {
                buffer = (LPSB8) d->ufstfontinfo;
                bufsize = d->ufstfontinfosize;
            }
            if (buffer) {
                (void)CGIFfco_Access(FSA (LPUB8)ff->font_file_path, ff->subfont,
                                     TFATRIB_KEY, &bufsize, buffer);
                d->ufstfontinfo = (TTFONTINFOTYPE *) buffer;
                d->ufstfontinfosize = bufsize;
            }
        }
    }

    return code;
}

static gs_fapi_retcode
gs_fapi_ufst_get_scaled_font(gs_fapi_server * server, gs_fapi_font * ff,
                const gs_fapi_font_scale * font_scale, const char *xlatmap,
                gs_fapi_descendant_code dc)
{
    return (gs_fapi_ufst_get_scaled_font_aux(server, ff, font_scale, xlatmap, dc));
}

static gs_fapi_retcode
gs_fapi_ufst_get_font_info(gs_fapi_server * server, gs_fapi_font * ff,
                   gs_fapi_font_info item, int index, void *data,
                   int *data_len)
{
    gs_fapi_retcode code = 0;
    ufst_common_font_data *d = (ufst_common_font_data *) ff->server_font_data;

    if (!d || !d->ufstfontinfo)
        return (gs_error_invalidfont);

    if (code == 0) {
        switch (item) {
            case gs_fapi_font_info_name:
                {
                    LPSB8 pname =
                        ((LPSB8) d->ufstfontinfo) + d->ufstfontinfo->psname;
                    int len = strlen(pname);

                    if (data && *data_len >= len + 1) {
                        strcpy(data, pname);
                    }
                    *data_len = len + 1;
                    break;
                }

            case gs_fapi_font_info_uid:
                if (*data_len >= sizeof(d->ufstfontinfo->pcltFontNumber)) {
                    *((int *)data) = d->ufstfontinfo->pcltFontNumber;
                }
                *data_len = sizeof(d->ufstfontinfo->pcltFontNumber);
                break;

            case gs_fapi_font_info_pitch:
                if (*data_len >= sizeof(d->ufstfontinfo->spaceBand)) {
                    *((int *)data) = d->ufstfontinfo->spaceBand;
                }
                *data_len = sizeof(d->ufstfontinfo->spaceBand);
                break;

            case gs_fapi_font_info_design_units:
                if (*data_len >= sizeof(d->ufstfontinfo->scaleFactor)) {
                    *((int *)data) = d->ufstfontinfo->scaleFactor;
                }
                *data_len = sizeof(d->ufstfontinfo->scaleFactor);
                break;

            default:
                code = gs_error_invalidaccess;
                break;
        }
    }
    return (code);
}

static gs_fapi_retcode
gs_fapi_ufst_get_decodingID(gs_fapi_server * server, gs_fapi_font * ff,
               const char **decodingID_result)
{
    fapi_ufst_server *r = If_to_I(server);
    ufst_common_font_data *d = (ufst_common_font_data *) r->fc.font_hdr - 1;

    *decodingID_result = d->decodingID;
    return 0;
}

static gs_fapi_retcode
gs_fapi_ufst_get_font_bbox(gs_fapi_server * server, gs_fapi_font * ff, int BBox[4], int unitsPerEm[2])
{
    fapi_ufst_server *r = If_to_I(server);
    SW16 VLCPower = 0;
    int code;

    FSA_FROM_SERVER;

    if ((code = CGIFbound_box(FSA BBox, &VLCPower)) < 0)
        return code;
    /*  UFST expands bbox for internal needs, and retrives the expanded bbox.
       We believe it's bug in UFST.
       Now we collapse it back to the correct size :
     */
    BBox[0] += 2;
    BBox[1] += 2;
    BBox[2] -= 2;
    BBox[3] -= 2;
    BBox[0] >>= VLCPower;
    BBox[1] >>= VLCPower;
    BBox[2] >>= VLCPower;
    BBox[3] >>= VLCPower;

    unitsPerEm[0] = unitsPerEm[1] = 1;

    return 0;
}

static gs_fapi_retcode
gs_fapi_ufst_get_font_proportional_feature(gs_fapi_server * server, gs_fapi_font * ff,
                              bool * bProportional)
{
    fapi_ufst_server *r = If_to_I(server);

    FSA_FROM_SERVER;

    *bProportional = FALSE;
    if (ff->font_file_path == NULL || ff->is_type1)
        return 0;
#if TT_ROM || TT_DISK
    {
        UB8 buf[74];
        UL32 length = sizeof(buf);

        if (CGIFtt_query
            (FSA(UB8 *) ff->font_file_path, *(UL32 *) "OS/2",
             (UW16) ff->subfont, &length, buf) != 0)
            return 0;           /* No OS/2 table - no chance to get the info. Use default == FALSE. */
        *bProportional = (buf[35] == 9);
    }
#endif
    return 0;
}

static inline void
make_asciiz_char_name(char *buf, int buf_length, gs_fapi_char_ref * c)
{
    int len = min(buf_length - 1, c->char_name_length);

    memcpy(buf, c->char_name, len);
    buf[len] = 0;
}

#define MAX_CHAR_NAME_LENGTH 30

static gs_fapi_retcode
gs_fapi_ufst_can_retrieve_char_by_name(gs_fapi_server * server, gs_fapi_font * ff,
                          gs_fapi_char_ref * c, int *result)
{
    fapi_ufst_server *r = If_to_I(server);

    FSA_FROM_SERVER;

    *result = 0;

    switch (r->fc.format & FC_FONTTYPE_MASK) {
        case FC_PST1_TYPE:
            *result = 1;
            break;
        case FC_TT_TYPE:
            break;
    }

    return 0;
}

static gs_fapi_retcode
gs_fapi_ufst_can_replace_metrics(gs_fapi_server * server, gs_fapi_font * ff,
                    gs_fapi_char_ref * c, int *result)
{
    *result = (!ff->is_type1 && ff->font_file_path == NULL &&
               c->metrics_scale == 0
               && c->metrics_type == gs_fapi_metrics_replace);
    return 0;
}

static void
release_glyphs(fapi_ufst_server * r, ufst_common_font_data * d)
{
    if (d) {
        while (d->glyphs != 0) {
            pcleo_glyph_list_elem *e = d->glyphs;

            d->glyphs = e->next;
            gs_free(r->mem, e, 0, 0, "PCLEO char");
        }
        d->glyphs = NULL;
    }
}

static gs_fapi_retcode
gs_fapi_ufst_get_fontmatrix(gs_fapi_server * I, gs_matrix * m)
{
    fapi_ufst_server *r = If_to_I(I);
    ufst_common_font_data *d =
        (ufst_common_font_data *) I->ff.server_font_data;
    gs_fapi_retcode code = 0;
    bool is_FCO_PSfont = false;

    if (I->ff.is_type1 && I->ff.font_file_path) {
        if (d == 0) {
            if ((code =
                 ufst_make_font_data(r, I->ff.font_file_path, &(I->ff),
                                     &d)) != 0)
                return (code);
            I->ff.server_font_data = d;
            prepare_typeface(r, d);
        }
        if (d) {
            is_FCO_PSfont = (d->font_type & FC_FCO_TYPE) > 0;
        }
    }

    /* There are PS jobs that rely on the standard fonts having
     * a FontMatrix of [0.001 0 0 0.001 0 0], but MT fonts actually
     * have an identity matrix. We need to compensate here. Other
     * fonts need an identity matrix returned here, as we apply the
     * font matrix explicitly in the scale calculation in zfapi.c
     * Luckily in PS, we "pretend" MT fonts are Type 1, but in
     * PCL they have their own font type.
     */
    if (is_FCO_PSfont) {
        m->xx = 0.001;
        m->xy = 0.0;
        m->yx = 0.0;
        m->yy = 0.001;
        m->tx = 0.0;
        m->ty = 0.0;
    }
    else {
        m->xx = 1.0;
        m->xy = 0.0;
        m->yx = 0.0;
        m->yy = 1.0;
        m->tx = 0.0;
        m->ty = 0.0;
    }

#if 0
    gs_matrix *base_font_matrix = &I->initial_FontMatrix;

    m->xx = I->initial_FontMatrix.xx;
    m->xy = I->initial_FontMatrix.xy;
    m->yx = I->initial_FontMatrix.yx;
    m->yy = I->initial_FontMatrix.yy;
    m->tx = I->initial_FontMatrix.tx;
    m->ty = I->initial_FontMatrix.ty;
#endif
    return (code);

}

static int
export_outline(fapi_ufst_server * r, PIFOUTLINE pol, gs_fapi_path * p)
{
    POUTLINE_CHAR outchar;
    SW16 num_contrs, num_segmts;
    LPSB8 segment;
    PINTRVECTOR points;
    SW16 i, j;

    if (pol == NULL)
        return 0;
    p->shift += r->If.frac_shift + pol->VLCpower;
    outchar = &pol->ol;
    num_contrs = outchar->num_loops;
    for (i = 0; i < num_contrs; i++) {
        num_segmts = outchar->loop[i].num_segmts;
        segment =
            (LPSB8) ((LPSB8) (outchar->loop) + outchar->loop[i].segmt_offset);
        points =
            (PINTRVECTOR) ((LPSB8) (outchar->loop) +
                           outchar->loop[i].coord_offset);
        for (j = 0; j < num_segmts; j++) {

            if (*segment == 0x00) {
                if ((p->gs_error =
                     p->moveto(p, ((int64_t) points->x) << 16,
                               ((int64_t) points->y) << 16)) != 0)
                    return p->gs_error;
                points++;
            }
            else if (*segment == 0x01) {
                if ((p->gs_error =
                     p->lineto(p, ((int64_t) points->x) << 16,
                               ((int64_t) points->y) << 16)) != 0)
                    return p->gs_error;
                points++;
            }
            else if (*segment == 0x02) {
                points += 2;
                return_error(gs_error_invalidfont);   /* This must not happen */
            }
            else if (*segment == 0x03) {
                if ((p->gs_error =
                     p->curveto(p, ((int64_t) points[0].x) << 16,
                                ((int64_t) points[0].y) << 16,
                                ((int64_t) points[1].x) << 16,
                                ((int64_t) points[1].y) << 16,
                                ((int64_t) points[2].x) << 16,
                                ((int64_t) points[2].y) << 16) < 0))
                    return p->gs_error;
                points += 3;
            }
            else
                return_error(gs_error_invalidfont);   /* This must not happen */
            segment++;
        }
    }
    return 0;
}

static inline void
set_metrics(fapi_ufst_server * r, gs_fapi_metrics * metrics,
            SL32 design_bbox[4], SW16 design_escapement[2], SW16 du_emx,
            SW16 du_emy)
{

    metrics->escapement = design_escapement[0];
    metrics->v_escapement = design_escapement[1];
    metrics->em_x = du_emx;
    metrics->em_y = du_emy;
    metrics->bbox_x0 = design_bbox[0];
    metrics->bbox_y0 = design_bbox[1];
    metrics->bbox_x1 = design_bbox[2];
    metrics->bbox_y1 = design_bbox[3];
}

static gs_fapi_retcode
get_char(gs_fapi_server *server, gs_fapi_font *ff, gs_fapi_char_ref *c,
         gs_fapi_path *p, gs_fapi_metrics *metrics, UW16 format)
{
    fapi_ufst_server *r = If_to_I(server);
    UW16 code = 0, code2 = 0;
    UL32 cc = (UL32) c->char_codes[0];
    SL32 design_bbox[4];
    char PSchar_name[MAX_CHAR_NAME_LENGTH];
    MEM_HANDLE result = NULL;
    ufst_common_font_data *d = (ufst_common_font_data *) ff->server_font_data;
    SW16 design_escapement[2];
    SW16 du_emx = 0, du_emy = 0;
    gs_string notdef_str;
    const char *notdef = ".notdef";
    const char *space = "space";
    const void *client_char_data = ff->char_data;
    const int client_char_data_len = ff->char_data_len;
    int length;
    FONTCONTEXT *fc = &r->fc;
    UW16 glyph_width[2];
    WIDTH_LIST_INPUT_ENTRY glyph_width_code;
    bool use_XL_format = ff->is_mtx_skipped;

    FSA_FROM_SERVER;

    design_escapement[0] = design_escapement[1] = 0;

    /* NAFF! UFST requires that we recreate the MT font "object"
     * each call (although, it should not!). So we want to defeat
     * the optimisation that normally prevents that in gs_fapi_do_char()
     * by blanking out the cached scale.
     */
    if(d->font_type == FC_FCO_TYPE) {
        memset(&(server->face.ctm), 0x00, sizeof(server->face.ctm));
    }

    if (ff->is_type1) {
        /* If a charstring in a Type 1 has been replaced with a PS procedure
         * get_glyph will return -1. We can then return char_code + 1 which
         * tells the FAPI code we might be dealing with a procedure, and to
         * try executing it as such.
         */
        ff->need_decrypt = false;
        length = ff->get_glyph(ff, c->char_codes[0], NULL, 0);
        if (length == -1) {
            return (c->char_codes[0] + 1);
        }
    }
    ff->need_decrypt = true;
    memset(metrics, 0, sizeof(*metrics));
    metrics->bbox_x1 = -1;

    if (ff->is_type1 && !ff->is_cid) {
        int len = min(ff->char_data_len, sizeof(PSchar_name) - 1);

        memcpy(PSchar_name, ff->char_data, len);
        PSchar_name[len] = 0;
    }
    else {
        make_asciiz_char_name(PSchar_name, sizeof(PSchar_name), c);
    }

    CGIFchIdptr(FSA & cc, PSchar_name); /* fixme : Likely only FC_PST1_TYPE needs it. */
    {                           /* hack : Changing UFST internal data. Change to r->fc doesn't help,
                                   because UFST thinks that the "outline/raster" is a property of current font. */
        pIFS->fcCur.format &= ~FC_OUTPUT_MASK;
        pIFS->fcCur.format |= format;
    }
    r->bRaster = FALSE;
    r->ff = ff;
    r->callback_error = 0;
    r->sb_x = c->sb_x;
    r->aw_x = c->aw_x;
    r->metrics_type = c->metrics_type;
    if (d->font_type == FC_FCO_TYPE
        && r->fc.ExtndFlags & EF_SUBSTHOLLOWBOX_TYPE) {
        if (c->char_name != NULL && c->char_name_length == 7
            && !memcmp(c->char_name, ".notdef", 7)) {
            /* With EF_SUBSTHOLLOWBOX_TYPE and FCO,
               UFST paints a hollow box insted .notdef .
               For Adobe compatibility we substitute a space,
               because Adobe Type 1 fonts define .notdef as a space . */
            cc = 32;
        }
    }
#if !UFST_REENTRANT
    static_server_ptr_for_ufst_callback = r;
#endif

#if PST1_RDR
    pIFS->gpps = NULL;
#endif

    if (d->font_type == FC_TT_TYPE || d->font_type == FC_FCO_TYPE) {
        glyph_width_code.CharType.TT_unicode = cc;
    }

    if (use_XL_format || d->font_type == FC_PST1_TYPE) {
        code = 0;
        glyph_width[0] = glyph_width[1] = 0;
    }
    else {
        /* CGIFwidth2() is an Artifex addition to UFST, returns an error for a
         * missing glyph, instead of silently resorting to notdef
         */
        code = CGIFFwidth2(FSA &glyph_width_code, 1, sizeof(glyph_width), glyph_width);
    }

    if (code >= 0) {
        code = CGIFchar_handle(FSA cc, &result, (SW16) 0);
    }
    else {
        result = NULL;
    }

    if (code == ERR_TT_UNDEFINED_INSTRUCTION
#if UFST_VERSION_MAJOR >= 6
        || (code >= ERR_TT_NULL_FUNCDEF && code <= ERR_TT_STACK_OUT_OF_RANGE)
#endif
        ) {
        int savehint = FC_DONTHINTTT(fc);

        fc->ExtndFlags |= EF_TT_NOHINT;

        (void)CGIFfont(FSA fc);
        code = CGIFchar_handle(FSA cc, &result, (SW16) 0);

        fc->ExtndFlags |= savehint;
        (void)CGIFfont(FSA fc);
    }
    if ((result == NULL && code != ERR_bm_buff && code != ERR_bm_too_big)
        || (code && code != ERR_fixed_space && code != ERR_bm_buff
            && code != ERR_bm_too_big)) {
        /* There is no such char in the font, try the glyph 0 (notdef) : */
        UW16 c1 = 0, ssnum = pIFS->fcCur.ssnum;
        int savehint = FC_DONTHINTTT(fc);

        fc->ExtndFlags |= EF_TT_NOHINT;
        (void)CGIFfont(FSA fc);

        if (d->font_type == FC_FCO_TYPE) {
            /* EF_SUBSTHOLLOWBOX_TYPE must work against it.
               Ensure the plugin plug__xi.fco is loaded. */
            /* fixme : Due to unknown reason EF_SUBSTHOLLOWBOX_TYPE
               doesn't work for Symbol, Dingbats, Wingdings.
               hack : render the space character. */
            c1 = 32;
        }
        else {
            /* hack : Changing UFST internal data - see above. */
            pIFS->fcCur.ssnum = RAW_GLYPH;
        }
        r->callback_error = 0;
        notdef_str.data = (byte *)notdef;
        notdef_str.size = strlen(notdef);
        ff->char_data = (void *)&notdef_str;
        ff->char_data_len = 0;
        memcpy(PSchar_name, notdef, 8);
        CGIFchIdptr(FSA(void *) & c1, (void *)notdef);

        code2 = CGIFchar_handle(FSA c1, &result, (SW16) 0);
        if (code2 && code2 != ERR_fixed_space && code2 != ERR_bm_buff
            && code2 != ERR_bm_too_big) {

            notdef_str.data = (byte *)space;
            notdef_str.size = strlen(space);
            ff->char_data = (void *)&notdef_str;
            ff->char_data_len = 0;
            memcpy(PSchar_name, space, 6);

            CGIFchIdptr(FSA(void *) & c1, (void *)space);

            code2 = CGIFchar_handle(FSA c1, &result, (SW16) 0);
        }
        if (!code2 || code2 == ERR_fixed_space) {
            code = code2;
        }
        fc->ExtndFlags |= savehint;
        (void)CGIFfont(FSA fc);
        pIFS->fcCur.ssnum = ssnum;
    }
    ff->char_data = client_char_data;
    ff->char_data_len = client_char_data_len;


#if !UFST_REENTRANT
    static_server_ptr_for_ufst_callback = 0;
#endif

    r->ff = 0;
    release_glyphs(r, (ufst_common_font_data *) ff->server_font_data);
    if (code != ERR_fixed_space && code != 0)
        return code;

    if (r->callback_error != 0)
        return r->callback_error;

    if (format == FC_BITMAP_TYPE && result) {
        IFBITMAP *pbm = (IFBITMAP *) result;

        du_emx = pbm->du_emx;
        du_emy = pbm->du_emy;
        r->char_data = pbm;
        r->bRaster = TRUE;

        design_escapement[0] = pbm->escapement;

#if UFST_VERSION_MAJOR >= 6 && UFST_VERSION_MINOR >= 2

        design_bbox[0] = pbm->left_indent;
        design_bbox[1] = pbm->top_indent;
        design_bbox[2] = pbm->black_width;
        design_bbox[3] = pbm->black_depth;

        if (ff->is_vertical) {
            /* FIXME: this probably isn't need - we can probably just use glyph_width */
            if (pIFS->glyphMetricsDU.aw.x == 0 && pIFS->glyphMetricsDU.aw.y == 0) {
                design_escapement[1] = glyph_width[1];
            }
            else {
                design_escapement[1] = pIFS->glyphMetricsDU.aw.y;
            }
        }
#endif

    }
    else if (result) {
        IFOUTLINE *pol = (IFOUTLINE *) result;

        if (glyph_width[0] == 0x7fff) { /* not found */
            design_escapement[0] = pol->escapement;
        }
        else {
            design_escapement[0] = glyph_width[0];
        }
        if (ff->is_vertical) {
            if (glyph_width[1] == 0x7fff) { /* not found */
                design_escapement[1] = pol->advanceHeight;
            }
            else {
                design_escapement[1] = glyph_width[1];
            }
        }
        else {
            design_escapement[1] = 0;
        }

        if (pol->du_emx > 0 && pol->du_emy > 0) {
            du_emx = pol->du_emx;
            du_emy = pol->du_emy;
        }
        else {
            du_emx = pIFS->cs.du_emx;
            du_emy = pIFS->cs.du_emy;
        }

        design_bbox[0] = pol->left;
        design_bbox[1] = pol->bottom;
        design_bbox[2] = pol->right;
        design_bbox[3] = pol->top;

        r->char_data = (IFOUTLINE *) result;
    }
    else {
        design_escapement[0] = glyph_width[0];
        design_escapement[1] = glyph_width[1];

        design_bbox[0] = design_bbox[1] = design_bbox[2] = design_bbox[3] = 0;
    }
#if 1                           /* UFST 5.0 */
    if (USBOUNDBOX && d->font_type == FC_FCO_TYPE) {
        if (pIFS->USBBOXorigScaleFactor /* fixme : Must we check this ? */
            && pIFS->USBBOXorigScaleFactor != pIFS->USBBOXscaleFactor) {
            /* See fco_make_gaso_and_stats in fc_if.c . Debugged with hollow box in Helvetica. */
            /* Fixme : this looses a precision, an UFST bug has been reported. */
            int w = pIFS->USBBOXorigScaleFactor / 2;

            design_bbox[0] =
                pIFS->USBBOXxmin * pIFS->USBBOXscaleFactor /
                pIFS->USBBOXorigScaleFactor;
            design_bbox[1] =
                pIFS->USBBOXymin * pIFS->USBBOXscaleFactor /
                pIFS->USBBOXorigScaleFactor;
            design_bbox[2] =
                (pIFS->USBBOXxmax * pIFS->USBBOXscaleFactor +
                 w) / pIFS->USBBOXorigScaleFactor;
            design_bbox[3] =
                (pIFS->USBBOXymax * pIFS->USBBOXscaleFactor +
                 w) / pIFS->USBBOXorigScaleFactor;
        }
        else {
            design_bbox[0] = pIFS->USBBOXxmin;
            design_bbox[1] = pIFS->USBBOXymin;
            design_bbox[2] = pIFS->USBBOXxmax;
            design_bbox[3] = pIFS->USBBOXymax;
        }
    }
    else {
#if UFST_VERSION_MAJOR >= 6 && UFST_VERSION_MINOR >= 2
#else
        /* fixme: UFST 5.0 doesn't provide this data.
           Stubbing with Em box.
           Non-FCO fonts may be cropped if a glyph goes outside Em box
           (or occupy negative coordinates, such as 'y'.).
           Non-FCO fonts may be uncached if Em box is much bigger than the glyph.
         */
        design_bbox[0] = 0;
        design_bbox[1] = 0;
        design_bbox[2] = du_emx;
        design_bbox[3] = du_emy;
#endif
    }
#endif
    {                           /* UFST performs a dual rounding of the glyph origin : first
                                   the scaled glyph design origin is rounded to pixels with floor(x + 0.5),
                                   second the glyph position is rounded to pixels with floor(x + 0.5).
                                   Ghostscript rounds with floor(x) due to the pixel-center-inside rule.

                                   A right way would be to specify the half pixel offset to the glyph
                                   origin for the rendering glyph, but UFST has no interface for doing that.
                                   Instead that, to prevent a possible cropping while copying a glyph to cache cell,
                                   we expand the design bbox in a value of a pixel size.
                                   We could not determine the necessary expansion theoretically,
                                   and choosen expansion coefficients empirically,
                                   which appears equal to 2 pixels.

                                   fixme: Actually the expansion is the FONTCONTEXT property,
                                   so it could be computed at once when the scaled font is created.
                                 */
        const double expansion_x = 2, expansion_y = 2;  /* pixels *//* adesso5.pdf */
        const double XX =
            r->tran_xx * (r->fc.s.m2.xworld_res >> 16) / 72 / du_emx;
        const double XY =
            r->tran_xy * (r->fc.s.m2.yworld_res >> 16) / 72 / du_emy;
        const double YX =
            r->tran_yx * (r->fc.s.m2.xworld_res >> 16) / 72 / du_emx;
        const double YY =
            r->tran_yy * (r->fc.s.m2.yworld_res >> 16) / 72 / du_emy;
        const double det = XX * YY - XY * YX;
        const double deta = det < 0 ? -det : det;

        if (deta > 0.0000000001) {
            const double xx = YY / det, xy = -XY / det;
            const double yx = -YX / det, yy = XX / det;
            const double dx = -(expansion_x * xx + expansion_y * xy);
            const double dy = -(expansion_x * yx + expansion_y * yy);
            const SL32 dxa = (SL32) ((dx < 0 ? -dx : dx) + 0.5);
            const SL32 dya = (SL32) ((dy < 0 ? -dy : dy) + 0.5);

            design_bbox[0] -= dxa;
            design_bbox[1] -= dya;
            design_bbox[2] += dxa;
            design_bbox[3] += dya;
        }
    }
    set_metrics(r, metrics, design_bbox, design_escapement, du_emx, du_emy);
    if (code == ERR_fixed_space)
        gs_fapi_ufst_release_char_data_inline(r);
    return 0;
}

static gs_fapi_retcode
gs_fapi_ufst_get_char_outline_metrics(gs_fapi_server *server, gs_fapi_font * ff,
                         gs_fapi_char_ref * c, gs_fapi_metrics * metrics)
{
    fapi_ufst_server *r = If_to_I(server);

    gs_fapi_ufst_release_char_data_inline(r);
    return get_char(server, ff, c, NULL, metrics, FC_CUBIC_TYPE);
    /*  UFST cannot render enough metrics information without generating raster or outline.
       r->char_data keeps an outline after calling this function.
     */
}

static gs_fapi_retcode
gs_fapi_ufst_get_char_outline(gs_fapi_server * server, gs_fapi_path * p)
{
    fapi_ufst_server *r = If_to_I(server);

    return export_outline(r, (IFOUTLINE *) r->char_data, p);
}

static gs_fapi_retcode
gs_fapi_ufst_get_char_raster_metrics(gs_fapi_server * server, gs_fapi_font * ff,
                        gs_fapi_char_ref * c, gs_fapi_metrics * metrics)
{
    fapi_ufst_server *r = If_to_I(server);
    int code;

    gs_fapi_ufst_release_char_data_inline(r);
    code = get_char(server, ff, c, NULL, metrics, FC_BITMAP_TYPE);
    if (code == ERR_bm_buff || code == ERR_bm_too_big)  /* Too big character ? */
        return (gs_error_VMerror);
    return code;
    /*  UFST cannot render enough metrics information without generating raster or outline.
       r->char_data keeps a raster after calling this function.
     */
}

static gs_fapi_retcode
gs_fapi_ufst_get_char_width(gs_fapi_server * server, gs_fapi_font * ff,
               gs_fapi_char_ref * c, gs_fapi_metrics * metrics)
{
    fapi_ufst_server *r = If_to_I(server);
    int code;

    gs_fapi_ufst_release_char_data_inline(r);
    code =
        get_char(server, ff, c, NULL, metrics,
                 server->use_outline ? FC_CUBIC_TYPE : FC_BITMAP_TYPE);
    if (code == ERR_bm_buff || code == ERR_bm_too_big)  /* Too big character ? */
        return (gs_error_VMerror);
    return code;
    /*  UFST cannot render enough metrics information without generating raster or outline.
       r->char_data keeps a raster after calling this function.
     */
}


static gs_fapi_retcode
gs_fapi_ufst_get_char_raster(gs_fapi_server * server, gs_fapi_raster * rast)
{
    fapi_ufst_server *r = If_to_I(server);

    if (!r->bRaster)
        return_error(gs_error_unregistered);
    else if (r->char_data == NULL) {
        rast->height = rast->width = rast->line_step = 0;
        rast->p = 0;
    }
    else {
        IFBITMAP *pbm = (IFBITMAP *) r->char_data;

        rast->p = pbm->bm;
        rast->height = pbm->top_indent + pbm->black_depth;
        rast->width = pbm->left_indent + pbm->black_width;
        rast->line_step = pbm->width;
        rast->left_indent = pbm->left_indent;
        rast->top_indent = pbm->top_indent;
        rast->black_width = pbm->black_width;
        rast->black_height = pbm->black_depth;
        if (rast->width != 0) {
            rast->orig_x = pbm->xorigin;
            rast->orig_y = pbm->yorigin;
        }
        else
            rast->orig_x = rast->orig_y = 0;
    }
    return 0;
}

static gs_fapi_retcode
gs_fapi_ufst_release_char_data(gs_fapi_server * server)
{
    fapi_ufst_server *r = If_to_I(server);

    gs_fapi_ufst_release_char_data_inline(r);
    return 0;
}

static void
release_fco(fapi_ufst_server * r, SW16 fcHandle)
{
    fco_list_elem **e;

    if (gx_UFST_find_static_fco_handle(fcHandle) != NULL)
        return;
    for (e = &r->fco_list; *e != 0;)
        if ((*e)->fcHandle == fcHandle && (--(*e)->open_count) == 0) {
            fco_list_elem *ee = *e;

            FSA_FROM_SERVER;

            *e = ee->next;
            CGIFfco_Close(FSA ee->fcHandle);
            gs_free(r->mem, ee->file_path, 0, 0, "fco_file_path");
            gs_free(r->mem, ee, 0, 0, "fco_list_elem");
        }
        else
            e = &(*e)->next;
}

static gs_fapi_retcode
gs_fapi_ufst_release_typeface(gs_fapi_server * server, void *font_data)
{
    fapi_ufst_server *r = If_to_I(server);
    ufst_common_font_data *d;
    gs_fapi_retcode code = 0;

    FSA_FROM_SERVER;

    gs_fapi_ufst_release_char_data_inline(r);
    if (font_data == 0)
        return 0;
    d = (ufst_common_font_data *) font_data;
    if (d->ufstfontinfo) {
        gs_free(r->mem, d->ufstfontinfo, 0, 0, "ufst font data");
        d->ufstfontinfo = NULL;
    }
    prepare_typeface(r, d);
#if 1
    if (d->is_disk_font)
        code = CGIFhdr_font_purge(FSA & r->fc);
    else
        code = CGIFfont_purge(FSA & r->fc);
#endif

    release_glyphs(r, d);
    release_fco(r, (SW16) (d->font_id >> 16));
    gs_free(r->mem, font_data, 0, 0, "ufst font data");
    return code;
}

static gs_fapi_retcode
gs_fapi_ufst_check_cmap_for_GID(gs_fapi_server * server, uint * index)
{
    return 0;
}

static gs_fapi_retcode
gs_fapi_ufst_set_mm_weight_vector(gs_fapi_server *server, gs_fapi_font *ff, float *wvector, int length)
{
    (void)server;
    (void)ff;
    (void)wvector;
    (void)length;

    return gs_error_invalidaccess;
}

/* --------------------- The plugin definition : ------------------------- */

static void gs_fapi_ufst_destroy(gs_fapi_server ** server);

static const gs_fapi_server_descriptor ufst_descriptor = {
    "FAPI",
    "UFST",
    gs_fapi_ufst_destroy
};

static const gs_fapi_server ufstserver = {
    {&ufst_descriptor},
    NULL,                       /* client_ctx_p */
    16,                         /* frac_shift */
    {gs_no_id},
    {0},
    0,
    false,
    false,
    {1, 0, 0, 1, 0, 0},
    1,
    {1, 0, 0, 1, 0, 0},
    gs_fapi_ufst_ensure_open,
    gs_fapi_ufst_get_scaled_font,
    gs_fapi_ufst_get_decodingID,
    gs_fapi_ufst_get_font_bbox,
    gs_fapi_ufst_get_font_proportional_feature,
    gs_fapi_ufst_can_retrieve_char_by_name,
    gs_fapi_ufst_can_replace_metrics,
    NULL,                       /* can_simulate_style */
    gs_fapi_ufst_get_fontmatrix,
    gs_fapi_ufst_get_char_width,
    gs_fapi_ufst_get_char_raster_metrics,
    gs_fapi_ufst_get_char_raster,
    gs_fapi_ufst_get_char_outline_metrics,
    gs_fapi_ufst_get_char_outline,
    gs_fapi_ufst_release_char_data,
    gs_fapi_ufst_release_typeface,
    gs_fapi_ufst_check_cmap_for_GID,
    gs_fapi_ufst_get_font_info,
    gs_fapi_ufst_set_mm_weight_vector
};

int gs_fapi_ufst_init(gs_memory_t * mem, gs_fapi_server ** server);

int
gs_fapi_ufst_init(gs_memory_t * mem, gs_fapi_server ** server)
{
    fapi_ufst_server *serv;
    int code = 0;
    gs_memory_t *cmem = mem->non_gc_memory;

    code = gs_memory_chunk_wrap(&(cmem), mem);
    if (code != 0) {
        return (code);
    }

    serv =
        (fapi_ufst_server *) gs_alloc_bytes_immovable(cmem,
                                                      sizeof
                                                      (fapi_ufst_server),
                                                      "fapi_ufst_server");
    if (serv == NULL) {
        gs_memory_chunk_release(cmem);
        return_error(gs_error_Fatal);
    }
    memset(serv, 0, sizeof(*serv));

    serv->mem = cmem;

    serv->If = ufstserver;

    (*server) = (gs_fapi_server *) serv;

    return 0;
}

#ifndef UFST_MEMORY_CHECKING
#define UFST_MEMORY_CHECKING 0
#endif

#if UFST_MEMORY_CHECKING
extern unsigned long maxmem;
#endif

static void
gs_fapi_ufst_destroy(gs_fapi_server ** server)
{
    fapi_ufst_server *r = (fapi_ufst_server *) * server;
    gs_memory_t *cmem = r->mem;

    FSA_FROM_SERVER;

#if UFST_MEMORY_CHECKING
    dmprintf1(r->mem, "UFST used %Lf kb\n", ((long double)maxmem) / 1024);
#endif

#if 0                           /* Disabled against a reentrancy problem
                                   in a single language build for host-based applications. */
    gx_set_UFST_Callbacks(NULL, NULL, NULL);
#endif

    gs_fapi_ufst_release_char_data_inline(r);
    if (r->bInitialized && !r->ufst_is_singleton)
        gx_UFST_fini();

    while (r->fco_list != NULL) {
        fco_list_elem *e = r->fco_list;
        r->fco_list = e->next;
        gs_free(r->mem, e->file_path, 0, 0, "gs_fapi_ufst_destroy");
        gs_free(r->mem, e, 0, 0, "gs_fapi_ufst_destroy");
    }

    if (r->param) {
        gs_free(r->mem, r->param, 0, 0, "server_params");
    }

    gs_free(cmem, r, 0, 0, "gs_fapi_ufst_destroy: fapi_ufst_server");

    gs_memory_chunk_release(cmem);

    (*server) = NULL;
}
