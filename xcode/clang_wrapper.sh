#!/bin/sh

# This script is supposed to simply parrot out all it's arguments with
# any paths expanded to be absolute. Unfortunately, it fails due to
# invocations such as:
#
# clang_wrapper.sh BLAH="unsigned int" fred.c george.c

# This is a (very) poor mans copy of scan-build.

args="$@"

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

echo $newargs
clang --analyze $newargs
gcc $args

