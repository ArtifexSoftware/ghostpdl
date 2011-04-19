/* A simple memory allocator, based on the the allocator for ghostpcl */

#include "malloc_.h"
#include "memory_.h"
#include "gdebug.h"
#include "gsmemret.h" /* for gs_memory_type_ptr_t */
#include "gsmalloc.h"
#include "gsstype.h"
#include "gslt_alloc.h"

static int num_alloc_called = 0;
static int num_resize_called = 0;
static int num_free_called = 0;

/* a screwed up mess, we try to make it manageable here */
extern const gs_memory_struct_type_t st_bytes;

/* assume doubles are the largest primitive types and malloc alignment
   is consistent.  Covers the machines we care about */
static inline uint
round_up_to_align(uint size)
{
    uint result = (size + (ARCH_ALIGN_MEMORY_MOD - 1)) & -ARCH_ALIGN_MEMORY_MOD;
    return result;
}

/* accessors to get size and type given the pointer returned to the
   client, *not* the pointer returned by malloc or realloc */
static inline uint
get_size(byte *bptr)
{
    /* unpack the unsigned int we stored 2 words behind the object at
       alloc time... back up 2 */
    int size_offset = -round_up_to_align(1) * 2;
    uint size;
    /* unpack */
    memcpy(&size, &bptr[size_offset], sizeof(uint));
    return size;
}

static inline gs_memory_type_ptr_t
get_type(byte *bptr)
{
    /* unpack the pointer we stored 1 word behind the object at
       alloc time... back up 1 */
    gs_memory_type_ptr_t type;
    int type_offset = -round_up_to_align(1);
    /* unpack */
    memcpy(&type, &bptr[type_offset], sizeof(gs_memory_type_ptr_t));
    return type;
}

/* accessors to set size and typen give the pointer that was returned
   by malloc or realloc, *not* the pointer returned to the client */
static inline void
set_size(byte *bptr, uint size)
{
    memcpy(bptr, &size, sizeof(size));
}

static inline void
set_type(byte *bptr, gs_memory_type_ptr_t type)
{
    memcpy(&bptr[round_up_to_align(1)], &type, sizeof(type));
    return;
}

/* all of the allocation routines modulo realloc reduce to the this
   function */
static byte *
gslt_alloc(gs_memory_t *mem, uint size, gs_memory_type_ptr_t type, client_name_t cname)
{

    uint minsize, newsize;
    /* NB apparently there is code floating around that does 0 size
       mallocs.  sigh. */
    if ( size == 0 )
        return NULL;

    /* use 2 starting machine words for size and type - assumes
       malloc() returns on max boundary and first 2 words will hold
       two longs.  Doesn't check for overflow - malloc will fail for
       us.  Update size. */
    minsize = round_up_to_align(1);
    newsize = size + minsize + minsize;
    {

        byte *ptr = (byte *)malloc(newsize);
        if ( !ptr )
            return NULL;
        num_alloc_called ++;
#ifdef DEBUG
        if_debug2('A', "[da]:malloc:%p:%s\n", &ptr[minsize * 2], cname );
#endif
        /* set the type and size */
        set_type(ptr, type);
        set_size(ptr, size);
        /* initialize for debugging */
#ifdef DEBUG
        if ( gs_debug_c('@') )
            memset(&ptr[minsize * 2], 0xff, get_size(&ptr[minsize * 2]));
#endif
        /* return the memory after the size and type words. */
        return &ptr[minsize * 2];
    }
}

static byte *
gslt_alloc_bytes_immovable(gs_memory_t * mem, uint size, client_name_t cname)
{
    return gslt_alloc(mem, size, &st_bytes, cname);
}

static byte *
gslt_alloc_bytes(gs_memory_t * mem, uint size, client_name_t cname)
{
    return gslt_alloc(mem, size, &st_bytes, cname);
}

static void *
gslt_alloc_struct_immovable(gs_memory_t * mem, gs_memory_type_ptr_t pstype,
                         client_name_t cname)
{
    return gslt_alloc(mem, pstype->ssize, pstype, cname);
}

static void *
gslt_alloc_struct(gs_memory_t * mem, gs_memory_type_ptr_t pstype,
               client_name_t cname)
{
    return gslt_alloc(mem, pstype->ssize, pstype, cname);
}

static byte *
gslt_alloc_byte_array_immovable(gs_memory_t * mem, uint num_elements,
                             uint elt_size, client_name_t cname)
{
    return gslt_alloc_bytes(mem, num_elements * elt_size, cname);
}

static byte *
gslt_alloc_byte_array(gs_memory_t * mem, uint num_elements, uint elt_size,
                   client_name_t cname)
{
    return gslt_alloc_bytes(mem, num_elements * elt_size, cname);
}

static void *
gslt_alloc_struct_array_immovable(gs_memory_t * mem, uint num_elements,
                           gs_memory_type_ptr_t pstype, client_name_t cname)
{
    return gslt_alloc(mem, num_elements * pstype->ssize, pstype, cname);
}

static void *
gslt_alloc_struct_array(gs_memory_t * mem, uint num_elements,
                     gs_memory_type_ptr_t pstype, client_name_t cname)
{
    return gslt_alloc(mem, num_elements * pstype->ssize, pstype, cname);
}

static void *
gslt_resize_object(gs_memory_t * mem, void *obj, uint new_num_elements, client_name_t cname)
{
    byte *ptr;
    byte *bptr = (byte *)obj;
    /* get the type from the old object */
    gs_memory_type_ptr_t objs_type = get_type(obj);
    /* type-and-size header size */
    ulong header_size = round_up_to_align(1) + round_up_to_align(1);
    /* get new object's size */
    ulong new_size = (objs_type->ssize * new_num_elements) + header_size;
    /* replace the size field */
    ptr = (byte *)realloc(&bptr[-header_size], new_size);
    if ( !ptr )
        return NULL;

    num_resize_called ++;

    /* da for debug allocator - so scripts can parse the trace */
    if_debug2('A', "[da]:realloc:%p:%s\n", ptr, cname );
    /* we reset size and type - the type in case realloc moved us */
    set_size(ptr, new_size - header_size);
    set_type(ptr, objs_type);
    return &ptr[round_up_to_align(1) * 2];
}

static void
gslt_free_object(gs_memory_t * mem, void *ptr, client_name_t cname)
{
    if ( ptr != NULL ) {
        byte *bptr = (byte *)ptr;
        uint header_size = round_up_to_align(1) * 2;
#ifdef DEBUG
        if ( gs_debug_c('@') )
            memset(bptr-header_size, 0xee, header_size + get_size(ptr));
#endif
        free(bptr-header_size);

        num_free_called ++;

#ifdef DEBUG
            /* da for debug allocator - so scripts can parse the trace */
        if_debug2('A', "[da]:free:%p:%s\n", ptr, cname );
#endif
    }
}

static byte *
gslt_alloc_string_immovable(gs_memory_t * mem, uint nbytes, client_name_t cname)
{
    /* we just alloc bytes here */
    return gslt_alloc_bytes(mem, nbytes, cname);
}

static byte *
gslt_alloc_string(gs_memory_t * mem, uint nbytes, client_name_t cname)
{
    /* we just alloc bytes here */
    return gslt_alloc_bytes(mem, nbytes, cname);
}

static byte *
gslt_resize_string(gs_memory_t * mem, byte * data, uint old_num, uint new_num,
                client_name_t cname)
{
    /* just resize object - ignores old_num */
    return gslt_resize_object(mem, data, new_num, cname);
}

static void
gslt_free_string(gs_memory_t * mem, byte * data, uint nbytes,
              client_name_t cname)
{
    gslt_free_object(mem, data, cname);
    return;
}

static void
gslt_status(gs_memory_t * mem, gs_memory_status_t * pstat)
{
    return;
}

static void
gslt_enable_free(gs_memory_t * mem, bool enable)
{
    return;
}

static void
gslt_free_all(gs_memory_t * mem, uint free_mask, client_name_t cname)
{
    return;
}

static void
gslt_consolidate_free(gs_memory_t *mem)
{
    return;
}

static uint
gslt_object_size(gs_memory_t * mem, const void /*obj_header_t */ *obj)
{
    return get_size((byte *)obj);
}

static gs_memory_type_ptr_t
gslt_object_type(gs_memory_t * mem, const void /*obj_header_t */ *obj)
{
    return get_type((byte *)obj);
}

static int
gslt_register_root(gs_memory_t * mem, gs_gc_root_t * rp, gs_ptr_type_t ptype,
                 void **up, client_name_t cname)
{
    return 0;
}

static void
gslt_unregister_root(gs_memory_t * mem, gs_gc_root_t * rp, client_name_t cname)
{
    return;
}

/* Define a vacuous recovery procedure. */
static gs_memory_recover_status_t
no_recover_proc(gs_memory_retrying_t *rmem, void *proc_data)
{
    return RECOVER_STATUS_NO_RETRY;
}

/* forward decl */
static gs_memory_t * gslt_stable(gs_memory_t *mem);

gs_memory_retrying_t gslt_mem = {
    (gs_memory_t *)&gslt_mem, /* also this is stable_memory since no save/restore */
    { gslt_alloc_bytes_immovable, /* alloc_bytes_immovable */
      gslt_resize_object, /* resize_object */
      gslt_free_object, /* free_object */
      gslt_stable, /* stable */
      gslt_status, /* status */
      gslt_free_all, /* free_all */
      gslt_consolidate_free, /* consolidate_free */
      gslt_alloc_bytes, /* alloc_bytes */
      gslt_alloc_struct, /* alloc_struct */
      gslt_alloc_struct_immovable, /* alloc_struct_immovable */
      gslt_alloc_byte_array, /* alloc_byte_array */
      gslt_alloc_byte_array_immovable, /* alloc_byte_array_immovable */
      gslt_alloc_struct_array, /* alloc_struct_array */
      gslt_alloc_struct_array_immovable, /* alloc_struct_array_immovable */
      gslt_object_size, /* object_size */
      gslt_object_type, /* object_type */
      gslt_alloc_string, /* alloc_string */
      gslt_alloc_string_immovable, /* alloc_string_immovable */
      gslt_resize_string, /* resize_string */
      gslt_free_string, /* free_string */
      gslt_register_root, /* register_root */
      gslt_unregister_root, /* unregister_root */
      gslt_enable_free /* enable_free */
    },
    NULL, /* gs_lib_ctx */
    NULL, /* head */
    NULL, /* non_gc_memory */
    NULL,            /* target */
    no_recover_proc, /* recovery procedure */
    NULL             /* recovery data */
};

static gs_memory_t *
gslt_stable(gs_memory_t *mem)
{
    return (gs_memory_t *)&gslt_mem;
}

const gs_malloc_memory_t gslt_malloc_memory = {
    0, /* stable */
    { gslt_alloc_bytes_immovable, /* alloc_bytes_immovable */
      gslt_resize_object, /* resize_object */
      gslt_free_object, /* free_object */
      gslt_stable, /* stable */
      gslt_status, /* status */
      gslt_free_all, /* free_all */
      gslt_consolidate_free, /* consolidate_free */
      gslt_alloc_bytes, /* alloc_bytes */
      gslt_alloc_struct, /* alloc_struct */
      gslt_alloc_struct_immovable, /* alloc_struct_immovable */
      gslt_alloc_byte_array, /* alloc_byte_array */
      gslt_alloc_byte_array_immovable, /* alloc_byte_array_immovable */
      gslt_alloc_struct_array, /* alloc_struct_array */
      gslt_alloc_struct_array_immovable, /* alloc_struct_array_immovable */
      gslt_object_size, /* object_size */
      gslt_object_type, /* object_type */
      gslt_alloc_string, /* alloc_string */
      gslt_alloc_string_immovable, /* alloc_string_immovable */
      gslt_resize_string, /* resize_string */
      gslt_free_string, /* free_string */
      gslt_register_root, /* register_root */
      gslt_unregister_root, /* unregister_root */
      gslt_enable_free /* enable_free */
    },
    NULL, /* gs_lib_ctx */
    NULL, /* head */
    NULL, /* non_gc_memory */
    0, /* allocated */
    0, /* limit */
    0, /* used */
    0  /* max used */
};

/* retrun the c-heap manager set the global default as well. */
static gs_memory_t *
gslt_malloc_init(void)
{
    return (gs_memory_t *)&gslt_malloc_memory;
}

gs_memory_t *
gslt_alloc_init()
{
    if ( gslt_malloc_init() ==  NULL )
        return NULL;

    gs_lib_ctx_init((gs_memory_t *)&gslt_mem);

    gslt_mem.head = 0;
    gslt_mem.non_gc_memory = (gs_memory_t *)&gslt_mem;

    return (gs_memory_t *)&gslt_mem;
}

void
gslt_alloc_print_leaks(void)
{
    dprintf1("number of allocs: %d\n", num_alloc_called);
    dprintf1("number of frees: %d\n", num_free_called);
    dprintf1("number of resizes: %d\n", num_resize_called);
    dprintf1("number of leaked chunks: %d\n", num_alloc_called - num_free_called);
}
