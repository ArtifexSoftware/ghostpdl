/*
    jbig2dec
    
    Copyright (C) 2001-2002 artofcode LLC.
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    $Id: jbig2dec.c,v 1.21 2002/06/18 13:40:29 giles Exp $
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

typedef enum {
    usage,dump,render
} jbig2dec_mode;

typedef struct {
	jbig2dec_mode mode;
	int verbose;
	char *output_file;
} jbig2dec_params_t;


#ifdef DEAD_CODE

static Jbig2Ctx_foo *
jbig2_open (FILE *f)
{
  byte buf[9];
  const byte header[8] = { 0x97, 0x4a, 0x42, 0x32, 0x0d, 0x0a, 0x1a, 0x0a };
  Jbig2Ctx_foo *ctx;

  /* Annex D.4 */
  ctx = (Jbig2Ctx_foo *)malloc(sizeof(Jbig2Ctx_foo));
  ctx->f = f;
  ctx->eof = FALSE;
  get_bytes(ctx, buf, 9, 0);
  if (memcmp(buf, header, 8))
    {
      printf("not a JBIG2 file\n");
      return NULL;
    }
  ctx->flags = buf[8];
  if (ctx->flags & JBIG2_FILE_FLAGS_PAGECOUNT_UNKNOWN)
    {
      ctx->offset = 9;	/* number of pages unknown */
	  ctx->n_pages = 0;
    }
  else
    {
      ctx->offset = 13;
      ctx->n_pages = get_int32(ctx, 9);
    }

  if(!(ctx->flags & JBIG2_FILE_FLAGS_SEQUENTIAL_ACCESS)) {
	printf("warning: random access header organization.\n");
	printf("we don't handle that yet.\n");
	free(ctx);
	return NULL;
  }
	
  return ctx;
}

static Jbig2Ctx_foo *
jbig2_open_embedded (FILE *f_globals, FILE *f_page)
{
  Jbig2Ctx_foo *ctx;

  ctx = (Jbig2Ctx_foo *)malloc(sizeof(Jbig2Ctx_foo));
  ctx->f = f_globals;
  ctx->eof = 0;
  ctx->offset = 0;
  return ctx;
}

static Jbig2SegmentHeader *
jbig2_read_segment_header (Jbig2Ctx_foo *ctx)
{
  Jbig2SegmentHeader *result = (Jbig2SegmentHeader *)malloc(sizeof(Jbig2SegmentHeader));
  int	offset = ctx->offset;
  byte	rtscarf;
  int32	rtscarf_long;
  int	referred_to_segment_count;

  /* 7.2.2 */
  result->segment_number = get_int32(ctx, offset);

  if (ctx->eof)
    {
      free(result);
      return NULL;
    }

  /* 7.2.3 */
  get_bytes(ctx, &result->flags, 1, offset + 4);

  /* 7.2.4 */
  get_bytes(ctx, &rtscarf, 1, offset + 5);
  if ((rtscarf & 0xe0) == 0xe0)
    {
		/* FIXME: we break on non-seekable streams with this,
		   but for now it's shorter */
		rtscarf_long = get_int32(ctx, offset + 5);
		referred_to_segment_count = (rtscarf_long & 0x1ffffffc) >> 2;
		offset += 5 + 4 + (referred_to_segment_count + 1)/8;
	}
  else
    {
      referred_to_segment_count = (rtscarf >> 5);
      offset += 6 + referred_to_segment_count;
    }
  result->referred_to_segment_count = referred_to_segment_count;
  /* todo: read referred to segment numbers */

  /* 7.2.6 */
  if (result->flags & 0x40) {
	result->page_association = get_int32(ctx, offset);
	offset += 4;
  } else {
	byte spa;
	get_bytes(ctx, &spa, 1, offset);
	result->page_association = spa;
	offset += 1;
  }
  
  /* 7.2.7 */
  result->data_length = get_int32 (ctx, offset);
  ctx->offset = offset + 4;

  return result;
}

#endif	/* DEAD_CODE */


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

  jbig2_ctx_free(ctx);

  // fin
  return 0;
}
