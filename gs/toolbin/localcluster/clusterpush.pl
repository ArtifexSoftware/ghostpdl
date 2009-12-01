#!/usr/bin/perl

use strict;
use warnings;

use Data::Dumper;

my $verbose=0;

my %products=('gs' =>1,
              'pcl'=>1,
              'svg'=>1,
              'xps'=>1);

my $product=shift;
my $user=shift;

my $host="casper.ghostscript.com";
my $dir="/home/regression/cluster/users";
if (!$user) {
  $user=`echo \$USER`;
  chomp $user;
}

my $directory=`pwd`;
chomp $directory;

$directory =~ s|.+/||;
print "$user $directory $product\n" if ($verbose);

die "clusterpush.pl must be run from gs or ghostpdl directory" if ($directory ne 'gs' && $directory ne 'ghostpdl');

#           rsync -av -e "ssh -l ssh-user" rsync-user@host::module /dest

if ($directory eq 'gs') {
  $directory='ghostpdl/gs';
}


$product='gs pcl svg xps' if (!$product);
my @a=split ' ',$product;
foreach my $i (@a) {
  if (!exists $products{$i}) {
    print STDERR "illegal product: $i\n";
    exit;
  }
}

my $cmd="rsync -avxc".
" --delete".
" --exclude .svn --exclude .git".
" --exclude _darcs --exclude .bzr --exclude .hg".
" --exclude bin --exclude obj --exclude debugobj".
" --exclude sobin --exclude soobj".
" --exclude main/obj --exclude main/debugobj".
" --exclude language_switch/obj --exclude language_switch/obj".
" --exclude xps/obj --exclude xps/debugobj".
" --exclude svg/obj --exclude xps/debugobj".
" --exclude ufst --exclude ufst-obj".
" --exclude .ppm --exclude .pkm --exclude .pgm --exclude .pbm".
" -e \"ssh -l regression -i \$HOME/.ssh/cluster_key\"".
" .".
" regression\@$host:$dir/$user/$directory";

open(F,">cluster_command.run");
print F "$user $product\n";
close(F);

print STDERR "syncing and queuing\n";
print "$cmd\n" if ($verbose);
#`$cmd`;
open(T,"$cmd |");
while(<T>) {
  print $_;
}
close(T);

unlink "cluster_command.run";


#`ssh -l marcos -i \$HOME/.ssh/cluster_key $host touch $dir/$user/$product.run`;

#`scp -i ~/.ssh/cluster_key -q user.tmp marcos\@casper.ghostscript.com:/home/marcos/cluster/users/$user/user.run . >/dev/null 2>/dev/null`;

