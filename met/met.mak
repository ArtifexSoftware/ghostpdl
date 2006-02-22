METSRC      = $(METSRCDIR)$(D)
METGEN      = $(METGENDIR)$(D)
METOBJ      = $(METOBJDIR)$(D)
METO_       = $(O_)$(METOBJ)

METCCC  = $(CC_) $(I_)$(METSRCDIR)$(_I) $(I_)$(METGENDIR)$(_I) $(I_)$(PLSRCDIR)$(_I) $(I_)$(GLSRCDIR)$(_I) $(C_)

# METCCC  = $(CC_) $(I_)$(METSRCDIR)$(_I) $(I_)$(METGENDIR)$(_I) $(I_)$(PLSRCDIR)$(_I) $(I_)$(GLSRCDIR)$(_I) $(C_) -I/opt/local/include

# Define the name of this makefile.
MET_MAK     = $(METSRC)met.mak

met.clean: met.config-clean met.clean-not-config-clean

met.clean-not-config-clean:
	$(RM_) $(METOBJ)*.$(OBJ)

met.config-clean: clean_gs
	$(RM_) $(METOBJ)*.dev
	$(RM_) $(METOBJ)devs.tr5

# NB needs expat dependencies.

metgstate_h = $(METSRC)metgstate.h

metparse_h  = $(METSRC)metparse.h \
              $(gx_h)

metstate_h  = $(METSRC)metstate.h \
              $(gx_h)

metsimple_h = $(metsimple.h)

metcomplex_h = $(METSRC)metcomplex.h \
               $(metsimple_h)

metutil_h = $(METSRC)metutil.h

zipparse_h = $(METSRC)zipparse.h

$(METOBJ)metrecorder.$(OBJ): $(METSRC)metrecorder.c
	$(METCCC) $(METSRC)metrecorder.c $(METO_)metrecorder.$(OBJ)


$(METOBJ)metcanvas.$(OBJ): $(METSRC)metcanvas.c
	$(METCCC) $(METSRC)metcanvas.c $(METO_)metcanvas.$(OBJ)

$(METOBJ)metimage.$(OBJ): $(METSRC)metimage.c
	$(METCCC) $(METSRC)metimage.c $(METO_)metimage.$(OBJ)

$(METOBJ)xps_gradient_brush.$(OBJ): $(METSRC)xps_gradient_brush.c
	$(METCCC) $(METSRC)xps_gradient_brush.c $(METO_)xps_gradient_brush.$(OBJ)

$(METOBJ)metpage.$(OBJ): $(METSRC)metpage.c
	$(METCCC) $(METSRC)metpage.c $(METO_)metpage.$(OBJ)

$(METOBJ)metpath.$(OBJ): $(METSRC)metpath.c $(metutil_h)
	$(METCCC) $(METSRC)metpath.c $(METO_)metpath.$(OBJ)

$(METOBJ)metglyphs.$(OBJ): $(METSRC)metglyphs.c $(metutil_h)
	$(METCCC) $(METSRC)metglyphs.c $(METO_)metglyphs.$(OBJ)

$(METOBJ)metgstate.$(OBJ): $(METSRC)metgstate.c $(metgstate_h)
	$(METCCC) $(METSRC)metgstate.c $(METO_)metgstate.$(OBJ)

$(METOBJ)metutil.$(OBJ): $(METSRC)metutil.c $(metutil_h)
	$(METCCC) $(METSRC)metutil.c $(METO_)metutil.$(OBJ)

$(METOBJ)metparse.$(OBJ): $(METSRC)metparse.c \
                          $(metparse_h) $(metcomplex_h) $(metelement_h)\
                          $(memory__h)
	$(METCCC) $(METSRC)metparse.c $(METO_)metparse.$(OBJ)

$(METOBJ)metstate.$(OBJ): $(METSRC)metstate.c \
                          $(metstate_h)
	$(METCCC) $(METSRC)metstate.c $(METO_)metstate.$(OBJ)

$(METOBJ)metelement.$(OBJ): $(METSRC)metelement.c $(metelement_h)
	$(METCCC) $(METSRC)metelement.c $(METO_)metelement.$(OBJ)

$(METOBJ)metundone.$(OBJ): $(METSRC)metundone.c
	$(METCCC) $(METSRC)metundone.c $(METO_)metundone.$(OBJ)

$(METOBJ)zippart.$(OBJ): $(METSRC)zippart.c $(zipparse_h)
	$(METCCC) $(METSRC)zippart.c $(METO_)zippart.$(OBJ)

$(METOBJ)zipparse.$(OBJ): $(METSRC)zipparse.c $(zipparse_h) $(pltop_h)
	$(METCCC) $(METSRC)zipparse.c $(METO_)zipparse.$(OBJ)

$(METOBJ)xps_image_jpeg.$(OBJ): $(METSRC)xps_image_jpeg.c $(xps_error_h)
	$(METCCC) $(METSRC)xps_image_jpeg.c $(METO_)xps_image_jpeg.$(OBJ)
$(METOBJ)xps_image_png.$(OBJ): $(METSRC)xps_image_png.c $(xps_error_h)
	$(METCCC) $(METSRC)xps_image_png.c $(METO_)xps_image_png.$(OBJ)
$(METOBJ)xps_image_tiff.$(OBJ): $(METSRC)xps_image_tiff.c $(xps_error_h)
	$(METCCC) $(METSRC)xps_image_tiff.c $(METO_)xps_image_tiff.$(OBJ)

$(METOBJ)xps_gradient_draw.$(OBJ): $(METSRC)xps_gradient_draw.c $(xps_error_h)
	$(METCCC) $(METSRC)xps_gradient_draw.c $(METO_)xps_gradient_draw.$(OBJ)

$(MET_TOP_OBJ): $(METSRC)mettop.c \
                      $(metstate_h)     \
                      $(pltop_h)
	$(CP_) $(METGEN)pconf.h $(METGEN)pconfig.h
	$(METCCC) $(METSRC)mettop.c $(METO_)mettop.$(OBJ)

MET_OBJS=$(METOBJ)metparse.$(OBJ) $(METOBJ)metstate.$(OBJ) \
         $(METOBJ)metundone.$(OBJ) $(METOBJ)metpage.$(OBJ) \
         $(METOBJ)metelement.$(OBJ) $(METOBJ)metpath.$(OBJ)\
	 $(METOBJ)metglyphs.$(OBJ) $(METOBJ)metutil.$(OBJ) \
	 $(METOBJ)metimage.$(OBJ) $(METOBJ)xps_gradient_brush.$(OBJ) \
	 $(METOBJ)zipparse.$(OBJ) $(METOBJ)zippart.$(OBJ) \
	 $(METOBJ)xps_image_jpeg.$(OBJ) \
	 $(METOBJ)xps_image_png.$(OBJ) \
	 $(METOBJ)xps_image_tiff.$(OBJ) \
	 $(METOBJ)xps_gradient_draw.$(OBJ) \
	 $(METOBJ)metgstate.$(OBJ) \
	 $(METOBJ)metcanvas.$(OBJ) \
	 $(METOBJ)metrecorder.$(OBJ)

$(METOBJ)met.dev: $(MET_MAK) $(ECHOGS_XE) $(MET_OBJS)
	$(SETMOD) $(METOBJ)met $(MET_OBJS)

