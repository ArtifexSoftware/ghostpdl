#!/bin/sh
combine -compose difference $1 $2 /tmp/diff.tmp
display -negate /tmp/diff.tmp

