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


/* BaseFont implementation for pdfwrite */
#include "memory_.h"
#include "ctype_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsutil.h"		/* for bytes_compare */
#include "gxfcid.h"
#include "gxfcopy.h"
#include "gxfont.h"		/* for gxfont42.h */
#include "gxfont42.h"
#include "gdevpsf.h"
#include "gdevpdfx.h"
#include "gdevpdfo.h"
#include "gdevpdtb.h"
#include "gdevpdtf.h"
#include "gdevpdtd.h"
#include "smd5.h"
#include "gxfcache.h"   /* for gs_purge_font_from_char_caches_completely */
/*
 * Adobe's Distiller Parameters documentation for Acrobat Distiller 5
 * says that all fonts other than Type 1 are always subsetted; the
 * documentation for Distiller 4 says that fonts other than Type 1 and
 * TrueType are subsetted.  We do the latter, except that we always
 * subset large TrueType fonts.
 */
#define MAX_NO_SUBSET_GLYPHS 4096	/* arbitrary */

/* ---------------- Private ---------------- */

private_st_pdf_base_font();
gs_private_st_basic(st_pdf_base_font, pdf_base_font_t, "pdf_base_font_t",\
                    pdf_base_font_ptrs, pdf_base_font_data);

typedef struct pdf_base14_font_info_s {
    const char *urwname;
    const char *stdname;
} pdf_base14_font_info_t;

static const pdf_base14_font_info_t base14_font_info[] = {
    /* Standard mapping of URW fonts */
    {"NimbusMonL-Regu",       "Courier"              },
    {"NimbusMonL-Bold",       "Courier-Bold"         },
    {"NimbusMonL-ReguObli",   "Courier-Oblique"      },
    {"NimbusMonL-BoldObli",   "Courier-BoldOblique"  },
    {"NimbusSanL-Regu",       "Helvetica"            },
    {"NimbusSanL-Bold",       "Helvetica-Bold"       },
    {"NimbusSanL-ReguItal",   "Helvetica-Oblique"    },
    {"NimbusSanL-BoldItal",   "Helvetica-BoldOblique"},
    {"StandardSymL",          "Symbol"               },
    {"NimbusRomNo9L-Regu",    "Times-Roman"          },
    {"NimbusRomNo9L-Medi",    "Times-Bold"           },
    {"NimbusRomNo9L-ReguItal","Times-Italic"         },
    {"NimbusRomNo9L-MediItal","Times-BoldItalic"     },
    {"Dingbats",              "ZapfDingbats"         },
    /* A few other mappings of URW fonts */
    {"NimbusMono-Reg",        "Courier"              },
    {"NimbusMono-Bol",        "Courier-Bold"         },
    {"NimbusMono-Ita",        "Courier-Oblique"      },
    {"NimbusMono-BolIta",     "Courier-BoldOblique"   },
    {"NimbusSan-Reg",         "Helvetica"            },
    {"NimbusSan-Bol",         "Helvetica-Bold"       },
    {"NimbusSan-Ita",         "Helvetica-Oblique"    },
    {"NimbusSan-BolIta",      "Helvetica-BoldOblique"},
    {"A030-Reg",              "Helvetica"            },
    {"A030-Bol",              "Helvetica-Bold"       },
    {"A030-Ita",              "Helvetica-Oblique"    },
    {"A030-BolIta",           "Helvetica-BoldOblique"},
    {"NimbusSanNo2-Reg",      "Helvetica"            },
    {"NimbusSanNo2-Bol",      "Helvetica-Bold"       },
    {"NimbusSanNo2-Ita",      "Helvetica-Oblique"    },
    {"NimbusSanNo2-BolIta",   "Helvetica-BoldOblique"},
    {"NimbusRomanNo4-Lig",    "Times-Roman"          },
    {"NimbusRomanNo4-Bol",    "Times-Bold"           },
    {"NimbusRomanNo4-LigIta", "Times-Italic"         },
    {"NimbusRomanNo4-BolIta", "Times-BoldItalic"     },
    {"NimbusRomanNo9-Reg",    "Times-Roman"          },
    {"NimbusRomanNo9-Med",    "Times-Bold"           },
    {"NimbusRomanNo9-Ita",    "Times-Italic"         },
    {"NimbusRomanNo9-MedIta", "Times-BoldItalic"     },
    {"NimbusRom-Reg",         "Times-Roman"          },
    {"NimbusRom-Med",         "Times-Bold"           },
    {"NimbusRom-Ita",         "Times-Italic"         },
    {"NimbusRom-MedIta",      "Times-BoldItalic"     },
    {"NimbusRomNo9-Reg",      "Times-Roman"          },
    {"NimbusRomNo9-Bol",      "Times-Bold"           },
    {"NimbusRomNo9-Ita",      "Times-Italic"         },
    {"NimbusRomNo9-MedIta",   "Times-BoldItalic"     },
    {0}
};

/* Given a pointer to a font name, return a pointer to an
 * equivalent base 14 font name, of NULL if there is no
 * equivalent.
 */
const char *pdf_find_base14_name(const byte *str, uint size)
{
    const pdf_base14_font_info_t *ppsf;

    for (ppsf = base14_font_info; ppsf->urwname; ++ppsf) {
        if (strlen(ppsf->urwname) == size) {
            if (!memcmp(ppsf->urwname, (const char *)str, size))
                return ppsf->stdname;
        }
    }
    return NULL;
}


/*
 * Determine whether a font is a subset font by examining the name.
 */
bool
pdf_has_subset_prefix(const byte *str, uint size)
{
    int i;

    if (size < SUBSET_PREFIX_SIZE || str[SUBSET_PREFIX_SIZE - 1] != '+')
        return false;
    for (i = 0; i < SUBSET_PREFIX_SIZE - 1; ++i)
        if ((uint)(str[i] - 'A') >= 26)
            return false;
    return true;
}

static inline ulong
hash(ulong v, int index, ushort w)
{
    return v * 3141592653u + w;
}

/*
 * Add the XXXXXX+ prefix for a subset font.
 */
int
pdf_add_subset_prefix(const gx_device_pdf *pdev, gs_string *pstr, byte *used, int count, char *md5_hash)
{
    uint size = pstr->size;
    byte *data = gs_resize_string(pdev->pdf_memory, pstr->data, size,
                                  size + SUBSET_PREFIX_SIZE,
                                  "pdf_add_subset_prefix");
    int len = (count + 7) / 8;
    int len0 = len & ~(sizeof(ushort) - 1);
    ulong v = 0;
    int i;

    if (data == 0)
        return_error(gs_error_VMerror);

    if (md5_hash) {
        for (i = 0; i < 8; i += sizeof(ushort)) {
	    v = hash(v, i, (*(md5_hash + i) | *(md5_hash + i + 1) << 8));
        }
    }

    /* Hash the 'used' array. */
    for (i = 0; i < len0; i += sizeof(ushort))
        v = hash(v, i, *(ushort *)(used + i));
    for (; i < len; i++)
        v = hash(v, i, used[i]);

    memmove(data + SUBSET_PREFIX_SIZE, data, size);
    for (i = 0; i < SUBSET_PREFIX_SIZE - 1; ++i, v /= 26)
        data[i] = 'A' + (v % 26);
    data[SUBSET_PREFIX_SIZE - 1] = '+';
    pstr->data = data;
    pstr->size = size + SUBSET_PREFIX_SIZE;
    return 0;
}

/* Finish writing FontFile* data. */
static int
pdf_end_fontfile(gx_device_pdf *pdev, pdf_data_writer_t *pdw)
{
    /* We would like to call pdf_end_data,
       but we don't want to write the object to the output file now. */
    return pdf_close_aside(pdw->pdev);
}

static int copied_font_notify(void *proc_data, void *event_data)
{
    return gs_purge_font_from_char_caches_completely((gs_font *)proc_data);
}

/* ---------------- Public ---------------- */
/*
 * Allocate and initialize a base font structure, making the required
 * stable copy/ies of the gs_font.  Note that this removes any XXXXXX+
 * font name prefix from the copy.  If is_standard is true, the copy is
 * a complete one, and adding glyphs or Encoding entries is not allowed.
 */
int
pdf_base_font_alloc(gx_device_pdf *pdev, pdf_base_font_t **ppbfont,
                    gs_font_base *font, const gs_matrix *orig_matrix,
                    bool is_standard)
{
    gs_memory_t *mem = pdev->pdf_memory;
    gs_font *copied;
    gs_font *complete;
    pdf_base_font_t *pbfont =
        gs_alloc_struct(mem, pdf_base_font_t,
                        &st_pdf_base_font, "pdf_base_font_alloc");
    const gs_font_name *pfname = &font->font_name;
    gs_const_string font_name;
    char fnbuf[2*sizeof(long) + 3]; /* .F########\0 */
    int code, reserve_glyphs = -1;

    if (pbfont == 0)
        return_error(gs_error_VMerror);
    memset(pbfont, 0, sizeof(*pbfont));
    switch (font->FontType) {
    case ft_encrypted:
    case ft_encrypted2:
        {
            int index, count;
            gs_glyph glyph;

            for (index = 0, count = 0;
                (font->procs.enumerate_glyph((gs_font *)font, &index,
                                              GLYPH_SPACE_NAME, &glyph),
                  index != 0);
                 )
                    ++count;
            pbfont->num_glyphs = count;
            pbfont->do_subset = (is_standard ? DO_SUBSET_NO : DO_SUBSET_UNKNOWN);
        }
        /* If we find an excessively large type 1 font we won't be able to emit
         * a complete copy. Instead we will emit multiple subsets. Detect that here
         * and only reserve enough space in the font copy for the maximum subset
         * glyphs, 257.
         * This also prevents us making a 'complete' copy of the font below. NB the
         * value 2048 is merely a guess, intended to prevent copying very large fonts.
         */
        if(pbfont->num_glyphs > 2048 && !is_standard) {
            reserve_glyphs = 257;
            if(pbfont->do_subset != DO_SUBSET_NO){
                char buf[gs_font_name_max + 1];
                int l = min(font->font_name.size, sizeof(buf) - 1);

                memcpy(buf, font->font_name.chars, l);
                buf[l] = 0;
                emprintf1(pdev->memory,
                          "Can't embed the complete font %s as it is too large, embedding a subset.\n",
                          buf);
            }
        }
        break;
    case ft_TrueType:
        pbfont->num_glyphs = ((gs_font_type42 *)font)->data.trueNumGlyphs;
        pbfont->do_subset =
            (pbfont->num_glyphs <= MAX_NO_SUBSET_GLYPHS ?
             DO_SUBSET_UNKNOWN : DO_SUBSET_YES);
        break;
    case ft_CID_encrypted:
        pbfont->num_glyphs = ((gs_font_cid0 *)font)->cidata.common.CIDCount;
        goto cid;
    case ft_CID_TrueType:
        pbfont->num_glyphs = ((gs_font_cid2 *)font)->cidata.common.CIDCount;
    cid:
        pbfont->do_subset = DO_SUBSET_YES;
        pbfont->CIDSet =
            gs_alloc_bytes(mem, (pbfont->num_glyphs + 7) / 8,
                           "pdf_base_font_alloc(CIDSet)");
        if (pbfont->CIDSet == 0) {
            code = gs_note_error(gs_error_VMerror);
            goto fail;
        }
        pbfont->CIDSetLength = (pbfont->num_glyphs + 7) / 8;
        memset(pbfont->CIDSet, 0, (pbfont->num_glyphs + 7) / 8);
        break;
    default:
        code = gs_note_error(gs_error_rangecheck);
        goto fail;
    }

    code = gs_copy_font((gs_font *)font, orig_matrix, mem, &copied, reserve_glyphs);
    if (code < 0)
        goto fail;
    gs_notify_register(&copied->notify_list, copied_font_notify, copied);
    {
        /*
         * Adobe Technical Note # 5012 "The Type 42 Font Format Specification" says :
         *
         * There is a known bug in the TrueType rasterizer included in versions of the
         * PostScript interpreter previous to version 2013. The problem is that the
         * translation components of the FontMatrix, as used as an argument to the
         * definefont or makefont operators, are ignored. Translation of user space is
         * not affected by this bug.
         *
         * Besides that, we found that Adobe Acrobat Reader 4 and 5 ignore
         * FontMatrix.ty .
         */
        copied->FontMatrix.tx = copied->FontMatrix.ty = 0;
    }

    if (pbfont->do_subset != DO_SUBSET_YES && reserve_glyphs == -1) {
        /* The only possibly non-subsetted fonts are Type 1/2 and Type 42. */
        if (is_standard)
            complete = copied;
        else {
            code = gs_copy_font((gs_font *)font, &font->FontMatrix, mem, &complete, -1);
            if (code < 0) {
                gs_free_copied_font(copied);
                goto fail;
            }
        }
        code = gs_copy_font_complete((gs_font *)font, complete);
        if (code < 0 && pbfont->do_subset == DO_SUBSET_NO) {
            char buf[gs_font_name_max + 1];
            int l = min(copied->font_name.size, sizeof(buf) - 1);

            memcpy(buf, copied->font_name.chars, l);
            buf[l] = 0;
            emprintf1(pdev->memory,
                      "Can't embed the complete font %s due to font error.\n",
                      buf);
            gs_free_copied_font(copied);
            copied = NULL;
            goto fail;
        }
        if (code < 0) {
            /* A font error happened, but it may be caused by a glyph,
               which is not used in the document. Continue with subsetting the font.
               If the failed glyph will be used in the document,
               another error will hgappen when the glyph is used.
             */
            gs_free_copied_font(complete);
            complete = copied;
        }
    } else
        complete = copied;
    pbfont->copied = (gs_font_base *)copied;
    pbfont->complete = (gs_font_base *)complete;

    /* replace the font cache of the copied fonts with our own font cache
     * this is required for PCL, see 'pdf_free_pdf_font_cache' in gdevpdf.c
     * for further details.
     */
    pdev->pdf_font_dir->global_glyph_code = font->dir->global_glyph_code;

    pbfont->copied->dir = pbfont->complete->dir = pdev->pdf_font_dir;

    if (pbfont->copied->FontType == ft_CID_encrypted) {
        gs_font_cid0 *copied0 = (gs_font_cid0 *)pbfont->copied;
        int i;
        for (i = 0; i < copied0->cidata.FDArray_size; ++i) {
            ((gs_font *)copied0->cidata.FDArray[i])->dir = pdev->pdf_font_dir;
        }
    }

    pbfont->is_standard = is_standard;
    if (pfname->size > 0) {
        font_name.data = pfname->chars;
        font_name.size = pfname->size;
        while (pdf_has_subset_prefix(font_name.data, font_name.size)) {
            /* Strip off an existing subset prefix. */
            font_name.data += SUBSET_PREFIX_SIZE;
            font_name.size -= SUBSET_PREFIX_SIZE;
        }
    } else {
        gs_snprintf(fnbuf, sizeof(fnbuf), "Anonymous");
        font_name.data = (byte *)fnbuf;
        font_name.size = strlen(fnbuf);
    }
    pbfont->font_name.data =
        gs_alloc_string(mem, font_name.size, "pdf_base_font_alloc(font_name)");
    if (pbfont->font_name.data == 0)
        goto fail;
    memcpy(pbfont->font_name.data, font_name.data, font_name.size);
    pbfont->font_name.size = font_name.size;
    *ppbfont = pbfont;
    return 0;
 fail:
    pdf_base_font_free(pdev, pbfont);
    return code;
}

/*
 * Return a reference to the name of a base font.  This name is guaranteed
 * not to have a XXXXXX+ prefix.  The client may change the name at will,
 * but must not add a XXXXXX+ prefix.
 */
gs_string *
pdf_base_font_name(pdf_base_font_t *pbfont)
{
    return &pbfont->font_name;
}

/*
 * Return the (copied, subset) font associated with a base font.
 * This procedure probably shouldn't exist....
 */
gs_font_base *
pdf_base_font_font(const pdf_base_font_t *pbfont, bool complete)
{
    return (complete ? pbfont->complete : pbfont->copied);
}

/*
 * Check for subset font.
 */
bool
pdf_base_font_is_subset(const pdf_base_font_t *pbfont)
{
    return pbfont->do_subset == DO_SUBSET_YES;
}

/*
 * Drop the copied complete font associated with a base font.
 */
void
pdf_base_font_drop_complete(pdf_base_font_t *pbfont)
{
    /* gs_font is a subset of gs_font_base and we only want to
     * free the members which are common to both, so this cast is
     * (at the time of writing) safe.
     */
    if (pbfont->complete != pbfont->copied)
        gs_free_copied_font((gs_font *)pbfont->complete);
    pbfont->complete = NULL;
}

/*
 * Copy a glyph (presumably one that was just used) into a saved base
 * font.  Note that it is the client's responsibility to determine that
 * the source font is compatible with the target font.  (Normally they
 * will be the same.)
 */
int
pdf_base_font_copy_glyph(pdf_base_font_t *pbfont, gs_glyph glyph,
                         gs_font_base *font)
{
    int code;

    /* If we're a TrueType CIDFont, check teh GSUB table for replacement glyphs.
     * Bug #691574
     */
    if (font->FontType == ft_CID_TrueType) {
        code =
            gs_copy_glyph_options((gs_font *)font, glyph,
                              (gs_font *)pbfont->copied,
                              (pbfont->is_standard ? COPY_GLYPH_NO_NEW : COPY_GLYPH_USE_GSUB));
    } else {
        code =
            gs_copy_glyph_options((gs_font *)font, glyph,
                              (gs_font *)pbfont->copied,
                              (pbfont->is_standard ? COPY_GLYPH_NO_NEW : 0));
    }
    if (code < 0)
        return code;
    if (pbfont->CIDSet != 0 &&
        (uint)(glyph - GS_MIN_CID_GLYPH) < pbfont->num_glyphs
        ) {
        uint cid = glyph - GS_MIN_CID_GLYPH;

        pbfont->CIDSet[cid >> 3] |= 0x80 >> (cid & 7);
    }
    return 0;
}

/*
 * Determine whether a font should be subsetted.
 */
bool
pdf_do_subset_font(gx_device_pdf *pdev, pdf_base_font_t *pbfont, gs_id rid)
{
    gs_font_base *copied = pbfont->copied;

    /*
     * If the decision has not been made already, determine whether or not
     * to subset the font.
     */
    if (pbfont->do_subset == DO_SUBSET_UNKNOWN) {
        int max_pct = pdev->params.MaxSubsetPct;
        bool do_subset = pdev->params.SubsetFonts && max_pct > 0;

        if (do_subset && max_pct < 100) {
            /* We want to subset iff used <= total * MaxSubsetPct / 100. */
            do_subset = false;
            if (max_pct > 0) {
                int max_subset_used = pbfont->num_glyphs * max_pct / 100;
                int used, index;
                gs_glyph ignore_glyph;

                do_subset = true;
                for (index = 0, used = 0;
                     (copied->procs.enumerate_glyph((gs_font *)copied,
                                                &index, GLYPH_SPACE_INDEX,
                                                &ignore_glyph), index != 0);
                     )
                    if (++used > max_subset_used) {
                        do_subset = false;
                        break;
                    }
            }
        }
        pbfont->do_subset = (do_subset ? DO_SUBSET_YES : DO_SUBSET_NO);
    }
    return (pbfont->do_subset == DO_SUBSET_YES);
}

/*
 * Write the FontFile entry for an embedded font, /FontFile<n> # # R.
 */
int
pdf_write_FontFile_entry(gx_device_pdf *pdev, pdf_base_font_t *pbfont)
{
    stream *s = pdev->strm;
    const char *FontFile_key;

    switch (pbfont->copied->FontType) {
    case ft_TrueType:
    case ft_CID_TrueType:
        FontFile_key = "/FontFile2";
        break;
    default:			/* Type 1/2, CIDFontType 0 */
        if (!pdev->HaveCFF)
            FontFile_key = "/FontFile";
        else
            FontFile_key = "/FontFile3";
    }
    stream_puts(s, FontFile_key);
    pprintld1(s, " %ld 0 R", pbfont->FontFile->id);
    return 0;
}

/*
 * Adjust font name for Acrobat Reader 3.
 */
static int
pdf_adjust_font_name(gx_device_pdf *pdev, int64_t id, pdf_base_font_t *pbfont)
{
    /*
     * In contradiction with previous version of pdfwrite,
     * this always adds a suffix. We don't check the original name
     * for uniquity bacause the layered architecture
     * (see gdevpdtx.h) doesn't provide an easy access for
     * related information.
     */
    int i;
    byte *chars = (byte *)pbfont->font_name.data; /* break 'const' */
    byte *data;
    uint size = pbfont->font_name.size;
    char suffix[sizeof(int64_t) * 2 + 2];
    uint suffix_size;

#define SUFFIX_CHAR '~'
    /*
     * If the name looks as though it has one of our unique suffixes,
     * remove the suffix.
     */
    for (i = size;
         i > 0 && isxdigit(chars[i - 1]);
         --i)
        DO_NOTHING;
    if (i < size && i > 0 && chars[i - 1] == SUFFIX_CHAR) {
        do {
            --i;
        } while (i > 0 && chars[i - 1] == SUFFIX_CHAR);
        size = i + 1;
    }
    /* Create a unique name. */
    gs_snprintf(suffix, sizeof(suffix), "%c%lx", SUFFIX_CHAR, id);
    suffix_size = strlen(suffix);
    data = gs_resize_string(pdev->pdf_memory, chars, size,
                                  size + suffix_size,
                                  "pdf_adjust_font_name");
    if (data == 0)
        return_error(gs_error_VMerror);
    memcpy(data + size, (const byte *)suffix, suffix_size);
    pbfont->font_name.data = data;
    pbfont->font_name.size = size + suffix_size;
#undef SUFFIX_CHAR
    return 0;
}

/*
 * Write an embedded font.
 */
int
pdf_write_embedded_font(gx_device_pdf *pdev, pdf_base_font_t *pbfont, font_type FontType,
                        gs_int_rect *FontBBox, gs_id rid, cos_dict_t **ppcd)
{
    bool do_subset = pdf_do_subset_font(pdev, pbfont, rid);
    gs_font_base *out_font =
        (do_subset || pbfont->complete == NULL ? pbfont->copied : pbfont->complete);
    gs_const_string fnstr;
    pdf_data_writer_t writer;
    byte digest[6] = {0,0,0,0,0,0};
    int code = 0;
    int options=0;

    if (pbfont->written)
        return 0;		/* already written */
    code = copied_order_font((gs_font *)out_font);
    if (code < 0)
        return code;
    /* Since we now always ASCIIHex encode the eexec encrypted portion of a
     * Type 1 font, such a font cannot contain any binary data, if its not being
     * compressed then there is no reason to ASCII encode it (which will happen
     * we set DATA_STREAM_BINARY and the device does not permit binary output)
     * NB if HaveCFF is true then we convert type 1 to CFF which is a binary
     * format, so we still need to set DATA_STREAM_BINARY.
     */
    if (pdev->CompressFonts)
        options = DATA_STREAM_BINARY | DATA_STREAM_COMPRESS;
    else
         if (FontType != ft_encrypted || pdev->HaveCFF)
            options = DATA_STREAM_BINARY;
    /* Don't set DATA_STREAM_ENCRYPT since we write to a temporary file.
     * See comment in pdf_begin_encrypt.
     */
    code = pdf_begin_data_stream(pdev, &writer, options, 0);
    if (code < 0)
        return code;
    if (pdev->PDFA != 0) {
        stream *s = s_MD5C_make_stream(pdev->pdf_memory, writer.binary.strm);

        if (s == NULL)
            return_error(gs_error_VMerror);
        writer.binary.strm = s;
    }
    if (pdev->CompatibilityLevel == 1.2 &&
        !do_subset && !pbfont->is_standard ) {
        /*
         * Due to a bug in Acrobat Reader 3, we need to generate
         * unique font names, except base 14 fonts being not embedded.
         * To recognize base 14 fonts here we used the knowledge
         * that pbfont->is_standard is true for base 14 fonts only.
         * Note that subsetted fonts already have an unique name
         * due to subset prefix.
         */
         int code = pdf_adjust_font_name(pdev, writer.pres->object->id, pbfont);
         if (code < 0)
            return code;
    }
    fnstr.data = pbfont->font_name.data;
    fnstr.size = pbfont->font_name.size;
    /* Now write the font (or subset). */
    switch (FontType) {

    case ft_composite:
        /* Nothing to embed -- the descendant fonts do it all. */
        code = 0;
        break;

    case ft_encrypted2:
        if (!pdev->HaveCFF) {
            /* Must convert to Type 1 charstrings. */
            return_error(gs_error_unregistered); /* Not implemented yet. */
        }
    case ft_encrypted:
        if (pdev->HavePDFWidths) {
            code = copied_drop_extension_glyphs((gs_font *)out_font);
            if (code < 0)
                return code;
        }
        if (!pdev->HaveCFF) {
            /* Write the type 1 font with no converting to CFF. */
            int lengths[3];

            if (pbfont->do_subset != DO_SUBSET_NO)
                code = psf_write_type1_font(writer.binary.strm,
                                (gs_font_type1 *)out_font,
                                WRITE_TYPE1_WITH_LENIV | WRITE_TYPE1_EEXEC |
                                WRITE_TYPE1_EEXEC_PAD | WRITE_TYPE1_ASCIIHEX,
                                NULL, 0, &fnstr, lengths);
            else
                code = psf_write_type1_font(writer.binary.strm,
                                (gs_font_type1 *)out_font,
                                WRITE_TYPE1_WITH_LENIV | WRITE_TYPE1_EEXEC |
                                WRITE_TYPE1_EEXEC_PAD | WRITE_TYPE1_ASCIIHEX | WRITE_TYPE1_XUID,
                                NULL, 0, &fnstr, lengths);
            if (lengths[0] > 0) {
                if (code < 0)
                    goto finish;
                code = cos_dict_put_c_key_int((cos_dict_t *)writer.pres->object,
                            "/Length1", lengths[0]);
            }
            if (lengths[1] > 0) {
                if (code < 0)
                    goto finish;
                code = cos_dict_put_c_key_int((cos_dict_t *)writer.pres->object,
                            "/Length2", lengths[1]);
                if (code < 0)
                    return code;
                code = cos_dict_put_c_key_int((cos_dict_t *)writer.pres->object,
                            "/Length3", lengths[2]);
            }
        } else {
            /*
             * Since we only support PDF 1.2 and later, always write Type 1
             * fonts as Type1C (Type 2).  Acrobat Reader apparently doesn't
             * accept CFF fonts with Type 1 CharStrings, so we need to convert
             * them.  Also remove lenIV, so Type 2 fonts will compress better.
             */
#define TYPE2_OPTIONS (WRITE_TYPE2_NO_LENIV | WRITE_TYPE2_CHARSTRINGS)
            code = cos_dict_put_string_copy((cos_dict_t *)writer.pres->object, "/Subtype", "/Type1C");
            if (code < 0)
                return code;
            code = psf_write_type2_font(writer.binary.strm,
                                        (gs_font_type1 *)out_font,
                                        TYPE2_OPTIONS |
                            (pdev->CompatibilityLevel < 1.3 ? WRITE_TYPE2_AR3 : 0) |
                            (pbfont->do_subset == DO_SUBSET_NO ? WRITE_TYPE2_XUID : 0),
                                        NULL, 0, &fnstr, FontBBox);
        }
        goto finish;

    case ft_TrueType: {
        gs_font_type42 *const pfont = (gs_font_type42 *)out_font;
#define TRUETYPE_OPTIONS (WRITE_TRUETYPE_NAME | WRITE_TRUETYPE_HVMTX)
        /* Acrobat Reader 3 doesn't handle cmap format 6 correctly. */
        const int options = TRUETYPE_OPTIONS |
            (pdev->PDFA != 0 ? WRITE_TRUETYPE_UNICODE_CMAP : 0) |
            (pdev->CompatibilityLevel <= 1.2 ?
             WRITE_TRUETYPE_NO_TRIMMED_TABLE : 0) |
            /* Generate a cmap only for incrementally downloaded fonts
               and for subsetted fonts. */
            (pfont->data.numGlyphs != pfont->data.trueNumGlyphs ||
             pbfont->do_subset == DO_SUBSET_YES ?
             WRITE_TRUETYPE_CMAP : 0);
        stream poss;

        if (pdev->HavePDFWidths) {
            code = copied_drop_extension_glyphs((gs_font *)out_font);
            if (code < 0)
                return code;
        }
        s_init(&poss, pdev->memory);
        swrite_position_only(&poss);
        code = psf_write_truetype_font(&poss, pfont, options, NULL, 0, &fnstr);
        if (code < 0)
            goto finish;
        code = cos_dict_put_c_key_int((cos_dict_t *)writer.pres->object, "/Length1", stell(&poss));
        if (code < 0)
            goto finish;
        code = psf_write_truetype_font(writer.binary.strm, pfont,
                                       options, NULL, 0, &fnstr);
        goto finish;
    }

    case ft_CID_encrypted:
        code = cos_dict_put_string_copy((cos_dict_t *)writer.pres->object, "/Subtype", "/CIDFontType0C");
        if (code < 0)
            return code;
        code = psf_write_cid0_font(writer.binary.strm,
                                   (gs_font_cid0 *)out_font, TYPE2_OPTIONS,
                                   NULL, 0, &fnstr);
        goto finish;

    case ft_CID_TrueType:
        /* CIDFontType 2 fonts don't use cmap, name, OS/2, or post. */
#define CID2_OPTIONS WRITE_TRUETYPE_HVMTX
        code = psf_write_cid2_font(writer.binary.strm,
                                   (gs_font_cid2 *)out_font,
                                   CID2_OPTIONS, NULL, 0, &fnstr);
    finish:
        if (pdev->PDFA != 0) {
            sflush(writer.binary.strm);
            s_MD5C_get_digest(writer.binary.strm, digest, sizeof(digest));
        }
        *ppcd = (cos_dict_t *)writer.pres->object;
        if (code < 0) {
            pdf_end_fontfile(pdev, &writer);
            pdf_obj_mark_unused(pdev, writer.pres->object->id);
            return code;
        }
        code = pdf_end_fontfile(pdev, &writer);
        break;

    default:
        code = gs_note_error(gs_error_rangecheck);
    }

    pbfont->written = true;
    return code;
}

/*
 * Write the CharSet for a subsetted font, as a PDF string.
 */
int
pdf_write_CharSet(gx_device_pdf *pdev, pdf_base_font_t *pbfont)
{
    stream *s = pdev->strm;
    gs_font_base *font = pbfont->copied;
    int index;
    gs_glyph glyph;

    stream_puts(s, "(");
    for (index = 0;
         (font->procs.enumerate_glyph((gs_font *)font, &index,
                                      GLYPH_SPACE_NAME, &glyph),
          index != 0);
         ) {
        gs_const_string gstr;
        int code = font->procs.glyph_name((gs_font *)font, glyph, &gstr);

        /* Don't include .notdef. */
        if (code >= 0 &&
            bytes_compare(gstr.data, gstr.size, (const byte *)".notdef", 7)
            )
            pdf_put_name(pdev, gstr.data, gstr.size);
    }
    stream_puts(s, ")");
    return 0;
}

/*
 * Write the CIDSet object for a subsetted CIDFont.
 */
int
pdf_write_CIDSet(gx_device_pdf *pdev, pdf_base_font_t *pbfont,
                 int64_t *pcidset_id)
{
    pdf_data_writer_t writer;
    int code;

    code = pdf_begin_data_stream(pdev, &writer,
                      DATA_STREAM_BINARY |
                      (pdev->CompressFonts ? DATA_STREAM_COMPRESS : 0),
                      gs_no_id);
    if (code < 0)
        return code;
    stream_write(writer.binary.strm, pbfont->CIDSet,
                 pbfont->CIDSetLength);
    code = pdf_end_data(&writer);
    if (code < 0)
        return code;
    *pcidset_id = pdf_resource_id(writer.pres);
    return 0;
}
/*
 * Check whether a base font is standard.
 */
bool
pdf_is_standard_font(pdf_base_font_t *bfont)
{   return bfont->is_standard;
}

void
pdf_set_FontFile_object(pdf_base_font_t *bfont, cos_dict_t *pcd)
{
    bfont->FontFile = pcd;
}
const cos_dict_t *
pdf_get_FontFile_object(pdf_base_font_t *bfont)
{
    return bfont->FontFile;
}
