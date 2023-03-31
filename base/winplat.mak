# Copyright (C) 2001-2023 Artifex Software, Inc.
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
# Artifex Software, Inc.,  39 Mesa Street, Suite 108A, San Francisco,
# CA 94129, USA, for further information.
#
# Common makefile section for builds on 32-bit MS Windows, including the
# Watcom MS-DOS build.

# Define the name of this makefile.
WINPLAT_MAK=$(GLSRC)winplat.mak $(TOP_MAKEFILES)

GLCCWINXPSPRINT=$(CC_WX) $(XPSPRINTCFLAGS) $(CCWINFLAGS) $(I_)$(GLI_)$(_I) $(GLF_)

# Define generic Windows-specific modules.

winplatcommon_=$(GLOBJ)gp_ntfs.$(OBJ) $(GLOBJ)gp_win32.$(OBJ)

winplat_=$(winplatcommon_) $(GLOBJ)gp_nxpsprn.$(OBJ)
winplatxpsprint_=$(winplatcommon_) $(GLOBJ)gp_wxpsprn.$(OBJ)

$(GLD)winplat0.dev : $(WINPLAT_MAK) $(ECHOGS_XE) $(winplat_) $(WINPLAT_MAK)
	$(SETMOD) $(GLD)winplat0 $(winplat_)

$(GLD)winplat1.dev : $(WINPLAT_MAK) $(ECHOGS_XE) $(winplatxpsprint_) $(WINPLAT_MAK)
	$(SETMOD) $(GLD)winplat1 $(winplatxpsprint_)

$(GLD)winplat.dev : $(GLD)winplat$(XPSPRINT).dev
	$(CP_) $(GLD)winplat$(XPSPRINT).dev $(GLD)winplat.dev

$(GLOBJ)gp_ntfs.$(OBJ): $(GLSRC)gp_ntfs.c $(AK)\
 $(dos__h) $(memory__h) $(stdio__h) $(string__h) $(windows__h)\
 $(gp_h) $(gpmisc_h) $(gsmemory_h) $(gsstruct_h) $(gstypes_h) $(gsutil_h) \
 $(WINPLAT_MAK)
	$(GLCCWIN) $(GLO_)gp_ntfs.$(OBJ) $(C_) $(GLSRC)gp_ntfs.c

$(AUX)gp_ntfs.$(OBJ): $(GLSRC)gp_ntfs.c $(AK)\
 $(dos__h) $(memory__h) $(stdio__h) $(string__h) $(windows__h)\
 $(gp_h) $(gpmisc_h) $(gsmemory_h) $(gsstruct_h) $(gstypes_h) $(gsutil_h) \
 $(WINPLAT_MAK)
	$(GLCCAUX) $(AUXO_)gp_ntfs.$(OBJ) $(C_) $(GLSRC)gp_ntfs.c

$(GLOBJ)gp_win32.$(OBJ): $(GLSRC)gp_win32.c $(AK)\
 $(dos__h) $(malloc__h) $(stdio__h) $(string__h) $(windows__h)\
 $(gp_h) $(gsmemory_h) $(gstypes_h) $(stream_h) $(WINPLAT_MAK)
	$(GLCCWIN) $(GLO_)gp_win32.$(OBJ) $(C_) $(GLSRC)gp_win32.c

$(AUX)gp_win32.$(OBJ): $(GLSRC)gp_win32.c $(AK)\
 $(dos__h) $(malloc__h) $(stdio__h) $(string__h) $(windows__h)\
 $(gp_h) $(gsmemory_h) $(gstypes_h) $(stream_h) $(WINPLAT_MAK)
	$(GLCCAUX) $(AUXO_)gp_win32.$(OBJ) $(C_) $(GLSRC)gp_win32.c

# Define the Windows thread / synchronization module.

winsync_=$(GLOBJ)gp_wsync.$(OBJ)
$(GLD)winsync.dev : $(WINPLAT_MAK) $(ECHOGS_XE) $(winsync_) $(WINPLAT_MAK)
	$(SETMOD) $(GLD)winsync $(winsync_)
	$(ADDMOD) $(GLD)winsync -replace $(GLD)nosync

$(GLOBJ)gp_wsync.$(OBJ): $(GLSRC)gp_wsync.c $(AK)\
 $(dos__h) $(malloc__h) $(stdio__h) $(string__h) $(windows__h)\
 $(gp_h) $(gsmemory_h) $(gstypes_h) $(globals_h) $(WINPLAT_MAK)
	$(GLCCWIN) $(GLO_)gp_wsync.$(OBJ) $(C_) $(GLSRC)gp_wsync.c

# The XPS printer
$(GLOBJ)gp_wxpsprn.$(OBJ): $(GLSRC)gp_wxpsprn.cpp $(windows__h) $(string__h) \
 $(gx_h) $(gserrors_h) $(WINLIB_MAK)
	$(GLCCWINXPSPRINT) $(GLO_)gp_wxpsprn.$(OBJ) $(C_) $(GLSRC)gp_wxpsprn.cpp

