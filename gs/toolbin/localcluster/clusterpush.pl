#!/usr/bin/perl

use strict;
use warnings;

use Data::Dumper;

my $verbose=0;

my %products=('abort' =>1,
              'gs' =>1,
              'pcl'=>1,
              'svg'=>1,
              'xps'=>1);

my $res="";

my $product=shift;
if ($product && $product eq "lowres") {
  $product=shift;
  $res="lowres";
}
if ($product && $product eq "highres") {
  $product=shift;
  $res="highres";
}
my $user=shift;
if ($user && $user eq "lowres") {
  $user=shift;
  $res="lowres";
}
if ($user && $user eq "highres") {
  $user=shift;
  $res="highres";
}

unlink "cluster_command.run";

my $host="casper.ghostscript.com";
my $dir="/home/regression/cluster/users";
if (!$user) {
  $user=`echo \$USER`;
  chomp $user;
}

my $directory=`pwd`;
chomp $directory;

$directory =~ s|.+/||;
if ($directory ne 'gs' && $directory ne 'ghostpdl') {
  $directory="";
  if (-d "base" && -d "Resource") {
    $directory='gs';
  }
  if (-d "pxl" && -d "pcl") {
    $directory='ghostpdl';
  }
}

die "can't figure out if this is a ghostscript or ghostpdl directory" if ($directory eq "");

$product='gs pcl xps' if (!$product);
print "$user $directory $product\n" if ($verbose);


if ($directory eq 'gs') {
  $directory='ghostpdl/gs';
}


my @a=split ' ',$product;
foreach my $i (@a) {
  if (!exists $products{$i}) {
    print STDERR "illegal product: $i\n";
    exit;
  }
}


my $cmd="rsync -avxcz".
" --delete".
" --max-size=2500000".
" --exclude .svn --exclude .git".
" --exclude _darcs --exclude .bzr --exclude .hg".
" --exclude .deps --exclude .libs --exclude autom4te.cache".
" --exclude bin --exclude obj --exclude debugobj --exclude pgobj".
" --exclude sobin --exclude soobj".
" --exclude main/obj --exclude main/debugobj".
" --exclude language_switch/obj --exclude language_switch/obj".
" --exclude xps/obj --exclude xps/debugobj".
" --exclude svg/obj --exclude svg/debugobj".
" --exclude ufst --exclude ufst-obj".
" --exclude .ppm --exclude .pkm --exclude .pgm --exclude .pbm".
" -e \"ssh -l regression -i \$HOME/.ssh/cluster_key\"".
" .".
" regression\@$host:$dir/$user/$directory";

if ($product ne "abort") {
  print STDERR "syncing\n";
  print "$cmd\n" if ($verbose);
  #`$cmd`;
  open(T,"$cmd |");
  while(<T>) {
    print $_;
  }
  close(T);
}

open(F,">cluster_command.run");
print F "$user $product $res\n";
close(F);

if ($product ne "abort") {
  print STDERR "\nqueueing\n";
} else {
  print STDERR "\ndequeueing\n";
}
print "$cmd\n" if ($verbose);
`$cmd`;

unlink "cluster_command.run";

