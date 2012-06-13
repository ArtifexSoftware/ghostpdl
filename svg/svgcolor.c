/* Copyright (C) 2001-2012 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134, San Rafael,
   CA  94903, U.S.A., +1(415)492-9861, for further information.
*/


/* SVG interpreter - color support */

#include "ghostsvg.h"

/* Color keywords (white, blue, fuchsia)
 * System color keywords (ActiveBorder, ButtonFace -- need to find reasonable defaults)
 * #fb0 (expand to #ffbb00)
 * #ffbb00
 * rgb(255,255,255)
 * rgb(100%,100%,100%)
 *
 * "red icc-color(profileName,255,0,0)" (not going to support for now)
 */

struct
{
    const char *name;
    float red, green, blue;
}
svg_predefined_colors[] =
{
#include "svgcolorlist.h"
};

static int unhex(int chr)
{
    const char *hextable = "0123456789ABCDEF";
    return strchr(hextable, (toupper(chr))) - hextable;
}

void
svg_set_color(svg_context_t *ctx, float *rgb)
{
    gs_client_color cc;

    cc.pattern = 0;
    cc.paint.values[0] = rgb[0];
    cc.paint.values[1] = rgb[1];
    cc.paint.values[2] = rgb[2];

    gs_setcolor(ctx->pgs, &cc);
}

void svg_set_fill_color(svg_context_t *ctx)
{
    svg_set_color(ctx, ctx->fill_color);
}

void svg_set_stroke_color(svg_context_t *ctx)
{
    svg_set_color(ctx, ctx->stroke_color);
}

int
svg_parse_color(char *str, float *rgb)
{
    int i, l, m, r, cmp;

    rgb[0] = 0.0;
    rgb[1] = 0.0;
    rgb[2] = 0.0;

    /* Crack hex-coded RGB */

    if (str[0] == '#')
    {
        str ++;

        if (strlen(str) == 3)
        {
            rgb[0] = (unhex(str[0]) * 16 + unhex(str[0])) / 255.0;
            rgb[1] = (unhex(str[1]) * 16 + unhex(str[1])) / 255.0;
            rgb[2] = (unhex(str[2]) * 16 + unhex(str[2])) / 255.0;
            return 0;
        }

        if (strlen(str) == 6)
        {
            rgb[0] = (unhex(str[0]) * 16 + unhex(str[1])) / 255.0;
            rgb[1] = (unhex(str[2]) * 16 + unhex(str[3])) / 255.0;
            rgb[2] = (unhex(str[4]) * 16 + unhex(str[5])) / 255.0;
            return 0;
        }

        return gs_throw(-1, "syntax error in color - wrong length of string after #");
    }

    /* rgb(X,Y,Z) -- whitespace allowed around numbers */

    else if (strstr(str, "rgb("))
    {
        int numberlen = 0;
        char numberbuf[50];

        str = str + 4;

        for (i = 0; i < 3; i++)
        {
            while (svg_is_whitespace_or_comma(*str))
                str ++;

            if (svg_is_digit(*str))
            {
                numberlen = 0;
                while (svg_is_digit(*str) && numberlen < sizeof(numberbuf) - 1)
                    numberbuf[numberlen++] = *str++;
                numberbuf[numberlen] = 0;

                if (*str == '%')
                {
                    str ++;
                    rgb[i] = atof(numberbuf) / 100.0;
                }
                else
                {
                    rgb[i] = atof(numberbuf) / 255.0;
                }
            }
        }

        return 0;
    }

    /* TODO: parse icc-profile(X,Y,Z,W) syntax */

    /* Search for a pre-defined color */

    else
    {
        l = 0;
        r = sizeof(svg_predefined_colors) / sizeof(svg_predefined_colors[0]);

        while (l <= r)
        {
            m = (l + r) / 2;
            cmp = strcmp(svg_predefined_colors[m].name, str);
            if (cmp > 0)
                r = m - 1;
            else if (cmp < 0)
                l = m + 1;
            else
            {
                rgb[0] = svg_predefined_colors[m].red;
                rgb[1] = svg_predefined_colors[m].green;
                rgb[2] = svg_predefined_colors[m].blue;
                return 0;
            }
        }
    }

    return gs_throw1(-1, "cannot recognize color syntax: '%s'", str);
}
