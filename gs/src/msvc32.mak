#    Copyright (C) 1991-1997 Aladdin Enterprises.  All rights reserved.
# This software is licensed to a single customer by Artifex Software Inc.
# under the terms of a specific OEM agreement.

# msvc32.mak
# makefile for 32-bit Microsoft Visual C++, Windows NT or Windows 95 platform.
#
# All configurable options are surrounded by !ifndef/!endif to allow 
# preconfiguration from within another makefile.
#
# Optimization /O2 seems OK with MSVC++ 4.1 & 5.0.
# Created 1997-01-24 by Russell Lang from MSVC++ 2.0 makefile.
# Enhanced 97-05-15 by JD
# Common code factored out 1997-05-22 by L. Peter Deutsch.
# Made pre-configurable by JD 6/4/98

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
GS_LIB_DEFAULT=.;c:/gs
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

!ifndef GS_INIT
GS_INIT=gs_init.ps
!endif

# Choose generic configuration options.

# Setting DEBUG=1 includes debugging features (-Z switch) in the code.
# Code runs substantially slower even if no debugging switches are set,
# and also takes about another 25K of memory.

!ifndef DEBUG
DEBUG=1
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
GS=gswin32
!endif
!ifndef GSCONSOLE
GSCONSOLE=gswin32c
!endif
!ifndef GSDLL
GSDLL=gsdll32
!endif

# To build two small executables and a large DLL use MAKEDLL=1
# To build two large executables use MAKEDLL=0

!ifndef MAKEDLL
MAKEDLL=1
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

!ifndef FEATURE_DEVS
FEATURE_DEVS=level2.dev pdf.dev ttfont.dev
!endif

# Choose whether to compile the .ps initialization files into the executable.
# See gs.mak for details.

!ifndef COMPILE_INITS
COMPILE_INITS=0
!endif

# Choose whether to store band lists on files or in memory.
# The choices are 'file' or 'memory'.

!ifndef BAND_LIST_STORAGE
BAND_LIST_STORAGE=memory
!endif

# Choose which compression method to use when storing band lists in memory.
# The choices are 'lzw' or 'zlib'.  lzw is not recommended, because the
# LZW-compatible code in Ghostscript doesn't actually compress its input.

!ifndef BAND_LIST_COMPRESSOR
BAND_LIST_COMPRESSOR=zlib
!endif

# Choose the implementation of file I/O: 'stdio', 'fd', or 'both'.
# See gs.mak and sfxfd.c for more details.

!ifndef FILE_IMPLEMENTATION
FILE_IMPLEMENTATION=stdio
!endif

# Choose the device(s) to include.  See devs.mak for details.

!ifndef DEVICE_DEVS
DEVICE_DEVS=mswindll.dev mswinprn.dev mswinpr2.dev
DEVICE_DEVS2=asynmono.dev
DEVICE_DEVS3=bmpmono.dev
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
DEVICE_DEVS14=
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

MAKEFILE=msvc32.mak msvccmd.mak msvctail.mak winlib.mak winint.mak

# Define the files to be removed by `make clean'.
# nmake expands macros when encountered, not when used,
# so this must precede the !include statements.

BEGINFILES2=gsdll32.exp gsdll32.ilk gsdll32.pdb gsdll32.lib\
   gswin32.exp gswin32.ilk gswin32.pdb gswin32.lib\
   gswin32c.exp gswin32c.ilk gswin32c.pdb gswin32c.lib


!include msvccmd.mak
!include winlib.mak
!include msvctail.mak
!include winint.mak

# ----------------------------- Main program ------------------------------ #

!if $(MAKEDLL)
# The graphical small EXE loader
$(GS_XE): $(GSDLL).dll  $(DWOBJ) $(GSCONSOLE).exe
	echo /SUBSYSTEM:WINDOWS > gswin32.rsp
	echo /DEF:dwmain32.def /OUT:$(GS_XE) >> gswin32.rsp
        $(LINK) $(LCT) @gswin32.rsp $(DWOBJ) @$(LIBCTR) $(GS).res
	del gswin32.rsp

# The console mode small EXE loader
$(GSCONSOLE).exe: $(OBJC) $(GS).res dw32c.def
	echo /SUBSYSTEM:CONSOLE > gswin32.rsp
	echo  /DEF:dw32c.def /OUT:$(GSCONSOLE).exe >> gswin32.rsp
	$(LINK_SETUP)
        $(LINK) $(LCT) @gswin32.rsp $(OBJC) @$(LIBCTR) $(GS).res
	del gswin32.rsp

# The big DLL
$(GSDLL).dll: $(GS_ALL) $(DEVS_ALL) gsdll.$(OBJ) $(GSDLL).res
	echo /DLL /DEF:gsdll32.def /OUT:$(GSDLL).dll > gswin32.rsp
	$(LINK_SETUP)
        $(LINK) $(LCT) @gswin32.rsp gsdll @$(ld_tr) $(INTASM) @lib.tr @$(LIBCTR) $(GSDLL).res
	del gswin32.rsp

!else
# The big graphical EXE
$(GS_XE):   $(GSCONSOLE).exe $(GS_ALL) $(DEVS_ALL) gsdll.$(OBJ) $(DWOBJNO) $(GS).res dwmain32.def
	copy $(ld_tr) gswin32.tr
	echo dwnodll.obj >> gswin32.tr
	echo dwimg.obj >> gswin32.tr
	echo dwmain.obj >> gswin32.tr
	echo dwtext.obj >> gswin32.tr
	echo /DEF:dwmain32.def /OUT:$(GS_XE) > gswin32.rsp
	$(LINK_SETUP)
        $(LINK) $(LCT) @gswin32.rsp gsdll @gswin32.tr @$(LIBCTR) $(INTASM) @lib.tr $(GSDLL).res
	del gswin32.tr
	del gswin32.rsp

# The big console mode EXE
$(GSCONSOLE).exe:  $(GS_ALL) $(DEVS_ALL) gsdll.$(OBJ) $(OBJCNO) $(GS).res dw32c.def
	copy $(ld_tr) gswin32c.tr
	echo dwnodllc.obj >> gswin32c.tr
	echo dwmainc.obj >> gswin32c.tr
	echo  /SUBSYSTEM:CONSOLE > gswin32.rsp
	echo  /DEF:dw32c.def /OUT:$(GSCONSOLE).exe  >> gswin32.rsp
	$(LINK_SETUP)
        $(LINK) $(LCT) @gswin32.rsp gsdll @gswin32c.tr @$(LIBCTR) $(INTASM) @lib.tr $(GS).res
	del gswin32.rsp
	del gswin32c.tr
!endif

# end of makefile
