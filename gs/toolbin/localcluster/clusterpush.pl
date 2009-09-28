#!/usr/bin/perl

use strict;
use warnings;

use Data::Dumper;

my $verbose=1;

my $host="casper.ghostscript.com";
my $dir="/home/marcos/cluster/users";
my $user=`echo \$USER`;
chomp $user;

my $buildType=`pwd`;
chomp $buildType;

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
" -e \"ssh -l marcos -i \$HOME/.ssh/cluster_key\"".
" .".
" marcos\@$host:$dir/$user/$buildType";

print STDERR "syncing\n";
print "$cmd\n" if ($verbose);
`$cmd`;


`ssh -l marcos -i \$HOME/.ssh/cluster_key $host touch $dir/$user/$product.run`;

#  ./* $HOST:$DEST/$TARGET
#if test ! $? -eq 0; then
#  echo "$0 aborted."
#  exit 1
#fi


#HOST=atfxsw01@tticluster.com
#DEST=$USER
#
## try to use the same directory name on the cluster
#TARGET=`basename $PWD`
#
## determine which build we're doing
#if test -d gs; then
#  BUILD_TYPE='ghostpdl'
#else
#  BUILD_TYPE='gs'
#fi
## fall back to build type if we don't have a target
#if test -z "$TARGET"; then
#  TARGET="$BUILD_TYPE"
#fi
#
#
# try get the current revision
#REV=''
#if test -d .svn; then
#  REV=`svn info | grep Revision | cut -d ' ' -f 2`
#elif test -d .git; then
#  REV=`git rev-parse HEAD`
#fi
#if test -z "$REV"; then
#  REV='unknown'
#fi
#
#echo "Pushing to $DEST/$TARGET on the cluster..."
#
#echo -n "Copying regression baseline"
#if test -z "$BUILD_TYPE"; then echo " $0 aborted."; exit 1; fi
#echo " from $BUILD_TYPE..."
#ssh $HOST "cp regression/$BUILD_TYPE/reg_baseline.txt $DEST/$TARGET/"
#if test ! $? -eq 0; then
#  echo "$0 aborted."
#  exit 1
#fi
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
