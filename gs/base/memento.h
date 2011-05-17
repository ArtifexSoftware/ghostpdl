/* Copyright (C) 2011 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

/* Memento: A library to aid debugging of memory leaks/heap corruption.
 *
 * Usage:
 *    First, build your project with MEMENTO defined, and include this
 *    header file whereever you use malloc, realloc or free.
 *    This header file will use macros to point malloc, realloc and free to
 *    point to Memento_malloc, Memento_realloc, Memento_free.
 *
 *    Run your program, and all mallocs/frees/reallocs should be redirected
 *    through here. When the program exits, you will get a list of all the
 *    leaked blocks, together with some helpful statistics. You can get the
 *    same list of allocated blocks at any point during program execution by
 *    calling Memento_listBlocks();
 *
 *    Every call to malloc/free/realloc counts as an 'allocation event'.
 *    On each event Memento increments a counter. Every block is tagged with
 *    the current counter on allocation. Every so often during program
 *    execution, the heap is checked for consistency. By default this happens
 *    every 1024 events. This can be changed at runtime by using
 *    Memento_setParanoia(int level). 0 turns off such checking, 1 sets
 *    checking to happen on every event, any other number n sets checking to
 *    happen once every n events.
 *
 *    Memento keeps blocks around for a while after they have been freed, and
 *    checks them as part of these heap checks to see if they have been
 *    written to (or are freed twice etc).
 *
 *    A given heap block can be checked for consistency (it's 'pre' and
 *    'post' guard blocks are checked to see if they have been written to)
 *    by calling Memento_checkBlock(void *blockAddress);
 *
 *    A check of all the memory can be triggered by calling Memento_check();
 *    (or Memento_checkAllMemory(); if you'd like it to be quieter).
 *
 *    A good place to breakpoint is Memento_breakpoint, as this will then
 *    trigger your debugger if an error is detected. This is done
 *    automatically for debug windows builds.
 *
 *    If a block is found to be corrupt, information will be printed to the
 *    console, including the address of the block, the size of the block,
 *    the type of corruption, the number of the block and the event on which
 *    it last passed a check for correctness.
 *
 *    If you rerun, and call Memento_paranoidAt(int event); with this number
 *    the the code will wait until it reaches that event and then start
 *    checking the heap after every allocation event. Assuming it is a
 *    deterministic failure, you should then find out where in your program
 *    the error is occuring (between event x-1 and event x).
 *
 *    Then you can rerun the program again, and call
 *    Memento_breakAt(int event); and the program will call
 *    Memento_Breakpoint() when event x is reached, enabling you to step
 *    through.
 *
 *    Memento_find(address) will tell you what block (if any) the given
 *    address is in.
 */

#ifndef MEMENTO_H

#include "std.h"

#define MEMENTO_H

#ifndef MEMENTO_UNDERLYING_MALLOC
#define MEMENTO_UNDERLYING_MALLOC malloc
#endif
#ifndef MEMENTO_UNDERLYING_FREE
#define MEMENTO_UNDERLYING_FREE free
#endif
#ifndef MEMENTO_UNDERLYING_REALLOC
#define MEMENTO_UNDERLYING_REALLOC realloc
#endif
#ifndef MEMENTO_UNDERLYING_CALLOC
#define MEMENTO_UNDERLYING_CALLOC calloc
#endif

#ifndef MEMENTO_MAXALIGN
#define MEMENTO_MAXALIGN (sizeof(int))
#endif

#define MEMENTO_PREFILL   0xa6
#define MEMENTO_POSTFILL  0xa7
#define MEMENTO_ALLOCFILL 0xa8
#define MEMENTO_FREEFILL  0xa9

#define MEMENTO_FREELIST_MAX 0x2000000

int Memento_checkBlock(void *);
int Memento_checkAllMemory(void);
int Memento_check(void);

int Memento_setParanoia(int);
int Memento_paranoidAt(int);
int Memento_breakAt(int);
int Memento_getBlockNum(void *);
int Memento_find(void *a);
void Memento_breakpoint(void);
int Memento_failAt(int);

void *Memento_malloc(size_t s);
void *Memento_realloc(void *, size_t s);
void  Memento_free(void *);
void *Memento_calloc(size_t, size_t);

#ifdef MEMENTO

#ifndef COMPILING_MEMENTO_C
#define malloc  Memento_malloc
#define free    Memento_free
#define realloc Memento_realloc
#define calloc  Memento_calloc
#endif

#else

#define Memento_malloc  MEMENTO_UNDERLYING_MALLOC
#define Memento_free    MEMENTO_UNDERLYING_FREE
#define Memento_realloc MEMENTO_UNDERLYING_REALLOC
#define Memento_calloc  MEMENTO_UNDERLYING_CALLOC

#define Memento_checkBlock(A)    0
#define Memento_checkAllMemory() 0
#define Memento_check()          0
#define Memento_setParanoia(A)   0
#define Memento_paranoidAt(A)    0
#define Memento_breakAt(A)       0
#define Memento_getBlockNum(A)   0
#define Memento_find(A)          0
#define Memento_breakpoint()     do {} while (0)
#define Memento_failAt(A)        0

#endif /* MEMENTO */

#endif /* MEMENTO_H */
