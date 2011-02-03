#
# "$Id$"
#
# CUPS driver makefile for Ghostscript.
#
# Copyright 2001-2005 by Easy Software Products.
# Copyright 2007 Artifex Software, Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
#

# define the name of this makefile
CUPS_MAK=cups/cups.mak

### ----------------- CUPS Ghostscript Driver ---------------------- ###

cups_=	$(GLOBJ)gdevcups.$(OBJ)

# These are set in the toplevel Makefile via autoconf(1)
# CUPSCFLAGS=`cups-config --cflags`
# CUPSSERVERBIN=`cups-config --serverbin`
# CUPSSERVERROOT=`cups-config --serverroot`
# CUPSDATA=`cups-config --datadir`
# CUPSPDFTORASTER= 1 if CUPS is new enough (cups-config --version)

PDFTORASTER_XE=$(BINDIR)$(D)pdftoraster$(XE)

cups: pdftoraster
pdftoraster: $(PDFTORASTER_XE)
pdftoraster_=cups/pdftoraster.c

$(PDFTORASTER_XE): $(pdftoraster_)
	if [ "$(CUPSPDFTORASTER)" = "1" ]; then \
	    $(GLCC) $(LDFLAGS) -DBINDIR='"$(bindir)"' -DGS='"$(GS)"' -o $@ $(pdftoraster_) `cups-config --image --ldflags --libs`; \
	fi

install:	install-cups

install-cups: cups
	-mkdir -p $(DESTDIR)$(CUPSSERVERBIN)/filter
	$(INSTALL_PROGRAM) cups/pstoraster $(DESTDIR)$(CUPSSERVERBIN)/filter
	if [ "$(CUPSPDFTORASTER)" = "1" ]; then \
	    $(INSTALL_PROGRAM) $(PDFTORASTER_XE) $(DESTDIR)$(CUPSSERVERBIN)/filter; \
	fi
	$(INSTALL_PROGRAM) cups/pstopxl $(DESTDIR)$(CUPSSERVERBIN)/filter
	-mkdir -p $(DESTDIR)$(CUPSSERVERROOT)
	$(INSTALL_DATA) cups/pstoraster.convs $(DESTDIR)$(CUPSSERVERROOT)
	if [ "$(CUPSPDFTORASTER)" = "1" ]; then \
	    $(INSTALL_DATA) cups/pdftoraster.convs $(DESTDIR)$(CUPSSERVERROOT); \
	fi
	-mkdir -p $(DESTDIR)$(CUPSDATA)/model
	$(INSTALL_DATA) cups/pxlcolor.ppd $(DESTDIR)$(CUPSDATA)/model
	$(INSTALL_DATA) cups/pxlmono.ppd $(DESTDIR)$(CUPSDATA)/model


#
# End of "$Id$".
#
