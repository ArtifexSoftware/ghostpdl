/* Copyright (C) 2001-2024 Artifex Software, Inc.
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


/* Interface to PostScript-writing utilities */

#ifndef gdevpsu_INCLUDED
#  define gdevpsu_INCLUDED

#include "gsdevice.h"

/* Define parameters and state for PostScript-writing drivers. */
typedef struct gx_device_pswrite_common_s {
    float LanguageLevel;
    bool ProduceEPS;
    int ProcSet_version;
    int64_t bbox_position;		/* set when writing file header */
} gx_device_pswrite_common_t;
#define PSWRITE_COMMON_PROCSET_VERSION 1000 /* for definitions in gdevpsu.c */
#define PSWRITE_COMMON_VALUES(ll, eps, psv)\
  {ll, eps, PSWRITE_COMMON_PROCSET_VERSION + (psv)}

/* ---------------- Low level ---------------- */

/* Write a 0-terminated array of strings as lines. */
int psw_print_lines(gp_file *f, const char *const lines[]);

/* ---------------- File level ---------------- */

/*
 * Write the file header, up through the BeginProlog.  This must write to a
 * file, not a stream, because it may be called during finalization.
 */
int psw_begin_file_header(gp_file *f, const gx_device *dev,
                           const gs_rect *pbbox,
                           gx_device_pswrite_common_t *pdpc, bool ascii);

/* End the file header.*/
int psw_end_file_header(gp_file *f);

/* End the file. */
int psw_end_file(gp_file *f, const gx_device *dev,
                  const gx_device_pswrite_common_t *pdpc,
                  const gs_rect *pbbox, int page_count);

/* ---------------- Page level ---------------- */

/*
 * Write the page header.
 */
int psw_write_page_header(stream *s, const gx_device *dev,
                           const gx_device_pswrite_common_t *pdpc,
                           bool do_scale, int64_t page_ord,  int dictsize);
/*
 * Write the page trailer.  We do this directly to the file, rather than to
 * the stream, because we may have to do it during finalization.
 */
int psw_write_page_trailer(gp_file *f, int num_copies, int flush);

#endif /* gdevpsu_INCLUDED */
