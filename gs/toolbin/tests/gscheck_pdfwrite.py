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
# compares Ghostscript against a baseline made from file->pdf->raster->md5sum.
# this test tries to detect Ghostscript changes that affect the pdfwrite driver.

import os
import string
import gstestutils
import gsconf, gstestgs, gsparamsets, gssum


class GSPDFWriteCompareTestCase(gstestgs.GhostscriptTestCase):
    def shortDescription(self):
        if self.band:
	    return "Checking pdfwrite of %s (%s/%ddpi/banded) against baseline" % (self.file[string.rindex(self.file, '/') + 1:], self.device, self.dpi)
	else:
	    return "Checking pdfwrite of %s (%s/%ddpi/noband) against baseline" % (self.file[string.rindex(self.file, '/') + 1:], self.device, self.dpi)
	
    def runTest(self):
        file1 = '%s.%s.%d.%d.pdf' % (self.file[string.rindex(self.file, '/') + 1:], 'pdf', self.dpi, self.band)
	file2 = '%s.pdf.%s.%d.%d' % (self.file[string.rindex(self.file, '/') + 1:], self.device, self.dpi, self.band)

	gs = gstestgs.Ghostscript()
	gs.command = self.gs
	gs.dpi = self.dpi
	gs.band = self.band
	gs.infile = self.file
	gs.gsoptions = self.gsoptions
	if self.log_stdout:
	    gs.log_stdout = self.log_stdout
	if self.log_stderr:
	    gs.log_stderr = self.log_stderr

	# do file->PDF conversion

	gs.device = 'pdfwrite'
	gs.outfile = file1
	if not gs.process():
	    self.fail("non-zero exit code trying to create pdf file from " + self.file)

	# do PDF->device (pbmraw, pgmraw, ppmraw, pkmraw)
		
	gs.device = self.device
	gs.infile = file1
	gs.outfile = file2
	if not gs.process():
	    self.fail("non-zero exit code trying to"\
		      " rasterize " + file1)

	# compare baseline
		
	sum = gssum.make_sum(file2)
	os.unlink(file1)
	os.unlink(file2)
	
	# add test result to daily database
	if self.track_daily:
	    gssum.add_file(file2, dbname=gsconf.dailydb, sum=sum)

	self.assertEqual(sum, gssum.get_sum(file2), "md5sum did not match baseline (" + file2 + ") for file: " + self.file)

# Add the tests defined in this file to a suite

def add_compare_test(suite, f, device, dpi, band):
    suite.addTest(GSPDFWriteCompareTestCase(gs=gsconf.comparegs,
					    file=gsconf.comparefiledir + f,
					    device=device, dpi=dpi,
					    band=band, track_daily=1,
					    log_stdout=gsconf.log_stdout,
					    log_stderr=gsconf.log_stderr))

def addTests(suite, gsroot, **args):
    # get a list of test files
    comparefiles = os.listdir(gsconf.comparefiledir)

    for f in comparefiles:
        if f[-3:] == '.ps' or f[-4:] == '.pdf' or f[-4:] == '.eps':
	    for params in gsparamsets.pdftestparamsets:
	        add_compare_test(suite, f, params.device,
				 params.resolution,
				 params.banding)

if __name__ == "__main__":
    gstestutils.gsRunTestsMain(addTests)
