#!/usr/bin/perl

use strict;
use warnings;

use Data::Dumper;
use POSIX ":sys_wait_h";

my $file="";
my $error=0;
my $md5sum=0;
my $divider=0;

my $t1;
my $t2;
my $t3;
my $t4;

my %results;

my $input=shift;
my $output=shift;
my $machine=shift;
my $input2=shift;
my $rev=shift;

$rev=0 if (!$rev);

($machine) || die "usage: readlog.pl input.log output machine [input.out] [rev]";

open (F,"<$input") || die "file $input not found";

while(<F>) {

  chomp;

  if (m/^compileFail/) {
    close(F);
    print "$_\n";
    exit;
  }

  if (m/===(.+).log===/ || m/===(.+)===/) {
    $file=$1;
    $error=0;
    $divider=0;
    $md5sum=0;
    $results{$file}{"error"}=-1;
    $results{$file}{"md5"}=$md5sum;
    $results{$file}{"time1"}=0;
    $results{$file}{"time2"}=0;
    $results{$file}{"time3"}=0;
    $results{$file}{"time4"}=0;
    my $t=<F>;
    chomp $t;
    $results{$file}{"product"}=$t;
  }
  if (m/^---$/) {
    $divider=1;
  }

  if (m/Unrecoverable error, exit code/ || m/Command exited with non-zero status/ || m/Command terminated by signal/ || m/Warning interpreter exited with error code/ || m/Segmentation fault/) {
    $error=1 if ($divider==0 && $error==0);
    $error=2 if ($divider==1 && $error==0);
    $results{$file}{"error"}=$error;
  }
  if (m/killed: timeout/) {
    $error=3 if ($divider==0 && $error==0);
    $error=4 if ($divider==1 && $error==0);
    $results{$file}{"error"}=$error;
  }
  if (m/Unable to open/) {
    $error=5 if ($divider==0 && $error==0);
    $error=6 if ($divider==1 && $error==0);
    $results{$file}{"error"}=$error;
  }
  if (m/(\d+\.\d+) (\d+\.\d+) (\d+:\d\d\.\d\d) (\d+)%/) {
    $t1=$1;
    $t2=$2;
    $t3=$3;
    $t4=$4;
#   print "time=$t1 $t2 $t3 $t4\n";
  }
  if (m/^([0-9a-f]{32})$/ || m/^([0-9a-f]{32})  -/) {
    $md5sum=$1;
#   print "md5sum=$md5sum\n";
    $results{$file}{"error"}=$error;
    $results{$file}{"md5"}=$md5sum;
    $results{$file}{"time1"}=0;
    $results{$file}{"time2"}=0;
    $results{$file}{"time3"}=0;
    $results{$file}{"time4"}=0;
  }
}

close(F);

if ($input2) {
  open (F,"<$input2") || die "file $input2 not found";
  while(<F>) {
    if (m|Segmentation fault .+ ./temp/(\S+).log 2|) {
      my $file=$1;
      my $pdfwrite=0;
      $pdfwrite=1 if (m/pdfwrite/);
#     print "$pdfwrite $file\n";
      if (exists $results{$file}{"error"}) {
        $results{$file}{"error"}=7 if ($pdfwrite==1);
        $results{$file}{"error"}=8 if ($pdfwrite==0 && $results{$file}{"error"}%2 == 0);
      } else {
#       die "$file not found in ressults";
      }
    }
  }
  close(F);
}


open(F,">$output") || die "file $output can't be written to";
foreach (sort keys %results) {
  print F "$_\t$results{$_}{'error'}\t$results{$_}{'time1'}\t$results{$_}{'time2'}\t$results{$_}{'time3'}\t$results{$_}{'time4'}\t$results{$_}{'md5'}\t$rev\t$results{$_}{'product'}\t$machine\n";
}
close(F);

if (0) {
  foreach (sort keys %results) {
    print "$_\n" if ($results{$_}{'error'} != 0);
  }
}

