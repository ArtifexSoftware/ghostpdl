#    Copyright (C) 1994, 1996, 1997 Aladdin Enterprises.  All rights reserved.
# 
# This file is part of Aladdin Ghostscript.
# 
# Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
# or distributor accepts any responsibility for the consequences of using it,
# or for whether it serves any particular purpose or works at all, unless he
# or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
# License (the "License") for full details.
# 
# Every copy of Aladdin Ghostscript must include a copy of the License,
# normally in a plain ASCII text file named PUBLIC.  The License grants you
# the right to copy, modify and redistribute Aladdin Ghostscript, but only
# under certain conditions described in the License.  Among other things, the
# License requires that the copyright notice and this notice be preserved on
# all copies.

# makefile for Independent JPEG Group library code.

# NOTE: This makefile is only known to work with the following versions
# of the IJG library: 6, 6a.
# As of May 11, 1996, version 6a is the current version.
#
# You can get the IJG library by Internet anonymous FTP from the following
# places:
#	Standard distribution (tar + gzip format, Unix end-of-line):
#		ftp.uu.net:/graphics/jpeg/jpegsrc.v*.tar.gz
#		ftp.cs.wisc.edu:/ghost/jpegsrc.v*.tar.gz
#	MS-DOS archive (PKZIP a.k.a. zip format, MS-DOS end-of-line):
#		ftp.simtel.net:/pub/simtelnet/msdos/graphics/jpegsr*.zip
#		ftp.cs.wisc.edu:/ghost/jpeg-*.zip
# The first site named above (ftp.uu.net and ftp.simtel.net) is supposed
# to be the master distribution site, so it may have a more up-to-date
# version; the ftp.cs.wisc.edu site is the master distribution site for
# Ghostscript, so it will always have IJG library versions known to be
# compatible with Ghostscript.
#
# If the version number, and hence the subdirectory name, changes, you
# will probably want to change the definitions of JSRCDIR and possibly
# JVERSION (in the platform-specific makefile, not here) to reflect this,
# since that way you can use the IJG archive without change.
#
# NOTE: For some obscure reason (probably a bug in djtarx), if you are
# compiling on a DesqView/X system, you should use the zip version of the
# IJG library, not the tar.gz version.

# Define the name of this makefile.
JPEG_MAK=jpeg.mak

# JSRCDIR is defined in the platform-specific makefile, not here,
# as the directory where the IJG library sources are stored.
#JSRCDIR=jpeg-6a
# JVERSION is defined in the platform-specific makefile, not here,
# as the IJG library major version number (currently "5" or "6").
#JVERSION=6

JSRC=$(JSRCDIR)$(D)
# CCCJ is defined in gs.mak.
#CCCJ=$(CCC) -I. -I$(JSRCDIR)

# We keep all of the IJG code in a separate directory so as not to
# inadvertently mix it up with Aladdin Enterprises' own code.
# However, we need our own version of jconfig.h, and our own "wrapper" for
# jmorecfg.h.  We also need a substitute for jerror.c, in order to
# keep the error strings out of the automatic data segment in
# 16-bit environments.  For v5*, we also need our own version of jpeglib.h
# in order to change MAX_BLOCKS_IN_MCU for Adobe compatibility.
# (This need will go away when IJG v6 is released.)

# Because this file is included after lib.mak, we can't use _h macros
# to express indirect dependencies; instead, we build the dependencies
# into the rules for copying the files.
jconfig_h=jconfig.h
jerror_h=jerror.h
jmorecfg_h=jmorecfg.h
jpeglib_h=jpeglib.h

jconfig.h: gsjconf.h $(std_h)
	$(CP_) gsjconf.h jconfig.h

jmorecfg.h: gsjmorec.h jmcorig.h
	$(CP_) gsjmorec.h jmorecfg.h

jmcorig.h: $(JSRC)jmorecfg.h
	$(CP_) $(JSRC)jmorecfg.h jmcorig.h

jpeglib.h: jlib$(JVERSION).h jconfig.h jmorecfg.h
	$(CP_) jlib$(JVERSION).h jpeglib.h

jlib5.h: gsjpglib.h
	$(CP_) gsjpglib.h jlib5.h

jlib6.h: $(JSRC)jpeglib.h
	$(CP_) $(JSRC)jpeglib.h jlib6.h

# To ensure that the compiler finds our versions of jconfig.h and jmorecfg.h,
# regardless of the compiler's search rule, we must copy up all .c files,
# and all .h files that include either of these files, directly or
# indirectly.  The only such .h files currently are jinclude.h and jpeglib.h.
# (Currently, we supply our own version of jpeglib.h -- see above.)
# Also, to avoid including the JSRCDIR directory name in our source files,
# we must also copy up any other .h files that our own code references.
# Currently, the only such .h files are jerror.h and jversion.h.

JHCOPY=jinclude.h jpeglib.h jerror.h jversion.h

jinclude.h: $(JSRC)jinclude.h
	$(CP_) $(JSRC)jinclude.h jinclude.h

#jpeglib.h: $(JSRC)jpeglib.h
#	$(CP_) $(JSRC)jpeglib.h jpeglib.h

jerror.h: $(JSRC)jerror.h
	$(CP_) $(JSRC)jerror.h jerror.h

jversion.h: $(JSRC)jversion.h
	$(CP_) $(JSRC)jversion.h jversion.h

# In order to avoid having to keep the dependency lists for the IJG code
# accurate, we simply make all of them depend on the only files that
# we are ever going to change, and on all the .h files that must be copied up.
# This is too conservative, but only hurts us if we are changing our own
# j*.h files, which happens only rarely during development.

JDEP=$(AK) $(jconfig_h) $(jerror_h) $(jmorecfg_h) $(JHCOPY)

# Code common to compression and decompression.

jpegc_=jcomapi.$(OBJ) jutils.$(OBJ) sjpegerr.$(OBJ) jmemmgr.$(OBJ)
jpegc.dev: $(JPEG_MAK) $(ECHOGS_XE) $(jpegc_)
	$(SETMOD) jpegc $(jpegc_)

jcomapi.$(OBJ): $(JSRC)jcomapi.c $(JDEP)
	$(CP_) $(JSRC)jcomapi.c .
	$(CCCJ) jcomapi.c
	$(RM_) jcomapi.c

jutils.$(OBJ): $(JSRC)jutils.c $(JDEP)
	$(CP_) $(JSRC)jutils.c .
	$(CCCJ) jutils.c
	$(RM_) jutils.c

# Note that sjpegerr replaces jerror.
sjpegerr.$(OBJ): sjpegerr.c $(JDEP)
	$(CCCF) sjpegerr.c

jmemmgr.$(OBJ): $(JSRC)jmemmgr.c $(JDEP)
	$(CP_) $(JSRC)jmemmgr.c .
	$(CCCJ) jmemmgr.c
	$(RM_) jmemmgr.c

# Encoding (compression) code.

jpege.dev: jpege$(JVERSION).dev
	$(CP_) jpege$(JVERSION).dev jpege.dev

jpege5=jcapi.$(OBJ)
jpege6=jcapimin.$(OBJ) jcapistd.$(OBJ) jcinit.$(OBJ)

jpege_1=jccoefct.$(OBJ) jccolor.$(OBJ) jcdctmgr.$(OBJ) 
jpege_2=jchuff.$(OBJ) jcmainct.$(OBJ) jcmarker.$(OBJ) jcmaster.$(OBJ)
jpege_3=jcparam.$(OBJ) jcprepct.$(OBJ) jcsample.$(OBJ) jfdctint.$(OBJ)

jpege5.dev: $(JPEG_MAK) $(ECHOGS_XE) jpegc.dev $(jpege5) $(jpege_1) $(jpege_2) $(jpege_3)
	$(SETMOD) jpege5 $(jpege5)
	$(ADDMOD) jpege5 -include jpegc
	$(ADDMOD) jpege5 -obj $(jpege_1)
	$(ADDMOD) jpege5 -obj $(jpege_2)
	$(ADDMOD) jpege5 -obj $(jpege_3)

jpege6.dev: $(JPEG_MAK) $(ECHOGS_XE) jpegc.dev $(jpege6) $(jpege_1) $(jpege_2) $(jpege_3)
	$(SETMOD) jpege6 $(jpege6)
	$(ADDMOD) jpege6 -include jpegc
	$(ADDMOD) jpege6 -obj $(jpege_1)
	$(ADDMOD) jpege6 -obj $(jpege_2)
	$(ADDMOD) jpege6 -obj $(jpege_3)

# jcapi.c is v5* only
jcapi.$(OBJ): $(JSRC)jcapi.c $(JDEP)
	$(CP_) $(JSRC)jcapi.c .
	$(CCCJ) jcapi.c
	$(RM_) jcapi.c
  
# jcapimin.c is new in v6
jcapimin.$(OBJ): $(JSRC)jcapimin.c $(JDEP)
	$(CP_) $(JSRC)jcapimin.c .
	$(CCCJ) jcapimin.c
	$(RM_) jcapimin.c

# jcapistd.c is new in v6
jcapistd.$(OBJ): $(JSRC)jcapistd.c $(JDEP)
	$(CP_) $(JSRC)jcapistd.c .
	$(CCCJ) jcapistd.c
	$(RM_) jcapistd.c

# jcinit.c is new in v6
jcinit.$(OBJ): $(JSRC)jcinit.c $(JDEP)
	$(CP_) $(JSRC)jcinit.c .
	$(CCCJ) jcinit.c
	$(RM_) jcinit.c

jccoefct.$(OBJ): $(JSRC)jccoefct.c $(JDEP)
	$(CP_) $(JSRC)jccoefct.c .
	$(CCCJ) jccoefct.c
	$(RM_) jccoefct.c

jccolor.$(OBJ): $(JSRC)jccolor.c $(JDEP)
	$(CP_) $(JSRC)jccolor.c .
	$(CCCJ) jccolor.c
	$(RM_) jccolor.c

jcdctmgr.$(OBJ): $(JSRC)jcdctmgr.c $(JDEP)
	$(CP_) $(JSRC)jcdctmgr.c .
	$(CCCJ) jcdctmgr.c
	$(RM_) jcdctmgr.c

jchuff.$(OBJ): $(JSRC)jchuff.c $(JDEP)
	$(CP_) $(JSRC)jchuff.c .
	$(CCCJ) jchuff.c
	$(RM_) jchuff.c

jcmainct.$(OBJ): $(JSRC)jcmainct.c $(JDEP)
	$(CP_) $(JSRC)jcmainct.c .
	$(CCCJ) jcmainct.c
	$(RM_) jcmainct.c

jcmarker.$(OBJ): $(JSRC)jcmarker.c $(JDEP)
	$(CP_) $(JSRC)jcmarker.c .
	$(CCCJ) jcmarker.c
	$(RM_) jcmarker.c

jcmaster.$(OBJ): $(JSRC)jcmaster.c $(JDEP)
	$(CP_) $(JSRC)jcmaster.c .
	$(CCCJ) jcmaster.c
	$(RM_) jcmaster.c

jcparam.$(OBJ): $(JSRC)jcparam.c $(JDEP)
	$(CP_) $(JSRC)jcparam.c .
	$(CCCJ) jcparam.c
	$(RM_) jcparam.c

jcprepct.$(OBJ): $(JSRC)jcprepct.c $(JDEP)
	$(CP_) $(JSRC)jcprepct.c .
	$(CCCJ) jcprepct.c
	$(RM_) jcprepct.c

jcsample.$(OBJ): $(JSRC)jcsample.c $(JDEP)
	$(CP_) $(JSRC)jcsample.c .
	$(CCCJ) jcsample.c
	$(RM_) jcsample.c

jfdctint.$(OBJ): $(JSRC)jfdctint.c $(JDEP)
	$(CP_) $(JSRC)jfdctint.c .
	$(CCCJ) jfdctint.c
	$(RM_) jfdctint.c

# Decompression code

jpegd.dev: jpegd$(JVERSION).dev
	$(CP_) jpegd$(JVERSION).dev jpegd.dev

jpegd5=jdapi.$(OBJ)
jpegd6=jdapimin.$(OBJ) jdapistd.$(OBJ) jdinput.$(OBJ) jdphuff.$(OBJ)

jpegd_1=jdcoefct.$(OBJ) jdcolor.$(OBJ)
jpegd_2=jddctmgr.$(OBJ) jdhuff.$(OBJ) jdmainct.$(OBJ) jdmarker.$(OBJ)
jpegd_3=jdmaster.$(OBJ) jdpostct.$(OBJ) jdsample.$(OBJ) jidctint.$(OBJ)

jpegd5.dev: $(JPEG_MAK) $(ECHOGS_XE) jpegc.dev $(jpegd5) $(jpegd_1) $(jpegd_2) $(jpegd_3)
	$(SETMOD) jpegd5 $(jpegd5)
	$(ADDMOD) jpegd5 -include jpegc
	$(ADDMOD) jpegd5 -obj $(jpegd_1)
	$(ADDMOD) jpegd5 -obj $(jpegd_2)
	$(ADDMOD) jpegd5 -obj $(jpegd_3)

jpegd6.dev: $(JPEG_MAK) $(ECHOGS_XE) jpegc.dev $(jpegd6) $(jpegd_1) $(jpegd_2) $(jpegd_3)
	$(SETMOD) jpegd6 $(jpegd6)
	$(ADDMOD) jpegd6 -include jpegc
	$(ADDMOD) jpegd6 -obj $(jpegd_1)
	$(ADDMOD) jpegd6 -obj $(jpegd_2)
	$(ADDMOD) jpegd6 -obj $(jpegd_3)

# jdapi.c is v5* only
jdapi.$(OBJ): $(JSRC)jdapi.c $(JDEP)
	$(CP_) $(JSRC)jdapi.c .
	$(CCCJ) jdapi.c
	$(RM_) jdapi.c

# jdapimin.c is new in v6
jdapimin.$(OBJ): $(JSRC)jdapimin.c $(JDEP)
	$(CP_) $(JSRC)jdapimin.c .
	$(CCCJ) jdapimin.c
	$(RM_) jdapimin.c

# jdapistd.c is new in v6
jdapistd.$(OBJ): $(JSRC)jdapistd.c $(JDEP)
	$(CP_) $(JSRC)jdapistd.c .
	$(CCCJ) jdapistd.c
	$(RM_) jdapistd.c

jdcoefct.$(OBJ): $(JSRC)jdcoefct.c $(JDEP)
	$(CP_) $(JSRC)jdcoefct.c .
	$(CCCJ) jdcoefct.c
	$(RM_) jdcoefct.c

jdcolor.$(OBJ): $(JSRC)jdcolor.c $(JDEP)
	$(CP_) $(JSRC)jdcolor.c .
	$(CCCJ) jdcolor.c
	$(RM_) jdcolor.c

jddctmgr.$(OBJ): $(JSRC)jddctmgr.c $(JDEP)
	$(CP_) $(JSRC)jddctmgr.c .
	$(CCCJ) jddctmgr.c
	$(RM_) jddctmgr.c

jdhuff.$(OBJ): $(JSRC)jdhuff.c $(JDEP)
	$(CP_) $(JSRC)jdhuff.c .
	$(CCCJ) jdhuff.c
	$(RM_) jdhuff.c

# jdinput.c is new in v6
jdinput.$(OBJ): $(JSRC)jdinput.c $(JDEP)
	$(CP_) $(JSRC)jdinput.c .
	$(CCCJ) jdinput.c
	$(RM_) jdinput.c

jdmainct.$(OBJ): $(JSRC)jdmainct.c $(JDEP)
	$(CP_) $(JSRC)jdmainct.c .
	$(CCCJ) jdmainct.c
	$(RM_) jdmainct.c

jdmarker.$(OBJ): $(JSRC)jdmarker.c $(JDEP)
	$(CP_) $(JSRC)jdmarker.c .
	$(CCCJ) jdmarker.c
	$(RM_) jdmarker.c

jdmaster.$(OBJ): $(JSRC)jdmaster.c $(JDEP)
	$(CP_) $(JSRC)jdmaster.c .
	$(CCCJ) jdmaster.c
	$(RM_) jdmaster.c

# jdphuff.c is new in v6
jdphuff.$(OBJ): $(JSRC)jdphuff.c $(JDEP)
	$(CP_) $(JSRC)jdphuff.c .
	$(CCCJ) jdphuff.c
	$(RM_) jdphuff.c

jdpostct.$(OBJ): $(JSRC)jdpostct.c $(JDEP)
	$(CP_) $(JSRC)jdpostct.c .
	$(CCCJ) jdpostct.c
	$(RM_) jdpostct.c

jdsample.$(OBJ): $(JSRC)jdsample.c $(JDEP)
	$(CP_) $(JSRC)jdsample.c .
	$(CCCJ) jdsample.c
	$(RM_) jdsample.c

jidctint.$(OBJ): $(JSRC)jidctint.c $(JDEP)
	$(CP_) $(JSRC)jidctint.c .
	$(CCCJ) jidctint.c
	$(RM_) jidctint.c
