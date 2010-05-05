/* Copyright (C) 2001-2009 Artifex Software, Inc.
   All Rights Reserved.
  
   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
*/



#ifndef gsicc_create_INCLUDED
#  define gsicc_create_INCLUDED

typedef enum {
    CMYK2GRAY,
    CMYK2RGB,
    GRAY2RGB,
    GRAY2CMYK,
    RGB2GRAY,
    RGB2CMYK
} link_t;

int create_devicen_profile(cielab_t *cielab, colornames_t *colorant_names, int num_colors, int num_samples, TCHAR FileName[]);

int create_devicelink_profile(TCHAR FileName[],link_t link_type);

int create_psrgb_profile(TCHAR FileName[]);
int create_pscmyk_profile(TCHAR FileName[], bool pcs_islab);
int create_psgray_profile(TCHAR FileName[]);

#endif
