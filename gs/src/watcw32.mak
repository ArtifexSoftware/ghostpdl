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


# watcw32.mak
# makefile for Watcom C++ v??, Windows NT or Windows 95 platform.
#   Does NOT build gs16spl.exe, which is 16-bit and is used under Win32s.
#   Someone with access to the Watcom 16-bit documentation will need to 
#   do this.
# Created 1997-02-23 by Russell Lang from MSVC++ 4.0 makefile.
#**************** THIS MAKEFILE DOES NOT WORK. ****************
#**************** DO NOT ATTEMPT TO USE IT. ****************

# ------------------------------- Options ------------------------------- #

###### This section is the only part of the file you should need to edit.

# ------ Generic options ------ #

# Define the directory that will hold documentation at runtime.

GS_DOCDIR=c:/gs

# Define the default directory/ies for the runtime
# initialization and font files.  Separate multiple directories with \;.
# Use / to indicate directories, not a single \.

GS_LIB_DEFAULT=.;c:/gs\;c:/gs/fonts

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

GS=gswin32
GSCONSOLE=gswin32c
GSDLL=gsdll32

# To build two small executables and a large DLL use MAKEDLL=1
# To build two large executables use MAKEDLL=0

MAKEDLL=1

# Define the source, generated intermediate file, and object directories
# for the graphics library (GL) and the PostScript/PDF interpreter (PS).

GLSRCDIR=.
GLGENDIR=.
GLOBJDIR=.
PSSRCDIR=.
PSGENDIR=.
PSOBJDIR=.

# Define the directory where the IJG JPEG library sources are stored,
# and the major version of the library that is stored there.
# You may need to change this if the IJG library version changes.
# See jpeg.mak for more information.

JSRCDIR=jpeg
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

# Define the drive, directory, and compiler name for the Watcom C files.
# COMPDIR contains the compiler and linker.
# INCDIR contains the include files.
# LIBDIR contains the library files.
# COMP is the full C compiler path name.
# COMPCPP is the full C++ compiler path name.
# COMPAUX is the compiler name for DOS utilities.
# RCOMP is the resource compiler name.
# LINK is the full linker path name.
# WBIND is used for binding resources to an EXE or DLL
# Note that INCDIR and LIBDIR are always followed by a \,
#   so if you want to use the current directory, use an explicit '.'.

COMPBASE=d:\watcom
COMPDIR=$(COMPBASE)\binb
INCDIR=$(COMPBASE)\h;$(COMPBASE)\h\nt
LIBDIR=$(COMPBASE)\lib386;$(COMPBASE)\lib386\nt
COMP=$(COMPDIR)\wcc386
COMPCPP=$(COMPDIR)\wpp386
COMPAUX=$(COMPDIR)\wcc386
RCOMP=$(COMPDIR)\wrc
LINK=$(COMPDIR)\wlink
WBIND=$(COMPDIR)\wbind

# Define the processor architecture. (always i386)
CPU_FAMILY=i386

# Define the processor (CPU) type.  (386, 486 or 586)
CPU_TYPE=486

# Define the math coprocessor (FPU) type.
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

# ------ Devices and features ------ #

# Choose the language feature(s) to include.  See gs.mak for details.

FEATURE_DEVS=psl3.dev pdf.dev ttfont.dev

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
# See gs.mak and sfxfd.c for more details.

FILE_IMPLEMENTATION=stdio

# Choose the device(s) to include.  See devs.mak for details,
# devs.mak and contrib.mak for the list of available devices.

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

# ---------------------------- End of options ---------------------------- #

# Define the name of the makefile -- used in dependencies.

# The use of multiple file names here is garbage!
MAKEFILE=$(GLSRCDIR)\watcw32.mak winlib.mak winint.mak

# Define the current directory prefix and shell invocations.

D=\\

EXPP=
SH=
# The following is needed to work around a problem in wmake
SHP=command /c

# Define the arguments for genconf.

CONFILES=-p %%s, -l lib.tr
CONFLDTR=-o

# Define the generic compilation flags.

PLATOPT=

INTASM=
PCFBASM=

# Make sure we get the right default target for make.

dosdefault: default

# Define the compilation flags.

!ifeq CPU_TYPE 586
CPFLAGS=-5s
!else
!ifeq CPU_TYPE 486
CPFLAGS=-4s
!else
!ifeq CPU_TYPE 386
CPFLAGS=-3s
!else
CPFLAGS=
!endif
!endif
!endif

!ifeq FPU_TYPE 586
FPFLAGS=-fp5
!else
!ifeq FPU_TYPE 486
FPFLAGS=-fp4
!else
!ifeq FPU_TYPE 386
FPFLAGS=-fp3
!else
FPFLAGS=
!endif
!endif
!endif


!ifneq NOPRIVATE 0
CP=/DNOPRIVATE
!else
CP=
!endif

!ifneq DEBUG 0
CD=/DDEBUG
!else
CD=
!endif

!ifneq TDEBUG 0
# What options should WATCOM use for $(CT) when debugging?
CT=-d2
LCT=DEBUG ALL
COMPILE_FULL_OPTIMIZED=    # no optimization when debugging
COMPILE_WITH_FRAMES=    # no optimization when debugging
COMPILE_WITHOUT_FRAMES=    # no optimization when debugging
!else
CT=-d1
LCT=DEBUG LINES
COMPILE_FULL_OPTIMIZED=-Oilmre -s
COMPILE_WITH_FRAMES=-Of+
COMPILE_WITHOUT_FRAMES=-s
!endif

!ifneq DEBUG 0
CS=
!else
CS=-s
!endif

# Specify function prolog type
COMPILE_FOR_DLL=/LD
COMPILE_FOR_EXE=
COMPILE_FOR_CONSOLE_EXE=

GENOPT=$(CP) $(CD) $(CT) $(CS) -zq

CCFLAGS=$(PLATOPT) $(FPFLAGS) $(CPFLAGS) $(CFLAGS) $(XCFLAGS)
CC=$(COMP) -c $(CCFLAGS) @ccf32.tr
CPP=$(COMPCPP) -c $(CCFLAGS) @ccf32.tr
!ifneq MAKEDLL 0
WX=$(COMPILE_FOR_DLL)
!else
WX=$(COMPILE_FOR_EXE)
!endif
CC_WX=$(CC) $(WX)
CC_=$(CC_WX) $(COMPILE_FULL_OPTIMIZED)
CC_D=$(CC_WX) $(COMPILE_WITH_FRAMES)
CC_INT=$(CC)
CC_LEAF=$(CC_) $(COMPILE_WITHOUT_FRAMES)

# No additional flags are needed for Windows compilation.
CCWINFLAGS=

# Compiler for auxiliary programs

CCAUX=$(COMPAUX) -I$(INCDIR) -O

# Define the files to be removed by `make clean'.
# nmake expands macros when encountered, not when used,
# so this must precede the !include statements.

BEGINFILES2=gsdll32.rex gswin32.rex gswin32c.rex

# Include the generic makefiles.

!include $(GLSRCDIR)\winlib.mak
!include $(GLSRCDIR)\winint.mak

# -------------------------- Auxiliary programs --------------------------- #

ccf32.tr: $(MAKEFILE) makefile
	echo $(GENOPT) -I$(INCDIR) -DCHECK_INTERRUPTS -D_Windows -D__WIN32__ -D_WATCOM_ > ccf32.tr

$(GENARCH_XE): $(GLSRC)genarch.c $(stdpre_h) $(iref_h) ccf32.tr
	$(CCAUX_SETUP)
	$(CCAUX) @ccf32.tr $(GLSRC)genarch.c $(CCAUX_TAIL)

# -------------------------------- Library -------------------------------- #

# See winlib.mak

# ----------------------------- Main program ------------------------------ #

#LIBCTR=libc32.tr
LIBCTR=

#rjl
#$(LIBCTR): $(MAKEFILE) $(ECHOGS_XE)
#        echogs -w $(LIBCTR) $(LIBDIR)\shell32.lib
#        echogs -a $(LIBCTR) $(LIBDIR)\comdlg32.lib

DWOBJLINK=dwdll.obj, dwimg.obj, dwmain.obj, dwtext.obj, gscdefs.obj, gp_wgetv.obj
DWOBJNOLINK= dwnodll.obj, dwimg.obj, dwmain.obj, dwtext.obj
OBJCLINK=dwmainc.obj, dwdllc.obj, gscdefs.obj, gp_wgetv.obj
OBJCNOLINK=dwmainc.obj, dwnodllc.obj

!ifneq MAKEDLL 0
# The graphical small EXE loader
$(GS_XE): $(GSDLL).dll $(GSDLL).lib  $(DWOBJ) $(GSCONSOLE).exe $(GS).res 
	$(LINK) system nt_win $(LCT) Name $(GS_XE) File $(DWOBJLINK) Library $(GSDLL).lib
	$(WBIND) $(GS_XE) -R $(GS).res

# The console mode small EXE loader
$(GSCONSOLE).exe: $(OBJC) $(GS).res dw32c.def
	$(LINK) system nt option map $(LCT) Name $(GSCONSOLE).exe File $(OBJCLINK) Library $(GSDLL).lib
	$(WBIND) $(GSCONSOLE).exe -R $(GS).res

# The big DLL
$(GSDLL).dll: $(GS_ALL) $(DEVS_ALL) gsdll.$(OBJ) $(GSDLL).res 
	$(LINK) system nt_dll initinstance terminstance @gsdll32.wex $(LCT) Name $(GSDLL).dll File gsdll.obj, @$(ld_tr) 
	$(WBIND) $(GSDLL).dll -R $(GSDLL).res

$(GSDLL).lib: $(GSDLL).dll
	erase $(GSDLL).lib
	wlib $(GSDLL) +$(GSDLL).dll

!else
# The big graphical EXE
$(GS_XE): $(GSCONSOLE).exe $(GS_ALL) $(DEVS_ALL) gsdll.$(OBJ) $(DWOBJNO) $(GS).res dwmain32.def
	$(LINK) option map $(LCT) Name $(GS) File gsdll, $(DWOBJNOLINK), @$(ld_tr) 
	$(WBIND) $(GS_XE) -R $(GSDLL).res

# The big console mode EXE
$(GSCONSOLE).exe:  $(GS_ALL) $(DEVS_ALL) gsdll.$(OBJ) $(OBJCNO) $(GS).res dw32c.def
	$(COMPDIR)\$(LINK) option map $(LCT) Name $(GSCONSOLE).exe File gsdll, $(OBJCNOLINK), @$(ld_tr) 
	$(WBIND) $(GSCONSOLE).exe -R $(GSDLL).res
!endif

# end of makefile


