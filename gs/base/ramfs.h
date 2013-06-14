#ifndef __RAMFS_H__
#define __RAMFS_H__

#define RAMFS_BLOCKSIZE 1024

typedef struct _         ;
typedef struct _ramdirent ramdirent;
typedef struct _ramhandle ramhandle;
typedef struct _    _enum     _enum;

/*
      _new: NOMEM
      _open: NOTFOUND
      _unlink: NOTFOUND
      _enum_new: NOMEM
      _enum_next: none
      _enum_end: none
  ramfile_read: none
  ramfile_write: NOSPACE, NOMEM, NOACCESS
  ramfile_seek: none
  ramfile_pos: none
  ramfile_close: none
*/

/* Error constants */
#define RAMFS_NOTFOUND 2
#define RAMFS_NOACCESS 5
#define RAMFS_NOMEM 6
#define RAMFS_NOSPACE 7

/* Open mode flags */
#define RAMFS_READ 1
#define RAMFS_CREATE 2
#define RAMFS_WRITE 4
#define RAMFS_TRUNC 8
#define RAMFS_APPEND 16

#define RAMFS_SEEK_SET 0
#define RAMFS_SEEK_CUR 1
#define RAMFS_SEEK_END 2

     *     _new(gs_memory_t *mem, int size); /* size is in KiB */
void     _destroy(gs_memory_t *,      * fs);
int     _error(const      * fs);
ramhandle *     _open(gs_memory_t *mem,      * fs,const char * filename,int mode);
int     _blocksize(     * fs);
int     _blocksfree(     * fs);
int     _unlink(     * fs,const char *filename);
int     _rename(     * fs,const char *oldname,const char *newname);
    _enum *     _enum_new(     * fs);
char*     _enum_next(    _enum * e);
void     _enum_end(    _enum * e);
int ramfile_read(ramhandle * handle,void * buf,int len);
int ramfile_write(ramhandle * handle,const void * buf,int len);
int ramfile_seek(ramhandle * handle,int pos,int whence);
int ramfile_eof(ramhandle * handle);
int ramfile_tell(ramhandle * handle);
int ramfile_size(ramhandle * handle);
void ramfile_close(ramhandle * handle);
int ramfile_error(ramhandle * handle);

#endif /* __RAMFS_H__ */
