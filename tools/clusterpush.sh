#!/bin/sh

# helper script to syncronize a working tree with the regression cluster

HOST=atfxsw01@tticluster.com
DEST=$USER/ghostpcl

echo "Pushing to $DEST on the cluster..."
rsync -avz \
  --exclude .svn --exclude obj --exclude ufst --exclude ufst-obj \
  ./* $HOST:$DEST/
