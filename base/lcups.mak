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
# $Id: lcups.mak 11073 2010-04-15 08:30:48Z chrisl $
# makefile for libcups as part of the monolithic gs build.
#
# Users of this makefile must define the following:
#	LCUPSSRCDIR    - the source directory
#	LCUPSGENDIR    - the generated intermediate file directory
#	LCUPSOBJDIR    - the object file directory
#	SHARE_LCUPS - 0 to compile in libcups, 1 to link a shared library
#	LCUPS_LIBS  - if SHARE_CUPS=1, the link options for the shared library

# (Rename directories.)
LIBCUPSSRC=$(LCUPSSRCDIR)$(D)libs$(D)cups$(D)
LIBCUPSGEN=$(LCUPSGENDIR)$(D)
LIBCUPSOBJ=$(LCUPSOBJDIR)$(D)
LCUPSO_=$(O_)$(LIBCUPSOBJ)

# NB: we can't use the normal $(CC_) here because msvccmd.mak
# adds /Za which conflicts with the cups source.
LCUPS_CC=$(CUPS_CC) $(I_)$(LIBCUPSSRC) $(I_)$(LIBCUPSGEN)$(D)cups $(I_)$(LCUPSSRCDIR)$(D)libs 

# Define the name of this makefile.
LCUPS_MAK=$(GLSRC)lcups.mak $(TOP_MAKEFILES)

LIBCUPS_OBJS =\
	$(LIBCUPSOBJ)adminutil.$(OBJ) \
	$(LIBCUPSOBJ)array.$(OBJ) \
	$(LIBCUPSOBJ)attr.$(OBJ) \
	$(LIBCUPSOBJ)auth.$(OBJ) \
	$(LIBCUPSOBJ)backchannel.$(OBJ) \
	$(LIBCUPSOBJ)backend.$(OBJ) \
	$(LIBCUPSOBJ)conflicts.$(OBJ) \
	$(LIBCUPSOBJ)custom.$(OBJ) \
	$(LIBCUPSOBJ)debug.$(OBJ) \
	$(LIBCUPSOBJ)dest.$(OBJ) \
	$(LIBCUPSOBJ)dir.$(OBJ) \
	$(LIBCUPSOBJ)emit.$(OBJ) \
	$(LIBCUPSOBJ)encode.$(OBJ) \
	$(LIBCUPSOBJ)file.$(OBJ) \
	$(LIBCUPSOBJ)getdevices.$(OBJ) \
	$(LIBCUPSOBJ)getputfile.$(OBJ) \
	$(LIBCUPSOBJ)globals.$(OBJ) \
	$(LIBCUPSOBJ)http.$(OBJ) \
	$(LIBCUPSOBJ)http-addr.$(OBJ) \
	$(LIBCUPSOBJ)http-addrlist.$(OBJ) \
	$(LIBCUPSOBJ)http-support.$(OBJ) \
	$(LIBCUPSOBJ)ipp.$(OBJ) \
	$(LIBCUPSOBJ)ipp-support.$(OBJ) \
	$(LIBCUPSOBJ)langprintf.$(OBJ) \
	$(LIBCUPSOBJ)language.$(OBJ) \
	$(LIBCUPSOBJ)localize.$(OBJ) \
	$(LIBCUPSOBJ)mark.$(OBJ) \
	$(LIBCUPSOBJ)md5passwd.$(OBJ) \
	$(LIBCUPSOBJ)notify.$(OBJ) \
	$(LIBCUPSOBJ)options.$(OBJ) \
	$(LIBCUPSOBJ)page.$(OBJ) \
	$(LIBCUPSOBJ)ppd.$(OBJ) \
	$(LIBCUPSOBJ)pwg-media.$(OBJ) \
	$(LIBCUPSOBJ)request.$(OBJ) \
	$(LIBCUPSOBJ)snmp.$(OBJ) \
	$(LIBCUPSOBJ)string.$(OBJ) \
	$(LIBCUPSOBJ)tempfile.$(OBJ) \
	$(LIBCUPSOBJ)transcode.$(OBJ) \
	$(LIBCUPSOBJ)cups_util.$(OBJ) \
	$(LIBCUPSOBJ)cups_md5.$(OBJ) \
	$(LIBCUPSOBJ)cups_snpf.$(OBJ) \
	$(LIBCUPSOBJ)usersys.$(OBJ) \
	$(LIBCUPSOBJ)ppd-cache.$(OBJ) \
	$(LIBCUPSOBJ)thread.$(OBJ) 
#	$(LIBCUPSOBJ)sidechannel.$(OBJ) \
#	$(LIBCUPSOBJ)getifaddrs.$(OBJ) \
#	$(LIBCUPSOBJ)pwg-ppd.$(OBJ) \
#	$(LIBCUPSOBJ)pwg-file.$(OBJ) \

LIBCUPS_DEPS	=	\
		$(LIBCUPSSRC)adminutil.h \
		$(LIBCUPSSRC)array.h \
		$(LIBCUPSSRC)backend.h \
		$(LIBCUPSSRC)cups.h \
		$(LIBCUPSSRC)dir.h \
		$(LIBCUPSSRC)file.h \
		$(LIBCUPSSRC)http.h \
		$(LIBCUPSSRC)ipp.h \
		$(LIBCUPSSRC)language.h \
		$(LIBCUPSSRC)ppd.h \
		$(LIBCUPSSRC)raster.h \
		$(LIBCUPSSRC)sidechannel.h \
		$(LIBCUPSSRC)transcode.h \
		$(LIBCUPSSRC)versioning.h \
                $(LCUPS_MAK) \
		$(MAKEDIRS)

libcups.clean : libcups.config-clean libcups.clean-not-config-clean

libcups.clean-not-config-clean :
	$(EXP)$(ECHOGS_XE) $(LCUPSSRCDIR) $(LCUPSOBJDIR)
	$(RM_) $(LIBCUPSOBJ)*.$(OBJ)

libcups.config-clean :
	$(RMN_) $(LIBCUPSGEN)$(D)lcups*.dev
	$(RMN_) $(LIBCUPSGEN)$(D)cups$(D)*.h
	$(RMN_) $(LIBCUPSGEN)$(D)cups_md5.c
	$(RMN_) $(LIBCUPSGEN)$(D)cups_snpf.c
	$(RMN_) $(LIBCUPSGEN)$(D)cups_util.c

# instantiate the requested build option (shared or compiled in)
$(LIBCUPSGEN)lcups.dev : $(LIBCUPSGEN)lcups_$(SHARE_LCUPS).dev \
 $(LCUPS_MAK) $(MAKEDIRS)
	$(CP_) $(LIBCUPSGEN)lcups_$(SHARE_LCUPS).dev $(LIBCUPSGEN)lcups.dev

# Define the shared version.
$(LIBCUPSGEN)lcups_1.dev : $(ECHOGS_XE) $(LCUPS_MAK) \
		$(MAKEDIRS)
	$(SETMOD) $(LIBCUPSGEN)lcups_1 -link $(LCUPS_LIBS)
	$(ADDMOD) $(DD)lcups_1 -libpath $(CUPSLIBDIRS)
	$(ADDMOD) $(DD)lcups_1 -lib $(CUPSLIBS)

# Define the non-shared version.
$(LIBCUPSGEN)lcups_0.dev : $(ECHOGS_XE) $(LIBCUPS_OBJS) $(LIBCUPS_DEPS)
	$(SETMOD) $(LIBCUPSGEN)lcups_0 $(LIBCUPS_OBJS)

# explicit rules for building the source files
# for simplicity we have every source file depend on all headers

$(LIBCUPSGEN)$(D)cups$(D)config.h : $(LCUPSSRCDIR)$(D)libs$(D)config$(LCUPSBUILDTYPE).h $(LIBCUPS_DEPS)
	$(CP_) $(LCUPSSRCDIR)$(D)libs$(D)config$(LCUPSBUILDTYPE).h $(LIBCUPSGEN)$(D)cups$(D)config.h

$(LIBCUPSOBJ)adminutil.$(OBJ) : $(LIBCUPSSRC)adminutil.c $(LIBCUPSGEN)$(D)cups$(D)config.h $(LIBCUPS_DEPS) 
	$(LCUPS_CC) $(LCUPSO_)adminutil.$(OBJ) $(C_) $(LIBCUPSSRC)adminutil.c
	
$(LIBCUPSOBJ)array.$(OBJ) : $(LIBCUPSSRC)array.c $(LIBCUPS_DEPS)
	$(LCUPS_CC) $(LCUPSO_)array.$(OBJ) $(C_) $(LIBCUPSSRC)array.c
	
$(LIBCUPSOBJ)attr.$(OBJ) : $(LIBCUPSSRC)attr.c $(LIBCUPS_DEPS)
	$(LCUPS_CC) $(LCUPSO_)attr.$(OBJ) $(C_) $(LIBCUPSSRC)attr.c

$(LIBCUPSOBJ)auth.$(OBJ) : $(LIBCUPSSRC)auth.c $(LIBCUPS_DEPS)
	$(LCUPS_CC) $(LCUPSO_)auth.$(OBJ) $(C_) $(LIBCUPSSRC)auth.c

$(LIBCUPSOBJ)backchannel.$(OBJ) : $(LIBCUPSSRC)backchannel.c $(LIBCUPS_DEPS)
	$(LCUPS_CC) $(LCUPSO_)backchannel.$(OBJ) $(C_) $(LIBCUPSSRC)backchannel.c

$(LIBCUPSOBJ)backend.$(OBJ) : $(LIBCUPSSRC)backend.c $(LIBCUPS_DEPS)
	$(LCUPS_CC) $(LCUPSO_)backend.$(OBJ) $(C_) $(LIBCUPSSRC)backend.c

$(LIBCUPSOBJ)conflicts.$(OBJ) : $(LIBCUPSSRC)conflicts.c $(LIBCUPS_DEPS)
	$(LCUPS_CC) $(LCUPSO_)conflicts.$(OBJ) $(C_) $(LIBCUPSSRC)conflicts.c

$(LIBCUPSOBJ)custom.$(OBJ) : $(LIBCUPSSRC)custom.c $(LIBCUPS_DEPS)
	$(LCUPS_CC) $(LCUPSO_)custom.$(OBJ) $(C_) $(LIBCUPSSRC)custom.c

$(LIBCUPSOBJ)debug.$(OBJ) : $(LIBCUPSSRC)debug.c $(LIBCUPS_DEPS)
	$(LCUPS_CC) $(LCUPSO_)debug.$(OBJ) $(C_) $(LIBCUPSSRC)debug.c

$(LIBCUPSOBJ)dest.$(OBJ) : $(LIBCUPSSRC)dest.c $(LIBCUPS_DEPS)
	$(LCUPS_CC) $(LCUPSO_)dest.$(OBJ) $(C_) $(LIBCUPSSRC)dest.c

$(LIBCUPSOBJ)dir.$(OBJ) : $(LIBCUPSSRC)dir.c $(LIBCUPS_DEPS)
	$(LCUPS_CC) $(LCUPSO_)dir.$(OBJ) $(C_) $(LIBCUPSSRC)dir.c

$(LIBCUPSOBJ)emit.$(OBJ) : $(LIBCUPSSRC)emit.c $(LIBCUPS_DEPS)
	$(LCUPS_CC) $(LCUPSO_)emit.$(OBJ) $(C_) $(LIBCUPSSRC)emit.c

$(LIBCUPSOBJ)encode.$(OBJ) : $(LIBCUPSSRC)encode.c $(LIBCUPS_DEPS)
	$(LCUPS_CC) $(LCUPSO_)encode.$(OBJ) $(C_) $(LIBCUPSSRC)encode.c

$(LIBCUPSOBJ)file.$(OBJ) : $(LIBCUPSSRC)file.c $(LIBCUPS_DEPS)
	$(LCUPS_CC) $(LCUPSO_)file.$(OBJ) $(C_) $(LIBCUPSSRC)file.c

$(LIBCUPSOBJ)getdevices.$(OBJ) : $(LIBCUPSSRC)getdevices.c $(LIBCUPS_DEPS)
	$(LCUPS_CC) $(LCUPSO_)getdevices.$(OBJ) $(C_) $(LIBCUPSSRC)getdevices.c

$(LIBCUPSOBJ)getifaddrs.$(OBJ) : $(LIBCUPSSRC)getifaddrs.c $(LIBCUPS_DEPS)
	$(LCUPS_CC) $(LCUPSO_)getifaddrs.$(OBJ) $(C_) $(LIBCUPSSRC)getifaddrs.c

$(LIBCUPSOBJ)getputfile.$(OBJ) : $(LIBCUPSSRC)getputfile.c $(LIBCUPS_DEPS)
	$(LCUPS_CC) $(LCUPSO_)getputfile.$(OBJ) $(C_) $(LIBCUPSSRC)getputfile.c

$(LIBCUPSOBJ)globals.$(OBJ) : $(LIBCUPSSRC)globals.c $(LIBCUPS_DEPS)
	$(LCUPS_CC) $(LCUPSO_)globals.$(OBJ) $(C_) $(LIBCUPSSRC)globals.c

$(LIBCUPSOBJ)http.$(OBJ) : $(LIBCUPSSRC)http.c $(LIBCUPS_DEPS)
	$(LCUPS_CC) $(LCUPSO_)http.$(OBJ) $(C_) $(LIBCUPSSRC)http.c

$(LIBCUPSOBJ)http-addr.$(OBJ) : $(LIBCUPSSRC)http-addr.c $(LIBCUPS_DEPS)
	$(LCUPS_CC) $(LCUPSO_)http-addr.$(OBJ) $(C_) $(LIBCUPSSRC)http-addr.c

$(LIBCUPSOBJ)http-addrlist.$(OBJ) : $(LIBCUPSSRC)http-addrlist.c $(LIBCUPS_DEPS)
	$(LCUPS_CC) $(LCUPSO_)http-addrlist.$(OBJ) $(C_) $(LIBCUPSSRC)http-addrlist.c

$(LIBCUPSOBJ)http-support.$(OBJ) : $(LIBCUPSSRC)http-support.c $(LIBCUPS_DEPS)
	$(LCUPS_CC) $(LCUPSO_)http-support.$(OBJ) $(C_) $(LIBCUPSSRC)http-support.c

$(LIBCUPSOBJ)ipp.$(OBJ) : $(LIBCUPSSRC)ipp.c $(LIBCUPS_DEPS)
	$(LCUPS_CC) $(LCUPSO_)ipp.$(OBJ) $(C_) $(LIBCUPSSRC)ipp.c

$(LIBCUPSOBJ)ipp-support.$(OBJ) : $(LIBCUPSSRC)ipp-support.c $(LIBCUPS_DEPS)
	$(LCUPS_CC) $(LCUPSO_)ipp-support.$(OBJ) $(C_) $(LIBCUPSSRC)ipp-support.c

$(LIBCUPSOBJ)langprintf.$(OBJ) : $(LIBCUPSSRC)langprintf.c $(LIBCUPS_DEPS)
	$(LCUPS_CC) $(LCUPSO_)langprintf.$(OBJ) $(C_) $(LIBCUPSSRC)langprintf.c

$(LIBCUPSOBJ)language.$(OBJ) : $(LIBCUPSSRC)language.c $(LIBCUPS_DEPS)
	$(LCUPS_CC) $(LCUPSO_)language.$(OBJ) $(C_) $(LIBCUPSSRC)language.c

$(LIBCUPSOBJ)localize.$(OBJ) : $(LIBCUPSSRC)localize.c $(LIBCUPS_DEPS)
	$(LCUPS_CC) $(LCUPSO_)localize.$(OBJ) $(C_) $(LIBCUPSSRC)localize.c

$(LIBCUPSOBJ)mark.$(OBJ) : $(LIBCUPSSRC)mark.c $(LIBCUPS_DEPS)
	$(LCUPS_CC) $(LCUPSO_)mark.$(OBJ) $(C_) $(LIBCUPSSRC)mark.c

$(LIBCUPSOBJ)cups_md5.$(OBJ) : $(LIBCUPSSRC)md5.c $(LIBCUPS_DEPS)
	$(CP_) $(LIBCUPSSRC)md5.c $(LIBCUPSGEN)cups_md5.c
	$(LCUPS_CC) $(LCUPSO_)cups_md5.$(OBJ) $(C_) $(LIBCUPSGEN)cups_md5.c

$(LIBCUPSOBJ)md5passwd.$(OBJ) : $(LIBCUPSSRC)md5passwd.c $(LIBCUPS_DEPS)
	$(LCUPS_CC) $(LCUPSO_)md5passwd.$(OBJ) $(C_) $(LIBCUPSSRC)md5passwd.c

$(LIBCUPSOBJ)notify.$(OBJ) : $(LIBCUPSSRC)notify.c $(LIBCUPS_DEPS)
	$(LCUPS_CC) $(LCUPSO_)notify.$(OBJ) $(C_) $(LIBCUPSSRC)notify.c

$(LIBCUPSOBJ)options.$(OBJ) : $(LIBCUPSSRC)options.c $(LIBCUPS_DEPS)
	$(LCUPS_CC) $(LCUPSO_)options.$(OBJ) $(C_) $(LIBCUPSSRC)options.c

$(LIBCUPSOBJ)page.$(OBJ) : $(LIBCUPSSRC)page.c $(LIBCUPS_DEPS)
	$(LCUPS_CC) $(LCUPSO_)page.$(OBJ) $(C_) $(LIBCUPSSRC)page.c

$(LIBCUPSOBJ)ppd.$(OBJ) : $(LIBCUPSSRC)ppd.c $(LIBCUPS_DEPS)
	$(LCUPS_CC) $(LCUPSO_)ppd.$(OBJ) $(C_) $(LIBCUPSSRC)ppd.c

$(LIBCUPSOBJ)pwg-file.$(OBJ) : $(LIBCUPSSRC)pwg-file.c $(LIBCUPS_DEPS)
	$(LCUPS_CC) $(LCUPSO_)pwg-file.$(OBJ) $(C_) $(LIBCUPSSRC)pwg-file.c

$(LIBCUPSOBJ)pwg-media.$(OBJ) : $(LIBCUPSSRC)pwg-media.c $(LIBCUPS_DEPS)
	$(LCUPS_CC) $(LCUPSO_)pwg-media.$(OBJ) $(C_) $(LIBCUPSSRC)pwg-media.c

$(LIBCUPSOBJ)pwg-ppd.$(OBJ) : $(LIBCUPSSRC)pwg-ppd.c $(LIBCUPS_DEPS)
	$(LCUPS_CC) $(LCUPSO_)pwg-ppd.$(OBJ) $(C_) $(LIBCUPSSRC)pwg-ppd.c

$(LIBCUPSOBJ)request.$(OBJ) : $(LIBCUPSSRC)request.c $(LIBCUPS_DEPS)
	$(LCUPS_CC) $(LCUPSO_)request.$(OBJ) $(C_) $(LIBCUPSSRC)request.c

$(LIBCUPSOBJ)sidechannel.$(OBJ) : $(LIBCUPSSRC)sidechannel.c $(LIBCUPS_DEPS)
	$(LCUPS_CC) $(LCUPSO_)sidechannel.$(OBJ) $(C_) $(LIBCUPSSRC)sidechannel.c

$(LIBCUPSOBJ)snmp.$(OBJ) : $(LIBCUPSSRC)snmp.c $(LIBCUPS_DEPS)
	$(LCUPS_CC) $(LCUPSO_)snmp.$(OBJ) $(C_) $(LIBCUPSSRC)snmp.c

$(LIBCUPSOBJ)cups_snpf.$(OBJ) : $(LIBCUPSSRC)snprintf.c $(LIBCUPS_DEPS)
	$(CP_) $(LIBCUPSSRC)snprintf.c $(LIBCUPSGEN)cups_snpf.c
	$(LCUPS_CC) $(LCUPSO_)cups_snpf.$(OBJ) $(C_) $(LIBCUPSGEN)cups_snpf.c

$(LIBCUPSOBJ)string.$(OBJ) : $(LIBCUPSSRC)string.c $(LIBCUPS_DEPS)
	$(LCUPS_CC) $(LCUPSO_)string.$(OBJ) $(C_) $(LIBCUPSSRC)string.c

$(LIBCUPSOBJ)tempfile.$(OBJ) : $(LIBCUPSSRC)tempfile.c $(LIBCUPS_DEPS)
	$(LCUPS_CC) $(LCUPSO_)tempfile.$(OBJ) $(C_) $(LIBCUPSSRC)tempfile.c

$(LIBCUPSOBJ)transcode.$(OBJ) : $(LIBCUPSSRC)transcode.c $(LIBCUPS_DEPS)
	$(LCUPS_CC) $(LCUPSO_)transcode.$(OBJ) $(C_) $(LIBCUPSSRC)transcode.c

$(LIBCUPSOBJ)usersys.$(OBJ) : $(LIBCUPSSRC)usersys.c $(LIBCUPS_DEPS)
	$(LCUPS_CC) $(LCUPSO_)usersys.$(OBJ) $(C_) $(LIBCUPSSRC)usersys.c

$(LIBCUPSOBJ)ppd-cache.$(OBJ) : $(LIBCUPSSRC)ppd-cache.c $(LIBCUPS_DEPS)
	$(LCUPS_CC) $(LCUPSO_)ppd-cache.$(OBJ) $(C_) $(LIBCUPSSRC)ppd-cache.c

$(LIBCUPSOBJ)thread.$(OBJ) : $(LIBCUPSSRC)thread.c $(LIBCUPS_DEPS)
	$(LCUPS_CC) $(LCUPSO_)thread.$(OBJ) $(C_) $(LIBCUPSSRC)thread.c

$(LIBCUPSOBJ)cups_util.$(OBJ) : $(LIBCUPSSRC)util.c $(LIBCUPS_DEPS)
	$(CP_) $(LIBCUPSSRC)util.c $(LIBCUPSGEN)cups_util.c
	$(LCUPS_CC) $(LCUPSO_)cups_util.$(OBJ) $(C_) $(LIBCUPSGEN)cups_util.c
