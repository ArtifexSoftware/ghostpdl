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

/* pclfont.c */
/* PCL5 font preloading */
#include "ctype_.h"
#include "stdio_.h"
#include "string_.h"
/* The following are all for gxfont42.h, except for gp.h. */
#include "gx.h"
#include "gp.h"
#include "gsccode.h"
#include "gserrors.h"
#include "gsmatrix.h"
#include "gsutil.h"
#include "gxfont.h"
#include "gxfont42.h"
#include "plfont.h"
#include "pldict.h"
#include "pllfont.h"
#include "plvalue.h"
/* prescribed freetype include file setup */
#include <ftbuild.h>
#include FT_FREETYPE_H
#include <freetype/ftnames.h>

/* NB a global */
FT_Library library;

/* Fill in Freetype font boilerplate. */
void
pl_fill_in_ft_font(gs_font_base *pfont, FT_Face face)
{	gs_make_identity(&pfont->FontMatrix);
        pfont->FontType = ft_TrueType;
        pfont->BitmapWidths = true;
        pfont->ExactSize = fbit_use_outlines;
        pfont->InBetweenSize = fbit_use_outlines;
        pfont->TransformedChar = fbit_use_outlines;

	/* we get the bbox on the fly after grid fitting */
        pfont->FontBBox.p.x = pfont->FontBBox.p.y =
          pfont->FontBBox.q.x = pfont->FontBBox.q.y = 0;
	/* This is probably not a good way to use the face handle */
        uid_set_UniqueID(&pfont->UID, (unsigned long)face);
        pfont->encoding_index = 1;	/****** WRONG ******/
        pfont->nearest_encoding_index = 1;	/****** WRONG ******/
        pl_tt_init_procs((gs_font_type42 *)pfont);
}

/* Load a built-in Freetype font */
int
pl_load_ft_font(FT_Face face, gs_font_dir *pdir, gs_memory_t *mem,
       long unique_id, pl_font_t **pplfont)
{         
	  gs_font_base *pfont = gs_alloc_struct(mem, gs_font_base, 
	  	&st_gs_font_base, "pl_ft_load_font(gs_font_base)");
	  pl_font_t *plfont = pl_alloc_font(mem, "pl_ft_load_font(pl_font_t)");
          int code;

	  if ( pfont == 0 || plfont == 0 )
            code = gs_note_error(gs_error_VMerror);
          else
          {   /* Initialize general font boilerplate. */
              code = pl_fill_in_font((gs_font *)pfont, plfont, pdir, mem, "illegal name");
              if ( code >= 0 )
              {   /* Initialize Freetype font boilerplate. */
                  plfont->header = 0;
                  plfont->header_size = 0;
                  plfont->scaling_technology = plfst_MicroType;
                  plfont->font_type = plft_Unicode;
                  plfont->large_sizes = true;
                  pl_fill_in_ft_font(pfont, face);
                  code = gs_definefont(pdir, (gs_font *)pfont);
              }
          }
          if ( code < 0 )
          {   gs_free_object(mem, plfont, "pl_mt_load_font(pl_font_t)");
              gs_free_object(mem, pfont, "pl_mt_load_font(gs_font_base)");
              return code;
          }
          *pplfont = plfont;
          return 0;
}


/* NOTES ABOUT NB NB - if the font dir necessary */
 int
pl_load_built_in_fonts(const char *pathname, gs_memory_t *mem, pl_dict_t *pfontdict,
		       gs_font_dir *pdir, int storage, bool use_unicode_names_for_keys)
{	
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
#define pitch_60  fp_pitch_value_cp(60)
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
	{"NimbusMono", {'C','o','u','r','i','e','r',' ',' ',' ',' ',' ',' ',' ',' ',' '},
	 {0, 0, pitch_60, 0, style_word(0, 0, 0), 0, face_val(3, agfa)},
	 cc_alphabetic},
	{"NimbusRomanNo4 Light", {'C','G',' ','T','i','m','e','s',' ',' ',' ',' ',' ',' ',' ',' '},
	 {0, 1, pitch_1, 0, style_word(0, 0, 0), 0, face_val(5, agfa)},
	 cc_alphabetic},
	{"NimbusRomanNo4 Bold", {'C','G',' ','T','i','m','e','s',' ',' ',' ',' ',' ',' ','B','d'},
	 {0, 1, pitch_1, 0, style_word(0, 0, 0), 3, face_val(5, agfa)},
	 cc_alphabetic},
	{"NimbusRomanNo4 Light Italic", {'C','G',' ','T','i','m','e','s',' ',' ',' ',' ',' ',' ','I','t'},
	 {0, 1, pitch_1, 0, style_word(1, 0, 0), 0, face_val(5, agfa)},
	 cc_alphabetic},
	{"NimbusRomanNo4 Bold Italic", {'C','G',' ','T','i','m','e','s',' ',' ',' ',' ','B','d','I','t'},
	 {0, 1, pitch_1, 0, style_word(1, 0, 0), 3, face_val(5, agfa)},
	 cc_alphabetic},
	{"URWClassico", {'C','G',' ','O','m','e','g','a',' ',' ',' ',' ',' ',' ',' ',' '},
	 {0, 1, pitch_1, 0, style_word(0, 0, 0), 0, face_val(17, agfa)},
	 cc_alphabetic},
	{"URWClassico Bold", {'C','G',' ','O','m','e','g','a',' ',' ',' ',' ',' ',' ','B','d'},
	 {0, 1, pitch_1, 0, style_word(0, 0, 0), 3, face_val(17, agfa)},
	 cc_alphabetic},
	{"URWClassico Italic", {'C','G',' ','O','m','e','g','a',' ',' ',' ',' ',' ',' ','I','t'},
	 {0, 1, pitch_1, 0, style_word(1, 0, 0), 0, face_val(17, agfa)},
	 cc_alphabetic},
	{"URWClassico Bold Italic", {'C','G',' ','O','m','e','g','a',' ',' ',' ',' ','B','d','I','t'},
	 {0, 1, pitch_1, 0, style_word(1, 0, 0), 3, face_val(17, agfa)},
	 cc_alphabetic},
	{"Coronet", {'C','o','r','o','n','e','t',' ',' ',' ',' ',' ',' ',' ',' ',' '},
	 {0, 1, pitch_1, 0, style_word(1, 0, 0), 0, face_val(20, agfa)},
	 cc_alphabetic},
	{"ClarendonURWBolCon", {'C','l','a','r','e','n','d','o','n',' ',' ',' ','C','d','B','d'},
	 {0, 1, pitch_1, 0, style_word(0, 1, 0), 3, face_val(44, agfa)},
	 cc_alphabetic},
	{"U001", {'U','n','i','v','e','r','s',' ',' ',' ',' ',' ',' ',' ','M','d'},
	 {0, 1, pitch_1, 0, style_word(0, 0, 0), 0, face_val(52, agfa)},
	 cc_alphabetic},
	{"U001 Bold", {'U','n','i','v','e','r','s',' ',' ',' ',' ',' ',' ',' ','B','d'},
	 {0, 1, pitch_1, 0, style_word(0, 0, 0), 3, face_val(52, agfa)},
	 cc_alphabetic},
	{"U001 Italic", {'U','n','i','v','e','r','s',' ',' ',' ',' ',' ','M','d','I','t'},
	 {0, 1, pitch_1, 0, style_word(1, 0, 0), 0, face_val(52, agfa)},
	 cc_alphabetic},
	{"U001 Bold Italic", {'U','n','i','v','e','r','s',' ',' ',' ',' ',' ','B','d','I','t'},
	 {0, 1, pitch_1, 0, style_word(1, 0, 0), 3, face_val(52, agfa)},
	 cc_alphabetic},
	{"U001Con", {'U','n','i','v','e','r','s',' ',' ',' ',' ',' ','C','d','M','d'},
	 {0, 1, pitch_1, 0, style_word(0, 1, 0), 0, face_val(52, agfa)},
	 cc_alphabetic},
	{"U001Con Bold", {'U','n','i','v','e','r','s',' ',' ',' ',' ',' ','C','d','B','d'},
	 {0, 1, pitch_1, 0, style_word(0, 1, 0), 3, face_val(52, agfa)},
	 cc_alphabetic},
	{"U001Con Italic", {'U','n','i','v','e','r','s',' ',' ',' ','C','d','M','d','I','t'},
	 {0, 1, pitch_1, 0, style_word(1, 1, 0), 0, face_val(52, agfa)},
	 cc_alphabetic},
	{"U001Con Bold Italic", {'U','n','i','v','e','r','s',' ',' ',' ','C','d','B','d','I','t'},
	 {0, 1, pitch_1, 0, style_word(1, 1, 0), 3, face_val(52, agfa)},
	 cc_alphabetic},
	{"AntiqueOlive", {'A','n','t','i','q','O','l','i','v','e',' ',' ',' ',' ',' ',' '},
	 {0, 1, pitch_1, 0, style_word(0, 0, 0), 0, face_val(72, agfa)},
	 cc_alphabetic},
	{"AntiqueOlive Bold", {'A','n','t','i','q','O','l','i','v','e',' ',' ',' ',' ','B','d'},
	 {0, 1, pitch_1, 0, style_word(0, 0, 0), 3,
	  face_val(72, agfa)}, cc_alphabetic},
	{"AntiqueOlive Italic", {'A','n','t','i','q','O','l','i','v','e',' ',' ',' ',' ','I','t'},
	 {0, 1, pitch_1, 0, style_word(1, 0, 0), 0, face_val(72, agfa)},
	 cc_alphabetic},
	{"GaramondNo8", {'G','a','r','a','m','o','n','d',' ','A','n','t','i','q','u','a'},
	 {0, 1, pitch_1, 0, style_word(0, 0, 0), 0, face_val(101, agfa)}, cc_alphabetic},
	{"GaramondNo8 Medium", {'G','a','r','a','m','o','n','d',' ',' ',' ',' ',' ','H','l','b'},
	 {0, 1, pitch_1, 0, style_word(0, 0, 0), 3, face_val(101, agfa)}, cc_alphabetic},
	{"GaramondNo8 Italic", {'G','a','r','a','m','o','n','d',' ',' ',' ',' ','K','r','s','v'},
	 {0, 1, pitch_1, 0, style_word(1, 0, 0), 0,
	  face_val(101, agfa)}, cc_alphabetic},
	{"GaramondNo8 Medium Italic", {'G','a','r','a','m','o','n','d',' ','K','r','s','v','H','l','b'},
	 {0, 1, pitch_1, 0, style_word(1, 0, 0), 3,
	  face_val(101, agfa)}, cc_alphabetic},
	{"Mauritius", {'M','a','r','i','g','o','l','d',' ',' ',' ',' ',' ',' ',' ',' '},
	 {0, 1, pitch_1, 0, style_word(0, 0, 0), 0, face_val(201, agfa)},
	 cc_alphabetic},
	{"A028 Medium", {'A','l','b','e','r','t','u','s',' ',' ',' ',' ',' ',' ','M','d'},
	 {0, 1, pitch_1, 0, style_word(0, 0, 0), 1,
	  face_val(266, agfa)}, cc_alphabetic},
	{"A028 Extrabold", {'A','l','b','e','r','t','u','s',' ',' ',' ',' ',' ',' ','X','b'},
	 {0, 1, pitch_1, 0, style_word(0, 0, 0), 4,
	  face_val(266, agfa)}, cc_alphabetic},
	{"A030", {'A','r','i','a','l',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' '},
	 {0, 1, pitch_1, 0, style_word(0, 0, 0), 0,
	  face_val(218, agfa)}, cc_alphabetic},
	{"A030 Bold", {'A','r','i','a','l',' ',' ',' ',' ',' ',' ',' ',' ',' ','B','d'},
	 {0, 1, pitch_1, 0, style_word(0, 0, 0), 3,
	  face_val(218, agfa)}, cc_alphabetic},
	{"A030 Italic", {'A','r','i','a','l',' ',' ',' ',' ',' ',' ',' ',' ',' ','I','t'},
	 {0, 1, pitch_1, 0, style_word(1, 0, 0), 0, face_val(218, agfa)}, cc_alphabetic},
	{"A030 Bold Italic", {'A','r','i','a','l',' ',' ',' ',' ',' ',' ',' ','B','d','I','t'},
	 {0, 1, pitch_1, 0, style_word(1, 0, 0), 3, face_val(218, agfa)}, cc_alphabetic},
	{"NimbusRomanNo9", {'T','i','m','e','s','N','e','w','R','m','n',' ',' ',' ',' ',' '},
	 {0, 1, pitch_1, 0, style_word(0, 0, 0), 0, face_val(517, agfa)}, 
	 cc_alphabetic},
	{"NimbusRomanNo9 Medium",{'T','i','m','e','s','N','e','w','R','m','n',' ',' ',' ','B','d'},
	 {0, 1, pitch_1, 0, style_word(0, 0, 0), 3,
	  face_val(517, agfa)}, cc_alphabetic},
	{"NimbusRomanNo9 Italic", {'T','i','m','e','s','N','e','w','R','m','n',' ',' ',' ','I','t'},
	 {0, 1, pitch_1, 0, style_word(1, 0, 0), 0,
	  face_val(517, agfa)}, cc_alphabetic},
	{"NimbusRomanNo9 Medium Italic", {'T','i','m','e','s','N','e','w','R','m','n',' ','B','d','I','t'},
	 {0, 1, pitch_1, 0, style_word(1, 0, 0), 3,
	  face_val(517, agfa)}, cc_alphabetic},
	{"StandardSymL", {'S','y','m','b','o','l',' ',' ',' ',' ',' ',' ',' ',' ',' ',' '},
	 {621, 1, pitch_1, 0, style_word(0, 0, 0), 0,
	  face_val(302, agfa)}, cc_symbol},
	{"Dingbats", {'W','i','n','g','d','i','n','g','s',' ',' ',' ',' ',' ',' ',' '},
	 {18540, 1, pitch_1,0, style_word(0, 0, 0), 0,
	  face_val(2730, agfa)}, cc_dingbats},
	{"NimbusMono Bold", {'C','o','u','r','i','e','r',' ',' ',' ',' ',' ',' ',' ','B','d'},
	 {0, 0, pitch_60, 0, style_word(0, 0, 0), 3, face_val(3, agfa)}, 
	 cc_alphabetic },
	{"NimbusMono Italic", {'C','o','u','r','i','e','r',' ',' ',' ',' ',' ',' ',' ','I','t'},
	 {0, 0, pitch_60, 0, style_word(1, 0, 0), 0, face_val(3, agfa)},
	 cc_alphabetic },
	{"NimbusMono Bold Italic", {'C','o','u','r','i','e','r',' ',' ',' ',' ',' ','B','d','I','t'},
	 {0, 0, pitch_60, 0, style_word(1, 0, 0), 3, face_val(3, agfa)},
	 cc_alphabetic},
	{"LetterGothic", {'L','e','t','t','e','r','G','o','t','h','i','c',' ',' ',' ',' '},
	 {0, 0, pitch_60, 0, style_word(0, 0, 0), 0, face_val(6, agfa)},
	 cc_alphabetic},
	{"LetterGothic Bold",
	 {'L','e','t','t','e','r','G','o','t','h','i','c',' ',' ','B','d'},
	 {0, 0, pitch_60, 0, style_word(0, 0, 0), 3, face_val(6, agfa)},
	 cc_alphabetic},
	{"LetterGothic Italic", 
	 {'L','e','t','t','e','r','G','o','t','h','i','c',' ',' ','I','t'},
	 {0, 0, pitch_60, 0, style_word(1, 0, 0), 0, face_val(6, agfa)},
	 cc_alphabetic},
	{"Lpsb1", {'L','i','n','e',' ','P','r','i','n','t','e','r',' ',' ','0','N'},
	 {0, 0, pitch_60, 0, style_word(0, 0, 0), 0,
	  face_val(218, agfa)}, cc_alphabetic},
	{"Lpsb2", {'L','i','n','e',' ','P','r','i','n','t','e','r',' ','1','0','U'},
	 {0, 0, pitch_60, 0, style_word(0, 0, 0), 0,
	  face_val(218, agfa)}, cc_alphabetic},
	{"Lpsb3", {'L','i','n','e',' ','P','r','i','n','t','e','r',' ','1','1','U'},
	 {0, 0, pitch_60, 0, style_word(0, 0, 0), 0,
	  face_val(218, agfa)}, cc_alphabetic},
	{"Lpsb4", {'L','i','n','e',' ','P','r','i','n','t','e','r',' ','1','2','U'},
	 {0, 0, pitch_60, 0, style_word(0, 0, 0), 0,
	  face_val(218, agfa)}, cc_alphabetic},
	{"Lpsb5", {'L','i','n','e',' ','P','r','i','n','t','e','r',' ',' ','1','U'},
	 {0, 0, pitch_60, 0, style_word(0, 0, 0), 0,
	  face_val(218, agfa)}, cc_alphabetic},
	{"Lpsb6", {'L','i','n','e',' ','P','r','i','n','t','e','r',' ',' ','2','N'},
	 {0, 0, pitch_60, 0, style_word(0, 0, 0), 0,
	  face_val(218, agfa)}, cc_alphabetic},
	{"Lpsb7", {'L','i','n','e',' ','P','r','i','n','t','e','r',' ',' ','5','N'},
	 {0, 0, pitch_60, 0, style_word(0, 0, 0), 0,
	  face_val(218, agfa)}, cc_alphabetic},
	{"Lpsb8", {'L','i','n','e',' ','P','r','i','n','t','e','r',' ',' ','8','U'},
	 {0, 0, pitch_60, 0, style_word(0, 0, 0), 0,
	  face_val(218, agfa)}, cc_alphabetic},
	{"", {'0','0'},
	 {0, 0, pitch_60, 0, 0, 0, 0} }
#undef C
#undef cc_alphabetic
#undef cc_symbol
#undef cc_dingbats
#undef pitch_1
#undef agfa_value
#undef face_val
    };
    FT_Face  face;  /* face handle */
    FT_Error error;
    const font_resident_t *residentp;
    pl_font_t *font_found[countof(resident_table)];

    /* get rid of this should be keyed by pjl font number */
    byte key[3];

    if ( pathname == NULL ) {
	/* no font pathname */
	return 0;
    }
    /* don't load fonts more than once */
    if (pl_dict_length(pfontdict, true) > 0 ) {
	return true;
    }

    memset(font_found, 0, sizeof(font_found));

    error = FT_Init_FreeType( &library );
    if ( error ) {
        dprintf( "could not create library instance\n");
    }
    /* Enumerate through the files in the path */
    {
	/* max pathname of 1024 including pattern */
	char tmp_path_copy[1024];
	char *tmp_pathp;
	const char pattern[] = "*";
	/* make a copy of the path for strtok */
	strcpy( tmp_path_copy, pathname );
	for ( tmp_pathp = tmp_path_copy;
	      (tmp_pathp = strtok( tmp_pathp, ";" ) ) != NULL;  /* NB shouldn't use strtok */
	      tmp_pathp = NULL ) {
	    int code;
	    file_enum *fe;
	    /* concatenate path and pattern */
	    if ( (strlen( pattern ) + strlen( tmp_pathp) + 1 ) > sizeof( tmp_path_copy ) ) {
		dprintf1( "path name %s too long\n", tmp_pathp );
		continue;
	    }

	    strcpy( tmp_path_copy, tmp_pathp );
	    /* NOTE the gp code code takes care of converting * to *.*
               on stupid platforms */
	    strcat( tmp_path_copy, pattern );
	    /* enumerate all files on the current path */
	    fe = gp_enumerate_files_init( tmp_path_copy,
					  strlen( tmp_path_copy ), mem );

	    /* loop through the files */
	    while ( ( code = gp_enumerate_files_next( fe,
						      tmp_path_copy,
						      sizeof( tmp_path_copy ) ) ) >= 0 ) {
		char buffer[1024];
		pl_font_t *plfont;





		if ( code > sizeof( tmp_path_copy ) ) {
		    dprintf("filename length exceeds file name storage buffer length\n");
		    continue;
		}
		/* null terminate the string */
		tmp_path_copy[code] = '\0';

		error = FT_New_Face( library, tmp_path_copy, 0, &face );
		if ( error ) {
		    dprintf1( "cannot open file %s\n", tmp_path_copy );
		    continue;
		}

		{
                    FT_SfntName name;
                    FT_UInt len;
                    byte *bptr = buffer;
                    byte *ft_name_ptr;
		    error = FT_Get_Sfnt_Name(face, 4, &name);
		    if ( error ) {
			dprintf( "could not get name string - shouldn't happen" );
			continue;
		    }
                    len = name.string_len;
                    ft_name_ptr = name.string;
		    /* hack around unicode if necessary */
		    while( len-- )
			if ( isprint( *ft_name_ptr )  )
			    *bptr++ = *ft_name_ptr++;
			else
			    ft_name_ptr++;
		    /* null terminate */
		    *bptr = '\0';
		}
			
		/* lookup the font file name in the resident table */
		for ( residentp = resident_table; strlen(residentp->full_font_name); ++residentp )
		    if ( strcmp( buffer, residentp->full_font_name ) == 0 )
			/* found it */
			break;

		/* hit sentinnel, nothing found */
		if ( !strlen(residentp->full_font_name) )
		    continue;

		/* this font is already loaded, must be a duplicate */
		if ( font_found[residentp - resident_table] )
		    continue;

		if ( pl_load_ft_font(face, pdir, mem, residentp - resident_table, &plfont) < 0 )  {
		    /* vm error but we continue anyway */
		    dprintf1("Failed loading font %s\n", tmp_path_copy);
		    continue;
		}

		plfont->storage = storage;
		plfont->data_are_permanent = false;
		if ( residentp->params.symbol_set != 0 )
		    plfont->font_type = plft_8bit;
		plfont->params = residentp->params;
		memcpy(plfont->character_complement,
		       residentp->character_complement, 8);
		/* use the offset in the table as the pjl font number */
		plfont->params.pjl_font_number = (int)(residentp - resident_table);
		/* for unicode keying of the dictionary use the unicode
		   font name, otherwise use the keys. */
		if ( use_unicode_names_for_keys )
		    pl_dict_put( pfontdict, residentp->unicode_fontname, 32, plfont );
		else {
		    key[2] = (byte)(residentp - resident_table);
		    key[0] = key[1] = 0;
		    pl_dict_put( pfontdict, key, sizeof(key), plfont );

		}
		
		/* leave data stored in the file */
		if ( pl_store_resident_font_data_in_file( tmp_path_copy, mem, plfont ) < 0 ) {
		    dprintf1( "%s could not store data", tmp_path_copy );
		    continue;
		}
		/* mark the font as found */
		font_found[residentp - resident_table] = plfont;
	    } /* next file */
	} /* next directory */
	/* error message if font is missing. */
	for ( residentp = resident_table; strlen(residentp->full_font_name); ++residentp )
	    if ( !font_found[residentp - resident_table] ) {
		dprintf1( "Could not load resident font: %s\n", residentp->full_font_name );
	    }
    }
    return true;
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
