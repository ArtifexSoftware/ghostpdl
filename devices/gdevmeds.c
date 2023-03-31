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

/*
 * Media selection support for printer drivers
 *
 * Select from a NULL terminated list of media the smallest medium which is
 * almost equal or larger then the actual imagesize.
 *
 * Written by Ulrich Schmid, uschmid@mail.hh.provi.de.
 */

#include "gdevmeds.h"

#define CM        * 0.01f
#define INCH      * 0.0254f
#define TOLERANCE 0.1f CM

static const struct {
        const char* name;
        float       width;
        float       height;
        float       priority;
} media[] = {
#define X(name, width, height) {name, width, height, 1 / (width * height)}
        X("a0",           83.9611f CM,   118.816f CM),
        X("a1",           59.4078f CM,   83.9611f CM),
        X("a2",           41.9806f CM,   59.4078f CM),
        X("a3",           29.7039f CM,   41.9806f CM),
        X("a4",           20.9903f CM,   29.7039f CM),
        X("a5",           14.8519f CM,   20.9903f CM),
        X("a6",           10.4775f CM,   14.8519f CM),
        X("a7",           7.40833f CM,   10.4775f CM),
        X("a8",           5.22111f CM,   7.40833f CM),
        X("a9",           3.70417f CM,   5.22111f CM),
        X("a10",          2.61056f CM,   3.70417f CM),
        X("archA",        9 INCH,       12 INCH),
        X("archB",        12 INCH,      18 INCH),
        X("archC",        18 INCH,      24 INCH),
        X("archD",        24 INCH,      36 INCH),
        X("archE",        36 INCH,      48 INCH),
        X("b0",           100.048f CM,   141.393f CM),
        X("b1",           70.6967f CM,   100.048f CM),
        X("b2",           50.0239f CM,   70.6967f CM),
        X("b3",           35.3483f CM,   50.0239f CM),
        X("b4",           25.0119f CM,   35.3483f CM),
        X("b5",           17.6742f CM,   25.0119f CM),
        X("flsa",         8.5f INCH,     13 INCH),
        X("flse",         8.5f INCH,     13 INCH),
        X("halfletter",   5.5f INCH,     8.5f INCH),
        X("ledger",       17 INCH,      11 INCH),
        X("legal",        8.5f INCH,     14 INCH),
        X("letter",       8.5f INCH,     11 INCH),
        X("note",         7.5f INCH,     10 INCH),
        X("executive",    7.25f INCH,    10.5f INCH),
        X("com10",        4.125f INCH,   9.5f INCH),
        X("dl",           11 CM,        22 CM),
        X("c5",           16.2f CM,      22.9f CM),
        X("monarch",      3.875f INCH,   7.5f INCH)};

int select_medium(gx_device_printer *pdev, const char **available, int default_index)
{
        int i, j, index = default_index;
        float priority = 0;
        float width = pdev->width / pdev->x_pixels_per_inch INCH;
        float height = pdev->height / pdev->y_pixels_per_inch INCH;

        for (i = 0; available[i]; i++) {
                for (j = 0; j < sizeof(media) / sizeof(media[0]); j++) {
                        if (!strcmp(available[i], media[j].name) &&
                            media[j].width + TOLERANCE > width &&
                            media[j].height + TOLERANCE > height &&
                            media[j].priority > priority) {
                                index = i;
                                priority = media[j].priority;
                        }
                }
        }
        return index;
}
