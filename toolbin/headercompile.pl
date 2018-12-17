#!/usr/bin/perl

# First we figure out the order of dependency of the headers,
# then we build in reverse order. This ensures that we fix
# any breakages as close to the leaves of the tree as possible.

my %deps;
my %dirs;

my %fname2num;
my @num2fname;
my @deparr;
my @good;

sub readdeps($)
{
    my $dir = shift;
    my @headers = `cd $dir; ls *.h`;
    foreach my $h (@headers) {
        $h =~ s/[\r\n]*$//;

        if (!exists $fname2num{$h}) {
            my $c = scalar(keys %fname2num)+1;
            #print "New file $h: $c\n";
            $fname2num{$h} = $c;
            $num2fname[$c] = $h;
        }
        open(F, "$dir/$h") || die "WTF?";
        while (<F>) {
            if ($_ =~ m/^\s*#\s*include "abc\.h"/) {
                next; # Comment in std.h
            }
            if ($_ =~ m/^\s*#\s*include "(.+)"/) {
                if (!exists $fname2num{$1}) {
                    my $c = scalar(keys %fname2num)+1;
                    #print "New file $1: $c\n";
                    $fname2num{$1} = $c;
                    $num2fname[$c] = $1;
                    if ($1 eq "arch.h" ||
                        $1 eq "gconfig.h" ||
                        $1 eq "gconfig_.h" ||
                        $1 eq "jmcorig.h" ||
                        $1 eq "jpeglib.h") {
                        $dirs{$1} = "gen";
                    }
                    if ($1 eq "jerror.h" ||
                        $1 eq "jmemsys.h") {
                        $dirs{$1} = "jpeg";
                    }
                }
                my $n = $fname2num{$1};
                $deps{$h}{$1}=1;
#               print "($dir/)$h => $1\n";
                $deparr[$fname2num{$h}][$n]=1;
            }
        }
        $dirs{$h} = $dir;
    }
}

sub compare($$)
{
    my $a = shift;
    my $b = shift;

    my $v1 = $deparr[$a][$b];
    my $v2 = $deparr[$b][$a];

    if ($v1) {
        if ($v2) {
            print "Cycle: $a $b\n";
            return 0;
        } else {
            return -1;
        }
    } elsif ($v2) {
        return 1;
    } else {
        return 0;
    }
}

sub pcompare($$)
{
    my $a = shift;
    my $b = shift;
    my $v = compare($a,$b);

    print "$num2fname[$a] ";
    if ($v < 0) {
        print "<";
    } elsif ($v > 0) {
        print ">";
    } else {
        print "=";
    }
    print " $num2fname[$b]\n";

    return $v;
}

# main: Work starts here 

readdeps("base");
readdeps("psi");
readdeps("devices");
readdeps("devices/vector");

print "Processing header dependencies...\n";

my $mx = scalar @num2fname;

# Fill in the missing array elements with 0
for (my $i=1; $i < $mx; $i++) {
    for (my $j=1; $j < $mx; $j++) {
        if (!defined $deparr[$i][$j]) {
            $deparr[$i][$j] = 0;
        }
    }
}
#for (my $i=1; $i < $mx; $i++) {
#    for (my $j=1; $j < $mx; $j++) {
#       print "$deparr[$i][$j] ";
#    }
#    print "\n";
#}

# Form transitive closure.
# n-1 passes
my $changed;
my $pass = 0;
do {
    print "Pass $pass\n";
    $changed = 0;
    # For each row...
    for (my $i=1; $i < $mx; $i++) {
        # For each thing we point to...
        for (my $j=1; $j < $mx; $j++) {
            my $depth = $deparr[$i][$j];
            if ($depth != 0) {
                #print " $i $j $depth\n";
                for (my $k=1; $k < $mx; $k++) {
                    my $n = $deparr[$j][$k];
                    if ($n != 0 &&
                        ($deparr[$i][$k] == 0 ||
                         $deparr[$i][$k] > $depth + $n)) {
                        $deparr[$i][$k] = $depth + $n;
                        $changed = 1;
                    }
                }
            }
        }
    }
    $pass++;
} while ($changed);

#{
#    my $i = $fname2num{'gxdevice.h'};
#    for (my $j=1; $j < $mx; $j++) {
#       print "gxdevice.h vs $num2fname[$j] = $deparr[$i][$j]\n";
#    }
#    for (my $j=1; $j < $mx; $j++) {
#       print "$num2fname[$j] vs gxdevice.h = $deparr[$j][$i]\n";
#    }
#}

#for (my $i=1; $i < $mx; $i++) {
#    for (my $j=1; $j < $mx; $j++) {
#       print "$deparr[$i][$j] ";
#    }
#    print "\n";
#}

# Detect circularity
for (my $i = 1; $i < $mx; $i++) {
    if ($deparr[$i][$i] != 0) {
        print "Circularity detected: $i $num2fname[$i] $deparr[$i][$i]\n";
        # Eliminate that from the sort
        for (my $j = 0; $j < $mx; $j++) {
            $deparr[$j][$i] = 0;
            $deparr[$i][$j] = 0;
        }
    }
}

# Set up an ordering array.
my @order;
for (my $i = 1; $i < $mx; $i++) {
    $order[$i]=$i;
}

# For neatness, sort alphabetically first.
for (my $i = 1; $i < $mx-1; $i++) {
    for (my $j = $i+1; $j < $mx; $j++) {
        if ($order[$i] gt $order[$j]) {
            my $swap = $order[$i];
            $order[$i]=$order[$j];
            $order[$j]=$swap;
        }
    }
}

# Bubble sort the dependencies, cos that's the kind of reckless
# algorithmic efficiency junkie that I am.
for (my $i = 1; $i < $mx-1; $i++) {
    for (my $j = $i+1; $j < $mx; $j++) {
        if (compare($order[$i], $order[$j]) > 0) {
            my $swap = $order[$i];
            $order[$i]=$order[$j];
            $order[$j]=$swap;
        }
    }
}

#for (my $ui = 1; $ui < $mx; $ui++) {
#    my $i = $order[$ui];
#    my $a = $num2fname[$i];
#    my $found = 0;
#    if (!defined $a) { next; }
#    for (my $uj = 1; $uj < $mx; $uj++) {
#       my $j = $order[$uj];
#       if ($deparr[$i][$j] == 1) {
#           my $b = $num2fname[$j];
#           if ($found == 0) {
#               print "$a depends on:\n";
#               $found = 1;
#           }
#           print " $b\n";
#       }
#    }
#}

sub find_ordered($)
{
    my $a = shift;
    for ($i = 1; $i < $mx; $i++) {
        if ($num2fname[$order[$i]] eq $a) {
            return $i;
        }
    }
    return -1;
}

sub vcompare($$)
{
    my $a = shift;
    my $b = shift;

    my $ao = find_ordered($a);
    my $bo = find_ordered($b);
    my $au = $order[$ao];
    my $bu = $order[$bo];
    my $an = $num2fname[$au];
    my $bn = $num2fname[$bu];
    my $ad = $deparr[$au][$bu];
    my $bd = $deparr[$bu][$au];

    print "$ao->$au = $a,$an,$ad\n";
    print "$bo->$bu = $b,$bn,$bd\n";
}

# Do the compilation
for (my $ui=$mx-1; $ui >= 0; $ui--) {
    my $i = $order[$ui];
    my $f = $num2fname[$i];

    if (!exists $dirs{$f}) {
        next;
    }
    
    $h = $dirs{$f}."/".$f;

    my $ret = system("cc -DHAVE_INTTYPES_H -Ibase -Ipsi -Ilcms2mt -Ilcms2 -Iopenjpeg/src/lib/openjp2 -Ijpeg -Ijbig2dec   -Iobj -o tmp.o $h > tmp.err 2>&1");

    print "------------------------------------------------\n";
    $good[$i]=$ret;
    if ($ret == 0) {
        print "OK: $h\n";
    } else {
        print "BAD: $h\n";
    }
    open(F, "tmp.err") || die "WTF?";
    while (<F>) {
        print $_;
    }
    unlink("tmp.o");
    unlink("tmp.err");
}

# Write the dependencies
#for (my $ui=1; $ui < $mx; $ui++) {
#    my $i = $order[$ui];
#    if ($good[$i] != 0) { next; }
#
#    my $f = $num2fname[$i];
#    my $g = $f;
#    $g =~ s/\./_/;
#
#    if ($dirs{$f} eq "base") {
#        print "$f=\$(GLSRC)$g";
#    } elsif ($dirs{$f} eq "psi") {
#        print "$f=\$(PSSRC)$g";
#    } else {
#        print "$f=$g";
#    }
#    for (my $uj=1; $uj < $mx; $uj++) {
#       my $j = $order[$uj];
#       if ($deparr[$i][$j] != 1) { next; }
#       my $e = $num2fname[$j];
#       my $d = $e;
#
#       $e =~ s/\./_/;
#       print " \$($e)";
#    }
#    print "\n";
#}

sub prep($) {
    my $f = shift;
    if ($dirs{$f} eq "base") {
        return "\$(GLSRC)";
    } elsif ($dirs{$f} eq "psi") {
        return "\$(PSSRC)";
    } elsif ($dirs{$f} eq "gen") {
        return "\$(GLGEN)";
    } elsif ($dirs{$f} eq "jpeg") {
        return "\$(JSRCDIR)\$(D)";
    } elsif ($dirs{$f} eq "devices") {
        return "\$(DEVSRC)";
    } elsif ($dirs{$f} eq "devices/vector") {
        return "\$(DEVVECSRC)";
    } else {
        return "";
    }
}

sub rewrite($$) {
    my $i = shift;
    my $m = shift;

    my $f = $num2fname[$i];
    my $g = $f;
    $g =~ s/\./_/;

    my $h = prep($f);
    print "$g=$h$f";
    if ($m) {
        print " \$(MAKEFILE)";
    }
    print "\n";
}

sub write_dep($$)
{
    my $g = shift;
    my $m = shift;
    my $f = $g;

    $f =~ s/_h/\.h/;
    for (my $ui=1; $ui < $mx; $ui++) {
        my $i = $order[$ui];
        if ($num2fname[$i] ne $f) { next; }
        if (!exists $dirs{$num2fname[$i]}) {
            next;
        }
        if ($good[$i] != 0) {
            $good[$i] = -1;
            next;
        }
        $good[$i] = -2;

        rewrite($i, $m);
        return $i;
    }
    return 0;
}

sub rewrite_make($)
{
    my @deplist = ();
    my $f = shift;

    print "$f:\n";

    open(F, "$f") || die "WTF?";
    while (<F>) {
        $_ =~ s/[\r\n]*$//;
	if ($_ =~ m/^\# Dependencies:/) {
	    last;
	}
        if ($_ =~ m/^([a-z0-9_\-]+_h)=/) {
            my $f = $1;
            # Gather up the line, allowing for continuations
            if ($_ =~ m/\\$/) {
                my $block = $_;
                $block =~ s/\\$//;
                while (<F>) {
                    $_ =~ s/[\r\n]*$//;
                    my $new = $_;
                    $new =~ s/\\$//;
                    $block .= $new;
                    if ($_ =~ m/\\$/) {
                    } else {
                        last;
                    }
                }
                $_ = $block;
            }
            
            my $makefile = 0;
            if ($_ =~ m/\$\(MAKEFILE\)/) {
                $makefile = 1;
            }
            my $i = write_dep($f, $makefile);
            if ($i == 0) {
                print "$_\n";
            } else {
                push @deplist, $i;
            }
        } else {
            print "$_\n";
        }
    }
    print "# Dependencies:\n";
    foreach $i (@deplist) {
        my $f = $num2fname[$i];
        my $df = prep($f);
        for (my $uj=1; $uj < $mx; $uj++) {
            my $j = $order[$uj];
            if ($deparr[$i][$j] == 0) { next; }
            my $e = $num2fname[$j];
            if (!exists $dirs{$e}) {
                # Don't include things like sgidefs_h
                next;
            }
            my $d = prep($e);

            print "$df$f:$d$e\n";
        }
    }
    print "-----\n";
}

rewrite_make("base/gs.mak");
rewrite_make("base/pcwin.mak");
rewrite_make("base/lib.mak");
rewrite_make("base/tiff.mak");
rewrite_make("base/winlib.mak");
rewrite_make("psi/int.mak");
rewrite_make("psi/winint.mak");
rewrite_make("devices/contrib.mak");
rewrite_make("devices/devs.mak");
rewrite_make("gpdl/pspcl6_gcc.mak");
rewrite_make("pcl/pl/pl.mak");

# Now output the troubled ones.
print "\nThese references are questionable: (possibly they didn't compile?)\n";
for (my $ui=1; $ui < $mx; $ui++) {
    my $i = $order[$ui];
    if ($good[$i] != -1) {
        next;
    }

    rewrite($i,0);
}

print "\nThese references weren't found: (system headers?)\n";
for (my $ui=1; $ui < $mx; $ui++) {
    my $i = $order[$ui];
    if (defined $good[$i]) {
        next;
    }

    rewrite($i,0);
}

print "\nThese references were never mentioned:\n";
for (my $ui=1; $ui < $mx; $ui++) {
    my $i = $order[$ui];
    if (!defined $good[$i]) {
        next;
    }
    if ($good[$i] == -1) {
        next;
    }
    if ($good[$i] == -2) {
        next;
    }

    rewrite($i,0);
}
