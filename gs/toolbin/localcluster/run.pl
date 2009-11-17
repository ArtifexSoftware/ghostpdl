#!/usr/bin/perl

use strict;
use warnings;

use Data::Dumper;
use POSIX ":sys_wait_h";

my $updateGS=1;
my $updateTestFiles=1;

my $debug=0;
my $debug2=0;
my $verbose=0;

my $wordSize="64";
my $timeOut=300;
my $maxTimeout=20;

my %pids;
my %timeOuts;

my $machine=shift || die "usage: run.pl machine_name";

my $user;
#my $product;

my $products="gs pcl svg xps";


my $runningSemaphore="./run.pid";

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

if (0) {
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
}

open(F,">$runningSemaphore");
print F "$$\n";
close(F);

if (open(F,"<$machine.start")) {
  $_=<F>;
  close(F);
  if ($_) {
    chomp;
    my @a=split ' ';
    $user=$a[0];
#   $product=$a[1];
#   if ($user && $product) {
#     #   print "user=$user product=$product\n";
#   } else {
#     #   print "empty user\n";
#   }
  }
  unlink("$machine.start");
}


my $host="casper.ghostscript.com";

my $desiredRev;

my $maxCount=12;
$maxCount=16;


my $baseDirectory=`pwd`;
chomp $baseDirectory;
my $usersDir="/home/marcos/cluster/users";

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

open(F,"<$machine.jobs") || die "file $machine.jobs not found";
my @commands;
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

my %products;
my @products=split ' ',$products;
foreach (@products) {
  $products{$_}=1;
}


my $cmd;
my $s;
my $previousRev1;
my $previousRev2;
my $newRev1=99999;
my $newRev2=99999;

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
  spawn(60,"scp -i ~/.ssh/cluster_key  marcos\@casper.ghostscript.com:/home/marcos/cluster/$machine.abort . >/dev/null 2>/dev/null");
  if (open(F,"<$machine.abort")) {
    close(F);
    killAll();
    return(1);
  }
  return(0);
}

sub updateStatus($) {
  my $message=shift;
  `echo '$message' >$machine.status`;
  spawn(0,"scp -i ~/.ssh/cluster_key $machine.status marcos\@casper.ghostscript.com:/home/marcos/cluster/$machine.status");
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
    # `pkill -9 rsync`; exit;
    # my $s=`ps -ef | grep rsync | grep -v grep`;
    # chomp $s;
    # my @a=split ' ',$s;
    # `kill -9 $a[1]`;

    updateStatus("Fetching source from users/$user");
    mkdir "users";
    mkdir "users/$user";
    mkdir "users/$user/ghostpdl";
    mkdir "users/$user/ghostpdl/gs";
# if ($product eq 'gs') {
#   $cmd="cd users/$user/ghostpdl ; rsync -vlogDtprxe.iLs --delete -e \"ssh -l marcos -i \$HOME/.ssh/cluster_key\" marcos\@$host:$usersDir/$user/ghostpdl/gs .";
# } else {
    $cmd="cd users/$user          ; rsync -vlogDtprxe.iLs --delete -e \"ssh -l marcos -i \$HOME/.ssh/cluster_key\" marcos\@$host:$usersDir/$user/ghostpdl    .";
    # }
    print "$cmd\n" if ($verbose);
    `$cmd`;

    $gpdlSource=$baseDirectory."/users/$user/ghostpdl";
    $gsSource=$gpdlSource."/gs";
  } else {

    updateStatus('Updating Ghostscript');

    if ($updateGS) {
      # check previous revision
      $cmd="cd $gpdlSource ; svn info";
      $s=`$cmd`;
      if ($s=~m/Last Changed Rev: (\d+)/) {
        $previousRev1=$1;
      } else {
        die "svn info failed";
      }
      $cmd="cd $gsSource ; svn info";
      $s=`$cmd`;
      if ($s=~m/Last Changed Rev: (\d+)/) {
        $previousRev2=$1;
      } else {
        die "svn info failed";
      }

      # get update
      if ($desiredRev) {
        $cmd="cd $gsSource ; touch base/gscdef.c ; rm -f base/gscdef.c ; cd $gpdlSource ; export SVN_SSH=\"ssh -i \$HOME/.ssh/cluster_key\" ; svn update -r$desiredRev ; cd $gsSource ; export SVN_SSH=\"ssh -i \$HOME/.ssh/cluster_key\" ; svn update -r$desiredRev";
      } else {
        $cmd="cd $gsSource ; touch base/gscdef.c ; rm -f base/gscdef.c ; cd $gpdlSource ; export SVN_SSH=\"ssh -i \$HOME/.ssh/cluster_key\" ; svn update";
      }
      print "$cmd\n" if ($verbose);
      `$cmd`;

      updateStatus('Modifying Ghostscript');

      # check current revision
      $cmd="cd $gpdlSource ; svn info";
      $s=`$cmd`;
      if ($s=~m/Last Changed Rev: (\d+)/) {
        $newRev1=$1;
      } else {
        die "svn info failed";
      }
      $cmd="cd $gsSource ; svn info";
      $s=`$cmd`;
      if ($s=~m/Last Changed Rev: (\d+)/) {
        $newRev2=$1;
      } else {
        die "svn info failed";
      }

      print "$previousRev1 $newRev1\n" if ($verbose);
      print "$previousRev2 $newRev2\n" if ($verbose);

      if ($previousRev1==$newRev1 && $previousRev2==$newRev2) {
        #   print "no updates to repository, quitting\n";
        #   exit;
      }
    }
  }
}

$cmd="touch $temp2 ; rm -fr $temp2 ; mv $temp $temp2 ; mkdir $temp ; rm -fr $temp2 &";
print "$cmd\n" if ($verbose);
`$cmd`;

if ($updateGS) {
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

#   if (1 || !$product || $product eq 'gs') {  # always build ghostscript
    if ($products{'gs'}) {

      updateStatus('Building Ghostscript');

      # build ghostscript
      $cmd="cd $gsSource ; touch makegs.out ; rm -f makegs.out ; nice make distclean >makedistclean.out 2>&1 ; nice ./autogen.sh \"CC=gcc -m$wordSize\" --disable-cups --disable-fontconfig --disable-cairo --prefix=$gsBin >makegs.out 2>&1 ; nice make -j 12 >>makegs.out 2>&1 ; nice make >>makegs.out 2>&1";
      print "$cmd\n" if ($verbose);
      `$cmd`;

      updateStatus('Installing Ghostscript');

      #install ghostscript
      $cmd="rm -fr $gsBin ; cd $gsSource ; nice make install";
      print "$cmd\n" if ($verbose);
      `$cmd`;

      if (open(F,"<$gsBin/bin/gs")) {
        close(F);
      } else {
        $compileFail.="gs ";
      }
    }
  }

# if (1 || !$product || $product eq 'ghostpdl') {  # always build ghostpdl
#   if (1) {
      $abort=checkAbort;
      if (!$abort) {
        updateStatus('Make clean GhostPCL/XPS/SVG');
        $cmd="cd $gpdlSource ; nice make pcl-clean xps-clean svg-clean -j 12 >makeclean.out 2>&1";
        print "$cmd\n" if ($verbose);
        `$cmd`;
      }

      $abort=checkAbort;
      if ($products{'pcl'} && !$abort) {
        updateStatus('Building GhostPCL');
        $cmd="cd $gpdlSource ; touch makepcl.out ; rm -f makepcl.out ; nice make pcl \"CC=gcc -m$wordSize\" \"CCLD=gcc -m$wordSize\" >makepcl.out 2>&1 -j 12; nice make pcl \"CC=gcc -m$wordSize\" \"CCLD=gcc -m$wordSize\" >>makepcl.out 2>&1";
        print "$cmd\n" if ($verbose);
        `$cmd`;
        if (open(F,"<$gpdlSource/main/obj/pcl6")) {
          close(F);
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
        $cmd="cd $gpdlSource ; touch makexps.out ; rm -f makexps.out ; nice make xps \"CC=gcc -m$wordSize\" \"CCLD=gcc -m$wordSize\" >makexps.out 2>&1 -j 12; nice make xps \"CC=gcc -m$wordSize\" \"CCLD=gcc -m$wordSize\" >>makexps.out 2>&1";
        print "$cmd\n" if ($verbose);
        `$cmd`;
        if (open(F,"<$gpdlSource/xps/obj/gxps")) {
          close(F);
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
        $cmd="cd $gpdlSource ; touch makesvg.out ; rm -f makesvg.out ; nice make svg \"CC=gcc -m$wordSize\" \"CCLD=gcc -m$wordSize\" >makesvg.out 2>&1 -j 12; nice make svg \"CC=gcc -m$wordSize\" \"CCLD=gcc -m$wordSize\" >>makesvg.out 2>&1";
        print "$cmd\n" if ($verbose);
        `$cmd`;
        if (open(F,"<$gpdlSource/svg/obj/gsvg")) {
          close(F);
          $cmd="cp -p $gpdlSource/svg/obj/gsvg $gsBin/bin/.";
          print "$cmd\n" if ($verbose);
          `$cmd`;
        } else {
          $compileFail.="gsvg ";
        }
      }
#   }
# }
}

unlink "link.icc","wts_plane_0","wts_plane_1","wts_plane_2","wts_plane_3";
$cmd="cp -p ghostpdl/link.icc ghostpdl/wts_plane_0 ghostpdl/wts_plane_1 ghostpdl/wts_plane_2 ghostpdl/wts_plane_3 .";
print "$cmd\n" if ($verbose);
`$cmd`;

if ($md5sumFail ne "") {
  updateStatus('md5sum fail');
  @commands=();
} elsif ($compileFail ne "") {
  updateStatus('Compile fail');
  @commands=();
} else {
  updateStatus('Starting jobs');
}

my $totalJobs=scalar(@commands);
my $jobs=0;


my $startTime=time;
my $lastPercentage=-1;

while (scalar(@commands) && !$abort) {
  my $count=0;

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
        @commands=();
        $maxCount=0;
        killAll();
      }
    } else {
#print "\n$pids{$pid}{'filename'} ".(time-$pids{$pid}{'time'}) if ((time-$pids{$pid}{'time'})>20);
    }
    my $s;
    $s=waitpid($pid,WNOHANG);
    delete $pids{$pid} if ($s<0);
    $count++ if ($s>=0);
  }
  if ($count<$maxCount) {
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

      my $currentTime=time;
      my $elapsedTime=$currentTime-$startTime;
      my $t1=convertTime($elapsedTime);
      my $t2=convertTime($elapsedTime * $totalJobs/$jobs - $elapsedTime);
      my $t3=convertTime($elapsedTime * $totalJobs/$jobs);
      my $percentage=int($jobs/$totalJobs * 100 + 0.5);
      if ($percentage != $lastPercentage && ($percentage % 2) == 0) {
        my $t=sprintf "%2d%% completed",$percentage;
        updateStatus($t);
        $lastPercentage=$percentage;
        # check for abort
        $abort=checkAbort;

      }
      printf "\n$t1 $t2 $t3  %5d %5d  %3d",$jobs,$totalJobs, $percentage if ($debug);
    }
    my $pid = fork();
    if (not defined $pid) {
      die "fork() failed";
    } elsif ($pid == 0) {
      system("$cmd");
      #     `rm -f $outputFilenames` if (length($outputFilenames));

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

if ($abort) {
  updateStatus('Abort command received');
} else {

  updateStatus('Waiting for jobs to finish');

  print "\n" if ($debug);
  my $count;
  do {
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
          @commands=();
          $maxCount=0;
	  killAll();
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
    select(undef, undef, undef, 0.25);
    } while ($count>0);
  print "\n" if ($debug);

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
      if (open(F3,"<$dir/$i")) {
        while(<F3>) {
	  $count++;
        }
        close(F3);
      }
      $count-=20;
      print F2 "\n$i (last 20 lines):\n\n";
      if (open(F3,"<$dir/$i")) {
        while(<F3>) {
	  if ($count--<0) {
            print F2 $_;
	  }
        }
        close(F3);
      } else {
        print F2 "missing\n";
      }
    }
  }
  if ($timeoutFail ne "") {
    print F2 "timeoutFail: $timeoutFail\n";
  }
  foreach my $logfile (keys %logfiles) {
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
  spawn(300,"scp -i ~/.ssh/cluster_key $machine.log.gz marcos\@casper.ghostscript.com:/home/marcos/cluster/$machine.log.gz");
  spawn(300,"scp -i ~/.ssh/cluster_key $machine.out.gz marcos\@casper.ghostscript.com:/home/marcos/cluster/$machine.out.gz");

  system("date") if ($debug2);
  updateStatus('idle');
} # if (!$abort)
spawn(10,"ssh -i ~/.ssh/cluster_key marcos\@casper.ghostscript.com \"touch /home/marcos/cluster/$machine.done\"");

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

