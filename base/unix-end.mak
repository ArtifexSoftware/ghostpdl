# Copyright (C) 2001-2023 Artifex Software, Inc.
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
# Partial makefile common to all Unix and Desqview/X configurations.
# This is the next-to-last part of the makefile for these configurations.
UNIX_END_MAK=$(GLSRC)unix-end.mak $(TOP_MAKEFILES)
# Define the rule for building standard configurations.
directories: $(UNIX_END_MAK)
	@if test "$(BINDIR)"    != "" -a ! -d $(BINDIR);        then mkdir $(BINDIR);        fi
	@if test "$(GLGENDIR)"  != "" -a ! -d $(GLGENDIR);      then mkdir $(GLGENDIR);      fi
	@if test "$(GLOBJDIR)"  != "" -a ! -d $(GLOBJDIR);      then mkdir $(GLOBJDIR);      fi
	@if test "$(DEVGENDIR)" != "" -a ! -d $(DEVGENDIR);     then mkdir $(DEVGENDIR);     fi
	@if test "$(DEVOBJDIR)" != "" -a ! -d $(DEVOBJDIR);     then mkdir $(DEVOBJDIR);     fi
	@if test "$(AUXDIR)"    != "" -a ! -d $(AUXDIR);        then mkdir $(AUXDIR);        fi
	@if test "$(PSGENDIR)"  != "" -a ! -d $(PSGENDIR);      then mkdir $(PSGENDIR);      fi
	@if test "$(PSGENDIR)"  != "" -a ! -d $(PSGENDIR)/cups; then mkdir $(PSGENDIR)/cups; fi
	@if test "$(PSOBJDIR)"  != "" -a ! -d $(PSOBJDIR);      then mkdir $(PSOBJDIR);      fi


gs: .gssubtarget $(UNIX_END_MAK)
	$(NO_OP)

gpcl6: .pcl6subtarget $(UNIX_END_MAK)
	$(NO_OP)

gpcl6clean: cleansub
	$(NO_OP)

gxps: .xpssubtarget $(UNIX_END_MAK)
	$(NO_OP)

gxpsclean: cleansub
	$(NO_OP)

gpdf: .pdfsubtarget $(UNIX_END_MAK)
	$(NO_OP)

gpdfclean: cleansub
	$(NO_OP)

gpdl: .gpdlsubtarget $(UNIX_END_MAK)
	$(NO_OP)

gpdlclean: cleansub
	$(NO_OP)

libgs: $(GS_A)
	$(NO_OP)

libgsexe: $(GS_A_XE)
	$(NO_OP)

libgpcl6: $(GPCL_A)
	$(NO_OP)

libgpcl6exe: $(GPCL_A_XE)
	$(NO_OP)

libgxps: $(GXPS_A)
	$(NO_OP)

libgxpsexe: $(GXPS_A_XE)
	$(NO_OP)

libgpdl: $(GPDL_A)
	$(NO_OP)

libgpdlexe: $(GPDL_A_XE)
	$(NO_OP)

# Define a rule for building profiling configurations.
PGDEFS=GENOPT='-DPROFILE' CFLAGS='-pg $(CFLAGS_PROFILE) $(GCFLAGS) $(XCFLAGS)'\
 LDFLAGS='$(XLDFLAGS) -pg' XLIBS='Xt SM ICE Xext X11' GENOPTAUX= \
 CFLAGSAUX='$(CFLAGSAUX_STANDARD) $(GCFLAGSAUX) $(XCFLAGSAUX)'\
 LDFLAGSAUX='$(XLDFLAGSAUX)'

PROFILEMAKEOPTS=$(SUB_MAKE_OPTION) $(PGDEFS) BUILDDIRPREFIX=$(PGDIRPREFIX)

pg:
	$(MAKE) $(PROFILEMAKEOPTS) default

pgclean:
	$(MAKE) $(PROFILEMAKEOPTS) cleansub

gspg:
	$(MAKE) $(PROFILEMAKEOPTS) .gssubtarget

gpcl6pg:
	$(MAKE) $(PROFILEMAKEOPTS) .pcl6subtarget

gpcl6pgclean:
	$(MAKE) $(PROFILEMAKEOPTS) cleansub

gxpspg:
	$(MAKE) $(PROFILEMAKEOPTS) .xpssubtarget

gxpspgclean:
	$(MAKE) $(PROFILEMAKEOPTS) cleansub

gpdfpg:
	$(MAKE) $(PROFILEMAKEOPTS) .pdfsubtarget

gpdfpgclean:
	$(MAKE) $(PROFILEMAKEOPTS) cleansub

gpdlpg:
	$(MAKE) $(PROFILEMAKEOPTS) .gpdlsubtarget

gpdlpgclean:
	$(MAKE) $(PROFILEMAKEOPTS) cleansub

# Define a rule for building debugging configurations.
DEBUGDEFS=GENOPT='-DDEBUG' CFLAGS='$(CFLAGS_DEBUG) $(GCFLAGS) $(XCFLAGS)' \
GENOPTAUX='-DDEBUG_AUX' CFLAGSAUX='$(CFLAGSAUX_DEBUG) $(GCFLAGSAUX) $(XCFLAGSAUX)'

DEBUGMAKEOPTS=$(SUB_MAKE_OPTION) $(DEBUGDEFS) BUILDDIRPREFIX=$(DEBUGDIRPREFIX)

debug:
	$(MAKE) $(DEBUGMAKEOPTS) default

debug-apitest:
	$(MAKE) $(DEBUGMAKEOPTS) apitest

debugclean:
	$(MAKE) $(DEBUGMAKEOPTS) cleansub


gsdebug:
	$(MAKE) $(DEBUGMAKEOPTS) .gssubtarget

gpcl6debug:
	$(MAKE) $(DEBUGMAKEOPTS) .pcl6subtarget

#gpcl6-debug-apitest:
#	$(MAKE) $(DEBUGMAKEOPTS) apitest

gpcl6debugclean:
	$(MAKE) $(DEBUGMAKEOPTS) cleansub

gxpsdebug:
	$(MAKE) $(DEBUGMAKEOPTS) .xpssubtarget

#gpcl6-debug-apitest:
#	$(MAKE) $(DEBUGMAKEOPTS) apitest

gxpsdebugclean:
	$(MAKE) $(DEBUGMAKEOPTS) cleansub

gpdfdebug:
	$(MAKE) $(DEBUGMAKEOPTS) .pdfsubtarget

gpdfdebugclean:
	$(MAKE) $(DEBUGMAKEOPTS) cleansub

gpdldebug:
	$(MAKE) $(DEBUGMAKEOPTS) .gpdlsubtarget

#gpcl6-debug-apitest:
#	$(MAKE) $(DEBUGMAKEOPTS) apitest

gpdldebugclean:
	$(MAKE) $(DEBUGMAKEOPTS) cleansub


libgsdebug:
	$(MAKE) $(DEBUGMAKEOPTS) libgs

libgpcl6debug:
	$(MAKE) $(DEBUGMAKEOPTS) libgpcl6


libgxpsdebug:
	$(MAKE) $(DEBUGMAKEOPTS) libgxps

libgpdldebug:
	$(MAKE) $(DEBUGMAKEOPTS) libgpdl

# Define a rule for building memento configurations.
MEMENTODEFS=GENOPT='-DMEMENTO -DDEBUG' \
 CFLAGS='$(CFLAGS_DEBUG) $(GCFLAGS) $(XCFLAGS)'\
 BUILDDIRPREFIX=$(MEMENTODIRPREFIX) GENOPTAUX='-DMEMENTO' \
 CFLAGSAUX='$(CFLAGSAUX_DEBUG) $(GCFLAGSAUX) $(XCFLAGSAUX)'

MEMENTOMAKEOPTS=$(SUB_MAKE_OPTION) $(MEMENTODEFS)

memento:
	$(MAKE) $(MEMENTOMAKEOPTS) default

gsmemento:
	$(MAKE) $(MEMENTOMAKEOPTS) .gssubtarget

gpcl6memento:
	$(MAKE) $(MEMENTOMAKEOPTS) .pcl6subtarget

gxpsmemento:
	$(MAKE) $(MEMENTOMAKEOPTS) .xpssubtarget

gpdfmemento:
	$(MAKE) $(MEMENTOMAKEOPTS) .pdfsubtarget

gpdlmemento:
	$(MAKE) $(MEMENTOMAKEOPTS) .gpdlsubtarget

mementoclean:
	$(MAKE) $(MEMENTOMAKEOPTS) cleansub

# Define a rule for building memento configurations with valgrind
MEMENTOVGDEFS=GENOPT='-DMEMENTO -DDEBUG -DPACIFY_VALGRIND -DHAVE_VALGRIND'\
 CFLAGS='$(CFLAGS_DEBUG) $(GCFLAGS) $(XCFLAGS)'\
 BUILDDIRPREFIX=$(MEMENTODIRPREFIX) GENOPTAUX='-DMEMENTO'\
 CFLAGSAUX='$(CFLAGSAUX_DEBUG) $(GCFLAGSAUX) $(XCFLAGSAUX)'\
 BAND_LIST_STORAGE=memory

MEMENTOVGMAKEOPTS=$(SUB_MAKE_OPTION) $(MEMENTOVGDEFS)

mementovg:
	$(MAKE) $(MEMENTOVGMAKEOPTS) default

gsmementovg:
	$(MAKE) $(MEMENTOVGMAKEOPTS) .gssubtarget

gpcl6mementovg:
	$(MAKE) $(MEMENTOVGMAKEOPTS) .pcl6subtarget

gxpsmementovg:
	$(MAKE) $(MEMENTOVGMAKEOPTS) .xpssubtarget

gpdlmementovg:
	$(MAKE) $(MEMENTOVGMAKEOPTS) .gpdlsubtarget

gpdfmementovg:
	$(MAKE) $(MEMENTOVGMAKEOPTS) .pdfsubtarget

mementovgclean:
	$(MAKE) $(MEMENTOVGMAKEOPTS) cleansub

gpcl6_gxps_clean: gpcl6clean gxpsclean
	$(NO_OP)

# Define rules for building address sanitizer configurations.
SANITIZEDEFS=GENOPT='-DDEBUG' \
 CFLAGS='$(CFLAGS_DEBUG) $(CFLAGS_SANITIZE) $(GCFLAGS) $(XCFLAGS)'\
 LDFLAGS='$(LDFLAGS) $(LDFLAGS_SANITIZE)' \
 BUILDDIRPREFIX=$(SANITIZEDIRPREFIX)

SANITIZEMAKEOPTS=$(SUB_MAKE_OPTION) $(SANITIZEDEFS)

sanitize:
	$(MAKE) $(SANITIZEMAKEOPTS) default

gssanitize:
	$(MAKE) $(SANITIZEMAKEOPTS) .gssubtarget

gpcl6sanitize:
	$(MAKE) $(SANITIZEMAKEOPTS) .pcl6subtarget

gxpssanitize:
	$(MAKE) $(SANITIZEMAKEOPTS) .xpssubtarget

gpdfsanitize:
	$(MAKE) $(SANITIZEMAKEOPTS) .pdfsubtarget

gpdlsanitize:
	$(MAKE) $(SANITIZEMAKEOPTS) .gpdlsubtarget

sanitizeclean:
	$(MAKE) $(SANITIZEMAKEOPTS) cleansub

# Define a rule for building release configurations with valgrind
VGDEFS=GENOPT='-DPACIFY_VALGRIND -DHAVE_VALGRIND' \
 CFLAGS='$(GCFLAGS) $(XCFLAGS)'\
 CFLAGSAUX='$(CFLAGSAUX) $(GCFLAGSAUX) $(XCFLAGSAUX)'\
 BAND_LIST_STORAGE=memory

VGMAKEOPTS=$(SUB_MAKE_OPTION) $(VGDEFS)

vg:
	$(MAKE) $(VGMAKEOPTS) default

gsvg:
	$(MAKE) $(VGMAKEOPTS) .gssubtarget

gpcl6vg:
	$(MAKE) $(VGMAKEOPTS) .pcl6subtarget

gxpsvg:
	$(MAKE) $(VGMAKEOPTS) .xpssubtarget

gpdlvg:
	$(MAKE) $(VGMAKEOPTS) .gpdlsubtarget

gpdfvg:
	$(MAKE) $(VGMAKEOPTS) .pdfsubtarget

vgclean:
	$(MAKE) $(VGMAKEOPTS) cleansub

# Define a rule for building debugging configurations.
DEBUGVGDEFS=GENOPT='-DDEBUG -DPACIFY_VALGRIND -DHAVE_VALGRIND'\
 CFLAGS='$(CFLAGS_DEBUG) $(GCFLAGS) $(XCFLAGS)'\
 GENOPTAUXVG='-DDEBUG_AUX'\
 CFLAGSAUX='$(CFLAGSAUX_DEBUG) $(GCFLAGSAUX) $(XCFLAGSAUX)'\
 BAND_LIST_STORAGE=memory

DEBUGVGMAKEOPTS=$(SUB_MAKE_OPTION) $(DEBUGVGDEFS) BUILDDIRPREFIX=$(DEBUGDIRPREFIX)

debugvg:
	$(MAKE) $(DEBUGVGMAKEOPTS) default

gsdebugvg:
	$(MAKE) $(DEBUGVGMAKEOPTS) .gssubtarget

gpcl6debugvg:
	$(MAKE) $(DEBUGVGMAKEOPTS) .pcl6subtarget

gxpsdebugvg:
	$(MAKE) $(DEBUGVGMAKEOPTS) .xpssubtarget

gpdldebugvg:
	$(MAKE) $(DEBUGVGMAKEOPTS) .gpdlsubtarget

gpdfdebugvg:
	$(MAKE) $(DEBUGVGMAKEOPTS) .pdfsubtarget

debugvgclean:
	$(MAKE) $(DEBUGVGMAKEOPTS) cleansub

# Emacs tags maintenance.

TAGS:
	etags -t $(GLSRC)*.[ch] $(PSSRC)*.[ch]
