#!/usr/bin/perl

use strict;
use warnings;

use Time::localtime;
use File::stat;

use Data::Dumper;

my $verbose=1;

# todo:

my $runningSemaphore="./running";
my $maxDownTime=180;  # how many seconds before a machine is considered down

if (open(F,"<$runningSemaphore")) {
  close(F);
  exit;
}

open(F,">$runningSemaphore");
close(F);

my $currentRev1=`/usr/local/bin/svn info ghostpdl | grep "Last Changed Rev" | awk '{ print \$4} '`;
my $currentRev2=`/usr/local/bin/svn info ghostpdl/gs | grep "Last Changed Rev" | awk '{ print \$4} '`;
`/usr/local/bin/svn update ghostpdl`;
my $newRev1=`/usr/local/bin/svn info ghostpdl | grep "Last Changed Rev" | awk '{ print \$4} '`;
my $newRev2=`/usr/local/bin/svn info ghostpdl/gs | grep "Last Changed Rev" | awk '{ print \$4} '`;

chomp $currentRev1;
chomp $currentRev2;
chomp $newRev1;
chomp $newRev2;
#print "$currentRev1 $currentRev2 $newRev1 $newRev2\n";

if ($currentRev1!=0 && $currentRev2!=0 && ($currentRev1!=$newRev1 || $currentRev2!=$newRev2)) {

open(F,">revision");
print F "local cluster regression gs-r$newRev2 / ghostpdl-r$newRev1 (xefitra)\n";
close(F);



my %machines;
my @machines = <*.up>;
print "@machines\n" if ($verbose);
foreach (@machines) {
# print "$_\n";
# my $t=time-stat("$_")->ctime;
# print "$_ $t\n";
  $machines{$_}=1 if (stat("$_") && (time-stat("$_")->ctime)<$maxDownTime);
}
foreach (keys %machines) {
  delete $machines{$_};
  s/.up//;
  $machines{$_}=1;
}

print Dumper(\%machines) if ($verbose);

my $startTime;
my %doneTime;
my $s;
my $startText;
my %machineSpeeds;

my $abort=0;
do {

print "running with ".(scalar keys %machines)." machines\n" if ($verbose);

die "There aren't any cluster machines available" if (scalar keys %machines==0);

if (open(F,"<machinespeeds.txt")) {
  while(<F>) {
    chomp;
    my @a=split '\s';
    $machineSpeeds{$a[0]}=$a[1];
  }
  close(F);
}


my $options="";
foreach (sort keys %machines) {
  $machineSpeeds{$_}=1 if (!exists $machineSpeeds{$_});
  $options.=" $_.jobs $machineSpeeds{$_}";
}
print "$options\n" if ($verbose);
`./build.pl >jobs`;
`./splitjobs.pl jobs $options`;
unlink "jobs";

foreach (keys %machines) {
print "unlinking $_.done\n" if ($verbose);
print "unlinking $_.abort\n" if ($verbose);
  unlink("$_.jobs.gz");
  `gzip $_.jobs`;
  unlink("$_.done");
  unlink("$_.abort");
  `touch $_.start`;
}

$startText=`date +\"%H:%M:%S\"`;
chomp $startText;
open (F,">status");
print F "Regression gs-r$newRev2 / ghostpdl-r$newRev1 started at $startText";
close(F);

%doneTime=();
$abort=0;
$startTime=time;
print Dumper(\%doneTime) if ($verbose);
print Dumper(\%machines) if ($verbose);
print "".(scalar(keys %doneTime))." ".(scalar (keys %machines))."\n" if ($verbose);
while(scalar(keys %doneTime) < scalar(keys %machines)) {
  foreach (keys %machines) {
    if (!stat("$_.up") || (time-stat("$_.up")->ctime)>=$maxDownTime) {
print "$_ is down\n" if ($verbose);
      $abort=1;
      delete $machines{$_};
      %doneTime=();  # avoids a race condition where the machine we just aborted reports done
    } else {
      if (open(F,"<$_.done")) {
        close(F);
if ($verbose) {
print "$_ is reporting done\n" if (!exists $doneTime{$_});
}
        $doneTime{$_}=time if (!exists $doneTime{$_});
      }
    }
    `touch $_.abort` if ($abort);
  }
  sleep(1);
}
print "abort=$abort\n" if ($verbose);
} while ($abort);

my $elapsedTime=time-$startTime;

$s=`date +\"%H:%M:%S\"`;
chomp $s;
open (F,">status");
print F "Regression gs-r$newRev2 / ghostpdl-r$newRev1 started at $startText - finished at $s";
close(F);

my $averageTime=0;
foreach (keys %machines) {
  $averageTime+=$doneTime{$_}-$startTime;
}
$averageTime/=scalar(keys %machines);
print "averageTime=$averageTime\n" if ($verbose);
print "startTime=$startTime\n" if ($verbose);
print Dumper(\%doneTime) if ($verbose);
print Dumper(\%machineSpeeds) if ($verbose);
foreach (keys %machines) {
  $machineSpeeds{$_}*=$averageTime/($doneTime{$_}-$startTime);
}
print Dumper(\%machineSpeeds);
if (open(F,">machinespeeds.txt")) {
  foreach (sort keys %machineSpeeds) {
    print F "$_ $machineSpeeds{$_}\n";
  }
  close(F);
}


my $logs="";
foreach (keys %machines) {
  `touch $_.log`;
  `rm -f $_.log`;
  `gunzip $_.log.gz`;
  $logs.=" $_.log";
}

unlink "log";
`mv previous.tab previous2.tab`;
`mv current.tab previous.tab`;
`cat $logs >log`;
`./readlog.pl log current.tab`;

my $machineCount=scalar (keys %machines);

`./compare.pl current.tab previous.tab $elapsedTime $machineCount >email.txt`;
#`mail marcos.woehrmann\@artifex.com -s \"\`cat revision\`\" <email.txt`;
`mail gs-regression\@ghostscript.com -s \"\`cat revision\`\" <email.txt`;
`mail marcos.woehrmann\@artifex.com -s \"\`cat revision\`\" <email.txt`;

`touch archive/$newRev2-$newRev1`;
`rm -fr archive/$newRev2-$newRev1`;
`mkdir archive/$newRev2-$newRev1`;
`mv $logs archive/$newRev2-$newRev1/.`;
`gzip archive/$newRev2-$newRev1/*log`;
`cp current.tab archive/$newRev2-$newRev1.tab`;
`touch archive/$newRev2-$newRev1.tab.gz`;
unlink "archive/$newRev2-$newRev1.tab.gz";
`gzip archive/$newRev2-$newRev1.tab`;
unlink "log";

}


unlink $runningSemaphore;


