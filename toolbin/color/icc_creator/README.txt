  Copyright (C) 2001-2019 Artifex Software, Inc.
  All Rights Reserved.

  This software is provided AS-IS with no warranty, either express or
  implied.

  This software is distributed under license and may not be copied,
  modified or distributed except as expressly authorized under the terms
  of the license contained in the file LICENSE in this distribution.

  Refer to licensing information at http://www.artifex.com or contact
  Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
  CA 94945, U.S.A., +1(415)492-9861, for further information.

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

The Thresholding Profiles controls allow one to generate ICC profiles that will 
threshold  the output value for the device gray profile.  The threshold range is 
based upon L*.  Values with an L* above the threshold value will map to white 
those below will map to black.

Default Profile Generation is used to create ICC profiles that can emulate the 
old PS color mapping routines.  The mapping to CMYK requires the specification
of a particular undercolor and black generation mappings (UCR/BG).  The data is 
specified by the user in a table of tab delimited values.  The range of values
is 0 to 255 and example sets of data are given in the files ucr_bg_no_k.txt and
ucr_bg_max_k.  The CMYK ICC profiles generated from these data are given in
no_k.icc and max_k.icc.  The max_k.icc profile may be useful in generating black
only text output from neutral source data. This is achieved using ghostscript with
the option -sTextICCProfile=max_k.icc (where max_k.icc is included in ./iccprofiles
and ghostscript is built with the ROM file system).   


