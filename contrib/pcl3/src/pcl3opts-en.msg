$ *****************************************************************************
$ File:     @(#)$Id: pcl3opts-en.msg,v 1.9 2001/05/31 15:23:28 Martin Rel $
$ Contents: NLS message catalogue for pcl3opts in English
$ Author:   Martin Lottermoser, Greifswaldstrasse 28, 38124 Braunschweig,
$           Germany
$ 
$ *****************************************************************************
$									      *
$	Copyright (C) 1999, 2000, 2001 by Martin Lottermoser		      *
$	All rights reserved						      *
$									      *
$ *****************************************************************************

$set 2
$quote "
1	"Usage: %s [<options>] [<input files>].\n  <option> ::= -o <output file>\n"
2	"The file `%s' could not be opened for writing:\n  %s.\n"
3	"Could not open the input file `%s':\n  %s.\n"
4	"Error closing `%s':\n  %s.\n"
9	"Memory allocation failure: %s.\n"
10	"\nThe previous raster data section is followed by one with a different\nconfiguration.\n"
11	"I've found a raster data section which uses a combination of resolutions\nand intensity levels which pcl3 cannot reproduce:\n"
12	"    colorant %d: %4d x %4d ppi %2d levels\n"
13	"My best guess for the command line needed to generate a similar file is:\n"
14	"This file uses a PCL page size code (%d) which is unknown to pcl3.\n"
15	"The page size is set to %s"
16	"in"
17	"The media destination (output tray) selected is %d.\n"
18	"To be able to select a media position (input or output) you will have to\nconfigure the `InputAttributes' or `OutputAttributes' dictionaries\nappropriately. See the manual page gs-pcl3(1).\n"
19	"The media source (input tray) selected is %d.\n"
20	"Illegal field length in Configure Raster Data command.\n"
30	"This file uses at least one compression method for raster data (%d)\nwhich pcl3 does not know.\n"
31	"This file contains a Configure Raster Data command with an\n  illegal number of colorants (%d) and the following data:\n"
32	"This file contains a Configure Raster Data command with an\n  unknown format (%d).\n"
33	"  It *might* mean the following:\n"
34	"  This format is not supported by pcl3.\n"
$ Message 40 should not be used.
41	"This file uses a palette (%d) not supported by pcl3.\n"
50	"This does not seem to be a PCL file containing raster data.\n"
60	"I've found an unknown value for the command to select\nsimplex/duplex printing: %d.\n"
70	"Although the colour model in this file is declared as CMYK, the file does not\nuse the black colorant. If you find that the printer does not accept a file\ngenerated with the command line above, try the CMY colour model instead.\n\n"
$ Message number 80 is in use.
