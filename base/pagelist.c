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


#include "memory_.h"
#include "string_.h"		/* for strcspn */
#include <stdlib.h>		/* for atoi */
#include "gsmemory.h"
#include "gserrors.h"
#include "pagelist.h"

/* array contains an "ordered" flag, then even/odd, start, end triples	*/

/* Allocate an array of ranges. A range of 0-0 terminates the list	*/
/* Note the first int 0 is order unknown, < 0 is unordered, > 0 is	*/
/* ordered and thus is suitable for sequential parsers like PS or PCL.	*/
/* This first int is also used by test_printed as the offset into the	*/
/* array for fast processing by SkipPage.				*/
/* returns error code < 0 or number of ranges if success. 		*/
int
pagelist_parse_to_array(char *page_list, gs_memory_t *mem, int num_pages, int **parray)
{
    int i = 0, range_count = 0;
    int *pagelist_array = NULL;
    char *p = page_list;
    int prev_end = 0;		/* used to track if the ranges are ordered */
    int ordered = 1;		/* default is ordered, until it's not */
    int comma, len;

    *parray = NULL;			/* in case PageList is empty */
    if (page_list[0] == 0)
        return gs_error_rangecheck;	/* empty list not valid */

    do {
        len = strlen(p);
        comma = strcspn(p, ",");
        p += comma + 1;
        if (comma > 0)
            range_count++;
    } while (comma < len);
    if (range_count == 0)
        return gs_error_rangecheck;	/* not valid */

    range_count++;			/* room for the end of list marker: 0, 0, 0 */
    pagelist_array = (int *)gs_alloc_byte_array(mem, 1 + (range_count * 3), sizeof(int), "pagelist_parse_to_array");
    *parray = pagelist_array;		/* return the array (NULL is OK) */
    if (pagelist_array == NULL)
        return gs_error_VMerror;

    memset(pagelist_array, 0, (1 + range_count * 3) * sizeof(int));
    /* Fill the array */

    p = page_list;		/* go back to start of string */
    for (i=1; i < (range_count - 1) * 3; ) {
        /* Parsing method from Michael Vrhel's code in xps */
        int dash, start, end, even_odd;

        even_odd = 0;
        start = 1;
        end = -1;			/* internal flag for last page */
        len = strlen(p);
        comma = strcspn(p, ",");
        dash = strcspn(p, "-");

        if (comma == 0) {
            p++;
            continue;			/* skip leading or adjacent commas */
        }
        if (strncmp(p, "even", 4) == 0) {
            even_odd = 2;
            p += 4;
        } else if (strncmp(p, "odd", 3) == 0) {
            even_odd = 1;
            p += 3;
        }
        /* check if the next after "even" or "odd" is ':' */
        if (even_odd != 0) {
            start = even_odd;		/* default is entire range */
	    end = -1;
	    if (*p == ':')
		p++;			/* skip to the range */
	    len = strlen(p);
	    comma = strcspn(p, ",");
	    dash = strcspn(p, "-");
        }
	if (comma > 0 && *p != 0) {
	    /* We have a range to process */
	    if (dash < comma) {
		/* Dash at start */
		if (dash == 0) {
		    start = -1;
		    end = atoi(p + dash + 1);
		} else {
		    start = atoi(p);
		    /* Dash at end */
		    if (p[dash + 1] == 0 || p[dash + 1] == ',') {
			end = -1;
		    } else {
			end = atoi(p + dash + 1);
		    }
		}
	    } else {
		start = atoi(p);
		end = start;
	    }
	}
        if (comma == len)
            p += comma;
        else
            p += comma + 1;

        /* If even or odd, and we have a "num_pages" flag == -1, adjust to proper value */
        if (even_odd == 2) {
            if (start == -1)
                start = num_pages & -2;
            if (end == -1)
                end = num_pages & -2;
        } else if (even_odd == 1) {
            if (start == -1)
                start = num_pages - (1 & (num_pages ^ 1));
            if (end == -1)
                end = num_pages - (1 & (num_pages ^ 1));
        } else {
            if (start == -1)
                start = num_pages;
            if (end == -1)
                end = num_pages;
        }
        /* Store this range, bump the index as we do */
        pagelist_array[i++] = even_odd;
        pagelist_array[i++] = start;
        pagelist_array[i++] = end;

        if (start <= prev_end || start > end)
            ordered = -1;
        prev_end = end;
    }
    /* Note final array entries are 0, 0, 0 from memset above */
    pagelist_array[0] = ordered;
    return range_count;
}

/* function to calculate the total number of pages to be output		*/
int
pagelist_number_of_pages(const int *parray)
{
    int count = 0, i;

    /* loop over ranges until start == 0 */
    for (i=1; parray[i+1] != 0; i += 3) {
        int start = parray[i+1];
        int end = parray[i+2];

        if (end >= start)
            count += 1 + end - start;
        else
            count += 1 + start - end;
    }
    return count;
}

/* Return true if this pagelist is in strict increasing order		*/
bool
pagelist_test_ordered(int *parray)
{
    int prev;
    int i;

    /* check flag (fast for use by the flp SkipPage function	*/
    if (parray[0] > 0)
        return true;
    if (parray[0] < 0)
        return false;

    /* parray[0] == 0, scan until start of range is 0 */
    prev = 0;			/* page 1 or more is legal */
    for (i=1; parray[i+1] != 0; i += 3) {
        if (parray[i+1] <= prev || parray[i+1] < parray[i+2])
            break;		/* NOT ordered */
        prev = parray[i+2];	/* advance to end of range */
    }
    /* if we made it to the marker at the end, then the list is ordered */
    parray[0] = (parray[i+1] == 0) ? 1 : -1;
    return parray[0] > 0;
}

/* Return true if this page is to be printed. (For sequential processing)	*/
/* This assumes/requires that the PageList is strictly sequential.		*/
bool
pagelist_test_printed(int *parray, int pagenum)
{
    int i = parray[0];

    if (i <= 0) {			/* shouldn't happen */
        pagelist_test_ordered(parray);	/* in case caller didn't do this */
        i = parray[0];
        if (i < 0)
            return false;			/* list is NOT ordered, punt	*/
    }

    /* advance to next range if pagenum past the end of this range	*/
    /* stopping at the 0, 0, 0 marker (start == 0)			*/
    while (pagenum > parray[i+2] && parray[i+1] != 0) {
        i += 3;			/* bump to next range for next call */
        parray[0] = i;		/* update for next entry */
    }
    if (parray[i+1] == 0)
        return false;		/* reached end of ranges */

    /* First Test even, then odd */
    if (parray[i] == 2 && (pagenum & 1) == 1)
        return false;
    if (parray[i] == 1 && (pagenum & 1) == 0)
        return false;

    /* Fast case for ordered list and sequential page numbers (FLP device) */
    if (i > 0) {
        if (pagenum >= parray[i+1] && pagenum <= parray[i+2]) {
            return true;		/* page was in this range */
        } else {
            if (pagenum < parray[i+1])
                return false;	/* pagennum in between ranges that skip */
        }
    }
    return false;
}

void
pagelist_free_range_array(gs_memory_t *mem, int *parray)
{
    gs_free_object(mem, parray, "flp_close_device");
}
