/*
    jbig2dec
    
    Copyright (C) 2001 artofcode LLC.
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    $Id: jbig2dec.c,v 1.12 2002/02/12 02:19:36 giles Exp $
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jbig2dec.h"

typedef struct _Jbig2SegmentHeader Jbig2SegmentHeader;
typedef struct _Jbig2SymbolDictionary Jbig2SymbolDictionary;
typedef struct _Jbig2PageInfo Jbig2PageInfo;

/* our main 'context' structure for decoding a jbig2 bitstream */
struct _Jbig2Ctx {
  FILE *f;
  int offset;
  int eof;
  
  int32 n_pages;
  byte flags;
};

/* useful masks for parsing the flags field */
#define JBIG2_FILE_FLAGS_SEQUENTIAL_ACCESS	0x01
#define JBIG2_FILE_FLAGS_PAGECOUNT_UNKNOWN	0x02


struct _Jbig2SegmentHeader {
  int32 segment_number;
  byte flags;
  int referred_to_segment_count;
  int32 page_association;
  int data_length;
};

struct _Jbig2SymbolDictionary {
  int16 flags;
  int8 SDAT_flags[8];
  byte SDRAT_flags[4];
  int32 SDNUMEXSYMS;
  int32 SDNUMNEWSYMS;
};

struct _Jbig2PageInfo {
	int32	height, width;	/* in pixels */
	int32	x_resolution,
			y_resolution;	/* in pixels per meter */
	int16	stripe_size;
	bool	striped;
	byte	flags;
};

int32
get_bytes (Jbig2Ctx *ctx, byte *buf, int size, int off)
{
  int n_bytes;

  fseek(ctx->f, off, SEEK_SET);
  n_bytes = fread(buf, 1, size, ctx->f);
  if (n_bytes < size)
    ctx->eof = TRUE;
  return n_bytes;
}

int16
get_int16 (Jbig2Ctx *ctx, int off)
{
  byte buf[2];

  get_bytes(ctx, buf, 2, off);
  return (buf[0] << 8) | buf[1];
}

byte
get_byte (Jbig2Ctx *ctx, int off)
{
  byte buf;
  
  get_bytes(ctx, &buf, 1, off);
  return buf;
}

int32
get_int32 (Jbig2Ctx *ctx, int off)
{
  byte buf[4];

  get_bytes(ctx, buf, 4, off);
  return (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
}

static Jbig2Ctx *
jbig2_open (FILE *f)
{
  byte buf[9];
  const byte header[8] = { 0x97, 0x4a, 0x42, 0x32, 0x0d, 0x0a, 0x1a, 0x0a };
  Jbig2Ctx *ctx;

  /* Annex D.4 */
  ctx = (Jbig2Ctx *)malloc(sizeof(Jbig2Ctx));
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

static Jbig2Ctx *
jbig2_open_embedded (FILE *f_globals, FILE *f_page)
{
  Jbig2Ctx *ctx;

  ctx = (Jbig2Ctx *)malloc(sizeof(Jbig2Ctx));
  ctx->f = f_globals;
  ctx->eof = 0;
  ctx->offset = 0;
  return ctx;
}

static Jbig2SegmentHeader *
jbig2_read_segment_header (Jbig2Ctx *ctx)
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

/* parse the symbol dictionary starting at ctx->offset
   a pointer to a new Jbig2SymbolDictionary struct is returned

   the ctx->offset pointer is not advanced; the caller must
   take care of that, using the data_length field of the
   segment header.
*/
static Jbig2SymbolDictionary *
jbig2_read_symbol_dictionary (Jbig2Ctx *ctx)
{
  Jbig2SymbolDictionary *result = (Jbig2SymbolDictionary *)malloc(sizeof(Jbig2SymbolDictionary));
  int offset = ctx->offset;
  bool SDHUFF, SDREFAGG, SDRTEMPLATE;
  int sdat_bytes;

  /* 7.4.2.1.1 */
  result->flags = get_int16(ctx, offset);
  offset += 2;

  SDHUFF = result->flags & 1;
  SDREFAGG = (result->flags >> 1) & 1;
  SDRTEMPLATE = (result->flags >> 12) & 1;

  /* FIXME: there are quite a few of these conditions to check */
  /* maybe #ifdef CONFORMANCE and a separate routine */
  if(!SDHUFF && (result->flags & 0x0006)) {
	printf("warning: SDHUFF is zero, but contrary to spec SDHUFFDH is not.\n");
  }
  if(!SDHUFF && (result->flags & 0x0018)) {
	printf("warning: SDHUFF is zero, but contrary to spec SDHUFFDW is not.\n");
  }

  /* 7.4.2.1.2 - Symbol dictionary AT flags */
  if (!SDHUFF)
    {
      int SDTEMPLATE = (result->flags >> 10) & 3;
      if (SDTEMPLATE == 0)
	sdat_bytes = 8;
      else
	sdat_bytes = 2;
    }
  else
    sdat_bytes = 0;
  get_bytes(ctx, result->SDAT_flags, sdat_bytes, offset);
  memset(&result->SDAT_flags + sdat_bytes, 0, 8 - sdat_bytes);
  offset += sdat_bytes;

  /* 7.4.2.1.3 - Symbol dictionary refinement AT flags */
  if (SDREFAGG && !SDRTEMPLATE)
    {
      get_bytes(ctx, result->SDRAT_flags, 4, offset);
      offset += 4;
    }

  /* 7.4.2.1.4 */
  result->SDNUMEXSYMS = get_int32(ctx, offset);

  /* 7.4.2.1.5 */
  result->SDNUMNEWSYMS = get_int32(ctx, offset + 4);
  offset += 8;

  /* hardwire for the first annex-h example */
  
  return result;
}

/* parse the page info segment data starting at ctx->offset
   a pointer to a new Jbig2PageInfo struct is returned

   the ctx->offset pointer is not advanced; the caller must
   take care of that, using the data_length field of the
   segment header.
*/
static Jbig2PageInfo *
jbig2_read_page_info (Jbig2Ctx *ctx) {
  Jbig2PageInfo *info = (Jbig2PageInfo *)malloc(sizeof(Jbig2PageInfo));
  int offset = ctx->offset;

	if (info == NULL) {
		printf("unable to allocate memory to parse page info segment\n");
		return NULL;
	}
	
	info->width = get_int32(ctx, offset);
	info->height = get_int32(ctx, offset + 4);
	offset += 8;
	
	info->x_resolution = get_int32(ctx, offset);
	info->y_resolution = get_int32(ctx, offset);
	offset += 8;
	
	get_bytes(ctx, &(info->flags), 1, offset++);
	
	{
	int16 striping = get_int16(ctx, offset);
	if (striping & 0x8000) {
		info->striped = TRUE;
		info->stripe_size = striping & 0x7FFF;
	} else {
		info->striped = FALSE;
		info->stripe_size = 0;	/* would info->height be better? */
	}
	offset += 2;
	}
	
	return info;
}

static void
dump_symbol_dictionary (Jbig2SymbolDictionary *sd)
{
  printf ("symbol dictionary: flags = %04x, %d new symbols, %d exported\n",
	  sd->flags, sd->SDNUMNEWSYMS, sd->SDNUMEXSYMS);
}

static void
dump_page_info(Jbig2PageInfo *info)
{
	printf("image is %dx%d ", info->width, info->height);
	if (info->x_resolution == 0) {
		printf("(unknown res) ");
	} else if (info->x_resolution == info->y_resolution) {
		printf("(%d ppm) ", info->x_resolution);
	} else {
		printf("(%dx%d ppm) ", info->x_resolution, info->y_resolution);
	}
	if (info->striped) {
		printf("\tmaximum stripe size: %d\n", info->stripe_size);
	} else {
		printf("\tno striping\n");
	}
}

static bool
dump_segment (Jbig2Ctx *ctx)
{
  Jbig2SegmentHeader *sh;
  Jbig2SymbolDictionary *sd;
  Jbig2PageInfo	*page_info;

  sh = jbig2_read_segment_header(ctx);
  if (sh == NULL)
    return TRUE;
  
  printf("segment %d (%d bytes)\t", sh->segment_number, sh->data_length);
  switch (sh->flags & 63)
    {
    case 0:
      sd = jbig2_read_symbol_dictionary(ctx);
	  printf("\n");
      dump_symbol_dictionary(sd);
      break;
	case 4:
		printf("intermediate text region:");
		break;
	case 6:
		printf("immediate text region:");
		break;
	case 7:
		printf("immediate lossless text region:");
		break;
	case 16:
		printf("pattern dictionary:");
		break;
	case 20:
		printf("intermediate halftone region:");
		break;
	case 22:
		printf("immediate halftone region:");
		break;
	case 23:
		printf("immediate lossless halftone region:");
		break;
	case 36:
		printf("intermediate generic region:");
		break;
	case 38:
		printf("immediate generic region:");
		break;
	case 39:
		printf("immediate lossless generic region:");
		break;
	case 40:
		printf("intermediate generic refinement region:");
		break;
	case 42:
		printf("immediate generic refinement region:");
		break;
	case 43:
		printf("immediate lossless generic refinement region:");
		break;
	case 48:
		page_info = jbig2_read_page_info(ctx);
		printf("page info:\n");
		if (page_info) dump_page_info(page_info);
		break;
	case 49:
		printf("end of page");
		break;
	case 50:
		printf("end of stripe");
		break;
    case 51:
      printf("end of file\n");
      return TRUE;
	  break;
	case 52:
		printf("profiles:");
		break;
	case 53:
		printf("tables:");
		break;
	case 62:
		printf("extension:");
		break;
	default:
		printf("UNKNOWN SEGMENT TYPE!!!");
    }
	printf("\tflags = %02x, page %d\n",
	  sh->flags, sh->page_association);

  ctx->offset += sh->data_length;
  return FALSE;
}

static void
dump_jbig2 (Jbig2Ctx *ctx)
{
  bool last;

  if (!ctx) return;
  
  printf("Number of pages = %d\n", ctx->n_pages);
  for (;;)
    {
      last = dump_segment(ctx);
      if (last)
	break;
    }
}

static int
usage (void)
{
  fprintf(stderr,
    "Usage: jbig2dec file.jbig2\n"
    "   or  jbig2dec global_stream page_stream\n"
    "\n"
    "  When invoked with a single file, it attempts to parse it as\n"
    "  a normal jbig2 file. Invoked with two files, it treats the\n"
    "  first as the global segments, and the second as the segment\n"
    "  stream for a particular page. This is useful for examining\n"
    "  embedded streams.\n"
    "\n"
  );
  
  return 1;
}

int
main (int argc, char **argv)
{
  FILE *f = NULL, *f_page = NULL;
  Jbig2Ctx *ctx;

  if (argc == 2)
    {
      char *fn = argv[1];

      f = fopen(fn, "rb");
      if (f == NULL)
	{
	  fprintf(stderr, "error opening %s\n", fn);
	  return 1;
	}
      ctx = jbig2_open (f);
      if (ctx != NULL)
	dump_jbig2(ctx);
      fclose(f);
    }
  else if (argc == 3)
    {
      char *fn = argv[1], *fn_page = argv[2];

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
      ctx = jbig2_open_embedded(f, f_page);
      if (ctx != NULL)
	dump_jbig2(ctx);
      ctx->f = f_page;
      ctx->offset = 0;
      ctx->eof = FALSE;
      dump_jbig2(ctx);
      fclose(f);
      fclose(f_page);
    }
  else    
    return usage();
    
  // control should never reach this point
  return 0;
}
