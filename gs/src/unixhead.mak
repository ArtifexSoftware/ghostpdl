#    Copyright (C) 1990, 1993, 1996 Aladdin Enterprises.  All rights reserved.
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

# This part of the makefile gets inserted after the compiler-specific part
# (xxx-head.mak) and before gs.mak and devs.mak.

# ----------------------------- Generic stuff ----------------------------- #

# Define the platform name.  For a "stock" System V platform,
# use sysv_ instead of unix_.

PLATFORM=unix_

# Define the syntax for command, object, and executable files.

CMD=
O=-o ./
OBJ=o
XE=
XEAUX=

# Define the current directory prefix and command invocations.

CAT=cat
D=/
EXPP=
EXP=./
SHELL=/bin/sh
SH=$(SHELL)
SHP=$(SH) $(EXP)

# Define generic commands.

CP_=cp
RM_=rm -f
RMN_=rm -f

# Define the arguments for genconf.

CONFILES=-p "%s&s&&" -pl "&-l%s&s&&" -pL "&-L%s&s&&" -ol $(ld_tr)

# Define the compilation rules and flags.

CCFLAGS=$(GENOPT) $(CFLAGS)

.c.o: $(AK)
	$(CCC) $*.c

CCCF=$(CCC)
CCD=$(CCC)
CCINT=$(CCC)

BEGINFILES=
CCBEGIN=$(CCC) *.c

# Patch a couple of PC-specific things that aren't relevant to Unix builds,
# but that cause `make' to produce warnings.

BGIDIR=***UNUSED***
PCFBASM=
