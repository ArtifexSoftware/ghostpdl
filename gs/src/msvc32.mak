#    Copyright (C) 1991-1998 Aladdin Enterprises.  All rights reserved.
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
GS_LIB_DEFAULT=.;c:/gs;c:/gs/fonts
!endif

# Define whether or not searching for initialization files should always
# look in the current directory first.  This leads to well-known security
# and confusion problems, but users insist on it.
# NOTE: this also affects searching for files named on the command line:
# see the "File searching" section of Use.htm for full details.
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
# and is also substantially larger.

!ifndef DEBUG
DEBUG=0
!endif

# Setting TDEBUG=1 includes symbol table information for the debugger,
# and also enables stack checking.  Code is substantially slower and larger.

# NOTE: The MSVC++ 5.0 compiler produces incorrect output code with TDEBUG=0.
# Leave TDEBUG set to 1.

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

# Define the source, generated intermediate file, and object directories
# for the graphics library (GL) and the PostScript/PDF interpreter (PS).

# If only one of PSSRCDIR or GLSRCDIR defined, use the other as default.
# If neither defined, use .\
!ifdef GLSRCDIR
!elseifdef PSSRCDIR
GLSRCDIR=$(PSSRCDIR)
!else
GLSRCDIR=.
!endif
!ifndef PSSRCDIR
PSSRCDIR=$(GLSRCDIR)
!endif

# If only one of PSOBJDIR or GLOBJDIR defined, use the other as default.
# If neither defined, use .\obj
!ifdef GLOBJDIR
!elseifdef PSOBJDIR
GLOBJDIR=$(PSOBJDIR)
!else
GLOBJDIR=.\obj
!endif
!ifndef PSOBJDIR
PSOBJDIR=$(GLOBJDIR)
!endif

# If GLGENDIR undef'd, make = GLOBJDIR. If PSGENDIR undef'd, make = PSOBJDIR.
!ifndef GLGENDIR
GLGENDIR=$(GLOBJDIR)
!endif
!ifndef PSGENDIR
PSGENDIR=$(PSOBJDIR)
!endif

# Define the directory where the IJG JPEG library sources are stored,
# and the major version of the library that is stored there.
# You may need to change this if the IJG library version changes.
# See jpeg.mak for more information.

!ifndef JSRCDIR
JSRCDIR=jpeg
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

# Define the name of the makefile -- used in dependencies.

!ifndef MAKEFILE
MAKEFILE=$(GLSRCDIR)\msvc32.mak
!endif

# ------ Platform-specific options ------ #

# Define which major version of MSVC is being used
# (currently, 4 & 5 are supported).

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
FEATURE_DEVS=psl3.dev pdf.dev ttfont.dev
!endif

# Choose whether to compile the .ps initialization files into the executable.
# See gs.mak for details.

!ifndef COMPILE_INITS
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
# See gs.mak and sfxfd.c for more details.

!ifndef FILE_IMPLEMENTATION
FILE_IMPLEMENTATION=stdio
!endif

# Choose the device(s) to include.  See devs.mak for details,
# devs.mak and contrib.mak for the list of available devices.

!ifndef DEVICE_DEVS
DEVICE_DEVS=mswindll.dev mswinprn.dev mswinpr2.dev
DEVICE_DEVS2=epson.dev eps9high.dev eps9mid.dev epsonc.dev ibmpro.dev
DEVICE_DEVS3=deskjet.dev djet500.dev laserjet.dev ljetplus.dev ljet2p.dev ljet3.dev ljet4.dev
DEVICE_DEVS4=cdeskjet.dev cdjcolor.dev cdjmono.dev cdj550.dev pj.dev pjxl.dev pjxl300.dev
DEVICE_DEVS5=djet500c.dev declj250.dev lj250.dev jetp3852.dev r4081.dev lbp8.dev uniprint.dev
DEVICE_DEVS6=st800.dev stcolor.dev bj10e.dev bj200.dev m8510.dev necp6.dev bjc600.dev bjc800.dev
DEVICE_DEVS7=t4693d2.dev t4693d4.dev t4693d8.dev tek4696.dev
DEVICE_DEVS8=pcxmono.dev pcxgray.dev pcx16.dev pcx256.dev pcx24b.dev pcxcmyk.dev
DEVICE_DEVS9=pbm.dev pbmraw.dev pgm.dev pgmraw.dev pgnm.dev pgnmraw.dev pnm.dev pnmraw.dev ppm.dev ppmraw.dev
DEVICE_DEVS10=tiffcrle.dev tiffg3.dev tiffg32d.dev tiffg4.dev tifflzw.dev tiffpack.dev
DEVICE_DEVS11=bmpmono.dev bmp16.dev bmp256.dev bmp16m.dev tiff12nc.dev tiff24nc.dev
DEVICE_DEVS12=psmono.dev bit.dev bitrgb.dev bitcmyk.dev
DEVICE_DEVS13=pngmono.dev pnggray.dev png16.dev png256.dev png16m.dev
DEVICE_DEVS14=jpeg.dev jpeggray.dev
DEVICE_DEVS15=pdfwrite.dev pswrite.dev epswrite.dev pxlmono.dev pxlcolor.dev
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

# Define the files to be removed by `make clean'.
# nmake expands macros when encountered, not when used,
# so this must precede the !include statements.

BEGINFILES2=$(GLOBJDIR)\gs*32*.exp $(GLOBJDIR)\gs*32*.ilk $(GLOBJDIR)\gs*32*.pdb $(GLOBJDIR)\gs*32*.lib $(GLGENDIR)\lib32.rsp

!include $(GLSRCDIR)\msvccmd.mak
!include $(GLSRCDIR)\winlib.mak
!include $(GLSRCDIR)\msvctail.mak
!include $(GLSRCDIR)\winint.mak

# ----------------------------- Main program ------------------------------ #

GSCONSOLE_XE=$(GLOBJ)$(GSCONSOLE).exe
GSDLL_DLL=$(GLOBJ)$(GSDLL).dll

$(GLGEN)lib32.rsp: $(MAKEFILE)
	echo /NODEFAULTLIB:LIBC.lib > $(GLGEN)lib32.rsp
	echo $(LIBDIR)\libcmt.lib >> $(GLGEN)lib32.rsp

!if $(MAKEDLL)
# The graphical small EXE loader
$(GS_XE): $(GSDLL_DLL)  $(DWOBJ) $(GSCONSOLE_XE)
	echo /SUBSYSTEM:WINDOWS > $(GLGEN)gswin32.rsp
	echo /DEF:$(GLSRC)dwmain32.def /OUT:$(GS_XE) >> $(GLGEN)gswin32.rsp
        $(LINK) $(LCT) @$(GLGEN)gswin32.rsp $(DWOBJ) @$(LIBCTR) $(GLOBJ)$(GS).res
	del $(GLGEN)gswin32.rsp

# The console mode small EXE loader
$(GSCONSOLE_XE): $(OBJC) $(GLOBJ)$(GS).res $(GLSRC)dw32c.def
	echo /SUBSYSTEM:CONSOLE > $(GLGEN)gswin32.rsp
	echo  /DEF:$(GLSRC)dw32c.def /OUT:$(GSCONSOLE_XE) >> $(GLGEN)gswin32.rsp
	$(LINK_SETUP)
        $(LINK) $(LCT) @$(GLGEN)gswin32.rsp $(OBJC) @$(LIBCTR) $(GLOBJ)$(GS).res
	del $(GLGEN)gswin32.rsp

# The big DLL
$(GSDLL_DLL): $(GS_ALL) $(DEVS_ALL) $(GLOBJ)gsdll.$(OBJ) $(GLOBJ)$(GSDLL).res $(GLGEN)lib32.rsp
	echo /DLL /DEF:$(GLSRC)gsdll32.def /OUT:$(GSDLL_DLL) > $(GLGEN)gswin32.rsp
	$(LINK_SETUP)
        $(LINK) $(LCT) @$(GLGEN)gswin32.rsp $(GLOBJ)gsdll @$(ld_tr) $(INTASM) @$(GLGEN)lib.tr @$(GLGEN)lib32.rsp @$(LIBCTR) $(GLOBJ)$(GSDLL).res
	del $(GLGEN)gswin32.rsp

!else
# The big graphical EXE
$(GS_XE): $(GSCONSOLE_XE) $(GS_ALL) $(DEVS_ALL) $(GLOBJ)gsdll.$(OBJ) $(DWOBJNO) $(GLOBJ)$(GS).res $(GLSRC)dwmain32.def $(GLGEN)lib32.rsp
	copy $(ld_tr) $(GLGEN)gswin32.tr
	echo $(GLOBJ)dwnodll.obj >> $(GLGEN)gswin32.tr
	echo $(GLOBJ)dwimg.obj >> $(GLGEN)gswin32.tr
	echo $(GLOBJ)dwmain.obj >> $(GLGEN)gswin32.tr
	echo $(GLOBJ)dwtext.obj >> $(GLGEN)gswin32.tr
	echo /DEF:$(GLSRC)dwmain32.def /OUT:$(GS_XE) > $(GLGEN)gswin32.rsp
	$(LINK_SETUP)
        $(LINK) $(LCT) @$(GLGEN)gswin32.rsp $(GLOBJ)gsdll @$(GLGEN)gswin32.tr @$(LIBCTR) $(INTASM) @$(GLGEN)lib.tr @$(GLGEN)lib32.rsp $(GLOBJ)$(GSDLL).res
	del $(GLGEN)gswin32.tr
	del $(GLGEN)gswin32.rsp

# The big console mode EXE
$(GSCONSOLE_XE): $(GS_ALL) $(DEVS_ALL) $(GLOBJ)gsdll.$(OBJ) $(OBJCNO) $(GLOBJ)$(GS).res $(GLSRC)dw32c.def $(GLGEN)lib32.rsp
	copy $(ld_tr) $(GLGEN)gswin32c.tr
	echo $(GLOBJ)dwnodllc.obj >> $(GLGEN)gswin32c.tr
	echo $(GLOBJ)dwmainc.obj >> $(GLGEN)gswin32c.tr
	echo /SUBSYSTEM:CONSOLE > $(GLGEN)gswin32.rsp
	echo /DEF:$(GLSRC)dw32c.def /OUT:$(GSCONSOLE_XE) >> $(GLGEN)gswin32.rsp
	$(LINK_SETUP)
        $(LINK) $(LCT) @$(GLGEN)gswin32.rsp $(GLOBJ)gsdll @$(GLGEN)gswin32c.tr @$(LIBCTR) $(INTASM) @$(GLGEN)lib.tr @$(GLGEN)lib32.rsp $(GLOBJ)$(GS).res
	del $(GLGEN)gswin32.rsp
	del $(GLGEN)gswin32c.tr
!endif

# end of makefile
