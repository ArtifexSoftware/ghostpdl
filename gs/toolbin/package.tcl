#!/usr/bin/tclsh

# Copyright (C) 1996, 2000 Aladdin Enterprises, Menlo Park, CA.
# All rights reserved.
# $Id$

# Make various packages of PCL software.

set binary_extensions\
 {dll exe gif gz ico jpg o obj pcl pfb pfm ppm pxl res tar taz tgz tif trz txz z zip}
set binary_extensions_regexp \([join $binary_extensions |]\)

# Determine whether a file is binary, MS-DOS text (crlf-text),
# or Unix text (lf-text).
proc file_type {file} {
    global binary_extensions_regexp
    if [regexp \\.$binary_extensions_regexp\$ $file] {return binary}
    if [regexp ^/c/ $file] {return crlf-text}
    return lf-text
}

# This should be a primitive!
proc lconcat {upvar list} {
    upvar $upvar var
    set var [concat $var $list]
}

# Within each list, files are in this order: documentation, scripts,
# C source code, other source code, other data.
# Omit t.*, *~, *.orig, and *.mak.tcl, but keep all other files that match
# any of the argument patterns.
proc glob-list {args} {
    set list ""
    foreach pattern $args {
	if {[string first * $pattern] >= 0} {
	    set matches ""
	    foreach f [glob -nocomplain $pattern] {
		if ![regexp {(^|/)t\.|(~|\.orig|\.mak\.tcl)$} $f] {
		    lappend matches $f
		}
	    }
	    lconcat list [lsort $matches]
	} elseif {[file readable $pattern]} {
	    lappend list $pattern
	}
    }
    return $list
}
proc list-default {dir context} {
    return [glob-list $dir/README $dir/NEWS* $dir/*.txt\
	    $dir/*.bat $dir/makefile $dir/*.mak $dir/*.tcl\
	    $dir/*.h $dir/*.c $dir/*.ps]
}
proc list-pcl {dir context} {
    return [glob-list $dir/*.pcl]
}
proc list-pxl {dir context} {
    return [glob-list $dir/*.pxl]
}
proc list-contents {listname dir {context ""}} {
    set list [list-default $dir $context]
    if {[info procs list-$listname] != ""} {
	lconcat list [list-$listname $dir $context]
    }
    return $list
}

proc make.tar.gz {file list} {
    eval exec tar -cf - $list 2>@stderr | gzip -c >${file}
}
proc make.zip {file list} {
	# We have to be careful not to EOL-convert binary data.
    set binary ""
    set text ""
    foreach f $list {
	if {[file_type $f] == "binary"} {
	    lappend binary $f
	} else {
	    lappend text $f
	}
    }
    exec rm -f $file
    if {$text != ""} {
	eval exec zip -q -l $file $text >@stdout 2>@stderr
	if {$binary != ""} {
	    eval exec zip -q -g $file $binary >@stdout 2>@stderr
	}
    } else {
	eval exec zip -q $file $binary >@stdout 2>@stderr
    }
}
proc do_package {file {context ""}} {
    if ![regexp {^([A-Za-z]+)(.*/)*[^.]*(\.[A-Za-z.]+)$} $file skip pre skip2 extn] {
	return 0
    }
    if {[info procs make$extn] == ""} {return 0}
    set dir $pre
    if ![file isdir $dir] {
	set pre ${pre}l
	set dir ${dir}l
	if ![file isdir $dir] {return 0}
    }
    make$extn $file [list-contents $pre $dir $context]
    return 1
}
proc command_package {argv0 argv} {
    set len [llength $argv]
    set i 0
    set context "internal"
    if {$i < $len && [lindex $argv $i] == "--license"} {
	set context "license"
	incr i
    }
    if {$len == $i + 1 && [do_package [set file [lindex $argv $i]] $context]} {
	exec ls -l ${file} >@stdout
    } else {
	puts {Usage: package [--license] {common|pc[l]|pl|px[l]}xxxxx{.tar.gz|.zip}}
    }
}

if {$argv0 != "tclsh"} {
    command_[file tail $argv0] $argv0 $argv
}
