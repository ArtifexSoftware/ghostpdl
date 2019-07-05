/* Copyright (C) 2001-2019 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
*/


/* plapi.h */
/* pcl6 as a library or dll api */

#ifndef gsapi_INCLUDED
#  define gsapi_INCLUDED

/*
 * This API level is intended to hide everything behind
 * a simple main_like  argc, argv  interface
 */

/* Exported functions may need different prefix
 *  GSDLLEXPORT marks functions as exported
 *  GSDLLAPI is the calling convention used on functions exported
 * When you include this header file in the caller, you may
 * need to change the definitions by defining these
 * before including this header file.
 * Make sure you get the calling convention correct, otherwise your
 * program will crash soon after returning due to stack corruption.
 */

#ifdef __WINDOWS__
# define _Windows
#endif

#ifdef _Windows
# ifndef GSDLLEXPORT
#  define GSDLLEXPORT __declspec(dllexport)
# endif
# ifndef GSDLLAPI
#  define GSDLLAPI __stdcall
# endif
# ifndef GSDLLCALL
#  define GSDLLCALL __stdcall
# endif
#endif /* _Windows */

#if defined(OS2) && defined(__IBMC__)
# ifndef GSDLLAPI
#  define GSDLLAPI _System
# endif
# ifndef GSDLLCALL
#  define GSDLLCALL _System
# endif
#endif /* OS2 && __IBMC */

#ifdef __MACINTOSH__
# pragma export on
#endif

#ifndef GSDLLEXPORT
# define GSDLLEXPORT
#endif
#ifndef GSDLLAPI
# define GSDLLAPI
#endif
#ifndef GSDLLCALL
# define GSDLLCALL
#endif

#if defined(__IBMC__)
# define GSDLLAPIPTR * GSDLLAPI
# define GSDLLCALLPTR * GSDLLCALL
#else
# define GSDLLAPIPTR GSDLLAPI *
# define GSDLLCALLPTR GSDLLCALL *
#endif

#ifndef display_callback_DEFINED
# define display_callback_DEFINED
typedef struct display_callback_s display_callback;
#endif

#ifndef gs_memory_DEFINED
#  define gs_memory_DEFINED
typedef struct gs_memory_s gs_memory_t;
#endif

#ifndef gp_file_DEFINED
#  define gp_file_DEFINED
typedef struct gp_file_s gp_file;
#endif

#ifdef _Windows
GSDLLEXPORT int GSDLLAPI
pl_wchar_to_utf8(char *out, const void *in);
#endif

typedef struct gsapi_revision_s {
    const char *product;
    const char *copyright;
    long revision;
    long revisiondate;
} gsapi_revision_t;

/* Get version numbers and strings.
 * This is safe to call at any time.
 * You should call this first to make sure that the correct version
 * of GhostPDL is being used.
 * pr is a pointer to a revision structure.
 * len is the size of this structure in bytes.
 * Returns 0 if OK, or if len too small (additional parameters
 * have been added to the structure) it will return the required
 * size of the structure.
 */
GSDLLEXPORT int GSDLLAPI gsapi_revision(gsapi_revision_t *pr, int len);

/* Create a new instance of GhostPDL.
 * This instance is passed to most other API functions.
 * The caller_handle will be provided to callback functions.
 *
 * On success: Returns 0, with *instance set to the newly created
 *      instance handle.
 * On error: (such as the maximum number of instances being exceeded)
 * this will return <0 and set *instance=NULL.
 */
GSDLLEXPORT int GSDLLAPI gsapi_new_instance(void **instance, void *caller_handle);

/* Destroy an instance of GhostPDL
 * Before you call this, GhostPDL must have finished.
 * If GhostPDL has been initialised, you must call gsapi_exit()
 * before gsapi_delete_instance.
 */
GSDLLEXPORT int GSDLLAPI gsapi_delete_instance(void *instance);

/* Set the callback functions for stdio
 * The stdin callback function should return the number of
 * characters read, 0 for EOF, or -1 for error.
 * The stdout and stderr callback functions should return
 * the number of characters written.
 * If a callback address is NULL, the real stdio will be used.
 */
GSDLLEXPORT int GSDLLAPI
gsapi_set_stdio(void *instance,
    int (GSDLLCALLPTR stdin_fn)(void *caller_handle, char *buf, int len),
    int (GSDLLCALLPTR stdout_fn)(void *caller_handle, const char *str, int len),
    int (GSDLLCALLPTR stderr_fn)(void *caller_handle, const char *str, int len));

/* Set the callback function for polling.
 * This is used for handling window events or cooperative
 * multitasking.  This function will only be called if
 * Ghostscript was compiled with CHECK_INTERRUPTS
 * as described in gpcheck.h.
 * The polling function should return 0 if all is well,
 * and negative if it wants ghostscript to abort.
 * The polling function must be fast.
 */
GSDLLEXPORT int GSDLLAPI gsapi_set_poll(void *instance,
    int (GSDLLCALLPTR poll_fn)(void *caller_handle));

/* Set the display device callback structure.
 * If the display device is used, this must be called
 * after gsapi_new_instance() and before gsapi_init_with_args().
 * See gdevdisp.h for more details.
 */
GSDLLEXPORT int GSDLLAPI gsapi_set_display_callback(
   void *instance, display_callback *callback);

/* Set the string containing the list of default device names
 * for example "display x11alpha x11 bbox". Allows the calling
 * application to influence which device(s) gs will try in order
 * to select the default device
 *
 * If this is to be called, it must be called after
 * gsapi_new_instance() and before gsapi_init_with_args().
 */
GSDLLEXPORT int GSDLLAPI
gsapi_set_default_device_list(void *instance, char *list, int listlen);

/* Returns a pointer to the current default device string.
 */
GSDLLEXPORT int GSDLLAPI
gsapi_get_default_device_list(void *instance, char **list, int *listlen);

/* Set the encoding used for the args. By default we assume
 * 'local' encoding. For windows this equates to whatever the current
 * codepage is. For linux this is utf8.
 *
 * Use of this API (gsapi) with 'local' encodings (and hence without calling
 * this function) is now deprecated!
 */
GSDLLEXPORT int GSDLLAPI gsapi_set_arg_encoding(void *instance,
                                                int encoding);

enum {
    PL_ARG_ENCODING_LOCAL = 0,
    PL_ARG_ENCODING_UTF8 = 1,
    PL_ARG_ENCODING_UTF16LE = 2
};

/* Initialise the interpreter.
 * If this returns gs_error_Quit, then the interpreter quit due to
 * an explicit .quit. This is not an error. You must call gsapi_exit()
 * and gsapi_destroy_instance(). You may not call any other plapi
 * functions.
 * If this returns gs_error_Info, then usage info should be displayed.
 * This is not an error. You must call gsapi_exit() and
 * gsapi_destroy_instance(). You may not call any other plapi functions.
 * If this returns another negative value, this is an error.
 * Normal return is 0 or greater. Callers can then call other
 * gsapi_run... functions if required, ending with gsapi_exit and
 * gsapi_destroy_instance.
 */
GSDLLEXPORT int GSDLLAPI gsapi_init_with_args(void *instance, int argc, char **argv);

GSDLLEXPORT int GSDLLAPI gsapi_run_file(void *instance, const char *file_name);

GSDLLEXPORT int GSDLLAPI gsapi_exit(void *instance);

GSDLLEXPORT int GSDLLAPI gsapi_run_string_begin(void *instance);

GSDLLEXPORT int GSDLLAPI gsapi_run_string_continue(void *instance, const char *str, unsigned int length);

GSDLLEXPORT int GSDLLAPI gsapi_run_string_end(void *instance);

typedef enum {
    gs_spt_invalid = -1,
    gs_spt_null    = 0,   /* void * is NULL */
    gs_spt_bool    = 1,   /* void * is NULL (false) or non-NULL (true) */
    gs_spt_int     = 2,   /* void * is a pointer to an int */
    gs_spt_float   = 3,   /* void * is a float * */
    gs_spt_name    = 4,   /* void * is a char * */
    gs_spt_string  = 5    /* void * is a char * */
} gs_set_param_type;
GSDLLEXPORT int GSDLLAPI gsapi_set_param(void *instance, gs_set_param_type type, const char *param, const void *value);

enum {
    GS_PERMIT_FILE_READING = 0,
    GS_PERMIT_FILE_WRITING = 1,
    GS_PERMIT_FILE_CONTROL = 2
};

/* Add a path to one of the sets of permitted paths. */
GSDLLEXPORT int GSDLLAPI
gsapi_add_control_path(void *instance, int type, const char *path);

/* Remove a path from one of the sets of permitted paths. */
GSDLLEXPORT int GSDLLAPI
gsapi_remove_control_path(void *instance, int type, const char *path);

/* Purge all the paths from the one of the sets of permitted paths. */
GSDLLEXPORT void GSDLLAPI
gsapi_purge_control_paths(void *instance, int type);

GSDLLEXPORT void GSDLLAPI
gsapi_activate_path_control(void *instance, int enable);

GSDLLEXPORT int GSDLLAPI
gsapi_is_path_control_active(void *instance);

/* Details of gp_file can be found in gp.h.
 * Users wanting to use this function should include
 * that file. Not included here to avoid bloating the
 * API inclusions for the majority of people who won't
 * want it. */
typedef struct {
    int (*open_file)(const gs_memory_t *mem,
                           void        *secret,
                     const char        *fname,
                     const char        *mode,
                           gp_file    **file);
    int (*open_pipe)(const gs_memory_t *mem,
                           void        *secret,
                     const char        *fname,
                           char        *rfname, /* 4096 bytes */
                     const char        *mode,
                           gp_file    **file);
    int (*open_scratch)(const gs_memory_t *mem,
                              void        *secret,
                        const char        *prefix,
                              char        *rfname, /* 4096 bytes */
                        const char        *mode,
                              int          rm,
                              gp_file    **file);
    int (*open_printer)(const gs_memory_t *mem,
                              void        *secret,
                              char        *fname, /* 4096 bytes */
                              int          binary,
                              gp_file    **file);
    int (*open_handle)(const gs_memory_t *mem,
                             void        *secret,
                             char        *fname, /* 4096 bytes */
                       const char        *mode,
                             gp_file    **file);
} gsapi_fs_t;

GSDLLEXPORT int GSDLLAPI
gsapi_add_fs(void *instance, gsapi_fs_t *fs, void *secret);

GSDLLEXPORT void GSDLLAPI
gsapi_remove_fs(void *instance, gsapi_fs_t *fs, void *secret);

#endif /* gsapi_INCLUDED */
