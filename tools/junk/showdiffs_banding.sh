#!/bin/sh -x

#DIR=/home/henrys/ghostpcl_1.38p1/

DIR=/home/henrys/trees/fontid/

for file in $*
do
    echo $file
    (cd $DIR/main/obj; pcl6 -r100 -Z@ -sDEVICE=pbmraw -dNOPAUSE -sOutputFile=foo.%d $file)
    (cd $DIR/main/obj; pcl6 -dMaxBitmap=100000 -dBufferSpace=100000 -r100 -Z@ -sDEVICE=pbmraw -dNOPAUSE -sOutputFile=bandfoo.%d $file)
    OUTFILESDIR=$DIR/main/obj/
    (cd $OUTFILESDIR/; 
	for x in foo.*
	do
	    cmp -s $x band$x || showdiff.sh $x band$x
	done
    )
    rm $DIR/main/obj/foo.* $DIR/main/obj/bandfoo.*
done
