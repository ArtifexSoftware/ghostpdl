/* Copyright (C) 2003 artofcode LLC.  All rights reserved.
  
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

/* this is the ps interpreter interface to the jbig2decode filter
   used for (1bpp) scanned image compression. PDF only specifies
   a decoder filter, and we don't currently implement anything else */

#include "memory_.h"
#include "ghost.h"
#include "oper.h"
#include "gsstruct.h"
#include "ialloc.h"
#include "idict.h"
#include "stream.h"
#include "strimpl.h"
#include "ifilter.h"
#include "sjbig2.h"

/* <source> <dict> /JBIG2Decode <file> */

private int
z_jbig2decode(i_ctx_t * i_ctx_p)
{
    os_ptr op = osp;		/* i_ctx_p->op_stack.stack.p defined in osstack.h */
    ref *sop = NULL;
    stream_jbig2decode_state state;

#if 0
    /* initialize the fields for safety */
    if (state.template->set_defaults)
	(*state.template->set_defaults) ((stream_state *) & state);
#endif
    
    /* extract the global context reference, if any, from the parameter dictionary */
    check_type(*op, t_dictionary);
    check_dict_read(*op);
    if (dict_find_string(op, "JBIG2Globals", &sop) <= 0)
	return_error(e_rangecheck);
    
    /* we pass npop=0, since we've no arguments left to consume */
    /* we pass 0 instead of the usual rspace(sop) will allocate storage for 
       filter state from the same memory pool as the stream it's coding. this
       causes no trouble because we maintain no pointers */
    return filter_read(i_ctx_p, 0, &s_jbig2decode_template,
		       (stream_state *) & state, 0);
}


/* <astruct-bytestring> .jbig2makeglobalctx <reference> */
/* we call this from ps code to instanciate a jbig2_global_context
   object which the /JBIG2Decode filter uses if available */
private int
z_jbig2makeglobalctx(i_ctx_t * i_ctx_p)
{
    return 0;
}
   
   
/* match the above routine to the corresponding filter name
   this is how our 'private' routines get called externally */
const op_def zfjbig2_op_defs[] = {
    op_def_begin_filter(),
    {"2JBIG2Decode", z_jbig2decode},
    {"1.jbig2makeglobalctx", z_jbig2makeglobalctx},
    op_def_end(0)
};
