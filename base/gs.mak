# Copyright (C) 2001-2025 Artifex Software, Inc.
# All Rights Reserved.
#
# This software is provided AS-IS with no warranty, either express or
# implied.
#
# This software is distributed under license and may not be copied,
# modified or distributed except as expressly authorized under the terms
# of the license contained in the file LICENSE in this distribution.
#
# Refer to licensing information at http://www.artifex.com or contact
# Artifex Software, Inc.,  39 Mesa Street, Suite 108A, San Francisco,
# CA 94129, USA, for further information.
#
#
# Generic makefile, common to all platforms, products, and configurations.
# The platform-specific makefiles `include' this file.

# Ghostscript makefiles cannot use default compilation rules, because
# they may place the output in (multiple) different directories.
# All compilation rules must have the form
#	<<compiler>> $(O_)<<output_file>> $(C_)<<input_file>>
# to cope with the divergent syntaxes of the various compilers.
# Spaces must appear where indicated, and nowhere else; in particular,
# there must be no space between $(O_) and the output file name.

# The platform-specific makefiles define the following symbols:
#	GS - the name of the executable (without the extension, if any).
#	GS_LIB_DEFAULT - the default directory/ies for searching for the
#	    initialization and font files at run time.
#	GS_DEV_DEFAULT - array of default device names, in order of
#	    preference. If empty the first DEVICE_DEV will be used.
#	GS_CACHE_DIR - the default directory for caching data between
#	    ghostscript invocations.
#	SEARCH_HERE_FIRST - the default setting of -P (whether or not to
#	    look for files in the current directory first).
#	GS_DOCDIR - the directory where documentation will be available
#	    at run time.
#	FTSRCDIR - the directory where there the FreeType library
#	    source code is stored, relative to the source directory.
#	JSRCDIR - the directory where the IJG JPEG library source code
#	    is stored (at compilation time).
#	PNGSRCDIR - the same for libpng.
#	ZSRCDIR - the same for zlib.
#	SHARE_FT - normally 0; if set to 1, asks the linker to use
#	    and existing compiled freetype library instead of compiling
#	    in the source code availabel in FTSRCDIR.
#	SHARE_JPEG - normally 0; if set to 1, asks the linker to use
#	    an existing compiled libjpeg (-ljpeg) instead of compiling and
#	    linking libjpeg explicitly.  (We strongly recommend against
#	    doing this: see Make.htm details.)
#	JPEG_NAME - the name of the shared library, currently always
#	    jpeg (libjpeg, -lpjeg).
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
#	JBIG2_LIB - choice of which jbig2 implementation to use
#	SHARE_JBIG2 - normally 0; if set to 1, asks the linker to use
#	    an existing complied libjbig2dec instead of compiling and linking
#	    in from a local copy of the source
#	JBIG2SRCDIR - the name of the jbig2dec library source directory
#	    typically 'jbig2dec' or 'jbig2dec-/version/'
#	JPX_LIB - choice of which jpeg2k implementation to use
#	SHARE_JPX - If set to 1, asks the linker to use an existing
#	    complied jpeg2k library. If set to 0, asks to compile and
#	    link from a local copy of the source using our custom
#	    makefile.
#	JPXSRCDIR - the name of the jpeg2k library source directory
#	    e.g. 'openjpeg'
#	JPX_CFLAGS - any platform-specific flags that are required
#	    to properly compile in the jpeg2k library source
#	SHARE_LCMS - If set to 1, asks the linker to use a separately
#	    compiled lcms library. If set to 0, the build will compile
#	    in the library source found in LCMSSRCDIR
#	LCMSSRCDIR - the name of the lcms library source directory
#	    e.g. 'lcms' or 'lcms-<version>'
#	DEVICE_DEVS - the devices to include in the executable.
#	    See devs.mak for details.
#	DEVICE_DEVS1...DEVICE_DEVS21 - additional devices, if the definition
#	    of DEVICE_DEVS doesn't fit on one line.  See devs.mak for details.
#	FEATURE_DEVS - what features to include in the executable.
#	    Normally this is one of:
#		    $(PSD)psl1.dev - a PostScript Level 1 language interpreter.
#		    $(PSD)psl2.dev - a PostScript Level 2 language interpreter.
#		    $(PSD)psl3.dev - a PostScript LanguageLevel 3 language
#		      interpreter.
#	      and/or
#		    pdf - a PDF 1.2 interpreter.
#	    psl3 includes everything in psl2, and psl2 includes everything
#	      in psl1.  For backward compatibility, level1 is a synonym for
#	      psl1, and level2 is a synonym for psl2.
#	    The remaining features are of interest primarily to developers
#	      who want to "mix and match" features to create custom
#	      configurations:
#		    btoken - support for binary token encodings.
#			Included automatically in the dps and psl2 features.
#		    cidfont - (currently partial) support for CID-keyed fonts.
#		    color - support for the Level 1 CMYK color extensions.
#			Included automatically in the dps and psl2 features.
#		    compfont - support for composite (type 0) fonts.
#			Included automatically in the psl2 feature.
#		    dct - support for DCTEncode/Decode filters.
#			Included automatically in the psl2 feature.
#                   diskn - support for %disk IODevice emulation. Adds support
#                       for %disk0 thru %disk9. Use requires setting the /Root
#                       paramter for each %disk (see Language.htm).
#		    dps - (partial) support for Display PostScript extensions:
#			see Language.htm for details.
#		    epsf - support for recognizing and skipping the binary
#			header of MS-DOS EPSF files.
#		    filter - support for Level 2 filters (other than eexec,
#			ASCIIHexEncode/Decode, NullEncode, PFBDecode,
#			RunLengthEncode/Decode, and SubFileDecode, which are
#			always included, and DCTEncode/Decode,
#			which are separate).
#			Included automatically in the psl2 feature.
#		    fzlib - support for zlibEncode/Decode filters.
#		    ttfont - support for TrueType fonts.
#		    type1 - support for Type 1 fonts and eexec;
#			normally included automatically in all configurations.
#		    type32 - support for Type 32 (downloaded bitmap) fonts.
#			Included automatically in the psl2 feature.
#		    type42 - support for Type 42 (embedded TrueType) fonts.
#			Included automatically in the psl2 feature.
#                   fapi - Font API (3d party font renderer interface).
#		There are quite a number of other sub-features that can be
#		selectively included in or excluded from a configuration,
#		but the above are the ones that are most likely to be of
#		interest.
#	COMPILE_INITS - normally 1; compiles the PostScript language
#	    initialization files (gs_init.ps et al) and Resource/* tree
#	    into the executable, eliminating the need for these files
#	    to be present at run time. Files will be placed in the
#	    %rom% device.
#	BAND_LIST_STORAGE - normally file; if set to memory, stores band
#	    lists in memory (with compression if needed).
#	BAND_LIST_COMPRESSOR - normally zlib: selects the compression method
#	    to use for band lists in memory.
#	FILE_IMPLEMENTATION - normally stdio; if set to fd, uses file
#	    descriptors instead of buffered stdio for file I/O; if set to
#	    both, provides both implementations with different procedure
#	    names for the fd-based implementation (see sfxfd.c for
#	    more information).
#
# It is very unlikely that anyone would want to edit the remaining
#   symbols, but we describe them here for completeness:
#	GS_INIT - the name of the initialization file for the interpreter,
#		normally gs_init.ps.
#	GSPLATFORM - a "device" name for the platform, so that platforms can
#		add various kinds of resources like devices and features.
#	CMD - the suffix for shell command files (e.g., null or .bat).
#		(This is only needed in a few places.)
#	D - the directory separator character (\ for MS-DOS, / for Unix).
#	O_ - the string for specifying the output file from the C compiler
#		(-o for MS-DOS, -o ./ for Unix).
#	OBJ - the extension for relocatable object files (e.g., o or obj).
#	XE - the extension for executable files (e.g., null or .exe).
#	XEAUX - the extension for the executable files (e.g., null or .exe)
#		for the utility programs (those compiled with CCAUX).
#	BEGINFILES - the list of additional files that `make clean' should
#		delete.
#	CCAUX - the C invocation for auxiliary programs (echogs, genarch,
#		genconf, gendev, genht, geninit).
#	CC_ - the C invocation for normal compilation.
#	CCD - the C invocation for files that store into frame buffers or
#		device registers.  Needed because some optimizing compilers
#		will eliminate necessary stores.
#	CCINT - the C invocation for compiling the main interpreter module,
#		normally the same as CC_: this is needed because the
#		Borland compiler generates *worse* code for this module
#		(but only this module) when optimization (-O) is turned on.
#	AK - if a particular platform requires any programs or data files
#		to be built before compiling the source code, AK must list
#		them.
#	EXP - the prefix for invoking an executable program in a specified
#		directory (MCR on OpenVMS, null on all other platforms).
#	SH - the shell for scripts (null on MS-DOS, sh on Unix).
#	CONFILES - the arguments for genconf to generate the appropriate
#		linker control files (various).
#	CONFLDTR - the genconf switch for generating ld_tr.
#	CP_ - the command for copying one file to another.  Because of
#		limitations in the MS-DOS/MS Windows environment, the
#		second argument must be either '.' (in which case the
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
#	gconfigd.h - this is used for configuration-specific definitions
#	    such as paths that must be defined by all top-level makefiles.

#**************** PATCHES
FTGENDIR=$(GLGENDIR)
FTOBJDIR=$(GLOBJDIR)
JGENDIR=$(GLGENDIR)
JOBJDIR=$(GLOBJDIR)
PNGGENDIR=$(GLGENDIR)
PNGOBJDIR=$(GLOBJDIR)
ZGENDIR=$(GLGENDIR)
ZOBJDIR=$(GLOBJDIR)
ZAUXDIR=$(AUXDIR)
BROTLIGENDIR=$(GLGENDIR)
BROTLIOBJDIR=$(GLOBJDIR)
BROTLIAUXDIR=$(AUXDIR)
TIFFGENDIR=$(GLGENDIR)
TIFFOBJDIR=$(GLOBJDIR)
JBIG2GENDIR=$(GLGENDIR)
JBIG2OBJDIR=$(GLOBJDIR)
JPXGENDIR=$(GLGENDIR)
JPXOBJDIR=$(GLOBJDIR)
LCMSGENDIR=$(GLGENDIR)
LCMSOBJDIR=$(GLOBJDIR)
LCMS2GENDIR=$(GLGENDIR)
LCMS2OBJDIR=$(GLOBJDIR)
EXPATGENDIR=$(GLGENDIR)
EXPATOBJDIR=$(GLOBJDIR)
IJSGENDIR=$(GLGENDIR)
IJSOBJDIR=$(GLOBJDIR)
LCUPSGENDIR=$(GLGENDIR)
LCUPSOBJDIR=$(GLOBJDIR)
LCUPSIGENDIR=$(GLGENDIR)
LCUPSIOBJDIR=$(GLOBJDIR)
CALOBJDIR=$(GLOBJDIR)

#**************** END PATCHES

GSGEN=$(GLGENDIR)$(D)
GSOBJ=$(GLOBJDIR)$(D)
# All top-level makefiles define DD.
#DD=$(GLGEN)

# Define the name of this makefile.
GS_MAK=$(GLSRCDIR)$(D)gs.mak $(TOP_MAKEFILES)

# Define the names of the executables.
GS_XE=$(BINDIR)$(D)$(GS)$(XE)
GPCL_XE=$(BINDIR)$(D)$(PCL)$(XE)
GXPS_XE=$(BINDIR)$(D)$(XPS)$(XE)
GPDF_XE=$(BINDIR)$(D)$(PDF)$(XE)
GPDL_XE=$(BINDIR)$(D)$(GPDL)$(XE)

AUX=$(AUXDIR)$(D)
ECHOGS_XE=$(AUX)echogs$(XEAUX)
GENARCH_XE=$(AUX)genarch$(XEAUX)
GENCONF_XE=$(AUX)genconf$(XEAUX)
GENDEV_XE=$(AUX)gendev$(XEAUX)
GENHT_XE=$(AUX)genht$(XEAUX)
MKROMFS_XE=$(AUX)mkromfs$(XEAUX)
PACKPS_XE=$(AUX)packps$(XEAUX)

# Define the names of the generated header files.
# gconfig*.h and gconfx*.h are generated dynamically.
gconfig_h=$(GLGENDIR)$(D)gconfig.h
gconfxx_h=$(GLGENDIR)$(D)gconfxx.h
gconfigf_h=$(GLGENDIR)$(D)gconfxc.h
gconfigd_h=$(GLGENDIR)$(D)gconfigd.h

iconfxx_h=$(GLGENDIR)$(D)iconfxx.h
iconfig_h=$(GLGENDIR)$(D)iconfig.h

all default : $(GS_XE) $(PCL_XPS_PDL_TARGETS) $(GS_SHARED_OBJS) $(MAKEDIRSTOP) $(GS_MAK) $(MAKEDIRS)
	$(NO_OP)

experimental: all
	$(NO_OP)

# the distclean and maintainer-clean targets (if any)
# are the responsibility of the platform-specific
# makefiles. We only handle the internal build system
# apparatus here.
realclean : clean
	$(NO_OP)

cleansub : mostlyclean
	$(RM_) $(GSGEN)arch.h
	$(RM_) $(GS_XE)
	$(RM_) $(GS_SHARED_OBJS)
	$(RMN_) -r $(BINDIR) $(GLGENDIR) $(GLOBJDIR) $(PSGENDIR) $(PSOBJDIR)

clean : cleansub  $(PCL_XPS_CLEAN)
	$(NO_OP)
#****** FOLLOWING IS WRONG, NEEDS TO BE PER-SUBSYSTEM ******
mostlyclean : config-clean
	$(RMN_) $(GSOBJ)*.$(OBJ) $(GSOBJ)*.a $(GSOBJ)core $(GSOBJ)gmon.out
	$(RMN_) $(GSGEN)deflate.h $(GSGEN)zutil.h
	$(RMN_) $(GSGEN)gconfig*.c $(GSGEN)gscdefs*.c $(GSGEN)iconfig*.c
	$(RMN_) $(GSGEN)_temp_* $(GSGEN)_temp_*.* $(GSOBJ)*.map $(GSOBJ)*.sym
	$(RMN_) $(GENARCH_XE) $(GENCONF_XE) $(GENDEV_XE) $(GENHT_XE)
	$(RMN_) $(ECHOGS_XE)
	$(RMN_) $(GSGEN)gs_init.ps $(BEGINFILES)
	$(RMN_) $(MKROMFS_XE)
	$(RMN_) $(MKROMFS_XE)_0
	$(RMN_) $(MKROMFS_XE)_1
	$(RMN_) $(PACKPS_XE)
	$(RMN_) $(GSGEN)gsromfs1.c $(GSGEN)gsromfs1_.c $(GSGEN)gsromfs1_1.c
	$(RMN_) $(AUX)*.$(OBJ) $(AUX)gscdefs*.c

# Remove only configuration-dependent information.
#****** FOLLOWING IS WRONG, NEEDS TO BE PER-SUBSYSTEM ******
config-clean :
	$(RMN_) $(GSGEN)*.dev $(GSGEN)devs*.tr $(GSGEN)gconfig*.h
	$(RMN_) $(GSGEN)gconfx*.h $(GSGEN)j*.h $(GSGEN)tif*.h
	$(RMN_) $(GSGEN)c*.tr $(GSGEN)o*.tr $(GSGEN)l*.tr

# Macros for constructing the *.dev files that describe features and
# devices.
SETDEV=$(EXP)$(ECHOGS_XE) -e .dev -w- -l-dev -b -s -l-obj
SETPDEV=$(EXP)$(ECHOGS_XE) -e .dev -w- -l-dev -b -s -l-include -l$(GLGENDIR)$(D)page -l-obj
SETDEV2=$(EXP)$(ECHOGS_XE) -e .dev -w- -l-dev2 -b -s -l-obj
SETPDEV2=$(EXP)$(ECHOGS_XE) -e .dev -w- -l-dev2 -b -s -l-include -l$(GLGENDIR)$(D)page -l-obj
SETMOD=$(EXP)$(ECHOGS_XE) -e .dev -w- -l-obj
ADDMOD=$(EXP)$(ECHOGS_XE) -e .dev -a- $(NULL)
SETCOMP=$(EXP)$(ECHOGS_XE) -e .dev -w- -l-comp
ADDCOMP=$(EXP)$(ECHOGS_XE) -e .dev -a- -l-comp

IJSI_=$(IJSSRCDIR)
IJSF_=
# Currently there is no option for sharing ijs.
IJSCF_=
JI_=$(JSRCDIR)
JF_=
JCF_=$(D_)SHARE_JPEG=$(SHARE_JPEG)$(_D)
ZI_=$(ZSRCDIR)
BROTLII_=$(BROTLISRCDIR)$(D)c$(D)include
PI_=$(PNGSRCDIR) $(II)$(ZI_)
# PF_ should include PNG_USE_CONST, but this doesn't work.
#PF_=-DPNG_USE_CONST
TI_=$(TIFFSRCDIR)$(D)libtiff $(II)$(TIFFCONFDIR)$(D)libtiff $(II)$(JGENDIR) $(II)$(ZI_)
PF_=
PCF_=$(D_)SHARE_LIBPNG=$(SHARE_LIBPNG)$(_D)
ZF_=
BROTLIF_=
ZCF_=$(D_)SHARE_ZLIB=$(SHARE_ZLIB)$(_D)
JB2I_=$(JBIG2SRCDIR)
JB2CF_=$(JBIG2_CFLAGS)
LDF_JB2I_=$(JBIG2SRCDIR)$(D)source$(D)libraries
LWF_JPXI_=$(JPXSRCDIR)$(D)library$(D)source
JPXCF_=$(JPX_CFLAGS)
JPX_OPENJPEG_I_=$(JPXSRCDIR)$(D)src$(D)lib$(D)openjp2

######################## How to define new 'features' #######################
#
# One defines new 'features' exactly like devices (see devs.mak for details).
# For example, one would define a feature abc by adding the following to
# gs.mak:
#
#	abc_=abc1.$(OBJ) ...
#	$(PSD)abc.dev : $(GS_MAK) $(ECHOGS_XE) $(abc_)
#		$(SETMOD) $(PSD)abc $(abc_)
#		$(ADDMOD) $(PSD)abc -obj ... [if needed]
#		$(ADDMOD) $(PSD)abc -oper ... [if appropriate]
#		$(ADDMOD) $(PSD)abc -ps ... [if appropriate]
#
# Use PSD for interpreter-related features, GLD for library-related.
# If the abc feature requires the presence of some other features jkl and
# pqr, then the rules must look like this:
#
#	abc_=abc1.$(OBJ) ...
#	$(PSD)abc.dev : $(GS_MAK) $(ECHOGS_XE) $(abc_)\
#	 $(PSD)jkl.dev $(PSD)pqr.dev
#		$(SETMOD) $(PSD)abc $(abc_)
#		...
#		$(ADDMOD) $(PSD)abc -include $(PSD)jkl
#		$(ADDMOD) $(PSD)abc -include $(PSD)pqr

# --------------------- Configuration-dependent files --------------------- #

# gconfig.h shouldn't have to depend on DEVS_ALL, but that would
# involve rewriting gsconfig to only save the device name, not the
# contents of the <device>.dev files.
# FEATURE_DEVS must precede DEVICE_DEVS so that devices can override
# features in obscure cases.

# FEATURE_DEVS_EXTRA and DEVICE_DEVS_EXTRA are explicitly reserved
# to be set from the command line.

GSPLAT_DEVS_ALL=$(GLGENDIR)$(D)$(GSPLATFORM).dev

DEVICE_DEVS_ALL=$(DEVICE_DEVS) $(DEVICE_DEVS1) \
 $(DEVICE_DEVS2) $(DEVICE_DEVS3) $(DEVICE_DEVS4) $(DEVICE_DEVS5) \
 $(DEVICE_DEVS6) $(DEVICE_DEVS7) $(DEVICE_DEVS8) $(DEVICE_DEVS9) \
 $(DEVICE_DEVS10) $(DEVICE_DEVS11) $(DEVICE_DEVS12) $(DEVICE_DEVS13) \
 $(DEVICE_DEVS14) $(DEVICE_DEVS15) $(DEVICE_DEVS16) $(DEVICE_DEVS17) \
 $(DEVICE_DEVS18) $(DEVICE_DEVS19) $(DEVICE_DEVS20) $(DEVICE_DEVS21) \
 $(DEVICE_DEVS_EXTRA)

PSI_DEVS_ALL=$(GSPLAT_DEVS_ALL) \
 $(PSI_FEATURE_DEVS) \
 $(PSD)iapi.dev \
 $(FEATURE_DEVS) \
 $(FEATURE_DEVS_EXTRA) \
 $(DEVICE_DEVS_ALL)

PCL_DEVS_ALL=$(GSPLAT_DEVS_ALL) \
 $(FEATURE_DEVS) \
 $(FEATURE_DEVS_EXTRA) \
 $(DEVICE_DEVS_ALL)

XPS_DEVS_ALL=$(GSPLAT_DEVS_ALL) \
 $(FEATURE_DEVS) \
 $(FEATURE_DEVS_EXTRA) \
 $(DEVICE_DEVS_ALL)

PDF_DEVS_ALL=$(GSPLAT_DEVS_ALL) \
 $(FEATURE_DEVS) \
 $(FEATURE_DEVS_EXTRA) \
 $(DEVICE_DEVS_ALL)

DEVS_ALL=$(GLGENDIR)$(D)$(GSPLATFORM).dev\
 $(FEATURE_DEVS_EXTRA) \
 $(DEVICE_DEVS) $(DEVICE_DEVS1) \
 $(DEVICE_DEVS2) $(DEVICE_DEVS3) $(DEVICE_DEVS4) $(DEVICE_DEVS5) \
 $(DEVICE_DEVS6) $(DEVICE_DEVS7) $(DEVICE_DEVS8) $(DEVICE_DEVS9) \
 $(DEVICE_DEVS10) $(DEVICE_DEVS11) $(DEVICE_DEVS12) $(DEVICE_DEVS13) \
 $(DEVICE_DEVS14) $(DEVICE_DEVS15) $(DEVICE_DEVS16) $(DEVICE_DEVS17) \
 $(DEVICE_DEVS18) $(DEVICE_DEVS19) $(DEVICE_DEVS20) $(DEVICE_DEVS21) \
 $(DEVICE_DEVS_EXTRA)


$(GLGENDIR)$(D)fdevs.tr: $(GS_MAK) $(ECHOGS_XE) $(GSPLAT_DEVS_ALL) $(FEATURE_DEVS) $(MAKEDIRS)
	$(EXP)$(ECHOGS_XE) -w $(GLGENDIR)$(D)fdevs.tr - -include $(GLGENDIR)$(D)$(GSPLATFORM)
	$(EXP)$(ECHOGS_XE) -a $(GLGENDIR)$(D)fdevs.tr -+ $(FEATURE_DEVS)
	$(EXP)$(ECHOGS_XE) -a $(GLGENDIR)$(D)fdevs.tr -+ $(FEATURE_DEVS_EXTRA)

$(GLGENDIR)$(D)devdevs.tr: $(GS_MAK) $(ECHOGS_XE) $(DEVICE_DEVS_ALL) $(MAKEDIRS)
	$(EXP)$(ECHOGS_XE) -w $(GLGENDIR)$(D)devdevs.tr -+ $(DEVICE_DEVS)
	$(EXP)$(ECHOGS_XE) -a $(GLGENDIR)$(D)devdevs.tr -+ $(DEVICE_DEVS1)
	$(EXP)$(ECHOGS_XE) -a $(GLGENDIR)$(D)devdevs.tr -+ $(DEVICE_DEVS2)
	$(EXP)$(ECHOGS_XE) -a $(GLGENDIR)$(D)devdevs.tr -+ $(DEVICE_DEVS3)
	$(EXP)$(ECHOGS_XE) -a $(GLGENDIR)$(D)devdevs.tr -+ $(DEVICE_DEVS4)
	$(EXP)$(ECHOGS_XE) -a $(GLGENDIR)$(D)devdevs.tr -+ $(DEVICE_DEVS5)
	$(EXP)$(ECHOGS_XE) -a $(GLGENDIR)$(D)devdevs.tr -+ $(DEVICE_DEVS6)
	$(EXP)$(ECHOGS_XE) -a $(GLGENDIR)$(D)devdevs.tr -+ $(DEVICE_DEVS7)
	$(EXP)$(ECHOGS_XE) -a $(GLGENDIR)$(D)devdevs.tr -+ $(DEVICE_DEVS8)
	$(EXP)$(ECHOGS_XE) -a $(GLGENDIR)$(D)devdevs.tr -+ $(DEVICE_DEVS9)
	$(EXP)$(ECHOGS_XE) -a $(GLGENDIR)$(D)devdevs.tr -+ $(DEVICE_DEVS10)
	$(EXP)$(ECHOGS_XE) -a $(GLGENDIR)$(D)devdevs.tr -+ $(DEVICE_DEVS11)
	$(EXP)$(ECHOGS_XE) -a $(GLGENDIR)$(D)devdevs.tr -+ $(DEVICE_DEVS12)
	$(EXP)$(ECHOGS_XE) -a $(GLGENDIR)$(D)devdevs.tr -+ $(DEVICE_DEVS13)
	$(EXP)$(ECHOGS_XE) -a $(GLGENDIR)$(D)devdevs.tr -+ $(DEVICE_DEVS14)
	$(EXP)$(ECHOGS_XE) -a $(GLGENDIR)$(D)devdevs.tr -+ $(DEVICE_DEVS15)
	$(EXP)$(ECHOGS_XE) -a $(GLGENDIR)$(D)devdevs.tr -+ $(DEVICE_DEVS16)
	$(EXP)$(ECHOGS_XE) -a $(GLGENDIR)$(D)devdevs.tr -+ $(DEVICE_DEVS17)
	$(EXP)$(ECHOGS_XE) -a $(GLGENDIR)$(D)devdevs.tr -+ $(DEVICE_DEVS18)
	$(EXP)$(ECHOGS_XE) -a $(GLGENDIR)$(D)devdevs.tr -+ $(DEVICE_DEVS19)
	$(EXP)$(ECHOGS_XE) -a $(GLGENDIR)$(D)devdevs.tr -+ $(DEVICE_DEVS20)
	$(EXP)$(ECHOGS_XE) -a $(GLGENDIR)$(D)devdevs.tr -+ $(DEVICE_DEVS21)
	$(EXP)$(ECHOGS_XE) -a $(GLGENDIR)$(D)devdevs.tr -+ $(DEVICE_DEVS_EXTRA)
	$(EXP)$(ECHOGS_XE) -a $(GLGENDIR)$(D)devdevs.tr - $(GLGENDIR)$(D)libcore

devs_tr=$(GLGENDIR)$(D)devs.tr
psdevs_tr=$(GLGENDIR)$(D)psdevs.tr

$(devs_tr) : $(GS_MAK) $(ECHOGS_XE) $(GLGENDIR)$(D)fdevs.tr $(GLGENDIR)$(D)devdevs.tr $(MAKEDIRS)
	$(EXP)$(ECHOGS_XE) -w $(devs_tr) -n -R $(GLGENDIR)$(D)fdevs.tr
	$(EXP)$(ECHOGS_XE) -a $(devs_tr) -+ ""
	$(EXP)$(ECHOGS_XE) -a $(devs_tr) -n -R $(GLGENDIR)$(D)devdevs.tr

# GCONFIG_EXTRAS can be set on the command line.
# Note that it consists of arguments for echogs, i.e.,
# it isn't just literal text.
GCONFIG_EXTRAS=

ld_tr=$(GLGENDIR)$(D)ld.tr
$(ld_tr) : \
  $(GS_MAK) $(GLSRCDIR)$(D)version.mak $(GENCONF_XE) $(ECHOGS_XE) $(devs_tr)\
  $(GLGENDIR)$(D)libcore.dev $(MAKEDIRS)
	$(EXP)$(GENCONF_XE) $(devs_tr) -h $(gconfxx_h) $(CONFILES) $(CONFLDTR) $(ld_tr)
	$(EXP)$(ECHOGS_XE) -a $(gconfxx_h) $(GCONFIG_EXTRAS)

gsnoapi_tr=$(GLGENDIR)$(D)gsnoapi.tr
gs_tr=$(GLGENDIR)$(D)gs.tr
igs_tr=$(GLGENDIR)$(D)igs.tr
gsld_tr=$(GLGENDIR)$(D)gsld.tr
gsnoapild_tr=$(GLGENDIR)$(D)gsnoapild.tr
$(gsnoapi_tr): $(GS_MAK) $(GLSRCDIR)$(D)version.mak $(GENCONF_XE) $(ECHOGS_XE) $(ld_tr) $(devs_tr) $(PSI_DEVS_ALL)\
 $(GLGENDIR)$(D)libcore.dev $(MAKEDIRS)
	$(EXP)$(ECHOGS_XE) -w $(igs_tr) - -include $(PSI_FEATURE_DEVS)
	$(EXP)$(GENCONF_XE) $(igs_tr) -h $(iconfxx_h) $(CONFILES) $(CONFLDTR) $(gsld_tr)
	$(EXP)$(ECHOGS_XE) -w $(iconfig_h) -R $(iconfxx_h)
	$(EXP)$(ECHOGS_XE) -w $(gsnoapi_tr) -R $(devs_tr)
	$(EXP)$(ECHOGS_XE) -a $(gsnoapi_tr) -R $(igs_tr)
	$(EXP)$(GENCONF_XE) $(gsnoapi_tr) -h $(GLGENDIR)$(D)unused.h $(CONFILES) $(CONFLDTR) $(gsnoapild_tr)

$(gs_tr): $(gsnoapi_tr)
	$(EXP)$(ECHOGS_XE) -w $(gs_tr) -R $(gsnoapi_tr)
	$(EXP)$(ECHOGS_XE) -a $(gs_tr) - -include $(PSD)iapi.dev
	$(EXP)$(GENCONF_XE) $(gs_tr) -h $(GLGENDIR)$(D)unused.h $(CONFILES) $(CONFLDTR) $(gsld_tr)


pcl_tr=$(GLGENDIR)$(D)pcl.tr
ipcl_tr=$(GLGENDIR)$(D)ipcl.tr
pclld_tr=$(GLGENDIR)$(D)pclld.tr
$(pcl_tr): $(GS_MAK) $(GLSRCDIR)$(D)version.mak $(GENCONF_XE) $(ECHOGS_XE) $(ld_tr) $(devs_tr) $(PCL_DEVS_ALL) \
                                             $(devs_tr) $(PCL_FEATURE_DEVS) $(GLGENDIR)$(D)libcore.dev $(MAKEDIRS)
	$(EXP)$(ECHOGS_XE) -w $(ipcl_tr) - -include $(PCL_FEATURE_DEVS)
	$(EXP)$(ECHOGS_XE) -w $(pcl_tr) -R $(devs_tr)
	$(EXP)$(ECHOGS_XE) -a $(pcl_tr) -R $(ipcl_tr)
	$(EXP)$(GENCONF_XE) $(pcl_tr) -h $(GLGENDIR)$(D)unused.h $(CONFILES) $(CONFLDTR) $(pclld_tr)

xps_tr=$(GLGENDIR)$(D)xps.tr
ixps_tr=$(GLGENDIR)$(D)ixps.tr
xpsld_tr=$(GLGENDIR)$(D)xpsld.tr
$(xps_tr): $(GS_MAK) $(GLSRCDIR)$(D)version.mak $(GENCONF_XE) $(ECHOGS_XE) $(ld_tr) $(devs_tr) $(XPS_DEVS_ALL) \
                                             $(devs_tr) $(XPS_FEATURE_DEVS) $(GLGENDIR)$(D)libcore.dev $(MAKEDIRS)
	$(EXP)$(ECHOGS_XE) -w $(ixps_tr) - -include $(XPS_FEATURE_DEVS)
	$(EXP)$(ECHOGS_XE) -w $(xps_tr) -R $(devs_tr)
	$(EXP)$(ECHOGS_XE) -a $(xps_tr) -R $(ixps_tr)
	$(EXP)$(GENCONF_XE) $(xps_tr) -h $(GLGENDIR)$(D)unused.h $(CONFILES) $(CONFLDTR) $(xpsld_tr)

pdf_tr=$(GLGENDIR)$(D)pdf.tr
ipdf_tr=$(GLGENDIR)$(D)ipdf.tr
pdfld_tr=$(GLGENDIR)$(D)pdfld.tr
$(pdf_tr): $(GS_MAK) $(GLSRCDIR)$(D)version.mak $(GENCONF_XE) $(ECHOGS_XE) $(ld_tr) $(devs_tr) $(PDF_DEVS_ALL) \
                                             $(devs_tr) $(PDF_FEATURE_DEVS) $(GLGENDIR)$(D)libcore.dev $(MAKEDIRS)
	$(EXP)$(ECHOGS_XE) -w $(ipdf_tr) - -include $(PDF_FEATURE_DEVS)
	$(EXP)$(ECHOGS_XE) -w $(pdf_tr) -R $(devs_tr)
	$(EXP)$(ECHOGS_XE) -a $(pdf_tr) -R $(ipdf_tr)
	$(EXP)$(GENCONF_XE) $(pdf_tr) -h $(GLGENDIR)$(D)unused.h $(CONFILES) $(CONFLDTR) $(pdfld_tr)

gpdl_tr=$(GLGENDIR)$(D)gpdl.tr
igpdl_tr=$(GLGENDIR)$(D)igpdl.tr
gpdlld_tr=$(GLGENDIR)$(D)gpdlld.tr
$(gpdl_tr): $(GS_MAK) $(GLSRCDIR)$(D)version.mak $(GENCONF_XE) $(ECHOGS_XE) $(ld_tr) $(devs_tr) $(XPS_DEVS_ALL) \
		$(devs_tr) $(PSI_DEVS_ALL) $(PCL_FEATURE_DEVS) $(XPS_FEATURE_DEVS) $(PDF_FEATURE_DEVS) \
		$(GLGENDIR)$(D)libcore.dev $(MAKEDIRS)
	$(EXP)$(ECHOGS_XE) -w $(igpdl_tr) - -include $(PSI_FEATURE_DEVS)
	$(EXP)$(GENCONF_XE) $(igpdl_tr) -h $(iconfxx_h) $(CONFILES) $(CONFLDTR) $(gpdlld_tr)
	$(EXP)$(ECHOGS_XE) -w $(iconfig_h) -R $(iconfxx_h)
	$(EXP)$(ECHOGS_XE) -w $(gpdl_tr) -R $(devs_tr)
	$(EXP)$(ECHOGS_XE) -a $(gpdl_tr) -R $(igpdl_tr)
	$(EXP)$(ECHOGS_XE) -a $(gpdl_tr) - -include $(PCL_FEATURE_DEVS) $(XPS_FEATURE_DEVS) $(PDF_FEATURE_DEVS)
	$(EXP)$(GENCONF_XE) $(gpdl_tr) -h $(GLGENDIR)$(D)unused2.h $(CONFILES) $(CONFLDTR) $(gpdlld_tr)

$(gconfxx_h) : $(ld_tr)
	$(NO_OP)

$(gconfig_h) : $(gconfxx_h)
	$(RM_) $(gconfig_h)
	$(CP_) $(gconfxx_h) $(gconfig_h)

# The line above is an empty command; don't delete.

# save our set of makefile variables that are defined in every build (paths, etc.)
$(gconfigd_h) : $(ECHOGS_XE) $(GS_MAK) $(GLSRCDIR)$(D)version.mak $(MAKEDIRS)
	$(EXP)$(ECHOGS_XE) -w $(gconfigd_h) -x 23 define -s -u GS_LIB_DEFAULT -x 2022 $(GS_LIB_DEFAULT) -x 22
	$(EXP)$(ECHOGS_XE) -a $(gconfigd_h) -x 23 define -s -u GS_DEV_DEFAULT -x 2022 $(GS_DEV_DEFAULT) -x 22
	$(EXP)$(ECHOGS_XE) -a $(gconfigd_h) -x 23 define -s -u GS_CACHE_DIR -x 2022 $(GS_CACHE_DIR) -x 22
	$(EXP)$(ECHOGS_XE) -a $(gconfigd_h) -x 23 define -s -u SEARCH_HERE_FIRST -s $(SEARCH_HERE_FIRST)
	$(EXP)$(ECHOGS_XE) -a $(gconfigd_h) -x 23 define -s -u GS_DOCDIR -x 2022 $(GS_DOCDIR) -x 22
	$(EXP)$(ECHOGS_XE) -a $(gconfigd_h) -x 23 define -s -u GS_INIT -x 2022 $(GS_INIT) -x 22
	$(EXP)$(ECHOGS_XE) -a $(gconfigd_h) -x 23 define -s -u GS_REVISION -s $(GS_REVISION)
	$(EXP)$(ECHOGS_XE) -a $(gconfigd_h) -x 23 define -s -u GS_REVISIONDATE -s $(GS_REVISIONDATE)

obj_tr=$(GLGENDIR)$(D)obj.tr
$(obj_tr) : $(gs_tr)
	$(EXP)$(GENCONF_XE) $(gs_tr) -h $(GLGENDIR)$(D)unused.h $(CONFILES) -o $(obj_tr)


pclobj_tr=$(GLGENDIR)$(D)pclobj.tr
$(pclobj_tr) : $(pcl_tr)
	$(EXP)$(GENCONF_XE) $(pcl_tr) -h $(GLGENDIR)$(D)unused.h $(CONFILES) -o $(pclobj_tr)


xpsobj_tr=$(GLGENDIR)$(D)xpsobj.tr
$(xpsobj_tr) : $(xps_tr)
	$(EXP)$(GENCONF_XE) $(xps_tr) -h $(GLGENDIR)$(D)unused.h $(CONFILES) -o $(xpsobj_tr)

pdfobj_tr=$(GLGENDIR)$(D)pdfobj.tr
$(pdfobj_tr) : $(pdf_tr)
	$(EXP)$(GENCONF_XE) $(pdf_tr) -h $(GLGENDIR)$(D)unused.h $(CONFILES) -o $(pdfobj_tr)


pdlobj_tr=$(GLGENDIR)$(D)pdlobj.tr
$(pdlobj_tr) : $(gpdl_tr)
	$(EXP)$(GENCONF_XE) $(gpdl_tr) -h $(GLGENDIR)$(D)unused.h $(CONFILES) -o $(pdlobj_tr)
