#!/usr/bin/perl

use strict;
use warnings;

use Data::Dumper;

my $previousValues=50;

my $verbose=1;

my %current;
my %archiveCache;

print STDERR "reading archive directory\n" if ($verbose);
my %archives;
if (opendir(DIR, 'archive')) { # || die "can't opendir archive: $!";
foreach (readdir(DIR)) {
  $archives{$_}=1 if (!(-d $_) && (m/.tab$/) && !(m/mupdf/));
}
closedir DIR;
}

sub myCmp($$) {
  my $a=shift;
  my $b=shift;
  $a=~m/(\d+)/;
  my $a1=$1;
  $b=~m/(\d+)/;
  my $b1=$1;
  return($b1 cmp $a1);
}

my $count=$previousValues;
foreach my $i (sort myCmp keys %archives) {
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
      $a[6]=$a[1] if ($a[1] ne "0");  # if no md5sum store the error code instead
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

#print Dumper(\%archiveCache);

print "$previousValues\n";
foreach my $i (sort keys %archiveCache) {
  print "$i | $archiveCache{$i}\n";
}


