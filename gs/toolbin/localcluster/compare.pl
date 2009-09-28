#!/usr/bin/perl

use strict;
use warnings;

use Data::Dumper;

my $previousValues=20;

my @errorDescription=("none","Error_reading_input_file","Error_reading_Ghostscript_produced_PDF_file","Timeout_reading_input_file","Timeout_reading_Ghostscript_produced_PDF_File");

my $current=shift;
my $previous=shift;
my $elapsedTime=shift;
my $machineCount=shift || die "usage: compare.pl current.tab previous.tab elapsedTime machineCount";
my $skipMissing=shift;

my %current;
my %currentError;
my %currentProduct;
my %currentMachine;
my %previous;
my %previousError;
my %previousProduct;
my %previousMachine;
my %archive;
my %archiveProduct;
my %archiveMachine;
my %archiveCount;

my @filesRemoved;
my @filesAdded;
my @allErrors;
my @brokePrevious;
my @repairedPrevious;
my @differencePrevious;
my @archiveMatch;

my $t2;

open(F,"<$current") || die "file $current not found";
while(<F>) {
  chomp;
  s|__|/|g;
  my @a=split '\t';
  $current{$a[0]}=$a[6];
  $currentError{$a[0]}=0;
  if ($a[1]!=0) {
    $currentError{$a[0]}='unknown';
    $currentError{$a[0]}=$errorDescription[$a[1]] if (exists $errorDescription[$a[1]]);
  }
  $currentProduct{$a[0]}=$a[8];
  $currentMachine{$a[0]}=$a[9];
}


open(F,"<$previous") || die "file $previous not found";
while(<F>) {
  chomp;
  s|__|/|g;
  my @a=split '\t';
  $previous{$a[0]}=$a[6];
  $previousError{$a[0]}=0;
  if ($a[1]!=0) {
    $previousError{$a[0]}='unknown';
    $previousError{$a[0]}=$errorDescription[$a[1]] if (exists $errorDescription[$a[1]]);
  }
  $previousProduct{$a[0]}=$a[8];
  $previousMachine{$a[0]}=$a[9];
}
close(F);

# build list of archived files
my %archives;
opendir(DIR, 'archive') || die "can't opendir archive: $!";
foreach (readdir(DIR)) {
  $archives{$_}=1 if (!(-d $_) && (m/.tab$/));
}
closedir DIR;

my $count=$previousValues;
foreach my $i (sort {$b cmp $a} keys %archives) {
#print STDERR "$i\n";
  if ($count>0) {
    open(F,"<archive/$i") || die "file archive/$i not found";
    while(<F>) {
      chomp;
      s|__|/|g;
      my @a=split '\t';
      $i=~m/(.+)\.tab/;
      my $r=$1;
      $archive{$r}{$a[0]}=$a[6];
      $archiveProduct{$r}{$a[0]}=$a[8];
      $archiveMachine{$r}{$a[0]}=$a[9];
      $archiveMachine{$r}{$a[0]}="unknown" if (!$archiveMachine{$r}{$a[0]});
      $archiveCount{$r}=$previousValues-$count+1;
    }
    close(F);
    $count--;
  }
}

#print Dumper(\%archive);


foreach my $t (sort keys %previous) {
  if (exists $current{$t}) {
    if ($currentError{$t}) {
      push @allErrors,"$t $previousProduct{$t} $previousMachine{$t} $currentMachine{$t} $currentError{$t}";
    }
    if ($currentError{$t} && !$previousError{$t}) {
      push @brokePrevious,"$t $previousProduct{$t} $previousMachine{$t} $currentMachine{$t} $currentError{$t}";
    } else {
      if (!$currentError{$t} && $previousError{$t}) {
        push @repairedPrevious,"$t $previousProduct{$t} $previousMachine{$t} $currentMachine{$t} $previousError{$t}";
      } else {
        if ($current{$t} eq $previous{$t}) {
#         print "$t match $previous and $current\n";
        } else {
	  my $match=0;
	  foreach my $p (sort {$b cmp $a} keys %archive) {
	    if (!$match && exists $archive{$p}{$t} && $archive{$p}{$t} eq $current{$t}) {
	      $match=1;
	      push @archiveMatch,"$t $archiveProduct{$p}{$t} $archiveMachine{$p}{$t} $currentMachine{$t} $p $archiveCount{$p}";
	    }
	  }
	  if (!$match) {
            push @differencePrevious,"$t $previousProduct{$t} $previousMachine{$t} $currentMachine{$t}";
	  }
        }
      }
    }
  } else {
    push @filesRemoved,"$t $previousProduct{$t}";
  }
}

#print Dumper(\@archiveMatch);

foreach my $t (sort keys %current) {
  if (!exists $previous{$t}) {
    push @filesAdded,"$t $currentProduct{$t}";
  }
}


print "ran ".scalar(keys %current)." tests in $elapsedTime seconds on $machineCount nodes\n\n";

if (@differencePrevious) {
  print "Differences in ".scalar(@differencePrevious)." of ".scalar(keys %current)." test(s):\n";
  while(my $t=shift @differencePrevious) {
    print "$t\n";
  }
  print "\n";
} else {
  print "No differences in ".scalar(keys %current)." tests\n\n";
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
  }
  print "\n";
}

if (!$skipMissing) {

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
  }
  print "\n";
}

if (@allErrors) {
  print "The following ".scalar(@allErrors)." regression file(s) are producing errors:\n";
  while(my $t=shift @allErrors) {
    print "$t\n";
  }
  print "\n";
}

if (@archiveMatch) {
  print "The following ".scalar(@archiveMatch)." regression file(s) had md5sum differences but matched at least once in the previous $previousValues runs:\n";
  while(my $t=shift @archiveMatch) {
    print "$t\n";
  }
  print "\n";
}
}

