#    Copyright (C) 1996, 1997 Aladdin Enterprises.  All rights reserved.
#    Unauthorized use, copying, and/or distribution prohibited.

# makefile for PCL5*, HP RTL, and HP-GL/2 interpreters
# Users of this makefile must define the following:
#	GSSRCDIR - the GS library source directory
#	PLSRCDIR - the PCL* support library source directory
#	PLOBJDIR - the object directory for the PCL support library
#	PCLSRCDIR - the source directory
#	PCLGENDIR - the directory for source files generated during building
#	PCLOBJDIR - the object / executable directory

# This makefile includes support for HP RTL (both monochrome and color),
# PCL5e, PCL5c, and HP-GL/2 (both monochrome and color).  You select
# combinations of these by putting xxx.dev in the definition of FEATURE_DEVS,
# where xxx is chosen from the following list:
#	hprtl	HP RTL monochrome
#	hprtlc	HP RTL color	
#	hpgl2	HP-GL/2 monochrome
#	hpgl2c	HP-GL/2 color
#	pcl5	PCL5e (includes HP-GL/2 monochrome)
#	pcl5c	PCL5c (includes HP-GL/2 color)
# Any combination of these is allowed, although some are redundant: in
# particular, the color configurations include the monochrome as a subset.
# Currently, in order to include HP-GL/2, you must also include either
# HP RTL or PCL5*.
#
# The source files for these languages are named as follows:
#	rt*.[ch]	HP RTL and PCL5e raster commands
#	rtc*.[ch]	HP RTL color and PCL5C raster commands
#			  (except rtcursor.c)
#	pc*.[ch]	PCL5e, and a few files common to multiple languages
#	pcc*.[ch]	PCL5c extensions
#	pg*.[ch]	HP-GL/2
#	pgcolor.c	HP-GL/2 color
# We haven't yet attempted to build any configuration other than PCL5e and
# PCL5c, and we know that HP RTL-only configurations don't work, since (at
# least) a lot of the graphics state doesn't get initialized.

PLOBJ=$(PLOBJDIR)$(D)

PCLSRC=$(PCLSRCDIR)$(D)
PCLGEN=$(PCLGENDIR)$(D)
PCLOBJ=$(PCLOBJDIR)$(D)
PCLO_=$(O_)$(PCLOBJ)

PCLCCC=$(CCC) -I$(PCLSRCDIR) -I$(PCLGENDIR) -I$(PLSRCDIR) -I$(GSSRCDIR) $(C_)

# Define the name of this makefile.
PCL_MAK=$(PCLSRC)pcl.mak

pcl.clean: pcl.config-clean pcl.clean-not-config-clean

pcl.clean-not-config-clean:
	$(RM_) $(PCLOBJ)*.$(OBJ)

pcl.config-clean:
	$(RM_) $(PCLOBJ)*.dev

################ Remaining task list:
# PCL5C argument checking
#	cpalet
# PCL5C state definition
#	cmodes cpalet crendr
# PCL5C state initialization
#	cpalet crendr
# PCL5C state setting commands
#	cmodes cpalet crendr
# PCL5C miscellaneous commands
#	cpalet
# PCL5C drawing commands
#	cpalet crastr
# Garbage collector interface cleanup

################ PCL / RTL support ################

#### Miscellaneous

# pgstate.h is out of order because pcstate.h includes it.
pgstate_h=$(PCLSRC)pgstate.h $(gslparam_h) $(gsuid_h) $(gstypes_h) $(gxbitmap_h)
pcommand_h=$(PCLSRC)pcommand.h $(gserror_h) $(gserrors_h) $(scommon_h)
pcdraw_h=$(PCLSRC)pcdraw.h
pcfont_h=$(PCLSRC)pcfont.h $(plfont_h)
pcparam_h=$(PCLSRC)pcparam.h $(gsparam_h)
pcstate_h=$(PCLSRC)pcstate.h $(gsdcolor_h) $(gsiparam_h) $(gsmatrix_h) $(pldict_h) $(plfont_h) $(pgstate_h)

$(PCLOBJ)pcommand.$(OBJ): $(PCLSRC)pcommand.c $(std_h)\
 $(gsmatrix_h) $(gsmemory_h) $(gsdevice_h) $(gstypes_h)\
 $(gxstate_h)\
 $(pcommand_h) $(pcparam_h) $(pcstate_h)
	$(PCLCCC) $(PCLSRC)pcommand.c $(PCLO_)pcommand.$(OBJ)

$(PCLOBJ)pcdraw.$(OBJ): $(PCLSRC)pcdraw.c $(std_h) $(math__h)\
 $(gscolor2_h) $(gscoord_h) $(gscspace_h) $(gsdcolor_h) $(gsimage_h)\
 $(gsmatrix_h) $(gsmemory_h) $(gsstate_h) $(gstypes_h) $(gsutil_h)\
 $(gxfixed_h) $(gxstate_h)\
 $(pcdraw_h) $(pcommand_h) $(pcfont_h) $(pcstate_h)
	$(PCLCCC) $(PCLSRC)pcdraw.c $(PCLO_)pcdraw.$(OBJ)

#### PCL5 parsing

pcparse_h=$(PCLSRC)pcparse.h $(gsmemory_h) $(pcommand_h)

$(PCLOBJ)pcparse.$(OBJ): $(PCLSRC)pcparse.c $(AK) $(stdio__h) $(gdebug_h) $(gstypes_h)\
 $(scommon_h)\
 $(pcparse_h) $(pcstate_h)
	$(PCLCCC) $(PCLSRC)pcparse.c $(PCLO_)pcparse.$(OBJ)

PCL5_PARSE=$(PCLOBJ)pcommand.$(OBJ) $(PCLOBJ)pcparse.$(OBJ)
# PCL5_OTHER should include $(GD)stream.$(OBJ), but that is included
# automatically anyway.
PCL5_OTHER=$(PCL5_PARSE) $(PCLOBJ)pcdraw.$(OBJ)

$(PCLOBJ)pcl5base.dev: $(PCL_MAK) $(ECHOGS_XE) $(PCL5_OTHER) $(PLOBJ)pl.dev $(PLOBJ)pjl.dev
	$(SETMOD) $(PCLOBJ)pcl5base $(PCL5_OTHER)
	$(ADDMOD) $(PCLOBJ)pcl5base -include $(PLOBJ)pl $(PLOBJ)pjl
	$(ADDMOD) $(PCLOBJ)pcl5base -init pcparse

################ Raster graphics base ################

# This is the intersection of HP RTL and PCL5e/5C.
# We separate this out because HP RTL isn't *quite* a subset of PCL5.

# pgmand.h is here because it is needed for the enter/exit language
# commands in HP RTL.
pgmand_h=$(PCLSRC)pgmand.h $(stdio__h) $(gdebug_h) $(pcommand_h) $(pcstate_h)

#### Monochrome commands
# These are organized by chapter # in the PCL 5 Technical Reference Manual.

rtraster_h=$(PCLSRC)rtraster.h

# Chapters 4, 13, 18, and Comparison Guide
$(PCLOBJ)rtmisc.$(OBJ): $(PCLSRC)rtmisc.c $(math__h)\
 $(gscoord_h) $(gsmemory_h) $(gsrop_h)\
 $(pcdraw_h)\
 $(pgdraw_h) $(pgmand_h) $(pgmisc_h)
	$(PCLCCC) $(PCLSRC)rtmisc.c $(PCLO_)rtmisc.$(OBJ)

# Chapter 6
$(PCLOBJ)rtcursor.$(OBJ): $(PCLSRC)rtcursor.c $(std_h)\
 $(pcdraw_h) $(pcommand_h) $(pcstate_h)
	$(PCLCCC) $(PCLSRC)rtcursor.c $(PCLO_)rtcursor.$(OBJ)

# Chapter 15
$(PCLOBJ)rtraster.$(OBJ): $(PCLSRC)rtraster.c $(memory__h)\
 $(gscoord_h) $(gsdevice_h) $(gsmemory_h) $(gstypes_h)\
 $(gxdevice_h) $(gzstate_h)\
 $(pcdraw_h) $(pcommand_h) $(pcstate_h) $(rtraster_h)
	$(PCLCCC) $(PCLSRC)rtraster.c $(PCLO_)rtraster.$(OBJ)

rtlbase_=$(PCLOBJ)rtmisc.$(OBJ) $(PCLOBJ)rtcursor.$(OBJ) $(PCLOBJ)rtraster.$(OBJ)
$(PCLOBJ)rtlbase.dev: $(PCL_MAK) $(ECHOGS_XE) $(rtlbase_) $(PCLOBJ)pcl5base.dev
	$(SETMOD) $(PCLOBJ)rtlbase $(rtlbase_)
	$(ADDMOD) $(PCLOBJ)rtlbase -include $(PCLOBJ)pcl5base
	$(ADDMOD) $(PCLOBJ)rtlbase -init rtmisc rtcursor rtraster

#### Color commands
# These are organized by chapter # in the PCL 5 Color Technical Reference
# Manual.

# Chapters 2, 3, 4
$(PCLOBJ)rtcolor.$(OBJ): $(PCLSRC)rtcolor.c $(std_h)\
 $(gsrop_h) $(pcommand_h) $(pcstate_h)
	$(PCLCCC) $(PCLSRC)rtcolor.c $(PCLO_)rtcolor.$(OBJ)

# Chapter 6
$(PCLOBJ)rtcrastr.$(OBJ): $(PCLSRC)rtcrastr.c $(std_h)\
 $(gsbittab_h)\
 $(pcommand_h) $(pcstate_h) $(rtraster_h)
	$(PCLCCC) $(PCLSRC)rtcrastr.c $(PCLO_)rtcrastr.$(OBJ)

rtlbasec_=$(PCLOBJ)rtcolor.$(OBJ) $(PCLOBJ)rtcrastr.$(OBJ)
$(PCLOBJ)rtlbasec.dev: $(PCL_MAK) $(ECHOGS_XE) $(rtlbasec_) $(PCLOBJ)rtlbase.dev
	$(SETMOD) $(PCLOBJ)rtlbasec $(rtlbasec_)
	$(ADDMOD) $(PCLOBJ)rtlbasec -include $(PCLOBJ)rtlbase
	$(ADDMOD) $(PCLOBJ)rtlbasec -init rtcolor rtcrastr

################ HP RTL ################

# HP RTL has just a few commands that aren't in PCL5e, and also supports
# CCITT fax compression modes.

$(PCLOBJ)rtlrastr.$(OBJ): $(PCLSRC)rtlrastr.c $(std_h)\
 $(pcommand_h) $(pcstate_h) $(rtraster_h)
	$(PCLCCC) $(PCLSRC)rtlrastr.c $(PCLO_)rtlrastr.$(OBJ)

HPRTL_OPS=$(PCLOBJ)rtlrastr.$(OBJ)

$(PCLOBJ)hprtl.dev: $(PCL_MAK) $(ECHOGS_XE) $(HPRTL_OPS) $(PCLOBJ)rtlbase.dev
	$(SETMOD) $(PCLOBJ)hprtl $(HPRTL_OPS)
	$(ADDMOD) $(PCLOBJ)hprtl -include $(PCLOBJ)rtlbase
	$(ADDMOD) $(PCLOBJ)hprtl -init rtlrastr

$(PCLOBJ)hprtlc.dev: $(PCL_MAK) $(ECHOGS_XE) $(PCLOBJ)rtlbasec.dev
	$(SETMOD) $(PCLOBJ)hprtlc -include $(PCLOBJ)rtlbasec

################ PCL 5e/5c ################

#### Shared support

pcfsel_h=$(PCLSRC)pcfsel.h
pcsymbol_h=$(PCLSRC)pcsymbol.h $(plsymbol_h)

# Font selection is essentially identical in PCL and HP-GL/2.
$(PCLOBJ)pcfsel.$(OBJ): $(PCLSRC)pcfsel.c\
  $(pcommand_h) $(pcfont_h) $(pcfsel_h) $(pcstate_h) $(pcsymbol_h)
	$(PCLCCC) $(PCLSRC)pcfsel.c $(PCLO_)pcfsel.$(OBJ)

PCL_COMMON=$(PCLOBJ)pcfsel.$(OBJ)

#### PCL5(e) commands
# These are organized by chapter # in the PCL 5 Technical Reference Manual.

# Chapter 4
# Some of these replace implementations in rtmisc.c.
$(PCLOBJ)pcjob.$(OBJ): $(PCLSRC)pcjob.c $(std_h)\
 $(gsdevice_h) $(gsmatrix_h) $(gstypes_h) $(gsmemory_h)\
 $(pcdraw_h) $(pcommand_h) $(pcparam_h) $(pcstate_h)
	$(PCLCCC) $(PCLSRC)pcjob.c $(PCLO_)pcjob.$(OBJ)

# Chapter 5
$(PCLOBJ)pcpage.$(OBJ): $(PCLSRC)pcpage.c $(std_h)\
 $(gdevbbox_h)\
 $(gsdevice_h) $(gsmatrix_h) $(gspaint_h)\
 $(gxdevice_h)\
 $(pcdraw_h) $(pcfont_h) $(pcommand_h) $(pcparam_h) $(pcparse_h) $(pcstate_h)
	$(PCLCCC) $(PCLSRC)pcpage.c $(PCLO_)pcpage.$(OBJ)

# Chapter 6
# Some of these replace implementations in rtcursor.c.
$(PCLOBJ)pcursor.$(OBJ): $(PCLSRC)pcursor.c $(std_h)\
 $(pcdraw_h) $(pcfont_h) $(pcommand_h) $(pcstate_h)
	$(PCLCCC) $(PCLSRC)pcursor.c $(PCLO_)pcursor.$(OBJ)

# Chapter 8
$(PCLOBJ)pcfont.$(OBJ): $(PCLSRC)pcfont.c\
 $(memory__h) $(stdio__h) $(string__h) $(gp_h)\
 $(gschar_h) $(gscoord_h) $(gsfont_h) $(gsmatrix_h) $(gspaint_h)\
 $(gspath_h) $(gspath2_h) $(gsstate_h) $(gsutil_h)\
 $(plvalue_h)\
 $(pcdraw_h) $(pcfont_h) $(pcommand_h) $(pcstate_h) $(pcsymbol_h)
	$(PCLCCC) $(PCLSRC)pcfont.c $(PCLO_)pcfont.$(OBJ)

# Chapter 10
$(PCLOBJ)pcsymbol.$(OBJ): $(PCLSRC)pcsymbol.c $(stdio__h)\
 $(plvalue_h)\
 $(pcommand_h) $(pcfont_h) $(pcstate_h) $(pcsymbol_h)
	$(PCLCCC) $(PCLSRC)pcsymbol.c $(PCLO_)pcsymbol.$(OBJ)

# Chapter 9 & 11
$(PCLOBJ)pcifont.$(OBJ): $(PCLSRC)pcifont.c $(std_h)\
 $(gsccode_h) $(gschar_h) $(gsmatrix_h) $(gsmemory_h) $(gsstate_h) $(gstypes_h)\
 $(gxfont_h)\
 $(plfont_h) $(plvalue_h)
	$(PCLCCC) $(PCLSRC)pcifont.c $(PCLO_)pcifont.$(OBJ)

$(PCLOBJ)pcsfont.$(OBJ): $(PCLSRC)pcsfont.c $(stdio__h)\
 $(gsccode_h) $(gsmatrix_h) $(gsutil_h) $(gxfont_h) $(gxfont42_h)\
 $(pcommand_h) $(pcfont_h) $(pcstate_h)\
 $(pldict_h) $(plvalue_h)
	$(PCLCCC) $(PCLSRC)pcsfont.c $(PCLO_)pcsfont.$(OBJ)

# Chapter 12
$(PCLOBJ)pcmacros.$(OBJ): $(PCLSRC)pcmacros.c $(stdio__h)\
 $(pcommand_h) $(pcparse_h) $(pcstate_h)
	$(PCLCCC) $(PCLSRC)pcmacros.c $(PCLO_)pcmacros.$(OBJ)

# Chapter 13
# Some of these are in rtmisc.c.
$(PCLOBJ)pcprint.$(OBJ): $(PCLSRC)pcprint.c $(stdio__h)\
 $(gsbitops_h) $(gscoord_h) $(gsmatrix_h) $(gsrop_h) $(gsstate_h)\
 $(gxbitmap_h)\
 $(plvalue_h)\
 $(pcdraw_h) $(pcommand_h) $(pcstate_h)
	$(PCLCCC) $(PCLSRC)pcprint.c $(PCLO_)pcprint.$(OBJ)

# Chapter 14
$(PCLOBJ)pcrect.$(OBJ): $(PCLSRC)pcrect.c $(math__h)\
 $(gspaint_h) $(gspath_h) $(gspath2_h) $(gsrop_h)\
 $(pcdraw_h) $(pcommand_h) $(pcstate_h)
	$(PCLCCC) $(PCLSRC)pcrect.c $(PCLO_)pcrect.$(OBJ)

# Chapter 15
# All of these are in rtraster.c, but some of them are only registered
# in PCL5 mode.

# Chapter 16
$(PCLOBJ)pcstatus.$(OBJ): $(PCLSRC)pcstatus.c $(memory__h) $(stdio__h) $(string__h)\
 $(stream_h)\
 $(pcfont_h) $(pcommand_h) $(pcstate_h) $(pcsymbol_h)
	$(PCLCCC) $(PCLSRC)pcstatus.c $(PCLO_)pcstatus.$(OBJ)

# Chapter 24
$(PCLOBJ)pcmisc.$(OBJ): $(PCLSRC)pcmisc.c $(std_h) $(pcommand_h) $(pcstate_h)
	$(PCLCCC) $(PCLSRC)pcmisc.c $(PCLO_)pcmisc.$(OBJ)

PCL5_OPS1=$(PCLOBJ)pcjob.$(OBJ) $(PCLOBJ)pcpage.$(OBJ) $(PCLOBJ)pcursor.$(OBJ)
PCL5_OPS2=$(PCLOBJ)pcfont.$(OBJ) $(PCLOBJ)pcsymbol.$(OBJ) $(PCLOBJ)pcifont.$(OBJ)
PCL5_OPS3=$(PCLOBJ)pcsfont.$(OBJ) $(PCLOBJ)pcmacros.$(OBJ) $(PCLOBJ)pcprint.$(OBJ)
PCL5_OPS4=$(PCLOBJ)pcrect.$(OBJ) $(PCLOBJ)pcstatus.$(OBJ) $(PCLOBJ)pcmisc.$(OBJ)
PCL5_OPS=$(PCL5_OPS1) $(PCL5_OPS2) $(PCL5_OPS3) $(PCL5_OPS4)

# Note: we have to initialize the cursor after initializing the logical
# page dimensions, so we do it last.  This is a hack.
$(PCLOBJ)pcl5.dev: $(PCL_MAK) $(ECHOGS_XE) $(PCL_COMMON) $(PCL5_OPS) $(PCLOBJ)pcl5base.dev $(PCLOBJ)rtlbase.dev
	$(SETMOD) $(PCLOBJ)pcl5 $(PCL_COMMON)
	$(ADDMOD) $(PCLOBJ)pcl5 $(PCL5_OPS1)
	$(ADDMOD) $(PCLOBJ)pcl5 $(PCL5_OPS2)
	$(ADDMOD) $(PCLOBJ)pcl5 $(PCL5_OPS3)
	$(ADDMOD) $(PCLOBJ)pcl5 $(PCL5_OPS4)
	$(ADDMOD) $(PCLOBJ)pcl5 -include $(PCLOBJ)rtlbase
	$(ADDMOD) $(PCLOBJ)pcl5 -init pcjob pcpage pcdraw pcfont
	$(ADDMOD) $(PCLOBJ)pcl5 -init pcsymbol pcsfont pcmacros
	$(ADDMOD) $(PCLOBJ)pcl5 -init pcprint pcrect rtraster_pcl pcstatus
	$(ADDMOD) $(PCLOBJ)pcl5 -init pcmisc
	$(ADDMOD) $(PCLOBJ)pcl5 -init   pcursor

#### PCL5c commands
# These are organized by chapter # in the PCL 5 Color Technical Reference
# Manual.

# Chapter 2
# All of these are in rtcolor.c.

# Chapter 3
# Some of these are in rtcolor.c.
$(PCLOBJ)pccpalet.$(OBJ): $(PCLSRC)pccpalet.c $(stdio__h)\
 $(pcommand_h) $(pcstate_h)
	$(PCLCCC) $(PCLSRC)pccpalet.c $(PCLO_)pccpalet.$(OBJ)

# Chapter 4
# Some of these are in rtcolor.c.
$(PCLOBJ)pccrendr.$(OBJ): $(PCLSRC)pccrendr.c $(std_h) $(math__h) $(memory__h)\
 $(gsrop_h) $(gxht_h)\
 $(pcommand_h) $(pcstate_h)
	$(PCLCCC) $(PCLSRC)pccrendr.c $(PCLO_)pccrendr.$(OBJ)

# Chapter 5
$(PCLOBJ)pccprint.$(OBJ): $(PCLSRC)pccprint.c $(std_h)\
 $(gsmatrix_h) $(gsrop_h) $(gsstate_h)\
 $(pcommand_h) $(pcstate_h)
	$(PCLCCC) $(PCLSRC)pccprint.c $(PCLO_)pccprint.$(OBJ)

# Chapter 6
# All of these are in rtcrastr.c, but some of them are only registered
# in PCL5 mode.

PCL5C_OPS=$(PCLOBJ)pccpalet.$(OBJ) $(PCLOBJ)pccrendr.$(OBJ) $(PCLOBJ)pccprint.$(OBJ)

$(PCLOBJ)pcl5c.dev: $(PCL_MAK) $(ECHOGS_XE) $(PCL5C_OPS) $(PCLOBJ)pcl5.dev $(PCLOBJ)rtlbasec.dev
	$(SETMOD) $(PCLOBJ)pcl5c $(PCL5C_OPS)
	$(ADDMOD) $(PCLOBJ)pcl5c -include $(PCLOBJ)pcl5 $(PCLOBJ)rtlbasec
	$(ADDMOD) $(PCLOBJ)pcl5c -init pccpalet pccrendr pccprint rtcrastr_pcl

################ HP-GL/2 ################

pgdraw_h=$(PCLSRC)pgdraw.h
pgfont_h=$(PCLSRC)pgfont.h
pggeom_h=$(PCLSRC)pggeom.h $(math__h) $(gstypes_h)
pginit_h=$(PCLSRC)pginit.h
pgmisc_h=$(PCLSRC)pgmisc.h

#### HP-GL/2 non-commands

# Utilities

$(PCLOBJ)pgdraw.$(OBJ): $(PCLSRC)pgdraw.c $(math__h) $(stdio__h)\
 $(gdebug_h) $(gscoord_h) $(gsmatrix_h) $(gsmemory_h) $(gspaint_h) $(gspath_h)\
 $(gsstate_h) $(gstypes_h)\
 $(gxfarith_h) $(gxfixed_h)\
 $(pcdraw_h)\
 $(pgdraw_h) $(pggeom_h) $(pgmand_h) $(pgmisc_h)
	$(PCLCCC) $(PCLSRC)pgdraw.c $(PCLO_)pgdraw.$(OBJ)

$(PCLOBJ)pggeom.$(OBJ): $(PCLSRC)pggeom.c $(math__h) $(stdio__h)\
 $(pggeom_h)
	$(PCLCCC) $(PCLSRC)pggeom.c $(PCLO_)pggeom.$(OBJ)

$(PCLOBJ)pgmisc.$(OBJ): $(PCLSRC)pgmisc.c $(pgmand_h) $(pgmisc_h)
	$(PCLCCC) $(PCLSRC)pgmisc.c $(PCLO_)pgmisc.$(OBJ)

# Initialize/reset.  We break this out simply because it's easier to keep
# track of it this way.

$(PCLOBJ)pginit.$(OBJ): $(PCLSRC)pginit.c $(memory__h)\
 $(pgdraw_h) $(pginit_h) $(pgmand_h)
	$(PCLCCC) $(PCLSRC)pginit.c $(PCLO_)pginit.$(OBJ)

# Parsing

$(PCLOBJ)pgparse.$(OBJ): $(PCLSRC)pgparse.c $(AK) $(math__h) $(stdio__h)\
 $(gdebug_h) $(gstypes_h) $(scommon_h)\
 $(pgmand_h)
	$(PCLCCC) $(PCLSRC)pgparse.c $(PCLO_)pgparse.$(OBJ)

HPGL2_OTHER1=$(PCLOBJ)pgdraw.$(OBJ) $(PCLOBJ)pggeom.$(OBJ) $(PCLOBJ)pginit.$(OBJ)
HPGL2_OTHER2=$(PCLOBJ)pgparse.$(OBJ) $(PCLOBJ)pgfont.$(OBJ) $(PCLOBJ)pgmisc.$(OBJ)
HPGL2_OTHER=$(HPGL2_OTHER1) $(HPGL2_OTHER2)

#### HP-GL/2 commands
# These are organized by chapter # in the PCL 5 Technical Reference Manual.

# Chapter 18
# These are PCL commands, but are only relevant to HP RTL and/or HP-GL/2.
# Some of these are in rtmisc.c.
$(PCLOBJ)pgframe.$(OBJ): $(PCLSRC)pgframe.c $(math__h)\
 $(gsmatrix_h) $(gsmemory_h) $(gsstate_h) $(gstypes_h)\
 $(pgdraw_h) $(pgmand_h) $(pgmisc_h)
	$(PCLCCC) $(PCLSRC)pgframe.c $(PCLO_)pgframe.$(OBJ)

# Chapter 19
$(PCLOBJ)pgconfig.$(OBJ): $(PCLSRC)pgconfig.c $(std_h)\
 $(gscoord_h) $(gsmatrix_h) $(gsmemory_h) $(gsstate_h) $(gstypes_h)\
 $(pgdraw_h) $(pggeom_h) $(pginit_h) $(pgmand_h) $(pgmisc_h)
	$(PCLCCC) $(PCLSRC)pgconfig.c $(PCLO_)pgconfig.$(OBJ)

# Chapter 20
$(PCLOBJ)pgvector.$(OBJ): $(PCLSRC)pgvector.c $(math__h) $(stdio__h)\
 $(gdebug_h) $(gscoord_h) $(gspath_h)\
 $(pgdraw_h) $(pggeom_h) $(pgmand_h) $(pgmisc_h)
	$(PCLCCC) $(PCLSRC)pgvector.c $(PCLO_)pgvector.$(OBJ)

# Chapter 21
$(PCLOBJ)pgpoly.$(OBJ): $(PCLSRC)pgpoly.c $(std_h)\
 $(pgdraw_h) $(pggeom_h) $(pgmand_h) $(pgmisc_h)
	$(PCLCCC) $(PCLSRC)pgpoly.c $(PCLO_)pgpoly.$(OBJ)

# Chapter 22
$(PCLOBJ)pglfill.$(OBJ): $(PCLSRC)pglfill.c $(memory__h)\
 $(gstypes_h) $(gsuid_h) $(gxbitmap_h)\
 $(pgdraw_h) $(pggeom_h) $(pginit_h) $(pgmand_h) $(pgmisc_h)
	$(PCLCCC) $(PCLSRC)pglfill.c $(PCLO_)pglfill.$(OBJ)

# Chapter 23
$(PCLOBJ)pgchar.$(OBJ): $(PCLSRC)pgchar.c $(math__h) $(stdio__h)\
 $(gdebug_h)\
 $(pcfsel_h)\
 $(pgfont_h) $(pggeom_h) $(pginit_h) $(pgmand_h) $(pgmisc_h)
	$(PCLCCC) $(PCLSRC)pgchar.c $(PCLO_)pgchar.$(OBJ)

$(PCLOBJ)pglabel.$(OBJ): $(PCLSRC)pglabel.c $(ctype__h) $(math__h) $(stdio__h)\
 $(gdebug_h) $(gscoord_h) $(gsline_h)\
 $(pcfsel_h)\
 $(pgdraw_h) $(pgfont_h) $(pggeom_h) $(pginit_h) $(pgmand_h) $(pgmisc_h)
	$(PCLCCC) $(PCLSRC)pglabel.c $(PCLO_)pglabel.$(OBJ)

$(PCLOBJ)pgfont.$(OBJ): $(PCLSRC)pgfont.c $(math__h)\
 $(gspath_h)\
 $(pgdraw_h) $(pgfont_h) $(pgmand_h) $(pgmisc_h)
	$(PCLCCC) $(PCLSRC)pgfont.c $(PCLO_)pgfont.$(OBJ)

HPGL2_OPS1=$(PCLOBJ)pgframe.$(OBJ) $(PCLOBJ)pgconfig.$(OBJ) $(PCLOBJ)pgvector.$(OBJ)
HPGL2_OPS2=$(PCLOBJ)pgpoly.$(OBJ) $(PCLOBJ)pglfill.$(OBJ) $(PCLOBJ)pgchar.$(OBJ)
HPGL2_OPS3=$(PCLOBJ)pglabel.$(OBJ) $(PCLOBJ)pgfont.$(OBJ)
HPGL2_OPS=$(HPGL2_OPS1) $(HPGL2_OPS2) $(HPGL2_OPS3)

$(PCLOBJ)hpgl2.dev: $(PCL_MAK) $(ECHOGS_XE) $(PCL_COMMON) $(HPGL2_OTHER) $(HPGL2_OPS)
	$(SETMOD) $(PCLOBJ)hpgl2 $(PCL_COMMON)
	$(ADDMOD) $(PCLOBJ)hpgl2 $(HPGL2_OTHER1)
	$(ADDMOD) $(PCLOBJ)hpgl2 $(HPGL2_OTHER2)
	$(ADDMOD) $(PCLOBJ)hpgl2 $(HPGL2_OPS1)
	$(ADDMOD) $(PCLOBJ)hpgl2 $(HPGL2_OPS2)
	$(ADDMOD) $(PCLOBJ)hpgl2 $(HPGL2_OPS3)
	$(ADDMOD) $(PCLOBJ)hpgl2 -init pginit pgframe pgconfig pgvector
	$(ADDMOD) $(PCLOBJ)hpgl2 -init pgpoly pglfill pgchar pglabel

#### Color HP-GL/2 commands
# These correspond to chapter 7 in the PCL 5 Color Technical Reference
# Manual.

$(PCLOBJ)pgcolor.$(OBJ): $(PCLSRC)pgcolor.c $(std_h)\
 $(gsrop_h) $(gsstate_h)\
 $(pginit_h) $(pgmand_h)
	$(PCLCCC) $(PCLSRC)pgcolor.c $(PCLO_)pgcolor.$(OBJ)

HPGL2C_OPS=$(PCLOBJ)pgcolor.$(OBJ)

$(PCLOBJ)hpgl2c.dev: $(PCL_MAK) $(ECHOGS_XE) $(HPGL2C_OPS) $(PCLOBJ)hpgl2.dev
	$(SETMOD) $(PCLOBJ)hpgl2c $(HPGL2C_OPS)
	$(ADDMOD) $(PCLOBJ)hpgl2c -include $(PCLOBJ)hpgl2
