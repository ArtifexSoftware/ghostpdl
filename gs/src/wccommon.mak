#    Copyright (C) 1991, 1995, 1996, 1997 Aladdin Enterprises.  All rights reserved.
# 
# This file is part of Aladdin Ghostscript.
# 
# Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
# or distributor accepts any responsibility for the consequences of using it,
# or for whether it serves any particular purpose or works at all, unless he
# or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
# License (the "License") for full details.
# 
# Every copy of Aladdin Ghostscript must include a copy of the License,
# normally in a plain ASCII text file named PUBLIC.  The License grants you
# the right to copy, modify and redistribute Aladdin Ghostscript, but only
# under certain conditions described in the License.  Among other things, the
# License requires that the copyright notice and this notice be preserved on
# all copies.

# wccommon.mak
# Section of Watcom C/C++ makefile common to MS-DOS and MS Windows.
# We strongly recommend that you read the Watcom section of make.txt
# before attempting to build Ghostscript with the Watcom compiler.

# This file is used by watc.mak, watcwin.mak, and watclib.mak.
# Those files supply the following parameters:
#   Configuration, public:
#	GS_LIB_DEFAULT, SEARCH_HERE_FIRST, GS_INIT, FEATURE_DEVS,
#	DEVICE_DEVS*, COMPILE_INITS, BAND_LIST_*
#   Configuration, internal, generic:
#	PLATFORM, MAKEFILE, AK, CC*, DEBUG, NOPRIVATE, CP_, RM_, RMN_
#   Configuration, internal, specific to DOS/Windows:
#	TDEBUG, USE_ASM, ASM,
#	COMPDIR, INCDIR, LIBPATHS,
#	CPU_TYPE, FPU_TYPE

# We want Unix-compatible behavior.  This is part of it.

.NOCHECK

# Define additional extensions to keep `make' happy

.EXTENSIONS: .be .z

# Define the ANSI-to-K&R dependency.  Watcom C accepts ANSI syntax.

AK=

# Note that built-in libpng and zlib aren't available.

SHARE_LIBPNG=0
SHARE_ZLIB=0

# Define the extensions for command, object, and executable files.

CMD=.bat
O=-fo=
OBJ=obj
XE=.exe
XEAUX=.exe

# Define the current directory prefix and shell invocations.

D=\\

EXPP=dos4gw
SH=
# The following is needed to work around a problem in wmake.
SHP=command /c

# Define generic commands.

CP_=call cp.bat
RM_=erase
RMN_=call rm.bat

# Define the arguments for genconf.

CONFILES=-p FILE&s&ps -ol $(ld_tr)

# Define the names of the Watcom C files.
# See the comments in watc.mak and watcwin.mak regarding WCVERSION.

!ifeq WCVERSION 10.5
COMP=$(%WATCOM)\binw\wcc386
LINK=$(%WATCOM)\binw\wlink
STUB=$(%WATCOM)\binw\wstub.exe
WRC=$(%WATCOM)\binw\wrc.exe
!else
!ifeq WCVERSION 10.0
COMP=$(%WATCOM)\binb\wcc386
LINK=$(%WATCOM)\bin\wlink
STUB=$(%WATCOM)\binb\wstub.exe
WRC=$(%WATCOM)\binb\wrc.exe
!else
!ifeq WCVERSION 9.5
COMP=$(%WATCOM)\bin\wcc386
LINK=$(%WATCOM)\bin\wlinkp
STUB=$(%WATCOM)\binb\wstub.exe
WRC=$(%WATCOM)\binb\wrc.exe
!else
COMP=$(%WATCOM)\bin\wcc386p
LINK=$(%WATCOM)\bin\wlinkp
STUB=$(%WATCOM)\binb\wstub.exe
WRC=$(%WATCOM)\binb\rc.exe
!endif
!endif
!endif
INCDIR=$(%WATCOM)\h
WBIND=$(%WATCOM)\binb\wbind.exe

# Define the generic compilation flags.

!ifeq CPU_TYPE 586
!ifeq FPU_TYPE 0
FPU_TYPE=387
!endif
!else
!ifeq CPU_TYPE 486
!ifeq FPU_TYPE 0
FPU_TYPE=387
!endif
!endif
!endif

!ifeq FPU_TYPE 387
FPFLAGS=-fpi87
!else
!ifeq FPU_TYPE 287
FPFLAGS=-fpi287
!else
!ifeq FPU_TYPE -1
FPFLAGS=-fpc
!else
FPFLAGS=-fpi
!endif
!endif
!endif

INTASM=
PCFBASM=

# Define the generic compilation rules.

.asm.obj:
	$(ASM) $(ASMFLAGS) $<;

# Make sure we get the right default target for make.

dosdefault: default
	%null

# Define the compilation flags.

!ifneq NOPRIVATE 0
CP=-dNOPRIVATE
!else
CP=
!endif

!ifneq DEBUG 0
CD=-dDEBUG
!else
CD=
!endif

!ifneq TDEBUG 0
CT=-d2
LCT=DEBUG ALL
!else
CT=-d1
LCT=DEBUG LINES
!endif

!ifneq DEBUG 0
CS=
!else
CS=-s
!endif

GENOPT=$(CP) $(CD) $(CT) $(CS)

CCFLAGS=$(GENOPT) $(PLATOPT) $(FPFLAGS) $(CFLAGS) $(XCFLAGS)
CC=$(COMP) -oi -i=$(INCDIR) $(CCFLAGS) -zq
CCAUX=$(COMP) -oi -i=$(INCDIR) $(FPFLAGS) -zq
CCC=$(CC)
CCD=$(CC)
CCCF=$(CC)
CCINT=$(COMP) -oit -i=$(INCDIR) $(CCFLAGS)
CCLEAF=$(CCC) -s

.c.obj:
	$(CCC) $<
