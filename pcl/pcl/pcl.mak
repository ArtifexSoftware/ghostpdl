# Copyright (C) 2001-2019 Artifex Software, Inc.
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
# Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
# CA 94945, U.S.A., +1(415)492-9861, for further information.
#

# makefile for PCL5*, HP RTL, and HP-GL/2 interpreters
# Users of this makefile must define the following:
#	GSSRCDIR - the GS library source directory
#	PLSRCDIR - the PCL* support library source directory
#	PLOBJDIR - the object directory for the PCL support library
#	PCL5SRCDIR - the source directory
#	PCL5GENDIR - the directory for source files generated during building
#	PCL5OBJDIR - the object / executable directory
# PLOBJ       = $(PLOBJDIR)$(D)

PCLSRC      = $(PCL5SRCDIR)$(D)
PCLGEN      = $(PCL5GENDIR)$(D)
PCLOBJ      = $(PCL5OBJDIR)$(D)
PCLO_       = $(O_)$(PCLOBJ)

PCLCCC  = $(CC_) $(I_)$(PCL5SRCDIR)$(_I) $(I_)$(PCL5GENDIR)$(_I) $(I_)$(PLSRCDIR)$(_I) $(I_)$(GLSRCDIR)$(_I) $(C_)

# Define the name of this makefile.
PCL_MAK     = $(PCLSRC)pcl.mak $(TOP_MAKEFILES)

pcl.clean: pcl.config-clean pcl.clean-not-config-clean

pcl.clean-not-config-clean:
	$(RM_) $(PCLOBJ)*.$(OBJ)

pcl.config-clean: clean_gs
	$(RM_) $(PCLOBJ)*.dev
	$(RM_) $(PCLOBJ)devs.tr5

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
              $(gsimage_h)        \
              $(pccoord_h)

pcmtx3_h    = $(PCLSRC)pcmtx3.h   \
	      $(math__h)          \
              $(gx_h)             \
              $(gsmatrix_h)       \
              $(gscspace_h)       \
              $(gscolor2_h)       \
              $(gscie_h)

pcommand_h  = $(PCLSRC)pcommand.h \
              $(memory__h)        \
              $(gx_h)             \
              $(gserrors_h)

pcstate_h   = $(PCLSRC)pcstate.h  \
              $(gx_h)             \
	      $(gxdevice_h)	  \
              $(scommon_h)        \
              $(gscspace_h)       \
              $(gscolor2_h)       \
              $(gscrd_h)          \
              $(gsdcolor_h)       \
              $(gschar_h)         \
              $(pldict_h)         \
              $(plfont_h)         \
              $(pccoord_h)        \
              $(pcxfmst_h)        \
              $(pcfontst_h)       \
              $(pctpm_h)          \
              $(pcpattyp_h)       \
              $(pcdict_h)         \
              $(rtrstst_h)        \
              $(pcht_h)           \
              $(pcident_h)        \
              $(pccsbase_h)       \
              $(pjtop_h)          \
              $(pgstate_h)

# the next two are out of place because pginit_h is needed by pcl,
# pginit_h in turn requires pgmand_h

pgmand_h    = $(PCLSRC)pgmand.h   \
              $(stdio__h)         \
              $(gdebug_h)         \
              $(pcommand_h)       \
              $(pcstate_h)

pginit_h    = $(PCLSRC)pginit.h   \
              $(gx_h)             \
              $(pcstate_h)        \
              $(pcommand_h)       \
              $(pgmand_h)


pccid_h     = $(PCLSRC)pccid.h    \
              $(gx_h)             \
              $(gsstruct_h)       \
              $(pcommand_h)

pcdither_h  = $(PCLSRC)pcdither.h \
              $(gx_h)             \
              $(gsstruct_h)       \
              $(gsrefct_h)        \
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
              $(pcommand_h)       \
              $(pccid_h)          \
              $(pclookup_h)

pcht_h      = $(PCLSRC)pcht.h     \
              $(gx_h)             \
              $(gsstruct_h)       \
              $(gsrefct_h)        \
              $(gsht1_h)          \
              $(gshtx_h)          \
              $(pcident_h)        \
              $(pcommand_h)       \
              $(pclookup_h)       \
              $(pcdither_h)

pcindxed_h = $(PCLSRC)pcindxed.h  \
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
              $(pcindxed_h)       \
              $(pcht_h)

pcfrgrnd_h  = $(PCLSRC)pcfrgrnd.h \
              $(gx_h)             \
              $(gsstruct_h)       \
              $(gsrefct_h)        \
              $(pcstate_h)        \
              $(pcommand_h)       \
              $(pccsbase_h)       \
              $(pcht_h)           \
              $(pcpalet_h)

pcpage_h    = $(PCLSRC)pcpage.h   \
              $(pcstate_h)        \
              $(pcommand_h)

pcparam_h   = $(PCLSRC)pcparam.h  \
              $(gsparam_h)

pcparse_h   = $(PCLSRC)pcparse.h  \
              $(gsmemory_h)       \
              $(scommon_h)        \
              $(pcstate_h)        \
              $(pcommand_h)

pcpatrn_h   = $(PCLSRC)pcpatrn.h  \
              $(gx_h)             \
              $(gsstruct_h)       \
              $(gsrefct_h)        \
              $(pcindxed_h)      \
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

pctop_h	    = $(PCLSRC)pctop.h

pcuptrn_h   = $(PCLSRC)pcuptrn.h  \
              $(gx_h)             \
              $(pcommand_h)       \
              $(pcpatrn_h)

pcursor_h   = $(PCLSRC)pcursor.h  \
              $(gx_h)             \
              $(pcstate_h)        \
              $(pcommand_h)

pcwhtidx_h = $(PCLSRC)pcwhtidx.h\
              $(gx_h)             \
              $(gsbitmap_h)       \
              $(pcindxed_h)

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

rtmisc_h  = $(PCLSRC)rtmisc.h

$(PCLOBJ)pcommand.$(OBJ): $(PCLSRC)pcommand.c   \
                          $(std_h)              \
	                  $(memory__h)          \
                          $(gstypes_h)          \
                          $(gsmemory_h)         \
                          $(gsmatrix_h)         \
                          $(gxstate_h)          \
                          $(gsdevice_h)         \
                          $(pcindxed_h)         \
                          $(pcommand_h)         \
                          $(pcparse_h)          \
                          $(pcstate_h)          \
                          $(pcparam_h)          \
                          $(pcident_h)		\
                          $(pgmand_h) 		\
                          $(PCL_MAK) 		\
                          $(MAKEDIRS)
	$(PCLCCC) $(PCLSRC)pcommand.c $(PCLO_)pcommand.$(OBJ)

$(PCLOBJ)pcdraw.$(OBJ): $(PCLSRC)pcdraw.c   \
                        $(gx_h)             \
                        $(gsmatrix_h)       \
                        $(gscoord_h)        \
                        $(gspath_h)         \
                        $(gsstate_h)        \
                        $(gsrop_h)          \
                        $(gxfixed_h)        \
                        $(pcstate_h)        \
                        $(pcht_h)           \
                        $(pcpatrn_h)        \
                        $(pcdraw_h)         \
                        $(PCL_MAK)          \
                        $(MAKEDIRS)
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
                         $(rtgmode_h)       \
                         $(rtmisc_h)        \
                         $(PCL_MAK)         \
                         $(MAKEDIRS)
	$(PCLCCC) $(PCLSRC)pcparse.c $(PCLO_)pcparse.$(OBJ)

PCL5_PARSE  = $(PCLOBJ)pcommand.$(OBJ) $(PCLOBJ)pcparse.$(OBJ)

# PCL5_OTHER should include $(GD)stream.$(OBJ), but that is included
# automatically anyway.
PCL5_OTHER  = $(PCL5_PARSE) $(PCLOBJ)pcdraw.$(OBJ)

$(PCLOBJ)pcl5base.dev: $(ECHOGS_XE) $(PCL5_OTHER) $(PLOBJ)$(PCL_FONT_SCALER).dev \
                       $(PCL_MAK) $(MAKEDIRS)
	$(SETMOD) $(PCLOBJ)pcl5base $(PCL5_OTHER)
	$(ADDMOD) $(PCLOBJ)pcl5base -include $(PLOBJ)pl $(PLOBJ)pjl $(PLOBJ)$(PCL_FONT_SCALER)
	$(ADDMOD) $(PCLOBJ)pcl5base -init pcparse

################ Raster graphics base ################

# This is the intersection of HP RTL and PCL5e/5C.
# We separate this out because HP RTL isn't *quite* a subset of PCL5.


#### Monochrome commands
# These are organized by chapter # in the PCL 5 Technical Reference Manual.

rtraster_h  = $(PCLSRC)rtraster.h   \
              $(pcstate_h)          \
              $(pcommand_h)

# out of place because these are needed by rtmisc.h
pgmisc_h    = $(PCLSRC)pgmisc.h
pgdraw_h    = $(PCLSRC)pgdraw.h


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
                        $(pcdraw_h)         \
                        $(rtmisc_h)         \
                        $(PCL_MAK)          \
                        $(MAKEDIRS)
	$(PCLCCC) $(PCLSRC)rtmisc.c $(PCLO_)rtmisc.$(OBJ)

# Chapter 15
$(PCLOBJ)rtraster.$(OBJ): $(PCLSRC)rtraster.c   \
			  $(memory__h)          \
                          $(strimpl_h)          \
                          $(scf_h)              \
                          $(scfx_h)             \
                          $(stream_h)           \
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
                          $(pcpage_h)           \
                          $(pcindxed_h)         \
                          $(pcwhtidx_h)         \
                          $(pcdraw_h)           \
                          $(plvalue_h)          \
                          $(rtgmode_h)          \
                          $(rtrstcmp_h)         \
                          $(rtraster_h)         \
                          $(PCL_MAK)            \
                          $(MAKEDIRS)
	$(PCLCCC) $(PCLSRC)rtraster.c $(PCLO_)rtraster.$(OBJ)

rtlbase_    = $(PCLOBJ)rtmisc.$(OBJ) $(PCLOBJ)rtraster.$(OBJ)

$(PCLOBJ)rtlbase.dev: $(ECHOGS_XE) $(rtlbase_) $(PCLOBJ)pcl5base.dev         \
                      $(PCL_MAK) $(MAKEDIRS)
	$(SETMOD) $(PCLOBJ)rtlbase $(rtlbase_)
	$(ADDMOD) $(PCLOBJ)rtlbase -include $(PCLOBJ)pcl5base
	$(ADDMOD) $(PCLOBJ)rtlbase -init rtmisc rtraster

#### Color commands
# These are organized by chapter # in the PCL 5 Color Technical Reference
# Manual. (These are no longer separable from the base PCL set)

# Chapter 2, 3, 4, 5
$(PCLOBJ)pcbiptrn.$(OBJ): $(PCLSRC)pcbiptrn.c   \
			  $(math__h)            \
			  $(string__h)          \
			  $(gstypes_h)		\
			  $(gsmatrix_h)         \
			  $(gsmemory_h)		\
			  $(gsstate_h)		\
			  $(gscoord_h)          \
                          $(pcpatrn_h)          \
                          $(pcuptrn_h)          \
                          $(pcbiptrn_h)         \
			  $(pcstate_h)          \
                          $(PCL_MAK)            \
                          $(MAKEDIRS)
	$(PCLCCC) $(PCLSRC)pcbiptrn.c $(PCLO_)pcbiptrn.$(OBJ)

$(PCLOBJ)pccid.$(OBJ): $(PCLSRC)pccid.c     \
                       $(gx_h)              \
                       $(gsmemory_h)        \
                       $(gsstruct_h)        \
                       $(pcommand_h)        \
                       $(pcstate_h)         \
                       $(pcpalet_h)         \
                       $(pccid_h)           \
                       $(PCL_MAK)           \
                       $(MAKEDIRS)
	$(PCLCCC) $(PCLSRC)pccid.c $(PCLO_)pccid.$(OBJ)

$(PCLOBJ)pccolor.$(OBJ): $(PCLSRC)pccolor.c \
                         $(std_h)           \
                         $(pcommand_h)      \
                         $(pcstate_h)       \
                         $(pcpalet_h)       \
                         $(PCL_MAK)         \
                         $(MAKEDIRS)
	$(PCLCCC) $(PCLSRC)pccolor.c $(PCLO_)pccolor.$(OBJ)

$(PCLOBJ)pccsbase.$(OBJ): $(PCLSRC)pccsbase.c   \
                          $(gx_h)               \
                          $(math__h)            \
                          $(gstypes_h)          \
                          $(gsmatrix_h)         \
                          $(gsstruct_h)         \
                          $(gsrefct_h)          \
                          $(gscspace_h)         \
                          $(gscolor2_h)         \
                          $(gscie_h)            \
                          $(pcmtx3_h)           \
                          $(pccsbase_h)         \
                          $(pcstate_h)          \
                          $(PCL_MAK)            \
                          $(MAKEDIRS)
	$(PCLCCC) $(PCLSRC)pccsbase.c $(PCLO_)pccsbase.$(OBJ)

$(PCLOBJ)pcdither.$(OBJ): $(PCLSRC)pcdither.c   \
                          $(pcommand_h)         \
                          $(pcpalet_h)          \
                          $(pcdither_h)         \
                          $(PCL_MAK)            \
                          $(MAKEDIRS)
	$(PCLCCC) $(PCLSRC)pcdither.c $(PCLO_)pcdither.$(OBJ)

$(PCLOBJ)pcfrgrnd.$(OBJ): $(PCLSRC)pcfrgrnd.c   \
                          $(gx_h)               \
                          $(pcommand_h)         \
                          $(pcfont_h)           \
                          $(pcfrgrnd_h)         \
                          $(PCL_MAK)            \
                          $(MAKEDIRS)
	$(PCLCCC) $(PCLSRC)pcfrgrnd.c $(PCLO_)pcfrgrnd.$(OBJ)

$(PCLOBJ)pcht.$(OBJ): $(PCLSRC)pcht.c   \
                      $(gx_h)           \
                      $(math__h)        \
                      $(gsmemory_h)     \
                      $(gsstruct_h)     \
                      $(gsrefct_h)      \
                      $(gsdevice_h)     \
                      $(gsparam_h)      \
                      $(gxdevice_h)     \
                      $(pcommand_h)     \
                      $(pcstate_h)      \
                      $(pcdither_h)     \
                      $(pcht_h)         \
                      $(pcpalet_h)      \
                      $(pcindxed_h)     \
                      $(plht_h)         \
                      $(PCL_MAK)        \
                      $(MAKEDIRS)
	$(PCLCCC) $(PCLSRC)pcht.c $(PCLO_)pcht.$(OBJ)

$(PCLOBJ)pcident.$(OBJ): $(PCLSRC)pcident.c \
                         $(gx_h)            \
                         $(gsuid_h)         \
                         $(pcident_h)	    \
                         $(pcstate_h)       \
                         $(PCL_MAK)         \
                         $(MAKEDIRS)
	$(PCLCCC) $(PCLSRC)pcident.c $(PCLO_)pcident.$(OBJ)

$(PCLOBJ)pcindxed.$(OBJ): $(PCLSRC)pcindxed.c \
                           $(math__h)           \
                           $(string__h)         \
                           $(gx_h)              \
                           $(pcmtx3_h)          \
                           $(pccid_h)           \
                           $(pccsbase_h)        \
                           $(pcpalet_h)         \
                           $(PCL_MAK)           \
                           $(MAKEDIRS)
	$(PCLCCC) $(PCLSRC)pcindxed.c $(PCLO_)pcindxed.$(OBJ)

$(PCLOBJ)pclookup.$(OBJ): $(PCLSRC)pclookup.c   \
                          $(pcpalet_h)          \
                          $(pclookup_h)         \
                          $(PCL_MAK)            \
                          $(MAKEDIRS)
	$(PCLCCC) $(PCLSRC)pclookup.c $(PCLO_)pclookup.$(OBJ)

$(PCLOBJ)pcmtx3.$(OBJ): $(PCLSRC)pcmtx3.c   \
			$(string__h)        \
                        $(math__h)          \
                        $(gx_h)             \
                        $(gstypes_h)        \
                        $(pcommand_h)       \
                        $(pcmtx3_h)         \
                        $(PCL_MAK)          \
                        $(MAKEDIRS)
	$(PCLCCC) $(PCLSRC)pcmtx3.c $(PCLO_)pcmtx3.$(OBJ)

$(PCLOBJ)pcpalet.$(OBJ): $(PCLSRC)pcpalet.c \
                         $(gx_h)            \
                         $(pldict_h)        \
                         $(pcdraw_h)        \
                         $(pcpage_h)        \
                         $(pcursor_h)       \
                         $(pcpalet_h)       \
                         $(pcfrgrnd_h)      \
                         $(gsparam_h)       \
                         $(gsdevice_h)      \
                         $(gxcmap_h)        \
                         $(gxdcconv_h)      \
                         $(gzstate_h)       \
                         $(PCL_MAK)         \
                         $(MAKEDIRS)
	$(PCLCCC) $(PCLSRC)pcpalet.c $(PCLO_)pcpalet.$(OBJ)

$(PCLOBJ)pcpatrn.$(OBJ): $(PCLSRC)pcpatrn.c \
                         $(gx_h)            \
                         $(gsuid_h)         \
                         $(gsmatrix_h)      \
                         $(gspcolor_h)      \
                         $(gxdcolor_h)      \
                         $(gxpcolor_h)      \
                         $(gxstate_h)       \
                         $(pccid_h)         \
                         $(pcfont_h)        \
                         $(pcpalet_h)       \
                         $(pcfrgrnd_h)      \
                         $(pcht_h)          \
                         $(pcwhtidx_h)      \
                         $(pcpatrn_h)       \
                         $(pcbiptrn_h)      \
                         $(pcuptrn_h)       \
                         $(pcpatxfm_h)      \
                         $(PCL_MAK)         \
                         $(MAKEDIRS)
	$(PCLCCC) $(PCLSRC)pcpatrn.c $(PCLO_)pcpatrn.$(OBJ)

$(PCLOBJ)pcpatxfm.$(OBJ): $(PCLSRC)pcpatxfm.c   \
                          $(gx_h)               \
                          $(math__h)            \
                          $(pcpatrn_h)          \
                          $(pcfont_h)           \
                          $(pcpatxfm_h)         \
                          $(PCL_MAK)            \
                          $(MAKEDIRS)
	$(PCLCCC) $(PCLSRC)pcpatxfm.c $(PCLO_)pcpatxfm.$(OBJ)

$(PCLOBJ)pcuptrn.$(OBJ): $(PCLSRC)pcuptrn.c \
		         $(string__h)       \
                         $(gx_h)            \
                         $(gsuid_h)         \
                         $(gscsel_h)        \
                         $(gsdevice_h)      \
                         $(gxdevice_h)      \
                         $(gscspace_h)      \
                         $(gxdcolor_h)      \
                         $(gxcolor2_h)      \
                         $(gxpcolor_h)      \
                         $(pldict_h)        \
                         $(pcindxed_h)      \
                         $(pcpatrn_h)       \
                         $(pcbiptrn_h)      \
                         $(pcuptrn_h)       \
                         $(PCL_MAK)         \
                         $(MAKEDIRS)
	$(PCLCCC) $(PCLSRC)pcuptrn.c $(PCLO_)pcuptrn.$(OBJ)

$(PCLOBJ)pcwhtidx.$(OBJ): $(PCLSRC)pcwhtidx.c \
                           $(pcstate_h)       \
                           $(pcpalet_h)       \
                           $(pcindxed_h)      \
                           $(pcwhtidx_h)      \
                           $(PCL_MAK)         \
                           $(MAKEDIRS)
	$(PCLCCC) $(PCLSRC)pcwhtidx.c $(PCLO_)pcwhtidx.$(OBJ)

# Chapter 6
$(PCLOBJ)rtgmode.$(OBJ):  $(PCLSRC)rtgmode.c    \
                          $(gx_h)               \
                          $(math__h)            \
                          $(gsmatrix_h)         \
                          $(gscoord_h)          \
                          $(gsrect_h)           \
                          $(gsstate_h)          \
                          $(pcstate_h)          \
                          $(pcpatxfm_h)         \
                          $(pcpage_h)           \
                          $(pcindxed_h)         \
                          $(pcpalet_h)          \
                          $(pcursor_h)          \
                          $(pcdraw_h)           \
                          $(rtraster_h)         \
                          $(rtrstcmp_h)         \
                          $(rtgmode_h)          \
                          $(PCL_MAK)            \
                          $(MAKEDIRS)
	$(PCLCCC) $(PCLSRC)rtgmode.c $(PCLO_)rtgmode.$(OBJ)

$(PCLOBJ)rtrstcmp.$(OBJ): $(PCLSRC)rtrstcmp.c   \
                          $(string__h)          \
                          $(pcstate_h)          \
                          $(rtrstcmp_h)         \
                          $(PCL_MAK)            \
                          $(MAKEDIRS)
	$(PCLCCC) $(PCLSRC)rtrstcmp.c $(PCLO_)rtrstcmp.$(OBJ)


rtlbasec_   = $(PCLOBJ)pcbiptrn.$(OBJ) $(PCLOBJ)pccid.$(OBJ)    \
              $(PCLOBJ)pccolor.$(OBJ)                           \
              $(PCLOBJ)pccsbase.$(OBJ) $(PCLOBJ)pcdither.$(OBJ) \
              $(PCLOBJ)pcfrgrnd.$(OBJ) $(PCLOBJ)pcht.$(OBJ)     \
              $(PCLOBJ)pcident.$(OBJ) $(PCLOBJ)pcindxed.$(OBJ)  \
              $(PCLOBJ)pclookup.$(OBJ) $(PCLOBJ)pcmtx3.$(OBJ)   \
              $(PCLOBJ)pcpalet.$(OBJ)  $(PCLOBJ)pcpatrn.$(OBJ)   \
              $(PCLOBJ)pcpatxfm.$(OBJ) $(PCLOBJ)pcuptrn.$(OBJ)  \
              $(PCLOBJ)pcwhtidx.$(OBJ) $(PCLOBJ)rtgmode.$(OBJ)  \
              $(PCLOBJ)rtrstcmp.$(OBJ)

$(PCLOBJ)rtlbasec.dev: $(PCL_MAK) $(ECHOGS_XE) $(rtlbasec_) $(PCLOBJ)rtlbase.dev      \
                         $(PCL_MAK) $(MAKEDIRS)
	$(SETMOD) $(PCLOBJ)rtlbasec $(rtlbasec_)
	$(ADDMOD) $(PCLOBJ)rtlbasec -include $(PCLOBJ)rtlbase
	$(ADDMOD) $(PCLOBJ)rtlbasec -init pcl_cid pcl_color pcl_udither
	$(ADDMOD) $(PCLOBJ)rtlbasec -init pcl_frgrnd pcl_lookup_tbl
	$(ADDMOD) $(PCLOBJ)rtlbasec -init pcl_palette pcl_pattern
	$(ADDMOD) $(PCLOBJ)rtlbasec -init pcl_xfm pcl_upattern
	$(ADDMOD) $(PCLOBJ)rtlbasec -init rtgmode

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
                        $(pcsymbol_h)       \
                        $(plvalue_h)        \
                        $(plftable_h)       \
                        $(PCL_MAK)          \
                        $(MAKEDIRS)
	$(PCLCCC) $(PCLSRC)pcfsel.c $(PCLO_)pcfsel.$(OBJ)

pconfig_h    = $(PCLGEN)pconfig.h

$(PCL_TOP_OBJ):         $(PCLSRC)pctop.c            \
                        $(AK)                       \
                        $(malloc__h)                \
                        $(math__h)                  \
                        $(memory__h)                \
                        $(stdio__h)                 \
                        $(scommon_h)                \
                        $(pcparse_h)                \
                        $(pcpage_h)                 \
                        $(pcstate_h)                \
			$(pldebug_h)		    \
                        $(gdebug_h)                 \
                        $(gsmatrix_h)               \
                        $(gsrop_h)                  \
                        $(gspaint_h)                \
                        $(gsstate_h)                \
                        $(gxalloc_h)                \
                        $(gxdevice_h)               \
                        $(gxstate_h)                \
                        $(pjparse_h)                \
                        $(pltop_h)                  \
                        $(pctop_h)                  \
                        $(pcpalet_h)                \
                        $(rtgmode_h)                \
                        $(gsicc_manage_h)           \
                        $(pconfig_h)                \
                        $(PCL_MAK)                  \
                        $(MAKEDIRS)
	$(PCLCCC) $(PCLSRC)pctop.c $(PCLO_)pctop.$(OBJ)

PCL_COMMON  = $(PCLOBJ)pcfsel.$(OBJ)

#### PCL5(e) commands
# These are organized by chapter # in the PCL 5 Technical Reference Manual.

# Chapter 4
# Some of these replace implementations in rtmisc.c.
$(PCLOBJ)pcjob.$(OBJ): $(PCLSRC)pcjob.c \
		       $(std_h)         \
                       $(gx_h)          \
                       $(gsmemory_h)    \
                       $(gspaint_h)     \
                       $(gsmatrix_h)    \
                       $(gsdevice_h)    \
                       $(pcommand_h)    \
                       $(pcursor_h)     \
                       $(pcstate_h)     \
                       $(pcparam_h)     \
                       $(pcdraw_h)      \
                       $(pcpage_h)      \
                       $(pjtop_h)       \
                       $(plparams_h)    \
                       $(PCL_MAK)       \
                       $(MAKEDIRS)
	$(PCLCCC) $(PCLSRC)pcjob.c $(PCLO_)pcjob.$(OBJ)

# Chapter 5
$(PCLOBJ)pcpage.$(OBJ): $(PCLSRC)pcpage.c   \
                        $(math__h)          \
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
			$(plmain_h)         \
                        $(plvalue_h)        \
                        $(gsmatrix_h)       \
                        $(gscoord_h)        \
                        $(gsdevice_h)       \
                        $(gspaint_h)        \
                        $(gspath_h)         \
                        $(gxdevice_h)       \
                        $(pjtop_h)          \
                        $(rtgmode_h)        \
                        $(PCL_MAK)          \
                        $(MAKEDIRS)
	$(PCLCCC) $(PCLSRC)pcpage.c $(PCLO_)pcpage.$(OBJ)

# Chapter 6
$(PCLOBJ)pcursor.$(OBJ): $(PCLSRC)pcursor.c \
                         $(std_h)          \
                         $(math__h)         \
                         $(pcommand_h)      \
                         $(pcstate_h)       \
                         $(pcdraw_h)        \
                         $(pcpatxfm_h)      \
                         $(pcfont_h)        \
                         $(pcursor_h)       \
                         $(pcpage_h)        \
			 $(gscoord_h)       \
                         $(pjtop_h)         \
                         $(PCL_MAK)         \
                         $(MAKEDIRS)
	$(PCLCCC) $(PCLSRC)pcursor.c $(PCLO_)pcursor.$(OBJ)

# Chapter 8
$(PCLOBJ)pcfont.$(OBJ): $(PCLSRC)pcfont.c   \
			$(std_h)            \
			$(memory__h)        \
                        $(gx_h)             \
                        $(gsccode_h)        \
                        $(gsmatrix_h)       \
                        $(gxfixed_h)        \
                        $(gxchar_h)         \
                        $(gxfcache_h)       \
                        $(gxfont_h)         \
                        $(gxfont42_h)       \
                        $(pcommand_h)       \
                        $(pcstate_h)        \
                        $(pcursor_h)        \
                        $(pcfont_h)         \
                        $(pcfsel_h)         \
                        $(pjtop_h)	    \
			$(pllfont_h)        \
                        $(PCL_MAK)          \
                        $(MAKEDIRS)
	$(PCLCCC) $(PCLSRC)pcfont.c $(PCLO_)pcfont.$(OBJ)

$(PCLOBJ)pctext.$(OBJ): $(PCLSRC)pctext.c   \
                        $(math__h)          \
                        $(gx_h)             \
                        $(gsimage_h)        \
                        $(plvalue_h)        \
                        $(plvocab_h)        \
                        $(pcommand_h)       \
                        $(pcstate_h)        \
                        $(pcdraw_h)         \
                        $(pcfont_h)         \
                        $(pcursor_h)        \
                        $(pcpage_h)         \
                        $(pcfrgrnd_h)       \
                        $(gdebug_h)         \
                        $(gscoord_h)        \
                        $(gsdevice_h)       \
                        $(gxdevsop_h)       \
                        $(gsline_h)         \
                        $(gspaint_h)        \
                        $(gspath_h)         \
                        $(gspath2_h)        \
                        $(gsrop_h)          \
                        $(gsstate_h)        \
                        $(gxchar_h)         \
                        $(gxfont_h)         \
                        $(gxstate_h)        \
                        $(PCL_MAK)          \
                        $(MAKEDIRS)
	$(PCLCCC) $(PCLSRC)pctext.c $(PCLO_)pctext.$(OBJ)

# Chapter 10
$(PCLOBJ)pcsymbol.$(OBJ): $(PCLSRC)pcsymbol.c   \
                          $(stdio__h)           \
                          $(plvalue_h)          \
                          $(pcommand_h)         \
                          $(pcstate_h)          \
                          $(pcfont_h)           \
                          $(pcsymbol_h)         \
                          $(PCL_MAK)            \
                          $(MAKEDIRS)
	$(PCLCCC) $(PCLSRC)pcsymbol.c $(PCLO_)pcsymbol.$(OBJ)

$(PCLOBJ)pcsfont.$(OBJ): $(PCLSRC)pcsfont.c \
                         $(memory__h)       \
                         $(stdio__h)        \
                         $(math__h)         \
                         $(gdebug_h)        \
                         $(pcommand_h)      \
                         $(pcfont_h)        \
                         $(pcursor_h)       \
                         $(pcpage_h)        \
                         $(pcstate_h)       \
                         $(pcfsel_h)        \
                         $(pcparse_h)       \
                         $(pjparse_h)       \
                         $(pldict_h)        \
                         $(plvalue_h)       \
                         $(plmain_h)        \
                         $(gsbitops_h)      \
                         $(gsccode_h)       \
                         $(gsmatrix_h)      \
                         $(gsutil_h)        \
                         $(gxfont_h)        \
                         $(gxfont42_h)      \
                         $(plfapi_h)        \
                         $(PCL_MAK)         \
                         $(MAKEDIRS)
	$(PCLCCC) $(PCLSRC)pcsfont.c $(PCLO_)pcsfont.$(OBJ)

# Chapter 12
$(PCLOBJ)pcmacros.$(OBJ): $(PCLSRC)pcmacros.c   \
                          $(stdio__h)           \
                          $(pcommand_h)         \
                          $(pgmand_h)           \
                          $(pcstate_h)          \
                          $(pcparse_h)          \
                          $(PCL_MAK)            \
                          $(MAKEDIRS)
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
                        $(gsrop_h)          \
                        $(PCL_MAK)          \
                        $(MAKEDIRS)
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
                          $(pcparse_h)          \
                          $(pcpatrn_h)          \
                          $(pcuptrn_h)          \
                          $(pcpage_h)           \
                          $(pcursor_h)          \
                          $(stream_h)           \
                          $(PCL_MAK)            \
                          $(MAKEDIRS)
	$(PCLCCC) $(PCLSRC)pcstatus.c $(PCLO_)pcstatus.$(OBJ)

# Chapter 24
$(PCLOBJ)pcmisc.$(OBJ): $(PCLSRC)pcmisc.c   \
                        $(std_h)            \
                        $(pcommand_h)       \
                        $(pcstate_h)        \
                        $(PCL_MAK)          \
                        $(MAKEDIRS)
	$(PCLCCC) $(PCLSRC)pcmisc.c $(PCLO_)pcmisc.$(OBJ)

# font page
# Chapter 24
$(PCLOBJ)pcfontpg.$(OBJ): $(PCLSRC)pcfontpg.c \
                          $(std_h)            \
                          $(gstypes_h)        \
                          $(pldict_h)         \
                          $(pcfont_h)         \
                          $(pcstate_h)	      \
                          $(pcursor_h)        \
                          $(pcommand_h)       \
                          $(plftable_h)       \
                          $(pllfont_h)        \
                          $(PCL_MAK)          \
                          $(MAKEDIRS)
	$(PCLCCC) $(PCLSRC)pcfontpg.c $(PCLO_)pcfontpg.$(OBJ)

PCL5_OPS1   = $(PCLOBJ)pcjob.$(OBJ) $(PCLOBJ)pcpage.$(OBJ)      \
              $(PCLOBJ)pcursor.$(OBJ)

PCL5_OPS2   = $(PCLOBJ)pcfont.$(OBJ) $(PCLOBJ)pctext.$(OBJ)

PCL5_OPS3   = $(PCLOBJ)pcsymbol.$(OBJ)

PCL5_OPS4   = $(PCLOBJ)pcsfont.$(OBJ) $(PCLOBJ)pcmacros.$(OBJ)

PCL5_OPS5   = $(PCLOBJ)pcrect.$(OBJ) $(PCLOBJ)pcstatus.$(OBJ)   \
              $(PCLOBJ)pcmisc.$(OBJ) $(PCLOBJ)pcfontpg.$(OBJ)

PCL5_OPS    = $(PCL5_OPS1) $(PCL5_OPS2) $(PCL5_OPS3) $(PCL5_OPS4) $(PCL5_OPS5)

# Note: we have to initialize the cursor after initializing the logical
# page dimensions, so we do it last.  This is a hack.
$(PCLOBJ)pcl5.dev: $(PCL_MAK) $(ECHOGS_XE) $(PCL_COMMON) $(PCL5_OPS) \
                   $(PCLOBJ)pcl5base.dev $(PCLOBJ)rtlbase.dev \
                   $(PCL_MAK) $(MAKEDIRS)
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
	$(ADDMOD) $(PCLOBJ)pcl5 -init pcursor

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
                          $(gsrop_h)            \
                          $(PCL_MAK)            \
                          $(MAKEDIRS)
	$(PCLCCC) $(PCLSRC)pccprint.c $(PCLO_)pccprint.$(OBJ)

# Chapter 6
# All of these are in rtgmode.c, but some of them are only registered
# in PCL5 mode.

PCL5C_OPS   = $(PCLOBJ)pccprint.$(OBJ)

$(PCLOBJ)pcl5c.dev: $(PCL_MAK) $(ECHOGS_XE) $(PCL5C_OPS) \
                    $(PCLOBJ)pcl5.dev $(PCLOBJ)rtlbasec.dev      \
                    $(PCL_MAK) $(MAKEDIRS)
	$(SETMOD) $(PCLOBJ)pcl5c $(PCL5C_OPS)
	$(ADDMOD) $(PCLOBJ)pcl5c -include $(PCLOBJ)pcl5 $(PCLOBJ)rtlbasec
	$(ADDMOD) $(PCLOBJ)pcl5c -init pccprint rtgmode

################ HP-GL/2 ################

pgfdata_h   = $(PCLSRC)pgfdata.h

pgfont_h    = $(PCLSRC)pgfont.h \
              $(pgmand_h)

pggeom_h    = $(PCLSRC)pggeom.h   \
              $(math__h)          \
              $(gstypes_h)

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
                        $(pcpatrn_h)      \
                        $(pcpage_h)       \
                        $(PCL_MAK)        \
                        $(MAKEDIRS)
	$(PCLCCC) $(PCLSRC)pgdraw.c $(PCLO_)pgdraw.$(OBJ)

$(PCLOBJ)pggeom.$(OBJ): $(PCLSRC)pggeom.c \
                        $(stdio__h)       \
                        $(pggeom_h)       \
                        $(gxfarith_h)     \
                        $(PCL_MAK)        \
                        $(MAKEDIRS)
	$(PCLCCC) $(PCLSRC)pggeom.c $(PCLO_)pggeom.$(OBJ)

$(PCLOBJ)pgmisc.$(OBJ): $(PCLSRC)pgmisc.c \
                        $(pgmand_h)       \
                        $(pgmisc_h)       \
                        $(PCL_MAK)        \
                        $(MAKEDIRS)
	$(PCLCCC) $(PCLSRC)pgmisc.c $(PCLO_)pgmisc.$(OBJ)

# Initialize/reset.  We break this out simply because it's easier to keep
# track of it this way.

$(PCLOBJ)pginit.$(OBJ): $(PCLSRC)pginit.c \
                        $(gx_h)           \
                        $(gsmatrix_h)     \
                        $(gsmemory_h)     \
                        $(gsstate_h)      \
                        $(pgfont_h)       \
                        $(pgmand_h)       \
                        $(pginit_h)       \
                        $(pgdraw_h)       \
                        $(pgmisc_h)       \
                        $(pcpatrn_h)      \
                        $(PCL_MAK)        \
                        $(MAKEDIRS)
	$(PCLCCC) $(PCLSRC)pginit.c $(PCLO_)pginit.$(OBJ)

# Parsing and utilities

$(PCLOBJ)pgparse.$(OBJ): $(PCLSRC)pgparse.c \
                         $(AK)              \
                         $(math__h)         \
                         $(stdio__h)        \
                         $(gdebug_h)        \
                         $(gstypes_h)       \
                         $(scommon_h)       \
                         $(pgmand_h)        \
                         $(PCL_MAK)         \
                         $(MAKEDIRS)
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
                         $(pcstate_h)       \
                         $(PCL_MAK)         \
                         $(MAKEDIRS)
	$(PCLCCC) $(PCLSRC)pgframe.c $(PCLO_)pgframe.$(OBJ)

# Chapter 19
$(PCLOBJ)pgconfig.$(OBJ): $(PCLSRC)pgconfig.c \
                          $(gx_h)             \
                          $(gsmatrix_h)       \
                          $(gsmemory_h)       \
                          $(gsstate_h)        \
                          $(gscoord_h)        \
                          $(pgmand_h)         \
                          $(pcparse_h)        \
                          $(pgdraw_h)         \
                          $(pginit_h)         \
                          $(pggeom_h)         \
                          $(pgmisc_h)         \
                          $(pcursor_h)        \
                          $(pcpage_h)         \
                          $(pcpalet_h)        \
                          $(pcdraw_h)         \
                          $(PCL_MAK)          \
                          $(MAKEDIRS)
	$(PCLCCC) $(PCLSRC)pgconfig.c $(PCLO_)pgconfig.$(OBJ)

# Chapter 20
$(PCLOBJ)pgvector.$(OBJ): $(PCLSRC)pgvector.c \
                          $(stdio__h)         \
                          $(gdebug_h)         \
		          $(pcparse_h)        \
                          $(pgmand_h)         \
                          $(pggeom_h)         \
                          $(pgdraw_h)         \
                          $(pgmisc_h)         \
                          $(gspath_h)         \
                          $(gscoord_h)        \
                          $(math__h)          \
                          $(PCL_MAK)          \
                          $(MAKEDIRS)
	$(PCLCCC) $(PCLSRC)pgvector.c $(PCLO_)pgvector.$(OBJ)

# Chapter 21
$(PCLOBJ)pgpoly.$(OBJ): $(PCLSRC)pgpoly.c \
                        $(std_h)          \
                        $(pcparse_h)      \
                        $(pgmand_h)       \
                        $(pgdraw_h)       \
                        $(pggeom_h)       \
                        $(pgmisc_h)       \
                        $(pcpatrn_h)      \
                        $(gspath_h)       \
			$(gscoord_h)      \
                        $(PCL_MAK)        \
                        $(MAKEDIRS)
	$(PCLCCC) $(PCLSRC)pgpoly.c $(PCLO_)pgpoly.$(OBJ)

# Chapter 22
$(PCLOBJ)pglfill.$(OBJ): $(PCLSRC)pglfill.c \
                         $(memory__h)       \
                         $(pcparse_h)       \
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
                         $(pcpatrn_h)       \
                         $(pcuptrn_h)       \
                         $(PCL_MAK)         \
                         $(MAKEDIRS)
	$(PCLCCC) $(PCLSRC)pglfill.c $(PCLO_)pglfill.$(OBJ)

# Chapter 23
$(PCLOBJ)pgchar.$(OBJ): $(PCLSRC)pgchar.c \
                        $(math__h)        \
                        $(stdio__h)       \
                        $(gdebug_h)       \
                        $(pcparse_h)      \
                        $(pgmand_h)       \
                        $(pgdraw_h)       \
                        $(pginit_h)       \
                        $(pggeom_h)       \
                        $(pgmisc_h)       \
                        $(pcfsel_h)       \
                        $(pcpalet_h)      \
                        $(PCL_MAK)        \
                        $(MAKEDIRS)
	$(PCLCCC) $(PCLSRC)pgchar.c $(PCLO_)pgchar.$(OBJ)

$(PCLOBJ)pglabel.$(OBJ): $(PCLSRC)pglabel.c  \
                         $(math__h)          \
                         $(memory__h)        \
                         $(ctype__h)         \
                         $(stdio__h)         \
                         $(gdebug_h)         \
                         $(pcparse_h)        \
                         $(plvalue_h)        \
                         $(pgmand_h)         \
                         $(pginit_h)         \
                         $(pgfont_h)         \
                         $(pgdraw_h)         \
                         $(pggeom_h)         \
                         $(pgmisc_h)         \
                         $(pcpage_h)         \
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
                         $(gxstate_h)        \
                         $(PCL_MAK)          \
                         $(MAKEDIRS)
	$(PCLCCC) $(PCLSRC)pglabel.c $(PCLO_)pglabel.$(OBJ)

$(PCLOBJ)pgfdata.$(OBJ): $(PCLSRC)pgfdata.c  \
                         $(std_h)            \
                         $(gstypes_h)        \
                         $(gsccode_h)        \
                         $(gsstate_h)        \
                         $(gspath_h)         \
                         $(gserrors_h)       \
                         $(gxarith_h)        \
                         $(pgfdata_h)        \
                         $(PCL_MAK)          \
                         $(MAKEDIRS)
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
                        $(pgfont_h)       \
                        $(PCL_MAK)        \
                        $(MAKEDIRS)
	$(PCLCCC) $(PCLSRC)pgfont.c $(PCLO_)pgfont.$(OBJ)

HPGL2_OPS1  = $(PCLOBJ)pgframe.$(OBJ) $(PCLOBJ)pgconfig.$(OBJ)  \
              $(PCLOBJ)pgvector.$(OBJ)
HPGL2_OPS2  = $(PCLOBJ)pgpoly.$(OBJ) $(PCLOBJ)pglfill.$(OBJ)    \
              $(PCLOBJ)pgchar.$(OBJ)
HPGL2_OPS3  = $(PCLOBJ)pglabel.$(OBJ) $(PCLOBJ)pgfdata.$(OBJ)   \
              $(PCLOBJ)pgfont.$(OBJ)
HPGL2_OPS   = $(HPGL2_OPS1) $(HPGL2_OPS2) $(HPGL2_OPS3)

$(PCLOBJ)hpgl2.dev: $(PCL_MAK) $(ECHOGS_XE) $(PCL_COMMON) \
                    $(HPGL2_OTHER) $(HPGL2_OPS)      \
                    $(PCL_MAK) $(MAKEDIRS)
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
                         $(pcparse_h)       \
                         $(pgmand_h)        \
                         $(pginit_h)        \
                         $(pgmisc_h)        \
                         $(pgdraw_h)        \
                         $(gsstate_h)       \
                         $(pcpalet_h)       \
                         $(PCL_MAK)         \
                         $(MAKEDIRS)
	$(PCLCCC) $(PCLSRC)pgcolor.c $(PCLO_)pgcolor.$(OBJ)

HPGL2C_OPS  = $(PCLOBJ)pgcolor.$(OBJ)

$(PCLOBJ)hpgl2c.dev: $(ECHOGS_XE) $(HPGL2C_OPS) $(PCLOBJ)hpgl2.dev \
                     $(PCL_MAK) $(MAKEDIRS)
	$(SETMOD) $(PCLOBJ)hpgl2c $(HPGL2C_OPS)
	$(ADDMOD) $(PCLOBJ)hpgl2c -include $(PCLOBJ)hpgl2
	$(ADDMOD) $(PCLOBJ)hpgl2c -init pgcolor
