/* Copyright (C) 1997 Aladdin Enterprises.  All rights reserved.
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
	typedef struct font_hack {
	  const char *ext_name;
	  pl_font_params_t params;
	  byte character_complement[8];
	} font_hack_t;
	static const font_hack_t hack_table[] = {
	    /*
	     * Symbol sets, typeface family values, and character complements
	     * are faked; they do not (necessarily) match the actual fonts.
	     */
#define C(b) ((byte)((b) ^ 0xff))
#define cc_alphabetic\
  { C(0), C(0), C(0), C(0), C(0xff), C(0xc0), C(0), C(plgv_Unicode) }
#define cc_symbol\
  { C(0), C(0), C(0), C(4), C(0), C(0), C(0), C(plgv_Unicode) }
#define cc_dingbats\
  { C(0), C(0), C(0), C(1), C(0), C(0), C(0), C(plgv_Unicode) }
#define pitch_1 fp_pitch_value_cp(1)
	  /*
	   * Per TRM 23-87, PCL5 printers are supposed to have Univers
	   * and CG Times fonts.  Substitute Arial for Univers and
	   * Times for CG Times.
	   */
#define tf_arial 4148		/* Univers; should be 16602 */
#define tf_cour 4099
#define tf_times 4101		/* CG Times; should be 16901 */
	    {"arial",	{0, 1, pitch_1, 0, 0, 0, tf_arial}, cc_alphabetic },
	    {"arialbd",	{0, 1, pitch_1, 0, 0, 3, tf_arial}, cc_alphabetic },
	    {"arialbi",	{0, 1, pitch_1, 0, 1, 3, tf_arial}, cc_alphabetic },
	    {"ariali",	{0, 1, pitch_1, 0, 1, 0, tf_arial}, cc_alphabetic },
	    {"cour",	{0, 0, pitch_1, 0, 0, 0, tf_cour}, cc_alphabetic },
	    {"courbd",	{0, 0, pitch_1, 0, 0, 3, tf_cour}, cc_alphabetic },
	    {"courbi",	{0, 0, pitch_1, 0, 1, 3, tf_cour}, cc_alphabetic },
	    {"couri",	{0, 0, pitch_1, 0, 1, 0, tf_cour}, cc_alphabetic },
	    {"times",	{0, 1, pitch_1, 0, 0, 0, tf_times}, cc_alphabetic },
	    {"timesbd",	{0, 1, pitch_1, 0, 0, 3, tf_times}, cc_alphabetic },
	    {"timesbi",	{0, 1, pitch_1, 0, 1, 3, tf_times}, cc_alphabetic },
	    {"timesi",	{0, 1, pitch_1, 0, 1, 0, tf_times}, cc_alphabetic },
  /* Note that "bound" TrueType fonts are indexed starting at 0xf000, */
  /* not at 0. */
	    {"symbol",	{621,1,pitch_1, 0, 0, 0, 16686}, cc_symbol },
	    {"wingding",{2730,1,pitch_1,0, 0, 0, 19114}, cc_dingbats },
	    {NULL,	{0, 0, pitch_1, 0, 0, 0, 0} }
#undef C
#undef cc_alphabetic
#undef cc_symbol
#undef cc_dingbats
	};
	const font_hack_t *hackp;
	pl_font_t *font_found[countof(hack_table)];
	bool found_some = false;
	byte key[3];
	int i;

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
	  for ( hackp = hack_table; hackp->ext_name != 0; ++hackp )
	    if ( !font_found[hackp - hack_table] )
	      { char fname[150];
	        FILE *fnp;
		pl_font_t *plfont;

	        strcpy(fname, *pprefix);
		strcat(fname, hackp->ext_name);
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
#ifdef DEBUG
		dprintf1("Loaded %s\n", fname);
#endif
		plfont->storage = pcds_internal;
		if ( hackp->params.symbol_set != 0 )
		  plfont->font_type = plft_8bit;
		/*
		 * Don't smash the pitch, which was obtained
		 * from the actual font.
		 */
		{ pl_font_pitch_t save_pitch;
		  save_pitch = plfont->params.pitch;
		  plfont->params = hackp->params;
		  plfont->params.pitch = save_pitch;
		}
		memcpy(plfont->character_complement,
		       hackp->character_complement, 8);
		pl_dict_put(&pcls->built_in_fonts, key, sizeof(key), plfont);
		key[sizeof(key) - 1]++;
		font_found[hackp - hack_table] = plfont;
		found_some = true;
	    }

	/* Create fake condensed versions of the Arial ("Univers") fonts. */

	for ( i = 0; i < 4; ++i )
	  if ( font_found[i] )
	    { const pl_font_t *orig_font = font_found[i];
	      gs_font_type42 *pfont =
		gs_alloc_struct(pcls->memory, gs_font_type42,
				&st_gs_font_type42,
				"pl_tt_load_font(gs_font_type42)");
	      pl_font_t *plfont =
		pl_alloc_font(pcls->memory, "pl_tt_load_font(pl_font_t)");
	      int code;

	      if ( pfont == 0 || plfont == 0 )
		return_error(gs_error_VMerror);
#define factor 0.7
	      /* Condense the Type 42 font. */
	      *pfont = *(const gs_font_type42 *)orig_font->pfont;
	      pfont->FontMatrix.xx *= factor;
	      pfont->FontBBox.p.x *= factor;
	      pfont->FontBBox.q.x *= factor;
	      /* Condense our font data. */
	      *plfont = *orig_font;
	      pl_fp_set_pitch_cp(&plfont->params,
				 pl_fp_pitch_cp(&plfont->params) * factor);
	      plfont->params.style |= 4; /* condensed */
	      plfont->pfont = (gs_font *)pfont;
#undef factor
	      code = gs_definefont(pcls->font_dir, (gs_font *)pfont);
	      if ( code < 0 )
		return code;
	      pl_dict_put(&pcls->built_in_fonts, key, sizeof(key), plfont);
	      key[sizeof(key) - 1]++;
	    }
	return found_some;
}
