#!/usr/bin/tclsh
proc print {file {first ""} {last ""}} {
    if {$first == ""} {
	set first 1
	set last 99999
    } elseif {$last == ""} {
	set last $first
    }
    exec pclxl -sDEVICE=ljet4 -dMaxBitmap=200000 -dBufferSpace=200000 -sOutputFile=/prn -dNOPAUSE -dFirstPage=$first -dLastPage=$last $file >@stdout 2>@stderr
}
set argc [llength $argv]
if {$argc < 1 || $argc > 3} {
    puts stderr "Usage: pxprint file [first-page [last-page]]"
    exit
}
eval print $argv
