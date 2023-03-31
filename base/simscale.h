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


/* Definitions for image mask interpolation filter */
/* Requires scommon.h; strimpl.h if any templates are referenced */

#ifndef simscale_INCLUDED
#  define simscale_INCLUDED

#include "sisparam.h"

typedef struct stream_imscale_state_s stream_imscale_state;

struct stream_imscale_state_s {
    stream_image_scale_state_common;
    byte *window;

    int src_y;
    int src_offset;
    int src_size;
    int src_line_padded;

    byte *dst;
    int64_t dst_togo; /* down-counter of output bytes */
    int dst_offset;
    int dst_size;
    int dst_line_size;
    int dst_line_padded;
};

extern const stream_template s_imscale_template;

#endif /* simscale_INCLUDED */
