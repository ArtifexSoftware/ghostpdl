/* Copyright (C) 2001-2026 Artifex Software, Inc.
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

#ifndef __RAMFS_H__
#define __RAMFS_H__

#include "stream.h"

#define RAMFS_BLOCKSIZE 1024

typedef struct _ramfs ramfs;
typedef struct _ramdirent ramdirent;
typedef struct _ramhandle ramhandle;
typedef struct _ramfs_enum ramfs_enum;

/*
  ramfs_new: NOMEM
  ramfs_open: NOTFOUND
  ramfs_unlink: NOTFOUND
  ramfs_enum_new: NOMEM
  ramfs_enum_next: none
  ramfs_enum_end: none
  ramfile_read: none
  ramfile_write: NOSPACE, NOMEM, NOACCESS
  ramfile_seek: none
  ramfile_pos: none
  ramfile_close: none
*/

/* Error constants */
#define RAMFS_NOERROR 0
#define RAMFS_EOF 1
#define RAMFS_NOTFOUND 2
#define RAMFS_NOACCESS 5
#define RAMFS_NOMEM 6
#define RAMFS_NOSPACE 7
#define RAMFS_BADRANGE 8
#define RAMFS_DELETEOPENFILE 9

/* Open mode flags */
#define RAMFS_READ   s_mode_read      /* 1 */
#define RAMFS_WRITE  s_mode_write    /* 2 */
#define RAMFS_SEEK   s_mode_seek      /* 4 */
#define RAMFS_APPEND s_mode_append  /* 8 */
#define RAMFS_CREATE 16
#define RAMFS_TRUNC  32

#define RAMFS_SEEK_SET 0
#define RAMFS_SEEK_CUR 1
#define RAMFS_SEEK_END 2

ramfs * ramfs_new(gs_memory_t *mem, int size); /* size is in KiB */
void ramfs_destroy(gs_memory_t *, ramfs * fs);
int ramfs_error(const ramfs * fs);
ramhandle * ramfs_open(gs_memory_t *mem, ramfs * fs,const char * filename,int mode);
int ramfs_blocksize(ramfs * fs);
int ramfs_blocksfree(ramfs * fs);
int ramfs_unlink(ramfs * fs,const char *filename);
int ramfs_rename(ramfs * fs,const char *oldname,const char *newname);
ramfs_enum * ramfs_enum_new(ramfs * fs);
char* ramfs_enum_next(ramfs_enum * e);
void ramfs_enum_end(ramfs_enum * e);
int ramfile_read(ramhandle * handle,void * buf,int len);
int ramfile_write(ramhandle * handle,const void * buf,int len);
int ramfile_seek(ramhandle * handle,gs_offset_t pos,int whence);
int ramfile_eof(ramhandle * handle);
int ramfile_tell(ramhandle * handle);
int ramfile_size(ramhandle * handle);
void ramfile_close(ramhandle * handle);
int ramfile_error(ramhandle * handle);

/* gp_file implementation */
typedef struct {
    gp_file base;
    ramhandle *handle;
    int error;
} gp_file_RAM;

int gp_file_ram_close(gp_file *rf);
int gp_file_ram_getc(gp_file *rf);
int gp_file_ram_putc(gp_file *rf, int);
int gp_file_ram_read(gp_file *rf, size_t size, unsigned int count, void *buf);
int gp_file_ram_write(gp_file *rf, size_t size, unsigned int count, const void *buf);
int gp_file_ram_seek(gp_file *rf, gs_offset_t offset, int whence);
gs_offset_t gp_file_ram_tell(gp_file *rf);
int gp_file_ram_eof(gp_file *rf);
gp_file *gp_file_ram_dup(gp_file *rf, const char *mode);
int gp_file_ram_seekable(gp_file *rf);
int gp_file_ram_pread(gp_file *rf, size_t count, gs_offset_t offset, void *buf);
int gp_file_ram_pwrite(gp_file *rf, size_t count, gs_offset_t offset, const void *buf);
int gp_file_ram_is_char_buffered(gp_file *rf);
void gp_file_ram_fflush(gp_file *rf);
int gp_file_ram_ferror(gp_file *rf);
FILE *gp_file_ram_get_file(gp_file *rf);
void gp_file_ram_clearerror(gp_file *rf);
gp_file *gp_file_ram_reopen(gp_file *f, const char *fname, const char *mode);

#endif /* __RAMFS_H__ */
