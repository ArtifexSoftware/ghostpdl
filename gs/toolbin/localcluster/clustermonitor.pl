#!/usr/bin/perl

use strict;
use warnings;

use Time::localtime;
use File::stat;

my $watchdog=0;

my @machines = </home/regression/cluster/*.status>;

for (my $i=0;  $i<scalar(@machines);  $i++) {
  $machines[$i]=~s/.status//;
  $machines[$i]=~s|.*cluster/||;
}


#print "@machines\n";

my %status;


my %months=(
'Jan' => 1,
'Feb' => 2,
'Mar' => 3,
'Apr' => 4,
'May' => 5,
'Jun' => 6,
'Jul' => 7,
'Aug' => 8,
'Sep' => 9,
'Oct' => 10,
'Nov' => 11,
'Dec' => 12
);

my $b="";

my $lastCols=0;
my $lastRows=0;


while(1) {

  my ($rows,$cols) = split(/ /,`/bin/stty size`);
  chomp $cols;
  if ($rows != $lastRows || $cols != $lastCols) {
    $lastRows=$rows;
    $lastCols=$cols;
    print chr(0x1b)."[1;1H";  # clear screen
    print chr(0x1b)."[J";
    %status={};   # force refresh
    $b="";
  }

    my $status=`cat /home/regression/cluster/status`;
    chomp $status;

    if (!exists $status{'main'}{"status"} || $status{'main'}{"status"} ne $status) {
      print chr(0x1b)."[".(1).";1H";
      $status=substr($status,0,$cols-1);
      printf "%s".chr(0x1b)."[K\n",$status;
      print chr(0x1b)."[".(scalar(@machines)+2).";1H";
      $status{'main'}{"status"}=$status;
    }

  for (my $i=0;  $i<scalar(@machines);  $i++) {
    my $machine=$machines[$i];
    my $s1;

    my $s0=`cat /home/regression/cluster/$machine.status`;
    chomp $s0;
    my $down="";
    my $downTime=0;
    $down="--DOWN--" if (!stat("/home/regression/cluster/$machine.up") || ($downTime=(time-stat("/home/regression/cluster/$machine.up")->mtime))>300);
    if (!exists $status{$machine}{"status"} || $status{$machine}{"status"} ne $s0 || $status{$machine}{"down"} ne $down) {
      $status{$machine}{"status"}=$s0;
      $status{$machine}{"down"}=$down;
      print chr(0x1b)."[".($i+2).";1H";
      $s0=substr($s0,0,$cols-14-length($down));
      printf "%-10s  %s %s".chr(0x1b)."[K\n",$machine,$down,$s0;
      print chr(0x1b)."[".(scalar(@machines)+2).";1H";
    }
    my $a=`cat /home/regression/cluster/queue.lst`;
    if ($a ne $b) {
      print chr(0x1b)."[".(scalar(@machines)+3).";1H";
      print "Current and pending jobs:\n";
      print chr(0x1b)."[J";
      print $a;
      $b=$a;
    }
  }

  sleep 1;
}

