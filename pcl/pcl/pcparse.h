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


/* pcparse.h */
/* Interface and definitions for PCL5 parser */

#ifndef pcparse_INCLUDED
#  define pcparse_INCLUDED

#include "gsmemory.h"
#include "scommon.h"
#include "pcommand.h"
#include "pcstate.h"

/* Define the lexical state of the scanner. */
typedef enum
{
    scanning_none,
    scanning_parameter,
    scanning_display,           /* display_functions mode */
    scanning_data               /* data following a command */
} pcl_scan_type_t;

#define min_escape_2char '0'
#define max_escape_2char '~'
#define min_escape_class '!'
#define max_escape_class '/'
#define min_escape_group '`'
#define max_escape_group '~'
#define min_escape_command '@'  /* or '`' */
#define max_escape_command '^'  /* or '~' */

typedef struct pcl_command_definitions_s
{
    /*
     * First-level dispatch for control characters.
     */
    byte pcl_control_command_indices[33];
    /*
     * Second-level dispatch for 2-character escape sequences.
     */
    byte pcl_escape_command_indices[max_escape_2char - min_escape_2char + 1];
    /*
     * Dispatch on class, group, and command.
     */
    byte pcl_grouped_command_indices
        [5 /* number of implemented classes, see escape_class_indices above */
         ]
        [1 + max_escape_group - min_escape_group + 1]
        [max_escape_command - min_escape_command + 1];
    int pcl_command_next_index;
    /*
     * We register all the PCL5* commands dynamically, for maximum configuration
     * flexibility.  pcl_command_list points to the individual command
     * definitions; as each command is registered, we enter it in the list, and
     * then store its index in the actual dispatch table
     * (pcl_xxx_command_xxx_indices).
     */
    pcl_command_definition_t *pcl_command_list[256];
} pcl_command_definitions_t;

/* Define the state of the parser. */
struct pcl_parser_state_s
{
    /* Internal state */
    pcl_scan_type_t scan_type;
    bool short_hand;
    bool garbage_in_parameter;
    pcl_args_t args;
    double scale;               /* for accumulating floating numbers */
    byte param_class, param_group;      /* for parameterized commands */
    uint data_pos;              /* for data crossing buffer boundaries */
    hpgl_parser_state_t *hpgl_parser_state;
    byte min_bytes_needed; /* number of characters needed to recover from an underflow */
    pcl_command_definitions_t *definitions;
};

#define pcl_parser_init_inline(pst)\
    ((pst)->scan_type = scanning_none, (pst)->args.data = 0, (pst)->args.data_on_heap = false, (pst)->short_hand = false, (pst)->min_bytes_needed = 0)

/* Define the prefix of a macro definition. */
typedef struct pcl_macro_s
{
    pcl_data_storage_t storage;
} pcl_macro_t;

/* ---------------- Procedural interface ---------------- */

/* Allocate a parser state. */
pcl_parser_state_t *pcl_process_alloc(gs_memory_t * memory);

/* Initialize the PCL parser. */
int pcl_process_init(pcl_parser_state_t * pst, pcl_state_t * pcs);

/* Process a buffer of PCL commands. */
int pcl_process(pcl_parser_state_t * pst, pcl_state_t * pcs,
                stream_cursor_read * pr);

/* Execute a macro (in pcmacros.c). */
int pcl_execute_macro(const pcl_macro_t * pmac, pcl_state_t * pcs,
                      pcl_copy_operation_t before, pcl_reset_type_t reset,
                      pcl_copy_operation_t after);

int pcparse_do_reset(pcl_state_t * pcs, pcl_reset_type_t type);

int pcl_init_command_index(pcl_parser_state_t * pcl_parser_state,
                           pcl_state_t * pcs);

/* shutdown the pcl parser */
int pcl_parser_shutdown(pcl_parser_state_t * pcl_parser_state,
                        gs_memory_t * mem);
#endif /* pcparse_INCLUDED */
