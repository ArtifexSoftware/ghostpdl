/* Copyright (C) 2001-2012 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134, San Rafael,
   CA  94903, U.S.A., +1(415)492-9861, for further information.
*/


/* File-based command list implementation */
#include "assert.h"
#include "stdio_.h"
#include "string_.h"
#include "unistd_.h"
#include "gserrors.h"
#include "gsmemory.h"
#include "gp.h"
#include "gxclio.h"

#ifdef PACIFY_VALGRIND
#include "valgrind.h"
#endif

/* This is an implementation of the command list I/O interface */
/* that uses the file system for storage. */

/* ------ Open/close/unlink ------ */

#define ENC_FILE_STR ("encoded_file_ptr_%p")
#define ENC_FILE_STRX ("encoded_file_ptr_0x%p")

static void
file_to_fake_path(clist_file_ptr file, char fname[gp_file_name_sizeof])
{
    gs_sprintf(fname, ENC_FILE_STR, file);
}

static clist_file_ptr
fake_path_to_file(const char *fname)
{
    clist_file_ptr i1, i2;

    int r1 = sscanf(fname, ENC_FILE_STR, &i1);
    int r2 = sscanf(fname, ENC_FILE_STRX, &i2);
    return r2 == 1 ? i2 : (r1 == 1 ? i1 : NULL);
}

/* Use our own FILE structure so that, on some platforms, we write and read
 * tmp files via a single file descriptor. That allows cleaning of tmp files
 * to be addressed via DELETE_ON_CLOSE under Windows, and immediate unlink
 * after opening under Linux. When running in this mode, we keep our own
 * record of position within the file for the sake of thread safety
 */
typedef struct
{
    gs_memory_t *mem;
    FILE *f;
    int64_t pos;
} IFILE;

static IFILE *wrap_file(gs_memory_t *mem, FILE *f)
{
    IFILE *ifile;

    if (!f) return NULL;
    ifile = (IFILE *)gs_alloc_bytes(mem->non_gc_memory, sizeof(*ifile), "Allocate wrapped IFILE");
    if (!ifile)
    {
        fclose(f);
        return NULL;
    }
    ifile->mem = mem->non_gc_memory;
    ifile->f = f;
    ifile->pos = 0;
    return ifile;
}

int close_file(IFILE *ifile)
{
    int res = 0;
    if (ifile)
    {
        res = fclose(ifile->f);
        gs_free_object(ifile->mem, ifile, "Free wrapped IFILE");
    }
    return res;
}

static int
clist_fopen(char fname[gp_file_name_sizeof], const char *fmode,
            clist_file_ptr * pcf, gs_memory_t * mem, gs_memory_t *data_mem,
            bool ok_to_compress)
{
    if (*fname == 0)
    {
        if (fmode[0] == 'r')
            return_error(gs_error_invalidfileaccess);
        if (gp_can_share_fdesc())
        {
            *pcf = (clist_file_ptr)wrap_file(mem, gp_open_scratch_file_rm(mem,
                                                       gp_scratch_file_name_prefix,
                                                       fname, fmode));
            /* If the platform supports FILE duplication then we overwrite the
             * file name with an encoded form of the FILE pointer */
            file_to_fake_path(*pcf, fname);
        }
        else
        {
            *pcf = (clist_file_ptr)wrap_file(mem, gp_open_scratch_file_64(mem,
                                                       gp_scratch_file_name_prefix,
                                                       fname, fmode));
        }
    }
    else
    {
        // Check if a special path is passed in. If so, clone the FILE handle
        clist_file_ptr ocf = fake_path_to_file(fname);
        if (ocf)
        {
            *pcf = wrap_file(mem, gp_fdup(((IFILE *)ocf)->f, fmode));
        }
        else
        {
            *pcf = wrap_file(mem, gp_fopen(fname, fmode));
        }
    }

    if (*pcf == NULL)
    {
        emprintf1(mem, "Could not open the scratch file %s.\n", fname);
        return_error(gs_error_invalidfileaccess);
    }

    return 0;
}

static int
clist_unlink(const char *fname)
{
    clist_file_ptr ocf = fake_path_to_file(fname);
    if (ocf)
    {
        /* fname is an encoded file pointer. The file will either have been
         * created with the delete-on-close option, or already have been
         * unlinked. We need only close the FILE */
        return close_file((IFILE *)ocf) != 0 ? gs_note_error(gs_error_ioerror) : 0;
    }
    else
    {
        return (unlink(fname) != 0 ? gs_note_error(gs_error_ioerror) : 0);
    }
}

static int
clist_fclose(clist_file_ptr cf, const char *fname, bool delete)
{
    clist_file_ptr ocf = fake_path_to_file(fname);
    if (ocf == cf)
    {
        /* fname is an encoded file pointer, and cf is the FILE used to create it.
         * We shouldn't close it unless we have been asked to delete it, in which
         * case closing it will delete it */
        if (delete)
            close_file((IFILE *)ocf);
    }
    else
    {
        return (close_file((IFILE *) cf) != 0 ? gs_note_error(gs_error_ioerror) :
                delete ? clist_unlink(fname) :
                0);
    }
}

/* ------ Writing ------ */

static int
clist_fwrite_chars(const void *data, uint len, clist_file_ptr cf)
{
    if (gp_can_share_fdesc())
    {
        int res = gp_fpwrite(data, len, ((IFILE *)cf)->pos, ((IFILE *)cf)->f);
        if (res >= 0)
            ((IFILE *)cf)->pos += len; return res;
    }
    else
    {
        return fwrite(data, 1, len, ((IFILE *)cf)->f);
    }
}

/* ------ Reading ------ */

static int
clist_fread_chars(void *data, uint len, clist_file_ptr cf)
{
    if (gp_can_share_fdesc())
    {
        int res = gp_fpread(data, len, ((IFILE *)cf)->pos, ((IFILE *)cf)->f);
        if (res >= 0)
            ((IFILE *)cf)->pos += res;

        return res;
    }
    else
    {
        FILE *f = ((IFILE *)cf)->f;
        byte *str = data;

        /* The typical implementation of fread */
        /* is extremely inefficient for small counts, */
        /* so we just use straight-line code instead. */
        switch (len) {
            default:
                return fread(str, 1, len, f);
            case 8:
                *str++ = (byte) getc(f);
            case 7:
                *str++ = (byte) getc(f);
            case 6:
                *str++ = (byte) getc(f);
            case 5:
                *str++ = (byte) getc(f);
            case 4:
                *str++ = (byte) getc(f);
            case 3:
                *str++ = (byte) getc(f);
            case 2:
                *str++ = (byte) getc(f);
            case 1:
                *str = (byte) getc(f);
        }

        return len;
    }
}

/* ------ Position/status ------ */

static int
clist_set_memory_warning(clist_file_ptr cf, int bytes_left)
{
    return 0;			/* no-op */
}

static int
clist_ferror_code(clist_file_ptr cf)
{
    return (ferror(((IFILE *)cf)->f) ? gs_error_ioerror : 0);
}

static int64_t
clist_ftell(clist_file_ptr cf)
{
    IFILE *ifile = (IFILE *)cf;

    return gp_can_share_fdesc() ? ifile->pos : ftell(ifile->f);
}

static void
clist_rewind(clist_file_ptr cf, bool discard_data, const char *fname)
{
    FILE *f = ((IFILE *)cf)->f;
    IFILE *ocf = fake_path_to_file(fname);
    char fmode[4];

    strcpy(fmode, "w+");
    strcat(fmode, gp_fmode_binary_suffix);

    if (ocf)
    {
        if (discard_data)
        {
            /* fname is an encoded ifile pointer. We can use an entirely
             * new scratch file. */
            char tfname[gp_file_name_sizeof];
            fclose(ocf->f);
            ocf->f = gp_open_scratch_file_rm(NULL, gp_scratch_file_name_prefix, tfname, fmode);
        }
        else
        {
            ((IFILE *)cf)->pos = 0;
        }
    }
    else
    {
        if (discard_data) {
            /*
             * The ANSI C stdio specification provides no operation for
             * truncating a file at a given position, or even just for
             * deleting its contents; we have to use a bizarre workaround to
             * get the same effect.
             */

            /* Opening with "w" mode deletes the contents when closing. */
            (void)freopen(fname, gp_fmode_wb, f);
            (void)freopen(fname, fmode, f);
        } else {
            rewind(f);
        }
    }
}

static int
clist_fseek(clist_file_ptr cf, int64_t offset, int mode, const char *ignore_fname)
{
    IFILE *ifile = (IFILE *)cf;

    if (gp_can_share_fdesc())
    {
        switch (mode)
        {
            case SEEK_SET:
                ifile->pos = offset;
                break;
            case SEEK_CUR:
                ifile->pos += offset;
                break;
            case SEEK_END:
                gp_fseek_64(ifile->f, 0, SEEK_END);
                ifile->pos = ftell(ifile->f);
                break;
        }
        return 0;
    }
    else
    {
        return gp_fseek_64(ifile->f, offset, mode);
    }
}

static clist_io_procs_t clist_io_procs_file = {
    clist_fopen,
    clist_fclose,
    clist_unlink,
    clist_fwrite_chars,
    clist_fread_chars,
    clist_set_memory_warning,
    clist_ferror_code,
    clist_ftell,
    clist_rewind,
    clist_fseek,
};

init_proc(gs_gxclfile_init);
int
gs_gxclfile_init(gs_memory_t *mem)
{
#ifdef PACIFY_VALGRIND
    VALGRIND_HG_DISABLE_CHECKING(&clist_io_procs_file_global, sizeof(clist_io_procs_file_global));
#endif
    clist_io_procs_file_global = &clist_io_procs_file;
    return 0;
}
