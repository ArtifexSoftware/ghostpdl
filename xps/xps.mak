XPSSRC      = $(XPSSRCDIR)$(D)
XPSGEN      = $(XPSGENDIR)$(D)
XPSOBJ      = $(XPSOBJDIR)$(D)
XPSO_       = $(O_)$(XPSOBJ)

XPSCCC  = $(CC_) $(I_)$(XPSSRCDIR)$(_I) $(I_)$(XPSGENDIR)$(_I) $(I_)$(PLSRCDIR)$(_I) $(I_)$(GLSRCDIR)$(_I) $(C_) $(I_)$(EXPATINCDIR)$(_I)

# Define the name of this makefile.
XPS_MAK     = $(XPSSRC)xps.mak

xps.clean: xps.config-clean xps.clean-not-config-clean

xps.clean-not-config-clean:
	$(RM_) $(XPSOBJ)*.$(OBJ)

xps.config-clean: clean_gs
	$(RM_) $(XPSOBJ)*.dev
	$(RM_) $(XPSOBJ)devs.tr5


XPSINCLUDES=$(XPSSRC)*.h


$(XPSOBJ)xpsmem.$(OBJ): $(XPSSRC)xpsmem.c $(XPSINCLUDES)
	$(XPSCCC) $(XPSSRC)xpsmem.c $(XPSO_)xpsmem.$(OBJ)

$(XPSOBJ)xpszip.$(OBJ): $(XPSSRC)xpszip.c $(XPSINCLUDES)
	$(XPSCCC) $(XPSSRC)xpszip.c $(XPSO_)xpszip.$(OBJ)


$(XPS_TOP_OBJ): $(XPSSRC)xpstop.c $(pltop_h) $(XPSINCLUDES)
	$(CP_) $(XPSGEN)pconf.h $(XPSGEN)pconfig.h
	$(XPSCCC) $(XPSSRC)xpstop.c $(XPSO_)xpstop.$(OBJ)

XPS_OBJS=\
    $(XPSOBJ)xpsmem.$(OBJ) \
    $(XPSOBJ)xpszip.$(OBJ) \

$(XPSOBJ)xps.dev: $(XPS_MAK) $(ECHOGS_XE) $(XPS_OBJS)
	$(SETMOD) $(XPSOBJ)xps $(XPS_OBJS)

