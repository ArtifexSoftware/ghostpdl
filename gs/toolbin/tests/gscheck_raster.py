#!/usr/bin/env python

#    Copyright (C) 2001-2007 Artifex Software Inc.
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

# $Id$

#
# gscheck_raster.py
#
# rasterizes input files in several configurations and checks them
# against known baselines
#


import os
import string, calendar, time
import gstestutils
import gssum, gsconf, gstestgs, gsparamsets, gsutil
import rasterdb

class GSCompareTestCase(gstestgs.GhostscriptTestCase):
    def makefilename(self):
        file = "%s.%s.%d.%d" % (self.file[string.rindex(self.file, '/') + 1:], self.device, self.dpi, self.band)
	return file

    def shortDescription(self):
	file = self.makefilename()
	if not rasterdb.exists(file):
		os.system(gsconf.codedir + "update_baseline '%s'" %
                          (os.path.basename(self.file),))
		self.skip = 1
	try:
		ct = time.localtime(rasterdb.mtime(file))
		baseline_date = "%s %d, %4d %02d:%02d" % ( calendar.month_abbr[ct[1]], ct[2], ct[0], ct[3], ct[4] )
	except:
		self.skip = 1

	if self.band: banded = "banded"
	else: banded = "noband"
	if hasattr(self, "skip") and self.skip:
		return "Skipping %s (%s/%ddpi/%s) [no previous raster data found]" % (os.path.basename(self.file), self.device, self.dpi, banded)
	else:
		return "Checking %s (%s/%ddpi/%s) against baseline set on %s" % (os.path.basename(self.file), self.device, self.dpi, banded, baseline_date)

    def runTest(self):
        if hasattr(self, "skip") and self.skip == 1:
	    self.assert_(True)
	    return

	file = self.makefilename()

	gs = gstestgs.Ghostscript()
	gs.command = self.gs
	gs.device = self.device
	gs.dpi = self.dpi
	gs.band = self.band
	gs.infile = self.file
	gs.outfile = gsconf.scratchdir+file
	if self.log_stdout:
	    gs.log_stdout = self.log_stdout
	if self.log_stderr:
	    gs.log_stderr = self.log_stderr

	if gs.process():
	    sum = gssum.make_sum(gsconf.scratchdir+file)
        else:
	    sum = ''
	os.unlink(gsconf.scratchdir+file)

	# add test result to daily database
	if self.track_daily:
	    gssum.add_file(file, dbname=gsconf.get_dailydb_name(), sum=sum)

	if not sum:
	    self.fail("output file could not be created "\
		      "for file: " + self.file)
        else:
	    self.assertEqual(sum, gssum.get_sum(file),
			     'md5sum did not match baseline (' +
			     file + ') for file: ' + self.file)


# add compare tests
def add_compare_test(suite, f, device, dpi, band, track):
    suite.addTest(GSCompareTestCase(gs=gsconf.comparegs,
                                    file=gsconf.comparefiledir + f,
                                    device=device, dpi=dpi,
                                    band=band,
                                    log_stdout=gsconf.log_stdout,
                                    log_stderr=gsconf.log_stderr,
                                    track_daily=track))


def addTests(suite, gsroot, **args):
    if args.has_key('track'):
        track = args['track']
    else:
        track = 0

    # get a list of test files
    comparefiles = os.listdir(gsconf.comparefiledir)

    for f in comparefiles:
        if gsutil.check_extension(f):
	    for params in gsparamsets.testparamsets:
	        add_compare_test(suite, f, params.device,
				 params.resolution, params.banding, track)


if __name__ == '__main__':
    gstestutils.gsRunTestsMain(addTests)

