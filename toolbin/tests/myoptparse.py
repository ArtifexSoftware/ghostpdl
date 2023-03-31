#!/usr/bin/env python
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

import optparse
import sys, os
import types

def parseCommandLineBasic(optionsParser=None,args=sys.argv):
    optionsParser.add_option('--verbose','-v',action='store_true',help="noisy execution")

    myself=os.path.basename(args[0])
    options,arguments=optionsParser.parse_args()
    options.myself=myself
    return (options,arguments)

def parseCommandLine(optionsParser=None,args=sys.argv,revisionSkip=False,testfileSkip=False,listfileSkip=False,deviceSkip=False):
    if not optionsParser:
        optionsParser=optparse.OptionParser()

    if not testfileSkip:
        optionsParser.add_option('--testfile','-t',action='store',help="testfile:\"test\"",default="test")

    if not listfileSkip:
        optionsParser.add_option('--listfile','-l',action='store',help="listfile:\"list\"",default="list")

    if not revisionSkip:
        optionsParser.add_option('--revision','-e',action='store',help="revision:HEAD",default="HEAD")

    if not deviceSkip:
        optionsParser.add_option('--device','-d',action='store',help="output device:ppmraw",default="ppmraw")
        optionsParser.add_option('--resolution','-r',action='store',help="output resolution:300",default="300")
        optionsParser.add_option('--banding','-b',action='store',help="output banding:False",default=False)

    optionsParser.add_option('--quiet','-q',action='store_true',help="quiet execution")
    optionsParser.add_option('--verbose','-v',action='store_true',help="noisy execution")

    optionsParser.add_option('--nocleanup','-k',action='store_true',help="do not delete intermediate files")

    myself=os.path.basename(args[0])
    options,arguments=optionsParser.parse_args()
    options.myself=myself
    return (options,arguments)

if __name__ == "__main__":

    optionsParser=optparse.OptionParser()
    optionsParser.add_option('--option','-o',action='store_true',help="sample additional option")
    optionsParser.add_option('--nosvn','-s',action='store_true',help="no not update from svn")
    optionsParser.add_option('--nomake','-m',action='store_true',help="no not make")

    (options,arguments)=myoptparse.parseCommandLine(optionsParser)
    print options.revision
    print options.testfile
    print arguments

#   (options,arguments)=parseCommandLine(optionsParser,revisionSkip=True)
#   (options,arguments)=parseCommandLine(optionsParser,revisionSkip=True,testfileSkip=True,listfileSkip=True,deviceSkip=True):
