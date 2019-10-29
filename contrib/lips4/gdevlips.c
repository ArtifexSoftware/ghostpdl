/* Copyright (C) 1998, 1999 Norihito Ohmori.

   Ghostscript printer driver
   for Canon LBP and BJ printers (LIPS II+/III/IVc/IV)

   This software is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY.  No author or distributor accepts responsibility
   to anyone for the consequences of using it or for whether it serves any
   particular purpose or works at all, unless he says so in writing.  Refer
   to the GNU General Public License for full details.

   Everyone is granted permission to copy, modify and redistribute
   this software, but only under the conditions described in the GNU
   General Public License.  A copy of this license is supposed to have been
   given to you along with this software so you can know your rights and
   responsibilities.  It should be in a file named COPYING.  Among other
   things, the copyright notice and this notice must be preserved on all
   copies.
 */

/*$Id: gdevlips.c,v 1.3 2002/07/20 21:03:21 tillkamppeter Exp $ */
/* Common Utility for LIPS driver */

#include "gx.h"
#include "gdevlips.h"

typedef struct {
    int width;
    int height;
    int num_unit;
} paper_table;

static paper_table lips_paper_table[] =
{
    {842, 1190, 12},		/* A3 */
    {595, 842, 14},		/* A4 */
    {597, 842, 14},		/* A4     8.3 inch x 11.7 inch */
    {421, 595, 16},		/* A5 */
    {284, 419, 18},		/* PostCard */
    {729, 1032, 24},		/* JIS B4 */
    {516, 729, 26},		/* JIS B5 */
    {363, 516, 28},		/* JIS B6 */
    {612, 792, 30},		/* Letter */
    {612, 1008, 32},		/* Legal */
    {792, 1224, 34},		/* Ledger */
    {522, 756, 40},		/* Executive */
    {298, 666, 50},		/* envyou4 */
    {0, 0, 80},			/* User Size */
};

int
lips_media_selection(int width, int height)
{
    int landscape = 0;
    int tmp;
    paper_table *pt;

    if (width > height) {
        landscape = 1;
        tmp = width;
        width = height;
        height = tmp;
    }
    for (pt = lips_paper_table; pt->num_unit < 80; pt++)
        if (pt->width == width && pt->height == height)
            break;

    return pt->num_unit + landscape;
}

static int GetNumSameData(const byte * curPtr, const int maxnum);
static int GetNumWrongData(const byte * curPtr, const int maxnum);

/* This routine talkes from LipsIVRunLengthEncode in Yoshiharu ITO's patch  */
int
lips_packbits_encode(byte * inBuff, byte * outBuff, int Length)
{
    int size = 0;

    while (Length) {
        int count;

        if (1 < (count = GetNumSameData(inBuff,
                                        Length > 128 ? 128 : Length))) {
            Length -= count;
            size += 2;

            *outBuff++ = -(count - 1);
            *outBuff++ = *inBuff;
            inBuff += count;
        } else {
            count = GetNumWrongData(inBuff, Length > 128 ? 128 : Length);
            Length -= count;
            size += count + 1;

            *outBuff++ = count - 1;
            while (count--) {
                *outBuff++ = *inBuff++;
            }
        }
    }

    return (size);
}

/* LIPS III printer can no use Runlength encode mode
   but Mode 1-3 Format mode. */
/* This routine takes from LipsIIIRunLengthEncode in Yoshiharu ITO's patch  */
int
lips_mode3format_encode(byte * inBuff, byte * outBuff, int Length)
{
    int size = 0;

    while (Length) {
        int count;

        if (1 < (count = GetNumSameData(inBuff,
                                        Length > 257 ? 257 : Length))) {
            Length -= count;
            size += 3;

            *outBuff++ = *inBuff;
            *outBuff++ = *inBuff;
            *outBuff++ = count - 2;
            inBuff += count;
        } else {
            count = GetNumWrongData(inBuff, Length);
            Length -= count;
            size += count;

            while (count--) {
                *outBuff++ = *inBuff++;
            }
        }
    }

    return (size);
}

static int
GetNumSameData(const byte * curPtr, const int maxnum)
{
    int count = 1;

    if (1 == maxnum) {
        return (1);
    }
    while (maxnum > count && *curPtr == *(curPtr + count)) {
        count++;
    }

    return (count);
}

static int
GetNumWrongData(const byte * curPtr, const int maxnum)
{
    int count = 0;

    if (1 == maxnum) {
        return (1);
    }
    while (maxnum > count+1 && *(curPtr + count) != *(curPtr + count + 1)) {
        count++;
    }

    return (count);
}

/*

   This routine takes from gdevlips4-1.1.0.

 */
#define RLECOUNTMAX 0xff	/* 256 times */
int
lips_rle_encode(byte * inBuff, byte * outBuff, int Length)
{
    int i = 0;
    byte value;
    int count = 0;
    byte *ptr = inBuff;

    value = *ptr;
    ptr++;

    while (ptr < inBuff + Length) {
        if (*ptr == value) {
            count++;
            if (count > RLECOUNTMAX) {
                *outBuff++ = RLECOUNTMAX;
                *outBuff++ = value;
                i += 2;
                count = 0;
            }
        } else {
            *outBuff++ = count;
            *outBuff++ = value;
            i += 2;
            count = 0;
            value = *ptr;
        }
        ptr++;
    }
    *outBuff++ = count;
    *outBuff++ = value;
    i += 2;

    return i;
}
