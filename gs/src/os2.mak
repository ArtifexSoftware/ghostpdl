#    Copyright (C) 1989, 1995, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
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


# makefile for MS-DOS or OS/2 GCC/EMX platform.
# Uses Borland (MSDOS) MAKER or 
# Uses IBM NMAKE.EXE Version 2.000.000 Mar 27 1992

# ------------------------------- Options ------------------------------- #

###### This section is the only part of the file you should need to edit.

# ------ Generic options ------ #

# Define the directory that will hold documentation at runtime.

GS_DOCDIR=c:/gs

# Define the default directory/ies for the runtime
# initialization and font files.  Separate multiple directories with ;.
# Use / to indicate directories, not a single \.

GS_LIB_DEFAULT=c:/gs;c:/gs/fonts

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

# Setting GDEBUG=1 includes symbol table information for GDB.
# Produces larger .OBJ and .EXE files.

GDEBUG=0

# Setting NOPRIVATE=1 makes private (static) procedures and variables public,
# so they are visible to the debugger and profiler.
# No execution time or space penalty, just larger .OBJ and .EXE files.

NOPRIVATE=0

# Setting MAKEDLL=1 makes the target a DLL instead of an EXE
MAKEDLL=1

# Setting EMX=1 uses GCC/EMX
# Setting IBMCPP=1 uses IBM C++
EMX=1
IBMCPP=0

# Define the name of the executable file.

GS=gsos2
GSDLL=gsdll2

# Define the source, generated intermediate file, and object directories
# for the graphics library (GL) and the PostScript/PDF interpreter (PS).

# This makefile has never been tested with any other values than these,
# and almost certainly won't work with other values.
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

# ------ Platform-specific options ------ #

# If you don't have an assembler, set USE_ASM=0.  Otherwise, set USE_ASM=1,
# and set ASM to the name of the assembler you are using.  This can be
# a full path name if you want.  Normally it will be masm or tasm.

USE_ASM=0
ASM= 

# Define the drive, directory, and compiler name for the EMX files.
# COMP is the compiler name (gcc)
# COMPDIR contains the compiler and linker (normally \emx\bin).
# EMXPATH contains the path to the EMX directory (normally /emx)
# INCDIR contains the include files (normally /emx/include).
# LIBDIR contains the library files (normally /emx/lib).
# Note that these prefixes are always followed by a \,
#   so if you want to use the current directory, use an explicit '.'.

!if $(EMX)
COMP=gcc
COMPBASE=\emx
EMXPATH=/emx
COMPDIR=$(COMPBASE)\bin
INCDIR=$(EMXPATH)/include
LIBDIR=$(EMXPATH)/lib
!endif

!if $(IBMCPP)
COMP=icc /Q
COMPBASE=\ibmcpp
TOOLPATH=\toolkit
COMPDIR=$(COMPBASE)\bin
INCDIR=$(TOOLPATH)\h;$(COMPBASE)\include
LIBDIR=$(TOOLPATH)\lib;$(COMPBASE)\lib
!endif

# Choose platform-specific options.

# Define the processor (CPU) type.  Options are 86 (8086 or 8088),
# 186, 286, 386, 485 (486SX or Cyrix 486SLC), 486 (486DX), or 586 (Pentium).
# Higher numbers produce code that may be significantly smaller and faster,
# but the executable will bail out with an error message on any processor
# less capable than the designated one.

# EMX requires 386 or higher
CPU_TYPE=386

# Define the math coprocessor (FPU) type.
# Options are -1 (optimize for no FPU), 0 (optimize for FPU present,
# but do not require a FPU), 87, 287, or 387.
# If CPU_TYPE is 486 or above, FPU_TYPE is implicitly set to 387,
# since 486DX and later processors include the equivalent of an 80387 on-chip.
# An xx87 option means that the executable will run only if a FPU
# of that type (or higher) is available: this is NOT currently checked
# at runtime.

FPU_TYPE=0

# ---------------------------- End of options ---------------------------- #

# Note that built-in libpng and zlib aren't available.

SHARE_JPEG=0
SHARE_LIBPNG=0
SHARE_ZLIB=0

# Swapping `make' out of memory makes linking much faster.
# only used by Borland MAKER.EXE

#.swap

# Define the platform name.

PLATFORM=os2_

# Define the name of the makefile -- used in dependencies.

MAKEFILE=os2.mak

# Define the files to be deleted by 'make clean'.

BEGINFILES=gspmdrv.exe gs*.res gs*.ico $(GSDLL).dll

# Define the ANSI-to-K&R dependency.

AK=

#Compiler Optimiser option
!if $(EMX)
CO=-O
!endif
!if $(IBMCPP)
#CO=/O+
CO=/O-
!endif

# Make sure we get the right default target for make.

dosdefault: default gspmdrv.exe

# Define a rule for invoking just the preprocessor.

.c.i:
	$(COMPDIR)\cpp $(CCFLAGS) $<

# Define the extensions for command, object, and executable files.

CMD=.cmd
I_=-I
II=-I
_I=
# There should be a <space> at the end of the definition of O_,
# but we have to work around the fact that some `make' programs
# drop trailing spaces in macro definitions.
NULL=
O_=-o $(NULL)
!if $(MAKEDLL)
OBJ=obj
!else
OBJ=o
!endif
XE=.exe
XEAUX=.exe

# Define the current directory prefix, shell quote string, and shell name.

D=\#

EXP=
QQ="
SH=
SHP=

# Define generic commands.

# We use cp.cmd rather than copy /B so that we update the write date.
CP_=cp.cmd
# We use rm.cmd rather than erase because rm.cmd never generates
# a non-zero return code.
RM_=rm.cmd
# OS/2 erase, unlike MS-DOS erase, accepts multiple files or patterns.
RMN_=rm.cmd

# Define the arguments for genconf.

!if $(MAKEDLL)
CONFILES=-p %%s+ -l lib.tr
!else
CONFILES=-l lib.tr
!endif
CONFLDTR=-o

# Define the generic compilation flags.

!if $(CPU_TYPE) >= 486
ASMCPU=/DFOR80386 /DFOR80486
PLATOPT=-DFOR80386 -DFOR80486
!else
!if $(CPU_TYPE) >= 386
ASMCPU=/DFOR80386
PLATOPT=-DFOR80386
!endif
!endif

!if $(FPU_TYPE) > 0
ASMFPU=/DFORFPU
!else
ASMFPU=
!endif

!if $(USE_ASM)
INTASM=iutilasm.$(OBJ)
PCFBASM=gdevegaa.$(OBJ)
!else
INTASM=
PCFBASM=
!endif

# Define the generic compilation rules.

ASMFLAGS=$(ASMCPU) $(ASMFPU) $(ASMDEBUG)

.asm.o:
	$(ASM) $(ASMFLAGS) $<;

# ---------------------- MS-DOS I/O debugging option ---------------------- #

dosio_=$(GLOBJ)zdosio.$(OBJ)
dosio.dev: $(dosio_)
	$(SETMOD) dosio $(dosio_)
	$(ADDMOD) dosio -oper zdosio

$(PSOBJ)zdosio.$(OBJ): $(PSSRC)zdosio.c $(OP) $(store_h)
	$(PSCC) $(PSO_)zdosio.$(OBJ) $(C_) $(PSSRC)zdosio.c

# ----------------------------- Assembly code ----------------------------- #

$(PSOBJ)iutilasm.$(OBJ): $(PSSRC)iutilasm.asm

#################  END

# Define the compilation flags.

!if $(NOPRIVATE)
CP=-DNOPRIVATE
!else
CP=
!endif

!if $(DEBUG)
CD=-DDEBUG
!else
CD=
!endif
  
!if $(GDEBUG)
!if $(EMX)
CGDB=-g
!endif
!if $(IBMCPP)
CGDB=/Ti+
!endif
!else
CGDB=
!endif

!if $(MAKEDLL)
!if $(EMX)
CDLL=-Zdll -Zso -Zsys -Zomf -D__DLL__
!endif
!if $(IBMCPP)
CDLL=/Gd- /Ge- /Gm+ /Gs+ /D__DLL__
!endif
!else
CDLL=
!endif

GENOPT=$(CP) $(CD) $(CGDB) $(CDLL) $(CO)

CCFLAGS0=$(GENOPT) $(PLATOPT)
CCFLAGS=$(CCFLAGS0) 
CC=$(COMPDIR)\$(COMP) $(CCFLAGS0)
CC_=$(CC)
CC_D=$(CC) $(CO)
CC_INT=$(CC)
CC_LEAF=$(CC_)

# ------ Devices and features ------ #

# Choose the language feature(s) to include.  See gs.mak for details.
# Since we have a large address space, we include some optional features.

FEATURE_DEVS=psl3.dev pdf.dev

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
# devs.mak, pcwin.mak, and contrib.mak for the list of available devices.

!if $(MAKEDLL)
DEVICE_DEVS=os2pm.dev os2dll.dev os2prn.dev
!else
DEVICE_DEVS=os2pm.dev
!endif
#DEVICE_DEVS1=x11.dev x11alpha.dev x11cmyk.dev x11mono.dev
DEVICE_DEVS1=
DEVICE_DEVS2=epson.dev eps9high.dev eps9mid.dev epsonc.dev ibmpro.dev
DEVICE_DEVS3=deskjet.dev djet500.dev laserjet.dev ljetplus.dev ljet2p.dev ljet3.dev ljet4.dev
DEVICE_DEVS4=cdeskjet.dev cdjcolor.dev cdjmono.dev cdj550.dev pj.dev pjxl.dev pjxl300.dev
DEVICE_DEVS5=djet500c.dev declj250.dev lj250.dev jetp3852.dev r4081.dev t4693d2.dev t4693d4.dev t4693d8.dev tek4696.dev lbp8.dev uniprint.dev
DEVICE_DEVS6=st800.dev stcolor.dev bj10e.dev bj200.dev bjc600.dev bjc800.dev m8510.dev necp6.dev
DEVICE_DEVS7=dfaxhigh.dev dfaxlow.dev
DEVICE_DEVS8=pcxmono.dev pcxgray.dev pcx16.dev pcx256.dev pcx24b.dev pcxcmyk.dev
DEVICE_DEVS9=pbm.dev pbmraw.dev pgm.dev pgmraw.dev pgnm.dev pgnmraw.dev pnm.dev pnmraw.dev ppm.dev ppmraw.dev
DEVICE_DEVS10=tiffcrle.dev tiffg3.dev tiffg32d.dev tiffg4.dev tifflzw.dev tiffpack.dev
DEVICE_DEVS11=bmpmono.dev bmp16.dev bmp256.dev bmp16m.dev tiff12nc.dev tiff24nc.dev
DEVICE_DEVS12=psmono.dev psgray.dev bit.dev bitrgb.dev bitcmyk.dev
DEVICE_DEVS13=pngmono.dev pnggray.dev png16.dev png256.dev png16m.dev
DEVICE_DEVS14=jpeg.dev jpeggray.dev
DEVICE_DEVS15=pdfwrite.dev

# Include the generic makefiles.
!include "version.mak"
!include "gs.mak"
!include "lib.mak"
!include "jpeg.mak"
# zlib.mak must precede libpng.mak
!include "zlib.mak"
!include "libpng.mak"
!include "devs.mak"
!include "pcwin.mak"
!include "contrib.mak"
!include "int.mak"

# -------------------------------- Library -------------------------------- #

# The GCC/EMX platform

os2__=$(GLOBJ)gp_getnv.$(OBJ) $(GLOBJ)gp_nofb.$(OBJ) $(GLOBJ)gp_os2.$(OBJ)
os2_.dev: $(os2__) nosync.dev
	$(SETMOD) os2_ $(os2__) -include nosync
!if $(MAKEDLL)
# Using a file device resource to get the console streams re-initialized 
# is bad architecture (an upward reference to ziodev),                   
# but it will have to do for the moment.                                 
#   We need to redirect stdin/out/err to gsdll_callback
        $(ADDMOD) os2_ -iodev wstdio                                   
!endif
  

$(GLOBJ)gp_os2.$(OBJ): $(GSLRC)gp_os2.c\
 $(dos__h) $(pipe__h) $(string__h) $(time__h)\
 $(gsdll_h) $(gx_h) $(gsexit_h) $(gsutil_h) $(gp_h)
	$(GLCC) $(GLO_)gp_os2.$(OBJ) $(C_) $(GLSRC)gp_os2.c

# -------------------------- Auxiliary programs --------------------------- #

CCAUX=$(COMPDIR)\$(COMP) $(CO)

$(ECHOGS_XE): echogs.c
!if $(EMX)
	$(CCAUX) -o $(AUXGEN)echogs echogs.c
	$(COMPDIR)\emxbind $(EMXPATH)/bin/emxl.exe $(AUXGEN)echogs $(ECHOGS_XE)
	del $(AUXGEN)echogs
!endif
!if $(IBMCPP)
	$(CCAUX) /Fe$(ECHOGS_XE) echogs.c
!endif

$(GENARCH_XE): genarch.c $(stdpre_h)
!if $(EMX)
	$(CCAUX) -o $(AUXGEN)genarch genarch.c
	$(COMPDIR)\emxbind $(EMXPATH)/bin/emxl.exe $(AUXGEN)genarch $(GENARCH_XE)
	del $(AUXGEN)genarch
!endif
!if $(IBMCPP)
	$(CCAUX) /Fe$(GENARCH_XE) genarch.c
!endif

$(GENCONF_XE): genconf.c $(stdpre_h)
!if $(EMX)
	$(CCAUX) -o $(AUXGEN)genconf genconf.c
	$(COMPDIR)\emxbind $(EMXPATH)/bin/emxl.exe $(AUXGEN)genconf $(GENCONF_XE)
	del $(AUXGEN)genconf
!endif
!if $(IBMCPP)
	$(CCAUX) /Fe$(GENCONF_XE) genconf.c
!endif

$(GENDEV_XE): gendev.c $(stdpre_h)
!if $(EMX)
	$(CCAUX) -o $(AUXGEN)gendev gendev.c
	$(COMPDIR)\emxbind $(EMXPATH)/bin/emxl.exe $(AUXGEN)gendev $(GENDEV_XE)
	del $(AUXGEN)gendev
!endif
!if $(IBMCPP)
	$(CCAUX) /Fe$(GENDEV_XE) gendev.c
!endif

$(GENINIT_XE): $(PSSRC)geninit.c $(stdio__h) $(string__h)
!if $(EMX)
	$(CCAUX) -o $(AUXGEN)geninit $(PSSRC)geninit.c
	$(COMPDIR)\emxbind $(EMXPATH)/bin/emxl.exe $(AUXGEN)geninit $(GENINIT_XE)
	del $(AUXGEN)geninit
!endif
!if $(IBMCPP)
	$(CCAUX) /Fe$(GENINIT_XE) geninit.c
!endif

# No special gconfig_.h is needed.
$(gconfig__h): $(MAKEFILE) $(ECHOGS_XE)
	$(ECHOGS_XE) -w $(gconfig__h) /* This file deliberately left blank. */

$(gconfigv_h): os2.mak $(MAKEFILE) $(ECHOGS_XE)
	$(ECHOGS_XE) -w $(gconfigv_h) -x 23 define USE_ASM -x 2028 -q $(USE_ASM)-0 -x 29
	$(ECHOGS_XE) -a $(gconfigv_h) -x 23 define USE_FPU -x 2028 -q $(FPU_TYPE)-0 -x 29
	$(ECHOGS_XE) -a $(gconfigv_h) -x 23 define EXTEND_NAMES 0$(EXTEND_NAMES)
	$(ECHOGS_XE) -a $(gconfigv_h) -x 23 define SYSTEM_CONSTANTS_ARE_WRITABLE 0$(SYSTEM_CONSTANTS_ARE_WRITABLE)

# ----------------------------- Main program ------------------------------ #

# Interpreter main program

ICONS=gsos2.ico gspmdrv.ico

!if $(MAKEDLL)
#making a DLL
GS_ALL=$(GLOBJ)gsdll.$(OBJ) $(INT_ALL) $(INTASM)\
  $(LIB_ALL) $(LIBCTR) $(ld_tr) lib.tr $(GS).res $(ICONS)

$(GS_XE): $(GSDLL).dll dpmainc.c $(gsdll_h) gsos2.rc gscdefs.$(OBJ)
!if $(EMX)
	$(COMPDIR)\gcc $(CGDB) $(CO) -Zomf -o$(GS_XE) dpmainc.c gscdefs.$(OBJ) gsos2.def
!endif
!if $(IBMCPP)
	$(CCAUX) /Fe$(GX_XE) dpmainc.c gscdefs.$(OBJ)
!endif
	rc gsos2.res $(GS_XE)

$(GLOBJ)gsdll.$(OBJ): $(GLSRC)gsdll.c $(gsdll_h) $(ghost_h)
	$(PSCC) $(GLO_)gsdll.$(OBJ) $(C_) $(GLSRC)gsdll.c

$(GSDLL).dll: $(GS_ALL) $(ALL_DEVS) $(GLOBJ)gsdll.$(OBJ)
!if $(EMX)
	LINK386 /DEBUG $(COMPBASE)\lib\dll0.obj $(COMPBASE)\lib\end.lib @$(ld_tr) $(GLOBJ)gsdll.obj, $(GSDLL).dll, ,$(COMPBASE)\lib\gcc.lib $(COMPBASE)\lib\st\c.lib $(COMPBASE)\lib\st\c_dllso.lib $(COMPBASE)\lib\st\sys.lib $(COMPBASE)\lib\c_alias.lib $(COMPBASE)\lib\os2.lib, gsdll2.def
!endif
!if $(IBMCPP)
	LINK386 /NOE /DEBUG @$(ld_tr) $(GLOBJ)gsdll.obj, $(GSDLL).dll, , , gsdll2.def
!endif

!else
#making an EXE
GS_ALL=$(GLOBJ)gs.$(OBJ) $(INT_ALL) $(INTASM)\
  $(LIB_ALL) $(LIBCTR) $(ld_tr) lib.tr $(GS).res $(ICONS)

$(GS_XE): $(GS_ALL) $(ALL_DEVS)
	$(COMPDIR)\gcc $(CGDB) -o $(GS) $(GLOBJ)gs.$(OBJ) @$(ld_tr) $(INTASM) -lm
	$(COMPDIR)\emxbind -r$*.res $(COMPDIR)\emxl.exe $(GS) $(GS_XE) -ac
	del $(GS)
!endif

# Make the icons from their text form.

gsos2.ico: gsos2.icx $(ECHOGS_XE)
	$(ECHOGS_XE) -wb gsos2.ico -n -X -r gsos2.icx

gspmdrv.ico: gspmdrv.icx $(ECHOGS_XE)
	$(ECHOGS_XE) -wb gspmdrv.ico -n -X -r gspmdrv.icx

$(GS).res: $(GS).rc gsos2.ico
	rc -i $(COMPBASE)\include -r $*.rc

# PM driver program

gspmdrv.o: gspmdrv.c $(GLSRC)gspmdrv.h
	$(COMPDIR)\gcc $(CGDB) $(CO) -c $*.c

gspmdrv.res: gspmdrv.rc $(GLSRC)gspmdrv.h gspmdrv.ico
	rc -i $(COMPBASE)\include -r $*.rc

gspmdrv.exe: gspmdrv.o gspmdrv.res gspmdrv.def
	$(COMPDIR)\gcc $(CGDB) $(CO) -o $* $*.o
	$(COMPDIR)\emxbind -p -r$*.res -d$*.def $(COMPDIR)\emxl.exe $* $*.exe
	del $*
