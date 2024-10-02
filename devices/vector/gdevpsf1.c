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


/* Write an embedded Type 1 font */
#include "math.h"
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsccode.h"
#include "gsmatrix.h"
#include "gxfixed.h"
#include "gxfont.h"
#include "gxfont1.h"
#include "gxmatrix.h"		/* for gxtype1.h */
#include "gxtype1.h"
#include "strimpl.h"		/* required by Watcom compiler (why?) */
#include "stream.h"
#include "sfilter.h"
#include "spsdf.h"
#include "sstring.h"
#include "spprint.h"
#include "gdevpsf.h"

/* ------ Utilities shared with CFF writer ------ */

/* Gather glyph information for a Type 1 or Type 2 font. */
int
psf_type1_glyph_data(gs_font_base *pbfont, gs_glyph glyph,
                     gs_glyph_data_t *pgd, gs_font_type1 **ppfont)
{
    gs_font_type1 *const pfont = (gs_font_type1 *)pbfont;

    *ppfont = pfont;
    return pfont->data.procs.glyph_data(pfont, glyph, pgd);
}
int
psf_get_type1_glyphs(psf_outline_glyphs_t *pglyphs, gs_font_type1 *pfont,
                     gs_glyph *subset_glyphs, uint subset_size)
{
    return psf_get_outline_glyphs(pglyphs, (gs_font_base *)pfont,
                                  subset_glyphs, subset_size,
                                  psf_type1_glyph_data);
}

/* ------ Main program ------ */

/* Write a (named) array of floats. */
static int
write_float_array(gs_param_list *plist, const char *key, const float *values,
                  int count)
{
    if (count != 0) {
        gs_param_float_array fa;

        fa.persistent = false;
        fa.size = count;
        fa.data = values;
        return param_write_float_array(plist, key, &fa);
    }
    return 0;
}

/* Write a UniqueID and/or XUID. */
static void
write_uid(stream *s, const gs_uid *puid, int options)
{
    if (uid_is_UniqueID(puid))
        pprintld1(s, "/UniqueID %ld def\n", puid->id);
    else if (uid_is_XUID(puid) && (options & WRITE_TYPE1_XUID) != 0) {
        uint i, n = uid_XUID_size(puid);

        /* Adobe products (specifically Acrobat but the same limitation is mentioned
         * in the PLRM) cannot handle XUIDs > 16 entries.
         */
        if (n > 16)
            n = 16;

        stream_puts(s, "/XUID [");
        for (i = 0; i < n; ++i)
            pprintld1(s, "%ld ", uid_XUID_values(puid)[i]);
        stream_puts(s, "] readonly def\n");
    }
}

/* Write the font name. */
static void
write_font_name(stream *s, const gs_font_type1 *pfont,
                const gs_const_string *alt_font_name, bool as_name)
{
    const byte *c;
    const byte *name = (alt_font_name ? alt_font_name->data : pfont->font_name.chars);
    int         n    = (alt_font_name ? alt_font_name->size : pfont->font_name.size);

    if (n == 0)
        /* empty name, may need to write it as empty string */
        stream_puts(s, (as_name ? "/" : "()"));
    else {
        for (c = (byte *)"()<>[]{}/% \n\r\t\b\f\004\033"; *c; c++)
            if (memchr(name, *c, n))
                break;
        if (*c || memchr(name, 0, n)) {
            /* name contains whitespace (NUL included) or a PostScript separator */
            byte pssebuf[1 + 4 * gs_font_name_max + 1]; /* "(" + "\ooo" * gs_font_name_max + ")" */
            stream_cursor_read  r;
            stream_cursor_write w;

            pssebuf[0] = '(';
            r.limit = (r.ptr = name - 1) + n;
            w.limit = (w.ptr = pssebuf) + sizeof pssebuf - 1;
            s_PSSE_template.process(NULL, &r, &w, true);
            stream_write(s, pssebuf, w.ptr - pssebuf + 1);
            if (as_name)
                stream_puts(s, " cvn");
        } else {
            /* name without any special characters */
            if (as_name)
                stream_putc(s, '/');
            stream_write(s, name, n);
        }
    }
}
/*
 * Write the Encoding array.  This is a separate procedure only for
 * readability.
 */
static int
write_Encoding(stream *s, gs_font_type1 *pfont, int options,
              gs_glyph *subset_glyphs, uint subset_size, gs_glyph notdef)
{
    stream_puts(s, "/Encoding ");
    switch (pfont->encoding_index) {
        case ENCODING_INDEX_STANDARD:
            stream_puts(s, "StandardEncoding");
            break;
        case ENCODING_INDEX_ISOLATIN1:
            /* ATM only recognizes StandardEncoding. */
            if (options & WRITE_TYPE1_POSTSCRIPT) {
                stream_puts(s, "ISOLatin1Encoding");
                break;
            }
        default:{
                gs_char i;

                stream_puts(s, "256 array\n");
                stream_puts(s, "0 1 255 {1 index exch /.notdef put} for\n");
                for (i = 0; i < 256; ++i) {
                    gs_glyph glyph =
                        (*pfont->procs.encode_char)
                        ((gs_font *)pfont, (gs_char)i, GLYPH_SPACE_NAME);
                    gs_const_string namestr;

                    if (subset_glyphs && subset_size) {
                        /*
                         * Only write Encoding entries for glyphs in the
                         * subset.  Use binary search to check each glyph,
                         * since subset_glyphs are sorted.
                         */
                        if (!psf_sorted_glyphs_include(subset_glyphs,
                                                        subset_size, glyph))
                            continue;
                    }
                    if (glyph != GS_NO_GLYPH && glyph != notdef &&
                        pfont->procs.glyph_name((gs_font *)pfont, glyph,
                                                &namestr) >= 0
                        ) {
                        pprintd1(s, "dup %d /", (int)i);
                        stream_write(s, namestr.data, namestr.size);
                        stream_puts(s, " put\n");
                    }
                }
                stream_puts(s, "readonly");
            }
    }
    stream_puts(s, " def\n");
    return 0;
}

static int WriteNumber (byte *dest, int value)
{
    if (value >= -107 && value <= 107) {
        *dest = value + 139;
        return 1;
    } else {
        if (value >= 108 && value <= 1131) {
            int quotient = (int)floor((value - 108) / (double)256);
            dest[0] = quotient + 247;
            dest[1] = value - 108 - quotient * 256;
            return 2;
        } else {
            if (value <= -108 && value >= -1131) {
                int quotient = (int)floor((value + 108) / -256);
                int newval = value + 256 * quotient + 108;
                dest[0] = quotient + 251;
                dest[1] = newval * -1;
                return 2;
            } else {
                dest[0] = 255;
                dest[1] = value >> 24;
                dest[2] = (value & 0xFF0000) >> 16;
                dest[3] = (value & 0xFF00) >> 8;
                dest[4] = value & 0xFF;
                return 5;
            }
        }
    }
    return 0;
}

/* The following 2 routines attempt to parse out Multiple Master 'OtherSubrs'
 * calls, and replace the multiple arguments to $Blend with the two 'base'
 * parameters. This works reasonably well but can be defeated. FOr example a
 * CharString which puts some parameters on the operand stack, then calls a
 * Subr which puts the remaining parameters on the stack, and calls a MM
 * OtherSubr (constructions like this have been observed). In general we
 * work around this by storing the operands on the stack, but it is possible
 * that the values are calculated (eg x y div) which is a common way to get
 * float values into the interpreter. This will defeat the code below.
 *
 * The only way to solve this is to actually fully interpret the CharString
 * and any /Subrs it calls, and then emit the result as a non-MM CharString
 * by blending the values. This would mean writing a new routine like
 * 'psf_convert_type1_to_type2' (see gdevpsfx.c) or modifying that routine
 * so that it outputs type 1 CharStrings (which is probably simpler to do).
 */
static int CheckSubrForMM (gs_glyph_data_t *gdata, gs_font_type1 *pfont)
{
    crypt_state state = crypt_charstring_seed;
    int code = 0;
    gs_bytestring *data = (gs_bytestring *)&gdata->bits;
    byte *source = data->data, *end = source + data->size;
    int CurrentNumberIndex = 0, Stack[32];

    memset(Stack, 0x00, sizeof(Stack));
    gs_type1_decrypt(source, source, data->size, &state);

    if (pfont->data.lenIV >= data->size) {
        code = gs_note_error(gs_error_invalidfont);
        goto error;
    }

    if(pfont->data.lenIV > 0)
        source += pfont->data.lenIV;

    while (source < end) {
        if (*source < 32) {
            /* Command */
            switch (*source) {
                case 12:
                    if (source + 2 > end) {
                        code = gs_note_error(gs_error_invalidfont);
                        goto error;
                    }
                    if (*(source + 1) == 16) {
                        if (CurrentNumberIndex < 1) {
			                code = gs_note_error(gs_error_rangecheck);
                            goto error;
                        }
                        switch(Stack[CurrentNumberIndex-1]) {
                            case 18:
                                code = 6;
                                break;
                            case 17:
                                code = 4;
                                break;
                            case 16:
                                code = 3;
                                break;
                            case 15:
                                code = 2;
                                break;
                            case 14:
                                code = 1;
                                break;
                            default:
                                code = 0;
                                break;
                        }
                        source += 2;
                    } else {
                        source +=2;
                    }
                    break;
                default:
                    source++;
                    break;
            }
            CurrentNumberIndex = 0;
        } else {
            /* Number */
            if (CurrentNumberIndex >= count_of(Stack)) {
                code = gs_note_error(gs_error_rangecheck);
                goto error;
            }
            if (*source < 247) {
                Stack[CurrentNumberIndex++] = *source++ - 139;
            } else {
                if (*source < 251) {
                    if (source + 2 > end) {
                        code = gs_note_error(gs_error_invalidfont);
                        goto error;
                    }
                    Stack[CurrentNumberIndex] = ((*source++ - 247) * 256) + 108;
                    Stack[CurrentNumberIndex++] += *source++;
                } else {
                    if (*source < 255) {
                        if (source + 2 > end) {
                            code = gs_note_error(gs_error_invalidfont);
                            goto error;
                        }
                        Stack[CurrentNumberIndex] = ((*source++ - 251) * -256) - 108;
                        Stack[CurrentNumberIndex++] -= *source++;
                    } else {
                        if (source + 5 > end) {
                            code = gs_note_error(gs_error_invalidfont);
                            goto error;
                        }
                        source++;
                        Stack[CurrentNumberIndex] = *source++ << 24;
                        Stack[CurrentNumberIndex] += *source++ << 16;
                        Stack[CurrentNumberIndex] += *source++ << 8;
                        Stack[CurrentNumberIndex++] += *source++;
                    }
                }
            }
        }
    }
    /* We decrypted the data in place at the start of the routine, we must re-encrypt it
     * before we return, even if there's an error.
     */
error:
    state = crypt_charstring_seed;
    source = data->data;
    gs_type1_encrypt(source, source, data->size, &state);
    return code;
}
#undef MM_STACK_SIZE

static int strip_othersubrs(gs_glyph_data_t *gdata, gs_font_type1 *pfont, byte *stripped, byte *SubrsWithMM, int SubrsCount)
{
    crypt_state state = crypt_charstring_seed;
    gs_bytestring *data = (gs_bytestring *)&gdata->bits;
    byte *source = data->data, *dest = stripped, *end = source + data->size;
    int i, dest_length = 0, CurrentNumberIndex = 0, Stack[64], written;
    int OnlyCalcLength = 0;
    char Buffer[16];

    memset(Stack, 0x00, 64 * sizeof(int));
    if (stripped == NULL) {
        OnlyCalcLength = 1;
        dest = (byte *)&Buffer;
    }

    if (pfont->data.lenIV >= data->size)
        return gs_note_error(gs_error_invalidfont);

    gs_type1_decrypt(source, source, data->size, &state);

    if(pfont->data.lenIV >= 0) {
        for (i=0;i<pfont->data.lenIV;i++) {
            if (!OnlyCalcLength)
                *dest++ = *source++;
            else
                source++;
        }
        dest_length += pfont->data.lenIV;
    }
    while (source < end) {
        if (*source < 32) {
            /* Command */
            switch (*source) {
                case 12:
                    if (source + 2 > end) {
                        dest_length = gs_note_error(gs_error_invalidfont);
                        goto error;
                    }
                    if (*(source + 1) == 16) {
                        if (CurrentNumberIndex < 1) {
                            dest_length = gs_note_error(gs_error_rangecheck);
                            goto error;
                        }
                        /* Callothersubsr, the only thing we care about */
                        switch(Stack[CurrentNumberIndex-1]) {
                            /* If we find a Multiple Master call, remove all but the
                             * first set of arguments. Mimics the result of a call.
                             * Adobe 'encourages' the use of Subrs to do MM, but
                             * the spec doens't say you have to, so we need to be
                             * prepared, just in case. I doubt we will ever execute
                             * this code.
                             */
                            case 14:
                                CurrentNumberIndex -= pfont->data.WeightVector.count - 1;
                                for (i = 0;i < CurrentNumberIndex;i++) {
                                    written = WriteNumber(dest, Stack[i]);
                                    dest_length += written;
                                    if (!OnlyCalcLength)
                                        dest += written;
                                }
                                source += 2;
                                break;
                            case 15:
                                CurrentNumberIndex -= (pfont->data.WeightVector.count - 1) * 2;
                                for (i = 0;i < CurrentNumberIndex;i++) {
                                    written = WriteNumber(dest, Stack[i]);
                                    dest_length += written;
                                    if (!OnlyCalcLength)
                                        dest += written;
                                }
                                source += 2;
                                break;
                            case 16:
                                CurrentNumberIndex -= (pfont->data.WeightVector.count - 1) * 3;
                                for (i = 0;i < CurrentNumberIndex;i++) {
                                    written = WriteNumber(dest, Stack[i]);
                                    dest_length += written;
                                    if (!OnlyCalcLength)
                                        dest += written;
                                }
                                source += 2;
                                break;
                            case 17:
                                CurrentNumberIndex -= (pfont->data.WeightVector.count - 1) * 4;
                                for (i = 0;i < CurrentNumberIndex;i++) {
                                    written = WriteNumber(dest, Stack[i]);
                                    dest_length += written;
                                    if (!OnlyCalcLength)
                                        dest += written;
                                }
                                source += 2;
                                break;
                            case 18:
                                CurrentNumberIndex -= (pfont->data.WeightVector.count - 1) * 6;
                                for (i = 0;i < CurrentNumberIndex;i++) {
                                    written = WriteNumber(dest, Stack[i]);
                                    dest_length += written;
                                    if (!OnlyCalcLength)
                                        dest += written;
                                }
                                source += 2;
                                break;
                            default:
                                for (i = 0;i < CurrentNumberIndex;i++) {
                                    written = WriteNumber(dest, Stack[i]);
                                    dest_length += written;
                                    if (!OnlyCalcLength)
                                        dest += written;
                                }
                                if (!OnlyCalcLength) {
                                    *dest++ = *source++;
                                    *dest++ = *source++;
                                } else {
                                    source += 2;
                                }
                                dest_length += 2;
                                break;
                        }
                    } else {
                        for (i = 0;i < CurrentNumberIndex;i++) {
                            written = WriteNumber(dest, Stack[i]);
                            dest_length += written;
                            if (!OnlyCalcLength)
                                dest += written;
                        }
                        if (!OnlyCalcLength) {
                            *dest++ = *source++;
                            *dest++ = *source++;
                        } else {
                            source += 2;
                        }
                        dest_length += 2;
                    }
                    break;
                case 10:
                    if (CurrentNumberIndex != 0 && Stack[CurrentNumberIndex - 1] >= 0 &&
                        Stack[CurrentNumberIndex - 1] < SubrsCount && SubrsWithMM != NULL && SubrsWithMM[Stack[CurrentNumberIndex - 1]] != 0)
                    {
                        int index = Stack[CurrentNumberIndex - 1];
                        int StackBase = CurrentNumberIndex - 1 - pfont->data.WeightVector.count * SubrsWithMM[index];

                        CurrentNumberIndex--; /* Remove the subr index */

                        if (StackBase > CurrentNumberIndex) {
                            dest_length = gs_note_error(gs_error_invalidfont);
                            goto error;
                        }

                        for (i=0;i < StackBase; i++) {
                            written = WriteNumber(dest, Stack[i]);
                            dest_length += written;
                            if (!OnlyCalcLength)
                                dest += written;
                        }
                        for (i=0;i<SubrsWithMM[index];i++) {
                            /* See above, it may be that we don't have enough numbers on the stack
                             * (due to constructs such as x y div), if we don't have enough parameters
                             * just write a 0 instead. We know this is incorrect.....
                             */
                            if (StackBase + i >= 0 && StackBase + i < CurrentNumberIndex)
                                written = WriteNumber(dest, Stack[StackBase + i]);
                            else
                                written = WriteNumber(dest, 0);
                            dest_length += written;
                            if (!OnlyCalcLength)
                                dest += written;
                        }
                        source++;
                    } else {
                        for (i = 0;i < CurrentNumberIndex;i++) {
                            written = WriteNumber(dest, Stack[i]);
                            dest_length += written;
                            if (!OnlyCalcLength)
                                dest += written;
                        }
                        if (!OnlyCalcLength)
                            *dest++ = *source++;
                        else
                            source++;
                        dest_length++;
                    }
                    break;
                default:
                    for (i = 0;i < CurrentNumberIndex;i++) {
                        written = WriteNumber(dest, Stack[i]);
                        dest_length += written;
                        if (!OnlyCalcLength)
                            dest += written;
                    }
                    if (!OnlyCalcLength)
                        *dest++ = *source++;
                    else
                        source++;
                    dest_length++;
            }
            CurrentNumberIndex = 0;
        } else {
            if (CurrentNumberIndex >= count_of(Stack)) {
                dest_length = gs_note_error(gs_error_rangecheck);
                goto error;
            }
            /* Number */
            if (*source < 247) {
                Stack[CurrentNumberIndex++] = *source++ - 139;
            } else {
                if (*source < 251) {
                    if (source + 2 > end) {
                        dest_length = gs_note_error(gs_error_invalidfont);
                        goto error;
                    }
                    Stack[CurrentNumberIndex] = ((*source++ - 247) * 256) + 108;
                    Stack[CurrentNumberIndex++] += *source++;
                } else {
                    if (*source < 255) {
                        if (source + 2 > end) {
                            dest_length = gs_note_error(gs_error_invalidfont);
                            goto error;
                        }
                        Stack[CurrentNumberIndex] = ((*source++ - 251) * -256) - 108;
                        Stack[CurrentNumberIndex++] -= *source++;
                    } else {
                        if (source + 5 > end) {
                            dest_length = gs_note_error(gs_error_invalidfont);
                            goto error;
                        }
                        source++;
                        Stack[CurrentNumberIndex] = *source++ << 24;
                        Stack[CurrentNumberIndex] += *source++ << 16;
                        Stack[CurrentNumberIndex] += *source++ << 8;
                        Stack[CurrentNumberIndex++] += *source++;
                    }
                }
            }
        }
    }
    /* We decrypted the data in place at the start of the routine, we must re-encrypt it
     * before we return, even if there's an error.
     */
error:
    source = data->data;
    state = crypt_charstring_seed;
    gs_type1_encrypt(source, source, data->size, &state);

    if (!OnlyCalcLength && dest_length > 0) {
        state = crypt_charstring_seed;
        gs_type1_encrypt(stripped, stripped, dest_length, &state);
    }
    return dest_length;
}

/*
 * Write the Private dictionary.  This is a separate procedure only for
 * readability.  write_CharString is a parameter so that we can encrypt
 * Subrs and CharStrings when the font's lenIV == -1 but we are writing
 * the font with lenIV = 0.
 */
static int
write_Private(stream *s, gs_font_type1 *pfont,
              gs_glyph *subset_glyphs, uint subset_size,
              gs_glyph notdef, int lenIV,
              int (*write_CharString)(stream *, const void *, uint),
              const param_printer_params_t *ppp, int options)
{
    const gs_type1_data *const pdata = &pfont->data;
    printer_param_list_t rlist;
    gs_param_list *const plist = (gs_param_list *)&rlist;
    int code = s_init_param_printer(&rlist, ppp, s);
    byte *SubrsWithMM = 0;
    int SubrsCount = 0;

    if (code < 0)
        return 0;
    stream_puts(s, "dup /Private 17 dict dup begin\n");
    stream_puts(s, "/-|{string currentfile exch readstring pop}executeonly def\n");
    stream_puts(s, "/|-{noaccess def}executeonly def\n");
    stream_puts(s, "/|{noaccess put}executeonly def\n");
    {
        static const gs_param_item_t private_items[] = {
            {"BlueFuzz", gs_param_type_int,
             offset_of(gs_type1_data, BlueFuzz)},
            {"BlueScale", gs_param_type_float,
             offset_of(gs_type1_data, BlueScale)},
            {"BlueShift", gs_param_type_float,
             offset_of(gs_type1_data, BlueShift)},
            {"ExpansionFactor", gs_param_type_float,
             offset_of(gs_type1_data, ExpansionFactor)},
            {"ForceBold", gs_param_type_bool,
             offset_of(gs_type1_data, ForceBold)},
            {"LanguageGroup", gs_param_type_int,
             offset_of(gs_type1_data, LanguageGroup)},
            {"RndStemUp", gs_param_type_bool,
             offset_of(gs_type1_data, RndStemUp)},
            gs_param_item_end
        };
        gs_type1_data defaults;

        defaults.BlueFuzz = 1;
        defaults.BlueScale = (float)0.039625;
        defaults.BlueShift = 7.0;
        defaults.ExpansionFactor = (float)0.06;
        defaults.ForceBold = false;
        defaults.LanguageGroup = 0;
        defaults.RndStemUp = true;
        code = gs_param_write_items(plist, pdata, &defaults, private_items);
        if (code < 0)
            return code;
        if (lenIV != 4) {
            code = param_write_int(plist, "lenIV", &lenIV);
            if (code < 0)
                return code;
        }
        write_float_array(plist, "BlueValues", pdata->BlueValues.values,
                          pdata->BlueValues.count);
        write_float_array(plist, "OtherBlues", pdata->OtherBlues.values,
                          pdata->OtherBlues.count);
        write_float_array(plist, "FamilyBlues", pdata->FamilyBlues.values,
                          pdata->FamilyBlues.count);
        write_float_array(plist, "FamilyOtherBlues", pdata->FamilyOtherBlues.values,
                          pdata->FamilyOtherBlues.count);
        write_float_array(plist, "StdHW", pdata->StdHW.values,
                          pdata->StdHW.count);
        write_float_array(plist, "StdVW", pdata->StdVW.values,
                          pdata->StdVW.count);
        write_float_array(plist, "StemSnapH", pdata->StemSnapH.values,
                          pdata->StemSnapH.count);
        write_float_array(plist, "StemSnapV", pdata->StemSnapV.values,
                          pdata->StemSnapV.count);
    }
    write_uid(s, &pfont->UID, options);
    stream_puts(s, "/MinFeature{16 16} def\n");
    stream_puts(s, "/password 5839 def\n");

    /*
     * Write the Subrs.  We always write them all, even for subsets.
     * (We will fix this someday.)
     */

    {
        int n, i;
        gs_glyph_data_t gdata;
        int code;

        gdata.memory = pfont->memory;
        for (n = 0;
             (code = pdata->procs.subr_data(pfont, n, false, &gdata)) !=
                 gs_error_rangecheck;
             ) {
            ++n;
            if (code >= 0)
                gs_glyph_data_free(&gdata, "write_Private(Subrs)");
        }
        if (pfont->data.WeightVector.count != 0) {
            SubrsCount = n;
            SubrsWithMM = gs_alloc_bytes(pfont->memory, n, "Subrs record");
        }

        pprintd1(s, "/Subrs %d array\n", n);

        /* prescan the /Subrs array to see if any of the Subrs call out to OtherSubrs */
        if (pfont->data.WeightVector.count != 0) {
            for (i = 0; i < n; ++i) {
                if ((code = pdata->procs.subr_data(pfont, i, false, &gdata)) >= 0) {
                        code = CheckSubrForMM(&gdata, pfont);
                        gs_glyph_data_free(&gdata, "write_Private(Subrs)");
                        if (code < 0) {
                            if (SubrsWithMM != 0)
                                gs_free_object(pfont->memory, SubrsWithMM, "free Subrs record");
                            return code;
                        }
                        if (SubrsWithMM != 0)
                            SubrsWithMM[i] = code;
                }
            }
        }

        for (i = 0; i < n; ++i)
            if ((code = pdata->procs.subr_data(pfont, i, false, &gdata)) >= 0) {
                char buf[50];

                if (gdata.bits.size) {
                    if (pfont->data.WeightVector.count != 0) {
                        byte *stripped = NULL;
                        int length = 0;

                        length = strip_othersubrs(&gdata, pfont, NULL, SubrsWithMM, SubrsCount);
                        if (length < 0) {
                            gs_glyph_data_free(&gdata, "write_Private(CharStrings)");
                            if (SubrsWithMM != 0)
                                 gs_free_object(pfont->memory, SubrsWithMM, "free Subrs record");
                            return length;
                        }
                        if (length > 0) {
                            stripped = gs_alloc_bytes(pfont->memory, length, "Subrs copy for OtherSubrs");
                            if (stripped == NULL) {
                                if (SubrsWithMM != 0)
                                    gs_free_object(pfont->memory, SubrsWithMM, "free Subrs record");
                                gs_glyph_data_free(&gdata, "write_Private(Subrs)");
                                return gs_note_error(gs_error_VMerror);
                            }
                            length = strip_othersubrs(&gdata, pfont, stripped, SubrsWithMM, SubrsCount);
                            if (length < 0) {
                                gs_glyph_data_free(&gdata, "write_Private(CharStrings)");
                                gs_free_object(pfont->memory, stripped, "free CharStrings copy for OtherSubrs");
                                if (SubrsWithMM != 0)
                                     gs_free_object(pfont->memory, SubrsWithMM, "free Subrs record");
                                return length;
                            }
                            gs_snprintf(buf, sizeof(buf), "dup %d %u -| ", i, length);
                            stream_puts(s, buf);
                            write_CharString(s, stripped, length);
                            gs_free_object(pfont->memory, stripped, "free Subrs copy for OtherSubrs");
                        } else {
                            gs_snprintf(buf, sizeof(buf), "dup %d 0 -| ", i);
                            stream_puts(s, buf);
                        }
                    } else {
                        gs_snprintf(buf, sizeof(buf), "dup %d %u -| ", i, gdata.bits.size);
                        stream_puts(s, buf);
                        write_CharString(s, gdata.bits.data, gdata.bits.size);
                    }
                    stream_puts(s, " |\n");
                }
                gs_glyph_data_free(&gdata, "write_Private(Subrs)");
            }
        stream_puts(s, "|-\n");
    }

    /* We don't write OtherSubrs -- there had better not be any! */

    /* Write the CharStrings. */

    {
        int num_chars = 0;
        gs_glyph glyph;
        psf_glyph_enum_t genum;
        gs_glyph_data_t gdata;
        int code;

        gdata.memory = pfont->memory;
        psf_enumerate_glyphs_begin(&genum, (gs_font *)pfont, subset_glyphs,
                                    (subset_glyphs ? subset_size : 0),
                                    GLYPH_SPACE_NAME);
        for (glyph = GS_NO_GLYPH;
             (code = psf_enumerate_glyphs_next(&genum, &glyph)) != 1;
             )
            if (code == 0 &&
                (code = pdata->procs.glyph_data(pfont, glyph, &gdata)) >= 0
                ) {
                ++num_chars;
                gs_glyph_data_free(&gdata, "write_Private(CharStrings)");
            }
        pprintd1(s, "2 index /CharStrings %d dict dup begin\n", num_chars);
        psf_enumerate_glyphs_reset(&genum);
        for (glyph = GS_NO_GLYPH;
             (code = psf_enumerate_glyphs_next(&genum, &glyph)) != 1;
            )
            if (code == 0 &&
                (code = pdata->procs.glyph_data(pfont, glyph, &gdata)) >= 0
                ) {
                gs_const_string gstr;
                int code;

                code = pfont->procs.glyph_name((gs_font *)pfont, glyph, &gstr);
                if (code < 0) {
                    if (SubrsWithMM != 0)
                        gs_free_object(pfont->memory, SubrsWithMM, "free Subrs record");
                    gs_glyph_data_free(&gdata, "write_Private(Subrs)");
                    return code;
                }

                stream_puts(s, "/");
                stream_write(s, gstr.data, gstr.size);

                if (pfont->data.WeightVector.count != 0) {
                    byte *stripped = NULL;
                    int length = 0;

                    length = strip_othersubrs(&gdata, pfont, NULL, SubrsWithMM, SubrsCount);
                    if (length < 0) {
                        gs_glyph_data_free(&gdata, "write_Private(CharStrings)");
                        if (SubrsWithMM != 0)
                             gs_free_object(pfont->memory, SubrsWithMM, "free Subrs record");
                        return length;
                    }
                    if (length > 0) {
                        stripped = gs_alloc_bytes(pfont->memory, length, "Subrs copy for OtherSubrs");
                        if (stripped == NULL) {
                            if (SubrsWithMM != 0)
                                gs_free_object(pfont->memory, SubrsWithMM, "free Subrs record");
                            gs_glyph_data_free(&gdata, "write_Private(Subrs)");
                            return gs_note_error(gs_error_VMerror);
                        }
                        length = strip_othersubrs(&gdata, pfont, stripped, SubrsWithMM, SubrsCount);
                        if (length < 0) {
                            gs_glyph_data_free(&gdata, "write_Private(CharStrings)");
                            gs_free_object(pfont->memory, stripped, "free CharStrings copy for OtherSubrs");
                            if (SubrsWithMM != 0)
                                 gs_free_object(pfont->memory, SubrsWithMM, "free Subrs record");
                            return length;
                        }
                        pprintd1(s, " %d -| ", length);
                        write_CharString(s, stripped, length);
                        gs_free_object(pfont->memory, stripped, "free CharStrings copy for OtherSubrs");
                    } else
                        pprintd1(s, " %d -| ", 0);
                } else {
                    pprintd1(s, " %d -| ", gdata.bits.size);
                    write_CharString(s, gdata.bits.data, gdata.bits.size);
                }

                stream_puts(s, " |-\n");
                gs_glyph_data_free(&gdata, "write_Private(CharStrings)");
            }
    }
    if (SubrsWithMM != 0)
        gs_free_object(pfont->memory, SubrsWithMM, "free Subrs record");

    /* Wrap up. */

    stream_puts(s, "end\nend\nreadonly put\nnoaccess put\n");
    s_release_param_printer(&rlist);
    return 0;
}

/* Encrypt and write a CharString. */
static int
stream_write_encrypted(stream *s, const void *ptr, uint count)
{
    const byte *const data = ptr;
    crypt_state state = crypt_charstring_seed;
    byte buf[50];		/* arbitrary */
    uint left, n;
    int code = 0;

    for (left = count; left > 0; left -= n) {
        n = min(left, sizeof(buf));
        gs_type1_encrypt(buf, data + count - left, n, &state);
        code = stream_write(s, buf, n);
    }
    return code;
}

/* Write one FontInfo entry. */
static void
write_font_info(stream *s, const char *key, const gs_const_string *pvalue,
                int do_write)
{
    if (do_write) {
        pprints1(s, "\n/%s ", key);
        s_write_ps_string(s, pvalue->data, pvalue->size, PRINT_HEX_NOT_OK);
        stream_puts(s, " def");
    }
}

/* Write the definition of a Type 1 font. */
int
psf_write_type1_font(stream *s, gs_font_type1 *pfont, int options,
                      gs_glyph *orig_subset_glyphs, uint orig_subset_size,
                      const gs_const_string *alt_font_name, int lengths[3])
{
    stream *es = s;
    gs_offset_t start = stell(s);
    param_printer_params_t ppp;
    printer_param_list_t rlist;
    gs_param_list *const plist = (gs_param_list *)&rlist;
    stream AXE_stream;
    stream_AXE_state AXE_state;
    byte AXE_buf[200];		/* arbitrary */
    stream exE_stream;
    stream_exE_state exE_state;
    byte exE_buf[200];		/* arbitrary */
    psf_outline_glyphs_t glyphs;
    int lenIV = pfont->data.lenIV;
    int (*write_CharString)(stream *, const void *, uint) = stream_write;
    int code = psf_get_type1_glyphs(&glyphs, pfont, orig_subset_glyphs,
                                     orig_subset_size);

    if (code < 0)
        return code;

    /* Initialize the parameter printer. */

    ppp = param_printer_params_default;
    ppp.item_suffix = " def\n";
    ppp.print_ok =
        (options & WRITE_TYPE1_ASCIIHEX ? 0 : PRINT_BINARY_OK) |
        PRINT_HEX_NOT_OK;
    code = s_init_param_printer(&rlist, &ppp, s);
    if (code < 0)
        return code;

    /* Write the font header. */

    stream_puts(s, "%!FontType1-1.0: ");
    write_font_name(s, pfont, alt_font_name, false);
    stream_puts(s, "\n11 dict begin\n");

    /* Write FontInfo. */

    stream_puts(s, "/FontInfo 5 dict dup begin");
    {
        gs_font_info_t info;
        int code = pfont->procs.font_info((gs_font *)pfont, NULL,
                        (FONT_INFO_COPYRIGHT | FONT_INFO_NOTICE |
                         FONT_INFO_FAMILY_NAME | FONT_INFO_FULL_NAME),
                                          &info);

        if (code >= 0) {
            write_font_info(s, "Copyright", &info.Copyright,
                            info.members & FONT_INFO_COPYRIGHT);
            write_font_info(s, "Notice", &info.Notice,
                            info.members & FONT_INFO_NOTICE);
            write_font_info(s, "FamilyName", &info.FamilyName,
                            info.members & FONT_INFO_FAMILY_NAME);
            write_font_info(s, "FullName", &info.FullName,
                            info.members & FONT_INFO_FULL_NAME);
        }
    }
    stream_puts(s, "\nend readonly def\n");

    /* Write the main font dictionary. */

    stream_puts(s, "/FontName ");
    write_font_name(s, pfont, alt_font_name, true);
    stream_puts(s, " def\n");
    code = write_Encoding(s, pfont, options, glyphs.subset_glyphs,
                          glyphs.subset_size, glyphs.notdef);
    if (code < 0)
        return code;
    pprintg6(s, "/FontMatrix [%g %g %g %g %g %g] readonly def\n",
             pfont->FontMatrix.xx, pfont->FontMatrix.xy,
             pfont->FontMatrix.yx, pfont->FontMatrix.yy,
             pfont->FontMatrix.tx, pfont->FontMatrix.ty);
    write_uid(s, &pfont->UID, options);
    pprintg4(s, "/FontBBox {%g %g %g %g} readonly def\n",
             pfont->FontBBox.p.x, pfont->FontBBox.p.y,
             pfont->FontBBox.q.x, pfont->FontBBox.q.y);
    {
        static const gs_param_item_t font_items[] = {
            {"FontType", gs_param_type_int,
             offset_of(gs_font_type1, FontType)},
            {"PaintType", gs_param_type_int,
             offset_of(gs_font_type1, PaintType)},
            {"StrokeWidth", gs_param_type_float,
             offset_of(gs_font_type1, StrokeWidth)},
            gs_param_item_end
        };

        code = gs_param_write_items(plist, pfont, NULL, font_items);
        if (code < 0)
            return code;
    }

    /*
     * This is nonsense. We cna't write the WeightVector alonr from a Multiple
     * Master and expect any sensible results. Since its useless alone, there's
     * no point in emitting it at all. Leaving the code in place in case we
     * decide to write MM fonts one day.
    {
        const gs_type1_data *const pdata = &pfont->data;

        write_float_array(plist, "WeightVector", pdata->WeightVector.values,
                          pdata->WeightVector.count);
    }
    */
    stream_puts(s, "currentdict end\n");

    /* Write the Private dictionary. */

    if (lenIV < 0 && (options & WRITE_TYPE1_WITH_LENIV)) {
        /* We'll have to encrypt the CharStrings. */
        lenIV = 0;
        write_CharString = stream_write_encrypted;
    }
    if (options & WRITE_TYPE1_EEXEC) {
        stream_puts(s, "currentfile eexec\n");
        lengths[0] = (int)(stell(s) - start);
        start = stell(s);
        if (options & WRITE_TYPE1_ASCIIHEX) {
            s_init(&AXE_stream, s->memory);
            s_init_state((stream_state *)&AXE_state, &s_AXE_template, NULL);
            s_init_filter(&AXE_stream, (stream_state *)&AXE_state,
                          AXE_buf, sizeof(AXE_buf), es);
            /* We have to set this after s_init_filter() as that function
             * sets it to true.
             */
            AXE_state.EndOfData = false;
            es = &AXE_stream;
        }
        s_init(&exE_stream, s->memory);
        s_init_state((stream_state *)&exE_state, &s_exE_template, NULL);
        exE_state.cstate = 55665;
        s_init_filter(&exE_stream, (stream_state *)&exE_state,
                      exE_buf, sizeof(exE_buf), es);
        es = &exE_stream;
        /*
         * Note: eexec encryption always writes/skips 4 initial bytes, not
         * the number of initial bytes given by pdata->lenIV.
         */
        stream_puts(es, "****");
    }
    code = write_Private(es, pfont, glyphs.subset_glyphs, glyphs.subset_size,
                         glyphs.notdef, lenIV, write_CharString, &ppp, options);
    if (code < 0)
        return code;
    stream_puts(es, "dup/FontName get exch definefont pop\n");
    if (options & WRITE_TYPE1_EEXEC) {
        if (options & (WRITE_TYPE1_EEXEC_PAD | WRITE_TYPE1_EEXEC_MARK))
            stream_puts(es, "mark ");
        stream_puts(es, "currentfile closefile\n");
        s_close_filters(&es, s);
        lengths[1] = (int)(stell(s) - start);
        start = stell(s);
        if (options & WRITE_TYPE1_EEXEC_PAD) {
            int i;

            for (i = 0; i < 8; ++i)
                stream_puts(s, "\n0000000000000000000000000000000000000000000000000000000000000000");
            stream_puts(s, "\ncleartomark\n");
        }
        lengths[2] = (int)(stell(s) - start);
    } else {
        lengths[0] = (int)(stell(s) - start);
        lengths[1] = lengths[2] = 0;
    }

    /* Wrap up. */

    s_release_param_printer(&rlist);
    return 0;
}
