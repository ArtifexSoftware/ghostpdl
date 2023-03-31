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


/* Font API support : UFST common initialization */

/* GS includes : */
#include "std.h"
#include "gx.h"
#include "stream.h"
#include "strmio.h"
#include "gsmalloc.h"
#include "gxfixed.h" /* required by gxchar.h */
#include "gxchar.h"  /* for MAX_CCACHE_TEMP_BITMAP_BITS */

/* UFST includes : */
#undef frac_bits
#include "cgconfig.h"
#include "ufstport.h"
#include "shareinc.h"
/* GS includes : */
#include "gxfapiu.h"

#define MAX_STATIC_FCO_COUNT 4

/* Slightly nasty: UFST doesn't provide for data to be
   passed through it, so we've got to store a reference
   to our context here.
   When we fix the UFST_REENTRANT option, there will be
   provision for passing the context through the UFST.
*/
#if UFST_REENTRANT
/* not currently supported */
#else
static gs_memory_t *gs_mem_ctx = NULL;
#endif

/* -------------------- UFST callback dispatcher ------------- */

/* This code provides dispatching UFST callbacks to the PDL */

struct IF_STATE;

static LPUB8
stub_PCLEO_charptr(FSP LPUB8 pfont_hdr, UW16 sym_code)
{
    return NULL;
}

static LPUB8
stub_PCLchId2ptr(FSP UW16 chId)
{
    return NULL;
}

static LPUB8
stub_PCLglyphID2Ptr(FSP UW16 glyphID)
{
    return NULL;
}

/* This global is defined here because the UFST needs it to link against. */
/* set to 1 to display UFST debugging statements */
GLOBAL const SW16 trace_sw = 0;

/*
    The following 4 variables are defined statically

They could be stored in the gs_lib_ctx but that would require casting the types
to avoid including ufst's typedefs.
*/

static LPUB8(*m_PCLEO_charptr) (FSP LPUB8 pfont_hdr, UW16 sym_code) =
    stub_PCLEO_charptr;
static LPUB8(*m_PCLchId2ptr) (FSP UW16 chId) = stub_PCLchId2ptr;
static LPUB8(*m_PCLglyphID2Ptr) (FSP UW16 glyphID) = stub_PCLglyphID2Ptr;

#if !UFST_REENTRANT
static fco_list_elem static_fco_list[MAX_STATIC_FCO_COUNT] = { 0, 0, 0, 0 };
static char static_fco_paths[MAX_STATIC_FCO_COUNT][gp_file_name_sizeof];
static int static_fco_count = 0;
static bool ufst_initialized = FALSE;
#endif

LPUB8
PCLEO_charptr(FSP LPUB8 pfont_hdr, UW16 sym_code)
{
    return m_PCLEO_charptr(FSA pfont_hdr, sym_code);
}

LPUB8
PCLchId2ptr(FSP UW16 chId)
{
    return m_PCLchId2ptr(FSA chId);
}

LPUB8
PCLglyphID2Ptr(FSP UW16 glyphID)
{
    return m_PCLglyphID2Ptr(FSA glyphID);
}
/* File handling intermediates */

void *
FAPIU_fopen(char *path, char *mode)
{
    if (!gs_mem_ctx)
        return NULL;

    return ((void *)sfopen(path, mode, gs_mem_ctx));
}

int
FAPIU_fread(void *ptr, int size, int count, void *s)
{
    return (sfread(ptr, size, count, (stream *) (s)));
}

int
FAPIU_fgetc(void *s)
{
    return (sfgetc((stream *) (s)));
}

int
FAPIU_fseek(void *s, int offset, int whence)
{
    return (sfseek((stream *) (s), offset, whence));
}

int
FAPIU_frewind(void *s)
{
    return (srewind((stream *) (s)));
}

int
FAPIU_ftell(void *s)
{
    return (sftell((stream *) (s)));
}

int
FAPIU_feof(void *s)
{
    return (sfeof((stream *) (s)));
}

int
FAPIU_ferror(void *s)
{
    return (sferror((stream *) (s)));
}

int
FAPIU_fclose(void *s)
{
    return (sfclose((stream *) (s)));
}

void *
FAPIU_open(char *path, int mode)
{
    void *s;

    if (!gs_mem_ctx)
        return (void *)-1;

    /* FIXME: "mode" needs work */
    s = sfopen(path, "r+", gs_mem_ctx);
    if (!s)
        s = (void *)-1;

    return (s);
}

int
FAPIU_read(void *s, void *ptr, int count)
{
    return (sfread(ptr, 1, count, (stream *) (s)));
}

int
FAPIU_lseek(void *s, int offset, int whence)
{
    int pos = sfseek(s, offset, whence);

    if (pos >= 0) {
        pos = sftell(s);
    }
    return (pos);
}

int
fapi_ufseek(stream * s, long offset, int whence)
{
    int pos = sfseek(s, offset, whence);

    if (pos >= 0) {
        pos = sftell(s);
    }
    return (pos);
}

int
FAPIU_close(void *s)
{
    return (sfclose((stream *) (s)));
}

GLOBAL VOID
MEMinit(FSP0)
{
    uint cache_pool = MAX_CCACHE_TEMP_BITMAP_BITS << 1;
    uint buffer_pool = (uint)(4 * 1024 * 1024);
    uint chargen_pool = (uint)(16 * 1024 * 1024);

    if_state.pserver->mem_avail[CACHE_POOL] =
        if_state.pserver->mem_fund[CACHE_POOL] = cache_pool;

    if_state.pserver->mem_avail[BUFFER_POOL] =
        if_state.pserver->mem_fund[BUFFER_POOL] = buffer_pool;

    if_state.pserver->mem_avail[CHARGEN_POOL] =
        if_state.pserver->mem_fund[CHARGEN_POOL] = chargen_pool;

}
#ifndef UFST_MEMORY_CHECKING
#define UFST_MEMORY_CHECKING 0
#endif

#if UFST_MEMORY_CHECKING
static unsigned long curmem = 0;
unsigned long maxmem = 0;
#endif

GLOBAL MEM_HANDLE
MEMalloc(FSP UW16 pool, SL32 size)
{

    char *ptr;

    if(if_state.pserver->mem_avail[pool] < size)
        return (MEM_HANDLE)0;

#if UFST_MEMORY_CHECKING
    char *ptr2;

    size += sizeof(long) + 2 * sizeof(void *);
#endif

    size += sizeof(long);
    ptr = gs_malloc(gs_mem_ctx, size, 1, "UFST MEMalloc");
    if (!ptr) {
        return (NIL_MH);
    }
    else {
        *((long *)ptr) = size;

#if UFST_MEMORY_CHECKING
        *((long *)(ptr + size - sizeof(long))) = size;
        ptr2 = ptr;
#endif

        ptr += sizeof(long);

#if UFST_MEMORY_CHECKING
        if ((curmem += size) > maxmem) {
            maxmem = curmem;
        }
        size -= 2 * sizeof(long);
        *((char *)ptr) = ptr2;
        *((char *)(ptr + size - sizeof(void *))) = ptr2;
        ptr += sizeof(char *);

#endif

        return ((MEM_HANDLE) ptr);
    }
}

GLOBAL VOID
MEMfree(FSP UW16 pool, MEM_HANDLE ptr0)
{
    int size = 0;
    void *ptr1;
    char *ptr = (char *)ptr0;

#if UFST_MEMORY_CHECKING
    int size1;
    void *ptr2;

    size = sizeof(void *);
#endif

    if (!ptr) {
        return;
    }

    ptr -= (sizeof(long) + size);
    size = *((long *)ptr);

#if UFST_MEMORY_CHECKING
    ptr1 = ptr;
    size1 = size;
    curmem -= size;

    if (size1 != *((long *)(ptr + size - sizeof(long)))) {
        dprintf("Memory length corrupt!\n");
    }

    ptr1 += sizeof(long);
    size1 -= sizeof(long);
    if (*((char *)ptr) != *((char *)(ptr + size - sizeof(void *)))) {
        dprintf("Memory pointer record corrupt!\n");
    }
#endif

    memset(ptr, 0x00, size);
    gs_free(gs_mem_ctx, ptr, 0, 0, "UFST MEMfree");
}


/* Set UFST callbacks. Each PDL will want it's own character build function and must set the callbacks
 * upon language entry/initialization.
 */
/* Warning : this function may cause a reentrancy problem
     due to a modification of static variables.
     Nevertheless this problem isn't important in a
     single interpreter build, because the values
     really change on the first demand only.
     See also a comment in gs_fapiufst_finit.
   */
void
gx_set_UFST_Callbacks(LPUB8(*p_PCLEO_charptr)
                      (FSP LPUB8 pfont_hdr, UW16 sym_code),
                      LPUB8(*p_PCLchId2ptr) (FSP UW16 chId),
                      LPUB8(*p_PCLglyphID2Ptr) (FSP UW16 glyphID))
{
    m_PCLEO_charptr =
        (p_PCLEO_charptr != NULL ? p_PCLEO_charptr : stub_PCLEO_charptr);
    m_PCLchId2ptr =
        (p_PCLchId2ptr != NULL ? p_PCLchId2ptr : stub_PCLchId2ptr);
    m_PCLglyphID2Ptr =
        (p_PCLglyphID2Ptr != NULL ? p_PCLglyphID2Ptr : stub_PCLglyphID2Ptr);
}

#define MAX_OPEN_LIBRARIES  5   /* NB */
#define BITMAP_WIDTH        1   /* must be 1, 2, 4 or 8 */

/* returns negative on error,
 * 1 = "I just initialized for the first time and you might want to as well."
 * 0 = "I've already initialized but its ok to call me."
 * <0 = error.
 */
int
gx_UFST_init(gs_memory_t * mem, const UB8 * ufst_root_dir)
{
    IFCONFIG config_block;
    int status;

#if !UFST_REENTRANT
    if (ufst_initialized)
        return 0;
    gs_mem_ctx = mem;
#endif
    strcpy(config_block.ufstPath, ufst_root_dir);
    strcpy(config_block.typePath, ufst_root_dir);
    config_block.num_files = MAX_OPEN_LIBRARIES;        /* max open library files */
    config_block.bit_map_width = BITMAP_WIDTH;  /* bitmap width 1, 2 or 4 */

    /* These parameters were set in open_UFST() (fapiufst.c) but were left
       uninitialized in pl_load_built_in_fonts() (plulfont.c). */
    config_block.typePath[0] = 0;

    if ((status = CGIFinit(FSA0)) != 0) {
        dmprintf1(mem, "CGIFinit() error: %d\n", status);
        gs_mem_ctx = NULL;
        return status;
    }
    if ((status = CGIFconfig(FSA &config_block)) != 0) {
        dmprintf1(mem, "CGIFconfig() error: %d\n", status);
        gs_mem_ctx = NULL;
        return status;
    }
    CGIFfont_access(FSA DISK_ACCESS);
    if ((status = CGIFenter(FSA0)) != 0) {
        dmprintf1(mem, "CGIFenter() error: %u\n",status);
        gs_mem_ctx = NULL;
        return status;
    }
#if !UFST_REENTRANT
    ufst_initialized = TRUE;
#endif
    return 1;                   /* first time, caller may have more initialization to do */
}

int
gx_UFST_fini(void)
{
    CGIFexit(FSA0);
#if !UFST_REENTRANT
    ufst_initialized = FALSE;
    gs_mem_ctx = NULL;
#endif
    return 0;
}

/* Access to the static FCO list for the language switching project. */

fco_list_elem *
gx_UFST_find_static_fco(const char *font_file_path)
{
#if !UFST_REENTRANT
    int i;

    for (i = 0; i < static_fco_count; i++)
        if (!strcmp(static_fco_list[i].file_path, font_file_path))
            return &static_fco_list[i];
#endif
    return NULL;
}

fco_list_elem *
gx_UFST_find_static_fco_handle(SW16 fcHandle)
{
#if !UFST_REENTRANT
    int i;

    for (i = 0; i < static_fco_count; i++)
        if (static_fco_list[i].fcHandle == fcHandle)
            return &static_fco_list[i];
#endif
    return NULL;
}

SW16
gx_UFST_find_fco_handle_by_name(const char *font_file_path)
{
#if !UFST_REENTRANT
    fco_list_elem *fco = gx_UFST_find_static_fco(font_file_path);

    if (fco)
        return fco->fcHandle;
    return 0;
#endif
}

UW16
gx_UFST_open_static_fco(const char *font_file_path, SW16 * result_fcHandle)
{
#if !UFST_REENTRANT
    SW16 fcHandle;
    UW16 code;
    fco_list_elem *e;

    if (static_fco_count >= MAX_STATIC_FCO_COUNT)
        return ERR_fco_NoMem;
    code = CGIFfco_Open(FSA(UB8 *) font_file_path, &fcHandle);
    if (code != 0)
        return code;
    e = &static_fco_list[static_fco_count];
    strncpy(static_fco_paths[static_fco_count], font_file_path,
            sizeof(static_fco_paths[static_fco_count]));
    e->file_path = static_fco_paths[static_fco_count];
    e->fcHandle = fcHandle;
    e->open_count = -1;         /* Unused for static FCOs. */
    static_fco_count++;
    *result_fcHandle = fcHandle;
    return 0;
#else
    **result_fcHandle = -1;
    return ERR_fco_NoMem;
#endif
}

UW16
gx_UFST_close_static_fco(SW16 fcHandle)
{
#if !UFST_REENTRANT
    int i;

    for (i = 0; i < static_fco_count; i++)
        if (static_fco_list[i].fcHandle == fcHandle)
            break;
    if (i >= static_fco_count)
        return ERR_fco_NoMem;
    CGIFfco_Close(FSA fcHandle);
    for (i++; i < static_fco_count; i++) {
        static_fco_list[i - 1] = static_fco_list[i];
        strcpy(static_fco_paths[i - 1], static_fco_paths[i]);
    }
    static_fco_count--;
#endif
    return 0;
}

void
gx_UFST_close_static_fcos()
{
#if !UFST_REENTRANT
    for (; static_fco_count;)
        gx_UFST_close_static_fco(static_fco_list[0].fcHandle);
#endif
}

#if GRAYSCALING
/* ------------------------------- BLACKPIX --------------------------------
 * Description: Called by UFST gichar function.
 * Returns:     VOID
 * Notes:       This is a stub implementation for graymap.
 */
GLOBAL VOID
BLACKPIX(FSP SW16 x, SW16 y)
{
    return;
}

/* -------------------------------- GRAYPIX --------------------------------
 * Description: Called by UFST gichar function.
 * Returns:     VOID
 * Notes:       This is a stub implementation for graymap.
 */
GLOBAL VOID
GRAYPIX(FSP SW16 x, SW16 y, SW16 v)
{
    return;
}
#endif /* GRAYSCALING */
