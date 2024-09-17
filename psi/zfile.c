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


/* Non-I/O file operators */
#include "memory_.h"
#include "string_.h"
#include "unistd_.h"
#include "stat_.h" /* get system header early to avoid name clash on Cygwin */
#include "ghost.h"
#include "gscdefs.h"            /* for gx_io_device_table */
#include "gsutil.h"             /* for bytes_compare */
#include "gp.h"
#include "gpmisc.h"
#include "gsfname.h"
#include "gsstruct.h"           /* for registering root */
#include "gxalloc.h"            /* for streams */
#include "oper.h"
#include "dstack.h"             /* for systemdict */
#include "estack.h"             /* for filenameforall, .execfile */
#include "ialloc.h"
#include "ilevel.h"             /* %names only work in Level 2 */
#include "iname.h"
#include "isave.h"              /* for restore */
#include "idict.h"
#include "iddict.h"
#include "iutil.h"
#include "stream.h"
#include "strimpl.h"
#include "sfilter.h"
#include "gxiodev.h"            /* must come after stream.h */
                                /* and before files.h */
#include "files.h"
#include "main.h"               /* for gs_lib_paths */
#include "store.h"
#include "zfile.h"

/* Import the IODevice table. */
extern_gx_io_device_table();

/* Import the dtype of the stdio IODevices. */
extern const char iodev_dtype_stdio[];

/* Forward references: file name parsing. */
static int parse_file_name(const ref * op, gs_parsed_file_name_t * pfn,
                           bool safemode, gs_memory_t *memory);
static int parse_real_file_name(const ref * op,
                                 gs_parsed_file_name_t * pfn,
                                 gs_memory_t *mem, client_name_t cname);
static int parse_file_access_string(const ref *op, char file_access[4]);

/* Forward references: other. */
static int execfile_finish(i_ctx_t *);
static int execfile_cleanup(i_ctx_t *);
static iodev_proc_open_file(iodev_os_open_file);
stream_proc_report_error(filter_report_error);

/*
 * Since there can be many file objects referring to the same file/stream,
 * we can't simply free a stream when we close it.  On the other hand,
 * we don't want freed streams to clutter up memory needlessly.
 * Our solution is to retain the freed streams, and reuse them.
 * To prevent an old file object from being able to access a reused stream,
 * we keep a serial number in each stream, and check it against a serial
 * number stored in the file object (as the "size"); when we close a file,
 * we increment its serial number.  If the serial number ever overflows,
 * we leave it at zero, and do not reuse the stream.
 * (This will never happen.)
 *
 * Storage management for this scheme is a little tricky.  We maintain an
 * invariant that says that a stream opened at a given save level always
 * uses a stream structure allocated at that level.  By doing this, we don't
 * need to keep track separately of streams open at a level vs. streams
 * allocated at a level.  To make this interact properly with save and
 * restore, we maintain a list of all streams allocated at this level, both
 * open and closed.  We store this list in the allocator: this is a hack,
 * but it simplifies bookkeeping (in particular, it guarantees the list is
 * restored properly by a restore).
 *
 * We want to close streams freed by restore and by garbage collection.  We
 * use the finalization procedure for this.  For restore, we don't have to
 * do anything special to make this happen.  For garbage collection, we do
 * something more drastic: we simply clear the list of known streams (at all
 * save levels).  Any streams open at the time of garbage collection will no
 * longer participate in the list of known streams, but this does no harm;
 * it simply means that they won't get reused, and can only be reclaimed by
 * a future garbage collection or restore.
 */

/*
 * Define the default stream buffer sizes.  For file streams,
 * this is arbitrary, since the C library or operating system
 * does its own buffering in addition.
 * However, a buffer size of at least 2K bytes is necessary to prevent
 * JPEG decompression from running very slow. When less than 2K, an
 * intermediate filter is installed that transfers 1 byte at a time
 * causing many aborted roundtrips through the JPEG filter code.
 */
#define DEFAULT_BUFFER_SIZE 2048
extern const uint file_default_buffer_size;

/* Make an invalid file object. */
void
make_invalid_file(i_ctx_t *i_ctx_p, ref * fp)
{
    make_file(fp, avm_invalid_file_entry, ~0, i_ctx_p->invalid_file_stream);
}

/* Check a file name for permission by stringmatch on one of the */
/* strings of the permitgroup array. */
static int
check_file_permissions_reduced(i_ctx_t *i_ctx_p, const char *fname, int len,
                        gx_io_device *iodev, const char *permitgroup)
{
    long i;
    ref *permitlist = NULL;
    /* an empty string (first character == 0) if '\' character is */
    /* recognized as a file name separator as on DOS & Windows    */
    const char *win_sep2 = "\\";
    bool use_windows_pathsep = (gs_file_name_check_separator(win_sep2, 1, win_sep2) == 1);
    uint plen = gp_file_name_parents(fname, len);

    /* we're protecting arbitrary file system accesses, not Postscript device accesses.
     * Although, note that %pipe% is explicitly checked for and disallowed elsewhere
     */
    if (iodev && iodev != iodev_default(imemory)) {
        return 0;
    }

    /* Assuming a reduced file name. */
    if (dict_find_string(&(i_ctx_p->userparams), permitgroup, &permitlist) <= 0)
        return 0;       /* if Permissions not found, just allow access */

    for (i=0; i<r_size(permitlist); i++) {
        ref permitstring;
        const string_match_params win_filename_params = {
                '*', '?', '\\', true, true      /* ignore case & '/' == '\\' */
        };
        const byte *permstr;
        uint permlen;
        int cwd_len = 0;

        if (array_get(imemory, permitlist, i, &permitstring) < 0 ||
            r_type(&permitstring) != t_string
           )
            break;      /* any problem, just fail */
        permstr = permitstring.value.bytes;
        permlen = r_size(&permitstring);
        /*
         * Check if any file name is permitted with "*".
         */
        if (permlen == 1 && permstr[0] == '*')
            return 0;           /* success */
        /*
         * If the filename starts with parent references,
         * the permission element must start with same number of parent references.
         */
        if (plen != 0 && plen != gp_file_name_parents((const char *)permstr, permlen))
            continue;
        cwd_len = gp_file_name_cwds((const char *)permstr, permlen);
        /*
         * If the permission starts with "./", absolute paths
         * are not permitted.
         */
        if (cwd_len > 0 && gp_file_name_is_absolute(fname, len))
            continue;
        /*
         * If the permission starts with "./", relative paths
         * with no "./" are allowed as well as with "./".
         * 'fname' has no "./" because it is reduced.
         */
        if (string_match( (const unsigned char*) fname, len,
                          permstr + cwd_len, permlen - cwd_len,
                use_windows_pathsep ? &win_filename_params : NULL))
            return 0;           /* success */
    }
    /* not found */
    return gs_error_invalidfileaccess;
}

/* Check a file name for permission by stringmatch on one of the */
/* strings of the permitgroup array */
static int
check_file_permissions(i_ctx_t *i_ctx_p, const char *fname, int len,
                        gx_io_device *iodev, const char *permitgroup)
{
    char fname_reduced[gp_file_name_sizeof];
    uint rlen = sizeof(fname_reduced);

    if (gp_file_name_reduce(fname, len, fname_reduced, &rlen) != gp_combine_success)
        return gs_error_invalidaccess;         /* fail if we couldn't reduce */
    return check_file_permissions_reduced(i_ctx_p, fname_reduced, rlen, iodev, permitgroup);
}

/* z_check_file_permissions: see zfile.h for explanation
 */
int
z_check_file_permissions(gs_memory_t *mem, const char *fname, const int len, const char *permission)
{
    i_ctx_t *i_ctx_p = get_minst_from_memory(mem)->i_ctx_p;
    gs_parsed_file_name_t pname;
    const char *permitgroup = permission[0] == 'r' ? "PermitFileReading" : "PermitFileWriting";
    int code = gs_parse_file_name(&pname, fname, len, imemory);
    if (code < 0)
        return code;

    if (pname.iodev && i_ctx_p->LockFilePermissions
         && strcmp(pname.iodev->dname, "%pipe%") == 0) {
        code = gs_note_error(gs_error_invalidfileaccess);
    }
    else {
        code = check_file_permissions(i_ctx_p, pname.fname, pname.len, pname.iodev, permitgroup);
    }
    return code;
}

/* <name_string> <access_string> file <file> */
int                             /* exported for zsysvm.c */
zfile(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    char file_access[4];
    gs_parsed_file_name_t pname;
    int code = parse_file_access_string(op, file_access);
    stream *s;

    if (code < 0)
        return code;
    code = parse_file_name(op-1, &pname, i_ctx_p->LockFilePermissions, imemory);
    if (code < 0)
        return code;
        /*
         * HACK: temporarily patch the current context pointer into the
         * state pointer for stdio-related devices.  See ziodev.c for
         * more information.
         */
    if (pname.iodev && pname.iodev->dtype == iodev_dtype_stdio) {
        bool statement = (strcmp(pname.iodev->dname, "%statementedit%") == 0);
        bool lineedit = (strcmp(pname.iodev->dname, "%lineedit%") == 0);
        if (pname.fname)
            return_error(gs_error_invalidfileaccess);
        if (statement || lineedit) {
            /* These need special code to support callouts */
            gx_io_device *indev = gs_findiodevice(imemory,
                                                  (const byte *)"%stdin", 6);
            stream *ins;
            if (strcmp(file_access, "r"))
                return_error(gs_error_invalidfileaccess);
            indev->state = i_ctx_p;
            code = (indev->procs.open_device)(indev, file_access, &ins, imemory);
            indev->state = 0;
            if (code < 0)
                return code;
            check_ostack(2);
            push(2);
            make_stream_file(op - 3, ins, file_access);
            make_bool(op-2, statement);
            make_int(op-1, 0);
            make_string(op, icurrent_space, 0, NULL);
            return zfilelineedit(i_ctx_p);
        }
        pname.iodev->state = i_ctx_p;
        code = (*pname.iodev->procs.open_device)(pname.iodev,
                                                 file_access, &s, imemory);
        pname.iodev->state = NULL;
    } else {
        if (pname.iodev == NULL)
            pname.iodev = iodev_default(imemory);
        code = zopen_file(i_ctx_p, &pname, file_access, &s, imemory);
    }
    if (code < 0)
        return code;
    if (s == NULL)
        return_error(gs_error_undefinedfilename);
    code = ssetfilename(s, op[-1].value.const_bytes, r_size(op - 1));
    if (code < 0) {
        sclose(s);
        return_error(gs_error_VMerror);
    }
    make_stream_file(op - 1, s, file_access);
    pop(1);
    return code;
}

/*
 * Files created with .tempfile permit some operations even if the
 * temp directory is not explicitly named on the PermitFile... path
 * The names 'SAFETY' and 'tempfiles' are defined by gs_init.ps
*/
static bool
file_is_tempfile(i_ctx_t *i_ctx_p, const uchar *fname, int len)
{
    ref *SAFETY;
    ref *tempfiles;
    ref kname;

    if (dict_find_string(systemdict, "SAFETY", &SAFETY) <= 0 ||
            dict_find_string(SAFETY, "tempfiles", &tempfiles) <= 0)
        return false;
    if (name_ref(imemory, fname, len, &kname, -1) < 0 ||
            dict_find(tempfiles, &kname, &SAFETY) <= 0)
        return false;
    return true;
}

static int
record_file_is_tempfile(i_ctx_t *i_ctx_p, const uchar *fname, int len, bool add)
{
    ref *SAFETY;
    ref *tempfiles;
    ref kname, bref;
    int code = 0;

    if (dict_find_string(systemdict, "SAFETY", &SAFETY) <= 0 ||
            dict_find_string(SAFETY, "tempfiles", &tempfiles) <= 0) {
        return 0;
    }
    if ((code = name_ref(imemory, fname, len, &kname, 1)) < 0) {
        return code;
    }
    make_bool(&bref, true);
    if (add)
        return idict_put(tempfiles, &kname, &bref);
    else
        return idict_undef(tempfiles, &kname);
}

/* ------ Level 2 extensions ------ */

/* <string> deletefile - */
static int
zdeletefile(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    gs_parsed_file_name_t pname;
    int code;
    bool is_temp = false;

    check_op(1);
    code = parse_real_file_name(op, &pname, imemory, "deletefile");
    if (code < 0)
        return code;
    if (pname.iodev == iodev_default(imemory)) {
        if ((code = check_file_permissions(i_ctx_p, pname.fname, pname.len,
                pname.iodev, "PermitFileControl")) < 0 &&
                 !(is_temp = file_is_tempfile(i_ctx_p, op->value.bytes, r_size(op)))) {
            return code;
        }
    }

    code = (*pname.iodev->procs.delete_file)(pname.iodev, pname.fname);

    if (code >= 0 && is_temp)
        code = record_file_is_tempfile(i_ctx_p, (unsigned char *)pname.fname, strlen(pname.fname), false);

    gs_free_file_name(&pname, "deletefile");
    if (code < 0)
        return code;
    pop(1);
    return 0;
}

/* <template> <proc> <scratch> filenameforall - */
static int file_continue(i_ctx_t *);
static int file_cleanup(i_ctx_t *);
static int
zfilenameforall(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    file_enum *pfen;
    gx_io_device *iodev = NULL;
    gs_parsed_file_name_t pname;
    int code = 0;

    check_op(3);
    check_write_type(*op, t_string);
    check_proc(op[-1]);
    check_read_type(op[-2], t_string);
    /* Push a mark, the iodev, devicenamelen, the scratch string, the enumerator, */
    /* and the procedure, and invoke the continuation. */
    check_estack(7);
    /* Get the iodevice */
    code = parse_file_name(op-2, &pname, i_ctx_p->LockFilePermissions, imemory);
    if (code < 0)
        return code;
    iodev = (pname.iodev == NULL) ? iodev_default(imemory) : pname.iodev;

    /* Check for several conditions that just cause us to return success */
    if (pname.len == 0 || iodev->procs.enumerate_files == iodev_no_enumerate_files) {
        pop(3);
        return 0;       /* no pattern, or device not found -- just return */
    }
    pfen = iodev->procs.enumerate_files(imemory, iodev, (const char *)pname.fname,
                pname.len);
    if (pfen == 0)
        return_error(gs_error_VMerror);
    push_mark_estack(es_for, file_cleanup);
    ++esp;
    make_istruct(esp, 0, iodev);
    ++esp;
    make_int(esp, r_size(op-2) - pname.len);
    *++esp = *op;
    ++esp;
    make_istruct(esp, 0, pfen);
    *++esp = op[-1];
    ref_stack_pop(&o_stack, 3);
    code = file_continue(i_ctx_p);
    return code;
}
/* Continuation operator for enumerating files */
static int
file_continue(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    es_ptr pscratch = esp - 2;
    file_enum *pfen = r_ptr(esp - 1, file_enum);
    int devlen = esp[-3].value.intval;
    gx_io_device *iodev = r_ptr(esp - 4, gx_io_device);
    uint len = r_size(pscratch);
    uint code;

    if (len < devlen) {
        esp -= 6;               /* pop proc, pfen, scratch, devlen, iodev , mark */
        return_error(gs_error_rangecheck);     /* not even room for device len */
    }

    do {
        memcpy((char *)pscratch->value.bytes, iodev->dname, devlen);
        code = iodev->procs.enumerate_next(imemory, pfen, (char *)pscratch->value.bytes + devlen,
                    len - devlen);
        if (code == ~(uint) 0) {    /* all done */
            esp -= 6;               /* pop proc, pfen, scratch, devlen, iodev , mark */
            return o_pop_estack;
        } else if (code > len - devlen) {      /* overran string */
            return_error(gs_error_rangecheck);
        }
        else if (iodev != iodev_default(imemory)
              || (check_file_permissions(i_ctx_p, (char *)pscratch->value.bytes, code + devlen, iodev, "PermitFileReading")) == 0) {
            push(1);
            ref_assign(op, pscratch);
            r_set_size(op, code + devlen);
            push_op_estack(file_continue);  /* come again */
            *++esp = pscratch[2];   /* proc */
            return o_push_estack;
        }
    } while(1);
}
/* Cleanup procedure for enumerating files */
static int
file_cleanup(i_ctx_t *i_ctx_p)
{
    gx_io_device *iodev = r_ptr(esp + 2, gx_io_device);

    iodev->procs.enumerate_close(imemory, r_ptr(esp + 5, file_enum));
    /* See bug #707007, gp_enumerate_file_close() explicitly frees the file enumerator */
    make_null(esp + 5);
    return 0;
}

/* <string1> <string2> renamefile - */
static int
zrenamefile(i_ctx_t *i_ctx_p)
{
    int code;
    os_ptr op = osp;
    gs_parsed_file_name_t pname1, pname2;

    check_op(2);
    code = parse_real_file_name(op, &pname2, imemory, "renamefile(to)");
    if (code < 0)
        return code;

    pname1.fname = 0;
    code = parse_real_file_name(op - 1, &pname1, imemory, "renamefile(from)");
    if (code >= 0) {
        gx_io_device *iodev_dflt = iodev_default(imemory);
        if (pname1.iodev != pname2.iodev ) {
            if (pname1.iodev == iodev_dflt)
                pname1.iodev = pname2.iodev;
            if (pname2.iodev == iodev_dflt)
                pname2.iodev = pname1.iodev;
        }
        if (pname1.iodev != pname2.iodev ||
            (pname1.iodev == iodev_dflt &&
                /*
                 * We require FileControl permissions on the source path
                 * unless it is a temporary file. Also, we require FileControl
                 * and FileWriting permissions to the destination file/path.
                 */
              ((check_file_permissions(i_ctx_p, pname1.fname, pname1.len,
                                        pname1.iodev, "PermitFileControl") < 0 &&
                  !file_is_tempfile(i_ctx_p, op[-1].value.bytes, r_size(op - 1))) ||
              (check_file_permissions(i_ctx_p, pname2.fname, pname2.len,
                                        pname2.iodev, "PermitFileControl") < 0 ||
              check_file_permissions(i_ctx_p, pname2.fname, pname2.len,
                                        pname2.iodev, "PermitFileWriting") < 0 )))) {
            code = gs_note_error(gs_error_invalidfileaccess);
        } else {
            code = (*pname1.iodev->procs.rename_file)(pname1.iodev,
                            pname1.fname, pname2.fname);
        }
    }
    gs_free_file_name(&pname2, "renamefile(to)");
    gs_free_file_name(&pname1, "renamefile(from)");
    if (code < 0)
        return code;
    pop(2);
    return 0;
}

/* <file> status <open_bool> */
/* <string> status <pages> <bytes> <ref_time> <creation_time> true */
/* <string> status false */
static int
zstatus(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;

    check_op(1);
    switch (r_type(op)) {
        case t_file:
            {
                stream *s;

                make_bool(op, (file_is_valid(s, op) ? 1 : 0));
            }
            return 0;
        case t_string:
            {
                gs_parsed_file_name_t pname;
                struct stat fstat;
                int code = parse_file_name(op, &pname,
                                           i_ctx_p->LockFilePermissions, imemory);
                if (code < 0) {
                    if (code == gs_error_undefinedfilename) {
                        make_bool(op, 0);
                        code = 0;
                    }
                    return code;
                }
                code = gs_terminate_file_name(&pname, imemory, "status");
                if (code < 0)
                    return code;
                if ((code = check_file_permissions(i_ctx_p, pname.fname, pname.len,
                                       pname.iodev, "PermitFileReading")) >= 0) {
                    code = (*pname.iodev->procs.file_status)(pname.iodev,
                                                       pname.fname, &fstat);
                }
                switch (code) {
                    case 0:
                        check_ostack(4);
                        /*
                         * Check to make sure that the file size fits into
                         * a PostScript integer.  (On some systems, long is
                         * 32 bits, but file sizes are 64 bits.)
                         */
                        push(4);
                        make_int(op - 4, stat_blocks(&fstat));
                        make_int(op - 3, fstat.st_size);
                        /*
                         * We can't check the value simply by using ==,
                         * because signed/unsigned == does the wrong thing.
                         * Instead, since integer assignment only keeps the
                         * bottom bits, we convert the values to double
                         * and then test for equality.  This handles all
                         * cases of signed/unsigned or width mismatch.
                         */
                        if ((double)op[-4].value.intval !=
                              (double)stat_blocks(&fstat) ||
                            (double)op[-3].value.intval !=
                              (double)fstat.st_size
                            )
                            return_error(gs_error_limitcheck);
                        make_int(op - 2, fstat.st_mtime);
                        make_int(op - 1, fstat.st_ctime);
                        make_bool(op, 1);
                        break;
                    case gs_error_undefinedfilename:
                        make_bool(op, 0);
                        code = 0;
                }
                gs_free_file_name(&pname, "status");
                return code;
            }
        default:
            return_op_typecheck(op);
    }
}

/* ------ Non-standard extensions ------ */

/* <executable_file> .execfile - */
static int
zexecfile(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;

    check_op(1);
    check_type_access(*op, t_file, a_executable | a_read | a_execute);
    check_estack(4);            /* cleanup, file, finish, file */
    push_mark_estack(es_other, execfile_cleanup);
    *++esp = *op;
    push_op_estack(execfile_finish);
    return zexec(i_ctx_p);
}
/* Finish normally. */
static int
execfile_finish(i_ctx_t *i_ctx_p)
{
    check_ostack(1);
    esp -= 2;
    execfile_cleanup(i_ctx_p);
    return o_pop_estack;
}
/* Clean up by closing the file. */
static int
execfile_cleanup(i_ctx_t *i_ctx_p)
{
    check_ostack(1);
    *++osp = esp[2];
    return zclosefile(i_ctx_p);
}

/* - .filenamelistseparator <string> */
static int
zfilenamelistseparator(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;

    push(1);
    make_const_string(op, avm_foreign | a_readonly, 1,
                      (const byte *)&gp_file_name_list_separator);
    return 0;
}

/* <name> .filenamesplit <dir> <base> <extension> */
static int
zfilenamesplit(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;

    check_op(1);
    check_read_type(*op, t_string);
/****** NOT IMPLEMENTED YET ******/
    return_error(gs_error_undefined);
}

/* <string> .libfile <file> true */
/* <string> .libfile <string> false */
int                             /* exported for zsysvm.c */
zlibfile(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    int code;
    byte cname[DEFAULT_BUFFER_SIZE];
    uint clen;
    gs_parsed_file_name_t pname;
    stream *s;
    gx_io_device *iodev_dflt;

    check_op(1); /* Enough arguments */
    check_ostack(2); /* Enough space for our results */
    code = parse_file_name(op, &pname, i_ctx_p->LockFilePermissions, imemory);
    if (code < 0)
        return code;
    iodev_dflt = iodev_default(imemory);
    if (pname.iodev == NULL)
        pname.iodev = iodev_dflt;
    if (pname.iodev != iodev_dflt) { /* Non-OS devices don't have search paths (yet). */
        code = zopen_file(i_ctx_p, &pname, "r", &s, imemory);
        if (s == NULL) code = gs_note_error(gs_error_undefinedfilename);
        if (code >= 0) {
            code = ssetfilename(s, op->value.const_bytes, r_size(op));
            if (code < 0) {
                sclose(s);
                return_error(gs_error_VMerror);
            }
        }
        if (code < 0) {
            push(1);
            make_false(op);
            return 0;
        }
        make_stream_file(op, s, "r");
    } else {
        ref fref;

        code = lib_file_open(i_ctx_p->lib_path, imemory, i_ctx_p, pname.fname, pname.len,
                             (char *)cname, sizeof(cname), &clen, &fref);
        if (code >= 0) {
            s = fptr(&fref);
            code = ssetfilename(s, cname, clen);
            if (code < 0) {
                sclose(s);
                return_error(gs_error_VMerror);
            }
        }
        if (code < 0) {
            if (code == gs_error_VMerror || code == gs_error_invalidfileaccess)
                return code;
            push(1);
            make_false(op);
            return 0;
        }
        ref_assign(op, &fref);
    }
    push(1);
    make_true(op);
    return 0;
}

/* A "simple" prefix is defined as a (possibly empty) string of
   alphanumeric, underscore, and hyphen characters. */
static bool
prefix_is_simple(const char *pstr)
{
    int i;
    char c;

    for (i = 0; (c = pstr[i]) != 0; i++) {
        if (!(c == '-' || c == '_' || (c >= '0' && c <= '9') ||
              (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')))
            return false;
    }
    return true;
}

/* <prefix|null> <access_string> .tempfile <name_string> <file> */
static int
ztempfile(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    const char *pstr;
    char fmode[4];
    char fmode_temp[4];
    int code;
    char *prefix = NULL;
    char *fname= NULL;
    uint fnlen;
    gp_file *sfile;
    stream *s;
    byte *buf, *sbody;

    check_op(2);
    code = parse_file_access_string(op, fmode_temp);
    if (code < 0)
        return code;
    prefix = (char *)gs_alloc_bytes(imemory, gp_file_name_sizeof, "ztempfile(prefix)");
    fname = (char *)gs_alloc_bytes(imemory, gp_file_name_sizeof, "ztempfile(fname)");
    if (!prefix || !fname) {
        code = gs_note_error(gs_error_VMerror);
        goto done;
    }

    snprintf(fmode, sizeof(fmode), "%s%s", fmode_temp, gp_fmode_binary_suffix);
    if (r_has_type(op - 1, t_null))
        pstr = gp_scratch_file_name_prefix;
    else {
        uint psize;

        check_read_type(op[-1], t_string);
        psize = r_size(op - 1);
        if (psize >= gp_file_name_sizeof) {
            code = gs_note_error(gs_error_rangecheck);
            goto done;
        }
        memcpy(prefix, op[-1].value.const_bytes, psize);
        prefix[psize] = 0;
        pstr = prefix;
    }

    if (gp_file_name_is_absolute(pstr, strlen(pstr))) {
        int plen = strlen(pstr);
        const char *sep = gp_file_name_separator();
        int seplen = strlen(sep);

        /* This should not be possible if gp_file_name_is_absolute is true I think
         * But let's avoid the problem.
         */
        if (plen < seplen)
            return_error(gs_error_Fatal);

        plen -= seplen;
        /* strip off the file name prefix, leave just the directory name
         * so we can check if we are allowed to write to it
         */
        for ( ; plen >=0; plen--) {
            if ( gs_file_name_check_separator(&pstr[plen], seplen, &pstr[plen]))
                break;
        }
        if (plen < 0)
            return_error(gs_error_Fatal);

        memcpy(fname, pstr, plen);
        fname[plen] = '\0';
        if (check_file_permissions(i_ctx_p, fname, strlen(fname),
                                   NULL, "PermitFileWriting") < 0) {
            code = gs_note_error(gs_error_invalidfileaccess);
            goto done;
        }
    } else if (!prefix_is_simple(pstr)) {
        code = gs_note_error(gs_error_invalidfileaccess);
        goto done;
    }

    s = file_alloc_stream(imemory, "ztempfile(stream)");
    if (s == 0) {
        code = gs_note_error(gs_error_VMerror);
        goto done;
    }
    buf = gs_alloc_bytes(imemory, file_default_buffer_size,
                         "ztempfile(buffer)");
    if (buf == 0) {
        code = gs_note_error(gs_error_VMerror);
        goto done;
    }
    sfile = gp_open_scratch_file(imemory, pstr, fname, fmode);
    if (sfile == 0) {
        gs_free_object(imemory, buf, "ztempfile(buffer)");
        code = gs_note_error(gs_error_invalidfileaccess);
        goto done;
    }
    fnlen = strlen(fname);
    sbody = ialloc_string(fnlen, ".tempfile(fname)");
    if (sbody == 0) {
        gs_free_object(imemory, buf, "ztempfile(buffer)");
        code = gs_note_error(gs_error_VMerror);
        goto done;
    }
    memcpy(sbody, fname, fnlen);
    file_init_stream(s, sfile, fmode, buf, file_default_buffer_size);
    code = ssetfilename(s, (const unsigned char*) fname, fnlen);
    if (code < 0) {
        gx_io_device *iodev_dflt = iodev_default(imemory);
        sclose(s);
        iodev_dflt->procs.delete_file(iodev_dflt, fname);
        ifree_string(sbody, fnlen, ".tempfile(fname)");
        code = gs_note_error(gs_error_VMerror);
        goto done;
    }
    make_string(op - 1, a_readonly | icurrent_space, fnlen, sbody);
    make_stream_file(op, s, fmode);
    code = record_file_is_tempfile(i_ctx_p, (unsigned char *)fname, fnlen, true);

done:
    if (prefix)
        gs_free_object(imemory, prefix, "ztempfile(prefix)");
    if (fname)
        gs_free_object(imemory, fname, "ztempfile(fname)");
    return code;
}

/* Return the filename used to open a file object
 * this is currently only used by the PDF interpreter to
 * get a filename corresponding to the PDF file being
 * executed. Since we always execute PDF files from disk
 * this will always be OK.
 */
static int zgetfilename(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    uint fnlen;
    gs_const_string pfname;
    stream *s;
    byte *sbody;
    int code;

    check_op(1);
    check_ostack(1);
    check_read_type(*op, t_file);

    s = (op)->value.pfile;

    code = sfilename(s, &pfname);
    if (code < 0) {
        pfname.size = 0;
    }

    fnlen = pfname.size;
    sbody = ialloc_string(fnlen, ".getfilename");
    if (sbody == 0) {
        code = gs_note_error(gs_error_VMerror);
        return code;
    }
    memcpy(sbody, pfname.data, fnlen);
    make_string(op, a_readonly | icurrent_space, fnlen, sbody);

    return 0;
}

static int zaddcontrolpath(i_ctx_t *i_ctx_p)
{
    int code;
    os_ptr op = osp;
    ref nsref;
    unsigned int n = -1;

    check_op(2);
    check_read_type(*op, t_string);
    check_type(op[-1], t_name);

    name_string_ref(imemory, op-1, &nsref);
    if (r_size(&nsref) == 17 &&
        strncmp((const char *)nsref.value.const_bytes,
                "PermitFileReading", 17) == 0) {
        n = gs_permit_file_reading;
    } else if (r_size(&nsref) == 17 &&
               strncmp((const char *)nsref.value.const_bytes,
                       "PermitFileWriting", 17) == 0) {
        n = gs_permit_file_writing;
    } else if (r_size(&nsref) == 17 &&
               strncmp((const char *)nsref.value.const_bytes,
                       "PermitFileControl", 17) == 0) {
        n = gs_permit_file_control;
    }

    if (n == -1)
        code = gs_note_error(gs_error_rangecheck);
    else if (gs_is_path_control_active(imemory))
        code = gs_note_error(gs_error_Fatal);
    else
        code = gs_add_control_path_len(imemory, n,
                                       (const char *)op[0].value.const_bytes,
                                       (size_t)r_size(&op[0]));
    pop(2);
    return code;
}

static int zactivatepathcontrol(i_ctx_t *i_ctx_p)
{
    gs_activate_path_control(imemory, 1);
    return 0;
}
static int zcurrentpathcontrolstate(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    push(1);
    if (gs_is_path_control_active(imemory)) {
        make_true(op);
    }
    else {
        make_false(op);
    }
    return 0;
}

/* ------ Initialization procedure ------ */

const op_def zfile_op_defs[] =
{
    {"1deletefile", zdeletefile},
    {"1.execfile", zexecfile},
    {"2file", zfile},
    {"3filenameforall", zfilenameforall},
    {"0.filenamelistseparator", zfilenamelistseparator},
    {"1.filenamesplit", zfilenamesplit},
    {"1.libfile", zlibfile},
    {"2renamefile", zrenamefile},
    {"1status", zstatus},
    {"2.tempfile", ztempfile},
                /* Internal operators */
    {"0%file_continue", file_continue},
    {"0%execfile_finish", execfile_finish},
    {"1.getfilename", zgetfilename},
    /* Control path operators */
    {"2.addcontrolpath", zaddcontrolpath},
    {"0.activatepathcontrol", zactivatepathcontrol},
    {"0.currentpathcontrolstate", zcurrentpathcontrolstate},
    op_def_end(0)
};

/* ------ File name parsing ------ */

/* Parse a file name into device and individual name. */
/* See gsfname.c for details. */
static int
parse_file_name(const ref * op, gs_parsed_file_name_t * pfn, bool safemode,
                gs_memory_t *memory)
{
    int code;

    check_read_type(*op, t_string);
    code = gs_parse_file_name(pfn, (const char *)op->value.const_bytes,
                              r_size(op), memory);
    if (code < 0)
        return code;
    /*
     * Check here for the %pipe device which is illegal when
     * LockFilePermissions is true. In the future we might want to allow
     * the %pipe device to be included on the PermitFile... paths, but
     * for now it is simply disallowed.
     */
    if (pfn->iodev && safemode && strcmp(pfn->iodev->dname, "%pipe%") == 0)
        return gs_error_invalidfileaccess;
    return code;
}

/* Parse a real (non-device) file name and convert to a C string. */
/* See gsfname.c for details. */
static int
parse_real_file_name(const ref *op, gs_parsed_file_name_t *pfn,
                     gs_memory_t *mem, client_name_t cname)
{
    check_read_type(*op, t_string);
    return gs_parse_real_file_name(pfn, (const char *)op->value.const_bytes,
                                   r_size(op), mem, cname);
}

/* Parse the access string for opening a file. */
/* [4] is for r/w, +, b, \0. */
static int
parse_file_access_string(const ref *op, char file_access[4])
{
    const byte *astr;

    check_read_type(*op, t_string);
    astr = op->value.const_bytes;
    switch (r_size(op)) {
        case 2:
            if (astr[1] != '+')
                return_error(gs_error_invalidfileaccess);
            file_access[1] = '+';
            file_access[2] = 0;
            break;
        case 1:
            file_access[1] = 0;
            break;
        default:
            return_error(gs_error_invalidfileaccess);
    }
    switch (astr[0]) {
        case 'r':
        case 'w':
        case 'a':
            break;
        default:
            return_error(gs_error_invalidfileaccess);
    }
    file_access[0] = astr[0];
    return 0;
}

/* ------ Stream opening ------ */

/*
 * Open a file specified by a parsed file name (which may be only a
 * device).
 */
int
zopen_file(i_ctx_t *i_ctx_p, const gs_parsed_file_name_t *pfn,
           const char *file_access, stream **ps, gs_memory_t *mem)
{
    gx_io_device *const iodev = pfn->iodev;
    int code = 0;

    if (pfn->fname == NULL) {     /* just a device */
        iodev->state = i_ctx_p;
        code = iodev->procs.open_device(iodev, file_access, ps, mem);
        iodev->state = NULL;
        return code;
    }
    else {                      /* file */
        iodev_proc_open_file((*open_file)) = iodev->procs.open_file;

        if (open_file == 0)
            open_file = iodev_os_open_file;
        /* Check OS files to make sure we allow the type of access */
        if (open_file == iodev_os_open_file) {
            code = check_file_permissions(i_ctx_p, pfn->fname, pfn->len, pfn->iodev,
                file_access[0] == 'r' ? "PermitFileReading" : "PermitFileWriting");

            if (code < 0 && !file_is_tempfile(i_ctx_p,
                                          (const uchar *)pfn->fname, pfn->len))
                return code;
        }
        return open_file(iodev, pfn->fname, pfn->len, file_access, ps, mem);
    }
}

/*
 * Define the file_open procedure for the %os% IODevice (also used, as the
 * default, for %pipe% and possibly others).
 */
static int
iodev_os_open_file(gx_io_device * iodev, const char *fname, uint len,
                   const char *file_access, stream ** ps, gs_memory_t * mem)
{
    return file_open_stream(fname, len, file_access,
                            file_default_buffer_size, ps,
                            iodev, iodev->procs.gp_fopen, mem);
}

/* Make a t_file reference to a stream. */
void
make_stream_file(ref * pfile, stream * s, const char *access)
{
    uint attrs =
        (access[1] == '+' ? a_write + a_read + a_execute : 0) |
        imemory_space((gs_ref_memory_t *) s->memory);

    if (access[0] == 'r') {
        make_file(pfile, attrs | (a_read | a_execute), s->read_id, s);
        s->write_id = 0;
    } else {
        make_file(pfile, attrs | a_write, s->write_id, s);
        s->read_id = 0;
    }
}

/* return zero for success, -ve for error, +1 for continue */
static int
lib_file_open_search_with_no_combine(gs_file_path_ptr  lib_path, const gs_memory_t *mem, i_ctx_t *i_ctx_p,
                                     const char *fname, uint flen, char *buffer, int blen, uint *pclen, ref *pfile,
                                     gx_io_device *iodev, bool starting_arg_file, char *fmode)
{
    stream *s;
    uint blen1 = blen;
    struct stat fstat;
    int code = 1;

    if (gp_file_name_reduce(fname, flen, buffer, &blen1) != gp_combine_success)
      goto skip;

    if (starting_arg_file || check_file_permissions(i_ctx_p, buffer, blen1, iodev, "PermitFileReading") >= 0) {
        if (iodev_os_open_file(iodev, (const char *)buffer, blen1,
                       (const char *)fmode, &s, (gs_memory_t *)mem) == 0) {
            *pclen = blen1;
            make_stream_file(pfile, s, "r");
            code = 0;
        }
    }
    else {
        /* If we are not allowed to open the file by check_file_permissions_aux()
         * and if the file exists, throw an error.......
         * Otherwise, keep searching.
         */
        if ((*iodev->procs.file_status)(iodev,  buffer, &fstat) >= 0) {
            code = gs_note_error(gs_error_invalidfileaccess);
        }
    }

 skip:
    return code;
}

/* return zero for success, -ve for error, +1 for continue */
static int
lib_file_open_search_with_combine(gs_file_path_ptr  lib_path, const gs_memory_t *mem, i_ctx_t *i_ctx_p,
                                  const char *fname, uint flen, char *buffer, int blen, uint *pclen, ref *pfile,
                                  gx_io_device *iodev, bool starting_arg_file, char *fmode)
{
    stream *s;
    const gs_file_path *pfpath = lib_path;
    uint pi;
    int code = 1;

    for (pi = 0; pi < r_size(&pfpath->list) && code == 1; ++pi) {
        const ref *prdir = pfpath->list.value.refs + pi;
        const char *pstr = (const char *)prdir->value.const_bytes;
        uint plen = r_size(prdir), blen1 = blen;
        gs_parsed_file_name_t pname;
        gp_file_name_combine_result r;

        /* We need to concatenate and parse the file name here
         * if this path has a %device% prefix.              */
        if (pstr[0] == '%') {
            /* We concatenate directly since gp_file_name_combine_*
             * rules are not correct for other devices such as %rom% */
            code = gs_parse_file_name(&pname, pstr, plen, mem);
            if (code < 0) {
                code = 1;
                continue;
            }
            if (blen < max(pname.len, plen) + flen)
            	return_error(gs_error_limitcheck);
            memcpy(buffer, pname.fname, pname.len);
            memcpy(buffer+pname.len, fname, flen);
            if (pname.iodev->procs.open_file == NULL) {
                code = 1;
                continue;
            }
            code = pname.iodev->procs.open_file(pname.iodev, buffer, pname.len + flen, fmode,
                                          &s, (gs_memory_t *)mem);
            if (code < 0) {
                code = 1;
                continue;
            }
            make_stream_file(pfile, s, "r");
            /* fill in the buffer with the device concatenated */
            memcpy(buffer, pstr, plen);
            memcpy(buffer+plen, fname, flen);
            *pclen = plen + flen;
            code = 0;
        } else {
            r = gp_file_name_combine(pstr, plen,
                    fname, flen, false, buffer, &blen1);
            if (r != gp_combine_success)
                continue;
            if (starting_arg_file || check_file_permissions(i_ctx_p, buffer,
                                      blen1, iodev, "PermitFileReading") >= 0) {

                if (iodev_os_open_file(iodev, (const char *)buffer, blen1,
                            (const char *)fmode, &s, (gs_memory_t *)mem) == 0) {
                    *pclen = blen1;
                    make_stream_file(pfile, s, "r");
                    code = 0;
                }
            }
            else {
                struct stat fstat;
                /* If we are not allowed to open the file by check_file_permissions_aux()
                 * and if the file exists, throw an error.......
                 * Otherwise, keep searching.
                 */
                if ((*iodev->procs.file_status)(iodev,  (const char *)buffer, &fstat) >= 0) {
                    code = gs_note_error(gs_error_invalidfileaccess);
                }
            }
        }
    }
    return code;
}

/* Return a file object of of the file searched for using the search paths. */
/* The fname cannot contain a device part (%...%) but the lib paths might. */
/* The startup code calls this to open the initialization file gs_init.ps. */
/* The startup code also calls this to open @-files. */
int
lib_file_open(gs_file_path_ptr  lib_path, const gs_memory_t *mem, i_ctx_t *i_ctx_p,
                       const char *fname, uint flen, char *buffer, int blen, uint *pclen, ref *pfile)
{   /* i_ctx_p is NULL running arg (@) files.
     * lib_path and mem are never NULL
     */
    bool starting_arg_file = (i_ctx_p == NULL) ? true : i_ctx_p->starting_arg_file;
    bool search_with_no_combine = false;
    bool search_with_combine = false;
    char fmode[2] = { 'r', 0};
    gx_io_device *iodev = iodev_default(mem);
    gs_main_instance *minst = get_minst_from_memory(mem);
    int code;

    if (i_ctx_p && starting_arg_file)
        i_ctx_p->starting_arg_file = false;

    /* when starting arg files (@ files) iodev_default is not yet set */
    if (iodev == 0)
        iodev = (gx_io_device *)gx_io_device_table[0];

    if (gp_file_name_is_absolute(fname, flen)) {
       search_with_no_combine = true;
       search_with_combine = false;
    } else {
       search_with_no_combine = starting_arg_file;
       search_with_combine = true;
    }
    if (minst->search_here_first) {
      if (search_with_no_combine) {
        code = lib_file_open_search_with_no_combine(lib_path, mem, i_ctx_p,
                                                    fname, flen, buffer, blen, pclen, pfile,
                                                    iodev, starting_arg_file, fmode);
        if (code <= 0) /* +ve means continue continue */
          return code;
      }
      if (search_with_combine) {
        code = lib_file_open_search_with_combine(lib_path, mem, i_ctx_p,
                                                 fname, flen, buffer, blen, pclen, pfile,
                                                 iodev, starting_arg_file, fmode);
        if (code <= 0) /* +ve means continue searching */
          return code;
      }
    } else {
      if (search_with_combine) {
        code = lib_file_open_search_with_combine(lib_path, mem, i_ctx_p,
                                                 fname, flen, buffer, blen, pclen, pfile,
                                                 iodev, starting_arg_file, fmode);
        if (code <= 0) /* +ve means continue searching */
          return code;
      }
      if (search_with_no_combine) {
        code = lib_file_open_search_with_no_combine(lib_path, mem, i_ctx_p,
                                                    fname, flen, buffer, blen, pclen, pfile,
                                                    iodev, starting_arg_file, fmode);
        if (code <= 0) /* +ve means continue searching */
          return code;
      }
    }
    return_error(gs_error_undefinedfilename);
}

/* The startup code calls this to open @-files. */
stream *
lib_sopen(const gs_file_path_ptr pfpath, const gs_memory_t *mem, const char *fname)
{
    /* We need a buffer to hold the expanded file name. */
    char filename_found[DEFAULT_BUFFER_SIZE];
    uint fnamelen;
    ref obj;
    int code;

    /* open the usual 'stream', then if successful, return the file */
    code = lib_file_open(pfpath, mem, NULL, fname, strlen(fname),
                            filename_found, sizeof(filename_found), &fnamelen, &obj);

    if (code < 0)
        return NULL;

    return((stream *)(obj.value.pfile));
}

/* Open a file stream that reads a string. */
/* (This is currently used only by the ccinit feature.) */
/* The string must be allocated in non-garbage-collectable (foreign) space. */
int
file_read_string(const byte *str, uint len, ref *pfile, gs_ref_memory_t *imem)
{
    stream *s = file_alloc_stream((gs_memory_t *)imem, "file_read_string");

    if (s == 0)
        return_error(gs_error_VMerror);
    sread_string(s, str, len);
    s->foreign = 1;
    s->write_id = 0;
    make_file(pfile, a_readonly | imemory_space(imem), s->read_id, s);
    s->save_close = s->procs.close;
    s->procs.close = file_close_disable;
    return 0;
}

/* Report an error by storing it in the stream's error_string. */
int
filter_report_error(stream_state * st, const char *str)
{
    if_debug1m('s', st->memory, "[s]stream error: %s\n", str);
    strncpy(st->error_string, str, STREAM_MAX_ERROR_STRING);
    /* Ensure null termination. */
    st->error_string[STREAM_MAX_ERROR_STRING] = 0;
    return 0;
}

/* Open a file stream for a filter. */
int
filter_open(const char *file_access, uint buffer_size, ref * pfile,
            const stream_procs * procs, const stream_template * templat,
            const stream_state * st, gs_memory_t *mem)
{
    stream *s;
    uint ssize = gs_struct_type_size(templat->stype);
    stream_state *sst = 0;
    int code;

    if (templat->stype != &st_stream_state) {
        sst = s_alloc_state(mem, templat->stype, "filter_open(stream_state)");
        if (sst == 0)
            return_error(gs_error_VMerror);
    }
    code = file_open_stream((char *)0, 0, file_access, buffer_size, &s,
                                (gx_io_device *)0, (iodev_proc_fopen_t)0, mem);
    if (code < 0) {
        gs_free_object(mem, sst, "filter_open(stream_state)");
        return code;
    }
    s_std_init(s, s->cbuf, s->bsize, procs,
               (*file_access == 'r' ? s_mode_read : s_mode_write));
    s->procs.process = templat->process;
    s->save_close = s->procs.close;
    s->procs.close = file_close_file;
    if (sst == 0) {
        /* This stream doesn't have any state of its own. */
        /* Hack: use the stream itself as the state. */
        sst = (stream_state *) s;
    } else if (st != 0)         /* might not have client parameters */
        memcpy(sst, st, ssize);
    s->state = sst;
    s_init_state(sst, templat, mem);
    sst->report_error = filter_report_error;

    if (templat->init != 0) {
        code = (*templat->init)(sst);
        if (code < 0) {
            gs_free_object(mem, sst, "filter_open(stream_state)");
            gs_free_object(mem, s->cbuf, "filter_open(buffer)");
            return code;
        }
    }
    make_stream_file(pfile, s, file_access);
    return 0;
}

/* Close a file object. */
/* This is exported only for gsmain.c. */
int
file_close(ref * pfile)
{
    stream *s;

    if (file_is_valid(s, pfile)) {      /* closing a closed file is a no-op */
        if (sclose(s))
            return_error(gs_error_ioerror);
    }
    return 0;
}
