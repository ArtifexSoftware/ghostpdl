#!/bin/sh

name=$1
D=`dirname "$name"`
B=`basename "$name"`
name="`cd \"$D\" 2>/dev/null && pwd || echo \"$D\"`/$B"

echo $name
