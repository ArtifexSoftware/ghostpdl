# Copyright (C) 2001-2023 Artifex Software, Inc.
# All Rights Reserved.
#
# This software is provided AS-IS with no warranty, either express or
# implied.
#
# This software is distributed under license and may not be copied,
# modified or distributed except as expressly authorized under the terms
# of the license contained in the file LICENSE in this distribution.
#
# Refer to licensing information at http://www.artifex.com or contact
# Artifex Software, Inc.,  39 Mesa Street, Suite 108A, San Francisco,
# CA 94129, USA, for further information.
#


# gsparamsets.py
#
# Parameter sets for regression testing with gs

class _GSParamSet:
    def __init__(self, device, resolution, banding):
        self.device = device
        self.resolution = resolution
        self.banding = banding
 
testparamsets_name = ""

testparamsets_maximum = [
    # mono
    _GSParamSet('pbmraw', 72, 0),
    _GSParamSet('pbmraw', 72, 1),
    _GSParamSet('pbmraw', 300, 0),
    _GSParamSet('pbmraw', 300, 1),
#   _GSParamSet('pbmraw', 600, 0),
#   _GSParamSet('pbmraw', 600, 1),

    # grayscale
    _GSParamSet('pgmraw', 72, 0),
    _GSParamSet('pgmraw', 72, 1),
    _GSParamSet('pgmraw', 300, 0),
    _GSParamSet('pgmraw', 300, 1),
#   _GSParamSet('pgmraw', 600, 0),
#   _GSParamSet('pgmraw', 600, 1),

    # color
    _GSParamSet('ppmraw', 72, 0),
    _GSParamSet('ppmraw', 72, 1),
    _GSParamSet('ppmraw', 300, 0),
    _GSParamSet('ppmraw', 300, 1),
#   _GSParamSet('ppmraw', 600, 0),
#   _GSParamSet('ppmraw', 600, 1),

    # 1-bit CMYK
    _GSParamSet('pkmraw', 72, 0),
    _GSParamSet('pkmraw', 72, 1),
    _GSParamSet('pkmraw', 300, 0),
    _GSParamSet('pkmraw', 300, 1),
#   _GSParamSet('pkmraw', 600, 0),
#   _GSParamSet('pkmraw', 600, 1)
]

testparamsets_full = [
    # mono
    _GSParamSet('pbmraw', 72, 0),
    _GSParamSet('pbmraw', 300, 0),
    _GSParamSet('pbmraw', 300, 1),

    # grayscale
    _GSParamSet('pgmraw', 72, 0),
    _GSParamSet('pgmraw', 300, 0),
    _GSParamSet('pgmraw', 300, 1),

    # color
    _GSParamSet('ppmraw', 72, 0),
    _GSParamSet('ppmraw', 300, 0),
    _GSParamSet('ppmraw', 300, 1),

    # 1-bit CMYK
    _GSParamSet('pkmraw', 72, 0),
    _GSParamSet('pkmraw', 300, 0),
    _GSParamSet('pkmraw', 300, 1),
]
testparamsets_minimum = [
    # mono
    _GSParamSet('pbmraw', 72, 0),
    _GSParamSet('pbmraw', 300, 0),
    _GSParamSet('pgmraw', 300, 0),
    _GSParamSet('ppmraw', 300, 0),
]

testparamsets_one = [
    # mono
    _GSParamSet('pbmraw', 300, 0),
]

pdftestparamsets_full = [
    # color
    _GSParamSet('ppmraw', 72, 0),
    _GSParamSet('ppmraw', 300, 0),
    
    # 1-bit CMYK
    _GSParamSet('pkmraw', 300, 0)
]

pdftestparamsets_minimum = [
    # color
    _GSParamSet('ppmraw', 300, 0),
]

pdftestparamsets_maximum = pdftestparamsets_full

testparamsets = testparamsets_minimum
pdftestparamsets = pdftestparamsets_minimum
testparamsets_name = "minimum"

testparamsets = testparamsets_maximum
pdftestparamsets = pdftestparamsets_maximum
testparamsets_name = "maximum"

testparamsets = testparamsets_one
pdftestparamsets = pdftestparamsets_minimum
testparamsets_name = "one"

testparamsets = testparamsets_full
pdftestparamsets = pdftestparamsets_minimum
testparamsets_name = "full"

