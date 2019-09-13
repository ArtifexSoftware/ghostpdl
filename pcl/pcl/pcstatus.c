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


/* pcstatus.c - PCL5 status readback commands */

#include "memory_.h"
#include "stdio_.h"
#include <stdarg.h>             /* how to make this portable? */
#include "string_.h"
#include "gsmemory.h"
#include "gsmalloc.h"
#include "pcommand.h"
#include "pcstate.h"
#include "pcfont.h"
#include "pcsymbol.h"
#include "pcparse.h"
#include "pcpatrn.h"
#include "pcuptrn.h"
#include "pcpage.h"
#include "pcursor.h"
#include "stream.h"

#define STATUS_BUFFER_SIZE 10000

/* Internal routines */

static int status_add_symbol_id(ushort *, int, ushort);

/* Read out from the status buffer. */
/* Return the number of bytes read. */
uint
pcl_status_read(byte * data, uint max_data, pcl_state_t * pcs)
{
    uint count = min(max_data,
                     pcs->status.write_pos - pcs->status.read_pos);

    if (count)
        memcpy(data, pcs->status.buffer + pcs->status.read_pos, count);
    pcs->status.read_pos += count;
    if (pcs->status.read_pos == pcs->status.write_pos) {
        gs_free_object(pcs->memory, pcs->status.buffer, "status buffer");
        pcs->status.write_pos = pcs->status.read_pos = 0;
    }
    return count;
}

/* Write a string on a stream. */
static void
stputs(stream * s, const char *str)
{
    uint ignore_count;

    sputs(s, (const byte *)str, strlen(str), &ignore_count);
}

/* printf on a stream. */
/**** THIS SHOULD BE IN THE STREAM PACKAGE. ****/
static void
stprintf(stream * s, const char *fmt, ...)
{
    uint count;
    va_list args;
    char buf[1024];

    va_start(args, fmt);
    count = gs_vsprintf(buf, fmt, args);
    sputs(s, (const byte *)buf, count, &count);
    va_end(args);
}

/* Set up a stream for writing into the status buffer. */
static void
status_begin(stream * s, pcl_state_t * pcs)
{
    byte *buffer = pcs->status.buffer;

    if (pcs->status.read_pos > 0) {
        memmove(buffer, buffer + pcs->status.read_pos,
                pcs->status.write_pos - pcs->status.read_pos);
        pcs->status.write_pos -= pcs->status.read_pos;
        pcs->status.read_pos = 0;
    }
    if (buffer == 0) {
        buffer = gs_alloc_bytes(pcs->memory, STATUS_BUFFER_SIZE,
                                "status buffer");
        pcs->status.buffer = buffer;
    }
    if (buffer == 0)
        swrite_string(s, pcs->status.internal_buffer,
                      sizeof(pcs->status.internal_buffer));
    else
        swrite_string(s, buffer, gs_object_size(pcs->memory, buffer));
    sseek(s, pcs->status.write_pos);
    stputs(s, "PCL\r\n");
}

/* Add an ID to a list being written. */
static void
status_put_id(stream * s, const char *title, const char *id)
{                               /* HACK: we know that there's at least one character in the buffer. */
    if (*s->cursor.w.ptr == '\n') {     /* We haven't started the list yet. */
        stprintf(s, "%s=\"%s", title, id);
    } else {
        stprintf(s, ",%s", id);
    }
}

/* Finish writing an ID list. */
static void
status_end_id_list(stream * s)
{                               /* HACK: we know that there's at least one character in the buffer. */
    if (*s->cursor.w.ptr != '\n')
        stputs(s, "\"\r\n");
}

static void
status_print_idlist(stream * s, const ushort * idlist, int nid,
                    const char *title)
{
    int i;

    for (i = 0; i < nid; i++) {
        char idstr[6];          /* ddddL and a null */
        int n, l;

        n = idlist[i] >> 6;
        l = (idlist[i] & 077) + 'A' - 1;
        gs_sprintf(idstr, "%d%c", n, l);
        status_put_id(s, title, idstr);
    }
    status_end_id_list(s);
}

/* Output a number, at most two decimal places, but trimming trailing 0's
 * and possibly the ".".  Want to match HP's output as closely as we can. */
static void
status_put_floating(stream * s, double v)
{                               /* Figure the format--easier than printing and chipping out the
                                 * chars we need. */
    int vf = (int)(v * 100 + ((v < 0) ? -0.5 : 0.5));

    if (vf / 100 * 100 == vf)
        stprintf(s, "%d", vf / 100);
    else if (vf / 10 * 10 == vf)
        stprintf(s, "%.1f", v);
    else
        stprintf(s, "%.2f", v);
}

/* Print font status information. */
/* font_set = -1 for font list, 0 or 1 for current font. */
static int
status_put_font(stream * s, pcl_state_t * pcs,
                uint font_id, uint internal_id,
                pl_font_t * plfont, int font_set, bool extended)
{
    char paren = (font_set > 0 ? ')' : '(');
    bool proportional = plfont->params.proportional_spacing;

    /* first escape sequence: symbol-set selection */
    stputs(s, "SELECT=\"");
    if (pl_font_is_bound(plfont) || font_set > 0) {
        /* Bound or current font, put out the symbol set. */
        uint symbol_set = font_set > 0 ?
            pcs->font_selection[font_set].params.symbol_set :
            plfont->params.symbol_set;
        stprintf(s, "<Esc>%c%u%c", paren, symbol_set >> 5,
                 (symbol_set & 31) + 'A' - 1);
    }

    /* second escape sequence: font selection */
    stprintf(s, "<Esc>%cs%dp", paren, proportional);
    if (plfont->scaling_technology == plfst_bitmap) {   /* Bitmap font */
        status_put_floating(s, pl_fp_pitch_per_inch(&plfont->params));
        stputs(s, "h");
        status_put_floating(s, plfont->params.height_4ths / 4.0);
        stputs(s, "v");
    } else {
        /* Scalable font: output depends on whether selected */
        if (font_set > 0) {
            /* If selected, we have to cheat and reach up for info;
             * plfont is below where the scaled values exist. */
            if (proportional) {
                status_put_floating(s,
                                    pcs->font_selection[font_set].params.
                                    height_4ths / 4.0);
                stputs(s, "h");
            } else {
                status_put_floating(s,
                                    pl_fp_pitch_per_inch(&pcs->
                                                         font_selection
                                                         [font_set].params));
                stputs(s, "v");
            }
        } else {
            stputs(s, proportional ? "__v" : "__h");
        }
    }
    stprintf(s, "%ds%db%uT", plfont->params.style,
             plfont->params.stroke_weight, plfont->params.typeface_family);
    if (plfont->storage & pcds_downloaded)
        stprintf(s, "<Esc>%c%uX", paren, font_id);
    stputs(s, "\"\r\n");
    if (!pl_font_is_bound(plfont) && font_set < 0) {
        int nid;
        ushort *idlist;
        pl_dict_enum_t denum;
        gs_const_string key;
        void *value;

        idlist = (ushort *) gs_alloc_bytes(pcs->memory,
                                           pl_dict_length(&pcs->
                                                          soft_symbol_sets,
                                                          false) +
                                           pl_dict_length(&pcs->
                                                          built_in_symbol_sets,
                                                          false),
                                           "status_fonts(idlist)");
        if (idlist == NULL)
            return e_Memory;
        nid = 0;
        /* Current fonts show the symbol set bound to them, above. */

        /* NOTE: Temporarily chain soft, built-in symbol sets.  DON'T
         * exit this section without unchaining them. */
        pl_dict_set_parent(&pcs->soft_symbol_sets,
                           &pcs->built_in_symbol_sets);
        pl_dict_enum_begin(&pcs->soft_symbol_sets, &denum);
        while (pl_dict_enum_next(&denum, &key, &value)) {
            pcl_symbol_set_t *ssp = (pcl_symbol_set_t *) value;
            pl_glyph_vocabulary_t gx;

            for (gx = plgv_MSL; gx < plgv_next; gx++)
                if (ssp->maps[gx] != NULL &&
                    pcl_check_symbol_support(ssp->maps[gx]->
                                             character_requirements,
                                             plfont->character_complement)) {
                    nid =
                        status_add_symbol_id(idlist, nid,
                                             (ssp->maps[gx]->id[0] << 8) +
                                             ssp->maps[gx]->id[1]);
                    break;      /* one will suffice */
                }
        }
        pl_dict_set_parent(&pcs->soft_symbol_sets, NULL);
        /* Symbol sets are back to normal. */

        gs_free_object(pcs->memory, (void *)idlist, "status_fonts(idlist)");
    }
    if (extended) {             /* Put out the "internal ID number". */
        if (plfont->storage & pcds_temporary)
            stputs(s, "DEFID=NONE\r\n");
        else {
            stputs(s, "DEFID=\"");
            if (plfont->storage & pcds_all_cartridges) {
                int c;
                int n = (plfont->storage & pcds_all_cartridges) >>
                    pcds_cartridge_shift;

                /* pick out the bit index of the cartridge */
                for (c = 0; (n & 1) == 0; c++)
                    n >>= 1;
                stprintf(s, "C%d ", c);
            } else if (plfont->storage & pcds_all_simms) {
                int m;
                int n = (plfont->storage & pcds_all_simms) >> pcds_simm_shift;

                /* pick out the bit index of the SIMM */
                for (m = 0; (n & 1) == 0; m++)
                    n >>= 1;
                stprintf(s, "M%d ", m);
            } else
                /* internal _vs_ permanent soft */
                stputs(s, (plfont->storage & pcds_internal) ? "I " : "S ");
            stprintf(s, "%d\"\r\n", internal_id);
        }

        /* XXX Put out the font name - we need a way to get the name
         * for fonts that weren't downloaded, hence lack the known
         * header field. */
        if ((plfont->storage & pcds_downloaded) && plfont->header != NULL) {
            /* Wire in the size of the FontName field (16)--it can't
             * change anyway, and this saves work. */
            pcl_font_header_t *hdr = (pcl_font_header_t *) (plfont->header);

            stprintf(s, "NAME=\"%.16s\"\r\n", hdr->FontName);
        }
    }
    return 0;
}

/* Finish writing status. */
/* If we overflowed the buffer, store an error message. */
static void
status_end(stream * s, pcl_state_t * pcs)
{
    if (sendwp(s)) {            /* Overrun.  Scan back to the last EOL that leaves us */
        /* enough room for the error line. */
        static const char *error_line = "ERROR=INTERNAL ERROR\r\n";
        int error_size = strlen(error_line) + 1;
        uint limit = gs_object_size(pcs->memory, pcs->status.buffer);
        uint wpos = stell(s);

        while (limit - wpos < error_size ||
               pcs->status.buffer[wpos - 1] != '\n')
            --wpos;
        s->end_status = 0;      /**** SHOULDN'T BE NECESSARY ****/
        sseek(s, wpos);
        stputs(s, error_line);
    }
    sputc(s, FF);
    pcs->status.write_pos = stell(s);
}

/* Status readouts */
/* storage = 0 means currently selected, otherwise it is a mask. */

static int
status_do_fonts(stream * s, pcl_state_t * pcs,
                pcl_data_storage_t storage, bool extended)
{
    gs_const_string key;
    void *value;
    pl_dict_enum_t denum;
    int res;

    pl_dict_enum_begin(&pcs->soft_fonts, &denum);
    while (pl_dict_enum_next(&denum, &key, &value)) {
        uint id = (key.data[0] << 8) + key.data[1];

        if ((((pl_font_t *) value)->storage & storage) != 0 ||
            (storage == 0 && pcs->font == (pl_font_t *) value)
            ) {
            res = status_put_font(s, pcs, id, id, (pl_font_t *) value,
                                  (storage != 0 ? -1 : pcs->font_selected),
                                  extended);
            if (res != 0)
                return res;
        }
    }
    return 0;
}

static int
status_fonts(stream * s, pcl_state_t * pcs, pcl_data_storage_t storage)
{
    return status_do_fonts(s, pcs, storage, false);
}

static int
status_macros(stream * s, pcl_state_t * pcs, pcl_data_storage_t storage)
{
    gs_const_string key;
    void *value;
    pl_dict_enum_t denum;

    if (storage == 0)
        return 0;               /* no "currently selected" macro */
    pl_dict_enum_begin(&pcs->macros, &denum);
    while (pl_dict_enum_next(&denum, &key, &value))
        if (((pcl_macro_t *) value)->storage & storage) {
            char id_string[6];

            gs_sprintf(id_string, "%u", (key.data[0] << 8) + key.data[1]);
            status_put_id(s, "IDLIST", id_string);
        }
    status_end_id_list(s);
    return 0;
}

/*
 * Get a list of current provided patterns in the given storage class(es).
 * The pattern storage dictionary is now static, and provides no externally
 * visible enumeration; hence this operation is rather crudely implemented.
 */
static int
status_patterns(stream * s, pcl_state_t * pcs, pcl_data_storage_t storage)
{
    if (storage == 0) {
        int id = pcs->current_pattern_id;
        pcl_pattern_t *pptrn = pcl_pattern_get_pcl_uptrn(pcs, id);

        if ((pptrn != 0) && (pcs->pattern_type == pcl_pattern_user_defined)) {
            char id_string[6];

            gs_sprintf(id_string, "%u", id);
            status_put_id(s, "IDLIST", id_string);
        }
    } else {
        int id;

        for (id = 0; id < (1L << 15) - 1; id++) {
            pcl_pattern_t *pptrn = pcl_pattern_get_pcl_uptrn(pcs, id);

            if (pptrn != 0) {
                char id_string[6];

                gs_sprintf(id_string, "%u", id);
                status_put_id(s, "IDLIST", id_string);
            }
        }
    }
    status_end_id_list(s);
    return 0;
}

static bool                     /* Is this symbol map supported by any relevant font? */
status_check_symbol_set(pcl_state_t * pcs, pl_symbol_map_t * mapp,
                        pcl_data_storage_t storage)
{
    gs_const_string key;
    void *value;
    pl_dict_enum_t fenum;

    pl_dict_enum_begin(&pcs->soft_fonts, &fenum);
    while (pl_dict_enum_next(&fenum, &key, &value)) {
        pl_font_t *fp = (pl_font_t *) value;

        if (fp->storage != storage)
            continue;
        if (pcl_check_symbol_support(mapp->character_requirements,
                                     fp->character_complement))
            return true;
    }
    return false;
}

static int                      /* add symbol set ID to list (insertion), return new length */
status_add_symbol_id(ushort * idlist, int nid, ushort new_id)
{
    int i;
    ushort *idp;
    ushort t1, t2;

    for (i = 0, idp = idlist; i < nid; i++)
        if (new_id <= *idp)
            break;
    if (new_id == *idp)         /* duplicate item */
        return nid;
    /* insert new_id in front of *idp */
    for (t1 = new_id; i < nid; i++) {
        t2 = *idp;
        *idp++ = t1;
        t1 = t2;
    }
    *idp = t1;
    return nid + 1;
}

static int
status_symbol_sets(stream * s, pcl_state_t * pcs, pcl_data_storage_t storage)
{
    gs_const_string key;
    void *value;
    pl_dict_enum_t denum;
    ushort *idlist;
    int nid;

    if (storage == 0)
        return 0;               /* no "currently selected" symbol set */

    /* Note carefully the meaning of this status inquiry.  First,
     * we return only symbol sets applicable to unbound fonts.  Second,
     * the "storage" value refers to the location of fonts. */

    /* total up built-in symbol sets, downloaded ones */
    nid = pl_dict_length(&pcs->soft_symbol_sets, false) +
        pl_dict_length(&pcs->built_in_symbol_sets, false);
    idlist = (ushort *) gs_alloc_bytes(pcs->memory, nid * sizeof(ushort),
                                       "status_symbol_sets(idlist)");
    if (idlist == NULL)
        return e_Memory;
    nid = 0;

    /* For each symbol set,
     *   for each font in appropriate storage,
     *     if the font supports that symbol set, list the symbol set
     *     and break (because we only need to find one such font). */

    /* NOTE: Temporarily chain soft, built-in symbol sets.  DON'T
     * exit this section without unchaining them. */
    pl_dict_set_parent(&pcs->soft_symbol_sets, &pcs->built_in_symbol_sets);
    pl_dict_enum_begin(&pcs->soft_symbol_sets, &denum);
    while (pl_dict_enum_next(&denum, &key, &value)) {
        pcl_symbol_set_t *ssp = (pcl_symbol_set_t *) value;
        pl_glyph_vocabulary_t gx;

        for (gx = plgv_MSL; gx < plgv_next; gx++)
            if (ssp->maps[gx] != NULL &&
                status_check_symbol_set(pcs, ssp->maps[gx], storage)) {
                nid = status_add_symbol_id(idlist, nid,
                                           (ssp->maps[gx]->id[0] << 8) +
                                           ssp->maps[gx]->id[1]);
                break;          /* one will suffice */
            }
    }
    pl_dict_set_parent(&pcs->soft_symbol_sets, NULL);
    /* Symbol sets are back to normal. */

    status_print_idlist(s, idlist, nid, "IDLIST");
    gs_free_object(pcs->memory, (void *)idlist, "status_symbol_sets(idlist)");
    return 0;
}

static int
status_fonts_extended(stream * s, pcl_state_t * pcs,
                      pcl_data_storage_t storage)
{
    return status_do_fonts(s, pcs, storage, true);
}

static int (*const status_write[]) (stream * s, pcl_state_t * pcs,
                                    pcl_data_storage_t storage) = {
status_fonts, status_macros, status_patterns, status_symbol_sets,
        status_fonts_extended};

/* Commands */

static int                      /* ESC * s <enum> T */
pcl_set_readback_loc_type(pcl_args_t * pargs, pcl_state_t * pcs)
{
    pcs->location_type = uint_arg(pargs);
    return 0;
}

static int                      /* ESC * s <enum> U */
pcl_set_readback_loc_unit(pcl_args_t * pargs, pcl_state_t * pcs)
{
    pcs->location_unit = uint_arg(pargs);
    return 0;
}

static int                      /* ESC * s <enum> I */
pcl_inquire_readback_entity(pcl_args_t * pargs, pcl_state_t * pcs)
{
    uint i = uint_arg(pargs);
    int unit = pcs->location_unit;
    stream st;

    static const char *entity_types[] = {
        "FONTS", "MACROS", "PATTERNS", "SYMBOLSETS", "FONTS EXTENDED"
    };
    pcl_data_storage_t storage;
    int code = 0;
    long pos;

    if (i > 4)
        return e_Range;
    status_begin(&st, pcs);
    stprintf(&st, "INFO %s\r\n", entity_types[i]);
    switch (pcs->location_type) {
        case 0:                /* invalid location */
            code = -1;
            break;
        case 1:                /* currently selected */
            storage = (pcl_data_storage_t) 0;   /* indicates currently selected */
            break;
        case 2:                /* all locations */
            storage = (pcl_data_storage_t) ~ 0;
            break;
        case 3:                /* internal */
            if (unit != 0) {
                code = -1;
                break;
            }
            storage = pcds_internal;
            break;
        case 4:                /* downloaded */
            if (unit > 2)
                code = -1;
            else {
                static const pcl_data_storage_t dl_masks[] =
                    { pcds_downloaded, pcds_temporary, pcds_permanent
                };
                storage = dl_masks[unit];
            }
            break;
        case 5:                /* cartridges */
            if (unit == 0)
                storage = (pcl_data_storage_t) pcds_all_cartridges;
            else if (unit <= pcds_cartridge_max)
                storage = (pcl_data_storage_t)
                    (1 << (pcds_cartridge_shift + unit - 1));
            else
                code = -1;
            break;
        case 6:                /* SIMMs */
            if (unit == 0)
                storage = (pcl_data_storage_t) pcds_all_simms;
            else if (unit <= pcds_simm_max)
                storage =
                    (pcl_data_storage_t) (1 << (pcds_simm_shift + unit - 1));
            else
                code = -1;
            break;
        default:
            code = -1;
            stputs(&st, "ERROR=INVALID ENTITY\r\n");
            break;
    }
    if (code >= 0) {
        pos = stell(&st);
        code = (*status_write[i]) (&st, pcs, storage);
        if (code >= 0) {
            if (stell(&st) == pos)
                stputs(&st, "ERROR=NONE\r\n");
            else if (storage == 0)      /* currently selected */
                stprintf(&st, "LOCTYPE=%d\r\nLOCUNIT=%d\r\n",
                         pcs->location_type, unit);
        }
    }
    if (code < 0) {
        if (code == e_Memory)
            stputs(&st, "ERROR=INTERNAL ERROR\r\n");
        else
            stputs(&st, "ERROR=INVALID LOCATION\r\n");
    }
    status_end(&st, pcs);
    return 0;
}

static int                      /* ESC * s 1 M */
pcl_free_space(pcl_args_t * pargs, pcl_state_t * pcs)
{
    stream st;

    status_begin(&st, pcs);
    stprintf(&st, "INFO MEMORY\r\n");
    if (int_arg(pargs) != 1)
        stprintf(&st, "ERROR=INVALID UNIT\r\n");
    else {
        gs_memory_status_t mstat;

        gs_memory_status(pcs->memory, &mstat);
        if (pcs->memory != pcs->memory->non_gc_memory) {
            gs_memory_status_t dstat;

            gs_memory_status(pcs->memory->non_gc_memory, &dstat);
            mstat.allocated += dstat.allocated;
            mstat.used += dstat.used;
        }
        stprintf(&st, "TOTAL=%ld\r\n", mstat.allocated - mstat.used);
        /* We don't currently have an API for determining */
        /* the largest contiguous block. */
            /**** RETURN SOMETHING RANDOM ****/
        stprintf(&st, "LARGEST=%ld\r\n", (mstat.allocated - mstat.used) >> 2);
    }
    status_end(&st, pcs);
    return 0;
}

static int                      /* ESC & r <bool> F */
pcl_flush_all_pages(pcl_args_t * pargs, pcl_state_t * pcs)
{
    switch (uint_arg(pargs)) {
        case 0:
            {                   /* Flush all complete pages. */
                /* This is a driver function.... */
                return 0;
            }
        case 1:
            {                   /* Flush all pages, including an incomplete one. */
                int code = pcl_end_page_if_marked(pcs);

                if (code >= 0)
                    code = pcl_home_cursor(pcs);
                return code;
            }
        default:
            return e_Range;
    }
}

static int                      /* ESC * s <int_id> X */
pcl_echo(pcl_args_t * pargs, pcl_state_t * pcs)
{
    stream st;

    status_begin(&st, pcs);
    stprintf(&st, "ECHO %d\r\n", int_arg(pargs));
    status_end(&st, pcs);
    return 0;
}

/* Initialization */
static int
pcstatus_do_registration(pcl_parser_state_t * pcl_parser_state,
                         gs_memory_t * mem)
{                               /* Register commands */
    DEFINE_CLASS('*') {
    's', 'T',
            PCL_COMMAND("Set Readback Location Type",
                            pcl_set_readback_loc_type,
                            pca_neg_error | pca_big_error)}, {
    's', 'U',
            PCL_COMMAND("Set Readback Location Unit",
                            pcl_set_readback_loc_unit,
                            pca_neg_error | pca_big_error)}, {
    's', 'I',
            PCL_COMMAND("Inquire Readback Entity",
                            pcl_inquire_readback_entity,
                            pca_neg_error | pca_big_error)}, {
    's', 'M',
            PCL_COMMAND("Free Space", pcl_free_space,
                            pca_neg_ok | pca_big_ok)},
        END_CLASS
        DEFINE_CLASS_COMMAND_ARGS('&', 'r', 'F', "Flush All Pages",
                                  pcl_flush_all_pages,
                                  pca_neg_error | pca_big_error)
        DEFINE_CLASS_COMMAND_ARGS('*', 's', 'X', "Echo",
                                  pcl_echo, pca_neg_ok | pca_big_error)
        return 0;
}
static int
pcstatus_do_reset(pcl_state_t * pcs, pcl_reset_type_t type)
{
    if (type & (pcl_reset_initial | pcl_reset_printer)) {
        if (type & pcl_reset_initial) {
            pcs->status.buffer = 0;
            pcs->status.write_pos = 0;
            pcs->status.read_pos = 0;
        }
        pcs->location_type = 0;
        pcs->location_unit = 0;
    }

    return 0;
}

const pcl_init_t pcstatus_init = {
    pcstatus_do_registration, pcstatus_do_reset
};
