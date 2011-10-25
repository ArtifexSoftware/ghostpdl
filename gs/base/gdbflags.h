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
/* $Id$ */
/* Debugging flag definitions */

/* This is a chameleonic header file; that is to say, it appears differently
 * in different circumstances. The caller should provide an appropriate
 * definition of FLAG and UNUSED before including it. As such there is no guard against
 * repeated inclusion.
 */

UNUSED(0)
UNUSED(1)
UNUSED(2)
UNUSED(3)
UNUSED(4)
UNUSED(5)
UNUSED(6)
UNUSED(7)
UNUSED(8)
UNUSED(9)
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
FLAG(ps_op_names,       '!', "Postscript operator names"),
FLAG(contexts_detail,   '"', "Contexts (detail)"),
FLAG(trace_errors,      '#', "Turn on tracing of error returns from operators"),
FLAG(memfill_obj,       '$', "Fill unused parts of object with identifiable garbage values"),
FLAG(ext_commands,      '%', "Externally processed commands"),
UNUSED('&')
FLAG(contexts,          '\'',"Contexts (create/destroy)"),
UNUSED('(')
UNUSED(')')
FLAG(image,             '*', "Image and rasterop parameters"),
FLAG(min_stack,         '+', "Use minimum_size stack blocks"),
FLAG(no_path_banding,   ',', "Don't use path-based banding"),
UNUSED('-')
FLAG(small_mem_tables,  '.', "Use small-memory table sizes even on large-memory machines"),
FLAG(file_line,         '/', "Include file/line info on all trace output"),
FLAG(gc,                '0', "Garbage collection, minimal detail"),
FLAG(type1,             '1', "Type 1 and type 43 font interpreter"),
FLAG(curve,             '2', "Curve subdivider/rasterizer"),
FLAG(curve_detail,      '3', "Curve subdivider/rasterizer (detail)"),
FLAG(gc_strings,        '4', "Garbage collector (strings)"),
FLAG(gc_strings_detail, '5', "Garbage collector (strings, detail)"),
FLAG(gc_chunks,         '6', "Garbage collector (chunks, roots)"),
FLAG(gc_objects,        '7', "Garbage collector (objects)"),
FLAG(gc_refs,           '8', "Garbage collector (refs)"),
FLAG(gc_pointers,       '9', "Garbage collector (pointers)"),
FLAG(time,              ':', "Command list and allocator time summary"),
UNUSED(';')
UNUSED('<')
UNUSED('=')
UNUSED('>')
FLAG(validate_pointers, '?', "Validate pointers before/during/after garbage collection/save and restore"),
FLAG(memfill_empty,     '@', "Fill empty storage with a distinctive bit pattern for debugging"),
FLAG(alloc_detail,      'A', "Allocator (all calls)"),
FLAG(bitmap_detail,     'B', "Bitmap images (detail)"),
UNUSED('C')
FLAG(dict_detail,       'D', "Dictionary (every lookup)"),
UNUSED('E')
FLAG(fill_detail,       'F', "Fill algorithm (detail)"),
UNUSED('G')
FLAG(halftones_detail,  'H', "Halftones (detail)"),
FLAG(interp_detail,     'I', "Interpreter (detail)"),
UNUSED('J')
FLAG(char_cache_detail, 'K', "Character cache (detail)"),
FLAG(clist_detail,      'L', "Command list (detail)"),
UNUSED('M')
UNUSED('N')
FLAG(stroke_detail,     'O', "Stroke (detail)"),
FLAG(paths_detail,      'P', "Paths (detail)"),
UNUSED('Q')
UNUSED('R')
FLAG(scanner,           'S', "Scanner"),
UNUSED('T')
FLAG(undo_detail,       'U', "Undo saver (for save/restore) (detail)"),
FLAG(compositors_detail,'V', "Compositors (alpha/transparency/overprint/rop) (detail)"),
UNUSED('W')
UNUSED('X')
FLAG(type1_hints_detail,'Y', "Type 1 hints (detail)"),
UNUSED('Z')
UNUSED('[')
UNUSED('\\')
UNUSED(']')
FLAG(ref_counts,        '^', "Reference counting"),
FLAG(high_level,        '_', "High level output"),
FLAG(no_hl_img_banding, '`', "Don't use high_level banded images"),
FLAG(alloc,             'a', "Allocator (large blocks)"),
FLAG(bitmap,            'b', "Bitmap images"),
FLAG(color_halftones,   'c', "Color Halftones"),
FLAG(dict,              'd', "Dictionary (put/undef)"),
FLAG(external_calls,    'e', "External (OS related) calls"),
FLAG(fill,              'f', "Fill algorithm"),
FLAG(gsave,             'g', "gsave/grestore"),
FLAG(halftones,         'h', "Halftones"),
FLAG(interp,            'i', "Interpreter (names only)"),
FLAG(comp_fonts,        'j', "Composite fonts"),
FLAG(char_cache,        'k', "Character cache"),
FLAG(clist,             'l', "Command list (bands)"),
FLAG(makefont,          'm', "makefont and font cache"),
FLAG(names,             'n', "Name lookup (new names only)"),
FLAG(stroke,            'o', "Stroke"),
FLAG(paths,             'p', "Paths (band list)"),
FLAG(clipping,          'q', "Clipping"),
FLAG(arcs,              'r', "Arcs"),
FLAG(streams,           's', "Streams"),
FLAG(tiling,            't', "Tiling algorithm"),
FLAG(undo,              'u', "Undo saver (for save/restore)"),
FLAG(compositors,       'v', "Compositors (alpha/transparency/overprint/rop)"),
FLAG(compress,          'w', "Compression encoder/decoder"),
FLAG(transforms,        'x', "Transformations"),
FLAG(type1_hints,       'y', "Type 1 hints"),
FLAG(trapezoid_fill,    'z', "Trapezoid fill"),
UNUSED('{')
UNUSED('|') /* "Reserved for experimental code" */
UNUSED('}')
FLAG(math,              '~', "Math functions and Functions")
