# $Id: Makefile.am,v 1.15 2003/03/05 02:44:43 giles Exp $
## process this file with automake to generate Makefile.in

AUTOMAKE_OPTIONS = foreign 1.7 dist-bzip2 # require automake 1.7

lib_LIBRARIES = libjbig2dec.a
include_HEADERS = jbig2.h

libjbig2dec_a_SOURCES = jbig2.c \
	jbig2_arith.c jbig2_arith_int.c jbig2_arith_iaid.c jbig2_huffman.c \
	jbig2_segment.c jbig2_page.c \
	jbig2_symbol_dict.c jbig2_text.c \
	jbig2_generic.c jbig2_mmr.c \
	jbig2_image.c jbig2_image_pbm.c \
	os_types.h config_types.h config_win32.h \
	jbig2.h jbig2_priv.h jbig2_image.h \
	jbig2_arith.h jbig2_arith_iaid.h jbig2_arith_int.h \
	jbig2_huffman.h jbig2_hufftab.h jbig2_mmr.h \
	jbig2_generic.h jbig2_symbol_dict.h \
	jbig2_metadata.c jbig2_metadata.h

bin_PROGRAMS = jbig2dec
#noinst_PROGRAMS = test_huffman test_arith
noinst_PROGRAMS = test_sha1

jbig2dec_SOURCES = jbig2dec.c sha1.c sha1.h \
	jbig2.h jbig2_image.h getopt.h \
	os_types.h config_types.h config_win32.h
jbig2dec_LDADD = libjbig2dec.a @LIBOBJS@

test_sha1_SOURCES = sha1.c sha1.h
test_sha1_CFLAGS = -DTEST

TESTS = test_sha1 test_jbig2dec.py

EXTRA_DIST = test_jbig2dec.py msvc.mak

MAINTAINERCLEANFILES = config_types.h.in
