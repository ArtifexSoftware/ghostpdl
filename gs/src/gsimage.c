/* Copyright (C) 1996, 1997, 1998, 1999 Aladdin Enterprises.  All rights reserved.
 * This software is licensed to a single customer by Artifex Software Inc.
 * under the terms of a specific OEM agreement.
 */

/*$RCSfile$ $Revision$ */
/* Image setup procedures for Ghostscript library */
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsstruct.h"
#include "gscspace.h"
#include "gsmatrix.h"		/* for gsiparam.h */
#include "gsimage.h"
#include "gxarith.h"		/* for igcd */
#include "gxdevice.h"
#include "gxiparam.h"
#include "gxpath.h"		/* for gx_effective_clip_path */
#include "gzstate.h"

/* Define the enumeration state for this interface layer. */
/*typedef struct gs_image_enum_s gs_image_enum; *//* in gsimage.h */
typedef struct image_enum_plane_s {
    /* Change dynamically */
    uint pos;			/* byte position within the scan line */
    gs_const_string source;	/* source data, [0 .. num_planes - 1] */
    gs_string row;		/* row buffers, [0 .. num_planes - 1] */
} image_enum_plane_t;
struct gs_image_enum_s {
    /* The following are set at initialization time. */
    gs_memory_t *memory;
    gx_device *dev;		/* if 0, just skip over the data */
    gx_image_enum_common_t *info;	/* driver bookkeeping structure */
    int num_planes;
    int height;
    bool wanted_varies;
    /* The following are updated dynamically. */
    int plane_index;		/* index of next plane of data, */
				/* only needed for gs_image_next */
    int y;
    bool error;
    byte wanted[gs_image_max_planes];
    image_enum_plane_t planes[gs_image_max_planes];
};

gs_private_st_composite(st_gs_image_enum, gs_image_enum, "gs_image_enum",
			gs_image_enum_enum_ptrs, gs_image_enum_reloc_ptrs);
#define gs_image_enum_num_ptrs 2

/* GC procedures */
private 
ENUM_PTRS_WITH(gs_image_enum_enum_ptrs, gs_image_enum *eptr)
{
    /* Enumerate the data planes. */
    index -= gs_image_enum_num_ptrs;
    if (index < eptr->plane_index)
	ENUM_RETURN_STRING_PTR(gs_image_enum, planes[index].source);
    index -= eptr->plane_index;
    if (index < eptr->num_planes)
	ENUM_RETURN_STRING_PTR(gs_image_enum, planes[index].row);
    return 0;
}
ENUM_PTR(0, gs_image_enum, dev);
ENUM_PTR(1, gs_image_enum, info);
ENUM_PTRS_END
private RELOC_PTRS_WITH(gs_image_enum_reloc_ptrs, gs_image_enum *eptr)
{
    int i;

    RELOC_PTR(gs_image_enum, dev);
    RELOC_PTR(gs_image_enum, info);
    for (i = 0; i < eptr->plane_index; i++)
	RELOC_CONST_STRING_PTR(gs_image_enum, planes[i].source);
    for (i = 0; i < eptr->num_planes; i++)
	RELOC_STRING_PTR(gs_image_enum, planes[i].row);
}
RELOC_PTRS_END

/* Create an image enumerator given image parameters and a graphics state. */
int
gs_image_begin_typed(const gs_image_common_t * pic, gs_state * pgs,
		     bool uses_color, gx_image_enum_common_t ** ppie)
{
    gx_device *dev = gs_currentdevice(pgs);
    gx_clip_path *pcpath;
    int code = gx_effective_clip_path(pgs, &pcpath);

    if (code < 0)
	return code;
    if (uses_color)
	gx_set_dev_color(pgs);
    return gx_device_begin_typed_image(dev, (const gs_imager_state *)pgs,
		NULL, pic, NULL, pgs->dev_color, pcpath, pgs->memory, ppie);
}

/* Allocate an image enumerator. */
private void
image_enum_init(gs_image_enum * penum)
{
    /* Clean pointers for GC. */
    penum->info = 0;
    penum->dev = 0;
    penum->plane_index = 0;
    penum->num_planes = 0;
}
gs_image_enum *
gs_image_enum_alloc(gs_memory_t * mem, client_name_t cname)
{
    gs_image_enum *penum =
	gs_alloc_struct(mem, gs_image_enum, &st_gs_image_enum, cname);

    if (penum != 0) {
	penum->memory = mem;
	image_enum_init(penum);
    }
    return penum;
}

/* Start processing an ImageType 1 image. */
int
gs_image_init(gs_image_enum * penum, const gs_image_t * pim, bool multi,
	      gs_state * pgs)
{
    gs_image_t image;
    gx_image_enum_common_t *pie;
    int code;

    image = *pim;
    if (image.ImageMask) {
	image.ColorSpace = NULL;
	if (pgs->in_cachedevice <= 1)
	    image.adjust = false;
    } else {
	if (pgs->in_cachedevice)
	    return_error(gs_error_undefined);
	if (image.ColorSpace == NULL)
	    image.ColorSpace =
		gs_cspace_DeviceGray((const gs_imager_state *)pgs);
    }
    code = gs_image_begin_typed((const gs_image_common_t *)&image, pgs,
				image.ImageMask | image.CombineWithColor,
				&pie);
    if (code < 0)
	return code;
    return gs_image_enum_init(penum, pie, (const gs_data_image_t *)&image,
			      pgs);
}
/* Start processing a general image. */
private void
begin_planes(gs_image_enum *penum)
{
    /*
     * Initialize plane_index and (if appropriate) wanted and
     * wanted_varies at the beginning of a group of planes.
     */
    int px = 0;

    if (penum->wanted_varies) {
	penum->wanted_varies =
	    !gx_image_planes_wanted(penum->info, penum->wanted);
    }
    while (!penum->wanted[px])
	++px;
    penum->plane_index = px;
}
int
gs_image_enum_init(gs_image_enum * penum, gx_image_enum_common_t * pie,
		   const gs_data_image_t * pim, gs_state *pgs)
{
    return gs_image_common_init(penum, pie, pim, pgs->memory,
				(pgs->in_charpath ? NULL :
				 gs_currentdevice_inline(pgs)));
}
int
gs_image_common_init(gs_image_enum * penum, gx_image_enum_common_t * pie,
	    const gs_data_image_t * pim, gs_memory_t * mem, gx_device * dev)
{
    int i;

    if (pim->Width == 0 || pim->Height == 0) {
	gx_image_end(pie, false);
	return 1;
    }
    image_enum_init(penum);
    penum->memory = mem;
    penum->dev = dev;
    penum->info = pie;
    penum->num_planes = pie->num_planes;
    penum->height = pim->Height;
    for (i = 0; i < pie->num_planes; ++i) {
	penum->planes[i].pos = 0;
	penum->planes[i].source.size = 0;	/* for gs_image_next_planes */
	penum->planes[i].row.data = 0; /* for GC */
	penum->planes[i].row.size = 0; /* ditto */
    }
    /* Initialize the dynamic part of the state. */
    penum->y = 0;
    penum->error = false;
    penum->wanted_varies = true;
    begin_planes(penum);
    return 0;
}

/*
 * Return the number of bytes of data per row for a given plane.
 */
inline uint
gs_image_bytes_per_plane_row(const gs_image_enum * penum, int plane)
{
    const gx_image_enum_common_t *pie = penum->info;

    return (pie->plane_widths[plane] * pie->plane_depths[plane] + 7) >> 3;
}

/* Return the set of planes wanted. */
const byte *
gs_image_planes_wanted(const gs_image_enum *penum)
{
    return penum->wanted;
}

/* Free the row buffers when cleaning up. */
private void
free_row_buffers(gs_image_enum *penum, int num_planes, client_name_t cname)
{
    int i;

    for (i = num_planes - 1; i >= 0; --i) {
	gs_free_string(penum->memory, penum->planes[i].row.data,
		       penum->planes[i].row.size, cname);
	penum->planes[i].row.data = 0;
	penum->planes[i].row.size = 0;
    }
}

/* Process the next piece of an image. */
private int
copy_planes(gx_device * dev, gs_image_enum * penum,
	    const gx_image_plane_t *planes, int h, int *rows_used)
{
    int code;

    if (penum->dev == 0) {
	if (penum->y + h < penum->height)
	    *rows_used = h, code = 0;
	else
	    *rows_used = penum->height - penum->y, code = 1;
    } else {
	code = gx_image_plane_data_rows(penum->info, planes, h, rows_used);
	penum->error = code < 0;
    }
    return code;
}
int
gs_image_next(gs_image_enum * penum, const byte * dbytes, uint dsize,
	      uint * pused)
{
    int px = penum->plane_index;
    int num_planes = penum->num_planes;
    int i, code;
    uint used[gs_image_max_planes];
    gs_const_string plane_data[gs_image_max_planes];

    /*
     * Handle the following differences between gs_image_next and
     * the device image_data procedure:
     *
     *      - image_data requires an array of planes; gs_image_next
     *      expects planes in successive calls.
     *
     * This function is now deprecated -- gs_image_next_planes does
     * the real work of buffering.
     */
    for (i = 0; i < num_planes; i++)
	plane_data[i].size = 0;
    plane_data[px].data = dbytes;
    plane_data[px].size = dsize;
    penum->error = false;
    code = gs_image_next_planes(penum, plane_data, used);
    if (code >= 0) {
	/* Advance px to the next plane that needs data. */
	do {
	    if (++px == num_planes)
		px = 0;
	} while (!penum->wanted[px]);
	penum->plane_index = px;
    }
    *pused = used[px];
    return code;
}

int
gs_image_next_planes(gs_image_enum * penum, gs_const_string *plane_data,
		     uint * used)
{
    gx_image_plane_t planes[gs_image_max_planes];
    gx_device *dev = penum->dev;
    int num_planes = penum->num_planes;
    int i;
    int code = 0;
    bool need_planes = false;

    /*
     * Handle the following differences between gs_image_next_planes and
     * the device image_data procedure:
     *
     *      - image_data requires an array of planes; not all of the
     *      planes may be available on any given call.
     *
     *      - image_data requires that each call pass entire rows;
     *      gs_image_next_planes allows arbitrary amounts of data.
     */
    for (i = 0; i < num_planes; ++i) {
        used[i] = 0;
	if (!penum->wanted[i])
	    planes[i].data = 0;
	else {
	    if (plane_data[i].size != 0) {
		penum->planes[i].source.size = plane_data[i].size;
		penum->planes[i].source.data = plane_data[i].data;
		planes[i].data_x = 0;
		planes[i].raster = gs_image_bytes_per_plane_row(penum, i);
	    }
	    if (penum->planes[i].source.size == 0)
		need_planes = true;	/* can't proceed until we have all planes */
	}
    }
    if (need_planes) 
	return 0;	/* exit for more data, nothing used */
    while (!code) {
	int direct = max_int;
	bool filled = true;
	bool empty = false;

	/* see if we can do some rows without buffering (i.e., direct) */
	for (i = 0; i < num_planes; ++i) {
	    /* need to reset planes[].raster if wanted_varies */
	    if (penum->wanted_varies)
		planes[i].raster = gs_image_bytes_per_plane_row(penum, i);
	    if (penum->planes[i].pos == 0)  /* nothing buffered for this row */
		direct = min(direct,
			     penum->planes[i].source.size / planes[i].raster);
	    else
		direct = 0;
	}
	if (direct) {
	    /*
	     * Pass (a) row(s) directly from the source.  If wanted_varies,
	     * we can only safely pass 1 scan line at a time.
	     */
	    if (penum->wanted_varies)
		direct = 1;
	    for (i = 0; i < num_planes; ++i)
		if (penum->wanted[i])
		    planes[i].data = penum->planes[i].source.data;
	    code = copy_planes(dev, penum, planes, direct, &direct);
	    if (code < 0)
		return code;	/* don't update */
	    for (i = 0; i < num_planes; ++i)
		if (planes[i].data) {
		    uint direct_used = direct * planes[i].raster;

		    used[i] += direct_used;
		    penum->planes[i].source.data += direct_used;
		    penum->planes[i].source.size -= direct_used;
		    if (penum->planes[i].source.size == 0)
		        empty = true;
		}
	    penum->y += direct;
	    if (code < 0)
		break;
	} else {		/* Buffer a partial row. */
	    uint count[gs_image_max_planes];

	    for (i = 0; i < num_planes; ++i) {
		if (penum->planes[i].source.size != 0) {
		    uint raster = planes[i].raster;
		    uint old_size = penum->planes[i].row.size;

		    /* Make sure the row buffers are allocated. */
		    if (raster > old_size) {
			byte *old_data = penum->planes[i].row.data;
			byte *row =
			    (old_data == 0 ?
			     gs_alloc_string(penum->memory, raster,
					     "gs_image_next(row)") :
			     gs_resize_string(penum->memory, old_data,
					      old_size, raster,
					      "gs_image_next(row)"));

			if (row == 0) {
			    code = gs_note_error(gs_error_VMerror);
			    free_row_buffers(penum, i, "gs_image_next(row)");
			    break;
			}
			penum->planes[i].row.data = row;
			penum->planes[i].row.size = raster;
		    }
		    if ((count[i] = min(raster - penum->planes[i].pos,
				        penum->planes[i].source.size)) > 0) {
			memcpy(penum->planes[i].row.data + penum->planes[i].pos,
			   penum->planes[i].source.data, count[i]);
			if ((penum->planes[i].pos += count[i]) < raster)
			    filled = false;
		    }
		}
	    }
	    if (code < 0)
		break;
	    if (filled) {
		for (i = 0; i < num_planes; ++i)
		    if (penum->wanted[i])
			planes[i].data = penum->planes[i].row.data;
		code = copy_planes(dev, penum, planes, 1, &direct);
		if (code < 0) {
		    /* Undo the incrementing of pos. */
		    for (i = 0; i < num_planes; ++i)
			if (penum->wanted[i])
			    penum->planes[i].pos -= count[i];
		    break;
		}
		for (i = 0; i < num_planes; ++i) {
		    penum->planes[i].pos = 0;
		}
		penum->y++;
	    }
	    for (i = 0; i < num_planes; ++i) {
		if (penum->wanted[i]) {
		    used[i] += count[i];
		    penum->planes[i].source.data += count[i];
		    if ((penum->planes[i].source.size -= count[i]) == 0)
			empty = true;
		}
	    }
	}
	if (filled & !code)
	    begin_planes(penum);
	if (empty)
	    break;
    }
    return code;
}

/* Clean up after processing an image. */
void
gs_image_cleanup(gs_image_enum * penum)
{
    free_row_buffers(penum, penum->num_planes, "gs_image_cleanup(row)");
    if (penum->dev != 0)
	gx_image_end(penum->info, !penum->error);
    /* Don't free the local enumerator -- the client does that. */
}
