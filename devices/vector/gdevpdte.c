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


/* Encoding-based (Type 1/2/42) text processing for pdfwrite. */

#include "math_.h"
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsutil.h"
#include "gxfcmap.h"
#include "gxfcopy.h"
#include "gxfont.h"
#include "gxfont0.h"
#include "gxfont0c.h"
#include "gxpath.h"		/* for getting current point */
#include "gxchar.h"     /* for gx_compute_text_oversampling & gx_lookup_cached_char */
#include "gxfcache.h"    /* for gx_lookup_fm_pair */
#include "gdevpsf.h"
#include "gdevpdfx.h"
#include "gdevpdfg.h"
#include "gdevpdfo.h"
#include "gdevpdtx.h"
#include "gdevpdtd.h"
#include "gdevpdtf.h"
#include "gdevpdts.h"
#include "gdevpdtt.h"

#include "gximage.h"
#include "gxcpath.h"

#include "gsfcmap.h"
#include "tessocr.h"

static int pdf_char_widths(gx_device_pdf *const pdev,
                            pdf_font_resource_t *pdfont, int ch,
                            gs_font_base *font,
                            pdf_glyph_widths_t *pwidths /* may be NULL */);
static int pdf_process_string(pdf_text_enum_t *penum, gs_string *pstr,
                               const gs_matrix *pfmat,
                               pdf_text_process_state_t *ppts,
                               const gs_glyph *gdata);

/*
 * Process a string with a simple gs_font.
 */
int
pdf_process_string_aux(pdf_text_enum_t *penum, gs_string *pstr,
                          const gs_glyph *gdata, const gs_matrix *pfmat,
                          pdf_text_process_state_t *ppts)
{
    gs_font_base *font = (gs_font_base *)penum->current_font;

    switch (font->FontType) {
    case ft_TrueType:
    case ft_encrypted:
    case ft_encrypted2:
    case ft_user_defined:
    case ft_PDF_user_defined:
    case ft_PCL_user_defined:
    case ft_GL2_stick_user_defined:
    case ft_GL2_531:
    case ft_MicroType:
        break;
    default:
        return_error(gs_error_rangecheck);
    }
    return pdf_process_string(penum, pstr, pfmat, ppts, gdata);
}

/* #define OCR_DUMP_BITMAP */
#undef OCR_DUMP_BITMAP

static int OCRText(gx_device_pdf *pdev, gs_glyph glyph, gs_char ch, gs_char *length, byte **unicode)
{
#if OCR_VERSION > 0
    int code = 0;

    if(pdev->OCRStage == OCR_Rendered) {
        int llx, lly, urx, ury, char_count = 0, returned_count = 0, *returned;
        ocr_glyph_t *next_glyph = pdev->ocr_glyphs;
        int rows, stride, row, column;
        byte *bitmap = NULL, *src, *dest, *rowptr, srcmask, destmask;
        void *state;
        const char *language = pdev->ocr_language;
#ifdef OCR_DUMP_BITMAP
        gp_file *DbgFile;
#endif

        if(language == NULL || language[0] == 0)
            language = "eng";

        /* We should alredy have rendered a bitmap for all the glyphs in the
         * text operation, so this shuld be redundant, but best to be safe.
         */
        if(next_glyph == NULL)
            return_error(gs_error_unknownerror);

        /* Identify the bounding box of the returned glyphs by examing the bounds and position
         * of each glyph. At the same time count the number of expected returned characters.
         * We treat any empty bitmap (all 0x00 bytes) as a space because, obviously, the
         * OCR engine can't tell differentiate between a space character and no character at all.
         */
        llx = next_glyph->x;
        lly = next_glyph->y;
        urx = llx + next_glyph->width;
        ury = lly + next_glyph->height;
        if(next_glyph != NULL && !next_glyph->is_space)
            char_count++;
        next_glyph = (ocr_glyph_t *)next_glyph->next;
        while(next_glyph) {
            if(!next_glyph->is_space)
                char_count++;
            if(next_glyph->x < llx)
                llx = next_glyph->x;
            if(next_glyph->y < lly)
                lly = next_glyph->y;
            if(next_glyph->x + next_glyph->width > urx)
                urx = next_glyph->x + next_glyph->width;
            if(next_glyph->y + next_glyph->height > ury)
                ury = next_glyph->y + next_glyph->height;
            next_glyph = next_glyph->next;
        }

        /* Allocate and initialise the 'strip' bitmap which will receive all the
         * individual glyph bitmaps.
         */
        rows = ury - lly;
        stride = (((urx - llx) + 7) / 8) + 1;
        bitmap = gs_alloc_bytes(pdev->memory, rows * stride, "working OCR memory");
        if(bitmap == NULL)
            return_error(gs_error_VMerror);
        memset(bitmap, 0x00, rows * stride);

        /* Allocate a buffer for the OCR engine to return the Unicode code points. This needs work,
         * we might want more information returned (bounding boxes and confidence levels) and we
         * need to think about the possibility that the OCR engine finds more character than we
         * expected (eg fi ligatures returned as 'f' and 'i'.
         */
        returned = (int *)gs_alloc_bytes(pdev->memory, char_count * sizeof(int), "returned unicodes");
        if(returned == NULL) {
            gs_free_object(pdev->memory, bitmap, "working OCR memory");
            return_error(gs_error_VMerror);
        }
        memset(returned, 0x00, char_count * sizeof(int));

        /* Now copy each glyph bitmap to the correct position in the strip. This is complicated
         * by the fact that bitmaps are monochrome pcaked into bytes and so the destination
         * may not be aligned on a byte boundary.
         */
        next_glyph = (ocr_glyph_t *)pdev->ocr_glyphs;
        while(next_glyph) {
            rowptr = bitmap + ((next_glyph->y - lly) * stride) + (int)floor((next_glyph->x - llx) / 8);
            for(row = 0;row < next_glyph->height;row++) {
                dest = rowptr + row * stride;
                src = next_glyph->data + (row * next_glyph->raster);
                destmask = 0x80 >> (next_glyph->x - llx) % 8;
                srcmask = 0x80;
                for(column = 0; column < next_glyph->width;column++) {
                    if(*src & srcmask) {
                        *dest = *dest | destmask;
                    }
                    srcmask = srcmask >> 1;
                    if(srcmask == 0) {
                        srcmask = 0x80;
                        src++;
                    }
                    destmask = destmask >> 1;
                    if(destmask == 0) {
                        destmask = 0x80;
                        dest++;
                    }
                }
            }
            next_glyph = next_glyph->next;
        }

#ifdef OCR_DUMP_BITMAP
        DbgFile = gp_fopen(pdev->memory, "d:/temp/bits.txt", "wb+");
        for(row = 0;row < rows;row++) {
            for(column = 0;column < stride;column++) {
                dest = bitmap + (row * stride);
                gp_fprintf(DbgFile, "%02x", dest[column]);
            }
            gp_fprintf(DbgFile, "\n");
        }
        gp_fclose(DbgFile);
#endif
        /* Initialise the OCR engine */
        code = ocr_init_api(pdev->memory->non_gc_memory, language,
            pdev->ocr_engine, &state);
        if(code < 0) {
            gs_free_object(pdev->memory, bitmap, "working OCR memory");
            gs_free_object(pdev->memory, returned, "returned unicodes");
            return 0;
        }
        returned_count = char_count;

        /* Pass our strip to the OCR engine */
        code = ocr_bitmap_to_unicodes(state,
            bitmap, 0, stride * 8, rows, stride,
            (int)pdev->HWResolution[0],
            (int)pdev->HWResolution[1],
            returned, &returned_count);

        /* and close the engine back down again */
        ocr_fin_api(pdev->memory->non_gc_memory, state);
        gs_free_object(pdev->memory, bitmap, "working OCR memory");

        if(code < 0) {
            pdev->OCRStage = OCR_Failed;
            gs_free_object(pdev->memory, returned, "returned unicodes");
            return code;
        }

        /* Future enhancement we should fall back to trying the individual bitmap here */
        if(returned_count != char_count) {
            pdev->OCRStage = OCR_Failed;
            gs_free_object(pdev->memory, returned, "returned unicodes");
            return 0;
        }
        pdev->OCRUnicode = returned;

        /* Actually perform OCR on the stored bitmaps */
        pdev->OCRStage = OCR_UnicodeAvailable;
    }

    if(pdev->OCRStage == OCR_UnicodeAvailable) {
        /* We've OCR'ed the bitmaps already, find the unicode value */
        ocr_glyph_t *new_glyph = (ocr_glyph_t *)pdev->ocr_glyphs;
        int ocr_index = 0;
        uint mask = 0xFF;
        int ix;
        char *u;

        /* Find the bitmap which matches the character/glyph we are processing */
        while(new_glyph) {
            if(new_glyph->char_code == ch || new_glyph->glyph == glyph) {
                ocr_glyph_t *g1 = pdev->ocr_glyphs;

                /* Spaces are handled specially, so just jump out now */
                if(new_glyph->is_space)
                    break;

                /* Otherwise, find all the bitmaps which lie to the left of the
                 * one we found (we are assuming for now that the returned
                 * Unicode values are left to right)
                 */
                while(g1) {
                    if(!g1->is_space) {
                        if(g1->x < new_glyph->x)
                            ocr_index++;
                    }
                    g1 = g1->next;
                }
                break;
            }
            new_glyph = new_glyph->next;
        }

        /* If we found a matching bitmap, get the corresponding unicode code point from
         * the stored values returned by the OCR engine.
         */
        if(new_glyph) {
            if(pdev->OCRUnicode[ocr_index] > 0xFFFF) {
                *unicode = (byte *)gs_alloc_bytes(pdev->memory, 2 * sizeof(ushort), "temporary Unicode array");
                if(*unicode == NULL)
                    return_error(gs_error_VMerror);
                u = (char *)(*unicode);
                if(new_glyph->is_space) {
                    memset(u, 0x00, 3);
                    u[3] = 0x20;
                }
                else{
                    for(ix = 0;ix < 4;ix++) {
                        u[3 - ix] = (pdev->OCRUnicode[ocr_index] & mask) >> (8 * ix);
                        mask = mask << 8;
                    }
                }
                *length = 4;
            }else{
                *unicode = (byte *)gs_alloc_bytes(pdev->memory, sizeof(ushort), "temporary Unicode array");
                if(*unicode == NULL)
                    return_error(gs_error_VMerror);
                u = (char *)(*unicode);
                if(new_glyph->is_space) {
                    memset(u, 0x00, 2);
                    u[1] = 0x20;
                }else{
                    u[0] = (pdev->OCRUnicode[ocr_index] & 0xFF00) >> 8;
                    u[1] = (pdev->OCRUnicode[ocr_index] & 0xFF);
                }
                *length = 2;
            }
        }
    }
    #endif
    return 0;
}

/*
 * Add char code pair to ToUnicode CMap,
 * creating the CMap on neccessity.
 */
int
pdf_add_ToUnicode(gx_device_pdf *pdev, gs_font *font, pdf_font_resource_t *pdfont,
                  gs_glyph glyph, gs_char ch, const gs_const_string *gnstr)
{   int code = 0;
    gs_char length = 0;
    ushort *unicode = 0;

    if (glyph == GS_NO_GLYPH)
        return 0;
    if(pdev->UseOCR == UseOCRAlways) {
        code = OCRText(pdev, glyph, ch, &length, (byte **)&unicode);
        if(code < 0)
            return code;
    }
    else {
        length = font->procs.decode_glyph((gs_font *)font, glyph, ch, NULL, 0);
        if(length == 0 || length == GS_NO_CHAR) {
            if(gnstr != NULL && gnstr->size == 7) {
                if(!memcmp(gnstr->data, "uni", 3)) {
                    static const char *hexdigits = "0123456789ABCDEF";
                    char *d0 = strchr(hexdigits, gnstr->data[3]);
                    char *d1 = strchr(hexdigits, gnstr->data[4]);
                    char *d2 = strchr(hexdigits, gnstr->data[5]);
                    char *d3 = strchr(hexdigits, gnstr->data[6]);

                    unicode = (ushort *)gs_alloc_bytes(pdev->memory, sizeof(ushort), "temporary Unicode array");
                    if(d0 != NULL && d1 != NULL && d2 != NULL && d3 != NULL) {
                        char *u = (char *)unicode;
                        u[0] = ((d0 - hexdigits) << 4) + ((d1 - hexdigits));
                        u[1] = ((d2 - hexdigits) << 4) + ((d3 - hexdigits));
                        length = 2;
                    }
                }
            }
            else {
                if(pdev->UseOCR != UseOCRNever) {
                    code = OCRText(pdev, glyph, ch, &length, (byte **)&unicode);
                    if(code < 0)
                        return code;
                }
            }
        }
    }

    if (length != 0 && length != GS_NO_CHAR) {
        if (pdfont->cmap_ToUnicode == NULL) {
            /* ToUnicode CMaps are always encoded with two byte keys. See
             * Technical Note 5411, 'ToUnicode Mapping File Tutorial'
             * page 3.
             */
            /* Unfortunately, the above is not true. See the PDF Reference (version 1.7
             * p 472 'ToUnicode CMaps'. Even that documentation is incorrect as it
             * describes codespaceranges, in fact for Acrobat this is irrelevant,
             * but the bfranges must be one byte for simple fonts. By altering the
             * key size for CID fonts we can write both consistently correct.
             */
            uint num_codes = 256, key_size = 1;

            if (font->FontType == ft_CID_encrypted) {
                gs_font_cid0 *pfcid = (gs_font_cid0 *)font;

                num_codes = pfcid->cidata.common.CIDCount;
                key_size = 2;
            } else if (font->FontType == ft_CID_TrueType || font->FontType == ft_composite) {
                key_size = 2;
                /* Since PScript5.dll creates GlyphNames2Unicode with character codes
                   instead CIDs, and with the WinCharSetFFFF-H2 CMap
                   character codes appears from the range 0-0xFFFF (Bug 687954),
                   we must use the maximal character code value for the ToUnicode
                   code count. */
                num_codes = 65536;
            }
            code = gs_cmap_ToUnicode_alloc(pdev->pdf_memory, pdfont->rid, num_codes, key_size, length,
                                            &pdfont->cmap_ToUnicode);
            if (code < 0) {
                if (unicode)
                    gs_free_object(pdev->memory, unicode, "temporary Unicode array");
                return code;
            }
        } else {
            if (((gs_cmap_ToUnicode_t *)pdfont->cmap_ToUnicode)->value_size < length){
                gs_cmap_ToUnicode_realloc(pdev->pdf_memory, length, &pdfont->cmap_ToUnicode);
            }
        }

        if (!unicode) {
            unicode = (ushort *)gs_alloc_bytes(pdev->memory, length * sizeof(short), "temporary Unicode array");
            if (unicode == NULL)
                return_error(gs_error_VMerror);
            length = font->procs.decode_glyph((gs_font *)font, glyph, ch, unicode, length);
        }

        /* We use this when determining whether we should use an existing ToUnicode
         * CMap entry, for s aimple font. The basic problem appears to be that the front end ToUnicode
         * processing is somewhat limited, due to its origins in the PostScript GlyphNames2Unicode
         * handling. We cannot support more than 4 bytes on input, the bug for 702201 has a ToUnicode
         * entry which maps the single /ffi glyph to 'f' 'f' and 'i' code points for a total of 6
         * (3 x UTF-16BE code points) bytes. If we just leave the Encoding alone then Acrobat will use the
         * /ffi glyph to get the 'correct' answer. This was originally done using a 'TwoByteToUnicode'
         * flag in the font and if the flag was not true, then we dropped the entire ToUnicode CMap.
         *
         * Additionally; bug #708284, the ToUnicode CMap is actually broken, it contains invalid UTF-16BE
         * entries which actually are 4 bytes long. Acrobat silently ignores the broekn entires (!) but the fact
         * that we dropped the entire CMap meant that none of the content pasted correctly.
         *
         * So... Until we get to the point of preserving input codes in excess of 4 bytes we do some hideous
         * hackery here and simply drop ToUnicode entries in simple fonts, with a standard encoding, when the
         * ToUnicode entry is not a single UTF-16BE code point. Acrobat will use the Encoding for the missing entries
         * or the character code in extremis, which is as good as this is going to get right now.
         *
         */
        if (pdfont->cmap_ToUnicode != NULL) {
            if (pdfont->u.simple.Encoding != NULL) {
                if (length <= 2)
                    gs_cmap_ToUnicode_add_pair(pdfont->cmap_ToUnicode, ch, unicode, length);
            } else
                gs_cmap_ToUnicode_add_pair(pdfont->cmap_ToUnicode, ch, unicode, length);
        }
    }

    if (unicode)
        gs_free_object(pdev->memory, unicode, "temporary Unicode array");
    return 0;
}

typedef struct {
    gx_device_pdf *pdev;
    pdf_resource_type_t rtype;
} pdf_resource_enum_data_t;

static int
process_resources2(void *client_data, const byte *key_data, uint key_size, const cos_value_t *v)
{
    pdf_resource_enum_data_t *data = (pdf_resource_enum_data_t *)client_data;
    pdf_resource_t *pres = pdf_find_resource_by_resource_id(data->pdev, data->rtype, v->contents.object->id);

    if (pres == NULL)
        return_error(gs_error_unregistered); /* Must not happen. */
    pres->where_used |= data->pdev->used_mask;
    return 0;
}

static int
process_resources1(void *client_data, const byte *key_data, uint key_size, const cos_value_t *v)
{
    pdf_resource_enum_data_t *data = (pdf_resource_enum_data_t *)client_data;
    static const char *rn[] = {PDF_RESOURCE_TYPE_NAMES};
    int i;

    for (i = 0; i < count_of(rn); i++) {
        if (rn[i] != NULL && !bytes_compare((const byte *)rn[i], strlen(rn[i]), key_data, key_size))
            break;
    }
    if (i >= count_of(rn))
        return 0;
    data->rtype = i;
    return cos_dict_forall((cos_dict_t *)v->contents.object, data, process_resources2);
}

/*
 * Register charproc fonts with the page or substream.
 */
int
pdf_used_charproc_resources(gx_device_pdf *pdev, pdf_font_resource_t *pdfont)
{
    if (pdfont->where_used & pdev->used_mask)
        return 0;
    pdfont->where_used |= pdev->used_mask;
    if (pdev->CompatibilityLevel >= 1.2)
        return 0;
    if (pdfont->FontType == ft_user_defined ||
        pdfont->FontType == ft_PDF_user_defined ||
        pdfont->FontType == ft_PCL_user_defined ||
        pdfont->FontType == ft_MicroType ||
        pdfont->FontType == ft_GL2_stick_user_defined ||
        pdfont->FontType == ft_GL2_531) {
        pdf_resource_enum_data_t data;

        data.pdev = pdev;
        return cos_dict_forall(pdfont->u.simple.s.type3.Resources, &data, process_resources1);
    }
    return 0;
}

/*
 * Given a text string and a simple gs_font, return a font resource suitable
 * for the text string, possibly re-encoding the string.  This
 * may involve creating a font resource and/or adding glyphs and/or Encoding
 * entries to it.
 *
 * Sets *ppdfont.
 */
static int
pdf_encode_string_element(gx_device_pdf *pdev, gs_font *font, pdf_font_resource_t *pdfont,
                  gs_char ch, const gs_glyph *gdata)
{
    gs_font_base *cfont, *ccfont;
    int code;
    gs_glyph copied_glyph;
    gs_const_string gnstr;
    pdf_encoding_element_t *pet;
    gs_glyph glyph;

    /*
     * In contradiction with pre-7.20 versions of pdfwrite,
     * we never re-encode texts due to possible encoding conflict while font merging.
     */
    cfont = pdf_font_resource_font(pdfont, false);
    ccfont = pdf_font_resource_font(pdfont, true);
    pet = &pdfont->u.simple.Encoding[ch];
    glyph = (gdata == NULL ? font->procs.encode_char(font, ch, GLYPH_SPACE_NAME)
                           : *gdata);
    if (glyph == GS_NO_GLYPH || glyph == pet->glyph) {
        if((pdfont->cmap_ToUnicode == NULL || !gs_cmap_ToUnicode_check_pair(pdfont->cmap_ToUnicode, ch)) && pdev->UseOCR != UseOCRNever)
            (void)pdf_add_ToUnicode(pdev, font, pdfont, glyph, ch, NULL);
        return 0;
    }
    if (pet->glyph != GS_NO_GLYPH) { /* encoding conflict */
        return_error(gs_error_rangecheck);
        /* Must not happen because pdf_obtain_font_resource
         * checks for encoding compatibility.
         */
    }
    code = font->procs.glyph_name(font, glyph, &gnstr);
    if (code < 0)
        return code;	/* can't get name of glyph */
    if (font->FontType != ft_user_defined &&
        font->FontType != ft_PDF_user_defined &&
        font->FontType != ft_PCL_user_defined &&
        font->FontType != ft_MicroType &&
        font->FontType != ft_GL2_stick_user_defined &&
        font->FontType != ft_GL2_531) {
        /* The standard 14 fonts don't have a FontDescriptor. */
        code = (pdfont->base_font != 0 ?
                pdf_base_font_copy_glyph(pdfont->base_font, glyph, (gs_font_base *)font) :
                pdf_font_used_glyph(pdfont->FontDescriptor, glyph, (gs_font_base *)font));
        if (code < 0 && code != gs_error_undefined)
            return code;
        if (code == gs_error_undefined) {
            if (pdev->PDFA != 0 || pdev->PDFX != 0) {
                switch (pdev->PDFACompatibilityPolicy) {
                    case 0:
                        emprintf(pdev->memory,
                             "Requested glyph not present in source font,\n not permitted in PDF/A or PDF/X, reverting to normal PDF output\n");
                        pdev->AbortPDFAX = true;
                        pdev->PDFA = 0;
                        break;
                    case 1:
                        emprintf(pdev->memory,
                             "Requested glyph not present in source font,\n not permitted in PDF/A or PDF/X, glyph will not be present in output file\n\n");
                        /* Returning an error causees text processing to try and
                         * handle the glyph by rendering to a bitmap instead of
                         * as a glyph in a font. This will eliminate the problem
                         * and the fiel should appear the same as the original.
                         */
                        return_error(gs_error_unknownerror);
                        break;
                    case 2:
                        emprintf(pdev->memory,
                             "Requested glyph not present in source font,\n not permitted in PDF/A or PDF/X, aborting conversion\n");
                        /* Careful here, only certain errors will bubble up
                         * through the text processing.
                         */
                        return_error(gs_error_invalidfont);
                        break;
                    default:
                        emprintf(pdev->memory,
                             "Requested glyph not present in source font,\n not permitted in PDF/A or PDF/X, unrecognised PDFACompatibilityLevel,\nreverting to normal PDF output\n");
                        pdev->AbortPDFAX = true;
                        pdev->PDFA = 0;
                        break;
                }
            }
            /* PS font has no such glyph. */
            if (bytes_compare(gnstr.data, gnstr.size, (const byte *)".notdef", 7)) {
                pet->glyph = glyph;
                pdf_copy_string_to_encoding(pdev, &gnstr, pet);
                pet->is_difference = true;
            }
        } else if (pdfont->base_font == NULL && ccfont != NULL &&
                (gs_copy_glyph_options(font, glyph, (gs_font *)ccfont, COPY_GLYPH_NO_NEW) != 1 ||
                    gs_copied_font_add_encoding((gs_font *)ccfont, ch, glyph) < 0)) {
               /*
                * The "complete" copy of the font appears incomplete
                * due to incrementally added glyphs. Drop the "complete"
                * copy now and continue with subset font only.
                *
                * Note that we need to add the glyph to the encoding of the
                * "complete" font, because "PPI-ProPag 2.6.1.4 (archivePg)"
                * creates multiple font copies with reduced encodings
                * (we believe it is poorly designed),
                * and we can merge the copies back to a single font (see Bug 686875).
                * We also check whether the encoding is compatible.
                * It must be compatible here due to the pdf_obtain_font_resource
                * and ccfont logics, but we want to ensure for safety reason.
                */
            ccfont = NULL;
            pdf_font_descriptor_drop_complete_font(pdfont->FontDescriptor);
        }
        /*
            * We arbitrarily allow the first encoded character in a given
            * position to determine the encoding associated with the copied
            * font.
            */
        copied_glyph = cfont->procs.encode_char((gs_font *)cfont, ch,
                                                GLYPH_SPACE_NAME);
        if (glyph != copied_glyph &&
            gs_copied_font_add_encoding((gs_font *)cfont, ch, glyph) < 0
            )
            pet->is_difference = true;
        pdfont->used[ch >> 3] |= 0x80 >> (ch & 7);
    }
    /*
     * We always generate ToUnicode for simple fonts, because
     * we can't detemine in advance, which glyphs the font actually uses.
     * The decision about writing it out is deferred until pdf_write_font_resource.
     */
    code = pdf_add_ToUnicode(pdev, font, pdfont, glyph, ch, &gnstr);
    if(code < 0)
        return code;
    pet->glyph = glyph;
    return pdf_copy_string_to_encoding(pdev, &gnstr, pet);
}

/*
 * Estimate text bbox.
 */
static int
process_text_estimate_bbox(pdf_text_enum_t *pte, gs_font_base *font,
                          const gs_const_string *pstr,
                          const gs_matrix *pfmat,
                          gs_rect *text_bbox, gs_point *pdpt)
{
    int i;
    int space_char =
        (pte->text.operation & TEXT_ADD_TO_SPACE_WIDTH ?
         pte->text.space.s_char : -1);
    int WMode = font->WMode;
    int code = 0;
    gs_point total = {0, 0};
    gs_point p0, p1, p2, p3;
    gs_fixed_point origin;
    gs_matrix m;
    int xy_index = pte->xy_index, info_flags = 0;
    gx_path *path = gs_text_enum_path(pte);

    code = gx_path_current_point(path, &origin);
    if (code < 0)
        return code;
    m = ctm_only(pte->pgs);
    m.tx = fixed2float(origin.x);
    m.ty = fixed2float(origin.y);
    gs_matrix_multiply(pfmat, &m, &m);

    /* If the FontBBox is all 0, then its clearly wrong, so determine the text width
     * accurately by processing the glyph program.
     */
    if (font->FontBBox.p.x == font->FontBBox.q.x ||
        font->FontBBox.p.y == font->FontBBox.q.y) {
            info_flags = GLYPH_INFO_BBOX | GLYPH_INFO_WIDTH0 << WMode;
    } else {
        double width, height;

        /* This is a heuristic for Bug #700124. We try to determine whether a given glyph
         * is used in a string by using the current point, glyph width and advance width
         * to see if the glyph is fully clipped out, if it is we don't include it in the
         * output, or in subset fonts.
         *
         * Previously we used the FontBBox to determine a quick glyph width, but
         * in bug #699454 and bug #699571 we saw that OneVision EPSExport could construct
         * fonts with a wildly incorrect BBox ([0 0 2 1]) and then draw the text without
         * an advance width, leading us to conclude the glyph was clipped out and eliding it.
         *
         * To solve this we added code to process the glyph program and extract an accurate
         * width of the glyph. However, this proved slow. So here we attempt to decide if
         * the FontBBox is sensible by applying the FontMatrix to it, and looking to see
         * if that results in a reasonable number of co-ordinates in font space. If it
         * does then we use the FontBBox for speed, otherwise we carefully process the
         * glyphs in the font and extract their accurate widths.
         */
        gs_point_transform(font->FontBBox.p.x, font->FontBBox.p.y, &font->FontMatrix, &p0);
        gs_point_transform(font->FontBBox.p.x, font->FontBBox.q.y, &font->FontMatrix, &p1);
        gs_point_transform(font->FontBBox.q.x, font->FontBBox.p.y, &font->FontMatrix, &p2);
        gs_point_transform(font->FontBBox.q.x, font->FontBBox.q.y, &font->FontMatrix, &p3);
        width = max(fabs(p2.x), fabs(p3.x)) - p0.x;
        height = max(fabs(p1.y), fabs(p3.y)) - p0.y;

        /* Yes, this is a magic number. There's no reasoning here, its just a guess, we may
         * need to adjust this in future. Or possibly do away with it altogether if it proves
         * unreliable.
         */
        if (fabs(width) < 0.1 || fabs(height) < 0.1) {
            info_flags = GLYPH_INFO_BBOX | GLYPH_INFO_WIDTH0 << WMode;
        } else {
            gs_point_transform(font->FontBBox.p.x, font->FontBBox.p.y, &m, &p0);
            gs_point_transform(font->FontBBox.p.x, font->FontBBox.q.y, &m, &p1);
            gs_point_transform(font->FontBBox.q.x, font->FontBBox.p.y, &m, &p2);
            gs_point_transform(font->FontBBox.q.x, font->FontBBox.q.y, &m, &p3);
            info_flags = GLYPH_INFO_WIDTH0 << WMode;
        }
    }

    for (i = 0; i < pstr->size; ++i) {
        byte c = pstr->data[i];
        gs_rect bbox;
        gs_point wanted, tpt;
        gs_glyph glyph = font->procs.encode_char((gs_font *)font, c,
                                        GLYPH_SPACE_NAME);
        gs_glyph_info_t info;
        int code;

        if (glyph == GS_NO_GLYPH)
            return_error (gs_error_invalidfont);

        memset(&info, 0x00, sizeof(gs_glyph_info_t));
        code = font->procs.glyph_info((gs_font *)font, glyph, NULL,
                                            info_flags,
                                            &info);

        /* If we got an undefined error, and its a type 1/CFF font, try to
         * find the /.notdef glyph and use its width instead (as this is the
         * glyph which will be rendered). We don't do this for other font types
         * as it seems Acrobat/Distiller may not do so either.
         */
        /* The GL/2 stick font does not supply the enumerate_glyphs method,
         * *and* leaves it uninitialised. But it should not be possible to
         * get an undefiend error with this font anyway.
         */
        if (code < 0) {
            if ((font->FontType == ft_encrypted ||
            font->FontType == ft_encrypted2)) {
                int index;

                for (index = 0;
                    (font->procs.enumerate_glyph((gs_font *)font, &index,
                    (GLYPH_SPACE_NAME), &glyph)) >= 0 &&
                    index != 0;) {

                    if (gs_font_glyph_is_notdef(font, glyph)) {
                        code = font->procs.glyph_info((gs_font *)font, glyph, NULL,
                                            info_flags,
                                            &info);

                    if (code < 0)
                        return code;
                    }
                    break;
                }
            }
            if (code < 0)
                return code;
        }
        if (pte->text.operation & TEXT_REPLACE_WIDTHS) {
            code = gs_text_replaced_width(&pte->text, xy_index++, &tpt);
            if (code < 0)
                return code;

            gs_distance_transform(tpt.x, tpt.y, &ctm_only(pte->pgs), &wanted);
        } else {
            gs_distance_transform(info.width[WMode].x,
                                  info.width[WMode].y,
                                  &m, &wanted);
            if (pte->text.operation & TEXT_ADD_TO_ALL_WIDTHS) {
                gs_distance_transform(pte->text.delta_all.x,
                                      pte->text.delta_all.y,
                                      &ctm_only(pte->pgs), &tpt);
                wanted.x += tpt.x;
                wanted.y += tpt.y;
            }
            if (pstr->data[i] == space_char && pte->text.operation & TEXT_ADD_TO_SPACE_WIDTH) {
                gs_distance_transform(pte->text.delta_space.x,
                                      pte->text.delta_space.y,
                                      &ctm_only(pte->pgs), &tpt);
                wanted.x += tpt.x;
                wanted.y += tpt.y;
            }
        }

        if (info_flags & GLYPH_INFO_BBOX) {
            gs_point_transform(info.bbox.p.x, info.bbox.p.x, &m, &p0);
            gs_point_transform(info.bbox.p.x, info.bbox.q.y, &m, &p1);
            gs_point_transform(info.bbox.q.x, info.bbox.p.y, &m, &p2);
            gs_point_transform(info.bbox.q.x, info.bbox.q.y, &m, &p3);
        }

        bbox.p.x = min(min(p0.x, p1.x), min(p2.x, p3.x));
        bbox.p.y = min(min(p0.y, p1.y), min(p2.y, p3.y));
        bbox.q.x = max(max(p0.x, p1.x), max(p2.x, p3.x));
        bbox.q.y = max(max(p0.y, p1.y), max(p2.y, p3.y));

        bbox.q.x = bbox.p.x + max(bbox.q.x - bbox.p.x, wanted.x);
        bbox.q.y = bbox.p.y + max(bbox.q.y - bbox.p.y, wanted.y);
        bbox.p.x += total.x;
        bbox.p.y += total.y;
        bbox.q.x += total.x;
        bbox.q.y += total.y;

        total.x += wanted.x;
        total.y += wanted.y;

        if (i == 0)
            *text_bbox = bbox;
        else
            rect_merge(*text_bbox, bbox);
    }
    *pdpt = total;
    return 0;
}

void
adjust_first_last_char(pdf_font_resource_t *pdfont, byte *str, int size)
{
    int i;

    for (i = 0; i < size; ++i) {
        int chr = str[i];

        if (chr < pdfont->u.simple.FirstChar)
            pdfont->u.simple.FirstChar = chr;
        if (chr > pdfont->u.simple.LastChar)
            pdfont->u.simple.LastChar = chr;
    }
}

int
pdf_shift_text_currentpoint(pdf_text_enum_t *penum, gs_point *wpt)
{
    return gs_moveto_aux(penum->pgs, gx_current_path(penum->pgs),
                              fixed2float(penum->origin.x) + wpt->x,
                              fixed2float(penum->origin.y) + wpt->y);
}

/*
 * Internal procedure to process a string in a non-composite font.
 * Doesn't use or set pte->{data,size,index}; may use/set pte->xy_index;
 * may set penum->returned.total_width.  Sets ppts->values.
 *
 * Note that the caller is responsible for re-encoding the string, if
 * necessary; for adding Encoding entries in pdfont; and for copying any
 * necessary glyphs.  penum->current_font provides the gs_font for getting
 * glyph metrics, but this font's Encoding is not used.
 */
static int process_text_return_width(const pdf_text_enum_t *pte,
                                      gs_font_base *font,
                                      pdf_text_process_state_t *ppts,
                                      const gs_const_string *pstr, const gs_glyph *gdata,
                                      gs_point *pdpt, int *accepted, gs_rect *bbox);
static int
pdf_process_string(pdf_text_enum_t *penum, gs_string *pstr,
                   const gs_matrix *pfmat,
                   pdf_text_process_state_t *ppts, const gs_glyph *gdata)
{
    gx_device_pdf *const pdev = (gx_device_pdf *)penum->dev;
    gs_font_base *font = (gs_font_base *)penum->current_font;
    pdf_font_resource_t *pdfont;
    gs_text_params_t *text = &penum->text;
    int code = 0, mask;
    gs_point width_pt;
    int accepted;
    gs_rect text_bbox = {{0, 0}, {0, 0}}, glyphs_bbox = {{10000,10000}, {0,0}};
    unsigned int operation = text->operation;
    gx_path *path = gs_text_enum_path(penum);

    code = pdf_obtain_font_resource(penum, pstr, &pdfont);
    if (code < 0)
        return code;
    if (pfmat == 0)
        pfmat = &font->FontMatrix;
    if (text->operation & TEXT_RETURN_WIDTH) {
        code = gx_path_current_point(path, &penum->origin);
        if (code < 0)
            return code;
    }
    if (text->size == 0)
        return 0;
    if (penum->pgs->text_rendering_mode != 3 && !(text->operation & TEXT_DO_NONE)) {
        /*
         * Acrobat Reader can't handle text with huge coordinates,
         * so don't emit the text if it is outside the clip bbox
         * (Note : it ever fails with type 3 fonts).
         */

        code = process_text_estimate_bbox(penum, font, (gs_const_string *)pstr, pfmat,
                                          &text_bbox, &width_pt);
        if (code == 0) {
            gs_fixed_rect clip_bbox;
            gs_rect rect;

            if (penum->pcpath) {
                gx_cpath_outer_box(penum->pcpath, &clip_bbox);
                rect.p.x = fixed2float(clip_bbox.p.x);
                rect.p.y = fixed2float(clip_bbox.p.y);
                rect.q.x = fixed2float(clip_bbox.q.x);
                rect.q.y = fixed2float(clip_bbox.q.y);
                rect_intersect(rect, text_bbox);
                if (rect.p.x > rect.q.x || rect.p.y > rect.q.y) {
                    penum->index += pstr->size;
                    text->operation &= ~TEXT_DO_DRAW;
                    penum->text_clipped = true;
                }
            }
        } else {
            gs_matrix m;
            gs_fixed_point origin;
            gs_point p0, p1, p2, p3;

            code = gx_path_current_point(path, &origin);
            if (code < 0)
                goto done;

            m = ctm_only(penum->pgs);
            m.tx = fixed2float(origin.x);
            m.ty = fixed2float(origin.y);
            gs_matrix_multiply(pfmat, &m, &m);

            if (font->FontBBox.p.x != font->FontBBox.q.x) {
                text_bbox.p.x = font->FontBBox.p.x;
                text_bbox.q.x = font->FontBBox.q.x;
            } else {
                text_bbox.p.x = 0;
                text_bbox.q.x = 1000;
            }
            if (font->FontBBox.p.y != font->FontBBox.q.y) {
                text_bbox.p.y = font->FontBBox.p.y;
                text_bbox.q.y = font->FontBBox.q.y;
            } else {
                text_bbox.p.y = 0;
                text_bbox.q.y = 1000;
            }
            gs_point_transform(text_bbox.p.x, text_bbox.p.y, &m, &p0);
            gs_point_transform(text_bbox.p.x, text_bbox.q.y, &m, &p1);
            gs_point_transform(text_bbox.q.x, text_bbox.p.y, &m, &p2);
            gs_point_transform(text_bbox.q.x, text_bbox.q.y, &m, &p3);
            text_bbox.p.x = min(min(p0.x, p1.x), min(p1.x, p2.x));
            text_bbox.p.y = min(min(p0.y, p1.y), min(p1.y, p2.y));
            text_bbox.q.x = max(max(p0.x, p1.x), max(p1.x, p2.x));
            text_bbox.q.y = max(max(p0.y, p1.y), max(p1.y, p2.y));
        }
    } else {
        /* We have no penum->pcpath. */
    }

    /*
     * Note that pdf_update_text_state sets all the members of ppts->values
     * to their current values.
     */
    code = pdf_update_text_state(ppts, penum, pdfont, pfmat);
    if (code > 0) {
        /* Try not to emulate ADD_TO_WIDTH if we don't have to. */
        if (code & TEXT_ADD_TO_SPACE_WIDTH) {
            if (!memchr(pstr->data, penum->text.space.s_char, pstr->size))
                code &= ~TEXT_ADD_TO_SPACE_WIDTH;
        }
    }
    if (code < 0)
        goto done;
    mask = code;

    if (text->operation & TEXT_REPLACE_WIDTHS)
        mask |= TEXT_REPLACE_WIDTHS;

    /*
     * The only operations left to handle are TEXT_DO_DRAW and
     * TEXT_RETURN_WIDTH.
     */
    if (mask == 0) {
        /*
         * If any character has real_width != Width, we have to process
         * the string character-by-character.  process_text_return_width
         * will tell us what we need to know.
         */
        if (!(text->operation & (TEXT_DO_DRAW | TEXT_RETURN_WIDTH))) {
            code = 0;
            goto done;
        }
        code = process_text_return_width(penum, font, ppts,
                                         (gs_const_string *)pstr, gdata,
                                         &width_pt, &accepted, &glyphs_bbox);
        if (code < 0)
            goto done;
        if (code == 0) {
            /* No characters with redefined widths -- the fast case. */
            if (text->operation & TEXT_DO_DRAW || penum->pgs->text_rendering_mode == 3) {
                code = pdf_append_chars(pdev, pstr->data, accepted,
                                        width_pt.x, width_pt.y, false);
                if (code < 0)
                    goto done;
                adjust_first_last_char(pdfont, pstr->data, accepted);
                penum->index += accepted;
            } else if (text->operation & TEXT_DO_NONE)
                penum->index += accepted;
        } else {
            /* Use the slow case.  Set mask to any non-zero value. */
            mask = TEXT_RETURN_WIDTH;
        }
    }
    if (mask) {
        /* process_text_modify_width destroys text parameters, save them now. */
        int index0 = penum->index, xy_index = penum->xy_index;
        gs_text_params_t text = penum->text;
        int xy_index_step = (!(penum->text.operation & TEXT_REPLACE_WIDTHS) ? 0 :
                             penum->text.x_widths == penum->text.y_widths ? 2 : 1);
        /* A glyphshow takes a shortcut by storing the single glyph directly into
         * penum->text.data.d_glyph. However, process_text_modify_width
         * replaces pte->text.data.bytes (these two are part of a union) with
         * pstr->data, which is not valid for a glyphshow because it alters
         * the glyph value store there. If we make a copy of the single glyph,
         * it all works correctly.then
         */
        gs_glyph gdata_i, *gdata_p = (gs_glyph *)gdata;
        if (penum->text.operation & TEXT_FROM_SINGLE_GLYPH) {
            gdata_i = *gdata;
            gdata_p = &gdata_i;
        }

        if (penum->text.operation & TEXT_REPLACE_WIDTHS) {
            if (penum->text.x_widths != NULL)
                penum->text.x_widths += xy_index * xy_index_step;
            if (penum->text.y_widths != NULL)
                penum->text.y_widths += xy_index * xy_index_step;
        }
        penum->xy_index = 0;
        code = process_text_modify_width(penum, (gs_font *)font, ppts,
                                         (gs_const_string *)pstr,
                                         &width_pt, (const gs_glyph *)gdata_p, false, 1);
        if (penum->text.operation & TEXT_REPLACE_WIDTHS) {
            if (penum->text.x_widths != NULL)
                penum->text.x_widths -= xy_index * xy_index_step;
            if (penum->text.y_widths != NULL)
                penum->text.y_widths -= xy_index * xy_index_step;
        }
        penum->xy_index += xy_index;
        adjust_first_last_char(pdfont, pstr->data, penum->index);
        penum->text = text;
        penum->index += index0;
        if (code < 0)
            goto done;
    }

    /* Finally, return the total width if requested. */
    if (pdev->Eps2Write && penum->pcpath) {
        gx_device_clip cdev;
        gx_drawing_color devc;
        fixed x0, y0, bx2, by2;

        if (glyphs_bbox.p.x != 10000 && glyphs_bbox.q.x != 0){
            gs_matrix m;
            gs_fixed_point origin;
            gs_point p0, p1, p2, p3;

            code = gx_path_current_point(path, &origin);
            if (code < 0)
                return code;

            m = ctm_only(penum->pgs);
            m.tx = fixed2float(origin.x);
            m.ty = fixed2float(origin.y);
            gs_matrix_multiply(pfmat, &m, &m);

            gs_point_transform(glyphs_bbox.p.x, glyphs_bbox.p.y, &m, &p0);
            gs_point_transform(glyphs_bbox.p.x, glyphs_bbox.q.y, &m, &p1);
            gs_point_transform(glyphs_bbox.q.x, glyphs_bbox.p.y, &m, &p2);
            gs_point_transform(glyphs_bbox.q.x, glyphs_bbox.q.y, &m, &p3);
            glyphs_bbox.p.x = min(min(p0.x, p1.x), min(p1.x, p2.x));
            glyphs_bbox.p.y = min(min(p0.y, p1.y), min(p1.y, p2.y));
            glyphs_bbox.q.x = max(max(p0.x, p1.x), max(p1.x, p2.x));
            glyphs_bbox.q.y = max(max(p0.y, p1.y), max(p1.y, p2.y));
            if (glyphs_bbox.p.y > text_bbox.p.y)
                text_bbox.p.y = glyphs_bbox.p.y;
            if (glyphs_bbox.q.y < text_bbox.q.y)
                text_bbox.q.y = glyphs_bbox.q.y;
        }
        /* removed this section for bug #695671, where the rotated text
         * doesn't contribute the 'height' of the text to the x dimension
         * of the bounding box if this code is present. I can't see why
         * this clamping was done, if it turns out to be required then
         * we will need to revisit this and bug #695671.
        text_bbox.p.x = fixed2float(penum->origin.x);
        text_bbox.q.x = text_bbox.p.x + width_pt.x;
         */

        x0 = float2fixed(text_bbox.p.x);
        y0 = float2fixed(text_bbox.p.y);
        bx2 = float2fixed(text_bbox.q.x) - x0;
        by2 = float2fixed(text_bbox.q.y) - y0;

        pdev->AccumulatingBBox++;
        gx_make_clip_device_on_stack(&cdev, penum->pcpath, (gx_device *)pdev);
        set_nonclient_dev_color(&devc, gx_device_black((gx_device *)pdev));  /* any non-white color will do */
        gx_default_fill_triangle((gx_device *) pdev, x0, y0,
                                 float2fixed(text_bbox.p.x) - x0,
                                 float2fixed(text_bbox.q.y) - y0,
                                 bx2, by2, &devc, lop_default);
        gx_default_fill_triangle((gx_device *) & cdev, x0, y0,
                                 float2fixed(text_bbox.q.x) - x0,
                                 float2fixed(text_bbox.p.y) - y0,
                                 bx2, by2, &devc, lop_default);
        pdev->AccumulatingBBox--;
    }
    if (!(operation & TEXT_RETURN_WIDTH)) {
        code = 0;
        goto done;
    }
    if (operation & TEXT_DO_NONE) {
        /* stringwidth needs to transform to user space. */
        gs_point p;

        gs_distance_transform_inverse(width_pt.x, width_pt.y, &ctm_only(penum->pgs), &p);
        penum->returned.total_width.x += p.x;
        penum->returned.total_width.y += p.y;
    } else
        penum->returned.total_width = width_pt;
    code = pdf_shift_text_currentpoint(penum, &width_pt);

done:
    text->operation = operation;
    return code;
}

/*
 * Get the widths (unmodified and possibly modified) of a given character
 * in a simple font.  May add the widths to the widths cache (pdfont->Widths
 * and pdf_font_cache_elem::real_widths).  Return 1 if the widths were not cached.
 */
static int
pdf_char_widths(gx_device_pdf *const pdev,
                pdf_font_resource_t *pdfont, int ch, gs_font_base *font,
                pdf_glyph_widths_t *pwidths /* may be NULL */)
{
    pdf_glyph_widths_t widths;
    int code;
    byte *glyph_usage;
    double *real_widths;
    int char_cache_size, width_cache_size;
    pdf_font_resource_t *pdfont1;

    code = pdf_attached_font_resource(pdev, (gs_font *)font, &pdfont1,
                                &glyph_usage, &real_widths, &char_cache_size, &width_cache_size);
    if (code < 0)
        return code;
    if (pdfont1 != pdfont)
        return_error(gs_error_unregistered); /* Must not happen. */
    if (ch < 0 || ch > 255)
        return_error(gs_error_rangecheck);
    if (ch >= width_cache_size)
        return_error(gs_error_unregistered); /* Must not happen. */
    if (pwidths == 0)
        pwidths = &widths;
    if ((font->FontType != ft_user_defined &&
        font->FontType != ft_PDF_user_defined &&
        font->FontType != ft_PCL_user_defined &&
        font->FontType != ft_MicroType &&
        font->FontType != ft_GL2_stick_user_defined &&
        font->FontType != ft_GL2_531) && real_widths[ch] == 0) {
        /* Might be an unused char, or just not cached. */
        gs_glyph glyph = pdfont->u.simple.Encoding[ch].glyph;

        code = pdf_glyph_widths(pdfont, font->WMode, glyph, (gs_font *)font, pwidths, NULL);
        if (code < 0)
            return code;
        pwidths->BBox.p.x = pwidths->BBox.p.y = pwidths->BBox.q.x = pwidths->BBox.q.y = 0;
        if (font->WMode != 0 && code > 0 && !pwidths->replaced_v) {
            /*
             * The font has no Metrics2, so it must write
             * horizontally due to PS spec.
             * Therefore we need to fill the Widths array,
             * which is required by PDF spec.
             * Take it from WMode==0.
             */
            code = pdf_glyph_widths(pdfont, 0, glyph, (gs_font *)font, pwidths, NULL);
        }
        if (pwidths->replaced_v) {
            pdfont->u.simple.v[ch].x = pwidths->real_width.v.x - pwidths->Width.v.x;
            pdfont->u.simple.v[ch].y = pwidths->real_width.v.y - pwidths->Width.v.y;
        } else
            pdfont->u.simple.v[ch].x = pdfont->u.simple.v[ch].y = 0;
        if (code == 0) {
            pdfont->Widths[ch] = pwidths->Width.w;
            real_widths[ch] = pwidths->real_width.w;
        } else {
            if ((font->WMode == 0 || pwidths->ignore_wmode) && !pwidths->replaced_v)
                pdfont->Widths[ch] = pwidths->real_width.w;
        }
    } else {
        if (font->FontType == ft_user_defined || font->FontType == ft_PCL_user_defined ||
            font->FontType == ft_MicroType || font->FontType == ft_GL2_stick_user_defined ||
            font->FontType == ft_GL2_531 || font->FontType == ft_PDF_user_defined ) {
            if (!(pdfont->used[ch >> 3] & 0x80 >> (ch & 7)))
	      return_error(gs_error_undefined); /* The charproc was not accumulated. */
            if (!pdev->charproc_just_accumulated &&
                !(pdfont->u.simple.s.type3.cached[ch >> 3] & 0x80 >> (ch & 7))) {
                 /* The charproc uses setcharwidth.
                    Need to accumulate again to check for a glyph variation. */
	      return_error(gs_error_undefined);
            }
        }
        if (pdev->charproc_just_accumulated && (font->FontType == ft_user_defined || font->FontType == ft_PDF_user_defined)) {
            pwidths->BBox.p.x = pdev->charproc_BBox.p.x;
            pwidths->BBox.p.y = pdev->charproc_BBox.p.y;
            pwidths->BBox.q.x = pdev->charproc_BBox.q.x;
            pwidths->BBox.q.y = pdev->charproc_BBox.q.y;
        }
        pwidths->Width.w = pdfont->Widths[ch];
        pwidths->Width.v = pdfont->u.simple.v[ch];
        pwidths->real_width.v.x = pwidths->real_width.v.y = 0;
        pwidths->ignore_wmode = false;
        if (font->FontType == ft_user_defined || font->FontType == ft_PCL_user_defined ||
            font->FontType == ft_MicroType || font->FontType == ft_GL2_stick_user_defined ||
            font->FontType == ft_GL2_531 || font->FontType == ft_PDF_user_defined) {
            pwidths->real_width.w = real_widths[ch * 2];
            pwidths->Width.xy.x = pwidths->Width.w;
            pwidths->Width.xy.y = 0;
            pwidths->real_width.xy.x = real_widths[ch * 2 + 0];
            pwidths->real_width.xy.y = real_widths[ch * 2 + 1];
            pwidths->replaced_v = 0;
        } else if (font->WMode) {
            pwidths->real_width.w = real_widths[ch];
            pwidths->Width.xy.x = 0;
            pwidths->Width.xy.y = pwidths->Width.w;
            pwidths->real_width.xy.x = 0;
            pwidths->real_width.xy.y = pwidths->real_width.w;
        } else {
            pwidths->real_width.w = real_widths[ch];
            pwidths->Width.xy.x = pwidths->Width.w;
            pwidths->Width.xy.y = 0;
            pwidths->real_width.xy.x = pwidths->real_width.w;
            pwidths->real_width.xy.y = 0;
        }
        code = 0;
    }
    return code;
}

/*
 * Convert glyph widths (.Width.xy and .real_widths.xy) from design to PDF text space
 * Zero-out one of Width.xy.x/y per PDF Ref 5.3.3 "Text Space Details"
 */
static void
pdf_char_widths_to_uts(pdf_font_resource_t *pdfont /* may be NULL for non-Type3 */,
                       pdf_glyph_widths_t *pwidths)
{
    if (pdfont && (pdfont->FontType == ft_user_defined ||
        pdfont->FontType == ft_PDF_user_defined ||
        pdfont->FontType == ft_PCL_user_defined ||
        pdfont->FontType == ft_MicroType ||
        pdfont->FontType == ft_GL2_stick_user_defined ||
        pdfont->FontType == ft_GL2_531)) {
        gs_matrix *pmat = &pdfont->u.simple.s.type3.FontMatrix;

        pwidths->Width.xy.x *= pmat->xx; /* formula simplified based on wy in glyph space == 0 */
        pwidths->Width.xy.y  = 0.0; /* WMode == 0 for PDF Type 3 fonts */
        gs_distance_transform(pwidths->real_width.xy.x, pwidths->real_width.xy.y, pmat, &pwidths->real_width.xy);
    } else {
        /*
         * For other font types:
         * - PDF design->text space is a simple scaling by 0.001.
         * - The Width.xy.x/y that should be zeroed-out per 5.3.3 "Text Space Details" is already 0.
         */
        pwidths->Width.xy.x /= 1000.0;
        pwidths->Width.xy.y /= 1000.0;
        pwidths->real_width.xy.x /= 1000.0;
        pwidths->real_width.xy.y /= 1000.0;
    }
}

/*
 * Compute the total text width (in user space).  Return 1 if any
 * character had real_width != Width, otherwise 0.
 */
static int
process_text_return_width(const pdf_text_enum_t *pte, gs_font_base *font,
                          pdf_text_process_state_t *ppts,
                          const gs_const_string *pstr, const gs_glyph *gdata,
                          gs_point *pdpt, int *accepted, gs_rect *bbox)
{
    int i;
    gs_point w;
    gs_point dpt;
    int num_spaces = 0;
    int space_char =
        (pte->text.operation & TEXT_ADD_TO_SPACE_WIDTH ?
         pte->text.space.s_char : -1);
    int widths_differ = 0, code;
    gx_device_pdf *pdev = (gx_device_pdf *)pte->dev;
    pdf_font_resource_t *pdfont;

    code = pdf_attached_font_resource(pdev, (gs_font *)font, &pdfont, NULL, NULL, NULL, NULL);
    if (code < 0)
        return code;
    for (i = 0, w.x = w.y = 0; i < pstr->size; ++i) {
        pdf_glyph_widths_t cw; /* in PDF text space */
        gs_char ch = pstr->data[i];

        /* Initialise some variables */
        cw.real_width.xy.x = cw.real_width.xy.y = cw.Width.xy.x = cw.Width.xy.y = 0;
        cw.BBox.p.x = cw.BBox.p.y = cw.BBox.q.x = cw.BBox.q.y = 0;

        {  const gs_glyph *gdata_i = (gdata != NULL ? gdata + i : 0);

            code = pdf_encode_string_element(pdev, (gs_font *)font, pdfont, ch, gdata_i);

            if (code < 0)
                return code;
        }
        if ((font->FontType == ft_user_defined ||
            font->FontType == ft_PDF_user_defined ||
            font->FontType == ft_PCL_user_defined ||
            font->FontType == ft_GL2_stick_user_defined ||
            font->FontType == ft_MicroType ||
            font->FontType == ft_GL2_531) &&
            (i > 0 || !pdev->charproc_just_accumulated) &&
            !(pdfont->u.simple.s.type3.cached[ch >> 3] & (0x80 >> (ch & 7)))){
            code = gs_error_undefined;
        }
        else {
            if (font->FontType == ft_PCL_user_defined) {
                /* Check the cache, if the glyph has been flushed, assume that
                 * it has been redefined, and do not use the current glyph.
                 * Additional code in pdf_text_process will also spot this
                 * condition and will not capture the glyph in this font.
                 */
                /* Cache checking code copied from gxchar.c, show_proceed,
                 * case 0, 'plain char'.
                 */
                gs_font *rfont = (pte->fstack.depth < 0 ? pte->current_font : pte->fstack.items[0].font);
                gs_font *pfont = (pte->fstack.depth < 0 ? pte->current_font :
                    pte->fstack.items[pte->fstack.depth].font);
                int wmode = rfont->WMode;
                gs_log2_scale_point log2_scale = {0,0};
                gs_fixed_point subpix_origin = {0,0};
                cached_fm_pair *pair;

                code = gx_lookup_fm_pair(pfont, &ctm_only(pte->pgs), &log2_scale,
                    false, &pair);
                if (code < 0)
                    return code;
                if (gx_lookup_cached_char(pfont, pair, ch, wmode,
                    1, &subpix_origin) == 0) {
                        /* Character is not in cache, must have been redefined. */
                    code = gs_error_undefined;
                }
                else {
                    /* Character is in cache, go ahead and use it */
                    code = pdf_char_widths((gx_device_pdf *)pte->dev,
                                   ppts->values.pdfont, ch, font, &cw);
                }
            } else
                /* Not a PCL bitmap font, we don't need to worry about redefined glyphs */
                code = pdf_char_widths((gx_device_pdf *)pte->dev,
                                   ppts->values.pdfont, ch, font, &cw);
        }
        if (code < 0) {
            if (i)
                break;
            *accepted = 0;
            return code;
        }
        pdf_char_widths_to_uts(pdfont, &cw);
        w.x += cw.real_width.xy.x;
        w.y += cw.real_width.xy.y;
        if (cw.real_width.xy.x != cw.Width.xy.x ||
            cw.real_width.xy.y != cw.Width.xy.y
            )
            widths_differ = 1;
        if (pstr->data[i] == space_char)
            ++num_spaces;
        if (cw.BBox.p.x != 0 && cw.BBox.q.x != 0){
            if (cw.BBox.p.x < bbox->p.x)
                bbox->p.x = cw.BBox.p.x;
            if (cw.BBox.p.y < bbox->p.y)
                bbox->p.y = cw.BBox.p.y;
            if (cw.BBox.q.x > bbox->q.x)
                bbox->q.x = cw.BBox.q.x;
            if (cw.BBox.q.y > bbox->q.y)
                bbox->q.y = cw.BBox.q.y;
        }
    }
    *accepted = i;
    gs_distance_transform(w.x * ppts->values.size, w.y * ppts->values.size,
                          &ppts->values.matrix, &dpt);
    if (pte->text.operation & TEXT_ADD_TO_ALL_WIDTHS) {
        int num_chars = *accepted;
        gs_point tpt;

        gs_distance_transform(pte->text.delta_all.x, pte->text.delta_all.y,
                              &ctm_only(pte->pgs), &tpt);
        dpt.x += tpt.x * num_chars;
        dpt.y += tpt.y * num_chars;
    }
    if (pte->text.operation & TEXT_ADD_TO_SPACE_WIDTH) {
        gs_point tpt;

        gs_distance_transform(pte->text.delta_space.x, pte->text.delta_space.y,
                              &ctm_only(pte->pgs), &tpt);
        dpt.x += tpt.x * num_spaces;
        dpt.y += tpt.y * num_spaces;
    }
    *pdpt = dpt;

    return widths_differ;
}

/*
 * Emulate TEXT_ADD_TO_ALL_WIDTHS and/or TEXT_ADD_TO_SPACE_WIDTH,
 * and implement TEXT_REPLACE_WIDTHS if requested.
 * Uses and updates ppts->values.matrix; uses ppts->values.pdfont.
 *
 * Destroys the text parameters in *pte.
 * The caller must restore them.
 */
int
process_text_modify_width(pdf_text_enum_t *pte, gs_font *font,
                          pdf_text_process_state_t *ppts,
                          const gs_const_string *pstr,
                          gs_point *pdpt, const gs_glyph *gdata, bool composite, int decoded_bytes)
{
    gx_device_pdf *const pdev = (gx_device_pdf *)pte->dev;
    int space_char =
        (pte->text.operation & TEXT_ADD_TO_SPACE_WIDTH ?
         pte->text.space.s_char : -1);
    gs_point start, total;
    pdf_font_resource_t *pdfont3 = NULL;
    int code;

    if (font->FontType == ft_user_defined ||
        font->FontType == ft_PDF_user_defined ||
        font->FontType == ft_PCL_user_defined ||
        font->FontType == ft_MicroType ||
        font->FontType == ft_GL2_stick_user_defined ||
        font->FontType == ft_GL2_531) {
        code = pdf_attached_font_resource(pdev, font, &pdfont3, NULL, NULL, NULL, NULL);
        if (code < 0)
            return code;

    }
    pte->text.data.bytes = pstr->data;
    pte->text.size = pstr->size;
    pte->index = 0;
    pte->text.operation &= ~TEXT_FROM_ANY;
    pte->text.operation |= TEXT_FROM_STRING;
    start.x = ppts->values.matrix.tx;
    start.y = ppts->values.matrix.ty;
    total.x = total.y = 0;	/* user space */
    /*
     * Note that character widths are in design space, but text.delta_*
     * values and the width value returned in *pdpt are in user space,
     * and the width values for pdf_append_chars are in device space.
     */
    for (;;) {
        pdf_glyph_widths_t cw;	/* design space, then converted to PDF text space */
        gs_point did, wanted, tpt;	/* user space */
        gs_point v = {0, 0}; /* design space */
        gs_char chr;
        gs_glyph glyph;
        int index = pte->index;
        gs_text_enum_t pte1 = *(gs_text_enum_t *)pte;
        int FontType;
        bool use_cached_v = true;
        byte composite_type3_text[1];

        code = pte1.orig_font->procs.next_char_glyph(&pte1, &chr, &glyph);
        if (code == 2) { /* end of string */
            gs_text_enum_copy_dynamic((gs_text_enum_t *)pte, &pte1, true);
            break;
        }
        if (code < 0)
            return code;
        if (composite) { /* from process_cmap_text */
            gs_font *subfont = pte1.fstack.items[pte1.fstack.depth].font;

            if (subfont->FontType == ft_user_defined || subfont->FontType == ft_PDF_user_defined ) {
                pdf_font_resource_t *pdfont;

                FontType = subfont->FontType;
                code = pdf_attached_font_resource(pdev, subfont,
                            &pdfont, NULL, NULL, NULL, NULL);
                if (code < 0)
                    return code;
                chr = pdf_find_glyph(pdfont, glyph);
                composite_type3_text[0] = (byte)chr;
                code = pdf_char_widths((gx_device_pdf *)pte->dev,
                                       ppts->values.pdfont, chr, (gs_font_base *)subfont,
                                       &cw);
            } else {
                pdf_font_resource_t *pdsubf = ppts->values.pdfont->u.type0.DescendantFont;

                FontType = pdsubf->FontType;
                code = pdf_glyph_widths(pdsubf, font->WMode, glyph, subfont, &cw,
                    pte->cdevproc_callout ? pte->cdevproc_result : NULL);
            }
        } else {/* must be a base font */
            const gs_glyph *gdata_i = (gdata != NULL ? gdata + pte->index : 0);

                /* gdata is NULL when composite == true, or the text isn't a single byte.  */
            code = pdf_encode_string_element(pdev, font, ppts->values.pdfont, chr, gdata_i);
            FontType = font->FontType;
            if (code >= 0) {
                if (chr == GS_NO_CHAR && glyph != GS_NO_GLYPH) {
                    /* glyphshow, we have no char code. Bug 686988.*/
                    code = pdf_glyph_widths(ppts->values.pdfont, font->WMode, glyph, font, &cw, NULL);
                    use_cached_v = false; /* Since we have no chr and don't call pdf_char_widths. */
                } else {
                    code = pdf_char_widths((gx_device_pdf *)pte->dev,
                                       ppts->values.pdfont, chr, (gs_font_base *)font,
                                       &cw);
                    if (code == 0 && font->FontType == ft_PCL_user_defined) {
                        /* Check the cache, if the glyph has been flushed, assume that
                         * it has been redefined, and do not use the current glyph.
                         * Additional code in pdf_text_process will also spot this
                         * condition and will not capture the glyph in this font.
                         */
                        /* Cache checking code copied from gxchar.c, show_proceed,
                         * case 0, 'plain char'.
                         */
                        gs_font *rfont = (pte->fstack.depth < 0 ? pte->current_font : pte->fstack.items[0].font);
                        gs_font *pfont = (pte->fstack.depth < 0 ? pte->current_font :
                            pte->fstack.items[pte->fstack.depth].font);
                        int wmode = rfont->WMode;
                        gs_log2_scale_point log2_scale = {0,0};
                        gs_fixed_point subpix_origin = {0,0};
                        cached_fm_pair *pair;

                        code = gx_lookup_fm_pair(pfont, &ctm_only(pte->pgs), &log2_scale,
                            false, &pair);
                        if (code < 0)
                            return code;
                        if (gx_lookup_cached_char(pfont, pair, chr, wmode,
                            1, &subpix_origin) == 0) {
                        /* Character is not in cache, must have been redefined. */
                            code = gs_error_undefined;
                        }
                    }
                }
            }
        }
        if (code < 0) {
            if (index > 0)
                break;
            return code;
        }
        /* TrueType design grid is 2048x2048 against the nominal PS/PDF grid of
         * 1000x1000. This can lead to rounding errors when converting to text space
         * and comparing against any entries in /W or /Widths arrays. We fix the
         * TrueType widths to the nearest integer here to avoid this.
         * See Bug #693825
         */
        if (FontType == ft_CID_TrueType || FontType == ft_TrueType) {
            cw.Width.w = floor(cw.Width.w + 0.5);
            cw.Width.xy.x = floor(cw.Width.xy.x + 0.5);
            cw.Width.xy.y = floor(cw.Width.xy.y + 0.5);
            cw.Width.v.x = floor(cw.Width.v.x + 0.5);
            cw.Width.v.y = floor(cw.Width.v.y + 0.5);
            cw.real_width.w = floor(cw.real_width.w + 0.5);
            cw.real_width.xy.x = floor(cw.real_width.xy.x + 0.5);
            cw.real_width.xy.y = floor(cw.real_width.xy.y + 0.5);
            cw.real_width.v.x = floor(cw.real_width.v.x + 0.5);
            cw.real_width.v.y = floor(cw.real_width.v.y + 0.5);
        }

        gs_text_enum_copy_dynamic((gs_text_enum_t *)pte, &pte1, true);
        if (composite || !use_cached_v) {
            if (cw.replaced_v) {
                v.x = cw.real_width.v.x - cw.Width.v.x;
                v.y = cw.real_width.v.y - cw.Width.v.y;
            }
        } else
            v = ppts->values.pdfont->u.simple.v[chr];
        if (font->WMode && !cw.ignore_wmode) {
            /* With WMode 1 v-vector is (WMode 1 origin) - (WMode 0 origin).
               The glyph shifts in the opposite direction.  */
            v.x = - v.x;
            v.y = - v.y;
        } else {
            /* With WMode 0 v-vector is (Metrics sb) - (native sb).
               The glyph shifts in same direction.  */
        }
        /* pdf_glyph_origin is not longer used. */
        if (v.x != 0 || v.y != 0) {
            gs_point glyph_origin_shift;
            double scale0;

            if (FontType == ft_TrueType || FontType == ft_CID_TrueType)
                scale0 = (float)0.001;
            else
                scale0 = 1;
            glyph_origin_shift.x = v.x * scale0;
            glyph_origin_shift.y = v.y * scale0;
            if (composite) {
                gs_font *subfont = pte->fstack.items[pte->fstack.depth].font;

                gs_distance_transform(glyph_origin_shift.x, glyph_origin_shift.y,
                                      &subfont->FontMatrix, &glyph_origin_shift);
            }
            gs_distance_transform(glyph_origin_shift.x, glyph_origin_shift.y,
                                  &font->FontMatrix, &glyph_origin_shift);
            gs_distance_transform(glyph_origin_shift.x, glyph_origin_shift.y,
                                  &ctm_only(pte->pgs), &glyph_origin_shift);
            if (glyph_origin_shift.x != 0 || glyph_origin_shift.y != 0) {
                ppts->values.matrix.tx = start.x + total.x + glyph_origin_shift.x;
                ppts->values.matrix.ty = start.y + total.y + glyph_origin_shift.y;
                code = pdf_set_text_state_values(pdev, &ppts->values);
                if (code < 0)
                    break;
            }
        }
        pdf_char_widths_to_uts(pdfont3, &cw); /* convert design->text space */
        if (pte->text.operation & (TEXT_DO_DRAW | TEXT_RENDER_MODE_3)) {
            gs_distance_transform(cw.Width.xy.x * ppts->values.size,
                                  cw.Width.xy.y * ppts->values.size,
                                  &ppts->values.matrix, &did);
            gs_distance_transform(((font->WMode && !cw.ignore_wmode) ? 0 : ppts->values.character_spacing),
                                  ((font->WMode && !cw.ignore_wmode) ? ppts->values.character_spacing : 0),
                                  &ppts->values.matrix, &tpt);
            did.x += tpt.x;
            did.y += tpt.y;
            /* If pte->single_byte_space == 0 then we had a widthshow or awidthshow from
             * PostScript, so we apply the PostScript rules. Otherwise it was from PDF
             * in which case if the number of bytes in the character code was 1 we apply
             * word spacing. If it was PDF and we had a multi-byte decode, do not apply
             * word spacing (how ugly!). Note tht its important this is applied the same to
             * both the 'did' and 'wanted' calculations (see below).
             */
            if (chr == space_char && (!pte->single_byte_space || decoded_bytes == 1)) {
                gs_distance_transform(((font->WMode && !cw.ignore_wmode)? 0 : ppts->values.word_spacing),
                                      ((font->WMode && !cw.ignore_wmode) ? ppts->values.word_spacing : 0),
                                      &ppts->values.matrix, &tpt);
                did.x += tpt.x;
                did.y += tpt.y;
            }
            if (composite && (FontType == ft_user_defined || FontType == ft_PDF_user_defined))
                code = pdf_append_chars(pdev, composite_type3_text, 1, did.x, did.y, composite);
            else
                code = pdf_append_chars(pdev, pstr->data + index, pte->index - index, did.x, did.y, composite);
            if (code < 0)
                break;
        } else
            did.x = did.y = 0;
        if (pte->text.operation & TEXT_REPLACE_WIDTHS) {
            gs_point dpt;

            /* We are applying a width override, from x/y/xyshow. This coudl be from
             * a PostScript file, or it could be from a PDF file where we have a font
             * with a FontMatrix which is neither horizontal nor vertical. If we use TJ
             * for that, then we end up applying the displacement twice, once here where
             * we add a TJ, and once when we actually draw the glyph (TJ is added *after*
             * the glyph is drawn, unlike xshow). So in this case we don't want to try
             * and use a TJ, we need to position the glyphs using text positioning
             * operators.
             */
            if(cw.Width.xy.x != cw.real_width.xy.x || cw.Width.xy.y != cw.real_width.xy.y)
                pdev->text->text_state->can_use_TJ = false;

            code = gs_text_replaced_width(&pte->text, pte->xy_index++, &dpt);
            if (code < 0)
                return_error(gs_error_unregistered);
            gs_distance_transform(dpt.x, dpt.y, &ctm_only(pte->pgs), &wanted);

            gs_distance_transform(((font->WMode && !cw.ignore_wmode) ? 0 : ppts->values.character_spacing),
                                  ((font->WMode && !cw.ignore_wmode) ? ppts->values.character_spacing : 0),
                                  &ppts->values.matrix, &tpt);
            wanted.x += tpt.x;
            wanted.y += tpt.y;

            if (chr == space_char && (!pte->single_byte_space || decoded_bytes == 1)) {
                gs_distance_transform(((font->WMode && !cw.ignore_wmode)? 0 : ppts->values.word_spacing),
                                      ((font->WMode && !cw.ignore_wmode) ? ppts->values.word_spacing : 0),
                                      &ppts->values.matrix, &tpt);
                wanted.x += tpt.x;
                wanted.y += tpt.y;
            }
        } else {
            pdev->text->text_state->can_use_TJ = true;
            gs_distance_transform(cw.real_width.xy.x * ppts->values.size,
                                  cw.real_width.xy.y * ppts->values.size,
                                  &ppts->values.matrix, &wanted);
            if (pte->text.operation & TEXT_ADD_TO_ALL_WIDTHS) {
                gs_distance_transform(pte->text.delta_all.x,
                                      pte->text.delta_all.y,
                                      &ctm_only(pte->pgs), &tpt);
                wanted.x += tpt.x;
                wanted.y += tpt.y;
            }
            /* See comment above for 'did' calculations, the application of word spacing must
             * be the same for did and wanted.
             */
            if (chr == space_char && (!pte->single_byte_space || decoded_bytes == 1)) {
                gs_distance_transform(pte->text.delta_space.x,
                                      pte->text.delta_space.y,
                                      &ctm_only(pte->pgs), &tpt);
                wanted.x += tpt.x;
                wanted.y += tpt.y;
            }
        }
        total.x += wanted.x;
        total.y += wanted.y;
        if (wanted.x != did.x || wanted.y != did.y) {
            ppts->values.matrix.tx = start.x + total.x;
            ppts->values.matrix.ty = start.y + total.y;
            code = pdf_set_text_state_values(pdev, &ppts->values);
            if (code < 0)
                break;
        }
        pdev->charproc_just_accumulated = false;
    }
    *pdpt = total;
    return 0;
}

/*
 * Get character code from a glyph code.
 * An usage of this function is very undesirable,
 * because a glyph may be unlisted in Encoding.
 */
int
pdf_encode_glyph(gs_font_base *bfont, gs_glyph glyph0,
            byte *buf, int buf_size, int *char_code_length)
{
    gs_char c;

    *char_code_length = 1;
    if (*char_code_length > buf_size)
        return_error(gs_error_rangecheck); /* Must not happen. */
    for (c = 0; c < 255; c++) {
        gs_glyph glyph1 = bfont->procs.encode_char((gs_font *)bfont, c,
                    GLYPH_SPACE_NAME);
        if (glyph1 == glyph0) {
            buf[0] = (byte)c;
            return 0;
        }
    }
    return_error(gs_error_rangecheck); /* Can't encode. */
}

/* ---------------- Type 1 or TrueType font ---------------- */

/*
 * Process a text string in a simple font.
 */
int
process_plain_text(gs_text_enum_t *pte, void *vbuf, uint bsize)
{
    byte *const buf = vbuf;
    uint count;
    uint operation = pte->text.operation;
    pdf_text_enum_t *penum = (pdf_text_enum_t *)pte;
    int code;
    gs_string str;
    pdf_text_process_state_t text_state;
    const gs_glyph *gdata = NULL;

    if (operation & (TEXT_FROM_STRING | TEXT_FROM_BYTES)) {
        count = pte->text.size - pte->index;
        if (bsize < count)
            return_error(gs_error_unregistered); /* Must not happen. */
        memcpy(buf, (const byte *)pte->text.data.bytes + pte->index, count);
    } else if (operation & (TEXT_FROM_CHARS | TEXT_FROM_SINGLE_CHAR)) {
        /* Check that all chars fit in a single byte. */
        const gs_char *cdata;
        int i;

        if (operation & TEXT_FROM_CHARS) {
            cdata = pte->text.data.chars;
            count = (pte->text.size - pte->index);
        } else {
            cdata = &pte->text.data.d_char;
            count = 1;
        }
        if (bsize < count * sizeof(gs_char))
            return_error(gs_error_unregistered); /* Must not happen. */
        for (i = 0; i < count; ++i) {
            gs_char chr = cdata[pte->index + i];

            if (chr & ~0xff)
                return_error(gs_error_rangecheck);
            buf[i] = (byte)chr;
        }
    } else if (operation & (TEXT_FROM_GLYPHS | TEXT_FROM_SINGLE_GLYPH)) {
        /*
         * Since PDF has no analogue of 'glyphshow',
         * we try to encode glyphs with the current
         * font's encoding. If the current font has no encoding,
         * or the encoding doesn't contain necessary glyphs,
         * the text will be represented with a Type 3 font with
         * bitmaps or outlines.
         *
         * When we fail with encoding (136-01.ps is an example),
         * we could locate a PDF font resource or create a new one
         * with same outlines and an appropriate encoding.
         * Also we could change .notdef entries in the
         * copied font (assuming that document designer didn't use
         * .notdef for a meanful printing).
         * fixme: Not implemented yet.
         */
        gs_font *font = pte->current_font;
        uint size;
        int i;

        if (operation & TEXT_FROM_GLYPHS) {
            gdata = pte->text.data.glyphs;
            size = pte->text.size - pte->index;
        } else {
            gdata = &pte->text.data.d_glyph;
            size = 1;
        }
        if (!pdf_is_simple_font(font))
            return_error(gs_error_unregistered); /* Must not happen. */
        count = 0;
        for (i = 0; i < size; ++i) {
            pdf_font_resource_t *pdfont;
            gs_glyph glyph = gdata[pte->index + i];
            int char_code_length;

            code = pdf_encode_glyph((gs_font_base *)font, glyph,
                         buf + count, size - count, &char_code_length);
            if (code < 0)
                break;
            /* Even if we already have a glyph encoded at this position in the font
             * it may not be the *right* glyph. We effectively use the first byte of
             * the glyph name as the index when using glyphshow which means that
             * /o and /omicron would be encoded at the same index. So we need
             * to check the actual glyph to see if they are the same. To do
             * that we need the PDF font resource which is attached to the font (if any).
             * cf bugs #695259 and #695168
             */
            code = pdf_attached_font_resource((gx_device_pdf *)penum->dev, font,
                            &pdfont, NULL, NULL, NULL, NULL);
            if (code >= 0 && pdfont && pdfont->u.simple.Encoding[*(buf + count)].glyph != glyph)
                /* the glyph doesn't match the glyph already encoded at this position.
                 * Breaking out here will start a new PDF font resource in the code below.
                 */
                break;
            count += char_code_length;
            if (operation & TEXT_INTERVENE)
                break; /* Just do one character. */
        }
        if (i < size) {
            pdf_font_resource_t *pdfont;

            str.data = buf;
            str.size = size;
            code = pdf_obtain_font_resource_unencoded(penum, &str, &pdfont, gdata);
            if (code < 0) {
                /*
                 * pdf_text_process will fall back
                 * to default implementation.
                 */
                return code;
            }
            count = size;
        }
        /*  So far we will use TEXT_FROM_STRING instead
            TEXT_FROM_*_GLYPH*. Since we used a single
            byte encoding, the character index appears invariant
            during this substitution.
         */
    } else
        return_error(gs_error_rangecheck);
    str.data = buf;
    if (count > 1 && (operation & TEXT_INTERVENE)) {
        /* Just do one character. */
        str.size = 1;
        code = pdf_process_string_aux(penum, &str, gdata, NULL, &text_state);
        if (code >= 0) {
            pte->returned.current_char = buf[0];
            code = TEXT_PROCESS_INTERVENE;
        }
    } else {
        str.size = count;
        code = pdf_process_string_aux(penum, &str, gdata, NULL, &text_state);
    }
    return code;
}
