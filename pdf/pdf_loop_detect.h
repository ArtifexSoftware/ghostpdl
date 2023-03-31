/* Copyright (C) 2018-2023 Artifex Software, Inc.
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

/* We handle circular references (loops) in the Pages tree or Resources as follows;
 * When we want to check for loops, we start up the loop detector
 * This stores an array of object numbers in the context
 * Every time we dereference an object, if the loop detector is present, pdf_dereference_object
 *       checks to see if the object its asked to dereference is in the array. If it is,
 *       we return an error. Ohterwise, when we finish creating the dereferenced object,
 *       we add its number to the array.
 * Now its possible for the same object to be referenced in different branches of a tree
 * and this simple detection would cause a problem. So we define a 'mark' in the array.
 * We use object number 0 for this since that is, by definition, always free.
 * When we start checking a branch, we 'mark' the array, and when we've finished the branch
 * we 'cleartomark'. Then if we find we are dereferencing the same object again we won't
 * produce a spurious error.
 */
#ifndef PDF_LOOP_DETECTOR
#define PDF_LOOP_DETECTOR

int pdfi_loop_detector_add_object(pdf_context *ctx, uint64_t object);
bool pdfi_loop_detector_check_object(pdf_context *ctx, uint64_t object);
int pdfi_loop_detector_mark(pdf_context *ctx);
int pdfi_loop_detector_cleartomark(pdf_context *ctx);

#endif
