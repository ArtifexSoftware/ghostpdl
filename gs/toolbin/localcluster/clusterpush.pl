#!/usr/bin/perl

use strict;
use warnings;

use Data::Dumper;

my $verbose=0;

my $buildType=shift;
my $user=shift;

my $host="casper.ghostscript.com";
my $dir="/home/marcos/cluster/users";
if (!$user) {
  $user=`echo \$USER`;
  chomp $user;
}

if (!$buildType) {
  $buildType=`pwd`;
  chomp $buildType;
}

$buildType =~ s|.+/||;
print "$user $buildType\n" if ($verbose);

die "clusterpush.pl must be run from gs or ghostpdl directory" if ($buildType ne 'gs' && $buildType ne 'ghostpdl');

#           rsync -av -e "ssh -l ssh-user" rsync-user@host::module /dest

my $product="";
if ($buildType eq 'gs') {
  $buildType="ghostpdl/".$buildType;
  $product='gs';
} else {
  $product='ghostpdl';
}

my $cmd="rsync -avx".
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
" -e \"ssh -l marcos -i \$HOME/.ssh/cluster_key\"".
" .".
" marcos\@$host:$dir/$user/$buildType";

print STDERR "syncing\n";
print "$cmd\n" if ($verbose);
#`$cmd`;
open(T,"$cmd |");
while(<T>) {
  print $_;
}
close(T);


print STDERR "queuing\n";
`ssh -l marcos -i \$HOME/.ssh/cluster_key $host touch $dir/$user/$product.run`;




#
#echo "Queuing regression test..."
#echo "cd $DEST/$TARGET && run_regression" | ssh $HOST
#if test ! $? -eq 0; then
#  echo "$0 aborted."
#  exit 1
#fi
#
#REPORT=`ssh $HOST ls $DEST/$TARGET \| egrep '^regression-[0-9]+.log$' \| sort -r \| head -1`
#echo "Pulling $REPORT..."
#scp -q $HOST:$DEST/$TARGET/$REPORT .
#if test ! $? -eq 0; then
#  echo "$0 aborted."
#  exit 1
#fi
#cat $REPORT
#if test ! $? -eq 0; then
#  echo "$0 aborted."
#  exit 1
#fi
