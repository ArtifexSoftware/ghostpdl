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
/* Interface to PostScript-writing utilities */

#ifndef gdevpsu_INCLUDED
#  define gdevpsu_INCLUDED

/* Define parameters and state for PostScript-writing drivers. */
typedef struct gx_device_pswrite_common_s {
    float LanguageLevel;
    bool ProduceEPS;
    int ProcSet_version;
    long bbox_position;		/* set when writing file header */
} gx_device_pswrite_common_t;
#define PSWRITE_COMMON_PROCSET_VERSION 1000 /* for definitions in gdevpsu.c */
#define PSWRITE_COMMON_VALUES(ll, eps, psv)\
  {ll, eps, PSWRITE_COMMON_PROCSET_VERSION + (psv)}

/* ---------------- Low level ---------------- */

/* Write a 0-terminated array of strings as lines. */
void psw_print_lines(P2(FILE *f, const char *const lines[]));

/* ---------------- File level ---------------- */

/*
 * Write the file header, up through the BeginProlog.  This must write to a
 * file, not a stream, because it may be called during finalization.
 */
void psw_begin_file_header(P5(FILE *f, const gx_device *dev,
			      const gs_rect *pbbox,
			      gx_device_pswrite_common_t *pdpc, bool ascii));

/* End the file header.*/
void psw_end_file_header(P1(FILE *f));

/* End the file. */
void psw_end_file(P4(FILE *f, const gx_device *dev,
		     const gx_device_pswrite_common_t *pdpc,
		     const gs_rect *pbbox));

/* ---------------- Page level ---------------- */

/*
 * Write the page header.
 */
void psw_write_page_header(P4(stream *s, const gx_device *dev,
			      const gx_device_pswrite_common_t *pdpc,
			      bool do_scale));
/*
 * Write the page trailer.  We do this directly to the file, rather than to
 * the stream, because we may have to do it during finalization.
 */
void psw_write_page_trailer(P3(FILE *f, int num_copies, int flush));

#endif /* gdevpsu_INCLUDED */

