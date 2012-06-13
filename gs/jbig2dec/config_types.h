/* Copyright (C) 2001-2012 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134, San Rafael,
   CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

/*
   generated header with missing types for the
   jbig2dec program and library. include this
   after config.h, within the HAVE_CONFIG_H
   ifdef
*/

#ifndef HAVE_STDINT_H
#  ifdef JBIG2_REPLACE_STDINT_H
#   include <no_replacement_found>
#  else
    typedef unsigned int uint32_t;
    typedef unsigned short uint16_t;
    typedef unsigned char uint8_t;
    typedef signed int int32_t;
    typedef signed short int16_t;
    typedef signed char int8_t;
#  endif /* JBIG2_REPLACE_STDINT */
#endif /* HAVE_STDINT_H */
