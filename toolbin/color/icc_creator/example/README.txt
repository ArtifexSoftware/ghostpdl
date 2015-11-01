The tritone example purposely switches the orange and
green colorants.  Essentially the CIELAB values in tritone_cielab.txt
and the colorant tritone_names.txt are such that green and
orange should be swapped when rendering the file
tritone.pdf using -sDeviceNProfile=artifex_tritone.icc
as a command line option.  To make them correct you could
recreate the profile using ICC_Creator, swapping the order 
of the green and orange names in the file tritone_names.txt.