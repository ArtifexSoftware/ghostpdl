#    Copyright (C) 2001 Artifex Software Inc.
# 
# This software is provided AS-IS with no warranty, either express or
# implied.
# 
# This software is distributed under license and may not be copied,
# modified or distributed except as expressly authorized under the terms
# of the license contained in the file LICENSE in this distribution.
# 
# For more information about licensing, please refer to
# http://www.ghostscript.com/licensing/. For information on
# commercial licensing, go to http://www.artifex.com/licensing/ or
# contact Artifex Software, Inc., 101 Lucas Valley Road #110,
# San Rafael, CA  94903, U.S.A., +1(415)492-9861.

# $RCSfile$ $Revision$

# gsparamsets.py
#
# Parameter sets for regression testing with gs

class _GSParamSet:

    def __init__(self, device, resolution, banding):
        self.device = device
        self.resolution = resolution
        self.banding = banding

testparamsets = [

    # mono
    _GSParamSet('pbmraw', 72, 0),
    _GSParamSet('pbmraw', 72, 1),
    _GSParamSet('pbmraw', 300, 0),
    _GSParamSet('pbmraw', 300, 1),
    #_GSParamSet('pbmraw', 600, 0),
    #_GSParamSet('pbmraw', 600, 1),

    # grayscale
    _GSParamSet('pgmraw', 72, 0),
    _GSParamSet('pgmraw', 72, 1),
    _GSParamSet('pgmraw', 300, 0),
    _GSParamSet('pgmraw', 300, 1),
    #_GSParamSet('pgmraw', 600, 0),
    #_GSParamSet('pgmraw', 600, 1),

    # color
    _GSParamSet('ppmraw', 72, 0),
    _GSParamSet('ppmraw', 72, 1),
    _GSParamSet('ppmraw', 300, 0),
    _GSParamSet('ppmraw', 300, 1),
    #_GSParamSet('ppmraw', 600, 0),
    #_GSParamSet('ppmraw', 600, 1),

    # 1-bit CMYK
    _GSParamSet('pkmraw', 72, 0),
    _GSParamSet('pkmraw', 72, 1),
    _GSParamSet('pkmraw', 300, 0),
    _GSParamSet('pkmraw', 300, 1),
    #_GSParamSet('pkmraw', 600, 0),
    #_GSParamSet('pkmraw', 600, 1)

]
