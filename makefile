CFLAGS=-Wall -g

APPS=jbig2dec test_arith test_huffman

all:	$(APPS)

jbig2dec:	jbig2dec.o jbig2_huffman.o jbig2_arith.o

test_huffman:	jbig2_huffman.c
	gcc $(CFLAGS) -DTEST jbig2_huffman.c -o test_huffman

test_arith:	jbig2_arith.c
	gcc $(CFLAGS) -DTEST -DDEBUG jbig2_arith.c -o test_arith

clean:
	rm $(APPS) *.o