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

# $RCSfile$ $Revision$

# gssum.py
#
# this module contains routines for calculating sums and managing
# the sum database

def exists(file):
    import anydbm
    import gsconf

    db = anydbm.open(gsconf.testdatadb)
    exists = db.has_key(file)
    db.close()

    return exists

def add_file(file):
    import anydbm
    import gsconf

    db = anydbm.open(gsconf.testdatadb, 'w')
    db[file] = make_sum(file)
    db.close()

def get_sum(file):
    import anydbm, gsconf

    db = anydbm.open(gsconf.testdatadb)
    sum = db[file]
    db.close()

    return sum

def make_sum(file):
    import os, string, md5
    from stat import *

    mode = os.stat(file)[ST_MODE]
    if S_ISREG(mode):
	sum = md5.new()
	f = open(file, "r")
	data = f.read(1024)
	while data:
		sum.update(data)
		data = f.read(1024)
	f.close()

        return sum.hexdigest()
    
    return 0
