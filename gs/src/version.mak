#    Copyright (C) 1997, 1998, 1999, 2000 Aladdin Enterprises.  All rights reserved.
# 
# This software is licensed to a single customer by Artifex Software Inc.
# under the terms of a specific OEM agreement.

# $RCSfile$ $Revision$
# Makefile fragment containing the current revision identification.

# Major and minor version numbers.
# MINOR0 is different from MINOR only if MINOR is a single digit.
# Artifex 6.11 is derived from Aladdin 6.0
GS_VERSION_MAJOR=6
GS_VERSION_MINOR=11
GS_VERSION_MINOR0=11
# Revision date: year x 10000 + month x 100 + day.
GS_REVISIONDATE=20000203
# Derived values
GS_VERSION=$(GS_VERSION_MAJOR)$(GS_VERSION_MINOR0)
GS_DOT_VERSION=$(GS_VERSION_MAJOR).$(GS_VERSION_MINOR)
GS_REVISION=$(GS_VERSION)
