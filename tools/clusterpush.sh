#!/bin/sh

# helper script to syncronize a working tree with the regression cluster

HOST=atfxsw01@tticluster.com
DEST=$USER

# try to use the same directory name on the cluster
TARGET=`basename $PWD`
if test -z "$TARGET"; then
  TARGET='ghostpcl'
fi

# try get the current revision
REV=`svn info | grep Revision | cut -d ' ' -f 2`
if test -z "$REV"; then
  REV='unknown'
fi

echo "Pushing to $DEST/$TARGET on the cluster..."
rsync -avz \
  --exclude .svn \
  --exclude bin --exclude obj --exclude debugobj \
  --exclude main/obj --exclude main/debugobj \
  --exclude language_switch/obj --exclude language_switch/obj \
  --exclude xps/obj --exclude xps/debugobj \
  --exclude svg/obj --exclude xps/debugobj \
  --exclude ufst --exclude ufst-obj \
  ./* $HOST:$DEST/$TARGET

echo -n "Copying regression baseline"
LATEST=`ssh $HOST ls regression | \
 egrep 'ghostpcl-r[0-9]+\+[0-9]+' | sort -r | head -1`
echo " from $LATEST..."
ssh $HOST "cp regression/$LATEST/reg_baseline.txt $DEST/$TARGET/"

echo "Queuing regression test..."
echo "cd $DEST/$TARGET && run_regression" | ssh $HOST
