/* Copyright (C) 2018-2023 Artifex Software, Inc.
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

/* Simple example file to demonstrate multi-instance use of
 * the ghostscript/GPDL library. */

#ifdef _WIN32
/* Stop windows builds complaining about sprintf being insecure. */
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#define WINDOWS
#else
#include <pthread.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <assert.h>
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>

/* This can work either with the Ghostscript or GhostPDL
 * library. The code is the same for both. */
#if GHOSTPDL
#include "pcl/pl/plapi.h"   /* GSAPI - gpdf version */
#else
#include "psi/iapi.h"       /* GSAPI - ghostscript version */
#endif

/* We hold details of each threads working in a thread_data
 * structure. */
typedef struct
{
    /* What worker number are we ? */
    int thread_num;
    /* What input file? should this worker use? */
    char *in_file;
    /* Somewhere to store the thread id */
#ifdef WINDOWS
    HANDLE thread;
#else
    pthread_t thread;
#endif
    /* exit code for the thread */
    int code;
} thread_data;

/* The function to perform the work of the thread.
 * Starts a gs instance, runs a file, shuts it down.
 */
static
#ifdef WINDOWS
DWORD WINAPI
#else
void *
#endif
worker(void *td_)
{
    thread_data *td = (thread_data *)td_;
    int code;
    void *instance  = NULL;
    char out[32];

    /* Construct the argc/argv to pass to ghostscript. */
    int argc = 0;
    char *argv[10];

    sprintf(out, "multi_out_%d_", td->thread_num);
    strcat(out, "%d.png");
    argv[argc++] = "gpdl";
    argv[argc++] = "-sDEVICE=png16m";
    argv[argc++] = "-o";
    argv[argc++] = out;
    argv[argc++] = "-r100";
    argv[argc++] = td->in_file;

    /* Create a GS instance. */
    code = gsapi_new_instance(&instance, NULL);
    if (code < 0) {
        printf("Error %d in gsapi_new_instance\n", code);
        goto failearly;
    }

    /* Run our test. */
    code = gsapi_init_with_args(instance, argc, argv);
    if (code < 0) {
        printf("Error %d in gsapi_init_with_args\n", code);
        goto fail;
    }

    /* Close the interpreter down (important, or we will leak!) */
    code = gsapi_exit(instance);
    if (code < 0) {
        printf("Error %d in gsapi_exit\n", code);
        goto fail;
    }

fail:
    /* Delete the gs instance. */
    gsapi_delete_instance(instance);

failearly:
    td->code = code;

#ifdef WINDOWS
    return 0;
#else
    return NULL;
#endif
}

/* A list of input files to run. */
char *in_files[] =
{
  "../../examples/tiger.eps",
  "../../examples/golfer.eps",
  "../../examples/escher.ps",
  "../../examples/snowflak.ps"
#ifdef GHOSTPDL
  , "../../pcl/examples/grashopp.pcl"
  , "../../pcl/examples/owl.pcl"
  , "../../pcl/examples/tiger.xps"
#endif
};

#define NUM_INPUTS (sizeof(in_files)/sizeof(*in_files))
#define NUM_WORKERS (10)

int main(int argc, char *argv[])
{
    int failed = 0;
#ifndef WINDOWS
    int code;
#endif
    int i;
    thread_data td[NUM_WORKERS];

    /* Start NUM_WORKERS threads */
    for (i = 0; i < NUM_WORKERS; i++)
    {
        td[i].in_file = in_files[i % NUM_INPUTS];
        td[i].thread_num = i;

#ifdef WINDOWS
        td[i].thread = CreateThread(NULL, 0, worker, &td[i], 0, NULL);
#else
        code = pthread_create(&td[i].thread, NULL, worker, &td[i]);
        if (code != 0) {
            fprintf(stderr, "Thread %d creation failed\n", i);
            exit(1);
        }
#endif
    }

    /* Wait for them all to finish */
    for (i = 0; i < NUM_WORKERS; i++)
    {
        void *status = NULL;

#ifdef WINDOWS
        WaitForSingleObject(td[i].thread, INFINITE);
#else
        code = pthread_join(td[i].thread, &status);
        if (code != 0) {
            fprintf(stderr, "Thread join %d failed\n", i);
            exit(1);
        }
#endif
        /* All the threads should return with 0 */
        if (td[i].code != 0)
            failed = 1;
        fprintf(stderr, "Thread %d finished with %d\n", i, td[i].code);
    }

    return failed;
}
