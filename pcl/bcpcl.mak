#    Copyright (C) 1996, 1997 Aladdin Enterprises.  All rights reserved.
#    Unauthorized use, copying, and/or distribution prohibited.

# makefile for PCL XL interpreter, MS-DOS / Borland C platform.

# ------------------------------- Options ------------------------------- #

# Define generic configuration options.
DEBUG=1
TDEBUG=0
NOPRIVATE=0

# Note: the blank line below is necessary.
D=\\

# Define the directory with the Ghostscript source files.
#GSDIR=..
GSDIR=$(D)gs
GD=$(GSDIR)$(D)

# Define the name of the executable file.
PCLXL=pclxl

DEVICE_DEVS=vga.dev djet500.dev ljet4.dev pcxmono.dev

MAKEFILE=bcpcl.mak
CONFIG=p

GS=gs

COMP=bcc
COMPDIR=c:\bc\bin
INCDIR=c:\bc\include
LIBDIR=c:\bc\lib
AK=$(GD)ccf.tr turboc.cfg

!include "$(GD)version.mak"
!include "$(GD)tccommon.mak"
!include "$(GD)bcflags.mak"
CCC=$(COMPDIR)\$(COMP) -m$(MM) -zEGS_FAR_DATA @$(GD)ccf.tr -I$(GSDIR) -c
CCLEAF=$(CCC)
.c.obj:
	$(CCC) { $<}

!include "pclxl.mak"
!include "pcllib.mak"

turboc.cfg: $(GD)turboc.cfg
	$(CP_) $(GD)turboc.cfg turboc.cfg

# Build the required files in the GS directory.
ld.tr: $(MAKEFILE) pcl.mak $(GD)echogs$(XE)
	echo FEATURE_DEVS=$(FEATURE_DEVS) >$(GD)_bc_temp.mak
	echo DEVICE_DEVS=$(DEVICE_DEVS) >>$(GD)_bc_temp.mak
	echo !include bclib.mak >>$(GD)_bc_temp.mak
	cd >$(GD)_bc_dir.bat
	cd $(GSDIR)
	echogs$(XE) -w _bc_temp.bat $(CP_) ld$(CONFIG).tr -s -r _bc_dir.bat -q $(D)_.tr
	echogs$(XE) -a _bc_temp.bat cd -s -r _bc_dir.bat
	make -f_bc_temp.mak -DCONFIG=$(CONFIG) ld$(CONFIG).tr gsnogc.$(OBJ)
	make -f_bc_temp.mak -DCONFIG=$(CONFIG) gconfig$(CONFIG).$(OBJ) gscdefs$(CONFIG).$(OBJ)
	call _bc_temp.bat
	rem	Use type rather than copy to update the creation time
	type _.tr >ld.tr

# Link a Borland executable.
ldt.tr: $(MAKEFILE) ld.tr $(PCLXL_ALL)
	echo pxmain.$(OBJ) $(GD)gsnogc.$(OBJ)+ >ldt.tr
	echo $(GD)gconfig$(CONFIG).$(OBJ) $(GD)gscdefs$(CONFIG).$(OBJ)+ >ldt.tr
	echo $(PCLXL_OPS1)+ >>ldt.tr
	echo $(PCLXL_OPS2)+ >>ldt.tr
	echo $(PCLXL_OTHER1)+ >>ldt.tr
	echo $(PCLXL_OTHER2)+ >>ldt.tr
	$(CP_) ldt.tr+ld.tr

LIBCTR=libc$(MM).tr

$(LIBCTR): $(MAKEFILE) makefile $(ECHOGS_XE)
	echogs -w $(LIBCTR) $(LIBOVL) $(LIBDIR)\$(FPLIB)+
	echogs -a $(LIBCTR) $(LIBDIR)\math$(MM) $(LIBDIR)\c$(MM)

$(PCLXL)$(XE): ldt.tr $(GD)lib.tr $(LIBCTR)
	$(COMPDIR)\tlink $(LCT) $(LO) $(LIBDIR)\c0$(MM) @ldt.tr ,$(GS),$(GS),@$(GD)lib.tr @$(LIBCTR)
