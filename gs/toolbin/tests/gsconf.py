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

	fuzzy = 'D:/path/to/fuzzy.exe'

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

	fuzzy = '/path/to/fuzzy'
