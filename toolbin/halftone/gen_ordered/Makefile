# Makefile for gen_ordered standalone executable

CFLAGS=

GLSRC=../../../base/

gen_ordered: gen_ordered_main.o gen_ordered.o
	$(CC) -o gen_ordered gen_ordered_main.o gen_ordered.o -lm

gen_ordered_main.o: gen_ordered_main.c $(GLSRC)gen_ordered.h
	$(CC) -o gen_ordered_main.o -I$(GLSRC) -c gen_ordered_main.c

gen_ordered.o: $(GLSRC)gen_ordered.c $(GLSRC)gen_ordered.h
	$(CC) -o gen_ordered.o -I$(GLSRC) -c $(GLSRC)gen_ordered.c
