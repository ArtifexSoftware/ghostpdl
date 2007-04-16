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
# dump_baseline_plus.py [<dbfile> [name]]
#
# dumps (prints out) the contents of the baselinedb

import sys, anydbm, gsconf, os

args=sys.argv
myself=args.pop(0)

if len(args) > 0:
    name=args.pop(0)
else:
    print "no database name"
    sys.exit(1)

if not os.path.exists(name):
    print "cannot open",name
    sys.exit(1)
    
db = anydbm.open(name)

base=os.path.basename(name)

keys=db.keys()
keys.sort()
for k in keys:
    print "%50s %15s %s" % (k, base, db[k])

