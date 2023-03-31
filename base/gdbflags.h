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

/* Debugging flag definitions */

/* This is a chameleonic header file; that is to say, it appears differently
 * in different circumstances. The caller should provide an appropriate
 * definition of FLAG and UNUSED before including it. As such there is no guard against
 * repeated inclusion.
 */

#if defined(UNUSED) && defined(FLAG)

UNUSED(0) /* Never use 0, as lots of things 'imply' 0. */
FLAG(icc, 1, 'c', "ICC profile"),
FLAG(validate_clumps, 2, 0, "Validate clumps during interpretation"),
FLAG(gc_disable, 3, 0, "Disable Garbage Collection (completely)"),
FLAG(epo_disable, 4, 0, "Disable Erasepage Optimization (completely)"),
FLAG(epo_details, 5, 0, "Erasepage Optimization tracing"),
FLAG(epo_install_only, 6, 0, "Erasepage Optimization install only (for debugging subclass)"),
FLAG(init_details, 7, 0, "Language initialisation (detail)"),
FLAG(overprint, 8, 0, "Overprint"),
FLAG(clist_color, 9, 0, "Clist color"),
UNUSED(10)
UNUSED(11)
UNUSED(12)
UNUSED(13)
UNUSED(14)
UNUSED(15)
UNUSED(16)
UNUSED(17)
UNUSED(18)
UNUSED(19)
UNUSED(20)
UNUSED(21)
UNUSED(22)
UNUSED(23)
UNUSED(24)
UNUSED(25)
UNUSED(26)
UNUSED(27)
UNUSED(28)
UNUSED(29)
UNUSED(30)
UNUSED(31)
UNUSED(32)
FLAG(ps_op_names,       '!', 0,   "Postscript operator names"),
FLAG(contexts_detail,   '"', 0,   "Contexts (detail)"),
FLAG(trace_errors,      '#', 0,   "Turn on tracing of error returns from operators"),
FLAG(memfill_obj,       '$', 0,   "Fill unused parts of object with identifiable garbage values"),
FLAG(ext_commands,      '%', 0,   "Externally processed commands"),
UNUSED('&')
FLAG(contexts,          '\'',0,   "Contexts (create/destroy)"),
UNUSED('(')
UNUSED(')')
FLAG(image,             '*', 0,   "Image and rasterop parameters"),
FLAG(min_stack,         '+', 0,   "Use minimum_size stack blocks"),
FLAG(no_path_banding,   ',', 0,   "Don't use path-based banding"),
UNUSED('-')
FLAG(small_mem_tables,  '.', 0,   "Use small-memory table sizes even on large-memory machines"),
FLAG(file_line,         '/', 0,   "Include file/line info on all trace output"),
FLAG(gc,                '0', 0,   "Garbage collection, minimal detail"),
FLAG(type1,             '1', 0,   "Type 1 and type 43 font interpreter"),
FLAG(curve,             '2', 0,   "Curve subdivider/rasterizer"),
FLAG(curve_detail,      '3', 0,   "Curve subdivider/rasterizer (detail)"),
FLAG(gc_strings,        '4', 0,   "Garbage collector (strings)"),
FLAG(gc_strings_detail, '5', 0,   "Garbage collector (strings, detail)"),
FLAG(gc_clumps,         '6', 0,   "Garbage collector (clumps, roots)"),
FLAG(gc_objects,        '7', 0,   "Garbage collector (objects)"),
FLAG(gc_refs,           '8', 0,   "Garbage collector (refs)"),
FLAG(gc_pointers,       '9', 0,   "Garbage collector (pointers)"),
FLAG(time,              ':', 0,   "Command list and allocator time summary"),
UNUSED(';')
UNUSED('<')
UNUSED('=')
UNUSED('>')
FLAG(validate_pointers, '?', 0,   "Validate pointers before/during/after garbage collection/save and restore"),
FLAG(memfill_empty,     '@', 0,   "Fill empty storage with a distinctive bit pattern for debugging"),
FLAG(alloc_detail,      'A', 0,   "Allocator (all calls)"),
FLAG(bitmap_detail,     'B', 0,   "Bitmap images (detail)"),
FLAG(color_detail,      'C', 0,   "Color mapping (detail)"),
FLAG(dict_detail,       'D', 0,   "Dictionary (every lookup)"),
UNUSED('E')
FLAG(fill_detail,       'F', 0,   "Fill algorithm (detail)"),
UNUSED('G')
FLAG(halftones_detail,  'H', 0,   "Halftones (detail)"),
FLAG(interp_detail,     'I', 0,   "Interpreter (detail)"),
UNUSED('J')
FLAG(char_cache_detail, 'K', 0,   "Character cache (detail)"),
FLAG(clist_detail,      'L', 0,   "Command list (detail)"),
UNUSED('M')
UNUSED('N')
FLAG(stroke_detail,     'O', 0,   "Stroke (detail)"),
FLAG(paths_detail,      'P', 0,   "Paths (detail)"),
UNUSED('Q')
UNUSED('R')
FLAG(scanner,           'S', 0,   "Scanner"),
UNUSED('T')
FLAG(undo_detail,       'U', 0,   "Undo saver (for save/restore) (detail)"),
FLAG(compositors_detail,'V', 0,   "Compositors (alpha/transparency/overprint/rop) (detail)"),
UNUSED('W')
UNUSED('X')
FLAG(type1_hints_detail,'Y', 0,   "Type 1 hints (detail)"),
UNUSED('Z')
UNUSED('[')
UNUSED('\\')
UNUSED(']')
FLAG(ref_counts,        '^', 0,   "Reference counting"),
FLAG(high_level,        '_', 0,   "High level output"),
FLAG(no_hl_img_banding, '`', 0,   "Don't use high_level banded images"),
FLAG(alloc,             'a', 'A', "Allocator (large blocks)"),
FLAG(bitmap,            'b', 'B', "Bitmap images"),
FLAG(color_halftones,   'c', 'C', "Color mapping"),
FLAG(dict,              'd', 'D', "Dictionary (put/undef)"),
FLAG(external_calls,    'e', 0,   "External (OS related) calls"),
FLAG(fill,              'f', 'F', "Fill algorithm"),
FLAG(gsave,             'g', 0,   "gsave/grestore"),
FLAG(halftones,         'h', 'H', "Halftones"),
FLAG(interp,            'i', 'I', "Interpreter (names only)"),
FLAG(comp_fonts,        'j', 0,   "Composite fonts"),
FLAG(char_cache,        'k', 'K', "Character cache"),
FLAG(clist,             'l', 'L', "Command list (bands)"),
FLAG(makefont,          'm', 0,   "makefont and font cache"),
FLAG(names,             'n', 0,   "Name lookup (new names only)"),
FLAG(stroke,            'o', 'O', "Stroke"),
FLAG(paths,             'p', 'P', "Paths (band list)"),
FLAG(clipping,          'q', 0,   "Clipping"),
FLAG(arcs,              'r', 0,   "Arcs"),
FLAG(streams,           's', 'S', "Streams"),
FLAG(tiling,            't', 0,   "Tiling algorithm"),
FLAG(undo,              'u', 'U', "Undo saver (for save/restore)"),
FLAG(compositors,       'v', 'V', "Compositors (alpha/transparency/overprint/rop)"),
FLAG(compress,          'w', 0,   "Compression encoder/decoder"),
FLAG(transforms,        'x', 0,   "Transformations"),
FLAG(type1_hints,       'y', 'Y', "Type 1 hints"),
FLAG(trapezoid_fill,    'z', 0,   "Trapezoid fill"),
UNUSED('{')
UNUSED('|') /* "Reserved for experimental code" */
UNUSED('}')
FLAG(math,              '~', 0,   "Math functions and Functions")

#else
int dummy;
#endif
