/* Copyright (C) 1997, 1998 Aladdin Enterprises.  All rights reserved.
 * Unauthorized use, copying, and/or distribution prohibited.
 */

/* pclfont.c */
/* PCL5 font preloading */
#include "stdio_.h"
#include "string_.h"
/* The following are all for gxfont42.h, except for gp.h. */
#include "gx.h"
#include "gp.h"
#include "gsccode.h"
#include "gsmatrix.h"
#include "gsutil.h"
#include "gxfont.h"
#include "gxfont42.h"
#include "pcommand.h"
#include "pcstate.h"

/* Load some built-in fonts.  This must be done at initialization time, but
 * after the state and memory are set up.  Return an indication of whether
 * at least one font was successfully loaded.  XXX The existing code is more
 * than a bit of a hack.  Approach: expect to find some fonts in one or more
 * of a given list of directories with names *.ttf.  Load whichever ones are
 * found in the table below.  Probably very little of this code can be
 * salvaged for later.
 */
bool
pcl_load_built_in_fonts(pcl_state_t *pcls, const char *prefixes[])
{	const char **pprefix;
	typedef struct font_resident {
	  const char *ext_name;
	  pl_font_params_t params;
	  byte character_complement[8];
	} font_resident_t;
	static const font_resident_t resident_table[] = {
	  /*
	   * Symbol sets, typeface family values, and character complements
	   * are faked; they do not (necessarily) match the actual fonts.
	   */
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
	  {"letri", {0, 0, pitch_1, 0, style_word(1, 0, 0), 0,
		     face_val(6, agfa)}, cc_alphabetic},
	  {"letrbi", {0, 0, pitch_1, 0, style_word(1, 0, 0), 3,
		      face_val(6, agfa)}, cc_alphabetic},
	  {"letrb", {0, 0, pitch_1, 0, style_word(0, 0, 0), 3,
		     face_val(6, agfa)}, cc_alphabetic},
	  {"letr", {0, 0, pitch_1, 0, style_word(0, 0, 0), 0,
		    face_val(6, agfa)}, cc_alphabetic },
	  {"courbi", {0, 0, pitch_1, 0, style_word(1, 0, 0), 3,
		      face_val(3, agfa)}, cc_alphabetic},
	  {"couri", {0, 0, pitch_1, 0, style_word(1, 0, 0),
		     0, face_val(3, agfa)}, cc_alphabetic },
	  {"courbd", {0, 0, pitch_1, 0, style_word(0, 0, 0), 3,
		      face_val(3, agfa)}, cc_alphabetic },
	  {"cour", {0, 0, pitch_1, 0, style_word(0, 0, 0), 0,
		    face_val(3, agfa)}, cc_alphabetic},
	  /* Note that "bound" TrueType fonts are indexed starting at 0xf000, */
	  /* not at 0. */
	  {"wingding", {18540, 1, pitch_1,0, style_word(0, 0, 0), 0,
			face_val(2730, agfa)}, cc_dingbats},
	  {"symbol", {621, 1, pitch_1, 0, style_word(0, 0, 0), 0,
		      face_val(302, agfa)}, cc_symbol},
	  {"timesbi", {0, 1, pitch_1, 0, style_word(1, 0, 0), 3,
		       face_val(517, agfa)}, cc_alphabetic},
	  {"timesi", {0, 1, pitch_1, 0, style_word(1, 0, 0), 0,
		      face_val(517, agfa)}, cc_alphabetic},
	  {"timesbd", {0, 1, pitch_1, 0, style_word(0, 0, 0), 3,
		       face_val(517, agfa)}, cc_alphabetic},
	  {"times", {0, 1, pitch_1, 0, style_word(0, 0, 0), 0,
		     face_val(517, agfa)}, cc_alphabetic},
	  {"arialbi", {0, 1, pitch_1, 0, style_word(1, 0, 0), 3,
		       face_val(218, agfa)}, cc_alphabetic},
	  {"ariali", {0, 1, pitch_1, 0, style_word(1, 0, 0), 0,
		      face_val(218, agfa)}, cc_alphabetic},
	  {"arialbd", {0, 1, pitch_1, 0, style_word(0, 0, 0), 3,
		       face_val(218, agfa)}, cc_alphabetic},
	  {"arial", {0, 1, pitch_1, 0, style_word(0, 0, 0), 0,
		     face_val(218, agfa)}, cc_alphabetic},
	  {"albrmdi", {0, 1, pitch_1, 0, style_word(1, 0, 0), 0,
		       face_val(266, agfa)}, cc_alphabetic},
	  {"albrxb", {0, 1, pitch_1, 0, style_word(0, 0, 0), 4,
		      face_val(266, agfa)}, cc_alphabetic},
	  {"albrmd", {0, 1, pitch_1, 0, style_word(0, 0, 0), 0,
		      face_val(266, agfa)}, cc_alphabetic},
	  {"mari", {0, 1, pitch_1, 0, style_word(0, 0, 0), 0,
		    face_val(201, agfa)}, cc_alphabetic},
	  {"garrkh", {0, 1, pitch_1, 0, style_word(1, 0, 0), 3,
		      face_val(101, agfa)}, cc_alphabetic},
	  {"garak", {0, 1, pitch_1, 0, style_word(1, 0, 0), 0,
		     face_val(101, agfa)}, cc_alphabetic},
	  {"garrah", {0, 1, pitch_1, 0, style_word(0, 0, 0), 3,
		      face_val(101, agfa)}, cc_alphabetic},
	  {"garaa", {0, 1, pitch_1, 0, style_word(0, 0, 0), 0,
		     face_val(101, agfa)}, cc_alphabetic},
	  {"antoi", {0, 1, pitch_1, 0, style_word(1, 0, 0), 0,
		     face_val(72, agfa)}, cc_alphabetic},
	  {"antob", {0, 1, pitch_1, 0, style_word(0, 0, 0), 3,
		     face_val(72, agfa)}, cc_alphabetic},
	  {"anto", {0, 1, pitch_1, 0, style_word(0, 0, 0), 0,
		    face_val(72, agfa)}, cc_alphabetic },
	  {"univcbi", {0, 1, pitch_1, 0, style_word(1, 1, 0), 3,
		       face_val(52, agfa)}, cc_alphabetic},
	  {"univmci", {0, 1, pitch_1, 0, style_word(1, 1, 0), 0,
		       face_val(52, agfa)}, cc_alphabetic},
	  {"univcb", {0, 1, pitch_1, 0, style_word(0, 1, 0), 3,
		      face_val(52, agfa)}, cc_alphabetic},
	  {"univmc", {0, 1, pitch_1, 0, style_word(0, 1, 0), 0,
		      face_val(52, agfa)}, cc_alphabetic},
	  {"univbi", {0, 1, pitch_1, 0, style_word(1, 0, 0), 3,
		      face_val(52, agfa)}, cc_alphabetic},
	  {"univmi", {0, 1, pitch_1, 0, style_word(1, 0, 0), 0,
		      face_val(52, agfa)}, cc_alphabetic},
	  {"univb", {0, 1, pitch_1, 0, style_word(0, 0, 0), 3,
		     face_val(52, agfa)}, cc_alphabetic},
	  {"univm", {0, 1, pitch_1, 0, style_word(0, 0, 0), 0,
		     face_val(52, agfa)}, cc_alphabetic },
	  {"clarbc", {0, 1, pitch_1, 0, style_word(0, 1, 0), 3,
		      face_val(44, agfa)}, cc_alphabetic},
	  {"coro", {0, 1, pitch_1, 0, style_word(1, 0, 0), 0,
		    face_val(20, agfa)}, cc_alphabetic},
	  {"cgombi", {0, 1, pitch_1, 0, style_word(1, 0, 0), 3,
		      face_val(17, agfa)}, cc_alphabetic},
	  {"cgomi", {0, 1, pitch_1, 0, style_word(1, 0, 0), 0,
		     face_val(17, agfa)}, cc_alphabetic},
	  {"cgomb", {0, 1, pitch_1, 0, style_word(0, 0, 0), 3,
		     face_val(17, agfa)}, cc_alphabetic},
	  {"cgom", {0, 1, pitch_1, 0, style_word(0, 0, 0), 0,
		    face_val(17, agfa)}, cc_alphabetic},
	  {"cgtibi", {0, 1, pitch_1, 0, style_word(1, 0, 0), 3,
		      face_val(5, agfa)}, cc_alphabetic},
	  {"cgtii", {0, 1, pitch_1, 0, style_word(1, 0, 0), 0,
		     face_val(5, agfa)}, cc_alphabetic},
	  {"cgtib", {0, 1, pitch_1, 0, style_word(0, 0, 0), 3,
		     face_val(5, agfa)}, cc_alphabetic},
	  {"cgti", {0, 1, pitch_1, 0, style_word(0, 0, 0), 0,
		    face_val(5, agfa)}, cc_alphabetic},
	  {NULL, {0, 0, pitch_1, 0, 0, 0, 0} }
#undef C
#undef cc_alphabetic
#undef cc_symbol
#undef cc_dingbats
#undef pitch_1
#undef agfa_value
#undef face_val
	};
	const font_resident_t *residentp;
	pl_font_t *font_found[countof(resident_table)];
	bool found_some = false;
	byte key[3];

	/* This initialization was moved from the do_reset procedures to
	 * this point to localize the font initialization in the state
	 * and have it in a place where memory allocation is safe. */
	pcls->font_dir = gs_font_dir_alloc(pcls->memory);
	pl_dict_init(&pcls->built_in_fonts, pcls->memory, pl_free_font);
	pl_dict_init(&pcls->soft_fonts, pcls->memory, pl_free_font);
	pl_dict_set_parent(&pcls->soft_fonts, &pcls->built_in_fonts);

	/* Load only those fonts we know about. */
	memset(font_found, 0, sizeof(font_found));
	/* We use a 3-byte key so the built-in fonts don't have */
	/* IDs that could conflict with user-defined ones. */
	key[0] = key[1] = key[2] = 0;
	for ( pprefix = prefixes; *pprefix != 0; ++pprefix )
	  for ( residentp = resident_table; residentp->ext_name != 0; ++residentp )
	    if ( !font_found[residentp - resident_table] )
	      { char fname[150];
	        FILE *fnp;
		pl_font_t *plfont;
	        strcpy(fname, *pprefix);
		strcat(fname, residentp->ext_name);
		strcat(fname, ".ttf");
		if ( (fnp=fopen(fname, gp_fmode_rb)) == NULL )
		  continue;
		if ( pl_load_tt_font(fnp, pcls->font_dir, pcls->memory,
				     gs_next_ids(1), &plfont) < 0 )
		  {
#ifdef DEBUG
		    lprintf1("Failed loading font %s\n", fname);
#endif
		    continue;
		  }
		plfont->storage = pcds_internal;
		if ( residentp->params.symbol_set != 0 )
		  plfont->font_type = plft_8bit;
		/*
		 * Don't smash the pitch, which was obtained
		 * from the actual font.
		 */
		{ pl_font_pitch_t save_pitch;
		  save_pitch = plfont->params.pitch;
		  plfont->params = residentp->params;
		  plfont->params.pitch = save_pitch;
		}
		memcpy(plfont->character_complement,
		       residentp->character_complement, 8);
		pl_dict_put(&pcls->built_in_fonts, key, sizeof(key), plfont);
		key[sizeof(key) - 1]++;
		font_found[residentp - resident_table] = plfont;
		found_some = true;
	    }
#ifdef DEBUG
	/* check that we have loaded everything in the resident font table */
	{ int i;
	  for (i=0; resident_table[i].ext_name != 0; i++)
	    if ( !font_found[i] )
		dprintf1( "Could not load resident font: %s\n",
			  resident_table[i].ext_name );
	}
#endif
	return found_some;
}
