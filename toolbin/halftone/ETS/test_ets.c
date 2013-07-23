#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "ets.h"

#define noDUMP_FILE_NAME "dump.ebdp"

#define LITTLE_ENDIAN /* Needed for PSD support */
//#define TEST_PSD_DUMP 

/* PSD structure.  Since this data can be compressed planar data we need an object
   to maintain our context state. */
struct psd_ctx_s {
    int width;
    int height;
    int num_channels; 
    int depth;
    int curr_row;
    void *rle_row;
    int *row_lengths;
    int *band_row_length_index;
    long *band_file_offset;
    void *output_buffer;
    void (*read_line)(ETS_SrcPixel **ibufs, int xs, FILE *fi, int planes, void *image_ctx);
    void (*write_line)(uchar **obufs, int xs, FILE *fo, int planes, void *image_ctx);
    void (*finalize)(void *image_ctx);
    uchar *permute;
};

typedef struct psd_ctx_s psd_ctx_t;

static void
die(char *why)
{
    fprintf(stderr, "%s\n", why);
    exit(1);
}

#define MAX_SIZE 65536

#define M 16

static const char * get_arg (int argc, char **argv, int *pi, const char *arg)
{
    if (arg[0] != 0) {
        return arg;
    } else {
        (*pi)++;
        if (*pi == argc) {
            return NULL;
        } else {
            return argv[*pi];
        }
    }
}

static int usage (void)
{
    printf("Usage: ETS\n");
    printf("ETS <options>\n");
    printf("    -i <inputfile      .pam or .pgm - omit for stdin\n");
    printf("    -o <outputfile>    same format as input - omit for stdout\n");
    printf("    -g <gammafile>     optional textual table of gamma values\n");
    printf("    -e <style>         ets style (0 off, 1 = normal, etc)\n");
    printf("    -r <style>         random noise style (0 off, 1 = rnd, 2 = tm\n");
    printf("    -m <0 or 1>        enable/disable multiplane optimisations\n");
    printf("    -l <levels>        number of levels in the output\n");
    printf("    -a X:Y             aspect ratio of input\n");
    printf("    -n <noise level>   noise level (0 to 8)\n");
    printf("\n\nDefaults: -e1 -r1 -m1 -l2 -a1:1 -n0\n");
    return 1;
}

static void get4(int *value, uchar *buf)
{
#ifdef LITTLE_ENDIAN
    *value = buf[3] + (buf[2] << 8) + (buf[1] << 16) + (buf[0] << 24);
#else
    *value = buf[0] + (buf[1] << 8) + (buf[2] << 16) + (buf[3] << 24);
#endif
}

static void get2(int *value, uchar *buf)
{
#ifdef LITTLE_ENDIAN
    *value = buf[1] + (buf[0] << 8);
#else
    *value = buf[0] + (buf[1] << 8);
#endif
}

/* PSD uses PackBits RLE */
static void rle_decode(uchar *in_buff, int in_length, 
                       uchar *out_buff, int out_length)
{
    signed char hvalue, *input_buf;
    char value;
    int in_pos = 0, out_pos = 0;

    input_buf = (signed char*) in_buff;
    while (out_pos < out_length) 
    {
        hvalue = input_buf[in_pos];
        if (hvalue >= 0) 
        {
            /* 1+hvalue literal bytes follow */
            memcpy(&(out_buff[out_pos]), &(in_buff[in_pos + 1]), (1 + hvalue));
            in_pos += (1 + (1 + hvalue));
            out_pos += (1 + hvalue);
        } 
        else if (hvalue > -128)
        {
            /* Repeat next byte 1-n times */
            value = in_buff[in_pos+1];
            memset(&(out_buff[out_pos]), value, (1 - hvalue));
            in_pos += 2;
            out_pos += (1 - hvalue);
        }
        else
            in_pos += 1;
    }
}

/* Photoshop RLE 8 bit case */
static void read_psd_line_rle8(ETS_SrcPixel **ibufs, int xs, FILE *fi, int planes, 
                              void *image_ctx)
{
    int kk;
    psd_ctx_t *psd_ctx = (psd_ctx_t*) image_ctx;
    long curr_file_offset;
    int curr_row_length_index;
    int curr_length;

    /* Loop across each band */  
    for (kk = 0; kk < planes; kk++) 
    {
        /* Get a pointer and a length for the current one */
        curr_file_offset = psd_ctx->band_file_offset[kk];
        curr_row_length_index = psd_ctx->band_row_length_index[kk];
        curr_length = psd_ctx->row_lengths[curr_row_length_index];
        /* Get to where we are located in the file, read and decode */
        fseek(fi, curr_file_offset, SEEK_SET); 
        fread(psd_ctx->rle_row, 1, curr_length, fi);
        rle_decode((uchar*) psd_ctx->rle_row, curr_length, ibufs[psd_ctx->permute[kk]], 
                    psd_ctx->width);
        /* Update where we are in each band */
        psd_ctx->band_file_offset[kk] += curr_length;
        psd_ctx->band_row_length_index[kk] += 1;
    } 
}

/* Photoshop unencoded 8 bit case */
static void read_psd_line8(ETS_SrcPixel **ibufs, int xs, FILE *fi, int planes,
                          void *image_ctx)
{
    int kk;
    psd_ctx_t *psd_ctx = (psd_ctx_t*) image_ctx;
    long curr_file_offset;
    int curr_length = psd_ctx->width;

    /* Loop across each band */
    for (kk = 0; kk < planes; kk++) 
    {
        /* Get a pointer and a length for the current one */
        curr_file_offset = psd_ctx->band_file_offset[kk];
        /* Get to where we are located in the file, read and decode */
        fseek(fi, curr_file_offset, SEEK_SET); 
        fread(ibufs[psd_ctx->permute[kk]], 1, curr_length, fi);
        /* Update where we are in each band */
        psd_ctx->band_file_offset[kk] += curr_length;
    }
}

/* Photoshop 8 bit unencoded output.  Due to its planar form, this is stored in a 
   temp buffer until we reach the final line, at which point we write out the
   entire thing. */
static void write_psd_line8(uchar **obufs, int xs, FILE *fo, int planes,
                           void *image_ctx)
{
    int kk;
    psd_ctx_t *psd_ctx = (psd_ctx_t*) image_ctx;
    const void *input_buf;
    uchar *output_buff = (uchar*) psd_ctx->output_buffer;
    long plane_size = (psd_ctx->width) * (psd_ctx->height);
    long buffer_size = plane_size * planes;
    
    output_buff += ((psd_ctx->curr_row) * (psd_ctx->width));
    for (kk = 0; kk < planes; kk++)
    {
        input_buf = (const void*) obufs[psd_ctx->permute[kk]];
        memcpy(output_buff, input_buf, psd_ctx->width);
        output_buff += plane_size;
    }
    psd_ctx->curr_row += 1;
    /* If at end, then dump the buffer */
    if (psd_ctx->curr_row == psd_ctx->height)
        fwrite(psd_ctx->output_buffer, 1, buffer_size, fo);
}

/* Photoshop unencoded 16 bit case */
static void read_psd_line16(ETS_SrcPixel **ibufs, int xs, FILE *fi, int planes,
                          void *image_ctx)
{
    int kk;
    psd_ctx_t *psd_ctx = (psd_ctx_t*) image_ctx;
    long curr_file_offset;
    int curr_length = psd_ctx->width * 2;
    int i;
    unsigned short temp_value1, temp_value2;

    /* Loop across each band */
    for (kk = 0; kk < planes; kk++) 
    {
        /* Get a pointer and a length for the current one */
        curr_file_offset = psd_ctx->band_file_offset[kk];
        /* Get to where we are located in the file, read and decode */
        fseek(fi, curr_file_offset, SEEK_SET); 
        fread(ibufs[psd_ctx->permute[kk]], 1, curr_length, fi);
#ifdef LITTLE_ENDIAN
        for (i = 0; i < psd_ctx->width; i++)
        {
            temp_value1 = ibufs[psd_ctx->permute[kk]][i];
            temp_value2 = ((temp_value1 & 0xff) << 8) + ((temp_value1 & 0xff00) >> 8);
            ibufs[psd_ctx->permute[kk]][i] = temp_value2;
        }
#endif
        /* Update where we are in each band */
        psd_ctx->band_file_offset[kk] += curr_length;
    }
}

/* Photoshop 16 bit unencoded output.  Due to its planar form, this is stored in a 
   temp buffer until we reach the final line, at which point we write out the entire 
   thing. This is not used really, but here for completeness and some testing.
   Needs fix for little endian */
static void write_psd_line16(uchar **obufs, int xs, FILE *fo, int planes,
                           void *image_ctx)
{
    int kk;
    psd_ctx_t *psd_ctx = (psd_ctx_t*) image_ctx;
    const void *input_buf;
    uchar *output_buff = (uchar*) psd_ctx->output_buffer;
    long plane_size = (psd_ctx->width) * (psd_ctx->height) * 2;
    long buffer_size = plane_size * planes;
    
    output_buff += ((psd_ctx->curr_row) * (psd_ctx->width)) * 2;
    for (kk = 0; kk < planes; kk++)
    {
        input_buf = (const void*) obufs[psd_ctx->permute[kk]];
        memcpy(output_buff, input_buf, psd_ctx->width * 2);
        output_buff += plane_size;
    }
    psd_ctx->curr_row += 1;
    /* If at end, then dump the buffer */
    if (psd_ctx->curr_row == psd_ctx->height)
        fwrite(psd_ctx->output_buffer, 1, buffer_size, fo);
}

static void finalize_psd(void *image_ctx)
{
    psd_ctx_t *psd_ctx = (psd_ctx_t*) image_ctx;

    if (psd_ctx->output_buffer != NULL)
        free(psd_ctx->output_buffer);
    if (psd_ctx->band_row_length_index != NULL)
        free(psd_ctx->band_row_length_index);
    if (psd_ctx->band_file_offset != NULL)
        free(psd_ctx->band_file_offset);
    if (psd_ctx->rle_row != NULL)
        free(psd_ctx->rle_row);
    if (psd_ctx->permute != NULL)
        free(psd_ctx->permute);
}

static void read_psd(FILE *fi, psd_ctx_t *psd_ctx, FILE *fo)
{
    uchar buf[256];
    int depth;
    uchar num_channel;
    int height, width, color_mode;
    int size;
    int bytes;
    uchar *temp_buff;
    int kk, jj;
    long data_size;
    int codec, maxlength = 0;
    long count = 0;
    long curr_count = 0;
    int index = 0;
#ifdef TEST_PSD_DUMP
    uchar *in_buff, *out_buff;
#endif
    
    psd_ctx->curr_row = 0;

    /* rewind and get the header information */
    rewind(fi);

    fread(buf, 1, 26, fi);
    count += 26;

    num_channel = buf[13];
    get4(&height, &(buf[14]));
    get4(&width, &(buf[18]));

    depth = buf[23];
    color_mode = buf[25];

    if (!(depth == 8 || depth == 16))
        die("Only 8 or 16 bit PSD files supported");

    if (depth == 16 && (sizeof(ETS_SrcPixel) != 2 || ETS_SRC_MAX != 65535))
        die("ETS_SrcPixel type and ETS_SRC_MAX in ets.h not set for 16 bit support!");

    if (depth == 8 && (sizeof(ETS_SrcPixel) != 1 || ETS_SRC_MAX != 255))
        die("ETS_SrcPixel type and ETS_SRC_MAX in ets.h not set for 8 bit support!");

    /* Dont handle duotone or indexed data at this time */
    if (color_mode == 2 || color_mode == 8) 
        die("Indexed and Duotone PSD files not supported");

    psd_ctx->depth = depth;
    psd_ctx->height = height;
    psd_ctx->width = width;
    psd_ctx->num_channels = num_channel;

    if (depth == 8)
        bytes = 1;
    else 
        bytes = 2;

    /* Change the output to 8 bit */
    buf[23] = 8;

    /* Write out the header information */
    fwrite(buf, 1, 26, fo);

    /* Index or duotone information */
    fread(buf, 1, 4, fi);
    fwrite(buf, 1, 4, fo);
    count += 4;

    /* Record information. Pass along... */
    fread(buf, 1, 4, fi);
    get4(&size,  buf);
    fwrite(buf, 1, 4, fo);
    temp_buff = (uchar*) malloc(size);
    fread(temp_buff, 1, size, fi);
    fwrite(temp_buff, 1, size, fo);
    free(temp_buff);
    count += (size + 4);

    /* Layer information.  Pass along... */
    fread(buf, 1, 4, fi);
    get4(&size,  buf);
    fwrite(buf, 1, 4, fo);
    temp_buff = (uchar*) malloc(size);
    fread(temp_buff, 1, size, fi);
    fwrite(temp_buff, 1, size, fo);
    free(temp_buff);
    count += (size + 4);

    /* Image information.  The stuff we want.  Only support raw or RLE types for 
       input.  Output is uncompressed */
    fread(buf, 1, 2, fi);
    codec = buf[1];
    if (!(codec == 0 || codec == 1)) 
        die("ZIP data in PSD files not supported");

    buf[1] = 0;
    fwrite(buf, 1, 2, fo);
    count += 2;

    if (codec == 1 && depth == 16) 
        die("PSD RLE 16 bit files not supported");

    /* Allocate the output buffer */
    psd_ctx->output_buffer = malloc(num_channel * height * width * bytes);

    if (codec == 1) 
    {
        /* RLE. Need to decompress the data.  Each scan line is individually 
           compressed.  First read in the size for each compressed line */
        data_size = num_channel * height;
        psd_ctx->row_lengths = (int*) malloc(data_size * sizeof(int));
        psd_ctx->band_file_offset = (long*) malloc(num_channel * sizeof(long));
        psd_ctx->band_row_length_index = (int*) malloc(num_channel * sizeof(int));
        count += (2 * data_size); /* This gets us to the start of the image data */
        /* Here we compute where in the file we need to go, to get the start of 
           the scan line in each band, we compute the max length of all the 
           encoded bands and get the length of each encoded band */
        for (jj = 0; jj < num_channel; jj++) 
        {
            (psd_ctx->band_file_offset)[jj] = count + curr_count; 
            for (kk = 0; kk < height; kk++) 
            {
                fread(buf, 1, 2, fi);
                get2(&((psd_ctx->row_lengths)[index]), buf);
                if (maxlength < (psd_ctx->row_lengths)[index]) 
                    maxlength = (psd_ctx->row_lengths)[index];
                curr_count = curr_count + (psd_ctx->row_lengths)[index];
                index++;
            }
        }
        psd_ctx->rle_row = (void*) malloc(maxlength);
        psd_ctx->read_line = read_psd_line_rle8;
        psd_ctx->write_line = write_psd_line8;
        for (kk = 0; kk < num_channel; kk++)
        {
            (psd_ctx->band_row_length_index)[kk] = kk * height;
        }
#ifdef TEST_PSD_DUMP
        /* Now do the decode for testing */
        in_buff = (uchar*) malloc(maxlength);
        out_buff = (uchar*) malloc(width);
        for (kk = 0; kk < data_size; kk++) 
        {
            fread(in_buff, 1, (psd_ctx->row_lengths)[kk], fi);
            rle_decode(in_buff, (psd_ctx->row_lengths)[kk], out_buff, width); 
            fwrite(out_buff, 1, width, fo);
        }
        fclose(fo);
        free(in_buff);
        free(out_buff);
        die("Testing case");
#endif 
    } 
    else 
    {
        /* Uncompressed data.  Read directly */
        psd_ctx->row_lengths  = NULL;
        psd_ctx->rle_row = NULL;
        psd_ctx->band_row_length_index = NULL;
        psd_ctx->band_file_offset = (long*) malloc(num_channel * sizeof(long));
        for (kk = 0; kk < num_channel; kk++)
        {
            (psd_ctx->band_file_offset)[kk] = count + height * width * kk * bytes; 
        }
        if (depth == 8) 
        {
            psd_ctx->read_line = read_psd_line8;
            psd_ctx->write_line = write_psd_line8;
        }
        else 
        {
            psd_ctx->read_line = read_psd_line16;
            psd_ctx->write_line = write_psd_line8;
        }
    }
    psd_ctx->finalize = finalize_psd;
    psd_ctx->permute = (uchar*) malloc(num_channel);
    /* A default initialization */
    for (kk = 0; kk < num_channel; kk++)
    {
        psd_ctx->permute[kk] = kk;
    }
}

static void read_pgm(FILE *fi, int *xs, int *ys, FILE *fo)
{
    char buf[256];
    int depth;

    do
        fgets(buf, sizeof(buf), fi);
    while (buf[0] == '#');
    sscanf (buf, "%d", xs);
    do
        fgets (buf, sizeof(buf), fi);
    while (buf[0] == '#');
    sscanf (buf, "%d", ys);
    if (*xs <= 0 || *ys <= 0 || *xs > MAX_SIZE || *ys > MAX_SIZE)
        die("Input image size out of range");

    do
        fgets(buf, sizeof(buf), fi);
    while (buf[0] == '#');
    sscanf(buf, "%d", &depth);
    if (depth != 255)
        die("Only works with depth=255 images");

    fprintf(fo, "P5\n%d %d\n255\n", *xs, *ys);
}

static int read_pam(FILE *fi, int *xs, int *ys, FILE *fo)
{
    char buf[256];
    int i, depth;
    char c;

    fprintf(fo, "P7\n");

    do
    {
        fgets(buf, sizeof(buf), fi);
        if (buf[0] == '#')
        {
            fprintf(fo, "%s", buf);
            continue;
        }
        if (buf[0] == '\n' || buf[0] == '\r')
            continue;
        if (sscanf(buf, "WIDTH %d", xs))
        {
            fprintf(fo, "WIDTH %d\n", *xs);
        }
        else if (sscanf(buf, "HEIGHT %d", ys))
        {
            fprintf(fo, "HEIGHT %d\n", *ys);
        }
        else if (sscanf(buf, "DEPTH %d", &depth))
        {
            if (depth < 4)
                die("Only CMYK/DEVN pams supported");
            fprintf(fo, "DEPTH %d\n", depth);
        }
        else if (sscanf(buf, "MAXVAL %d", &i))
        {
            if (i != 255)
                die("Only pams with MAXVAL=255 supported");
            fprintf(fo, "MAXVAL 255\n");
        }
        else if (sscanf(buf, "TUPLTYPE DEV%c", &c) && c == 'N')
        {
            fprintf(fo, "TUPLTYPE DEVN\n");
        }
        else if (sscanf(buf, "TUPLTYPE CMY%c", &c) && c == 'K')
        {
            fprintf(fo, "TUPLTYPE CMYK\n");
        }
        else if (sscanf(buf, "TUPLTYP%c") && c == 'E')
        {
            die("Only CMYK/DEVN pams supported");
        }
        else if (sscanf(buf, "ENDHD%c", &c) && c == 'R')
        {
            fprintf(fo, "ENDHDR\n");
            break;
        }
        else
        {
            printf(stderr, "Unknown header field: %s\n", buf);
            die("Unknown header field\n");
        }
    }
    while (1);

    return depth;
}

static void read_pgm_line(ETS_SrcPixel **ibufs, int xs, FILE *fi, int planes, 
                          void *image_ctx)
{
    int nbytes;
    nbytes = fread(ibufs[0], 1, xs, fi);
    if (nbytes != xs)
        die("Error reading image (file truncated?)");
}

static void read_pam_line(ETS_SrcPixel **ibufs, int xs, FILE *fi, int planes, 
                          void *image_ctx)
{
    int i, j, c = 0;

    for (i = 0; i < xs && c != EOF; i++)
    {
        ibufs[1][i] = 255-fgetc(fi);
        ibufs[2][i] = 255-fgetc(fi);
        ibufs[3][i] = 255-fgetc(fi);
        ibufs[0][i] = 255-(c = fgetc(fi));
        for (j=4; j < planes; j++)
            ibufs[j][i] = 255-(c = fgetc(fi));
    }
    if (c == EOF)
        die("Error reading image (file truncated?)");
}

static void write_pgm_line(uchar **obufs, int xs, FILE *fo, int planes,
                           void *image_ctx)
{
    fwrite(obufs[0], 1, xs, fo);
}

static void write_pam_line(uchar **obufs, int xs, FILE *fo, int planes,
                           void *image_ctx)
{
    int i, j;

    for (i = 0; i < xs; i++)
    {
        fputc(255-obufs[1][i], fo);
        fputc(255-obufs[2][i], fo);
        fputc(255-obufs[3][i], fo);
        fputc(255-obufs[0][i], fo);
        for (j = 4; j < planes; j++)
        {
            fputc(255-obufs[j][i], fo);
        }
    }
}

int
main(int argc, char **argv)
{
    FILE *fi = stdin;
    FILE *fo = stdout;
    char buf[256];
    int xs, ys;
    int xsb;
    ETS_POLARITY polarity;
    ETS_Params params;
    ETS_Ctx *ctx;
    int lut[ETS_SRC_MAX+1], i;
    int *luts[M];
    int rs_lut[ETS_SRC_MAX+1];
    int *rs_luts[M];
    int strengths[M][M] = { { 0 },
                            { 0 },
                            { 0 },
                            { 128, 51, 51, 13 },                 // KCMY
                            { 0 },
                            { 128, 51, 51, 13, 13, 13 },         // KCMYcm
                            { 128, 51, 51, 13, 13, 13, 13 },     // KCMYcmk
                            { 128, 51, 51, 13, 13, 13, 13, 13},  // KCMYcmkk
                          };
    /* Used now in PSD case so that we can reorder the planes according to the above weights */
    uchar permutes[M][M] = { { 0 },
                        { 0 },
                        { 0 },
                        { 3, 0, 1, 2 },             // KCMY
                        { 0 },
                        { 3, 0, 1, 2, 4, 5 },       // KCMYcm
                        { 3, 0, 1, 2, 4, 5, 6 },    // KCMYcmk
                        {3, 0, 1, 2, 4, 5, 6, 7},  // KCMYcmkk
                        };
    int c1_scale[] = { 1, 1, 1, 1, 1, 1, 1, 1, 1 };
    ETS_SrcPixel *ibufs[M] = { 0 };
    uchar *obufs[M] = { 0 };
    int planes;
    void (*read_line)(ETS_SrcPixel **ibufs, int xs, FILE *fi, int planes, void *image_ctx);
    void (*write_line)(uchar **obufs, int xs, FILE *fo, int planes, void *image_ctx);
    void (*finalize)(void *image_ctx);
    char *gamma_tab = NULL;
    int multiplane = 1;
    int ets_style = 1;
    int r_style = 1;
    int levels = 2;
    int aspect_x = 1;
    int aspect_y = 1;
    int noise = 0;
    psd_ctx_t psd_ctx;
    void *image_ctx = NULL;
    uchar byte_count = 1;

    int y;

    for (i = 1; i < argc; i++)
    {
        const char *arg = argv[i];
        const char *arg_value;

        if (arg[0] == '-')
        {
            if ((arg_value = get_arg(argc, argv, &i, arg + 2)) == NULL)
                goto usage_exit;

            switch (arg[1])
            {
            case 'i':
                fi = fopen(arg_value,"rb");
                if (fi == NULL)
                    die ("Input not found");
                break;
            case 'o':
                fo = fopen(arg_value,"wb");
                if (fo == NULL)
                    die ("Output failed to open");
                break;
            case 'g':
                gamma_tab = arg_value;
                break;
            case 'm':
                multiplane = atoi(arg_value);
                break;
            case 'l':
                levels = atoi(arg_value);
                break;
            case 'e':
                ets_style = atoi(arg_value);
                break;
            case 'r':
                r_style = atoi(arg_value);
                break;
            case 'n':
                noise = atoi(arg_value);
                break;
            case 'a':
                sscanf(arg_value, "%d:%d", &aspect_x, &aspect_y);
                break;
            default:
                goto usage_exit;
            }
        }
    }

    fgets(buf, sizeof(buf), fi);
    xs = ys = 0;
    if (buf[0] == 'P' && buf[1] == '5')
    {
        read_pgm(fi, &xs, &ys, fo);
        read_line = read_pgm_line;
        write_line = write_pgm_line;
        planes = 1;
        finalize =  NULL;
        polarity = ETS_BLACK_IS_ZERO;
    }
    else if (buf[0] == 'P' && buf[1] == '7')
    {
        planes = read_pam(fi, &xs, &ys, fo);
        read_line = read_pam_line;
        write_line = write_pam_line;
        finalize =  NULL;
        polarity = ETS_BLACK_IS_ZERO;
    }
    else if (buf[0] == '8' && buf[1] == 'B' && buf[2] == 'P' && buf[3] == 'S')
    {
        read_psd(fi, &psd_ctx, fo);
        read_line = psd_ctx.read_line;
        write_line = psd_ctx.write_line;
        finalize = psd_ctx.finalize;
        planes = psd_ctx.num_channels;
        xs = psd_ctx.width;
        ys = psd_ctx.height;
        image_ctx = (void*) &psd_ctx;
        if (psd_ctx.depth == 16)
            byte_count = 2;
        polarity = ETS_BLACK_IS_ONE;
        /* Set the read and write permutation order according to permutes array */
        if (permutes[planes-1] != 0)
        {
            int sum = 0;  /* an error check */
            for (i = 0; i < planes; i++) 
            {
                psd_ctx.permute[i] = permutes[planes-1][i];
                sum += psd_ctx.permute[i];
            }
            if (sum != ((float) planes/2.0) * (float) (planes - 1.0))
                die ("Permutation vector values are not valid");
        }
    }
    else
        die("Need pgmraw, pamraw or psd image on input");

    xsb = xs;
    for (i = 0; i < planes; i++)
    {
        ibufs[i] = (ETS_SrcPixel*) malloc(xs * byte_count);
        obufs[i] = (uchar*) ets_malloc_aligned(xsb + 16, 16);
    }

    /* This sets up a simple gamma lookup table. */
    if (gamma_tab)
    {
        FILE *lutf = fopen(gamma_tab, "r");
        for (i = 0; i < (ETS_SRC_MAX+1); i++)
            fscanf(lutf, "%d", &lut[i]);
        fclose(lutf);
    }
    else
    {
        double scale = ETS_SRC_MAX;
        for (i = 0; i < (ETS_SRC_MAX+1); i++)
#if 1
            lut[i] = (int)((1 << 24) * (pow (i / scale, 1.0)));
#else
            lut[i] = (1 << 24) * (0.88 + 0.02 * i / 255.0);
#endif
    }

    for (i = 0; i < (ETS_SRC_MAX+1); i++)
        rs_lut[i] = 2 << 16;

#ifdef DUMP_FILE_NAME
    params.dump_file = fopen(DUMP_FILE_NAME, "wb");
#else
    params.dump_file = NULL;
#endif
    params.dump_level = ETS_DUMP_ALL;

    params.width = xs;
    params.n_planes = planes;
    params.levels = levels;
    for (i = 0; i < planes; i++)
        luts[i] = lut;
    params.luts = luts;
    params.strengths = (multiplane ? strengths[planes-1] : strengths[0]);
    params.aspect_x = aspect_x;
    params.aspect_y = aspect_y;
    params.distscale = 0;
    params.rand_scale = noise;
    params.c1_scale = c1_scale;
    params.ets_style = ets_style;
    params.r_style = r_style;
    params.polarity = polarity;
    for (i = 0; i < planes; i++)
        rs_luts[i] = rs_lut;
    params.rand_scale_luts = rs_luts;
    params.rand_scale_luts = NULL;
    ctx = ets_new(&params);
    for (y = 0; y < ys; y++)
    {
        int x;

        read_line(ibufs, xs, fi, planes, image_ctx);
        ets_line(ctx, obufs, (const ETS_SrcPixel *const *)&ibufs);
        
        for (i = 0; i < planes; i++)
        {
            uchar *obuf = obufs[i];
            if (polarity == ETS_BLACK_IS_ONE)
            {
                for (x = 0; x < xs; x++)
                {
                    obuf[x] = obuf[x] * 255 / (params.levels - 1);
                }
            } 
            else 
            {
                for (x = 0; x < xs; x++)
                {
                    obuf[x] = 255 - obuf[x] * 255 / (params.levels - 1);
                }
            }
        }
        write_line(obufs, xs, fo, planes, image_ctx);
    }
    if (finalize != NULL)
        finalize(image_ctx);

    ets_free(ctx);
    for (i=0; i < planes; i++)
    {
        ets_free_aligned(obufs[i]);
        free(ibufs[i]);
    } 
    if (fi != stdin)
        fclose(fi);
    if (fo != stdout)
        fclose(fo);

    return 0;
 usage_exit:
    return usage();
}
