#!/usr/bin/perl

use strict;
use warnings;

use Data::Dumper;

my $verbose=1;
my $reportAllErrors=0;  # possibly changed below
my $previousValues=50;

my @errorDescription=(
"none",
"Error_reading_input_file",
"Error_reading_Ghostscript_produced_PDF_file",
"Timeout_reading_input_file",
"Timeout_reading_Ghostscript_produced_PDF_File",
"Input_file_missing",
"Ghostscript_generated_PDF_file_missing",
"Seg_Fault_during_pdfwrite",
"Seg_Fault",
"Seg_Fault_reading_Ghostscript_produced_PDF_File",
"Internal_error");

my $current=shift;
my $previous=shift;
my $elapsedTime=shift;
my $machineCount=shift || die "usage: compare.pl current.tab previous.tab elapsedTime machineCount [skipMissing [products]]";
my $skipMissing=shift;
my $products=shift;

$products="gs pcl xps svg" if (!$products);

my %skip;
if (open(F,"<skip.lst")) {
  while(<F>) {
    chomp;
    s|__|/|g;
    my @a=split '\s';
    $skip{$_}=$a[0];
  }
  close(F);
}

$reportAllErrors=1 if ($current=~m/icc_work/);


my %current;
my %currentError;
my %currentProduct;
my %currentMachine;
my %currentTime1;
my %currentTime2;
my %previous;
my %previousError;
my %previousProduct;
my %previousMachine;
my %previousTime1;
my %previousTime2;
my %archive;
my %archiveProduct;
my %archiveMachine;
my %archiveCount;

my %archiveCache;

my @filesRemoved;
my @filesAdded;
my @allErrors;
my @brokePrevious;
my @repairedPrevious;
my @differencePrevious;
my @differencePreviousPdfwrite;
my @archiveMatch;

my @baselineUpdateNeeded;

my $t2;

print STDERR "reading $current\n" if ($verbose);
open(F,"<$current") || die "file $current not found";
while(<F>) {
  chomp;
  s|__|/|g;
  my @a=split '\t';
  next if (exists $skip{$a[0]} && $a[1]==0);  # skip only if no error
# $a[6]=$a[1] if ($a[1] ne "0");
  $current{$a[0]}=$a[6];
  $currentError{$a[0]}=0;
  if ($a[1]!=0) {
    $currentError{$a[0]}='unknown';
    $currentError{$a[0]}=$errorDescription[$a[1]] if (exists $errorDescription[$a[1]]);
    $current{$a[0]}=0;
  }
  $currentProduct{$a[0]}=$a[8];
  $currentMachine{$a[0]}=$a[9];
  $currentTime1{$a[0]}=$a[2];
  $currentTime2{$a[0]}=$a[3];
}

print STDERR "reading $previous\n" if ($verbose);
open(F,"<$previous") || die "file $previous not found";
while(<F>) {
  chomp;
  s|__|/|g;
  my @a=split '\t';
  next if (exists $skip{$a[0]} && $a[1]==0);  # skip only if no error
# $a[6]=$a[1] if ($a[1] ne "0");
  $previous{$a[0]}=$a[6];
  $previousError{$a[0]}=0;
  if ($a[1]!=0) {
    $previousError{$a[0]}='unknown';
    $previousError{$a[0]}=$errorDescription[$a[1]] if (exists $errorDescription[$a[1]]);
    $previous{$a[0]}=0;
  }
  $previousProduct{$a[0]}=$a[8];
  $previousMachine{$a[0]}=$a[9];
  $previousTime1{$a[0]}=$a[2];
  $previousTime2{$a[0]}=$a[3];
}
close(F);

if ($elapsedTime==0) {
} else {
if (open(F,"<md5sum.cache")) {
print STDERR "reading md5sum.cache\n" if ($verbose);
  while(<F>) {
    chomp;
    if (m/(.+) \| (.+)/) {
      $archiveCache{$1}=$2;
    } elsif (m/^(\d+)$/) {
      $previousValues=$1;
    }
  }
  close(F);
} else {

# build list of archived files
print STDERR "reading archive directory\n" if ($verbose);
my %archives;
if (opendir(DIR, 'archive')) { # || die "can't opendir archive: $!";
foreach (readdir(DIR)) {
  $archives{$_}=1 if (!(-d $_) && (m/.tab$/));
}
closedir DIR;
}

my $count=$previousValues;
my %current;
foreach my $i (sort {$b cmp $a} keys %archives) {
# print STDERR "$i\n";
  if ($count>0) {
    print STDERR "reading archive/$i\n" if ($verbose);
    open(F,"<archive/$i") || die "file archive/$i not found";
    while(<F>) {
      chomp;
      s|__|/|g;
      my @a=split '\t';
      $i=~m/(.+)\.tab/;
      my $r=$1;
#     $archive{$r}{$a[0]}=$a[6];
#     $archiveProduct{$r}{$a[0]}=$a[8];
#     $archiveMachine{$r}{$a[0]}=$a[9];
#     $archiveMachine{$r}{$a[0]}="unknown" if (!$archiveMachine{$r}{$a[0]});
#     $archiveCount{$r}=$previousValues-$count+1;
      $a[6]=$a[1] if ($a[1] ne "0");
      my $key=$a[0].' '.$a[6];
      if ($count==$previousValues) {
        $current{$key}=1;
      } else {
        if (!exists $current{$key} && !exists $archiveCache{$key}) {
          $archiveCache{$key}=$r."\t".$a[8]."\t".$a[9]."\t".($previousValues-$count+1);
        }
      }

    }
    close(F);
    $count--;
  }
}

}
#print Dumper(\%archiveCache);
}

#print "previous\n".Dumper(\%previous);
#print "current \n".Dumper(\%current);

my $first=0;

if (!($previous=~m/users/)) {
foreach my $t (sort keys %current) {
  if ($currentError{$t} =~ m/Seg_Fault/) {
    print "\n*****************************************************************************\nSeg faults with current rev:\n\n" if (!$first);
    $first=1;
    print "$t $currentMachine{$t} $currentError{$t}\n";
  }
}
print "\n*****************************************************************************\n\n\n" if ($first);
}


foreach my $t (sort keys %previous) {
  if (exists $current{$t}) {
    my $match=0;
    if ($currentError{$t}) {
      push @allErrors,"$t $previousProduct{$t} $previousMachine{$t} $currentMachine{$t} $currentError{$t}";
    }
    if ($currentError{$t} && !$previousError{$t}) {
      if (exists $archiveCache{$t.' '.$current{$t}}) {
            my @a=split "\t", $archiveCache{$t.' '.$current{$t}};
            my $message="";
            $message=$currentError{$t} if ($currentError{$t});
            push @archiveMatch,"$t $a[1] $a[2] $currentMachine{$t} $a[0] $a[3] $message";
            $match=1;
      } else {
        push @brokePrevious,"$t $previousProduct{$t} $previousMachine{$t} $currentMachine{$t} $currentError{$t}";
      }
    } else {
      if (!$currentError{$t} && $previousError{$t}) {
        if (exists $archiveCache{$t.' '.$current{$t}}) {
            my @a=split "\t", $archiveCache{$t.' '.$current{$t}};
            my $message="";
            $message=$currentError{$t} if ($currentError{$t});
            push @archiveMatch,"$t $a[1] $a[2] $currentMachine{$t} $a[0] $a[3] $message";
            $match=1;
        } else {
          push @repairedPrevious,"$t $previousProduct{$t} $previousMachine{$t} $currentMachine{$t} $previousError{$t}";
        }
      }
    }
##    } else {
        if ($current{$t} eq $previous{$t}) {
          #         print "$t match $previous and $current\n";
        } else {
#         foreach my $p (sort {$b cmp $a} keys %archive) {
#           if (!$match && exists $archive{$p}{$t} && $archive{$p}{$t} eq $current{$t}) {
#             $match=1;
#             push @archiveMatch,"$t $archiveProduct{$p}{$t} $archiveMachine{$p}{$t} $currentMachine{$t} $p $archiveCount{$p}";
#           }
#         }
          if (exists $archiveCache{$t.' '.$current{$t}}) {
            my @a=split "\t", $archiveCache{$t.' '.$current{$t}};
            my $message="";
            $message=$currentError{$t} if ($currentError{$t});
            # die "happened" if ($currentError{$t});
            push @archiveMatch,"$t $a[1] $a[2] $currentMachine{$t} $a[0] $a[3] $message";
            $match=1;
          }
          if (!$match) {
	    if ($currentProduct{$t} =~ m/pdfwrite/) {
              push @differencePreviousPdfwrite,"$t $previousProduct{$t} $previousMachine{$t} $currentMachine{$t}";
	    } else {
              push @differencePrevious,"$t $previousProduct{$t} $previousMachine{$t} $currentMachine{$t}";
	    }
          }
        }
        if ($currentMachine{$t} eq $previousMachine{$t}) {
          my $timeDelta1=0;
          my $timeDelta2=0;
          if ($previousTime1{$t}>0) {
            $timeDelta1=($currentTime1{$t}-$previousTime1{$t})/$previousTime1{$t};
          }
          if ($previousTime2{$t}>0) {
            $timeDelta2=($currentTime2{$t}-$previousTime2{$t})/$previousTime2{$t};
          }
#printf  "%10f %10f %10f %s\n",$timeDelta1,$currentTime1{$t},$previousTime1{$t},$t;
        }
##    }
##  }
  } else {
    push @filesRemoved,"$t $previousProduct{$t}";
  }
}

#print Dumper(\@archiveMatch);

my $pdfwriteTestCount=0;
my $notPdfwriteTestCount=0;

foreach my $t (sort keys %current) {
  if (!exists $previous{$t}) {
    push @filesAdded,"$t $currentProduct{$t}";
    if ($currentError{$t}) {
      push @allErrors,"$t $currentMachine{$t} $currentError{$t}";
      push @brokePrevious,"$t $currentMachine{$t} $currentError{$t}";
    }
  }
  my $p=$currentProduct{$t};
  $p =~ s/ pdfwrite//;
  if ($products =~ m/$p/) {
    if ($currentProduct{$t} =~ m/pdfwrite/) {
      $pdfwriteTestCount++;
    } else {
      $notPdfwriteTestCount++;
    }
  }
}

if ($elapsedTime==0 || $elapsedTime==1) {
} else {
  print "ran ".($pdfwriteTestCount+$notPdfwriteTestCount)." tests in $elapsedTime seconds on $machineCount nodes\n\n";
}

if (@differencePrevious) {
  print "Differences in ".scalar(@differencePrevious)." of $notPdfwriteTestCount non-pdfwrite test(s):\n";
  while(my $t=shift @differencePrevious) {
    print "$t\n";
    push @baselineUpdateNeeded,$t;
  }
  print "\n";
} else {
  print "No differences in $notPdfwriteTestCount non-pdfwrite tests\n\n";
}

if (@differencePreviousPdfwrite) {
  print "Differences in ".scalar(@differencePreviousPdfwrite)." of $pdfwriteTestCount pdfwrite test(s):\n";
  while(my $t=shift @differencePreviousPdfwrite) {
    print "$t\n";
    push @baselineUpdateNeeded,$t;
  }
  print "\n";
} else {
  print "No differences in $pdfwriteTestCount pdfwrite tests\n\n";
}

if (@brokePrevious) {
  print "The following ".scalar(@brokePrevious)." regression file(s) have started producing errors:\n";
  while(my $t=shift @brokePrevious) {
    print "$t\n";
  }
  print "\n";
}

if (@repairedPrevious) {
  print "The following ".scalar(@repairedPrevious)." regression file(s) have stopped producing errors:\n";
  while(my $t=shift @repairedPrevious) {
    print "$t\n";
    push @baselineUpdateNeeded,$t;
  }
  print "\n";
}

if (!$skipMissing || $skipMissing eq "false" || $skipMissing eq "0") {

  if (@filesRemoved) {
    print "The following ".scalar(@filesRemoved)." regression file(s) have been removed:\n";
    while(my $t=shift @filesRemoved) {
      print "$t\n";
    }
    print "\n";
  }

  if (@filesAdded) {
    print "The following ".scalar(@filesAdded)." regression file(s) have been added:\n";
    while(my $t=shift @filesAdded) {
      print "$t\n";
      push @baselineUpdateNeeded,$t;
    }
    print "\n";
  }

if ($reportAllErrors) {
  if (@allErrors) {
    print "The following ".scalar(@allErrors)." regression file(s) are producing errors:\n";
    while(my $t=shift @allErrors) {
      print "$t\n";
    }
    print "\n";
  }
}

my $first=1;
foreach my $t (sort keys %current) {
  if ($t =~ m/(.+\.)1$/) {
    $t2=$1.'0';
    if (exists $current{$t2}) {
      if ($current{$t} ne $current{$t2} && (!exists $previous{$t} || !exists $previous{$t2} || $previous{$t} eq $previous{$t2})) {
        if ($first) {
          print "\nThe following files are showing a new mismatch between banded and page mode:\n";
          $first=0;
        }
        print "$t\n";
      }
    }
  }
}
print "\n" if (!$first);


  if (@archiveMatch) {
    print "-------------------------------------------------------------------------------------------------------\n\n";
    print "The following ".scalar(@archiveMatch)." regression file(s) had differences but matched at least once in the previous $previousValues runs:\n";
    while(my $t=shift @archiveMatch) {
      print "$t\n";
    }
    print "\n";
  }

# open(F,">>baselineupdateneeded.lst");
# while(my $t=shift @baselineUpdateNeeded) {
#   my @a=split ' ',$t;
#   $a[0] =~ s/\//__/g;
#   print F "$a[0]\n";
# }
# close(F);
}

