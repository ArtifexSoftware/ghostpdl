#    Copyright (C) 1997 Aladdin Enterprises. All rights reserved.
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

# makefile for OpenVMS VAX and Alpha
#
# Please contact Jim Dunham (dunham@omtool.com) if you have questions.
#
# ------------------------------- Options ------------------------------- #

###### This section is the only part of the file you should need to edit.

# on the make command line specify:
#	make -fopenvms.mak "OPENVMS={VAX,ALPHA}" "DECWINDOWS={1.2,<blank>}"

# ------ Generic options ------ #

# Define the directory that will hold documentation at runtime.

GS_DOCDIR=GS_DOC
#GS_DOCDIR=SYS$COMMON:[GS]

# Define the default directory/ies for the runtime
# initialization and font files.  Separate multiple directories with ,.

GS_LIB_DEFAULT=GS_LIB
#GS_LIB_DEFAULT=SYS$COMMON:[GS],SYS$COMMON:[GS.FONT]

# Define whether or not searching for initialization files should always
# look in the current directory first.  This leads to well-known security
# and confusion problems, but users insist on it.
# NOTE: this also affects searching for files named on the command line:
# see the "File searching" section of use.txt for full details.
# Because of this, setting SEARCH_HERE_FIRST to 0 is not recommended.

SEARCH_HERE_FIRST=1

# Define the name of the interpreter initialization file.
# (There is no reason to change this.)

GS_INIT=GS_INIT.PS

# Choose generic configuration options.

# Setting DEBUG=1 includes debugging features in the code

DEBUG=

# Setting TDEBUG=1 includes symbol table information for the debugger,
# and also enables stack tracing on failure.

TDEBUG=

# Setting CDEBUG=1 enables 'C' compiler debugging and turns off optimization
# Code is substantially slower and larger.

CDEBUG=

# Define the name of the executable file.

GS=GS

# Define the directory where the IJG JPEG library sources are stored,
# and the major version of the library that is stored there.
# You may need to change this if the IJG library version changes.
# See jpeg.mak for more information.

JSRCDIR=[.jpeg-6a]
JVERSION=6

# Define the directory where the PNG library sources are stored,
# and the version of the library that is stored there.
# You may need to change this if the libpng version changes.
# See libpng.mak for more information.

PSRCDIR=[.libpng-0_96]
PVERSION=96

# Define the directory where the zlib sources are stored.
# See zlib.mak for more information.

ZSRCDIR=[.zlib-1_0_4]

# Note that built-in libpng and zlib aren't available.

SHARE_LIBPNG=0
SHARE_ZLIB=0

# Define the configuration ID.  Read gs.mak carefully before changing this.

CONFIG=

# Define the path to X11 include files

X_INCLUDE=DECW$$INCLUDE

# ------ Platform-specific options ------ #

# Define the drive, directory, and compiler name for the 'C' compiler.
# COMP is the full compiler path name.

COMP=CC

ifdef DEBUG
COMP:=$(COMP)/DEBUG/NOOPTIMIZE
else
COMP:=$(COMP)/NODEBUG/OPTIMIZE
endif

ifeq "$(OPENVMS)"	"VAX"
COMP:=$(COMP)/VAXC
else
COMP:=$(COMP)/DECC/PREFIX=ALL/NESTED_INCLUDE=PRIMARY
endif

# Define any other compilation flags. 
# Including defines for A4 paper size

ifdef A4_PAPER
COMP:=$(COMP)/DEFINE=("A4")
endif

# LINK is the full linker path name

ifdef LDEBUG
LINKER=LINK/DEBUG/TRACEBACK
else
LINKER=LINK/NODEBUG/NOTRACEBACK
endif

# INCDIR contains the include files
INCDIR=

# LIBDIR contains the library files
LIBDIR=

# ------ Devices and features ------ #

# Choose the device(s) to include.  See devs.mak for details.

DEVICE_DEVS=x11.dev x11alpha.dev x11cmyk.dev x11mono.dev
DEVICE_DEVS3=deskjet.dev djet500.dev laserjet.dev ljetplus.dev ljet2p.dev ljet3.dev ljet4.dev
DEVICE_DEVS4=cdeskjet.dev cdjcolor.dev cdjmono.dev cdj550.dev pj.dev pjxl.dev pjxl300.dev
DEVICE_DEVS5=uniprint.dev
DEVICE_DEVS6=bj10e.dev bj200.dev bjc600.dev bjc800.dev
DEVICE_DEVS7=faxg3.dev faxg32d.dev faxg4.dev
DEVICE_DEVS8=pcxmono.dev pcxgray.dev pcx16.dev pcx256.dev pcx24b.dev pcxcmyk.dev
DEVICE_DEVS9=pbm.dev pbmraw.dev pgm.dev pgmraw.dev pgnm.dev pgnmraw.dev pnm.dev pnmraw.dev ppm.dev ppmraw.dev
DEVICE_DEVS10=tiffcrle.dev tiffg3.dev tiffg32d.dev tiffg4.dev tifflzw.dev tiffpack.dev
DEVICE_DEVS11=tiff12nc.dev tiff24nc.dev
DEVICE_DEVS12=psmono.dev psgray.dev bit.dev bitrgb.dev bitcmyk.dev
DEVICE_DEVS13=pngmono.dev pnggray.dev png16.dev png256.dev png16m.dev
DEVICE_DEVS14=jpeg.dev jpeggray.dev
DEVICE_DEVS15=pdfwrite.dev

# Choose the language feature(s) to include.  See gs.mak for details.

FEATURE_DEVS=level2.dev pdf.dev

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

# Define the name table capacity size of 2^(16+n).

EXTEND_NAMES=0

# Define the platform name.

PLATFORM=openvms_

# Define the name of the makefile -- used in dependencies.

MAKEFILE=openvms.mak

# Define the platform options

PLATOPT=

# Patch a couple of PC-specific things that aren't relevant to OpenVMS builds,
# but that cause `make' to produce warnings.

BGIDIR=***UNUSED***
PCFBASM=

# It is very unlikely that anyone would want to edit the remaining
#   symbols, but we describe them here for completeness:

# Define the suffix for command files (e.g., null or .bat).

CMD=

# Define the directory separator character (\ for MS-DOS, / for Unix,
# nothing for OpenVMS).

D=

# Define the string for specifying the output file from the C compiler.

O=/OBJECT=

# Define the extension for executable files (e.g., null or .exe).

XE=.exe

# Define the extension for executable files for the auxiliary programs
# (e.g., null or .exe).

XEAUX=.exe

# Define the list of files that `make begin' and `make clean' remove.

BEGINFILES=OPENVMS.OPT OPENVMS.COM

# Define the C invocation for the ansi2knr program.  We don't use this.

CCA2K=

# Define the C invocation for auxiliary programs (echogs, genarch).
# We don't need to define this separately.

CCAUX=

# Define the compilation command for `make begin'.  We don't use this.

CCBEGIN=

# Define the C invocation for normal compilation.

CC=$(COMP)/OBJECT=$@ $<

# Define the Link invocation.

LINK=$(LINKER)/MAP/EXE=$@ $^,OPENVMS.OPT/OPTION

# Define the ANSI-to-K&R dependency.  We don't need this.

AK=

# Define the syntax for command, object, and executable files.

OBJ=obj

# Define the current directory prefix for image invocations.

EXPP=
EXP=MCR []

# Define the current directory prefix for shell invocations.

SH=
SHP=

# Define generic commands.

CP_=$$ @COPY_ONE

# Define the command for deleting (a) file(s) (including wild cards)

RM_=$$ @RM_ONE

# Define the command for deleting multiple files / patterns.

RMN_=$$ @RM_ALL

# Define the arguments for genconf.

CONFILES=-p %s -o $(ld_tr)

# Define the generic compilation rules.

.suffixes: .c .obj .exe

.c.obj:
	$(CC)

.obj.exe:
	$(LINK)

# ---------------------------- End of options ---------------------------- #


# ------------------- Include the generic makefiles ---------------------- #

include version.mak
include gs.mak
include lib.mak
include jpeg.mak
include libpng.mak
include zlib.mak
include devs.mak
include int.mak

# Define various incantations of the 'c' compiler.

CCC=$(COMP)/OBJECT=$@ 
CCCF=$(CCC)
CCCJ=$(CCC)/INCLUDE=($(JSRCDIR))
CCCZ=$(CCC)/INCLUDE=($(ZSRCDIR))
CCCP=$(CCC)/INCLUDE=($(ZSRCDIR),$(PSRCDIR))
CCINT=$(CCC)
CCLEAF=$(CCC)

# ----------------------------- Main program ------------------------------ #

$(GS_XE): openvms gs.$(OBJ) $(INT_ALL) $(LIB_ALL)
	$(LINKER)/MAP/EXE=$@ gs.$(OBJ),$(ld_tr)/OPTIONS,OPENVMS.OPT/OPTION

# OpenVMS.dev

openvms__=gp_vms.$(OBJ) gp_nofb.$(OBJ)
openvms_.dev: $(openvms__)
	$(SETMOD) openvms_ $(openvms__)

# Interpreter AUX programs

$(ECHOGS_XE):  echogs.$(OBJ) 
$(GENARCH_XE): genarch.$(OBJ)
$(GENCONF_XE): genconf.$(OBJ)
$(GENINIT_XE): geninit.$(OBJ)

# Preliminary definitions

openvms: openvms.com openvms.opt
	$$ @OPENVMS

openvms.com: append_l.com
	$$ @APPEND_L $@ "$$ DEFINE/JOB X11 $(X_INCLUDE)"
	$$ @APPEND_L $@ "$$ DEFINE/JOB GS_LIB ''F$$ENVIRONMENT(""DEFAULT"")'"
	$$ @APPEND_L $@ "$$ DEFINE/JOB GS_DOC ''F$$ENVIRONMENT(""DEFAULT"")'"
ifeq "$(OPENVMS)" "VAX"
	$$ @APPEND_L $@ "$$ DEFINE/JOB C$$INCLUDE ''F$$ENVIRONMENT(""DEFAULT"")', DECW$$INCLUDE, SYS$$LIBRARY"
	$$ @APPEND_L $@ "$$ DEFINE/JOB VAXC$$INCLUDE C$$INCLUDE"
	$$ @APPEND_L $@ "$$ DEFINE/JOB SYS SYS$$LIBRARY"
else
	$$ @APPEND_L $@ "$$ DEFINE/JOB DECC$$USER_INCLUDE ''F$$ENVIRONMENT(""DEFAULT"")', DECW$$INCLUDE, DECC$$LIBRARY_INCLUDE, SYS$$LIBRARY"
	$$ @APPEND_L $@ "$$ DEFINE/JOB DECC$$SYSTEM_INCLUDE ''F$$ENVIRONMENT(""DEFAULT"")', DECW$$INCLUDE, DECC$$LIBRARY_INCLUDE, SYS$$LIBRARY"
	$$ @APPEND_L $@ "$$ DEFINE/JOB SYS "DECC$$LIBRARY_INCLUDE,SYS$$LIBRARY"
endif

openvms.opt:
ifeq "$(OPENVMS)" "VAX"
	$$ @APPEND_L $@ "SYS$$SHARE:VAXCRTL.EXE/SHARE"
endif
ifeq "$(DECWINDOWS)" "1.2"
	$$ @APPEND_L $@ "SYS$$SHARE:DECW$$XMLIBSHR12.EXE/SHARE"
	$$ @APPEND_L $@ "SYS$$SHARE:DECW$$XTLIBSHRR5.EXE/SHARE"
	$$ @APPEND_L $@ "SYS$$SHARE:DECW$$XLIBSHR.EXE/SHARE"
else
	$$ @APPEND_L $@ "SYS$$SHARE:DECW$$XMLIBSHR.EXE/SHARE"
	$$ @APPEND_L $@ "SYS$$SHARE:DECW$$XTSHR.EXE/SHARE"
	$$ @APPEND_L $@ "SYS$$SHARE:DECW$$XLIBSHR.EXE/SHARE"
endif
	$$ @APPEND_L $@ ""Ident="""""GS $(GS_DOT_VERSION)"""""

# The platform-specific makefiles must also include rules for creating
# certain dynamically generated files:
#	gconfig_.h - this indicates the presence or absence of
#	    certain system header files that are located in different
#	    places on different systems.  (It could be generated by
#	    the GNU `configure' program.)
#	gconfigv.h - this indicates the status of certain machine-
#	    and configuration-specific features derived from definitions
#	    in the platform-specific makefile.

gconfig_.h: $(MAKEFILE) $(ECHOGS_XE)
	$(EXP)echogs -w gconfig_.h -x 23 define "HAVE_SYS_TIME_H"

gconfigv.h: $(MAKEFILE) $(ECHOGS_XE)
	$(EXP)echogs -w gconfigv.h -x 23 define "USE_ASM" 0
	$(EXP)echogs -a gconfigv.h -x 23 define "USE_FPU" 1
	$(EXP)echogs -a gconfigv.h -x 23 define "EXTEND_NAMES" 0$(EXTEND_NAME)
