/* Portions Copyright (C) 2001 artofcode LLC.
   Portions Copyright (C) 1996, 2001 Artifex Software Inc.
   Portions Copyright (C) 1988, 2000 Aladdin Enterprises.
   This software is based in part on the work of the Independent JPEG Group.
   All Rights Reserved.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */

/*$RCSfile$ $Revision$ */
/* Miscellaneous support for platform facilities */

#ifndef gpmisc_INCLUDED
#  define gpmisc_INCLUDED

/*
 * The facilities defined in this file and implemented in gpmisc.c are
 * generic utilities shared among multiple gp_ platform files.
 */

/*
 * Get the name of the directory for temporary files, if any.  Currently
 * this checks the TMPDIR and TEMP environment variables, in that order.
 * The return value and the setting of *ptr and *plen are as for gp_getenv.
 */
int gp_gettmpdir(char *ptr, int *plen);

/*
 * Open a temporary file, using O_EXCL and S_IRWXU to prevent race
 * conditions and symlink attacks.
 */
FILE *gp_fopentemp(const char *fname, const char *mode);

typedef enum {
    gp_combine_small_buffer = -1,
    gp_combine_cant_handle = 0,
    gp_combine_success = 1
} gp_file_name_combine_result;

/*
 * Combine a file name with a prefix.
 * Concatenates two paths and reduce parent references and current 
 * directory references from the concatenation when possible.
 * The trailing zero byte is being added.
 */
gp_file_name_combine_result gp_file_name_combine_generic(const char *prefix, uint plen, 
	    const char *fname, uint flen, bool no_sibling, char *buffer, uint *blen);

/*
 * Reduces parent references and current directory references when possible.
 * The trailing zero byte is being added.
 */
gp_file_name_combine_result gp_file_name_reduce(const char *fname, uint flen, 
		char *buffer, uint *blen);

/* 
 * Answers whether a file name is absolute (starts from a root). 
 */
bool gp_file_name_is_absolute(const char *fname, uint flen);

/* 
 * Returns length of all starting parent references.
 */
uint gp_file_name_parents(const char *fname, uint flen);

/* 
 * Returns length of all starting cwd references.
 */
uint gp_file_name_cwds(const char *fname, uint flen);

#endif /* gpmisc_INCLUDED */
