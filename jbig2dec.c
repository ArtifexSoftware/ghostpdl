/*
    jbig2dec
    
    Copyright (C) 2001-2002 artofcode LLC.
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    $Id: jbig2dec.c,v 1.23 2002/06/21 19:11:28 giles Exp $
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_GETOPT_H
# include <getopt.h>
#else
# include "getopt.h"
#endif

#include "jbig2.h"
#include "jbig2_image.h"

typedef enum {
    usage,dump,render
} jbig2dec_mode;

typedef struct {
	jbig2dec_mode mode;
	int verbose;
	char *output_file;
} jbig2dec_params_t;


static int
print_usage (void)
{
  fprintf(stderr,
    "Usage: jbig2dec [options] <file.jbig2>\n"
    "   or  jbig2dec [options] <global_stream> <page_stream>\n"
    "\n"
    "  When invoked with a single file, it attempts to parse it as\n"
    "  a normal jbig2 file. Invoked with two files, it treats the\n"
    "  first as the global segments, and the second as the segment\n"
    "  stream for a particular page. This is useful for examining\n"
    "  embedded streams.\n"
    "\n"
    "  available options:\n"
    "    -q --quiet     suppress diagnostic output\n"
    "    -d --dump      print the structure of the jbig2 file,\n"
    "                   rather than explicitly decoding\n"
    "    -o <file>	send decoded output to <file>. defaults to the same\n"
    " 			filename as the input with a different extension.\n"
    "    -h --help	this usage summary.\n"
    "\n"
  );
  
  return 1;
}

static int
parse_options(int argc, char *argv[], jbig2dec_params_t *params)
{
	static struct option long_options[] = {
		{"quiet", 0, NULL, 'q'},
		{"help", 0, NULL, 'h'},
		{"dump", 0, NULL, 'd'},
		{"output", 1, NULL, 'o'},
		{NULL, 0, NULL, 0}
	};
	int option_idx = 1;
	int option;

	while (1) {
		option = getopt_long(argc, argv,
			"qhdo:", long_options, &option_idx);
		if (option == -1) break;

		fprintf(stderr, "option '%c' value '%s'\n", option, optarg);
		switch (option) {
			case 0:	// unknown long option
				if (!params->verbose) fprintf(stdout,
					"unrecognized option: --%s\n",
					long_options[option_idx].name);
					break;
			case 'q':
				params->verbose=0;
				break;
			case 'h':
			case '?':
				print_usage();
				exit (0);
			case 'd':
				params->mode=dump;
				break;
			case 'o':
				params->output_file = strdup(optarg);
				break;
			default:
				if (!params->verbose) fprintf(stdout,
					"unrecognized option: -%c\n", option);
				break;
		}
	}
	fprintf(stderr, "final option index %d out of %d\n", optind, argc);
	return (optind);
}


static int
error_callback(void *error_callback_data, const char *buf, Jbig2Severity severity,
	       int32_t seg_idx)
{
    char *string;
    
    switch (severity) {
        case JBIG2_SEVERITY_DEBUG: string = "DEBUG"; break;;
        case JBIG2_SEVERITY_INFO: string = "info"; break;;
        case JBIG2_SEVERITY_WARNING: string = "WARNING"; break;;
        case JBIG2_SEVERITY_FATAL: string = "FATAL ERROR"; break;;
        default: string = "unknown message"; break;;
    }
    
    fprintf(stderr, "jbig2dec %s %s (segment 0x%0x)\n", string, buf, seg_idx);

    return 0;
}

int
main (int argc, char **argv)
{
  FILE *f = NULL, *f_page = NULL;
  Jbig2Ctx *ctx;
  uint8_t buf[4096];
  jbig2dec_params_t params = {usage,1,NULL};
  int filearg;

  filearg = parse_options(argc, argv, &params);

  if (params.output_file == NULL)
    {
#ifdef HAVE_LIBPNG
      params.output_file = "out.png";
#else
      params.output_file = "out.pbm";
#endif
    }
    
  if ((argc - filearg) == 1)
  // only one argument--open as a jbig2 file
    {
      char *fn = argv[filearg];

      f = fopen(fn, "rb");
      if (f == NULL)
	{
	  fprintf(stderr, "error opening %s\n", fn);
	  return 1;
	}
    }
  else if ((argc - filearg) == 2)
  // two arguments open as separate global and page streams
    {
      char *fn = argv[filearg];
      char *fn_page = argv[filearg+1];

      f = fopen(fn, "rb");
      if (f == NULL)
	{
	  fprintf(stderr, "error opening %s\n", fn);
	  return 1;
	}

      f_page = fopen(fn_page, "rb");
      if (f_page == NULL)
	{
	  fprintf(stderr, "error opening %s\n", fn_page);
	  return 1;
	}
    }
  else
  // any other number of arguments
    return print_usage();
    
  ctx = jbig2_ctx_new(NULL, f_page != NULL ? JBIG2_OPTIONS_EMBEDDED : 0,
		      NULL,
		      error_callback, NULL);

  // pull the whole file/global stream into memory
  for (;;)
    {
      int n_bytes = fread(buf, 1, sizeof(buf), f);
      if (n_bytes <= 0)
	break;
      jbig2_write(ctx, buf, n_bytes);
    }
  fclose(f);

  // if there's a local page stream read that in its entirety
  if (f_page != NULL)
    {
      Jbig2GlobalCtx *global_ctx = jbig2_make_global_ctx(ctx);
      ctx = jbig2_ctx_new(NULL, JBIG2_OPTIONS_EMBEDDED, global_ctx,
			 error_callback, NULL);
      for (;;)
	{
	  int n_bytes = fread(buf, 1, sizeof(buf), f_page);
	  if (n_bytes <= 0)
	    break;
	  jbig2_write(ctx, buf, n_bytes);
	}
      fclose(f_page);
      jbig2_global_ctx_free(global_ctx);
    }

  // retrieve and output the returned pages
  {
    Jbig2Image *image;
    
    while ((image = jbig2_get_page(ctx)) != NULL) {
        fprintf(stderr, "saving decoded page as '%s'\n", params.output_file);
#ifdef HAVE_LIBPNG
        jbig2_image_write_png_file(image, params.output_file);
#else
        jbig2_image_write_pbm_file(image, params.output_file);
#endif
        jbig2_release_page(ctx, image);
    }
  }
  
  jbig2_ctx_free(ctx);

  // fin
  return 0;
}
