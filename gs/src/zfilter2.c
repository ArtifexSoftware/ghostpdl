/* Copyright (C) 1991, 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* zfilter2.c */
/* Additional filter creation */
#include "memory_.h"
#include "ghost.h"
#include "errors.h"
#include "oper.h"
#include "gsstruct.h"
#include "ialloc.h"
#include "idict.h"
#include "idparam.h"
#include "store.h"
#include "strimpl.h"
#include "sfilter.h"
#include "scfx.h"
#include "slzwx.h"
#include "spdiffx.h"
#include "spngpx.h"
#include "ifilter.h"

/* Import setup code from zfdecode.c */
int zcf_setup(P2(os_ptr op, stream_CF_state *pcfs));
int zlz_setup(P2(os_ptr op, stream_LZW_state *plzs));
int zpd_setup(P2(os_ptr op, stream_PDiff_state *ppds));
int zpp_setup(P2(os_ptr op, stream_PNGP_state *ppps));

/* ------ CCITTFaxEncode filter ------ */

/* <target> <dict> CCITTFaxEncode/filter <file> */
private int
zCFE(os_ptr op)
{	stream_CFE_state cfs;
	int code;

	check_type(*op, t_dictionary);
	check_dict_read(*op);
	code = zcf_setup(op, (stream_CF_state *)&cfs);
	if ( code < 0 )
	  return code;
	return filter_write(op, 1, &s_CFE_template, (stream_state *)&cfs, 0);
}

/* ------ Common setup for possibly pixel-oriented encoding filters ------ */

int
filter_write_predictor(os_ptr op, int npop, const stream_template *template,
  stream_state *st)
{	int predictor, code;
	stream_PDiff_state pds;
	stream_PNGP_state pps;

	if ( r_has_type(op, t_dictionary) )
	  { if ( (code = dict_int_param(op, "Predictor", 0, 15, 1, &predictor)) < 0 )
	      return code;
	    switch ( predictor )
	      {
	      case 0:		/* identity */
		predictor = 1;
	      case 1:		/* identity */
		break;
	      case 2:		/* componentwise horizontal differencing */
		code = zpd_setup(op, &pds);
		break;
	      case 10: case 11: case 12: case 13: case 14: case 15:
				/* PNG prediction */
		code = zpp_setup(op, &pps);
		break;
	      default:
		return_error(e_rangecheck);
	      }
	    if ( code < 0 )
	      return code;
	  }
	else
	  predictor = 1;
	if ( predictor == 1 )
	  return filter_write(op, npop, template, st, 0);
	{ /* We need to cascade filters. */
	  ref rtarget, rdict, rfd;
	  int code;

	  /* Save the operands, just in case. */
	  ref_assign(&rtarget, op - 1);
	  ref_assign(&rdict, op);
	  code = filter_write(op, 1, template, st, 0);
	  if ( code < 0 )
	    return code;
	  /* filter_write changed osp.... */
	  op = osp;
	  ref_assign(&rfd, op);
	  code =
	    (predictor == 2 ?
	     filter_read(op, 0, &s_PDiffE_template, (stream_state *)&pds, 0) :
	     filter_read(op, 0, &s_PNGPE_template, (stream_state *)&pps, 0));
	  if ( code < 0 )
	    { /* Restore the operands.  Don't bother trying to clean up */
	      /* the first stream. */
	      osp = ++op;
	      ref_assign(op - 1, &rtarget);
	      ref_assign(op, &rdict);
	      return code;
	    }
	  filter_mark_temp(&rfd, 2);	/* Mark the compression stream as temporary. */
	  return code;
	}
}

/* ------ LZW encoding filter ------ */

/* <target> LZWEncode/filter <file> */
/* <target> <dict> LZWEncode/filter <file> */
/* Note: the default implementation of this filter, in slzwce.c, */
/* does not use any algorithms that could reasonably be claimed */
/* to be subject to Unisys' Welch Patent. */
private int
zLZWE(os_ptr op)
{	stream_LZW_state lzs;
	int code = zlz_setup(op, &lzs);

	if ( code < 0 )
	  return code;
	return filter_write_predictor(op, code, &s_LZWE_template,
				      (stream_state *)&lzs);
}

/* ================ Initialization procedure ================ */

BEGIN_OP_DEFS(zfilter2_op_defs) {
		op_def_begin_filter(),
	{"2CCITTFaxEncode", zCFE},
	{"1LZWEncode", zLZWE},
END_OP_DEFS(0) }
