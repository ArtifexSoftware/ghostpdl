#!/usr/bin/tclsh

#    Copyright (C) 2000 artofcode LLC.  All rights reserved.
# 
# This file is part of AFPL Ghostscript.
# 
# AFPL Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author or
# distributor accepts any responsibility for the consequences of using it, or
# for whether it serves any particular purpose or works at all, unless he or
# she says so in writing.  Refer to the Aladdin Free Public License (the
# "License") for full details.
# 
# Every copy of AFPL Ghostscript must include a copy of the License, normally
# in a plain ASCII text file named PUBLIC.  The License grants you the right
# to copy, modify and redistribute AFPL Ghostscript, but only under certain
# conditions described in the License.  Among other things, the License
# requires that the copyright notice and this notice be preserved on all
# copies.

# $Id$

# hrefcov.tcl - check that the hrefs in an HTML document mention all of a
# set of files.  The requirement is that the union of all the docfiles
# must somewhere reference all the files directories.  Usage:
set USAGE {Usage:
    hrefcov ([+-]from (<docfile> | -) | [+-]to (<directory> | <file> | -))*
}
# +from or +to adds files; -from or -to removes them.
# After -from or -to, "-" removes ALL directories or files:
# this is the only way to remove the defaults.

# Define the defaults.  These are Ghostscript-specific.
set DEFAULTS [list\
	+from doc/Develop.htm\
	+to lib src\
	-to lib/CVS src/CVS\
	-to src/*.mak.tcl
]

# Global variables:
#   TO(file) is defined for every file in the "to" list
#   TO_DIR(dir) is defined for every directory in the "to" list
#   FROM(file) is defined for every file mentioned in a "from" document
# In both cases, path names are normalized by removing ./ and/or ../
# whenever possible, to produce file names relative to the directory where
# this program is being run.

# Initialize the internal data base.
proc init {} {
    global FROM TO TO_DIR

    catch {unset FROM}
    catch {unset TO}
    catch {unset TO_DIR}
}

# Normalize a file name by removing all occurrences of ./ and
# all occurrences of <dir>/../.
proc normalize_fname {fname} {
    set name $fname
				# Remove a trailing /
    regsub {/$} $name "" name
				# Remove occurrences of ./
    while {[regsub {^\./} $name "" name]} {}
    while {[regsub {/\./} $name / name]} {}
    while {[regsub {/\.$} $name "" name]} {}
    if {$name == ""} {return /}
				# Remove occurrences of <dir>/../
    while {[regsub {(^|/)([^./]|.[^./])[^/]*/../} $name {\1} name]} {}
    if {$name == ""} {return .}
    return $name
}

# Add or remove a file, or all the files in a directory, to/from TO.
proc add_to {to} {
    global TO TO_DIR

    if {[file isfile $to]} {
	set TO($to) 1
    } elseif {[file isdirectory $to]} {
	set TO_DIR($to) 1
	foreach f [glob $to/*] {add_to $f}
    }
}
proc remove_to {to} {
    global TO TO_DIR

    if {[file isfile $to]} {
	catch {unset TO($to)}
    } elseif {[file isdirectory $to]} {
	catch {unset TO_DIR($to)}
	foreach f [glob $to/*] {remove_to $f}
    }
}

# Add or remove all the files mentioned in a document to/from FROM.
# Note that we only add/remove files mentioned as a whole, i.e., without #.
proc for_from {doc proc} {
    set lines ""
    set prefix ""
    regexp {^(.*/)[^/]+$} $doc skip prefix
    catch {set lines [split [exec egrep -i {href="[^#]} $doc] "\n"]}
    set href_exp {href="([^"#]*)"(.*)$}
    foreach line $lines {
	while {[regexp -nocase $href_exp $line skip ref line]} {
	    $proc [normalize_fname $prefix$ref]
	}
    }
}
proc add1_from {from} {global FROM; set FROM($from) 1}
proc add_from {doc} {for_from $doc add1_from}
proc remove1_from {from} {global FROM; catch {unset FROM($from)}}
proc remove_from {doc} {for_from $doc remove1_from}

# Main program.
proc main_args {arglist} {
    foreach arg $arglist {
	switch -- $arg {
	    +from {set do add_from}
	    -from {set do remove_from}
	    +to {set do add_to}
	    -to {set do remove_to}
	    default {
		if {[regexp {[*]} $arg]} {
		    foreach a [glob -nocomplain $arg] {$do $a}
		} else {
		    $do $arg
		}
	    }
	}
    }
}
proc main {argv} {
    global DEFAULTS FROM TO TO_DIR

    init
    main_args $DEFAULTS
    main_args $argv
    set dirs_exp {^$}
    foreach dir [array names TO_DIR] {
	append dirs_exp "|$dir/"
    }
    set list {}
    foreach f [array names TO] {
	if {![info exists FROM($f)]} {lappend list $f}
    }
    if {$list != {}} {
	puts "        ****** Files defined but not referenced ******"
	foreach f [lsort $list] {puts $f}
    }
    set list {}
    foreach f [array names FROM] {
	if {![info exists TO($f)]} {
	    # Only report files that should be in a scanned directory.
	    if {[regexp "(${dirs_exp})\[^/\]+$" $f]} {
		lappend list $f
	    }
	}
    }
    if {$list != {}} {
	puts "        ****** Files referenced but not defined ******"
	foreach f [lsort $list] {puts $f}
    }
}

main $argv
