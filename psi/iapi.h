/* Copyright (C) 2001-2020 Artifex Software, Inc.
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



/*
 * Public API for Ghostscript interpreter
 * for use both as DLL and for static linking.
 *
 * Should work for Windows, OS/2, Linux, Mac.
 *
 * DLL exported functions should be as similar as possible to imain.c
 * You will need to include "ierrors.h".
 *
 * Current problems:
 * 1. Ghostscript does not support multiple instances.
 * 2. Global variables in gs_main_instance_default()
 *    and gsapi_instance_counter
 */

/* Exported functions may need different prefix
 *  GSDLLEXPORT marks functions as exported
 *  GSDLLAPI is the calling convention used on functions exported
 *   by Ghostscript
 *  GSDLLCALL is used on callback functions called by Ghostscript
 * When you include this header file in the caller, you may
 * need to change the definitions by defining these
 * before including this header file.
 * Make sure you get the calling convention correct, otherwise your
 * program will crash either during callbacks or soon after returning
 * due to stack corruption.
 */

#ifndef iapi_INCLUDED
#  define iapi_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WINDOWS_) || defined(__WINDOWS__)
# ifndef _Windows
#  define _Windows
# endif
#endif

#ifdef _Windows
# ifndef GSDLLEXPORT
/* We don't need both the "__declspec(dllexport)" declaration
 * and the listing in the .def file - having both results in
 * a linker warning on x64 builds (but is incorrect on x86, too)
 */
#  if 0
#    define GSDLLEXPORT __declspec(dllexport)
#  else
#    define GSDLLEXPORT
#  endif
# endif
# ifdef __MINGW32__
/* export stdcall functions as "name" instead of "_name@ordinal" */
#  undef GSDLLAPI
#  define GSDLLAPI
# endif
# ifndef GSDLLAPI
#  define GSDLLAPI __stdcall
# endif
# ifndef GSDLLCALL
#  define GSDLLCALL __stdcall
# endif
#endif  /* _Windows */

#if defined(OS2) && defined(__IBMC__)
# ifndef GSDLLAPI
#  define GSDLLAPI _System
# endif
# ifndef GSDLLCALL
#  define GSDLLCALL _System
# endif
#endif	/* OS2 && __IBMC */

#ifdef __MACOS__
# pragma export on
#endif

#ifndef GSDLLEXPORT
# ifdef __GNUC__
#   define GSDLLEXPORT __attribute__ ((visibility ("default")))
# else
#   define GSDLLEXPORT
# endif
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

typedef struct gsapi_revision_s {
    const char *product;
    const char *copyright;
    long revision;
    long revisiondate;
} gsapi_revision_t;

/* Get version numbers and strings.
 * This is safe to call at any time.
 * You should call this first to make sure that the correct version
 * of the Ghostscript is being used.
 * pr is a pointer to a revision structure.
 * len is the size of this structure in bytes.
 * Returns 0 if OK, or if len too small (additional parameters
 * have been added to the structure) it will return the required
 * size of the structure.
 */
GSDLLEXPORT int GSDLLAPI
gsapi_revision(gsapi_revision_t *pr, int len);

/*
 * WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
 *  Ghostscript supports only one instance.
 *  The current implementation uses a global static instance
 *  counter to make sure that only a single instance is used.
 *  If you try to create two instances, the second attempt
 *  will return < 0 and set pinstance to NULL.
 * WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
 */
/* Create a new instance of Ghostscript.
 * This instance is passed to most other API functions.
 * The caller_handle will be provided to callback functions.
 */

GSDLLEXPORT int GSDLLAPI
gsapi_new_instance(void **pinstance, void *caller_handle);

/*
 * WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
 *  Ghostscript supports only one instance.
 *  The current implementation uses a global static instance
 *  counter to make sure that only a single instance is used.
 * WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
 */
/* Destroy an instance of Ghostscript
 * Before you call this, Ghostscript must have finished.
 * If Ghostscript has been initialised, you must call gsapi_exit()
 * before gsapi_delete_instance.
 */
GSDLLEXPORT void GSDLLAPI
gsapi_delete_instance(void *instance);

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

/* Does the same as the above, but using the caller_handle given here,
 * rather than the default one specified at gsapi_new_instance time. */
GSDLLEXPORT int GSDLLAPI
gsapi_set_stdio_with_handle(void *instance,
    int (GSDLLCALLPTR stdin_fn)(void *caller_handle, char *buf, int len),
    int (GSDLLCALLPTR stdout_fn)(void *caller_handle, const char *str, int len),
    int (GSDLLCALLPTR stderr_fn)(void *caller_handle, const char *str, int len),
    void *caller_handle);

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

/* Does the same as the above, but using the caller_handle given here,
 * rather than the default one specified at gsapi_new_instance time. */
GSDLLEXPORT int GSDLLAPI gsapi_set_poll_with_handle(void *instance,
    int (GSDLLCALLPTR poll_fn)(void *caller_handle), void *caller_handle);

/* Set the display device callback structure.
 * If the display device is used, this must be called
 * after gsapi_new_instance() and before gsapi_init_with_args().
 * See gdevdisp.h for more details.
 * DEPRECATED: Use the gsapi_register_callback mechanism instead.
 */
GSDLLEXPORT int GSDLLAPI gsapi_set_display_callback(
   void *instance, display_callback *callback);

/* The callout mechanism allows devices to query "callers" (users of the
 * DLL) in device specific ways. The callout function pointer type will
 * be called with:
 *   callout_handle = the value given at registration
 *   device_name    = the name of the current device
 *   id             = An integer, guaranteed to be unique within the
 *                    callouts from a given device, identifying the
 *                    purpose of this call.
 *   size           = device/id specific, but typically the size of 'data'.
 *   data           = device/id specific, but typically the pointer to
 *                    an in/out data block.
 *  Returns an error code (gs_error_unknownerror (-1) if unclaimed,
 *  non-negative on success, standard gs error numbers recommended).
 */
typedef int (*gs_callout)(void *instance,
                          void *callout_handle,
                          const char *device_name,
                          int id,
                          int size,
                          void *data);

/* Register a handler for gs callouts.
 * This must be called after gsapi_new_instance() and (typically)
 * before gsapi_init_with_args().
 */
GSDLLEXPORT int GSDLLAPI gsapi_register_callout(
   void *instance, gs_callout callout, void *callout_handle);

/* Deregister a handler for gs callouts. */
GSDLLEXPORT void GSDLLAPI gsapi_deregister_callout(
   void *instance, gs_callout callout, void *callout_handle);

/* Set the string containing the list of default device names
 * for example "display x11alpha x11 bbox". Allows the calling
 * application to influence which device(s) gs will try in order
 * to select the default device
 *
 * *Must* be called after gsapi_new_instance() and before
 * gsapi_init_with_args().
 */
GSDLLEXPORT int GSDLLAPI
gsapi_set_default_device_list(void *instance, const char *list, int listlen);

/* Returns a pointer to the current default device string
 * *Must* be called after gsapi_new_instance().
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
    GS_ARG_ENCODING_LOCAL = 0,
    GS_ARG_ENCODING_UTF8 = 1,
    GS_ARG_ENCODING_UTF16LE = 2
};

/* Initialise the interpreter.
 * This calls gs_main_init_with_args() in imainarg.c
 * 1. If quit or EOF occur during gsapi_init_with_args(),
 *    the return value will be gs_error_Quit.  This is not an error.
 *    You must call gsapi_exit() and must not call any other
 *    gsapi_XXX functions.
 * 2. If usage info should be displayed, the return value will be gs_error_Info
 *    which is not an error.  Do not call gsapi_exit().
 * 3. Under normal conditions this returns 0.  You would then
 *    call one or more gsapi_run_*() functions and then finish
 *    with gsapi_exit().
 */
GSDLLEXPORT int GSDLLAPI gsapi_init_with_args(void *instance,
    int argc, char **argv);

#ifdef __WIN32__
GSDLLEXPORT int GSDLLAPI gsapi_init_with_argsA(void *instance,
    int argc, char **argv);

GSDLLEXPORT int GSDLLAPI gsapi_init_with_argsW(void *instance,
    int argc, wchar_t **argv);
#endif

/*
 * The gsapi_run_* functions are like gs_main_run_* except
 * that the error_object is omitted.
 * If these functions return <= -100, either quit or a fatal
 * error has occured.  You then call gsapi_exit() next.
 * The only exception is gsapi_run_string_continue()
 * which will return gs_error_NeedInput if all is well.
 */

GSDLLEXPORT int GSDLLAPI
gsapi_run_string_begin(void *instance,
    int user_errors, int *pexit_code);

GSDLLEXPORT int GSDLLAPI
gsapi_run_string_continue(void *instance,
    const char *str, unsigned int length, int user_errors, int *pexit_code);

GSDLLEXPORT int GSDLLAPI
gsapi_run_string_end(void *instance,
    int user_errors, int *pexit_code);

GSDLLEXPORT int GSDLLAPI
gsapi_run_string_with_length(void *instance,
    const char *str, unsigned int length, int user_errors, int *pexit_code);

GSDLLEXPORT int GSDLLAPI
gsapi_run_string(void *instance,
    const char *str, int user_errors, int *pexit_code);

GSDLLEXPORT int GSDLLAPI
gsapi_run_file(void *instance,
    const char *file_name, int user_errors, int *pexit_code);

#ifdef __WIN32__
GSDLLEXPORT int GSDLLAPI
gsapi_run_fileA(void *instance,
    const char *file_name, int user_errors, int *pexit_code);

GSDLLEXPORT int GSDLLAPI
gsapi_run_fileW(void *instance,
    const wchar_t *file_name, int user_errors, int *pexit_code);
#endif

/* Exit the interpreter.
 * This must be called on shutdown if gsapi_init_with_args()
 * has been called, and just before gsapi_delete_instance().
 */
GSDLLEXPORT int GSDLLAPI
gsapi_exit(void *instance);

typedef enum {
    gs_spt_invalid = -1,
    gs_spt_null    = 0,   /* void * is NULL */
    gs_spt_bool    = 1,   /* void * is a pointer to an int (0 false,
                           * non-zero true). */
    gs_spt_int     = 2,   /* void * is a pointer to an int */
    gs_spt_float   = 3,   /* void * is a float * */
    gs_spt_name    = 4,   /* void * is a char * */
    gs_spt_string  = 5,   /* void * is a char * */
    gs_spt_long    = 6,   /* void * is a long * */
    gs_spt_i64     = 7,   /* void * is an int64_t * */
    gs_spt_size_t  = 8,   /* void * is a size_t * */
    gs_spt_parsed  = 9,   /* void * is a pointer to a char * to be parsed */

    /* Setting a typed param causes it to be instantly fed to to the
     * device. This can cause the device to reinitialise itself. Hence,
     * setting a sequence of typed params can cause the device to reset
     * itself several times. Accordingly, if you OR the type with
     * gs_spt_more_to_come, the param will held ready to be passed into
     * the device, and will only actually be sent when the next typed
     * param is set without this flag (or on device init). Not valid
     * for get_typed_param. */
    gs_spt_more_to_come = 1<<31
} gs_set_param_type;
/* gs_spt_parsed allows for a string such as "<< /Foo 0 /Bar true >>" or
 * "[ 1 2 3 ]" etc to be used so more complex parameters can be set. */

GSDLLEXPORT int GSDLLAPI gsapi_set_param(void *instance, const char *param, const void *value, gs_set_param_type type);

/* Called to get a value. value points to storage of the appropriate
 * type. If value is passed as NULL on entry, then the return code is
 * the number of bytes storage required for the type. Thus to read a
 * name/string/parsed value, call once with value=NULL, then obtain
 * the storage, and call again with value=the storage to get a nul
 * terminated string. (nul terminator is included in the count - hence
 * an empty string requires 1 byte storage). Returns gs_error_undefined
 * (-21) if not found. */
GSDLLEXPORT int GSDLLAPI gsapi_get_param(void *instance, const char *param, void *value, gs_set_param_type type);

/* Enumerator to list all the parameters.
 * Caller defines void *iter = NULL, and calls with &iter.
 * Each call, iter is updated to reflect the position within the
 * enumeration, so passing iterator back in gets the next key. The call
 * returns negative values for errors, 0 for success, and 1 for "no more
 * keys".
 *
 *  void *iter = NULL;
 *  gs_set_param_type type;
 *  const char *key;
 *  int code;
 *  while ((code = gsapi_enumerate_params(inst, &iter, &key, &type)) == 0) {
 *      // Process key
 *  }
 *
 * Note that the ordering of enumerations is NOT defined. key is valid
 * until the next call to gsapi_enumerate_params. Only one enumeration
 * at a time (starting a new enumeration will invalidate any previous
 * enumeration).
 */
GSDLLEXPORT int GSDLLAPI gsapi_enumerate_params(void *instance, void **iterator, const char **key, gs_set_param_type *type);

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
#ifndef gp_file_name_sizeof
#define gp_file_name_sizeof 4096
#endif

typedef struct
{
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

/* function prototypes */
typedef int (GSDLLAPIPTR PFN_gsapi_revision)(
    gsapi_revision_t *pr, int len);
typedef int (GSDLLAPIPTR PFN_gsapi_new_instance)(
    void **pinstance, void *caller_handle);
typedef void (GSDLLAPIPTR PFN_gsapi_delete_instance)(
    void *instance);
typedef int (GSDLLAPIPTR PFN_gsapi_set_stdio)(void *instance,
    int (GSDLLCALLPTR stdin_fn)(void *caller_handle, char *buf, int len),
    int (GSDLLCALLPTR stdout_fn)(void *caller_handle, const char *str, int len),
    int (GSDLLCALLPTR stderr_fn)(void *caller_handle, const char *str, int len));
typedef int (GSDLLAPIPTR PFN_gsapi_set_poll)(void *instance,
    int(GSDLLCALLPTR poll_fn)(void *caller_handle));
typedef int (GSDLLAPIPTR PFN_gsapi_set_display_callback)(
    void *instance, display_callback *callback);
typedef int (GSDLLAPIPTR PFN_gsapi_set_default_device_list)(
    void *instance, char *list, int listlen);
typedef int (GSDLLAPIPTR PFN_gsapi_get_default_device_list)(
    void *instance, char **list, int *listlen);
typedef int (GSDLLAPIPTR PFN_gsapi_init_with_args)(
    void *instance, int argc, char **argv);
#ifdef __WIN32__
typedef int (GSDLLAPIPTR PFN_gsapi_init_with_argsA)(
    void *instance, int argc, char **argv);
typedef int (GSDLLAPIPTR PFN_gsapi_init_with_argsW)(
    void *instance, int argc, wchar_t **argv);
#endif
typedef int (GSDLLAPIPTR PFN_gsapi_set_arg_encoding)(
    void *instance, int encoding);
typedef int (GSDLLAPIPTR PFN_gsapi_run_string_begin)(
    void *instance, int user_errors, int *pexit_code);
typedef int (GSDLLAPIPTR PFN_gsapi_run_string_continue)(
    void *instance, const char *str, unsigned int length,
    int user_errors, int *pexit_code);
typedef int (GSDLLAPIPTR PFN_gsapi_run_string_end)(
    void *instance, int user_errors, int *pexit_code);
typedef int (GSDLLAPIPTR PFN_gsapi_run_string_with_length)(
    void *instance, const char *str, unsigned int length,
    int user_errors, int *pexit_code);
typedef int (GSDLLAPIPTR PFN_gsapi_run_string)(
    void *instance, const char *str,
    int user_errors, int *pexit_code);
typedef int (GSDLLAPIPTR PFN_gsapi_run_file)(void *instance,
    const char *file_name, int user_errors, int *pexit_code);
#ifdef __WIN32__
typedef int (GSDLLAPIPTR PFN_gsapi_run_fileA)(void *instance,
    const char *file_name, int user_errors, int *pexit_code);
typedef int (GSDLLAPIPTR PFN_gsapi_run_fileW)(void *instance,
    const wchar_t *file_name, int user_errors, int *pexit_code);
#endif
typedef int (GSDLLAPIPTR PFN_gsapi_exit)(void *instance);
typedef int (GSDLLAPIPTR PFN_gsapi_set_param)(void *instance, const char *param, const void *value, gs_set_param_type type);

typedef int (GSDLLAPIPTR PFN_gsapi_add_control_path)(void *instance, int type, const char *path);
typedef int (GSDLLAPIPTR PFN_gsapi_remove_control_path)(void *instance, int type, const char *path);
typedef void (GSDLLAPIPTR PFN_gsapi_purge_control_paths)(void *instance, int type);
typedef void (GSDLLAPIPTR PFN_gsapi_activate_path_control)(void *instance, int enable);
typedef int (GSDLLAPIPTR PFN_gsapi_is_path_control_active)(void *instance);
typedef int (GSDLLAPIPTR PFN_gsapi_add_fs)(void *instance, gsapi_fs_t *fs, void *secret);
typedef void (GSDLLAPIPTR PFN_gsapi_remove_fs)(void *instance, gsapi_fs_t *fs, void *secret);

#ifdef __MACOS__
#pragma export off
#endif

#ifdef __cplusplus
} /* extern 'C' protection */
#endif

#endif /* iapi_INCLUDED */
