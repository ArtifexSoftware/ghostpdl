#/bin/sh
# script to smoke check the renderer against known files

# define the parameters of the run
TESTS=$(find ../tests/ -type f)
TESTS=../tests/pcl/pcl5cfts/fts.*
EXE=./language_switch/obj/pspcl6
#EXE=./main/obj/pcl6
OPTS="-dNOPAUSE -sDEVICE=ppmraw -r100"
BASELINE=tools/smoke_baseline.txt

# check for baseline and test files
if ! test -r $BASELINE; then
  echo "Couldn't find baseline data file '$BASELINE'"
  exit 1
fi
if test -z "$TESTS"; then
 echo "no test files found"
 exit 1
fi

# loop over the test files comparing checksums
all=0
failed=0
for file in $TESTS; do
 echo -n "$file: "
 fsum=`md5sum $file | cut -f 1 -d ' '`
 result=`$EXE $OPTS -sOutputFile="|md5sum" $file`
 rsum=`echo $result | cut -f 1 -d ' '`
 bsumline=`cat $BASELINE | egrep ^$fsum`
 if test -z "$bsumline"; then
  echo "file doesn't exist in baseline"
 else
  all=`expr $all + 1`
  bsum=`echo $bsumline | cut -f 2 -d ' '`
  if test "x$rsum" = "x$bsum"; then
   echo "ok"
  else
   echo "DIFFERS"
#   echo $rsum vs $bsum"
   failed=`expr $failed + 1`
  fi
 fi
done

# report
if test $failed -gt 0; then
 echo "differences in $failed of $all files"
else
 echo "all known files match"
fi 
