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

# PLOBJ       = $(PLOBJDIR)$(D)

PCLSRC      = $(PCLSRCDIR)$(D)
PCLGEN      = $(PCLGENDIR)$(D)
PCLOBJ      = $(PCLOBJDIR)$(D)
PCLO_       = $(O_)$(PCLOBJ)

PCLCCC  = $(CCC) -I$(PCLSRCDIR) -I$(PCLGENDIR) -I$(PLSRCDIR) -I$(GLSRCDIR) $(C_)

# Define the name of this makefile.
PCL_MAK     = $(PCLSRC)pcl.mak

pcl.clean: pcl.config-clean pcl.clean-not-config-clean

pcl.clean-not-config-clean:
	$(RM_) $(PCLOBJ)*.$(OBJ)

# due to a deficiency of the current make system devices are built in
# the current working directory.  Also all of the gs generated files
# are not "cleaned" we hack around that problem here.
pcl.config-clean:
	$(RM_) $(PCLOBJ)*.dev
	$(RM_) $(PCLOBJ)devs.tr5
	make -f $(GLSRCDIR)$(D)ugcclib.mak \
	GLSRCDIR='$(GLSRCDIR)' GLGENDIR='$(GLGENDIR)' \
	GLOBJDIR='$(GLOBJDIR)' clean

################ Remaining task list:
# PCL5C drawing commands
#	crastr crastr
# Garbage collector interface cleanup

################ PCL / RTL support ################

#### Miscellaneous

# pgstate.h is out of order because pcstate.h includes it.
pgstate_h   = $(PCLSRC)pgstate.h  \
              $(gx_h)             \
              $(gxfixed_h)        \
              $(gslparam_h)       \
              $(gzpath_h)

pccoord_h   = $(PCLSRC)pccoord.h

pcxfmst_h   = $(PCLSRC)pcxfmst.h  \
              $(gx_h)             \
              $(gsmatrix_h)       \
              $(gxfixed_h)        \
              $(pccoord_h)

pcfontst_h  = $(PCLSRC)pcfontst.h \
              $(gx_h)             \
              $(plfont_h)

pctpm_h     = $(PCLSRC)pctpm.h    \
              $(gx_h)

pcident_h   = $(PCLSRC)pcident.h  \
              $(gx_h)

pcpattyp_h  = $(PCLSRC)pcpattyp.h

pcdict_h    = $(PCLSRC)pcdict.h   \
              $(gx_h)

rtrstst_h   = $(PCLSRC)rtrstst.h  \
              $(gx_h)             \
              $(pccoord_h)

pcmtx3_h    = $(PCLSRC)pcmtx3.h   \
              $(gx_h)             \
              $(gsmatrix_h)       \
              $(gscspace_h)       \
              $(gscolor2_h)       \
              $(gscie_h)

pcommand_h  = $(PCLSRC)pcommand.h \
              $(gx_h)             \
              $(gserrors_h)

pcstate_h   = $(PCLSRC)pcstate.h  \
              $(gx_h)             \
              $(scommon_h)        \
              $(gsdcolor_h)       \
              $(gschar.h)         \
              $(pldict_h)         \
              $(plfont_h)         \
              $(pccoord_h)        \
              $(pcxfmst_h)        \
              $(pcfontst_h)       \
              $(pctpm_h)          \
              $(pcpattyp_h)       \
              $(pcdict_h)         \
              $(rtrstst_h)        \
              $(pgstate_h)

pccid_h     = $(PCLSRC)pccid.h    \
              $(gx_h)             \
              $(gsstruct_h)       \
              $(pcstate_h)        \
              $(pcommand_h)

pccrd_h     = $(PCLSRC)pccrd.h    \
              $(gx_h)             \
              $(gsstruct_h)       \
              $(gsrefct_h)        \
              $(gsmatrix_h)       \
              $(gscspace_h)       \
              $(gscolor2_h)       \
              $(gscie_h)          \
              $(gscrd_h)          \
              $(pcident_h)        \
              $(pcstate_h)

pcdither_h  = $(PCLSRC)pcdither.h \
              $(gx_h)             \
              $(gsstruct_h)       \
              $(gsrefct_h)        \
              $(pcstate_h)        \
              $(pcommand_h)

pcdraw_h    = $(PCLSRC)pcdraw.h   \
              $(pcstate_h)

pcfont_h    = $(PCLSRC)pcfont.h   \
              $(pcstate_h)        \
              $(plfont_h)

pclookup_h  = $(PCLSRC)pclookup.h \
              $(gx_h)             \
              $(gsstruct_h)       \
              $(gsrefct_h)        \
              $(pcommand_h)       \
              $(pccid_h)

pccsbase_h  = $(PCLSRC)pccsbase.h \
              $(gx_h)             \
              $(gsstruct_h)       \
              $(gsrefct_h)        \
              $(gscspace_h)       \
              $(pcident_h)        \
              $(pcstate_h)        \
              $(pcommand_h)       \
              $(pccid_h)          \
              $(pclookup_h)

pcht_h      = $(PCLSRC)pcht.h     \
              $(gx_h)             \
              $(gsstruct_h)       \
              $(gsrefct_h)        \
              $(gsht1_h)          \
              $(gxhtx_h)          \
              $(pcident_h)        \
              $(pcstate_h)        \
              $(pcommand_h)       \
              $(pclookup_h)       \
              $(pcdither_h)

pcindexed_h = $(PCLSRC)pcindexed.h\
              $(gx_h)             \
              $(gsstruct_h)       \
              $(gsrefct_h)        \
              $(gscspace_h)       \
              $(pcident_h)        \
              $(pcstate_h)        \
              $(pccid_h)          \
              $(pclookup_h)       \
              $(pccsbase_h)

pcpalet_h   = $(PCLSRC)pcpalet.h  \
              $(gx_h)             \
              $(gsstruct_h)       \
              $(gsrefct_h)        \
              $(pcident_h)        \
              $(pcstate_h)        \
              $(pcommand_h)       \
              $(pclookup_h)       \
              $(pcdither_h)       \
              $(pccid_h)          \
              $(pcindexed_h)      \
              $(pcht_h)           \
              $(pccrd_h)

pcfrgrnd_h  = $(PCLSRC)pcfrgrnd.h \
              $(gx_h)             \
              $(gsstruct_h)       \
              $(gsrefct_h)        \
              $(pcstate_h)        \
              $(pcommand_h)       \
              $(pccsbase_h)       \
              $(pcht_h)           \
              $(pccrd_h)          \
              $(pcpalet_h)

pcpage_h    = $(PCLSRC)/pcpage.h  \
              $(pcstate_h)        \
              $(pcommand_h)

pcparam_h   = $(PCLSRC)pcparam.h  \
              $(gsparam_h)

pcparse_h   = $(PCLSRC)pcparse.h  \
              $(gsmemory_h)       \
              $(pcommand_h)

pcpatrn_h   = $(PCLSRC)pcpatrn.h  \
              $(gx_h)             \
              $(gsstruct_h)       \
              $(gsrefct_h)        \
              $(pcindexed_h)      \
              $(pccsbase_h)

pcbiptrn_h  = $(PCLSRC)pcbiptrn.h \
              $(pcpatrn_h)

pcpatxfm_h  = $(PCLSRC)pcpatxfm.h \
              $(gx_h)             \
              $(gsmatrix_h)       \
              $(gscoord_h)        \
              $(pcstate_h)        \
              $(pcommand_h)       \
              $(pcpatrn_h)

pcuptrn_h   = $(PCLSRC)pcuptrn.h  \
              $(gx_h)             \
              $(pcommand_h)       \
              $(pcpatrn_h)

pcursor_h   = $(PCLSRC)pcursor.h  \
              $(gx_h)             \
              $(pcstate_h)        \
              $(pcommand_h)

pcwhtindx_h = $(PCLSRC)pcwhtindx.h\
              $(gx_h)             \
              $(gsbitmap_h)       \
              $(pcindexed_h)

rtgmode_h   = $(PCLSRC)rtgmode.h  \
              $(rtrstst_h)        \
              $(pcstate_h)        \
              $(pcommand_h)

rtraster_h  = $(PCLSRC)rtraster.h \
              $(pcstate_h)        \
              $(pcommand_h)

rtrstcmp_h  = $(PCLSRC)rtrstcmp.h \
              $(gx_h)             \
              $(gsstruct_h)



$(PCLOBJ)pcommand.$(OBJ): $(PCLSRC)pcommand.c   \
                          $(std_h)              \
                          $(gstypes_h)          \
                          $(gsmemory_h)         \
                          $(gsmatrix_h)         \
                          $(gxstate_h)          \
                          $(pcommand_h)         \
                          $(pcstate_h)          \
                          $(pcparam_h)          \
                          $(pcident_h)
	$(PCLCCC) $(PCLSRC)pcommand.c $(PCLO_)pcommand.$(OBJ)

$(PCLOBJ)pcdraw.$(OBJ): $(PCLSRC)pcdraw.c   \
                        $(gx_h)             \
                        $(gsmatrix_h)       \
                        $(gscoord_h)        \
                        $(gsstate_h)        \
                        $(gsrop_h)          \
                        $(gxfixed_h)        \
                        $(pcstate_h)        \
                        $(pcht_h)           \
                        $(pccrd_h)          \
                        $(pcpatrn_h)        \
                        $(pcdraw_h)
	$(PCLCCC) $(PCLSRC)pcdraw.c $(PCLO_)pcdraw.$(OBJ)

#### PCL5 parsing

$(PCLOBJ)pcparse.$(OBJ): $(PCLSRC)pcparse.c \
                         $(AK)              \
                         $(stdio__h)        \
                         $(gdebug_h)        \
                         $(gstypes_h)       \
                         $(scommon_h)       \
                         $(pcparse_h)       \
                         $(pcstate_h)       \
                         $(pcursor_h)       \
                         $(rtgmode_h)
	$(PCLCCC) $(PCLSRC)pcparse.c $(PCLO_)pcparse.$(OBJ)

PCL5_PARSE  = $(PCLOBJ)pcommand.$(OBJ) $(PCLOBJ)pcparse.$(OBJ)

# PCL5_OTHER should include $(GD)stream.$(OBJ), but that is included
# automatically anyway.
PCL5_OTHER  = $(PCL5_PARSE) $(PCLOBJ)pcdraw.$(OBJ)

$(PCLOBJ)pcl5base.dev: $(PCL_MAK) $(ECHOGS_XE) $(PCL5_OTHER)    \
                       $(PLOBJ)pl.dev $(PLOBJ)pjl.dev
	$(SETMOD) $(PCLOBJ)pcl5base $(PCL5_OTHER)
	$(ADDMOD) $(PCLOBJ)pcl5base -include $(PLOBJ)pl $(PLOBJ)pjl
	$(ADDMOD) $(PCLOBJ)pcl5base -init pcparse

################ Raster graphics base ################

# This is the intersection of HP RTL and PCL5e/5C.
# We separate this out because HP RTL isn't *quite* a subset of PCL5.

# pgmand.h is here because it is needed for the enter/exit language
# commands in HP RTL.
pgmand_h    = $(PCLSRC)pgmand.h   \
              $(stdio__h)         \
              $(gdebug_h)         \
              $(pcommand_h)       \
              $(pcstate_h)

#### Monochrome commands
# These are organized by chapter # in the PCL 5 Technical Reference Manual.

rtraster_h  = $(PCLSRC)rtraster.h   \
              $(pcstate_h)          \
              $(pcommand_h)

# Chapters 4, 13, 18, and Comparison Guide
$(PCLOBJ)rtmisc.$(OBJ): $(PCLSRC)rtmisc.c   \
                        $(math__h)          \
                        $(pgmand_h)         \
                        $(pgdraw_h)         \
                        $(pgmisc_h)         \
                        $(gsmemory_h)       \
                        $(gsrop_h)          \
                        $(gscoord_h)        \
                        $(pcpatxfm_h)       \
                        $(pcpage_h)         \
                        $(pcdraw_h)
	$(PCLCCC) $(PCLSRC)rtmisc.c $(PCLO_)rtmisc.$(OBJ)

# Chapter 6
$(PCLOBJ)rtcursor.$(OBJ): $(PCLSRC)rtcursor.c   \
                          $(std_h)              \
                          $(pcommand_h)         \
                          $(pcstate_h)          \
                          $(pcdraw_h)
	$(PCLCCC) $(PCLSRC)rtcursor.c $(PCLO_)rtcursor.$(OBJ)

# Chapter 15
$(PCLOBJ)rtraster.$(OBJ): $(PCLSRC)rtraster.c   \
                          $(gx_h)               \
                          $(gsmatrix_h)         \
                          $(gscoord_h)          \
                          $(gspath_h)           \
                          $(gspath2_h)          \
                          $(gsimage_h)          \
                          $(gsiparam_h)         \
                          $(gsiparm4_h)         \
                          $(gsdevice_h)         \
                          $(gsrop_h)            \
                          $(pcstate_h)          \
                          $(pcpalet_h)          \
                          $(pcindexed_h)        \
                          $(pcwhtindx_h)        \
                          $(pcdraw_h)           \
                          $(rtgmode_h)          \
                          $(rtrstcmp_h)         \
                          $(rtraster_h)
	$(PCLCCC) $(PCLSRC)rtraster.c $(PCLO_)rtraster.$(OBJ)

rtlbase_    = $(PCLOBJ)rtmisc.$(OBJ) $(PCLOBJ)rtcursor.$(OBJ)   \
              $(PCLOBJ)rtraster.$(OBJ)

$(PCLOBJ)rtlbase.dev: $(PCL_MAK) $(ECHOGS_XE) $(rtlbase_) $(PCLOBJ)pcl5base.dev
	$(SETMOD) $(PCLOBJ)rtlbase $(rtlbase_)
	$(ADDMOD) $(PCLOBJ)rtlbase -include $(PCLOBJ)pcl5base
	$(ADDMOD) $(PCLOBJ)rtlbase -init rtmisc rtcursor rtraster

#### Color commands
# These are organized by chapter # in the PCL 5 Color Technical Reference
# Manual. (These are no longer separable from the base PCL set)

# Chapter 2, 3, 4, 5
$(PCLOBJ)pcbiptrn.$(OBJ): $(PCLSRC)pcbiptrn.c   \
                          $(pcpatrn_h)          \
                          $(pcuptrn_h)          \
                          $(pcbiptrn_h)
	$(PCLCCC) $(PCLSRC)pcbiptrn.c $(PCLO_)pcbiptrn.$(OBJ)

$(PCLOBJ)pccid.$(OBJ): $(PCLSRC)pccid.c     \
                       $(gx_h)              \
                       $(gsmemory_h)        \
                       $(gsstruct_h)        \
                       $(pcommand_h)        \
                       $(pcstate_h)         \
                       $(pcpalet_h)         \
                       $(pccid_h)
	$(PCLCCC) $(PCLSRC)pccid.c $(PCLO_)pccid.$(OBJ)

$(PCLOBJ)pccolor.$(OBJ): $(PCLSRC)pccolor.c \
                         $(std.h)           \
                         $(pcommand_h)      \
                         $(pcstate_h)       \
                         $(pcpalet_h)
	$(PCLCCC) $(PCLSRC)pccolor.c $(PCLO_)pccolor.$(OBJ)

$(PCLOBJ)pccrd.$(OBJ): $(PCLSRC)pccrd.c \
                       $(gx_h)          \
                       $(gsmatrix_h)    \
                       $(gsmemory_h)    \
                       $(gsstruct_h)    \
                       $(gsrefct_h)     \
                       $(gscspace_h)    \
                       $(gscolor2_h)    \
                       $(gscie_h)       \
                       $(gscrd_h)       \
                       $(pcommand_h)    \
                       $(pccrd_h)
	$(PCLCCC) $(PCLSRC)pccrd.c $(PCLO_)pccrd.$(OBJ)

$(PCLOBJ)pccsbase.$(OBJ): $(PCLSRC)pccsbase.c   \
                          $(gx_h)               \
                          $(math.h)             \
                          $(gstypes.h)          \
                          $(gsmatrix_h)         \
                          $(gsstruct_h)         \
                          $(gsrefct_h)          \
                          $(gscspace_h)         \
                          $(gscolor2_h)         \
                          $(gscie_h)            \
                          $(pcmtx3_h)           \
                          $(pccsbase_h)
	$(PCLCCC) $(PCLSRC)pccsbase.c $(PCLO_)pccsbase.$(OBJ)

$(PCLOBJ)pcdither.$(OBJ): $(PCLSRC)pcdither.c   \
                          $(pcommand_h)         \
                          $(pcpalet_h)          \
                          $(pcdither_h)
	$(PCLCCC) $(PCLSRC)pcdither.c $(PCLO_)pcdither.$(OBJ)

$(PCLOBJ)pcfrgrnd.$(OBJ): $(PCLSRC)pcfrgrnd.c   \
                          $(gx_h)               \
                          $(pcommand_h)         \
                          $(pcfont_h)           \
                          $(pcfrgrnd_h)
	$(PCLCCC) $(PCLSRC)pcfrgrnd.c $(PCLO_)pcfrgrnd.$(OBJ)

$(PCLOBJ)pcht.$(OBJ): $(PCLSRC)pcht.c   \
                      $(gx_h)           \
                      $(math_h)         \
                      $(gsmemory_h)     \
                      $(gsstruct_h)     \
                      $(gsrefct_h)      \
                      $(gsdevice_h)     \
                      $(gxdevice_h)     \
                      $(gdevcmap.h)     \
                      $(pcommand_h)     \
                      $(pcstate_h)      \
                      $(pcdither_h)     \
                      $(pcht_h)
	$(PCLCCC) $(PCLSRC)pcht.c $(PCLO_)pcht.$(OBJ)

$(PCLOBJ)pcident.$(OBJ): $(PCLSRC)pcident.c \
                         $(gx_h)            \
                         $(gsuid_h)         \
                         $(pcident_h)
	$(PCLCCC) $(PCLSRC)pcident.c $(PCLO_)pcident.$(OBJ)

$(PCLOBJ)pcindexed.$(OBJ): $(PCLSRC)pcindexed.c \
                           $(gx_h)              \
                           $(math_h)            \
                           $(pcmtx3_h)          \
                           $(pccid_h)           \
                           $(pccsbase_h)        \
                           $(pcindexed_h)       \
                           $(pcpalet_h)
	$(PCLCCC) $(PCLSRC)pcindexed.c $(PCLO_)pcindexed.$(OBJ)

$(PCLOBJ)pclookup.$(OBJ): $(PCLSRC)pclookup.c   \
                          $(pcpalet_h)          \
                          $(pclookup_h)
	$(PCLCCC) $(PCLSRC)pclookup.c $(PCLO_)pclookup.$(OBJ)

$(PCLOBJ)pcmtx3.$(OBJ): $(PCLSRC)pcmtx3.c   \
                        $(gx_h)             \
                        $(math_h)           \
                        $(gstypes_h)        \
                        $(pcommand_h)       \
                        $(pcmtx3_h)
	$(PCLCCC) $(PCLSRC)pcmtx3.c $(PCLO_)pcmtx3.$(OBJ)

$(PCLOBJ)pcpalet.$(OBJ): $(PCLSRC)pcpalet.c \
                         $(gx_h)            \
                         $(pldict_h)        \
                         $(pcdraw_h)        \
                         $(pcpage_h)        \
                         $(pcursor_h)       \
                         $(pcpalet_h)       \
                         $(pcfrgrnd_h)
	$(PCLCCC) $(PCLSRC)pcpalet.c $(PCLO_)pcpalet.$(OBJ)

$(PCLOBJ)pcpatrn.$(OBJ): $(PCLSRC)pcpatrn.c \
                         $(gx_h)            \
                         $(gsuid_h)         \
                         $(gsmatrix_h)      \
                         $(gspcolor_h)      \
                         $(pccid_h)         \
                         $(pcfont_h)        \
                         $(pcpalet_h)       \
                         $(pcfrgrnd_h)      \
                         $(pcht_h)          \
                         $(pcwhtindx_h)     \
                         $(pcpatrn_h)       \
                         $(pcbiptrn_h)      \
                         $(pcuptrn_h)       \
                         $(pcpatxfm_h)
	$(PCLCCC) $(PCLSRC)pcpatrn.c $(PCLO_)pcpatrn.$(OBJ)

$(PCLOBJ)pcpatxfm.$(OBJ): $(PCLSRC)pcpatxfm.c   \
                          $(gx_h)               \
                          $(math_h)             \
                          $(pcpatrn_h)          \
                          $(pcfont.h)           \
                          $(pcpatxfm_h)
	$(PCLCCC) $(PCLSRC)pcpatxfm.c $(PCLO_)pcpatxfm.$(OBJ)

$(PCLOBJ)pcuptrn.$(OBJ): $(PCLSRC)pcuptrn.c \
                         $(gx_h)            \
                         $(gscsel.h)        \
                         $(gxdevice.h)      \
                         $(gxpcolor.h)      \
                         $(pldict_h)        \
                         $(pcindexed_h)     \
                         $(pcpatrn_h)       \
                         $(pcbiptrn_h)      \
                         $(pcuptrn_h)
	$(PCLCCC) $(PCLSRC)pcuptrn.c $(PCLO_)pcuptrn.$(OBJ)

$(PCLOBJ)pcwhtindx.$(OBJ): $(PCLSRC)pcwhtindx.c \
                           $(pcstate_h)         \
                           $(pcpalet_h)         \
                           $(pcindexed_h)       \
                           $(pcwhtindx_h)
	$(PCLCCC) $(PCLSRC)pcwhtindx.c $(PCLO_)pcwhtindx.$(OBJ)

# Chapter 6
$(PCLOBJ)rtgmode.$(OBJ):  $(PCLSRC)rtgmode.c    \
                          $(gx_h)               \
                          $(math__h)            \
                          $(gsmatrix_h)         \
                          $(gscoord_h)          \
                          $(gsstate_h)          \
                          $(pcstate_h)          \
                          $(pcpatxfm_h)         \
                          $(pcindexed_h)        \
                          $(pcpalet_h)          \
                          $(pcursor_h)          \
                          $(pcdraw_h)           \
                          $(rtraster_h)         \
                          $(rtrstcmp_h)         \
                          $(rtgmode_h)
	$(PCLCCC) $(PCLSRC)rtgmode.c $(PCLO_)rtgmode.$(OBJ)

$(PCLOBJ)rtrstcmp.$(OBJ): $(PCLSRC)rtrstcmp.c   \
                          $(pcstate_h)          \
                          $(rtrstcmp_h)
	$(PCLCCC) $(PCLSRC)rtrstcmp.c $(PCLO_)rtrstcmp.$(OBJ)


rtlbasec_   = $(PCLOBJ)pcbiptrn.$(OBJ) $(PCLOBJ)pccid.$(OBJ)    \
              $(PCLOBJ)pccolor.$(OBJ) $(PCLOBJ)pccrd.$(OBJ)     \
              $(PCLOBJ)pccsbase.$(OBJ) $(PCLOBJ)pcdither.$(OBJ) \
              $(PCLOBJ)pcfrgrnd.$(OBJ) $(PCLOBJ)pcht.$(OBJ)     \
              $(PCLOBJ)pcident.$(OBJ) $(PCLOBJ)pcindexed.$(OBJ) \
              $(PCLOBJ)pclookup.$(OBJ) $(PCLOBJ)pcmtx3.$(OBJ)   \
              $(PCLOBJ)pcpalet.$(OBJ) $(PCLOBJ)pcpatrn.$(OBJ)   \
              $(PCLOBJ)pcpatxfm.$(OBJ) $(PCLOBJ)pcuptrn.$(OBJ)  \
              $(PCLOBJ)pcwhtindx.$(OBJ) $(PCLOBJ)rtgmode.$(OBJ) \
              $(PCLOBJ)rtrstcmp.$(OBJ)

$(PCLOBJ)rtlbasec.dev: $(PCL_MAK) $(ECHOGS_XE) $(rtlbasec_) $(PCLOBJ)rtlbase.dev
	$(SETMOD) $(PCLOBJ)rtlbasec $(rtlbasec_)
	$(ADDMOD) $(PCLOBJ)rtlbasec -include $(PCLOBJ)rtlbase
	$(ADDMOD) $(PCLOBJ)rtlbasec -init pcl_cid pcl_color pcl_udither
	$(ADDMOD) $(PCLOBJ)rtlbasec -init pcl_frgrnd pcl_lookup_tbl
	$(ADDMOD) $(PCLOBJ)rtlbasec -init pcl_palette pcl_pattern
	$(ADDMOD) $(PCLOBJ)rtlbasec -init pcl_xfm pcl_upattern
	$(ADDMOD) $(PCLOBJ)rtlbasec -init rtgmode

################ HP RTL ################

# HP RTL has just a few commands that aren't in PCL5e, and also supports
# CCITT fax compression modes.

$(PCLOBJ)rtlrastr.$(OBJ): $(PCLSRC)rtlrastr.c   \
                          $(std_h)              \
                          $(pcommand_h)         \
                          $(pcstate_h)          \
                          $(rtgmode_h)
	$(PCLCCC) $(PCLSRC)rtlrastr.c $(PCLO_)rtlrastr.$(OBJ)

HPRTL_OPS   = $(PCLOBJ)rtlrastr.$(OBJ)

$(PCLOBJ)hprtl.dev: $(PCL_MAK) $(ECHOGS_XE) $(HPRTL_OPS) $(PCLOBJ)rtlbase.dev
	$(SETMOD) $(PCLOBJ)hprtl $(HPRTL_OPS)
	$(ADDMOD) $(PCLOBJ)hprtl -include $(PCLOBJ)rtlbase
	$(ADDMOD) $(PCLOBJ)hprtl -init rtlrastr

$(PCLOBJ)hprtlc.dev: $(PCL_MAK) $(ECHOGS_XE) $(PCLOBJ)rtlbasec.dev
	$(SETMOD) $(PCLOBJ)hprtlc -include $(PCLOBJ)rtlbasec

################ PCL 5e/5c ################

#### Shared support

pcfsel_h    = $(PCLSRC)pcfsel.h   \
              $(pcstate_h)

pcsymbol_h  = $(PCLSRC)pcsymbol.h \
              $(plsymbol_h)

# Font selection is essentially identical in PCL and HP-GL/2.
$(PCLOBJ)pcfsel.$(OBJ): $(PCLSRC)pcfsel.c   \
                        $(stdio__h)         \
                        $(gdebug_h)         \
                        $(pcommand_h)       \
                        $(pcstate_h)        \
                        $(pcfont_h)         \
                        $(pcfsel_h)         \
                        $(pcsymbol_h)
	$(PCLCCC) $(PCLSRC)pcfsel.c $(PCLO_)pcfsel.$(OBJ)

PCL_COMMON  = $(PCLOBJ)pcfsel.$(OBJ)

#### PCL5(e) commands
# These are organized by chapter # in the PCL 5 Technical Reference Manual.

# Chapter 4
# Some of these replace implementations in rtmisc.c.
$(PCLOBJ)pcjob.$(OBJ): $(PCLSRC)pcjob.c \
                       $(gx_h)          \
                       $(gsmemory_h)    \
                       $(gsmatrix_h)    \
                       $(gsdevice_h)    \
                       $(pcommand_h)    \
                       $(pcstate_h)     \
                       $(pcparam_h)     \
                       $(pcdraw_h)      \
                       $(pcpage.h)
	$(PCLCCC) $(PCLSRC)pcjob.c $(PCLO_)pcjob.$(OBJ)

# Chapter 5
$(PCLOBJ)pcpage.$(OBJ): $(PCLSRC)pcpage.c   \
                        $(std_h)            \
                        $(pcommand_h)       \
                        $(pcstate_h)        \
                        $(pcdraw_h)         \
                        $(pcparam_h)        \
                        $(pcparse_h)        \
                        $(pcfont_h)         \
                        $(pcpatxfm_h)       \
                        $(pcursor_h)        \
                        $(pcpage_h)         \
                        $(pgmand_h)         \
                        $(pginit_h)         \
                        $(gsmatrix_h)       \
                        $(gscoord_h)        \
                        $(gsdevice_h)       \
                        $(gspaint_h)        \
                        $(gxdevice_h)       \
                        $(gdevbbox_h)
	$(PCLCCC) $(PCLSRC)pcpage.c $(PCLO_)pcpage.$(OBJ)

# Chapter 6
# Some of these replace implementations in rtcursor.c.
$(PCLOBJ)pcursor.$(OBJ): $(PCLSRC)pcursor.c \
                         $(std_h)           \
                         $(math__h)         \
                         $(pcommand_h)      \
                         $(pcstate_h)       \
                         $(pcdraw_h)        \
                         $(pcpatxfm_h)      \
                         $(pcfont_h)        \
                         $(pcursor_h)       \
                         $(pcpage_h)        \
                         $(gscoord_h)
	$(PCLCCC) $(PCLSRC)pcursor.c $(PCLO_)pcursor.$(OBJ)

# Chapter 8
$(PCLOBJ)pcfont.$(OBJ): $(PCLSRC)pcfont.c   \
                        $(gx_h)             \
                        $(gsccode_h)        \
                        $(gsmatrix_h)       \
                        $(gxfont_h)         \
                        $(gxfont42_h)       \
                        $(pcommand_h)       \
                        $(pcstate_h)        \
                        $(pcursor_h)        \
                        $(pcfont_h)         \
                        $(pcfsel_h)
	$(PCLCCC) $(PCLSRC)pcfont.c $(PCLO_)pcfont.$(OBJ)

$(PCLOBJ)pclfont.$(OBJ): $(PCLSRC)pclfont.c \
                         $(stdio__h)        \
                         $(string__h)       \
                         $(gx_h)            \
                         $(gp_h)            \
                         $(gsccode_h)       \
                         $(gsmatrix_h)      \
                         $(gsutil_h)        \
                         $(gxfont_h)        \
                         $(gxfont42_h)      \
                         $(pcommand_h)      \
                         $(pcstate_h)
	$(PCLCCC) $(PCLSRC)pclfont.c $(PCLO_)pclfont.$(OBJ)

$(PCLOBJ)pctext.$(OBJ): $(PCLSRC)pctext.c   \
                        $(gx_h)             \
                        $(gsimage_h)        \
                        $(plvalue_h)        \
                        $(plvocab_h)        \
                        $(pcommand_h)       \
                        $(pcstate_h)        \
                        $(pcdraw_h)         \
                        $(pcfont_h)         \
                        $(pcursor_h)        \
                        $(gdebug_h)         \
                        $(gscoord_h)        \
                        $(gsline_h)         \
                        $(gspaint_h)        \
                        $(gspath_h)         \
                        $(gspath2_h)        \
                        $(gsrop)            \
                        $(gsstate.h)        \
                        $(gxchar_h)         \
                        $(gxfont_h)         \
                        $(gxstate_h)
	$(PCLCCC) $(PCLSRC)pctext.c $(PCLO_)pctext.$(OBJ)

# Chapter 10
$(PCLOBJ)pcsymbol.$(OBJ): $(PCLSRC)pcsymbol.c   \
                          $(stdio__h)           \
                          $(plvalue_h)          \
                          $(pcommand_h)         \
                          $(pcstate_h)          \
                          $(pcfont_h)           \
                          $(pcsymbol_h)
	$(PCLCCC) $(PCLSRC)pcsymbol.c $(PCLO_)pcsymbol.$(OBJ)

$(PCLOBJ)pcsfont.$(OBJ): $(PCLSRC)pcsfont.c \
                         $(memory__h)       \
                         $(stdio__h)        \
                         $(math__h)         \
                         $(pcommand_h)      \
                         $(pcfont_h)        \
                         $(pcstate_h)       \
                         $(pcfsel_h)        \
                         $(pldict_h)        \
                         $(plvalue_h)       \
                         $(gsbitops_h)      \
                         $(gsccode_h)       \
                         $(gsmatrix_h)      \
                         $(gsutil_h)        \
                         $(gxfont_h)        \
                         $(gxfont42_h)
	$(PCLCCC) $(PCLSRC)pcsfont.c $(PCLO_)pcsfont.$(OBJ)

# Chapter 12
$(PCLOBJ)pcmacros.$(OBJ): $(PCLSRC)pcmacros.c   \
                          $(stdio__h)           \
                          $(pcommand_h)         \
                          $(pcstate_h)          \
                          $(pcparse_h) 
	$(PCLCCC) $(PCLSRC)pcmacros.c $(PCLO_)pcmacros.$(OBJ)

# Chapter 14
$(PCLOBJ)pcrect.$(OBJ): $(PCLSRC)pcrect.c   \
                        $(math__h)          \
                        $(pcommand_h)       \
                        $(pcstate_h)        \
                        $(pcdraw_h)         \
                        $(pcpatrn_h)        \
                        $(pcbiptrn_h)       \
                        $(pcuptrn_h)        \
                        $(gspath_h)         \
                        $(gspath2_h)        \
                        $(gsmatrix_h)       \
                        $(gscoord_h)        \
                        $(gspaint_h)        \
                        $(gsrop_h)
	$(PCLCCC) $(PCLSRC)pcrect.c $(PCLO_)pcrect.$(OBJ)

# Chapter 15
# All of these are in rtraster.c, but some of them are only registered
# in PCL5 mode.

# Chapter 16
$(PCLOBJ)pcstatus.$(OBJ): $(PCLSRC)pcstatus.c   \
                          $(memory__h)          \
                          $(stdio__h)           \
                          $(string__h)          \
                          $(gsmemory_h)         \
                          $(gsmalloc_h)         \
                          $(pcommand_h)         \
                          $(pcstate_h)          \
                          $(pcfont_h)           \
                          $(pcsymbol_h)         \
                          $(pcpatrn_h)          \
                          $(pcuptrn_h)          \
                          $(pcpage_h)           \
                          $(pcursor_h)          \
                          $(stream_h)
	$(PCLCCC) $(PCLSRC)pcstatus.c $(PCLO_)pcstatus.$(OBJ)

# Chapter 24
$(PCLOBJ)pcmisc.$(OBJ): $(PCLSRC)pcmisc.c   \
                        $(std_h)            \
                        $(pcommand_h)       \
                        $(pcstate_h)
	$(PCLCCC) $(PCLSRC)pcmisc.c $(PCLO_)pcmisc.$(OBJ)

PCL5_OPS1   = $(PCLOBJ)pcjob.$(OBJ) $(PCLOBJ)pcpage.$(OBJ)      \
              $(PCLOBJ)pcursor.$(OBJ)

PCL5_OPS2   = $(PCLOBJ)pcfont.$(OBJ) $(PCLOBJ)pclfont.$(OBJ)    \
              $(PCLOBJ)pctext.$(OBJ)

PCL5_OPS3   = $(PCLOBJ)pcsymbol.$(OBJ)

PCL5_OPS4   = $(PCLOBJ)pcsfont.$(OBJ) $(PCLOBJ)pcmacros.$(OBJ)

PCL5_OPS5   = $(PCLOBJ)pcrect.$(OBJ) $(PCLOBJ)pcstatus.$(OBJ)   \
              $(PCLOBJ)pcmisc.$(OBJ)

PCL5_OPS    = $(PCL5_OPS1) $(PCL5_OPS2) $(PCL5_OPS3) $(PCL5_OPS4) $(PCL5_OPS5)

# Note: we have to initialize the cursor after initializing the logical
# page dimensions, so we do it last.  This is a hack.
$(PCLOBJ)pcl5.dev: $(PCL_MAK) $(ECHOGS_XE) $(PCL_COMMON) $(PCL5_OPS) \
                   $(PCLOBJ)pcl5base.dev $(PCLOBJ)rtlbase.dev
	$(SETMOD) $(PCLOBJ)pcl5 $(PCL_COMMON)
	$(ADDMOD) $(PCLOBJ)pcl5 $(PCL5_OPS1)
	$(ADDMOD) $(PCLOBJ)pcl5 $(PCL5_OPS2)
	$(ADDMOD) $(PCLOBJ)pcl5 $(PCL5_OPS3)
	$(ADDMOD) $(PCLOBJ)pcl5 $(PCL5_OPS4)
	$(ADDMOD) $(PCLOBJ)pcl5 $(PCL5_OPS5)
	$(ADDMOD) $(PCLOBJ)pcl5 -include $(PCLOBJ)rtlbase
	$(ADDMOD) $(PCLOBJ)pcl5 -init pcjob pcpage pcfont pctext
	$(ADDMOD) $(PCLOBJ)pcl5 -init pcsymbol pcsfont pcmacros
	$(ADDMOD) $(PCLOBJ)pcl5 -init pcrect rtraster pcstatus
	$(ADDMOD) $(PCLOBJ)pcl5 -init pcmisc
	$(ADDMOD) $(PCLOBJ)pcl5 -init   pcursor

#### PCL5c commands
# These are organized by chapter # in the PCL 5 Color Technical Reference
# Manual.

# Chapter 5
$(PCLOBJ)pccprint.$(OBJ): $(PCLSRC)pccprint.c   \
                          $(std_h)              \
                          $(pcommand_h)         \
                          $(pcstate_h)          \
                          $(pcfont_h)           \
                          $(gsmatrix_h)         \
                          $(gsstate_h)          \
                          $(gsrop_h)
	$(PCLCCC) $(PCLSRC)pccprint.c $(PCLO_)pccprint.$(OBJ)

# Chapter 6
# All of these are in rtgmode.c, but some of them are only registered
# in PCL5 mode.

PCL5C_OPS   = $(PCLOBJ)pccprint.$(OBJ)

$(PCLOBJ)pcl5c.dev: $(PCL_MAK) $(ECHOGS_XE) $(PCL5C_OPS) \
                    $(PCLOBJ)pcl5.dev $(PCLOBJ)rtlbasec.dev
	$(SETMOD) $(PCLOBJ)pcl5c $(PCL5C_OPS)
	$(ADDMOD) $(PCLOBJ)pcl5c -include $(PCLOBJ)pcl5 $(PCLOBJ)rtlbasec
	$(ADDMOD) $(PCLOBJ)pcl5c -init pccprint rtgmode

################ HP-GL/2 ################

pgdraw_h    = $(PCLSRC)pgdraw.h

pgfdata_h   = $(PCLSRC)pgfdata.h

pgfont_h    = $(PCLSRC)pgfont.h

pggeom_h    = $(PCLSRC)pggeom.h   \
              $(math__h)          \
              $(gstypes.h)

pginit_h    = $(PCSRC)pginit.h    \
              $(gx_h)             \
              $(pcstate_h)        \
              $(pcommand_h)       \
              $(pgmand_h)

pgmisc_h    = $(PCLSRC)pgmisc.h

#### HP-GL/2 non-commands

# Utilities

$(PCLOBJ)pgdraw.$(OBJ): $(PCLSRC)pgdraw.c \
                        $(stdio__h)       \
                        $(math__h)        \
                        $(gdebug_h)       \
                        $(gstypes_h)      \
                        $(gsmatrix_h)     \
                        $(gsmemory_h)     \
                        $(gsstate_h)      \
                        $(gscoord_h)      \
                        $(gspath_h)       \
                        $(gspaint_h)      \
                        $(gsrop_h)        \
                        $(gxfarith_h)     \
                        $(gxfixed_h)      \
                        $(pgmand_h)       \
                        $(pgdraw_h)       \
                        $(pggeom_h)       \
                        $(pgmisc_h)       \
                        $(pcdraw_h)       \
                        $(pcpalet_h)      \
                        $(pcpatrn_h)
	$(PCLCCC) $(PCLSRC)pgdraw.c $(PCLO_)pgdraw.$(OBJ)

$(PCLOBJ)pggeom.$(OBJ): $(PCLSRC)pggeom.c \
                        $(stdio__h)       \
                        $(pggeom_h)       \
                        $(gxfarith_h)
	$(PCLCCC) $(PCLSRC)pggeom.c $(PCLO_)pggeom.$(OBJ)

$(PCLOBJ)pgmisc.$(OBJ): $(PCLSRC)pgmisc.c \
                        $(pgmand_h)       \
                        $(pgmisc_h)
	$(PCLCCC) $(PCLSRC)pgmisc.c $(PCLO_)pgmisc.$(OBJ)

# Initialize/reset.  We break this out simply because it's easier to keep
# track of it this way.

$(PCLOBJ)pginit.$(OBJ): $(PCLSRC)pginit.c \
                        $(gx_h)           \
                        $(gsmatrix_h)     \
                        $(gsmemory_h)     \
                        $(gsstate_h)      \
                        $(pgmand_h)       \
                        $(pginit_h)       \
                        $(pgdraw_h)       \
                        $(pgmisc_h)       \
                        $(pcpatrn_h)
	$(PCLCCC) $(PCLSRC)pginit.c $(PCLO_)pginit.$(OBJ)

# Parsing and utilities

$(PCLOBJ)pgparse.$(OBJ): $(PCLSRC)pgparse.c \
                         $(AK)              \
                         $(math__h)         \
                         $(stdio__h)        \
                         $(gdebug_h)        \
                         $(gstypes_h)       \
                         $(scommon_h)       \
                         $(pgmand_h)
	$(PCLCCC) $(PCLSRC)pgparse.c $(PCLO_)pgparse.$(OBJ)

HPGL2_OTHER1    = $(PCLOBJ)pgdraw.$(OBJ) $(PCLOBJ)pggeom.$(OBJ) \
                  $(PCLOBJ)pginit.$(OBJ)
HPGL2_OTHER2    = $(PCLOBJ)pgparse.$(OBJ) $(PCLOBJ)pgmisc.$(OBJ)
HPGL2_OTHER     = $(HPGL2_OTHER1) $(HPGL2_OTHER2)

#### HP-GL/2 commands
# These are organized by chapter # in the PCL 5 Technical Reference Manual.

# Chapter 18
# These are PCL commands, but are only relevant to HP RTL and/or HP-GL/2.
# Some of these are in rtmisc.c.
$(PCLOBJ)pgframe.$(OBJ): $(PCLSRC)pgframe.c \
                         $(math__h)         \
                         $(pgmand_h)        \
                         $(pgdraw_h)        \
                         $(pgmisc_h)        \
                         $(gstypes_h)       \
                         $(gsmatrix_h)      \
                         $(gsmemory_h)      \
                         $(gsstate_h)       \
                         $(pcdraw_h)        \
                         $(pcfont_h)        \
                         $(pcstate_h)
	$(PCLCCC) $(PCLSRC)pgframe.c $(PCLO_)pgframe.$(OBJ)

# Chapter 19
$(PCLOBJ)pgconfig.$(OBJ): $(PCLSRC)pgconfig.c \
                          $(gx_h)             \
                          $(gsmatrix_h)       \
                          $(gsmemory_h)       \
                          $(gsstate_h)        \
                          $(gscoord_h)        \
                          $(pgmand_h)         \
                          $(pgdraw_h)         \
                          $(pginit_h)         \
                          $(pggeom_h)         \
                          $(pgmisc_h)         \
                          $(pcpalet_h)        \
                          $(pcdraw_h)
	$(PCLCCC) $(PCLSRC)pgconfig.c $(PCLO_)pgconfig.$(OBJ)

# Chapter 20
$(PCLOBJ)pgvector.$(OBJ): $(PCLSRC)pgvector.c \
                          $(stdio__h)         \
                          $(gdebug_h)         \
                          $(pgmand_h)         \
                          $(pggeom_h)         \
                          $(pgdraw_h)         \
                          $(pgmisc_h)         \
                          $(gspath_h)         \
                          $(gscoord_h)        \
                          $(math__h)
	$(PCLCCC) $(PCLSRC)pgvector.c $(PCLO_)pgvector.$(OBJ)

# Chapter 21
$(PCLOBJ)pgpoly.$(OBJ): $(PCLSRC)pgpoly.c \
                        $(std_h)          \
                        $(pgmand_h)       \
                        $(pgdraw_h)       \
                        $(pggeom_h)       \
                        $(pgmisc_h)       \
                        $(pcpatrn_h)
	$(PCLCCC) $(PCLSRC)pgpoly.c $(PCLO_)pgpoly.$(OBJ)

# Chapter 22
$(PCLOBJ)pglfill.$(OBJ): $(PCLSRC)pglfill.c \
                         $(memory__h)       \
                         $(pgmand_h)        \
                         $(pginit_h)        \
                         $(pggeom_h)        \
                         $(pgdraw_h)        \
                         $(pgmisc_h)        \
                         $(pcdraw_h)        \
                         $(gsuid_h)         \
                         $(gstypes_h)       \
                         $(gsstate_h)       \
                         $(gsrop_h)         \
                         $(gxbitmap_h)      \
                         $(pcpalet_h)       \
                         $(pcpatrn_h)
	$(PCLCCC) $(PCLSRC)pglfill.c $(PCLO_)pglfill.$(OBJ)

# Chapter 23
$(PCLOBJ)pgchar.$(OBJ): $(PCLSRC)pgchar.c \
                        $(math__h)        \
                        $(stdio__h)       \
                        $(gdebug_h)       \
                        $(pgmand_h)       \
                        $(pginit_h)       \
                        $(pggeom_h)       \
                        $(pgmisc_h)       \
                        $(pcfsel_h)       \
                        $(pcpalet_h)
	$(PCLCCC) $(PCLSRC)pgchar.c $(PCLO_)pgchar.$(OBJ)

$(PCLOBJ)pglabel.$(OBJ): $(PCLSRC)pglabel.c  \
                         $(math__h)          \
                         $(memory__h)        \
                         $(ctype__h)         \
                         $(stdio__h)         \
                         $(gdebug_h)         \
                         $(plvalue_h)        \
                         $(pgmand_h)         \
                         $(pginit_h)         \
                         $(pgfont_h)         \
                         $(pgdraw_h)         \
                         $(pggeom_h)         \
                         $(pgmisc_h)         \
                         $(pcfsel_h)         \
                         $(pcsymbol_h)       \
                         $(pcpalet_h)        \
                         $(pcdraw_h)         \
                         $(gscoord_h)        \
                         $(gsline_h)         \
                         $(gspath_h)         \
                         $(gsutil_h)         \
                         $(gxchar_h)         \
                         $(gxfont_h)         \
                         $(gxstate.h)
	$(PCLCCC) $(PCLSRC)pglabel.c $(PCLO_)pglabel.$(OBJ)

$(PCLOBJ)pgfdata.$(OBJ): $(PCLSRC)pgfdata.c  \
                         $(std_h)            \
                         $(gstypes_h)        \
                         $(gsccode_h)        \
                         $(gxarith_h)        \
                         $(pgfdata_h)        \
                         $(math__h)
	$(PCLCCC) $(PCLSRC)pgfdata.c $(PCLO_)pgfdata.$(OBJ)

$(PCLOBJ)pgfont.$(OBJ): $(PCLSRC)pgfont.c \
                        $(math__h)        \
                        $(gstypes_h)      \
                        $(gsccode_h)      \
                        $(gsmemory_h)     \
                        $(gsstate_h)      \
                        $(gsmatrix_h)     \
                        $(gscoord_h)      \
                        $(gspaint_h)      \
                        $(gspath_h)       \
                        $(gxfixed_h)      \
                        $(gxchar_h)       \
                        $(gxfarith_h)     \
                        $(gxfont_h)       \
                        $(plfont_h)       \
                        $(pgfdata_h)      \
                        $(pgfont_h)
	$(PCLCCC) $(PCLSRC)pgfont.c $(PCLO_)pgfont.$(OBJ)

HPGL2_OPS1  = $(PCLOBJ)pgframe.$(OBJ) $(PCLOBJ)pgconfig.$(OBJ)  \
              $(PCLOBJ)pgvector.$(OBJ)
HPGL2_OPS2  = $(PCLOBJ)pgpoly.$(OBJ) $(PCLOBJ)pglfill.$(OBJ)    \
              $(PCLOBJ)pgchar.$(OBJ)
HPGL2_OPS3  = $(PCLOBJ)pglabel.$(OBJ) $(PCLOBJ)pgfdata.$(OBJ)   \
              $(PCLOBJ)pgfont.$(OBJ)
HPGL2_OPS   = $(HPGL2_OPS1) $(HPGL2_OPS2) $(HPGL2_OPS3)

$(PCLOBJ)hpgl2.dev: $(PCL_MAK) $(ECHOGS_XE) $(PCL_COMMON) \
                    $(HPGL2_OTHER) $(HPGL2_OPS)
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

$(PCLOBJ)pgcolor.$(OBJ): $(PCLSRC)pgcolor.c \
                         $(std_h)           \
                         $(pgmand_h)        \
                         $(pginit_h)        \
                         $(pgmisc_h)        \
                         $(pgdraw_h)        \
                         $(gsstate_h)       \
                         $(pcpalet_h)
	$(PCLCCC) $(PCLSRC)pgcolor.c $(PCLO_)pgcolor.$(OBJ)

HPGL2C_OPS  = $(PCLOBJ)pgcolor.$(OBJ)

$(PCLOBJ)hpgl2c.dev: $(PCL_MAK) $(ECHOGS_XE) $(HPGL2C_OPS) $(PCLOBJ)hpgl2.dev
	$(SETMOD) $(PCLOBJ)hpgl2c $(HPGL2C_OPS)
	$(ADDMOD) $(PCLOBJ)hpgl2c -include $(PCLOBJ)hpgl2
	$(ADDMOD) $(PCLOBJ)hpgl2c -init pgcolor
