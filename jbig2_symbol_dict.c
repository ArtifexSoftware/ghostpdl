/*
    jbig2dec
    
    Copyright (C) 2001-2002 artofcode LLC.
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    $Id: jbig2_symbol_dict.c,v 1.4 2002/06/15 14:12:50 giles Exp $
*/

#include <stddef.h>
#include <stdint.h>

#include "jbig2.h"
#include "jbig2_priv.h"
#include "jbig2_arith.h"
#include "jbig2_arith_int.h"
#include "jbig2_generic.h"
#include "jbig2_symbol_dict.h"

#ifdef GILES

struct _Jbig2SymbolDictionary {
  int16 flags;
  int8 SDAT_flags[8];
  byte SDRAT_flags[4];
  int32 SDNUMEXSYMS;
  int32 SDNUMNEWSYMS;
};

/* parse the symbol dictionary starting at ctx->offset
   a pointer to a new Jbig2SymbolDictionary struct is returned

   the ctx->offset pointer is not advanced; the caller must
   take care of that, using the data_length field of the
   segment header.
*/
static Jbig2SymbolDictionary *
jbig2_read_symbol_dictionary (Jbig2Ctx_foo *ctx)
{
  Jbig2SymbolDictionary *result = (Jbig2SymbolDictionary *)malloc(sizeof(Jbig2SymbolDictionary));
  int offset = ctx->offset;
  bool SDHUFF, SDREFAGG, SDRTEMPLATE;
  int SDTEMPLATE;
  int sdat_bytes;

  /* 7.4.2.1.1 */
  result->flags = get_int16(ctx, offset);
  offset += 2;

  SDHUFF = result->flags & 1;
  SDREFAGG = (result->flags >> 1) & 1;
  SDTEMPLATE = (result->flags >> 10) & 3;
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

#endif /* GILES */

/* Table 13 */
typedef struct {
  bool SDHUFF;
  bool SDREFAGG;
  int32_t SDNUMINSYMS;
  /* SDINSYMS */
  uint32_t SDNUMNEWSYMS;
  uint32_t SDNUMEXSYMS;
  /* SDHUFFDH */
  /* SDHUFFDW */
  /* SDHUFFBMSIZE */
  /* SDHUFFAGGINST */
  int SDTEMPLATE;
  int8_t sdat[8];
  bool SDRTEMPLATE;
  int8_t sdrat[4];
} Jbig2SymbolDictParams;

/* 6.5 */
int
jbig2_decode_symbol_dict(Jbig2Ctx *ctx,
			 int32_t seg_number,
			 const Jbig2SymbolDictParams *params,
			 const byte *data, size_t size,
			 Jbig2ArithCx *GB_stats)
{
  int32_t HCHEIGHT;
  uint32_t NSYMSDECODED;
  int32_t SYMWIDTH, TOTWIDTH;
  uint32_t HCFIRSTSYM;
  Jbig2ArithState *as = NULL;
  Jbig2ArithIntCtx *IADH = NULL;
  Jbig2ArithIntCtx *IADW = NULL;
  int code;

  /* 6.5.5 (3) */
  HCHEIGHT = 0;
  NSYMSDECODED = 0;

  if (!params->SDHUFF)
    {
      Jbig2WordStream *ws = jbig2_word_stream_buf_new(ctx, data, size);
      as = jbig2_arith_new(ctx, ws);
      IADH = jbig2_arith_int_ctx_new(ctx);
      IADW = jbig2_arith_int_ctx_new(ctx);
    }

  /* 6.5.5 (4a) */
  while (NSYMSDECODED < params->SDNUMNEWSYMS)
    {
      int32_t HCDH, DW;

      /* 6.5.6 */
      if (params->SDHUFF)
	; /* todo */
      else
	{
	  code = jbig2_arith_int_decode(IADH, as, &HCDH);
	}

      /* 6.5.5 (4b) */
      HCHEIGHT = HCHEIGHT + HCDH;
      SYMWIDTH = 0;
      TOTWIDTH = 0;
      HCFIRSTSYM = NSYMSDECODED;

      if (HCHEIGHT < 0)
	/* todo: mem cleanup */
	return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, seg_number,
			   "Invalid HCHEIGHT value");

      printf ("HCHEIGHT = %d\n", HCHEIGHT);
      for (;;)
	{
	  /* 6.5.7 */
	  if (params->SDHUFF)
	    ; /* todo */
	  else
	    {
	      code = jbig2_arith_int_decode(IADW, as, &DW);
	    }

	  /* 6.5.5 (4c.i) */
	  if (code == 1)
	    break;
	  SYMWIDTH = SYMWIDTH + DW;
	  TOTWIDTH = TOTWIDTH + SYMWIDTH;
	  if (SYMWIDTH < 0)
	    /* todo: mem cleanup */
	    return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, seg_number,
			       "Invalid SYMWIDTH value");
	  printf ("SYMWIDTH = %d\n", SYMWIDTH);

	  /* 6.5.5 (4c.ii) */
	  if (!params->SDHUFF || params->SDREFAGG)
	    {
	      /* 6.5.8 */
	      if (!params->SDREFAGG)
		{
		  Jbig2GenericRegionParams region_params;
		  int sdat_bytes;
		  byte *gbreg;

		  /* Table 16 */
		  region_params.MMR = 0;
		  region_params.GBW = SYMWIDTH;
		  region_params.GBH = HCHEIGHT;
		  region_params.GBTEMPLATE = params->SDTEMPLATE;
		  region_params.TPGDON = 0;
		  region_params.USESKIP = 0;
		  sdat_bytes = params->SDTEMPLATE == 0 ? 8 : 2;
		  memcpy(region_params.gbat, params->sdat, sdat_bytes);

		  gbreg = jbig2_alloc(ctx->allocator,
				      ((SYMWIDTH + 7) >> 3) * HCHEIGHT);

		  code = jbig2_decode_generic_region(ctx, seg_number,
						     &region_params,
						     as,
						     gbreg, GB_stats);
		  /* todo: handle errors */
		  /* todo: stash gbreg in SDNEWSYMS */
						     
		}
	    }

	  /* 6.5.5 (4c.iv) */
	  NSYMSDECODED = NSYMSDECODED + 1;
	  printf ("%d of %d decoded\n", NSYMSDECODED, params->SDNUMNEWSYMS);
	}
    }

  jbig2_free(ctx->allocator, GB_stats);

  return 0;
}

/* 7.4.2 */
int
jbig2_symbol_dictionary(Jbig2Ctx *ctx, Jbig2SegmentHeader *sh,
			const byte *segment_data)
{
  Jbig2SymbolDictParams params;
  uint16_t flags;
  int sdat_bytes;
  int offset;
  Jbig2ArithCx *GB_stats = NULL;

  if (sh->data_length < 10)
    goto too_short;

  /* 7.4.2.1.1 */
  flags = jbig2_get_int16(segment_data);
  params.SDHUFF = flags & 1;
  params.SDREFAGG = (flags >> 1) & 1;
  params.SDTEMPLATE = (flags >> 10) & 3;
  params.SDRTEMPLATE = (flags >> 12) & 1;

  if (params.SDHUFF)
    return 0;

  /* FIXME: there are quite a few of these conditions to check */
  /* maybe #ifdef CONFORMANCE and a separate routine */
  if(!params.SDHUFF && (flags & 0x000c))
    {
      jbig2_error(ctx, JBIG2_SEVERITY_WARNING, sh->segment_number,
		  "SDHUFF is zero, but contrary to spec SDHUFFDH is not.");
    }
  if(!params.SDHUFF && (flags & 0x0030))
    {
      jbig2_error(ctx, JBIG2_SEVERITY_WARNING, sh->segment_number,
		  "SDHUFF is zero, but contrary to spec SDHUFFDW is not.");
    }

  /* 7.4.2.1.2 */
  sdat_bytes = params.SDHUFF ? 0 : params.SDTEMPLATE == 0 ? 8 : 2;
  memcpy(params.sdat, segment_data + 2, sdat_bytes);
  offset = 2 + sdat_bytes;

  /* 7.4.2.1.3 */
  if (params.SDREFAGG && !params.SDRTEMPLATE)
    {
      if (offset + 4 > sh->data_length)
	goto too_short;
      memcpy(params.sdrat, segment_data + offset, 4);
      offset += 4;
    }

  if (offset + 8 > sh->data_length)
    goto too_short;

  /* 7.4.2.1.4 */
  params.SDNUMEXSYMS = jbig2_get_int32(segment_data + offset);
  /* 7.4.2.1.5 */
  params.SDNUMNEWSYMS = jbig2_get_int32(segment_data + offset + 4);
  offset += 8;

  jbig2_error(ctx, JBIG2_SEVERITY_INFO, sh->segment_number,
	      "symbol dictionary, flags=%04x, %d exported syms, %d new syms",
	      flags, params.SDNUMEXSYMS, params.SDNUMNEWSYMS);

  /* 7.4.2.2 (4) */
  if (!params.SDHUFF)
    {
      int stats_size = params.SDTEMPLATE == 0 ? 65536 :
	params.SDTEMPLATE == 1 ? 8192 : 1024;
      GB_stats = jbig2_alloc(ctx->allocator, stats_size);
      memset(GB_stats, 0, stats_size);
    }

  return jbig2_decode_symbol_dict(ctx, sh->segment_number,
				  &params,
				  segment_data + offset,
				  sh->data_length - offset,
				  GB_stats);

  /* todo: retain or free GB_stats */
  
 too_short:
  return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, sh->segment_number,
		     "Segment too short");
}
