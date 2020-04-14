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


/* pjtop.c */
/* Interface to PJL-specific portions parser */

#include "string_.h"
#include "pjtop.h"
#include "pjparse.h"

/* return the current setting of a pjl environment variable. */
pjl_envvar_t *
pjl_proc_get_envvar(pl_interp_implementation_t * pli, const char *pjl_var)
{
    return pjl_get_envvar(pli->interp_client_data, pjl_var);
}

/* compare a pjl environment variable to a string values. */
int
pjl_proc_compare(pl_interp_implementation_t * pli,
                 const pjl_envvar_t * s1, const char *s2)
{
    return pjl_compare(s1, s2);
}

/* map a pjl symbol set name to a pcl integer */
int
pjl_proc_map_pjl_sym_to_pcl_sym(pl_interp_implementation_t * pli,
                                const pjl_envvar_t * symname)
{
    return pjl_map_pjl_sym_to_pcl_sym(symname);
}

/* pjl environment variable to integer. */
int
pjl_proc_vartoi(pl_interp_implementation_t * pli, const pjl_envvar_t * s)
{
    return pjl_vartoi(s);
}

/* pjl envioronment variable to float. */
double
pjl_proc_vartof(pl_interp_implementation_t * pli, const pjl_envvar_t * s)
{
    return pjl_vartof(s);
}

/* convert a pjl designated fontsource to a subdirectory pathname. */
char *
pjl_proc_fontsource_to_path(pl_interp_implementation_t * pli,
                            const pjl_envvar_t * fontsource)
{
    return pjl_fontsource_to_path(pli->interp_client_data, fontsource);
}

/* Change to next highest priority font source. */
void
pjl_proc_set_next_fontsource(pl_interp_implementation_t * pli)
{
    pjl_set_next_fontsource(pli->interp_client_data);
}

/* tell pjl that a soft font is being deleted. */
int
pjl_proc_register_permanent_soft_font_deletion(pl_interp_implementation_t * pli,
                                               int font_number)
{
    return pjl_register_permanent_soft_font_deletion(pli->interp_client_data,
                                                     font_number);
}

/* request that pjl add a soft font and return a pjl font number for the font. */
int
pjl_proc_register_permanent_soft_font_addition(pl_interp_implementation_t * pli)
{
    return pjl_register_permanent_soft_font_addition(pli->interp_client_data);
}

long int
pjl_proc_get_named_resource_size(pl_interp_implementation_t * pli, char *name)
{
    return pjl_get_named_resource_size(pli->interp_client_data, name);
}

int
pjl_proc_get_named_resource(pl_interp_implementation_t * pli, char *name,
                            byte * data)
{
    return pjl_get_named_resource(pli->interp_client_data, name, data);
}

int
pjl_proc_process(pl_interp_implementation_t * pli, stream_cursor_read * pr)
{
    return pjl_process(pli->interp_client_data, NULL, pr);
}
