# Copyright (C) 2018-2023 Artifex Software, Inc.
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
# This needs a file on its own due to the required include ordering
# in the top level makefiles. Being empty, it's here purely for symmetry

PDF_FONT_ROMFS_ARGS=

# The -C turns "compaction" on, -B off. For debugging convenience
# it's off just now.
# PDF_ROMFS_ARGS=-d Resource/ -P $(PSRESDIR)$(D) -C CMap$(D)*
PDF_ROMFS_ARGS=-d Resource/ -P $(PSRESDIR)$(D) -C CMap$(D)* -B Font$(D)* CIDFSubst$(D)* -C Init$(D)Fontmap.GS Init$(D)cidfmap
