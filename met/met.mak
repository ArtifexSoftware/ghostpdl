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


$(METOBJ)metparse.$(OBJ): $(METSRC)metparse.c \
                          $(metparse_h)
	$(METCCC) $(METSRC)metparse.c $(METO_)metparse.$(OBJ)

$(METOBJ)metstate.$(OBJ): $(METSRC)metstate.c \
                          $(metstate_h)
	$(METCCC) $(METSRC)metstate.c $(METO_)metstate.$(OBJ)

$(MET_TOP_OBJ):           $(METSRC)mettop.c \
                          $(metstate_h)     \
                          $(pltop_h)
	$(CP_) $(METGEN)pconf.h $(METGEN)pconfig.h
	$(METCCC) $(METSRC)mettop.c $(METO_)mettop.$(OBJ)


MET_OBJS=$(METOBJ)metparse.$(OBJ) $(METOBJ)metstate.$(OBJ)

$(METOBJ)met.dev: $(MET_MAK) $(ECHOGS_XE) $(MET_OBJS)
	$(SETMOD) $(METOBJ)met $(MET_OBJS)

