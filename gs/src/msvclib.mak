#    Copyright (C) 1991-1997 Aladdin Enterprises.  All rights reserved.
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

# msvclib.mak
# makefile for Microsoft Visual C++ 4.1 or later, Windows NT or Windows 95 LIBRARY.
#

# ------------------------------- Options ------------------------------- #

###### This section is the only part of the file you should need to edit.

# ------ Generic options ------ #

# Define the directory that will hold documentation at runtime.

GS_DOCDIR=c:/gs

# Define the default directory/ies for the runtime initialization and
# font files.  Separate multiple directories with ';'.
# Use / to indicate directories, not \.
# MSVC will not allow \'s here because it sees '\;' CPP-style as an
# illegal escape.

GS_LIB_DEFAULT=.;c:/gs/lib;c:/gs/fonts

# Define whether or not searching for initialization files should always
# look in the current directory first.  This leads to well-known security
# and confusion problems, but users insist on it.
# NOTE: this also affects searching for files named on the command line:
# see the "File searching" section of use.txt for full details.
# Because of this, setting SEARCH_HERE_FIRST to 0 is not recommended.

SEARCH_HERE_FIRST=1

# Define the name of the interpreter initialization file.
# (There is no reason to change this.)

GS_INIT=gs_init.ps

# Choose generic configuration options.

# Setting DEBUG=1 includes debugging features (-Z switch) in the code.
# Code runs substantially slower even if no debugging switches are set,
# and also takes about another 25K of memory.

DEBUG=0

# Setting TDEBUG=1 includes symbol table information for the debugger,
# and also enables stack checking.  Code is substantially slower and larger.

TDEBUG=0

# Setting NOPRIVATE=1 makes private (static) procedures and variables public,
# so they are visible to the debugger and profiler.
# No execution time or space penalty, just larger .OBJ and .EXE files.

NOPRIVATE=0

# Define the name of the executable file.

GS=gslib

# Define the directory where the IJG JPEG library sources are stored,
# and the major version of the library that is stored there.
# You may need to change this if the IJG library version changes.
# See jpeg.mak for more information.

JSRCDIR=jpeg-6a
JVERSION=6

# Define the directory where the PNG library sources are stored,
# and the version of the library that is stored there.
# You may need to change this if the libpng version changes.
# See libpng.mak for more information.

PSRCDIR=libpng
PVERSION=96

# Define the directory where the zlib sources are stored.
# See zlib.mak for more information.

ZSRCDIR=zlib

# Define the configuration ID.  Read gs.mak carefully before changing this.

CONFIG=

# Define any other compilation flags.

CFLAGS=

# ------ Platform-specific options ------ #

# Define which major version of MSVC is being used (currently, 4 & 5 supported)

MSVC_VERSION = 4

# Define the drive, directory, and compiler name for the Microsoft C files.
# COMPDIR contains the compiler and linker (normally \msdev\bin).
# INCDIR contains the include files (normally \msdev\include).
# LIBDIR contains the library files (normally \msdev\lib).
# COMP is the full C compiler path name (normally \msdev\bin\cl).
# COMPCPP is the full C++ compiler path name (normally \msdev\bin\cl).
# COMPAUX is the compiler name for DOS utilities (normally \msdev\bin\cl).
# RCOMP is the resource compiler name (normallly \msdev\bin\rc).
# LINK is the full linker path name (normally \msdev\bin\link).
# Note that when INCDIR and LIBDIR are used, they always get a '\' appended,
#   so if you want to use the current directory, use an explicit '.'.

!if $(MSVC_VERSION) == 4
DEVSTUDIO=c:\msdev
COMPBASE=$(DEVSTUDIO)
SHAREDBASE=$(DEVSTUDIO)
!else
DEVSTUDIO=c:\devstudio
COMPBASE=$(DEVSTUDIO)\VC
SHAREDBASE=$(DEVSTUDIO)\SharedIDE
!endif
COMPDIR=$(COMPBASE)\bin
LINKDIR=$(COMPBASE)\bin
RCDIR=$(SHAREDBASE)\bin
INCDIR=$(COMPBASE)\include
LIBDIR=$(COMPBASE)\lib
COMP=$(COMPDIR)\cl
COMPCPP=$(COMP)
COMPAUX=$(COMPDIR)\cl
RCOMP=$(RCDIR)\rc
LINK=$(LINKDIR)\link

# Define the processor architecture. (i386, ppc, alpha)

CPU_FAMILY=i386
#CPU_FAMILY=ppc
#CPU_FAMILY=alpha  # not supported yet - we need someone to tweak

# Define the processor (CPU) type. Allowable values depend on the family:
#   i386: 386, 486, 586
#   ppc: 601, 604, 620
#   alpha: not currently used.

CPU_TYPE=486
#CPU_TYPE=601

!if "$(CPU_FAMILY)"=="i386"

# Intel(-compatible) processors are the only ones for which the CPU type
# doesn't indicate whether a math coprocessor is present.
# For Intel processors only, define the math coprocessor (FPU) type.
# Options are -1 (optimize for no FPU), 0 (optimize for FPU present,
# but do not require a FPU), 87, 287, or 387.
# If you have a 486 or Pentium CPU, you should normally set FPU_TYPE to 387,
# since most of these CPUs include the equivalent of an 80387 on-chip;
# however, the 486SX and the Cyrix 486SLC do not have an on-chip FPU, so if
# you have one of these CPUs and no external FPU, set FPU_TYPE to -1 or 0.
# An xx87 option means that the executable will run only if a FPU
# of that type (or higher) is available: this is NOT currently checked
# at runtime.

FPU_TYPE=0

!endif

# ------ Devices and features ------ #

# Choose the language feature(s) to include.  See gs.mak for details.

FEATURE_DEVS=patlib.dev path1lib.dev hsblib.dev

# Choose whether to compile the .ps initialization files into the executable.
# See gs.mak for details.

COMPILE_INITS=0

# Choose whether to store band lists on files or in memory.
# The choices are 'file' or 'memory'.

BAND_LIST_STORAGE=file

# Choose which compression method to use when storing band lists in memory.
# The choices are 'lzw' or 'zlib'.  lzw is not recommended, because the
# LZW-compatible code in Ghostscript doesn't actually compress its input.

BAND_LIST_COMPRESSOR=zlib

# Choose the implementation of file I/O: 'stdio', 'fd', or 'both'.
# See gs.mak and sfilefd.c for more details.

FILE_IMPLEMENTATION=stdio

# Choose the device(s) to include.  See devs.mak for details.
DEVICE_DEVS=ljet2p.dev
DEVICE_DEVS2=
DEVICE_DEVS3=
DEVICE_DEVS4=
DEVICE_DEVS5=
DEVICE_DEVS6=
DEVICE_DEVS7=
DEVICE_DEVS8=
DEVICE_DEVS9=
DEVICE_DEVS10=
DEVICE_DEVS11=
DEVICE_DEVS12=
DEVICE_DEVS13=
DEVICE_DEVS15=

# ---------------------------- End of options ---------------------------- #

# Derive values for FPU_TYPE for non-Intel processors.

!if "$(CPU_FAMILY)"=="ppc"
!if $(CPU_TYPE)>601
FPU_TYPE=2
!else
FPU_TYPE=1
!endif
!endif

!if "$(CPU_FAMILY)"=="alpha"
# *** alpha *** This needs fixing
FPU_TYPE=1
!endif

# Define the name of the makefile -- used in dependencies.

MAKEFILE=msvclib.mak msvccom.mak winlib.mak

# Define the files to be removed by `make clean'.
# nmake expands macros when encountered, not when used,
# so this must precede the !include statements.

BEGINFILES2=$(GS).ilk $(GS).pdb

# Define these right away because they modify the behavior of
# msvccom.mak & winlib.mak.

LIB_ONLY=gslib.obj gsnogc.obj gconfig.obj gscdefs.obj
MAKEDLL=0
PLATFORM=mslib32_


!include msvccom.mak

# -------------------------------- Library -------------------------------- #

# The Windows Win32 platform for library

# For some reason, C-file dependencies have to be before mslib32__.dev

gp_mslib.$(OBJ): gp_mslib.c $(AK)
	$(CCCWIN) gp_mslib.c

gp_mswin.$(OBJ): gp_mswin.c $(AK) gp_mswin.h \
 $(ctype__h) $(dos__h) $(malloc__h) $(memory__h) $(string__h) $(windows__h) \
 $(gx_h) $(gp_h) $(gpcheck_h) $(gserrors_h) $(gsexit_h)
	$(CCCWIN) gp_mswin.c

gp_win32.$(OBJ): gp_win32.c $(AK) $(stdio__h) $(dos__h) $(string__h) \
 $(gstypes_h) $(gsmemory_h) $(gp_h) $(gp_sync_h) $(gserror_h) $(gserrors_h)
	$(CCCWIN) gp_win32.c

gp_ntfs.$(OBJ): gp_ntfs.c $(AK) $(stdio__h) $(dos__h) $(string__h) $(gstypes_h) \
 $(gsmemory_h) $(gsstruct_h) $(gp_h) $(gsutil_h) $(windows__h)
	$(CCCWIN) gp_ntfs.c

mslib32__=gp_mslib.$(OBJ) gp_mswin.$(OBJ) gp_win32.$(OBJ) gp_nofb.$(OBJ) \
 gp_ntfs.$(OBJ)

mslib32_.dev: $(mslib32__) $(ECHOGS_XE)
        $(SETMOD) mslib32_ $(mslib32__)

# ----------------------------- Main program ------------------------------ #

# The library tester EXE 
$(GS_XE):  $(GS_ALL) $(DEVS_ALL) $(LIB_ONLY) $(LIBCTR)
	copy $(ld_tr) gslib32.tr
	echo gsnogc.obj >> gslib32.tr
	echo gconfig.obj >> gslib32.tr
	echo gscdefs.obj >> gslib32.tr
	echo  /SUBSYSTEM:CONSOLE > gslib32.rsp
	echo  /OUT:$(GS_XE) >> gslib32.rsp
	$(LINK_SETUP)
        $(LINK) $(LCT) @gslib32.rsp gslib @gslib32.tr @$(LIBCTR) $(INTASM) @lib.tr
	-del gslib32.rsp
	-del gslib32.tr


# end of makefile
