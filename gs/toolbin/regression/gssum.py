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
    import os, string
    from stat import *

    mode = os.stat(file)[ST_MODE]
    if S_ISREG(mode):
        f = os.popen('md5sum %s' % (file,))
        line = string.split(f.readline(), ' ')[0]
        f.close()
        return line
    
    return 0
