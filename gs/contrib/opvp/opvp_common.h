/*

Copyright (c) 2003-2004, AXE, Inc.  All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/
/* OpenPrinting Vector Printer Driver Glue Code */

#ifndef	_OPVP_COMMON_H_
#define	_OPVP_COMMON_H_

#include "opvp_0_2_0.h"
#undef _PDAPI_VERSION_MAJOR_
#undef _PDAPI_VERSION_MINOR_
/* undefine conflicted macros */
#undef OPVP_INFO_PREFIX
#undef OPVP_OK
#undef OPVP_FATALERROR
#undef OPVP_BADREQUEST
#undef OPVP_BADCONTEXT
#undef OPVP_NOTSUPPORTED
#undef OPVP_JOBCANCELED
#undef OPVP_PARAMERROR
/* define 0_2 error no as different macros */
#define OPVP_FATALERROR_0_2	-101
#define OPVP_BADREQUEST_0_2	-102
#define OPVP_BADCONTEXT_0_2	-103
#define OPVP_NOTSUPPORTED_0_2	-104
#define OPVP_JOBCANCELED_0_2	-105
#define OPVP_PARAMERROR_0_2	-106

#include "opvp.h"
#define OPVP_INFO_PREFIX ""

#endif	/* _OPVP_COMMON_H_ */

