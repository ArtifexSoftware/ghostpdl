# Copyright (C) 2001-2012 Artifex Software, Inc.
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
# Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134, San Rafael,
# CA  94903, U.S.A., +1(415)492-9861, for further information.
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

gpdl: .gpdlsubtarget $(UNIX_END_MAK)
	$(NO_OP)

gpdlclean: .cleansub
	$(NO_OP)

# Define a rule for building profiling configurations.
PGDEFS=GENOPT='-DPROFILE' CFLAGS='$(CFLAGS_PROFILE) $(GCFLAGS) $(XCFLAGS)'\
 LDFLAGS='$(XLDFLAGS) -pg' XLIBS='Xt SM ICE Xext X11'

pg:
	$(MAKE) $(SUB_MAKE_OPTION) $(PGDEFS) BUILDDIRPREFIX=$(PGDIRPREFIX) default

pgclean:
	$(MAKE) $(SUB_MAKE_OPTION) $(PGDEFS) BUILDDIRPREFIX=$(PGDIRPREFIX) cleansub

gspg:
	$(MAKE) $(SUB_MAKE_OPTION) $(PGDEFS) BUILDDIRPREFIX=$(PGDIRPREFIX) .gssubtarget

gpcl6pg:
	$(MAKE) $(SUB_MAKE_OPTION) $(PGDEFS) BUILDDIRPREFIX=$(PGDIRPREFIX) .pcl6subtarget

gpcl6pgclean:
	$(MAKE) $(SUB_MAKE_OPTION) $(PGDEFS) BUILDDIRPREFIX=$(PGDIRPREFIX) cleansub

gxpspg:
	$(MAKE) $(SUB_MAKE_OPTION) $(PGDEFS) BUILDDIRPREFIX=(PGDIRPREFIX) .xpssubtarget

gxpspgclean:
	$(MAKE) $(SUB_MAKE_OPTION) $(PGDEFS) BUILDDIRPREFIX=$(PGDIRPREFIX) cleansub

gpdlpg:
	$(MAKE) $(SUB_MAKE_OPTION) $(PGDEFS) BUILDDIRPREFIX=(PGDIRPREFIX) .gpdlsubtarget

gpdlpgclean:
	$(MAKE) $(SUB_MAKE_OPTION) $(PGDEFS) BUILDDIRPREFIX=$(PGDIRPREFIX) cleansub

# Define a rule for building debugging configurations.
DEBUGDEFS=GENOPT='-DDEBUG' CFLAGS='$(CFLAGS_DEBUG) $(GCFLAGS) $(XCFLAGS)'


debug:
	$(MAKE) $(SUB_MAKE_OPTION) $(DEBUGDEFS) BUILDDIRPREFIX=$(DEBUGDIRPREFIX) default

debug-apitest:
	$(MAKE) $(SUB_MAKE_OPTION) $(DEBUGDEFS) BUILDDIRPREFIX=$(DEBUGDIRPREFIX) apitest

debugclean:
	$(MAKE) $(SUB_MAKE_OPTION) $(DEBUGDEFS) BUILDDIRPREFIX=$(DEBUGDIRPREFIX) cleansub


gsdebug:
	$(MAKE) $(SUB_MAKE_OPTION) $(DEBUGDEFS) BUILDDIRPREFIX=$(DEBUGDIRPREFIX) .gssubtarget

gpcl6debug:
	$(MAKE) $(SUB_MAKE_OPTION) $(DEBUGDEFS) BUILDDIRPREFIX=$(DEBUGDIRPREFIX) .pcl6subtarget

#gpcl6-debug-apitest:
#	$(MAKE) $(SUB_MAKE_OPTION) $(DEBUGDEFS) BUILDDIRPREFIX=$(DEBUGDIRPREFIX) apitest

gpcl6debugclean:
	$(MAKE) $(SUB_MAKE_OPTION) $(DEBUGDEFS) BUILDDIRPREFIX=$(DEBUGDIRPREFIX) cleansub

gxpsdebug:
	$(MAKE) $(SUB_MAKE_OPTION) $(DEBUGDEFS) BUILDDIRPREFIX=$(DEBUGDIRPREFIX) .xpssubtarget

#gpcl6-debug-apitest:
#	$(MAKE) $(SUB_MAKE_OPTION) $(DEBUGDEFS) BUILDDIRPREFIX=$(DEBUGDIRPREFIX) apitest

gxpsdebugclean:
	$(MAKE) $(SUB_MAKE_OPTION) $(DEBUGDEFS) BUILDDIRPREFIX=$(DEBUGDIRPREFIX) cleansub

gpdldebug:
	$(MAKE) $(SUB_MAKE_OPTION) $(DEBUGDEFS) BUILDDIRPREFIX=$(DEBUGDIRPREFIX) .gpdlsubtarget

#gpcl6-debug-apitest:
#	$(MAKE) $(SUB_MAKE_OPTION) $(DEBUGDEFS) BUILDDIRPREFIX=$(DEBUGDIRPREFIX) apitest

gpdldebugclean:
	$(MAKE) $(SUB_MAKE_OPTION) $(DEBUGDEFS) BUILDDIRPREFIX=$(DEBUGDIRPREFIX) cleansub



# Define a rule for building memento configurations.
MEMENTODEFS=GENOPT='-DMEMENTO -DDEBUG' \
 CFLAGS='$(CFLAGS_DEBUG) $(GCFLAGS) $(XCFLAGS)'\
 BUILDDIRPREFIX=$(MEMENTODIRPREFIX)

memento:
	$(MAKE) $(SUB_MAKE_OPTION) $(MEMENTODEFS) default

gsmemento:
	$(MAKE) $(SUB_MAKE_OPTION) $(MEMENTODEFS) .gssubtarget

gpcl6memento:
	$(MAKE) $(SUB_MAKE_OPTION) $(MEMENTODEFS) .pcl6subtarget

gxpsmemento:
	$(MAKE) $(SUB_MAKE_OPTION) $(MEMENTODEFS) .xpssubtarget

mementoclean:
	$(MAKE) $(SUB_MAKE_OPTION) $(MEMENTODEFS) cleansub

gpcl6_gxps_clean: gpcl6clean gxpsclean
	$(NO_OP)

# Define rules for building address sanitizer configurations.
# NOTE: Currently these targets rely on ignoring errors. This
# will be fixed in future.
SANITIZEDEFS=GENOPT='-DDEBUG' \
 CFLAGS='$(CFLAGS_DEBUG) $(CFLAGS_SANITIZE) $(GCFLAGS) $(XCFLAGS)'\
 LDFLAGS='$(LDFLAGS) $(LDFLAGS_SANITIZE)' \
 BUILDDIRPREFIX=$(SANITIZEDIRPREFIX) \
 -i

sanitize:
	$(MAKE) $(SUB_MAKE_OPTION) $(SANITIZEDEFS) default

gssanitize:
	$(MAKE) $(SUB_MAKE_OPTION) $(SANITIZEDEFS) .gssubtarget

gpcl6sanitize:
	$(MAKE) $(SUB_MAKE_OPTION) $(SANITIZEDEFS) .pcl6subtarget

gxpssanitize:
	$(MAKE) $(SUB_MAKE_OPTION) $(SANITIZEDEFS) .xpssubtarget

sanitizeclean:
	$(MAKE) $(SUB_MAKE_OPTION) $(SANITIZEDEFS) cleansub

# Emacs tags maintenance.

TAGS:
	etags -t $(GLSRC)*.[ch] $(PSSRC)*.[ch]
