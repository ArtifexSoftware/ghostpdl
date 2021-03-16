#!/usr/bin/tclsh
# Copyright (C) 2001-2021 Artifex Software, Inc.
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

# Run some or all of a Genoa test suite, optionally checking for memory leaks.
# Command line syntax:
#	suite (--[no-]band | --[no-]check | --[no-]debug |
#	   --[no-]print[=<device>] | --[no-]profile | --[no-]sort |
#	   --[no-]together |
#	   -<switch> |
#	   <dirname>[,<filename>] | <filename>)*

# Note: test failure is typically indicated by one or more of the following:
#	exit status
#	Error:
#	fail/FAIL
#	wrong:
#	*not* Final time

set __band 1			;# use banding
set __check 0			;# check for memory leaks
set __debug 0			;# don't actually execute the test
set __display 0			;# use display instead of printer
set __print 0			;# don't discard output, print for LJ4
				;# (or device if value != 1)
set __profile 0			;# assume -pg executable, profile execution
set __sort 1			;# do in order of increasing file size
set __together 0		;# run all files together, not individually

proc test_args {band display print switches} {
    set args [list -K40000 -Z@:? -dNOPAUSE -dBATCH]
    if {$display} {
				# Use the default device
    } elseif {$print == "0"} {
	lappend args -r600 -sDEVICE=pbmraw -sOutputFile=/dev/null
    } else {
	if {$print == "1"} {set print ljet4}
	lappend args -r600 -sDEVICE=$print -sOutputFile=t.[exec date +%H%M%S].%03d.$print
    }
    if $band {
	lappend args -dMaxBitmap=200000 -dBufferSpace=200000
    }
    return [concat $args $switches {-c false 0 startjob pop -f}]
}

proc test_xe {file} {
    if {[string first xl $file] >= 0} {return pclxl}
    if {[string first pcl $file] >= 0} {return pcl5}
    return gs
}

proc catch_exec {command} {
    global __debug

    puts $command; flush stdout
    if {!$__debug && [catch [concat exec $command] msg]} {
	puts "*** Non-zero exit code from command:"
	puts $command
	puts $msg
    }
}

proc output_name {fl} {
    if {[llength $fl] == 1} {
	set output [lindex $fl 0]
    } else {
	set output "[lindex $fl 0]-[lindex $fl end]"
    }
    regsub -all / $output - output
    return $output
}

proc suite {filelist switches} {
    global __band __check __display __print __profile __together

    set files [list]
    if $__together {
	set left $filelist
	set max_files 100
	set max_files1 [expr $max_files - 1]
	set test_xe [test_xe [lindex $filelist 0]]
	while {[llength $left] > $max_files} {
	    lappend files [lrange $left 0 $max_files1]
	    set left [lreplace $left 0 $max_files1]
	}
	if {$left != {}} {
	    lappend files $left
	}
    } else {
	foreach f $filelist {lappend files [list $f]}
    }
    foreach fl $files {
	set test_args [test_args $__band $__display $__print $switches]
	set test_xe [test_xe [lindex $fl 0]]
	set output [output_name $fl]
	if {$test_xe != "gs"} {
	    set pre [list]
	    set post $fl
	} elseif {[llength $fl] == 1} {
	    set pre [list]
	    set post [list - < [lindex $fl 0]]
	} else {
	    set pre [concat cat $fl |]
	    set post [list -]
	}
	if $__check {
	    puts $fl
	    flush stdout
	    catch_exec [concat $pre $test_xe -ZA $test_args $post > t]
	    exec leaks < t > ${output}.leak
	} else {
	    set log ${output}.log
	    set command [concat $pre time $test_xe $test_args $post | tee -a $log >@ stdout 2>@ stderr]
	    exec echo $command > $log
	    catch_exec $command
	    if $__profile {
		exec gprof $test_xe > ${output}.out
	    }
	}
    }
}

# Define the suites, sorted by increasing file size.
# We compute the sorted lists lazily.
proc compare_file_size {file1 file2} {
    expr [file size $file1] - [file size $file2]
}
proc get_suite {dir {sort 1}} {
    global list

    set files [glob -nocomplain $dir/*.bin]
    if {$files == {}} {
	set files [glob -nocomplain $dir/*.ps]
    }
    if !$sort {
	return [lsort $files]
    }
    if [info exists list($dir)] {
	return $list($dir)
    }
    set files [lsort -command compare_file_size $files]
    if {[lindex $files 0] == "0release.bin"} {
	set files [lrange $files 1 end]
    }
    return [set list($dir) $files]
}

# Run the program.
set switches ""
set files ""
puts "-- [exec date]"
puts "-- $argv0 $argv"
foreach file $argv {
    if [regexp {^-} $file] {
	if {$files != ""} {suite $files $switches; set files ""}
	if [regexp {^--no-(.*)$} $file all var] {
	    if [info exists __$var] {
		set __$var 0
	    } else {
		puts "Unknown switch $file"
		exit 1
	    }
	} elseif {[regexp {^--(.*)$} $file all var]} {
	    if [info exists __$var] {
		set __$var 1
	    } elseif {[regexp {^(print)=(.*)$} $var all var value]} {
		set __$var $value
	    } else {
		puts "Unknown switch $file"
		exit 1
	    }
	} else {
	    lappend switches $file
	}
    } elseif {[file isdir $file]} {
	if {$files != ""} {suite $files $switches; set files ""}
	suite [get_suite $file $__sort] $switches
    } elseif {[regexp {(.*),(.*)$} $file all suite from] && [file isdir $suite]} {
	if {$files != ""} {suite $files $switches; set files ""}
	set sfiles [get_suite $suite $__sort]
	set first [lsearch $sfiles $suite/$from]
	if {$first < 0} {
	    puts "No such file: $suite/$from"
	} else {
	    suite [lreplace $sfiles 0 [expr $first - 1]] $switches
	}
    } else {
	lappend files $file
    }
}
if {$files != ""} {suite $files $switches; set files ""}
