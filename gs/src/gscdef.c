/* Copyright (C) 1996, 1997 Aladdin Enterprises.  All rights reserved.
  
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

/* gscdef.c */
/* Configuration scalars */
#include "stdpre.h"
#include "gscdefs.h"		/* interface */
#include "gconfig.h"		/* for #defines */

/* ---------------- Miscellaneous system parameters ---------------- */

/* All of these can be set in the makefile. */
/* They should all be const; see gscdefs.h for more information. */

#ifndef GS_BUILDTIME
#  define GS_BUILDTIME\
	0		/* should be set in the makefile */
#endif
long gs_buildtime = GS_BUILDTIME;

#ifndef GS_COPYRIGHT
#  define GS_COPYRIGHT\
	"Copyright (C) 1997 Aladdin Enterprises, Menlo Park, CA.  All rights reserved."
#endif
const char *gs_copyright = GS_COPYRIGHT;	

#ifndef GS_PRODUCT
#  define GS_PRODUCT\
	"Aladdin Ghostscript"
#endif
const char *gs_product = GS_PRODUCT;

/* GS_REVISION must be defined in the makefile. */
long gs_revision = GS_REVISION;		/* should be const, see gscdefs.h */

/* GS_REVISIONDATE must be defined in the makefile. */
long gs_revisiondate = GS_REVISIONDATE;	/* should be const, see gscdefs.h */

#ifndef GS_SERIALNUMBER
#  define GS_SERIALNUMBER\
	42		/* a famous number */
#endif
long gs_serialnumber = GS_SERIALNUMBER;	/* should be const, see gscdefs.h */

/* ---------------- Installation directories and files ---------------- */

/* Here is where the library search path, the name of the */
/* initialization file, and the doc directory are defined. */
/* Define the documentation directory (only used in help messages). */
const char *gs_doc_directory = GS_DOCDIR;

/* Define the default library search path. */
const char *gs_lib_default_path = GS_LIB_DEFAULT;

/* Define the interpreter initialization file. */
const char *gs_init_file = GS_INIT;
