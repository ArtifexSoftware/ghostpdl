/* Copyright (C) 2001-2020 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
*/


/* pllfont.h */
/* Interface for pcl and xl resident fonts */

#ifndef pllfont_INCLUDED
#  define pllfont_INCLUDED
/* This interface is used to load resident or more exactly font
   resources that are not downloaded */
/* NB - pass in store data in a file and permanent data */
int pl_load_built_in_fonts(const char *pathname, gs_memory_t * mem,
                           pl_dict_t * pfontdict, gs_font_dir * pdir,
                           int storage, bool use_unicode_names_for_keys);
int pl_load_simm_fonts(const char *pathname, gs_memory_t * mem,
                       pl_dict_t * pfontdict, gs_font_dir * pdir,
                       int storage);
int pl_load_cartridge_fonts(const char *pathname, gs_memory_t * mem,
                            pl_dict_t * pfontdict, gs_font_dir * pdir,
                            int storage);
int pl_load_ufst_lineprinter(gs_memory_t * mem, pl_dict_t * pfontdict,
                             gs_font_dir * pdir, int storage,
                             bool use_unicode_names_for_keys);

#endif /* plfont_INCLUDED */
