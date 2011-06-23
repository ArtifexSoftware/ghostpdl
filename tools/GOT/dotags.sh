#!/bin/sh

# files produced:
#    bit.tmp - bitrgb file with pixel tags for object types
#    bit_detagged.tmp.ppm - the file stripped of tags
#    bit.tmp.ppm - detagged bit file converted to ppmraw output
#    tag.tmp.ppm - just the tags color coded see tagimage.c for mapping.
# 
# 
FILES=$*

for x in $FILES
do
    ../../main/obj/pcl6 -dNOPAUSE -sDEVICE=bitrgbtags -sOutputFile=bit.tmp $x
    make detag
    ./detag bit.tmp bit_detagged.tmp.ppm
    make tagimage
    ./tagimage bit.tmp tag.tmp.ppm
    display bit_detagged.tmp.ppm tag.tmp.ppm
done

