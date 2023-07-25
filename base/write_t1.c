/* Copyright (C) 2001-2023 Artifex Software, Inc.
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

/*
Functions to serialize a type 1 font as PostScript code that can then be
passed to FreeType via the FAPI FreeType bridge.
Started by Graham Asher, 26th July 2002.
*/

#include <stdio.h>
#include "wrfont.h"
#include "write_t1.h"

/*
Public structures and functions in this file are prefixed with FF_ because they are part of
the FAPI FreeType implementation.
*/

static int
write_word_entry(gs_fapi_font * a_fapi_font, WRF_output * a_output,
                 const char *a_name, int a_index, int a_divisor)
{
    short x;
    int code;

    WRF_wbyte(a_fapi_font->memory, a_output, '/');
    WRF_wstring(a_fapi_font->memory, a_output, a_name);
    WRF_wbyte(a_fapi_font->memory, a_output, ' ');

    /* Get the value and convert it from unsigned to signed by assigning it to a short. */
    code = a_fapi_font->get_word(a_fapi_font, a_index, 0, (unsigned short *)&x);
    if (code < 0)
        return code;

    /* Divide by the divisor to bring it back to font units. */
    x = (short)(x / a_divisor);
    WRF_wint(a_fapi_font->memory, a_output, x);
    WRF_wstring(a_fapi_font->memory, a_output, " def\n");

    return 0;
}

static int
write_array_entry_with_count(gs_fapi_font * a_fapi_font,
                             WRF_output * a_output, const char *a_name,
                             int a_index, int a_count, int a_divisor)
{
    int i, code;;

    if (a_count <= 0)
        return 0;

    WRF_wbyte(a_fapi_font->memory, a_output, '/');
    WRF_wstring(a_fapi_font->memory, a_output, a_name);
    WRF_wstring(a_fapi_font->memory, a_output, " [");
    for (i = 0; i < a_count; i++) {
        /* Get the value and convert it from unsigned to signed by assigning it to a short. */
        short x;
        code = a_fapi_font->get_word(a_fapi_font, a_index, i, (unsigned short *)&x);
        if (code < 0)
            return code;

        /* Divide by the divisor to bring it back to font units. */
        x = (short)(x / a_divisor);
        WRF_wint(a_fapi_font->memory, a_output, x);
        WRF_wbyte(a_fapi_font->memory, a_output, (byte) (i == a_count - 1 ? ']' : ' '));
    }
    WRF_wstring(a_fapi_font->memory, a_output, " def\n");

    return 0;
}

static int
write_array_entry(gs_fapi_font * a_fapi_font, WRF_output * a_output,
                  const char *a_name, int a_index, int a_divisor)
{
    /* NOTE that the feature index must be preceded by the count index for this to work. */
    unsigned short count;
    int code = a_fapi_font->get_word(a_fapi_font, a_index - 1, 0, &count);

    if (code < 0)
        return code;

    return write_array_entry_with_count(a_fapi_font, a_output, a_name, a_index,
                                 count, a_divisor);
}

static int
write_subrs(gs_fapi_font * a_fapi_font, WRF_output * a_output, int raw)
{
    int i;
    unsigned short count;
    int code = a_fapi_font->get_word(a_fapi_font, gs_fapi_font_feature_Subrs_count,
                              0, &count);
    if (code < 0)
        return code;

    if (count > 0) {
        WRF_wstring(a_fapi_font->memory, a_output, "/Subrs ");
        WRF_wint(a_fapi_font->memory, a_output, count);
        WRF_wstring(a_fapi_font->memory, a_output, " array\n");

        for (i = 0; i < count; i++) {
            int length;
            int buffer_size;


            if (raw)
                length = a_fapi_font->get_raw_subr(a_fapi_font, i, 0, 0);
            else
                length = a_fapi_font->get_subr(a_fapi_font, i, 0, 0);
            if (length < 0)
                return length;

            WRF_wstring(a_fapi_font->memory, a_output, "dup ");
            WRF_wint(a_fapi_font->memory, a_output, i);
            WRF_wbyte(a_fapi_font->memory, a_output, ' ');
            WRF_wint(a_fapi_font->memory, a_output, length);
            WRF_wstring(a_fapi_font->memory, a_output, " RD ");

            /* Get the subroutine into the buffer and encrypt it in place. */
            buffer_size = a_output->m_limit - a_output->m_count;
            if (buffer_size >= length) {
                int l2;

                if (raw)
                    l2 = a_fapi_font->get_raw_subr(a_fapi_font, i, a_output->m_pos,
                                              (ushort) length);
                else
                    l2 = a_fapi_font->get_subr(a_fapi_font, i, a_output->m_pos,
                                          (ushort) length);
                if (l2 < 0)
                    return l2;

                WRF_wtext(a_fapi_font->memory, a_output, a_output->m_pos, length);
            } else
                a_output->m_count += length;

            WRF_wstring(a_fapi_font->memory, a_output, " NP\n");
        }

        WRF_wstring(a_fapi_font->memory, a_output, "ND\n");
    }
    return 0;
}

static int
write_charstrings(gs_fapi_font * a_fapi_font, WRF_output * a_output)
{
    int length;
    int buffer_size;
    int i;
    unsigned short count;
    int code = a_fapi_font->get_word(a_fapi_font,
                                         gs_fapi_font_feature_CharStrings_count,
                                         0, &count);
    char NameBuf[256];

    if (code < 0)
        return code;

    if (count > 0) {
        WRF_wstring(a_fapi_font->memory, a_output, "2 index /CharStrings ");
        WRF_wint(a_fapi_font->memory, a_output, count);
        WRF_wstring(a_fapi_font->memory, a_output, " dict dup begin\n");
        for (i = 0; i < count; i++) {
            length =
                a_fapi_font->get_charstring_name(a_fapi_font, i,
                                                 (byte *) & NameBuf, 256);
            if (length < 0)
                return length;

            if (length > 0) {
                length = a_fapi_font->get_charstring(a_fapi_font, i, 0, 0);
                if (length < 0)
                    return length;

                WRF_wbyte(a_fapi_font->memory, a_output, '/');
                WRF_wstring(a_fapi_font->memory, a_output, (const char *)&NameBuf);
                WRF_wbyte(a_fapi_font->memory, a_output, ' ');
                WRF_wint(a_fapi_font->memory, a_output, length);
                WRF_wstring(a_fapi_font->memory, a_output, " RD ");

                /* Get the CharString into the buffer and encrypt it in place. */
                buffer_size = a_output->m_limit - a_output->m_count;
                if (buffer_size >= length) {
                    int l2;
                    l2 = a_fapi_font->get_charstring(a_fapi_font, i, a_output->m_pos,
                                                (ushort) length);
                    if (l2 < 0)
                        return l2;

                    WRF_wtext(a_fapi_font->memory, a_output, a_output->m_pos, length);
                } else
                    a_output->m_count += length;
                WRF_wstring(a_fapi_font->memory, a_output, " ND\n");
            }
        }
        WRF_wstring(a_fapi_font->memory, a_output, " end");
    }
    return 0;
}

static bool
is_MM_font(gs_fapi_font * a_fapi_font)
{
    unsigned short db;
    int code = a_fapi_font->get_word(a_fapi_font, gs_fapi_font_feature_DollarBlend, 0, &db);

    if (code >= 0 && db == 1)
        return true;

    return false;
}

static int
write_private_blend_dictionary(gs_fapi_font * a_fapi_font, WRF_output * a_output)
{
    short x, x1, x2, i, j, acc;
    int code;

    if (is_MM_font(a_fapi_font)) {

        WRF_wstring(a_fapi_font->memory, a_output, "3 index /Blend get /Private get begin\n");
        code = a_fapi_font->get_word(a_fapi_font,
                                  gs_fapi_font_feature_BlendBlueValues_length, 0, (unsigned short *)&x);
        if (code < 0)
            return code;

        if (x > 0) {
            WRF_wstring(a_fapi_font->memory, a_output, "/BlueValues [");
            acc = 0;
            for (i = 0; i < x; i++) {
                WRF_wstring(a_fapi_font->memory, a_output, " [");
                code = a_fapi_font->get_word(a_fapi_font,
                                      gs_fapi_font_feature_BlendBlueValues_count, i, (unsigned short *)&x1);
                if (code < 0)
                    return code;

                for (j = 0; j < x1; j++) {
                    code = a_fapi_font->get_word(a_fapi_font,
                                      gs_fapi_font_feature_BlendBlueValues, acc++, (unsigned short *)&x2);
                    if (code < 0)
                        return code;

                    WRF_wint(a_fapi_font->memory, a_output, x2);
                    WRF_wbyte(a_fapi_font->memory, a_output, (byte)' ');
                }
                WRF_wstring(a_fapi_font->memory, a_output, " ]");
            }
            WRF_wstring(a_fapi_font->memory, a_output, " ]\n");
        }

        code = a_fapi_font->get_word(a_fapi_font,
                                  gs_fapi_font_feature_BlendOtherBlues_length, 0, (unsigned short *)&x);
        if (code < 0)
            return code;

        if (x > 0) {
            WRF_wstring(a_fapi_font->memory, a_output, "/OtherBlues [");
            acc = 0;
            for (i = 0; i < x; i++) {
                WRF_wstring(a_fapi_font->memory, a_output, " [");
                code = a_fapi_font->get_word(a_fapi_font,
                                      gs_fapi_font_feature_BlendOtherBlues_count, i, (unsigned short *)&x1);
                if (code < 0)
                    return code;

                for (j = 0; j < x1; j++) {
                    code = a_fapi_font->get_word(a_fapi_font,
                                      gs_fapi_font_feature_BlendOtherBlues, acc++, (unsigned short *)&x2);
                    if (code < 0)
                        return code;

                    WRF_wint(a_fapi_font->memory, a_output, x2);
                    WRF_wbyte(a_fapi_font->memory, a_output, (byte)' ');
                }
                WRF_wstring(a_fapi_font->memory, a_output, " ]");
            }
            WRF_wstring(a_fapi_font->memory, a_output, " ]\n");
        }
        code = a_fapi_font->get_word(a_fapi_font,
                                  gs_fapi_font_feature_BlendBlueScale_count, 0, (unsigned short *)&x);
        if (code < 0)
            return 0;

        if (x > 0) {
            float v;
            WRF_wstring(a_fapi_font->memory, a_output, "/BlueScale [");
            for (i =0; i < x; i++) {
                code = a_fapi_font->get_float(a_fapi_font,
                                      gs_fapi_font_feature_BlendBlueScale, i, &v);
                if (code < 0)
                    return code;

                WRF_wfloat(a_fapi_font->memory, a_output, v);
                WRF_wbyte(a_fapi_font->memory, a_output, (byte)' ');
            }
            WRF_wstring(a_fapi_font->memory, a_output, " ]\n");
        }

        code = a_fapi_font->get_word(a_fapi_font,
                                  gs_fapi_font_feature_BlendBlueShift_count, 0, (unsigned short *)&x);
        if (code < 0)
            return code;

        if (x > 0) {
            WRF_wstring(a_fapi_font->memory, a_output, "/BlueShift [");
            for (i =0; i < x; i++) {
                code = a_fapi_font->get_word(a_fapi_font,
                                      gs_fapi_font_feature_BlendBlueShift, i, (unsigned short *)&x2);
                if (code < 0)
                    return code;

                WRF_wint(a_fapi_font->memory, a_output, x2);
                WRF_wbyte(a_fapi_font->memory, a_output, (byte)' ');
            }
            WRF_wstring(a_fapi_font->memory, a_output, " ]\n");
        }

        code = a_fapi_font->get_word(a_fapi_font,
                                  gs_fapi_font_feature_BlendBlueFuzz_count, 0, (unsigned short *)&x);
        if (code < 0)
            return code;

        if (x > 0) {
            WRF_wstring(a_fapi_font->memory, a_output, "/BlueFuzz [");
            for (i =0; i < x; i++) {
                code = a_fapi_font->get_word(a_fapi_font,
                                      gs_fapi_font_feature_BlendBlueFuzz, i, (unsigned short *)&x2);
                if (code < 0)
                    return code;

                WRF_wint(a_fapi_font->memory, a_output, x2);
                WRF_wbyte(a_fapi_font->memory, a_output, (byte)' ');
            }
            WRF_wstring(a_fapi_font->memory, a_output, " ]\n");
        }

        code = a_fapi_font->get_word(a_fapi_font,
                                  gs_fapi_font_feature_BlendForceBold_count, 0, (unsigned short *)&x);
        if (code < 0)
            return code;

        if (x > 0) {
            WRF_wstring(a_fapi_font->memory, a_output, "/ForceBold [");
            for (i =0; i < x; i++) {
                code = a_fapi_font->get_word(a_fapi_font,
                                      gs_fapi_font_feature_BlendForceBold, i, (unsigned short *)&x2);
                if (code < 0)
                    return code;

                WRF_wstring(a_fapi_font->memory, a_output, x2 ? "/true" : "/false");
                WRF_wbyte(a_fapi_font->memory, a_output, (byte)' ');
            }
            WRF_wstring(a_fapi_font->memory, a_output, " ]\n");
        }

        code = a_fapi_font->get_word(a_fapi_font,
                                  gs_fapi_font_feature_BlendStdHW_length, 0, (unsigned short *)&x);
        if (code < 0)
            return code;

        if (x > 0) {
            WRF_wstring(a_fapi_font->memory, a_output, "/StdHW [");
            acc = 0;
            for (i = 0; i < x; i++) {
                WRF_wstring(a_fapi_font->memory, a_output, " [");
                code = a_fapi_font->get_word(a_fapi_font,
                                      gs_fapi_font_feature_BlendStdHW_count, i, (unsigned short *)&x1);
                if (code < 0)
                    return code;

                for (j = 0; j < x1; j++) {
                    code = a_fapi_font->get_word(a_fapi_font,
                                      gs_fapi_font_feature_BlendStdHW, acc++, (unsigned short *)&x2);
                    if (code < 0)
                        return code;

                    WRF_wint(a_fapi_font->memory, a_output, x2);
                    WRF_wbyte(a_fapi_font->memory, a_output, (byte)' ');
                }
                WRF_wstring(a_fapi_font->memory, a_output, " ]");
            }
            WRF_wstring(a_fapi_font->memory, a_output, " ]\n");
        }

        code = a_fapi_font->get_word(a_fapi_font,
                                  gs_fapi_font_feature_BlendStdVW_length, 0, (unsigned short *)&x);
        if (code < 0)
            return code;

        if (x > 0) {
            WRF_wstring(a_fapi_font->memory, a_output, "/StdVW [");
            acc = 0;
            for (i = 0; i < x; i++) {
                WRF_wstring(a_fapi_font->memory, a_output, " [");
                code = a_fapi_font->get_word(a_fapi_font,
                                      gs_fapi_font_feature_BlendStdVW_count, i, (unsigned short *)&x1);
                if (code < 0)
                    return code;

                for (j = 0; j < x1; j++) {
                    code = a_fapi_font->get_word(a_fapi_font,
                                      gs_fapi_font_feature_BlendStdVW, acc++, (unsigned short *)&x2);
                    if (code < 0)
                        return code;

                    WRF_wint(a_fapi_font->memory, a_output, x2);
                    WRF_wbyte(a_fapi_font->memory, a_output, (byte)' ');
                }
                WRF_wstring(a_fapi_font->memory, a_output, " ]");
            }
            WRF_wstring(a_fapi_font->memory, a_output, " ]\n");
        }

        code = a_fapi_font->get_word(a_fapi_font,
                                  gs_fapi_font_feature_BlendStemSnapH_length, 0, (unsigned short *)&x);
        if (code < 0)
            return code;

        if (x > 0) {
            WRF_wstring(a_fapi_font->memory, a_output, "/StemSnapH [");
            acc = 0;
            for (i = 0; i < x; i++) {
                WRF_wstring(a_fapi_font->memory, a_output, " [");
                code = a_fapi_font->get_word(a_fapi_font,
                                      gs_fapi_font_feature_BlendStemSnapH_count, i, (unsigned short *)&x1);
                if (code < 0)
                    return code;

                for (j = 0; j < x1; j++) {
                    code = a_fapi_font->get_word(a_fapi_font,
                                      gs_fapi_font_feature_BlendStemSnapH, acc++, (unsigned short *)&x2);
                    if (code < 0)
                        return code;

                    WRF_wint(a_fapi_font->memory, a_output, x2);
                    WRF_wbyte(a_fapi_font->memory, a_output, (byte)' ');
                }
                WRF_wstring(a_fapi_font->memory, a_output, " ]");
            }
            WRF_wstring(a_fapi_font->memory, a_output, " ]\n");
        }

        code = a_fapi_font->get_word(a_fapi_font,
                                  gs_fapi_font_feature_BlendStemSnapV_length, 0, (unsigned short *)&x);
        if (code < 0)
            return code;

        if (x > 0) {
            WRF_wstring(a_fapi_font->memory, a_output, "/StemSnapV [");
            acc = 0;
            for (i = 0; i < x; i++) {
                WRF_wstring(a_fapi_font->memory, a_output, " [");
                code = a_fapi_font->get_word(a_fapi_font,
                                      gs_fapi_font_feature_BlendStemSnapV_count, i, (unsigned short *)&x1);
                if (code < 0)
                    return code;

                for (j = 0; j < x1; j++) {
                    code = a_fapi_font->get_word(a_fapi_font,
                                      gs_fapi_font_feature_BlendStemSnapV, acc++, (unsigned short *)&x2);
                    if (code < 0)
                        return code;

                    WRF_wint(a_fapi_font->memory, a_output, x2);
                    WRF_wbyte(a_fapi_font->memory, a_output, (byte)' ');
                }
                WRF_wstring(a_fapi_font->memory, a_output, " ]");
            }
            WRF_wstring(a_fapi_font->memory, a_output, " ]\n");
        }

        WRF_wstring(a_fapi_font->memory, a_output, "end\n");
    }
    return 0;
}

static int
write_private_dictionary(gs_fapi_font * a_fapi_font, WRF_output * a_output,
                         int Write_CharStrings)
{
    int code;
    unsigned long ulval;
    a_output->m_encrypt = true;

    /* Write 4 bytes that must encrypt to at least one character that cannot be a valid hexadecimal character. */
    WRF_wstring(a_fapi_font->memory, a_output, "XXXX");

    /*+ to do: correct size of dictionary from 8. */
    WRF_wstring(a_fapi_font->memory, a_output, "dup /Private 8 dict dup begin\n");

    WRF_wstring(a_fapi_font->memory, a_output, "/MinFeature {16 16} def\n");
    WRF_wstring(a_fapi_font->memory, a_output, "/password 5839 def\n");
    if (Write_CharStrings) {
        code = write_word_entry(a_fapi_font, a_output, "lenIV", gs_fapi_font_feature_lenIV, 1);
        if (code < 0)
            return code;
    }
    else {
        WRF_wstring(a_fapi_font->memory, a_output, "/lenIV -1 def\n");       /* indicate that /subrs are not encoded. */
    }
    code = write_word_entry(a_fapi_font, a_output, "BlueFuzz", gs_fapi_font_feature_BlueFuzz, 16);
    if (code < 0)
        return code;

    WRF_wstring(a_fapi_font->memory, a_output, "/BlueScale ");
    code = a_fapi_font->get_long(a_fapi_font,gs_fapi_font_feature_BlueScale, 0, &ulval);
    if (code < 0)
        return code;

    WRF_wfloat(a_fapi_font->memory, a_output, (float)ulval/65536.0);

    WRF_wstring(a_fapi_font->memory, a_output, " def\n");

    code = write_word_entry(a_fapi_font, a_output, "BlueShift", gs_fapi_font_feature_BlueShift, 16);
    if (code < 0)
        return code;

    code = write_array_entry(a_fapi_font, a_output, "BlueValues",
                      gs_fapi_font_feature_BlueValues, 16);
    if (code < 0)
        return code;

    code = write_array_entry(a_fapi_font, a_output, "OtherBlues",
                      gs_fapi_font_feature_OtherBlues, 16);
    if (code < 0)
        return code;

    code = write_array_entry(a_fapi_font, a_output, "FamilyBlues",
                      gs_fapi_font_feature_FamilyBlues, 16);
    if (code < 0)
        return code;

    code = write_array_entry(a_fapi_font, a_output, "FamilyOtherBlues",
                      gs_fapi_font_feature_FamilyOtherBlues, 16);
    if (code < 0)
        return code;

    code = write_word_entry(a_fapi_font, a_output, "ForceBold", gs_fapi_font_feature_ForceBold, 1);
    if (code < 0)
        return code;

    code = write_array_entry_with_count(a_fapi_font, a_output, "StdHW",
                                 gs_fapi_font_feature_StdHW, 1, 16);
    if (code < 0)
        return code;

    code = write_array_entry_with_count(a_fapi_font, a_output, "StdVW",
                                 gs_fapi_font_feature_StdVW, 1, 16);
    if (code < 0)
        return code;

    code = write_array_entry(a_fapi_font, a_output, "StemSnapH",
                      gs_fapi_font_feature_StemSnapH, 16);
    if (code < 0)
        return code;

    code = write_array_entry(a_fapi_font, a_output, "StemSnapV",
                      gs_fapi_font_feature_StemSnapV, 16);
    if (code < 0)
        return code;

    code = write_private_blend_dictionary(a_fapi_font, a_output);
    if (code < 0)
        return code;

    if (Write_CharStrings)
        code = write_subrs(a_fapi_font, a_output, 1);
    else
        code = write_subrs(a_fapi_font, a_output, 0);
    if (code < 0)
        return code;

    if (Write_CharStrings)
        code = write_charstrings(a_fapi_font, a_output);

    return code;
}

static int
write_main_dictionary(gs_fapi_font * a_fapi_font, WRF_output * a_output, int Write_CharStrings)
{
    int i, code;
    float fval;

    WRF_wstring(a_fapi_font->memory, a_output, "5 dict begin\n");

    WRF_wstring(a_fapi_font->memory, a_output, "/FontType 1 def\n");

    WRF_wstring(a_fapi_font->memory, a_output, "/FontMatrix [");
    for (i = 0; i < 6; i++) {
        code = a_fapi_font->get_float(a_fapi_font, gs_fapi_font_feature_FontMatrix, i, &fval);
        if (code < 0)
            return code;

        WRF_wfloat(a_fapi_font->memory, a_output, fval);
        WRF_wbyte(a_fapi_font->memory, a_output, (byte) (i == 5 ? ']' : ' '));
    }
    WRF_wbyte(a_fapi_font->memory, a_output, '\n');
    /* For now, specify standard encoding - I think GS will pass glyph indices so doesn't matter. */
    WRF_wstring(a_fapi_font->memory, a_output, "/Encoding StandardEncoding def\n");

    WRF_wstring(a_fapi_font->memory, a_output, "/FontBBox {");
    for (i = 0; i < 4; i++) {
        short x;
        code = a_fapi_font->get_word(a_fapi_font, gs_fapi_font_feature_FontBBox,
                                  i, (unsigned short *)&x);
        if (code < 0)
            return code;

        WRF_wint(a_fapi_font->memory, a_output, x);
        WRF_wbyte(a_fapi_font->memory, a_output, (byte) (i == 3 ? '}' : ' '));
    }
    WRF_wbyte(a_fapi_font->memory, a_output, '\n');
    if (is_MM_font(a_fapi_font)) {
        short x, x2;
        float x1;
        uint i, j, entries;
        char Buffer[255];

        entries = 0;
        code = a_fapi_font->get_word(a_fapi_font,
                                  gs_fapi_font_feature_BlendAxisTypes_count,
                                  0, (unsigned short *)&x);
        if (code < 0)
            return code;

        if (x)
            entries++;

        code = a_fapi_font->get_word(a_fapi_font,
                                  gs_fapi_font_feature_BlendDesignPositionsArrays_count,
                                  0, (unsigned short *)&x);
        if (code < 0)
            return code;

        if (x)
            entries++;
        code = a_fapi_font->get_word(a_fapi_font,
                                  gs_fapi_font_feature_BlendDesignMapArrays_count,
                                  0, (unsigned short *)&x);
        if (code < 0)
            return code;
        if (x)

            entries++;

        gs_snprintf(Buffer, sizeof(Buffer), "/FontInfo %d dict dup begin\n", entries);
        WRF_wstring(a_fapi_font->memory, a_output, Buffer);
        code = a_fapi_font->get_word(a_fapi_font,
                                  gs_fapi_font_feature_BlendAxisTypes_count,
                                  0, (unsigned short *)&x);
        if (code < 0)
            return code;

        if (x) {
            WRF_wstring(a_fapi_font->memory, a_output, "/BlendAxisTypes [");
            for (i = 0; i < x; i++) {
                WRF_wstring(a_fapi_font->memory, a_output, " /");
                code = a_fapi_font->get_name(a_fapi_font,
                                      gs_fapi_font_feature_BlendAxisTypes, i,
                                      (char *)&Buffer, 255);
                if (code < 0)
                    return code;

                WRF_wstring(a_fapi_font->memory, a_output, Buffer);
            }
            WRF_wstring(a_fapi_font->memory, a_output, "] def\n");
        }
        code = a_fapi_font->get_word(a_fapi_font,
                                  gs_fapi_font_feature_BlendDesignPositionsArrays_count,
                                  0, (unsigned short *)&x);
        if (code < 0)
            return code;

        if (x) {
            WRF_wstring(a_fapi_font->memory, a_output, "/BlendDesignPositions [");
            code = a_fapi_font->get_word(a_fapi_font,
                                       gs_fapi_font_feature_BlendAxisTypes_count,
                                       0, (unsigned short *)&x2);
            if (code < 0)
                return code;

            for (i = 0; i < x; i++) {
                WRF_wstring(a_fapi_font->memory, a_output, "[");
                for (j = 0; j < x2; j++) {
                    code = a_fapi_font->get_float(a_fapi_font,
                                                gs_fapi_font_feature_BlendDesignPositionsArrayValue,
                                                i * 8 + j, &x1);
                    if (code < 0)
                        return code;

                    gs_snprintf(Buffer, sizeof(Buffer), "%f ", x1);
                    WRF_wstring(a_fapi_font->memory, a_output, Buffer);
                }
                WRF_wstring(a_fapi_font->memory, a_output, "]");
            }
            WRF_wstring(a_fapi_font->memory, a_output, "] def\n");
        }
        code = a_fapi_font->get_word(a_fapi_font,
                                  gs_fapi_font_feature_BlendDesignMapArrays_count,
                                  0, (unsigned short *)&x);
        if (code < 0)
            return code;
        if (x) {
            WRF_wstring(a_fapi_font->memory, a_output, "/BlendDesignMap [");
            for (i = 0; i < x; i++) {
                code = a_fapi_font->get_word(a_fapi_font,
                                           gs_fapi_font_feature_BlendDesignMapSubArrays_count,
                                           i, (unsigned short *)&x2);
                if (code < 0)
                    return code;

                WRF_wstring(a_fapi_font->memory, a_output, "[");
                for (j = 0; j < x2; j++) {
                    WRF_wstring(a_fapi_font->memory, a_output, "[");
                    code = a_fapi_font->get_float(a_fapi_font,
                                                gs_fapi_font_feature_BlendDesignPositionsArrayValue,
                                                i * 64 + j * 64, &x1);
                    if (code < 0)
                        return code;

                    gs_snprintf(Buffer, sizeof(Buffer), "%f ", x1);
                    WRF_wstring(a_fapi_font->memory, a_output, Buffer);
                    code = a_fapi_font->get_float(a_fapi_font,
                                                gs_fapi_font_feature_BlendDesignPositionsArrayValue,
                                                i * 64 + j * 64 + 1, &x1);
                    if (code < 0)
                        return code;
                    gs_snprintf(Buffer, sizeof(Buffer), "%f ", x1);
                    WRF_wstring(a_fapi_font->memory, a_output, Buffer);
                    WRF_wstring(a_fapi_font->memory, a_output, "]");
                }
                WRF_wstring(a_fapi_font->memory, a_output, "]");
            }
            WRF_wstring(a_fapi_font->memory, a_output, "] def\n");
        }
        WRF_wstring(a_fapi_font->memory, a_output, "end readonly def\n");

        /* Previously we tried to write $Blend twice - the "real" one from the font,
         * and the boiler plate one below.
         * For now, I assume there was a good reason for including the second, but it may
         * be because the "get_proc" method below was missing the code to handle PS name
         * objects.
         */
        code = a_fapi_font->get_word(a_fapi_font,
                                   gs_fapi_font_feature_DollarBlend_length,
                                   0, (unsigned short *)&x);
        if (code < 0)
            return code;

        if (x > 0) {
            int len;
            WRF_wstring(a_fapi_font->memory, a_output, "/$Blend {");

            if (a_output->m_count)
                a_output->m_count += x;
            len = a_fapi_font->get_proc(a_fapi_font,
                                      gs_fapi_font_feature_DollarBlend, 0,
                                      (char *)a_output->m_pos);
            if (a_output->m_pos && len > 0)
                a_output->m_pos += len;
            WRF_wstring(a_fapi_font->memory, a_output, "} def\n");
        } else {
            WRF_wstring(a_fapi_font->memory, a_output,
                        "/$Blend {0.1 mul exch 0.45 mul add exch 0.17 mul add add} def\n");
        }
        WRF_wstring(a_fapi_font->memory, a_output, "/WeightVector [");
        code = a_fapi_font->get_word(a_fapi_font,
                                  gs_fapi_font_feature_WeightVector_count, 0, (unsigned short *)&x);
        if (code < 0)
            return code;

        for (i = 0; i < x; i++) {
            code = a_fapi_font->get_float(a_fapi_font,
                                        gs_fapi_font_feature_WeightVector, i, &x1);
            if (code < 0)
                return code;

            gs_snprintf(Buffer, sizeof(Buffer), "%f ", x1);
            WRF_wstring(a_fapi_font->memory, a_output, Buffer);
        }
        WRF_wstring(a_fapi_font->memory, a_output, "] def\n");

        WRF_wstring(a_fapi_font->memory, a_output, "/Blend 3 dict dup begin\n");
        WRF_wstring(a_fapi_font->memory, a_output, "/FontBBox {");
        code = a_fapi_font->get_word(a_fapi_font,
                                 gs_fapi_font_feature_BlendFontBBox_length , 0, (unsigned short *)&x);
        if (code < 0)
            return code;

        for (i = 0; i < x; i++) {
            int j;
            WRF_wstring(a_fapi_font->memory, a_output, " {");
            for (j = 0; j < 4; j++) {
                code = a_fapi_font->get_word(a_fapi_font,
                                  gs_fapi_font_feature_BlendFontBBox,
                                  j + (i * 4), (unsigned short *)&x2);
                if (code < 0)
                    return code;

                WRF_wint(a_fapi_font->memory, a_output, x2);
                WRF_wbyte(a_fapi_font->memory, a_output, (byte)' ');
            }
            WRF_wstring(a_fapi_font->memory, a_output, "}");
        }
        WRF_wstring(a_fapi_font->memory, a_output, " } def\n");
        WRF_wstring(a_fapi_font->memory, a_output, "/Private 14 dict def\n");
        WRF_wstring(a_fapi_font->memory, a_output, "end def\n");
#undef TEMP_BUF_LEN
    }
    WRF_wstring(a_fapi_font->memory, a_output, "currentdict end\ncurrentfile eexec\n");

    return write_private_dictionary(a_fapi_font, a_output, Write_CharStrings);
}

/**
Write a Type 1 font in textual format and return its length in bytes.
If a_buffer_size is less than the total length, only a_buffer_size bytes are written, but the total
length is returned correctly.

The PostScript is non-standard. The main dictionary contains no /Charstrings dictionary. This
is supplied to FreeType using the incremental interface, There is also no /PaintType entry. This is required
by PostScript but FreeType doesn't use it.
*/
long
gs_fapi_serialize_type1_font(gs_fapi_font * a_fapi_font,
                             unsigned char *a_buffer, long a_buffer_size)
{
    int code;
    WRF_output output;

    WRF_init(&output, a_buffer, a_buffer_size);

    /* Leading comment identifying a Type 1 font. */
    WRF_wstring(a_fapi_font->memory, &output, "%!PS-AdobeFont-1\n");

    code = write_main_dictionary(a_fapi_font, &output, 0);
    if (code < 0)
        return (long)code;
#if 0
    {
        extern FILE *stdout;
        if (a_buffer && a_buffer_size >= output.m_count) {
            fwrite(a_buffer, 1, output.m_count, stdout);
        }
    }
#endif

    return output.m_count;
}

long
gs_fapi_serialize_type1_font_complete(gs_fapi_font * a_fapi_font,
                                      unsigned char *a_buffer,
                                      long a_buffer_size)
{
    int code;
    WRF_output output;

    WRF_init(&output, a_buffer, a_buffer_size);

    /* Leading comment identifying a Type 1 font. */
    WRF_wstring(a_fapi_font->memory, &output, "%!PS-AdobeFont-1\n");

    code = write_main_dictionary(a_fapi_font, &output, 1);
    if (code < 0)
        return (long)code;

    return output.m_count;
}
