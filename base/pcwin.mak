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
# makefile for PC window system (MS Windows and OS/2) -specific device
# drivers.

# Define the name of this makefile.
PCWIN_MAK=$(GLSRC)pcwin.mak $(TOP_MAKEFILES)

# We have to isolate these in their own file because the MS Windows code
# requires special compilation switches, different from all other files
# and platforms.

### -------------------- The MS-Windows 3.n DLL ------------------------- ###

gp_mswin_h=$(GLSRC)gp_mswin.h
gsdll_h=$(GLSRC)gsdll.h
gsdllwin_h=$(GLSRC)gsdllwin.h

### -------------------- The MS-Windows DIB 3.n printer ----------------- ###

mswinpr2_=$(GLOBJ)gdevwpr2.$(OBJ)
$(DD)mswinpr2.dev: $(mswinpr2_) $(GLD)page.dev $(PCWIN_MAK)
	$(SETPDEV) $(DD)mswinpr2 $(mswinpr2_)

$(GLOBJ)gdevwpr2.$(OBJ): $(DEVSRC)gdevwpr2.c $(PDEVH) $(windows__h)\
 $(gdevpccm_h) $(gp_h) $(gp_mswin_h) $(gsicc_manage_h) $(PCWIN_MAK)
	$(GLCCWIN) $(GLO_)gdevwpr2.$(OBJ) $(C_) $(DEVSRC)gdevwpr2.c

### --------------------------- The OS/2 printer ------------------------ ###

os2prn_=$(GLOBJ)gdevos2p.$(OBJ)
$(DD)os2prn.dev: $(os2prn_) $(GLD)page.dev $(PCWIN_MAK)
	$(SETPDEV) $(DD)os2prn $(os2prn_)

$(GLOBJ)gdevos2p.$(OBJ): $(GLSRC)gdevos2p.c $(gp_h) $(gdevpccm_h) $(gdevprn_h) $(gscdefs_h) $(PCWIN_MAK)
	$(GLCC) $(GLO_)gdevos2p.$(OBJ) $(C_) $(GLSRC)gdevos2p.c
