/* Portions Copyright (C) 2001 artofcode LLC.
   Portions Copyright (C) 1996, 2001 Artifex Software Inc.
   Portions Copyright (C) 1988, 2000 Aladdin Enterprises.
   This software is based in part on the work of the Independent JPEG Group.
   All Rights Reserved.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */
/*$Id$ */

/* pllfont.h */
/* Interface for pcl and xl resident fonts */

#ifndef pllfont_INCLUDED
#  define pllfont_INCLUDED
/* This interface is used to load resident or more exactly font
   resources that are not downloaded */
/* NB - pass in store data in a file and permanent data */
int pl_load_built_in_fonts(const char *pathname, gs_memory_t *mem, pl_dict_t *pfontdict, gs_font_dir *pdir, int storage, bool use_unicode_names_for_keys);
int pl_load_simm_fonts(const char *pathname, gs_memory_t *mem, pl_dict_t *pfontdict, gs_font_dir *pdir, int storage);
int pl_load_cartridge_fonts(const char *pathname, gs_memory_t *mem, pl_dict_t *pfontdict, gs_font_dir *pdir, int storage);
#endif				/* plfont_INCLUDED */
