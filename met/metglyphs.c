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

/*$Id$*/

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
#include "mt_error.h"
#include <stdlib.h> /* nb for atof */
#include "plfont.h"
#include "gstext.h"
#include "zipparse.h"
#include <wchar.h>  /* unicode */
#include <locale.h>

/* utilities for fonts */

/* utf8 string to wchar_t array conversion
 *
 * max input num wptr array elements
 * wcnt output num wchar's written
 * wptr wchar_t* array for output
 * inptr input string containing utf-8 
 * returns 0 on OK -1 on error
 *
 * NB: Portability uses mbrtowc() 
 *
 */ 
int met_utf8_to_wchars(const int max_out, int *wcnt, const wchar_t *wptr_in, const char *inptr_in)
{
    mbstate_t ps;
    wchar_t *wptr = wptr_in;
    char *inptr = inptr_in;

    /* NB: only tested for UTF-8, specs says utf-16 but all input is utf8 to date 
     */
    setlocale(LC_ALL,"");  
    memset(&ps, 0, sizeof(ps));
    memset(wptr, 0, *wcnt*sizeof(wchar_t));

    // NB: strlen(inptr) ? this assumes a null will be hit in time and that output array is big enough
    *wcnt = 0;
    while (*inptr) {
	int cnt = mbrtowc(wptr, inptr, 5, &ps);
	if (cnt < 0)
	    return mt_throw1(-1, "mbrtowc(%d) utf8 decode failed", cnt);
	inptr += cnt;
	++wptr;
	++(*wcnt);
						
	if(*wcnt > max_out)
	    return mt_throw2(-1, "max output array size exceeded %d > %d", *wcnt, max_out);
    }
    return 0;
}


/* load the TrueType data from a file to memory */
private byte *
load_tt_font(zip_part_t *part, gs_memory_t *mem) {
    byte *pfont_data;
    int len = zip_part_length(part);
    int size = -1;

    zip_part_seek(part, 0, 0);

    pfont_data = gs_alloc_bytes(mem, len+6, "xps_tt_load_font data");
    if ( pfont_data == 0 ) {
	return NULL;
    }

    // zip_part_length BUG, length is long, read is correct !!
    size = zip_part_read(pfont_data + 6, len, part);
    if ( size != len ) {
	dprintf2(0, "BUG! zip_part_length != zip_part_read %d, %d\n", len, size);
    }
    
    //if (part->parent->inline_mode)
	zip_part_free_all(part);
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
            mt_throw2(-1, "unsupported attribute %s=%s\n",
                     attr[i], attr[i+1]);
        }
    }
#undef MYSET

    /* copy back the data for the parser. */
    *ppdata = aGlyphs;
    return 0;
}

private int
set_rgb_text_color(gs_state *pgs, ST_RscRefColor Fill)
{
   if (!Fill) {
       return gs_setnullcolor(pgs);
   }
   else {
       rgb_t rgb = met_hex2rgb(Fill);
       return gs_setrgbcolor(pgs, (floatp)rgb.r, (floatp)rgb.g,
                              (floatp)rgb.b);
   }
   return -1; /* not reached */
}

private bool is_semi_colon(char c) {return c == ';';}
private bool is_colon(char c) {return c == ':';}

/* nb hack temporary static buffer for glyphs */
private gs_glyph glyphs[1024];
private float advance[1024];
private float uOffset[1024];
private float vOffset[1024];

private bool iscomma(char c) {return c == ',';}

/* no error return 
 * glyph 
 * advance integer in 100 em units 
 * uOffset not used
 * vOffset not used
 *
 */
private void
get_glyph_advance(char *index, gs_font *pfont, gs_glyph unicode, 
		  gs_glyph *glyph, float *advance,
		  float *uOffset, float *vOffset )
{
   char *args[strlen(index)];
   int num = 0;
   char *p;
   float metrics[4];

   *advance = *uOffset = *vOffset = 0.0f;

   if ((p = strchr(index, ','))) {
       num = met_split(index, args, iscomma);
       if (p != index) {  /* chr, width */
	   if (num-- > 0) 
	       *glyph = atoi(args[0]);
	   if (num-- > 0) 
	       *advance = atoi(args[1]) / 100.0f;
	   else {
	       gs_type42_get_metrics((gs_font_type42 *)pfont, *glyph, metrics);
	       // NB: type 1 fonts will need 
	       //(pfont->data.get_metrics)(pfont, *glyph, metrics);
	       *advance = metrics[2];
	   }
	   if (num-- > 0) 
	       *uOffset = atoi(args[2]);
	   if (num-- > 0) 
	       *vOffset = atoi(args[3]);
       }
       else {  /* ,num */ 
	   *glyph = (*pfont->procs.encode_char)(pfont, unicode, 0);
	   if (num-- > 0) 
	       *advance = atoi(args[0])/ 100.0f;
	   else {
	       gs_type42_get_metrics((gs_font_type42 *)pfont, *glyph, metrics);
	       *advance = metrics[2];
	   }
	   if (num-- > 0) 
	       *uOffset = atoi(args[1]);
	   if (num-- > 0) 
	       *vOffset = atoi(args[2]);
       }
   }
   else {
       *glyph = atoi(index);
       gs_type42_get_metrics((gs_font_type42 *)pfont, *glyph, metrics);
       *advance = metrics[2];
   }
}

private int 
build_text_params(gs_text_params_t *text, pl_font_t *pcf,
		  ST_UnicodeString UnicodeString, ST_Indices Indices)
{
    int code = 0;

   if (!Indices) {
       /* this would be the hallaluah case */
       text->operation = TEXT_FROM_STRING | TEXT_DO_DRAW | TEXT_RETURN_WIDTH;
       text->data.bytes = UnicodeString;
       text->size = strlen(UnicodeString);
   } else {
       /* the non halaluah case */
       gs_font *pfont = pcf->pfont;
       char expindstr[strlen(Indices) * 2 + 1];
       char *args[strlen(Indices) * 2 + 1];
       char **pargs = args;
       int cnt = 0;
       int glyph_index = 0;
       int combine_cnt = 0;
       int wcnt = strlen(UnicodeString);
       wchar_t wstr[strlen(UnicodeString)];
       bool has_widths = (bool)strchr(Indices, ',');/* pre scan for advance width usage */


       if ((code = met_utf8_to_wchars(wcnt, &wcnt, &wstr[0], &UnicodeString[0])) != 0)
	   mt_rethrow(code, "utf8 to wchar conversion failed");

       met_expand(expindstr, Indices, ';', 'Z');
       cnt = met_split(expindstr, args, is_semi_colon);
       while (*pargs) {
           /* if the argument is empty try the unicode string */
           if (**pargs == 'Z') {
               if (wcnt > glyph_index) {
		   gs_point point;

		   glyphs[glyph_index] = 
		       (*pfont->procs.encode_char)(pfont, wstr[glyph_index+combine_cnt], 0);

		   if (has_widths) {
		       (*pcf->char_width)(pcf, NULL, wstr[glyph_index+combine_cnt], &point);
		       advance[glyph_index] = point.x;
		   }
		   ++glyph_index;
	       }
               else
                   /* punt */
                   glyphs[glyph_index++] = 0;
           } else {       
	       char *p = 0;

	       if ((*pargs) && (p = strchr(*pargs, '('))) {
		   /* parse (a:b)  combine_cnt += a-b */
		   char colon_str[100];
		   char *paren[3];
		   int a, b;

		   *pargs = p+1;
		   met_expand(colon_str, *pargs, ';', 'Z');
		   if ( 2 != (cnt = met_split(colon_str, paren, is_colon)))
		       return mt_throw(-1, "xps parse unbalanced expression (a:b) missing ) "); 

		   a = atoi(paren[0]);
		   b = atoi(paren[1]);

		   combine_cnt += a - b;
		   p = strchr(*pargs, ')');
		   *pargs = p+1;
	       }

	       get_glyph_advance(*pargs, pfont, 
				 (gs_glyph)wstr[glyph_index+combine_cnt], 
				 &glyphs[glyph_index], &advance[glyph_index],
				 &uOffset[glyph_index], &vOffset[glyph_index]);
	       ++glyph_index;
           }
           pargs++;
       }
       while (wcnt > glyph_index+combine_cnt) {
	   glyphs[glyph_index] = 
	       (*pfont->procs.encode_char)(pfont, wstr[glyph_index+combine_cnt], 0);
	   if (has_widths) {  /* need advance widths, out of index */
	       gs_point point;
	       (*pcf->char_width)(pcf, NULL, wstr[glyph_index+combine_cnt], &point);
	       advance[glyph_index] = point.x;
	   } else 
	       advance[glyph_index] = 0.0;
	   uOffset[glyph_index] = 0.0;
	   vOffset[glyph_index] = 0.0;
	   ++glyph_index;
       }
       text->operation = TEXT_FROM_GLYPHS | TEXT_DO_DRAW | TEXT_RETURN_WIDTH;
       text->data.glyphs = glyphs;
       text->size = glyph_index;

       if (has_widths) {
	   text->x_widths = advance;
	   text->y_widths = uOffset; // NB: wrong better be zeroed for now.
	   text->widths_size = glyph_index;
	   text->operation |= TEXT_REPLACE_WIDTHS;
       }
   }
   return 0;
}

private bool is_Data_delimeter(char b) 
{
    return (b == ',') || (isspace(b));
}

/* action associated with this element.  NB this procedure should be
  decomposed into manageable pieces */
private int
Glyphs_action(void *data, met_state_t *ms)
{

   /* load the font */
   CT_Glyphs *aGlyphs = data;
   ST_Name fname = aGlyphs->FontUri; /* font uri */
   ST_RscRefColor color = aGlyphs->Fill;
   gs_memory_t *mem = ms->memory;
   gs_state *pgs = ms->pgs;
   pl_font_t *pcf = NULL; /* current font */
   gs_font_dir *pdir = ms->font_dir;
   FILE *in; /* tt font file */
   int code = 0;
   gs_matrix font_mat;
   zip_part_t *part;

   if (!fname) {
       return mt_throw(-1, "no font is defined");
   }

   /* nb cleanup on error.  if it is not in the font dictionary,
      get the data, build a plfont structure add it to the font
      dictionary */
   if (!find_font(ms, fname)) {
       byte *pdata = 0;
       if (0 && 
	   !strcmp(aGlyphs->FontUri, "/font_0.TTF")) {
	   /* HACK pulls in a fixed font file */
           uint len, size;
           if ((in = fopen(aGlyphs->FontUri, gp_fmode_rb)) == NULL)
	       return mt_throw1(-1, "file open failed %s", aGlyphs->FontUri);
           len = (fseek(in, 0L, SEEK_END), ftell(in));
           size = 6 + len;
           rewind(in);
           pdata = gs_alloc_bytes(mem, size, "foo");
           if (!pdata)
               return -1;
           fread(pdata + 6, 1, len, in);
           fclose(in);
       } else {
           part = find_zip_part_by_name(ms->pzip, aGlyphs->FontUri);
           if (part == NULL)
               return mt_throw1(-1, "Can't find zip part %s", aGlyphs->FontUri);

           /* load the font header */
           if ((pdata = load_tt_font(part, mem)) == NULL)
               return mt_throw(-1, "Can't load ttf");
       }
       /* create a plfont */
       if ((pcf = new_plfont(pdir, mem, pdata)) == NULL)
           return mt_throw(-1, "new_plfont failed");
       /* add the font to the font dictionary keyed on uri */
       if (!put_font(ms, fname, pcf))
           return mt_throw(-1, "font dictionary insert failed");
   }
   /* shouldn't fail since we added it if it wasn't in the dict */
   if ((pcf = get_font(ms, fname)) == NULL)
       return mt_throw(-1, "get_font");

   if ((code = gs_setfont(pgs, pcf->pfont)) != 0)
       return mt_rethrow(code, "gs_setfont");

   /* if a render transformation is specified we use it otherwise the
      font matrix starts out as identity, later it will be
      concatenated to the graphics state ctm after scaling for point
      size. */
   if (aGlyphs->RenderTransform) {
       char transstring[strlen(aGlyphs->RenderTransform)];
       strcpy(transstring, aGlyphs->RenderTransform);
       /* nb wasteful */
       char *args[strlen(aGlyphs->RenderTransform)];
       char **pargs = args;

       if ( 6 != met_split(transstring, args, is_Data_delimeter))
	   return mt_throw(-1, "Glyphs RenderTransform number of args");
       /* nb checking for sane arguments */
       font_mat.xx = atof(pargs[0]);
       font_mat.xy = atof(pargs[1]);
       font_mat.yx = atof(pargs[2]);
       font_mat.yy = atof(pargs[3]);
       font_mat.tx = atof(pargs[4]);
       font_mat.ty = atof(pargs[5]);
   } else {
       gs_make_identity(&font_mat);
   }

   /* move the character "cursor".  Note the render transform
      apparently applies to the coordinates of the origin argument as
      well as the font, so we transform the origin values by the font
      matrix! */
   {
       gs_point pt;
       if ((code = gs_point_transform(mem, aGlyphs->OriginX, aGlyphs->OriginY,
                                      &font_mat, &pt)) != 0)
           return mt_rethrow(code, "gs_point_transform");
       if ((code = gs_moveto(pgs, pt.x, pt.y)) != 0)
           return mt_rethrow(code, "gs_moveto");
   }

   /* save the current ctm and the current color */
   gs_gsave(pgs);

   /* set the text color */
   set_rgb_text_color(pgs, color);

   /* flip y metro goes down the page */
   if ((code = gs_matrix_scale(&font_mat, aGlyphs->FontRenderingEmSize,
                               (-aGlyphs->FontRenderingEmSize), &font_mat)) != 0)
       return mt_rethrow(code, "gs_matrix_scale");

   /* finally! */
   if ((code = gs_concat(pgs, &font_mat)) != 0)
       return mt_rethrow(code, "gs_concat");

   {
       gs_text_params_t text;
       gs_text_enum_t *penum;

       if ((code = build_text_params(&text, pcf, 
				     aGlyphs->UnicodeString,
				     aGlyphs->Indices)) != 0)
           return mt_rethrow(code, "build_text_params");
       if ((code = gs_text_begin(pgs, &text, mem, &penum)) != 0)
	   return mt_rethrow(code, "gs_text_begin");
       if ((code = gs_text_process(penum)) != 0)
	   return mt_rethrow(code, "gs_text_process");
       gs_text_release(penum, "Glyphs_action()");
   }

   /* restore the ctm */
   gs_grestore(pgs);
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
