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
#include "plftable.h"
/* prescribed freetype include file setup */
#include <ft2build.h>
#include FT_FREETYPE_H
#include <freetype/ftsnames.h>

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
        pl_ft_init_procs((gs_font_type42 *)pfont);
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
    FT_Face  face;  /* face handle */
    FT_Error error;
    const font_resident_t *residentp;
    /* get rid of this should be keyed by pjl font number */
    byte key[3];
    bool one_font_found = false;

    if ( pathname == NULL ) {
	/* no font pathname */
	return 0;
    }
    /* don't load fonts more than once */
    if (pl_dict_length(pfontdict, true) > 0 ) {
	return true;
    }

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
                {
                    int i = strlen(buffer);
                    while (--i >= 0) {
                        if (!isspace(buffer[i]))
                            break;
                    }
                    buffer[++i] = '\0';
                }
			
		/* lookup the font file name in the resident table */
		for ( residentp = resident_table; strlen(residentp->full_font_name); ++residentp )
		    if ( strcmp( buffer, residentp->full_font_name ) == 0 )
			/* found it */
			break;

		/* hit sentinnel, nothing found */
		if ( !strlen(residentp->full_font_name) )
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
                one_font_found = true;
	    } /* next file */
	} /* next directory */
    }
    if ( one_font_found )
        return true;
    else
        return false;
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
