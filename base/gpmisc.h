/* Copyright (C) 2001-2023 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  39 Mesa Street, Suite 108A, San Francisco,
   CA 94129, USA, for further information.
*/


/* Miscellaneous support for platform facilities */

#ifndef gpmisc_INCLUDED
#  define gpmisc_INCLUDED

#include "gp.h"

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
