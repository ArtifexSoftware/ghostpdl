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
#include "plfont.h"
#include "pldict.h"
#include "pllfont.h"
#include "plvalue.h"
/* agfa includes */
#include "cgconfig.h"
#include "port.h"
#include "shareinc.h"
/* set to 1 to display UFST debugging statements */
SW16 trace_sw = 0;

/*
 * NB NB NB The root of the directory tree containing UFST fonts, plugins, and symbol
 * set files. Adjust this as required on your system. Note that the final
 * character must be a separator ('/').
 */
#define UFST_DATA_ROOT  "../../pl/agfa/fontdata/"

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
private const UB8           fcNmPl[] =    "mtfonts/pcl45/mt1/plug___f.fco";
private const UB8 *const    fcNmAry[] = { "mtfonts/pcl45/mt1/pcl____f.fco",
                                          "mtfonts/pcl45/mt1/wd_____f.fco" };

/*
 * Handle for the plugin object and the various font collection objects. The
 * latter is made large enough to hold the largest reasonable case.
 */
private SW16  fcHndlPl = -1;
private SW16  fcHndlAry[16];

/*
 * Paths for dynamically loaded files. Currently, all such files are kept in
 * the same place. Adjust the definition of UFST_DATA
 */
private const SB8   ufstPath[] = UFST_DATA_ROOT "support";
private const SB8   typePath[] = UFST_DATA_ROOT "support";

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

    typedef struct font_resident {
	const char full_font_name[40];    /* name entry 4 in truetype fonts */
	const short unicode_fontname[17]; /* pxl name */
	pl_font_params_t params;
	byte character_complement[8];
	/* a ridiculous hack because somebody thought this was a font solution... */
    } font_resident_t;
    static const font_resident_t resident_table[] = {
	/*
	 * Symbol sets, typeface family values, and character complements
	 * are faked; they do not (necessarily) match the actual fonts.  */
#define C(b) ((byte)((b) ^ 0xff))
#define cc_alphabetic\
	  { C(0), C(0), C(0), C(0), C(0xff), C(0xc0), C(0), C(plgv_Unicode) }
#define cc_symbol\
	  { C(0), C(0), C(0), C(4), C(0), C(0), C(0), C(plgv_MSL) }
#define cc_dingbats\
	  { C(0), C(0), C(0), C(1), C(0), C(0), C(0), C(plgv_MSL) }
#define pitch_1 fp_pitch_value_cp(1)
	/*
	 * Per TRM 23-87, PCL5 printers are supposed to have Univers
	 * and CG Times fonts.  Substitute Arial for Univers and
	 * Times for CG Times.
	 */
	/* hack the vendor value to be agfa's. */
#define agfa (4096)
	/* the actual typeface number is vendor + base value.  Base
	   values are found in the pcl 5 Comparison guide - Appendix
	   C-6.  We can add a parameter for vendor if necessary, for
	   now it is agfa. */
#define face_val(base_value, vendor) (vendor + (base_value))
	/* definition for style word as defined on 11-19 PCLTRM */
#define style_word(posture, width, structure) \
	  ((posture) + (4 * (width)) + (32 * (structure)))
	{"Courier", {'C','o','u','r','i','e','r',' ',' ',' ',' ',' ',' ',' ',' ',' '},
	 {0, 0, pitch_1, 0, style_word(0, 0, 0), 0, face_val(3, agfa)},
	 cc_alphabetic},
	{"CG Times", {'C','G',' ','T','i','m','e','s',' ',' ',' ',' ',' ',' ',' ',' '},
	 {0, 1, pitch_1, 0, style_word(0, 0, 0), 0, face_val(5, agfa)},
	 cc_alphabetic},
	{"CG Times Bold", {'C','G',' ','T','i','m','e','s',' ',' ',' ',' ',' ',' ','B','d'},
	 {0, 1, pitch_1, 0, style_word(0, 0, 0), 3, face_val(5, agfa)},
	 cc_alphabetic},
	{"CG Times Italic", {'C','G',' ','T','i','m','e','s',' ',' ',' ',' ',' ',' ','I','t'},
	 {0, 1, pitch_1, 0, style_word(1, 0, 0), 0, face_val(5, agfa)},
	 cc_alphabetic},
	{"CG Times Bold Italic", {'C','G',' ','T','i','m','e','s',' ',' ',' ',' ','B','d','I','t'},
	 {0, 1, pitch_1, 0, style_word(1, 0, 0), 3, face_val(5, agfa)},
	 cc_alphabetic},
	{"CG Omega", {'C','G',' ','O','m','e','g','a',' ',' ',' ',' ',' ',' ',' ',' '},
	 {0, 1, pitch_1, 0, style_word(0, 0, 0), 0, face_val(17, agfa)},
	 cc_alphabetic},
	{"CG Omega Bold", {'C','G',' ','O','m','e','g','a',' ',' ',' ',' ',' ',' ','B','d'},
	 {0, 1, pitch_1, 0, style_word(0, 0, 0), 3, face_val(17, agfa)},
	 cc_alphabetic},
	{"CG Omega Italic", {'C','G',' ','O','m','e','g','a',' ',' ',' ',' ',' ',' ','I','t'},
	 {0, 1, pitch_1, 0, style_word(1, 0, 0), 0, face_val(17, agfa)},
	 cc_alphabetic},
	{"CG Omega Bold Italic", {'C','G',' ','O','m','e','g','a',' ',' ',' ',' ','B','d','I','t'},
	 {0, 1, pitch_1, 0, style_word(1, 0, 0), 3, face_val(17, agfa)},
	 cc_alphabetic},
	{"Coronet", {'C','o','r','o','n','e','t',' ',' ',' ',' ',' ',' ',' ',' ',' '},
	 {0, 1, pitch_1, 0, style_word(1, 0, 0), 0, face_val(20, agfa)},
	 cc_alphabetic},
	{"Clarendon Condensed Bold", {'C','l','a','r','e','n','d','o','n',' ',' ',' ','C','d','B','d'},
	 {0, 1, pitch_1, 0, style_word(0, 1, 0), 3, face_val(44, agfa)},
	 cc_alphabetic},
	{"Univers Medium", {'U','n','i','v','e','r','s',' ',' ',' ',' ',' ',' ',' ','M','d'},
	 {0, 1, pitch_1, 0, style_word(0, 0, 0), 0, face_val(52, agfa)},
	 cc_alphabetic},
	{"Univers Bold", {'U','n','i','v','e','r','s',' ',' ',' ',' ',' ',' ',' ','B','d'},
	 {0, 1, pitch_1, 0, style_word(0, 0, 0), 3, face_val(52, agfa)},
	 cc_alphabetic},
	{"Univers Medium Italic", {'U','n','i','v','e','r','s',' ',' ',' ',' ',' ','M','d','I','t'},
	 {0, 1, pitch_1, 0, style_word(1, 0, 0), 0, face_val(52, agfa)},
	 cc_alphabetic},
	{"Univers Bold Italic", {'U','n','i','v','e','r','s',' ',' ',' ',' ',' ','B','d','I','t'},
	 {0, 1, pitch_1, 0, style_word(1, 0, 0), 3, face_val(52, agfa)},
	 cc_alphabetic},
	{"Univers Condensed Medium", {'U','n','i','v','e','r','s',' ',' ',' ',' ',' ','C','d','M','d'},
	 {0, 1, pitch_1, 0, style_word(0, 1, 0), 0, face_val(52, agfa)},
	 cc_alphabetic},
	{"Univers Condensed Bold", {'U','n','i','v','e','r','s',' ',' ',' ',' ',' ','C','d','B','d'},
	 {0, 1, pitch_1, 0, style_word(0, 1, 0), 3, face_val(52, agfa)},
	 cc_alphabetic},
	{"Univers Condensed Medium Italic", {'U','n','i','v','e','r','s',' ',' ',' ','C','d','M','d','I','t'},
	 {0, 1, pitch_1, 0, style_word(1, 1, 0), 0, face_val(52, agfa)},
	 cc_alphabetic},
	{"Univers Condensed Bold Italic", {'U','n','i','v','e','r','s',' ',' ',' ','C','d','B','d','I','t'},
	 {0, 1, pitch_1, 0, style_word(1, 1, 0), 3, face_val(52, agfa)},
	 cc_alphabetic},
	{"Antique Olive", {'A','n','t','i','q','O','l','i','v','e',' ',' ',' ',' ',' ',' '},
	 {0, 1, pitch_1, 0, style_word(0, 0, 0), 0, face_val(72, agfa)},
	 cc_alphabetic},
	{"Antique Olive Bold", {'A','n','t','i','q','O','l','i','v','e',' ',' ',' ',' ','B','d'},
	 {0, 1, pitch_1, 0, style_word(0, 0, 0), 3,
	  face_val(72, agfa)}, cc_alphabetic},
	{"Antique Olive Italic", {'A','n','t','i','q','O','l','i','v','e',' ',' ',' ',' ','I','t'},
	 {0, 1, pitch_1, 0, style_word(1, 0, 0), 0, face_val(72, agfa)},
	 cc_alphabetic},
	{"Garamond Antiqua", {'G','a','r','a','m','o','n','d',' ','A','n','t','i','q','u','a'},
	 {0, 1, pitch_1, 0, style_word(0, 0, 0), 0, face_val(101, agfa)}, cc_alphabetic},
	{"Garamond Halbfett", {'G','a','r','a','m','o','n','d',' ',' ',' ',' ',' ','H','l','b'},
	 {0, 1, pitch_1, 0, style_word(0, 0, 0), 3, face_val(101, agfa)}, cc_alphabetic},
	{"Garamond Kursiv", {'G','a','r','a','m','o','n','d',' ',' ',' ',' ','K','r','s','v'},
	 {0, 1, pitch_1, 0, style_word(1, 0, 0), 0,
	  face_val(101, agfa)}, cc_alphabetic},
	{"Garamond Kursiv Halbfett", {'G','a','r','a','m','o','n','d',' ','K','r','s','v','H','l','b'},
	 {0, 1, pitch_1, 0, style_word(1, 0, 0), 3,
	  face_val(101, agfa)}, cc_alphabetic},
	{"Marigold", {'M','a','r','i','g','o','l','d',' ',' ',' ',' ',' ',' ',' ',' '},
	 {0, 1, pitch_1, 0, style_word(0, 0, 0), 0, face_val(201, agfa)},
	 cc_alphabetic},
	{"Albertus Medium", {'A','l','b','e','r','t','u','s',' ',' ',' ',' ',' ',' ','M','d'},
	 {0, 1, pitch_1, 0, style_word(0, 0, 0), 1,
	  face_val(266, agfa)}, cc_alphabetic},
	{"Albertus Extra Bold", {'A','l','b','e','r','t','u','s',' ',' ',' ',' ',' ',' ','X','b'},
	 {0, 1, pitch_1, 0, style_word(0, 0, 0), 4,
	  face_val(266, agfa)}, cc_alphabetic},
	{"Arial", {'A','r','i','a','l',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' '},
	 {0, 1, pitch_1, 0, style_word(0, 0, 0), 0,
	  face_val(218, agfa)}, cc_alphabetic},
	{"Arial Bold", {'A','r','i','a','l',' ',' ',' ',' ',' ',' ',' ',' ',' ','B','d'},
	 {0, 1, pitch_1, 0, style_word(0, 0, 0), 3,
	  face_val(218, agfa)}, cc_alphabetic},
	{"Arial Italic", {'A','r','i','a','l',' ',' ',' ',' ',' ',' ',' ',' ',' ','I','t'},
	 {0, 1, pitch_1, 0, style_word(1, 0, 0), 0, face_val(218, agfa)}, cc_alphabetic},
	{"Arial Bold Italic", {'A','r','i','a','l',' ',' ',' ',' ',' ',' ',' ','B','d','I','t'},
	 {0, 1, pitch_1, 0, style_word(1, 0, 0), 3, face_val(218, agfa)}, cc_alphabetic},
	{"Times New Roman", {'T','i','m','e','s','N','e','w','R','m','n',' ',' ',' ',' ',' '},
	 {0, 1, pitch_1, 0, style_word(0, 0, 0), 0, face_val(517, agfa)}, 
	 cc_alphabetic},
	{"Times New Roman Bold",{'T','i','m','e','s','N','e','w','R','m','n',' ',' ',' ','B','d'},
	 {0, 1, pitch_1, 0, style_word(0, 0, 0), 3,
	  face_val(517, agfa)}, cc_alphabetic},
	{"Times New Roman Italic", {'T','i','m','e','s','N','e','w','R','m','n',' ',' ',' ','I','t'},
	 {0, 1, pitch_1, 0, style_word(1, 0, 0), 0,
	  face_val(517, agfa)}, cc_alphabetic},
	{"Times New Roman Bold Italic", {'T','i','m','e','s','N','e','w','R','m','n',' ','B','d','I','t'},
	 {0, 1, pitch_1, 0, style_word(1, 0, 0), 3,
	  face_val(517, agfa)}, cc_alphabetic},
	{"Symbol", {'S','y','m','b','o','l',' ',' ',' ',' ',' ',' ',' ',' ',' ',' '},
	 {621, 1, pitch_1, 0, style_word(0, 0, 0), 0,
	  face_val(302, agfa)}, cc_symbol},
	{"Wingdings", {'W','i','n','g','d','i','n','g','s',' ',' ',' ',' ',' ',' ',' '},
	 {18540, 1, pitch_1,0, style_word(0, 0, 0), 0,
	  face_val(2730, agfa)}, cc_dingbats},
	{"Courier Bold", {'C','o','u','r','i','e','r',' ',' ',' ',' ',' ',' ',' ','B','d'},
	 {0, 0, pitch_1, 0, style_word(0, 0, 0), 3, face_val(3, agfa)}, 
	 cc_alphabetic },
	{"Courier Italic", {'C','o','u','r','i','e','r',' ',' ',' ',' ',' ',' ',' ','I','t'},
	 {0, 0, pitch_1, 0, style_word(1, 0, 0), 0, face_val(3, agfa)},
	 cc_alphabetic },
	{"Courier Bold Italic", {'C','o','u','r','i','e','r',' ',' ',' ',' ',' ','B','d','I','t'},
	 {0, 0, pitch_1, 0, style_word(1, 0, 0), 3, face_val(3, agfa)},
	 cc_alphabetic},
	{"Letter Gothic", {'L','e','t','t','e','r','G','o','t','h','i','c',' ',' ',' ',' '},
	 {0, 0, pitch_1, 0, style_word(0, 0, 0), 0, face_val(6, agfa)},
	 cc_alphabetic},
	{"Letter Gothic Bold",
	 {'L','e','t','t','e','r','G','o','t','h','i','c',' ',' ','B','d'},
	 {0, 0, pitch_1, 0, style_word(0, 0, 0), 3, face_val(6, agfa)},
	 cc_alphabetic},
	{"Letter Gothic Italic", 
	 {'L','e','t','t','e','r','G','o','t','h','i','c',' ',' ','I','t'},
	 {0, 0, pitch_1, 0, style_word(1, 0, 0), 0, face_val(6, agfa)},
	 cc_alphabetic},
	/* Note that "bound" TrueType fonts are indexed starting at 0xf000, */
	/* not at 0. */
	{"", {'0','0'},
	 {0, 0, pitch_1, 0, 0, 0, 0} }
#undef C
#undef cc_alphabetic
#undef cc_symbol
#undef cc_dingbats
#undef pitch_1
#undef agfa_value
#undef face_val
    };
    pl_font_t *font_found[countof(resident_table)];

    /* don't load fonts more than once */
    if (pl_dict_length(pfontdict, true) > 0)
	return true;

#define MAX_OPEN_LIBRARIES  5   /* NB */
#define BITMAP_WIDTH        1   /* must be 1, 2, 4 or 8 */

    strcpy(config_block.ufstPath, ufstPath);
    strcpy(config_block.typePath, typePath);
    config_block.num_files = MAX_OPEN_LIBRARIES;  /* max open library files */
    config_block.bit_map_width = BITMAP_WIDTH;    /* bitmap width 1, 2 or 4 */

    /* the following part should be called only once at the beginning */
    if ((status = CGIFinit()) != 0) {
        dprintf1 ("CGIFinit() error: %d\n", status);
        return TRUE;
    }
    if ((status = CGIFconfig(&config_block)) != 0) {
        dprintf1 ("CGIFconfig() error: %d\n", status);
        return TRUE;
    }
    if ((status = CGIFenter()) != 0) {
        dprintf1 ("CGIFenter() error: %u\n",status);
        return TRUE;
    }

    /* open and register the plug-in font collection object */
    strcpy((char *)pthnm, UFST_DATA_ROOT);
    strcat((char *)pthnm, fcNmPl);
    if ((status = CGIFfco_Open(pthnm,(LPSW16)&fcHndlPl)) != 0) {
        dprintf2("CGIFfco_Open error %d for %s\n", status, pthnm);
        return TRUE;
    }
    if ((status = CGIFfco_Plugin(fcHndlPl)) != 0) {
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

        strcpy((char *)pthnm, UFST_DATA_ROOT);
        strcat((char *)pthnm, (const char *)fcNmAry[k]);
        if ((status = CGIFfco_Open(pthnm, &fcHndlAry[k])) != 0) {
            dprintf2("CGIFfco_Open error %d for %s\n", status, pthnm);
            continue;
        }

        for ( i = 0;
              CGIFfco_Access(pthnm, i, TFATRIB_KEY, &bSize, NULL) == 0;
              i++, key[2] += 1 ) {
            LPSB8   pBuffer = (LPSB8)gs_alloc_bytes( mem,
                                                     bSize,
                                                     "TTFONTINFO buffer" );

            if (pBuffer == 0) {
                dprintf1("VM error for built-in font %d", i);
                continue;
            }
            status = CGIFfco_Access( pthnm,
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
                      j < sizeof(resident_table) / sizeof(resident_table[0]) &&
                      strcmp(resident_table[j].full_font_name, pname) != 0;
                      j++ )
                    ;
                if (j < sizeof(resident_table) / sizeof(resident_table[0])) {
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
        CGIFfco_Close(fcHndlPl);
    for (i = 0; i < sizeof(fcNmAry) / sizeof(fcNmAry[0]); i++) {
        if (fcHndlAry[i] != -1)
            CGIFfco_Close(fcHndlAry[i]);
    }
    CGIFexit();
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
