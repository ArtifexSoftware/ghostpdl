/* Copyright (C) 1995, 1996, 1997 Aladdin Enterprises.  All rights reserved.
  
  This file is part of Aladdin Ghostscript.
  
  Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
  or distributor accepts any responsibility for the consequences of using it,
  or for whether it serves any particular purpose or works at all, unless he
  or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
  License (the "License") for full details.
  
  Every copy of Aladdin Ghostscript must include a copy of the License,
  normally in a plain ASCII text file named PUBLIC.  The License grants you
  the right to copy, modify and redistribute Aladdin Ghostscript, but only
  under certain conditions described in the License.  Among other things, the
  License requires that the copyright notice and this notice be preserved on
  all copies.
*/

/* iconf.c */
/* Configuration-dependent tables and initialization for interpreter */
#include "stdio_.h"			/* stdio for stream.h */
#include "gsmemory.h"			/* for gscdefs.h */
#include "gscdefs.h"
#include "gconfig.h"
#include "iref.h"
#include "ivmspace.h"
#include "opdef.h"
#include "stream.h"			/* for files.h */
#include "files.h"
#include "iminst.h"

/* Define the default values for an interpreter instance. */
const gs_main_instance gs_main_instance_init_values =
  { gs_main_instance_default_init_values };

/* Set up the .ps file name string array. */
/* We fill in the lengths at initialization time. */
#define ref_(t) struct { struct tas_s tas; t value; }
#define string_(s)\
 { { (t_string<<r_type_shift) + a_readonly + avm_foreign, 0 }, s },
#define psfile_(fns) string_(fns)
ref_(const char *) gs_init_file_array[] = {
#include "gconfig.h"
	string_(0)
};
#undef psfile_

/* Set up the emulator name string array similarly. */
#define emulator_(ems) string_(ems)
ref_(const char *) gs_emulator_name_array[] = {
#include "gconfig.h"
	string_(0)
};
#undef emulator_

/* Initialize the operators. */
extern op_def_ptr
	/* Initialization operators */
#define oper_(defs) defs(P0()),
#include "gconfig.h"
#undef oper_
	/* Interpreter operators */
  interp_op_defs(P0());
op_def_ptr (*(op_defs_all[]))(P0()) = {
	/* Initialization operators */
#define oper_(defs) defs,
#include "gconfig.h"
#undef oper_
	/* Interpreter operators */
  interp_op_defs,
	/* end marker */
  0
};
