#!/usr/bin/env python2.1

#    Copyright (C) 2002 Aladdin Enterprises.  All rights reserved.
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

gsFilesInTopDirectory = ['autogen.sh']
gsDirectories = [
    'doc', 'examples', 'lib', 'src', 'toolbin', 'toolbin/tests',
    'toolbin/regression'
    ]

# Add the tests defined in this file to a suite.

def addTests(suite, gsroot, **args):
    suite.addTest(GSCheckEmptyTopDirectory(gsroot, gsFilesInTopDirectory))
    for dir in gsDirectories:
        suite.addTest(GSCheckDirectoryMatchesCVS(gsroot, dir))

if __name__ == "__main__":
    gsRunTestsMain(addTests)
