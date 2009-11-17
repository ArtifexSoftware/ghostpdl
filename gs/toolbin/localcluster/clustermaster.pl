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

my $footer="";

# the concept with checkPID is that if the semaphore file is missing or doesn't
# contain our PID something bad has happened and we just just exit
sub checkPID {
  if (open(F,"<$runningSemaphore")) {
    my $t=<F>;
    close(F);
    chomp $t;
    if ($t == $$) {
      return(1);
    }
    print "terminating: $t $$\n";
    exit;
  }
  print "terminating: $runningSemaphore missing\n";
  exit;
}

  my $usersDir="users";

  opendir(DIR, $usersDir) || die "can't opendir $usersDir: $!";
  foreach my $user (readdir(DIR)) {
    my $product="";
    my $s="";
    if (open(F,"<$usersDir/$user/gs.run")) {
      close(F);
      unlink "$usersDir/$user/gs.run";
      $product="$user gs pcl xps svg";
    }
    if (open(F,"<$usersDir/$user/ghostpdl.run")) {
      close(F);
      unlink "$usersDir/$user/ghostpdl.run";
      $product="$user gs pcl xps svg";
    }
    if (open(F,"<$usersDir/$user/ghostpdl/cluster_command.run")) {
      $product=<F>;
      chomp $product;
      close(F);
      unlink "$usersDir/$user/ghostpdl/cluster_command.run";
    }
    if (open(F,"<$usersDir/$user/ghostpdl/gs/cluster_command.run")) {
      $product=<F>;
      chomp $product;
      close(F);
      unlink "$usersDir/$user/ghostpdl/gs/cluster_command.run";
    }
    if ($product) {
      open(F,">>user.run");
      print F "$product\n";
      close(F);
    }
  }
  closedir DIR;

if (open(F,"<$runningSemaphore")) {
  close(F);
  my $fileTime = stat($runningSemaphore)->mtime;
  my $t=time;
  if ($t-$fileTime>7200) {
    print "semaphore file too old, removing\n";
    open(F,">status");
    print F "Regression terminated due to timeout";
    close(F);
    unlink $runningSemaphore;
  }
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
print F "$$\n";
close(F);

my $product="";


#gs/base, gs/Resource: all languages
#gs/psi: ps, pdf
#psi: language switch
#pl, main, common: pcl, pxl, xps, svg, language switch
#pcl, pxl, urwfonts: pcl, pxl
#xps: xps
#svg: svg

# svg: 1000
# xps: 0100
# pcl: 0010
# ps : 0001

my %tests=(
  'gs'  => 1,
  'pcl' => 2,
  'xps' => 4,
  'svg' => 8
);


my %rules=(
 'xps' => 4,
 'svg' => 8,
 'pcl' => 2,
 'pxl' => 2,
 'urwfonts' => 2,
 'pl' => 15,
 'main' => 15,
 'common' => 15,
 'gs/psi' => 1,
 'gs/base' => 15,
 'gs/Resource' => 15
);


my $currentRev1=`/usr/local/bin/svn info ghostpdl | grep "Last Changed Rev" | awk '{ print \$4} '`;
my $currentRev2=`/usr/local/bin/svn info ghostpdl/gs | grep "Last Changed Rev" | awk '{ print \$4} '`;

$footer.="\nupdated files:\n";
my $a=`/usr/local/bin/svn update ghostpdl`;
#print "$a";
my @a=split '\n',$a;
my $set=0;
for (my $i=0;  $i<scalar(@a);  $i++) {
  my $s=$a[$i];
  chomp $s;

  if ($s =~ m|/|) {
#   print "$s\n";
    $s=~ s|ghostpdl/||;
    if ($s =~ m|. +(.+)/|) {
      print "$s\n";
$footer.="$s\n";
      my $t=$1;
      print "$t ";
      $t="gs/$1" if ($t =~ m|gs/(.+?)/|);
print "($t) ";
      if (exists $rules{$t}) {
        print "$rules{$t}";
        $set|=$rules{$t};
      } else {
        print "missing";
      }
      print "\n";
    }
  }
}

#print "$set\n";
foreach my $i (keys %tests) {
  $product .= "$i " if ($set & $tests{$i});
}
print "$product\n" if (length($product) && $verbose);

$footer.="\nproducts to be tested: $product\n";

# $product="gs pcl xps svg";

#`/usr/local/bin/svn update ghostpdl`;
my $newRev1=`/usr/local/bin/svn info ghostpdl | grep "Last Changed Rev" | awk '{ print \$4} '`;
my $newRev2=`/usr/local/bin/svn info ghostpdl/gs | grep "Last Changed Rev" | awk '{ print \$4} '`;

chomp $currentRev1;
chomp $currentRev2;
chomp $newRev1;
chomp $newRev2;
#print "$currentRev1 $currentRev2 $newRev1 $newRev2\n";

my $normalRegression=0;
my $userRegression="";
my $userName="";

if (length($product)>0 && $currentRev1!=0 && $currentRev2!=0 && ($currentRev1!=$newRev1 || $currentRev2!=$newRev2)) {
  $normalRegression=1;
} else {

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

# if ($normalRegression) {
    open(F,">revision");
    print F "local cluster regression gs-r$newRev2 / ghostpdl-r$newRev1 (xefitra)\n";
    close(F);
# }

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

    if (scalar keys %machines==0) {
      unlink $runningSemaphore;
      die "There aren't any cluster machines available"
    }

    if (open(F,"<machinespeeds.txt")) {
      while(<F>) {
        chomp;
        my @a=split '\s';
        $machineSpeeds{$a[0]}=$a[1];
      }
      close(F);
    }

    checkPID();
    my $options="";
    foreach (sort {lc $a cmp lc $b} keys %machines) {
      $machineSpeeds{$_}=1 if (!exists $machineSpeeds{$_});
      $options.=" $_.jobs $machineSpeeds{$_}";
    }
    print "$options\n" if ($verbose);
    if (!$normalRegression) {
      my @a=split ' ',$userRegression,2;
      $userName=$a[0];
#     $product="gs pcl xps svg"; # always test everything for users
      $product=$a[1];
      print "userName=$userName product=$product\n" if ($verbose);
    }
    `./build.pl $product >jobs`;
    if ($? != 0) {
      # horrible hack, fix later
      unlink  $runningSemaphore;
      exit;
    }
    `./splitjobs.pl jobs $options`;
    unlink "jobs";

    checkPID();
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

    checkPID();
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
      checkPID();
      foreach my $m (keys %machines) {
        if (!stat("$m.up") || (time-stat("$m.up")->ctime)>=$maxDownTime) {
          print "$m is down\n" if ($verbose);
          $abort=1;
          %doneTime=();  # avoids a race condition where the machine we just aborted reports done
          foreach my $n (keys %machines) {
            `touch $n.abort`;
	  }
          delete $machines{$m};
	  sleep 60;  # hack
        } else {
          if (open(F,"<$m.done")) {
            close(F);
            if ($verbose) {
              print "$m is reporting done\n" if (!exists $doneTime{$m});
            }
            $doneTime{$m}=time if (!exists $doneTime{$m});
          }
        }
      }
      sleep(1);
    }
    print "abort=$abort\n" if ($verbose);
  } while ($abort);

  my $elapsedTime=time-$startTime;

  checkPID();
  $s=`date +\"%H:%M:%S\"`;
  chomp $s;
  open (F,">status");
  if ($normalRegression) {
    print F "Regression gs-r$newRev2 / ghostpdl-r$newRev1 started at $startText - finished at $s";
  } else {
    print F "Regression $userRegression started at $startText - finished at $s";
  }
  close(F);

  checkPID();
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
    my $totalSpeed=0;
    my $totalTime=0;
    my $max=0;
    foreach (keys %machines) {
      $totalSpeed+=$machineSpeeds{$_};
      $totalTime+=$doneTime{$_}-$startTime;
    }
print "totalSpeed=$totalSpeed totalTime=$totalTime\n" if ($verbose);
    foreach (keys %machines) {
printf "%s %f %f %f ",$_,($machineSpeeds{$_}/$totalSpeed),(($doneTime{$_}-$startTime)/$totalTime),$machineSpeeds{$_} if ($verbose);
      $machineSpeeds{$_}=($machineSpeeds{$_}/$totalSpeed)/(($doneTime{$_}-$startTime)/$totalTime);
printf "%f\n",$machineSpeeds{$_} if ($verbose);
      $max=$machineSpeeds{$_} if ($max<$machineSpeeds{$_});
    }
    if (0) {
    foreach (keys %machines) {
printf "%s %f %f\n",$_,$doneTime{$_}-$startTime,$machineSpeeds{$_} if ($verbose);
      $machineSpeeds{$_}=$machineSpeeds{$_}*($shortestTime/($doneTime{$_}-$startTime));
printf "%s %f %f\n",$_,$doneTime{$_}-$startTime,$machineSpeeds{$_} if ($verbose);
      $max=$machineSpeeds{$_} if ($max<$machineSpeeds{$_});
    }
    }
    foreach (keys %machines) {
      $machineSpeeds{$_}/=$max;
      $machineSpeeds{$_}=(int($machineSpeeds{$_}*100+0.5))/100;
      $machineSpeeds{$_}=0.01 if ($machineSpeeds{$_}==0);
printf "%s %f %f\n",$_,$doneTime{$_}-$startTime,$machineSpeeds{$_} if ($verbose);
    }
    print Dumper(\%machineSpeeds);
    if (open(F,">machinespeeds.txt")) {
      foreach (sort keys %machineSpeeds) {
        print F "$_ $machineSpeeds{$_}\n";
      }
      close(F);
    }
  }

  checkPID();
  my $buildFail=0;  # did at least one machine report a build failure?
  my %buildFail;    # list of machines that did
  my $failMessage="";
  my $logs="";
  my $tabs="";
  foreach (keys %machines) {
    `touch $_.log; rm -f $_.log; gunzip $_.log.gz`;
    `touch $_.out; rm -f $_.out; gunzip $_.out.gz`;
    my $a=`./readlog.pl $_.log $_.tab $_ $_.out`;
    if ($a ne "") {
      chomp $a;
      print "$_: $a\n" if ($verbose);
      $buildFail=1;
      $failMessage.="$_ reports: $a\n";
      $buildFail{$_}=1;
    }
    $logs.=" $_.log $_.out";
    $tabs.=" $_.tab";
  }

  checkPID();
  $userName="email" if ($normalRegression);
  if (!$buildFail) {

    my $machineCount=scalar (keys %machines);
    if ($normalRegression) {
      # unlink "log";
      my @a=split ' ',$product;
      my $filter="cat current.tab";
      foreach (@a) {
        $filter.=" | grep -v \"\t$_\t\"";
      }
      $filter.=">t.tab";
      print "$filter\n" if ($verbose);
      `$filter`;

      `mv previous.tab previous2.tab`;
      `mv current.tab previous.tab`;
      `cat $tabs t.tab | sort >current.tab`;
      `rm $tabs`;
      #  `cat $logs >log`;
      #  `./readlog.pl log current.tab`;

      checkPID();
      `./compare.pl current.tab previous.tab $elapsedTime $machineCount >email.txt`;
     #  `mail marcos.woehrmann\@artifex.com -s \"\`cat revision\`\" <email.txt`;

      checkPID();
      `touch archive/$newRev2-$newRev1`;
      `rm -fr archive/$newRev2-$newRev1`;
      `mkdir archive/$newRev2-$newRev1`;
      `mv $logs archive/$newRev2-$newRev1/.`;
      `gzip archive/$newRev2-$newRev1/*log`;
      `cp -p email.txt archive/$newRev2-$newRev1/.`;
      `cp -p current.tab archive/$newRev2-$newRev1/current.tab`;
      `cp -p previous.tab archive/$newRev2-$newRev1/previous.tab`;
      `cp -p current.tab archive/$newRev2-$newRev1.tab`;
      #  `touch archive/$newRev2-$newRev1.tab.gz`;
      #  unlink "archive/$newRev2-$newRev1.tab.gz";
      #  `gzip archive/$newRev2-$newRev1.tab`;
      # unlink "log";
    } else {
      my @a=split ' ',$product;
      my $filter="cat current.tab";
      foreach (@a) {
        $filter.=" | grep -v \"\t$_\t\"";
      }
      $filter.=">t.tab";
      print "$filter\n" if ($verbose);
      `$filter`;

      `cat $tabs t.tab | sort >temp.tab`;
      `rm $tabs`;

      checkPID();
      `./compare.pl temp.tab current.tab $elapsedTime $machineCount true >$userName.txt`;
      `mv $logs users/$userName/.`;
      `cp -p $userName.txt users/$userName/.`;
      `cp -p temp.tab users/$userName/.`;
      `cp -p current.tab users/$userName/.`;

    }
  } else {
    checkPID();
    open (F,">$userName.txt");
    print F "An error occurred that prevented the local cluster run from finishing:\n\n$failMessage\n";
    foreach my $machine (keys %machines) {
      if ($buildFail{$machine}) {
        print F "\n\n$machine log:\n\n";
	`tail -100 $machine.log >temp.log`;
        open(F2,"<temp.log");
        while(<F2>) {
          print F $machine;
        }
        close(F2);
        print F "\n\n$machine stdout:\n\n";
	`tail -100 $machine.out >temp.out`;
        open(F2,"<temp.out");
        while(<F2>) {
          print F $machine;
        }
        close(F2);
      }
    }
    close(F);
  }

  {
    my @t=split '\n',$footer;
    open(F,">>$userName.txt");
    foreach (@t) {
      print F "$_\n";
    }
    close(F);
  }

  checkPID();
  if ($normalRegression) {
    `mail -a \"From: marcos.woehrmann\@artifex.com\" gs-regression\@ghostscript.com -s \"\`cat revision\`\" <email.txt`;
#   `mail -a \"From: marcos.woehrmann\@artifex.com\" marcos\@ghostscript.com -s \"\`cat revision\`\" <email.txt`;
    `./cp.all`;
  } else {
    if (exists $emails{$userName}) {
#     `mail -a \"From: marcos.woehrmann\@artifex.com\" marcos.woehrmann\@artifex.com -s \"$userRegression regression\" <$userName.txt`;
#     `mail -a \"From: marcos.woehrmann\@artifex.com\" $emails{$userName} -s \"$userRegression \`cat revision\`\" <$userName.txt`;
      `mail marcos.woehrmann\@artifex.com -s \"$userRegression regression\" <$userName.txt`;
      `mail $emails{$userName} -s \"$userRegression \`cat revision\`\" <$userName.txt`;
    } else {
#     `mail -a \"From: marcos.woehrmann\@artifex.com\" marcos.woehrmann\@artifex.com -s \"bad username: $userName\" <$userName.txt`;
      `mail marcos.woehrmann\@artifex.com -s \"bad username: $userName\" <$userName.txt`;
    }
  }

}

checkPID();
unlink $runningSemaphore;
