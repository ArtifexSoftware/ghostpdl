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

# gstestgs.py
#
# base classes for regression testing

import os
import string
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
