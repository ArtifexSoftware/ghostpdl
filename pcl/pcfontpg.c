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
#include "plftable.h"
#include "pllfont.h"

/* utility procedure to print n blank lines */
static inline void
print_blank_lines(pcl_state_t *pcs, int count)
{
    while( count-- ) {
	pcl_do_CR(pcs);
	pcl_do_LF(pcs);
    }
}

static int
process_font(pcl_state_t *pcs, pl_font_t *fp)
{
    pcl_font_selection_t *  pfs = &pcs->font_selection[pcs->font_selected];
    int code;

    if (!fp)
        return gs_throw(-1, "font is null");
    pcl_decache_font(pcs, -1);
    pfs->params = fp->params;
    pl_fp_set_pitch_per_inch(&pfs->params, 10);
    pfs->params.height_4ths = 12 * 4;
    {
        /* TODO we should print out the font name here to be more like
           the HP font page */
        const char *alphabet = "ABCDEFGHIJKLMNOPQRSTUVWYZ";
        /* current parameters */
        pl_font_params_t *pfp = &pfs->params;
        char buff[150];
        code = pcl_text((byte *)alphabet, strlen(alphabet), pcs, false);
        if (code < 0)
            return gs_rethrow(code, "failed to display font");

        /* go to approx center of the page */
        pcl_set_cap_x(pcs, pcs->margins.right / 2, false, false);

        pcl_decache_font(pcs, -1);
        /* reset to default font and print the select font string and pjl number */
        code = pcl_set_current_font_environment(pcs);
        if (code < 0)
            return gs_rethrow(code, "failed to set default font");
        /* create the string command for font selection */
        sprintf(buff, "<esc>(%u<esc>(s%dp%uv%us%db%dT\n",
                pfp->symbol_set, (pfp->proportional_spacing ? 1 : 0),
                pfp->height_4ths / 10, pfp->style, pfp->stroke_weight,
                pfp->typeface_family);

        code = pcl_text((byte *)buff, strlen(buff), pcs, false);
        if (code < 0)
            return gs_rethrow(code, "failed to display font");

        pcl_set_cap_x(pcs, pcs->margins.right / (16.0/15.0), false, false);
        sprintf(buff, "%d", fp->params.pjl_font_number);
        code = pcl_text((byte *)buff, strlen(buff), pcs, false);

        if (code < 0)
            return gs_rethrow(code, "failed to display font number");
    }
    print_blank_lines(pcs, 2);
    return 0;
}

/* print a pcl font page.  We do a pcl printer reset before and after
   the font list is printed */
static int
pcl_print_font_page(pcl_args_t *pargs, pcl_state_t *pcs)
{
    pl_dict_enum_t font_enum;
    gs_const_string key;
    void *value;
    int pjl_num;
    int code;

    /* reset the printer for a clean page */
    code = pcl_do_printer_reset(pcs);
    if (code < 0)
        return gs_rethrow(code, "printer reset failied");

    /* font page header */
    {
	const char *header_str = "PCL Font List";
	const char *sample_str = "Sample";
	const char *select_str = "Font Selection Command";
	uint hlen = strlen(header_str);
	/* assume the pcl font list string is 1 inch 7200 units */
	uint pos = pcs->margins.right / 2 - 7200 / 2;
	pcl_set_cap_x(pcs, pos, false, false);
	pcl_text((byte *)header_str, hlen, pcs, false);
	print_blank_lines(pcs, 2);
	pcl_text((byte *)sample_str, strlen(sample_str), pcs, false);
	pcl_set_cap_x(pcs, pcs->margins.right / 2, false, false);
	pcl_text((byte *)select_str, strlen(select_str), pcs, false);
	print_blank_lines(pcs, 2);
    }

    /* enumerate all non-resident fonts.*/
    pl_dict_enum_begin(&pcs->soft_fonts, &font_enum);
    while ( pl_dict_enum_next(&font_enum, &key, &value) ) {
        pl_font_t *fp = value;
        /* skip internal fonts */
        if (fp->storage == pcds_internal)
            continue;
        code = process_font(pcs, fp);
        if (code < 0)
            return gs_rethrow(code, "printing downloaded font failed\n");
    }

    /* now print out the resident fonts in pjl font number order */
    for (pjl_num = 0; pjl_num < pl_built_in_resident_font_table_count; pjl_num++) {
        pl_font_t *fp = pl_lookup_font_by_pjl_number(&pcs->built_in_fonts, pjl_num);
        code = process_font(pcs, fp);
        if (code < 0)
            return gs_rethrow1(code, "printing font number %d failed\n", pjl_num);
    }
    code = pcl_do_printer_reset(pcs);
    if (code < 0)
        return gs_rethrow(code, "printer reset failied");
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
