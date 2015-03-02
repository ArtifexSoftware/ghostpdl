# Copyright (C) 2001-2012 Artifex Software, Inc.
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
# Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134, San Rafael,
# CA  94903, U.S.A., +1(415)492-9861, for further information.
#

# pcdefs.mak
# Definitions for compilation on MS-DOS and MS Windows systems.

# The line following the next one must be blank.
#C_ is defined per-compiler.
D=\\

OBJ=obj
I_=-I
II=-I
_I=
#O_ is defined per-compiler.
XE=.exe

CP_=copy /B
RM_=-erase
RMN_=-call $(COMMONDIR)\rm.bat
