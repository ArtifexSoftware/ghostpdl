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


/* jconfig.h file for Independent JPEG Group code */

#ifndef gsjconf_INCLUDED
#  define gsjconf_INCLUDED

/*
 * We should have the following here:

#include "stdpre.h"

 * But because of the directory structure used to build the IJG library, we
 * actually concatenate stdpre.h on the front of this file instead to
 * construct the jconfig.h file used for the compilation.
 */

#include "arch.h"

#ifdef GS_HIDE_INTERNAL_JPEG
/* The first list is culled from NEED_SHORT_EXTERNAL_NAMES */
#define jpeg_std_error                gpeg_std_error
#define jpeg_CreateCompress           gpeg_CreateCompress
#define jpeg_CreateDecompress         gpeg_CreateDecompress
#define jpeg_destroy_compress         gpeg_destroy_compress
#define jpeg_destroy_decompress       gpeg_destroy_decompress
#define jpeg_stdio_dest               gpeg_stdio_dest
#define jpeg_stdio_src                gpeg_stdio_src
#define jpeg_mem_dest                 gpeg_mem_dest
#define jpeg_mem_src                  gpeg_mem_src
#define jpeg_set_defaults             gpeg_set_defaults
#define jpeg_set_colorspace           gpeg_set_colorspace
#define jpeg_default_colorspace       gpeg_default_colorspace
#define jpeg_set_quality              gpeg_set_quality
#define jpeg_set_linear_quality       gpeg_set_linear_quality
#define jpeg_default_qtables          gpeg_default_qtables
#define jpeg_add_quant_table          gpeg_add_quant_table
#define jpeg_quality_scaling          gpeg_quality_scaling
#define jpeg_simple_progression       gpeg_simple_progression
#define jpeg_suppress_tables          gpeg_suppress_tables
#define jpeg_alloc_quant_table        gpeg_alloc_quant_table
#define jpeg_alloc_huff_table         gpeg_alloc_huff_table
#define jpeg_start_compress           gpeg_start_compress
#define jpeg_write_scanlines          gpeg_write_scanlines
#define jpeg_finish_compress          gpeg_finish_compress
#define jpeg_calc_jpeg_dimensions     gpeg_calc_jpeg_dimensions
#define jpeg_write_raw_data           gpeg_write_raw_data
#define jpeg_write_marker             gpeg_write_marker
#define jpeg_write_m_header           gpeg_write_m_header
#define jpeg_write_m_byte             gpeg_write_m_byte
#define jpeg_write_tables             gpeg_write_tables
#define jpeg_read_header              gpeg_read_header
#define jpeg_start_decompress         gpeg_start_decompress
#define jpeg_read_scanlines           gpeg_read_scanlines
#define jpeg_finish_decompress        gpeg_finish_decompress
#define jpeg_read_raw_data            gpeg_read_raw_data
#define jpeg_has_multiple_scans       gpeg_has_multiple_scans
#define jpeg_start_output             gpeg_start_output
#define jpeg_finish_output            gpeg_finish_output
#define jpeg_input_complete           gpeg_input_complete
#define jpeg_new_colormap             gpeg_new_colormap
#define jpeg_consume_input            gpeg_consume_input
#define jpeg_core_output_dimensions   gpeg_core_output_dimensions
#define jpeg_calc_output_dimensions   gpeg_calc_output_dimensions
#define jpeg_save_markers             gpeg_save_markers
#define jpeg_set_marker_processor     gpeg_set_marker_processor
#define jpeg_read_coefficients        gpeg_read_coefficients
#define jpeg_write_coefficients       gpeg_write_coefficients
#define jpeg_copy_critical_parameters gpeg_copy_critical_parameters
#define jpeg_abort_compress           gpeg_abort_compress
#define jpeg_abort_decompress         gpeg_abort_decompress
#define jpeg_abort                    gpeg_abort
#define jpeg_destroy                  gpeg_destroy
#define jpeg_resync_to_restart        gpeg_resync_to_restart

/* This second list comes from examination of symbols in the lib */
#define jpeg_free_small               gpeg_free_small
#define jpeg_get_small                gpeg_get_small
#define jpeg_get_large                gpeg_get_large
#define jpeg_free_large               gpeg_free_large
#define jpeg_mem_available            gpeg_mem_available
#define jpeg_open_backing_store       gpeg_open_backing_store
#define jpeg_mem_init                 gpeg_mem_init
#define jpeg_mem_term                 gpeg_mem_term
#define jpeg_natural_order            gpeg_natural_order
#define jpeg_natural_order2           gpeg_natural_order2
#define jpeg_natural_order3           gpeg_natural_order3
#define jpeg_natural_order4           gpeg_natural_order4
#define jpeg_natural_order5           gpeg_natural_order5
#define jpeg_natural_order6           gpeg_natural_order6
#define jpeg_natural_order7           gpeg_natural_order7
#define jpeg_fdct_10x10               gpeg_fdct_10x10
#define jpeg_fdct_10x5                gpeg_fdct_10x5
#define jpeg_fdct_11x11               gpeg_fdct_11x11
#define jpeg_fdct_12x12               gpeg_fdct_12x12
#define jpeg_fdct_12x6                gpeg_fdct_12x6
#define jpeg_fdct_13x13               gpeg_fdct_13x13
#define jpeg_fdct_14x14               gpeg_fdct_14x14
#define jpeg_fdct_14x7                gpeg_fdct_14x7
#define jpeg_fdct_15x15               gpeg_fdct_15x15
#define jpeg_fdct_16x16               gpeg_fdct_16x16
#define jpeg_fdct_16x8                gpeg_fdct_16x8
#define jpeg_fdct_1x1                 gpeg_fdct_1x1
#define jpeg_fdct_1x2                 gpeg_fdct_1x2
#define jpeg_fdct_2x1                 gpeg_fdct_2x1
#define jpeg_fdct_2x2                 gpeg_fdct_2x2
#define jpeg_fdct_2x4                 gpeg_fdct_2x4
#define jpeg_fdct_3x3                 gpeg_fdct_3x3
#define jpeg_fdct_3x6                 gpeg_fdct_3x6
#define jpeg_fdct_4x2                 gpeg_fdct_4x2
#define jpeg_fdct_4x4                 gpeg_fdct_4x4
#define jpeg_fdct_4x8                 gpeg_fdct_4x8
#define jpeg_fdct_5x10                gpeg_fdct_5x10
#define jpeg_fdct_5x5                 gpeg_fdct_5x5
#define jpeg_fdct_6x12                gpeg_fdct_6x12
#define jpeg_fdct_6x3                 gpeg_fdct_6x3
#define jpeg_fdct_6x6                 gpeg_fdct_6x6
#define jpeg_fdct_7x14                gpeg_fdct_7x14
#define jpeg_fdct_7x7                 gpeg_fdct_7x7
#define jpeg_fdct_8x16                gpeg_fdct_8x16
#define jpeg_fdct_8x4                 gpeg_fdct_8x4
#define jpeg_fdct_9x9                 gpeg_fdct_9x9
#define jpeg_cust_mem_init            gpeg_cust_mem_init
#define jpeg_cust_mem_set_private     gpeg_cust_mem_set_private
#define jpeg_fill_bit_buffer          gpeg_fill_bit_buffer
#define jpeg_huff_decode              gpeg_huff_decode
#define jpeg_make_c_derived_tbl       gpeg_make_c_derived_tbl
#define jpeg_make_d_derived_tbl       gpeg_make_d_derived_tbl
#define jpeg_zigzag_order             gpeg_zigzag_order
#define jpeg_zigzag_order2            gpeg_zigzag_order2
#define jpeg_zigzag_order3            gpeg_zigzag_order3
#define jpeg_zigzag_order4            gpeg_zigzag_order4
#define jpeg_zigzag_order5            gpeg_zigzag_order5
#define jpeg_zigzag_order6            gpeg_zigzag_order6
#define jpeg_zigzag_order7            gpeg_zigzag_order7
#define jpeg_std_message_table        gpeg_std_message_table
#define jpeg_aritab                   gpeg_aritab
#define jpeg_idct_islow               gpeg_idct_islow
#define jpeg_fdct_islow               gpeg_fdct_islow
#define jpeg_aritab                   gpeg_aritab
#define jpeg_gen_optimal_table        gpeg_gen_optimal_table
#define jinit_marker_reader           ginit_marker_reader
#define jdiv_round_up                 gdiv_round_up
#define jround_up                     ground_up
#define jcopy_block_row               gcopy_block_row
#define jcopy_sample_rows             gcopy_sample_rows
#define jinit_input_controller        ginit_input_controller
#define jinit_memory_mgr              ginit_memory_mgr
#define jinit_master_decompress       ginit_master_decompress
#define jinit_huff_decoder            ginit_huff_decoder
#define jinit_d_coef_controller       ginit_d_coef_controller
#define jinit_color_deconverter       ginit_color_deconverter
#define jinit_inverse_dct             ginit_inverse_dct
#define jinit_d_main_controller       ginit_d_main_controller
#define jinit_arith_decoder           ginit_arith_decoder
#define jinit_color_deconverter       ginit_color_deconverter
#define jinit_d_coef_controller       ginit_d_coef_controller
#define jinit_d_main_controller       ginit_d_main_controller
#define jinit_d_post_controller       ginit_d_post_controller
#define jinit_huff_decoder            ginit_huff_decoder
#define jinit_master_decompress       ginit_master_decompress
#define jinit_upsampler               ginit_upsampler
#define jinit_d_post_controller       ginit_d_post_controller
#define jinit_downsampler             ginit_downsampler
#define jinit_arith_decoder           ginit_arith_decoder
#define jinit_marker_writer           ginit_marker_writer
#define jinit_marker_mgr              ginit_marker_mgr
#define jinit_compress_master         ginit_compress_master
#define jinit_c_coef_controller       ginit_c_coef_controller
#define jinit_color_converter         ginit_color_converter
#define jinit_forward_dct             ginit_forward_dct
#define jinit_arith_encoder           ginit_arith_encoder
#define jinit_huff_encoder            ginit_huff_encoder
#define jinit_c_main_controller       ginit_c_main_controller
#define jinit_marker_writer           ginit_marker_writer
#define jinit_c_master_control        ginit_c_master_control
#define jinit_c_prep_controller       ginit_c_prep_controller
#endif

/* See IJG's jconfig.doc for the contents of this file. */

#ifdef __PROTOTYPES__
#  define HAVE_PROTOTYPES
#endif

#define HAVE_UNSIGNED_CHAR
#define HAVE_UNSIGNED_SHORT
#undef CHAR_IS_UNSIGNED

#ifdef __STDC__			/* is this right? */
# ifndef HAVE_STDDEF_H
#  define HAVE_STDDEF_H
# endif
# ifndef HAVE_STDLIB_H
#  define HAVE_STDLIB_H
# endif
#endif

#undef NEED_BSD_STRINGS		/* WRONG */
#undef NEED_SYS_TYPES_H		/* WRONG */
#undef NEED_FAR_POINTERS
#undef NEED_SHORT_EXTERNAL_NAMES

#undef INCOMPLETE_TYPES_BROKEN

/* The following is documented in jmemsys.h, not jconfig.doc. */
#if ARCH_LOG2_SIZEOF_INT <= 1
#  undef MAX_ALLOC_CHUNK
#  define MAX_ALLOC_CHUNK 0xfff0
#endif

#ifdef JPEG_INTERNALS

#if ARCH_ARITH_RSHIFT == 0
#  define RIGHT_SHIFT_IS_UNSIGNED
#else
#  undef RIGHT_SHIFT_IS_UNSIGNED
#endif

#endif /* JPEG_INTERNALS */

#endif /* gsjconf_INCLUDED */
