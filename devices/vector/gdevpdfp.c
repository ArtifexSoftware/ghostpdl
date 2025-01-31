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


/* Get/put parameters for PDF-writing driver */
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gdevpdfx.h"
#include "gdevpdfo.h"
#include "gdevpdfg.h"
#include "gdevpsdf.h"
#include "gsparamx.h"
#include "gsicc_manage.h"

/*
 * The pdfwrite device supports the following "real" parameters:
 *      OutputFile <string>
 *      all the Distiller parameters (also see gdevpsdp.c)
 * Only some of the Distiller parameters actually have any effect.
 *
 * The device also supports the following write-only pseudo-parameters that
 * serve only to communicate other information from the PostScript file.
 * Their "value" is an array of strings, some of which may be the result
 * of converting arbitrary PostScript objects to string form.
 *      pdfmark - see gdevpdfm.c
 *	DSC - processed in this file
 */
static int pdf_dsc_process(gx_device_pdf * pdev,
                            const gs_param_string_array * pma);

static const int CoreDistVersion = 5000;	/* Distiller 5.0 */
static const gs_param_item_t pdf_param_items[] = {
#define pi(key, type, memb) { key, type, offset_of(gx_device_pdf, memb) }

        /* Acrobat Distiller 4 parameters */

    /*
     * EndPage and StartPage are renamed because EndPage collides with
     * a page device parameter.
     */
    pi("PDFEndPage", gs_param_type_int, EndPage),
    pi("PDFStartPage", gs_param_type_int, StartPage),
    pi("Optimize", gs_param_type_bool, Optimize),
    pi("ParseDSCCommentsForDocInfo", gs_param_type_bool,
       ParseDSCCommentsForDocInfo),
    pi("ParseDSCComments", gs_param_type_bool, ParseDSCComments),
    pi("EmitDSCWarnings", gs_param_type_bool, EmitDSCWarnings),
    pi("CreateJobTicket", gs_param_type_bool, CreateJobTicket),
    pi("PreserveEPSInfo", gs_param_type_bool, PreserveEPSInfo),
    pi("AutoPositionEPSFiles", gs_param_type_bool, AutoPositionEPSFiles),
    pi("PreserveCopyPage", gs_param_type_bool, PreserveCopyPage),
    pi("UsePrologue", gs_param_type_bool, UsePrologue),

        /* Acrobat Distiller 5 parameters */

    pi("OffOptimizations", gs_param_type_int, OffOptimizations),

        /* Ghostscript-specific parameters */

    pi("ReAssignCharacters", gs_param_type_bool, ReAssignCharacters),
    pi("ReEncodeCharacters", gs_param_type_bool, ReEncodeCharacters),
    pi("FirstObjectNumber", gs_param_type_long, FirstObjectNumber),
    pi("CompressFonts", gs_param_type_bool, CompressFonts),
    pi("CompressStreams", gs_param_type_bool, CompressStreams),
    pi("PrintStatistics", gs_param_type_bool, PrintStatistics),
    pi("MaxInlineImageSize", gs_param_type_long, MaxInlineImageSize),

        /* PDF Encryption */
    pi("OwnerPassword", gs_param_type_string, OwnerPassword),
    pi("UserPassword", gs_param_type_string, UserPassword),
    pi("KeyLength", gs_param_type_int, KeyLength),
    pi("Permissions", gs_param_type_int, Permissions),
    pi("EncryptionR", gs_param_type_int, EncryptionR),
    pi("NoEncrypt", gs_param_type_string, NoEncrypt),

        /* Target viewer capabilities (Ghostscript-specific)  */
 /* pi("ForOPDFRead", gs_param_type_bool, ForOPDFRead),			    pdfwrite-only */
    pi("ProduceDSC", gs_param_type_bool, ProduceDSC),
    pi("PatternImagemask", gs_param_type_bool, PatternImagemask),
    pi("MaxClipPathSize", gs_param_type_int, MaxClipPathSize),
    pi("MaxShadingBitmapSize", gs_param_type_int, MaxShadingBitmapSize),
    pi("HaveTrueTypes", gs_param_type_bool, HaveTrueTypes),
    pi("HaveCIDSystem", gs_param_type_bool, HaveCIDSystem),
    pi("HaveTransparency", gs_param_type_bool, HaveTransparency),
    pi("CompressEntireFile", gs_param_type_bool, CompressEntireFile),
    pi("PDFX", gs_param_type_int, PDFX),
    pi("PDFA", gs_param_type_int, PDFA),
    pi("DocumentUUID", gs_param_type_string, DocumentUUID),
    pi("InstanceUUID", gs_param_type_string, InstanceUUID),
    pi("DocumentTimeSeq", gs_param_type_int, DocumentTimeSeq),

    /* PDF/X parameters */
    pi("PDFXTrimBoxToMediaBoxOffset", gs_param_type_float_array, PDFXTrimBoxToMediaBoxOffset),
    pi("PDFXSetBleedBoxToMediaBox", gs_param_type_bool, PDFXSetBleedBoxToMediaBox),
    pi("PDFXBleedBoxToTrimBoxOffset", gs_param_type_float_array, PDFXBleedBoxToTrimBoxOffset),

    /* media selection parameters */
    pi("SetPageSize", gs_param_type_bool, SetPageSize),
    pi("RotatePages", gs_param_type_bool, RotatePages),
    pi("FitPages", gs_param_type_bool, FitPages),
    pi("CenterPages", gs_param_type_bool, CenterPages),
    pi("DoNumCopies", gs_param_type_bool, DoNumCopies),
    pi("PreserveSeparation", gs_param_type_bool, PreserveSeparation),
    pi("PreserveDeviceN", gs_param_type_bool, PreserveDeviceN),
    pi("PDFACompatibilityPolicy", gs_param_type_int, PDFACompatibilityPolicy),
    pi("DetectDuplicateImages", gs_param_type_bool, DetectDuplicateImages),
    pi("AllowIncrementalCFF", gs_param_type_bool, AllowIncrementalCFF),
    pi("WantsToUnicode", gs_param_type_bool, WantsToUnicode),
    pi("WantsOptionalContent", gs_param_type_bool, WantsOptionalContent),
    pi("PdfmarkCapable", gs_param_type_bool, PdfmarkCapable),
    pi("AllowPSRepeatFunctions", gs_param_type_bool, AllowPSRepeatFunctions),
    pi("IsDistiller", gs_param_type_bool, IsDistiller),
    pi("PreserveSMask", gs_param_type_bool, PreserveSMask),
    pi("PreserveTrMode", gs_param_type_bool, PreserveTrMode),
    pi("NoT3CCITT", gs_param_type_bool, NoT3CCITT),
    pi("FastWebView", gs_param_type_bool, Linearise),
    pi("NoOutputFonts", gs_param_type_bool, FlattenFonts),
    pi("WantsPageLabels", gs_param_type_bool, WantsPageLabels),
    pi("UserUnit", gs_param_type_float, UserUnit),
    pi("OmitInfoDate", gs_param_type_bool, OmitInfoDate),
    pi("OmitID", gs_param_type_bool, OmitID),
    pi("OmitXMP", gs_param_type_bool, OmitXMP),
    pi("ModifiesPageSize", gs_param_type_bool, ModifiesPageSize),
    pi("ModifiesPageOrder", gs_param_type_bool, ModifiesPageOrder),
    pi("WriteObjStms", gs_param_type_bool, WriteObjStms),
    pi("WriteXRefStm", gs_param_type_bool, WriteXRefStm),
    pi("ToUnicodeForStdEnc", gs_param_type_bool, ToUnicodeForStdEnc),
    pi("EmbedSubstituteFonts", gs_param_type_bool, EmbedSubstituteFonts),
#undef pi
    gs_param_item_end
};

/*
  Notes on implementing the remaining Distiller functionality
  ===========================================================

  Architectural issues
  --------------------

  Must optionally disable application of TR, BG, UCR similarly.  Affects:
    PreserveHalftoneInfo
    PreserveOverprintSettings
    TransferFunctionInfo
    UCRandBGInfo

  Current limitations
  -------------------

  Non-primary elements in HalftoneType 5 are not written correctly

  Acrobat Distiller 3
  -------------------

  ---- Image parameters ----

  AntiAlias{Color,Gray,Mono}Images

  ---- Other parameters ----

  CompressPages
    Compress things other than page contents
  * PreserveHalftoneInfo
  PreserveOPIComments
    ? see OPI spec?
  * PreserveOverprintSettings
  * TransferFunctionInfo
  * UCRandBGInfo
  ColorConversionStrategy
    Select color space for drawing commands
  ConvertImagesToIndexed
    Postprocess image data *after* downsampling (requires an extra pass)

  Acrobat Distiller 4
  -------------------

  ---- Other functionality ----

  Document structure pdfmarks

  ---- Parameters ----

  xxxDownsampleType = /Bicubic
    Add new filter (or use siscale?) & to setup (gdevpsdi.c)
  DetectBlends
    Idiom recognition?  PatternType 2 patterns / shfill?  (see AD4)
  DoThumbnails
    Also output to memory device -- resolution issue

  ---- Job-level control ----

  EmitDSCWarnings
    Require DSC parser / interceptor
  CreateJobTicket
    ?
  AutoPositionEPSFiles
    Require DSC parsing
  PreserveCopyPage
    Concatenate Contents streams
  UsePrologue
    Needs hack in top-level control?

*/

/* Transfer a collection of parameters. */
static const byte xfer_item_sizes[] = {
    GS_PARAM_TYPE_SIZES(0)
};
int
gdev_pdf_get_param(gx_device *dev, char *Param, void *list)
{
    gx_device_pdf *pdev = (gx_device_pdf *)dev;
    const gs_param_item_t *pi;
    gs_param_list * plist = (gs_param_list *)list;
    int code = 0;

    for (pi = pdf_param_items; pi->key != 0; ++pi) {
        if (strcmp(pi->key, Param) == 0) {
            const char *key = pi->key;
            const void *pvalue = (const void *)((const char *)pdev + pi->offset);
            int size = xfer_item_sizes[pi->type];
            gs_param_typed_value typed;

            memcpy(&typed.value, pvalue, size);
            typed.type = pi->type;
            code = (*plist->procs->xmit_typed) (plist, key, &typed);
            return code;
        }
    }
    if (strcmp(Param, "CoreDistVersion") == 0) {
        return(param_write_int(plist, "CoreDistVersion", &CoreDistVersion));
    }
    if (strcmp(Param, "CompatibilityLevel") == 0) {
        float f = pdev->CompatibilityLevel;
        return(param_write_float(plist, "CompatibilityLevel", &f));
    }
    if (strcmp(Param, "ForOPDFRead") == 0) {
        return(param_write_bool(plist, "ForOPDFRead", &pdev->ForOPDFRead));
    }
    if (strcmp(Param, "PassUserUnit") == 0) {
        bool dummy;
        if (pdev->CompatibilityLevel > 1.5)
            dummy = true;
        else
            dummy = false;
        return(param_write_bool(plist, "PassUserUnit", &dummy));
    }
    if (!pdev->is_ps2write) {
        if (strcmp(Param, "pdfmark")  == 0){
            return(param_write_null(plist, "pdfmark"));
        }
        if (strcmp(Param, "DSC") == 0){
            return(param_write_null(plist, "DSC"));
        }
    }

#if OCR_VERSION > 0
    if (strcmp(Param, "OCRLanguage") == 0) {
        gs_param_string langstr;
        if (pdev->ocr_language[0]) {
            langstr.data = (const byte *)pdev->ocr_language;
            langstr.size = strlen(pdev->ocr_language);
            langstr.persistent = false;
        } else {
            langstr.data = (const byte *)"eng";
            langstr.size = 3;
            langstr.persistent = false;
        }
        return param_write_string(plist, "OCRLanguage", &langstr);
    }
    if (strcmp(Param, "OCREngine") == 0)
        return param_write_int(plist, "OCREngine", &pdev->ocr_engine);

    if (strcmp(Param, "UseOCR") == 0) {
        gs_param_string ocrstr;

        switch(pdev->UseOCR) {
            case UseOCRNever:
                ocrstr.data = (const byte *)"Never";
                ocrstr.size = 5;
                ocrstr.persistent = false;
                break;
            case UseOCRAsNeeded:
                ocrstr.data = (const byte *)"AsNeeded";
                ocrstr.size = 8;
                ocrstr.persistent = false;
                break;
            case UseOCRAlways:
                ocrstr.data = (const byte *)"Always";
                ocrstr.size = 8;
                ocrstr.persistent = false;
                break;
        }
        return param_write_string(plist, "UseOCR", &ocrstr);
    }
#endif

    if (strcmp(Param, "OmitInfoDate") == 0)
        return(param_write_bool(plist, "OmitInfoDate", &pdev->OmitInfoDate));
    if (strcmp(Param, "OmitXMP") == 0)
        return(param_write_bool(plist, "OmitXMP", &pdev->OmitXMP));
    if (strcmp(Param, "OmitID") == 0)
        return(param_write_bool(plist, "OmitID", &pdev->OmitID));

    return gdev_psdf_get_param(dev, Param, list);
}

/* ---------------- Get parameters ---------------- */

/* Get parameters. */
int
gdev_pdf_get_params(gx_device * dev, gs_param_list * plist)
{
    gx_device_pdf *pdev = (gx_device_pdf *) dev;
    float cl = (float)pdev->CompatibilityLevel;
    int code;
    int cdv = CoreDistVersion;

#if OCR_VERSION > 0
    gs_param_string langstr;

    if (pdev->ocr_language[0]) {
        langstr.data = (const byte *)pdev->ocr_language;
        langstr.size = strlen(pdev->ocr_language);
        langstr.persistent = false;
    } else {
        langstr.data = (const byte *)"eng";
        langstr.size = 3;
        langstr.persistent = false;
    }

    {
        gs_param_string ocrstr;

        switch(pdev->UseOCR) {
            case UseOCRNever:
                ocrstr.data = (const byte *)"Never";
                ocrstr.size = 5;
                ocrstr.persistent = false;
                break;
            case UseOCRAsNeeded:
                ocrstr.data = (const byte *)"AsNeeded";
                ocrstr.size = 8;
                ocrstr.persistent = false;
                break;
            case UseOCRAlways:
                ocrstr.data = (const byte *)"Always";
                ocrstr.size = 8;
                ocrstr.persistent = false;
                break;
        }
        code = param_write_string(plist, "UseOCR", &ocrstr);
    }
    code = param_write_string(plist, "OCRLanguage", &langstr);
    if(code < 0)
        return code;
    code = param_write_int(plist, "OCREngine", &pdev->ocr_engine);
    if(code < 0)
        return code;
#endif

    pdev->ParamCompatibilityLevel = cl;
    code = gdev_psdf_get_params(dev, plist);
    if (code < 0 ||
        (code = param_write_int(plist, "CoreDistVersion", &cdv)) < 0 ||
        (code = param_write_float(plist, "CompatibilityLevel", &cl)) < 0 ||
        (!pdev->is_ps2write && (code = param_write_bool(plist, "ForOPDFRead", &pdev->ForOPDFRead)) < 0) ||
        /* Indicate that we can process pdfmark and DSC. */
        (param_requested(plist, "pdfmark") > 0 &&
         (code = param_write_null(plist, "pdfmark")) < 0) ||
        (param_requested(plist, "DSC") > 0 &&
         (code = param_write_null(plist, "DSC")) < 0) ||
        (code = gs_param_write_items(plist, pdev, NULL, pdf_param_items)) < 0
        )
    {}
    return code;
}

/* ---------------- Put parameters ---------------- */

/* Put parameters, implementation */
static int
gdev_pdf_put_params_impl(gx_device * dev, const gx_device_pdf * save_dev, gs_param_list * plist)
{
    int ecode, code;
    gx_device_pdf *pdev = (gx_device_pdf *) dev;
    float cl = (float)pdev->CompatibilityLevel;
    bool locked = pdev->params.LockDistillerParams, ForOPDFRead;
    bool XRefStm_set = false, ObjStms_set = false;
    gs_param_name param_name;

    pdev->pdf_memory = gs_memory_stable(pdev->memory);
    /*
     * If this is a pseudo-parameter (pdfmark or DSC),
     * don't bother checking for any real ones.
     */

    {
        gs_param_string_array ppa;
        gs_param_string pps;

        code = param_read_string_array(plist, (param_name = "pdfmark"), &ppa);
        switch (code) {
            case 0:
                code = pdfwrite_pdf_open_document(pdev);
                if (code < 0)
                    return code;
                code = pdfmark_process(pdev, &ppa);
                if (code >= 0)
                    return code;
                /* falls through for errors */
            default:
                param_signal_error(plist, param_name, code);
                return code;
            case 1:
                break;
        }

        code = param_read_string_array(plist, (param_name = "DSC"), &ppa);
        switch (code) {
            case 0:
                code = pdfwrite_pdf_open_document(pdev);
                if (code < 0)
                    return code;
                code = pdf_dsc_process(pdev, &ppa);
                if (code >= 0)
                    return code;
                /* falls through for errors */
            default:
                param_signal_error(plist, param_name, code);
                return code;
            case 1:
                break;
        }

        code = param_read_string(plist, (param_name = "pdfpagelabels"), &pps);
        switch (code) {
            case 0:
                {
                    if (!pdev->ForOPDFRead) {
                        cos_dict_t *const pcd = pdev->Catalog;
                        code = pdfwrite_pdf_open_document(pdev);
                        if (code < 0)
                            return code;
                        code = cos_dict_put_string(pcd, (const byte *)"/PageLabels", 11,
                                   pps.data, pps.size);
                        if (code >= 0)
                            return code;
                    } else
                        return 0;
                 }
                /* falls through for errors */
            default:
                param_signal_error(plist, param_name, code);
                return code;
            case 1:
                break;
        }
    }

#if OCR_VERSION > 0
    {
        int len;
        gs_param_string langstr;
        switch (code = param_read_string(plist, (param_name = "OCRLanguage"), &langstr)) {
            case 0:
                if (gs_is_path_control_active(pdev->memory)
                && (strlen(pdev->ocr_language) != langstr.size || memcmp(pdev->ocr_language, langstr.data, langstr.size) != 0)) {
                    return_error(gs_error_invalidaccess);
                }
                else {
                    len = langstr.size;
                    if (len >= sizeof(pdev->ocr_language))
                        len = sizeof(pdev->ocr_language)-1;
                    memcpy(pdev->ocr_language, langstr.data, len);
                    pdev->ocr_language[len] = 0;
                }
                break;
            case 1:
                break;
            default:
                ecode = code;
                param_signal_error(plist, param_name, ecode);
        }
    }

    {
        int engine;
        switch (code = param_read_int(plist, (param_name = "OCREngine"), &engine)) {
            case 0:
                pdev->ocr_engine = engine;
                break;
            case 1:
                break;
            default:
                ecode = code;
                param_signal_error(plist, param_name, ecode);
        }
    }

    {
        gs_param_string ocrstr;

        code = param_read_string(plist, (param_name = "UseOCR"), &ocrstr);
        switch(code) {
            case 0:
                if (ocrstr.size == 5 && memcmp(ocrstr.data, "Never", 5) == 0)
                    pdev->UseOCR = UseOCRNever;
                if (ocrstr.size == 8 && memcmp(ocrstr.data, "AsNeeded", 8) == 0)
                    pdev->UseOCR = UseOCRAsNeeded;
                if (ocrstr.size == 6 && memcmp(ocrstr.data, "Always", 6) == 0)
                    pdev->UseOCR = UseOCRAlways;
                break;
            case 1:
                break;
            default:
                param_signal_error(plist, param_name, code);
                break;
        }
    }

    {
        gs_param_string ocrstr;

        code = param_read_string(plist, (param_name = "UseOCR"), &ocrstr);
        switch(code) {
            case 0:
                if (ocrstr.size == 5 && memcmp(ocrstr.data, "Never", 5) == 0)
                    pdev->UseOCR = UseOCRNever;
                if (ocrstr.size == 8 && memcmp(ocrstr.data, "AsNeeded", 8) == 0)
                    pdev->UseOCR = UseOCRAsNeeded;
                if (ocrstr.size == 6 && memcmp(ocrstr.data, "Always", 6) == 0)
                    pdev->UseOCR = UseOCRAlways;
                break;
            case 1:
                break;
            default:
                param_signal_error(plist, param_name, code);
                break;
        }
    }
#endif

    /*
     * Check for LockDistillerParams before doing anything else.
     * If LockDistillerParams is true and is not being set to false,
     * ignore all resettings of PDF-specific parameters.  Note that
     * LockDistillerParams is read again, and reset if necessary, in
     * psdf_put_params.
     */
    ecode = param_read_bool(plist, (param_name = "LockDistillerParams"), &locked);
    if (ecode < 0)
        param_signal_error(plist, param_name, ecode);

    /* General parameters. */

    {
        int efo = 1;

        ecode = param_put_int(plist, (param_name = ".EmbedFontObjects"), &efo, ecode);
        if (ecode < 0)
            param_signal_error(plist, param_name, ecode);
        if (efo != 1)
            param_signal_error(plist, param_name, ecode = gs_error_rangecheck);
    }
    {
        int cdv = CoreDistVersion;

        ecode = param_put_int(plist, (param_name = "CoreDistVersion"), &cdv, ecode);
        if (ecode < 0)
            return gs_note_error(ecode);
        if (cdv != CoreDistVersion)
            param_signal_error(plist, param_name, ecode = gs_error_rangecheck);
    }

    switch (code = param_read_float(plist, (param_name = "CompatibilityLevel"), &cl)) {
        default:
            ecode = code;
            param_signal_error(plist, param_name, ecode);
            break;
        case 0:
            if (!(locked && pdev->params.LockDistillerParams)) {
                /*
                 * Must be 1.2, 1.3, 1.4, or 1.5.  Per Adobe documentation, substitute
                 * the nearest achievable value.
                 */
                if (cl < (float)1.15)
                    cl = (float)1.1;
                else if (cl < (float)1.25)
                    cl = (float)1.2;
                else if (cl < (float)1.35)
                    cl = (float)1.3;
                else if (cl < (float)1.45)
                    cl = (float)1.4;
                else if (cl < (float)1.55)
                    cl = (float)1.5;
                else if (cl < (float)1.65)
                    cl = (float)1.6;
                else if (cl < (float)1.75)
                    cl = (float)1.7;
                else {
                    cl = (float)2.0;
                    if (pdev->params.TransferFunctionInfo == tfi_Preserve)
                        pdev->params.TransferFunctionInfo = tfi_Apply;
                }
            }
        case 1:
            break;
    }

    ecode = param_read_bool(plist, (param_name = "WriteXRefStm"), &pdev->WriteXRefStm);
    if (ecode < 0)
        param_signal_error(plist, param_name, ecode);
    if (ecode == 0)
        XRefStm_set = true;
    ecode = param_read_bool(plist, (param_name = "WriteObjStms"), &pdev->WriteObjStms);
    if (ecode < 0)
        param_signal_error(plist, param_name, ecode);
    if (ecode == 0)
        ObjStms_set = true;

    {
        code = gs_param_read_items(plist, pdev, pdf_param_items, pdev->pdf_memory);
        if (code < 0 || (code = param_read_bool(plist, "ForOPDFRead", &ForOPDFRead)) < 0)
        {
        }
        if (code == 0 && !pdev->is_ps2write && !(locked && pdev->params.LockDistillerParams))
            pdev->ForOPDFRead = ForOPDFRead;
    }
    if (code < 0)
        ecode = code;
    {
        /*
         * Setting FirstObjectNumber is only legal if the file
         * has just been opened and nothing has been written,
         * or if we are setting it to the same value.
         */
        int64_t fon = pdev->FirstObjectNumber;

        if (fon != save_dev->FirstObjectNumber) {
            if (fon <= 0 || fon > 0x7fff0000 ||
                (pdev->next_id != 0 &&
                 pdev->next_id !=
                 save_dev->FirstObjectNumber + pdf_num_initial_ids)
                ) {
                ecode = gs_error_rangecheck;
                param_signal_error(plist, "FirstObjectNumber", ecode);
            }
        }
    }
    {
        /*
         * Set ProcessColorModel now, because gx_default_put_params checks
         * it.
         */
        static const char *const pcm_names[] = {
            "DeviceGray", "DeviceRGB", "DeviceCMYK", "DeviceN", 0
        };
        int pcm = -1;

        ecode = param_put_enum(plist, "ProcessColorModel", &pcm,
                               pcm_names, ecode);
        if (ecode < 0)
            goto fail;
        if (pcm >= 0) {
            pdf_set_process_color_model(pdev, pcm);
            rc_decrement(pdev->icc_struct, "gdev_pdf_put_params_impl, ProcessColorModel changed");
            pdev->icc_struct = 0;
        }
    }

    if (pdev->is_ps2write && (code = param_read_bool(plist, "ProduceDSC", &pdev->ProduceDSC)) < 0) {
        param_signal_error(plist, param_name, code);
    }

    if (pdev->OmitInfoDate && pdev->PDFX != 0) {
        emprintf(pdev->memory, "\nIt is not possible to omit the CreationDate when creating PDF/X\nOmitInfoDate is being ignored.\n");
        pdev->OmitInfoDate = 0;
    }
    if (pdev->OmitID && pdev->PDFX != 0) {
        emprintf(pdev->memory, "\nIt is not possible to omit the ID array when creating PDF/X\nOmitID is being ignored.\n");
        pdev->OmitID = 0;
    }
    if (pdev->OmitID && pdev->CompatibilityLevel > 1.7) {
        emprintf(pdev->memory, "\nIt is not possible to omit the ID array when creating a version 2.0 or greater PDF\nOmitID is being ignored.\n");
        pdev->OmitID = 0;
    }
    if (pdev->OmitID && pdev->OwnerPassword.size != 0) {
        emprintf(pdev->memory, "\nIt is not possible to omit the ID array when creating an encrypted PDF\nOmitID is being ignored.\n");
        pdev->OmitID = 0;
    }

    if (pdev->OmitXMP && pdev->PDFA != 0) {
        emprintf(pdev->memory, "\nIt is not possible to omit the XMP metadta when creating a PDF/A\nOmitXMP is being ignored.\n");
        pdev->OmitXMP = 0;
    }

    /* PDFA and PDFX are stored in the page device dictionary and therefore
     * set on every setpagedevice. However, if we have encountered a file which
     * can't be made this way, and the PDFACompatibilityPolicy is 1, we want to
     * continue producing the file, but not as a PDF/A or PDF/X file. Its more
     * or less impossible to alter the setting in the (potentially saved) page
     * device dictionary, so we use this rather clunky method.
     */
    if (pdev->PDFA < 0 || pdev->PDFA > 3){
        ecode = gs_note_error(gs_error_rangecheck);
        param_signal_error(plist, "PDFA", ecode);
        goto fail;
    }
    if(pdev->PDFA != 0 && pdev->AbortPDFAX)
        pdev->PDFA = 0;
    if(pdev->PDFX && pdev->AbortPDFAX)
        pdev->PDFX = 0;
    if (pdev->PDFX && pdev->PDFA != 0) {
        ecode = gs_note_error(gs_error_rangecheck);
        param_signal_error(plist, "PDFA", ecode);
        goto fail;
    }
    if (pdev->PDFX != 0 && pdev->ForOPDFRead) {
        ecode = gs_note_error(gs_error_rangecheck);
        param_signal_error(plist, "PDFX", ecode);
        goto fail;
    }
    if (pdev->PDFA != 0 && pdev->ForOPDFRead) {
        ecode = gs_note_error(gs_error_rangecheck);
        param_signal_error(plist, "PDFA", ecode);
        goto fail;
    }
    if (pdev->PDFA == 1 || (pdev->PDFX > 0 && pdev->PDFX < 4) || pdev->CompatibilityLevel < 1.4) {
         pdev->HaveTransparency = false;
         pdev->PreserveSMask = false;
         pdev->WantsOptionalContent = false;
    }

    /* We cannot guarantee that all page objects have a Group with a /CS (ColorSpace) entry
     * which is a requirement for PDF/A-2+ when objects are defined in Device Independent Colour.
     * Try and warn the user.
     */
    if (pdev->PDFA > 1 && pdev->params.ColorConversionStrategy == ccs_UseDeviceIndependentColor) {
        switch (pdev->PDFACompatibilityPolicy) {
            case 0:
                emprintf(pdev->memory,
                 "\n\tpdfwrite cannot guarantee creating a conformant PDF/A-2 file with device-independent colour.\n\tWe recommend converting to a device colour space.\n\tReverting to normal output.\n");
                pdev->AbortPDFAX = true;
                pdev->PDFA = 0;
                break;
            case 1:
                emprintf(pdev->memory,
                 "\n\tpdfwrite cannot guarantee creating a conformant PDF/A-2 file with device-independent colour.\n\tWe recommend converting to a device colour space.\n\tWe cannot ignore this request, reverting to normal output.\n");
                break;
            case 2:
                emprintf(pdev->memory,
                 "\n\tpdfwrite cannot guarantee creating a conformant PDF/A-2 file with device-independent colour.\n\tWe recommend converting to a device colour space.\n\tAborting.\n");
                return_error(gs_error_unknownerror);
                break;
            default:
                emprintf(pdev->memory,
                 "\n\tpdfwrite cannot guarantee creating a conformant PDF/A-2 file with device-independent colour.\n\tWe recommend converting to a device colour space.\n\tReverting to normal output.\n");
                pdev->AbortPDFAX = true;
                pdev->PDFA = 0;
                break;
        }
    }
    /*
     * We have to set version to the new value, because the set of
     * legal parameter values for psdf_put_params varies according to
     * the version.
     */
    if (pdev->PDFX != 0) {
        if (pdev->PDFX < 4) {
            cl = (float)1.3; /* Instead pdev->CompatibilityLevel = 1.2; - see below. */
            if (pdev->WriteObjStms && ObjStms_set)
                emprintf(pdev->memory, "Can't use ObjStm before PDF 1.5, PDF/X does not support PDF 1.5, ignoring WriteObjStms directive\n");
            if (pdev->WriteXRefStm && XRefStm_set)
                emprintf(pdev->memory, "Can't use an XRef stream before PDF 1.5, PDF/X does not support PDF 1.5, ignoring WriteXRefStm directive\n");

            pdev->WriteObjStms = false;
            pdev->WriteXRefStm = false;
        } else {
            cl = (float)1.6; /* Instead pdev->CompatibilityLevel = 1.2; - see below. */
        }
    }
    if (pdev->PDFA == 1 && cl != 1.4) {
        cl = (float)1.4;

        if (pdev->WriteObjStms && ObjStms_set)
            emprintf(pdev->memory, "Can't use ObjStm before PDF 1.5, PDF/A-1 does not support PDF 1.5, ignoring WriteObjStms directive\n");
        if (pdev->WriteXRefStm && XRefStm_set)
            emprintf(pdev->memory, "Can't use an XRef stream before PDF 1.5, PDF/A-1 does not support PDF 1.5, ignoring WriteXRefStm directive\n");
        pdev->WriteObjStms = false;
        pdev->WriteXRefStm = false;
    }
    if (pdev->PDFA == 2 && cl < 1.7)
        cl = (float)1.7;
    pdev->version = (cl < 1.2 ? psdf_version_level2 : psdf_version_ll3);
    if (pdev->ForOPDFRead) {
        pdev->ResourcesBeforeUsage = true;
        pdev->HaveCFF = false;
        pdev->HavePDFWidths = false;
        pdev->HaveStrokeColor = false;
        cl = (float)1.2; /* Instead pdev->CompatibilityLevel = 1.2; - see below. */
        pdev->MaxInlineImageSize = max_long; /* Save printer's RAM from saving temporary image data.
                                                Immediate images doen't need buffering. */
        pdev->version = psdf_version_level2;
    } else {
        pdev->ResourcesBeforeUsage = false;
        pdev->HaveCFF = true;
        pdev->HavePDFWidths = true;
        pdev->HaveStrokeColor = true;
    }
    pdev->ParamCompatibilityLevel = cl;
    if (cl < 1.2) {
        pdev->HaveCFF = false;
    }
    if (cl < 1.5)
        pdev->WantsOptionalContent = false;

    ecode = param_read_float(plist, "UserUnit", &pdev->UserUnit);
    if (ecode < 0)
        goto fail;
    if (pdev->UserUnit == 0 || (pdev->UserUnit != 1 && pdev->CompatibilityLevel < 1.6)) {
        ecode = gs_note_error(gs_error_rangecheck);
        param_signal_error(plist, "UserUnit", ecode);
        goto fail;
    }

    ecode = gdev_psdf_put_params(dev, plist);
    if (ecode < 0)
        goto fail;

    if (pdev->CompatibilityLevel > 1.7 && pdev->params.TransferFunctionInfo == tfi_Preserve) {
        pdev->params.TransferFunctionInfo = tfi_Apply;
        emprintf(pdev->memory, "\nIt is not possible to preserve transfer functions in PDF 2.0\ntransfer functions will be applied instead\n");
    }

    if (pdev->params.ConvertCMYKImagesToRGB) {
        if (pdev->params.ColorConversionStrategy == ccs_CMYK) {
            emprintf(pdev->memory, "ConvertCMYKImagesToRGB is not compatible with ColorConversionStrategy of CMYK\n");
        } else {
            if (pdev->params.ColorConversionStrategy == ccs_Gray) {
                emprintf(pdev->memory, "ConvertCMYKImagesToRGB is not compatible with ColorConversionStrategy of Gray\n");
            } else {
                pdf_set_process_color_model(pdev,1);
                ecode = gsicc_init_device_profile_struct((gx_device *)pdev, NULL, 0);
                if (ecode < 0)
                    goto fail;
            }
        }
    }
    /* Record if the user has specified a custom profile, so we don't replace it
       with a default when we change color space - see win_pr2_set_bpp()
       Here, we only want to know *if* we have a user specced profile, actually
       setting it will be handled by gdev_vector_put_params()
     */
    if (!pdev->user_icc) {
        gs_param_string icc_pro_dummy;

        pdev->user_icc = param_read_string(plist, "OutputICCProfile", &icc_pro_dummy) == 0;
    }
    switch (pdev->params.ColorConversionStrategy) {
        case ccs_ByObjectType:
        case ccs_LeaveColorUnchanged:
            break;
        case ccs_UseDeviceIndependentColor:
        case ccs_UseDeviceIndependentColorForImages:
            pdev->params.TransferFunctionInfo = tfi_Apply;
            break;
        case ccs_CMYK:
            pdf_set_process_color_model(pdev, 2);
            if (!pdev->user_icc) {
                ecode = gsicc_init_device_profile_struct((gx_device *)pdev, NULL, 0);
                if (ecode < 0)
                    goto fail;
            }
            break;
        case ccs_Gray:
            pdf_set_process_color_model(pdev,0);
            if (!pdev->user_icc) {
                ecode = gsicc_init_device_profile_struct((gx_device *)pdev, NULL, 0);
                if (ecode < 0)
                    goto fail;
            }
            break;
        case ccs_sRGB:
        case ccs_RGB:
            /* Only bother to do this if we didn't handle it above */
            if (!pdev->params.ConvertCMYKImagesToRGB) {
                pdf_set_process_color_model(pdev,1);
                if (!pdev->user_icc) {
                    ecode = gsicc_init_device_profile_struct((gx_device *)pdev, NULL, 0);
                    if (ecode < 0)
                        goto fail;
                }
            }
            break;
        default:
            break;
    }
    if (cl < 1.5f && pdev->params.ColorImage.Filter != NULL &&
            !strcmp(pdev->params.ColorImage.Filter, "JPXEncode")) {
        emprintf(pdev->memory,
                 "JPXEncode requires CompatibilityLevel >= 1.5 .\n");
        ecode = gs_note_error(gs_error_rangecheck);
    }
    if (cl < 1.5f && pdev->params.GrayImage.Filter != NULL &&
            !strcmp(pdev->params.GrayImage.Filter, "JPXEncode")) {
        emprintf(pdev->memory,
                 "JPXEncode requires CompatibilityLevel >= 1.5 .\n");
        ecode = gs_note_error(gs_error_rangecheck);
    }
    if (cl < 1.4f && pdev->params.MonoImage.Filter != NULL &&
            !strcmp(pdev->params.MonoImage.Filter, "JBIG2Encode")) {
        emprintf(pdev->memory,
                 "JBIG2Encode requires CompatibilityLevel >= 1.4 .\n");
        ecode = gs_note_error(gs_error_rangecheck);
    }
    if (pdev->HaveTrueTypes && pdev->version == psdf_version_level2) {
        pdev->version = psdf_version_level2_with_TT ;
    }
    if (ecode < 0)
        goto fail;

    if (pdev->FirstObjectNumber != save_dev->FirstObjectNumber) {
        if (pdev->xref.file != 0) {
            if (gp_fseek(pdev->xref.file, 0L, SEEK_SET) != 0) {
                ecode = gs_error_ioerror;
                goto fail;
            }
            pdf_initialize_ids(pdev);
        }
    }
    /* Handle the float/double mismatch. */
    pdev->CompatibilityLevel = (int)(cl * 10 + 0.5) / 10.0;

    if (pdev->ForOPDFRead && pdev->OwnerPassword.size != 0) {
        emprintf(pdev->memory, "\n\tSetting OwnerPassword for PostScript output would result in an encrypted\n\tunusable PostScript file, ignoring.\n");
        pdev->OwnerPassword.size = 0;
    }

    if(pdev->OwnerPassword.size != save_dev->OwnerPassword.size ||
        (pdev->OwnerPassword.size != 0 &&
         memcmp(pdev->OwnerPassword.data, save_dev->OwnerPassword.data,
         pdev->OwnerPassword.size) != 0)) {
        if (pdev->is_open) {
            if (pdev->PageCount == 0) {
                gs_closedevice((gx_device *)save_dev);
                return 0;
            }
            else
                emprintf(pdev->memory, "Owner Password changed mid-job, ignoring.\n");
        }
    }

    if (pdev->Linearise && pdev->file != NULL && !gp_fseekable(pdev->file)) {
        emprintf(pdev->memory, "Can't linearise a non-seekable output file, ignoring\n");
        pdev->Linearise = false;
    }

    if (pdev->Linearise && pdev->is_ps2write) {
        emprintf(pdev->memory, "Can't linearise PostScript output, ignoring\n");
        pdev->Linearise = false;
    }

    if (pdev->Linearise && pdev->OwnerPassword.size != 0) {
        emprintf(pdev->memory, "Can't linearise encrypted PDF, ignoring\n");
        pdev->Linearise = false;
    }

    /* Object streams and XRef streams */
    if (pdev->is_ps2write) {
        pdev->WriteObjStms = false;
        pdev->WriteXRefStm = false;
    } else {
        if (pdev->Linearise) {
            if (XRefStm_set)
                emprintf(pdev->memory, "We don't support XRefStm with FastWebView (Linearized PDF), ignoring WriteXRefStm directive\n");
            pdev->WriteXRefStm = false;
            if (ObjStms_set)
                emprintf(pdev->memory, "We don't support ObjStms with FastWebView (Linearized PDF), ignoring WriteObjStms directive\n");
            pdev->WriteObjStms = false;
        }
    }
    if (pdev->WriteObjStms && pdev->CompatibilityLevel < 1.5) {
        if (ObjStms_set)
            emprintf(pdev->memory, "Can't use Object streams before PDF 1.5, ignoring WriteObjStms directive\n");
        pdev->WriteObjStms = false;
    }
    if (pdev->WriteXRefStm && pdev->CompatibilityLevel < 1.5) {
        if (XRefStm_set)
            emprintf(pdev->memory, "Can't use an XRef stream before PDF 1.5, ignoring WriteXRefStm directive\n");
        pdev->WriteXRefStm = false;
    }
    if (pdev->WriteObjStms && !pdev->WriteXRefStm) {
        if (ObjStms_set)
            emprintf(pdev->memory, "Can't use Object streams without XRef stream, ignoring WriteObjStms directive\n");
        pdev->WriteObjStms = false;
    }
    if (pdev->WriteObjStms) {
        if (pdev->next_id == 1)
            pdev->doubleXref = true;
        if (!pdev->doubleXref) {
            emprintf(pdev->memory, "Already started writing output, too late to select WriteObjStms, ignoring directive\n");
            pdev->WriteObjStms = false;
        }
    }

    if (pdev->FlattenFonts)
        pdev->PreserveTrMode = false;
    return 0;
 fail:
    /* Restore all the parameters to their original state. */
    pdev->version = save_dev->version;
    pdf_set_process_color_model(pdev, save_dev->pcm_color_info_index);
    pdev->saved_fill_color = save_dev->saved_fill_color;
    pdev->saved_stroke_color = save_dev->saved_fill_color;
    {
        const gs_param_item_t *ppi = pdf_param_items;

        for (; ppi->key; ++ppi)
            memcpy((char *)pdev + ppi->offset,
                   (char *)save_dev + ppi->offset,
                   gs_param_type_sizes[ppi->type]);
        pdev->ForOPDFRead = save_dev->ForOPDFRead;
    }
    return ecode;
}

/* Put parameters */
int
gdev_pdf_put_params(gx_device * dev, gs_param_list * plist)
{
    int code;
    gx_device_pdf *pdev = (gx_device_pdf *) dev;
    gs_memory_t *mem = gs_memory_stable(pdev->memory);
    gx_device_pdf *save_dev = gs_malloc(mem, sizeof(gx_device_pdf), 1,
        "saved gx_device_pdf");

    if (!save_dev)
        return_error(gs_error_VMerror);
    memcpy(save_dev, pdev, sizeof(gx_device_pdf));
    code = gdev_pdf_put_params_impl(dev, save_dev, plist);
    gs_free(mem, save_dev, sizeof(gx_device_pdf), 1, "saved gx_device_pdf");
    return code;
}

/* ---------------- Process DSC comments ---------------- */

/* Bug #695850 DSC comments are not actually encoded, nor are they PostScript strings
 * they are simply a sequence of bytes. SO it would seem we should just preserve that
 * byte sequence. Bizarrely, Distiller treats the comment as 'almost' a PostScript
 * string. In particular it converts octal codes into an equivalent binary byte. It
 * also converts (eg) '\n' and '\r' into 'n' and 'r' and invalid octal escapes
 * (eg \11) simply have the '\' turned into a '?'.
 * We think this is nuts and have no intention of trying to mimic it precisely. This
 * routine will find octal escapes and convert them into binary. The way this works is
 * a little obscure. The DSC parser does convert the comment into a PostScript string
 * and so has to escape any unusual characters. This means our octal escaped values in
 * the original DSC comment have had the escape character ('\') escaped to become '\\'.
 * All we need to do is remove the escape of the escape and we will end up with a
 * properly escaped PostScript string.
 */
static int unescape_octals(gx_device_pdf * pdev, char *src, int size)
{
    char *start, *dest;

    start = src;
    dest = src;

    while(size) {
        if (size > 4 && src[0] == '\\' && src[1] == '\\' &&
                src[2] > 0x29 && src[2] < 0x35 &&
                src[3] > 0x29 &&src[3] < 0x38 &&
                src[4] > 0x29 && src[4] < 0x38) {
            src++;
            size--;
        } else {
            *dest++ = *src++;
            size--;
        }
    }
    return (dest - start);
}

static int
pdf_dsc_process(gx_device_pdf * pdev, const gs_param_string_array * pma)
{
    /*
     * The Adobe "Distiller Parameters" documentation says that Distiller
     * looks at DSC comments, but it doesn't say which ones.  We look at
     * the ones that we see how to map directly to obvious PDF constructs.
     */
    int code = 0;
    uint i;

    /*
     * If ParseDSCComments is false, all DSC comments are ignored, even if
     * ParseDSCComentsForDocInfo or PreserveEPSInfo is true.
     */
    if (!pdev->ParseDSCComments)
        return 0;

    for (i = 0; i + 1 < pma->size && code >= 0; i += 2) {
        const gs_param_string *pkey = &pma->data[i];
        gs_param_string *pvalue = (gs_param_string *)&pma->data[i + 1];
        const char *key;
        int newsize;

        /*
         * %%For, %%Creator, and %%Title are recognized only if either
         * ParseDSCCommentsForDocInfo or PreserveEPSInfo is true.
         * The other DSC comments are always recognized.
         *
         * Acrobat Distiller sets CreationDate and ModDate to the current
         * time, not the value of %%CreationDate.  We think this is wrong,
         * but we do the same -- we ignore %%CreationDate here.
         */

        if (pdf_key_eq(pkey, "Creator") && pdev->CompatibilityLevel <= 1.7) {
            key = "/Creator";
            newsize = unescape_octals(pdev, (char *)pvalue->data, pvalue->size);
            code = cos_dict_put_c_key_string(pdev->Info, key,
                                             pvalue->data, newsize);
            continue;
        } else if (pdf_key_eq(pkey, "Title") && pdev->CompatibilityLevel <= 1.7) {
            key = "/Title";
            newsize = unescape_octals(pdev, (char *)pvalue->data, pvalue->size);
            code = cos_dict_put_c_key_string(pdev->Info, key,
                                             pvalue->data, newsize);
            continue;
        } else if (pdf_key_eq(pkey, "For") && pdev->CompatibilityLevel <= 1.7) {
            key = "/Author";
            newsize = unescape_octals(pdev, (char *)pvalue->data, pvalue->size);
            code = cos_dict_put_c_key_string(pdev->Info, key,
                                             pvalue->data, newsize);
            continue;
        } else {
            pdf_page_dsc_info_t *ppdi;
            char scan_buf[200]; /* arbitrary */

            if ((ppdi = &pdev->doc_dsc_info,
                 pdf_key_eq(pkey, "Orientation")) ||
                (ppdi = &pdev->page_dsc_info,
                 pdf_key_eq(pkey, "PageOrientation"))
                ) {
                if (pvalue->size == 1 && pvalue->data[0] >= '0' &&
                    pvalue->data[0] <= '3'
                    )
                    ppdi->orientation = pvalue->data[0] - '0';
                else
                    ppdi->orientation = -1;
            } else if ((ppdi = &pdev->doc_dsc_info,
                        pdf_key_eq(pkey, "ViewingOrientation")) ||
                       (ppdi = &pdev->page_dsc_info,
                        pdf_key_eq(pkey, "PageViewingOrientation"))
                       ) {
                gs_matrix mat;
                int orient;

                if(pvalue->size >= sizeof(scan_buf) - 1)
                    continue;	/* error */
                memcpy(scan_buf, pvalue->data, pvalue->size);
                scan_buf[pvalue->size] = 0;
                if (sscanf(scan_buf, "[%g %g %g %g]",
                           &mat.xx, &mat.xy, &mat.yx, &mat.yy) != 4
                    )
                    continue;	/* error */
                for (orient = 0; orient < 4; ++orient) {
                    if (mat.xx == 1 && mat.xy == 0 && mat.yx == 0 && mat.yy == 1)
                        break;
                    gs_matrix_rotate(&mat, -90.0, &mat);
                }
                if (orient == 4) /* error */
                    orient = -1;
                ppdi->viewing_orientation = orient;
            } else {
                gs_rect box;

                if (pdf_key_eq(pkey, "EPSF")) {
                    pdev->is_EPS = (pvalue->size >= 1 && pvalue->data[0] != '0');
                    continue;
                }
                /*
                 * We only parse the BoundingBox for the sake of
                 * AutoPositionEPSFiles.
                 */
                if (pdf_key_eq(pkey, "BoundingBox"))
                    ppdi = &pdev->doc_dsc_info;
                else if (pdf_key_eq(pkey, "PageBoundingBox"))
                    ppdi = &pdev->page_dsc_info;
                else
                    continue;
                if(pvalue->size >= sizeof(scan_buf) - 1)
                    continue;	/* error */
                memcpy(scan_buf, pvalue->data, pvalue->size);
                scan_buf[pvalue->size] = 0;
                if (sscanf(scan_buf, "[%lg %lg %lg %lg]",
                           &box.p.x, &box.p.y, &box.q.x, &box.q.y) != 4
                    )
                    continue;	/* error */
                ppdi->bounding_box = box;
            }
            continue;
        }
    }
    return code;
}
