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


/* SVG interpreter - coordinate transformations */

#include "ghostsvg.h"

int svg_parse_transform(svg_context_t *ctx, char *str)
{
    char keyword[20];
    int keywordlen;
    char number[20];
    int numberlen;
    float args[6];
    int nargs;

    nargs = 0;
    keywordlen = 0;

    while (*str)
    {
        keywordlen = 0;
        nargs = 0;

        /*
         * Parse keyword and opening parenthesis.
         */

        while (svg_is_whitespace(*str))
            str ++;

        if (*str == 0)
            break;

        while (svg_is_alpha(*str) && keywordlen < sizeof(keyword) - 1)
            keyword[keywordlen++] = *str++;
        keyword[keywordlen] = 0;

        if (keywordlen == 0)
            return gs_throw(-1, "syntax error in transform attribute - no keyword");

        while (svg_is_whitespace(*str))
            str ++;

        if (*str != '(')
            return gs_throw(-1, "syntax error in transform attribute - no open paren");
        str ++;

        while (svg_is_whitespace(*str))
            str ++;

        /*
         * Parse list of numbers until closing parenthesis
         */

        while (nargs < 6)
        {
            numberlen = 0;

            while (svg_is_digit(*str) && numberlen < sizeof(number) - 1)
                number[numberlen++] = *str++;
            number[numberlen] = 0;

            args[nargs++] = atof(number);

            while (svg_is_whitespace_or_comma(*str))
                str ++;

            if (*str == ')')
            {
                str ++;
                break;
            }

            if (*str == 0)
                return gs_throw(-1, "syntax error in transform attribute - no close paren");
        }

        while (svg_is_whitespace_or_comma(*str))
            str ++;

        /*
         * Execute the transform.
         */

        if (!strcmp(keyword, "matrix"))
        {
            gs_matrix mtx;

            if (nargs != 6)
                return gs_throw1(-1, "wrong number of arguments to matrix(): %d", nargs);

            mtx.xx = args[0];
            mtx.xy = args[1];
            mtx.yx = args[2];
            mtx.yy = args[3];
            mtx.tx = args[4];
            mtx.ty = args[5];

            gs_concat(ctx->pgs, &mtx);
        }

        else if (!strcmp(keyword, "translate"))
        {
            if (nargs != 2)
                return gs_throw1(-1, "wrong number of arguments to translate(): %d", nargs);
            gs_translate(ctx->pgs, args[0], args[1]);
        }

        else if (!strcmp(keyword, "scale"))
        {
            if (nargs == 1)
                gs_scale(ctx->pgs, args[0], args[0]);
            else if (nargs == 2)
                gs_scale(ctx->pgs, args[0], args[1]);
            else
                return gs_throw1(-1, "wrong number of arguments to scale(): %d", nargs);
        }

        else if (!strcmp(keyword, "rotate"))
        {
            if (nargs != 1)
                return gs_throw1(-1, "wrong number of arguments to rotate(): %d", nargs);
            gs_rotate(ctx->pgs, args[0]);
        }

        else if (!strcmp(keyword, "skewX"))
        {
            gs_matrix mtx;

            if (nargs != 1)
                return gs_throw1(-1, "wrong number of arguments to skewX(): %d", nargs);

            mtx.xx = 1.0;
            mtx.xy = 0.0;
            mtx.yx = tan(args[0] * 0.0174532925);
            mtx.yy = 1.0;
            mtx.tx = 0.0;
            mtx.ty = 0.0;

            gs_concat(ctx->pgs, &mtx);
        }

        else if (!strcmp(keyword, "skewY"))
        {
            gs_matrix mtx;

            if (nargs != 1)
                return gs_throw1(-1, "wrong number of arguments to skewY(): %d", nargs);

            mtx.xx = 1.0;
            mtx.xy = tan(args[0] * 0.0174532925);
            mtx.yx = 0.0;
            mtx.yy = 1.0;
            mtx.tx = 0.0;
            mtx.ty = 0.0;

            gs_concat(ctx->pgs, &mtx);
        }

        else
        {
            return gs_throw1(-1, "unknown transform function: %d", keyword);
        }
    }

    return 0;
}
