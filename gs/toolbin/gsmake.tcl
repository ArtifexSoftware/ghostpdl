#!/usr/bin/tclsh

# Copyright (C) 1996, 2000 Aladdin Enterprises, Menlo Park, CA.
# All rights reserved.
# $Id$

# gsmake.tcl - Tcl tools for Aladdin's products.  Eventually we hope to:
#   Generate automatically:
#     For compiling and linking:
#	most of lib.mak, int.mak, and devs.mak
#	  (_h defs, most .$(OBJ) rules, most .dev rules)
#	most of devs.mak
#     For fonts:
#	cfonts.mak
#	fonts.mak
#	ccfonts option
# The tools in this file can currently:
#	Check the makefiles for consistency with the #include lists
#	  in .c/.cpp and .h files.
#	Check the makefiles for consistency with the devices defined
#	  in .c/.cpp files.

# Define a set of dependencies that we know about and don't consider
# "unknown".
set KNOWN_DEPS(cc.tr) 1
set KNOWN_DEPS(echogs) 1

# ---------------- Reading makefiles ---------------- #

# The following procedures parse makefiles by reading them into an array
# with the following components:
#	files - a list of all the makefiles read in
#	names - a list of all the defined names, in appearance order,
#		in the form macro= or target:
#	pos:M= - the file and position of the definition of macro M
#	pos:T: - the file and position of the rule for T
#	defn:M - definition of macro M
#	deps:T - dependencies for target T
#	body:T - rule body for T, a list of lines
# Global variables used: CWD

# Initialize the tables.
proc makefile_init {mfarray} {
    catch {uplevel 1 [list unset $mfarray]}
    upvar $mfarray mf
    set mf(files) ""
    set mf(names) ""
}

# Set CWD to the current working directory name (as a list).
proc setcwd {} {
    global CWD
    set CWD [split [exec pwd] /]
    if {[lindex $CWD 0] == ""} {
	set CWD [lrange $CWD 1 end]
    }
}

# Normalize a file name by removing all occurrences of ./,
# all occurrences of <dir>/../,
# and any occurrences of ../<dir>/ that are vacuous relative to CWD.
proc normalize_fname {fname} {
    global CWD
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
				# Now if any ../ are left, they are
				# the first thing in the file name.
    if {[regexp {^((../)+)(.*)$} $name skip up skip2 rest] && $rest != "" && $rest != ".."} {
	set count [expr {[string length $up] / 3}]
	if {$count <= [llength $CWD]} {
	    set tail [lrange $CWD [expr {[llength $CWD] - $count}] end]
	    while {$count > 0 && [regsub "^[lindex $tail 0]/" $rest "" rest]} {
		set up [string range $up 3 end]
		set tail [lrange $tail 1 end]
		incr count -1
	    }
	    set name "$up$rest"
	}
    }
    if {$name == ""} {return .}
    return $name
}

# Find all the macro references in a string (macro, dependencies, or
# rule body).
proc macro_refs {line} {
    regsub -all {[^$]*(\$\(([^)]+)\)|\$|$)} $line {\2 } refs
    return [string trim $refs]
}

# Expand macro definitions in a string.
# nosub decides whether a macro should (not) be expanded.
# defer says what non-expanded macros should become.
proc always_subst {var} {
    return 0
}
proc macro_expand {mfarray str {nosub always_subst} {defer {$(\1)}}} {
    upvar $mfarray mf
    set in $str
    set out ""
    while {[regexp {^([^$]*)\$\(([^)]+)\)(.*)$} $in skip first var rest]} {
	if {[uplevel 1 [concat $nosub $var]] || ![info exists mf(pos:$var=)]} {
	    regsub {^(.*)$} $var $defer var
	    append out "$first$var"
	} else {
	    append out "$first[macro_expand mf $mf(defn:$var) $nosub $defer]"
	}
	set in $rest
    }
    return "$out$in"
}

# Check the references to macros in a definition or rule line.
proc check_refs {mfarray line ref} {
    upvar $mfarray mf
    foreach var [macro_refs $line] {
	if ![info exists mf(defn:$var)] {
	    puts "Warning: $ref refers to undefined macro $var"
	    set mf(defn:$var) ""
	}
    }
}

# Read a line from a makefile, recognizing a trailing \ for continuation.
proc linegets {infile linevar} {
    upvar $linevar line
    if {[gets $infile line] < 0} {return -1}
    while {[regsub {\\$} $line {} line]} {
	gets $infile l
	append line $l
    }
    return [string length $line]
}

# Read a makefile, adding to the tables.
proc read_makefile {mfarray inname} {
    global CWD
    upvar $mfarray mf
    setcwd
    set infile [open $inname]
    lappend mf(files) $inname
    set pos [tell $infile]
    while {[linegets $infile line] >= 0} {
	if [regexp {^([A-Za-z_$][^=:]*)([=:])(.*)$} $line skip lhs eq rhs] {
	    define$eq mf $lhs $rhs $inname:$pos $infile
	} elseif {[regsub {^(!|)include([ ]+)} $line {} file]} {
	    regsub -all {"} $file {} file
	    set file [macro_expand mf $file {string match {"}}]
	    read_makefile mf $file
	}
	set pos [tell $infile]
    }
    close $infile
}
# Define a list (macro).
proc define= {mfarray lhs rhs pos infile} {
    upvar $mfarray mf
    set var [string trim [macro_expand mf $lhs]]
    if [info exists mf(pos:$var=)] {
	puts "Warning: $pos: macro $var redefined"
	puts "         $mf(pos:$var=): previous definition"
    }
    set mf(pos:$var=) $pos
    set mf(defn:$var) $rhs
    check_refs mf $rhs "$pos: Macro $var"
    lappend mf(names) $var=
}
# Define a rule.
proc define: {mfarray lhs rhs pos infile} {
    upvar $mfarray mf
    set lines ""
    while {[linegets $infile line] > 0} {
	if ![regexp {^#} $line] {
	    lappend lines $line
	}
    }
    foreach target [macro_expand mf $lhs] {
	set target [normalize_fname $target]
	set mf(pos:$target:) $pos
	set mf(deps:$target) $rhs
	set mf(body:$target) $lines
	lappend mf(names) $target:
	check_refs mf [join $lines] "$pos: Rule for $target"
    }
}

# ---------------- Reading source code ---------------- #

# Scan a list of .c, .cpp, or .h files and extract references that conform
# to a particular syntax.  We use egrep to find the lines containing
# the references, and regexp to extract the referent.
proc set_references {refarray files grepexp rexp} {
    catch {uplevel 1 [list unset $refarray]}
    upvar $refarray refs
    switch [llength $files] {
	0 {return}
	1 {			;# force grep to output file name
	    close [open _.nul w]
	    lappend files _.nul
	}
    }
    foreach f $files {
	append refs($f) {}		;# ensure existence
    }
    set cmd [list exec -keepnewline grep -E $grepexp]
    append cmd " $files >_.tmp"
    if {![catch $cmd]} {
	set in [open _.tmp]
	set re {^([^:]*):}
	append re $rexp
	while {[gets $in line] > 0} {
	    regexp $re $line skip f i
	    lappend refs($f) $i
	}
	close $in
    }
}

# Scan a list of .c, .cpp, or .h files and extract the "include" lists.
# Set the array incarray to the (sorted) lists.
proc set_includes {incarray files} {
    upvar $incarray incs
    set gre {^#[ 	]*include[ 	]+\"}
    set re {#[\ \	]*include[\ \	]+"([^"]*)"}
    set_references incs $files $gre $re
    foreach f [array names incs] {
	set incs($f) [lsort $incs($f)]
    }
}

# Scan a list of .c or .cpp files and extract any devices they define.
# Set the array devarray to the lists.
proc set_devices {devarray files} {
    upvar $devarray devs
    set gre {gs_[0-9a-zA-Z]+_device.=}
    set re {.*gs_([0-9a-zA-Z]+)_device.=}
    set_references devs $files $gre $re
}

# ---------------- Checking makefiles ---------------- #

# Expand a dependency list by substituting the values of all macro
# references except _h macros.
proc expand_deps {deps mfarray} {
    upvar $mfarray mf
    return [macro_expand mf $deps {regexp {_h$}}]
}

# Check the definition of one .h file.
proc check_h {file incarray mfarray} {
    global KNOWN_DEPS

    upvar $incarray incs $mfarray mf
    set base [file tail $file]
    regsub {\.} $base {_} file_h
    if ![info exists mf(defn:$file_h)] {
	puts "$file_h not defined"
    } else {
	set here {
	    puts "In definition of $file_h at $mf(pos:$file_h=):"
	    set here ""
	}
	foreach i $incs($file) {
	    set inc($i) 1
	}
	foreach d [expand_deps $mf(defn:$file_h) mf] {
	    if [regexp {^\$\((.*)_h\)$} $d skip b] {
		set def($b.h) 1
	    } else {
		set d [normalize_fname $d]
		if {$d == $base || $d == $file} {
		} elseif {[regexp {\.h$} $d]} {
		    set def($d) 1
		} elseif {![info exists KNOWN_DEPS([file tail $d])]} {
		    eval $here
		    puts "    Unknown element $d"
		}
	    }
	}
	foreach i [array names inc] {
	    if ![info exists def($i)] {
		eval $here
		puts "    $base includes $i, missing from definition"
	    }
	}
	foreach d [array names def] {
	    if ![info exists inc($d)] {
		eval $here
		puts "    Definition references $d, not included by $base"
	    }
	}
    }
}

# Check the definition of one .c or .cpp file.
proc check_c {file incarray mfarray} {
    global KNOWN_DEPS

    upvar $incarray incs $mfarray mf
    set base [file tail $file]
    regsub {\.(c|cpp)$} $file {.$(OBJ)} file_obj
    set file_obj [macro_expand mf $file_obj]
    if ![info exists mf(deps:$file_obj)] {
	# Maybe the object files are in another directory.
	set tail [file tail $file_obj]
	set known [concat [array names mf deps:$tail]\
		[array names mf deps:*/$tail]]
	switch [llength $known] {
	    0 {
		puts "No rule for $file_obj"
		return
	    }
	    1 {
		regsub {^deps:} [lindex $known 0] {} file_obj
	    }
	    default {
		puts "Ambiguous matches for $file_obj: $known"
		return
	    }
	}
    }
    set here {
	puts "In rule for $file_obj at $mf(pos:$file_obj:):"
	set here ""
    }
    foreach i $incs($file) {
	set inc($i) 1
    }
    foreach d [expand_deps $mf(deps:$file_obj) mf] {
	if [regexp {^\$\((.*)_h\)$} $d skip b] {
	    set def($b.h) 1
	} else {
	    set d [normalize_fname $d]
	    if {$d == $base || $d == $file} {
	    } elseif {[regexp {\.h$} $d]} {
		set def($d) 1
	    } elseif {![info exists KNOWN_DEPS([file tail $d])]} {
		eval $here
		puts "    Unknown element $d"
	    }
	}
    }
    foreach i [array names inc] {
	if ![info exists def($i)] {
	    eval $here
	    puts "    $base includes $i, missing from dependencies"
	}
    }
    foreach d [array names def] {
	if ![info exists inc($d)] {
	    eval $here
	    puts "    Dependencies include $d, not included by $base"
	}
    }
}

# Check that makefiles agree with device definitions in a .c/.cpp file.
proc check_c_devs {file mfarray devsarray} {
    upvar $mfarray mf $devsarray devs
    set base [file tail $file]
    regsub {\.[cC]([pP][pP]|)$} $file {.$(OBJ)} file_obj
    foreach d $devs($file) {
	if ![info exists mf(pos:$d.dev:)] {
	    puts "No rule for $d.dev, defined in $file"
	} elseif {[lindex $file_obj $mf(deps:$d.dev)] < 0} {
	    puts "$file_obj missing from dependencies of $d.dev"
	}
    }
}

# ---------------- Test code ---------------- #

proc init_files {} {
    global HFILES CFILES CPPFILES ALLFILES

    set HFILES {}
    set CFILES {}
    set CPPFILES {}
    set ALLFILES {}
}
proc add_files {{dir .}} {
    global HFILES CFILES CPPFILES ALLFILES

    if {$dir == "."} {
	set pre ""
    } else {
	set pre $dir/
    }
    set HFILES [concat $HFILES [lsort [glob ${pre}*.h]]]
    set CFILES [concat $CFILES [lsort [glob ${pre}*.c]]]
    set CPPFILES [concat $CPPFILES [lsort [glob -nocomplain ${pre}*.cpp]]]
    llength [set ALLFILES [concat $HFILES $CFILES $CPPFILES]]
}
proc set_files {{dir .}} {
    init_files
    add_files $dir
}
proc get_includes {} {
    global ALLFILES INCS

    puts stdout [time {set_includes INCS $ALLFILES}]
}
proc get_gs_devices {} {
    global DEVS

    puts stdout [time {set_devices DEVS [glob /gs/gdev*.c]}]
}
proc check_headers {} {
    global HFILES INCS MF

    foreach h $HFILES {
	check_h $h INCS MF
    }
}
proc check_code {} {
    global CFILES CPPFILES INCS MF

    foreach c [concat $CFILES $CPPFILES] {
	check_c $c INCS MF
    }
}
proc check_devices {} {
    global DEVS

    foreach c [array names DEVS] {
	check_c_devs $c MF DEVS
    }
}
proc check_makefile {args} {
    global MF INCS

    if {$args == ""} {set args {makefile}}
    init_files
    makefile_init MF
    foreach f $args {
	while {![catch {set f [file readlink $f]}]} {}
	puts "Reading makefile $f"
	set dir [file dirname $f]
	if {![info exists dirs($dir)]} {
	    set dirs($dir) 1
	    puts "Scanning source directory $dir"
	    add_files $dir
	}
	read_makefile MF $f
    }
    get_includes
    check_headers
    check_code
}

init_files
#set_files
#get_includes
#get_gs_devices
#check_headers
#check_code
#check_devices
#make_gs_rules
#check_makefile pcl_ugcc.mak
#check_makefile makefile
if {$argv == [list "check"]} {
    eval check_makefile [lreplace $argv 0 0]
}
