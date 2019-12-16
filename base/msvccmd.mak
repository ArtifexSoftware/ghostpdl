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
# Command definition section for Microsoft Visual C++ 4.x/5.x,
# Windows NT or Windows 95 platform.
# Created 1997-05-22 by L. Peter Deutsch from msvc4/5 makefiles.
# edited 1997-06-xx by JD to factor out interpreter-specific sections
# edited 2000-03-30 by lpd to make /FPi87 conditional on MSVC version
# edited 2000-06-05 by lpd to treat empty MSINCDIR and LIBDIR specially.

# Set up linker differently for MSVC 4 vs. later versions

!if $(MSVC_VERSION) == 4

# MSVC version 4.x doesn't recognize the /QI0f switch, which works around
# an unspecified bug in the Pentium decoding of certain 0F instructions.
QI0f=

!else
#else $(MSVC_VERSION) != 4

# MSVC 5.x does recognize /QI0f.
QI0f=/QI0f

# Define separate CCAUX command-line switch that must be at END of line.
!if $(TDEBUG)!=0
CCAUX_TAIL_DEBUG=/DEBUG
!endif

!if $(MSVC_VERSION) < 7
CCAUX_TAIL= /link $(COMPAUXLDFLAGS) $(CCAUX_TAIL_DEBUG)
!else
!ifdef WIN64
CCAUX_TAIL= /link $(COMPAUXLDFLAGS) $(LINKLIBPATH) $(CCAUX_TAIL_DEBUG)
!else
CCAUX_TAIL= /link $(COMPAUXLDFLAGS) /LIBPATH:"$(COMPBASE)\lib" $(CCAUX_TAIL_DEBUG)
!endif
!endif

!endif
#endif #$(MSVC_VERSION) == 4


# Define the current directory prefix and shell invocations.

D=\#

SH=

# Define switches for the compilers.

C_=/c
O_=-Fo
RO_=$(O_)

# Define the arguments for genconf.

CONFILES=-p %%s
CONFLDTR=-ol

# Define the generic compilation flags.

PLATOPT=

# Make sure we get the right default target for make.

dosdefault: default
	$(NO_OP)

# Define the compilation flags.

# MSVC 8 (2005) warns about deprecated unsafe common functions like strcpy.
!if ($(MSVC_VERSION) > 7) || defined(WIN64)
VC8WARN=/wd4996 /wd4224
!else
VC8WARN=
!endif

!if ($(MSVC_VERSION) < 8)
CDCC=/Gi /Zi
!else
!ifdef WIN64
CDCC=/Zi
!else
CDCC=/Zi
!endif
!endif

!if "$(CPU_FAMILY)"=="i386"

!if ($(MSVC_VERSION) <= 12)
# GB and QI0f were removed at (or before) VS2015
!if ($(MSVC_VERSION) >= 8) || defined(WIN64)
# MSVC 8 (2005) attempts to produce code good for all processors.
# and doesn't used /G5 or /GB.
# MSVC 8 (2005) avoids buggy 0F instructions.
CPFLAGS=
!else
!if $(CPU_TYPE)>500
CPFLAGS=/G5 $(QI0f)
!else if $(CPU_TYPE)>400
CPFLAGS=/GB $(QI0f)
!else
CPFLAGS=/GB $(QI0f)
!endif
!endif
!endif

!if $(MSVC_VERSION)<5
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

!if $(DEBUG)!=0
CD=/DDEBUG
!else
CD=
!endif

# Precompiled headers
!if $(MSVC_VERSION) >= 8
CPCH=/Fp$(GLOBJDIR)\gs.pch
!else
# Precompiled headers don't work with #include MACRO used by freetype
CPCH=
!endif

!ifndef DEBUGSYM
DEBUGSYM=0
!endif

!if $(TDEBUG)!=0
# /Fd designates the directory for the .pdb file.
# Note that it must be followed by a space.
CT=/Od /Fd$(GLOBJDIR)\ $(NULL) $(CDCC) $(CPCH)
LCT=/DEBUG /INCREMENTAL:YES
COMPILE_FULL_OPTIMIZED=    # no optimization when debugging
COMPILE_WITH_FRAMES=    # no optimization when debugging
COMPILE_WITHOUT_FRAMES=    # no optimization when debugging
CMT=/MTd
!else
!if $(DEBUGSYM)==0
CT=
LCT=
CMT=/MT
COMPILE_WITHOUT_FRAMES=/Oy
!else
# Assume that DEBUGSYM != 0 implies a PROFILE build
CT=/Zi /Fd$(GLOBJDIR)\ $(NULL) $(CDCC) $(CPCH)
LCT=/DEBUG /PROFILE /OPT:REF /OPT:ICF
CMT=/MTd
# Do not disable frame pointers in profile builds.
COMPILE_WITHOUT_FRAMES=/Oy-
!endif
!if $(MSVC_VERSION) == 5
# NOTE: With MSVC++ 5.0, /O2 produces a non-working executable.
# We believe the following list of optimizations works around this bug.
COMPILE_FULL_OPTIMIZED=/GF /Ot /Oi /Ob2 /Oa- /Ow- $(COMPILE_WITHOUT_FRAMES)
!else
COMPILE_FULL_OPTIMIZED=/GF /O2 /Ob2 $(COMPILE_WITHOUT_FRAMES)
!endif
COMPILE_WITH_FRAMES=/Oy-
!endif

!if $(MSVC_VERSION) >= 8
# MSVC 8 (2005) always does stack probes and checking.
CS=
!else
!if $(DEBUG)!=0 || $(TDEBUG)!=0
!if $(MSVC_VERSION) < 14
# This flag (Enable stack checks for all functions) has gone in
# VS2015.
CS=/Ge
!endif
!else
CS=/Gs
!endif
!endif

!if ($(MSVC_VERSION) == 7) && defined(WIN64)
# Need to specify DDK include directories before .NET 2003 directories.
MSINCFLAGS=-I"$(INCDIR64A)" -I"$(INCDIR64B)"
!else
MSINCFLAGS=
!endif

# Specify output object name
CCOBJNAME=-Fo

# Specify function prolog type
COMPILE_FOR_DLL=
COMPILE_FOR_EXE=
COMPILE_FOR_CONSOLE_EXE=

# The /MT is for multi-threading.  We would like to make this an option,
# but it's too much work right now.
GENOPT=$(CP) $(CD) $(CT) $(CS) $(WARNOPT) $(VC8WARN) /nologo $(CMT)

CCFLAGS=$(PLATOPT) $(FPFLAGS) $(CPFLAGS) $(CFLAGS) $(XCFLAGS) $(MSINCFLAGS) $(SBRFLAGS)
CC=$(COMP) /c $(CCFLAGS) $(COMPILE_FULL_OPTIMIZED) @$(GLGENDIR)\ccf32.tr
CPP=$(COMPCPP) /c $(CCFLAGS) @$(GLGENDIR)\ccf32.tr
!if $(MAKEDLL)
WX=$(COMPILE_FOR_DLL)
!else
WX=$(COMPILE_FOR_EXE)
!endif

!if $(COMPILE_INITS)
ZM=/Zm1100
!else
ZM=
!endif


# /Za disables the MS-specific extensions & enables ANSI mode.
CC_WX=$(CC) $(WX)
CC_=$(CC_WX) /Za $(ZM)
CC_NO_WARN=$(CC_)

# Compiler for auxiliary programs

CCAUX=$(COMPAUX) $(VC8WARN) $(CCFLAGS) @$(GLGENDIR)\ccf32.tr
CCAUX_=$(COMPAUX) $(VC8WARN) $(CCFLAGS) @$(GLGENDIR)\ccf32.tr
CCAUX_NO_WARN=$(COMPAUX) $(CCFLAGS) @$(GLGENDIR)\ccf32.tr

# Compiler for Windows programs.
CCWINFLAGS=$(COMPILE_FULL_OPTIMIZED)

#end msvccmd.mak
