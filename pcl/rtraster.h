/* Copyright (C) 1996 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* rtraster.h */
/* Internal interface to HP RTL / PCL5 raster procedures */

/* These procedures are exported by rtraster.c for rtcrastr.c. */

/* Enter raster graphics mode. */
int pcl_begin_raster_graphics(P2(pcl_state_t *pcls, int setting));

/* If a transfer raster data command occurs outside graphics mode, */
/* enter graphics mode automatically. */
#define enter_graphics_mode_if_needed(pcls)\
do {\
  	if ( !pcls->raster.graphics_mode )\
	  { /* Enter raster graphics mode implicitly. */\
	    int code = pcl_begin_raster_graphics(pcls, 1);\
	    if ( code < 0 )\
	      return code;\
	  }\
} while ( 0 )

/* Resize (expand) a row buffer if needed. */
int pcl_resize_row(P4(pcl_raster_row_t *row, uint new_size,
		      gs_memory_t *mem, client_name_t cname));

/* Read one plane of data. */
int pcl_read_raster_data(P4(pcl_args_t *pargs, pcl_state_t *pcls,
			    int bits_per_pixel, bool last_plane));

/* Pass one row of raster data to an image. */
/* Note that this may pad the row with zeros. */
int pcl_image_row(P2(pcl_state_t *pcls, pcl_raster_row_t *row));

/* The following is exported by rtraster.c for rtlrastr.c. */

/* Register a data compression method. */
typedef int (*pcl_uncompress_proc_t)(P3(pcl_raster_row_t *row, uint in_size,
					stream_cursor_read *pr));
typedef struct pcl_compression_method_s {
  pcl_uncompress_proc_t proc;
  bool block;
} pcl_compression_method_t;
void pcl_register_compression_method(P3(int method, pcl_uncompress_proc_t proc,
					bool block));
extern /*const, for us*/ pcl_compression_method_t pcl_compression_methods[10];
