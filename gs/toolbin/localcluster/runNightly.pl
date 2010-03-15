#!/usr/bin/perl

use strict;
use warnings;

use Data::Dumper;


my $name=0;
my $error=1;
my $time1=2;
my $time2=3;
my $unused1=4;
my $unused2=5;
my $md5=6;
my $revision=7;
my $product=8;
my $node=9;

#my %data;

open (LOG,">runNightly.log");

my $local=1;
sub mylog($) {
  my $d=`date`;
  chomp $d;
  my $s=shift;
  chomp $s;
if ($local) {
  print     "$d: $s\n";
} else {
  print LOG "$d: $s\n";
}
}


sub getRevision() {
  my $r1=`svn info ghostpdl    | grep \"Last Changed Rev\" | awk '{ print \$4 } '`;
  chomp $r1;
  my $r2=`svn info ghostpdl/gs | grep \"Last Changed Rev\" | awk '{ print \$4 } '`;
  chomp $r2;
  $r1=$r2 if ($r2>$r1);
  return($r1);
}

`touch temp; rm -fr temp ; mkdir temp`;
#`touch /media/ssd/temp; rm -fr /media/ssd/temp ; mkdir /media/ssd/temp`;

mylog "getRevision()";
my $rev=getRevision();

mylog "svn update tests";
`svn update tests`;
mylog "svn update tests_private";
`svn update tests_private`;

print "current rev: $rev\n";

#if (open(F,"<baseline.tab")) {
#  while(<F>) {
#    chomp;
#    my @a=split '\t';
#    foreach (my $i=0;  $i<scalar(@a);  $i++) {
#      $data{$a[0]}[$i]=$a[$i];
#    }
#  }
#  close(F);
#}

#Run build.pl to build a list of jobs
mylog "build.pl local";
`build.pl local >jobs`;

#Check to see what files are missing from baseline by using ls/grep. Removing those from the jobs list.

my $a=`cd baseline ; ls`;

$a =~ s/\.gz//g;

open (F,">temp.lst");
print F "$a";
close(F);

`fgrep -v -f temp.lst jobs >jobs.new`;
`fgrep    -f temp.lst baseline.tab >old.tab`;

`mv jobs.new jobs`;

#`head -1000 jobs >jobs.tmp ; mv jobs.tmp jobs`;  # mhw


#Run the old ghostscript to build bitmaps.
mylog "run.pl local_no_builds";
#`./run.pl local_no_build 2> local.out`;

#Move the bitmaps from temp to permanent.
$a=`cd temp ; ls`;
my @a=split '\n',$a;
foreach (@a) {
  chomp;
  if (m/\.gz$/) {
    `mv temp/$_ baseline/.`;
  }
}

`readlog.pl local.log temp.tab local local.out $rev`;
`cat temp.tab old.tab | sort >baseline.new ; mv baseline.new baseline.tab`;

`mv jobs baseline.jobs`;
#`rm temp.tab old.tab local.log local.out temp.lsts`;

#open(F,">new.tab") || die "can't write to new.tab";
#foreach (sort keys %data) {
#  print F "$data{$_}[0]";
#  for (my $i=1;  $i<scalar(@{$data{$_}});  $i++) {
#    print F "\t$data{$_}[$i]";
#  }
#  print F "\n";
#}
#close(F);

#Run build.pl to build a list of jobs
mylog "build.pl local";
`build.pl local >jobs`;


#Store revision as previousRevision.

my $preRev=$rev;

#Update Ghostscript.

mylog "svn update ghostpdl";
`svn update ghostpdl`;

$rev=getRevision();

#if (revision ne previsionRevision) begin

if ($rev != $preRev) {

mylog "run.pl local";
`./run.pl local 2> local.out`;
#  die "$rev != $preRev : do something";

#Build.

#Run

#Gather md5 files/errors in $revision.tab
mylog "readlog.pl local.log";
`readlog.pl local.log $rev.tab local local.out $rev`;

#Compare $revision.tab to baseline.tab
#Compare $revision.tab to $previousRevision.tab

#end

}

close(LOG);


