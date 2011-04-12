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


This is a windows application for creating ICC profiles.  It has multiple uses 
described below.

The Device N profiles controls can be used to accurately define
color for PDF and PS DeviceN color spaces.  To use this tool, you will need 
to have CIELAB measurements of M^N equally spaced in device values from min 
to max inkoverprinted spot colors (or spot and process colors).  See the example 
folder for sample input when we have N=2 colors with M=6 samples.  You will also 
need a file that has the  names of the colorants as they are used for the 
DeviceN color space in the document.  The order of the names must be related to 
the rate of change of the CIELAB data in the hypercube of measurements.  The 
first name relates to the device value that changes more slowly in the CIELAB 
data, while the last name relates to the device value that changes more rapidly 
in the CIELAB data.  To include the DeviceN ICC profiles in ghostscript when 
processing a PDF or PS file that includes those spaces, use 
-sDeviceNProfile="5channel.icc; 6channel.icc; duotone.icc". 

The DeviceLink Profiles controls enable one to generate specialized linked ICC 
profiles for testing purposes.  This is not really a user feature at this time.

The Thresholding Profiles controls allow one to generate ICC profiles that will 
threshold  the output value for the device gray profile.  The threshold range is 
based upon L*.  Values with an L* above the threshold value will map to white 
those below will map to black.

Default Profile Generation is used to create ICC profiles that can emulate the 
old PS color mapping routines with a specified UCR/BG operation.  This is 
achieved by the user first creating a table of values that define the desired 
relationship between RGB and CMYK.  The data should be in tab delimited
text form.  The range of values is 0 to 255 and an example set of data is given 
in the file table_data.txt where we have a case that has no UCR or BG.
Note that in this table of data, the UCR is explicit in the data by
UCR_C = 255 - R - C
UCR_M = 255 - G - M
UCR_Y = 255 - B - Y

Given the above mapping, the other mappings are implemented using:

Gray -> RGB is implemented as Gray -> R = G = B. 
 
RGB -> Gray is implemented using Gray = 0.30 R + 0.59 G + 0.11 B 

CMYK -> RGB is implemented using 
R = (1.0 - C) * (1.0 - K)
G = (1.0 - M) * (1.0 - K)
B = (1.0 - Y) * (1.0 - K)
for CPSI mode
or 
R =  0 if C > (1-K) else  R = (1-K) - C
G =  0 if M > (1-K) else  R = (1-K) - M
B =  0 if Y > (1-K) else  R = (1-K) - Y
for the non CPSI case

CMYK -> Gray is implemented using CMYK -> RGB -> Gray

Gray -> CMYK is implemented using
cmy_gray = rgb_gray ((1.0 - C), (1.0 - M), (1.0 - Y))
if cmy_gray > 1 - K then gray = 0
else gray = 1 - (cmy_gray + K)



