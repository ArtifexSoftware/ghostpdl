/* Copyright (C) 2001-2012 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134, San Rafael,
   CA  94903, U.S.A., +1(415)492-9861, for further information.
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

/* Define the default language level and PDF compatibility level. */
/* Acrobat 4 (PDF 1.3) is the default. */
#define PSDF_VERSION_INITIAL psdf_version_ll3
#define PDF_COMPATIBILITY_LEVEL_INITIAL 1.4

/* Define the size of internal stream buffers. */
/* (This is not a limitation, it only affects performance.) */
#define sbuf_size 512

/* GC descriptors */
private_st_pdf_page();
gs_private_st_element(st_pdf_page_element, pdf_page_t, "pdf_page_t[]",
                      pdf_page_elt_enum_ptrs, pdf_page_elt_reloc_ptrs,
                      st_pdf_page);
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
    if (index <= pdev->outline_depth)
        ENUM_RETURN(pdev->outline_levels[index].first.action);
    index -= pdev->outline_depth + 1;
    if (index <= pdev->outline_depth)
        ENUM_RETURN(pdev->outline_levels[index].last.action);
    index -= pdev->outline_depth + 1;
    ENUM_PREFIX(st_device_psdf, 0);
}
#define e1(i,elt) ENUM_PTR(i, gx_device_pdf, elt);
gx_device_pdf_do_ptrs(e1)
#undef e1
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
#define r1(i,elt) RELOC_PTR(gx_device_pdf,elt);
    gx_device_pdf_do_ptrs(r1)
#undef r1
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
        for (i = 0; i <= pdev->outline_depth; ++i) {
            RELOC_PTR(gx_device_pdf, outline_levels[i].first.action);
            RELOC_PTR(gx_device_pdf, outline_levels[i].last.action);
        }
    }
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

#include "gdevpdfb.h"

#undef PDF_DEVICE_NAME
#undef PDF_DEVICE_IDENT
#undef PDF_DEVICE_MaxInlineImageSize
#undef PDF_FOR_OPDFREAD

#define PDF_DEVICE_NAME "ps2write"
#define PDF_DEVICE_IDENT gs_ps2write_device
#define PDF_DEVICE_MaxInlineImageSize max_long
#define PDF_FOR_OPDFREAD 1

#include "gdevpdfb.h"

#undef PDF_DEVICE_NAME
#undef PDF_DEVICE_IDENT
#undef PDF_DEVICE_MaxInlineImageSize
#undef PDF_FOR_OPDFREAD
/* ---------------- Device open/close ---------------- */

/* Close and remove temporary files. */
static int
pdf_close_temp_file(gx_device_pdf *pdev, pdf_temp_file_t *ptf, int code)
{
    int err = 0;
    FILE *file = ptf->file;

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
        err = ferror(file) | fclose(file);
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
    ptf->file =	gp_open_scratch_file_64(pdev->memory,
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

        time(&t);
        tms = *gmtime(&t);
        tms.tm_isdst = -1;
        timeoffset = (int)difftime(t, mktime(&tms)); /* tz+dst in seconds */
        timesign = (timeoffset == 0 ? 'Z' : timeoffset < 0 ? '-' : '+');
        timeoffset = any_abs(timeoffset) / 60;
        tms = *localtime(&t);

        sprintf(buf, "(D:%04d%02d%02d%02d%02d%02d%c%02d\'%02d\')",
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
    gp_get_usertime(secs_ns);
    sputs(s, (byte *)secs_ns, sizeof(secs_ns), &ignore);
    sputs(s, (const byte *)pdev->fname, strlen(pdev->fname), &ignore);
    pdev->strm = s;
    code = cos_dict_elements_write(pdev->Info, pdev);
    pdev->strm = strm;
    pdev->KeyLength = KeyLength;
    if (code < 0)
        return code;
    sclose(s);
    gs_free_object(mem, s, "pdf_compute_fileID");
#ifdef DEPRECATED_906
    memcpy(pdev->fileID, "xxxxxxxxxxxxxxxx", sizeof(pdev->fileID)); /* Debug */
#endif
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
            set_dev_proc(pdev, decode_color, cmyk_8bit_map_color_rgb);
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
    pdf_reset_text_state(pdev->text);
}

/* Open the device. */
static int
pdf_open(gx_device * dev)
{
    gx_device_pdf *const pdev = (gx_device_pdf *) dev;
    gs_memory_t *mem = pdev->pdf_memory = gs_memory_stable(pdev->memory);
    int code;

    if ((code = pdf_open_temp_file(pdev, &pdev->xref)) < 0 ||
        (code = pdf_open_temp_stream(pdev, &pdev->asides)) < 0 ||
        (code = pdf_open_temp_stream(pdev, &pdev->streams)) < 0 ||
        (code = pdf_open_temp_stream(pdev, &pdev->pictures)) < 0
        )
        goto fail;
    code = gdev_vector_open_file((gx_device_vector *) pdev, sbuf_size);
    if (code < 0)
        goto fail;
    if (pdev->ComputeDocumentDigest) {
        stream *s = s_MD5C_make_stream(pdev->pdf_memory, pdev->strm);

        if (s == NULL)
            return_error(gs_error_VMerror);
        pdev->strm = s;
    }
    gdev_vector_init((gx_device_vector *) pdev);
    gp_get_realtime(pdev->uuid_time);
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
    pdev->sbstack_size = count_of(pdev->vgstack); /* Overestimated a few. */
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
    pdev->outline_levels[0].first.id = 0;
    pdev->outline_levels[0].left = max_int;
    pdev->outline_levels[0].first.action = 0;
    pdev->outline_levels[0].last.action = 0;
    pdev->outline_depth = 0;
    pdev->closed_outline_depth = 0;
    pdev->outlines_open = 0;
    pdev->articles = 0;
    pdev->Dests = 0;
    /* {global,local}_named_objects was initialized above */
    pdev->PageLabels = 0;
    pdev->PageLabels_current_page = 0;
    pdev->PageLabels_current_label = 0;
    pdev->pte = NULL;
    pdf_reset_page(pdev);
    return 0;
  fail:
    gdev_vector_close_file((gx_device_vector *) pdev);
    return pdf_close_files(pdev, code);
}

/* Detect I/O errors. */
static int
pdf_ferror(gx_device_pdf *pdev)
{
    fflush(pdev->file);
    fflush(pdev->xref.file);
    sflush(pdev->strm);
    sflush(pdev->asides.strm);
    sflush(pdev->streams.strm);
    sflush(pdev->pictures.strm);
    return ferror(pdev->file) || ferror(pdev->xref.file) ||
        ferror(pdev->asides.file) || ferror(pdev->streams.file) ||
        ferror(pdev->pictures.file);
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

#ifdef DEPRECATED_906
        /* Bug 687800 together with Bug687489.ps . */
        const gs_point *pbox = &(page != NULL ? page : &pdev->pages[0])->MediaBox;

        if (dsc_orientation >= 0 && pbox->x > pbox->y) {
            /* The page is in landscape format. Adjust the rotation accordingly. */
            dsc_orientation ^= 1;
        }
#endif

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
        /* If not combinable, prefer dsc rotation : */
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

        /* Save viewer's memory with cleaning resources. */

#ifdef DEPRECATED_906
        if (pdev->MaxViewerMemorySize < 10000000) {
            /* fixme: the condition above and the cleaning algorithm
                may be improved with counting stored resource size
                and creating multiple streams per page. */

            if (pdev->ForOPDFRead) {
                pdf_resource_t *pres = pdf_find_resource_by_resource_id(pdev, resourcePage, pdev->contents_id);

                if (pres != NULL) {
                    code = cos_dict_put_c_strings((cos_dict_t *)pres->object, "/.CleanResources", "/All");
                    if (code < 0)
                        return code;
                }
            }
            code = pdf_close_text_document(pdev);
            if (code < 0)
                return code;
            code = pdf_write_and_free_all_resource_objects(pdev);
            if (code < 0)
                return code;
        }
#endif

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
round_box_coord(floatp xy)
{
    return (int)(xy * 100 + 0.5) / 100.0;
}
static int
pdf_write_page(gx_device_pdf *pdev, int page_num)
{
    long page_id = pdf_page_id(pdev, page_num);
    pdf_page_t *page = &pdev->pages[page_num - 1];
    floatp mediabox[4] = {0, 0};
    stream *s;

    mediabox[2] = round_box_coord(page->MediaBox.x);
    mediabox[3] = round_box_coord(page->MediaBox.y);
    pdf_open_obj(pdev, page_id, resourcePage);
    s = pdev->strm;
    pprintg2(s, "<</Type/Page/MediaBox [0 0 %g %g]\n",
                mediabox[2], mediabox[3]);
    if (pdev->PDFX) {
        const cos_value_t *v_trimbox = cos_dict_find_c_key(page->Page, "/TrimBox");
        const cos_value_t *v_artbox = cos_dict_find_c_key(page->Page, "/ArtBox");
        const cos_value_t *v_cropbox = cos_dict_find_c_key(page->Page, "/CropBox");
        const cos_value_t *v_bleedbox = cos_dict_find_c_key(page->Page, "/BleedBox");
        floatp trimbox[4] = {0, 0}, bleedbox[4] = {0, 0};
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
                         return gs_error_unknownerror;
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
                             return gs_error_unknownerror;
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
    pprintld1(s, "/Parent %ld 0 R\n", pdev->Pages->id);
    if (pdev->ForOPDFRead && pdev->DoNumCopies && !pdev->ProduceDSC) {
        if (page->NumCopies_set)
            pprintld1(s, "/NumCopies %ld\n", page->NumCopies);
    }
    if (page->group_id > 0) {
        pprintld1(s, "/Group %ld 0 R\n", page->group_id);
    }
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
    int code = pdf_close_page(pdev, num_copies);

    if (code < 0)
        return code;
    if (pdf_ferror(pdev))
        gs_note_error(gs_error_ioerror);

    if ((code = gx_finish_output_page(dev, num_copies, flush)) < 0)
        return code;

    if (gx_outputfile_is_separate_pages(((gx_device_vector *)dev)->fname, dev->memory)) {
        if ((code = pdf_close(dev)) < 0)
            return code;
        code = pdf_open(dev);
    }
    return code;
}

static int find_end_xref_section (gx_device_pdf *pdev, FILE *tfile, int64_t start, int resource_pos)
{
    int64_t start_offset = (start - pdev->FirstObjectNumber) * sizeof(ulong);

    gp_fseek_64(tfile, start_offset, SEEK_SET);
    {
        long i, r;

        for (i = start; i < pdev->next_id; ++i) {
            ulong pos;

            r = fread(&pos, sizeof(pos), 1, tfile);
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

static int write_xref_section(gx_device_pdf *pdev, FILE *tfile, int64_t start, int end, int resource_pos, ulong *Offsets)
{
    int64_t start_offset = (start - pdev->FirstObjectNumber) * sizeof(ulong);

    gp_fseek_64(tfile, start_offset, SEEK_SET);
    {
        long i, r;

        for (i = start; i < end; ++i) {
            ulong pos;
            char str[21];

            r = fread(&pos, sizeof(pos), 1, tfile);
            if (r != 1)
                return(gs_note_error(gs_error_ioerror));
            if (pos & ASIDES_BASE_POSITION)
                pos += resource_pos - ASIDES_BASE_POSITION;
            pos -= pdev->OPDFRead_procset_length;
            sprintf(str, "%010ld 00000 n \n", pos);
            stream_puts(pdev->strm, str);
            if (Offsets)
                Offsets[i] = pos;
        }
    }
    return 0;
}

static int
rewrite_object(pdf_linearisation_t *linear_params, int object)
{
    ulong read, end, Size;
    char c, Scratch[16384];
    int i, code;

    return 0;

    end = linear_params->xref;
    for (i=0;i<linear_params->LastResource;i++) {
        if (linear_params->Offsets[i] > linear_params->Offsets[object] && linear_params->Offsets[i] < end)
            end = linear_params->Offsets[i];
    }
    Size = end - linear_params->Offsets[object];

    code = gp_fseek_64(linear_params->sfile, linear_params->Offsets[object], SEEK_SET);
    read = 0;
    do {
        code = fread(&c, 1, 1, linear_params->sfile);
        read++;
    } while (c != '\n');
    sprintf(Scratch, "%d 0 obj\n", linear_params->LastResource++);
    fwrite(Scratch, strlen(Scratch), 1, linear_params->Lin_File.file);
    Size -= read;
    do {
        if (Size > 16384) {
            code = fread(Scratch, 16384, 1, linear_params->sfile);
            fwrite(Scratch, 16384, 1, linear_params->Lin_File.file);
            Size -= 16384;
        } else {
            code = fread(Scratch, Size, 1, linear_params->sfile);
            fwrite(Scratch, Size, 1, linear_params->Lin_File.file);
            Size = 0;
        }
    } while (Size);
    return 0;
}

int pdf_record_usage(gx_device_pdf *const pdev, long resource_id, int page_num)
{
    if (!pdev->Linearise)
        return 0;

    if (resource_id >= pdev->ResourceUsageSize) {
        if (pdev->ResourceUsageSize == 0) {
            pdev->ResourceUsageSize = resource_id + 1;
            pdev->ResourceUsage = (int *)gs_alloc_bytes_immovable(pdev->pdf_memory, (resource_id + 1) * sizeof(int), "Storage of resource used by pages");
            memset((char *)pdev->ResourceUsage, 0x00, (resource_id + 1) * sizeof(int));
        } else {
            int *Temp = (int *)gs_alloc_bytes_immovable(pdev->pdf_memory, (resource_id + 1) * sizeof(int), "New Storage of resource used by pages");
            memset((char *)Temp, 0x00, (resource_id + 1) * sizeof(int));
            memcpy((char *)Temp, pdev->ResourceUsage, pdev->ResourceUsageSize * sizeof(int));
            pdev->ResourceUsageSize = resource_id + 1;
            gs_free_object(pdev->pdf_memory, (byte *)pdev->ResourceUsage, "Free Old Storage of resource usage");
            pdev->ResourceUsage = Temp;
        }
    }
    if (page_num > 0) {
        if (pdev->ResourceUsage[resource_id] == 0)
            pdev->ResourceUsage[resource_id] = page_num;
        else {
            if (pdev->ResourceUsage[resource_id] > 0)
                pdev->ResourceUsage[resource_id] = resource_usage_page_shared;
            else {
                /* Should not happen, raise an error */
            }
        }
    } else {
        if (pdev->ResourceUsage[resource_id] != 0)
           /* Should not happen, raise an error */
            ;
        pdev->ResourceUsage[resource_id] = page_num;
    }
    return 0;
}

int pdf_record_usage_by_parent(gx_device_pdf *const pdev, long resource_id, long parent_id)
{
    if (!pdev->Linearise)
        return 0;

    pdf_record_usage(pdev, resource_id, pdev->ResourceUsage[parent_id]);
    return 0;
}

/* Close the device. */
static int
pdf_close(gx_device * dev)
{
    gx_device_pdf *const pdev = (gx_device_pdf *) dev;
    gs_memory_t *mem = pdev->pdf_memory;
    stream *s;
    FILE *tfile = pdev->xref.file;
    long xref = 0;
    long resource_pos;
    long Catalog_id = pdev->Catalog->id, Info_id = pdev->Info->id,
        Pages_id = pdev->Pages->id, Encrypt_id = 0;
    long Threads_id = 0;
    bool partial_page = (pdev->contents_id != 0 && pdev->next_page != 0);
    int code = 0, code1, pagecount=0;
    int64_t start_section, end_section;

    pdf_linearisation_t linear_params;
    memset(&linear_params, 0x00, sizeof(linear_params));

    /*
     * If this is an EPS file, or if the file didn't end with a showpage for
     * some other reason, or if the file has produced no marks at all, we
     * need to tidy up a little so as not to produce illegal PDF.  However,
     * if there is at least one complete page, we discard any leftover
     * marks.
     */
    if (pdev->next_page == 0) {
        code = pdf_open_page(pdev, PDF_IN_STREAM);

        if (code < 0)
            return code;
    }
    if (pdev->contents_id != 0)
        pdf_close_page(pdev, 1);

    if (pdev->Linearise) {
        for (pagecount = 1; pagecount <= pdev->next_page; ++pagecount)
            ;
        if (pagecount == 1)
            /* No point in linearising a 1 page file */
            pdev->Linearise = 0;
    }

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
#ifdef DEPRECATED_906
    code1 = pdf_close_text_document(pdev);
    if (code >= 0)
        code = code1;
#endif
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

    code1 = pdf_free_resource_objects(pdev, resourceOther);
    if (code >= 0)
        code = code1;

    /* Create the Pages tree. */
    if (!(pdev->ForOPDFRead && pdev->ProduceDSC)) {
        pdf_open_obj(pdev, Pages_id, resourcePagesTree);
        pdf_record_usage(pdev, Pages_id, resource_usage_part1_structure);

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
            pdf_record_usage(pdev, pdev->outlines_id, resource_usage_part1_structure);
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

        if (pdev->Dests)
            COS_WRITE_OBJECT(pdev->Dests, pdev, resourceDests);

        /* Write the PageLabel array */
        pdfmark_end_pagelabels(pdev);
        if (pdev->PageLabels) {
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
        if (pdev->Dests)
            pprintld1(s, "/Dests %ld 0 R\n", pdev->Dests->id);
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
        if (pdev->PageLabels) {
            COS_FREE(pdev->PageLabels, "pdf_close(PageLabels)");
            pdev->PageLabels = 0;
            pdev->PageLabels_current_label = 0;
        }

        /* Prevent writing special named objects twice. */

        pdev->Catalog->id = 0;
        /*pdev->Info->id = 0;*/	/* Info should get written */
        pdf_record_usage(pdev, pdev->Info->id, resource_usage_part9_structure);
        pdev->Pages->id = 0;
        {
            int i;

            for (i = 0; i < pdev->num_pages; ++i)
                if (pdev->pages[i].Page)
                    pdev->pages[i].Page->id = 0;
        }
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

        code = ps2write_dsc_header(pdev, pages - 1);
    }

    /* Copy the resources into the main file. */

    s = pdev->strm;
    resource_pos = stell(s);
    sflush(pdev->asides.strm);
    {
        FILE *rfile = pdev->asides.file;
        int64_t res_end = gp_ftell_64(rfile);

        gp_fseek_64(rfile, 0L, SEEK_SET);
        pdf_copy_data(s, rfile, res_end, NULL);
    }

    if (pdev->ForOPDFRead && pdev->ProduceDSC) {
        int j;

        code = 0;
        pagecount = 1;

        /* All resources and procsets written, end the prolog */
        stream_puts(pdev->strm, "%%EndProlog\n");

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
                    pprintd2(pdev->strm, "%%%%PageBoundingBox: 0 0 %d %d\n", (int)page->MediaBox.x, (int)page->MediaBox.y);
                    stream_puts(pdev->strm, "%%BeginPageSetup\n");
                    stream_puts(pdev->strm, "/pagesave save def\n");
                    pdf_write_page(pdev, pagecount++);
                    stream_puts(pdev->strm, "%%EndPageSetup\n");
                    pprintld1(pdev->strm, "%ld 0 obj\n", pres->object->id);
                    code = cos_write(pres->object, pdev, pres->object->id);
                    stream_puts(pdev->strm, "endobj\n");
                    pres->object->written = true;
                    stream_puts(pdev->strm, "pagesave restore\n%%PageTrailer\n");
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
        linear_params.Offsets = (ulong *)gs_alloc_bytes(pdev->pdf_memory, pdev->next_id * sizeof(ulong), "temp xref storage");
        memset(linear_params.Offsets, 0x00, linear_params.LastResource * sizeof(ulong));
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
        if (pdev->FirstObjectNumber == 1)
            pprintld1(s, "xref\n0 %ld\n0000000000 65535 f \n",
                  end_section);
        else
            pprintld2(s, "xref\n0 1\n0000000000 65535 f \n%ld %ld\n",
                  start_section,
                  end_section - start_section);

        do {
            write_xref_section(pdev, tfile, start_section, end_section, resource_pos, linear_params.Offsets);
            if (end_section >= pdev->next_id)
                break;
            start_section = end_section + 1;
            end_section = find_end_xref_section(pdev, tfile, start_section, resource_pos);
            pprintld2(s, "%ld %ld\n", start_section, end_section - start_section);
        } while (1);

        /* Write the trailer. */

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
        pprintld1(s, "startxref\n%ld\n%%%%EOF\n", xref);
    }

    if (pdev->Linearise) {
        char Header[32], Binary[7] = "%\307\354\217\242\n";
        int level = (int)(pdev->CompatibilityLevel * 10 + 0.5), i, j;

        /* Make sure we've written everything to the main file */
        sflush(s);
        linear_params.sfile = pdev->file;
        linear_params.MainFileEnd = gp_ftell_64(pdev->file);
        linear_params.xref = xref;
        code = pdf_open_temp_file(pdev, &linear_params.Lin_File);
        linear_params.Lin_File.strm = 0x0;

        for (i = 0;i < pdev->ResourceUsageSize; i++) {
            if (pdev->ResourceUsage[i] == 1)
                linear_params.Page1_High = i;
        }

        /* Linearisation. Part 1 */
        sprintf(Header, "%%PDF-%d.%d\n", level / 10, level % 10);
        fwrite(Header, strlen(Header), 1, linear_params.Lin_File.file);
        if (pdev->binary_ok)
            fwrite(Binary, strlen(Binary), 1, linear_params.Lin_File.file);

        /* Re-order document, linearization dict to follow later (Part 2)
         * this is a FIXME
         */
        /* First page cross-reference table here (Part 3)
         * this is a FIXME
         */

        /* Write document catalog (Part 4) */
        code = rewrite_object(&linear_params, Catalog_id);
        /* In here we need the ViewerPreferences (don't think we support this),
         * the PaegMode entry (I think this would be direct in the catalog), The
         * Threads entry (if any), The OpenAction (again, direct object ?) The
         * AcroForm object (don't think we support this) and (TaDa) the Encrypt entry
         * in the first page trailer dictionary.
         * This is a FIXME.
         */

        /* Primary Hint Stream here (Part 5)
         * this is a FIXME
         */
#if 0
        /* First page section (Part 6) NB we do not currently support the OpenAction
         * In the Catalogso this is always page 0, this needs to change if we ever
         * support the OpenAction.
         */
        code = rewrite_object(&linear_params, pdev->pages[0].Page->id);
        pdev->ResourceUsage[pdev->pages[0].Page->id] = resource_usage_written;
        for (i = 0;i < pdev->ResourceUsageSize; i++) {
            if (pdev->ResourceUsage[i] == 1) {
                code = rewrite_object(&linear_params, i);
                pdev->ResourceUsage[i] = resource_usage_written;
            }
        }

        /* All remaining pages (part 7) */
        for (i = 1;i < pdev->next_page;i++) {
            code = rewrite_object(&linear_params, pdev->pages[i].Page->id);
            pdev->ResourceUsage[pdev->pages[i].Page->id] = resource_usage_written;
            for (j = 0;j < pdev->ResourceUsageSize; j++) {
                if (pdev->ResourceUsage[j] == i) {
                    code = rewrite_object(&linear_params, j);
                    pdev->ResourceUsage[j] = resource_usage_written;
                }
            }
        }

        /* Shared objects for all pages except the first (part 8) */
        for (i = 0;i < pdev->ResourceUsageSize; i++) {
            if (pdev->ResourceUsage[i] == resource_usage_page_shared) {
                code = rewrite_object(&linear_params, i);
                pdev->ResourceUsage[i] = resource_usage_written;
            }
        }

        /* All objects not on any page (Part 9) */
        for (i = 0;i < pdev->ResourceUsageSize; i++) {
            if (pdev->ResourceUsage[i] == resource_usage_not_referenced) {
                code = rewrite_object(&linear_params, i);
                pdev->ResourceUsage[i] = resource_usage_written;
            }
        }

        /* We won't bother with an overflow hint stream (I Hope) (part 10) */

        /* Main xref (part 11) */
#endif
        code = pdf_close_temp_file(pdev, &linear_params.Lin_File, code);
    }

    /* Require special handling for Fonts, ColorSpace and Pattern resources
     * These are tracked in pdev->last_resource, and are complex structures which may
     * contain other memory allocations. All other resource types can be simply dicarded
     * as above.
     */
    {
        /* PDF font records and all their associated contents need to be
         * freed by means other than the garbage collector, or the memory
         * usage will increase with languages other than PostScript. In addition
         * debug builds of non-PS languages take a long time to close down
         * due to reporting the dangling memory allocations.
         */
        int j, code = 0;

        for (j = 0; j < NUM_RESOURCE_CHAINS && code >= 0; ++j) {
            pdf_resource_t *pres = pdev->resources[resourceFont].chains[j];

            for (; pres != 0;) {
                pdf_font_resource_t *pdfont = (pdf_font_resource_t *)pres;

                font_resource_free(pdev, pdfont);
                pres = pres->next;
            }
        }
    }

    {
        int j, code = 0;

        for (j = 0; j < NUM_RESOURCE_CHAINS && code >= 0; ++j) {
            pdf_resource_t *pres = pdev->resources[resourceCIDFont].chains[j];

            for (; pres != 0;) {
                pdf_font_resource_t *pdfont = (pdf_font_resource_t *)pres;

                font_resource_free(pdev, pdfont);
                pres = pres->next;
            }
        }
    }

    {
        int j, code = 0;

        for (j = 0; j < NUM_RESOURCE_CHAINS && code >= 0; ++j) {
            pdf_resource_t *pres = pdev->resources[resourceFontDescriptor].chains[j];
            for (; pres != 0;) {
                pdf_font_descriptor_free(pdev, pres);
                pres = pres->next;
            }
        }
    }

    {
        int j, code = 0;

        for (j = 0; j < NUM_RESOURCE_CHAINS && code >= 0; ++j) {
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
        int j, code = 0;

        for (j = 0; j < NUM_RESOURCE_CHAINS && code >= 0; ++j) {
            pdf_resource_t *pres = pdev->resources[resourceColorSpace].chains[j];
            for (; pres != 0;) {
                free_color_space(pdev, pres);
                pres = pres->next;
            }
        }
    }

    {
        int j, code = 0;

        for (j = 0; j < NUM_RESOURCE_CHAINS && code >= 0; ++j) {
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
        int j, code = 0;

        for (j = 0; j < NUM_RESOURCE_CHAINS && code >= 0; ++j) {
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
        int j, code = 0;

        for (j = 0; j < NUM_RESOURCE_CHAINS && code >= 0; ++j) {
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
        int j, code = 0;

        for (j = 0; j < NUM_RESOURCE_CHAINS && code >= 0; ++j) {
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
        int j, code = 0;

        for (j = 0; j < NUM_RESOURCE_CHAINS && code >= 0; ++j) {
            pdf_resource_t *pnext = 0, *pres = pdev->resources[resourceFunction].chains[j];

            for (; pres != 0;) {
                if (pres->object) {
                    cos_release(pres->object, "free function dict");
                    gs_free_object(pdev->pdf_memory, pres->object, "free function resources");
                    pres->object = 0;
                    pnext = pres->next;
                }
                pres = pnext;
            }
        }
    }

    {
        int i, j;

        for (i = 0; i < NUM_RESOURCE_TYPES; i++) {
            for (j = 0; j < NUM_RESOURCE_CHAINS && code >= 0; ++j) {
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

    cos_release((cos_object_t *)pdev->NI_stack, "Release Name Index stack");
    gs_free_object(mem, pdev->NI_stack, "Free Name Index stack");
    pdev->NI_stack = 0;

    cos_release((cos_object_t *)pdev->Namespace_stack, "release Name space stack");
    gs_free_object(mem, pdev->Namespace_stack, "Free Name space stack");
    pdev->Namespace_stack = 0;

    pdev->Catalog = 0;
    pdev->Info = 0;

    memset(&pdev->outline_levels, 0x00, MAX_OUTLINE_DEPTH * sizeof(pdf_outline_level_t));

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
#ifdef DEPRECATED_906 /* Temporary disabled due to Bug 687686. */
        if (code >= 0)
            code = gs_note_error(gs_error_rangecheck);
#endif
    }
    code = pdf_close_files(pdev, code);
    if (code < 0)
        return code;

    if (pdev->Linearise) {
        gs_free_object(pdev->pdf_memory, linear_params.Offsets, "free temp xref storage");
        gs_free_object(pdev->pdf_memory, pdev->ResourceUsage, "free resource usage storage");
    }
    return code;
}
