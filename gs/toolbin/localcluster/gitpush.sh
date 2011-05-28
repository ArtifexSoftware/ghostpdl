#!/bin/bash
#
# gs/toolbin/localcluster/gitpush.sh
#
# This relies on being called in that directory (i.e. don't copy it elsewhere
# to use it).

SCRIPTDIR=`dirname $0`

if [ "$CLUSTER_USER" != "" ]
then
  myuser=$CLUSTER_USER
else
  myuser=$USER
fi

date > $SCRIPTDIR/../../../clusterdatestamp
echo $* > $SCRIPTDIR/../../../clustercmd
git add $SCRIPTDIR/../../../clusterdatestamp
git add $SCRIPTDIR/../../../clustercmd
git stash --keep-index -q
git push -f cluster stash:$myuser
git stash pop -q --index
git rm -q -f $SCRIPTDIR/../../../clusterdatestamp
git rm -q -f $SCRIPTDIR/../../../clustercmd
