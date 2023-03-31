/* Copyright (C) 2020-2023 Artifex Software, Inc.
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

/* platform-specific string to UTF8 conversion routines */

/* This code is in its own file because it needs to be compiled with
 * a different set of complier flags to normal when built for MS Windows.
 * We can't use the /Za flag with a file whcih includes windows.h, disabling
 * the language extensions breaks the compilation. See pdf/pdf.mak for the
 * build-specifics.
 */

#include "ghostpdf.h"
#include "pdf_utf8.h"
#include "pdf_types.h"
#include "pdf_stack.h"

#ifdef HAVE_LIBIDN
#  include <stringprep.h>
#  include <errno.h>
/* Convert a string from the current locale's character set to UTF-8.
 * <string> .locale_to_utf8 <string> */
int
locale_to_utf8(pdf_context *ctx, pdf_string *input, pdf_string **output)
{
    char *out = NULL;
    int code;

    out = stringprep_locale_to_utf8((const char *)input->data);
    if (out == NULL) {
        /* This function is intended to be used on strings whose
         * character set is unknown, so it's not an error if the
         * input contains invalid characters.  Just return the input
         * string unchanged.
         *
         * Sadly, EINVAL from stringprep_locale_to_utf8 can mean
         * either an invalid character set conversion (which we care
         * about), or an incomplete input string (which we don't).
         * For now, we ignore EINVAL; the right solution is probably
         * to not use stringprep_locale_to_utf8, and just call iconv
         * by hand. */
        if (errno == EILSEQ || errno == EINVAL)
            return 0;

        /* Other errors (like ENFILE) are real errors, which we
         * want to return to the user. */
        return_error(gs_error_ioerror);
    }

    code = pdfi_object_alloc(ctx, PDF_STRING, strlen(out), (pdf_obj **)output);
    if (code < 0)
        return code;
    pdfi_countup(*output);
    memcpy((*output)->data, out, strlen(out));

    free(out);
    return 0;
}
#else
#ifdef _MSC_VER
#include "windows_.h"
/* Convert a string from the current locale's character set to UTF-8.
 * Unfortunately, "current locale" can mean a few different things on
 * Windows -- we use the default ANSI code page, which does the right
 * thing for command-line arguments (like "-sPDFPassword=foo") and
 * for strings typed as input to gswin32.exe.  It doesn't work for
 * strings typed as input to gswin32c.exe, which are normally in the
 * default OEM code page instead.
 * <string> .locale_to_utf8 <string> */
int
locale_to_utf8(pdf_context *ctx, pdf_string *input, pdf_string **output)
{
#define LOCALE_TO_UTF8_BUFFER_SIZE 1024
    WCHAR wide_buffer[LOCALE_TO_UTF8_BUFFER_SIZE];
    char utf8_buffer[LOCALE_TO_UTF8_BUFFER_SIZE];
    int code, BytesWritten;

    *output = NULL;

    BytesWritten = MultiByteToWideChar(CP_ACP, 0, input->data, input->length,
        wide_buffer, LOCALE_TO_UTF8_BUFFER_SIZE);
    if (BytesWritten == 0)
        return_error(gs_error_ioerror);

    BytesWritten = WideCharToMultiByte(CP_UTF8, 0, wide_buffer, BytesWritten,
        utf8_buffer, LOCALE_TO_UTF8_BUFFER_SIZE, NULL, NULL);
    if (BytesWritten == 0)
        return_error(gs_error_ioerror);

    code = pdfi_object_alloc(ctx, PDF_STRING, BytesWritten, (pdf_obj **)output);
    if (code < 0)
        return code;
    pdfi_countup(*output);
    memcpy((*output)->data, utf8_buffer, BytesWritten);

    return 0;
#undef LOCALE_TO_UTF8_BUFFER_SIZE
}
#else
/* We have no known method to create a UTF-8 string. Just copy the input and pretend.
 */
int
locale_to_utf8(pdf_context *ctx, pdf_string *input, pdf_string **output)
{
    int code = 0;

    code = pdfi_object_alloc(ctx, PDF_STRING, input->length, (pdf_obj **)output);
    if (code < 0)
        return code;
    pdfi_countup(*output);
    memcpy((*output)->data, input->data, input->length);

    return 0;
}
#endif /* _WINDOWS_ */
#endif /* HAVE_LIBIDN */
