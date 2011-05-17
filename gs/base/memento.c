/* Copyright (C) 2011 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

/* Inspired by Fortify by Simon P Bullen. */

#define COMPILING_MEMENTO_C
#include "memento.h"

/* Define the underlying allocators, just in case */
void *MEMENTO_UNDERLYING_MALLOC(size_t);
void MEMENTO_UNDERLYING_FREE(void *);
void *MEMENTO_UNDERLYING_REALLOC(void *,size_t);
void *MEMENTO_UNDERLYING_CALLOC(size_t,size_t);

#ifdef MEMENTO

enum {
    Memento_PreSize  = 16,
    Memento_PostSize = 16
};

typedef struct Memento_BlkHeader Memento_BlkHeader;

struct Memento_BlkHeader
{
    size_t             rawsize;
    int                sequence;
    int                lastCheckedOK;
    Memento_BlkHeader *next;
    char               preblk[Memento_PreSize];
};

/* In future this could (should) be a smarter data structure, like, say,
 * splay trees. For now, we use a list.
 */
typedef struct Memento_Blocks
{
    Memento_BlkHeader *head;
} Memento_Blocks;

/* And our global structure */
static struct {
    int            inited;
    Memento_Blocks used;
    Memento_Blocks free;
    size_t         freeListSize;
    int            sequence;
    int            paranoia;
    int            paranoidAt;
    int            countdown;
    int            lastChecked;
    int            breakAt;
    size_t         alloc;
    size_t         peakAlloc;
    size_t         totalAlloc;
    size_t         numMallocs;
    size_t         numFrees;
    size_t         numReallocs;
} globals;


#define MEMENTO_EXTRASIZE (sizeof(Memento_BlkHeader) + Memento_PostSize)

/* Round up size S to the next multiple of N (where N is a power of 2) */
#define MEMENTO_ROUNDUP(S,N) ((S + N-1)&~(N-1))

#define MEMBLK_SIZE(s) MEMENTO_ROUNDUP(s + MEMENTO_EXTRASIZE, MEMENTO_MAXALIGN)

#define MEMBLK_FROMBLK(B)   (&((Memento_BlkHeader*)(void *)(B))[-1])
#define MEMBLK_TOBLK(B)     ((void*)(&((Memento_BlkHeader*)(void*)(B))[1]))
#define MEMBLK_POSTPTR(B) \
          (&((char *)(void *)(B))[(B)->rawsize + sizeof(Memento_BlkHeader)])

void Memento_breakpoint(void)
{
    /* A handy externally visible function for breakpointing */
#if 0 /* Enable this to force automatic breakpointing */
#ifdef DEBUG
#ifdef _MSC_VER
    __asm int 3;
#endif
#endif
#endif
}

static void Memento_addBlock(Memento_Blocks *blks, Memento_BlkHeader *b)
{
    b->next    = blks->head;
    blks->head = b;
    memset(b->preblk, MEMENTO_PREFILL, Memento_PreSize);
    memset(MEMBLK_POSTPTR(b), MEMENTO_POSTFILL, Memento_PostSize);
}

typedef struct BlkCheckData {
    int found;
    int preCorrupt;
    int postCorrupt;
    int freeCorrupt;
    int index;
} BlkCheckData;

static int Memento_Internal_checkAllocedBlock(Memento_BlkHeader *b, void *arg)
{
    int           i;
    char         *p;
    int           corrupt = 0;
    BlkCheckData *data = (BlkCheckData *)arg;
    int           res = 0;

    p = b->preblk;
    i = Memento_PreSize;
    do {
        corrupt |= (*p++ ^ (char)MEMENTO_PREFILL);
    } while (--i);
    if (corrupt) {
        data->preCorrupt = 1;
    }
    p = MEMBLK_POSTPTR(b);
    i = Memento_PreSize;
    do {
        corrupt |= (*p++ ^ (char)MEMENTO_POSTFILL);
    } while (--i);
    if (corrupt) {
        data->postCorrupt = 1;
    }
    if ((data->freeCorrupt | data->preCorrupt | data->postCorrupt) == 0) {
        b->lastCheckedOK = globals.sequence;
    }
    data->found |= 1;
    return 0;
}

static int Memento_Internal_checkFreedBlock(Memento_BlkHeader *b, void *arg)
{
    int           i;
    char         *p;
    BlkCheckData *data = (BlkCheckData *)arg;

    p = MEMBLK_TOBLK(b);
    i = b->rawsize;
    do {
        if (*p++ != (char)MEMENTO_FREEFILL)
            break;
    } while (--i);
    if (i) {
        data->freeCorrupt = 1;
        data->index       = b->rawsize-i;
    }
    return Memento_Internal_checkAllocedBlock(b, arg);
}

static void Memento_removeBlock(Memento_Blocks    *blks,
                                const char        *listName,
                                Memento_BlkHeader *b)
{
    Memento_BlkHeader **head = &blks->head;
    while ((*head) && (*head != b)) {
        head = &(*head)->next;
    }
    if (*head == NULL) {
        /* FAIL! Will have been reported to user earlier, so just exit. */
        return;
    }
    (*head) = (*head)->next;
}

static void Memento_touchBlock(Memento_Blocks    *blks,
                               const char        *listName,
                               Memento_BlkHeader *b)
{
    Memento_BlkHeader *head = blks->head;
    while (head && (head != b)) {
        head = head->next;
    }
    if (head == NULL) {
        /* FAIL! */
        /* FIXME: Check the free list */
        fprintf(stderr, "Block %x not in %s list!\n", MEMBLK_TOBLK(b), listName);
        Memento_breakpoint();
        return;
    }
    Memento_Internal_checkBlock(b, NULL);
}

static int Memento_Internal_makeSpace(size_t space)
{
    Memento_BlkHeader **head, **next;
    size_t listSize = globals.freeListSize;
    /* If too big, it can never go on the freelist */
    if (space > MEMENTO_FREELIST_MAX)
        return 0;
    /* Pretend we added it on. */
    globals.freeListSize += space;
    /* Did it fit? */
    if (globals.freeListSize <= MEMENTO_FREELIST_MAX) {
        return 1;
    }
    /* We need to bin some entries from the list */
    listSize = MEMENTO_FREELIST_MAX-space;
    head = &globals.free.head;
    while (*head && listSize != 0) {
        if (listSize < MEMBLK_SIZE((*head)->rawsize)) {
            break;
        }
        listSize -= MEMBLK_SIZE((*head)->rawsize);
        head = &(*head)->next;
    }
    /* Free all blocks forwards from here */
    *head = NULL;
    while (*head) {
        next = &(*head)->next;
        globals.freeListSize -= MEMBLK_SIZE((*head)->rawsize);
        MEMENTO_UNDERLYING_FREE((*head));
        head = next;
    }
    return 1;
}

static int Memento_appBlocks(Memento_Blocks *blks,
                             int             (*app)(Memento_BlkHeader *,
                                                    void *),
                             void           *arg)
{
    Memento_BlkHeader *head = blks->head;
    int                result;
    while (head) {
        result = app(head, arg);
        if (result)
            return result;
        head = head->next;
    }
    return 0;
}

static int Memento_appBlock(Memento_Blocks    *blks,
                            int                (*app)(Memento_BlkHeader *,
                                                      void *),
                            void              *arg,
                            Memento_BlkHeader *b)
{
    Memento_BlkHeader *head = blks->head;
    int                result;
    while (head && head != b) {
        head = head->next;
    }
    if (head == b) {
        return app(head, arg);
    }
    return 0;
}

static int Memento_listBlock(Memento_BlkHeader *b,
                              void              *arg)
{
    int *counts = (int *)arg;
    fprintf(stderr, "    %x:(size=%d,num=%d)\n",
            MEMBLK_TOBLK(b), b->rawsize, b->sequence);
    counts[0]++;
    counts[1]+= b->rawsize;
    return 0;
}

void Memento_listBlocks(void) {
    int counts[2];
    counts[0] = 0;
    counts[1] = 0;
    fprintf(stderr, "Allocated blocks:\n");
    Memento_appBlocks(&globals.used, Memento_listBlock, &counts[0]);
    fprintf(stderr, "  Total number of blocks = %d\n", counts[0]);
    fprintf(stderr, "  Total size of blocks = %d\n", counts[1]);
}

static void Memento_fin(void)
{
    fprintf(stderr, "Total memory malloced = %d bytes\n", globals.totalAlloc);
    fprintf(stderr, "Peak memory malloced = %d bytes\n", globals.peakAlloc);
    fprintf(stderr, "%d mallocs, %d frees, %d reallocs\n", globals.numMallocs,
            globals.numFrees, globals.numReallocs);
    fprintf(stderr, "Average allocation size %d bytes\n",
            globals.totalAlloc/globals.numMallocs);
    if (globals.used.head != NULL) {
        Memento_listBlocks();
        Memento_breakpoint();
    }
}

static void Memento_init(void)
{
    memset(&globals, 0, sizeof(globals));
    globals.inited    = 1;
    globals.used.head = NULL;
    globals.free.head = NULL;
    globals.sequence  = 1;
    globals.paranoia  = 1024;
    globals.countdown = 1024;
    atexit(Memento_fin);
}

static void Memento_event(void)
{
    if ((globals.sequence == globals.paranoidAt) &&
        (globals.sequence != 0)) {
        globals.paranoia = 1;
        globals.countdown = 1;
    }

    if (--globals.countdown == 0) {
        Memento_checkAllMemory();
        globals.countdown = globals.paranoia;
    }
    globals.sequence++;

    if (globals.sequence == globals.breakAt)
        Memento_breakpoint();
}

void Memento_breakAt(int event)
{
    globals.breakAt = event;
}

void *Memento_malloc(size_t s)
{
    Memento_BlkHeader *memblk;
    int               *blk;
    size_t             smem = MEMBLK_SIZE(s);

    if (!globals.inited)
        Memento_init();

    Memento_event();

    if (s == 0)
        return NULL;

    globals.numMallocs++;

    memblk = MEMENTO_UNDERLYING_MALLOC(smem);
    if (memblk == NULL)
        return NULL;
    globals.alloc      += s;
    globals.totalAlloc += s;
    if (globals.peakAlloc < globals.alloc)
        globals.peakAlloc = globals.alloc;
    memset(MEMBLK_TOBLK(memblk), MEMENTO_ALLOCFILL, s);
    memblk->rawsize       = s;
    memblk->sequence      = globals.sequence;
    memblk->lastCheckedOK = memblk->sequence;
    Memento_addBlock(&globals.used, memblk);
    return MEMBLK_TOBLK(memblk);
}

void *Memento_calloc(size_t n, size_t s)
{
    void *block = Memento_malloc(n*s);

    if (block)
        memset(block, 0, n*s);
    return block;
}

static int checkBlock(Memento_BlkHeader *memblk, const char *action)
{
    BlkCheckData data;

    memset(&data, 0, sizeof(data));
    Memento_appBlock(&globals.used, Memento_Internal_checkAllocedBlock,
                     &data, memblk);
    if (!data.found) {
        /* Failure! */
        fprintf(stderr, "Attempt to %s block %x(size=%d,num=%d) not on allocated list!\n",
                action, memblk, memblk->rawsize, memblk->sequence);
        Memento_breakpoint();
        return 1;
    } else if (data.preCorrupt || data.postCorrupt) {
        fprintf(stderr, "Block %x(size=%d,num=%d) found to be corrupted on %s!\n",
                action, memblk->rawsize, memblk->sequence, action);
        if (data.preCorrupt) {
            fprintf(stderr, "Preguard corrupted\n");
        }
        if (data.postCorrupt) {
            fprintf(stderr, "Postguard corrupted\n");
        }
        fprintf(stderr, "Block last checked OK at allocation %d. Now %d.\n",
                memblk->lastCheckedOK, globals.sequence);
        Memento_breakpoint();
        return 1;
    }
    return 0;
}

void Memento_free(void *blk)
{
    Memento_BlkHeader *memblk;
    int                res;

    if (!globals.inited)
        Memento_init();

    Memento_event();

    if (blk == NULL)
        return;

    memblk = MEMBLK_FROMBLK(blk);

    if (checkBlock(memblk, "free"))
        return;

    globals.alloc -= memblk->rawsize;
    globals.numFrees++;

    Memento_removeBlock(&globals.used, "allocated", memblk);

    if (Memento_Internal_makeSpace(MEMBLK_SIZE(memblk->rawsize))) {
        Memento_addBlock(&globals.free, memblk);
        memset(MEMBLK_TOBLK(memblk), MEMENTO_FREEFILL, memblk->rawsize);
    } else {
        MEMENTO_UNDERLYING_FREE(memblk);
    }
}

void *Memento_realloc(void *blk, size_t newsize)
{
    Memento_BlkHeader *memblk, *newmemblk;
    size_t             rawsize, newsizemem;

    if (!globals.inited)
        Memento_init();

    if (blk == NULL)
        return Memento_malloc(newsize);
    if (newsize == 0) {
        Memento_free(blk);
        return NULL;
    }

    memblk     = MEMBLK_FROMBLK(blk);

    Memento_event();

    if (checkBlock(memblk, "realloc"))
        return NULL;

    newsizemem = MEMBLK_SIZE(newsize);
    rawsize    = memblk->rawsize;
    Memento_removeBlock(&globals.used, "allocated", memblk);
    newmemblk  = MEMENTO_UNDERLYING_REALLOC(memblk, newsizemem);
    if (newmemblk == NULL)
    {
        Memento_addBlock(&globals.used, memblk);
        return NULL;
    }
    globals.numReallocs++;
    globals.totalAlloc += newsize;
    globals.alloc      -= newmemblk->rawsize;
    globals.alloc      += newsize;
    if (globals.peakAlloc < globals.alloc)
        globals.peakAlloc = globals.alloc;
    if (newmemblk->rawsize < newsize)
        memset(MEMBLK_TOBLK(newmemblk), MEMENTO_ALLOCFILL,
               newsize - newmemblk->rawsize);
    newmemblk->rawsize = newsize;
    Memento_addBlock(&globals.used, newmemblk);
    memset(newmemblk->preblk, MEMENTO_PREFILL, Memento_PreSize);
    memset(MEMBLK_POSTPTR(newmemblk), MEMENTO_POSTFILL, Memento_PostSize);
    return MEMBLK_TOBLK(newmemblk);
}

void Memento_checkBlock(void *blk)
{
    Memento_BlkHeader *memblk;

    if (blk == NULL)
        return;
    memblk = MEMBLK_FROMBLK(blk);
    if (checkBlock(memblk, "check"))
        return;
}

static int Memento_Internal_checkAllAlloced(Memento_BlkHeader *memblk, void *arg)
{
    BlkCheckData *data = (BlkCheckData *)arg;

    Memento_Internal_checkAllocedBlock(memblk, data);
    if (data->preCorrupt || data->postCorrupt) {
        if ((data->found & 2) == 0) {
            fprintf(stderr, "Allocated blocks:\n");
            data->found |= 2;
        }
        fprintf(stderr, "  Block %x(size=%d,num=%d)",
                memblk, memblk->rawsize, memblk->sequence);
        if (data->preCorrupt) {
            fprintf(stderr, " Preguard ");
        }
        if (data->postCorrupt) {
            fprintf(stderr, "%s Postguard ",
                    (data->preCorrupt ? "&" : ""));
        }
        fprintf(stderr, "corrupted.\n    "
                "Block last checked OK at allocation %d. Now %d.\n",
                memblk->lastCheckedOK, globals.sequence);
        data->preCorrupt  = 0;
        data->postCorrupt = 0;
        data->freeCorrupt = 0;
    }
    return 0;
}

static int Memento_Internal_checkAllFreed(Memento_BlkHeader *memblk, void *arg)
{
    BlkCheckData *data = (BlkCheckData *)arg;

    Memento_Internal_checkFreedBlock(memblk, data);
    if (data->preCorrupt || data->postCorrupt || data->freeCorrupt) {
        if ((data->found & 4) == 0) {
            fprintf(stderr, "Freed blocks:\n");
            data->found |= 4;
        }
        fprintf(stderr, "  Block %x(size=%d,num=%d) ",
                memblk, memblk->rawsize, memblk->sequence);
        if (data->freeCorrupt) {
            fprintf(stderr, "index %d onwards ", data->index);
            if (data->preCorrupt) {
                fprintf(stderr, "+ preguard ");
            }
            if (data->postCorrupt) {
                fprintf(stderr, "+ postguard ");
            }
        } else {
            if (data->preCorrupt) {
                fprintf(stderr, " preguard ");
            }
            if (data->postCorrupt) {
                fprintf(stderr, "%s Postguard ",
                        (data->preCorrupt ? "+" : ""));
            }
        }
        fprintf(stderr, "corrupted.\n    "
                "Block last checked OK at allocation %d. Now %d.\n",
                memblk->lastCheckedOK, globals.sequence);
        data->preCorrupt  = 0;
        data->postCorrupt = 0;
        data->freeCorrupt = 0;
    }
    return 0;
}

void Memento_checkAllMemory(void)
{
    BlkCheckData data;

    memset(&data, 0, sizeof(data));
    Memento_appBlocks(&globals.used, Memento_Internal_checkAllAlloced, &data);
    Memento_appBlocks(&globals.free, Memento_Internal_checkAllFreed, &data);
    if (data.found & 6)
        Memento_breakpoint();
}

void Memento_setParanoia(int i)
{
    globals.paranoia = i;
}

void Memento_paranoidAt(int i)
{
    globals.paranoidAt = i;
}

int Memento_getBlockNum(void *b)
{
    Memento_BlkHeader *memblk;
    if (b == NULL)
        return 0;
    memblk = MEMBLK_FROMBLK(b);
    return (memblk->sequence);
}

void Memento_check(void)
{
    fprintf(stderr, "Checking memory\n");
    Memento_checkAllMemory();
    fprintf(stderr, "Memory checked!\n");
}

typedef struct findBlkData {
    void              *addr;
    Memento_BlkHeader *blk;
    int                flags;
} findBlkData;

static int Memento_containsAddr(Memento_BlkHeader *b,
                                void *arg)
{
    findBlkData *data = (findBlkData *)arg;
    char *blkend = &((char *)MEMBLK_TOBLK(b))[b->rawsize];
    if ((MEMBLK_TOBLK(b) <= data->addr) &&
        (blkend > data->addr)) {
        data->blk = b;
        data->flags = 1;
        return 1;
    }
    if ((b <= data->addr) &&
        (MEMBLK_TOBLK(b) > data->addr)) {
        data->blk = b;
        data->flags = 2;
        return 1;
    }
    if ((blkend <= data->addr) &&
        (blkend + Memento_PostSize > data->addr)) {
        data->blk = b;
        data->flags = 3;
        return 1;
    }
    return 0;
}

void Memento_find(void *a)
{
    findBlkData data;

    data.addr  = a;
    data.blk   = NULL;
    data.flags = 0;
    Memento_appBlocks(&globals.used, Memento_containsAddr, &data);
    if (data.blk != NULL) {
        fprintf(stderr, "Address %x is in %sallocated block %x(%d)\n",
                data.addr,
                (data.flags == 1 ? "" : (data.flags == 2 ?
                                         "preguard of " : "postguard of ")),
                data.blk, data.blk->rawsize);
        return;
    }
    Memento_appBlocks(&globals.free, Memento_containsAddr, &data);
    if (data.blk != NULL) {
        fprintf(stderr, "Address %x is in %sfreed block %x(%d)\n",
                data.addr,
                (data.flags == 1 ? "" : (data.flags == 2 ?
                                         "preguard of " : "postguard of ")),
                data.blk, data.blk->rawsize);
        return;
    }
}

#else

/* Just in case anyone has left some debugging code in... */
void (Memento_breakpoint)(void)
{
}

void (Memento_checkBlock)(void *b)
{
}

void (Memento_checkAllMemory)(void)
{
}

void (Memento_check)(void)
{
}

void (Memento_setParanoia)(int i)
{
}

void (Memento_paranoidAt)(int i)
{
}

void (Memento_breakAt)(int i)
{
}

int  (Memento_getBlockNum)(void *i)
{
    return 0;
}

void (Memento_find)(void *a)
{
}

#undef Memento_malloc
#undef Memento_free
#undef Memento_realloc
#undef Memento_calloc

void *Memento_malloc(size_t size)
{
    return MEMENTO_UNDERLYING_MALLOC(size);
}

void Memento_free(void *b)
{
    MEMENTO_UNDERLYING_FREE(b);
}

void *Memento_realloc(void *b, size_t s)
{
    return MEMENTO_UNDERLYING_REALLOC(b, s);
}

void *Memento_calloc(size_t n, size_t s)
{
    return MEMENTO_UNDERLYING_CALLOC(n, s);
}


#endif
