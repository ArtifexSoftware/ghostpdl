/* Copyright (C) 2001-2019 Artifex Software, Inc.
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


/* PDF-writing driver */
#include "fcntl_.h"
#include "memory_.h"
#include "time_.h"
#include "unistd_.h"
#include "gx.h"
#include "gp.h"			/* for gp_get_realtime */
#include "gserrors.h"
#include "gxdevice.h"
#include "gdevpdfx.h"
#include "gdevpdfg.h"		/* only for pdf_reset_graphics */
#include "gdevpdfo.h"
#include "smd5.h"
#include "sarc4.h"
#include "gscms.h"
#include "gdevpdtf.h"
#include "gdevpdtx.h"
#include "gdevpdtd.h"
#include "gdevpdti.h"
#include "gsfcmap.h"        /* For gs_cmap_ToUnicode_free */

#include "gxfcache.h"
#include "gdevpdts.h"       /* for sync_text_state */

/* Define the default language level and PDF compatibility level. */
/* Acrobat 8 (PDF 1.7) is the default. (1.7 for ICC V4.2.0 profile support) */
#define PSDF_VERSION_INITIAL psdf_version_ll3
#define PDF_COMPATIBILITY_LEVEL_INITIAL 1.7

/* Define the size of internal stream buffers. */
/* (This is not a limitation, it only affects performance.) */
#define sbuf_size 512

/* GC descriptors */
private_st_pdf_page();
gs_private_st_element(st_pdf_page_element, pdf_page_t, "pdf_page_t[]",
                      pdf_page_elt_enum_ptrs, pdf_page_elt_reloc_ptrs,
                      st_pdf_page);
private_st_pdf_linearisation_record();
gs_private_st_element(st_pdf_linearisation_record_element, pdf_linearisation_record_t, "pdf_linearstion_record_t[]",
                      pdf_linearisation_record_elt_enum_ptrs, pdf_linearisation_record_elt_reloc_ptrs,
                      st_pdf_linearisation_record);
private_st_device_pdfwrite();
private_st_pdf_substream_save();
private_st_pdf_substream_save_element();

/* GC procedures */
static
ENUM_PTRS_WITH(device_pdfwrite_enum_ptrs, gx_device_pdf *pdev)
{
    index -= gx_device_pdf_num_ptrs + gx_device_pdf_num_param_strings;
    if (index < NUM_RESOURCE_TYPES * NUM_RESOURCE_CHAINS)
        ENUM_RETURN(pdev->resources[index / NUM_RESOURCE_CHAINS].chains[index % NUM_RESOURCE_CHAINS]);
    index -= NUM_RESOURCE_TYPES * NUM_RESOURCE_CHAINS;
    if (index <= pdev->outline_depth && pdev->outline_levels)
        ENUM_RETURN(pdev->outline_levels[index].first.action);
    index -= pdev->outline_depth + 1;
    if (index <= pdev->outline_depth && pdev->outline_levels)
        ENUM_RETURN(pdev->outline_levels[index].last.action);
    index -= pdev->outline_depth + 1;
    ENUM_PREFIX(st_device_psdf, 0);
}
 ENUM_PTR(0, gx_device_pdf, asides.strm);
 ENUM_PTR(1, gx_device_pdf, asides.strm_buf);
 ENUM_PTR(2, gx_device_pdf, asides.save_strm);
 ENUM_PTR(3, gx_device_pdf, streams.strm);
 ENUM_PTR(4, gx_device_pdf, streams.strm_buf);
 ENUM_PTR(5, gx_device_pdf, pictures.strm);
 ENUM_PTR(6, gx_device_pdf, pictures.strm_buf);
 ENUM_PTR(7, gx_device_pdf, pictures.save_strm);
 ENUM_PTR(8, gx_device_pdf, Catalog);
 ENUM_PTR(9, gx_device_pdf, Info);
 ENUM_PTR(10, gx_device_pdf, Pages);
 ENUM_PTR(11, gx_device_pdf, text);
 ENUM_PTR(12, gx_device_pdf, pages);
 ENUM_PTR(13, gx_device_pdf, cs_Patterns[0]);
 ENUM_PTR(14, gx_device_pdf, cs_Patterns[1]);
 ENUM_PTR(15, gx_device_pdf, cs_Patterns[3]);
 ENUM_PTR(16, gx_device_pdf, cs_Patterns[4]);
 ENUM_PTR(17, gx_device_pdf, last_resource);
 ENUM_PTR(18, gx_device_pdf, articles);
 ENUM_PTR(19, gx_device_pdf, Dests);
 ENUM_PTR(20, gx_device_pdf, global_named_objects);
 ENUM_PTR(21, gx_device_pdf, local_named_objects);
 ENUM_PTR(22, gx_device_pdf, NI_stack);
 ENUM_PTR(23, gx_device_pdf, Namespace_stack);
 ENUM_PTR(24, gx_device_pdf, font_cache);
 ENUM_PTR(25, gx_device_pdf, clip_path);
 ENUM_PTR(26, gx_device_pdf, PageLabels);
 ENUM_PTR(27, gx_device_pdf, PageLabels_current_label);
 ENUM_PTR(28, gx_device_pdf, sbstack);
 ENUM_PTR(29, gx_device_pdf, substream_Resources);
 ENUM_PTR(30, gx_device_pdf, font3);
 ENUM_PTR(31, gx_device_pdf, accumulating_substream_resource);
 ENUM_PTR(32, gx_device_pdf, pres_soft_mask_dict);
 ENUM_PTR(33, gx_device_pdf, PDFXTrimBoxToMediaBoxOffset.data);
 ENUM_PTR(34, gx_device_pdf, PDFXBleedBoxToTrimBoxOffset.data);
 ENUM_PTR(35, gx_device_pdf, Identity_ToUnicode_CMaps[0]);
 ENUM_PTR(36, gx_device_pdf, Identity_ToUnicode_CMaps[1]);
 ENUM_PTR(37, gx_device_pdf, vgstack);
 ENUM_PTR(38, gx_device_pdf, outline_levels);
 ENUM_PTR(39, gx_device_pdf, EmbeddedFiles);
 ENUM_PTR(40, gx_device_pdf, pdf_font_dir);
 ENUM_PTR(41, gx_device_pdf, ExtensionMetadata);
 ENUM_PTR(42, gx_device_pdf, PassThroughWriter);
#define e1(i,elt) ENUM_PARAM_STRING_PTR(i + gx_device_pdf_num_ptrs, gx_device_pdf, elt);
gx_device_pdf_do_param_strings(e1)
#undef e1
#define e1(i,elt) ENUM_STRING_PTR(i + gx_device_pdf_num_ptrs + gx_device_pdf_num_param_strings,\
                                 gx_device_pdf, elt);
gx_device_pdf_do_const_strings(e1)
#undef e1
ENUM_PTRS_END
static RELOC_PTRS_WITH(device_pdfwrite_reloc_ptrs, gx_device_pdf *pdev)
{
    RELOC_PREFIX(st_device_psdf);
 RELOC_PTR(gx_device_pdf, asides.strm);
 RELOC_PTR(gx_device_pdf, asides.strm_buf);
 RELOC_PTR(gx_device_pdf, asides.save_strm);
 RELOC_PTR(gx_device_pdf, streams.strm);
 RELOC_PTR(gx_device_pdf, streams.strm_buf);
 RELOC_PTR(gx_device_pdf, pictures.strm);
 RELOC_PTR(gx_device_pdf, pictures.strm_buf);
 RELOC_PTR(gx_device_pdf, pictures.save_strm);
 RELOC_PTR(gx_device_pdf, Catalog);
 RELOC_PTR(gx_device_pdf, Info);
 RELOC_PTR(gx_device_pdf, Pages);
 RELOC_PTR(gx_device_pdf, text);
 RELOC_PTR(gx_device_pdf, pages);
 RELOC_PTR(gx_device_pdf, cs_Patterns[0]);
 RELOC_PTR(gx_device_pdf, cs_Patterns[1]);
 RELOC_PTR(gx_device_pdf, cs_Patterns[3]);
 RELOC_PTR(gx_device_pdf, cs_Patterns[4]);
 RELOC_PTR(gx_device_pdf, last_resource);
 RELOC_PTR(gx_device_pdf, articles);
 RELOC_PTR(gx_device_pdf, Dests);
 RELOC_PTR(gx_device_pdf, global_named_objects);
 RELOC_PTR(gx_device_pdf, local_named_objects);
 RELOC_PTR(gx_device_pdf, NI_stack);
 RELOC_PTR(gx_device_pdf, Namespace_stack);
 RELOC_PTR(gx_device_pdf, font_cache);
 RELOC_PTR(gx_device_pdf, clip_path);
 RELOC_PTR(gx_device_pdf, PageLabels);
 RELOC_PTR(gx_device_pdf, PageLabels_current_label);
 RELOC_PTR(gx_device_pdf, sbstack);
 RELOC_PTR(gx_device_pdf, substream_Resources);
 RELOC_PTR(gx_device_pdf, font3);
 RELOC_PTR(gx_device_pdf, accumulating_substream_resource);
 RELOC_PTR(gx_device_pdf, pres_soft_mask_dict);
 RELOC_PTR(gx_device_pdf, PDFXTrimBoxToMediaBoxOffset.data);
 RELOC_PTR(gx_device_pdf, PDFXBleedBoxToTrimBoxOffset.data);
 RELOC_PTR(gx_device_pdf, Identity_ToUnicode_CMaps[0]);
 RELOC_PTR(gx_device_pdf, Identity_ToUnicode_CMaps[1]);
 RELOC_PTR(gx_device_pdf, vgstack);
 RELOC_PTR(gx_device_pdf, EmbeddedFiles);
 RELOC_PTR(gx_device_pdf, pdf_font_dir);
 RELOC_PTR(gx_device_pdf, ExtensionMetadata);
 RELOC_PTR(gx_device_pdf, PassThroughWriter);
#define r1(i,elt) RELOC_PARAM_STRING_PTR(gx_device_pdf,elt);
        gx_device_pdf_do_param_strings(r1)
#undef r1
#define r1(i,elt) RELOC_CONST_STRING_PTR(gx_device_pdf,elt);
        gx_device_pdf_do_const_strings(r1)
#undef r1
    {
        int i, j;

        for (i = 0; i < NUM_RESOURCE_TYPES; ++i)
            for (j = 0; j < NUM_RESOURCE_CHAINS; ++j)
                RELOC_PTR(gx_device_pdf, resources[i].chains[j]);
        if (pdev->outline_levels) {
            for (i = 0; i <= pdev->outline_depth; ++i) {
                RELOC_PTR(gx_device_pdf, outline_levels[i].first.action);
                RELOC_PTR(gx_device_pdf, outline_levels[i].last.action);
            }
        }
    }
 RELOC_PTR(gx_device_pdf, outline_levels);
}
RELOC_PTRS_END
/* Even though device_pdfwrite_finalize is the same as gx_device_finalize, */
/* we need to implement it separately because st_composite_final */
/* declares all 3 procedures as private. */
static void
device_pdfwrite_finalize(const gs_memory_t *cmem, void *vpdev)
{
    gx_device_finalize(cmem, vpdev);
}

/* Driver procedures */
static dev_proc_open_device(pdf_open);
static dev_proc_output_page(pdf_output_page);
static dev_proc_close_device(pdf_close);
/* Driver procedures defined in other files are declared in gdevpdfx.h. */

#ifndef X_DPI
#  define X_DPI 720
#endif
#ifndef Y_DPI
#  define Y_DPI 720
#endif

/* ---------------- Device prototype ---------------- */

#define PDF_DEVICE_NAME "pdfwrite"
#define PDF_DEVICE_IDENT gs_pdfwrite_device
#define PDF_DEVICE_MaxInlineImageSize 4000
#define PDF_FOR_OPDFREAD 0
#define PDF_FOR_EPS2WRITE 0

#include "gdevpdfb.h"

#undef PDF_DEVICE_NAME
#undef PDF_DEVICE_IDENT
#undef PDF_DEVICE_MaxInlineImageSize
#undef PDF_FOR_OPDFREAD

#define PDF_DEVICE_NAME "ps2write"
#define PDF_DEVICE_IDENT gs_ps2write_device
#define PDF_DEVICE_MaxInlineImageSize max_long
#define PDF_FOR_OPDFREAD 1
#define PDF_FOR_EPS2WRITE 0

#include "gdevpdfb.h"

#undef PDF_DEVICE_NAME
#undef PDF_DEVICE_IDENT
#undef PDF_DEVICE_MaxInlineImageSize
#undef PDF_FOR_OPDFREAD
#undef PDF_FOR_EPS2WRITE

#define PDF_DEVICE_NAME "eps2write"
#define PDF_DEVICE_IDENT gs_eps2write_device
#define PDF_DEVICE_MaxInlineImageSize max_long
#define PDF_FOR_OPDFREAD 1
#define PDF_FOR_EPS2WRITE 1

#include "gdevpdfb.h"

#undef PDF_DEVICE_NAME
#undef PDF_DEVICE_IDENT
#undef PDF_DEVICE_MaxInlineImageSize
#undef PDF_FOR_OPDFREAD
#undef PDF_FOR_EPS2WRITE
/* ---------------- Device open/close ---------------- */

/* Close and remove temporary files. */
static int
pdf_close_temp_file(gx_device_pdf *pdev, pdf_temp_file_t *ptf, int code)
{
    int err = 0;
    gp_file *file = ptf->file;

    /*
     * ptf->strm == 0 or ptf->file == 0 is only possible if this procedure
     * is called to clean up during initialization failure, but ptf->strm
     * might not be open if it was finalized before the device was closed.
     */
    if (ptf->strm) {
        if (s_is_valid(ptf->strm)) {
            sflush(ptf->strm);
            /* Prevent freeing the stream from closing the file. */
            ptf->strm->file = 0;
        } else
            ptf->file = file = 0;	/* file was closed by finalization */
        gs_free_object(pdev->pdf_memory, ptf->strm_buf,
                       "pdf_close_temp_file(strm_buf)");
        ptf->strm_buf = 0;
        gs_free_object(pdev->pdf_memory, ptf->strm,
                       "pdf_close_temp_file(strm)");
        ptf->strm = 0;
    }
    if (file) {
        err = gp_ferror(file) | gp_fclose(file);
        unlink(ptf->file_name);
        ptf->file = 0;
    }
    ptf->save_strm = 0;
    return
        (code < 0 ? code : err != 0 ? gs_note_error(gs_error_ioerror) : code);
}
static int
pdf_close_files(gx_device_pdf * pdev, int code)
{
    code = pdf_close_temp_file(pdev, &pdev->pictures, code);
    code = pdf_close_temp_file(pdev, &pdev->streams, code);
    code = pdf_close_temp_file(pdev, &pdev->asides, code);
    return pdf_close_temp_file(pdev, &pdev->xref, code);
}

/* Reset the state of the current page. */
static void
pdf_reset_page(gx_device_pdf * pdev)
{
    pdev->page_dsc_info = gs_pdfwrite_device.page_dsc_info;
    pdev->contents_id = 0;
    pdf_reset_graphics(pdev);
    pdev->procsets = NoMarks;
    memset(pdev->cs_Patterns, 0, sizeof(pdev->cs_Patterns));	/* simplest to create for each page */
    pdf_reset_text_page(pdev->text);
    pdf_remember_clip_path(pdev, 0);
    pdev->clip_path_id = pdev->no_clip_path_id;
}

/* Open a temporary file, with or without a stream. */
static int
pdf_open_temp_file(gx_device_pdf *pdev, pdf_temp_file_t *ptf)
{
    char fmode[4];

    if (strlen(gp_fmode_binary_suffix) > 2)
        return_error(gs_error_invalidfileaccess);

    strcpy(fmode, "w+");
    strcat(fmode, gp_fmode_binary_suffix);
    ptf->file =	gp_open_scratch_file(pdev->memory,
                                     gp_scratch_file_name_prefix,
                                     ptf->file_name,
                                     fmode);
    if (ptf->file == 0)
        return_error(gs_error_invalidfileaccess);
    return 0;
}
static int
pdf_open_temp_stream(gx_device_pdf *pdev, pdf_temp_file_t *ptf)
{
    int code = pdf_open_temp_file(pdev, ptf);

    if (code < 0)
        return code;
    ptf->strm = s_alloc(pdev->pdf_memory, "pdf_open_temp_stream(strm)");
    if (ptf->strm == 0)
        return_error(gs_error_VMerror);
    ptf->strm_buf = gs_alloc_bytes(pdev->pdf_memory, sbuf_size,
                                   "pdf_open_temp_stream(strm_buf)");
    if (ptf->strm_buf == 0) {
        gs_free_object(pdev->pdf_memory, ptf->strm,
                       "pdf_open_temp_stream(strm)");
        ptf->strm = 0;
        return_error(gs_error_VMerror);
    }
    swrite_file(ptf->strm, ptf->file, ptf->strm_buf, sbuf_size);
    return 0;
}

/* Initialize the IDs allocated at startup. */
void
pdf_initialize_ids(gx_device_pdf * pdev)
{
    gs_param_string nstr;

    pdev->next_id = pdev->FirstObjectNumber;

    /* Initialize the Catalog. */

    param_string_from_string(nstr, "{Catalog}");
    pdf_create_named_dict(pdev, &nstr, &pdev->Catalog, 0L);

    /* Initialize the Info dictionary. */

    param_string_from_string(nstr, "{DocInfo}");
    pdf_create_named_dict(pdev, &nstr, &pdev->Info, 0L);
    {
        char buf[PDF_MAX_PRODUCER];

        pdf_store_default_Producer(buf);
        if (pdev->CompatibilityLevel <= 1.7)
            cos_dict_put_c_key_string(pdev->Info, "/Producer", (byte *)buf,
                                  strlen(buf));
    }
    /*
     * Acrobat Distiller sets CreationDate and ModDate to the current
     * date and time, rather than (for example) %%CreationDate from the
     * PostScript file.  We think this is wrong, but we do the same.
     */
    {
        struct tm tms;
        time_t t;
        char buf[1+2+4+2+2+2+2+2+1+2+1+2+1+1+1]; /* (D:yyyymmddhhmmssZhh'mm')\0 */
        int timeoffset;
        char timesign;

#ifdef CLUSTER
        memset(&t, 0, sizeof(t));
        memset(&tms, 0, sizeof(tms));
        timesign = 'Z';
        timeoffset = 0;
#else
        time(&t);
        tms = *gmtime(&t);
        tms.tm_isdst = -1;
        timeoffset = (int)difftime(t, mktime(&tms)); /* tz+dst in seconds */
        timesign = (timeoffset == 0 ? 'Z' : timeoffset < 0 ? '-' : '+');
        timeoffset = any_abs(timeoffset) / 60;
        tms = *localtime(&t);
#endif

        gs_sprintf(buf, "(D:%04d%02d%02d%02d%02d%02d%c%02d\'%02d\')",
            tms.tm_year + 1900, tms.tm_mon + 1, tms.tm_mday,
            tms.tm_hour, tms.tm_min, tms.tm_sec,
            timesign, timeoffset / 60, timeoffset % 60);

        cos_dict_put_c_key_string(pdev->Info, "/CreationDate", (byte *)buf,
                                  strlen(buf));
        cos_dict_put_c_key_string(pdev->Info, "/ModDate", (byte *)buf,
                                  strlen(buf));
    }

    /* Allocate the root of the pages tree. */

    pdf_create_named_dict(pdev, NULL, &pdev->Pages, 0L);
}

static int
pdf_compute_fileID(gx_device_pdf * pdev)
{
    /* We compute a file identifier when beginning a document
       to allow its usage with PDF encryption. Due to that,
       in contradiction to the Adobe recommendation, our
       ID doesn't depend on the document size.
    */
    gs_memory_t *mem = pdev->pdf_memory;
    stream *strm = pdev->strm;
    uint ignore;
    int code;
    stream *s = s_MD5E_make_stream(mem, pdev->fileID, sizeof(pdev->fileID));
    long secs_ns[2];
    uint KeyLength = pdev->KeyLength;

    if (s == NULL)
        return_error(gs_error_VMerror);
    pdev->KeyLength = 0; /* Disable encryption. Not so important though. */
#ifdef CLUSTER
    secs_ns[0] = 0;
    secs_ns[1] = 0;
#else
    gp_get_realtime(secs_ns);
#endif
    sputs(s, (byte *)secs_ns, sizeof(secs_ns), &ignore);
#ifdef CLUSTER
    /* Don't have the ID's vary by filename output in the cluster testing.
     * This prevents us comparing gs to gpdl results, and makes it harder
     * to manually reproduce results. */
    sputs(s, (const byte *)"ClusterTest.pdf", strlen("ClusterTest.pdf"), &ignore);
#else
    sputs(s, (const byte *)pdev->fname, strlen(pdev->fname), &ignore);
#endif
    pdev->strm = s;
    code = cos_dict_elements_write(pdev->Info, pdev);
    pdev->strm = strm;
    pdev->KeyLength = KeyLength;
    if (code < 0)
        return code;
    sclose(s);
    gs_free_object(mem, s, "pdf_compute_fileID");
    return 0;
}

static const byte pad[32] = { 0x28, 0xBF, 0x4E, 0x5E, 0x4E, 0x75, 0x8A, 0x41,
                               0x64, 0x00, 0x4E, 0x56, 0xFF, 0xFA, 0x01, 0x08,
                               0x2E, 0x2E, 0x00, 0xB6, 0xD0, 0x68, 0x3E, 0x80,
                               0x2F, 0x0C, 0xA9, 0xFE, 0x64, 0x53, 0x69, 0x7A};

static inline void
copy_padded(byte buf[32], gs_param_string *str)
{
    memcpy(buf, str->data, min(str->size, 32));
    if (32 > str->size)
        memcpy(buf + str->size, pad, 32 - str->size);
}

static void
Adobe_magic_loop_50(byte digest[16], int key_length)
{
    gs_md5_state_t md5;
    int i;

    for (i = 0; i < 50; i++) {
        gs_md5_init(&md5);
        gs_md5_append(&md5, digest, key_length);
        gs_md5_finish(&md5, digest);
    }
}

static void
Adobe_magic_loop_19(byte *data, int data_size, const byte *key, int key_size)
{
    stream_arcfour_state sarc4;
    byte key_buf[16];
    int i, j;

    for (i = 1; i <= 19; i++) {
        for (j = 0; j < key_size; j++)
            key_buf[j] = key[j] ^ (byte)i;
        s_arcfour_set_key(&sarc4, key_buf, key_size);
        s_arcfour_process_buffer(&sarc4, data, data_size);
    }
}

static int
pdf_compute_encryption_data(gx_device_pdf * pdev)
{
    gs_md5_state_t md5;
    byte digest[16], buf[32], t;
    stream_arcfour_state sarc4;

    if (pdev->PDFX && pdev->KeyLength != 0) {
        emprintf(pdev->memory,
                 "Encryption is not allowed in a PDF/X doucment.\n");
        return_error(gs_error_rangecheck);
    }
    if (pdev->KeyLength == 0)
        pdev->KeyLength = 40;
    if (pdev->EncryptionV == 0 && pdev->KeyLength == 40)
        pdev->EncryptionV = 1;
    if (pdev->EncryptionV == 0 && pdev->KeyLength > 40)
        pdev->EncryptionV = 2;
    if (pdev->EncryptionV > 1 && pdev->CompatibilityLevel < 1.4) {
        emprintf(pdev->memory, "PDF 1.3 only supports 40 bits keys.\n");
        return_error(gs_error_rangecheck);
    }
    if (pdev->EncryptionR == 0)
        pdev->EncryptionR = 2;
    if (pdev->EncryptionR < 2 || pdev->EncryptionR > 3) {
        emprintf(pdev->memory,
                 "Encryption revisions 2 and 3 are only supported.\n");
        return_error(gs_error_rangecheck);
    }
    if (pdev->EncryptionR > 2 && pdev->CompatibilityLevel < 1.4) {
        emprintf(pdev->memory,
                 "PDF 1.3 only supports the encryption revision 2.\n");
        return_error(gs_error_rangecheck);
    }
    if (pdev->KeyLength > 128) {
        emprintf(pdev->memory,
                 "The maximal length of PDF encryption key is 128 bits.\n");
        return_error(gs_error_rangecheck);
    }
    if (pdev->KeyLength % 8) {
        emprintf(pdev->memory,
                 "PDF encryption key length must be a multiple of 8.\n");
        return_error(gs_error_rangecheck);
    }
    if (pdev->EncryptionR == 2 &&
        ((pdev->Permissions & (0xFFFFFFC3)) != 0xFFFFFFC0)) {
        emprintf(pdev->memory,
                 "Some of Permissions are not allowed with R=2.\n");
        return_error(gs_error_rangecheck);
    }
    if (pdev->EncryptionV == 2 && pdev->EncryptionR == 2 && pdev->KeyLength > 40) {
        emprintf(pdev->memory, "Encryption version 2 revision 2 with "
                 "KeyLength > 40 appears incompatible to some viewers. With "
                 "long keys use revision 3.\n");
        return_error(gs_error_rangecheck);
    }
    /* Compute O : */
    gs_md5_init(&md5);
    copy_padded(buf, &pdev->OwnerPassword);
    gs_md5_append(&md5, buf, sizeof(buf));
    gs_md5_finish(&md5, digest);
    if (pdev->EncryptionR == 3)
        Adobe_magic_loop_50(digest, pdev->KeyLength / 8);
    copy_padded(buf, &pdev->UserPassword);
    s_arcfour_set_key(&sarc4, digest, pdev->KeyLength / 8);
    s_arcfour_process_buffer(&sarc4, buf, sizeof(buf));
    if (pdev->EncryptionR == 3)
        Adobe_magic_loop_19(buf, sizeof(buf), digest, pdev->KeyLength / 8);
    memcpy(pdev->EncryptionO, buf, sizeof(pdev->EncryptionO));
    /* Compute Key : */
    gs_md5_init(&md5);
    copy_padded(buf, &pdev->UserPassword);
    gs_md5_append(&md5, buf, sizeof(buf));
    gs_md5_append(&md5, pdev->EncryptionO, sizeof(pdev->EncryptionO));
    t = (byte)(pdev->Permissions >>  0);  gs_md5_append(&md5, &t, 1);
    t = (byte)(pdev->Permissions >>  8);  gs_md5_append(&md5, &t, 1);
    t = (byte)(pdev->Permissions >> 16);  gs_md5_append(&md5, &t, 1);
    t = (byte)(pdev->Permissions >> 24);  gs_md5_append(&md5, &t, 1);
    gs_md5_append(&md5, pdev->fileID, sizeof(pdev->fileID));
    if (pdev->EncryptionR == 3)
        if (!pdev->EncryptMetadata) {
            const byte v[4] = {0xFF, 0xFF, 0xFF, 0xFF};

            gs_md5_append(&md5, v, 4);
        }
    gs_md5_finish(&md5, digest);
    if (pdev->EncryptionR == 3)
        Adobe_magic_loop_50(digest, pdev->KeyLength / 8);
    memcpy(pdev->EncryptionKey, digest, pdev->KeyLength / 8);
    /* Compute U : */
    if (pdev->EncryptionR == 3) {
        gs_md5_init(&md5);
        gs_md5_append(&md5, pad, sizeof(pad));
        gs_md5_append(&md5, pdev->fileID, sizeof(pdev->fileID));
        gs_md5_finish(&md5, digest);
        s_arcfour_set_key(&sarc4, pdev->EncryptionKey, pdev->KeyLength / 8);
        s_arcfour_process_buffer(&sarc4, digest, sizeof(digest));
        Adobe_magic_loop_19(digest, sizeof(digest), pdev->EncryptionKey, pdev->KeyLength / 8);
        memcpy(pdev->EncryptionU, digest, sizeof(digest));
        memcpy(pdev->EncryptionU + sizeof(digest), pad,
                sizeof(pdev->EncryptionU) - sizeof(digest));
    } else {
        memcpy(pdev->EncryptionU, pad, sizeof(pdev->EncryptionU));
        s_arcfour_set_key(&sarc4, pdev->EncryptionKey, pdev->KeyLength / 8);
        s_arcfour_process_buffer(&sarc4, pdev->EncryptionU, sizeof(pdev->EncryptionU));
    }
    return 0;
}

#ifdef __DECC
/* The ansi alias rules are violated in this next routine.  Tell the compiler
   to ignore this.
 */
#pragma optimize save
#pragma optimize ansi_alias=off
#endif
/*
 * Update the color mapping procedures after setting ProcessColorModel.
 *
 * The 'index' value indicates the ProcessColorModel.
 *	0 = DeviceGray
 *	1 = DeviceRGB
 *	2 = DeviceCMYK
 *	3 = DeviceN (treat like CMYK except for color model name)
 */
void
pdf_set_process_color_model(gx_device_pdf * pdev, int index)
{
    const static gx_device_color_info pcm_color_info[] = {
        dci_values(1, 8, 255, 0, 256, 0),		/* Gray */
        dci_values(3, 24, 255, 255, 256, 256),		/* RGB */
        dci_values(4, 32, 255, 255, 256, 256),		/* CMYK */
        dci_values(4, 32, 255, 255, 256, 256)	/* Treat DeviceN like CMYK */
    };
    /* Don't blow away the ICC information */

    pdev->pcm_color_info_index = index;
    pdev->color_info = pcm_color_info[index];
    /* Set the separable and linear shift, masks, bits. */
    set_linear_color_bits_mask_shift((gx_device *)pdev);
    pdev->color_info.separable_and_linear = GX_CINFO_SEP_LIN;
    /*
     * The conversion from PS to PDF should be transparent as possible.
     * Particularly it should not change representation of colors.
     * Perhaps due to historical reasons the source color information
     * sometimes isn't accessible from device methods, and
     * therefore they perform a mapping of colors to
     * an output color model. Here we handle some color models,
     * which were selected almost due to antique reasons.
     */
    switch (index) {
        case 0:		/* DeviceGray */
            set_dev_proc(pdev, map_rgb_color, gx_default_gray_map_rgb_color);
            set_dev_proc(pdev, map_color_rgb, gx_default_gray_map_color_rgb);
            set_dev_proc(pdev, map_cmyk_color, NULL);
            set_dev_proc(pdev, get_color_mapping_procs,
                        gx_default_DevGray_get_color_mapping_procs);
            set_dev_proc(pdev, get_color_comp_index,
                        gx_default_DevGray_get_color_comp_index);
            set_dev_proc(pdev, encode_color, gx_default_gray_encode);
            set_dev_proc(pdev, decode_color, gx_default_decode_color);
            break;
        case 1:		/* DeviceRGB */
            set_dev_proc(pdev, map_rgb_color, gx_default_rgb_map_rgb_color);
            set_dev_proc(pdev, map_color_rgb, gx_default_rgb_map_color_rgb);
            set_dev_proc(pdev, map_cmyk_color, NULL);
            set_dev_proc(pdev, get_color_mapping_procs,
                        gx_default_DevRGB_get_color_mapping_procs);
            set_dev_proc(pdev, get_color_comp_index,
                        gx_default_DevRGB_get_color_comp_index);
            set_dev_proc(pdev, encode_color, gx_default_rgb_map_rgb_color);
            set_dev_proc(pdev, decode_color, gx_default_rgb_map_color_rgb);
            break;
        case 3:		/* DeviceN - treat like DeviceCMYK except for cm_name */
            pdev->color_info.cm_name = "DeviceN";
        case 2:		/* DeviceCMYK */
            set_dev_proc(pdev, map_rgb_color, NULL);
            set_dev_proc(pdev, map_color_rgb, cmyk_8bit_map_color_rgb);
           /* possible problems with aliassing on next statement */
            set_dev_proc(pdev, map_cmyk_color, cmyk_8bit_map_cmyk_color);
            set_dev_proc(pdev, get_color_mapping_procs,
                        gx_default_DevCMYK_get_color_mapping_procs);
            set_dev_proc(pdev, get_color_comp_index,
                        gx_default_DevCMYK_get_color_comp_index);
            set_dev_proc(pdev, encode_color, cmyk_8bit_map_cmyk_color);
            set_dev_proc(pdev, decode_color, cmyk_8bit_map_color_cmyk);
            break;
        default:	/* can't happen - see the call from gdev_pdf_put_params. */
            DO_NOTHING;
    }
}
#ifdef __DECC
#pragma optimize restore
#endif

/*
 * Reset the text state parameters to initial values.
 */
void
pdf_reset_text(gx_device_pdf * pdev)
{
    /* we need to flush the text buffer, in case we have (eg) Tr set,
     * but have reset it to 0 for the current (buffered) text. If we restore to a
     * graphics state which also has Tr 0 then we won't ever write out the change.
     * I suspect this can theoretically happen with other graphics state values too
     * See PCL file enter.test.
     */
    sync_text_state(pdev);
    pdf_reset_text_state(pdev->text);
}

/* Open the device. */
static int
pdf_open(gx_device * dev)
{
    gx_device_pdf *pdev = (gx_device_pdf *) dev;
    gs_memory_t *mem = pdev->pdf_memory = gs_memory_stable(pdev->memory);
    int code;

    pdev->InOutputPage = false;

    if ((code = pdf_open_temp_file(pdev, &pdev->xref)) < 0 ||
        (code = pdf_open_temp_stream(pdev, &pdev->asides)) < 0 ||
        (code = pdf_open_temp_stream(pdev, &pdev->streams)) < 0 ||
        (code = pdf_open_temp_stream(pdev, &pdev->pictures)) < 0
        )
        goto fail;
    code = gdev_vector_open_file((gx_device_vector *) pdev, sbuf_size);
    if (code < 0)
        goto fail;
    while (pdev->child) {
        /* we've been subclassed by gdev_vector_open_file. Ordinarily we would want to call
         * open_file last, in order to make sure that we don't care if we are subclessed
         * but we want to set up the stream, so we can't do that....
         */
        pdev = (gx_device_pdf *)pdev->child;
    }
    if (pdev->ComputeDocumentDigest) {
        stream *s = s_MD5C_make_stream(pdev->pdf_memory, pdev->strm);

        if (s == NULL)
            return_error(gs_error_VMerror);
        pdev->strm = s;
    }
    gdev_vector_init((gx_device_vector *) pdev);
#ifdef CLUSTER
    memset(&pdev->uuid_time, 0, sizeof(pdev->uuid_time));
#else
    gp_get_realtime(pdev->uuid_time);
#endif
    pdev->vec_procs = &pdf_vector_procs;
    pdev->fill_options = pdev->stroke_options = gx_path_type_optimize;
    /* Set in_page so the vector routines won't try to call */
    /* any vector implementation procedures. */
    pdev->in_page = true;
    /*
     * pdf_initialize_ids allocates some (global) named objects, so we must
     * initialize the named objects dictionary now.
     */
    pdev->local_named_objects =
        pdev->global_named_objects =
        cos_dict_alloc(pdev, "pdf_open(global_named_objects)");
    /* Initialize internal structures that don't have IDs. */
    pdev->NI_stack = cos_array_alloc(pdev, "pdf_open(NI stack)");
    pdev->vgstack = (pdf_viewer_state *)gs_alloc_bytes(pdev->pdf_memory, 11 * sizeof(pdf_viewer_state), "pdf_open(graphics state stack)");
    if (pdev->vgstack == 0) {
        code = gs_error_VMerror;
        goto fail;
    }
    memset(pdev->vgstack, 0x00, 11 * sizeof(pdf_viewer_state));
    pdev->vgstack_size = 11;
    pdev->Namespace_stack = cos_array_alloc(pdev, "pdf_open(Namespace stack)");
    pdf_initialize_ids(pdev);
    code = pdf_compute_fileID(pdev);
    if (code < 0)
        goto fail;
    if (pdev->OwnerPassword.size > 0) {
        code = pdf_compute_encryption_data(pdev);
        if (code < 0)
            goto fail;
    } else if(pdev->UserPassword.size > 0) {
        emprintf(pdev->memory,
                 "User password is specified. Need an Owner password or both.\n");
        return_error(gs_error_rangecheck);
    } else if (pdev->KeyLength) {
        emprintf(pdev->memory,
                 "Can't accept encryption options without a password.\n");
        return_error(gs_error_rangecheck);
    }
    /* Now create a new dictionary for the local named objects. */
    pdev->local_named_objects =
        cos_dict_alloc(pdev, "pdf_open(local_named_objects)");
    pdev->outlines_id = 0;
    pdev->next_page = 0;
    pdev->text = pdf_text_data_alloc(mem);
    pdev->sbstack_size = pdev->vgstack_size; /* Overestimated a few. */
    pdev->sbstack = gs_alloc_struct_array(mem, pdev->sbstack_size, pdf_substream_save,
                                 &st_pdf_substream_save_element, "pdf_open");
    pdev->pages =
        gs_alloc_struct_array(mem, initial_num_pages, pdf_page_t,
                              &st_pdf_page_element, "pdf_open(pages)");
    if (pdev->text == 0 || pdev->pages == 0 || pdev->sbstack == 0) {
        code = gs_error_VMerror;
        goto fail;
    }
    memset(pdev->sbstack, 0, pdev->sbstack_size * sizeof(pdf_substream_save));
    memset(pdev->pages, 0, initial_num_pages * sizeof(pdf_page_t));
    pdev->num_pages = initial_num_pages;
    {
        int i, j;

        for (i = 0; i < NUM_RESOURCE_TYPES; ++i)
            for (j = 0; j < NUM_RESOURCE_CHAINS; ++j)
                pdev->resources[i].chains[j] = 0;
    }
    pdev->outline_levels = (pdf_outline_level_t *)gs_alloc_bytes(mem, INITIAL_MAX_OUTLINE_DEPTH * sizeof(pdf_outline_level_t), "outline_levels array");
    memset(pdev->outline_levels, 0x00, INITIAL_MAX_OUTLINE_DEPTH * sizeof(pdf_outline_level_t));
    pdev->max_outline_depth = INITIAL_MAX_OUTLINE_DEPTH;
    pdev->outline_levels[0].first.id = 0;
    pdev->outline_levels[0].left = max_int;
    pdev->outline_levels[0].first.action = 0;
    pdev->outline_levels[0].last.action = 0;
    pdev->outline_depth = 0;
    pdev->closed_outline_depth = 0;
    pdev->outlines_open = 0;
    pdev->articles = 0;
    pdev->Dests = 0;
    pdev->EmbeddedFiles = 0;
    /* {global,local}_named_objects was initialized above */
    pdev->PageLabels = 0;
    pdev->PageLabels_current_page = 0;
    pdev->PageLabels_current_label = 0;
    pdev->pte = NULL;
    pdf_reset_page(pdev);
    pdev->BBox.p.x = pdev->width;
    pdev->BBox.p.y = pdev->height;
    pdev->BBox.q.x = 0;
    pdev->BBox.q.y = 0;

    if(pdev->UseCIEColor) {
        emprintf(pdev->memory, "\n\nUse of -dUseCIEColor detected!\nSince the release of version 9.11 of Ghostscript we recommend you do not set\n-dUseCIEColor with the pdfwrite/ps2write device family.\n\n");
    }

    /* Build a font cache for pdfwrite, see 'pdf_free_pdf_font_cache' for why we need this. */
    pdev->pdf_font_dir = gs_font_dir_alloc2(pdev->memory->stable_memory, pdev->memory->non_gc_memory);
    if (pdev->pdf_font_dir == 0) {
        code = gs_error_VMerror;
        goto fail;
    }
    /* If we have a gs_lib_ctx, then we need to copy these function pointers from it (we are in PostScript).
     * We can't fill them in otherwise, as the functions are declared static in gsfont.c.
     * If we don't have one then we are in PCL/PXL/XPS, and cannot copy these function pointers. Fortunately
     * we don't need them for fonts in these languages.
     */
    if (pdev->memory->gs_lib_ctx->font_dir) {
        pdev->pdf_font_dir->ccache.mark_glyph = pdev->memory->gs_lib_ctx->font_dir->ccache.mark_glyph;
        pdev->pdf_font_dir->global_glyph_code = pdev->memory->gs_lib_ctx->font_dir->global_glyph_code;
    }

    /* gs_opendevice() sets the device 'is_open' flag which is now of course the parent. We
     * still need to set the child's flag, we'll do it here to avoid setting it if we get any
     * failures, as those will also leave the parent not open.
     */
    if (pdev->parent)
        pdev->is_open = true;

    return 0;
  fail:
    gdev_vector_close_file((gx_device_vector *) pdev);
    return pdf_close_files(pdev, code);
}

/* Detect I/O errors. */
static int
pdf_ferror(gx_device_pdf *pdev)
{
    gp_fflush(pdev->file);
    gp_fflush(pdev->xref.file);
    sflush(pdev->strm);
    sflush(pdev->asides.strm);
    sflush(pdev->streams.strm);
    sflush(pdev->pictures.strm);
    return gp_ferror(pdev->file) || gp_ferror(pdev->xref.file) ||
        gp_ferror(pdev->asides.file) || gp_ferror(pdev->streams.file) ||
        gp_ferror(pdev->pictures.file);
}

/* Compute the dominant text orientation of a page. */
static int
pdf_dominant_rotation(const pdf_text_rotation_t *ptr)
{
    int i, imax = -1;
    long max_count = 0;
    static const int angles[] = { pdf_text_rotation_angle_values };

    for (i = 0; i < countof(ptr->counts); ++i) {
        long count = ptr->counts[i];

        if (count > max_count)
            imax = i, max_count = count;
    }
    return (imax < 0 ? imax : angles[imax]);
}

/* Print a Rotate command, if requested and possible. */
static void
pdf_print_orientation(gx_device_pdf * pdev, pdf_page_t *page)
{
    stream *s = pdev->strm;
    int dsc_orientation = -1;
    const pdf_page_dsc_info_t *ppdi;

    if (pdev->params.AutoRotatePages == arp_None)
        return; /* Not requested. */

    ppdi = (page != NULL ? &page->dsc_info : &pdev->doc_dsc_info);

    /* Determine DSC orientation : */
    if (ppdi->viewing_orientation >= 0)
        dsc_orientation = ppdi->viewing_orientation;
    else if (ppdi->orientation >= 0)
        dsc_orientation = ppdi->orientation;
    if ((page == NULL && pdev->params.AutoRotatePages == arp_All) || /* document */
        (page != NULL && page->text_rotation.Rotate >= 0) || /* page */
        dsc_orientation >= 0 /* have DSC */) {
        const pdf_text_rotation_t *ptr =
            (page != NULL ? &page->text_rotation : &pdev->text_rotation);
        int angle = -1;

        /* Combine DSC rotation with text rotation : */
        if (dsc_orientation == 0) {
            if (ptr->Rotate == 0 || ptr->Rotate == 180)
                angle = ptr->Rotate;
        } else if (dsc_orientation == 1) {
            if (ptr->Rotate == 90 || ptr->Rotate == 270)
                angle = ptr->Rotate;
            else
                angle = 90;
        }

        if (angle < 0) {
        /* If DSC and heuristic disagree, prefer dsc rotation to match the documented Adobe behaviour */
            if (dsc_orientation >= 0)
                angle = dsc_orientation * 90;
            else
                angle = ptr->Rotate;
        }

        /* If got some, write it out : */
        if (angle >= 0)
            pprintd1(s, "/Rotate %d", angle);
    }
}

/* Close the current page. */
static int
pdf_close_page(gx_device_pdf * pdev, int num_copies)
{
    int page_num;
    pdf_page_t *page;
    int code, i;

    while (pdev->FormDepth > 0) {
        pdev->FormDepth--;
        code = pdf_exit_substream(pdev);
        if (code < 0)
            return code;
    }

    /*
     * If the very first page is blank, we need to open the document
     * before doing anything else.
     */

    code = pdfwrite_pdf_open_document(pdev);
    if (code < 0)
        return code;
    if (pdev->ForOPDFRead && pdev->context == PDF_IN_NONE) {
        /* Must create a context stream for empty pages. */
        code = pdf_open_contents(pdev, PDF_IN_STREAM);
        if (code < 0)
            return code;
    }
    pdf_close_contents(pdev, true);

    if (!pdev->DoNumCopies)
        num_copies = 1;

    for(i=0;i<num_copies;i++) {
        bool clear_resource_use = i < num_copies - 1 ? 0 : 1;

        page_num = ++(pdev->next_page);
        /*
         * We can't write the page object or the annotations array yet, because
         * later pdfmarks might add elements to them.  Write the other objects
         * that the page references, and record what we'll need later.
         *
         * Start by making sure the pages array element exists.
         */

        pdf_page_id(pdev, page_num);
        page = &pdev->pages[page_num - 1];
        page->MediaBox.x = pdev->MediaSize[0];
        page->MediaBox.y = pdev->MediaSize[1];
        page->contents_id = pdev->contents_id;
        page->NumCopies_set = pdev->NumCopies_set;
        page->NumCopies = pdev->NumCopies;
        page->UserUnit = pdev->UserUnit;
        pdf_record_usage(pdev, pdev->contents_id, pdev->next_page);
        pdf_record_usage(pdev, pdev->contents_length_id, pdev->next_page);
        pdf_record_usage(pdev, page->Page->id, pdev->next_page);

        /* pdf_store_page_resources sets procsets, resource_ids[]. */
        code = pdf_store_page_resources(pdev, page, clear_resource_use);
        if (code < 0)
            return code;

        /* Write the Functions. */

        code = pdf_write_resource_objects(pdev, resourceFunction);
        if (code < 0)
            return code;

        /* Close use of text on the page. */

        pdf_close_text_page(pdev);

        /* Accumulate text rotation. */

        page->text_rotation.Rotate =
            (pdev->params.AutoRotatePages == arp_PageByPage ?
            pdf_dominant_rotation(&page->text_rotation) : -1);
        {
            int i;

            for (i = 0; i < countof(page->text_rotation.counts); ++i)
                pdev->text_rotation.counts[i] += page->text_rotation.counts[i];
        }

        /* Record information from DSC comments. */

        page->dsc_info = pdev->page_dsc_info;
        if (page->dsc_info.orientation < 0)
            page->dsc_info.orientation = pdev->doc_dsc_info.orientation;
        /* Bug 688793 */
        if (page->dsc_info.viewing_orientation < 0)
        page->dsc_info.viewing_orientation =
           pdev->doc_dsc_info.viewing_orientation;
        if (page->dsc_info.bounding_box.p.x >= page->dsc_info.bounding_box.q.x ||
            page->dsc_info.bounding_box.p.y >= page->dsc_info.bounding_box.q.y
            )
            page->dsc_info.bounding_box = pdev->doc_dsc_info.bounding_box;

        /* Finish up. */

        if(pdf_ferror(pdev))
            return(gs_note_error(gs_error_ioerror));
    }
    pdf_reset_page(pdev);
    return (pdf_ferror(pdev) ? gs_note_error(gs_error_ioerror) : 0);
}

/* Write the page object. */
static double
round_box_coord(double xy)
{
    return (int)(xy * 100 + 0.5) / 100.0;
}
static int
pdf_write_page(gx_device_pdf *pdev, int page_num)
{
    long page_id = pdf_page_id(pdev, page_num);
    pdf_page_t *page = &pdev->pages[page_num - 1];
    double mediabox[4] = {0, 0};
    stream *s;
    const cos_value_t *v_mediabox = cos_dict_find_c_key(page->Page, "/MediaBox");

    /* If we have not been given a MediaBox overriding pdfmark, use the current media size. */
    s = pdev->strm;
    pdf_open_obj(pdev, page_id, resourcePage);

    if (v_mediabox == NULL ) {
        mediabox[2] = round_box_coord(page->MediaBox.x);
        mediabox[3] = round_box_coord(page->MediaBox.y);
        pprintg2(s, "<</Type/Page/MediaBox [0 0 %g %g]\n",
                mediabox[2], mediabox[3]);
    } else {
        const byte *p = v_mediabox->contents.chars.data;
        char buf[100];
        int i, l = min (v_mediabox->contents.chars.size, sizeof(buf) - 1);
        float temp[4]; /* the type is float for sscanf. */

        temp[0] = temp[1] = 0;
        temp[2] = round_box_coord(page->MediaBox.x);
        temp[3] = round_box_coord(page->MediaBox.y);
        memcpy(buf, p, l);
        buf[l] = 0;
        if (sscanf(buf, "[ %g %g %g %g ]",
                &temp[0], &temp[1], &temp[2], &temp[3]) == 4) {
            cos_dict_delete_c_key(page->Page, "/MediaBox");
        }
        pprintg4(s, "<</Type/Page/MediaBox [%g %g %g %g]\n",
                temp[0], temp[1], temp[2], temp[3]);
        for (i=0;i<4;i++)
            mediabox[i] = temp[i];
    }
    if (pdev->PDFX) {
        const cos_value_t *v_trimbox = cos_dict_find_c_key(page->Page, "/TrimBox");
        const cos_value_t *v_artbox = cos_dict_find_c_key(page->Page, "/ArtBox");
        const cos_value_t *v_cropbox = cos_dict_find_c_key(page->Page, "/CropBox");
        const cos_value_t *v_bleedbox = cos_dict_find_c_key(page->Page, "/BleedBox");
        double trimbox[4] = {0, 0}, bleedbox[4] = {0, 0};
        bool print_bleedbox = false;

        trimbox[2] = bleedbox[2] = mediabox[2];
        trimbox[3] = bleedbox[3] = mediabox[3];
        /* Offsets are [left right top bottom] according to the Acrobat 7.0
           distiller parameters manual, 12/7/2004, pp. 102-103. */
        if (v_trimbox != NULL && v_trimbox->value_type == COS_VALUE_SCALAR) {
            const byte *p = v_trimbox->contents.chars.data;
            char buf[100];
            int l = min (v_trimbox->contents.chars.size, sizeof(buf) - 1);
            float temp[4]; /* the type is float for sscanf. */

            memcpy(buf, p, l);
            buf[l] = 0;
            if (sscanf(buf, "[ %g %g %g %g ]",
                    &temp[0], &temp[1], &temp[2], &temp[3]) == 4) {
                trimbox[0] = temp[0];
                trimbox[1] = temp[1];
                trimbox[2] = temp[2];
                trimbox[3] = temp[3];
                cos_dict_delete_c_key(page->Page, "/TrimBox");
            }
            if (v_artbox != NULL && v_artbox->value_type == COS_VALUE_SCALAR)
                cos_dict_delete_c_key(page->Page, "/ArtBox");

        } else if (v_artbox != NULL && v_artbox->value_type == COS_VALUE_SCALAR) {
            /* We have no TrimBox, but we ahve an ArtBox, set the TrimBox to be
             * the supplied ArtBox (TrimBox is preferred for PDF/X)
             */
            const byte *p = v_artbox->contents.chars.data;
            char buf[100];
            int l = min (v_artbox->contents.chars.size, sizeof(buf) - 1);
            float temp[4]; /* the type is float for sscanf. */

            memcpy(buf, p, l);
            buf[l] = 0;
            if (sscanf(buf, "[ %g %g %g %g ]",
                    &temp[0], &temp[1], &temp[2], &temp[3]) == 4) {
                trimbox[0] = temp[0];
                trimbox[1] = temp[1];
                trimbox[2] = temp[2];
                trimbox[3] = temp[3];
                cos_dict_delete_c_key(page->Page, "/ArtBox");
            }
        } else {
            if (pdev->PDFXTrimBoxToMediaBoxOffset.size >= 4 &&
                    pdev->PDFXTrimBoxToMediaBoxOffset.data[0] >= 0 &&
                    pdev->PDFXTrimBoxToMediaBoxOffset.data[1] >= 0 &&
                    pdev->PDFXTrimBoxToMediaBoxOffset.data[2] >= 0 &&
                    pdev->PDFXTrimBoxToMediaBoxOffset.data[3] >= 0) {
                trimbox[0] = mediabox[0] + pdev->PDFXTrimBoxToMediaBoxOffset.data[0];
                trimbox[1] = mediabox[1] + pdev->PDFXTrimBoxToMediaBoxOffset.data[3];
                trimbox[2] = mediabox[2] - pdev->PDFXTrimBoxToMediaBoxOffset.data[1];
                trimbox[3] = mediabox[3] - pdev->PDFXTrimBoxToMediaBoxOffset.data[2];
            }
        }

        if (v_bleedbox != NULL && v_bleedbox->value_type == COS_VALUE_SCALAR) {
            const byte *p = v_bleedbox->contents.chars.data;
            char buf[100];
            int l = min (v_bleedbox->contents.chars.size, sizeof(buf) - 1);
            float temp[4]; /* the type is float for sscanf. */

            memcpy(buf, p, l);
            buf[l] = 0;
            if (sscanf(buf, "[ %g %g %g %g ]",
                    &temp[0], &temp[1], &temp[2], &temp[3]) == 4) {
                if (temp[0] < mediabox[0])
                    bleedbox[0] = mediabox[0];
                else
                    bleedbox[0] = temp[0];
                if (temp[1] < mediabox[1])
                    bleedbox[1] = mediabox[1];
                else
                    bleedbox[1] = temp[1];
                if (temp[2] > mediabox[2])
                    bleedbox[2] = mediabox[2];
                else
                    bleedbox[2] = temp[2];
                if (temp[3] > mediabox[3])
                    bleedbox[3] = mediabox[3];
                else
                    bleedbox[3] = temp[3];
                print_bleedbox = true;
                cos_dict_delete_c_key(page->Page, "/BleedBox");
            }
        } else if (pdev->PDFXSetBleedBoxToMediaBox)
            print_bleedbox = true;
        else if (pdev->PDFXBleedBoxToTrimBoxOffset.size >= 4 &&
                pdev->PDFXBleedBoxToTrimBoxOffset.data[0] >= 0 &&
                pdev->PDFXBleedBoxToTrimBoxOffset.data[1] >= 0 &&
                pdev->PDFXBleedBoxToTrimBoxOffset.data[2] >= 0 &&
                pdev->PDFXBleedBoxToTrimBoxOffset.data[3] >= 0) {
            bleedbox[0] = trimbox[0] - pdev->PDFXBleedBoxToTrimBoxOffset.data[0];
            bleedbox[1] = trimbox[1] - pdev->PDFXBleedBoxToTrimBoxOffset.data[3];
            bleedbox[2] = trimbox[2] + pdev->PDFXBleedBoxToTrimBoxOffset.data[1];
            bleedbox[3] = trimbox[3] + pdev->PDFXBleedBoxToTrimBoxOffset.data[2];
            print_bleedbox = true;
        }

        if (print_bleedbox == true) {
            if (trimbox[0] < bleedbox[0] || trimbox[1] < bleedbox[1] ||
                trimbox[2] > bleedbox[2] || trimbox[3] > bleedbox[3]) {
                switch (pdev->PDFACompatibilityPolicy) {
                    case 0:
                        emprintf(pdev->memory,
                         "TrimBox does not fit inside BleedBox, not permitted in PDF/X-3, reverting to normal PDF output\n");
                        pdev->AbortPDFAX = true;
                        pdev->PDFX = 0;
                        break;
                    case 1:
                        emprintf(pdev->memory,
                         "TrimBox does not fit inside BleedBox, not permitted in PDF/X-3, reducing TrimBox\n");
                        if (trimbox[0] < bleedbox[0])
                            trimbox[0] = bleedbox[0];
                        if (trimbox[1] < bleedbox[1])
                            trimbox[1] = bleedbox[1];
                        if (trimbox[2] > bleedbox[2])
                            trimbox[2] = bleedbox[2];
                        if (trimbox[3] > bleedbox[3])
                            trimbox[3] = bleedbox[3];
                        break;
                    case 2:
                        emprintf(pdev->memory,
                         "TrimBox does not fit inside BleedBox, not permitted in PDF/X-3, aborting conversion\n");
			return_error(gs_error_unknownerror);
                        break;
                    default:
                        emprintf(pdev->memory,
                         "TrimBox does not fit inside BleedBox, not permitted in PDF/X-3\nunrecognised PDFACompatibilityLevel,\nreverting to normal PDF output\n");
                        pdev->AbortPDFAX = true;
                        pdev->PDFX = 0;
                        break;
                }
            }
        }

        if (v_cropbox != NULL && v_cropbox->value_type == COS_VALUE_SCALAR) {
            const byte *p = v_cropbox->contents.chars.data;
            char buf[100];
            int l = min (v_cropbox->contents.chars.size, sizeof(buf) - 1);
            float temp[4]; /* the type is float for sscanf. */

            memcpy(buf, p, l);
            buf[l] = 0;
            if (sscanf(buf, "[ %g %g %g %g ]",
                    &temp[0], &temp[1], &temp[2], &temp[3]) == 4) {
                cos_dict_delete_c_key(page->Page, "/CropBox");
                /* Ensure that CropBox is no larger than MediaBox. The spec says *nothing* about
                 * this, but Acrobat Preflight complains if it is larger. This can happen because
                 * we apply 'round_box_coord' to the mediabox at the start of this rouinte.
                 */
                if (temp[0] < mediabox[0])
                    temp[0] = mediabox[0];
                if (temp[1] < mediabox[1])
                    temp[1] = mediabox[1];
                if (temp[2] > mediabox[2])
                    temp[2] = mediabox[2];
                if (temp[3] > mediabox[3])
                    temp[3] = mediabox[3];
                pprintg4(s, "/CropBox [%g %g %g %g]\n",
                    temp[0], temp[1], temp[2], temp[3]);
                /* Make sure TrimBox fits inside CropBox. Spec says 'must not extend
                 * beyond the boundaries' but Acrobat Preflight complains if they
                 * are the same value.
                 */
                if (trimbox[0] < temp[0] || trimbox[1] < temp[1] ||
                    trimbox[2] > temp[2] || trimbox[3] > temp[3]) {
                    switch (pdev->PDFACompatibilityPolicy) {
                        case 0:
                            emprintf(pdev->memory,
                             "TrimBox does not fit inside CropBox, not permitted in PDF/X-3, reverting to normal PDF output\n");
                            pdev->AbortPDFAX = true;
                            pdev->PDFX = 0;
                            break;
                        case 1:
                            emprintf(pdev->memory,
                             "TrimBox does not fit inside CropBox, not permitted in PDF/X-3, reducing TrimBox\n");
                            if (trimbox[0] < temp[0])
                                trimbox[0] = temp[0];
                            if (trimbox[1] < temp[1])
                                trimbox[1] = temp[1];
                            if (trimbox[2] > temp[2])
                                trimbox[2] = temp[2];
                            if (trimbox[3] > temp[3])
                                trimbox[3] = temp[3];
                            break;
                        case 2:
                            emprintf(pdev->memory,
                             "TrimBox does not fit inside CropBox, not permitted in PDF/X-3, aborting conversion\n");
			    return_error(gs_error_unknownerror);
                            break;
                        default:
                            emprintf(pdev->memory,
                             "TrimBox does not fit inside CropBox, not permitted in PDF/X-3\nunrecognised PDFACompatibilityLevel,\nreverting to normal PDF output\n");
                            pdev->AbortPDFAX = true;
                            pdev->PDFX = 0;
                            break;
                    }
                }
            }
        }

        if (cos_dict_find_c_key(page->Page, "/TrimBox") == NULL &&
            cos_dict_find_c_key(page->Page, "/ArtBox") == NULL)
            pprintg4(s, "/TrimBox [%g %g %g %g]\n",
                trimbox[0], trimbox[1], trimbox[2], trimbox[3]);
        if (print_bleedbox &&
            cos_dict_find_c_key(page->Page, "/BleedBox") == NULL)
            pprintg4(s, "/BleedBox [%g %g %g %g]\n",
                bleedbox[0], bleedbox[1], bleedbox[2], bleedbox[3]);
    }
    pdf_print_orientation(pdev, page);
    if (page->UserUnit != 1)
        pprintg1(s, "/UserUnit %g\n", page->UserUnit);

    pprintld1(s, "/Parent %ld 0 R\n", pdev->Pages->id);
    if (pdev->ForOPDFRead && pdev->DoNumCopies && !pdev->ProduceDSC) {
        if (page->NumCopies_set)
            pprintld1(s, "/NumCopies %ld\n", page->NumCopies);
    }
    if (page->group_id > 0) {
        pprintld1(s, "/Group %ld 0 R\n", page->group_id);
    }
    if (pdev->CompatibilityLevel <= 1.7) {
            stream_puts(s, "/Resources<</ProcSet[/PDF");
        if (page->procsets & ImageB)
            stream_puts(s, " /ImageB");
        if (page->procsets & ImageC)
            stream_puts(s, " /ImageC");
        if (page->procsets & ImageI)
            stream_puts(s, " /ImageI");
        if (page->procsets & Text)
            stream_puts(s, " /Text");
        stream_puts(s, "]\n");
    } else {
        stream_puts(s, "/Resources<<");
    }
    {
        int i;

        for (i = 0; i < countof(page->resource_ids); ++i)
            if (page->resource_ids[i] && pdf_resource_type_names[i]) {
                stream_puts(s, pdf_resource_type_names[i]);
                pprintld1(s, " %ld 0 R\n", page->resource_ids[i]);
            }
    }
    stream_puts(s, ">>\n");

    /* Write the annotations array if any. */

    if (page->Annots) {
        stream_puts(s, "/Annots");
        COS_WRITE(page->Annots, pdev);
        COS_FREE(page->Annots, "pdf_write_page(Annots)");
        page->Annots = 0;
    }
    /*
     * The PDF documentation allows, and this code formerly emitted,
     * a Contents entry whose value was an empty array.  Acrobat Reader
     * 3 and 4 accept this, but Acrobat Reader 5.0 rejects it.
     * Fortunately, the Contents entry is optional.
     */
    if (page->contents_id != 0)
        pprintld1(s, "/Contents %ld 0 R\n", page->contents_id);

    /* Write any elements stored by pdfmarks. */

    cos_dict_elements_write(page->Page, pdev);

    stream_puts(s, ">>\n");
    pdf_end_obj(pdev, resourcePage);
    return 0;
}

/* Wrap up ("output") a page. */
/* if we are doing separate pages, call pdf_close to emit the file, then */
/* pdf_open to open the next page as a new file */
/* NB: converting an input PDF with offpage links will generate warnings */
static int
pdf_output_page(gx_device * dev, int num_copies, int flush)
{
    gx_device_pdf *const pdev = (gx_device_pdf *) dev;
    int code;

    if (pdev->ForOPDFRead) {
        code = pdf_close_page(pdev, num_copies);
        if (code < 0)
            return code;

        while (pdev->sbstack_depth) {
            code = pdf_exit_substream(pdev);
            if (code < 0)
                return code;
        }
    } else {
        while (pdev->sbstack_depth) {
            code = pdf_exit_substream(pdev);
            if (code < 0)
                return code;
        }
        code = pdf_close_page(pdev, num_copies);
        if (code < 0)
            return code;
    }

    if(pdev->UseCIEColor) {
        emprintf(pdev->memory, "\n\nUse of -dUseCIEColor detected!\nSince the release of version 9.11 of Ghostscript we recommend you do not set\n-dUseCIEColor with the pdfwrite/ps2write device family.\n\n");
    }
    if (pdf_ferror(pdev))
        return_error(gs_error_ioerror);

    if ((code = gx_finish_output_page(dev, num_copies, flush)) < 0)
        return code;

    if (gx_outputfile_is_separate_pages(((gx_device_vector *)dev)->fname, dev->memory)) {
        pdev->InOutputPage = true;
        if ((code = pdf_close(dev)) < 0)
            return code;
        code = pdf_open(dev);
        dev->is_open = true;
    }
    return code;
}

static int find_end_xref_section (gx_device_pdf *pdev, gp_file *tfile, int64_t start, gs_offset_t resource_pos)
{
    int64_t start_offset = (start - pdev->FirstObjectNumber) * sizeof(gs_offset_t);

    if (gp_fseek(tfile, start_offset, SEEK_SET) == 0)
    {
        long i, r;

        for (i = start; i < pdev->next_id; ++i) {
            gs_offset_t pos;

            r = gp_fread(&pos, sizeof(pos), 1, tfile);
            if (r != 1)
                return(gs_note_error(gs_error_ioerror));
            if (pos & ASIDES_BASE_POSITION)
                pos += resource_pos - ASIDES_BASE_POSITION;
            pos -= pdev->OPDFRead_procset_length;
            if (pos == 0) {
                return i;
            }
        }
    }
    return pdev->next_id;
}

static int write_xref_section(gx_device_pdf *pdev, gp_file *tfile, int64_t start, int end, gs_offset_t resource_pos, gs_offset_t *Offsets)
{
    int64_t start_offset = (start - pdev->FirstObjectNumber) * sizeof(gs_offset_t);

    if (gp_fseek(tfile, start_offset, SEEK_SET) == 0)
    {
        long i, r;

        for (i = start; i < end; ++i) {
            gs_offset_t pos;
            char str[21];

            r = gp_fread(&pos, sizeof(pos), 1, tfile);
            if (r != 1)
                return(gs_note_error(gs_error_ioerror));
            if (pos & ASIDES_BASE_POSITION)
                pos += resource_pos - ASIDES_BASE_POSITION;
            pos -= pdev->OPDFRead_procset_length;

            /* check to see we haven't got an offset which is too large to represent
             * in an xref (10 digits). Throw an error if we do.
             */
            if (pos > 9999999999) {
                emprintf(pdev->pdf_memory, "ERROR - Attempt to create an xref entry with more than 10 digits which is illegal.\n");
                emprintf(pdev->pdf_memory, "PDF file production has been aborted.\n");
                return_error(gs_error_rangecheck);
            }

            /* If we are linearising there's no point in writing an xref we will
             * later replace. Also makes the file slightly smaller reducing the
             * chances of needing to write white space to pad the file out.
             */
            if (!pdev->Linearise) {
                gs_sprintf(str, "%010"PRId64" 00000 n \n", pos);
                stream_puts(pdev->strm, str);
            }
            if (Offsets)
                Offsets[i] = pos;
        }
    }
    return 0;
}

static int
rewrite_object(gx_device_pdf *const pdev, pdf_linearisation_t *linear_params, int object)
{
    ulong read, Size;
    char c, *Scratch, *source, *target, Buf[280], *next;
    int code, ID, ScratchSize=16384;

    Size = pdev->ResourceUsage[object].Length;

    Scratch = (char *)gs_alloc_bytes(pdev->pdf_memory, ScratchSize, "Working memory for object rewriting");
    if (Scratch == 0L)
        return (gs_note_error(gs_error_VMerror));

    pdev->ResourceUsage[object].LinearisedOffset = gp_ftell(linear_params->Lin_File.file);
    code = gp_fseek(linear_params->sfile, pdev->ResourceUsage[object].OriginalOffset, SEEK_SET);
    if (code < 0)
        return code;

    read = 0;
    do {
        code = gp_fread(&c, 1, 1, linear_params->sfile);
        read++;
    } while (c != '\n' && code > 0);
    gs_sprintf(Scratch, "%d 0 obj\n", pdev->ResourceUsage[object].NewObjectNumber);
    gp_fwrite(Scratch, strlen(Scratch), 1, linear_params->Lin_File.file);

    code = gp_fread(&c, 1, 1, linear_params->sfile);
    if (code != 1)
        return_error(gs_error_ioerror);

    read++;
    if (c == '<' || c == '[') {
        int index = 0;
        Scratch[index++] = c;
        do {
            do {
                code = gp_fread(&c, 1, 1, linear_params->sfile);
                Scratch[index++] = c;
                read++;
                if (index == ScratchSize - 2) {
                    char *Temp;

                    Temp = (char *)gs_alloc_bytes(pdev->pdf_memory, ScratchSize * 2, "Working memory for object rewriting");
                    if (Temp == 0L) {
                        gs_free_object(pdev->pdf_memory, Scratch, "Free working memory for object rewriting");
                        return (gs_note_error(gs_error_VMerror));
                    }
                    memcpy(Temp, Scratch, ScratchSize);
                    gs_free_object(pdev->pdf_memory, Scratch, "Increase working memory for object rewriting");
                    Scratch = Temp;
                    ScratchSize *= 2;
                }
            }while (c != '\r' && c != '\n');
            Scratch[index] = 0;
            if (strncmp(&Scratch[index - 7], "endobj", 6) == 0  || strncmp(&Scratch[index - 7], "stream", 6) == 0)
                break;
        } while (code);
    } else {
        Scratch[0] = 0;
        gp_fwrite(&c, 1, 1, linear_params->Lin_File.file);
    }

    Size -= read;

    source = Scratch;
    do {
        target = strstr(source, " 0 R");
        if (target) {
            next = target + 4;
            do {
                target--;
            }while (*target >= '0' && *target <= '9');
            target++;
            (void)sscanf(target, "%d 0 R", &ID);
            gp_fwrite(source, target - source, 1, linear_params->Lin_File.file);
            gs_sprintf(Buf, "%d 0 R", pdev->ResourceUsage[ID].NewObjectNumber);
            gp_fwrite(Buf, strlen(Buf), 1, linear_params->Lin_File.file);
            source = next;
        } else {
            gp_fwrite(source, strlen(source), 1, linear_params->Lin_File.file);
        }
    } while (target);

    while (Size) {
        if (Size > ScratchSize) {
            code = gp_fread(Scratch, ScratchSize, 1, linear_params->sfile);
            if (code != 1)
	      return_error(gs_error_ioerror);
            gp_fwrite(Scratch, ScratchSize, 1, linear_params->Lin_File.file);
            Size -= 16384;
        } else {
            code = gp_fread(Scratch, Size, 1, linear_params->sfile);
            if (code != 1)
	      return_error(gs_error_ioerror);
            gp_fwrite(Scratch, Size, 1, linear_params->Lin_File.file);
            Size = 0;
        }
    };

    gs_free_object(pdev->pdf_memory, Scratch, "Free working memory for object rewriting");
    return 0;
}

static int flush_hint_stream(pdf_linearisation_t *linear_params)
{
    int code;

    code = gp_fwrite(linear_params->HintBuffer, linear_params->HintByte, 1, linear_params->sfile);
    linear_params->HintBits = 0;
    linear_params->HintByte = 0;
    return code;
}

static int write_hint_stream(pdf_linearisation_t *linear_params, gs_offset_t val, char size_bits)
{
    unsigned int input_mask, output_mask;

    if (size_bits == 0)
        return 0;

    while(size_bits) {
        input_mask = 1 << (size_bits - 1);
        output_mask = 0x80 >> linear_params->HintBits;
        if (input_mask & val)
            linear_params->HintBuffer[linear_params->HintByte] |= output_mask;
        else
            linear_params->HintBuffer[linear_params->HintByte] &= ~output_mask;
        size_bits--;
        linear_params->HintBits++;
        if (linear_params->HintBits == 8) {
            linear_params->HintByte++;
            if (linear_params->HintByte > 254) {
                flush_hint_stream(linear_params);
                memset(linear_params->HintBuffer, 0x00, 256);
            }
            linear_params->HintBits = 0;
        }
    }
    return 0;
}

static int pdf_linearise(gx_device_pdf *pdev, pdf_linearisation_t *linear_params)
{
    char Buffer[1024];
    char Header[32], Binary[9] = "%\307\354\217\242\n", LDict[1024], fileID[35], Pad;
    int level = (int)(pdev->CompatibilityLevel * 10 + 0.5), i, j;
    int code=0, Part1To6 = 2; /* Include space for linearisation dict */
    int Part7To8 = 1;
    int PartNone = 1;
    int Part9 = 1;
    int LDictObj, HintStreamObj, k;
    char T;
    int64_t mainxref, Length, HintStreamLen, HintStreamStart, HintLength, SharedHintOffset;

    fileID[0] = '<';
    fileID[33] = '>';
    fileID[34] = 0x00;
    for (i = 0;i< sizeof(pdev->fileID);i++) {
        T = pdev->fileID[i] >> 4;
        if (T > 9) {
            fileID[(i*2) + 1] = T - 10 + 'A';
        } else {
            fileID[(i*2) + 1] = T + '0';
        }
        T = pdev->fileID[i] & 0x0f;
        if (T > 9) {
            fileID[(i*2) + 2] = T - 10 + 'A';
        } else {
            fileID[(i*2) + 2] = T + '0';
        }
    }

    /* Make sure we've written everything to the main file */
    sflush(pdev->strm);
    linear_params->sfile = pdev->file;
    linear_params->MainFileEnd = gp_ftell(pdev->file);
#ifdef LINEAR_DEBUGGING
    code = gx_device_open_output_file((gx_device *)pdev, "/temp/linear.pdf",
                                   true, true, &linear_params->Lin_File.file);
#else
    code = pdf_open_temp_file(pdev, &linear_params->Lin_File);
#endif
    if (code < 0)
        return code;

    linear_params->Lin_File.strm = 0x0;

    /* Count resources used by page 1 */
    linear_params->NumPage1Resources=0;
    for (i = 0;i < pdev->ResourceUsageSize; i++) {
        if (pdev->ResourceUsage[i].PageUsage == 1)
            linear_params->NumPage1Resources++;
        if (pdev->ResourceUsage[i].PageUsage == resource_usage_part1_structure)
            linear_params->NumPart1StructureResources++;
    }

    /* Count resources associated with pages */
    linear_params->NumUniquePageResources=0;
    linear_params->NumSharedResources=0;
    for (i = 0;i < pdev->ResourceUsageSize; i++) {
        if (pdev->ResourceUsage[i].PageUsage > 1)
            linear_params->NumUniquePageResources++;
        if (pdev->ResourceUsage[i].PageUsage == resource_usage_page_shared)
            linear_params->NumSharedResources++;
    }

    /* Count resources not associated with pages */
    linear_params->NumNonPageResources=0;
    for (i = 1;i < pdev->ResourceUsageSize; i++) {
        if (pdev->ResourceUsage[i].PageUsage == 0)
            linear_params->NumNonPageResources++;
    }

    /* Count resources not associated with pages */
    linear_params->NumPart9Resources=0;
    for (i = 0;i < pdev->ResourceUsageSize; i++) {
        if (pdev->ResourceUsage[i].PageUsage == resource_usage_part9_structure)
            linear_params->NumPart9Resources++;
    }

    Part1To6 += linear_params->NumUniquePageResources + linear_params->NumSharedResources + linear_params->NumNonPageResources + linear_params->NumPart9Resources;
    PartNone += linear_params->NumUniquePageResources + linear_params->NumSharedResources;
    Part9 += linear_params->NumUniquePageResources + linear_params->NumSharedResources + linear_params->NumNonPageResources;
    LDictObj = linear_params->NumUniquePageResources + linear_params->NumSharedResources + linear_params->NumNonPageResources + linear_params->NumPart9Resources + 1;

    /* Record length and positions of all the objects and calculate the new object number */
    for (i = 1;i < pdev->ResourceUsageSize; i++) {
        int j;
        gs_offset_t end;

        pdev->ResourceUsage[i].OriginalOffset = linear_params->Offsets[i];

        end = linear_params->xref;
        for (j=0;j<=linear_params->LastResource;j++) {
            if (linear_params->Offsets[j] > linear_params->Offsets[i] && linear_params->Offsets[j] < end)
                end = linear_params->Offsets[j];
        }
        pdev->ResourceUsage[i].Length = end - linear_params->Offsets[i];

        if (pdev->ResourceUsage[i].PageUsage == 1 || pdev->ResourceUsage[i].PageUsage == resource_usage_part1_structure)
            pdev->ResourceUsage[i].NewObjectNumber = Part1To6++;
        else {
            if (pdev->ResourceUsage[i].PageUsage == resource_usage_not_referenced)
                pdev->ResourceUsage[i].NewObjectNumber = PartNone++;
            else {
                if (pdev->ResourceUsage[i].PageUsage == resource_usage_part9_structure)
                    pdev->ResourceUsage[i].NewObjectNumber = Part9++;
                else
                    pdev->ResourceUsage[i].NewObjectNumber = Part7To8++;
            }
        }
    }
    gs_free_object(pdev->pdf_memory, linear_params->Offsets, "free temp xref storage");

#ifdef LINEAR_DEBUGGING
    {
    dmprintf6(pdev->pdf_memory, "NumPage1Resources %ld\tNumPart1StructureResources %ld\t\tNumUniquePageResources %ld\tNumSharedResources %ld\t\tNumNonPageResources %d\nNumPart9Resources %ld\n", linear_params->NumPage1Resources, linear_params->NumPart1StructureResources, linear_params->NumUniquePageResources, linear_params->NumSharedResources, linear_params->NumNonPageResources, linear_params->NumPart9Resources);
    dmprintf(pdev->pdf_memory, "Old ID\tPage Usage\tNew ID\n");
    for (i = 1;i < pdev->ResourceUsageSize; i++) {
        dmprintf3(pdev->pdf_memory, "%d\t%d\t%d\n", i, pdev->ResourceUsage[i].PageUsage, pdev->ResourceUsage[i].NewObjectNumber);
    }
    }
#endif
    /* Linearisation. Part 1, file header */
    gs_sprintf(Header, "%%PDF-%d.%d\n", level / 10, level % 10);
    gp_fwrite(Header, strlen(Header), 1, linear_params->Lin_File.file);
    if (pdev->binary_ok)
        gp_fwrite(Binary, strlen(Binary), 1, linear_params->Lin_File.file);
    pdfwrite_fwrite_args_comment(pdev, linear_params->Lin_File.file);

    pdf_record_usage(pdev, linear_params->LastResource + 1, resource_usage_part1_structure);
    pdev->ResourceUsage[linear_params->LastResource + 1].OriginalOffset = 0;
    pdev->ResourceUsage[linear_params->LastResource + 1].NewObjectNumber = LDictObj;
    pdev->ResourceUsage[linear_params->LastResource + 1].LinearisedOffset = gp_ftell(linear_params->Lin_File.file);

    /* Linearisation. Part 2, the Linearisation dictioanry */
    linear_params->LDictOffset = gp_ftell(linear_params->Lin_File.file);
    gs_sprintf(LDict, "%d 0 obj\n<<                                                                                                                        \n",
        LDictObj);
    gp_fwrite(LDict, strlen(LDict), 1, linear_params->Lin_File.file);

    /* First page cross-reference table here (Part 3) */
    linear_params->FirstxrefOffset = gp_ftell(linear_params->Lin_File.file);
    gs_sprintf(Header, "xref\n%d %d\n", LDictObj, Part1To6 - LDictObj + 1); /* +1 for the primary hint stream */
    gp_fwrite(Header, strlen(Header), 1, linear_params->Lin_File.file);

    gs_sprintf(Header, "0000000000 00000 n \n");

    for (i = LDictObj;i <= linear_params->LastResource + 2; i++) {
        gp_fwrite(Header, 20, 1, linear_params->Lin_File.file);
    }

    /* Size below is given as the Last Resource in the original file, +1 for object 0 (always free)
     * +1 for the linearisation dict and +1 for the primary hint stream.
     */
    linear_params->FirsttrailerOffset = gp_ftell(linear_params->Lin_File.file);
    gs_sprintf(LDict, "\ntrailer\n<</Size %ld/Info %d 0 R/Root %d 0 R/ID[%s%s]/Prev %d>>\nstartxref\r\n0\n%%%%EOF\n        \n",
        linear_params->LastResource + 3, pdev->ResourceUsage[linear_params->Info_id].NewObjectNumber, pdev->ResourceUsage[linear_params->Catalog_id].NewObjectNumber, fileID, fileID, 0);
    gp_fwrite(LDict, strlen(LDict), 1, linear_params->Lin_File.file);

    /* Write document catalog (Part 4) */
    code = rewrite_object(pdev, linear_params, linear_params->Catalog_id);
    if (code < 0)
        goto error;

    /* In here we need the ViewerPreferences (don't think we support this),
     * the PageMode entry (I think this would be direct in the catalog), The
     * Threads entry (if any), The OpenAction (again, direct object ?) The
     * AcroForm object (don't think we support this) and (TaDa) the Encrypt entry
     * in the first page trailer dictionary.
     */

    /* First page section (Part 6) NB we do not currently support the OpenAction
     * In the Catalogso this is always page 0, this needs to change if we ever
     * support the OpenAction. The 1.7 PDF Reference says in implementation note 181
     * that Acrobat always treats page 0 as the first page for linearisation purposes
     * EVEN IF there is an OpenAction, so we are Acrobat-compatible :-)
     */
    code = rewrite_object(pdev, linear_params, pdev->pages[0].Page->id);
    if (code < 0)
        goto error;
    for (i = 0;i < pdev->ResourceUsageSize; i++) {
        /* we explicitly write the page objct above, make sure when writing the
         * 'objects uniquely used on the page' that we don't write the page object again!
         */
        if (pdev->ResourceUsage[i].PageUsage == 1 && i != pdev->pages[0].Page->id) {
            code = rewrite_object(pdev, linear_params, i);
            if (code < 0)
                goto error;
        }
    }
    linear_params->E = gp_ftell(linear_params->Lin_File.file);

    /* Primary Hint Stream here (Part 5)
     * this is a FIXME
     */
    HintStreamObj = linear_params->LastResource + 2;
    pdf_record_usage(pdev, HintStreamObj, resource_usage_part1_structure);
    pdev->ResourceUsage[HintStreamObj].OriginalOffset = 0;
    pdev->ResourceUsage[HintStreamObj].NewObjectNumber = HintStreamObj;
    pdev->ResourceUsage[HintStreamObj].LinearisedOffset = gp_ftell(linear_params->Lin_File.file);

    /* We don't actually embed the hint stream yet. We will do this when we copy the 'linearised'
     * PDF file from the temp file back to the main file, and will insert the hint stream at this point.
     * We will then know all the offsets of all the objects following the hint stream, so we can
     * write these correctly into the hint stream. Since we will then know the size of the hint
     * stream, we can add this to the stored offsets to generate the 'real' offsets for teh xref.
     */

    /* All remaining pages (part 7) */
    for (i = 1;i < pdev->next_page;i++) {
        code = rewrite_object(pdev, linear_params, pdev->pages[i].Page->id);
        if (code < 0)
            goto error;
        for (j = 0;j < pdev->ResourceUsageSize; j++) {
            /* we explicitly write the page objct above, make sure when writing the
             * 'objects uniquely used on the page' that we don't write the page object again!
             */
            if (pdev->ResourceUsage[j].PageUsage - 1 == i && j != pdev->pages[i].Page->id) {
                code = rewrite_object(pdev, linear_params, j);
                if (code < 0)
                    goto error;
            }
        }
    }

    /* Shared objects for all pages except the first (part 8) */
    for (i = 0;i < pdev->ResourceUsageSize; i++) {
        if (pdev->ResourceUsage[i].PageUsage == resource_usage_page_shared) {
            code = rewrite_object(pdev, linear_params, i);
            if (code < 0)
                goto error;
        }
    }

    /* All objects not on any page (Part 9) */
    for (i = 1;i < pdev->ResourceUsageSize; i++) {
        if (pdev->ResourceUsage[i].PageUsage == resource_usage_not_referenced ||
            pdev->ResourceUsage[i].PageUsage == resource_usage_part9_structure) {
            code = rewrite_object(pdev, linear_params, i);
            if (code < 0)
                goto error;
        }
    }

    /* We won't bother with an overflow hint stream (I Hope) (part 10)
     * Implementation Note 181 in the 1.7 PDF Reference Manual says Acrobat
     * doesn't support overflow hint streams anyway....
     */

    /* Now copy the linearised data back to the main file, as far as the offset of the
     * primary hint stream
     */
    if (gp_fseek(linear_params->Lin_File.file, 0, SEEK_SET) != 0) {
        code = gs_error_ioerror;
        goto error;
    }
    if (gp_fseek(linear_params->sfile, 0, SEEK_SET) != 0){
        code = gs_error_ioerror;
        goto error;
    }
    Length = pdev->ResourceUsage[HintStreamObj].LinearisedOffset;
    while (Length && code >= 0) {
        if (Length > 1024) {
            code = gp_fread(Buffer, 1024, 1, linear_params->Lin_File.file);
            gp_fwrite(Buffer, 1024, 1, linear_params->sfile);
            Length -= 1024;
        } else {
            code = gp_fread(Buffer, Length, 1, linear_params->Lin_File.file);
            gp_fwrite(Buffer, Length, 1, linear_params->sfile);
            Length = 0;
        }
    }
    /* insert the primary hint stream */
    gs_sprintf(LDict, "%d 0 obj\n<</Length           \n/S           >>\nstream\n", HintStreamObj);
    gp_fwrite(LDict, strlen(LDict), 1, linear_params->sfile);

    HintStreamStart = gp_ftell(linear_params->sfile);
    memset(linear_params->HintBuffer, 0x00, 256);
    linear_params->HintBits = linear_params->HintByte = 0;

    linear_params->PageHints = (page_hint_stream_t *)gs_alloc_bytes(pdev->pdf_memory, pdev->next_page * sizeof(page_hint_stream_t), "Hints for the pages");
    memset(linear_params->PageHints, 0x00, pdev->next_page * sizeof(page_hint_stream_t));
    linear_params->NumPageHints = pdev->next_page;

    linear_params->SharedHints = (shared_hint_stream_t *)gs_alloc_bytes(pdev->pdf_memory, (linear_params->NumPage1Resources + linear_params->NumSharedResources) * sizeof(shared_hint_stream_t), "Hints for the shared objects");
    memset(linear_params->SharedHints, 0x00, (linear_params->NumPage1Resources + linear_params->NumSharedResources) * sizeof(shared_hint_stream_t));
    linear_params->NumSharedHints = linear_params->NumPage1Resources + linear_params->NumSharedResources;

    memset(&linear_params->PageHintHeader,0x00, sizeof(page_hint_stream_header_t));
    memset(&linear_params->SharedHintHeader,0x00, sizeof(shared_hint_stream_header_t));

    linear_params->PageHintHeader.LeastObjectsPerPage = 0x7fffffff;
    linear_params->PageHintHeader.LeastPageLength = 0x7fffffff;
    linear_params->PageHintHeader.LeastPageOffset = 0x7fffffff;
    linear_params->PageHintHeader.LeastContentLength = 0x7fffffff;

    /* Record the offset and length of the content stream for each page */
    for (i=0;i<pdev->next_page;i++) {
        int PageContentID = pdev->pages[i].contents_id;
        pdf_linearisation_record_t *record = &pdev->ResourceUsage[PageContentID];
        page_hint_stream_t *pagehint = &linear_params->PageHints[i];

        pagehint->ContentOffset = record->LinearisedOffset;
        pagehint->ContentLength = record->Length;
    }

    /* For every object, record it in the page hints and shared hints. k is a counter
     * of the current shared object, we increment it each time we find a new one.
     */
    k = 0;
    for (i = 1;i < pdev->ResourceUsageSize; i++) {
        pdf_linearisation_record_t *record = &pdev->ResourceUsage[i];

        if (record->PageUsage < resource_usage_page_shared || record->PageUsage == resource_usage_not_referenced)
            continue;
        if (record->PageUsage == resource_usage_page_shared) {
            /* shared objects are recorded in the shared object hints, and also the page hints */
            for (j=0;j<record->NumPagesUsing;j++) {
                int page = record->PageList[j];
                page_hint_stream_t *pagehint;

                if (page >= pdev->next_page)
                    /* This can happen if the job makes marks, but does not call showpage */
                    continue;

                pagehint = &linear_params->PageHints[page - 1];
                if (pagehint->SharedObjectRef){
                    int *Temp = (int *)gs_alloc_bytes(pdev->pdf_memory, (pagehint->NumSharedObjects + 1) * sizeof(int), "realloc shared object hints");
                    memcpy(Temp, pagehint->SharedObjectRef, (pagehint->NumSharedObjects) * sizeof(int));
                    gs_free_object(pdev->pdf_memory, pagehint->SharedObjectRef, "realloc shared object hints");
                    pagehint->SharedObjectRef = (unsigned int *)Temp;
                } else {
                    pagehint->SharedObjectRef = (unsigned int *)gs_alloc_bytes(pdev->pdf_memory, (pagehint->NumSharedObjects + 1) * sizeof(int), "shared object hints");
                }
                pagehint->SharedObjectRef[pagehint->NumSharedObjects] = i;
                pagehint->NumSharedObjects++;
            }
            linear_params->SharedHints[k].ObjectNumber = record->NewObjectNumber;
            linear_params->SharedHints[k].ObjectOffset = record->LinearisedOffset;
            linear_params->SharedHints[k++].ObjectLength = record->Length;
        } else {
            /* Objects uniquely used on a page are stored in the page hints table, except
             * page 1 whose objects are *also* stored in the shared objects hints.
             */
            int page = record->PageUsage, pageID = pdev->pages[page - 1].Page->id;
            int64_t LinearisedPageOffset = pdev->ResourceUsage[pageID].LinearisedOffset;
            page_hint_stream_t *pagehint;

            /* If the final page makes marks but does not call showpage we don't emit it
             * which can lead to references to non-existent pages.
             */
            if (page < pdev->next_page) {
                pagehint = &linear_params->PageHints[page - 1];
                pagehint->NumUniqueObjects++;
                if (record->LinearisedOffset - LinearisedPageOffset > pagehint->PageLength)
                    pagehint->PageLength = (record->LinearisedOffset + record->Length) - LinearisedPageOffset;
                if (page == 1) {
                    linear_params->SharedHintHeader.FirstPageEntries++;
                }
            }
        }
    }

    linear_params->PageHintHeader.LargestSharedObject = k;

    for (i = 0;i < linear_params->NumPageHints;i++) {
        page_hint_stream_t *hint = &linear_params->PageHints[i];

        if (hint->NumUniqueObjects + hint->NumSharedObjects < linear_params->PageHintHeader.LeastObjectsPerPage)
            linear_params->PageHintHeader.LeastObjectsPerPage = hint->NumUniqueObjects + hint->NumSharedObjects;
        if (hint->NumUniqueObjects + hint->NumSharedObjects > linear_params->PageHintHeader.MostObjectsPerPage)
            linear_params->PageHintHeader.MostObjectsPerPage = hint->NumUniqueObjects + hint->NumSharedObjects;
        if (hint->PageLength < linear_params->PageHintHeader.LeastPageLength)
            linear_params->PageHintHeader.LeastPageLength = hint->PageLength;
        if (hint->PageLength > linear_params->PageHintHeader.MostPageLength)
            linear_params->PageHintHeader.MostPageLength = hint->PageLength;
        if (hint->ContentOffset < linear_params->PageHintHeader.LeastPageOffset)
            linear_params->PageHintHeader.LeastPageOffset = hint->ContentOffset;
        if (hint->ContentOffset > linear_params->PageHintHeader.MostPageOffset)
            linear_params->PageHintHeader.MostPageOffset = hint->ContentOffset;
        if (hint->ContentLength < linear_params->PageHintHeader.LeastContentLength)
            linear_params->PageHintHeader.LeastContentLength = hint->ContentLength;
        if (hint->ContentLength > linear_params->PageHintHeader.MostContentLength)
            linear_params->PageHintHeader.MostContentLength = hint->ContentLength;
        if (hint->NumSharedObjects > linear_params->PageHintHeader.MostSharedObjects)
            linear_params->PageHintHeader.MostSharedObjects = hint->NumSharedObjects;
    }

    write_hint_stream(linear_params, (gs_offset_t)linear_params->PageHintHeader.LeastObjectsPerPage, 32);
    write_hint_stream(linear_params, (gs_offset_t)pdev->ResourceUsage[pdev->pages[0].Page->id].LinearisedOffset, 32);
    i = (linear_params->PageHintHeader.MostObjectsPerPage - linear_params->PageHintHeader.MostObjectsPerPage + 1);
    j = 0;
    while (i) {
        i = i / 2;
        j++;
    }
    linear_params->PageHintHeader.ObjectNumBits = j;
    write_hint_stream(linear_params, (gs_offset_t)linear_params->PageHintHeader.ObjectNumBits, 16);
    write_hint_stream(linear_params, (gs_offset_t)linear_params->PageHintHeader.LeastPageLength, 32);
    i = (linear_params->PageHintHeader.MostPageLength - linear_params->PageHintHeader.LeastPageLength + 1);
    j = 0;
    while (i) {
        i = i / 2;
        j++;
    }
    linear_params->PageHintHeader.PageLengthNumBits = j;
    write_hint_stream(linear_params, (gs_offset_t)linear_params->PageHintHeader.PageLengthNumBits, 16);
    write_hint_stream(linear_params, (gs_offset_t)linear_params->PageHintHeader.LeastPageOffset, 32);
    i = (linear_params->PageHintHeader.MostPageOffset - linear_params->PageHintHeader.LeastPageOffset + 1);
    j = 0;
    while (i) {
        i = i / 2;
        j++;
    }
    linear_params->PageHintHeader.PageOffsetNumBits = j;
    write_hint_stream(linear_params, (gs_offset_t)linear_params->PageHintHeader.PageOffsetNumBits, 16);
    write_hint_stream(linear_params, (gs_offset_t)linear_params->PageHintHeader.LeastContentLength, 32);
    i = (linear_params->PageHintHeader.MostContentLength - linear_params->PageHintHeader.LeastContentLength + 1);
    j = 0;
    while (i) {
        i = i / 2;
        j++;
    }
    linear_params->PageHintHeader.ContentLengthNumBits = j;
    write_hint_stream(linear_params, (gs_offset_t)linear_params->PageHintHeader.ContentLengthNumBits, 16);
    write_hint_stream(linear_params, (gs_offset_t)linear_params->PageHintHeader.MostSharedObjects, 16);
    i = (linear_params->PageHintHeader.LargestSharedObject + 1);
    j = 0;
    while (i) {
        i = i / 2;
        j++;
    }
    linear_params->PageHintHeader.SharedObjectNumBits = j;
    write_hint_stream(linear_params, (gs_offset_t)linear_params->PageHintHeader.SharedObjectNumBits, 16);
    j = 1;
    write_hint_stream(linear_params, (gs_offset_t)j, 16);
    write_hint_stream(linear_params, (gs_offset_t)j, 16);

#ifdef LINEAR_DEBUGGING
    dmprintf1(pdev->pdf_memory, "LeastObjectsPerPage %d\n", linear_params->PageHintHeader.LeastObjectsPerPage);
    dmprintf1(pdev->pdf_memory, "Page 1 Offset %"PRId64"\n", pdev->ResourceUsage[pdev->pages[0].Page->id].LinearisedOffset);
    dmprintf1(pdev->pdf_memory, "ObjectNumBits %d\n", linear_params->PageHintHeader.ObjectNumBits);
    dmprintf1(pdev->pdf_memory, "LeastPageLength %d\n", linear_params->PageHintHeader.LeastPageLength);
    dmprintf1(pdev->pdf_memory, "MostPagelength %d\n", linear_params->PageHintHeader.MostPageLength);
    dmprintf1(pdev->pdf_memory, "PaegLengthNumBits %d\n", linear_params->PageHintHeader.PageLengthNumBits);
    dmprintf1(pdev->pdf_memory, "LeastPageOffset %d\n", linear_params->PageHintHeader.LeastPageOffset);
    dmprintf1(pdev->pdf_memory, "MostPageOffset %d\n", linear_params->PageHintHeader.MostPageOffset);
    dmprintf1(pdev->pdf_memory, "PageOffsetNumBits %d\n", linear_params->PageHintHeader.PageOffsetNumBits);
    dmprintf1(pdev->pdf_memory, "LeastContentLength %d\n", linear_params->PageHintHeader.LeastContentLength);
    dmprintf1(pdev->pdf_memory, "MostContentLength %d\n", linear_params->PageHintHeader.MostContentLength);
    dmprintf1(pdev->pdf_memory, "COntentLengthNumBits %d\n", linear_params->PageHintHeader.ContentLengthNumBits);
    dmprintf1(pdev->pdf_memory, "MostSharedObjects %d\n", linear_params->PageHintHeader.MostSharedObjects);
    dmprintf1(pdev->pdf_memory, "LargetsSharedObject %d\n", linear_params->PageHintHeader.LargestSharedObject);
    dmprintf1(pdev->pdf_memory, "SharedObjectNumBits %d\n", linear_params->PageHintHeader.SharedObjectNumBits);
    dmprintf(pdev->pdf_memory, "Position Numerator 1\n");
    dmprintf(pdev->pdf_memory, "Position Denominator 1\n\n");
#endif

    for (i=0;i < pdev->next_page;i++) {
        page_hint_stream_t *hint = &linear_params->PageHints[i];
        int Num;

        Num = hint->NumUniqueObjects - linear_params->PageHintHeader.LeastObjectsPerPage;
        write_hint_stream(linear_params, (gs_offset_t)Num, linear_params->PageHintHeader.ObjectNumBits);
        dmprintf2(pdev->pdf_memory, "Page %d NumUniqueObjects %d\n", i, Num);
    }
    for (i=0;i < pdev->next_page;i++) {
        page_hint_stream_t *hint = &linear_params->PageHints[i];
        int Num;

        Num = hint->PageLength - linear_params->PageHintHeader.LeastPageLength;
        write_hint_stream(linear_params, (gs_offset_t)Num, linear_params->PageHintHeader.PageLengthNumBits);
        dmprintf2(pdev->pdf_memory, "Page %d PageLength %d\n", i, Num);
    }
    for (i=0;i < pdev->next_page;i++) {
        page_hint_stream_t *hint = &linear_params->PageHints[i];

        if (i == 0) {
            write_hint_stream(linear_params, (gs_offset_t)i, linear_params->PageHintHeader.SharedObjectNumBits);
            dmprintf2(pdev->pdf_memory, "Page %d NumSharedObjects %d\n", i, 1);
        }
        else {
            write_hint_stream(linear_params, (gs_offset_t)hint->NumSharedObjects, linear_params->PageHintHeader.SharedObjectNumBits);
            dmprintf2(pdev->pdf_memory, "Page %d NumSharedObjects %d\n", i, hint->NumSharedObjects);
        }
    }
    for (i=1;i < pdev->next_page;i++) {
        page_hint_stream_t *hint = &linear_params->PageHints[i];

        for (j=0;j < hint->NumSharedObjects;j++) {
            write_hint_stream(linear_params, (gs_offset_t)hint->SharedObjectRef[j], linear_params->PageHintHeader.SharedObjectNumBits);
            dmprintf3(pdev->pdf_memory, "Page %d SharedObject %d ObjectRef %d\n", i, j, hint->SharedObjectRef[j]);
        }
    }

    for (i=0;i < pdev->next_page;i++) {
        page_hint_stream_t *hint = &linear_params->PageHints[i];

        for (j=0;j < hint->NumSharedObjects;j++) {
            write_hint_stream(linear_params, (gs_offset_t)j, 1);
            dmprintf2(pdev->pdf_memory, "Page %d SharedObject %d Position Numerator 1\n", i, j);
        }
    }
    for (i=1;i < pdev->next_page;i++) {
        page_hint_stream_t *hint = &linear_params->PageHints[i];
        int Num;

        Num = hint->ContentOffset - linear_params->PageHintHeader.LeastPageOffset;
        write_hint_stream(linear_params, (gs_offset_t)Num, linear_params->PageHintHeader.PageOffsetNumBits);
        dmprintf2(pdev->pdf_memory, "Page %d ContentStreamOffset %d\n", i, Num);
    }
    for (i=1;i < pdev->next_page;i++) {
        page_hint_stream_t *hint = &linear_params->PageHints[i];
        int Num;

        Num = hint->ContentLength - linear_params->PageHintHeader.LeastContentLength;
        write_hint_stream(linear_params, (gs_offset_t)Num, linear_params->PageHintHeader.ContentLengthNumBits);
        dmprintf2(pdev->pdf_memory, "Page %d ContentStreamLength %d\n", i, Num);
    }
    flush_hint_stream(linear_params);

    SharedHintOffset = gp_ftell(linear_params->sfile) - HintStreamStart;
    linear_params->SharedHintHeader.FirstSharedObject = linear_params->SharedHints[0].ObjectNumber;
    linear_params->SharedHintHeader.LeastObjectLength = linear_params->SharedHints[0].ObjectLength;
    linear_params->SharedHintHeader.MostObjectLength = linear_params->SharedHints[0].ObjectLength;

    for (i = 1; i< linear_params->NumSharedHints; i++) {
        if (linear_params->SharedHints[i].ObjectNumber < linear_params->SharedHintHeader.FirstSharedObject) {
            linear_params->SharedHintHeader.FirstSharedObject = linear_params->SharedHints[1].ObjectNumber;
            linear_params->SharedHintHeader.FirstObjectOffset = linear_params->SharedHints[1].ObjectOffset;
        }
        if (linear_params->SharedHints[i].ObjectLength < linear_params->SharedHintHeader.LeastObjectLength) {
            linear_params->SharedHintHeader.LeastObjectLength = linear_params->SharedHints[i].ObjectLength;
        }
        if (linear_params->SharedHints[i].ObjectLength > linear_params->SharedHintHeader.MostObjectLength) {
            linear_params->SharedHintHeader.MostObjectLength = linear_params->SharedHints[i].ObjectLength;
        }
    }

    linear_params->SharedHintHeader.FirstPageEntries = linear_params->NumPage1Resources;
    linear_params->SharedHintHeader.NumSharedObjects = linear_params->NumSharedResources + linear_params->SharedHintHeader.FirstPageEntries;

    write_hint_stream(linear_params, (gs_offset_t)linear_params->SharedHintHeader.FirstSharedObject, 32);
    dmprintf1(pdev->pdf_memory, "\nFirstSharedObject %d\n", linear_params->SharedHintHeader.FirstSharedObject);
    write_hint_stream(linear_params, (gs_offset_t)linear_params->SharedHintHeader.FirstObjectOffset, 32);
    dmprintf1(pdev->pdf_memory, "FirstObjectOffset %"PRId64"\n", linear_params->SharedHintHeader.FirstObjectOffset);
    write_hint_stream(linear_params, (gs_offset_t)linear_params->SharedHintHeader.FirstPageEntries, 32);
    dmprintf1(pdev->pdf_memory, "FirstPageEntries %d\n", linear_params->SharedHintHeader.FirstPageEntries);
    write_hint_stream(linear_params, (gs_offset_t)linear_params->SharedHintHeader.NumSharedObjects, 32);
    dmprintf1(pdev->pdf_memory, "NumSharedObjects %d\n", linear_params->SharedHintHeader.NumSharedObjects);
    j = 1;
    write_hint_stream(linear_params, (gs_offset_t)j, 32);
    dmprintf(pdev->pdf_memory, "GreatestObjectsNumBits 1\n");
    write_hint_stream(linear_params, (gs_offset_t)linear_params->SharedHintHeader.FirstObjectOffset, 16);
    dmprintf1(pdev->pdf_memory, "FirstObjectOffset %"PRId64"\n", linear_params->SharedHintHeader.FirstObjectOffset);
    write_hint_stream(linear_params, (gs_offset_t)linear_params->SharedHintHeader.LeastObjectLength, 32);
    dmprintf1(pdev->pdf_memory, "LeastObjectLength %d\n", linear_params->SharedHintHeader.LeastObjectLength);

    i = (linear_params->SharedHintHeader.MostObjectLength - linear_params->SharedHintHeader.LeastObjectLength + 1) / 2;
    j = 0;
    while (i) {
        i = i / 2;
        j++;
    }
    linear_params->SharedHintHeader.LengthNumBits = j + 1;
    write_hint_stream(linear_params, (gs_offset_t)linear_params->SharedHintHeader.LengthNumBits, 16);

    for (i = 0; i< linear_params->NumSharedHints; i++) {
        unsigned int Length = linear_params->SharedHints[i].ObjectLength - linear_params->SharedHintHeader.LeastObjectLength;

        write_hint_stream(linear_params, (gs_offset_t)Length, linear_params->SharedHintHeader.LengthNumBits);
        dmprintf2(pdev->pdf_memory, "Shared Object group %d, Length %d\n", i, Length);
    }

    j = 0;
    for (i = 0; i< linear_params->NumSharedHints; i++) {
        write_hint_stream(linear_params, (gs_offset_t)j, 1);
        dmprintf1(pdev->pdf_memory, "Shared Object group %d, SignatureFlag false\n", i);
    }
    for (i = 0; i< linear_params->NumSharedHints; i++) {
        write_hint_stream(linear_params, (gs_offset_t)j, 1);
        dmprintf1(pdev->pdf_memory, "Shared Object group %d, NumObjects 1\n", i);
    }

    flush_hint_stream(linear_params);
    HintLength = gp_ftell(linear_params->sfile) - HintStreamStart;

    gs_sprintf(LDict, "\nendstream\nendobj\n");
    gp_fwrite(LDict, strlen(LDict), 1, linear_params->sfile);
    /* Calculate the length of the primary hint stream */
    HintStreamLen = gp_ftell(linear_params->sfile) - pdev->ResourceUsage[HintStreamObj].LinearisedOffset;
    /* Read remainder of linearised file and write to the final file */
    do {
        code = gp_fread(Buffer, 1, 1024, linear_params->Lin_File.file);
        if (code > 0)
            gp_fwrite(Buffer, 1, code, linear_params->sfile);
    } while (code > 0);

    /* Main xref (part 11) */
    mainxref = gp_ftell(linear_params->sfile);
    /* Acrobat 9 and X (possibly other versions) won't recognise a file as
     * optimised unless the file is at least 4k bytes in length (!!!)
     * Also, it is possible that the new file might be smaller than the old one, If a
     * frequently used object changed to a lower number (eg form object 100 to object 10)
     * We don't close and reopen the file, so we need to make sure that any difference
     * is filled in with white space.
     */
    if (linear_params->MainFileEnd < 4096)
        Length = 4096 - (mainxref + strlen(LDict) + strlen(Header) + LDictObj * 20);
    else
        Length = linear_params->MainFileEnd - (mainxref + strlen(LDict) + strlen(Header) + LDictObj * 20);
    Pad = ' ';

    while(Length > 0) {
        gp_fwrite(&Pad, 1, 1, linear_params->sfile);
        Length--;
    }

    /* Now the file is long enough, write the xref */
    mainxref = gp_ftell(linear_params->sfile);
    gs_sprintf(Header, "xref\n0 %d\n", LDictObj);
    gp_fwrite(Header, strlen(Header), 1, linear_params->sfile);

    linear_params->T = gp_ftell(linear_params->sfile) - 1;
    gs_sprintf(Header, "0000000000 65535 f \n");
    gp_fwrite(Header, strlen(Header), 1, linear_params->sfile);

    for (i = 1;i < LDictObj; i++) {
        for (j = 0; j < pdev->ResourceUsageSize;j++) {
            if (pdev->ResourceUsage[j].NewObjectNumber == i) {
                gs_sprintf(Header, "%010"PRId64" 00000 n \n", pdev->ResourceUsage[j].LinearisedOffset + HintStreamLen);
                gp_fwrite(Header, 20, 1, linear_params->sfile);
            }
        }
    }

    gs_sprintf(LDict, "trailer\n<</Size %d>>\nstartxref\n%"PRId64"\n%%%%EOF\n",
        LDictObj, linear_params->FirstxrefOffset);
    gp_fwrite(LDict, strlen(LDict), 1, linear_params->sfile);

    linear_params->FileLength = gp_ftell(linear_params->sfile);
    /* Return to the linearisation dictionary and write it again filling
     * in the missing data.
     */
    /* Implementation Note 178 in the PDF Reference 1.7 says that Acrobat requires
     * a white space after the '[' for the hint stream offset. This appears not to be true
     * for current versions, but we follow it anyway for the possible benefit of older
     * versions.
     */
    gp_fseek(linear_params->sfile, linear_params->LDictOffset, SEEK_SET);
    gs_sprintf(LDict, "%d 0 obj\n<</Linearized 1/L %"PRId64"/H[ ", LDictObj, linear_params->FileLength);
    gp_fwrite(LDict, strlen(LDict), 1, linear_params->sfile);

    gs_sprintf(LDict, "%"PRId64"", pdev->ResourceUsage[HintStreamObj].LinearisedOffset);
    gp_fwrite(LDict, strlen(LDict), 1, linear_params->sfile);
    gs_sprintf(LDict, " %"PRId64"]", HintStreamLen);
    gp_fwrite(LDict, strlen(LDict), 1, linear_params->sfile);
    /* Implementation Note 180 in hte PDF Reference 1.7 says that Acrobat
     * gets the 'E' value wrong. So its probably not important....
     */
    gs_sprintf(LDict, "/O %d/E %"PRId64"",pdev->ResourceUsage[pdev->pages[0].Page->id].NewObjectNumber, linear_params->E);
    gp_fwrite(LDict, strlen(LDict), 1, linear_params->sfile);
    gs_sprintf(LDict, "/N %d/T %"PRId64">>\nendobj\n", pdev->next_page, linear_params->T);
    gp_fwrite(LDict, strlen(LDict), 1, linear_params->sfile);

    /* Return to the secondary xref and write it again filling
     * in the missing offsets.
     */
    if (gp_fseek(linear_params->sfile, linear_params->FirstxrefOffset, SEEK_SET) != 0) {
        code = gs_error_ioerror;
        goto error;
    }
    gs_sprintf(Header, "xref\n%d %d\n", LDictObj, Part1To6 - LDictObj + 1); /* +1 for the primary hint stream */
    gp_fwrite(Header, strlen(Header), 1, linear_params->sfile);

    for (i = LDictObj;i <= linear_params->LastResource + 2; i++) {
        for (j = 0; j < pdev->ResourceUsageSize;j++) {
            if (pdev->ResourceUsage[j].NewObjectNumber == i) {
                gs_sprintf(Header, "%010"PRId64" 00000 n \n", pdev->ResourceUsage[j].LinearisedOffset);
                gp_fwrite(Header, 20, 1, linear_params->sfile);
            }
        }
    }

    /* Return to the secondary trailer dict and write it again filling
     * in the missing values.
     */
    code = gp_fseek(linear_params->sfile, linear_params->FirsttrailerOffset, SEEK_SET);
    if (code != 0)
        return_error(gs_error_ioerror);

    gs_sprintf(LDict, "\ntrailer\n<</Size %ld/Info %d 0 R/Root %d 0 R/ID[%s%s]/Prev %"PRId64">>\nstartxref\r\n0\n%%%%EOF\n",
        linear_params->LastResource + 3, pdev->ResourceUsage[linear_params->Info_id].NewObjectNumber, pdev->ResourceUsage[linear_params->Catalog_id].NewObjectNumber, fileID, fileID, mainxref);
    gp_fwrite(LDict, strlen(LDict), 1, linear_params->sfile);

    code = gp_fseek(linear_params->sfile, pdev->ResourceUsage[HintStreamObj].LinearisedOffset, SEEK_SET);
    if (code != 0)
        return_error(gs_error_ioerror);

    gs_sprintf(LDict, "%d 0 obj\n<</Length %10"PRId64"", HintStreamObj, HintLength);
    gp_fwrite(LDict, strlen(LDict), 1, linear_params->sfile);
    gs_sprintf(LDict, "\n/S %10"PRId64">>\nstream\n", SharedHintOffset);
    gp_fwrite(LDict, strlen(LDict), 1, linear_params->sfile);

error:
#ifdef LINEAR_DEBUGGING
    fclose(linear_params->Lin_File.file);
#else
    pdf_close_temp_file(pdev, &linear_params->Lin_File, code);
#endif
        /* FIXME free all the linearisation records */

    for (i=0;i<pdev->next_page;i++) {
        page_hint_stream_t *pagehint = &linear_params->PageHints[i];

        if (pagehint && pagehint->SharedObjectRef)
            gs_free_object(pdev->pdf_memory, pagehint->SharedObjectRef, "Free Shared object references");
    }

    gs_free_object(pdev->pdf_memory, linear_params->PageHints, "Free Page Hint data");
    gs_free_object(pdev->pdf_memory, linear_params->SharedHints, "Free Shared hint data");

    return code;
}

int pdf_record_usage(gx_device_pdf *const pdev, long resource_id, int page_num)
{
    int i;
    void *Temp;
    pdf_linearisation_record_t *resize;

    if (!pdev->Linearise)
        return 0;

    if (resource_id < 0)
        return 0;

    if (resource_id >= pdev->ResourceUsageSize) {
        if (pdev->ResourceUsageSize == 0) {
            pdev->ResourceUsageSize = resource_id + 1;
            pdev->ResourceUsage = gs_alloc_struct_array(pdev->pdf_memory->non_gc_memory, resource_id + 1, pdf_linearisation_record_t,
                              &st_pdf_linearisation_record_element, "start resource usage array");
            memset((char *)pdev->ResourceUsage, 0x00, (resource_id + 1) * sizeof(pdf_linearisation_record_t));
        } else {
            resize = gs_resize_object(pdev->pdf_memory->non_gc_memory, pdev->ResourceUsage, resource_id + 1, "resize resource usage array");
            memset(&resize[pdev->ResourceUsageSize], 0x00, sizeof(pdf_linearisation_record_t) * (resource_id - pdev->ResourceUsageSize + 1));
            pdev->ResourceUsageSize = resource_id + 1;
            pdev->ResourceUsage = resize;
        }
    }
    if (page_num > 0) {
        if (pdev->ResourceUsage[resource_id].PageUsage == 0)
            pdev->ResourceUsage[resource_id].PageUsage = page_num;
        else {
            if (pdev->ResourceUsage[resource_id].PageUsage > 1)
                pdev->ResourceUsage[resource_id].PageUsage = resource_usage_page_shared;
            else {
                /* Should not happen, raise an error */
            }
        }
    } else {
        if (pdev->ResourceUsage[resource_id].PageUsage != 0)
           /* Should not happen, raise an error */
            ;
        pdev->ResourceUsage[resource_id].PageUsage = page_num;
    }

    /* First check we haven't already recorded this Resource being used by this page */
    if (pdev->ResourceUsage[resource_id].NumPagesUsing != 0) {
        for (i=0;i < pdev->ResourceUsage[resource_id].NumPagesUsing; i++) {
            if (pdev->ResourceUsage[resource_id].PageList[i] == page_num)
                return 0;
        }
    }
    Temp = gs_alloc_bytes(pdev->pdf_memory->non_gc_memory, (pdev->ResourceUsage[resource_id].NumPagesUsing + 1) * sizeof (int), "Page usage records");
    memset((char *)Temp, 0x00, (pdev->ResourceUsage[resource_id].NumPagesUsing + 1) * sizeof (int));
    memcpy((char *)Temp, pdev->ResourceUsage[resource_id].PageList, pdev->ResourceUsage[resource_id].NumPagesUsing * sizeof (int));
    gs_free_object(pdev->pdf_memory->non_gc_memory, (byte *)pdev->ResourceUsage[resource_id].PageList, "Free old page usage records");
    pdev->ResourceUsage[resource_id].PageList = (int *)Temp;
    pdev->ResourceUsage[resource_id].PageList[pdev->ResourceUsage[resource_id].NumPagesUsing] = page_num;
    pdev->ResourceUsage[resource_id].NumPagesUsing++;

    return 0;
}

int pdf_record_usage_by_parent(gx_device_pdf *const pdev, long resource_id, long parent_id)
{
    int i;
    if (!pdev->Linearise)
        return 0;

    if (pdev->ResourceUsage[parent_id].PageUsage >= 0)
        pdf_record_usage(pdev, resource_id, pdev->ResourceUsage[parent_id].PageUsage);
    else {
        for (i = 0; i < pdev->ResourceUsage[parent_id].NumPagesUsing; i++) {
            pdf_record_usage(pdev, resource_id, pdev->ResourceUsage[parent_id].PageList[i]);
        }
    }
    return 0;
}

/* This routine is related to the PCL interpreter. Because of the way that
 * PCL pass through works, the PCL interpreter can shut down and free its font cache
 * while still running. This leaves us with copies of fonts, which point to a now
 * freed font cache. Large parts of the code which retrieve font information require
 * that the font cache be present, and fail badly if it isn't. So we construct a
 * font cache of our own, and when we copy fonts from the interpreter we point the
 * copied font at this font cache, instead of the one the original font used.
 * This allows the PCL interpreter to shut down and free its cache, thus eliminating
 * a memory leak, while still allowing pdfwrite to retrieve the information it needs
 * from the copied fonts.
 * Here we need to shut down and free our font cache.
 */
static void pdf_free_pdf_font_cache(gx_device_pdf *pdev)
{
    if (pdev->pdf_font_dir == NULL)
        return;
    gs_free_object(pdev->pdf_font_dir->memory, pdev->pdf_font_dir, "pdf_free_pdf_font_cache");
    pdev->pdf_font_dir = NULL;
}

static int discard_dict_refs(void *client_data, const byte *key_data, uint key_size, const cos_value_t *v);

static int discard_array_refs(gx_device_pdf *pdev, cos_object_t *pco)
{
    int i;
    long index;
    cos_array_t *pca = (cos_array_t *)pco;
    const cos_array_element_t *element = cos_array_element_first(pca);
    cos_value_t *v;

    while (element) {
        element = cos_array_element_next(element, &index, (const cos_value_t **)&v);
        if (v->value_type == COS_VALUE_OBJECT) {
            for (i=0;i<NUM_RESOURCE_TYPES;i++) {
                if (i == resourceOther)
                    continue;
                if (pdf_find_resource_by_resource_id(pdev, i, v->contents.object->id)){
                    v->value_type = COS_VALUE_CONST;
                    break;
                }
                if (cos_type(v->contents.object) == cos_type_array) {
                    discard_array_refs(pdev, v->contents.object);
                }
                if (cos_type(v->contents.object) == cos_type_dict) {
                    cos_dict_forall((const cos_dict_t *)v->contents.object, pdev, discard_dict_refs);
                }
            }
        }
    }
    return 0;
}

static int discard_dict_refs(void *client_data, const byte *key_data, uint key_size, const cos_value_t *v)
{
    int i;
    gx_device_pdf *pdev = (gx_device_pdf *)client_data;
    cos_value_t *v1 = (cos_value_t *)v;

    if (v->value_type == COS_VALUE_OBJECT) {
        for (i=0;i<NUM_RESOURCE_TYPES;i++) {
            if (i == resourceOther)
                continue;
            if (pdf_find_resource_by_resource_id(pdev, i, v->contents.object->id)){
                v1->value_type = COS_VALUE_CONST;
                break;
            }
            if (cos_type(v->contents.object) == cos_type_array) {
                discard_array_refs(pdev, v->contents.object);
            }
            if (cos_type(v->contents.object) == cos_type_dict) {
                cos_dict_forall((const cos_dict_t *)v->contents.object, pdev, discard_dict_refs);
            }
        }
    }
    return 0;
}

/* Close the device. */
static int
pdf_close(gx_device * dev)
{
    gx_device_pdf *const pdev = (gx_device_pdf *) dev;
    gs_memory_t *mem = pdev->pdf_memory;
    stream *s;
    gp_file *tfile = pdev->xref.file;
    gs_offset_t xref = 0;
    gs_offset_t resource_pos;
    long Catalog_id = 0, Info_id = 0,
        Pages_id = 0, Encrypt_id = 0;
    long Threads_id = 0;
    bool partial_page = (pdev->contents_id != 0 && pdev->next_page != 0);
    int code = 0, code1, pagecount=0;
    int64_t start_section, end_section;
    char str[256];
    pdf_linearisation_t linear_params;

    if (!dev->is_open)
      return_error(gs_error_undefined);
    dev->is_open = false;

    Catalog_id = pdev->Catalog->id;
    Info_id = pdev->Info->id;
    Pages_id = pdev->Pages->id;

    memset(&linear_params, 0x00, sizeof(linear_params));
    linear_params.Info_id = Info_id;
    linear_params.Pages_id = Pages_id;
    linear_params.Catalog_id = Catalog_id;

    /*
     * If this is an EPS file, or if the file didn't end with a showpage for
     * some other reason, or if the file has produced no marks at all, we
     * need to tidy up a little so as not to produce illegal PDF.  However,
     * if there is at least one complete page, we discard any leftover
     * marks.
     */
    if (pdev->next_page == 0) {
        /* If we didn't get called from pdf_output_page, and we are doign file-per-page
         * output, then the call from close_device will leave an empty file which we don't
         * want. So here we delete teh file.
         */
        if (!pdev->InOutputPage && gx_outputfile_is_separate_pages(pdev->fname, pdev->memory)) {
            code = gdev_vector_close_file((gx_device_vector *) pdev);
            if (code != 0)
                return code;
            code = pdf_close_files(pdev, 0);
            if (code < 0)
                return code;
            code = gx_device_delete_output_file((const gx_device *)pdev, pdev->fname);
            if (code != 0)
                return gs_note_error(gs_error_ioerror);
            return code;
        }
        code = pdf_open_page(pdev, PDF_IN_STREAM);

        if (code < 0)
            return code;
    }
    if (pdev->contents_id != 0)
        pdf_close_page(pdev, 1);

    /* Write the page objects. */
    if (!(pdev->ForOPDFRead && pdev->ProduceDSC)) {
        for (pagecount = 1; pagecount <= pdev->next_page; ++pagecount)
            pdf_write_page(pdev, pagecount);
    }

    if (pdev->PrintStatistics)
        pdf_print_resource_statistics(pdev);

    /* Write the font resources and related resources. */
    code1 = pdf_write_resource_objects(pdev, resourceXObject);
    if (code >= 0)
        code = code1;
    code1 = pdf_free_resource_objects(pdev, resourceXObject);
    if (code >= 0)
        code = code1;
    code1 = pdf_free_resource_objects(pdev, resourceProperties);
    if (code >= 0)
        code = code1;
    code1 = pdf_write_resource_objects(pdev, resourceGroup);
    if (code >= 0)
        code = code1;
    code1 = pdf_free_resource_objects(pdev, resourceGroup);
    if (code >= 0)
        code = code1;
    code1 = pdf_write_resource_objects(pdev, resourceSoftMaskDict);
    if (code >= 0)
        code = code1;
    code1 = pdf_free_resource_objects(pdev, resourceSoftMaskDict);
    if (code >= 0)
        code = code1;
    /* This was in pdf_close_document, but that made no sense, so moved here
     * for more consistency (and ease of finding it). This code deals with
     * emitting fonts and FontDescriptors
     */
    pdf_clean_standard_fonts(pdev);
    code1 = pdf_free_font_cache(pdev);
    if (code >= 0)
        code = code1;
    code1 = pdf_write_resource_objects(pdev, resourceCharProc);
    if (code >= 0)
        code = code1;
    code1 = pdf_finish_resources(pdev, resourceFont, pdf_convert_truetype_font);
    if (code >= 0)
        code = code1;
    code1 = pdf_finish_resources(pdev, resourceFontDescriptor, pdf_finish_FontDescriptor);
    if (code >= 0)
        code = code1;
    code1 = write_font_resources(pdev, &pdev->resources[resourceCIDFont]);
    if (code >= 0)
        code = code1;
    code1 = write_font_resources(pdev, &pdev->resources[resourceFont]);
    if (code >= 0)
        code = code1;
    code1 = pdf_finish_resources(pdev, resourceFontDescriptor, pdf_write_FontDescriptor);
    if (code >= 0)
        code = code1;
    /* If required, write the Encoding for Type 3 bitmap fonts. */
    code1 = pdf_write_bitmap_fonts_Encoding(pdev);
    if (code >= 0)
        code = code1;

    code1 = pdf_write_resource_objects(pdev, resourceCMap);
    if (code >= 0)
        code = code1;
    code1 = pdf_free_resource_objects(pdev, resourceCMap);
    if (code >= 0)
        code = code1;
    if (!(pdev->ForOPDFRead && pdev->ProduceDSC)) {
        if (pdev->ResourcesBeforeUsage)
            pdf_reverse_resource_chain(pdev, resourcePage);
        code1 = pdf_write_resource_objects(pdev, resourcePage);
        if (code >= 0)
            code = code1;
        code1 = pdf_free_resource_objects(pdev, resourcePage);
        if (code >= 0)
            code = code1;
    }

    /* Create the Pages tree. */
    if (!(pdev->ForOPDFRead && pdev->ProduceDSC)) {
        pdf_open_obj(pdev, Pages_id, resourcePagesTree);
        pdf_record_usage(pdev, Pages_id, resource_usage_part9_structure);

        s = pdev->strm;
        stream_puts(s, "<< /Type /Pages /Kids [\n");
        /* Omit the last page if it was incomplete. */
        if (partial_page)
            --(pdev->next_page);
        {
            int i;

            for (i = 0; i < pdev->next_page; ++i)
                pprintld1(s, "%ld 0 R\n", pdev->pages[i].Page->id);
        }
        pprintd1(s, "] /Count %d\n", pdev->next_page);

        /* If the last file was PostScript, its possible that DSC comments might be lying around
         * and pdf_print_orientation will use that if its present. So make sure we get rid of those
         * before considering the dominant page direction for the Pages tree.
         */
        pdev->doc_dsc_info.viewing_orientation = pdev->doc_dsc_info.orientation = -1;
        pdev->text_rotation.Rotate = pdf_dominant_rotation(&pdev->text_rotation);
        pdf_print_orientation(pdev, NULL);

        cos_dict_elements_write(pdev->Pages, pdev);
        stream_puts(s, ">>\n");
        pdf_end_obj(pdev, resourcePagesTree);

        /* Close outlines and articles. */

        if (pdev->outlines_id != 0) {
            pdf_record_usage(pdev, pdev->outlines_id, resource_usage_part9_structure);
            /* depth > 0 is only possible for an incomplete outline tree. */
            while (pdev->outline_depth > 0) {
                code1 = pdfmark_close_outline(pdev);
                if (code >= 0)
                    code = code1;
            }
            code = pdfmark_close_outline(pdev);
            if (code >= 0)
                code = code1;
            pdf_open_obj(pdev, pdev->outlines_id, resourceOutline);
            pprintd1(s, "<< /Count %d", pdev->outlines_open);
            pprintld2(s, " /First %ld 0 R /Last %ld 0 R >>\n",
                  pdev->outline_levels[0].first.id,
                  pdev->outline_levels[0].last.id);
            pdf_end_obj(pdev, resourceOutline);
        }
        if (pdev->articles != 0) {
            pdf_article_t *part;

            /* Write the remaining information for each article. */
            for (part = pdev->articles; part != 0; part = part->next)
                pdfmark_write_article(pdev, part);
        }

        /* Write named destinations.  (We can't free them yet.) */

        if (pdev->CompatibilityLevel < 1.2) {
            if (pdev->Dests) {
                pdf_record_usage(pdev, pdev->Dests->id, resource_usage_part9_structure);
                COS_WRITE_OBJECT(pdev->Dests, pdev, resourceDests);
            }
        } else {
            if (pdev->Dests) {
                pdf_record_usage(pdev, pdev->Dests->id, resource_usage_part9_structure);
                cos_write_dict_as_ordered_array((cos_object_t *)pdev->Dests, pdev, resourceDests);
            }
            /* Write Embedded files.  (We can't free them yet.) */

            if (pdev->EmbeddedFiles) {
                pdf_record_usage(pdev, pdev->EmbeddedFiles->id, resource_usage_part9_structure);
                cos_write_dict_as_ordered_array((cos_object_t *)pdev->EmbeddedFiles, pdev, resourceEmbeddedFiles);
            }
        }

        /* Write the PageLabel array */
        pdfmark_end_pagelabels(pdev);
        if (pdev->PageLabels) {
            pdf_record_usage(pdev, pdev->PageLabels->id, resource_usage_part9_structure);
            COS_WRITE_OBJECT(pdev->PageLabels, pdev, resourceLabels);
        }

        /* Write the document metadata. */
        code1 = pdf_document_metadata(pdev);
        if (code >= 0)
            code = code1;

        /* Write the Catalog. */

        /*
         * The PDF specification requires Threads to be an indirect object.
         * Write the threads now, if any.
         */
        if (pdev->articles != 0) {
            pdf_article_t *part;

            Threads_id = pdf_begin_obj(pdev, resourceThread);
            pdf_record_usage(pdev, Threads_id, resource_usage_part9_structure);
            s = pdev->strm;
            stream_puts(s, "[ ");
            while ((part = pdev->articles) != 0) {
                pdev->articles = part->next;
                pprintld1(s, "%ld 0 R\n", part->contents->id);
                COS_FREE(part->contents, "pdf_close(article contents)");
                gs_free_object(mem, part, "pdf_close(article)");
            }
            stream_puts(s, "]\n");
            pdf_end_obj(pdev, resourceThread);
        }
        pdf_open_obj(pdev, Catalog_id, resourceCatalog);
        pdf_record_usage(pdev, Catalog_id, resource_usage_part1_structure);

        s = pdev->strm;
        stream_puts(s, "<<");
        pprintld1(s, "/Type /Catalog /Pages %ld 0 R\n", Pages_id);
        if (pdev->outlines_id != 0)
            pprintld1(s, "/Outlines %ld 0 R\n", pdev->outlines_id);
        if (Threads_id) {
            pprintld1(s, "/Threads %ld 0 R\n", Threads_id);
            pdf_record_usage(pdev, Threads_id, resource_usage_part1_structure);
        }
        if (pdev->CompatibilityLevel < 1.2) {
            if (pdev->Dests)
                pprintld1(s, "/Dests %ld 0 R\n", pdev->Dests->id);
        } else {
            if (pdev->Dests || pdev->EmbeddedFiles) {
                stream_puts(s, "/Names <<\n");
                if (pdev->Dests)
                    pprintld1(s, "/Dests <</Kids [%ld 0 R]>>\n", pdev->Dests->id);
                if (pdev->EmbeddedFiles)
                    pprintld1(s, "/EmbeddedFiles << /Kids [%ld 0 R]>>\n", pdev->EmbeddedFiles->id);
                stream_puts(s, ">>\n");
            }
        }
        if (pdev->PageLabels)
            pprintld1(s, "/PageLabels << /Nums  %ld 0 R >>\n",
                  pdev->PageLabels->id);
        cos_dict_elements_write(pdev->Catalog, pdev);
        stream_puts(s, ">>\n");
        pdf_end_obj(pdev, resourceCatalog);
        if (pdev->Dests) {
            COS_FREE(pdev->Dests, "pdf_close(Dests)");
            pdev->Dests = 0;
        }
        if (pdev->EmbeddedFiles) {
            COS_FREE(pdev->EmbeddedFiles, "pdf_close(EmbeddFiles)");
            pdev->EmbeddedFiles = 0;
        }
        if (pdev->PageLabels) {
            COS_FREE(pdev->PageLabels, "pdf_close(PageLabels)");
            pdev->PageLabels = 0;
            pdev->PageLabels_current_label = 0;
        }

        /* Prevent writing special named objects twice. */

        pdev->Catalog->id = 0;
        /*pdev->Info->id = 0;*/	/* Info should get written */
        pdf_record_usage(pdev, pdev->Info->id, resource_usage_part9_structure);

    } else {
        pdev->Info->id = 0;	/* Don't write Info dict for DSC PostScript */
    }
    /*
     * Write the definitions of the named objects.
     * Note that this includes Form XObjects created by BP/EP, named PS
     * XObjects, and images named by NI.
     */

    do {
        cos_dict_objects_write(pdev->local_named_objects, pdev);
    } while (pdf_pop_namespace(pdev) >= 0);
    cos_dict_objects_write(pdev->global_named_objects, pdev);

    if (pdev->ForOPDFRead && pdev->ProduceDSC) {
        int pages;

        for (pages = 0; pages <= pdev->next_page; ++pages)
            ;

        code1 = ps2write_dsc_header(pdev, pages - 1);
        if (code >= 0)
            code = code1;
    }

    /* Copy the resources into the main file. */

    s = pdev->strm;
    resource_pos = stell(s);
    sflush(pdev->asides.strm);
    {
        gp_file *rfile = pdev->asides.file;
        int64_t res_end = gp_ftell(rfile);

        gp_fseek(rfile, 0L, SEEK_SET);
        code1 = pdf_copy_data(s, rfile, res_end, NULL);
        if (code >= 0)
            code = code1;
    }

    if (pdev->ForOPDFRead && pdev->ProduceDSC) {
        int j;

        pagecount = 1;

        /* All resources and procsets written, end the prolog */
        stream_puts(pdev->strm, "%%EndProlog\n");

        if (pdev->params.PSDocOptions.data) {
            int i;
            char *p = (char *)pdev->params.PSDocOptions.data;

            stream_puts(pdev->strm, "%%BeginSetup\n");
            for (i=0;i<pdev->params.PSDocOptions.size;i++)
                stream_putc(s, *p++);
            stream_puts(pdev->strm, "\n");
            stream_puts(pdev->strm, "\n%%EndSetup\n");
        }

        if (pdev->ResourcesBeforeUsage)
            pdf_reverse_resource_chain(pdev, resourcePage);
        for (j = 0; j < NUM_RESOURCE_CHAINS && code >= 0; ++j) {
            pdf_resource_t *pres = pdev->resources[resourcePage].chains[j];

            for (; pres != 0; pres = pres->next)
                if ((!pres->named || pdev->ForOPDFRead)
                    && !pres->object->written) {
                    pdf_page_t *page = &pdev->pages[pagecount - 1];

                    pprintd2(pdev->strm, "%%%%Page: %d %d\n",
                        pagecount, pagecount);
                    if (!pdev->Eps2Write)
                        pprintd2(pdev->strm, "%%%%PageBoundingBox: 0 0 %d %d\n", (int)page->MediaBox.x, (int)page->MediaBox.y);
                    stream_puts(pdev->strm, "%%BeginPageSetup\n");

                    if (pdev->params.PSPageOptions.size) {
                        int i, index = (pagecount - 1) % pdev->params.PSPageOptions.size;
                        char *p = (char *)pdev->params.PSPageOptions.data[index].data;

                        for (i=0;i<pdev->params.PSPageOptions.data[index].size;i++)
                            stream_putc(s, *p++);
                        stream_puts(pdev->strm, "\n");
                    }

                    pdf_write_page(pdev, pagecount++);

                    stream_puts(pdev->strm, "%%EndPageSetup\n");
                    pprintld1(pdev->strm, "%ld 0 obj\n", pres->object->id);
                    code = cos_write(pres->object, pdev, pres->object->id);
                    stream_puts(pdev->strm, "endobj\n");
                    pres->object->written = true;
                    stream_puts(pdev->strm, "%%PageTrailer\n");
                }
        }
        code1 = pdf_free_resource_objects(pdev, resourcePage);
        if (code >= 0)
            code = code1;
        stream_puts(pdev->strm, "%%Trailer\n");
        stream_puts(pdev->strm, "end\n");
        stream_puts(pdev->strm, "%%EOF\n");
    }

    if (pdev->Linearise) {
        linear_params.LastResource = pdev->next_id - 1;
        linear_params.Offsets = (gs_offset_t *)gs_alloc_bytes(pdev->pdf_memory, pdev->next_id * sizeof(gs_offset_t), "temp xref storage");
        memset(linear_params.Offsets, 0x00, linear_params.LastResource * sizeof(gs_offset_t));
    }

    if (!(pdev->ForOPDFRead && pdev->ProduceDSC)) {
        /* Write Encrypt. */
        if (pdev->OwnerPassword.size > 0) {
            Encrypt_id = pdf_obj_ref(pdev);

            pdf_open_obj(pdev, Encrypt_id, resourceEncrypt);
            s = pdev->strm;
            stream_puts(s, "<<");
            stream_puts(s, "/Filter /Standard ");
            pprintld1(s, "/V %ld ", pdev->EncryptionV);
            pprintld1(s, "/Length %ld ", pdev->KeyLength);
            pprintld1(s, "/R %ld ", pdev->EncryptionR);
            pprintld1(s, "/P %ld ", pdev->Permissions);
            stream_puts(s, "/O ");
            pdf_put_string(pdev, pdev->EncryptionO, sizeof(pdev->EncryptionO));
            stream_puts(s, "\n/U ");
            pdf_put_string(pdev, pdev->EncryptionU, sizeof(pdev->EncryptionU));
            stream_puts(s, ">>\n");
            pdf_end_obj(pdev, resourceEncrypt);
            s = pdev->strm;
        }

        /* Write the cross-reference section. */

        start_section = pdev->FirstObjectNumber;
        end_section = find_end_xref_section(pdev, tfile, start_section, resource_pos);

        xref = pdf_stell(pdev) - pdev->OPDFRead_procset_length;
        if (pdev->Linearise)
            linear_params.xref = xref;

        if (pdev->FirstObjectNumber == 1) {
            gs_sprintf(str, "xref\n0 %"PRId64"\n0000000000 65535 f \n",
                  end_section);
            stream_puts(s, str);
        }
        else {
            gs_sprintf(str, "xref\n0 1\n0000000000 65535 f \n%"PRId64" %"PRId64"\n",
                  start_section,
                  end_section - start_section);
            stream_puts(s, str);
        }

        do {
            code = write_xref_section(pdev, tfile, start_section, end_section, resource_pos, linear_params.Offsets);
            if (code < 0)
                return code;

            if (end_section >= pdev->next_id)
                break;
            start_section = end_section + 1;
            end_section = find_end_xref_section(pdev, tfile, start_section, resource_pos);
            if (end_section < 0)
                return end_section;
            gs_sprintf(str, "%"PRId64" %"PRId64"\n", start_section, end_section - start_section);
            stream_puts(s, str);
        } while (1);

        /* Write the trailer. */

        if (!pdev->Linearise) {
            char xref_str[32];
            stream_puts(s, "trailer\n");
            pprintld3(s, "<< /Size %ld /Root %ld 0 R /Info %ld 0 R\n",
                  pdev->next_id, Catalog_id, Info_id);
            stream_puts(s, "/ID [");
            psdf_write_string(pdev->strm, pdev->fileID, sizeof(pdev->fileID), 0);
            psdf_write_string(pdev->strm, pdev->fileID, sizeof(pdev->fileID), 0);
            stream_puts(s, "]\n");
            if (pdev->OwnerPassword.size > 0) {
                pprintld1(s, "/Encrypt %ld 0 R ", Encrypt_id);
            }
            stream_puts(s, ">>\n");
            gs_sprintf(xref_str, "startxref\n%"PRId64"\n%%%%EOF\n", xref);
            stream_puts(s, xref_str);
        }
    }

    if (pdev->Linearise) {
        int i;

        code = pdf_linearise(pdev, &linear_params);
        for (i=0; i<pdev->ResourceUsageSize; i++) {
            if (pdev->ResourceUsage[i].PageList)
                gs_free_object(pdev->pdf_memory->non_gc_memory, pdev->ResourceUsage[i].PageList, "Free linearisation Page Usage list records");
        }
        gs_free_object(pdev->pdf_memory->non_gc_memory, pdev->ResourceUsage, "Free linearisation resource usage records");
    }

    /* Require special handling for Fonts, ColorSpace and Pattern resources
     * These are tracked in pdev->last_resource, and are complex structures which may
     * contain other memory allocations. All other resource types can be simply dicarded
     * as above.
     */

    /* Memory management of resources in pdfwrite is bizarre and complex. Originally there was no means
     * to free any resorucesw on completionj, pdfwrite simply relied on the garbage collector to clean up
     * and all the resource objects are GC-visible. However, this doesn't work well when the interpreter
     * does not use GC, ie PCL or XPS, and even when GC is available, the time taken to clean up the
     * (sometimes enormous numbers) of objects can be surprisingly significant. So code was added above
     * to handle the simple cases useing pdf_free_resource_object(), and below to handle the more complex
     * situations.
     * The way this works is that for each resrouce type we free the 'object', if the object is itself
     * a 'cos' object (array, dictionary) then we free each of its members. However, if any of the objects
     * have an ID which is not zero, then we don't free them (this is true only for contents, all the
     * objects of a given type are freed regardless of whether their ID is 0). These are taken to be
     * references to resource objects of a type which has not yet been freed, and will be freed when
     * that resource type is freed. For the simple resources, which is most of them, this works well.
     *
     * However, there are complications; colour spaces and functions can contain cos objects
     * whose members pointers to other objects of the same resoruce type (eg a type 3 stitching function
     * can point to an array of type 0 functions). We can't afford to have these free the object, because
     * the resource chain is still pointing at it, and will try to free it again. The same is also true if
     * we should encounter the object which is referenced before we find the reference. So for these cases
     * we first scan the members of each resource (if present) to see if they are references to another
     * resource of the same type. If they are we simply 'corrupt' the reference by changing the object type.
     * The resource has already been written, and the reference will free the memory, so this is safe to do.
     *
     * The final case is 'resourceOther' which can contain pretty much anything. In particular it can contain
     * Colorant dictionaries for colour spaces, but technically it could contain cos objects pointing at any
     * resource type. So far I have only found this to be possible with colorspace and function resources, but
     * the routines 'discard_array_refs' and 'discard_dict_refs' actually check for *all* the resource types except
     * resourceOther. We can't free the resourceOther types because they cvan contain resources that are being used
     * by resources, eg the spot colour names for /Separation and DeviceN spaces.
     *
     * There is a 'gotcha' here; previously we used to free the 'resourceOther' resources *before* calling
     * pdf_document_metadata(), now we call it after. The problem is that the metadata is stored as a resourceOther
     * resource *and* referenced from the Catalog dictionary, which is a 'global named resource'. We free global
     * named resoruces as the absolute last action. Previously because we had free resourceOther resoruces before
     * creating the new reference to the Metadata, the fact that it was freed by the action of releasing the
     * global named resources wasn't a problem, now it is. If we free the metadata as a 'resourceOther' then when
     * we try to free it as a global named resource we will run into trouble again. So pdf_document_metadata() has been
     * specifally altered to remove the reference to the metadata from the resourceOther resource chain immediately
     * after it has been created. Ick.....
     */
    {
        int j;

        for (j = 0; j < NUM_RESOURCE_CHAINS; ++j) {
            pdf_resource_t *pres = pdev->resources[resourceOther].chains[j];

            for (; pres != 0;) {
                if (pres->object) {
                    if (cos_type(pres->object) == cos_type_array) {
                        discard_array_refs(pdev, pres->object);
                    }
                    if (cos_type(pres->object) == cos_type_dict) {
                        cos_dict_forall((const cos_dict_t *)pres->object, pdev, discard_dict_refs);
                    }
                }
                pres = pres->next;
            }
        }
    }

    {
        /* PDF font records and all their associated contents need to be
         * freed by means other than the garbage collector, or the memory
         * usage will increase with languages other than PostScript. In addition
         * debug builds of non-PS languages take a long time to close down
         * due to reporting the dangling memory allocations.
         */
        int j;

        for (j = 0; j < NUM_RESOURCE_CHAINS; ++j) {
            pdf_resource_t *pres = pdev->resources[resourceFont].chains[j];

            for (; pres != 0;) {
                pdf_font_resource_t *pdfont = (pdf_font_resource_t *)pres;

                font_resource_free(pdev, pdfont);
                pres = pres->next;
            }
        }
    }

    {
        int j;

        for (j = 0; j < NUM_RESOURCE_CHAINS; ++j) {
            pdf_resource_t *pres = pdev->resources[resourceCIDFont].chains[j];

            for (; pres != 0;) {
                pdf_font_resource_t *pdfont = (pdf_font_resource_t *)pres;

                font_resource_free(pdev, pdfont);
                pres = pres->next;
            }
        }
    }

    {
        int j;

        for (j = 0; j < NUM_RESOURCE_CHAINS; ++j) {
            pdf_resource_t *pres = pdev->resources[resourceFontDescriptor].chains[j];
            for (; pres != 0;) {
                pdf_font_descriptor_free(pdev, pres);
                pres = pres->next;
            }
        }
    }

    {
        int j;

        for (j = 0; j < NUM_RESOURCE_CHAINS; ++j) {
            pdf_resource_t *pres = pdev->resources[resourceCharProc].chains[j];

            for (; pres != 0;) {
                if (pres->object) {
                    gs_free_object(pdev->pdf_memory, (byte *)pres->object, "Free CharProc");
                    pres->object = 0;
                }
                pres = pres->next;
            }
        }
    }

    {
        int j;

        for (j = 0; j < NUM_RESOURCE_CHAINS; ++j) {
            pdf_resource_t *pres = pdev->resources[resourceExtGState].chains[j];

            for (; pres != 0;) {
                if (pres->object) {
                    cos_release(pres->object, "release ExtGState object");
                    gs_free_object(pdev->pdf_memory, pres->object, "free ExtGState");
                    pres->object = 0;
                }
                pres = pres->next;
            }
        }
    }

    {
        int j;

        for (j = 0; j < NUM_RESOURCE_CHAINS; ++j) {
            pdf_resource_t *pres = pdev->resources[resourcePattern].chains[j];

            for (; pres != 0;) {
                if (pres->object) {
                    cos_release(pres->object, "free pattern dict");
                    gs_free_object(pdev->pdf_memory, pres->object, "free pattern resources");
                    pres->object = 0;
                }
                pres = pres->next;
            }
        }
    }

    {
        int j;

        for (j = 0; j < NUM_RESOURCE_CHAINS; ++j) {
            pdf_resource_t *pres = pdev->resources[resourceShading].chains[j];

            for (; pres != 0;) {
                if (pres->object) {
                    cos_release(pres->object, "free Shading dict");
                    gs_free_object(pdev->pdf_memory, pres->object, "free Shading resources");
                    pres->object = 0;
                }
                pres = pres->next;
            }
        }
    }

    {
        int j;

        /* prescan the colour space arrays to see if any of them reference other colour
         * spaces. If they do, then remove the reference by changing it from a COS_VALUE_OBJECT
         * into a COS_VALUE_CONST, see comment at the start of this section as to why this is safe.
         */
        for (j = 0; j < NUM_RESOURCE_CHAINS; ++j) {
            pdf_resource_t *pres = pdev->resources[resourceColorSpace].chains[j];
            for (; pres != 0;) {
                if (cos_type(pres->object) == cos_type_array) {
                    long index;
                    cos_array_t *pca = (cos_array_t *)pres->object;
                    const cos_array_element_t *element = cos_array_element_first(pca);
                    cos_value_t *v;

                    while (element) {
                        element = cos_array_element_next(element, &index, (const cos_value_t **)&v);
                        if (v->value_type == COS_VALUE_OBJECT) {
                            if (pdf_find_resource_by_resource_id(pdev, resourceColorSpace, v->contents.object->id)){
                                v->value_type = COS_VALUE_CONST;
                            }
                        }
                    }
                }
                pres = pres->next;
            }
        }
        for (j = 0; j < NUM_RESOURCE_CHAINS; ++j) {
            pdf_resource_t *pres = pdev->resources[resourceColorSpace].chains[j];
            for (; pres != 0;) {
                free_color_space(pdev, pres);
                pres = pres->next;
            }
        }
    }

    {
        int j;

        for (j = 0; j < NUM_RESOURCE_CHAINS; ++j) {
            pdf_resource_t *pres = pdev->resources[resourceGroup].chains[j];

            for (; pres != 0;) {
                if (pres->object) {
                    cos_release(pres->object, "free Group dict");
                    gs_free_object(pdev->pdf_memory, pres->object, "free Group resources");
                    pres->object = 0;
                }
                pres = pres->next;
            }
        }
    }

    {
        int j;

        /* prescan the function arrays and dictionariesto see if any of them reference other
         * functions. If they do, then remove the reference by changing it from a COS_VALUE_OBJECT
         * into a COS_VALUE_CONST, see comment at the start of this section as to why this is safe.
         */
        for (j = 0; j < NUM_RESOURCE_CHAINS; ++j) {
            pdf_resource_t *pnext = 0, *pres = pdev->resources[resourceFunction].chains[j];

            for (; pres != 0;) {
                if (pres->object) {
                    free_function_refs(pdev, pres->object);
                }
                pnext = pres->next;
                pres = pnext;
            }
        }
        for (j = 0; j < NUM_RESOURCE_CHAINS; ++j) {
            pdf_resource_t *pnext = 0, *pres = pdev->resources[resourceFunction].chains[j];

            for (; pres != 0;) {
                if (pres->object) {
                    cos_release(pres->object, "free function dict");
                    gs_free_object(pdev->pdf_memory, pres->object, "free function resources");
                    pres->object = 0;
                }
                pnext = pres->next;
                pres = pnext;
            }
        }
    }

    code1 = pdf_free_resource_objects(pdev, resourceOther);
    if (code >= 0)
        code = code1;

    if (code >= 0) {
        int i, j;

        for (i = 0; i < NUM_RESOURCE_TYPES; i++) {
            for (j = 0; j < NUM_RESOURCE_CHAINS; ++j) {
                pdev->resources[i].chains[j] = 0;
            }
        }
    }

    /* Release the resource records. */
    /* So what exactly is stored in this list ? I believe the following types of resource:
     *
     * resourceCharProc
     * resourceFont
     * resourceCIDFont
     * resourceFontDescriptor
     * resourceColorSpace
     * resourceExtGState
     * resourceXObject
     * resourceSoftMaskDict
     * resourceGroup
     * resourcePattern
     * resourceShading
     *
     * resourceCMap resourcePage and resourceFunction don't appear to be tracked. I note
     * that resourcePage and resourceCMap are freed above, as is resourceXObject and resourceSoftmaskDict.
     * So presumably reourceFunction just leaks.
     *
     * It seems that all these types of resources are added when they are created
     * in addition there are 'pdf_forget_resource' and pdf_cancel_resource which
     * remove entries from this list.
     */
    {
        pdf_resource_t *pres;
        pdf_resource_t *prev;

        for (prev = pdev->last_resource; (pres = prev) != 0;) {
            prev = pres->prev;
            gs_free_object(mem, pres, "pdf_resource_t");
        }
        pdev->last_resource = 0;
    }

    /* Free named objects. */

    cos_release((cos_object_t *)pdev->NI_stack, "Release Name Index stack");
    gs_free_object(mem, pdev->NI_stack, "Free Name Index stack");
    pdev->NI_stack = 0;

    cos_dict_objects_delete(pdev->local_named_objects);
    COS_FREE(pdev->local_named_objects, "pdf_close(local_named_objects)");
    pdev->local_named_objects = 0;

    /* global resources include the Catalog object and apparently the Info dict */
    cos_dict_objects_delete(pdev->global_named_objects);
    COS_FREE(pdev->global_named_objects, "pdf_close(global_named_objects)");
    pdev->global_named_objects = 0;

    /* Wrap up. */

    pdev->font_cache = 0;

    {
        int i;
        for (i=0;i < pdev->next_page;i++) {
            cos_release((cos_object_t *)pdev->pages[i].Page, "Free page dict");
            if (pdev->pages[i].Annots) {
                cos_release((cos_object_t *)pdev->pages[i].Annots, "Release Annots dict");
                gs_free_object(mem, pdev->pages[i].Annots, "Free Annots dict");
            }
            gs_free_object(mem, pdev->pages[i].Page, "Free Page object");
        }
    }
    gs_free_object(mem, pdev->pages, "pages");
    pdev->pages = 0;

    pdev->num_pages = 0;

    gs_free_object(mem, pdev->sbstack, "Free sbstack");
    pdev->sbstack = 0;

    text_data_free(mem, pdev->text);
    pdev->text = 0;

    cos_release((cos_object_t *)pdev->Pages, "release Pages dict");
    gs_free_object(mem, pdev->Pages, "Free Pages dict");
    pdev->Pages = 0;

    {
        int i;
        for (i=0;i < pdev->vgstack_depth;i++)
            gs_free_object(pdev->memory->non_gc_memory, pdev->vgstack[i].dash_pattern, "pdfwrite final free stored dash in gstate");
    }
    gs_free_object(pdev->pdf_memory, pdev->vgstack, "pdf_close(graphics state stack)");
    pdev->vgstack = 0;

    cos_release((cos_object_t *)pdev->Namespace_stack, "release Name space stack");
    gs_free_object(mem, pdev->Namespace_stack, "Free Name space stack");
    pdev->Namespace_stack = 0;

    pdev->Catalog = 0;
    pdev->Info = 0;

    gs_free_object(mem, pdev->outline_levels, "outline_levels array");
    pdev->outline_levels = 0;
    pdev->outline_depth = -1;
    pdev->max_outline_depth = 0;

    {
        /* pdf_open_dcument could set up filters for entire document.
           Removing them now. */
        int status;

        if (!pdev->ProduceDSC)
            stream_putc(s, 0x04);
        while (s->strm) {
            s = s->strm;
        }
        status = s_close_filters(&pdev->strm, s);
        if (status < 0 && code == 0)
            code = gs_error_ioerror;
    }

    code1 = gdev_vector_close_file((gx_device_vector *) pdev);
    if (code >= 0)
        code = code1;
    if (pdev->max_referred_page >= pdev->next_page + 1) {
        /* Note : pdev->max_referred_page counts from 1,
           and pdev->next_page counts from 0. */
        emprintf2(pdev->memory, "ERROR: A pdfmark destination page %d "
                  "points beyond the last page %d.\n",
                  pdev->max_referred_page, pdev->next_page);
    }
    code = pdf_close_files(pdev, code);
    if (code < 0)
        return code;

    pdf_free_pdf_font_cache(pdev);
    return code;
}
