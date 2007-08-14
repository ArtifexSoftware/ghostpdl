#!/bin/sh

# helper script to syncronize a working tree with the regression cluster

rsync -avz --exclude .svn --exclude obj --exclude ufst --exclude ufst-obj ./* atfxsw01@tticluster.com:$USER/ghostpcl/
