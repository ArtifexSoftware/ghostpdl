/* Copyright (C) 2001-2006 Artifex Software, Inc.
   All Rights Reserved.
  
   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

This is a Windows application for creating source ICC profiles that can be used to accurately define
color for PDF and PS DeviceN color spaces.  To use this tool, you will need to have
CIELAB measurements of M^N equally spaced in device values from min to max ink
overprinted spot colors (or spot and process colors).  See the example folder for sample
input when we have N=2 colors with M=6 samples.  You will also need a file that has the 
names of the colorants as they are used for the DeviceN color space in the document.  The
order of the names must be related to the rate of change of the CIELAB data in the 
hypercube of measurements.  The first name relates to the device value that
changes more slowly in the CIELAB data, while the last name relates to the device value
that changes more rapidly in the CIELAB data.  To include the DeviceN ICC profiles in ghostscript
when processing a PDF or PS file that includes those spaces, use -sDeviceNProfile="5channel.icc; 6channel.icc; duotone.icc". 

  
