/*
    jbig2dec
    
    Copyright (C) 2001-2002 artofcode LLC.
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    $Id: jbig2_symbol_dict.h,v 1.2 2002/06/15 14:12:50 giles Exp $
*/

int
jbig2_symbol_dictionary(Jbig2Ctx *ctx, Jbig2SegmentHeader *sh,
			const byte *segment_data);
