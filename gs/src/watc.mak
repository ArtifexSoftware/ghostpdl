#    Copyright (C) 1991, 1995, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
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


# makefile for MS-DOS/Watcom C386 platform.
# We strongly recommend that you read the Watcom section of Make.htm
# before attempting to build Ghostscript with the Watcom compiler.

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
# see the "File searching" section of Use.htm for full details.
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

# Setting TDEBUG=1 includes symbol table information for the Watcom debugger.
# (This option is NOT needed for using the Watcom profiler.)
# Code runs substantially slower, because some optimizations are disabled.

TDEBUG=0

# Setting NOPRIVATE=1 makes private (static) procedures and variables public,
# so they are visible to the debugger and profiler.
# No execution time or space penalty, just larger .OBJ and .EXE files.

NOPRIVATE=0

# Define the name of the executable file.

GS=gs386

# Define the source, generated intermediate file, and object directories
# for the graphics library (GL) and the PostScript/PDF interpreter (PS).

GLSRCDIR=..\gs
GLGENDIR=..\gs\obj
GLOBJDIR=..\gs\obj
PSSRCDIR=..\gs
PSGENDIR=..\gs\obj
PSOBJDIR=..\gs\obj

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

# Define any other compilation flags.  Including -DA4 makes A4 paper size
# the default for most, but not, printer drivers.

CFLAGS=

# ------ Platform-specific options ------ #

# Define which version of Watcom C we are using.
# Possible values are 8.5, 9.0, 9.5, 10.0, 10.5, or 11.0.
# Unfortunately, wmake can only test identity, not compare magnitudes,
# so the version must be exactly one of those strings.
#
# 10.695 equates to version 10.6 on 95 or NT
WCVERSION=10.695

# Define the locations of the libraries.
LIBPATHS=LIBPATH $(%WATCOM)\lib386 LIBPATH $(%WATCOM)\lib386\dos

# Choose platform-specific options.

# Define the processor (CPU) type.  Options are 386,
# 485 (486SX or Cyrix 486SLC), 486 (486DX), or 586 (Pentium).
# Currently the only difference is that 486 and above assume
# the presence of a FPU, and the other processor types do not.

CPU_TYPE=586

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

# Define the platform name.

PLATFORM=watc_

# Define the name of the makefile -- used in dependencies.

MAKEFILE=$(GLSRCDIR)\watc.mak

# Define additional platform compilation flags.

PLATOPT=

!include $(GLSRCDIR)\wccommon.mak

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
# devs.mak and contrib.mak for the list of available devices.

###	DEVICE_DEVS=vga.dev ega.dev svga16.dev
###	DEVICE_DEVS1=atiw.dev tseng.dev tvga.dev
DEVICE_DEVS3=deskjet.dev djet500.dev laserjet.dev ljetplus.dev ljet2p.dev ljet3.dev ljet4.dev
DEVICE_DEVS4=cdeskjet.dev cdjcolor.dev cdjmono.dev cdj550.dev pj.dev pjxl.dev pjxl300.dev
DEVICE_DEVS5=uniprint.dev
DEVICE_DEVS6=epson.dev eps9high.dev ibmpro.dev bj10e.dev bj200.dev bjc600.dev bjc800.dev
DEVICE_DEVS8=pcxmono.dev pcxgray.dev pcx16.dev pcx256.dev pcx24b.dev pcxcmyk.dev
DEVICE_DEVS10=tiffcrle.dev tiffg3.dev tiffg32d.dev tiffg4.dev tifflzw.dev tiffpack.dev
DEVICE_DEVS11=bmpmono.dev bmp16.dev bmp256.dev bmp16m.dev tiff12nc.dev tiff24nc.dev
DEVICE_DEVS12=psmono.dev psgray.dev bit.dev bitrgb.dev bitcmyk.dev
DEVICE_DEVS14=jpeg.dev jpeggray.dev
DEVICE_DEVS15=pdfwrite.dev

!include $(GLSRCDIR)\wctail.mak
!include $(GLSRCDIR)\devs.mak
!include $(GLSRCDIR)\contrib.mak
!include $(PSSRCDIR)\int.mak

# -------------------------------- Library -------------------------------- #

# make sure the target directories exist - use special Watcom .BEFORE
.BEFORE
	@if not exist $(GLGENDIR) mkdir $(GLGENDIR)
	@if not exist $(GLOBJDIR) mkdir $(GLOBJDIR)
	@if not exist $(PSGENDIR) mkdir $(PSGENDIR)
	@if not exist $(PSOBJDIR) mkdir $(PSOBJDIR)

$(GLOBJ)gp_ntfs.$(OBJ): $(GLSRC)gp_ntfs.c $(AK)\
 $(dos__h) $(memory__h) $(stdio__h) $(string__h) $(windows__h)\
 $(gp_h) $(gsmemory_h) $(gsstruct_h) $(gstypes_h) $(gsutil_h)
	$(GLCC) $(GLO_)gp_ntfs.$(OBJ) $(C_) $(GLSRC)gp_ntfs.c

$(GLOBJ)gp_win32.$(OBJ): $(GLSRC)gp_win32.c $(AK)\
 $(dos__h) $(stdio__h) $(string__h) $(windows__h)\
 $(gp_h) $(gsmemory_h) $(gstypes_h)
	$(GLCC) $(GLO_)gp_win32.$(OBJ) $(C_) $(GLSRC)gp_win32.c

# The Watcom C platform

!ifeq WAT32 0
watc_1=$(GLOBJ)gp_getnv.$(OBJ) $(GLOBJ)gp_iwatc.$(OBJ) $(GLOBJ)gp_msdos.$(OBJ)
watc_2=$(GLOBJ)gp_dosfb.$(OBJ) $(GLOBJ)gp_dosfs.$(OBJ) $(GLOBJ)gp_dosfe.$(OBJ)
watc__=$(watc_1) $(watc_2)
watc_.dev: $(watc__) nosync.dev
	$(SETMOD) watc_ $(watc_1)
	$(ADDMOD) watc_ -obj $(watc_2)
	$(ADDMOD) watc_ -include nosync
!else
watc_1=$(GLOBJ)gp_getnv.$(OBJ) $(GLOBJ)gp_iwatc.$(OBJ) $(GLOBJ)gp_win32.$(OBJ)
watc_2=$(GLOBJ)gp_dosfb.$(OBJ) $(GLOBJ)gp_ntfs.$(OBJ) $(GLOBJ)gxsync.$(OBJ)
watc__=$(watc_1) $(watc_2)
watc_.dev: $(watc__)
	$(SETMOD) watc_ $(watc_1)
	$(ADDMOD) watc_ -obj $(watc_2)
!endif

$(GLOBJ)gp_iwatc.$(OBJ): $(GLSRC)gp_iwatc.c $(stat__h) $(string__h)\
 $(gx_h) $(gp_h)
	$(GLCC) $(GLO_)gp_iwatc.$(OBJ) $(C_) $(GLSRC)gp_iwatc.c

# ----------------------------- Main program ------------------------------ #

BEGINFILES=*.err

LIBDOS=$(LIB_ALL) $(watc__) $(ld_tr)

# Interpreter main program

GS_ALL=$(GLOBJ)gs.$(OBJ) $(INT_ALL) $(INTASM) $(LIBDOS)

ll_tr=$(GLGENDIR)ll$(CONFIG).tr
$(ll_tr): $(MAKEFILE)
	echo OPTION STACK=64k >$(ll_tr)
!ifeq WAT32 0
	echo SYSTEM DOS4G >>$(ll_tr)
	echo OPTION STUB=$(STUB) >>$(ll_tr)
!endif

$(GS_XE): $(GS_ALL) $(DEVS_ALL) $(ll_tr)
	$(LINK) $(LCT) NAME $(GS) OPTION MAP=$(GS) FILE $(GLOBJ)gs @$(ld_tr) @$(ll_tr)
