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

# 51x.mak
# 5.1x extensions for 5c project

# Masked images

gsiparm3_h=gsiparm3.h $(gsiparam_h)
gsiparm4_h=gsiparm4.h $(gsiparam_h)

gxclip_h=gxclip.h
gxmclip_h=gxmclip.h $(gxclip_h)
gxclipm_h=gxclipm.h $(gxmclip_h)

gxclip.$(OBJ): gxclip.c $(GX)\
 $(gxclip_h) $(gxdevice_h) $(gzpath_h) $(gzcpath_h)

gxclipm.$(OBJ): gxclipm.c $(memory__h)\
 $(gx_h) $(gxclipm_h) $(gxdevice_h) $(gxdevmem_h)

gxitype3.$(OBJ): gxitype3.c $(math__h) $(GXERR)\
 $(gsbitops_h) $(gscspace_h) $(gsiparm3_h) $(gsstruct_h)\
 $(gxclipm_h) $(gxdevice_h) $(gxdevmem_h) $(gxiparam_h) $(gxistate_h)

gxitype4.$(OBJ): gxitype4.c $(memory__h) $(GXERR)\
 $(gscspace_h) $(gsiparm3_h) $(gsiparm4_h) $(gxiparam_h)

gxmclip.$(OBJ): gxmclip.c\
 $(gx_h) $(gxdevice_h) $(gxdevmem_h) $(gxmclip_h)

# CRD creation API

gscrd_h=gscrd.h $(gscie_h)

gscrd.$(OBJ): gscrd.c $(GXERR) $(math__h)\
 $(gscolor2_h) $(gscrd_h) $(gsmatrix_h) $(gxcspace_h)

# Colors
gxp1fill_h=gxp1fill.h

gscscie.$(OBJ): gscscie.c $(GXERR) $(math__h)\
 $(gscie_h) $(gscolor2_h) $(gsmatrix_h) $(gsstruct_h)\
 $(gxarith_h) $(gxcmap_h) $(gxcspace_h) $(gxdevice_h) $(gzstate_h)

gscspace.$(OBJ): gscspace.c $(GXERR)\
 $(gsccolor_h) $(gsstruct_h) $(gxcspace_h) $(gxistate_h)

gxp1fill.$(OBJ): gxp1fill.c $(GXERR) $(math__h)\
 $(gsrop_h) $(gsmatrix_h)\
 $(gxcolor2_h) $(gxclip2_h) $(gxcspace_h) $(gxdcolor_h) $(gxdevcli_h)\
 $(gxdevmem_h) $(gxp1fill_h) $(gxpcolor_h)

# Extended halftone support

gshtx_h=gshtx.h $(gsmemory_h) $(gsht1_h) $(gscsepnm_h) $(gxtmap_h)

gshtx.$(OBJ): gshtx.c $(GXERR) $(memory__h)\
 $(gshtx_h) $(gsstruct_h) $(gsutil_h)\
 $(gxfmap_h) $(gzht_h) $(gzstate_h)

# Color snapping devices

gdevcmap_h=gdevcmap.h

gdevcmap.$(OBJ): gdevcmap.c $(GXERR)\
 $(gxdevice_h) $(gxlum_h) $(gdevcmap_h)

# Special stuff

gx51x.$(OBJ): gx51x.c $(gsstruct_h) $(gserrors_h)\
 $(gsutil_h) $(gscspace_h) $(gx_h) $(gxdevice_h)\
 $(gxistate_h) $(gxiparam_h)

51xlib1_=gxclip.$(OBJ) gxclipm.$(OBJ) gxitype3.$(OBJ) gxitype4.$(OBJ) gxmclip.$(OBJ)
51xlib2_=gscrd.$(OBJ) gscscie.$(OBJ) gscspace.$(OBJ) gxp1fill.$(OBJ)
51xlib3_=gshtx.$(OBJ) gdevcmap.$(OBJ) gx51x.$(OBJ)
51xlib_=$(51xlib1_) $(51xlib2_) $(51xlib3_)
51xlib.dev: $(LIB_MAK) $(ECHOGS_XE) 51x.mak $(51xlib_) bbox.dev cielib.dev
	$(SETMOD) 51xlib $(51xlib1_)
	$(ADDMOD) 51xlib $(51xlib2_)
	$(ADDMOD) 51xlib $(51xlib3_)
	$(ADDMOD) 51xlib -include bbox
	$(ADDMOD) 51xlib -include cielib
	$(ADDMOD) 51xlib -init 51x
