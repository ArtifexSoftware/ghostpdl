# makefile for jbig2dec
# under Microsoft Visual C++

LIBPNGDIR=../libpng
ZLIBDIR=../zlib

EXE=.exe
OBJ=.obj
NUL=
CFLAGS=-nologo -W4 -Zi -DHAVE_STRING_H=1 -I$(LIBPNGDIR) -I$(ZLIBDIR)
CC=cl
FE=-Fe

all: jbig2dec$(EXE)

jbig2dec$(EXE): $(libjbig2_OBJS) $(jbig2dec_OBJS)
	$(CC) $(CFLAGS) $(FE)jbig2dec$(EXE) $(libjbig2_OBJS) $(jbig2dec_OBJS) $(LIBPNGDIR)/libpng.lib $(ZLIBDIR)/zlib.lib

!include common.mak

clean:
	-del $(OBJS)
	-del jbig2dec$(EXE)
	-del jbig2dec.ilk
	-del jbig2dec.pdb
	-del pbm2png$(EXE)
	-del pbm2png.ilk
	-del pbm2png.pdb
	-del vc70.pdb
	-del vc60.pdb
	-del vc50.pdb

