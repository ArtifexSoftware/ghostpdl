#    Copyright (C) 1990, 1995, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
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


# Partial makefile common to all Unix configurations.
# This is the last part of the makefile for Unix configurations.

# Define the name of this makefile.
UNIXTAIL_MAK=$(GLSRC)unixtail.mak

# The following prevents GNU make from constructing argument lists that
# include all environment variables, which can easily be longer than
# brain-damaged system V allows.

.NOEXPORT:

# -------------------------------- Library -------------------------------- #

## The Unix platforms

# We have to include a test for the existence of sys/time.h,
# because some System V platforms don't have it.

# Define pipes as a separable feature.

pipe_=$(GLOBJ)gdevpipe.$(OBJ)
pipe.dev: $(UNIXTAIL_MAK) $(ECHOGS_XE) $(pipe_)
	$(SETMOD) pipe $(pipe_)
	$(ADDMOD) pipe -iodev pipe

$(GLOBJ)gdevpipe.$(OBJ): $(GLSRC)gdevpipe.c $(AK) $(errno__h) $(pipe__h) $(stdio__h) $(string__h) \
  $(gserror_h) $(gsmemory_h) $(gstypes_h) $(gxiodev_h) $(stream_h)
	$(GLCC) $(GLO_)gdevpipe.$(OBJ) $(C_) $(GLSRC)gdevpipe.c

# Unix platforms other than System V, and also System V Release 4
# (SVR4) platforms.
unix__=$(GLOBJ)gp_getnv.$(OBJ) $(GLOBJ)gp_nofb.$(OBJ) $(GLOBJ)gp_unix.$(OBJ) $(GLOBJ)gp_unifs.$(OBJ) $(GLOBJ)gp_unifn.$(OBJ)
unix_.dev: $(unix__) nosync.dev
	$(SETMOD) unix_ $(unix__) -include nosync

$(GLOBJ)gp_unix.$(OBJ): $(GLSRC)gp_unix.c $(AK)\
 $(pipe__h) $(string__h) $(time__h)\
 $(gx_h) $(gsexit_h) $(gp_h)
	$(GLCC) $(GLO_)gp_unix.$(OBJ) $(C_) $(GLSRC)gp_unix.c

# System V platforms other than SVR4, which lack some system calls,
# but have pipes.
sysv__=$(GLOBJ)gp_getnv.$(OBJ) $(GLOBJ)gp_nofb.$(OBJ) $(GLOBJ)gp_unix.$(OBJ) $(GLOBJ)gp_unifs.$(OBJ) $(GLOBJ)gp_unifn.$(OBJ) $(GLOBJ)gp_sysv.$(OBJ)
sysv_.dev: $(sysv__) nosync.dev
	$(SETMOD) sysv_ $(sysv__) -include nosync

$(GLOBJ)gp_sysv.$(OBJ): $(GLSRC)gp_sysv.c $(stdio__h) $(time__h) $(AK)
	$(GLCC) $(GLO_)gp_sysv.$(OBJ) $(C_) $(GLSRC)gp_sysv.c

# -------------------------- Auxiliary programs --------------------------- #

$(GENINIT_XE): $(GLSRC)geninit.c $(AK) $(stdio__h) $(string__h)
	$(CCAUX) $(O_)$(GENINIT_XE) $(GLSRC)geninit.c

$(ANSI2KNR_XE): $(GLSRC)ansi2knr.c
	$(CCA2K) $(O_)$(ANSI2KNR_XE) $(GLSRC)ansi2knr.c

$(ECHOGS_XE): $(GLSRC)echogs.c $(AK)
	$(CCAUX) $(O_)$(ECHOGS_XE) $(GLSRC)echogs.c

# On the RS/6000 (at least), compiling genarch.c with gcc with -O
# produces a buggy executable.
$(GENARCH_XE): $(GLSRC)genarch.c $(AK) $(stdpre_h)
	$(CCAUX) $(O_)$(GENARCH_XE) $(GLSRC)genarch.c

$(GENCONF_XE): $(GLSRC)genconf.c $(AK) $(stdpre_h)
	$(CCAUX) $(O_)$(GENCONF_XE) $(GLSRC)genconf.c

$(GENDEV_XE): $(GLSRC)gendev.c $(AK) $(stdpre_h)
	$(CCAUX) $(O_)$(GENDEV_XE) $(GLSRC)gendev.c

# Query the environment to construct gconfig_.h.
# The "else true; is required because Ultrix's implementation of sh -e
# terminates execution of a command if any error occurs, even if the command
# traps the error with ||.
INCLUDE=/usr/include
$(gconfig__h): $(UNIXTAIL_MAK) $(ECHOGS_XE)
	$(ECHOGS_XE) -w $(gconfig__h) -x 2f2a -s This file was generated automatically. -s -x 2a2f
	if ( test -f $(INCLUDE)/dirent.h ); then $(ECHOGS_XE) -a $(gconfig__h) -x 23 define HAVE_DIRENT_H; else true; fi
	if ( test -f $(INCLUDE)/ndir.h ); then $(ECHOGS_XE) -a $(gconfig__h) -x 23 define HAVE_NDIR_H; else true; fi
	if ( test -f $(INCLUDE)/sys/dir.h ); then $(ECHOGS_XE) -a $(gconfig__h) -x 23 define HAVE_SYS_DIR_H; else true; fi
	if ( test -f $(INCLUDE)/sys/ndir.h ); then $(ECHOGS_XE) -a $(gconfig__h) -x 23 define HAVE_SYS_NDIR_H; else true; fi
	if ( test -f $(INCLUDE)/sys/time.h ); then $(ECHOGS_XE) -a $(gconfig__h) -x 23 define HAVE_SYS_TIME_H; else true; fi
	if ( test -f $(INCLUDE)/sys/times.h ); then $(ECHOGS_XE) -a $(gconfig__h) -x 23 define HAVE_SYS_TIMES_H; else true; fi

# ----------------------------- Main program ------------------------------ #

### Library files and archive

LIB_ARCHIVE_ALL=$(LIB_ALL) $(DEVS_ALL)\
 $(GLOBJ)gsnogc.$(OBJ) $(GLOBJ)gconfig.$(OBJ) $(GLOBJ)gscdefs.$(OBJ)

# Build an archive for the library only.
# This is not used in a standard build.
GSLIB_A=$(GS)lib.a
$(GSLIB_A): $(LIB_ARCHIVE_ALL)
	rm -f $(GSLIB_A)
	$(AR) $(ARFLAGS) $(GSLIB_A) $(LIB_ARCHIVE_ALL)
	$(RANLIB) $(GSLIB_A)

### Interpreter main program

INT_ARCHIVE_ALL=$(PSOBJ)imainarg.$(OBJ) $(PSOBJ)imain.$(OBJ) $(INT_ALL) $(DEVS_ALL)\
 $(GLOBJ)gconfig.$(OBJ) $(GLOBJ)gscdefs.$(OBJ)
XE_ALL=$(PSOBJ)gs.$(OBJ) $(INT_ARCHIVE_ALL)

# Build a library archive for the entire interpreter.
# This is not used in a standard build.
GS_A=$(GS).a
$(GS_A): $(INT_ARCHIVE_ALL)
	rm -f $(GS_A)
	$(AR) $(ARFLAGS) $(GS_A) $(INT_ARCHIVE_ALL)
	$(RANLIB) $(GS_A)

# Here is the final link step.  The stuff with LD_RUN_PATH is for SVR4
# systems with dynamic library loading; I believe it's harmless elsewhere.
# The resetting of the environment variables to empty strings is for SCO Unix,
# which has limited environment space.
ldt_tr=$(PSOBJ)ldt.tr
$(GS_XE): $(ld_tr) $(ECHOGS_XE) $(XE_ALL)
	$(ECHOGS_XE) -w $(ldt_tr) -n - $(CCLD) $(LDFLAGS) $(XLIBDIRS) -o $(GS_XE)
	$(ECHOGS_XE) -a $(ldt_tr) -n -s $(PSOBJ)gs.$(OBJ) -s
	cat $(ld_tr) >>$(ldt_tr)
	$(ECHOGS_XE) -a $(ldt_tr) -s - $(EXTRALIBS) -lm
	LD_RUN_PATH=$(XLIBDIR); export LD_RUN_PATH; \
	XCFLAGS= XINCLUDE= XLDFLAGS= XLIBDIRS= XLIBS= \
	FEATURE_DEVS= DEVICE_DEVS= DEVICE_DEVS1= DEVICE_DEVS2= DEVICE_DEVS3= \
	DEVICE_DEVS4= DEVICE_DEVS5= DEVICE_DEVS6= DEVICE_DEVS7= DEVICE_DEVS8= \
	DEVICE_DEVS9= DEVICE_DEVS10= DEVICE_DEVS11= DEVICE_DEVS12= \
	DEVICE_DEVS13= DEVICE_DEVS14= DEVICE_DEVS15= \
	$(SH) <$(ldt_tr)
