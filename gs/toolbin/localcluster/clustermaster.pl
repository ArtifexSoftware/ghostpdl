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

my %emails;
if (open(F,"<emails.tab")) {
  while(<F>) {
    chomp;
    my @a=split '\t';
    $emails{$a[0]}=$a[1];
  }
  close(F);
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

my $normalRegression=0;
my $userRegression="";
my $product="";
my $userName="";

if ($currentRev1!=0 && $currentRev2!=0 && ($currentRev1!=$newRev1 || $currentRev2!=$newRev2)) {
  $normalRegression=1;
} else {
  my $usersDir="users";

  opendir(DIR, $usersDir) || die "can't opendir $usersDir: $!";
  foreach my $user (readdir(DIR)) {
    if (open(F,"<$usersDir/$user/gs.run")) {
      close(F);
      unlink "$usersDir/$user/gs.run";
      open(F,">>user.run");
      print F "$user gs\n";
      close(F);
    }
    if (open(F,"<$usersDir/$user/ghostpdl.run")) {
      close(F);
      unlink "$usersDir/$user/ghostpdl.run";
      open(F,">>user.run");
      print F "$user ghostpdl\n";
      close(F);
    }
  }
  closedir DIR;

  if (open(F,"<user.run")) {
    my @a;
    while(<F>) {
      chomp;
      push @a,$_;
    }
    close(F);
    if (scalar(@a)>0) {
      open(F,">user.run");
      for (my $i=1;  $i<scalar(@a);  $i++) {
        print F "$a[$i]\n";
      }
      close(F);
      $userRegression=$a[0];
    }
  }
}

if ($normalRegression==1 || $userRegression ne "") {

if ($userRegression ne "") {
  print "running: $userRegression\n" if ($verbose);
}

if ($normalRegression) {
  open(F,">revision");
  print F "local cluster regression gs-r$newRev2 / ghostpdl-r$newRev1 (xefitra)\n";
  close(F);
}



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
foreach (keys %machines) {
  delete $machines{$_} if (stat("$_.down"));
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
if (!$normalRegression) {
  my @a=split ' ',$userRegression;
  $userName=$a[0];
  $product=$a[1];
  print "userName=$userName product=$product\n" if ($verbose);
}
`./build.pl $product >jobs`;
`./splitjobs.pl jobs $options`;
unlink "jobs";

foreach (keys %machines) {
print "unlinking $_.done\n" if ($verbose);
print "unlinking $_.abort\n" if ($verbose);
  unlink("$_.jobs.gz");
  `gzip $_.jobs`;
  unlink("$_.done");
  unlink("$_.abort");
  if ($normalRegression) {
    `touch $_.start`;
  } else {
    open(F,">$_.start");
    print F "$userRegression\n";
    close(F);
  }
}

$startText=`date +\"%D %H:%M:%S\"`;
chomp $startText;
open (F,">status");
if ($normalRegression) {
  print F "Regression gs-r$newRev2 / ghostpdl-r$newRev1 started at $startText";
} else {
  print F "Regression $userRegression started at $startText";
}
close(F);

%doneTime=();
$abort=0;
$startTime=time;
#print Dumper(\%doneTime) if ($verbose);
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
if ($normalRegression) {
  print F "Regression gs-r$newRev2 / ghostpdl-r$newRev1 started at $startText - finished at $s";
} else {
  print F "Regression $userRegression started at $startText - finished at $s";
}
close(F);

if ($normalRegression) {
my $averageTime=0;
foreach (keys %machines) {
  $averageTime+=$doneTime{$_}-$startTime;
}
$averageTime/=scalar(keys %machines);
my $shortestTime=99999999;
foreach (keys %machines) {
  $shortestTime=$doneTime{$_}-$startTime if ($shortestTime>$doneTime{$_}-$startTime);
}
print "averageTime=$averageTime\n" if ($verbose);
print "shortestTime=$shortestTime\n" if ($verbose);
print "startTime=$startTime\n" if ($verbose);
print Dumper(\%doneTime) if ($verbose);
print Dumper(\%machineSpeeds) if ($verbose);
my $max=0;
foreach (keys %machines) {
  $machineSpeeds{$_}=$machineSpeeds{$_}*($shortestTime/($doneTime{$_}-$startTime));
  $max=$machineSpeeds{$_} if ($max<$machineSpeeds{$_});
}
foreach (keys %machines) {
  $machineSpeeds{$_}/=$max;
  $machineSpeeds{$_}=(int($machineSpeeds{$_}*100+0.5))/100;
  $machineSpeeds{$_}=0.01 if ($machineSpeeds{$_}==0);
}
print Dumper(\%machineSpeeds);
if (open(F,">machinespeeds.txt")) {
  foreach (sort keys %machineSpeeds) {
    print F "$_ $machineSpeeds{$_}\n";
  }
  close(F);
}
}


my $logs="";
my $tabs="";
foreach (keys %machines) {
  `touch $_.log`;
  `rm -f $_.log`;
  `gunzip $_.log.gz`;
  `./readlog.pl $_.log $_.tab $_`;
  $logs.=" $_.log";
  $tabs.=" $_.tab";
}

my $machineCount=scalar (keys %machines);
if ($normalRegression) {
# unlink "log";
  `mv previous.tab previous2.tab`;
  `mv current.tab previous.tab`;
  `cat $tabs | sort >current.tab`;
  `rm $tabs`;
#  `cat $logs >log`;
#  `./readlog.pl log current.tab`;


  `./compare.pl current.tab previous.tab $elapsedTime $machineCount >email.txt`;
#  `mail marcos.woehrmann\@artifex.com -s \"\`cat revision\`\" <email.txt`;
  `mail -a \"From: marcos.woehrmann\@artifex.com\" gs-regression\@ghostscript.com -s \"\`cat revision\`\" <email.txt`;

  `touch archive/$newRev2-$newRev1`;
  `rm -fr archive/$newRev2-$newRev1`;
  `mkdir archive/$newRev2-$newRev1`;
  `mv $logs archive/$newRev2-$newRev1/.`;
  `gzip archive/$newRev2-$newRev1/*log`;
  `cp email.txt archive/$newRev2-$newRev1/.`;
  `cp current.tab archive/$newRev2-$newRev1.tab`;
#  `touch archive/$newRev2-$newRev1.tab.gz`;
#  unlink "archive/$newRev2-$newRev1.tab.gz";
#  `gzip archive/$newRev2-$newRev1.tab`;
# unlink "log";
} else {
  `cat $tabs | sort >temp.tab`;
  `rm $tabs`;

  `./compare.pl temp.tab current.tab $elapsedTime $machineCount true >$userName.txt`;
   if (exists $emails{$userName}) {
    `mail -a \"From: marcos.woehrmann\@artifex.com\" marcos.woehrmann\@artifex.com -s \"$userRegression regression\" <$userName.txt`;
    `mail -a \"From: marcos.woehrmann\@artifex.com\" $emails{$userName} -s \"$userRegression regression\" <$userName.txt`;
   } else {
    `mail -a \"From: marcos.woehrmann\@artifex.com\" marcos.woehrmann\@artifex.com -s \"bad username: $userName\" <$userName.txt`;
   }

}

}


unlink $runningSemaphore;


