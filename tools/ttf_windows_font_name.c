#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#define get_uint16(bptr)\
  (((bptr)[0] << 8) | (bptr)[1])
#define get_int16(bptr)\
  (((int)get_uint16(bptr) ^ 0x8000) - 0x8000)

int
pl_get_int16(const unsigned char *bptr)
{	return get_int16(bptr);
}

uint
pl_get_uint16(const unsigned char *bptr)
{	return get_uint16(bptr);
}

long
pl_get_int32(const unsigned char *bptr)
{	return ((long)get_int16(bptr) << 16) | get_uint16(bptr + 2);
}

ulong
pl_get_uint32(const unsigned char *bptr)
{	return ((ulong)get_uint16(bptr) << 16) | get_uint16(bptr + 2);
}

int 
get_windows_name_from_tt_file(FILE *tt_file, char *pfontfilename)
{
    unsigned long len;
    char *ptr = pfontfilename;
    unsigned char *ptt_font_data;

    /* seek to end and get the file length and allocate a buffer
       for the entire file */
    if ( fseek( tt_file, 0L, SEEK_END ) )
	return -1;
    len = ftell( tt_file );

    /* allocate a buffer for the entire file */
    ptt_font_data = malloc( len );
    if ( ptt_font_data == NULL )
	return -1;

    /* seek back to the beginning of the file and read the data
       into the buffer */
    if ( ( fseek( tt_file, 0L, SEEK_SET ) == 0 ) &&
	 ( fread( ptt_font_data, 1, len, tt_file ) == len ) )
	; /* ok */
    else {
        printf ("couldn''t read file\n");
	return -1;
    }

    {
	/* find the "name" table */
	unsigned char *pnum_tables_data = ptt_font_data + 4;
	unsigned char *ptable_directory_data = ptt_font_data + 12;
	int table;
	for ( table = 0; table < pl_get_uint16( pnum_tables_data ); table++ )
	    if ( !memcmp( ptable_directory_data + (table * 16), "name", 4 ) ) {
		unsigned int offset = 
		    pl_get_uint32( ptable_directory_data + (table * 16) + 8 );
		unsigned char *name_table = ptt_font_data + offset;
		/* the offset to the string pool */
		unsigned short storageOffset = pl_get_uint16( name_table + 4 );
		unsigned char *name_recs = name_table + 6;
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

int
main(int argc, char **argv)
{
    char fontfilename[1024];
    int ret;
    while (--argc) {
        *argv++;
        FILE *in = fopen( *argv, "rb" );
        if ( in == NULL ) {
            printf( "file %s not found\n", *argv );
            continue;
        }
        ret = get_windows_name_from_tt_file(in, fontfilename);
        if ( ret < 0 ) {
            printf ("failed to get windows name\n");
            return ret;
        }
        printf("%s\n", fontfilename);
    }
    return ret;
}
