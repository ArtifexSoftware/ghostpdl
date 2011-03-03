#!/usr/bin/perl

use strict;
use warnings;

use Data::Dumper;

my $verbose=0;

# bmpcmp usage: [gs] [pcl] [xps] [gs] [bmpcmp] [lowres] [$user] | abort



my %products=('abort' =>1,
              'bmpcmp' =>1,
              'gs' =>1,
              'pcl'=>1,
              'svg'=>1,
              'xps'=>1,
              'ls'=>1);

my $user;
my $product="";
my $command="";
my $res="";
my $t1;
while ($t1=shift) {
  if ($t1 eq "lowres") {
    $res="lowres";
  } elsif ($t1 eq "highres") {
    $res="highres";
  } elsif ($t1=~m/^-/ || $t1=~m/^\d/) {
    $command.=$t1.' ';
  } elsif (exists $products{$t1}) {
    $product.=$t1.' ';
  } elsif ($t1 =~ m/ /) {
    $product.=$t1.' ';
  } else {
    $user=$t1;
  }
}

$product="" if (!$product);
$user=""    if (!$user);


#print "product=$product res=$res user=$user command=$command\n";  exit;

unlink "cluster_command.run";

my $host="casper.ghostscript.com";
my $dir="/home/regression/cluster/users";
if (!$user) {
  $user=`echo \$USER`;
  chomp $user;
}

# This is horrid, but it works. Replace it when I find a better way
if ($user eq 'Robin Watts') {
  $user = "robin";
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

#$directory="gs" if ($directory eq "" && $product eq "bmpcmp");
$directory="gs" if ($directory eq "" && $product && $product eq "abort");

die "can't figure out if this is a ghostscript or ghostpdl directory" if ($directory eq "");

$product='gs pcl xps ls' if (!$product);
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
" -e \"ssh -l regression -i \\\"\$HOME/.ssh/cluster_key\\\"\"".
" .".
" regression\@$host:$dir/$user/$directory";

if ($product ne "abort" ) { #&& $product ne "bmpcmp") {
  print STDERR "syncing\n";
  print "$cmd\n" if ($verbose);
  #`$cmd`;
  open(T,"$cmd |");
  while(<T>) {
    chomp;
    print "$_\n" if (!m/\/$/);
  }
  close(T);
}

open(F,">cluster_command.run");
print F "$user $product $res\n";
print F "$command\n";
close(F);

$cmd="rsync -avxcz".
" -e \"ssh -l regression -i \\\"\$HOME/.ssh/cluster_key\\\"\"".
" cluster_command.run".
" regression\@$host:$dir/$user/$directory";

if ($product ne "abort") {
  print STDERR "\nqueueing\n";
} else {
  print STDERR "\ndequeueing\n";
}
print "$cmd\n" if ($verbose);
`$cmd`;

unlink "cluster_command.run";

