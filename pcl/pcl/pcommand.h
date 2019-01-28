/* Copyright (C) 2001-2019 Artifex Software, Inc.
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


/* pcommand.h */
/* Definitions for PCL5 commands */

#ifndef pcommand_INCLUDED
#  define pcommand_INCLUDED

#include "memory_.h"
#include "gx.h"
#include "gserrors.h"

/* Define a PCL value (PCL command parameter). */
/* The 16+16 representation is required to match the PCL5 documentation. */
#if ARCH_SIZEOF_INT == 4
typedef int int32;

typedef uint uint32;
#else
# if ARCH_SIZEOF_LONG == 4
typedef long int32;

typedef ulong uint32;
# endif
#endif

typedef enum
{
    pcv_none = 0,
    /* The following masks merge together: */
    pcv_int = 1,
    pcv_float = 2,
    pcv_neg = 4,
    pcv_pos = 8
} pcl_value_type_t;

#define value_is_present(pv)\
  ((pv)->type != pcv_none)
#define value_is_float(pv)\
  ((pv)->type & pcv_float)
#define value_is_neg(pv)\
  ((pv)->type & pcv_neg)
#define value_is_signed(pv)\
  ((pv)->type & (pcv_neg | pcv_pos))
typedef struct pcl_value_s
{
    pcl_value_type_t type;
    uint i;                     /* integer part */
    float fraction;
} pcl_value_t;

#define value_set_uint(pv, ui)\
  ((pv)->type = pcv_int, (pv)->i = (ui))
#define value_set_float(pv, in, fr)\
  ((pv)->type = pcv_float, (pv)->i = (in), (pv)->fraction = (fr))
#define _vshift 16
typedef int32 pcl_fixed;        /* sign + 15 int + 16 fraction */

typedef uint32 pcl_ufixed;      /* 16 int + 16 fraction */

/* Get a command argument as an int, uint, or float. */
int int_value(const pcl_value_t *);

uint uint_value(const pcl_value_t *);

float float_value(const pcl_value_t *);

/* Convert a 32-bit IEEE float to the local representation. */
float word2float(uint32 word);

/* Define the argument passed to PCL commands. */
typedef struct pcl_args_s
{
    pcl_value_t value;          /* PCL5 command argument */
    byte *data;                 /* data following the command */
    bool data_on_heap;          /* if true, data is allocated on heap */
    char command;               /* (last) command character */
} pcl_args_t;

#define int_arg(pargs) int_value(&(pargs)->value)
#define uint_arg(pargs) uint_value(&(pargs)->value)
#define float_arg(pargs) float_value(&(pargs)->value)
#define arg_data(pargs) ((pargs)->data)
#define arg_data_size(pargs) uint_arg(pargs)    /* data size */
#define arg_is_present(pargs) value_is_present(&(pargs)->value)
#define arg_is_signed(pargs) value_is_signed(&(pargs)->value)
#define arg_set_uint(pargs, ui) value_set_uint(&(pargs)->value, ui)
#define arg_set_float(pargs, in, fr) value_set_float(&(pargs)->value, in, fr)
/* Define an opaque type for the PCL state. */
#ifndef pcl_state_DEFINED
#  define pcl_state_DEFINED
typedef struct pcl_state_s pcl_state_t;
#endif

#ifndef pcl_parser_state_DEFINED
#  define pcl_parser_state_DEFINED
typedef struct pcl_parser_state_s pcl_parser_state_t;
#endif

#ifndef hpgl_parser_state_DEFINED
#  define hpgl_parser_state_DEFINED
typedef struct hpgl_args_s hpgl_parser_state_t;
#endif

/* Define a command processing procedure. */
#define pcl_command_proc(proc)\
  int proc(pcl_args_t *, pcl_state_t *)
typedef pcl_command_proc((*pcl_command_proc_t));

/* Define a few exported processing procedures. */
pcl_command_proc(pcl_disable_display_functions);
uint pcl_status_read(byte * data, uint max_data, pcl_state_t * pcs);

/* Process a string of plain (printable) text. */
int pcl_text(const byte * str, uint size, pcl_state_t * pcs, bool literal);

/* Process a single text character.  This is almost never called. */
pcl_command_proc(pcl_plain_char);

/* Define error returns. */
#define e_Range (0)             /* ignore range errors */
#define e_Syntax  gs_error_syntaxerror
#define e_Memory gs_error_VMerror
#define e_Unimplemented (0)   /* ignore unimplemented commands */
#define e_ExitLanguage gs_error_InterpreterExit

/* Define a command definition. */
typedef struct
{
    pcl_command_proc_t proc;
    int /*pcl_command_action_t */ actions;
#ifdef DEBUG
    const char *cname;
#  define PCL_COMMAND(cname, proc, actions) { proc, actions, cname }
#else
#  define PCL_COMMAND(cname, proc, actions) { proc, actions }
#endif
} pcl_command_definition_t;

/* Define actions associated with a command. */
typedef enum
{
    /* Negative arguments may be clamped to 0, give an error, cause the */
    /* command to be ignored, or be passed to the command. */
    pca_neg_action = 3,
    pca_neg_clamp = 0,
    pca_neg_error = 1,
    pca_neg_ignore = 2,
    pca_neg_ok = 3,
    /* Arguments in the range 32K to 64K-1 may be clamped, give an error, */
    /* cause the command to be ignored, or be passed to the command. */
    pca_big_action = 0xc,
    pca_big_clamp = 0,
    pca_big_error = 4,
    pca_big_ignore = 8,
    pca_big_ok = 0xc,
    /* Indicate whether the command is followed by data bytes. */
    pca_byte_data = 0x10,
    pca_bytes = pca_neg_error | pca_big_ok | pca_byte_data,
    /* Indicate whether the command is allowed in raster graphics mode. */
    pca_raster_graphics = 0x20,
    /* Indicate whether the command should be called while defining a macro. */
    pca_in_macro = 0x40,
    /* Indicate whether the command is allowed in rtl mode */
    pca_in_rtl = 0x80,
    /* */
    pca_dont_lockout_in_rtl = 0x100,
} pcl_command_action_t;

/* Define a table of command definitions. */
typedef struct
{
    char group;
    char command;
    pcl_command_definition_t defn;
} pcl_grouped_command_definition_t;

/* Register (a) command definition(s). */
void pcl_define_control_command(int /*char */ ,
                                const pcl_command_definition_t *,
                                pcl_parser_state_t *);
#define DEFINE_CONTROL(chr, cname, proc)\
{ static const pcl_command_definition_t defn_ = PCL_COMMAND(cname, proc, 0);\
  pcl_define_control_command(chr, &defn_, pcl_parser_state);\
}
void pcl_define_escape_command(int /*char */ ,
                               const pcl_command_definition_t *,
                               pcl_parser_state_t *);
#define DEFINE_ESCAPE_ARGS(chr, cname, proc, acts)\
{ static const pcl_command_definition_t defn_ = PCL_COMMAND(cname, proc, acts);\
  pcl_define_escape_command(chr, &defn_, pcl_parser_state);\
}
#define DEFINE_ESCAPE(chr, cname, proc)\
  DEFINE_ESCAPE_ARGS(chr, cname, proc, 0)
void pcl_define_class_command(int /*char */ , int /*char */ , int /*char */ ,
                              const pcl_command_definition_t *,
                              pcl_parser_state_t *);
#define DEFINE_CLASS_COMMAND_ARGS(cls, group, chr, cname, proc, acts)\
{ static const pcl_command_definition_t defn_ = PCL_COMMAND(cname, proc, acts);\
  pcl_define_class_command(cls, group, chr, &defn_, pcl_parser_state);\
}
#define DEFINE_CLASS_COMMAND(cls, group, chr, cname, proc)\
  DEFINE_CLASS_COMMAND_ARGS(cls, group, chr, cname, proc, 0)
void pcl_define_class_commands(int /*char */ ,
                               const pcl_grouped_command_definition_t *,
                               pcl_parser_state_t *);
#define DEFINE_CLASS(cls)\
{ const byte class_ = cls;\
  static const pcl_grouped_command_definition_t defs_[] = {
#define END_CLASS\
    {0, 0}\
  };\
  pcl_define_class_commands(class_, defs_, pcl_parser_state);\
}

/*
 * Define the different kinds of reset that may occur during execution.
 * Some of them are only of interest to HPGL.  We define them as bit masks
 * rather than as ordinals so that if it turns out to be useful, we can
 * defer HPGL resets until we enter HPGL mode.
 */
typedef enum
{
    pcl_reset_none = 0,
    pcl_reset_initial = 1,
    pcl_reset_cold = 2,
    pcl_reset_printer = 4,
    pcl_reset_overlay = 8,
    pcl_reset_page_params = 0x10,
    pcl_reset_picture_frame = 0x20,
    pcl_reset_anchor_point = 0x40,
    pcl_reset_plot_size = 0x80,
    /* define a special reset to be used to free permanent and internal
       objects: fonts, symbol sets and macros */
    pcl_reset_permanent = 0x100
} pcl_reset_type_t;

/*
 * Define the different kinds of state copying operation that are required
 * for macro call and overlay.
 */
typedef enum
{
    pcl_copy_none = 0,
    pcl_copy_before_call = 1,
    pcl_copy_after_call = 2,
    pcl_copy_before_overlay = 4,
    pcl_copy_after_overlay = 8,
    pcl_copy_after = pcl_copy_after_call | pcl_copy_after_overlay
} pcl_copy_operation_t;

/* Define the structure for per-module implementation procedures. */
typedef struct pcl_init_s
{
    /* Register commands */
    int (*do_registration) (pcl_parser_state_t * pcl_parser_state,
                            gs_memory_t * mem);
    /* Initialize state at startup, printer reset, and other times. */
    int (*do_reset) (pcl_state_t * pcs, pcl_reset_type_t type);
    /* Partially copy the state for macro call, overlay, and exit. */
    int (*do_copy) (pcl_state_t * psaved, const pcl_state_t * pcs,
                    pcl_copy_operation_t operation);
} pcl_init_t;

/* Define the table of pointers to init structures (in pcjob.c). */
extern const pcl_init_t *pcl_init_table[];

/* run the init code */
int pcl_do_registrations(pcl_state_t * pcs, pcl_parser_state_t * pst);

/* Run the reset code of all the modules. */
int pcl_do_resets(pcl_state_t * pcs, pcl_reset_type_t type);

/* Define stored entity storage status. */
/* Note that this is a mask, not an ordinal. */
typedef enum
{
    pcds_temporary = 1,
    pcds_permanent = 2,
    pcds_downloaded = pcds_temporary | pcds_permanent,
    pcds_internal = 4
#define pcds_cartridge_shift 3
#define pcds_cartridge_max 8
#define pcds_all_cartridges\
  ( ((1 << pcds_cartridge_max) - 1) << pcds_cartridge_shift )
#define pcds_simm_shift (pcds_cartridge_shift + pcds_cartridge_max)
#define pcds_simm_max 8
#define pcds_all_simms\
  ( ((1 << pcds_simm_max) - 1) << pcds_simm_shift )
} pcl_data_storage_t;

/* Define the control characters. */
#define BS 0x8
#define HT 0x9
#define LF 0xa
#define VERT_TAB 0xb
#define FF 0xc
#define CR 0xd
#define SO 0xe
#define SI 0xf
#define ESC 0x1b
#define SP 0x20

#ifdef DEBUG
void pcl_debug_dump_data(gs_memory_t *mem, const byte *d, int len);
#endif

#endif /* pcommand_INCLUDED */
