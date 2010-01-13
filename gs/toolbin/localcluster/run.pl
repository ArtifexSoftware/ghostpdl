#!/usr/bin/perl

use strict;
use warnings;

use Data::Dumper;
use POSIX ":sys_wait_h";

use File::stat;

my $updateTestFiles=1;

my $debug=0;
my $debug2=0;
my $verbose=0;

my $wordSize="64";
my $timeOut=240;
my $maxTimeout=20;

my %pids;
my %timeOuts;

my $machine=shift || die "usage: run.pl machine_name";

open (LOG,">>$machine.dbg");
print LOG "\n\n";

sub mylog($) {
  my $d=`date`;
  chomp $d;
  my $s=shift;
  chomp $s;
  print LOG "$d: $s\n";
}


my $user;
my $revs;
#my $product;
my @commands;

my $products;

my $runningSemaphore="./run.pid";

sub spawn;

sub watchdog {
  spawn(0,"ssh -i ~/.ssh/cluster_key regression\@casper.ghostscript.com touch /home/regression/cluster/$machine.up");
}

# the concept with checkPID is that if the semaphore file is missing or doesn't
# contain our PID something bad has happened and we just just exit
sub checkPID {
  watchdog();
  if (open(F,"<$runningSemaphore")) {
    my $t=<F>;
    close(F);
    chomp $t;
    if ($t == $$) {
      return(1);
    }
    mylog "terminating: $runningSemaphore pid mismatch $t $$\n";
    print "terminating: $runningSemaphore pid mismatch $t $$\n";
    exit;
  }
  mylog "terminating: $runningSemaphore missing\n";
  print "terminating: $runningSemaphore missing\n";
  exit;
}

if (-e $runningSemaphore) {
  my $fileTime = stat($runningSemaphore)->mtime;
  my $t=time;
  if ($t-$fileTime>7200) {
    mylog "semaphore file too old, removing\n";
    open(F,">status");
    print F "Regression terminated due to timeout";
    close(F);
    unlink $runningSemaphore;
  }
  exit;
}

open(F,">$runningSemaphore");
print F "$$\n";
close(F);

if (open(F,"<$machine.start")) {
  my $t=<F>;
  chomp $t;
  close(F);
  if ($t) {
    my @a=split '\t', $t;
    if ($a[0] eq "svn") {
      $revs=$a[1];
      $products=$a[2];
    } elsif ($a[0] eq "user") {
      $user=$a[1];
      $products=$a[2];
    } else {
      mylog "oops 3";
      die "oops 3";  # hack
    }
  } else {
    mylog "oops 1";  # hack
    die "oops 1";  # hack
  }
  unlink("$machine.start");
} else {
  mylog "oops 2";  # hack
  die "oops 2";  # hack
}

my $host="casper3.ghostscript.com";

my $desiredRev;

my $maxCount=12;
#$maxCount=16;

my $baseDirectory=`pwd`;
chomp $baseDirectory;
my $usersDir="/home/regression/cluster/users";

my $temp="./temp";
my $temp2="./temp.tmp";

my $gpdlSource=$baseDirectory."/ghostpdl";
my $gsSource=$gpdlSource."/gs";
my $gsBin=$baseDirectory."/gs";

my $abort=0;
unlink ("$machine.abort");

my $compileFail="";
my $md5sumFail=`which md5sum`;
my $timeoutFail="";

#my $md5Command='md5sum';
#if ($md5sumFail eq "") {
#  $md5sumFail=`which md5`;
#  $md5Command='md5';
#}
#if ($md5sumFail eq "") {
#  $md5sumFail=`which /sbin/md5`;
#  $md5Command='/sbin/md5';
#}
if ($md5sumFail) {
  $md5sumFail="";
}  else {
  $md5sumFail="md5sum and md5 missing";
  my $a=`echo \$PATH`;
  $md5sumFail.="\n$a";
}

local $| = 1;

my %testSource=(
  $baseDirectory."/tests/" => '1',
  $baseDirectory."/tests_private/" => '1'
  );

system("date") if ($debug2);

if (0) {
  open(F,"<$machine.jobs") || die "file $machine.jobs not found";
  my $t=<F>;
  chomp $t;
  if ($t =~ m/^\d+$/) {
    $desiredRev=$t;
  } elsif ($t =~m/^pdl=(.+)/) {
    $products=$1;
  } else {
    push @commands, $t;
  }
  while(<F>) {
    chomp;
    push @commands, $_;
  }
  close(F);
}

my $poll=1;

$products="gs pcl xps svg" if (length($products)==0);
my %products;
my @products=split ' ',$products;
foreach (@products) {
  $products{$_}=1;
}

my $cmd;
my $s;

my %spawnPIDs;

sub killAll() {
  print "in killAll()\n";
  my $a=`ps -ef`;
  my @a=split '\n',$a;
  my %children;
  foreach (@a) {
    if (m/\S+ +(\d+) +(\d+)/ && !m/<defunct>/) {
      $children{$2}=$1;
    }
  }

  foreach my $pid (keys %pids) {
    #     kill 1, $pid;
    my $p=$pid;
    while (exists $children{$p}) {
      print "$p->$children{$p}\n";
      $p=$children{$p};
    }
    kill 1, $p;
  }
}

sub spawn($$) {
  my $timeout=shift;
  my $s=shift;
  my $done=0;

  while (!$done) {
    my $pid = fork();
    if (not defined $pid) {
      mylog "fork() failed";
      die "fork() failed";
    } elsif ($pid == 0) {
      `$s`;
      exit(0);
    } else {
      if ($timeout==0) {
        $spawnPIDs{$pid}=1;
        $done=1;
      } else {
        my $t=0;
        my $status;
        do {
          select(undef, undef, undef, 0.1);
          $status=waitpid($pid,WNOHANG);
          $t++;
        } while($status>=0 && $t<abs($timeout*10));
        $done=1 if ($timeout<0);
        if ($status>=0) {
          kill 9, $pid;
        } else {
          $done=1;
        }
      }
    }
  }
}

sub checkAbort {
  checkPID();
  return (1) if ($abort==1);
  spawn(60,"scp -i ~/.ssh/cluster_key  regression\@casper3.ghostscript.com:/home/regression/cluster/$machine.abort . >/dev/null 2>/dev/null");
  if (-e "$machine.abort") {
    mylog("found $machine.abort on casper\n");
    spawn(70,"ssh -i ~/.ssh/cluster_key regression\@casper3.ghostscript.com \"rm /home/regression/cluster/$machine.abort\"");
    mylog("removing $machine.abort on casper\n");
    killAll();
    return(1);
  }
  return(0);
}

sub updateStatus($) {
  my $message=shift;
  `echo '$message' >$machine.status`;
  spawn(0,"scp -i ~/.ssh/cluster_key $machine.status regression\@casper3.ghostscript.com:/home/regression/cluster/$machine.status");
  mylog($message);
}

if ($user) {
} else {
  updateStatus('Updating test files');

  #update the regression file source directory
  if ($updateTestFiles) {

    foreach my $testSource (sort keys %testSource) {
      $cmd="cd $testSource ; export SVN_SSH=\"ssh -i \$HOME/.ssh/cluster_key\" ; svn update";
      print "$cmd\n" if ($verbose);
      `$cmd`;
    }

  }
}

$abort=checkAbort;
if (!$abort) {

  if ($user) {
    updateStatus("Fetching source from users/$user");
    mkdir "users";
    mkdir "users/$user";
    mkdir "users/$user/ghostpdl";
    mkdir "users/$user/ghostpdl/gs";
# if ($product eq 'gs') {
#   $cmd="cd users/$user/ghostpdl ; rsync -vlogDtprxe.iLsz --delete -e \"ssh -l regression -i \$HOME/.ssh/cluster_key\" regression\@$host:$usersDir/$user/ghostpdl/gs .";
# } else {
    $cmd="cd users/$user          ; rsync -vlogDtprxe.iLsz --delete -e \"ssh -l regression -i \$HOME/.ssh/cluster_key\" regression\@$host:$usersDir/$user/ghostpdl    .";
    # }
    print "$cmd\n" if ($verbose);
    `$cmd`;

    $gpdlSource=$baseDirectory."/users/$user/ghostpdl";
    $gsSource=$gpdlSource."/gs";
  } else {

    updateStatus('Updating Ghostscript');

    # get update
    my $pdlrev;
    my $gsrev;
    if ($revs =~ m/(\d+) (\d+)/) {
      $pdlrev=$1;
      $gsrev=$2;
    } else {
      mylog "oops 4";
      die "oops 4";  # hack
    }
    $cmd="cd $gsSource ; touch base/gscdef.c ; rm -f base/gscdef.c ; cd $gpdlSource ; export SVN_SSH=\"ssh -i \$HOME/.ssh/cluster_key\" ; svn update -r$pdlrev ; cd $gsSource ; export SVN_SSH=\"ssh -i \$HOME/.ssh/cluster_key\" ; svn update -r$gsrev";
    print "$cmd\n" if ($verbose);
    `$cmd`;

  }
}

$cmd="touch $temp2 ; rm -fr $temp2 ; mv $temp $temp2 ; mkdir $temp ; rm -fr $temp2 &";
print "$cmd\n" if ($verbose);
`$cmd`;

$abort=checkAbort;
if (!$abort) {

  # modify product name so that files that print that information don't change
  # when we rev. the revision
  open(F1,"<$gsSource/base/gscdef.c") || die "file $gsSource/base/gscdef.c not found";
  open(F2,">$gsSource/base/gscdef.tmp") || die "$gsSource/base/gscdef.tmp cannot be open for writing";
  while(<F1>) {
    print F2 $_;
    if (m/define GS_PRODUCT/) {
      my $dummy=<F1>;
      print F2 "\t\"GPL Ghostscript\"\n";
    }
  }
  close(F1);
  close(F2);
  $cmd="mv $gsSource/base/gscdef.tmp $gsSource/base/gscdef.c";
  `$cmd`;

  unlink "$gsSource/makegs.out";
  unlink "$gpdlSource/makepcl.out";
  unlink "$gpdlSource/makexps.out";
  unlink "$gpdlSource/makesvg.out";

  if ($products{'gs'}) {

    updateStatus('Building Ghostscript');

    # build ghostscript
    $cmd="cd $gsSource ; touch makegs.out ; rm -f makegs.out ; nice make distclean >makedistclean.out 2>&1 ; nice ./autogen.sh \"CC=gcc -m$wordSize\" --disable-cups --disable-fontconfig --disable-cairo --without-system-libtiff --prefix=$gsBin >makegs.out 2>&1 ; nice make -j 12 >>makegs.out 2>&1 ; echo >>makegs.out ; nice make >>makegs.out 2>&1";
    print "$cmd\n" if ($verbose);
    `$cmd`;

    updateStatus('Installing Ghostscript');

    #install ghostscript
    $cmd="rm -fr $gsBin ; cd $gsSource ; nice make install";
    print "$cmd\n" if ($verbose);
    `$cmd`;

    if (-e "$gsBin/bin/gs") {
      if (!$user) {
        `cp -p $gsBin/bin/gs ./gs.save`;
      }
    } else {
      $compileFail.="gs ";
    }
  } else {
    if (-e "./gs.save") {
      `cp -p ./gs.save $gsBin/bin/gs`;
    }
  }
} 

$abort=checkAbort;
if ($products{'pcl'} && !$abort) {
  updateStatus('Building GhostPCL');
  $cmd="cd $gpdlSource ; nice make pcl-clean ; touch makepcl.out ; rm -f makepcl.out ; nice make pcl \"CC=gcc -m$wordSize\" \"CCLD=gcc -m$wordSize\" >makepcl.out 2>&1 -j 12; echo >>makepcl.out ;  nice make pcl \"CC=gcc -m$wordSize\" \"CCLD=gcc -m$wordSize\" >>makepcl.out 2>&1";
  print "$cmd\n" if ($verbose);
  `$cmd`;
  if (-e "$gpdlSource/main/obj/pcl6") {
    $cmd="cp -p $gpdlSource/main/obj/pcl6 $gsBin/bin/.";
    print "$cmd\n" if ($verbose);
    `$cmd`;
  } else {
    $compileFail.="pcl6 ";
  }
}

$abort=checkAbort;
if ($products{'xps'} && !$abort) {
  updateStatus('Building GhostXPS');
  $cmd="cd $gpdlSource ; nice make xps-clean ; touch makexps.out ; rm -f makexps.out ; nice make xps \"CC=gcc -m$wordSize\" \"CCLD=gcc -m$wordSize\" >makexps.out 2>&1 -j 12; echo >>makexps.out ; nice make xps \"CC=gcc -m$wordSize\" \"CCLD=gcc -m$wordSize\" >>makexps.out 2>&1";
  print "$cmd\n" if ($verbose);
  `$cmd`;
  if (-e "$gpdlSource/xps/obj/gxps") {
    $cmd="cp -p $gpdlSource/xps/obj/gxps $gsBin/bin/.";
    print "$cmd\n" if ($verbose);
    `$cmd`;
  } else {
    $compileFail.="gxps ";
  }
}

$abort=checkAbort;
if ($products{'svg'} && !$abort) {
  updateStatus('Building GhostSVG');
  $cmd="cd $gpdlSource ; nice make svg-clean ; touch makesvg.out ; rm -f makesvg.out ; nice make svg \"CC=gcc -m$wordSize\" \"CCLD=gcc -m$wordSize\" >makesvg.out 2>&1 -j 12; echo >>makesvg.out ; nice make svg \"CC=gcc -m$wordSize\" \"CCLD=gcc -m$wordSize\" >>makesvg.out 2>&1";
  print "$cmd\n" if ($verbose);
  `$cmd`;
  if (-e "$gpdlSource/svg/obj/gsvg") {
    $cmd="cp -p $gpdlSource/svg/obj/gsvg $gsBin/bin/.";
    print "$cmd\n" if ($verbose);
    `$cmd`;
  } else {
    $compileFail.="gsvg ";
  }
}

unlink "link.icc","wts_plane_0","wts_plane_1","wts_plane_2","wts_plane_3";
$cmd="cp -p ghostpdl/link.icc ghostpdl/wts_plane_0 ghostpdl/wts_plane_1 ghostpdl/wts_plane_2 ghostpdl/wts_plane_3 .";
print "$cmd\n" if ($verbose);
`$cmd`;
$cmd="touch urwfonts ; rm -fr urwfonts ; cp -pr ghostpdl/urwfonts .";
print "$cmd\n" if ($verbose);
`$cmd`;

if ($md5sumFail ne "") {
  updateStatus('md5sum fail');
mylog("setting $machine.fail on casper3\n");
spawn(300,"ssh -i ~/.ssh/cluster_key regression\@casper3.ghostscript.com \"touch /home/regression/cluster/$machine.fail\"");
  @commands=();
} elsif ($compileFail ne "") {
  updateStatus('Compile fail');
mylog("setting $machine.fail on casper3\n");
spawn(300,"ssh -i ~/.ssh/cluster_key regression\@casper3.ghostscript.com \"touch /home/regression/cluster/$machine.fail\"");
  @commands=();
} else {
  updateStatus('Starting jobs');
}

my $totalJobs=scalar(@commands);
my $jobs=0;

my $startTime=time;

while (($poll==1 || scalar(@commands)) && !$abort && !$compileFail) {
  my $count=0;

  if ($poll==1 && scalar(@commands)==0) {

    use IO::Socket;
    my ($host, $port, $handle, $line);

    $host="casper.ghostscript.com";
    $port=9000;

    my $retry=10;
    do {
      if ($retry<10) {
#       my $s=`date +\"%y_%m_%d_%H_%M_%S\"`;
#       $s.="_$retry";
#       `touch $s`;
        mylog("retrying connection\n");
        $abort=checkAbort;
        $retry=0 if ($abort);
      }

      sleep 5 if ($retry<10);
      # create a tcp connection to the specified host and port
      $handle = IO::Socket::INET->new(Proto     => "tcp",
        PeerAddr  => $host,
        PeerPort  => $port);
    } until (($handle) || ($retry--<=0));
    if (!$handle) {
      unlink $runningSemaphore;
      updateStatus("Can not connect to $host, quitting");
      mylog "can not connect to port $port on $host: $!";
      die "can not connect to port $port on $host: $!";  # hack
    }

    print $handle "$machine\n";

    my $n=0;
    my $s="";
    eval {
      local $SIG{ALRM} = sub { die "alarm\n" };
      alarm 60;
      do {
        $n=sysread($handle,$line,4096);
        $s.=$line;
        #print STDOUT "$line";
        } until (!defined($n) || $n==0);
      alarm 0;
      };
    alarm 0;  # avoid race condition
    if ($@) {
      # die unless $@ eq "alarm\n";
      # unlink $runningSemaphore;
      $s="";
      updateStatus('Connection timed out, quitting');
      @commands=();
      $maxCount=0;
      killAll();
      checkAbort();
      $poll=0;
      $abort=1;
      # die "timed out";  # hack
    }
    close $handle;

    if (!defined($n)) {
      # unlink $runningSemaphore;
      $s="";
      updateStatus('Connection interrupted, quitting');
      @commands=();
      $maxCount=0;
      killAll();
      checkAbort();
      $poll=0;
      $abort=1;
      # die "sysread returned null";  # hack
    }

    @commands = split '\n',$s;
    $totalJobs=scalar(@commands);
    mylog("received ".scalar(@commands)." commands\n");
    mylog("commands[0] eq 'done'\n") if ($commands[0] eq "done");
    if ((scalar @commands==0) || $commands[0] eq "done") {
      $poll=0;
      @commands=();
    }
  }
  mylog("end of loop: scalar(commands)=".scalar(@commands)." poll=$poll\n") if (scalar(@commands)==0 || $poll==0);

  my $a=`ps -ef`;
  my @a=split '\n',$a;
  my %children;
  foreach (@a) {
    if (m/\S+ +(\d+) +(\d+)/ && !m/<defunct>/) {
      $children{$2}=$1;
    }
  }

  foreach my $pid (keys %pids) {
    if (time-$pids{$pid}{'time'} >= $timeOut) {
      #     kill 1, $pid;
      my $p=$pid;
      while (exists $children{$p}) {
        print "$p->$children{$p}\n";
        $p=$children{$p};
      }
      kill 1, $p;

      $timeOuts{$pids{$pid}{'filename'}}=1;
      my $count=scalar (keys %timeOuts);
      print "\nkilled:  $p ($pid) $pids{$pid}{'filename'}  total $count\n";
      if ($count>=$maxTimeout) {
        $timeoutFail="too many timeouts";
mylog("setting $machine.fail on casper3\n");
spawn(300,"ssh -i ~/.ssh/cluster_key regression\@casper3.ghostscript.com \"touch /home/regression/cluster/$machine.fail\"");
        updateStatus('Timeout fail');
        @commands=();
        $maxCount=0;
        killAll();
        checkAbort();
        $poll=0;
        $abort=1;
      }
    } else {
#print "\n$pids{$pid}{'filename'} ".(time-$pids{$pid}{'time'}) if ((time-$pids{$pid}{'time'})>20);
    }
    my $s;
    $s=waitpid($pid,WNOHANG);
    delete $pids{$pid} if ($s<0);
    $count++ if ($s>=0);
  }
  if (scalar(@commands)>0 && $count<$maxCount) {
    my $n=rand(scalar @commands);
    my @a=split '\t',$commands[$n];
    splice(@commands,$n,1);
    my $filename=$a[0];
    my $cmd=$a[1];
    $jobs++;
    {
      sub convertTime($) {
        my $t=shift;
        my $seconds=$t % 60;
        $t=($t-$seconds)/60;
        my $minutes=$t % 60;
        $t=($t-$minutes)/60;

        my $s=sprintf("%2d:%02d:%02d",$t,$minutes,$seconds);
        return($s);
      }

      my $elapsedTime=time-$startTime;
      if ($elapsedTime>=60) {
        my $t=sprintf "%d tests completed",$jobs;
        updateStatus($t);
        $abort=checkAbort;
        $startTime=time;

      }
#     printf "\n$t1 $t2 $t3  %5d %5d  %3d",$jobs,$totalJobs, $percentage if ($debug);
    }
    my $pid = fork();
    if (not defined $pid) {
      mylog "fork() failed";
      die "fork() failed";
    } elsif ($pid == 0) {
      system("$cmd");
      exit(0);
    } else {
      $pids{$pid}{'time'}=time;
      $pids{$pid}{'filename'}=$filename;
    }
  } else {
    select(undef, undef, undef, 0.25);
    #   print ".";
  }
  # if (open (F,"</tmp/nightly.abort")) {
  #    close(F);
  #    sleep 10;
  #   `rm /tmp/nightly.abort`;
  #   die "\n\nnightly abort was set\n\n";
  # }
}

if (!$abort) {


  print "\n" if ($debug);
  my $count;
  my $startTime=time;
  my $lastCount=-1;
  do {
    my $tempCount=scalar keys %pids;
    if ($tempCount != $lastCount) {
      my $message=("Waiting for $tempCount jobs to finish");
      if ($tempCount<=3) {
        if ($tempCount==1) {
          $message="Waiting for $tempCount job to finish";
        }
        $message.=":";
        foreach my $pid (keys %pids) {
          $pids{$pid}{'filename'} =~ m/.+__(.+)$/;
          $message.= ' '.$1;
        }
      }
      updateStatus($message);
      $lastCount=$tempCount;
    }

    if (time-$startTime>60) {
      $abort=checkAbort;
      $startTime=time;
    }
    $count=0;

    my $a=`ps -ef`;
    my @a=split '\n',$a;
    my %children;
    foreach (@a) {
      if (m/\S+ +(\d+) +(\d+)/ && !m/<defunct>/) {
        $children{$2}=$1;
      }
    }

    foreach my $pid (keys %pids) {
      if (time-$pids{$pid}{'time'} >= $timeOut) {
        #     kill 1, $pid;
        my $p=$pid;
        while (exists $children{$p}) {
          print "$p->$children{$p}\n";
          $p=$children{$p};
        }
        kill 1, $p;

        $timeOuts{$pids{$pid}{'filename'}}=1;
        my $count=scalar (keys %timeOuts);
        print "\nkilled:  $p ($pid) $pids{$pid}{'filename'}  total $count\n";
        if ($count>=$maxTimeout) {
          $timeoutFail="too many timeouts";
          updateStatus('Timeout fail');
mylog("setting $machine.fail on casper3\n");
spawn(300,"ssh -i ~/.ssh/cluster_key regression\@casper3.ghostscript.com \"touch /home/regression/cluster/$machine.fail\"");
          @commands=();
          $maxCount=0;
          killAll();
          checkAbort();
          $poll=0;
          $abort=1;
        }
      } else {
#print "\n$pids{$pid}{'filename'} ".(time-$pids{$pid}{'time'}) if ((time-$pids{$pid}{'time'})>20);
      }
      my $s;
      $s=waitpid($pid,WNOHANG);
      delete $pids{$pid} if ($s<0);
      $count++ if ($s>=0);
    }
    print "$count " if ($debug);
    select(undef, undef, undef, 1.00);
  } while ($count>0 && !$abort);
  print "\n" if ($debug);

  if (!$abort) {
    updateStatus('Collecting log files');

    my %logfiles;
    opendir(DIR, $temp) || die "can't opendir $temp: $!";
    foreach (readdir(DIR)) {
      $logfiles{$1}=1 if (m/(.+)\.log$/);
    }
    closedir DIR;

    #print Dumper(\%logfiles);

    system("date") if ($debug2);

    open(F2,">$machine.log");

    if ($md5sumFail ne "") {
      print F2 "md5sumFail: $md5sumFail";
    }
    if ($compileFail ne "") {
      print F2 "compileFail: $compileFail\n\n";
      my $dir="ghostpdl";
      $dir="users/$user/ghostpdl" if ($user);
      foreach my $i ('gs/makegs.out','makepcl.out','makexps.out','makesvg.out') {
        my $count=0;
        my $start=-1;
        if (open(F3,"<$dir/$i")) {
          while(<F3>) {
            $start=$count if (m/^gcc/);
            $count++;
          }
          close(F3);
        }
        my $t1=$count;
        $count-=20;
        $count=$start-5 if ($start!=-1);
        $count=$t1-10 if ($t1-$count<10);
        $count=$t1-250 if ($t1-$count>250);
        $t1-=$count;
        print F2 "\n$i (last $t1 lines):\n\n";
        if (open(F3,"<$dir/$i")) {
          while(<F3>) {
            if ($count--<0) {
              print F2 $_;
            }
          }
          close(F3);
        } else {
          #       print F2 "missing\n";
        }
      }
    }
    if ($timeoutFail ne "") {
      print F2 "timeoutFail: $timeoutFail\n";
    }
    my $lastTime=time;
    foreach my $logfile (keys %logfiles) {
      if ($lastTime-time>60) {
        $abort=checkAbort;
        $lastTime=time;
      }

      print F2 "===$logfile===\n";
      open(F,"<$temp/$logfile.log") || die "file $temp/$logfile.log not found";
      while(<F>) {
        print F2 $_;
      }
      close(F);
      if (open(F,"<$temp/$logfile.md5")) {
        while(<F>) {
          print F2 $_;
        }
        close(F);
      }
      if (exists $timeOuts{$logfile}) {
        print F2 "killed: timeout\n";
      }
      print F2 "\n";
    }

    close(F2);

    updateStatus('Uploading log files');
    system("date") if ($debug2);
    `touch $machine.log.gz ; rm -f $machine.log.gz ; gzip $machine.log`;
    `touch $machine.out.gz ; rm -f $machine.out.gz ; gzip $machine.out`;
    spawn(300,"scp -i ~/.ssh/cluster_key $machine.log.gz regression\@casper3.ghostscript.com:/home/regression/cluster/$machine.log.gz");
    spawn(300,"scp -i ~/.ssh/cluster_key $machine.out.gz regression\@casper3.ghostscript.com:/home/regression/cluster/$machine.out.gz");

    system("date") if ($debug2);
    updateStatus('idle');
  }
} # if (!$abort)

if ($abort) {
  updateStatus('Abort command received');
}

mylog("setting $machine.done on casper3\n");
spawn(300,"ssh -i ~/.ssh/cluster_key regression\@casper3.ghostscript.com \"touch /home/regression/cluster/$machine.done\"");

system("date") if ($debug2);
#`rm -fr $temp`;
system("date") if ($debug2);

unlink $runningSemaphore;

if (0) {
  foreach my $pid (keys %spawnPIDs) {
    my $status1=waitpid($pid,WNOHANG);
    select(undef, undef, undef, 0.1);
    my $status2=waitpid($pid,WNOHANG);
    print "$pid: $status1 $status2   ";
  }
  print "\n";
}

close(LOG);

