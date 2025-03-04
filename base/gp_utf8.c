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


#include "gp_utf8.h"

static int
decode_utf8(const char **inp, unsigned int leading_byte)
{
    const char *in = *inp;
    unsigned char c;
    unsigned int codepoint;

    if (leading_byte < 0x80) {
        codepoint = leading_byte;
    } else if ((leading_byte & 0xE0) == 0xC0) {
        codepoint = leading_byte & 0x1F;
        /* Any encoded value that fails to use bit 1 upwards of this
         * byte would have been better encoded in a short form. */
        if (codepoint < 2)
            goto fail_overlong;
        c = (unsigned char)*in++;
        if ((c & 0xC0) != 0x80)
            goto fail;
        codepoint = (codepoint<<6) | (c & 0x3f);
    } else if ((leading_byte & 0xF0) == 0xE0) {
        codepoint = leading_byte & 0xF;
        /* Any encoding that does not use any of the data bits in this
         * byte would have been better encoded in a shorter form. */
        if (codepoint == 0)
            goto fail_overlong;
        c = (unsigned char)*in++;
        if ((c & 0xC0) != 0x80)
            goto fail;
        codepoint = (codepoint<<6) | (c & 0x3f);
        c = (unsigned char)*in++;
        if ((c & 0xC0) != 0x80)
            goto fail;
        codepoint = (codepoint<<6) | (c & 0x3f);
    } else if ((leading_byte & 0xF8) == 0xF0) {
        codepoint = leading_byte & 0x7;
        /* Any encoding that does not use any of the data bits in this
         * byte would have been better encoded in a shorter form. */
        if (codepoint == 0)
            goto fail_overlong;
        c = (unsigned char)*in++;
        if ((c & 0xC0) != 0x80)
            goto fail;
        codepoint = (codepoint<<6) | (c & 0x3f);
        c = (unsigned char)*in++;
        if ((c & 0xC0) != 0x80)
            goto fail;
        codepoint = (codepoint<<6) | (c & 0x3f);
        c = (unsigned char)*in++;
        if ((c & 0xC0) != 0x80)
            goto fail;
        codepoint = (codepoint<<6) | (c & 0x3f);
        /* Check for UTF-16 surrogates which are invalid in UTF-8 */
        if (codepoint >= 0xD800 && codepoint <= 0xDFFF)
            goto fail_overlong;
        /* Codepoints 0 to 0xFFFF (other than the surrogate pair
         * ranges) can be coded for trivially. We can code for
         * codepoints up to 0x10FFFF using surrogate pairs.
         * Anything higher than that is forbidden. */
        if (codepoint > 0x10FFFF)
            goto fail_overlong;
    }
    else
    {
        /* Longer UTF-8 encodings give more than 21 bits of data.
         * The longest we can encode in utf-16 (even using surrogate
         * pairs is 20 bits, so all these should fail. */
        if (0)
        {
            /* If we fail, unread the last one, and return the unicode replacement char. */
fail:
           in--;
        }
fail_overlong:
       /* If we jump to here it's because we've detected an 'overlong' encoding.
        * While this seems harmless, it's actually illegal, for good reason;
        * this is typically an attempt to sneak stuff past security checks, like
        * "../" in paths. Fail this. */
       codepoint = 0xfffd;
    }
    *inp = in;

    return codepoint;
}

int gp_utf8_to_uint16(unsigned short *out, const char *in)
{
    unsigned int i;
    unsigned int len = 1;

    if (out) {
        while ((i = *(unsigned char *)in++) != 0) {
            /* Decode UTF-8 */
            i = decode_utf8(&in, i);

            /* Encode, allowing for surrogates. */
            if (i >= 0x10000 && i <= 0x10ffff)
            {
                i -= 0x10000;
                *out++ = 0xd800 + (i>>10);
                *out++ = 0xdc00 + (i & 0x3ff);
                len++;
            }
            else if (i > 0x10000)
            {
                return -1;
            }
            else
                *out++ = (unsigned short)i;
            len++;
        }
        *out = 0;
    } else {
        while ((i = *(unsigned char *)in++) != 0) {
            /* Decode UTF-8 */
            i = decode_utf8(&in, i);

            /* Encode, allowing for surrogates. */
            if (i >= 0x10000 && i <= 0x10ffff)
                len++;
            else if (i > 0x10000)
                return -1;
            len++;
        }
    }
    return len;
}

int gp_uint16_to_utf8(char *out, const unsigned short *in)
{
    unsigned int i;
    unsigned int len = 1;

    if (out) {
        while ((i = (unsigned int)*in++) != 0) {
            /* Decode surrogates */
            if (i >= 0xD800 && i <= 0xDBFF)
            {
                /* High surrogate. Must be followed by a low surrogate, or this is a failure. */
                int hi = i & 0x3ff;
                int j = (unsigned int)*in++;
                if (j == 0 || (j <= 0xDC00 || j >= 0xDFFF))
                {
                    /* Failure! Unicode replacement char! */
                    in--;
                    i = 0xfffd;
                } else {
                    /* Decode surrogates */
                    i = 0x10000 + (hi<<10) + (j & 0x3ff);
                }
            } else if (i >= 0xDC00 && i <= 0xDFFF)
            {
                /* Lone low surrogate. Failure. Unicode replacement char. */
                i = 0xfffd;
            }

            /* Encode output */
            if (i < 0x80) {
                *out++ = (char)i;
                len++;
            } else if (i < 0x800) {
                *out++ = 0xC0 | ( i>> 6        );
                *out++ = 0x80 | ( i      & 0x3F);
                len+=2;
            } else if (i < 0x10000) {
                *out++ = 0xE0 | ( i>>12        );
                *out++ = 0x80 | ((i>> 6) & 0x3F);
                *out++ = 0x80 | ( i      & 0x3F);
                len+=3;
            } else {
                *out++ = 0xF0 | ( i>>18        );
                *out++ = 0x80 | ((i>>12) & 0x3F);
                *out++ = 0x80 | ((i>> 6) & 0x3F);
                *out++ = 0x80 | ( i      & 0x3F);
                len+=4;
            }
        }
        *out = 0;
    } else {
        while ((i = (unsigned int)*in++) != 0) {
            /* Decode surrogates */
            if (i >= 0xD800 && i <= 0xDBFF)
            {
                /* High surrogate. Must be followed by a low surrogate, or this is a failure. */
                int hi = i & 0x3ff;
                int j = (unsigned int)*in++;
                if (j == 0 || (j <= 0xDC00 || j >= 0xDFFF))
                {
                    /* Failure! Unicode replacement char! */
                    in--;
                    i = 0xfffd;
                } else {
                    /* Decode surrogates */
                    i = 0x10000 + (hi<<10) + (j & 0x3ff);
                }
            } else if (i >= 0xDC00 && i <= 0xDFFF)
            {
                /* Lone low surrogate. Failure. Unicode replacement char. */
                i = 0xfffd;
            }

            if (i < 0x80) {
                len++;
            } else if (i < 0x800) {
                len += 2;
            } else if (i < 0x10000) {
                len += 3;
            } else {
                len += 4;
            }
        }
    }
    return len;
}
