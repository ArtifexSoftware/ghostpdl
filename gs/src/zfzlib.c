/* Copyright (C) 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* zfzlib.c */
/* zlib and Flate filter creation */
#include "ghost.h"
#include "errors.h"
#include "oper.h"
#include "idict.h"
#include "strimpl.h"
#include "spdiffx.h"
#include "spngpx.h"
#include "szlibx.h"
#include "ifilter.h"

/* Import the Predictor machinery from zfdecode.c and zfilter2.c. */
int filter_read_predictor(P4(os_ptr op, int npop,
  const stream_template *template, stream_state *st));
int filter_write_predictor(P4(os_ptr op, int npop,
  const stream_template *template, stream_state *st));

/* <source> zlibEncode/filter <file> */
/* <source> <dict_ignored> zlibEncode/filter <file> */
private int
zzlibE(os_ptr op)
{	stream_zlib_state zls;

	(*s_zlibE_template.set_defaults)((stream_state *)&zls);
	return filter_write(op, (r_has_type(op, t_dictionary) ? 1 : 0),
			    &s_zlibE_template, (stream_state *)&zls, 0);
}

/* <target> zlibDecode/filter <file> */
/* <target> <dict_ignored> zlibDecode/filter <file> */
private int
zzlibD(os_ptr op)
{	stream_zlib_state zls;

	(*s_zlibD_template.set_defaults)((stream_state *)&zls);
	return filter_read(op, (r_has_type(op, t_dictionary) ? 1 : 0),
			   &s_zlibD_template, (stream_state *)&zls, 0);
}

/* <source> FlateEncode/filter <file> */
/* <source> <dict> FlateEncode/filter <file> */
private int
zFlateE(os_ptr op)
{	stream_zlib_state zls;
	int npop;

	if ( r_has_type(op, t_dictionary) )
	  { check_dict_read(*op);
	    npop = 1;
	  }
	else
	  npop = 0;
	(*s_zlibE_template.set_defaults)((stream_state *)&zls);
	return filter_write_predictor(op, npop, &s_zlibE_template,
				      (stream_state *)&zls);
}

/* <target> FlateDecode/filter <file> */
/* <target> <dict> FlateDecode/filter <file> */
private int
zFlateD(os_ptr op)
{	stream_zlib_state zls;
	int npop;

	if ( r_has_type(op, t_dictionary) )
	  { check_dict_read(*op);
	    npop = 1;
	  }
	else
	  npop = 0;
	(*s_zlibD_template.set_defaults)((stream_state *)&zls);
	return filter_read_predictor(op, npop, &s_zlibD_template,
				     (stream_state *)&zls);
}

/* ------ Initialization procedure ------ */

BEGIN_OP_DEFS(zfzlib_op_defs) {
		op_def_begin_filter(),
	{"1zlibEncode", zzlibE},
	{"1zlibDecode", zzlibD},
	{"1FlateEncode", zFlateE},
	{"1FlateDecode", zFlateD},
END_OP_DEFS(0) }
