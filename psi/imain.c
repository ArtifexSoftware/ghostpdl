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


/* Common support for interpreter front ends */


#include "malloc_.h"
#include "memory_.h"
#include "string_.h"
#include "ghost.h"
#include "gp.h"
#include "gscdefs.h"            /* for gs_init_file */
#include "gslib.h"
#include "gsmatrix.h"           /* for gxdevice.h */
#include "gsutil.h"             /* for bytes_compare */
#include "gspaint.h"		/* for gs_erasepage */
#include "gxdevice.h"
#include "gxdevsop.h"		/* for gxdso_* enums */
#include "gxclpage.h"
#include "gdevprn.h"
#include "gxalloc.h"
#include "gxiodev.h"            /* for iodev struct */
#include "gzstate.h"
#include "ierrors.h"
#include "oper.h"
#include "iconf.h"              /* for gs_init_* imports */
#include "idebug.h"
#include "iddict.h"
#include "iname.h"              /* for name_init */
#include "dstack.h"
#include "estack.h"
#include "ostack.h"             /* put here for files.h */
#include "stream.h"             /* for files.h */
#include "files.h"
#include "ialloc.h"
#include "iinit.h"
#include "strimpl.h"            /* for sfilter.h */
#include "sfilter.h"            /* for iscan.h */
#include "iscan.h"
#include "main.h"
#include "store.h"
#include "isave.h"              /* for prototypes */
#include "interp.h"
#include "ivmspace.h"
#include "idisp.h"              /* for setting display device callback */
#include "iplugin.h"
#include "zfile.h"

#include "valgrind.h"

/* ------ Exported data ------ */

/** using backpointers retrieve minst from any memory pointer
 *
 */
gs_main_instance*
get_minst_from_memory(const gs_memory_t *mem)
{
    return (gs_main_instance*)mem->gs_lib_ctx->top_of_system;
}

/** construct main instance caller needs to retain */
gs_main_instance *
gs_main_alloc_instance(gs_memory_t *mem)
{
    gs_main_instance *minst;
    if (mem == NULL)
        return NULL;

    minst = (gs_main_instance *)gs_alloc_bytes_immovable(mem,
                                                         sizeof(gs_main_instance),
                                                         "init_main_instance");
    if (minst == NULL)
        return NULL;
    memset(minst, 0, sizeof(gs_main_instance));
    memcpy(minst, &gs_main_instance_init_values, sizeof(gs_main_instance_init_values));
    minst->heap = mem;
    mem->gs_lib_ctx->top_of_system = minst;
    return minst;
}

op_array_table *
get_op_array(const gs_memory_t *mem, int size)
{
    gs_main_instance *minst = get_minst_from_memory(mem);
    return op_index_op_array_table(minst->i_ctx_p,size);
}

op_array_table *
get_global_op_array(const gs_memory_t *mem)
{
    gs_main_instance *minst = get_minst_from_memory(mem);
    return &minst->i_ctx_p->op_array_table_global;
}

op_array_table *
get_local_op_array(const gs_memory_t *mem)
{
    gs_main_instance *minst = get_minst_from_memory(mem);
    return &minst->i_ctx_p->op_array_table_local;
}

/* ------ Forward references ------ */

static int gs_run_init_file(gs_main_instance *, int *, ref *);
void print_resource_usage(const gs_main_instance *,
                                  gs_dual_memory_t *, const char *);

/* ------ Initialization ------ */

/* Initialization to be done before anything else. */
int
gs_main_init0(gs_main_instance * minst, gp_file * in, gp_file * out, gp_file * err,
              int max_lib_paths)
{
    ref *array;
    int code = 0;

    if (gs_debug_c(gs_debug_flag_init_details))
        dmprintf1(minst->heap, "%% Init phase 0 started, instance "PRI_INTPTR"\n",
                  (intptr_t)minst);

    /* Do platform-dependent initialization. */
    /* We have to do this as the very first thing, */
    /* because it detects attempts to run 80N86 executables (N>0) */
    /* on incompatible processors. */
    gp_init();

    /* Initialize the imager. */

    /* Reset debugging flags */
#ifdef PACIFY_VALGRIND
    VALGRIND_HG_DISABLE_CHECKING(gs_debug, 128);
#endif
    memset(gs_debug, 0, 128);
    gs_log_errors = 0;  /* gs_debug['#'] = 0 */

    gp_get_realtime(minst->base_time);

    /* Initialize the file search paths. */
    array = (ref *) gs_alloc_byte_array(minst->heap, max_lib_paths, sizeof(ref),
                                        "lib_path array");
    if (array == 0) {
        gs_lib_finit(1, gs_error_VMerror, minst->heap);
        code = gs_note_error(gs_error_VMerror);
        goto fail;
    }
    make_array(&minst->lib_path.container, avm_foreign, max_lib_paths,
               array);
    make_array(&minst->lib_path.list, avm_foreign | a_readonly, 0,
               minst->lib_path.container.value.refs);
    minst->lib_path.env = NULL;
    minst->lib_path.final = NULL;
    minst->lib_path.count = 0;
    minst->lib_path.first_is_current = 0;
    minst->user_errors = 1;
    minst->init_done = 0;

fail:
    if (gs_debug_c(gs_debug_flag_init_details))
        dmprintf2(minst->heap, "%% Init phase 0 %s, instance "PRI_INTPTR"\n",
                  code < 0 ? "failed" : "done", (intptr_t)minst);

    return code;
}

/* Initialization to be done before constructing any objects. */
int
gs_main_init1(gs_main_instance * minst)
{
    gs_dual_memory_t idmem;
    name_table *nt = NULL;
    int code;

    if (minst->init_done >= 1)
        return 0;

    if (gs_debug_c(gs_debug_flag_init_details))
        dmprintf1(minst->heap, "%% Init phase 1 started, instance "PRI_INTPTR"\n",
                  (intptr_t)minst);

    code = ialloc_init(&idmem, minst->heap,
                       minst->memory_clump_size, gs_have_level2());

    if (code < 0)
        goto fail_early;

    code = gs_lib_init1((gs_memory_t *)idmem.space_system);
    if (code < 0)
        goto fail;
    alloc_save_init(&idmem);
    {
        gs_memory_t *mem = (gs_memory_t *)idmem.space_system;
        nt = names_init(minst->name_table_size, idmem.space_system);

        if (nt == 0) {
            code = gs_note_error(gs_error_VMerror);
            goto fail;
        }
        mem->gs_lib_ctx->gs_name_table = nt;
        code = gs_register_struct_root(mem, &mem->gs_lib_ctx->name_table_root,
                                       (void **)&mem->gs_lib_ctx->gs_name_table,
                                       "the_gs_name_table");
        if (code < 0)
            goto fail;
        mem->gs_lib_ctx->client_check_file_permission = z_check_file_permissions;
    }
    code = obj_init(&minst->i_ctx_p, &idmem);  /* requires name_init */
    if (code < 0)
        goto fail;
    minst->init_done = 1;
    code = i_plugin_init(minst->i_ctx_p);
    if (code < 0)
        goto fail;
    code = i_iodev_init(&idmem);
    if (code < 0) {
fail:
        names_free(nt);
        if (minst->i_ctx_p == NULL)
            ialloc_finit(&idmem);
    }
fail_early:

    if (gs_debug_c(gs_debug_flag_init_details))
        dmprintf2(minst->heap, "%% Init phase 1 %s, instance "PRI_INTPTR"\n",
                  code < 0 ? "failed" : "done", (intptr_t)minst);

    return code;
}

/*
 * Invoke the interpreter. This layer doesn't do much (previously stdio
 * callouts were handled here instead of in the stream processing.
 */
static int
gs_main_interpret(gs_main_instance *minst, ref * pref, int user_errors,
                  int *pexit_code, ref * perror_object)
{
    int code;

    /* set interpreter pointer to lib_path */
    minst->i_ctx_p->lib_path = &minst->lib_path;

    code = gs_interpret(&minst->i_ctx_p, pref,
                        user_errors, pexit_code, perror_object);
    return code;
}

/* gcc wants prototypes for all external functions. */
int gs_main_init2aux(gs_main_instance * minst);

static const op_array_table empty_table = { { { 0 } } };

/* This is an external function to work around      */
/* a bug in gcc 4.5.1 optimizer. See bug 692684.    */
int gs_main_init2aux(gs_main_instance * minst) {
    i_ctx_t * i_ctx_p = minst->i_ctx_p;

    if (minst->init_done < 2) {
        int code, exit_code;
        ref error_object, ifa;

        /* Set up enough so that we can safely be garbage collected */
        i_ctx_p->op_array_table_global = empty_table;
        i_ctx_p->op_array_table_local = empty_table;

        code = zop_init(i_ctx_p);
        if (code < 0)
            return code;
        code = op_init(i_ctx_p);        /* requires obj_init */
        if (code < 0)
            return code;

        /* Set up the array of additional initialization files. */
        make_const_string(&ifa, a_readonly | avm_foreign, gs_init_files_sizeof - 2, gs_init_files);
        code = i_initial_enter_name(i_ctx_p, "INITFILES", &ifa);
        if (code < 0)
            return code;

        /* Set up the array of emulator names. */
        make_const_string(&ifa, a_readonly | avm_foreign, gs_emulators_sizeof - 2, gs_emulators);
        code = i_initial_enter_name(i_ctx_p, "EMULATORS", &ifa);
        if (code < 0)
            return code;

        /* Pass the search path. */
        code = i_initial_enter_name(i_ctx_p, "LIBPATH", &minst->lib_path.list);
        if (code < 0)
            return code;

        /* Execute the standard initialization file. */
        code = gs_run_init_file(minst, &exit_code, &error_object);
        if (code < 0)
            return code;
        minst->init_done = 2;

        /* NB this is to be done with device parameters
         * both minst->display and  display_set_callback() are going away
        */
        if ((code = reopen_device_if_required(minst)) < 0)
            return code;

        if ((code = gs_main_run_string(minst,
                "JOBSERVER "
                " { false 0 .startnewjob } "
                " { NOOUTERSAVE not { save pop } if } "
                "ifelse", 0, &exit_code,
                &error_object)) < 0)
           return code;
    }
    return 0;
}

int
gs_main_set_language_param(gs_main_instance *minst,
                           gs_param_list    *plist)
{
    ref value;
    int code = 0;
    i_ctx_t *i_ctx_p = minst->i_ctx_p;
    uint space = icurrent_space;
    gs_param_enumerator_t enumerator;
    gs_param_key_t key;
    gs_lib_ctx_t *ctx = minst->heap->gs_lib_ctx;
    ref error_object;

    /* If we're up and running as a jobserver, exit encapsulation. */
    if (minst->init_done > 1) {
        code = gs_main_run_string(minst,
                                  "JOBSERVER {true 0 startjob pop} if",
                                  0, &code, &error_object);
        if (code < 0) return code;
    }

    ialloc_set_space(idmemory, avm_system);

    param_init_enumerator(&enumerator);
    while ((code = param_get_next_key(plist, &enumerator, &key)) == 0) {
        char string_key[256];	/* big enough for any reasonable key */
        gs_param_typed_value pvalue;

        if (key.size > sizeof(string_key) - 1) {
            code = gs_note_error(gs_error_rangecheck);
            break;
        }
        memcpy(string_key, key.data, key.size);
        string_key[key.size] = 0;
        if ((code = param_read_typed(plist, string_key, &pvalue)) != 0) {
            code = (code > 0 ? gs_note_error(gs_error_unknownerror) : code);
            break;
        }
        switch (pvalue.type) {
        case gs_param_type_null:
            make_null(&value);
            break;
        case gs_param_type_bool:
            if (pvalue.value.b)
                make_true(&value);
            else
                make_false(&value);
            break;
        case gs_param_type_int:
            make_int(&value, (ps_int)pvalue.value.i);
            break;
        case gs_param_type_i64:
            make_int(&value, (ps_int)pvalue.value.i64);
            break;
        case gs_param_type_long:
            make_int(&value, (ps_int)pvalue.value.l);
            break;
        case gs_param_type_size_t:
            make_int(&value, (ps_int)pvalue.value.z);
            break;
        case gs_param_type_float:
            make_real(&value, pvalue.value.f);
            break;
        case gs_param_type_dict:
            /* We don't support dicts for now */
            continue;
        case gs_param_type_dict_int_keys:
            /* We don't support dicts with int keys for now */
            continue;
        case gs_param_type_array:
            /* We don't support arrays for now */
            continue;
        case gs_param_type_string:
            if (pvalue.value.s.data == NULL || pvalue.value.s.size == 0)
                make_empty_string(&value, a_readonly);
            else {
                size_t len = pvalue.value.s.size;
                byte *body = ialloc_string(len, "-s");

                if (body == NULL)
                    return gs_error_Fatal;
                memcpy(body, pvalue.value.s.data, len);
                make_const_string(&value, a_readonly | avm_system, len, body);
            }
            break;
        case gs_param_type_name:
            code = name_ref(ctx->memory, pvalue.value.n.data, pvalue.value.n.size, &value, 1);
            break;
        case gs_param_type_int_array:
            /* We don't support arrays for now */
            continue;
        case gs_param_type_float_array:
            /* We don't support arrays for now */
            continue;
        case gs_param_type_string_array:
            /* We don't support arrays for now */
            continue;
        default:
            continue;
        }
        if (code < 0)
            break;

        ialloc_set_space(idmemory, space);
        /* Enter the name in systemdict. */
        i_initial_enter_name_copy(minst->i_ctx_p, string_key, &value);
    }

    if (minst->init_done > 1) {
        int code2 = 0;
        code2 = gs_main_run_string(minst,
                                   "JOBSERVER {false 0 startjob pop} if",
                                   0, &code2, &error_object);
        if (code >= 0)
            code = code2;
    }

    return code;
}

static int
gs_main_push_params(gs_main_instance *minst)
{
    int code;
    gs_c_param_list *plist;

    plist = minst->param_list;
    if (!plist)
        return 0;

    code = gs_putdeviceparams(minst->i_ctx_p->pgs->device,
                              (gs_param_list *)plist);
    if (code < 0)
        return code;

    code = gs_main_set_language_param(minst, (gs_param_list *)plist);
    if (code < 0)
        return code;

    gs_c_param_list_release(plist);

    return code;
}

int
gs_main_init2(gs_main_instance * minst)
{
    i_ctx_t *i_ctx_p;
    int code = gs_main_init1(minst);

    if (code < 0)
        return code;

    code = gs_main_push_params(minst);
    if (code < 0)
        return code;

    if (minst->init_done >= 2)
        return 0;

    if (gs_debug_c(gs_debug_flag_init_details))
        dmprintf1(minst->heap, "%% Init phase 2 started, instance "PRI_INTPTR"\n",
                  (intptr_t)minst);

    code = gs_main_init2aux(minst);
    if (code < 0)
       goto fail;

    i_ctx_p = minst->i_ctx_p; /* reopen_device_if_display or run_string may change it */

    /* Now process the initial saved-pages=... argument, if any as well as saved-pages-test */
    {
       gx_device *pdev = gs_currentdevice(minst->i_ctx_p->pgs);	/* get the current device */
       gx_device_printer *ppdev = (gx_device_printer *)pdev;

        if (minst->saved_pages_test_mode) {
            if ((dev_proc(pdev, dev_spec_op)(pdev, gxdso_supports_saved_pages, NULL, 0) <= 0)) {
                /* no warning or error if saved-pages-test mode is used, just disable it */
                minst->saved_pages_test_mode = false;  /* device doesn't support it */
            } else {
                if ((code = gx_saved_pages_param_process(ppdev, (byte *)"begin", 5)) < 0)
                    goto fail;
                if (code > 0)
                    if ((code = gs_erasepage(minst->i_ctx_p->pgs)) < 0)
                        goto fail;
            }
        } else if (minst->saved_pages_initial_arg != NULL) {
            if (dev_proc(pdev, dev_spec_op)(pdev, gxdso_supports_saved_pages, NULL, 0) <= 0) {
                while (pdev->child)
                    pdev = pdev->child;		/* so we get the real device name */
                outprintf(minst->heap,
                          "   --saved-pages not supported by the '%s' device.\n",
                          pdev->dname);
                code = gs_error_Fatal;
                goto fail;
            }
            code = gx_saved_pages_param_process(ppdev, (byte *)minst->saved_pages_initial_arg,
                                                strlen(minst->saved_pages_initial_arg));
            if (code > 0)
                if ((code = gs_erasepage(minst->i_ctx_p->pgs)) < 0)
                    goto fail;
        }
    }

    if (code >= 0) {
        if (gs_debug_c(':'))
            print_resource_usage(minst, i_ctx_p ? &gs_imemory : NULL, "Start");
        gp_readline_init(&minst->readline_data, minst->heap); /* lgtm [cpp/useless-expression] */
    }

fail:
    if (gs_debug_c(gs_debug_flag_init_details))
        dmprintf2(minst->heap, "%% Init phase 2 %s, instance "PRI_INTPTR"\n",
                  code < 0 ? "failed" : "done", (intptr_t)minst);

    return code;
}

/* ------ Search paths ------ */

#define LIB_PATH_EXTEND 5

/* If the existing array is full, extend it */
static int
extend_path_list_container (gs_main_instance * minst, gs_file_path * pfp)
{
    uint len = r_size(&minst->lib_path.container);
    ref *paths, *opaths = minst->lib_path.container.value.refs;

    /* Add 5 entries at a time to reduce VM thrashing */
    paths = (ref *) gs_alloc_byte_array(minst->heap, len + LIB_PATH_EXTEND, sizeof(ref),
                                        "extend_path_list_container array");

    if (paths == 0) {
        return_error(gs_error_VMerror);
    }
    make_array(&minst->lib_path.container, avm_foreign, len + LIB_PATH_EXTEND, paths);
    make_array(&minst->lib_path.list, avm_foreign | a_readonly, 0,
               minst->lib_path.container.value.refs);

    memcpy(paths, opaths, len * sizeof(ref));
    r_set_size(&minst->lib_path.list, len);

    gs_free_object (minst->heap, opaths, "extend_path_list_container");
    return(0);
}

static int
lib_path_insert_copy_of_string(gs_main_instance *minst, int pos, size_t strlen, const char *str)
{
    ref *paths;
    int listlen = r_size(&minst->lib_path.list); /* The real number of entries currently in the list */
    byte *newstr;
    int code;

    /* Extend the container if required */
    if (listlen == r_size(&minst->lib_path.container)) {
        code = extend_path_list_container(minst, &minst->lib_path);
        if (code < 0) {
            emprintf(minst->heap, "\nAdding path to search paths failed.\n");
            return(code);
        }
    }

    /* Copy the string */
    newstr = gs_alloc_string(minst->heap, strlen, "lib_path_add");
    if (newstr == NULL)
        return gs_error_VMerror;
    memcpy(newstr, str, strlen);

    /* Shuffle up the entries */
    paths = &minst->lib_path.container.value.refs[pos];
    if (pos != listlen)
        memmove(paths + 1, paths, (listlen-pos) * sizeof(*paths));

    /* And insert the new string, converted to a PS thing */
    make_const_string(paths, avm_foreign | a_readonly,
                      strlen,
                      newstr);

    /* Update the list length */
    r_set_size(&minst->lib_path.list, listlen+1);

    return 0;
}

/* Internal routine to add a set of directories to a search list. */
/* Returns 0 or an error code. */

static int
lib_path_add(gs_main_instance * minst, const char *dirs)
{
    gs_file_path * pfp = &minst->lib_path;
    uint len = r_size(&pfp->list);
    const char *dpath = dirs;
    int code;

    if (dirs == 0)
        return 0;

    for (;;) {                  /* Find the end of the next directory name. */
        const char *npath = dpath;

        while (*npath != 0 && *npath != gp_file_name_list_separator)
            npath++;
        if (npath > dpath) {
            code = gs_add_control_path_len(minst->heap, gs_permit_file_reading, dpath, npath - dpath);
            if (code < 0) return code;

            code = lib_path_insert_copy_of_string(minst, len, npath - dpath, dpath);
            if (code < 0)
                return code;
            len++;
            /* Update length/count so we don't lose entries if a later
             * iteration fails. */
            r_set_size(&pfp->list, len);
        }
        if (!*npath)
            break;
        dpath = npath + 1;
    }
    return 0;
}

static void
set_lib_path_length(gs_main_instance * minst, int newsize)
{
    gs_file_path * pfp = &minst->lib_path;
    uint len = r_size(&pfp->list); /* Yes, list, not container */
    uint i;

    /* Free any entries that are discarded by us shortening the list */
    for (i = newsize; i < len; i++)
        gs_free_object(minst->heap, pfp->container.value.refs[i].value.bytes, "lib_path entry");
    r_set_size(&pfp->list, newsize);
}

/* Add a library search path to the list. */
int
gs_main_add_lib_path(gs_main_instance * minst, const char *lpath)
{
    gs_file_path * pfp = &minst->lib_path;
    int code;

    /* Throw away all the elements after the user ones. */
    set_lib_path_length(minst, pfp->count + pfp->first_is_current);

    /* Now add the new one */
    code = lib_path_add(minst, lpath);
    if (code < 0)
        return code;

    /* Update the count of user paths */
    pfp->count = r_size(&pfp->list) - pfp->first_is_current;

    /* Now add back in the others */
    return gs_main_set_lib_paths(minst);
}

/* ------ Execution ------ */

extern_gx_io_device_table();

/* Complete the list of library search paths. */
/* This may involve adding the %rom%Resource/Init and %rom%lib/ paths (for COMPILE_INITS) */
/* and adding or removing the current directory as the first element (for -P and -P-). */
int
gs_main_set_lib_paths(gs_main_instance * minst)
{
    int code = 0;
    int i, have_rom_device = 0;

    /* We are entered with a list potentially full of stuff already.
     * In general the list is of the form:
     *    <optionally, the current directory>
     *    <any -I specified directories, "count" of them>
     *    <any directories from the environment variable>
     *    <if there is a romfs, the romfs paths>
     *    <any directories from the 'final' variable>
     *
     * Unfortunately, the value read from the environment variable may not
     * have actually been read the first time this function is called, so
     * we can see the value change over time. Short of a complete rewrite
     * this requires us to clear the latter part of the list and repopulate
     * it each time we run through.
     */

    /* First step, ensure that we have the current directory as our
     * first entry, iff we need it. */
    if (minst->search_here_first && !minst->lib_path.first_is_current) {
        /* We should have a "gp_current_directory" at the start, and we haven't.
         * So insert one. */

        code = gs_add_control_path_len(minst->heap, gs_permit_file_reading,
                gp_current_directory_name, strlen(gp_current_directory_name));
        if (code < 0) return code;

        code = lib_path_insert_copy_of_string(minst, 0,
                                              strlen(gp_current_directory_name),
                                              gp_current_directory_name);
        if (code < 0)
            return code;
    } else if (!minst->search_here_first && minst->lib_path.first_is_current) {
        /* We have a "gp_current_directory" at the start, and we shouldn't have.
         * Remove it. */
        int listlen = r_size(&minst->lib_path.list); /* The real number of entries currently in the list */
        ref *paths = minst->lib_path.container.value.refs;

        gs_free_object(minst->heap, paths->value.bytes, "lib_path entry");
        --listlen;
        memmove(paths, paths + 1, listlen * sizeof(*paths));
        r_set_size(&minst->lib_path.list, listlen);

        code = gs_remove_control_path_len(minst->heap, gs_permit_file_reading,
                gp_current_directory_name, strlen(gp_current_directory_name));
        if (code < 0) return code;
    }
    minst->lib_path.first_is_current = minst->search_here_first;

    /* Now, the horrid bit. We throw away all the entries after the user entries */
    set_lib_path_length(minst, minst->lib_path.count + minst->lib_path.first_is_current);

    /* Now we (re)populate the end of the list. */
    if (minst->lib_path.env != 0) {
        code = lib_path_add(minst, minst->lib_path.env);
        if (code < 0) return code;

        code = gs_add_control_path(minst->heap, gs_permit_file_reading, minst->lib_path.env);
        if (code < 0) return code;
    }
    /* now put the %rom%lib/ device path before the gs_lib_default_path on the list */
    for (i = 0; i < gx_io_device_table_count; i++) {
        const gx_io_device *iodev = gx_io_device_table[i];
        const char *dname = iodev->dname;
        const char *fcheckname = "Resource/Init/gs_init.ps";

        if (dname && strlen(dname) == 5 && !memcmp("%rom%", dname, 5)) {
            struct stat pstat;
            /* gs_error_unregistered means no usable romfs is available */
            /* gpdl always includes a real romfs, but it may or may have the Postscript
               resources in it, so we explicitly check for a fairly vital file
             */
            int code = iodev->procs.file_status((gx_io_device *)iodev, fcheckname, &pstat);
            if (code != gs_error_unregistered && code != gs_error_undefinedfilename){
                have_rom_device = 1;
            }
            break;
        }
    }
    if (have_rom_device) {
        code = lib_path_add(minst, "%rom%Resource/Init/");
        if (code < 0)
            return code;
        code = lib_path_add(minst, "%rom%lib/");
    }
    else code = 0;

    if (minst->lib_path.final != NULL && code >= 0)
        code = lib_path_add(minst, minst->lib_path.final);
    return code;
}

/* Open a file, using the search paths. */
int
gs_main_lib_open(gs_main_instance * minst, const char *file_name, ref * pfile)
{
    /* This is a separate procedure only to avoid tying up */
    /* extra stack space while running the file. */
    i_ctx_t *i_ctx_p = minst->i_ctx_p;
#define maxfn 2048
    char fn[maxfn];
    uint len;

    return lib_file_open(&minst->lib_path, imemory,
                         NULL, /* Don't check permissions here, because permlist
                                  isn't ready running init files. */
                          file_name, strlen(file_name), fn, maxfn, &len, pfile);
}

/* Open and execute a file. */
int
gs_main_run_file(gs_main_instance * minst, const char *file_name, int user_errors, int *pexit_code, ref * perror_object)
{
    ref initial_file;
    int code = gs_main_run_file_open(minst, file_name, &initial_file);

    if (code < 0)
        return code;
    return gs_main_interpret(minst, &initial_file, user_errors,
                        pexit_code, perror_object);
}
int
gs_main_run_file_open(gs_main_instance * minst, const char *file_name, ref * pfref)
{
    gs_main_set_lib_paths(minst);
    if (gs_main_lib_open(minst, file_name, pfref) < 0) {
        emprintf1(minst->heap,
                  "Can't find initialization file %s.\n",
                  file_name);
        return_error(gs_error_Fatal);
    }
    r_set_attrs(pfref, a_execute + a_executable);
    return 0;
}

/* Open and run the very first initialization file. */
static int
gs_run_init_file(gs_main_instance * minst, int *pexit_code, ref * perror_object)
{
    i_ctx_t *i_ctx_p = minst->i_ctx_p;
    ref ifile;
    ref first_token;
    int code;
    scanner_state state;

    gs_main_set_lib_paths(minst);
    code = gs_main_run_file_open(minst, gs_init_file, &ifile);
    if (code < 0) {
        *pexit_code = 255;
        return code;
    }
    /* Check to make sure the first token is an integer */
    /* (for the version number check.) */
    gs_scanner_init(&state, &ifile);
    code = gs_scan_token(i_ctx_p, &first_token, &state);
    if (code != 0 || !r_has_type(&first_token, t_integer)) {
        emprintf1(minst->heap,
                  "Initialization file %s does not begin with an integer.\n",
                  gs_init_file);
        *pexit_code = 255;
        return_error(gs_error_Fatal);
    }
    *++osp = first_token;
    r_set_attrs(&ifile, a_executable);
    return gs_main_interpret(minst, &ifile, minst->user_errors,
                        pexit_code, perror_object);
}

/* Run a string. */
int
gs_main_run_string(gs_main_instance * minst, const char *str, int user_errors,
                   int *pexit_code, ref * perror_object)
{
    return gs_main_run_string_with_length(minst, str, (uint) strlen(str),
                                          user_errors,
                                          pexit_code, perror_object);
}
int
gs_main_run_string_with_length(gs_main_instance * minst, const char *str,
         uint length, int user_errors, int *pexit_code, ref * perror_object)
{
    int code;

    code = gs_main_run_string_begin(minst, user_errors,
                                    pexit_code, perror_object);
    if (code < 0)
        return code;
    code = gs_main_run_string_continue(minst, str, length, user_errors,
                                       pexit_code, perror_object);
    if (code != gs_error_NeedInput)
        return code;

    code = gs_main_run_string_end(minst, user_errors,
                                  pexit_code, perror_object);
    /* Not okay for user to use .needinput
     * This treats it as a fatal error.
     */
    if (code == gs_error_NeedInput)
        return_error(gs_error_Fatal);
    return code;
}

/* Set up for a suspendable run_string. */
int
gs_main_run_string_begin(gs_main_instance * minst, int user_errors,
                         int *pexit_code, ref * perror_object)
{
    const char *setup = ".runstringbegin";
    ref rstr;
    int code;

    gs_main_set_lib_paths(minst);
    make_const_string(&rstr, avm_foreign | a_readonly | a_executable,
                      strlen(setup), (const byte *)setup);
    code = gs_main_interpret(minst, &rstr, user_errors, pexit_code,
                             perror_object);
    return (code == gs_error_NeedInput ? 0 : code == 0 ? gs_error_Fatal : code);
}
/* Continue running a string with the option of suspending. */
int
gs_main_run_string_continue(gs_main_instance * minst, const char *str,
         uint length, int user_errors, int *pexit_code, ref * perror_object)
{
    ref rstr;

    if (length == 0)
        return 0;               /* empty string signals EOF */
    make_const_string(&rstr, avm_foreign | a_readonly, length,
                      (const byte *)str);
    return gs_main_interpret(minst, &rstr, user_errors, pexit_code,
                             perror_object);
}
uint
gs_main_get_uel_offset(gs_main_instance * minst)
{
    if (minst->i_ctx_p == NULL)
        return 0;
    return (uint)minst->i_ctx_p->uel_position;
}

/* Signal EOF when suspended. */
int
gs_main_run_string_end(gs_main_instance * minst, int user_errors,
                       int *pexit_code, ref * perror_object)
{
    ref rstr;

    make_empty_const_string(&rstr, avm_foreign | a_readonly);
    return gs_main_interpret(minst, &rstr, user_errors, pexit_code,
                             perror_object);
}

gs_memory_t *
gs_main_get_device_memory(gs_main_instance * minst)
{
  gs_memory_t *dev_mem = NULL;
  if (minst && minst->init_done >= 1) {
      i_ctx_t * i_ctx_p = minst->i_ctx_p;
      dev_mem = imemory_global->stable_memory;
  }
  return dev_mem;
}

int
gs_main_set_device(gs_main_instance * minst, gx_device *pdev)
{
    i_ctx_t *i_ctx_p = minst->i_ctx_p;
    ref error_object;
    int code;

    if (pdev == NULL) {
        /* Leave job encapsulation, restore the graphics state gsaved below (so back to the nullpage device)
           and re-enter job encapsulation.
           We rely on the end of job encapsulation restore to the put the gstate stack back how it was when
           we entered job encapsulation below, so the grestore will pickup the correct gstate.
         */
        code = gs_main_run_string(minst,
                                 "true 0 startjob pop grestore false 0 startjob pop",
                                 0, &code, &error_object);
        if (code < 0) goto done;
    }
    else {
        /* Leave job encapsulation, and save the graphics state (including the device: nullpage)
           Store the page size in a dictionary, which we'll use to configure the incoming device */
        code = gs_main_run_string(minst,
                                  "true 0 startjob pop gsave "
                                  /* /PageSize /GetDeviceParam .special_op will either return:
                                   * /PageSize [ <width> <height> ] true   (if it exists) or
                                   * false                                 (if it does not) */
                                  "<< /PageSize /GetDeviceParam .special_op "
                                  /* If we wanted to force a default pagesize, we'd do:
                                   * "not { /PageSize [595 842] } if "
                                   * but for now we'll just leave the default as it is, and do: */
                                  "pop "
                                  ">> "
                                  , 0, &code, &error_object);
        if (code < 0) goto done;
        /* First call goes to the C directly to actually set the device. This
         * avoids the SAFER checks. */
        code = zsetdevice_no_safer(i_ctx_p, pdev);
        if (code < 0) goto done;
        code = zcurrentdevice(i_ctx_p);
        if (code < 0) goto done;
        code = gs_main_run_string(minst,
                                  /* Set the device again to the same one. This determines
                                   * whether to erase page or not, but passes the safer
                                   * checks as the device is unchanged. */
                                  "setdevice "
                                  "setpagedevice "
                                  /* GS specifics: Force the cached copy of the params to be updated. */
                                  "currentpagedevice pop "
                                  /* Setup the halftone */
                                  ".setdefaultscreen "
                                  /* Re-run the scheduled initialisation procs, in case we've just set pdfwrite */
                                  "1183615869 internaldict /.execute_scheduled_inits get exec "
                                  /* Re-enter job encapsulation */
                                  "false 0 startjob pop "
                                  , 0, &code, &error_object);
        if (code < 0) goto done;
    }
done:
    return code;
}

/* ------ Operand stack access ------ */

/* These are built for comfort, not for speed. */

static int
push_value(gs_main_instance *minst, ref * pvalue)
{
    i_ctx_t *i_ctx_p = minst->i_ctx_p;
    int code = ref_stack_push(&o_stack, 1);
    ref *o = ref_stack_index(&o_stack, 0L);

    if (o == NULL)
        return_error(gs_error_stackoverflow);

    if (code < 0)
        return code;
    *o = *pvalue;
    return 0;
}

int
gs_push_boolean(gs_main_instance * minst, bool value)
{
    ref vref;

    make_bool(&vref, value);
    return push_value(minst, &vref);
}

int
gs_push_integer(gs_main_instance * minst, long value)
{
    ref vref;

    make_int(&vref, value);
    return push_value(minst, &vref);
}

int
gs_push_real(gs_main_instance * minst, double value)
{
    ref vref;

    make_real(&vref, value);
    return push_value(minst, &vref);
}

int
gs_push_string(gs_main_instance * minst, byte * chars, uint length,
               bool read_only)
{
    ref vref;

    make_string(&vref, avm_foreign | (read_only ? a_readonly : a_all),
                length, (byte *) chars);
    return push_value(minst, &vref);
}

static int
pop_value(i_ctx_t *i_ctx_p, ref * pvalue)
{
    if (!ref_stack_count(&o_stack))
        return_error(gs_error_stackunderflow);
    *pvalue = *ref_stack_index(&o_stack, 0L);
    return 0;
}

int
gs_pop_boolean(gs_main_instance * minst, bool * result)
{
    i_ctx_t *i_ctx_p = minst->i_ctx_p;
    ref vref;
    int code = pop_value(i_ctx_p, &vref);

    if (code < 0)
        return code;
    check_type_only(vref, t_boolean);
    *result = vref.value.boolval;
    ref_stack_pop(&o_stack, 1);
    return 0;
}

int
gs_pop_integer(gs_main_instance * minst, long *result)
{
    i_ctx_t *i_ctx_p = minst->i_ctx_p;
    ref vref;
    int code = pop_value(i_ctx_p, &vref);

    if (code < 0)
        return code;
    check_type_only(vref, t_integer);
    *result = vref.value.intval;
    ref_stack_pop(&o_stack, 1);
    return 0;
}

int
gs_pop_real(gs_main_instance * minst, float *result)
{
    i_ctx_t *i_ctx_p = minst->i_ctx_p;
    ref vref;
    int code = pop_value(i_ctx_p, &vref);

    if (code < 0)
        return code;
    switch (r_type(&vref)) {
        case t_real:
            *result = vref.value.realval;
            break;
        case t_integer:
            *result = (float)(vref.value.intval);
            break;
        default:
            return_error(gs_error_typecheck);
    }
    ref_stack_pop(&o_stack, 1);
    return 0;
}

int
gs_pop_string(gs_main_instance * minst, gs_string * result)
{
    i_ctx_t *i_ctx_p = minst->i_ctx_p;
    ref vref;
    int code = pop_value(i_ctx_p, &vref);

    if (code < 0)
        return code;
    switch (r_type(&vref)) {
        case t_name:
            name_string_ref(minst->heap, &vref, &vref);
            code = 1;
            goto rstr;
        case t_string:
            code = (r_has_attr(&vref, a_write) ? 0 : 1);
          rstr:result->data = vref.value.bytes;
            result->size = r_size(&vref);
            break;
        default:
            return_error(gs_error_typecheck);
    }
    ref_stack_pop(&o_stack, 1);
    return code;
}

/* ------ Termination ------ */

/* Get the names of temporary files.
 * Each name is null terminated, and the last name is
 * terminated by a double null.
 * We retrieve the names of temporary files just before
 * the interpreter finishes, and then delete the files
 * after the interpreter has closed all files.
 */
static char *gs_main_tempnames(gs_main_instance *minst)
{
    i_ctx_t *i_ctx_p = minst->i_ctx_p;
    ref *SAFETY;
    ref *tempfiles;
    ref keyval[2];      /* for key and value */
    char *tempnames = NULL;
    int i;
    int idict;
    int len = 0;
    const byte *data = NULL;
    uint size;
    if (minst->init_done >= 2) {
        if (dict_find_string(systemdict, "SAFETY", &SAFETY) <= 0 ||
            dict_find_string(SAFETY, "tempfiles", &tempfiles) <= 0)
            return NULL;
        /* get lengths of temporary filenames */
        idict = dict_first(tempfiles);
        while ((idict = dict_next(tempfiles, idict, &keyval[0])) >= 0) {
            if (obj_string_data(minst->heap, &keyval[0], &data, &size) >= 0)
                len += size + 1;
        }
        if (len != 0)
            tempnames = (char *)malloc(len+1);
        if (tempnames) {
            memset(tempnames, 0, len+1);
            /* copy temporary filenames */
            idict = dict_first(tempfiles);
            i = 0;
            while ((idict = dict_next(tempfiles, idict, &keyval[0])) >= 0) {
                if (obj_string_data(minst->heap, &keyval[0], &data, &size) >= 0) {
                    memcpy(tempnames+i, (const char *)data, size);
                    i+= size;
                    tempnames[i++] = '\0';
                }
            }
        }
    }
    return tempnames;
}

static void
gs_finit_push_systemdict(i_ctx_t *i_ctx_p)
{
    if (i_ctx_p == NULL)
        return;
    if (dsp == dstop ) {
        if (ref_stack_extend(&d_stack, 1) < 0) {
            /* zend() cannot fail */
            (void)zend(i_ctx_p);
        }
    }
    dsp++;
    ref_assign(dsp, systemdict);
}

/* Free all resources and return. */
int
gs_main_finit(gs_main_instance * minst, int exit_status, int env_code)
{
    i_ctx_t *i_ctx_p = minst->i_ctx_p;
    gs_dual_memory_t dmem = {0};
    int exit_code;
    ref error_object;
    char *tempnames = NULL;
    gs_lib_ctx_core_t *core;

    /* NB: need to free gs_name_table
     */

    /*
     * Previous versions of this code closed the devices in the
     * device list here.  Since these devices are now prototypes,
     * they cannot be opened, so they do not need to be closed;
     * alloc_restore_all will close dynamically allocated devices.
     */
    tempnames = gs_main_tempnames(minst);

    /* by the time we get here, we *must* avoid any random redefinitions of
     * operators etc, so we push systemdict onto the top of the dict stack.
     * We do this in C to avoid running into any other re-defininitions in the
     * Postscript world.
     */
    gs_finit_push_systemdict(i_ctx_p);

    /* We have to disable BGPrint before we call interp_reclaim() to prevent the
     * parent rendering thread initialising for the next page, whilst we are
     * removing objects it may want to access - for example, the I/O device table.
     * We also have to mess with the BeginPage/EndPage procs so that we don't
     * trigger a spurious extra page to be emitted.
     */
    if (minst->init_done >= 2) {
        gs_main_run_string(minst,
            "/BGPrint /GetDeviceParam .special_op \
            {{ <</BeginPage {pop} /EndPage {pop pop //false } \
              /BGPrint false /NumRenderingThreads 0>> setpagedevice} if} if \
              serverdict /.jobsavelevel get 0 eq {/quit} {/stop} ifelse \
              .systemvar exec",
            0 , &exit_code, &error_object);
    }

    /*
     * Close the "main" device, because it may need to write out
     * data before destruction. pdfwrite needs so.
     */
    if (minst->init_done >= 2) {
        int code = 0;

        if (idmemory->reclaim != 0) {
            /* In extreme error conditions, these references can persist, despite the
             * arrays themselves having been restored away.
             */
            gs_main_run_string(minst,
                "$error /dstack undef \
                 $error /estack undef \
                 $error /ostack undef \
                 serverdict /.jobsavelevel get 0 eq {/quit} {/stop} ifelse \
                 .systemvar exec",
                 0 , &exit_code, &error_object);

            ref_stack_clear(&o_stack);
            code = interp_reclaim(&minst->i_ctx_p, avm_global);

            /* We ignore gs_error_VMerror because it comes from gs_vmreclaim()
            calling context_state_load(), and we don't seem to depend on the
            missing fields. */
            if (code == gs_error_VMerror) {
                if (exit_status == 0 || exit_status == gs_error_Quit) {
                    exit_status = gs_error_VMerror;
                }
            }
            else if (code < 0) {
                ref error_name;
                if (tempnames)
                    free(tempnames);

                if (gs_errorname(i_ctx_p, code, &error_name) >= 0) {
                    char err_str[32] = {0};
                    name_string_ref(imemory, &error_name, &error_name);
                    memcpy(err_str, error_name.value.const_bytes, r_size(&error_name));
                    emprintf2(imemory, "ERROR: %s (%d) reclaiming the memory while the interpreter finalization.\n", err_str, code);
                }
                else {
                    emprintf1(imemory, "UNKNOWN ERROR %d reclaiming the memory while the interpreter finalization.\n", code);
                }
#ifdef MEMENTO
                if (Memento_squeezing() && code != gs_error_VMerror ) return gs_error_Fatal;
#endif
                return gs_error_Fatal;
            }
            i_ctx_p = minst->i_ctx_p; /* interp_reclaim could change it. */
        }

        if (i_ctx_p->pgs != NULL && i_ctx_p->pgs->device != NULL &&
            gx_device_is_null(i_ctx_p->pgs->device)) {
            /* if the job replaced the device with the nulldevice, we we need to grestore
               away that device, so the block below can properly dispense
               with the default device.
             */
            int code = gs_grestoreall(i_ctx_p->pgs);
            if (code < 0) {
                free(tempnames);
                return_error(gs_error_Fatal);
            }
        }

        if (i_ctx_p->pgs != NULL && i_ctx_p->pgs->device != NULL) {
            gx_device *pdev = i_ctx_p->pgs->device;
            const char * dname = pdev->dname;
            gs_gc_root_t dev_root;
            gs_gc_root_t *dev_root_ptr = &dev_root;
            /* There is a chance that, during the call to gs_main_run_string(), the interpreter may
             * decide to call the garbager - the device is in gc memory, and the only reference to it
             * (in the gstate) has been removed, thus it can be destroyed by the garbager.
             * Counter-intuitively, adjusting the reference count makes not difference to that.
             * Register the device as a gc 'root' so it will be implicitely marked by garbager, and
             * and thus surive until control returns here.
             */
            if (gs_register_struct_root(pdev->memory, &dev_root_ptr, (void **)&pdev, "gs_main_finit") < 0) {
                free(tempnames);
                return_error(gs_error_Fatal);
            }

            /* make sure device doesn't isn't freed by .uninstalldevice */
            rc_adjust(pdev, 1, "gs_main_finit");
            /* deactivate the device just before we close it for the last time */
            gs_main_run_string(minst,
                /* we need to do the 'quit' so we don't loop for input (double quit) */
                ".uninstallpagedevice serverdict \
                /.jobsavelevel get 0 eq {/quit} {/stop} ifelse .systemvar exec",
                0 , &exit_code, &error_object);
            code = gs_closedevice(pdev);
            if (code < 0) {
                ref error_name;
                if (gs_errorname(i_ctx_p, code, &error_name) >= 0) {
                    char err_str[32] = {0};
                    name_string_ref(imemory, &error_name, &error_name);
                    memcpy(err_str, error_name.value.const_bytes, r_size(&error_name));
                    emprintf3(imemory, "ERROR: %s (%d) on closing %s device.\n", err_str, code, dname);
                }
                else {
                    emprintf2(imemory, "UNKNOWN ERROR %d closing %s device.\n", code, dname);
               }
            }
            gs_unregister_root(pdev->memory, dev_root_ptr, "gs_main_finit");
            rc_decrement(pdev, "gs_main_finit");                /* device might be freed */
            if (exit_status == 0 || exit_status == gs_error_Quit)
                exit_status = code;
        }

      /* Flush stdout and stderr */
      gs_main_run_string(minst,
        "(%stdout) (w) file closefile (%stderr) (w) file closefile \
        serverdict /.jobsavelevel get 0 eq {/quit} {/stop} ifelse .systemexec \
          systemdict /savedinitialgstate .forceundef",
        0 , &exit_code, &error_object);
    }
    gp_readline_finit(minst->readline_data);
    gs_free_object(minst->heap, minst->saved_pages_initial_arg, "gs_main_finit");
    i_ctx_p = minst->i_ctx_p;		/* get current interp context */
    if (gs_debug_c(':')) {
        print_resource_usage(minst, i_ctx_p ? &gs_imemory : NULL, "Final");
        dmprintf1(minst->heap, "%% Exiting instance "PRI_INTPTR"\n", (intptr_t)minst);
    }
    /* Do the equivalent of a restore "past the bottom". */
    /* This will release all memory, close all open files, etc. */
    if (minst->init_done >= 1) {
        gs_memory_t *mem_raw = i_ctx_p->memory.current->non_gc_memory;
        i_plugin_holder *h = i_ctx_p->plugin_list;

        dmem = *idmemory;
        env_code = alloc_restore_all(i_ctx_p);
        if (env_code < 0)
            emprintf1(mem_raw,
                      "ERROR %d while the final restore. See gs/psi/ierrors.h for code explanation.\n",
                      env_code);
        i_iodev_finit(&dmem);
        i_plugin_finit(mem_raw, h);
    }

    /* clean up redirected stdout */
    core = minst->heap->gs_lib_ctx->core;
    if (core->fstdout2
        && (gp_get_file(core->fstdout2) != core->fstdout)
        && (gp_get_file(core->fstdout2) != core->fstderr)) {
        gp_fclose(core->fstdout2);
        core->fstdout2 = NULL;
    }

    minst->heap->gs_lib_ctx->core->stdout_is_redirected = 0;
    minst->heap->gs_lib_ctx->core->stdout_to_stderr = 0;
    /* remove any temporary files, after ghostscript has closed files */
    if (tempnames) {
        char *p = tempnames;
        while (*p) {
            gp_unlink(minst->heap, p);
            p += strlen(p) + 1;
        }
        free(tempnames);
    }
    gs_lib_finit(exit_status, env_code, minst->heap);

    set_lib_path_length(minst, 0);
    gs_free_object(minst->heap, minst->lib_path.container.value.refs, "lib_path array");
    if (minst->init_done == 0 && i_ctx_p) {
        /* This fixes leak if memento forces failure in gs_main_init1(). */
        dmem = *idmemory;
    }
    ialloc_finit(&dmem);
    return exit_status;
}
int
gs_to_exit_with_code(const gs_memory_t *mem, int exit_status, int code)
{
    return gs_main_finit(get_minst_from_memory(mem), exit_status, code);
}
int
gs_to_exit(const gs_memory_t *mem, int exit_status)
{
    return gs_to_exit_with_code(mem, exit_status, 0);
}
void
gs_abort(const gs_memory_t *mem)
{
    /* In previous versions, we tried to do a cleanup (using gs_to_exit),
     * but more often than not, that will trip another abort and create
     * an infinite recursion. So just abort without trying to cleanup.
     */
    gp_do_exit(1);
}

/* ------ Debugging ------ */

/* Print resource usage statistics. */
void
print_resource_usage(const gs_main_instance * minst, gs_dual_memory_t * dmem,
                     const char *msg)
{
    ulong used = 0;		/* this we accumulate for the PS memories */
    long utime[2];
    int i;
    gs_memory_status_t status = { 0 };

    gp_get_realtime(utime);

    if (dmem)
    {
        for (i = 0; i < countof(dmem->spaces_indexed); ++i) {
            gs_ref_memory_t *mem = dmem->spaces_indexed[i];

            if (mem != 0 && (i == 0 || mem != dmem->spaces_indexed[i - 1])) {
                gs_ref_memory_t *mem_stable =
                    (gs_ref_memory_t *)gs_memory_stable((gs_memory_t *)mem);

                gs_memory_status((gs_memory_t *)mem, &status);
                used += status.used;
                if (mem_stable != mem) {
                    gs_memory_status((gs_memory_t *)mem_stable, &status);
                    used += status.used;
                }
            }
        }
    }
    /* Now get the overall values from the heap memory */
    gs_memory_status(minst->heap, &status);
    dmprintf5(minst->heap, "%% %s time = %g, memory allocated = %lu, used = %lu, max_used = %lu\n",
              msg, utime[0] - minst->base_time[0] +
              (utime[1] - minst->base_time[1]) / 1000000000.0,
              status.allocated, used, status.max_used);
}

/* Dump the stacks after interpretation */
void
gs_main_dump_stack(gs_main_instance *minst, int code, ref * perror_object)
{
    i_ctx_t *i_ctx_p = minst->i_ctx_p;

    zflush(i_ctx_p);            /* force out buffered output */
    dmprintf1(minst->heap, "\nUnexpected interpreter error %d.\n", code);
    if (perror_object != 0) {
        dmputs(minst->heap, "Error object: ");
        debug_print_ref(minst->heap, perror_object);
        dmputc(minst->heap, '\n');
    }
    debug_dump_stack(minst->heap, &o_stack, "Operand stack");
    debug_dump_stack(minst->heap, &e_stack, "Execution stack");
    debug_dump_stack(minst->heap, &d_stack, "Dictionary stack");
}

int
gs_main_force_resolutions(gs_main_instance * minst, const float *resolutions)
{
    ref value;
    int code;

    if (resolutions == NULL)
        return 0;

    if (minst == NULL)
        return gs_error_Fatal;

    make_true(&value);
    code = i_initial_enter_name(minst->i_ctx_p, "FIXEDRESOLUTION", &value);
    if (code < 0)
        return code;
    make_real(&value, resolutions[0]);
    code = i_initial_enter_name(minst->i_ctx_p, "DEVICEXRESOLUTION", &value);
    if (code < 0)
        return code;
    make_real(&value, resolutions[1]);
    return i_initial_enter_name(minst->i_ctx_p, "DEVICEYRESOLUTION", &value);
}

int
gs_main_force_dimensions(gs_main_instance *minst, const long *dimensions)
{
    ref value;
    int code = 0;

    if (dimensions == NULL)
        return 0;
    if (minst == NULL)
        return gs_error_Fatal;

    make_true(&value);
    code = i_initial_enter_name(minst->i_ctx_p, "FIXEDMEDIA", &value);
    if (code < 0)
        return code;
    make_int(&value, dimensions[0]);
    code = i_initial_enter_name(minst->i_ctx_p, "DEVICEWIDTH", &value);
    if (code < 0)
        return code;
    make_int(&value, dimensions[1]);
    return i_initial_enter_name(minst->i_ctx_p, "DEVICEHEIGHT", &value);
}
