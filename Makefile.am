# $Id: Makefile.am,v 1.2 2002/05/08 02:36:04 giles Exp $
## process this file with automake to generate Makefile.in

lib_LIBRARIES = libjbig2dec.a

libjbig2dec_a_SOURCES = jbig2.c \
	jbig2_arith.c jbig2_arith_int.c jbig2_huffman.c \
	jbig2_symbol_dict.c \
	jbig2_generic.c jbig2_mmr.c \
	jbig2_image.c jbig2_image_pbm.c

bin_PROGRAMS = jbig2dec
#noinst_PROGRAMS = test_huffman test_arith test_png

jbig2dec_SOURCES = jbig2dec.c
jbig2dec_LDADD = libjbig2dec.a @LIBOBJS@


