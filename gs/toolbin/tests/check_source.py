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

# $RCSfile$ $Revision$

# Check basic hygiene of source code.

import os
from gstestutils import GSTestCase, gsRunTestsMain

################ Check that every file certain to be source has an
################ Id or RCSFile line.

class GSCheckForIdLines(GSTestCase):

    def __init__(self, root, dirName, extensions = ['*'], skip = []):
        self.root = root
        self.dirName = dirName
        self.extensions = extensions
        self.skip = skip
        GSTestCase.__init__(self)

    def shortDescription(self):
        return "All relevant files must have correct Id lines. (checking %s/)" % self.dirName

    def runTest(self):
        import re, glob
        pattern = re.compile("[$][ ]*(Id|RCSfile):[ ]*([^,]+),v")
        d, extns, omit = self.root + self.dirName, self.extensions, self.skip
        omit = map((lambda o,d=d: d+'/'+o), omit)
        missing = []
        incorrect = []
        for e in extns:
            for f in glob.glob(d + '/*.' + e):
                if f in omit or f[-8:] == '.mak.tcl' or os.path.isdir(f):
                    continue
                fp = open(f, 'r')
                contents = fp.read(10000)
                fp.close()
                if len(contents) < 20:  # skip very short files
                    continue
                found = pattern.search(contents)
                if found == None:
                    missing.append(f)
                elif found.group(2) != os.path.basename(f):
                    incorrect.append(f + ' has ' + found.group(1) + ': '+ found.group(2))                    
        if missing:
            missing = ['These %d files have no Id or RCSfile line:' % len(missing)] + missing
        if incorrect:
            incorrect = ['These %d files have an incorrect Id or RCSfile line:' % len(incorrect)] + incorrect
        self.failIfMessages(missing + incorrect)

################ Check that all .h files have Ghostscript standard
################ double-inclusion protection.

class GSCheckDoubleInclusion(GSTestCase):

    def __init__(self, root, skip = []):
        self.root = root
        self.skip = skip
        GSTestCase.__init__(self)

    def runTest(self):
        """All .h files must have #ifdef <file>_INCLUDED protection."""
        import re, glob
        messages = []
        for fname in glob.glob(self.root + 'src/*.h'):
            if fname in self.skip:
                continue
            fp = open(fname, 'r')
            contents = fp.read(10000)
            fp.close()
            included = os.path.basename(fname)[:-2] + '_INCLUDED'
            pattern = '#[ ]*if(ndef[ ]+' + included + '|[ ]+!defined\\(' + included + '\\))'
            if re.search(pattern, contents) == None:
                messages.append(fname)
        if messages:
            messages = ['These %d files do not have _INCLUDED protection:' % len(messages)] + messages
        self.failIfMessages(messages)

################ Main program

gsSourceSets = [
    ('doc', ['*'], ['Changes.htm', 'gs.css', 'gsdoc.el']),
    ('lib', ['eps', 'ps'], []),
    ('man', ['*'], []),
    ('src', ['c', 'cpp', 'h', 'mak'], []),
    ('toolbin', ['*'], ['pre.chk'])
    ]
gsDoubleInclusionOK = [
    'src/gconf.h'
    ]

# Add the tests defined in this file to a suite.

def addTests(suite, gsroot, **args):
    for dir, extns, skip in gsSourceSets:
        suite.addTest(GSCheckForIdLines(gsroot, dir, extns, skip))
    suite.addTest(GSCheckDoubleInclusion(gsroot, gsDoubleInclusionOK))

if __name__ == "__main__":
    gsRunTestsMain(addTests)
