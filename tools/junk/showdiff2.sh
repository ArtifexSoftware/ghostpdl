#!/bin/sh
echo "showdiff.sh $1 $2"
combine -compose difference $1 $2 /tmp/diff.tmp
display /tmp/diff.tmp $1 $2
