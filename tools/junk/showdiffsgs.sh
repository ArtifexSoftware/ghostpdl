#!/bin/sh -x

. $HOME/bin/releasesgs.sh

for file in $*
do
  echo $file
  (cd $DIR1; ../debugobj/gs -sDEVICE=ppmraw -dNOPAUSE -sOutputFile=foo.%d  $file -c quit > /dev/null)
  (cd $DIR2; ../debugobj/gs -sDEVICE=ppmraw -dNOPAUSE -sOutputFile=foo.%d $file -c quit > /dev/null)
  for x in $DIR2/foo.*
  do
    echo $x
    BASE=$(basename $x)
    cmp -s $DIR1/$BASE $DIR2/$BASE || showdiff.sh $DIR1/$BASE $DIR2/$BASE
  done
  rm $DIR1/foo.* $DIR2/foo.*
done
