/* Copyright (C) 1993, 1998 Aladdin Enterprises.  All rights reserved.

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

/*Id: fname.h  */
/* Requires gxiodev.h */

#ifndef fname_INCLUDED
#  define fname_INCLUDED

/*
 * Define a structure for representing a parsed file name, consisting of
 * an IODevice name in %'s, a file name, or both.  Note that the file name
 * may be either a gs_string (no terminator) or a C string (null terminator).
 */
typedef struct parsed_file_name_s {
    gx_io_device *iodev;
    const char *fname;
    uint len;
} parsed_file_name;

/* Parse a file name into device and individual name. */
int parse_file_name(P2(const ref *, parsed_file_name *));

/* Parse a real (non-device) file name and convert to a C string. */
int parse_real_file_name(P3(const ref *, parsed_file_name *, client_name_t));

/* Convert a file name to a C string by adding a null terminator. */
int terminate_file_name(P2(parsed_file_name *, client_name_t));

/* Free a file name that was copied to a C string. */
void free_file_name(P2(parsed_file_name *, client_name_t));

#endif /* fname_INCLUDED */
