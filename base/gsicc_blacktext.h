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

#ifndef gsicc_blacktext_INCLUDED
#  define gsicc_blacktext_INCLUDED

typedef struct gsicc_blacktextvec_state_s gsicc_blacktextvec_state_t;

struct gsicc_blacktextvec_state_s {
    gs_memory_t *memory;
    rc_header rc;
    bool is_fill;                 /* Needed for proper color restore */
    gs_color_space *pcs;          /* color spaces to restore */
    gs_color_space *pcs_alt;
    gs_client_color *pcc;         /* client colors to restore */
    gs_client_color *pcc_alt;     /* client colors to restore */
    float value[2];               /* DeviceGray setting blows away the client color zero value */
    bool is_text;
};

gsicc_blacktextvec_state_t* gsicc_blacktextvec_state_new(gs_memory_t *memory, bool is_text);
void gsicc_restore_blacktextvec(gs_gstate *pgs, bool is_text);
bool gsicc_setup_blacktextvec(gs_gstate *pgs, gx_device *dev, bool is_text);
bool gsicc_is_white_blacktextvec(gs_gstate *pgs, gx_device *dev, gs_color_space *pcs, gs_client_color *pcc);
#endif
