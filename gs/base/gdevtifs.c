/* Copyright (C) 2001-2006 Artifex Software, Inc.
   All Rights Reserved.
  
   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

/* $Id$ */
/* TIFF-writing substructure */
#include "stdio_.h"
#include "time_.h"
#include "gstypes.h"
#include "gscdefs.h"
#include "gdevprn.h"
#include "gdevtifs.h"

/*
 * Define the standard contents of a TIFF directory.
 * Clients may add more items, also sorted in increasing tag order.
 */
typedef struct TIFF_std_directory_entries_s {
    TIFF_dir_entry SubFileType;
    TIFF_dir_entry ImageWidth;
    TIFF_dir_entry ImageLength;
    TIFF_dir_entry StripOffsets;
    TIFF_dir_entry Orientation;
    TIFF_dir_entry RowsPerStrip;
    TIFF_dir_entry StripByteCounts;
    TIFF_dir_entry XResolution;
    TIFF_dir_entry YResolution;
    TIFF_dir_entry PlanarConfig;
    TIFF_dir_entry ResolutionUnit;
    TIFF_dir_entry PageNumber;
    TIFF_dir_entry Software;
    TIFF_dir_entry DateTime;
} TIFF_std_directory_entries;

/* Define values that follow the directory entries. */
typedef struct TIFF_std_directory_values_s {
    TIFF_ulong diroff;		/* offset to next directory */
    TIFF_ulong xresValue[2];	/* XResolution indirect value */
    TIFF_ulong yresValue[2];	/* YResolution indirect value */
#define maxSoftware 40
    char softwareValue[maxSoftware];	/* Software indirect value */
    char dateTimeValue[20];	/* DateTime indirect value */
} TIFF_std_directory_values;
static const TIFF_std_directory_entries std_entries_initial =
{
    {TIFFTAG_SubFileType, TIFF_LONG, 1, SubFileType_page},
    {TIFFTAG_ImageWidth, TIFF_LONG, 1},
    {TIFFTAG_ImageLength, TIFF_LONG, 1},
    {TIFFTAG_StripOffsets, TIFF_LONG, 1},
    {TIFFTAG_Orientation, TIFF_SHORT, 1, Orientation_top_left},
    {TIFFTAG_RowsPerStrip, TIFF_LONG, 1},
    {TIFFTAG_StripByteCounts, TIFF_LONG, 1},
    {TIFFTAG_XResolution, TIFF_RATIONAL | TIFF_INDIRECT, 1,
     offset_of(TIFF_std_directory_values, xresValue[0])},
    {TIFFTAG_YResolution, TIFF_RATIONAL | TIFF_INDIRECT, 1,
     offset_of(TIFF_std_directory_values, yresValue[0])},
    {TIFFTAG_PlanarConfig, TIFF_SHORT, 1, PlanarConfig_contig},
    {TIFFTAG_ResolutionUnit, TIFF_SHORT, 1, ResolutionUnit_inch},
    {TIFFTAG_PageNumber, TIFF_SHORT, 2},
    {TIFFTAG_Software, TIFF_ASCII | TIFF_INDIRECT, 0,
     offset_of(TIFF_std_directory_values, softwareValue[0])},
    {TIFFTAG_DateTime, TIFF_ASCII | TIFF_INDIRECT, 20,
     offset_of(TIFF_std_directory_values, dateTimeValue[0])}
};
static const TIFF_std_directory_values std_values_initial =
{
    0,
    {0, 1},
    {0, 1},
    {0},
    {0, 0}
};

/* Get/put the BigEndian parameter. */
int
tiff_get_params(gx_device * dev, gs_param_list * plist)
{
    gx_device_tiff *const tfdev = (gx_device_tiff *)dev;
    int code = gdev_prn_get_params(dev, plist);
    int ecode = code;
    
    if ((code = param_write_bool(plist, "BigEndian", &tfdev->BigEndian)) < 0)
        ecode = code;
    return ecode;
}

int
tiff_put_params(gx_device * dev, gs_param_list * plist)
{
    gx_device_tiff *const tfdev = (gx_device_tiff *)dev;
    int ecode = 0;
    int code;
    const char *param_name;
    bool big_endian = tfdev->BigEndian;
    
    /* Read BigEndian option as bool */ 
    switch (code = param_read_bool(plist, (param_name = "BigEndian"), &big_endian)) {
	default:
	    ecode = code;
	    param_signal_error(plist, param_name, ecode);
        case 0:
	case 1:
	    break;
    }

    if (ecode < 0)
	return ecode;
    code = gdev_prn_put_params(dev, plist);
    if (code < 0)
	return code;

    tfdev->BigEndian = big_endian;
    return code;
}

/* Fix up tag values on big-endian machines if necessary. */
static void
tiff_fixup_tag(TIFF_dir_entry * dp)
{
    switch (dp->type) {
	case TIFF_SHORT:
	case TIFF_SSHORT:
	    /* We may have two shorts packed into a TIFF_ulong. */
	    dp->value = (dp->value << 16) + (dp->value >> 16);
	    break;
	case TIFF_BYTE:
	case TIFF_SBYTE:
	    dp->value <<= 24;
	    break;
    }
}

static size_t fwrite_short(TIFF_ushort nVal, FILE *stream, bool swap_bytes)
{
    TIFF_ushort v;
    
    if (swap_bytes)
        v = nVal << 8 | nVal >> 8;
    else
        v = nVal;
    return fwrite(&v, sizeof(v), 1, stream);
}

static size_t fwrite_long(TIFF_ulong nVal, FILE *stream, bool swap_bytes)
{
    TIFF_ulong v;
    
    if (swap_bytes)
       v = nVal << 24 | nVal >> 24 | (nVal & 0xff0000) >> 8 | (nVal & 0xff00) << 8;
    else
       v = nVal;
    return fwrite(&v, sizeof(v), 1, stream);
 }

static void fwrite_entry(TIFF_dir_entry *entry, FILE *stream, bool swap_bytes)
{
   fwrite_short(entry->tag, stream, swap_bytes);
   fwrite_short(entry->type, stream, swap_bytes);
   fwrite_long(entry->count, stream, swap_bytes);
   fwrite_long(entry->value, stream, swap_bytes);
}

static void fwrite_std_values(TIFF_std_directory_values *std_values, FILE *stream, bool swap_bytes)
{
   fwrite_long(std_values->diroff, stream, swap_bytes);
   fwrite_long(std_values->xresValue[0], stream, swap_bytes);
   fwrite_long(std_values->xresValue[1], stream, swap_bytes);
   fwrite_long(std_values->yresValue[0], stream, swap_bytes);
   fwrite_long(std_values->yresValue[1], stream, swap_bytes);
   fwrite(std_values->softwareValue, 1, maxSoftware, stream);
   fwrite(std_values->dateTimeValue, 1, 20, stream);
}

/* Begin a TIFF page. */
int
gdev_tiff_begin_page(gx_device_printer * pdev, gdev_tiff_state * tifs,
		     FILE * fp,
		     const TIFF_dir_entry * entries, int entry_count,
		     const byte * values, int value_size, long max_strip_size,
		     bool swap_bytes)
{
    gs_memory_t *mem = pdev->memory;
    TIFF_std_directory_entries std_entries;
    const TIFF_dir_entry *pse;
    const TIFF_dir_entry *pce;
    TIFF_dir_entry entry;
#define std_entry_count\
  (sizeof(TIFF_std_directory_entries) / sizeof(TIFF_dir_entry))
    int nse, nce, ntags;
    TIFF_std_directory_values std_values;
    bool big_endian = arch_is_big_endian ^ swap_bytes;

#define std_value_size sizeof(TIFF_std_directory_values)

    tifs->mem = mem;
    if (gdev_prn_file_is_new(pdev)) {
	/* This is a new file; write the TIFF header. */
	TIFF_header hdr;
 	hdr.magic = big_endian ? TIFF_magic_big_endian : TIFF_magic_little_endian;
	hdr.version = TIFF_version_value;
	hdr.diroff = sizeof(TIFF_header);
	fwrite((char *)&hdr.magic, sizeof(hdr.magic), 1, fp);
	fwrite_short(hdr.version, fp, swap_bytes);
	fwrite_long(hdr.diroff, fp, swap_bytes);
	tifs->prev_dir = 0;
    } else {			/* Patch pointer to this directory from previous. */
	TIFF_ulong offset = (TIFF_ulong) tifs->dir_off;

	fseek(fp, tifs->prev_dir, SEEK_SET);
	fwrite_long(offset, fp, swap_bytes);
	fseek(fp, tifs->dir_off, SEEK_SET);
    }

    /* We're going to shuffle the two tag lists together. */
    /* Both lists are sorted; entries in the client list */
    /* replace entries with the same tag in the standard list. */
    for (ntags = 0, pse = (const TIFF_dir_entry *)&std_entries_initial,
	 nse = std_entry_count, pce = entries, nce = entry_count;
	 nse && nce; ++ntags
	) {
	if (pse->tag < pce->tag)
	    ++pse, --nse;
	else if (pce->tag < pse->tag)
	    ++pce, --nce;
	else
	    ++pse, --nse, ++pce, --nce;
    }
    ntags += nse + nce;
    tifs->ntags = ntags;

    /* Write count of tags in directory. */
    {
	TIFF_short dircount = ntags;
	fwrite_short(dircount, fp, swap_bytes);
    }
    tifs->dir_off = ftell(fp);

    /* Fill in standard directory tags. */
    std_entries = std_entries_initial;
    std_values = std_values_initial;
    std_entries.ImageWidth.value = pdev->width;
    std_entries.ImageLength.value = pdev->height;
    if (max_strip_size == 0) {
	tifs->strip_count = 1;
	tifs->rows = pdev->height;
	std_entries.RowsPerStrip.value = pdev->height;
    } else {
	int max_strip_rows =
	    max_strip_size / gdev_mem_bytes_per_scan_line((gx_device *)pdev);
        int rps = max(1, max_strip_rows);

	tifs->strip_count = (pdev->height + rps - 1) / rps;
	tifs->rows = rps;
	std_entries.RowsPerStrip.value = rps;
	std_entries.StripOffsets.count = tifs->strip_count;
	std_entries.StripByteCounts.count = tifs->strip_count;
    }
    tifs->StripOffsets = (TIFF_ulong *)gs_alloc_bytes(mem,
    		    (tifs->strip_count)*2*sizeof(TIFF_ulong),
		    "gdev_tiff_begin_page(StripOffsets)");
    tifs->StripByteCounts = &(tifs->StripOffsets[tifs->strip_count]);
    if (tifs->StripOffsets == 0)
	return_error(gs_error_VMerror);
    std_entries.PageNumber.value = (TIFF_ulong) pdev->PageCount;
    std_values.xresValue[0] = (TIFF_ulong)pdev->x_pixels_per_inch;
    std_values.yresValue[0] = (TIFF_ulong)pdev->y_pixels_per_inch;
    {
	char revs[10];

	strncpy(std_values.softwareValue, gs_product, maxSoftware);
	std_values.softwareValue[maxSoftware - 1] = 0;
	sprintf(revs, " %1.2f", gs_revision / 100.0);
	strncat(std_values.softwareValue, revs,
		maxSoftware - strlen(std_values.softwareValue) - 1);
	std_entries.Software.count =
	    strlen(std_values.softwareValue) + 1;
    }
    {
	struct tm tms;
	time_t t;

	time(&t);
	tms = *localtime(&t);
	sprintf(std_values.dateTimeValue,
		"%04d:%02d:%02d %02d:%02d:%02d",
		tms.tm_year + 1900, tms.tm_mon + 1, tms.tm_mday,
		tms.tm_hour, tms.tm_min, tms.tm_sec);
    }

    /* Write the merged directory. */
    for (pse = (const TIFF_dir_entry *)&std_entries,
	 nse = std_entry_count, pce = entries, nce = entry_count;
	 nse || nce;
	) {
	bool std;

	if (nce == 0 || (nse != 0 && pse->tag < pce->tag))
	    std = true, entry = *pse++, --nse;
	else if (nse == 0 || (nce != 0 && pce->tag < pse->tag))
	    std = false, entry = *pce++, --nce;
	else
	    std = false, ++pse, --nse, entry = *pce++, --nce;
	if (entry.tag == TIFFTAG_StripOffsets)  {
	    if (tifs->strip_count > 1) {
		tifs->offset_StripOffsets = tifs->dir_off +
		    (ntags * sizeof(TIFF_dir_entry)) + std_value_size + value_size;
		entry.value = tifs->offset_StripOffsets;
	    } else {
		tifs->offset_StripOffsets = ftell(fp) +
		    offset_of(TIFF_dir_entry, value);
	    }
	}
	if (entry.tag == TIFFTAG_StripByteCounts) {
	    if (tifs->strip_count > 1) {
	        tifs->offset_StripByteCounts = tifs->dir_off +
		    (ntags * sizeof(TIFF_dir_entry)) + std_value_size + value_size +
		    (sizeof(TIFF_ulong) * tifs->strip_count);
	        entry.value = tifs->offset_StripByteCounts;
	    } else {
		tifs->offset_StripByteCounts = ftell(fp) +
		    offset_of(TIFF_dir_entry, value);
	    }
	}
	if (big_endian)
	tiff_fixup_tag(&entry);	/* don't fix up indirects */
	if (entry.type & TIFF_INDIRECT) {
	    /* Fix up the offset for an indirect value. */
	    entry.type -= TIFF_INDIRECT;
	    entry.value +=
		tifs->dir_off + ntags * sizeof(TIFF_dir_entry) +
		(std ? 0 : std_value_size);
	}
	fwrite_entry(&entry, fp, swap_bytes);
    }

    /* Write the indirect values. */
    fwrite_std_values(&std_values, fp, swap_bytes);
    if (values)
    fwrite((const char *)values, value_size, 1, fp);
    /* Write placeholders for the strip offsets. */
    fwrite(tifs->StripOffsets, sizeof(TIFF_ulong), 2 * tifs->strip_count, fp);
    tifs->strip_index = 0;
    tifs->StripOffsets[0] = ftell(fp);
    return 0;
}

/* End a TIFF strip. */
/* Record the size of the current strip, update the	*/
/* start of the next strip  and bump the strip_index	*/
int
gdev_tiff_end_strip(gdev_tiff_state * tifs, FILE * fp)
{
    TIFF_ulong strip_size;
    TIFF_ulong next_strip_start;
    int pad = 0;

    next_strip_start = (TIFF_ulong)ftell(fp);
    strip_size = next_strip_start - tifs->StripOffsets[tifs->strip_index];
    if (next_strip_start & 1) {
        next_strip_start++;	/* WORD alignment */
	fwrite(&pad, 1, 1, fp);
    }
    tifs->StripByteCounts[tifs->strip_index++] = strip_size;
    if (tifs->strip_index < tifs->strip_count)
	tifs->StripOffsets[tifs->strip_index] = next_strip_start;
    return 0;
}

/* End a TIFF page. */
int
gdev_tiff_end_page(gdev_tiff_state * tifs, FILE * fp, bool swap_bytes)
{
    gs_memory_t *mem = tifs->mem;
    long dir_off = tifs->dir_off;
    int tags_size = tifs->ntags * sizeof(TIFF_dir_entry), i;

    tifs->prev_dir =
	dir_off + tags_size + offset_of(TIFF_std_directory_values, diroff);
    tifs->dir_off = ftell(fp);
    /* Patch strip byte counts and offsets values. */
    /* The offset in the file was determined at begin_page and may be indirect */
    fseek(fp, tifs->offset_StripOffsets, SEEK_SET);
    for (i=0; i<tifs->strip_count; i++)
        fwrite_long(tifs->StripOffsets[i], fp, swap_bytes);
    fseek(fp, tifs->offset_StripByteCounts, SEEK_SET);
    for (i=0; i<tifs->strip_count; i++)
        fwrite_long(tifs->StripByteCounts[i], fp, swap_bytes);
    gs_free_object(mem, tifs->StripOffsets, "gdev_tiff_begin_page(StripOffsets)");
    return 0;
}
