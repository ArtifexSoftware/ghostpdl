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
