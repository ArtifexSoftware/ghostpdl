#    Copyright (C) 1997 Aladdin Enterprises.  All rights reserved.
#    Unauthorized use, copying, and/or distribution prohibited.

# gccdefs.mak
# Definitions for compilation with gcc.

CC_=gcc $(GENOPT) $(CFLAGS) $(XCFLAGS) -c
CCAUX=gcc $(CFLAGS)
