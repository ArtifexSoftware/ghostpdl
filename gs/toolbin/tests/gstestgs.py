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

# $Id$

# gstestgs.py
#
# base classes for regression testing

import os
import string
import gsconf
from gstestutils import GSTestCase

class Ghostscript:
	def __init__(self):
		self.command = '/usr/bin/gs'
		self.dpi = 72
		self.band = 0
		self.device = ''
		self.infile = 'input'
		if os.name == 'nt':
			self.nullfile = 'nul'
		else:
			self.nullfile = '/dev/null'
		self.outfile = self.nullfile
		self.gsoptions = ''

	def process(self):
		bandsize = 10000
		if (self.band): bandsize = 30000000
		
		cmd = self.command
		cmd = cmd + self.gsoptions
		cmd = cmd + ' -dQUIET -dNOPAUSE -dBATCH -K100000 '
		cmd = cmd + '-r%d ' % (self.dpi,)
		cmd = cmd + '-dMaxBitmap=%d ' % (bandsize,)
		cmd = cmd + '-sDEVICE=%s ' % (self.device,)
		cmd = cmd + '-sOutputFile=%s ' % (self.outfile,)

		cmd = cmd + ' -c false 0 startjob pop '

		if self.infile[-4:] == ".pdf":
			cmd = cmd + ' -dFirstPage=1 -dLastPage=1 '
		else:
			cmd = cmd + '- < '

		cmd = cmd + ' %s > %s 2> %s' % (self.infile, self.nullfile, self.nullfile)

		ret = os.system(cmd)

		if ret == 0:
			return 1
		else:
			return 0

		
class _GhostscriptTestCase(GSTestCase):
	def __init__(self, gs='gs', dpi=72, band=0, file='test.ps', device='pdfwrite'):
		self.gs = gs
		self.file = file
		self.dpi = dpi
		self.band = band
		self.device = device
		GSTestCase.__init__(self)


class GSCrashTestCase(_GhostscriptTestCase):
	def runTest(self):
		gs = Ghostscript()
		gs.command = self.gs
		gs.dpi = self.dpi
		gs.band = self.band
		gs.device = self.device
		gs.infile = self.file

		self.assert_(gs.process(), 'ghostscript failed to render file: ' + self.file)

class GSCompareTestCase(_GhostscriptTestCase):
	def shortDescription(self):
		file = "%s.%s.%d.%d" % (self.file[string.rindex(self.file, '/') + 1:], self.device, self.dpi, self.band)

		if self.band:
			return "Checking %s (%s/%ddpi/banded) against baseline" % (os.path.basename(self.file), self.device, self.dpi)
		else:
			return "Checking %s (%s/%ddpi/noband) against baseline" % (os.path.basename(self.file), self.device, self.dpi)

	def runTest(self):
		import string, os, gssum
		
		file = "%s.%s.%d.%d" % (self.file[string.rindex(self.file, '/') + 1:], self.device, self.dpi, self.band)

		gs = Ghostscript()
		gs.command = self.gs
		gs.device = self.device
		gs.dpi = self.dpi
		gs.band = self.band
		gs.infile = self.file
		gs.outfile = file

		gs.process()
		sum = gssum.make_sum(file)
		os.unlink(file)
		
		self.assertEqual(sum, gssum.get_sum(file), 'md5sum did not match baseline (' + file + ') for file: ' + self.file)


def fuzzy_compare(file1, file2, tolerance=2, windowsize=5):
	cmd = gsconf.fuzzy + ' -w%d -t%d %s %s > /dev/null 2> /dev/null' % (windowsize, tolerance, file1, file2)

	ret = os.system(cmd)
	if ret == 0:
		return 1
	else:
		return 0

		
class GSFuzzyCompareTestCase(_GhostscriptTestCase):
	def shortDescription(self):
		return "Doing pdfwrite fuzzy test of %s (%s/%d/%d)" % (self.file[string.rindex(self.file, '/') + 1:], self.device, self.dpi, self.band)
	
	def runTest(self):
		file1 = '%s.%s.%d.%d' % (self.file, self.device, self.dpi, self.band)
		file2 = '%s.%s.%d.%d.pdf' % (self.file, 'pdfwrite', self.dpi, self.band)
		file3 = '%s.pdfwrite.%s.%d.%d' % (self.file, self.device, self.dpi, self.band)

		gs = Ghostscript()
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
