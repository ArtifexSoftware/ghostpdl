/* Copyright (C) 1993, 1998, 1999 Aladdin Enterprises.  All rights reserved.
 * This software is licensed to a single customer by Artifex Software Inc.
 * under the terms of a specific OEM agreement.
 */

/*$RCSfile$ $Revision$ */

#ifndef gsfname_INCLUDED
#  define gsfname_INCLUDED

/*
 * Structure and procedures for parsing file names.
 *
 * Define a structure for representing a parsed file name, consisting of
 * an IODevice name in %'s, a file name, or both.  Note that the file name
 * may be either a gs_string (no terminator) or a C string (null terminator).
 *
 * NOTE: You must use parse_[real_]file_name to construct parsed_file_names.
 * Do not simply allocate the structure and fill it in.
 */
#ifndef gx_io_device_DEFINED
#  define gx_io_device_DEFINED
typedef struct gx_io_device_s gx_io_device;
#endif

typedef struct gs_parsed_file_name_s {
    gs_memory_t *memory;	/* allocator for terminated name string */
    gx_io_device *iodev;
    const char *fname;
    uint len;
} gs_parsed_file_name_t;

/* Parse a file name into device and individual name. */
int gs_parse_file_name(P3(gs_parsed_file_name_t *, const char *, uint));

/* Parse a real (non-device) file name and convert to a C string. */
int gs_parse_real_file_name(P5(gs_parsed_file_name_t *, const char *, uint,
			       gs_memory_t *, client_name_t));

/* Convert a file name to a C string by adding a null terminator. */
int gs_terminate_file_name(P3(gs_parsed_file_name_t *, gs_memory_t *,
			      client_name_t));

/* Free a file name that was copied to a C string. */
void gs_free_file_name(P2(gs_parsed_file_name_t *, client_name_t));

#endif /* gsfname_INCLUDED */
