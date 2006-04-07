# The !ifndef-laden style of this makefile is designed to facilitate "deriving"
# your own make file from it by setting your own custom options, then !including
# this file. In its current form, this file will compile using default options
# and locations.
#
# This file only defines the portions of the MSVC makefile that are different
# between the present language switcher vs. the standard pcl6 makefile which
# is !included near the bottom. All other settings default to the base makefile.

# Define the name of this makefile.
MAKEFILE=$(MAKEFILE) ..\language_switch\pspcl6_msvc.mak

# PLPLATFORM indicates should be set to 'ps' for language switch
# builds and null otherwise.
!ifndef PLPLATFORM
PLPLATFORM=ps
!endif

# The build process will put all of its output in this directory:
!ifndef GENDIR
GENDIR=.\obj
!endif

# The sources are taken from these directories:
!ifndef APPSRCDIR
APPSRCDIR=.
!endif
!ifndef PSSRCDIR
PSSRCDIR=..\gs\src
!endif
!ifndef PSISRCDIR
PSISRCDIR=..\psi
!endif
!ifndef MAINSRCDIR
MAINSRCDIR=..\main
!endif
!ifndef PSLIBDIR
PSLIBDIR=..\gs\lib
!endif
!ifndef ICCSRCDIR
ICCSRCDIR=..\gs\icclib
!endif

# If you want to build the individual packages in their own directories,
# you can define this here, although normally you won't need to do this:
!ifndef GLGENDIR
GLGENDIR=$(GENDIR)
!endif
!ifndef PCLOBJDIR
PCLOBJDIR=$(GENDIR)
!endif
!ifndef PXLOBJDIR
PXLOBJDIR=$(GENDIR)
!endif
!ifndef ICCGENDIR
ICCGENDIR=$(GENDIR)
!endif
!ifndef ICCOBJDIR
ICCOBJDIR=$(GENDIR)
!endif
!ifndef PSGENDIR
PSGENDIR=$(GENDIR)
!endif
!ifndef PSOBJDIR
PSOBJDIR=$(GENDIR)
!endif
!ifndef GLGEN
GLGEN=$(GENDIR)
!endif
!ifndef GLOBJ
GLOBJ=$(GENDIR)
!endif
!ifndef PSIGENDIR
PSIGENDIR=$(GENDIR)
!endif
!ifndef PSIOBJDIR
PSIOBJDIR=$(GENDIR)
!endif

!ifndef DD
DD=$(GLGENDIR)
!endif

# Executable path\name w/o the .EXE extension
!ifndef TARGET_XE
TARGET_XE=$(GENDIR)\pspcl6
!endif
!ifndef PSI_TOP_OBJ
PSI_TOP_OBJ=$(PSIOBJDIR)\psitop.$(OBJ)
!endif

# Banding options
!ifndef BAND_LIST_STORAGE
BAND_LIST_STORAGE=file
!endif

!ifndef COMPILE_INITS
COMPILE_INITS=1
!endif

!ifndef APP_CCC
APP_CCC=$(CC_) -I..\pl -I..\gs\src -I.\obj $(C_)
!endif

# Assorted definitions.  Some of these should probably be factored out....
# We use -O0 for debugging, because optimization confuses gdb.
# Note that the omission of -Dconst= rules out the use of gcc versions
# between 2.7.0 and 2.7.2 inclusive.  (2.7.2.1 is OK.)

!ifndef XCFLAGS
XCFLAGS=/DPSI_INCLUDED
!endif
!ifndef PSICFLAGS
PSICFLAGS=/DPSI_INCLUDED
!endif

# #define xxx_BIND is in std.h
# putting compile time bindings here will have the side effect of having different options
# based on application build.  PSI_INCLUDED is and example of this.
!ifndef EXPERIMENT_CFLAGS
EXPERIMENT_CFLAGS=
!endif

# Language and configuration.  These are actually platform-independent,
# but we define them here just to keep all parameters in one place.
!ifndef TARGET_DEVS
TARGET_DEVS=$(PXLOBJDIR)\pxl.dev $(PCLOBJDIR)\pcl5c.dev $(PCLOBJDIR)\hpgl2c.dev
!endif

!ifndef FEATURE_DEVS    
FEATURE_DEVS    = \
		  $(DD)\psl3.dev		\
		  $(DD)\pdf.dev		\
		  $(DD)\dpsnext.dev	\
                  $(DD)\htxlib.dev	\
                  $(DD)\roplib.dev	\
		  $(DD)\ttfont.dev	\
		  $(DD)\pipe.dev
!endif

!include $(MAINSRCDIR)\pcl6_msvc.mak
!include $(PSISRCDIR)\psi.mak

