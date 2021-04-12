# Copyright (C) 2018-2021 Artifex Software, Inc.
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
# This needs a file on its own due to the required include ordering
# in the top level makefiles. Being empty, it's here purely for symmetry

PDF_FONT_ROMFS_ARGS=

# The -C turns "compaction" on, -B off. For debugging convenience
# it's off just now.
# PDF_ROMFS_ARGS=-d Resource/ -P $(PSRESDIR)$(D) -C CMap$(D)*
PDF_ROMFS_ARGS=-d Resource/ -P $(PSRESDIR)$(D) -C CMap$(D)* -B Font$(D)* -C Init$(D)Fontmap.GS
