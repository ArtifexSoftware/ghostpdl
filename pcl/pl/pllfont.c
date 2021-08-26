/* Copyright (C) 2001-2021 Artifex Software, Inc.
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


/* pclfont.c */
/* PCL5 font preloading */
#include "ctype_.h"
#include "stdio_.h"
#include "string_.h"
#include "gx.h"
#include "gxiodev.h"
#include "gp.h"
#include "gsccode.h"
#include "gserrors.h"
#include "gsmatrix.h"
#include "gsutil.h"
#include "gxfont.h"
#include "gxfont42.h"
#include "stream.h"
#include "strmio.h"
#include "plfont.h"
#include "pldict.h"
#include "pllfont.h"
#include "plftable.h"
#include "plvalue.h"
#include "plvocab.h"
#include "gxfapi.h"
#include "plfapi.h"
#include "plufstlp.h"


extern const char gp_file_name_list_separator;

/* Load some built-in fonts.  This must be done at initialization time, but
 * after the state and memory are set up.  Return an indication of whether
 * at least one font was successfully loaded.  XXX The existing code is more
 * than a bit of a hack.  Approach: expect to find some fonts in one or more
 * of a given list of directories with names *.ttf.  Load whichever ones are
 * found in the table below.  Probably very little of this code can be
 * salvaged for later.
 */

/* get the windows truetype font file name */
#define WINDOWSNAME 4
#define PSNAME 6

static int
get_name_from_tt_file(stream * tt_file, gs_memory_t * mem,
                      char *pfontfilename, int nameoffset)
{
    /* check if an open file a ttfile saving and restoring the file position */
    long pos;                   /* saved file position */
    unsigned long len;
    char *ptr = pfontfilename;
    byte *ptt_font_data;

    if ((pos = sftell(tt_file)) < 0)
        return -1;
    /* seek to end and get the file length and allocate a buffer
       for the entire file */
    if (sfseek(tt_file, 0L, SEEK_END))
        return -1;
    len = sftell(tt_file);

    /* allocate a buffer for the entire file */
    ptt_font_data = gs_alloc_bytes(mem, len, "get_name_from_tt_file");
    if (ptt_font_data == NULL)
        return_error(gs_error_VMerror);

    /* seek back to the beginning of the file and read the data
       into the buffer */
    if ((sfseek(tt_file, 0L, SEEK_SET) == 0) && (sfread(ptt_font_data, 1, len, tt_file) == len));       /* ok */
    else {
        gs_free_object(mem, ptt_font_data, "get_name_from_tt_file");
        return -1;
    }

    {
        /* find the "name" table */
        byte *pnum_tables_data = ptt_font_data + 4;
        byte *ptable_directory_data = ptt_font_data + 12;
        int table;

        for (table = 0; table < pl_get_uint16(pnum_tables_data); table++)
            if (!memcmp(ptable_directory_data + (table * 16), "name", 4)) {
                unsigned int offset =
                    pl_get_uint32(ptable_directory_data + (table * 16) + 8);
                byte *name_table = ptt_font_data + offset;
                /* the offset to the string pool */
                unsigned short storageOffset = pl_get_uint16(name_table + 4);
                byte *name_recs = name_table + 6;

                {
                    /* 4th entry in the name table - the complete name */
                    unsigned short length =
                        pl_get_uint16(name_recs + (12 * nameoffset) + 8);
                    unsigned short offset =
                        pl_get_uint16(name_recs + (12 * nameoffset) + 10);
                    int k;

                    for (k = 0; k < length; k++) {
                        /* hack around unicode if necessary */
                        int c = name_table[storageOffset + offset + k];

                        if (isprint(c))
                            *ptr++ = (char)c;
                    }
                }
                break;
            }
    }
    /* free up the data and restore the file position */
    gs_free_object(mem, ptt_font_data, "get_name_from_tt_file");
    if (sfseek(tt_file, pos, SEEK_SET) < 0)
        return -1;
    /* null terminate the fontname string and return success.  Note
       the string can be 0 length if no fontname was found. */
    *ptr = '\0';

    /* trim trailing white space */
    {
        int i = strlen(pfontfilename);

        while (--i >= 0) {
            if (!isspace(pfontfilename[i]))
                break;
        }
        pfontfilename[++i] = '\0';
    }

    return 0;
}

#ifdef DEBUG
static void
check_resident_ufst_fonts(pl_dict_t * pfontdict,
                          bool use_unicode_names_for_keys, gs_memory_t * mem)
{
    int j;

    for (j = 0;
         *pl_built_in_resident_font_table[j].full_font_name[AGFANAME] != 0
         && j < pl_built_in_resident_font_table_count; j++) {
        void *value;

        /* lookup unicode key in the resident table */
        if (use_unicode_names_for_keys) {
            if (!pl_dict_lookup(pfontdict,
                                (const byte *)pl_built_in_resident_font_table[j].
                                unicode_fontname,
                                sizeof(pl_built_in_resident_font_table[j].unicode_fontname),
                                &value, true,
                                NULL) /* return data ignored */ ) {
                int i;

                dmprintf(mem, "Font with unicode key: ");
                for (i = 0;
                     i <
                     sizeof(pl_built_in_resident_font_table[j].unicode_fontname) /
                     sizeof(pl_built_in_resident_font_table[j].unicode_fontname[0]); i++) {
                    dmprintf1(mem, "%c",
                              (char)pl_built_in_resident_font_table[j].unicode_fontname[i]);
                }
                dmprintf1(mem,
                          " not available in font dictionary, resident table position: %d\n",
                          j);
            }
        } else {
            byte key[3];

            key[2] = (byte) j;
            key[0] = key[1] = 0;
            if (!pl_dict_lookup(pfontdict,
                                key,
                                sizeof(key),
                                &value, true,
                                NULL) /* return data ignored */ )
                dmprintf2(mem,
                          "%s not available in font dictionary, resident table position: %d\n",
                          pl_built_in_resident_font_table[j].full_font_name, j);
        }
    }
    return;
}

static void
check_resident_fonts(pl_dict_t * pfontdict, gs_memory_t * mem)
{
    int i;

    for (i = 0;
         *pl_built_in_resident_font_table[i].full_font_name[URWNAME] != 0
         && i < pl_built_in_resident_font_table_count; i++)
        if (!pl_lookup_font_by_pjl_number(pfontdict, i)) {
            int j;

            dmprintf2(mem, "%s (entry %d) not found\n",
                      pl_built_in_resident_font_table[i].full_font_name[URWNAME], i);
            dmprintf(mem, "pxl unicode name:");
            for (j = 0; j < countof(pl_built_in_resident_font_table[i].unicode_fontname); j++)
                dmprintf1(mem, "'%c'", pl_built_in_resident_font_table[i].unicode_fontname[j]);
            dmprintf(mem, "\n");
        }
}
#endif

/* Load a built-in AGFA MicroType font */
static int
pl_fill_in_mt_font(gs_font_base * pfont, pl_font_t * plfont, ushort handle,
                   char *fco_path, gs_font_dir * pdir, gs_memory_t * mem,
                   long unique_id)
{
    int code = 0;

    if (pfont == 0 || plfont == 0)
        code = gs_note_error(gs_error_VMerror);
    else {                      /* Initialize general font boilerplate. */
        code =
            pl_fill_in_font((gs_font *) pfont, plfont, pdir, mem,
                            "illegal_font");
        if (code >= 0) {        /* Initialize MicroType font boilerplate. */
            plfont->header = 0;
            plfont->header_size = 0;
            plfont->scaling_technology = plfst_MicroType;
            plfont->font_type = plft_Unicode;
            plfont->large_sizes = true;
            plfont->is_xl_format = false;
            plfont->allow_vertical_substitutes = false;

            gs_make_identity(&pfont->FontMatrix);
            pfont->FontMatrix.xx = pfont->FontMatrix.yy = 0.001f;
            pfont->FontType = ft_MicroType;
            pfont->BitmapWidths = true;
            pfont->ExactSize = fbit_use_outlines;
            pfont->InBetweenSize = fbit_use_outlines;
            pfont->TransformedChar = fbit_use_outlines;

            pfont->FontBBox.p.x = pfont->FontBBox.p.y =
                pfont->FontBBox.q.x = pfont->FontBBox.q.y = 0;

            uid_set_UniqueID(&pfont->UID, unique_id | ( ((long) handle) << 16));
            pfont->encoding_index = 1;      /****** WRONG ******/
            pfont->nearest_encoding_index = 1;      /****** WRONG ******/
        }
    }
    return (code);
}

int
pl_load_ufst_lineprinter(gs_memory_t * mem, pl_dict_t * pfontdict,
                         gs_font_dir * pdir, int storage,
                         bool use_unicode_names_for_keys)
{
    int i;

    for (i = 0;
         *pl_built_in_resident_font_table[i].full_font_name[AGFANAME] != 0
         && i < pl_built_in_resident_font_table_count; i++) {
        if (pl_built_in_resident_font_table[i].params.typeface_family == 0) {
            const byte *header = NULL;
            const byte *char_data = NULL;
            pl_font_t *pplfont =
                pl_alloc_font(mem, "pl_load_ufst_lineprinter pplfont");
            gs_font_base *pfont =
                gs_alloc_struct(mem, gs_font_base, &st_gs_font_base,
                                "pl_load_ufst_lineprinter pfont");
            int code;

            pl_get_ulp_character_data((byte **) & header,
                                      (byte **) & char_data);

            if (!header || !char_data) {
                return -1;
            }

            /* these shouldn't happen during system setup */
            if (pplfont == 0 || pfont == 0)
                return -1;
            if (pl_fill_in_font
                ((gs_font *) pfont, pplfont, pdir, mem,
                 "lineprinter_font") < 0)
                return -1;

            pl_fill_in_bitmap_font(pfont, gs_next_ids(mem, 1));
            pplfont->params = pl_built_in_resident_font_table[i].params;
            memcpy(pplfont->character_complement,
                   pl_built_in_resident_font_table[i].character_complement, 8);

            if (use_unicode_names_for_keys)
                code = pl_dict_put(pfontdict,
                            (const byte *)pl_built_in_resident_font_table[i].unicode_fontname,
                            sizeof(pl_built_in_resident_font_table[i].unicode_fontname), pplfont);
            else {
                byte key[3];

                key[2] = (byte) i;
                key[0] = key[1] = 0;
                code = pl_dict_put(pfontdict, key, sizeof(key), pplfont);
            }
            if (code < 0)
                return code;
            pplfont->storage = storage; /* should be an internal font */
            pplfont->data_are_permanent = true;
            pplfont->header = (byte *) header;
            pplfont->font_type = plft_8bit_printable;
            pplfont->scaling_technology = plfst_bitmap;
            pplfont->is_xl_format = false;
            pplfont->resolution.x = pplfont->resolution.y = 300;

            code = pl_font_alloc_glyph_table(pplfont, 256, mem,
                                             "pl_load_ufst_lineprinter pplfont (glyph table)");
            if (code < 0)
                return code;

            while (1) {

                uint width = pl_get_uint16(char_data + 12);
                uint height = pl_get_uint16(char_data + 14);
                uint ccode_plus_header_plus_data =
                    2 + 16 + (((width + 7) >> 3) * height);
                uint ucode =
                    pl_map_MSL_to_Unicode(pl_get_uint16(char_data), 0);
                int code = 0;

                /* NB this shouldn't happen but it does, should be
                   looked at */
                if (ucode != 0xffff)
                    code = pl_font_add_glyph(pplfont, ucode, char_data + 2, ccode_plus_header_plus_data);

                if (code < 0)
                    /* shouldn't happen */
                    return -1;
                /* calculate the offset of the next character code in the table */
                char_data += ccode_plus_header_plus_data;

                /* char code 0 is end of table */
                if (pl_get_uint16(char_data) == 0)
                    break;
            }
            code = gs_definefont(pdir, (gs_font *) pfont);
            if (code < 0)
                /* shouldn't happen */
                return -1;
        }
    }
    return 0;
}

static int
pl_load_built_in_mtype_fonts(const char *pathname, gs_memory_t * mem,
                             pl_dict_t * pfontdict, gs_font_dir * pdir,
                             int storage, bool use_unicode_names_for_keys)
{
    int i, k;
    short status = 0;
    int bSize;
    byte key[3];
    char pthnm[1024];
    char *ufst_root_dir;
    char *fco;
    char *fco_start, *fco_lim;
    pl_font_t *plfont = NULL;
    gs_font *pfont = NULL;
    gs_font_base *pbfont;

    /* don't load fonts more than once */
    if (pl_dict_length(pfontdict, true) > 0)
        return 1;

    if (!pl_fapi_ufst_available(mem)) {
        return (0);
    }


    /*
     * Open and install the various font collection objects.
     *
     * For each font collection object, step through the object until it is
     * exhausted, placing any fonts found in the built_in_fonts dictionary.
     *
     */
    ufst_root_dir = (char *)pl_fapi_ufst_get_font_dir(mem);
    fco_start = fco = (char *)pl_fapi_ufst_get_fco_list(mem);
    fco_lim = fco_start + strlen(fco_start) + 1;

    for (k = 0; fco < fco_lim && strlen(fco) > 0; k++) {
        status = 0;
        /* build and open (get handle) for the k'th fco file name */
        gs_strlcpy((char *)pthnm, ufst_root_dir, sizeof pthnm);

        for (i = 2; fco[i] != gp_file_name_list_separator && (&fco[i]) < fco_lim - 1; i++)
            ;

        strncat(pthnm, fco, i);
        fco += (i + 1);

        /* enumerat the files in this fco */
        for (i = 0; status == 0; i++, key[2] += 1) {
            char *pname = NULL;

            /* If we hit a font we're not going to use, we'll reuse the allocated
             * memory.
             */
            if (!plfont) {

                pbfont =
                    gs_alloc_struct(mem, gs_font_base, &st_gs_font_base,
                                    "pl_mt_load_font(gs_font_base)");
                plfont = pl_alloc_font(mem, "pl_mt_load_font(pl_font_t)");
                if (!pbfont || !plfont) {
                    gs_free_object(mem, plfont, "pl_mt_load_font(pl_font_t)");
                    gs_free_object(mem, pfont,
                                   "pl_mt_load_font(gs_font_base)");
                    dmprintf1(mem, "VM error for built-in font %d", i);
                    continue;
                }
            }
            pfont = (gs_font *) pbfont;

            status =
                pl_fill_in_mt_font(pbfont, plfont, i, pthnm, pdir, mem, i);
            if (status < 0) {
                dmprintf2(mem, "Error %d for built-in font %d", status, i);
                continue;
            }

            status =
                pl_fapi_passfont(plfont, i, (char *)"UFST", pthnm, NULL, 0);

            if (status != 0) {
#ifdef DEBUG
                dmprintf1(mem, "CGIFfco_Access error %d\n", status);
#endif
            } else {
                int font_number = 0;
                /* unfortunately agfa has 2 fonts named symbol.  We
                   believe the font with internal number, NB, NB, NB  */
                char *symname = (char *)"SymbPS";
                int j;
                uint spaceBand;
                uint scaleFactor;
                bool used = false;
                int code = 0;

                /* For Microtype fonts, once we get here, these
                 * pl_fapi_get*() calls cannot fail, so we can
                 * safely ignore the return value
                 */
                (void)pl_fapi_get_mtype_font_name(pfont, NULL, &bSize);

                pname =
                    (char *)gs_alloc_bytes(mem, bSize,
                                           "pl_mt_load_font: font name buffer");
                if (!pname) {
                    dmprintf1(mem, "VM Error for built-in font %d", i);
                    continue;
                }

                (void)pl_fapi_get_mtype_font_name(pfont, (byte *) pname,
                                                  &bSize);

                (void)pl_fapi_get_mtype_font_number(pfont, &font_number);
                (void)pl_fapi_get_mtype_font_spaceBand(pfont, &spaceBand);
                (void)pl_fapi_get_mtype_font_scaleFactor(pfont, &scaleFactor);

                if (font_number == 24463) {
                    gs_free_object(mem, pname,
                                   "pl_mt_load_font: font name buffer");
                    pname = symname;
                }

                for (j = 0; *pl_built_in_resident_font_table[j].full_font_name[AGFANAME]; j++) {
                    if (strcmp
                        ((char *)pl_built_in_resident_font_table[j].full_font_name[AGFANAME],
                         (char *)pname) != 0)
                        continue;

#ifdef DEBUG
                    if (gs_debug_c('='))
                        dmprintf2(mem, "Loading %s from fco %s\n", pname,
                                  pthnm);
#endif
                    /* Record the differing points per inch value
                       for Intellifont derived fonts. */

                    if (scaleFactor == 8782) {
                        plfont->pts_per_inch = 72.307f;
                    }

                    plfont->font_type = pl_built_in_resident_font_table[j].font_type;
                    plfont->storage = storage;
                    plfont->data_are_permanent = false;
                    plfont->params = pl_built_in_resident_font_table[j].params;

                    /*
                     * NB: though the TTFONTINFOTYPE structure has a
                     * pcltChComp field, it is not filled in by the UFST
                     * code (which just initializes it to 0). Hence, the
                     * hard-coded information in the resident font
                     * initialization structure is used.
                     */
                    memcpy(plfont->character_complement,
                           pl_built_in_resident_font_table[j].character_complement, 8);

                    status = gs_definefont(pdir, (gs_font *) pfont);
                    if (status < 0) {
                        status = 0;
                        continue;
                    }
                    status =
                        pl_fapi_passfont(plfont, i, (char *)"UFST", pthnm,
                                         NULL, 0);
                    if (status < 0) {
                        status = 0;
                        continue;
                    }
                    if (use_unicode_names_for_keys)
                        code = pl_dict_put(pfontdict,
                                    (const byte *)pl_built_in_resident_font_table[j].unicode_fontname,
                                    sizeof(pl_built_in_resident_font_table[j].unicode_fontname),
                                    plfont);
                    else {
                        key[2] = (byte) j;
                        key[0] = key[1] = 0;
                        code = pl_dict_put(pfontdict, key, sizeof(key), plfont);
                    }
                    if (code < 0)
                        return code;
                    used = true;
                }
                /* If we've stored the font, null the local reference */
                if (used) {
                    plfont = NULL;
                    pfont = NULL;
                }
                if (pname != symname)
                    gs_free_object(mem, pname,
                                   "pl_mt_load_font: font name buffer");
                pname = NULL;
            }
        }
    }                           /* end enumerate fco loop */

    gs_free_object(mem, plfont, "pl_mt_load_font(pl_font_t)");
    gs_free_object(mem, pfont, "pl_mt_load_font(gs_font_base)");

    /* finally add lineprinter NB return code ignored */
    (void)pl_load_ufst_lineprinter(mem, pfontdict, pdir, storage,
                                   use_unicode_names_for_keys);

#ifdef DEBUG
    if (gs_debug_c('='))
        check_resident_ufst_fonts(pfontdict, use_unicode_names_for_keys, mem);
#endif

    if (status == 0 || status == -10)
        return (1);

    return (0);
}


/* NOTES ABOUT NB NB - if the font dir necessary */
int
pl_load_built_in_fonts(const char *pathname, gs_memory_t * mem,
                       pl_dict_t * pfontdict, gs_font_dir * pdir,
                       int storage, bool use_unicode_names_for_keys)
{
    const font_resident_t *residentp;
    /* get rid of this should be keyed by pjl font number */
    byte key[3];
    char path[1024];
    bool found;
    bool found_any = false;
    file_enum *fe;
    int code = 0;

    if ((code =
         pl_load_built_in_mtype_fonts(pathname, mem, pfontdict, pdir, storage,
                                      use_unicode_names_for_keys))) {
        return (code);
    }

    /* don't load fonts more than once */
    if (pl_dict_length(pfontdict, true) > 0)
        return 1;

    if (pathname == NULL)
        return 0;

    if (gs_strlcpy(path, pathname, sizeof(path)) >= sizeof(path))
        return 0;

    if (gs_strlcat(path, "*", sizeof(path)) >= sizeof(path))
        return 0;

    fe = gs_enumerate_files_init(mem, path, strlen(path));
    if (fe == NULL)
        return 0;

    /* loop through the files */
    while ((code = gs_enumerate_files_next(mem, fe, path, sizeof(path) - 1)) >= 0) {
        char buffer[1024];
        pl_font_t *plfont;
        stream *in;

        if (code > sizeof(path) - 1) {
            dmprintf(mem,
                     "filename length exceeds file name storage buffer length\n");
            continue;
        }
        /* null terminate the string */
        path[code] = '\0';

        in = sfopen(path, "r", mem);
        if (in == NULL) {   /* shouldn't happen */
            dmprintf1(mem, "cannot open file %s\n", path);
            continue;
        }

        code = get_name_from_tt_file(in, mem, buffer, PSNAME);
        if (code < 0) {
            dmprintf1(mem, "input output failure on TrueType File %s\n",
                      path);
            sfclose(in);
            continue;
        }

        if (strlen(buffer) == 0) {
            dmprintf1(mem,
                      "could not extract font file name from file %s\n",
                      path);
            sfclose(in);
            continue;
        }

        /* lookup the font file name in the resident table */
        found = false;
        for (residentp = pl_built_in_resident_font_table;
             *residentp->full_font_name[URWNAME]; ++residentp) {
            if (strcmp(buffer, residentp->full_font_name[URWNAME]) != 0)
                continue;
            /* load the font file into memory.  NOTE: this closes the file - argh... */
            if (pl_load_tt_font(in, pdir, mem,
                                gs_next_ids(mem, 1), &plfont,
                                buffer) < 0) {
                /* vm error */
                gs_enumerate_files_close(mem, fe);
                return gs_throw1(0,
                                 "An unrecoverable failure occurred while reading the resident font %s\n",
                                 path);
            }
            /* reopen the file */
            in = sfopen(path, "r", mem);
            if (in == NULL) {
                gs_free_object(mem, plfont->pfont, "pl_tt_load_font(gs_font_type42)");
                pl_free_tt_fontfile_buffer(mem, plfont->header);
                gs_free_object(mem, plfont, "pl_tt_load_font(pl_font_t)");
                gs_enumerate_files_close(mem, fe);
                return gs_throw1(0,
                                 "An unrecoverable failure occurred while reading the resident font %s\n",
                                 path);
            }

            plfont->storage = storage;
            plfont->data_are_permanent = false;

            /* use the offset in the table as the pjl font number */
            /* for unicode keying of the dictionary use the unicode
               font name, otherwise use the keys. */
            plfont->font_type = residentp->font_type;
            plfont->params = residentp->params;
            memcpy(plfont->character_complement,
                   residentp->character_complement, 8);
            if (use_unicode_names_for_keys)
                code = pl_dict_put(pfontdict,
                                   (const byte *)residentp->unicode_fontname,
                                   sizeof(residentp->unicode_fontname),
                                   plfont);
            else {
                key[2] = (byte) (residentp - pl_built_in_resident_font_table);
                key[0] = key[1] = 0;
                code = pl_dict_put(pfontdict, key, sizeof(key), plfont);
                /* leave data stored in the file.  NB this should be a fatal error also. */
                if ((code >= 0) && pl_store_resident_font_data_in_file
                    (path, mem, plfont) < 0) {
                    dmprintf1(mem, "%s could not store data",
                              path);
                    continue;
                }
            }
            if (code < 0) {
                /* on error, pl_dict_put consumes plfont */
                continue;
            }
            found = true;
            found_any = true;
        }
        sfclose(in);

        /* nothing found */
        if (!found) {
#ifdef DEBUG
            if (gs_debug_c('=')) {
                dmprintf2(mem,
                          "TrueType font %s in file %s not found in table\n",
                          buffer, path);
                in = sfopen(path, "r", mem);
                code =
                    get_name_from_tt_file(in, mem, buffer, WINDOWSNAME);
                sfclose(in);
                dmprintf1(mem, "Windows name %s\n", buffer);
                if (code < 0)
                    return code;
            }
#endif
        }
    }                       /* next file */
#ifdef DEBUG
    if (gs_debug_c('='))
        check_resident_fonts(pfontdict, mem);
#endif
    return found_any;
}

/* These are not implemented */

/* load simm fonts given a path */
int
pl_load_simm_fonts(const char *pathname, gs_memory_t * mem,
                   pl_dict_t * pfontdict, gs_font_dir * pdir, int storage)
{
    /* not implemented */
    return 0;
}

/* load simm fonts given a path */
int
pl_load_cartridge_fonts(const char *pathname, gs_memory_t * mem,
                        pl_dict_t * pfontdict, gs_font_dir * pdir,
                        int storage)
{
    /* not implemented */
    return 0;
}
