#!/usr/bin/perl

use strict;
use warnings;

use Data::Dumper;
use POSIX ":sys_wait_h";

my $updateBaseline=0;
my $rerunIfMd5sumDifferences=0;

my %allowedProducts=(
  'gs'  => 1,
  'pcl' => 1,
  'xps' => 1,
  'svg' => 1,
  'ls' => 1,
  'mupdf' => 1
);

my $lowres=0;
my $highres=0;
my %products;
my $bmpcmp=0;
my $local=0;
my $filename="";
my $bmpcmpOptions;
my $bmpcmp_icc_work=0;

my $t;

while ($t=shift) {
  if ($t eq "baseline") {
    $updateBaseline=1;
    $rerunIfMd5sumDifferences=0;
  } elsif ($t eq "lowres") {
    $lowres=1;
  } elsif ($t eq "highres") {
    $highres=1;
  } elsif ($t eq "bmpcmp_icc_work") {
    $bmpcmp_icc_work=1;
  } elsif ($t eq "bmpcmp") {
    $bmpcmp=1;
    $filename=shift;
    $filename.=".txt";
    $bmpcmpOptions=shift;
    $bmpcmpOptions="" if (!$bmpcmpOptions);
  } elsif ($t eq "local") {
    $local=1;
  } else {
    $products{$t}=1;
    die "usage: build.pl [gs] [pcl] [xps] [svg] [ls] [mupdf]" if (!exists $allowedProducts{$t});
  }
}

my $additionalOptions="";
my $additionalOptions_gs="";
my $additionalOptions_pcl="";
my $additionalOptions_pdfwrite_step1="";
my $additionalOptions_pdfwrite_step2="";
my $additionalOptions_gs_pdfwrite_step1="";
my $additionalOptions_gs_pdfwrite_step2="";
if (open(F,"<weekly.cfg")) {
  while(<F>) {
    chomp;
    my @a=split ' ',$_,2;
    if ($a[0] eq "runoption") {
      $additionalOptions.=' ' if (length($additionalOptions));
      $additionalOptions.=$a[1];
    }
    if ($a[0] eq "gs_runoption") {
      $additionalOptions_gs.=' ' if (length($additionalOptions_gs));
      $additionalOptions_gs.=$a[1];
    }
    if ($a[0] eq "runoption_pdfwrite_step1") {
      $additionalOptions_pdfwrite_step1.=' ' if (length($additionalOptions_pdfwrite_step1));
      $additionalOptions_pdfwrite_step1.=$a[1];
    }
    if ($a[0] eq "runoption_pdfwrite_step2") {
      $additionalOptions_pdfwrite_step2.=' ' if (length($additionalOptions_pdfwrite_step2));
      $additionalOptions_pdfwrite_step2.=$a[1];
    }
    if ($a[0] eq "gs_runoption_pdfwrite_step1") {
      $additionalOptions_gs_pdfwrite_step1.=' ' if (length($additionalOptions_gs_pdfwrite_step1));
      $additionalOptions_gs_pdfwrite_step1.=$a[1];
    }
    if ($a[0] eq "gs_runoption_pdfwrite_step2") {
      $additionalOptions_gs_pdfwrite_step2.=' ' if (length($additionalOptions_gs_pdfwrite_step2));
      $additionalOptions_gs_pdfwrite_step2.=$a[1];
    }
  }
  close(F);
}

my $updateTestFiles=1;
my $verbose=0;

local $| = 1;
my %md5sum;
my %skip;

if ($rerunIfMd5sumDifferences) {
  if ($local) {
  open(F,"<baseline.tab") || die "file baseline.tab not found";
  while(<F>) {
    chomp;
    my @a=split '\t';
    $md5sum{$a[0]}=$a[6];
  }
  close(F);
  } else {
  open(F,"<current.tab") || die "file current.tab not found";
  while(<F>) {
    chomp;
    my @a=split '\t';
    $md5sum{$a[0]}=$a[6];
  }
  close(F);
  open(F,"<md5sum.cache") || die "file md5sum.cache not found";
  my $header=<F>;
  while(<F>) {
    chomp;
    my @a=split ' ';
    $a[0]=~s|/|__|g;
    $md5sum{$a[0]}.= '|'.$a[1];
  }
  close(F);
  if (open(F,"<skip.lst")) {
    while(<F>) {
      chomp;
      $skip{$_}=1;
    }
    close(F);
  }
  }
}

#print Dumper(\%skip);  exit;





my $baseDirectory='./';

my $svnURLPrivate='file:///var/lib/svn-private/ghostpcl/trunk/';
my $svnURLPublic ='http://svn.ghostscript.com/ghostscript/';

if ($local) {
$svnURLPrivate='svn+ssh://svn.ghostscript.com/var/lib/svn-private/ghostpcl/trunk/';
$svnURLPublic='http://svn.ghostscript.com/ghostscript/';
}

my $preCommand="";

my $temp="./temp";
#$temp="/tmp/space/temp";
#$temp="/dev/shm/temp";
#$temp="/media/sdd/temp";

my $raster="$temp/raster";
my $bmpcmpDir="$temp/bmpcmp";
my $baselineRaster="./baselineraster";

my $gsBin=$baseDirectory."gs/bin/gs";
if ($local) {
  $gsBin.=" -I./gs/lib";
}
my $pclBin=$baseDirectory."gs/bin/pcl6";
my $xpsBin=$baseDirectory."gs/bin/gxps";
my $svgBin=$baseDirectory."gs/bin/gsvg";
my $lsBin=$baseDirectory."gs/bin/pspcl6";
my $mupdfBin=$baseDirectory."gs/bin/pdfdraw";
my $timeBin=$baseDirectory."gs/bin/time";

# mupdf uses the same test files as gs but only ones ending in pdf (or PDF)


my %testSource=(
  $svnURLPublic."tests/pdf" => 'gs',
  $svnURLPublic."tests/ps" => 'gs',
  $svnURLPublic."tests/eps" => 'gs',
  $svnURLPrivate."tests_private/ps/ps3cet" => 'gs',
  $svnURLPrivate."tests_private/comparefiles" => 'gs',
  $svnURLPrivate."tests_private/pdf/PDFIA1.7_SUBSET" => 'gs',
  $svnURLPrivate."tests_private/pdf/PDF_1.7_FTS" => 'gs',

  $svnURLPublic."tests/pcl" => 'pcl',
  $svnURLPrivate."tests_private/customer_tests" => 'pcl',
  $svnURLPrivate."tests_private/pcl/pcl5cfts" => 'pcl',
  $svnURLPrivate."tests_private/pcl/pcl5cats/Subset" => 'pcl',
  $svnURLPrivate."tests_private/pcl/pcl5efts" => 'pcl',
  $svnURLPrivate."tests_private/pcl/pcl5ccet" => 'pcl',
  $svnURLPrivate."tests_private/xl/pxlfts3.0" => 'pcl',
  $svnURLPrivate."tests_private/xl/pcl6cet" => 'pcl',
  $svnURLPrivate."tests_private/xl/pcl6cet3.0" => 'pcl',
  $svnURLPrivate."tests_private/xl/pxlfts" => 'pcl',
  $svnURLPrivate."tests_private/xl/pxlfts2.0" => 'pcl',

# $svnURLPublic."tests/xps" => 'xps',
  $svnURLPrivate."tests_private/xps/xpsfts-a4" => 'xps',

# $svnURLPublic."tests/svg/svgw3c-1.1-full/svg" => 'svg',
  # $svnURLPublic."tests/svg/svgw3c-1.1-full/svgHarness" => 'svg',
  # $svnURLPublic."tests/svg/svgw3c-1.1-full/svggen" => 'svg',
# $svnURLPublic."tests/svg/svgw3c-1.2-tiny/svg" => 'svg',
  # $svnURLPublic."tests/svg/svgw3c-1.2-tiny/svgHarness" => 'svg',
  # $svnURLPublic."tests/svg/svgw3c-1.2-tiny/svggen" => 'svg',

  $svnURLPublic."tests/language_switch" => 'ls',
  $svnURLPrivate."tests_private/language_switch" => 'ls'
  );

my $cmd;
my $s;
my $previousRev;
my $newRev=99999;

my %tests=(
  'gs' => [
    "pbmraw.72.0",
    "pbmraw.300.0",
    "pbmraw.300.1",
    "pgmraw.72.0",
    "pgmraw.300.0",
    "pgmraw.300.1",
    "pkmraw.72.0",
    "pkmraw.300.0",
    "pkmraw.300.1",
    "ppmraw.72.0",
    "ppmraw.300.0",
    "ppmraw.300.1",
    "pam.72.0",
    "psdcmyk.72.0",
    "cups.300.1",
##"psdcmyk.300.0",
##"psdcmyk.300.1",
    "pdf.ppmraw.72.0",
    "pdf.ppmraw.300.0",
    "pdf.pkmraw.300.0",
    "ps.ppmraw.72.0"
   ],
  'pcl' => [
    "pbmraw.75.0",
#   "pbmraw.600.0",
    "pbmraw.600.1",
#   "pgmraw.75.0",
#   "pgmraw.600.0",
#   "pgmraw.600.1",
    #"wtsimdi.75.0",
    #"wtsimdi.600.0",
    #"wtsimdi.600.1",
    "ppmraw.75.0",
#   "ppmraw.600.0",
    "ppmraw.600.1",
#   "bitrgb.75.0",
#   "bitrgb.600.0",
#   "bitrgb.600.1",
    #"psdcmyk.75.0",
##"psdcmyk.600.0",
##"psdcmyk.600.1",
    "pdf.ppmraw.75.0",
    "pdf.ppmraw.600.0"
#   "pdf.pkmraw.600.0"
    ],
  'xps' => [
    "pbmraw.72.0",
##"pbmraw.300.0",
##"pbmraw.300.1",
    #"pgmraw.72.0",
##"pgmraw.300.0",
##"pgmraw.300.1",
##"wtsimdi.72.0",
##"wtsimdi.300.0",
##"wtsimdi.300.1",
    "ppmraw.72.0",
##"ppmraw.300.0",
##"ppmraw.300.1",
    "bitrgb.72.0",
##"bitrgb.300.0",
##"bitrgb.300.1",
##"psdcmyk.72.0",
###"psdcmyk.300.0",
###"psdcmyk.300.1",
    "pdf.ppmraw.72.0",
##"pdf.ppmraw.300.0",
##"pdf.pkmraw.300.0"
    ],
  'svg' => [
#   "pbmraw.72.0",
    #"pbmraw.300.0",
    #"pbmraw.300.1",
#   "pgmraw.72.0",
    #"pgmraw.300.0",
    #"pgmraw.300.1",
    #"wtsimdi.72.0",
    #"wtsimdi.300.0",
    #"wtsimdi.300.1",
    "ppmraw.72.0",
    #"ppmraw.300.0",
    #"ppmraw.300.1",
#   "bitrgb.72.0",
    #"bitrgb.300.0",
    #"bitrgb.300.1",
    #"psdcmyk.72.0",
    #"psdcmyk.300.0",
    #"psdcmyk.300.1",
#   "pdf.ppmraw.72.0",
    #"pdf.ppmraw.300.0",
    #"pdf.pkmraw.300.0"
    ],
   'ls' => [
    "pbmraw.72.0",
    #"pbmraw.300.0",
    #"pbmraw.300.1",
    "pgmraw.72.0",
    #"pgmraw.300.0",
    #"pgmraw.300.1",
    #"wtsimdi.72.0",
    #"wtsimdi.300.0",
    #"wtsimdi.300.1",
    "ppmraw.72.0",
    #"ppmraw.300.0",
    #"ppmraw.300.1",
    "bitrgb.72.0",
    #"bitrgb.300.0",
    #"bitrgb.300.1",
    #"psdcmyk.72.0",
    #"psdcmyk.300.0",
    #"psdcmyk.300.1",
#   "pdf.ppmraw.72.0",
    #"pdf.ppmraw.300.0",
    #"pdf.pkmraw.300.0"
    ],
  'mupdf' => [
# mupdf only supports ppmraw output, so don't bother specifying anything else
    "ppmraw.72.0",
    "ppmraw.300.0",
    "ppmraw.300.1"
    ]
  );

my %testfiles;
#update the regression file source directories
if (0) {
if ($updateTestFiles) {
  foreach my $testSource (sort keys %testSource) {
    $cmd="cd $testSource ; svn update";
    print STDERR "$cmd\n" if ($verbose);
    `$cmd`;
  }
}

# build a list of the source files
foreach my $testSource (sort keys %testSource) {
  if (scalar keys %products==0 || exists $products{$testSource{$testSource}}) {
    opendir(DIR, $testSource) || die "can't opendir $testSource: $!";
    foreach (readdir(DIR)) {
      $testfiles{$testSource.'/'.$_}=$testSource{$testSource} if (!-d $testSource.'/'.$_ && ! m/^\./ && ! m/.disabled$/);
    }
    closedir DIR;
  }
}
}


if (!$bmpcmp) {

foreach my $testSource (sort keys %testSource) {
  if (scalar keys %products==0 || exists $products{$testSource{$testSource}} || ($testSource{$testSource} eq 'gs' && exists $products{'mupdf'})) {
#print "$testSource\n";
    my $a=`svn list $testSource`;

    my %nightly_only;
    if (!$local) {
      unlink "nightly_only.lst";
      my $b=`svn export $testSource/nightly_only.lst 2> /dev/null`;
      if (open(F,"<nightly_only.lst")) {
        foreach (<F>) {
          chomp;
          $nightly_only{$_}=1;
        }
        close(F);
        unlink "nightly_only.lst";
      }
    }

    my $t1=$testSource;
    my $t2=$svnURLPrivate;
    my $t3=$svnURLPublic ;
    $t1 =~ s/\+//g;
    $t2 =~ s/\+//g;
    $t3 =~ s/\+//g;
    $t1 =~ s|$t2||;
    $t1 =~ s|$t3||;

#   print "$testSource{$testSource}\n";  exit;

    my @a=split '\n',$a;
    foreach (@a) {
      chomp;
      if (exists $nightly_only{$_} || $_ eq "nightly_only.lst") {
#print STDERR "skipping: $_\n";
      } else {
        my $testfile=$t1.'/'.$_;
        if (exists $products{'mupdf'}) {
          $testfiles{'./'.$testfile}='mupdf'                  if (!($testfile =~ m|/$|) && !($testfile =~ m/^\./) && !($testfile =~ m/.disabled$/) && $testfile =~ m/.pdf$/i);
        } else {
          $testfiles{'./'.$testfile}=$testSource{$testSource} if (!($testfile =~ m|/$|) && !($testfile =~ m/^\./) && !($testfile =~ m/.disabled$/));
        }
#       print "$testfile\n";
      }
    }
  }
}
}

#print Dumper(\%testfiles); exit;

sub build($$$$$) {
  my $product=shift;  # gs|pcl|xps|svg|ls
  my $inputFilename=shift;
  my $options=shift;
  my $md5sumOnly=shift;
  my $bmpcmp=shift;

  if ($md5sumOnly) {
    $preCommand = "nice $timeBin -f \"%U %S %E %P\"";
  } else {
    $preCommand = "$timeBin -f \"%U %S %E %P\"";
  }

  my $cmd="";
  my $cmd1a="";
  my $cmd1b="";
  my $cmd1c="";
  my $cmd2a="";
  my $cmd2b="";
  my $cmd2c="";
  my $outputFilenames="";
  my $rmCmd="true";

  my @a=split '\.',$options;

  my $filename=$inputFilename;
  $filename =~ s|.+/||;

  my $tempname=$inputFilename;
  $tempname =~ s|^./||;
  $tempname =~ s|/|__|g;
  my $logFilename="$temp/$tempname.$options.log";
  my $md5Filename="$temp/$tempname.$options.md5";
  my $filename2="$tempname.$options";

  my $outputFilename;
  my $baselineFilename;
  my $rasterFilename;
  my $bmpcmpFilename;

  # $cmd .= " touch $logFilename ; rm -f $logFilename ";

  $cmd  .= " true ";
  $cmd  .= "; touch $md5Filename" if (!$updateBaseline && !$bmpcmp);


  if ($a[0] eq 'pdf' || $a[0] eq 'ps') {

    if ($a[0] eq 'pdf') {
      $cmd .= " ; echo \"$product pdfwrite\" >>$logFilename ";
      $outputFilename="$temp/$tempname.$options.pdf";
    } else {
      $cmd .= " ; echo \"$product ps2write\" >>$logFilename ";
      $outputFilename="$temp/$tempname.$options.ps";
    }

    if ($product eq 'gs') {
      $cmd1a.="$gsBin";
    } elsif ($product eq 'pcl') {
      $cmd1a.="$pclBin";
    } elsif ($product eq 'xps') {
      $cmd1a.="$xpsBin";
    } elsif ($product eq 'svg') {
      $cmd1a.="$svgBin";
    } elsif ($product eq 'ls') {
      $cmd1a.="$lsBin";
    } else {
      die "unexpected product: $product";
    }
    $cmd1b.=" -sOutputFile=$outputFilename";
    if ($a[0] eq 'pdf') {
      $cmd1c.=" -sDEVICE=pdfwrite";
    } else {
      $cmd1c.=" -sDEVICE=ps2write";
    }
    $cmd1c.=" ".$additionalOptions_pdfwrite_step1;
    $cmd1c.=" ".$additionalOptions_gs_pdfwrite_step1 if ($product eq 'gs');
    $cmd1c.=" -r".$a[2];
#   $cmd1.=" -q" if ($product eq 'gs');
    $cmd1c.=" -sDEFAULTPAPERSIZE=letter" if ($product eq 'gs');
    $cmd1c.=" -dNOPAUSE -dBATCH";  # -Z:
#   $cmd1c.=" -dNOOUTERSAVE -dJOBSERVER -c false 0 startjob pop -f" if ($product eq 'gs');
    $cmd1c.=" -dJOBSERVER" if ($product eq 'gs');

    $cmd1c.=" %rom%Resource/Init/gs_cet.ps" if ($filename =~ m/.PS$/ && $product eq 'gs');
#   $cmd1.=" -dFirstPage=1 -dLastPage=1" if ($filename =~ m/.pdf$/i || $filename =~ m/.ai$/i);

    $cmd1c.=" - < " if (!($filename =~ m/.pdf$/i || $filename =~ m/.ai$/i) && $product eq 'gs');

    $cmd1c.=" $inputFilename";
#   $cmd.=" 2>&1";

    if ($bmpcmp) {
      $cmd.=" ; $preCommand $cmd1a $cmd1b $cmd1c >>$logFilename 2>&1";
      if ($bmpcmp_icc_work) {
        $cmd1a =~ s|/gs/|/icc_work/|;
      } else {
        $cmd1a =~ s|/gs/|/head/|;
      }
      $cmd1b =~ s|$temp|$baselineRaster|;
      $cmd.=" ; $preCommand $cmd1a $cmd1b $cmd1c >>$logFilename 2>&1";
    } else {
      $cmd.=" ; echo \"$cmd1a $cmd1b $cmd1c\" >>$logFilename ";
      $cmd.=" ; $preCommand $cmd1a $cmd1b $cmd1c >>$logFilename 2>&1";
    }

    $cmd.=" ; echo '---' >>$logFilename";

    my $inputFilename=$outputFilename;

    $outputFilename="$temp/$tempname.$options";
    $baselineFilename="$baselineRaster/$tempname.$options";
    $rasterFilename="$raster/$tempname.$options";
    $bmpcmpFilename="$bmpcmpDir/$tempname.$options";

    $cmd2a.=" $gsBin";
    if ($updateBaseline) {
      $cmd2b.=" -sOutputFile='|gzip -1 -n >$baselineFilename.gz'";
    } elsif ($md5sumOnly) {
      $cmd2b.=" -sOutputFile='|md5sum >>$md5Filename'";
    } else {
      if ($local) {
#       $cmd2b.=" -sOutputFile='|gzip -1 -n >$outputFilename.gz'";
        if ($rerunIfMd5sumDifferences) {
          $cmd2b.=" -sOutputFile='|md5sum >>$md5Filename'";
        } else {
          $cmd2b.=" -sOutputFile='|lzop -f -o $outputFilename.lzo'";
        }
      } else {
        $cmd2b.=" -sOutputFile='|gzip -1 -n >$outputFilename.gz'";
      }
#     $cmd2b.=" -sOutputFile='|gzip -1 -n | md5sum >>$md5Filename'";
    }

    $cmd2c.=" -dMaxBitmap=30000000" if ($a[3]==0);
    $cmd2c.=" -dMaxBitmap=10000"    if ($a[3]==1);

    $cmd2c.=" -sDEVICE=".$a[1];
    $cmd2c.=" -dGrayValues=256" if ($a[0] eq 'bitrgb');
    $cmd2c.=" -dcupsColorSpace=0" if ($a[0] eq 'cups');
    $cmd2c.=" ".$additionalOptions_pdfwrite_step2;
    $cmd2c.=" ".$additionalOptions_gs_pdfwrite_step2 if ($product eq 'gs');
    $cmd2c.=" -r".$a[2];
#   $cmd2c.=" -q"
    $cmd2c.=" -sDEFAULTPAPERSIZE=letter" if ($product eq 'gs');
    $cmd2c.=" -dNOPAUSE -dBATCH -K1000000";  # -Z:
#   $cmd2c.=" -dNOOUTERSAVE -dJOBSERVER -c false 0 startjob pop -f";
    $cmd2c.=" -dJOBSERVER";

    #   $cmd2c.=" -dFirstPage=1 -dLastPage=1";

    $cmd2c.=" $inputFilename";

    if ($bmpcmp) {
      $cmd.=" ; $preCommand $cmd2a -sOutputFile='|gzip -1 -n >$outputFilename.gz' $cmd2c >>$logFilename 2>&1";
      if ($bmpcmp_icc_work) {
        $cmd2a =~ s|/gs/|/icc_work/|;
      } else {
        $cmd2a =~ s|/gs/|/head/|;
      }
      $cmd2c =~ s|$temp|$baselineRaster|;
      $cmd.=" ; $preCommand $cmd2a -sOutputFile='|gzip -1 -n >$baselineFilename.gz' $cmd2c >>$logFilename 2>&1";
      $cmd.=" ; bash -c \"nice ./bmpcmp $bmpcmpOptions <(gunzip -c $outputFilename.gz) <(gunzip -c $baselineFilename.gz) $bmpcmpFilename 0 100\""; # ; gzip $bmpcmpFilename.* ";
      $cmd.=" ; ./checkSize.pl $bmpcmpFilename.*";
      $cmd.=" ; bash -c \"for (( c=1; c<=5; c++ )); do scp -q -o ConnectTimeout=30 -i ~/.ssh/cluster_key $bmpcmpFilename.* regression\@casper.ghostscript.com:/home/regression/cluster/bmpcmp/. ; t=\\\$?; if [ \\\$t == 0 ]; then break; fi; echo 'scp retry \$c' ; done \"";
#     $cmd.=" ; scp -q -o ConnectTimeout=30 -i ~/.ssh/cluster_key $logFilename regression\@casper.ghostscript.com:/home/regression/cluster/bmpcmp/.";
    } else {
      $cmd.=" ; echo \"$cmd2a $cmd2b $cmd2c\" >>$logFilename ";
      $cmd.=" ; $preCommand $cmd2a $cmd2b $cmd2c >>$logFilename 2>&1";
      if ($local) {
#       $cmd.=" ; zcat $outputFilename.gz | md5sum >$md5Filename";
        if ($rerunIfMd5sumDifferences) {
        } else {
          $cmd.=" ; sleep 0 ; lzop -c -d $outputFilename.lzo | md5sum >$md5Filename";
        }
#       $cmd.=" ; md5sum $outputFilename.gz >$md5Filename";
      }
   }

    if ($rerunIfMd5sumDifferences && exists $md5sum{$filename2} && !exists $skip{$filename2}) {
      $cmd.=" ; sleep 0 ; grep -q -E \"".$md5sum{$filename2}."\" $md5Filename; a=\$? ;  if [ \"\$a\" -eq \"1\" ]; then $cmd2a -sOutputFile='| lzop -f -o $rasterFilename.lzo' $cmd2c >>/dev/null 2>&1 ; fi";
    }

#   $cmd.=" ; gzip -f $inputFilename >>$logFilename 2>&1";
    $outputFilenames.="$inputFilename ";

  } else {
    $cmd .= " ; echo \"$product\" >>$logFilename " if (!$bmpcmp);

    $outputFilename="$temp/$tempname.$options";
    $baselineFilename="$baselineRaster/$tempname.$options";
    $rasterFilename="$raster/$tempname.$options";
    $bmpcmpFilename="$bmpcmpDir/$tempname.$options";

    if ($product eq 'gs') {
      $cmd2a.=" $gsBin";
    } elsif ($product eq 'pcl') {
      $cmd2a.=" $pclBin";
    } elsif ($product eq 'xps') {
      $cmd2a.=" $xpsBin";
    } elsif ($product eq 'svg') {
      $cmd2a.=" $svgBin";
    } elsif ($product eq 'ls') {
      $cmd2a.=" $lsBin";
    } elsif ($product eq 'mupdf') {
      $cmd2a.=" $mupdfBin";
    } else {
      die "unexpected product: $product";
    }

    if ($product eq 'mupdf') {
      $cmd2b.=" -r$a[1]";
      $cmd2b.=" -b20"    if ($a[2]==1);
      $cmd2b.=" -o $outputFilename.%04d";
      $cmd2b.=" $inputFilename  ";
      $cmd.=" ; echo \"$cmd2a $cmd2b $cmd2c\" >>$logFilename ";
      $cmd.=" ; $preCommand $cmd2a $cmd2b $cmd2c >>$logFilename 2>&1";
      $cmd.=" ; cat $outputFilename.???? >$outputFilename ";
      $cmd.=" ; md5sum $outputFilename >>$md5Filename";
    } else {
    if ($updateBaseline) {
      $cmd2b.=" -sOutputFile='|gzip -1 -n >$baselineFilename.gz'";
    } elsif ($md5sumOnly) {
      $cmd2b.=" -sOutputFile='|md5sum >>$md5Filename'";
    } else {
      if ($local) {
#       $cmd2b.=" -sOutputFile='|gzip -1 -n >$outputFilename.gz'";
        if ($rerunIfMd5sumDifferences) {
          $cmd2b.=" -sOutputFile='|md5sum >>$md5Filename'";
        } else {
          $cmd2b.=" -sOutputFile='|lzop -f -o $outputFilename.lzo'";
        }
      } else {
        $cmd2b.=" -sOutputFile='|gzip -1 -n >$outputFilename.gz'";
      }
#     $cmd2b.=" -sOutputFile='|gzip -1 -n | md5sum >>$md5Filename'";
    }

    $cmd2c.=" -dMaxBitmap=30000000" if ($a[2]==0);
    $cmd2c.=" -dMaxBitmap=10000"    if ($a[2]==1);

    $cmd2c.=" -sDEVICE=".$a[0];
    $cmd2c.=" -dGrayValues=256" if ($a[0] eq 'bitrgb');
    $cmd2c.=" -dcupsColorSpace=0" if ($a[0] eq 'cups');
    $cmd2c.=" ".$additionalOptions;
    $cmd2c.=" ".$additionalOptions_gs if ($product eq 'gs');
    $cmd2c.=" -r".$a[1];
#   $cmd2c.=" -q" if ($product eq 'gs');
    $cmd2c.=" -sDEFAULTPAPERSIZE=letter" if ($product eq 'gs');
    $cmd2c.=" -dNOPAUSE -dBATCH -K1000000";  # -Z:
#   $cmd2c.=" -dNOOUTERSAVE -dJOBSERVER -c false 0 startjob pop -f" if ($product eq 'gs');
    $cmd2c.=" -dJOBSERVER" if ($product eq 'gs');

    $cmd2c.=" %rom%Resource/Init/gs_cet.ps" if ($filename =~ m/.PS$/ && $product eq 'gs');
#   $cmd2c.=" -dFirstPage=1 -dLastPage=1" if ($filename =~ m/.pdf$/i || $filename =~ m/.ai$/i);

    $cmd2c.=" - < " if (!($filename =~ m/.pdf$/i || $filename =~ m/.ai$/i) && $product eq 'gs');

    $cmd2c.=" $inputFilename ";

    if ($bmpcmp) {
      $cmd.=" ; $preCommand $cmd2a -sOutputFile='|gzip -1 -n >$outputFilename.gz' $cmd2c >>$logFilename 2>&1";
      if ($bmpcmp_icc_work) {
        $cmd2a =~ s|/gs/|/icc_work/|;
      } else {
        $cmd2a =~ s|/gs/|/head/|;
      }
      $cmd.=" ; $preCommand $cmd2a -sOutputFile='|gzip -1 -n >$baselineFilename.gz' $cmd2c >>$logFilename 2>&1";
      $cmd.=" ; bash -c \"nice ./bmpcmp $bmpcmpOptions <(gunzip -c $outputFilename.gz) <(gunzip -c $baselineFilename.gz) $bmpcmpFilename 0 100\""; # ; gzip $bmpcmpFilename.* ";
      $cmd.=" ; ./checkSize.pl $bmpcmpFilename.*";
      $cmd.=" ; bash -c \"for (( c=1; c<=5; c++ )); do scp -q -o ConnectTimeout=30 -i ~/.ssh/cluster_key $bmpcmpFilename.* regression\@casper.ghostscript.com:/home/regression/cluster/bmpcmp/. ; t=\\\$?; if [ \\\$t == 0 ]; then break; fi; echo 'scp retry \$c' ; done \"";
#     $cmd.=" ; scp -q -o ConnectTimeout=30 -i ~/.ssh/cluster_key $logFilename regression\@casper.ghostscript.com:/home/regression/cluster/bmpcmp/.";
    } else {
      $cmd.=" ; echo \"$cmd2a $cmd2b $cmd2c\" >>$logFilename ";
      $cmd.=" ; $preCommand $cmd2a $cmd2b $cmd2c >>$logFilename 2>&1";
      if ($local) {
#       $cmd.=" ; zcat $outputFilename.gz | md5sum >$md5Filename";
        if ($rerunIfMd5sumDifferences) {
        } else {
          $cmd.=" ; sleep 0 ; lzop -c -d $outputFilename.lzo | md5sum >$md5Filename";
        }
#       $cmd.=" ; md5sum $outputFilename.gz >$md5Filename";
      }
   }

    if ($rerunIfMd5sumDifferences && exists $md5sum{$filename2} && !exists $skip{$filename2}) {
      $cmd.=" ; sleep 0 ; grep -q -E \"".$md5sum{$filename2}."\" $md5Filename; a=\$? ;  if [ \"\$a\" -eq \"1\" ]; then $cmd2a -sOutputFile='| lzop -f -c $rasterFilename.lzo' $cmd2c >>/dev/null 2>&1 ; fi";
    }


    }

  }
  if ($md5sumOnly) {
  } else {
#   $cmd.=" ; md5sum $outputFilename.gz >>$md5Filename 2>&1 ";
#   $outputFilenames.="$outputFilename";
  }

  # $cmd.=" ; gzip -f $outputFilename >>$logFilename 2>&1 ";

  return($cmd,$outputFilenames,$filename2);
}

# main

$temp=$baselineRaster if ($updateBaseline==1);
$rerunIfMd5sumDifferences=0 if ($updateBaseline==1);

my %quickFiles;
if (open (F,"<quickfiles.lst")) {
  while(<F>) {
    chomp;
    $quickFiles{$_}=1;
  }
  close(F);
}

my @commands;
my @outputFilenames;
my @filenames;
my @slowCommands;
my @slowOutputFilenames;
my @slowFilenames;

if ($bmpcmp) {
  open (F,"$filename") || die "file $filename not found";
  my $done=0;
  my $start=0;
  while(<F>) {
    chomp;
    my $cmd="";
    my $outputFilenames="";
    my $filename="";
    my @a=split ' ';
    $start=1 if (m/ran \d+ tests in \d+ seconds/);
    if ($start && scalar(@a)>=4) {  # horrible, horrible hack
    if (m/^(.+)\.(pdf\.p.mraw\.\d+\.[01]) (\S+) pdfwrite /) {
#       print "$1 $2 $3 -- pdfwrite\n";
      ($cmd,$outputFilenames,$filename)=build($3,$1,$2,!$local,1) if (!$done);
print "$filename\t$cmd\n" if (!$done);
    } elsif (m/^(.+)\.(p.mraw\.\d+\.[01]) (\S+)/) {
#      print "$1 $2 $3\n";
      ($cmd,$outputFilenames,$filename)=build($3,$1,$2,!$local,1) if (!$done);
print "$filename\t$cmd\n" if (!$done);
    } elsif (m/^(.+)\.(pam\.\d+\.[01]) (\S+)/) {
#      print "$1 $2 $3\n";
      ($cmd,$outputFilenames,$filename)=build($3,$1,$2,!$local,1) if (!$done);
print "$filename\t$cmd\n" if (!$done);
    } elsif (m/errors:$/ || m/previous clusterpush/i) {
      $done=1;
    }
    }
    
  }
  close(F);
} else {
foreach my $testfile (sort keys %testfiles) {
  foreach my $test (@{$tests{$testfiles{$testfile}}}) {
    if (($lowres==1  && ($test =~ m/\.72\./ || $test =~ m/\.75\./)) ||
        ($highres==1 && ($test =~ m/\.300\./ || $test =~ m/\.600\./)) ||
        ($lowres==0  && $highres==0)) {
    my $t=$testfile.".".$test;
    my $cmd="";
    my $outputFilenames="";
    my $filename="";
    ($cmd,$outputFilenames,$filename)=build($testfiles{$testfile},$testfile,$test,!$local,0);
    if (exists $quickFiles{$filename}) {
      push @commands,$cmd;
      push @outputFilenames,$outputFilenames;
      push @filenames,$filename;
    } else {
      push @slowCommands,$cmd;
      push @slowOutputFilenames,$outputFilenames;
      push @slowFilenames,$filename;
    }
    }
  }
}
}

if (-e "gccwarnings" && !$bmpcmp && !$local && (scalar keys %products==0 || exists $products{'gs'})) {
  print "gs_build\tcd __gsSource__ ; make clean >/dev/null 2>&1 ; make -j 1 >__temp__/gs_build.log 2>&1\n";
}

while (scalar(@slowCommands)) {
  my $n=rand(scalar @slowCommands);
  my $filename=$slowFilenames[$n];  splice(@slowFilenames,$n,1);
  my $command=$slowCommands[$n];  splice(@slowCommands,$n,1);
  print "$filename\t$command\n";
}

while (scalar(@commands)) {
  my $n=rand(scalar @commands);
  my $filename=$filenames[$n];  splice(@filenames,$n,1);
  my $command=$commands[$n];  splice(@commands,$n,1);
  print "$filename\t$command\n";
}

#while (scalar(@slowCommands)) {
#  my $n=rand(scalar @slowCommands);
#  my $filename=$slowFilenames[$n];  splice(@slowFilenames,$n,1);
#  my $command=$slowCommands[$n];  splice(@slowCommands,$n,1);
#  print "$filename\t$command\n";
#}

#while (scalar(@commands)) {
#  my $n=rand(scalar @commands);
#  my $filename=$filenames[$n];  splice(@filenames,$n,1);
#  my $command=$commands[$n];  splice(@commands,$n,1);
#  print "$filename\t$command\n";
#}

