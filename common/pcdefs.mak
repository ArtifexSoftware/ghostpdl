#    Copyright (C) 1997, 1998 Aladdin Enterprises.  All rights reserved.
#    Unauthorized use, copying, and/or distribution prohibited.

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
RM_=erase
RMN_=call $(COMMONDIR)\rm.bat
