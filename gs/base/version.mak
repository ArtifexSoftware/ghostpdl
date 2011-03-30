#  Copyright (C) 2001-2010 Artifex Software, Inc.
#  All Rights Reserved.
#
#  This software is provided AS-IS with no warranty, either express or
#  implied.
#
#  This software is distributed under license and may not be copied, modified
#  or distributed except as expressly authorized under the terms of that
#  license.  Refer to licensing information at http://www.artifex.com/
#  or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
#  San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
#
# $Id$
# Makefile fragment containing the current revision identification.

# Major and minor version numbers.
# MINOR0 is different from MINOR only if MINOR is a single digit.
GS_VERSION_MAJOR=9
GS_VERSION_MINOR=02
GS_VERSION_MINOR0=02
# Revision date: year x 10000 + month x 100 + day.
GS_REVISIONDATE=20110330
# Derived values
GS_VERSION=$(GS_VERSION_MAJOR)$(GS_VERSION_MINOR0)
GS_DOT_VERSION=$(GS_VERSION_MAJOR).$(GS_VERSION_MINOR0)
GS_REVISION=$(GS_VERSION)
