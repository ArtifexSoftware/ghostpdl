#    Copyright (C) 1998 Aladdin Enterprises.  All rights reserved.
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


# makefile for PC window system (MS Windows and OS/2) -specific device
# drivers.

# Define the name of this makefile.
PCWIN_MAK=$(GLSRC)pcwin.mak

# We have to isolate these in their own file because the MS Windows code
# requires special compilation switches, different from all other files
# and platforms.

### -------------------- The MS-Windows 3.n DLL ------------------------- ###

gp_mswin_h=$(GLSRC)gp_mswin.h
gsdll_h=$(GLSRC)gsdll.h

gdevmswn_h=$(GLSRC)gdevmswn.h $(GDEVH)\
 $(dos__h) $(memory__h) $(string__h) $(windows__h)\
 $(gp_mswin_h)

$(GLOBJ)gdevmswn.$(OBJ): $(GLSRC)gdevmswn.c $(gdevmswn_h) $(gp_h) $(gpcheck_h)\
 $(gsdll_h) $(gsparam_h) $(gdevpccm_h)
	$(GLCCWIN) $(GLO_)gdevmswn.$(OBJ) $(C_) $(GLSRC)gdevmswn.c

$(GLOBJ)gdevmsxf.$(OBJ): $(GLSRC)gdevmsxf.c $(ctype__h) $(math__h) $(memory__h) $(string__h)\
 $(gdevmswn_h) $(gsstruct_h) $(gsutil_h) $(gxxfont_h)
	$(GLCCWIN) $(GLO_)gdevmsxf.$(OBJ) $(C_) $(GLSRC)gdevmsxf.c

# An implementation using a DIB filled by an image device.
$(GLOBJ)gdevwdib.$(OBJ): $(GLSRC)gdevwdib.c $(gdevmswn_h) $(gsdll_h) $(gxdevmem_h)
	$(GLCCWIN) $(GLO_)gdevwdib.$(OBJ) $(C_) $(GLSRC)gdevwdib.c

mswindll1_=$(GLOBJ)gdevmswn.$(OBJ) $(GLOBJ)gdevmsxf.$(OBJ) $(GLOBJ)gdevwdib.$(OBJ)
mswindll2_=$(GLOBJ)gdevemap.$(OBJ) $(GLOBJ)gdevpccm.$(OBJ)
mswindll_=$(mswindll1_) $(mswindll2_)
mswindll.dev: $(mswindll_)
	$(SETDEV) mswindll $(mswindll1_)
	$(ADDMOD) mswindll $(mswindll2_)

### -------------------- The MS-Windows DDB 3.n printer ----------------- ###

mswinprn_=$(GLOBJ)gdevwprn.$(OBJ) $(GLOBJ)gdevmsxf.$(OBJ)
mswinprn.dev: $(mswinprn_)
	$(SETDEV) mswinprn $(mswinprn_)

$(GLOBJ)gdevwprn.$(OBJ): $(GLSRC)gdevwprn.c $(gdevmswn_h) $(gp_h)
	$(GLCCWIN) $(GLO_)gdevwprn.$(OBJ) $(C_) $(GLSRC)gdevwprn.c

### -------------------- The MS-Windows DIB 3.n printer ----------------- ###

mswinpr2_=$(GLOBJ)gdevwpr2.$(OBJ)
mswinpr2.dev: $(mswinpr2_) page.dev
	$(SETPDEV) mswinpr2 $(mswinpr2_)

$(GLOBJ)gdevwpr2.$(OBJ): $(GLSRC)gdevwpr2.c $(PDEVH) $(windows__h)\
 $(gdevpccm_h) $(gp_h) $(gp_mswin_h)
	$(GLCCWIN) $(GLO_)gdevwpr2.$(OBJ) $(C_) $(GLSRC)gdevwpr2.c

### ------------------ OS/2 Presentation Manager device ----------------- ###

os2pm_=$(GLOBJ)gdevpm.$(OBJ) $(GLOBJ)gdevpccm.$(OBJ)
os2pm.dev: $(os2pm_)
	$(SETDEV) os2pm $(os2pm_)

os2dll_=$(GLOBJ)gdevpm.$(OBJ) $(GLOBJ)gdevpccm.$(OBJ)
os2dll.dev: $(os2dll_)
	$(SETDEV) os2dll $(os2dll_)

$(GLOBJ)gdevpm.$(OBJ): $(GLSRC)gdevpm.c $(string__h)\
 $(gp_h) $(gpcheck_h)\
 $(gsdll_h) $(gserrors_h) $(gsexit_h) $(gsparam_h)\
 $(gx_h) $(gxdevice_h) $(gxdevmem_h)\
 $(gdevpccm_h) $(GLSRC)gdevpm.h
	$(GLCC) $(GLO_)gdevpm.$(OBJ) $(C_) $(GLSRC)gdevpm.c

### --------------------------- The OS/2 printer ------------------------ ###

os2prn_=$(GLOBJ)gdevos2p.$(OBJ)
os2prn.dev: $(os2prn_) page.dev
	$(SETPDEV) os2prn $(os2prn_)

$(GLOBJ)gdevos2p.$(OBJ): gdevos2p.c $(gp_h) $(gdevpccm_h) $(gdevprn_h) $(gscdefs_h)
	$(GLCC) $(GLO_)gdevos2p.$(OBJ) $(C_) $(GLSRC)gdevos2p.c
