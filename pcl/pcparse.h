/* Copyright (C) 1996, 1997, 1998 Aladdin Enterprises.  All rights
   reserved.  Unauthorized use, copying, and/or distribution
   prohibited.  */

/* pcparse.h */
/* Interface and definitions for PCL5 parser */

#ifndef pcparse_INCLUDED
#  define pcparse_INCLUDED

#include "gsmemory.h"
#include "pcommand.h"

/* Define the lexical state of the scanner. */
typedef enum {
	scanning_none,
	scanning_parameter,
	scanning_display,	/* display_functions mode */
	scanning_data		/* data following a command */
} pcl_scan_type_t;
/* Define the state of the parser. */
typedef struct pcl_parser_state_s {
		/* Internal state */
	pcl_scan_type_t scan_type;
	pcl_args_t args;
	double scale;		/* for accumulating floating numbers */
	byte param_class, param_group;	/* for parameterized commands */
	uint data_pos;		/* for data crossing buffer boundaries */
} pcl_parser_state_t;
#define pcl_parser_init_inline(pst)\
  ((pst)->scan_type = scanning_none, (pst)->args.data = 0)

/* Define the prefix of a macro definition. */
typedef struct pcl_macro_s {
  pcl_entity_common;
} pcl_macro_t;

/* ---------------- Procedural interface ---------------- */

/* Allocate a parser state. */
pcl_parser_state_t *pcl_process_alloc(P1(gs_memory_t *memory));

/* Initialize the PCL parser. */
void pcl_process_init(P1(pcl_parser_state_t *pst));

/* Process a buffer of PCL commands. */
int pcl_process(P3(pcl_parser_state_t *pst, pcl_state_t *pcls,
		   stream_cursor_read *pr));

/* Execute a macro (in pcmacros.c). */
int pcl_execute_macro(P5(const pcl_macro_t *pmac, pcl_state_t *pcls,
			 pcl_copy_operation_t before, pcl_reset_type_t reset,
			 pcl_copy_operation_t after));

#endif				/* pcparse_INCLUDED */
