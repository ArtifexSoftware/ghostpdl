#!/bin/sh -x

DIR1=/home/henrys/now
DIR2=/home/henrys/now2

for file in $*
do
    echo $file
    out=$(basename $file)
    (cd $DIR1/pcl/obj; pcl5 -sDEVICE=pbmraw -dNOPAUSE -sOutputFile=foo.$out.%d $file)
    (cd $DIR2/pcl/obj; pcl5 -sDEVICE=pbmraw -dNOPAUSE -sOutputFile=foo.$out.%d $file)
    (cd $DIR1/; 
	for x in pcl/obj/foo.*
	do
	    cmp -s $DIR1/$x $DIR2/$x || writediff.sh $DIR1/$x $DIR2/$x
	done
    )
    rm $DIR1/pcl/obj/foo.* $DIR2/pcl/obj/foo.*
done