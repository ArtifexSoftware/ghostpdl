GSDIR=../../gs

CC = gcc
CFLAGS = -Wall -g -O0 -ggdb
GX_COLOR_INDEX_DEFINE=-DGX_COLOR_INDEX_TYPE="unsigned long long"
CPPFLAGS = $(GX_COLOR_INDEX_DEFINE) -DDEBUG
INCLUDES = -I$(GSDIR)/src -I$(GSDIR)/soobj
LDFLAGS = -L$(GSDIR)/sobin
LIBS = -lgs
SRCS = gslt_alloc.c gslt.c
OBJS = $(SRCS:.c=.o)
MAIN = gslt

.PHONY: depend clean

all:    $(MAIN)

gslt:   $(OBJS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) -o $(MAIN) $(OBJS) $(LDFLAGS) $(LIBS)

.c.o:
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) -c $<

clean:
	$(RM) *.o *~ $(MAIN)

depend: $(SRCS)
	makedepend $(INCLUDES) $^

# DO NOT DELETE THIS LINE -- make depend needs it

