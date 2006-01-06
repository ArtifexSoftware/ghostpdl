#!/usr/bin/env python

#    Copyright (C) 2002 Aladdin Enterprises.  All rights reserved.
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

# Check the contents of Ghostscript directories.

import os
from gstestutils import GSTestCase, gsRunTestsMain

################ Check that there are no files in the top-level directory,
################ other than a specified list.

class GSCheckEmptyTopDirectory(GSTestCase):

    def __init__(self, root, allowedFiles = []):
        self.root = root
        self.allowedFiles = allowedFiles
        GSTestCase.__init__(self)

    def runTest(self):
        """The top-level directory must not have extraneous files."""
        import glob, os.path
        messages = []
        for f in glob.glob(os.path.join(self.root, '*')):
            if not (os.path.isdir(f) or os.path.islink(f) or os.path.basename(f) in self.allowedFiles):
                messages.append(f)
	messages.sort()
        self.failIfMessages(messages)

################ Main program

gsFilesInTopDirectory = ['LICENSE', 'autogen.sh', 'Makefile',
			 'configure', 'config.log', 'config.status']
# Add the tests defined in this file to a suite.

def addTests(suite, gsroot, **args):
    suite.addTest(GSCheckEmptyTopDirectory(gsroot, gsFilesInTopDirectory))

if __name__ == "__main__":
    gsRunTestsMain(addTests)
