#include "ghostxps.h"

void
xps_parse_color(xps_context_t *ctx, char *hexstring, float *argb)
{
    const char *hextable = "0123456789ABCDEF";
    char *hexstringp = hexstring;

#define HEXTABLEINDEX(chr) (strchr(hextable, (toupper(chr))) - hextable)

    /* nb need to look into color specification */
    if (strlen(hexstring) == 9)
    {
	argb[0] = HEXTABLEINDEX(hexstringp[1]) * 16 + HEXTABLEINDEX(hexstringp[2]);
	argb[1] = HEXTABLEINDEX(hexstringp[3]) * 16 + HEXTABLEINDEX(hexstringp[4]);
	argb[2] = HEXTABLEINDEX(hexstringp[5]) * 16 + HEXTABLEINDEX(hexstringp[6]);
	argb[3] = HEXTABLEINDEX(hexstringp[7]) * 16 + HEXTABLEINDEX(hexstringp[8]);
    }
    else
    {
	argb[0] = 255.0;
	argb[1] = HEXTABLEINDEX(hexstringp[1]) * 16 + HEXTABLEINDEX(hexstringp[2]);
	argb[2] = HEXTABLEINDEX(hexstringp[3]) * 16 + HEXTABLEINDEX(hexstringp[4]);
	argb[3] = HEXTABLEINDEX(hexstringp[5]) * 16 + HEXTABLEINDEX(hexstringp[6]);
    }

    argb[0] /= 255.0;
    argb[1] /= 255.0;
    argb[2] /= 255.0;
    argb[3] /= 255.0;
}

