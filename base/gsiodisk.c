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


/* %disk*% IODevice implementation for Ghostscript */

/*
 * This file contains the sources for implmenting the 'flat' disk
 * file structure specified by Adobe.  This disk file structure is used
 * to implement the I/O devices refered to as %disk0% to %disk9%.
 *
 * These 'disks' are implemented as a set of directories of the
 * operating system.  The location of the directories on the operating
 * system disk structure is defined by the /Root I/O device key.  Thus
 * the following Postscript:
 *	mark /Root (/gs/disk0) (%disk0) .putdevparams
 * would use the /gs/disk0 directory for storing the data for the disk0
 * iodevice.
 *
 * Due to conflicts between file name formats (name lengths, restricted
 * characters, etc.) a 'mapping file' (map.txt) is used to translate
 * between a logical file name and a real file name on the system.
 * This name translation also flattens the directory structure of the
 * logical file names.  (This may be changed in the future.  But for
 * now it makes the implementation easier.)
 *
 * A special thanks is given for Noriyuki Matsushima for his creation of
 * the original version of the mapping file logic.
 */

/*
 * Mapping file:
 *
 * The mapping file is used to create a 'flat disk' file structure.  All file
 * are stored in a single directory.  To accomplish this, the logical file
 * name, including the path, are stored in a 'mapping file'.  For each file in
 * flat disk, there is a single line entry in the mapping file.  The mapping
 * file entry also includes a file number value.  This file number is used as
 * the actual file name in the flat disk directory.
 *
 * The flat disk format and the mapping file are used to eliminate problems
 * with trying to emulate a logical directory structure on different operating
 * systems which have different rules about what is allowed and not allowed
 * in a file name and path.
 *
 * Mapping file format:
 *
 * First line:
 * FileVersion<tab>1<tab>This file is machine generated. No not edit.<newline>
 *
 * Remaining lines:
 * <space>filenumber<tab>file_name<newline>
 *
 * The first line of the mapping file contains the text "FileVersion" followed
 * by a tab character. Then the number 1 (ascii - 31 hex) followed by a tab
 * and then a short message telling people to not edit the file.  This is an
 * attempt to keep the mapping file from being corrupted.
 *
 * The file version number must be a "1".  This is the only version currently
 * supported.  This line exists in case it is necessary to modify the mapping
 * file format at some future date.  The presence of a version number allows
 * for seamless upgrading of the file format.
 *
 * The format of the entry lines has been created so that the read entry
 * routine would:
 * 1. Grab the number (including the space if it has not already been read.)
 * 2. Grab the tab
 * 3. Grab everything up to a linefeed or carriage return as the file name.
 * 4. Grab any remaining linefeeds/carriage returns. Note this step might
 * grab the first character of the next line if there is only a single
 * carriage return or linefeed.  Since it is not desireable to have to worry
 * about doing an 'unget character', an extra space has been put at the
 * beginning of each line.  The extra logic with the linefeed/carriage return
 * is primarily there in case someone accidentally mangles the file by looking
 * at it with a text editor.
 *
 * This basically allows for any file name except one that includes
 * a zero, a carriage return, or a linefeed.  The requirement for excluding
 * zero is that there are too many C string routines that assume zero is
 * a string terminator (including much of the current Ghostscript code and
 * the current map handler).  The carriage return/linefeed requirement is
 * there simply because it is being used as a terminator for the file name.
 * It could easily be eliminated by adding a zero as the name terminator.
 * The only disadvantage is that the file is no longer totally printable ascii.
 */

#include "errno_.h"
#include "string_.h"
#include "unistd_.h"
#include "gx.h"
#include "gserrors.h"
#include "gp.h"
#include "gscdefs.h"
#include "gsparam.h"
#include "gsstruct.h"
#include "gxiodev.h"
#include "gsutil.h"

/* Function prototypes */
static iodev_proc_init(iodev_diskn_init);
static iodev_proc_finit(iodev_diskn_finit);
static iodev_proc_fopen(iodev_diskn_fopen);
static iodev_proc_delete_file(diskn_delete);
static iodev_proc_rename_file(diskn_rename);
static iodev_proc_file_status(diskn_status);
static iodev_proc_enumerate_files(diskn_enumerate_files);
static iodev_proc_enumerate_next(diskn_enumerate_next);
static iodev_proc_enumerate_close(diskn_enumerate_close);
static iodev_proc_get_params(diskn_get_params);
static iodev_proc_put_params(diskn_put_params);
iodev_proc_put_params(diskn_os_put_params);

/* Define a diskn (%diskn%) device macro */
#define diskn(varname,diskname) \
const gx_io_device varname = \
{ \
    diskname, "FileSystem", \
    {iodev_diskn_init, iodev_diskn_finit, iodev_no_open_device, \
     NULL /* no longer used */ , iodev_diskn_fopen, iodev_os_fclose, \
     diskn_delete, diskn_rename, diskn_status, \
     iodev_no_enumerate_files, /* Only until we have a root location */ \
     diskn_enumerate_next, diskn_enumerate_close, \
     diskn_get_params, diskn_put_params \
    }, \
    NULL, \
    NULL \
}

/*
 * Define the disk0-6 (%diskn%) devices. These devices are
 * used in the device list in gconf.c.
 */
diskn(gs_iodev_disk0,"%disk0%");
diskn(gs_iodev_disk1,"%disk1%");
diskn(gs_iodev_disk2,"%disk2%");
diskn(gs_iodev_disk3,"%disk3%");
diskn(gs_iodev_disk4,"%disk4%");
diskn(gs_iodev_disk5,"%disk5%");
diskn(gs_iodev_disk6,"%disk6%");
/* We could have more disks, but the DynaLab font installer */
/* has problems with more than 7 disks */
#undef diskn

typedef struct diskn_state_s {
    int root_size;	    /* size of root buffer */
    char * root;            /* root path pointer  */
    gs_memory_t * memory;   /* memory structure used */
} diskn_state;

gs_private_st_ptrs1(st_diskn_state, struct diskn_state_s, "diskn_state",
    diskn_state_enum_ptrs, diskn_state_reloc_ptrs, root);

#define MAP_FILE_NAME "map.txt"
#define TEMP_FILE_NAME "Tmp.txt"
#define MAP_FILE_VERSION 1
#define InitialNumber 0

typedef struct map_file_enum_s {
    gp_file     *stream;   /* stream to map file */
    char        *pattern;  /* pattern pointer    */
    char        *root;     /* root path pointer  */
    gs_memory_t *memory;   /* memory structure used */
} map_file_enum;

gs_private_st_ptrs2(st_map_file_enum, struct map_file_enum_s, "map_file_enum",
    map_file_enum_enum_ptrs, map_file_enum_reloc_ptrs, pattern, root);

static void * map_file_enum_init(gs_memory_t *, const char *, const char *, uint);
static bool map_file_enum_next(void *, char *);
static void map_file_enum_close(void *);
static bool map_file_name_get(gs_memory_t *, const char *, const char *, char *);
static void map_file_name_add(gs_memory_t *, const char *, const char *);
static void map_file_name_ren(gs_memory_t *, const char*, const char *, const char *);
static void map_file_name_del(gs_memory_t *, const char *, const char *);

static int
iodev_diskn_init(gx_io_device * iodev, gs_memory_t * mem)
{
    diskn_state * pstate = gs_alloc_struct(mem, diskn_state, &st_diskn_state,
                                                "iodev_diskn_init(state)");
    if (!pstate)
        return_error(gs_error_VMerror);
    pstate->root_size = 0;
    pstate->root = NULL;
    pstate->memory = mem;
    iodev->state = pstate;
    return 0;
}

static void
iodev_diskn_finit(gx_io_device * iodev, gs_memory_t * mem)
{
    gs_object_free(mem, iodev->state, "iodev_diskn_finit");
    iodev->state = NULL;
    return;
}

static int
iodev_diskn_fopen(gx_io_device * iodev, const char *fname, const char *access,
                  gp_file ** pfile, char *rfname, uint rnamelen)
{
    diskn_state * pstate = (diskn_state *)iodev->state;
    char *realname = (char *)gs_alloc_bytes(pstate->memory, gp_file_name_sizeof, "iodev_diskn_fopen(realname)");
    int code = 0;

    if (realname == NULL) {
        code = gs_note_error(gs_error_VMerror);
	goto done;
    }

    /* Exit if we do not have a root location */
    if (!pstate->root) {
        code = gs_note_error(gs_error_undefinedfilename);
	goto done;
    }

    /* Remap filename (if it exists). */
    if (!map_file_name_get(pstate->memory, (char *)pstate->root, fname, realname)) {
        if (strchr(access, 'w')) {
            map_file_name_add(pstate->memory, pstate->root, fname);
            map_file_name_get(pstate->memory, pstate->root, fname, realname);
        }
        else {
            code = gs_note_error(gs_error_undefinedfilename);
	    goto done;
	}
    }

    code = iodev_os_gp_fopen(iodev_default(pstate->memory), realname, access, pfile, rfname, rnamelen);

done:
    if (realname != NULL)
        gs_free_object(pstate->memory, realname, "iodev_diskn_fopen(realname)");

    return(code);
}

static int
diskn_delete(gx_io_device * iodev, const char *fname)
{
    diskn_state * pstate = (diskn_state *)iodev->state;
    char *realname = (char *)gs_alloc_bytes(pstate->memory, gp_file_name_sizeof, "diskn_delete(realname)");
    int code = 0;

    if (realname == NULL) {
        code = gs_note_error(gs_error_VMerror);
	goto done;
    }

    /* Exit if we do not have a root location */
    if (!pstate->root) {
        code = gs_note_error(gs_error_undefinedfilename);
        goto done;
    }

    /* Map filename (if it exists). */
    if (!map_file_name_get(pstate->memory, (char *)pstate->root, fname, realname)) {
        code = gs_note_error(gs_error_undefinedfilename);
        goto done;
    }

    map_file_name_del(pstate->memory, (char *)pstate->root, fname);
    code = (unlink(realname) == 0 ? 0 : gs_note_error(gs_error_ioerror));

done:
    if (realname != NULL)
        gs_free_object(pstate->memory, realname, "diskn_delete(realname)");

    return(code);
}

static int
diskn_rename(gx_io_device * iodev, const char *from, const char *to)
{
    diskn_state * pstate = (diskn_state *)iodev->state;
    char *toreal = (char *)gs_alloc_bytes(pstate->memory, gp_file_name_sizeof, "diskn_rename(toreal)");
    int code = 0;

    if (toreal == NULL) {
        code = gs_note_error(gs_error_VMerror);
	goto done;
    }

    /* Exit if we do not have a root location */
    if (!pstate->root) {
        code = gs_note_error(gs_error_undefinedfilename);
        goto done;
    }

    /* if filenames are the same them we are done. */
    if (strcmp(to, from) == 0) {
        code = 0;
        goto done;
    }

    /*
     * If the destination file already exists, then we want to delete it.
     */
    if (map_file_name_get(pstate->memory, (char *)pstate->root, to, toreal)) {
        map_file_name_del(pstate->memory, (char *)pstate->root, to);
        code = unlink(toreal) == 0 ? 0 : gs_note_error(gs_error_ioerror);
    }

    map_file_name_ren(pstate->memory, (char *)pstate->root, from, to);

done:
    if (toreal != NULL)
        gs_free_object(pstate->memory, toreal, "diskn_rename(toreal)");

    return(code);
}

static int
diskn_status(gx_io_device * iodev, const char *fname, struct stat *pstat)
{
    diskn_state * pstate = (diskn_state *)iodev->state;
    char *realname = (char *)gs_alloc_bytes(pstate->memory, gp_file_name_sizeof, "diskn_status(realname)");
    int code = 0;

    if (realname == NULL) {
        code = gs_note_error(gs_error_VMerror);
	goto done;
    }

    /* Exit if we do not have a root location */
    if (!pstate->root) {
        code = gs_note_error(gs_error_undefinedfilename);
        goto done;
    }

    /* Map filename (if it exists). */
    if (!map_file_name_get(pstate->memory, (char *)pstate->root, fname, realname)) {
        code = gs_note_error(gs_error_undefinedfilename);
    }

    code = stat((char *)realname, pstat) < 0 ? gs_note_error(gs_error_undefinedfilename) : 0;

done:
    if (realname != NULL)
        gs_free_object(pstate->memory, realname, "diskn_status(realname)");

    return(code);
}

static file_enum *
diskn_enumerate_files(gs_memory_t * mem, gx_io_device * iodev, const char *pat,
                      uint patlen)
{
    diskn_state * pstate = (diskn_state *)iodev->state;

    return (file_enum *)map_file_enum_init(mem, (char *)pstate->root, pat, patlen);
}

static void
diskn_enumerate_close(gs_memory_t * mem, file_enum *pfen)
{
    (void)mem;
    map_file_enum_close((void *)pfen);
}

static uint
diskn_enumerate_next(gs_memory_t * mem, file_enum *pfen, char *ptr, uint maxlen)
{
    (void)mem;
    if (map_file_enum_next((void *)pfen, ptr))
        return strlen(ptr);
    /* If we did not find a file then clean up */
    diskn_enumerate_close(pfen);
    return ~(uint) 0;
}

static int
diskn_get_params(gx_io_device * iodev, gs_param_list * plist)
{
    int code;
    int i0 = 0, so = 1;
    bool btrue = true, bfalse = false;
    diskn_state * pstate = (diskn_state *)iodev->state;
    bool bsearch = pstate->root != 0;
    int BlockSize;
    long Free, LogicalSize;
    gs_param_string rootstring;

    /*
     * Return fake values for BlockSize and Free, since we can't get the
     * correct values in a platform-independent manner.
     */
    BlockSize = 1024;
    LogicalSize = bsearch ? 2000000000 / BlockSize : 0;	/* about 2 Gb */
    Free = LogicalSize * 3 / 4;			/* about 1.5 Gb */

    if (
        (code = param_write_bool(plist, "HasNames", &btrue)) < 0 ||
        (code = param_write_int(plist, "BlockSize", &BlockSize)) < 0 ||
        (code = param_write_long(plist, "Free", &Free)) < 0 ||
        (code = param_write_int(plist, "InitializeAction", &i0)) < 0 ||
        (code = param_write_bool(plist, "Mounted", &bsearch)) < 0 ||
        (code = param_write_bool(plist, "Removable", &bfalse)) < 0 ||
        (code = param_write_bool(plist, "Searchable", &bsearch)) < 0 ||
        (code = param_write_int(plist, "SearchOrder", &so)) < 0 ||
        (code = param_write_bool(plist, "Writeable", &bsearch)) < 0 ||
        (code = param_write_long(plist, "LogicalSize", &LogicalSize)) < 0
        )
        return code;

    if (pstate->root) {
        rootstring.data = (const byte *)pstate->root;
        rootstring.size = strlen(pstate->root);
        rootstring.persistent = false;
        return param_write_string(plist, "Root", &rootstring);
    }
    else {
        return param_write_null(plist, "Root");
    }
}

static int
diskn_put_params(gx_io_device *iodev, gs_param_list *plist)
{
    gs_param_string rootstr;
    int code;
    diskn_state * pstate = (diskn_state *)iodev->state;

    switch (code = param_read_string(plist, "Root", &rootstr)) {
        case 0:
            break;
        default:
            param_signal_error(plist, "Root", code);
        case 1:
            rootstr.data = 0;
            break;
    }

    /* Process the other device parameters */
    code = iodev_no_put_params(iodev, plist);
    if (code < 0)
        return code;

    /* Process parameter changes */

    if (rootstr.data) {
        /* Make sure that we have room for the root string */
        if (!pstate->root || pstate->root_size <= rootstr.size) {
            if (pstate->root)	/* The current storge is too small */
                gs_free_object(pstate->memory, pstate->root, "diskn(rootdir)");
            pstate->root = (char *)gs_alloc_byte_array(pstate->memory,
                        gp_file_name_sizeof, sizeof(char), "diskn(rootdir)");
            if (!pstate->root)
                return_error(gs_error_VMerror);
            pstate->root_size = rootstr.size + 1;
            /* Now allow enumeration of files on the disk */
            iodev->procs.enumerate_files = diskn_enumerate_files;
        }

        memcpy(pstate->root, rootstr.data, rootstr.size);
        pstate->root[rootstr.size] = 0;
    }
    return 0;
}

/*
 * Open the specified file with specified attributes.
 *
 * rootpath - Path to base disk location.
 * filename - File name string
 * attributes - File attributes string
 * Returns - NULL if error, file structure pointer if no error
 */
static gp_file *
MapFileOpen(gs_memory_t *mem, const char * rootpath, const char * filename, const char * attributes)
{
    char *fullname = NULL;
    gp_file *f = NULL;
    int totlen = strlen(rootpath) + strlen(filename) + 1;

    if (totlen >= gp_file_name_sizeof)
        return NULL;

    fullname = (char *)gs_alloc_bytes(mem, totlen, "MapFileOpen(fullname)");
    if (fullname) {

        gs_snprintf(fullname, totlen, "%s%s", rootpath, filename);
        f = gp_fopen(fullname, attributes);

        gs_free_object(mem, fullname , "MapFileOpen(fullname)");
    }

    return(f);
}

/*
 * Read map file version (first line)
 *
 * mapfile - Mapping file structure pointer
 * value - pointer to version number storage location
 * Returns 1 if data read, else 0
 */
static int
MapFileReadVersion(gp_file * mapfile, int * value)
{
    int code = fscanf(mapfile, "FileVersion\t%d\t", value) == 1 ? 1 : 0;
    int c;

    /* Skip comment on version line. */
    do {
        c = fgetc(mapfile);
    } while (c != EOF && c != '\n' && c != '\r');

    /* Clean up any trailing linefeeds or carriage returns */
    while (c != EOF && (c == '\n' || c == '\r')) {
        c = fgetc(mapfile);
    }
    return code;
}

/*
 * Write a map file version (first line) into the map file
 *
 * stream - File structure pointer
 * value - version number
 */
static void
MapFileWriteVersion(gp_file * mapfile, int value)
{
    fprintf(mapfile,
        "FileVersion\t%d\tThis file is machine generated.  Do not edit.\n",
        value);
}

/*
 * Read an entry in the map file
 *
 * mapfile - Mapping file structure pointer
 * namebuf - Buffer for the file name storage
 * value - pointer to file number storage location
 * Returns 1 if data read, else 0
 */
static int
MapFileRead(gp_file * mapfile, char * namebuf, int * value)
{
    int count = 0;
    int c;

    /* Get the file number */
    if (fscanf(mapfile, "%d\t", value) != 1)
        return 0;

    /* Get the file name */
    do {
        namebuf[count++] = c = fgetc(mapfile);
    } while (count < gp_file_name_sizeof && c != EOF && c != '\n' && c != '\r');
    namebuf[--count] = 0;    /* Terminate file name */

    /* Clean up any trailing linefeeds or carriage returns */
    while (c != EOF && (c == '\n' || c == '\r')) {
        c = fgetc(mapfile);
    }

    return count != 0 ? 1: 0;
}

/*
 * Write an entry in the map file
 *
 * stream - File structure pointer
 * namebuf - Buffer for the file name storage
 * value - file number
 */
static void
MapFileWrite(gp_file * mapfile, const char * namebuf, int value)
{
    fprintf(mapfile, " %d\t%s\n", value, namebuf);
}

/*
 * Remove the specified file
 *
 * rootpath - Path to base disk location.
 * filename - File name string
 */
static void
MapFileUnlink(gs_memory_t *mem, const char * rootpath, const char * filename)
{
    char *fullname = NULL;
    int totlen = strlen(rootpath) + strlen(filename) + 1;

    if (totlen >= gp_file_name_sizeof)
        return;
    fullname = (char *)gs_alloc_bytes(mem, totlen, "MapFileUnlink(fullname)");
    if (fullname) {
        gs_snprintf(fullname, totlen, "%s%s", rootpath, filename);

        unlink(fullname);

        gs_free_object(mem, fullname , "MapFileUnlink(fullname)");
    }
}

/*
 * Rename the specified file to new specified name
 *
 * rootpath - Path to base disk location.
 * oldfilename - Old file name string
 * newfilename - New file name string
 */
static void
MapFileRename(gs_memory_t *mem, const char * rootpath, const char * newfilename, const char * oldfilename)
{
    char *oldfullname = NULL;
    char *newfullname = NULL;
    int ototlen = strlen(rootpath) + strlen(oldfilename) + 1;
    int ntotlen = strlen(rootpath) + strlen(newfilename) + 1;

    if (ototlen >= gp_file_name_sizeof)
        return;
    if (ntotlen >= gp_file_name_sizeof)
        return;
    oldfullname = (char *)gs_alloc_bytes(mem, ototlen, "MapFileRename(oldfullname)");
    newfullname = (char *)gs_alloc_bytes(mem, ntotlen, "MapFileRename(newfullname)");

    if (oldfullname && newfullname) {
        gs_snprintf(oldfullname, ototlen, "%s%s", rootpath, oldfilename);
        gs_snprintf(newfullname, ntotlen, "%s%s", rootpath, newfilename);
        rename(oldfullname, newfullname);
    }

    gs_free_object(mem, oldfullname , "MapFileRename(oldfullname)");
    gs_free_object(mem, newfullname , "MapFileRename(newfullname)");
}

/*
 * MapToFile
 *
 * Search for mapped number from map file with requied name. If same name exist,
 * first (lowest number) will be returned.
 * rootpath        char*   string of the base path where in disk0 reside (C:\PS\Disk0\)
 * name            char*   string to search pattern for full match (font\Ryumin-Light)
 * returns	    -1 if file not found, file number if found.
 */
static int
MapToFile(gs_memory_t *mem, const char* rootpath, const char* name)
{
    gp_file * mapfile;
    int d = -1;
    char *filename = NULL;
    int file_version;

    mapfile = MapFileOpen(mem, rootpath, MAP_FILE_NAME, "r");
    if (mapfile == NULL)
        return -1;

    /* Verify the mapping file version number */

    if (MapFileReadVersion(mapfile, &file_version)
        && file_version == MAP_FILE_VERSION) {

        filename = (char *)gs_alloc_bytes(mem, gp_file_name_sizeof, "MapToFile(filename)");
        /* Scan the file looking for the given name */

        while (filename && MapFileRead(mapfile, filename, &d)) {
            if (strcmp(filename, name) == 0)
                break;
            d = -1;
        }
        gs_free_object(mem, filename, "MapToFile(filename)");
    }
    fclose(mapfile);
    return d;
}

/*
 * map_file_enum_init
 *
 * create enumiate structure for a mapped file and fill pattern for search
 * root_name       char*   string of the base path where in disk0 reside (e.g. C:\PS\Disk0\)
 * search_pattern  char*   string to search pattern (font\*) or full match
 *                          (e.g. font\Ryumin-Light) or NULL
 *                          NULL means all files
 * Returns:		    NULL if error, else pointer to enumeration structure.
 */
static void *
map_file_enum_init(gs_memory_t *mem, const char * root_name, const char * search_pattern, uint search_pattern_len)
{
    int file_version;
    map_file_enum * mapfileenum = gs_alloc_struct(mem, map_file_enum, &st_map_file_enum,
                                                        "diskn:enum_init(file_enum)");

    if (mapfileenum == NULL)
        return NULL;
    memset(mapfileenum, 0, sizeof(map_file_enum));
    mapfileenum->memory = mem;

    if (search_pattern) {
        mapfileenum->pattern = (char *)gs_alloc_bytes(mem, search_pattern_len + 1,
                                                        "diskn:enum_init(pattern)");
        if (mapfileenum->pattern == NULL) {
            map_file_enum_close((file_enum *) mapfileenum);
            return NULL;
        }
        memcpy(mapfileenum->pattern, search_pattern, search_pattern_len);
        /* Terminate string */
        mapfileenum->pattern[search_pattern_len] = '\0';
    }

    mapfileenum->root = (char *)gs_alloc_bytes(mem, strlen(root_name) + 1,
                                                "diskn:enum_init(root)");
    if (mapfileenum->root == NULL) {
        map_file_enum_close((file_enum *) mapfileenum);
        return NULL;
    }

    if (strlen(root_name) >= gp_file_name_sizeof)
        return NULL;
    strcpy(mapfileenum->root, root_name);
    mapfileenum->stream = MapFileOpen(mem, root_name, MAP_FILE_NAME, "r");

    /* Check the mapping file version number */
    if (mapfileenum->stream != NULL
        && (!MapFileReadVersion(mapfileenum->stream, &file_version)
            || file_version != MAP_FILE_VERSION)) {
        fclose(mapfileenum->stream);   /* Invalid file version */
        mapfileenum->stream = NULL;
    }

    return mapfileenum;
}

/*
 * map_file_enum_next
 *
 * enum_mem        void*   pointer for map file enum structure
 * search_pattern  char*   string array for next target
 */
static bool
map_file_enum_next(void * enum_mem, char* target)
{
    int d = -1;
    map_file_enum * mapfileenum;

    if (enum_mem == NULL)
        return false;

    mapfileenum = (map_file_enum*)enum_mem;
    if (mapfileenum->stream == NULL)
        return false;

    if (mapfileenum->pattern) {
        /*  Search for next entry that matches pattern */
        while (MapFileRead(mapfileenum->stream, target, &d)) {
            if (string_match((byte *)target, strlen(target),
                             (byte *)mapfileenum->pattern,
                             strlen(mapfileenum->pattern), 0))
                return true;
        }
    }
    else {
        /*  Just get next */
        if (MapFileRead(mapfileenum->stream, target, &d))
            return true;
    }
    return false;
}

/*
 * map_file_enum_close
 *
 * cleans up after an enumeration, this may only be called
 * if map_file_enum_init did not fail
 */
static void
map_file_enum_close(void * enum_mem)
{
    map_file_enum * mapfileenum = (map_file_enum *) enum_mem;
    gs_memory_t * mem = mapfileenum->memory;

    if (mapfileenum->stream)
        fclose(mapfileenum->stream);
    if (mapfileenum->root)
        gs_free_object(mem, mapfileenum->root, "diskn_enum_init(root)");
    if (mapfileenum->pattern)
        gs_free_object(mem, mapfileenum->pattern, "diskn_enum_init(pattern)");
    gs_free_object(mem, mapfileenum, "diskn_enum_init(mapfileenum)");
}

/*
 * map_file_name_get
 *
 * maps the psname(Fname) to the osname using concatening the root_name and id
 * Id will be a lowest one if same psname exists more than one in map file. See MapToFile
 * for detail.
 * root_name       char*   string of the base path where in disk0 reside
 *                         (e.g.C:\PS\Disk0\)
 * Fname           char*   name of the entry to find in the map
 * osname          char*   resulting os specific path to the file
 */
static bool
map_file_name_get(gs_memory_t *mem, const char * root_name, const char * Fname, char * osname)
{
    int d = MapToFile(mem, root_name, Fname);

    if (d != -1) {
        /* 20 characters are enough for even a 64 bit integer */
        if ((strlen(root_name) + 20) < gp_file_name_sizeof) {
            gs_snprintf(osname, gp_file_name_sizeof, "%s%d", root_name, d);
            return true;
        }
    }

    *osname = 0;
    return false;
}

/*
 * map_file_name_del
 *
 * Deletes Fname from the mapping table and does not delete the actual file
 * If same Fname exists, all same Fname will be deleted
 * root_name       char*   string of the base path where in disk0 reside (C:\PS\Disk0\)
 * Fname           char*   name of the entry to add to the map
 */
static void
map_file_name_del(gs_memory_t *mem, const char * root_name, const char * Fname)
{
    /*  search for target entry */
    int d = MapToFile(mem, root_name, Fname);
    int file_version;
    char *name = NULL;

    name = (char *)gs_alloc_bytes(mem, gp_file_name_sizeof, "map_file_name_del(name)");

    if (name && d != -1) {			 /* if the file exists ... */
        FILE*   newMap;
        FILE*   oldMap;

        /* Open current map file and a working file */

        MapFileUnlink(mem, root_name, TEMP_FILE_NAME );
        newMap = MapFileOpen(mem, root_name, TEMP_FILE_NAME, "w");
        if (newMap == NULL) {
            gs_free_object(mem, name , "map_file_name_del(name)");
            return;
        }
        oldMap = MapFileOpen(mem, root_name, MAP_FILE_NAME, "r");
        if (oldMap != NULL && (!MapFileReadVersion(oldMap, &file_version)
            || file_version != MAP_FILE_VERSION)) {
            fclose(oldMap);
            oldMap= NULL;
        }
        if (oldMap == NULL) {
            gs_free_object(mem, name , "map_file_name_del(name)");
            fclose(newMap);
            MapFileUnlink(mem, root_name, TEMP_FILE_NAME);
            return;
        }

        /* Copy every line of the map file except the one with given name */

        MapFileWriteVersion(newMap, MAP_FILE_VERSION);
        while (MapFileRead(oldMap, name, &d))
            if (strcmp(name, Fname))
                MapFileWrite(newMap, name, d);
        fclose(newMap);
        fclose(oldMap);
        MapFileUnlink(mem, root_name, MAP_FILE_NAME);
        MapFileRename(mem, root_name, MAP_FILE_NAME, TEMP_FILE_NAME);
    }
    gs_free_object(mem, name, "map_file_name_del(name)");
}

/*
 * map_file_add
 *
 * adds Fname to the mapping table and does not create an unique new empty file
 * If same Fname exists, new Fname will not be added.
 * root_name       char*   string of the base path where in disk0 reside (C:\PS\Disk0\)
 * Fname           char*   name of the entry to add to the map
 */
static void
map_file_name_add(gs_memory_t *mem, const char * root_name, const char * Fname)
{
    /*
     * add entry to map file
     * entry number is one greater than biggest number
     */
    char *name = NULL;
    int d;
    int dmax = -1;
    int file_version;
    FILE*   newMap;
    FILE*   oldMap;

    name = (char *)gs_alloc_bytes(mem, gp_file_name_sizeof, "map_file_name_add(name)");
    if (name) {
        oldMap = MapFileOpen(mem, root_name, MAP_FILE_NAME, "r");
        if (oldMap != NULL && (!MapFileReadVersion(oldMap, &file_version)
            || file_version != MAP_FILE_VERSION)) {
            fclose(oldMap);
            oldMap = NULL;
        }
        if (oldMap == NULL) {
            oldMap = MapFileOpen(mem, root_name, MAP_FILE_NAME, "w");
            if (!oldMap)
                return;
            MapFileWriteVersion(oldMap, MAP_FILE_VERSION);
            MapFileWrite(oldMap, Fname, InitialNumber);
            fclose(oldMap);
        }
        else {
            MapFileUnlink(mem, root_name, TEMP_FILE_NAME);
            newMap = MapFileOpen(mem, root_name, TEMP_FILE_NAME, "w");
            if (newMap != NULL) {
                MapFileWriteVersion(newMap, MAP_FILE_VERSION);
                while (MapFileRead(oldMap, name, &d)) {
                    MapFileWrite(newMap, name, d);
                    if (dmax < d)
                        dmax = d;
                }

                dmax += 1;
                MapFileWrite(newMap, Fname, dmax);
                fclose(newMap);
                fclose(oldMap);
                MapFileUnlink(mem, root_name, MAP_FILE_NAME);
                MapFileRename(mem, root_name, MAP_FILE_NAME, TEMP_FILE_NAME);
            }
        }
        gs_free_object(mem, name , "map_file_name_add(name)");
    }
}

/*
 * map_file_name_ren
 *
 * renames the oldname into newname in the mapping table. newname must not exist and must be
 * checked by the caller. If same name exist, all same name will be renamed.
 * root_name       char*   string of the base path where in disk0 reside (C:\PS\Disk0\)
 * oldname         char*   name currently existing in the map
 * newname         char*   name to change the entry indicated by oldname into
 */
static void
map_file_name_ren(gs_memory_t *mem, const char* root_name, const char * oldname, const char * newname)
{
    /*  search for target entry */

    int d = MapToFile(mem, root_name, oldname);
    int file_version;
    char *name = (char *)gs_alloc_bytes(mem, gp_file_name_sizeof, "map_file_name_ren(name)");

    if (name && d != -1) { 		/* if target exists ... */
        FILE*   newMap;
        FILE*   oldMap;

        /* Open current map file and a working file */

        MapFileUnlink(mem, root_name, TEMP_FILE_NAME );
        newMap = MapFileOpen(mem, root_name, TEMP_FILE_NAME, "w");
        if (newMap == NULL)
            return;
        oldMap = MapFileOpen(mem, root_name, MAP_FILE_NAME, "r");
        if (oldMap != NULL && (!MapFileReadVersion(oldMap, &file_version)
            || file_version != MAP_FILE_VERSION)) {
            fclose(oldMap);
            oldMap= NULL;
        }
        if (oldMap == NULL) {
            fclose(newMap);
            MapFileUnlink(mem, root_name, TEMP_FILE_NAME);
            return;
        }

        /* Now copy data from old to new, change file name when found */

        MapFileWriteVersion(newMap, MAP_FILE_VERSION);  /* Copy the version number */
        while (MapFileRead(oldMap, name, &d))
            if (strcmp(name, oldname))
                MapFileWrite(newMap, name, d);
            else
                MapFileWrite(newMap, newname, d);
        fclose(newMap);
        fclose(oldMap);
        MapFileUnlink(mem, root_name, MAP_FILE_NAME);
        MapFileRename(mem, root_name, MAP_FILE_NAME, TEMP_FILE_NAME);
    }
    gs_free_object(mem, name ,"map_file_name_ren(name)");
}
