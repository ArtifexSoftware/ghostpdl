/* Copyright (C) 1996, 1997 Aladdin Enterprises.  All rights reserved.
 * Unauthorized use, copying, and/or distribution prohibited.
 */

/* pcfont.c */
/* PCL5 font selection and text printing commands */
#include "memory_.h"
/* XXX #include tangle:  We need pl_load_tt_font, which is only supplied
 * by plfont.h if stdio.h is included, but pcstate.h back-door includes
 * plfont.h, so we have to pull in stdio.h here even though it should only
 * be down local to the font-init-hack code. */
#include "stdio_.h"
#include "pcommand.h"
#include "pcstate.h"
#include "pcfont.h"
#include "pcfsel.h"

/*
 * It appears that the default symbol set in the LJ 6MP is PC-8,
 * not Roman-8.  Eventually we will have to deal with this in a more
 * systematic way, but for now, we just define it at compile time.
 */
#define ROMAN_8_SYMBOL_SET 277
/*#define DEFAULT_SYMBOL_SET 277*/	/* Roman-8 */
#define DEFAULT_SYMBOL_SET 341		/* PC-8 */

/*
 * Decache the HMI after resetting the font.  According to TRM 5-22,
 * this always happens, regardless of how the HMI was set; formerly,
 * we only invalidated the HMI if it was set from the font rather than
 * explicitly.
 */
#define pcl_decache_hmi(pcls)\
  do {\
    /*if ( pcls->hmi_set == hmi_set_from_font )*/\
      pcls->hmi_set = hmi_not_set;\
  } while (0)\


/* Clear the font pointer cache after changing a font parameter.  set
 * indicates which font (0/1 for primary/secondary).  -1 means both. */
void
pcl_decache_font(pcl_state_t *pcls, int set)
{	
	if ( set < 0 )
	  {
	    pcl_decache_font(pcls, 0);
	    pcl_decache_font(pcls, 1);
	  }
	else
	  {
	    pcls->font_selection[set].font = NULL;
	    if ( pcls->font_selected == set )
	      {
	        pcls->font = NULL;
		pcls->map = NULL;
		pcl_decache_hmi(pcls);
	      }
	  }
}

/* Recompute the current font from the descriptive parameters. */
/* This is exported for resetting HMI. */
int
pcl_recompute_font(pcl_state_t *pcls)
{	pcl_font_selection_t *pfs = &pcls->font_selection[pcls->font_selected];
	int code = pcl_reselect_font(pfs, pcls);

	if ( code < 0 )
	  return code;
	pcls->font = pfs->font;
	pcls->map = pfs->map;
	return 0;
}

/* The font parameter commands all come in primary and secondary variants. */

private int
pcl_symbol_set(pcl_args_t *pargs, pcl_state_t *pcls, int set)
{	uint num = uint_arg(pargs);
	uint symset;

	if ( num > 1023 )
	  return e_Range;
	/* The following algorithm is from Appendix C of the */
	/* PCL5 Comparison Guide. */
	symset = (num << 5) + pargs->command - 64;
	if ( symset != pcls->font_selection[set].params.symbol_set )
	  {
	    pcls->font_selection[set].params.symbol_set = symset;
	    pcl_decache_font(pcls, set);
	  }
	return 0;
}
private int /* ESC ( <> others */
pcl_primary_symbol_set(pcl_args_t *pargs, pcl_state_t *pcls)
{	return pcl_symbol_set(pargs, pcls, 0);
}
private int /* ESC ) <> others */
pcl_secondary_symbol_set(pcl_args_t *pargs, pcl_state_t *pcls)
{	return pcl_symbol_set(pargs, pcls, 1);
}

private int
pcl_spacing(pcl_args_t *pargs, pcl_state_t *pcls, int set)
{	uint spacing = uint_arg(pargs);

	if ( spacing > 1 )
	  return 0;
	if ( spacing != pcls->font_selection[set].params.proportional_spacing )
	  {
	    pcls->font_selection[set].params.proportional_spacing = spacing;
	    pcl_decache_font(pcls, set);
	  }
	return 0;
}
private int /* ESC ( s <prop_bool> P */
pcl_primary_spacing(pcl_args_t *pargs, pcl_state_t *pcls)
{	return pcl_spacing(pargs, pcls, 0);
}
private int /* ESC ) s <prop_bool> P */
pcl_secondary_spacing(pcl_args_t *pargs, pcl_state_t *pcls)
{	return pcl_spacing(pargs, pcls, 1);
}

private int
pcl_pitch(pcl_args_t *pargs, pcl_state_t *pcls, int set)
{	float cpi = float_arg(pargs) + 0.005;
	uint pitch_cp;
	pcl_font_selection_t *pfs = &pcls->font_selection[set];

	if ( cpi < 0.1 )
	  cpi = 0.1;
	/* Convert characters per inch to 100ths of design units. */
	pitch_cp = 7200.0 / cpi;
	if ( pitch_cp > 65535 )
	  return e_Range;
	if ( pitch_cp < 1 )
	  pitch_cp = 1;
	if ( pitch_cp != pl_fp_pitch_cp(&pfs->params) )
	  {
	    pl_fp_set_pitch_cp(&pfs->params, pitch_cp);
	    if ( !pfs->selected_by_id )
	      pcl_decache_font(pcls, set);
	  }
	return 0;
}
private int /* ESC ( s <pitch> H */
pcl_primary_pitch(pcl_args_t *pargs, pcl_state_t *pcls)
{	return pcl_pitch(pargs, pcls, 0);
}
private int /* ESC ) s <pitch> H */
pcl_secondary_pitch(pcl_args_t *pargs, pcl_state_t *pcls)
{	return pcl_pitch(pargs, pcls, 1);
}

private int
pcl_height(pcl_args_t *pargs, pcl_state_t *pcls, int set)
{	uint height_4ths = (uint)(float_arg(pargs) * 4 + 0.5);
	pcl_font_selection_t *pfs = &pcls->font_selection[set];

	if ( height_4ths >= 4000 )
	  return e_Range;
	if ( height_4ths != pfs->params.height_4ths )
	  {
	    pfs->params.height_4ths = height_4ths;
	    if ( !pfs->selected_by_id )
	      pcl_decache_font(pcls, set);
	  }
	return 0;
}
private int /* ESC ( s <height> V */
pcl_primary_height(pcl_args_t *pargs, pcl_state_t *pcls)
{	return pcl_height(pargs, pcls, 0);
}
private int /* ESC ) s <height> V */
pcl_secondary_height(pcl_args_t *pargs, pcl_state_t *pcls)
{	return pcl_height(pargs, pcls, 1);
}

private int
pcl_style(pcl_args_t *pargs, pcl_state_t *pcls, int set)
{	uint style = uint_arg(pargs);

	if ( style != pcls->font_selection[set].params.style )
	  {
	    pcls->font_selection[set].params.style = style;
	    pcl_decache_font(pcls, set);
	  }
	return 0;
}
private int /* ESC ( s <style> S */
pcl_primary_style(pcl_args_t *pargs, pcl_state_t *pcls)
{	return pcl_style(pargs, pcls, 0);
}
private int /* ESC ) s <style> S */
pcl_secondary_style(pcl_args_t *pargs, pcl_state_t *pcls)
{	return pal_style(pargs, pcls, 1);
}

private int
pcl_stroke_weight(pcl_args_t *pargs, pcl_state_t *pcls, int set)
{	int weight = int_arg(pargs);

	if ( weight < -7 )
	  weight = -7;
	else if ( weight > 7 )
	  weight = 7;
	if ( weight != pcls->font_selection[set].params.stroke_weight )
	  {
	    pcls->font_selection[set].params.stroke_weight = weight;
	    pcl_decache_font(pcls, set);
	  }
	return 0;
}
private int /* ESC ( s <weight> B */
pcl_primary_stroke_weight(pcl_args_t *pargs, pcl_state_t *pcls)
{	return pcl_stroke_weight(pargs, pcls, 0);
}
private int /* ESC ) s <weight> B */
pcl_secondary_stroke_weight(pcl_args_t *pargs, pcl_state_t *pcls)
{	return pcl_stroke_weight(pargs, pcls, 1);
}

private int
pcl_typeface(pcl_args_t *pargs, pcl_state_t *pcls, int set)
{	uint typeface = uint_arg(pargs);

	if ( typeface != pcls->font_selection[set].params.typeface_family )
	  {
	    pcls->font_selection[set].params.typeface_family = typeface;
	    pcl_decache_font(pcls, set);
	  }
	return 0;
}
private int /* ESC ( s <typeface> T */
pcl_primary_typeface(pcl_args_t *pargs, pcl_state_t *pcls)
{	return pcl_typeface(pargs, pcls, 0);
}
private int /* ESC ) s <typeface> T */
pcl_secondary_typeface(pcl_args_t *pargs, pcl_state_t *pcls)
{	return pcl_typeface(pargs, pcls, 1);
}

private int
pcl_font_selection_id(pcl_args_t *pargs, pcl_state_t *pcls, int set)
{	uint id = uint_arg(pargs);
	pcl_font_selection_t *pfs = &pcls->font_selection[set];
	int code = pcl_select_font_by_id(pfs, id, pcls);

	switch ( code )
	  {
	  default:		/* error */
	    return code;
	  case 0:
	    if ( pcls->font_selected == set )
	      pcls->font = pfs->font;
	    pcl_decache_hmi(pcls);
	  case 1:		/* not found */
	    return 0;
	  }
}
private int /* ESC ( <font_id> X */
pcl_primary_font_selection_id(pcl_args_t *pargs, pcl_state_t *pcls)
{	return pcl_font_selection_id(pargs, pcls, 0);
}
private int /* ESC ) <font_id> X */
pcl_secondary_font_selection_id(pcl_args_t *pargs, pcl_state_t *pcls)
{	return pcl_font_selection_id(pargs, pcls, 1);
}

private int
pcl_select_default_font(pcl_args_t *pargs, pcl_state_t *pcls, int set)
{	if ( int_arg(pargs) != 3 )
	  return e_Range;
	pcl_decache_font(pcls, set);
	return e_Unimplemented;
}
private int /* ESC ( 3 @ */
pcl_select_default_font_primary(pcl_args_t *pargs, pcl_state_t *pcls)
{	return pcl_select_default_font(pargs, pcls, 0);
}
private int /* ESC ) 3 @ */
pcl_select_default_font_secondary(pcl_args_t *pargs, pcl_state_t *pcls)
{	return pcl_select_default_font(pargs, pcls, 1);
}

private int /* SO */
pcl_SO(pcl_args_t *pargs, pcl_state_t *pcls)
{	if ( pcls->font_selected != 1 )
	  { pcls->font_selected = 1;
	    pcls->font = pcls->font_selection[1].font;
	    pcl_decache_hmi(pcls);
	  }
	return 0;
}

private int /* SI */
pcl_SI(pcl_args_t *pargs, pcl_state_t *pcls)
{	if ( pcls->font_selected != 0 )
	  { pcls->font_selected = 0;
	    pcls->font = pcls->font_selection[0].font;
	    pcl_decache_hmi(pcls);
	  }
	return 0;
}

/* This command is listed only on p. A-7 of the PCL5 comparison guide. */
/* It is not referenced or documented anywhere! */
private int /* ESC & k <mode> S */
pcl_set_pitch_mode(pcl_args_t *pargs, pcl_state_t *pcls)
{	int mode = int_arg(pargs);
	switch ( mode )
	  {
	  case 0: case 2: case 4:
	    break;
	  default:
	    return e_Range;
	  }
	return e_Unimplemented;
}

/* Initialization */
private int
pcfont_do_init(gs_memory_t *mem)
{		/* Register commands */
	{ int chr;
	  /*
	   * The H-P manual only talks about A through Z, but it appears
	   * that characters from A through ^ (94) can be used.
	   */
	  for ( chr = 'A'; chr <= '^'; ++chr )
	    if ( chr != 'X' )
	      { DEFINE_CLASS_COMMAND_ARGS('(', 0, chr, "Primary Symbol Set",
					  pcl_primary_symbol_set,
					  pca_neg_error|pca_big_error);
	        DEFINE_CLASS_COMMAND_ARGS(')', 0, chr, "Secondary Symbol Set",
					  pcl_secondary_symbol_set,
					  pca_neg_error|pca_big_error);
	      }
	}
	DEFINE_CLASS('(')
	  {'s', 'P',
	     PCL_COMMAND("Primary Spacing", pcl_primary_spacing,
			 pca_neg_ignore|pca_big_ignore)},
	  {'s', 'H',
	     PCL_COMMAND("Primary Pitch", pcl_primary_pitch,
			 pca_neg_error|pca_big_error)},
	  {'s', 'V',
	     PCL_COMMAND("Primary Height", pcl_primary_height,
			 pca_neg_error|pca_big_error)},
	  {'s', 'S',
	     PCL_COMMAND("Primary Style", pcl_primary_style,
			 pca_neg_error|pca_big_clamp)},
	  {'s', 'B',
	     PCL_COMMAND("Primary Stroke Weight", pcl_primary_stroke_weight,
			 pca_neg_ok|pca_big_error)},
	  {'s', 'T',
	     PCL_COMMAND("Primary Typeface", pcl_primary_typeface,
			 pca_neg_error|pca_big_ok)},
	  {0, 'X',
	     PCL_COMMAND("Primary Font Selection ID",
			 pcl_primary_font_selection_id,
			 pca_neg_error|pca_big_error)},
	  {0, '@',
	     PCL_COMMAND("Default Font Primary",
			 pcl_select_default_font_primary,
			 pca_neg_error|pca_big_error)},
	END_CLASS
	DEFINE_CLASS(')')
	  {'s', 'P',
	     PCL_COMMAND("Secondary Spacing", pcl_secondary_spacing,
			 pca_neg_ignore|pca_big_ignore)},
	  {'s', 'H',
	     PCL_COMMAND("Secondary Pitch", pcl_secondary_pitch,
			 pca_neg_error|pca_big_error)},
	  {'s', 'V',
	     PCL_COMMAND("Secondary Height", pcl_secondary_height,
			 pca_neg_error|pca_big_error)},
	  {'s', 'S',
	     PCL_COMMAND("Secondary Style", pcl_secondary_style,
			 pca_neg_error|pca_big_clamp)},
	  {'s', 'B',
	     PCL_COMMAND("Secondary Stroke Weight",
			 pcl_secondary_stroke_weight,
			 pca_neg_ok|pca_big_error)},
	  {'s', 'T',
	     PCL_COMMAND("Secondary Typeface", pcl_secondary_typeface,
			 pca_neg_error|pca_big_ok)},
	  {0, 'X',
	     PCL_COMMAND("Secondary Font Selection ID",
			 pcl_secondary_font_selection_id,
			 pca_neg_error|pca_big_error)},
	  {0, '@',
	     PCL_COMMAND("Default Font Secondary",
			 pcl_select_default_font_secondary,
			 pca_neg_error|pca_big_error)},
	END_CLASS
	DEFINE_CONTROL(SO, "SO", pcl_SO)
	DEFINE_CONTROL(SI, "SI", pcl_SI)
	DEFINE_CLASS('&')
	  {'k', 'S',
	     PCL_COMMAND("Set Pitch Mode", pcl_set_pitch_mode,
			 pca_neg_error|pca_big_error)},
	END_CLASS
	return 0;
}
private void
pcfont_do_reset(pcl_state_t *pcls, pcl_reset_type_t type)
{	if ( type & (pcl_reset_initial | pcl_reset_printer) )
	  { pcls->font_selection[0].params.symbol_set =
	      DEFAULT_SYMBOL_SET;
	    pcls->font_selection[0].params.proportional_spacing = false;
	    pl_fp_set_pitch_per_inch(&pcls->font_selection[0].params, 10);
	    pcls->font_selection[0].params.height_4ths = 12*4;
	    pcls->font_selection[0].params.style = 0;
	    pcls->font_selection[0].params.stroke_weight = 0;
	    pcls->font_selection[0].params.typeface_family = 3;	/* Courier */
	    pcls->font_selection[0].font = 0;		/* not looked up yet */
	    pcls->font_selection[1] = pcls->font_selection[0];
	    pcls->font_selected = 0;
	    pcls->font = 0;
	  }
}

/* Load some built-in fonts.  This must be done at initialization time, but
 * after the state and memory are set up.  Return an indication of whether
 * at least one font was successfully loaded.
 * XXX The existing code is more than a bit of a hack; however it does
 * allow us to see some text.  Approach: expect to find some fonts in
 * one or more of a given list of directories with names
 * *.ttf.  Load whichever ones are found in the table below.  Probably
 * very little of this code can be salvaged for later.
 */
bool
pcl_load_built_in_fonts(pcl_state_t *pcls, const char *prefixes[])
{	const char **pprefix;
#include "string_.h"
#include "gp.h"
#include "gsutil.h"
#include "gxfont.h"
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
	    {"arial",	{0, 1, pitch_1, 0, 0, 0, 16602}, cc_alphabetic },
	    {"arialbd",	{0, 1, pitch_1, 0, 0, 3, 16602}, cc_alphabetic },
	    {"arialbi",	{0, 1, pitch_1, 0, 1, 3, 16602}, cc_alphabetic },
	    {"ariali",	{0, 1, pitch_1, 0, 1, 0, 16602}, cc_alphabetic },
	    {"cour",	{0, 0, pitch_1, 0, 0, 0,  4099}, cc_alphabetic },
	    {"courbd",	{0, 0, pitch_1, 0, 0, 3,  4099}, cc_alphabetic },
	    {"courbi",	{0, 0, pitch_1, 0, 1, 3,  4099}, cc_alphabetic },
	    {"couri",	{0, 0, pitch_1, 0, 1, 0,  4099}, cc_alphabetic },
	    {"times",	{0, 1, pitch_1, 0, 0, 0, 16901}, cc_alphabetic },
	    {"timesbd",	{0, 1, pitch_1, 0, 0, 3, 16901}, cc_alphabetic },
	    {"timesbi",	{0, 1, pitch_1, 0, 1, 3, 16901}, cc_alphabetic },
	    {"timesi",	{0, 1, pitch_1, 0, 1, 0, 16901}, cc_alphabetic },
	    {"symbol",	{621,1,pitch_1, 0, 0, 0, 16686}, cc_symbol },
	    {"wingding",{2730,1,pitch_1,0, 0, 0, 19114}, cc_dingbats },
	    {NULL,	{0, 0, pitch_1, 0, 0, 0, 0} }
#undef C
#undef cc_alphabetic
#undef cc_symbol
#undef cc_dingbats
	};
	const font_hack_t *hackp;
	byte font_found[countof(hack_table)];
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
		font_found[hackp - hack_table] = 1;
		found_some = true;
	    }
	return found_some;
}

const pcl_init_t pcfont_init = {
  pcfont_do_init, pcfont_do_reset
};
