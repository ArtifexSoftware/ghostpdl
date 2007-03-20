#include "ghostxps.h"

/*
 * http://www.cl.cam.ac.uk/~mgk25/ucs/utf-8-history.txt
 */

#define OFF1   0x0000080
#define OFF2   0x0002080
#define OFF3   0x0082080
#define OFF4   0x2082080

int xps_ucs_to_utf8(char *s, int wc)
{
    if (s == 0)
	return 0;       /* no shift states */

    if (wc < 0)
	return -1;

    if (wc <= 0x7f)         /* fits in 7 bits */
    {
	s[0] = wc;
	return 1;
    }

    if (wc <= 0x1fff + OFF1)        /* fits in 13 bits */
    {
	wc -= OFF1;
	s[0] = 0x80 | (wc >> 7);
	s[1] = 0x80 | (wc & 0x7f);
	return 2;
    }

    if (wc <= 0x7ffff + OFF2)       /* fits in 19 bits */
    {
	wc -= OFF2;
	s[0] = 0xc0 | (wc >> 14);
	s[1] = 0x80 | ((wc >> 7) & 0x7f);
	s[2] = 0x80 | (wc & 0x7f);
	return 3;
    }

    if (wc <= 0x1ffffff + OFF3)     /* fits in 25 bits */
    {
	wc -= OFF3;
	s[0] = 0xe0 | (wc >> 21);
	s[1] = 0x80 | ((wc >> 14) & 0x7f);
	s[2] = 0x80 | ((wc >> 7) & 0x7f);
	s[3] = 0x80 | (wc & 0x7f);
	return 4;
    }

    wc -= OFF4;
    s[0] = 0xf0 | (wc >> 28);
    s[1] = 0x80 | ((wc >> 21) & 0x7f);
    s[2] = 0x80 | ((wc >> 14) & 0x7f);
    s[3] = 0x80 | ((wc >> 7) & 0x7f);
    s[4] = 0x80 | (wc & 0x7f);
    return 5;
}

int xps_utf8_to_ucs(int *p, const char *s, int n)
{
    unsigned char *uc;      /* so that all bytes are nonnegative */

    uc = (unsigned char *)s;

    if (n == 0)
	return 0;

    if ((*p = uc[0]) < 0x80)
	return uc[0] != '\0';   /* return 0 for '\0', else 1 */

    if (uc[0] < 0xc0)
    {
	if (n < 2)
	    return -1;
	if (uc[1] < 0x80)
	    return -1;
	*p &= 0x3f;
	*p <<= 7;
	*p |= uc[1] & 0x7f;
	*p += OFF1;
	return 2;
    }

    if (uc[0] < 0xe0)
    {
	if (n < 3)
	    return -1;
	if (uc[1] < 0x80 || uc[2] < 0x80)
	    return -1;
	*p &= 0x1f;
	*p <<= 14;
	*p |= (uc[1] & 0x7f) << 7;
	*p |= uc[2] & 0x7f;
	*p += OFF2;
	return 3;
    }

    if (uc[0] < 0xf0)
    {
	if (n < 4)
	    return -1;
	if (uc[1] < 0x80 || uc[2] < 0x80 || uc[3] < 0x80)
	    return -1;
	*p &= 0x0f;
	*p <<= 21;
	*p |= (uc[1] & 0x7f) << 14;
	*p |= (uc[2] & 0x7f) << 7;
	*p |= uc[3] & 0x7f;
	*p += OFF3;
	return 4;
    }

    if (uc[0] < 0xf8)
    {
	if (n < 5)
	    return -1;
	if (uc[1] < 0x80 || uc[2] < 0x80 || uc[3] < 0x80 || uc[4] < 0x80)
	    return -1;
	*p &= 0x07;
	*p <<= 28;
	*p |= (uc[1] & 0x7f) << 21;
	*p |= (uc[2] & 0x7f) << 14;
	*p |= (uc[3] & 0x7f) << 7;
	*p |= uc[4] & 0x7f;
	if (((*p += OFF4) & ~0x7fffffff) == 0)
	    return 5;
    }

    return -1;
}

