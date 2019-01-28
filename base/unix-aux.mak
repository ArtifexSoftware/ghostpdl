# Copyright (C) 2001-2019 Artifex Software, Inc.
# All Rights Reserved.
#
# This software is provided AS-IS with no warranty, either express or
# implied.
#
# This software is distributed under license and may not be copied,
# modified or distributed except as expressly authorized under the terms
# of the license contained in the file LICENSE in this distribution.
#
# Refer to licensing information at http://www.artifex.com or contact
# Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
# CA 94945, U.S.A., +1(415)492-9861, for further information.
#
# Partial makefile common to all Unix configurations.
# This makefile contains the build rules for the auxiliary programs such as
# echogs, and the 'platform' modules.

# Define the name of this makefile.
UNIX_AUX_MAK=$(GLSRC)unix-aux.mak $(TOP_MAKEFILES)

# -------------------------------- Library -------------------------------- #

## The Unix platforms

# We have to include a test for the existence of sys/time.h,
# because some System V platforms don't have it.

# Unix platforms other than System V, and also System V Release 4
# (SVR4) platforms.
unix__=$(GLOBJ)gp_getnv.$(OBJ) $(GLOBJ)gp_upapr.$(OBJ) $(GLOBJ)gp_unix.$(OBJ)\
       $(GLOBJ)gp_unifs.$(OBJ) $(GLOBJ)gp_unifn.$(OBJ) $(GLOBJ)gp_stdia.$(OBJ)\
       $(GLOBJ)gp_nxpsprn.$(OBJ)

$(GLGEN)unix_.dev: $(unix__) $(GLD)nosync.dev $(GLD)smd5.dev $(UNIX_AUX_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLGEN)unix_ $(unix__) -include $(GLD)nosync
	$(ADDMOD) $(GLGEN)unix_ -include $(GLD)smd5

$(GLOBJ)gp_unix.$(OBJ): $(GLSRC)gp_unix.c $(AK)\
 $(pipe__h) $(string__h) $(time__h) $(gx_h) $(gsexit_h) $(gp_h) $(UNIX_AUX_MAK) $(MAKEDIRS)
	$(GLCC) $(FONTCONFIG_CFLAGS) $(GLO_)gp_unix.$(OBJ) $(C_) $(GLSRC)gp_unix.c

$(AUX)gp_unix.$(OBJ): $(GLSRC)gp_unix.c $(AK)\
 $(pipe__h) $(string__h) $(time__h)\
 $(gx_h) $(gsexit_h) $(gp_h) $(UNIX_AUX_MAK) $(MAKEDIRS)
	$(GLCCAUX) $(AUXO_)gp_unix.$(OBJ) $(C_) $(GLSRC)gp_unix.c

# assume all Unix platforms support unbuffered read
$(GLOBJ)gp_stdia.$(OBJ): $(GLSRC)gp_stdia.c $(AK)\
  $(stdio__h) $(time__h) $(unistd__h) $(gx_h) $(gp_h) $(UNIX_AUX_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gp_stdia.$(OBJ) $(C_) $(GLSRC)gp_stdia.c

$(AUX)gp_stdia.$(OBJ): $(GLSRC)gp_stdia.c $(AK)\
  $(stdio__h) $(time__h) $(unistd__h) $(gx_h) $(gp_h) $(UNIX_AUX_MAK) $(MAKEDIRS)
	$(GLCCAUX) $(AUXO_)gp_stdia.$(OBJ) $(C_) $(GLSRC)gp_stdia.c

# -------------------------- Auxiliary programs --------------------------- #

$(ECHOGS_XE): $(GLSRC)echogs.c $(AK) $(stdpre_h) $(UNIX_AUX_MAK) $(MAKEDIRS)
	$(CCAUX_) $(I_)$(GLSRCDIR)$(_I) $(O_)$(ECHOGS_XE) $(GLSRC)echogs.c $(AUXEXTRALIBS)

$(PACKPS_XE): $(GLSRC)pack_ps.c $(stdpre_h) $(UNIX_AUX_MAK) $(MAKEDIRS)
	$(CCAUX_) $(I_)$(GLSRCDIR)$(_I) $(O_)$(PACKPS_XE) $(GLSRC)pack_ps.c $(AUXEXTRALIBS)

# On the RS/6000 (at least), compiling genarch.c with gcc with -O
# produces a buggy executable.
$(GENARCH_XE): $(GLSRC)genarch.c $(AK) $(GENARCH_DEPS) $(UNIX_AUX_MAK) $(MAKEDIRS)
	$(CCAUX_) $(I_)$(GLSRCDIR)$(_I) $(O_)$(GENARCH_XE) $(GLSRC)genarch.c $(AUXEXTRALIBS)

$(GENCONF_XE): $(GLSRC)genconf.c $(AK) $(GENCONF_DEPS) $(UNIX_AUX_MAK) $(MAKEDIRS)
	$(CCAUX_) $(I_)$(GLSRCDIR)$(_I) $(O_)$(GENCONF_XE) $(GLSRC)genconf.c $(AUXEXTRALIBS)

$(GENDEV_XE): $(GLSRC)gendev.c $(AK) $(GENDEV_DEPS) $(UNIX_AUX_MAK) $(MAKEDIRS)
	$(CCAUX_) $(I_)$(GLSRCDIR)$(_I) $(O_)$(GENDEV_XE) $(GLSRC)gendev.c $(AUXEXTRALIBS)

$(GENHT_XE): $(GLSRC)genht.c $(AK) $(GENHT_DEPS) $(UNIX_AUX_MAK) $(MAKEDIRS)
	$(CCAUX_) $(GENHT_CFLAGS) $(O_)$(GENHT_XE) $(GLSRC)genht.c $(AUXEXTRALIBS)

# To get GS to use the system zlib, you remove/hide the gs/zlib directory
# which means that the mkromfs build can't find the zlib source it needs.
# So it's split into two targets, one using the zlib source directly.....
MKROMFS_OBJS_0=$(MKROMFS_ZLIB_OBJS) $(AUX)gpmisc.$(OBJ) $(AUX)gp_getnv.$(OBJ) \
 $(AUX)gscdefs.$(OBJ) $(AUX)gp_unix.$(OBJ) $(AUX)gp_unifs.$(OBJ) $(AUX)gp_unifn.$(OBJ) \
 $(AUX)gp_stdia.$(OBJ) $(AUX)gsutil.$(OBJ) $(AUX)memento.$(OBJ)

$(MKROMFS_XE)_0: $(GLSRC)mkromfs.c $(MKROMFS_COMMON_DEPS) $(MKROMFS_OBJS_0) $(UNIX_AUX_MAK) $(MAKEDIRS)
	$(CCAUX_) $(GENOPTAUX) $(I_)$(GLSRCDIR)$(_I) $(I_)$(GLOBJ)$(_I) $(I_)$(ZSRCDIR)$(_I) $(GLSRC)mkromfs.c $(O_)$(MKROMFS_XE)_0 $(MKROMFS_OBJS_0) $(AUXEXTRALIBS)

# .... and one using the zlib library linked via the command line
MKROMFS_OBJS_1=$(AUX)gscdefs.$(OBJ) \
 $(AUX)gpmisc.$(OBJ) $(AUX)gp_getnv.$(OBJ) \
 $(AUX)gp_unix.$(OBJ) $(AUX)gp_unifs.$(OBJ) $(AUX)gp_unifn.$(OBJ) \
 $(AUX)gp_stdia.$(OBJ) $(AUX)gsutil.$(OBJ)

$(MKROMFS_XE)_1: $(GLSRC)mkromfs.c $(MKROMFS_COMMON_DEPS) $(MKROMFS_OBJS_1) $(UNIX_AUX_MAK) $(MAKEDIRS)
	$(CCAUX_) $(GENOPTAUX) $(I_)$(GLSRCDIR)$(_I) $(I_)$(GLOBJ)$(_I) $(I_)$(ZSRCDIR)$(_I) $(GLSRC)mkromfs.c $(O_)$(MKROMFS_XE)_1 $(MKROMFS_OBJS_1) $(AUXEXTRALIBS)

$(MKROMFS_XE): $(MKROMFS_XE)_$(SHARE_ZLIB) $(UNIX_AUX_MAK) $(MAKEDIRS)
	$(CP_) $(MKROMFS_XE)_$(SHARE_ZLIB) $(MKROMFS_XE)

# Query the environment to construct gconfig_.h.
# These are all defined conditionally (except the JasPER one), so that
# they can be overridden by settings from the configure script.
# The "empty" $(ECHOGS_XE) lines just append a white space line to the
# header file.
INCLUDE=/usr/include
$(gconfig__h): $(UNIX_AUX_MAK) $(ECHOGS_XE) $(UNIX_AUX_MAK) $(MAKEDIRS)
	$(ECHOGS_XE) -w $(gconfig__h) -x 2f2a -s This file was generated automatically by unix-aux.mak. -s -x 2a2f
	$(ECHOGS_XE) -a $(gconfig__h)

	$(ECHOGS_XE) -a $(gconfig__h) -x 23 ifndef HAVE_DIRENT_H
	if ( test -f $(INCLUDE)/dirent.h ); then $(ECHOGS_XE) -a $(gconfig__h) -x 23 define HAVE_DIRENT_H 1; \
              else $(ECHOGS_XE) -a $(gconfig__h) -x 23 define HAVE_DIRENT_H 0; fi
	$(ECHOGS_XE) -a $(gconfig__h) -x 23 endif
	$(ECHOGS_XE) -a $(gconfig__h)

	$(ECHOGS_XE) -a $(gconfig__h) -x 23 ifndef HAVE_NDIR_H
	if ( test -f $(INCLUDE)/ndir.h ); then $(ECHOGS_XE) -a $(gconfig__h) -x 23 define HAVE_NDIR_H 1; \
              else $(ECHOGS_XE) -a $(gconfig__h) -x 23 define HAVE_NDIR_H 0; fi
	$(ECHOGS_XE) -a $(gconfig__h) -x 23 endif
	$(ECHOGS_XE) -a $(gconfig__h)

	$(ECHOGS_XE) -a $(gconfig__h) -x 23 ifndef HAVE_SYS_DIR_H
	if ( test -f $(INCLUDE)/sys/dir.h ); then $(ECHOGS_XE) -a $(gconfig__h) -x 23 define HAVE_SYS_DIR_H 1; \
              else $(ECHOGS_XE) -a $(gconfig__h) -x 23 define HAVE_SYS_DIR_H 0; fi
	$(ECHOGS_XE) -a $(gconfig__h) -x 23 endif
	$(ECHOGS_XE) -a $(gconfig__h)

	$(ECHOGS_XE) -a $(gconfig__h) -x 23 ifndef HAVE_SYS_NDIR_H
	if ( test -f $(INCLUDE)/sys/ndir.h ); then $(ECHOGS_XE) -a $(gconfig__h) -x 23 define HAVE_SYS_NDIR_H 1; \
              else $(ECHOGS_XE) -a $(gconfig__h) -x 23 define HAVE_SYS_NDIR_H 0; fi
	$(ECHOGS_XE) -a $(gconfig__h) -x 23 endif
	$(ECHOGS_XE) -a $(gconfig__h)

	$(ECHOGS_XE) -a $(gconfig__h) -x 23 ifndef HAVE_SYS_TIME_H
	if ( test -f $(INCLUDE)/sys/time.h ); then $(ECHOGS_XE) -a $(gconfig__h) -x 23 define HAVE_SYS_TIME_H 1; \
              else $(ECHOGS_XE) -a $(gconfig__h) -x 23 define HAVE_SYS_TIME_H 0; fi
	$(ECHOGS_XE) -a $(gconfig__h) -x 23 endif
	$(ECHOGS_XE) -a $(gconfig__h)

	$(ECHOGS_XE) -a $(gconfig__h) -x 23 ifndef HAVE_SYS_TIMES_H
	if ( test -f $(INCLUDE)/sys/times.h ); then $(ECHOGS_XE) -a $(gconfig__h) -x 23 define HAVE_SYS_TIMES_H 1; \
              else $(ECHOGS_XE) -a $(gconfig__h) -x 23 define HAVE_SYS_TIMES_H 0; fi
	$(ECHOGS_XE) -a $(gconfig__h) -x 23 endif
	$(ECHOGS_XE) -a $(gconfig__h)

	if ( test -f $(JSRCDIR)/jmemsys.h); then true; else $(ECHOGS_XE) -a $(gconfig__h) -x 23 define DONT_HAVE_JMEMSYS_H; fi
