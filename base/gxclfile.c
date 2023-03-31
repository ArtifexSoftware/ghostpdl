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


/* File-based command list implementation */
#include "assert_.h"
#include "stdio_.h"
#include "string_.h"
#include "unistd_.h"
#include "gserrors.h"
#include "gsmemory.h"
#include "gp.h"
#include "gxclio.h"

#include "valgrind.h"


/* This is an implementation of the command list I/O interface */
/* that uses the file system for storage. */

/* clist cache code so that wrapped files don't incur a performance penalty */
#define CL_CACHE_NSLOTS (3)
#define CL_CACHE_SLOT_SIZE_LOG2 (15)
#define CL_CACHE_SLOT_EMPTY (-1)

static clist_io_procs_t clist_io_procs_file;

typedef struct
{
    int64_t blocknum;
    byte *base;
} CL_CACHE_SLOT;

typedef struct
{
    int block_size;		/* full block size, MUST BE power of 2 */
    int nslots;
    int64_t filesize;
    gs_memory_t *memory;	/* save our allocator */
    CL_CACHE_SLOT *slots;	/* array of slots */
    byte *base;                 /* save base of slot data area */
} CL_CACHE;

/* Forward references */
CL_CACHE *cl_cache_alloc(gs_memory_t *mem);
void cl_cache_destroy(CL_CACHE *cache);
CL_CACHE *cl_cache_read_init(CL_CACHE *cache, int nslots, int64_t block_size, int64_t filesize);
int cl_cache_read(byte *data, int len, int64_t pos, CL_CACHE *cache);
CL_CACHE_SLOT * cl_cache_get_empty_slot(CL_CACHE *cache, int64_t pos);
void cl_cache_load_slot(CL_CACHE *cache, CL_CACHE_SLOT *slot, int64_t pos, byte *data, int len);

#define CL_CACHE_NEEDS_INIT(cache) (cache != NULL && cache->filesize == 0)

CL_CACHE *
cl_cache_alloc(gs_memory_t *mem)
{
    CL_CACHE *cache;

    /* allocate and initialilze the cache to filesize = 0 to signal read_init needed */
    cache = (CL_CACHE *)gs_alloc_bytes(mem, sizeof(CL_CACHE), "alloc CL_CACHE");
    if (cache != NULL) {
        cache->filesize = 0;
        cache->nslots = 0;
        cache->block_size = 0;
        cache->slots = NULL;
        cache->base = NULL;
        cache->memory = mem;
    }
    return cache;
}

void
cl_cache_destroy(CL_CACHE *cache)
{
    if (cache == NULL)
        return;

    if (cache->slots != NULL) {
        gs_free_object(cache->memory, cache->base, "CL_CACHE SLOT data");
        gs_free_object(cache->memory, cache->slots, "CL_CACHE slots array");
    }
    gs_free_object(cache->memory, cache, "CL_CACHE for IFILE");
}

/* Set the cache up for reading. The filesize is used for EOF */
CL_CACHE *
cl_cache_read_init(CL_CACHE *cache, int nslots, int64_t block_size, int64_t filesize)
{
    /* NB: if fail, and cache is still NULL, proceed without cache, reading will cope */
    if (cache == NULL || cache->filesize != 0)
        return cache;		/* once we've done the init, filesize will be set */

    if ((filesize+block_size)/block_size < nslots)
        nslots = (filesize + block_size)/block_size;	/* limit at blocks needed for entire file */
    cache->slots = (CL_CACHE_SLOT *)gs_alloc_bytes(cache->memory, nslots * sizeof(CL_CACHE_SLOT),
                                                       "CL_CACHE slots array");
    if (cache->slots == NULL) {
        gs_free_object(cache->memory, cache, "Free CL_CACHE for IFILE");
        cache = NULL;			/* cache not possible */
    } else {
        cache->slots[0].base = (byte *)gs_alloc_bytes(cache->memory, nslots * block_size,
                                                       "CL_CACHE_SLOT data");
        if (cache->slots[0].base == NULL) {
            gs_free_object(cache->memory, cache->slots, "Free CL_CACHE for IFILE");
            gs_free_object(cache->memory, cache, "Free CL_CACHE for IFILE");
            cache = NULL;			/* cache not possible */
        } else {
            /* success, initialize the slots */
            int i;

            for (i=0; i < nslots; i++) {
                cache->slots[i].blocknum = CL_CACHE_SLOT_EMPTY;
                cache->slots[i].base = cache->slots[0].base + (i * block_size);
            }
            cache->base = cache->slots[0].base;         /* save for the 'destroy' (slots array moves around) */
            cache->nslots = nslots;
            cache->block_size = block_size;
            cache->filesize = filesize;
        }
    }
    return cache;	/* May be NULL. If so, no cache used */
}

/* Find the cache for the slot containing the 'pos'. */
/* return the number of bytes read, up to 'len' bytes */
/* returns 0 if 'pos' not in cache, -1 if pos at or past EOF. */
int
cl_cache_read(byte *data, int len, int64_t pos, CL_CACHE *cache)
{
    int nread = 0;
    int slot;
    int offset;
    int64_t blocknum = pos / cache->block_size;

    if (pos >= cache->filesize)
        return -1;

    /* find the slot */
    for (slot = 0; slot < cache->nslots; slot++) {
        if (blocknum == cache->slots[slot].blocknum)
            break;
    }
    if (slot >= cache->nslots)
        return 0;               /* block not in cache */

    if (slot != 0) {
        /* move the slot we found to the top, moving the rest down */
        byte *base = cache->slots[slot].base;
        int i;

        for (i = slot; i > 0; i--) {
            cache->slots[i].base = cache->slots[i-1].base;
            cache->slots[i].blocknum = cache->slots[i-1].blocknum;
        }
        cache->slots[0].blocknum = blocknum;
        cache->slots[0].base = base;
    }
    offset = pos - cache->slots[0].blocknum * cache->block_size;
    nread = min(cache->block_size - offset, len);
    if (nread + pos > cache->filesize)
        nread = cache->filesize - pos;	/* limit for EOF */
    memcpy(data, cache->slots[0].base + offset, nread);
    return nread;
}

/* 'pos' not used yet */
/* discard the LRU, move remaining slots down and return the first as new MRU */
CL_CACHE_SLOT *
cl_cache_get_empty_slot(CL_CACHE *cache, int64_t pos)
{
    /* the LRU is in the last slot, so re-use it */
    CL_CACHE_SLOT *pslot = &(cache->slots[0]);             /* slot used will always be first, possibly after moving */
    int64_t slot0_blocknum = pslot->blocknum;

    if (slot0_blocknum == CL_CACHE_SLOT_EMPTY)
        return pslot;

    /* if more than on slot in the cache, handle moving slots to bump the LRU (last) */
    /* If the block at slot 0 hasn't been flushed at least once before, just use slot 0 */
    if (cache->nslots > 1) {
        /* rotate the cache to re-use the last slot (LRU) and move it to the top, moving the rest down */
        byte *last_slot_base = cache->slots[cache->nslots - 1].base;    /* save the base for the last slot */
        int i;

        /* move the rest down */
        for (i=cache->nslots - 1; i > 0; i--) {
            cache->slots[i].blocknum = cache->slots[i-1].blocknum;
            cache->slots[i].base = cache->slots[i-1].base;
        }
        pslot->base = last_slot_base;
    }
    pslot->blocknum = CL_CACHE_SLOT_EMPTY;
    return pslot;
}

void
cl_cache_load_slot(CL_CACHE *cache, CL_CACHE_SLOT *slot, int64_t pos, byte *data, int len)
{
    slot->blocknum = pos / cache->block_size;
    memmove(slot->base, data, len);
}

/* Use our own FILE structure so that, on some platforms, we write and read
 * tmp files via a single file descriptor. That allows cleaning of tmp files
 * to be addressed via DELETE_ON_CLOSE under Windows, and immediate unlink
 * after opening under Linux. When running in this mode, we keep our own
 * record of position within the file for the sake of thread safety
 */

#define ENC_FILE_STR ("encoded_file_ptr_%p")
#define ENC_FILE_STRX ("encoded_file_ptr_0x%p")

typedef struct
{
    gs_memory_t *mem;
    gp_file *f;
    int64_t pos;
    int64_t filesize;		/* filesize maintained by clist_fwrite */
    CL_CACHE *cache;
} IFILE;

static void
file_to_fake_path(clist_file_ptr file, char fname[gp_file_name_sizeof])
{
    gs_snprintf(fname, gp_file_name_sizeof, ENC_FILE_STR, file);
}

static clist_file_ptr
fake_path_to_file(const char *fname)
{
    clist_file_ptr i1, i2;

    int r1 = sscanf(fname, ENC_FILE_STR, &i1);
    int r2 = sscanf(fname, ENC_FILE_STRX, &i2);
    return r2 == 1 ? i2 : (r1 == 1 ? i1 : NULL);
}

static IFILE *wrap_file(gs_memory_t *mem, gp_file *f, const char *fmode)
{
    IFILE *ifile;

    if (!f) return NULL;
    ifile = (IFILE *)gs_alloc_bytes(mem->non_gc_memory, sizeof(*ifile), "Allocate wrapped IFILE");
    if (!ifile) {
        gp_fclose(f);
        return NULL;
    }
    ifile->mem = mem->non_gc_memory;
    ifile->f = f;
    ifile->pos = 0;
    ifile->filesize = 0;
    ifile->cache = cl_cache_alloc(ifile->mem);
    return ifile;
}

static int clist_close_file(IFILE *ifile)
{
    int res = 0;
    if (ifile) {
        if (ifile->f != NULL)
            res = gp_fclose(ifile->f);
        if (ifile->cache != NULL)
            cl_cache_destroy(ifile->cache);
        gs_free_object(ifile->mem, ifile, "Free wrapped IFILE");
    }
    return res;
}

/* ------ Open/close/unlink ------ */

static int
clist_fopen(char fname[gp_file_name_sizeof], const char *fmode,
            clist_file_ptr * pcf, gs_memory_t * mem, gs_memory_t *data_mem,
            bool ok_to_compress)
{
    if (*fname == 0) {
        if (fmode[0] == 'r')
            return_error(gs_error_invalidfileaccess);
        if (gp_can_share_fdesc()) {
            *pcf = (clist_file_ptr)wrap_file(mem, gp_open_scratch_file_rm(mem,
                                                       gp_scratch_file_name_prefix,
                                                       fname, fmode), fmode);
            /* If the platform supports FILE duplication then we overwrite the
             * file name with an encoded form of the FILE pointer */
            if (*pcf != NULL)
                file_to_fake_path(*pcf, fname);
        } else {
            *pcf = (clist_file_ptr)wrap_file(mem, gp_open_scratch_file(mem,
                                                       gp_scratch_file_name_prefix,
                                                       fname, fmode), fmode);
        }
    } else {
        clist_file_ptr ocf = fake_path_to_file(fname);
        if (ocf) {
            /*  A special (fake) fname is passed in. If so, clone the FILE handle */
            *pcf = wrap_file(mem, gp_fdup(((IFILE *)ocf)->f, fmode), fmode);
            /* when cloning, copy other parts not done by wrap_file */
            if (*pcf)
                ((IFILE *)(*pcf))->filesize = ((IFILE *)ocf)->filesize;
        } else {
            *pcf = wrap_file(mem, gp_fopen(mem, fname, fmode), fmode);
        }
    }

    if (*pcf == NULL) {
        emprintf1(mem, "Could not open the scratch file %s.\n", fname);
        return_error(gs_error_invalidfileaccess);
    }

    return 0;
}

static int
clist_unlink(const char *fname)
{
    clist_file_ptr ocf = fake_path_to_file(fname);
    if (ocf) {
        /* fname is an encoded file pointer. The file will either have been
         * created with the delete-on-close option, or already have been
         * unlinked. We need only close the FILE */
        return clist_close_file((IFILE *)ocf) != 0 ? gs_note_error(gs_error_ioerror) : 0;
    } else {
        return (unlink(fname) != 0 ? gs_note_error(gs_error_ioerror) : 0);
    }
}

static int
clist_fclose(clist_file_ptr cf, const char *fname, bool delete)
{
    clist_file_ptr ocf = fake_path_to_file(fname);
    if (ocf == cf) {
        /* fname is an encoded file pointer, and cf is the FILE used to create it.
         * We shouldn't close it unless we have been asked to delete it, in which
         * case closing it will delete it */
        return delete ? (clist_close_file((IFILE *)ocf) ? gs_note_error(gs_error_ioerror) : 0) : 0;
    } else {
        return (clist_close_file((IFILE *) cf) != 0 ? gs_note_error(gs_error_ioerror) :
                delete ? clist_unlink(fname) :
                0);
    }
}

/* ------ Writing ------ */

static int
clist_fwrite_chars(const void *data, uint len, clist_file_ptr cf)
{
    int res = 0;
    IFILE *icf = (IFILE *)cf;

    if (gp_can_share_fdesc()) {
        res = gp_fpwrite((char *)data, len, ((IFILE *)cf)->pos, ((IFILE *)cf)->f);
    } else {
        res = gp_fwrite(data, 1, len, ((IFILE *)cf)->f);
    }
    if (res >= 0)
        icf->pos += len;
    icf->filesize = icf->pos;	/* write truncates file */
    if (!CL_CACHE_NEEDS_INIT(icf->cache)) {
        /* writing invalidates the read cache */
        cl_cache_destroy(icf->cache);
        icf->cache = NULL;
    }
    return res;
}

/* ------ Reading ------ */

static int
clist_fread_chars(void *data, uint len, clist_file_ptr cf)
{
    int nread = 0;

    if (gp_can_share_fdesc()) {
        IFILE *icf = (IFILE *)cf;
        byte *dp = data;

        /* if we have a cache, check if it needs init, and do it */
        if (CL_CACHE_NEEDS_INIT(icf->cache)) {
            icf->cache = cl_cache_read_init(icf->cache, CL_CACHE_NSLOTS, 1<<CL_CACHE_SLOT_SIZE_LOG2, icf->filesize);
        }
        /* cl_cache_read_init may have failed, and set cache to NULL, check before using it */
        if (icf->cache != NULL) {
            do {
                int n;

                if ((n = cl_cache_read(dp, len-nread, icf->pos+nread, icf->cache)) < 0)
                    break;
                if (n == 0) {
                    /* pos was not in cache, get a slot and load it, then loop */
                    CL_CACHE_SLOT *slot = cl_cache_get_empty_slot(icf->cache, icf->pos+nread);  /* cannot fail */
                    int64_t block_pos = (icf->pos+nread) & ~(icf->cache->block_size - 1);
                    int fill_len = gp_fpread((char *)(slot->base), icf->cache->block_size, block_pos, icf->f);

                    cl_cache_load_slot(icf->cache, slot, block_pos, slot->base, fill_len);
                }
                nread += n;
                dp += n;
            } while (nread < len);
        } else {
             /* no cache -- just do the read */
            nread = gp_fpread(data, len, icf->pos, icf->f);
        }
        if (nread >= 0)
            icf->pos += nread;
    } else {
        gp_file *f = ((IFILE *)cf)->f;
        byte *str = data;

        /* The typical implementation of fread */
        /* is extremely inefficient for small counts, */
        /* so we just use straight-line code instead. */
        switch (len) {
            default:
                return gp_fread(str, 1, len, f);
            case 8:
                *str++ = (byte) gp_fgetc(f);
            case 7:
                *str++ = (byte) gp_fgetc(f);
            case 6:
                *str++ = (byte) gp_fgetc(f);
            case 5:
                *str++ = (byte) gp_fgetc(f);
            case 4:
                *str++ = (byte) gp_fgetc(f);
            case 3:
                *str++ = (byte) gp_fgetc(f);
            case 2:
                *str++ = (byte) gp_fgetc(f);
            case 1:
                *str = (byte) gp_fgetc(f);
        }
        nread = len;
    }
    return nread;
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
    return (gp_ferror(((IFILE *)cf)->f) ? gs_error_ioerror : 0);
}

static int64_t
clist_ftell(clist_file_ptr cf)
{
    IFILE *ifile = (IFILE *)cf;

    return gp_can_share_fdesc() ? ifile->pos : gp_ftell(ifile->f);
}

static int
clist_rewind(clist_file_ptr cf, bool discard_data, const char *fname)
{
    gp_file *f = ((IFILE *)cf)->f;
    IFILE *ocf = fake_path_to_file(fname);
    char fmode[4];

    snprintf(fmode, sizeof(fmode), "w+%s", gp_fmode_binary_suffix);

    if (ocf) {
        if (discard_data) {
            /* fname is an encoded ifile pointer. We can use an entirely
             * new scratch file. */
            char tfname[gp_file_name_sizeof] = {0};
            const gs_memory_t *mem = ocf->f->memory;
            gp_fclose(ocf->f);
            ocf->f = gp_open_scratch_file_rm(mem, gp_scratch_file_name_prefix, tfname, fmode);
            if (ocf->f == NULL)
                return_error(gs_error_ioerror);
            /* if there was a cache, get rid of it an get a new (empty) one */
            /* When we start reading, we will allocate a cache based on the filesize */
            if (ocf->cache != NULL) {
                cl_cache_destroy(ocf->cache);
                ocf->cache = cl_cache_alloc(ocf->mem);
                if (ocf->cache == NULL)
                    return_error(gs_error_ioerror);
            }
            ((IFILE *)cf)->filesize = 0;
        }
        ((IFILE *)cf)->pos = 0;
    } else {
        if (discard_data) {
            /*
             * The ANSI C stdio specification provides no operation for
             * truncating a file at a given position, or even just for
             * deleting its contents; we have to use a bizarre workaround to
             * get the same effect.
             */

            /* Opening with "w" mode deletes the contents when closing. */
            f = gp_freopen(fname, gp_fmode_wb, f);
            if (f == NULL) return_error(gs_error_ioerror);
            ((IFILE *)cf)->f = gp_freopen(fname, fmode, f);
            if (((IFILE *)cf)->f == NULL) return_error(gs_error_ioerror);
            ((IFILE *)cf)->pos = 0;
            ((IFILE *)cf)->filesize = 0;
        } else {
            gp_rewind(f);
        }
    }
    return 0;
}

static int
clist_fseek(clist_file_ptr cf, int64_t offset, int mode, const char *ignore_fname)
{
    IFILE *ifile = (IFILE *)cf;
    int res = 0;

    if (!gp_can_share_fdesc()) {
        res = gp_fseek(ifile->f, offset, mode);
    }
    /* NB: if gp_can_share_fdesc, we don't actually seek */
    /* The following lgtm tag is required because on some platforms
     * !gp_can_share_fdesc() is always true, so the value of res is
     * known. On other platforms though, this is NOT true. */
    if (res >= 0) { /* lgtm [cpp/constant-comparison] */
        /* Update the ifile->pos */
        switch (mode) {
            case SEEK_SET:
                ifile->pos = offset;
                break;
            case SEEK_CUR:
                ifile->pos += offset;
                break;
            case SEEK_END:
                ifile->pos = ifile->filesize;	/* filesize maintained in clist_fwrite */
                break;
        }
    }
    return res;
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
    gs_lib_ctx_core_t *core = mem->gs_lib_ctx->core;
#ifdef PACIFY_VALGRIND
    VALGRIND_HG_DISABLE_CHECKING(&core->clist_io_procs_file, sizeof(core->clist_io_procs_file));
#endif
    core->clist_io_procs_file = &clist_io_procs_file;
    return 0;
}
