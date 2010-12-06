#!/usr/bin/perl

use strict;
use warnings;

my $limit=10000000;

my $total=0;
my $unlink=0;
my $f;
while ($f=shift) {
  my $s = -s $f;
  if ($s) {
    print "$f $s\n";
    $unlink=1 if ($total>$limit && $f =~ /.meta$/);
    if ($unlink) {
      unlink($f);
      if (open(F,">>checkSize.log")) {
        print F "total=$total unlink($f)\n";
        close(F);
      }
    }
    $total+=$s;
  }
}

