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
IF_STATE IFS;
PIF_STATE pIFS = &IFS;

 /*
 * Paths for the statically loadded data. Since only MicroType font collection
 * objects are used for built-in fonts, all of these names are prefaced by fc.
 *
 *     fcNmPl       the plugin object, to be used for characters that are
 *                  identical among several fonts and thus not included
 *                  in the base set
 *
 *     fcNmAry      The array of font collection objects. There are currently
 *                  there of these, for the base, Symbol, and WingDings fonts
 *
 * Note that the plug-in font collection object must be specifically registered
 * via the CGIFfco_Plugin procedure.
 */
#ifdef MSVC
private const UB8           fcNmPl[] =    "\\mtfonts\\pcl45\\mt1\\plug___g.fco";
private const UB8 *const    fcNmAry[] = { "\\mtfonts\\pcl45\\mt1\\pcl____g.fco",
                                          "\\mtfonts\\pcl45\\mt1\\wd_____g.fco" };
#else

private const UB8           fcNmPl[] =    "mtfonts/pcl45/mt1/plug___g.fco";
private const UB8 *const    fcNmAry[] = { "mtfonts/pcl45/mt1/pcl____g.fco",
                                          "mtfonts/pcl45/mt1/wd_____g.fco" };
#endif

/* GLOBAL Warning! */
/*
 * Handle for the plugin object and the various font collection objects. The
 * latter is made large enough to hold the largest reasonable case.
 */
private SW16  fcHndlPl = -1;
private SW16  fcHndlAry[16];

private int
pl_ufst_root_dir(char *pathname, int pathlen)
{
 
    /* get the ufst root directory */
    /* CONFIG: Hard coded ufst root directory could go here. */
    if ( gp_getenv( "UFSTFONTSOURCE", pathname, &pathlen) != 0 ) {
	dprintf( "UFST configuration error.  Set environment variable UFSTFONTSOURCE\n" );
	dprintf( "to the directory containing the ufst fontdata directory\n" );
	dprintf( "ie: export UFSTFONTSOURCE=/usr/local/ufst/fontdata/ \n" );
	return -1;
    } else 
	return 0;
}

/*
 * The pathnames parameter is ignored for now - the name of the font collection
 * to be used is hard-coded above.
 */
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

    pl_font_t *font_found[pl_resident_font_table_count];
    /* don't load fonts more than once */
    if (pl_dict_length(pfontdict, true) > 0)
	return true;

#define MAX_OPEN_LIBRARIES  5   /* NB */
#define BITMAP_WIDTH        1   /* must be 1, 2, 4 or 8 */
    if ( pl_ufst_root_dir(ufst_root_dir, sizeof(ufst_root_dir)) < 0 )
        return false;

    strcpy(config_block.ufstPath, ufst_root_dir);
    strcat(config_block.ufstPath, "support");
    strcpy(config_block.typePath, ufst_root_dir);
    strcat(config_block.typePath, "support");
    config_block.num_files = MAX_OPEN_LIBRARIES;  /* max open library files */
    config_block.bit_map_width = BITMAP_WIDTH;    /* bitmap width 1, 2 or 4 */

    /* the following part should be called only once at the beginning */
    if ((status = CGIFinit(FSA0)) != 0) {
        dprintf1 ("CGIFinit() error: %d\n", status);
        return TRUE;
    }
    if ((status = CGIFconfig(FSA &config_block)) != 0) {
        dprintf1 ("CGIFconfig() error: %d\n", status);
        return TRUE;
    }
    if ((status = CGIFenter(FSA0)) != 0) {
        dprintf1 ("CGIFenter() error: %u\n",status);
        return TRUE;
    }

    /* open and register the plug-in font collection object */
    strcpy((char *)pthnm, ufst_root_dir);
    strcat((char *)pthnm, fcNmPl);
    if ((status = CGIFfco_Open(FSA pthnm,(LPSW16)&fcHndlPl)) != 0) {
        dprintf2("CGIFfco_Open error %d for %s\n", status, pthnm);
        return TRUE;
    }
    if ((status = CGIFfco_Plugin(FSA fcHndlPl)) != 0) {
        dprintf1("CGIFfco_Plugin error %d\n", status);
        return TRUE;
    }

    /*
     * Open and install the various font collection objects.
     *
     * For each font collection object, step through the object until it is
     * exhausted, placing any fonts found in the built_in_fonts dcitonary.
     *
     */
    for (k = 0; k < sizeof(fcNmAry) / sizeof(fcNmAry[0]); k++)
        fcHndlAry[k] = -1;

    for (k = 0; k < sizeof(fcNmAry) / sizeof(fcNmAry[0]); k++) {

        strcpy((char *)pthnm, ufst_root_dir);
        strcat((char *)pthnm, (const char *)fcNmAry[k]);
        if ((status = CGIFfco_Open(FSA pthnm, &fcHndlAry[k])) != 0) {
            dprintf2("CGIFfco_Open error %d for %s\n", status, pthnm);
            continue;
        }

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
                int                 j;

                /* ignore PostScript-specific fonts */
                if (pfDesc->scaleFactor == 1000) {
                    gs_free_object(mem, pBuffer, "TTFONTINFO buffer");
                    continue;
                }

                for ( j = 0; 
                      j < pl_resident_font_table_count &&
                      strcmp(resident_table[j].full_font_name, pname) != 0;
                      j++ )
                    ;
                if (j < pl_resident_font_table_count) {
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

                        /*
                         * Record the differing points per inch value for
                         * Intellifont derived fonts.
                         */
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
			/* mark the font as found */
			font_found[j] = plfont;
                    }
                } else
                    dprintf1("No resident table entry for font %s\n", pname);
            }

            gs_free_object(mem, pBuffer, "TTFONTINFO buffer");
        }
    }
    return true;
}

/*
 * Close the font collection objects for the built-in fonts. This should be
 * done whenever the built-in font dictionary is released.
 */
  void
pl_close_built_in_fonts(void)
{
    int     i;

    if (fcHndlPl != -1)
        CGIFfco_Close(FSA fcHndlPl);
    for (i = 0; i < sizeof(fcNmAry) / sizeof(fcNmAry[0]); i++) {
        if (fcHndlAry[i] != -1)
            CGIFfco_Close(FSA fcHndlAry[i]);
    }
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
