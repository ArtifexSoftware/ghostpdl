/* pjtop.h */
/* Interface to PJL parser */

#ifndef pjtop_INCLUDED
#  define pjtop_INCLUDED

#include "pltop.h"

/*
 * Generic PJL data types which may be subclassed by specific implementations
 */
struct pjl_implementation_s;	/* fwd decl */

#ifndef PJL_ENVAR_T
#define PJL_ENVAR_T
typedef char pjl_envvar_t;	/* opaque decl */
#endif /* PJL_ENVVAR_T */

/*
 * Define PJL-specific procedures
 */
/* return the current setting of a pjl environment variable.  The
   input parameter should be the exact string used in PJLTRM.
   Sample Usage: 
         char *formlines = pjl_get_envvar(pst, "formlines");
	 if (formlines) {
	     int fl = atoi(formlines);
	     .
	     .
	 }
   Both variables and values are case insensitive.
*/
typedef pjl_envvar_t * (*pjl_proc_get_envvar_t)(P2(pl_interp_instance_t *pli, const char *pjl_var));
pjl_envvar_t * pjl_proc_get_envvar(P2(pl_interp_instance_t *pli, const char *pjl_var));

/* compare a pjl environment variable to a string values. */
typedef int (*pjl_proc_compare_t)(P3(pl_interp_instance_t *pli, const pjl_envvar_t *s1, const char *s2));
int pjl_proc_compare(P3(pl_interp_instance_t *pli, const pjl_envvar_t *s1, const char *s2));

/* map a pjl symbol set name to a pcl integer */
typedef int (*pjl_proc_map_pjl_sym_to_pcl_sym_t)(P2(pl_interp_instance_t *pli, const pjl_envvar_t *symname));
int pjl_proc_map_pjl_sym_to_pcl_sym(P2(pl_interp_instance_t *pli, const pjl_envvar_t *symname));

/* pjl environment variable to integer. */
typedef int (*pjl_proc_vartoi_t)(P2(pl_interp_instance_t *pli, const pjl_envvar_t *s));
int pjl_proc_vartoi(P2(pl_interp_instance_t *pli, const pjl_envvar_t *s));

/* pjl envioronment variable to float. */
typedef floatp (*pjl_proc_vartof_t)(P2(pl_interp_instance_t *pli, const pjl_envvar_t *s));
floatp pjl_proc_vartof(P2(pl_interp_instance_t *pli, const pjl_envvar_t *s));

/* convert a pjl designated fontsource to a subdirectory pathname. */
typedef char * (*pjl_proc_fontsource_to_path_t)(P2(pl_interp_instance_t *pli,
  const pjl_envvar_t *fontsource));
char * pjl_proc_fontsource_to_path(P2(pl_interp_instance_t *pli,
  const pjl_envvar_t *fontsource));

/* get a pjl fontnumber for an internal pcl font.  This must be
   handled differently in the future.  PJL should enumerate the fonts
   and give the fonts to pcl.  We cannot do this now because pcl
   depends on the ordering of the font table for proper font
   selection.  It also adds attributes that should be acquired from
   the font itselft.  Consequently, pcl enumerates the fonts and asks
   pjl for a font number to associate with each file font file read.  */
typedef int (*pjl_proc_get_pcl_internal_font_number_t)(P2(pl_interp_instance_t *pli,
 const char *filename));
int pjl_proc_get_pcl_internal_font_number(P2(pl_interp_instance_t *pli,
 const char *filename));

/* Change to next highest priority font source.  The following events
   automatically change the value of the FONTSOURCE variable to the
   next highest priority font source containing a default-marked font:
   if the currently set font source is C, C1, or C2, and the cartridge
   is removed from the printer; if the currently set font source is S
   and all soft fonts are deleted; if the currently set font source is
   S, while the currently set font number is the highest-numbered soft
   font, and any soft font is deleted.  Ideally this function would be
   solely responsible for these activities, with the current
   architecture we depend in part on pcl to keep up with font resource
   bookkeeping.  PJLTRM is not careful to define distinguish between
   default font source vs environment font source.  Both are set when
   the font source is changed. */
typedef void (*pjl_proc_set_next_fontsource_t)(P1(pl_interp_instance_t *pli));
void pjl_proc_set_next_fontsource(P1(pl_interp_instance_t *pli));

/* tell pjl that a soft font is being deleted.  We return 0 if no
   state change is required and 1 if the pdl should update its font
   state.  (see discussion above) */
typedef int (*pjl_proc_register_permanent_soft_font_deletion_t)(P2(pl_interp_instance_t *pli,
 int font_number));
int pjl_proc_register_permanent_soft_font_deletion(P2(pl_interp_instance_t *pli,
 int font_number));

/* request that pjl add a soft font and return a pjl font number for
   the font.   */
typedef int (*pjl_proc_register_permanent_soft_font_addition_t)(P1(pl_interp_instance_t *pli));
int pjl_proc_register_permanent_soft_font_addition(P1(pl_interp_instance_t *pli));


/*
 * Define a generic interpreter implementation
 */
typedef struct pjl_implementation_s {
  pl_interp_implementation_t               pl;   /* MUST BE FIRST generic impl */ 

  /* PJL-specific procedure vector */
  pjl_proc_get_envvar_t                    proc_get_envvar;
  pjl_proc_compare_t                       proc_compare;
  pjl_proc_map_pjl_sym_to_pcl_sym_t        proc_map_pjl_sym_to_pcl_sym;
  pjl_proc_vartoi_t                        proc_vartoi;
  pjl_proc_vartof_t                        proc_vartof;
  pjl_proc_fontsource_to_path_t            proc_fontsource_to_path;
  pjl_proc_get_pcl_internal_font_number_t  proc_get_pcl_internal_font_number;
  pjl_proc_set_next_fontsource_t           proc_set_next_fontsource;
  pjl_proc_register_permanent_soft_font_deletion_t
                                           proc_register_permanent_soft_font_deletion;
  pjl_proc_register_permanent_soft_font_addition_t
                                           proc_register_permanent_soft_font_addition;
} pjl_implementation_t;


#endif				/* pjtop_INCLUDED */

