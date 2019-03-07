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
# Common tail section for Microsoft Visual C++ 4.x/5.x,
# Windows NT or Windows 95 platform.
# Created 1997-05-22 by L. Peter Deutsch from msvc4/5 makefiles.
# edited 1997-06-xx by JD to factor out interpreter-specific sections
# edited 2000-06-05 by lpd to handle empty MSINCDIR specially.

MSVCTAIL_MAK=$(GLSRC)msvctail.mak $(TOP_MAKEFILES)

# -------------------------- Auxiliary programs --------------------------- #

# This also creates the subdirectories since this (hopefully) will be the
# first need. Too bad nmake doesn't have .BEFORE symbolic target.
$(GLGENDIR)\ccf32.tr: $(MSVCTAIL_MAK)
	-if not exist $(PSOBJDIR) mkdir $(PSOBJDIR)
	-if not exist $(PSGENDIR) mkdir $(PSGENDIR)
	-if not exist $(PSGENDIR)$(D)cups mkdir $(PSGENDIR)$(D)cups
	-if not exist $(GLOBJDIR) mkdir $(GLOBJDIR)
	-if not exist $(GLGENDIR) mkdir $(GLGENDIR)
	-if not exist $(DEVOBJDIR) mkdir $(DEVOBJDIR)
	-if not exist $(DEVGENDIR) mkdir $(DEVGENDIR)
	-if not exist $(AUXDIR) mkdir $(AUXDIR)
	-if not exist $(BINDIR) mkdir $(BINDIR)
	echo $(GENOPT) -DCHECK_INTERRUPTS -D_Windows -D__WIN32__ > $(GLGENDIR)\ccf32.tr

$(ECHOGS_XE): $(GLSRC)echogs.c $(GLGENDIR)\ccf32.tr $(MSVCTAIL_MAK)
	$(CCAUX_) $(GLSRC)echogs.c $(AUXO_)echogs.obj /Fe$(ECHOGS_XE) $(CCAUX_TAIL)

# Don't create genarch if it's not needed
!ifdef GENARCH_XE
!ifdef WIN64
# The genarch.exe that is generated is 64-bit, so the OS must be able to run it
$(GENARCH_XE): $(GLSRC)genarch.c $(GENARCH_DEPS) $(GLGENDIR)\ccf32.tr $(MSVCTAIL_MAK)
	$(CCAUX_) /Fo$(AUX)genarch.obj /Fe$(GENARCH_XE) $(GLSRC)genarch.c $(CCAUX_TAIL)
!else
$(GENARCH_XE): $(GLSRC)genarch.c $(GENARCH_DEPS) $(GLGENDIR)\ccf32.tr $(MSVCTAIL_MAK)
	$(CCAUX_) /Fo$(AUX)genarch.obj /Fe$(GENARCH_XE) $(GLSRC)genarch.c $(CCAUX_TAIL)
!endif
!endif

$(GENCONF_XE): $(GLSRC)genconf.c $(GLGENDIR)\ccf32.tr $(GENCONF_DEPS) $(MSVCTAIL_MAK)
	$(CCAUX_) $(GLSRC)genconf.c /Fo$(AUX)genconf.obj /Fe$(GENCONF_XE) $(CCAUX_TAIL)

$(GENDEV_XE): $(GLSRC)gendev.c $(GLGENDIR)\ccf32.tr $(GENDEV_DEPS) $(MSVCTAIL_MAK)
	$(CCAUX_) $(GLSRC)gendev.c /Fo$(AUX)gendev.obj /Fe$(GENDEV_XE) $(CCAUX_TAIL)

$(GENHT_XE): $(GLSRC)genht.c $(GLGENDIR)\ccf32.tr $(GENHT_DEPS) $(MSVCTAIL_MAK)
	$(CCAUX_) $(GENHT_CFLAGS) $(GLSRC)genht.c /Fo$(AUX)genht.obj /Fe$(GENHT_XE) $(CCAUX_TAIL)

MKROMFS_OBJS=$(MKROMFS_ZLIB_OBJS) $(AUX)gp_ntfs.$(OBJ) $(AUX)gp_win32.$(OBJ) $(AUX)gpmisc.$(OBJ)\
            $(AUX)gp_getnv.$(OBJ) $(AUX)gp_wutf8.$(OBJ) $(AUX)gp_winfs.$(OBJ)\
	    $(AUX)gp_winfs2.$(OBJ)
$(MKROMFS_XE): $(GLSRC)mkromfs.c $(GLGENDIR)\ccf32.tr $(MKROMFS_COMMON_DEPS) $(MKROMFS_OBJS) $(MSVCTAIL_MAK)
	$(CCAUX_) -I$(GLOBJ) -I$(ZSRCDIR) $(GLSRC)mkromfs.c /Fo$(AUX)mkromfs.obj /Fe$(MKROMFS_XE) $(MKROMFS_OBJS) $(CCAUX_TAIL)

$(PACKPS_XE): $(GLSRC)pack_ps.c $(GLSRC)stdpre.h $(MSVCTAIL_MAK)
	$(CCAUX_) -I$(GLOBJ) -I$(ZSRCDIR) $(GLSRC)pack_ps.c /Fo$(AUX)pack_ps.obj /Fe$(PACKPS_XE) $(CCAUX_TAIL)

# -------------------------------- Library -------------------------------- #

# See winlib.mak

# ----------------------------- Main program ------------------------------ #

# ole32 is only used for xpsprint, but the pcl/language switch
# build systems don't have XPSPRINT set at this point, so we can't
# rely on it. Always offer the lib for linking, and the linker
# will drop it if unrequired.

LIBCTR=$(GLGEN)libc32.tr

$(LIBCTR): $(TOP_MAKEFILES)
!ifdef METRO
	echo "" >$(LIBCTR)
!else
	echo shell32.lib >$(LIBCTR)
	echo comdlg32.lib >>$(LIBCTR)
	echo gdi32.lib >>$(LIBCTR)
	echo user32.lib >>$(LIBCTR)
	echo winspool.lib >>$(LIBCTR)
	echo advapi32.lib >>$(LIBCTR)
	echo ws2_32.lib >>$(LIBCTR)
	echo ole32.lib >>$(LIBCTR)
!endif

# end of msvctail.mak
