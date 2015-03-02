#!/bin/bash

# Produce a trimmed HPGL/2 RTL plot.  Ignore the Plot Size command in
# the RTL/GL2 file and produce output whose extant corresponds to the
# bounding box of all the graphics in the plot.  This requires 2
# passes over the file.

for f in $*
do
    # get the bounding box in points.
    BBOX_PTS=( $(pcl6 -dNOPAUSE -lRTL -sDEVICE=bbox $f 2>&1 |\
       grep "%%BoundingBox" | awk '{print $2, $3, $4, $5}') )

    # convert to plotter units
    SF="(1016.0/72.0)"
    for coord in 0 1 2 3
    do
        BBOX_PLU[$coord]=$(echo "scale=2;${BBOX_PTS[$coord]} * $SF"|bc)
    done

    # now run pcl with our new coordinates.  Annoyingly those @PJL's
    # need to start in the first colummn, as no leading space is
    # allowed by the PJL parser.
    pcl6 -lRTL \
      -J"@PJL DEFAULT PLOTSIZEOVERRIDE=ON;\
@PJL DEFAULT PLOTSIZE1=${BBOX_PLU[2]};\
@PJL DEFAULT PLOTSIZE2=${BBOX_PLU[3]}"\
    -sDEVICE=pdfwrite -o "$(basename $f).pdf" $f
done
