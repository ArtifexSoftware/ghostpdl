/* Portions Copyright (C) 2001 artofcode LLC.
   Portions Copyright (C) 1996, 2001 Artifex Software Inc.
   Portions Copyright (C) 1988, 2000 Aladdin Enterprises.
   This software is based in part on the work of the Independent JPEG Group.
   All Rights Reserved.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */

/*$Id: */
/* pllrfont.c */
/* PCL5 font preloading */
#include "ctype_.h"
#include "stdio_.h"
#include "string_.h"
/* The following are all for gxfont42.h, except for gp.h. */
#include "gx.h"
#include "gp.h"
#include "gsccode.h"
#include "gserrors.h"
#include "gsmatrix.h"
#include "gsutil.h"
#include "gxfont.h"
#include "gxfont42.h"
#include "plfont.h"
#include "pldict.h"
#include "pllfont.h"
#include "plftable.h"
#include "plvalue.h"
#include "romfnttab.h"
#include "zlib.h"

/* Load some built-in fonts.  This must be done at initialization time, but
 * after the state and memory are set up.  Return an indication of whether
 * at least one font was successfully loaded.  XXX The existing code is more
 * than a bit of a hack.  Approach: expect to find some fonts in one or more
 * of a given list of directories with names *.ttf.  Load whichever ones are
 * found in the table below.
 */


/* Load a built-in (TrueType) font from external storage. */
static int
pl_append_tt_data(gs_memory_t *mem, char **ppheader, char *buffer, int length)
{
    if (*ppheader == NULL) {
        *ppheader = gs_alloc_bytes(mem, length, "pl_append_tt_data");
        if (*ppheader == NULL)
            return -1;
        memcpy(*ppheader, buffer, length);
    }else {
        uint size = gs_object_size(mem, *ppheader);
        byte *new_header = gs_resize_object(mem, *ppheader, size + length,
			   "pl_append_tt_data");
        if (new_header == NULL)
            return -1;
        memcpy(new_header + size, buffer, length);
        *ppheader = new_header;
    }
    return 0;
}

static int
pl_load_romtt_font(gs_font_dir *pdir, gs_memory_t *mem, long unique_id, pl_font_t **pplfont, char *font_name, unsigned char *font_data)
{	
    bool compressed;
    int code;
    gs_font_type42 *pfont;
    pl_font_t *plfont;
    char *uncompressed_header = NULL;
    compressed = pl_is_tt_zlibC(font_data);

    if (compressed) {
        char buf[1024];
        z_stream zs;
        char *start = font_data;
        zs.next_in = start;
        zs.avail_in = 1024;
        zs.next_out = &buf;
        zs.avail_out = 1024;
        zs.zalloc = Z_NULL;
        zs.zfree = Z_NULL;
        zs.opaque = Z_NULL;

        code = inflateInit (&zs);
        while ((code = inflate (&zs, Z_NO_FLUSH)) == Z_OK) {
            if (zs.avail_out == 0) {
                pl_append_tt_data(mem, &uncompressed_header, buf, 1024);
                zs.next_out = &buf;
                zs.avail_out = 1024;
            }
            if (zs.avail_in == 0) {
                zs.avail_in = 1024;
            }
        }
        while ((code = inflate (&zs, Z_FINISH)) == Z_OK) {
            if (zs.avail_out == 0) {
                pl_append_tt_data(mem, &uncompressed_header, buf, 1024);
                zs.next_out = &buf;
                zs.avail_out = 1024;
            }
            if (zs.avail_in == 0) {
                zs.avail_in = 1024;
            }
        }
        
        if ( code != Z_OK && code != Z_STREAM_END ) {
            dprintf1(mem, "fatal zlib failure code %d\n", code);
            /* since this is fatal we don't cleanup */
            return -1;
        }
        
        code = inflateEnd(&zs);
        if (code != Z_OK) {
            dprintf1(mem, "fatal zlib failure code %d\n", code);
            /* since this is fatal we don't cleanup */
            return -1;
        }
                   
    }
        
    /* Make a Type 42 font out of the TrueType data. */
    pfont = gs_alloc_struct(mem, gs_font_type42, &st_gs_font_type42,
			    "pl_fill_in_romtt_font(gs_font_type42)");
    plfont = pl_alloc_font(mem, "pl_fill_in_romtt_load_font(pl_font_t)");

    if ( pfont == 0 || plfont == 0 )
	code = gs_note_error(mem, gs_error_VMerror);
    else { /* Initialize general font boilerplate. */
	code = pl_fill_in_font((gs_font *)pfont, plfont, pdir, mem, font_name);
	if ( code >= 0 ) { /* Initialize TrueType font boilerplate. */
	    plfont->header = compressed ? uncompressed_header : font_data;
	    plfont->header_size = 0; /* nb clone fonts won't work */
	    plfont->scaling_technology = plfst_TrueType;
	    plfont->font_type = plft_Unicode;
	    plfont->large_sizes = true;
	    plfont->offsets.GT = 0;
            plfont->is_xl_format = false;            
            plfont->allow_vertical_substitutes = false;
	    pl_fill_in_tt_font(pfont, font_data, unique_id);
	    code = gs_definefont(pdir, (gs_font *)pfont);
	    if ( code >= 0 ) { 
		/*
		 * Set the nominal design width to the
		 * width of a small 'x' character.  If
		 * there isn't one, set the design
		 * width arbitrarily at 0.6 em.  */
		gs_char space = ' ';
		float sbw[4];
		uint glyph_index =
		    (*pfont->procs.encode_char)
		    ((gs_font *)pfont, space, gs_no_glyph);

		if (glyph_index == gs_no_glyph || glyph_index == 0xffff)
		    glyph_index = 0;
		if ( gs_type42_get_metrics(pfont, glyph_index, sbw) < 0 )
		    sbw[2] = 0.6;
		pl_fp_set_pitch_cp(&plfont->params, sbw[2] * 100);
	    }
	}
    }
    if ( code < 0 ) { 
	gs_free_object(mem, plfont, "pl_tt_load_font(pl_font_t)");
	gs_free_object(mem, pfont, "pl_tt_load_font(gs_font_type42)");
	return code;
    }
    *pplfont = plfont;
    return 0;
}

 int
pl_load_built_in_fonts(const char *pathname, gs_memory_t *mem,
                       pl_dict_t *pfontdict, gs_font_dir *pdir,
                       int storage, bool use_unicode_names_for_keys)
{	
    const font_resident_t *residentp;
    /* get rid of this should be keyed by pjl font number */
    byte key[3];
    bool one_font_found = false;

    /* don't load fonts more than once */
    if (pl_dict_length(pfontdict, true) > 0 ) {
	return true;
    }

    /* Enumerate through the files in the path */
    {
        int i;
        for (i = 0; pcl_font_variable_name_table[i].data != 0; i++) {
            pl_font_t *plfont;
            pcl_font_variable_name_t rec =
                pcl_font_variable_name_table[i];
            const char *buffer = rec.font_name;
            
            /* lookup the font file name in the resident table */
            for ( residentp = resident_table; strlen(residentp->full_font_name); ++residentp )
                if ( strcmp( buffer, residentp->full_font_name ) == 0 )
                    /* found it */
                    break;
            /* hit sentinnel, nothing found */
            if ( !strlen(residentp->full_font_name) )
                continue;

            if ( pl_load_romtt_font(pdir, mem, gs_next_ids(mem, 1), &plfont, buffer, rec.data) < 0 )  {
                /* vm error but we continue anyway */
                dprintf1(mem, "Failed loading font %s\n", rec.font_name);
                continue;
            }

            plfont->storage = storage;
            plfont->data_are_permanent = true;
            if ( residentp->params.symbol_set != 0 )
                plfont->font_type = plft_8bit;
            /*
             * Don't smash the pitch, which was obtained
             * from the actual font.
             */
            { 
                pl_font_pitch_t save_pitch;
                save_pitch = plfont->params.pitch;
                plfont->params = residentp->params;
                plfont->params.pitch = save_pitch;
            }

            memcpy(plfont->character_complement,
                   residentp->character_complement, 8);
            /* use the offset in the table as the pjl font number */
            plfont->params.pjl_font_number = (int)(residentp - resident_table);
            /* for unicode keying of the dictionary use the unicode
               font name, otherwise use the keys. */
            if ( use_unicode_names_for_keys )
                pl_dict_put( pfontdict, (const byte *)residentp->unicode_fontname, 32, plfont );
            else {
                key[2] = (byte)(residentp - resident_table);
                key[0] = key[1] = 0;
                pl_dict_put( pfontdict, key, sizeof(key), plfont );

            }
            /* mark the font as found */
            one_font_found = true;
        } /* next file */
        if ( one_font_found )
            return true;
        else
            return false;
    }
}

/* These are not implemented */

/* load simm fonts given a path */
 int
pl_load_simm_fonts(const char *pathname, gs_memory_t *mem, pl_dict_t *pfontdict, gs_font_dir *pdir, int storage)
{
    /* not implemented */
    return 0;
}

/* load simm fonts given a path */
 int
pl_load_cartridge_fonts(const char *pathname, gs_memory_t *mem, pl_dict_t *pfontdict, gs_font_dir *pdir, int storage)
{
    /* not implemented */
    return 0;
}
