#!/usr/bin/env python2.1

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
        for f in glob.glob(self.root + '*'):
            if not (os.path.isdir(f) or os.path.islink(f) or f[len(self.root):] in self.allowedFiles):
                messages.append(f)
	messages.sort()
        self.failIfMessages(messages)

################ Check that the set of files in the directories is the
################ same as the set of files registered with CVS.

class GSCheckDirectoryMatchesCVS(GSTestCase):

    def __init__(self, root, dirName):
        self.root = root
        self.dirName = dirName
        GSTestCase.__init__(self)

    def shortDescription(self):
        return "The contents of %s/ must match its CVS/Entries." % self.dirName

    def runTest(self):
        import re, glob, os.path
        pattern = re.compile("^/([^/]+)/")
        Entries = {}
        Files = {}
        d = self.root + self.dirName
        # Skip files matching patterns in .cvsignore.
        fp = None
        skip = []
        try:
            fp = open(d + '/.cvsignore', 'r')
        except:
            pass
        if fp != None:
            while 1:
                line = fp.readline()
                if line == '': break
                skip += glob.glob(d + '/' + line.rstrip())
            fp.close()
        try:
            fp = open(d + '/CVS/Entries', 'r')
        except:
            self.fail("Cannot find %s/CVS/Root" % d)
            return
        while 1:
            line = fp.readline()
            if line == '': break
            found = pattern.match(line)
            if found != None:
                Entries[found.group(1)] = line
        fp.close()
        for f in glob.glob(d + "/*") + glob.glob(d + "/.[a-zA-Z0-9_-]*"):
            if f not in skip and not os.path.isdir(f):
                Files[os.path.basename(f)] = 1
        m1 = []
        for f in Entries.keys():
            if not Files.has_key(f):
                m1.append("%s/%s" % (d, f))
        if m1:
            m1.sort()
            m1 = ['These %d files are registered with CVS, but do not exist:' % len(m1)] + m1
        m2 = []
        for f in Files.keys():
            if not Entries.has_key(f):
                m2.append("%s/%s" % (d, f))
        if m2:
            m2.sort()
            m2 = ['These %d files are not registered with CVS:' % len(m2)] + m2
        self.failIfMessages(m1 + m2)

################ Main program

gsFilesInTopDirectory = ['LICENSE', 'autogen.sh', 'Makefile',
			 'configure', 'config.log', 'config.status']
gsDirectories = [
    'doc', 'examples', 'lib', 'src', 'toolbin', 'toolbin/tests'
    ]

# Add the tests defined in this file to a suite.

def addTests(suite, gsroot, **args):
    suite.addTest(GSCheckEmptyTopDirectory(gsroot, gsFilesInTopDirectory))
    for dir in gsDirectories:
        suite.addTest(GSCheckDirectoryMatchesCVS(gsroot, dir))

if __name__ == "__main__":
    gsRunTestsMain(addTests)
