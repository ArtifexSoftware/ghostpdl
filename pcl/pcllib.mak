#    Copyright (C) 1996 Aladdin Enterprises.  All rights reserved.
#    Unauthorized use, copying, and/or distribution prohibited.

# makefile for PCL* interpreter libraries and for PJL

# Currently, the PJL parser is just a stub so we can run the Genoa tests.
# Eventually it will be integrated properly.

# Define the name of this makefile.
PCLLIB_MAK=pcllib.mak

################ PJL ################

# As noted above, this is just a stub.

pjparse_h=pjparse.h

pjparse.$(OBJ): pjparse.c $(memory__h)\
 $(scommon_h) $(pjparse_h)

PJL=pjparse.$(OBJ)

pjl.dev: $(PCLLIB_MAK) $(ECHOGS_XE) $(PJL)
	$(SETMOD) pjl $(PJL)

################ Shared libraries ################

pldict_h=pldict.h
plmain_h=plmain.h
plsymbol_h=plsymbol.h
plvalue_h=plvalue.h
plvocab_h=plvocab.h
unicode1_h=unicode1.h
# Out of order because of inclusion
plfont_h=plfont.h $(gsccode_h) $(plsymbol_h)

plchar.$(OBJ): plchar.c $(AK) $(math__h) $(memory__h) $(stdio__h)\
 $(gdebug_h)\
 $(gschar_h) $(gscoord_h) $(gserror_h) $(gserrors_h) $(gsimage_h)\
 $(gsmatrix_h) $(gsmemory_h) $(gspaint_h) $(gspath_h)\
 $(gsstate_h) $(gsstruct_h) $(gstypes_h)\
 $(gxarith_h) $(gxchar_h) $(gxfcache_h) $(gxdevice_h) $(gxdevmem_h)\
 $(gxfixed_h) $(gxfont_h) $(gxfont42_h) $(gxpath_h) $(gzstate_h)\
 $(plfont_h) $(plvalue_h)

pldict.$(OBJ): pldict.c $(AK) $(memory__h)\
 $(gsmemory_h) $(gsstruct_h) $(gstypes_h)\
 $(pldict_h)

plfont.$(OBJ): plfont.c $(AK) $(memory__h) $(stdio__h)\
 $(gdebug_h)\
 $(gschar_h) $(gserror_h) $(gserrors_h) $(gsmatrix_h) $(gsmemory_h)\
 $(gsstate_h) $(gsstruct_h) $(gstypes_h)\
 $(gxfont_h) $(gxfont42_h)\
 $(plfont_h) $(plvalue_h)

plmain.$(OBJ): plmain.c $(AK) $(stdio__h) $(string__h)\
 $(gdebug_h) $(gscdefs_h) $(gsdevice_h) $(gslib_h)\
 $(gsmatrix_h) $(gsmemory_h) $(gsparam_h) $(gsstate_h) $(gstypes_h)\
 $(plmain_h)

plsymbol.$(OBJ): plsymbol.c $(AK) $(stdpre_h)\
 $(plsymbol_h)

plvalue.$(OBJ): plvalue.c $(AK) $(std_h)\
 $(plvalue_h)
	$(CCLEAF) plvalue.c

plvocab.$(OBJ): plvocab.c $(AK) $(stdpre_h)\
 $(plvocab_h)

PCL_LIB1=plchar.$(OBJ) pldict.$(OBJ) plfont.$(OBJ)
PCL_LIB2=plmain.$(OBJ) plsymbol.$(OBJ) plvalue.$(OBJ) plvocab.$(OBJ)
PCL_LIB=$(PCL_LIB1) $(PCL_LIB2)

pcllib.dev: $(PCLLIB_MAK) $(ECHOGS_XE) $(PCL_LIB)
	$(SETMOD) pcllib $(PCL_LIB1)
	$(ADDMOD) pcllib $(PCL_LIB2)
