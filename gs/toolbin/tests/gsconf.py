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

# gsconf.py
#
# configuration variables for regression testing

import os

if os.name == 'nt':

	testroot = 'D:/path/to/testbase/'
	baselinedir = 'D:/path/to/baseline/gs/'
	comparedir  = 'D:/path/to/compare/gs/'
	gsroot = 'D:/path/to/current/gs/'
	gsfontdir   = 'D:/path/to/fonts'

	comparefiledir = testroot + 'comparefiles/'
	crashfiledir   = testroot + 'crashfiles/'

	testdatadb = baselinedir + 'testdata.db'
	baselinegs = baselinedir + 'bin/gswin32c.exe'
	comparegs  = comparedir  + 'bin/gswin32c.exe '
	baselineoptions = ' -I' + baselinedir + 'lib;' + gsfontdir + ' -dGS_FONTPATH:' + gsfontdir
	compareoptions  = ' -I' + comparedir  + 'lib;' + gsfontdir + ' -dGS_FONTPATH:' + gsfontdir

else:

	testroot = '/path/to/testbase/'
	baselinedir = '/path/to/baseline/gs/'
	comparedir = '/path/to/compare/gs/'
	gsroot = '/path/to/current/gs/'

	comparefiledir = testroot + 'comparefiles/'
	crashfiledir = testroot + 'crashfiles/'

	testdatadb = testroot + 'testdata.db'
	baselinegs = baselinedir + 'bin/gs'
	comparegs = comparedir + 'bin/gs'
	baselineoptions = ''
	compareoptions  = ''
