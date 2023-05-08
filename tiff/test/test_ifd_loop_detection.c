/*
 * Copyright (c) 2022, Even Rouault <even.rouault at spatialys.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that (i) the above copyright notices and this permission notice appear in
 * all copies of the software and related documentation, and (ii) the names of
 * Sam Leffler and Silicon Graphics may not be used in any advertising or
 * publicity relating to the software without the specific, prior written
 * permission of Sam Leffler and Silicon Graphics.
 *
 * THE SOFTWARE IS PROVIDED "AS-IS" AND WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS, IMPLIED OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY
 * WARRANTY OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 *
 * IN NO EVENT SHALL SAM LEFFLER OR SILICON GRAPHICS BE LIABLE FOR
 * ANY SPECIAL, INCIDENTAL, INDIRECT OR CONSEQUENTIAL DAMAGES OF ANY KIND,
 * OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER OR NOT ADVISED OF THE POSSIBILITY OF DAMAGE, AND ON ANY THEORY OF
 * LIABILITY, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

/*
 * TIFF Library
 *
 * Test IFD loop detection
 */

#include "tif_config.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tiffio.h"

int main()
{
    int ret = 0;
    {
        TIFF *tif =
            TIFFOpen(SOURCE_DIR "/images/test_ifd_loop_to_self.tif", "r");
        assert(tif);
        if (TIFFReadDirectory(tif))
        {
            fprintf(stderr, "(1) Expected TIFFReadDirectory() to fail\n");
            ret = 1;
        }
        TIFFClose(tif);
    }
    {
        TIFF *tif =
            TIFFOpen(SOURCE_DIR "/images/test_ifd_loop_to_self.tif", "r");
        assert(tif);
        int n = TIFFNumberOfDirectories(tif);
        if (n != 1)
        {
            fprintf(
                stderr,
                "(2) Expected TIFFNumberOfDirectories() to return 1. Got %d\n",
                n);
            ret = 1;
        }
        TIFFClose(tif);
    }
    {
        TIFF *tif =
            TIFFOpen(SOURCE_DIR "/images/test_ifd_loop_to_first.tif", "r");
        assert(tif);
        if (TIFFReadDirectory(tif) != 1)
        {
            fprintf(stderr, "(3) Expected TIFFReadDirectory() to succeed\n");
            ret = 1;
        }
        if (TIFFReadDirectory(tif))
        {
            fprintf(stderr, "(4) Expected TIFFReadDirectory() to fail\n");
            ret = 1;
        }
        if (TIFFSetDirectory(tif, 1) != 1)
        {
            fprintf(stderr, "(5) Expected TIFFSetDirectory() to succeed\n");
            ret = 1;
        }
        if (TIFFReadDirectory(tif))
        {
            fprintf(stderr, "(6) Expected TIFFReadDirectory() to fail\n");
            ret = 1;
        }
        if (TIFFSetDirectory(tif, 0) != 1)
        {
            fprintf(stderr, "(7) Expected TIFFSetDirectory() to succeed\n");
            ret = 1;
        }
        if (TIFFReadDirectory(tif) != 1)
        {
            fprintf(stderr, "(8) Expected TIFFReadDirectory() to succeed\n");
            ret = 1;
        }
        if (TIFFReadDirectory(tif))
        {
            fprintf(stderr, "(9) Expected TIFFReadDirectory() to fail\n");
            ret = 1;
        }
        TIFFClose(tif);
    }
    {
        TIFF *tif =
            TIFFOpen(SOURCE_DIR "/images/test_ifd_loop_to_first.tif", "r");
        assert(tif);
        int n = TIFFNumberOfDirectories(tif);
        if (n != 2)
        {
            fprintf(
                stderr,
                "(10) Expected TIFFNumberOfDirectories() to return 2. Got %d\n",
                n);
            ret = 1;
        }
        TIFFClose(tif);
    }
    {
        TIFF *tif = TIFFOpen(SOURCE_DIR "/images/test_two_ifds.tif", "r");
        assert(tif);
        if (TIFFReadDirectory(tif) != 1)
        {
            fprintf(stderr, "(11) Expected TIFFReadDirectory() to succeed\n");
            ret = 1;
        }
        if (TIFFReadDirectory(tif))
        {
            fprintf(stderr, "(12) Expected TIFFReadDirectory() to fail\n");
            ret = 1;
        }
        if (TIFFSetDirectory(tif, 1) != 1)
        {
            fprintf(stderr, "(13) Expected TIFFSetDirectory() to succeed\n");
            ret = 1;
        }
        if (TIFFReadDirectory(tif))
        {
            fprintf(stderr, "(14) Expected TIFFReadDirectory() to fail\n");
            ret = 1;
        }
        if (TIFFSetDirectory(tif, 0) != 1)
        {
            fprintf(stderr, "(15) Expected TIFFSetDirectory() to succeed\n");
            ret = 1;
        }
        if (TIFFReadDirectory(tif) != 1)
        {
            fprintf(stderr, "(16) Expected TIFFReadDirectory() to succeed\n");
            ret = 1;
        }
        if (TIFFReadDirectory(tif))
        {
            fprintf(stderr, "(17) Expected TIFFReadDirectory() to fail\n");
            ret = 1;
        }
        TIFFClose(tif);
    }
    {
        TIFF *tif = TIFFOpen(SOURCE_DIR "/images/test_two_ifds.tif", "r");
        assert(tif);
        int n = TIFFNumberOfDirectories(tif);
        if (n != 2)
        {
            fprintf(
                stderr,
                "(18) Expected TIFFNumberOfDirectories() to return 2. Got %d\n",
                n);
            ret = 1;
        }
        TIFFClose(tif);
    }
    return ret;
}
