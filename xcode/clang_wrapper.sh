#!/bin/sh

# This is a (very) poor mans copy of scan-build.

args=$*

# Run through the list of args converting any .c files into absolute paths
newargs=""
for name in $args
do
   if [ -f $name ]
   then
       # Convert filename to absolute path
       D=`dirname "$name"`
       B=`basename "$name"`
       name="`cd \"$D\" 2>/dev/null && pwd || echo \"$D\"`/$B"
   fi

   if [ "$name" = "-c" ]
   then
       name=""
   fi

   newargs="$newargs $name"
done

clang --analyze $newargs
gcc $args

