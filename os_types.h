/*
    jbig2dec
    
    Copyright (c) 2002 artofcode LLC.
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
        
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

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
