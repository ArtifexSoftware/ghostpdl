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
#include "gx.h"
#include "gxiodev.h"
#include "gp.h"
#include "gsccode.h"
#include "gserrors.h"
#include "gsmatrix.h"
#include "gsutil.h"
#include "gxfont.h"
#include "gxfont42.h"
#include "strmio.h"
#include "plfont.h"
#include "pldict.h"
#include "pllfont.h"
#include "plftable.h"
#include "plvalue.h"

/* Load some built-in fonts.  This must be done at initialization time, but
 * after the state and memory are set up.  Return an indication of whether
 * at least one font was successfully loaded.  XXX The existing code is more
 * than a bit of a hack.  Approach: expect to find some fonts in one or more
 * of a given list of directories with names *.ttf.  Load whichever ones are
 * found in the table below.  Probably very little of this code can be
 * salvaged for later.
 */

/* utilities for poking around in tt files. It returns true if it a tt
   file, false if not.  Also returns false if there are any I/O
   failures */
static bool
is_ttfile(stream *ttfile)
{
    /* check if an open file a ttfile saving and restoring the file position */
    long pos;     /* saved file position */
    byte buffer[4]; /* version number buffer */
    bool is_tt;     /* true if a tt file */
    if ( (pos = sftell( ttfile )) < 0 )
	return false;
    /* seek to beginning */
    if ( sfseek( ttfile, 0L, SEEK_SET ) )
	return false;
    /* read 4 byte version number */
    is_tt = false;
    if ( ( sfread( &buffer, 1, 4, ttfile ) == 4 ) &&
	 ( pl_get_uint32( buffer ) == 0x10000 ) )
	    is_tt = true;
    /* restore the file position */
    if ( sfseek( ttfile, pos, SEEK_SET ) < 0 )
	return false;
    return is_tt;
}

/* get the windows truetype font file name - position 4 in the name
   table.  Assumes file is a reasonable tt_file - use is_ttfile() to
   check before calling this procedure. */
#define WINDOWSNAME 4
#define PSNAME 6

static 
int get_name_from_tt_file(stream *tt_file, gs_memory_t *mem, char *pfontfilename, int nameoffset)
{
    /* check if an open file a ttfile saving and restoring the file position */
    long pos;     /* saved file position */
    unsigned long len;
    char *ptr = pfontfilename;
    byte *ptt_font_data;

    if ( (pos = sftell( tt_file )) < 0 )
	return -1;
    /* seek to end and get the file length and allocate a buffer
       for the entire file */
    if ( sfseek( tt_file, 0L, SEEK_END ) )
	return -1;
    len = sftell( tt_file );

    /* allocate a buffer for the entire file */
    ptt_font_data = gs_alloc_bytes( mem, len, "get_name_from_tt_file" );
    if ( ptt_font_data == NULL )
	return_error(gs_error_VMerror );

    /* seek back to the beginning of the file and read the data
       into the buffer */
    if ( ( sfseek( tt_file, 0L, SEEK_SET ) == 0 ) &&
	 ( sfread( ptt_font_data, 1, len, tt_file ) == len ) )
	; /* ok */
    else {
	gs_free_object( mem, ptt_font_data, "get_name_from_tt_file" );
	return -1;
    }

    {
	/* find the "name" table */
	byte *pnum_tables_data = ptt_font_data + 4;
	byte *ptable_directory_data = ptt_font_data + 12;
	int table;
	for ( table = 0; table < pl_get_uint16( pnum_tables_data ); table++ )
	    if ( !memcmp( ptable_directory_data + (table * 16), "name", 4 ) ) {
		unsigned int offset = 
		    pl_get_uint32( ptable_directory_data + (table * 16) + 8 );
		byte *name_table = ptt_font_data + offset;
		/* the offset to the string pool */
		unsigned short storageOffset = pl_get_uint16( name_table + 4 );
		byte *name_recs = name_table + 6;
		{
		    /* 4th entry in the name table - the complete name */
		    unsigned short length = 
                        pl_get_uint16( name_recs + (12 * nameoffset) + 8 );
		    unsigned short offset = 
                        pl_get_uint16( name_recs + (12 * nameoffset) + 10 );
		    int k;
		    for ( k = 0; k < length; k++ ) {
			/* hack around unicode if necessary */
			int c = name_table[storageOffset + offset + k];
			if ( isprint( c ) )
			    *ptr++ = (char)c;
		    }
		}
		break;
	    }
    }
    /* free up the data and restore the file position */
    gs_free_object( mem, ptt_font_data, "get_name_from_tt_file" );
    if ( sfseek( tt_file, pos, SEEK_SET ) < 0 )
	return -1;
    /* null terminate the fontname string and return success.  Note
       the string can be 0 length if no fontname was found. */
    *ptr = '\0';

    /* trim trailing white space */
    {
	int i = strlen(pfontfilename);
	while (--i >= 0) {
	    if (!isspace(pfontfilename[i]))
		break;
	}
	pfontfilename[++i] = '\0';
    }

    return 0;
}

#ifdef DEBUG
static bool
lookup_pjl_number(pl_dict_t *pfontdict, int pjl_font_number)
{
    pl_dict_enum_t dictp;
    gs_const_string key;
    void *value;
    pl_dict_enum_begin(pfontdict, &dictp);
    while ( pl_dict_enum_next(&dictp, &key, &value) ) {
        pl_font_t *plfont = value;
        if (plfont->params.pjl_font_number == pjl_font_number)
            return true;
    }
    return false;
}

static void
check_resident_fonts(pl_dict_t *pfontdict, gs_memory_t *mem)
{
    int i;
    for (i = 0; 
         strlen(resident_table[i].full_font_name) != 0;
         i ++)
        if (!lookup_pjl_number(pfontdict, i)) {
            int j;
            dprintf2("%s (entry %d) not found\n", resident_table[i].full_font_name, i);
            dprintf("pxl unicode name:");
            for (j = 0; 
                 j < sizeof(resident_table[i].unicode_fontname); 
                 j++)
                dprintf1("'%c'", resident_table[i].unicode_fontname[j]);
            dprintf("\n");
        }
}
#endif

/* NOTES ABOUT NB NB - if the font dir necessary */
 int
pl_load_built_in_fonts(const char *pathname, gs_memory_t *mem,
                       pl_dict_t *pfontdict, gs_font_dir *pdir,
                       int storage, bool use_unicode_names_for_keys)
{	
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
	    stream *in = NULL;

            /* handle trailing separator */
            bool append_separator = false;
            int separator_length = strlen(gp_file_name_directory_separator());
            int offset = strlen(tmp_pathp) - separator_length;
            /* make sure the filename string ends in directory separator */
            if (strcmp(tmp_pathp + offset, gp_file_name_directory_separator()) != 0)
                append_separator = true;

            /* concatenate path and pattern */
            if ( (strlen( pattern ) + 
                  strlen( tmp_pathp) + 1 ) +
                 (append_separator ? separator_length : 0) > sizeof( tmp_path_copy ) ) {
                dprintf1("path name %s too long\n", tmp_pathp );
                continue;
            }

            strcpy( tmp_path_copy, tmp_pathp );

            if (append_separator == true)
                strcat(tmp_path_copy, gp_file_name_directory_separator());
                
	    /* NOTE the gp code code takes care of converting * to *.* */
            strcat( tmp_path_copy, pattern );

	    /* enumerate all files on the current path */
	    fe = gs_enumerate_files_init( tmp_path_copy,
					  strlen( tmp_path_copy ), mem );

	    /* loop through the files */
	    while ( ( code = gs_enumerate_files_next( fe,
						      tmp_path_copy,
						      sizeof( tmp_path_copy ) ) ) >= 0 ) {
		char buffer[1024];


		pl_font_t *plfont;

		/* loop failed/continued and left the file open */
		if ( in != NULL )
		    sfclose( in );

		if ( code > sizeof( tmp_path_copy ) ) {
		    dprintf("filename length exceeds file name storage buffer length\n");
		    continue;
		}
		/* null terminate the string */
		tmp_path_copy[code] = '\0';

		in = sfopen( tmp_path_copy, "rb", mem);
		if ( in == NULL ) { /* shouldn't happen */
		    dprintf1("cannot open file %s\n", tmp_path_copy );
		    continue;
		}

		if ( !is_ttfile( in ) ) {
                   #ifdef DEBUG
                    if ( gs_debug_c('=') ) {
                      dprintf1("%s not a TrueType file\n", tmp_path_copy);
                    }
                   #endif
		    continue;
                }
		code = get_name_from_tt_file( in, mem, buffer, PSNAME);
		if ( code < 0 ) {
		    dprintf1("input output failure on TrueType File %s\n", tmp_path_copy );
		    continue;
		}

		if ( strlen( buffer ) == 0 ) {
		    dprintf1("could not extract font file name from file %s\n", tmp_path_copy );
		    continue;
		}

		/* lookup the font file name in the resident table */
		for ( residentp = resident_table; strlen(residentp->full_font_name); ++residentp )
		    if ( strcmp( buffer, residentp->full_font_name ) == 0 )
			/* found it */
			break;

		/* hit sentinnel, nothing found */
		if ( !strlen(residentp->full_font_name) ) {
                   #ifdef DEBUG
                   if ( gs_debug_c('=') ) {
                    dprintf2("TrueType font %s in file %s not found in table\n", buffer, tmp_path_copy);
                    code = get_name_from_tt_file( in, mem, buffer, WINDOWSNAME);
                    dprintf1("Windows name %s\n", buffer);
                   }
                   #endif
		    continue;
                }

		/* load the font file into memory.  NOTE: this closes the file - argh... */
		if ( pl_load_tt_font(in, pdir, mem,
				     gs_next_ids(mem, 1), &plfont,
				     buffer) < 0 )  {
		    /* vm error but we continue anyway */
		    dprintf1("Failed loading font %s\n", tmp_path_copy);
		    continue;
		}

		/* save some bookkepping in the loop, if in is not
                   NULL at the beginning we know the file was left
                   open because of an error */
		in = NULL;

		plfont->storage = storage;
		plfont->data_are_permanent = false;
                plfont->font_type = residentp->font_type;
                plfont->params = residentp->params;
		memcpy(plfont->character_complement,
		       residentp->character_complement, 8);
		/* use the offset in the table as the pjl font number */
		/* for unicode keying of the dictionary use the unicode
		   font name, otherwise use the keys. */
		if ( use_unicode_names_for_keys )
		    pl_dict_put( pfontdict, (const byte *)residentp->unicode_fontname, 32, plfont );
		else {
		    key[2] = (byte)(residentp - resident_table);
		    key[0] = key[1] = 0;
		    pl_dict_put( pfontdict, key, sizeof(key), plfont );

		}
		/* leave data stored in the file */
		if ( pl_store_resident_font_data_in_file( tmp_path_copy, mem, plfont ) < 0 ) {
		    dprintf1("%s could not store data", tmp_path_copy );
		    continue;
		}
		/* mark the font as found */
                one_font_found = true;
	    } /* next file */
	} /* next directory */
    }
#ifdef DEBUG
    if ( gs_debug_c('=') )
        check_resident_fonts(pfontdict, mem);
#endif
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
