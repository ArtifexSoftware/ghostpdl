/* Copyright (C) 2022-2023 Artifex Software, Inc.
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

/* Functions to parse a PageList string and processing utilitiess	*/

/* Allocate an array of ranges. A range of 0-0 terminates the list	*/
/* array contains even/odd flag, start, end (3 ints per range).	*/
/* returns error code < 0 or number of ranges if success 		*/
int pagelist_parse_to_array(char *pPageList, gs_memory_t *mem, int num_pages, int **parray);

/* function to calculate the total number of pages to be output		*/
int pagelist_number_of_pages(const int *parray);

/* Return true if this pagelist is in strict increasing order		*/
bool pagelist_test_ordered(int *parray);

/* Return true if this page is to be printed (for sequential processing)	*/
/* This assumes that the PageList is strictly sequential, tested by client.	*/
bool pagelist_test_printed(int *parray, int pagenum);

void pagelist_free_range_array(gs_memory_t *mem, int *parray);
