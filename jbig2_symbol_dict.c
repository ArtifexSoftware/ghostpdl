/*
    jbig2dec
    
    Copyright (C) 2001-2003 artofcode LLC.
    
    This software is distributed under license and may not
    be copied, modified or distributed except as expressly
    authorized under the terms of the license contained in
    the file LICENSE in this distribution.
                                                                                
    For information on commercial licensing, go to
    http://www.artifex.com/licensing/ or contact
    Artifex Software, Inc.,  101 Lucas Valley Road #110,
    San Rafael, CA  94903, U.S.A., +1(415)492-9861.

    $Id$
    
    symbol dictionary segment decode and support
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif 
#include "os_types.h"

#include <stddef.h>
#include <string.h> /* memset() */
#include <stdio.h> /* debugging, remove me! */

#include "jbig2.h"
#include "jbig2_priv.h"
#include "jbig2_arith.h"
#include "jbig2_arith_int.h"
#include "jbig2_generic.h"
#include "jbig2_symbol_dict.h"

#if defined(OUTPUT_PBM) || defined(DUMP_SYMDICT)
#include <stdio.h>
#include "jbig2_image.h"
#endif

/* Table 13 */
typedef struct {
  bool SDHUFF;
  bool SDREFAGG;
  int32_t SDNUMINSYMS;
  Jbig2SymbolDict *SDINSYMS;
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


/* Utility routines */

#ifdef DUMP_SYMDICT
void
jbig2_dump_symbol_dict(Jbig2SymbolDict *dict)
{
    int index;
    char filename[24];
    
    fprintf(stderr, "dumping symbol dict as %d individual png files\n", dict->n_symbols);
    for (index = 0; index < dict->n_symbols; index++) {
        snprintf(filename, sizeof(filename), "symbol_%04d.png", index);
#ifdef HAVE_LIBPNG
        jbig2_image_write_png_file(dict->glyphs[index], filename);
#else
        jbig2_image_write_pbm_file(dict->glyphs[index], filename);
#endif
    }
}
#endif /* DUMP_SYMDICT */

/* return a new empty symbol dict */
Jbig2SymbolDict *
jbig2_sd_new(Jbig2Ctx *ctx, int n_symbols)
{
   Jbig2SymbolDict *new = NULL;
   
   new = (Jbig2SymbolDict *)jbig2_alloc(ctx->allocator,
   				sizeof(Jbig2SymbolDict));
   if (new != NULL) {
     new->glyphs = (Jbig2Image **)jbig2_alloc(ctx->allocator,
     				n_symbols*sizeof(Jbig2Image*));
     new->n_symbols = n_symbols;
   }
   if (new->glyphs != NULL) {
     memset(new->glyphs, 0, n_symbols*sizeof(Jbig2Image*));
   } else {
     jbig2_free(ctx->allocator, new);
     new = NULL;
   }
   
   return new;
}

/* release the memory associated with a symbol dict */
void
jbig2_sd_release(Jbig2Ctx *ctx, Jbig2SymbolDict *dict)
{
   int i;
   
   if (dict == NULL) return;
   for (i = 0; i < dict->n_symbols; i++)
     if (dict->glyphs[i]) jbig2_image_release(ctx, dict->glyphs[i]);
   jbig2_free(ctx->allocator, dict);
}

/* get a particular glyph by index */
Jbig2Image *
jbig2_sd_glyph(Jbig2SymbolDict *dict, unsigned int id)
{
   if (dict == NULL) return NULL;
   return dict->glyphs[id];
}

/* count the number of dictionary segments referred to by the given segment */
int
jbig2_sd_count_referred(Jbig2Ctx *ctx, Jbig2Segment *segment)
{
    int index;
    Jbig2Segment *rsegment;
    int n_dicts = 0;
 
    for (index = 0; index < segment->referred_to_segment_count; index++) {
        rsegment = jbig2_find_segment(ctx, segment->referred_to_segments[index]);
        if (rsegment && ((rsegment->flags & 63) == 0)) {
            n_dicts++;
            n_dicts+= jbig2_sd_count_referred(ctx, rsegment);
        }
    }
                                                                                
    return (n_dicts);
}

/* return an array of pointers to symbol dictionaries referred to by the given segment */
Jbig2SymbolDict **
jbig2_sd_list_referred(Jbig2Ctx *ctx, Jbig2Segment *segment)
{
    int index;
    Jbig2Segment *rsegment;
    Jbig2SymbolDict **dicts, **rdicts;
    int n_dicts = jbig2_sd_count_referred(ctx, segment);
    int dindex = 0;
                                                     
    dicts = jbig2_alloc(ctx->allocator, sizeof(Jbig2SymbolDict *) * n_dicts);
    for (index = 0; index < segment->referred_to_segment_count; index++) {
        rsegment = jbig2_find_segment(ctx, segment->referred_to_segments[index]);
        if (rsegment && ((rsegment->flags & 63) == 0)) {
            /* recurse for imported symbols */
            int j, n_rdicts = jbig2_sd_count_referred(ctx, rsegment);
            if (n_rdicts > 0) {
                rdicts = jbig2_sd_list_referred(ctx, rsegment);
                for (j = 0; j < n_rdicts; j++)
                    dicts[dindex++] = rdicts[j];
                jbig2_free(ctx->allocator, rdicts);
            }
            /* add this referred to symbol dictionary */
            dicts[dindex++] = (Jbig2SymbolDict *)rsegment->result;
        }
    }
                                                                                
    if (dindex != n_dicts) {
        /* should never happen */
        jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
            "counted %d symbol dictionaries but build a list with %d.\n",
            n_dicts, dindex);
    }
                                                                                
    return (dicts);
}

/* generate a new symbol dictionary by concatenating a list of 
   existing dictionaries */
Jbig2SymbolDict *
jbig2_sd_cat(Jbig2Ctx *ctx, int n_dicts, Jbig2SymbolDict **dicts)
{
  int i,j,k, symbols;
  Jbig2SymbolDict *new = NULL;
  

  /* count the imported symbols and allocate a new array */
  symbols = 0;
  for(i = 0; i < n_dicts; i++)
    symbols += dicts[i]->n_symbols;

  /* fill a new array with cloned glyph pointers */
  new = jbig2_sd_new(ctx, symbols);
  if (new != NULL) {
    k = 0;
    for (i = 0; i < n_dicts; i++)
      for (j = 0; j < dicts[i]->n_symbols; j++)
        new->glyphs[k++] = jbig2_image_clone(ctx, dicts[i]->glyphs[j]);
  }
  
  return new;
}


/* Decoding routines */

/* 6.5 */
static Jbig2SymbolDict *
jbig2_decode_symbol_dict(Jbig2Ctx *ctx,
			 Jbig2Segment *segment,
			 const Jbig2SymbolDictParams *params,
			 const byte *data, size_t size,
			 Jbig2ArithCx *GB_stats)
{
  Jbig2SymbolDict *SDNEWSYMS;
  Jbig2SymbolDict *SDEXSYMS;
  int32_t HCHEIGHT;
  uint32_t NSYMSDECODED;
  int32_t SYMWIDTH, TOTWIDTH;
  uint32_t HCFIRSTSYM;
  Jbig2ArithState *as = NULL;
  Jbig2ArithIntCtx *IADH = NULL;
  Jbig2ArithIntCtx *IADW = NULL;
  Jbig2ArithIntCtx *IAEX = NULL;
  int code = 0;

  /* 6.5.5 (3) */
  HCHEIGHT = 0;
  NSYMSDECODED = 0;

  if (!params->SDHUFF)
    {
      Jbig2WordStream *ws = jbig2_word_stream_buf_new(ctx, data, size);
      as = jbig2_arith_new(ctx, ws);
      IADH = jbig2_arith_int_ctx_new(ctx);
      IADW = jbig2_arith_int_ctx_new(ctx);
      IAEX = jbig2_arith_int_ctx_new(ctx);
    }

  SDNEWSYMS = jbig2_sd_new(ctx, params->SDNUMNEWSYMS);
  
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
        {
	  /* todo: mem cleanup */
	  code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
			   "Invalid HCHEIGHT value");
          return NULL;
        }
#ifdef JBIG2_DEBUG
      jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number,
        "HCHEIGHT = %d", HCHEIGHT);
#endif        
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
            {
	      /* todo: mem cleanup */
              code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
                "Invalid SYMWIDTH value (%d) at symbol %d", SYMWIDTH, NSYMSDECODED+1);
              return NULL;
            }
#ifdef JBIG2_DEBUG
	  jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number,
            "SYMWIDTH = %d", SYMWIDTH);
#endif
	  /* 6.5.5 (4c.ii) */
	  if (!params->SDHUFF || params->SDREFAGG)
	    {
	      /* 6.5.8 */
	      if (!params->SDREFAGG)
		{
		  Jbig2GenericRegionParams region_params;
		  int sdat_bytes;
		  Jbig2Image *image;

		  /* Table 16 */
		  region_params.MMR = 0;
		  region_params.GBTEMPLATE = params->SDTEMPLATE;
		  region_params.TPGDON = 0;
		  region_params.USESKIP = 0;
		  sdat_bytes = params->SDTEMPLATE == 0 ? 8 : 2;
		  memcpy(region_params.gbat, params->sdat, sdat_bytes);

		  image = jbig2_image_new(ctx, SYMWIDTH, HCHEIGHT);

		  code = jbig2_decode_generic_region(ctx, segment, &region_params,
						     as, image, GB_stats);
		  /* todo: handle errors */
                  
                  SDNEWSYMS->glyphs[NSYMSDECODED] = image;

#ifdef OUTPUT_PBM
                  jbig2_image_write_pbm(image, stdout);
#endif
		}
	    }

	  /* 6.5.5 (4c.iv) */
	  NSYMSDECODED = NSYMSDECODED + 1;
#ifdef JBIG2_DEBUG
	  jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number,
            "%d of %d decoded", NSYMSDECODED, params->SDNUMNEWSYMS);
#endif
	}
    }

  jbig2_free(ctx->allocator, GB_stats);
  
  /* 6.5.10 */
  SDEXSYMS = jbig2_sd_new(ctx, params->SDNUMEXSYMS);
  {
    int i = 0;
    int j = 0;
    int k, m, exrunlength, exflag = 0;
    
    if (params->SDINSYMS != NULL)
      m = params->SDINSYMS->n_symbols;
    else
      m = 0;
    fprintf(stderr, "building export symbol dictionary\n");
    fprintf(stderr, "\tinput: %d symbols, decoded: %d symbols\n",
    	params->SDNUMINSYMS, NSYMSDECODED);
    fprintf(stderr, "\tto export: %d symbols\n", params->SDNUMEXSYMS);
    while (j < params->SDNUMEXSYMS) {
      if (params->SDHUFF)
      	/* FIXME: implement reading from huff table B.1 */
        exrunlength = params->SDNUMEXSYMS;
      else
        code = jbig2_arith_int_decode(IAEX, as, &exrunlength);
      fprintf(stderr, "  read runlength %d symbols (exflag = %d)\n",
      	exrunlength, exflag);
      for(k = 0; k < exrunlength; k++)
        if (exflag) {
          SDEXSYMS->glyphs[j++] = (i < m) ? 
            jbig2_image_clone(ctx, params->SDINSYMS->glyphs[i]) :
            jbig2_image_clone(ctx, SDNEWSYMS->glyphs[i-m]);
          i++;
        }
        exflag = !exflag;
        fprintf(stderr, " export index %d; import index %d\n", j, i);
    }
  }
  
  jbig2_sd_release(ctx, SDNEWSYMS);
  
  return SDEXSYMS;
}

/* 7.4.2 */
int
jbig2_symbol_dictionary(Jbig2Ctx *ctx, Jbig2Segment *segment,
			const byte *segment_data)
{
  Jbig2SymbolDictParams params;
  uint16_t flags;
  int sdat_bytes;
  int offset;
  Jbig2ArithCx *GB_stats = NULL;
  
  if (segment->data_length < 10)
    goto too_short;

  /* 7.4.2.1.1 */
  flags = jbig2_get_int16(segment_data);
  params.SDHUFF = flags & 1;
  params.SDREFAGG = (flags >> 1) & 1;
  params.SDTEMPLATE = (flags >> 10) & 3;
  params.SDRTEMPLATE = (flags >> 12) & 1;

  if (params.SDHUFF) {
    jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
        "symbol dictionary uses the Huffman encoding variant (NYI)");
    return 0;
  }

  /* FIXME: there are quite a few of these conditions to check */
  /* maybe #ifdef CONFORMANCE and a separate routine */
  if(!params.SDHUFF && (flags & 0x000c))
    {
      jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
		  "SDHUFF is zero, but contrary to spec SDHUFFDH is not.");
    }
  if(!params.SDHUFF && (flags & 0x0030))
    {
      jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
		  "SDHUFF is zero, but contrary to spec SDHUFFDW is not.");
    }

  if (flags & 0x0080)
    {
      jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
        "bitmap coding context is used (NYI) symbol data likely to be garbage!");
    }

  /* 7.4.2.1.2 */
  sdat_bytes = params.SDHUFF ? 0 : params.SDTEMPLATE == 0 ? 8 : 2;
  memcpy(params.sdat, segment_data + 2, sdat_bytes);
  offset = 2 + sdat_bytes;

  /* 7.4.2.1.3 */
  if (params.SDREFAGG && !params.SDRTEMPLATE)
    {
      if (offset + 4 > segment->data_length)
	goto too_short;
      memcpy(params.sdrat, segment_data + offset, 4);
      offset += 4;
    }

  if (offset + 8 > segment->data_length)
    goto too_short;

  /* 7.4.2.1.4 */
  params.SDNUMEXSYMS = jbig2_get_int32(segment_data + offset);
  /* 7.4.2.1.5 */
  params.SDNUMNEWSYMS = jbig2_get_int32(segment_data + offset + 4);
  offset += 8;

  jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number,
	      "symbol dictionary, flags=%04x, %d exported syms, %d new syms",
	      flags, params.SDNUMEXSYMS, params.SDNUMNEWSYMS);

  /* 7.4.2.2 (2) */
  {
    int n_dicts = jbig2_sd_count_referred(ctx, segment);
    Jbig2SymbolDict **dicts = NULL;
  
    params.SDINSYMS = NULL;	
    if (n_dicts > 0) {
      dicts = jbig2_sd_list_referred(ctx, segment);
      params.SDINSYMS = jbig2_sd_cat(ctx, n_dicts, dicts);
    }
    if (params.SDINSYMS != NULL) {
      params.SDNUMINSYMS = params.SDINSYMS->n_symbols;
    } else {
     params.SDNUMINSYMS = 0;
    }
  }
  
  /* 7.4.2.2 (4) */
  if (!params.SDHUFF)
    {
      int stats_size = params.SDTEMPLATE == 0 ? 65536 :
	params.SDTEMPLATE == 1 ? 8192 : 1024;
      GB_stats = jbig2_alloc(ctx->allocator, stats_size);
      memset(GB_stats, 0, stats_size);
    }

  if (flags & 0x0100)
    {
      jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
        "segment marks bitmap coding context as retained (NYI)");
    }

  segment->result = (void *)jbig2_decode_symbol_dict(ctx, segment,
				  &params,
				  segment_data + offset,
				  segment->data_length - offset,
				  GB_stats);
#ifdef DUMP_SYMDICT
  if (segment->result) jbig2_dump_symbol_dict(segment->result);
#endif

  /* todo: retain or free GB_stats */

  return (segment->result != NULL) ? 0 : -1;

 too_short:
  return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
		     "Segment too short");
}
