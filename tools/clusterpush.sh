#!/bin/sh

# helper script to syncronize a working tree with the regression cluster

HOST=atfxsw01@tticluster.com
DEST=$USER

# try to use the same directory name on the cluster
TARGET=`basename $PWD`
if test -z "$TARGET"; then
  TARGET='ghostpcl'
fi

echo "Pushing to $DEST/$TARGET on the cluster..."
rsync -avz \
  --exclude .svn \
  --exclude bin --exclude obj --exclude debugobj \
  --exclude ufst --exclude ufst-obj \
  ./* $HOST:$DEST/$TARGET
