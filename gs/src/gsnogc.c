/* Copyright (C) 1996 Aladdin Enterprises.  All rights reserved.
  
  This file is part of Aladdin Ghostscript.
  
  Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
  or distributor accepts any responsibility for the consequences of using it,
  or for whether it serves any particular purpose or works at all, unless he
  or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
  License (the "License") for full details.
  
  Every copy of Aladdin Ghostscript must include a copy of the License,
  normally in a plain ASCII text file named PUBLIC.  The License grants you
  the right to copy, modify and redistribute Aladdin Ghostscript, but only
  under certain conditions described in the License.  Among other things, the
  License requires that the copyright notice and this notice be preserved on
  all copies.
*/

/* gsnogc.c */
/* String freelist implementation and ersatz garbage collector */
#include "gx.h"
#include "gsgc.h"
#include "gsmdebug.h"
#include "gsstruct.h"
#include "gxalloc.h"

/*
 * We implement string freelists here, because they are only useful
 * in non-garbage-collected environments.
 */

#define get2(ptr) (((ptr)[0] << 8) + (ptr)[1])
#define put2(ptr, val) ((ptr)[0] = (val) >> 8, (ptr)[1] = (byte)(val))

#define imem ((gs_ref_memory_t *)mem)

/* Allocate a string. */
/* Scan the current chunk's free list if the request is large enough. */
/* Currently we require an exact match of the block size. */
private byte *
sf_alloc_string(gs_memory_t *mem, uint nbytes, client_name_t cname)
{	if ( nbytes >= 40 && nbytes < imem->large_size )
	  { byte *base = csbase(&imem->cc);
	    byte *prev = 0;
	    uint offset = imem->cc.sfree;
	    uint next;
	    byte *ptr;

	    for ( ; offset != 0; prev = ptr, offset = next )
	      { ptr = base + offset;
	        next = get2(ptr + 2);
	        if ( get2(ptr) != nbytes )
		  continue;
		/* Take this block. */
		if ( prev == 0 )
		  imem->cc.sfree = next;
		else
		  put2(prev + 2, next);
		if_debug4('A', "[a%d:+>F]%s(%u) = 0x%lx\n", imem->space,
			  client_name_string(cname), nbytes, (ulong)ptr);
		gs_alloc_fill(ptr, gs_alloc_fill_alloc, nbytes);
		imem->lost.strings -= nbytes;
		return ptr;
	      }
	  }
	return (*gs_ref_memory_procs.alloc_string)(mem, nbytes, cname);
}

/* Free a string. */
private void
sf_free_string(gs_memory_t *mem, byte *str, uint size, client_name_t cname)
{	chunk_t *cp;
	uint str_offset;

	if ( str == imem->cc.ctop )
	  {	if_debug4('A', "[a%d:-> ]%s(%u) 0x%lx\n", imem->space,
			  client_name_string(cname), size, (ulong)str);
		imem->cc.ctop += size;
		gs_alloc_fill(str, gs_alloc_fill_free, size);
		return;
	  }
	if_debug4('A', "[a%d:->#]%s(%u) 0x%lx\n", imem->space,
		  client_name_string(cname), size, (ulong)str);
	imem->lost.strings += size;
	if ( ptr_is_in_chunk(str, &imem->cc) )
	  cp = &imem->cc;
	else
	  { chunk_locator_t loc;
	    loc.memory = imem;
	    loc.cp = imem->clast;
	    if ( !chunk_locate_ptr(str, &loc) )
	      return;	/* something is probably wrong.... */
	    cp = loc.cp;
	  }
	if ( str == cp->ctop )
	  { cp->ctop += size;
	    return;
	  }
	str_offset = str - csbase(cp);
	if ( size >= 4 )
	  { byte *prev;
	    uint next;

	    put2(str, size);
	    if ( cp->sfree == 0 || str_offset < cp->sfree )
	      { /* Put the string at the head of the free list. */
		put2(str + 2, cp->sfree);
		cp->sfree = str_offset;
		return;
	      }
	    /* Insert the new string in address order. */
	    prev = csbase(cp) + cp->sfree;
#ifdef DEBUG
	    if ( gs_debug_c('?') )
	      { if ( prev < str + size && prev + get2(prev) > str )
		  { lprintf4("freeing string 0x%lx(%u), overlaps 0x%lx(%u)!\n",
			     (ulong)str, size, (ulong)prev, get2(prev));
		    return;
		  }
	      }
#endif
	    for ( ; ; )
	      { next = get2(prev + 2);
#ifdef DEBUG
		if ( gs_debug_c('?') && next != 0 )
		  { byte *pnext = csbase(cp) + next;
		    if ( pnext < str + size && pnext + get2(pnext) > str )
		      { lprintf4("freeing string 0x%lx(%u), overlaps 0x%lx(%u)!\n",
				 (ulong)str, size, (ulong)pnext, get2(pnext));
		        return;
		      }
		  }
#endif
	        if ( next >= str_offset || next == 0 )
		  break;
		prev = csbase(cp) + next;
	      }
	    put2(str + 2, next);
	    put2(prev + 2, str_offset);
	    gs_alloc_fill(str + 4, gs_alloc_fill_free, size - 4);
	  }
	else
	  { /* Insert the string in the 1-byte free list(s).  Note that */
	    /* if it straddles a 256-byte block, we need to do this twice. */
	    ushort *pfree1 = &cp->sfree1[str_offset >> 8];
	    uint count = size;
	    byte *prev;
	    byte *ptr = str;

	    if ( *pfree1 == 0 )
	      { *str = 0;
		*pfree1 = str_offset;
		prev = str;
	      }
	    else if ( str_offset < *pfree1 )
	      { *str = *pfree1 - str_offset;
		*pfree1 = str_offset;
		prev = str;
	      }
	    else
	      { uint next;
		prev = csbase(cp) + *pfree1;
		while ( prev + (next = *prev) < str )
		  prev += next;
	      }
	    for ( ; ; )
	      { /*
		 * Invariants:
		 *	prev < sfbase + str_offset
		 *	*prev == 0 || prev + *prev > sfbase + str_offset
		 */
		*ptr = (*prev == 0 ? 0 : prev + *prev - ptr);
		*prev = ptr - prev;
		if ( !--count )
		  break;
		prev = ptr++;
		if ( !(++str_offset & 255) )
		  { /* Move to the next block of 256 bytes. */
		    ++pfree1;
		    *ptr = (byte)*pfree1;
		    *pfree1 = str_offset;
		    if ( !--count )
		      break;
		    prev = ptr++;
		    ++str_offset;
		  }
	      }
	  }
}

/* Enable or disable freeing. */
private void
sf_enable_free(gs_memory_t *mem, bool enable)
{	(*gs_ref_memory_procs.enable_free)(mem, enable);
	if ( enable )
	  mem->procs.free_string = sf_free_string;
}

#undef imem

/* Merge free strings at the bottom of a chunk's string storage. */
private void
sf_merge_strings(chunk_t *cp)
{	for ( ; ; )
	  { byte *ctop = cp->ctop;
	    uint top_offset = ctop - csbase(cp);
	    ushort *pfree1;

	    if ( cp->sfree == top_offset )
	      { /* Merge a large free block. */
		cp->sfree = get2(ctop + 2);
		cp->ctop += get2(ctop);
		continue;
	      }
	    if ( !cp->sfree1 )
	      break;
	    pfree1 = &cp->sfree1[top_offset >> 8];
	    if ( *pfree1 != top_offset )
	      break;
	    /* Merge a small (1-byte) free block. */
	    *pfree1 = (*ctop ? *pfree1 + *ctop : 0);
	    cp->ctop++;
	  }
}

/*
 * This procedure has the same API as the garbage collector used by the
 * PostScript interpreter, but it is designed to be used in environments
 * that don't need garbage collection and don't use save/restore.  All it
 * does is coalesce free blocks at the high end of the object area of each
 * chunk, and free strings at the low end of the string area, and then free
 * completely empty chunks.
 */

void
gs_reclaim(vm_spaces *pspaces, bool global)
{	int space;
	gs_ref_memory_t *mem_prev = 0;

	for ( space = 0; space < countof(pspaces->indexed); ++space )
	  { gs_ref_memory_t *mem = pspaces->indexed[space];
	    chunk_t *cp;
	    chunk_t *cprev;
	    /*
	     * We're going to recompute lost.objects, by subtracting the
	     * amount of space reclaimed minus the amount of that space that
	     * was on free lists.
	     */
	    ulong found = 0;

	    if ( mem == 0 || mem == mem_prev )
	      continue;
	    mem_prev = mem;
	    alloc_close_chunk(mem);

	    /*
	     * Change the allocator to use string freelists in the future.
	     */
	    mem->procs.alloc_string = sf_alloc_string;
	    if ( mem->procs.free_string != gs_ignore_free_string )
	      mem->procs.free_string = sf_free_string;
	    mem->procs.enable_free = sf_enable_free;

	    /* Visit chunks in reverse order to encourage LIFO behavior. */
	    for ( cp = mem->clast; cp != 0; cp = cprev )
	      { obj_header_t *begin_free = (obj_header_t *)cp->cbase;

		cprev = cp->cprev;
	        SCAN_CHUNK_OBJECTS(cp)
		DO_ALL
		  if ( pre->o_type == &st_free )
		    { if ( begin_free == 0 )
		        begin_free = pre;
		    }
		  else
		    begin_free = 0;
		END_OBJECTS_SCAN
		if ( !begin_free )
		  continue;
		/* We found free objects at the top of the object area. */
		found += (byte *)cp->cbot - (byte *)begin_free;
		/* Remove the free objects from the freelists. */
		    { int i;
		      for ( i = 0; i < num_freelists; i++ )
			{ obj_header_t *pfree;
			  obj_header_t **ppfprev = &mem->freelists[i];
			  uint free_size =
			    (i << log2_obj_align_mod) + sizeof(obj_header_t);

			  while ( (pfree = *ppfprev) != 0 )
			    if ( ptr_ge(pfree, begin_free) &&
				 ptr_lt(pfree, cp->cbot)
			       )
			      { /* We're removing an object. */
				*ppfprev = *(obj_header_t **)pfree;
				found -= free_size;
			      }
			    else
			      ppfprev = (obj_header_t **)pfree;
			}
		    }
		    { byte *top = cp->ctop;
		      sf_merge_strings(cp);
		      mem->lost.strings -= cp->ctop - top;
		    }
		if ( begin_free == (obj_header_t *)cp->cbase &&
		     cp->ctop == cp->climit
		   )
		  { /* The entire chunk is free. */
		    chunk_t *cnext = cp->cnext;

		    alloc_free_chunk(cp, mem);
		    if ( mem->pcc == cp )
		      mem->pcc =
			(cnext == 0 ? cprev : cprev == 0 ? cnext :
			 cprev->cbot - cprev->ctop > cnext->cbot - cnext->ctop ?
			 cprev : cnext);
		  }
		else
		  { if_debug4('a', "[a]resetting chunk 0x%lx cbot from 0x%lx to 0x%lx (%lu free)\n",
			      (ulong)cp, (ulong)cp->cbot, (ulong)begin_free,
			      (ulong)((byte *)cp->cbot - (byte *)begin_free));
		    cp->cbot = (byte *)begin_free;
		  }
	      }
	    mem->lost.objects -= found;
	    alloc_open_chunk(mem);
	  }
}
