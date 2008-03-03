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
# $Id:$
# mkromfs macros for PostScript %rom% when COMPILE_INITS=1

# The following list of files needed by the interpreter is maintained here.
# This changes infrequently, but is a potential point of bitrot, but since
# unix-inst.mak uses this macro, problems should surface when testing installed
# versions.

#	The init files are put in the lib/ directory (gs_init.ps + EXTRA_INIT_FILES)
#	Resource files go into Resource/...

RESOURCE_LIST=ColorSpace$(D) Decoding$(D) Encoding$(D) Font$(D) ProcSet$(D) IdiomSet$(D) CIDFont$(D) CMap$(D)

#	Note: gs_cet.ps is only needed to match Adobe CPSI defaults
PS_ROMFS_ARGS=-c -P $(PSRESDIR)$(D) -d Resource/ $(RESOURCE_LIST) \
   -d lib/ -P $(PSGENDIR)$(D) $(GS_INIT) \
   -P $(PSLIBDIR)$(D) Fontmap cidfmap xlatmap FAPI FCOfontmap-PCLPS2 gs_cet.ps

PS_ROMFS_DEPS=$(PSGENDIR)$(D)$(GS_INIT)
