# Portions Copyright (C) 2001 artofcode LLC. 
#  Portions Copyright (C) 1996, 2001 Artifex Software Inc.
#  Portions Copyright (C) 1988, 2000 Aladdin Enterprises.
#  This software is based in part on the work of the Independent JPEG Group.
#  All Rights Reserved.
#
#  This software is distributed under license and may not be copied, modified
#  or distributed except as expressly authorized under the terms of that
#  license.  Refer to licensing information at http://www.artifex.com/ or
#  contact Artifex Software, Inc., 101 Lucas Valley Road #110,
#  San Rafael, CA  94903, (415)492-9861, for further information.

# $RCSfile$ $Revision$
# Common makefile section for builds on 32-bit MS Windows, including the
# Watcom MS-DOS build.

# Define the name of this makefile.
WINPLAT_MAK=$(GLSRC)winplat.mak

# Define generic Windows-specific modules.

winplat_=$(GLOBJ)gp_ntfs.$(OBJ) $(GLOBJ)gp_win32.$(OBJ)
$(GLD)winplat.dev : $(WINPLAT_MAK) $(ECHOGS_XE) $(winplat_)
	$(SETMOD) $(GLD)winplat $(winplat_)

$(GLOBJ)gp_ntfs.$(OBJ): $(GLSRC)gp_ntfs.c $(AK)\
 $(dos__h) $(memory__h) $(stdio__h) $(string__h) $(windows__h)\
 $(gp_h) $(gsmemory_h) $(gsstruct_h) $(gstypes_h) $(gsutil_h)
	$(GLCCWIN) $(GLO_)gp_ntfs.$(OBJ) $(C_) $(GLSRC)gp_ntfs.c

$(GLOBJ)gp_win32.$(OBJ): $(GLSRC)gp_win32.c $(AK)\
 $(dos__h) $(malloc__h) $(stdio__h) $(string__h) $(windows__h)\
 $(gp_h) $(gsmemory_h) $(gstypes_h)
	$(GLCCWIN) $(GLO_)gp_win32.$(OBJ) $(C_) $(GLSRC)gp_win32.c

# Define the Windows thread / synchronization module.

winsync_=$(GLOBJ)gp_wsync.$(OBJ)
$(GLD)winsync.dev : $(WINPLAT_MAK) $(ECHOGS_XE) $(winsync_)
	$(SETMOD) $(GLD)winsync $(winsync_)
	$(ADDMOD) $(GLD)winsync -replace $(GLD)nosync

$(GLOBJ)gp_wsync.$(OBJ): $(GLSRC)gp_wsync.c $(AK)\
 $(dos__h) $(malloc__h) $(stdio__h) $(string__h) $(windows__h)\
 $(gp_h) $(gsmemory_h) $(gstypes_h)
	$(GLCCWIN) $(GLO_)gp_wsync.$(OBJ) $(C_) $(GLSRC)gp_wsync.c
