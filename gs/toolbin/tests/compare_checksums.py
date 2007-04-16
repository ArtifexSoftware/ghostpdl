#!/usr/bin/python
# -*- Mode: python -*-

import os,sys
import optparse, myoptparse

if __name__ == "__main__":

    optionsParser=optparse.OptionParser()
    optionsParser.add_option('--checksum1',action='store',help="path to checksum db 1")
    optionsParser.add_option('--checksum2',action='store',help="path to checksum db 2")
    (options,arguments)=myoptparse.parseCommandLineBasic(optionsParser)
    
    myself=options.myself
    checksum1=options.checksum1
    checksum2=options.checksum2

    if not checksum1 or not checksum2:
        print myself,"both checksum files are required"
        sys.exit(1)

    try:
        print checksum1
        checksum1_db = anydbm.open(checksum1, 'r')
    except:
        checksum1_db=None
        print myself,"ERROR: cannot open "+checksum1

    try:
        print checksum2
        checksum2_db = anydbm.open(checksum2, 'r')
    except:
        checksum2_db=None
        print myself,"ERROR: cannot open "+checksum2        
    
    if not checksum1 or not checksum2:
        sys.exit(1)
    
