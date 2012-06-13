/* Copyright (C) 2001-2012 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134, San Rafael,
   CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

/* $Id: gslt_image_threads_test.c 2993 2007-12-18 23:28:31Z giles $ */
/* example client for the gslt image loading library */

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

typedef unsigned char byte;
#include "gslt.h"
#include "gslt_image.h"

extern void gs_erasepage(gs_state *gs);
extern void gs_moveto(gs_state *gs, double, double);
extern void gs_output_page(gs_state *gs, int, int);

gslt_image_t *
decode_image_file(gs_memory_t *mem, FILE *in)
{
    gslt_image_t *image;
    int len, bytes;
    byte *buf;

    /* get compressed data size */
    fseek(in, 0, SEEK_END);
    len = ftell(in);

    /* load the file into memory */
    fseek(in, 0, SEEK_SET);
    buf = malloc(len);
    if (buf == NULL) return NULL;

    bytes = fread(buf, 1, len, in);
    if (bytes != len) {
        free(buf);
        return NULL;
    }

    image = gslt_image_decode(mem, buf, len);
    free(buf);

    return image;
}

gslt_image_t *
decode_image_filename(gs_memory_t *mem, const char *filename)
{
    gslt_image_t *image;
    FILE *in;

    in = fopen(filename, "rb");
    if (in == NULL) return NULL;

    image = decode_image_file(mem, in);
    fclose(in);

    return image;
}

/* write out the image as a pnm file for verification */
int
write_image_file(gslt_image_t *image, FILE *out)
{
    byte *row;
    int j, bytes;

    if (image == NULL || image->samples == NULL) {
        fprintf(stderr, "ignoring empty image object\n");
        return  -1;
    }

    if (image->components == 1 && image->bits == 1) {
        /* PBM file */
        int i;
        int rowbytes = (image->width+7)>>3;
        byte *local = malloc(rowbytes);

        fprintf(out, "P4\n%d %d\n", image->width, image->height);
        row = image->samples;
        for (j = 0; j < image->height; j++) {
            /* PBM images are inverted relative to our XPS/PS convention */
            for (i = 0; i < rowbytes; i++)
                local[i] = row[i] ^ 0xFF;
            bytes = fwrite(local, 1, rowbytes, out);
            row += image->stride;
        }
        free(local);
    } else if (image->components == 1 && image->bits == 8) {
        /* PGM file */
        fprintf(out, "P5\n%d %d\n255\n", image->width, image->height);
        row = image->samples;
        for (j = 0; j < image->height; j++) {
            bytes = fwrite(row, 1, image->width, out);
            row += image->stride;
        }
    } else {
        /* PPM file */
        fprintf(out, "P6\n%d %d\n255\n", image->width, image->height);
        row = image->samples;
        for (j = 0; j < image->height; j++) {
            bytes = fwrite(row, image->components, image->width, out);
            row += image->stride;
        }
    }

    return 0;
}

int
write_image_filename(gslt_image_t *image, const char *filename)
{
    FILE *out;
    int error;

    out = fopen(filename, "wb");
    if (out == NULL) {
        fprintf(stderr, "could not open '%s' for writing\n", filename);
        return -1;
    }

    error = write_image_file(image, out);
    fclose(out);

    return error;
}

/* global input filename. */
char *filename;

void *
print_image(void *threadid)
{
    gs_memory_t *mem;
    gx_device *dev;
    gs_state *gs;
    gslt_image_t *image;
    char *s;
    int code = 0;

    /* get the device from the environment, or default */
    s = getenv("DEVICE");
    if (!s)
        s = "nullpage";

    /* initialize the graphicis library and set up a drawing state */
    mem = gslt_init_library();
    dev = gslt_init_device(mem, s);
    gs = gslt_init_state(mem, dev);

    gslt_get_device_param(mem, dev, "Name");
    gslt_set_device_param(mem, dev, "OutputFile", "gslt.out");

    /* prepare page for drawing (unused) */
    gs_erasepage(gs);
    gs_moveto(gs, 72.0, 72.0);

    /* load and decode the image */
    image = decode_image_filename(mem, filename);
    if (image == NULL) {
        fprintf(stderr, "reading image failed.\n");
        code = -1;
    }
    /* save an uncompressed copy for verification */
    {
        char outname[50];
        sprintf(outname, "out_%d.pnm", (int)threadid);
        fprintf(stderr, "writing %s\n", outname);
        write_image_filename(image, outname);
    }

    /* image could be drawn to the page here */

    /* release the image */
    gslt_image_free(mem, image);

    /* output the page (unused) */
    gs_output_page(gs, 1, 1);

    /* clean up the library */
    gslt_free_state(mem, gs);
    gslt_free_device(mem, dev);
    gslt_free_library(mem);

    pthread_exit(NULL);
}

int
main(int argc, const char *argv[])
{

    pthread_t thread1, thread2;
    int ret;
    int t1 = 1;
    int t2 = 2;

    filename = argv[argc-1];
    fprintf(stderr, "loading '%s'\n", filename);

    if ( ((ret=pthread_create(&thread1, NULL, print_image, (void *)t1)) != 0) ||
         ((ret=pthread_create(&thread2, NULL, print_image, (void *)t2)) != 0) ) {
        fprintf(stderr, "Error creating thread code=%d", ret);
        exit(-1);
    }
    pthread_exit(NULL);
}
