/* pllfont.h */
/* Interface for pcl and xl resident fonts */

#ifndef pllfont_INCLUDED
#  define pllfont_INCLUDED
/* This interface is used to load resident or more exactly font
   resources that are not downloaded */
/* NB - pass in store data in a file and permanent data */
int pl_load_built_in_fonts(P6(const char *pathname, gs_memory_t *mem, pl_dict_t *pfontdict, gs_font_dir *pdir, int storage, bool use_unicode_names_for_keys));
int pl_load_simm_fonts(P5(const char *pathname, gs_memory_t *mem, pl_dict_t *pfontdict, gs_font_dir *pdir, int storage));
int pl_load_cartridge_fonts(P5(const char *pathname, gs_memory_t *mem, pl_dict_t *pfontdict, gs_font_dir *pdir, int storage));

#endif				/* plfont_INCLUDED */
