#!/usr/bin/tclsh
# Copyright (C) 1996 Aladdin Enterprises, Menlo Park, CA. All rights reserved.

# pcl/package.tcl
# Make various packages and disk sets.

# This should be a primitive!
proc lconcat {upvar list} {
    upvar $upvar var
    set var [concat $var $list]
}

# Within each list, files are in this order: documentation, makefiles,
# other scripts, C source code, other source code, other data.
proc glob-list {args} {
    set list ""
    foreach pattern $args {
	if [string first * $pattern] {
	    lconcat list [glob $pattern]
	} else {
	    lappend list $pattern
	}
    }
    return $list
}
proc list-common {dir context} {
    set list [glob-list $dir/README $dir/*pcl.mak $dir/pcllib.mak]
    if {$context != "license"} {lconcat list [glob-list $dir/*.tcl]}
    lconcat list [glob-list $dir/p\[jl\]*.\[ch\] $dir/uni*.h]
    return $list
}
proc list-pcl5-only {dir context} {
    set list [glob-list $dir/NEWS-5 $dir/pcl5.mak]
    if {$context != "license"} {lconcat list [glob-list $dir/*_pcl]}
    lconcat list [glob-list $dir/p\[cg\]*.\[ch\] $dir/rt*.\[ch\]]
    return $list
}
proc list-pcl5 {dir context} {
    return [concat [list-common $dir $context] [list-pcl5-only $dir $context]]
}
proc list-pclxl-only {dir context} {
    set list [glob-list $dir/NEWS-XL $dir/pclxl.mak]
    if {$context != "license"} {lconcat list [glob-list $dir/*_pxl]}
    lconcat list [glob-list $dir/pxasm $dir/px*.bat $dir/px*.\[ch\]\
	    $dir/px*.ps $dir/*.pxs $dir/px*.txt]
    return $list
}
proc list-pclxl {dir context} {
    return [concat [list-common $dir $context] [list-pclxl-only $dir $context]]
}

proc make_tar {file list} {
    eval exec tar -cf - $list 2>@stderr | gzip -c >${file}.tar.gz
    exec ls -l ${file}.tar.gz >@stdout
}
proc make_zip {file list} {
    eval exec zip -q -l ${file}.zip $list >@stdout 2>@stderr
    exec ls -l ${file}.zip >@stdout
}
proc make1 {args listname maker usage} {
    global argv0
    set len [llength $args]
    set i 0
    set context "internal"
    if {$i < $len && [lindex $args $i] == "--license"} {
	set context "license"
	incr i
    }
    if {$len == $i + 1} {
	set dir [file dirname $argv0]
	if {$dir == "/"} {set dir ""}
	$maker [lindex $args $i] [$listname $dir $context]
    } else {
	puts "Usage: $argv0 \[--license\] xxx  =>  $usage"
    }
}

proc command_tar_pcl {args} {make1 $args list-pcl5 make_tar xxx.tar}
proc command_tar_pxl {args} {make1 $args list-pclxl make_tar xxx.tar}
proc command_zip_pcl {args} {make1 $args list-pcl5 make_zip xxx.zip}
proc command_zip_pxl {args} {make1 $args list-pclxl make_zip xxx.zip}

if {$argv0 != "tclsh"} {
    eval command_[file tail $argv0] $argv
}
