#!/bin/sh -x

# roll back 1 - 365 days.
DAYS_PAST=$(count.py 1 365)

# make a starting release and name in pcl6.orig
cvs update -A
make debug
mkdir -p /tmp/main/obj/
cp main/obj/pcl6 /tmp/main/obj

DIR1=/tmp/
DIR2=/home/henrys/trees/history/ghostpcl/

for x in $DAYS_PAST
do
  # check for upate x days ago
  if cvs -n update -D"$x day ago" 2>&1 | grep -q ^U
  then
      # have and update so update the tree and recompile
      cvs update -D"$x day ago" > /dev/null 2>&1
      # try the make
      if make debug > /dev/null 2>&1
      then
          echo "$x days ago ok"
      else
          # build failed - sometimes need make clean
          echo "$x days ago bad make doing clean"
          make clean > /dev/null 2>&1
          if make debug > /dev/null 2>&1
          then
              echo "$x days ago good after clean"
          else
              # clean build failed - tree broken - clean then continue
              echo "$x days ago bad after clean"
              make clean > /dev/null 2>&1
          fi
      fi
      # at this point we have either have 2 executables for day n
      # and day n - 1 or day n failed to build
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
      cp main/obj/pcl6 /tmp/main/obj
  fi # if we have an update.
done
