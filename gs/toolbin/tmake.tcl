#!/bin/sh
# hack to restart using tclsh \
exec tclsh "$0" "$@"

# Copyright (C) 1999, 2000 Aladdin Enterprises, Menlo Park, CA.
# All rights reserved.
# $Id$

# This file is intended to be a drop-in replacement for a large and
# useful subset of 'make'.  It compiles makefiles into Tcl scripts
# (lazily) and then executes the scripts.  It requires makefiles to be
# well-behaved:
#
#	- If a rule body modifies a file, then either that file is a
#	target of the rule, or the file is not a target or dependent
#	of any rule.
#
#	- If a rule body reads a file, then either that file is a
#	dependent of the rule, or the file is not a target of any rule.
#
#	- No target is the target of more than one rule.

# Define the backward-compatibility version of this file.
set TMAKE_VERSION 104

#****** -j doesn't work yet ******#

# The following variables are recognized when the script is executed:
#	DEBUG - print a debugging trace
#	DRYRUN - just print commands, don't execute them
#	IGNORE_ERRORS - ignore errors in rule bodies
#	KEEP_GOING - continue past errors, but don't build anything affected
#	MAKEFLAGS - flags for recursive invocations
#	MAX_JOBS - maximum number of concurrent rule executions
#	MAX_LOAD - maximum load for parallel execution
#	SILENT - don't print commands
#	WARN_REDEFINED - warn about redefined variables
#	WARN_MULTIPLE - warn about variables defined more than once,
#	  even if the definitions are identical
#	WARN_UNDEFINED - warn about undefined variables

set FLAGS [list\
    DEBUG DRYRUN IGNORE_ERRORS KEEP_GOING SILENT\
	    WARN_MULTIPLE WARN_REDEFINED WARN_UNDEFINED\
]
set GLOBALS "$FLAGS\
    MAKEFLAGS MAX_JOBS MAX_LOAD\
"
proc init_globals {} {
    global FLAGS GLOBALS

    foreach v $GLOBALS {global $v}
    foreach v $FLAGS {set $v 0}
    set MAKEFLAGS ""
    set MAX_JOBS 1
    set MAX_LOAD 99999
}

# ================ Runtime support ================ #

# Patch in case we're running a pre-8.0 tcl.
if {[info command clock] == ""} {
    proc clock {ignore} {exec date +%s}
}

# Replace the following on systems that don't have /proc
# (don't ask me how!).
proc proc_loadavg {} {
    set fid [open /dev/proc]
    set load [lindex [read $fid] 0]
    close $fid
    return $load
}
proc proc_exists {pid} {
    return [file exists /proc/$pid]
}

proc init_runtime {} {
    global DEBUG I

    set I 0
    if {$DEBUG} {
	proc ifdebug {script} {uplevel $script}
    } {
	proc ifdebug {script} {}
    }
    V MAKELEVEL 0 implicit
}

rename unknown old_unknown
proc unknown_ {var} {
    global WARN_UNDEFINED

    if {$WARN_UNDEFINED} {
	puts "*** Warning: $var is not defined"
    }
    V $var "" default
    return 1
}
proc unknown@ {var} {
    if {[catch {tset $var [file mtime $var]}]} {
	global N TARGET_FAILED

	set tlist "$var"
	set i -1
	puts "*** No rule to make target $var"
	catch {while {1} {
	    set cmd [info level $i]
	    if {[lindex $cmd 0] == "target"} {
		set t [lindex $cmd 1]
		if {[info exists N($t)]} {
		    set at "$N($t):"
		} {
		    set at ""
		}
		puts "\trequired by ${at}$t"
	    }
	    incr i -1
	}}
	puts "*** Can't continue."
	set TARGET_FAILED $var
	return -errorcode error
    }
    return 1
}
proc unknown {cmd args} {
    if {[regexp {^([_@])(.*)$} $cmd skip 1st var]} {
	if {$args == "" && [unknown$1st $var]} {return [$cmd]}
    }
    eval old_unknown [concat [list $cmd] $args]
}
proc var_value {var} {
    # Skip over the "return"
    set value [info body _$var]
    if {[regexp "^proc _$var {} \\\[list return (.*)\\\];" $value skip value]} {
    } else {
	regexp {^return (.*)$} $value skip value
    }
    return $value
}
proc redefined {var value lnum} {
    global WARN_MULTIPLE WARN_REDEFINED

    if {$WARN_MULTIPLE || ($WARN_REDEFINED && $value != [var_value $var])} {
	global _

	set old_lnum $_($var)
	if {!(($old_lnum == "default" || $old_lnum == "implicit") && $lnum == "command-line")} {
	    set old_value [var_value $var]
	    puts "*** Warning: $var redefined from $old_value ($old_lnum) to $value ($lnum)"
	}
    }
}
proc V {var value lnum} {
    global _

    if {![info exists _($var)]} {
	ifdebug {puts "$var=$value"}
	set _($var) $lnum
	proc _$var {} [list return $value]
    } elseif {[set old $_($var)] == "default"} {
	unset _($var)
	V $var $value $lnum
    } elseif {$old != "command-line"} {
	redefined $var $value $lnum
	unset _($var)
	V $var $value $lnum
    }
}
proc P {var vexpr lnum} {
    global _

    if {![info exists _($var)]} {
	ifdebug {puts "$var=$vexpr"}
	set _($var) $lnum
	proc _$var {} "proc _$var {} \[list return $vexpr\];_$var"
    } elseif {[set old $_($var)] == "default"} {
	unset _($var)
	P $var $vexpr $lnum
    } elseif {$old != "command-line"} {
	redefined $var $vexpr $lnum
	unset _($var)
	P $var $vexpr $lnum
    }
}

# Record the very first target as the default target.
proc R {tl dl body lnum} {
    global TARGETS

    if {$TARGETS == ""} {
	lappend TARGETS [lindex $tl 0]
    }
    proc R {tl dl body lnum} {
	global C I N T

	set C([incr I]) $body
	foreach t [set T($I) $tl] {
	    set N($t) $lnum
	    proc @$t {} [list target $t $dl $I]
	}
    }
    R $tl $dl $body $lnum
}
proc tset {p t} {
    proc @$p {} [list return $t]
    ifdebug {puts "ftime($p) <- $t"}
}
proc reap_jobs {} {
    global JOBS

    set jobs {}
    foreach j $JOBS {
	if {[proc_exists [lindex $j 0]]} {
	    lappend jobs $j
	}
    }
    set JOBS $jobs
}
proc shell_exec {cmds} {
    global JOBS MAX_JOBS MAX_LOAD

    set args [list sh -c - $cmds <@ stdin >@ stdout 2>@ stderr]
    if {$MAX_JOBS <= 1} {
	return [eval exec $args]
    }
    while {[llength $JOBS] > 0 &&
	    ([llength $JOBS] > $MAX_JOBS || [proc_loadavg] >= $MAX_LOAD)} {
	# There doesn't seem to be any standard way of either yielding
	# the CPU, or sleeping for less than 1 second....
	reap_jobs
    }
    lappend JOBS [eval exec $args &]
}
proc rexec {i} {
    global C T DRYRUN IGNORE_ERRORS SILENT

    set cmds [eval $C($i)]
    set ok 1
    if {$DRYRUN} {
	foreach c $cmds {
	    if {!$SILENT || ![regexp {^@} $c]} {puts $c}
	}
	flush stdout
    } else {
	set status 0
	foreach c $cmds {
	    if {!([regsub {^@} $c "" c] || $SILENT)} {puts $c}
	    set ignore [regsub {^-} $c "" c]
	    if {![regexp {[][(){}*?!$|;&<>'"\=]} $c]} {
		# We could execute these more efficiently, if we knew how
		# to resolve the command name!
		set status [catch {shell_exec $c}]
	    } else {
		set status [catch {shell_exec $c}]
	    }
	    if {$status != 0 && !($ignore || $IGNORE_ERRORS)} {break}
	}
	flush stdout
	if {$status} {
	    global errorCode IGNORE_ERRORS KEEP_GOING

	    set info $errorCode
	    set level [_MAKELEVEL]
	    if {$level == 0} {set level ""} {set level "\[$level\]"}
	    set code 255
	    catch {
		if {[lindex $info 0] == "CHILDSTATUS"} {
		    set code [lindex $info 2]
		}
	    }
	    puts "tmake$level: *** \[$T($i)\] Error $code"
	    if {!$IGNORE_ERRORS} {
		if {!$KEEP_GOING} {exit $code}
		set ok 0
	    }
	}
    }
    # Set the last mod time of dummy targets to -infinity, so that they
    # won't force their dependents to rebuild.
    foreach t $T($i) {
	if {[file exists $t]} {
	    tset $t [file mtime $t]
	} {
	    tset $t -0x80000000
	}
    }
    return $ok
}
proc target {t dl i} {
    if {[catch {set mt [file mtime $t]}]} {
	ifdebug {puts "no ttime($t)"}
	foreach d $dl {@$d}
	rexec $i
	return [@$t]
    }
    ifdebug {puts "ttime($t)=$mt"}
    set do 0
	# The 'functional' interpretation of dependency would allow us
	# to stop as soon as we reach the first dependent that is newer
	# than the target, but all 'make' programs build all dependents,
	# and some 'operational' rules depend on this.
    foreach d $dl {
	# For safety, the following test should be a >= rather than a >,
	# but this causes excessive unnecessary rebuilding because of
	# rules whose bodies take <1 second to execute.
	if {[@$d] > $mt} {
	    ifdebug {puts "time($d)=[@$d] > ttime($t)=$mt"}
	    set do 1
	}
    }
    if {$do} {rexec $i; return [@$t]}
    tset $t $mt
    ifdebug {puts "OK: $t"}
    return $mt
}

proc _MAKEFLAGS {} {
    global MAKEFLAGS

    set flags $MAKEFLAGS
    if {[regexp {^[^-]} $flags]} {set flags "-$flags"}
    V MAKEFLAGS $flags implicit
    return $flags
}
proc _MAKELEVEL_1 {} {
    V MAKELEVEL_1 [set level1 [expr [_MAKELEVEL] + 1]] implicit
    return $level1
}
proc tcompile {fname version} {
    global TMAKE_TIME

    set mf $fname
    while {![catch {set mf [file readlink $mf]}]} {}
    set tf ${mf}.tcl
    if {![file exists $tf] || [file mtime $tf] < [file mtime $mf] || [file mtime $tf] < $TMAKE_TIME} {
	puts "Compiling $mf to $tf."
	flush stdout
	mak2tcl $mf $tf
    }
    return $tf
}
proc tsource {fname {version 0}} {
    set tf [tcompile $fname $version]
    uplevel [list source $tf]
}

# ================ Compilation ================ #

# 'Compile' a makefile to a Tcl script.
# Each macro becomes a Tcl procedure prefixed by _.
# This is so we can use Tcl's 'unknown' facility to default macro values
# to the empty string, since Tcl doesn't appear to provide a way to trap
# references to undefined variables.
# Each target or precondition becomes a Tcl procedure prefixed by @.

# ---------------- Utilities ---------------- #

# Convert variable references from $(vname) to [_vname],
# escape characters that need to be quoted within "",
# and surround the result with "".
proc quote {defn {refsvar ""}} {
    set orig $defn
    set fixed ""
    set refs {}
    while {[regexp {^(([^$]|\$[^$(])*)\$(\$|\(([^)]*)\))(.*)$} $orig skip pre skip2 dollar var orig]} {
	regsub -all {([][\"$])} $pre {\\\1} pre
	if {$dollar == "\$"} {
	    append fixed "$pre\\\$"
	} else {
	    append fixed "$pre\[_$var\]"
	}
	lappend refs $var
    }
    regsub -all {([][\"$])} $orig {\\\1} orig
    append fixed $orig
    if {[string match {*[ \\]*} $fixed] || $fixed == ""} {
	return "\"$fixed\""
    }
    if {$refsvar != ""} {
	upvar $refsvar rv
	set rv $refs
    }
    return $fixed
}

# ---------------- Writing ---------------- #

# Write the boilerplate at the beginning of the converted file.
proc write_header {out fname} {
    global TMAKE_VERSION

    puts $out {#!/bin/tcl}
    puts $out "# File $fname created [exec date] by tmake ${TMAKE_VERSION}."
}

# Write the definition of a macro.
proc write_macro {out var defn linenum} {
    puts $out "P $var {[quote $defn]} [list $linenum]"
}

# Write an 'include'.
proc write_include {out fname} {
    global TMAKE_VERSION

    puts $out "tsource [quote $fname] $TMAKE_VERSION"
}

# Write a rule.
proc write_rule {out targets deps commands linenum} {
	# Convert all uses of 'make' or $(MAKE) in rule bodies to tmake.
    set body list
    foreach c $commands {
	regsub {^(make|\$\(MAKE\)) } $c {tmake $(MAKEFLAGS) MAKELEVEL=$(MAKELEVEL_1) } c
	append body " [quote $c]"
    }
    puts $out "R [quote $targets] [quote [string trim $deps]] [list $body] [list $linenum]"
}

# ---------------- Top level ---------------- #

proc lgets {in lvar lnvar} {
    upvar $lvar line $lnvar linenum
    set line ""
    set len [gets $in line]
    if {$len < 0} {return $len}
    incr linenum
    while {[regsub {\\$} $line {} line]} {
	if {[gets $in l] < 0} {break}
	incr linenum
	append line $l
    }
    return [string length $line]
}

proc mak2tcl {inname {outname ""}} {
    global =

    catch {unset =}
    set in [open $inname]
    if {$outname == ""} {
	set out stdout
    } {
	set out [open $outname w]
    }
    write_header $out $outname
    set linenum 1
    for {set lnfirst $linenum} {[lgets $in line linenum] >= 0} {set lnfirst $linenum} {
	if {$line == ""} {continue}
	if {[string index $line 0] == "#"} {continue}
	if {[regexp {^([0-9A-Za-z_]+)[ ]*=[ ]*(.*)[ ]*$} $line skip var defn]} {
	    write_macro $out $var $defn ${inname}:$lnfirst
	    continue
	}
	if {[regexp {^([^:]+):(.*)$} $line skip targets deps]} {
	    set commands {}
	    while {[lgets $in line linenum] > 0} {
		regsub {^[	]} $line {} line
		lappend commands $line
	    }
	    write_rule $out $targets $deps $commands ${inname}:$lnfirst
	    continue
	}
	if {[regexp {^(!|)include[ ]+("|)([^ "]*)("|)$} $line skip skip2 skip3 fname]} {
	    write_include $out $fname
	    continue
	}
	# Recognize some GNU constructs
	if {[regexp {^unexport } $line]} {continue}
	puts "****Not recognized: $line"
    }
    if {$out != "stdout"} {
	close $out
    }
    close $in
}

# ================ Command line processing ================ #

proc tmake_args {args} {
    global GLOBALS COMPILE DEFINES JOBS MAKEFILE TARGETS

    foreach v $GLOBALS {global $v}
    set argv $args
    while {[llength $argv] > 0} {
	set n 0
	set copy 1
	set arg [lindex $argv 0]
	switch -glob -- $arg {
	    # -C is not implemented; set copy 0
	    --compile-only {set COMPILE 1}
	    -d {set DEBUG 1}
	    -f {set MAKEFILE [lindex $argv 1]; set n 1; set copy 0}
	    -i {set IGNORE_ERRORS 1}
	    -j {
		if {[llength $argv] > 1 && [regexp {^[0-9]+$} [lindex $argv 1]]} {
		    set MAX_JOBS [lindex $argv 1]; set n 1
		} else {
		    set MAX_JOBS 99999
		}
	    }
	    -k {set KEEP_GOING 1}
	    -l {set MAX_LOAD [lindex $argv 1]; set n 1}
	    # -m is ignored for compatibility with GNU make;
	    # also, because MAKEFLAGS omits the initial '-', we need a
	    # dummy switch in case there are variable definitions (!).
	    -m {set copy 0}
	    -n {set DRYRUN 1}
	    -s {set SILENT 1}
	    --warn-multiply-defined-variables {set WARN_MULTIPLE 1}
	    --warn-redefined-variables {set WARN_REDEFINED 1}
	    --warn-undefined-variables {set WARN_UNDEFINED 1}
	    -* {
		puts "Unknown option: $arg"
		puts {Usage: tmake (<option> | <var>=<value> | <target>)*}
		puts {Options:}
		puts {	--compile-only -d -i -k -n -s}
		puts {	--warn-multiply-defined-variables --warn-redefined-variables}
		puts {	--warn-undefined-variables}
		puts {	-f <file> -j <jobs> -l <load>}
		exit
	    }
	    *=* {
		regexp {^([^=]*)=(.*)$} $arg skip lhs rhs
		lappend DEFINES [list $lhs $rhs]
		set copy 0
	    }
	    default {
		lappend TARGETS $arg
		set copy 0
	    }
	}
	if $copy {lappend MAKEFLAGS [lrange $argv 0 $n]}
	set argv [lreplace $argv 0 $n]
    }
}
proc tmake {args} {
    global argv0
    global GLOBALS COMPILE DEFINES JOBS MAKEFILE TARGETS
    global TMAKE_TIME TMAKE_VERSION

    set TMAKE_TIME [file mtime $argv0]
    foreach v $GLOBALS {global $v}
    init_globals
    set MAKEFILE makefile
    set TARGETS ""
    set DEFINES [list]
    set COMPILE 0
    set JOBS {}
    eval tmake_args $args
    # POSIX requires the following nonsense:
    regsub {^-([^-])} $MAKEFLAGS {\1} MAKEFLAGS
    if {$MAKEFLAGS == ""} {set MAKEFLAGS m}
    foreach d $DEFINES {
	append MAKEFLAGS " [lindex $d 0]='[lindex $d 1]'"
    }
    init_runtime
    foreach d $DEFINES {
	catch {unset _[lindex $d 0]}
	V [lindex $d 0] [lindex $d 1] command-line
	set _($d) 1
    }
    if {$COMPILE} {
	# Just compile the given makefile(s).
	tcompile $MAKEFILE $TMAKE_VERSION
    } {
	# Build the selected targets.
	tsource $MAKEFILE $TMAKE_VERSION
	foreach t $TARGETS {
	    global errorInfo TARGET_FAILED

	    set TARGET_FAILED ""
	    set status [catch "@$t" result]
	    if {$status == 0} {continue}
	    if {$status == 1 && $TARGET_FAILED != ""} {
		exit 1
	    }
	    puts stderr $errorInfo
	    exit $status
	}
    }
}

eval tmake $argv
