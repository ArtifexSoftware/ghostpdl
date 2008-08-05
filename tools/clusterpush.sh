#!/bin/sh

# helper script to syncronize a working tree with the regression cluster

HOST=atfxsw01@tticluster.com
DEST=$USER

# try to use the same directory name on the cluster
TARGET=`basename $PWD`
if test -z "$TARGET"; then
  if test -d gs; then
    TARGET='ghostpcl'
  else
    TARGET='gs'
  fi
fi

# try get the current revision
REV=`svn info | grep Revision | cut -d ' ' -f 2`
if test -z "$REV"; then
  REV='unknown'
fi

echo "Pushing to $DEST/$TARGET on the cluster..."
rsync -avz \
  --exclude .svn --exclude .git \
  --exclude bin --exclude obj --exclude debugobj \
  --exclude sobin --exclude soobj \
  --exclude main/obj --exclude main/debugobj \
  --exclude language_switch/obj --exclude language_switch/obj \
  --exclude xps/obj --exclude xps/debugobj \
  --exclude svg/obj --exclude xps/debugobj \
  --exclude ufst --exclude ufst-obj \
  ./* $HOST:$DEST/$TARGET
if test $? != 0; then
  echo "$0 aborted."
  exit 1
fi

echo -n "Copying regression baseline"
if test -d src; then
  LATEST='gs'
else
  LATEST='ghostpdl'
fi
if test -z "$LATEST"; then echo " $0 aborted."; exit 1; fi
echo " from $LATEST..."
ssh $HOST "cp regression/$LATEST/reg_baseline.txt $DEST/$TARGET/"
if test $? != 0; then
  echo "$0 aborted."
  exit 1
fi

echo "Queuing regression test..."
echo "cd $DEST/$TARGET && run_regression" | ssh $HOST
if test $? != 0; then
  echo "$0 aborted."
  exit 1
fi

REPORT=`ssh $HOST ls $DEST/$TARGET \| egrep '^regression-[0-9]+.log$' \| sort -r \| head -1`
echo "Pulling $REPORT..."
scp -q $HOST:$DEST/$TARGET/$REPORT .
if test $? != 0; then
  echo "$0 aborted."
  exit 1
fi
cat $REPORT
if test $? != 0; then
  echo "$0 aborted."
  exit 1
fi
