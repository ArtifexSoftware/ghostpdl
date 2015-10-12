/* Copyright (C) 2001-2012 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134, San Rafael,
   CA  94903, U.S.A., +1(415)492-9861, for further information.
*/


/* pjtop.c */
/* Interface to PJL-specific portions parser */

#include "string_.h"
#include "pjtop.h"

/* return the current setting of a pjl environment variable. */
pjl_envvar_t *
pjl_proc_get_envvar(pl_interp_instance_t * pli, const char *pjl_var)
{
    return ((pjl_implementation_t *) pli->interp->implementation)
        ->proc_get_envvar(pli, pjl_var);
}

void
pjl_proc_set_envvar(pl_interp_instance_t * pli, const char *pjl_var, const char *data)
{
    ((pjl_implementation_t *) pli->interp->implementation)
        ->proc_set_envvar(pli, pjl_var, data);
}

void
pjl_proc_set_defvar(pl_interp_instance_t * pli, const char *pjl_var, const char *data)
{
    ((pjl_implementation_t *) pli->interp->implementation)
        ->proc_set_defvar(pli, pjl_var, data);
}

/* compare a pjl environment variable to a string values. */
int
pjl_proc_compare(pl_interp_instance_t * pli,
                 const pjl_envvar_t * s1, const char *s2)
{
    return ((pjl_implementation_t *) pli->interp->implementation)
        ->proc_compare(pli, s1, s2);
}

/* map a pjl symbol set name to a pcl integer */
int
pjl_proc_map_pjl_sym_to_pcl_sym(pl_interp_instance_t * pli,
                                const pjl_envvar_t * symname)
{
    return ((pjl_implementation_t *) pli->interp->implementation)
        ->proc_map_pjl_sym_to_pcl_sym(pli, symname);
}

/* pjl environment variable to integer. */
int
pjl_proc_vartoi(pl_interp_instance_t * pli, const pjl_envvar_t * s)
{
    return ((pjl_implementation_t *) pli->interp->implementation)
        ->proc_vartoi(pli, s);
}

/* pjl envioronment variable to float. */
double
pjl_proc_vartof(pl_interp_instance_t * pli, const pjl_envvar_t * s)
{
    return ((pjl_implementation_t *) pli->interp->implementation)
        ->proc_vartof(pli, s);
}

/* convert a pjl designated fontsource to a subdirectory pathname. */
char *
pjl_proc_fontsource_to_path(pl_interp_instance_t * pli,
                            const pjl_envvar_t * fontsource)
{
    return ((pjl_implementation_t *) pli->interp->implementation)
        ->proc_fontsource_to_path(pli, fontsource);
}

/* Change to next highest priority font source. */
void
pjl_proc_set_next_fontsource(pl_interp_instance_t * pli)
{
    ((pjl_implementation_t *) pli->interp->implementation)
        ->proc_set_next_fontsource(pli);
}

/* tell pjl that a soft font is being deleted. */
int
pjl_proc_register_permanent_soft_font_deletion(pl_interp_instance_t * pli,
                                               int font_number)
{
    return ((pjl_implementation_t *) pli->interp->implementation)
        ->proc_register_permanent_soft_font_deletion(pli, font_number);
}

/* request that pjl add a soft font and return a pjl font number for the font. */
int
pjl_proc_register_permanent_soft_font_addition(pl_interp_instance_t * pli)
{
    return ((pjl_implementation_t *) pli->interp->implementation)
        ->proc_register_permanent_soft_font_addition(pli);
}

long int
pjl_proc_get_named_resource_size(pl_interp_instance_t * pli, char *name)
{
    return ((pjl_implementation_t *) pli->interp->implementation)
        ->proc_get_named_resource_size(pli, name);
}

int
pjl_proc_get_named_resource(pl_interp_instance_t * pli, char *name,
                            byte * data)
{
    return ((pjl_implementation_t *) pli->interp->implementation)
        ->proc_get_named_resource(pli, name, data);
}

int
pjl_proc_process(pl_interp_instance_t * pli, stream_cursor_read * pr)
{
    return ((pjl_implementation_t *) pli->interp->implementation)
        ->proc_process(pli, pr);
}
