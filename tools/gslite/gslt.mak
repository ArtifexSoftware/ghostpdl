GSLT_CC=$(CC_) $(I_)$(GSLTSRCDIR)$(_I) $(I_)$(GLSRCDIR)$(_I)\
	$(I_)$(GLGENDIR)$(_I) $(I_)$(PSRCDIR)$(_I)\
	$(I_)$(ZSRCDIR)$(_I)
GSLTO_=$(O_)$(GSLTOBJ)
GSLTOBJ=$(GLOBJDIR)$(D)
GSLTSRC=$(GSLTSRCDIR)$(D)

GSLT_OBJS = \
	$(GSLTOBJ)gslt_alloc.$(OBJ) \
	$(GSLTOBJ)gslt_init.$(OBJ) \
	$(GSLTOBJ)gslt_image.$(OBJ) \
	$(GSLTOBJ)gslt_image_jpeg.$(OBJ) \
	$(GSLTOBJ)gslt_image_png.$(OBJ) \
	$(GSLTOBJ)gslt_image_tiff.$(OBJ) \
	$(GSLTOBJ)gslt_font_cache.$(OBJ) \
	$(GSLTOBJ)gslt_font_encoding.$(OBJ) \
	$(GSLTOBJ)gslt_font_glyph.$(OBJ) \
	$(GSLTOBJ)gslt_font_ttf.$(OBJ) \
	$(GSLTOBJ)gslt_font_cff.$(OBJ) \
	$(GSLTOBJ)gslt_stubs.$(OBJ) \

$(GSLTOBJ)gslt_alloc.$(OBJ) : $(GSLTSRC)gslt_alloc.c
	$(GSLT_CC) $(GSLTO_)gslt_alloc.$(OBJ) $(C_) $(GSLTSRC)gslt_alloc.c

$(GSLTOBJ)gslt_init.$(OBJ) : $(GSLTSRC)gslt_init.c
	$(GSLT_CC) $(GSLTO_)gslt_init.$(OBJ) $(C_) $(GSLTSRC)gslt_init.c

$(GSLTOBJ)gslt_image.$(OBJ) : $(GSLTSRC)gslt_image.c
	$(GSLT_CC) $(GSLTO_)gslt_image.$(OBJ) $(C_) $(GSLTSRC)gslt_image.c

$(GSLTOBJ)gslt_image_png.$(OBJ) : $(GSLTSRC)gslt_image_png.c
	$(GSLT_CC) $(GSLTO_)gslt_image_png.$(OBJ) $(C_) $(GSLTSRC)gslt_image_png.c

$(GSLTOBJ)gslt_image_jpeg.$(OBJ) : $(GSLTSRC)gslt_image_jpeg.c
	$(GSLT_CC) $(GSLTO_)gslt_image_jpeg.$(OBJ) $(C_) $(GSLTSRC)gslt_image_jpeg.c

$(GSLTOBJ)gslt_image_tiff.$(OBJ) : $(GSLTSRC)gslt_image_tiff.c
	$(GSLT_CC) $(GSLTO_)gslt_image_tiff.$(OBJ) $(C_) $(GSLTSRC)gslt_image_tiff.c

$(GSLTOBJ)gslt_font_cache.$(OBJ) : $(GSLTSRC)gslt_font_cache.c
	$(GSLT_CC) $(GSLTO_)gslt_font_cache.$(OBJ) $(C_) $(GSLTSRC)gslt_font_cache.c

$(GSLTOBJ)gslt_font_encoding.$(OBJ) : $(GSLTSRC)gslt_font_encoding.c
	$(GSLT_CC) $(GSLTO_)gslt_font_encoding.$(OBJ) $(C_) $(GSLTSRC)gslt_font_encoding.c

$(GSLTOBJ)gslt_font_glyph.$(OBJ) : $(GSLTSRC)gslt_font_glyph.c
	$(GSLT_CC) $(GSLTO_)gslt_font_glyph.$(OBJ) $(C_) $(GSLTSRC)gslt_font_glyph.c

$(GSLTOBJ)gslt_font_ttf.$(OBJ) : $(GSLTSRC)gslt_font_ttf.c
	$(GSLT_CC) $(GSLTO_)gslt_font_ttf.$(OBJ) $(C_) $(GSLTSRC)gslt_font_ttf.c

$(GSLTOBJ)gslt_font_cff.$(OBJ) : $(GSLTSRC)gslt_font_cff.c
	$(GSLT_CC) $(GSLTO_)gslt_font_cff.$(OBJ) $(C_) $(GSLTSRC)gslt_font_cff.c

$(GSLTOBJ)gslt_stubs.$(OBJ) : $(GSLTSRC)gslt_stubs.c
	$(GSLT_CC) $(GSLTO_)gslt_stubs.$(OBJ) $(C_) $(GSLTSRC)gslt_stubs.c
