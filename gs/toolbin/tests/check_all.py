#!/usr/bin/env python2.1

#    Copyright (C) 2002 Aladdin Enterprises.  All rights reserved.
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

# Run all the Ghostscript 'check' tests.

from gstestutils import gsRunTestsMain

def addTests(suite, **args):
    import check_dirs; check_dirs.addTests(suite, **args)
    import check_docrefs; check_docrefs.addTests(suite, **args)
    import check_source; check_source.addTests(suite, **args)

if __name__ == "__main__":
    gsRunTestsMain(addTests)
