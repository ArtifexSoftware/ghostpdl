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


/* pjparsei.c   PJL parser implementation glue file (To pjparse.c) */

#include "string_.h"
#include "gserrors.h"
#include "pjtop.h"
#include "pjparse.h"
#include "plparse.h"
#include "plver.h"

/*
 * PJL interpeter: derived from pl_interp_t
 */
typedef struct pjl_interp_s
{
    pl_interp_t pl;             /* common part: must be first */
    gs_memory_t *memory;        /* memory allocator to use */
} pjl_interp_t;

/*
 * PJL interpreter instance: derived from pl_interp_instance_t
 */
typedef struct pjl_interp_instance_s
{
    pl_interp_instance_t pl;    /* common part: must be first */
    gs_memory_t *memory;        /* memory allocator to use */
    pjl_parser_state *state;    /* parser's state */
} pjl_interp_instance_t;

/* Get implemtation's characteristics */
static const pl_interp_characteristics_t *      /* always returns a descriptor */
pjl_impl_characteristics(const pl_interp_implementation_t * impl        /* implementation of interpereter to alloc */
    )
{
    static const pl_interp_characteristics_t pjl_characteristics = {
        "PJL",
        "@PJL",
        "Artifex",
        PJLVERSION,
        PJLBUILDDATE,
        17                      /* sizeof min buffer == sizeof UEL */
    };
    return &pjl_characteristics;
}

/* Allocate a PJL interp */
static int                      /* ret 0 ok, else -ve error code */
pjl_impl_allocate_interp(pl_interp_t ** interp, /* RETURNS abstract interpreter struct */
                         const pl_interp_implementation_t * impl,       /* implementation of interpereter to alloc */
                         gs_memory_t * mem      /* allocator to allocate interp from */
    )
{
    /* Allocate an interpreter */
    pjl_interp_t *pjl      /****** SHOULD HAVE A STRUCT DESCRIPTOR ******/
        = (pjl_interp_t *) gs_alloc_bytes(mem,
                                          sizeof(pjl_interp_t),
                                          "pjl_impl_allocate_interp(pjl_interp_t)");

    if (pjl == 0)
        return gs_error_VMerror;
    pjl->memory = mem;
    *interp = (pl_interp_t *) pjl;
    return 0;                   /* success */
}

/* Do per-instance interpreter allocation/init. No device is set yet */
static int                      /* ret 0 ok, else -ve error code */
pjl_impl_allocate_interp_instance(pl_interp_instance_t ** instance,     /* RETURNS instance struct */
                                  pl_interp_t * interp, /* dummy interpreter */
                                  gs_memory_t * mem     /* allocator to allocate instance from */
    )
{
    /* Allocate everything up front */
    pjl_interp_instance_t *pjli      /****** SHOULD HAVE A STRUCT DESCRIPTOR ******/
        = (pjl_interp_instance_t *) gs_alloc_bytes(mem,
                                                   sizeof
                                                   (pjl_interp_instance_t),
                                                   "pjl_impl_allocate_interp_instance(pjl_interp_instance_t)");

    pjl_parser_state *pjls = pjl_process_init(mem);

    /* If any allocation error simply return */
    if (!pjli || !pjls)
        return gs_error_VMerror;

    /* Setup pointers to allocated mem within instance */
    pjli->state = pjls;
    pjli->memory = mem;

    /* Return success */
    *instance = (pl_interp_instance_t *) pjli;
    return 0;
}

/* Set a device into an interperter instance */
static int                      /* ret 0 ok, else -ve error code */
pjl_impl_set_device(pl_interp_instance_t * instance,    /* interp instance to use */
                    gx_device * device  /* device to set (open or closed) */
    )
{
    return 0;
}

/* Prepare interp instance for the next "job" */
static int                      /* ret 0 ok, else -ve error code */
pjl_impl_init_job(pl_interp_instance_t * instance       /* interp instance to start job in */
    )
{
    pjl_interp_instance_t *pjli = (pjl_interp_instance_t *) instance;

    if (pjli->state == 0)
        return gs_error_VMerror;
    /* copy the default state to the initial state */
    pjl_set_init_from_defaults(pjli->state);
    return 0;
}

/* Parse a cursor-full of data */

/* The parser reads data from the input
 * buffer and returns either:
 *	>=0 - OK, more input is needed.
 *	e_ExitLanguage - Non-PJL was detected.
 *	<0 value - an error was detected.
 */

static int
pjl_impl_process(pl_interp_instance_t * instance,       /* interp instance to process data job in */
                 stream_cursor_read * cursor    /* data to process */
    )
{
    pjl_interp_instance_t *pjli = (pjl_interp_instance_t *) instance;
    int code = pjl_process(pjli->state, NULL, cursor);

    return code == 1 ? e_ExitLanguage : code;
}

/* Skip to end of job ret 1 if done, 0 ok but EOJ not found, else -ve error code */
static int
pjl_impl_flush_to_eoj(pl_interp_instance_t * instance,  /* interp instance to flush for */
                      stream_cursor_read * cursor       /* data to process */
    )
{
    return pjl_skip_to_uel(cursor) ? 1 : 0;
}

/* Parser action for end-of-file */
static int                      /* ret 0 or +ve if ok, else -ve error code */
pjl_impl_process_eof(pl_interp_instance_t * instance    /* interp instance to process data job in */
    )
{
    return 0;
}

/* Report any errors after running a job */
static int                      /* ret 0 ok, else -ve error code */
pjl_impl_report_errors(pl_interp_instance_t * instance, /* interp instance to wrap up job in */
                       int code,        /* prev termination status */
                       long file_position,      /* file position of error, -1 if unknown */
                       bool force_to_cout       /* force errors to cout */
    )
{
    return 0;
}

/* Wrap up interp instance after a "job" */
static int                      /* ret 0 ok, else -ve error code */
pjl_impl_dnit_job(pl_interp_instance_t * instance       /* interp instance to wrap up job in */
    )
{
    int code = 0;
    return code;
}

/* Remove a device from an interperter instance */
static int                      /* ret 0 ok, else -ve error code */
pjl_impl_remove_device(pl_interp_instance_t * instance  /* interp instance to use */
    )
{
    return 0;
}

/* Deallocate a interpreter instance */
static int                      /* ret 0 ok, else -ve error code */
pjl_impl_deallocate_interp_instance(pl_interp_instance_t * instance     /* instance to dealloc */
    )
{
    pjl_interp_instance_t *pjli = (pjl_interp_instance_t *) instance;
    gs_memory_t *mem = pjli->memory;

    pjl_process_destroy(pjli->state, mem);
    gs_free_object(mem, pjli,
                   "pjl_impl_deallocate_interp_instance(pjl_interp_instance_t)");

    return 0;
}

/* Do static deinit of PJL interpreter */
static int                      /* ret 0 ok, else -ve error code */
pjl_impl_deallocate_interp(pl_interp_t * interp /* interpreter to deallocate */
    )
{
    pjl_interp_t *pi = (pjl_interp_t *) interp;
    gs_memory_t *mem = pi->memory;

    gs_free_object(mem, pi, "pjl_impl_deallocte_interp(pjl_interp_t)");
    return 0;
}

/* return the current setting of a pjl environment variable. */
static pjl_envvar_t *
pjl_impl_get_envvar(pl_interp_instance_t * pli, const char *pjl_var)
{
    pjl_interp_instance_t *pjli = (pjl_interp_instance_t *) pli;
    return pjl_get_envvar(pjli->state, pjl_var);
}

/* return the current setting of a pjl environment variable. */
static void
pjl_impl_set_envvar(pl_interp_instance_t * pli, const char *pjl_var, const char *data)
{
    pjl_interp_instance_t *pjli = (pjl_interp_instance_t *) pli;
    pjl_set_envvar(pjli->state, pjl_var, data);
}

/* return the current setting of a pjl environment variable. */
static void
pjl_impl_set_defvar(pl_interp_instance_t * pli, const char *pjl_var, const char *data)
{
    pjl_interp_instance_t *pjli = (pjl_interp_instance_t *) pli;
    pjl_set_defvar(pjli->state, pjl_var, data);
}

/* compare a pjl environment variable to a string values. */
static int
pjl_impl_compare(pl_interp_instance_t * pli,
                 const pjl_envvar_t * s1, const char *s2)
{
    return pjl_compare(s1, s2);
}

/* map a pjl symbol set name to a pcl integer */
static int
pjl_impl_map_pjl_sym_to_pcl_sym(pl_interp_instance_t * pli,
                                const pjl_envvar_t * symname)
{
    return pjl_map_pjl_sym_to_pcl_sym(symname);
}

/* pjl environment variable to integer. */
static int
pjl_impl_vartoi(pl_interp_instance_t * pli, const pjl_envvar_t * s)
{
    return pjl_vartoi(s);
}

/* pjl envioronment variable to float. */
static double
pjl_impl_vartof(pl_interp_instance_t * pli, const pjl_envvar_t * s)
{
    return pjl_vartof(s);
}

/* convert a pjl designated fontsource to a subdirectory pathname. */
static char *
pjl_impl_fontsource_to_path(pl_interp_instance_t * pli,
                            const pjl_envvar_t * fontsource)
{
    pjl_interp_instance_t *pjli = (pjl_interp_instance_t *) pli;

    return pjl_fontsource_to_path(pjli->state, fontsource);
}

/* Change to next highest priority font source. */
static void
pjl_impl_set_next_fontsource(pl_interp_instance_t * pli)
{
    pjl_interp_instance_t *pjli = (pjl_interp_instance_t *) pli;

    pjl_set_next_fontsource(pjli->state);
}

/* tell pjl that a soft font is being deleted. */
static int
pjl_impl_register_permanent_soft_font_deletion(pl_interp_instance_t * pli,
                                               int font_number)
{
    pjl_interp_instance_t *pjli = (pjl_interp_instance_t *) pli;

    return pjl_register_permanent_soft_font_deletion(pjli->state,
                                                     font_number);
}

/* request that pjl add a soft font and return a pjl font number for the font. */
static int
pjl_impl_register_permanent_soft_font_addition(pl_interp_instance_t * pli)
{
    pjl_interp_instance_t *pjli = (pjl_interp_instance_t *) pli;

    return pjl_register_permanent_soft_font_addition(pjli->state);
}

static long int
pjl_impl_get_named_resource_size(pl_interp_instance_t * pli, char *name)
{
    pjl_interp_instance_t *pjli = (pjl_interp_instance_t *) pli;

    return pjl_get_named_resource_size(pjli->state, name);
}

static int
pjl_impl_get_named_resource(pl_interp_instance_t * pli, char *name,
                            unsigned char *data)
{
    pjl_interp_instance_t *pjli = (pjl_interp_instance_t *) pli;

    return pjl_get_named_resource(pjli->state, name, data);
}

/* Parser implementation descriptor */
pjl_implementation_t pjl_implementation = {
    /* Generic language parser portion */
    {pjl_impl_characteristics,
     pjl_impl_allocate_interp,
     pjl_impl_allocate_interp_instance,
     pjl_impl_set_device,
     pjl_impl_init_job,
     NULL,                      /* process_file */
     pjl_impl_process,
     pjl_impl_flush_to_eoj,
     pjl_impl_process_eof,
     pjl_impl_report_errors,
     pjl_impl_dnit_job,
     pjl_impl_remove_device,
     pjl_impl_deallocate_interp_instance,
     pjl_impl_deallocate_interp
    },
    /* PJL-specific portion */
    pjl_impl_get_envvar,
    pjl_impl_compare,
    pjl_impl_map_pjl_sym_to_pcl_sym,
    pjl_impl_vartoi,
    pjl_impl_vartof,
    pjl_impl_fontsource_to_path,
    pjl_impl_set_next_fontsource,
    pjl_impl_register_permanent_soft_font_deletion,
    pjl_impl_register_permanent_soft_font_addition,
    pjl_impl_get_named_resource_size,
    pjl_impl_get_named_resource,
    pjl_impl_process
};
