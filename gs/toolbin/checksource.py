#!/usr/bin/python

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

# Check basic hygiene of source code and directories.

import os

def message(str):
    import sys
    sys.stderr.write(str + '\n')

################ Check that the set of files in the directories is the
################ same as the set of files registered with CVS.

def checkDirsAgainstCVS():
    import re, glob, os.path
    pattern = re.compile("^/([^/]+)/")
    for d in ['doc', 'examples', 'lib', 'man', 'src', 'toolbin']:
        Entries = {}
        Files = {}
        try:
            fp = open(d + '/CVS/Entries', 'r')
        except:
            print "Error: Cannot find CVS/Root"
            return
        while 1:
            line = fp.readline()
            if line == '': break
            found = pattern.match(line)
            if found != None:
                Entries[found.group(1)] = line
        fp.close()
        for f in glob.glob(d + "/*") + glob.glob(d + "/.[a-zA-Z0-9_-]*"):
            if f[-8:] != '.mak.tcl' and not os.path.isdir(f):
                Files[os.path.basename(f)] = 1
        if Files.has_key('CVS'):
            del Files['CVS']
        print "In %s/, %d files, %d CVS entries" % (d, len(Files), len(Entries))
        for f in Entries.keys():
            if not Files.has_key(f):
                message("%s/%s is registered with CVS, but does not exist." % (d, f))
        for f in Files.keys():
            if not Entries.has_key(f):
                message("%s/%s is not registered with CVS." % (d, f))

################ Check that every file certain to be source has an
################ Id or RCSFile line.

def checkForIdLines():
    import re, glob
    pattern = re.compile("[$][ ]*(Id|RCSfile):[ ]*([^,]+),v")
    for d, extns, omit in [
        ('doc', ['*'], ['Changes.htm', 'gs.css', 'gsdoc.el']),
        ('lib', ['eps', 'ps'], []),
        ('man', ['*'], []),
        ('src', ['c', 'cpp', 'h', 'mak'], []),
        ('toolbin', ['*'], ['pre.chk'])
        ]:
        omit = map((lambda o,d=d: d+'/'+o), omit)
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
                    message(f + ' has no Id or RCSfile line.')
                elif found.group(2) != os.path.basename(f):
                    message(found.group(1) + ' line in ' + f + ' has incorrect filename ' + found.group(2))                    

if __name__ == "__main__":
    checkDirsAgainstCVS()
    checkForIdLines()
