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


my $machine=shift || die "usage: run.pl machine_name";
my $desiredRev;

my $maxCount=12;
$maxCount=16;

my $timeOut=300;

my $baseDirectory=`pwd`;
chomp $baseDirectory;

my $temp="./temp";
my $temp2="./temp.tmp";

my $gpdlSource=$baseDirectory."/ghostpdl";
my $gsSource=$gpdlSource."/gs";
my $gsBin=$baseDirectory."/gs";
 
my $abort=0;
unlink ("$machine.abort");

local $| = 1;

my %testSource=(
  $baseDirectory."/tests/pdf" => 'gs',
  $baseDirectory."/tests/ps" => 'gs',
  $baseDirectory."/tests/eps" => 'gs',
  $baseDirectory."/tests_private/ps/ps3cet" => 'gs',
  $baseDirectory."/tests_private/comparefiles" => 'gs',
  $baseDirectory."/tests_private/pdf/PDFIA1.7_SUBSET" => 'gs',
  $baseDirectory."/tests/pcl" => 'pcl',
  $baseDirectory."/tests_private/pcl/pcl5cfts" => 'pcl',
  $baseDirectory."/tests_private/pcl/pcl5efts" => 'pcl',
  $baseDirectory."/tests_private/xl/pxlfts3.0" => 'pcl',
  $baseDirectory."/tests/xps" => 'xps',
  $baseDirectory."/tests_private/xps/xpsfts-a4" => 'xps',
  $baseDirectory."/tests/svg/svgw3c-1.1-full/svg" => 'svg',
# $baseDirectory."/tests/svg/svgw3c-1.1-full/svgHarness" => 'svg',
# $baseDirectory."/tests/svg/svgw3c-1.1-full/svggen" => 'svg',
# $baseDirectory."/tests/svg/svgw3c-1.2-tiny/svg" => 'svg',
# $baseDirectory."/tests/svg/svgw3c-1.2-tiny/svgHarness" => 'svg',
# $baseDirectory."/tests/svg/svgw3c-1.2-tiny/svggen" => 'svg'
);


system("date") if ($debug2);

open(F,"<$machine.jobs") || die "file $machine.jobs not found";
my @commands;
my $t=<F>;
chomp $t;
if ($t =~ m/^\d+$/) {
  $desiredRev=$t;
} else {
  push @commands, $t;
}
while(<F>) {
  chomp;
  push @commands, $_;
}
close(F);

my $cmd;
my $s;
my $previousRev1;
my $previousRev2;
my $newRev1=99999;
my $newRev2=99999;

my %spawnPIDs;

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
  spawn(5,"scp -i ~/.ssh/cluster_key  marcos\@casper.ghostscript.com:/home/marcos/cluster/$machine.abort .");
  if (open(F,"<$machine.abort")) {
    close(F);
    return(1);
  }
  return(0);
}

sub updateStatus($) {
  my $message=shift;
  `echo '$message' >$machine.status`;
  spawn(0,"scp -i ~/.ssh/cluster_key $machine.status marcos\@casper.ghostscript.com:/home/marcos/cluster/$machine.status");
}



updateStatus('Updating test files');

#update the regression file source directory
if ($updateTestFiles) {

  foreach my $testSource (sort keys %testSource) {
  $cmd="cd $testSource ; export SVN_SSH=\"ssh -i \$HOME/.ssh/cluster_key\" ; svn update";
  print "$cmd\n" if ($verbose);
  `$cmd`;
  }

}

$abort=checkAbort;
if (!$abort) {
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

updateStatus('Building Ghostscript');

# build ghostscript
$cmd="cd $gsSource ; make distclean ; ./autogen.sh \"CC=gcc -m$wordSize\" --disable-cups --disable-fontconfig --disable-cairo --prefix=$gsBin >autogen.out 2>&1 ; make -j 12 >make1.out 2>&1 ; make  >make2.out 2>&1";
print "$cmd\n" if ($verbose);
`$cmd`;

updateStatus('Installing Ghostscript');

#install ghostscript
$cmd="rm -fr $gsBin ; cd $gsSource ; make install";
print "$cmd\n" if ($verbose);
`$cmd`;
}

if (1) {
$abort=checkAbort;
if (!$abort) {
updateStatus('Building GhostPCL');
$cmd="cd $gpdlSource ; make pcl-clean ; make pcl \"CC=gcc -m$wordSize\" \"CCLD=gcc -m$wordSize\" -j 12 >makepcl1.out 2>&1 ; make pcl \"CC=gcc -m$wordSize\" \"CCLD=gcc -m$wordSize\" -j 12 >makepcl2.out 2>&1 ; make pcl \"CC=gcc -m$wordSize\" \"CCLD=gcc -m$wordSize\" >makepcl3.out 2>&1";
print "$cmd\n" if ($verbose);
`$cmd`;
}

$abort=checkAbort;
if (!$abort) {
updateStatus('Building GhostXPS');
$cmd="cd $gpdlSource ; make xps-clean ; make xps \"CC=gcc -m$wordSize\" \"CCLD=gcc -m$wordSize\" -j 12 >makexps1.out 2>&1 ; make xps \"CC=gcc -m$wordSize\" \"CCLD=gcc -m$wordSize\" -j 12 >makexps2.out 2>&1 ; make xps \"CC=gcc -m$wordSize\" \"CCLD=gcc -m$wordSize\" >makexps3.out 2>&1";
print "$cmd\n" if ($verbose);
`$cmd`;
}

$abort=checkAbort;
if (!$abort) {

updateStatus('Building GhostSVG');
$cmd="cd $gpdlSource ; make svg-clean ; make svg \"CC=gcc -m$wordSize\" \"CCLD=gcc -m$wordSize\" -j 12 >makesvg1.out 2>&1 ; make svg \"CC=gcc -m$wordSize\" \"CCLD=gcc -m$wordSize\" -j 12 >makesvg2.out 2>&1 ; make svg \"CC=gcc -m$wordSize\" \"CCLD=gcc -m$wordSize\" >makesvg3.out 2>&1";
print "$cmd\n" if ($verbose);
`$cmd`;
}
}

}

unlink "link.icc","wts_plane_0","wts_plane_1","wts_plane_2","wts_plane_3";
$cmd="cp -p ghostpdl/link.icc ghostpdl/wts_plane_0 ghostpdl/wts_plane_1 ghostpdl/wts_plane_2 ghostpdl/wts_plane_3 .";
print "$cmd\n" if ($verbose);
`$cmd`;


updateStatus('Starting jobs');



my $totalJobs=scalar(@commands);
my $jobs=0;

my %pids;
my %timeOuts;

my $startTime=time;
my $lastPercentage=-1;


while (scalar(@commands) && !$abort) {
  my $count=0;

  my $a=`ps -ef`;
  my @a=split '\n',$a;
  my %children;
  foreach (@a) {
    if (m/(\d+) +(\d+)/ && !m/<defunct>/) {
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
print "\nkilled:  $p ($pid) $pids{$pid}{'filename'}\n";
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
    my @a=split '\t',$commands[$n];  splice(@commands,$n,1);
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
      if ($percentage != $lastPercentage && ($percentage % 5) == 0) {
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
    if (m/(\d+) +(\d+)/ && !m/<defunct>/) {
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
print "\nkilled:  $p ($pid) $pids{$pid}{'filename'}\n";
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
spawn(60,"scp -i ~/.ssh/cluster_key $machine.log.gz marcos\@casper.ghostscript.com:/home/marcos/cluster/$machine.log.gz");

system("date") if ($debug2);
updateStatus('idle');
} # if (!$abort)
spawn(10,"ssh -i ~/.ssh/cluster_key marcos\@casper.ghostscript.com \"touch /home/marcos/cluster/$machine.done\"");

system("date") if ($debug2);
#`rm -fr $temp`;
system("date") if ($debug2);

if (0) {
foreach my $pid (keys %spawnPIDs) {
  my $status1=waitpid($pid,WNOHANG);
  select(undef, undef, undef, 0.1);
  my $status2=waitpid($pid,WNOHANG);
  print "$pid: $status1 $status2   ";
}
print "\n";
}


