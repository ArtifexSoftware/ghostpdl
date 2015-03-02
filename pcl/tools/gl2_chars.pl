#!/usr/bin/env perl -w

# these can be changed.
$chars_per_line=4;
$lines_per_page=5;

$font_size_plu=(2 * 1016);

# height and width of paper in gl/2 plotter units minus enough for
# fonts.
$plu_height=(1016 * 11) - $font_size_plu;
$plu_width=(1016 * 8.5) - $font_size_plu;

# use this if control character should print associated character and
# not perform their associated funtion.
$transparent_data_command="TD1;";

# uncomment first scale command a comment the second for stick font.
# Comment first scale command and uncomment the second for arc fonts.
# stick font pitch 3 characters per inch

# the 2,0 selection fixed space 3,.5 select 1/2 char per inch.
#$scale_command="SD2,0,3,.5";
#$scale_command="SD2,0,3,.5";
# the 2,1 select proportion 4 selects font height
$scale_command="SD2,1,4,144;";
# $scale_command="SD4,144,2,1,7,4101;";

# stroke weight - use the 9999 weight value to take on the current pen
# width
$stroke_weight="SD6,9999;";
#$stroke_weight="";
# the rest should not need to be changed.
$line_height=$plu_height/$lines_per_page;
$col_width=$plu_width/$chars_per_line;

# draw a plus sign - used at the origin of each character
$horizontal_tick="PA;PD;PW1;PR-4,0,8,0,-4,0;PW0;PA;";
$vertical_tick="PA;PD;PW1;PR0,-4,0,8,0,-4;PW0;PA;";

$header="\033E\033%1BINSP1;PW0;$scale_command$transparent_data_command$stroke_weight";
$trailer="\033%1A\033E";

for( $sym=32; $sym < 256; $sym++ ) {
    # calculate position
    $line = int( ($sym-32) / $chars_per_line );
    $ypos = $plu_height - (( $line % $lines_per_page ) * $line_height);
    $xpos = (($sym-32) % $chars_per_line) * $col_width;

    if ( (($sym-32) % ($lines_per_page * $chars_per_line)) == 0 ) {
	# don't need a trailer on the first page.
	if ( ($sym-32) != 0 ) {
	    print $trailer;
	}
	print $header;
    }
    # print the tick marks and character
    print "PU$xpos,$ypos;$horizontal_tick,$vertical_tick";
    printf "LB%c\003", $sym;
}

# done - no harm if printed twice
print $trailer
