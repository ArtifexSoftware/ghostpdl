#    Copyright (C) 1997 Aladdin Enterprises.  All rights reserved.
#    Unauthorized use, copying, and/or distribution prohibited.

# sgidefs.mak
# Definitions for compilation with the Silicon Graphics C compiler

CC_         = $(CCLD) $(GENOPT) $(CFLAGS) $(XCFLAGS) -c
CCAUX       = $(CCLD)
