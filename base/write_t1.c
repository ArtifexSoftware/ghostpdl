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

static void
write_word_entry(gs_fapi_font * a_fapi_font, WRF_output * a_output,
                 const char *a_name, int a_index, int a_divisor)
{
    short x;

    WRF_wbyte(a_fapi_font->memory, a_output, '/');
    WRF_wstring(a_fapi_font->memory, a_output, a_name);
    WRF_wbyte(a_fapi_font->memory, a_output, ' ');
    /* Get the value and convert it from unsigned to signed by assigning it to a short. */
    x = a_fapi_font->get_word(a_fapi_font, a_index, 0);
    /* Divide by the divisor to bring it back to font units. */
    x = (short)(x / a_divisor);
    WRF_wint(a_fapi_font->memory, a_output, x);
    WRF_wstring(a_fapi_font->memory, a_output, " def\n");
}

static void
write_array_entry_with_count(gs_fapi_font * a_fapi_font,
                             WRF_output * a_output, const char *a_name,
                             int a_index, int a_count, int a_divisor)
{
    int i;

    if (a_count <= 0)
        return;

    WRF_wbyte(a_fapi_font->memory, a_output, '/');
    WRF_wstring(a_fapi_font->memory, a_output, a_name);
    WRF_wstring(a_fapi_font->memory, a_output, " [");
    for (i = 0; i < a_count; i++) {
        /* Get the value and convert it from unsigned to signed by assigning it to a short. */
        short x = a_fapi_font->get_word(a_fapi_font, a_index, i);

        /* Divide by the divisor to bring it back to font units. */
        x = (short)(x / a_divisor);
        WRF_wint(a_fapi_font->memory, a_output, x);
        WRF_wbyte(a_fapi_font->memory, a_output, (byte) (i == a_count - 1 ? ']' : ' '));
    }
    WRF_wstring(a_fapi_font->memory, a_output, " def\n");
}

static void
write_array_entry(gs_fapi_font * a_fapi_font, WRF_output * a_output,
                  const char *a_name, int a_index, int a_divisor)
{
    /* NOTE that the feature index must be preceded by the count index for this to work. */
    int count = a_fapi_font->get_word(a_fapi_font, a_index - 1, 0);

    write_array_entry_with_count(a_fapi_font, a_output, a_name, a_index,
                                 count, a_divisor);
}

static void
write_subrs(gs_fapi_font * a_fapi_font, WRF_output * a_output, int raw)
{
    int i;
    int count =
        a_fapi_font->get_word(a_fapi_font, gs_fapi_font_feature_Subrs_count,
                              0);
    if (count <= 0)
        return;

    WRF_wstring(a_fapi_font->memory, a_output, "/Subrs ");
    WRF_wint(a_fapi_font->memory, a_output, count);
    WRF_wstring(a_fapi_font->memory, a_output, " array\n");

    for (i = 0; i < count; i++) {
        long length;
        long buffer_size;

        if (raw)
            length = a_fapi_font->get_raw_subr(a_fapi_font, i, 0, 0);
        else
            length = a_fapi_font->get_subr(a_fapi_font, i, 0, 0);
        WRF_wstring(a_fapi_font->memory, a_output, "dup ");
        WRF_wint(a_fapi_font->memory, a_output, i);
        WRF_wbyte(a_fapi_font->memory, a_output, ' ');
        WRF_wint(a_fapi_font->memory, a_output, length);
        WRF_wstring(a_fapi_font->memory, a_output, " RD ");

        /* Get the subroutine into the buffer and encrypt it in place. */
        buffer_size = a_output->m_limit - a_output->m_count;
        if (buffer_size >= length) {
            if (raw)
                a_fapi_font->get_raw_subr(a_fapi_font, i, a_output->m_pos,
                                          (ushort) length);
            else
                a_fapi_font->get_subr(a_fapi_font, i, a_output->m_pos,
                                      (ushort) length);
            WRF_wtext(a_fapi_font->memory, a_output, a_output->m_pos, length);
        } else
            a_output->m_count += length;

        WRF_wstring(a_fapi_font->memory, a_output, " NP\n");
    }

    WRF_wstring(a_fapi_font->memory, a_output, "ND\n");
}

static void
write_charstrings(gs_fapi_font * a_fapi_font, WRF_output * a_output)
{
    long length;
    long buffer_size;
    int i, count = a_fapi_font->get_word(a_fapi_font,
                                         gs_fapi_font_feature_CharStrings_count,
                                         0);
    char NameBuf[256];

    if (count <= 0)
        return;

    WRF_wstring(a_fapi_font->memory, a_output, "2 index /CharStrings ");
    WRF_wint(a_fapi_font->memory, a_output, count);
    WRF_wstring(a_fapi_font->memory, a_output, " dict dup begin\n");
    for (i = 0; i < count; i++) {
        length =
            a_fapi_font->get_charstring_name(a_fapi_font, i,
                                             (byte *) & NameBuf, 256);
        if (length > 0) {
            length = a_fapi_font->get_charstring(a_fapi_font, i, 0, 0);

            WRF_wbyte(a_fapi_font->memory, a_output, '/');
            WRF_wstring(a_fapi_font->memory, a_output, (const char *)&NameBuf);
            WRF_wbyte(a_fapi_font->memory, a_output, ' ');
            WRF_wint(a_fapi_font->memory, a_output, length);
            WRF_wstring(a_fapi_font->memory, a_output, " RD ");

            /* Get the CharString into the buffer and encrypt it in place. */
            buffer_size = a_output->m_limit - a_output->m_count;
            if (buffer_size >= length) {
                a_fapi_font->get_charstring(a_fapi_font, i, a_output->m_pos,
                                            (ushort) length);
                WRF_wtext(a_fapi_font->memory, a_output, a_output->m_pos, length);
            } else
                a_output->m_count += length;
            WRF_wstring(a_fapi_font->memory, a_output, " ND\n");
        }
    }
    WRF_wstring(a_fapi_font->memory, a_output, " end");
}

static int
is_MM_font(gs_fapi_font * a_fapi_font)
{
    return a_fapi_font->get_word(a_fapi_font,
                                 gs_fapi_font_feature_DollarBlend, 0);
}

static void
write_private_blend_dictionary(gs_fapi_font * a_fapi_font, WRF_output * a_output)
{
    short x, x1, x2, i, j, acc;
    if (is_MM_font(a_fapi_font)) {

        WRF_wstring(a_fapi_font->memory, a_output, "3 index /Blend get /Private get begin\n");
        x = a_fapi_font->get_word(a_fapi_font,
                                  gs_fapi_font_feature_BlendBlueValues_length, 0);
        if (x > 0) {
            WRF_wstring(a_fapi_font->memory, a_output, "/BlueValues [");
            acc = 0;
            for (i = 0; i < x; i++) {
                WRF_wstring(a_fapi_font->memory, a_output, " [");
                x1 = a_fapi_font->get_word(a_fapi_font,
                                      gs_fapi_font_feature_BlendBlueValues_count, i);
                for (j = 0; j < x1; j++) {
                    x2 = a_fapi_font->get_word(a_fapi_font,
                                      gs_fapi_font_feature_BlendBlueValues, acc++);
                    WRF_wint(a_fapi_font->memory, a_output, x2);
                    WRF_wbyte(a_fapi_font->memory, a_output, (byte)' ');
                }
                WRF_wstring(a_fapi_font->memory, a_output, " ]");
            }
            WRF_wstring(a_fapi_font->memory, a_output, " ]\n");
        }

        x = a_fapi_font->get_word(a_fapi_font,
                                  gs_fapi_font_feature_BlendOtherBlues_length, 0);
        if (x > 0) {
            WRF_wstring(a_fapi_font->memory, a_output, "/OtherBlues [");
            acc = 0;
            for (i = 0; i < x; i++) {
                WRF_wstring(a_fapi_font->memory, a_output, " [");
                x1 = a_fapi_font->get_word(a_fapi_font,
                                      gs_fapi_font_feature_BlendOtherBlues_count, i);
                for (j = 0; j < x1; j++) {
                    x2 = a_fapi_font->get_word(a_fapi_font,
                                      gs_fapi_font_feature_BlendOtherBlues, acc++);
                    WRF_wint(a_fapi_font->memory, a_output, x2);
                    WRF_wbyte(a_fapi_font->memory, a_output, (byte)' ');
                }
                WRF_wstring(a_fapi_font->memory, a_output, " ]");
            }
            WRF_wstring(a_fapi_font->memory, a_output, " ]\n");
        }
        x = a_fapi_font->get_word(a_fapi_font,
                                  gs_fapi_font_feature_BlendBlueScale_count, 0);
        if (x > 0) {
            float v;
            WRF_wstring(a_fapi_font->memory, a_output, "/BlueScale [");
            for (i =0; i < x; i++) {
                v = a_fapi_font->get_float(a_fapi_font,
                                      gs_fapi_font_feature_BlendBlueScale, i);
                WRF_wfloat(a_fapi_font->memory, a_output, v);
                WRF_wbyte(a_fapi_font->memory, a_output, (byte)' ');
            }
            WRF_wstring(a_fapi_font->memory, a_output, " ]\n");
        }

        x = a_fapi_font->get_word(a_fapi_font,
                                  gs_fapi_font_feature_BlendBlueShift_count, 0);
        if (x > 0) {
            WRF_wstring(a_fapi_font->memory, a_output, "/BlueShift [");
            for (i =0; i < x; i++) {
                x2 = a_fapi_font->get_word(a_fapi_font,
                                      gs_fapi_font_feature_BlendBlueShift, i);
                WRF_wint(a_fapi_font->memory, a_output, x2);
                WRF_wbyte(a_fapi_font->memory, a_output, (byte)' ');
            }
            WRF_wstring(a_fapi_font->memory, a_output, " ]\n");
        }

        x = a_fapi_font->get_word(a_fapi_font,
                                  gs_fapi_font_feature_BlendBlueFuzz_count, 0);
        if (x > 0) {
            WRF_wstring(a_fapi_font->memory, a_output, "/BlueFuzz [");
            for (i =0; i < x; i++) {
                x2 = a_fapi_font->get_word(a_fapi_font,
                                      gs_fapi_font_feature_BlendBlueFuzz, i);
                WRF_wint(a_fapi_font->memory, a_output, x2);
                WRF_wbyte(a_fapi_font->memory, a_output, (byte)' ');
            }
            WRF_wstring(a_fapi_font->memory, a_output, " ]\n");
        }

        x = a_fapi_font->get_word(a_fapi_font,
                                  gs_fapi_font_feature_BlendForceBold_count, 0);
        if (x > 0) {
            WRF_wstring(a_fapi_font->memory, a_output, "/ForceBold [");
            for (i =0; i < x; i++) {
                x2 = a_fapi_font->get_word(a_fapi_font,
                                      gs_fapi_font_feature_BlendForceBold, i);
                WRF_wstring(a_fapi_font->memory, a_output, x2 ? "/true" : "/false");
                WRF_wbyte(a_fapi_font->memory, a_output, (byte)' ');
            }
            WRF_wstring(a_fapi_font->memory, a_output, " ]\n");
        }

        x = a_fapi_font->get_word(a_fapi_font,
                                  gs_fapi_font_feature_BlendStdHW_length, 0);
        if (x > 0) {
            WRF_wstring(a_fapi_font->memory, a_output, "/StdHW [");
            acc = 0;
            for (i = 0; i < x; i++) {
                WRF_wstring(a_fapi_font->memory, a_output, " [");
                x1 = a_fapi_font->get_word(a_fapi_font,
                                      gs_fapi_font_feature_BlendStdHW_count, i);
                for (j = 0; j < x1; j++) {
                    x2 = a_fapi_font->get_word(a_fapi_font,
                                      gs_fapi_font_feature_BlendStdHW, acc++);
                    WRF_wint(a_fapi_font->memory, a_output, x2);
                    WRF_wbyte(a_fapi_font->memory, a_output, (byte)' ');
                }
                WRF_wstring(a_fapi_font->memory, a_output, " ]");
            }
            WRF_wstring(a_fapi_font->memory, a_output, " ]\n");
        }

        x = a_fapi_font->get_word(a_fapi_font,
                                  gs_fapi_font_feature_BlendStdVW_length, 0);
        if (x > 0) {
            WRF_wstring(a_fapi_font->memory, a_output, "/StdVW [");
            acc = 0;
            for (i = 0; i < x; i++) {
                WRF_wstring(a_fapi_font->memory, a_output, " [");
                x1 = a_fapi_font->get_word(a_fapi_font,
                                      gs_fapi_font_feature_BlendStdVW_count, i);
                for (j = 0; j < x1; j++) {
                    x2 = a_fapi_font->get_word(a_fapi_font,
                                      gs_fapi_font_feature_BlendStdVW, acc++);
                    WRF_wint(a_fapi_font->memory, a_output, x2);
                    WRF_wbyte(a_fapi_font->memory, a_output, (byte)' ');
                }
                WRF_wstring(a_fapi_font->memory, a_output, " ]");
            }
            WRF_wstring(a_fapi_font->memory, a_output, " ]\n");
        }

        x = a_fapi_font->get_word(a_fapi_font,
                                  gs_fapi_font_feature_BlendStemSnapH_length, 0);
        if (x > 0) {
            WRF_wstring(a_fapi_font->memory, a_output, "/StemSnapH [");
            acc = 0;
            for (i = 0; i < x; i++) {
                WRF_wstring(a_fapi_font->memory, a_output, " [");
                x1 = a_fapi_font->get_word(a_fapi_font,
                                      gs_fapi_font_feature_BlendStemSnapH_count, i);
                for (j = 0; j < x1; j++) {
                    x2 = a_fapi_font->get_word(a_fapi_font,
                                      gs_fapi_font_feature_BlendStemSnapH, acc++);
                    WRF_wint(a_fapi_font->memory, a_output, x2);
                    WRF_wbyte(a_fapi_font->memory, a_output, (byte)' ');
                }
                WRF_wstring(a_fapi_font->memory, a_output, " ]");
            }
            WRF_wstring(a_fapi_font->memory, a_output, " ]\n");
        }

        x = a_fapi_font->get_word(a_fapi_font,
                                  gs_fapi_font_feature_BlendStemSnapV_length, 0);
        if (x > 0) {
            WRF_wstring(a_fapi_font->memory, a_output, "/StemSnapV [");
            acc = 0;
            for (i = 0; i < x; i++) {
                WRF_wstring(a_fapi_font->memory, a_output, " [");
                x1 = a_fapi_font->get_word(a_fapi_font,
                                      gs_fapi_font_feature_BlendStemSnapV_count, i);
                for (j = 0; j < x1; j++) {
                    x2 = a_fapi_font->get_word(a_fapi_font,
                                      gs_fapi_font_feature_BlendStemSnapV, acc++);
                    WRF_wint(a_fapi_font->memory, a_output, x2);
                    WRF_wbyte(a_fapi_font->memory, a_output, (byte)' ');
                }
                WRF_wstring(a_fapi_font->memory, a_output, " ]");
            }
            WRF_wstring(a_fapi_font->memory, a_output, " ]\n");
        }

        WRF_wstring(a_fapi_font->memory, a_output, "end\n");
    }
}

static void
write_private_dictionary(gs_fapi_font * a_fapi_font, WRF_output * a_output,
                         int Write_CharStrings)
{
    a_output->m_encrypt = true;

    /* Write 4 bytes that must encrypt to at least one character that cannot be a valid hexadecimal character. */
    WRF_wstring(a_fapi_font->memory, a_output, "XXXX");

    /*+ to do: correct size of dictionary from 8. */
    WRF_wstring(a_fapi_font->memory, a_output, "dup /Private 8 dict dup begin\n");

    WRF_wstring(a_fapi_font->memory, a_output, "/MinFeature {16 16} def\n");
    WRF_wstring(a_fapi_font->memory, a_output, "/password 5839 def\n");
    if (Write_CharStrings)
        write_word_entry(a_fapi_font, a_output, "lenIV",
                         gs_fapi_font_feature_lenIV, 1);
    else
        WRF_wstring(a_fapi_font->memory, a_output, "/lenIV -1 def\n");       /* indicate that /subrs are not encoded. */
    write_word_entry(a_fapi_font, a_output, "BlueFuzz",
                     gs_fapi_font_feature_BlueFuzz, 16);

    WRF_wstring(a_fapi_font->memory, a_output, "/BlueScale ");
    WRF_wfloat(a_fapi_font->memory, a_output,
               a_fapi_font->get_long(a_fapi_font,
                                     gs_fapi_font_feature_BlueScale,
                                     0) / 65536.0);
    WRF_wstring(a_fapi_font->memory, a_output, " def\n");

    write_word_entry(a_fapi_font, a_output, "BlueShift",
                     gs_fapi_font_feature_BlueShift, 16);
    write_array_entry(a_fapi_font, a_output, "BlueValues",
                      gs_fapi_font_feature_BlueValues, 16);
    write_array_entry(a_fapi_font, a_output, "OtherBlues",
                      gs_fapi_font_feature_OtherBlues, 16);
    write_array_entry(a_fapi_font, a_output, "FamilyBlues",
                      gs_fapi_font_feature_FamilyBlues, 16);
    write_array_entry(a_fapi_font, a_output, "FamilyOtherBlues",
                      gs_fapi_font_feature_FamilyOtherBlues, 16);
    write_word_entry(a_fapi_font, a_output, "ForceBold",
                     gs_fapi_font_feature_ForceBold, 1);
    write_array_entry_with_count(a_fapi_font, a_output, "StdHW",
                                 gs_fapi_font_feature_StdHW, 1, 16);
    write_array_entry_with_count(a_fapi_font, a_output, "StdVW",
                                 gs_fapi_font_feature_StdVW, 1, 16);
    write_array_entry(a_fapi_font, a_output, "StemSnapH",
                      gs_fapi_font_feature_StemSnapH, 16);
    write_array_entry(a_fapi_font, a_output, "StemSnapV",
                      gs_fapi_font_feature_StemSnapV, 16);

    write_private_blend_dictionary(a_fapi_font, a_output);

    if (Write_CharStrings)
        write_subrs(a_fapi_font, a_output, 1);
    else
        write_subrs(a_fapi_font, a_output, 0);
    if (Write_CharStrings)
        write_charstrings(a_fapi_font, a_output);
}

static void
write_main_dictionary(gs_fapi_font * a_fapi_font, WRF_output * a_output,
                      int Write_CharStrings)
{
    int i;

    WRF_wstring(a_fapi_font->memory, a_output, "5 dict begin\n");

    WRF_wstring(a_fapi_font->memory, a_output, "/FontType 1 def\n");

    WRF_wstring(a_fapi_font->memory, a_output, "/FontMatrix [");
    for (i = 0; i < 6; i++) {
        WRF_wfloat(a_fapi_font->memory, a_output,
                   a_fapi_font->get_float(a_fapi_font,
                                          gs_fapi_font_feature_FontMatrix,
                                          i));
        WRF_wbyte(a_fapi_font->memory, a_output, (byte) (i == 5 ? ']' : ' '));
    }
    WRF_wbyte(a_fapi_font->memory, a_output, '\n');
    /* For now, specify standard encoding - I think GS will pass glyph indices so doesn't matter. */
    WRF_wstring(a_fapi_font->memory, a_output, "/Encoding StandardEncoding def\n");

    WRF_wstring(a_fapi_font->memory, a_output, "/FontBBox {");
    for (i = 0; i < 4; i++) {
        short x =
            a_fapi_font->get_word(a_fapi_font, gs_fapi_font_feature_FontBBox,
                                  i);
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
        x = a_fapi_font->get_word(a_fapi_font,
                                  gs_fapi_font_feature_BlendAxisTypes_count,
                                  0);
        if (x)
            entries++;
        x = a_fapi_font->get_word(a_fapi_font,
                                  gs_fapi_font_feature_BlendDesignPositionsArrays_count,
                                  0);
        if (x)
            entries++;
        x = a_fapi_font->get_word(a_fapi_font,
                                  gs_fapi_font_feature_BlendDesignMapArrays_count,
                                  0);
        if (x)
            entries++;

        gs_sprintf(Buffer, "/FontInfo %d dict dup begin\n", entries);
        WRF_wstring(a_fapi_font->memory, a_output, Buffer);
        x = a_fapi_font->get_word(a_fapi_font,
                                  gs_fapi_font_feature_BlendAxisTypes_count,
                                  0);
        if (x) {
            WRF_wstring(a_fapi_font->memory, a_output, "/BlendAxisTypes [");
            for (i = 0; i < x; i++) {
                WRF_wstring(a_fapi_font->memory, a_output, " /");
                a_fapi_font->get_name(a_fapi_font,
                                      gs_fapi_font_feature_BlendAxisTypes, i,
                                      (char *)&Buffer, 255);
                WRF_wstring(a_fapi_font->memory, a_output, Buffer);
            }
            WRF_wstring(a_fapi_font->memory, a_output, "] def\n");
        }
        x = a_fapi_font->get_word(a_fapi_font,
                                  gs_fapi_font_feature_BlendDesignPositionsArrays_count,
                                  0);
        if (x) {
            WRF_wstring(a_fapi_font->memory, a_output, "/BlendDesignPositions [");
            x2 = a_fapi_font->get_word(a_fapi_font,
                                       gs_fapi_font_feature_BlendAxisTypes_count,
                                       0);
            for (i = 0; i < x; i++) {
                WRF_wstring(a_fapi_font->memory, a_output, "[");
                for (j = 0; j < x2; j++) {
                    x1 = a_fapi_font->get_float(a_fapi_font,
                                                gs_fapi_font_feature_BlendDesignPositionsArrayValue,
                                                i * 8 + j);
                    gs_sprintf(Buffer, "%f ", x1);
                    WRF_wstring(a_fapi_font->memory, a_output, Buffer);
                }
                WRF_wstring(a_fapi_font->memory, a_output, "]");
            }
            WRF_wstring(a_fapi_font->memory, a_output, "] def\n");
        }
        x = a_fapi_font->get_word(a_fapi_font,
                                  gs_fapi_font_feature_BlendDesignMapArrays_count,
                                  0);
        if (x) {
            WRF_wstring(a_fapi_font->memory, a_output, "/BlendDesignMap [");
            for (i = 0; i < x; i++) {
                x2 = a_fapi_font->get_word(a_fapi_font,
                                           gs_fapi_font_feature_BlendDesignMapSubArrays_count,
                                           i);
                WRF_wstring(a_fapi_font->memory, a_output, "[");
                for (j = 0; j < x2; j++) {
                    WRF_wstring(a_fapi_font->memory, a_output, "[");
                    x1 = a_fapi_font->get_float(a_fapi_font,
                                                gs_fapi_font_feature_BlendDesignPositionsArrayValue,
                                                i * 64 + j * 64);
                    gs_sprintf(Buffer, "%f ", x1);
                    WRF_wstring(a_fapi_font->memory, a_output, Buffer);
                    x1 = a_fapi_font->get_float(a_fapi_font,
                                                gs_fapi_font_feature_BlendDesignPositionsArrayValue,
                                                i * 64 + j * 64 + 1);
                    gs_sprintf(Buffer, "%f ", x1);
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
        if ((x =
             a_fapi_font->get_word(a_fapi_font,
                                   gs_fapi_font_feature_DollarBlend_length,
                                   0)) > 0) {
            WRF_wstring(a_fapi_font->memory, a_output, "/$Blend {");

            if (a_output->m_count)
                a_output->m_count += x;
            x = a_fapi_font->get_proc(a_fapi_font,
                                      gs_fapi_font_feature_DollarBlend, 0,
                                      (char *)a_output->m_pos);
            if (a_output->m_pos)
                a_output->m_pos += x;
            WRF_wstring(a_fapi_font->memory, a_output, "} def\n");
        } else {
            WRF_wstring(a_fapi_font->memory, a_output,
                        "/$Blend {0.1 mul exch 0.45 mul add exch 0.17 mul add add} def\n");
        }
#if 1
        WRF_wstring(a_fapi_font->memory, a_output, "/WeightVector [");
        x = a_fapi_font->get_word(a_fapi_font,
                                  gs_fapi_font_feature_WeightVector_count, 0);
        for (i = 0; i < x; i++) {
            x1 = a_fapi_font->get_float(a_fapi_font,
                                        gs_fapi_font_feature_WeightVector, i);
            gs_sprintf(Buffer, "%f ", x1);
            WRF_wstring(a_fapi_font->memory, a_output, Buffer);
        }
        WRF_wstring(a_fapi_font->memory, a_output, "] def\n");
#endif

        WRF_wstring(a_fapi_font->memory, a_output, "/Blend 3 dict dup begin\n");
        WRF_wstring(a_fapi_font->memory, a_output, "/FontBBox {");
        x = a_fapi_font->get_word(a_fapi_font,
                                 gs_fapi_font_feature_BlendFontBBox_length , 0);
        for (i = 0; i < x; i++) {
            int j;
            WRF_wstring(a_fapi_font->memory, a_output, " {");
            for (j = 0; j < 4; j++) {
                x2 = a_fapi_font->get_word(a_fapi_font,
                                  gs_fapi_font_feature_BlendFontBBox,
                                  j + (i * 4));
                WRF_wint(a_fapi_font->memory, a_output, x2);
                WRF_wbyte(a_fapi_font->memory, a_output, (byte)' ');
            }
            WRF_wstring(a_fapi_font->memory, a_output, "}");
        }
        WRF_wstring(a_fapi_font->memory, a_output, " } def\n");
        WRF_wstring(a_fapi_font->memory, a_output, "/Private 14 dict def\n");
        WRF_wstring(a_fapi_font->memory, a_output, "end def\n");
    }
    WRF_wstring(a_fapi_font->memory, a_output, "currentdict end\ncurrentfile eexec\n");
    write_private_dictionary(a_fapi_font, a_output, Write_CharStrings);
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
    WRF_output output;

    WRF_init(&output, a_buffer, a_buffer_size);

    /* Leading comment identifying a Type 1 font. */
    WRF_wstring(a_fapi_font->memory, &output, "%!PS-AdobeFont-1\n");

    write_main_dictionary(a_fapi_font, &output, 0);
#if 0
    {
        extern FILE *stdout;
        if (is_MM_font(a_fapi_font) && a_buffer && a_buffer_size >= output.m_count) {
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
    WRF_output output;

    WRF_init(&output, a_buffer, a_buffer_size);

    /* Leading comment identifying a Type 1 font. */
    WRF_wstring(a_fapi_font->memory, &output, "%!PS-AdobeFont-1\n");

    write_main_dictionary(a_fapi_font, &output, 1);
    return output.m_count;
}
