#    Copyright (C) 1989-1997 Aladdin Enterprises.  All rights reserved.
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

# bcwin32.mak
# makefile for (MS-Windows 3.1/Win32s / Windows 95 / Windows NT) +
#   Borland C++ 4.5 platform.

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

# Define the names of the executable files.

GS=gswin32
GSCONSOLE=gswin32c
GSDLL=gsdll32

# To build two small executables and a large DLL use MAKEDLL=1.
# To build two large executables use MAKEDLL=0.

MAKEDLL=1

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

# Define the drive, directory, and compiler name for the Borland C files.
# COMPDIR contains the compiler and linker (normally \bc\bin).
# INCDIR contains the include files (normally \bc\include).
# LIBDIR contains the library files (normally \bc\lib).
# COMP is the full C compiler name (bcc32 for Borland C++).
# COMPCPP is the full C++ compiler path name (bcc32 for Borland C++).
# COMPAUX is the compiler name for DOS utilities (bcc for Borland C++).
# RCOMP is the resource compiler name (brcc32 for Borland C++).
# LINK is the full linker path name (normally \bc\bin\tlink32).
# Note that these prefixes are always followed by a \,
#   so if you want to use the current directory, use an explicit '.'.

COMPBASE=c:\bc
COMPDIR=$(COMPBASE)\bin
INCDIR=$(COMPBASE)\include
LIBDIR=$(COMPBASE)\lib
COMP=$(COMPDIR)\bcc32
COMPCPP=$(COMP)
COMPAUX=$(COMPDIR)\bcc
RCOMP=$(COMPDIR)\brcc32
LINK=$(COMPDIR)\tlink32

# If you don't have an assembler, set USE_ASM=0.  Otherwise, set USE_ASM=1,
# and set ASM to the name of the assembler you are using.  This can be
# a full path name if you want.  Normally it will be masm or tasm.

USE_ASM=0
ASM=tasm

# Define the processor architecture. (always i386)

CPU_FAMILY=i386

# Define the processor (CPU) type.  (386, 486 or 586)

CPU_TYPE=386

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

FEATURE_DEVS=level2.dev pdf.dev ttfont.dev

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

# Choose the device(s) to include.  See devs.mak for details.

DEVICE_DEVS=mswindll.dev mswinprn.dev mswinpr2.dev
DEVICE_DEVS2=epson.dev eps9high.dev eps9mid.dev epsonc.dev ibmpro.dev
DEVICE_DEVS3=deskjet.dev djet500.dev laserjet.dev ljetplus.dev ljet2p.dev ljet3.dev ljet4.dev
DEVICE_DEVS4=cdeskjet.dev cdjcolor.dev cdjmono.dev cdj550.dev pj.dev pjxl.dev pjxl300.dev
DEVICE_DEVS5=djet500c.dev declj250.dev lj250.dev jetp3852.dev r4081.dev lbp8.dev uniprint.dev
DEVICE_DEVS6=st800.dev stcolor.dev bj10e.dev bj200.dev m8510.dev necp6.dev bjc600.dev bjc800.dev
DEVICE_DEVS7=t4693d2.dev t4693d4.dev t4693d8.dev tek4696.dev
DEVICE_DEVS8=pcxmono.dev pcxgray.dev pcx16.dev pcx256.dev pcx24b.dev
DEVICE_DEVS9=pbm.dev pbmraw.dev pgm.dev pgmraw.dev pgnm.dev pgnmraw.dev pnm.dev pnmraw.dev ppm.dev ppmraw.dev
DEVICE_DEVS10=tiffcrle.dev tiffg3.dev tiffg32d.dev tiffg4.dev tifflzw.dev tiffpack.dev
DEVICE_DEVS11=bmpmono.dev bmp16.dev bmp256.dev bmp16m.dev tiff12nc.dev tiff24nc.dev
DEVICE_DEVS12=psmono.dev bit.dev bitrgb.dev bitcmyk.dev
DEVICE_DEVS13=pngmono.dev pnggray.dev png16.dev png256.dev png16m.dev
DEVICE_DEVS14=jpeg.dev jpeggray.dev
DEVICE_DEVS15=pdfwrite.dev pswrite.dev epswrite.dev pxlmono.dev pxlcolor.dev

# ---------------------------- End of options ---------------------------- #

# Define the name of the makefile -- used in dependencies.

MAKEFILE=bcwin32.mak winlib.mak winint.mak

# Define the current directory prefix and shell invocations.

D=\\

EXP=
EXPP=
SH=
SHP=

# Define the arguments for genconf.

#CONFILES=-p %s+ -o $(ld_tr) -l lib.tr
# We can't use $(ld_tr) because Borland make expands macro usages in
# macro definitions at definition time, not at use time.
CONFILES=-p %s+ -o ld$(CONFIG).tr -l lib.tr

# Define the generic compilation flags.

PLATOPT=

INTASM=
PCFBASM=

# Make sure we get the right default target for make.

dosdefault: default gs16spl.exe

# Define the compilation flags.

!if $(CPU_TYPE)>500
ASMCPU=/DFOR80386 /DFOR80486
CPFLAGS=-DFOR80486 -DFOR80386
!else if $(CPU_TYPE)>400
ASMCPU=/DFOR80386 /DFOR80486
CPFLAGS=-DFOR80486 -DFOR80386
!else
ASMCPU=/DFOR80386
CPFLAGS=-DFOR80386
!endif

!if $(CPU_TYPE) >= 486 || $(FPU_TYPE) > 0
ASMFPU=/DFORFPU
!else
!if $(FPU_TYPE) < 0
ASMFPU=/DNOFPU
!else
ASMFPU=
!endif
!endif
FPFLAGS=
FPLIB=

!if $(NOPRIVATE)!=0
CP=-DNOPRIVATE
!else
CP=
!endif

!if $(DEBUG)!=0
CD=-DDEBUG
!else
CD=
!endif

!if $(TDEBUG)!=0
CT=-v
LCT=-v -m -s
CO=    # no optimization when debugging
ASMDEBUG=/DDEBUG
!else
CT=
LCT=
CO=-Z -O2
!endif

!if $(DEBUG)!=0 || $(TDEBUG)!=0
CS=-N
!else
CS=
!endif

# Specify output object name
CCOBJNAME=-o

# Specify function prolog type
COMPILE_FOR_DLL=-WDE
COMPILE_FOR_EXE=-WE
COMPILE_FOR_CONSOLE_EXE=-WC

GENOPT=$(CP) $(CD) $(CT) $(CS)

CCFLAGS0=$(GENOPT) $(PLATOPT) $(CPFLAGS) $(FPFLAGS) $(CFLAGS) $(XCFLAGS)
CCFLAGS=$(CCFLAGS0)
CC=$(COMP) @ccf32.tr
CPP=$(COMPCPP) @ccf32.tr
!if $(MAKEDLL)
WX=$(COMPILE_FOR_DLL)
!else
WX=$(COMPILE_FOR_EXE)
!endif
CCC=$(CC) $(WX) $(CO) -c
CCD=$(CC) $(WX) -c
CCINT=$(CC) $(WX) -c
CCCF=$(CCC)
CCLEAF=$(CCC)

# Compiler for auxiliary programs

CCAUX=$(COMPAUX) -ml -I$(INCDIR) -L$(LIBDIR) -O

# Compiler for Windows headers includes

CCCWIN=$(CCC)

# Define the generic compilation rules.

.c.obj:
	$(CCC) { $<}

.cpp.obj:
	$(CCC) { $<}

# Define the files to be removed by `make clean'.
# nmake expands macros when encountered, not when used,
# so this must precede the !include statements.

BEGINFILES2=gs16spl.exe

# Include the generic makefiles.

!include winlib.mak
!include winint.mak

# -------------------------- Auxiliary programs --------------------------- #

ccf32.tr: $(MAKEFILE) makefile
	echo -a1 -d -r -G -N -X -I$(INCDIR) $(CCFLAGS0) -DCHECK_INTERRUPTS > ccf32.tr

# Since we are running in a Windows environment with a different compiler
# for the DOS utilities, we have to invoke genarch by hand:
$(GENARCH_XE): genarch.c $(stdpre_h) $(iref_h) ccf32.tr
	$(COMP) -I$(INCDIR) -L$(LIBDIR) -O genarch.c
	echo ***** Run "win genarch arch.h", then continue make. *****

# -------------------------------- Library -------------------------------- #

# See winlib.mak

# ----------------------------- Main program ------------------------------ #

LIBCTR=libc32.tr

$(LIBCTR): $(MAKEFILE) $(ECHOGS_XE)
        echogs -w $(LIBCTR) $(LIBDIR)\import32.lib+
        echogs -a $(LIBCTR) $(LIBDIR)\cw32.lib

!if $(MAKEDLL)
# The graphical small EXE loader
$(GS_XE): $(GSDLL).dll  $(DWOBJ) $(GSCONSOLE).exe
	$(LINK) /Tpe $(LCT) @&&!
$(LIBDIR)\c0w32 +
$(DWOBJ) +
,$(GS_XE),$(GS), +
$(LIBDIR)\import32 +
$(LIBDIR)\cw32, +
dwmain32.def, +
$(GS).res
!

# The console mode small EXE loader
$(GSCONSOLE).exe: $(OBJC) $(GS).res dw32c.def
	$(LINK) /Tpe /ap $(LCT) $(DEBUGLINK) @&&!
$(LIBDIR)\c0w32 +
$(OBJC) +
,$(GSCONSOLE).exe,$(GSCONSOLE), +
$(LIBDIR)\import32 +
$(LIBDIR)\cw32, +
dw32c.def, +
$(GS).res
!

# The big DLL
$(GSDLL).dll: $(GS_ALL) $(DEVS_ALL) gsdll.$(OBJ) $(GSDLL).res
	$(LINK) $(LCT) /Tpd $(LIBDIR)\c0d32 gsdll @$(ld_tr) $(INTASM) ,$(GSDLL).dll,$(GSDLL),@lib.tr @$(LIBCTR),$(GSDLL).def,$(GSDLL).res

!else
# The big graphical EXE
$(GS_XE):   $(GSCONSOLE).exe $(GS_ALL) $(DEVS_ALL) gsdll.$(OBJ) $(DWOBJNO) $(GS).res dwmain32.def
	copy $(ld_tr) gswin32.tr
	echo $(DWOBJNO) + >> gswin32.tr
	$(LINK) $(LCT) /Tpe $(LIBDIR)\c0w32 gsdll @gswin32.tr $(INTASM) ,$(GS_XE),$(GS),@lib.tr @$(LIBCTR),dwmain32.def,$(GS).res
	-del gswin32.tr

# The big console mode EXE
$(GSCONSOLE).exe:  $(GS_ALL) $(DEVS_ALL) gsdll.$(OBJ) $(OBJCNO) $(GS).res dw32c.def
	copy $(ld_tr) gswin32.tr
	echo $(OBJCNO) + >> gswin32.tr
	$(LINK) $(LCT) /Tpe /ap $(LIBDIR)\c0w32 gsdll @gswin32.tr $(INTASM) ,$(GSCONSOLE),$(GSCONSOLE),@lib.tr @$(LIBCTR),dw32c.def,$(GS).res
	-del gswin32.tr
!endif

# Access to 16 spooler from Win32s

gs16spl.exe: gs16spl.c gs16spl.rc
	$(CCAUX) -W -ms -c -v -I$(INCDIR) $*.c
	$(COMPDIR)\brcc -i$(INCDIR) -r $*.rc
	$(COMPDIR)\tlink /Twe /c /m /s /l @&&!
$(LIBDIR)\c0ws +
$*.obj +
,$*.exe,$*, +
$(LIBDIR)\import +
$(LIBDIR)\mathws +
$(LIBDIR)\cws, +
$*.def
!
	$(COMPDIR)\rlink -t $*.res $*.exe

# end of makefile



