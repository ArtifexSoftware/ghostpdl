#!/usr/bin/tclsh

#    Copyright (C) 1997, 2000 Aladdin Enterprises.  All rights reserved.
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

# This file makes a GNU-licensed GS distribution from an AFPL Ghostscript
# distribution.  Usage:
#	makegnu N.N[N]
# creates /tmp/gnu-gs-N.NN.tar.gz.
#	
# The actual steps involved in making a distribution are:
#
#	- makegnu N.NN
#
#	- FTP /tmp/gnu-gs-N.NN.tar.gz to an appropriate server.
#
#	- rm -f /tmp/gnu-gs*.tar.gz
#
# The script does the following:
#	+ Change the copyright/license notices.
#	+ Move COPYLEFT to COPYING.
#	+ Change the mention of PUBLIC in gs_init.ps to COPYLEFT.
#	+ Append gnudevs.mak to contrib.mak.
#	+ Change the default product names in gscdef.c.
#	+ Change the release date information in whichever of the following
#	exist: gs.1, version.mak, gs-vms.hlp, NEWS, News.htm, gscdef.c,
#	gconf.c.
#	+ Make the actual archive.

# ---------------- Parameters ---------------- #

set COPYRITE /gs/master/copyrite.tcl
set MASTER /gs/master
set NOTICE /tmp/gnu.not
set SUBDIRS {doc examples lib man src toolbin}
set TMPROOT /gs/master/temp
set TMP $TMPROOT/[pid]
set TMPDIR $TMP.dir

# Define the formats for the output file name and directory.
# Parameters are major & minor version #.
set GSNAME_FORMAT "gnu-gs-%s.%s"
set GSDIR_FORMAT "gs%s.%s"

# Other global variables
#FILES()
#TARDIR
#VMAJOR
#VMINOR[0]

# Define the text of the GNU Ghostscript copyright/license notice.
set NOTICE_TEXT {
This file is part of GNU Ghostscript.

GNU Ghostscript is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY.  No author or distributor accepts responsibility
to anyone for the consequences of using it or for whether it serves any
particular purpose or works at all, unless he says so in writing.  Refer
to the GNU General Public License for full details.

Everyone is granted permission to copy, modify and redistribute GNU
Ghostscript, but only under the conditions described in the GNU General
Public License.  A copy of this license is supposed to have been given
to you along with GNU Ghostscript so you can know your rights and
responsibilities.  It should be in a file named COPYING.  Among other
things, the copyright notice and this notice must be preserved on all
copies.

Aladdin Enterprises supports the work of the GNU Project, but is not
affiliated with the Free Software Foundation or the GNU Project.  GNU
Ghostscript, as distributed by Aladdin Enterprises, does not require any
GNU software to build or run it.
}

# ---------------- Utilities ---------------- #

# Prefix a subdirectory to a file name iff this release uses subdirectories.
proc / {sub file} {
    global SUBDIRS

    if {[file isdirectory [lindex $SUBDIRS 0]]} {
	return $sub/$file
    } else {
	return $file
    }
}

# Process a file line-by-line by copying it to a temporary file first.
proc for_lines {linevar fname outvar body} {
    global TMP
    upvar $linevar line $outvar out

    exec cp -p $fname $TMP
    set in [open $TMP]
    set out [open $fname w]
    while {[gets $in line] >= 0} {
	uplevel $body
    }
    close $out
    close $in
}

# Change the notice(s) in a file.
proc change_notices {files {print 0}} {
    global COPYRITE FILES NOTICE TMP TMPDIR

    foreach f $files {
	# copyrite doesn't preserve access attributes.
	set attrs [exec ls -l $f]
	set cwd [pwd]
	cd $TMPDIR
	set log [exec $COPYRITE -n $NOTICE -w $cwd/$f]
	if {$log != ""} {
	    puts $log
	}
	exec mv [file tail $f] $cwd/$f
	cd $cwd
	# Fix the attributes now (awkwardly).
	set mask 0444
	for {set i 1} {$i < 10} {incr i} {
	    if {[string index $attrs $i] != "-"} {
		set mask [expr {$mask | (01000 >> $i)}]
	    }
	}
	exec chmod [format %03o $mask] $f
	unset FILES($f)
	if {$print} {puts "Changed notices in $f"}
    }
}

foreach m {Jan January Feb February Mar March Apr April May Jun June Jul July Aug August Sep September Oct October Nov November Dec December} {
    set MONTHNAME($m) ""
}
# Replace date fields with the current date.
proc subst_date {lvar} {
    global MONTHNAME

    upvar $lvar line
    if {[regexp {^(.*)([0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9])(.*)$} $line skip pre date post]} {
	# yyyymmdd
	set date [exec date +%Y%m%d]
    } elseif {[regexp {^(.*)([0-9]+/[0-9]+/[0-9]+)(.*)$} $line skip pre date post]} {
	# [m]m/[d]d/[yy]yy => yyyy-mm-dd
	set date [exec date +%Y-%m-%d]
    } elseif {[regexp {^(.*[^0-9])([0-9]+ ([A-Z][a-z]+) [0-9]+)(.*)$} $line skip pre date month post] && [info exists MONTHNAME($month)]} {
	# dd monthname [yy]yy
	set date [exec date "+%d %B %Y"]
    } else {
	return 0
    }
    set newline "$pre$date$post"
    puts "Changing date: $line => $newline"
    set line $newline
    return 1
}

# Replace the notice at the end of documentation files.
proc replace_doc_notice {f} {
    set trailer 0
    for_lines l $f out {
	if {[regsub {This file is part of (AFPL|Aladdin) Ghostscript} $l {This file is part of GNU Ghostscript} l]} {
	    set trailer 1
	} elseif {[regsub {<a href="Public.htm">Aladdin Free Public License</a>} $l {<a href="COPYING">GNU General Public License</a>} l]} {
	} elseif {$trailer} {
	    regsub {(AFPL|Aladdin) Ghostscript} $l {GNU Ghostscript} l
	}
	puts $out $l
    }
}

# Note that a file has already been processed (or deleted).
proc skip_file {file {delete 0}} {
    global FILES

    catch {unset FILES($file)}
    if {$delete} {
	puts "Deleting $file"
	exec rm -f $file
    }
}

# ---------------- Main program ---------------- #

### Check arguments and environment.
proc usage {} {
    puts stderr {Usage: makegnu N.N[N]}
    exit 1
}
proc check_arguments {argv} {
    global TARDIR MASTER VMAJOR VMINOR VMINOR0

    set args $argv
    if {[llength $args] != 1 || ![regexp {^([0-9])\.([0-9])([0-9]|)$} [lindex $args 0] skip VMAJOR b c]} {
	usage
    }
    if {"$b$c" == "00"} {
	set VMINOR0 [set VMINOR 0]0
    } else {
	set VMINOR0 [set VMINOR $b$c]
    }
    set TARDIR $MASTER/$VMAJOR$VMINOR0
    if {![file isdir $TARDIR]} {
	puts stderr "****** Archive directory $TARDIR does not exist."
	exit 1
    }
}

### Initialize the working directory.
proc init_directory {} {
    global TMPDIR

    exec rm -rf $TMPDIR
    exec mkdir -p $TMPDIR
    cd $TMPDIR
    puts "Working directory is $TMPDIR"
}

### Unpack the distribution into the gsN.N[N] subdirectory of the current
### directory, and make the subdirectory current.
proc unpack_distribution {} {
    global TARDIR VMAJOR VMINOR

    set archives [list\
	    $TARDIR/ghostscript-$VMAJOR.$VMINOR.tar.gz\
	    $TARDIR/ghostscript-$VMAJOR.${VMINOR}gnu.tar.gz\
    ]
    foreach f $archives {
	if {![file readable $f]} {
	    puts stderr "****** $f does not exist."
	    exit 1
	}
    }
    foreach f $archives {exec tar -xzf $f}
    cd gs$VMAJOR.$VMINOR
}

### Apply patches to the distribution.
proc apply_patches {} {
    global TARDIR

    if {[file isdirectory $TARDIR/patches]} {
	foreach f [glob $TARDIR/patches/*.txt] {
	    puts "Patching with $f"
	    exec patch --no-backup-if-mismatch < $f >@ stdout
	}
    }
}

### Acquire the list of files to be processed.
proc acquire_files {} {
    global FILES SUBDIRS

    catch {unset FILES}
    set files [glob *]
    foreach sub $SUBDIRS {
	set files [concat $files [glob -nocomplain $sub/*]]
    }
    foreach f $files {
	if {[file isfile $f]} {
	    set FILES($f) $f
	} else {
	    puts "$f is a [file type $f]"
	}
    }
    puts "Found [array size FILES] files."
}

### Process files requiring special handling.
proc make_changes {} {
    global FILES NOTICE NOTICE_TEXT VMAJOR VMINOR

    # Write the notice text on a temporary file.

    set notice [open NOTICE w]
    puts -nonewline $notice $NOTICE_TEXT
    close $notice

    # Substitute the GNU license for the AFPL.

    set copying /gs/COPYLEFT
    set dest [/ doc COPYING]
    puts "Copying $copying to $dest"
    exec cp -p $copying $dest
    skip_file $dest 0
    foreach f [list COPYLEFT [/ doc Copying.htm] [/ doc PUBLIC] [/ doc Public.htm]] {
	skip_file $f 1
    }

    # Change the mention of PUBLIC in gs_init.ps to COPYING.

    set gs_init [/ lib gs_init.ps]
    change_notices $gs_init 1
    for_lines l $gs_init out {
	regsub PUBLIC $l COPYING l
	puts $out $l
    }
    set gscdef [/ src gscdef.c]
    if {[file exists $gscdef]} {
	# Change the default product name.
	change_notices $gscdef 1
	set found 0
	for_lines l $gscdef out {
	    if {[regsub {(AFPL|Aladdin) Ghostscript} $l {GNU Ghostscript} l]} {set found 1}
 	    subst_date l
	    puts $out $l
	}
	if {!$found} {
	    puts stderr "****** \"(AFPL|Aladdin) Ghostscript\" not found in $gscdef"
	}
    }

    # Change the dates in the documentation.

    if {[file exists NEWS]} {
	set News NEWS
    } else {
	set News [/ doc News.htm]
    }
    set news $News
    foreach f [list [/ man gs.1] [/ src version.mak] [/ doc gs-vms.hlp] $News] {
	if {[file exists $f]} {
	    change_notices $f 1
	    set found 0
	    for_lines l $f out {
		if {!$found && [subst_date l]} {set found 1}
		puts $out $l
	    }
	    if {[string match *.htm $f]} {
		replace_doc_notice $f
	    }
	}
    }

    # Change the references to AFPL|Aladdin Ghostscript in the documentation
    # trailers.

    foreach f [array names FILES *.htm] {
	change_notices $f 0
	replace_doc_notice $f
    }

    # Get the release number from the NEWS file.

    if {[file exists NEWS]} {
	set greppat ^Version
	set vnpat {^Version ([0-9]+).([0-9]+)(\[[^)]*\)|) \([0-9]+/[0-9]+/[0-9]+\)}
    } else {
	set greppat {^<li><a href="#Version}
	set vnpat {^<li><a href="#Version([0-9]+).([0-9]+)">Version }
    }
    if {[catch {set versions [exec grep $greppat $News]}] ||
        ![regexp $vnpat $versions skip VMAJOR VMINOR]} {
	puts stderr "****** Unable to find version # in $News"
	exit 1
    }
    puts "Version # in $News is $VMAJOR.$VMINOR"

    # Don't copy makefile: make it a link.

    exec rm -f makefile
    set unix_gcc [/ src unix-gcc.mak]
    if {[file exists $unix_gcc]} {
	exec ln -s $unix_gcc makefile
    }

    # Remove the temporary notice file.

    exec rm -f $NOTICE
}

### Change the copyright/license notices in the remaining files.
proc copy_files {} {
    global FILES

    set files [array names FILES]
    change_notices $files
    puts "Changed notices in [llength $files] files."
    set contrib [/ src contrib.mak]
    set gnudevs [/ src gnudevs.mak]
    if {[file readable $contrib] && [file readable $gnudevs]} {
	exec cat $gnudevs >> $contrib
	exec rm -f $gnudevs
	puts "Appended $gnudevs to $contrib."
    }
}

### Make the actual archive.
proc make_archive {} {
    global GSNAME_FORMAT VMAJOR VMINOR

    set tarname [format $GSNAME_FORMAT $VMAJOR $VMINOR].tar.gz
    set dir [file tail [pwd]]
    cd ..
    if {[/ * *] == "*"} {
	set all "$dir/*"
    } else {
	set all "$dir/* $dir/*/*"
    }
    exec sh -c "tar -cvzf /tmp/$tarname $all"
    cd $dir
    exec ls -l /tmp/$tarname >@ stdout
}

### Do the work.
proc doit {argv} {
    global GSDIR_FORMAT TMP TMPDIR TMPROOT

    check_arguments $argv
    init_directory
    unpack_distribution
    apply_patches
    acquire_files
    make_changes
    copy_files
    make_archive
    cd $TMPROOT
    exec rm -rf $TMPDIR
    exec rm -f $TMP
    puts "Done."
    exit
}
if {!$tcl_interactive} {
    doit $argv
}
