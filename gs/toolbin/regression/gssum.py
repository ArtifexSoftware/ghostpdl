# gssum.py
#
# this module contains routines for calculating sums and managing
# the sum database

# $Id$

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
