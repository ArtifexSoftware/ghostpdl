#!/bin/sh

DIR1=/home/henrys/now/
DIR2=/home/henrys/now2/

rm -f $DIR1/pcl/obj/foo.* $DIR2/pcl/obj/foo.* /tmp/diff.tmp

trap 'rm -f $DIR1/pcl/obj/foo.* $DIR2/pcl/obj/foo.* /tmp/diff.tmp; exit 1' 1 2 3 13 15

for file in $*
do
    echo $file
    (cd $DIR1/pcl/obj; pcl5 -sDEVICE=pbmraw -dNOPAUSE -sOutputFile=foo.%d $file)
    (cd $DIR2/pcl/obj; pcl5 -sDEVICE=pbmraw -dNOPAUSE -sOutputFile=foo.%d $file)
    if [ ! -f $DIR1/pcl/obj/foo.1 ]
    then
	continue
    fi
    (cd $DIR1/;
	for x in pcl/obj/foo.[0-9]*
	do
	
	    if cmp -s $DIR1/$x $DIR2/$x
	    then
		continue
	    fi
	    combine -compose difference $DIR1/$x $DIR2/$x /tmp/diff.tmp
	    display /tmp/diff.tmp $DIR1/$x $DIR2/$x
	    PAGE=$(echo $x | sed 's/.*\.//')
	    echo '
		  1:done
		  2: rasterize both go to next
		  3: rasterize both and rediplay\c'
	    read choice
	    echo
	    case "$choice"
	    in
		1) echo "next";;
		2) (cd $DIR1/pcl/obj; pcl5 -sDEVICE=ljet4 -dNOPAUSE -dFirstPage="$PAGE" -dLastPage="$PAGE" -sOutputFile="|lpr" $file)
        	       (cd $DIR2/pcl/obj; pcl5 -sDEVICE=ljet4 -dNOPAUSE -dFirstPage="$PAGE" -dLastPage="$PAGE" -sOutputFile="|lpr" $file);;

		3) (cd $DIR1/pcl/obj; pcl5 -sDEVICE=ljet4 -dNOPAUSE -dFirstPage="$PAGE" -dLastPage="$PAGE" -sOutputFile="|lpr" $file)
			(cd $DIR2/pcl/obj; pcl5 -sDEVICE=ljet4 -dNOPAUSE -dFirstPage="$PAGE" -dLastPage="$PAGE" -sOutputFile="|lpr" $file)
		       display /tmp/diff.tmp $DIR1/$x $DIR2/$x;;
		*) echo "bad option";;
		esac
	    done
		)
rm -f $DIR1/pcl/obj/foo.* $DIR2/pcl/obj/foo.* /tmp/diff.tmp
done
