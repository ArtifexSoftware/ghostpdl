CFLAGS=-Wall -g

all:	jbig2

jbig2:	jbig2.o jbig2_huffman.o jbig2_arith.o

test_huffman:	jbig2_huffman.c
	gcc $(CFLAGS) -DTEST jbig2_huffman.c -o test_huffman

test_arith:	jbig2_arith.c
	gcc $(CFLAGS) -DTEST -DDEBUG jbig2_arith.c -o test_arith

