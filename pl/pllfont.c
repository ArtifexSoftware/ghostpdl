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
private bool
is_ttfile(FILE *ttfile)
{
    /* check if an open file a ttfile saving and restoring the file position */
    fpos_t pos;     /* saved file position */
    byte buffer[4]; /* version number buffer */
    bool is_tt;     /* true if a tt file */
    if ( fgetpos( ttfile, &pos ) )
	return false;
    /* seek to beginning */
    if ( fseek( ttfile, 0L, SEEK_SET ) )
	return false;
    /* read 4 byte version number */
    is_tt = false;
    if ( ( fread( &buffer, 1, 4, ttfile ) == 4 ) &&
	 ( pl_get_uint32( buffer ) == 0x10000 ) )
	    is_tt = true;
    /* restore the file position */
    if ( fsetpos( ttfile, &pos ) )
	return false;
    return is_tt;
}

/* get the windows truetype font file name - position 4 in the name
   table.  Assumes file is a reasonable tt_file - use is_ttfile() to
   check before calling this procedure. */
private 
int get_windows_name_from_tt_file(FILE *tt_file, gs_memory_t *mem, char *pfontfilename)
{
    /* check if an open file a ttfile saving and restoring the file position */
    fpos_t pos;     /* saved file position */
    unsigned long len;
    char *ptr = pfontfilename;
    byte *ptt_font_data;

    if ( fgetpos( tt_file, &pos ) )
	return -1;
    /* seek to end and get the file length and allocate a buffer
       for the entire file */
    if ( fseek( tt_file, 0L, SEEK_END ) )
	return -1;
    len = ftell( tt_file );

    /* allocate a buffer for the entire file */
    ptt_font_data = gs_alloc_bytes( mem, len, "get_windows_name_from_tt_file" );
    if ( ptt_font_data == NULL )
	return_error( gs_error_VMerror );

    /* seek back to the beginning of the file and read the data
       into the buffer */
    if ( ( fseek( tt_file, 0L, SEEK_SET ) == 0 ) &&
	 ( fread( ptt_font_data, 1, len, tt_file ) == len ) )
	; /* ok */
    else {
	gs_free_object( mem, ptt_font_data, "get_windows_name_from_tt_file" );
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
		    unsigned short length = pl_get_uint16( name_recs + (12 * 4) + 8 );
		    unsigned short offset = pl_get_uint16( name_recs + (12 * 4) + 10 );
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
    gs_free_object( mem, ptt_font_data, "get_windows_name_from_tt_file" );
    if ( fgetpos( tt_file, &pos ) )
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
	 {0, 0, pitch_1, 0, style_word(0, 0, 0), 0, face_val(3, agfa)},
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
	 {0, 0, pitch_1, 0, style_word(0, 0, 0), 3, face_val(3, agfa)}, 
	 cc_alphabetic },
	{"NimbusMono Italic", {'C','o','u','r','i','e','r',' ',' ',' ',' ',' ',' ',' ','I','t'},
	 {0, 0, pitch_1, 0, style_word(1, 0, 0), 0, face_val(3, agfa)},
	 cc_alphabetic },
	{"NimbusMono Bold Italic", {'C','o','u','r','i','e','r',' ',' ',' ',' ',' ','B','d','I','t'},
	 {0, 0, pitch_1, 0, style_word(1, 0, 0), 3, face_val(3, agfa)},
	 cc_alphabetic},
	{"LetterGothic", {'L','e','t','t','e','r','G','o','t','h','i','c',' ',' ',' ',' '},
	 {0, 0, pitch_1, 0, style_word(0, 0, 0), 0, face_val(6, agfa)},
	 cc_alphabetic},
	{"LetterGothic Bold",
	 {'L','e','t','t','e','r','G','o','t','h','i','c',' ',' ','B','d'},
	 {0, 0, pitch_1, 0, style_word(0, 0, 0), 3, face_val(6, agfa)},
	 cc_alphabetic},
	{"LetterGothic Italic", 
	 {'L','e','t','t','e','r','G','o','t','h','i','c',' ',' ','I','t'},
	 {0, 0, pitch_1, 0, style_word(1, 0, 0), 0, face_val(6, agfa)},
	 cc_alphabetic},
	{"Lpsb1", {'L','i','n','e',' ','P','r','i','n','t','e','r',' ',' ','0','N'},
	 {0, 0, pitch_1, 0, style_word(0, 0, 0), 0,
	  face_val(218, agfa)}, cc_alphabetic},
	{"Lpsb2", {'L','i','n','e',' ','P','r','i','n','t','e','r',' ','1','0','U'},
	 {0, 0, pitch_1, 0, style_word(0, 0, 0), 0,
	  face_val(218, agfa)}, cc_alphabetic},
	{"Lpsb3", {'L','i','n','e',' ','P','r','i','n','t','e','r',' ','1','1','U'},
	 {0, 0, pitch_1, 0, style_word(0, 0, 0), 0,
	  face_val(218, agfa)}, cc_alphabetic},
	{"Lpsb4", {'L','i','n','e',' ','P','r','i','n','t','e','r',' ','1','2','U'},
	 {0, 0, pitch_1, 0, style_word(0, 0, 0), 0,
	  face_val(218, agfa)}, cc_alphabetic},
	{"Lpsb5", {'L','i','n','e',' ','P','r','i','n','t','e','r',' ',' ','1','U'},
	 {0, 0, pitch_1, 0, style_word(0, 0, 0), 0,
	  face_val(218, agfa)}, cc_alphabetic},
	{"Lpsb6", {'L','i','n','e',' ','P','r','i','n','t','e','r',' ',' ','2','N'},
	 {0, 0, pitch_1, 0, style_word(0, 0, 0), 0,
	  face_val(218, agfa)}, cc_alphabetic},
	{"Lpsb7", {'L','i','n','e',' ','P','r','i','n','t','e','r',' ',' ','5','N'},
	 {0, 0, pitch_1, 0, style_word(0, 0, 0), 0,
	  face_val(218, agfa)}, cc_alphabetic},
	{"Lpsb8", {'L','i','n','e',' ','P','r','i','n','t','e','r',' ',' ','8','U'},
	 {0, 0, pitch_1, 0, style_word(0, 0, 0), 0,
	  face_val(218, agfa)}, cc_alphabetic},
	{"", {'0','0'},
	 {0, 0, pitch_1, 0, 0, 0, 0} }
#undef C
#undef cc_alphabetic
#undef cc_symbol
#undef cc_dingbats
#undef pitch_1
#undef agfa_value
#undef face_val
    };
    const font_resident_t *residentp;
    pl_font_t *font_found[countof(resident_table)];

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

    memset(font_found, 0, sizeof(font_found));
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
	    FILE *in = NULL;
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

		/* loop failed/continued and left the file open */
		if ( in != NULL )
		    fclose( in );

		if ( code > sizeof( tmp_path_copy ) ) {
		    dprintf("filename length exceeds file name storage buffer length\n");
		    continue;
		}
		/* null terminate the string */
		tmp_path_copy[code] = '\0';

		in = fopen( tmp_path_copy, "rb" );
		if ( in == NULL ) { /* shouldn't happen */
		    dprintf1( "cannot open file %s\n", tmp_path_copy );
		    continue;
		}

		if ( !is_ttfile( in ) )
		    continue;

		code = get_windows_name_from_tt_file( in, mem, buffer );
		if ( code < 0 ) {
		    dprintf1( "input output failure on TrueType File %s\n", tmp_path_copy );
		    continue;
		}

		if ( strlen( buffer ) == 0 ) {
		    dprintf1( "could not extract font file name from file %s\n", tmp_path_copy );
		    continue;
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

		/* load the font file into memory.  NOTE: this closes the file - argh... */
		if ( pl_load_tt_font(in, pdir, mem,
				     gs_next_id(), &plfont,
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
		if ( residentp->params.symbol_set != 0 )
		    plfont->font_type = plft_8bit;
		/*
		 * Don't smash the pitch, which was obtained
		 * from the actual font.
		 */
		{ 
		    pl_font_pitch_t save_pitch;
		    save_pitch = plfont->params.pitch;
		    plfont->params = residentp->params;
		    plfont->params.pitch = save_pitch;
		}
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
                one_font_found = true;
	    } /* next file */
	} /* next directory */
	/* we need to have *all* of the resident entries accounted for */
	for ( residentp = resident_table; strlen(residentp->full_font_name); ++residentp )
	    if ( !font_found[residentp - resident_table] ) {
		dprintf1( "Could not load resident font: %s\n", residentp->full_font_name );
	    }
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
