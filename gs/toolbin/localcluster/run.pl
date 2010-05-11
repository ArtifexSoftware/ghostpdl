#!/usr/bin/perl

use strict;
use warnings;

use Data::Dumper;
use POSIX ":sys_wait_h";

use File::stat;

my $updateTestFiles=1;
my $dontBuild=0;

my $debug=0;
my $verbose=0;

my $wordSize="64";
my $timeOut=300;
my $maxTimeout=20;  # starting value, is adjusted by value below based on jobs completed
my $maxTimeoutPercentage=1.0;

my $maxCount=12;
$maxCount=16;

my $maxRaster=25;

my %pids;
my %timeOuts;

my $machine=shift || die "usage: run.pl machine_name";

my $local=0;
$dontBuild=1     if ($machine eq "local_no_build");
$machine="local" if ($machine eq "local_no_build");
$local=1         if ($machine eq "local");
$timeOut=600     if ($local);

if (!$local) {
  open (LOG,">>$machine.dbg");
  print LOG "\n\n";
}

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

# the commands below only need to be done once
mkdir("icc_work");
mkdir("icc_work/bin");
mkdir("./head/");
mkdir("./head/bin");

my $user;
my $revs;
my $icc_work;
my $mupdf;
#my $product;
my @commands;

my $products="";

my $runningSemaphore="./run.pid";

sub spawn;

sub watchdog {
  spawn(0,"ssh -i ~/.ssh/cluster_key regression\@casper.ghostscript.com touch /home/regression/cluster/$machine.up");
}

# the concept with checkPID is that if the semaphore file is missing or doesn't
# contain our PID something bad has happened and we just just exit
sub checkPID {
  return(1) if ($local);
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

if (!$local) {
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
}

mylog "starting run.pl:  pid=$$\n";

if (1) {
  mylog "about to kill any jobs that appear to be running from previous regression\n";
  my $a=`ps -ef | grep md5sum | grep temp     | grep -v grep`;
    $a.=`ps -ef | grep md5sum | grep baseline | grep -v grep`;
  my @a=split '\n',$a;
  foreach (@a) {
    chomp;
    my @b=split ' ',$_,8;
    my $minutes=-1;
    $minutes=$1 if ($b[6]=~ m/\d+:(\d+):\d+/);    # ps -ef results from linux (I know, this isn't right)
    $minutes=$1 if ($b[6]=~ m/^(\d+):\d+\.\d+/);  # ps -ef results from max os x
    $minutes+=0;                                  # convert from 01 to 1
#   print "$b[1] $b[6] $minutes\n";
    if ($minutes>=10) {
      mylog "killing $b[1] (running $minutes minutes): $b[7]\n";
      kill 9, $b[1];
    }
  }
}

if (!$local) {
  if (open(F,"<$machine.start")) {
    my $t=<F>;
    chomp $t;
    close(F);
    if ($t) {
      my @a=split '\t', $t;
      if ($a[0] eq "svn") {
        $revs=$a[1];
        $products=$a[2];
      } elsif ($a[0] eq "mupdf") {
        $mupdf=1;
        $products="mupdf";
        $maxCount=4;
        $maxTimeoutPercentage=5.0;
        $timeOut=600;
      } elsif ($a[0] eq "svn-icc_work") {
        $icc_work=$a[1];
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
}

mylog "user products=$products user=$user\n" if ($user);
mylog "svn products=$products rev=$revs\n" if ($revs);
mylog "icc_work products=$products rev=$icc_work\n" if ($icc_work);
mylog "mupdf\n" if ($mupdf);

my $host="casper.ghostscript.com";

my $desiredRev;

my $baseDirectory=`pwd`;
chomp $baseDirectory;
my $usersDir="/home/regression/cluster/users";

my $temp="./temp";
my $temp2="./temp.tmp";
my $raster="./temp/raster";
my $bmpcmpOutput="./temp/bmpcmp";
my $baselineRaster="./baselineraster";
my $baselineRaster2="./baselineraster.tmp";

my $gpdlSource=$baseDirectory."/ghostpdl";
my $gsSource=$gpdlSource."/gs";
my $gsBin=$baseDirectory."/gs";
my $icc_workSource=$baseDirectory."/icc_work";
my $icc_workGsSource=$icc_workSource."/gs";

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

if ($local) {
  open(F,"<jobs") || die "file jobs not found";
  my $t=<F>;
  chomp $t;
  if ($t =~ m/^\d+$/) {
    $desiredRev=$t;
  } elsif ($t =~m/^pdl=(.+)/) {
    $products=$1;
  } else {
    push @commands, $t;
  }
  my %products;
  while(<F>) {
    chomp;
    push @commands, $_;
    if (m/echo "(.+?)[ "]/) {
      $products{$1}=1;
    }
  }
  close(F);
  foreach (sort keys %products) {
    $products.="$_ ";
  }
}

my $poll=1;

$poll=0 if ($local);

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
  mylog "in killAll()\n";
  my $a=`ps -ef`;
  my @a=split '\n',$a;
  my %children;
  foreach (@a) {
    if (m/\S+ +(\d+) +(\d+)/ && !m/<defunct>/) {
      $children{$2}=$1;
    }
  }

  my @pids;
  foreach my $pid (keys %pids) {
#   kill 1, $pid;
    my $p=$pid;
    push @pids,$p;
    while (exists $children{$p}) {
      print "$p->$children{$p}\n";
      $p=$children{$p};
      push @pids,$p;
    }
  }
  foreach my $p (@pids) {
    mylog ("killing (killAll) $p\n");
    kill 9, $p;
  }
}

sub spawn($$) {
  my $timeout=shift;
  my $s=shift;
  my $done=0;

  return if ($local);

  while (!$done) {
    my $pid = fork();
    if (not defined $pid) {
      mylog "fork() failed";
      die "fork() failed";
    } elsif ($pid == 0) {
      exec($s);
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
          mylog ("killing (spawn) $pid\n");
          kill 9, $pid;
        } else {
          $done=1;
        }
      }
    }
  }
}

sub checkAbort {
  return(0) if ($local);
  checkPID();
  return (1) if ($abort==1);
  spawn(60,"scp -q -i ~/.ssh/cluster_key  regression\@casper.ghostscript.com:/home/regression/cluster/$machine.abort . >/dev/null 2>/dev/null");
  if (-e "$machine.abort") {
    mylog("found $machine.abort on casper\n");
    mylog("removing $machine.abort on casper\n");
    spawn(70,"ssh -i ~/.ssh/cluster_key regression\@casper.ghostscript.com \"rm /home/regression/cluster/$machine.abort\"");
    killAll();
    return(1);
  }
  return(0);
}

sub updateStatus($) {
  my $message=shift;
  if (!$local) {
    `echo '$message' >$machine.status`;
    spawn(0,"scp -q -i ~/.ssh/cluster_key $machine.status regression\@casper.ghostscript.com:/home/regression/cluster/$machine.status");
  }
  mylog($message);
}

sub systemWithRetry($) {
  my $cmd=shift;
  my $a;

  mylog "$cmd";
  print "$cmd\n" if ($verbose);
  my $count=0;
  do {
    `$cmd`;
    $a=$?/256;
    mylog "error with: $cmd; a=$a count=$count\n" if ($a!=0);
    $count++;
    sleep 10 if ($a!=0);
    } while ($a!=0 && $count<5);
  if ($a!=0) {
    unlink $runningSemaphore;
  }
}

my %logged;
sub addToLog($) {
  my $logfile=shift;
  my $md5sumIncluded=0;
  print F4 "===$logfile===\n";
  if (open(F,"<$temp/$logfile.log")) {
    while(<F>) {
      print F4 $_;
    }
    close(F);
  } else {
    mylog "file $temp/$logfile.log not found";
  }
  if (open(F,"<$temp/$logfile.md5")) {
    while(<F>) {
      chomp;
      $md5sumIncluded=1 if (m/^([0-9a-f]{32})$/ || m/^([0-9a-f]{32})  -/ || m/^([0-9a-f]{32})  \.\/temp/ || m/^MD5 .+ = ([0-9a-f]{32})$/);
      print F4 "$_\n";
    }
    close(F);
    $logged{$logfile}=1 if ($md5sumIncluded);
  }
  if (exists $timeOuts{$logfile}) {
    print F4 "killed: timeout\n";
  }
  print F4 "\n";
}

if (!$local) {
###if (!$user) {
  updateStatus('Updating test files');

  #update the regression file source directory
  if ($updateTestFiles) {

    foreach my $testSource (sort keys %testSource) {
      $cmd="cd $testSource ; export SVN_SSH=\"ssh -i \$HOME/.ssh/cluster_key\" ; svn update";
      systemWithRetry($cmd);
    }

  }
###}

  $abort=checkAbort;
  if (!$abort) {

    if ($user) {
      updateStatus("Fetching source from users/$user");
      mkdir "users";
      mkdir "users/$user";
      mkdir "users/$user/ghostpdl";
      mkdir "users/$user/ghostpdl/gs";
      $cmd="cd users/$user          ; rsync -vlogDtprxe.iLsz --delete -e \"ssh -l regression -i \$HOME/.ssh/cluster_key\" regression\@$host:$usersDir/$user/ghostpdl    .";
      systemWithRetry($cmd);

      $gpdlSource=$baseDirectory."/users/$user/ghostpdl";
      $gsSource=$gpdlSource."/gs";
    } elsif ($mupdf) {
      updateStatus('Fetching mupdf.tar.gz');
      `touch mupdf ; rm -fr mupdf; touch mupdf.tar.gz ; rm mupdf.tar.gz`;
      $cmd=`scp -q -o ConnectTimeout=30 -i ~/.ssh/cluster_key regression\@casper.ghostscript.com:/home/regression/cluster/mupdf.tar.gz .`;
      systemWithRetry($cmd);

    } elsif ($icc_work) {

      if (!-e $icc_workGsSource) {
        `rm -fr $icc_workSource`;
        updateStatus('Checking out icc_work');
        $cmd="svn co --ignore-externals http://svn.ghostscript.com/ghostscript/trunk/ghostpdl $icc_workSource";
        systemWithRetry($cmd);
        $cmd="svn co http://svn.ghostscript.com/ghostscript/branches/icc_work $icc_workGsSource";
        systemWithRetry($cmd);
      }
      updateStatus('Updating icc_work');
      $cmd="svn update -r$icc_work --ignore-externals $icc_workSource ; cd $icc_workGsSource ; touch base/gscdef.c ; rm -f base/gscdef.c ; svn update -r$icc_work";
      systemWithRetry($cmd);

      $gpdlSource=$icc_workSource;
      $gsSource=$icc_workGsSource;
    } else {

      updateStatus('Updating Ghostscript');

      # get update
      if ($revs eq "head") {
      } elsif ($revs =~ m/(\d+)/) {
      } else {
        mylog "oops 4: revs=$revs";
        die "oops 4";  # hack
      }
      $cmd="cd $gsSource ; touch base/gscdef.c ; rm -f base/gscdef.c ; cd $gpdlSource ; export SVN_SSH=\"ssh -i \$HOME/.ssh/cluster_key\" ; svn update -r$revs --ignore-externals";
      mylog "$cmd";
      print "$cmd\n" if ($verbose);
      systemWithRetry($cmd);
      $cmd="cd $gsSource ; export SVN_SSH=\"ssh -i \$HOME/.ssh/cluster_key\" ; svn update -r$revs";
      systemWithRetry($cmd);

    }

  }
}

#`cc -o bmpcmp ghostpdl/gs/toolbin/bmpcmp.c`;
`svn update $baseDirectory/ghostpdl/gs/toolbin/bmpcmp.c`;
`cc -I$baseDirectory/ghostpdl/gs/libpng -o bmpcmp -DHAVE_LIBPNG $baseDirectory/ghostpdl/gs/toolbin/bmpcmp.c $baseDirectory/ghostpdl/gs/libpng/png.c $baseDirectory/ghostpdl/gs/libpng/pngerror.c $baseDirectory/ghostpdl/gs/libpng/pnggccrd.c $baseDirectory/ghostpdl/gs/libpng/pngget.c $baseDirectory/ghostpdl/gs/libpng/pngmem.c $baseDirectory/ghostpdl/gs/libpng/pngpread.c $baseDirectory/ghostpdl/gs/libpng/pngread.c $baseDirectory/ghostpdl/gs/libpng/pngrio.c $baseDirectory/ghostpdl/gs/libpng/pngrtran.c $baseDirectory/ghostpdl/gs/libpng/pngrutil.c $baseDirectory/ghostpdl/gs/libpng/pngset.c $baseDirectory/ghostpdl/gs/libpng/pngtrans.c $baseDirectory/ghostpdl/gs/libpng/pngvcrd.c $baseDirectory/ghostpdl/gs/libpng/pngwio.c $baseDirectory/ghostpdl/gs/libpng/pngwrite.c $baseDirectory/ghostpdl/gs/libpng/pngwtran.c $baseDirectory/ghostpdl/gs/libpng/pngwutil.c -lm -lz`;

mkdir("$gsBin");
mkdir("$gsBin/bin");

$cmd="touch $baselineRaster2 ; rm -fr $baselineRaster2 ; mv $baselineRaster $baselineRaster2 ; mkdir $baselineRaster ; rm -fr $baselineRaster2 &";
print "$cmd\n" if ($verbose);
`$cmd`;

$cmd="touch $temp2 ; rm -fr $temp2 ; mv $temp $temp2 ; mkdir $temp ; mkdir $raster ; mkdir $bmpcmpOutput ; touch raster.yes ; rm -fr $temp2 &";
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

  if (!$dontBuild) {
    if (-e "./head/bin/gs") {
      `cp -p ./head/bin/* $gsBin/bin/.`;
    }
  }

  if (!$dontBuild) {
    if ($mupdf) {
      updateStatus('Building mupdf');
      $cmd="touch mupdf.tar ; rm mupdf.tar ; gunzip mupdf.tar.gz ; tar xf mupdf.tar";
      print "$cmd\n" if ($verbose);
      `$cmd`;
      if (-e "./Jamrules") {
        $cmd="cp -p Jamrules mupdf/.";
        print "$cmd\n" if ($verbose);
        `$cmd`;
      }
      $cmd="cd mupdf ; jam >makemupdf.out";
      print "$cmd\n" if ($verbose);
      `$cmd`;

      $cmd="touch gs/bin/mupdf ; rm gs/bin/mupdf ; cp -p mupdf/build/*/pdfdraw gs/bin/.";
      print "$cmd\n" if ($verbose);
      `$cmd`;
    }
  }

  if (!$dontBuild) {
    if ($products{'gs'}) {

      updateStatus('Building Ghostscript');

      # build ghostscript
      $cmd="cd $gsSource ; touch makegs.out ; rm -f makegs.out ; nice make distclean >makedistclean.out 2>&1 ; nice ./autogen.sh \"CC=gcc -m$wordSize\" --disable-cups --disable-fontconfig --disable-cairo --without-system-libtiff --prefix=$gsBin >makegs.out 2>&1 ; nice make -j 12 >>makegs.out 2>&1 ; echo >>makegs.out ; nice make >>makegs.out 2>&1";
      print "$cmd\n" if ($verbose);
      `$cmd`;

      updateStatus('Installing Ghostscript');

      # install ghostscript
      $cmd="rm -fr $gsBin ; cd $gsSource ; nice make install";
      print "$cmd\n" if ($verbose);
      `$cmd`;

      if (-e "$gsBin/bin/gs") {
        if ($icc_work) {
          `cp -p $gsBin/bin/gs ./icc_work/bin/.`;
        }
        if ($revs) {
          `cp -p $gsBin/bin/gs ./head/bin/.`;
        }
      } else {
        $compileFail.="gs ";
      }
    }
  }
}

$abort=checkAbort;
if (!$dontBuild) {
  if ($products{'pcl'} && !$abort) {
    updateStatus('Building GhostPCL');
    $cmd="cd $gpdlSource ; nice make pcl-clean ; touch makepcl.out ; rm -f makepcl.out ; nice make pcl \"CC=gcc -m$wordSize\" \"CCLD=gcc -m$wordSize\" >makepcl.out 2>&1 -j 12; echo >>makepcl.out ;  nice make pcl \"CC=gcc -m$wordSize\" \"CCLD=gcc -m$wordSize\" >>makepcl.out 2>&1";
    print "$cmd\n" if ($verbose);
    `$cmd`;
    if (-e "$gpdlSource/main/obj/pcl6") {
      $cmd="cp -p $gpdlSource/main/obj/pcl6 $gsBin/bin/.";
      print "$cmd\n" if ($verbose);
      `$cmd`;
      if ($icc_work) {
        `cp -p $gsBin/bin/pcl6 ./icc_work/bin/.`;
      }
      if ($revs) {
        `cp -p $gsBin/bin/pcl6 ./head/bin/.`;
      }
    } else {
      $compileFail.="pcl6 ";
    }
  }
}

$abort=checkAbort;
if (!$dontBuild) {
  if ($products{'xps'} && !$abort) {
    updateStatus('Building GhostXPS');
    $cmd="cd $gpdlSource ; nice make xps-clean ; touch makexps.out ; rm -f makexps.out ; nice make xps \"CC=gcc -m$wordSize\" \"CCLD=gcc -m$wordSize\" >makexps.out 2>&1 -j 12; echo >>makexps.out ; nice make xps \"CC=gcc -m$wordSize\" \"CCLD=gcc -m$wordSize\" >>makexps.out 2>&1";
    print "$cmd\n" if ($verbose);
    `$cmd`;
    if (-e "$gpdlSource/xps/obj/gxps") {
      $cmd="cp -p $gpdlSource/xps/obj/gxps $gsBin/bin/.";
      print "$cmd\n" if ($verbose);
      `$cmd`;
      if ($icc_work) {
        `cp -p $gsBin/bin/gxps ./icc_work/bin/.`;
      }
      if ($revs) {
        `cp -p $gsBin/bin/gxps ./head/bin/.`;
      }
    } else {
      $compileFail.="gxps ";
    }
  }
}

$abort=checkAbort;
if (!$dontBuild) {
  if ($products{'svg'} && !$abort) {
    updateStatus('Building GhostSVG');
    $cmd="cd $gpdlSource ; nice make svg-clean ; touch makesvg.out ; rm -f makesvg.out ; nice make svg \"CC=gcc -m$wordSize\" \"CCLD=gcc -m$wordSize\" >makesvg.out 2>&1 -j 12; echo >>makesvg.out ; nice make svg \"CC=gcc -m$wordSize\" \"CCLD=gcc -m$wordSize\" >>makesvg.out 2>&1";
    print "$cmd\n" if ($verbose);
    `$cmd`;
    if (-e "$gpdlSource/svg/obj/gsvg") {
      $cmd="cp -p $gpdlSource/svg/obj/gsvg $gsBin/bin/.";
      print "$cmd\n" if ($verbose);
      `$cmd`;
      if ($icc_work) {
        `cp -p $gsBin/bin/gsvg ./icc_work/bin/.`;
      }
      if ($revs) {
        `cp -p $gsBin/bin/gsvg ./head/bin/.`;
      }
    } else {
      $compileFail.="gsvg ";
    }
  }
}

unlink "link.icc","wts_plane_0","wts_plane_1","wts_plane_2","wts_plane_3";
$cmd="cp -p $gpdlSource/link.icc $gpdlSource/wts_plane_0 $gpdlSource/wts_plane_1 $gpdlSource/wts_plane_2 $gpdlSource/wts_plane_3 .";
print "$cmd\n" if ($verbose);
`$cmd`;
$cmd="touch urwfonts ; rm -fr urwfonts ; cp -pr $gpdlSource/urwfonts .";
print "$cmd\n" if ($verbose);
`$cmd`;

if (-e '../bin/time') {
  `cp ../bin/time $gsBin/bin/.`;    # which is different on Mac OS X, so get the one I built
} else {
  `cp /usr/bin/time $gsBin/bin/.`;  # get the generic time command
}

if ($md5sumFail ne "") {
  updateStatus('md5sum fail');
  mylog("setting $machine.fail on casper\n");
  spawn(300,"ssh -i ~/.ssh/cluster_key regression\@casper.ghostscript.com \"touch /home/regression/cluster/$machine.fail\"");
  @commands=();
} elsif ($compileFail ne "") {
  updateStatus('Compile fail');
  mylog("setting $machine.fail on casper\n");
  spawn(300,"ssh -i ~/.ssh/cluster_key regression\@casper.ghostscript.com \"touch /home/regression/cluster/$machine.fail\"");
  @commands=();
} else {
  updateStatus('Starting jobs');
}

my $totalJobs=scalar(@commands);
my $jobs=0;

open(F4,">$machine.log");

my $startTime=time;

while (($poll==1 || scalar(@commands)) && !$abort && $compileFail eq "") {
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
      } until (($abort) || ($handle) || ($retry--<=0));
    if (!$handle) {
      unlink $runningSemaphore;
      updateStatus("Can not connect to $host, quitting");
      mylog "can not connect to port $port on $host: $!";
      @commands=();
      killAll();
      checkAbort();
      die "can not connect to port $port on $host: $!";  # hack
    }

    if ($abort) {
      close($handle) if ($handle);
    } else {

      print $handle "$machine\n";

      my $n=0;
      my $s="";
      eval {
        local $SIG{ALRM} = sub { die "alarm\n" };
        alarm 60;
        do {
          $n=sysread($handle,$line,4096);
          $s.=$line;
          # print STDOUT "$line";
          } until (!defined($n) || $n==0);
        alarm 0;
        };
      alarm 0;  # avoid race condition
      if ($@) {
        # die unless $@ eq "alarm\n";
        # unlink $runningSemaphore;
        $s="";
        my $t=$@;
        chomp $t;
        updateStatus("Connection timed out, quitting: $t");
        mylog "Connection timed out, quitting: $t";
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
        updateStatus("Connection interrupted, quitting");
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
      mylog("commands[0] eq 'done'\n") if ((scalar @commands==0) || $commands[0] eq "done");
      if ((scalar @commands==0) || $commands[0] eq "done") {
        $poll=0;
        @commands=();
      }
    }
  }
# mylog("end of loop: scalar(commands)=".scalar(@commands)." poll=$poll abort=$abort\n") if (scalar(@commands)==0 || $poll==0 || $abort!=0);

  my $a=`ps -ef`;
  my @a=split '\n',$a;
  my %children;
  my %name;
  foreach (@a) {
    chomp;
    if (m/\S+ +(\d+) +(\d+) .+ \d+:\d\d.\d\d (.+)$/ && !m/<defunct>/ && !m/\(sh\)/) {
      $children{$2}=$1;
      $name{$1}=$3;
    }
  }

  foreach my $pid (keys %pids) {
    if (time-$pids{$pid}{'time'} >= $timeOut) {
      my $p=$pid;
      while (exists $children{$p}) {
#       mylog "$p->$children{$p}\n";  # mhw
        $p=$children{$p};
        $name{$p}='missing' if (!exists $name{$p});
        mylog ("killing (timeout 1) $p $name{$p}\n");
        kill 1, $p;
        kill 9, $p;
      }
      $name{$pid}='missing' if (!exists $name{$pid});
      mylog ("killing (timeout 1) $pid $name{$pid}\n");
      kill 1, $pid;
      kill 9, $pid;
      $timeOuts{$pids{$pid}{'filename'}}=1;
      addToLog($pids{$pid}{'filename'});
      my $count=scalar (keys %timeOuts);
      delete $pids{$pid};

#     mylog "killed:  $p ($pid) $pids{$pid}{'filename'}  total $count\n";  # mhw
#     mylog "killed:  $pids{$pid}{'filename'}\n";  # mhw
      if ($count>=$maxTimeout) {
        $timeoutFail="too many timeouts";
        mylog("setting $machine.fail on casper\n");
        spawn(300,"ssh -i ~/.ssh/cluster_key regression\@casper.ghostscript.com \"touch /home/regression/cluster/$machine.fail\"");
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
      my $s;
      $s=waitpid($pid,WNOHANG);
      if ($s<0) {
        addToLog($pids{$pid}{'filename'});
        delete $pids{$pid};
      } else {
        $count++;
      }
    }
  }

  my $clusterRegressionRunning=0;
  $clusterRegressionRunning=1 if ($local && -e "/home/marcos/cluster/$runningSemaphore");  # hack: directory name shouldn't be hard coded
  sleep 5 if ($clusterRegressionRunning);

  if (scalar(@commands)>0 && $count<$maxCount && !$clusterRegressionRunning) {
    my $n=rand(scalar @commands);
    my @a=split '\t',$commands[$n];
    splice(@commands,$n,1);
    my $filename=$a[0];
    my $cmd=$a[1];
    $jobs++;
    my $t=int($jobs*$maxTimeoutPercentage/100+0.5);
    $maxTimeout=$t if ($maxTimeout<$t);
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
      if ($elapsedTime>=30) {
        my $t=sprintf "%d tests completed",$jobs;
        updateStatus($t);
        mylog("updateStatus() done");
        $abort=checkAbort;
        mylog("checkAbort() done");
        $startTime=time;

#       my @files = <$raster/*>;
#       my $count = @files;
        my $count2=scalar (keys %timeOuts);
        unlink('raster.yes') if ($count>$maxRaster);
        mylog("timeOuts=$count2 maxTimeout=$maxTimeout");

      }
#     printf "\n$t1 $t2 $t3  %5d %5d  %3d",$jobs,$totalJobs, $percentage if ($debug);
    }
    my $pid = fork();
    if (not defined $pid) {
      mylog "fork() failed";
      die "fork() failed";
    } elsif ($pid == 0) {
      exec($cmd);
#     system("$cmd");  marcos
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

if (!$abort || $compileFail ne "" || $timeoutFail ne "") {

  print "\n" if ($debug);
  my $count;
  my $startTime=time;
  my $lastCount=-1;
  do {
    my $tempCount=scalar keys %pids;
    if ($tempCount != $lastCount && $tempCount>0) {
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
    my %name;
    foreach (@a) {
      if (m/\S+ +(\d+) +(\d+) .+ \d+:\d\d.\d\d (.+)$/ && !m/<defunct>/ && !m/\(sh\)/) {
        $children{$2}=$1;
        $name{$1}=$3;
      }
    }

    foreach my $pid (keys %pids) {
      if (time-$pids{$pid}{'time'} >= $timeOut) {
        my $p=$pid;
        while (exists $children{$p}) {
#         mylog "$p->$children{$p}\n"; # mhw
          $p=$children{$p};
          $name{$p}='missing' if (!exists $name{$p});
          mylog ("killing (timeout 2) $p $name{$p}\n");
          kill 1, $p;
          kill 9, $p;
        }
        $name{$pid}='missing' if (!exists $name{$pid});
        mylog ("killing (timeout 2) $pid $name{$pid}\n");
        kill 1, $pid;
        kill 9, $pid;
        $timeOuts{$pids{$pid}{'filename'}}=1;
        addToLog($pids{$pid}{'filename'});
        my $count=scalar (keys %timeOuts);
        delete $pids{$pid};

#       mylog "killed:  $p ($pid) $pids{$pid}{'filename'}  total $count\n";  # mhw
#       mylog "killed:  $pids{$pid}{'filename'}\n";  # mhw
        if ($count>=$maxTimeout) {
          $timeoutFail="too many timeouts";
          mylog("setting $machine.fail on casper\n");
          spawn(300,"ssh -i ~/.ssh/cluster_key regression\@casper.ghostscript.com \"touch /home/regression/cluster/$machine.fail\"");
          updateStatus('Timeout fail');
          @commands=();
          $maxCount=0;
          killAll();
          checkAbort();
          $poll=0;
          $abort=1;
        }
      } else {
#       print "\n$pids{$pid}{'filename'} ".(time-$pids{$pid}{'time'}) if ((time-$pids{$pid}{'time'})>20);
        my $s;
        $s=waitpid($pid,WNOHANG);
        if ($s<0) {
          addToLog($pids{$pid}{'filename'});
          delete $pids{$pid};
        } else {
          $count++;
        }

      }
    }
    print "$count " if ($debug);
    select(undef, undef, undef, 1.00);
    } while ($count>0 && !$abort);
  print "\n" if ($debug);

# if (!$abort) {

  mylog "done reading $temp directory";

  #print Dumper(\%logfiles);

  if ($md5sumFail || $compileFail || $timeoutFail) {
    close(F4);
    open(F4,">$machine.log");
    if ($md5sumFail ne "") {
      print F4 "md5sumFail: $md5sumFail";
    }
    if ($compileFail ne "") {
      print F4 "compileFail: $compileFail\n\n";
      my $dir="";
      $dir="ghostpdl" if ($revs);
      $dir="users/$user/ghostpdl" if ($user);
      $dir="icc_work" if ($icc_work);
      foreach my $i ('gs/makegs.out','makepcl.out','makexps.out','makesvg.out') {
        my $count=0;
        my $start=-1;
        if (open(F3,"<$dir/$i")) {
          while(<F3>) {
            $start=$count if (m/^gcc/);
            $count++;
          }
          close(F3);
          my $t1=$count;
          $count-=20;
          $count=$start-5 if ($start!=-1);
          $count=$t1-10 if ($t1-$count<10);
          $count=$t1-250 if ($t1-$count>250);
          $t1-=$count;
          print F4 "\n$i (last $t1 lines):\n\n";
          if (open(F3,"<$dir/$i")) {
            while(<F3>) {
              if ($count--<0) {
                print F4 $_;
              }
            }
            close(F3);
          }
        }
      }
    }
    if ($timeoutFail ne "") {
      print F4 "timeoutFail: $timeoutFail\n";
    }
  } else {
    updateStatus('Collecting log files');

    my %logfiles;
    opendir(DIR, $temp) || die "can't opendir $temp: $!";
    foreach (readdir(DIR)) {
      $logfiles{$1}=1 if (m/(.+)\.log$/);
    }
    closedir DIR;
    print F4 "\n\nmissed logs:\n";
    my $lastTime=time;
#   my $tempcount=0;
    foreach my $logfile (keys %logfiles) {
#      mylog "$tempcount log files processed" if (((++$tempcount) % 1000) == 0);
      if ($lastTime-time>60) {
        $abort=checkAbort;
        $lastTime=time;
      }
      addToLog($logfile) if (!exists $logged{$logfile});
    }
  }

  close(F4);

  if (!$local) {
    updateStatus('Uploading log files');
    unlink "$machine.log.gz";
    unlink "$machine.out.gz";
    `gzip $machine.log`;
    `gzip $machine.out`;

    mylog "about to upload $machine.log.gz";
    $cmd="scp -q -o ConnectTimeout=30 -i ~/.ssh/cluster_key $machine.log.gz regression\@casper.ghostscript.com:/home/regression/cluster/$machine.log.gz";
    systemWithRetry($cmd);
    mylog "done with uploading $machine.log.gz";

    mylog "about to upload $machine.out.gz";
    $cmd="scp -q -o ConnectTimeout=30 -i ~/.ssh/cluster_key $machine.out.gz regression\@casper.ghostscript.com:/home/regression/cluster/$machine.out.gz 2>&1";
    systemWithRetry($cmd);
    mylog "done with uploading $machine.out.gz";

    updateStatus('idle');
  } else {
    updateStatus('done');
  }
# }
} # if (!$abort)

if ($abort) {
  updateStatus('Abort command received');
}

if (!$local) {
  mylog("setting $machine.done on casper\n");
  spawn(300,"ssh -i ~/.ssh/cluster_key regression\@casper.ghostscript.com \"touch /home/regression/cluster/$machine.done\"");
  mylog("removing $machine.abort on casper\n");
  spawn(70,"ssh -i ~/.ssh/cluster_key regression\@casper.ghostscript.com \"rm /home/regression/cluster/$machine.abort\"");
}

unlink $runningSemaphore;

if (!$local) {
  close(LOG);
}

