#    Copyright (C) 1991-1997 Aladdin Enterprises.  All rights reserved.
# This software is licensed to a single customer by Artifex Software Inc.
# under the terms of a specific OEM agreement.

# msvclib.mak
# makefile for Microsoft Visual C++ 4.1 or later, Windows NT or Windows 95 LIBRARY.
#
# All configurable options are surrounded by !ifndef/!endif to allow 
# preconfiguration from within another makefile.

# ------------------------------- Options ------------------------------- #

###### This section is the only part of the file you should need to edit.

# ------ Generic options ------ #

# Define the directory that will hold documentation at runtime.

!ifndef GS_DOCDIR
GS_DOCDIR=c:/gs
!endif

# Define the default directory/ies for the runtime initialization and
# font files.  Separate multiple directories with ';'.
# Use / to indicate directories, not \.
# MSVC will not allow \'s here because it sees '\;' CPP-style as an
# illegal escape.

!ifndef GS_LIB_DEFAULT
GS_LIB_DEFAULT=.;c:/gs/lib;c:/gs/fonts
!endif

# Define whether or not searching for initialization files should always
# look in the current directory first.  This leads to well-known security
# and confusion problems, but users insist on it.
# NOTE: this also affects searching for files named on the command line:
# see the "File searching" section of use.txt for full details.
# Because of this, setting SEARCH_HERE_FIRST to 0 is not recommended.

!ifndef SEARCH_HERE_FIRST
SEARCH_HERE_FIRST=1
!endif

# Define the name of the interpreter initialization file.
# (There is no reason to change this.)

GS_INIT=gs_init.ps

# Choose generic configuration options.

# Setting DEBUG=1 includes debugging features (-Z switch) in the code.
# Code runs substantially slower even if no debugging switches are set,
# and also takes about another 25K of memory.

!ifndef DEBUG
DEBUG=0
!endif

# Setting TDEBUG=1 includes symbol table information for the debugger,
# and also enables stack checking.  Code is substantially slower and larger.

!ifndef TDEBUG
TDEBUG=1
!endif

# Setting NOPRIVATE=1 makes private (static) procedures and variables public,
# so they are visible to the debugger and profiler.
# No execution time or space penalty, just larger .OBJ and .EXE files.

!ifndef NOPRIVATE
NOPRIVATE=0
!endif

# Define the name of the executable file.

!ifndef GS
GS=gslib
!endif

# Define the directory where the IJG JPEG library sources are stored,
# and the major version of the library that is stored there.
# You may need to change this if the IJG library version changes.
# See jpeg.mak for more information.

!ifndef JSRCDIR
JSRCDIR=jpeg-6a
JVERSION=6
!endif

# Define the directory where the PNG library sources are stored,
# and the version of the library that is stored there.
# You may need to change this if the libpng version changes.
# See libpng.mak for more information.

!ifndef PSRCDIR
PSRCDIR=libpng
PVERSION=96
!endif

# Define the directory where the zlib sources are stored.
# See zlib.mak for more information.

!ifndef ZSRCDIR
ZSRCDIR=zlib
!endif

# Define the configuration ID.  Read gs.mak carefully before changing this.

!ifndef CONFIG
CONFIG=
!endif

# Define any other compilation flags.

!ifndef CFLAGS
CFLAGS=
!endif

# ------ Platform-specific options ------ #

# Define which major version of MSVC is being used (currently, 4 & 5 supported)

!ifndef MSVC_VERSION
MSVC_VERSION = 5
!endif

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
! ifndef DEVSTUDIO
DEVSTUDIO=c:\msdev
! endif
COMPBASE=$(DEVSTUDIO)
SHAREDBASE=$(DEVSTUDIO)
!else
! ifndef DEVSTUDIO
DEVSTUDIO=c:\devstudio
! endif
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

!ifndef CPU_FAMILY
CPU_FAMILY=i386
#CPU_FAMILY=ppc
#CPU_FAMILY=alpha  # not supported yet - we need someone to tweak
!endif

# Define the processor (CPU) type. Allowable values depend on the family:
#   i386: 386, 486, 586
#   ppc: 601, 604, 620
#   alpha: not currently used.

!ifndef CPU_TYPE
CPU_TYPE=486
#CPU_TYPE=601
!endif

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

! ifndef FPU_TYPE
FPU_TYPE=0
! endif

!endif

# ------ Devices and features ------ #

# Choose the language feature(s) to include.  See gs.mak for details.

# Allow predefinition of selectable options
# when using this makefile from inside another one.
!ifndef FEATURE_DEVS
FEATURE_DEVS=patlib.dev path1lib.dev hsblib.dev
!endif

# Choose whether to compile the .ps initialization files into the executable.
# See gs.mak for details.

!ifndef COMPILED_INITS
COMPILE_INITS=0
!endif

# Choose whether to store band lists on files or in memory.
# The choices are 'file' or 'memory'.

!ifndef BAND_LIST_STORAGE
BAND_LIST_STORAGE=file
!endif

# Choose which compression method to use when storing band lists in memory.
# The choices are 'lzw' or 'zlib'.  lzw is not recommended, because the
# LZW-compatible code in Ghostscript doesn't actually compress its input.

!ifndef BAND_LIST_COMPRESSOR
BAND_LIST_COMPRESSOR=zlib
!endif

# Choose the implementation of file I/O: 'stdio', 'fd', or 'both'.
# See gs.mak and sfilefd.c for more details.

!ifndef FILE_IMPLEMENTATION
FILE_IMPLEMENTATION=stdio
!endif

# Choose the device(s) to include.  See devs.mak for details.
!ifndef DEVICE_DEVS
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
!endif

# ---------------------------- End of options ---------------------------- #

# Derive values for FPU_TYPE for non-Intel processors.

!if "$(CPU_FAMILY)"=="ppc"
! if $(CPU_TYPE)>601
FPU_TYPE=2
! else
FPU_TYPE=1
! endif
!endif

!if "$(CPU_FAMILY)"=="alpha"
# *** alpha *** This needs fixing
FPU_TYPE=1
!endif

# Define the name of the makefile -- used in dependencies.

MAKEFILE=msvclib.mak msvccmd.mak msvctail.mak winlib.mak

# Define the files to be removed by `make clean'.
# nmake expands macros when encountered, not when used,
# so this must precede the !include statements.

BEGINFILES2=$(GS).ilk $(GS).pdb

# Define these right away because they modify the behavior of
# msvccmd.mak, msvctail.mak & winlib.mak.

LIB_ONLY=gslib.obj gsnogc.obj gconfig.obj gscdefs.obj
MAKEDLL=0
PLATFORM=mslib32_


!include msvccmd.mak
!include winlib.mak
!include msvctail.mak

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


# end of msvclib.mak
