/* Copyright (C) 2004 artofcode LLC.  All rights reserved.
  
  This software is provided AS-IS with no warranty, either express or
  implied.
  
  This software is distributed under license and may not be copied,
  modified or distributed except as expressly authorized under the terms
  of the license contained in the file LICENSE in this distribution.
  
  For more information about licensing, please refer to
  http://www.ghostscript.com/licensing/. For information on
  commercial licensing, go to http://www.artifex.com/licensing/ or
  contact Artifex Software, Inc., 101 Lucas Valley Road #110,
  San Rafael, CA  94903, U.S.A., +1(415)492-9861.
*/

/* $Id$ */
/* encapsulates jp2/jpx JPEG2000 images in PDF 1.5 files */
/* based on jpegtopdf by Tor Andersson */

/* uses the jasper library from http://www.ece.uvic.ca/~mdadams/jasper/
   compile with 'cc -o jpxtopdf jpxtopdf.c -ljasper'
   or 'cc -o jpxtopdf jpxtopdf.c -ljasper -ljpeg -lm' */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <jasper/jasper.h>

int pages[8000];
int npages = 0;

int xref[8000*3+100];
int nxref = 0;

typedef struct {
  char     *filename;             /* name of image file			 */
  int      filesize;		  /* data length of image file           */
  int      width;                 /* pixels per line			 */
  int      height;                /* rows				 */
  int      numcmpts;              /* number of color components		 */
  int      depth;                 /* bits per color component		 */
  float    dpi;                   /* image resolution in dots per inch   */
} imagedata;

enum { FALSE, TRUE };

static int JPXtoPDF(imagedata * JPX, FILE * out)
{
	FILE *in;
	jas_stream_t *stream;
	jas_image_t *image;
	int format;
	char *format_name;
	float scale;
	char buf[4096];
	int n;

    if (jas_init()) return;
    
	/* read image parameters and fill JPX struct */
	stream = jas_stream_fopen(JPX->filename, "rb");
	if (stream == NULL) {
		fprintf(stderr, "Error: could not open input file '%s'\n", JPX->filename);
		return;
	}

	/* check that it's a jp2/jpx image */
	format = jas_image_getfmt(stream);
	format_name = jas_image_fmttostr(format);
	if (format_name == NULL) {
        format_name = "unknown";
    }
    if (strncmp(format_name, "jp2", 3)) {
		fprintf(stderr, "Error: '%s' is not a proper JPX file!\n", JPX->filename);
		return;
	}
	fprintf(stderr, "Info: input file is '%s' format.\n", format_name);

	image = jas_image_decode(stream, format, 0);
	jas_stream_close(stream);
	if (image == NULL) {
		fprintf(stderr, "Error: unable to decode image '%s'\n", JPX->filename);
		return;
	}

	JPX->width = jas_image_width(image);
	JPX->height = jas_image_height(image);
	JPX->numcmpts = jas_image_numcmpts(image);
	JPX->depth = jas_image_cmptprec(image, 0);
	JPX->dpi = 0; /* how do we get this from jasper? */

	//dump_jas_image(image);
	fprintf(stderr,
		"Note on file '%s': %dx%d pixel, %d color component%s, dpi %f\n",
			JPX->filename, JPX->width, JPX->height,
			JPX->numcmpts, (JPX->numcmpts == 1 ? "" : "s"),
			JPX->dpi);

	if (JPX->dpi == 0)
		JPX->dpi = 100.0;

	scale = 72.0 / JPX->dpi;

    jas_image_destroy(image);
    
	in = fopen(JPX->filename, "rb");
	if (in == NULL) {
		fprintf(stderr, "Error: unable to open input file '%s'\n", JPX->filename);
		return;
	}
	fseek(in, 0, SEEK_END);
	JPX->filesize = ftell(in);
	fseek(in, 0, SEEK_SET);

	xref[nxref++] = ftell(out);
	fprintf(out, "%d 0 obj\n", nxref);
	fprintf(out, "<</Type/XObject/Subtype/Image\n");
	fprintf(out, "/Width %d /Height %d\n", JPX->width, JPX->height);
	fprintf(out, "/ColorSpace/%s\n", JPX->numcmpts == 1 ? "DeviceGray" : "DeviceRGB");
	fprintf(out, "/BitsPerComponent %d\n", JPX->depth);
	fprintf(out, "/Length %d\n", JPX->filesize);
	fprintf(out, "/Filter/JPXDecode\n");
	fprintf(out, ">>\n");
	fprintf(out, "stream\n");

#if 1
	/* copy data from JPX file */
	//fseek(in, 0, SEEK_SET);
	while ((n = fread(buf, 1, sizeof(buf), in)) != 0)
		fwrite(buf, 1, n, out);
#endif

	fprintf(out, "endstream\n");
	fprintf(out, "endobj\n");
	fprintf(out, "\n");

	fclose(in);

	sprintf(buf, "%d 0 0 %d 0 0 cm /x%d Do\n",
		(int)ceil(JPX->width * scale),
		(int)ceil(JPX->height * scale),
		nxref);

	xref[nxref++] = ftell(out);
	fprintf(out, "%d 0 obj\n<</Length %d>>\n", nxref, strlen(buf));
	fprintf(out, "stream\n");
	fprintf(out, "%s", buf);
	fprintf(out, "endstream\n");
	fprintf(out, "endobj\n");
	fprintf(out, "\n");

	xref[nxref++] = ftell(out);
	fprintf(out, "%d 0 obj\n", nxref);
	fprintf(out, "<</Type/Page/Parent 3 0 R\n");
	fprintf(out, "/Resources << /XObject << /x%d %d 0 R >> >>\n", nxref-2, nxref-2);
	fprintf(out, "/MediaBox [0 0 %d %d]\n",
		(int)ceil(JPX->width * scale),
		(int)ceil(JPX->height * scale));
	fprintf(out, "/Contents %d 0 R\n", nxref-1);
	fprintf(out, ">>\n");
	fprintf(out, "endobj\n");
	fprintf(out, "\n");

	return nxref;
}

int main(int argc, char **argv)
{
	imagedata image;
	FILE *outfile;
	int i;
	int startxref;

	image.filename = NULL;

	outfile = fopen("out.pdf", "w");

	fprintf(outfile, "%%PDF-1.5\n\n");

	xref[nxref++] = ftell(outfile);
	fprintf(outfile, "1 0 obj\n");
	fprintf(outfile, "<</Type/Catalog/Pages 3 0 R>>\n");
	fprintf(outfile, "endobj\n\n");

	xref[nxref++] = ftell(outfile);
	fprintf(outfile, "2 0 obj\n");
	fprintf(outfile, "<</Creator(JPXtopdf)/Title(%s)>>\n", getenv("TITLE"));
	fprintf(outfile, "endobj\n\n");

	/* delay obj #3 (pages) until later */
	nxref++;

	for (i = 1; i < argc; i++) {
		image.filename = argv[i];

		pages[npages++] = JPXtoPDF(&image, outfile);	/* convert JPX data */
	}

	xref[2] = ftell(outfile);
	fprintf(outfile, "3 0 obj\n");
	fprintf(outfile, "<</Type/Pages/Count %d/Kids[\n", npages);
	for (i = 0; i < npages; i++)
		fprintf(outfile, "%d 0 R\n", pages[i]);
	fprintf(outfile, "]>>\nendobj\n\n");

	startxref = ftell(outfile);
	fprintf(outfile, "xref\n0 %d\n", nxref + 1);
	fprintf(outfile, "0000000000 65535 f \n");
	for (i = 0; i < nxref; i++)
		fprintf(outfile, "%0.10d 00000 n \n", xref[i]);
	fprintf(outfile, "trailer\n<< /Size %d /Root 1 0 R /Info 2 0 R >>\n", nxref + 1);
	fprintf(outfile, "startxref\n%d\n%%%%EOF\n", startxref);

	fclose(outfile);

	return 0;
}

