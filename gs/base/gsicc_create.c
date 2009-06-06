/* Copyright (C) 2001-2009 Artifex Software, Inc.
   All Rights Reserved.
  
   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
*/



/* This is the code that is used to convert the various PDF and PS CIE
   based color spaces to ICC profiles.  This enables the use of an
   external CMS that is ICC centric to be used for ALL color management.

   The following spaces are handled:

   From PDF

   % Input Spaces

   CalRGB      -->  ICC 1-D LUTS and Matrix
   CalGray     -->  ICC 1-D LUT
   LAB         -->  ICC MLUT with a 2x2 sized table 

   From PS

   %% Input Spaces

   CIEBasedABC  --> ICC 1-D LUTs and Matrix
   CIEBasedA    --> ICC 1-D LUT
   CIEBasedDEF  --> 3-D MLUT plus 1-D LUTs
   CIEBasedDEFG --> 4-D MLUT pluse 1-D LUTs

   %% Output Spaces

   Type1 CRD -->  ICC will have MLUT if render table present.  
   
   */

#include "icc34.h"
#include "string.h"


static void
setdatetime(icDateTimeNumber *datetime)
{

    datetime->day = 0;
    datetime->hours = 0;
    datetime->minutes = 0;
    datetime->month = 0;
    datetime->seconds = 0;
    datetime->year = 0;

}

static icS15Fixed16Number
double2XYZtype(double number_in){

    short s;
    unsigned short m;

    s = (short) number_in;
    m = (unsigned short) ((number_in - s) * 65536.0);
    return((icS15Fixed16Number) ((s << 16) | m) );
}

static void
setheader_common(icHeader *header)
{
    /* This needs to all be predefined for a simple copy. MJV todo */
    header->cmmId = 0;
    header->version = 0x03400000;  /* Back during a simplier time.... */
    setdatetime(&(header->date));
    header->magic = 0x61637379;
    header->platform = icSigMacintosh;
    header->flags = 0;
    header->manufacturer = 0;
    header->model = 0;
    header->attributes[0] = 0;
    header->attributes[1] = 0;
    header->renderingIntent = 0;
    header->illuminant.X = double2XYZtype(0.9642);
    header->illuminant.Y = double2XYZtype(1.0);
    header->illuminant.Z = double2XYZtype(0.8249);
    header->creator = 0;
    memset(header->reserved,0,44);  /* Version 4 includes a profile id field which is an md5 sum */

}


void
gsicc_create_fromabc(unsigned char *buffer)
{

    icProfile iccprofile;
    icHeader  *header = &(iccprofile.header);
    int size_count;

    setheader_common(header);

    /* This needs to all be predefined for a simple copy. MJV todo */

    header->deviceClass = icSigDisplayClass;
    header->colorSpace = icSigRgbData;
    header->pcs = icSigXYZData;

    size_count = 128; /* Header size */

    /* Now lets add the tags */




    header->size = size_count;



}

void
gsicc_create_fromdef(unsigned char *buffer)
{

 



}


void
gsicc_create_fromdefg(unsigned char *buffer)
{

 



}


void
gsicc_create_froma(unsigned char *buffer)
{

 



}


void
gsicc_create_fromcrd(unsigned char *buffer)
{

 



}