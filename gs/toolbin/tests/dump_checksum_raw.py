#!/usr/bin/env python
# -*- Mode: python -*-

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

# $Id: dump_testdb,v 1.7 2004/07/14 18:21:24 ray Exp $

#
# dump_baseline.py [<dbfile>]
#
# dumps (prints out) the contents of the baselinedb

import string, sys, anydbm, gsconf, os

def compare_field_2(s1, s2):
    if string.split(s1,' ')[1] < string.split(s2,' ')[1]:
    	return -1
    else:
    	return 1

if len(sys.argv) == 2:
    name=sys.argv[1]
else:
    name=gsconf.baselinedb

if not os.path.exists(name):
    print "cannot open",name
    sys.exit(1)

print "opening ", name
db = anydbm.open(name)

# collect the database as strings
dump = []
for k in db.keys():
    print '-%50s- %s' % (k,db[k])

