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

/* Bobbin: A library to aid debugging/performance tuning of threads.
 *
 * Usage (with C):
 *    First, build your project with BOBBIN defined, and include this
 *    header file wherever you use threading functions.
 *    This header file will use macros to point threading functions to
 *    Bobbin equivalents.
 *
 *    Run your program, and all threading functions should be redirected
 *    through here. When the program exits, you will get an interesting
 *    list of facts.
 */

#ifndef BOBBIN_H

#define BOBBIN_H

#if defined(BOBBIN) && BOBBIN

#ifdef _WIN32
#include <windows.h>

#define BOBBIN_WINDOWS

#error Not yet!

#else

#include <pthread.h>

#define BOBBIN_PTHREADS

#ifndef BOBBIN_C

#define pthread_mutex_init     Bobbin_pthread_mutex_init
#define pthread_mutex_destroy  Bobbin_pthread_mutex_destroy
#define pthread_mutex_lock     Bobbin_pthread_mutex_lock
#define pthread_mutex_unlock   Bobbin_pthread_mutex_unlock

#define pthread_create         Bobbin_pthread_create
#define pthread_join           Bobbin_pthread_join

#define pthread_cond_init      Bobbin_pthread_cond_init
#define pthread_cond_destroy   Bobbin_pthread_cond_destroy
#define pthread_cond_wait      Bobbin_pthread_cond_wait
//#define pthread_cond_signal    Bobbin_pthread_cond_signal

#endif

int Bobbin_pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr);
int Bobbin_pthread_mutex_destroy(pthread_mutex_t *mutex);
int Bobbin_pthread_mutex_lock(pthread_mutex_t *mutex);
int Bobbin_pthread_mutex_unlock(pthread_mutex_t *mutex);
int Bobbin_pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr);
int Bobbin_pthread_cond_destroy(pthread_cond_t *cond);
int Bobbin_pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex);
int Bobbin_pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                          void *(*fn)(void *), void *arg);
int Bobbin_pthread_join(pthread_t thread, void **arg);
void Bobbin_label_thread(pthread_t thread, const char *name);
void Bobbin_label_mutex(pthread_mutex_t *mutex, const char *name);
void Bobbin_label_cond(pthread_cond_t *cond, const char *name);

#endif

#else

#define Bobbin_label_thread(A,B) do { } while (0)
#define Bobbin_label_mutex(A,B) do { } while (0)
#define Bobbin_label_cond(A,B) do { } while (0)

#endif

#endif /* BOBBIN_H */
