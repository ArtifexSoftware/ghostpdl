#!/usr/bin/tclsh

# Copyright (C) 1999, 2000 Aladdin Enterprises, Menlo Park, CA.
# All rights reserved.
# $Id$

# Make various Ghostscript filesets.  Assumes the current directory is gs.
#   maketars
#	ghostscript-#.#[#].tar.gz
#	    ({doc,examples,lib,man,src,toolbin}/* => gs#.#[#]/*/*)
#	ghostscript-#.#[#].tar.bz2
#	    (--same--)
#	ghostscript-#.#[#]gnu.tar.gz
#	    (gnu/*/* => gs#.#[#]/)
#   makefonts [#.#[#]]
#	ghostscript-fonts-std-#.#[#].tar.gz
#	    (fonts/?0*l.*, fonts/metrics/?0*l.* => fonts/)
#	gnu-gs-fonts-std-#.#[#].tar.gz
#	    (--same--)
#	ghostscript-fonts-other-#.#[#].tar.gz
#	    (...other fonts...)
#	gnu-gs-fonts-other-#.#[#].tar.gz
#	    (--same--)
#   makehist
#	(merges doc/News.htm and doc/Changes.htm into doc/History#.htm)
#   makewin
#	gs###.zip
#	    ({man,src,doc,examples,lib,toolbin}/* => gs#.#[#]/*/*,
#	     fonts/*.*, fonts/metrics/*.* => fonts/,
#	     {jpeg,libpng,zlib}/* => ibid.,
#	     gs###.bat)
#   makemaster
#	(moves the maketars & makefonts files to master/###/)

# Acquire the Ghostscript version numbers.
proc setnum {num3} {
    global Num3 Dot Dir

    set Num3 $num3
    set Dot [expr ${Num3} / 100].[expr ${Num3} % 100]

# Set other, derived globals.
    set Dir gs$Dot
}
setnum [exec egrep {^[0-9]+$} lib/gs_init.ps]
set Work {c:\work}

# Execute a shell command with *- and ?-expansion.
proc sh {args} {
    if {[lindex $args 0] == "-c"} {
	set cmd [concat sh $args]
    } elseif {[regexp {[*?]} $args]} {
	set cmd [list sh -c [join $args]]
    } else {
	set cmd $args
    }
    puts stdout $cmd
    puts stdout [eval exec -- $cmd 2>@ stderr]
}

# Move all or none of a list of files to a directory.
proc movelist {name files todir} {
    set ex {}
    set nex {}
    foreach f $files {
	if {[file exists $f]} {lappend ex $f} else {lappend nex $f}
    }
    if {$ex == ""} {
	puts stderr "$name archives do not exist."
	return
    }
    if {$nex != ""} {
	puts stderr "Missing files: $nex"
	exit 1
    }
    if {![file isdirectory $todir]} {
	puts "Creating $todir"; flush stdout
	sh mkdir -p $todir
    }
    foreach f $files {
	if {[regexp {^/c} $f]} {
	    sh cp $f $todir
	} else {
	    sh mv $f $todir
	}
    }
    sh ls -l $todir/*
}

# Create a symbolic link from name to path.
proc ln-s {path name} {sh ln -s $path $name}

# Make the tarballs for the code and documentation.
proc maketars {} {
    global Dir Dot

    set agz ghostscript-$Dot.tar.gz
    set abz2 ghostscript-$Dot.tar.bz2
    set agnu ghostscript-${Dot}gnu.tar.gz

    file delete $agz $abz2 $Dir
    ln-s . $Dir
    sh tar -chf ghostscript-$Dot.tar --exclude=\\*CVS\\* --exclude=\\*.mak.tcl $Dir/src $Dir/doc $Dir/lib $Dir/man $Dir/examples $Dir/toolbin
    file delete $Dir
    sh time bzip2 -c4 ghostscript-$Dot.tar > $abz2
    sh time gzip ghostscript-$Dot.tar
    ln-s ./gnu $Dir
    sh tar -czhf $agnu --exclude=\\*CVS\\* $Dir/*
    file delete $Dir
    sh ls -l ghostscript-$Dot*.tar.*
}

# Assemble the fonts and metrics with a given license in $tmp/fonts.
proc licensefonts {tmp annot eol} {
    set ftmp $tmp/fonts

    sh rm -rf $ftmp
    sh mkdir -p $ftmp
    sh cp -p /gs/fonts/*.*f* /gs/fonts/fonts.* $ftmp
    sh cp -p /gs/fonts/metrics/*.?f? $ftmp
    sh chmod 644 $ftmp/*
    sh -c "\
	    cd $tmp/fonts;\
	    /gs/gs -I/gs/lib -dNODISPLAY /gs/aladdin/anntfont.ps -c $annot quit;\
	    ctf $eol *.afm *.pfa *.gsf"
}

# Make the tarballs for the fonts.
proc makefonts {{dot ""}} {
    global Dot

    if {$dot != ""} {
	if {![regexp {^[0-9]+\.(0|[0-9][0-9])$} $dot]} {
	    puts stderr "Version numbers must be #.0 or #.##."
	    exit 1
	}
	setnum [expr "int($dot * 100)"]
    }
    set cwd [pwd]
    set afonts $cwd/ghostscript-fonts-std-$Dot.tar.gz
    set ofonts $cwd/ghostscript-fonts-other-$Dot.tar.gz
    set agfonts $cwd/gnu-gs-fonts-std-$Dot.tar.gz
    set ogfonts $cwd/gnu-gs-fonts-other-$Dot.tar.gz

    file delete $afonts $ofonts $agfonts $ogfonts
    set tmp /tmp/[pid].tmp

    licensefonts $tmp annotURWAladdin -u
    sh -c "\
	    cd $tmp;\
	    tar -czf $afonts fonts/?0*l.pfb fonts/?0*l.afm fonts/?0*l.pfm fonts/fonts.*;\
	    rm -f fonts/?0*l.?f?;\
	    tar -czf $ofonts fonts/*.pf\[ab\] fonts/*.gsf fonts/*.afm fonts/*.pfm"

    licensefonts $tmp annotURWGPL -u
    sh -c "\
	    cd $tmp;\
	    tar -czf $agfonts fonts/?0*l.pfb fonts/?0*l.afm fonts/?0*l.pfm fonts/fonts.*;\
	    rm -f fonts/?0*l.?f?;\
	    tar -czf $ogfonts fonts/*.pf\[ab\] fonts/*.gsf fonts/*.afm fonts/*.pfm"

    sh rm -rf $tmp
    sh ls -l $afonts $ofonts $agfonts $ogfonts
}

# Merge News and Changes into History#.
proc mergehist {news changes hist tmp} {
    # Merge News, Changes, and the existing History# to tmp.
    # Return 0 if OK, non-0 or an error if not.

    # Define the pattern for the version/date line.
    set vdpattern {Version ([0-9.]+) \((([0-9]+)-0*([1-9][0-9]*)-0*([1-9][0-9]*))\)}

    # Scan the News file to find the header line.
    while {[string first <h1> [set l [gets $news]]] != 0} {}
    if {![regexp $vdpattern $l skip nver ndate nyear nmonth nday]} {
	puts stderr "Can't parse header line in News: $l"
	return 1
    }
    set monthnames [list\
	January February March April May June\
	July August September October November December]
    set nmonthname [lindex $monthnames [expr $nmonth - 1]]

    # Read the rest of the News file to determine whether there are any
    # Incompatible Changes, which we need to know for the TOC entry.
    set nlines [list $l]
    set have_changes 0
    while {[string first </pre> [set l [gets $news]]] != 0} {
	if {[string first "Incompatible changes</h" $l] >= 0} {
	    set have_changes 1
	}
	lappend nlines $l
    }

    # Copy the prefix of the existing History file.
    while {[string first <li> [set l [gets $hist]]] != 0} {
	puts $tmp $l
    }

    # If there is already a History TOC for this version, delete it.
    if {![regexp $vdpattern $l skip hver hdate]} {
	puts stderr "Can't parse header line in History: $l"
	return 1
    }
    if {$hver > $nver} {
	puts stderr "First History version = $hver > first News version = $nver"
	return 1
    }
    if {$hver == $nver} {
	# Skip over the TOC section.
	while {[gets $hist] != "</ul>"} {}
	set l [gets $hist]
    }
    set hline $l

    # Advance the Changes file to the TOC.
    while {[string first <blockquote> [set l [gets $changes]]] != 0} {}

    # Create the new TOC entry.
    puts $tmp "<li><a href=\"#Version$nver\">Version $nver ($ndate)</a>"
    puts $tmp "<ul>"
    if {$have_changes} {
	puts $tmp "<li><a href=\"#${nver}_Incompatible_changes\">Incompatible changes</a>,"
	set l [gets $changes]
	if {[string first <li> $l] == 0} {
	    puts $tmp "    [string range $l 4 end]"
	} else {
	    # Changes TOC must be empty.
	    puts $tmp $l
	}
    }
    while {[set l [gets $changes]] != "</ul></blockquote>"} {
	puts $tmp $l
    }

    # Copy the rest of the TOC and preamble.
    puts $tmp $hline
    while {[string first <h1> [set l [gets $hist]]] != 0} {
	puts $tmp $l
    }

    # If there is a History section for this version, delete it.
    if {![regexp $vdpattern $l skip hver hdate]} {
	puts stderr "Can't parse header line in History: $l"
	return 1
    }
    if {$hver == $nver} {
	# Skip over the history section.
	while {[set l [gets $hist]] != "<hr>"} {}
	# Skip the following blank line, too.
	gets $hist
	set l [gets $hist]
    }
    set hline $l

    # Copy the comment and "incompatible changes" sections from News.
    foreach l $nlines {
	puts $tmp $l
    }

    # Copy the rest of Changes.
    while {[string first </pre></body> [set l [gets $changes]]] != 0} {
	puts $tmp $l
    }

    # Copy the rest of the History file, changing the date at the end.
    puts $tmp <hr>
    puts $tmp ""
    puts $tmp $hline
    while {[string first "Ghostscript version " [set l [gets $hist]]] != 0} {
	puts $tmp $l
    }
    
    puts $tmp "Ghostscript version $nver, $nday $nmonthname $nyear"
    while {[gets $hist l] >= 0} {
	puts $tmp $l
    }

    return 0
}
proc makehist {} {
    global Dot

    set tmpname /tmp/[pid].htm
    set news [open doc/News.htm]
    set changes [open doc/Changes.htm]
    set inum [expr int($Dot)]
    if {$inum > 9} {
	set histname doc/Hist$inum.htm
    } else {
	set histname doc/History$inum.htm
    }
    set hist [open $histname]
    set tmp [open $tmpname w]
    set ecode [catch {set code [mergehist $news $changes $hist $tmp]} errMsg]
    close $tmp
    close $hist
    close $changes
    close $news
    if {$ecode == 0 && $code == 0} {
	file rename -force $tmpname $histname
    } else {
	file delete $tmpname
	if {$ecode != 0} {
	    error $errMsg
	}
    }
}

# Make the zip file for building on Windows.
proc makewin {} {
    global Dir Num3 Work

    set cwd [pwd]
    set atmp $cwd/gs${Num3}.zip
    set asetup gs${Num3}.bat
    set tmp /tmp/[pid].tmp

    file delete $atmp $asetup $Dir
    ln-s . $Dir

    set out [open $asetup w]
    # The dir\NUL test is per Microsoft....
    puts $out "if not exist $Work\\$Dir\\bin\\NUL mkdir $Work\\$Dir\\bin"
    puts $out "if not exist $Work\\$Dir\\obj\\NUL mkdir $Work\\$Dir\\obj"
    puts $out "set GS_LIB=$Work\\$Dir\\lib;$Work\\fonts"
    puts $out "set MOREPATH=%MOREPATH%$Work\\$Dir\\bin;$Work\\$Dir\\lib;"
    puts $out "call \\postboot.bat"
    puts $out "cd $Dir"
    close $out

    sh zip -q -l $atmp $asetup $Dir/src/* $Dir/man/* -x $Dir/\\*/CVS/ $Dir/\\*.mak.tcl
    sh zip -q -l -g $atmp $Dir/lib/* $Dir/doc/* $Dir/examples/* $Dir/toolbin/* -x $Dir/\\*/CVS/
    sh zip -q -l -r -g $atmp $Dir/jpeg $Dir/libpng $Dir/zlib
    
    licensefonts $tmp annotURWAladdin -d
    sh -c "\
	    cd $tmp;\
	    zip -q -g $atmp fonts/*.pfa fonts/*.pfb fonts/*.gsf fonts/*.afm fonts/*.pfm fonts/fonts.*"

    file delete $asetup $Dir
    sh ls -l $atmp
}

# Move the tar archives to the 'master' directory.
proc makemaster {} {
    global Num3

    set todir master/$Num3

    set agz ghostscript-$Dot.tar.gz
    set abz2 ghostscript-$Dot.tar.bz2
    set agnu ghostscript-${Dot}gnu.tar.gz
    movelist Code [list $agz $abz2 $agnu] $todir

    set afonts ghostscript-fonts-std-$Dot.tar.gz
    set ofonts ghostscript-fonts-other-$Dot.tar.gz
    set agfonts gnu-gs-fonts-std-$Dot.tar.gz
    set ogfonts gnu-gs-fonts-other-$Dot.tar.gz
    movelist Font [list $afonts $ofonts $agfonts $ogfonts] $todir
}

# Call the procedure selected by the link name.
switch [file tail $argv0] {
    maketars {eval maketars $argv}
    makefonts {eval makefonts $argv}
    makehist {eval makehist $argv}
    makewin {eval makewin $argv}
    makemaster {eval makemaster $argv}
}
