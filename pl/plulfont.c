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
/* agfa includes */
#include "cgconfig.h"
#include "port.h"
#include "shareinc.h"
/* set to 1 to display UFST debugging statements */
SW16 trace_sw = 0;
/* global warning.  ufst state structure passed to each ufst function */
IF_STATE IFS;
PIF_STATE pIFS = &IFS;


/* GLOBAL Warning! NB fix me */
/*
 * fco and plugin handles which must be freed when the interpreter shuts down
 */
private SW16  fcHndlPlAry[16];
private SW16  fcHndlAry[16];

/* defaults for locations of font collection objects (fco's) and
   plugins the root data directory.  These are internally separated with
   ':' but environment variable use the gp separator */
const char *UFSTFONTDIR="/usr/local/fontdata/";

/* default list of fcos and plugins - relative to UFSTFONTDIR */
const char *UFSTFCOS="mtfonts/pclps2/mt1/pclp2__g.fco";
const char *UFSTPLUGINS="mtfonts/pcl45/mt1/wd_____g.fco";

/* return a null terminated array of strings on the heap from
   str0:str1:str:.  Use gs separator */

private 
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
            dprintf( "Fatal System Failure\n" );
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
            printf( "Fatal System Failure\n" );
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

private void
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
private char **
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
                dprintf( "Font with unicode key: ");
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
    IFCONFIG            config_block;
    byte                key[3];
    UB8                 pthnm[1024];
    UB8                 ufst_root_dir[1024];
    char                 **fcos;
    char                 **plugins;
    /* don't load fonts more than once */
    if (pl_dict_length(pfontdict, true) > 0)
	return true;

#define MAX_OPEN_LIBRARIES  5   /* NB */
#define BITMAP_WIDTH        1   /* must be 1, 2, 4 or 8 */
    pl_ufst_root_dir(ufst_root_dir, sizeof(ufst_root_dir));
    strcpy(config_block.ufstPath, ufst_root_dir);
    strcat(config_block.ufstPath, "support");
    strcpy(config_block.typePath, ufst_root_dir);
    strcat(config_block.typePath, "support");
    config_block.num_files = MAX_OPEN_LIBRARIES;  /* max open library files */
    config_block.bit_map_width = BITMAP_WIDTH;    /* bitmap width 1, 2 or 4 */

    /* the following part should be called only once at the beginning */
    if ((status = CGIFinit(FSA0)) != 0) {
        dprintf1 ("CGIFinit() error: %d\n", status);
        return FALSE;
    }
    if ((status = CGIFconfig(FSA &config_block)) != 0) {
        dprintf1 ("CGIFconfig() error: %d\n", status);
        return FALSE;
    }
    if ((status = CGIFenter(FSA0)) != 0) {
        dprintf1 ("CGIFenter() error: %u\n",status);
        return FALSE;
    }

    /* open and register the plug-in font collection object */
    plugins = pl_ufst_get_list(mem, "UFSTPLUGINS", UFSTPLUGINS);
    for (k = 0; plugins[k]; k++) {
        strcpy((char *)pthnm, ufst_root_dir);
        strcat((char *)pthnm, plugins[k]);
        if ((status = CGIFfco_Open(FSA pthnm, &fcHndlPlAry[k])) != 0) {
            dprintf2("CGIFfco_Open error %d for %s\n", status, pthnm);
            return FALSE;
        }
        if ((status = CGIFfco_Plugin(FSA fcHndlPlAry[k])) != 0) {
            dprintf1("CGIFfco_Plugin error %d\n", status);
            return FALSE;
        }
        /* enumerate the files in this plugin NB - we should eliminate
           the code redundancy between these routines and fco loading code below */
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
                LPSB8               pname = pBuffer + pfDesc->tfName;
                /* unfortunately agfa has 2 fonts named symbol.  We
                   believe the font with internal number, NB, NB, NB  */
                LPSB8               symname = "SymbPS";
                int                 j;
                if ( pfDesc->pcltFontNumber == 24463 )
                    pname = symname;
                for ( j = 0;
                      strlen(resident_table[j].full_font_name) &&
                      strcmp(resident_table[j].full_font_name, pname) != 0;
                      j++ )
                    ;
                if ( strlen(resident_table[j].full_font_name) ) {
                    pl_font_t * plfont;
                    int         err_cd = pl_load_mt_font( fcHndlAry[k],
                                                          pdir,
                                                          mem,
                                                          i,
                                                          &plfont );
                    if (err_cd != 0)
                        dprintf2( "Error %d while loading font %s\n",
                                  err_cd,
                                  pname );
                    else {
                        uint    pitch_cp = (pfDesc->spaceBand * 100.0)
                            / pfDesc->scaleFactor + 0.5;

                        /* Record the differing points per inch value
                           for Intellifont derived fonts. */

                        if (pfDesc->scaleFactor == 8782) {
                            plfont->pts_per_inch = 72.307;
                            pitch_cp = (pfDesc->spaceBand * 100 * 72.0)
                                         / (pfDesc->scaleFactor * 72.307) + 0.5;
                        }

                        if (resident_table[j].params.symbol_set != 0)
                            plfont->font_type = plft_8bit;

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
                        memcpy( plfont->character_complement,
                                resident_table[j].character_complement,
                                8 );

                        /* update some parameters from the file */
                        pl_fp_set_pitch_cp(&plfont->params, pitch_cp);
                        plfont->params.proportional_spacing =
                                                         !pfDesc->isFixedPitch;
                        plfont->params.style = pfDesc->pcltStyle;
                        plfont->params.stroke_weight = pfDesc->pcltStrokeWt;
                        plfont->params.typeface_family = pfDesc->pcltTypeFamily;
			/* use the offset in the table as the pjl font number */
			plfont->params.pjl_font_number = j;
                        /* store the font in the built-in-fonts dictionary */
			if ( use_unicode_names_for_keys )
			    pl_dict_put( pfontdict, resident_table[j].unicode_fontname, 32, plfont );
			else {
			    key[2] = (byte)j;
			    key[0] = key[1] = 0;
			    pl_dict_put( pfontdict, key, sizeof(key), plfont );
			}
                    }
                } else
                    dprintf1("%s found in FCO but not used by PCL\n", pname);
            }

            gs_free_object(mem, pBuffer, "TTFONTINFO buffer");
        }
    }
    free_strs(mem, plugins);

    /*
     * Open and install the various font collection objects.
     *
     * For each font collection object, step through the object until it is
     * exhausted, placing any fonts found in the built_in_fonts dcitonary.
     *
     */
    fcos = pl_ufst_get_list(mem, "UFSTFCOS", UFSTFCOS);
    for (k = 0; fcos[k]; k++) {

        /* build and open (get handle) for the k'th fco file name */
        strcpy((char *)pthnm, ufst_root_dir);
        strcat((char *)pthnm, fcos[k]);
        if ((status = CGIFfco_Open(FSA pthnm, &fcHndlAry[k])) != 0) {
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
                LPSB8               pname = pBuffer + pfDesc->tfName;
                /* unfortunately agfa has 2 fonts named symbol.  We
                   believe the font with internal number, NB, NB, NB  */
                LPSB8               symname = "SymbPS";
                int                 j;
                if ( pfDesc->pcltFontNumber == 24463 )
                    pname = symname;
                for ( j = 0;
                      strlen(resident_table[j].full_font_name) &&
                      strcmp(resident_table[j].full_font_name, pname) != 0;
                      j++ )
                    ;
                if ( strlen(resident_table[j].full_font_name) ) {
                    pl_font_t * plfont;
                    int         err_cd = pl_load_mt_font( fcHndlAry[k],
                                                          pdir,
                                                          mem,
                                                          i,
                                                          &plfont );
                    if (err_cd != 0)
                        dprintf2( "Error %d while loading font %s\n",
                                  err_cd,
                                  pname );
                    else {
                        uint    pitch_cp = (pfDesc->spaceBand * 100.0)
                            / pfDesc->scaleFactor + 0.5;

                        /* Record the differing points per inch value
                           for Intellifont derived fonts. */

                        if (pfDesc->scaleFactor == 8782) {
                            plfont->pts_per_inch = 72.307;
                            pitch_cp = (pfDesc->spaceBand * 100 * 72.0)
                                         / (pfDesc->scaleFactor * 72.307) + 0.5;
                        }

                        if (resident_table[j].params.symbol_set != 0)
                            plfont->font_type = plft_8bit;

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
                        memcpy( plfont->character_complement,
                                resident_table[j].character_complement,
                                8 );

                        /* update some parameters from the file */
                        pl_fp_set_pitch_cp(&plfont->params, pitch_cp);
                        plfont->params.proportional_spacing =
                                                         !pfDesc->isFixedPitch;
                        plfont->params.style = pfDesc->pcltStyle;
                        plfont->params.stroke_weight = pfDesc->pcltStrokeWt;
                        plfont->params.typeface_family = pfDesc->pcltTypeFamily;
			/* use the offset in the table as the pjl font number */
			plfont->params.pjl_font_number = j;
                        /* store the font in the built-in-fonts dictionary */
			if ( use_unicode_names_for_keys )
			    pl_dict_put( pfontdict, resident_table[j].unicode_fontname, 32, plfont );
			else {
			    key[2] = (byte)j;
			    key[0] = key[1] = 0;
			    pl_dict_put( pfontdict, key, sizeof(key), plfont );
			}
                    }
                } else
                    dprintf1("%s found in FCO but not used by PCL\n", pname);
            }

            gs_free_object(mem, pBuffer, "TTFONTINFO buffer");
        }
    } /* end enumerate fco loop */
    free_strs(mem, fcos);
#ifdef DEBUG
    pl_check_fonts(pfontdict, use_unicode_names_for_keys);
#endif
    return TRUE;
}

/*
 * Close the font collection objects for the built-in fonts. This should be
 * done whenever the built-in font dictionary is released.
 */
  void
pl_close_built_in_fonts(void)
{
    int     i;
    /* close fco's */
    for (i = 0; fcHndlAry[i]; i++)
        if (fcHndlAry[i])
            CGIFfco_Close(FSA fcHndlAry[i]);

    /* close plugins */
    for (i = 0; fcHndlPlAry[i]; i++)
        if (fcHndlPlAry[i])
            CGIFfco_Close(FSA fcHndlPlAry[i]);
    /* exit ufst */
    CGIFexit(FSA0);
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
