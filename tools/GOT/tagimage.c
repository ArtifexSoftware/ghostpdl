#include <stdio.h>
#include <string.h>

#define UNKNOWN_TAG 0x1
#define TEXT_TAG 0x2
#define IMAGE_TAG 0x4
#define PATH_TAG 0x8
#define TEXTnImage_TAG 0x6
#define TEXTnPATH_TAG 0xA
#define PATHnImage_TAG 0xC
#define TEXTnPATHnIMAGE_TAG 0xE
#define UNTOUCHED_TAG 0x10

const char red[]   =  { 0xff, 0x00, 0x00 };
const char green[] =  { 0x00, 0xff, 0x00 };
const char blue[]  =  { 0x00, 0x00, 0xff };
const char white[] =  { 0xff, 0xff, 0xff };
const char black[] =  { 0x00, 0x00, 0x00 };
const char yellow[] = { 0xff, 0xff, 0x00 };
const char magenta[] = { 0xff, 0x00, 0xff };
const char cyan[] = { 0x00, 0xff, 0xff };
const char gray[] = { 0x80, 0x80, 0x80 };

static const char *
tag2color(int tag)
{
    switch (tag) {
    case UNKNOWN_TAG:
        return black;
    case TEXT_TAG:
        return red;
    case IMAGE_TAG:
        return green;
    case PATH_TAG:
        return blue;
    case TEXTnImage_TAG :
        return yellow;
    case TEXTnPATH_TAG :
        return magenta;
    case PATHnImage_TAG :
        return cyan;
    case TEXTnPATHnIMAGE_TAG :
        return white;
    case UNTOUCHED_TAG:
        return gray;
    default:
        return black;
    }
}

/* create tag image in ppmraw output format */
int
main(int argc, char *argv[]) {

    /* Source output should already have ppm header */
    char line[128]; 
    int k;
    int len;
    char *infile;
    char *outfile;
    char c;
    FILE *fid_in, *fid_out;

    if (argc != 3) {
        fprintf(stderr, "Syntax: tagimage <infile1> <outfile2> \n");
        return(-1);
    }
    infile = argv[1];
    outfile = argv[2];
    fid_in = fopen(infile,"rb");
    fid_out = fopen(outfile,"wb+");
    if (fid_in == NULL) {
        fprintf(stderr, "Cannot open input file \n");
        return(-1);
    }
    if (fid_out == NULL) {
        fprintf(stderr, "Cannot open output file \n");
        return(-1);
    }
    c = 0x0A;
    /* header */
    for (k = 0; k < 3; k++) {
        if (fgets( line, sizeof(line), fid_in ) != NULL ) {
            len = strlen(line);
            fwrite(line,1,len-1,fid_out);
            /* Make sure we don't have 0D 0A nonsense */
            fwrite(&c,1,1,fid_out);
        } else {
            return -1;  /* error! Should have this data */
        }
    }
    /* data */
    while (!feof(fid_in)) {
        int tag = getc(fid_in); /* Get tag value, first byte */
        char *color = tag2color(tag); /* Get tag pseudo color */
        fwrite(color,1,3,fid_out); /* Write out tag pseudo color */
        getc(fid_in); getc(fid_in); getc(fid_in);  /* Trash image in source */
    }
    fclose(fid_in);
    fclose(fid_out);
    return 0;
}
