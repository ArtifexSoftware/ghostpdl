#    Copyright (C) 1997 Aladdin Enterprises.  All rights reserved.
#    Unauthorized use, copying, and/or distribution prohibited.

# generic.mak
# Definitions common to all subsystem makefiles.

# All of Aladdin's products use a common scheme for their makefiles.
# Products are built out of one or more subsystems.
# Each subsystem has its own source directory, which includes a makefile
# for that subsystem.
# All makefiles have a common structure, shown here in the form of a tree
# in which the edges are created by 'include' statements:
#
#	1. Top-level makefile:
#		<< Parameter and directory definitions >>
#		2. Definitions for top-level products built using a
#		   particular OS/compiler combinations:
#			3. Definitions for a particular compiler;
#			4. Definitions for a particular OS;
#			5. Generic definitions (this file);
#			<< rules >>
#		6. Makefiles for each subsystem;
#		7. Definitions and rules for the product.
#
# The following table shows the dependencies of each makefile on the
# different configuration variables, and also indicates in which directory
# the makefile is normally located:
#
#	Prod.	Subsys.	OS	Compiler	directory
#   1	X	X	X	X		product source
#   2			X	X		common
#   3				X		common
#   4			X			common
#   5						common
#   6		X				subsystem source
#   7	X					product source
#
# Currently, the product and subsystem source directories are the same,
# but there is no logical requirement for this and it may change in
# the future.
#
# Each makefile in the tree must provide certain definitions for makefiles
# that follow it in the traversal (execution) order, since some versions
# of 'make' expand macros eagerly rather than lazily.  Here are the
# definitions that each makefile must provide.  See the individual
# makefiles for more detailed descriptions.
#
#   1	MAKEFILE, <subsys>DIR for each subsystem, TARGET_XE
#   2	(rule for building executable)
#   3	CC_
#   4	D, OBJ, C_, I_, II, _I, O_, XE, CP_, RM_, RMN_
#   5	GS_XE, ECHOGS_XE, SETMOD, ADDMOD
#   6	<subsys>_MAK
#   7	(default rule)
#
# Here are some examples of the 7 different makefile types that may be
# helpful in understanding the structure:
#
#   1	pcl/pcl_ugcc.mak
#   2	common/ugcc_top.mak
#   3	common/gccdefs.mak
#   4	common/unixdefs.mak
#   5	common/generic.mak
#   6	pcl/pcl.mak
#   7	pcl/pcl_top.mak

# Define the Ghostscript executable.
# Currently it always lives in the GLOBJ directory.
GS_XE=$(GLOBJDIR)$(D)gs$(XE)

# Define the echogs executable.
# Currently it always lives in the GLOBJ directory.
ECHOGS_XE=$(GLOBJDIR)$(D)echogs$(XE)

# Define the commands for building module descriptions.
SETMOD=$(ECHOGS_XE) -e .dev -w- -Q-obj
ADDMOD=$(ECHOGS_XE) -e .dev -a-
