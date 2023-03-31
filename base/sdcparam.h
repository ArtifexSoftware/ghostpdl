/* Copyright (C) 2001-2023 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  39 Mesa Street, Suite 108A, San Francisco,
   CA 94129, USA, for further information.
*/


/* DCT filter parameter setting and reading interface */

#ifndef sdcparam_INCLUDED
#  define sdcparam_INCLUDED

#include "sdct.h"
#include "gsparam.h"

/*
 * All of these procedures are defined in sdcparam.c and are only for
 * internal use (by sddparam.c and sdeparam.c), so they are not
 * documented here.
 */

int s_DCT_get_params(gs_param_list * plist, const stream_DCT_state * ss,
                     const stream_DCT_state * defaults);
int s_DCT_get_quantization_tables(gs_param_list * plist,
                                  const stream_DCT_state * pdct,
                                  const stream_DCT_state * defaults,
                                  bool is_encode);
int s_DCT_get_huffman_tables(gs_param_list * plist,
                             const stream_DCT_state * pdct,
                             const stream_DCT_state * defaults,
                             bool is_encode);

int s_DCT_byte_params(gs_param_list * plist, gs_param_name key, int start,
                      int count, UINT8 * pvals);
int s_DCT_put_params(gs_param_list * plist, stream_DCT_state * pdct);
int s_DCT_put_quantization_tables(gs_param_list * plist,
                                  stream_DCT_state * pdct,
                                  bool is_encode);
int s_DCT_put_huffman_tables(gs_param_list * plist, stream_DCT_state * pdct,
                             bool is_encode);

#endif /* sdcparam_INCLUDED */
