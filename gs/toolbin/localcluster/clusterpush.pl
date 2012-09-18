#!/usr/bin/perl

use strict;
use warnings;

use Data::Dumper;

my $verbose=0;

# bmpcmp usage: [gs] [pcl] [xps] [gs] [mupdf] [bmpcmp] [lowres] [32] [$user] | abort



my %products=('abort' =>1,
              'bmpcmp' =>1,
              'bmpcmphead' =>1,
              'gs' =>1,
              'pcl'=>1,
              'svg'=>1,
              'xps'=>1,
              'ls'=>1,
              'mupdf'=>1);

my $user;
my $product="";
my $filters="";
my $command="";
my $res="";
my $w32="";
my $relaxTimeout="";
my $t1;
while ($t1=shift) {
  if ($t1 eq "lowres") {
    $res="lowres";
  } elsif ($t1 eq "highres") {
    $res="highres";
  } elsif ($t1 eq "32") {
    $w32="32";
  } elsif ($t1 eq "timeout" || $t1 eq "relaxtimeout") {
    $relaxTimeout="relaxTimeout";
  } elsif ($t1=~m/^-/ || $t1=~m/^\d/) {
    $command.=$t1.' ';
  } elsif ($t1 =~ m/filter=.*/) {
    $filters.=$t1.' ';
  } elsif (exists $products{$t1}) {
    $product.=$t1.' ';
  } elsif ($t1 =~ m/ /) {
    $product.=$t1.' ';
  } else {
    $user=$t1;
  }
}

# Strip the stray space off the end. This can probably be done far more
# efficiently with some unreadable bit of perl, but I am too simple for
# that.
while (substr($product,-1) eq " ") {
  $product = substr($product,0,-1);
}

$product="" if (!$product);
$user=""    if (!$user);


#print "product=$product res=$res user=$user command=$command\n";  exit;

unlink "cluster_command.run";

my $host="casper.ghostscript.com";
my $dir="/home/regression/cluster/users";

# To cater for those whose cluster user name doesn't match the user name
# on their development machine.
if (!$user) {
  $user=`echo \$CLUSTER_USER`;
  chomp $user;
}

if (!$user) {
  $user=`echo \$USER`;
  chomp $user;
}

# Msys Bash seems to set USERNAME, not USER
if (!$user) {
  $user=`echo \$USERNAME`;
  chomp $user;
}

my $directory=`pwd`;
chomp $directory;

$directory =~ s|.+/||;
if ($directory ne 'gs' && $directory ne 'ghostpdl' && $directory ne 'mupdf') {
  $directory="";
  if (-d "base" && -d "Resource") {
    $directory='gs';
  }
  if (-d "pxl" && -d "pcl") {
    $directory='ghostpdl';
  }
  if (-d "fitz" && -d "draw" && -d "pdf") {
    $directory='mupdf';
  }
}

#$directory="gs" if ($directory eq "" && $product eq "bmpcmp");
$directory="gs" if ($directory eq "" && $product && $product eq "abort");

die "can't figure out if this is a ghostpdl, gs, or mupdf source directory" if ($directory eq "");

if (!$product) {
  if ($directory eq 'mupdf') {
    $product='mupdf';
  } else {
    $product='gs pcl xps ls'
  }
}

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

# Detect if we are running under msys
my $bashversion=`bash --version`;
my $msys=0;
if ($bashversion =~ /pc-msys/) {
   $msys=1;
}
my $ssh="ssh -l regression -i \\\"\$HOME/.ssh/cluster_key\\\"";
my $hostpath="regression\@$host:$dir/$user/$directory";
if ($msys) {
  $ssh="cygnative plink";
  $hostpath="regression:$dir/$user/$directory";
}

my $cmd="rsync -avxcz ".
" --max-size=10000000".
" --delete".
" --exclude .svn --exclude .git".
" --exclude _darcs --exclude .bzr --exclude .hg".
" --exclude .deps --exclude .libs --exclude autom4te.cache".
" --exclude bin --exclude obj --exclude debugobj --exclude pgobj".
" --exclude bin64 --exclude obj64 --exclude debugobj64 --exclude pgobj64".
" --exclude membin --exclude memobj --exclude membin64 --exclude memobj64".
" --exclude profbin --exclude profobj --exclude profbin64 --exclude profobj64".
" --exclude sobin --exclude soobj --exclude debugbin".
" --exclude ufst --exclude ufst-obj --exclude ufst-debugobj".
" --exclude config.log --exclude .png".
" --exclude .ppm --exclude .pkm --exclude .pgm --exclude .pbm".
" --exclude build --exclude generated".
" -e \"$ssh\" ".
" .".
" $hostpath";

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
print F "$user $product $res $w32 $relaxTimeout\n";
print F "$command\n";
print F "$filters\n";
close(F);

$cmd="rsync -avxcz".
" -e \"$ssh\"".
" cluster_command.run".
" $hostpath";

if ($product ne "abort") {
  print STDERR "\nqueueing\n";
} else {
  print STDERR "\ndequeueing\n";
}
print "$cmd\n" if ($verbose);
`$cmd`;

unlink "cluster_command.run";

