#!/usr/bin/tclsh

# Run some or all of a Genoa test suite, optionally checking for memory leaks.
# Command line syntax:
#	suite (--[no-]check | --[no-]print | --[no-]together | -<switch> |
#	   <dirname>[,<filename>] | <filename>)*

proc pcl_args {print} {
    if $print {
	return [list -Z@? -sDEVICE=ljet4 -r600 -sOutputFile=t.%03d.lj -dNOPAUSE -dMaxBitmap=500000 -dBufferSpace=500000]
    } else {
	return [list -Z:@? -sDEVICE=pbmraw -r600 -sOutputFile=/dev/null -dNOPAUSE]
    }
}

proc pcl_xe {file} {
    if {[string first xl/ $file] >= 0} {return pclxl} else {return pcl5}
}

proc catch_exec {command} {
    puts $command; flush stdout
    if [catch [concat exec $command] msg] {
	puts "Non-zero exit code from command:"
	puts $command
	puts $msg
    }
}

set __check 0
set __print 0
set __together 0
proc suite {files switches} {
    global __check __print __together
    set pcl_args [pcl_args $__print]
    if {!$__check && $__together} {
	set max_files 240
	set max_files1 [expr $max_files - 1]
	set pcl_xe [pcl_xe [lindex $files 0]]
	while {[llength $files] > $max_files} {
	    catch_exec [concat time $pcl_xe $pcl_args $switches [lrange $files 0 $max_files1] >&@stdout]
	    set files [lreplace $files 0 $max_files1]
	}
	catch_exec [concat time $pcl_xe $pcl_args $switches $files >&@stdout]
    } else {
	foreach f $files {
	    set pcl_xe [pcl_xe $f]
	    if $__check {
		puts $f
		flush stdout
		catch_exec [concat $pcl_xe -ZA $pcl_args $switches $f >t]
		set output $f.leak
		regsub -all / $output - output
		exec leaks <t >$output
	    } else {
		catch_exec [concat time $pcl_xe $pcl_args $switches $f >&@stdout]
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
    return [set list($dir) [lsort -command compare_file_size [glob $dir/*.bin]]]
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
	} elseif [regexp {^--(.*)$} $file all var] {
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
