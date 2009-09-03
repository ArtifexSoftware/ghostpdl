#!/usr/bin/perl

use strict;
use warnings;

use Data::Dumper;

my $current=shift;
my $previous=shift;
my $elapsedTime=shift;
my $machineCount=shift || die "usage: compare.pl current.tab previous.tab elapsedTime machineCount";
my %current;
my %currentError;
my %previous;
my %previousError;

my @filesRemoved;
my @filesAdded;
my @brokePrevious;
my @repairedPrevious;
my @differencePrevious;

my $t2;

open(F,"<$current") || die "file $current not found";
while(<F>) {
  chomp;
  s|__|/|g;
  my @a=split '\t';
  $current{$a[0]}=$a[6];
  $currentError{$a[0]}=$a[1];
}


open(F,"<$previous") || die "file $previous not found";
while(<F>) {
  chomp;
  s|__|/|g;
  my @a=split '\t';
  $previous{$a[0]}=$a[6];
  $previousError{$a[0]}=$a[1];
}
close(F);

foreach my $t (sort keys %previous) {
  if (exists $current{$t}) {
    if ($currentError{$t} && !$previousError{$t}) {
      push @brokePrevious,$t;
    } else {
      if (!$currentError{$t} && $previousError{$t}) {
        push @repairedPrevious,$t;
      } else {
        if ($current{$t} eq $previous{$t}) {
#         print "$t match $previous and $current\n";
        } else {
            push @differencePrevious,$t;
        }
      }
    }
  } else {
    push @filesRemoved,$t;
  }
}

foreach my $t (sort keys %current) {
  if (!exists $previous{$t}) {
    push @filesAdded,$t;
  }
}


print "ran ".scalar(keys %current)." tests in $elapsedTime seconds on $machineCount nodes\n\n";

{
  print "Differences in ".scalar(@differencePrevious)." of ".scalar(keys %current)." test(s):\n";
  while(my $t=shift @differencePrevious) {
    print "$t\n";
  }
  print "\n";
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
