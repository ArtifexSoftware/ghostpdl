
# Change these to suit.
#CFLAGS = -Wall -g -DDETAILED_DEBUG
#CFLAGS = -Wall -O
CFLAGS = -g

# The "jpegxr" command is build as a demonstration for the
# libjpegxr library.
O = jpegxr.o file.o qp.tab.o qp_lexor.o

jpegxr: $O libjpegxr.a

qp.tab.o: qp.tab.c jpegxr.h
qp_lexor.o: qp_lexor.c qp.tab.h jpegxr.h

jpegxr.o: jpegxr.c jpegxr.h file.h
file.o:    file.c    file.h

qp_c_code: qp.tab.c qp.tab.h qp_lexor.c

# Use this rule (i.e. make dist_files) to pre-generate the C files
# for distribution to systems that do not have a working bison/flex.
dist_files: qp.tab.c qp.tab.h qp_lexor.c
	sed -e "/<unistd.h>/ c /* #include <unistd.h>  ...is not needed */" qp_lexor.c > tmp.c
	mv tmp.c qp_lexor.c

qp.tab.c qp.tab.h: qp_parse.y
	bison -d -p qp_ -b qp qp_parse.y

qp_lexor.c: qp_lexor.lex
	flex -oqp_lexor.c -Pqp_ qp_lexor.lex

# The library files are here. Note that it is the library that is
# the reference code for JPEG XR. The "jpegxr" command that is
# built above (and the source files that go into it) is only an
# example program that demonstrates this library.
L = algo.o api.o w_emit.o flags.o init.o io.o cr_parse.o cw_emit.o r_parse.o jpegxr_pixelformat.o\
r_strip.o r_tile_spatial.o r_tile_frequency.o w_strip.o w_tile_spatial.o w_tile_frequency.o x_strip.o
libjpegxr.a: $L
	-rm -f $@
	ar cq $@ $L


algo.o:    algo.c    jxr_priv.h jpegxr.h
api.o:     api.c     jxr_priv.h jpegxr.h
w_emit.o:  w_emit.c  jxr_priv.h jpegxr.h
flags.o:   flags.c   jxr_priv.h jpegxr.h
init.o:    init.c    jxr_priv.h jpegxr.h
io.o:      io.c      jxr_priv.h jpegxr.h
r_parse.o: r_parse.c jxr_priv.h jpegxr.h
r_strip.o: r_strip.c jxr_priv.h jpegxr.h
w_strip.o: w_strip.c jxr_priv.h jpegxr.h
x_strip.o: x_strip.c jxr_priv.h jpegxr.h
jpegxr_pixelformat.o: jpegxr_pixelformat.c jxr_priv.h
cr_parse.o: cr_parse.c jxr_priv.h jpegxr.h
cw_emit.o: cw_emit.c jxr_priv.h jpegxr.h

r_tile_frequency.o: r_tile_frequency.c jxr_priv.h jpegxr.h
r_tile_spatial.o: r_tile_spatial.c jxr_priv.h jpegxr.h
w_tile_spatial.o: w_tile_spatial.c jxr_priv.h jpegxr.h
w_tile_frequency.o: w_tile_frequency.c jxr_priv.h jpegxr.h


