/* Convert a TrueType font to a downloadable pcl soft font. */
#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <stdlib.h>

unsigned long stdout_offset = 0;

#define get_uint16(bptr)\
  (((bptr)[0] << 8) | (bptr)[1])
#define get_int16(bptr)\
  (((int)get_uint16(bptr) ^ 0x8000) - 0x8000)

int
pl_get_int16(const unsigned char *bptr)
{	return get_int16(bptr);
}

unsigned int
pl_get_uint16(const unsigned char *bptr)
{	return get_uint16(bptr);
}

long
pl_get_int32(const unsigned char *bptr)
{	return ((long)get_int16(bptr) << 16) | get_uint16(bptr + 2);
}

unsigned long
pl_get_uint32(const unsigned char *bptr)
{	return ((unsigned long)get_uint16(bptr) << 16) | get_uint16(bptr + 2);
}

typedef struct pcl_font_header_s {
    unsigned short   FontDescriptorSize;
    unsigned char    HeaderFormat;
    unsigned char    FontType;
    unsigned char    StyleMSB;
    unsigned char    Reserved;		    /* must be 0 */
    unsigned short   BaselinePosition;
    unsigned short   CellWidth;
    unsigned short   CellHeight;
    unsigned char    Orientation;
    unsigned char    Spacing;
    unsigned short   SymbolSet;
    unsigned short   Pitch;
    unsigned short   Height;
    unsigned short   xHeight;
    unsigned char    WidthType;
    unsigned char    StyleLSB;
    unsigned char    StrokeWeight;
    unsigned char    TypefaceLSB;
    unsigned char    TypefaceMSB;
    unsigned char    SerifStyle;
    unsigned char    Quality;
    unsigned char    Placement;
    unsigned char    UnderlinePosition;
    unsigned char    UnderlineThickness;
    unsigned short   TextHeight;
    unsigned short   TextWidth;
    unsigned short   FirstCode;
    unsigned short   LastCode;
    unsigned char    PitchExtended;
    unsigned char    HeightExtended;
    unsigned short   CapHeight;
    unsigned long    FontNumber;
    char             FontName[16];
    unsigned short   ScaleFactor;
    unsigned short   Master_Underline_Position;
    unsigned short   Master_Underline_Thickness;
    unsigned char    Font_Scaling_Technology;
    unsigned char    Variety;
} pcl_font_header_t;

typedef struct pcl_segment_s {
    unsigned short id;
    unsigned short size;
} pcl_segment_t;

#define SYMSET_NUM 1023
#define SYMSET_LETTER 'Z'
    
#define NUM_TABLES 8
/* alphabetical */
const char *tables[NUM_TABLES] = {
    "cvt ",
    "fpgm",
    "gdir",
    "head",
    "hhea",
    "hmtx",
    "maxp",
    "prep"
};


typedef struct table_directory_s {
    unsigned long sfnt_version; /* should be fixed 16.16 */
    unsigned short num_tables;
    unsigned short searchRange;
    unsigned short entrySelector;
    unsigned short rangeShift;
} table_directory_t;

typedef struct table_s {
    unsigned long tag;
    unsigned long checkSum;
    unsigned long offset;
    unsigned long length;
} table_t;
 
static void
write_pcl_header()
{
    pcl_font_header_t hdr;
    /* init header */
    memset(&hdr, 0, sizeof(hdr));
    /* truetype */
    hdr.HeaderFormat = 15;
    /* unicode */
    hdr.FontType = 11;
    hdr.Spacing = 1;
    hdr.TypefaceLSB = 100; /* don't know */
    //    hdr.SymbolSet = htons((SYMSET_NUM * 32) + (SYMSET_LETTER - 64));
    hdr.FontDescriptorSize = htons(sizeof(pcl_font_header_t));
    hdr.Font_Scaling_Technology = 1;
    hdr.Pitch = htons(500); /* arbitrary */
    hdr.CellHeight = htons(2048); /* don't know */
    hdr.CellWidth = htons(2048); /* don't know */
    hdr.FirstCode = htons(0);
    hdr.LastCode = htons(255);
    hdr.FontName[0] = 'f';
    stdout_offset += fwrite(&hdr, 1, sizeof(pcl_font_header_t), stdout);
}

static int
log2(int n)
{
    int lg = 0;

    while (n != 1) {
        n /= 2;
        lg += 1;
    }
    return lg;
}

static void
write_table_directory()
{
    table_directory_t td;
    if ( NUM_TABLES & -NUM_TABLES != NUM_TABLES ) {
        fprintf( stderr, "number of tables must be a power of 2");
        exit(EXIT_FAILURE);
    }
    fprintf(stderr, "Table Directory started at %d\n", stdout_offset);
    td.sfnt_version = htonl(0x00010000);
    td.num_tables = htons(NUM_TABLES);
    td.searchRange = htons(NUM_TABLES * 16);
    td.entrySelector = htons(log2(NUM_TABLES));
    td.rangeShift = htons(0); /* since power of 2 */
    
    stdout_offset += fwrite(&td, 1, sizeof(table_directory_t), stdout);
}

static int 
get_tt_buffer(char *filename, unsigned char **pptt_font_data, unsigned long *size)
{

    unsigned long len;
    FILE *fp;

    fp = fopen(filename, "r");
    if ( fp == NULL ) {
        fprintf(stderr, "file %s not found\n", filename);
        return -1;
    }
    fseek(fp, 0L, SEEK_END);
    len = ftell(fp);
    rewind(fp);
    *size = len;
    *pptt_font_data = (unsigned char *)malloc(*size);
    if ( *pptt_font_data == 0 ) {
        fprintf(stderr, "failed to allocate memory to buffer file %s\n", filename);
        fclose(fp);
        return -1;
    }
    fread(*pptt_font_data, 1, len, fp);
    return 0;
}

/* offset of table "table_name" */
static unsigned long
find_table(const unsigned char *ptt_data, const char *table_name, unsigned long *table_size) 
{
    unsigned int num_tables;
    const unsigned char *ptt_table_directory;
    int i;
    num_tables = pl_get_uint16(ptt_data + 4);
    ptt_table_directory = ptt_data + 12;
    //    fprintf(stderr, "number of tables %d ", num_tables);
    for ( i = 0; i < num_tables; i++ ) {
        const unsigned char *tab = ptt_table_directory + i * 16;
        //        fprintf(stderr, "found table %c%c%c%c table size=%d\n", tab[0], tab[1], tab[2], tab[3], pl_get_uint32(tab + 12));
        if ( !memcmp(table_name, tab, 4) ) {
            *table_size = pl_get_uint32(tab + 12);
            return pl_get_uint32(tab + 8);
        }
    }
    *table_size = 0;
    return 0;
}


static int
write_table_entrys(unsigned char *ptt_data)
{
    unsigned long current_offset = sizeof(table_directory_t) +
        sizeof(table_t) * NUM_TABLES;
    int i;
    for ( i = 0; i < NUM_TABLES; i++ ) {
        table_t t;
        unsigned long length;
        unsigned long offset;
        unsigned long *ptable = (unsigned long *)tables[i];
        t.tag = *ptable;
        t.offset = htonl(current_offset);
        offset = find_table(ptt_data, tables[i], &length);
        if (offset < 0) {
            fprintf(stderr, "table %s not found\n", tables[i]);
            exit(EXIT_FAILURE);
        }
        t.length = htonl(length);
        {
            unsigned long sum = 0;
            int k;
            for ( k = 0; k < length; k++ )
                sum += ptt_data[k + offset];
            t.checkSum = sum;
        }
        fprintf(stderr, "writing table entry %c%c%c%c at %d\n",
                tables[i][0], tables[i][1], tables[i][2], tables[i][3], stdout_offset);
        fprintf(stderr, "the table should end up at offset %d with length %d \n", current_offset, length);
        current_offset += length;
        stdout_offset += fwrite(&t, 1, sizeof(t), stdout);
    }
}

void
write_tables(unsigned char *ptt_data)
{
    int i;
    unsigned long offset;
    for ( i = 0; i < NUM_TABLES; i++ ) {
        unsigned long length;

        offset = find_table(ptt_data, tables[i], &length);
        if (offset < 0) {
            fprintf(stderr, "table %s not found\n", tables[i]);
            exit(EXIT_FAILURE);
        }
        fprintf(stderr, "writing table %c%c%c%c at %d\n", 
                tables[i][0], tables[i][1], tables[i][2], tables[i][3], stdout_offset);
        stdout_offset += fwrite(ptt_data+offset, 1, length, stdout);
    }
}

static unsigned long
total_segments_size(unsigned char *ptt_data)
{
    int i;
    unsigned long offset;
    unsigned long total = 0;
    for ( i = 0; i < NUM_TABLES; i++ ) {
        unsigned long this_table;
        offset = find_table(ptt_data, tables[i], &this_table);
        if ( offset < 0 ) {
            fprintf(stderr, "table %s not found\n", tables[i]);
            exit(EXIT_FAILURE);
        }
        total += this_table;
    }
    return total;
}

static void
write_GT_segment(unsigned char *ptt_data)
{

    pcl_segment_t GT_seg;
    GT_seg.id = htons(18260); /* GT */
    GT_seg.size = htons(sizeof(table_directory_t) +
                        sizeof(table_t) * NUM_TABLES +
                        total_segments_size(ptt_data));
    fprintf(stderr, "GT seg started at %d size = %d\n", stdout_offset, sizeof(table_directory_t) +
            sizeof(table_t) * NUM_TABLES + total_segments_size(ptt_data));
    stdout_offset += fwrite(&GT_seg, 1, sizeof(pcl_segment_t), stdout);
    write_table_directory();
    write_table_entrys(ptt_data);
    write_tables(ptt_data);
 
}

static void
write_NULL_segment()
{
    pcl_segment_t NULL_seg;
    NULL_seg.id = htons(0xffff);
    NULL_seg.size = htons(0);
    fprintf( stderr, "NULL segment started at %d\n", stdout_offset);
    stdout_offset += fwrite(&NULL_seg, 1, sizeof(pcl_segment_t), stdout);
}


static void
set_id(char *fontname)
{
    stdout_offset += fprintf(stdout, "\033*c1D");
    /* nb alphanumeric is broken in pcl */
    //    fprintf(stdout, "\033&n%dW%c%s", strlen(fontname) + 1, 0 /* operation */, fontname);
}

typedef struct pcl_symbol_set_s {
    unsigned short HeaderSize;
    unsigned short EncodedSymbolSetDesignator;
    unsigned char Format;
    unsigned char SymbolSetType;
    unsigned short FirstCode;
    unsigned short LastCode;
    unsigned char CharacterRequirements[8];
} pcl_symbol_set_t;

static void
write_mac_glyph_pcl_symbol_set(unsigned char *glyph_table)
{
    int i;
    pcl_symbol_set_t sym_hdr;
    short symbol_set = (SYMSET_NUM * 32) + (SYMSET_LETTER - 64);
    sym_hdr.HeaderSize = htons(18);
    sym_hdr.EncodedSymbolSetDesignator = htons(symbol_set);
    sym_hdr.Format = 3; /* unicode */
    sym_hdr.SymbolSetType = 2; /* 0 - 255 are printable */
    sym_hdr.FirstCode = htons(0);
    sym_hdr.LastCode = htons(255);
    memset(sym_hdr.CharacterRequirements, 0, sizeof(sym_hdr.CharacterRequirements));
    sym_hdr.CharacterRequirements[7] = 1;
    stdout_offset += fprintf(stdout, "\033*c%dR", symbol_set);
    stdout_offset += fprintf(stdout, "\033(f%dW", sizeof(sym_hdr) + 256 * 2);
    stdout_offset += fwrite(&sym_hdr, 1, sizeof(sym_hdr), stdout);
    for (i = 0; i < 256; i++ ) {
        unsigned short glyph = glyph_table[i];

        // NB identity.
        unsigned short pcl_glyph = glyph == 0 ? htons(0xffff) : htons(i);
        //        unsigned short pcl_glyph = glyph == 0 ? htons(0xffff) : htons(glyph);

        stdout_offset += fwrite(&pcl_glyph, 1, sizeof(pcl_glyph), stdout);
    }
}

static unsigned char *
get_mac_glyph_index(unsigned char *ptt_data)
{
    unsigned long length, cmap_offset;
    unsigned short table_version;
    unsigned short n, i;
    cmap_offset = find_table(ptt_data, "cmap", &length);
    if ( cmap_offset <= 0 ) {
        fprintf(stderr, "could not find cmap\n");
        exit(EXIT_FAILURE);
    }

    table_version = pl_get_uint16(ptt_data + cmap_offset);
    n = pl_get_uint16(ptt_data + cmap_offset + 2);
    for ( i = 0; i < n; i++ ) {
        unsigned char *enc_tab =
            ptt_data + cmap_offset + 4 + i * 8;
        unsigned short platform_id, platform_enconding;
        unsigned long offset_subtable;
        platform_id = pl_get_uint16(enc_tab);
        platform_enconding = pl_get_uint16(enc_tab + 2);
        offset_subtable = pl_get_uint32(enc_tab + 4);
        fprintf(stderr, "found cmap platform id=%d, platform encoding=%d\n",
                platform_id, platform_enconding);
        {
            unsigned char *subtable = ptt_data + cmap_offset + offset_subtable;
            unsigned short format, length, version;
            format = pl_get_uint16(subtable);
            length = pl_get_uint16(subtable + 2);
            version = pl_get_uint16(subtable + 4);
            fprintf(stderr, "found subtable format=%d, length=%d, version=%d\n", format, length, version );
            if ( format == 0 )
                return subtable + 6;
        }
    }
    return 0;
}

void
write_symbol_set(unsigned char *ptt_data)
{
    unsigned char *table;
    if ( (table = get_mac_glyph_index(ptt_data)) == 0 ) {
        fprintf(stderr, "mac table not found\n");
        exit(EXIT_FAILURE);
    }
    write_mac_glyph_pcl_symbol_set(table);
}

typedef struct character_descriptor_s {
    unsigned char format;
    unsigned char continuation;
    unsigned char descriptor_size;
    unsigned char class;
    unsigned char addititonal_data[2];
    unsigned short character_data_size;
    unsigned short glyph_id;
} character_descriptor_t;

static unsigned char *
find_glyph_data(const glyph_index, const unsigned char *ptt_data, unsigned long *glyph_length)
{
    unsigned long length;
    unsigned long offset;
    unsigned char indexToLocFormat;
    const unsigned char *loca_table, *glyf_table;
    offset = find_table(ptt_data, "head", &length);
    if ( offset <= 0 ) {
        fprintf(stderr, "could not find head table\n");
        exit(EXIT_FAILURE);
    }
    indexToLocFormat = pl_get_uint16(ptt_data + offset + 50);
    fprintf(stderr, "glyphs indexed with %s\n", indexToLocFormat == 0 ? "short format" : "long format");
    offset = find_table(ptt_data, "loca", &length);
    if ( offset <= 0 ) {
        fprintf(stderr, "could not find loca table\n");
        exit(EXIT_FAILURE);
    }
    loca_table = ptt_data + offset;
    offset = find_table(ptt_data, "glyf", &length);
    if ( offset <= 0 ) {
        fprintf(stderr, "could not find glyf table\n");
        exit(EXIT_FAILURE);
    }
    glyf_table = ptt_data + offset;
    if ( indexToLocFormat == 0 ) {
        unsigned char *pthis_glyph = loca_table + glyph_index * 2;
        unsigned char *pnext_glyph = pthis_glyph + 2;
        *glyph_length = pl_get_uint16(pnext_glyph) * 2 - pl_get_uint16(pthis_glyph) * 2;
        return glyf_table + pl_get_uint16(pthis_glyph) * 2;
    } else {
        unsigned char *pthis_glyph = loca_table + glyph_index * 2;
        unsigned char *pnext_glyph = pthis_glyph + 4;
        *glyph_length = pnext_glyph - pthis_glyph;
        return glyf_table + pl_get_uint16(pthis_glyph) * 2;
    }
}

void
write_CC_segment()
{
    pcl_segment_t CC_seg;
    unsigned char CharacterComplement[8];
    memset(CharacterComplement, 0, 8);
    CharacterComplement[7] = 0xfe;
    CC_seg.id = htons(17219);
    CC_seg.size = htons(8);
    stdout_offset += fwrite(&CC_seg, 1, sizeof(pcl_segment_t), stdout);
    stdout_offset += fwrite(&CharacterComplement, 1, 8, stdout);
}


static void
write_character_descriptor(unsigned char *ptt_data)
{
    int i;
    unsigned char *table;
    character_descriptor_t tt_char_des;

    if ( (table = get_mac_glyph_index(ptt_data)) == 0 ) {
        fprintf(stderr, "mac table not found\n");
        exit(EXIT_FAILURE);
    }
    
    for (i = 0; i < 256; i++ ) {
        unsigned short glyph = table[i];
        unsigned short pcl_glyph = (glyph == 0 ? htons(0xffff) : htons(glyph));
        if ( pcl_glyph != 0xffff ) {
            unsigned long glyph_length;
            unsigned char *glyph_data;
            glyph_data = find_glyph_data(glyph, ptt_data, &glyph_length);
            if (glyph_length == 0)
                continue;
            stdout_offset += fprintf(stdout, "\033*c%dE", i);
            stdout_offset += fprintf(stdout, "\033(s%dW", sizeof(tt_char_des) + glyph_length);
            tt_char_des.format = 15;
            tt_char_des.continuation = 0;
            tt_char_des.descriptor_size = 4;
            tt_char_des.class = 15;
            tt_char_des.addititonal_data[0] = 0; tt_char_des.addititonal_data[1] = 0;
            tt_char_des.character_data_size = tt_char_des.descriptor_size + glyph_length;
            tt_char_des.glyph_id = pcl_glyph;
            fprintf(stderr, "table index=%d glyph index=%d, glyph_length=%d ", i, glyph, glyph_length);
            {
                unsigned long sum = 0;
                int i;
                for ( i = 0; i < glyph_length; i++ )
                    sum += glyph_data[i];

                fprintf(stderr, "checksum=%d\n", sum);
            }
            stdout_offset += fwrite(&tt_char_des, 1, sizeof(character_descriptor_t), stdout);
            stdout_offset += fwrite(glyph_data, 1, glyph_length, stdout);
        }
    }
}

void
write_test(char *fontname)
{
    stdout_offset += fprintf(stdout, "\033(%d%c", SYMSET_NUM, SYMSET_LETTER);
    stdout_offset += fprintf(stdout, "\033(s1P\033(s14V\033(1X");
    // fprintf(stdout, "\033&n%dW%c%s", strlen(fontname) + 1, 2 /* operation */ , fontname);
    {
        int i;
        for ( i = 0; i < 256; i++ ) {
            if ( i  % 39 == 0 )
                stdout_offset += fprintf(stdout, "\r\n");
            stdout_offset += fprintf(stdout, "\033&p1X%c ", i);
        }
    }
}

int
main(int argc, char **argv)
{
    unsigned char *ptt_data;
    unsigned long tt_data_size;

    /* NB - no parameter checking */
    set_id(argv[1]);
    if ( get_tt_buffer(argv[1], &ptt_data, &tt_data_size) < 0 )
        return -1;
    {
        unsigned long total_bytes =
            sizeof(pcl_font_header_t) + 
            sizeof(table_directory_t) +
            sizeof(table_t) * NUM_TABLES +
            total_segments_size(ptt_data) +
            3 * sizeof(pcl_segment_t) + /* GT segment + NULL segment + CC segiment */
            8 + /* character complement */
            2 /* reserved + checksum */;
        stdout_offset += fprintf(stdout, "\033)s%dW", total_bytes);
    }
    write_pcl_header();
    write_CC_segment();
    write_GT_segment(ptt_data);
    write_NULL_segment();
    {
        unsigned char reserved_and_checksum[2];
        reserved_and_checksum[0] = 0;
        reserved_and_checksum[1] = 0;
        stdout_offset += fwrite(&reserved_and_checksum, 1, 2, stdout);
    }

    write_symbol_set(ptt_data);
    write_character_descriptor(ptt_data);
    write_test(argv[1]);
    return 0;
}
