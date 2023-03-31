#!/usr/bin/python
# -*- Mode: python -*-

# Copyright (C) 2001-2023 Artifex Software, Inc.
# All Rights Reserved.
#
# This software is provided AS-IS with no warranty, either express or
# implied.
#
# This software is distributed under license and may not be copied,
# modified or distributed except as expressly authorized under the terms
# of the license contained in the file LICENSE in this distribution.
#
# Refer to licensing information at http://www.artifex.com or contact
# Artifex Software, Inc.,  39 Mesa Street, Suite 108A, San Francisco,
# CA 94129, USA, for further information.
#


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
    
