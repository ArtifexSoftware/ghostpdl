/* Copyright (C) 2016-2018 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license. Refer to licensing information at http://www.artifex.com
   or contact Artifex Software, Inc.,  1305 Grant Avenue - Suite 200,
   Novato, CA 94945, U.S.A., +1(415)492-9861, for further information.
*/

#ifdef BOBBIN

#define BOBBIN_C
#include "bobbin.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#define MAX_NAME 32

#undef DEBUG_BOBBIN

typedef struct
{
    void *mutex;
    unsigned int flags;
    char name[MAX_NAME];
} Bobbin_mutex_data;

enum {
    BOBBIN_MUTEX_DELETED = 1
};

typedef struct
{
    void                *cond;
    unsigned int         flags;
    char                 name[MAX_NAME];
} Bobbin_cond_data;

enum {
    BOBBIN_COND_DELETED = 1
};

typedef struct
{
    int mutex;
    clock_t locktime;
    unsigned char locked;
} Bobbin_thr_mut_data;

typedef struct
{
    int cond;
    clock_t waittime;
    unsigned char waiting;
} Bobbin_thr_con_data;

typedef struct
{
    void                *thread;
    unsigned int         flags;
    clock_t              jointime;
    int                  num_mutexes;
    int                  max_mutexes;
    Bobbin_thr_mut_data *mutex;
    int                  num_conds;
    int                  max_conds;
    Bobbin_thr_con_data *cond;
    char                 name[MAX_NAME];
} Bobbin_thread_data;

enum {
    BOBBIN_THREAD_JOINED = 1,
    BOBBIN_THREAD_FINISHED = 2
};

typedef struct
{
    int                 inited;
#ifdef BOBBIN_PTHREADS
    pthread_mutex_t     lock;
#endif
    int                 max_mutexes;
    int                 num_mutexes;
    Bobbin_mutex_data  *mutex;
    int                 max_threads;
    int                 num_threads;
    Bobbin_thread_data *thread;
    int                 max_conds;
    int                 num_conds;
    Bobbin_cond_data   *cond;
} Bobbin_globals;

static Bobbin_globals bobbins;

static void Bobbin_breakpoint(void)
{
}

static void Bobbin_fin(void)
{
    int t, i;

    fprintf(stderr, "Bobbin report:\n");
    fprintf(stderr, "%d threads, %d mutexes\n",
            bobbins.num_threads,
            bobbins.num_mutexes);
    for (t = 0; t < bobbins.num_threads; t++)
    {
        Bobbin_thread_data *td = &bobbins.thread[t];
        fprintf(stderr, "  Thread %d: (%s)\n", t,
                td->name[0] ? td->name : "unnamed");
        for (i = 0; i < td->num_mutexes; i++)
        {
            int m = td->mutex[i].mutex;
            fprintf(stderr, "    Spent %g waiting for mutex %d (%s)%s\n",
                    td->mutex[i].locktime/(double)CLOCKS_PER_SEC, m,
                    bobbins.mutex[m].name[0] ? bobbins.mutex[m].name : "unnamed",
                    td->mutex[i].locked ? " LOCKED" : "");
        }
        for (i = 0; i < td->num_conds; i++)
        {
            int c = td->cond[i].cond;
            fprintf(stderr, "    Spent %g waiting for cond %d (%s)%s\n",
                    td->cond[i].waittime/(double)CLOCKS_PER_SEC, c,
                    bobbins.cond[c].name[0] ? bobbins.cond[c].name : "unnamed",
                    td->cond[i].waiting ? " WAITING" : "");
        }
    }
}

static void grow_threads(void)
{
    int new_size = bobbins.max_threads ? bobbins.max_threads*2 : 32;
    Bobbin_thread_data *new_thread;

    new_thread = realloc(bobbins.thread,
                         sizeof(*bobbins.thread) * new_size);
    if (new_thread == NULL)
    {
        fprintf(stderr, "Bobbin increase thread data block\n");
        exit(1);
    }
    bobbins.thread = new_thread;
    memset(&bobbins.thread[bobbins.max_threads], 0,
           (new_size - bobbins.max_threads) * sizeof(*bobbins.thread));
    bobbins.max_threads = new_size;
}

static void Bobbin_init(void)
{
    int t;

#ifdef BOBBIN_PTHREADS
    (void)pthread_mutex_init(&bobbins.lock, NULL);
#endif
    atexit(Bobbin_fin);
    bobbins.inited = 1;

    grow_threads();
    t = bobbins.num_threads++;
    strcpy(bobbins.thread[t].name, "Main");
#ifdef BOBBIN_PTHREADS
    bobbins.thread[t].thread = (void *)pthread_self();
#endif
}

static void Bobbin_lock(void)
{
    if (bobbins.inited == 0)
        Bobbin_init();
#ifdef BOBBIN_PTHREADS
    (void)pthread_mutex_lock(&bobbins.lock);
#endif
}

static void Bobbin_unlock(void)
{
#ifdef BOBBIN_PTHREADS
    (void)pthread_mutex_unlock(&bobbins.lock);
#endif
}

static int Bobbin_mutex(void *mutex)
{
    int i;

    for (i = 0; i < bobbins.num_mutexes; i++)
        if (bobbins.mutex[i].mutex == mutex && (bobbins.mutex[i].flags & BOBBIN_MUTEX_DELETED) == 0)
            break;
    if (i == bobbins.num_mutexes)
    {
        fprintf(stderr, "Unknown mutex!\n");
        Bobbin_breakpoint();
    }
    return i;
}

static int Bobbin_mutex_idx(int thread, int mutex)
{
    Bobbin_thread_data *td = &bobbins.thread[thread];
    int i;

    for (i = 0; i < td->num_mutexes; i++)
    {
        if (td->mutex[i].mutex == mutex)
            break;
    }
    if (i == td->num_mutexes)
    {
        if (i == td->max_mutexes)
        {
            int new_size = i ? i*2 : 32;
            Bobbin_thr_mut_data *mutexes;

            mutexes = realloc(td->mutex,
                              sizeof(*mutexes) * new_size);
            if (mutexes == NULL)
            {
                fprintf(stderr, "Bobbin failed to increase thread mutex data block\n");
                exit(1);
            }
            td->mutex = mutexes;
            memset(&td->mutex[i], 0,
                   (new_size - i) * sizeof(*mutexes));
            td->max_mutexes = new_size;
        }
        td->mutex[i].mutex = mutex;
        td->num_mutexes++;
    }
    return i;
}

static int Bobbin_cond(void *cond)
{
    int i;

    for (i = 0; i < bobbins.num_conds; i++)
        if (bobbins.cond[i].cond == cond && (bobbins.cond[i].flags & BOBBIN_COND_DELETED) == 0)
            break;
    if (i == bobbins.num_conds)
    {
        fprintf(stderr, "Unknown condition variable!\n");
        Bobbin_breakpoint();
    }
    return i;
}

static int Bobbin_cond_idx(int thread, int cond)
{
    Bobbin_thread_data *td = &bobbins.thread[thread];
    int i;

    for (i = 0; i < td->num_conds; i++)
    {
        if (td->cond[i].cond == cond)
            break;
    }
    if (i == td->num_conds)
    {
        if (i == td->max_conds)
        {
            int new_size = i ? i*2 : 32;
            Bobbin_thr_con_data *conds;

            conds = realloc(td->cond,
                            sizeof(*conds) * new_size);
            if (conds == NULL)
            {
                fprintf(stderr, "Bobbin failed to increase thread cond data block\n");
                exit(1);
            }
            td->cond = conds;
            memset(&td->cond[i], 0,
                   (new_size - i) * sizeof(*conds));
            td->max_conds = new_size;
        }
        td->cond[i].cond = cond;
        td->num_conds++;
    }
    return i;
}

#ifdef BOBBIN_PTHREADS
#define BOBBIN_THREAD_EQUAL(A, B) pthread_equal((pthread_t)(A), (pthread_t)(B))
#else
#define BOBBIN_THREAD_EQUAL(A, B) (A == B)
#endif

static int Bobbin_thread(void *thread)
{
    int i;

    for (i = 0; i < bobbins.num_threads; i++)
        if (BOBBIN_THREAD_EQUAL(bobbins.thread[i].thread, thread) && 
            (bobbins.thread[i].flags & BOBBIN_THREAD_FINISHED) == 0)
            break;
    if (i == bobbins.num_threads)
    {
        fprintf(stderr, "Unknown thread!\n");
        Bobbin_breakpoint();
    }
    return i;
}

static int Bobbin_add_thread(void)
{
    int t;

    if (bobbins.inited == 0)
        Bobbin_init();

    Bobbin_lock();

    t = bobbins.num_threads++;
    if (t == bobbins.max_threads)
    {
        grow_threads();
    }

    Bobbin_unlock();

    return t;
}

static void Bobbin_add_mutex(void *mutex)
{
    int i;

    Bobbin_lock();

    for (i = 0; i < bobbins.num_mutexes; i++)
        if (bobbins.mutex[i].mutex == mutex && (bobbins.mutex[i].flags & BOBBIN_MUTEX_DELETED) == 0)
            break;
    if (i != bobbins.num_mutexes)
    {
        fprintf(stderr, "Reinitialising existing mutex!\n");
        Bobbin_breakpoint();
    }
    else
    {
        if (i == bobbins.max_mutexes)
        {
            int new_size = i ? i * 2 : 32;
            Bobbin_mutex_data *new_mutex;

            new_mutex = realloc(bobbins.mutex,
                                sizeof(*bobbins.mutex) * new_size);
            if (new_mutex == NULL)
            {
                fprintf(stderr, "Bobbin failed to increase mutex data block\n");
                exit(1);
            }
            bobbins.mutex = new_mutex;
            memset(&bobbins.mutex[i], 0,
                   (new_size - i) * sizeof(*bobbins.mutex));
            bobbins.max_mutexes = new_size;
        }
        bobbins.mutex[i].mutex = mutex;
        bobbins.mutex[i].flags = 0;
        bobbins.num_mutexes++;
    }

    Bobbin_unlock();
}

static void Bobbin_del_mutex(void *mutex)
{
    int i, t;

    Bobbin_lock();

    for (i = 0; i < bobbins.num_mutexes; i++)
        if (bobbins.mutex[i].mutex == mutex && (bobbins.mutex[i].flags & BOBBIN_MUTEX_DELETED) == 0)
            break;
    if (i == bobbins.num_mutexes)
    {
        fprintf(stderr, "Deleting non-existent mutex!\n");
        Bobbin_breakpoint();
    }
    else
    {
        int borked = 0;
        bobbins.mutex[i].flags |= BOBBIN_MUTEX_DELETED;
        for (t = 0; t < bobbins.num_threads; t++)
        {
            int mi;
            Bobbin_thread_data *td = &bobbins.thread[t];
            for (mi = 0; mi < td->num_mutexes; mi++)
            {
                if (td->mutex[mi].mutex == i && td->mutex[mi].locked)
                {
                    fprintf(stderr, "Thread %d has mutex %d locked at deletion time\n");
                    borked = 1;
                }
            }
        }
        if (borked)
            Bobbin_breakpoint();
    }

    Bobbin_unlock();
}

typedef struct
{
    clock_t time;
    int thread;
    int mutex;
    int mutex_idx;
}
Bobbin_mutex_lock_data;

static void Bobbin_prelock_mutex(void *thread, void *mutex, Bobbin_mutex_lock_data *mld)
{
    int mi;
    Bobbin_lock();
    mld->thread = Bobbin_thread(thread);
    mld->mutex = Bobbin_mutex(mutex);
    mi = mld->mutex_idx = Bobbin_mutex_idx(mld->thread, mld->mutex);
    if (bobbins.thread[mld->thread].mutex[mi].locked)
    {
        fprintf(stderr, "Thread %d trying to lock locked mutex %d!\n", mld->thread, mld->mutex);
        Bobbin_breakpoint();
    }
    bobbins.thread[mld->thread].mutex[mi].locked = 1;
    Bobbin_unlock();
    mld->time = clock();
}

static void Bobbin_postlock_mutex(Bobbin_mutex_lock_data *mld)
{
    clock_t time = clock();

    Bobbin_lock();
    time -= mld->time;
    bobbins.thread[mld->thread].mutex[mld->mutex_idx].locktime += time;
    Bobbin_unlock();
}

static void Bobbin_unlock_mutex(void *thread, void *mutex)
{
    int t, m, mi;
    Bobbin_thread_data *td;

    Bobbin_lock();
    t = Bobbin_thread(thread);
    m = Bobbin_mutex(mutex);
    mi = Bobbin_mutex_idx(t, m);
    td = &bobbins.thread[t];
    if (td->mutex[mi].locked == 0)
    {
        fprintf(stderr, "Thread %d trying to unlock unlocked mutex %d!\n", t, m);
        Bobbin_breakpoint();
    }
    td->mutex[mi].locked = 0;
    Bobbin_unlock();
}

static void Bobbin_add_cond(void *cond)
{
    int i;

    Bobbin_lock();

    for (i = 0; i < bobbins.num_conds; i++)
        if (bobbins.cond[i].cond == cond && (bobbins.cond[i].flags & BOBBIN_COND_DELETED) == 0)
            break;
    if (i != bobbins.num_conds)
    {
        fprintf(stderr, "Reinitialising existing cond!\n");
        Bobbin_breakpoint();
    }
    else
    {
        if (i == bobbins.max_conds)
        {
            int new_size = i ? i * 2 : 32;
            Bobbin_cond_data *new_cond;

            new_cond = realloc(bobbins.cond,
                               sizeof(*bobbins.cond) * new_size);
            if (new_cond == NULL)
            {
                fprintf(stderr, "Bobbin failed to increase cond data block\n");
                exit(1);
            }
            bobbins.cond = new_cond;
            memset(&bobbins.cond[i], 0,
                   (new_size - i) * sizeof(*bobbins.cond));
            bobbins.max_conds = new_size;
        }
        bobbins.cond[i].cond = cond;
        bobbins.cond[i].flags = 0;
        bobbins.num_conds++;
    }

    Bobbin_unlock();
}

static void Bobbin_del_cond(void *cond)
{
    int i, t;

    Bobbin_lock();

    for (i = 0; i < bobbins.num_conds; i++)
        if (bobbins.cond[i].cond == cond && (bobbins.cond[i].flags & BOBBIN_COND_DELETED) == 0)
            break;
    if (i == bobbins.num_conds)
    {
        fprintf(stderr, "Deleting non-existent cond!\n");
        Bobbin_breakpoint();
    }
    else
    {
        int borked = 0;
        bobbins.cond[i].flags |= BOBBIN_COND_DELETED;
        for (t = 0; t < bobbins.num_threads; t++)
        {
            int i;
            Bobbin_thread_data *td = &bobbins.thread[t];
            for (i = 0; i < td->num_conds; i++)
            {
                if (td->cond[i].cond == i && td->cond[i].waiting)
                {
                    fprintf(stderr, "Thread %d has cond %d waiting at deletion time\n");
                    borked = 1;
                }
            }
        }
        if (borked)
            Bobbin_breakpoint();
    }

    Bobbin_unlock();
}

typedef struct
{
    clock_t time;
    int thread;
    int cond;
    int cond_idx;
}
Bobbin_cond_wait_data;

static void Bobbin_prewait_cond(void *thread, void *cond, Bobbin_cond_wait_data *cwd)
{
    int i;
    Bobbin_lock();
    cwd->thread = Bobbin_thread(thread);
    cwd->cond = Bobbin_cond(cond);
    i = cwd->cond_idx = Bobbin_cond_idx(cwd->thread, cwd->cond);
    bobbins.thread[cwd->thread].cond[i].waiting++;
    Bobbin_unlock();
    cwd->time = clock();
}

static void Bobbin_postwait_cond(Bobbin_cond_wait_data *cwd)
{
    clock_t time = clock();

    Bobbin_lock();
    time -= cwd->time;
    bobbins.thread[cwd->thread].cond[cwd->cond_idx].waiting--;
    bobbins.thread[cwd->thread].cond[cwd->cond_idx].waittime += time;
    Bobbin_unlock();
}

static void label_thread(void *thread, const char *name)
{
    int t;
    int l;

    if (thread == NULL || name == NULL)
        return;

    Bobbin_lock();
    t = Bobbin_thread(thread);

    strncpy(&bobbins.thread[t].name[0], name, MAX_NAME);
    bobbins.thread[t].name[MAX_NAME-1] = 0;
    Bobbin_unlock();
}

static void label_mutex(pthread_mutex_t *mutex, const char *name)
{
    int m;
    int l;

    if (mutex == NULL || name == NULL)
        return;

    Bobbin_lock();
    m = Bobbin_mutex(mutex);

    strncpy(&bobbins.mutex[m].name[0], name, MAX_NAME);
    bobbins.mutex[m].name[MAX_NAME-1] = 0;
    Bobbin_unlock();
}

static void label_cond(pthread_cond_t *cond, const char *name)
{
    int c;
    int l;

    if (cond == NULL || name == NULL)
        return;

    Bobbin_lock();
    c = Bobbin_cond(cond);

    strncpy(&bobbins.cond[c].name[0], name, MAX_NAME);
    bobbins.cond[c].name[MAX_NAME-1] = 0;
    Bobbin_unlock();
}

#ifdef BOBBIN_WINDOWS
#endif

#ifdef BOBBIN_PTHREADS

#include "pthread.h"

int Bobbin_pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr)
{
    Bobbin_add_mutex((void *)mutex);
#ifdef DEBUG_BOBBIN
    fprintf(stderr, "mutex_init %p\n", mutex);
    fflush(stderr);
#endif
    return pthread_mutex_init(mutex, attr);
}

int Bobbin_pthread_mutex_destroy(pthread_mutex_t *mutex)
{
    Bobbin_del_mutex((void *)mutex);
#ifdef DEBUG_BOBBIN
    fprintf(stderr, "mutex_destroy %p\n", mutex);
    fflush(stderr);
#endif
    return pthread_mutex_destroy(mutex);
}

int Bobbin_pthread_mutex_lock(pthread_mutex_t *mutex)
{
    Bobbin_mutex_lock_data mld;
    int e;

#ifdef DEBUG_BOBBIN
    fprintf(stderr, "mutex_lock %p\n", mutex);
    fflush(stderr);
#endif

    Bobbin_prelock_mutex((void *)pthread_self(), mutex, &mld);
    e = pthread_mutex_lock(mutex);
    Bobbin_postlock_mutex(&mld);
    return e;
}

int Bobbin_pthread_mutex_unlock(pthread_mutex_t *mutex)
{
#ifdef DEBUG_BOBBIN
    fprintf(stderr, "mutex_unlock %p\n", mutex);
    fflush(stderr);
#endif

    Bobbin_unlock_mutex((void *)pthread_self(), mutex);
    return pthread_mutex_unlock(mutex);
}

int Bobbin_pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr)
{
    Bobbin_add_cond((void *)cond);
#ifdef DEBUG_BOBBIN
    fprintf(stderr, "cond_init %p\n", cond);
    fflush(stderr);
#endif
    return pthread_cond_init(cond, attr);
}

int Bobbin_pthread_cond_destroy(pthread_cond_t *cond)
{
    Bobbin_del_cond((void *)cond);
#ifdef DEBUG_BOBBIN
    fprintf(stderr, "cond_destroy %p\n", cond);
    fflush(stderr);
#endif
    return pthread_cond_destroy(cond);
}

int Bobbin_pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
    Bobbin_cond_wait_data cwd;
    int e;

#ifdef DEBUG_BOBBIN
    fprintf(stderr, "cond_wait %p\n", cond);
    fflush(stderr);
#endif

    Bobbin_prewait_cond((void *)pthread_self(), cond, &cwd);
    e = pthread_cond_wait(cond, mutex);
    Bobbin_postwait_cond(&cwd);
    return e;
}

int Bobbin_pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                          void *(*fn)(void *), void *arg)
{
    int t = Bobbin_add_thread();
    int e;

    e = pthread_create((pthread_t *)&bobbins.thread[t].thread, attr, fn, arg);

    *thread = (pthread_t)bobbins.thread[t].thread;

#ifdef DEBUG_BOBBIN
    fprintf(stderr, "pthread_create %p\n", *thread);
    fflush(stderr);
#endif

    return e;
}

int Bobbin_pthread_join(pthread_t thread, void **arg)
{
    int t;
    clock_t t0, t1;
    int e;

#ifdef DEBUG_BOBBIN
    fprintf(stderr, "pthread_join %p\n", thread);
    fflush(stderr);
#endif

    Bobbin_lock();
    t = Bobbin_thread((void *)thread);
    bobbins.thread[t].flags |= BOBBIN_THREAD_JOINED;
    Bobbin_unlock();

    t0 = clock();
    e = pthread_join(thread, arg);
    t1 = clock();

    Bobbin_lock();
    bobbins.thread[t].flags |= BOBBIN_THREAD_FINISHED;
    bobbins.thread[t].jointime = t1-t0;
    Bobbin_unlock();

    return e;
}

void Bobbin_label_thread(pthread_t thread, const char *name)
{
#ifdef DEBUG_BOBBIN
    fprintf(stderr, "label_thread %p\n", thread);
    fflush(stderr);
#endif
    label_thread((void *)thread, name);
}

void Bobbin_label_mutex(pthread_mutex_t *mutex, const char *name)
{
#ifdef DEBUG_BOBBIN
    fprintf(stderr, "label_mutex %p\n", mutex);
    fflush(stderr);
#endif
    label_mutex((void *)mutex, name);
}

void Bobbin_label_cond(pthread_cond_t *cond, const char *name)
{
#ifdef DEBUG_BOBBIN
    fprintf(stderr, "label_cond %p\n", cond);
    fflush(stderr);
#endif
    label_cond((void *)cond, name);
}

#endif

#endif /* BOBBIN */
