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
import string
import gstestutils
import gsconf, gstestgs, gsparamsets


def fuzzy_compare(file1, file2, tolerance=2, windowsize=5):
	cmd = gsconf.fuzzy + ' -w%d -t%d %s %s > /dev/null 2> /dev/null' % (windowsize, tolerance, file1, file2)

	ret = os.system(cmd)
	if ret == 0:
		return 1
	else:
		return 0
		
class GSFuzzyCompareTestCase(gstestgs.GhostscriptTestCase):
	def shortDescription(self):
		return "Doing pdfwrite fuzzy test of %s (%s/%d/%d)" % (self.file[string.rindex(self.file, '/') + 1:], self.device, self.dpi, self.band)
	
	def runTest(self):
		file1 = '%s.%s.%d.%d' % (self.file, self.device, self.dpi, self.band)
		file2 = '%s.%s.%d.%d.pdf' % (self.file, 'pdfwrite', self.dpi, self.band)
		file3 = '%s.pdfwrite.%s.%d.%d' % (self.file, self.device, self.dpi, self.band)

		gs = gstestgs.Ghostscript()
		gs.command = self.gs
		gs.dpi = self.dpi
		gs.band = self.band
		gs.infile = self.file
		gs.device = self.device

		# do PostScript->device (pbmraw, pgmraw, ppmraw, pkmraw)

		gs.outfile = file1
		gs.process()

		# do PostScript->pdfwrite
		
		gs.device = 'pdfwrite'
		gs.outfile = file2
		gs.process()

		# do PDF->device (pbmraw, pgmraw, ppmraw, pkmraw)
		
		gs.device = self.device
		gs.infile = file2
		gs.outfile = file3
		gs.process()

		# fuzzy compare PostScript->device with PostScript->PDF->device
		
		ret = fuzzy_compare(file1, file3)
		os.unlink(file1)
		os.unlink(file2)
		os.unlink(file3)
		self.assert_(ret, "fuzzy match failed")

# Add the tests defined in this file to a suite

def add_compare_test(suite, f, device, dpi, band):
        suite.addTest(GSFuzzyCompareTestCase(gs=gsconf.comparegs, file=gsconf.comparefiledir + f, device=device, dpi=dpi, band=band))

def addTests(suite, gsroot, **args):
	# get a list of test files
	comparefiles = os.listdir(gsconf.comparefiledir)


	for f in comparefiles:
		if f[-3:] == '.ps':
			for params in gsparamsets.testparamsets:
				add_compare_test(suite, f, params.device, params.resolution, params.banding)

if __name__ == "__main__":
	gstestutils.gsRunTestsMain(addTests)
