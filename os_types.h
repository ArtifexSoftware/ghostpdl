/*
    jbig2dec
    
    Copyright (c) 2003 artofcode LLC.
    
    This software is distributed under license and may not
    be copied, modified or distributed except as expressly
    authorized under the terms of the license contained in
    the file LICENSE in this distribution.
                                                                                
    For information on commercial licensing, go to
    http://www.artifex.com/licensing/ or contact
    Artifex Software, Inc.,  101 Lucas Valley Road #110,
    San Rafael, CA  94903, U.S.A., +1(415)492-9861.
        
    $Id$
*/

/*
   indirection layer for build and platform-specific definitions

   in general, this header should ensure that the stdint types are
   available, and that any optional compile flags are defined if
   the build system doesn't pass them directly.
*/

#ifndef _JBIG2_OS_TYPES_H
#define _JBIG2_OS_TYPES_H

#if defined(__CYGWIN__) && !defined(HAVE_STDINT_H)
# include <sys/types.h>
  /*
   * Old versions of Cygwin have no stdint.h but define "MS types". Some of
   * them conflict with a standard type emulation provided by config_types.h
   * so we do a fixup here.
   */
   typedef u_int8_t uint8_t;
   typedef u_int16_t uint16_t;
   typedef u_int32_t uint32_t;
#elif defined(HAVE_CONFIG_H)
# include "config_types.h"
#elif defined(_WIN32)
# include "config_win32.h"
#endif

#if defined(HAVE_STDINT_H) || defined(__MACOS__)
# include <stdint.h>
#elif defined(__VMS)
# include <inttypes.h>
#endif

#endif /* _JBIG2_OS_TYPES_H */
