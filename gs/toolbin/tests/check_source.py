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
