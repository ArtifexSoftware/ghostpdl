/* Copyright (C) 2000 Aladdin Enterprises.  All rights reserved.
  
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
/* CMap and CID-keyed font services */
#include "ghost.h"
#include "errors.h"
#include "gxcid.h"
#include "icid.h"		/* for checking prototype */
#include "idict.h"
#include "idparam.h"
#include "oper.h"

/* Get the information from a CIDSystemInfo dictionary. */
int
cid_system_info_param(gs_cid_system_info_t *pcidsi, const ref *prcidsi)
{
    ref *pregistry;
    ref *pordering;
    int code;

    if (!r_has_type(prcidsi, t_dictionary))
	return_error(e_typecheck);
    if (dict_find_string(prcidsi, "Registry", &pregistry) <= 0 ||
	dict_find_string(prcidsi, "Ordering", &pordering) <= 0
	)
	return_error(e_rangecheck);
    check_read_type_only(*pregistry, t_string);
    check_read_type_only(*pordering, t_string);
    pcidsi->Registry.data = pregistry->value.const_bytes;
    pcidsi->Registry.size = r_size(pregistry);
    pcidsi->Ordering.data = pordering->value.const_bytes;
    pcidsi->Ordering.size = r_size(pordering);
    code = dict_int_param(prcidsi, "Supplement", 0, max_int, -1,
			  &pcidsi->Supplement);
    return (code < 0 ? code : 0);
}
