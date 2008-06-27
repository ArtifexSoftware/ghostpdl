SVGSRC      = $(SVGSRCDIR)$(D)
SVGGEN      = $(SVGGENDIR)$(D)
SVGOBJ      = $(SVGOBJDIR)$(D)
SVGO_       = $(O_)$(SVGOBJ)
EXPATINCDIR = $(EXPATSRCDIR)$(D)lib
PLOBJ       = $(PLOBJDIR)$(D)

SVGCCC  = $(CC_) $(I_)$(SVGSRCDIR)$(_I) $(I_)$(SVGGENDIR)$(_I) $(I_)$(PLSRCDIR)$(_I) $(I_)$(GLSRCDIR)$(_I) $(I_)$(EXPATINCDIR)$(_I) $(I_)$(ZSRCDIR)$(_I) $(C_)

# Define the name of this makefile.
SVG_MAK     = $(SVGSRC)svg.mak

svg.clean: svg.config-clean svg.clean-not-config-clean

svg.clean-not-config-clean:
	$(RM_) $(SVGOBJ)*.$(OBJ)

svg.config-clean: clean_gs
	$(RM_) $(SVGOBJ)*.dev
	$(RM_) $(SVGOBJ)devs.tr5


SVGINCLUDES=$(SVGSRC)*.h


$(SVGOBJ)svgxml.$(OBJ): $(SVGSRC)svgxml.c $(SVGINCLUDES)
	$(SVGCCC) $(SVGSRC)svgxml.c $(SVGO_)svgxml.$(OBJ)

$(SVGOBJ)svgdoc.$(OBJ): $(SVGSRC)svgdoc.c $(SVGINCLUDES)
	$(SVGCCC) $(SVGSRC)svgdoc.c $(SVGO_)svgdoc.$(OBJ)


$(SVG_TOP_OBJ): $(SVGSRC)svgtop.c $(pltop_h) $(SVGINCLUDES)
	$(CP_) $(SVGGEN)pconf.h $(SVGGEN)pconfig.h
	$(SVGCCC) $(SVGSRC)svgtop.c $(SVGO_)svgtop.$(OBJ)

SVG_OBJS=\
    $(SVGOBJ)svgxml.$(OBJ) \
    $(SVGOBJ)svgdoc.$(OBJ) \

# NB - note this is a bit squirrely.  Right now the pjl interpreter is
# required and shouldn't be and PLOBJ==SVGGEN is required.

$(SVGOBJ)svg.dev: $(SVG_MAK) $(ECHOGS_XE) $(SVG_OBJS)  $(SVGGEN)expat.dev \
		  $(SVGGEN)pl.dev $(SVGGEN)$(PL_SCALER).dev $(SVGGEN)pjl.dev
	$(SETMOD) $(SVGOBJ)svg $(SVG_OBJS)
	$(ADDMOD) $(SVGOBJ)svg -include $(SVGGEN)expat $(SVGGEN)pl $(SVGGEN)$(PL_SCALER) $(SVGGEN)pjl.dev

