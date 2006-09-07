GSDIR=./gs
#GSDIR=../ghostscript-head

CC = gcc
#CFLAGS = -Wall -g -O2 -ggdb
CFLAGS = -Wall -g -O0 -ggdb
CPPFLAGS = $(GX_COLOR_INDEX_DEFINE) -DDEBUG -DGSLT_DEBUG_TIFF
CPPFLAGS = $(GX_COLOR_INDEX_DEFINE) -DDEBUG
INCLUDES = -I$(GSDIR)/src -I$(GSDIR)/soobj -I/opt/local/include
LDFLAGS = -L$(GSDIR)/sobin -L/opt/local/lib
LIBS = -lgs

gslt_SRCS = gslt_alloc.c gslt_init.c \
	gslt_font_api.c \
	gslt_image.c \
	gslt_image_jpeg.c gslt_image_png.c gslt_image_tiff.c
gslt_HDRS = gslite.h gslt_alloc.h gslt_font_api.h gslt_image.h
gslt_OBJS = $(gslt_SRCS:.c=.o)

MAIN = gslt_test gslt_image_test gslt_font_api_test

.PHONY: depend clean

all:    $(MAIN)

gslt_test:   gslt_test.o $(gslt_OBJS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LDFLAGS) $(LIBS)

gslt_image_test: gslt_image_test.o $(gslt_OBJS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LDFLAGS) $(LIBS)

gslt_font_api_test: gslt_font_api_test.o $(gslt_OBJS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LDFLAGS) $(LIBS)


.c.o:
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) -c $<

clean:
	$(RM) *.o *~ $(MAIN)

check: $(MAIN)
	@failed=0; all=0; \
	for case in 1 2 3 4 5 6 7 8 9; do \
	  all=`expr $$all + 1`; \
	if LD_LIBRARY_PATH=$(GSDIR)/sobin ./gslt_test x11 $$case; then \
	  echo "PASS"; \
	else \
	  failed=`expr $$failed + 1`; \
	  echo "FAILED"; \
	fi; \
	done; \
	for file in *.png *.jpeg *.jpg *.tiff; do \
	  all=`expr $$all + 1`; \
	if test -r $$file; then \
	if LD_LIBRARY_PATH=$(GSDIR)/sobin ./gslt_image_test $$file; then \
	  echo "PASS"; \
	else \
	  failed=`expr $$failed + 1`; \
	  echo "FAILED"; \
	fi; \
	fi; \
	done; \
	all=`expr $$all + 1`; \
	if LD_LIBRARY_PATH=$(GSDIR)/sobin ./gslt_font_api_test; then \
          echo "PASS"; \
        else \
          failed=`expr $$failed + 1`; \
          echo "FAILED"; \
        fi; \
	if test "$$failed" -eq 0; then \
	  echo "All $$all tests passed"; \
	else \
	  echo "FAILED $$failed of $$all tests"; \
	fi

depend: $(gslt_SRCS)
	makedepend $(INCLUDES) $^

# DO NOT DELETE THIS LINE -- make depend needs it

