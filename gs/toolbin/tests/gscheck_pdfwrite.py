#!/usr/bin/env python2.1

#    Copyright (C) 2001 Artifex Software Inc.
# 
# This file is part of AFPL Ghostscript.
# 
# AFPL Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author or
# distributor accepts any responsibility for the consequences of using it, or
# for whether it serves any particular purpose or works at all, unless he or
# she says so in writing.  Refer to the Aladdin Free Public License (the
# "License") for full details.
# 
# Every copy of AFPL Ghostscript must include a copy of the License, normally
# in a plain ASCII text file named PUBLIC.  The License grants you the right
# to copy, modify and redistribute AFPL Ghostscript, but only under certain
# conditions described in the License.  Among other things, the License
# requires that the copyright notice and this notice be preserved on all
# copies.

# $Id$

#
# gscheck_pdfwrite.py
#
# does a fuzzy compare against the output of ps->raster and ps->pdf->raster
# to make sure that the pdfwrite device is functioning as expected

import os
import gstestutils
import gsconf, gstestgs, gsparamsets

# get a list of test files
comparefiles = os.listdir(gsconf.comparefiledir)

suite = gstestutils.GSTestSuite()

# add compare tests
def add_compare_test(suite, f, device, dpi, band):
        suite.addTest(gstestgs.GSFuzzyCompareTestCase(gs=gsconf.comparegs,
                                               file=gsconf.comparefiledir + f,
                                          device=device, dpi=dpi, band=band))

for f in comparefiles:
    if f[-3:] == '.ps':
	for params in gsparamsets.testparamsets:
            add_compare_test(suite, f, params.device, params.resolution, params.banding)

# run all the tests
runner = gstestutils.GSTestRunner(verbosity=2)
result = runner.run(suite)

