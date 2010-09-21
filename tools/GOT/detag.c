#include <stdio.h>
#include <string.h>

/* detag bit rgb and produce ppmraw output */
int
main(int argc, char *argv[]) {

    /* Source output should already have ppm header */
    char line[128]; 
    int k,j;
    int len;
    char *infile;
    char *outfile;
    char c;
    FILE *fid_in, *fid_out;

    if (argc != 3) {
        fprintf(stderr, "Syntax: detag <infile1> <outfile2> \n");
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
        c = getc(fid_in);
        putc(getc(fid_in),fid_out);
        putc(getc(fid_in),fid_out);
        putc(getc(fid_in),fid_out);
    }
    fclose(fid_in);
    fclose(fid_out);
    return 0;
}

