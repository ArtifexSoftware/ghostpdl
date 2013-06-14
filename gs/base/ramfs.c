/*
  memory-based simulated file system

  files only, no directories (well, one)
  (C) 2006 Michael Slade <micksa@knobbits.org>
 */

#include "unistd_.h"
#include "string_.h"
#include "malloc_.h"
#include "gx.h"
#include "gserrors.h"
#include "gp.h"
#include "gscdefs.h"
#include "gsparam.h"
#include "gsstruct.h"
#include "ramfs.h"

#define MACROBLOCK_REALLOC_MAX 128

typedef struct _ramfs {
    struct _ramdirent * files;
    struct _ramfs_enum* active_enums;
    int blocksfree;
    int last_error;
};

gs_private_st_ptrs2(st_ramfs, struct _ramfs, "gsram_ramfs",
    _ramfs_enum_ptrs, _ramfs_reloc_ptrs, files, active_enums);

struct _ramdirent {
    char* filename;
    struct _ramfile* inode;
    struct _ramdirent* next;
};

gs_private_st_ptrs3(st_ramdirent, struct _ramdirent, "gsram_ramdirent",
    _ramdirent_enum_ptrs, _ramdirent_reloc_ptrs, filename, inode, next);

typedef struct _ramfile {
    ramfs* fs;
    int refcount;
    int size;
    int blocks;
    int blocklist_size;
    char** data;
} ramfile;

gs_private_st_ptrs2(st_ramfile, struct _ramfile, "gsram_ramfile",
    _ramfile_enum_ptrs, _ramfile_reloc_ptrs, fs, data);

struct _ramhandle {
    ramfile * file;
    int last_error;
    int filepos;
    int mode;
};

gs_private_st_ptrs1(st_ramhandle, struct _ramhandle, "gsram_ramhandle",
    _ramhandle_enum_ptrs, _ramhandle_reloc_ptrs, file);

struct _ramfs_enum {
    ramfs* fs;
    ramdirent * current;
    struct _ramfs_enum* next;
};

gs_private_st_ptrs3(st_ramfs_enum, struct _ramfs_enum, "gsram_ramfs_enum",
    _ramfs_enum_enum_ptrs, _ramfs_enum_reloc_ptrs, fs, current, next);

static void unlink_node(ramfile * inode);
static int ramfile_truncate(ramhandle * handle,int size);


ramfs * ramfs_new(gs_memory_t *mem, int size) {
    ramfs * fs = gs_alloc_struct(mem, ramfs, &st_ramfs,
    "ramfs_new"
    );

    if (fs == NULL) {
        fs->last_error = RAMFS_NOMEM;
        return NULL;
    }
    size = size/(RAMFS_BLOCKSIZE/1024);
    fs->files = NULL;
    fs->active_enums = NULL;
    fs->blocksfree = size;
    fs->last_error = 0;
    return fs;
}

/* This function makes no attempt to check that there are no open files or
   enums.  If there are any when this function is called, memory leakage will
   result and any attempt to access the open files or enums will probably
   cause a segfault.  Caveat emptor, or something.
*/
void ramfs_destroy(gs_memory_t *mem, ramfs * fs) {
    ramdirent * ent;

    if(fs == NULL) return;

    ent = fs->files;
    while(ent) {
        ramdirent* prev;
        free(ent->filename);
        unlink_node(ent->inode);
        prev = ent;
        ent = ent->next;
        free(prev);
    }
    gs_free_object(mem, fs, "ramfs_destroy");
}

int ramfs_error(const ramfs* fs) { return fs->last_error; }

static int resize(ramfile * file,int size) {
    int newblocks = (size+RAMFS_BLOCKSIZE-1)/RAMFS_BLOCKSIZE;
    if(newblocks > file->blocks) {
        /* allocate blocks for file as necessary */

        if(newblocks-file->blocks > file->fs->blocksfree) {
            return -RAMFS_NOSPACE;
        }
        if(file->blocklist_size < newblocks) {
            int newsize = file->blocklist_size;
            if (newsize > MACROBLOCK_REALLOC_MAX) {
                newsize = ((newblocks+MACROBLOCK_REALLOC_MAX-1)/
                    MACROBLOCK_REALLOC_MAX) * MACROBLOCK_REALLOC_MAX;
            } else {
                if(!newsize) newsize = 1;
                while(newsize < newblocks) newsize *= 2;
            }
            file->data = realloc(file->data,newsize * sizeof(char*));
            file->blocklist_size = newsize;
        }
        while(file->blocks<newblocks) {
            char * block = file->data[file->blocks] = malloc(RAMFS_BLOCKSIZE);
            if(!block) {
                return -RAMFS_NOMEM;
            }
            file->blocks++;
            file->fs->blocksfree--;
        }
    } else if (newblocks < file->blocks) {
        /* don't bother shrinking the block array */
        file->fs->blocksfree += (file->blocks-newblocks);
        while(file->blocks > newblocks) {
            free(file->data[--file->blocks]);
        }
    }
    file->size = size;
    return 0;
}

static ramdirent * ramfs_findfile(const ramfs* fs,const char *filename) {
    ramdirent * this = fs->files;
    while(this) {
        if(strcmp(this->filename,filename) == 0) break;
        this = this->next;
    }
    return this;
}

ramhandle * ramfs_open(gs_memory_t *mem, ramfs* fs,const char * filename,int mode) {
    ramdirent * this;
    ramfile* file;
    ramhandle* handle;

    if(mode & (RAMFS_CREATE|RAMFS_APPEND)) mode |= RAMFS_WRITE;

    this = ramfs_findfile(fs,filename);

    if(!this) {
        /* create file? */
        char * dirent_filename;

        if(!(mode & RAMFS_CREATE)) {
            fs->last_error = RAMFS_NOTFOUND;
            return NULL;
        }
        this = malloc(sizeof(ramdirent));
        file = malloc(sizeof(ramfile));
        dirent_filename = malloc(strlen(filename)+1);
        if(!(this && file && dirent_filename)) {
            free(this);
            free(file);
            free(dirent_filename);
            fs->last_error = RAMFS_NOMEM;
            return NULL;
        }
        strcpy(dirent_filename,filename);
        this->filename = dirent_filename;
        file->refcount = 1;
        file->size = 0;
        file->blocks = 0;
        file->blocklist_size = 0;
        file->data = NULL;
        file->fs = fs;
        this->inode = file;
        this->next = fs->files;
        fs->files = this;
    }
    file = this->inode;
    file->refcount++;

    handle = malloc(sizeof(ramhandle));
    if(!handle) {
        fs->last_error = RAMFS_NOMEM;
        return NULL;
    }
    handle->file = file;
    handle->filepos = 0;
    handle->mode = mode;

    if(mode & RAMFS_TRUNC) {
        resize(file,0);
    }
    return handle;
}

int ramfs_blocksize(ramfs * fs) { return RAMFS_BLOCKSIZE; }
int ramfs_blocksfree(ramfs * fs) { return fs->blocksfree; }
int ramfile_error(ramhandle * handle) { return handle->last_error; }

static void unlink_node(ramfile * inode) {
    int c;

    --inode->refcount;
    if(inode->refcount) return;

    /* remove the file and its data */
    for(c=0;c<inode->blocks;c++) {
        free(inode->data[c]);
    }
    inode->fs->blocksfree += c;
    free(inode->data);
    free(inode);
}

int ramfs_unlink(ramfs * fs,const char *filename) {
    ramdirent ** last;
    ramdirent * this;
    ramfs_enum* e;

    last = &fs->files;
    while(1) {
        if(!(this = *last)) {
            fs->last_error = RAMFS_NOTFOUND;
            return -1;
        }
        if(strcmp(this->filename,filename) == 0) break;
        last = &(this->next);
    }

    unlink_node(this->inode);
    free(this->filename);
    (*last) = this->next;

    e = fs->active_enums;
    /* advance enums that are pointing to the just-deleted file */
    while(e) {
        if(e->current == this) e->current = this->next;
        e = e->next;
    }
    free(this);
    return 0;
}

int ramfs_rename(ramfs * fs,const char* oldname,const char* newname) {
    ramdirent * this;
    char * newnamebuf;

    this = ramfs_findfile(fs,oldname);

    if(!this) {
        fs->last_error = RAMFS_NOTFOUND;
        return -1;
    }

    /* just in case */
    if(strcmp(oldname,newname) == 0) return 0;

    newnamebuf = realloc(this->filename,strlen(newname)+1);
    if(!newnamebuf) {
        fs->last_error = RAMFS_NOMEM;
        return -1;
    }

    /* this may return RAMFS_NOTFOUND, which can be ignored. */
    ramfs_unlink(fs,newname);

    strcpy(newnamebuf,newname);
    this->filename = newnamebuf;
    return 0;
}

ramfs_enum * ramfs_enum_new(ramfs * fs) {
    ramfs_enum * e;

    e = malloc(sizeof(ramfs_enum));
    if(!e) {
        fs->last_error = RAMFS_NOMEM;
        return NULL;
    }
    e->current = fs->files;
    e->next = fs->active_enums;
    e->fs = fs;
    fs->active_enums = e;
    return e;
}

char* ramfs_enum_next(ramfs_enum * e) {
    char * filename = NULL;
    if(e->current) {
        filename = e->current->filename;
        e->current = e->current->next;
    }
    return filename;
}

void ramfs_enum_end(ramfs_enum * e) {
    ramfs_enum** last = &e->fs->active_enums;
    while(*last) {
        if(*last == e) {
            *last = e->next;
            break;
        }
        last = &(e->next);
    }
    free(e);
}

int ramfile_read(ramhandle * handle,void * buf,int len) {
    ramfile * file = handle->file;
    int left;
    char *t = (char *)buf;

    if(len>file->size - handle->filepos) len = file->size-handle->filepos;
    if(len<0) return 0;

    left = len;
    while(left) {
        char * p = file->data[handle->filepos/RAMFS_BLOCKSIZE]+handle->filepos%RAMFS_BLOCKSIZE;
        int x = RAMFS_BLOCKSIZE-handle->filepos%RAMFS_BLOCKSIZE;
        if(x>left) x = left;

        memcpy(t,p,x);
        handle->filepos += x;
        left -= x;
        t += x;
    }
    buf = (void *)t;
    return len;
}

int ramfile_write(ramhandle * handle,const void * buf,int len) {
    ramfile * file = handle->file;
    int left;
    char *t = (char *)buf;

    if(!handle->mode & RAMFS_WRITE) {
        handle->last_error = RAMFS_NOACCESS;
        return -1;
    }

    if(handle->mode & RAMFS_APPEND) {
        handle->filepos = file->size;
    }

    if(file->size < handle->filepos) {
        /* if this fails then pass the error on */
        if(ramfile_truncate(handle,handle->filepos) == -1) return -1;
    }

    if(file->size < handle->filepos+len) {
        int x = resize(file,handle->filepos+len);
        if(x) {
            handle->last_error = -x;
            return -1;
        }
    }

    /* This is exactly the same as for reading, cept the copy is in the
       other direction. */
    left = len;
    while(left) {
        char * p = file->data[handle->filepos/RAMFS_BLOCKSIZE] +
            handle->filepos%RAMFS_BLOCKSIZE;
        int x = RAMFS_BLOCKSIZE-handle->filepos%RAMFS_BLOCKSIZE;
        if(x>left) x = left;

        memcpy(p,buf,x);
        handle->filepos += x;
        left -= x;
        t += x;
    }
    buf = (void *)t;
    return len;
}

int ramfile_seek(ramhandle * handle,int pos,int whence) {
    /* Just set the handle's file position.  The effects become noticeable
       at the next read or write.
    */
    if(whence == RAMFS_SEEK_CUR) {
        handle->filepos += pos;
    } else if(whence == RAMFS_SEEK_END) {
        handle->filepos = handle->file->size+pos;
    } else {
        handle->filepos = pos;
    }
    return 0;
}

int ramfile_size(ramhandle * handle) {
    return handle->file->size;
}

static int ramfile_truncate(ramhandle * handle,int size) {
    ramfile * file = handle->file;
    int oldsize = file->size;
    int x = resize(file,size);

    if(x) {
        handle->last_error = -x;
        return -1;
    }
    if(oldsize >= size) return 0;

    /* file was expanded. fill the new space with zeros. */
    while(oldsize < file->size) {
        char * p = file->data[oldsize/RAMFS_BLOCKSIZE]+oldsize%RAMFS_BLOCKSIZE;
        int len = RAMFS_BLOCKSIZE - oldsize%RAMFS_BLOCKSIZE;
        if(len>file->size-oldsize) len = file->size-oldsize;
        oldsize += len;
        memset(p,0,len);
    }
    return 0;
}

void ramfile_close(ramhandle * handle) {
    ramfile * file = handle->file;
    unlink_node(file);
    free(handle);
}

int ramfile_tell(ramhandle* handle) {
    return handle->filepos;
}

int ramfile_eof(ramhandle* handle) {
    return (handle->filepos >= handle->file->size);
}
