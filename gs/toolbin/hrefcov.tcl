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

# hrefcov.tcl - check that the hrefs in an HTML document cover all the
# files in one or more directories.  Usage:
set USAGE {Usage:
    hrefcov ([+-]d <directory> | [+-]f <docfile>)*
}
# +d or +f adds to the defaults; -d or -f cancels the defaults and then adds.
# The requirement is that the union of all the docfiles must somewhere
# reference all the files in all the directories, other than CVS control files.

# Define the defaults.  These are Ghostscript-specific.
set DEFAULT_DIRS {lib src}
set DEFAULT_DOCS {doc/Develop.htm}

# Global variables:
#   DEFINED(file) is defined for every file in the directories
#   MENTIONED(file) is defined for every file mentioned in a docfile
# In both cases, path names are normalized by removing ./ and/or ../
# whenever possible, to produce file names relative to the directory where
# this program is being run.

# Initialize the internal data base.
proc init {} {
    global DEFINED MENTIONED

    catch {unset DEFINED}
    catch {unset MENTIONED}
    set DEFINED() 1
    set MENTIONED() 1
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

# Add all the files in a directory to DEFINED.
proc add_dir {dir} {
    global DEFINED

    foreach f [glob $dir/*] {
	if {[file isfile $f]} {
	    set DEFINED($f) 1
	}
    }
}

# Add all the files mentioned in a document to MENTIONED.
# Note that we only add files mentioned as a whole, i.e., without #.
proc add_doc {doc} {
    global MENTIONED

    set lines ""
    set prefix ""
    regexp {^(.*/)[^/]+$} $doc skip prefix
    catch {set lines [split [exec egrep -i {href="[^#]} $doc] "\n"]}
    set href_exp {href="([^"#]*)"(.*)$}
    foreach line $lines {
	while {[regexp -nocase $href_exp $line skip ref line]} {
	    set MENTIONED([normalize_fname $prefix$ref]) 1
	}
    }
}

proc main {argv} {
    global DEFAULT_DIRS DEFAULT_DOCS DEFINED MENTIONED

    set default_dirs 1
    set default_docs 1
    set dirs {}
    set docs {}
    foreach arg $argv {
	switch -- $arg {
	    +d {set which dirs}
	    -d {set default_dirs 0; set which dirs}
	    +f {set which docs}
	    -f {set default_docs 0; set which docs}
	    default {lappend $which $arg}
	}
    }
    if {$default_dirs} {set dirs [concat $dirs $DEFAULT_DIRS]}
    if {$default_docs} {set docs [concat $docs $DEFAULT_DOCS]}
    init
    set dirs_exp {^$}
    foreach dir $dirs {
	add_dir $dir
	append dirs_exp "|$dir/"
    }
    foreach doc $docs {add_doc $doc}
    set list {}
    foreach f [array names DEFINED] {
	if {![info exists MENTIONED($f)]} {lappend list $f}
    }
    if {$list != {}} {
	puts "Files defined but not mentioned:"
	foreach f [lsort $list] {puts "    $f"}
    }
    set list {}
    foreach f [array names MENTIONED] {
	if {![info exists DEFINED($f)]} {
	    # Only report files that should be in a scanned directory.
	    if {[regexp "(${dirs_exp})\[^/\]+$" $f]} {
		lappend list $f
	    }
	}
    }
    if {$list != {}} {
	puts "Files mentioned but not defined:"
	foreach f [lsort $list] {puts "    $f"}
    }
}

main $argv
