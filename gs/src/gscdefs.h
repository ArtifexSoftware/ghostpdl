/* Copyright (C) 1994, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gscdefs.h */
/* Prototypes for configuration definitions in gconfig.c. */

/* Miscellaneous system constants (read-only systemparams). */
/* They should all be const, but one application needs some of them */
/* to be writable.... */
extern long gs_buildtime;
extern const char *gs_copyright;
extern const char *gs_product;
extern long gs_revision;
extern long gs_revisiondate;
extern long gs_serialnumber;

/* Installation directories and files */
extern const char *gs_doc_directory;
extern const char *gs_lib_default_path;
extern const char *gs_init_file;

/* Resource tables.  In order to avoid importing a large number of types, */
/* we only provide macros for the externs, not the externs themselves. */

#define extern_gx_init_table()\
  extern void (*gx_init_table[])(P1(gs_memory_t *))

#define extern_gx_io_device_table()\
  extern gx_io_device *gx_io_device_table[]

/* Return the list of device prototypes, the list of their structure */
/* descriptors, and (as the value) the length of the lists. */
#define extern_gs_lib_device_list()\
  int gs_lib_device_list(P2(const gx_device ***plist,\
			    gs_memory_struct_type_t **pst))
