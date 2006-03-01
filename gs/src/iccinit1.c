/* Copyright (C) 1995 Aladdin Enterprises.  All rights reserved.
  
  This software is provided AS-IS with no warranty, either express or
  implied.
  
  This software is distributed under license and may not be copied,
  modified or distributed except as expressly authorized under the terms
  of the license contained in the file LICENSE in this distribution.
  
  For more information about licensing, please refer to
  http://www.ghostscript.com/licensing/. For information on
  commercial licensing, go to http://www.artifex.com/licensing/ or
  contact Artifex Software, Inc., 101 Lucas Valley Road #110,
  San Rafael, CA  94903, U.S.A., +1(415)492-9861.
*/

/* $Id: iccinit1.c */
/* Initialization string for compiled initialization */
#include "stdpre.h"

/* The following PostScript is a prefix to the "real" gs_init.ps which is in the %rom%	
 * as gs_init.ps. This sequence must start with an integer token. We don't bother to
 * make it match the current version since it is discarded.
 * Care must be taken to only use basic operators since ones like '[' and ']' are
 * defined later in gs_init.ps.
 */
const byte gs_init_string[] = 
	"0 pop "			/* required integer token. DO NOT REMOVE.            */
	"systemdict /GenericResourceDir known not { " 		/* if GenericResourceDir was */
	"systemdict /GenericResourceDir (%rom%Resource/) put "	/* not set on command line,  */
	"} if "							/* set to %rom%Resource/     */
	"(gs_init.ps) .libfile not { "
	"(Can't find initialization file gs_init.ps.\\n) print flush quit "	/* OOPS! */
	"} if cvx exec "					/* now run the init file       */
;
const uint gs_init_string_sizeof = sizeof(gs_init_string);
