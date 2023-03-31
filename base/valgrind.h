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


/* $Id:$ */
/* This header provides stubbed out definitions for the valgrind/memcheck
 * client requests that do nothing. This allows us to use these requests
 * in the gs source without requiring that Valgrind is available on our
 * target platform.
 *
 * To enable the macros, simply predefine HAVE_VALGRIND, whereupon this
 * file will #include "valgrind/memcheck.h" and get the real versions.
 *
 * This mechanism has been chosen over the more traditional "copy memcheck.h
 * into your code" approach as not all our platforms support valgrind, and
 * we'd rather avoid the (albeit small) overhead that the client requests
 * cost at runtime when valgrind is not in use. Also, this protects us from
 * changes in valgrind/memcheck.h between valgrind releases.
 */

#ifndef valgrind_INCLUDED
#  define valgrind_INCLUDED

#ifdef HAVE_VALGRIND

/* Enabled, so get the real header from the system. */
#include "valgrind/memcheck.h"
#include "valgrind/helgrind.h"

/* At least some versions of valgrind/helgrind don't define this */
#ifndef VALGRIND_HG_DISABLE_CHECKING
#define VALGRIND_HG_DISABLE_CHECKING(addr, len) \
    DO_NOTHING
#endif

#else

/* Disabled, so just stub the calls out */
#include "stdpre.h"

/* Stub out the Valgrind memcheck functions here */
#define VALGRIND_MAKE_MEM_NOACCESS(_qzz_addr,_qzz_len)           \
    DO_NOTHING
#define VALGRIND_MAKE_MEM_UNDEFINED(_qzz_addr,_qzz_len)          \
    DO_NOTHING
#define VALGRIND_MAKE_MEM_DEFINED(_qzz_addr,_qzz_len)            \
    DO_NOTHING
#define VALGRIND_MAKE_MEM_DEFINED_IF_ADDRESSABLE(_qzz_addr,_qzz_len)     \
    DO_NOTHING
#define VALGRIND_CREATE_BLOCK(_qzz_addr,_qzz_len, _qzz_desc)	   \
    DO_NOTHING
#define VALGRIND_DISCARD(_qzz_blkindex)                          \
    DO_NOTHING
#define VALGRIND_CHECK_MEM_IS_ADDRESSABLE(_qzz_addr,_qzz_len)      \
    DO_NOTHING
#define VALGRIND_CHECK_MEM_IS_DEFINED(_qzz_addr,_qzz_len)        \
    DO_NOTHING
#define VALGRIND_CHECK_VALUE_IS_DEFINED(__lvalue)                \
    DO_NOTHING
#define VALGRIND_DO_LEAK_CHECK                                   \
    DO_NOTHING
#define VALGRIND_DO_QUICK_LEAK_CHECK				 \
    DO_NOTHING
#define VALGRIND_COUNT_LEAKS(leaked, dubious, reachable, suppressed)     \
    DO_NOTHING
#define VALGRIND_COUNT_LEAK_BLOCKS(leaked, dubious, reachable, suppressed) \
    DO_NOTHING
#define VALGRIND_GET_VBITS(zza,zzvbits,zznbytes)                     \
    DO_NOTHING
#define VALGRIND_SET_VBITS(zza,zzvbits,zznbytes)                     \
    DO_NOTHING

#define VALGRIND_HG_DISABLE_CHECKING(addr, len) \
    DO_NOTHING

#endif /* HAVE_VALGRIND */

#endif /* valgrind_INCLUDED */
