/* Copyright (C) 2001-2021 Artifex Software, Inc.
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


/* plmain.c */
/* Main program command-line interpreter for PCL interpreters */
#include "std.h"
#include "gserrors.h"
#include "ctype_.h"
#include "string_.h"
#include <stdlib.h> /* atof */
#include "assert_.h"
#include "gdebug.h"
#include "gscdefs.h"
#include "gsio.h"
#include "gstypes.h"
#include "gserrors.h"
#include "gsmemory.h"
#include "gsmalloc.h"
#include "gsmchunk.h"
#include "gsstruct.h"
#include "gxalloc.h"
#include "gsalloc.h"
#include "gsargs.h"
#include "gp.h"
#include "gsdevice.h"
#include "gxdevice.h"
#include "gxdevsop.h"       /* for gxdso_* */
#include "gxclpage.h"
#include "gdevprn.h"
#include "gsparam.h"
#include "gslib.h"
#include "pjtop.h"
#include "plparse.h"
#include "plmain.h"
#include "pltop.h"
#include "plapi.h"
#include "gslibctx.h"
#include "gsicc_manage.h"
#include "gxiodev.h"
#include "stream.h"
#include "strmio.h"
#include "gp.h"

/* includes for the display device */
#include "gdevdevn.h"
#include "gsequivc.h"
#include "gdevdsp.h"
#include "gdevdsp2.h"

#define OMIT_SAVED_PAGES
#define OMIT_SAVED_PAGES_TEST

/* if we are not doing saved_pages, omit saved-pages-test mode as well */
#ifdef OMIT_SAVED_PAGES
#   ifndef OMIT_SAVED_PAGES_TEST
#       define OMIT_SAVED_PAGES_TEST
#   endif
#endif
/* Include the extern for the device list. */
extern_gs_lib_device_list();

/* Extern for PDL(s): currently in one of: plimpl.c (XL & PCL), */
/* pcimpl.c (PCL only), or pximpl (XL only) depending on make configuration.*/
extern pl_interp_implementation_t *pdl_implementations[];    /* zero-terminated list */

#define _STRING_IT(s) #s
#define STRING_IT(s) _STRING_IT(s)
#define PJL_VERSION_STRING STRING_IT(PJLVERSION)

/* Define the usage message. */
static const char *pl_usage = "\
Usage: %s [option* file]+...\n\
Options: -dNOPAUSE -E[#] -h -L<PCL|PCLXL> -K<maxK> -l<PCL5C|PCL5E|RTL> -Z...\n\
         -sDEVICE=<dev> -g<W>x<H> -r<X>[x<Y>] -d{First|Last}Page=<#>\n\
         -H<l>x<b>x<r>x<t> -dNOCACHE\n\
         -sOutputFile=<file> (-s<option>=<string> | -d<option>[=<value>])*\n\
         -J<PJL commands>\n";

/* Simple structure to hold the contents of a buffered file.
 * We have a list of chunks of data held in an index. */
typedef struct
{
    gs_memory_t *memory;
    size_t index_max; /* Size of the index */
    size_t len;
    byte **index;
} buffered_file;

/* Work in 1 meg chunks for now. */
#define BUFFERED_FILE_CHUNK_SHIFT 20
#define BUFFERED_FILE_CHUNK_SIZE (1<<BUFFERED_FILE_CHUNK_SHIFT)

typedef enum {
    /* Default: Never reset resources automatically on job changes. */
    PL_RESET_RESOURCES_NEVER = 0,

    /* Reset resources whenver we drop back to PJL. */
    PL_RESET_RESOURCES_ON_PJL = 1,

    /* Reset resources whenever we change language (other than to drop
     * back to PJL). */
    PL_RESET_RESOURCES_ON_LANGUAGE_CHANGE = 2
} pl_resource_reset;

/*
 * Main instance for all interpreters.
 */
struct pl_main_instance_s
{
    /* The following are set at initialization time. */
    gs_memory_t *memory;
    gs_memory_t *device_memory;
    long base_time[2];          /* starting time */
    int error_report;           /* -E# */
    bool pause;                 /* -dNOPAUSE => false */
    gx_device *device;
    gs_gc_root_t *device_root;
    pl_main_get_codepoint_t *get_codepoint;
                                /* Get next 'unicode' codepoint */

    pl_interp_implementation_t *implementation; /*-L<Language>*/

    char pcl_personality[6];    /* a character string to set pcl's
                                   personality - rtl, pcl5c, pcl5e, and
                                   pcl == default.  NB doesn't belong here. */
    bool interpolate;
    bool nocache;
    bool page_set_on_command_line;
    long page_size[2];
    bool res_set_on_command_line;
    float res[2];
    bool high_level_device;
#ifndef OMIT_SAVED_PAGES_TEST
    bool saved_pages_test_mode;
#endif
    bool pjl_from_args; /* pjl was passed on the command line */
    int scanconverter;
    /* we have to store these in the main instance until the languages
       state is sufficiently initialized to set the parameters. */
    char *piccdir;
    char *pdefault_gray_icc;
    char *pdefault_rgb_icc;
    char *pdefault_cmyk_icc;
    gs_c_param_list params;
    arg_list args;
    pl_interp_implementation_t **implementations;
    pl_interp_implementation_t *curr_implementation;

    /* We keep track of the last (non PJL) implementation we used.
     * This is used to conditionally reset soft fonts etc. */
    pl_interp_implementation_t *prev_non_pjl_implementation;

    pl_resource_reset reset_resources;

    /* When processing data via 'run_string', interpreters may not
     * completely consume the data they are passed each time. We use
     * this buffer to carry over data between calls. */
    byte *buf_ptr;
    int buf_fill;
    int buf_max;

    void *display; /* display device pointer - to support legacy API. Will
                    * be removed. */
    bool detect_runstring_language;
    bool revert_to_pjl_after_runstring;

    bool mid_runstring; /* True if we are mid-runstring */

    void *buffering_runstring_as_file;

    /* The state for gsapi param enumeration */
    gs_c_param_list enum_params;
    gs_param_enumerator_t enum_iter;
    char *enum_keybuf;
    int enum_keybuf_max;
};


/* ---------------- Forward decls ------------------ */

static int pl_main_languages_init(gs_memory_t * mem, pl_main_instance_t * inst);

static pl_interp_implementation_t
    *pl_select_implementation(pl_interp_implementation_t * pjl_instance,
                              pl_main_instance_t * pmi,
                              const char *data,
                              int len);

/* Process the options on the command line. */
static gp_file *pl_main_arg_fopen(const char *fname, void *mem);

/* Process the options on the command line, including making the
   initial device and setting its parameters.  */
static int pl_main_process_options(pl_main_instance_t * pmi, arg_list * pal,
                                   pl_interp_implementation_t * pjl_instance);

static pl_interp_implementation_t *pl_pjl_select(pl_main_instance_t * pmi, pl_interp_implementation_t *pjl_instance);

/* return index in gs device list -1 if not found */
static inline int
get_device_index(const gs_memory_t * mem, const char *value)
{
    const gx_device *const *dev_list;
    int num_devs = gs_lib_device_list(&dev_list, NULL);
    int di;

    for (di = 0; di < num_devs; ++di)
        if (!strcmp(gs_devicename(dev_list[di]), value))
            break;
    if (di == num_devs) {
        errprintf(mem, "Unknown device name %s.\n", value);
        return -1;
    }
    return di;
}

static int
legacy_display_callout(void *instance,
                       void *handle,
                       const char *dev_name,
                       int id,
                       int size,
                       void *data)
{
    pl_main_instance_t *inst = (pl_main_instance_t *)handle;

    if (dev_name == NULL)
        return -1;
    if (strcmp(dev_name, "display") != 0)
        return -1;

    if (id == DISPLAY_CALLOUT_GET_CALLBACK_LEGACY) {
        /* get display callbacks */
        gs_display_get_callback_t *cb = (gs_display_get_callback_t *)data;
        cb->callback = inst->display;
        return 0;
    }
    return -1;
}

int
pl_main_set_display_callback(pl_main_instance_t *inst, void *callback)
{
    int code;

    if (inst->display == NULL && callback != NULL) {
        /* First registration. */
        code = gs_lib_ctx_register_callout(inst->memory,
                                           legacy_display_callout,
                                           inst);
        if (code < 0)
            return code;
    }
    if (inst->display != NULL && callback == NULL) {
        /* Deregistered. */
        gs_lib_ctx_deregister_callout(inst->memory,
                                      legacy_display_callout,
                                      inst);
    }
    inst->display = callback;
    return 0;
}

int
pl_main_init_with_args(pl_main_instance_t *inst, int argc, char *argv[])
{
    gs_memory_t *mem = inst->memory;
    pl_interp_implementation_t *pjli;
    int code;

    gp_init();
    /* debug flags we reset this out of gs_lib_init0 which sets these
       and the allocator we want the debug setting but we do our own
       allocator */
#if defined(PACIFY_VALGRIND) && defined VALGRIND_HG_DISABLE_CHECKING
    VALGRIND_HG_DISABLE_CHECKING(gs_debug, 128);
#endif
    memset(gs_debug, 0, 128);
    gs_log_errors = 0;

    if (gs_lib_init1(mem) < 0)
        return -1;

    {
        /*
         * gs_iodev_init has to be called here (late), rather than
         * with the rest of the library init procedures, because of
         * some hacks specific to MS Windows for patching the
         * stdxxx IODevices.
         */

        if (gs_iodev_init(mem) < 0)
            return gs_error_Fatal;
    }

    gp_get_realtime(inst->base_time);

    if (arg_init(&inst->args, (const char **)argv, argc, pl_main_arg_fopen, mem,
                 inst->get_codepoint, mem) < 0)
        return gs_error_Fatal;

    /* Create PDL instances, etc */
    if (pl_main_languages_init(mem, inst) < 0) {
        return gs_error_Fatal;
    }

    inst->curr_implementation = pjli = inst->implementations[0];

    /* initialize pjl, needed for option processing. */
    if (pl_init_job(pjli, inst->device) < 0) {
        return gs_error_Fatal;
    }

    code = pl_main_process_options(inst,
                                   &inst->args,
                                   pjli);
    if (code == gs_error_invalidexit)
        code = 0; /* This is given by someone running "quit" in PS. */
    else if (argc == 1 || code < 0) {
        /* Print error verbage and return */
        int i;
        const gx_device **dev_list;
        int num_devs =
            gs_lib_device_list((const gx_device * const **)&dev_list,
                               NULL);
        if (code != gs_error_Info)
            errprintf(mem, pl_usage, argv[0]);

        errprintf(mem, "Version: %s\n", PJL_VERSION_STRING);
        errprintf(mem, "Languages:");
        for (i = 0; inst->implementations[i] != NULL; i++) {
            if (((i + 1)) % 9 == 0)
                errprintf(mem, "\n");
            errprintf(mem, " %s", pl_characteristics(inst->implementations[i])->language);
        }
        errprintf(mem, "\nDevices:");
        for (i = 0; i < num_devs; ++i) {
            if (((i + 1)) % 9 == 0)
                errprintf(mem, "\n");
            errprintf(mem, " %s", gs_devicename(dev_list[i]));
        }
        errprintf(mem, "\n");
        return code == gs_error_Info ? 0 : gs_error_Fatal;
    }
    return 0;
}

static int
revert_to_pjli(pl_main_instance_t *minst)
{
    pl_interp_implementation_t *pjli =
        minst->implementations[0];
    int code;

    /* If we're already in PJL, don't clear the state. */
    if (minst->curr_implementation == pjli)
        return 0;

    if (minst->curr_implementation) {
        code = pl_dnit_job(minst->curr_implementation);
        if (code < 0) {
            minst->curr_implementation = NULL;
            return code;
        }
    }
    minst->curr_implementation = NULL;
    /* We may want to reset the fonts. */
    if (minst->prev_non_pjl_implementation &&
        minst->reset_resources == PL_RESET_RESOURCES_ON_PJL) {
        if_debug1m('I', minst->memory, "Resetting resources (%s)\n",
                   pl_characteristics(minst->curr_implementation)->language);
        code = pl_reset(minst->prev_non_pjl_implementation, PL_RESET_RESOURCES);
        if (code < 0)
            return code;
    }
    minst->curr_implementation = pjli;

    code = pl_init_job(minst->curr_implementation, minst->device);

    return code;
}

int
pl_main_run_string_begin(pl_main_instance_t *minst)
{
    int code;

    if (minst->mid_runstring == 1) {
        dmprintf(minst->memory, "Can't begin a run_string during a run_string\n");
        return -1;
    }
    minst->mid_runstring = 1;

    /* If the current implementation is PJL (language 0) then don't
     * actually init here. Instead, just mark us as needing to auto
     * detect the language type when we get some data. */
    minst->detect_runstring_language = (minst->curr_implementation == minst->implementations[0]);
    minst->revert_to_pjl_after_runstring = minst->detect_runstring_language;
    if (minst->detect_runstring_language)
        return 0;

    code = pl_process_begin(minst->curr_implementation);

    if (code < 0)
        minst->mid_runstring = 0;

    return code;
}

static int
ensure_buf_size(pl_main_instance_t *minst, int len)
{
    int max = minst->buf_max;

    if (len < max)
        return 0;

    if (max == 0)
        max = 4096;

    while (max < len) {
        max *= 2;
        if (max < 0)
            return_error(gs_error_VMerror);
    }

    if (minst->buf_max == 0) {
        minst->buf_ptr = gs_alloc_bytes(minst->memory, max, "minst_buffer");
        if (minst->buf_ptr == NULL)
            return_error(gs_error_VMerror);
    } else {
        byte *newbuf = gs_resize_object(minst->memory, minst->buf_ptr, max, "minst_buffer");
        if (newbuf == NULL)
            return_error(gs_error_VMerror);
        minst->buf_ptr = newbuf;
    }
    minst->buf_max = max;

    return 0;
}

static
int buffer_data(pl_main_instance_t *minst, const char *data, unsigned int len)
{
    buffered_file *bf = minst->buffering_runstring_as_file;
    size_t newlen = bf->len + len;
    size_t start = bf->len;
    size_t chunk_max;
    size_t num_chunks = (bf->len + BUFFERED_FILE_CHUNK_SIZE-1)>>BUFFERED_FILE_CHUNK_SHIFT;

    if (len == 0)
         return 0;

    /* Copy anything that fits into the next chunk. */
    if (start < num_chunks<<BUFFERED_FILE_CHUNK_SHIFT) {
        size_t max = (num_chunks<<BUFFERED_FILE_CHUNK_SHIFT) - start;
        if (max > len)
            max = len;
        memcpy(&bf->index[start>>BUFFERED_FILE_CHUNK_SHIFT][start & (BUFFERED_FILE_CHUNK_SIZE-1)],
               data, max);
        bf->len += max;
        data += max;
        start += max;
        len -= (unsigned int)max;
        if (len == 0)
             return 0;
    }

    /* Do we need to extend the chunk index? */
    chunk_max = bf->index_max<<BUFFERED_FILE_CHUNK_SHIFT;
    if (newlen > chunk_max) {
        byte **new_index;
        if (chunk_max < 32*BUFFERED_FILE_CHUNK_SIZE)
            chunk_max = 32*BUFFERED_FILE_CHUNK_SIZE;
        while (newlen > chunk_max)
            chunk_max *= 2;
        chunk_max >>= BUFFERED_FILE_CHUNK_SHIFT;
        if (bf->index == NULL) {
            new_index = (byte **)gs_alloc_bytes(bf->memory,
                                                sizeof(byte *) * chunk_max,
                                                "buffered_file_index");
        } else {
            new_index = (byte **)gs_resize_object(bf->memory,
                                                  bf->index,
                                                  sizeof(byte *) * chunk_max,
                                                  "buffered_file_index");
        }
        if (new_index == NULL)
            return_error(gs_error_VMerror);
        bf->index = new_index;
        bf->index_max = chunk_max;
    }

    /* Now allocate chunks and copy in the data. */
    do {
        unsigned int max;
        byte *chunk = gs_alloc_bytes(bf->memory,
                                     BUFFERED_FILE_CHUNK_SIZE,
                                     "buffered_file_chunk");
        if (chunk == NULL)
            return_error(gs_error_VMerror);
        bf->index[start>>BUFFERED_FILE_CHUNK_SHIFT] = chunk;
        max = len;
        if (max > BUFFERED_FILE_CHUNK_SIZE)
            max = BUFFERED_FILE_CHUNK_SIZE;
        memcpy(chunk, data, max);
        data += max;
        bf->len = (start += max);
        len -= max;
    } while (len);

    return 0;
}

static int
start_need_file(pl_main_instance_t *minst, const char *data, unsigned int len)
{
    buffered_file *bf = (buffered_file *)gs_alloc_bytes(minst->memory->non_gc_memory,
                                                        sizeof(*bf), "buffered_file");
    if (bf == NULL)
        return_error(gs_error_VMerror);

    memset(bf, 0, sizeof(*bf));
    bf->memory = minst->memory->non_gc_memory;
    minst->buffering_runstring_as_file = bf;

    return buffer_data(minst, data, len);
}

static void
drop_buffered_file(buffered_file *bf)
{
    size_t i;

    if (bf == NULL)
        return;

    for (i = (bf->len+BUFFERED_FILE_CHUNK_SIZE-1)>>BUFFERED_FILE_CHUNK_SHIFT; i > 0; i--)
    {
        gs_free_object(bf->memory, bf->index[i-1], "buffered_file_chunk");
    }
    gs_free_object(bf->memory, bf->index, "buffered_file_index");
    gs_free_object(bf->memory, bf, "buffered_file_index");
}

typedef struct
{
   gp_file base;
   buffered_file *bf;
   gs_offset_t pos;
} gp_file_buffered;

static int
buffered_file_getc(gp_file *gp_)
{
    gp_file_buffered *gp = (gp_file_buffered *)gp_;
    gs_offset_t pos = gp->pos;

    if (pos >= gp->bf->len)
        return -1;
    gp->pos++;

    return (int)gp->bf->index[pos>>BUFFERED_FILE_CHUNK_SHIFT][pos & ((1<<BUFFERED_FILE_CHUNK_SHIFT)-1)];
}

static int
buffered_file_read(gp_file *gp_, size_t size, unsigned int count, void *buf)
{
    gp_file_buffered *gp = (gp_file_buffered *)gp_;
    gs_offset_t pos = gp->pos;
    gs_offset_t end;
    gs_offset_t n = 0;
    size_t bytes;

    bytes = size * count;
    end = pos + bytes;

    if (end > gp->bf->len)
        end = gp->bf->len;
    if (end <= pos)
        return 0;
    if (bytes > end - pos)
        bytes = end - pos;

    while (bytes > 0) {
        gs_offset_t off = pos & (BUFFERED_FILE_CHUNK_SIZE-1);
        gs_offset_t left = BUFFERED_FILE_CHUNK_SIZE - off;
        if (left > bytes)
            left = bytes;
        memcpy(buf, &gp->bf->index[pos>>BUFFERED_FILE_CHUNK_SHIFT][off],
               left);
        bytes -= left;
        n += left;
    }
    gp->pos += n;

    return n/size;
}

static int
buffered_file_seek(gp_file *gp_, gs_offset_t offset, int whence)
{
    gp_file_buffered *gp = (gp_file_buffered *)gp_;
    gs_offset_t pos;

    if (whence == SEEK_END)
        pos = gp->bf->len + offset;
    else if (whence == SEEK_CUR)
        pos = gp->pos + offset;
    else
        pos = offset;

    if (pos < 0)
        pos = 0;
    else if (pos > gp->bf->len)
        pos = gp->bf->len;
    gp->pos = pos;

    return 0;
}

static gs_offset_t
buffered_file_tell(gp_file *gp_)
{
    gp_file_buffered *gp = (gp_file_buffered *)gp_;

    return gp->pos;
}

static int
buffered_file_eof(gp_file *gp_)
{
    gp_file_buffered *gp = (gp_file_buffered *)gp_;

    return gp->pos == gp->bf->len;
}

static int
buffered_file_seekable(gp_file *gp)
{
    return 1;
}

static gp_file_ops_t buffered_file_ops = {
    NULL, /* close */
    buffered_file_getc,
    NULL, /* putc */
    buffered_file_read,
    NULL, /* write */
    buffered_file_seek,
    buffered_file_tell,
    buffered_file_eof,
    NULL, /* dup */
    buffered_file_seekable,
    NULL, /* pread */
    NULL, /* pwrite */
    NULL, /* is_char_buffered */
    NULL, /* fflush */
    NULL, /* ferror */
    NULL, /* get_file */
    NULL, /* clearerr */
    NULL /* reopen */
};

static int
buffered_file_fs_open_file(const gs_memory_t *mem,
                           void *secret,
                     const char *fname,
                     const char *mode,
                           gp_file **file)
{
    gp_file_buffered *gp;

    if (strcmp(fname, "gpdl_buffered_file:"))
        return 0;

    if (mode[0] != 'r')
        return_error(gs_error_invalidaccess);

    gp = (gp_file_buffered *)gp_file_alloc(mem, &buffered_file_ops,
                                           sizeof(*gp), "gp_file_buffered");
    if (gp == NULL)
        return_error(gs_error_VMerror); /* Allocation failed */

    /* Setup the crypt-specific state */
    gp->bf = (buffered_file *)secret;

    /* Return the new instance */
    *file = &gp->base;

    return 0;
}

static gsapi_fs_t buffered_file_fs = {
    buffered_file_fs_open_file,
    NULL,
    NULL,
    NULL,
    NULL
};

static int
do_run_string_continue(pl_main_instance_t *minst, const char *str, unsigned int length)
{
    stream_cursor_read cursor;
    int code;
    pl_interp_implementation_t *pjli = minst->implementations[0];
    const byte *initial_pos;
    int unread;

    if (minst->buffering_runstring_as_file)
        return buffer_data(minst, str, length);

    /* First off, get all our data into a single buffer (both anything
     * buffered from previous calls, and anything we have now). */
    if (minst->buf_fill > 0) {
        /* Ensure we have a buffer large enough. */
        code = ensure_buf_size(minst, length + minst->buf_fill);
        if (code < 0)
            return code;

        memcpy(&minst->buf_ptr[minst->buf_fill], str, length);
        minst->buf_fill += length;
        stream_cursor_read_init(&cursor, (const byte *)minst->buf_ptr, length);
    } else {
        /* Use the callers buffer directly. */
        stream_cursor_read_init(&cursor, (const byte *)str, length);
    }

    /* Now process that buffer. The outside loop here is used as we
     * try different languages. */
    do {
        /* Remember where we started this attempt so we can spot us
         * failing to make progress. */
        initial_pos = cursor.ptr;

        if (minst->detect_runstring_language) {
            pl_interp_implementation_t *desired_implementation;

            minst->detect_runstring_language = 0;

            desired_implementation = pl_select_implementation(pjli,
                                                              minst,
                                                (const char *)cursor.ptr+1,
                                                         (int)length);

            /* Possibly this never happens? But attempt to cope anyway. */
            if (desired_implementation == NULL) {
                code = -1;
                break;
            }
            if (gs_debug_c('I') || gs_debug_c(':'))
                dmlprintf1(minst->memory, "PDL detected as %s\n",
                           pl_characteristics(desired_implementation)->language);

            /* If the language implementation needs changing, change it. */
            if (desired_implementation != pjli) {
                code = pl_init_job(desired_implementation, minst->device);
                minst->curr_implementation = desired_implementation;
                if (code < 0)
                    return code;
            }
            code = pl_process_begin(minst->curr_implementation);
            if (code < 0)
                return code;
        }

        code = pl_process(minst->curr_implementation, &cursor);
        if (code == gs_error_NeedFile) {
            code = start_need_file(minst, (const char *)cursor.ptr + 1, cursor.limit - cursor.ptr);
            cursor.ptr = cursor.limit;
            break;
        }
        if (code == gs_error_NeedInput) {
            code = 0;
            break;
        }
        if (code == 0 && cursor.ptr != cursor.limit) {
            code = gs_error_InterpreterExit;
        }
        if (code == gs_error_InterpreterExit) {
            if (minst->curr_implementation == pjli) {
                /* We might have just hit a ENTER LANGUAGE. */
                /* Ideally, we should look at what language ENTER LANGUAGE
                 * said to use, and then swap to that. That means looking up
                 * a variable in the pjl interpreter, and we don't have the
                 * means to do that at the moment. For now let's assume it
                 * says "auto" and let's process it based on that. */
                minst->detect_runstring_language = 1;
            } else {
                /* The interpreter itself has hit UEL. Automatically guess
                 * what comes next. Probably back to PJL. */
                minst->detect_runstring_language = 1;
                if (minst->revert_to_pjl_after_runstring) {
                    code = revert_to_pjli(minst);
                    if (code < 0)
                        break;
                }
            }
            code = 0;
        }
        /* If we have data left, and we made progress last time,
         * head back to the top to try again with the remaining
         * data in the buffer. */
    } while (code >= 0 && cursor.ptr != initial_pos);

    /* Now, deal with any unread data. */
    unread = (int)(cursor.limit - cursor.ptr);
    if (unread) {
        /* We used count bytes, but still have unread bytes left. */
        if (minst->buf_fill > 0) {
            /* Move the remaining data down our buffer. */
            int count = (int)(cursor.ptr + 1 - minst->buf_ptr);
            memmove(&minst->buf_ptr[0], &minst->buf_ptr[count],
                    unread);
        } else {
            /* Store the remaining data in the buffer. */
            int count = (int)(cursor.ptr + 1 - (const byte *)str);
            int code2 = ensure_buf_size(minst, unread);
            if (code2 < 0)
                return code < 0 ? code : code2;
            memcpy(&minst->buf_ptr[0], str + count, unread);
        }
    }
    minst->buf_fill = (int)unread;

    return code;
}

int
pl_main_run_string_continue(pl_main_instance_t *minst, const char *str, unsigned int length)
{
    int code;

    code = do_run_string_continue(minst, str, length);
    if (code < 0)
        minst->mid_runstring = 0;

    return code;
}

static int
pl_main_run_file_utf8(pl_main_instance_t *minst, const char *prefix_commands, const char *filename)
{
    bool new_job = true;
    pl_interp_implementation_t *pjli =
        minst->implementations[0];
    gs_memory_t *mem = minst->memory;
    stream *s;
    int code = 0;
    bool is_stdin = filename[0] == '-' && filename[1] == 0;
    bool use_process_file = false;
    bool first_job = true;
    pl_interp_implementation_t *desired_implementation = NULL;

    if (is_stdin) {
        code = gs_get_callout_stdin(&s, mem);
        if (code < 0)
            return code;
    } else {
        s = sfopen(filename, "r", mem);
    }
    if (s == NULL)
        return gs_error_undefinedfilename;

    /* This function can run in 2 modes. Either it can run a file directly
     * using the run_file mechanism, or it can feed the data piecemeal
     * using the run_string mechanism. Which one depends on several things:
     *
     * If we're being piped data, then we have to use run_string.
     * If we are entered (as is usually the case) with PJL as the selected
     * interpreter, then we do a quick assessment of the file contents to
     * pick an interpreter. If the first interpreter has a run_file method
     * then we'll use that.
     *
     * This means that files that start with PJL data will always be run
     * using run_string.
     */

    for (;;) {
        if_debug1m('I', mem, "[i][file pos=%ld]\n",
                   sftell(s));

        /* Check for EOF and prepare the next block of data. */
        if (s->cursor.r.ptr == s->cursor.r.limit && sfeof(s)) {
            if_debug0m('I', mem, "End of of data\n");
            if (pl_process_end(minst->curr_implementation) < 0)
                 goto error_fatal;
            pl_process_eof(minst->curr_implementation);
            if (revert_to_pjli(minst) < 0)
                goto error_fatal_reverted;
            break;
        }
        code = s_process_read_buf(s);
        if (code < 0)
            break;
        if (s->end_status == ERRC)
            break;

        if (new_job) {
            /* The only time the current implementation won't be PJL,
             * is if someone has preselected a particular language
             * before calling this function. */
            if (minst->curr_implementation == pjli) {
                /* Autodetect the language based on the content. */
                desired_implementation = pl_select_implementation(
                                                    pjli,
                                                    minst,
                                      (const char *)s->cursor.r.ptr+1,
                                               (int)(s->cursor.r.limit -
                                                     s->cursor.r.ptr));

                /* Possibly this never happens? But attempt to cope anyway. */
                if (desired_implementation == NULL)
                    goto flush_to_end_of_job;
                if (gs_debug_c('I') || gs_debug_c(':'))
                    dmlprintf1(mem, "PDL detected as %s\n",
                               pl_characteristics(desired_implementation)->language);

                /* If the language implementation needs changing, change it. */
                if (desired_implementation != pjli) {

                    /* If we are being asked to swap to a language implementation
                     * that is different to the last (non-PJL) implementation that
                     * we were using, we may want to clear soft-fonts. */
                    if (minst->prev_non_pjl_implementation &&
                        minst->reset_resources == PL_RESET_RESOURCES_ON_LANGUAGE_CHANGE &&
                        desired_implementation != minst->prev_non_pjl_implementation) {
                        if_debug1m('I', mem, "Resetting resources (%s)\n",
                                   pl_characteristics(minst->curr_implementation)->language);
                        code = pl_reset(minst->prev_non_pjl_implementation, PL_RESET_RESOURCES);
                        if (code < 0)
                            goto error_fatal;
                    }

                    code = pl_dnit_job(pjli);
                    minst->curr_implementation = NULL;
                    if (code >= 0)
                        code = pl_init_job(desired_implementation, minst->device);
                    minst->curr_implementation = desired_implementation;
                    minst->prev_non_pjl_implementation = desired_implementation;
                    if (code < 0)
                        goto error_fatal;
                }

                /* Run any prefix commands */
                code = pl_run_prefix_commands(minst->curr_implementation, prefix_commands);
                if (code < 0)
                    goto error_fatal;
                prefix_commands = NULL;
            }

            if (minst->curr_implementation != pjli) {
                if_debug1m('I', mem, "initialised (%s)\n",
                           pl_characteristics(minst->curr_implementation)->language);
                if (first_job &&
                    !is_stdin &&
                    minst->curr_implementation->proc_process_file) {
                    /* If we aren't being piped data, and this interpreter
                     * is capable of coping with running a file directly,
                     * let's do that. */
                    use_process_file = true;
                    break;
                }
            }

            first_job = false;
            new_job = false;

            if (pl_process_begin(minst->curr_implementation) < 0)
                 goto error_fatal;
        }

        if_debug2m('I', mem, "processing (%s) job from offset %ld\n",
                   pl_characteristics(minst->curr_implementation)->language,
                   sftell(s));
        code = pl_process(minst->curr_implementation, &s->cursor.r);
        if_debug2m('I', mem, "processed (%s) job to offset %ld\n",
                   pl_characteristics(minst->curr_implementation)->language,
                   sftell(s));
        if (code == gs_error_NeedInput || code >= 0) {
            continue;
        }
        if (code != e_ExitLanguage) {
            /* error and not exit language */
            dmprintf1(mem,
                      "Warning interpreter exited with error code %d\n",
                      code);
flush_to_end_of_job:
            dmprintf(mem, "Flushing to end of job\n");
            /* flush eoj may require more data */
            while ((pl_flush_to_eoj(minst->curr_implementation, &s->cursor.r)) == 0) {
                int code2;
                if (s->cursor.r.ptr == s->cursor.r.limit && sfeof(s)) {
                    if_debug0m('I', mem,
                               "end of data found while flushing\n");
                    break;
                }
                code2 = s_process_read_buf(s);
                if (code2 < 0)
                    break;
            }
            pl_report_errors(minst->curr_implementation, code,
                             sftell(s),
                             minst->error_report > 0);
        }

        if (pl_process_end(minst->curr_implementation) < 0)
            goto error_fatal;
        new_job = true;
        /* Always revert to PJL after each job. We avoid reinitialising PJL
         * if we are already in PJL to avoid clearing the state. */
        if (revert_to_pjli(minst))
            goto error_fatal_reverted;
    }
    sfclose(s);
    s = NULL;
    if (use_process_file)
    {
        if_debug1m('I', mem, "processing job from file (%s)\n",
                   filename);

        code = pl_process_file(minst->curr_implementation, (char *)filename);
        if (code == gs_error_InterpreterExit)
            code = 0;
        if (code < 0) {
            errprintf(mem, "Warning interpreter exited with error code %d\n",
                      code);
        }
    }
    if (revert_to_pjli(minst) < 0)
        goto error_fatal_reverted;
    return 0;

error_fatal:
    (void)revert_to_pjli(minst);
error_fatal_reverted:
    sfclose(s);
    return gs_error_Fatal;
}

int
pl_main_run_string_end(pl_main_instance_t *minst)
{
    int code;

    if (minst->buffering_runstring_as_file) {
        buffered_file *bf = minst->buffering_runstring_as_file;
        minst->buffering_runstring_as_file = NULL;
        code = gsapi_add_fs(minst, &buffered_file_fs, bf);
        if (code >= 0) {
            code = pl_process_end(minst->curr_implementation);
            if (code >= 0)
                code = pl_process_file(minst->curr_implementation,
                                       "gpdl_buffered_file:");
            gsapi_remove_fs(minst, &buffered_file_fs, bf);
        }
        drop_buffered_file(bf);
    } else {
        code = pl_process_end(minst->curr_implementation);

        if (code >= 0)
            code = pl_process_eof(minst->curr_implementation);

        if (minst->buf_fill != 0)
            code = gs_note_error(gs_error_syntaxerror);
        minst->buf_fill = 0;
    }

    if (minst->revert_to_pjl_after_runstring) {
        int code2 = revert_to_pjli(minst);
        if (code2 < 0)
            code = code < 0 ? code : code2;
        else
            minst->revert_to_pjl_after_runstring = 0;
    }

    minst->mid_runstring = 0;

    return code;
}

/* We assume that the desired language has been set as minst->implementation here. */
static int
pl_main_run_prefix(pl_main_instance_t *minst, const char *prefix_commands)
{
    int code;

    if (minst->curr_implementation != minst->implementation) {
        code = pl_dnit_job(minst->curr_implementation);
        minst->curr_implementation = NULL;
        if (code < 0)
            return code;
    }
    code = pl_init_job(minst->implementation, minst->device);
    if (code < 0)
        return code;
    minst->curr_implementation = minst->implementation;

    code = pl_run_prefix_commands(minst->curr_implementation, prefix_commands);

    return code;
}

void
pl_main_set_arg_decode(pl_main_instance_t *minst,
                       pl_main_get_codepoint_t *get_codepoint)
{
    if (minst == NULL)
        return;

    minst->get_codepoint = get_codepoint;
}

int
pl_main_run_file(pl_main_instance_t *minst, const char *file_name)
{
    char *d, *temp;
    const char *c = file_name;
    char dummy[6];
    int rune, code, len;

    if (minst == NULL)
        return 0;

    if (minst->mid_runstring == 1) {
        dmprintf(minst->memory, "Can't run_file during a run_string\n");
        return -1;
    }

    /* Convert the file_name to utf8 */
    if (minst->get_codepoint) {
        len = 1;
        while ((rune = minst->get_codepoint(NULL, &c)) >= 0)
            len += codepoint_to_utf8(dummy, rune);
        temp = (char *)gs_alloc_bytes_immovable(minst->memory, len, "gsapi_run_file");
        if (temp == NULL)
            return gs_error_VMerror;
        c = file_name;
        d = temp;
        while ((rune = minst->get_codepoint(NULL, &c)) >= 0)
           d += codepoint_to_utf8(d, rune);
        *d = 0;
    }
    else {
      temp = (char *)file_name;
    }
    code = pl_main_run_file_utf8(minst, NULL, temp);
    if (temp != file_name)
        gs_free_object(minst->memory, temp, "gsapi_run_file");
    return code;
}

int
pl_main_delete_instance(pl_main_instance_t *minst)
{
    /* Dnit PDLs */
    gs_memory_t *mem;
    pl_interp_implementation_t **impl;

    if (minst == NULL)
        return 0;

    /* close and deallocate the device */
    if (minst->device) {
        gs_closedevice(minst->device);
        if (minst->device_root) {
            gs_unregister_root(minst->device->memory, minst->device_root,
                               "pl_main_languages_delete_instance");
        }
        minst->device_root = NULL;
        gx_device_retain(minst->device, false);
        minst->device = NULL;
    }
    mem = minst->memory;
    impl = minst->implementations;
    if (impl != NULL) {
        /* dnit interps */
        int index;
        for (index = 0; impl[index] != 0; ++index) {
            if (pl_deallocate_interp_instance(impl[index]) < 0) {
                return -1;
            }
            gs_free_object(mem, impl[index], "pl_main_languages_init interp");
        }

        gs_free_object(mem, impl, "pl_main_languages_delete_instance()");
    }

    drop_buffered_file(minst->buffering_runstring_as_file);

    gs_free_object(mem, minst->buf_ptr, "minst_buffer");

    gs_c_param_list_release(&minst->enum_params);
    gs_free_object(mem, minst->enum_keybuf, "param enumerator keybuf");

    gs_iodev_finit(mem);
    gs_lib_finit(0, 0, mem);
    gs_free_object(mem, minst, "pl_main_instance");
    mem->gs_lib_ctx->top_of_system = NULL;

#ifdef PL_LEAK_CHECK
    gs_memory_chunk_dump_memory(mem);
#endif
    gs_malloc_release(gs_memory_chunk_unwrap(mem));

    return 0;
}

int
pl_to_exit(gs_memory_t *mem)
{
    int ret = 0;
    pl_main_instance_t *minst = mem->gs_lib_ctx->top_of_system;
    /* Deselect last-selected device */
    if (minst->curr_implementation
        && pl_dnit_job(minst->curr_implementation) < 0) {
        ret = -1;
    }

    gs_c_param_list_release(&minst->params);
    arg_finit(&minst->args);
    return ret;
}

static int                             /* 0 ok, else -1 error */
pl_main_languages_init(gs_memory_t * mem,        /* deallocator for devices */
                      pl_main_instance_t * minst         /* instance for pre/post print */
    )
{
    int index;
    int count;
    int sz;
    pl_interp_implementation_t **impls;

    for (count = 0; pdl_implementations[count] != 0; ++count)
        ;

    /* add one so we can zero terminate the table */
    sz = sizeof(pl_interp_implementation_t *) * (count + 1);

    impls = (pl_interp_implementation_t **)gs_alloc_bytes_immovable(mem, sz, "pl_main_languages_init");

    if (impls == NULL)
        goto pmui_err;

    minst->implementations = impls;
    minst->curr_implementation = NULL;
    memset(impls, 0, sz);

    /* Create & init PDL all instances. Could do this lazily to save memory, */
    /* but for now it's simpler to just create all instances up front. */
    for (index = 0; index < count; ++index) {
        int code;
        impls[index] = (pl_interp_implementation_t *)
            gs_alloc_bytes_immovable(mem,
                                     sizeof(pl_interp_implementation_t),
                                     "pl_main_languages_init interp");

        if (impls[index] == NULL)
            goto pmui_err;

        *impls[index] = *pdl_implementations[index];

        /* Whatever instance we allocate here will become the current
         * instance during initialisation; this allows init files to be
         * successfully read etc. */
        code = pl_allocate_interp_instance(impls[index], mem);

        if (code < 0) {
            errprintf(mem, "Unable to create %s interpreter.\n",
                        pl_characteristics(impls[index])->
                        language);
            gs_free_object(mem, impls[index], "pl_main_languages_init interp");
            impls[index] = NULL;
            goto pmui_err;
        }

    }

    return 0;

 pmui_err:
    return -1;
}

pl_main_instance_t *
pl_main_get_instance(const gs_memory_t *mem)
{
    return mem->gs_lib_ctx->top_of_system;
}

char *
pl_main_get_pcl_personality(const gs_memory_t *mem)
{
    return pl_main_get_instance(mem)->pcl_personality;
}

/* ------- Functions related to pl_main_instance_t ------ */

/* Initialize the instance parameters. */
pl_main_instance_t *
pl_main_alloc_instance(gs_memory_t * mem)
{
    pl_main_instance_t *minst;
    if (mem == NULL)
        return NULL;

    minst = (pl_main_instance_t *)gs_alloc_bytes_immovable(mem,
                                                           sizeof(pl_main_instance_t),
                                                           "pl_main_instance");
    if (minst == NULL)
        return 0;

    memset(minst, 0, sizeof(*minst));

    minst->memory = minst->device_memory = mem;

    minst->pjl_from_args = false;
    minst->error_report = -1;
    minst->pause = true;
    minst->device = 0;
    minst->implementation = NULL;
    minst->prev_non_pjl_implementation = NULL;
    minst->reset_resources = PL_RESET_RESOURCES_NEVER;
    minst->base_time[0] = 0;
    minst->base_time[1] = 0;
    minst->interpolate = false;
    minst->nocache = false;
    minst->page_set_on_command_line = false;
    minst->res_set_on_command_line = false;
    minst->high_level_device = false;
#ifndef OMIT_SAVED_PAGES_TEST
    minst->saved_pages_test_mode = false;
#endif
    minst->scanconverter = GS_SCANCONVERTER_DEFAULT;
    minst->piccdir = NULL;
    minst->pdefault_gray_icc = NULL;
    minst->pdefault_rgb_icc = NULL;
    minst->pdefault_cmyk_icc = NULL;
    strncpy(&minst->pcl_personality[0], "PCL",
            sizeof(minst->pcl_personality) - 1);
    mem->gs_lib_ctx->top_of_system = minst;
    gs_c_param_list_write(&minst->params, mem);
    gs_param_list_set_persistent_keys((gs_param_list *)&minst->params, false);
    return minst;
}

/* -------- Command-line processing ------ */

/* Create a default device if not already defined. */
static int
pl_top_create_device(pl_main_instance_t * pti, int index)
{
    int code = 0;

    if (!pti->device) {
        const gx_device *dev;
        pl_interp_implementation_t **impl;
        gs_memory_t *mem = pti->device_memory;
        /* We assume that nobody else changes pti->device,
           and this function is called from this module only.
           Due to that device_root is always consistent with pti->device,
           and it is regisrtered if and only if pti->device != NULL.
         */
        if (pti->device != NULL)
            pti->device = NULL;

        if (index == -1) {
            dev = gs_getdefaultlibdevice(pti->memory);
        }
        else {
            const gx_device **list;
            gs_lib_device_list((const gx_device * const **)&list, NULL);
            dev = list[index];
        }
        for (impl = pti->implementations; *impl != 0; ++impl) {
           mem = pl_get_device_memory(*impl);
           if (mem)
               break;
        }
        if (mem)
            pti->device_memory = mem;
#ifdef DEBUG
        for (; *impl != 0; ++impl) {
            mem = pl_get_device_memory(*impl);
            assert(mem == NULL || mem == pti->device_memory);
        }
#endif
        code = gs_copydevice(&pti->device, dev, pti->device_memory);

        if (code < 0)
            return code;

        if (pti->device == NULL)
            return gs_error_VMerror;

        pti->device_root = NULL;
        code = gs_register_struct_root(pti->device_memory, &pti->device_root,
                                (void **)&pti->device,
                                "pl_top_create_device");
        if (code < 0)
            return code;

        {
            gs_c_param_list list;

            /* Check if the device is a high level device (pdfwrite etc) */
            gs_c_param_list_write(&list, pti->device->memory);
            code = gs_getdeviceparams(pti->device, (gs_param_list *)&list);
            if (code >= 0) {
                gs_c_param_list_read(&list);
                code = param_read_bool((gs_param_list *)&list, "HighLevelDevice", &pti->high_level_device);
            }
            gs_c_param_list_release(&list);
            if (code < 0)
                return code;
        }

        if (pti->device->is_open &&
            dev_proc(pti->device, dev_spec_op)(pti->device,
                                               gxdso_reopen_after_init,
                                               NULL, 0) == 1) {
            code = gs_closedevice(pti->device);
            if (code < 0)
                return code;

            code = gs_opendevice(pti->device);
            if (code < 0) {
                dmprintf(pti->device->memory,
                         "**** Unable to open the device, quitting.\n");
                return code;
            }
        }
    }
    return code;
}

/* Process the options on the command line. */
static gp_file *
pl_main_arg_fopen(const char *fname, void *data)
{
    const gs_memory_t *mem = (const gs_memory_t *)data;
    return gp_fopen(mem, fname, "r");
}

static void
set_debug_flags(const char *arg, char *flags)
{
    byte value = (*arg == '-' ? (++arg, 0) : 0xff);

    while (*arg)
        flags[*arg++ & 127] = value;
}

/*
 * scan floats delimited by spaces, tabs and/or 'x'.  Return the
 * number of arguments parsed which will be less than or equal to the
 * number of arguments requested in the parameter arg_count.
 */
static int
parse_floats(gs_memory_t * mem, uint arg_count, const char *arg, float *f)
{
    int float_index = 0;
    char *tok, *l = NULL;
    /* copy the input because strtok() steps on the string */
    char *s = arg_copy(arg, mem);
    if (s == NULL)
        return -1;

    /* allow 'x', tab or spaces to delimit arguments */
    tok = gs_strtok(s, " \tx", &l);
    while (tok != NULL && float_index < arg_count) {
        f[float_index++] = atof(tok);
        tok = gs_strtok(NULL, " \tx", &l);
    }

    gs_free_object(mem, s, "parse_floats()");

    /* return the number of args processed */
    return float_index;
}

#define argcmp(A, S, L) \
    (!strncmp(A, S, L) && (A[L] == 0 || A[L] == '='))

static int check_for_special_int(pl_main_instance_t * pmi, const char *arg, int64_t b)
{
    if (argcmp(arg, "BATCH", 5))
        return (b == 1) ? 0 : gs_note_error(gs_error_rangecheck);
    if (argcmp(arg, "NOPAUSE", 7)) {
        pmi->pause = !b;
        return 0;
    }
    if (argcmp(arg, "DOINTERPOLATE", 13)) {
        pmi->interpolate = !!b;
        return 0;
    }
    if (argcmp(arg, "NOCACHE", 7)) {
        pmi->nocache = !!b;
        return 0;
    }
    if (argcmp(arg, "SCANCONVERTERTYPE", 17)) {
        pmi->scanconverter = b;
        return 0;
    }
    if (argcmp(arg, "RESETRESOURCES", 14)) {
        pmi->reset_resources = b;
        return 0;
    }
    return 1;
}

static int check_for_special_float(pl_main_instance_t * pmi, const char *arg, float f)
{
    if (argcmp(arg, "BATCH", 5) ||
        argcmp(arg, "NOPAUSE", 7) ||
        argcmp(arg, "DOINTERPOLATE", 13) ||
        argcmp(arg, "NOCACHE", 7) ||
        argcmp(arg, "SCANCONVERTERTYPE", 17) ||
        argcmp(arg, "RESETRESOURCES", 14)) {
        return gs_note_error(gs_error_rangecheck);
    }
    return 1;
}

static int check_for_special_str(pl_main_instance_t * pmi, const char *arg, gs_param_string *f)
{
    if (argcmp(arg, "BATCH", 5) ||
        argcmp(arg, "NOPAUSE", 7) ||
        argcmp(arg, "DOINTERPOLATE", 13) ||
        argcmp(arg, "NOCACHE", 7) ||
        argcmp(arg, "SCANCONVERTERTYPE", 17) ||
        argcmp(arg, "RESETRESOURCES", 14)) {
        return gs_note_error(gs_error_rangecheck);
    }
    return 1;
}

static int
pass_param_to_languages(pl_main_instance_t *pmi,
                        gs_param_list      *plist)
{
    pl_interp_implementation_t **imp;
    int code = 0;

    for (imp = pmi->implementations; *imp != NULL; imp++) {
        code = pl_set_param(*imp, plist);
        if (code != 0)
            break;
    }

    return code;
}

static int
pass_path_to_languages(pl_main_instance_t *pmi,
                       const char         *path)
{
    pl_interp_implementation_t **imp;
    int code = 0;

    for (imp = pmi->implementations; *imp != NULL; imp++) {
        code = pl_add_path(*imp, path);
        if (code < 0)
            break;
    }

    return code;
}

static int
pl_main_post_args_init(pl_main_instance_t * pmi)
{
    pl_interp_implementation_t **imp;
    int code = 0;

    for (imp = pmi->implementations; *imp != NULL; imp++) {
        code = pl_post_args_init(*imp);
        if (code != 0)
            break;
    }

    return code;
}

static int
handle_dash_c(pl_main_instance_t *pmi, arg_list *pal, char **collected_commands, const char **arg)
{
    bool ats = pal->expand_ats;
    int code = 0;

    *arg = NULL;
    pal->expand_ats = false;
    while ((code = arg_next(pal, (const char **)arg, pmi->memory)) > 0) {
        size_t arglen;
        if ((*arg)[0] == '@' ||
            ((*arg)[0] == '-' && !isdigit((unsigned char)(*arg)[1]))
            )
            break;
        code = gs_lib_ctx_stash_sanitized_arg(pmi->memory->gs_lib_ctx, "?");
        if (code < 0) {
            goto end;
        }
        arglen = strlen(*arg);
        if (*collected_commands == NULL) {
            *collected_commands = (char *)gs_alloc_bytes(pmi->memory, arglen+1,
                                                         "-c buffer");
            if (*collected_commands == NULL) {
                code = gs_note_error(gs_error_VMerror);
                goto end;
            }
            memcpy(*collected_commands, *arg, arglen+1);
        } else {
            char *newc;
            size_t oldlen = strlen(*collected_commands);
            newc = (char *)gs_resize_object(pmi->memory,
                                            *collected_commands,
                                            oldlen + 1 + arglen + 1,
                                            "-c buffer");
            if (newc == NULL) {
                code = gs_note_error(gs_error_VMerror);
                goto end;
            }
            newc[oldlen] = 32;
            memcpy(newc + oldlen + 1, *arg, arglen + 1);
            *collected_commands = newc;
        }
        *arg = NULL;
    }

end:
    if (code == gs_error_VMerror) {
        dmprintf(pmi->memory, "Failed to allocate memory while handling -c\n");
    }
    else if (code < 0) {
        dmprintf(pmi->memory, "Syntax: -c <postscript commands>\n");
    }

    pal->expand_ats = ats;

    return code;
}

int
pl_main_set_param(pl_main_instance_t * pmi, const char *arg)
{
    /* We're setting a device parameter to a non-string value. */
    const char *eqp = strchr(arg, '=');
    const char *value;
    int64_t vi;
    float vf;
    bool bval = true;
    char buffer[128];
    static const char const_true_string[] = "true";
    int code = 0;
    gs_c_param_list *params = &pmi->params;

    if (eqp || (eqp = strchr(arg, '#')))
        value = eqp + 1;
    else {
        /* -dDefaultBooleanIs_TRUE */
        value = const_true_string;
        eqp = arg + strlen(arg);
    }

    /* Arrange for a null terminated copy of the key name in buffer. */
    if (eqp-arg >= sizeof(buffer)-1) {
        dmprintf1(pmi->memory, "Command line key is too long: %s\n", arg);
        return -1;
    }
    gs_c_param_list_write_more(params);
    strncpy(buffer, arg, eqp - arg);
    buffer[eqp - arg] = '\0';
    if (value && value[0] == '/') {
        /* We have a name! */
        gs_param_string str;

        code = check_for_special_str(pmi, arg, &str);
        if (code <= 0)
            return code;

        param_string_from_transient_string(str, value + 1);
        code = param_write_name((gs_param_list *) params,
                                buffer, &str);
    } else if (strchr(value, '#')) {
        /* We have a non-decimal 'radix' number */
        int64_t base = 0;
        const char *val = strchr(value, '#') + 1;
        const char *v = value;
        char c;

        while ((c = *v++) >= '0' && c <= '9')
            base = base*10 + (c - '0');
        if (c != '#') {
            dmprintf1(pmi->memory, "Malformed base value for radix. %s",
                      value);
            return -1;
        }

        if (base < 2 || base > 36) {
            dmprintf1(pmi->memory, "Base out of range %s",
                      value);
            return -1;
        }
        vi = 0;
        while (*val) {
            if (*val >= '0' && *val < ('0'+(base<=10?base:10))) {
                vi = vi * base + (*val - '0');
            } else if (base > 10) {
                if (*val >= 'A' && *val < 'A'+base-10) {
                    vi = vi * base + (*val - 'A' + 10);
                } else if (*val >= 'a' && *val < 'a'+base-10) {
                    vi = vi * base + (*val - 'a' + 10);
                } else {
                    dmprintf1(pmi->memory,
                              "Value out of range %s\n",
                              val);
                    return -1;
                }
            }
            val++;
        }
        code = check_for_special_int(pmi, arg, vi);
        if (code < 0) code = 0;
        if (code <= 0)
            return code;
        code = param_write_i64((gs_param_list *) params,
                               buffer, &vi);
    } else if ((!strchr(value, '.')) &&
               (sscanf(value, "%"PRId64, &vi) == 1)) {
        /* Here we have an int -- check for a scaling suffix */
        char suffix = eqp[strlen(eqp) - 1];

        switch (suffix) {
            case 'k':
            case 'K':
                vi *= 1024;
                break;
            case 'm':
            case 'M':
                vi *= 1024 * 1024;
                break;
            case 'g':
            case 'G':
                /* caveat emptor: more than 2g will overflow */
                /* and really should produce a 'real', so don't do this */
                vi *= 1024 * 1024 * 1024;
                break;
            default:
                break;  /* not a valid suffix or last char was digit */
        }
        code = check_for_special_int(pmi, arg, vi);
        if (code < 0) code = 0;
        if (code <= 0)
            return code;
        code = param_write_i64((gs_param_list *) params,
                               buffer, &vi);
    } else if (sscanf(value, "%f", &vf) == 1) {
        /* We have a float */
        code = check_for_special_float(pmi, arg, vf);
        if (code <= 0)
            return code;
        code = param_write_float((gs_param_list *) params,
                                 buffer, &vf);
    } else if (!strcmp(value, "null")) {
        code = check_for_special_int(pmi, arg, (int)bval);
        if (code < 0) code = 0;
        if (code <= 0)
            return code;
        code = param_write_null((gs_param_list *) params,
                                buffer);
    } else if (!strcmp(value, "true")) {
        /* bval = true; */
        code = check_for_special_int(pmi, arg, (int)bval);
        if (code < 0) code = 0;
        if (code <= 0)
            return code;
        code = param_write_bool((gs_param_list *) params,
                                buffer, &bval);
    } else if (!strcmp(value, "false")) {
        bval = false;
        code = check_for_special_int(pmi, arg, (int)bval);
        if (code < 0) code = 0;
        if (code <= 0)
            return code;
        code = param_write_bool((gs_param_list *) params,
                                buffer, &bval);
    } else {
        dmprintf(pmi->memory,
                 "Usage for -d is -d<option>=[<integer>|<float>|null|true|false|name]\n");
        arg = NULL;
        return 0;
    }
    if (code < 0)
        return code;
    gs_c_param_list_read(params);
    code = pass_param_to_languages(pmi, (gs_param_list *) params);

    /* If we have a device, send it to the device, and clear it from
     * the list. If we don't have a device, we leave it in the list for
     * it to be sent to device later. */
    if (pmi->device) {
        gs_c_param_list_read(params);
        code = gs_putdeviceparams(pmi->device, (gs_param_list *) params);
        gs_c_param_list_release(params);
    }

    return code;
}

int
pl_main_set_string_param(pl_main_instance_t * pmi, const char *arg)
{
    /* We're setting a device or user parameter to a string. */
    char *eqp;
    const char *value;
    gs_param_string str;
    int code = 0;
    gs_c_param_list *params = &pmi->params;

    eqp = strchr(arg, '=');
    if (!(eqp || (eqp = strchr(arg, '#')))) {
        return -1;
    }
    value = eqp + 1;
    if (!strncmp(arg, "DEVICE", 6)) {
        dmprintf(pmi->memory, "DEVICE can only be set on the command line!\n");
        return -1;
    } else if (!strncmp(arg, "DefaultGrayProfile",
                        strlen("DefaultGrayProfile"))) {
        dmprintf(pmi->memory, "DefaultGrayProfile can only be set on the command line!\n");
        return -1;
    } else if (!strncmp(arg, "DefaultRGBProfile",
                        strlen("DefaultRGBProfile"))) {
        dmprintf(pmi->memory, "DefaultRGBProfile can only be set on the command line!\n");
        return -1;
    } else if (!strncmp(arg, "DefaultCMYKProfile",
                        strlen("DefaultCMYKProfile"))) {
        dmprintf(pmi->memory, "DefaultCMYKProfile can only be set on the command line!\n");
        return -1;
    } else if (!strncmp(arg, "ICCProfileDir", strlen("ICCProfileDir"))) {
        dmprintf(pmi->memory, "ICCProfileDir can only be set on the command line!\n");
        return -1;
    } else {
        char buffer[128];

        if (eqp-arg >= sizeof(buffer)-1) {
            dmprintf1(pmi->memory, "Command line key is too long: %s\n", arg);
            return -1;
        }
        strncpy(buffer, arg, eqp - arg);
        buffer[eqp - arg] = '\0';

        gs_c_param_list_write_more(params);
        param_string_from_transient_string(str, value);
        code = param_write_string((gs_param_list *) params,
                                  buffer, &str);
        if (code < 0)
            return code;
        gs_c_param_list_read(params);
        code = pass_param_to_languages(pmi, (gs_param_list *)params);
        if (code < 0) {
            gs_c_param_list_release(params);
            return code;
        }

        if (pmi->device) {
            code = gs_putdeviceparams(pmi->device, (gs_param_list *)params);
            gs_c_param_list_release(params);
        }
    }
    return code;
}

int
pl_main_set_parsed_param(pl_main_instance_t * pmi, const char *arg)
{
    char *eqp;
    const char *value;
    char buffer[128];

    eqp = strchr(arg, '=');
    if (!(eqp || (eqp = strchr(arg, '#')))) {
        return -1;
    }
    value = eqp + 1;
    if (!strncmp(arg, "DEVICE", 6)) {
        dmprintf(pmi->memory, "DEVICE cannot be set by -p!\n");
        return -1;
    } else if (!strncmp(arg, "DefaultGrayProfile",
                        strlen("DefaultGrayProfile"))) {
        dmprintf(pmi->memory, "DefaultGrayProfile cannot be set by -p!\n");
        return -1;
    } else if (!strncmp(arg, "DefaultRGBProfile",
                        strlen("DefaultRGBProfile"))) {
        dmprintf(pmi->memory, "DefaultRGBProfile cannot be set by -p!\n");
        return -1;
    } else if (!strncmp(arg, "DefaultCMYKProfile",
                        strlen("DefaultCMYKProfile"))) {
        dmprintf(pmi->memory, "DefaultCMYKProfile cannot be set by -p!\n");
        return -1;
    } else if (!strncmp(arg, "ICCProfileDir", strlen("ICCProfileDir"))) {
        dmprintf(pmi->memory, "ICCProfileDir cannot be set by -p!\n");
        return -1;
    }

    if (eqp-arg >= sizeof(buffer)-1) {
        dmprintf1(pmi->memory, "Command line key is too long: %s\n", arg);
        return -1;
    }
    strncpy(buffer, arg, eqp - arg);
    buffer[eqp - arg] = '\0';

    return pl_main_set_typed_param(pmi, pl_spt_parsed, buffer, value);
}

int
pl_main_set_typed_param(pl_main_instance_t *pmi, pl_set_param_type type, const char *param, const void *value)
{
    int code = 0;
    gs_c_param_list *params = &pmi->params;
    gs_param_string str_value;
    bool bval;
    int more_to_come = type & pl_spt_more_to_come;

    if (pmi->mid_runstring) {
        dmprintf(pmi->memory, "Can't set parameters mid run_string\n");
        return -1;
    }

    type &= ~pl_spt_more_to_come;

    /* First set it in the device params */
    gs_c_param_list_write_more(params);
    switch (type)
    {
    case pl_spt_null:
        code = param_write_null((gs_param_list *) params,
                                param);
        break;
    case pl_spt_bool:
        bval = !!*(int *)value;
        code = param_write_bool((gs_param_list *) params,
                                param, &bval);
        break;
    case pl_spt_int:
        code = param_write_int((gs_param_list *) params,
                               param, (int *)value);
        break;
    case pl_spt_float:
        code = param_write_float((gs_param_list *) params,
                                 param, (float *)value);
        break;
    case pl_spt_name:
        param_string_from_transient_string(str_value, value);
        code = param_write_name((gs_param_list *) params,
                                param, &str_value);
        break;
    case pl_spt_string:
        param_string_from_transient_string(str_value, value);
        code = param_write_string((gs_param_list *) params,
                                  param, &str_value);
        break;
    case pl_spt_long:
        code = param_write_long((gs_param_list *) params,
                                param, (long *)value);
        break;
    case pl_spt_i64:
        code = param_write_i64((gs_param_list *) params,
                               param, (int64_t *)value);
        break;
    case pl_spt_size_t:
        code = param_write_size_t((gs_param_list *) params,
                                  param, (size_t *)value);
        break;
    case pl_spt_parsed:
        code = gs_param_list_add_parsed_value((gs_param_list *)params,
                                              param, (void *)value);
        break;
    default:
        code = gs_note_error(gs_error_rangecheck);
    }
    if (code < 0) {
        gs_c_param_list_release(params);
        return code;
    }
    gs_c_param_list_read(params);

    if (pmi->implementations == NULL || more_to_come) {
        /* Leave it in the param list for later. */
        return 0;
    }

    /* Then send it to the languages */
    code = pass_param_to_languages(pmi, (gs_param_list *)params);
    if (code < 0)
        return code;

    /* Send it to the device, if there is one. */
    if (pmi->device) {
        code = gs_putdeviceparams(pmi->device, (gs_param_list *)params);
        gs_c_param_list_release(params);
    }

    return code;
}

int
pl_main_get_typed_param(pl_main_instance_t *pmi, pl_set_param_type type, const char *param, void *value)
{
    int code = 0;
    gs_c_param_list params;
    gs_param_string str_value;

    if (pmi->mid_runstring) {
        dmprintf(pmi->memory, "Can't get parameters mid run_string\n");
        return -1;
    }

    /* The "more to come" bit should never be set, but clear it anyway. */
    type &= ~pl_spt_more_to_come;

    gs_c_param_list_write(&params, pmi->memory);
    code = gs_getdeviceparams(pmi->device, (gs_param_list *)&params);
    if (code < 0) {
        gs_c_param_list_release(&params);
        return code;
    }

    gs_c_param_list_read(&params);
    switch (type)
    {
    case pl_spt_null:
        code = param_read_null((gs_param_list *)&params, param);
        if (code == 1)
            code = gs_error_undefined;
        if (code < 0)
            break;
        code = 0;
        break;
    case pl_spt_bool:
    {
        bool b;
        code = param_read_bool((gs_param_list *)&params, param, &b);
        if (code == 1)
            code = gs_error_undefined;
        if (code < 0)
            break;
        code = sizeof(int);
        if (value != NULL)
            *(int *)value = !!b;
        break;
    }
    case pl_spt_int:
    {
        int i;
        code = param_read_int((gs_param_list *)&params, param, &i);
        if (code == 1)
            code = gs_error_undefined;
        if (code < 0)
            break;
        code = sizeof(int);
        if (value != NULL)
            *(int *)value = i;
        break;
    }
    case pl_spt_float:
    {
        float f;
        code = param_read_float((gs_param_list *)&params, param, &f);
        if (code == 1)
            code = gs_error_undefined;
        if (code < 0)
            break;
        code = sizeof(float);
        if (value != NULL)
            *(float *)value = f;
        break;
    }
    case pl_spt_name:
        code = param_read_name((gs_param_list *)&params, param, &str_value);
        if (code == 1)
            code = gs_error_undefined;
        if (code < 0)
            break;
        if (value != NULL) {
            memcpy(value, str_value.data, str_value.size);
            ((char *)value)[str_value.size] = 0;
        }
        code = str_value.size+1;
        break;
    case pl_spt_string:
        code = param_read_string((gs_param_list *)&params, param, &str_value);
        if (code == 1)
            code = gs_error_undefined;
        if (code < 0)
            break;
        else if (value != NULL) {
            memcpy(value, str_value.data, str_value.size);
            ((char *)value)[str_value.size] = 0;
        }
        code = str_value.size+1;
        break;
    case pl_spt_long:
    {
        long l;
        code = param_read_long((gs_param_list *)&params, param, &l);
        if (code == 1)
            code = gs_error_undefined;
        if (code < 0)
            break;
        if (value != NULL)
            *(long *)value = l;
        code = sizeof(long);
        break;
    }
    case pl_spt_i64:
    {
        int64_t i64;
        code = param_read_i64((gs_param_list *)&params, param, &i64);
        if (code == 1)
            code = gs_error_undefined;
        if (code < 0)
            break;
        if (value != NULL)
            *(int64_t *)value = i64;
        code = sizeof(int64_t);
        break;
    }
    case pl_spt_size_t:
    {
        size_t z;
        code = param_read_size_t((gs_param_list *)&params, param, &z);
        if (code == 1)
            code = gs_error_undefined;
        if (code < 0)
            break;
        if (value != NULL)
            *(size_t *)value = z;
        code = sizeof(size_t);
        break;
    }
    case pl_spt_parsed:
    {
        int len;
        code = gs_param_list_to_string((gs_param_list *)&params,
                                       param, (char *)value, &len);
        if (code == 1)
            code = gs_error_undefined;
        if (code >= 0)
            code = len;
        break;
    }
    default:
        code = gs_note_error(gs_error_rangecheck);
    }
    gs_c_param_list_release(&params);

    return code;
}

int pl_main_enumerate_params(pl_main_instance_t *pmi, void **iter, const char **key, pl_set_param_type *type)
{
    int code = 0;
    gs_param_key_t keyp;

    if (key == NULL)
        return -1;
    *key = NULL;
    if (pmi == NULL || iter == NULL)
        return -1;

    if (*iter == NULL) {
        /* Free any existing param list. */
        gs_c_param_list_release(&pmi->enum_params);
        /* No device -> no params. */
        if (pmi->device == NULL)
            return 1;
        /* Set up a new one. */
        gs_c_param_list_write(&pmi->enum_params, pmi->memory);
        /* Get the keys. */
        code = gs_getdeviceparams(pmi->device, (gs_param_list *)&pmi->enum_params);
        if (code < 0)
            return code;

        param_init_enumerator(&pmi->enum_iter);
        *iter = &pmi->enum_iter;
    } else if (*iter != &pmi->enum_iter)
        return -1;

    gs_c_param_list_read(&pmi->enum_params);
    code = param_get_next_key((gs_param_list *)&pmi->enum_params, &pmi->enum_iter, &keyp);
    if (code < 0)
        return code;
    if (code != 0) {
        /* End of iteration. */
        *iter = NULL;
        *key = NULL;
        return 1;
    }
    if (pmi->enum_keybuf_max < keyp.size+1) {
        int newsize = keyp.size+1;
        char *newkey;
        if (newsize < 128)
            newsize = 128;
        if (pmi->enum_keybuf == NULL) {
            newkey = (char *)gs_alloc_bytes(pmi->memory, newsize, "enumerator key buffer");
        } else {
            newkey = (char *)gs_resize_object(pmi->memory, pmi->enum_keybuf, newsize, "enumerator key buffer");
        }
        if (newkey == NULL)
            return_error(gs_error_VMerror);
        pmi->enum_keybuf = newkey;
        pmi->enum_keybuf_max = newsize;
    }
    memcpy(pmi->enum_keybuf, keyp.data, keyp.size);
    pmi->enum_keybuf[keyp.size] = 0;
    *key = pmi->enum_keybuf;

    if (type) {
        gs_param_typed_value pvalue;
        pvalue.type = gs_param_type_any;
        code = param_read_typed((gs_param_list *)&pmi->enum_params, *key, &pvalue);
        if (code < 0)
            return code;
        if (code > 0)
            return_error(gs_error_unknownerror);

        switch (pvalue.type) {
        case gs_param_type_null:
            *type = pl_spt_null;
            break;
        case gs_param_type_bool:
            *type = pl_spt_bool;
            break;
        case gs_param_type_int:
            *type = pl_spt_int;
            break;
        case gs_param_type_long:
            *type = pl_spt_long;
            break;
        case gs_param_type_size_t:
            *type = pl_spt_size_t;
            break;
        case gs_param_type_i64:
            *type = pl_spt_i64;
            break;
        case gs_param_type_float:
            *type = pl_spt_float;
            break;
        case gs_param_type_string:
            *type = pl_spt_string;
            break;
        case gs_param_type_name:
            *type = pl_spt_name;
            break;
        default:
            *type = pl_spt_parsed;
            break;
        }
    }

    return code;
}

static int
handle_dash_s(pl_main_instance_t *pmi, const char *arg, int *device_index)
{
    int code = 0;
    char *eqp;
    const char *value;

    eqp = strchr(arg, '=');
    if (!(eqp || (eqp = strchr(arg, '#')))) {
        dmprintf(pmi->memory,
                 "Usage for -s is -s<option>=<string>\n");
        return -1;
    }
    value = eqp + 1;
    if (!strncmp(arg, "DEVICE", 6)) {
        if (device_index == NULL) {
            dmprintf(pmi->memory, "DEVICE cannot be set this late!\n");
            return -1;
        }
        if (*device_index != -1) {
            dmprintf(pmi->memory, "DEVICE can only be set once!\n");
            return -1;
        }
        *device_index = get_device_index(pmi->memory, value);
        if (*device_index == -1)
            return -1;
    } else if (!strncmp(arg, "DefaultGrayProfile",
                        strlen("DefaultGrayProfile"))) {
        pmi->pdefault_gray_icc = arg_copy(value, pmi->memory);
    } else if (!strncmp(arg, "DefaultRGBProfile",
                        strlen("DefaultRGBProfile"))) {
        pmi->pdefault_rgb_icc = arg_copy(value, pmi->memory);
    } else if (!strncmp(arg, "DefaultCMYKProfile",
                        strlen("DefaultCMYKProfile"))) {
        pmi->pdefault_cmyk_icc = arg_copy(value, pmi->memory);
    } else if (!strncmp(arg, "ICCProfileDir", strlen("ICCProfileDir"))) {
        pmi->piccdir = arg_copy(value, pmi->memory);
    } else if (!strncmp(arg, "OutputFile", 10) && strlen(eqp) > 0) {
        code = gs_add_outputfile_control_path(pmi->memory, eqp+1);
        if (code < 0)
            return code;
        code = pl_main_set_string_param(pmi, arg);
    } else {
        code = pl_main_set_string_param(pmi, arg);
    }
    return code;
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

static int
pl_main_process_options(pl_main_instance_t * pmi, arg_list * pal,
                        pl_interp_implementation_t * pjli)
{
    int code = 0;
    const char *arg = NULL;
    gs_c_param_list *params = &pmi->params;
    int device_index = -1;
    char *collected_commands = NULL;
    bool not_an_arg = 1;

    /* By default, stdin_is_interactive = 1. This mirrors what stdin_init
     * would have done in the iodev one time initialisation. We aren't
     * getting our stdin via an iodev though, so we do it here. */
    pmi->memory->gs_lib_ctx->core->stdin_is_interactive = 1;

    gs_c_param_list_write_more(params);
    while (arg != NULL || (code = arg_next(pal, (const char **)&arg, pmi->memory)) > 0) {
        if (*arg != '-') /* Stop when we hit something that isn't an option */
            break;
        code = gs_lib_ctx_stash_sanitized_arg(pmi->memory->gs_lib_ctx, arg);
        if (code < 0)
            return code;
        if (arg[1] == 0) {
            /* Stdin, not an option! */
            not_an_arg = 1;
            break;
        }
        arg += 2;
        switch (arg[-1]) {
            case '-':
                if (strcmp(arg, "help") == 0) {
                    goto help;
                } else if (strcmp(arg, "debug") == 0) {
                    gs_debug_flags_list(pmi->memory);
                    break;
                } else if (strncmp(arg, "debug=", 6) == 0) {
                    gs_debug_flags_parse(pmi->memory, arg + 6);
                    break;
#ifndef OMIT_SAVED_PAGES	/* TBI */
                } else if (strncmp(arg, "saved-pages=", 12) == 0) {
                    gx_device *pdev = pmi->device;
                    gx_device_printer *ppdev = (gx_device_printer *)pdev;

                    /* open the device if not yet open */
                    if (pdev->is_open == 0 && (code = gs_opendevice(pdev)) < 0) {
                        break;
                    }
                    if (dev_proc(pdev, dev_spec_op)(pdev, gxdso_supports_saved_pages, NULL, 0) <= 0) {
                        errprintf(pmi->memory, "   --saved-pages not supported by the '%s' device.\n",
                                  pdev->dname);
                        return -1;
                    }
                    code = gx_saved_pages_param_process(ppdev, (byte *)arg+12, strlen(arg+12));
                    if (code > 0) {
                        /* erase the page */
                        gx_color_index color;
                        gx_color_value rgb_white[3] = { 65535, 65535, 65535 };
                        gx_color_value cmyk_white[4] = { 0, 0, 0, 0 };

                        if (pdev->color_info.polarity == GX_CINFO_POLARITY_ADDITIVE)
                            color = dev_proc(pdev, map_rgb_color)(pdev, rgb_white);
                        else
                            color = dev_proc(pdev, map_cmyk_color)(pdev, cmyk_white);
                        code = dev_proc(pdev, fill_rectangle)(pdev, 0, 0, pdev->width, pdev->height, color);
                    }
                    break;
#endif /* not defined OMIT_SAVED_PAGES */
                /* The following code is only to allow regression testing of saved-pages */
                /* if OMIT_SAVED_PAGES_TEST is defined, ignore it so testing can proceed */
                } else if (strncmp(arg, "saved-pages-test", 16) == 0) {
#ifndef OMIT_SAVED_PAGES_TEST
                    gx_device *pdev = pmi->device;

                    if ((code = gs_opendevice(pdev)) < 0)
                        break;

                    if (dev_proc(pdev, dev_spec_op)(pdev, gxdso_supports_saved_pages, NULL, 0) <= 0) {
                        errprintf(pmi->memory, "   --saved-pages-test not supported by the '%s' device.\n",
                                  pdev->dname);
                        break;			/* just ignore it */
                    }
                    code = gx_saved_pages_param_process((gx_device_printer *)pdev, (byte *)"begin", 5);
                    if (code > 0) {
                        /* erase the page */
                        gx_color_index color;
                        gx_color_value rgb_white[3] = { 65535, 65535, 65535 };
                        gx_color_value cmyk_white[4] = { 0, 0, 0, 0 };

                        if (pdev->color_info.polarity == GX_CINFO_POLARITY_ADDITIVE)
                            color = dev_proc(pdev, map_rgb_color)(pdev, rgb_white);
                        else
                            color = dev_proc(pdev, map_cmyk_color)(pdev, cmyk_white);
                        code = dev_proc(pdev, fill_rectangle)(pdev, 0, 0, pdev->width, pdev->height, color);
                    }
                    if (code >= 0)
                        pmi->saved_pages_test_mode = true;
#endif /* OMIT_SAVED_PAGES_TEST */
                    break;
                }
                /* Now handle the explicitly added paths to the file control lists */
                else if (arg_match(&arg, "permit-file-read")) {
                    code = gs_add_explicit_control_path(pmi->memory, arg, gs_permit_file_reading);
                    if (code < 0) return code;
                    break;
                } else if (arg_match(&arg, "permit-file-write")) {
                    code = gs_add_explicit_control_path(pmi->memory, arg, gs_permit_file_writing);
                    if (code < 0) return code;
                    break;
                } else if (arg_match(&arg, "permit-file-control")) {
                    code = gs_add_explicit_control_path(pmi->memory, arg, gs_permit_file_control);
                    if (code < 0) return code;
                    break;
                } else if (arg_match(&arg, "permit-file-all")) {
                    code = gs_add_explicit_control_path(pmi->memory, arg, gs_permit_file_reading);
                    if (code < 0) return code;
                    code = gs_add_explicit_control_path(pmi->memory, arg, gs_permit_file_writing);
                    if (code < 0) return code;
                    code = gs_add_explicit_control_path(pmi->memory, arg, gs_permit_file_control);
                    if (code < 0) return code;
                    break;
                }
                if (*arg != 0) {
                    dmprintf1(pmi->memory, "Unrecognized switch: %s\n", arg-2);
                    code = -1;
                    break;
                }
                break;
            default:
                dmprintf1(pmi->memory, "Unrecognized switch: %s\n", arg-2);
                code = -1;
                break;
            case 'c':
                code = handle_dash_c(pmi, pal, &collected_commands, &arg);
                if (code < 0)
                    arg = NULL;
                continue; /* Next arg has already been read, if there is one */
            case 'd':
            case 'D':
                code = pl_main_set_param(pmi, arg);
                if (code < 0)
                    return code;
                break;
            case 'E':
                if (*arg == 0)
                    gs_debug['#'] = 1;
                else
                    (void)sscanf(arg, "%d", &pmi->error_report);
                break;
            case 'f':
                code = arg_next(pal, (const char **)&arg, pmi->memory);
                if (code < 0)
                    return code;
                if (arg == NULL) {
                    dmprintf(pmi->memory, "-f must be followed by a filename\n");
                    continue;
                }
                code = gs_lib_ctx_stash_sanitized_arg(pmi->memory->gs_lib_ctx, "?");
                if (code < 0)
                    return code;
                goto out;
            case 'g':
                {
                    int geom[2];
                    gs_param_int_array ia;

                    if (sscanf(arg, "%ux%u", &geom[0], &geom[1]) != 2) {
                        dmprintf(pmi->memory,
                                 "-g must be followed by <width>x<height>\n");
                        return -1;
                    }
                    ia.data = geom;
                    ia.size = 2;
                    ia.persistent = false;
                    gs_c_param_list_write_more(params);
                    code =
                        param_write_int_array((gs_param_list *) params,
                                              "HWSize", &ia);
                    if (code >= 0) {
                        pmi->page_set_on_command_line = true;
                        pmi->page_size[0] = geom[0];
                        pmi->page_size[1] = geom[1];
                    }
                }
                break;
            case 'h':
help:
                arg_finit(pal);
                gs_c_param_list_release(params);
                return gs_error_Info;
            case 'H':
                {
                    float hwmarg[4];
                    gs_param_float_array fa;
                    uint sz = countof(hwmarg);
                    uint parsed = parse_floats(pmi->memory, sz, arg, hwmarg);
                    if (parsed != sz) {
                        dmprintf(pmi->memory,
                                 "-H must be followed by <left>x<bottom>x<right>x<top>\n");
                        return -1;
                    }
                    fa.data = hwmarg;
                    fa.size = parsed;
                    fa.persistent = false;
                    gs_c_param_list_write_more(params);
                    code =
                        param_write_float_array((gs_param_list *) params,
                                              ".HWMargins", &fa);
                }
                break;
            case 'I':               /* specify search path */
                {
                    const char *path;

                    if (arg[0] == 0) {
                        code = arg_next(pal, (const char **)&path, pmi->memory);
                        if (code < 0)
                            return code;
                        code = gs_lib_ctx_stash_sanitized_arg(pmi->memory->gs_lib_ctx, "?");
                        if (code < 0)
                            return code;
                    } else
                        path = arg;
                    if (path == NULL)
                        return gs_error_Fatal;
                    code = pass_path_to_languages(pmi, path);
                }
                break;
                /* job control line follows - PJL */
            case 'j':
            case 'J':
                /* set up the read cursor and send it to the pjl parser */
                {
                    stream_cursor_read cursor;
                    /* PJL lines have max length of 80 character + null terminator */
                    byte buf[512];
                    /* length of arg + newline (expected by PJL parser) + null */
                    int buf_len = strlen(arg) + 2;
                    if ((buf_len) > sizeof(buf)) {
                        dmprintf(pmi->memory, "pjl sequence too long\n");
                        return -1;
                    }

                    /* replace ";" with newline for the PJL parser and
                       concatenate NULL. */
                    {
                        int i;

                        for (i = 0; i < buf_len - 2; i++) {
                            if (arg[i] == ';')
                                buf[i] = '\n';
                            else
                                buf[i] = arg[i];
                        }
                        buf[i] = '\n';
                        i++;
                        buf[i] = '\0';
                    }
                    stream_cursor_read_init(&cursor, (const byte *)buf, buf_len);

                    /* process the pjl */
                    code = pl_process(pjli, &cursor);
                    pmi->pjl_from_args = true;
                    /* we expect the language to exit properly otherwise
                       there was some sort of problem */
                    if (code != e_ExitLanguage) {
                        if (code == 0) code = -1;
                    } else
                        code = 0;
                }
                break;
            case 'K':          /* max memory in K */
                {
                    int maxk;
                    gs_malloc_memory_t *rawheap =
                        (gs_malloc_memory_t *) gs_memory_chunk_target(pmi->
                                                                      memory)->
                        non_gc_memory;
                    if (sscanf(arg, "%d", &maxk) != 1) {
                        dmprintf(pmi->memory,
                                 "-K must be followed by a number\n");
                        code = -1;
                    }
                    else
                        rawheap->limit = (long)maxk << 10;
                }
                break;
            case 'l':          /* personality */
                {
                    if (!strcmp(arg, "RTL") || !strcmp(arg, "PCL5E") ||
                        !strcmp(arg, "PCL5C"))
                        strcpy(pmi->pcl_personality, arg);
                    else
                        dmprintf(pmi->memory,
                                 "PCL personality must be RTL, PCL5E or PCL5C\n");
                }
                break;
            case 'L':          /* language */
                {
                    int index;
                    pl_interp_implementation_t **impls = pmi->implementations;

                    /*
                     * the 0th entry always holds the PJL
                     * implementation, which is never explicitly
                     * selected.
                     */
                    for (index = 1; impls[index] != 0; ++index)
                        if (!strcmp(arg,
                                    pl_characteristics(impls[index])->
                                    language))
                            break;
                    if (impls[index] != 0)
                        pmi->implementation = impls[index];
                    else {
                        dmprintf(pmi->memory,
                                 "Choose language in -L<language> from: ");
                        for (index = 1; impls[index] != 0; ++index)
                            dmprintf1(pmi->memory, "%s ",
                                      pl_characteristics(impls[index])->
                                      language);
                        dmprintf(pmi->memory, "\n");
                        return -1;
                    }
                    break;
                }
            case 'm':
                {
                    float marg[2];
                    gs_param_float_array fa;
                    uint sz = countof(marg);
                    uint parsed = parse_floats(pmi->memory, sz, arg, marg);
                    if (parsed != sz) {
                        dmprintf(pmi->memory,
                                 "-m must be followed by <left>x<bottom>\n");
                        return -1;
                    }
                    fa.data = marg;
                    fa.size = parsed;
                    fa.persistent = false;
                    gs_c_param_list_write_more(params);
                    code =
                        param_write_float_array((gs_param_list *) params,
                                                "Margins", &fa);

                }
                break;
            case 'o':
                {
                    const char *adef;
                    gs_param_string str;

                    if (arg[0] == 0) {
                        code = arg_next(pal, (const char **)&adef, pmi->memory);
                        if (code < 0)
                            break;
                        code = gs_lib_ctx_stash_sanitized_arg(pmi->memory->gs_lib_ctx, "?");
                        if (code < 0)
                            return code;
                    } else
                        adef = arg;
                    if (adef == NULL)
                    {
                        code = gs_error_undefinedfilename;
                        break;
                    }
                    code = gs_add_outputfile_control_path(pmi->memory, adef);
                    if (code < 0)
                        break;
                    param_string_from_transient_string(str, adef);
                    gs_c_param_list_write_more(params);
                    code =
                        param_write_string((gs_param_list *) params,
                                           "OutputFile", &str);
                    pmi->pause = false;
                    break;
                }
            case 'p':
                code = pl_main_set_parsed_param(pmi, arg);
                if (code < 0)
                    return code;
                break;
            case 'r':
                {
                    float res[2];
                    gs_param_float_array fa;
                    uint sz = countof(res);
                    uint parsed = parse_floats(pmi->memory, sz, arg, res);
                    switch (parsed) {
                        default:
                            dmprintf(pmi->memory,
                                     "-r must be followed by <res> or <xres>x<yres>\n");
                            return -1;
                        case 1:        /* -r<res> */
                            res[1] = res[0];
                        case 2:        /* -r<xres>x<yres> */
                            ;
                    }
                    fa.data = res;
                    fa.size = sz;
                    fa.persistent = false;
                    gs_c_param_list_write_more(params);
                    code =
                        param_write_float_array((gs_param_list *) params,
                                                "HWResolution", &fa);
                    if (code == 0) {
                        pmi->res_set_on_command_line = true;
                        pmi->res[0] = res[0];
                        pmi->res[1] = res[1];
                    }
                }
                break;
            case 's':
            case 'S':
                code = handle_dash_s(pmi, arg, &device_index);
                if (code < 0)
                    return code;
                break;
            case 'q':
                /* Silently accept -q to match gs. We are pretty quiet
                 * on startup anyway. */
                code = pl_main_set_param(pmi, "QUIET");
                break;
            case 'v':               /* print revision */
                errprintf(pmi->memory, "%s\n", PJL_VERSION_STRING);
                arg_finit(pal);
                gs_c_param_list_release(params);
                return 0;
            case 'Z':
                set_debug_flags(arg, gs_debug);
                break;
        }
        if (code < 0)
            return code;
        arg = NULL;
    }
  out:
    /* PCL does not support spot colors and XPS files with spot colors are very
       rare (as in never found in the wild). Handling XPS spots for a separation
       device will require a little work. To avoid issues at the current time,
       we will do the following
    */
    {
        int num_spots = 0;
        gs_c_param_list_write_more(params);
        code = param_write_int((gs_param_list *)params, "PageSpotColors", &(num_spots));
        if (code < 0)
            return code;
    }

    /* Do any last minute language specific device initialisation
     * (i.e. let gs_init.ps do its worst). */
    code = pl_main_post_args_init(pmi);
    if (code < 0)
        return code;

    gs_c_param_list_read(params);
    code = pl_top_create_device(pmi, device_index); /* create default device if needed */
    if (code < 0)
        return code;

    code = gs_putdeviceparams(pmi->device, (gs_param_list *) params);
    if (code < 0)
        return code;

    /* If we have (at least one) filename to process */
    if (arg) {
        /* From here on in, we can only accept -c, -d, -f, -s, -p and filenames */
        /* In the unlikely event that someone wants to run a file starting with
         * a '-', they'll do "-f -blah". The - of the "-blah" must not be accepted
         * by the arg processing in the loop below. We use 'not_an_arg' to handle
         * this. */
        while (1) {
            if (arg == NULL) {
                code = arg_next(pal, (const char **)&arg, pmi->memory);
                if (code < 0)
                    break;
                if (arg == NULL)
                    break;
                code = gs_lib_ctx_stash_sanitized_arg(pmi->memory->gs_lib_ctx, arg);
                if (code < 0)
                    return code;
            }
            if (!not_an_arg && arg[0] == '-' && arg[1] == 'c') {
                code = handle_dash_c(pmi, pal, &collected_commands, &arg);
                if (code < 0)
                    break;
                not_an_arg = 0;
                continue; /* We've already read any -f into arg */
            } else if (!not_an_arg && arg[0] == '-' &&
                       (arg[1] == 's' || arg[1] == 'S')) {
                code = handle_dash_s(pmi, arg+2, NULL);
                if (code < 0)
                    break;
                arg = NULL;
                not_an_arg = 0;
                continue; /* We've already read any -f into arg */
            } else if (!not_an_arg && arg[0] == '-' &&
                       (arg[1] == 'd' || arg[1] == 'D')) {
                code = pl_main_set_param(pmi, arg+2);
                if (code < 0)
                    break;
                arg = NULL;
                not_an_arg = 0;
                continue; /* We've already read any -f into arg */
            } else if (!not_an_arg && arg[0] == '-' && arg[1] == 'p') {
                code = pl_main_set_parsed_param(pmi, arg+2);
                if (code < 0)
                    break;
                arg = NULL;
                not_an_arg = 0;
                continue; /* We've already read any -f into arg */
            } else if (!not_an_arg && arg[0] == '-' && arg[1] == 'f') {
                code = arg_next(pal, (const char **)&arg, pmi->memory);
                if (code < 0)
                    return code;
                if (arg == NULL) {
                    dmprintf(pmi->memory, "-f must be followed by a filename\n");
                    continue;
                }
                code = gs_lib_ctx_stash_sanitized_arg(pmi->memory->gs_lib_ctx, "?");
                if (code < 0)
                    return code;
                not_an_arg = 1;
                continue;
            } else {
                code = gs_add_control_path(pmi->memory, gs_permit_file_reading, arg);
                if (code < 0)
                    break;
                code = pl_main_run_file_utf8(pmi, collected_commands, arg);
                (void)gs_remove_control_path(pmi->memory, gs_permit_file_reading, arg);
                if (code == gs_error_undefinedfilename)
                    errprintf(pmi->memory, "Failed to open file '%s'\n", arg);
                gs_free_object(pmi->memory, collected_commands, "-c buffer");
                collected_commands = NULL;
                if (code < 0)
                    break;
            }
            arg = NULL;
            not_an_arg = 0;
        }
    }

    if (code == 0 && collected_commands != NULL) {
        /* Find the PS interpreter */
        int index;
        pl_interp_implementation_t **impls = pmi->implementations;

        /* Start at 1 to skip PJL */
        for (index = 1; impls[index] != 0; ++index)
            if (!strcmp("POSTSCRIPT",
                        pl_characteristics(impls[index])->language))
                break;
        if (impls[index] == 0) {
            dmprintf(pmi->memory, "-c can only be used in a built with POSTSCRIPT included.\n");
            return gs_error_Fatal;
        }
        pmi->implementation = impls[index];
        code = pl_main_run_prefix(pmi, collected_commands);
    }
    gs_free_object(pmi->memory, collected_commands, "-c buffer");
    collected_commands = NULL;

    return code;
}

#define PJL_UEL "\033%-12345X"

/* Find default language implementation */
static pl_interp_implementation_t *
pl_auto_sense(pl_main_instance_t *minst, const char *buffer,  int buffer_length)
{
    /* Lookup this string in the auto sense field for each implementation */
    pl_interp_implementation_t **impls = minst->implementations;
    pl_interp_implementation_t **impl;
    size_t uel_len = strlen(PJL_UEL);
    pl_interp_implementation_t *best = NULL;
    int max_score = 0;

    /* first check for a UEL */
    if (buffer_length >= uel_len) {
        if (!memcmp(buffer, PJL_UEL, uel_len))
            return impls[0];
    }

    /* Defaults to language 1 (if there is one): PJL is language 0, PCL is language 1. */
    best = impls[1] ? impls[1] : impls[0];
    for (impl = impls; *impl != NULL; ++impl) {
        int score = pl_characteristics(*impl)->auto_sense(buffer, buffer_length);
        if (score > max_score) {
            best = *impl;
            max_score = score;
        }
    }
    return best;
}

/* either the (1) implementation has been selected on the command line or
   (2) it has been selected in PJL or (3) we need to auto sense. */
static pl_interp_implementation_t *
pl_select_implementation(pl_interp_implementation_t * pjli,
                         pl_main_instance_t * pmi,
                         const char *ptr,
                         int len)
{
    /* Determine language of file to interpret. We're making the incorrect */
    /* assumption that any file only contains jobs in one PDL. The correct */
    /* way to implement this would be to have a language auto-detector. */
    pl_interp_implementation_t *impl;

    if (pmi->implementation)
        return pmi->implementation;     /* was specified as cmd opt */
    /* select implementation */
    if ((impl = pl_pjl_select(pmi, pjli)) != 0)
        return impl;
    /* lookup string in name field for each implementation */
    return pl_auto_sense(pmi, ptr, len);
}

/* Find default language implementation */
static pl_interp_implementation_t *
pl_pjl_select(pl_main_instance_t * pmi, pl_interp_implementation_t * pjli)
{
    pjl_envvar_t *language;
    pl_interp_implementation_t **impls = pmi->implementations;
    pl_interp_implementation_t **impl;

    language = pjl_proc_get_envvar(pjli, "language");
    for (impl = impls; *impl != 0; ++impl) {
        if (!strcmp(pl_characteristics(*impl)->language, language))
            return *impl;
    }
    return 0;
}

/* Print memory and time usage. */
void
pl_print_usage(const pl_main_instance_t * pti, const char *msg)
{
    long utime[2];
    gx_device *pdev = pti->device;

    gp_get_realtime(utime);
    dmprintf3(pti->memory, "%% %s time = %g, pages = %ld\n",
              msg, utime[0] - pti->base_time[0] +
              (utime[1] - pti->base_time[1]) / 1000000000.0, pdev->PageCount);
}

/* Log a string to console, optionally wait for input */
static void
pl_log_string(const gs_memory_t * mem, const char *str, int wait_for_key)
{
    errwrite(mem, str, strlen(str));
    if (wait_for_key)
        (void)fgetc(mem->gs_lib_ctx->core->fstdin);
}

pl_interp_implementation_t *
pl_main_get_pcl_instance(const gs_memory_t *mem)
{
    return pl_main_get_instance(mem)->implementations[1];
}

pl_interp_implementation_t *
pl_main_get_pjl_instance(const gs_memory_t *mem)
{
    return pl_main_get_instance(mem)->implementations[0];
}

bool pl_main_get_interpolate(const gs_memory_t *mem)
{
    return pl_main_get_instance(mem)->interpolate;
}

bool pl_main_get_nocache(const gs_memory_t *mem)
{
    return pl_main_get_instance(mem)->nocache;
}

bool pl_main_get_page_set_on_command_line(const gs_memory_t *mem)
{
    return pl_main_get_instance(mem)->page_set_on_command_line;
}

bool pl_main_get_res_set_on_command_line(const gs_memory_t *mem)
{
    return pl_main_get_instance(mem)->res_set_on_command_line;
}

void pl_main_get_forced_geometry(const gs_memory_t *mem, const float **resolutions, const long **dimensions)
{
    pl_main_instance_t *minst = pl_main_get_instance(mem);

    if (resolutions) {
        if (minst->res_set_on_command_line)
            *resolutions = minst->res;
        else
            *resolutions = NULL;
    }
    if (dimensions) {
        if (minst->page_set_on_command_line)
            *dimensions = minst->page_size;
        else
            *dimensions = NULL;
    }
}

bool pl_main_get_high_level_device(const gs_memory_t *mem)
{
    return pl_main_get_instance(mem)->high_level_device;
}

bool pl_main_get_scanconverter(const gs_memory_t *mem)
{
    return pl_main_get_instance(mem)->scanconverter;
}

int
pl_set_icc_params(const gs_memory_t *mem, gs_gstate *pgs)
{
    pl_main_instance_t *minst = pl_main_get_instance(mem);
    gs_param_string p;
    int code = 0;

    if (minst->pdefault_gray_icc) {
        param_string_from_transient_string(p, minst->pdefault_gray_icc);
        code = gs_setdefaultgrayicc(pgs, &p);
        if (code < 0)
            return gs_throw_code(gs_error_Fatal);
    }

    if (minst->pdefault_rgb_icc) {
        param_string_from_transient_string(p, minst->pdefault_rgb_icc);
        code = gs_setdefaultrgbicc(pgs, &p);
        if (code < 0)
            return gs_throw_code(gs_error_Fatal);
    }

    if (minst->piccdir) {
        param_string_from_transient_string(p, minst->piccdir);
        code = gs_seticcdirectory(pgs, &p);
        if (code < 0)
            return gs_throw_code(gs_error_Fatal);
    }
    return code;
}

int
pl_finish_page(pl_main_instance_t * pmi, gs_gstate * pgs, int num_copies, int flush)
{
    int code        = 0;
    gx_device *pdev = pmi->device;

    /* output the page */
    if (gs_debug_c(':'))
        pl_print_usage(pmi, "parse done :");
    code = gs_output_page(pgs, num_copies, flush);
    if (code < 0)
        return code;

    if (pmi->pause) {
        char strbuf[256];

        gs_sprintf(strbuf, "End of page %d, press <enter> to continue.\n",
                pdev->PageCount);
        pl_log_string(pmi->memory, strbuf, 1);
    } else if (gs_debug_c(':'))
        pl_print_usage(pmi, "render done :");
    return 0;
}
