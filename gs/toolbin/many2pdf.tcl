#!/usr/bin/tclsh

#    Copyright (C) 2000 Aladdin Enterprises.  All rights reserved.
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

# Batch-convert a list of files to PDF.  If restarted, only convert files
# that have not been converted yet.  Detect excessive running times and
# conversion failures.

# Define the file containing the list of input file names.
set LIST_FILE_NAME /gs/show.lst
# Define the directory where the output will be stored.
set PDF_DIR /gs/tmp-pdf

proc maxwaitfor {filesize} {
    return [expr $filesize / 5000 + 30]
}

set lst [open $LIST_FILE_NAME]
set pid [pid]
set skip 0
while {[gets $lst ps] > 0} {
    set pdf $PDF_DIR/[file rootname [file tail $ps]].pdf
    if {[file exists $pdf] || [file exists ${pdf}.bad]} {
	puts -nonewline "."
	set skip 1
    } else {
	if {$skip} {
	    puts ""
	    set skip 0
	}
	puts "$ps --> $pdf"
	flush stdout
	catch {exec rm -f $pdf ${pdf}.bad ${pdf}.tmp}
	if {![file exists $ps]} {
	    puts "****** $ps FAILED, DOES NOT EXIST ******"
	    continue
	}
	set script /tmp/${pid}.tcl
	set status /tmp/${pid}.out
	set tmp [open $script w]
	puts $tmp "\
	set tmp \[open $status w\]
	puts \$tmp \[catch \[list exec gs -dCompatibilityLevel=1.3 -dCompressPages=false -q -dNOPAUSE -dBATCH -sDEVICE=pdfwrite -sOutputFile=${pdf}.tmp -c .setpdfwrite -f $ps >@ stdout 2>@ stderr\]\]
	close \$tmp"
	close $tmp
	set gspid [lindex [exec tclsh $script >@ stdout 2>@ stderr &] 0]
	set wait 0
	set maxwait [maxwaitfor [file size $ps]]
	while {[file exists /proc/$gspid] && $wait < $maxwait} {
	    exec sleep 1
	    incr wait
	}
	if {[file exists /proc/$gspid]} {
	    # Program ran too long.
	    catch {exec sh -c "kill -KILL $gspid"}
	    puts "****** $ps FAILED, RAN TOO LONG ******"
	    catch {exec mv ${pdf}.tmp ${pdf}.bad}
	} elseif {[file size $status] < 2} {
	    # Exit status not recorded (not supposed to be possible).
	    puts "****** $ps FAILED, STATUS NOT WRITTEN ******"
	    catch {exec mv ${pdf}.tmp ${pdf}.bad}
	} else {
	    set tmp [open $status]
	    set result [gets $tmp]
	    close $tmp
	    if {$result != 0} {
		puts "****** $ps FAILED, RESULT = $result ******"
		catch {exec mv ${pdf}.tmp ${pdf}.bad}
	    } else {
		exec mv ${pdf}.tmp $pdf
		exec ls -l $ps $pdf
	    }
	}
	flush stderr
    }
    flush stdout
}
close $lst
puts Done.
