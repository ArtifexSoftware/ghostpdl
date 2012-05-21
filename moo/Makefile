MUPDF_DIR := ../../mupdf
MUPDF_OBJ := ../../mupdf/build/debug

CFLAGS += -Wall -g -Dpdf_open_document=mupdf_open_document
CFLAGS += -I$(MUPDF_DIR)/fitz -I$(MUPDF_DIR)/pdf -I$(MUPDF_DIR)/xps
CFLAGS += -I$(MUPDF_DIR)/thirdparty/freetype-2.4.9/include
CFLAGS += -I../gs/base -I../pl -I.

LIBS += $(MUPDF_OBJ)/libfitz.a
LIBS += $(MUPDF_OBJ)/libfreetype.a
LIBS +=	$(MUPDF_OBJ)/libjbig2dec.a
LIBS += $(MUPDF_OBJ)/libopenjpeg.a
# LIBS += $(MUPDF_OBJ)/libjpeg.a
# LIBS += $(MUPDF_OBJ)/libz.a

LIBS += lib/libgs.a

LIBS += -lX11 -lXt -lm

SRCS := $(wildcard *.c)
OBJS := $(SRCS:%.c=%.o)

moo : $(OBJS)
	gcc -o $@ $^ $(LIBS)

$(OBJS) : mooscript.h

clean :
	rm -f *.o
