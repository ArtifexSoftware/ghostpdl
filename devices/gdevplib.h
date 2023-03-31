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


/* Interface functions that need to be provided for the plib device. */

#ifndef gdevplib_INCLUDED
#  define gdevplib_INCLUDED

#include "std.h"

/* Function called at the start of a job.
 *
 * Pass in a memory pointer, get back an opaque value to parrot in future
 * calls. Standard gs error return code.
 */
int gs_band_donor_init(void        **opaque, /* Opaque value   */
                       gs_memory_t  *mem);   /* Memory pointer */

/* Function called at the start of each page to get a band buffer.
 *
 * Returns a band buffer to be filled. NULL indicates error.
 */
void *gs_band_donor_band_get(void *opaque,       /* Value returned at init */
                             uint  uWidth,       /* Page Width (pixels)    */
                             uint  uHeight,      /* Page Height (pixels)   */
                             uint  uBitDepth,    /* Num bits per component */
                             uint  uComponents,  /* Number of components   */
                             uint  uStride,      /* Line stride (bytes)    */
                             uint  uBandHeight); /* Band height (pixels)   */

/* Called repeatedly when the band buffer is filled.
 *
 * Buffer is filled with nLines*uComponents*uStride*uComponents of data.
 * (First component first scanline padded to uStride bytes, then next
 * component first scanline (similarly padded), until all the components have
 * been sent, then first component, second scanline etc...)
 * Standard gs error return code.
 */
int gs_band_donor_band_full(void *opaque,  /* Value returned at init */
                            uint  nLines); /* How many lines are filled */

/* Called at the end of each page to release the band buffer.
 */
int gs_band_donor_band_release(void *opaque); /* Value returned at init */

/* Called at the end of the job to allow the band donor to release
 * any resources. */
void gs_band_donor_fin(void *opaque);

#endif /* gdevplib_INCLUDED */
