#!/usr/bin/perl

use strict;
use warnings;

use Data::Dumper;

my $input=shift;
my @jobs;
my @svgJobs;

my @machine;
my @ratio;
my @count;

my $header;

while(my $t=shift) {
  push @machine,$t;
  push @ratio,shift;
}

die "usage: splitjobs.pl input [machine ratio ...]" if (!$input || scalar(@ratio)==0);

open(F,"<$input") || die "file $input not found";
$header=<F>;
chomp $header;
close(F);

for (my $i=0;  $i<scalar(@machine);  $i++) {
  open(F,">$machine[$i]") || die "can't write to file $machine[$i]";
  print F "$header\n";
  close(F);
}
exit;


#print Dumper(\@machine);
#print Dumper(\@ratio);

$header="";
open(F,"<$input") || die "file $input not found";
while(<F>) {
  chomp;
  if (m/^pdl=/) {
    $header=$_;
  } else {
    if (m/__svg__/ || m/__pcl__/ || m/__xl__/ || m/__xps__/) {
      push @svgJobs,$_;
    } else {
      push @jobs,$_;
    }
  }
}
close(F);

#print scalar(@jobs)."\n";



my $total=0;
foreach my $t (@ratio) {
  $total+=$t;
}

my $remainder=scalar(@jobs)+scalar(@svgJobs);
foreach my $t (@ratio) {
# print $t/$total."\n";
  my $c=int(($t/$total)*(scalar(@jobs)+scalar(@svgJobs))+0.5);
  push @count,$c;
  $remainder-=$c;
}
$count[-1]+=$remainder;

for (my $i=0;  $i<scalar(@machine);  $i++) {
  open(F,">$machine[$i]") || die "can't write to file $machine[$i]";
  print F "$header\n" if ($header);
  while($count[$i]--) {
    if (scalar(@svgJobs)>0) {
      print F (shift @svgJobs)."\n";
    } else {
      print F (shift @jobs)."\n";
    }
  }
  close(F);
}

