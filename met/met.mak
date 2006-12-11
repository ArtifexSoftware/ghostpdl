XPSSRC      = $(XPSSRCDIR)$(D)
XPSGEN      = $(XPSGENDIR)$(D)
XPSOBJ      = $(XPSOBJDIR)$(D)
XPSO_       = $(O_)$(XPSOBJ)

XPSCCC  = $(CC_) $(I_)$(XPSSRCDIR)$(_I) $(I_)$(XPSGENDIR)$(_I) $(I_)$(PLSRCDIR)$(_I) $(I_)$(GLSRCDIR)$(_I) $(C_)

# $(I_)$(EXPATINCDIR)$(_I)

# Define the name of this makefile.
XPS_MAK     = $(XPSSRC)met.mak

met.clean: met.config-clean met.clean-not-config-clean

met.clean-not-config-clean:
	$(RM_) $(XPSOBJ)*.$(OBJ)

met.config-clean: clean_gs
	$(RM_) $(XPSOBJ)*.dev
	$(RM_) $(XPSOBJ)devs.tr5

# NB needs expat dependencies.

# NB hack - we don't seem to be keeping up with dependencies so make
# all .h files dependent and include them in each target...  fix me
# later

XPSINCLUDES=$(XPSSRC)*.h

metgstate_h = $(XPSSRC)metgstate.h

metparse_h  = $(XPSSRC)metparse.h \
              $(gx_h)

metstate_h  = $(XPSSRC)metstate.h \
              $(gx_h)

metsimple_h = $(metsimple.h)

metcomplex_h = $(XPSSRC)metcomplex.h \
               $(metsimple_h)

metutil_h = $(XPSSRC)metutil.h

zipparse_h = $(XPSSRC)zipparse.h

$(XPSOBJ)metrecorder.$(OBJ): $(XPSSRC)metrecorder.c $(XPSINCLUDES)
	$(XPSCCC) $(XPSSRC)metrecorder.c $(XPSO_)metrecorder.$(OBJ)


$(XPSOBJ)metcanvas.$(OBJ): $(XPSSRC)metcanvas.c $(XPSINCLUDES)
	$(XPSCCC) $(XPSSRC)metcanvas.c $(XPSO_)metcanvas.$(OBJ)

$(XPSOBJ)metimage.$(OBJ): $(XPSSRC)metimage.c $(XPSINCLUDES)
	$(XPSCCC) $(XPSSRC)metimage.c $(XPSO_)metimage.$(OBJ)

$(XPSOBJ)xps_gradient_brush.$(OBJ): $(XPSSRC)xps_gradient_brush.c $(XPSINCLUDES)
	$(XPSCCC) $(XPSSRC)xps_gradient_brush.c $(XPSO_)xps_gradient_brush.$(OBJ)

$(XPSOBJ)xps_gradient_stop.$(OBJ): $(XPSSRC)xps_gradient_stop.c $(XPSINCLUDES)
	$(XPSCCC) $(XPSSRC)xps_gradient_stop.c $(XPSO_)xps_gradient_stop.$(OBJ)

$(XPSOBJ)xps_visual_brush.$(OBJ): $(XPSSRC)xps_visual_brush.c $(XPSINCLUDES)
	$(XPSCCC) $(XPSSRC)xps_visual_brush.c $(XPSO_)xps_visual_brush.$(OBJ)

$(XPSOBJ)metpage.$(OBJ): $(XPSSRC)metpage.c $(XPSINCLUDES)
	$(XPSCCC) $(XPSSRC)metpage.c $(XPSO_)metpage.$(OBJ)

$(XPSOBJ)metpath.$(OBJ): $(XPSSRC)metpath.c $(metutil_h) $(XPSINCLUDES)
	$(XPSCCC) $(XPSSRC)metpath.c $(XPSO_)metpath.$(OBJ)

$(XPSOBJ)metglyphs.$(OBJ): $(XPSSRC)metglyphs.c $(metutil_h) $(XPSINCLUDES)
	$(XPSCCC) $(XPSSRC)metglyphs.c $(XPSO_)metglyphs.$(OBJ)

$(XPSOBJ)metgstate.$(OBJ): $(XPSSRC)metgstate.c $(metgstate_h) $(XPSINCLUDES)
	$(XPSCCC) $(XPSSRC)metgstate.c $(XPSO_)metgstate.$(OBJ)

$(XPSOBJ)metutil.$(OBJ): $(XPSSRC)metutil.c $(metutil_h) $(XPSINCLUDES)
	$(XPSCCC) $(XPSSRC)metutil.c $(XPSO_)metutil.$(OBJ)

$(XPSOBJ)metparse.$(OBJ): $(XPSSRC)metparse.c \
                          $(metparse_h) $(metcomplex_h) $(metelement_h)\
                          $(memory__h) $(XPSINCLUDES)
	$(XPSCCC) $(XPSSRC)metparse.c $(XPSO_)metparse.$(OBJ)

$(XPSOBJ)metstate.$(OBJ): $(XPSSRC)metstate.c \
                          $(metstate_h) $(XPSINCLUDES)
	$(XPSCCC) $(XPSSRC)metstate.c $(XPSO_)metstate.$(OBJ)

$(XPSOBJ)metelement.$(OBJ): $(XPSSRC)metelement.c $(metelement_h) $(XPSINCLUDES)
	$(XPSCCC) $(XPSSRC)metelement.c $(XPSO_)metelement.$(OBJ)

$(XPSOBJ)metundone.$(OBJ): $(XPSSRC)metundone.c $(XPSINCLUDES)
	$(XPSCCC) $(XPSSRC)metundone.c $(XPSO_)metundone.$(OBJ)

$(XPSOBJ)zippart.$(OBJ): $(XPSSRC)zippart.c $(zipparse_h) $(XPSINCLUDES)
	$(XPSCCC) $(XPSSRC)zippart.c $(XPSO_)zippart.$(OBJ)

$(XPSOBJ)zipparse.$(OBJ): $(XPSSRC)zipparse.c $(zipparse_h) $(pltop_h) $(XPSINCLUDES)
	$(XPSCCC) $(XPSSRC)zipparse.c $(XPSO_)zipparse.$(OBJ)

$(XPSOBJ)xps_image_jpeg.$(OBJ): $(XPSSRC)xps_image_jpeg.c $(xps_error_h) $(XPSINCLUDES)
	$(XPSCCC) $(XPSSRC)xps_image_jpeg.c $(XPSO_)xps_image_jpeg.$(OBJ)

$(XPSOBJ)xps_image_png.$(OBJ): $(XPSSRC)xps_image_png.c $(xps_error_h) $(XPSINCLUDES)
	$(XPSCCC) $(I_)$(PSRCDIR)$(_I) $(XPSSRC)xps_image_png.c $(XPSO_)xps_image_png.$(OBJ)

$(XPSOBJ)xps_image_tiff.$(OBJ): $(XPSSRC)xps_image_tiff.c $(xps_error_h) $(XPSINCLUDES)
	$(XPSCCC) $(XPSSRC)xps_image_tiff.c $(XPSO_)xps_image_tiff.$(OBJ)

$(XPSOBJ)xps_gradient_draw.$(OBJ): $(XPSSRC)xps_gradient_draw.c $(xps_error_h) $(XPSINCLUDES)
	$(XPSCCC) $(XPSSRC)xps_gradient_draw.c $(XPSO_)xps_gradient_draw.$(OBJ)

$(XPS_TOP_OBJ): $(XPSSRC)mettop.c \
                      $(metstate_h)     \
                      $(pltop_h) $(XPSINCLUDES)
	$(CP_) $(XPSGEN)pconf.h $(XPSGEN)pconfig.h
	$(XPSCCC) $(XPSSRC)mettop.c $(XPSO_)mettop.$(OBJ)

XPS_OBJS=$(XPSOBJ)metparse.$(OBJ) $(XPSOBJ)metstate.$(OBJ) \
         $(XPSOBJ)metundone.$(OBJ) $(XPSOBJ)metpage.$(OBJ) \
         $(XPSOBJ)metelement.$(OBJ) $(XPSOBJ)metpath.$(OBJ)\
	 $(XPSOBJ)metglyphs.$(OBJ) $(XPSOBJ)metutil.$(OBJ) \
	 $(XPSOBJ)metimage.$(OBJ) \
	 $(XPSOBJ)xps_gradient_brush.$(OBJ) \
	 $(XPSOBJ)xps_gradient_stop.$(OBJ) \
	 $(XPSOBJ)xps_visual_brush.$(OBJ) \
	 $(XPSOBJ)zipparse.$(OBJ) $(XPSOBJ)zippart.$(OBJ) \
	 $(XPSOBJ)xps_image_jpeg.$(OBJ) \
	 $(XPSOBJ)xps_image_png.$(OBJ) \
	 $(XPSOBJ)xps_image_tiff.$(OBJ) \
	 $(XPSOBJ)xps_gradient_draw.$(OBJ) \
	 $(XPSOBJ)metgstate.$(OBJ) \
	 $(XPSOBJ)metcanvas.$(OBJ) \
	 $(XPSOBJ)metrecorder.$(OBJ)

$(XPSOBJ)xps.dev: $(XPS_MAK) $(ECHOGS_XE) $(XPS_OBJS)
	$(SETMOD) $(XPSOBJ)xps $(XPS_OBJS)

