METSRC      = $(METSRCDIR)$(D)
METGEN      = $(METGENDIR)$(D)
METOBJ      = $(METOBJDIR)$(D)
METO_       = $(O_)$(METOBJ)

METCCC  = $(CC_) $(I_)$(METSRCDIR)$(_I) $(I_)$(METGENDIR)$(_I) $(I_)$(PLSRCDIR)$(_I) $(I_)$(GLSRCDIR)$(_I) $(C_)

# Define the name of this makefile.
MET_MAK     = $(METSRC)met.mak

met.clean: met.config-clean met.clean-not-config-clean

met.clean-not-config-clean:
	$(RM_) $(METOBJ)*.$(OBJ)

met.config-clean: clean_gs
	$(RM_) $(METOBJ)*.dev
	$(RM_) $(METOBJ)devs.tr5

# NB needs expat dependencies.

metparse_h  = $(METSRC)metparse.h \
              $(gx_h)

metstate_h  = $(METSRC)metstate.h \
              $(gx_h)

metsimple_h = $(metsimple.h)

metcomplex_h = $(METSRC)metcomplex.h \
               $(metsimple_h)

metutil_h = $(METSRC)metutil.h

zipparse_h = $(METSRC)zipparse.h

mt_error_h = $(METSRC)mt_error.h

$(METOBJ)metimage.$(OBJ): $(METSRC)metimage.c
	$(METCCC) $(METSRC)metimage.c $(METO_)metimage.$(OBJ)

$(METOBJ)metpage.$(OBJ): $(METSRC)metpage.c
	$(METCCC) $(METSRC)metpage.c $(METO_)metpage.$(OBJ)

$(METOBJ)metpath.$(OBJ): $(METSRC)metpath.c $(metutil_h)
	$(METCCC) $(METSRC)metpath.c $(METO_)metpath.$(OBJ)

$(METOBJ)metglyphs.$(OBJ): $(METSRC)metglyphs.c $(metutil_h)
	$(METCCC) $(METSRC)metglyphs.c $(METO_)metglyphs.$(OBJ)

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

$(METOBJ)mt_error.$(OBJ): $(METSRC)mt_error.c $(mt_error_h)
	$(METCCC) $(METSRC)mt_error.c $(METO_)mt_error.$(OBJ)

$(METOBJ)mt_png.$(OBJ): $(METSRC)mt_png.c $(mt_error_h)
	$(METCCC) $(METSRC)mt_png.c $(METO_)mt_png.$(OBJ)

$(MET_TOP_OBJ): $(METSRC)mettop.c \
                      $(metstate_h)     \
                      $(pltop_h)
	$(CP_) $(METGEN)pconf.h $(METGEN)pconfig.h
	$(METCCC) $(METSRC)mettop.c $(METO_)mettop.$(OBJ)


MET_OBJS=$(METOBJ)metparse.$(OBJ) $(METOBJ)metstate.$(OBJ) \
         $(METOBJ)metundone.$(OBJ) $(METOBJ)metpage.$(OBJ) \
         $(METOBJ)metelement.$(OBJ) $(METOBJ)metpath.$(OBJ)\
	 $(METOBJ)metglyphs.$(OBJ) $(METOBJ)metutil.$(OBJ) \
	 $(METOBJ)metimage.$(OBJ) $(METOBJ)zipparse.$(OBJ) \
	 $(METOBJ)zippart.$(OBJ) $(METOBJ)mt_error.$(OBJ) \
	 $(METOBJ)mt_png.$(OBJ) 

$(METOBJ)met.dev: $(MET_MAK) $(ECHOGS_XE) $(MET_OBJS)
	$(SETMOD) $(METOBJ)met $(MET_OBJS)

