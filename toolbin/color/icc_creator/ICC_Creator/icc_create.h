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

#ifndef gsicc_create_INCLUDED
#  define gsicc_create_INCLUDED

typedef enum
{
	CMYK2GRAY,
	CMYK2RGB,
	GRAY2RGB,
	GRAY2CMYK,
	RGB2GRAY,
	RGB2CMYK
} link_t;

typedef struct ucr_bg
{
	unsigned char *cyan;
	unsigned char *magenta;
	unsigned char *yellow;
	unsigned char *black;
} ucrbg_t;

int create_devicen_profile(cielab_t *cielab, colornames_t *colorant_names, int num_colors, int num_samples, TCHAR FileName[]);
int create_devicelink_profile(TCHAR FileName[],link_t link_type);
int create_psrgb_profile(TCHAR FileName[]);
int create_pscmyk_profile(TCHAR FileName[], bool pcs_islab, bool cpsi_mode, ucrbg_t *ucr_data);
int create_psgray_profile(TCHAR FileName[]);
int create_gray_threshold_profile(TCHAR FileName[], float threshold);
int create_input_threshold_profile(TCHAR FileName[], float threshold, int num_colors);
int create_effect_profile(TCHAR FileName[], ucrbg_t *effect_data, char desc_ptr[]);

#endif
