#    Copyright (C) 1996, 1997 Aladdin Enterprises.  All rights reserved.
#    Unauthorized use, copying, and/or distribution prohibited.

# makefile for PCL5*, HP RTL, and HP-GL/2 interpreters
# Requires pcllib.mak.

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
#	pc*.[ch]	PCL5e, and a few files common to all 3 languages
#	pcc*.[ch]	PCL5c extensions
#	pg*.[ch]	HP-GL/2
#	pgcolor.c	HP-GL/2 color
# We haven't yet attempted to build any configuration other than PCL5e and
# PCL5c, and we know that HP RTL-only configurations don't work, since (at
# least) a lot of the graphics state doesn't get initialized.

# Define the name of this makefile.
PCL5_MAK=pcl5.mak

################ Remaining task list:
# PCL5 state setting commands:
#	font sfont
# PCL5 miscellaneous commands:
#	font sfont
#	status
# PCL5 drawing commands:
#	font
#	raster
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
# LOST mode handling
# Garbage collector interface cleanup

#DEVICE_DEVS is defined in the platform-specific file.
FEATURE_DEVS=dps2lib.dev path1lib.dev patlib.dev rld.dev roplib.dev ttflib.dev

default: $(PCL5)$(XE)
	echo Done.

clean: config-clean
	$(RMN_) $(PCL5)$(XE) *.$(OBJ)

config-clean:
	$(RMN_) *.dev *.tr $(GD)devs.tr$(CONFIG) $(GD)ld$(CONFIG).tr
	$(RMN_) pconf$(CONFIG).h pconfig.h

# Macros for constructing the *.dev files that describe features.
ECHOGS_XE=$(GD)echogs$(XE)
SETMOD=$(ECHOGS_XE) -e .dev -w- -Q-obj
ADDMOD=$(ECHOGS_XE) -e .dev -a-

################ PCL / RTL support ################

#### Miscellaneous

# pgstate.h is out of order because pcstate.h includes it.
pgstate_h=pgstate.h $(gslparam_h) $(gsuid_h) $(gstypes_h) $(gxbitmap_h)
pcommand_h=pcommand.h $(gserror_h) $(gserrors_h) $(scommon_h)
pcdraw_h=pcdraw.h
pcparam_h=pcparam.h $(gsparam_h)
pcstate_h=pcstate.h $(gsiparam_h) $(gsmatrix_h) $(pldict_h) $(plfont_h) $(pgstate_h)

pcommand.$(OBJ): pcommand.c $(std_h)\
 $(gsmatrix_h) $(gsmemory_h) $(gsdevice_h) $(gstypes_h)\
 $(gxstate_h)\
 $(pcommand_h) $(pcparam_h) $(pcstate_h)

pcdraw.$(OBJ): pcdraw.c $(std_h)\
 $(gscoord_h) $(gsdcolor_h) $(gsmatrix_h) $(gsmemory_h) $(gsstate_h) $(gstypes_h)\
 $(gxfixed_h) $(gxpath_h)\
 $(pcdraw_h) $(pcommand_h) $(pcstate_h)

#### PCL5 parsing

pcparse_h=pcparse.h $(gsmemory_h) $(pcommand_h)

pcparse.$(OBJ): pcparse.c $(AK) $(stdio__h) $(gdebug_h) $(gstypes_h)\
 $(scommon_h)\
 $(pcparse_h) $(pcstate_h)

PCL5_PARSE=pcommand.$(OBJ) pcparse.$(OBJ)
# PCL5_OTHER should include $(GD)stream.$(OBJ), but that is included
# automatically anyway.
PCL5_OTHER=$(PCL5_PARSE) pcdraw.$(OBJ)

pcl5base.dev: $(PCL5_MAK) $(ECHOGS_XE) $(PCL5_OTHER) pcllib.dev pjl.dev
	$(SETMOD) pcl5base $(PCL5_OTHER)
	$(ADDMOD) pcl5base -include pcllib pjl
	$(ADDMOD) pcl5base -init pcparse

################ Raster graphics base ################

# This is the intersection of HP RTL and PCL5e/5C.
# We separate this out because HP RTL isn't *quite* a subset of PCL5.

# pgmand.h is here because it is needed for the enter/exit language
# commands in HP RTL.
pgmand_h=pgmand.h $(stdio__h) $(gdebug_h) $(pcommand_h) $(pcstate_h)

#### Monochrome commands
# These are organized by chapter # in the PCL 5 Technical Reference Manual.

rtraster_h=rtraster.h

# Chapters 4, 13, 18, and Comparison Guide
rtmisc.$(OBJ): rtmisc.c $(std_h)\
 $(gsmemory_h) $(gsrop_h)\
 $(pgmand_h)

# Chapter 6
rtcursor.$(OBJ): rtcursor.c $(std_h)\
 $(pcdraw_h) $(pcommand_h) $(pcstate_h)

# Chapter 15
rtraster.$(OBJ): rtraster.c $(memory__h)\
 $(gscoord_h) $(gsdevice_h) $(gsmemory_h) $(gstypes_h)\
 $(gxdevice_h) $(gzstate_h)\
 $(pcdraw_h) $(pcommand_h) $(pcstate_h) $(rtraster_h)

rtlbase_=rtmisc.$(OBJ) rtcursor.$(OBJ) rtraster.$(OBJ)
rtlbase.dev: $(PCL5_MAK) $(ECHOGS_XE) $(rtlbase_) pcl5base.dev
	$(SETMOD) rtlbase $(rtlbase_)
	$(ADDMOD) rtlbase -include pcl5base
	$(ADDMOD) rtlbase -init rtmisc rtcursor rtraster

#### Color commands
# These are organized by chapter # in the PCL 5 Color Technical Reference
# Manual.

# Chapters 2, 3, 4
rtcolor.$(OBJ): rtcolor.c $(std_h)\
 $(gsrop_h) $(pcommand_h) $(pcstate_h)

# Chapter 6
rtcrastr.$(OBJ): rtcrastr.c $(std_h)\
 $(gsbittab_h)\
 $(pcommand_h) $(pcstate_h) $(rtraster_h)

rtlbasec_=rtcolor.$(OBJ) rtcrastr.$(OBJ)
rtlbasec.dev: $(PCL5_MAK) $(ECHOGS_XE) $(rtlbasec_) rtlbase.dev
	$(SETMOD) rtlbasec $(rtlbasec_)
	$(ADDMOD) rtlbasec -include rtlbase
	$(ADDMOD) rtlbasec -init rtcolor rtcrastr

################ HP RTL ################

# HP RTL has just a few commands that aren't in PCL5e, and also supports
# CCITT fax compression modes.

rtlrastr.$(OBJ): rtlrastr.c $(std_h)\
 $(pcommand_h) $(pcstate_h) $(rtraster_h)

HPRTL_OPS=rtlrastr.$(OBJ)

hprtl.dev: $(PCL5_MAK) $(ECHOGS_XE) $(HPRTL_OPS) rtlbase.dev
	$(SETMOD) hprtl $(HPRTL_OPS)
	$(ADDMOD) hprtl -include rtlbase
	$(ADDMOD) hprtl -init rtlrastr

hprtlc.dev: $(PCL5_MAK) $(ECHOGS_XE) rtlbasec.dev
	$(SETMOD) hprtlc -include rtlbasec

################ PCL 5e/5c ################

#### PCL5(e) commands
# These are organized by chapter # in the PCL 5 Technical Reference Manual.

pcfont_h=pcfont.h $(plfont_h)
pcsymbol_h=pcsymbol.h $(plsymbol_h)

# Chapter 4
# Some of these replace implementations in rtmisc.c.
pcjob.$(OBJ): pcjob.c $(std_h)\
 $(gsdevice_h) $(gsmatrix_h) $(gstypes_h) $(gsmemory_h)\
 $(pcdraw_h) $(pcommand_h) $(pcparam_h) $(pcstate_h)

# Chapter 5
pcpage.$(OBJ): pcpage.c $(std_h)\
 $(gsdevice_h) $(gsmatrix_h) $(gspaint_h)\
 $(pcdraw_h) $(pcommand_h) $(pcparam_h) $(pcparse_h) $(pcstate_h)

# Chapter 6
# Some of these replace implementations in rtcursor.c.
pcursor.$(OBJ): pcursor.c $(std_h)\
 $(pcdraw_h) $(pcommand_h) $(pcstate_h)

# Chapter 8
pcfont.$(OBJ): pcfont.c $(std_h) $(stdio__h)\
 $(gschar_h) $(gscoord_h) $(gsfont_h) $(gsmatrix_h) $(gspath_h) $(gsstate_h)\
 $(gsutil_h)\
 $(plvalue_h)\
 $(pcdraw_h) $(pcfont_h) $(pcommand_h) $(pcstate_h) $(pcsymbol_h)

# Chapter 10
pcsymbol.$(OBJ): pcsymbol.c $(std_h)\
 $(plvalue_h)\
 $(pcommand_h) $(pcfont_h) $(pcstate_h) $(pcsymbol_h)

# Chapter 9 & 11
pcsfont.$(OBJ): pcsfont.c $(stdio__h)\
 $(gsccode_h) $(gsmatrix_h) $(gsutil_h) $(gxfont_h) $(gxfont42_h)\
 $(pcommand_h) $(pcfont_h) $(pcstate_h)\
 $(pldict_h) $(plvalue_h)

# Chapter 12
pcmacros.$(OBJ): pcmacros.c $(std_h)\
 $(pcommand_h) $(pcparse_h) $(pcstate_h)

# Chapter 13
# Some of these are in rtmisc.c.
pcprint.$(OBJ): pcprint.c $(std_h)\
 $(gsbitops_h) $(gscoord_h) $(gsmatrix_h) $(gsrop_h) $(gsstate_h)\
 $(gxbitmap_h)\
 $(plvalue_h)\
 $(pcdraw_h) $(pcommand_h) $(pcstate_h)

# Chapter 14
pcrect.$(OBJ): pcrect.c $(math__h)\
 $(gspaint_h) $(gspath_h) $(gspath2_h)\
 $(pcdraw_h) $(pcommand_h) $(pcstate_h)

# Chapter 15
# All of these are in rtraster.c, but some of them are only registered
# in PCL5 mode.

# Chapter 16
pcstatus.$(OBJ): pcstatus.c $(memory__h) $(stdio__h) $(string__h)\
 $(stream_h)\
 $(pcfont_h) $(pcommand_h) $(pcstate_h) $(pcsymbol_h)

# Chapter 24
pcmisc.$(OBJ): pcmisc.c $(std_h) $(pcommand_h) $(pcstate_h)

PCL5_OPS1=pcjob.$(OBJ) pcpage.$(OBJ) pcursor.$(OBJ) pcfont.$(OBJ)
PCL5_OPS2=pcsymbol.$(OBJ) pcsfont.$(OBJ) pcmacros.$(OBJ) pcprint.$(OBJ)
PCL5_OPS3=pcrect.$(OBJ) pcstatus.$(OBJ) pcmisc.$(OBJ)
PCL5_OPS=$(PCL5_OPS1) $(PCL5_OPS2) $(PCL5_OPS3)

pcl5.dev: $(PCL5_MAK) $(ECHOGS_XE) $(PCL5_OPS) pcl5base.dev rtlbase.dev
	$(SETMOD) pcl5 $(PCL5_OPS1)
	$(ADDMOD) pcl5 $(PCL5_OPS2)
	$(ADDMOD) pcl5 $(PCL5_OPS3)
	$(ADDMOD) pcl5 -include rtlbase
	$(ADDMOD) pcl5 -init pcjob pcpage pcursor pcfont
	$(ADDMOD) pcl5 -init pcsymbol pcsfont pcmacros
	$(ADDMOD) pcl5 -init pcprint pcrect rtraster_pcl pcstatus
	$(ADDMOD) pcl5 -init pcmisc

#### PCL5c commands
# These are organized by chapter # in the PCL 5 Color Technical Reference
# Manual.

# Chapter 2
# All of these are in rtcolor.c.

# Chapter 3
# Some of these are in rtcolor.c.
pccpalet.$(OBJ): pccpalet.c $(std_h) $(pcommand_h) $(pcstate_h)

# Chapter 4
# Some of these are in rtcolor.c.
pccrendr.$(OBJ): pccrendr.c $(std_h) $(math__h) $(memory__h)\
 $(gsrop_h) $(gxht_h)\
 $(pcommand_h) $(pcstate_h)

# Chapter 5
pccprint.$(OBJ): pccprint.c $(std_h)\
 $(gsmatrix_h) $(gsrop_h) $(gsstate_h)\
 $(pcommand_h) $(pcstate_h)

# Chapter 6
# All of these are in rtcrastr.c, but some of them are only registered
# in PCL5 mode.

PCL5C_OPS=pccpalet.$(OBJ) pccrendr.$(OBJ) pccprint.$(OBJ)

pcl5c.dev: pcl5.dev $(PCL5_MAK) $(ECHOGS_XE) $(PCL5C_OPS) rtlbasec.dev
	$(SETMOD) pcl5c $(PCL5C_OPS)
	$(ADDMOD) pcl5c -include pcl5 rtlbasec
	$(ADDMOD) pcl5c -init pccpalet pccrendr pccprint rtcrastr_pcl

################ HP-GL/2 ################

pgdraw_h=pgdraw.h
pggeom_h=pggeom.h $(math__h)
pginit_h=pginit.h
pgfont_h=pgfont.h $(stdpre_h) $(gstypes_h)

#### HP-GL/2 non-commands

# Utilities

pgdraw.$(OBJ): pgdraw.c $(math__h) $(stdio__h)\
 $(gdebug_h) $(gscoord_h) $(gsmatrix_h) $(gsmemory_h) $(gspaint_h) $(gspath_h)\
 $(gsstate_h) $(gstypes_h)\
 $(gxfixed_h) $(gxpath_h)\
 $(pcdraw_h)\
 $(pgdraw_h) $(pggeom_h) $(pgmand_h)

pgfont.$(OBJ): pgfont.c $(pgfont_h)

pggeom.$(OBJ): pggeom.c $(math__h) $(stdio__h)\
 $(pggeom_h)

# Initialize/reset.  We break this out simply because it's easier to keep
# track of it this way.

pginit.$(OBJ): pginit.c $(std_h)\
 $(pgdraw_h) $(pginit_h) $(pgmand_h)

# Parsing

pgparse.$(OBJ): pgparse.c $(AK) $(math__h) $(stdio__h)\
 $(gdebug_h) $(gstypes_h) $(scommon_h)\
 $(pgmand_h)

HPGL2_OTHER=pgdraw.$(OBJ) pggeom.$(OBJ) pginit.$(OBJ) pgparse.$(OBJ) pgfont.$(OBJ)

#### HP-GL/2 commands
# These are organized by chapter # in the PCL 5 Technical Reference Manual.

# Chapter 18
# These are PCL commands, but are only relevant to HP RTL and/or HP-GL/2.
# Some of these are in rtmisc.c.
pgframe.$(OBJ): pgframe.c $(math__h)\
 $(gsmatrix_h) $(gsmemory_h) $(gsstate_h) $(gstypes_h)\
 $(pgdraw_h) $(pgmand_h)

# Chapter 19
pgconfig.$(OBJ): pgconfig.c $(std_h)\
 $(gscoord_h) $(gsmatrix_h) $(gsmemory_h) $(gsstate_h) $(gstypes_h)\
 $(pgdraw_h) $(pggeom_h) $(pginit_h) $(pgmand_h)

# Chapter 20
pgvector.$(OBJ): pgvector.c $(math__h) $(stdio__h)\
 $(gdebug_h) $(gscoord_h) $(gspath_h)\
 $(pgdraw_h) $(pggeom_h) $(pgmand_h)

# Chapter 21
pgpoly.$(OBJ): pgpoly.c $(std_h) $(pgdraw_h) $(pggeom_h) $(pgmand_h)

# Chapter 22
pglfill.$(OBJ): pglfill.c $(memory__h)\
 $(gstypes_h) $(gsuid_h) $(gxbitmap_h)\
 $(pgdraw_h) $(pggeom_h) $(pginit_h) $(pgmand_h)

# Chapter 23
pgchar.$(OBJ): pgchar.c $(math__h) $(stdio__h)\
 $(gdebug_h) $(pgfont_h) $(pginit_h) $(pgmand_h)

HPGL2_OPS1=pgframe.$(OBJ) pgconfig.$(OBJ) pgvector.$(OBJ)
HPGL2_OPS2=pgpoly.$(OBJ) pglfill.$(OBJ) pgchar.$(OBJ)
HPGL2_OPS=$(HPGL2_OPS1) $(HPGL2_OPS2)

hpgl2.dev: $(PCL5_MAK) $(ECHOGS_XE) $(HPGL2_OTHER) $(HPGL2_OPS)
	$(SETMOD) hpgl2 $(HPGL2_OTHER)
	$(ADDMOD) hpgl2 $(HPGL2_OPS1)
	$(ADDMOD) hpgl2 $(HPGL2_OPS2)
	$(ADDMOD) hpgl2 -init pginit pgframe pgconfig pgvector
	$(ADDMOD) hpgl2 -init pgpoly pglfill pgchar

#### Color HP-GL/2 commands
# These correspond to chapter 7 in the PCL 5 Color Technical Reference
# Manual.

pgcolor.$(OBJ): pgcolor.c $(std_h)\
 $(gsrop_h) $(gsstate_h)\
 $(pginit_h) $(pgmand_h)

HPGL2C_OPS=pgcolor.$(OBJ)

hpgl2c.dev: hpgl2.dev $(PCL5_MAK) $(ECHOGS_XE) $(HPGL2C_OPS)
	$(SETMOD) hpgl2c $(HPGL2_OPS)
	$(ADDMOD) hpgl2c -include hpgl2

#### Main program

pcmain.$(OBJ): pcmain.c $(AK) $(malloc__h) $(math__h) $(memory__h) $(stdio__h)\
 $(gdebug_h) $(gp_h)\
 $(gscdefs_h) $(gscoord_h) $(gsgc_h) $(gslib_h) $(gsmatrix_h)\
 $(gspaint_h) $(gspath_h) $(gsstate_h)\
 $(gxalloc_h)\
 $(scommon_h)\
 $(plmain_h)\
 $(pjparse_h)\
 $(pcparse_h) $(pcstate_h) pconf$(CONFIG).h
	$(CP_) pconf$(CONFIG).h pconfig.h
	$(CCC) pcmain.c

pcl5.$(OBJ): pcmain.$(OBJ)
	$(CP_) pcmain.$(OBJ) pcl5.$(OBJ)
