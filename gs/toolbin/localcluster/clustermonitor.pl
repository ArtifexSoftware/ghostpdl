#!/usr/bin/perl

use strict;
use warnings;

use Time::localtime;
use File::stat;

my $watchdog=0;

#my @machines=("i7","i7a","macpro","peeves","test");
#my @machines=("i7","i7a","test");
#
my @machines = </home/regression/cluster/*.status>;
#print "@machines\n";

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

print chr(0x1b)."[1;1H";
print chr(0x1b)."[J";

while(1) {

  my $status=`cat /home/regression/cluster/status`;
  chomp $status;

  if (!exists $status{'main'}{"status"} || $status{'main'}{"status"} ne $status) {
    print chr(0x1b)."[".(1).";1H";
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
  $down="--DOWN--" if (!stat("/home/regression/cluster/$machine.up") || ($downTime=(time-stat("/home/regression/cluster/$machine.up")->ctime))>300);
  if (!exists $status{$machine}{"status"} || $status{$machine}{"status"} ne $s0 || $status{$machine}{"down"} ne $down) {
    $status{$machine}{"status"}=$s0;
    $status{$machine}{"down"}=$down;
    print chr(0x1b)."[".($i+2).";1H";
    printf "%-10s  %s %s".chr(0x1b)."[K\n",$machine,$down,$s0;
    print chr(0x1b)."[".(scalar(@machines)+2).";1H";
  }
  my $a=`cat /home/regression/cluster/user.run`;
  if ($a ne $b) {
    print chr(0x1b)."[".(scalar(@machines)+3).";1H";
    print "pending jobs:\n";
    print $a;
    $b=$a;
print chr(0x1b)."[J";
  }
}


#print $s."\n";
#print $s2."\n";

sleep 1;
}

