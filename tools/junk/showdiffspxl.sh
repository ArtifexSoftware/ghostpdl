#!/bin/sh

DIR1=/home/henrys/now/
DIR2=/home/henrys/now2/

for file in $*
do
    echo $file
    (cd $DIR1/pxl/obj; pclxl -sDEVICE=pbmraw -dNOPAUSE -sOutputFile=foo.%d $file)
    (cd $DIR2/pxl/obj; pclxl -sDEVICE=pbmraw -dNOPAUSE -sOutputFile=foo.%d $file)
    (cd $DIR1/; 
	for x in pxl/obj/foo.*
	do
	    cmp -s $DIR1/$x $DIR2/$x || showdiff.sh $DIR1/$x $DIR2/$x
	done
    )
    rm $DIR1/pxl/obj/foo.* $DIR2/pxl/obj/foo.*
done
