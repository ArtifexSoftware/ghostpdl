/* Copyright (C) 1996, 1997 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* pcommand.h */
/* Definitions for PCL5 commands */

#ifndef pcommand_INCLUDED
#  define pcommand_INCLUDED

#include "gserror.h"
#include "gserrors.h"
#include "scommon.h"

/* Define a PCL value (PCL command parameter). */
/* The 16+16 representation is required to match the PCL5 documentation. */
#if arch_sizeof_int == 4
typedef int int32;
typedef uint uint32;
#else
# if arch_sizeof_long == 4
typedef long int32;
typedef ulong uint32;
# endif
#endif
typedef enum {
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
typedef struct pcl_value_s {
	pcl_value_type_t type;
	uint i;			/* integer part */
	float fraction;
} pcl_value_t;
#define value_set_uint(pv, ui)\
  ((pv)->type = pcv_int, (pv)->i = (ui))
#define value_set_float(pv, in, fr)\
  ((pv)->type = pcv_float, (pv)->i = (in), (pv)->fraction = (fr))
#define _vshift 16
typedef int32 pcl_fixed;	/* sign + 15 int + 16 fraction */
typedef uint32 pcl_ufixed;	/* 16 int + 16 fraction */
/* Get a command argument as an int, uint, or float. */
int int_value(P1(const pcl_value_t *));
uint uint_value(P1(const pcl_value_t *));
float float_value(P1(const pcl_value_t *));
pcl_fixed pcl_fixed_value(P1(const pcl_value_t *));
pcl_ufixed pcl_ufixed_value(P1(const pcl_value_t *));

/* Convert a 32-bit IEEE float to the local representation. */
float word2float(P1(uint32 word));

/* Define the argument passed to PCL commands. */
typedef struct pcl_args_s {
  pcl_value_t value;		/* PCL5 command argument */
  const byte *data;		/* data following the command */
  bool data_on_heap;		/* if true, data is allocated on heap */
  char command;			/* (last) command character */
} pcl_args_t;
#define int_arg(pargs) int_value(&(pargs)->value)
#define uint_arg(pargs) uint_value(&(pargs)->value)
#define float_arg(pargs) float_value(&(pargs)->value)
#define pcl_fixed_arg(pargs) pcl_fixed_value(&(pargs)->value)
#define arg_data(pargs) ((pargs)->data)
#define arg_data_size(pargs) uint_arg(pargs)	/* data size */
#define arg_is_present(pargs) value_is_present(&(pargs)->value)
#define arg_is_signed(pargs) value_is_signed(&(pargs)->value)
#define arg_set_uint(pargs, ui) value_set_uint(&(pargs)->value, ui)
#define arg_set_float(pargs, in, fr) value_set_float(&(pargs)->value, in, fr)
/* Define an opaque type for the PCL state. */
#ifndef pcl_state_DEFINED
#  define pcl_state_DEFINED
typedef struct pcl_state_s pcl_state_t;
#endif

/* Define a command processing procedure. */
#define pcl_command_proc(proc)\
  int proc(P2(pcl_args_t *, pcl_state_t *))
typedef pcl_command_proc((*pcl_command_proc_t));

/* Define a few exported processing procedures. */
int pcl_do_CR(P1(pcl_state_t *));
int pcl_do_LF(P1(pcl_state_t *));
pcl_command_proc(pcl_disable_display_functions);
typedef enum {
  pcl_print_always,
  pcl_print_if_marked
} pcl_print_condition_t;
int pcl_end_page(P2(pcl_state_t *pcls, pcl_print_condition_t condition));
#define pcl_end_page_always(pcls)\
  pcl_end_page(pcls, pcl_print_always)
#define pcl_end_page_if_marked(pcls)\
  pcl_end_page(pcls, pcl_print_if_marked)
int pcl_default_finish_page(P1(pcl_state_t *pcls));
uint pcl_status_read(P3(byte *data, uint max_data, pcl_state_t *pcls));
/* Process a string of plain (printable) text. */
int pcl_text(P3(const byte *str, uint size, pcl_state_t *pcls));
/* Process a single text character.  This is almost never called. */
pcl_command_proc(pcl_plain_char);

/* Define error returns. */
#define e_Range 0		/* ignore range errors */
#define e_Syntax (-18)		/* gs_error_syntaxerror */
#define e_Memory gs_error_VMerror
#define e_Unimplemented 0	/* ignore unimplemented commands */
#define e_ExitLanguage (-102)	/* e_InterpreterExit */

/* Define a command definition. */
typedef struct {
  pcl_command_proc_t proc;
  byte/*pcl_command_action_t*/ actions;
} pcl_command_definition_t;
/* Define actions associated with a command. */
typedef enum {
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
  pca_in_macro = 0x40
} pcl_command_action_t;

/* Define a table of command definitions. */
typedef struct {
  char group;
  char command;
  pcl_command_definition_t defn;
} pcl_grouped_command_definition_t;

/* Register (a) command definition(s). */
void pcl_define_control_command(P2(int/*char*/,
				   const pcl_command_definition_t *));
#define DEFINE_CONTROL(chr, proc)\
{ static const pcl_command_definition_t defn_ = { proc };\
  pcl_define_control_command(chr, &defn_);\
}
void pcl_define_escape_command(P2(int/*char*/,
				  const pcl_command_definition_t *));
#define DEFINE_ESCAPE_ARGS(chr, proc, acts)\
{ static const pcl_command_definition_t defn_ = { proc, acts };\
  pcl_define_escape_command(chr, &defn_);\
}
#define DEFINE_ESCAPE(chr, proc)\
  DEFINE_ESCAPE_ARGS(chr, proc, 0)
void pcl_define_class_command(P4(int/*char*/, int/*char*/, int/*char*/,
				 const pcl_command_definition_t *));
#define DEFINE_CLASS_COMMAND_ARGS(cls, group, chr, proc, acts)\
{ static const pcl_command_definition_t defn_ = { proc, acts };\
  pcl_define_class_command(cls, group, chr, &defn_);\
}
#define DEFINE_CLASS_COMMAND(cls, group, chr, proc)\
  DEFINE_CLASS_COMMAND_ARGS(cls, group, chr, proc, 0)
void pcl_define_class_commands(P2(int/*char*/,
				  const pcl_grouped_command_definition_t *));
#define DEFINE_CLASS(cls)\
{ const byte class_ = cls;\
  static const pcl_grouped_command_definition_t defs_[] = {
#define END_CLASS\
    {0, 0}\
  };\
  pcl_define_class_commands(class_, defs_);\
}

/*
 * Define the different kinds of reset that may occur during execution.
 * Some of them are only of interest to HPGL.  We define them as bit masks
 * rather than as ordinals so that if it turns out to be useful, we can
 * defer HPGL resets until we enter HPGL mode.
 */
typedef enum {
  pcl_reset_initial = 1,
  pcl_reset_cold = 2,
  pcl_reset_printer = 4,
  pcl_reset_overlay = 8,
  pcl_reset_page_params = 0x10,
  pcl_reset_picture_frame = 0x20,
  pcl_reset_anchor_point = 0x40,
  pcl_reset_plot_size = 0x80
} pcl_reset_type_t;
/*
 * Define the different kinds of state copying operation that are required
 * for macro call and overlay.
 */
typedef enum {
  pcl_copy_before_call = 1,
  pcl_copy_after_call = 2,
  pcl_copy_before_overlay = 4,
  pcl_copy_after_overlay = 8,
  pcl_copy_after = pcl_copy_after_call | pcl_copy_after_overlay
} pcl_copy_operation_t;
/* Define the structure for per-module implementation procedures. */
typedef struct pcl_init_s {
	/* Register commands and do true one-time initialization. */
  int (*do_init)(P1(gs_memory_t *mem));
	/* Initialize state at startup, printer reset, and other times. */
  void (*do_reset)(P2(pcl_state_t *pcls, pcl_reset_type_t type));
	/* Partially copy the state for macro call, overlay, and exit. */
  int (*do_copy)(P3(pcl_state_t *psaved, const pcl_state_t *pcls,
		    pcl_copy_operation_t operation));
} pcl_init_t;
/* Define the table of pointers to init structures (in pcjob.c). */
extern const pcl_init_t *pcl_init_table[];
/* Run the reset code of all the modules. */
int pcl_do_resets(P2(pcl_state_t *pcls, pcl_reset_type_t type));

/* Define stored entity storage status. */
/* Note that this is a mask, not an ordinal. */
typedef enum {
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

/* Define the common prefix for all stored entities (macros, patterns, */
/* fonts, symbol sets). */
#define pcl_entity_common\
  pcl_data_storage_t storage
typedef struct pcl_entity_s {
  pcl_entity_common;
} pcl_entity_t;

/* Define the control characters. */
#define BS 0x8
#define HT 0x9
#define LF 0xa
#define FF 0xc
#define CR 0xd
#define SO 0xe
#define SI 0xf
#define ESC 0x1b
#define SP 0x20

#endif				/* pcommand_INCLUDED */
