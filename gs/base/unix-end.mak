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

# Define the rule for building standard configurations.
directories:
	@if test "$(BINDIR)"    != "" -a ! -d $(BINDIR);        then mkdir $(BINDIR);        fi
	@if test "$(GLGENDIR)"  != "" -a ! -d $(GLGENDIR);      then mkdir $(GLGENDIR);      fi
	@if test "$(GLOBJDIR)"  != "" -a ! -d $(GLOBJDIR);      then mkdir $(GLOBJDIR);      fi
	@if test "$(DEVGENDIR)" != "" -a ! -d $(DEVGENDIR);     then mkdir $(DEVGENDIR);     fi
	@if test "$(DEVOBJDIR)" != "" -a ! -d $(DEVOBJDIR);     then mkdir $(DEVOBJDIR);     fi
	@if test "$(AUXDIR)"    != "" -a ! -d $(AUXDIR);        then mkdir $(AUXDIR);        fi
	@if test "$(PSGENDIR)"  != "" -a ! -d $(PSGENDIR);      then mkdir $(PSGENDIR);      fi
	@if test "$(PSGENDIR)"  != "" -a ! -d $(PSGENDIR)/cups; then mkdir $(PSGENDIR)/cups; fi
	@if test "$(PSOBJDIR)"  != "" -a ! -d $(PSOBJDIR);      then mkdir $(PSOBJDIR);      fi


gpcl6:
	$(MAKE) BUILDDIRPREFIX=pcl PRODUCT_STRING=pcl .pcl6subtarget

gpcl6clean:
	$(MAKE) BUILDDIRPREFIX=pcl PRODUCT_STRING=pcl cleansub

gxps:
	$(MAKE) BUILDDIRPREFIX=xps PRODUCT_STRING=xps .xpssubtarget

gxpsclean:
	$(MAKE) BUILDDIRPREFIX=xps PRODUCT_STRING=xps cleansub



# Define a rule for building profiling configurations.
PGDEFS=GENOPT='-DPROFILE' CFLAGS='$(CFLAGS_PROFILE) $(GCFLAGS) $(XCFLAGS)'\
 LDFLAGS='$(XLDFLAGS) -pg' XLIBS='Xt SM ICE Xext X11'

pg:
	$(MAKE) $(PGDEFS) BUILDDIRPREFIX=$(PGDIRPREFIX) default

pgclean:
	$(MAKE) $(PGDEFS) BUILDDIRPREFIX=$(PGDIRPREFIX) cleansub

gpcl6pg:
	$(MAKE) $(PGDEFS) BUILDDIRPREFIX=pcl$(PGDIRPREFIX) PRODUCT_STRING=pcl .pcl6subtarget

gpcl6pgclean:
	$(MAKE) $(PGDEFS) BUILDDIRPREFIX=pcl$(PGDIRPREFIX) PRODUCT_STRING=pcl cleansub

gxpspg:
	$(MAKE) $(PGDEFS) BUILDDIRPREFIX=xps(PGDIRPREFIX) PRODUCT_STRING=xps .xpssubtarget

gxpspgclean:
	$(MAKE) $(PGDEFS) BUILDDIRPREFIX=xps$(PGDIRPREFIX) PRODUCT_STRING=xps cleansub

# Define a rule for building debugging configurations.
DEBUGDEFS=GENOPT='-DDEBUG' CFLAGS='$(CFLAGS_DEBUG) $(GCFLAGS) $(XCFLAGS)'


debug:
	$(MAKE) $(DEBUGDEFS) BUILDDIRPREFIX=$(DEBUGDIRPREFIX) default

debug-apitest:
	$(MAKE) $(DEBUGDEFS) BUILDDIRPREFIX=$(DEBUGDIRPREFIX) apitest

debugclean:
	$(MAKE) $(DEBUGDEFS) BUILDDIRPREFIX=$(DEBUGDIRPREFIX) cleansub


gpcl6debug:
	$(MAKE) $(DEBUGDEFS) BUILDDIRPREFIX=pcl$(DEBUGDIRPREFIX) PRODUCT_STRING=pcl .pcl6subtarget

#pcl6-debug-apitest:
#	$(MAKE) $(DEBUGDEFS) BUILDDIRPREFIX=pcl$(DEBUGDIRPREFIX) apitest

pcl6debugclean:
	$(MAKE) $(DEBUGDEFS) BUILDDIRPREFIX=pcl$(DEBUGDIRPREFIX) PRODUCT_STRING=pcl cleansub

gxpsdebug:
	$(MAKE) $(DEBUGDEFS) BUILDDIRPREFIX=xps$(DEBUGDIRPREFIX) PRODUCT_STRING=xps .xpssubtarget

#pcl6-debug-apitest:
#	$(MAKE) $(DEBUGDEFS) BUILDDIRPREFIX=xps$(DEBUGDIRPREFIX) apitest

gxpsdebugclean:
	$(MAKE) $(DEBUGDEFS) BUILDDIRPREFIX=xps$(DEBUGDIRPREFIX) PRODUCT_STRING=xps cleansub

# Define a rule for building memento configurations.
MEMENTODEFS=GENOPT='-DMEMENTO -DDEBUG' \
 CFLAGS='$(CFLAGS_DEBUG) $(GCFLAGS) $(XCFLAGS)'\
 BUILDDIRPREFIX=$(MEMENTODIRPREFIX)

memento:
	$(MAKE) $(MEMENTODEFS) default

mementoclean:
	$(MAKE) $(MEMENTODEFS) cleansub

pcl_xps_clean: gpcl6clean gxpsclean
	$(NO_OP)

# Emacs tags maintenance.

TAGS:
	etags -t $(GLSRC)*.[ch] $(PSSRC)*.[ch]
