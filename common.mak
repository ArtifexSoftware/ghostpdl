# common makefile for jbig2dec

# assumes the following are defined:
# CC, CFLAGS, OBJ 

libjbig2_OBJS=jbig2$(OBJ) jbig2_arith$(OBJ) \
 jbig2_arith_iaid$(OBJ) jbig2_arith_int$(OBJ) jbig2_generic$(OBJ) \
 jbig2_huffman$(OBJ) jbig2_image$(OBJ) jbig2_image_pbm$(OBJ) \
 jbig2_image_png$(OBJ) jbig2_mmr$(OBJ) jbig2_page$(OBJ) \
 jbig2_segment$(OBJ) jbig2_symbol_dict$(OBJ) jbig2_text$(OBJ) \
 jbig2_metadata$(OBJ)

libjbig2_HDRS=jbig2.h jbig2_arith.h jbig2_arith_iaid.h jbig2_arith_int.h \
 jbig2_generic.h jbig2_huffman.h jbig2_hufftab.h jbig2_image.h \
 jbig2_mmr.h jbig2_priv.h jbig2_symbol_dict.h jbig2_metatdata.h \
 config_win32.h

jbig2dec_OBJS=getopt$(OBJ) getopt1$(OBJ) sha1$(OBJ)
jbig2dec_HDRS=getopt.h sha1.h

getopt$(OBJ): getopt.c getopt.h
	$(CC) $(CFLAGS) -c getopt.c

getopt1$(OBJ): getopt1.c getopt.h
	$(CC) $(CFLAGS) -c getopt1.c

jbig2$(OBJ): jbig2.c $(libjbig2_HDRS)
	$(CC) $(CFLAGS) -c jbig2.c

jbig2_arith$(OBJ): jbig2_arith.c $(libjbig2_HDRS)
	$(CC) $(CFLAGS) -c jbig2_arith.c

jbig2_arith_iaid$(OBJ): jbig2_arith_iaid.c $(libjbig2_HDRS)
	$(CC) $(CFLAGS) -c jbig2_arith_iaid.c

jbig2_arith_int$(OBJ): jbig2_arith_int.c $(libjbig2_HDRS)
	$(CC) $(CFLAGS) -c jbig2_arith_int.c

jbig2_generic$(OBJ): jbig2_generic.c $(libjbig2_HDRS)
	$(CC) $(CFLAGS) -c jbig2_generic.c

jbig2_huffman$(OBJ): jbig2_huffman.c $(libjbig2_HDRS)
	$(CC) $(CFLAGS) -c jbig2_huffman.c

jbig2_image$(OBJ): jbig2_image.c $(libjbig2_HDRS)
	$(CC) $(CFLAGS) -c jbig2_image.c

jbig2_image_pbm$(OBJ): jbig2_image_pbm.c $(libjbig2_HDRS)
	$(CC) $(CFLAGS) -c jbig2_image_pbm.c

jbig2_image_png$(OBJ): jbig2_image_png.c $(libjbig2_HDRS)
	$(CC) $(CFLAGS) -c jbig2_image_png.c

jbig2_mmr$(OBJ): jbig2_mmr.c $(libjbig2_HDRS)
	$(CC) $(CFLAGS) -c jbig2_mmr.c

jbig2_page$(OBJ): jbig2_page.c $(libjbig2_HDRS)
	$(CC) $(CFLAGS) -c jbig2_page.c

jbig2_segment$(OBJ): jbig2_segment.c $(libjbig2_HDRS)
	$(CC) $(CFLAGS) -c jbig2_segment.c

jbig2_symbol_dict$(OBJ): jbig2_symbol_dict.c $(libjbig2_HDRS)
	$(CC) $(CFLAGS) -c jbig2_symbol_dict.c

jbig2_text$(OBJ): jbig2_text.c $(libjbig2_HDRS)
	$(CC) $(CFLAGS) -c jbig2_text.c

jbig2_metadata$(OBJ) : jbig2_metadata.c $(libjbig2_HDRS)
	$(CC) $(CFLAGS) -c jbig2_metadata.c

jbig2dec$(OBJ): jbig2dec.c $(libjbig2_HDRS)
	$(CC) $(CFLAGS) -c jbig2dec.c

sha1$(OBJ): sha1.c $(libjbig2_HDRS)
	$(CC) $(CFLAGS) -c sha1.c

