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
/*$Id$ */

/* plulfont.c */
/* PCL5 font preloading */
#include "stdio_.h"
#include "string_.h"
#include "gsmemory.h"
#include "gstypes.h"
#include "gp.h"
#include "gpgetenv.h"
#include "plfont.h"
#include "pldict.h"
#include "pllfont.h"
#include "plftable.h"
#include "plvalue.h"
#include "plvocab.h"
#include "gdebug.h"
#include "gserror.h"
#include "gsstate.h"
#include "gxfont.h"
#include "uconfig.h"
#undef true
#undef false
#undef frac_bits
#include "cgconfig.h"
#include "ufstport.h"
#include "shareinc.h"
#include "gxfapiu.h"


/* the line printer font NB FIXME use a header file. */
#include "plulp.c"
/* global warning.  ufst state structure passed to each ufst function */
IF_STATE IFS;
PIF_STATE pIFS = &IFS;


/* GLOBAL Warning! NB fix me */
/*
 * fco and plugin handles which must be freed when the interpreter shuts down
 */
static SW16  fcHndlPlAry[16];
static bool  plugins_opened = false;

/* NB fixme - we might as well require an environment variable for the
   fco names and plugins, these change every UFST release */


/* defaults for locations of font collection objects (fco's) and
   plugins the root data directory.  These are internally separated with
   ':' but environment variable use the gp separator */
#ifndef UFSTFONTDIR
const char *UFSTFONTDIR="/usr/local/fontdata5.0/"; /* A bogus linux location */
#endif

/* default list of fcos and plugins - relative to UFSTFONTDIR */
const char *UFSTFCOS="mtfonts/pclps2/mt3/pclp2_xj.fco:mtfonts/pcl45/mt3/wd____xh.fco";
const char *UFSTPLUGINS="mtfonts/pcl45/mt3/plug__xi.fco";

/* return a null terminated array of strings on the heap from
   str0:str1:str:.  Use gs separator */

static 
char **build_strs(gs_memory_t *mem, char *str, char separator)
{
    int i;
    char *start_path = str;
    char *end_path = str;
    char **list;

    /* null str or 0 length string */
    if (( str == NULL ) || ( str[0] == '\0' ))
        return NULL;

    for (i=0; ;i++) {
        /* find next separator or NULL */
        while( *end_path != (char)NULL && *end_path != separator )
            end_path++;
        /* reallocate the list that holds the string pointers */
        if ( i == 0 )
            /* first time */
            list = 
                (char **)gs_alloc_bytes(mem,
                                        ((i+1) + 1) /* terminating null */ * sizeof(char *),
                                        "build_strs");
        else
            list = gs_resize_object(mem,
                                    list,
                                    ((i+1) + 1) /* terminating null */ * sizeof(char *),
                                    "build_strs");
        if ( list == NULL ) {
            /* NB no fall back or freeing of memory already consumed */
            dprintf("Fatal System Failure\n" );
            return NULL;
        }
        /* terminate the list of strings */
        list[i + 1] = (char *)NULL;
        /* allocate space for the string */
        list[i] = gs_alloc_bytes(mem, 
                                 end_path - start_path + 1 /* NULL term */,
                                 "build_strs");
        
        if ( list[i] == NULL ) {
            /* NB no fall back or freeing of memory already consumed */
            dprintf("Fatal System Failure\n" );
            return NULL;
        }

        memcpy(list[i], start_path, end_path - start_path);
        list[i][end_path - start_path] = (char)NULL;

        if ( *end_path == (char)NULL )
            break; /* done */
        else { /* separator - continue */
            start_path = end_path + 1;
            end_path = start_path;
            /* handle special case of separator ending the list */
            if ( *end_path == '\0' )
                break;
        }
    }
    return list;
}

static void
free_strs(gs_memory_t *mem, char **str_of_strs)
{
    int i;
    for ( i = 0 ; str_of_strs[i]; i++ )
        /* free string entry */
        gs_free_object(mem, str_of_strs[i], "free_strs");
    /* free container for freed strings */
    gs_free_object(mem, str_of_strs, "free_strs");
    return;

}

#define MAXPATHLEN 1024


/* these are lists of fco's and plugins relative to the root directory
   they can be set by an environment variable or defaults.  */
static char **
pl_ufst_get_list(gs_memory_t *mem, char *key, char *defaultval)
{
    char pathname[MAXPATHLEN];
    int pathlen = sizeof(pathname);

    if ( gp_getenv(key, pathname, &pathlen) != 0 )
        /* no env var use default */
        return build_strs(mem, defaultval, ':');
    else
        return build_strs(mem, pathname, gp_file_name_list_separator);
}

void
pl_ufst_root_dir(char *pathname, int pathlen)
{
    /* get the ufst root directory, check for an environment variable,
       if not set return default */
    if ( gp_getenv( "UFSTFONTDIR", pathname, &pathlen) != 0 )
        strcpy(pathname, UFSTFONTDIR);
    return;
}

/* check all table entries have a value in the font dictionary */
#ifdef DEBUG
void
pl_check_fonts(pl_dict_t *pfontdict, bool use_unicode_names_for_keys)
{
    int j;
    for ( j = 0; strlen(resident_table[j].full_font_name); j++ ) {
        void *value;
        /* lookup unicode key in the resident table */
        if ( use_unicode_names_for_keys ) {
            if ( !pl_dict_lookup(pfontdict,
                                 resident_table[j].unicode_fontname,
                                 sizeof(resident_table[j].unicode_fontname),
                                 &value, true, NULL) /* return data ignored */ ) {
                int i;
                dprintf("Font with unicode key: ");
                for (i = 0; 
                     i < sizeof(resident_table[j].unicode_fontname)/sizeof(resident_table[j].unicode_fontname[0]); 
                     i++) {
                    dprintf1("%c", (char)resident_table[j].unicode_fontname[i]);
                }
                dprintf1(" not available in font dictionary, resident table position: %d\n", j);
            }
        } else {
            byte key[3];
            key[2] = (byte)j;
            key[0] = key[1] = 0;
            if ( !pl_dict_lookup(pfontdict,
                                 key,
                                 sizeof(key),
                                 &value, true, NULL) /* return data ignored */ )
                dprintf2("%s not available in font dictionary, resident table position: %d\n",
                         resident_table[j].full_font_name, j);
        }
    }
    return;
}
#endif

int
pl_load_built_in_fonts(const char *pathname, gs_memory_t *mem, pl_dict_t *pfontdict,
		       gs_font_dir *pdir, int storage, bool use_unicode_names_for_keys)
{
    int                 i, k;
    UW16                bSize, status = 0;
    byte                key[3];
    UB8                 pthnm[1024];
    UB8                 ufst_root_dir[1024];
    char                 **fcos;
    char                 **plugins;
    /* don't load fonts more than once */
    if (pl_dict_length(pfontdict, true) > 0)
	return true;

    pl_ufst_root_dir(ufst_root_dir, sizeof(ufst_root_dir));

    status = gx_UFST_init(ufst_root_dir);

    if (!plugins_opened) {

	gx_UFST_close_static_fcos();
        plugins = pl_ufst_get_list(mem, "UFSTPLUGINS", UFSTPLUGINS);
        for (k = 0; plugins[k]; k++) {
            strcpy((char *)pthnm, ufst_root_dir);
            strcat((char *)pthnm, plugins[k]);
            if ((status = gx_UFST_open_static_fco(pthnm, &fcHndlPlAry[k])) != 0) {
                dprintf2("CGIFfco_Open error %d for %s\n", status, pthnm);
                return FALSE;
            }
            if ((status = CGIFfco_Plugin(FSA fcHndlPlAry[k])) != 0) {
                dprintf1("CGIFfco_Plugin error %d\n", status);
                return FALSE;
            }
        }
        free_strs(mem, plugins);
        /* end of list */
        fcHndlPlAry[k] = -1;
        plugins_opened = true;
    }
    /* step on the callback expect FAPI todo the same on language switch */
    plu_set_callbacks();

    /*
     * Open and install the various font collection objects.
     *
     * For each font collection object, step through the object until it is
     * exhausted, placing any fonts found in the built_in_fonts dcitonary.
     *
     */
    fcos = pl_ufst_get_list(mem, "UFSTFCOS", UFSTFCOS);
    for (k = 0; fcos[k]; k++) {
	SW16 fcoHandle;
        /* build and open (get handle) for the k'th fco file name */
        strcpy((char *)pthnm, ufst_root_dir);
        strcat((char *)pthnm, fcos[k]);
	
	fcoHandle = gx_UFST_find_fco_handle_by_name(pthnm);

	if (fcoHandle == 0 && 
	    (status = gx_UFST_open_static_fco(pthnm, &fcoHandle)) != 0) {
            dprintf2("CGIFfco_Open error %d for %s\n", status, pthnm);
            continue;
        }
        /* enumerat the files in this fco */
        for ( i = 0;
              CGIFfco_Access(FSA pthnm, i, TFATRIB_KEY, &bSize, NULL) == 0;
              i++, key[2] += 1 ) {
            LPSB8   pBuffer = (LPSB8)gs_alloc_bytes( mem,
                                                     bSize,
                                                     "TTFONTINFO buffer" );

            if (pBuffer == 0) {
                dprintf1("VM error for built-in font %d", i);
                continue;
            }
            status = CGIFfco_Access( FSA
                                     pthnm,
                                     i,
                                     TFATRIB_KEY,
                                     &bSize,
                                     pBuffer );
            if (status != 0)
                dprintf1("CGIFfco_Access error %d\n", status);
            else {
                TTFONTINFOTYPE *    pfDesc = (TTFONTINFOTYPE *)pBuffer;
                LPSB8               pname = pBuffer + pfDesc->psname;
                /* unfortunately agfa has 2 fonts named symbol.  We
                   believe the font with internal number, NB, NB, NB  */
                LPSB8               symname = "SymbPS";
                int                 j;
                bool                found;
                if ( pfDesc->pcltFontNumber == 24463 )
                    pname = symname;
                for (j = 0; strlen(resident_table[j].full_font_name); j++) {
                    pl_font_t * plfont;
                    int         err_cd;
                    if (strcmp(resident_table[j].full_font_name, pname) != 0)
                        continue;
                    err_cd = pl_load_mt_font(fcoHandle, pdir, mem, i, &plfont);
                    if (err_cd != 0)
                        return gs_throw1(err_cd, "An unrecoverable failure occurred while loading the resident font %s\n", pname);
                    else {
                        uint    pitch_cp = (pfDesc->spaceBand * 100.0)
                            / pfDesc->scaleFactor + 0.5;
#ifdef DEBUG
                        if (gs_debug_c('=') )
                            dprintf2("Loading %s from fco %s\n", pname, fcos[k] );
#endif
                        /* Record the differing points per inch value
                           for Intellifont derived fonts. */
                        
                        if (pfDesc->scaleFactor == 8782) {
                            plfont->pts_per_inch = 72.307;
                            pitch_cp = (pfDesc->spaceBand * 100 * 72.0)
                                / (pfDesc->scaleFactor * 72.307) + 0.5;
                        }
#ifdef DEBUG
                        if (gs_debug_c('=') )
                            dprintf3("scale factor=%d, pitch (cp)=%d per_inch_x100=%d\n", pfDesc->scaleFactor, pitch_cp, (uint)(720000.0/pitch_cp));
#endif

                        plfont->font_type = resident_table[j].font_type;
                        plfont->storage = storage;
                        plfont->data_are_permanent = false;
                        plfont->params = resident_table[j].params;

                        /*
                         * NB: though the TTFONTINFOTYPE structure has a
                         * pcltChComp field, it is not filled in by the UFST
                         * code (which just initializes it to 0). Hence, the
                         * hard-coded information in the resident font
                         * initialization structure is used.
                         */
                        memcpy(plfont->character_complement,
                               resident_table[j].character_complement, 8);

                        if ( use_unicode_names_for_keys )
                            pl_dict_put( pfontdict, resident_table[j].unicode_fontname, 32, plfont );
                        else {
                            key[2] = (byte)j;
                            key[0] = key[1] = 0;
                            pl_dict_put( pfontdict, key, sizeof(key), plfont );
                        }
                    }
                }
            }
            gs_free_object(mem, pBuffer, "TTFONTINFO buffer");
        }
    } /* end enumerate fco loop */
    free_strs(mem, fcos);
    /* finally add lineprinter NB return code ignored */
    (void)pl_load_ufst_lineprinter(mem, pfontdict, pdir, storage, use_unicode_names_for_keys);
#ifdef DEBUG
    if (gs_debug_c('=') )
        pl_check_fonts(pfontdict, use_unicode_names_for_keys);
#endif
    return TRUE;
}
  
int
pl_load_ufst_lineprinter(gs_memory_t *mem, pl_dict_t *pfontdict, gs_font_dir *pdir, 
                         int storage, bool use_unicode_names_for_keys)
{
    int i;

    for (i = 0; strlen(resident_table[i].full_font_name); i++) {
        if (resident_table[i].params.typeface_family == 0) {
            const byte *header = pl_ulp_header;
            const byte *char_data = pl_ulp_character_data;
            pl_font_t *pplfont = pl_alloc_font(mem, "pl_load_ufst_lineprinter pplfont");
            gs_font_base *pfont = gs_alloc_struct(mem, gs_font_base, &st_gs_font_base,
                                                  "pl_load_ufst_lineprinter pfont");
            int code;
            /* these shouldn't happen during system setup */
            if (pplfont == 0 || pfont == 0)
                return -1;
            if (pl_fill_in_font(pfont, pplfont, pdir, mem, "lineprinter fonts") < 0)
                return -1;
           
            pl_fill_in_bitmap_font(pfont, gs_next_ids(mem, 1));
            pplfont->params = resident_table[i].params;
            memcpy(pplfont->character_complement, resident_table[i].character_complement, 8);


            if ( use_unicode_names_for_keys )
                pl_dict_put(pfontdict, resident_table[i].unicode_fontname, 32, pplfont );
            else {
                byte key[3];
                key[2] = (byte)i;
                key[0] = key[1] = 0;
                pl_dict_put(pfontdict, key, sizeof(key), pplfont);
            }
            pplfont->storage = storage; /* should be an internal font */
            pplfont->data_are_permanent = true;
            pplfont->header = (byte *)header;
            pplfont->font_type = plft_8bit_printable;
            pplfont->scaling_technology = plfst_bitmap;
            pplfont->is_xl_format = false;
            pplfont->resolution.x = pplfont->resolution.y = 300;

            code = pl_font_alloc_glyph_table(pplfont, 256, mem, 
                                             "pl_load_ufst_lineprinter pplfont (glyph table)");
            if ( code < 0 )
                return code;

            while (1) {
	
                uint width = pl_get_uint16(char_data + 12);
                uint height = pl_get_uint16(char_data + 14);
                uint ccode_plus_header_plus_data = 2 + 16 + (((width + 7) >> 3) * height);
                uint ucode = pl_map_MSL_to_Unicode(pl_get_uint16(char_data), 0);
                int code = 0;

                /* NB this shouldn't happen but it does, should be
                   looked at */
                if (ucode != 0xffff)
                     code = pl_font_add_glyph(pplfont, ucode, char_data + 2);

                if (code < 0)
                    /* shouldn't happen */
                    return -1;
                /* calculate the offset of the next character code in the table */
                char_data += ccode_plus_header_plus_data;

                /* char code 0 is end of table */
                if (pl_get_uint16(char_data) == 0)
                    break;
            }
            code = gs_definefont(pdir, (gs_font *)pfont);
            if (code < 0)
                /* shouldn't happen */
                return -1;
        }
    }
    return 0;
}

/*
 * Close the font collection objects for the built-in fonts. This should be
 * done whenever the built-in font dictionary is released.
 */
  void
pl_close_built_in_fonts(pl_dict_t *builtinfonts)
{
    int     i;

    /* close fco's */
    gx_UFST_close_static_fcos();

    /* close plugins */
    for ( i = 0; fcHndlPlAry[i] != -1; i++ ) {
        CGIFfco_Close(FSA fcHndlPlAry[i]);
        dprintf1("closing handle %d\n", fcHndlPlAry[i]);
    }
    gx_UFST_fini();
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
