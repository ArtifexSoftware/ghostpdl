#    Copyright (C) 1997, 1998 Aladdin Enterprises.  All rights reserved.
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

# Makefile fragment containing the current revision identification.

# Define the name of this makefile.
VERSION_MAK=version.mak

# Major and minor version numbers.
# MINOR0 is different from MINOR only if MINOR is a single digit.
GS_VERSION_MAJOR=5
GS_VERSION_MINOR=13
GS_VERSION_MINOR0=13
# Revision date: year x 10000 + month x 100 + day.
GS_REVISIONDATE=19980427

# Derived values
GS_VERSION=$(GS_VERSION_MAJOR)$(GS_VERSION_MINOR0)
GS_DOT_VERSION=$(GS_VERSION_MAJOR).$(GS_VERSION_MINOR)
GS_REVISION=$(GS_VERSION)
#    Copyright (C) 1994, 1995, 1996, 1997 Aladdin Enterprises.  All rights reserved.
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

# makefile for DesqView/X/gcc/X11 configuration.
# Note: this makefile assumes you are using gcc in ANSI mode.

#****************************************************************#
#   If you want to change options, DO NOT edit dvx-gcc.mak       #
#   or makefile.  Edit dgc-head.mak and run the tar_cat script.  #
#****************************************************************#

# ------------------------------- Options ------------------------------- #

####### The following are the only parts of the file you should need to edit.

# ------ Generic options ------ #

# Define the installation commands and target directories for
# executables and files.  The commands are only relevant to `make install';
# the directories also define the default search path for the
# initialization files (gs_*.ps) and the fonts.

INSTALL = install -c
INSTALL_PROGRAM = $(INSTALL) -m 755
INSTALL_DATA = $(INSTALL) -m 644

prefix = c:/bin
bindir = c:/bin
gsdatadir = c:/gs
gsfontdir = c:/gsfonts

docdir=$(gsdatadir)/doc
exdir=$(gsdatadir)/examples
GS_DOCDIR=$(docdir)

# Define the default directory/ies for the runtime
# initialization and font files.  Separate multiple directories with a ;.

GS_LIB_DEFAULT="$(gsdatadir);$(gsfontdir)"

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

# -DDEBUG
#	includes debugging features (-Z switch) in the code.
#	  Code runs substantially slower even if no debugging switches
#	  are set.
# -DNOPRIVATE
#	makes private (static) procedures and variables public,
#	  so they are visible to the debugger and profiler.
#	  No execution time or space penalty.

GENOPT=

# Define the name of the executable file.

GS=gs

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

# Choose whether to use a shared version of the PNG library (-lpng).
# See gs.mak and make.txt for more information.

SHARE_LIBPNG=0

# Define the directory where the zlib sources are stored.
# See zlib.mak for more information.

ZSRCDIR=zlib

# Choose whether to use a shared version of the zlib library (-lgz).
# See gs.mak and make.txt for more information.

SHARE_ZLIB=0

# Define the configuration ID.  Read gs.mak carefully before changing this.

CONFIG=

# ------ Platform-specific options ------ #

# Define the name of the C compiler.

CC=gcc

# Define the other compilation flags.
# Add -DBSD4_2 for 4.2bsd systems.
# Add -DSYSV for System V or DG/UX.
# Add -DSYSV -D__SVR3 for SCO ODT, ISC Unix 2.2 or before,
#   or any System III Unix, or System V release 3-or-older Unix.
# Add -DSVR4 (not -DSYSV) for System V release 4.
# XCFLAGS can be set from the command line.
# We don't include -ansi, because this gets in the way of the platform-
#   specific stuff that <math.h> typically needs; nevertheless, we expect
#   gcc to accept ANSI-style function prototypes and function definitions.
XCFLAGS=

# Under DJGPP, since we strip the executable by default, we may as
# well *not* use '-g'.

# CFLAGS=-g -O $(XCFLAGS)
CFLAGS=-O $(XCFLAGS)

# Define platform flags for ld.
# Ultrix wants -x.
# SunOS 4.n may need -Bstatic.
# XLDFLAGS can be set from the command line.
XLDFLAGS=

LDFLAGS=$(XLDFLAGS)

# Define any extra libraries to link into the executable.
# ISC Unix 2.2 wants -linet.
# SCO Unix needs -lsocket if you aren't including the X11 driver.
# (Libraries required by individual drivers are handled automatically.)

EXTRALIBS=-lsys -lc

# Define the include switch(es) for the X11 header files.
# This can be null if handled in some other way (e.g., the files are
# in /usr/include, or the directory is supplied by an environment variable);
# in particular, SCO Xenix, Unix, and ODT just want
#XINCLUDE=
# Note that x_.h expects to find the header files in $(XINCLUDE)/X11,
# not in $(XINCLUDE).

XINCLUDE=

# Define the directory/ies and library names for the X11 library files.
# The former can be null if these files are in the default linker search path.
# Unfortunately, Quarterdeck's old libraries did not conform to the
# X11 conventions for naming, in that the main Xlib library was called
# libx.a, not libx11.a.  To make things worse, both are provided in
# the v2.00 library.  Creation dates indicate that 'libx.a' is left
# over from a previous build (or this could just be on my system, but
# others who have upgraded from the early version will have the same
# problem---SJT).  Thus I will make the default to look for
# 'libx11.a', since v1.0x does *not* have it and the linker will
# complain.  With the reverse default, the linker will find to the
# obsolete library on some systems.

# XLIBDIRS includes a prefix -L; XLIBDIR does not.
XLIBDIRS=
XLIBDIR=
# reverse the comments if you have QDDVX10x or the prerelease version
# of QDLIB200 (still available on some Simtel mirrors, unfortunately)
# XLIBS=Xt Xext X
XLIBS=Xt Xext X11

# Define whether this platform has floating point hardware:
#	FPU_TYPE=2 means floating point is faster than fixed point.
# (This is the case on some RISCs with multiple instruction dispatch.)
#	FPU_TYPE=1 means floating point is at worst only slightly slower
# than fixed point.
#	FPU_TYPE=0 means that floating point may be considerably slower.
#	FPU_TYPE=-1 means that floating point is always much slower than
# fixed point.

FPU_TYPE=1

# ------ Devices and features ------ #

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

# Choose the device(s) to include.  See devs.mak for details.

DEVICE_DEVS=x11.dev
DEVICE_DEVS1=
DEVICE_DEVS2=
DEVICE_DEVS3=deskjet.dev djet500.dev laserjet.dev ljetplus.dev ljet2p.dev ljet3.dev ljet4.dev
DEVICE_DEVS4=cdeskjet.dev cdjcolor.dev cdjmono.dev cdj550.dev pj.dev pjxl.dev pjxl300.dev
DEVICE_DEVS5=paintjet.dev pjetxl.dev uniprint.dev
DEVICE_DEVS6=
DEVICE_DEVS7=
DEVICE_DEVS8=
DEVICE_DEVS9=pbm.dev pbmraw.dev pgm.dev pgmraw.dev pgnm.dev pgnmraw.dev pnm.dev pnmraw.dev ppm.dev ppmraw.dev
DEVICE_DEVS10=tiffcrle.dev tiffg3.dev tiffg32d.dev tiffg4.dev tifflzw.dev tiffpack.dev
DEVICE_DEVS11=tiff12nc.dev tiff24nc.dev
DEVICE_DEVS12=psmono.dev psgray.dev bit.dev bitrgb.dev bitcmyk.dev
DEVICE_DEVS13=
DEVICE_DEVS14=
DEVICE_DEVS15=

# ---------------------------- End of options --------------------------- #

# Define the name of the partial makefile that specifies options --
# used in dependencies.

MAKEFILE=dgc-head.mak

# Define the ANSI-to-K&R dependency.  (gcc accepts ANSI syntax.)

AK=

# Define the compilation rules and flags.

CCC=$(CC) $(CCFLAGS) -c
CCLEAF=$(CCC) -fomit-frame-pointer

# --------------------------- Generic makefile ---------------------------- #

# The remainder of the makefile (unixhead.mak, gs.mak, devs.mak, unixtail.mak)
# is generic.  tar_cat concatenates all these together.
#    Copyright (C) 1994, 1996 Aladdin Enterprises.  All rights reserved.
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

# Partial makefile, common to all Desqview/X configurations.

# This part of the makefile gets inserted after the compiler-specific part
# (xxx-head.mak) and before gs.mak and devs.mak.

# ----------------------------- Generic stuff ----------------------------- #

# Define the platform name.

PLATFORM=dvx_

# Define the syntax for command, object, and executable files.

CMD=.bat
O=-o ./
OBJ=o
XE=.exe
XEAUX=.exe

# Define the current directory prefix and command invocations.

CAT=type
D=\\
EXP=
SHELL=
SH=
SHP=

# Define generic commands.

CP_=cp
RM_=rm -f

# Define the arguments for genconf.

CONFILES=-p -pl &-l%%s -ol ld.tr

# Define the compilation rules and flags.

CCFLAGS=$(GENOPT) $(CFLAGS)

.c.o: $(AK)
	$(CCC) $*.c

CCCF=$(CCC)
CCD=$(CCC)
CCINT=$(CCC)

# Patch a couple of PC-specific things that aren't relevant to DV/X builds,
# but that cause `make' to produce warnings.

BGIDIR=***UNUSED***
PCFBASM=
#    Copyright (C) 1989, 1996, 1997 Aladdin Enterprises.  All rights reserved.
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

# Generic makefile, common to all platforms.
# The platform-specific makefiles `include' this file.
# They define the following symbols:
#	GS - the name of the executable (without the extension, if any).
#	GS_LIB_DEFAULT - the default directory/ies for searching for the
#	    initialization and font files at run time.
#	SEARCH_HERE_FIRST - the default setting of -P (whether or not to
#	    look for files in the current directory first).
#	GS_DOCDIR - the directory where documentation will be available
#	    at run time.
#	JSRCDIR - the directory where the IJG JPEG library source code
#	    is stored (at compilation time).
#	JVERSION - the major version number of the IJG JPEG library.
#	PSRCDIR, PVERSION - the same for libpng.
#	ZSRCDIR - the same for zlib.
#	SHARE_LIBPNG - normally 0; if set to 1, asks the linker to use
#	    an existing compiled libpng (-lpng) instead of compiling and
#	    linking libpng explicitly.
#	LIBPNG_NAME, the name of the shared libpng, currently always
#	    png (libpng, -lpng).
#	SHARE_ZLIB - normally 0; if set to 1, asks the linker to use
#	    an existing compiled zlib (-lgz or -lz) instead of compiling
#	    and linking libgz/libz explicitly.
#	ZLIB_NAME - the name of the shared zlib, either gz (for libgz, -lgz)
#	    or z (for libz, -lz).
#	CONFIG - a configuration ID, added at the request of a customer,
#	    that is supposed to help in maintaining multiple variants in
#	    a single directory.  Normally this is an empty string;
#	    it may be any string that is legal as part of a file name.
#	DEVICE_DEVS - the devices to include in the executable.
#	    See devs.mak for details.
#	DEVICE_DEVS1...DEVICE_DEVS15 - additional devices, if the definition
#	    of DEVICE_DEVS doesn't fit on one line.  See devs.mak for details.
#	FEATURE_DEVS - what features to include in the executable.
#	    Normally this is one of:
#		    level1 - a standard PostScript Level 1 language
#			interpreter.
#		    level2 - a standard PostScript Level 2 language
#			interpreter.
#		    pdf - a PDF-capable interpreter.
#	    You may include both level1 and pdf, or both level2 and pdf.
#	    The following feature may be added to either of the standard
#		configurations:
#		    ccfonts - precompile fonts into C, and link them
#			with the executable.  See fonts.txt for details.
#	    The remaining features are of interest primarily to developers
#		who want to "mix and match" features to create custom
#		configurations:
#		    dps - (partial) support for Display PostScript extensions:
#			see language.txt for details.
#		    btoken - support for binary token encodings.
#			Included automatically in the dps and level2 features.
#		    cidfont - (currently partial) support for CID-keyed fonts.
#		    color - support for the Level 1 CMYK color extensions.
#			Included automatically in the dps and level2 features.
#		    compfont - support for composite (type 0) fonts.
#			Included automatically in the level2 feature.
#		    dct - support for DCTEncode/Decode filters.
#			Included automatically in the level2 feature.
#		    epsf - support for recognizing and skipping the binary
#			header of MS-DOS EPSF files.
#		    filter - support for Level 2 filters (other than eexec,
#			ASCIIHexEncode/Decode, NullEncode, PFBDecode,
#			RunLengthEncode/Decode, and SubFileDecode, which are
#			always included, and DCTEncode/Decode,
#			which are separate).
#			Included automatically in the level2 feature.
#		    fzlib - support for zlibEncode/Decode filters.
#		    ttfont - support for TrueType fonts.
#		    type1 - support for Type 1 fonts and eexec;
#			normally included automatically in all configurations.
#		    type42 - support for Type 42 (embedded TrueType) fonts.
#			Included automatically in the level2 feature.
#		There are quite a number of other sub-features that can be
#		selectively included in or excluded from a configuration,
#		but the above are the ones that are most likely to be of
#		interest.
#	COMPILE_INITS - normally 0; if set to 1, compiles the PostScript
#	    language initialization files (gs_init.ps et al) into the
#	    executable, eliminating the need for these files to be present
#	    at run time.
#	BAND_LIST_STORAGE - normally file; if set to memory, stores band
#	    lists in memory (with compression if needed).
#	BAND_LIST_COMPRESSOR - normally zlib: selects the compression method
#	    to use for band lists in memory.
#	FILE_IMPLEMENTATION - normally stdio; if set to fd, uses file
#	    descriptors instead of buffered stdio for file I/O; if set to
#	    both, provides both implementations with different procedure
#	    names for the fd-based implementation (see sfxfd.c for
#	    more information).
#	EXTEND_NAMES - a value N between 0 and 6, indicating that the name
#	    table should have a capacity of 2^(16+N) names.  This normally
#	    should be set to 0 (or left undefined), since non-zero values
#	    result in a larger fixed space overhead and slightly slower code.
#	    EXTEND_NAMES is ignored in 16-bit environments.
#
# It is very unlikely that anyone would want to edit the remaining
#   symbols, but we describe them here for completeness:
#	GS_INIT - the name of the initialization file for the interpreter,
#		normally gs_init.ps.
#	PLATFORM - a "device" name for the platform, so that platforms can
#		add various kinds of resources like devices and features.
#	CMD - the suffix for shell command files (e.g., null or .bat).
#		(This is only needed in a few places.)
#	D - the directory separator character (\ for MS-DOS, / for Unix).
#	O - the string for specifying the output file from the C compiler
#		(-o for MS-DOS, -o ./ for Unix).
#	OBJ - the extension for relocatable object files (e.g., o or obj).
#	XE - the extension for executable files (e.g., null or .exe).
#	XEAUX - the extension for the executable files (e.g., null or .exe)
#		for the utility programs (ansi2knr and those compiled with
#		CCAUX).
#	BEGINFILES - the list of files that `make begin' and `make clean'
#		should delete.
#	CCA2K - the C invocation for the ansi2knr program, which is the only
#		one that doesn't use ANSI C syntax.  (It is only needed if
#		the main C compiler also isn't an ANSI compiler.)
#	CCAUX - the C invocation for auxiliary programs (echogs, genarch,
#		genconf, geninit).
#	CCBEGIN - the compilation command for `make begin', normally
#		$(CCC) *.c.
#	CCC - the C invocation for normal compilation.
#	CCD - the C invocation for files that store into frame buffers or
#		device registers.  Needed because some optimizing compilers
#		will eliminate necessary stores.
#	CCCF - the C invocation for compiled fonts and other large,
#		self-contained data modules.  Needed because MS-DOS
#		requires using the 'huge' memory model for these.
#	CCINT - the C invocation for compiling the main interpreter module,
#		normally the same as CCC: this is needed because the
#		Borland compiler generates *worse* code for this module
#		(but only this module) when optimization (-O) is turned on.
#	CCLEAF - the C invocation for compiling modules that contain only
#		leaf procedures, which don't need to build stack frames.
#		This is needed only because many compilers aren't able to
#		recognize leaf procedures on their own.
#	AK - if source files must be converted from ANSI to K&R syntax,
#		this is $(ANSI2KNR_XE); if not, it is null.
#		If a particular platform requires other utility programs
#		to be built, AK must include them too.
#	SHP - the prefix for invoking a shell script in the current directory
#		(null for MS-DOS, $(SH) ./ for Unix).
#	EXPP, EXP - the prefix for invoking an executable program in the
#		current directory (null for MS-DOS, ./ for Unix).
#	SH - the shell for scripts (null on MS-DOS, sh on Unix).
#	CONFILES - the arguments for genconf to generate the appropriate
#		linker control files (various).
#	CP_ - the command for copying one file to another.  Because of
#		limitations in the MS-DOS/MS Windows environment, the
#		second argument must either be '.' (in which case the
#		write date may be either preserved or set to the current
#		date) or a file name (in which case the write date is
#		always updated).
#	RM_ - the command for deleting (a) file(s) (including wild cards,
#		but limited to a single file or pattern).
#	RMN_ = the command for deleting multiple files / patterns.
#
# The platform-specific makefiles must also include rules for creating
# certain dynamically generated files:
#	gconfig_.h - this indicates the presence or absence of
#	    certain system header files that are located in different
#	    places on different systems.  (It could be generated by
#	    the GNU `configure' program.)
#	gconfigv.h - this indicates the status of certain machine-
#	    and configuration-specific features derived from definitions
#	    in the platform-specific makefile.

# Define the name of this makefile.
GS_MAK=gs.mak

# Define the names of the executables.
GS_XE=$(GS)$(XE)
ANSI2KNR_XE=ansi2knr$(XEAUX)
ECHOGS_XE=echogs$(XEAUX)
GENARCH_XE=genarch$(XEAUX)
GENCONF_XE=genconf$(XEAUX)
GENINIT_XE=geninit$(XEAUX)

# Define the names of the CONFIG-dependent header files.
# gconfig*.h and gconfx*.h are generated dynamically.
gconfig_h=gconfxx$(CONFIG).h
gconfigf_h=gconfxc$(CONFIG).h

# Watcom make insists that rules have a non-empty body!
all default: $(GS_XE)
	$(RM_) _temp_*

distclean maintainer-clean realclean: clean
	$(RM_) makefile

clean: mostlyclean
	$(RM_) arch.h
	$(RM_) $(GS_XE)

mostlyclean:
	$(RMN_) *.$(OBJ) *.a core gmon.out
	$(RMN_) *.dev *.d_* devs*.tr gconfig*.h gconfx*.h j*.h o*.tr l*.tr
	$(RMN_) deflate.h zutil.h
	$(RMN_) gconfig*.c gscdefs*.c iconfig*.c
	$(RMN_) _temp_* _temp_*.* *.map *.sym
	$(RMN_) $(ANSI2KNR_XE) $(ECHOGS_XE) $(GENARCH_XE) $(GENCONF_XE) $(GENINIT_XE)
	$(RMN_) gs_init.c $(BEGINFILES)

# Remove only configuration-dependent information.
config-clean:
	$(RMN_) *.dev devs*.tr gconfig*.h gconfx*.h o*.tr l*.tr

# A rule to do a quick and dirty compilation attempt when first installing
# the interpreter.  Many of the compilations will fail:
# follow this with 'make'.

begin:
	$(RMN_) arch.h gconfig*.h gconfx*.h $(GENARCH_XE) $(GS_XE)
	$(RMN_) gconfig*.c gscdefs*.c iconfig*.c
	$(RMN_) gs_init.c $(BEGINFILES)
	make arch.h gconfigv.h
	- $(CCBEGIN)
	$(RMN_) gconfig.$(OBJ) gdev*.$(OBJ) gp_*.$(OBJ) gscdefs.$(OBJ) gsmisc.$(OBJ)
	$(RMN_) icfontab.$(OBJ) iconfig.$(OBJ) iinit.$(OBJ) interp.$(OBJ)

# Auxiliary programs

arch.h: $(GENARCH_XE)
	$(EXPP) $(EXP)genarch arch.h

# Macros for constructing the *.dev files that describe features and
# devices.
SETDEV=$(EXP)echogs -e .dev -w- -l-dev -F -s -l-obj
SETPDEV=$(EXP)echogs -e .dev -w- -l-dev -F -s -l-include -lpage -l-obj
SETMOD=$(EXP)echogs -e .dev -w- -l-obj
ADDMOD=$(EXP)echogs -e .dev -a-

# Define the compilation commands for the third-party libraries.
CCCP=$(CCC) -I$(PSRCDIR) -I$(ZSRCDIR) -DPNG_USE_CONST
CCCJ=$(CCC) -I. -I$(JSRCDIR)
CCCZ=$(CCC) -I. -I$(ZSRCDIR)

######################## How to define new 'features' #######################
#
# One defines new 'features' exactly like devices (see devs.mak for details).
# For example, one would define a feature abc by adding the following to
# gs.mak:
#
#	abc_=abc1.$(OBJ) ...
#	abc.dev: $(GS_MAK) $(ECHOGS_XE) $(abc_)
#		$(SETMOD) abc $(abc_)
#		$(ADDMOD) abc -obj ... [if needed]
#		$(ADDMOD) abc -oper ... [if appropriate]
#		$(ADDMOD) abc -ps ... [if appropriate]
#
# If the abc feature requires the presence of some other features jkl and
# pqr, then the rules must look like this:
#
#	abc_=abc1.$(OBJ) ...
#	abc.dev: $(GS_MAK) $(ECHOGS_XE) $(abc_) jkl.dev pqr.dev
#		$(SETMOD) abc $(abc_)
#		...
#		$(ADDMOD) abc -include jkl pqr

# --------------------- Configuration-dependent files --------------------- #

# gconfig.h shouldn't have to depend on DEVS_ALL, but that would
# involve rewriting gsconfig to only save the device name, not the
# contents of the <device>.dev files.
# FEATURE_DEVS must precede DEVICE_DEVS so that devices can override
# features in obscure cases.

DEVS_ALL=$(PLATFORM).dev $(FEATURE_DEVS) \
  $(DEVICE_DEVS) $(DEVICE_DEVS1) \
  $(DEVICE_DEVS2) $(DEVICE_DEVS3) $(DEVICE_DEVS4) $(DEVICE_DEVS5) \
  $(DEVICE_DEVS6) $(DEVICE_DEVS7) $(DEVICE_DEVS8) $(DEVICE_DEVS9) \
  $(DEVICE_DEVS10) $(DEVICE_DEVS11) $(DEVICE_DEVS12) $(DEVICE_DEVS13) \
  $(DEVICE_DEVS14) $(DEVICE_DEVS15)

devs_tr=devs.tr$(CONFIG)
$(devs_tr): $(GS_MAK) $(MAKEFILE) $(ECHOGS_XE)
	$(EXP)echogs -w $(devs_tr) - -include $(PLATFORM).dev
	$(EXP)echogs -a $(devs_tr) - $(FEATURE_DEVS)
	$(EXP)echogs -a $(devs_tr) - $(DEVICE_DEVS)
	$(EXP)echogs -a $(devs_tr) - $(DEVICE_DEVS1)
	$(EXP)echogs -a $(devs_tr) - $(DEVICE_DEVS2)
	$(EXP)echogs -a $(devs_tr) - $(DEVICE_DEVS3)
	$(EXP)echogs -a $(devs_tr) - $(DEVICE_DEVS4)
	$(EXP)echogs -a $(devs_tr) - $(DEVICE_DEVS5)
	$(EXP)echogs -a $(devs_tr) - $(DEVICE_DEVS6)
	$(EXP)echogs -a $(devs_tr) - $(DEVICE_DEVS7)
	$(EXP)echogs -a $(devs_tr) - $(DEVICE_DEVS8)
	$(EXP)echogs -a $(devs_tr) - $(DEVICE_DEVS9)
	$(EXP)echogs -a $(devs_tr) - $(DEVICE_DEVS10)
	$(EXP)echogs -a $(devs_tr) - $(DEVICE_DEVS11)
	$(EXP)echogs -a $(devs_tr) - $(DEVICE_DEVS12)
	$(EXP)echogs -a $(devs_tr) - $(DEVICE_DEVS13)
	$(EXP)echogs -a $(devs_tr) - $(DEVICE_DEVS14)
	$(EXP)echogs -a $(devs_tr) - $(DEVICE_DEVS15)

# GCONFIG_EXTRAS can be set on the command line.
# Note that it consists of arguments for echogs, i.e.,
# it isn't just literal text.
GCONFIG_EXTRAS=

ld_tr=ld$(CONFIG).tr
$(gconfig_h) $(ld_tr) lib.tr: \
  $(GS_MAK) $(MAKEFILE) version.mak $(GENCONF_XE) $(ECHOGS_XE) $(devs_tr) $(DEVS_ALL) libcore.dev
	$(EXP)genconf $(devs_tr) libcore.dev -h $(gconfig_h) $(CONFILES)
	$(EXP)echogs -a $(gconfig_h) -x 23 define -s -u GS_LIB_DEFAULT -x 2022 $(GS_LIB_DEFAULT) -x 22
	$(EXP)echogs -a $(gconfig_h) -x 23 define -s -u SEARCH_HERE_FIRST -s $(SEARCH_HERE_FIRST)
	$(EXP)echogs -a $(gconfig_h) -x 23 define -s -u GS_DOCDIR -x 2022 $(GS_DOCDIR) -x 22
	$(EXP)echogs -a $(gconfig_h) -x 23 define -s -u GS_INIT -x 2022 $(GS_INIT) -x 22
	$(EXP)echogs -a $(gconfig_h) -x 23 define -s -u GS_REVISION -s $(GS_REVISION)
	$(EXP)echogs -a $(gconfig_h) -x 23 define -s -u GS_REVISIONDATE -s $(GS_REVISIONDATE)
	$(EXP)echogs -a $(gconfig_h) $(GCONFIG_EXTRAS)

################################################################
# The other platform-independent makefiles are concatenated
# (or included) after this one:
#	lib.mak
#	int.mak
#	jpeg.mak
#	libpng.mak
#	zlib.mak
#	devs.mak
################################################################
#    Copyright (C) 1995, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
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

# (Platform-independent) makefile for graphics library and other support code.
# See the end of gs.mak for where this fits into the build process.

# Define the name of this makefile.
LIB_MAK=lib.mak

# Define the inter-dependencies of the .h files.
# Since not all versions of `make' defer expansion of macros,
# we must list these in bottom-to-top order.

# Generic files

arch_h=arch.h
stdpre_h=stdpre.h
std_h=std.h $(arch_h) $(stdpre_h)

# Platform interfaces

gp_h=gp.h
gpcheck_h=gpcheck.h
gpsync_h=gpsync.h

# Configuration definitions

# gconfig*.h are generated dynamically.
gconfig__h=gconfig_.h
gconfigv_h=gconfigv.h
gscdefs_h=gscdefs.h

# C library interfaces

# Because of variations in the "standard" header files between systems, and
# because we must include std.h before any file that includes sys/types.h,
# we define local include files named *_.h to substitute for <*.h>.

vmsmath_h=vmsmath.h

dos__h=dos_.h
ctype__h=ctype_.h $(std_h)
dirent__h=dirent_.h $(std_h) $(gconfig__h)
errno__h=errno_.h $(std_h)
malloc__h=malloc_.h $(std_h)
math__h=math_.h $(std_h) $(vmsmath_h)
memory__h=memory_.h $(std_h)
stat__h=stat_.h $(std_h)
stdio__h=stdio_.h $(std_h)
string__h=string_.h $(std_h)
time__h=time_.h $(std_h) $(gconfig__h)
windows__h=windows_.h

# Miscellaneous

gdebug_h=gdebug.h
gsalloc_h=gsalloc.h
gsargs_h=gsargs.h
gserror_h=gserror.h
gserrors_h=gserrors.h
gsexit_h=gsexit.h
gsgc_h=gsgc.h
gsio_h=gsio.h
gsmdebug_h=gsmdebug.h
gsmemraw_h=gsmemraw.h
gsmemory_h=gsmemory.h $(gsmemraw_h)
gsrefct_h=gsrefct.h
gsstruct_h=gsstruct.h
gstypes_h=gstypes.h
gx_h=gx.h $(stdio__h) $(gdebug_h) $(gserror_h) $(gsio_h) $(gsmemory_h) $(gstypes_h)

GX=$(AK) $(gx_h)
GXERR=$(GX) $(gserrors_h)

###### Support

### Include files

gsbitmap_h=gsbitmap.h $(gsstruct_h)
gsbitops_h=gsbitops.h
gsbittab_h=gsbittab.h
gsflip_h=gsflip.h
gsuid_h=gsuid.h
gsutil_h=gsutil.h
gxarith_h=gxarith.h
gxbitmap_h=gxbitmap.h $(gsbitmap_h) $(gstypes_h)
gxfarith_h=gxfarith.h $(gconfigv_h) $(gxarith_h)
gxfixed_h=gxfixed.h
gxobj_h=gxobj.h $(gxbitmap_h)
# Out of order
gxalloc_h=gxalloc.h $(gsalloc_h) $(gxobj_h)

### Executable code

gsalloc.$(OBJ): gsalloc.c $(GX) $(memory__h) $(string__h) \
  $(gsmdebug_h) $(gsstruct_h) $(gxalloc_h)

gsargs.$(OBJ): gsargs.c $(ctype__h) $(stdio__h) $(string__h)\
 $(gsargs_h) $(gsexit_h) $(gsmemory_h)

gsbitops.$(OBJ): gsbitops.c $(AK) $(memory__h) $(stdio__h)\
 $(gdebug_h) $(gsbitops_h) $(gstypes_h)

gsbittab.$(OBJ): gsbittab.c $(AK) $(stdpre_h) $(gsbittab_h)

# gsfemu is only used in FPU-less configurations, and currently only with gcc.
# We thought using CCLEAF would produce smaller code, but it actually
# produces larger code!
gsfemu.$(OBJ): gsfemu.c $(AK) $(std_h)

# gsflip is not part of the standard configuration: it's rather large,
# and no standard facility requires it.
gsflip.$(OBJ): gsflip.c $(GX) $(gsbittab_h) $(gsflip_h)
	$(CCLEAF) gsflip.c

gsmemory.$(OBJ): gsmemory.c $(GX) $(malloc__h) $(memory__h) \
  $(gsmdebug_h) $(gsrefct_h) $(gsstruct_h) $(gsmemraw_h)

gsmisc.$(OBJ): gsmisc.c $(GXERR) $(gconfigv_h) \
  $(malloc__h) $(math__h) $(memory__h) $(gpcheck_h) $(gxfarith_h) $(gxfixed_h)

# gsnogc currently is only used in library-only configurations.
gsnogc.$(OBJ): gsnogc.c $(GX)\
 $(gsgc_h) $(gsmdebug_h) $(gsstruct_h) $(gxalloc_h)

gsutil.$(OBJ): gsutil.c $(AK) $(memory__h) $(string__h) $(gconfigv_h)\
 $(gstypes_h) $(gsuid_h) $(gsutil_h)

###### Low-level facilities and utilities

### Include files

gdevbbox_h=gdevbbox.h
gdevmem_h=gdevmem.h $(gsbitops_h)
gdevmrop_h=gdevmrop.h

gsccode_h=gsccode.h
gsccolor_h=gsccolor.h $(gsstruct_h)
gscsel_h=gscsel.h
gscolor1_h=gscolor1.h
gscoord_h=gscoord.h
gscpm_h=gscpm.h
gsdevice_h=gsdevice.h
gsfcmap_h=gsfcmap.h $(gsccode_h)
gsfont_h=gsfont.h
gshsb_h=gshsb.h
gsht_h=gsht.h
gsht1_h=gsht1.h $(gsht_h)
gsiparam_h=gsiparam.h
gsjconf_h=gsjconf.h $(std_h)
gslib_h=gslib.h
gslparam_h=gslparam.h
gsmatrix_h=gsmatrix.h
gspaint_h=gspaint.h
gsparam_h=gsparam.h
gsparams_h=gsparams.h $(gsparam_h)
gspath2_h=gspath2.h
gspenum_h=gspenum.h
gsropt_h=gsropt.h
gsxfont_h=gsxfont.h
# Out of order
gschar_h=gschar.h $(gsccode_h) $(gscpm_h)
gscolor2_h=gscolor2.h $(gsccolor_h) $(gsuid_h) $(gxbitmap_h)
gsimage_h=gsimage.h $(gsiparam_h)
gsline_h=gsline.h $(gslparam_h)
gspath_h=gspath.h $(gspenum_h)
gsrop_h=gsrop.h $(gsropt_h)

gxbcache_h=gxbcache.h $(gxbitmap_h)
gxchar_h=gxchar.h $(gschar_h)
gxcindex_h=gxcindex.h
gxcvalue_h=gxcvalue.h
gxclio_h=gxclio.h
gxclip2_h=gxclip2.h
gxcolor2_h=gxcolor2.h $(gscolor2_h) $(gsrefct_h) $(gxbitmap_h)
gxcoord_h=gxcoord.h $(gscoord_h)
gxcpath_h=gxcpath.h
gxdda_h=gxdda.h
gxdevrop_h=gxdevrop.h
gxdevmem_h=gxdevmem.h
gxdither_h=gxdither.h
gxfcmap_h=gxfcmap.h $(gsfcmap_h) $(gsuid_h)
gxfont0_h=gxfont0.h
gxfrac_h=gxfrac.h
gxftype_h=gxftype.h
gxhttile_h=gxhttile.h
gxhttype_h=gxhttype.h
gxiodev_h=gxiodev.h $(stat__h)
gxline_h=gxline.h $(gslparam_h)
gxlum_h=gxlum.h
gxmatrix_h=gxmatrix.h $(gsmatrix_h)
gxpaint_h=gxpaint.h
gxpath_h=gxpath.h $(gscpm_h) $(gslparam_h) $(gspenum_h)
gxpcache_h=gxpcache.h
gxpcolor_h=gxpcolor.h $(gxpcache_h)
gxsample_h=gxsample.h
gxstate_h=gxstate.h
gxtmap_h=gxtmap.h
gxxfont_h=gxxfont.h $(gsccode_h) $(gsmatrix_h) $(gsuid_h) $(gsxfont_h)
# The following are out of order because they include other files.
gsdcolor_h=gsdcolor.h $(gsccolor_h) $(gxarith_h) $(gxbitmap_h) $(gxcindex_h) $(gxhttile_h)
gxdcolor_h=gxdcolor.h $(gscsel_h) $(gsdcolor_h) $(gsropt_h) $(gsstruct_h)
gxdevice_h=gxdevice.h $(stdio__h) $(gsdcolor_h) $(gsiparam_h) $(gsmatrix_h) \
  $(gsropt_h) $(gsstruct_h) $(gsxfont_h) \
  $(gxbitmap_h) $(gxcindex_h) $(gxcvalue_h) $(gxfixed_h)
gxdht_h=gxdht.h $(gsrefct_h) $(gxarith_h) $(gxhttype_h)
gxctable_h=gxctable.h $(gxfixed_h) $(gxfrac_h)
gxfcache_h=gxfcache.h $(gsuid_h) $(gsxfont_h) $(gxbcache_h) $(gxftype_h)
gxfont_h=gxfont.h $(gsfont_h) $(gsuid_h) $(gsstruct_h) $(gxftype_h)
gscie_h=gscie.h $(gsrefct_h) $(gxctable_h)
gscsepr_h=gscsepr.h
gscspace_h=gscspace.h
gxdcconv_h=gxdcconv.h $(gxfrac_h)
gxfmap_h=gxfmap.h $(gsrefct_h) $(gxfrac_h) $(gxtmap_h)
gxistate_h=gxistate.h $(gscsel_h) $(gsropt_h) $(gxcvalue_h) $(gxfixed_h) $(gxline_h) $(gxmatrix_h) $(gxtmap_h)
gxband_h=gxband.h $(gxclio_h)
gxclist_h=gxclist.h $(gscspace_h) $(gxbcache_h) $(gxclio_h) $(gxistate_h) $(gxband_h)
gxcmap_h=gxcmap.h $(gscsel_h) $(gxcvalue_h) $(gxfmap_h)
gxcspace_h=gxcspace.h $(gscspace_h) $(gsccolor_h) $(gscsel_h) $(gsstruct_h) $(gxfrac_h)
gxht_h=gxht.h $(gsht1_h) $(gsrefct_h) $(gxhttype_h) $(gxtmap_h)
gscolor_h=gscolor.h $(gxtmap_h)
gsstate_h=gsstate.h $(gscolor_h) $(gscsel_h) $(gsdevice_h) $(gsht_h) $(gsline_h)

gzacpath_h=gzacpath.h
gzcpath_h=gzcpath.h $(gxcpath_h)
gzht_h=gzht.h $(gscsel_h) $(gxdht_h) $(gxfmap_h) $(gxht_h) $(gxhttile_h)
gzline_h=gzline.h $(gxline_h)
gzpath_h=gzpath.h $(gsstruct_h) $(gxpath_h)
gzstate_h=gzstate.h $(gscpm_h) $(gsrefct_h) $(gsstate_h)\
 $(gxdcolor_h) $(gxistate_h) $(gxstate_h)

gdevprn_h=gdevprn.h $(memory__h) $(string__h) $(gx_h) \
  $(gserrors_h) $(gsmatrix_h) $(gsparam_h) $(gsutil_h) \
  $(gxdevice_h) $(gxdevmem_h) $(gxclist_h)

sa85x_h=sa85x.h
sbtx_h=sbtx.h
scanchar_h=scanchar.h
scommon_h=scommon.h $(gsmemory_h) $(gstypes_h) $(gsstruct_h)
sdct_h=sdct.h
shc_h=shc.h $(gsbittab_h)
siscale_h=siscale.h $(gconfigv_h)
sjpeg_h=sjpeg.h
slzwx_h=slzwx.h
spcxx_h=spcxx.h
spdiffx_h=spdiffx.h
spngpx_h=spngpx.h
srlx_h=srlx.h
sstring_h=sstring.h
strimpl_h=strimpl.h $(scommon_h) $(gstypes_h) $(gsstruct_h)
szlibx_h=szlibx.h
# Out of order
scf_h=scf.h $(shc_h)
scfx_h=scfx.h $(shc_h)
gximage_h=gximage.h $(gsiparam_h) $(gxcspace_h) $(gxdda_h) $(gxsample_h)\
 $(siscale_h) $(strimpl_h)

### Executable code

# gconfig and gscdefs are handled specially.  Currently they go in psbase
# rather than in libcore, which is clearly wrong.
gconfig=gconfig$(CONFIG)
$(gconfig).$(OBJ): gconf.c $(GX) \
  $(gscdefs_h) $(gconfig_h) $(gxdevice_h) $(gxiodev_h) $(MAKEFILE)
	$(RM_) gconfig.h
	$(RM_) $(gconfig).c
	$(CP_) $(gconfig_h) gconfig.h
	$(CP_) gconf.c $(gconfig).c
	$(CCC) $(gconfig).c
	$(RM_) gconfig.h
	$(RM_) $(gconfig).c

gscdefs=gscdefs$(CONFIG)
$(gscdefs).$(OBJ): gscdef.c $(stdpre_h) $(gscdefs_h) $(gconfig_h) $(MAKEFILE)
	$(RM_) gconfig.h
	$(RM_) $(gscdefs).c
	$(CP_) $(gconfig_h) gconfig.h
	$(CP_) gscdef.c $(gscdefs).c
	$(CCC) $(gscdefs).c
	$(RM_) gconfig.h
	$(RM_) $(gscdefs).c

gxacpath.$(OBJ): gxacpath.c $(GXERR) \
  $(gsdcolor_h) $(gsrop_h) $(gsstruct_h) $(gsutil_h) \
  $(gxdevice_h) $(gxfixed_h) $(gxpaint_h) \
  $(gzacpath_h) $(gzcpath_h) $(gzpath_h)

gxbcache.$(OBJ): gxbcache.c $(GX) $(memory__h) \
  $(gsmdebug_h) $(gxbcache_h)

gxccache.$(OBJ): gxccache.c $(GXERR) $(gpcheck_h) \
  $(gscspace_h) $(gsimage_h) $(gsstruct_h) \
  $(gxchar_h) $(gxdevice_h) $(gxdevmem_h) $(gxfcache_h) \
  $(gxfixed_h) $(gxfont_h) $(gxhttile_h) $(gxmatrix_h) $(gxxfont_h) \
  $(gzstate_h) $(gzpath_h) $(gzcpath_h) 

gxccman.$(OBJ): gxccman.c $(GXERR) $(memory__h) $(gpcheck_h)\
 $(gsbitops_h) $(gsstruct_h) $(gsutil_h) $(gxfixed_h) $(gxmatrix_h)\
 $(gxdevice_h) $(gxdevmem_h) $(gxfont_h) $(gxfcache_h) $(gxchar_h)\
 $(gxxfont_h) $(gzstate_h) $(gzpath_h)

gxcht.$(OBJ): gxcht.c $(GXERR) $(memory__h)\
 $(gsutil_h)\
 $(gxcmap_h) $(gxdcolor_h) $(gxdevice_h) $(gxfixed_h) $(gxistate_h)\
 $(gxmatrix_h) $(gzht_h)

gxcmap.$(OBJ): gxcmap.c $(GXERR) \
  $(gsccolor_h) \
  $(gxcmap_h) $(gxcspace_h) $(gxdcconv_h) $(gxdevice_h) $(gxdither_h) \
  $(gxfarith_h) $(gxfrac_h) $(gxlum_h) $(gzstate_h)

gxcpath.$(OBJ): gxcpath.c $(GXERR)\
 $(gscoord_h) $(gsstruct_h) $(gsutil_h)\
 $(gxdevice_h) $(gxfixed_h) $(gzpath_h) $(gzcpath_h)

gxdcconv.$(OBJ): gxdcconv.c $(GX) \
  $(gsdcolor_h) $(gxcmap_h) $(gxdcconv_h) $(gxdevice_h) \
  $(gxfarith_h) $(gxistate_h) $(gxlum_h)

gxdcolor.$(OBJ): gxdcolor.c $(GX) \
  $(gsbittab_h) $(gxdcolor_h) $(gxdevice_h)

gxdither.$(OBJ): gxdither.c $(GX) \
  $(gsstruct_h) $(gsdcolor_h) \
  $(gxcmap_h) $(gxdevice_h) $(gxdither_h) $(gxlum_h) $(gzht_h)

gxfill.$(OBJ): gxfill.c $(GXERR) $(math__h) \
  $(gsstruct_h) \
  $(gxdcolor_h) $(gxdevice_h) $(gxfixed_h) $(gxhttile_h) \
  $(gxistate_h) $(gxpaint_h) \
  $(gzcpath_h) $(gzpath_h)

gxht.$(OBJ): gxht.c $(GXERR) $(memory__h)\
 $(gsbitops_h) $(gsstruct_h) $(gsutil_h)\
 $(gxdcolor_h) $(gxdevice_h) $(gxfixed_h) $(gxistate_h) $(gzht_h)

gximage.$(OBJ): gximage.c $(GXERR) $(math__h) $(memory__h) $(gpcheck_h)\
 $(gsccolor_h) $(gspaint_h) $(gsstruct_h)\
 $(gxfixed_h) $(gxfrac_h) $(gxarith_h) $(gxmatrix_h)\
 $(gxdevice_h) $(gzpath_h) $(gzstate_h)\
 $(gzcpath_h) $(gxdevmem_h) $(gximage_h) $(gdevmrop_h)

gximage0.$(OBJ): gximage0.c $(GXERR) $(memory__h)\
 $(gxcpath_h) $(gxdevice_h) $(gximage_h)

gximage1.$(OBJ): gximage1.c $(GXERR) $(memory__h) $(gpcheck_h)\
 $(gdevmem_h) $(gsbittab_h) $(gsccolor_h) $(gspaint_h) $(gsutil_h)\
 $(gxarith_h) $(gxcmap_h) $(gxcpath_h) $(gxdcolor_h) $(gxdevice_h)\
 $(gxdevmem_h) $(gxfixed_h) $(gximage_h) $(gxistate_h) $(gxmatrix_h)\
 $(gzht_h) $(gzpath_h)

gximage2.$(OBJ): gximage2.c $(GXERR) $(memory__h) $(gpcheck_h)\
 $(gdevmem_h) $(gsccolor_h) $(gspaint_h) $(gsutil_h)\
 $(gxarith_h) $(gxcmap_h) $(gxcpath_h) $(gxdcolor_h) $(gxdevice_h)\
 $(gxdevmem_h) $(gxfixed_h) $(gximage_h) $(gxistate_h) $(gxmatrix_h)\
 $(gzht_h) $(gzpath_h)

gxpaint.$(OBJ): gxpaint.c $(GX) \
  $(gxdevice_h) $(gxhttile_h) $(gxpaint_h) $(gxpath_h) $(gzstate_h)

gxpath.$(OBJ): gxpath.c $(GXERR) \
  $(gsstruct_h) $(gxfixed_h) $(gzpath_h)

gxpath2.$(OBJ): gxpath2.c $(GXERR) $(math__h) \
  $(gxfixed_h) $(gxarith_h) $(gzpath_h)

gxpcopy.$(OBJ): gxpcopy.c $(GXERR) $(math__h) $(gconfigv_h) \
  $(gxfarith_h) $(gxfixed_h) $(gzpath_h)

gxpdash.$(OBJ): gxpdash.c $(GX) $(math__h) \
  $(gscoord_h) $(gsline_h) $(gsmatrix_h) \
  $(gxfixed_h) $(gzline_h) $(gzpath_h)

gxpflat.$(OBJ): gxpflat.c $(GX)\
 $(gxarith_h) $(gxfixed_h) $(gzpath_h)

gxsample.$(OBJ): gxsample.c $(GX)\
 $(gxsample_h)

gxstroke.$(OBJ): gxstroke.c $(GXERR) $(math__h) $(gpcheck_h) \
  $(gscoord_h) $(gsdcolor_h) $(gsdevice_h) \
  $(gxdevice_h) $(gxfarith_h) $(gxfixed_h) \
  $(gxhttile_h) $(gxistate_h) $(gxmatrix_h) $(gxpaint_h) \
  $(gzcpath_h) $(gzline_h) $(gzpath_h)

###### Higher-level facilities

gschar.$(OBJ): gschar.c $(GXERR) $(memory__h) $(string__h)\
 $(gspath_h) $(gsstruct_h) \
 $(gxfixed_h) $(gxarith_h) $(gxmatrix_h) $(gxcoord_h) $(gxdevice_h) $(gxdevmem_h) \
 $(gxfont_h) $(gxfont0_h) $(gxchar_h) $(gxfcache_h) $(gzpath_h) $(gzstate_h)

gscolor.$(OBJ): gscolor.c $(GXERR) \
  $(gsccolor_h) $(gsstruct_h) $(gsutil_h) \
  $(gxcmap_h) $(gxcspace_h) $(gxdcconv_h) $(gxdevice_h) $(gzstate_h)

gscoord.$(OBJ): gscoord.c $(GXERR) $(math__h) \
  $(gsccode_h) $(gxcoord_h) $(gxdevice_h) $(gxfarith_h) $(gxfixed_h) $(gxfont_h) \
  $(gxmatrix_h) $(gxpath_h) $(gzstate_h)

gsdevice.$(OBJ): gsdevice.c $(GXERR) $(ctype__h) $(memory__h) $(string__h) $(gp_h)\
 $(gscdefs_h) $(gscoord_h) $(gsmatrix_h) $(gspaint_h) $(gspath_h) $(gsstruct_h)\
 $(gxcmap_h) $(gxdevice_h) $(gxdevmem_h) $(gzstate_h)

gsdevmem.$(OBJ): gsdevmem.c $(GXERR) $(math__h) $(memory__h) \
  $(gxarith_h) $(gxdevice_h) $(gxdevmem_h)

gsdparam.$(OBJ): gsdparam.c $(GXERR) $(memory__h) $(string__h) \
  $(gsparam_h) $(gxdevice_h) $(gxfixed_h)

gsfont.$(OBJ): gsfont.c $(GXERR) $(memory__h)\
 $(gschar_h) $(gsstruct_h) \
 $(gxdevice_h) $(gxfixed_h) $(gxmatrix_h) $(gxfont_h) $(gxfcache_h)\
 $(gzstate_h)

gsht.$(OBJ): gsht.c $(GXERR) $(memory__h)\
 $(gsstruct_h) $(gsutil_h) $(gxarith_h) $(gxdevice_h) $(gzht_h) $(gzstate_h)

gshtscr.$(OBJ): gshtscr.c $(GXERR) $(math__h) \
  $(gsstruct_h) $(gxarith_h) $(gxdevice_h) $(gzht_h) $(gzstate_h)

gsimage.$(OBJ): gsimage.c $(GXERR) $(memory__h)\
  $(gscspace_h) $(gsimage_h) $(gsmatrix_h) $(gsstruct_h) \
  $(gxarith_h) $(gxdevice_h) $(gzstate_h)

gsimpath.$(OBJ): gsimpath.c $(GXERR) \
  $(gsmatrix_h) $(gsstate_h) $(gspath_h)

gsinit.$(OBJ): gsinit.c $(memory__h) $(stdio__h) \
  $(gdebug_h) $(gp_h) $(gscdefs_h) $(gslib_h) $(gsmemory_h)

gsiodev.$(OBJ): gsiodev.c $(GXERR) $(errno__h) $(string__h) \
  $(gp_h) $(gsparam_h) $(gxiodev_h)

gsline.$(OBJ): gsline.c $(GXERR) $(math__h) $(memory__h)\
 $(gsline_h) $(gxfixed_h) $(gxmatrix_h) $(gzstate_h) $(gzline_h)

gsmatrix.$(OBJ): gsmatrix.c $(GXERR) $(math__h) \
  $(gxfarith_h) $(gxfixed_h) $(gxmatrix_h)

gspaint.$(OBJ): gspaint.c $(GXERR) $(math__h) $(gpcheck_h)\
 $(gspaint_h) $(gspath_h) $(gsropt_h)\
 $(gxcpath_h) $(gxdevmem_h) $(gxdevice_h) $(gxfixed_h) $(gxmatrix_h) $(gxpaint_h)\
 $(gzpath_h) $(gzstate_h)

gsparam.$(OBJ): gsparam.c $(GXERR) $(memory__h) $(string__h)\
 $(gsparam_h) $(gsstruct_h)

gsparams.$(OBJ): gsparams.c $(gx_h) $(memory__h) $(gserrors_h) $(gsparam_h)

gspath.$(OBJ): gspath.c $(GXERR) \
  $(gscoord_h) $(gxdevice_h) $(gxfixed_h) $(gxmatrix_h) \
  $(gzcpath_h) $(gzpath_h) $(gzstate_h)

gsstate.$(OBJ): gsstate.c $(GXERR) $(memory__h)\
 $(gscie_h) $(gscolor2_h) $(gscoord_h) $(gspath_h) $(gsstruct_h) $(gsutil_h) \
 $(gxcmap_h) $(gxcspace_h) $(gxdevice_h) $(gxpcache_h) \
 $(gzstate_h) $(gzht_h) $(gzline_h) $(gzpath_h) $(gzcpath_h)

###### The internal devices

### The built-in device implementations:

# The bounding box device is not normally a free-standing device.
# To configure it as one for testing, change SETMOD to SETDEV, and also
# define TEST in gdevbbox.c.
bbox.dev: $(LIB_MAK) $(ECHOGS_XE) gdevbbox.$(OBJ)
	$(SETMOD) bbox gdevbbox.$(OBJ)

gdevbbox.$(OBJ): gdevbbox.c $(GXERR) $(math__h) $(memory__h) \
  $(gdevbbox_h) $(gsdevice_h) $(gsparam_h) \
  $(gxcpath_h) $(gxdevice_h) $(gxistate_h) $(gxpaint_h) $(gxpath_h)

gdevddrw.$(OBJ): gdevddrw.c $(GXERR) $(math__h) $(gpcheck_h) \
  $(gxdcolor_h) $(gxdevice_h) $(gxfixed_h) $(gxmatrix_h)

gdevdflt.$(OBJ): gdevdflt.c $(GXERR) $(gpcheck_h)\
 $(gsbittab_h) $(gsropt_h)\
 $(gxcpath_h) $(gxdcolor_h) $(gxdevice_h) $(gxdevmem_h)

gdevnfwd.$(OBJ): gdevnfwd.c $(GX) \
  $(gxdevice_h)

# The render/RGB device is only here as an example, but we can configure
# it as a real device for testing.
rrgb.dev: $(LIB_MAK) $(ECHOGS_XE) gdevrrgb.$(OBJ) page.dev
	$(SETPDEV) rrgb gdevrrgb.$(OBJ)

gdevrrgb.$(OBJ): gdevrrgb.c $(AK)\
 $(gdevprn_h)

### The memory devices:

gdevabuf.$(OBJ): gdevabuf.c $(GXERR) $(memory__h)\
 $(gxdevice_h) $(gxdevmem_h) $(gdevmem_h)

gdevmem.$(OBJ): gdevmem.c $(GXERR) $(memory__h)\
 $(gsstruct_h) $(gxdevice_h) $(gxdevmem_h) $(gdevmem_h)

gdevm1.$(OBJ): gdevm1.c $(GX) $(memory__h) $(gsrop_h)\
 $(gxdevice_h) $(gxdevmem_h) $(gdevmem_h)

gdevm2.$(OBJ): gdevm2.c $(GX) $(memory__h)\
 $(gxdevice_h) $(gxdevmem_h) $(gdevmem_h)

gdevm4.$(OBJ): gdevm4.c $(GX) $(memory__h)\
 $(gxdevice_h) $(gxdevmem_h) $(gdevmem_h)

gdevm8.$(OBJ): gdevm8.c $(GX) $(memory__h)\
 $(gxdevice_h) $(gxdevmem_h) $(gdevmem_h)

gdevm16.$(OBJ): gdevm16.c $(GX) $(memory__h)\
 $(gxdevice_h) $(gxdevmem_h) $(gdevmem_h)

gdevm24.$(OBJ): gdevm24.c $(GX) $(memory__h)\
 $(gxdevice_h) $(gxdevmem_h) $(gdevmem_h)

gdevm32.$(OBJ): gdevm32.c $(GX) $(memory__h)\
 $(gxdevice_h) $(gxdevmem_h) $(gdevmem_h)

gdevmpla.$(OBJ): gdevmpla.c $(GX) $(memory__h)\
 $(gxdevice_h) $(gxdevmem_h) $(gdevmem_h)

# Create a pseudo-"feature" for the entire graphics library.

LIB1s=gsalloc.$(OBJ) gsbitops.$(OBJ) gsbittab.$(OBJ)
LIB2s=gschar.$(OBJ) gscolor.$(OBJ) gscoord.$(OBJ) gsdevice.$(OBJ) gsdevmem.$(OBJ)
LIB3s=gsdparam.$(OBJ) gsfont.$(OBJ) gsht.$(OBJ) gshtscr.$(OBJ)
LIB4s=gsimage.$(OBJ) gsimpath.$(OBJ) gsinit.$(OBJ) gsiodev.$(OBJ)
LIB5s=gsline.$(OBJ) gsmatrix.$(OBJ) gsmemory.$(OBJ) gsmisc.$(OBJ)
LIB6s=gspaint.$(OBJ) gsparam.$(OBJ) gsparams.$(OBJ) gspath.$(OBJ) gsstate.$(OBJ) gsutil.$(OBJ)
LIB1x=gxacpath.$(OBJ) gxbcache.$(OBJ)
LIB2x=gxccache.$(OBJ) gxccman.$(OBJ) gxcht.$(OBJ) gxcmap.$(OBJ) gxcpath.$(OBJ)
LIB3x=gxdcconv.$(OBJ) gxdcolor.$(OBJ) gxdither.$(OBJ) gxfill.$(OBJ) gxht.$(OBJ)
LIB4x=gximage.$(OBJ) gximage0.$(OBJ) gximage1.$(OBJ) gximage2.$(OBJ)
LIB5x=gxpaint.$(OBJ) gxpath.$(OBJ) gxpath2.$(OBJ) gxpcopy.$(OBJ)
LIB6x=gxpdash.$(OBJ) gxpflat.$(OBJ) gxsample.$(OBJ) gxstroke.$(OBJ)
LIB1d=gdevabuf.$(OBJ) gdevddrw.$(OBJ) gdevdflt.$(OBJ) gdevnfwd.$(OBJ)
LIB2d=gdevmem.$(OBJ) gdevm1.$(OBJ) gdevm2.$(OBJ) gdevm4.$(OBJ) gdevm8.$(OBJ)
LIB3d=gdevm16.$(OBJ) gdevm24.$(OBJ) gdevm32.$(OBJ) gdevmpla.$(OBJ)
LIBs=$(LIB1s) $(LIB2s) $(LIB3s) $(LIB4s) $(LIB5s) $(LIB6s)
LIBx=$(LIB1x) $(LIB2x) $(LIB3x) $(LIB4x) $(LIB5x) $(LIB6x)
LIBd=$(LIB1d) $(LIB2d) $(LIB3d)
LIB_ALL=$(LIBs) $(LIBx) $(LIBd)
libs.dev: $(LIB_MAK) $(ECHOGS_XE) $(LIBs)
	$(EXP)echogs -w libs.dev $(LIB1s)
	$(EXP)echogs -a libs.dev $(LIB2s)
	$(EXP)echogs -a libs.dev $(LIB3s)
	$(EXP)echogs -a libs.dev $(LIB4s)
	$(EXP)echogs -a libs.dev $(LIB5s)
	$(EXP)echogs -a libs.dev $(LIB6s)
	$(ADDMOD) libs -init gscolor

libx.dev: $(LIB_MAK) $(ECHOGS_XE) $(LIBx)
	$(EXP)echogs -w libx.dev $(LIB1x)
	$(EXP)echogs -a libx.dev $(LIB2x)
	$(EXP)echogs -a libx.dev $(LIB3x)
	$(EXP)echogs -a libx.dev $(LIB4x)
	$(EXP)echogs -a libx.dev $(LIB5x)
	$(EXP)echogs -a libx.dev $(LIB6x)
	$(ADDMOD) libx -init gximage1 gximage2

libd.dev: $(LIB_MAK) $(ECHOGS_XE) $(LIBd)
	$(EXP)echogs -w libd.dev $(LIB1d)
	$(EXP)echogs -a libd.dev $(LIB2d)
	$(EXP)echogs -a libd.dev $(LIB3d)

# roplib shouldn't be required....
libcore.dev: $(LIB_MAK) $(ECHOGS_XE)\
  libs.dev libx.dev libd.dev iscale.dev roplib.dev
	$(SETMOD) libcore
	$(ADDMOD) libcore -dev nullpage
	$(ADDMOD) libcore -include libs libx libd iscale roplib

# ---------------- Stream support ---------------- #
# Currently the only things in the library that use this are clists
# and file streams.

stream_h=stream.h $(scommon_h)

stream.$(OBJ): stream.c $(AK) $(stdio__h) $(memory__h) \
  $(gdebug_h) $(gpcheck_h) $(stream_h) $(strimpl_h)

# ---------------- File streams ---------------- #
# Currently only the high-level drivers use these, but more drivers will
# probably use them eventually.

sfile_=sfx$(FILE_IMPLEMENTATION).$(OBJ) stream.$(OBJ)
sfile.dev: $(LIB_MAK) $(ECHOGS_XE) $(sfile_)
	$(SETMOD) sfile $(sfile_)

sfxstdio.$(OBJ): sfxstdio.c $(AK) $(stdio__h) $(memory__h) \
  $(gdebug_h) $(gpcheck_h) $(stream_h) $(strimpl_h)

sfxfd.$(OBJ): sfxfd.c $(AK) $(stdio__h) $(errno__h) $(memory__h) \
  $(gdebug_h) $(gpcheck_h) $(stream_h) $(strimpl_h)

sfxboth.$(OBJ): sfxboth.c sfxstdio.c sfxfd.c

# ---------------- CCITTFax filters ---------------- #
# These are used by clists, some drivers, and Level 2 in general.

cfe_=scfe.$(OBJ) scfetab.$(OBJ) shc.$(OBJ)
cfe.dev: $(LIB_MAK) $(ECHOGS_XE) $(cfe_)
	$(SETMOD) cfe $(cfe_)

scfe.$(OBJ): scfe.c $(AK) $(memory__h) $(stdio__h) $(gdebug_h)\
  $(scf_h) $(strimpl_h) $(scfx_h)

scfetab.$(OBJ): scfetab.c $(AK) $(std_h) $(scommon_h) $(scf_h)

shc.$(OBJ): shc.c $(AK) $(std_h) $(scommon_h) $(shc_h)

cfd_=scfd.$(OBJ) scfdtab.$(OBJ)
cfd.dev: $(LIB_MAK) $(ECHOGS_XE) $(cfd_)
	$(SETMOD) cfd $(cfd_)

scfd.$(OBJ): scfd.c $(AK) $(memory__h) $(stdio__h) $(gdebug_h)\
  $(scf_h) $(strimpl_h) $(scfx_h)

scfdtab.$(OBJ): scfdtab.c $(AK) $(std_h) $(scommon_h) $(scf_h)

# ---------------- DCT (JPEG) filters ---------------- #
# These are used by Level 2, and by the JPEG-writing driver.

# Common code

sdctc_=sdctc.$(OBJ) sjpegc.$(OBJ)

sdctc.$(OBJ): sdctc.c $(AK) $(stdio__h)\
 $(sdct_h) $(strimpl_h)\
 jpeglib.h

sjpegc.$(OBJ): sjpegc.c $(AK) $(stdio__h) $(string__h) $(gx_h)\
 $(gserrors_h) $(sjpeg_h) $(sdct_h) $(strimpl_h) \
 jerror.h jpeglib.h

# Encoding (compression)

sdcte_=$(sdctc_) sdcte.$(OBJ) sjpege.$(OBJ)
sdcte.dev: $(LIB_MAK) $(ECHOGS_XE) $(sdcte_) jpege.dev
	$(SETMOD) sdcte $(sdcte_)
	$(ADDMOD) sdcte -include jpege

sdcte.$(OBJ): sdcte.c $(AK) $(memory__h) $(stdio__h) $(gdebug_h)\
  $(sdct_h) $(sjpeg_h) $(strimpl_h) \
  jerror.h jpeglib.h

sjpege.$(OBJ): sjpege.c $(AK) $(stdio__h) $(string__h) $(gx_h)\
 $(gserrors_h) $(sjpeg_h) $(sdct_h) $(strimpl_h) \
 jerror.h jpeglib.h

# Decoding (decompression)

sdctd_=$(sdctc_) sdctd.$(OBJ) sjpegd.$(OBJ)
sdctd.dev: $(LIB_MAK) $(ECHOGS_XE) $(sdctd_) jpegd.dev
	$(SETMOD) sdctd $(sdctd_)
	$(ADDMOD) sdctd -include jpegd

sdctd.$(OBJ): sdctd.c $(AK) $(memory__h) $(stdio__h) $(gdebug_h)\
  $(sdct_h) $(sjpeg_h) $(strimpl_h) \
  jerror.h jpeglib.h

sjpegd.$(OBJ): sjpegd.c $(AK) $(stdio__h) $(string__h) $(gx_h)\
 $(gserrors_h) $(sjpeg_h) $(sdct_h) $(strimpl_h)\
 jerror.h jpeglib.h

# ---------------- LZW filters ---------------- #
# These are used by Level 2 in general.

slzwe_=slzwce
#slzwe_=slzwe
lzwe_=$(slzwe_).$(OBJ) slzwc.$(OBJ)
lzwe.dev: $(LIB_MAK) $(ECHOGS_XE) $(lzwe_)
	$(SETMOD) lzwe $(lzwe_)

# We need slzwe.dev as a synonym for lzwe.dev for BAND_LIST_STORAGE = memory.
slzwe.dev: lzwe.dev
	$(CP_) lzwe.dev slzwe.dev

slzwce.$(OBJ): slzwce.c $(AK) $(stdio__h) $(gdebug_h)\
  $(slzwx_h) $(strimpl_h)

slzwe.$(OBJ): slzwe.c $(AK) $(stdio__h) $(gdebug_h)\
  $(slzwx_h) $(strimpl_h)

slzwc.$(OBJ): slzwc.c $(AK) $(std_h)\
  $(slzwx_h) $(strimpl_h)

lzwd_=slzwd.$(OBJ) slzwc.$(OBJ)
lzwd.dev: $(LIB_MAK) $(ECHOGS_XE) $(lzwd_)
	$(SETMOD) lzwd $(lzwd_)

# We need slzwd.dev as a synonym for lzwd.dev for BAND_LIST_STORAGE = memory.
slzwd.dev: lzwd.dev
	$(CP_) lzwd.dev slzwd.dev

slzwd.$(OBJ): slzwd.c $(AK) $(stdio__h) $(gdebug_h)\
  $(slzwx_h) $(strimpl_h)

# ---------------- PCX decoding filter ---------------- #
# This is an adhoc filter not used by anything in the standard configuration.

pcxd_=spcxd.$(OBJ)
pcxd.dev: $(LIB_MAK) $(ECHOGS_XE) $(pcxd_)
	$(SETMOD) pcxd $(pcxd_)

spcxd.$(OBJ): spcxd.c $(AK) $(stdio__h) $(memory__h) \
  $(spcxx_h) $(strimpl_h)

# ---------------- Pixel-difference filters ---------------- #
# The Predictor facility of the LZW and Flate filters uses these.

pdiff_=spdiff.$(OBJ)
pdiff.dev: $(LIB_MAK) $(ECHOGS_XE) $(pdiff_)
	$(SETMOD) pdiff $(pdiff_)

spdiff.$(OBJ): spdiff.c $(AK) $(stdio__h)\
 $(spdiffx_h) $(strimpl_h)

# ---------------- PNG pixel prediction filters ---------------- #
# The Predictor facility of the LZW and Flate filters uses these.

pngp_=spngp.$(OBJ)
pngp.dev: $(LIB_MAK) $(ECHOGS_XE) $(pngp_)
	$(SETMOD) pngp $(pngp_)

spngp.$(OBJ): spngp.c $(AK) $(memory__h)\
  $(spngpx_h) $(strimpl_h)

# ---------------- RunLength filters ---------------- #
# These are used by clists and also by Level 2 in general.

rle_=srle.$(OBJ)
rle.dev: $(LIB_MAK) $(ECHOGS_XE) $(rle_)
	$(SETMOD) rle $(rle_)

srle.$(OBJ): srle.c $(AK) $(stdio__h) $(memory__h) \
  $(srlx_h) $(strimpl_h)

rld_=srld.$(OBJ)
rld.dev: $(LIB_MAK) $(ECHOGS_XE) $(rld_)
	$(SETMOD) rld $(rld_)

srld.$(OBJ): srld.c $(AK) $(stdio__h) $(memory__h) \
  $(srlx_h) $(strimpl_h)

# ---------------- String encoding/decoding filters ---------------- #
# These are used by the PostScript and PDF writers, and also by the
# PostScript interpreter.

scantab.$(OBJ): scantab.c $(AK) $(stdpre_h)\
 $(scanchar_h) $(scommon_h)

sfilter2.$(OBJ): sfilter2.c $(AK) $(memory__h) $(stdio__h)\
 $(sa85x_h) $(scanchar_h) $(sbtx_h) $(strimpl_h)

sstring.$(OBJ): sstring.c $(AK) $(stdio__h) $(memory__h) $(string__h)\
 $(scanchar_h) $(sstring_h) $(strimpl_h)

# ---------------- zlib filters ---------------- #
# These are used by clists and are also available as filters.

szlibc_=szlibc.$(OBJ)

szlibc.$(OBJ): szlibc.c $(AK) $(std_h) \
  $(gsmemory_h) $(gsstruct_h) $(gstypes_h) $(strimpl_h) $(szlibx_h)
	$(CCCZ) szlibc.c

szlibe_=$(szlibc_) szlibe.$(OBJ)
szlibe.dev: $(LIB_MAK) $(ECHOGS_XE) zlibe.dev $(szlibe_)
	$(SETMOD) szlibe $(szlibe_)
	$(ADDMOD) szlibe -include zlibe

szlibe.$(OBJ): szlibe.c $(AK) $(std_h) \
  $(gsmemory_h) $(strimpl_h) $(szlibx_h)
	$(CCCZ) szlibe.c

szlibd_=$(szlibc_) szlibd.$(OBJ)
szlibd.dev: $(LIB_MAK) $(ECHOGS_XE) zlibd.dev $(szlibd_)
	$(SETMOD) szlibd $(szlibd_)
	$(ADDMOD) szlibd -include zlibd

szlibd.$(OBJ): szlibd.c $(AK) $(std_h) \
  $(gsmemory_h) $(strimpl_h) $(szlibx_h)
	$(CCCZ) szlibd.c

# ---------------- Command lists ---------------- #

gxcldev_h=gxcldev.h $(gxclist_h) $(gsropt_h) $(gxht_h) $(gxtmap_h) $(gxdht_h)\
  $(strimpl_h) $(scfx_h) $(srlx_h)
gxclpage_h=gxclpage.h $(gxclio_h)
gxclpath_h=gxclpath.h $(gxfixed_h)

# Command list package.  Currently the higher-level facilities are required,
# but eventually they will be optional.
clist.dev: $(LIB_MAK) $(ECHOGS_XE) clbase.dev clpath.dev
	$(SETMOD) clist -include clbase clpath

# Base command list facility.
clbase1_=gxclist.$(OBJ) gxclbits.$(OBJ) gxclpage.$(OBJ)
clbase2_=gxclread.$(OBJ) gxclrect.$(OBJ) stream.$(OBJ)
clbase_=$(clbase1_) $(clbase2_)
clbase.dev: $(LIB_MAK) $(ECHOGS_XE) $(clbase_) cl$(BAND_LIST_STORAGE).dev \
  cfe.dev cfd.dev rle.dev rld.dev
	$(SETMOD) clbase $(clbase1_)
	$(ADDMOD) clbase -obj $(clbase2_)
	$(ADDMOD) clbase -include cl$(BAND_LIST_STORAGE) cfe cfd rle rld

gdevht_h=gdevht.h $(gzht_h)

gdevht.$(OBJ): gdevht.c $(GXERR) \
  $(gdevht_h) $(gxdcconv_h) $(gxdcolor_h) $(gxdevice_h) $(gxdither_h)

gxclist.$(OBJ): gxclist.c $(GXERR) $(memory__h) $(string__h)\
 $(gp_h) $(gpcheck_h)\
 $(gxcldev_h) $(gxclpath_h) $(gxdevice_h) $(gxdevmem_h) $(gsparams_h)

gxclbits.$(OBJ): gxclbits.c $(GXERR) $(memory__h) $(gpcheck_h)\
 $(gsbitops_h) $(gxcldev_h) $(gxdevice_h) $(gxdevmem_h) $(gxfmap_h)

gxclpage.$(OBJ): gxclpage.c $(AK)\
 $(gdevprn_h) $(gxcldev_h) $(gxclpage_h)

# (gxclread shouldn't need gxclpath.h)
gxclread.$(OBJ): gxclread.c $(GXERR) $(memory__h) $(gp_h) $(gpcheck_h)\
 $(gdevht_h)\
 $(gsbitops_h) $(gscoord_h) $(gsdevice_h) $(gsstate_h)\
 $(gxcldev_h) $(gxclpath_h) $(gxcmap_h) $(gxcspace_h) $(gxdcolor_h)\
 $(gxdevice_h) $(gxdevmem_h) $(gsparams_h)\
 $(gxhttile_h) $(gxpaint_h) $(gzacpath_h) $(gzcpath_h) $(gzpath_h)\
 $(stream_h) $(strimpl_h)

gxclrect.$(OBJ): gxclrect.c $(GXERR)\
 $(gsutil_h) $(gxcldev_h) $(gxdevice_h) $(gxdevmem_h)

# Higher-level command list facilities.
clpath_=gxclimag.$(OBJ) gxclpath.$(OBJ)
clpath.dev: $(LIB_MAK) $(ECHOGS_XE) $(clpath_) psl2cs.dev
	$(SETMOD) clpath $(clpath_)
	$(ADDMOD) clpath -include psl2cs
	$(ADDMOD) clpath -init climag clpath

gxclimag.$(OBJ): gxclimag.c $(GXERR) $(math__h) $(memory__h)\
 $(gscspace_h)\
 $(gxarith_h) $(gxcldev_h) $(gxclpath_h) $(gxdevice_h) $(gxdevmem_h)\
 $(gxpath_h) $(gxfmap_h)\
 $(siscale_h) $(strimpl_h)

gxclpath.$(OBJ): gxclpath.c $(GXERR) $(math__h) $(memory__h) $(gpcheck_h)\
 $(gxcldev_h) $(gxclpath_h) $(gxcolor2_h) $(gxdevice_h) $(gxdevmem_h)\
 $(gxpaint_h) \
 $(gzcpath_h) $(gzpath_h)

# Implement band lists on files.

clfile_=gxclfile.$(OBJ)
clfile.dev: $(LIB_MAK) $(ECHOGS_XE) $(clfile_)
	$(SETMOD) clfile $(clfile_)

gxclfile.$(OBJ): gxclfile.c $(stdio__h) $(string__h) \
  $(gp_h) $(gsmemory_h) $(gserror_h) $(gserrors_h) $(gxclio_h)

# Implement band lists in memory (RAM).

clmemory_=gxclmem.$(OBJ) gxcl$(BAND_LIST_COMPRESSOR).$(OBJ)
clmemory.dev: $(LIB_MAK) $(ECHOGS_XE) $(clmemory_) s$(BAND_LIST_COMPRESSOR)e.dev s$(BAND_LIST_COMPRESSOR)d.dev
	$(SETMOD) clmemory $(clmemory_)
	$(ADDMOD) clmemory -include s$(BAND_LIST_COMPRESSOR)e s$(BAND_LIST_COMPRESSOR)d
	$(ADDMOD) clmemory -init cl_$(BAND_LIST_COMPRESSOR)

gxclmem_h=gxclmem.h $(gxclio_h) $(strimpl_h)

gxclmem.$(OBJ): gxclmem.c $(GXERR) $(LIB_MAK) $(memory__h) \
  $(gxclmem_h)

# Implement the compression method for RAM-based band lists.

gxcllzw.$(OBJ): gxcllzw.c $(std_h)\
 $(gsmemory_h) $(gstypes_h) $(gxclmem_h) $(slzwx_h)

gxclzlib.$(OBJ): gxclzlib.c $(std_h)\
 $(gsmemory_h) $(gstypes_h) $(gxclmem_h) $(szlibx_h)
	$(CCCZ) gxclzlib.c

# ---------------- Page devices ---------------- #
# We include this here, rather than in devs.mak, because it is more like
# a feature than a simple device.

page_=gdevprn.$(OBJ)
page.dev: $(LIB_MAK) $(ECHOGS_XE) $(page_) clist.dev
	$(SETMOD) page $(page_)
	$(ADDMOD) page -include clist

gdevprn.$(OBJ): gdevprn.c $(ctype__h) \
  $(gdevprn_h) $(gp_h) $(gsparam_h) $(gxclio_h)

# ---------------- Vector devices ---------------- #
# We include this here for the same reasons as page.dev.

gdevvec_h=gdevvec.h $(gdevbbox_h) $(gsropt_h) $(gxdevice_h) $(gxistate_h) $(stream_h)

vector_=gdevvec.$(OBJ)
vector.dev: $(LIB_MAK) $(ECHOGS_XE) $(vector_) bbox.dev sfile.dev
	$(SETMOD) vector $(vector_)
	$(ADDMOD) vector -include bbox sfile

gdevvec.$(OBJ): gdevvec.c $(GXERR) $(math__h) $(memory__h) $(string__h)\
 $(gdevvec_h) $(gp_h) $(gscspace_h) $(gsparam_h) $(gsutil_h)\
 $(gxdcolor_h) $(gxfixed_h) $(gxpaint_h)\
 $(gzcpath_h) $(gzpath_h)

# ---------------- Image scaling filter ---------------- #

iscale_=siscale.$(OBJ)
iscale.dev: $(LIB_MAK) $(ECHOGS_XE) $(iscale_)
	$(SETMOD) iscale $(iscale_)

siscale.$(OBJ): siscale.c $(AK) $(math__h) $(memory__h) $(stdio__h) \
  $(siscale_h) $(strimpl_h)

# ---------------- RasterOp et al ---------------- #
# Currently this module is required, but it should be optional.

roplib_=gdevmrop.$(OBJ) gsrop.$(OBJ) gsroptab.$(OBJ)
roplib.dev: $(LIB_MAK) $(ECHOGS_XE) $(roplib_)
	$(SETMOD) roplib $(roplib_)
	$(ADDMOD) roplib -init roplib

gdevrun.$(OBJ): gdevrun.c $(GXERR) $(memory__h) \
  $(gxdevice_h) $(gxdevmem_h)

gdevmrop.$(OBJ): gdevmrop.c $(GXERR) $(memory__h) \
  $(gsbittab_h) $(gsropt_h) \
  $(gxdcolor_h) $(gxdevice_h) $(gxdevmem_h) $(gxdevrop_h) \
  $(gdevmrop_h)

gsrop.$(OBJ): gsrop.c $(GXERR) \
  $(gsrop_h) $(gzstate_h)

gsroptab.$(OBJ): gsroptab.c $(stdpre_h) $(gsropt_h)
	$(CCLEAF) gsroptab.c

# ---------------- Async rendering ---------------- #

gsmemfix_h=gsmemfix.h $(gsmemraw_h)
gxsync_h=gxsync.h $(gpsync_h) $(gsmemory_h)
gxpageq_h=gxpageq.h $(gsmemory_h) $(gxband_h) $(gxsync_h)
gsmemlok_h=gsmemlok.h $(gsmemory_h) $(gxsync_h)
gdevprna_h=gdevprna.h $(gdevprn_h) $(gxsync_h)

async_=gdevprna.$(OBJ) gxsync.$(OBJ) gxpageq.$(OBJ) gsmemlok.$(OBJ)\
 gsmemfix.$(OBJ)
async.dev: $(INT_MAK) $(ECHOGS_XE) $(async_) clist.dev
	$(SETMOD) async $(async_)

gdevprna.$(OBJ): gdevprna.c $(AK) $(ctype__h) $(gdevprna_h) $(gsparam_h)\
 $(gsdevice_h) $(gxcldev_h) $(gxclpath_h) $(gxpageq_h) $(gsmemory_h)\
 $(gsmemlok_h) $(gsmemfix_h)

gsmemfix.$(OBJ): gsmemfix.c $(AK) $(memory__h) $(gsmemraw_h) $(gsmemfix_h)

gxsync.$(OBJ): gxsync.c $(AK) $(gxsync_h) $(memory__h) $(gx_h) $(gserrors_h)\
 $(gsmemory_h)

gxpageq.$(OBJ): gxpageq.c $(GXERR) $(gxdevice_h) $(gxclist_h)\
 $(gxpageq_h) $(gserrors_h)

gsmemlok.$(OBJ): gsmemlok.c $(GXERR) $(gsmemlok_h) $(gserrors_h)

# -------- Composite (PostScript Type 0) font support -------- #

cmaplib_=gsfcmap.$(OBJ)
cmaplib.dev: $(LIB_MAK) $(ECHOGS_XE) $(cmaplib_)
	$(SETMOD) cmaplib $(cmaplib_)

gsfcmap.$(OBJ): gsfcmap.c $(GXERR)\
 $(gsstruct_h) $(gxfcmap_h)

psf0lib_=gschar0.$(OBJ) gsfont0.$(OBJ)
psf0lib.dev: $(LIB_MAK) $(ECHOGS_XE) cmaplib.dev $(psf0lib_)
	$(SETMOD) psf0lib $(psf0lib_)
	$(ADDMOD) psf0lib -include cmaplib

gschar0.$(OBJ): gschar0.c $(GXERR) $(memory__h)\
 $(gsstruct_h) $(gxfixed_h) $(gxdevice_h) $(gxdevmem_h)\
 $(gsfcmap_h) $(gxfont_h) $(gxfont0_h) $(gxchar_h)

gsfont0.$(OBJ): gsfont0.c $(GXERR) $(memory__h)\
 $(gsmatrix_h) $(gsstruct_h) $(gxfixed_h) $(gxdevmem_h) $(gxfcache_h)\
 $(gxfont_h) $(gxfont0_h) $(gxchar_h) $(gxdevice_h)

# ---------------- Pattern color ---------------- #

patlib_=gspcolor.$(OBJ) gxclip2.$(OBJ) gxpcmap.$(OBJ)
patlib.dev: $(LIB_MAK) $(ECHOGS_XE) cmyklib.dev psl2cs.dev $(patlib_)
	$(SETMOD) patlib -include cmyklib psl2cs
	$(ADDMOD) patlib -obj $(patlib_)

gspcolor.$(OBJ): gspcolor.c $(GXERR) $(math__h) \
  $(gsimage_h) $(gspath_h) $(gsrop_h) $(gsstruct_h) $(gsutil_h) \
  $(gxarith_h) $(gxcolor2_h) $(gxcoord_h) $(gxclip2_h) $(gxcspace_h) \
  $(gxdcolor_h) $(gxdevice_h) $(gxdevmem_h) \
  $(gxfixed_h) $(gxmatrix_h) $(gxpath_h) $(gxpcolor_h) $(gzstate_h)

gxclip2.$(OBJ): gxclip2.c $(GXERR) $(memory__h) \
  $(gsstruct_h) $(gxclip2_h) $(gxdevice_h) $(gxdevmem_h)

gxpcmap.$(OBJ): gxpcmap.c $(GXERR) $(math__h) $(memory__h)\
 $(gsstruct_h) $(gsutil_h)\
 $(gxcolor2_h) $(gxcspace_h) $(gxdcolor_h) $(gxdevice_h) $(gxdevmem_h)\
 $(gxfixed_h) $(gxmatrix_h) $(gxpcolor_h)\
 $(gzcpath_h) $(gzpath_h) $(gzstate_h)

# ---------------- PostScript Type 1 (and Type 4) fonts ---------------- #

type1lib_=gxtype1.$(OBJ) gxhint1.$(OBJ) gxhint2.$(OBJ) gxhint3.$(OBJ)

gscrypt1_h=gscrypt1.h
gstype1_h=gstype1.h
gxfont1_h=gxfont1.h
gxop1_h=gxop1.h
gxtype1_h=gxtype1.h $(gscrypt1_h) $(gstype1_h) $(gxop1_h)

gxtype1.$(OBJ): gxtype1.c $(GXERR) $(math__h)\
 $(gsccode_h) $(gsline_h) $(gsstruct_h)\
 $(gxarith_h) $(gxcoord_h) $(gxfixed_h) $(gxmatrix_h)\
 $(gxfont_h) $(gxfont1_h) $(gxistate_h) $(gxtype1_h)\
 $(gzpath_h)

gxhint1.$(OBJ): gxhint1.c $(GXERR)\
 $(gxarith_h) $(gxfixed_h) $(gxmatrix_h) $(gxchar_h)\
 $(gxfont_h) $(gxfont1_h) $(gxtype1_h)

gxhint2.$(OBJ): gxhint2.c $(GXERR) $(memory__h)\
 $(gxarith_h) $(gxfixed_h) $(gxmatrix_h) $(gxchar_h)\
 $(gxfont_h) $(gxfont1_h) $(gxtype1_h)

gxhint3.$(OBJ): gxhint3.c $(GXERR) $(math__h)\
 $(gxarith_h) $(gxfixed_h) $(gxmatrix_h) $(gxchar_h)\
 $(gxfont_h) $(gxfont1_h) $(gxtype1_h)\
 $(gzpath_h)

# Type 1 charstrings

psf1lib_=gstype1.$(OBJ)
psf1lib.dev: $(LIB_MAK) $(ECHOGS_XE) $(psf1lib_) $(type1lib_)
	$(SETMOD) psf1lib $(psf1lib_)
	$(ADDMOD) psf1lib $(type1lib_)
	$(ADDMOD) psf1lib -init gstype1

gstype1.$(OBJ): gstype1.c $(GXERR) $(math__h) $(memory__h)\
 $(gsstruct_h)\
 $(gxarith_h) $(gxcoord_h) $(gxfixed_h) $(gxmatrix_h) $(gxchar_h)\
 $(gxfont_h) $(gxfont1_h) $(gxistate_h) $(gxtype1_h)\
 $(gzpath_h)

# Type 2 charstrings

psf2lib_=gstype2.$(OBJ)
psf2lib.dev: $(LIB_MAK) $(ECHOGS_XE) $(psf2lib_) $(type1lib_)
	$(SETMOD) psf2lib $(psf2lib_)
	$(ADDMOD) psf2lib $(type1lib_)
	$(ADDMOD) psf2lib -init gstype2

gstype2.$(OBJ): gstype2.c $(GXERR) $(math__h) $(memory__h)\
 $(gsstruct_h)\
 $(gxarith_h) $(gxcoord_h) $(gxfixed_h) $(gxmatrix_h) $(gxchar_h)\
 $(gxfont_h) $(gxfont1_h) $(gxistate_h) $(gxtype1_h)\
 $(gzpath_h)

# ---------------- TrueType and PostScript Type 42 fonts ---------------- #

ttflib_=gstype42.$(OBJ)
ttflib.dev: $(LIB_MAK) $(ECHOGS_XE) $(ttflib_)
	$(SETMOD) ttflib $(ttflib_)

gxfont42_h=gxfont42.h

gstype42.$(OBJ): gstype42.c $(GXERR) $(memory__h) \
  $(gsccode_h) $(gsmatrix_h) $(gsstruct_h) \
  $(gxfixed_h) $(gxfont_h) $(gxfont42_h) $(gxistate_h) $(gxpath_h)

# -------- Level 1 color extensions (CMYK color and colorimage) -------- #

cmyklib_=gscolor1.$(OBJ) gsht1.$(OBJ)
cmyklib.dev: $(LIB_MAK) $(ECHOGS_XE) $(cmyklib_)
	$(SETMOD) cmyklib $(cmyklib_)
	$(ADDMOD) cmyklib -init gscolor1

gscolor1.$(OBJ): gscolor1.c $(GXERR) \
  $(gsccolor_h) $(gscolor1_h) $(gsstruct_h) $(gsutil_h) \
  $(gxcmap_h) $(gxcspace_h) $(gxdcconv_h) $(gxdevice_h) \
  $(gzstate_h)

gsht1.$(OBJ): gsht1.c $(GXERR) $(memory__h)\
 $(gsstruct_h) $(gsutil_h) $(gxdevice_h) $(gzht_h) $(gzstate_h)

colimlib_=gximage3.$(OBJ)
colimlib.dev: $(LIB_MAK) $(ECHOGS_XE) $(colimlib_)
	$(SETMOD) colimlib $(colimlib_)
	$(ADDMOD) colimlib -init gximage3

gximage3.$(OBJ): gximage3.c $(GXERR) $(memory__h) $(gpcheck_h)\
 $(gsccolor_h) $(gspaint_h)\
 $(gxarith_h) $(gxcmap_h) $(gxcpath_h) $(gxdcconv_h) $(gxdcolor_h)\
 $(gxdevice_h) $(gxdevmem_h) $(gxfixed_h) $(gxfrac_h)\
 $(gximage_h) $(gxistate_h) $(gxmatrix_h)\
 $(gzpath_h) $(gzstate_h)

# ---------------- HSB color ---------------- #

hsblib_=gshsb.$(OBJ)
hsblib.dev: $(LIB_MAK) $(ECHOGS_XE) $(hsblib_)
	$(SETMOD) hsblib $(hsblib_)

gshsb.$(OBJ): gshsb.c $(GX) \
  $(gscolor_h) $(gshsb_h) $(gxfrac_h)

# ---- Level 1 path miscellany (arcs, pathbbox, path enumeration) ---- #

path1lib_=gspath1.$(OBJ)
path1lib.dev: $(LIB_MAK) $(ECHOGS_XE) $(path1lib_)
	$(SETMOD) path1lib $(path1lib_)

gspath1.$(OBJ): gspath1.c $(GXERR) $(math__h) \
  $(gscoord_h) $(gspath_h) $(gsstruct_h) \
  $(gxfarith_h) $(gxfixed_h) $(gxmatrix_h) \
  $(gzstate_h) $(gzpath_h)

# --------------- Level 2 color space and color image support --------------- #

psl2cs_=gscolor2.$(OBJ)
psl2cs.dev: $(LIB_MAK) $(ECHOGS_XE) $(psl2cs_)
	$(SETMOD) psl2cs $(psl2cs_)

gscolor2.$(OBJ): gscolor2.c $(GXERR) \
  $(gxarith_h) $(gxcolor2_h) $(gxcspace_h) $(gxfixed_h) $(gxmatrix_h) \
  $(gzstate_h)

psl2lib_=gximage4.$(OBJ) gximage5.$(OBJ)
psl2lib.dev: $(LIB_MAK) $(ECHOGS_XE) $(psl2lib_) colimlib.dev psl2cs.dev
	$(SETMOD) psl2lib $(psl2lib_)
	$(ADDMOD) psl2lib -init gximage4 gximage5
	$(ADDMOD) psl2lib -include colimlib psl2cs

gximage4.$(OBJ): gximage4.c $(GXERR) $(memory__h) $(gpcheck_h)\
 $(gsccolor_h) $(gspaint_h)\
 $(gxarith_h) $(gxcmap_h) $(gxcpath_h) $(gxdcolor_h) $(gxdevice_h)\
 $(gxdevmem_h) $(gxfixed_h) $(gxfrac_h) $(gximage_h) $(gxistate_h)\
 $(gxmatrix_h)\
 $(gzpath_h)

gximage5.$(OBJ): gximage5.c $(GXERR) $(math__h) $(memory__h) $(gpcheck_h)\
 $(gsccolor_h) $(gspaint_h)\
 $(gxarith_h) $(gxcmap_h) $(gxcpath_h) $(gxdcolor_h) $(gxdevice_h)\
 $(gxdevmem_h) $(gxfixed_h) $(gxfrac_h) $(gximage_h) $(gxistate_h)\
 $(gxmatrix_h)\
 $(gzpath_h)

# ---------------- Display Postscript / Level 2 support ---------------- #

dps2lib_=gsdps1.$(OBJ)
dps2lib.dev: $(LIB_MAK) $(ECHOGS_XE) $(dps2lib_)
	$(SETMOD) dps2lib $(dps2lib_)

gsdps1.$(OBJ): gsdps1.c $(GXERR) $(math__h)\
 $(gscoord_h) $(gsmatrix_h) $(gspaint_h) $(gspath_h) $(gspath2_h)\
 $(gxdevice_h) $(gxfixed_h) $(gxmatrix_h) $(gzcpath_h) $(gzpath_h) $(gzstate_h)

# ---------------- Display Postscript extensions ---------------- #

gsdps_h=gsdps.h

dpslib_=gsdps.$(OBJ)
dpslib.dev: $(LIB_MAK) $(ECHOGS_XE) $(dpslib_)
	$(SETMOD) dpslib $(dpslib_)

gsdps.$(OBJ): gsdps.c $(GX) $(gsdps_h)\
 $(gsdps_h) $(gspath_h) $(gxdevice_h) $(gzcpath_h) $(gzpath_h) $(gzstate_h)

# ---------------- CIE color ---------------- #

cielib_=gscie.$(OBJ) gxctable.$(OBJ)
cielib.dev: $(LIB_MAK) $(ECHOGS_XE) $(cielib_)
	$(SETMOD) cielib $(cielib_)

gscie.$(OBJ): gscie.c $(GXERR) $(math__h) \
  $(gscie_h) $(gscolor2_h) $(gsmatrix_h) $(gsstruct_h) \
  $(gxarith_h) $(gxcmap_h) $(gxcspace_h) $(gxdevice_h) $(gzstate_h)

gxctable.$(OBJ): gxctable.c $(GX) \
  $(gxfixed_h) $(gxfrac_h) $(gxctable_h)

# ---------------- Separation colors ---------------- #

seprlib_=gscsepr.$(OBJ)
seprlib.dev: $(LIB_MAK) $(ECHOGS_XE) $(seprlib_)
	$(SETMOD) seprlib $(seprlib_)

gscsepr.$(OBJ): gscsepr.c $(GXERR)\
 $(gscsepr_h) $(gsmatrix_h) $(gsrefct_h)\
 $(gxcolor2_h) $(gxcspace_h) $(gxfixed_h) $(gzstate_h)

# ---------------- Functions ---------------- #

gsdsrc_h=gsdsrc.h $(gsstruct_h)
gsfunc_h=gsfunc.h
gsfunc0_h=gsfunc0.h $(gsdsrc_h) $(gsfunc_h)
gxfunc_h=gxfunc.h $(gsfunc_h) $(gsstruct_h)

# Generic support, and FunctionType 0.
funclib_=gsdsrc.$(OBJ) gsfunc.$(OBJ) gsfunc0.$(OBJ)
funclib.dev: $(LIB_MAK) $(ECHOGS_XE) $(funclib_)
	$(SETMOD) funclib $(funclib_)

gsdsrc.$(OBJ): gsdsrc.c $(GX) $(memory__h)\
 $(gsdsrc_h) $(gserrors_h) $(stream_h)

gsfunc.$(OBJ): gsfunc.c $(GX)\
 $(gserrors_h) $(gxfunc_h)

gsfunc0.$(OBJ): gsfunc0.c $(GX) $(math__h)\
 $(gserrors_h) $(gsfunc0_h) $(gxfunc_h)

# ----------------------- Platform-specific modules ----------------------- #
# Platform-specific code doesn't really belong here: this is code that is
# shared among multiple platforms.

# Frame buffer implementations.

gp_nofb.$(OBJ): gp_nofb.c $(GX) \
  $(gp_h) $(gxdevice_h)

gp_dosfb.$(OBJ): gp_dosfb.c $(AK) $(malloc__h) $(memory__h)\
 $(gx_h) $(gp_h) $(gserrors_h) $(gxdevice_h)

# MS-DOS file system, also used by Desqview/X.
gp_dosfs.$(OBJ): gp_dosfs.c $(AK) $(dos__h) $(gp_h) $(gx_h)

# MS-DOS file enumeration, *not* used by Desqview/X.
gp_dosfe.$(OBJ): gp_dosfe.c $(AK) $(stdio__h) $(memory__h) $(string__h) \
  $(dos__h) $(gstypes_h) $(gsmemory_h) $(gsstruct_h) $(gp_h) $(gsutil_h)

# Other MS-DOS facilities.
gp_msdos.$(OBJ): gp_msdos.c $(AK) $(dos__h) $(stdio__h) $(string__h)\
 $(gsmemory_h) $(gstypes_h) $(gp_h)

# Unix(-like) file system, also used by Desqview/X.
gp_unifs.$(OBJ): gp_unifs.c $(AK) $(memory__h) $(string__h) $(gx_h) $(gp_h) \
  $(gsstruct_h) $(gsutil_h) $(stat__h) $(dirent__h)

# Unix(-like) file name syntax, *not* used by Desqview/X.
gp_unifn.$(OBJ): gp_unifn.c $(AK) $(gx_h) $(gp_h)

# ----------------------------- Main program ------------------------------ #

# Main program for library testing

gslib.$(OBJ): gslib.c $(AK) $(math__h) \
  $(gx_h) $(gp_h) $(gserrors_h) $(gsmatrix_h) $(gsstate_h) $(gscspace_h) \
  $(gscdefs_h) $(gscolor2_h) $(gscoord_h) $(gslib_h) $(gsparam_h) \
  $(gspaint_h) $(gspath_h) $(gsstruct_h) $(gsutil_h) \
  $(gxalloc_h) $(gxdevice_h)
#    Copyright (C) 1995, 1996, 1997 Aladdin Enterprises.  All rights reserved.
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

# (Platform-independent) makefile for language interpreters.
# See the end of gs.mak for where this fits into the build process.

# Define the name of this makefile.
INT_MAK=int.mak

# ======================== Interpreter support ======================== #

# This is support code for all interpreters, not just PostScript and PDF.
# It knows about the PostScript data types, but isn't supposed to
# depend on anything outside itself.

errors_h=errors.h
idebug_h=idebug.h
idict_h=idict.h
igc_h=igc.h
igcstr_h=igcstr.h
iname_h=iname.h
inamedef_h=inamedef.h $(gconfigv_h) $(iname_h)
ipacked_h=ipacked.h
iref_h=iref.h
isave_h=isave.h
isstate_h=isstate.h
istruct_h=istruct.h $(gsstruct_h)
iutil_h=iutil.h
ivmspace_h=ivmspace.h $(gsgc_h)
opdef_h=opdef.h
# Nested include files
ghost_h=ghost.h $(gx_h) $(iref_h)
imemory_h=imemory.h $(gsalloc_h) $(ivmspace_h)
ialloc_h=ialloc.h $(imemory_h)
iastruct_h=iastruct.h $(gxobj_h) $(ialloc_h)
iastate_h=iastate.h $(gxalloc_h) $(ialloc_h) $(istruct_h)
store_h=store.h $(ialloc_h)

GH=$(AK) $(ghost_h)

isupport1_=ialloc.$(OBJ) igc.$(OBJ) igcref.$(OBJ) igcstr.$(OBJ)
isupport2_=ilocate.$(OBJ) iname.$(OBJ) isave.$(OBJ)
isupport_=$(isupport1_) $(isupport2_)
isupport.dev: $(INT_MAK) $(ECHOGS_XE) $(isupport_)
	$(SETMOD) isupport $(isupport1_)
	$(ADDMOD) isupport -obj $(isupport2_)
	$(ADDMOD) isupport -init igcref

ialloc.$(OBJ): ialloc.c $(AK) $(memory__h) $(gx_h)\
 $(errors_h) $(gsstruct_h) $(gxarith_h)\
 $(iastate_h) $(iref_h) $(ivmspace_h) $(store_h)

# igc.c, igcref.c, and igcstr.c should really be in the dpsand2 list,
# but since all the GC enumeration and relocation routines refer to them,
# it's too hard to separate them out from the Level 1 base.
igc.$(OBJ): igc.c $(GH) $(memory__h)\
  $(errors_h) $(gsexit_h) $(gsmdebug_h) $(gsstruct_h) $(gsutil_h) \
  $(iastate_h) $(idict_h) $(igc_h) $(igcstr_h) $(inamedef_h) \
  $(ipacked_h) $(isave_h) $(isstate_h) $(istruct_h) $(opdef_h)

igcref.$(OBJ): igcref.c $(GH) $(memory__h)\
 $(gsexit_h) $(gsstruct_h)\
 $(iastate_h) $(idebug_h) $(igc_h) $(iname_h) $(ipacked_h) $(store_h)

igcstr.$(OBJ): igcstr.c $(GH) $(memory__h)\
 $(gsmdebug_h) $(gsstruct_h) $(iastate_h) $(igcstr_h)

ilocate.$(OBJ): ilocate.c $(GH) $(memory__h)\
 $(errors_h) $(gsexit_h) $(gsstruct_h)\
 $(iastate_h) $(idict_h) $(igc_h) $(igcstr_h) $(iname_h)\
 $(ipacked_h) $(isstate_h) $(iutil_h) $(ivmspace_h)\
 $(store_h)

iname.$(OBJ): iname.c $(GH) $(memory__h) $(string__h)\
 $(gsstruct_h) $(gxobj_h)\
 $(errors_h) $(imemory_h) $(inamedef_h) $(isave_h) $(store_h)

isave.$(OBJ): isave.c $(GH) $(memory__h)\
 $(errors_h) $(gsexit_h) $(gsstruct_h) $(gsutil_h)\
 $(iastate_h) $(inamedef_h) $(isave_h) $(isstate_h) $(ivmspace_h)\
 $(ipacked_h) $(store_h)

### Include files

idparam_h=idparam.h
ilevel_h=ilevel.h
iparam_h=iparam.h $(gsparam_h)
istack_h=istack.h
iutil2_h=iutil2.h
opcheck_h=opcheck.h
opextern_h=opextern.h
# Nested include files
dstack_h=dstack.h $(istack_h)
estack_h=estack.h $(istack_h)
ostack_h=ostack.h $(istack_h)
oper_h=oper.h $(iutil_h) $(opcheck_h) $(opdef_h) $(opextern_h) $(ostack_h)

idebug.$(OBJ): idebug.c $(GH) $(string__h)\
 $(ialloc_h) $(idebug_h) $(idict_h) $(iname_h) $(istack_h) $(iutil_h) $(ivmspace_h)\
 $(ostack_h) $(opdef_h) $(ipacked_h) $(store_h)

idict.$(OBJ): idict.c $(GH) $(string__h) $(errors_h)\
 $(ialloc_h) $(idebug_h) $(ivmspace_h) $(inamedef_h) $(ipacked_h)\
 $(isave_h) $(store_h) $(iutil_h) $(idict_h) $(dstack_h)

idparam.$(OBJ): idparam.c $(GH) $(memory__h) $(string__h) $(errors_h)\
 $(gsmatrix_h) $(gsuid_h)\
 $(idict_h) $(idparam_h) $(ilevel_h) $(imemory_h) $(iname_h) $(iutil_h)\
 $(oper_h) $(store_h)

iparam.$(OBJ): iparam.c $(GH) $(memory__h) $(string__h) $(errors_h)\
 $(ialloc_h) $(idict_h) $(iname_h) $(imemory_h) $(iparam_h) $(istack_h) $(iutil_h) $(ivmspace_h)\
 $(opcheck_h) $(store_h)

istack.$(OBJ): istack.c $(GH) $(memory__h) \
  $(errors_h) $(gsstruct_h) $(gsutil_h) \
  $(ialloc_h) $(istack_h) $(istruct_h) $(iutil_h) $(ivmspace_h) $(store_h)

iutil.$(OBJ): iutil.c $(GH) $(math__h) $(memory__h) $(string__h)\
 $(gsccode_h) $(gsmatrix_h) $(gsutil_h) $(gxfont_h)\
 $(errors_h) $(idict_h) $(imemory_h) $(iutil_h) $(ivmspace_h)\
 $(iname_h) $(ipacked_h) $(oper_h) $(store_h)

# ======================== PostScript Level 1 ======================== #

###### Include files

files_h=files.h
fname_h=fname.h
ichar_h=ichar.h
icharout_h=icharout.h
icolor_h=icolor.h
icontext_h=icontext.h $(imemory_h) $(istack_h)
icsmap_h=icsmap.h
ifont_h=ifont.h $(gsccode_h) $(gsstruct_h)
iht_h=iht.h
iimage_h=iimage.h
imain_h=imain.h $(gsexit_h)
imainarg_h=imainarg.h
iminst_h=iminst.h $(imain_h)
interp_h=interp.h
iparray_h=iparray.h
iscannum_h=iscannum.h
istream_h=istream.h
main_h=main.h $(iminst_h)
overlay_h=overlay.h
sbwbs_h=sbwbs.h
sfilter_h=sfilter.h $(gstypes_h)
shcgen_h=shcgen.h
smtf_h=smtf.h
# Nested include files
bfont_h=bfont.h $(ifont_h)
ifilter_h=ifilter.h $(istream_h) $(ivmspace_h)
igstate_h=igstate.h $(gsstate_h) $(gxstate_h) $(istruct_h)
iscan_h=iscan.h $(sa85x_h) $(sstring_h)
sbhc_h=sbhc.h $(shc_h)
# Include files for optional features
ibnum_h=ibnum.h

### Initialization and scanning

iconfig=iconfig$(CONFIG)
$(iconfig).$(OBJ): iconf.c $(stdio__h) \
  $(gconfig_h) $(gscdefs_h) $(gsmemory_h) \
  $(files_h) $(iminst_h) $(iref_h) $(ivmspace_h) $(opdef_h) $(stream_h)
	$(RM_) gconfig.h
	$(RM_) $(iconfig).c
	$(CP_) $(gconfig_h) gconfig.h
	$(CP_) iconf.c $(iconfig).c
	$(CCC) $(iconfig).c
	$(RM_) gconfig.h
	$(RM_) $(iconfig).c

iinit.$(OBJ): iinit.c $(GH) $(string__h)\
 $(gscdefs_h) $(gsexit_h) $(gsstruct_h)\
 $(ialloc_h) $(idict_h) $(dstack_h) $(errors_h)\
 $(ilevel_h) $(iname_h) $(interp_h) $(opdef_h)\
 $(ipacked_h) $(iparray_h) $(iutil_h) $(ivmspace_h) $(store_h)

iscan.$(OBJ): iscan.c $(GH) $(memory__h)\
 $(ialloc_h) $(idict_h) $(dstack_h) $(errors_h) $(files_h)\
 $(ilevel_h) $(iutil_h) $(iscan_h) $(iscannum_h) $(istruct_h) $(ivmspace_h)\
 $(iname_h) $(ipacked_h) $(iparray_h) $(istream_h) $(ostack_h) $(store_h)\
 $(stream_h) $(strimpl_h) $(sfilter_h) $(scanchar_h)

iscannum.$(OBJ): iscannum.c $(GH) $(math__h)\
  $(errors_h) $(iscannum_h) $(scanchar_h) $(scommon_h) $(store_h)

### Streams

sfilter1.$(OBJ): sfilter1.c $(AK) $(stdio__h) $(memory__h) \
  $(sfilter_h) $(strimpl_h)

###### Operators

OP=$(GH) $(errors_h) $(oper_h)

### Non-graphics operators

zarith.$(OBJ): zarith.c $(OP) $(math__h) $(store_h)

zarray.$(OBJ): zarray.c $(OP) $(memory__h) $(ialloc_h) $(ipacked_h) $(store_h)

zcontrol.$(OBJ): zcontrol.c $(OP) $(string__h)\
 $(estack_h) $(files_h) $(ipacked_h) $(iutil_h) $(store_h) $(stream_h)

zdict.$(OBJ): zdict.c $(OP) \
  $(dstack_h) $(idict_h) $(ilevel_h) $(iname_h) $(ipacked_h) $(ivmspace_h) \
  $(store_h)

zfile.$(OBJ): zfile.c $(OP) $(memory__h) $(string__h) $(gp_h)\
 $(gsstruct_h) $(gxiodev_h) \
 $(ialloc_h) $(estack_h) $(files_h) $(fname_h) $(ilevel_h) $(interp_h) $(iutil_h)\
 $(isave_h) $(main_h) $(sfilter_h) $(stream_h) $(strimpl_h) $(store_h)

zfileio.$(OBJ): zfileio.c $(OP) $(gp_h) \
  $(files_h) $(ifilter_h) $(store_h) $(stream_h) $(strimpl_h) \
  $(gsmatrix_h) $(gxdevice_h) $(gxdevmem_h)

zfilter.$(OBJ): zfilter.c $(OP) $(memory__h)\
 $(gsstruct_h) $(files_h) $(ialloc_h) $(idict_h) $(idparam_h) $(ifilter_h) \
 $(sfilter_h) $(srlx_h) $(sstring_h) $(store_h) $(stream_h) $(strimpl_h)

zfname.$(OBJ): zfname.c $(OP) $(memory__h)\
 $(fname_h) $(gxiodev_h) $(ialloc_h) $(stream_h)

zfproc.$(OBJ): zfproc.c $(GH) $(memory__h)\
 $(errors_h) $(oper_h)\
 $(estack_h) $(files_h) $(gsstruct_h) $(ialloc_h) $(ifilter_h) $(istruct_h)\
 $(store_h) $(stream_h) $(strimpl_h)

zgeneric.$(OBJ): zgeneric.c $(OP) $(memory__h)\
 $(idict_h) $(estack_h) $(ivmspace_h) $(iname_h) $(ipacked_h) $(store_h)

ziodev.$(OBJ): ziodev.c $(OP) $(memory__h) $(stdio__h) $(string__h)\
 $(gp_h) $(gpcheck_h)\
 $(gsstruct_h) $(gxiodev_h)\
 $(files_h) $(ialloc_h) $(ivmspace_h) $(store_h) $(stream_h)

zmath.$(OBJ): zmath.c $(OP) $(math__h) $(gxfarith_h) $(store_h)

zmisc.$(OBJ): zmisc.c $(OP) $(gscdefs_h) $(gp_h) \
  $(errno__h) $(memory__h) $(string__h) \
  $(ialloc_h) $(idict_h) $(dstack_h) $(iname_h) $(ivmspace_h) $(ipacked_h) $(store_h)

zpacked.$(OBJ): zpacked.c $(OP) \
  $(ialloc_h) $(idict_h) $(ivmspace_h) $(iname_h) $(ipacked_h) $(iparray_h) \
  $(istack_h) $(store_h)

zrelbit.$(OBJ): zrelbit.c $(OP) $(gsutil_h) $(store_h) $(idict_h)

zstack.$(OBJ): zstack.c $(OP) $(memory__h)\
 $(ialloc_h) $(istack_h) $(store_h)

zstring.$(OBJ): zstring.c $(OP) $(memory__h)\
 $(gsutil_h)\
 $(ialloc_h) $(iname_h) $(ivmspace_h) $(store_h)

zsysvm.$(OBJ): zsysvm.c $(GH)\
 $(ialloc_h) $(ivmspace_h) $(oper_h) $(store_h)

ztoken.$(OBJ): ztoken.c $(OP) \
  $(estack_h) $(files_h) $(gsstruct_h) $(iscan_h) \
  $(sfilter_h) $(store_h) $(stream_h) $(strimpl_h)

ztype.$(OBJ): ztype.c $(OP) $(math__h) $(memory__h) $(string__h)\
 $(dstack_h) $(idict_h) $(imemory_h) $(iname_h)\
 $(iscan_h) $(iutil_h) $(sfilter_h) $(store_h) $(stream_h) $(strimpl_h)

zvmem.$(OBJ): zvmem.c $(OP)\
 $(dstack_h) $(estack_h) $(files_h)\
 $(ialloc_h) $(idict_h) $(igstate_h) $(isave_h) $(store_h) $(stream_h)\
 $(gsmatrix_h) $(gsstate_h) $(gsstruct_h)

### Graphics operators

zchar.$(OBJ): zchar.c $(OP)\
 $(gsstruct_h) $(gxarith_h) $(gxfixed_h) $(gxmatrix_h)\
 $(gxchar_h) $(gxdevice_h) $(gxfont_h) $(gzpath_h) $(gzstate_h)\
 $(dstack_h) $(estack_h) $(ialloc_h) $(ichar_h) $(idict_h) $(ifont_h)\
 $(ilevel_h) $(iname_h) $(igstate_h) $(ipacked_h) $(store_h)

# zcharout is used for Type 1 and Type 42 fonts only.
zcharout.$(OBJ): zcharout.c $(OP)\
 $(gschar_h) $(gxdevice_h) $(gxfont_h)\
 $(dstack_h) $(estack_h) $(ichar_h) $(icharout_h)\
 $(idict_h) $(ifont_h) $(igstate_h) $(store_h)

zcolor.$(OBJ): zcolor.c $(OP) \
  $(gxfixed_h) $(gxmatrix_h) $(gzstate_h) $(gxdevice_h) $(gxcmap_h) \
  $(ialloc_h) $(icolor_h) $(estack_h) $(iutil_h) $(igstate_h) $(store_h)

zdevice.$(OBJ): zdevice.c $(OP) $(string__h)\
 $(ialloc_h) $(idict_h) $(igstate_h) $(iname_h) $(interp_h) $(iparam_h) $(ivmspace_h)\
 $(gsmatrix_h) $(gsstate_h) $(gxdevice_h) $(store_h)

zfont.$(OBJ): zfont.c $(OP)\
 $(gschar_h) $(gsstruct_h) $(gxdevice_h) $(gxfont_h) $(gxfcache_h)\
 $(gzstate_h)\
 $(ialloc_h) $(idict_h) $(igstate_h) $(iname_h) $(isave_h) $(ivmspace_h)\
 $(bfont_h) $(store_h)

zfont2.$(OBJ): zfont2.c $(OP) $(memory__h) $(string__h)\
 $(gsmatrix_h) $(gxdevice_h) $(gschar_h) $(gxfixed_h) $(gxfont_h)\
 $(ialloc_h) $(bfont_h) $(idict_h) $(idparam_h) $(ilevel_h) $(iname_h) $(istruct_h)\
 $(ipacked_h) $(store_h)

zgstate.$(OBJ): zgstate.c $(OP) $(math__h)\
 $(gsmatrix_h) $(ialloc_h) $(idict_h) $(igstate_h) $(istruct_h) $(store_h)

zht.$(OBJ): zht.c $(OP) $(memory__h)\
 $(gsmatrix_h) $(gsstate_h) $(gsstruct_h) $(gxdevice_h) $(gzht_h) \
 $(ialloc_h) $(estack_h) $(igstate_h) $(iht_h) $(store_h)

zimage.$(OBJ): zimage.c $(OP) \
  $(estack_h) $(ialloc_h) $(ifilter_h) $(igstate_h) $(iimage_h) $(ilevel_h) \
  $(gscspace_h) $(gsimage_h) $(gsmatrix_h) $(gsstruct_h) \
  $(store_h) $(stream_h)

zmatrix.$(OBJ): zmatrix.c $(OP)\
 $(gsmatrix_h) $(igstate_h) $(gscoord_h) $(store_h)

zpaint.$(OBJ): zpaint.c $(OP)\
 $(gspaint_h) $(igstate_h)

zpath.$(OBJ): zpath.c $(OP) $(math__h) \
  $(gsmatrix_h) $(gspath_h) $(igstate_h) $(store_h)

# Define the base PostScript language interpreter.
# This is the subset of PostScript Level 1 required by our PDF reader.

INT1=idebug.$(OBJ) idict.$(OBJ) idparam.$(OBJ)
INT2=iinit.$(OBJ) interp.$(OBJ) iparam.$(OBJ) ireclaim.$(OBJ)
INT3=iscan.$(OBJ) iscannum.$(OBJ) istack.$(OBJ) iutil.$(OBJ)
INT4=scantab.$(OBJ) sfilter1.$(OBJ) sstring.$(OBJ) stream.$(OBJ)
Z1=zarith.$(OBJ) zarray.$(OBJ) zcontrol.$(OBJ) zdict.$(OBJ)
Z1OPS=zarith zarray zcontrol zdict
Z2=zfile.$(OBJ) zfileio.$(OBJ) zfilter.$(OBJ) zfname.$(OBJ) zfproc.$(OBJ)
Z2OPS=zfile zfileio zfilter zfproc
Z3=zgeneric.$(OBJ) ziodev.$(OBJ) zmath.$(OBJ) zmisc.$(OBJ) zpacked.$(OBJ)
Z3OPS=zgeneric ziodev zmath zmisc zpacked
Z4=zrelbit.$(OBJ) zstack.$(OBJ) zstring.$(OBJ) zsysvm.$(OBJ)
Z4OPS=zrelbit zstack zstring zsysvm
Z5=ztoken.$(OBJ) ztype.$(OBJ) zvmem.$(OBJ)
Z5OPS=ztoken ztype zvmem
Z6=zchar.$(OBJ) zcolor.$(OBJ) zdevice.$(OBJ) zfont.$(OBJ) zfont2.$(OBJ)
Z6OPS=zchar zcolor zdevice zfont zfont2
Z7=zgstate.$(OBJ) zht.$(OBJ) zimage.$(OBJ) zmatrix.$(OBJ) zpaint.$(OBJ) zpath.$(OBJ)
Z7OPS=zgstate zht zimage zmatrix zpaint zpath
# We have to be a little underhanded with *config.$(OBJ) so as to avoid
# circular definitions.
INT_OBJS=imainarg.$(OBJ) gsargs.$(OBJ) imain.$(OBJ) \
  $(INT1) $(INT2) $(INT3) $(INT4) \
  $(Z1) $(Z2) $(Z3) $(Z4) $(Z5) $(Z6) $(Z7)
INT_CONFIG=$(gconfig).$(OBJ) $(gscdefs).$(OBJ) $(iconfig).$(OBJ) \
  iccinit$(COMPILE_INITS).$(OBJ)
INT_ALL=$(INT_OBJS) $(INT_CONFIG)
# We omit libcore.dev, which should be included here, because problems
# with the Unix linker require libcore to appear last in the link list
# when libcore is really a library.
# We omit $(INT_CONFIG) from the dependency list because they have special
# dependency requirements and are added to the link list at the very end.
# zfilter.c shouldn't include the RLE and RLD filters, but we don't want to
# change this now.
psbase.dev: $(INT_MAK) $(ECHOGS_XE) $(INT_OBJS)\
 isupport.dev rld.dev rle.dev sfile.dev
	$(SETMOD) psbase imainarg.$(OBJ) gsargs.$(OBJ) imain.$(OBJ)
	$(ADDMOD) psbase -obj $(INT_CONFIG)
	$(ADDMOD) psbase -obj $(INT1)
	$(ADDMOD) psbase -obj $(INT2)
	$(ADDMOD) psbase -obj $(INT3)
	$(ADDMOD) psbase -obj $(INT4)
	$(ADDMOD) psbase -obj $(Z1)
	$(ADDMOD) psbase -oper $(Z1OPS)
	$(ADDMOD) psbase -obj $(Z2)
	$(ADDMOD) psbase -oper $(Z2OPS)
	$(ADDMOD) psbase -obj $(Z3)
	$(ADDMOD) psbase -oper $(Z3OPS)
	$(ADDMOD) psbase -obj $(Z4)
	$(ADDMOD) psbase -oper $(Z4OPS)
	$(ADDMOD) psbase -obj $(Z5)
	$(ADDMOD) psbase -oper $(Z5OPS)
	$(ADDMOD) psbase -obj $(Z6)
	$(ADDMOD) psbase -oper $(Z6OPS)
	$(ADDMOD) psbase -obj $(Z7)
	$(ADDMOD) psbase -oper $(Z7OPS)
	$(ADDMOD) psbase -iodev stdin stdout stderr lineedit statementedit
	$(ADDMOD) psbase -include isupport rld rle sfile

# -------------------------- Feature definitions -------------------------- #

# ---------------- Full Level 1 interpreter ---------------- #

level1.dev: $(INT_MAK) $(ECHOGS_XE) psbase.dev bcp.dev hsb.dev path1.dev type1.dev
	$(SETMOD) level1 -include psbase bcp hsb path1 type1
	$(ADDMOD) level1 -emulator PostScript PostScriptLevel1

# -------- Level 1 color extensions (CMYK color and colorimage) -------- #

color.dev: $(INT_MAK) $(ECHOGS_XE) cmyklib.dev colimlib.dev cmykread.dev
	$(SETMOD) color -include cmyklib colimlib cmykread

cmykread_=zcolor1.$(OBJ) zht1.$(OBJ)
cmykread.dev: $(INT_MAK) $(ECHOGS_XE) $(cmykread_)
	$(SETMOD) cmykread $(cmykread_)
	$(ADDMOD) cmykread -oper zcolor1 zht1

zcolor1.$(OBJ): zcolor1.c $(OP) \
  $(gscolor1_h) \
  $(gxcmap_h) $(gxcspace_h) $(gxdevice_h) $(gxfixed_h) $(gxmatrix_h) \
  $(gzstate_h) \
  $(ialloc_h) $(icolor_h) $(iimage_h) $(estack_h) $(iutil_h) $(igstate_h) $(store_h)

zht1.$(OBJ): zht1.c $(OP) $(memory__h)\
 $(gsmatrix_h) $(gsstate_h) $(gsstruct_h) $(gxdevice_h) $(gzht_h)\
 $(ialloc_h) $(estack_h) $(igstate_h) $(iht_h) $(store_h)

# ---------------- HSB color ---------------- #

hsb_=zhsb.$(OBJ)
hsb.dev: $(INT_MAK) $(ECHOGS_XE) $(hsb_) hsblib.dev
	$(SETMOD) hsb $(hsb_)
	$(ADDMOD) hsb -include hsblib
	$(ADDMOD) hsb -oper zhsb

zhsb.$(OBJ): zhsb.c $(OP) \
  $(gshsb_h) $(igstate_h) $(store_h)

# ---- Level 1 path miscellany (arcs, pathbbox, path enumeration) ---- #

path1_=zpath1.$(OBJ)
path1.dev: $(INT_MAK) $(ECHOGS_XE) $(path1_) path1lib.dev
	$(SETMOD) path1 $(path1_)
	$(ADDMOD) path1 -include path1lib
	$(ADDMOD) path1 -oper zpath1

zpath1.$(OBJ): zpath1.c $(OP) $(memory__h)\
 $(ialloc_h) $(estack_h) $(gspath_h) $(gsstruct_h) $(igstate_h) $(store_h)

# ================ Level-independent PostScript options ================ #

# ---------------- BCP filters ---------------- #

bcp_=sbcp.$(OBJ) zfbcp.$(OBJ)
bcp.dev: $(INT_MAK) $(ECHOGS_XE) $(bcp_)
	$(SETMOD) bcp $(bcp_)
	$(ADDMOD) bcp -oper zfbcp

sbcp.$(OBJ): sbcp.c $(AK) $(stdio__h) \
  $(sfilter_h) $(strimpl_h)

zfbcp.$(OBJ): zfbcp.c $(OP) $(memory__h)\
 $(gsstruct_h) $(ialloc_h) $(ifilter_h)\
 $(sfilter_h) $(stream_h) $(strimpl_h)

# ---------------- Incremental font loading ---------------- #
# (This only works for Type 1 fonts without eexec encryption.)

diskfont.dev: $(INT_MAK) $(ECHOGS_XE)
	$(SETMOD) diskfont -ps gs_diskf

# ---------------- Double-precision floats ---------------- #

double_=zdouble.$(OBJ)
double.dev: $(INT_MAK) $(ECHOGS_XE) $(double_)
	$(SETMOD) double $(double_)
	$(ADDMOD) double -oper zdouble

zdouble.$(OBJ): zdouble.c $(OP) $(ctype__h) $(math__h) $(memory__h) $(string__h) \
  $(gxfarith_h) $(store_h)

# ---------------- EPSF files with binary headers ---------------- #

epsf.dev: $(INT_MAK) $(ECHOGS_XE)
	$(SETMOD) epsf -ps gs_epsf

# ---------------- RasterOp ---------------- #
# This should be a separable feature in the core also....

rasterop.dev: $(INT_MAK) $(ECHOGS_XE) roplib.dev ropread.dev
	$(SETMOD) rasterop -include roplib ropread

ropread_=zrop.$(OBJ)
ropread.dev: $(INT_MAK) $(ECHOGS_XE) $(ropread_)
	$(SETMOD) ropread $(ropread_)
	$(ADDMOD) ropread -oper zrop

zrop.$(OBJ): zrop.c $(OP) $(memory__h)\
 $(gsrop_h) $(gsutil_h) $(gxdevice_h)\
 $(idict_h) $(idparam_h) $(igstate_h) $(store_h)

# ---------------- PostScript Type 1 (and Type 4) fonts ---------------- #

type1.dev: $(INT_MAK) $(ECHOGS_XE) psf1lib.dev psf1read.dev
	$(SETMOD) type1 -include psf1lib psf1read

psf1read_=seexec.$(OBJ) zchar1.$(OBJ) zcharout.$(OBJ) zfont1.$(OBJ) zmisc1.$(OBJ)
psf1read.dev: $(INT_MAK) $(ECHOGS_XE) $(psf1read_)
	$(SETMOD) psf1read $(psf1read_)
	$(ADDMOD) psf1read -oper zchar1 zfont1 zmisc1
	$(ADDMOD) psf1read -ps gs_type1

seexec.$(OBJ): seexec.c $(AK) $(stdio__h) \
  $(gscrypt1_h) $(scanchar_h) $(sfilter_h) $(strimpl_h)

zchar1.$(OBJ): zchar1.c $(OP) \
  $(gspaint_h) $(gspath_h) $(gsstruct_h) \
  $(gxchar_h) $(gxdevice_h) $(gxfixed_h) $(gxmatrix_h) \
  $(gxfont_h) $(gxfont1_h) $(gxtype1_h) $(gzstate_h) \
  $(estack_h) $(ialloc_h) $(ichar_h) $(icharout_h) \
  $(idict_h) $(ifont_h) $(igstate_h) $(store_h)

zfont1.$(OBJ): zfont1.c $(OP) \
  $(gsmatrix_h) $(gxdevice_h) $(gschar_h) \
  $(gxfixed_h) $(gxfont_h) $(gxfont1_h) \
  $(bfont_h) $(ialloc_h) $(idict_h) $(idparam_h) $(store_h)

zmisc1.$(OBJ): zmisc1.c $(OP) $(memory__h)\
 $(gscrypt1_h)\
 $(idict_h) $(idparam_h) $(ifilter_h)\
 $(sfilter_h) $(stream_h) $(strimpl_h)

# -------------- Compact Font Format and Type 2 charstrings ------------- #

cff.dev: $(INT_MAK) $(ECHOGS_XE) gs_cff.ps psl2int.dev
	$(SETMOD) cff -ps gs_cff

type2.dev: $(INT_MAK) $(ECHOGS_XE) type1.dev psf2lib.dev
	$(SETMOD) type2 -include psf2lib

# ---------------- TrueType and PostScript Type 42 fonts ---------------- #

# Native TrueType support
ttfont.dev: $(INT_MAK) $(ECHOGS_XE) type42.dev
	$(SETMOD) ttfont -include type42
	$(ADDMOD) ttfont -ps gs_mro_e gs_wan_e gs_ttf

# Type 42 (embedded TrueType) support
type42read_=zchar42.$(OBJ) zcharout.$(OBJ) zfont42.$(OBJ)
type42.dev: $(INT_MAK) $(ECHOGS_XE) $(type42read_) ttflib.dev
	$(SETMOD) type42 $(type42read_)
	$(ADDMOD) type42 -include ttflib	
	$(ADDMOD) type42 -oper zchar42 zfont42
	$(ADDMOD) type42 -ps gs_typ42

zchar42.$(OBJ): zchar42.c $(OP) \
  $(gsmatrix_h) $(gspaint_h) $(gspath_h) \
  $(gxfixed_h) $(gxchar_h) $(gxfont_h) $(gxfont42_h) \
  $(gxistate_h) $(gxpath_h) $(gzstate_h) \
  $(dstack_h) $(estack_h) $(ichar_h) $(icharout_h) \
  $(ifont_h) $(igstate_h) $(store_h)

zfont42.$(OBJ): zfont42.c $(OP) \
  $(gsccode_h) $(gsmatrix_h) $(gxfont_h) $(gxfont42_h) \
  $(bfont_h) $(idict_h) $(idparam_h) $(store_h)

# ======================== Precompilation options ======================== #

# ---------------- Precompiled fonts ---------------- #
# See fonts.txt for more information.

ccfont_h=ccfont.h $(std_h) $(gsmemory_h) $(iref_h) $(ivmspace_h) $(store_h)

CCFONT=$(OP) $(ccfont_h)

# List the fonts we are going to compile.
# Because of intrinsic limitations in `make', we have to list
# the object file names and the font names separately.
# Because of limitations in the DOS shell, we have to break the fonts up
# into lists that will fit on a single line (120 characters).
# The rules for constructing the .c files from the fonts themselves,
# and for compiling the .c files, are in cfonts.mak, not here.
# For example, to compile the Courier fonts, you should invoke
#	make -f cfonts.mak Courier_o
# By convention, the names of the 35 standard compiled fonts use '0' for
# the foundry name.  This allows users to substitute different foundries
# without having to change this makefile.
ccfonts_ps=gs_ccfnt
ccfonts1_=0agk.$(OBJ) 0agko.$(OBJ) 0agd.$(OBJ) 0agdo.$(OBJ)
ccfonts1=agk agko agd agdo
ccfonts2_=0bkl.$(OBJ) 0bkli.$(OBJ) 0bkd.$(OBJ) 0bkdi.$(OBJ)
ccfonts2=bkl bkli bkd bkdi
ccfonts3_=0crr.$(OBJ) 0cri.$(OBJ) 0crb.$(OBJ) 0crbi.$(OBJ)
ccfonts3=crr cri crb crbi
ccfonts4_=0hvr.$(OBJ) 0hvro.$(OBJ) 0hvb.$(OBJ) 0hvbo.$(OBJ)
ccfonts4=hvr hvro hvb hvbo
ccfonts5_=0hvrrn.$(OBJ) 0hvrorn.$(OBJ) 0hvbrn.$(OBJ) 0hvborn.$(OBJ)
ccfonts5=hvrrn hvrorn hvbrn hvborn
ccfonts6_=0ncr.$(OBJ) 0ncri.$(OBJ) 0ncb.$(OBJ) 0ncbi.$(OBJ)
ccfonts6=ncr ncri ncb ncbi
ccfonts7_=0plr.$(OBJ) 0plri.$(OBJ) 0plb.$(OBJ) 0plbi.$(OBJ)
ccfonts7=plr plri plb plbi
ccfonts8_=0tmr.$(OBJ) 0tmri.$(OBJ) 0tmb.$(OBJ) 0tmbi.$(OBJ)
ccfonts8=tmr tmri tmb tmbi
ccfonts9_=0syr.$(OBJ) 0zcmi.$(OBJ) 0zdr.$(OBJ)
ccfonts9=syr zcmi zdr
# The free distribution includes Bitstream Charter, Utopia, and
# freeware Cyrillic and Kana fonts.  We only provide for compiling
# Charter and Utopia.
ccfonts10free_=bchr.$(OBJ) bchri.$(OBJ) bchb.$(OBJ) bchbi.$(OBJ)
ccfonts10free=chr chri chb chbi
ccfonts11free_=putr.$(OBJ) putri.$(OBJ) putb.$(OBJ) putbi.$(OBJ)
ccfonts11free=utr utri utb utbi
# Uncomment the alternatives in the next 4 lines if you want
# Charter and Utopia compiled in.
#ccfonts10_=$(ccfonts10free_)
ccfonts10_=
#ccfonts10=$(ccfonts10free)
ccfonts10=
#ccfonts11_=$(ccfonts11free_)
ccfonts11_=
#ccfonts11=$(ccfonts11free)
ccfonts11=
# Add your own fonts here if desired.
ccfonts12_=
ccfonts12=
ccfonts13_=
ccfonts13=
ccfonts14_=
ccfonts14=
ccfonts15_=
ccfonts15=

# It's OK for ccfonts_.dev not to be CONFIG-dependent, because it only
# exists during the execution of the following rule.
# font2c has the prefix "gs" built into it, so we need to instruct
# genconf to use the same one.
$(gconfigf_h): $(MAKEFILE) $(INT_MAK) $(GENCONF_XE)
	$(SETMOD) ccfonts_ -font $(ccfonts1)
	$(ADDMOD) ccfonts_ -font $(ccfonts2)
	$(ADDMOD) ccfonts_ -font $(ccfonts3)
	$(ADDMOD) ccfonts_ -font $(ccfonts4)
	$(ADDMOD) ccfonts_ -font $(ccfonts5)
	$(ADDMOD) ccfonts_ -font $(ccfonts6)
	$(ADDMOD) ccfonts_ -font $(ccfonts7)
	$(ADDMOD) ccfonts_ -font $(ccfonts8)
	$(ADDMOD) ccfonts_ -font $(ccfonts9)
	$(ADDMOD) ccfonts_ -font $(ccfonts10)
	$(ADDMOD) ccfonts_ -font $(ccfonts11)
	$(ADDMOD) ccfonts_ -font $(ccfonts12)
	$(ADDMOD) ccfonts_ -font $(ccfonts13)
	$(ADDMOD) ccfonts_ -font $(ccfonts14)
	$(ADDMOD) ccfonts_ -font $(ccfonts15)
	$(EXP)genconf ccfonts_.dev -n gs -f $(gconfigf_h)

# We separate icfontab.dev from ccfonts.dev so that a customer can put
# compiled fonts into a separate shared library.

icfontab=icfontab$(CONFIG)

# Define ccfont_table separately, so it can be set from the command line
# to select an alternate compiled font table.
ccfont_table=$(icfontab)

$(icfontab).dev: $(MAKEFILE) $(INT_MAK) $(ECHOGS_XE) $(icfontab).$(OBJ) \
  $(ccfonts1_) $(ccfonts2_) $(ccfonts3_) $(ccfonts4_) $(ccfonts5_) \
  $(ccfonts6_) $(ccfonts7_) $(ccfonts8_) $(ccfonts9_) $(ccfonts10_) \
  $(ccfonts11_) $(ccfonts12_) $(ccfonts13_) $(ccfonts14_) $(ccfonts15_)
	$(SETMOD) $(icfontab) -obj $(icfontab).$(OBJ)
	$(ADDMOD) $(icfontab) -obj $(ccfonts1_)
	$(ADDMOD) $(icfontab) -obj $(ccfonts2_)
	$(ADDMOD) $(icfontab) -obj $(ccfonts3_)
	$(ADDMOD) $(icfontab) -obj $(ccfonts4_)
	$(ADDMOD) $(icfontab) -obj $(ccfonts5_)
	$(ADDMOD) $(icfontab) -obj $(ccfonts6_)
	$(ADDMOD) $(icfontab) -obj $(ccfonts7_)
	$(ADDMOD) $(icfontab) -obj $(ccfonts8_)
	$(ADDMOD) $(icfontab) -obj $(ccfonts9_)
	$(ADDMOD) $(icfontab) -obj $(ccfonts10_)
	$(ADDMOD) $(icfontab) -obj $(ccfonts11_)
	$(ADDMOD) $(icfontab) -obj $(ccfonts12_)
	$(ADDMOD) $(icfontab) -obj $(ccfonts13_)
	$(ADDMOD) $(icfontab) -obj $(ccfonts14_)
	$(ADDMOD) $(icfontab) -obj $(ccfonts15_)

$(icfontab).$(OBJ): icfontab.c $(AK) $(ccfont_h) $(gconfigf_h)
	$(CP_) $(gconfigf_h) gconfigf.h
	$(CCCF) icfontab.c

# Strictly speaking, ccfonts shouldn't need to include type1,
# since one could choose to precompile only Type 0 fonts,
# but getting this exactly right would be too much work.
ccfonts=ccfonts$(CONFIG)
$(ccfonts).dev: $(MAKEFILE) $(INT_MAK) type1.dev iccfont.$(OBJ) \
  $(ccfont_table).dev
	$(SETMOD) $(ccfonts) -include type1
	$(ADDMOD) $(ccfonts) -include $(ccfont_table)
	$(ADDMOD) $(ccfonts) -obj iccfont.$(OBJ)
	$(ADDMOD) $(ccfonts) -oper ccfonts
	$(ADDMOD) $(ccfonts) -ps $(ccfonts_ps)

iccfont.$(OBJ): iccfont.c $(GH) $(string__h)\
 $(gsstruct_h) $(ccfont_h) $(errors_h)\
 $(ialloc_h) $(idict_h) $(ifont_h) $(iname_h) $(isave_h) $(iutil_h)\
 $(oper_h) $(ostack_h) $(store_h) $(stream_h) $(strimpl_h) $(sfilter_h) $(iscan_h)
	$(CCCF) iccfont.c

# ---------------- Compiled initialization code ---------------- #

# We select either iccinit0 or iccinit1 depending on COMPILE_INITS.

iccinit0.$(OBJ): iccinit0.c $(stdpre_h)
	$(CCCF) iccinit0.c

iccinit1.$(OBJ): gs_init.$(OBJ)
	$(CP_) gs_init.$(OBJ) iccinit1.$(OBJ)

# All the gs_*.ps files should be prerequisites of gs_init.c,
# but we don't have any convenient list of them.
gs_init.c: $(GS_INIT) $(GENINIT_XE) $(gconfig_h)
	$(EXP)geninit $(GS_INIT) $(gconfig_h) -c gs_init.c

gs_init.$(OBJ): gs_init.c $(stdpre_h)
	$(CCCF) gs_init.c

# ======================== PostScript Level 2 ======================== #

level2.dev: $(INT_MAK) $(ECHOGS_XE) \
 cidfont.dev cie.dev cmapread.dev compfont.dev dct.dev devctrl.dev dpsand2.dev\
 filter.dev level1.dev pattern.dev psl2lib.dev psl2read.dev sepr.dev\
 type42.dev xfilter.dev
	$(SETMOD) level2 -include cidfont cie cmapread compfont
	$(ADDMOD) level2 -include dct devctrl dpsand2 filter
	$(ADDMOD) level2 -include level1 pattern psl2lib psl2read
	$(ADDMOD) level2 -include sepr type42 xfilter
	$(ADDMOD) level2 -emulator PostScript PostScriptLevel2

# Define basic Level 2 language support.
# This is the minimum required for CMap and CIDFont support.

psl2int_=iutil2.$(OBJ) zmisc2.$(OBJ) zusparam.$(OBJ)
psl2int.dev: $(INT_MAK) $(ECHOGS_XE) $(psl2int_) dps2int.dev
	$(SETMOD) psl2int $(psl2int_)
	$(ADDMOD) psl2int -include dps2int
	$(ADDMOD) psl2int -oper zmisc2 zusparam
	$(ADDMOD) psl2int -ps gs_lev2 gs_res

iutil2.$(OBJ): iutil2.c $(GH) $(memory__h) $(string__h)\
 $(gsparam_h) $(gsutil_h)\
 $(errors_h) $(opcheck_h) $(imemory_h) $(iutil_h) $(iutil2_h)

zmisc2.$(OBJ): zmisc2.c $(OP) $(memory__h) $(string__h)\
 $(idict_h) $(idparam_h) $(iparam_h) $(dstack_h) $(estack_h)\
 $(ilevel_h) $(iname_h) $(iutil2_h) $(ivmspace_h) $(store_h)

# Note that zusparam includes both Level 1 and Level 2 operators.
zusparam.$(OBJ): zusparam.c $(OP) $(memory__h) $(string__h)\
 $(gscdefs_h) $(gsfont_h) $(gsstruct_h) $(gsutil_h) $(gxht_h)\
 $(ialloc_h) $(idict_h) $(idparam_h) $(iparam_h) $(dstack_h) $(estack_h)\
 $(iname_h) $(iutil2_h) $(store_h)

# Define full Level 2 support.

psl2read_=zcolor2.$(OBJ) zcsindex.$(OBJ) zht2.$(OBJ) zimage2.$(OBJ)
# Note that zmisc2 includes both Level 1 and Level 2 operators.
psl2read.dev: $(INT_MAK) $(ECHOGS_XE) $(psl2read_) psl2int.dev dps2read.dev
	$(SETMOD) psl2read $(psl2read_)
	$(ADDMOD) psl2read -include psl2int dps2read
	$(ADDMOD) psl2read -oper zcolor2_l2 zcsindex_l2
	$(ADDMOD) psl2read -oper zht2_l2 zimage2_l2

zcolor2.$(OBJ): zcolor2.c $(OP)\
 $(gscolor_h) $(gsmatrix_h) $(gsstruct_h)\
 $(gxcolor2_h) $(gxcspace_h) $(gxdcolor_h) $(gxdevice_h) $(gxdevmem_h) $(gxfixed_h) $(gxpcolor_h)\
 $(estack_h) $(ialloc_h) $(idict_h) $(idparam_h) $(igstate_h) $(istruct_h)\
 $(store_h)

zcsindex.$(OBJ): zcsindex.c $(OP) $(memory__h) \
  $(gscolor_h) $(gsstruct_h) $(gxfixed_h) $(gxcolor2_h) $(gxcspace_h) $(gsmatrix_h) \
  $(ialloc_h) $(icsmap_h) $(estack_h) $(igstate_h) $(ivmspace_h) $(store_h)

zht2.$(OBJ): zht2.c $(OP) \
  $(gsstruct_h) $(gxdevice_h) $(gzht_h) \
  $(estack_h) $(ialloc_h) $(icolor_h) $(idict_h) $(idparam_h) $(igstate_h) \
  $(iht_h) $(store_h)

zimage2.$(OBJ): zimage2.c $(OP) $(math__h) $(memory__h)\
 $(gscolor_h) $(gscolor2_h) $(gscspace_h) $(gsimage_h) $(gsmatrix_h)\
 $(idict_h) $(idparam_h) $(iimage_h) $(ilevel_h) $(igstate_h)

# ---------------- Device control ---------------- #
# This is a catch-all for setpagedevice and IODevices.

devctrl_=zdevice2.$(OBJ) ziodev2.$(OBJ) zmedia2.$(OBJ) zdevcal.$(OBJ)
devctrl.dev: $(INT_MAK) $(ECHOGS_XE) $(devctrl_)
	$(SETMOD) devctrl $(devctrl_)
	$(ADDMOD) devctrl -oper zdevice2_l2 ziodev2_l2 zmedia2_l2
	$(ADDMOD) devctrl -iodev null ram calendar
	$(ADDMOD) devctrl -ps gs_setpd

zdevice2.$(OBJ): zdevice2.c $(OP) $(math__h) $(memory__h)\
 $(dstack_h) $(estack_h) $(idict_h) $(idparam_h) $(igstate_h) $(iname_h) $(store_h)\
 $(gxdevice_h) $(gsstate_h)

ziodev2.$(OBJ): ziodev2.c $(OP) $(string__h) $(gp_h)\
 $(gxiodev_h) $(stream_h) $(files_h) $(iparam_h) $(iutil2_h) $(store_h)

zmedia2.$(OBJ): zmedia2.c $(OP) $(math__h) $(memory__h) \
  $(gsmatrix_h) $(idict_h) $(idparam_h) $(iname_h) $(store_h)

zdevcal.$(OBJ): zdevcal.c $(GH) $(time__h) \
  $(gxiodev_h) $(iparam_h) $(istack_h)

# ---------------- Filters other than the ones in sfilter.c ---------------- #

# Standard Level 2 decoding filters only.  The PDF configuration uses this.
fdecode_=scantab.$(OBJ) sfilter2.$(OBJ) zfdecode.$(OBJ)
fdecode.dev: $(INT_MAK) $(ECHOGS_XE) $(fdecode_) cfd.dev lzwd.dev pdiff.dev pngp.dev rld.dev
	$(SETMOD) fdecode $(fdecode_)
	$(ADDMOD) fdecode -include cfd lzwd pdiff pngp rld
	$(ADDMOD) fdecode -oper zfdecode

zfdecode.$(OBJ): zfdecode.c $(OP) $(memory__h)\
 $(gsstruct_h) $(ialloc_h) $(idict_h) $(idparam_h) $(ifilter_h) \
 $(sa85x_h) $(scf_h) $(scfx_h) $(sfilter_h) $(slzwx_h) $(spdiffx_h) $(spngpx_h) \
 $(store_h) $(stream_h) $(strimpl_h)

# Complete Level 2 filter capability.
filter_=zfilter2.$(OBJ)
filter.dev: $(INT_MAK) $(ECHOGS_XE) fdecode.dev $(filter_) cfe.dev lzwe.dev rle.dev
	$(SETMOD) filter -include fdecode
	$(ADDMOD) filter -obj $(filter_)
	$(ADDMOD) filter -include cfe lzwe rle
	$(ADDMOD) filter -oper zfilter2

zfilter2.$(OBJ): zfilter2.c $(OP) $(memory__h)\
  $(gsstruct_h) $(ialloc_h) $(idict_h) $(idparam_h) $(ifilter_h) $(store_h) \
  $(sfilter_h) $(scfx_h) $(slzwx_h) $(spdiffx_h) $(spngpx_h) $(strimpl_h)

# Extensions beyond Level 2 standard.
xfilter_=sbhc.$(OBJ) sbwbs.$(OBJ) shcgen.$(OBJ) smtf.$(OBJ) \
 zfilterx.$(OBJ)
xfilter.dev: $(INT_MAK) $(ECHOGS_XE) $(xfilter_) pcxd.dev pngp.dev
	$(SETMOD) xfilter $(xfilter_)
	$(ADDMOD) xfilter -include pcxd
	$(ADDMOD) xfilter -oper zfilterx

sbhc.$(OBJ): sbhc.c $(AK) $(memory__h) $(stdio__h)\
 $(gdebug_h) $(sbhc_h) $(shcgen_h) $(strimpl_h)

sbwbs.$(OBJ): sbwbs.c $(AK) $(stdio__h) $(memory__h) \
  $(gdebug_h) $(sbwbs_h) $(sfilter_h) $(strimpl_h)

shcgen.$(OBJ): shcgen.c $(AK) $(memory__h) $(stdio__h)\
 $(gdebug_h) $(gserror_h) $(gserrors_h) $(gsmemory_h)\
 $(scommon_h) $(shc_h) $(shcgen_h)

smtf.$(OBJ): smtf.c $(AK) $(stdio__h) \
  $(smtf_h) $(strimpl_h)

zfilterx.$(OBJ): zfilterx.c $(OP) $(memory__h)\
 $(gsstruct_h) $(ialloc_h) $(idict_h) $(idparam_h) $(ifilter_h)\
 $(store_h) $(sfilter_h) $(sbhc_h) $(sbtx_h) $(sbwbs_h) $(shcgen_h)\
 $(smtf_h) $(spcxx_h) $(strimpl_h)

# ---------------- Binary tokens ---------------- #

btoken_=iscanbin.$(OBJ) zbseq.$(OBJ)
btoken.dev: $(INT_MAK) $(ECHOGS_XE) $(btoken_)
	$(SETMOD) btoken $(btoken_)
	$(ADDMOD) btoken -oper zbseq_l2
	$(ADDMOD) btoken -ps gs_btokn

bseq_h=bseq.h
btoken_h=btoken.h

iscanbin.$(OBJ): iscanbin.c $(GH) $(math__h) $(memory__h) $(errors_h)\
 $(gsutil_h) $(ialloc_h) $(ibnum_h) $(idict_h) $(iname_h)\
 $(iscan_h) $(iutil_h) $(ivmspace_h)\
 $(bseq_h) $(btoken_h) $(dstack_h) $(ostack_h)\
 $(sfilter_h) $(store_h) $(stream_h) $(strimpl_h)

zbseq.$(OBJ): zbseq.c $(OP) $(memory__h)\
 $(ialloc_h) $(idict_h) $(isave_h)\
 $(sfilter_h) $(store_h) $(stream_h) $(strimpl_h)\
 $(iname_h) $(ibnum_h) $(btoken_h) $(bseq_h)

# ---------------- User paths & insideness testing ---------------- #

upath_=zupath.$(OBJ) ibnum.$(OBJ)
upath.dev: $(INT_MAK) $(ECHOGS_XE) $(upath_)
	$(SETMOD) upath $(upath_)
	$(ADDMOD) upath -oper zupath_l2

zupath.$(OBJ): zupath.c $(OP) \
  $(idict_h) $(dstack_h) $(iutil_h) $(igstate_h) $(store_h) $(stream_h) $(ibnum_h) \
  $(gscoord_h) $(gsmatrix_h) $(gspaint_h) $(gspath_h) $(gsstate_h) \
  $(gxfixed_h) $(gxdevice_h) $(gzpath_h) $(gzstate_h)

# -------- Additions common to Display PostScript and Level 2 -------- #

dpsand2.dev: $(INT_MAK) $(ECHOGS_XE) btoken.dev color.dev upath.dev dps2lib.dev dps2read.dev
	$(SETMOD) dpsand2 -include btoken color upath dps2lib dps2read

dps2int_=zvmem2.$(OBJ) zdps1.$(OBJ)
# Note that zvmem2 includes both Level 1 and Level 2 operators.
dps2int.dev: $(INT_MAK) $(ECHOGS_XE) $(dps2int_)
	$(SETMOD) dps2int $(dps2int_)
	$(ADDMOD) dps2int -oper zvmem2 zdps1_l2
	$(ADDMOD) dps2int -ps gs_dps1

dps2read_=ibnum.$(OBJ) zchar2.$(OBJ)
dps2read.dev: $(INT_MAK) $(ECHOGS_XE) $(dps2read_) dps2int.dev
	$(SETMOD) dps2read $(dps2read_)
	$(ADDMOD) dps2read -include dps2int
	$(ADDMOD) dps2read -oper ireclaim_l2 zchar2_l2
	$(ADDMOD) dps2read -ps gs_dps2

ibnum.$(OBJ): ibnum.c $(GH) $(math__h) $(memory__h)\
 $(errors_h) $(stream_h) $(ibnum_h) $(imemory_h) $(iutil_h)

zchar2.$(OBJ): zchar2.c $(OP)\
 $(gschar_h) $(gsmatrix_h) $(gspath_h) $(gsstruct_h)\
 $(gxchar_h) $(gxfixed_h) $(gxfont_h)\
 $(ialloc_h) $(ichar_h) $(estack_h) $(ifont_h) $(iname_h) $(igstate_h)\
 $(store_h) $(stream_h) $(ibnum_h)

zdps1.$(OBJ): zdps1.c $(OP) \
  $(gsmatrix_h) $(gspath_h) $(gspath2_h) $(gsstate_h) \
  $(ialloc_h) $(ivmspace_h) $(igstate_h) $(store_h) $(stream_h) $(ibnum_h)

zvmem2.$(OBJ): zvmem2.c $(OP) \
  $(estack_h) $(ialloc_h) $(ivmspace_h) $(store_h)

# ---------------- Display PostScript ---------------- #

dps_=zdps.$(OBJ) icontext.$(OBJ) zcontext.$(OBJ)
dps.dev: $(INT_MAK) $(ECHOGS_XE) dpslib.dev level2.dev $(dps_)
	$(SETMOD) dps -include dpslib level2
	$(ADDMOD) dps -obj $(dps_)
	$(ADDMOD) dps -oper zcontext zdps
	$(ADDMOD) dps -ps gs_dps

icontext.$(OBJ): icontext.c $(GH)\
 $(gsstruct_h) $(gxalloc_h)\
 $(dstack_h) $(errors_h) $(estack_h) $(ostack_h)\
 $(icontext_h) $(igstate_h) $(interp_h) $(store_h)

zdps.$(OBJ): zdps.c $(OP)\
 $(gsdps_h) $(gsstate_h) $(igstate_h) $(iname_h) $(store_h)

zcontext.$(OBJ): zcontext.c $(OP) $(gp_h) $(memory__h)\
 $(gsexit_h) $(gsstruct_h) $(gsutil_h) $(gxalloc_h)\
 $(icontext_h) $(idict_h) $(igstate_h) $(istruct_h)\
 $(dstack_h) $(estack_h) $(ostack_h) $(store_h)

# The following #ifdef ... #endif are just a comment to mark a DPNEXT area.
#ifdef DPNEXT

# ---------------- NeXT Display PostScript ---------------- #
#**************** NOT READY FOR USE YET ****************#

# There should be a gsdpnext.c, but there isn't yet.
#dpsnext_=zdpnext.$(OBJ) gsdpnext.$(OBJ)
dpsnext_=zdpnext.$(OBJ)
dpsnext.dev: $(INT_MAK) $(ECHOGS_XE) dps.dev $(dpsnext_) gs_dpnxt.ps
	$(SETMOD) dpsnext -include dps
	$(ADDMOD) dpsnext -obj $(dpsnext_)
	$(ADDMOD) dpsnext -oper zdpnext
	$(ADDMOD) dpsnext -ps gs_dpnxt

zdpnext.$(OBJ): zdpnext.c $(OP)\
 $(gscspace_h) $(gsiparam_h) $(gsmatrix_h) $(gxcvalue_h) $(gxsample_h)\
 $(ialloc_h) $(igstate_h) $(iimage_h)

# See above re the following.
#endif				/* DPNEXT */

# -------- Composite (PostScript Type 0) font support -------- #

compfont.dev: $(INT_MAK) $(ECHOGS_XE) psf0lib.dev psf0read.dev
	$(SETMOD) compfont -include psf0lib psf0read

# We always include zfcmap.$(OBJ) because zfont0.c refers to it,
# and it's not worth the trouble to exclude.
psf0read_=zchar2.$(OBJ) zfcmap.$(OBJ) zfont0.$(OBJ)
psf0read.dev: $(INT_MAK) $(ECHOGS_XE) $(psf0read_)
	$(SETMOD) psf0read $(psf0read_)
	$(ADDMOD) psf0read -oper zfont0 zchar2 zfcmap

zfcmap.$(OBJ): zfcmap.c $(OP)\
 $(gsmatrix_h) $(gsstruct_h) $(gsutil_h)\
 $(gxfcmap_h) $(gxfont_h)\
 $(ialloc_h) $(idict_h) $(idparam_h) $(ifont_h) $(iname_h) $(store_h)

zfont0.$(OBJ): zfont0.c $(OP)\
 $(gschar_h) $(gsstruct_h)\
 $(gxdevice_h) $(gxfcmap_h) $(gxfixed_h) $(gxfont_h) $(gxfont0_h) $(gxmatrix_h)\
 $(gzstate_h)\
 $(bfont_h) $(ialloc_h) $(idict_h) $(idparam_h) $(igstate_h) $(iname_h)\
 $(store_h)

# ---------------- CMap support ---------------- #
# Note that this requires at least minimal Level 2 support,
# because it requires findresource.

cmapread_=zfcmap.$(OBJ)
cmapread.dev: $(INT_MAK) $(ECHOGS_XE) $(cmapread_) cmaplib.dev psl2int.dev
	$(SETMOD) cmapread $(cmapread_)
	$(ADDMOD) cmapread -include cmaplib psl2int
	$(ADDMOD) cmapread -oper zfcmap
	$(ADDMOD) cmapread -ps gs_cmap

# ---------------- CIDFont support ---------------- #
# Note that this requires at least minimal Level 2 support,
# because it requires findresource.

cidread_=zcid.$(OBJ)
cidfont.dev: $(INT_MAK) $(ECHOGS_XE) psf1read.dev psl2int.dev type42.dev\
 $(cidread_)
	$(SETMOD) cidfont $(cidread_)
	$(ADDMOD) cidfont -include psf1read psl2int type42
	$(ADDMOD) cidfont -ps gs_cidfn
	$(ADDMOD) cidfont -oper zcid

zcid.$(OBJ): zcid.c $(OP)\
 $(gsccode_h) $(gsmatrix_h) $(gxfont_h)\
 $(bfont_h) $(iname_h) $(store_h)

# ---------------- CIE color ---------------- #

cieread_=zcie.$(OBJ) zcrd.$(OBJ)
cie.dev: $(INT_MAK) $(ECHOGS_XE) $(cieread_) cielib.dev
	$(SETMOD) cie $(cieread_)
	$(ADDMOD) cie -oper zcie_l2 zcrd_l2
	$(ADDMOD) cie -include cielib

icie_h=icie.h

zcie.$(OBJ): zcie.c $(OP) $(math__h) $(memory__h) \
  $(gscolor2_h) $(gscie_h) $(gsstruct_h) $(gxcspace_h) \
  $(ialloc_h) $(icie_h) $(idict_h) $(idparam_h) $(estack_h) \
  $(isave_h) $(igstate_h) $(ivmspace_h) $(store_h)

zcrd.$(OBJ): zcrd.c $(OP) $(math__h) \
  $(gscspace_h) $(gscolor2_h) $(gscie_h) $(gsstruct_h) \
  $(ialloc_h) $(icie_h) $(idict_h) $(idparam_h) $(estack_h) \
  $(isave_h) $(igstate_h) $(ivmspace_h) $(store_h)

# ---------------- Pattern color ---------------- #

pattern.dev: $(INT_MAK) $(ECHOGS_XE) patlib.dev patread.dev
	$(SETMOD) pattern -include patlib patread

patread_=zpcolor.$(OBJ)
patread.dev: $(INT_MAK) $(ECHOGS_XE) $(patread_)
	$(SETMOD) patread $(patread_)
	$(ADDMOD) patread -oper zpcolor_l2

zpcolor.$(OBJ): zpcolor.c $(OP)\
 $(gscolor_h) $(gsmatrix_h) $(gsstruct_h)\
 $(gxcolor2_h) $(gxcspace_h) $(gxdcolor_h) $(gxdevice_h) $(gxdevmem_h)\
   $(gxfixed_h) $(gxpcolor_h)\
 $(estack_h) $(ialloc_h) $(idict_h) $(idparam_h) $(igstate_h) $(istruct_h)\
 $(store_h)

# ---------------- Separation color ---------------- #

seprread_=zcssepr.$(OBJ)
sepr.dev: $(INT_MAK) $(ECHOGS_XE) $(seprread_) seprlib.dev
	$(SETMOD) sepr $(seprread_)
	$(ADDMOD) sepr -oper zcssepr_l2
	$(ADDMOD) sepr -include seprlib

zcssepr.$(OBJ): zcssepr.c $(OP) \
  $(gscolor_h) $(gscsepr_h) $(gsmatrix_h) $(gsstruct_h) \
  $(gxcolor2_h) $(gxcspace_h) $(gxfixed_h) \
  $(ialloc_h) $(icsmap_h) $(estack_h) $(igstate_h) $(ivmspace_h) $(store_h)

# ---------------- Functions ---------------- #

ifunc_h=ifunc.h

# Generic support, and FunctionType 0.
funcread_=zfunc.$(OBJ) zfunc0.$(OBJ)
func.dev: $(INT_MAK) $(ECHOGS_XE) $(funcread_) funclib.dev
	$(SETMOD) func $(funcread_)
	$(ADDMOD) func -oper zfunc zfunc0
	$(ADDMOD) func -include funclib

zfunc.$(OBJ): zfunc.c $(OP) $(memory__h)\
 $(gsfunc_h) $(gsstruct_h)\
 $(ialloc_h) $(idict_h) $(idparam_h) $(ifunc_h) $(store_h)

zfunc0.$(OBJ): zfunc0.c $(OP) $(memory__h)\
 $(gsdsrc_h) $(gsfunc_h) $(gsfunc0_h)\
 $(stream_h)\
 $(files_h) $(ialloc_h) $(idict_h) $(idparam_h) $(ifunc_h)

# ---------------- DCT filters ---------------- #
# The definitions for jpeg*.dev are in jpeg.mak.

dct.dev: $(INT_MAK) $(ECHOGS_XE) dcte.dev dctd.dev
	$(SETMOD) dct -include dcte dctd

# Common code

dctc_=zfdctc.$(OBJ)

zfdctc.$(OBJ): zfdctc.c $(GH) $(memory__h) $(stdio__h)\
 $(errors_h) $(opcheck_h)\
 $(idict_h) $(idparam_h) $(imemory_h) $(ipacked_h) $(iutil_h)\
 $(sdct_h) $(sjpeg_h) $(strimpl_h)\
 jpeglib.h

# Encoding (compression)

dcte_=$(dctc_) zfdcte.$(OBJ)
dcte.dev: $(INT_MAK) $(ECHOGS_XE) sdcte.dev $(dcte_)
	$(SETMOD) dcte -include sdcte
	$(ADDMOD) dcte -obj $(dcte_)
	$(ADDMOD) dcte -oper zfdcte

zfdcte.$(OBJ): zfdcte.c $(OP) $(memory__h) $(stdio__h)\
  $(idict_h) $(idparam_h) $(ifilter_h) $(sdct_h) $(sjpeg_h) $(strimpl_h) \
  jpeglib.h

# Decoding (decompression)

dctd_=$(dctc_) zfdctd.$(OBJ)
dctd.dev: $(INT_MAK) $(ECHOGS_XE) sdctd.dev $(dctd_)
	$(SETMOD) dctd -include sdctd
	$(ADDMOD) dctd -obj $(dctd_)
	$(ADDMOD) dctd -oper zfdctd

zfdctd.$(OBJ): zfdctd.c $(OP) $(memory__h) $(stdio__h)\
 $(ifilter_h) $(sdct_h) $(sjpeg_h) $(strimpl_h) \
 jpeglib.h

# ---------------- zlib/Flate filters ---------------- #

fzlib.dev: $(INT_MAK) $(ECHOGS_XE) zfzlib.$(OBJ) szlibe.dev szlibd.dev
	$(SETMOD) fzlib -include szlibe szlibd
	$(ADDMOD) fzlib -obj zfzlib.$(OBJ)
	$(ADDMOD) fzlib -oper zfzlib

zfzlib.$(OBJ): zfzlib.c $(OP) \
  $(errors_h) $(idict_h) $(ifilter_h) \
  $(spdiffx_h) $(spngpx_h) $(strimpl_h) $(szlibx_h)
	$(CCCZ) zfzlib.c

# ================================ PDF ================================ #

# We need most of the Level 2 interpreter to do PDF, but not all of it.
# In fact, we don't even need all of a Level 1 interpreter.

# Because of the way the PDF encodings are defined, they must get loaded
# before we install the Level 2 resource machinery.
# On the other hand, the PDF .ps files must get loaded after
# level2dict is defined.
pdfmin.dev: $(INT_MAK) $(ECHOGS_XE)\
 psbase.dev color.dev dps2lib.dev dps2read.dev\
 fdecode.dev type1.dev pdffonts.dev psl2lib.dev psl2read.dev pdfread.dev
	$(SETMOD) pdfmin -include psbase color dps2lib dps2read
	$(ADDMOD) pdfmin -include fdecode type1
	$(ADDMOD) pdfmin -include pdffonts psl2lib psl2read pdfread
	$(ADDMOD) pdfmin -emulator PDF

pdf.dev: $(INT_MAK) $(ECHOGS_XE)\
 pdfmin.dev cff.dev cidfont.dev cie.dev compfont.dev cmapread.dev dctd.dev\
 func.dev ttfont.dev type2.dev
	$(SETMOD) pdf -include pdfmin cff cidfont cie cmapread compfont dctd
	$(ADDMOD) pdf -include func ttfont type2

# Reader only

pdffonts.dev: $(INT_MAK) $(ECHOGS_XE) \
  gs_mex_e.ps gs_mro_e.ps gs_pdf_e.ps gs_wan_e.ps
	$(SETMOD) pdffonts -ps gs_mex_e gs_mro_e gs_pdf_e gs_wan_e

# pdf_2ps must be the last .ps file loaded.
pdfread.dev: $(INT_MAK) $(ECHOGS_XE) fzlib.dev
	$(SETMOD) pdfread -include fzlib
	$(ADDMOD) pdfread -ps gs_pdf gs_l2img
	$(ADDMOD) pdfread -ps pdf_base pdf_draw pdf_font pdf_main pdf_sec
	$(ADDMOD) pdfread -ps pdf_2ps

# ============================= Main program ============================== #

gs.$(OBJ): gs.c $(GH) \
  $(imain_h) $(imainarg_h) $(iminst_h)

imainarg.$(OBJ): imainarg.c $(GH) $(ctype__h) $(memory__h) $(string__h) \
  $(gp_h) \
  $(gsargs_h) $(gscdefs_h) $(gsdevice_h) $(gsmdebug_h) $(gxdevice_h) $(gxdevmem_h) \
  $(errors_h) $(estack_h) $(files_h) \
  $(ialloc_h) $(imain_h) $(imainarg_h) $(iminst_h) \
  $(iname_h) $(interp_h) $(iscan_h) $(iutil_h) $(ivmspace_h) \
  $(ostack_h) $(sfilter_h) $(store_h) $(stream_h) $(strimpl_h)

imain.$(OBJ): imain.c $(GH) $(memory__h) $(string__h)\
 $(gp_h) $(gslib_h) $(gsmatrix_h) $(gsutil_h) $(gxdevice_h)\
 $(dstack_h) $(errors_h) $(estack_h) $(files_h)\
 $(ialloc_h) $(idebug_h) $(idict_h) $(iname_h) $(interp_h)\
 $(isave_h) $(iscan_h) $(ivmspace_h)\
 $(main_h) $(oper_h) $(ostack_h)\
 $(sfilter_h) $(store_h) $(stream_h) $(strimpl_h)

interp.$(OBJ): interp.c $(GH) $(memory__h) $(string__h)\
 $(gsstruct_h)\
 $(dstack_h) $(errors_h) $(estack_h) $(files_h)\
 $(ialloc_h) $(iastruct_h) $(inamedef_h) $(idict_h) $(interp_h) $(ipacked_h)\
 $(iscan_h) $(isave_h) $(istack_h) $(iutil_h) $(ivmspace_h)\
 $(oper_h) $(ostack_h) $(sfilter_h) $(store_h) $(stream_h) $(strimpl_h)
	$(CCINT) interp.c

ireclaim.$(OBJ): ireclaim.c $(GH) \
  $(errors_h) $(gsstruct_h) $(iastate_h) $(opdef_h) $(store_h) \
  $(dstack_h) $(estack_h) $(ostack_h)
#    Copyright (C) 1994, 1996, 1997 Aladdin Enterprises.  All rights reserved.
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

# makefile for Independent JPEG Group library code.

# NOTE: This makefile is only known to work with the following versions
# of the IJG library: 6, 6a.
# As of May 11, 1996, version 6a is the current version.
#
# You can get the IJG library by Internet anonymous FTP from the following
# places:
#	Standard distribution (tar + gzip format, Unix end-of-line):
#		ftp.uu.net:/graphics/jpeg/jpegsrc.v*.tar.gz
#		ftp.cs.wisc.edu:/ghost/jpegsrc.v*.tar.gz
#	MS-DOS archive (PKZIP a.k.a. zip format, MS-DOS end-of-line):
#		ftp.simtel.net:/pub/simtelnet/msdos/graphics/jpegsr*.zip
#		ftp.cs.wisc.edu:/ghost/jpeg-*.zip
# The first site named above (ftp.uu.net and ftp.simtel.net) is supposed
# to be the master distribution site, so it may have a more up-to-date
# version; the ftp.cs.wisc.edu site is the master distribution site for
# Ghostscript, so it will always have IJG library versions known to be
# compatible with Ghostscript.
#
# If the version number, and hence the subdirectory name, changes, you
# will probably want to change the definitions of JSRCDIR and possibly
# JVERSION (in the platform-specific makefile, not here) to reflect this,
# since that way you can use the IJG archive without change.
#
# NOTE: For some obscure reason (probably a bug in djtarx), if you are
# compiling on a DesqView/X system, you should use the zip version of the
# IJG library, not the tar.gz version.

# Define the name of this makefile.
JPEG_MAK=jpeg.mak

# JSRCDIR is defined in the platform-specific makefile, not here,
# as the directory where the IJG library sources are stored.
#JSRCDIR=jpeg-6a
# JVERSION is defined in the platform-specific makefile, not here,
# as the IJG library major version number (currently "5" or "6").
#JVERSION=6

JSRC=$(JSRCDIR)$(D)
# CCCJ is defined in gs.mak.
#CCCJ=$(CCC) -I. -I$(JSRCDIR)

# We keep all of the IJG code in a separate directory so as not to
# inadvertently mix it up with Aladdin Enterprises' own code.
# However, we need our own version of jconfig.h, and our own "wrapper" for
# jmorecfg.h.  We also need a substitute for jerror.c, in order to
# keep the error strings out of the automatic data segment in
# 16-bit environments.  For v5*, we also need our own version of jpeglib.h
# in order to change MAX_BLOCKS_IN_MCU for Adobe compatibility.
# (This need will go away when IJG v6 is released.)

# Because this file is included after lib.mak, we can't use _h macros
# to express indirect dependencies; instead, we build the dependencies
# into the rules for copying the files.
jconfig_h=jconfig.h
jerror_h=jerror.h
jmorecfg_h=jmorecfg.h
jpeglib_h=jpeglib.h

jconfig.h: gsjconf.h $(std_h)
	$(CP_) gsjconf.h jconfig.h

jmorecfg.h: gsjmorec.h jmcorig.h
	$(CP_) gsjmorec.h jmorecfg.h

jmcorig.h: $(JSRC)jmorecfg.h
	$(CP_) $(JSRC)jmorecfg.h jmcorig.h

jpeglib.h: jlib$(JVERSION).h jconfig.h jmorecfg.h
	$(CP_) jlib$(JVERSION).h jpeglib.h

jlib5.h: gsjpglib.h
	$(CP_) gsjpglib.h jlib5.h

jlib6.h: $(JSRC)jpeglib.h
	$(CP_) $(JSRC)jpeglib.h jlib6.h

# To ensure that the compiler finds our versions of jconfig.h and jmorecfg.h,
# regardless of the compiler's search rule, we must copy up all .c files,
# and all .h files that include either of these files, directly or
# indirectly.  The only such .h files currently are jinclude.h and jpeglib.h.
# (Currently, we supply our own version of jpeglib.h -- see above.)
# Also, to avoid including the JSRCDIR directory name in our source files,
# we must also copy up any other .h files that our own code references.
# Currently, the only such .h files are jerror.h and jversion.h.

JHCOPY=jinclude.h jpeglib.h jerror.h jversion.h

jinclude.h: $(JSRC)jinclude.h
	$(CP_) $(JSRC)jinclude.h jinclude.h

#jpeglib.h: $(JSRC)jpeglib.h
#	$(CP_) $(JSRC)jpeglib.h jpeglib.h

jerror.h: $(JSRC)jerror.h
	$(CP_) $(JSRC)jerror.h jerror.h

jversion.h: $(JSRC)jversion.h
	$(CP_) $(JSRC)jversion.h jversion.h

# In order to avoid having to keep the dependency lists for the IJG code
# accurate, we simply make all of them depend on the only files that
# we are ever going to change, and on all the .h files that must be copied up.
# This is too conservative, but only hurts us if we are changing our own
# j*.h files, which happens only rarely during development.

JDEP=$(AK) $(jconfig_h) $(jerror_h) $(jmorecfg_h) $(JHCOPY)

# Code common to compression and decompression.

jpegc_=jcomapi.$(OBJ) jutils.$(OBJ) sjpegerr.$(OBJ) jmemmgr.$(OBJ)
jpegc.dev: $(JPEG_MAK) $(ECHOGS_XE) $(jpegc_)
	$(SETMOD) jpegc $(jpegc_)

jcomapi.$(OBJ): $(JSRC)jcomapi.c $(JDEP)
	$(CP_) $(JSRC)jcomapi.c .
	$(CCCJ) jcomapi.c
	$(RM_) jcomapi.c

jutils.$(OBJ): $(JSRC)jutils.c $(JDEP)
	$(CP_) $(JSRC)jutils.c .
	$(CCCJ) jutils.c
	$(RM_) jutils.c

# Note that sjpegerr replaces jerror.
sjpegerr.$(OBJ): sjpegerr.c $(JDEP)
	$(CCCF) sjpegerr.c

jmemmgr.$(OBJ): $(JSRC)jmemmgr.c $(JDEP)
	$(CP_) $(JSRC)jmemmgr.c .
	$(CCCJ) jmemmgr.c
	$(RM_) jmemmgr.c

# Encoding (compression) code.

jpege.dev: jpege$(JVERSION).dev
	$(CP_) jpege$(JVERSION).dev jpege.dev

jpege5=jcapi.$(OBJ)
jpege6=jcapimin.$(OBJ) jcapistd.$(OBJ) jcinit.$(OBJ)

jpege_1=jccoefct.$(OBJ) jccolor.$(OBJ) jcdctmgr.$(OBJ) 
jpege_2=jchuff.$(OBJ) jcmainct.$(OBJ) jcmarker.$(OBJ) jcmaster.$(OBJ)
jpege_3=jcparam.$(OBJ) jcprepct.$(OBJ) jcsample.$(OBJ) jfdctint.$(OBJ)

jpege5.dev: $(JPEG_MAK) $(ECHOGS_XE) jpegc.dev $(jpege5) $(jpege_1) $(jpege_2) $(jpege_3)
	$(SETMOD) jpege5 $(jpege5)
	$(ADDMOD) jpege5 -include jpegc
	$(ADDMOD) jpege5 -obj $(jpege_1)
	$(ADDMOD) jpege5 -obj $(jpege_2)
	$(ADDMOD) jpege5 -obj $(jpege_3)

jpege6.dev: $(JPEG_MAK) $(ECHOGS_XE) jpegc.dev $(jpege6) $(jpege_1) $(jpege_2) $(jpege_3)
	$(SETMOD) jpege6 $(jpege6)
	$(ADDMOD) jpege6 -include jpegc
	$(ADDMOD) jpege6 -obj $(jpege_1)
	$(ADDMOD) jpege6 -obj $(jpege_2)
	$(ADDMOD) jpege6 -obj $(jpege_3)

# jcapi.c is v5* only
jcapi.$(OBJ): $(JSRC)jcapi.c $(JDEP)
	$(CP_) $(JSRC)jcapi.c .
	$(CCCJ) jcapi.c
	$(RM_) jcapi.c
  
# jcapimin.c is new in v6
jcapimin.$(OBJ): $(JSRC)jcapimin.c $(JDEP)
	$(CP_) $(JSRC)jcapimin.c .
	$(CCCJ) jcapimin.c
	$(RM_) jcapimin.c

# jcapistd.c is new in v6
jcapistd.$(OBJ): $(JSRC)jcapistd.c $(JDEP)
	$(CP_) $(JSRC)jcapistd.c .
	$(CCCJ) jcapistd.c
	$(RM_) jcapistd.c

# jcinit.c is new in v6
jcinit.$(OBJ): $(JSRC)jcinit.c $(JDEP)
	$(CP_) $(JSRC)jcinit.c .
	$(CCCJ) jcinit.c
	$(RM_) jcinit.c

jccoefct.$(OBJ): $(JSRC)jccoefct.c $(JDEP)
	$(CP_) $(JSRC)jccoefct.c .
	$(CCCJ) jccoefct.c
	$(RM_) jccoefct.c

jccolor.$(OBJ): $(JSRC)jccolor.c $(JDEP)
	$(CP_) $(JSRC)jccolor.c .
	$(CCCJ) jccolor.c
	$(RM_) jccolor.c

jcdctmgr.$(OBJ): $(JSRC)jcdctmgr.c $(JDEP)
	$(CP_) $(JSRC)jcdctmgr.c .
	$(CCCJ) jcdctmgr.c
	$(RM_) jcdctmgr.c

jchuff.$(OBJ): $(JSRC)jchuff.c $(JDEP)
	$(CP_) $(JSRC)jchuff.c .
	$(CCCJ) jchuff.c
	$(RM_) jchuff.c

jcmainct.$(OBJ): $(JSRC)jcmainct.c $(JDEP)
	$(CP_) $(JSRC)jcmainct.c .
	$(CCCJ) jcmainct.c
	$(RM_) jcmainct.c

jcmarker.$(OBJ): $(JSRC)jcmarker.c $(JDEP)
	$(CP_) $(JSRC)jcmarker.c .
	$(CCCJ) jcmarker.c
	$(RM_) jcmarker.c

jcmaster.$(OBJ): $(JSRC)jcmaster.c $(JDEP)
	$(CP_) $(JSRC)jcmaster.c .
	$(CCCJ) jcmaster.c
	$(RM_) jcmaster.c

jcparam.$(OBJ): $(JSRC)jcparam.c $(JDEP)
	$(CP_) $(JSRC)jcparam.c .
	$(CCCJ) jcparam.c
	$(RM_) jcparam.c

jcprepct.$(OBJ): $(JSRC)jcprepct.c $(JDEP)
	$(CP_) $(JSRC)jcprepct.c .
	$(CCCJ) jcprepct.c
	$(RM_) jcprepct.c

jcsample.$(OBJ): $(JSRC)jcsample.c $(JDEP)
	$(CP_) $(JSRC)jcsample.c .
	$(CCCJ) jcsample.c
	$(RM_) jcsample.c

jfdctint.$(OBJ): $(JSRC)jfdctint.c $(JDEP)
	$(CP_) $(JSRC)jfdctint.c .
	$(CCCJ) jfdctint.c
	$(RM_) jfdctint.c

# Decompression code

jpegd.dev: jpegd$(JVERSION).dev
	$(CP_) jpegd$(JVERSION).dev jpegd.dev

jpegd5=jdapi.$(OBJ)
jpegd6=jdapimin.$(OBJ) jdapistd.$(OBJ) jdinput.$(OBJ) jdphuff.$(OBJ)

jpegd_1=jdcoefct.$(OBJ) jdcolor.$(OBJ)
jpegd_2=jddctmgr.$(OBJ) jdhuff.$(OBJ) jdmainct.$(OBJ) jdmarker.$(OBJ)
jpegd_3=jdmaster.$(OBJ) jdpostct.$(OBJ) jdsample.$(OBJ) jidctint.$(OBJ)

jpegd5.dev: $(JPEG_MAK) $(ECHOGS_XE) jpegc.dev $(jpegd5) $(jpegd_1) $(jpegd_2) $(jpegd_3)
	$(SETMOD) jpegd5 $(jpegd5)
	$(ADDMOD) jpegd5 -include jpegc
	$(ADDMOD) jpegd5 -obj $(jpegd_1)
	$(ADDMOD) jpegd5 -obj $(jpegd_2)
	$(ADDMOD) jpegd5 -obj $(jpegd_3)

jpegd6.dev: $(JPEG_MAK) $(ECHOGS_XE) jpegc.dev $(jpegd6) $(jpegd_1) $(jpegd_2) $(jpegd_3)
	$(SETMOD) jpegd6 $(jpegd6)
	$(ADDMOD) jpegd6 -include jpegc
	$(ADDMOD) jpegd6 -obj $(jpegd_1)
	$(ADDMOD) jpegd6 -obj $(jpegd_2)
	$(ADDMOD) jpegd6 -obj $(jpegd_3)

# jdapi.c is v5* only
jdapi.$(OBJ): $(JSRC)jdapi.c $(JDEP)
	$(CP_) $(JSRC)jdapi.c .
	$(CCCJ) jdapi.c
	$(RM_) jdapi.c

# jdapimin.c is new in v6
jdapimin.$(OBJ): $(JSRC)jdapimin.c $(JDEP)
	$(CP_) $(JSRC)jdapimin.c .
	$(CCCJ) jdapimin.c
	$(RM_) jdapimin.c

# jdapistd.c is new in v6
jdapistd.$(OBJ): $(JSRC)jdapistd.c $(JDEP)
	$(CP_) $(JSRC)jdapistd.c .
	$(CCCJ) jdapistd.c
	$(RM_) jdapistd.c

jdcoefct.$(OBJ): $(JSRC)jdcoefct.c $(JDEP)
	$(CP_) $(JSRC)jdcoefct.c .
	$(CCCJ) jdcoefct.c
	$(RM_) jdcoefct.c

jdcolor.$(OBJ): $(JSRC)jdcolor.c $(JDEP)
	$(CP_) $(JSRC)jdcolor.c .
	$(CCCJ) jdcolor.c
	$(RM_) jdcolor.c

jddctmgr.$(OBJ): $(JSRC)jddctmgr.c $(JDEP)
	$(CP_) $(JSRC)jddctmgr.c .
	$(CCCJ) jddctmgr.c
	$(RM_) jddctmgr.c

jdhuff.$(OBJ): $(JSRC)jdhuff.c $(JDEP)
	$(CP_) $(JSRC)jdhuff.c .
	$(CCCJ) jdhuff.c
	$(RM_) jdhuff.c

# jdinput.c is new in v6
jdinput.$(OBJ): $(JSRC)jdinput.c $(JDEP)
	$(CP_) $(JSRC)jdinput.c .
	$(CCCJ) jdinput.c
	$(RM_) jdinput.c

jdmainct.$(OBJ): $(JSRC)jdmainct.c $(JDEP)
	$(CP_) $(JSRC)jdmainct.c .
	$(CCCJ) jdmainct.c
	$(RM_) jdmainct.c

jdmarker.$(OBJ): $(JSRC)jdmarker.c $(JDEP)
	$(CP_) $(JSRC)jdmarker.c .
	$(CCCJ) jdmarker.c
	$(RM_) jdmarker.c

jdmaster.$(OBJ): $(JSRC)jdmaster.c $(JDEP)
	$(CP_) $(JSRC)jdmaster.c .
	$(CCCJ) jdmaster.c
	$(RM_) jdmaster.c

# jdphuff.c is new in v6
jdphuff.$(OBJ): $(JSRC)jdphuff.c $(JDEP)
	$(CP_) $(JSRC)jdphuff.c .
	$(CCCJ) jdphuff.c
	$(RM_) jdphuff.c

jdpostct.$(OBJ): $(JSRC)jdpostct.c $(JDEP)
	$(CP_) $(JSRC)jdpostct.c .
	$(CCCJ) jdpostct.c
	$(RM_) jdpostct.c

jdsample.$(OBJ): $(JSRC)jdsample.c $(JDEP)
	$(CP_) $(JSRC)jdsample.c .
	$(CCCJ) jdsample.c
	$(RM_) jdsample.c

jidctint.$(OBJ): $(JSRC)jidctint.c $(JDEP)
	$(CP_) $(JSRC)jidctint.c .
	$(CCCJ) jidctint.c
	$(RM_) jidctint.c
#    Copyright (C) 1995, 1996, 1997 Aladdin Enterprises.  All rights reserved.
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

# makefile for PNG (Portable Network Graphics) code.

# This partial makefile compiles the png library for use in the Ghostscript
# PNG drivers.  You can get the source code for this library from:
#   ftp://swrinde.nde.swri.edu/pub/png/src/
# The makefile is known to work with the following library versions:
# 0.89, 0.90, 0.95, and 0.96.  NOTE: the archive for libpng 0.95 may
# be inconsistent: if you have compilation problems, use an older version.
# Please see Ghostscript's `make.txt' file for instructions about how to
# unpack these archives.
#
# The specification for the PNG file format is available from:
#   http://www.group42.com/png.htm
#   http://www.w3.org/pub/WWW/TR/WD-png

# Define the name of this makefile.
LIBPNG_MAK=libpng.mak

# PSRCDIR is defined in the platform-specific makefile, not here,
# as the directory where the PNG library sources are stored.
#PSRCDIR=libpng
# PVERSION is defined in the platform-specific makefile, not here,
# as the libpng version number ("89", "90", "95", or "96").
#PVERSION=96

PSRC=$(PSRCDIR)$(D)
# CCCP is defined in gs.mak.
#CCCP=$(CCC) -I$(PSRCDIR) -I$(ZSRCDIR)

# We keep all of the PNG code in a separate directory so as not to
# inadvertently mix it up with Aladdin Enterprises' own code.
PDEP=$(AK)

png_1=png.$(OBJ) pngmem.$(OBJ) pngerror.$(OBJ)
png_2=pngtrans.$(OBJ) pngwrite.$(OBJ) pngwtran.$(OBJ) pngwutil.$(OBJ)

# libpng modules

png.$(OBJ): $(PSRC)png.c $(PDEP)
	$(CCCP) $(PSRC)png.c

# version 0.89 uses pngwio.c
pngwio.$(OBJ): $(PSRC)pngwio.c $(PDEP)
	$(CCCP) $(PSRC)pngwio.c

pngmem.$(OBJ): $(PSRC)pngmem.c $(PDEP)
	$(CCCP) $(PSRC)pngmem.c

pngerror.$(OBJ): $(PSRC)pngerror.c $(PDEP)
	$(CCCP) $(PSRC)pngerror.c

pngtrans.$(OBJ): $(PSRC)pngtrans.c $(PDEP)
	$(CCCP) $(PSRC)pngtrans.c

pngwrite.$(OBJ): $(PSRC)pngwrite.c $(PDEP)
	$(CCCP) $(PSRC)pngwrite.c

pngwtran.$(OBJ): $(PSRC)pngwtran.c $(PDEP)
	$(CCCP) $(PSRC)pngwtran.c

pngwutil.$(OBJ): $(PSRC)pngwutil.c $(PDEP)
	$(CCCP) $(PSRC)pngwutil.c

# Define the version of libpng.dev that we are actually using.
libpng.dev: $(MAKEFILE) libpng_$(SHARE_LIBPNG).dev
	$(CP_) libpng_$(SHARE_LIBPNG).dev libpng.dev

# Define the shared version of libpng.
# Note that it requires libz, which must be searched *after* libpng.
libpng_1.dev: $(MAKEFILE) $(LIBPNG_MAK) $(ECHOGS_XE) zlibe.dev
	$(SETMOD) libpng_1 -lib $(LIBPNG_NAME)
	$(ADDMOD) libpng_1 -include zlibe

# Define the non-shared version of libpng.
libpng_0.dev: $(LIBPNG_MAK) $(ECHOGS_XE) $(png_1) $(png_2)\
 zlibe.dev libpng$(PVERSION).dev
	$(SETMOD) libpng_0 $(png_1)
	$(ADDMOD) libpng_0 $(png_2)
	$(ADDMOD) libpng_0 -include zlibe libpng$(PVERSION)

libpng89.dev: $(LIBPNG_MAK) $(ECHOGS_XE) pngwio.$(OBJ)
	$(SETMOD) libpng89 pngwio.$(OBJ)

libpng90.dev: $(LIBPNG_MAK) $(ECHOGS_XE) pngwio.$(OBJ) crc32.dev
	$(SETMOD) libpng90 pngwio.$(OBJ) -include crc32

libpng95.dev: $(LIBPNG_MAK) $(ECHOGS_XE) pngwio.$(OBJ) crc32.dev
	$(SETMOD) libpng95 pngwio.$(OBJ) -include crc32

libpng96.dev: $(LIBPNG_MAK) $(ECHOGS_XE) pngwio.$(OBJ) crc32.dev
	$(SETMOD) libpng96 pngwio.$(OBJ) -include crc32
#    Copyright (C) 1995, 1996, 1997 Aladdin Enterprises.  All rights reserved.
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

# makefile for zlib library code.

# This partial makefile compiles the zlib library for use in Ghostscript.
# You can get the source code for this library from:
#   ftp://ftp.uu.net/pub/archiving/zip/zlib/zlib104.zip   (zlib 1.0.4)
#		or zlib-1.0.4.tar.gz
# Please see Ghostscript's `make.txt' file for instructions about how to
# unpack these archives.

# Define the name of this makefile.
ZLIB_MAK=zlib.mak

# ZSRCDIR is defined in the platform-specific makefile, not here,
# as the directory where the zlib sources are stored.
#ZSRCDIR=zlib
ZSRC=$(ZSRCDIR)$(D)
# We would like to define
#CCCZ=$(CCC) -I$(ZSRCDIR) -Dverbose=-1
# but the Watcom C compiler has strange undocumented restrictions on what can
# follow a -D=, and it doesn't allow negative numbers.  Instead, we define
# (in gs.mak):
#CCCZ=$(CCC) -I. -I$(ZSRCDIR)
# and handle the definition of verbose in a different, more awkward way.

# We keep all of the zlib code in a separate directory so as not to
# inadvertently mix it up with Aladdin Enterprises' own code.
ZDEP=$(AK)

# Contrary to what some portability bigots assert as fact, C compilers are
# not consistent about where they start searching for #included files:
# some always start by looking in the same directory as the .c file being
# compiled, before using the search path specified with -I on the command
# line, while others do not do this.  For this reason, we must explicitly
# copy and then delete all the .c files, because they need to obtain our
# modified version of zutil.h.  We must also copy all header files that
# reference zutil.h directly or indirectly.

# Code common to compression and decompression.

zlibc_=zutil.$(OBJ)
zlibc.dev: $(ZLIB_MAK) $(ECHOGS_XE) $(zlibc_)
	$(SETMOD) zlibc $(zlibc_)

zutil.h: $(ZSRC)zutil.h $(ECHOGS_XE)
	$(EXP)echogs -w zutil.h -x 23 define verbose -s - -1
	$(EXP)echogs -a zutil.h -+R $(ZSRC)zutil.h

zutil.$(OBJ): $(ZSRC)zutil.c $(ZDEP) zutil.h
	$(CP_) $(ZSRC)zutil.c .
	$(CCCZ) zutil.c
	$(RM_) zutil.c

# Encoding (compression) code.

deflate.h: $(ZSRC)deflate.h zutil.h
	$(CP_) $(ZSRC)deflate.h .

zlibe.dev: $(MAKEFILE) zlibe_$(SHARE_ZLIB).dev
	$(CP_) zlibe_$(SHARE_ZLIB).dev zlibe.dev

zlibe_1.dev: $(MAKEFILE) $(ZLIB_MAK) $(ECHOGS_XE)
	$(SETMOD) zlibe_1 -lib $(ZLIB_NAME)

zlibe_=adler32.$(OBJ) deflate.$(OBJ) trees.$(OBJ)
zlibe_0.dev: $(ZLIB_MAK) $(ECHOGS_XE) zlibc.dev $(zlibe_)
	$(SETMOD) zlibe_0 $(zlibe_)
	$(ADDMOD) zlibe_0 -include zlibc

adler32.$(OBJ): $(ZSRC)adler32.c $(ZDEP)
	$(CP_) $(ZSRC)adler32.c .
	$(CCCZ) adler32.c
	$(RM_) adler32.c

deflate.$(OBJ): $(ZSRC)deflate.c $(ZDEP) deflate.h
	$(CP_) $(ZSRC)deflate.c .
	$(CCCZ) deflate.c
	$(RM_) deflate.c

trees.$(OBJ): $(ZSRC)trees.c $(ZDEP) deflate.h
	$(CP_) $(ZSRC)trees.c .
	$(CCCZ) trees.c
	$(RM_) trees.c

# The zlib filters per se don't need crc32, but libpng versions starting
# with 0.90 do.

crc32.dev: $(MAKEFILE) crc32_$(SHARE_ZLIB).dev
	$(CP_) crc32_$(SHARE_ZLIB).dev crc32.dev

crc32_1.dev: $(MAKEFILE) $(ZLIB_MAK) $(ECHOGS_XE)
	$(SETMOD) crc32_1 -lib $(ZLIB_NAME)

crc32_0.dev: $(ZLIB_MAK) $(ECHOGS_XE) crc32.$(OBJ)
	$(SETMOD) crc32_0 crc32.$(OBJ)

crc32.$(OBJ): $(ZSRC)crc32.c $(ZDEP) deflate.h
	$(CP_) $(ZSRC)crc32.c .
	$(CCCZ) crc32.c
	$(RM_) crc32.c

# Decoding (decompression) code.

zlibd.dev: $(MAKEFILE) zlibd_$(SHARE_ZLIB).dev
	$(CP_) zlibd_$(SHARE_ZLIB).dev zlibd.dev

zlibd_1.dev: $(MAKEFILE) $(ZLIB_MAK) $(ECHOGS_XE)
	$(SETMOD) zlibd_1 -lib $(ZLIB_NAME)

zlibd1_=infblock.$(OBJ) infcodes.$(OBJ) inffast.$(OBJ)
zlibd2_=inflate.$(OBJ) inftrees.$(OBJ) infutil.$(OBJ)
zlibd_ = $(zlibd1_) $(zlibd2_)
zlibd_0.dev: $(ZLIB_MAK) $(ECHOGS_XE) zlibc.dev $(zlibd_)
	$(SETMOD) zlibd_0 $(zlibd1_)
	$(ADDMOD) zlibd_0 -obj $(zlibd2_)
	$(ADDMOD) zlibd_0 -include zlibc

infblock.$(OBJ): $(ZSRC)infblock.c $(ZDEP) zutil.h
	$(CP_) $(ZSRC)infblock.c .
	$(CCCZ) infblock.c
	$(RM_) infblock.c

infcodes.$(OBJ): $(ZSRC)infcodes.c $(ZDEP) zutil.h
	$(CP_) $(ZSRC)infcodes.c .
	$(CCCZ) infcodes.c
	$(RM_) infcodes.c

inffast.$(OBJ): $(ZSRC)inffast.c $(ZDEP) zutil.h
	$(CP_) $(ZSRC)inffast.c .
	$(CCCZ) inffast.c
	$(RM_) inffast.c

inflate.$(OBJ): $(ZSRC)inflate.c $(ZDEP) zutil.h
	$(CP_) $(ZSRC)inflate.c .
	$(CCCZ) inflate.c
	$(RM_) inflate.c

inftrees.$(OBJ): $(ZSRC)inftrees.c $(ZDEP) zutil.h
	$(CP_) $(ZSRC)inftrees.c .
	$(CCCZ) inftrees.c
	$(RM_) inftrees.c

infutil.$(OBJ): $(ZSRC)infutil.c $(ZDEP) zutil.h
	$(CP_) $(ZSRC)infutil.c .
	$(CCCZ) infutil.c
	$(RM_) infutil.c
#    Copyright (C) 1989, 1996, 1997 Aladdin Enterprises.  All rights reserved.
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

# makefile for device drivers.

# Define the name of this makefile.
DEVS_MAK=devs.mak

###### --------------------------- Catalog -------------------------- ######

# It is possible to build configurations with an arbitrary collection of
# device drivers, although some drivers are supported only on a subset
# of the target platforms.  The currently available drivers are:

# MS-DOS displays (note: not usable with Desqview/X):
#   MS-DOS EGA and VGA:
#	ega	EGA (640x350, 16-color)
#	vga	VGA (640x480, 16-color)
#   MS-DOS SuperVGA:
# *	ali	SuperVGA using Avance Logic Inc. chipset, 256-color modes
# *	atiw	ATI Wonder SuperVGA, 256-color modes
# *	s3vga	SuperVGA using S3 86C911 chip (e.g., Diamond Stealth board)
#	svga16	Generic SuperVGA in 800x600, 16-color mode
# *	tseng	SuperVGA using Tseng Labs ET3000/4000 chips, 256-color modes
# *	tvga	SuperVGA using Trident chipset, 256-color modes
#   ****** NOTE: The vesa device does not work with the Watcom (32-bit MS-DOS)
#   ****** compiler or executable.
#	vesa	SuperVGA with VESA standard API driver
#   MS-DOS other:
#	bgi	Borland Graphics Interface (CGA)  [MS-DOS only]
# *	herc	Hercules Graphics display   [MS-DOS only]
# *	pe	Private Eye display
# Other displays:
#   MS Windows:
#	mswindll  Microsoft Windows 3.1 DLL  [MS Windows only]
#	mswinprn  Microsoft Windows 3.0, 3.1 DDB printer  [MS Windows only]
#	mswinpr2  Microsoft Windows 3.0, 3.1 DIB printer  [MS Windows only]
#   OS/2:
# *	os2pm	OS/2 Presentation Manager   [OS/2 only]
# *	os2dll	OS/2 DLL bitmap             [OS/2 only]
# *	os2prn	OS/2 printer                [OS/2 only]
#   Unix and VMS:
#   ****** NOTE: For direct frame buffer addressing under SCO Unix or Xenix,
#   ****** edit the definition of EGAVGA below.
# *	att3b1	AT&T 3b1/Unixpc monochrome display   [3b1 only]
# *	lvga256  Linux vgalib, 256-color VGA modes  [Linux only]
# *	sonyfb	Sony Microsystems monochrome display   [Sony only]
# *	sunview  SunView window system   [SunOS only]
# +	vgalib	Linux PC with VGALIB   [Linux only]
#	x11	X Windows version 11, release >=4   [Unix and VMS only]
#	x11alpha  X Windows masquerading as a device with alpha capability
#	x11cmyk  X Windows masquerading as a 1-bit-per-plane CMYK device
#	x11gray2  X Windows as a 2-bit gray-scale device
#	x11mono  X Windows masquerading as a black-and-white device
#   Platform-independent:
# *	sxlcrt	CRT sixels, e.g. for VT240-like terminals
# Printers:
# *	ap3250	Epson AP3250 printer
# *	appledmp  Apple Dot Matrix Printer (should also work with Imagewriter)
#	bj10e	Canon BubbleJet BJ10e
# *	bj200	Canon BubbleJet BJ200
# *	bjc600   Canon Color BubbleJet BJC-600, BJC-4000 and BJC-70
#               also good for Apple printers like the StyleWriter 2x00
# *	bjc800   Canon Color BubbleJet BJC-800
# *     ccr     CalComp Raster format
# *	cdeskjet  H-P DeskJet 500C with 1 bit/pixel color
# *	cdjcolor  H-P DeskJet 500C with 24 bit/pixel color and
#		high-quality color (Floyd-Steinberg) dithering;
#		also good for DeskJet 540C and Citizen Projet IIc (-r200x300)
# *	cdjmono  H-P DeskJet 500C printing black only;
#		also good for DeskJet 510, 520, and 540C (black only)
# *	cdj500	H-P DeskJet 500C (same as cdjcolor)
# *	cdj550	H-P DeskJet 550C/560C/660C/660Cse
# *	cp50	Mitsubishi CP50 color printer
# *	declj250  alternate DEC LJ250 driver
# +	deskjet  H-P DeskJet and DeskJet Plus
#	djet500  H-P DeskJet 500; use -r600 for DJ 600 series
# *	djet500c  H-P DeskJet 500C alternate driver
#		(does not work on 550C or 560C)
# *	dnj650c  H-P DesignJet 650C
#	epson	Epson-compatible dot matrix printers (9- or 24-pin)
# *	eps9mid  Epson-compatible 9-pin, interleaved lines
#		(intermediate resolution)
# *	eps9high  Epson-compatible 9-pin, interleaved lines
#		(triple resolution)
# *	epsonc	Epson LQ-2550 and Fujitsu 3400/2400/1200 color printers
# *	ibmpro  IBM 9-pin Proprinter
# *	imagen	Imagen ImPress printers
# *	iwhi	Apple Imagewriter in high-resolution mode
# *	iwlo	Apple Imagewriter in low-resolution mode
# *	iwlq	Apple Imagewriter LQ in 320 x 216 dpi mode
# *	jetp3852  IBM Jetprinter ink-jet color printer (Model #3852)
# +	laserjet  H-P LaserJet
# *	la50	DEC LA50 printer
# *	la70	DEC LA70 printer
# *	la70t	DEC LA70 printer with low-resolution text enhancement
# *	la75	DEC LA75 printer
# *	la75plus DEC LA75plus printer
# *	lbp8	Canon LBP-8II laser printer
# *	lips3	Canon LIPS III laser printer in English (CaPSL) mode
# *	ln03	DEC LN03 printer
# *	lj250	DEC LJ250 Companion color printer
# +	ljet2p	H-P LaserJet IId/IIp/III* with TIFF compression
# +	ljet3	H-P LaserJet III* with Delta Row compression
# +	ljet3d	H-P LaserJet IIID with duplex capability
# +	ljet4	H-P LaserJet 4 (defaults to 600 dpi)
# +	lj4dith  H-P LaserJet 4 with Floyd-Steinberg dithering
# +	ljetplus  H-P LaserJet Plus
#	lj5mono  H-P LaserJet 5 & 6 family (PCL XL), bitmap:
#		see below for restrictions & advice
#	lj5gray  H-P LaserJet 5 & 6 family, gray-scale bitmap;
#		see below for restrictions & advice
# *	lp2563	H-P 2563B line printer
# *	lp8000	Epson LP-8000 laser printer
# *     lq850   Epson LQ850 printer at 360 x 360 DPI resolution;
#               also good for Canon BJ300 with LQ850 emulation
# *	m8510	C.Itoh M8510 printer
# *	necp6	NEC P6/P6+/P60 printers at 360 x 360 DPI resolution
# *	nwp533  Sony Microsystems NWP533 laser printer   [Sony only]
# *	oce9050  OCE 9050 printer
# *	oki182	Okidata MicroLine 182
# *	okiibm	Okidata MicroLine IBM-compatible printers
# *	paintjet  alternate H-P PaintJet color printer
# *	pj	H-P PaintJet XL driver 
# *	pjetxl	alternate H-P PaintJet XL driver
# *	pjxl	H-P PaintJet XL color printer
# *	pjxl300  H-P PaintJet XL300 color printer;
#		also good for PaintJet 1200C
#	(pxlmono) H-P black-and-white PCL XL printers (LaserJet 5 and 6 family)
#	(pxlcolor) H-P color PCL XL printers (none available yet)
# *	r4081	Ricoh 4081 laser printer
# *	sj48	StarJet 48 inkjet printer
# *	sparc	SPARCprinter
# *	st800	Epson Stylus 800 printer
# *	stcolor	Epson Stylus Color
# *	t4693d2  Tektronix 4693d color printer, 2 bits per R/G/B component
# *	t4693d4  Tektronix 4693d color printer, 4 bits per R/G/B component
# *	t4693d8  Tektronix 4693d color printer, 8 bits per R/G/B component
# *	tek4696  Tektronix 4695/4696 inkjet plotter
# *	uniprint  Unified printer driver -- Configurable Color ESC/P-,
#		ESC/P2-, HP-RTL/PCL mono/color driver
# *	xes	Xerox XES printers (2700, 3700, 4045, etc.)
# Fax systems:
# *	dfaxhigh  DigiBoard, Inc.'s DigiFAX software format (high resolution)
# *	dfaxlow  DigiFAX low (normal) resolution
# Fax file format:
#   ****** NOTE: all of these drivers adjust the page size to match
#   ****** one of the three CCITT standard sizes (U.S. letter with A4 width,
#   ****** A4, or B4).
#	faxg3	Group 3 fax, with EOLs but no header or EOD
#	faxg32d  Group 3 2-D fax, with EOLs but no header or EOD
#	faxg4	Group 4 fax, with EOLs but no header or EOD
#	tiffcrle  TIFF "CCITT RLE 1-dim" (= Group 3 fax with no EOLs)
#	tiffg3	TIFF Group 3 fax (with EOLs)
#	tiffg32d  TIFF Group 3 2-D fax
#	tiffg4	TIFF Group 4 fax
# High-level file formats:
#	epswrite  EPS output (like PostScript Distillery)
#	pdfwrite  PDF output (like Adobe Acrobat Distiller)
#	pswrite  PostScript output (like PostScript Distillery)
#	pxlmono  Black-and-white PCL XL
#	pxlcolor  Color PCL XL
# Other raster file formats and devices:
#	bit	Plain bits, monochrome
#	bitrgb	Plain bits, RGB
#	bitcmyk  Plain bits, CMYK
#	bmpmono	Monochrome MS Windows .BMP file format
#	bmp16	4-bit (EGA/VGA) .BMP file format
#	bmp256	8-bit (256-color) .BMP file format
#	bmp16m	24-bit .BMP file format
#	cgmmono  Monochrome (black-and-white) CGM -- LOW LEVEL OUTPUT ONLY
#	cgm8	8-bit (256-color) CGM -- DITTO
#	cgm24	24-bit color CGM -- DITTO
# *	cif	CIF file format for VLSI
#	jpeg	JPEG format, RGB output
#	jpeggray  JPEG format, gray output
#	miff24	ImageMagick MIFF format, 24-bit direct color, RLE compressed
# *	mgrmono  1-bit monochrome MGR devices
# *	mgrgray2  2-bit gray scale MGR devices
# *	mgrgray4  4-bit gray scale MGR devices
# *	mgrgray8  8-bit gray scale MGR devices
# *	mgr4	4-bit (VGA) color MGR devices
# *	mgr8	8-bit color MGR devices
#	pcxmono	PCX file format, monochrome (1-bit black and white)
#	pcxgray	PCX file format, 8-bit gray scale
#	pcx16	PCX file format, 4-bit planar (EGA/VGA) color
#	pcx256	PCX file format, 8-bit chunky color
#	pcx24b	PCX file format, 24-bit color (3 8-bit planes)
#	pcxcmyk PCX file format, 4-bit chunky CMYK color
#	pbm	Portable Bitmap (plain format)
#	pbmraw	Portable Bitmap (raw format)
#	pgm	Portable Graymap (plain format)
#	pgmraw	Portable Graymap (raw format)
#	pgnm	Portable Graymap (plain format), optimizing to PBM if possible
#	pgnmraw	Portable Graymap (raw format), optimizing to PBM if possible
#	pnm	Portable Pixmap (plain format) (RGB), optimizing to PGM or PBM
#		 if possible
#	pnmraw	Portable Pixmap (raw format) (RGB), optimizing to PGM or PBM
#		 if possible
#	ppm	Portable Pixmap (plain format) (RGB)
#	ppmraw	Portable Pixmap (raw format) (RGB)
#	pkm	Portable inKmap (plain format) (4-bit CMYK => RGB)
#	pkmraw	Portable inKmap (raw format) (4-bit CMYK => RGB)
#	pngmono	Monochrome Portable Network Graphics (PNG)
#	pnggray	8-bit gray Portable Network Graphics (PNG)
#	png16	4-bit color Portable Network Graphics (PNG)
#	png256	8-bit color Portable Network Graphics (PNG)
#	png16m	24-bit color Portable Network Graphics (PNG)
#	psmono	PostScript (Level 1) monochrome image
#	psgray	PostScript (Level 1) 8-bit gray image
#	sgirgb	SGI RGB pixmap format
#	tiff12nc  TIFF 12-bit RGB, no compression
#	tiff24nc  TIFF 24-bit RGB, no compression (NeXT standard format)
#	tifflzw  TIFF LZW (tag = 5) (monochrome)
#	tiffpack  TIFF PackBits (tag = 32773) (monochrome)

# User-contributed drivers marked with * require hardware or software
# that is not available to Aladdin Enterprises.  Please contact the
# original contributors, not Aladdin Enterprises, if you have questions.
# Contact information appears in the driver entry below.
#
# Drivers marked with a + are maintained by Aladdin Enterprises with
# the assistance of users, since Aladdin Enterprises doesn't have access to
# the hardware for these either.

# If you add drivers, it would be nice if you kept each list
# in alphabetical order.

###### ----------------------- End of catalog ----------------------- ######

# As noted in gs.mak, DEVICE_DEVS and DEVICE_DEVS1..15 select the devices
# that should be included in a given configuration.  By convention, these
# are used as follows.  Each of these must be limited to about 10 devices
# so as not to overflow the 120 character limit on MS-DOS command lines.
#	DEVICE_DEVS - the default device, and any display devices.
#	DEVICE_DEVS1 - additional display devices if needed.
#	DEVICE_DEVS2 - dot matrix printers.
#	DEVICE_DEVS3 - H-P monochrome printers.
#	DEVICE_DEVS4 - H-P color printers.
#	DEVICE_DEVS5 - additional H-P printers if needed.
#	DEVICE_DEVS6 - other ink-jet and laser printers.
#	DEVICE_DEVS7 - fax file formats.
#	DEVICE_DEVS8 - PCX file formats.
#	DEVICE_DEVS9 - PBM/PGM/PPM file formats.
#	DEVICE_DEVS10 - black-and-white TIFF file formats.
#	DEVICE_DEVS11 - BMP and color TIFF file formats.
#	DEVICE_DEVS12 - PostScript image and 'bit' file formats.
#	DEVICE_DEVS13 - PNG file formats.
#	DEVICE_DEVS14 - CGM, JPEG, and MIFF file formats.
#	DEVICE_DEVS15 - high-level (PostScript and PDF) file formats.
# Feel free to disregard this convention if it gets in your way.

# If you want to add a new device driver, the examples below should be
# enough of a guide to the correct form for the makefile rules.
# Note that all drivers other than displays must include page.dev in their
# dependencies and use $(SETPDEV) rather than $(SETDEV) in their rule bodies.

# All device drivers depend on the following:
GDEV=$(AK) $(ECHOGS_XE) $(gserrors_h) $(gx_h) $(gxdevice_h)

# "Printer" drivers depend on the following:
PDEVH=$(AK) $(gdevprn_h)

# Define the header files for device drivers.  Every header file used by
# more than one device driver family must be listed here.
gdev8bcm_h=gdev8bcm.h
gdevpccm_h=gdevpccm.h
gdevpcfb_h=gdevpcfb.h $(dos__h)
gdevpcl_h=gdevpcl.h
gdevsvga_h=gdevsvga.h
gdevx_h=gdevx.h

###### ----------------------- Device support ----------------------- ######

# Provide a mapping between StandardEncoding and ISOLatin1Encoding.
gdevemap.$(OBJ): gdevemap.c $(AK) $(std_h)

# Implement dynamic color management for 8-bit mapped color displays.
gdev8bcm.$(OBJ): gdev8bcm.c $(AK) \
  $(gx_h) $(gxdevice_h) $(gdev8bcm_h)

###### ------------------- MS-DOS display devices ------------------- ######

# There are really only three drivers: an EGA/VGA driver (4 bit-planes,
# plane-addressed), a SuperVGA driver (8 bit-planes, byte addressed),
# and a special driver for the S3 chip.

# PC display color mapping
gdevpccm.$(OBJ): gdevpccm.c $(AK) \
  $(gx_h) $(gsmatrix_h) $(gxdevice_h) $(gdevpccm_h)

### ----------------------- EGA and VGA displays ----------------------- ###

# The shared MS-DOS makefile defines PCFBASM as either gdevegaa.$(OBJ)
# or an empty string.

gdevegaa.$(OBJ): gdevegaa.asm

# NOTE: for direct frame buffer addressing under SCO Unix or Xenix,
# change gdevevga to gdevsco in the following line.  Also, since
# SCO's /bin/as does not support the "out" instructions, you must build
# the gnu assembler and have it on your path as "as".
EGAVGA=gdevevga.$(OBJ) gdevpcfb.$(OBJ) gdevpccm.$(OBJ) $(PCFBASM)
#EGAVGA=gdevsco.$(OBJ) gdevpcfb.$(OBJ) gdevpccm.$(OBJ) $(PCFBASM)

gdevevga.$(OBJ): gdevevga.c $(GDEV) $(memory__h) $(gdevpcfb_h)
	$(CCD) gdevevga.c

gdevsco.$(OBJ): gdevsco.c $(GDEV) $(memory__h) $(gdevpcfb_h)

# Common code for MS-DOS and SCO.
gdevpcfb.$(OBJ): gdevpcfb.c $(GDEV) $(memory__h) $(gconfigv_h)\
 $(gdevpccm_h) $(gdevpcfb_h) $(gsparam_h)
	$(CCD) gdevpcfb.c

# The EGA/VGA family includes EGA and VGA.  Many SuperVGAs in 800x600,
# 16-color mode can share the same code; see the next section below.

ega.dev: $(EGAVGA)
	$(SETDEV) ega $(EGAVGA)

vga.dev: $(EGAVGA)
	$(SETDEV) vga $(EGAVGA)

### ------------------------- SuperVGA displays ------------------------ ###

# SuperVGA displays in 16-color, 800x600 mode are really just slightly
# glorified VGA's, so we can handle them all with a single driver.
# The way to select them on the command line is with
#	-sDEVICE=svga16 -dDisplayMode=NNN
# where NNN is the display mode in decimal.  See use.txt for the modes
# for some popular display chipsets.

svga16.dev: $(EGAVGA)
	$(SETDEV) svga16 $(EGAVGA)

# More capable SuperVGAs have a wide variety of slightly differing
# interfaces, so we need a separate driver for each one.

SVGA=gdevsvga.$(OBJ) gdevpccm.$(OBJ) $(PCFBASM)

gdevsvga.$(OBJ): gdevsvga.c $(GDEV) $(memory__h) $(gconfigv_h)\
 $(gsparam_h) $(gxarith_h) $(gdevpccm_h) $(gdevpcfb_h) $(gdevsvga_h)
	$(CCD) gdevsvga.c

# The SuperVGA family includes: Avance Logic Inc., ATI Wonder, S3,
# Trident, Tseng ET3000/4000, and VESA.

ali.dev: $(SVGA)
	$(SETDEV) ali $(SVGA)

atiw.dev: $(SVGA)
	$(SETDEV) atiw $(SVGA)

tseng.dev: $(SVGA)
	$(SETDEV) tseng $(SVGA)

tvga.dev: $(SVGA)
	$(SETDEV) tvga $(SVGA)

vesa.dev: $(SVGA)
	$(SETDEV) vesa $(SVGA)

# The S3 driver doesn't share much code with the others.

s3vga_=gdevs3ga.$(OBJ) gdevsvga.$(OBJ) gdevpccm.$(OBJ)
s3vga.dev: $(SVGA) $(s3vga_)
	$(SETDEV) s3vga $(SVGA)
	$(ADDMOD) s3vga -obj $(s3vga_)

gdevs3ga.$(OBJ): gdevs3ga.c $(GDEV) $(gdevpcfb_h) $(gdevsvga_h)
	$(CCD) gdevs3ga.c

### ------------ The BGI (Borland Graphics Interface) device ----------- ###

cgaf.$(OBJ): $(BGIDIR)\cga.bgi
	$(BGIDIR)\bgiobj /F $(BGIDIR)\cga

egavgaf.$(OBJ): $(BGIDIR)\egavga.bgi
	$(BGIDIR)\bgiobj /F $(BGIDIR)\egavga

# Include egavgaf.$(OBJ) for debugging only.
bgi_=gdevbgi.$(OBJ) cgaf.$(OBJ)
bgi.dev: $(bgi_)
	$(SETDEV) bgi $(bgi_)
	$(ADDMOD) bgi -lib $(LIBDIR)\graphics

gdevbgi.$(OBJ): gdevbgi.c $(GDEV) $(MAKEFILE) $(gxxfont_h)
	$(CCC) -DBGI_LIB="$(BGIDIRSTR)" gdevbgi.c

### ------------------- The Hercules Graphics display ------------------- ###

herc_=gdevherc.$(OBJ)
herc.dev: $(herc_)
	$(SETDEV) herc $(herc_)

gdevherc.$(OBJ): gdevherc.c $(GDEV) $(dos__h) $(gsmatrix_h) $(gxbitmap_h)
	$(CCC) gdevherc.c

### ---------------------- The Private Eye display ---------------------- ###
### Note: this driver was contributed by a user:                          ###
###   please contact narf@media-lab.media.mit.edu if you have questions.  ###

pe_=gdevpe.$(OBJ)
pe.dev: $(pe_)
	$(SETDEV) pe $(pe_)

gdevpe.$(OBJ): gdevpe.c $(GDEV) $(memory__h)

###### ----------------------- Other displays ------------------------ ######

### -------------------- The MS-Windows 3.n DLL ------------------------- ###

gsdll_h=gsdll.h

gdevmswn_h=gdevmswn.h $(GDEV)\
 $(dos__h) $(memory__h) $(string__h) $(windows__h)\
 gp_mswin.h

gdevmswn.$(OBJ): gdevmswn.c $(gdevmswn_h) $(gp_h) $(gpcheck_h) \
 $(gsdll_h) $(gsparam_h) $(gdevpccm_h)
	$(CCCWIN) gdevmswn.c

gdevmsxf.$(OBJ): gdevmsxf.c $(ctype__h) $(math__h) $(memory__h) $(string__h)\
 $(gdevmswn_h) $(gsstruct_h) $(gsutil_h) $(gxxfont_h)
	$(CCCWIN) gdevmsxf.c

# An implementation using a DIB filled by an image device.
gdevwdib.$(OBJ): gdevwdib.c $(gdevmswn_h) $(gsdll_h) $(gxdevmem_h)
	$(CCCWIN) gdevwdib.c

mswindll_=gdevmswn.$(OBJ) gdevmsxf.$(OBJ) gdevwdib.$(OBJ) \
  gdevemap.$(OBJ) gdevpccm.$(OBJ)
mswindll.dev: $(mswindll_)
	$(SETDEV) mswindll $(mswindll_)

### -------------------- The MS-Windows DDB 3.n printer ----------------- ###

mswinprn_=gdevwprn.$(OBJ) gdevmsxf.$(OBJ)
mswinprn.dev: $(mswinprn_)
	$(SETDEV) mswinprn $(mswinprn_)

gdevwprn.$(OBJ): gdevwprn.c $(gdevmswn_h) $(gp_h)
	$(CCCWIN) gdevwprn.c

### -------------------- The MS-Windows DIB 3.n printer ----------------- ###

mswinpr2_=gdevwpr2.$(OBJ)
mswinpr2.dev: $(mswinpr2_) page.dev
	$(SETPDEV) mswinpr2 $(mswinpr2_)

gdevwpr2.$(OBJ): gdevwpr2.c $(PDEVH) $(windows__h)\
 $(gdevpccm_h) $(gp_h) gp_mswin.h
	$(CCCWIN) gdevwpr2.c

### ------------------ OS/2 Presentation Manager device ----------------- ###

os2pm_=gdevpm.$(OBJ) gdevpccm.$(OBJ)
os2pm.dev: $(os2pm_)
	$(SETDEV) os2pm $(os2pm_)

os2dll_=gdevpm.$(OBJ) gdevpccm.$(OBJ)
os2dll.dev: $(os2dll_)
	$(SETDEV) os2dll $(os2dll_)

gdevpm.$(OBJ): gdevpm.c $(string__h)\
 $(gp_h) $(gpcheck_h)\
 $(gsdll_h) $(gserrors_h) $(gsexit_h) $(gsparam_h)\
 $(gx_h) $(gxdevice_h) $(gxdevmem_h)\
 $(gdevpccm_h) gdevpm.h

### --------------------------- The OS/2 printer ------------------------ ###

os2prn_=gdevos2p.$(OBJ)
os2prn.dev: $(os2prn_) page.dev
	$(SETPDEV) os2prn $(os2prn_)

os2prn.$(OBJ): os2prn.c $(gp_h)

### -------------- The AT&T 3b1 Unixpc monochrome display --------------- ###
### Note: this driver was contributed by a user: please contact           ###
###       Andy Fyfe (andy@cs.caltech.edu) if you have questions.          ###

att3b1_=gdev3b1.$(OBJ)
att3b1.dev: $(att3b1_)
	$(SETDEV) att3b1 $(att3b1_)

gdev3b1.$(OBJ): gdev3b1.c $(GDEV)

### ---------------------- Linux PC with vgalib ------------------------- ###
### Note: these drivers were contributed by users.                        ###
### For questions about the lvga256 driver, please contact                ###
###       Ludger Kunz (ludger.kunz@fernuni-hagen.de).                     ###
### For questions about the vgalib driver, please contact                 ###
###       Erik Talvola (talvola@gnu.ai.mit.edu).                          ###

lvga256_=gdevl256.$(OBJ)
lvga256.dev: $(lvga256_)
	$(SETDEV) lvga256 $(lvga256_)
	$(ADDMOD) lvga256 -lib vga vgagl

gdevl256.$(OBJ): gdevl256.c $(GDEV)

vgalib_=gdevvglb.$(OBJ) gdevpccm.$(OBJ)
vgalib.dev: $(vgalib_)
	$(SETDEV) vgalib $(vgalib_)
	$(ADDMOD) vgalib -lib vga

gdevvglb.$(OBJ): gdevvglb.c $(GDEV) $(gdevpccm_h) $(gsparam_h)

### ------------------- Sony NeWS frame buffer device ------------------ ###
### Note: this driver was contributed by a user: please contact          ###
###       Mike Smolenski (mike@intertech.com) if you have questions.     ###

# This is implemented as a 'printer' device.
sonyfb_=gdevsnfb.$(OBJ)
sonyfb.dev: $(sonyfb_) page.dev
	$(SETPDEV) sonyfb $(sonyfb_)

gdevsnfb.$(OBJ): gdevsnfb.c $(PDEVH)

### ------------------------ The SunView device ------------------------ ###
### Note: this driver is maintained by a user: if you have questions,    ###
###       please contact Andreas Stolcke (stolcke@icsi.berkeley.edu).    ###

sunview_=gdevsun.$(OBJ)
sunview.dev: $(sunview_)
	$(SETDEV) sunview $(sunview_)
	$(ADDMOD) sunview -lib suntool sunwindow pixrect

gdevsun.$(OBJ): gdevsun.c $(GDEV) $(malloc__h)\
 $(gscdefs_h) $(gserrors_h) $(gsmatrix_h)

### -------------------------- The X11 device -------------------------- ###

# Aladdin Enterprises does not support Ghostview.  For more information
# about Ghostview, please contact Tim Theisen (ghostview@cs.wisc.edu).

# See the main makefile for the definition of XLIBS.
x11_=gdevx.$(OBJ) gdevxini.$(OBJ) gdevxxf.$(OBJ) gdevemap.$(OBJ)
x11.dev: $(x11_)
	$(SETDEV) x11 $(x11_)
	$(ADDMOD) x11 -lib $(XLIBS)

# See the main makefile for the definition of XINCLUDE.
GDEVX=$(GDEV) x_.h gdevx.h $(MAKEFILE)
gdevx.$(OBJ): gdevx.c $(GDEVX) $(math__h) $(memory__h) $(gsparam_h)
	$(CCC) $(XINCLUDE) gdevx.c

gdevxini.$(OBJ): gdevxini.c $(GDEVX) $(math__h) $(memory__h) $(gserrors_h)
	$(CCC) $(XINCLUDE) gdevxini.c

gdevxxf.$(OBJ): gdevxxf.c $(GDEVX) $(math__h) $(memory__h)\
 $(gsstruct_h) $(gsutil_h) $(gxxfont_h)
	$(CCC) $(XINCLUDE) gdevxxf.c

# Alternate X11-based devices to help debug other drivers.
# x11alpha pretends to have 4 bits of alpha channel.
# x11cmyk pretends to be a CMYK device with 1 bit each of C,M,Y,K.
# x11gray2 pretends to be a 2-bit gray-scale device.
# x11mono pretends to be a black-and-white device.
x11alt_=$(x11_) gdevxalt.$(OBJ)
x11alpha.dev: $(x11alt_)
	$(SETDEV) x11alpha $(x11alt_)
	$(ADDMOD) x11alpha -lib $(XLIBS)

x11cmyk.dev: $(x11alt_)
	$(SETDEV) x11cmyk $(x11alt_)
	$(ADDMOD) x11cmyk -lib $(XLIBS)

x11gray2.dev: $(x11alt_)
	$(SETDEV) x11gray2 $(x11alt_)
	$(ADDMOD) x11gray2 -lib $(XLIBS)

x11mono.dev: $(x11alt_)
	$(SETDEV) x11mono $(x11alt_)
	$(ADDMOD) x11mono -lib $(XLIBS)

gdevxalt.$(OBJ): gdevxalt.c $(GDEVX) $(math__h) $(memory__h) $(gsparam_h)
	$(CCC) $(XINCLUDE) gdevxalt.c

### ------------------------- DEC sixel displays ------------------------ ###
### Note: this driver was contributed by a user: please contact           ###
###   Phil Keegstra (keegstra@tonga.gsfc.nasa.gov) if you have questions. ###

# This is a "printer" device, but it probably shouldn't be.
# I don't know why the implementor chose to do it this way.
sxlcrt_=gdevln03.$(OBJ)
sxlcrt.dev: $(sxlcrt_) page.dev
	$(SETPDEV) sxlcrt $(sxlcrt_)

###### --------------- Memory-buffered printer devices --------------- ######

### --------------------- The Apple printer devices --------------------- ###
### Note: these drivers were contributed by users.                        ###
###   If you have questions about the DMP driver, please contact          ###
###	Mark Wedel (master@cats.ucsc.edu).                                ###
###   If you have questions about the Imagewriter drivers, please contact ###
###	Jonathan Luckey (luckey@rtfm.mlb.fl.us).                          ###
###   If you have questions about the Imagewriter LQ driver, please       ###
###	contact Scott Barker (barkers@cuug.ab.ca).                        ###

appledmp_=gdevadmp.$(OBJ)

gdevadmp.$(OBJ): gdevadmp.c $(PDEVH)

appledmp.dev: $(appledmp_) page.dev
	$(SETPDEV) appledmp $(appledmp_)

iwhi.dev: $(appledmp_) page.dev
	$(SETPDEV) iwhi $(appledmp_)

iwlo.dev: $(appledmp_) page.dev
	$(SETPDEV) iwlo $(appledmp_)

iwlq.dev: $(appledmp_) page.dev
	$(SETPDEV) iwlq $(appledmp_)

### ------------ The Canon BubbleJet BJ10e and BJ200 devices ------------ ###

bj10e_=gdevbj10.$(OBJ)

bj10e.dev: $(bj10e_) page.dev
	$(SETPDEV) bj10e $(bj10e_)

bj200.dev: $(bj10e_) page.dev
	$(SETPDEV) bj200 $(bj10e_)

gdevbj10.$(OBJ): gdevbj10.c $(PDEVH)

### ------------- The CalComp Raster Format ----------------------------- ###
### Note: this driver was contributed by a user: please contact           ###
###       Ernst Muellner (ernst.muellner@oenzl.siemens.de) if you have    ###
###       questions.                                                      ###

ccr_=gdevccr.$(OBJ)
ccr.dev: $(ccr_) page.dev
	$(SETPDEV) ccr $(ccr_)

gdevccr.$(OBJ): gdevccr.c $(PDEVH)

### ----------- The H-P DeskJet and LaserJet printer devices ----------- ###

### These are essentially the same device.
### NOTE: printing at full resolution (300 DPI) requires a printer
###   with at least 1.5 Mb of memory.  150 DPI only requires .5 Mb.
### Note that the lj4dith driver is included with the H-P color printer
###   drivers below.

HPPCL=gdevpcl.$(OBJ)
HPMONO=gdevdjet.$(OBJ) $(HPPCL)

gdevpcl.$(OBJ): gdevpcl.c $(PDEVH) $(gdevpcl_h)

gdevdjet.$(OBJ): gdevdjet.c $(PDEVH) $(gdevpcl_h)

deskjet.dev: $(HPMONO) page.dev
	$(SETPDEV) deskjet $(HPMONO)

djet500.dev: $(HPMONO) page.dev
	$(SETPDEV) djet500 $(HPMONO)

laserjet.dev: $(HPMONO) page.dev
	$(SETPDEV) laserjet $(HPMONO)

ljetplus.dev: $(HPMONO) page.dev
	$(SETPDEV) ljetplus $(HPMONO)

### Selecting ljet2p provides TIFF (mode 2) compression on LaserJet III,
### IIIp, IIId, IIIsi, IId, and IIp. 

ljet2p.dev: $(HPMONO) page.dev
	$(SETPDEV) ljet2p $(HPMONO)

### Selecting ljet3 provides Delta Row (mode 3) compression on LaserJet III,
### IIIp, IIId, IIIsi.

ljet3.dev: $(HPMONO) page.dev
	$(SETPDEV) ljet3 $(HPMONO)

### Selecting ljet3d also provides duplex printing capability.

ljet3d.dev: $(HPMONO) page.dev
	$(SETPDEV) ljet3d $(HPMONO)

### Selecting ljet4 also provides Delta Row compression on LaserJet IV series.

ljet4.dev: $(HPMONO) page.dev
	$(SETPDEV) ljet4 $(HPMONO)

lp2563.dev: $(HPMONO) page.dev
	$(SETPDEV) lp2563 $(HPMONO)

oce9050.dev: $(HPMONO) page.dev
	$(SETPDEV) oce9050 $(HPMONO)

### ------------------ The H-P LaserJet 5 and 6 devices ----------------- ###

### These drivers use H-P's new PCL XL printer language, like H-P's
### LaserJet 5 Enhanced driver for MS Windows.  We don't recommend using
### them:
###	- If you have a LJ 5L or 5P, which isn't a "real" LaserJet 5,
###	use the ljet4 driver instead.  (The lj5 drivers won't work.)
###	- If you have any other model of LJ 5 or 6, use the pxlmono
###	driver, which often produces much more compact output.

gdevpxat_h=gdevpxat.h
gdevpxen_h=gdevpxen.h
gdevpxop_h=gdevpxop.h

ljet5_=gdevlj56.$(OBJ) $(HPPCL)
lj5mono.dev: $(ljet5_) page.dev
	$(SETPDEV) lj5mono $(ljet5_)

lj5gray.dev: $(ljet5_) page.dev
	$(SETPDEV) lj5gray $(ljet5_)

gdevlj56.$(OBJ): gdevlj56.c $(PDEVH) $(gdevpcl_h)\
 $(gdevpxat_h) $(gdevpxen_h) $(gdevpxop_h)

### The H-P DeskJet, PaintJet, and DesignJet family color printer devices.###
### Note: there are two different 500C drivers, both contributed by users.###
###   If you have questions about the djet500c driver,                    ###
###       please contact AKayser@et.tudelft.nl.                           ###
###   If you have questions about the cdj* drivers,                       ###
###       please contact g.cameron@biomed.abdn.ac.uk.                     ###
###   If you have questions about the dnj560c driver,                     ###
###       please contact koert@zen.cais.com.                              ###
###   If you have questions about the lj4dith driver,                     ###
###       please contact Eckhard.Rueggeberg@ts.go.dlr.de.                 ###
###   If you have questions about the BJC600/BJC4000, BJC800, or ESCP     ###
###       drivers, please contact Yves.Arrouye@imag.fr.                   ###

cdeskjet_=gdevcdj.$(OBJ) $(HPPCL)

cdeskjet.dev: $(cdeskjet_) page.dev
	$(SETPDEV) cdeskjet $(cdeskjet_)

cdjcolor.dev: $(cdeskjet_) page.dev
	$(SETPDEV) cdjcolor $(cdeskjet_)

cdjmono.dev: $(cdeskjet_) page.dev
	$(SETPDEV) cdjmono $(cdeskjet_)

cdj500.dev: $(cdeskjet_) page.dev
	$(SETPDEV) cdj500 $(cdeskjet_)

cdj550.dev: $(cdeskjet_) page.dev
	$(SETPDEV) cdj550 $(cdeskjet_)

declj250.dev: $(cdeskjet_) page.dev
	$(SETPDEV) declj250 $(cdeskjet_)

dnj650c.dev: $(cdeskjet_) page.dev
	$(SETPDEV) dnj650c $(cdeskjet_)

lj4dith.dev: $(cdeskjet_) page.dev
	$(SETPDEV) lj4dith $(cdeskjet_)

pj.dev: $(cdeskjet_) page.dev
	$(SETPDEV) pj $(cdeskjet_)

pjxl.dev: $(cdeskjet_) page.dev
	$(SETPDEV) pjxl $(cdeskjet_)

pjxl300.dev: $(cdeskjet_) page.dev
	$(SETPDEV) pjxl300 $(cdeskjet_)

# Note: the BJC600 driver also works for the BJC4000.
bjc600.dev: $(cdeskjet_) page.dev
	$(SETPDEV) bjc600 $(cdeskjet_)

bjc800.dev: $(cdeskjet_) page.dev
	$(SETPDEV) bjc800 $(cdeskjet_)

escp.dev: $(cdeskjet_) page.dev
	$(SETPDEV) escp $(cdeskjet_)

# NB: you can also customise the build if required, using
# -DBitsPerPixel=<number> if you wish the default to be other than 24
# for the generic drivers (cdj500, cdj550, pjxl300, pjtest, pjxltest).
gdevcdj.$(OBJ): gdevcdj.c $(std_h) $(PDEVH) gdevbjc.h\
 $(gsparam_h) $(gsstate_h) $(gxlum_h)\
 $(gdevpcl_h)

djet500c_=gdevdjtc.$(OBJ) $(HPPCL)
djet500c.dev: $(djet500c_) page.dev
	$(SETPDEV) djet500c $(djet500c_)

gdevdjtc.$(OBJ): gdevdjtc.c $(PDEVH) $(malloc__h) $(gdevpcl_h)

### -------------------- The Mitsubishi CP50 printer -------------------- ###
### Note: this driver was contributed by a user: please contact           ###
###       Michael Hu (michael@ximage.com) if you have questions.          ###

cp50_=gdevcp50.$(OBJ)
cp50.dev: $(cp50_) page.dev
	$(SETPDEV) cp50 $(cp50_)

gdevcp50.$(OBJ): gdevcp50.c $(PDEVH)

### ----------------- The generic Epson printer device ----------------- ###
### Note: most of this code was contributed by users.  Please contact    ###
###       the following people if you have questions:                    ###
###   eps9mid - Guenther Thomsen (thomsen@cs.tu-berlin.de)               ###
###   eps9high - David Wexelblat (dwex@mtgzfs3.att.com)                  ###
###   ibmpro - James W. Birdsall (jwbirdsa@picarefy.picarefy.com)        ###

epson_=gdevepsn.$(OBJ)

epson.dev: $(epson_) page.dev
	$(SETPDEV) epson $(epson_)

eps9mid.dev: $(epson_) page.dev
	$(SETPDEV) eps9mid $(epson_)

eps9high.dev: $(epson_) page.dev
	$(SETPDEV) eps9high $(epson_)

gdevepsn.$(OBJ): gdevepsn.c $(PDEVH)

### ----------------- The IBM Proprinter printer device ---------------- ###

ibmpro.dev: $(epson_) page.dev
	$(SETPDEV) ibmpro $(epson_)

### -------------- The Epson LQ-2550 color printer device -------------- ###
### Note: this driver was contributed by users: please contact           ###
###       Dave St. Clair (dave@exlog.com) if you have questions.         ###

epsonc_=gdevepsc.$(OBJ)
epsonc.dev: $(epsonc_) page.dev
	$(SETPDEV) epsonc $(epsonc_)

gdevepsc.$(OBJ): gdevepsc.c $(PDEVH)

### ------------- The Epson ESC/P 2 language printer devices ------------- ###
### Note: these drivers were contributed by users.                         ###
### For questions about the Stylus 800 and AP3250 drivers, please contact  ###
###        Richard Brown (rab@tauon.ph.unimelb.edu.au).                    ###
### For questions about the Stylus Color drivers, please contact           ###
###        Gunther Hess (gunther@elmos.de).                                ###

ESCP2=gdevescp.$(OBJ)

gdevescp.$(OBJ): gdevescp.c $(PDEVH)

ap3250.dev: $(ESCP2) page.dev
	$(SETPDEV) ap3250 $(ESCP2)

st800.dev: $(ESCP2) page.dev
	$(SETPDEV) st800 $(ESCP2)

stcolor1_=gdevstc.$(OBJ) gdevstc1.$(OBJ) gdevstc2.$(OBJ)
stcolor2_=gdevstc3.$(OBJ) gdevstc4.$(OBJ)
stcolor.dev: $(stcolor1_) $(stcolor2_) page.dev
	$(SETPDEV) stcolor $(stcolor1_)
	$(ADDMOD) stcolor -obj $(stcolor2_)

gdevstc.$(OBJ): gdevstc.c gdevstc.h $(PDEVH)

gdevstc1.$(OBJ): gdevstc1.c gdevstc.h $(PDEVH)

gdevstc2.$(OBJ): gdevstc2.c gdevstc.h $(PDEVH)

gdevstc3.$(OBJ): gdevstc3.c gdevstc.h $(PDEVH)

gdevstc4.$(OBJ): gdevstc4.c gdevstc.h $(PDEVH)

### --------------- Ugly/Update -> Unified Printer Driver ---------------- ###
### For questions about this driver, please contact:                       ###
###        Gunther Hess (gunther@elmos.de)                                 ###

uniprint_=gdevupd.$(OBJ)
uniprint.dev: $(uniprint_) page.dev
	$(SETPDEV) uniprint $(uniprint_)

gdevupd.$(OBJ): gdevupd.c $(PDEVH) $(gsparam_h)

### -------------- cdj850 - HP 850c Driver under development ------------- ###
### Since this driver is in the development-phase it is not distributed    ###
### with ghostscript, but it is available via anonymous ftp from:          ###
###                        ftp://bonk.ethz.ch                              ###
### For questions about this driver, please contact:                       ###
###       Uli Wortmann (E-Mail address inside the driver-package)          ###

cdeskjet8_=gdevcd8.$(OBJ) $(HPPCL)

cdj850.dev: $(cdeskjet8_) page.dev
	$(SETPDEV) cdj850 $(cdeskjet8_)

### ------------ The H-P PaintJet color printer device ----------------- ###
### Note: this driver also supports the DEC LJ250 color printer, which   ###
###       has a PaintJet-compatible mode, and the PaintJet XL.           ###
### If you have questions about the XL, please contact Rob Reiss         ###
###       (rob@moray.berkeley.edu).                                      ###

PJET=gdevpjet.$(OBJ) $(HPPCL)

gdevpjet.$(OBJ): gdevpjet.c $(PDEVH) $(gdevpcl_h)

lj250.dev: $(PJET) page.dev
	$(SETPDEV) lj250 $(PJET)

paintjet.dev: $(PJET) page.dev
	$(SETPDEV) paintjet $(PJET)

pjetxl.dev: $(PJET) page.dev
	$(SETPDEV) pjetxl $(PJET)

### -------------- Imagen ImPress Laser Printer device ----------------- ###
### Note: this driver was contributed by a user: please contact          ###
###       Alan Millar (AMillar@bolis.sf-bay.org) if you have questions.  ###
### Set USE_BYTE_STREAM if using parallel interface;                     ###
### Don't set it if using 'ipr' spooler (default).                       ###
### You may also add -DA4 if needed for A4 paper.			 ###

imagen_=gdevimgn.$(OBJ)
imagen.dev: $(imagen_) page.dev
	$(SETPDEV) imagen $(imagen_)

gdevimgn.$(OBJ): gdevimgn.c $(PDEVH)
	$(CCC) gdevimgn.c			# for ipr spooler
#	$(CCC) -DUSE_BYTE_STREAM gdevimgn.c	# for parallel

### ------- The IBM 3852 JetPrinter color inkjet printer device -------- ###
### Note: this driver was contributed by users: please contact           ###
###       Kevin Gift (kgift@draper.com) if you have questions.           ###
### Note that the paper size that can be addressed by the graphics mode  ###
###   used in this driver is fixed at 7-1/2 inches wide (the printable   ###
###   width of the jetprinter itself.)                                   ###

jetp3852_=gdev3852.$(OBJ)
jetp3852.dev: $(jetp3852_) page.dev
	$(SETPDEV) jetp3852 $(jetp3852_)

gdev3852.$(OBJ): gdev3852.c $(PDEVH) $(gdevpcl_h)

### ---------- The Canon LBP-8II and LIPS III printer devices ---------- ###
### Note: these drivers were contributed by users.                       ###
### For questions about these drivers, please contact                    ###
###       Lauri Paatero, lauri.paatero@paatero.pp.fi                     ###

lbp8_=gdevlbp8.$(OBJ)
lbp8.dev: $(lbp8_) page.dev
	$(SETPDEV) lbp8 $(lbp8_)

lips3.dev: $(lbp8_) page.dev
	$(SETPDEV) lips3 $(lbp8_)

gdevlbp8.$(OBJ): gdevlbp8.c $(PDEVH)

### ----------- The DEC LN03/LA50/LA70/LA75 printer devices ------------ ###
### Note: this driver was contributed by users: please contact           ###
###       Ulrich Mueller (ulm@vsnhd1.cern.ch) if you have questions.     ###
### For questions about LA50 and LA75, please contact                    ###
###       Ian MacPhedran (macphed@dvinci.USask.CA).                      ###
### For questions about the LA70, please contact                         ###
###       Bruce Lowekamp (lowekamp@csugrad.cs.vt.edu).                   ###
### For questions about the LA75plus, please contact                     ###
###       Andre' Beck (Andre_Beck@IRS.Inf.TU-Dresden.de).                ###

ln03_=gdevln03.$(OBJ)
ln03.dev: $(ln03_) page.dev
	$(SETPDEV) ln03 $(ln03_)

la50.dev: $(ln03_) page.dev
	$(SETPDEV) la50 $(ln03_)

la70.dev: $(ln03_) page.dev
	$(SETPDEV) la70 $(ln03_)

la75.dev: $(ln03_) page.dev
	$(SETPDEV) la75 $(ln03_)

la75plus.dev: $(ln03_) page.dev
	$(SETPDEV) la75plus $(ln03_)

gdevln03.$(OBJ): gdevln03.c $(PDEVH)

# LA70 driver with low-resolution text enhancement.

la70t_=gdevla7t.$(OBJ)
la70t.dev: $(la70t_) page.dev
	$(SETPDEV) la70t $(la70t_)

gdevla7t.$(OBJ): gdevla7t.c $(PDEVH)

### -------------- The Epson LP-8000 laser printer device -------------- ###
### Note: this driver was contributed by a user: please contact Oleg     ###
###       Oleg Fat'yanov <faty1@rlem.titech.ac.jp> if you have questions.###

lp8000_=gdevlp8k.$(OBJ)
lp8000.dev: $(lp8000_) page.dev
	$(SETPDEV) lp8000 $(lp8000_)

gdevlp8k.$(OBJ): gdevlp8k.c $(PDEVH)

### -------------- The C.Itoh M8510 printer device --------------------- ###
### Note: this driver was contributed by a user: please contact Bob      ###
###       Smith <bob@snuffy.penfield.ny.us> if you have questions.       ###

m8510_=gdev8510.$(OBJ)
m8510.dev: $(m8510_) page.dev
	$(SETPDEV) m8510 $(m8510_)

gdev8510.$(OBJ): gdev8510.c $(PDEVH)

### -------------- 24pin Dot-matrix printer with 360DPI ---------------- ###
### Note: this driver was contributed by users.  Please contact:         ###
###    Andreas Schwab (schwab@ls5.informatik.uni-dortmund.de) for        ###
###      questions about the NEC P6;                                     ###
###    Christian Felsch (felsch@tu-harburg.d400.de) for                  ###
###      questions about the Epson LQ850.                                ###

dm24_=gdevdm24.$(OBJ)
gdevdm24.$(OBJ): gdevdm24.c $(PDEVH)

necp6.dev: $(dm24_) page.dev
	$(SETPDEV) necp6 $(dm24_)

lq850.dev: $(dm24_) page.dev
	$(SETPDEV) lq850 $(dm24_)

### ----------------- The Okidata MicroLine 182 device ----------------- ###
### Note: this driver was contributed by a user: please contact          ###
###       Maarten Koning (smeg@bnr.ca) if you have questions.            ###

oki182_=gdevo182.$(OBJ)
oki182.dev: $(oki182_) page.dev
	$(SETPDEV) oki182 $(oki182_)

gdevo182.$(OBJ): gdevo182.c $(PDEVH)

### ------------- The Okidata IBM compatible printer device ------------ ###
### Note: this driver was contributed by a user: please contact          ###
###       Charles Mack (chasm@netcom.com) if you have questions.         ###

okiibm_=gdevokii.$(OBJ)
okiibm.dev: $(okiibm_) page.dev
	$(SETPDEV) okiibm $(okiibm_)

gdevokii.$(OBJ): gdevokii.c $(PDEVH)

### ------------- The Ricoh 4081 laser printer device ------------------ ###
### Note: this driver was contributed by users:                          ###
###       please contact kdw@oasis.icl.co.uk if you have questions.      ###

r4081_=gdev4081.$(OBJ)
r4081.dev: $(r4081_) page.dev
	$(SETPDEV) r4081 $(r4081_)


gdev4081.$(OBJ): gdev4081.c $(PDEVH)

### -------------------- Sony NWP533 printer device -------------------- ###
### Note: this driver was contributed by a user: please contact Tero     ###
###       Kivinen (kivinen@joker.cs.hut.fi) if you have questions.       ###

nwp533_=gdevn533.$(OBJ)
nwp533.dev: $(nwp533_) page.dev
	$(SETPDEV) nwp533 $(nwp533_)

gdevn533.$(OBJ): gdevn533.c $(PDEVH)

### ------------------------- The SPARCprinter ------------------------- ###
### Note: this driver was contributed by users: please contact Martin    ###
###       Schulte (schulte@thp.uni-koeln.de) if you have questions.      ###
###       He would also like to hear from anyone using the driver.       ###
### Please consult the source code for additional documentation.         ###

sparc_=gdevsppr.$(OBJ)
sparc.dev: $(sparc_) page.dev
	$(SETPDEV) sparc $(sparc_)

gdevsppr.$(OBJ): gdevsppr.c $(PDEVH)

### ----------------- The StarJet SJ48 device -------------------------- ###
### Note: this driver was contributed by a user: if you have questions,  ###
###	                      .                                          ###
###       please contact Mats Akerblom (f86ma@dd.chalmers.se).           ###

sj48_=gdevsj48.$(OBJ)
sj48.dev: $(sj48_) page.dev
	$(SETPDEV) sj48 $(sj48_)

gdevsj48.$(OBJ): gdevsj48.c $(PDEVH)

### ----------------- Tektronix 4396d color printer -------------------- ###
### Note: this driver was contributed by a user: please contact          ###
###       Karl Hakimian (hakimian@haney.eecs.wsu.edu)                    ###
###       if you have questions.                                         ###

t4693d_=gdev4693.$(OBJ)
t4693d2.dev: $(t4693d_) page.dev
	$(SETPDEV) t4693d2 $(t4693d_)

t4693d4.dev: $(t4693d_) page.dev
	$(SETPDEV) t4693d4 $(t4693d_)

t4693d8.dev: $(t4693d_) page.dev
	$(SETPDEV) t4693d8 $(t4693d_)

gdev4693.$(OBJ): gdev4693.c $(PDEVH)

### -------------------- Tektronix ink-jet printers -------------------- ###
### Note: this driver was contributed by a user: please contact          ###
###       Karsten Spang (spang@nbivax.nbi.dk) if you have questions.     ###

tek4696_=gdevtknk.$(OBJ)
tek4696.dev: $(tek4696_) page.dev
	$(SETPDEV) tek4696 $(tek4696_)

gdevtknk.$(OBJ): gdevtknk.c $(PDEVH) $(malloc__h)

### ----------------- The Xerox XES printer device --------------------- ###
### Note: this driver was contributed by users: please contact           ###
###       Peter Flass (flass@lbdrscs.bitnet) if you have questions.      ###

xes_=gdevxes.$(OBJ)
xes.dev: $(xes_) page.dev
	$(SETPDEV) xes $(xes_)

gdevxes.$(OBJ): gdevxes.c $(PDEVH)

###### ------------------------- Fax devices ------------------------- ######

### --------------- Generic PostScript system compatible fax ------------ ###

# This code doesn't work yet.  Don't even think about using it.

PSFAX=gdevpfax.$(OBJ)

psfax_=$(PSFAX)
psfax.dev: $(psfax_) page.dev
	$(SETPDEV) psfax $(psfax_)
	$(ADDMOD) psfax -iodev Fax

gdevpfax.$(OBJ): gdevpfax.c $(PDEVH) $(gsparam_h) $(gxiodev_h)

### ------------------------- The DigiFAX device ------------------------ ###
###    This driver outputs images in a format suitable for use with       ###
###    DigiBoard, Inc.'s DigiFAX software.  Use -sDEVICE=dfaxhigh for     ###
###    high resolution output, -sDEVICE=dfaxlow for normal output.        ###
### Note: this driver was contributed by a user: please contact           ###
###       Rick Richardson (rick@digibd.com) if you have questions.        ###

dfax_=gdevdfax.$(OBJ) gdevtfax.$(OBJ)

dfaxlow.dev: $(dfax_) page.dev
	$(SETPDEV) dfaxlow $(dfax_)
	$(ADDMOD) dfaxlow -include cfe

dfaxhigh.dev: $(dfax_) page.dev
	$(SETPDEV) dfaxhigh $(dfax_)
	$(ADDMOD) dfaxhigh -include cfe

gdevdfax.$(OBJ): gdevdfax.c $(PDEVH) $(scfx_h) $(strimpl_h)

### --------------See under TIFF below for fax-format TIFF -------------- ###

###### ------------------- High-level file formats ------------------- ######

# Support for PostScript and PDF

gdevpsdf_h=gdevpsdf.h $(gdevvec_h) $(strimpl_h)
gdevpstr_h=gdevpstr.h

gdevpsdf.$(OBJ): gdevpsdf.c $(stdio__h) $(string__h)\
 $(gserror_h) $(gserrors_h) $(gsmemory_h) $(gsparam_h) $(gstypes_h)\
 $(gxdevice_h)\
 $(scfx_h) $(slzwx_h) $(srlx_h) $(strimpl_h)\
 $(gdevpsdf_h) $(gdevpstr_h)

gdevpstr.$(OBJ): gdevpstr.c $(math__h) $(stdio__h) $(string__h)\
 $(gdevpstr_h) $(stream_h)

# PostScript and EPS writers

pswrite1_=gdevps.$(OBJ) gdevpsdf.$(OBJ) gdevpstr.$(OBJ)
pswrite2_=scantab.$(OBJ) sfilter2.$(OBJ)
pswrite_=$(pswrite1_) $(pswrite2_)
epswrite.dev: $(ECHOGS_XE) $(pswrite_) vector.dev
	$(SETDEV) epswrite $(pswrite1_)
	$(ADDMOD) epswrite $(pswrite2_)
	$(ADDMOD) epswrite -include vector

pswrite.dev: $(ECHOGS_XE) $(pswrite_) vector.dev
	$(SETDEV) pswrite $(pswrite1_)
	$(ADDMOD) pswrite $(pswrite2_)
	$(ADDMOD) pswrite -include vector

gdevps.$(OBJ): gdevps.c $(GDEV) $(math__h) $(time__h)\
 $(gscdefs_h) $(gscspace_h) $(gsparam_h) $(gsiparam_h) $(gsmatrix_h)\
 $(gxdcolor_h)\
 $(sa85x_h) $(strimpl_h)\
 $(gdevpsdf_h) $(gdevpstr_h)

# PDF writer
# Note that gs_pdfwr.ps will only actually be loaded if the configuration
# includes a PostScript interpreter.

pdfwrite1_=gdevpdf.$(OBJ) gdevpdfd.$(OBJ) gdevpdfi.$(OBJ) gdevpdfm.$(OBJ)
pdfwrite2_=gdevpdfp.$(OBJ) gdevpdft.$(OBJ) gdevpsdf.$(OBJ) gdevpstr.$(OBJ)
pdfwrite3_=gsflip.$(OBJ) scantab.$(OBJ) sfilter2.$(OBJ) sstring.$(OBJ)
pdfwrite_=$(pdfwrite1_) $(pdfwrite2_) $(pdfwrite3_)
pdfwrite.dev: $(ECHOGS_XE) $(pdfwrite_) \
  cmyklib.dev cfe.dev dcte.dev lzwe.dev rle.dev vector.dev
	$(SETDEV) pdfwrite $(pdfwrite1_)
	$(ADDMOD) pdfwrite $(pdfwrite2_)
	$(ADDMOD) pdfwrite $(pdfwrite3_)
	$(ADDMOD) pdfwrite -ps gs_pdfwr
	$(ADDMOD) pdfwrite -include cmyklib cfe dcte lzwe rle vector

gdevpdfx_h=gdevpdfx.h $(gsparam_h) $(gxdevice_h) $(gxline_h) $(stream_h)\
 $(gdevpsdf_h) $(gdevpstr_h)

gdevpdf.$(OBJ): gdevpdf.c $(math__h) $(memory__h) $(string__h) $(time__h)\
 $(gp_h)\
 $(gdevpdfx_h) $(gscdefs_h) $(gserrors_h)\
 $(gx_h) $(gxdevice_h) $(gxfixed_h) $(gxistate_h) $(gxpaint_h)\
 $(gzcpath_h) $(gzpath_h)\
 $(scanchar_h) $(scfx_h) $(slzwx_h) $(sstring_h) $(strimpl_h) $(szlibx_h)
	$(CCCZ) gdevpdf.c

gdevpdfd.$(OBJ): gdevpdfd.c $(math__h)\
 $(gdevpdfx_h)\
 $(gx_h) $(gxdevice_h) $(gxfixed_h) $(gxistate_h) $(gxpaint_h)\
 $(gzcpath_h) $(gzpath_h)

gdevpdfi.$(OBJ): gdevpdfi.c $(math__h) $(memory__h) $(gx_h) \
  $(gdevpdfx_h) $(gscie_h) $(gscolor2_h) $(gserrors_h) $(gsflip_h)\
  $(gxcspace_h) $(gxistate_h) \
  $(sa85x_h) $(scfx_h) $(srlx_h) $(strimpl_h)

gdevpdfm.$(OBJ): gdevpdfm.c $(memory__h) $(string__h) $(gx_h) \
  $(gdevpdfx_h) $(gserrors_h) $(gsutil_h) $(scanchar_h)

gdevpdfp.$(OBJ): gdevpdfp.c $(gx_h)\
 $(gdevpdfx_h) $(gserrors_h)

gdevpdft.$(OBJ): gdevpdft.c $(math__h) $(memory__h) $(string__h) $(gx_h)\
 $(gdevpdfx_h) $(gserrors_h) $(gsutil_h)\
 $(scommon_h)

# High-level PCL XL writer

pxl_=gdevpx.$(OBJ)
pxlmono.dev: $(pxl_) $(GDEV) vector.dev
	$(SETDEV) pxlmono $(pxl_)
	$(ADDMOD) pxlmono -include vector

pxlcolor.dev: $(pxl_) $(GDEV) vector.dev
	$(SETDEV) pxlcolor $(pxl_)
	$(ADDMOD) pxlcolor -include vector

gdevpx.$(OBJ): gdevpx.c $(math__h) $(memory__h) $(string__h)\
 $(gx_h) $(gsccolor_h) $(gsdcolor_h) $(gserrors_h)\
 $(gxcspace_h) $(gxdevice_h) $(gxpath_h)\
 $(gdevpxat_h) $(gdevpxen_h) $(gdevpxop_h) $(gdevvec_h)\
 $(srlx_h) $(strimpl_h)

###### --------------------- Raster file formats --------------------- ######

### --------------------- The "plain bits" devices ---------------------- ###

bit_=gdevbit.$(OBJ)

bit.dev: $(bit_) page.dev
	$(SETPDEV) bit $(bit_)

bitrgb.dev: $(bit_) page.dev
	$(SETPDEV) bitrgb $(bit_)

bitcmyk.dev: $(bit_) page.dev
	$(SETPDEV) bitcmyk $(bit_)

gdevbit.$(OBJ): gdevbit.c $(PDEVH) $(gsparam_h) $(gxlum_h)

### ------------------------- .BMP file formats ------------------------- ###

bmp_=gdevbmp.$(OBJ) gdevpccm.$(OBJ)

gdevbmp.$(OBJ): gdevbmp.c $(PDEVH) $(gdevpccm_h)

bmpmono.dev: $(bmp_) page.dev
	$(SETPDEV) bmpmono $(bmp_)

bmp16.dev: $(bmp_) page.dev
	$(SETPDEV) bmp16 $(bmp_)

bmp256.dev: $(bmp_) page.dev
	$(SETPDEV) bmp256 $(bmp_)

bmp16m.dev: $(bmp_) page.dev
	$(SETPDEV) bmp16m $(bmp_)

### ------------- BMP driver that serves as demo of async rendering ---- ###
devasync_=gdevasyn.$(OBJ) gdevpccm.$(OBJ) gxsync.$(OBJ)

gdevasyn.$(OBJ): gdevasyn.c $(AK) $(stdio__h) $(gdevprna_h) $(gdevpccm_h)\
 $(gserrors_h) $(gpsync_h)

asynmono.dev: $(devasync_) page.dev async.dev
	$(SETPDEV) asynmono $(devasync_)
	$(ADDMOD) asynmono -include async


### -------------------------- CGM file format ------------------------- ###
### This driver is under development.  Use at your own risk.             ###
### The output is very low-level, consisting only of rectangles and      ###
### cell arrays.                                                         ###

cgm_=gdevcgm.$(OBJ) gdevcgml.$(OBJ)

gdevcgml_h=gdevcgml.h
gdevcgmx_h=gdevcgmx.h $(gdevcgml_h)

gdevcgm.$(OBJ): gdevcgm.c $(GDEV) $(memory__h)\
 $(gsparam_h) $(gdevpccm_h) $(gdevcgml_h)

gdevcgml.$(OBJ): gdevcgml.c $(memory__h) $(stdio__h)\
 $(gdevcgmx_h)

cgmmono.dev: $(cgm_)
	$(SETDEV) cgmmono $(cgm_)

cgm8.dev: $(cgm_)
	$(SETDEV) cgm8 $(cgm_)

cgm24.dev: $(cgm_)
	$(SETDEV) cgm24 $(cgm_)

### -------------------- The CIF file format for VLSI ------------------ ###
### Note: this driver was contributed by a user: please contact          ###
###       Frederic Petrot (petrot@masi.ibp.fr) if you have questions.    ###

cif_=gdevcif.$(OBJ)
cif.dev: $(cif_) page.dev
	$(SETPDEV) cif $(cif_)

gdevcif.$(OBJ): gdevcif.c $(PDEVH)

### ------------------------- JPEG file format ------------------------- ###

jpeg_=gdevjpeg.$(OBJ)

# RGB output
jpeg.dev: $(jpeg_) sdcte.dev page.dev
	$(SETPDEV) jpeg $(jpeg_)
	$(ADDMOD) jpeg -include sdcte

# Gray output
jpeggray.dev: $(jpeg_) sdcte.dev page.dev
	$(SETPDEV) jpeggray $(jpeg_)
	$(ADDMOD) jpeggray -include sdcte

gdevjpeg.$(OBJ): gdevjpeg.c $(stdio__h) $(PDEVH)\
 $(sdct_h) $(sjpeg_h) $(stream_h) $(strimpl_h) jpeglib.h

### ------------------------- MIFF file format ------------------------- ###
### Right now we support only 24-bit direct color, but we might add more ###
### formats in the future.                                               ###

miff_=gdevmiff.$(OBJ)

miff24.dev: $(miff_) page.dev
	$(SETPDEV) miff24 $(miff_)

gdevmiff.$(OBJ): gdevmiff.c $(PDEVH)

### --------------------------- MGR devices ---------------------------- ###
### Note: these drivers were contributed by a user: please contact       ###
###       Carsten Emde (carsten@ce.pr.net.ch) if you have questions.     ###

MGR=gdevmgr.$(OBJ) gdevpccm.$(OBJ)

gdevmgr.$(OBJ): gdevmgr.c $(PDEVH) $(gdevpccm_h) gdevmgr.h

mgrmono.dev: $(MGR) page.dev
	$(SETPDEV) mgrmono $(MGR)

mgrgray2.dev: $(MGR) page.dev
	$(SETPDEV) mgrgray2 $(MGR)

mgrgray4.dev: $(MGR) page.dev
	$(SETPDEV) mgrgray4 $(MGR)

mgrgray8.dev: $(MGR) page.dev
	$(SETPDEV) mgrgray8 $(MGR)

mgr4.dev: $(MGR) page.dev
	$(SETPDEV) mgr4 $(MGR)

mgr8.dev: $(MGR) page.dev
	$(SETPDEV) mgr8 $(MGR)

### ------------------------- PCX file formats ------------------------- ###

pcx_=gdevpcx.$(OBJ) gdevpccm.$(OBJ)

gdevpcx.$(OBJ): gdevpcx.c $(PDEVH) $(gdevpccm_h) $(gxlum_h)

pcxmono.dev: $(pcx_) page.dev
	$(SETPDEV) pcxmono $(pcx_)

pcxgray.dev: $(pcx_) page.dev
	$(SETPDEV) pcxgray $(pcx_)

pcx16.dev: $(pcx_) page.dev
	$(SETPDEV) pcx16 $(pcx_)

pcx256.dev: $(pcx_) page.dev
	$(SETPDEV) pcx256 $(pcx_)

pcx24b.dev: $(pcx_) page.dev
	$(SETPDEV) pcx24b $(pcx_)

pcxcmyk.dev: $(pcx_) page.dev
	$(SETPDEV) pcxcmyk $(pcx_)

# The 2-up PCX device is here only as an example, and for testing.
pcx2up.dev: $(LIB_MAK) $(ECHOGS_XE) gdevp2up.$(OBJ) page.dev pcx256.dev
	$(SETPDEV) pcx2up gdevp2up.$(OBJ)
	$(ADDMOD) pcx2up -include pcx256

gdevp2up.$(OBJ): gdevp2up.c $(AK)\
 $(gdevpccm_h) $(gdevprn_h) $(gxclpage_h)

### ------------------- Portable Bitmap file formats ------------------- ###
### For more information, see the pbm(5), pgm(5), and ppm(5) man pages.  ###

pxm_=gdevpbm.$(OBJ)

gdevpbm.$(OBJ): gdevpbm.c $(PDEVH) $(gscdefs_h) $(gxlum_h)

### Portable Bitmap (PBM, plain or raw format, magic numbers "P1" or "P4")

pbm.dev: $(pxm_) page.dev
	$(SETPDEV) pbm $(pxm_)

pbmraw.dev: $(pxm_) page.dev
	$(SETPDEV) pbmraw $(pxm_)

### Portable Graymap (PGM, plain or raw format, magic numbers "P2" or "P5")

pgm.dev: $(pxm_) page.dev
	$(SETPDEV) pgm $(pxm_)

pgmraw.dev: $(pxm_) page.dev
	$(SETPDEV) pgmraw $(pxm_)

# PGM with automatic optimization to PBM if this is possible.

pgnm.dev: $(pxm_) page.dev
	$(SETPDEV) pgnm $(pxm_)

pgnmraw.dev: $(pxm_) page.dev
	$(SETPDEV) pgnmraw $(pxm_)

### Portable Pixmap (PPM, plain or raw format, magic numbers "P3" or "P6")

ppm.dev: $(pxm_) page.dev
	$(SETPDEV) ppm $(pxm_)

ppmraw.dev: $(pxm_) page.dev
	$(SETPDEV) ppmraw $(pxm_)

# PPM with automatic optimization to PGM or PBM if possible.

pnm.dev: $(pxm_) page.dev
	$(SETPDEV) pnm $(pxm_)

pnmraw.dev: $(pxm_) page.dev
	$(SETPDEV) pnmraw $(pxm_)

### Portable inKmap (CMYK internally, converted to PPM=RGB at output time)

pkm.dev: $(pxm_) page.dev
	$(SETPDEV) pkm $(pxm_)

pkmraw.dev: $(pxm_) page.dev
	$(SETPDEV) pkmraw $(pxm_)

### --------------- Portable Network Graphics file format --------------- ###
### Requires libpng 0.81 and zlib 0.95 (or more recent versions).         ###
### See libpng.mak and zlib.mak for more details.                         ###

png_=gdevpng.$(OBJ) gdevpccm.$(OBJ)

gdevpng.$(OBJ): gdevpng.c $(gdevprn_h) $(gdevpccm_h) $(gscdefs_h) $(PSRC)png.h
	$(CCCP) gdevpng.c

pngmono.dev: libpng.dev $(png_) page.dev
	$(SETPDEV) pngmono  $(png_)
	$(ADDMOD) pngmono  -include libpng

pnggray.dev: libpng.dev $(png_) page.dev
	$(SETPDEV) pnggray  $(png_)
	$(ADDMOD) pnggray  -include libpng

png16.dev: libpng.dev $(png_) page.dev
	$(SETPDEV) png16  $(png_)
	$(ADDMOD) png16  -include libpng

png256.dev: libpng.dev $(png_) page.dev
	$(SETPDEV) png256  $(png_)
	$(ADDMOD) png256  -include libpng

png16m.dev: libpng.dev $(png_) page.dev
	$(SETPDEV) png16m  $(png_)
	$(ADDMOD) png16m  -include libpng

### ---------------------- PostScript image format ---------------------- ###
### These devices make it possible to print Level 2 files on a Level 1    ###
###   printer, by converting them to a bitmap in PostScript format.       ###

ps_=gdevpsim.$(OBJ)

gdevpsim.$(OBJ): gdevpsim.c $(PDEVH)

psmono.dev: $(ps_) page.dev
	$(SETPDEV) psmono $(ps_)

psgray.dev: $(ps_) page.dev
	$(SETPDEV) psgray $(ps_)

# Someday there will be RGB and CMYK variants....

### -------------------------- SGI RGB pixmaps -------------------------- ###

sgirgb_=gdevsgi.$(OBJ)

gdevsgi.$(OBJ): gdevsgi.c $(PDEVH) gdevsgi.h

sgirgb.dev: $(sgirgb_) page.dev
	$(SETPDEV) sgirgb $(sgirgb_)

### -------------------- Plain or TIFF fax encoding --------------------- ###
###    Use -sDEVICE=tiffg3 or tiffg4 and				  ###
###	  -r204x98 for low resolution output, or			  ###
###	  -r204x196 for high resolution output				  ###
###    These drivers recognize 3 page sizes: letter, A4, and B4.	  ###

gdevtifs_h=gdevtifs.h

tfax_=gdevtfax.$(OBJ)
tfax.dev: $(tfax_) cfe.dev lzwe.dev rle.dev tiffs.dev
	$(SETMOD) tfax $(tfax_)
	$(ADDMOD) tfax -include cfe lzwe rle tiffs

gdevtfax.$(OBJ): gdevtfax.c $(PDEVH)\
 $(gdevtifs_h) $(scfx_h) $(slzwx_h) $(srlx_h) $(strimpl_h)

### Plain G3/G4 fax with no header

faxg3.dev: tfax.dev
	$(SETDEV) faxg3 -include tfax

faxg32d.dev: tfax.dev
	$(SETDEV) faxg32d -include tfax

faxg4.dev: tfax.dev
	$(SETDEV) faxg4 -include tfax

### ---------------------------- TIFF formats --------------------------- ###

tiffs_=gdevtifs.$(OBJ)
tiffs.dev: $(tiffs_) page.dev
	$(SETMOD) tiffs $(tiffs_)
	$(ADDMOD) tiffs -include page

gdevtifs.$(OBJ): gdevtifs.c $(PDEVH) $(stdio__h) $(time__h) \
 $(gdevtifs_h) $(gscdefs_h) $(gstypes_h)

# Black & white, G3/G4 fax

tiffcrle.dev: tfax.dev
	$(SETDEV) tiffcrle -include tfax

tiffg3.dev: tfax.dev
	$(SETDEV) tiffg3 -include tfax

tiffg32d.dev: tfax.dev
	$(SETDEV) tiffg32d -include tfax

tiffg4.dev: tfax.dev
	$(SETDEV) tiffg4 -include tfax

# Black & white, LZW compression

tifflzw.dev: tfax.dev
	$(SETDEV) tifflzw -include tfax

# Black & white, PackBits compression

tiffpack.dev: tfax.dev
	$(SETDEV) tiffpack -include tfax

# RGB, no compression

tiffrgb_=gdevtfnx.$(OBJ)

tiff12nc.dev: $(tiffrgb_) tiffs.dev
	$(SETPDEV) tiff12nc $(tiffrgb_)
	$(ADDMOD) tiff12nc -include tiffs

tiff24nc.dev: $(tiffrgb_) tiffs.dev
	$(SETPDEV) tiff24nc $(tiffrgb_)
	$(ADDMOD) tiff24nc -include tiffs

gdevtfnx.$(OBJ): gdevtfnx.c $(PDEVH) $(gdevtifs_h)
#    Copyright (C) 1994, 1995, 1997 Aladdin Enterprises.  All rights reserved.
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

# Partial makefile, common to all Desqview/X configurations.

# This is the last part of the makefile for Desqview/X configurations.
# Since Unix make doesn't have an 'include' facility, we concatenate
# the various parts of the makefile together by brute force (in tar_cat).

# The following prevents GNU make from constructing argument lists that
# include all environment variables, which can easily be longer than
# brain-damaged system V allows.

.NOEXPORT:

# -------------------------------- Library -------------------------------- #

## The Desqview/X platform

dvx__=gp_nofb.$(OBJ) gp_dvx.$(OBJ) gp_unifs.$(OBJ) gp_dosfs.$(OBJ)
dvx_.dev: $(dvx__)
	$(SETMOD) dvx_ $(dvx__)

gp_dvx.$(OBJ): gp_dvx.c $(AK) $(string__h) $(gx_h) $(gsexit_h) $(gp_h) \
  $(time__h) $(dos__h)
	$(CCC) -D__DVX__ gp_dvx.c

# -------------------------- Auxiliary programs --------------------------- #

$(ANSI2KNR_XE): ansi2knr.c $(stdio__h) $(string__h) $(malloc__h)
	$(CC) -o ansi2knr $(CFLAGS) ansi2knr.c

$(ECHOGS_XE): echogs.c
	$(CC) -o echogs $(CFLAGS) echogs.c
	strip echogs
	coff2exe echogs
	del echogs

$(GENARCH_XE): genarch.c $(stdpre_h)
	$(CC) -o genarch genarch.c
	strip genarch
	coff2exe genarch
	del genarch

$(GENCONF_XE): genconf.c $(stdpre_h)
	$(CC) -o genconf genconf.c
	strip genconf
	coff2exe genconf
	del genconf

$(GENINIT_XE): geninit.c $(stdio__h) $(string__h)
	$(CC) -o geninit geninit.c
	strip geninit
	coff2exe geninit
	del geninit

# Construct gconfig_.h to reflect the environment.
INCLUDE=/djgpp/include
gconfig_.h: dvx-tail.mak $(ECHOGS_XE)
	echogs -w gconfig_.h -x 2f2a -s This file was generated automatically. -s -x 2a2f
	echogs -a gconfig_.h -x 23 define HAVE_SYS_TIME_H
	echogs -a gconfig_.h -x 23 define HAVE_DIRENT_H

# ----------------------------- Main program ------------------------------ #

BEGINFILES=
CCBEGIN=$(CCC) *.c

# Interpreter main program

$(GS_XE): ld.tr gs.$(OBJ) $(INT_ALL) $(LIB_ALL) $(DEVS_ALL)
	$(CP_) ld.tr _temp_
	echo $(EXTRALIBS) -lm >>_temp_
	$(CC) $(LDFLAGS) $(XLIBDIRS) -o $(GS) gs.$(OBJ) @_temp_
	strip $(GS)
	coff2exe $(GS)  
	del $(GS)  
#    Copyright (C) 1994, 1995, 1996, 1997 Aladdin Enterprises.  All rights reserved.
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

# Partial makefile common to all Unix and Desqview/X configurations.

# This is the very last part of the makefile for these configurations.
# Since Unix make doesn't have an 'include' facility, we concatenate
# the various parts of the makefile together by brute force (in tar_cat).

# Define a rule for building profiling configurations.
pg:
	make GENOPT='' CFLAGS='-pg -O $(GCFLAGS) $(XCFLAGS)' LDFLAGS='$(XLDFLAGS) -pg' XLIBS='Xt SM ICE Xext X11' CCLEAF='$(CCC)'

# Define a rule for building debugging configurations.
debug:
	make GENOPT='-DDEBUG' CFLAGS='-g -O $(GCFLAGS) $(XCFLAGS)'

# The rule for gconfigv.h is here because it is shared between Unix and
# DV/X environments.
gconfigv.h: unix-end.mak $(MAKEFILE) $(ECHOGS_XE)
	$(EXP)echogs -w gconfigv.h -x 23 define USE_ASM -x 2028 -q $(USE_ASM)-0 -x 29
	$(EXP)echogs -a gconfigv.h -x 23 define USE_FPU -x 2028 -q $(FPU_TYPE)-0 -x 29
	$(EXP)echogs -a gconfigv.h -x 23 define EXTEND_NAMES 0$(EXTEND_NAMES)

# The following rules are equivalent to what tar_cat does.
# The rm -f is so that we don't overwrite a file that `make'
# may currently be reading from.
GENERIC_MAK_LIST=$(GS_MAK) $(LIB_MAK) $(INT_MAK) $(JPEG_MAK) $(LIBPNG_MAK) $(ZLIB_MAK) $(DEVS_MAK)
UNIX_MAK_LIST=dvx-gcc.mak unixansi.mak unix-cc.mak unix-gcc.mak

unix.mak: $(UNIX_MAK_LIST)

DVX_GCC_MAK=$(VERSION_MAK) dgc-head.mak dvx-head.mak $(GENERIC_MAK_LIST) dvx-tail.mak unix-end.mak
dvx-gcc.mak: $(DVX_GCC_MAK)
	rm -f dvx-gcc.mak
	$(CAT) $(DVX_GCC_MAK) >dvx-gcc.mak

UNIXANSI_MAK=$(VERSION_MAK) ansihead.mak unixhead.mak $(GENERIC_MAK_LIST) unixtail.mak unix-end.mak
unixansi.mak: $(UNIXANSI_MAK)
	rm -f unixansi.mak
	$(CAT) $(UNIXANSI_MAK) >unixansi.mak

UNIX_CC_MAK=$(VERSION_MAK) cc-head.mak unixhead.mak $(GENERIC_MAK_LIST) unixtail.mak unix-end.mak
unix-cc.mak: $(UNIX_CC_MAK)
	rm -f unix-cc.mak
	$(CAT) $(UNIX_CC_MAK) >unix-cc.mak

UNIX_GCC_MAK=$(VERSION_MAK) gcc-head.mak unixhead.mak $(GENERIC_MAK_LIST) unixtail.mak unix-end.mak
unix-gcc.mak: $(UNIX_GCC_MAK)
	rm -f unix-gcc.mak
	$(CAT) $(UNIX_GCC_MAK) >unix-gcc.mak

# Installation

TAGS:
	etags -t *.c *.h

install: install-exec install-scripts install-data

# The sh -c in the rules below is required because Ultrix's implementation
# of sh -e terminates execution of a command if any error occurs, even if
# the command traps the error with ||.

install-exec: $(GS)
	-mkdir $(bindir)
	$(INSTALL_PROGRAM) $(GS) $(bindir)/$(GS)

install-scripts: gsnd
	-mkdir $(scriptdir)
	sh -c 'for f in gsbj gsdj gsdj500 gslj gslp gsnd bdftops font2c \
pdf2dsc pdf2ps printafm ps2ascii ps2epsi ps2pdf wftopfa ;\
	do if ( test -f $$f ); then $(INSTALL_PROGRAM) $$f $(scriptdir)/$$f; fi;\
	done'

MAN1_PAGES=gs pdf2dsc pdf2ps ps2ascii ps2epsi ps2pdf
install-data: gs.1
	-mkdir $(mandir)
	-mkdir $(man1dir)
	sh -c 'for f in $(MAN1_PAGES) ;\
	do if ( test -f $$f.1 ); then $(INSTALL_DATA) $$f.1 $(man1dir)/$$f.$(man1ext); fi;\
	done'
	-mkdir $(datadir)
	-mkdir $(gsdir)
	-mkdir $(gsdatadir)
	sh -c 'for f in Fontmap \
cbjc600.ppd cbjc800.ppd *.upp \
gs_init.ps gs_btokn.ps gs_ccfnt.ps gs_cff.ps gs_cidfn.ps gs_cmap.ps \
gs_diskf.ps gs_dpnxt.ps gs_dps.ps gs_dps1.ps gs_dps2.ps gs_epsf.ps \
gs_fonts.ps gs_kanji.ps gs_lev2.ps \
gs_pfile.ps gs_res.ps gs_setpd.ps gs_statd.ps \
gs_ttf.ps gs_typ42.ps gs_type1.ps \
gs_dbt_e.ps gs_iso_e.ps gs_ksb_e.ps gs_std_e.ps gs_sym_e.ps \
acctest.ps align.ps bdftops.ps caption.ps decrypt.ps docie.ps \
font2c.ps gslp.ps impath.ps landscap.ps level1.ps lines.ps \
markhint.ps markpath.ps \
packfile.ps pcharstr.ps pfbtogs.ps ppath.ps prfont.ps printafm.ps \
ps2ai.ps ps2ascii.ps ps2epsi.ps ps2image.ps \
quit.ps showchar.ps showpage.ps stcinfo.ps stcolor.ps \
traceimg.ps traceop.ps type1enc.ps type1ops.ps uninfo.ps unprot.ps \
viewcmyk.ps viewgif.ps viewjpeg.ps viewpcx.ps viewpbm.ps viewps2a.ps \
winmaps.ps wftopfa.ps wrfont.ps zeroline.ps \
gs_l2img.ps gs_pdf.ps \
pdf2dsc.ps \
pdf_base.ps pdf_draw.ps pdf_font.ps pdf_main.ps pdf_sec.ps pdf_2ps.ps \
gs_mex_e.ps gs_mro_e.ps gs_pdf_e.ps gs_wan_e.ps \
gs_pdfwr.ps ;\
	do if ( test -f $$f ); then $(INSTALL_DATA) $$f $(gsdatadir)/$$f; fi;\
	done'
	-mkdir $(docdir)
	sh -c 'for f in COPYING NEWS PUBLIC README \
bug-form.txt c-style.txt current.txt devices.txt drivers.txt fonts.txt \
helpers.txt hershey.txt history1.txt history2.txt history3.txt humor.txt \
install.txt language.txt lib.txt make.txt new-user.txt \
ps2epsi.txt ps2pdf.txt psfiles.txt public.txt \
unix-lpr.txt use.txt xfonts.txt ;\
	do if ( test -f $$f ); then $(INSTALL_DATA) $$f $(docdir)/$$f; fi;\
	done'
	-mkdir $(exdir)
	for f in alphabet.ps chess.ps cheq.ps colorcir.ps escher.ps golfer.ps \
grayalph.ps snowflak.ps tiger.ps waterfal.ps \
ridt91.eps ;\
	do $(INSTALL_DATA) $$f $(exdir)/$$f ;\
	done
