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

# $Id$

# gsconf.py
#
# configuration file parser for regression tests

import os
import re
import sys
import time

configdir = os.path.dirname(sys.argv[0])
if len(configdir) > 0:
    configdir = configdir + "/"

def parse_config(file=configdir+"testing.cfg"):
    try:
        cf = open(file, "r")
    except:
        print "ERROR: Could not open config file '%s'." % (file,)
        return

    config_re = re.compile("^([^\s]+)\s+(.*)$")

    for l in cf.readlines():
        # strip off EOL chars
        while l and (l[-1] == '\r' or l[-1] == '\n'):
            l = l[:-1]

        # ignore comments and blank lines
        if not l or l[0] == '#':
            continue

        m = config_re.match(l)
        if m:
            sys.modules["gsconf"].__dict__[m.group(1)] = m.group(2)


def get_dailydb_name():
    return dailydir + time.strftime("%Y%m%d", time.localtime()) + ".db"

parse_config()
