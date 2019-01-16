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


/* Configuration-dependent tables and initialization for interpreter */
#include "stdio_.h"		/* stdio for stream.h */
#include "gstypes.h"
#include "gsmemory.h"		/* for iminst.h */
#include "gconfigd.h"
#include "iref.h"
#include "ivmspace.h"
#include "opdef.h"
#include "ifunc.h"
#include "iapi.h"
#include "iminst.h"
#include "iplugin.h"
#include "gxiodev.h"

/* Declare IODevices as extern. */
#define io_device_(iodev) extern const gx_io_device iodev;
#include "iconfig.h"
#undef io_device_

/* Define the default values for an interpreter instance. */
const gs_main_instance gs_main_instance_init_values =
{gs_main_instance_default_init_values};

/* Set up the .ps file name string. */
#define psfile_(fns,len) fns "\000"
const byte gs_init_files[] =
#include "iconfig.h"
#include "gconf.h"
;
#undef psfile_
const uint gs_init_files_sizeof = sizeof(gs_init_files);

/* Set up the emulator name string. */
#define emulator_(ems,len) ems "\000"
const byte gs_emulators[] =
#include "iconfig.h"
#include "gconf.h"
;
#undef emulator_
const uint gs_emulators_sizeof = sizeof(gs_emulators);

/* Set up the function type table similarly. */
#define function_type_(i,proc) extern build_function_proc(proc);
#include "iconfig.h"
#include "gconf.h"
#undef function_type_
#define function_type_(i,proc) {i,proc},
const build_function_type_t build_function_type_table[] = {
#include "iconfig.h"
#include "gconf.h"

    {0}
};
#undef function_type_
const uint build_function_type_table_count =
    countof(build_function_type_table) - 1;

/* Initialize the operators. */
        /* Declare the externs. */
#define oper_(xx_op_defs) extern const op_def xx_op_defs[];
oper_(interp1_op_defs)		/* Interpreter operators */
oper_(interp2_op_defs)		/* ibid. */
#include "iconfig.h"
#undef oper_
const op_def *const op_defs_all[] = {
#define oper_(defs) defs,
    oper_(interp1_op_defs)	/* Interpreter operators */
    oper_(interp2_op_defs)	/* ibid. */
#include "iconfig.h"
#include "gconf.h"
#undef oper_
    0
};
const uint op_def_count = (countof(op_defs_all) - 1) * OP_DEFS_MAX_SIZE;

/* Set up the plugin table. */

#define plugin_(proc) extern plugin_instantiation_proc(proc);
#include "iconfig.h"
#include "gconf.h"
#undef plugin_

extern_i_plugin_table();
#define plugin_(proc) proc,
const i_plugin_instantiation_proc i_plugin_table[] = {
#include "iconfig.h"
#include "gconf.h"
    0
};
#undef plugin_

#define io_device_(iodev) &iodev,
const gx_io_device *const i_io_device_table[] = {
#include "iconfig.h"
    0
};
#undef io_device_
/* We must use unsigned here, not uint.  See gscdefs.h. */
const unsigned i_io_device_table_count = countof(i_io_device_table) - 1;
