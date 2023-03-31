% Copyright (C) 2001-2023 Artifex Software, Inc.
%   All Rights Reserved.
%
%   This software is provided AS-IS with no warranty, either express or
%   implied.
%
%   This software is distributed under license and may not be copied,
%   modified or distributed except as expressly authorized under the terms
%   of the license contained in the file LICENSE in this distribution.
%
%   Refer to licensing information at http://www.artifex.com or contact
%   Artifex Software, Inc.,  39 Mesa Street, Suite 108A, San Francisco,
%   CA 94129, USA, for further information.
%
%
% Paths in this example are relative to the MATLAB
% folder in the ghostscript project.  MATLAB only
% runs on Windows 64 bit machines. Interface is
% limited to a subset of the API methods due to
% the fact that the MATLAB API does not allow
% function pointers.  We want to use the display
% device callbacks to get the image data created
% from Ghostscript.  To do this, we have to wrap the
% callback set up and associated functions into a
% MEX-file that will serve as an interface between MATLAB
% and the Ghostscript DLL. We use the C MEX API (as opposed to
% the C++ MEX API).  The mex file is gs_displaydevice.c

% The creation of the mex file for the display device handling only
% needs to be done once. Note the use of -R2018a as we are using
% type-safe data access in the mex file. This also is doing a debug version -g.
mex 'gs_displaydevice.c' -R2018a -g '../../debugbin/gpdldll64.lib'

% You have to load the library to get the mex file to find the library and
% to make direct calls to gpdldll64 directly from MATLAB
if not(libisloaded('gpdldll64'))
    [nf,warn] = loadlibrary('../../debugbin/gpdldll64.dll','../../pcl/pl/plapi.h')
end

% Show us the various methods in the DLL
% libfunctions('gpdldll64')

% Use planar format for MATLAB. See gdevdsp.h for what these bits mean.
PlanarGray = 0x800802;
PlanarRGB = 0x800804;
PlanarCMYK = 0x800808;
PlanarSpots = 0x880800;

% Let try the display device and return the image
page_number = 1;
resolution = 200;
input_file = '../../examples/tiger.eps';
tiger_image_rgb = gs_displaydevice(input_file, PlanarRGB, page_number, resolution);
figure(1);
imshow(tiger_image_rgb);
title('RGB rendering');

tiger_image_gray = gs_displaydevice(input_file, PlanarGray, page_number, resolution);
figure(2);
imshow(tiger_image_gray);
title('Gray rendering');

% MATLAB will not display CMYK or NColor Images.  We have to show
% the separations for this case
tiger_image_cmyk = gs_displaydevice(input_file, PlanarCMYK, page_number, resolution);
for k=1:4
    eval(sprintf('figure(2+k);'));
    imshow(tiger_image_cmyk(:,:,k));
    switch k
        case 1
            title('Cyan Separation');
        case 2
            title('Magenta Separation');
        case 3
            title('Yellow Separation');
        case 4
            title('Black Separation');
    end
end

% At this stage, you can push the image data through MATLAB's ocr if you
% have the Computer Vision Toolbox and want to do some sort of text
% analysis.  You can also use direct calls into gpdldll64.  See the
% C-API demo for examples on what can be done to render to file output or
% save as PDF, PS, etc.