#    Copyright (C) 1990, 1995, 1996, 1997 Aladdin Enterprises.  All rights reserved.
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
# Since Unix make doesn't have an 'include' facility, we concatenate
# the various parts of the makefile together by brute force (in tar_cat).

# Define the name of this makefile.
UNIXTAIL_MAK=unixtail.mak

# The following prevents GNU make from constructing argument lists that
# include all environment variables, which can easily be longer than
# brain-damaged system V allows.

.NOEXPORT:

# -------------------------------- Library -------------------------------- #

## The Unix platforms

# We have to include a test for the existence of sys/time.h,
# because some System V platforms don't have it.

# Define pipes as a separable feature.

pipe_=gdevpipe.$(OBJ)
pipe.dev: $(UNIXTAIL_MAK) $(ECHOGS_XE) $(pipe_)
	$(SETMOD) pipe $(pipe_)
	$(ADDMOD) pipe -iodev pipe

gdevpipe.$(OBJ): gdevpipe.c $(AK) $(errno__h) $(stdio__h) $(string__h) \
  $(gserror_h) $(gsmemory_h) $(gstypes_h) $(gxiodev_h) $(stream_h)

# Unix platforms other than System V, and also System V Release 4
# (SVR4) platforms.
unix__=gp_nofb.$(OBJ) gp_unix.$(OBJ) gp_unifs.$(OBJ) gp_unifn.$(OBJ)
unix_.dev: $(unix__)
	$(SETMOD) unix_ $(unix__)

gp_unix.$(OBJ): gp_unix.c $(AK) $(string__h) $(gx_h) $(gsexit_h) $(gp_h) \
  $(time__h)

# System V platforms other than SVR4, which lack some system calls,
# but have pipes.
sysv__=gp_nofb.$(OBJ) gp_unix.$(OBJ) gp_unifs.$(OBJ) gp_unifn.$(OBJ) gp_sysv.$(OBJ)
sysv_.dev: $(sysv__)
	$(SETMOD) sysv_ $(sysv__)

gp_sysv.$(OBJ): gp_sysv.c $(stdio__h) $(time__h) $(AK)

# -------------------------- Auxiliary programs --------------------------- #

$(ANSI2KNR_XE): ansi2knr.c
	$(CCA2K) $(O)$(ANSI2KNR_XE) ansi2knr.c

$(ECHOGS_XE): echogs.c $(AK)
	$(CCAUX) $(O)$(ECHOGS_XE) echogs.c

# On the RS/6000 (at least), compiling genarch.c with gcc with -O
# produces a buggy executable.
$(GENARCH_XE): genarch.c $(AK) $(stdpre_h)
	$(CCAUX) $(O)$(GENARCH_XE) genarch.c

$(GENCONF_XE): genconf.c $(AK) $(stdpre_h)
	$(CCAUX) $(O)$(GENCONF_XE) genconf.c

$(GENINIT_XE): geninit.c $(AK) $(stdio__h) $(string__h)
	$(CCAUX) $(O)$(GENINIT_XE) geninit.c

# Query the environment to construct gconfig_.h.
# The "else true; is required because Ultrix's implementation of sh -e
# terminates execution of a command if any error occurs, even if the command
# traps the error with ||.
INCLUDE=/usr/include
gconfig_.h: $(UNIXTAIL_MAK) $(ECHOGS_XE)
	./echogs -w gconfig_.h -x 2f2a -s This file was generated automatically. -s -x 2a2f
	if ( test -f $(INCLUDE)/dirent.h ); then ./echogs -a gconfig_.h -x 23 define HAVE_DIRENT_H; else true; fi
	if ( test -f $(INCLUDE)/ndir.h ); then ./echogs -a gconfig_.h -x 23 define HAVE_NDIR_H; else true; fi
	if ( test -f $(INCLUDE)/sys/dir.h ); then ./echogs -a gconfig_.h -x 23 define HAVE_SYS_DIR_H; else true; fi
	if ( test -f $(INCLUDE)/sys/ndir.h ); then ./echogs -a gconfig_.h -x 23 define HAVE_SYS_NDIR_H; else true; fi
	if ( test -f $(INCLUDE)/sys/time.h ); then ./echogs -a gconfig_.h -x 23 define HAVE_SYS_TIME_H; else true; fi
	if ( test -f $(INCLUDE)/sys/times.h ); then ./echogs -a gconfig_.h -x 23 define HAVE_SYS_TIMES_H; else true; fi

# ----------------------------- Main program ------------------------------ #

### Library files and archive

LIB_ARCHIVE_ALL=$(LIB_ALL) $(DEVS_ALL)\
 gsnogc.$(OBJ) gconfig.$(OBJ) gscdefs.$(OBJ)

# Build an archive for the library only.
# This is not used in a standard build.
GSLIB_A=$(GS)lib.a
$(GSLIB_A): $(LIB_ARCHIVE_ALL)
	rm -f $(GSLIB_A)
	$(AR) $(ARFLAGS) $(GSLIB_A) $(LIB_ARCHIVE_ALL)
	$(RANLIB) $(GSLIB_A)

### Interpreter main program

INT_ARCHIVE_ALL=imainarg.$(OBJ) imain.$(OBJ) $(INT_ALL) $(DEVS_ALL)\
 gconfig.$(OBJ) gscdefs.$(OBJ)
XE_ALL=gs.$(OBJ) $(INT_ARCHIVE_ALL)

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
$(GS_XE): ld.tr echogs $(XE_ALL)
	./echogs -w ldt.tr -n - $(CCLD) $(LDFLAGS) $(XLIBDIRS) -o $(GS_XE)
	./echogs -a ldt.tr -n -s gs.$(OBJ) -s
	cat ld.tr >>ldt.tr
	./echogs -a ldt.tr -s - $(EXTRALIBS) -lm
	LD_RUN_PATH=$(XLIBDIR); export LD_RUN_PATH; \
	XCFLAGS= XINCLUDE= XLDFLAGS= XLIBDIRS= XLIBS= \
	FEATURE_DEVS= DEVICE_DEVS= DEVICE_DEVS1= DEVICE_DEVS2= DEVICE_DEVS3= \
	DEVICE_DEVS4= DEVICE_DEVS5= DEVICE_DEVS6= DEVICE_DEVS7= DEVICE_DEVS8= \
	DEVICE_DEVS9= DEVICE_DEVS10= DEVICE_DEVS11= DEVICE_DEVS12= \
	DEVICE_DEVS13= DEVICE_DEVS14= DEVICE_DEVS15= \
	$(SH) <ldt.tr
