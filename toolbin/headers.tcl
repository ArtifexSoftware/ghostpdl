#!/usr/bin/tclsh

# Copyright (C) 2001-2019 Artifex Software, Inc.
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


# Make TOC from 1-line module headers in source files.
# Usage: headers.tcl
# Reads src/*.[ch], writes headers.txt

##############################################################################
#
#	get_header	Scan file for the initial comment block skipping
#			Copyright and optional RCS keyword lines
#
#	args:	FInName	Input file name
#		FOUTvar	Output File ID
#
#	return:	integer 0	normal header processed
#			1	No Header Found
#
proc get_header { FInName FOUT } {

	# Maximum lines allowed in the copyright or comment block
    set limit 30
    set FIN [open $FInName r]

    # Print out the file name -- pad filename to a specific length
    puts -nonewline $FOUT [format {%-16s} "$FInName: "]

    # Skip initial comment block (Copyright block) by scanning for
    # terminating '*/'
    set i 0
    while { 1 } {
       incr i
       set s [gets $FIN Line]
       if { $s == -1 || $i == $limit } then {
	   puts stderr "Fail to find end of Copyright block in: $FInName"
	   puts $FOUT "	**** PROBLEM PROCESSING COPYRIGHT BLOCK ****"
	   close $FIN
           return 1
       }
       if { [regexp {\*/} $Line] } then {
	   #DEBUG puts "Found end of Copyright at line $i"
           break
       }
    }

    # Scan for the start of the header line/block -- ignore RCS keyword lines
    set i 0
    while { 1 } {
        incr i
        set s [gets $FIN Line]
        if { $s == -1 || $i == $limit } then {
	    puts stderr "Fail to find header after Copyright in: $FInName"
	    puts $FOUT "	**** NO HEADER FOUND AFTER COPYRIGHT BLOCK ****"
	    close $FIN
            return 1
	    break
        }
	# Ignore RCS keyword comment lines
        if { [regexp {\$.+\$} $Line] } then {
            continue
        }
        if { [regexp {^[	 ]*/[*/]} $Line] } then {
            break
        }
    }

    # Process header lines -- stripping off leading and trailing delimiters
    # including '/*', '*', and  '*/'
    set f 0
    for { set i 0 } { $i < $limit } { incr i } {
        regsub {/[*/]} $Line {} Lx
        regsub {\*/} $Lx {} Lx
        regsub {^[ 	]*(\*|)} $Lx {} Lx
	set blank [expr [regexp {^[ 	]$} $Lx] || [string length $Lx] == 0]
	if { $f == 0 && $blank == 1 } then {
	    #DEBUG puts "Ignoring leading blank header line #$linecount"
	} else {
	    if { $blank == 1 } then {
		break
	    }
	    incr f
	    if { $f > 1 } then {
	        set Lx "                   $Lx"
	    }
	    puts $FOUT $Lx
	    if { [regexp {\*/} $Line] } then {
		break
	    }
	}
        set s [gets $FIN Line]
    }

    if { $i == $limit } then {
        puts stderr "Comment lines ignored in file: $FInName"
    }

    close $FIN
    return 0
}

######################### Begin processing #####################################

# overwrite the file without asking
set FOUT [open headers.txt w]

set NoHeaderCount 0

foreach Group {a b c d e f g[0-9a-c] gd g[e-z] h i j k l m n o p q r s t u v w x y z} {
    set List [concat [glob -nocomplain src/$Group*.c] [glob -nocomplain src/$Group*.h]]
    if { $List != {} } {
        puts "Processing Group: $Group"
        puts $FOUT "     ------------------- $Group ------------------"
        foreach File [lsort $List] {
	    set s [get_header $File $FOUT]
            incr NoHeaderCount $s
        }
    }
}
puts $FOUT "Number of modules without descriptions: $NoHeaderCount"
close $FOUT
puts "Number of modules without descriptions: $NoHeaderCount"
