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

/* pcfontpg - print pcl resident typefaces */

#include "std.h"
#include "gstypes.h"
#include "pldict.h"
#include "pcfont.h"
#include "pcstate.h"
#include "pcursor.h"
#include "pcommand.h"


/* utility procedure to print n blank lines */
static inline void
print_blank_lines(pcl_state_t *pcs, int count)
{
    while( count-- ) {
	pcl_do_CR(pcs);
	pcl_do_LF(pcs);
    }
}
/* print a pcl font page.  We do a pcl printer reset before and after
   the font list is printed */
int
pcl_print_font_page(pcl_args_t *pargs, pcl_state_t *pcs)
{
    pl_dict_enum_t font_enum;
    gs_const_string key;
    void *value;

    /* reset the printer for a clean page */
    if ( pcl_do_printer_reset(pcs) < 0 )
	return -1;
    /* font page header */
    {
	const char *header_str = "PCL Font List";
	const char *sample_str = "Sample";
	const char *select_str = "Font Selection Command";
	uint hlen = strlen(header_str);
	/* assume the pcl font list string is 1 inch 7200 units */
	uint pos = pcs->margins.right / 2 - 7200 / 2;
	pcl_set_cap_x(pcs, pos, false, false);
	pcl_text(header_str, hlen, pcs, false);
	print_blank_lines(pcs, 2);
	pcl_text(sample_str, strlen(sample_str), pcs, false);
	pcl_set_cap_x(pcs, pcs->margins.right / 2, false, false);
	pcl_text(select_str, strlen(select_str), pcs, false);
	print_blank_lines(pcs, 2);
    }
    
    /* enumerate all of the fonts */
    pl_dict_enum_begin(&pcs->soft_fonts, &font_enum);
    while ( pl_dict_enum_next(&font_enum, &key, &value) ) {
	pl_font_t *fp = (pl_font_t *)value;
	pcl_font_selection_t *  pfs = &pcs->font_selection[pcs->font_selected];
	pcl_decache_font(pcs, -1);
	pfs->params = fp->params;
	pl_fp_set_pitch_per_inch(&pfs->params, 10);
	pfs->params.height_4ths = 12 * 4;
	{
	    const byte *alphabet = "ABCDEFGHIJKLMNOPQRSTUVWYZ";
	    /* current parameters */
	    pl_font_params_t *pfp = &pfs->params;
	    char buff[150];
	    if ( pcl_text(alphabet, strlen(alphabet), pcs, false) < 0 ) {
		dprintf("pcl_text failed\n" );
		return 1;
	    }
	    /* go to approx center of the page */
	    pcl_set_cap_x(pcs, pcs->margins.right / 2, false, false);
	    /* create the string command for font selection */
	    sprintf(buff, "<esc>(%u<esc>(s%dp%uv%us%db%dT\n",
		    pfp->symbol_set, (pfp->proportional_spacing ? 1 : 0),
		    pfp->height_4ths / 10, pfp->style, pfp->stroke_weight,
		    pfp->typeface_family);
	    pcl_decache_font(pcs, -1);
	    /* reset to default font and print the select font string */
	    if ( pcl_set_current_font_environment(pcs) < 0 ) {
		dprintf("pcl_set_current_font_environment failed\n");
		return -1;
	    }
	    pcl_text(buff, strlen(buff), pcs, false);
	}
	print_blank_lines(pcs, 2);
    }
    if ( pcl_do_printer_reset(pcs) < 0 )
	return -1;
    return 0;
}

static int
pcfontpg_do_registration(
    pcl_parser_state_t *pcl_parser_state,
    gs_memory_t *mem
)
{
    DEFINE_ESCAPE_ARGS('A', "Print Font Page", pcl_print_font_page, pca_in_rtl);
    return 0;
}

const pcl_init_t fontpg_init = {
    pcfontpg_do_registration, 0, 0
};
