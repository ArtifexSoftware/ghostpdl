/* subclass the allocater for pcl and xl.  Redefine methods to use the
   standard C allocator.  For a number of reasons the default
   allocator is not necessary for pcl or pxl.  Neither language
   supports garbage collection nor do they use relocation.  There is
   not necessary distinction between system, global or local vm.
   Hence a simple allocator based on malloc, realloc and free */
/*$Id$*/

#include "malloc_.h"
#include "memory_.h"
#include "gdebug.h"
#include "gsmemory.h" /* for gs_memory_type_ptr_t */
#include "gsstype.h"


/* it is easier to do this then lug in the gsstruct.h include file */
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
get_size(char *bptr)
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
set_size(char *bptr, uint size)
{
    memcpy(bptr, &size, sizeof(size));
}

inline void
set_type(char *bptr, gs_memory_type_ptr_t type)
{
    memcpy(&bptr[round_up_to_align(1)], &type, sizeof(type));
    return;
}
    
/* all of the allocation routines modulo realloc reduce to the this
   function */
private byte *
pl_alloc(gs_memory_t * mem, uint size, gs_memory_type_ptr_t type, client_name_t cname)
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
	/* da for debug allocator - so scripts can parse the trace */
	if_debug2('A', "[da]:malloc:%x:%s\n", &ptr[minsize * 2], cname );
	/* set the type and size */
	set_type(ptr, type);
	set_size(ptr, size);
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
    ptr = (byte *)realloc(&bptr[-header_size], new_size);
    if ( !ptr )
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
	if ( gs_debug_c('@') )
	    memset(&bptr[-header_size], 0xee, header_size + get_size(ptr));
	free(&bptr[-header_size]);
	/* da for debug allocator - so scripts can parse the trace */
	if_debug2('A', "[da]:free:%x:%s\n", ptr, cname );
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
    return pl_free_object(mem, data, cname);
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
    return get_size(obj);
}

private gs_memory_type_ptr_t
pl_object_type(gs_memory_t * mem, const void /*obj_header_t */ *obj)
{
    return get_type(obj);
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

/* forward decl */
private gs_memory_t * pl_stable(P1(gs_memory_t *mem));

const gs_memory_t pl_mem = {
    0, /* stable_memory */
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
    } 
};

private gs_memory_t *
pl_stable(gs_memory_t *mem)
{
    return &pl_mem;
}

gs_memory_t *
pl_get_allocator()
{
    return &pl_mem;
}
