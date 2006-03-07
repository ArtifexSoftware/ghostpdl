/* Portions Copyright (C) 2001 artofcode LLC.
   Portions Copyright (C) 1996, 2001, 2005 Artifex Software Inc.
   Portions Copyright (C) 1988, 2000 Aladdin Enterprises.
   This software is based in part on the work of the Independent JPEG Group.
   All Rights Reserved.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */

/*$Id$*/

/* metro recorder interface */

#ifndef metrecorder_INCLUDED
#  define metrecorder_INCLUDED

#include "metparse.h"

/* record a cooked resource */
int met_record(gs_memory_t *mem, const data_element_t *data, const char *el, bool open, int depth);
/* play it back */
int met_playback(met_state_t *ms);
/* store it in the state */
int met_store(met_state_t *ms);

#endif				/* metrecorder_INCLUDED */
