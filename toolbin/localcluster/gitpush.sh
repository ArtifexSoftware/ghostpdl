#!/bin/bash
#
# gs/toolbin/localcluster/gitpush.sh
#
# 1) Copy this script into your path: e.g.
#     cp gs/toolbin/localcluster/gitpush.sh /bin
#
# 2) Set an alias:
#     git config --global alias.cluster '!gitpush.sh'
#
# 3) Set up a remote cluster:
#     git remote add cluster regression@ghostscript.com:/home/regression/cluster/ghostbridge/ghostpdl
#
# 4) Ensure you can ssh into regression@ghostscript.com. How you achieve this
#    is your own lookout, but I personally do it by running Pageant in the
#    background, having GIT_SSH set to plink.exe, and having the regression
#    user set up to have my key as one of it's authorised ones.

# First, find the root of this git checkout
GITROOT=`git rev-parse --show-toplevel`
if [ $? -ne 0 ]
then
  echo "Not in a git checkout!"
  exit 1
fi

# Find who we are
if [ "$CLUSTER_USER" != "" ]
then
  myuser=$CLUSTER_USER
elif [ "USER" != "" ]
then
  myuser=$USER
else
  myuser=$USERNAME
fi

if [ "$myuser" = "" ]
then
  echo "Unable to determine username; please set CLUSTER_USER"
  exit 1
fi

# Check that we are setup correctly
GITTEST=`git remote show cluster`
if [ $? -ne 0 ]
then
  echo "To use this script, you must have setup an appropriate git remote, such as:"
  echo ""
  echo " git remote add cluster regression@ghostscript.com:/home/regression/cluster/gitbridge/ghostpdl"
  echo ""
  echo "You must also be setup so that you can ssh to regression@ghostscript.com."
fi

# Every push must be unique, so make a timestamp file
date > $GITROOT/clusterdatestamp

# Put the params into a file that the hook can read
echo -n $* > $GITROOT/clustercmd

# Make sure git can see those files
git add $GITROOT/clusterdatestamp
git add $GITROOT/clustercmd

# Stash everything.
git stash --keep-index -q
if [ $? -ne 0 ]
then
  echo "Git stash failed. Aborting to avoid us getting into a mess."
  exit 1
fi

# Push the changes to our branch on remote branch cluster
git push -f cluster stash:refs/heads/$myuser

# Restore the state before we stashed
git stash pop -q --index

# Remove our two files from gits vision (and the file system)
git rm -q -f $GITROOT/clusterdatestamp
git rm -q -f $GITROOT/clustercmd
