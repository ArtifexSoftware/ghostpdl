# Copyright (C) 2001-2025 Artifex Software, Inc.
# All Rights Reserved.
#
#  This software is provided AS-IS with no warranty, either express or
#  implied.
#
#  This software is distributed under license and may not be copied,
#  modified or distributed except as expressly authorized under the terms
#  of the license contained in the file LICENSE in this distribution.
#
#  Refer to licensing information at http://www.artifex.com or contact
#  Artifex Software, Inc.,  39 Mesa Street, Suite 108A, San Francisco,
#  CA 94129, USA, for further information.

#
# Makefile fragment containing the current revision identification.

# Major, minor and patch version numbers.
GS_VERSION_MAJOR=10
GS_VERSION_MINOR=05
GS_VERSION_PATCH=0
# Revision date: year x 10000 + month x 100 + day.
GS_REVISIONDATE=20250312
# Derived values
GS_VERSION=$(GS_VERSION_MAJOR)$(GS_VERSION_MINOR)$(GS_VERSION_PATCH)
GS_DOT_VERSION=$(GS_VERSION_MAJOR).$(GS_VERSION_MINOR).$(GS_VERSION_PATCH)
GS_REVISION=$(GS_VERSION)
