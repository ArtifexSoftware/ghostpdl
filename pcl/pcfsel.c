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

/* pcfsel.c */
/* PCL5 / HP-GL/2 font selection */
#include "stdio_.h"		/* stdio for gdebug */
#include "gdebug.h"
#include "pcommand.h"
#include "pcstate.h"
#include "pcfont.h"
#include "pcfsel.h"
#include "pcsymbol.h"

/* hack to avoid compiler message */
#ifndef abs
extern  int     abs( int );
#endif

/* Vector for scoring how well a font matches selection criteria.  It
 * would be nice to do this with a single scalar, but pitch/height in
 * particular require too much info.  Elements of the vector correspond
 * to the selection criteria, in order - TRM Ch 8. */
typedef enum {
  score_first = 0,
  score_symbol_set=score_first,
  score_spacing,
  score_pitch,
  score_height,
  score_style,
  score_weight,
  score_typeface,
  score_location,
  score_orientation,
  score_fontnumber,
  score_limit
} score_index_t;
typedef	int match_score_t[score_limit];

#ifdef DEBUG
#include "plvalue.h"
static const char * const score_name[] = {
    "symbol",
    "spacing",
    "pitch",
    "height",
    "style",
    "weight",
    "typeface",
    "location",
    "orientation",
    "fontnumber"
};

static void
dprint_cc(const byte *pcc)
{	dprintf8("cc=%02x %02x %02x %02x %02x %02x %02x %02x", pcc[0],
		 pcc[1], pcc[2], pcc[3], pcc[4], pcc[5], pcc[6], pcc[7]);
}
void
dprint_font_params_t(const pl_font_params_t *pfp)
{	dprintf8("symset=%u %s pitch=%g ht=%u style=%u wt=%d face=%u:%u\n",
		 pfp->symbol_set,
		 (pfp->proportional_spacing ? "prop." : "fixed"),
		 pl_fp_pitch_cp(pfp) / 100.0, pfp->height_4ths / 4, pfp->style,
		 pfp->stroke_weight, pfp->typeface_family, (0x07FF &pfp->typeface_family) );
}

#include "plftable.h"

static void
dprint_font_name(const pl_font_t *pfont) 
{
    int i;
    bool found = false;

    for (i = 0; strlen(resident_table[i].full_font_name); i++) {
        if (!memcmp(&resident_table[i].params,
                    &pfont->params, sizeof(pfont->params))) {

            found = true;
            break;
        }
    }
    if (found) {
        dprintf1("%s ", resident_table[i].full_font_name);
    } else {
        if (pfont->storage == pcds_internal) {
            dprintf("internal font not found in resident table");
            dprintf1("%s\n", pfont->font_file);
        }
        dprintf("external font ");
    }
}

static void
dprint_font_t(const pl_font_t *pfont)
{	
    dprint_font_name(pfont);
    dprintf3("storage=%d scaling=%d type=%d ",
             pfont->storage, pfont->scaling_technology, pfont->font_type);
    dprint_cc(pfont->character_complement);
    dputs(";\n   ");
    dprint_font_params_t(&pfont->params);
}

static void
dprint_font_map(const pl_symbol_map_t *pmap)
{
    if (pmap != 0)
        dprintf3("selected symbol set id:%d type:%d format:%s\n", pl_get_uint16(pmap->id),
                 pmap->type, (pmap->format == 1 ? "MSL" : "Unicode"));
    else
        dprintf("selected symbol set NULL\n");

}

static void
dprintf_font_scoring(const char *type, const pl_font_t *pfont, pl_symbol_map_t *pmap, match_score_t score)
{
    int i;
    dprintf1("%s: ", type);
    dprint_font_t(pfont);
    dprint_font_map(pmap);
    dputs("   score:");
    for ( i = 0; i < score_limit; ++i )
        dprintf2(" %s: %d", score_name[i], score[i]);
    dputs("\n");
}

#endif

/* Decide whether an unbound font supports a symbol set (mostly
   convenient setup for pcl_check_symbol_support(). Has the
   side effect of setting the symbol table to the default if
   the requested map is not available or to the requested symbol set
   if it is available. 2 is returned for no matching symbol set, 1 for
   mismatched vocabulary and 0 if the sets match. */
static int
check_support(const pcl_state_t *pcs, uint symbol_set, const pl_font_t *fp,
              pl_symbol_map_t **mapp, bool id_selection)
{
    pl_glyph_vocabulary_t gv;
    byte id[2];

    id[0] = symbol_set >> 8;
    id[1] = symbol_set;

    /* a bound font is really neither unicode or MSL, setting the
       vocabulary to unicode is arbritrary to satisfy the call to
       "pcl_find_symbol_map" */
    gv = pl_font_is_bound(fp) ? plgv_Unicode :
        pl_complement_to_vocab(fp->character_complement);

    *mapp = pcl_find_symbol_map(pcs, id, gv, fp->font_type == plft_16bit);

    if (pl_font_is_bound(fp))
        return fp->params.symbol_set == symbol_set ? 2 : 0;
        

    if (*mapp == 0) {
        /* selecting by id and did not find the symbol map specified
           by the font (punt) or if this is a wide encoding no map is
           needed. */
        if (id_selection || pl_wide_encoding(symbol_set))
            return 2;
        else {
            /* 
             * Interestingly, if we look up a symbol set that does not
             * exist the default symbol set is replaced with roman-8.
             * This is certainly a bug in HP printers.
             */
            if (!pl_font_is_bound(fp)) {
                id[0] = 0x01;
                id[1] = 0x15;
                *mapp = pcl_find_symbol_map(pcs, id, gv, fp->font_type == plft_16bit);
                return 1;
            }
            return 0; /* worst */
        }
    }

    if ( pcl_check_symbol_support((*mapp)->character_requirements,
                                   fp->character_complement) )
	return 2; /* best */
    else
	return 1;
}

/* Identify the internal lineprinter font */
#define INTERNAL_LPFONT \
    (fp->params.typeface_family == 0 && fp->storage == pcds_internal)

/* a font may be scalable but we want to treat it a bitmap for the
   purpose of selection.  The internal lineprinter is the only example
   of this. */
static bool
font_is_scalable_selection_wise(const pl_font_t *fp)
{
    if (INTERNAL_LPFONT)
        return false;
    else
        return pl_font_is_scalable(fp);
}
    
    
/* Compute a font's score against selection parameters.  TRM 8-27.
 * Also set *mapp to the symbol map to be used if this font wins. */
static void
score_match(const pcl_state_t *pcs, const pcl_font_selection_t *pfs,
    const pl_font_t *fp, pl_symbol_map_t **mapp, match_score_t score)
{
	int tscore;

	/* 1.  Symbol set.  2 for exact match or full support, 1 for
	 * default supported, 0 for no luck. */
        score[score_symbol_set] = check_support(pcs, pfs->params.symbol_set, fp, mapp, false);
	/* 2.  Spacing. */
	score[score_spacing] =
	  pfs->params.proportional_spacing == fp->params.proportional_spacing;

	/* 3.  Pitch. */
	/* Need to score this so that (1) exact match is highest, (2) any
	 * higher-than-requested pitch is better than any lower-than-
	 * requested pitch, (3) within these categories, nearer is better. */
	if ( pfs->params.proportional_spacing )
	    score[score_pitch] = 0;	/* should not influence score */
	else { /* fixed space selection */
            if ( fp->params.proportional_spacing )
                /* scalable; match is worst possible */
                score[score_pitch] = 0;
	    else { 
                int delta = pl_fp_pitch_per_inch_x100(&fp->params) -
		    pl_fp_pitch_per_inch_x100(&pfs->params);

		/* If within one unit, call it exact; otherwise give
		 * preference to higher pitch than requested. */
		if (font_is_scalable_selection_wise(fp) || (abs(delta) <= 10 ))
                    score[score_pitch] = 0x2000000;
		else if ( delta > 0 )
                    score[score_pitch] = 0x2000000 - delta;
		else 
                    score[score_pitch] = 0x1000000 + delta;
            }
        }
	/* 4.  Height. */
	/* Closest match scores highest (no preference for + or -). Otherwise
	 * similar to pitch, and again, values assigned have no meaning out-
	 * side this code. */
	if ( font_is_scalable_selection_wise(fp) )
	  score[score_height] = 0x1000000;
	else
	  { int delta = abs(pfs->params.height_4ths - fp->params.height_4ths);

	    /* As before, allow one unit of error to appear "exact".  */
	    if ( delta <= 1 )
	      delta = 0;
	    score[score_height] = 0x1000000 - delta;
	  }

	/* 5.  Style. */
	if ( pfs->params.style == fp->params.style )
	    score[score_style] = 2;
	else if ( fp->params.style == 0 && pfs->params.style != 0 )
	    score[score_style] = 1;  /* prefer NO style fonts to wrong style */
	else
	    score[score_style] = 0;

	/* 6.  Stroke Weight. */
	/* If not exact match, prefer closest value away from 0 (more
	 * extreme).  If none, then prefer closest value nearer 0.  Once
	 * again, the actual values assigned here have no meaning outside
	 * this chunk of code. */
	{ /* Worst cases (font value toward zero from request) 0..13.
	   * Nearest more extreme: 14..21.  21 is exact.  */
	  int fwt = fp->params.stroke_weight;
	  int pwt = pfs->params.stroke_weight;
	  int delta = pwt - fwt;

	  /* With a little time, this could be collapsed. */
	  if ( pwt >= 0 )
	    if ( fwt >= pwt )
	      tscore = 21 + delta;
	    else
	      tscore = 14 - delta;
	  else
	    if ( fwt <= pwt )
	      tscore = 21 - delta;
	    else
	      tscore = 14 + delta;
	  score[score_weight] = tscore;
	}

	/* 7.  Typeface family. */
	{ 
            uint diff = pfs->params.typeface_family - fp->params.typeface_family;
            if (diff == 0)
                score[score_typeface] = 2;
            else {
                diff = (pfs->params.typeface_family & 0x7ff) - (fp->params.typeface_family & 0x7ff);
                if (diff == 0)
                    score[score_typeface] = 1;
                else
                    score[score_typeface] = 0;
            }
	}

	/* 8. Location. */
	/* Rearrange the value from "storage" into priority for us.  We
	 * only care that we get the priority sequence: soft-font, cartridge,
	 * SIMM, and internal, and that we order at least 2 cartridges.  (TRM
	 * implies that SIMMs are *not* to be ordered.)  Once again, values
	 * assigned here have no external meaning. */
	/* 1 bit at bottom for SIMM _vs_ internal, then cartridge-number
	 * bits, then soft-font bit at top. */
	if ( fp->storage & pcds_downloaded )
	  tscore = 1 << (pcds_cartridge_max + 1);
	else if ( fp->storage & pcds_all_cartridges )
	  tscore = (fp->storage & pcds_all_cartridges) >>
	      (pcds_cartridge_shift - 1);
	else
	  /* no priority among SIMMs, and internal is 0 */
	  tscore = (fp->storage & pcds_all_simms)? 1: 0;
	score[score_location] = tscore;

	/* 9.  Orientation */
	if ( fp->scaling_technology != plfst_bitmap )
	  score[score_orientation] = 1;
	else
	  { pcl_font_header_t *fhp = (pcl_font_header_t *)(fp->header);

	    score[score_orientation] = fhp?
	        fhp->Orientation == pcs->xfm_state.lp_orient :
		0;
	  }

	/* 10. Location: 11 in the implementors Guide. 
	 * lower typeface ID's are better.
	 * supposed to be ordered by bitmap/scalable and SoftFont ... Internal but
	 * these didn't seem to have an effect.
	 *  
	 * Lineprinter has a special case, making it harder to accidently select.
	 */

	if (INTERNAL_LPFONT)
	    score[score_fontnumber] = 0x100000 - fp->params.typeface_family; 
	else
	    score[score_fontnumber] = 0x200000 - fp->params.typeface_family; 

#ifdef DEBUG
	if ( gs_debug_c('=') )
            dprintf_font_scoring("candidate", fp, *mapp, score);
#endif

}

/* Recompute the current font from the descriptive parameters. */
/* This is used by both PCL and HP-GL/2. */
int
pcl_reselect_font(pcl_font_selection_t *pfs, const pcl_state_t *pcs, bool internal_only)
{	if ( pfs->font == 0 )
	  { pl_dict_enum_t dictp;
	    gs_const_string key;
	    void *value;
	    pl_font_t *best_font = 0;
	    pl_symbol_map_t *best_map = 0;
	    pl_symbol_map_t *mapp = 0;
	    match_score_t best_match;
            score_index_t i;

#ifdef DEBUG
	    if ( gs_debug_c('=') )
	      { dputs("[=]request: ");
	        dprint_font_params_t(&pfs->params);
	      }
#endif
	    /* if the font table is set up to select character by id
               we attempt to reselect the font by id.  As a fallback
               we use family selection.  NB We do not correctly handle
               the fonts with alphanumeric id's */
	    if ( (int)pfs->selected_id >= 0 ) {
		byte id_key[2];
		void *value;
		id_key[0] = pfs->selected_id >> 8;
		id_key[1] = (byte)(pfs->selected_id);
		if ( pl_dict_find((pl_dict_t *)&pcs->soft_fonts, id_key, 2, &value) ) {

		    pfs->font = (pl_font_t *)value;
                    (void)check_support(pcs, pfs->params.symbol_set, pfs->font, &pfs->map, true);
                    return 0;
		}
	    }
	    /* Initialize the best match to be worse than any real font. */
            for (i=(score_index_t)0; i<score_limit; i++)
                best_match[i] = -1;

	    pl_dict_enum_begin(&pcs->soft_fonts, &dictp);
	    while ( pl_dict_enum_next(&dictp, &key, &value) )
	      { pl_font_t *fp = (pl_font_t *)value;
		match_score_t match;
                if ((internal_only) && fp->storage != pcds_internal)
                    continue;

		score_match(pcs, pfs, fp, &mapp, match);
#ifdef DEBUG
                if ( gs_debug_c('=') ) {
                    if (best_match[0] != -1) /* skip sentinel */
                        dprintf_font_scoring("best", best_font, mapp, best_match);
                }
#endif
		for (i=(score_index_t)0; i<score_limit; i++)
		  if ( match[i] != best_match[i] )
		    {
		      if ( match[i] > best_match[i] )
			{
			  best_font = fp;
			  best_map = mapp;
			  memcpy((void*)best_match, (void*)match,
			      sizeof(match));
#ifdef DEBUG
                          if ( gs_debug_c('=') ) {
                              dprintf_font_scoring("usurper", fp, mapp, best_match);
                              dprintf1("   better %s***)\n", score_name[i]);

                          }
#endif

			}
		      break;
		    }
	      }
	    if ( best_font == 0 )
                return -1;	/* no fonts */
#ifdef DEBUG
            if ( gs_debug_c('=') ) {
                dprintf_font_scoring("champion", best_font, mapp, best_match);
            }
#endif

	    pfs->font = best_font;
	    pfs->map = best_map;
	  }
	pfs->selected_id = (uint)-1;
	return 0;
}

/* set font parameters after an id selection */
void
pcl_set_id_parameters(const pcl_state_t *pcs, 
		      pcl_font_selection_t *pfs, pl_font_t *fp, uint id)
{
	/* Transfer parameters from the selected font into the selection
	 * parameters, being careful with the softer parameters. */
	pfs->font = fp;
	pfs->selected_id = id;
	pfs->map = 0;
	if ( pl_font_is_bound(fp) )
	  pfs->params.symbol_set = fp->params.symbol_set;

	if ( check_support(pcs, pfs->params.symbol_set, fp, &pfs->map, true) )
	      DO_NOTHING;
        else if ( check_support(pcs, pcs->default_symbol_set_value,
                                fp, &pfs->map, true)
		    )
            DO_NOTHING;
        else { /*
		 * This font doesn't support the required symbol set.
		 * Punt -- map 1-for-1.
		 */
        }
	pfs->params.proportional_spacing = fp->params.proportional_spacing;
	if ( !pfs->params.proportional_spacing && !pl_font_is_scalable(fp) )
	  pfs->params.pitch = fp->params.pitch;
	if ( !pl_font_is_scalable(fp) )
	  pfs->params.height_4ths = fp->params.height_4ths;
	pfs->params.style = fp->params.style;
	pfs->params.stroke_weight = fp->params.stroke_weight;
	pfs->params.typeface_family = fp->params.typeface_family;
	return;
}
  
/* Select a font by ID, updating the selection parameters. */
/* Return 0 normally, 1 if no font was found, or an error code. */
int
pcl_select_font_by_id(pcl_font_selection_t *pfs, uint id, pcl_state_t *pcs)
{	byte id_key[2];
	void *value;
	pl_font_t *fp;

	id_key[0] = id >> 8;
	id_key[1] = (byte)id;
	if ( !pl_dict_find(&pcs->soft_fonts, id_key, 2, &value) )
	  return 1;		/* font not found */
	fp = (pl_font_t *)value;
	pcl_set_id_parameters(pcs, pfs, fp, id);
	return 0;
}
