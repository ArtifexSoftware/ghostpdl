#    Copyright (C) 1997 Aladdin Enterprises.  All rights reserved.
#    Unauthorized use, copying, and/or distribution prohibited.

# pcdefs.mak
# Definitions for compilation on MS-DOS and MS Windows systems.

# The line following the next one must be blank.
D=\\

OBJ=obj
#O_ is defined per-compiler.
XE=.exe

CP_=copy /B
RM_=erase
RMN_=$(GSDIR)\rm.bat
