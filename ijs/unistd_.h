/**
 * Copyright (C) 2001-2021 Artifex Software, Inc.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
**/

/* unistd_.h */
#if defined(_WIN32)
#  include <process.h>
#  include <io.h>
#  include <fcntl.h>
#if !defined(__WATCOMC__)
   /* everything except watcom have _read()/_write(),
      watcom has read()/write() but not _read()/_write() */
#  define read(handle, buffer, count) _read(handle, buffer, count)
#  define write(handle, buffer, count) _write(handle, buffer, count)
#endif /* !__WATCOMC__ */
#ifdef _MSC_VER
   /* MSVC alone require this? */
#  define close(fd) _close(fd)
#endif /* _MSC_VER */
#else
#  include <unistd.h>
#endif
