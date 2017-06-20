#/bin/sh 
# script to generate a rough baseline for smoketests

# define the parameters of the run
#TESTS=$(find ../tests/ -type f)
TESTS=../tests/pcl/pcl5cfts/fts.*
#EXE=./main/obj/gpcl6
EXE=./language_switch/obj/pspcl6
OPTS="-dNOPAUSE -sDEVICE=ppmraw -r100"
BASELINE=tools/smoke_baseline.txt

# get the current revisions
rev=`svn info | grep Revision | cut -f 2 -d ' '`
gsrev=`svn info gs | grep Revision | cut -f 2 -d ' '`

echo "updating smoke test baseline for r$rev+$gsrev..."

if test -z "$TESTS"; then
 echo "no test files found"
 exit 1
fi

# remove the old baseline
if test -w $BASELINE; then
  rm $BASELINE
fi

# loop over the test files
for file in $TESTS; do
 fsum=`md5sum $file | cut -f 1 -d ' '`
 result=`$EXE $OPTS -sOutputFile="|md5sum" $file`
 rsum=`echo $result | cut -f 1 -d ' '`
 if test -z "$rsum"; then
  echo "no output for $file"
 else
  echo "$fsum $rsum $EXE r$rev+$gsrev $file" >> $BASELINE
 fi
done
