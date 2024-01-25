/* Copyright (C) 2001-2024 Artifex Software, Inc.
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


/* Command line parsing and dispatching */

#include "ctype_.h"
#include "memory_.h"
#include "string_.h"
#include <stdlib.h>     /* for qsort */

#include "ghost.h"
#include "gp.h"
#include "gsargs.h"
#include "gscdefs.h"
#include "gsmalloc.h"           /* for gs_malloc_limit */
#include "gsmdebug.h"
#include "gspaint.h"		/* for gs_erasepage */
#include "gxdevice.h"
#include "gxdevmem.h"
#include "gsdevice.h"
#include "gxdevsop.h"		/* for gxdso_* enums */
#include "gxclpage.h"
#include "gdevprn.h"
#include "stream.h"
#include "ierrors.h"
#include "estack.h"
#include "ialloc.h"
#include "strimpl.h"            /* for sfilter.h */
#include "sfilter.h"            /* for iscan.h */
#include "ostack.h"             /* must precede iscan.h */
#include "iscan.h"
#include "iconf.h"
#include "imain.h"
#include "imainarg.h"
#include "iapi.h"
#include "iminst.h"
#include "iname.h"
#include "store.h"
#include "files.h"              /* requires stream.h */
#include "interp.h"
#include "iutil.h"
#include "ivmspace.h"

/* Import operator procedures */
extern int zflush(i_ctx_t *);
extern int zflushpage(i_ctx_t *);

#ifndef GS_LIB
#  define GS_LIB "GS_LIB"
#endif

#ifndef GS_OPTIONS
#  define GS_OPTIONS "GS_OPTIONS"
#endif

/* This is now the default number of entries we initially
 * allocate for the search path array objects. The arrays
 * will now enlarge as required -
 * see imain.c: file_path_add()/extend_path_list_container()
 */

#ifndef GS_MAX_LIB_DIRS
#  define GS_MAX_LIB_DIRS 25
#endif

#define MAX_BUFFERED_SIZE 1024

/* Note: sscanf incorrectly defines its first argument as char * */
/* rather than const char *.  This accounts for the ugly casts below. */

/* Redefine puts to use outprintf, */
/* so it will work even without stdio. */
#undef puts
#define puts(mem, str) outprintf(mem, "%s\n", str)

/* Forward references */
#define runInit 1
#define runFlush 2
#define runBuffer 4
static int swproc(gs_main_instance *, const char *, arg_list *);
static int argproc(gs_main_instance *, const char *);
static int run_buffered(gs_main_instance *, const char *);
static int esc_strlen(const char *);
static void esc_strcat(char *, const char *);
static int runarg(gs_main_instance *, const char *, const char *, const char *, int, int, int *, ref *);
static int run_string(gs_main_instance *, const char *, int, int, int *, ref *);
static int run_finish(gs_main_instance *, int, int, ref *);
static int try_stdout_redirect(gs_main_instance * minst,
                                const char *command, const char *filename);

/* Forward references for help printout */
static void print_help(gs_main_instance *);
static void print_revision(const gs_main_instance *);
static void print_version(const gs_main_instance *);
static void print_usage(const gs_main_instance *);
static void print_devices(const gs_main_instance *);
static void print_emulators(const gs_main_instance *);
static void print_paths(gs_main_instance *);
static void print_help_trailer(const gs_main_instance *);

/* ------ Main program ------ */

/* Process the command line with a given instance. */
static stream *
gs_main_arg_sopen(const char *fname, void *vminst)
{
    gs_main_set_lib_paths((gs_main_instance *) vminst);
    return lib_sopen(&((gs_main_instance *)vminst)->lib_path,
                     ((gs_main_instance *)vminst)->heap, fname);
}
static void
set_debug_flags(const char *arg, char *flags)
{
    byte value = (*arg == '-' ? (++arg, 0) : 0xff);

    while (*arg)
        flags[*arg++ & 127] = value;
}

int
gs_main_init_with_args01(gs_main_instance * minst, int argc, char *argv[])
{
    const char *arg;
    arg_list args;
    int code;
    int have_dumped_args = 0;

    /* Now we actually process them */
    code = arg_init(&args, (const char **)argv, argc,
                    gs_main_arg_sopen, (void *)minst,
                    minst->get_codepoint,
                    minst->heap);
    if (code < 0)
        return code;
    code = gs_main_init0(minst, 0, 0, 0, GS_MAX_LIB_DIRS);
    if (code < 0)
        return code;
/* This first check is not needed on VMS since GS_LIB evaluates to the same
   value as that returned by gs_lib_default_path.  Also, since GS_LIB is
   defined as a searchlist logical and getenv only returns the first entry
   in the searchlist, it really doesn't make sense to search that particular
   directory twice.
*/
#ifndef __VMS
    {
        int len = 0;
        int code = gp_getenv(GS_LIB, (char *)0, &len);

        if (code < 0) {         /* key present, value doesn't fit */
            char *path = (char *)gs_alloc_bytes(minst->heap, len, "GS_LIB");

            gp_getenv(GS_LIB, path, &len);      /* can't fail */
            minst->lib_path.env = path;
        }
    }
#endif /* __VMS */
    minst->lib_path.final = gs_lib_default_path;
    code = gs_main_set_lib_paths(minst);
    if (code < 0)
        return code;
    /* Prescan the command line for --help and --version. */
    {
        int i;
        bool helping = false;

        for (i = 1; i < argc; ++i)
            if (!arg_strcmp(&args, argv[i], "--")) {
                /* A PostScript program will be interpreting all the */
                /* remaining switches, so stop scanning. */
                helping = false;
                break;
            } else if (!arg_strcmp(&args, argv[i], "--help")) {
                print_help(minst);
                helping = true;
            } else if (!arg_strcmp(&args, argv[i], "--debug")) {
                gs_debug_flags_list(minst->heap);
                helping = true;
            } else if (!arg_strcmp(&args, argv[i], "--version")) {
                print_version(minst);
                puts(minst->heap, "");  /* \n */
                helping = true;
            }
        if (helping)
            return gs_error_Info;
    }
    /* Execute files named in the command line, */
    /* processing options along the way. */
    /* Wait until the first file name (or the end */
    /* of the line) to finish initialization. */
    minst->run_start = true;

    {
        int len = 0;
        int code = gp_getenv(GS_OPTIONS, (char *)0, &len);

        if (code < 0) {         /* key present, value doesn't fit */
            char *opts =
            (char *)gs_alloc_bytes(minst->heap, len, "GS_OPTIONS");

            gp_getenv(GS_OPTIONS, opts, &len);  /* can't fail */
            if (arg_push_decoded_memory_string(&args, opts, false, true, minst->heap))
                return gs_error_Fatal;
        }
    }
    while ((code = arg_next(&args, (const char **)&arg, minst->heap)) > 0) {
        code = gs_lib_ctx_stash_sanitized_arg(minst->heap->gs_lib_ctx, arg);
        if (code < 0)
            return code;
        switch (*arg) {
            case '-':
                code = swproc(minst, arg, &args);
                if (code < 0)
                    return code;
                if (code > 0)
                    outprintf(minst->heap, "Unknown switch %s - ignoring\n", arg);
                if (gs_debug[':'] && !have_dumped_args) {
                    int i;

                    if (gs_debug_c(gs_debug_flag_init_details))
                        dmprintf1(minst->heap, "%% Args passed to instance "PRI_INTPTR": ",
                              (intptr_t)minst);
                    for (i=1; i<argc; i++)
                        dmprintf1(minst->heap, "%s ", argv[i]);
                    dmprintf(minst->heap, "\n");
                    have_dumped_args = 1;
                }
                break;
            default:
                /* default is to treat this as a file name to be run */
                code = argproc(minst, arg);
                if (code < 0)
                    return code;
                if (minst->saved_pages_test_mode) {
                    gx_device *pdev;
                    int ret;
                    gxdso_device_child_request child_dev_data;

                    /* get the real target (printer) device */
                    pdev = gs_currentdevice(minst->i_ctx_p->pgs);
                    do {
                        child_dev_data.target = pdev;
                        ret = dev_proc(pdev, dev_spec_op)(pdev, gxdso_device_child, &child_dev_data,
                                                          sizeof(child_dev_data));
                        if (ret > 0)
                            pdev = child_dev_data.target;
                    } while ((ret > 0) && (child_dev_data.n != 0));
                    if ((code = gx_saved_pages_param_process((gx_device_printer *)pdev,
                               (byte *)"print normal flush", 18)) < 0)
                        return code;
                    if (code > 0)
                        if ((code = gs_erasepage(minst->i_ctx_p->pgs)) < 0)
                            return code;
                }
        }
    }

    return code;
}

int
gs_main_init_with_args2(gs_main_instance * minst)
{
    int code;

    code = gs_main_init2(minst);
    if (code < 0)
        return code;

    if (!minst->run_start)
        return gs_error_Quit;
    return code;
}

int
gs_main_init_with_args(gs_main_instance * minst, int argc, char *argv[])
{
    int code = gs_main_init_with_args01(minst, argc, argv);

    if (code < 0)
        return code;
    return gs_main_init_with_args2(minst);
}

/*
 * Specify a decoding function
 */
void gs_main_inst_arg_decode(gs_main_instance * minst,
                             gs_arg_get_codepoint *get_codepoint)
{
    minst->get_codepoint = get_codepoint;
}

gs_arg_get_codepoint *gs_main_inst_get_arg_decode(gs_main_instance * minst)
{
    return minst->get_codepoint;
}

/*
 * Run the 'start' procedure (after processing the command line).
 */
int
gs_main_run_start(gs_main_instance * minst)
{
    return run_string(minst, "systemdict /start get exec", runFlush, minst->user_errors, NULL, NULL);
}

static int
do_arg_match(const char **arg, const char *match, size_t match_len)
{
    const char *s = *arg;
    if (strncmp(s, match, match_len) != 0)
        return 0;
    s += match_len;
    if (*s == '=')
        *arg = ++s;
    else if (*s != 0)
        return 0;
    else
        *arg = NULL;
    return 1;
}

#define arg_match(A, B) do_arg_match(A, B, sizeof(B)-1)

/* Process switches.  Return 0 if processed, 1 for unknown switch, */
/* <0 if error. */
static int
swproc(gs_main_instance * minst, const char *arg, arg_list * pal)
{
    char sw = arg[1];
    ref vtrue;
    int code = 0;

    make_true(&vtrue);
    arg += 2;                   /* skip - and letter */
    switch (sw) {
        default:
            return 1;
        case 0:         /* read stdin as a file char-by-char */
            /* This is a ******HACK****** for Ghostview. */
            minst->heap->gs_lib_ctx->core->stdin_is_interactive = true;
            goto run_stdin;
        case '_':       /* read stdin with normal buffering */
            minst->heap->gs_lib_ctx->core->stdin_is_interactive = false;
run_stdin:
            minst->run_start = false;   /* don't run 'start' */
            /* Set NOPAUSE so showpage won't try to read from stdin. */
            code = swproc(minst, "-dNOPAUSE", pal);
            if (code)
                return code;
            code = gs_main_init2(minst);        /* Finish initialization */
            if (code < 0)
                return code;

            code = run_string(minst, ".runstdin", runFlush, minst->user_errors, NULL, NULL);
            if (code < 0)
                return code;
            /* If in saved_pages_test_mode, print and flush previous job before the next file */
            if (minst->saved_pages_test_mode) {
                gx_device *pdev;
                int ret;
                gxdso_device_child_request child_dev_data;

                /* get the real target (printer) device */
                pdev = gs_currentdevice(minst->i_ctx_p->pgs);
                do {
                    child_dev_data.target = pdev;
                    ret = dev_proc(pdev, dev_spec_op)(pdev, gxdso_device_child, &child_dev_data,
                                                      sizeof(child_dev_data));
                    if (ret > 0)
                        pdev = child_dev_data.target;
                } while ((ret > 0) && (child_dev_data.n != 0));
                if ((code = gx_saved_pages_param_process((gx_device_printer *)pdev,
                           (byte *)"print normal flush", 18)) < 0)
                    return code;
                if (code > 0)
                    if ((code = gs_erasepage(minst->i_ctx_p->pgs)) < 0)
                        return code;
            }
            break;
        case '-':               /* run with command line args */
            if (strncmp(arg, "debug=", 6) == 0) {
                code = gs_debug_flags_parse(minst->heap, arg+6);
                if (code < 0)
                    return code;
                break;
            } else if (strncmp(arg, "saved-pages=", 12) == 0) {
                gx_device *pdev;

                /* If init2 not yet done, just save the argument for processing then */
                if (minst->init_done < 2) {
                    if (minst->saved_pages_initial_arg == NULL) {
                        /* Tuck the parameters away for later when init2 is done (usually "begin") */
                        minst->saved_pages_initial_arg = (char *)gs_alloc_bytes(minst->heap,
                                                                                1+strlen((char *)arg+12),
                                                                               "GS_OPTIONS");
                        if (minst->saved_pages_initial_arg != NULL) {
                            strcpy(minst->saved_pages_initial_arg,(char *)arg+12);
                        } else {
                            outprintf(minst->heap,
                                      "   saved_pages_initial_arg larger than expected\n");
                            arg_finit(pal);
                            return gs_error_Fatal;
                        }
                    } else {
                        outprintf(minst->heap,
                                  "   Only one --saved-pages=... command allowed before processing input\n");
                        arg_finit(pal);
                        return gs_error_Fatal;
                    }
                } else {
                    int ret;
                    gxdso_device_child_request child_dev_data;

                    /* get the current device */
                    pdev = gs_currentdevice(minst->i_ctx_p->pgs);
                    if (dev_proc(pdev, dev_spec_op)(pdev, gxdso_supports_saved_pages, NULL, 0) <= 0) {
                        outprintf(minst->heap,
                                  "   --saved-pages not supported by the '%s' device.\n",
                                  pdev->dname);
                        arg_finit(pal);
                        return gs_error_Fatal;
                    }
                    /* get the real target (printer) device */
                    do {
                        child_dev_data.target = pdev;
                        ret = dev_proc(pdev, dev_spec_op)(pdev, gxdso_device_child, &child_dev_data,
                                                          sizeof(child_dev_data));
                        if (ret > 0)
                            pdev = child_dev_data.target;
                    } while ((ret > 0) && (child_dev_data.n != 0));
                    if ((code = gx_saved_pages_param_process((gx_device_printer *)pdev,
                                                              (byte *)arg+12, strlen(arg+12))) < 0)
                        return code;
                    if (code > 0)
                        if ((code = gs_erasepage(minst->i_ctx_p->pgs)) < 0)
                            return code;
                }
                break;
            /* The following code is only to allow regression testing of saved-pages */
            } else if (strncmp(arg, "saved-pages-test", 16) == 0) {
                minst->saved_pages_test_mode = true;
                break;
            /* Now handle the explicitly added paths to the file control lists */
            } else if (arg_match(&arg, "permit-file-read")) {
                code = gs_add_explicit_control_path(minst->heap, arg, gs_permit_file_reading);
                if (code < 0) return code;
                break;
            } else if (arg_match(&arg, "permit-file-write")) {
                code = gs_add_explicit_control_path(minst->heap, arg, gs_permit_file_writing);
                if (code < 0) return code;
                break;
            } else if (arg_match(&arg, "permit-file-control")) {
                code = gs_add_explicit_control_path(minst->heap, arg, gs_permit_file_control);
                if (code < 0) return code;
                break;
            } else if (arg_match(&arg, "permit-file-all")) {
                code = gs_add_explicit_control_path(minst->heap, arg, gs_permit_file_reading);
                if (code < 0) return code;
                code = gs_add_explicit_control_path(minst->heap, arg, gs_permit_file_writing);
                if (code < 0) return code;
                code = gs_add_explicit_control_path(minst->heap, arg, gs_permit_file_control);
                if (code < 0) return code;
                break;
            }
            if (*arg != 0) {
                /* Unmatched switch. */
                outprintf(minst->heap,
                          "   Unknown switch '--%s'.\n",
                          arg);
                arg_finit(pal);
                return gs_error_Fatal;
            }
            /* FALLTHROUGH */
        case '+':
            pal->expand_ats = false;
            /* FALLTHROUGH */
        case '@':               /* ditto with @-expansion */
            {
                char *psarg;

                code = arg_next(pal, (const char **)&psarg, minst->heap);
                /* Don't stash the @ file name */

                if (code < 0)
                    return gs_error_Fatal;
                if (psarg == NULL) {
                    outprintf(minst->heap, "Usage: gs ... -%c file.ps arg1 ... argn\n", sw);
                    arg_finit(pal);
                    return gs_error_Fatal;
                }
                psarg = arg_copy(psarg, minst->heap);
                if (psarg == NULL)
                    code = gs_error_Fatal;
                else
                    code = gs_main_init2(minst);
                if (code >= 0)
                    code = run_string(minst, "userdict/ARGUMENTS[", 0, minst->user_errors, NULL, NULL);
                if (code >= 0)
                    while ((code = arg_next(pal, (const char **)&arg, minst->heap)) > 0) {
                        code = gs_lib_ctx_stash_sanitized_arg(minst->heap->gs_lib_ctx, arg);
                        if (code < 0)
                            break;
                        code = runarg(minst, "", arg, "", runInit, minst->user_errors, NULL, NULL);
                        if (code < 0)
                            break;
                    }
                if (code >= 0)
                    code = run_string(minst, "]put", 0, minst->user_errors, NULL, NULL);
                if (code >= 0)
                    code = argproc(minst, psarg);
                arg_free((char *)psarg, minst->heap);
                if (code >= 0)
                    code = gs_error_Quit;

                return code;
            }
        case 'B':               /* set run_string buffer size */
            if (*arg == '-')
                minst->run_buffer_size = 0;
            else {
                uint bsize;

                if (sscanf((const char *)arg, "%u", &bsize) != 1 ||
                    bsize <= 0 || bsize > MAX_BUFFERED_SIZE
                    ) {
                    outprintf(minst->heap,
                              "-B must be followed by - or size between 1 and %u\n",
                              MAX_BUFFERED_SIZE);
                    return gs_error_Fatal;
                }
                minst->run_buffer_size = bsize;
            }
            break;
        case 'c':               /* code follows */
            {
                bool ats = pal->expand_ats;

                code = gs_main_init2(minst);
                if (code < 0)
                    return code;
                pal->expand_ats = false;
                while ((code = arg_next(pal, (const char **)&arg, minst->heap)) > 0) {
                    if (arg[0] == '@' ||
                        (arg[0] == '-' && !isdigit((unsigned char)arg[1]))
                        )
                        break;
                    code = gs_lib_ctx_stash_sanitized_arg(minst->heap->gs_lib_ctx, "?");
                    if (code < 0)
                        return code;
                    code = runarg(minst, "", arg, ".runstring", 0, minst->user_errors, NULL, NULL);
                    if (code < 0)
                        return code;
                }
                if (code < 0)
                    return gs_error_Fatal;
                if (arg != NULL) {
                    char *p = arg_copy(arg, minst->heap);
                    if (p == NULL)
                        return gs_error_Fatal;
                    arg_push_string(pal, p, true);
                }
                pal->expand_ats = ats;
                break;
            }
        case 'f':               /* run file of arbitrary name */
            if (*arg != 0) {
                code = argproc(minst, arg);
                if (code < 0)
                    return code;
                /* If in saved_pages_test_mode, print and flush previous job before the next file */
                if (minst->saved_pages_test_mode) {
                    gx_device *pdev;
                    int ret;
                    gxdso_device_child_request child_dev_data;

                    /* get the real target (printer) device */
                    pdev = gs_currentdevice(minst->i_ctx_p->pgs);
                    do {
                        child_dev_data.target = pdev;
                        ret = dev_proc(pdev, dev_spec_op)(pdev, gxdso_device_child, &child_dev_data,
                                                          sizeof(child_dev_data));
                        if (ret > 0)
                            pdev = child_dev_data.target;
                    } while ((ret > 0) && (child_dev_data.n != 0));
                    if ((code = gx_saved_pages_param_process((gx_device_printer *)pdev,
                               (byte *)"print normal flush", 18)) < 0)
                        return code;
                    if (code > 0)
                        if ((code = gs_erasepage(minst->i_ctx_p->pgs)) < 0)
                            return code;
                }
            }
            break;
        case 'F':               /* run file with buffer_size = 1 */
            if (!*arg) {
                puts(minst->heap, "-F requires a file name");
                return gs_error_Fatal;
            } {
                uint bsize = minst->run_buffer_size;

                minst->run_buffer_size = 1;
                code = argproc(minst, arg);
                minst->run_buffer_size = bsize;
                if (code < 0)
                    return code;
                /* If in saved_pages_test_mode, print and flush previous job before the next file */
                if (minst->saved_pages_test_mode) {
                    gx_device *pdev;
                    int ret;
                    gxdso_device_child_request child_dev_data;

                    /* get the real target (printer) device */
                    pdev = gs_currentdevice(minst->i_ctx_p->pgs);
                    do {
                        child_dev_data.target = pdev;
                        ret = dev_proc(pdev, dev_spec_op)(pdev, gxdso_device_child, &child_dev_data,
                                                          sizeof(child_dev_data));
                        if (ret > 0)
                            pdev = child_dev_data.target;
                    } while ((ret > 0) && (child_dev_data.n != 0));
                    if ((code = gx_saved_pages_param_process((gx_device_printer *)pdev,
                               (byte *)"print normal flush", 18)) < 0)
                        return code;
                    if (code > 0)
                        if ((code = gs_erasepage(minst->i_ctx_p->pgs)) < 0)
                            return code;
                }
            }
            break;
        case 'g':               /* define device geometry */
            {
                long dimensions[2];

                if ((code = gs_main_init1(minst)) < 0)
                    return code;
                if (sscanf((const char *)arg, "%ldx%ld", &dimensions[0], &dimensions[1]) != 2) {
                    puts(minst->heap, "-g must be followed by <width>x<height>");
                    return gs_error_Fatal;
                }
                gs_main_force_dimensions(minst, dimensions);
                break;
            }
        case 'h':               /* print help */
        case '?':               /* ditto */
            print_help(minst);
            return gs_error_Info;      /* show usage info on exit */
        case 'I':               /* specify search path */
            {
                const char *path;

                if (arg[0] == 0) {
                    code = arg_next(pal, (const char **)&path, minst->heap);
                    if (code < 0)
                        return code;
                    code = gs_lib_ctx_stash_sanitized_arg(minst->heap->gs_lib_ctx, "?");
                    if (code < 0)
                        return code;
                } else
                    path = arg;
                if (path == NULL)
                    return gs_error_Fatal;
                gs_main_add_lib_path(minst, path);
            }
            break;
        case 'K':               /* set malloc limit */
            {
                long msize = 0;
                gs_malloc_memory_t *rawheap = gs_malloc_wrapped_contents(minst->heap);

                (void)sscanf((const char *)arg, "%ld", &msize);
                if (msize <= 0 || msize > max_long >> 10) {
                    outprintf(minst->heap, "-K<numK> must have 1 <= numK <= %ld\n",
                              max_long >> 10);
                    return gs_error_Fatal;
                }
                rawheap->limit = msize << 10;
            }
            break;
        case 'M':               /* set memory allocation increment */
            {
                unsigned msize = 0;

                (void)sscanf((const char *)arg, "%u", &msize);
                if (msize <= 0 || msize >= (max_uint >> 10)) {
                    outprintf(minst->heap, "-M must be between 1 and %d\n", (int)((max_uint >> 10) - 1));
                    return gs_error_Fatal;
                }
                minst->memory_clump_size = msize << 10;
            }
            break;
        case 'N':               /* set size of name table */
            {
                unsigned nsize = 0;

                (void)sscanf((const char *)arg, "%d", &nsize);
                if (nsize < 2 || nsize > (max_uint >> 10)) {
                    outprintf(minst->heap, "-N must be between 2 and %d\n", (int)(max_uint >> 10));
                    return gs_error_Fatal;
                }
                minst->name_table_size = (ulong) nsize << 10;
            }
            break;
        case 'o':               /* set output file name and batch mode */
            {
                i_ctx_t *i_ctx_p;
                uint space;
                const char *adef;
                byte *str;
                ref value;
                int len;

                if ((code = gs_main_init1(minst)) < 0)
                    return code;

                i_ctx_p = minst->i_ctx_p;
                space = icurrent_space;
                if (arg[0] == 0) {
                    code = arg_next(pal, (const char **)&adef, minst->heap);
                    if (code < 0)
                        return code;
                    if (code == 0)
                        return gs_error_undefinedfilename;
                    code = gs_lib_ctx_stash_sanitized_arg(minst->heap->gs_lib_ctx, "?");
                    if (code < 0)
                        return code;
                } else
                    adef = arg;
                if ((code = gs_main_init1(minst)) < 0)
                    return code;

                if (strlen(adef) > 0) {
                    code = gs_add_outputfile_control_path(minst->heap, adef);
                    if (code < 0) return code;
                }

                ialloc_set_space(idmemory, avm_system);
                len = strlen(adef);
                str = ialloc_string(len, "-o");
                if (str == NULL)
                    return gs_error_VMerror;
                memcpy(str, adef, len);
                make_const_string(&value, a_readonly | avm_system, len, str);
                ialloc_set_space(idmemory, space);
                i_initial_enter_name(minst->i_ctx_p, "OutputFile", &value);
                i_initial_enter_name(minst->i_ctx_p, "NOPAUSE", &vtrue);
                i_initial_enter_name(minst->i_ctx_p, "BATCH", &vtrue);
            }
            break;
        case 'P':               /* choose whether search '.' first */
            if (!strcmp(arg, ""))
                minst->search_here_first = true;
            else if (!strcmp(arg, "-"))
                minst->search_here_first = false;
            else {
                puts(minst->heap, "Only -P or -P- is allowed.");
                return gs_error_Fatal;
            }
            break;
        case 'q':               /* quiet startup */
            if ((code = gs_main_init1(minst)) < 0)
                return code;
            i_initial_enter_name(minst->i_ctx_p, "QUIET", &vtrue);
            break;
        case 'r':               /* define device resolution */
            {
                float res[2];

                if ((code = gs_main_init1(minst)) < 0)
                    return code;
                switch (sscanf((const char *)arg, "%fx%f", &res[0], &res[1])) {
                    default:
                        puts(minst->heap, "-r must be followed by <res> or <xres>x<yres>");
                        return gs_error_Fatal;
                    case 1:     /* -r<res> */
                        res[1] = res[0];
                        /* fall through */
                    case 2:     /* -r<xres>x<yres> */
                        gs_main_force_resolutions(minst, res);
                }
                break;
            }
        case 'D':               /* define name */
        case 'd':
        case 'S':               /* define name as string */
        case 's':
            {
                char *adef = arg_copy(arg, minst->heap);
                char *eqp;
                bool isd = (sw == 'D' || sw == 'd');
                ref value;

                if (adef == NULL)
                    return gs_error_Fatal;
                eqp = strchr(adef, '=');

                if (eqp == NULL)
                    eqp = strchr(adef, '#');
                /* Initialize the object memory, scanner, and */
                /* name table now if needed. */
                if ((code = gs_main_init1(minst)) < 0) {
                    arg_free((char *)adef, minst->heap);
                    return code;
                }
                if (eqp == adef) {
                    puts(minst->heap, "Usage: -dNAME, -dNAME=TOKEN, -sNAME=STRING");
                    arg_free((char *)adef, minst->heap);
                    return gs_error_Fatal;
                }
                if (eqp == NULL) {
                    if (isd)
                        make_true(&value);
                    else
                        make_empty_string(&value, a_readonly);
                } else {
                    int code;
                    i_ctx_t *i_ctx_p = minst->i_ctx_p;
                    uint space = icurrent_space;

                    *eqp++ = 0;

                    if (strlen(adef) == 10 && strncmp(adef, "OutputFile", 10) == 0 && strlen(eqp) > 0) {
                        code = gs_add_outputfile_control_path(minst->heap, eqp);
                        if (code < 0) {
                            arg_free((char *)adef, minst->heap);
                            return code;
                        }
                    }

                    ialloc_set_space(idmemory, avm_system);
                    if (isd) {
                        int i;
                        int64_t num;

                        /* Check for numbers so we can provide for suffix scalers */
                        /* Note the check for '#' is for PS "radix" numbers such as 16#ff */
                        /* and check for '.' and 'e' or 'E' which are 'real' numbers */
                        if ((strchr(eqp, '#') == NULL) && (strchr(eqp, '.') == NULL) &&
                            (strchr(eqp, 'e') == NULL) && (strchr(eqp, 'E') == NULL) &&
                            ((i = sscanf((const char *)eqp, "%"PRIi64, &num)) == 1)) {
                            char suffix = eqp[strlen(eqp) - 1];

                            switch (suffix) {
                                case 'k':
                                case 'K':
                                    num *= 1024;
                                    break;
                                case 'm':
                                case 'M':
                                    num *= 1024 * 1024;
                                    break;
                                case 'g':
                                case 'G':
                                    /* caveat emptor: more than 2g will overflow */
                                    /* and really should produce a 'real', so don't do this */
                                    num *= 1024 * 1024 * 1024;
                                    break;
                                default:
                                    break;   /* not a valid suffix or last char was digit */
                            }
                            make_int(&value, (ps_int)num);
                        } else {
                            /* use the PS scanner to capture other valid token types */
                            stream astream;
                            scanner_state state;

                            s_init(&astream, NULL);
                            sread_string(&astream,
                                         (const byte *)eqp, strlen(eqp));
                            gs_scanner_init_stream(&state, &astream);
                            code = gs_scan_token(minst->i_ctx_p, &value, &state);
                            if (code) {
                                outprintf(minst->heap, "Invalid value for option -d%s, -dNAME= must be followed by a valid token\n", arg);
                                arg_free((char *)adef, minst->heap);
                                return gs_error_Fatal;
                            }
                            if (r_has_type_attrs(&value, t_name,
                                                 a_executable)) {
                                ref nsref;

                                name_string_ref(minst->heap, &value, &nsref);
#undef string_is
#define string_is(nsref, str, len)\
       (r_size(&(nsref)) == (len) &&\
       !strncmp((const char *)(nsref).value.const_bytes, str, (len)))
                                if (string_is(nsref, "null", 4))
                                    make_null(&value);
                                else if (string_is(nsref, "true", 4))
                                    make_true(&value);
                                else if (string_is(nsref, "false", 5))
                                    make_false(&value);
                                else {
                                    outprintf(minst->heap, "Invalid value for option -d%s, use -sNAME= to define string constants\n", arg);
                                    arg_free((char *)adef, minst->heap);
                                    return gs_error_Fatal;
                                }
                            }
                        }
                    } else {
                        int len = strlen(eqp);
                        byte *body = ialloc_string(len, "-s");

                        if (body == NULL) {
                            lprintf("Out of memory!\n");
                            arg_free((char *)adef, minst->heap);
                            return gs_error_Fatal;
                        }
                        memcpy(body, eqp, len);
                        make_const_string(&value, a_readonly | avm_system, len, body);
                        if ((code = try_stdout_redirect(minst, adef, eqp)) < 0) {
                            arg_free((char *)adef, minst->heap);
                            return code;
                        }
                    }
                    ialloc_set_space(idmemory, space);
                }
                /* Enter the name in systemdict. */
                i_initial_enter_name_copy(minst->i_ctx_p, adef, &value);
                arg_free((char *)adef, minst->heap);
                break;
            }
        case 'p':
            {
                char *adef = arg_copy(arg, minst->heap);
                char *eqp;

                if (adef == NULL)
                    return gs_error_Fatal;
                eqp = strchr(adef, '=');

                if (eqp == NULL)
                    eqp = strchr(adef, '#');
                if (eqp == NULL) {
                    outprintf(minst->heap, "Usage: -pNAME=STRING\n");
                    arg_free((char *)adef, minst->heap);
                    return gs_error_Fatal;
                }
                *eqp++ = 0;
                /* Slightly uncomfortable calling back up to a higher
                 * level, but we'll live with it. */
                code = gsapi_set_param(gs_lib_ctx_get_interp_instance(minst->heap),
                                       adef, eqp, gs_spt_parsed);
                if (code < 0) {
                    arg_free((char *)adef, minst->heap);
                    return code;
                }
                break;
            }
        case 'u':               /* undefine name */
            if (!*arg) {
                puts(minst->heap, "-u requires a name to undefine.");
                return gs_error_Fatal;
            }
            if ((code = gs_main_init1(minst)) < 0)
                return code;
            i_initial_remove_name(minst->i_ctx_p, arg);
            break;
        case 'v':               /* print revision */
            print_revision(minst);
            return gs_error_Info;
/*#ifdef DEBUG */
            /*
             * Here we provide a place for inserting debugging code that can be
             * run in place of the normal interpreter code.
             */
        case 'X':
            code = gs_main_init2(minst);
            if (code < 0)
                return code;
            {
                int xec;        /* exit_code */
                ref xeo;        /* error_object */

#define start_x()\
  gs_main_run_string_begin(minst, 1, &xec, &xeo)
#define run_x(str)\
  gs_main_run_string_continue(minst, str, strlen(str), 1, &xec, &xeo)
#define stop_x()\
  gs_main_run_string_end(minst, 1, &xec, &xeo)
                start_x();
                run_x("\216\003abc");
                run_x("== flush\n");
                stop_x();
            }
            return gs_error_Quit;
/*#endif */
        case 'Z':
            set_debug_flags(arg, gs_debug);
            break;
    }
    return 0;
}

/* Define versions of strlen and strcat that encode strings in hex. */
/* This is so we can enter escaped characters regardless of whether */
/* the Level 1 convention of ignoring \s in strings-within-strings */
/* is being observed (sigh). */
static int
esc_strlen(const char *str)
{
    return strlen(str) * 2 + 2;
}
static void
esc_strcat(char *dest, const char *src)
{
    char *d = dest + strlen(dest);
    const char *p;
    static const char *const hex = "0123456789abcdef";

    *d++ = '<';
    for (p = src; *p; p++) {
        byte c = (byte) * p;

        *d++ = hex[c >> 4];
        *d++ = hex[c & 0xf];
    }
    *d++ = '>';
    *d = 0;
}

/* Process file names */
static int
argproc(gs_main_instance * minst, const char *arg)
{
    int code1, code = gs_main_init1(minst);            /* need i_ctx_p to proceed */

    if (code < 0)
        return code;

    code = gs_add_control_path(minst->heap, gs_permit_file_reading, arg);
    if (code < 0) return code;

    if (minst->run_buffer_size) {
        /* Run file with run_string. */
        code = run_buffered(minst, arg);
    } else {
        /* Run file directly in the normal way. */
        code = runarg(minst, "", arg, ".runfile", runInit | runFlush, minst->user_errors, NULL, NULL);
    }

    code1 = gs_remove_control_path(minst->heap, gs_permit_file_reading, arg);
    if (code >= 0 && code1 < 0) code = code1;

    return code;
}
static int
run_buffered(gs_main_instance * minst, const char *arg)
{
    gp_file *in = gp_fopen(minst->heap, arg, gp_fmode_rb);
    int exit_code;
    ref error_object;
    int code;

    if (in == 0) {
        outprintf(minst->heap, "Unable to open %s for reading", arg);
        return_error(gs_error_invalidfileaccess);
    }
    code = gs_main_init2(minst);
    if (code < 0) {
        gp_fclose(in);
        return code;
    }
    code = gs_main_run_string_begin(minst, minst->user_errors,
                                    &exit_code, &error_object);
    if (!code) {
        char buf[MAX_BUFFERED_SIZE];
        int count;

        code = gs_error_NeedInput;
        while ((count = gp_fread(buf, 1, minst->run_buffer_size, in)) > 0) {
            code = gs_main_run_string_continue(minst, buf, count,
                                               minst->user_errors,
                                               &exit_code, &error_object);
            if (code != gs_error_NeedInput)
                break;
        }
        if (code == gs_error_NeedInput) {
            code = gs_main_run_string_end(minst, minst->user_errors,
                                          &exit_code, &error_object);
        }
    }
    gp_fclose(in);
    zflush(minst->i_ctx_p);
    zflushpage(minst->i_ctx_p);
    return run_finish(minst, code, exit_code, &error_object);
}
static int
runarg(gs_main_instance *minst,
       const char       *pre,
       const char       *arg,
       const char       *post,
       int               options,
       int               user_errors,
       int              *pexit_code,
       ref              *perror_object)
{
    int len = strlen(pre) + esc_strlen(arg) + strlen(post) + 1;
    int code;
    char *line;

    if (options & runInit) {
        code = gs_main_init2(minst);    /* Finish initialization */

        if (code < 0)
            return code;
    }
    line = (char *)gs_alloc_bytes(minst->heap, len, "runarg");
    if (line == 0) {
        lprintf("Out of memory!\n");
        return_error(gs_error_VMerror);
    }
    strcpy(line, pre);
    esc_strcat(line, arg);
    strcat(line, post);
    minst->i_ctx_p->starting_arg_file = true;
    code = run_string(minst, line, options, user_errors, pexit_code, perror_object);
    minst->i_ctx_p->starting_arg_file = false;
    gs_free_object(minst->heap, line, "runarg");
    return code;
}
int
gs_main_run_file2(gs_main_instance *minst,
                  const char       *filename,
                  int               user_errors,
                  int              *pexit_code,
                  ref              *perror_object)
{
    int code, code1;

    code = gs_add_control_path(minst->heap, gs_permit_file_reading, filename);
    if (code < 0) return code;

    code = runarg(minst, "", filename, ".runfile", runFlush, user_errors, pexit_code, perror_object);

    code1 = gs_remove_control_path(minst->heap, gs_permit_file_reading, filename);
    if (code >= 0 && code1 < 0) code = code1;

    return code;
}
static int
run_string(gs_main_instance *minst,
           const char       *str,
           int               options,
           int               user_errors,
           int              *pexit_code,
           ref              *perror_object)
{
    int exit_code;
    ref error_object;
    int code;

    if (pexit_code == NULL)
        pexit_code = &exit_code;
    if (perror_object == NULL)
        perror_object = &error_object;

    code = gs_main_run_string(minst, str, user_errors,
                              pexit_code, perror_object);

    if ((options & runFlush) || code != 0) {
        zflush(minst->i_ctx_p);         /* flush stdout */
        zflushpage(minst->i_ctx_p);     /* force display update */
    }
    return run_finish(minst, code, *pexit_code, perror_object);
}
static int
run_finish(gs_main_instance *minst, int code, int exit_code,
           ref * perror_object)
{
    switch (code) {
        case gs_error_Quit:
        case 0:
            break;
        case gs_error_Fatal:
            if (exit_code == gs_error_InterpreterExit)
                code = exit_code;
            else
                emprintf1(minst->heap,
                          "Unrecoverable error, exit code %d\n",
                          exit_code);
            break;
        default:
            gs_main_dump_stack(minst, code, perror_object);
    }
    return code;
}

/* Redirect stdout to a file:
 *  -sstdout=filename
 *  -sstdout=-
 *  -sstdout=%stdout
 *  -sstdout=%stderr
 * -sOutputFile=- is not affected.
 * File is closed at program exit (if not stdout/err)
 * or when -sstdout is used again.
 */
static int
try_stdout_redirect(gs_main_instance * minst,
    const char *command, const char *filename)
{
    gs_lib_ctx_core_t *core = minst->heap->gs_lib_ctx->core;
    if (strcmp(command, "stdout") == 0) {
        core->stdout_to_stderr = 0;
        core->stdout_is_redirected = 0;
        /* If stdout already being redirected and it is not stdout
         * or stderr, close it
         */
        if (core->fstdout2
            && (gp_get_file(core->fstdout2) != core->fstdout)
            && (gp_get_file(core->fstdout2) != core->fstderr)) {
            gp_fclose(core->fstdout2);
            core->fstdout2 = NULL;
        }
        /* If stdout is being redirected, set minst->fstdout2 */
        if ( (filename != 0) && strlen(filename) &&
            strcmp(filename, "-") && strcmp(filename, "%stdout") ) {
            if (strcmp(filename, "%stderr") == 0) {
                core->stdout_to_stderr = 1;
            } else {
                core->fstdout2 = gp_fopen(minst->heap, filename, "w");
                if (core->fstdout2 == NULL)
                    return_error(gs_error_invalidfileaccess);
            }
            core->stdout_is_redirected = 1;
        }
        return 0;
    }
    return 1;
}

/* ---------------- Print information ---------------- */

/*
 * Help strings.  We have to break them up into parts, because
 * the Watcom compiler has a limit of 510 characters for a single token.
 * For PC displays, we want to limit the strings to 24 lines.
 */
static const char help_usage1[] = "\
Usage: gs [switches] [file1.ps file2.ps ...]\n\
Most frequently used switches: (you can use # in place of =)\n\
 -dNOPAUSE           no pause after page   | -q       `quiet', fewer messages\n\
 -g<width>x<height>  page size in pixels   | -r<res>  pixels/inch resolution\n";
static const char help_usage2[] = "\
 -sDEVICE=<devname>  select device         | -dBATCH  exit after last file\n\
 -sOutputFile=<file> select output file: - for stdout, |command for pipe,\n\
                                         embed %d or %ld for page #\n";
#ifdef DEBUG
static const char help_debug[] = "\
 --debug=<option>[,<option>]*  select debugging options\n\
 --debug                       list debugging options\n";
#endif
static const char help_trailer[] = "\
For more information, see %s\n\
Please report bugs to bugs.ghostscript.com.\n";
static const char help_devices[] = "Available devices:";
static const char help_default_device[] = "Default output device:";
static const char help_emulators[] = "Input formats:";
static const char help_paths[] = "Search path:";
#ifdef HAVE_FONTCONFIG
static const char help_fontconfig[] = "Ghostscript is also using fontconfig to search for font files\n";
#else
static const char help_fontconfig[] = "";
#endif

extern_gx_io_device_table();

/* Print the standard help message. */
static void
print_help(gs_main_instance * minst)
{
    int i, have_rom_device = 0;

    print_revision(minst);
    print_usage(minst);
    print_emulators(minst);
    print_devices(minst);
    print_paths(minst);
    /* Check if we have the %rom device */
    for (i = 0; i < gx_io_device_table_count; i++) {
        const gx_io_device *iodev = gx_io_device_table[i];
        const char *dname = iodev->dname;

        if (dname && strlen(dname) == 5 && !memcmp("%rom%", dname, 5)) {
            struct stat pstat;
            /* gs_error_unregistered means no usable romfs is available */
            int code = iodev->procs.file_status((gx_io_device *)iodev, dname, &pstat);
            if (code != gs_error_unregistered){
                have_rom_device = 1;
            }
            break;
        }
    }
    if (have_rom_device) {
        outprintf(minst->heap, "Initialization files are compiled into the executable.\n");
    }
    print_help_trailer(minst);
}

/* Print the revision, revision date, and copyright. */
static void
print_revision(const gs_main_instance *minst)
{
    printf_program_ident(minst->heap, gs_product, gs_revision);
    outprintf(minst->heap, " (%d-%02d-%02d)\n%s\n",
            (int)(gs_revisiondate / 10000),
            (int)(gs_revisiondate / 100 % 100),
            (int)(gs_revisiondate % 100),
            gs_copyright);
}

/* Print the version number. */
static void
print_version(const gs_main_instance *minst)
{
    printf_program_ident(minst->heap, NULL, gs_revision);
}

/* Print usage information. */
static void
print_usage(const gs_main_instance *minst)
{
    outprintf(minst->heap, "%s", help_usage1);
    outprintf(minst->heap, "%s", help_usage2);
#ifdef DEBUG
    outprintf(minst->heap, "%s", help_debug);
#endif
}

/* compare function for qsort */
static int
cmpstr(const void *v1, const void *v2)
{
    return strcmp( *(char * const *)v1, *(char * const *)v2 );
}

/* Print the list of available devices. */
static void
print_devices(const gs_main_instance *minst)
{
    outprintf(minst->heap, "%s", help_default_device);
    outprintf(minst->heap, " %s\n", gs_devicename(gs_getdefaultdevice()));
    outprintf(minst->heap, "%s", help_devices);
    {
        int i;
        int pos = 100;
        const gx_device *pdev;
        const char **names;
        size_t ndev = 0;

        for (i = 0; gs_getdevice(i) != 0; i++)
            ;
        ndev = (size_t)i;
        names = (const char **)gs_alloc_bytes(minst->heap, ndev * sizeof(const char*), "print_devices");
        if (names == (const char **)NULL) { /* old-style unsorted device list */
            for (i = 0; (pdev = gs_getdevice(i)) != 0; i++) {
                const char *dname = gs_devicename(pdev);
                int len = strlen(dname);

                if (pos + 1 + len > 76)
                    outprintf(minst->heap, "\n  "), pos = 2;
                outprintf(minst->heap, " %s", dname);
                pos += 1 + len;
            }
        }
        else {                          /* new-style sorted device list */
            for (i = 0; (pdev = gs_getdevice(i)) != 0; i++)
                names[i] = gs_devicename(pdev);
            qsort((void*)names, ndev, sizeof(const char*), cmpstr);
            for (i = 0; i < ndev; i++) {
                int len = strlen(names[i]);

                if (pos + 1 + len > 76)
                    outprintf(minst->heap, "\n  "), pos = 2;
                outprintf(minst->heap, " %s", names[i]);
                pos += 1 + len;
            }
            gs_free(minst->heap, (char *)names, ndev * sizeof(const char*), 1, "print_devices");
        }
    }
    outprintf(minst->heap, "\n");
}

/* Print the list of language emulators. */
static void
print_emulators(const gs_main_instance *minst)
{
    outprintf(minst->heap, "%s", help_emulators);
    {
        const byte *s;

        for (s = gs_emulators; s[0] != 0; s += strlen((const char *)s) + 1)
            outprintf(minst->heap, " %s", s);
    }
    outprintf(minst->heap, "\n");
}

/* Print the search paths. */
static void
print_paths(gs_main_instance * minst)
{
    outprintf(minst->heap, "%s", help_paths);
    gs_main_set_lib_paths(minst);
    {
        uint count = r_size(&minst->lib_path.list);
        uint i;
        int pos = 100;
        char fsepr[3];

        fsepr[0] = ' ', fsepr[1] = gp_file_name_list_separator,
            fsepr[2] = 0;
        for (i = 0; i < count; ++i) {
            const ref *prdir =
            minst->lib_path.list.value.refs + i;
            uint len = r_size(prdir);
            const char *sepr = (i == count - 1 ? "" : fsepr);

            if (1 + pos + strlen(sepr) + len > 76)
                outprintf(minst->heap, "\n  "), pos = 2;
            outprintf(minst->heap, " ");
            /*
             * This is really ugly, but it's necessary because some
             * platforms rely on all console output being funneled through
             * outprintf.  We wish we could just do:
             fwrite(prdir->value.bytes, 1, len, minst->fstdout);
             */
            {
                const char *p = (const char *)prdir->value.bytes;
                uint j;

                for (j = len; j; j--)
                    outprintf(minst->heap, "%c", *p++);
            }
            outprintf(minst->heap, "%s", sepr);
            pos += 1 + len + strlen(sepr);
        }
    }
    outprintf(minst->heap, "\n");
    outprintf(minst->heap, "%s", help_fontconfig);
}

/* Print the help trailer. */
static void
print_help_trailer(const gs_main_instance *minst)
{
    char buffer[gp_file_name_sizeof];
    const char *use_htm = "Use.html", *p = buffer;
    const char *rtd_url = "https://ghostscript.readthedocs.io/en";
    const char *latest_ver="latest";
    const char *vers = latest_ver;
    const char *gs_str = "gs";
    const char *gs = "";
    uint blen = sizeof(buffer);

    if (strlen(GS_PRODUCT) == strlen(GS_PRODUCTFAMILY)) {
        vers = GS_STRINGIZE(GS_DOT_VERSION);
        gs = gs_str;
    }

    snprintf(buffer, blen, "%s/%s%s/%s", rtd_url, gs, vers, use_htm);
    outprintf(minst->heap, help_trailer, p);
}
