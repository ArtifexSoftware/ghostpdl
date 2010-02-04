#  Copyright (C) 2001-2006 Artifex Software, Inc.
#  All Rights Reserved.
#
#  This software is provided AS-IS with no warranty, either express or
#  implied.
#
#  This software is distributed under license and may not be copied, modified
#  or distributed except as expressly authorized under the terms of that
#  license.  Refer to licensing information at http://www.artifex.com/
#  or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
#  San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
#
# $Id$
# mkromfs macros for PostScript %rom% when COMPILE_INITS=1

# The following list of files needed by the interpreter is maintained here.
# This changes infrequently, but is a potential point of bitrot, but since
# unix-inst.mak uses this macro, problems should surface when testing installed
# versions.

#	Resource files go into Resource/...
#	The init files are in the %rom%Resource/Init/ directory
#       Any EXTRA_INIT_FILES go into %rom%lib/

RESOURCE_LIST=CIDFont$(D) CMap$(D) ColorSpace$(D) Decoding$(D) Encoding$(D) Font$(D) IdiomSet$(D) Init$(D) ProcSet$(D) SubstCID$(D)

#	Note: gs_cet.ps is only needed to match Adobe CPSI defaults
PS_ROMFS_ARGS=-c -P $(PSRESDIR)$(D) -d Resource/ $(RESOURCE_LIST) -d lib/ -P $(PSLIBDIR)$(D) $(EXTRA_INIT_FILES)

# A list of all the files in Resource/Init/ as dependencies for COMPILE_INITS=1.
# If you add a file remember to add it here. If you forget then builds from
# clean will work (as all files in the directory are included), but rebuilds
# after changes to unlisted files will not cause the romfs to be regenerated.
PS_ROMFS_DEPS=\
	$(PSRESDIR)$(D)Init$(D)cidfmap \
	$(PSRESDIR)$(D)Init$(D)FCOfontmap-PCLPS2 \
	$(PSRESDIR)$(D)Init$(D)Fontmap \
	$(PSRESDIR)$(D)Init$(D)Fontmap.GS \
	$(PSRESDIR)$(D)Init$(D)gs_agl.ps \
	$(PSRESDIR)$(D)Init$(D)gs_btokn.ps \
	$(PSRESDIR)$(D)Init$(D)gs_cet.ps \
	$(PSRESDIR)$(D)Init$(D)gs_cff.ps \
	$(PSRESDIR)$(D)Init$(D)gs_cidcm.ps \
	$(PSRESDIR)$(D)Init$(D)gs_ciddc.ps \
	$(PSRESDIR)$(D)Init$(D)gs_cidfm.ps \
	$(PSRESDIR)$(D)Init$(D)gs_cidfn.ps \
	$(PSRESDIR)$(D)Init$(D)gs_cidtt.ps \
	$(PSRESDIR)$(D)Init$(D)gs_cmap.ps \
	$(PSRESDIR)$(D)Init$(D)gs_cspace.ps \
	$(PSRESDIR)$(D)Init$(D)gs_css_e.ps \
	$(PSRESDIR)$(D)Init$(D)gs_dbt_e.ps \
	$(PSRESDIR)$(D)Init$(D)gs_diskf.ps \
	$(PSRESDIR)$(D)Init$(D)gs_diskn.ps \
	$(PSRESDIR)$(D)Init$(D)gs_dpnxt.ps \
	$(PSRESDIR)$(D)Init$(D)gs_dps.ps \
	$(PSRESDIR)$(D)Init$(D)gs_dps1.ps \
	$(PSRESDIR)$(D)Init$(D)gs_dps2.ps \
	$(PSRESDIR)$(D)Init$(D)gs_dscp.ps \
	$(PSRESDIR)$(D)Init$(D)gs_epsf.ps \
	$(PSRESDIR)$(D)Init$(D)gs_fapi.ps \
	$(PSRESDIR)$(D)Init$(D)gs_fntem.ps \
	$(PSRESDIR)$(D)Init$(D)gs_fonts.ps \
	$(PSRESDIR)$(D)Init$(D)gs_frsd.ps \
	$(PSRESDIR)$(D)Init$(D)gs_icc.ps \
	$(PSRESDIR)$(D)Init$(D)gs_il1_e.ps \
	$(PSRESDIR)$(D)Init$(D)gs_img.ps \
	$(PSRESDIR)$(D)Init$(D)gs_init.ps \
	$(PSRESDIR)$(D)Init$(D)gs_l2img.ps \
	$(PSRESDIR)$(D)Init$(D)gs_lev2.ps \
	$(PSRESDIR)$(D)Init$(D)gs_ll3.ps \
	$(PSRESDIR)$(D)Init$(D)gs_mex_e.ps \
	$(PSRESDIR)$(D)Init$(D)gs_mgl_e.ps \
	$(PSRESDIR)$(D)Init$(D)gs_mro_e.ps \
	$(PSRESDIR)$(D)Init$(D)gs_pdfwr.ps \
	$(PSRESDIR)$(D)Init$(D)gs_pdf_e.ps \
	$(PSRESDIR)$(D)Init$(D)gs_res.ps \
	$(PSRESDIR)$(D)Init$(D)gs_resmp.ps \
	$(PSRESDIR)$(D)Init$(D)gs_setpd.ps \
	$(PSRESDIR)$(D)Init$(D)gs_statd.ps \
	$(PSRESDIR)$(D)Init$(D)gs_std_e.ps \
	$(PSRESDIR)$(D)Init$(D)gs_sym_e.ps \
	$(PSRESDIR)$(D)Init$(D)gs_trap.ps \
	$(PSRESDIR)$(D)Init$(D)gs_ttf.ps \
	$(PSRESDIR)$(D)Init$(D)gs_typ32.ps \
	$(PSRESDIR)$(D)Init$(D)gs_typ42.ps \
	$(PSRESDIR)$(D)Init$(D)gs_type1.ps \
	$(PSRESDIR)$(D)Init$(D)gs_wan_e.ps \
	$(PSRESDIR)$(D)Init$(D)opdfread.ps \
	$(PSRESDIR)$(D)Init$(D)pdf_base.ps \
	$(PSRESDIR)$(D)Init$(D)pdf_cslayer.ps \
	$(PSRESDIR)$(D)Init$(D)pdf_draw.ps \
	$(PSRESDIR)$(D)Init$(D)pdf_font.ps \
	$(PSRESDIR)$(D)Init$(D)pdf_main.ps \
	$(PSRESDIR)$(D)Init$(D)pdf_ops.ps \
	$(PSRESDIR)$(D)Init$(D)pdf_rbld.ps \
	$(PSRESDIR)$(D)Init$(D)pdf_sec.ps \
	$(PSRESDIR)$(D)Init$(D)xlatmap
