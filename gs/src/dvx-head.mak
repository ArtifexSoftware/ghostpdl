#    Copyright (C) 1994, 1996, 1997, 1998, 1999 Aladdin Enterprises.  All rights reserved.
# 
# This software is licensed to a single customer by Artifex Software Inc.
# under the terms of a specific OEM agreement.

# $RCSfile$ $Revision$
# Partial makefile, common to all Desqview/X configurations.

# This part of the makefile gets inserted after the compiler-specific part
# (xxx-head.mak) and before gs.mak, devs.mak, and contrib.mak.

# ----------------------------- Generic stuff ----------------------------- #

# Define the platform name.

PLATFORM=dvx_

# Define the syntax for command, object, and executable files.

# Work around the fact that some `make' programs drop trailing spaces
# or interpret == as a special definition operator.
NULL=

CMD=.bat
D_=-D
_D_=$(NULL)=
_D=
I_=-I
II=-I
_I=
NO_OP=@:
O_=-o $(NULL)
OBJ=o
Q=
XE=.exe
XEAUX=.exe

# Define the current directory prefix and command invocations.

CAT=type
D=\\
EXP=
SHELL=
SH=

# Define generic commands.

CP_=cp
RM_=rm -f

# Define the arguments for genconf.

CONFILES=-p -pl &-l%%s
CONFLDTR=-ol

# Define the compilation rules and flags.

CC_D=$(CC_)
CC_INT=$(CC_)

# Patch a couple of PC-specific things that aren't relevant to DV/X builds,
# but that cause `make' to produce warnings.

PCFBASM=
