#!/usr/bin/tclsh

# Run some or all of a Genoa test suite, optionally checking for memory leaks.
# Command line syntax:
#	suite (--[no-]band | --[no-]check | --[no-]debug | --[no-]print |
#	   --[no-]profile | --[no-]together | -<switch> |
#	   <dirname>[,<filename>] | <filename>)*

set __band 1
set __check 0
set __debug 0
set __print 0
set __profile 0
set __together 0

proc pcl_args {band print} {
    set args [list -Z@:? -r600 -dNOPAUSE]
    if $print {
	lappend args -sDEVICE=ljet4 -sOutputFile=t.%03d.lj
    } else {
	lappend args -sDEVICE=pbmraw -sOutputFile=/dev/null
    }
    if $band {
	lappend args -dMaxBitmap=200000 -dBufferSpace=200000
    }
    return $args
}

proc pcl_xe {file} {
    if {[string first xl $file] >= 0} {return pclxl} else {return pcl5}
}

proc catch_exec {command} {
    global __debug
    puts $command; flush stdout
    if {!$__debug && [catch [concat exec $command] msg]} {
	puts "Non-zero exit code from command:"
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
    global __band __check __print __profile __together
    set pcl_args [pcl_args $__band $__print]
    set files [list]
    if $__together {
	set left $filelist
	set max_files 100
	set max_files1 [expr $max_files - 1]
	set pcl_xe [pcl_xe [lindex $filelist 0]]
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
	set pcl_xe [pcl_xe [lindex $fl 0]]
	set output [output_name $fl]
	if $__check {
	    puts $fl
	    flush stdout
	    catch_exec [concat $pcl_xe -ZA $pcl_args $switches $fl >t]
	    exec leaks <t >${output}.leak
	} else {
	    catch_exec [concat time $pcl_xe $pcl_args $switches $fl | tee ${output}.log >&@stdout]
	    if $__profile {
		exec gprof $pcl_xe >${output}.out
	    }
	}
    }
}

# Define the suites, sorted by increasing file size.
# We compute the sorted lists lazily.
proc compare_file_size {file1 file2} {
    expr [file size $file1] - [file size $file2]
}
proc get_suite {dir} {
    global list
    if [info exists list($dir)] {
	return $list($dir)
    }
    set files [lsort -command compare_file_size [glob $dir/*.bin]]
    if {[lindex $files 0] == "0release.bin"} {
	set files [lrange $files 1 end]
    }
    return [set list($dir) $files]
}

# Run the program.
set switches ""
set files ""
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
	    } else {
		puts "Unknown switch $file"
		exit 1
	    }
	} else {
	    lappend switches $file
	}
    } elseif {[file isdir $file]} {
	if {$files != ""} {suite $files $switches; set files ""}
	suite [get_suite $file] $switches
    } elseif {[regexp {(.*),(.*)$} $file all suite from] && [file isdir $suite]} {
	if {$files != ""} {suite $files $switches; set files ""}
	set sfiles [get_suite $suite]
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
