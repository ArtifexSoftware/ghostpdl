/* subclass the allocater for pcl and xl.  Redefine methods to use the
   standard C allocator.  For a number of reasons the default
   allocator is not necessary for pcl or pxl.  Neither language
   supports garbage collection nor do they use relocation.  There is
   no necessary distinction between system, global or local vm.
   Hence a simple allocator based on malloc, realloc and free. */
/*$Id$*/

#include "malloc_.h"
#include "memory_.h"
#include "gdebug.h"
#include "gsmemret.h" /* for gs_memory_type_ptr_t */
#include "gsmalloc.h"
#include "gsstype.h"
#include "plalloc.h"

/* a screwed up mess, we try to make it manageable here */
extern const gs_memory_struct_type_t st_bytes;

/* assume doubles are the largest primitive types and malloc alignment
   is consistent.  Covers the machines we care about */
inline private uint
round_up_to_align(uint size)
{
    return (size + (arch_align_double_mod - 1)) & -arch_align_double_mod;
}

/* aceesors to get size and type given the pointer returned to the
   client, *not* the pointer returned by malloc or realloc */
inline private uint
get_size(byte *bptr)
{
    /* unpack the unsigned long we stored 2 words behind the object at
       alloc time... back up 2 */
    uint size_offset = -round_up_to_align(1) * 2;
    uint size;
    /* unpack */
    memcpy(&size, &bptr[size_offset], sizeof(uint));
    return size;
}

inline gs_memory_type_ptr_t
get_type(char *bptr)
{
    /* unpack the unsigned long we stored 1 word behind the object at
       alloc time... back up 1 */
    gs_memory_type_ptr_t type;
    int type_offset = -round_up_to_align(1);
    /* unpack */
    memcpy(&type, &bptr[type_offset], sizeof(gs_memory_type_ptr_t));
    return type;
}

/* aceesors to get size and typen give the pointer that was returned
   by malloc or realloc, *not* the pointer returned by malloc or
   realloc */
inline void
set_size(byte *bptr, uint size)
{
    memcpy(bptr, &size, sizeof(size));
}

inline void
set_type(byte *bptr, gs_memory_type_ptr_t type)
{
    memcpy(&bptr[round_up_to_align(1)], &type, sizeof(type));
    return;
}
    
#ifndef PL_KEEP_GLOBAL_FREE_LIST
#error "feature binding PL_KEEP_GLOBAL_FREE_LIST is undefined"
#endif

struct pl_mem_node_s {
    byte *address;
    struct pl_mem_node_s *next;
    char *cname;
#ifdef DEBUG
    long op_count;
#endif
};

#ifdef DEBUG
/* each add operation increments the operation count.  So if a
   reference is reported unfreed in the remaining free list it's
   allocation can easily be found with a conditional
   breakpoint */
    static long mem_node_operation_count = 0;
#endif


/* return -1 on error, 0 on success */
int 
pl_mem_node_add(gs_memory_t * mem, byte *add, char *cname)
{
  if( PL_KEEP_GLOBAL_FREE_LIST ) {
    pl_mem_node_t *node = (pl_mem_node_t *)malloc(sizeof(pl_mem_node_t));
    if ( node == NULL )
        return -1;
    if (mem->head == NULL) {
        mem->head = node;
        mem->head->next = NULL;
    } else {
        node->next = mem->head;
        mem->head = node;
    }
    mem->head->address = add;
    mem->head->cname = cname;
#ifdef DEBUG
    mem_node_operation_count++;
    mem->head->op_count = mem_node_operation_count;
#endif
  }
  return 0;
}

int        
pl_mem_node_remove(gs_memory_t *mem, byte *addr)
{
  if ( PL_KEEP_GLOBAL_FREE_LIST ) {
    pl_mem_node_t *head = mem->head;
    pl_mem_node_t *current;
    /* check the head first */
    if ( head == NULL ) {
        dprintf( "FAIL - no nodes to be removed\n" );
        return -1;
    }

    if ( head && head->address == addr ) {
        pl_mem_node_t *tmp = head->next;
	free(head);
        mem->head = tmp;
    } else {
        /* stop in front of element */
        bool found = false;
        for (current = head; current != NULL; current = current->next) {

            if ( current->next && (current->next->address == addr) ) {
                pl_mem_node_t *tmp = current->next->next;
		free(current->next);
                current->next = tmp;
                found = true;
                break;
            }
            
        }
        if ( !found )
            dprintf1( "FAIL freeing wild pointer freed address %x not found\n", addr );
    }
  }
  return 0;
}
    
void
pl_mem_node_free_all_remaining(gs_memory_t *mem)
{
    if( PL_KEEP_GLOBAL_FREE_LIST ) {
	pl_mem_node_t *head = mem->head;
#       ifdef DEBUG
            static const bool print_recovered_block_info = true;
#       else
            static const bool print_recovered_block_info = false;
#       endif
        uint blk_count = 0;
	uint size = 0;
	uint total_size = 0;
	byte* ptr; 

	pl_mem_node_t *current;
	pl_mem_node_t *next;
	current = head;
	while ( current != NULL ) { 
	    next = current->next; 
            if ( print_recovered_block_info ) {
	       ++blk_count;
	       ptr = ((byte*)current->address) + (2 * round_up_to_align(1));
	       size = get_size(ptr);
	       total_size += size;
	       if (mem != gs_memory_t_default)
#ifdef DEBUG
		   dprintf4("Recovered pjl %x size %d %ld'th allocation client %s\n",
			    ptr, size, current->op_count, current->cname)
#endif
                       ;
	       else
#ifdef DEBUG
		   dprintf4("Recovered %x size %d %ld'th allocation client %s\n",
			    ptr, size, current->op_count, current->cname)
#endif
                       ;
	    }
	    free(current->address);
	    free(current);
	    current = next;
	}
	if ( print_recovered_block_info && blk_count )
	    if (mem != gs_memory_t_default)
		dprintf2("Recovered pjl %d blocks, %d bytes\n", 
			 blk_count, total_size);
	    else
		dprintf2("Recovered %d blocks, %d bytes\n", 
			 blk_count, total_size);
	mem->head = NULL;
    }
}


/* all of the allocation routines modulo realloc reduce to the this
   function */
private byte *
pl_alloc(gs_memory_t *mem, uint size, gs_memory_type_ptr_t type, client_name_t cname)
{

    uint minsize, newsize;
    /* NB apparently there is code floating around that does 0 size
       mallocs.  sigh. */
    if ( size == 0 )
	return 0;

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
#ifdef DEBUG
	if (mem != gs_memory_t_default)
	    /* da for debug allocator - so scripts can parse the trace */
	    if_debug2('A', "[da]:malloc pjl:%x:%s\n", &ptr[minsize * 2], cname );
	else
	    if_debug2('A', "[da]:malloc:%x:%s\n", &ptr[minsize * 2], cname );
#endif
	/* set the type and size */
	set_type(ptr, type);
	set_size(ptr, size);
	/* initialize for debugging */
#ifdef DEBUG
	if ( gs_debug_c('@') )
	    memset(&ptr[minsize * 2], 0xff, get_size(&ptr[minsize * 2]));
#endif
	if ( pl_mem_node_add(mem, ptr, cname) ) { 
	   free( ptr );
	   return NULL;
	}
	/* return the memory after the size and type words. */
	return &ptr[minsize * 2];
    }
}

private byte *
pl_alloc_bytes_immovable(gs_memory_t * mem, uint size, client_name_t cname)
{
    return pl_alloc(mem, size, &st_bytes, cname);
}

private byte *
pl_alloc_bytes(gs_memory_t * mem, uint size, client_name_t cname)
{
    return pl_alloc(mem, size, &st_bytes, cname);
}

private void *
pl_alloc_struct_immovable(gs_memory_t * mem, gs_memory_type_ptr_t pstype,
			 client_name_t cname)
{
    return pl_alloc(mem, pstype->ssize, pstype, cname);
}

private void *
pl_alloc_struct(gs_memory_t * mem, gs_memory_type_ptr_t pstype,
	       client_name_t cname)
{
    return pl_alloc(mem, pstype->ssize, pstype, cname);
}

private byte *
pl_alloc_byte_array_immovable(gs_memory_t * mem, uint num_elements,
			     uint elt_size, client_name_t cname)
{
    return pl_alloc_bytes(mem, num_elements * elt_size, cname);
}

private byte *
pl_alloc_byte_array(gs_memory_t * mem, uint num_elements, uint elt_size,
		   client_name_t cname)
{
    return pl_alloc_bytes(mem, num_elements * elt_size, cname);
}

private void *
pl_alloc_struct_array_immovable(gs_memory_t * mem, uint num_elements,
			   gs_memory_type_ptr_t pstype, client_name_t cname)
{
    return pl_alloc(mem, num_elements * pstype->ssize, pstype, cname);
}

private void *
pl_alloc_struct_array(gs_memory_t * mem, uint num_elements,
		     gs_memory_type_ptr_t pstype, client_name_t cname)
{
    return pl_alloc(mem, num_elements * pstype->ssize, pstype, cname);
}


private void *
pl_resize_object(gs_memory_t * mem, void *obj, uint new_num_elements, client_name_t cname)
{
    byte *ptr;
    byte *bptr = (byte *)obj;
    /* get the type from the old object */
    gs_memory_type_ptr_t objs_type = get_type(obj);
    /* type and size header size */
    ulong header_size = round_up_to_align(1) + round_up_to_align(1);
    /* get new object's size */
    ulong new_size = (objs_type->ssize * new_num_elements) + header_size;
    /* replace the size field */
    pl_mem_node_remove(mem, &bptr[-header_size]);
    ptr = (byte *)realloc(&bptr[-header_size], new_size);
    if ( !ptr || pl_mem_node_add(mem, ptr, cname) )
	return NULL;
    /* da for debug allocator - so scripts can parse the trace */
    if_debug2('A', "[da]:realloc:%x:%s\n", ptr, cname );
    /* we reset size and type - the type in case realloc moved us */
    set_size(ptr, new_size - header_size);
    set_type(ptr, objs_type);
    return &ptr[round_up_to_align(1) * 2];
}

	
private void
pl_free_object(gs_memory_t * mem, void *ptr, client_name_t cname)
{
    if ( ptr != NULL ) {
	byte *bptr = (byte *)ptr;
	uint header_size = round_up_to_align(1) + round_up_to_align(1);
#ifdef DEBUG
	if ( gs_debug_c('@') )
	    memset(&bptr[-header_size], 0xee, header_size + get_size(ptr));
#endif
	pl_mem_node_remove(mem, &bptr[-header_size]);
	free(&bptr[-header_size]);
	
#ifdef DEBUG
	if (mem != gs_memory_t_default)
	    if_debug2('A', "[da]:free pjl:%x:%s\n", ptr, cname );
	else
	    /* da for debug allocator - so scripts can parse the trace */
	    if_debug2('A', "[da]:free:%x:%s\n", ptr, cname );
#endif
    }
}

private byte *
pl_alloc_string_immovable(gs_memory_t * mem, uint nbytes, client_name_t cname)
{
    /* we just alloc bytes here */
    return pl_alloc_bytes(mem, nbytes, cname);
}

private byte *
pl_alloc_string(gs_memory_t * mem, uint nbytes, client_name_t cname)
{
    /* we just alloc bytes here */
    return pl_alloc_bytes(mem, nbytes, cname);
}

private byte *
pl_resize_string(gs_memory_t * mem, byte * data, uint old_num, uint new_num,
		client_name_t cname)
{
    /* just resize object - ignores old_num */
    return pl_resize_object(mem, data, new_num, cname);
}

private void
pl_free_string(gs_memory_t * mem, byte * data, uint nbytes,
	      client_name_t cname)
{
    pl_free_object(mem, data, cname);
    return;
}


private void
pl_status(gs_memory_t * mem, gs_memory_status_t * pstat)
{
    return;
}

private void
pl_enable_free(gs_memory_t * mem, bool enable)
{
    return;
}

private void
pl_free_all(gs_memory_t * mem, uint free_mask, client_name_t cname)
{
    return;
}

private void
pl_consolidate_free(gs_memory_t *mem)
{
    return;
}


private uint
pl_object_size(gs_memory_t * mem, const void /*obj_header_t */ *obj)
{
    return get_size((byte *)obj);
}

private gs_memory_type_ptr_t
pl_object_type(gs_memory_t * mem, const void /*obj_header_t */ *obj)
{
    return get_type((byte *)obj);
}

private int
pl_register_root(gs_memory_t * mem, gs_gc_root_t * rp, gs_ptr_type_t ptype,
		 void **up, client_name_t cname)
{
    return 0;
}

private void
pl_unregister_root(gs_memory_t * mem, gs_gc_root_t * rp, client_name_t cname)
{
    return;
}

/* Define a vacuous recovery procedure. */
private gs_memory_recover_status_t
no_recover_proc(gs_memory_retrying_t *rmem, void *proc_data)
{
    return RECOVER_STATUS_NO_RETRY;
}


/* forward decl */
private gs_memory_t * pl_stable(P1(gs_memory_t *mem));


gs_memory_retrying_t pl_mem = {
    &pl_mem, /* also this is stable_memory since no save/restore */
    { pl_alloc_bytes_immovable, /* alloc_bytes_immovable */
      pl_resize_object, /* resize_object */
      pl_free_object, /* free_object */
      pl_stable, /* stable */
      pl_status, /* status */
      pl_free_all, /* free_all */
      pl_consolidate_free, /* consolidate_free */
      pl_alloc_bytes, /* alloc_bytes */ 
      pl_alloc_struct, /* alloc_struct */
      pl_alloc_struct_immovable, /* alloc_struct_immovable */
      pl_alloc_byte_array, /* alloc_byte_array */
      pl_alloc_byte_array_immovable, /* alloc_byte_array_immovable */
      pl_alloc_struct_array, /* alloc_struct_array */
      pl_alloc_struct_array_immovable, /* alloc_struct_array_immovable */
      pl_object_size, /* object_size */
      pl_object_type, /* object_type */
      pl_alloc_string, /* alloc_string */
      pl_alloc_string_immovable, /* alloc_string_immovable */
      pl_resize_string, /* resize_string */
      pl_free_string, /* free_string */
      pl_register_root, /* register_root */ 
      pl_unregister_root, /* unregister_root */
      pl_enable_free /* enable_free */ 
    },
    NULL,            /* target */
    no_recover_proc, /* recovery procedure */
    NULL,            /* recovery data */
    0                /* head */
};

gs_memory_retrying_t pl_pjl_mem = {
    &pl_pjl_mem, /* also this is stable_memory since no save/restore */
    { pl_alloc_bytes_immovable, /* alloc_bytes_immovable */
      pl_resize_object, /* resize_object */
      pl_free_object, /* free_object */
      pl_stable, /* stable */
      pl_status, /* status */
      pl_free_all, /* free_all */
      pl_consolidate_free, /* consolidate_free */
      pl_alloc_bytes, /* alloc_bytes */ 
      pl_alloc_struct, /* alloc_struct */
      pl_alloc_struct_immovable, /* alloc_struct_immovable */
      pl_alloc_byte_array, /* alloc_byte_array */
      pl_alloc_byte_array_immovable, /* alloc_byte_array_immovable */
      pl_alloc_struct_array, /* alloc_struct_array */
      pl_alloc_struct_array_immovable, /* alloc_struct_array_immovable */
      pl_object_size, /* object_size */
      pl_object_type, /* object_type */
      pl_alloc_string, /* alloc_string */
      pl_alloc_string_immovable, /* alloc_string_immovable */
      pl_resize_string, /* resize_string */
      pl_free_string, /* free_string */
      pl_register_root, /* register_root */ 
      pl_unregister_root, /* unregister_root */
      pl_enable_free /* enable_free */ 
    },
    NULL,            /* target */
    no_recover_proc, /* recovery procedure */
    NULL,            /* recovery data */
    0                /* head */
};

private gs_memory_t *
pl_stable(gs_memory_t *mem)
{
    return &pl_mem;
}

const gs_malloc_memory_t pl_malloc_memory = {
    0, /* stable */
    { pl_alloc_bytes_immovable, /* alloc_bytes_immovable */
      pl_resize_object, /* resize_object */
      pl_free_object, /* free_object */
      pl_stable, /* stable */
      pl_status, /* status */
      pl_free_all, /* free_all */
      pl_consolidate_free, /* consolidate_free */
      pl_alloc_bytes, /* alloc_bytes */ 
      pl_alloc_struct, /* alloc_struct */
      pl_alloc_struct_immovable, /* alloc_struct_immovable */
      pl_alloc_byte_array, /* alloc_byte_array */
      pl_alloc_byte_array_immovable, /* alloc_byte_array_immovable */
      pl_alloc_struct_array, /* alloc_struct_array */
      pl_alloc_struct_array_immovable, /* alloc_struct_array_immovable */
      pl_object_size, /* object_size */
      pl_object_type, /* object_type */
      pl_alloc_string, /* alloc_string */
      pl_alloc_string_immovable, /* alloc_string_immovable */
      pl_resize_string, /* resize_string */
      pl_free_string, /* free_string */
      pl_register_root, /* register_root */ 
      pl_unregister_root, /* unregister_root */
      pl_enable_free /* enable_free */ 
    },
    0, /* allocated */
    0, /* limit */ 
    0, /* used */ 
    0, /* max used */
    0  /* head */
};

/* retrun the c-heap manager set the global default as well. */
private gs_memory_t *
pl_malloc_init(void)
{
    return &pl_malloc_memory;
}

gs_memory_t *
pl_pjl_alloc_init(void)
{
    return &pl_pjl_mem;
}

gs_memory_t *
pl_alloc_init(void)
{

#ifndef PSI_INCLUDED
    if ( pl_malloc_init() ==  NULL )
	return NULL;
    return &pl_mem;
#else

    /* foo hack gag 
     * looks like local variables,
     * should be universe scoped
     * actually is setting globals in ps!
     */
    gs_memory_t *local_memory_t_default = 0;

# ifndef NO_GS_MEMORY_GLOBALS_BIND
    /* foo have globals so if it is already initialized
     * point to the same memory manager.
     */
    if (gs_memory_t_default != 0 )
	local_memory_t_default = gs_memory_t_default;
# endif

    if (gs_malloc_init(&local_memory_t_default))
	return 0;
    local_memory_t_default->head = 0;
    return local_memory_t_default;
#endif
}

#ifndef PSI_INCLUDED
/* Define the default allocator. */
    
# ifndef NO_WRAPPED_MEMORY_BIND
gs_malloc_memory_t *gs_malloc_memory_default = &pl_malloc_memory;
# endif

gs_memory_t *gs_memory_t_default = &pl_mem;


#endif
