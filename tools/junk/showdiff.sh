#!/bin/sh -x
echo "showdiff.sh $1 $2"
composite -compose difference $1 $2 /tmp/diff.tmp
display /tmp/diff.tmp $1 $2
rm /tmp/diff.tmp

