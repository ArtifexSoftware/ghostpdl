#!/usr/bin/perl

#
# this is a simple script that can be used to gather performance data from
# different revisions of Ghostscript.  
#
# the command line options for performance.pl are:
#
#  shellScript - a simple shell script that calls Ghostscript, the format
#                is the same as used for search-svn-revs, for example:
#    bin/gs -Ilib -I/home/support/fonts -sDEVICE=ppmraw -dNOPAUSE -dBATCH -sOutputFile=test.out ../input.ps
#
#  startRev  - the first rev to test
#
#  endRev    - the last rev to test
#
#  increment - revision increment
#
#
#  example command line (assuming the shellScript is called testscript):
#
#    performance.pl testscript 8500 9500 10 >perf.log
#    -  test every 10 revs from r8500 to r9500
#
#  The output is formatted to make it easily plotted with gnuplot (rev tab time tab size_of_exe)


use strict;
use warnings;

my $command=shift;
my $startRev=shift;
my $endRev=shift || die "usage: performance.pl shellScript startRev endRev [increment]";
my $increment=shift;
$increment=1 if (!$increment);

for (my $i=$startRev;  $i<=$endRev;  $i+=$increment) {
  my $exe="gs.$i/bin/gs";
  my $bad="gs.$i/does_not_build";
  if (-e $bad) {
    print STDERR "skipping r$i, build previously failed\n";
  } else {
    if (! -e $exe) {
      print STDERR "fetching r$i\n";
      my $s;
      $s=sprintf "touch gs.%d ; rm -fr gs.%d ; svn export -r%d http://svn.ghostscript.com/ghostscript/trunk/gs gs.%d",$i,$i,$i,$i;
      `$s`;
      $s=sprintf "cd gs.%d ; ./autogen.sh 2>/dev/null ; make -j 2 2>/dev/null",$i;
      `$s`;
      if (! -e $exe) {
        print STDERR "skipping r$i, build failed\n";
        `touch $bad`;
      }
    } else {
      print STDERR "already exists r$i\n";
    }
  }
  if (-e $exe) {
    my $size=-s $exe;
    my $a=`cd gs.$i ; /home/marcos/bin/time -f "%U %S %E %P" ../$command 2>&1`;
    my @a=split '\n',$a;
    chomp $a[-1];
    my $t=0;
    if ($a[-1] =~ m/(\d+.\d+) (\d+.\d+)/) {
       $t=$1+$2;
    }
    print "$i\t$t\t$size\n";
  }
}


