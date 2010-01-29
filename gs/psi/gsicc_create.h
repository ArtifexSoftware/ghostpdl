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

void gsicc_create_fromcrd(unsigned char *buffer, gs_memory_t *memory);
void gsicc_create_froma(gs_cie_a *pcie, unsigned char *buffer, gs_memory_t *memory,
                        bool has_a_proc, bool has_lmn_procs);
int gsicc_create_fromabc(gs_cie_abc *pcie, unsigned char **buffer, int *profile_size_out, gs_memory_t *memory,
                          bool has_abc_procs, bool has_lmn_procs);
void gsicc_create_fromdefg(gs_cie_defg *pcie, unsigned char *buffer, gs_memory_t *memory,
                           bool has_defg_procs, bool has_abc_procs, bool has_lmn_procs);
void gsicc_create_fromdef(gs_cie_def *pcie, unsigned char *buffer, gs_memory_t *memory, 
                          bool has_def_procs, bool has_abc_procs, bool has_lmn_procs);
cmm_profile_t* gsicc_create_from_cal(float *white, float *black, float *gamma, 
                                     float *matrix, gs_memory_t *memory, int num_colors);

#endif
