# $Id: Makefile.am,v 1.10 2002/07/20 17:06:04 giles Exp $
## process this file with automake to generate Makefile.in

AUTOMAKE_OPTIONS = 1.6.1 # require automake 1.6

lib_LIBRARIES = libjbig2dec.a

libjbig2dec_a_SOURCES = jbig2.c \
	jbig2_arith.c jbig2_arith_int.c jbig2_arith_iaid.c jbig2_huffman.c \
	jbig2_segment.c jbig2_page.c \
	jbig2_symbol_dict.c jbig2_text.c \
	jbig2_generic.c jbig2_mmr.c \
	jbig2_image.c jbig2_image_pbm.c

bin_PROGRAMS = jbig2dec test_sha1
#noinst_PROGRAMS = test_huffman test_arith

jbig2dec_SOURCES = jbig2dec.c sha1.c
jbig2dec_LDADD = libjbig2dec.a @LIBOBJS@

test_sha1_SOURCES = sha1.c
test_sha1_CFLAGS = -DTEST

MAINTAINERCLEANFILES = config_types.h.in

TESTS = test_sha1 test_jbig2dec.py
