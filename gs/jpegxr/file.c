
/*************************************************************************
*
* This software module was originally contributed by Microsoft
* Corporation in the course of development of the
* ITU-T T.832 | ISO/IEC 29199-2 ("JPEG XR") format standard for
* reference purposes and its performance may not have been optimized.
*
* This software module is an implementation of one or more
* tools as specified by the JPEG XR standard.
*
* ITU/ISO/IEC give You a royalty-free, worldwide, non-exclusive
* copyright license to copy, distribute, and make derivative works
* of this software module or modifications thereof for use in
* products claiming conformance to the JPEG XR standard as
* specified by ITU-T T.832 | ISO/IEC 29199-2.
*
* ITU/ISO/IEC give users the same free license to this software
* module or modifications thereof for research purposes and further
* ITU/ISO/IEC standardization.
*
* Those intending to use this software module in products are advised
* that its use may infringe existing patents. ITU/ISO/IEC have no
* liability for use of this software module or modifications thereof.
*
* Copyright is not released for products that do not conform to
* to the JPEG XR standard as specified by ITU-T T.832 |
* ISO/IEC 29199-2.
*
* Microsoft Corporation retains full right to modify and use the code
* for its own purpose, to assign or donate the code to a third party,
* and to inhibit third parties from using the code for products that
* do not conform to the JPEG XR standard as specified by ITU-T T.832 |
* ISO/IEC 29199-2.
*
* This copyright notice must be included in all copies or derivative
* works.
*
* Copyright (c) ITU-T/ISO/IEC 2008, 2009.
**********************************************************************/

#ifdef _MSC_VER
#pragma comment (user,"$Id: file.c,v 1.15 2008/03/17 23:34:54 steve Exp $")
#else
#ident "$Id: file.c,v 1.15 2008/03/17 23:34:54 steve Exp $"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include "jpegxr.h"
# include "jxr_priv.h"

#ifndef _MSC_VER
# include <stdint.h>
#else
/* MSVC (as of 2008) does not support C99 or the stdint.h header
file. So include a private little header file here that does the
minimal typedefs that we need. */
# include "stdint_minimal.h"
/* MSVC doesn't have strcasecmp. Use _stricmp instead */
# define strcasecmp _stricmp
#endif

typedef struct context{
    const char *name; /* name of TIFF or PNM file */
    int wid; /* image width in pixels */
    int hei; /* image height in pixels */
    int ncomp; /* num of color components, 1, 3, or 4 */
    int bpi; /* bits per component, 1..16 */
    int ycc_bd10_flag; /* flag to distinguish RGB101010 from YCC BD10 formats*/
    int ycc_format; /* ycc format, 0 (Not Applicable), 1 (YUV420), 2 (YUV422), 3 (YUV444)*/
    short sf; /* sample format, 1 (UINT), 2 (FixedPoint), 3 (float) or 4 (RGBE)*/
    int format; /* component format code 0..15 */
    unsigned swap : 1; /* byte swapping required ? */
    FILE *file; /* input or output file pointer */
    void *buf; /* source or destination data buffer */
    int my; /* last MB strip (of 16 lines) read, init to -1 */
    int nstrips; /* num of TIFF strips, 0 for PNM */
    int strip; /* index of the current TIFF strip, 0 for PNM */
    int nlines; /* num of lines per TIFF strip, height for PNM */
    int line; /* index of current line in current strip */
    short photometric; /* PhotometricInterpretation:
                            WhiteIsZero 0
                            BlackIsZero 1
                            RGB 2
                            RGB Palette 3
                            Transparency mask 4
                            CMYK 5
                            YCbCr 6
                            CIELab 8
                       */
    uint32_t offoff; /* offset in TIFF file of StripOffsets ifd entry*/
    int padBytes;
    int alpha;  /* with alpha channel */
    unsigned int isBgr;
    int top_pad;
    int top_pad_remaining;
    int left_pad;
}context;

static void error(char *format,...)
{
    va_list args;
    fprintf(stderr, "Error: ");
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fprintf(stderr, "\n");
    abort();
}

/* YCC/CMYKDIRECT format tests */

unsigned int isOutputYUV444(jxr_image_t image)
{
     switch (jxr_get_pixel_format(image))
     {
        case JXRC_FMT_24bppYCC444:
        case JXRC_FMT_30bppYCC444:
        case JXRC_FMT_48bppYCC444:
        case JXRC_FMT_48bppYCC444FixedPoint:
        case JXRC_FMT_32bppYCC444Alpha:
        case JXRC_FMT_40bppYCC444Alpha:
        case JXRC_FMT_64bppYCC444Alpha:
        case JXRC_FMT_64bppYCC444AlphaFixedPoint:
            return 1;
        default:
            return 0;
     }
}

unsigned int isOutputYUV422(jxr_image_t image)
{
     switch (jxr_get_pixel_format(image))
     {
        case JXRC_FMT_16bppYCC422:
        case JXRC_FMT_20bppYCC422:
        case JXRC_FMT_32bppYCC422:
        case JXRC_FMT_24bppYCC422Alpha:
        case JXRC_FMT_30bppYCC422Alpha:
        case JXRC_FMT_48bppYCC422Alpha:
            return 1;
        default:
            return 0;
     }
}

unsigned int isOutputYUV420(jxr_image_t image)
{
     switch (jxr_get_pixel_format(image))
     {
        case JXRC_FMT_12bppYCC420:
        case JXRC_FMT_20bppYCC420Alpha:
           return 1;
        default:
            return 0;
     }
}

unsigned int isOutputCMYKDirect(jxr_image_t image)
{
     switch (jxr_get_pixel_format(image))
     {
     case JXRC_FMT_32bppCMYKDIRECT:
     case JXRC_FMT_64bppCMYKDIRECT:
     case JXRC_FMT_40bppCMYKDIRECTAlpha:
     case JXRC_FMT_80bppCMYKDIRECTAlpha:
         return 1;
     default:    
            return 0;
     }
}

/* File Reading Primitives: */

static void read_data(context *con, void *buf, int size, int count)
{
    size_t rc = fread(buf, size, count, con->file);
    if (rc!=count)
        error("premature EOF in input file %s", con->name);
}

static inline void read_uint8(context *con, uint8_t *buf, int count)
{
    read_data(con, buf, 1, count);
}

static void read_uint16(context *con, uint16_t *buf, int count)
{
    read_data(con, buf, 2, count);
    if (con->swap) {
        uint8_t *p, *plim, t;
        for (p=(uint8_t*)buf, plim=p+count*2; p<plim; p+=2)
            t=p[0], p[0]=p[1], p[1]=t;
    }
}

static void read_uint32(context *con, uint32_t *buf, int count)
{
    read_data(con, buf, 4, count);
    if (con->swap) {
        uint8_t *p, *plim, t;
        for (p=(uint8_t*)buf, plim=p+count*4; p<plim; p+=4)
            t=p[0], p[0]=p[3], p[3]=t, t=p[1], p[1]=p[2], p[2]=t;
    }
}

static inline uint8_t get_uint8(context *con)
{
    uint8_t c;
    read_uint8(con, &c, 1);
    return c;
}

static inline uint16_t get_uint16(context *con)
{
    uint16_t c;
    read_uint16(con, &c, 1);
    return c;
}

static inline uint32_t get_uint32(context *con)
{
    uint32_t c;
    read_uint32(con, &c, 1);
    return c;
}

static void seek_file(context *con, long offset, int whence)
{
    int rc = fseek(con->file, offset, whence);
    if (rc!=0)
        error("cannot seek to desired offset in input file %s", con->name);
}

/* File Writing Primitives: */

static void write_data(context *con, const void *buf, int size, int count)
{
    size_t rc = fwrite(buf, size, count, con->file);
    if (rc!=count)
        error("unable to write to output file %s", con->name);
}

static inline void write_uint8(context *con, const uint8_t *buf, int count)
{
    write_data(con, buf, 1, count);
}

static void write_uint16(context *con, uint16_t *buf, int count)
{
    if (con->swap) { /* destructive ! */
        char *p, *plim, t;
        for (p=(char*)buf, plim=p+count*2; p<plim; p+=2)
            t=p[0], p[0]=p[1], p[1]=t;
    }
    write_data(con, buf, 2, count);
}

static void write_uint32(context *con, uint32_t *buf, int count)
{
    if (con->swap) { /* destructive ! */
        char *p, *plim, t;
        for (p=(char*)buf, plim=p+count*4; p<plim; p+=4)
            t=p[0], p[0]=p[3], p[3]=t, t=p[1], p[1]=p[2], p[2]=t;
    }
    write_data(con, buf, 4, count);
}

static inline void put_uint8(context *con, const uint8_t value)
{
    write_uint8(con, &value, 1);
}

static inline void put_uint16(context *con, const uint16_t value)
{
    uint16_t buf = value;
    write_uint16(con, &buf, 1);
}

static inline void put_uint32(context *con, const uint32_t value)
{
    uint32_t buf = value;
    write_uint32(con, &buf, 1);
}

/* PNM File Header Operations: */

static unsigned read_pnm_number(context *con)
{
    int c;
    unsigned n=0;

    while (1) {
        c = get_uint8(con);
        if (c=='#')
            while (get_uint8(con)!='\n');
        else if (isdigit(c)) break;
        else if (!isspace(c))
            error("unexpected character 0x%02x (%c) found in PNM file %s", c, c, con->name);
    }
    do {
        n = 10*n+c-'0';
        c = get_uint8(con);
    }
    while (isdigit(c));
    if (!isspace(c))
        error("unexpected character 0x%02x (%c) following max value in PNM file %s", c, c, con->name);
    return n;
}

static void open_pnm_input_file(context *con)
{
    int c, max, one=1;

    rewind(con->file);
    c=get_uint8(con);
    if (c!='P')
        error("unexpected character 0x%02x (%c) at start of PNM file %s", c, c, con->name);
    switch (c=get_uint8(con)) {
        case '1' :
        case '2' :
        case '3' :
        case '4' : error("unsupported PNM file type P%c in PNM file %s", c, con->name);
        case '5' : con->ncomp = 1; break;
        case '6' : con->ncomp = 3; break;
        default : error("unexpected character 0x%02x (%c) following 'P' in PNM file %s", c, c, con->name);
    }

    con->wid = read_pnm_number(con);
    if (con->wid<=0)
        error("invalid image width %d in PNM file %s",con->wid, con->name);
    con->hei = read_pnm_number(con);
    if (con->hei<=0)
        error("invalid image height %d in PNM file %s", con->hei, con->name);
    max=read_pnm_number(con);
    if (max>=0x10000)
        error("invalid maximum value 0x%02x (%d) in PNM file %s", max, max, con->name);
    if (max<0x100) con->swap = 0;
    else con->swap = *(char*)&one==1;
    for (con->bpi=1; max>(1<<con->bpi); con->bpi++);

    con->nlines = con->hei;
}

static void start_pnm_output_file(context *con)
{
    int max, one=1; 
    con->file = fopen(con->name, "wb");
    if (con->file==0)
        error("cannot create PNM output file %s",con->name);
    max = (1<<con->bpi)-1;
    if (max<256) con->swap = 0;
    else con->swap = *(char*)&one==1;
    fprintf(con->file, "P%c\n%d %d\n%d\n", con->ncomp==1?'5':'6', con->wid, con->hei, max);
}

/* TIFF File Header Operations: */

#define ImageWidth 256
#define ImageLength 257
#define BitsPerSample 258
#define Compression 259
#define Photometric 262
#define StripOffsets 273
#define SamplesPerPixel 277
#define RowsPerStrip 278
#define StripByteCounts 279
#define SampleFormat 339
#define InkSet 332

static void read_tif_ifd_entry(context *con, uint32_t ifdoff, uint16_t *tag, uint16_t *type, uint32_t *count)
{
    seek_file(con, ifdoff, SEEK_SET);
    read_uint16(con, tag, 1);
    read_uint16(con, type, 1);
    read_uint32(con, count, 1);
}

static void read_tif_data(context *con, uint16_t type, uint32_t count, int index, int nval, void *buf)
{
    int size=0;

    switch (type) {
        case 3 : size = 2; break;
        case 4 : size = 4; break;
        default : return;
    }

    if (index+nval>(int)count)
        error("data array is to small for read request in TIFF file %s", con->name);

    index *= size;
    size *= count;
    uint32_t offset = ftell(con->file);
    if (size>4)
        read_uint32(con, &offset, 1);
    seek_file(con, offset+index, SEEK_SET);
    switch (type) {
        case 3 : read_uint16(con, (uint16_t*)buf, nval); break;
        case 4 : read_uint32(con, (uint32_t*)buf, nval);
    }
}

static uint32_t read_tif_datum(context *con, uint16_t type, uint32_t count, int index)
{
    uint8_t buf[4];

    read_tif_data(con, type, count, index, 1, buf);
    switch (type) {
        case 3 : return (uint32_t)*(uint16_t*)buf;
        case 4 : return (uint32_t)*(uint32_t*)buf;
        default : return 0;
    }
}

static uint32_t get_tif_datum(context *con, uint32_t ifdoff, int index)
{
    uint16_t tag, type;
    uint32_t count;

    read_tif_ifd_entry(con, ifdoff, &tag, &type, &count);
    return read_tif_datum(con, type, count, index);
}

static void open_tif_input_file(context *con)
{
    int i, one=1;
    uint16_t magic, nentry, tag, type;
    uint32_t count, diroff;

    rewind(con->file);
    read_uint16(con, &magic, 1);
    switch (magic) {
        case 0x4949 : con->swap = *(char*)&one!=1; break;
        case 0x4d4d : con->swap = *(char*)&one==1; break;
        default : error("bad magic number 0x%04x found at start of TIFF file %s", magic, con->name);
    }
    read_uint16(con, &magic, 1);
    if (magic!=42)
        error("magic number 42 not found in TIFF file %s", con->name);
    read_uint32(con, &diroff, 1);

    seek_file(con, diroff, SEEK_SET);
    read_uint16(con, &nentry, 1);
    for (i=0; i<nentry; i++) {
        int ifdoff = diroff+2+12*i;
        read_tif_ifd_entry(con, ifdoff, &tag, &type, &count);
        uint32_t data = read_tif_datum(con, type, count, 0);
        switch (tag) {
            case ImageWidth: 
                con->wid = data; 
                break;
            case ImageLength: 
                con->hei = data; 
                break;
            case BitsPerSample: 
                con->bpi = data;
                break;
            case SamplesPerPixel:
                con->ncomp = data; 
                break;
            case Compression: 
                if (data!=1)
                    error("TIFF input file %s is compressed", con->name); 
                break;
            case StripOffsets: 
                con->nstrips = count;
                con->offoff = ifdoff; 
                break;
            case RowsPerStrip: 
                con->nlines = data;
                break;
            case SampleFormat:
                con->sf = data; 
                break; 
            case Photometric:
                con->photometric = data; 
                break; 

        }
    }

    if (con->wid<=0)
        error("valid ImageWidth entry not found in directory of TIFF file %s", con->name);
    if (con->hei<=0)
        error("valid ImageLength entry not found in directory of TIFF file %s", con->name);
    if (con->ncomp!=1 && con->ncomp!=3 && con->ncomp!=4 && con->ncomp!=5)
        error("valid SamplesPerPixel entry (1, 3, 4, or 5) not found in directory of TIFF file %s", con->name);
    if (con->bpi>32)
        error("valid BitsPerSample entry not found in directory of TIFF file %s", con->name);
    if (con->nstrips<=0 || con->offoff==0)
        error("valid StripOffsets entry not found in directory of TIFF file %s", con->name);
    if (con->nlines<=0)
        error("valid RowsPerStrip entry not found in directory of TIFF file %s", con->name);
    con->line = con->nlines;
}


static void put_ifd_entry(context *con, int tag, int type, int count, uint32_t offset)
{
    put_uint16(con, tag);
    put_uint16(con, type);
    put_uint32(con, count);
    if (type==3 && count==1) {
        put_uint16(con, offset);
        put_uint16(con, 0);
    }
    else put_uint32(con, offset);
}

unsigned int validate_tif_output(jxrc_t_pixelFormat ePixelFormat)
{
    switch(ePixelFormat)
    {
    case JXRC_FMT_24bppRGB:return 1;
    case JXRC_FMT_24bppBGR: return 0;
    case JXRC_FMT_32bppBGR:return 0;
    case JXRC_FMT_48bppRGB:return 1;
    case JXRC_FMT_48bppRGBFixedPoint: return 1;
    case JXRC_FMT_48bppRGBHalf: return 1;
    case JXRC_FMT_96bppRGBFixedPoint:return 1;
    case JXRC_FMT_64bppRGBFixedPoint:return 1;
    case JXRC_FMT_64bppRGBHalf:return 1;
    case JXRC_FMT_128bppRGBFixedPoint:return 1;
    case JXRC_FMT_128bppRGBFloat:return 1;
    case JXRC_FMT_32bppBGRA:return 0;
    case JXRC_FMT_64bppRGBA:return 1;
    case JXRC_FMT_64bppRGBAFixedPoint:return 1;
    case JXRC_FMT_64bppRGBAHalf:return 1;
    case JXRC_FMT_128bppRGBAFixedPoint:return 1;
    case JXRC_FMT_128bppRGBAFloat:return 1;
    case JXRC_FMT_32bppPBGRA:return 0;
    case JXRC_FMT_64bppPRGBA:return 0;
    case JXRC_FMT_128bppPRGBAFloat:return 0;
    case JXRC_FMT_32bppCMYK:return 1;
    case JXRC_FMT_40bppCMYKAlpha:return 1;
    case JXRC_FMT_64bppCMYK:return 1;
    case JXRC_FMT_80bppCMYKAlpha:return 1;
    case JXRC_FMT_24bpp3Channels:return 0;
    case JXRC_FMT_32bpp4Channels:return 0;
    case JXRC_FMT_40bpp5Channels:return 0;
    case JXRC_FMT_48bpp6Channels:return 0;
    case JXRC_FMT_56bpp7Channels:return 0;
    case JXRC_FMT_64bpp8Channels:return 0;
    case JXRC_FMT_32bpp3ChannelsAlpha:return 0;
    case JXRC_FMT_40bpp4ChannelsAlpha:return 0;
    case JXRC_FMT_48bpp5ChannelsAlpha:return 0;
    case JXRC_FMT_56bpp6ChannelsAlpha:return 0;
    case JXRC_FMT_64bpp7ChannelsAlpha:return 0;
    case JXRC_FMT_72bpp8ChannelsAlpha:return 0;
    case JXRC_FMT_48bpp3Channels:return 0;
    case JXRC_FMT_64bpp4Channels:return 0;
    case JXRC_FMT_80bpp5Channels:return 0;
    case JXRC_FMT_96bpp6Channels:return 0;
    case JXRC_FMT_112bpp7Channels:return 0;
    case JXRC_FMT_128bpp8Channels:return 0;
    case JXRC_FMT_64bpp3ChannelsAlpha:return 0;
    case JXRC_FMT_80bpp4ChannelsAlpha:return 0;
    case JXRC_FMT_96bpp5ChannelsAlpha:return 0;
    case JXRC_FMT_112bpp6ChannelsAlpha:return 0;
    case JXRC_FMT_128bpp7ChannelsAlpha:return 0;
    case JXRC_FMT_144bpp8ChannelsAlpha:return 0;
    case JXRC_FMT_8bppGray:return 1;
    case JXRC_FMT_16bppGray:return 1;
    case JXRC_FMT_16bppGrayFixedPoint:return 1;
    case JXRC_FMT_16bppGrayHalf:return 1;
    case JXRC_FMT_32bppGrayFixedPoint:return 1;
    case JXRC_FMT_32bppGrayFloat:return 1;
    case JXRC_FMT_BlackWhite:return 1;
    case JXRC_FMT_16bppBGR555:return 0;
    case JXRC_FMT_16bppBGR565:return 0;
    case JXRC_FMT_32bppBGR101010:return 0;
    case JXRC_FMT_32bppRGBE:return 0;
    case JXRC_FMT_32bppCMYKDIRECT:return 0;
    case JXRC_FMT_64bppCMYKDIRECT:return 0;
    case JXRC_FMT_40bppCMYKDIRECTAlpha:return 0;
    case JXRC_FMT_80bppCMYKDIRECTAlpha:return 0;
    case JXRC_FMT_12bppYCC420:return 0;
    case JXRC_FMT_16bppYCC422:return 0;
    case JXRC_FMT_20bppYCC422:return 0;
    case JXRC_FMT_32bppYCC422:return 0;
    case JXRC_FMT_24bppYCC444:return 0;
    case JXRC_FMT_30bppYCC444:return 0;
    case JXRC_FMT_48bppYCC444:return 0;
    case JXRC_FMT_48bppYCC444FixedPoint:return 0;
    case JXRC_FMT_20bppYCC420Alpha:return 0;
    case JXRC_FMT_24bppYCC422Alpha:return 0;
    case JXRC_FMT_30bppYCC422Alpha:return 0;
    case JXRC_FMT_48bppYCC422Alpha:return 0;
    case JXRC_FMT_32bppYCC444Alpha:return 0;
    case JXRC_FMT_40bppYCC444Alpha:return 0;
    case JXRC_FMT_64bppYCC444Alpha:return 0;
    case JXRC_FMT_64bppYCC444AlphaFixedPoint:return 0;
    default: return 0;
    }
}
unsigned int validate_pnm_output(jxrc_t_pixelFormat ePixelFormat)
{
    switch(ePixelFormat)
    {
    case JXRC_FMT_24bppRGB:return 1;
    case JXRC_FMT_8bppGray:return 1;       
    default: return 0;
    }
}

static void start_tif_output_file(context *con)
{     
    con->file = fopen(con->name, "wb");
    if (con->file==0)
        error("cannot create TIFF output file %s", con->name);
    con->swap = 0;

    int one = 1;
    char magic = *(char*)&one==1 ? 'I' : 'M';
    int nentry = 10;
    if (con->ncomp == 4 && !con->alpha) /* CMYK */
        nentry += 1;
    int bitsize = con->ncomp>=3 ? 2*con->ncomp : 0;
    int bitoff = 8 + 2 + 12*nentry + 4;
    int datoff = bitoff + bitsize;
    
    put_uint8(con, magic);
    put_uint8(con, magic);
    put_uint16(con, 42);
    put_uint32(con, 8);
    put_uint16(con, nentry);
    put_ifd_entry(con, ImageWidth, 4, 1, con->wid);
    put_ifd_entry(con, ImageLength, 4, 1, con->hei);
    put_ifd_entry(con, BitsPerSample, 3, con->ncomp, con->ncomp>=3 ? bitoff : con->bpi);
    switch (con->format)
    {
        case 0: /* BD1WHITE1*/
        case 1: /* BD8 */
        case 2: /* BD16 */
        /* case 5: Reserved */
        case 8: /* BD5 */
        case 9: /* BD10 */
        case 15: /* BD1BLACK1 */
            con->sf = 1;
            break;
        case 3: /* BD16S */
        case 6: /* BD32S */
            con->sf = 2;
            break;
        case 4: /* BD16F */
        case 7: /* BD32F */
            con->sf = 3;
            break;
        default:
            assert(0);
    }
    put_ifd_entry(con, SampleFormat, 3, 1, con->sf);
    put_ifd_entry(con, Compression, 3, 1, 1);
    put_ifd_entry(con, Photometric, 3, 1, con->photometric);
    put_ifd_entry(con, StripOffsets, 4, 1, datoff);
    if(!con->padBytes)
        put_ifd_entry(con, SamplesPerPixel, 3, 1, con->ncomp);
    else
        put_ifd_entry(con, SamplesPerPixel, 3, 1, con->ncomp + 1);

    put_ifd_entry(con, RowsPerStrip, 4, 1, con->hei);
    if(con->bpi == 1)
        put_ifd_entry(con, StripByteCounts, 4, 1, ((con->wid+7)>>3) * con->hei * con->ncomp);
    else if(!con->padBytes)
        put_ifd_entry(con, StripByteCounts, 4, 1, con->wid * con->hei * con->ncomp * ((con->bpi+7)/8));
    else
        put_ifd_entry(con, StripByteCounts, 4, 1, con->wid * con->hei * (con->ncomp + 1)* ((con->bpi+7)/8));

    if (con->ncomp == 4 && !con->alpha)
        put_ifd_entry(con, InkSet, 3, 1, 1);

    put_uint32(con, 0);
    assert(ftell(con->file)==bitoff);
    if (con->ncomp>=3) {
        int i;
        for (i=0; i<con->ncomp; i++)
            put_uint16(con, con->bpi);
    }
    assert(ftell(con->file)==datoff);
}

static void start_raw_output_file(context *con)
{
    con->file = fopen(con->name, "w+b");
    if (con->file==0)
        error("cannot create RAW output file %s", con->name);
    con->swap = 0;
}

/* Generic File Header Operations: */

void *open_input_file(const char *name, const raw_info *raw_info_t, int *alpha_mode, int *padded_format)
{
    context *con;

    con = (context*)malloc(sizeof(context));
    if (con==0)
        error("unable to allocate memory");

    con->name = name;
    con->wid = 0;
    con->hei = 0;
    con->ncomp = 0;
    con->bpi = 0;
    con->format = 0;
    con->sf = 1; 
    con->swap = 0;
    con->buf = 0;
    con->my = -1;
    con->nstrips = 0;
    con->strip = 0;
    con->nlines = 0;
    con->line = 0;
    con->photometric = 0; 
    con->offoff = 0;
    con->ycc_bd10_flag = 0;
    con->ycc_format = 0;

    con->file = fopen(name, "rb");
    if (con->file==0)
        error("cannot find input file %s", name);

    if (!raw_info_t->is_raw) {
        switch (get_uint8(con)) {
            case 'P' : open_pnm_input_file(con); break;
            case 'I' :
            case 'M' : open_tif_input_file(con); break;
            default : error("format of input file %s is unrecognized", name);
        }
        
        con->padBytes = 0; 
        if(con->photometric == 2 && con->ncomp == 4) /* RGBA */
            if( *padded_format == 1) { /* for RGB_NULL, there is a padding channel */
                con->padBytes = 1; 
                con->ncomp --; 
                if(*alpha_mode != 0)
                {
                    *alpha_mode = 0; /* Turn off alpha mode */
                    fprintf(stderr, "Setting alpha_mode to 0 to encode a padded format \n");
                }
            }
            else
            {
                if(*alpha_mode == 0)
                {
                    *alpha_mode = 2; /* Turn on separate alpha */                    
                }
            }
    }
    else { /* raw input */
        con->wid = raw_info_t->raw_width;
        con->hei = raw_info_t->raw_height;
        con->nlines = con->hei;
        con->bpi = raw_info_t->raw_bpc;
        con->sf = 1; /* UINT */
        con->padBytes = 0; 
        if (con->bpi == 10 && raw_info_t->raw_format > 18) {
            con->ycc_bd10_flag = 1;
            con->bpi = 16;
        }

        if ((raw_info_t->raw_format >= 3) && (raw_info_t->raw_format <= 8)) { /* N-channel */
            con->ncomp = raw_info_t->raw_format;
        }
        else if ((raw_info_t->raw_format >= 9) && (raw_info_t->raw_format <= 14)) { /* N-channel with Alpha */
            con->ncomp = raw_info_t->raw_format - 5;
        }
        else if (15 == raw_info_t->raw_format) {/* RGBE */
            con->ncomp = 4;
            con->sf = 4;
        }
        else if (16 == raw_info_t->raw_format) {/* 555 */
            con->ncomp = 3;
            con->bpi = 5;
        }
        else if (17 == raw_info_t->raw_format) {/* 565 */
            con->ncomp = 3;
            con->bpi = 6;
        }
        else if (18 == raw_info_t->raw_format) {/* 101010 */
            con->ncomp = 3;
            con->bpi = 10;
        }
        else if (19 == raw_info_t->raw_format) {
            con->ncomp = 3;
            con->ycc_format = 1;
        }
        else if (20 == raw_info_t->raw_format) {
            con->ncomp = 3;
            con->ycc_format = 2;
        }
        else if (21 == raw_info_t->raw_format) {
            con->ncomp = 3;
            con->ycc_format = 3;
        }
        else if (22 == raw_info_t->raw_format) {
            con->ncomp = 3;
            con->sf = 2; /* SINT */
            con->ycc_format = 3;
        }
        else if (23 == raw_info_t->raw_format) {
            con->ncomp = 4;
            con->ycc_format = 1;
        }
        else if (24 == raw_info_t->raw_format) {
            con->ncomp = 4;
            con->ycc_format = 2;
        }
        else if (25 == raw_info_t->raw_format) {
            con->ncomp = 4;
            con->ycc_format = 3;
        }
        else if (26 == raw_info_t->raw_format) {
            con->ncomp = 4;
            con->sf = 2; /* SINT */
            con->ycc_format = 3;
        }
        else if (27 == raw_info_t->raw_format) {
            con->ncomp = 4;
        }
        else if (28 == raw_info_t->raw_format) {
            con->ncomp = 5;
        }
        else if(29 == raw_info_t->raw_format) {/* 24bppBGR */
            con->ncomp = 3;
            con->bpi = 8;            
        }
        else if(30 == raw_info_t->raw_format) {/* 32bppBGR */
            con->ncomp = 3;
            con->bpi = 8;
            con->padBytes = 1;
            *padded_format = 1;
        }
        else if(31 == raw_info_t->raw_format) {/* 32bppBGRA */
            con->ncomp = 4;
            con->bpi = 8;            
        }
        else if(32 == raw_info_t->raw_format) {/* 32bppPBGRA */
            con->ncomp = 4;
            con->bpi = 8;            
        }
        else if(33 == raw_info_t->raw_format) {/* 64bppPRGBA */
            con->ncomp = 4;
            con->bpi = 16;            
        }
        else if(34 == raw_info_t->raw_format) {/* 128bppPRGBAFloat */
            con->ncomp = 4;
            con->bpi = 32;            
            con->sf = 3;
            con->photometric = 2;            
        }
    }

    if(con->padBytes == 0 && *padded_format)
    {
        *padded_format = 0;
        fprintf(stderr, "Ignoring -p option from command line \n");
    }

    size_t strip_bytes;
    if (con->ycc_format == 1)
        strip_bytes = 8 * ((4*con->ncomp-6) + con->padBytes) * ((con->bpi+7)/8) * ((con->wid+15)&~15)/2;
    else if (con->ycc_format == 2)
        strip_bytes = 16 * ((2*con->ncomp-2) + con->padBytes) * ((con->bpi+7)/8) * ((con->wid+15)&~15)/2;
    else
        strip_bytes = 16 * (con->ncomp + con->padBytes) * ((con->bpi+7)/8) * ((con->wid+15)&~15);

    if (con->bpi == 1)
        strip_bytes >>= 3;
    else if ((con->bpi == 5) || (con->bpi == 6) || (con->bpi == 10 && !con->ycc_bd10_flag))
        strip_bytes = (strip_bytes << 1) / 3; /* external buffer */

    con->buf = calloc(strip_bytes, 1);
    if (con->buf==0)
        error("cannot allocate memory");

    return con;
}
void set_ncomp(void *input_handle, int ncomp)
{
    context *con = (context *)input_handle;
    con->ncomp = ncomp;    
}

void *open_output_file(const char *name)
{
    context *con = (context*)malloc(sizeof(context));
    if (con==0)
        error("unable to allocate memory");
    con->file = 0;
    con->name = name;
    con->wid = 0;
    con->hei = 0;
    con->ncomp = 0;
    con->bpi = 0;
    con->format = 0;
    con->swap = 0;
    con->buf = 0;
    return con;
}

void close_file(void *handle)
{
    if(handle == NULL)
        return;
    context *con = (context*)handle;
    if (con->file) fclose(con->file);
    if (con->buf) free(con->buf);
    free(con);    
}

void get_file_parameters(void *handle, int *wid, int *hei, int *ncomp, int *bpi, short *sf, short *photometric, int *padBytes)
{
    context *con = (context*)handle;
    if (wid) *wid = con->wid;
    if (hei) *hei = con->hei;
    if (ncomp) *ncomp = con->ncomp;
    if (bpi) *bpi = con->bpi;
    if (sf) *sf = con->sf;
    if (photometric) *photometric = con->photometric;
    if (padBytes) *padBytes = con->padBytes;
}

void set_photometric_interp(context *con, jxrc_t_pixelFormat pixelFormat)
{
    switch(pixelFormat)
    {
    case JXRC_FMT_32bppCMYK:
    case JXRC_FMT_40bppCMYKAlpha:
    case JXRC_FMT_64bppCMYK:
    case JXRC_FMT_80bppCMYKAlpha:
        con->photometric = 5;
        return;
    case JXRC_FMT_8bppGray:        
    case JXRC_FMT_16bppGray:
    case JXRC_FMT_16bppGrayFixedPoint:
    case JXRC_FMT_16bppGrayHalf:
    case JXRC_FMT_32bppGrayFixedPoint:
    case JXRC_FMT_32bppGrayFloat:
    case JXRC_FMT_BlackWhite:
        con->photometric = 1;
        return;
    default:
        con->photometric = 2;
        return;
    }
}

static void start_output_file(context *con, int ext_width, int ext_height, int width, int height, int ncomp, int format, jxrc_t_pixelFormat pixelFormat)
{
    int bpi=0;
    switch (format) {
        case 0 : bpi=1; break;
        case 1 : bpi=8; break;
        case 2 :
        case 3 :
        case 4 : bpi=16; break;
        case 5 :
        case 6 :
        case 7 : bpi=32; break;
        case 8 : bpi=5; break;
        case 9 : bpi=10; break;
        case 10 : bpi=6; break;
        case 15 : bpi=1; break;
        default : error("invalid component format code (%d) for output file %s", format, con->name);
    }

    if (bpi<1 || bpi>32)
        error("invalid bits per sample (%d) for output file %s", bpi, con->name);
    if (width<=0 || height<=0)
        error("invalid dimensions (%d X %d) for output file %s", width, height, con->name);
    
    con->wid = width;
    con->hei = height;
    con->ncomp = ncomp;
    con->bpi = bpi;
    con->format = format;
    /* Add one extra component for possible padding channel */
    con->buf = malloc(((ext_width)/16) * 256 * (ncomp + 1) * ((con->bpi+7)/8));
    if (con->buf==0) error("unable to allocate memory");

    const char *p = strrchr(con->name, '.');
    if (p==0)
        error("output file name %s needs a suffix to determine its format", con->name);
    if (!strcasecmp(p, ".pnm") || !strcasecmp(p, ".pgm") || !strcasecmp(p, ".ppm"))
    {
        if(!validate_pnm_output(pixelFormat))
        {
            printf("User error: PixelFormat is incompatible with pnm output, use .raw or .tif extension for output file\n");
            assert(0);
        }
        start_pnm_output_file(con);
    }
    else if (!strcasecmp(p, ".tif"))
    {       
        if(!validate_tif_output(pixelFormat) && ncomp != 1)
        {
            printf("User error: PixelFormat is incompatible with tif output, use .raw extension for output file\n");
            assert(0);
        }
        set_photometric_interp(con, pixelFormat);
        start_tif_output_file(con);
    }
    else if (!strcasecmp(p, ".raw"))
        start_raw_output_file(con);
    else error("unrecognized suffix on output file name %s", con->name);
}

/* File Read and Write Operations: */

static int PreScalingBD16F(int hHalf)
{
    int s;
    s = (hHalf >> 31);
    hHalf = ((hHalf & 0x7fff) ^ s) - s;
    return hHalf;
}

static int PreScalingBD32F(int f, const char _c, const unsigned char _lm)
{
    int _h, e, e1, m, s;

    if (f == 0)
    {
        _h = 0;
    }
    else
    {
        e = (f >> 23) & 0x000000ff;/* here set e as e, not s! e includes s: [s e] 9 bits [31..23] */
        m = (f & 0x007fffff) | 0x800000; /* actual mantissa, with normalizer */
        if (e == 0) { /* denormal-land */
            m ^= 0x800000;  /* actual mantissa, removing normalizer */
            e++; /* actual exponent -126 */
        }

        e1 = e - 127 + _c;  /* this is basically a division or quantization to a different exponent */

        if (e1 <= 1) {  /* denormal */
            if (e1 < 1)
                m >>= (1 - e1);  /* shift mantissa right to make exponent 1 */
            e1 = 1;
            if ((m & 0x800000) == 0) /* if denormal, set e1 to zero else to 1 */
                e1 = 0;
        }
        m &= 0x007fffff;

        _h = (e1 << _lm) + ((m + (1 << (23 - _lm - 1))) >> (23 - _lm));/* take 23-bit m, shift (23-lm), get lm-bit m */
        s = (f >> 31);
        /* padding to int-32: */
        _h = (_h ^ s) - s;	
    }

    return _h;
}

int forwardRGBE (int RGB, int E)
{
    int iResult = 0, iAppend = 1;

    if (E == 0)
        return 0;

    E--;
    while (((RGB & 0x80) == 0) && (E > 0)) {
        RGB = (RGB << 1) + iAppend;
        iAppend = 0;
        E--;    
    }

    if (E == 0) {
        iResult = RGB;
    }
    else {
        E++;
        iResult = (RGB & 0x7f) + (E << 7);
    }

    return iResult;
}

void set_pad_bytes(context *con, jxr_image_t image)
{
    /* Incomplete: Add padding data for other formats as we go */
    switch (jxr_get_pixel_format(image))
    {
        case JXRC_FMT_128bppRGBFloat:
        case JXRC_FMT_128bppRGBFixedPoint:
            con->padBytes = 32;
            break;
        case JXRC_FMT_64bppRGBFixedPoint:
        case JXRC_FMT_64bppRGBHalf:
            con->padBytes = 16;
            break;
        case JXRC_FMT_32bppBGR:
            con->padBytes = 8;
            break;
        default:
            con->padBytes = 0;
            break;
    }

}

void set_bgr_flag(context *con, jxr_image_t image)
{
    con->isBgr = 0;
    switch (jxr_get_pixel_format(image))
    {
        case JXRC_FMT_24bppBGR:
        case JXRC_FMT_32bppBGR:
        case JXRC_FMT_32bppBGRA:
        case JXRC_FMT_32bppPBGRA:        
            con->isBgr = 1;
            break;
        default:
            break;
    }    
    return;
}

void switch_r_b(void *data, int bpi)
{
    uint32_t tmp;
    data = (uint8_t*)data;
    if(bpi == 8)
    {
        uint8_t *p = (uint8_t*)data;
        tmp = p[0];
        p[0] = p[2];
        p[2] = tmp;
        
    }
    
    else if(bpi == 16)
    {
        uint16_t *p = (uint16_t*)data;
        tmp = p[0];
        p[0] = p[2];
        p[2] = tmp;
    }
    else if(bpi == 32)
    {
        uint32_t *p = (uint32_t *)data;
        tmp = p[0];
        p[0] = p[2];
        p[2] = tmp;        
    }
    else
    {
        assert(!"Invalid bpi\n");
    }
}

static void read_setup(context *con)
{
    if (con->line == con->nlines) {
        if (con->strip == con->nstrips)
            error("unexpected end of data encountered in input file %s", con->name);
        seek_file(con, get_tif_datum(con, con->offoff, con->strip), SEEK_SET);
        con->strip++;
    }
    con->line++;
}

void read_file_YCC(jxr_image_t image, int mx, int my, int *data) {
    /* There are 3 or 4 buffers, depending on whether there is an alpha channel or not */
    context *con = (context*) jxr_get_user_data(image);
    int num_channels = con->ncomp;

    unsigned int widthY = con->wid, heightY = con->hei;
    unsigned int widthUV = con->wid >> ((con->ycc_format == 1 || con->ycc_format == 2) ? 1 : 0);
    unsigned int heightUV = con->hei >> (con->ycc_format == 1 ? 1 : 0);
    unsigned int MBheightY = 16;
    unsigned int MBheightUV = con->ycc_format == 1 ? 8 : 16;
    unsigned int sizeY = widthY * heightY;
    unsigned int sizeUV = widthUV * heightUV;

    if (my != con->my) {
        if (con->bpi == 8) {
            unsigned int offsetY = my * MBheightY * widthY;
            unsigned int offsetU = sizeY + my * MBheightUV * widthUV;
            unsigned int offsetV = sizeY + sizeUV + my * MBheightUV * widthUV;
            unsigned int offsetA = sizeY + 2 * sizeUV + my * MBheightY * widthY;

            uint8_t *sp = (uint8_t*)con->buf;

            seek_file(con, offsetY, SEEK_SET);
            memset(sp, 0, widthY * MBheightY);
            read_data(con, sp, 1, widthY * MBheightY);
            sp += widthY * MBheightY;

            seek_file(con, offsetU, SEEK_SET);
            memset(sp, 0, widthUV * MBheightUV);
            read_data(con, sp, 1, widthUV * MBheightUV);
            sp += widthUV * MBheightUV;

            seek_file(con, offsetV, SEEK_SET);
            memset(sp, 0, widthUV * MBheightUV);
            read_data(con, sp, 1, widthUV * MBheightUV);
            sp += widthUV * MBheightUV;

            if (con->ncomp == 4) {
                seek_file(con, offsetA, SEEK_SET);
                memset(sp, 0, widthY * MBheightY);
                read_data(con, sp, 1, widthY * MBheightY);
                sp += widthY * MBheightY;
            }
        }
        else if (con->bpi == 16) {
            unsigned int offsetY = 2 * (my * MBheightY * widthY);
            unsigned int offsetU = 2 * (sizeY + my * MBheightUV * widthUV);
            unsigned int offsetV = 2 * (sizeY + sizeUV + my * MBheightUV * widthUV);
            unsigned int offsetA = 2 * (sizeY + 2 * sizeUV + my * MBheightY * widthY);

            uint16_t *sp = (uint16_t*)con->buf;

            seek_file(con, offsetY, SEEK_SET);
            memset(sp, 0, 2 * widthY * MBheightY);
            read_data(con, sp, 2, widthY * MBheightY);
            sp += widthY * MBheightY;

            seek_file(con, offsetU, SEEK_SET);
            memset(sp, 0, 2 * widthUV * MBheightUV);
            read_data(con, sp, 2, widthUV * MBheightUV);
            sp += widthUV * MBheightUV;

            seek_file(con, offsetV, SEEK_SET);
            memset(sp, 0, 2 * widthUV * MBheightUV);
            read_data(con, sp, 2, widthUV * MBheightUV);
            sp += widthUV * MBheightUV;

            if (con->ncomp == 4) {
                seek_file(con, offsetA, SEEK_SET);
                memset(sp, 0, 2 * widthY * MBheightY);
                read_data(con, sp, 2, widthY * MBheightY);
                sp += widthY * MBheightY;
            }
        }
        con->my = my;
    }

    int idx1, idx2;
    int xDiv = 16;
    int yDiv = 16;

    if (con->bpi == 8) {
        /* Y */
        uint8_t *sp = (uint8_t*)con->buf + xDiv*mx;
        for (idx1 = 0; idx1 < yDiv; idx1 += 1) {
            for (idx2 = 0; idx2 < xDiv; idx2 += 1) 
                data[(idx1 * xDiv + idx2) * num_channels + 0] = sp[idx2];
            sp += widthY;
        }

        if (con->ycc_format == 2)
            xDiv = 8;

        if (con->ycc_format == 1)
            xDiv = yDiv = 8;

        /* U */
        sp = (uint8_t*)con->buf + widthY * MBheightY + xDiv*mx;
        for (idx1 = 0; idx1 < yDiv; idx1 += 1) {
            for (idx2 = 0; idx2 < xDiv; idx2 += 1) 
                data[(idx1 * xDiv + idx2) * num_channels + 1] = sp[idx2];
            sp += widthUV;
        }

        /* V */
        sp = (uint8_t*)con->buf + widthY * MBheightY + widthUV * MBheightUV + xDiv*mx;
        for (idx1 = 0; idx1 < yDiv; idx1 += 1) {
            for (idx2 = 0; idx2 < xDiv; idx2 += 1) 
                data[(idx1 * xDiv + idx2) * num_channels + 2] = sp[idx2];
            sp += widthUV;
        }

        if(con->ncomp == 4)
        {
            xDiv = yDiv = 16;
            sp = (uint8_t*)con->buf + widthY * MBheightY + 2 * widthUV * MBheightUV  + xDiv*mx;
            for (idx1 = 0; idx1 < yDiv; idx1 += 1) {
                for (idx2 = 0; idx2 < xDiv; idx2 += 1) 
                    data[(idx1 * xDiv + idx2) * num_channels + 3] = sp[idx2];
                sp += widthY;
            }
        }
    }
    else if (con->bpi == 16 || con->bpi == 10) {
        if (con->sf == 1) {
            /* Y */
            uint16_t *sp = (uint16_t*)con->buf + xDiv*mx;
            for (idx1 = 0; idx1 < yDiv; idx1 += 1) {
                for (idx2 = 0; idx2 < xDiv; idx2 += 1) 
                    data[(idx1 * xDiv + idx2) * num_channels + 0] = sp[idx2];
                sp += widthY;
            }

            if (con->ycc_format == 2)
                xDiv = 8;

            if (con->ycc_format == 1)
                xDiv = yDiv = 8;

            /* U */
            sp = (uint16_t*)con->buf + widthY * MBheightY + xDiv*mx;
            for (idx1 = 0; idx1 < yDiv; idx1 += 1) {
                for (idx2 = 0; idx2 < xDiv; idx2 += 1) 
                    data[(idx1 * xDiv + idx2) * num_channels + 1] = sp[idx2];
                sp += widthUV;
            }

            /* V */
            sp = (uint16_t*)con->buf + widthY * MBheightY + widthUV * MBheightUV + xDiv*mx;
            for (idx1 = 0; idx1 < yDiv; idx1 += 1) {
                for (idx2 = 0; idx2 < xDiv; idx2 += 1) 
                    data[(idx1 * xDiv + idx2) * num_channels + 2] = sp[idx2];
                sp += widthUV;
            }

            if(con->ncomp == 4)
            {
                xDiv = yDiv = 16;
                sp = (uint16_t*)con->buf + widthY * MBheightY + 2 * widthUV * MBheightUV  + xDiv*mx;
                for (idx1 = 0; idx1 < yDiv; idx1 += 1) {
                    for (idx2 = 0; idx2 < xDiv; idx2 += 1) 
                        data[(idx1 * xDiv + idx2) * num_channels + 3] = sp[idx2];
                    sp += widthY;
                }
            }
        }
        else if (con->sf == 2) {
            /* Y */
            short *sp = (short*)con->buf + xDiv*mx;
            for (idx1 = 0; idx1 < yDiv; idx1 += 1) {
                for (idx2 = 0; idx2 < xDiv; idx2 += 1) 
                    data[(idx1 * xDiv + idx2) * num_channels + 0] = sp[idx2];
                sp += widthY;
            }

            if (con->ycc_format == 2)
                xDiv = 8;

            if (con->ycc_format == 1)
                xDiv = yDiv = 8;

            /* U */
            sp = (short*)con->buf + widthY * MBheightY + xDiv*mx;
            for (idx1 = 0; idx1 < yDiv; idx1 += 1) {
                for (idx2 = 0; idx2 < xDiv; idx2 += 1) 
                    data[(idx1 * xDiv + idx2) * num_channels + 1] = sp[idx2];
                sp += widthUV;
            }

            /* V */
            sp = (short*)con->buf + widthY * MBheightY + widthUV * MBheightUV + xDiv*mx;
            for (idx1 = 0; idx1 < yDiv; idx1 += 1) {
                for (idx2 = 0; idx2 < xDiv; idx2 += 1) 
                    data[(idx1 * xDiv + idx2) * num_channels + 2] = sp[idx2];
                sp += widthUV;
            }

            if(con->ncomp == 4)
            {
                xDiv = yDiv = 16;
                sp = (short*)con->buf + widthY * MBheightY + 2 * widthUV * MBheightUV  + xDiv*mx;
                for (idx1 = 0; idx1 < yDiv; idx1 += 1) {
                    for (idx2 = 0; idx2 < xDiv; idx2 += 1) 
                        data[(idx1 * xDiv + idx2) * num_channels + 3] = sp[idx2];
                    sp += widthY;
                }
            }
        }
    }
}

void read_file_CMYK(jxr_image_t image, int mx, int my, int *data) {
    /* There are 4 or 5 buffers, depending on whether there is an alpha channel or not */
    context *con = (context*) jxr_get_user_data(image);
    int num_channels = con->ncomp;

    unsigned int width = con->wid, height = con->hei;
    unsigned int MBheight = 16;
    unsigned int size = width * height;

    if (my != con->my) {
        if (con->bpi == 8) {
            unsigned int offsetC = my * MBheight * width + 0 * size;
            unsigned int offsetM = my * MBheight * width + 1 * size;
            unsigned int offsetY = my * MBheight * width + 2 * size;
            unsigned int offsetK = my * MBheight * width + 3 * size;
            unsigned int offsetA = my * MBheight * width + 4 * size;

            uint8_t *sp = (uint8_t*)con->buf;

            seek_file(con, offsetC, SEEK_SET);
            memset(sp, 0, width * MBheight);
            read_data(con, sp, 1, width * MBheight);
            sp += width * MBheight;

            seek_file(con, offsetM, SEEK_SET);
            memset(sp, 0, width * MBheight);
            read_data(con, sp, 1, width * MBheight);
            sp += width * MBheight;

            seek_file(con, offsetY, SEEK_SET);
            memset(sp, 0, width * MBheight);
            read_data(con, sp, 1, width * MBheight);
            sp += width * MBheight;

            seek_file(con, offsetK, SEEK_SET);
            memset(sp, 0, width * MBheight);
            read_data(con, sp, 1, width * MBheight);
            sp += width * MBheight;

            if (con->ncomp == 5) {
                seek_file(con, offsetA, SEEK_SET);
                memset(sp, 0, width * MBheight);
                read_data(con, sp, 1, width * MBheight);
                sp += width * MBheight;
            }
        }
        else if (con->bpi == 16) {
            unsigned int offsetC = 2 * (my * MBheight * width + 0 * size);
            unsigned int offsetM = 2 * (my * MBheight * width + 1 * size);
            unsigned int offsetY = 2 * (my * MBheight * width + 2 * size);
            unsigned int offsetK = 2 * (my * MBheight * width + 3 * size);
            unsigned int offsetA = 2 * (my * MBheight * width + 4 * size);

            uint16_t *sp = (uint16_t*)con->buf;

            seek_file(con, offsetC, SEEK_SET);
            memset(sp, 0, 2 * width * MBheight);
            read_data(con, sp, 2, width * MBheight);
            sp += width * MBheight;

            seek_file(con, offsetM, SEEK_SET);
            memset(sp, 0, 2 * width * MBheight);
            read_data(con, sp, 2, width * MBheight);
            sp += width * MBheight;

            seek_file(con, offsetY, SEEK_SET);
            memset(sp, 0, 2 * width * MBheight);
            read_data(con, sp, 2, width * MBheight);
            sp += width * MBheight;

            seek_file(con, offsetK, SEEK_SET);
            memset(sp, 0, 2 * width * MBheight);
            read_data(con, sp, 2, width * MBheight);
            sp += width * MBheight;

            if (con->ncomp == 5) {
                seek_file(con, offsetA, SEEK_SET);
                memset(sp, 0, 2 * width * MBheight);
                read_data(con, sp, 2, width * MBheight);
                sp += width * MBheight;
            }
        }
        con->my = my;
    }

    int idx1, idx2;
    int xDiv = 16;
    int yDiv = 16;

    if (con->bpi == 8) {
        /* C */
        uint8_t *sp = (uint8_t*)con->buf + xDiv*mx + 0 * width * MBheight;
        for (idx1 = 0; idx1 < yDiv; idx1 += 1) {
            for (idx2 = 0; idx2 < xDiv; idx2 += 1) 
                data[(idx1 * xDiv + idx2) * num_channels + 0] = sp[idx2];
            sp += width;
        }

        /* M */
        sp = (uint8_t*)con->buf + xDiv*mx + 1 * width * MBheight;
        for (idx1 = 0; idx1 < yDiv; idx1 += 1) {
            for (idx2 = 0; idx2 < xDiv; idx2 += 1) 
                data[(idx1 * xDiv + idx2) * num_channels + 1] = sp[idx2];
            sp += width;
        }

        /* Y */
        sp = (uint8_t*)con->buf + xDiv*mx + 2 * width * MBheight;
        for (idx1 = 0; idx1 < yDiv; idx1 += 1) {
            for (idx2 = 0; idx2 < xDiv; idx2 += 1) 
                data[(idx1 * xDiv + idx2) * num_channels + 2] = sp[idx2];
            sp += width;
        }

        /* K */
        sp = (uint8_t*)con->buf + xDiv*mx + 3 * width * MBheight;
        for (idx1 = 0; idx1 < yDiv; idx1 += 1) {
            for (idx2 = 0; idx2 < xDiv; idx2 += 1) 
                data[(idx1 * xDiv + idx2) * num_channels + 3] = sp[idx2];
            sp += width;
        }

        if(con->ncomp == 5)
        {
            sp = (uint8_t*)con->buf + xDiv*mx + 4 * width * MBheight;
            for (idx1 = 0; idx1 < yDiv; idx1 += 1) {
                for (idx2 = 0; idx2 < xDiv; idx2 += 1) 
                    data[(idx1 * xDiv + idx2) * num_channels + 4] = sp[idx2];
                sp += width;
            }
        }
    }
    else if (con->bpi == 16) {
        /* C */
        uint16_t *sp = (uint16_t*)con->buf + xDiv*mx + 0 * width * MBheight;
        for (idx1 = 0; idx1 < yDiv; idx1 += 1) {
            for (idx2 = 0; idx2 < xDiv; idx2 += 1) 
                data[(idx1 * xDiv + idx2) * num_channels + 0] = sp[idx2];
            sp += width;
        }

        /* M */
        sp = (uint16_t*)con->buf + xDiv*mx + 1 * width * MBheight;
        for (idx1 = 0; idx1 < yDiv; idx1 += 1) {
            for (idx2 = 0; idx2 < xDiv; idx2 += 1) 
                data[(idx1 * xDiv + idx2) * num_channels + 1] = sp[idx2];
            sp += width;
        }

        /* Y */
        sp = (uint16_t*)con->buf + xDiv*mx + 2 * width * MBheight;
        for (idx1 = 0; idx1 < yDiv; idx1 += 1) {
            for (idx2 = 0; idx2 < xDiv; idx2 += 1) 
                data[(idx1 * xDiv + idx2) * num_channels + 2] = sp[idx2];
            sp += width;
        }

        /* K */
        sp = (uint16_t*)con->buf + xDiv*mx + 3 * width * MBheight;
        for (idx1 = 0; idx1 < yDiv; idx1 += 1) {
            for (idx2 = 0; idx2 < xDiv; idx2 += 1) 
                data[(idx1 * xDiv + idx2) * num_channels + 3] = sp[idx2];
            sp += width;
        }

        if(con->ncomp == 5)
        {
            sp = (uint16_t*)con->buf + xDiv*mx + 4 * width * MBheight;
            for (idx1 = 0; idx1 < yDiv; idx1 += 1) {
                for (idx2 = 0; idx2 < xDiv; idx2 += 1) 
                    data[(idx1 * xDiv + idx2) * num_channels + 4] = sp[idx2];
                sp += width;
            }
        }
    }
}


void read_file(jxr_image_t image, int mx, int my, int *data)
{
    context *con = (context*) jxr_get_user_data(image);
    unsigned char uExternalNcomp = con->ncomp + con->padBytes;
    int block_wid = uExternalNcomp * ((con->wid+15)&~15);

    if((isOutputYUV444(image) || isOutputYUV422(image) || isOutputYUV420(image)) && jxr_get_IMAGE_CHANNELS(image) == 3)
    {
        /* Use special YCC format only for primary image */
        read_file_YCC(image, mx, my, data);
        return;
    }
    else if(isOutputCMYKDirect(image) && jxr_get_IMAGE_CHANNELS(image) == 4)
    {
        /* Use special YCC format only for primary image */
        read_file_CMYK(image, mx, my, data);
        return;
    }

    if(con->ncomp == 3 || con->ncomp == 4)
    {
        set_bgr_flag(con, image);
    }
    else
    {
        con->isBgr = 0;
    }

    if (my != con->my) {
        int trans = (my*16 + 16 > con->hei) ? con->hei%16 : 16;
        int line_wid = uExternalNcomp * con->wid;
        int idx;
        if (con->bpi == 1) {
            uint8_t *sp = (uint8_t*)con->buf;
            line_wid = ((line_wid + 7) >> 3);
            block_wid >>= 3;
            for (idx = 0; idx < trans; idx += 1) {
                read_setup(con);
                read_uint8(con, sp, line_wid);
                sp += block_wid; 
            } 
        }
        else if ((con->bpi == 5) || (con->bpi == 6)) {
            uint16_t *sp = (uint16_t*)con->buf;            
            block_wid = ((con->wid+15)&~15);
            for (idx = 0; idx < trans; idx += 1) {
                read_setup(con);
                read_uint16(con, sp, con->wid);
                sp += block_wid; 
            } 
        }
        else if (con->bpi == 8) {
            uint8_t *sp = (uint8_t*)con->buf;
            for (idx = 0; idx < trans; idx += 1) {
                read_setup(con);
                read_uint8(con, sp, line_wid);
                if(con->isBgr)
                {
                    int i;
                    for(i = 0; i < con->wid; i ++)
                    {
                        int tmp = sp[i*(uExternalNcomp)] ;
                        sp[i*(uExternalNcomp)] = sp[i*(uExternalNcomp)+ 2];
                        sp[i*(uExternalNcomp)+ 2] = tmp;
                    }
                }
                sp += block_wid; 
            } 
        }
        else if (con->bpi == 10) {
            uint32_t *sp = (uint32_t*)con->buf;
            block_wid = ((con->wid+15)&~15);
            for (idx = 0; idx < trans; idx += 1) {
                read_setup(con);
                read_uint32(con, sp, con->wid);
                sp += block_wid; 
            } 
        }
        else if (con->bpi == 16) {
            if (con->sf == 1) { /* UINT */
                uint16_t *sp = (uint16_t*)con->buf;
                for (idx = 0; idx < trans; idx += 1) {
                    read_setup(con);
                    read_uint16(con, sp, line_wid);
                    sp += block_wid; 
                } 
            }
            else if (con->sf == 2) { /* fixed point */
                short *sp = (short*)con->buf;
                for (idx = 0; idx < trans; idx += 1) {
                    read_setup(con);
                    read_uint16(con, (uint16_t *)sp, line_wid);
                    sp += block_wid; 
                } 
            }
            else if (con->sf == 3) { /* Half float */
                short *sp = (short*)con->buf;
                for (idx = 0; idx < trans; idx += 1) {
                    read_setup(con);
                    read_uint16(con, (uint16_t *)sp, line_wid);
                    sp += block_wid; 
                } 
            }
        }
        else if (con->bpi == 32) {
            if (con->sf == 2) { /* fixed point */
                uint32_t *sp = (uint32_t*)con->buf;
                for (idx = 0; idx < trans; idx += 1) {
                    read_setup(con);
                    read_uint32(con, sp, line_wid);
                    sp += block_wid; 
                } 
            }
            else if (con->sf == 3) { /* float */
                uint32_t *sp = (uint32_t*)con->buf;
                for (idx = 0; idx < trans; idx += 1) {
                    read_setup(con);
                    read_uint32(con, sp, line_wid);
                    sp += block_wid; 
                } 
            }
        }
        con->my = my;
    }

    int xdx, ydx, sxdx;
    if (con->bpi == 1) {
        block_wid = (((con->wid+15)&~15) >> 3);
        for (ydx = 0 ; ydx < 16 ; ydx += 1) {
            if (con->sf == 1) { 
                uint8_t *sp = (uint8_t*)con->buf + ydx*block_wid + ((con->ncomp*16*mx) >> 3);
                int *dp = data + con->ncomp*16*ydx;
                if (!con->photometric) {
                    for (xdx = 0 ; xdx < con->ncomp*16 ; xdx++)
                        dp[xdx] = ((sp[xdx >> 3] >> (7 - (xdx & 7))) & 1);
                } 
                else {
                    for (xdx = 0 ; xdx < con->ncomp*16 ; xdx++)
                        dp[xdx] = (((~sp[xdx >> 3]) >> (7 - (xdx & 7))) & 1);
                }
            }
        }
    }
    else if (con->bpi == 5) {
        block_wid = ((con->wid+15)&~15);
        for (ydx = 0 ; ydx < 16 ; ydx += 1) {              
            uint16_t *sp = (uint16_t*)con->buf + ydx*block_wid + 16*mx;
            int *dp = data + uExternalNcomp*16*ydx;
            for (xdx = 0, sxdx = 0; xdx < uExternalNcomp*16 ; xdx += uExternalNcomp, sxdx++) {
                dp[xdx] = (sp[sxdx] & 0x1f);
                dp[xdx + 1] = ((sp[sxdx] >> 5) & 0x1f);
                dp[xdx + 2] = ((sp[sxdx] >> 10) & 0x1f);
            }
        }
    }
    else if (con->bpi == 6) {
        block_wid = ((con->wid+15)&~15);
        for (ydx = 0 ; ydx < 16 ; ydx += 1) {            
            uint16_t *sp = (uint16_t*)con->buf + ydx*block_wid + 16*mx;
            int *dp = data + uExternalNcomp*16*ydx;
            for (xdx = 0, sxdx = 0; xdx < uExternalNcomp*16 ; xdx += uExternalNcomp, sxdx++) {
                dp[xdx] = ((sp[sxdx] & 0x1f) << 1);
                dp[xdx + 1] = ((sp[sxdx] >> 5) & 0x3f);
                dp[xdx + 2] = (((sp[sxdx] >> 11) & 0x1f) << 1);
            }
        }
    }
    else if (con->bpi == 8) {
        for (ydx = 0 ; ydx < 16 ; ydx += 1) {
            if (con->sf == 1) { /* UINT */
                uint8_t *sp = (uint8_t*)con->buf + ydx*block_wid + uExternalNcomp*16*mx;
                int *dp = data +  con->ncomp*16*ydx;
                int iCh;
                for (xdx = 0 ; xdx < 16 ; xdx++) {
                    for (iCh = 0; iCh < con->ncomp; iCh ++){
                        dp[iCh] = (sp[iCh] >> image->shift_bits); 
                    }
                    dp += con->ncomp;
                    sp += uExternalNcomp;
                }
            }
            else if (con->sf == 4){ /* RGBE */
                uint8_t *sp = (uint8_t*)con->buf + ydx*block_wid + con->ncomp*16*mx; /* external ncomp is 4, internal is 3 */
                int *dp = data + 3*16*ydx;
                for (xdx = 0, sxdx = 0 ; xdx < 3*16 ; xdx += 3, sxdx +=4) {
                    dp[xdx] = forwardRGBE((int)sp[sxdx], (int)sp[sxdx + 3]); 
                    dp[xdx + 1] = forwardRGBE((int)sp[sxdx + 1], (int)sp[sxdx + 3]);
                    dp[xdx + 2] = forwardRGBE((int)sp[sxdx + 2], (int)sp[sxdx + 3]);
                }
            }
        }
    }
    else if (con->bpi == 10) {
        block_wid = ((con->wid+15)&~15);
        for (ydx = 0 ; ydx < 16 ; ydx += 1) {            
            uint32_t *sp = (uint32_t*)con->buf + ydx*block_wid + 16*mx;
            int *dp = data + uExternalNcomp*16*ydx;
            for (xdx = 0, sxdx = 0; xdx < uExternalNcomp*16 ; xdx += uExternalNcomp, sxdx++) {
                dp[xdx] = (sp[sxdx] & 0x3ff);
                dp[xdx + 1] = ((sp[sxdx] >> 10) & 0x3ff);
                dp[xdx + 2] = ((sp[sxdx] >> 20) & 0x3ff);
            }
        }
    }
    else if (con->bpi == 16) {
        if (con->sf == 1) { /* UINT */
            for (ydx = 0 ; ydx < 16 ; ydx += 1) {
                uint16_t *sp = (uint16_t*)con->buf + ydx*block_wid + con->ncomp*16*mx;
                int *dp = data + con->ncomp*16*ydx;
                for (xdx = 0 ; xdx < con->ncomp*16 ; xdx++)
                    dp[xdx] = (sp[xdx] >> image->shift_bits); 
            } 
        }
        else if (con->sf == 2) { /* fixed point */
            for (ydx = 0 ; ydx < 16 ; ydx += 1) {
                short *sp = (short*)con->buf + ydx*block_wid + uExternalNcomp*16*mx;
                int *dp = data + con->ncomp*16*ydx;
                int iCh;
                for (xdx = 0 ; xdx < 16 ; xdx++) {
                    for (iCh = 0; iCh < con->ncomp; iCh ++){
                        dp[iCh] = (sp[iCh] >> image->shift_bits); 
                    }
                    dp += con->ncomp;
                    sp += uExternalNcomp;
                }
            } 
        } 
        else if (con->sf == 3) { /* Half float */
            for (ydx = 0 ; ydx < 16 ; ydx += 1) {
                short *sp = (short*)con->buf + ydx*block_wid + uExternalNcomp*16*mx;
                int *dp = data + con->ncomp*16*ydx;
                int iCh;
                for (xdx = 0 ; xdx < 16 ; xdx++) {
                    for (iCh = 0; iCh < con->ncomp; iCh ++) {
                        dp[iCh] = PreScalingBD16F((int)sp[iCh]); 
                    }
                    dp += con->ncomp;
                    sp += uExternalNcomp;
                }
            }
        }
    }
    else if (con->bpi == 32) { 
        /* no 32-UINT */
        if (con->sf == 2) { /* fixed point */
            for (ydx = 0 ; ydx < 16 ; ydx += 1) {
                int32_t *sp = (int32_t*)con->buf + ydx*block_wid + uExternalNcomp*16*mx;
                int *dp = data + con->ncomp*16*ydx;
                int iCh;
                for (xdx = 0 ; xdx < 16 ; xdx++) {
                    for (iCh = 0; iCh < con->ncomp; iCh ++) {
                        dp[iCh] = (sp[iCh] >> image->shift_bits); 
                    }
                    dp += con->ncomp;
                    sp += uExternalNcomp;
                }
            } 
        }
        else if (con->sf == 3) { /* float */
            for (ydx = 0 ; ydx < 16 ; ydx += 1) {
                int *sp = (int*)con->buf + ydx*block_wid + uExternalNcomp*16*mx;
                int *dp = data + con->ncomp*16*ydx;
                int iCh;
                for (xdx = 0 ; xdx < 16 ; xdx++) {
                    for (iCh = 0; iCh < con->ncomp; iCh ++) {
                        dp[iCh] = PreScalingBD32F(sp[iCh], image->exp_bias, image->len_mantissa); 
                    }
                    dp += con->ncomp;
                    sp += uExternalNcomp;
                }
            } 
        }
    }
}

void write_file_YCC(jxr_image_t image, int mx, int my, int* data)
{
    /* There are 3 or 4 buffers, depending on whether there is an alpha channel or not */
    /* Write each individual component to its own file and then concatenate(not interleave these files together */

    int *dataY = data;
    int *dataU = dataY + 256;
    int *dataV = dataU + 256;
    int *dataA = dataV + 256;
    static context *conY = NULL;
    static context *conU = NULL;
    static context *conV = NULL;
    static context *conA = NULL;
    context *con = (context*) jxr_get_user_data(image);

    
    if (con->file==0)
    {
        con->alpha = jxr_get_ALPHACHANNEL_FLAG(image);
        
        conY = (context *)malloc(sizeof(context));
        conU = (context *)malloc(sizeof(context));
        conV = (context *)malloc(sizeof(context));
        if(con->alpha)
            conA = (context *)malloc(sizeof(context));
        memcpy(conY, con, sizeof(context));
        memcpy(conU, con, sizeof(context));
        memcpy(conV, con, sizeof(context));
        if(con->alpha)
            memcpy(conA, con, sizeof(context));
        conY->name = "Y.raw";
        conU->name = "U.raw";
        conV->name = "V.raw";
        if(con->alpha)
            conA->name = "A.raw";
    
        con->left_pad = image->window_extra_left;
        con->top_pad_remaining = image->window_extra_top;
        con->top_pad = image->window_extra_top;
        
        con->ncomp = conY->ncomp = conU->ncomp = conV->ncomp =1;
        if(con->alpha)
            conA->ncomp = 1;
        start_output_file(con, jxr_get_EXTENDED_IMAGE_WIDTH(image), jxr_get_EXTENDED_IMAGE_HEIGHT(image), 
            jxr_get_IMAGE_WIDTH(image), jxr_get_IMAGE_HEIGHT(image),
            1 , jxr_get_OUTPUT_BITDEPTH(image), jxr_get_pixel_format(image));
        start_output_file(conY, jxr_get_EXTENDED_IMAGE_WIDTH(image), jxr_get_EXTENDED_IMAGE_HEIGHT(image), 
            jxr_get_IMAGE_WIDTH(image), jxr_get_IMAGE_HEIGHT(image),
            1 , jxr_get_OUTPUT_BITDEPTH(image), jxr_get_pixel_format(image));
        start_output_file(conU, jxr_get_EXTENDED_IMAGE_WIDTH(image), jxr_get_EXTENDED_IMAGE_HEIGHT(image), 
            jxr_get_IMAGE_WIDTH(image), jxr_get_IMAGE_HEIGHT(image),
            1 , jxr_get_OUTPUT_BITDEPTH(image), jxr_get_pixel_format(image));
        start_output_file(conV, jxr_get_EXTENDED_IMAGE_WIDTH(image), jxr_get_EXTENDED_IMAGE_HEIGHT(image), 
            jxr_get_IMAGE_WIDTH(image), jxr_get_IMAGE_HEIGHT(image),
            1 , jxr_get_OUTPUT_BITDEPTH(image), jxr_get_pixel_format(image));
        if(con->alpha)
            start_output_file(conA, jxr_get_EXTENDED_IMAGE_WIDTH(image), jxr_get_EXTENDED_IMAGE_HEIGHT(image), 
            jxr_get_IMAGE_WIDTH(image), jxr_get_IMAGE_HEIGHT(image),
                1 , jxr_get_OUTPUT_BITDEPTH(image), jxr_get_pixel_format(image));

    }

    int idx;
    int strip_blocks = (image->extended_width)/16;
    int dy = 16*strip_blocks;
    
    if (con->bpi == 8) {

        int xDiv = 16;
        int yDiv = 16;
        /* Y */
        uint8_t *dp = (uint8_t*)conY->buf + xDiv*mx;
        for (idx = 0; idx < xDiv*yDiv; idx += 1) {
            int dix = (idx/xDiv)*dy + (idx%xDiv);            
            dp[dix] = dataY[idx];
        }

        if(isOutputYUV422(image))
        {
            dy = 8*strip_blocks;
            xDiv = 8;
        }
        
        if(isOutputYUV420(image))
        {
            dy = 8*strip_blocks;
            xDiv = yDiv = 8;
        }

        /* U */        
        dp = (uint8_t*)conU->buf + xDiv*mx;
        for (idx = 0; idx < xDiv*yDiv; idx += 1) {
            int dix = (idx/xDiv)*dy + (idx%xDiv);            
            dp[dix] = dataU[idx];
        }
        /* V */
        dp = (uint8_t*)conV->buf + xDiv*mx;
        for (idx = 0; idx < xDiv*yDiv; idx += 1) {
            int dix = (idx/xDiv)*dy + (idx%xDiv);            
            dp[dix] = dataV[idx];
        }

        if(con->alpha)
        {
            xDiv = yDiv = 16;
            dy = 16*strip_blocks;
            dp = (uint8_t*)conA->buf + xDiv*mx;
            for (idx = 0; idx < xDiv*yDiv; idx += 1) {
                int dix = (idx/xDiv)*dy + (idx%xDiv);            
                dp[dix] = dataA[idx];
            }
        }
        
    }
    else if(con->bpi == 16 || con->bpi == 10) {

        int xDiv = 16;
        int yDiv = 16;
        /* Y */
        uint16_t *dp = (uint16_t*)conY->buf + xDiv*mx;
        for (idx = 0; idx < xDiv*yDiv; idx += 1) {
            int dix = (idx/xDiv)*dy + (idx%xDiv);            
            dp[dix] = dataY[idx];
        }

        if(isOutputYUV422(image))
        {
            dy = 8*strip_blocks;
            xDiv = 8;
        }
        
        if(isOutputYUV420(image))
        {
            assert(!"There is no 420 pixel format with bitdepth 16\n");
            dy = 8*strip_blocks;
            xDiv = yDiv = 8;
        }

        /* U */        
        dp = (uint16_t*)conU->buf + xDiv*mx;
        for (idx = 0; idx < xDiv*yDiv; idx += 1) {
            int dix = (idx/xDiv)*dy + (idx%xDiv);            
            dp[dix] = dataU[idx];
        }
        /* V */
        dp = (uint16_t*)conV->buf + xDiv*mx;
        for (idx = 0; idx < xDiv*yDiv; idx += 1) {
            int dix = (idx/xDiv)*dy + (idx%xDiv);            
            dp[dix] = dataV[idx];
        }

        if(con->alpha)
        {
            xDiv = yDiv = 16;
            dy = 16*strip_blocks;
            dp = (uint16_t*)conA->buf + xDiv*mx;
            for (idx = 0; idx < xDiv*yDiv; idx += 1) {
                int dix = (idx/xDiv)*dy + (idx%xDiv);            
                dp[dix] = dataA[idx];
            }
        }
        
    }
    else
    {
        assert(!"Unsupported bitdepth\n");
    }

    if (mx+1 == strip_blocks) {

        int xDiv = 16;
        int yDiv = 16;
        int subX = 1;
        int subY = 1;
        
        if(con->bpi == 8)
        {
            /* Y */
            int left_pad_shift = con->left_pad;
            int first = (con->top_pad_remaining > yDiv) ? yDiv : con->top_pad_remaining;
            int trans = (my*yDiv + yDiv > (con->hei + con->top_pad)) ? (con->hei + con->top_pad)%yDiv : yDiv;     
            dy = 16*strip_blocks;
            for (idx = first; idx < trans; idx += 1) {
                uint8_t *dp = (uint8_t*)conY->buf + idx*dy + con->left_pad;                
                write_uint8(conY, dp, conY->wid );
            }
            /* U */
            if(isOutputYUV422(image))
            {
                dy = 8*strip_blocks;
                xDiv = 8;
                subX = 2;
                left_pad_shift >>= 1;
            }
            
            if(isOutputYUV420(image))
            {
                dy = 8*strip_blocks;
                xDiv = yDiv = 8;
                subX = subY = 2;
                left_pad_shift >>= 1;
            }
            first = (con->top_pad_remaining > yDiv) ? yDiv : con->top_pad_remaining/subY;
            trans = (my*yDiv + yDiv > (con->hei + con->top_pad)) ? ((con->hei + con->top_pad)/subY)%yDiv : yDiv;      
            for (idx = first; idx < trans; idx += 1) {
                uint8_t *dp = (uint8_t*)conU->buf + idx*dy + left_pad_shift;                
                write_uint8(conU, dp, (conU->wid)/subX );
            }
            /* V */            
            trans = (my*yDiv + yDiv > (con->hei + con->top_pad)) ? ((con->hei + con->top_pad)/subY)%yDiv : yDiv;      
            for (idx = first; idx < trans; idx += 1) {
                uint8_t *dp = (uint8_t*)conV->buf + idx*dy + left_pad_shift;                
                write_uint8(conV, dp, (conV->wid)/subX );
            }
            /* A */
            if(con->alpha)
            {
                dy = 16*strip_blocks;
                xDiv = yDiv = 16;
                first = (con->top_pad_remaining > yDiv) ? yDiv : con->top_pad_remaining;
                trans = (my*yDiv + yDiv > (con->hei + con->top_pad)) ? (con->hei + con->top_pad)%yDiv : yDiv;      
                for (idx = first; idx < trans; idx += 1) {
                    uint8_t *dp = (uint8_t*)conA->buf + idx*dy + con->left_pad;                                
                    write_uint8(conA, dp, conA->wid);                
                }
            }
            first = (con->top_pad_remaining > 16) ? 16 : con->top_pad_remaining;
            con->top_pad_remaining -= first;
        }
        else if(con->bpi == 16 || con->bpi == 10)
        {
            /* Y */
            int left_pad_shift = con->left_pad;
            int first = (con->top_pad_remaining > yDiv) ? yDiv : con->top_pad_remaining;
            int trans = (my*yDiv + yDiv > (con->hei + con->top_pad)) ? (con->hei + con->top_pad)%yDiv : yDiv;     
            dy = 16*strip_blocks;
            for (idx = first; idx < trans; idx += 1) {
                uint16_t *dp = (uint16_t*)conY->buf + idx*dy + con->left_pad;                
                write_uint16(conY, dp, conY->wid );
            }
            /* U */
            if(isOutputYUV422(image))
            {
                dy = 8*strip_blocks;
                xDiv = 8;
                subX = 2;
                left_pad_shift >>= 1;
            }
            
            if(isOutputYUV420(image))
            {
                dy = 8*strip_blocks;
                xDiv = yDiv = 8;
                subX = subY = 2;
                left_pad_shift >>= 1;
            }
            first = (con->top_pad_remaining > yDiv) ? yDiv : con->top_pad_remaining/subY;
            trans = (my*yDiv + yDiv > (con->hei + con->top_pad)) ? ((con->hei + con->top_pad)/subY)%yDiv : yDiv;      
            for (idx = first; idx < trans; idx += 1) {
                uint16_t *dp = (uint16_t*)conU->buf + idx*dy + left_pad_shift;                
                write_uint16(conU, dp, (conU->wid)/subX );
            }
            /* V */   
            first = (con->top_pad_remaining > yDiv) ? yDiv : con->top_pad_remaining/subY;
            trans = (my*yDiv + yDiv > (con->hei + con->top_pad)) ? ((con->hei + con->top_pad)/subY)%yDiv : yDiv;      
            for (idx = first; idx < trans; idx += 1) {
                uint16_t *dp = (uint16_t*)conV->buf + idx*dy + left_pad_shift;                
                write_uint16(conV, dp, (conV->wid)/subX );
            }
            /* A */
            if(con->alpha)
            {
                dy = 16*strip_blocks;
                xDiv = yDiv = 16;
                first = (con->top_pad_remaining > yDiv) ? yDiv : con->top_pad_remaining;
                trans = (my*yDiv + yDiv >(con->hei + con->top_pad)) ? (con->hei + con->top_pad)%yDiv : yDiv;      
                for (idx = first; idx < trans; idx += 1) {
                    uint16_t *dp = (uint16_t*)conA->buf + idx*dy + con->left_pad;                                
                    write_uint16(conA, dp, conA->wid);                
                }
            }
            first = (con->top_pad_remaining > 16) ? 16 : con->top_pad_remaining;
            con->top_pad_remaining -= first;
        }      
        else
            assert(!"Unsupported bitdepth\n");
    }
    if(my*16 + 16 >=  (con->hei + con->top_pad) && mx+1 == strip_blocks)
    {
        /* End of decode */        
        long size = ftell(conY->file);  
        fseek(conY->file, 0, 0);  
        long ii;
        for(ii = 0; ii < size; ii++)
        {
            uint8_t val;
            fread(&val, 1, 1, conY->file);
            fwrite(&val, 1, 1, con->file);
        }
        size = ftell(conU->file);
        fseek(conU->file, 0, 0);        
        for( ii = 0; ii < size; ii++)
        {
            uint8_t val;
            fread(&val, 1, 1, conU->file);
            fwrite(&val, 1, 1, con->file);
        }
        size = ftell(conV->file);
        fseek(conV->file, 0, 0);
        for( ii = 0; ii < size; ii++)
        {
            uint8_t val;
            fread(&val, 1, 1, conV->file);
            fwrite(&val, 1, 1, con->file);
        }
        if(con->alpha)
        {
            size = ftell(conA->file);
            fseek(conA->file, 0, 0);            
            for( ii = 0; ii < size && con->alpha; ii++)
            {
                uint8_t val;
                fread(&val, 1, 1, conA->file);
                fwrite(&val, 1, 1, con->file);
            }
        }
        fclose(conY->file);
        fclose(conU->file);
        fclose(conV->file);
        if(con->alpha)
            fclose(conA->file);

        free(conY->buf);
        free(conU->buf);
        free(conV->buf);
        if(con->alpha)
            free(conA->buf);

        free(conY);
        free(conU);
        free(conV);
        if(con->alpha)
            free(conA);
        
        remove("Y.raw");
        remove("U.raw");
        remove("V.raw");
        if(con->alpha)
            remove("A.raw");        

    }
}

void write_file_CMYK(jxr_image_t image, int mx, int my, int* data)
{
    /* There are 4 or 5 buffers, depending on whether there is an alpha channel or not */
    /* Write each individual component to its own file and then concatenate(not interleave these files together */

    int *dataC = data;
    int *dataM = dataC + 256;
    int *dataY = dataM + 256;
    int *dataK = dataY + 256;
    int *dataA = dataK + 256;
    static context *conC = NULL;
    static context *conM = NULL;
    static context *conY = NULL;
    static context *conK = NULL;
    static context *conA = NULL;
    context *con = (context*) jxr_get_user_data(image);    
    if (con->file==0)
    {
        con->alpha = jxr_get_ALPHACHANNEL_FLAG(image);
        
        conC = (context *)malloc(sizeof(context));
        conM = (context *)malloc(sizeof(context));
        conY = (context *)malloc(sizeof(context));
        conK = (context *)malloc(sizeof(context));
        if(con->alpha)
            conA = (context *)malloc(sizeof(context));
        memcpy(conC, con, sizeof(context));
        memcpy(conM, con, sizeof(context));
        memcpy(conY, con, sizeof(context));
        memcpy(conK, con, sizeof(context));
        if(con->alpha)
            memcpy(conA, con, sizeof(context));
        conC->name = "C.raw";
        conM->name = "M.raw";
        conY->name = "Y.raw";
        conK->name = "K.raw";
        if(con->alpha)
            conA->name = "A.raw";
    
        con->left_pad = image->window_extra_left;
        con->top_pad_remaining = image->window_extra_top;
        con->top_pad = image->window_extra_top;        
        
        con->ncomp = conC->ncomp = conM->ncomp = conY->ncomp = conK->ncomp = 1;
        if(con->alpha)
            conA->ncomp = 1;
        start_output_file(con, jxr_get_EXTENDED_IMAGE_WIDTH(image), jxr_get_EXTENDED_IMAGE_HEIGHT(image), 
            jxr_get_IMAGE_WIDTH(image), jxr_get_IMAGE_HEIGHT(image),
            1 , jxr_get_OUTPUT_BITDEPTH(image), jxr_get_pixel_format(image));
        start_output_file(conC, jxr_get_EXTENDED_IMAGE_WIDTH(image), jxr_get_EXTENDED_IMAGE_HEIGHT(image), 
            jxr_get_IMAGE_WIDTH(image), jxr_get_IMAGE_HEIGHT(image),
            1 , jxr_get_OUTPUT_BITDEPTH(image), jxr_get_pixel_format(image));
        start_output_file(conM, jxr_get_EXTENDED_IMAGE_WIDTH(image), jxr_get_EXTENDED_IMAGE_HEIGHT(image), 
            jxr_get_IMAGE_WIDTH(image), jxr_get_IMAGE_HEIGHT(image),
            1 , jxr_get_OUTPUT_BITDEPTH(image), jxr_get_pixel_format(image));
        start_output_file(conY, jxr_get_EXTENDED_IMAGE_WIDTH(image), jxr_get_EXTENDED_IMAGE_HEIGHT(image), 
            jxr_get_IMAGE_WIDTH(image), jxr_get_IMAGE_HEIGHT(image),
            1 , jxr_get_OUTPUT_BITDEPTH(image), jxr_get_pixel_format(image));
        start_output_file(conK, jxr_get_EXTENDED_IMAGE_WIDTH(image), jxr_get_EXTENDED_IMAGE_HEIGHT(image), 
            jxr_get_IMAGE_WIDTH(image), jxr_get_IMAGE_HEIGHT(image),
            1 , jxr_get_OUTPUT_BITDEPTH(image), jxr_get_pixel_format(image));
        
        if(con->alpha)
            start_output_file(conA, jxr_get_EXTENDED_IMAGE_WIDTH(image), jxr_get_EXTENDED_IMAGE_HEIGHT(image), 
                jxr_get_IMAGE_WIDTH(image), jxr_get_IMAGE_HEIGHT(image),
                1 , jxr_get_OUTPUT_BITDEPTH(image),jxr_get_pixel_format(image));

    }

    int idx;
    int strip_blocks = (image->extended_width)/16;
    int dy = 16*strip_blocks;
    
    if (con->bpi == 8) {

        /* C */
        uint8_t *dp = (uint8_t*)conC->buf + 16*mx;
        for (idx = 0; idx < 256; idx += 1) {
            int dix = (idx/16)*dy + (idx%16);            
            dp[dix] = dataC[idx];
        }

        /* M */        
        dp = (uint8_t*)conM->buf + 16*mx;
        for (idx = 0; idx < 256; idx += 1) {
            int dix = (idx/16)*dy + (idx%16);            
            dp[dix] = dataM[idx];
        }
        /* Y */
        dp = (uint8_t*)conY->buf + 16*mx;
        for (idx = 0; idx < 256; idx += 1) {
            int dix = (idx/16)*dy + (idx%16);            
            dp[dix] = dataY[idx];
        }
         /* K */
        dp = (uint8_t*)conK->buf + 16*mx;
        for (idx = 0; idx < 256; idx += 1) {
            int dix = (idx/16)*dy + (idx%16);            
            dp[dix] = dataK[idx];
        }


        if(con->alpha)
        {
            dp = (uint8_t*)conA->buf + 16*mx;
            for (idx = 0; idx < 256; idx += 1) {
                int dix = (idx/16)*dy + (idx%16);            
                dp[dix] = dataA[idx];
            }
        }
        
    }
    else if(con->bpi == 16 || con->bpi == 10) {

        /* C */
        uint16_t *dp = (uint16_t*)conC->buf + 16*mx;
        for (idx = 0; idx < 256; idx += 1) {
            int dix = (idx/16)*dy + (idx%16);            
            dp[dix] = dataC[idx];
        }
       
        /* M */        
        dp = (uint16_t*)conM->buf + 16*mx;
        for (idx = 0; idx < 256; idx += 1) {
            int dix = (idx/16)*dy + (idx%16);            
            dp[dix] = dataM[idx];
        }
        /* Y */
        dp = (uint16_t*)conY->buf + 16*mx;
        for (idx = 0; idx < 256; idx += 1) {
            int dix = (idx/16)*dy + (idx%16);            
            dp[dix] = dataY[idx];
        }
        /* K */
        dp = (uint16_t*)conK->buf + 16*mx;
        for (idx = 0; idx < 256; idx += 1) {
            int dix = (idx/16)*dy + (idx%16);            
            dp[dix] = dataK[idx];
        }

        /* A */
        if(con->alpha)
        {
            dp = (uint16_t*)conA->buf + 16*mx;
            for (idx = 0; idx < 256; idx += 1) {
                int dix = (idx/16)*dy + (idx%16);            
                dp[dix] = dataA[idx];
            }
        }        
    }
    else
    {
        assert(!"Unsupported bitdepth\n");
    }

    if (mx+1 == strip_blocks) {
        
        if(con->bpi == 8)
        {          
            int first = (con->top_pad_remaining > 16) ? 16 : con->top_pad_remaining;
            int trans = (my*16 + 16 > (con->hei + con->top_pad)) ? (con->hei + con->top_pad)%16 : 16;     

            dy = 16*strip_blocks;
            for (idx = first; idx < trans; idx += 1) {
                uint8_t *dp = (uint8_t*)conC->buf + idx*dy + con->left_pad;                
                write_uint8(conC, dp, conC->wid );
            }           
            for (idx = first; idx < trans; idx += 1) {
                uint8_t *dp = (uint8_t*)conM->buf + idx*dy + con->left_pad;                
                write_uint8(conM, dp, (conM->wid));
            }            
            for (idx = first; idx < trans; idx += 1) {
                uint8_t *dp = (uint8_t*)conY->buf + idx*dy + con->left_pad;                
                write_uint8(conY, dp, (conY->wid));
            }
            for (idx = first; idx < trans; idx += 1) {
                uint8_t *dp = (uint8_t*)conK->buf + idx*dy + con->left_pad;                
                write_uint8(conK, dp, (conK->wid));
            }

            if(con->alpha)
            {                
                for (idx = first; idx < trans; idx += 1) {
                    uint8_t *dp = (uint8_t*)conA->buf + idx*dy + con->left_pad;                                
                    write_uint8(conA, dp, conA->wid);                
                }
            }            
            con->top_pad_remaining -= first;
        }
        else if(con->bpi == 16 || con->bpi == 10)
        {   
            int first = (con->top_pad_remaining > 16) ? 16 : con->top_pad_remaining;
            int trans = (my*16 + 16 > (con->hei + con->top_pad)) ? (con->hei + con->top_pad)%16 : 16;     
            dy = 16*strip_blocks;
            for (idx = first; idx < trans; idx += 1) {
                uint16_t *dp = (uint16_t*)conC->buf + idx*dy + con->left_pad;                
                write_uint16(conC, dp, conC->wid );
            }
            
            for (idx = first; idx < trans; idx += 1) {
                uint16_t *dp = (uint16_t*)conM->buf + idx*dy + con->left_pad;                
                write_uint16(conM, dp, (conM->wid) );
            }
                       
            for (idx = first; idx < trans; idx += 1) {
                uint16_t *dp = (uint16_t*)conY->buf + idx*dy + con->left_pad;                
                write_uint16(conY, dp, (conY->wid) );
            }            

            for (idx = first; idx < trans; idx += 1) {
                uint16_t *dp = (uint16_t*)conK->buf + idx*dy + con->left_pad;                
                write_uint16(conK, dp, (conK->wid) );
            }
            
            if(con->alpha)
            {                
                for (idx = first; idx < trans; idx += 1) {
                    uint16_t *dp = (uint16_t*)conA->buf + idx*dy + con->left_pad;                                
                    write_uint16(conA, dp, conA->wid);                
                }
            }            
            con->top_pad_remaining -= first;
        }      
        else
            assert(!"Unsupported bitdepth\n");
    }
    if(my*16 + 16 >=  (con->hei + con->top_pad) && mx+1 == strip_blocks)
    {
        /* End of decode */        
        long size = ftell(conC->file);  
        long ii;
        fseek(conC->file, 0, 0); 
        for( ii = 0; ii < size; ii++)
        {
            uint8_t val;
            fread(&val, 1, 1, conC->file);
            fwrite(&val, 1, 1, con->file);
        }
        size = ftell(conM->file);
        fseek(conM->file, 0, 0);        
        for( ii = 0; ii < size; ii++)
        {
            uint8_t val;
            fread(&val, 1, 1, conM->file);
            fwrite(&val, 1, 1, con->file);
        }
        size = ftell(conY->file);
        fseek(conY->file, 0, 0);
        for( ii = 0; ii < size; ii++)
        {
            uint8_t val;
            fread(&val, 1, 1, conY->file);
            fwrite(&val, 1, 1, con->file);
        }
        size = ftell(conK->file);
        fseek(conK->file, 0, 0);
        for( ii = 0; ii < size; ii++)
        {
            uint8_t val;
            fread(&val, 1, 1, conK->file);
            fwrite(&val, 1, 1, con->file);
        }
        if(con->alpha)
        {
            size = ftell(conA->file);
            fseek(conA->file, 0, 0);            
            for( ii = 0; ii < size && con->alpha; ii++)
            {
                uint8_t val;
                fread(&val, 1, 1, conA->file);
                fwrite(&val, 1, 1, con->file);
            }
        }
        fclose(conC->file);
        fclose(conM->file);
        fclose(conY->file);
        fclose(conK->file);
        if(con->alpha)
            fclose(conA->file);

        free(conC->buf);
        free(conM->buf);
        free(conY->buf);
        free(conK->buf);
        if(con->alpha)
            free(conA->buf);

        free(conC);
        free(conM);
        free(conY);
        free(conK);
        if(con->alpha)
            free(conA);
        
        remove("C.raw");
        remove("M.raw");
        remove("Y.raw");
        remove("K.raw");
        if(con->alpha)
            remove("A.raw");
    }
}

void write_file(jxr_image_t image, int mx, int my, int*data)
{
    context *con = (context*) jxr_get_user_data(image);
    
    if((isOutputYUV444(image) || isOutputYUV422(image) || isOutputYUV420(image)) && jxr_get_IMAGE_CHANNELS(image) == 3)
    {
        /* Use special YCC format only for primary image */
        write_file_YCC(image, mx, my, data);
        return;
    }
    else if(isOutputCMYKDirect(image) && jxr_get_IMAGE_CHANNELS(image) == 4)
    {
        /* Use special YCC format only for primary image */
        write_file_CMYK(image, mx, my, data);
        return;
    }

    if (con->file==0)
    {
        int channels = jxr_get_IMAGE_CHANNELS(image);
        if( channels == 3)
            set_bgr_flag(con, image);
        else
            con->isBgr = 0;

        if(jxr_get_pixel_format(image) == JXRC_FMT_32bppRGBE)
        {           
            channels++;
        }

        set_pad_bytes(con, image);

        con->left_pad = image->window_extra_left;
        con->top_pad_remaining = image->window_extra_top;
        con->top_pad = image->window_extra_top;

        con->alpha = jxr_get_ALPHACHANNEL_FLAG(image);
        if (con->alpha)  /* with alpha channel */
            channels ++;

        start_output_file(con, jxr_get_EXTENDED_IMAGE_WIDTH(image), jxr_get_EXTENDED_IMAGE_HEIGHT(image),
             jxr_get_IMAGE_WIDTH(image), jxr_get_IMAGE_HEIGHT(image),
             channels, jxr_get_OUTPUT_BITDEPTH(image), jxr_get_pixel_format(image));
    }

    int idx, jdx;
    int strip_blocks = (image->extended_width)/16;
    int dy = 16*con->ncomp*strip_blocks;
    if(con->padBytes !=0)
        dy = dy+16*strip_blocks;
    if (con->bpi == 1 || con->bpi == 5 || con->bpi == 6 || con->bpi == 8) {
        uint8_t *dp = (uint8_t*)con->buf + 16*con->ncomp*mx;
        if(con->padBytes != 0)
        {
            dp = dp + 16*mx; /* Add padding channel offset */
        }
        for (idx = 0; idx < 256; idx += 1) {
            int dix = (idx/16)*dy + (idx%16)*con->ncomp;
            if(con->padBytes)
                dix +=(idx%16);
            int six = idx*con->ncomp;
            int pl;
            for (pl = 0 ; pl < con->ncomp ; pl += 1)
                dp[dix + pl] = data[six + pl];
            if(con->padBytes != 0)
                dp[dix + pl] = 0; /* Padding data after all n channels */
            if(con->isBgr)            
                switch_r_b(dp+dix, con->bpi);            
        }
    }    
    else if(con->bpi == 10 || con->bpi == 16){
        uint16_t *dp = (uint16_t*)con->buf + 16*con->ncomp*mx;
        if(con->padBytes != 0)
        {
            dp = dp + 16*mx; /* Add padding channel offset */
        }

        for (idx = 0; idx < 256; idx += 1) {
            int dix = (idx/16)*dy + (idx%16)*con->ncomp;
            if(con->padBytes)
                dix +=(idx%16);
            int six = idx*con->ncomp;
            int pl;            
            for (pl = 0 ; pl < con->ncomp ; pl += 1)
                dp[dix + pl] = data[six + pl];
            if(con->padBytes != 0)
                dp[dix + pl] = 0; /* Padding data after all n channels */
            if(con->isBgr)            
                switch_r_b(dp + dix, con->bpi);            
        }
    }
    else if(con->bpi ==32) {
        uint32_t *dp = (uint32_t*)con->buf + 16*con->ncomp*mx;
        if(con->padBytes != 0)
        {
            dp = dp + 16*mx; /* Add padding channel offset */
        }
        for (idx = 0; idx < 256; idx += 1) {
            int dix = (idx/16)*dy + (idx%16)*con->ncomp;
            if(con->padBytes)
                dix +=(idx%16);
            int six = idx*con->ncomp;
            int pl;
            for (pl = 0 ; pl < con->ncomp ; pl += 1)
                dp[dix + pl] = data[six + pl];
            if(con->padBytes != 0)
                dp[dix + pl] = 0; /* Padding data after all n channels */
            if(con->isBgr)            
                switch_r_b(dp + dix,  con->bpi);
        }
    }
    else
    {
        assert(!"Unsupported bitdepth\n");
    }

    if (mx+1 == strip_blocks) {
        int first = (con->top_pad_remaining > 16) ? 16 : con->top_pad_remaining;
        con->top_pad_remaining -= first; /* skip the padding rows */
        int trans = (my*16 + 16 > (con->hei + con->top_pad)) ? (con->hei + con->top_pad)%16 : 16;
        if(con->bpi == 1)
        {         
            /* ClipAndPackBD1BorW */
            /* Clipping is already handled in scale_and_emit_top, so just pack */
            for (idx = first; idx < trans; idx += 1) {
                uint8_t *dp = (uint8_t*)con->buf + idx*dy;
                for(jdx = con->left_pad; jdx < (con->wid + con->left_pad); jdx = jdx + 8)
                {
                    uint8_t buff[8];
                    uint8_t iResult = 0;
                    int end = jdx + 8 > con->wid? con->wid:jdx + 8;
                    memset(buff, 0, 8 * sizeof(uint8_t));
                    memcpy(buff, dp+jdx, (end-jdx) * sizeof(uint8_t));
                    if (jxr_get_OUTPUT_BITDEPTH(image) == JXR_BD1BLACK1)
                    {                      
                        iResult = (1-buff[7]) + ((1 - buff[6]) << 1) + ((1 - buff[5]) << 2) + 
                            ((1 - buff[4]) << 3) + ((1 - buff[3]) << 4) + ((1 - buff[2]) << 5) + 
                            ((1 - buff[1]) << 6) + ((1 - buff[0]) << 7);
                    }
                    else/* jxr_output_bitdepth(image) = = BD1WHITE1 */
                    {
                        iResult = buff[7] + (buff[6] << 1) + (buff[5] << 2) + 
                            (buff[4] << 3) + (buff[3] << 4) + (buff[2] << 5) + 
                            (buff[1] << 6) + (buff[0] << 7);                 
                    }

                    write_uint8(con, &iResult,1 );
                }
            }
        }
        else if(con->bpi == 5)
        {
            /* ClipAndPack555 */
            /* Clipping is already handled in scale_and_emit_top, so just pack */
            for (idx = first; idx < trans; idx += 1) {
                uint8_t *dp = (uint8_t*)con->buf + idx*dy;
                assert(con->ncomp == 3);                
                for(jdx = con->left_pad*con->ncomp; jdx < (con->wid + con->left_pad)*con->ncomp; jdx = jdx + con->ncomp)
                {
                    uint16_t iResult = 0;
                    iResult = (uint16_t)dp[jdx] + (((uint16_t)dp[jdx + 1])<<5)  + (((uint16_t)dp[jdx + 2])<<10);
                    write_uint16(con, &iResult,1 );                    
                }
            }            
        }
        else if(con->bpi == 6)
        {
            /* ClipAndPack565 */
            /* Clipping is already handled in scale_and_emit_top, so just pack */
            for (idx = first; idx < trans; idx += 1) {
                uint8_t *dp = (uint8_t*)con->buf + idx*dy;
                assert(con->ncomp == 3);                
                for(jdx = con->left_pad*con->ncomp; jdx < (con->wid + con->left_pad)*con->ncomp; jdx = jdx + con->ncomp)
                {
                    uint16_t iResult = 0;
                    iResult = (uint16_t)dp[jdx] + (((uint16_t)dp[jdx + 1])<<5)  + (((uint16_t)dp[jdx + 2])<<11);
                    write_uint16(con, &iResult,1 );                    
                }
            }            
        }
        else if (con->bpi==8)
            for (idx = first; idx < trans; idx += 1) {
                uint8_t *dp = (uint8_t*)con->buf + idx*dy + con->left_pad*con->ncomp;
                int padComp = con->ncomp;
                if(con->padBytes != 0 )
                    padComp ++;
                write_uint8(con, dp, con->wid*padComp); }
        else if(con->bpi == 10 && con->ncomp == 3)
        {
            /* ClipAndPack10 */
            /* Clipping is already handled in scale_and_emit_top, so just pack */
            for (idx = first; idx < trans; idx += 1) {
                uint16_t *dp = (uint16_t*)con->buf + idx*dy;
                for(jdx = con->left_pad*con->ncomp; jdx < (con->wid + con->left_pad)*con->ncomp; jdx = jdx + con->ncomp)
                {
                    uint32_t iResult = 0;
                    iResult = (uint32_t)dp[jdx] + (((uint32_t)dp[jdx + 1])<<10)  + (((uint32_t)dp[jdx + 2])<<20);
                    write_uint32(con, &iResult,1 );                    
                }
            }            
        }
        else if(con->bpi == 10 && con->ncomp == 1)/*Alpha image decoding for JXR_30bppYCC422Alpha JXR_40bppYCC4444Alpha */
        {
            /* ClipAndPack10 */
            /* Clipping is already handled in scale_and_emit_top, so just pack */          
            for (idx = 0; idx < trans; idx += 1) {
                uint16_t *dp = (uint16_t*)con->buf + idx*dy;                
                for(jdx = con->left_pad*con->ncomp; jdx < (con->wid + con->left_pad)*con->ncomp; jdx = jdx + con->ncomp)
                {
                    uint16_t iResult = 0;
                    iResult = (uint16_t)dp[jdx];
                    write_uint16(con, &iResult,1 );                    
                }
            }            
        }
        else if(con->bpi==16)
            for (idx = first; idx < trans; idx += 1) {
                uint16_t *dp = (uint16_t*)con->buf + idx*dy + con->left_pad*con->ncomp; /* dy already contains offset for padding data */
                int padComp = con->ncomp;
                if(con->padBytes != 0 )
                    padComp ++;
                write_uint16(con, dp, con->wid*padComp );
            }
        else if(con->bpi == 32)
            for (idx = first; idx < trans; idx += 1) {
                uint32_t *dp = (uint32_t*)con->buf + idx*dy + con->left_pad*con->ncomp;
                int padComp = con->ncomp;
                if(con->padBytes != 0 )
                    padComp ++;
                write_uint32(con, dp, con->wid*padComp );
            }
        else
            assert(!"Unsupported bitdepth\n");
    }
}


void concatenate_primary_alpha(jxr_image_t image, FILE *fpPrimary, FILE *fpAlpha)
{
    context *con = (context*) jxr_get_user_data(image);
    if (con->file==0)
    {
        set_pad_bytes(con, image);
        /* Add 1 to number of channels for alpha */
        con->alpha = 1;        
        start_output_file(con, jxr_get_EXTENDED_IMAGE_WIDTH(image), jxr_get_EXTENDED_IMAGE_HEIGHT(image), 
            jxr_get_IMAGE_WIDTH(image), jxr_get_IMAGE_HEIGHT(image),
            jxr_get_IMAGE_CHANNELS(image) + 1, jxr_get_OUTPUT_BITDEPTH(image), jxr_get_pixel_format(image));
    }
    fseek(fpPrimary, 0, SEEK_END);
    long size = ftell(fpPrimary);
    fseek(fpPrimary, 0, SEEK_SET);
    long i;
    for(i = 0; i < size; i++)
    {
        uint8_t val;
        fread(&val, 1, 1, fpPrimary);
        fwrite(&val, 1, 1, con->file);
    }
    fseek(fpAlpha, 0, SEEK_END);
    size = ftell(fpAlpha);
    fseek(fpAlpha, 0, SEEK_SET);
    for(i = 0; i < size; i++)
    {
        uint8_t val;
        fread(&val, 1, 1, fpAlpha);
        fwrite(&val, 1, 1, con->file);
    } 
}



void write_file_combine_primary_alpha(jxr_image_t image, FILE *fpPrimary, FILE *fpAlpha)
{
    context *con = (context*) jxr_get_user_data(image);
    int i;

    if(isOutputYUV444(image) || isOutputYUV422(image) || isOutputYUV420(image) || isOutputCMYKDirect(image))
    {
        concatenate_primary_alpha(image, fpPrimary, fpAlpha);
        return;
    }
    if (con->file==0)
    {
        set_pad_bytes(con, image);
        /* Add 1 to number of channels for alpha */
        con->alpha = 1;        
        start_output_file(con, jxr_get_EXTENDED_IMAGE_WIDTH(image), jxr_get_EXTENDED_IMAGE_HEIGHT(image), 
            jxr_get_IMAGE_WIDTH(image), jxr_get_IMAGE_HEIGHT(image),
            jxr_get_IMAGE_CHANNELS(image) + 1, jxr_get_OUTPUT_BITDEPTH(image), jxr_get_pixel_format(image));
    }
    int numPixels = jxr_get_IMAGE_WIDTH(image) * jxr_get_IMAGE_HEIGHT(image);
    int nPrimaryComp = jxr_get_IMAGE_CHANNELS(image);
    if(con->padBytes)
        nPrimaryComp ++; 
    if (con->bpi == 8) {
        for (i=0; i<numPixels; i++) {
            unsigned char combine[MAX_CHANNELS + 1 + 1];/* +1 for alpha + 1 for padded channel */
            fread(&combine[0],sizeof(char),nPrimaryComp, fpPrimary);
            fread(&(combine[nPrimaryComp]),sizeof(char), 1,fpAlpha);
            write_uint8(con, combine, nPrimaryComp+1);       
        }
    }
    else if(con->bpi == 16)
    {
        for (i=0; i<numPixels; i++) {
            uint16_t combine[MAX_CHANNELS + 1 + 1];/* +1 for alpha + 1 for padded channel */
            fread(&combine[0],sizeof(uint16_t),nPrimaryComp, fpPrimary);
            fread(&(combine[nPrimaryComp]),sizeof(uint16_t), 1,fpAlpha);
            write_uint16(con, combine, nPrimaryComp+1);       
        }
    }
    else if(con->bpi == 32)
    {
        for (i=0; i<numPixels; i++) {
            uint32_t combine[MAX_CHANNELS + 1 + 1];/* +1 for alpha + 1 for padded channel */
            fread(&combine[0],sizeof(uint32_t),nPrimaryComp, fpPrimary);
            fread(&(combine[nPrimaryComp]),sizeof(uint32_t), 1,fpAlpha);
            write_uint32(con, combine,nPrimaryComp+1);       
        }
    }
    else
        assert(!"Unsupported bitdepth\n");
}


void split_primary_alpha(jxr_image_t image,void *input_handle, context *con_primary, context *con_alpha, jxr_container_t container)
{
    /* Used for YCCA and CMYKA */
    int wid, hei, ncomp, bpi;
    short sf, photometric;
    int padBytes;

    get_file_parameters(input_handle, &wid, &hei, &ncomp, &bpi, &sf, &photometric, &padBytes);
    int numPixels = wid * hei;
    int nPrimaryComp = ncomp-1;
    context *con = (context *)input_handle;
    read_setup(con);

    int i;

    start_output_file(con_primary, wid, hei, wid, hei, con_primary->ncomp, jxr_get_OUTPUT_BITDEPTH(image), jxrc_get_pixel_format(container));
    start_output_file(con_alpha, wid, hei, wid, hei, con_alpha->ncomp, jxr_get_OUTPUT_BITDEPTH(image), jxrc_get_pixel_format(container));

    if(isOutputYUV444(image) || isOutputYUV422(image) || isOutputYUV420(image) || isOutputCMYKDirect(image))
    {
        //16 * (con->ncomp + con->padBytes) * ((con->bpi+7)/8) * ((con->wid+15)&~15);
        unsigned bytes = (con->bpi+7)/8;
        unsigned size_luma = numPixels * bytes;
        unsigned size_chroma = size_luma;
        if (isOutputYUV422(image))
            size_chroma >>= 1;
        if (isOutputYUV420(image))
            size_chroma >>= 2;

        uint8_t * combine = 0;
        combine = (uint8_t*)malloc(size_luma);
        assert(combine != 0);

        read_uint8(con, combine, size_luma);
        write_uint8(con_primary, combine, size_luma);

        for (i = 1; i < nPrimaryComp; i++) {
            read_uint8(con, combine, size_chroma);
            write_uint8(con_primary, combine, size_chroma);
        }

        read_uint8(con, combine, size_luma);
        write_uint8(con_alpha, combine, size_luma);

        free(combine);
    }
    else 
    {
        for (i=0; i<numPixels; i++)
        {
            if (con->bpi == 8) {

                unsigned char combine[MAX_CHANNELS + 1];/*+ 1 for padded channel */
                read_uint8(con, &combine[0],nPrimaryComp);
                write_uint8(con_primary, combine, nPrimaryComp);

            }
            else if(con->bpi == 16)
            {

                uint16_t combine[MAX_CHANNELS + 1];/*+ 1 for padded channel */
                read_uint16(con, &combine[0],nPrimaryComp);
                write_uint16(con_primary, combine, nPrimaryComp);

            }
            else if(con->bpi == 32)
            {

                uint32_t combine[MAX_CHANNELS + 1];/*+ 1 for padded channel */
                read_uint32(con, &combine[0],nPrimaryComp);
                write_uint32(con_primary, combine, nPrimaryComp);

            }
            else
                assert(!"Unsupported bitdepth\n");
        }
        for (i=0; i<numPixels; i++)
        {
            if (con->bpi == 8) {

                unsigned char combine[1];
                read_uint8(con, &combine[0],1);
                write_uint8(con_alpha, combine, 1);

            }
            else if(con->bpi == 16)
            {

                uint16_t combine[1];
                read_uint16(con, &combine[0],1);
                write_uint16(con_alpha, combine, 1);

            }
            else if(con->bpi == 32)
            {

                uint32_t combine[1];
                read_uint32(con, &combine[0],1);
                write_uint32(con_alpha, combine, 1);

            }
            else
                assert(!"Unsupported bitdepth\n");
        }
    }
    close_file(con_primary);
    close_file(con_alpha);
}


void separate_primary_alpha(jxr_image_t image, void *input_handle, char *path_out, char * path_primary, char *path_alpha, jxr_container_t container)
{

    context *con;

    int i;
    int wid, hei, ncomp, bpi;
    short sf, photometric;
    int padBytes;

    con = (context *)input_handle;
    read_setup(con);


    context *con_primary = (context *)malloc(sizeof(context));
    assert(con_primary != NULL);
    context *con_alpha = (context *)malloc(sizeof(context));
    assert(con_alpha != NULL);

    memcpy(con_primary, con, sizeof(*con));
    memcpy(con_alpha, con, sizeof(*con));
    con_primary->alpha = con_alpha->alpha = 0;
    con_primary->buf = con_alpha->buf = 0;
    con_primary->ncomp = con->ncomp - 1;
    con_alpha->ncomp = 1;

    get_file_parameters(con, &wid, &hei, &ncomp, &bpi, &sf, &photometric, &padBytes);

    const char *p = strrchr(con->name, '.');
    if (p==0)
        error("output file name %s needs a suffix to determine its format", con->name);
    if (!strcasecmp(p, ".pnm") || !strcasecmp(p, ".pgm") || !strcasecmp(p, ".ppm"))
    {
        error("Alpha channel not supported by PNM, PGM and PPM");
    }
    else if (!strcasecmp(p, ".tif"))
    {
        strcpy(path_primary, path_out);
        strcat(path_primary, "_input_primary.tif");
        strcpy(path_alpha, path_out);
        strcat(path_alpha, "_input_alpha.tif");

    }
    else if (!strcasecmp(p, ".raw"))
    {
        strcpy(path_primary, path_out);
        strcat(path_primary, "_input_primary.raw");
        strcpy(path_alpha, path_out);
        strcat(path_alpha, "_input_alpha.raw");
    }
    else error("unrecognized suffix on output file name %s", con->name);

    con_primary->name = path_primary;
    con_alpha->name = path_alpha;


    if(isOutputYUV444(image) || isOutputYUV422(image) || isOutputYUV420(image) || isOutputCMYKDirect(image))
    {
        split_primary_alpha(image, input_handle,con_primary, con_alpha, container);
        return;
    }

    start_output_file(con_primary, wid, hei, wid, hei, con_primary->ncomp, jxr_get_OUTPUT_BITDEPTH(image), jxrc_get_pixel_format(container));
    start_output_file(con_alpha, wid, hei, wid, hei, con_alpha->ncomp, jxr_get_OUTPUT_BITDEPTH(image), jxrc_get_pixel_format(container));

    int numPixels = wid * hei;
    int nPrimaryComp = ncomp-1;

    if (con->bpi == 8) {
        for (i=0; i<numPixels; i++) {
            unsigned char combine[MAX_CHANNELS + 1 + 1];/* +1 for alpha + 1 for padded channel */
            read_uint8(con, &combine[0],ncomp);
            write_uint8(con_primary, combine, nPrimaryComp);
            write_uint8(con_alpha, &(combine[nPrimaryComp]), 1);

        }
    }
    else if(con->bpi == 16)
    {
        for (i=0; i<numPixels; i++) {
            uint16_t combine[MAX_CHANNELS + 1 + 1];/* +1 for alpha + 1 for padded channel */
            read_uint16(con, &combine[0],ncomp);
            write_uint16(con_primary, combine, nPrimaryComp);
            write_uint16(con_alpha, &(combine[nPrimaryComp]), 1);
        }
    }
    else if(con->bpi == 32)
    {
        for (i=0; i<numPixels; i++) {
            uint32_t combine[MAX_CHANNELS + 1 + 1];/* +1 for alpha + 1 for padded channel */
            read_uint32(con, &combine[0],ncomp);
            write_uint32(con_primary, combine, nPrimaryComp);
            write_uint32(con_alpha, &(combine[nPrimaryComp]), 1);
        }
    }
    else
        assert(!"Unsupported bitdepth\n");
    close_file(con_primary);
    close_file(con_alpha);
}

/*
* $Log: file.c,v $
* Revision 1.17 2009/05/29 12:00:00 microsoft
* Reference Software v1.6 updates.
*
* Revision 1.16 2009/04/13 12:00:00 microsoft
* Reference Software v1.5 updates.
*
* Revision 1.15 2008/03/17 23:34:54 steve
* Support output of CMYK TIFF images.
*
* Revision 1.14 2008/03/06 09:33:47 rick
* Add support for reading multi-strip TIFF files; fix TIFF reader bug.
*
* Revision 1.13 2008/03/05 19:32:02 gus
* *** empty log message ***
*
* Revision 1.12 2008/03/05 06:58:10 gus
* *** empty log message ***
*
* Revision 1.11 2008/03/03 03:16:18 rick
* Re-implement BDx to bpi mapping.
*
* Revision 1.10 2008/03/03 01:57:34 steve
* Fix BDx to bpi mapping.
*
* Revision 1.9 2008/03/03 01:51:40 rick
* Allow output file depths other than 8.
*
* Revision 1.8 2008/02/29 01:03:31 steve
* MSC doesnt have strcasecmp. Use stricmp instead.
*
* Revision 1.7 2008/02/29 00:45:21 steve
* Portability, esp w/ C++ compilers.
*
* Revision 1.6 2008/02/27 06:15:49 rick
* Replace macro ASSERT, which had optional arguments.
*
* Revision 1.5 2008/02/26 23:52:44 steve
* Remove ident for MS compilers.
*
* Revision 1.4 2008/02/26 23:44:25 steve
* Handle the special case of compiling via MS C.
*
* Revision 1.3 2008/02/23 08:45:07 rick
* Changed some types to avoid compiler warnings.
*
* Revision 1.2 2008/01/31 23:20:14 rick
* Fix bug in read_file() affecting color input images.
*
* Revision 1.1 2008/01/19 02:30:46 rick
* Re-implement and extend file interface.
*
*/

