#    Copyright (C) 1997 Aladdin Enterprises.  All rights reserved.
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

# msvccom.mak
# Common makefile section for Microsoft Visual C++ 4.x/5.x,
# Windows NT or Windows 95 platform.
# Created 1997-05-22 by L. Peter Deutsch from msvc4/5 makefiles.
# edited 1997-06-xx by JD to factor out interpreter-specific sections

# Set up linker differently for MSVC 4 vs. later versions

!if $(MSVC_VERSION) == 4

# MSVC version 4.x doesn't recognize the /QI0f switch, which works around
# an unspecified bug in the Pentium decoding of certain 0F instructions.
QI0f=

# Set up LIB enviromnent variable to include LIBDIR. This is a hack for
# MSVC4.x, which doesn't have compiler switches to do the deed

!ifdef LIB
LIB=$(LIBDIR);$(LIB)
!else
LINK_SETUP=set LIB=$(LIBDIR)
CCAUX_SETUP=$(LINK_SETUP)
!endif

!else
#else #$(MSVC_VERSION) == 4

# MSVC 5.x does recognize /QI0f.
QI0f=/QI0f

# Define linker switch that will select where MS libraries are.

LINK_LIB_SWITCH=/LIBPATH:$(LIBDIR)

# Define separate CCAUX command-line switch that must be at END of line.

CCAUX_TAIL= /link $(LINK_LIB_SWITCH)

!endif
#endif #$(MSVC_VERSION) == 4


# Define the current directory prefix and shell invocations.

D=\#

EXPP=
SH=
SHP=

# Define the arguments for genconf.

CONFILES=-p %%s -o $(ld_tr) -l lib.tr

# Define the generic compilation flags.

PLATOPT=

INTASM=
PCFBASM=

# Make sure we get the right default target for make.

dosdefault: default

# Define the compilation flags.

!if "$(CPU_FAMILY)"=="i386"

!if $(CPU_TYPE)>500
CPFLAGS=/G5 $(QI0f)
!else if $(CPU_TYPE)>400
CPFLAGS=/GB $(QI0f)
!else
CPFLAGS=/GB $(QI0f)
!endif

!if $(FPU_TYPE)>0
FPFLAGS=/FPi87
!else
FPFLAGS=
!endif

!endif

!if "$(CPU_FAMILY)"=="ppc"

!if $(CPU_TYPE)>=620
CPFLAGS=/QP620
!else if $(CPU_TYPE)>=604
CPFLAGS=/QP604
!else
CPFLAGS=/QP601
!endif

FPFLAGS=

!endif

!if "$(CPU_FAMILY)"=="alpha"

# *** alpha *** This needs fixing
CPFLAGS=
FPFLAGS=

!endif

!if $(NOPRIVATE)!=0
CP=/DNOPRIVATE
!else
CP=
!endif

!if $(DEBUG)!=0
CD=/DDEBUG
!else
CD=
!endif

!if $(TDEBUG)!=0
CT=/Zi /Od
LCT=/DEBUG $(LINK_LIB_SWITCH)
COMPILE_FULL_OPTIMIZED=    # no optimization when debugging
COMPILE_WITH_FRAMES=    # no optimization when debugging
COMPILE_WITHOUT_FRAMES=    # no optimization when debugging
!else
CT=
LCT=$(LINK_LIB_SWITCH)
COMPILE_FULL_OPTIMIZED=/O2
COMPILE_WITH_FRAMES=
COMPILE_WITHOUT_FRAMES=/Oy
!endif

!if $(DEBUG)!=0 || $(TDEBUG)!=0
CS=/Ge
!else
CS=/Gs
!endif

# Specify output object name
CCOBJNAME=-Fo

# Specify function prolog type
COMPILE_FOR_DLL=
COMPILE_FOR_EXE=
COMPILE_FOR_CONSOLE_EXE=


GENOPT=$(CP) $(CD) $(CT) $(CS) /W2 /nologo

CCFLAGS=$(PLATOPT) $(FPFLAGS) $(CPFLAGS) $(CFLAGS) $(XCFLAGS)
CC=$(COMP) /c $(CCFLAGS) @ccf32.tr
CPP=$(COMPCPP) /c $(CCFLAGS) @ccf32.tr
!if $(MAKEDLL)
WX=$(COMPILE_FOR_DLL)
!else
WX=$(COMPILE_FOR_EXE)
!endif
CCC=$(CC) $(WX) $(COMPILE_FULL_OPTIMIZED) /Za
CCD=$(CC) $(WX) $(COMPILE_WITH_FRAMES)
CCINT=$(CCC)
CCCF=$(CCC)
CCLEAF=$(CCC) $(COMPILE_WITHOUT_FRAMES)

# Compiler for auxilliary programs

CCAUX=$(COMPAUX) /I$(INCDIR) /O

# Compiler w/Microsoft extensions for compiling Windows files

CCCWIN=$(CC) $(WX) $(COMPILE_FULL_OPTIMIZED) /Ze

# Define the generic compilation rules.

.c.obj:
	$(CCC) $<

.cpp.obj:
	$(CPP) $<


# Include the generic library makefiles.

!include winlib.mak


# -------------------------- Auxiliary programs --------------------------- #

ccf32.tr: $(MAKEFILE) makefile
	echo $(GENOPT) /I$(INCDIR) -DCHECK_INTERRUPTS -D_Windows -D__WIN32__ > ccf32.tr

$(GENARCH_XE): genarch.c $(stdpre_h) $(iref_h) ccf32.tr
	$(CCAUX_SETUP)
	$(CCAUX) @ccf32.tr genarch.c $(CCAUX_TAIL)

# -------------------------------- Library -------------------------------- #

# See winlib.mak

# ----------------------------- Main program ------------------------------ #

LIBCTR=libc32.tr

$(LIBCTR): $(MAKEFILE) $(ECHOGS_XE)
        echogs -w $(LIBCTR) $(LIBDIR)\shell32.lib
        echogs -a $(LIBCTR) $(LIBDIR)\comdlg32.lib
        echogs -a $(LIBCTR) $(LIBDIR)\gdi32.lib
        echogs -a $(LIBCTR) $(LIBDIR)\user32.lib
        echogs -a $(LIBCTR) $(LIBDIR)\winspool.lib

# end of msvccom.mak
