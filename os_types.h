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
        
    $Id: os_types.h,v 1.1 2002/07/20 17:23:15 giles Exp $
*/

/*
   indirection layer for build and platform-specific definitions

   in general, this header should insure that the stdint types are
   available, and that any optional compile flags are defined if
   the build system doesn't pass them directly.
*/

#ifdef HAVE_CONFIG_H
#include "config_types.h"
#elif defined(_WIN32)
#include "config_win32.h"
#endif

#if defined(HAVE_STDINT_H) || defined(__MACOS__)
#include <stdint.h>
#endif
