/* Copyright (C) 1994, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* zfbcp.c */
/* (T)BCP filter creation */
#include "memory_.h"
#include "ghost.h"
#include "errors.h"
#include "oper.h"
#include "gsstruct.h"
#include "ialloc.h"
#include "stream.h"
#include "strimpl.h"
#include "sfilter.h"
#include "ifilter.h"

/* Define null handlers for the BCP out-of-band signals. */
private int
no_bcp_signal_interrupt(stream_state *st)
{	return 0;
}
private int
no_bcp_request_status(stream_state *st)
{	return 0;
}

/* <source> BCPEncode/filter <file> */
/* <source> <dict_ignored> BCPEncode/filter <file> */
private int
zBCPE(os_ptr op)
{	return filter_write_simple(op, &s_BCPE_template);
}

/* <target> BCPDecode/filter <file> */
/* <target> <dict_ignored> BCPDecode/filter <file> */
private int
zBCPD(os_ptr op)
{	stream_BCPD_state state;
	state.signal_interrupt = no_bcp_signal_interrupt;
	state.request_status = no_bcp_request_status;
	return filter_read(op, (r_has_type(op, t_dictionary) ? 1 : 0),
			   &s_BCPD_template, (stream_state *)&state, 0);
}

/* <source> TBCPEncode/filter <file> */
/* <source> <dict_ignored> TBCPEncode/filter <file> */
private int
zTBCPE(os_ptr op)
{	return filter_write_simple(op, &s_TBCPE_template);
}

/* <target> TBCPDecode/filter <file> */
/* <target> <dict_ignored> TBCPDecode/filter <file> */
private int
zTBCPD(os_ptr op)
{	stream_BCPD_state state;
	state.signal_interrupt = no_bcp_signal_interrupt;
	state.request_status = no_bcp_request_status;
	return filter_read(op, (r_has_type(op, t_dictionary) ? 1 : 0),
			   &s_TBCPD_template, (stream_state *)&state, 0);
}

/* ------ Initialization procedure ------ */

BEGIN_OP_DEFS(zfbcp_op_defs) {
		op_def_begin_filter(),
	{"1BCPEncode", zBCPE},
	{"1BCPDecode", zBCPD},
	{"1TBCPEncode", zTBCPE},
	{"1TBCPDecode", zTBCPD},
END_OP_DEFS(0) }
