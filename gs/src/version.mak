#    Copyright (C) 1997, 2000 Aladdin Enterprises.  All rights reserved.
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
# Makefile fragment containing the current revision identification.

# Major and minor version numbers.
# MINOR0 is different from MINOR only if MINOR is a single digit.
GS_VERSION_MAJOR=7
GS_VERSION_MINOR=20
GS_VERSION_MINOR0=20
# Revision date: year x 10000 + month x 100 + day.
GS_REVISIONDATE=`date +%Y%m%d`
# Derived values
GS_VERSION=$(GS_VERSION_MAJOR)$(GS_VERSION_MINOR0)
GS_DOT_VERSION=$(GS_VERSION_MAJOR).$(GS_VERSION_MINOR)
GS_REVISION=$(GS_VERSION)
