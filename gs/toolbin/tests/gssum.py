#    Copyright (C) 2001 Artifex Software Inc.
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
