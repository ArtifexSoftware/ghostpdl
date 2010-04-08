/* Copyright (C) 2001-2006 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

/* $Id$ */
/* IODevice implementation for Ghostscript */
#include "errno_.h"
#include "string_.h"
#include "unistd_.h"
#include "gx.h"
#include "gserrors.h"
#include "gp.h"
#include "gscdefs.h"
#include "gsfname.h"
#include "gsparam.h"
#include "gsstruct.h"
#include "gxiodev.h"

/* Import the IODevice table from gconf.c. */
extern_gx_io_device_table();

private_st_io_device();
gs_private_st_ptr(st_io_device_ptr, gx_io_device *, "gx_io_device *",
		  iodev_ptr_enum_ptrs, iodev_ptr_reloc_ptrs);
gs_private_st_element(st_io_device_ptr_element, gx_io_device *,
      "gx_io_device *[]", iodev_ptr_elt_enum_ptrs, iodev_ptr_elt_reloc_ptrs,
		      st_io_device_ptr);

/* Define the OS (%os%) device. */
iodev_proc_fopen(iodev_os_fopen);
iodev_proc_fclose(iodev_os_fclose);
static iodev_proc_delete_file(os_delete);
static iodev_proc_rename_file(os_rename);
static iodev_proc_file_status(os_status);
static iodev_proc_enumerate_files(os_enumerate);
static iodev_proc_get_params(os_get_params);
const gx_io_device gs_iodev_os =
{
    "%os%", "FileSystem",
    {iodev_no_init, iodev_no_open_device,
     NULL /*iodev_os_open_file */ , iodev_os_fopen, iodev_os_fclose,
     os_delete, os_rename, os_status,
     os_enumerate, gp_enumerate_files_next, gp_enumerate_files_close,
     os_get_params, iodev_no_put_params
    }
};

/* ------ Initialization ------ */

init_proc(gs_iodev_init);	/* check prototype */
int
gs_iodev_init(gs_memory_t * mem)
{				/* Make writable copies of all IODevices. */
    gx_io_device **table =
	gs_alloc_struct_array(mem, gx_io_device_table_count,
			      gx_io_device *, &st_io_device_ptr_element,
			      "gs_iodev_init(table)");
    gs_lib_ctx_t *libctx = gs_lib_ctx_get_interp_instance(mem);
    int i, j;
    int code = 0;

    if ((table == NULL) || (libctx == NULL))
	return_error(gs_error_VMerror);
    for (i = 0; i < gx_io_device_table_count; ++i) {
	gx_io_device *iodev =
	    gs_alloc_struct(mem, gx_io_device, &st_io_device,
			    "gs_iodev_init(iodev)");

	if (iodev == 0)
	    goto fail;
	table[i] = iodev;
	memcpy(table[i], gx_io_device_table[i], sizeof(gx_io_device));
    }
    libctx->io_device_table = table;
    code = gs_register_struct_root(mem, NULL,
                                   (void **)&libctx->io_device_table,
				   "io_device_table");
    if (code < 0)
	goto fail;
    /* Run the one-time initialization of each IODevice. */
    for (j = 0; j < gx_io_device_table_count; ++j)
	if ((code = (table[j]->procs.init)(table[j], mem)) < 0)
	    goto f2;
    return 0;
 f2:
    /****** CAN'T FIND THE ROOT ******/
    /*gs_unregister_root(mem, root, "io_device_table");*/
 fail:
    for (; i >= 0; --i)
	gs_free_object(mem, table[i - 1], "gs_iodev_init(iodev)");
    gs_free_object(mem, table, "gs_iodev_init(table)");
    libctx->io_device_table = 0;
    return (code < 0 ? code : gs_note_error(gs_error_VMerror));
}

/* ------ Default (unimplemented) IODevice procedures ------ */

int
iodev_no_init(gx_io_device * iodev, gs_memory_t * mem)
{
    return 0;
}

int
iodev_no_open_device(gx_io_device * iodev, const char *access, stream ** ps,
		     gs_memory_t * mem)
{
    return_error(gs_error_invalidfileaccess);
}

int
iodev_no_open_file(gx_io_device * iodev, const char *fname, uint namelen,
		   const char *access, stream ** ps, gs_memory_t * mem)
{
    return_error(gs_error_invalidfileaccess);
}

int
iodev_no_fopen(gx_io_device * iodev, const char *fname, const char *access,
	       FILE ** pfile, char *rfname, uint rnamelen)
{
    return_error(gs_error_invalidfileaccess);
}

int
iodev_no_fclose(gx_io_device * iodev, FILE * file)
{
    return_error(gs_error_ioerror);
}

int
iodev_no_delete_file(gx_io_device * iodev, const char *fname)
{
    return_error(gs_error_invalidfileaccess);
}

int
iodev_no_rename_file(gx_io_device * iodev, const char *from, const char *to)
{
    return_error(gs_error_invalidfileaccess);
}

int
iodev_no_file_status(gx_io_device * iodev, const char *fname, struct stat *pstat)
{
    return_error(gs_error_undefinedfilename);
}

file_enum *
iodev_no_enumerate_files(gx_io_device * iodev, const char *pat, uint patlen,
			 gs_memory_t * memory)
{
    return NULL;
}

int
iodev_no_get_params(gx_io_device * iodev, gs_param_list * plist)
{
    return 0;
}

int
iodev_no_put_params(gx_io_device * iodev, gs_param_list * plist)
{
    return param_commit(plist);
}

/* ------ %os% ------ */

/* The fopen routine is exported for %null. */
int
iodev_os_fopen(gx_io_device * iodev, const char *fname, const char *access,
	       FILE ** pfile, char *rfname, uint rnamelen)
{
    errno = 0;
    *pfile = gp_fopen(fname, access);
    if (*pfile == NULL)
	return_error(gs_fopen_errno_to_code(errno));
    if (rfname != NULL && rfname != fname)
	strcpy(rfname, fname);
    return 0;
}

/* The fclose routine is exported for %null. */
int
iodev_os_fclose(gx_io_device * iodev, FILE * file)
{
    fclose(file);
    return 0;
}

static int
os_delete(gx_io_device * iodev, const char *fname)
{
    return (unlink(fname) == 0 ? 0 : gs_error_ioerror);
}

static int
os_rename(gx_io_device * iodev, const char *from, const char *to)
{
    return (rename(from, to) == 0 ? 0 : gs_error_ioerror);
}

static int
os_status(gx_io_device * iodev, const char *fname, struct stat *pstat)
{				/* The RS/6000 prototype for stat doesn't include const, */
    /* so we have to explicitly remove the const modifier. */
    return (stat((char *)fname, pstat) < 0 ? gs_error_undefinedfilename : 0);
}

static file_enum *
os_enumerate(gx_io_device * iodev, const char *pat, uint patlen,
	     gs_memory_t * mem)
{
    return gp_enumerate_files_init(pat, patlen, mem);
}

static int
os_get_params(gx_io_device * iodev, gs_param_list * plist)
{
    int code;
    int i0 = 0, i2 = 2;
    bool btrue = true, bfalse = false;
    int BlockSize;
    long Free, LogicalSize;

    /*
     * Return fake values for BlockSize and Free, since we can't get the
     * correct values in a platform-independent manner.
     */
    BlockSize = 1024;
    LogicalSize = 2000000000 / BlockSize;	/* about 2 Gb */
    Free = LogicalSize * 3 / 4;			/* about 1.5 Gb */

    if (
	(code = param_write_bool(plist, "HasNames", &btrue)) < 0 ||
	(code = param_write_int(plist, "BlockSize", &BlockSize)) < 0 ||
	(code = param_write_long(plist, "Free", &Free)) < 0 ||
	(code = param_write_int(plist, "InitializeAction", &i0)) < 0 ||
	(code = param_write_bool(plist, "Mounted", &btrue)) < 0 ||
	(code = param_write_bool(plist, "Removable", &bfalse)) < 0 ||
	(code = param_write_bool(plist, "Searchable", &btrue)) < 0 ||
	(code = param_write_int(plist, "SearchOrder", &i2)) < 0 ||
	(code = param_write_bool(plist, "Writeable", &btrue)) < 0 ||
	(code = param_write_long(plist, "LogicalSize", &LogicalSize)) < 0
	)
	return code;
    return 0;
}

/* ------ Utilities ------ */

/* Get the N'th IODevice from the known device table. */
gx_io_device *
gs_getiodevice(const gs_memory_t *mem, int index)
{
    gs_lib_ctx_t *libctx = gs_lib_ctx_get_interp_instance(mem);

    if (libctx == NULL || libctx->io_device_table == NULL ||
        index < 0      || index >= gx_io_device_table_count)
	return 0;		/* index out of range */
    return libctx->io_device_table[index];
}

/* Look up an IODevice name. */
/* The name may be either %device or %device%. */
gx_io_device *
gs_findiodevice(const gs_memory_t *mem, const byte * str, uint len)
{
    int i;
    gs_lib_ctx_t *libctx = gs_lib_ctx_get_interp_instance(mem);

    if (len > 1 && str[len - 1] == '%')
	len--;
    for (i = 0; i < gx_io_device_table_count; ++i) {
	gx_io_device *iodev = libctx->io_device_table[i];
	const char *dname = iodev->dname;

	if (dname && strlen(dname) == len + 1 && !memcmp(str, dname, len))
	    return iodev;
    }
    return 0;
}

/* ------ Accessors ------ */

/* Get IODevice parameters. */
int
gs_getdevparams(gx_io_device * iodev, gs_param_list * plist)
{				/* All IODevices have the Type parameter. */
    gs_param_string ts;
    int code;

    param_string_from_string(ts, iodev->dtype);
    code = param_write_name(plist, "Type", &ts);
    if (code < 0)
	return code;
    return (*iodev->procs.get_params) (iodev, plist);
}

/* Put IODevice parameters. */
int
gs_putdevparams(gx_io_device * iodev, gs_param_list * plist)
{
    return (*iodev->procs.put_params) (iodev, plist);
}

/* Convert an OS error number to a PostScript error */
/* if opening a file fails. */
int
gs_fopen_errno_to_code(int eno)
{				/* Different OSs vary widely in their error codes. */
    /* We try to cover as many variations as we know about. */
    switch (eno) {
#ifdef ENOENT
	case ENOENT:
	    return_error(gs_error_undefinedfilename);
#endif
#ifdef ENOFILE
#  ifndef ENOENT
#    define ENOENT ENOFILE
#  endif
#  if ENOFILE != ENOENT
	case ENOFILE:
	    return_error(gs_error_undefinedfilename);
#  endif
#endif
#ifdef ENAMETOOLONG
	case ENAMETOOLONG:
	    return_error(gs_error_undefinedfilename);
#endif
#ifdef EACCES
	case EACCES:
	    return_error(gs_error_invalidfileaccess);
#endif
#ifdef EMFILE
	case EMFILE:
	    return_error(gs_error_limitcheck);
#endif
#ifdef ENFILE
	case ENFILE:
	    return_error(gs_error_limitcheck);
#endif
	default:
	    return_error(gs_error_ioerror);
    }
}

/* Generic interface for filesystem enumeration given a path that may	*/
/* include a %iodev% prefix */

typedef struct gs_file_enum_s gs_file_enum;
struct gs_file_enum_s {
    gs_memory_t *memory;
    gx_io_device *piodev;	/* iodev's are static, so don't need GC tracing */
    file_enum *pfile_enum;
    int prepend_iodev_name;
};

gs_private_st_ptrs1(st_gs_file_enum, gs_file_enum, "gs_file_enum",
		    gs_file_enum_enum_ptrs, gs_file_enum_reloc_ptrs, pfile_enum);

file_enum *
gs_enumerate_files_init(const char *pat, uint patlen, gs_memory_t * mem)
{
    file_enum *pfen;
    gs_file_enum *pgs_file_enum;
    gx_io_device *iodev = NULL;
    gs_parsed_file_name_t pfn;
    int code = 0;

    /* Get the iodevice */
    code = gs_parse_file_name(&pfn, pat, patlen, mem);
    if (code < 0)
	return NULL;
    iodev = (pfn.iodev == NULL) ? iodev_default(mem) : pfn.iodev;

    /* Check for several conditions that just cause us to return success */
    if (pfn.len == 0 || iodev->procs.enumerate_files == iodev_no_enumerate_files) {
        return NULL;	/* no pattern, or device not found -- just return */
    }
    pfen = iodev->procs.enumerate_files(iodev, (const char *)pfn.fname,
    		pfn.len, mem);
    if (pfen == 0)
	return NULL;
    pgs_file_enum = gs_alloc_struct(mem, gs_file_enum, &st_gs_file_enum,
			   "gs_enumerate_files_init");
    if (pgs_file_enum == 0)
	return NULL;
    pgs_file_enum->memory = mem;
    pgs_file_enum->piodev = iodev;
    pgs_file_enum->pfile_enum = pfen;
    pgs_file_enum->prepend_iodev_name = (pfn.iodev != NULL);
    return (file_enum *)pgs_file_enum;
}

uint
gs_enumerate_files_next(file_enum * pfen, char *ptr, uint maxlen)
{
    gs_file_enum *pgs_file_enum = (gs_file_enum *)pfen;
    int iodev_name_len = pgs_file_enum->prepend_iodev_name ?
			strlen(pgs_file_enum->piodev->dname) : 0;
    uint return_len;

    if (iodev_name_len > maxlen)
	return maxlen + 1;	/* signal overflow error */
    if (iodev_name_len > 0)
	memcpy(ptr, pgs_file_enum->piodev->dname, iodev_name_len);
    return_len = pgs_file_enum->piodev->procs.enumerate_next(pgs_file_enum->pfile_enum,
				ptr + iodev_name_len, maxlen - iodev_name_len);
    if (return_len == ~0) {
        gs_memory_t *mem = pgs_file_enum->memory;

        gs_free_object(mem, pgs_file_enum, "gs_enumerate_files_close");
	return ~0;
    }
    return return_len+iodev_name_len;
}

void
gs_enumerate_files_close(file_enum * pfen)
{
    gs_file_enum *pgs_file_enum = (gs_file_enum *)pfen;
    gs_memory_t *mem = pgs_file_enum->memory;

    pgs_file_enum->piodev->procs.enumerate_close(pgs_file_enum->pfile_enum);
    gs_free_object(mem, pgs_file_enum, "gs_enumerate_files_close");
}
