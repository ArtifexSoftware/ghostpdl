#!/bin/sh
# hack to restart using tclsh \
exec tclsh "$0" "$@"

# Copyright (C) 2000 Aladdin Enterprises, Menlo Park, CA.
# All rights reserved.
# $Id$

# This file is supposed to help with 3-way merges.
# Currently there is only one function:
#	3way <olddir> <branchdir> <maindir>
# produces a report for merging the olddir/branchdir changes into maindir.

proc filesame {f1 f2} {
    set t /tmp/t
    if {![catch {exec diff $f1 $f2 > $t}]} {
	return 1
    }
    # Two files are the same if they differ only in the copyright date
    # and/or the $Id: line.  These must be the first two differences in
    # the file.
    set same 1
    set in [open $t]
    while {[gets $in l] > 0} {
	if {[regexp {^([0-9]+)c([0-9]+)$} $l skip first last] &&
	    $first == $last &&
	    [gets $in a] > 0 && [gets $in b] > 0 && [gets $in c] > 0 &&
	    $b == "---"
	} {
	    if {[regexp {Copyright} $a]} {
		if {[regexp {Copyright} $c]} {continue}
	    } elseif {[regexp {\$Id$a]} {
		if {[regexp {\$Id$c]} {continue}
	    }
	}
	set same 0
	break
    }
    close $in
    return $same
}

proc 3way {olddir branchdir maindir} {
    foreach f [glob $olddir/*] {
	set t [file tail $f]
	set old($t) $f
	append all($t) o
    }
    foreach f [glob $branchdir/*] {
	set t [file tail $f]
	set branch($t) $f
	append all($t) b
    }
    foreach f [glob $maindir/*] {
	set t [file tail $f]
	set main($t) $f
	append all($t) m
    }
    set present [list]
    foreach f [lsort [array names all]] {
	switch $all($f) {
	    o {puts "$f only in: old"}
	    b {puts "$f only in: branch"}
	    ob {
		puts "$f only in: old branch"
		if {![filesame $old($f) $branch($f)]} {
		    puts "    old and branch differ"
		}
	    }
	    m {}
	    om {
		puts "$f only in: old main"
		if {![filesame $old($f) $main($f)]} {
		    puts "    old and main differ"
		}
	    }
	    bm {
		puts "$f only in: branch main"
		if {![filesame $branch($f) $main($f)]} {
		    puts "    branch and main differ"
		}
	    }
	    obm {
		set o $old($f)
		set b $branch($f)
		set m $main($f)
		if {[filesame $o $b]} {
		    if {![filesame $o $m]} {
			puts "$f: main differs from old & branch"
		    }
		} elseif {[filesame $o $m]} {
		    puts "$f: branch differs from old & main"
		} elseif {[filesame $b $m]} {
		    puts "$f: old differs from branch & main"
		} else {
		    puts "$f: old, branch, main all differ"
		}
	    }
	}
    }
}

foreach dir {doc lib src} {
    3way /gs/post60/pre60/$dir /gs/post60/$dir /gs/$dir
}
