# Copyright (C) 2001-2024 Artifex Software, Inc.
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
# Artifex Software, Inc.,  39 Mesa Street, Suite 108A, San Francisco,
# CA 94129, USA, for further information.

# Define which major version of MSVC is being used
# (currently, 4, 5, 6, 7, and 8 are supported).
# Define the minor version of MSVC, currently only
# used for Microsoft Visual Studio .NET 2003 (7.1)

#MSVC_VERSION_GUESS=6
#MSVC_MINOR_VERSION=0

# Make a guess at the version of MSVC in use
# This will not work if service packs change the version numbers.

!if defined(_NMAKE_VER) && !defined(MSVC_VERSION)
!if "$(_NMAKE_VER)" == "162"
MSVC_VERSION_GUESS=5
!endif
!if "$(_NMAKE_VER)" == "6.00.8168.0"
# VC 6
MSVC_VERSION_GUESS=6
!endif
!if "$(_NMAKE_VER)" == "7.00.9466"
MSVC_VERSION_GUESS=7
!endif
!if "$(_NMAKE_VER)" == "7.00.9955"
MSVC_VERSION_GUESS=7
!endif
!if "$(_NMAKE_VER)" == "7.10.3077"
MSVC_VERSION_GUESS=7
MSVC_MINOR_VERSION=1
!endif
!if "$(_NMAKE_VER)" == "8.00.40607.16"
# VS2005
MSVC_VERSION_GUESS=8
!endif
!if "$(_NMAKE_VER)" == "8.00.50727.42"
# VS2005
MSVC_VERSION_GUESS=8
!endif
!if "$(_NMAKE_VER)" == "8.00.50727.762"
# VS2005
MSVC_VERSION_GUESS=8
!endif
!if "$(_NMAKE_VER)" == "9.00.21022.08"
# VS2008
MSVC_VERSION_GUESS=9
!endif
!if "$(_NMAKE_VER)" == "9.00.30729.01"
# VS2008
MSVC_VERSION_GUESS=9
!endif
!if "$(_NMAKE_VER)" == "10.00.30319.01"
# VS2010
MSVC_VERSION_GUESS=10
!endif
!if "$(_NMAKE_VER)" == "11.00.50522.1"
# VS2012
MSVC_VERSION_GUESS=11
!endif
!if "$(_NMAKE_VER)" == "11.00.50727.1"
# VS2012
MSVC_VERSION_GUESS=11
!endif
!if "$(_NMAKE_VER)" == "11.00.60315.1"
# VS2012
MSVC_VERSION_GUESS=11
!endif
!if "$(_NMAKE_VER)" == "11.00.60610.1"
# VS2012
MSVC_VERSION_GUESS=11
!endif
!if "$(_NMAKE_VER)" == "12.00.21005.1"
# VS 2013
MSVC_VERSION_GUESS=12
!endif
!if "$(_NMAKE_VER)" == "14.00.23506.0"
# VS2015
MSVC_VERSION_GUESS=14
!endif
!if "$(_NMAKE_VER)" == "14.00.24210.0"
# VS2015
MSVC_VERSION_GUESS=14
!endif
!if "$(_NMAKE_VER)" == "14.16.27034.0"
# VS2017 or VS2019 (Toolset v141)
MSVC_VERSION_GUESS=15
MS_TOOLSET_VERSION_GUESS=14.16.27034
!endif
!if "$(_NMAKE_VER)" == "14.16.27043.0"
# VS2017 or VS2019 (Toolset v141)
MSVC_VERSION_GUESS=15
MS_TOOLSET_VERSION_GUESS=14.16.27034
!endif
!if "$(_NMAKE_VER)" == "14.24.28314.0"
# VS2019 (Toolset v142)
MSVC_VERSION_GUESS=16
MS_TOOLSET_VERSION_GUESS=14.24.28314
!endif
!if "$(_NMAKE_VER)" == "14.24.28315.0"
# VS2019 (Toolset v142 - update)
MSVC_VERSION_GUESS=16
MS_TOOLSET_VERSION_GUESS=14.24.28315
!endif
!if "$(_NMAKE_VER)" == "14.24.28316.0"
# VS2019 (Toolset v142 - update)
MSVC_VERSION_GUESS=16
MS_TOOLSET_VERSION_GUESS=14.24.28316
!endif
!if "$(_NMAKE_VER)" == "14.25.28614.0"
# VS2019 (Toolset v142 - update)
MSVC_VERSION_GUESS=16
MS_TOOLSET_VERSION_GUESS=14.25.28614
!endif
!if "$(_NMAKE_VER)" == "14.26.28805.0"
# VS2019 (Toolset v142 - update)
MSVC_VERSION_GUESS=16
MS_TOOLSET_VERSION_GUESS=14.26.28805
!endif
!if "$(_NMAKE_VER)" == "14.26.28806.0"
# VS2019 (Toolset v142 - update)
MSVC_VERSION_GUESS=16
MS_TOOLSET_VERSION_GUESS=14.26.28806
!endif
!if "$(_NMAKE_VER)" == "14.27.29111.0"
# VS2019 (Toolset v142)
MSVC_VERSION_GUESS=16
MS_TOOLSET_VERSION_GUESS=14.27.29111
!endif
!if "$(_NMAKE_VER)" == "14.27.29112.0"
# VS2019 (Toolset v142)
MSVC_VERSION_GUESS=16
MS_TOOLSET_VERSION_GUESS=14.27.29112
!endif
!if "$(_NMAKE_VER)" == "14.28.29333.0"
# VS2019 (Toolset v142)
MSVC_VERSION_GUESS=16
MS_TOOLSET_VERSION_GUESS=14.28.29333
!endif
!if "$(_NMAKE_VER)" == "14.28.29334.0"
# VS2019 (Toolset v142)
MSVC_VERSION_GUESS=16
MS_TOOLSET_VERSION_GUESS=14.28.29333
!endif
!if "$(_NMAKE_VER)" == "14.28.29335.0"
# VS2019 (Toolset v142)
MSVC_VERSION_GUESS=16
MS_TOOLSET_VERSION_GUESS=14.28.29333
!endif
!if "$(_NMAKE_VER)" == "14.28.29336.0"
# VS2019 (Toolset v142)
MSVC_VERSION_GUESS=16
MS_TOOLSET_VERSION_GUESS=14.28.29333
!endif
!if "$(_NMAKE_VER)" == "14.28.29910.0"
# VS2019 (Toolset v142)
MSVC_VERSION_GUESS=16
MS_TOOLSET_VERSION_GUESS=14.28.29333
!endif
!if "$(_NMAKE_VER)" == "14.28.29913.0"
# VS2019 (Toolset v142)
MSVC_VERSION_GUESS=16
MS_TOOLSET_VERSION_GUESS=14.28.29333
!endif
!if "$(_NMAKE_VER)" == "14.28.29914.0"
# VS2019 (Toolset v142)
MSVC_VERSION_GUESS=16
MS_TOOLSET_VERSION_GUESS=14.28.29333
!endif
!if "$(_NMAKE_VER)" == "14.28.29915.0"
# VS2019 (Toolset v142)
MSVC_VERSION_GUESS=16
MS_TOOLSET_VERSION_GUESS=14.28.29333
!endif
!if "$(_NMAKE_VER)" == "14.29.30038.1"
# VS2019 (Toolset v142)
MSVC_VERSION_GUESS=16
MS_TOOLSET_VERSION_GUESS=14.29.30037
!endif
!if "$(_NMAKE_VER)" == "14.29.30133.0"
# VS2019 (Toolset v142)
MSVC_VERSION_GUESS=16
MS_TOOLSET_VERSION_GUESS=14.29.30133
!endif
!if "$(_NMAKE_VER)" == "14.29.30136.0"
# VS2019 (Toolset v142)
MSVC_VERSION_GUESS=16
MS_TOOLSET_VERSION_GUESS=14.29.30133
!endif
!if "$(_NMAKE_VER)" == "14.29.30137.0"
# VS2019 (Toolset v142)
MSVC_VERSION_GUESS=16
MS_TOOLSET_VERSION_GUESS=14.29.30133
!endif
!if "$(_NMAKE_VER)" == "14.29.30139.0"
# VS2019 (Toolset v142)
MSVC_VERSION_GUESS=16
MS_TOOLSET_VERSION_GUESS=14.29.30133
!endif
!if "$(_NMAKE_VER)" == "14.29.30140.0"
# VS2019 (Toolset v142)
MSVC_VERSION_GUESS=16
MS_TOOLSET_VERSION_GUESS=14.29.30133
!endif
!if "$(_NMAKE_VER)" == "14.29.30141.0"
# VS2019 (Toolset v142)
MSVC_VERSION_GUESS=16
MS_TOOLSET_VERSION_GUESS=14.29.30133
!endif
!if "$(_NMAKE_VER)" == "14.29.30142.1"
# VS2019 (Toolset v142)
MSVC_VERSION_GUESS=16
MS_TOOLSET_VERSION_GUESS=14.29.30133
!endif
!if "$(_NMAKE_VER)" == "14.29.30145.0"
# VS2019 (Toolset v142)
MSVC_VERSION_GUESS=16
MS_TOOLSET_VERSION_GUESS=14.29.30133
!endif
!if "$(_NMAKE_VER)" == "14.29.30146.0"
# VS2019 (Toolset v142)
MSVC_VERSION_GUESS=16
MS_TOOLSET_VERSION_GUESS=14.29.30133
!endif
!if "$(_NMAKE_VER)" == "14.29.30147.0"
# VS2019 (Toolset v142)
MSVC_VERSION_GUESS=16
MS_TOOLSET_VERSION_GUESS=14.29.30133
!endif
!if "$(_NMAKE_VER)" == "14.29.30148.0"
# VS2019 (Toolset v142)
MSVC_VERSION_GUESS=16
MS_TOOLSET_VERSION_GUESS=14.29.30133
!endif
!if "$(_NMAKE_VER)" == "14.29.30152.0"
# VS2019 (Toolset v142)
MSVC_VERSION_GUESS=16
MS_TOOLSET_VERSION_GUESS=14.29.30133
!endif
!if "$(_NMAKE_VER)" == "14.29.30153.0"
# VS2019 (Toolset v142)
MSVC_VERSION_GUESS=16
MS_TOOLSET_VERSION_GUESS=14.29.30133
!endif
!if "$(_NMAKE_VER)" == "14.29.30154.0"
# VS2019 (Toolset v142)
MSVC_VERSION_GUESS=16
MS_TOOLSET_VERSION_GUESS=14.29.30133
!endif
!if "$(_NMAKE_VER)" == "14.29.30156.0"
# VS2019 (Toolset v142)
MSVC_VERSION_GUESS=16
MS_TOOLSET_VERSION_GUESS=14.29.30133
!endif
!if "$(_NMAKE_VER)" == "14.37.32822.0"
# VS2019 (Toolset v142)
MSVC_VERSION_GUESS=16
MS_TOOLSET_VERSION_GUESS=14.37.32822.0
!endif
!if "$(_NMAKE_VER)" == "14.38.33133.0"
# VS2019 (Toolset v142)
MSVC_VERSION_GUESS=16
MS_TOOLSET_VERSION_GUESS=14.38.33133.0
!endif
!if "$(_NMAKE_VER)" == "14.38.33134.0"
# VS2019 (Toolset v142)
MSVC_VERSION_GUESS=16
MS_TOOLSET_VERSION_GUESS=14.38.33134.0
!endif
!if "$(_NMAKE_VER)" == "14.39.33523.0"
# VS2022 (Toolset v143)
MSVC_VERSION_GUESS=17
MS_TOOLSET_VERSION_GUESS=14.38.33519.0
!endif
!if "$(_NMAKE_VER)" == "14.41.34120.0"
# VS2022 (Toolset v143)
MSVC_VERSION_GUESS=17
MS_TOOLSET_VERSION_GUESS=14.41.34120.0
!endif

!endif


# Visual Studio 2019 and 2022 at least (16 and 17) now sets some
# helpful environment variables for us:
#   VisualStudioDir=C:\Users\Robin\Documents\Visual Studio 2022
#   VisualStudioEdition=Microsoft Visual Studio Community 2022
#   VisualStudioVersion=17.0
# The command prompt also sets:
#   VCToolsVersion=14.29.30133
# Let's use these to refine our guessing.

!if "$(VisualStudioVersion)" == "16.0"
MSVC_VERSION_GUESS=16
!endif
!if "$(VisualStudioVersion)" == "17.0"
MSVC_VERSION_GUESS=17
!endif
!if "$(VCToolsVersion)" != ""
MS_TOOLSET_VERSION_GUESS=$(VCToolsVersion).0
!endif

# We allow MSVC_VERSION to be defined by the caller, and
# only override it by our guess if it hasn't already been
# set.
!ifndef MSVC_VERSION
MSVC_VERSION=$(MSVC_VERSION_GUESS)
!endif

# We allow MS_TOOLSET_VERSION to be defined by the caller, and
# only override it by our guess if it hasn't already been
# set.
!ifndef MS_TOOLSET_VERSION
MS_TOOLSET_VERSION=$(MS_TOOLSET_VERSION_GUESS)
!endif

!if "$(MSVC_VERSION)" == ""
!MESSAGE Could not determine MSVC_VERSION! Guessing at an ancient one.
!MESSAGE Unknown nmake version: $(_NMAKE_VER)
MSVC_VERSION=6
!endif
!ifndef MSVC_MINOR_VERSION
MSVC_MINOR_VERSION=0
!endif

# Define the drive, directory, and compiler name for the Microsoft C files.
# COMPDIR contains the compiler and linker (normally \msdev\bin).
# MSINCDIR contains the include files (normally \msdev\include).
# LIBDIR contains the library files (normally \msdev\lib).
# COMP is the full C compiler path name (normally \msdev\bin\cl).
# COMPCPP is the full C++ compiler path name (normally \msdev\bin\cl).
# COMPAUX is the compiler name for DOS utilities (normally \msdev\bin\cl).
# RCOMP is the resource compiler name (normallly \msdev\bin\rc).
# LINK is the full linker path name (normally \msdev\bin\link).
# Note that when MSINCDIR and LIBDIR are used, they always get a '\' appended,
#   so if you want to use the current directory, use an explicit '.'.

!if $(MSVC_VERSION) == 4
! ifndef DEVSTUDIO
DEVSTUDIO=c:\msdev
! endif
COMPBASE=$(DEVSTUDIO)
SHAREDBASE=$(DEVSTUDIO)
!endif

!if $(MSVC_VERSION) == 5
! ifndef DEVSTUDIO
DEVSTUDIO=C:\Program Files\Devstudio
! endif
!if "$(DEVSTUDIO)"==""
COMPBASE=
SHAREDBASE=
!else
COMPBASE=$(DEVSTUDIO)\VC
SHAREDBASE=$(DEVSTUDIO)\SharedIDE
!endif
!endif

!if $(MSVC_VERSION) == 6
! ifndef DEVSTUDIO
DEVSTUDIO=C:\Program Files\Microsoft Visual Studio
! endif
!if "$(DEVSTUDIO)"==""
COMPBASE=
SHAREDBASE=
!else
COMPBASE=$(DEVSTUDIO)\VC98
SHAREDBASE=$(DEVSTUDIO)\Common\MSDev98
!endif
!endif

!if $(MSVC_VERSION) == 7
! ifndef DEVSTUDIO
!if $(MSVC_MINOR_VERSION) == 0
DEVSTUDIO=C:\Program Files\Microsoft Visual Studio .NET
!else
DEVSTUDIO=C:\Program Files\Microsoft Visual Studio .NET 2003
!endif
! endif
!if "$(DEVSTUDIO)"==""
COMPBASE=
SHAREDBASE=
!else
COMPBASE=$(DEVSTUDIO)\Vc7
SHAREDBASE=$(DEVSTUDIO)\Vc7
!endif
!endif

!if $(MSVC_VERSION) == 8
! ifndef DEVSTUDIO
!if $(BUILD_SYSTEM) == 64
DEVSTUDIO=C:\Program Files (x86)\Microsoft Visual Studio 8
!else
DEVSTUDIO=C:\Program Files\Microsoft Visual Studio 8
!endif
! endif
!if "$(DEVSTUDIO)"==""
COMPBASE=
SHAREDBASE=
!else
COMPBASE=$(DEVSTUDIO)\VC
SHAREDBASE=$(DEVSTUDIO)\VC
!ifdef WIN64
COMPDIR64=$(COMPBASE)\bin\amd64
LINKLIBPATH=/LIBPATH:"$(COMPBASE)\lib\amd64" /LIBPATH:"$(COMPBASE)\PlatformSDK\Lib\AMD64"
!endif
!endif
!endif

!if $(MSVC_VERSION) == 9
! ifndef DEVSTUDIO
!if $(BUILD_SYSTEM) == 64
DEVSTUDIO=C:\Program Files (x86)\Microsoft Visual Studio 9.0
!else
DEVSTUDIO=C:\Program Files\Microsoft Visual Studio 9.0
!endif
! endif
!if "$(DEVSTUDIO)"==""
COMPBASE=
SHAREDBASE=
!else
# There are at least 4 different values:
# "v6.0"=Vista, "v6.0A"=Visual Studio 2008,
# "v6.1"=Windows Server 2008, "v7.0"=Windows 7
! ifdef MSSDK
RCDIR=$(MSSDK)\bin
! else
RCDIR=C:\Program Files\Microsoft SDKs\Windows\v6.0A\bin
! endif
COMPBASE=$(DEVSTUDIO)\VC
SHAREDBASE=$(DEVSTUDIO)\VC
!ifdef WIN64
COMPDIR64=$(COMPBASE)\bin\amd64
LINKLIBPATH=/LIBPATH:"$(COMPBASE)\lib\amd64" /LIBPATH:"$(COMPBASE)\PlatformSDK\Lib\AMD64"
!endif
!endif
!endif

!if $(MSVC_VERSION) == 10
! ifndef DEVSTUDIO
!if $(BUILD_SYSTEM) == 64
DEVSTUDIO=C:\Program Files (x86)\Microsoft Visual Studio 10.0
!else
DEVSTUDIO=C:\Program Files\Microsoft Visual Studio 10.0
!endif
! endif
!if "$(DEVSTUDIO)"==""
COMPBASE=
SHAREDBASE=
!else
# There are at least 4 different values:
# "v6.0"=Vista, "v6.0A"=Visual Studio 2008,
# "v6.1"=Windows Server 2008, "v7.0"=Windows 7
! ifdef MSSDK
!  ifdef WIN64
RCDIR=$(MSSDK)\bin\x64
!  else
RCDIR=$(MSSDK)\bin
!  endif
! else
!ifdef WIN64
RCDIR=C:\Program Files (x86)\Microsoft SDKs\Windows\v7.0\bin
!else
RCDIR=C:\Program Files\Microsoft SDKs\Windows\v7.0\bin
!endif
! endif
COMPBASE=$(DEVSTUDIO)\VC
SHAREDBASE=$(DEVSTUDIO)\VC
!ifdef WIN64
!if $(BUILD_SYSTEM) == 64
COMPDIR64=$(COMPBASE)\bin\amd64
LINKLIBPATH=/LIBPATH:"$(MSSDK)\lib\x64" /LIBPATH:"$(COMPBASE)\lib\amd64"
!else
COMPDIR64=$(COMPBASE)\bin\x86_amd64
LINKLIBPATH=/LIBPATH:"$(COMPBASE)\lib\amd64" /LIBPATH:"$(COMPBASE)\PlatformSDK\Lib\x64"
!endif
!endif
!endif
!endif

!if $(MSVC_VERSION) == 11
! ifndef DEVSTUDIO
!if $(BUILD_SYSTEM) == 64
DEVSTUDIO=C:\Program Files (x86)\Microsoft Visual Studio 11.0
!else
DEVSTUDIO=C:\Program Files\Microsoft Visual Studio 11.0
!endif
! endif
!if "$(DEVSTUDIO)"==""
COMPBASE=
SHAREDBASE=
!else
# There are at least 4 different values:
# "v6.0"=Vista, "v6.0A"=Visual Studio 2008,
# "v6.1"=Windows Server 2008, "v7.0"=Windows 7
! ifdef MSSDK
!  ifdef WIN64
RCDIR=$(MSSDK)\bin\x64
!  else
RCDIR=$(MSSDK)\bin
!  endif
! else
!if $(BUILD_SYSTEM) == 64
RCDIR=C:\Program Files (x86)\Windows Kits\8.0\bin\x64
!else
RCDIR=C:\Program Files\Windows Kits\8.0\bin\x86
!endif
! endif
COMPBASE=$(DEVSTUDIO)\VC
SHAREDBASE=$(DEVSTUDIO)\VC
!ifdef WIN64
!if $(BUILD_SYSTEM) == 64
COMPDIR64=$(COMPBASE)\bin\x86_amd64
LINKLIBPATH=/LIBPATH:"$(MSSDK)\lib\x64" /LIBPATH:"$(COMPBASE)\lib\amd64"
!else
COMPDIR64=$(COMPBASE)\bin\x86_amd64
LINKLIBPATH=/LIBPATH:"$(COMPBASE)\lib\amd64" /LIBPATH:"$(COMPBASE)\PlatformSDK\Lib\x64"
!endif
!endif
!endif
!endif

!if $(MSVC_VERSION) == 12
! ifndef DEVSTUDIO
!  if $(BUILD_SYSTEM) == 64
DEVSTUDIO=C:\Program Files (x86)\Microsoft Visual Studio 12.0
!  else
DEVSTUDIO=C:\Program Files\Microsoft Visual Studio 12.0
!  endif
! endif
! if "$(DEVSTUDIO)"==""
COMPBASE=
SHAREDBASE=
! else
# There are at least 4 different values:
# "v6.0"=Vista, "v6.0A"=Visual Studio 2008,
# "v6.1"=Windows Server 2008, "v7.0"=Windows 7
!  ifdef MSSDK
!   ifdef WIN64
RCDIR=$(MSSDK)\bin\x64
!   else
RCDIR=$(MSSDK)\bin
!   endif
!  else
!   if $(BUILD_SYSTEM) == 64
RCDIR=C:\Program Files (x86)\Microsoft SDKs\Windows\v7.1A\Bin
!   else
RCDIR=C:\Program Files\Microsoft SDKs\Windows\v7.1A\Bin
!   endif
!  endif
COMPBASE=$(DEVSTUDIO)\VC
SHAREDBASE=$(DEVSTUDIO)\VC
!  ifdef WIN64
!   if $(BUILD_SYSTEM) == 64
COMPDIR64=$(COMPBASE)\bin\x86_amd64
LINKLIBPATH=/LIBPATH:"$(COMPBASE)\lib\amd64"
!   else
COMPDIR64=$(COMPBASE)\bin\x86_amd64
LINKLIBPATH=/LIBPATH:"$(COMPBASE)\lib\amd64" /LIBPATH:"$(COMPBASE)\PlatformSDK\Lib\x64"
!   endif
!  endif
! endif
!endif

!if $(MSVC_VERSION) == 14
! ifndef DEVSTUDIO
!  if $(BUILD_SYSTEM) == 64
DEVSTUDIO=C:\Program Files (x86)\Microsoft Visual Studio 14.0
!  else
DEVSTUDIO=C:\Program Files\Microsoft Visual Studio 14.0
!  endif
! endif
! if "$(DEVSTUDIO)"==""
COMPBASE=
SHAREDBASE=
! else
# There are at least 4 different values:
# "v6.0"=Vista, "v6.0A"=Visual Studio 2008,
# "v6.1"=Windows Server 2008, "v7.0"=Windows 7
!  ifdef MSSDK
!   ifdef WIN64
RCDIR=$(MSSDK)\bin\x64
!   else
RCDIR=$(MSSDK)\bin
!   endif
!  else
!   if $(BUILD_SYSTEM) == 64
RCDIR=C:\Program Files (x86)\Microsoft SDKs\Windows\v10.0A\Bin
!   else
RCDIR=C:\Program Files\Microsoft SDKs\Windows\v10.0A\Bin
!   endif
!  endif
COMPBASE=$(DEVSTUDIO)\VC
SHAREDBASE=$(DEVSTUDIO)\VC
!  ifdef WIN64
!   if $(BUILD_SYSTEM) == 64
COMPDIR64=$(COMPBASE)\bin\x86_amd64
LINKLIBPATH=/LIBPATH:"$(COMPBASE)\lib\amd64"
!   else
COMPDIR64=$(COMPBASE)\bin\x86_amd64
LINKLIBPATH=/LIBPATH:"$(COMPBASE)\lib\amd64" /LIBPATH:"$(COMPBASE)\PlatformSDK\Lib\x64"
!   endif
!  endif
! endif
!endif

!if $(MSVC_VERSION) == 15
! ifndef DEVSTUDIO
DEVSTUDIO=C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Tools\MSVC\$(MS_TOOLSET_VERSION)
! endif
! if "$(DEVSTUDIO)"==""
COMPBASE=
SHAREDBASE=
! else
!  if $(BUILD_SYSTEM) == 64
DEVSTUDIO_HOST=Hostx64
!  else
DEVSTUDIO_HOST=Hostx86
!  endif
!  ifdef WIN64
DEVSTUDIO_TARGET=x64
!  else
DEVSTUDIO_TARGET=x86
!  endif
COMPDIR=$(DEVSTUDIO)\bin\$(DEVSTUDIO_HOST)\$(DEVSTUDIO_TARGET)
RCDIR=
LINKLIBPATH=/LIBPATH:"$(DEVSTUDIO)\lib\$(DEVSTUDIO_TARGET)"
! endif
!endif

!if $(MSVC_VERSION) == 16
! ifndef DEVSTUDIO
!  ifdef VCToolsInstallDir
DEVSTUDIO=$(VCToolsInstallDir)
!  else
DEVSTUDIO_PATH=C:\Program Files (x86)\Microsoft Visual Studio\2019
!   if "$(VisualStudioEdition)" == "Microsoft Visual Studio Community 2019"
DEVSTUDIO_VARIANT=Community
!   else
!    if "$(VisualStudioEdition)" == "Microsoft Visual Studio Professional 2019"
DEVSTUDIO_VARIANT=Professional
!    else
!     if exist("$(DEVSTUDIO_PATH)\Professional")
DEVSTUDIO_VARIANT=Professional
!     else
DEVSTUDIO_VARIANT=Community
!     endif
!    endif
!   endif
DEVSTUDIO=$(DEVSTUDIO_PATH)\$(DEVSTUDIO_VARIANT)\VC\Tools\MSVC\$(MS_TOOLSET_VERSION)
!  endif
! endif
! if "$(DEVSTUDIO)"==""
COMPBASE=
SHAREDBASE=
! else
!  if $(BUILD_SYSTEM) == 64
DEVSTUDIO_HOST=Hostx64
!  else
DEVSTUDIO_HOST=Hostx86
!  endif
!  ifdef WIN64
DEVSTUDIO_TARGET=x64
!  else
DEVSTUDIO_TARGET=x86
!  endif
COMPDIR=$(DEVSTUDIO)\bin\$(DEVSTUDIO_HOST)\$(DEVSTUDIO_TARGET)
RCDIR=
LINKLIBPATH=/LIBPATH:"$(DEVSTUDIO)\lib\$(DEVSTUDIO_TARGET)"
! endif
!endif

!if $(MSVC_VERSION) == 17
! ifndef DEVSTUDIO
!  ifdef VCToolsInstallDir
DEVSTUDIO=$(VCToolsInstallDir)
!  else
DEVSTUDIO_PATH=C:\Program Files (x86)\Microsoft Visual Studio\2022
!   if "$(VisualStudioEdition)" == "Microsoft Visual Studio Community 2022"
DEVSTUDIO_VARIANT=Community
!   else
!    if "$(VisualStudioEdition)" == "Microsoft Visual Studio Professional 2022"
DEVSTUDIO_VARIANT=Professional
!    else
!     if exist("$(DEVSTUDIO_PATH)\Professional")
DEVSTUDIO_VARIANT=Professional
!     else
DEVSTUDIO_VARIANT=Community
!      if !exist("$(DEVSTUDIO_PATH)\Community")
!       if exist("$(DEVSTUDIO_PATH)\BuildTools")
DEVSTUDIO_VARIANT=BuildTools
!       endif
!      endif
!     endif
!    endif
!   endif
DEVSTUDIO=$(DEVSTUDIO_PATH)\$(DEVSTUDIO_VARIANT)\VC\Tools\MSVC\$(MS_TOOLSET_VERSION)
!  endif
! endif
! if "$(DEVSTUDIO)"==""
COMPBASE=
SHAREDBASE=
! else
!  if $(BUILD_SYSTEM) == 64
DEVSTUDIO_HOST=Hostx64
!  else
DEVSTUDIO_HOST=Hostx86
!  endif
!  ifdef WIN64
DEVSTUDIO_TARGET=x64
!  else
DEVSTUDIO_TARGET=x86
!  endif
COMPDIR=$(DEVSTUDIO)\bin\$(DEVSTUDIO_HOST)\$(DEVSTUDIO_TARGET)
RCDIR=
LINKLIBPATH=/LIBPATH:"$(DEVSTUDIO)\lib\$(DEVSTUDIO_TARGET)"
! endif
!endif

!if "$(ARM)"=="1"
VCINSTDIR=$(VS110COMNTOOLS)..\..\VC\

!ifndef WINSDKVER
WINSDKVER=8.0
!endif

!ifndef WINSDKDIR
WINSDKDIR=$(VS110COMNTOOLS)..\..\..\Windows Kits\$(WINSDKVER)
!endif

COMPAUX__="$(VCINSTDIR)\bin\cl.exe"
COMPAUXCFLAGS=/I"$(VCINSTDIR)\INCLUDE" /I"$(VCINSTDIR)\ATLMFC\INCLUDE" \
/I"$(WINSDKDIR)\include\shared" /I"$(WINSDKDIR)\include\um" \
/I"$(WINDSKDIR)include\winrt"

COMPAUXLDFLAGS=/LIBPATH:"$(VCINSTDIR)\LIB" \
/LIBPATH:"$(VCINSTDIR)\ATLMFC\LIB" \
/LIBPATH:"$(WINSDKDIR)\lib\win8\um\x86"

COMPAUX=$(COMPAUX__) $(COMPAUXCFLAGS)

!else

COMPAUXLDFLAGS=""

!endif

# Some environments don't want to specify the path names for the tools at all.
# Typical definitions for such an environment would be:
#   MSINCDIR= LIBDIR= COMP=cl COMPAUX=cl RCOMP=rc LINK=link
# COMPDIR, LINKDIR, and RCDIR are irrelevant, since they are only used to
# define COMP, LINK, and RCOMP respectively, but we allow them to be
# overridden anyway for completeness.
!ifndef COMPDIR
!if "$(COMPBASE)"==""
COMPDIR=
!else
!ifdef WIN64
COMPDIR=$(COMPDIR64)
!else
COMPDIR=$(COMPBASE)\bin
!endif
!endif
!endif

!ifndef LINKDIR
!if "$(COMPBASE)"==""
LINKDIR=
!else
!ifdef WIN64
LINKDIR=$(COMPDIR64)
!else
LINKDIR=$(COMPBASE)\bin
!endif
!endif
!endif

!ifndef RCDIR
!if "$(SHAREDBASE)"==""
RCDIR=
!else
RCDIR=$(SHAREDBASE)\bin
!endif
!endif

!ifndef MSINCDIR
!if "$(COMPBASE)"==""
MSINCDIR=
!else
MSINCDIR=$(COMPBASE)\include
!endif
!endif

!ifndef LIBDIR
!if "$(COMPBASE)"==""
LIBDIR=
!else
!ifdef WIN64
LIBDIR=$(COMPBASE)\lib\amd64
!else
LIBDIR=$(COMPBASE)\lib
!endif
!endif
!endif

!ifndef COMP
!if "$(COMPDIR)"==""
COMP=cl
!else
COMP="$(COMPDIR)\cl"
!endif
!endif
!ifndef COMPCPP
COMPCPP=$(COMP)
!endif
!ifndef COMPAUX
!ifdef WIN64
COMPAUX=$(COMP)
!else
COMPAUX=$(COMP)
!endif
!endif

!ifndef RCOMP
!if "$(RCDIR)"==""
RCOMP=rc
!else
RCOMP="$(RCDIR)\rc"
!endif
!endif

!ifndef LINK
!if "$(LINKDIR)"==""
LINK=link
!else
LINK="$(LINKDIR)\link"
!endif
!endif

# nmake does not have a form of .BEFORE or .FIRST which can be used
# to specify actions before anything else is done.  If LIB and INCLUDE
# are not defined then we want to define them before we link or
# compile.  Here is a kludge which allows us to to do what we want.
# nmake does evaluate preprocessor directives when they are encountered.
# So the desired set statements are put into dummy preprocessor
# directives.
!ifndef INCLUDE
!if "$(MSINCDIR)"!=""
!if [set INCLUDE=$(MSINCDIR)]==0
!endif
!endif
!endif
!ifndef LIB
!if "$(LIBDIR)"!=""
!if [set LIB=$(LIBDIR)]==0
!endif
!endif
!endif

!ifndef LINKLIBPATH
LINKLIBPATH=
!endif

# Define the processor architecture. (i386, ppc, alpha)

!ifndef CPU_FAMILY
CPU_FAMILY=i386
#CPU_FAMILY=ppc
#CPU_FAMILY=alpha  # not supported yet - we need someone to tweak
!endif

# Define the processor (CPU) type. Allowable values depend on the family:
#   i386: 386, 486, 586
#   ppc: 601, 604, 620
#   alpha: not currently used.

!ifndef CPU_TYPE
CPU_TYPE=486
#CPU_TYPE=601
!endif

# Define special features of CPUs

# We'll assume that if you have an x86 machine, you've got a modern
# enough one to have SSE2 instructions. If you don't, then predefine
# DONT_HAVE_SSE2 when calling this makefile
!ifndef ARM
!if "$(CPU_FAMILY)" == "i386"
!ifndef DONT_HAVE_SSE2
!ifndef HAVE_SSE2
!message **************************************************************
!message * Assuming that target has SSE2 instructions available. If   *
!message * this is NOT the case, define DONT_HAVE_SSE2 when building. *
!message **************************************************************
!endif
HAVE_SSE2=1
CFLAGS=$(CFLAGS) /DHAVE_SSE2
# add "/D__SSE__" here, but causes crashes just now
JPX_SSE_CFLAGS=
!endif
!endif
!endif

# Define the .dev module that implements thread and synchronization
# primitives for this platform.  Don't change this unless you really know
# what you're doing.

!ifndef SYNC
SYNC=winsync
!endif

# OpenJPEG compiler flags
#
!if "$(JPX_LIB)" == "openjpeg"
!ifndef JPXSRCDIR
JPXSRCDIR=openjpeg
!endif
!ifndef JPX_CFLAGS
!ifdef WIN64
JPX_CFLAGS=-DMUTEX_pthread=0 -DUSE_OPENJPEG_JP2 -DUSE_JPIP $(JPX_SSE_CFLAGS) -DOPJ_STATIC -DWIN64
!else
JPX_CFLAGS=-DMUTEX_pthread=0 -DUSE_OPENJPEG_JP2 -DUSE_JPIP $(JPX_SSE_CFLAGS) -DOPJ_STATIC -DWIN32
!endif
!else
JPX_CFLAGS = $JPX_CFLAGS -DUSE_JPIP -DUSE_OPENJPEG_JP2 -DOPJ_STATIC
!endif
!endif
