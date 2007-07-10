#
# "$Id: cups.mak,v 1.1.2.4 2003/07/20 22:54:43 mike Exp $"
#
# CUPS driver makefile for Ghostscript.
#
# Copyright 2001-2005 by Easy Software Products.
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

### ----------------- CUPS Ghostscript Driver ---------------------- ###

cups_=	$(GLOBJ)gdevcups.$(OBJ)

CUPSSERVER= $(DESTDIR)`cups-config --serverbin`
CUPSCONFIG= $(DESTDIR)`cups-config --serverroot`
CUPSDATA= $(DESTDIR)`cups-config --datadir`

$(DD)cups.dev:	$(cups_) $(GLD)page.dev
	$(ADDMOD) $(DD)cups -lib cupsimage -lib cups
	$(SETPDEV2) $(DD)cups $(cups_)

$(GLOBJ)gdevcups.$(OBJ): cups/gdevcups.c $(PDEVH)
	$(GLCC) $(GLO_)gdevcups.$(OBJ) $(C_) cups/gdevcups.c

install:	install-cups

install-cups:
	-mkdir -p $(DESTDIR)$(CUPSSERVER)/filter
	$(INSTALL_PROGRAM) cups/pstoraster $(DESTDIR)$(CUPSSERVER)/filter
	$(INSTALL_PROGRAM) cups/pstopxl $(DESTDIR)$(CUPSSERVER)/filter
	-mkdir -p $(DESTDIR)$(CUPSCONFIG)
	$(INSTALL_DATA) cups/pstoraster.convs $(DESTDIR)$(CUPSCONFIG)
	-mkdir -p $(DESTDIR)$(CUPSDATA)/model
	$(INSTALL_DATA) cups/pxlcolor.ppd $(DESTDIR)$(CUPSDATA)/model
	$(INSTALL_DATA) cups/pxlmono.ppd $(DESTDIR)$(CUPSDATA)/model


#
# End of "$Id: cups.mak,v 1.1.2.4 2003/07/20 22:54:43 mike Exp $".
#
