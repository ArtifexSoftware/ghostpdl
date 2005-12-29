/* Portions Copyright (C) 2001 artofcode LLC.
   Portions Copyright (C) 1996, 2001, 2005 Artifex Software Inc.
   Portions Copyright (C) 1988, 2000 Aladdin Enterprises.
   This software is based in part on the work of the Independent JPEG Group.
   All Rights Reserved.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */

/*$Id: */

/* metglyphs.c */

#include "memory_.h"
#include "gp.h"
#include "gsmemory.h"
#include "metcomplex.h"
#include "metelement.h"
#include "metsimple.h"
#include "metutil.h"
#include "gspath.h"
#include "gsutil.h"
#include "gsccode.h"
#include "gsgdata.h"
#include "gxfont.h"
#include "gxfont42.h"
#include <stdlib.h> /* nb for atof */
#include "plfont.h"
#include "gstext.h"

/* utilities for fonts */

/* load the TrueType data from a file to memory */
private byte *
load_tt_font(FILE *in, gs_memory_t *mem) {
    byte *pfont_data;
    unsigned long size; /* not used */

    int code = pl_alloc_tt_fontfile_buffer(in, mem, &pfont_data, &size);
    if (code < 0)
        return NULL;
    else
        return pfont_data;
}

/* find a font (keyed by uri) in the font dictionary. */
private bool
find_font(met_state_t *ms, ST_Name FontUri)
{
    void *data; /* ignored */
    return pl_dict_find(&ms->font_dict, FontUri, strlen(FontUri), &data);
}

/* add a font (keyed by uri) to the dictionary */
private bool
put_font(met_state_t *ms, ST_Name FontUri, pl_font_t *plfont)
{
    return (pl_dict_put(&ms->font_dict,
                        FontUri, strlen(FontUri), plfont) == 0);
}

/* get a font - the font should exist */
private pl_font_t *
get_font(met_state_t *ms, ST_Name FontUri)
{
    void *pdata;
    pl_font_t *pf;
    if (pl_dict_find(&ms->font_dict, FontUri, strlen(FontUri), &pdata)) {
        pf = (pl_font_t *)pdata;
        return pf;
    } else
        return NULL;
}


/* a constructor for a plfont type42 suitable for metro.  nb needs
   memory cleanup on error. */
private pl_font_t *
new_plfont(gs_font_dir *pdir, gs_memory_t *mem, byte *pfont_data)
{
    pl_font_t *pf;
    gs_font_type42 *p42;
    int code;

    if ((pf = pl_alloc_font(mem, "pf new_plfont(pl_font_t)")) == NULL)
        return pf;
    /* dispense with the structure descriptor */
    if ((p42 = gs_alloc_struct(mem, gs_font_type42, &st_gs_font_type42,
                               "p42 new_plfont(pl_font_t)")) == NULL)
        return (pl_font_t *)p42;
    code = pl_fill_in_font((gs_font *)p42, pf, pdir, mem, "" /* font_name */);
    if (code < 0) {
        return NULL;
    }
    /* nb this can be set up to leave the data in the file */
    pf->header = pfont_data;
    /* nb shouldn't be used */
    pf->header_size = 0;
    /* this needs to be fixed wrt all languages */
    pf->storage = 0;
    pf->scaling_technology = plfst_TrueType;
    pf->font_type = plft_Unicode;
    /* nb not sure */
    pf->large_sizes = true;
    pf->offsets.GT = 0;
    pf->is_xl_format = false;
    pf->data_are_permanent = false;
    /* not used */
    memset(pf->character_complement, 0, 8);
    pl_fill_in_tt_font(p42, pf->header, gs_next_ids(mem, 1));
    code = gs_definefont(pdir, (gs_font *)p42);
    if (code < 0)
        return NULL;
    return pf;
}
    


/* element constructor */
private int
Glyphs_cook(void **ppdata, met_state_t *ms, const char *el, const char **attr)
{
    CT_Glyphs *aGlyphs = 
        (CT_Glyphs *)gs_alloc_bytes(ms->memory,
                                     sizeof(CT_Glyphs),
                                       "Glyphs_cook");
    int i;

    memset(aGlyphs, 0, sizeof(CT_Glyphs));

#define MYSET(field, value)                                                   \
    met_cmp_and_set((field), attr[i], attr[i+1], (value))


    /* parse attributes, filling in the zeroed out C struct */
    for(i = 0; attr[i]; i += 2) {
        if (!MYSET(&aGlyphs->CaretStops, "CaretStops"))
            ;
        else if (!MYSET(&aGlyphs->Fill, "Fill"))
            ;
        else if (!MYSET(&aGlyphs->FontUri, "FontUri"))
            ;
        else if (!MYSET(&aGlyphs->Indices, "Indices"))
            ;
        else if (!MYSET(&aGlyphs->UnicodeString, "UnicodeString"))
            ;
        else if (!MYSET(&aGlyphs->StyleSimulations, "StyleSimulations"))
            ;
        else if (!MYSET(&aGlyphs->RenderTransform, "RenderTransform"))
            ;
        else if (!MYSET(&aGlyphs->Clip, "Clip"))
            ;
        else if (!MYSET(&aGlyphs->OpacityMask, "OpacityMask"))
            ;
        else if (!strcmp(attr[i], "OriginX")) {
            aGlyphs->OriginX = atof(attr[i+1]);
            aGlyphs->avail.OriginX = 1;
        }
        else if (!strcmp(attr[i], "OriginY")) {
            aGlyphs->OriginY = atof(attr[i+1]);
            aGlyphs->avail.OriginY = 1;
        }
        else if (!strcmp(attr[i], "FontRenderingEmSize")) {
            aGlyphs->FontRenderingEmSize = atof(attr[i+1]);
            aGlyphs->avail.FontRenderingEmSize = 1;
        } else {
            dprintf2(ms->memory, "unsupported attribute %s=%s\n",
                     attr[i], attr[i+1]);
        }
    }
#undef MYSET

    /* copy back the data for the parser. */
    *ppdata = aGlyphs;
    return 0;
}

/* action associated with this element.  NB this procedure should be
   decomposed into manageable pieces */
private int
Glyphs_action(void *data, met_state_t *ms)
{
   
    /* load the font */
    CT_Glyphs *aGlyphs = data;
    ST_Name fname = aGlyphs->FontUri; /* font uri */
    gs_memory_t *mem = ms->memory;
    gs_state *pgs = ms->pgs;
    pl_font_t *pcf = NULL; /* current font */
    gs_font_dir *pdir = ms->font_dir;
    FILE *in; /* tt font file */
    int code = 0;
    gs_matrix font_mat, save_ctm;

    if (!fname) {
        dprintf(mem, "no font is defined\n");
        return 0;
    }

    /* nb cleanup on error.  if it is not in the font dictionary,
       get the data, build a plfont structure add it to the font
       dictionary */
    if (!find_font(ms, fname)) {
        byte *pdata;
        /* nb obviously this will not do */
        if ((in = fopen(aGlyphs->FontUri, gp_fmode_rb)) == NULL)
            return -1;
        /* load the font header */
        if ((pdata = load_tt_font(in, mem)) == NULL)
            return -1;
        /* create a plfont */
        if ((pcf = new_plfont(pdir, mem, pdata)) == NULL)
            return -1;
        /* add the font to the font dictionary keyed on uri */
        if (!put_font(ms, fname, pcf))
            return -1;
    }

    /* shouldn't fail since we added it if it wasn't in the dict */
    if ((pcf = get_font(ms, fname)) == NULL)
        return -1;

    if ((code = gs_setfont(pgs, pcf->pfont)) != 0)
        return code;

    /* move the character "cursor" */
    if ((code = gs_moveto(pgs, aGlyphs->OriginX, aGlyphs->OriginY)) != 0)
         return code;
    
    /* save the current ctm and concatenate a font matrix */
    gs_currentmatrix(pgs, &save_ctm);

    gs_make_identity(&font_mat);

    /* flip y metro goes down the page */
    if ((code = gs_matrix_scale(&font_mat, aGlyphs->FontRenderingEmSize,
                                -aGlyphs->FontRenderingEmSize, &font_mat)) != 0)
        return code;

    if ((code = gs_concat(pgs, &font_mat)) != 0)
        return code;

    {
        gs_text_enum_t *penum;
        gs_text_params_t text;
        text.operation = TEXT_FROM_STRING | TEXT_DO_DRAW | TEXT_RETURN_WIDTH;
        text.data.bytes = aGlyphs->UnicodeString;
        text.size = strlen(aGlyphs->UnicodeString);
        if ((code = gs_text_begin(pgs, &text, mem, &penum)) != 0)
            return code;
        if ((code = gs_text_process(penum)) != 0)
            return code;
        gs_text_release(penum, "Glyphs_action()");
    }
               
    /* restore the ctm */
    gs_setmatrix(pgs, &save_ctm);
    return 0;
}

private int
Glyphs_done(void *data, met_state_t *ms)
{
        
    gs_free_object(ms->memory, data, "Glyphs_done");
    return 0; /* incomplete */
}


const met_element_t met_element_procs_Glyphs = {
    "Glyphs",
    Glyphs_cook,
    Glyphs_action,
    Glyphs_done
};
