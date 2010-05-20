#!/usr/bin/perl

use strict;
use warnings;

use Time::localtime;
use File::stat;
use Fcntl qw(:flock);

use Data::Dumper;

my $verbose=1;


my $runningSemaphore="./clustermaster.pid";
my $queue="./queue.lst";
my $lock="./queue.lck";
my $usersDir="users";
my $jobs="./jobs";

my $maxTouchTime=300;     # how many seconds after the last touch before a machine is considered down
my $maxTransferTime=600;  # how many seconds after the last transfer before a machine is considered down

my $jobsPerRequest=250;

my %machines;


my $footer="";

sub mylog($) {
  my $d=`date`;
  chomp $d;
  my $s=shift;
  chomp $s;
  print "$d: $s\n";
}

sub updateStatus($) {
  my $s=shift;
  open(F,">status");
  print F "$s";
  close(F);
}

sub removeQueue {
  mylog "removing top element from queue\n";
  open(LOCK,">$lock") || die "can't write to $lock";
  flock(LOCK,LOCK_EX);
  if (open(F,"<$queue")) {
    my @a;
    while(<F>) {
      chomp;
      push @a,$_;
    }
    close(F);
    open(F,">$queue");
    for (my $i=1;  $i<scalar(@a);  $i++) {
      print F "$a[$i]\n";
    }
    close(F);
  }
  close(LOCK);
}

sub abortAll {
mylog "aborting all machines\n";
  my $startTime=time;
  foreach my $m (keys %machines) {
mylog "touching $m.abort\n";
    `touch $m.abort`;
  }
  my $done=0;
  my %doneTable;
  while (!$done) {
    $done=1;
    foreach my $m (keys %machines) {
      if (-e "$m.abort") {
        $done=0;
      } else {
        mylog "$m.abort removed\n" if (!exists $doneTable{$m});
        $doneTable{$m}=1;
      }
    }
    $done=1 if (time-$startTime>120);
    sleep 1;
  }
  sleep 5;  # allow final machine to set status
  foreach my $machine (keys %machines) {
    open(F,">$machine.status");
    print F "idle\n";
    close(F);
  }

}


sub checkAbort {
  if (-e "abort.job") {
mylog "abort.job found\n";
    alarm 0;
    unlink "abort.job";
    updateStatus "Aborting current job";

#   removeQueue();  not necessary
    abortAll();

    updateStatus "Last job aborted";
    unlink $runningSemaphore;
    exit;

  }
}



# the concept with checkPID is that if the semaphore file is missing or doesn't
# contain our PID something bad has happened and we just just exit
sub checkPID {
  checkAbort();
  if (open(F,"<$runningSemaphore")) {
    my $t=<F>;
    close(F);
    chomp $t;
    if ($t == $$) {
      return(0);
    }
    mylog "terminating: $runningSemaphore mismatch $t $$\n";
  } else {
    mylog "terminating: $runningSemaphore missing\n";
  }
  alarm 0;
  open(F,">$runningSemaphore");
  print F "0\n";
  close(F);
  abortAll();
  unlink $runningSemaphore;
  exit;
}

    my %lastTransfer;
    my %sent;
    my $doneCount=0;
    my @jobs;
    my $abort=0;
    my $tempDone=0;
    my $failOccured=0;

sub checkProblem {
      foreach my $m (keys %machines) {
        if (!stat("$m.up") || (time-stat("$m.up")->ctime)>=$maxTouchTime) {
          mylog "machine $m hasn't updated $m.up in $maxTouchTime seconds, assuming it went down\n";
          delete $lastTransfer{$m} if (exists $lastTransfer{$m});
          delete $machines{$m} if (exists $machines{$m});
          if (scalar keys %lastTransfer) {
            `touch $m.abort`;
            if (exists $sent{$m}) {
              mylog "1: adding jobs from $m back into queue: ".scalar(@{$sent{$m}})."\n";
              @jobs=(@jobs,@{$sent{$m}});
            } else {
              mylog "1: $m queue empty, nothing to add back in\n";
            }
          } else {
            $doneCount=scalar keys %machines;
            mylog "1: setting doneCount to $doneCount\n";
            $abort=1;
          }
        }
      }
      foreach my $m (keys %lastTransfer) {
        if (time-$lastTransfer{$m}>=$maxTransferTime) {
          mylog "machine $m hasn't connected in ".(time-$lastTransfer{$m})." seconds, assuming it went down\n";
          unlink "$m.start";  # so it does connect later and try to run jobs
          delete $lastTransfer{$m} if (exists $lastTransfer{$m});
          delete $machines{$m} if (exists $machines{$m});
          if (scalar keys %lastTransfer) {
            `touch $m.abort`;
            if (exists $sent{$m}) {
              mylog "2: adding jobs from $m back into queue: ".scalar(@{$sent{$m}})."\n";
              @jobs=(@jobs,@{$sent{$m}});
            } else {
              mylog "2: $m queue empty, nothing to add back in\n";
            }
          } else {
            $doneCount=scalar keys %machines;
            mylog "2: setting doneCount to $doneCount\n";
            unlink "$m.up";
            $abort=1;
          }

        }
      }

      foreach my $m (keys %machines) {
        if (-e "$m.done" && exists $lastTransfer{$m}) {
          unlink "$m.start";  # so it does connect later and try to run jobs
          if (-e "$m.fail") {
            mylog "both $m.done and $m.fail are set, ignoring $m.done\n";
          } else {
            mylog "$m is reporting done even though it should not be done\n";
            delete $lastTransfer{$m} if (exists $lastTransfer{$m});
            delete $machines{$m} if (exists $machines{$m});
            $tempDone=1;
            $abort=1;
          }
        }
        if (-e "$m.fail") {
          mylog "$m.fail is set\n";
          $failOccured=1;
        }
      }

}

{
  my $url1=`svn info ghostpdl | grep "URL" | awk '{ print \$2 } '`;
  my $url2=`svn info ghostpdl/gs | grep "URL" | awk '{ print \$2 } '`;
  chomp $url1;
  chomp $url2;
  my $currentRev1=`svn info ghostpdl | grep "Last Changed Rev" | awk '{ print \$4 } '`;
  my $currentRev2=`svn info ghostpdl/gs | grep "Last Changed Rev" | awk '{ print \$4 } '`;
  my $newRev1    =`svn info $url1 | grep "Last Changed Rev" | awk '{ print \$4 } '`;
  my $newRev2    =`svn info $url2 | grep "Last Changed Rev" | awk '{ print \$4 } '`;
  chomp $currentRev1;
  chomp $currentRev2;
  chomp $newRev1;
  chomp $newRev2;
  if ($currentRev1 eq "" || $currentRev2 eq "" || $newRev1 eq "" || $newRev2 eq "") {
    checkPID();
    unlink $runningSemaphore;
    die "svn info failed";
  }
  if ($currentRev1 != $newRev1 || $currentRev2 != $newRev2) {
    #   print "$currentRev1 $newRev1\n$currentRev2 $newRev2\n";
    open(LOCK,">$lock") || die "can't write to $lock";
    flock(LOCK,LOCK_EX);
    my $rev=$newRev1;
    $rev=$newRev2 if ($newRev2>$rev);
    my $s="svn $rev";
    my $t=`grep "$s" $queue`;
    chomp $t;
    #   print "grep '$s' returned: $t\n";
    if (length($t)==0) {
      mylog "grep '$s' returns no match, adding to queue\n";
      open(F,">>$queue");
      print F "$s\n";
      close(F);
    }
    close(LOCK);
  }
}

{
  my $url1=`svn info icc_work | grep "URL" | awk '{ print \$2 } '`;
  chomp $url1;
  my $currentRev1=`svn info icc_work | grep "Last Changed Rev" | awk '{ print \$4 } '`;
  my $newRev1    =`svn info $url1 | grep "Last Changed Rev" | awk '{ print \$4 } '`;
  chomp $currentRev1;
  chomp $newRev1;
  if ($currentRev1 eq "" || $newRev1 eq "") {
    checkPID();
    unlink $runningSemaphore;
    die "svn info failed";
  }
  if ($currentRev1 != $newRev1) {
    #   print "$currentRev1 $newRev1\n";
    open(LOCK,">$lock") || die "can't write to $lock";
    flock(LOCK,LOCK_EX);
    my $s="svn-icc_work $newRev1";
    my $t=`grep "$s" $queue`;
    chomp $t;
    #   print "grep '$s' returned: $t\n";
    if (length($t)==0) {
      mylog "grep '$s' returns no match, adding to queue\n";
      open(F,">>$queue");
      print F "$s\n";
      close(F);
    }
    close(LOCK);
  }
}


if (0) {
  my $cmd="cd mupdf ; darcs pull -a";
  my $value=`$cmd`;
  chomp $value;
  if ($value =~ "No remote changes") {
  } else {
    open(LOCK,">$lock") || die "can't write to $lock";
    flock(LOCK,LOCK_EX);
    my $s="mupdf";
    my $t=`grep "$s" $queue`;
    chomp $t;
    #   print "grep '$s' returned: $t\n";
    if (length($t)==0) {
      mylog "grep '$s' returns no match, adding to queue\n";
      open(F,">>$queue");
      print F "$s\n";
      close(F);
    }
    close(LOCK);
  }
}

{
  opendir(DIR, $usersDir) || die "can't opendir $usersDir: $!";
  foreach my $user (readdir(DIR)) {
    my $product="";
    my $options;
    my $s="";
    if (open(F,"<$usersDir/$user/gs.run")) {
      close(F);
      unlink "$usersDir/$user/gs.run";
      $product="$user gs pcl xps svg ls";
    }
    if (open(F,"<$usersDir/$user/ghostpdl.run")) {
      close(F);
      unlink "$usersDir/$user/ghostpdl.run";
      $product="$user gs pcl xps svg ls";
    }
    if (open(F,"<$usersDir/$user/ghostpdl/cluster_command.run")) {
      $product=<F>;
      chomp $product;
      $options=<F>;
      chomp $options;
      close(F);
      unlink "$usersDir/$user/ghostpdl/cluster_command.run";
    }
    if (open(F,"<$usersDir/$user/ghostpdl/gs/cluster_command.run")) {
      $product=<F>;
      chomp $product;
      $options=<F>;
      chomp $options;
      close(F);
      unlink "$usersDir/$user/ghostpdl/gs/cluster_command.run";
    }
    $options="" if (!$options);
    if ($product) {
      $product =~ s/ +$//;
      mylog "user $product\n";
      open(LOCK,">$lock") || die "can't write to $lock";
      flock(LOCK,LOCK_EX);
      if ($product =~  m/abort$/) {
        if ($product =~ m/^(.+) /) {
          $user = $1;
mylog "abort for user $user\n";
          if (open(F,"<$queue")) {
            my $first=1;
            my @a;
            while(<F>) {
              chomp;
              if (! m/^user $user/) {
                push @a,$_ 
              } else {
                if ($first) {
mylog "setting 'abort.job'\n";
                  `touch abort.job`;
                }
              }
              $first=0;
            }
            close(F);
            open(F,">$queue");
            for (my $i=0;  $i<scalar(@a);  $i++) {
              print F "$a[$i]\n";
            }
            close(F);
          }
        }
      } else {
        my $s="user $product";
        my $t=`grep "$s" $queue`;
        chomp $t;
        mylog "grep '$s' returned: $t\n";
        if (length($t)==0) {
          mylog "grep '$s' returns no match, adding to queue\n";
          open(F,">>$queue");
          print F "$s $options\n";
          close(F);
          mylog "running: find $usersDir/$user/ghostpdl -name \\*.sh | xargs \$HOME/bin/flip -u\n";
          `find $usersDir/$user/ghostpdl -name \\*.sh | xargs \$HOME/bin/flip -u`;
          mylog "running: find $usersDir/$user/ghostpdl -name instcopy | xargs \$HOME/bin/flip -u\n";
          `find $usersDir/$user/ghostpdl -name instcopy | xargs \$HOME/bin/flip -u`;
          mylog "running: chmod -R +xr $usersDir/$user/ghostpdl\n";
          `chmod -R +xr $usersDir/$user/ghostpdl`;
        }
        close(LOCK);
      }
    }
  }
  closedir DIR;
}

if (open(F,"<$runningSemaphore")) {
  my $pid=<F>;
  close(F);
  my $fileTime = stat($runningSemaphore)->mtime;
  my $t=time;
  if ($t-$fileTime>7200) {
    mylog "semaphore file too old, removing\n";
    updateStatus "Regression terminated due to timeout";
    unlink $runningSemaphore;
    sleep 60;
    exit;   # we pause and then exit here since we have to wait until the current clustermaster realizes we've removed the semaphore
  }
  chomp $pid;
  my $running=`ps -p $pid`;
  if ($running =~ m/clustermaster/) {
    exit;
  } else {
    mylog "process $pid no longer running, removing semaphore\n";
    updateStatus "Regression terminated due to missing clustermaster.pl process";
    unlink $runningSemaphore;
  }
}

open(F,">$runningSemaphore");
print F "$$\n";
close(F);

checkPID();

my %emails;
if (open(F,"<emails.tab")) {
  while(<F>) {
    chomp;
    my @a=split '\t';
    $emails{$a[0]}=$a[1];
  }
  close(F);
}

my $product="";
my $options="";

#gs/base, gs/Resource: all languages
#gs/psi: ps, pdf
#psi: language switch
#pl, main, common: pcl, pxl, xps, svg, language switch
#pcl, pxl, urwfonts: pcl, pxl
#xps: xps
#svg: svg

# ls : 10000
# svg: 01000
# xps: 00100
# pcl: 00010
# ps : 00001

my %tests=(
  'gs'  =>  1,
  'pcl' =>  2,
  'xps' =>  4,
  'svg' =>  8,
  'ls'  => 16
);

my $allTests=31;

# ls is tested if any test is called for

my %rules=(
  'xps' => 4,
  'svg' => 8,
  'pcl' => 2,
  'pxl' => 2,
  'language_switch' => 16,
  'urwfonts' => 2,
  'pl' => 14,
  'main' => 2,
  'common' => 14,
  'gs/psi' => 1,
  'gs/base' => 15,
  'gs/Resource' => 15,
  'gs/doc' => 0,
  'gs/toolbin' => 0,
  'gs/examples' => 0,
  'gs/lib' => 0,
  'gs/freetype' => 15,
  'language_switch' => 0,
  'tools' => 0,
  'win32' => 0,
  'gs/contrib' => 0,
  'psi' => 16,
  'gs/jasper' => 15,
  'gs/cups' =>0
);

#my $currentRev1=`svn info ghostpdl | grep "Last Changed Rev" | awk '{ print \$4} '`;
#my $currentRev2=`svn info ghostpdl/gs | grep "Last Changed Rev" | awk '{ print \$4} '`;
#chomp $currentRev1;
#chomp $currentRev2;

my $regression;

open(LOCK,">$lock") || die "can't write to $lock";
flock(LOCK,LOCK_EX);
if (open(F,"<$queue")) {
  $regression=<F>;
  close(F);
}
close(LOCK);

if (!$regression) {
  checkPID();
  unlink $runningSemaphore;
  exit;
}

my $normalRegression=0;
my $icc_workRegression=0;
my $userRegression="";
my $bmpcmp=0;
my $mupdfRegression=0;
my $updateBaseline=0;
my $userName="";
my $rev;

if ($regression =~ m/svn (\d+)/) {
  mylog "found svn regression in queue: $regression\n";
  $normalRegression=1;
  $rev=$1;

  my $currentRev1=`svn info ghostpdl | grep "Last Changed Rev" | awk '{ print \$4 } '`;
  my $currentRev2=`svn info ghostpdl/gs | grep "Last Changed Rev" | awk '{ print \$4 } '`;
  chomp $currentRev1;
  chomp $currentRev2;

  my $a=`svn update ghostpdl -r$rev --ignore-externals`;
  my $b=`svn update ghostpdl/gs -r$rev`;
  mylog "svn update ghostpdl -r$rev --ignore-externals\n" if ($verbose);
  mylog "svn update ghostpdl/gs -r$rev\n" if ($verbose);

  $footer.="\nChanged files:\n";
  $a.=$b;
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
        $footer.="$s\n";
        my $t=$1;
        $t="gs/$1" if ($t =~ m|gs/(.+?)/|);
        if (exists $rules{$t}) {
          mylog "$s: $rules{$t}\n";
          $set|=$rules{$t};
        } else {
          mylog "$s ($t): missing, testing all\n";
          $set|=$allTests;
        }
      } else {
        mylog "unknown commit: testing all\n";
        $set|=$allTests;
      }
    }
  }
  $set|=$tests{'ls'} if ($set>0);  # we test the language_switch build if anything has changed

  #print "$set\n";
  foreach my $i (sort keys %tests) {
    $product .= "$i " if ($set & $tests{$i});
  }

  $product =~ s/svg//;  # disable svg tests
  $product =~ s/  / /g; # get rid of extra space left by previous line
  # $product="gs pcl xps svg ls";

  mylog "products: $product\n";

  $footer.="\nProducts tested: $product\n\n";

  if (length($product)) {
# un-update the source so that if the regression fails were are back to the where we started
    `svn update ghostpdl -r$currentRev1 --ignore-externals`;
    `svn update ghostpdl/gs -r$currentRev2`;
    mylog "svn update ghostpdl -r$currentRev1 --ignore-externals\n";
    mylog "svn update ghostpdl/gs -r$currentRev2\n";
  } else {
    mylog "no interesting files changed, skipping regression\n";
    $normalRegression=0;
  }

} elsif ($regression=~/user (.+)/) {
  mylog "found user regression in queue: $regression\n";
  $userRegression=$1;
} elsif ($regression=~/svn-icc_work (.+)/) {
  mylog "found icc_work regression in queue: $regression\n";
  $icc_workRegression=1;
  $rev=$1;
  $product="gs pcl xps ls";
  $footer.="icc_work regression: $rev\n\nProducts tested: $product\n\n";
} elsif ($regression=~/mupdf/) {
  mylog "found mupdf entry in queue.lst\n";
  my $cmd="touch mupdf.tar.gz ; rm mupdf.tar.gz ; tar cvf mupdf.tar --exclude=_darcs mupdf ; gzip mupdf.tar";
  `$cmd`;
  $cmd="cd mupdf ; darcs changes --count";
  $rev=`$cmd`;
  chomp $rev;
$mupdfRegression=1;
  $product="mupdf";
} elsif ($regression=~/updatebaseline/) {
  mylog "found updatebaseline in regression: $regression\n";
  $updateBaseline=1;
} else {
  mylog "found unknown entry in queue.lst, removing.\n";
}

$product =~ s/\s+$//;
$userRegression =~ s/\s+$//;

if ($normalRegression==1 || $userRegression ne "" || $mupdfRegression==1 || $updateBaseline==1 || $icc_workRegression==1) {

  if ($userRegression ne "") {
    mylog "running: $userRegression\n" if ($verbose);
  }

  if ($normalRegression) {
    open(F,">revision.gs");
    print F "local cluster regression r$rev (xefitra)\n";
    close(F);
  }

  if ($icc_workRegression) {
    open(F,">icc_workRevision.gs");
    print F "local cluster regression icc_work-r$rev (xefitra)\n";
    close(F);
  }

  if ($mupdfRegression) {
    open(F,">revision.mudpf");
    print F "local cluster regression mupdf-r$rev (xefitra)\n";
    close(F);
  }

  if ($updateBaseline) {
    mylog "running: updateBaseline\n" if ($verbose);
  }

  my @machines = <*.up>;
  mylog "@machines\n" if ($verbose);
  foreach (@machines) {
    # print "$_\n";
    # my $t=time-stat("$_")->ctime;
    # print "$_ $t\n";
    $machines{$_}=1 if (stat("$_") && (time-stat("$_")->ctime)<$maxTouchTime);
  }
  foreach (keys %machines) {
    delete $machines{$_};
    s/.up//;
    $machines{$_}=1;
  }
  foreach (keys %machines) {
    delete $machines{$_} if (stat("$_.down"));
  }

# mylog Dumper(\%machines) if ($verbose);
  if ($verbose) {
    foreach (sort keys %machines) {
      mylog("  $_\n");
    }
  }

  my $startTime;
  my %doneTime;
  my $s;
  my $startText;

  $abort=0;
  do {

    mylog "running with ".(scalar keys %machines)." machines\n" if ($verbose);

    if (scalar keys %machines<=2) {
      sleep 600;
      unlink $runningSemaphore;
      die "There aren't enough cluster machines available"
    }

    checkPID();

    if ($userRegression ne "") {
      my @a=split ' ',$userRegression,2;
      $userName=$a[0];
      $product=$a[1];
      if ($product=~m/^bmpcmp/) {
        $bmpcmp=1;
        my @a=split ' ',$product,2;
        if ($userName eq "mvrhel") {
          $product="bmpcmp_icc_work bmpcmp $userName";
        } else {
          $product="bmpcmp $userName";
        }
        $options=$a[1];
        $options="" if (!$options);
#     print "userName=$userName product=$product options=$options\n"; exit;
      }
      mylog "userName=$userName product=$product options=$options\n" if ($verbose);
      my $t=`date +\"%D %H:%M:%S\"`;
      chomp $t;
      $footer="\n\nUser regression: user $userName  options $product $options start $t\n";
    }

    my $baseline="";
    $baseline="baseline" if ($updateBaseline);  # mhw

    `./build.pl $product $baseline \"$options\" >$jobs`;
    if ($? != 0) {
      # horrible hack, fix later
      mylog "build.pl $product failed\n";
      unlink $runningSemaphore;
      exit;
    }

if ($bmpcmp) {
`head -1000 jobs >jobs.tmp ; mv jobs.tmp jobs`;
        my $gs="";
        my $pcl="";
        my $xps="";
        my $ls="";
        my $gsBin="gs/bin/gs";
        my $pclBin="gs/bin/pcl6";
        my $xpsBin="gs/bin/gxps";
        my $lsBin="gs/bin/pspcl6";
        if (open(F2,"<jobs")) {
          while (<F2>) {
            chomp;
            $gs="gs " if (m/$gsBin/);
            $pcl="pcl " if (m/$pclBin/);
            $xps="xps " if (m/$xpsBin/);
            $ls="ls " if (m/$lsBin/);
          }
          close(F2);
          $product=$gs.$pcl.$xps.$ls;
        }
mylog "done checking jobs, product=$product\n";
        `touch bmpcmp.tmp ; rm -fr bmpcmp.tmp ; mv bmpcmp bmpcmp.tmp ; mkdir bmpcmp ; rm -fr bmpcmp.tmp &`;
}

    checkPID();
    foreach (keys %machines) {
      mylog "unlinking $_.done\n" if ($verbose);
      mylog "unlinking $_.fail\n" if ($verbose);
      mylog "unlinking $_.abort\n" if ($verbose);
      mylog "writing $_.start\n" if ($verbose);
      unlink("$_.done");
      unlink("$_.fail");
      unlink("$_.abort");
      if (-e "$_.start") {
            alarm 0;
            mylog "$_.start exists when it shouldn't, aborting\n";
            foreach my $m (keys %machines) {
              unlink "$m.start";
            }
            abortAll();
            unlink $runningSemaphore;
            exit;
      }
      open(F,">$_.start");
      if ($normalRegression) {
        print F "svn\t$rev\t$product\n";
      } elsif ($icc_workRegression) {
        print F "svn-icc_work\t$rev\t$product";
      } elsif ($mupdfRegression) {
        print F "mupdf\t$rev";
      } elsif ($updateBaseline) {
        print F "svn\thead\t$product";
      } elsif ($bmpcmp) {
        print F "user\t$userName\t$product\n";
      } else {
        print F "user\t$userName\t$product\n";
      }
      close(F);
    }

    checkPID();
    $startText=`date +\"%D %H:%M:%S\"`;
    chomp $startText;
    if ($normalRegression) {
      updateStatus "Regression r$rev ($product) started at $startText UTC";
    } elsif ($icc_workRegression) {
      updateStatus "Regression icc_work-r$rev started at $startText UTC";
    } elsif ($mupdfRegression) {
      updateStatus "Regression mupdf-r$rev started at $startText UTC";
    } elsif ($updateBaseline) {
      updateStatus "Update baseline started at $startText UTC";
    } else {
      updateStatus "Regression $userRegression started at $startText UTC";
      `touch users/$userName/starttime`;
    }

    %doneTime=();
    $abort=0;
    $startTime=time;
    mylog "".(scalar(keys %doneTime))." ".(scalar (keys %machines))."\n" if ($verbose);
#   mylog Dumper(\%machines) if ($verbose);
    if ($verbose) {
      foreach (sort keys %machines) {
        mylog("  $_\n");
      }
    }

    if ($userRegression ne "") {
      if ($userName eq "mvrhel") {
      my $cmd="diff -I '\$Id:' -w -c -r ./icc_work ./users/$userName/ghostpdl/gs | grep -v \"Only in\" > $userName.diff";
      `$cmd`;
      } else {
      my $cmd="diff -I '\$Id:' -w -c -r ./ghostpdl ./users/$userName/ghostpdl | grep -v \"Only in\" > $userName.diff";
      `$cmd`;
      }
    }

    use IO::Socket;
    use Net::hostent;
    my $PORT = 9000;

    my $server = IO::Socket::INET->new(
      Proto     => 'tcp',
      LocalPort => $PORT,
      Listen    => SOMAXCONN,
      Reuse     => 1
      );

    if (!$server) {
      abortAll();
      unlink $runningSemaphore;
      die "can't setup server";
    }

    @jobs=();
    open(F,"<$jobs");
    while(<F>) {
      push @jobs,$_;
    }
    close(F);

    foreach my $m (keys %machines) {
      $lastTransfer{$m}=time+180;  # allow 3 minutes for the machines to build ghostscript, hack
    }

    my $client;
    my $totalJobs=scalar(@jobs);

    $tempDone=0;
    $doneCount=0;
    while (!$tempDone) {

      eval {

        local $SIG{ALRM} = sub { die "alarm\n" };
        alarm 30;

        while (!$failOccured && !$tempDone && ($client = $server->accept())) {
          alarm 30;

          #print "doneCount=$doneCount machines=".(scalar(keys %machines))."\n";
          $SIG{PIPE} = 'IGNORE';
          $client->autoflush(1);
          my $t=<$client>;
          chomp $t;
          if (!exists $lastTransfer{$t}) {
            mylog "received connection from unexpected client $t (".($client->peerhost)."); sending done\n";
            print $client "done\n";
            unlink "$t.start";
          } elsif (-e "$t.start") {
            # if we got here we received a connection from a client who appears to be out of sync
            # (i.e. they are probably still running the previous job).  this is a rare condition
            # and not worth recovering from, just give up.
            alarm 0;
            mylog "$t.start exists when it shouldn't, aborting\n";
            foreach my $m (keys %machines) {
              unlink "$m.start";
            }
            abortAll();
            unlink $runningSemaphore;
            exit;
          } else {
          mylog "Connect from $t (".$client->peerhost.") (".(time-$lastTransfer{$t})." seconds); jobs remaining ".scalar(@jobs)."\n";
          $lastTransfer{$t}=time;
          if (scalar(@jobs)==0) {
            print $client "done\n";
            $doneCount++;
            delete $lastTransfer{$t};
            mylog "sending done: $doneCount\n";
            if ($doneCount==scalar keys %machines) {
              $tempDone=1 ;
              mylog "setting tempDone to 1\n";
            }
          }
          $jobsPerRequest=250;
          $jobsPerRequest= 50 if (scalar(@jobs)<2000);
          $jobsPerRequest= 10 if ($bmpcmp);
          for (my $i=0;  $i<$jobsPerRequest && scalar(@jobs);  $i++) {
            my $a=shift @jobs;
            print $client $a;
            push @{$sent{$t}},$a;
          }
          }
          mylog "Connect finished; jobs remaining ".scalar(@jobs)."\n";
          mylog "not connectecd\n" if (!$client->connected);
          close $client;

          my $percentage=int(($totalJobs-scalar(@jobs))/$totalJobs*100+0.5);
          $s=`date +\"%H:%M:%S\"`;
          chomp $s;
          if ($normalRegression) {
            updateStatus "Regression r$rev ($product) started at $startText UTC - ".($totalJobs-scalar(@jobs))."/$totalJobs sent - $percentage%";
          } elsif ($icc_workRegression) {
            updateStatus "Regression icc_work-r$rev started at $startText UTC - ".($totalJobs-scalar(@jobs))."/$totalJobs sent - $percentage%";
          } elsif ($mupdfRegression) {
            updateStatus "Regression mupdf-r$rev started at $startText UTC - ".($totalJobs-scalar(@jobs))."/$totalJobs sent - $percentage%";
          } elsif ($updateBaseline) {
          } else {
            updateStatus "Regression $userRegression started at $startText UTC - ".($totalJobs-scalar(@jobs))."/$totalJobs sent - $percentage%";
          }
          checkProblem;
          checkPID();
        }
        alarm 0;
      };

      alarm 0;  # avoid race condition

      if ($@) {
        mylog "no connections, checking done status\n";
      }
      checkProblem;
      checkPID();

      if ($doneCount==scalar keys %machines) {
        $tempDone=1 ;
        mylog "all machines are done, setting tempDone to 1\n";
      }

      if ($failOccured) {
        $tempDone=1;
        $abort=0;
        mylog "fail occured, setting tempDone to 1 and abort to 0\n";
        my $startTime=time;
        my $count=0;
        my %tempMachines=%machines;
        while ($count<scalar keys %machines && time-$startTime<60) {
          foreach my $m (keys %machines) {
            if (-e "$m.fail") {
              mylog "$m is reporting fail\n";
              $count++;
              unlink "$m.fail";
              delete $machines{$m};
            }
          }
        }
        if ($count<scalar keys %machines) {
          mylog "aborting all machines\n";
          abortAll();
        }
        %machines=%tempMachines;
      }


    }

    mylog "all machines sent done, some machine went down, or one or more failed\n";
    if ($abort) {
      abortAll();
    }

    my $machineSentDoneTime=time;

    while(!$abort && scalar(keys %doneTime) < scalar(keys %machines)) {
      checkPID();
      if (time-$machineSentDoneTime>=$maxTransferTime) {
        foreach my $m (keys %machines) {
          if (!exists $doneTime{$m}) {
            mylog "machine $m hasn't reported done in ".(time-$machineSentDoneTime)." seconds, assuming it went down\n";
            unlink "$m.up";
          }
        }
      }
      foreach my $m (keys %machines) {
        if (open(F,"<$m.done")) {
          close(F);
          if ($verbose) {
            mylog "$m is reporting done\n" if (!exists $doneTime{$m});
          }
          $doneTime{$m}=time if (!exists $doneTime{$m});
        }
        if (!stat("$m.up") || (time-stat("$m.up")->ctime)>=$maxTouchTime) {
          mylog "$m is down\n" if ($verbose);
          $abort=1;
          %doneTime=();  # avoids a race condition where the machine we just aborted reports done
          abortAll();
          delete $machines{$m};
        }
      }
      sleep(1);
    }
    mylog "abort=$abort\n" if ($verbose);
  } while ($abort && !$failOccured);

  my $elapsedTime=time-$startTime;

  checkPID();
  $s=`date +\"%H:%M:%S\"`;
  chomp $s;
  if ($normalRegression) {
    updateStatus "Regression r$rev ($product) started at $startText UTC - finished at $s";
  } elsif ($icc_workRegression) {
    updateStatus "Regression icc_work-r$rev started at $startText UTC - finished at $s";
  } elsif ($mupdfRegression) {
    updateStatus "Regression mupdf-r$rev started at $startText UTC - finished at $s";
  } elsif ($updateBaseline) {
    updateStatus "Update baseline started at $startText UTC - finished at $s";
  } else {
    updateStatus "Regression $userRegression started at $startText UTC - finished at $s";
  }

  checkPID();
  my $buildFail=0;  # did at least one machine report a build failure?
  my %buildFail;    # list of machines that did
  my $failMessage="";
  my $logs="";
  my $tabs="";

# sleep(10);

  foreach (keys %machines) {
    if (-e "$_.log.gz" && -e "$_.out.gz") {
      mylog "reading log for $_\n";
      `touch $_.log $_.out`;
      `rm -f $_.log $_.out`;
      `gunzip $_.log.gz $_.out.gz`;
      if (!$failOccured && $?!=0) {
        mylog "$_.log.gz and/or $_.out.gz missing, terminating\n";
        checkPID();
        unlink $runningSemaphore;  # force checkPID() to fail
        checkPID();
        exit;  # unecessary, checkPID() won't return
      }
      my $a=`./readlog.pl $_.log $_.tab $_ $_.out`;
      if ($a ne "") {
        chomp $a;
        mylog "$_: $a\n" if ($verbose);
        $buildFail=1;
        $failMessage.="$_ reports: $a\n";
        $buildFail{$_}=1;
      }
      $logs.=" $_.log $_.out";
      $tabs.=" $_.tab";
    } else {
      if ($failOccured) {
        mylog "Warning: $_.log or $_.out missing (ignoring because failOccured is true)\n";
      } else {
        mylog "ERROR: $_.log or $_.out missing\n";
my $a=`ls -ls *log* *out*`;
mylog "ls:\n$a";
        unlink $runningSemaphore;
        exit;
      }
    }
  }

  checkPID();
  $userName="email" if ($normalRegression);
  $userName="email" if ($icc_workRegression);
  $userName="email" if ($mupdfRegression);

  if (!$bmpcmp) {
    my @t=split '\n',$footer;
    open(F,">$userName.txt");
    foreach (@t) {
      print F "$_\n";
    }
    print F "\n\n";
    close(F);
  }

  if (!$buildFail) {

    my $machineCount=scalar (keys %machines);
    if ($normalRegression) {
      # unlink "log";
      my @a=split ' ',$product;
      my $filter="cat current.tab";
      foreach (@a) {
        $filter.=" | grep -v \"\t$_\t\"";
        $filter.=" | grep -v \"\t$_ pdfwrite\t\"";
      }
      $filter.=">t.tab";
      mylog "$filter\n" if ($verbose);
      `$filter`;
#     my $oldCount=`wc -l t.tab | awk ' { print $1 } '`;

      `mv previous.tab previous2.tab`;
      `mv current.tab previous.tab`;
      `cat $tabs t.tab | sort >current.tab`;
      `rm $tabs`;
      #  `cat $logs >log`;
      #  `./readlog.pl log current.tab`;

      checkPID();
mylog "now running ./compare.pl current.tab previous.tab $elapsedTime $machineCount false \"$product\"\n";
      `./compare.pl current.tab previous.tab $elapsedTime $machineCount false \"$product\" >>email.txt`;
     #  `mail marcos.woehrmann\@artifex.com -s \"\`cat revision.gs\`\" <email.txt`;

      checkPID();
      `touch archive/$rev`;
      `rm -fr archive/$rev`;
      `mkdir archive/$rev`;
      `mv $logs archive/$rev/.`;
      `gzip archive/$rev/*log`;
      `cp -p email.txt archive/$rev/.`;
      `cp -p current.tab archive/$rev/current.tab`;
      `cp -p previous.tab archive/$rev/previous.tab`;
      `cp -p current.tab archive/$rev.tab`;
      #  `touch archive/$rev.tab.gz`;
      #  unlink "archive/$rev.tab.gz";
      #  `gzip archive/$rev.tab`;
      # unlink "log";
    } elsif ($icc_workRegression) {
      `mv icc_work_previous.tab icc_work_previous2.tab`;
      `mv icc_work_current.tab icc_work_previous.tab`;
      `cat $tabs | sort >icc_work_current.tab`;
      `rm $tabs`;

      checkPID();
mylog "now running ./compare.pl icc_work_current.tab icc_work_previous.tab $elapsedTime $machineCount false \"$product\"\n";
      `./compare.pl icc_work_current.tab icc_work_previous.tab $elapsedTime $machineCount false \"$product\" >>email.txt`;

      checkPID();
      `touch archive/icc_work-$rev`;
      `rm -fr archive/icc_work-$rev`;
      `mkdir archive/icc_work-$rev`;
      `mv $logs archive/icc_work-$rev/.`;
      `gzip archive/icc_work-$rev/*log`;
      `cp -p email.txt archive/icc_work-$rev/.`;
      `cp -p icc_work_current.tab archive/icc_work-$rev/icc_work_current.tab`;
      `cp -p icc_work_previous.tab archive/icc_work-$rev/icc_work_previous.tab`;
      `cp -p icc_work_current.tab archive/icc_work-$rev.tab`;
    } elsif ($mupdfRegression) {
      `mv mupdf_previous.tab mupdf_previous2.tab`;
      `mv mupdf_current.tab mupdf_previous.tab`;
      `cat $tabs | sort >mupdf_current.tab`;
      `rm $tabs`;

      checkPID();
mylog "now running ./compare.pl mupdf_current.tab mupdf_previous.tab $elapsedTime $machineCount false \"$product\"\n";
      `./compare.pl mupdf_current.tab mupdf_previous.tab $elapsedTime $machineCount false \"$product\" >>email.txt`;

      checkPID();
      `touch archive/mupdf-$rev`;
      `rm -fr archive/mupdf-$rev`;
      `mkdir archive/mupdf-$rev`;
      `mv $logs archive/mupdf-$rev/.`;
      `gzip archive/mupdf-$rev/*log`;
      `cp -p email.txt archive/mupdf-$rev/.`;
      `cp -p mupdf_current.tab archive/mupdf-$rev/mupdf_current.tab`;
      `cp -p mupdf_previous.tab archive/mupdf-$rev/mupdf_previous.tab`;
      `cp -p mupdf_current.tab archive/mupdf-$rev.tab`;

    } elsif ($updateBaseline) {
    } elsif ($bmpcmp) {
    } else {
      my @a=split ' ',$product;
      my $filter="cat current.tab";
      foreach (@a) {
        $filter.=" | grep -v \"\t$_\t\"";
        $filter.=" | grep -v \"\t$_ pdfwrite\t\"";
      }
      $filter.=">t.tab";
      mylog "$filter\n" if ($verbose);
      `$filter`;
#     my $oldCount=`wc -l t.tab | awk ' { print $1 } '`;

      `cat $tabs t.tab | sort >temp.tab`;
      `rm $tabs`;

      checkPID();

      if ($userName eq "mvrhel") {
        open(F,">>$userName.txt");
        print F "\nComparison made to current icc_work branch md5sums\n\n";
        close(F);
        mylog "now running ./compare.pl temp.tab icc_work_current.tab $elapsedTime $machineCount true \"$product\"\n";
        `./compare.pl temp.tab icc_work_current.tab $elapsedTime $machineCount true \"$product\" >>$userName.txt`;
      } else {
      mylog "now running ./compare.pl temp.tab current.tab $elapsedTime $machineCount true \"$product\"\n";
      `./compare.pl temp.tab current.tab $elapsedTime $machineCount true \"$product\" >>$userName.txt`;
      }

      open(F,">>$userName.txt");
      print F "\n\nDifferences from previous clusterpush:\n\n";
      close(F);
      mylog "now running ./compare.pl temp.tab $usersDir/$userName/temp.tab $elapsedTime $machineCount true \"$product\"\n";
      `./compare.pl temp.tab $usersDir/$userName/temp.tab 1 1 true \"$product\" >>$userName.txt`;

      `mv $logs $usersDir/$userName/.`;
      `cp -p $userName.txt $usersDir/$userName/.`;
      `cp -p $userName.txt results/.`;
      `mv    $usersDir/$userName/temp.tab $usersDir/$userName/previousTemp.tab`;
      `cp -p temp.tab $usersDir/$userName/.`;
      `cp -p current.tab $usersDir/$userName/.`;

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
          print F $_;
        }
        close(F2);
        print F "\n\n$machine stdout:\n\n";
        `tail -100 $machine.out >temp.out`;
        open(F2,"<temp.out");
        while(<F2>) {
          print F $_;
        }
        close(F2);
      }
    }
    if ($normalRegression || $icc_workRegression || $mupdfRegression) {
      my $tempRev=$rev;
      $tempRev="icc_work-$rev" if ($icc_workRegression);
      $tempRev="mupdf-$rev" if ($mupdfRegression);
      `touch archive/$tempRev`;
      `rm -fr archive/$tempRev`;
      `mkdir archive/$tempRev`;
      `mv $logs archive/$tempRev/.`;
      `gzip archive/$tempRev/*log`;
      `cp -p email.txt archive/$tempRev/.`;
    } else {
      `mv $logs $usersDir/$userName/.`;
    }
    close(F);
  }

  checkPID();
  if ($normalRegression) {
    `mail -a \"From: marcos.woehrmann\@artifex.com\" gs-regression\@ghostscript.com -s \"\`cat revision.gs\`\" <email.txt`;
#   `mail marcos.woehrmann\@artifex.com -s \"\`cat revision.gs\`\" <email.txt`;

    mylog "test complete, performing final svn update\n";
    mylog "svn update ghostpdl -r$rev --ignore-externals\nsvn update ghostpdl/gs -r$rev\n";
    `svn update ghostpdl -r$rev --ignore-externals`;
    `svn update ghostpdl/gs -r$rev`;

    `./cp.all.sh`;
mylog("calling cachearchive.pl");
    `./cachearchive.pl >md5sum.cache`;
mylog("finished cachearchive.pl");
  } elsif ($icc_workRegression) {
    `mail -a \"From: marcos.woehrmann\@artifex.com\" gs-regression\@ghostscript.com -s \"\`cat icc_workRevision.gs\`\" <email.txt`;
#   `mail -a \"From: marcos.woehrmann\@artifex.com\" marcos\@ghostscript.com -s \"\`cat icc_workRevision.gs\`\" <email.txt`;

    mylog "test complete, performing final svn update\n";
    mylog "svn update icc_work -r$rev\n";
    `svn update icc_work -r$rev`;

mylog("calling cachearchive.pl");
    `./cachearchive.pl >md5sum.cache`;
mylog("finished cachearchive.pl");

  } elsif ($mupdfRegression) {
  } elsif ($updateBaseline) {
  } elsif ($bmpcmp) {
    if (exists $emails{$userName}) {
      `mail $emails{$userName} -s \"bmpcmp finished\" <$userName.txt`;
      `mail marcos.woehrmann\@artifex.com -s \"bmpcmp finished\" <bmpcmpFinished.txt`;
    }
  } elsif ($userRegression) {
    `echo >>$userName.txt`;
    `echo "Source differences from trunk (first 2500 lines):" >>$userName.txt`;
    `echo >>$userName.txt`;
    `head -2500 $userName.diff >>$userName.txt`;
    if (exists $emails{$userName}) {
#     `mail -a \"From: marcos.woehrmann\@artifex.com\" marcos.woehrmann\@artifex.com -s \"$userRegression regression\" <$userName.txt`;
#     `mail -a \"From: marcos.woehrmann\@artifex.com\" $emails{$userName} -s \"$userRegression \`cat revision.gs\`\" <$userName.txt`;
      `mail $emails{$userName} -s \"$userRegression \`cat revision.gs\`\" <$userName.txt`;
      `mail marcos.woehrmann\@artifex.com -s \"$userRegression \`cat revision.gs\`\" <$userName.txt`;
    } else {
#     `mail -a \"From: marcos.woehrmann\@artifex.com\" marcos.woehrmann\@artifex.com -s \"bad username: $userName\" <$userName.txt`;
      `mail marcos.woehrmann\@artifex.com -s \"bad username: $userName\" <$userName.txt`;
    }
  } else {
    mylog("internal error");
  }

  foreach my $machine (keys %machines) {
    open(F,">$machine.status");
    print F "idle\n";
    close(F);
  }

}

if ($userRegression) {
  `touch users/$userName/stoptime`;
}

checkPID();
removeQueue();

checkPID();
mylog("removing $runningSemaphore");
unlink $runningSemaphore;

if ($bmpcmp) {
  `touch ../public_html/$userName`;
  `rm -fr ../public_html/$userName`;
  `mkdir ../public_html/$userName`;
  `chmod 777 ../public_html/$userName`;
  `cd ../public_html/$userName; ln -s compare.html index.html`;
  `./pngs2html.pl bmpcmp ../public_html/$userName`;
}

