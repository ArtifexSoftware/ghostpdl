CC=cc
CFLAGS=-Wall -g -I/usr/local/include

APPS=jbig2dec test_arith test_huffman test_png

all:	$(APPS)

jbig2dec:	jbig2.o jbig2dec.o jbig2_huffman.o jbig2_arith.o jbig2_image.o jbig2_generic.o jbig2_symbol_dict.o jbig2_arith_int.o

test_huffman:	jbig2_huffman.c
	$(CC) $(CFLAGS) -DTEST jbig2_huffman.c -o test_huffman

test_arith:	jbig2_arith.c
	$(CC) $(CFLAGS) -DTEST -DDEBUG jbig2_arith.c -o test_arith

test_png:	png_image.o jbig2_image.o
	$(CC) $(CFLAGS) -DTEST -DDEBUG png_image.c jbig2_image.o -lpng -lz -o test_png

clean:
	rm $(APPS) *.o
