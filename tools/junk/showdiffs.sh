#!/bin/sh -x

. $HOME/bin/releases.sh

for file in $*
do
    echo $file
    (cd $DIR1/main/obj; pcl6 -r100 -Z@ -sDEVICE=ppmraw -dNOPAUSE -sOutputFile=foo.%d $file)
    (cd $DIR2/main/obj; pcl6 -r100 -Z@ -sDEVICE=ppmraw -dNOPAUSE -sOutputFile=foo.%d $file)
    (cd $DIR1/; 
	for x in main/obj/foo.*
	do
	    cmp -s $DIR1/$x $DIR2/$x || showdiff.sh $DIR1/$x $DIR2/$x
	done
    )
    rm $DIR1/main/obj/foo.* $DIR2/main/obj/foo.*
done
