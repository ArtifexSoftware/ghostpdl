/* Copyright (C) 1995, 1996, 1997, 1998, 1999, 2001 Aladdin Enterprises.  All rights reserved.
  
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
/* Agfa UFST plugin */

#include <assert.h>
/* GS includes : */
#include "stdio_.h"
#include "memory_.h"
#include "math_.h"
#include "errors.h"
#include "iplugin.h"
#include "ifapi.h"
/* UFST includes : */
#include "CGCONFIG.H"
#include "PORT.H"    
#include "SHAREINC.H"
#include "T1ISFNT.H"
#include "CGMACROS.H"
#define DOES_ANYONE_USE_THIS_STRUCTURE /* see TTPCLEO.H, UFST 4.2 */
#include "TTPCLEO.H"
#undef  DOES_ANYONE_USE_THIS_STRUCTURE
/* GS includes : */
#include "gxfapi.h"

#define CheckRET(a) { int x = a; if(x != 0) return x; }

GLOBAL UW16  PCLswapHdr(FSP LPUB8 ); /* UFST header doesn't define it. */

typedef enum {
    UFST_ERROR_success = 0,
    UFST_ERROR_memory = -1,
    UFST_ERROR_file = -2,
    UFST_ERROR_unimplemented = -3
} UFST_ERROR;

typedef struct pcleo_glyph_list_elem_s pcleo_glyph_list_elem;
struct pcleo_glyph_list_elem_s {
    UW16 chId;
    pcleo_glyph_list_elem *next;
    /* more data follows depending on font type */
};

typedef struct fco_list_elem_s fco_list_elem;
struct fco_list_elem_s {
    int open_count;
    SW16 fcHandle;
    char *file_path;
    fco_list_elem *next;
};

typedef struct {
    SL32 font_id;
    uint tt_font_body_offset;
    UW16 is_disk_font;
    UW16 font_type;
    UW16 platformId;
    UW16 specificId;
    pcleo_glyph_list_elem *glyphs;
} ufst_common_font_data;

typedef struct { 
    PCLETTO_CHR_HDR h;
    UW16   add_data;
    UW16   charDataSize;
    UW16   glyphID;
} PCLETTO_CHDR;

typedef struct fapi_ufst_instance_s fapi_ufst_server;
struct fapi_ufst_instance_s {
    FAPI_server If;
    int bInitialized;
    FAPI_font *ff; /* work around UFST callback reentrancy bug */
    i_plugin_client_memory client_mem;
    IF_STATE IFS;
    FONTCONTEXT fc;
    IFBITMAP * pbm;    
    double tran_xx, tran_xy, tran_yx, tran_yy;
    const char *decodingID;
    fco_list_elem *fco_list;
    FAPI_retcode callback_error;
};

/* Type casts : */

private inline fapi_ufst_server *If_to_I(FAPI_server *If)
{   return (fapi_ufst_server *)If;
}

/* ----------the ststic data work around UFST callback reentrancy bug : ----------*/

private LPUB8 fontdata;
private fapi_ufst_server *renderer = 0;

/*------------------ FAPI_server members ------------------------------------*/

private FAPI_retcode open_UFST(fapi_ufst_server *r)
{   IFCONFIG   config_block;
    /* int path_len = sizeof(config_block.ufstPath); */
    /* int type_len = sizeof(config_block.typePath); */
    CheckRET(CGIFinit(&r->IFS));
    /* gp_getenv("FAPI_USFT", config_block.ufstPath, &path_len);  fixme : do we really need this ? */
    /* gp_getenv("FAPI_USFT_TYPE", config_block.typePath, &type_len);  fixme : do we really need this ? */
    config_block.num_files = 10;
    config_block.bit_map_width = 1;
    CheckRET(CGIFconfig(&r->IFS, &config_block));
    CheckRET(CGIFenter(&r->IFS));
    return 0;
}		      

private FAPI_retcode ensure_open(FAPI_server *server)
{   fapi_ufst_server *r = If_to_I(server);
    if (r->bInitialized)
        return 0;
    r->bInitialized = 1;
    return open_UFST(r);
}

private UW16 get_font_type(FILE *f)
{   char buf[20], mark_PS[]="%!";
    int i;
    if (fread(buf, 1, sizeof(buf), f) != sizeof(buf))
        return 0;
    if (buf[0] == 0x13 || buf[0] == 0x14) /* fixme : don't know how to do correctly. */
        return FC_FCO_TYPE;
    for (i = 0; i < sizeof(buf) - sizeof(mark_PS); i++)
        if(!memcmp(buf + i, mark_PS, sizeof(mark_PS) - 1))
            return FC_PST1_TYPE;
    if (buf[0] == '\0' && buf[1] == '\1')
        return FC_TT_TYPE;
    if (buf[0] == 't' && buf[1] == 't')
        return FC_TT_TYPE;
    return 0; /* fixme : unknown type - actually an error. */
}

    
private int choose_decoding_PS(fapi_ufst_server *r, ufst_common_font_data *d, const char *cmapId)
{ /*    Work around UFST bug :
        It can't choose character by name due to bug with caching.
  */
  r->decodingID = "Latin1";
  return 1;
}

private int choose_decoding_TT(fapi_ufst_server *r, ufst_common_font_data *d, const char *cmapId)
{   int platId, specId, i;
    CMAP_QUERY q;
    UW16 font_access;
    bool failed;
    void *p = (d->is_disk_font ? (void *)(d + 1) : (void *)((UB8 *)d + d->tt_font_body_offset));
    if (sscanf(cmapId, "%d.%d", &platId, &specId) != 2)
        return 0;
    font_access = r->IFS.font_access;
    r->IFS.font_access = (d->is_disk_font ? DISK_ACCESS : ROM_ACCESS);
    failed = CGIFtt_cmap_query(&r->IFS, p, r->fc.ttc_index, &q);
    r->IFS.font_access = font_access;
    if(failed)
        return 0;
    for (i = 0; i < q.numCmap; i++)
        if (q.entry[i].platId == platId && q.entry[i].specId == specId) {
            d->platformId = platId;
            d->specificId = specId;
            return 1;
        }
    return 0;
}

private void scan_xlatmap(fapi_ufst_server *r, ufst_common_font_data *d, const char *xlatmap, const char *font_kind, 
                                    int (*choose_proc)(fapi_ufst_server *r, ufst_common_font_data *d, const char *cmapId))
{   const char *p = xlatmap;
    while(*p) {
        int good_kind =!strcmp(p, font_kind);
        p += strlen(p) + 2;
        while(*p) {
            const char *cmapId = p;
            r->decodingID = p + strlen(p) + 1;
            if (!*r->decodingID)
                break;
            p = r->decodingID + strlen(r->decodingID) + 1;
            if (good_kind)
                if (choose_proc(r, d, cmapId))
                    return;
        }
    }
    r->decodingID = NULL;
}

private void choose_decoding(fapi_ufst_server *r, ufst_common_font_data *d, const char *xlatmap)
{   if (xlatmap != 0)
        switch (d->font_type) {
            case FC_IF_TYPE: /* fixme */ break;
            case FC_PST1_TYPE: scan_xlatmap(r, d, xlatmap, "PostScript", choose_decoding_PS); break;
            case FC_TT_TYPE: scan_xlatmap(r, d, xlatmap, "TrueType", choose_decoding_TT); break;
            case FC_FCO_TYPE: /* fixme */ break;
        } 
}

private LPUB8 get_TT_glyph(fapi_ufst_server *r, FAPI_font *ff, UW16 chId)
{   pcleo_glyph_list_elem *g;
    PCLETTO_CHDR *h;
    ufst_common_font_data *d = (ufst_common_font_data *)r->fc.font_hdr - 1;
    LPUB8 q;
    ushort glyph_length = ff->get_glyph(ff, chId, 0, 0);
    g = (pcleo_glyph_list_elem *)r->client_mem.alloc(&r->client_mem, sizeof(pcleo_glyph_list_elem) + sizeof(PCLETTO_CHDR) + glyph_length + 2, "PCLETTO char");
    if (g == 0) {
        renderer->callback_error = UFST_ERROR_memory;
        return 0;
    }
    g->chId = chId;
    g->next = d->glyphs;
    d->glyphs = g;
    h = (PCLETTO_CHDR *)(g + 1);
    h->h.format = 15;
    h->h.continuation = 0; /* fixme */
    h->h.descriptorsize = 4;
    h->h.class = 15;
    h->add_data = 0;
    q = (LPUB8)&h->charDataSize;
    q[0] = (glyph_length + 4) / 256; /* fixme : why + 4 ? */
    q[1] = (glyph_length + 4) % 256;
    q = (LPUB8)&h->glyphID;
    q[0] = chId / 256;
    q[1] = chId % 256;
    ff->get_glyph(ff, chId, (LPUB8)(h + 1), glyph_length);
    q = (LPUB8)(h + 1) + glyph_length;
    q[0] = 0;
    q[1] = 0; /* checksum */
    return (LPUB8)h;
}

private LPUB8 get_T1_glyph(fapi_ufst_server *r, FAPI_font *ff, UW16 chId)
{   ushort glyph_length = ff->get_glyph(ff, chId, 0, 0);
    LPUB8 q;
    pcleo_glyph_list_elem *g = (pcleo_glyph_list_elem *)r->client_mem.alloc(&r->client_mem, sizeof(pcleo_glyph_list_elem) + sizeof(PS_CHAR_HDR) + 2 + 2 + glyph_length + 1, "PSEO char");
    PS_CHAR_HDR *h;
    ufst_common_font_data *d = (ufst_common_font_data *)r->fc.font_hdr - 1;
    if (g == 0) {
        renderer->callback_error = UFST_ERROR_memory;
        return 0;
    }
    g->chId = chId;
    g->next = d->glyphs;
    d->glyphs = g;
    h = (PS_CHAR_HDR *)(g + 1);
    h->format = 30;           /* raster=4, DJ=5, IF=10, TT=15, PS=30 */
    h->continuation = 0;     /* always 0 */
    h->descriptorsize = 2;   /* always 2 */
    h->class = 11;           /* contour=3, compound=4, tt=10, ps=11 */
    h->len = 0;              /* # of bytes to follow (not including csum) */
    q = (byte *)h + sizeof(*h);
    q[0] = 0; /* Namelen */
    q[1] = 0; /* Namelen */
    q[2] = (glyph_length) / 256; /* Datalen */
    q[3] = (glyph_length) % 256; /* Datalen */
    /* fixme : glyph name goes here */
    q+=4;
    glyph_length = ff->get_glyph(ff, chId, q, glyph_length);
    q += glyph_length;
    *q = 1; /* Decrypt flag */
    return (LPUB8)h;
}

private pcleo_glyph_list_elem * find_glyph(ufst_common_font_data *d, UW16 chId)
{   pcleo_glyph_list_elem *e;
    for (e = d->glyphs; e != 0; e = e->next)
        if (e->chId == chId)
            return e;
    return 0;
}

private LPUB8 gs_PCLEO_charptr(LPUB8 pfont_hdr, UW16  sym_code)
{   /* never called */
    renderer->callback_error = UFST_ERROR_unimplemented;
    return 0;
}

private LPUB8 gs_PCLchId2ptr(UW16 chId)
{   FAPI_font *ff = renderer->ff;
    ufst_common_font_data *d = (ufst_common_font_data *)renderer->fc.font_hdr - 1;
    pcleo_glyph_list_elem *g = find_glyph(d, chId);
    LPUB8 result = 0;
    if (g != 0)
        result = (LPUB8)(g + 1);
    if ((renderer->fc.format & FC_FONTTYPE_MASK) == FC_PST1_TYPE)
        result = get_T1_glyph(renderer, ff, chId);
    if ((renderer->fc.format & FC_FONTTYPE_MASK) == FC_TT_TYPE)
        result = get_TT_glyph(renderer, ff, chId);
    return result;
}

private LPUB8 gs_PCLglyphID2Ptr(UW16 glyphID)
{   /* never called */
    renderer->callback_error = UFST_ERROR_unimplemented;
    return 0;
}

private inline void pack_word(LPUB8 *p, UW16 v)
{   LPUB8 q = (LPUB8)&v;
    #if (BYTEORDER == LOHI) /* defied in UFST includes */
         (*p)[1] = q[0];
         (*p)[0] = q[1];
    #else
        *(UW16 *)(*p) = v;
    #endif
    *p += 2;
}

private inline void pack_long(LPUB8 *p, UL32 v)
{   LPUB8 q = (LPUB8)&v;
    #if (BYTEORDER == LOHI) /* defied in UFST includes */
         (*p)[3] = q[0];
         (*p)[2] = q[1];
         (*p)[1] = q[2];
         (*p)[0] = q[3];
    #else
        *(UL32 *)(*p) = v;
    #endif
    *p += 4;
}

private inline void pack_float(LPUB8 *p, float v)
{   sprintf((char *)(*p), "%f", v);
    *p += strlen((const char *)*p) + 1;
}

#define PACK_ZERO(p) *(p++) = 0
#define PACK_BYTE(p, c) *(p++) = c
#define PACK_WORD(p, i, var) pack_word(&p, ff->get_word(ff, var, i))
#define PACK_LONG(p, i, var) pack_long(&p, ff->get_long(ff, var, i))

private void pack_pseo_word_array(fapi_ufst_server *r, FAPI_font *ff, UB8 **p, UW16 max_count, fapi_font_feature count_id, fapi_font_feature array_id)
{   UW16 k = min(ff->get_word(ff, count_id, 0), max_count), j;
    pack_word(p, k);
    for (j = 0; j < k; j++)
        PACK_WORD(*p, j, array_id);
    for (; j < max_count; j++)
        pack_word(p, 0);
}

private void pack_pseo_fhdr(fapi_ufst_server *r, FAPI_font *ff, UB8 *p)
{   ushort j, n;
    while ((UL32)p & 0x03) /* align to QUADWORD */
	PACK_ZERO(p);
    pack_long(&p, 1);  /* format = 1 */
    for (j = 0; j < 6; j++)
        pack_float(&p, ff->get_float(ff, FAPI_FONT_FEATURE_FontMatrix, j));
    while ((UL32)p & 0x03) /* align to QUADWORD */
	PACK_ZERO(p);
    /* UFST has no definition for PSEO structure, so implementalization : */
    PACK_LONG(p, 0, FAPI_FONT_FEATURE_UniqueID);
    PACK_LONG(p, 0, FAPI_FONT_FEATURE_BlueScale);
    PACK_WORD(p, 0, FAPI_FONT_FEATURE_Weight);
    PACK_WORD(p, 0, FAPI_FONT_FEATURE_ItalicAngle);
    PACK_WORD(p, 0, FAPI_FONT_FEATURE_IsFixedPitch);
    PACK_WORD(p, 0, FAPI_FONT_FEATURE_UnderLinePosition);
    PACK_WORD(p, 0, FAPI_FONT_FEATURE_UnderlineThickness);
    PACK_WORD(p, 0, FAPI_FONT_FEATURE_FontType);
    PACK_WORD(p, 0, FAPI_FONT_FEATURE_FontBBox);
    PACK_WORD(p, 1, FAPI_FONT_FEATURE_FontBBox);
    PACK_WORD(p, 2, FAPI_FONT_FEATURE_FontBBox);
    PACK_WORD(p, 3, FAPI_FONT_FEATURE_FontBBox);
    pack_pseo_word_array(r, ff, &p, 14, FAPI_FONT_FEATURE_BlueValues_count, FAPI_FONT_FEATURE_BlueValues);
    pack_pseo_word_array(r, ff, &p, 10, FAPI_FONT_FEATURE_OtherBlues_count, FAPI_FONT_FEATURE_OtherBlues);
    pack_pseo_word_array(r, ff, &p, 14, FAPI_FONT_FEATURE_FamilyBlues_count, FAPI_FONT_FEATURE_FamilyBlues);
    pack_pseo_word_array(r, ff, &p, 10, FAPI_FONT_FEATURE_FamilyOtherBlues_count, FAPI_FONT_FEATURE_FamilyOtherBlues);
    PACK_WORD(p, 0, FAPI_FONT_FEATURE_BlueShift);
    PACK_WORD(p, 0, FAPI_FONT_FEATURE_BlueFuzz);
    PACK_WORD(p, 0, FAPI_FONT_FEATURE_StdHW);
    PACK_WORD(p, 0, FAPI_FONT_FEATURE_StdVW);
    pack_pseo_word_array(r, ff, &p, 12, FAPI_FONT_FEATURE_StemSnapH_count, FAPI_FONT_FEATURE_StemSnapH);
    pack_pseo_word_array(r, ff, &p, 12, FAPI_FONT_FEATURE_StemSnapV_count, FAPI_FONT_FEATURE_StemSnapV);
    PACK_WORD(p, 0, FAPI_FONT_FEATURE_ForceBold);
    PACK_WORD(p, 0, FAPI_FONT_FEATURE_LanguageGroup);
    PACK_WORD(p, 0, FAPI_FONT_FEATURE_lenIV);
    for (j = 0; j < 12; j++)
        PACK_ZERO(p), PACK_ZERO(p);     /* Reserved2 */
    /* max data size = 107 words + 6 floats in ASCII */
    n = ff->get_word(ff, FAPI_FONT_FEATURE_Subrs_count, 0);
    pack_word(&p, n);
    for (j = 0; j < n; j++) {
        ushort subr_len = ff->get_subr(ff, j, 0, 0);
        if (subr_len != 0) {
            pack_word(&p, j);
            pack_word(&p, subr_len);
            PACK_BYTE(p, 1); /* is_decrypted */
            ff->get_subr(ff, j, p, subr_len);
            p += subr_len;
        }
    }
}

private void enumerate_fco(fapi_ufst_server *r, const char *font_file_path)
{   /* development perpose only */
    #if 0
        UW16 i;
        for (i = 0; ; i++) {
            UW16 size;
            TTFONTINFOTYPE *pBuffer;
            UW16 code = CGIFfco_Access(&r->IFS, (LPUB8)font_file_path, i, TFATRIB_KEY, &size, NULL);
            if (code)
                break;
            pBuffer = (TTFONTINFOTYPE *)malloc(size);
            if (pBuffer == 0)
                break;
            code = CGIFfco_Access(&r->IFS, (LPUB8)font_file_path, i, TFATRIB_KEY, &size, (SB8 *)pBuffer);
            if (code)
                break;
            {   char *tfName          = (char *)pBuffer + pBuffer->tfName;
                char *pcltTypeface    = (char *)pBuffer + pBuffer->pcltTypeface;
                char *pcltFileName    = (char *)pBuffer + pBuffer->pcltFileName;
                char *familyName      = (char *)pBuffer + pBuffer->familyName;
                char *weightName      = (char *)pBuffer + pBuffer->weightName;
                char *copyrightNotice = (char *)pBuffer + pBuffer->copyrightNotice;
                pBuffer += 0; /* a place for breakpoint */
            }
            free(pBuffer);
            (void)code;
        }
    #endif
}

private char *my_strdup(fapi_ufst_server *r, const char *s, const char *cname)
{   int l = strlen(s) + 1;
    char *p = (char *)r->client_mem.alloc(&r->client_mem, l, cname);
    if (p != 0)
        memcpy(p, s, l);
    return p;
}

private FAPI_retcode fco_open(fapi_ufst_server *r, const char *font_file_path, fco_list_elem **result)
{   fco_list_elem *e = r->fco_list;
    for (; e != 0; e = e->next) {
        if (!strcmp(e->file_path, font_file_path))
            break;
    }
    if (e == 0) {
        SW16 fcHandle;
        CheckRET(CGIFfco_Open(&r->IFS, (UB8 *)font_file_path, &fcHandle));
        e = (fco_list_elem *)r->client_mem.alloc(&r->client_mem, sizeof(*e), "fco_list_elem");
        if (e == 0) {
            CGIFfco_Close(&r->IFS, fcHandle);
            return UFST_ERROR_memory;
        }
        e->open_count = 0;
        e->fcHandle = fcHandle;
        e->file_path = my_strdup(r, font_file_path, "fco_file_path");
        if (e->file_path == 0) {
            CGIFfco_Close(&r->IFS, fcHandle);
            r->client_mem.free(&r->client_mem, e, "fco_list_elem");
            return UFST_ERROR_memory;
        }
        e->next = r->fco_list;
        r->fco_list = e;
    }
    e->open_count++;
    *result = e;
    return 0;
}

private FAPI_retcode make_font_data(fapi_ufst_server *r, const char *font_file_path, int subfont, const char *xlatmap, FAPI_font *ff, ufst_common_font_data **return_data)
{   /* fixme : use portable functions */
    ulong area_length = sizeof(ufst_common_font_data), tt_size = 0;
    LPUB8 buf;
    PCLETTO_FHDR *h;
    ufst_common_font_data *d;
    *return_data = 0;
    r->fc.ttc_index = subfont;
    if (ff->font_file_path == NULL) {
        area_length += PCLETTOFONTHDRSIZE;
        if (ff->is_type1) {
            int subrs_count  = ff->get_word(ff, FAPI_FONT_FEATURE_Subrs_count, 0);
            int subrs_length = ff->get_long(ff, FAPI_FONT_FEATURE_Subrs_total_size, 0);
            int lenIV = ff->get_long(ff, FAPI_FONT_FEATURE_lenIV, 0);
            int subrs_area_size = subrs_count * (5 - (lenIV == 0xFFFF ? 0 : lenIV)) + subrs_length;
            area_length += 360 + subrs_area_size; /* some inprecise - see pack_pseo_fhdr */
        } else {
            tt_size  = ff->get_long(ff, FAPI_FONT_FEATURE_TT_size, 0);
            area_length += tt_size + 4 + 4 + 2;
        }
    } else
        area_length += strlen(font_file_path) + 1;
    buf = r->client_mem.alloc(&r->client_mem, area_length, "ufst font data");
    if (buf == 0)
        return UFST_ERROR_memory;
    d = (ufst_common_font_data *)buf;
    d->tt_font_body_offset = 0;
    d->platformId = 0;
    d->specificId = 0;
    d->glyphs = 0;
    d->font_id = 0;
    d->is_disk_font = (ff->font_file_path != NULL);
    if (d->is_disk_font) {
        FILE *f = fopen(font_file_path, "rb"); /* note: gp_fopen isn't better since UFST calls fopen. */
        memcpy(d + 1, font_file_path, strlen(font_file_path) + 1);
        if (f == NULL)
            return UFST_ERROR_file;
        d->font_type = get_font_type(f);
        fclose(f);
        if (d->font_type == FC_FCO_TYPE) {
            fco_list_elem *e;
            CheckRET(fco_open(r, font_file_path, &e));
            enumerate_fco(r, font_file_path); /* development perpose only */
            d->font_id = (e->fcHandle << 16) | subfont;
        }
    } else {
        d->font_type = (ff->is_type1 ? FC_PST1_TYPE : FC_TT_TYPE);
        h = (PCLETTO_FHDR *)(buf + sizeof(ufst_common_font_data));
        h->fontDescriptorSize = PCLETTOFONTHDRSIZE;
        h->descriptorFormat = 15;
        h->fontType = 11; /* wrong */                /*  3- 11=Unicode; 0,1,2 also possible */
        h->style_msb = 0; /* wrong */               /*  4- from PCLT table in TrueType font */
        h->reserved1 = 0;
        h->baselinePosition = 0; /* wrong */        /*  6- from head table in TT font; = 0 */
        h->cellWidth = 1024; /* wrong */               /*  8- head, use xMax - xMin */
        h->cellHeight = 1024; /* wrong */             /* 10- head, use yMax - yMin */
        h->orientation = 0;             /* 12- 0 */
        h->spacing = 1; /* wrong */                 /* 13- 1=proportional, 0-fixed pitch */
        h->characterSet = 56; /* wrong */            /* 14- same as symSetCode; =56 if unbound. */
        h->pitch = 1024; /* wrong */                   /* 16- PCLT */
        h->height = 0;                  /* 18- 0 if TrueType */
        h->xHeight = 512; /* wrong */                 /* 20- PCLT */
        h->widthType = 0; /* wrong */               /* 22- PCLT */
        h->style_lsb = 0; /* wrong */               /* 23- PCLT */
        h->strokeWeight = 0; /* wrong */            /* 24- PCLT */
        h->typeface_lsb = 0; /* wrong */            /* 25- PCLT */
        h->typeface_msb = 0; /* wrong */           /* 26- PCLT */
        h->serifStyle = 0; /* wrong */              /* 27- PCLT */
        h->quality = 0; /* wrong */                 /* 28- 2 if TrueType */
        h->placement = 0; /* wronfg */               /* 29- 0 if TrueType */
        h->underlinePosition = 0;       /* 30- 0 */
        h->underlineHeight = 0;         /* 31- 0 */
        h->textHeight = 102; /* wrong */              /* 32- from OS/2 table in TT font */
        h->textWidth = 1024; /* wrong */              /* 34- OS/2 */
        h->firstCode = 0;               /* 36- set to 0 if unbound */
        h->lastCode = 255; /* wrong */                /* 38- max number of downloadable chars if unbound */
        h->pitch_ext = 0;               /* 40- 0 if TrueType */
        h->height_ext = 0;              /* 41- 0 if TrueType */
        h->capHeight = 1024; /* wrong */               /* 42- PCLT */
        h->fontNumber = 0; /* wrong */             /* 44- PCLT */
        h->fontName[0] = 0; /* wrong */            /* 48- PCLT */
        h->scaleFactor = 1024; /* wrong */             /* 64- head:unitsPerEm */
        h->masterUnderlinePosition = 0; /* wrong */ /* 66- post table, or -20% of em */
        h->masterUnderlineHeight = 0; /* wrong */   /* 68- post table, or 5% of em */
        h->fontScalingTechnology = 1;   /* 70- 1=TrueType; 0=Intellifont */
        h->variety = 0;                 /* 71- 0 if TrueType */
        memset((LPUB8)h + PCLETTOFONTHDRSIZE, 0 ,8); /* work around bug in PCLswapHdr : it wants format 10 */
        PCLswapHdr(&r->IFS, (UB8 *)h);
        if (ff->is_type1) {
            fontdata = (LPUB8)h + PCLETTOFONTHDRSIZE;
            pack_pseo_fhdr(r, ff, fontdata);
        } else {
            LPUB8 pseg = (LPUB8)h + PCLETTOFONTHDRSIZE;
            if (tt_size > 65000)
                return UFST_ERROR_unimplemented; /* Must not happen because we skip 'glyp', 'loca' and 'cmap'. */
            pseg[0] = 'G';
            pseg[1] = 'T';
            pseg[2] = tt_size / 256; /* fixme : tt_size bigger than 65536 */
            pseg[3] = tt_size % 256;
            fontdata = pseg + 4;
            d->tt_font_body_offset = (LPUB8)fontdata - (LPUB8)d;
            ff->serialize_tt_font(ff, fontdata, tt_size);
            *(fontdata + tt_size    ) = 255;
            *(fontdata + tt_size + 1) = 255;
            *(fontdata + tt_size + 2) = 0;
            *(fontdata + tt_size + 3) = 0;
            *(fontdata + tt_size + 4) = 0;
            *(fontdata + tt_size + 5) = 0;  /* checksum */
        }
    }
    *return_data = d;
    return 0;
}

private void prepare_typeface(fapi_ufst_server *r, ufst_common_font_data *d)
{   r->fc.format = d->font_type;
    r->fc.font_id = d->font_id;
    r->fc.font_hdr = (UB8 *)(d + 1);
    if (!d->is_disk_font)
        r->fc.format |= FC_EXTERN_TYPE;
}

private FAPI_retcode get_scaled_font(FAPI_server *server, FAPI_font *ff, const char *font_file_path, int subfont, const FracInt matrix[6], const FracInt HWResolution[2], const char *xlatmap, int bCache)
{   fapi_ufst_server *r = If_to_I(server);
    FONTCONTEXT *fc = &r->fc;
    /*  Note : UFST doesn't provide handles for opened fonts,
        but copies FONTCONTEXT to IFSTATE and caches it.
        Due to this the plugin cannot provide a handle for the font.
        This assumes that only one font context is active at a moment.
    */
    /*  Fixme: bCache = 0 is not implemented yet.  */
    ufst_common_font_data *d = (ufst_common_font_data *)ff->server_font_data;
    const double scale = F_ONE;
    double hx, hy, sx, sy;
    FAPI_retcode code;
    ff->need_decrypt = 1;
    if (d == 0) {
        CheckRET(make_font_data(r, font_file_path, subfont, xlatmap, ff, &d));
        ff->server_font_data = d;
        prepare_typeface(r, d);
        if (ff->font_file_path != NULL || ff->is_type1) /* such fonts don't use RAW_GLYPH */
            choose_decoding(r, d, xlatmap);
    } else
        prepare_typeface(r, d);
    r->decodingID = 0; /* Safety : don't keep pending pointers. */
    r->tran_xx = matrix[0] / scale, r->tran_xy = matrix[1] / scale;
    r->tran_yx = matrix[2] / scale, r->tran_yy = matrix[3] / scale;
    hx = hypot(r->tran_xx, r->tran_xy), hy = hypot(r->tran_yx, r->tran_yy);
    sx = r->tran_xx * r->tran_yx + r->tran_xy * r->tran_yy; 
    sy = r->tran_xx * r->tran_yy - r->tran_xy * r->tran_yx;
    fc->xspot     = F_ONE;
    fc->yspot     = F_ONE;
    fc->fc_type   = FC_MAT2_TYPE;
    fc->s.m2.m[0] = (int)((double)matrix[0] / hx + 0.5);
    fc->s.m2.m[1] = (int)((double)matrix[1] / hx + 0.5);
    fc->s.m2.m[2] = (int)((double)matrix[2] / hy + 0.5);
    fc->s.m2.m[3] = (int)((double)matrix[3] / hy + 0.5);
    fc->s.m2.matrix_scale = 16;
    fc->s.m2.xworld_res = HWResolution[0];
    fc->s.m2.yworld_res = HWResolution[1];
    fc->s.m2.world_scale = 16;
    fc->s.m2.point_size   = (int)(hy * 8 + 0.5); /* 1/8ths of pixels */
    fc->s.m2.set_size     = (int)(hx * 8 + 0.5);
    fc->ssnum       = (ff->font_file_path == NULL && !ff->is_type1 ? RAW_GLYPH : 0x8000 /* no symset mapping */ );
    fc->format      |= FC_NON_Z_WIND;   /* NON_ZERO Winding required for TrueType */
    fc->format      |= FC_INCHES_TYPE;  /* output in units per inch */
    fc->user_platID = 0;
    fc->user_specID = 0;
    fc->dl_ssnum = (d->specificId << 4) | d->platformId;
    fc->ttc_index   = subfont;
    r->callback_error = 0;
    renderer = r;
    gx_set_UFST_Callbacks(gs_PCLEO_charptr, gs_PCLchId2ptr, gs_PCLglyphID2Ptr);
    code = CGIFfont(&r->IFS, fc);
    CheckRET(r->callback_error);
    return code;
}

private FAPI_retcode get_decodingID(FAPI_server *server, FAPI_font *ff, const char **decodingID_result)
{   fapi_ufst_server *r = If_to_I(server);
    *decodingID_result = r->decodingID;
    return 0;
}

private FAPI_retcode get_font_bbox(FAPI_server *server, FAPI_font *ff, int BBox[4])
{   fapi_ufst_server *r = If_to_I(server);
    SW16 VLCPower = 0;
    CheckRET(CGIFbound_box(&r->IFS, BBox, &VLCPower));
    BBox[0] >>= VLCPower;
    BBox[1] >>= VLCPower;
    BBox[2] >>= VLCPower;
    BBox[3] >>= VLCPower;
    return 0;
}

private FAPI_retcode can_retrieve_char_by_name(FAPI_server *server, FAPI_font *ff, int *result)
{   
    #if 0 /* UFST bug : it can retrive Type 1 by char name, but it needs char code as cache key. */
        fapi_ufst_server *r = If_to_I(server);
        *result = (r->fc.format & FC_FONTTYPE_MASK) == FC_PST1_TYPE;
    #else
        *result = 0;
    #endif
    return 0;
}

private void release_glyphs(fapi_ufst_server *r, ufst_common_font_data *d)
{   while (d->glyphs != 0) {
        pcleo_glyph_list_elem *e = d->glyphs;
        d->glyphs = e->next;
        r->client_mem.free(&r->client_mem, e, "PCLEO char");
    }
}

private FAPI_retcode get_char_width(FAPI_server *server, FAPI_font *ff, FAPI_char_ref *c, FracInt *wx, FracInt *wy)
{   fapi_ufst_server *r = If_to_I(server);
    UW16 buffer[2];
    UW16 code = CGIFwidth(&r->IFS, (UW16)c->char_code, 1, 4, buffer);
    release_glyphs(r, (ufst_common_font_data *)ff->server_font_data);
    *wx = (int)((double)buffer[0] * r->tran_xx / buffer[1] / 72.0 * r->fc.s.m2.xworld_res);
    *wy = (int)((double)buffer[0] * r->tran_xy / buffer[1] / 72.0 * r->fc.s.m2.yworld_res);
    return code;
}

private FAPI_retcode get_char_raster_metric(FAPI_server *server, FAPI_font *ff, FAPI_char_ref *c, FAPI_metrics *metrics)
{   fapi_ufst_server *r = If_to_I(server);
    IFBITMAP *pbm;
    int code;
    UW16 cc = (UW16)c->char_code;
    char PSchar_name[30];
    int len = min(sizeof(PSchar_name) - 1, c->char_name_length);
    memcpy(PSchar_name, c->char_name, len);
    PSchar_name[len] = 0;
    r->fc.ssnum = (c->is_glyph_index ? RAW_GLYPH : 0x8000 /* no symset mapping */);
    CheckRET(CGIFchIdptr(&r->IFS, &cc, PSchar_name));
    r->fc.format &= ~FC_OUTPUT_MASK; /* set bitmap format output */
    r->ff = ff;
    r->callback_error = 0;
    renderer = r;
    code = CGIFchar(&r->IFS, cc, &r->pbm, (SW16)0);
    r->ff = 0;
    release_glyphs(r, (ufst_common_font_data *)ff->server_font_data);
    pbm = r->pbm;
    CheckRET(r->callback_error);
    if (code != ERR_fixed_space)
	CheckRET(code);
    metrics->esc_x = (int)((double)pbm->escapement * r->tran_xx / pbm->du_emx / 72.0 * r->fc.s.m2.xworld_res);
    metrics->esc_y = (int)((double)pbm->escapement * r->tran_xy / pbm->du_emy / 72.0 * r->fc.s.m2.yworld_res);
    if (code == ERR_fixed_space) {
	metrics->bbox_x0 = metrics->bbox_y0 = 0;
	metrics->bbox_x1 = metrics->bbox_y1 = -1;
	metrics->orig_x = metrics->orig_y = 0;
	metrics->lsb_x = metrics->lsb_y = 0;
	metrics->tsb_x = metrics->tsb_y = 0;
	return 0;
    }
    metrics->bbox_x0 = pbm->left_indent;
    metrics->bbox_y0 = pbm->top_indent;
    metrics->bbox_x1 = pbm->left_indent + pbm->black_width;
    metrics->bbox_y1 = pbm->top_indent + pbm->black_depth;
    metrics->orig_x = pbm->left_indent * 16 + pbm->xorigin;
    metrics->orig_y = pbm->top_indent  * 16 + pbm->yorigin;
    metrics->lsb_x = metrics->orig_x;
    metrics->lsb_y = 0;
    metrics->tsb_x = metrics->orig_x + pbm->black_width / 2;
    metrics->tsb_y = 0; /* pbm->topbearing; */
    return 0;
    /*	UFST cannot render enough metrics information
	without generating raster or outline. 
	If Agfa improves UFST API, this code to be optimized.
    */
}

private int export_outline(fapi_ufst_server *r, PIFOUTLINE pol, FAPI_path *p)
{   POUTLINE_CHAR outchar;
    SW16 num_contrs,num_segmts;
    LPSB8 segment;
    PINTRVECTOR points;
    SW16  i,j;
    p->shift += r->If.frac_shift + pol->VLCpower;
    outchar = &pol->ol;
    num_contrs = outchar->num_loops;
    for(i=0; i<num_contrs; i++) {
     	num_segmts = outchar->loop[i].num_segmts;
        segment = (LPSB8)((LPSB8)(outchar->loop) + outchar->loop[i].segmt_offset);
        points = (PINTRVECTOR)((LPSB8)(outchar->loop) + outchar->loop[i].coord_offset);
        for(j=0; j<num_segmts; j++) {
            if(*segment == 0x00) {
             	CheckRET(p->moveto(p, points->x, points->y));
                points++;
            } else if (*segment == 0x01) {
		CheckRET(p->lineto(p, points->x, points->y));
                points++;
            } else if (*segment == 0x02) {
                points+=2;
                return -10; /* gs_error_invalidfont */
             	/* This must not happen */
            } else if (*segment == 0x03) {
		CheckRET(p->curveto(p, points[0].x, points[0].y, points[1].x, points[1].y, points[2].x, points[2].y));
                points+=3;
            } else {
                return -10; /* gs_error_invalidfont */
             	/* This must not happen */
	    }
            segment++;
        }
    }
    return 0;
}

private FAPI_retcode outline_char(FAPI_server *server, FAPI_font *ff, FAPI_char_ref *c, FAPI_path *p, FAPI_metrics *metrics)
{   fapi_ufst_server *r = If_to_I(server);
    HIFOUTLINE hol= NIL_MH;
    PIFOUTLINE pol;
    r->fc.format &= ~FC_OUTPUT_MASK;
    r->fc.format |= FC_CUBIC_TYPE;       /* "cubic outline" output */
    CheckRET(CGIFchar_handle(&r->IFS, (UW16)c->char_code, (PHIFOUTLINE)&hol, (SW16)0)); /* fixme : char_name */
    release_glyphs(r, (ufst_common_font_data *)ff->server_font_data);
    pol = (PIFOUTLINE)MEMptr(hol);
    if (pol != 0) {
	CheckRET(export_outline(r, pol, p));
	metrics->bbox_x0 = pol->left;
	metrics->bbox_y0 = pol->top;
	metrics->bbox_x1 = pol->right;
	metrics->bbox_y1 = pol->bottom;
	metrics->orig_x = 0;
	metrics->orig_y = 0;
	metrics->lsb_x = 0;
	metrics->lsb_y = 0;
	metrics->tsb_x = (pol->left + pol->right) / 2;
	metrics->tsb_y = 0; /* pbm->topbearing; */
    } else
	memset(&metrics, 0, sizeof(metrics));
    if(hol)
        CHARfree(&r->IFS, hol);
    return 0;
}

#if 0 /* This is iseful if renderer doesn't cache chars. */
private int export_bitmap(LPUB8 t, SW16 ww, SW16 d, FAPI_raster *r)
{   int i,j;
    int h = (d < r->height ? d : r->height);
    int w = (ww < r->width ? ww << CHUNK_SHIFT : r->width);
    LPCHUNK tchnk = (LPCHUNK)t;
    int line_step =  (ww + (1 << (CHUNK_SHIFT - 3)) - 1) >> (CHUNK_SHIFT - 3);
    int num_chunks = (w  + (1 << CHUNK_SHIFT) - 1) >> CHUNK_SHIFT;
    for (i = 0; i < h; i++) {
	LPCHUNK o = (LPCHUNK)((char *)r->p + r->line_step * i);
	LPCHUNK s = tchnk + line_step * i;
        for (j=0; j < num_chunks; j++)
            *o++ = *s++;
    }
    return 0;
}
#endif

private FAPI_retcode get_char_raster(FAPI_server *server, FAPI_font *ff, FAPI_char_ref *c, FAPI_raster *rast)
{   fapi_ufst_server *r = If_to_I(server);
    IFBITMAP *pbm = r->pbm;
    if (pbm == 0)
	return UFST_ERROR_memory; /* Can't get raster */
    /*  CheckRET(export_bitmap((LPUB8)pbm->bm, pbm->width, pbm->depth, r));
	Use this code if renderer doesn't provide glyph cache.
    */
    rast->p = pbm->bm;
    rast->height = pbm->depth;
    rast->width = pbm->width << CHUNK_SHIFT;
    rast->line_step = (pbm->width + (1 << (CHUNK_SHIFT - 3)) - 1) >> (CHUNK_SHIFT - 3);
    return 0;
}

private FAPI_retcode release_char_raster(FAPI_server *server, FAPI_font *ff, FAPI_char_ref *c)
{   /* UFST has no function to release cached character. */ 
    fapi_ufst_server *r = If_to_I(server);
    r->pbm = 0;
    return 0;
}

private void release_fco(fapi_ufst_server *r, SW16 fcHandle)
{   fco_list_elem **e = &r->fco_list;
    for (; *e != 0; )
        if ((*e)->fcHandle == fcHandle && (--(*e)->open_count) == 0) {
            fco_list_elem *ee = *e;
            *e = ee->next;
            CGIFfco_Close(&r->IFS, ee->fcHandle);
            r->client_mem.free(&r->client_mem, ee->file_path, "fco_file_path");
            r->client_mem.free(&r->client_mem, ee, "fco_list_elem");
        } else
            e = &(*e)->next;
}

private FAPI_retcode release_font_data(FAPI_server *server, void *font_data)
{   fapi_ufst_server *r = If_to_I(server);
    ufst_common_font_data *d;
    FAPI_retcode code;
    if (font_data == 0)
        return 0;
    d = (ufst_common_font_data *)font_data;
    if (d->is_disk_font)
        code = CGIFhdr_font_purge(&r->IFS, &r->fc);
    else
        code = CGIFfont_purge(&r->IFS, &r->fc);
    assert(code == 0); /* development purpose only */
    release_glyphs(r, d);
    release_fco(r, (SW16)(d->font_id >> 16));
    r->client_mem.free(&r->client_mem, font_data, "ufst font data");
    prepare_typeface(r, d);
    return code;
}

/* --------------------- The plugin definition : ------------------------- */


private void gs_fapiufst_finit(i_plugin_instance *instance, i_plugin_client_memory *mem);

private const i_plugin_descriptor ufst_descriptor = {
    "FAPI",
    "AgfaUFST",
    gs_fapiufst_finit
};

private const FAPI_server If0 = {
    {   &ufst_descriptor
    },
    16, /* frac_shift */
    1, /* server_caches_chars */
    ensure_open,
    get_scaled_font,
    get_decodingID,
    get_font_bbox,
    can_retrieve_char_by_name,
    outline_char,
    get_char_width,
    get_char_raster_metric,
    get_char_raster,
    release_char_raster,
    release_font_data
};

plugin_instantiation_proc(gs_fapiufst_instantiate);      /* check prototype */

int gs_fapiufst_instantiate(i_ctx_t *i_ctx_p, i_plugin_client_memory *client_mem, i_plugin_instance **p_instance)
{   fapi_ufst_server *I = (fapi_ufst_server *)client_mem->alloc(client_mem, sizeof(fapi_ufst_server), "fapi_ufst_server");
    if (I == 0)
        return e_Fatal;
    memset(I, 0, sizeof(*I));
    I->If = If0;
    I->client_mem = *client_mem;
    *p_instance = &I->If.ig;
    return 0;
}

private void gs_fapiufst_finit(i_plugin_instance *this, i_plugin_client_memory *mem)
{   fapi_ufst_server *I = (fapi_ufst_server *)this;
    assert(I->If.ig.d == &ufst_descriptor); /* development purpose only */
    if (I->bInitialized) {
        CGIFexit(&I->IFS);
    }
    mem->free(mem, I, "fapi_ufst_server");
}

