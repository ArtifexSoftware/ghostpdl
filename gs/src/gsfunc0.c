/* Copyright (C) 1997 Aladdin Enterprises.  All rights reserved.
  
  This file is part of Aladdin Ghostscript.
  
  Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
  or distributor accepts any responsibility for the consequences of using it,
  or for whether it serves any particular purpose or works at all, unless he
  or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
  License (the "License") for full details.
  
  Every copy of Aladdin Ghostscript must include a copy of the License,
  normally in a plain ASCII text file named PUBLIC.  The License grants you
  the right to copy, modify and redistribute Aladdin Ghostscript, but only
  under certain conditions described in the License.  Among other things, the
  License requires that the copyright notice and this notice be preserved on
  all copies.
*/

/* gsfunc0.c */
/* Implementation of FunctionType 0 (Sampled) Functions */
#include "math_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsfunc0.h"
#include "gxfunc.h"

typedef struct gs_function_Sd_s {
  gs_function_head_t head;
  gs_function_Sd_params_t params;
} gs_function_Sd_t;
private_st_function_Sd();

/* Define the maximum plausible number of inputs and outputs */
/* for a Sampled function. */
#define max_Sd_m 16
#define max_Sd_n 16

/* Get one set of sample values. */
private int
fn_get_samples(const gs_function_Sd_t *pfn, ulong offset, uint *samples)
{	int bps = pfn->params.BitsPerSample;
	int n = pfn->params.n;
	byte buf[max_Sd_n * 4];
	const byte *p;
	int i;

	data_source_access(&pfn->params.DataSource, offset >> 3,
			   (bps * n + 7) >> 3, buf, &p);
	switch ( bps ) {
	  case 1:
	    for ( i = 0; i < n; ++i ) {
	      samples[i] = (*p >> (~offset & 7)) & 1;
	      if ( !(++offset & 7) )
		p++;
	    }
	    break;
	  case 2:
	    for ( i = 0; i < n; ++i ) {
	      samples[i] = (*p >> (6 - (offset & 7))) & 3;
	      if ( !((offset += 2) & 7) )
		p++;
	    }
	    break;
	  case 4:
	    for ( i = 0; i < n; offset ^= 4, ++i )
	      samples[i] = (offset & 4 ? *p++ & 0xf : *p >> 4);
	    break;
	  case 8:
	    for ( i = 0; i < n; ++i )
	      samples[i] = *p++;
	    break;
	  case 12:
	    for ( i = 0; i < n; offset ^= 4, ++i )
	      if ( offset & 4 )
		samples[i] = ((*p & 0xf) << 8) + p[1], p += 2;
	      else
		samples[i] = (*p << 4) + (p[1] >> 4), p++;
	    break;
	  case 16:
	    for ( i = 0; i < n; p += 2, ++i )
	      samples[i] = (*p << 8) + p[1];
	    break;
	  case 24:
	    for ( i = 0; i < n; p += 3, ++i )
	      samples[i] = (*p << 16) + (p[1] << 8) + p[2];
	    break;
	  case 32:
	    for ( i = 0; i < n; p += 4, ++i )
	      samples[i] = (*p << 24) + (p[1] << 16) + (p[2] << 8) + p[3];
	    break;
	}
	return 0;
}

/* Calculate a result by multilinear interpolation. */
private void
fn_interpolate_linear(const gs_function_Sd_t *pfn, const float *values,
  const ulong *factors, uint *samples, ulong offset, int i)
{	if ( i == pfn->params.m )
	  fn_get_samples(pfn, offset, samples);
	else {
	  float fpart = values[i] - floor(values[i]);

	  fn_interpolate_linear(pfn, values, factors, samples, offset, i + 1);
	  if ( fpart != 0 ) {
	    int j;
	    uint samples1[max_Sd_n];

	    fn_interpolate_linear(pfn, values, factors, samples1,
			       offset + factors[i], i + 1);
	    for ( j = 0; j < pfn->params.n; ++j )
	      samples[j] += (samples1[j] - samples[j]) * fpart;
	  }
	}
}

/* Evaluate a Sampled function. */
private int
fn_Sd_evaluate(const gs_function_t *pfn_common, const float *in, float *out)
{	const gs_function_Sd_t *pfn = (const gs_function_Sd_t *)pfn_common;
	int bps = pfn->params.BitsPerSample;
	ulong offset = 0;
	int i;
	float encoded[max_Sd_m];
	ulong factors[max_Sd_m];
	uint samples[max_Sd_n];

	/* Encode the input values. */

	for ( i = 0; i < pfn->params.m; ++i ) {
	  float d0 = pfn->params.Domain[2 * i],
	    d1 = pfn->params.Domain[2 * i + 1];
	  float arg = in[i], enc;

	  if ( arg < d0 )
	    arg = d0;
	  else if ( arg > d1 )
	    arg = d1;
	  if ( pfn->params.Encode ) {
	    float e0 = pfn->params.Encode[2 * i];
	    float e1 = pfn->params.Encode[2 * i + 1];

	    enc = (arg - d0) * (e1 - e0) / (d1 - d0) + e0;
	    if ( enc < 0 )
	      encoded[i] = 0;
	    else if ( enc >= pfn->params.Size[i] - 1 )
	      encoded[i] = pfn->params.Size[i] - 1;
	    else
	      encoded[i] = enc;
	  } else {
	    /* arg is guaranteed to be in bounds, ergo so is enc */
	    encoded[i] = (arg - d0) * (pfn->params.Size[i] - 1) / (d1 - d0);
	  }
	}

	/* Look up and interpolate the output values. */

	{ ulong factor = bps * pfn->params.n;

	  for ( i = 0; i < pfn->params.m; factor *= pfn->params.Size[i++] )
	    offset += (factors[i] = factor) * (int)encoded[i];
	}
	/****** LINEAR INTERPOLATION ONLY ******/
	fn_interpolate_linear(pfn, encoded, factors, samples, offset, 0);

	/* Encode the output values. */

	for ( i = 0; i < pfn->params.n; offset += bps, ++i ) {
	  float d0, d1, r0, r1, value;

	  if ( pfn->params.Range )
	    r0 = pfn->params.Range[2 * i], r1 = pfn->params.Range[2 * i + 1];
	  else
	    r0 = 0, r1 = (1 << bps) - 1;
	  if ( pfn->params.Decode )
	    d0 = pfn->params.Decode[2 * i], d1 = pfn->params.Decode[2 * i + 1];
	  else
	    d0 = r0, d1 = r1;

	  value = samples[i] * (d1 - d0) / ((1 << bps) - 1) + d0;
	  if ( value < r0 )
	    out[i] = r0;
	  else if ( value > r1 )
	    out[i] = r1;
	  else
	    out[i] = value;
	}

	return 0;
}

/* Free the parameters of a Sampled function. */
void
gs_function_Sd_free_params(gs_function_Sd_params_t *params, gs_memory_t *mem)
{	gs_free_object(mem, (void *)params->Size, "Size"); /* break const */
	gs_free_object(mem, (void *)params->Decode, "Decode"); /* break const */
	gs_free_object(mem, (void *)params->Encode, "Encode"); /* break const */
	fn_common_free_params((gs_function_params_t *)params, mem);
}

/* Allocate and initialize a Sampled function. */
int
gs_function_Sd_init(gs_function_t **ppfn,
   const gs_function_Sd_params_t *params, gs_memory_t *mem)
{	static const gs_function_head_t function_Sd_head = {
	  function_type_Sampled,
	  (fn_evaluate_proc_t)fn_Sd_evaluate,
	  (fn_free_params_proc_t)gs_function_Sd_free_params,
	  fn_common_free
	};
	int i;

	*ppfn = 0;		/* in case of error */
	fn_check_mnDR(params, params->m, params->n);
	if ( params->m > max_Sd_m )
	  return_error(gs_error_limitcheck);
	switch ( params->Order ) {
	  case 0:		/* use default */
	  case 1: case 3:
	    break;
	  default:
	    return_error(gs_error_rangecheck);
	}
	switch ( params->BitsPerSample ) {
	  case 1: case 2: case 4: case 8: case 12: case 16: case 24: case 32:
	    break;
	  default:
	    return_error(gs_error_rangecheck);
	}
	for ( i = 0; i < params->m; ++i )
	  if ( params->Size[i] <= 0 )
	    return_error(gs_error_rangecheck);
	{ gs_function_Sd_t *pfn =
	    gs_alloc_struct(mem, gs_function_Sd_t, &st_function_Sd,
			    "gs_function_Sd_init");

	  if ( pfn == 0 )
	    return_error(gs_error_VMerror);
	  pfn->params = *params;
	  if ( params->Order == 0 )
	    pfn->params.Order = 1; /* default */
	  pfn->head = function_Sd_head;
	  *ppfn = (gs_function_t *)pfn;
	}
	return 0;
}
