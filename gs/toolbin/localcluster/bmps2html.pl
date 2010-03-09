#!/usr/bin/perl

# #include <disclaimer.h>
# If you speak perl, and are offended by the code herein, I apologise.
# Please feel free to tidy it up.

# Syntax: bmps2html <inputdir> <outputdir>

########################################################################
# SETUP SECTION

# The path to the executables.
$convertexe= "convert"; # ImageMagick

# Set the following to the maximum number (approx) of bitmap sets that
# should be put into a single HTML file.
$maxsets = 100;

# Set the following to true to convert bmps to pngs (useful to save space
# if you want to show people this across the web).
$pngize = 1;

# Set the following to true if you want to use parallel dispatch of jobs
$parallel = 0;

# If set, use iframes rather than copying the content of the frames into
# the top html file.
$iframes = 0;

# END SETUP SECTION
########################################################################

########################################################################
# EXTERNAL USES
use Errno qw(EAGAIN);

########################################################################

########################################################################
# FUNCTIONS

sub getfilename {
    my ($num) = @_;
    my $filename = "compare";
    
    if ($num != 0)
    {
        $filename .= $num;
    }
    $filename .= ".html";
    return $filename;
}

sub openiframe {
    $framedir = sprintf("%05d",$framenum);
    $outdir   = $baseoutdir."/".$framedir;

    if ($iframes)
    {
        print $html "<IFRAME src=\"".$framedir."/frame.html\" WIDTH=\"100%\" FRAMEBORDER=\"0\" id=\"iframe".$framedir."\" scrolling=\"off\"></IFRAME><BR>";
    }

    mkdir $outdir;
    open($iframe, ">", $outdir."/frame.html");
        
    print $iframe "<HTML><HEAD><TITLE>Bitmap Comparison</TITLE>";
    print $iframe "$javascript</HEAD><BODY onLoad=\"parent.document.getElementById('iframe".$framedir."').style.height=document.getElementById('content').offsetHeight;parent.document.getElementById('iframe".$framedir."').style.width=document.getElementById('content').offsetWidth;\">";
    print $iframe "<DIV id=\"content\">";
}

sub closeiframe {
    if ($iframes)
    {
        print $iframe "</DIV></BODY>";
        close $iframe;
    }
    $framenum++;
}

sub openhtml {
    $setsthisfile = 0;
    open($html, ">", $baseoutdir."/".getfilename($filenum));

    $javascript  = "<SCRIPT LANGUAGE=\"JavaScript\"><!--\n";
    $javascript .= "function swap(n){";
    $javascript .=   "var n0 = '00000'+(3*Math.floor(n/3));"
    $javascript .=   "n0=n0.substring(n0.length-5,n0.length);"
    $javascript .=   "var n1 = '00000'+(3*Math.floor(n/3)+1);"
    $javascript .=   "n1=n1.substring(n1.length-5,n1.length);"
    $javascript .=   "var x = document.images['compare'+n0].src;";
    $javascript .=   "document.images['compare'+n0].src=document.images['compare'+n1].src;";
    $javascript .=   "document.images['compare'+n1].src = x;";
    $javascript .= "}";
    $javascript .= "var undef;";
    $javascript .= "function findPosX(obj){";
    $javascript .=   "var curLeft = 0;";
    $javascript .=   "if (obj.offsetParent){";
    $javascript .=     "while(1) {";
    $javascript .=       "curLeft += obj.offsetLeft;";
    $javascript .=       "if (!obj.offsetParent)";
    $javascript .=         "break;";
    $javascript .=       "obj = obj.offsetParent;";
    $javascript .=     "}";
    $javascript .=   "} else if (obj.x)";
    $javascript .=     "curLeft += obj.x;";
    $javascript .=   "return curLeft;";
    $javascript .= "}";
    $javascript .= "function findPosY(obj){";
    $javascript .=   "var curTop = 0;";
    $javascript .=   "if (obj.offsetParent){";
    $javascript .=     "while(1) {";
    $javascript .=       "curTop += obj.offsetTop;";
    $javascript .=       "if (!obj.offsetParent)";
    $javascript .=         "break;";
    $javascript .=       "obj = obj.offsetParent;";
    $javascript .=     "}";
    $javascript .=   "} else if (obj.x)";
    $javascript .=     "curTop += obj.x;";
    $javascript .=   "return curTop;";
    $javascript .= "}";
    $javascript .= "function coord(event,obj,n,x,y){";
    $javascript .=   "if (event.offsetX == undef) {";
    $javascript .=     "x += event.pageX-findPosX(obj)-1;";
    $javascript .=     "y += event.pageY-findPosY(obj)-1;";
    $javascript .=   "} else {";
    $javascript .=     "x += event.offsetX;";
    $javascript .=     "y += event.offsetY;";
    $javascript .=   "}";
    $javascript .=   "document['Coord'+n].X.value = x;";
    $javascript .=   "document['Coord'+n].Y.value = y;";
    $javascript .= "}\n--></SCRIPT>";
    print $html "<HTML><HEAD><TITLE>Bitmap Comparison</TITLE>";
    print $html "$javascript</HEAD><BODY>\n";
    
    if ($filenum > 0) {
        print $html "<P>";
        if ($num > 1)
        {
            print $html "<A href=\"".getfilename(0)."\">First</A>&nbsp;&nbsp;";
        }
        print $html "<A href=\"".getfilename($filenum-1)."\">Previous(".($filenum-1).")</A>";
        print $html "</P>\n";
    }
    $filenum++;
}

sub closehtml {
    print $html "</BODY>";
    close $html;
}

sub nexthtml {
    print $html "<P><A href=\"".getfilename($filenum)."\">Next(".$filenum.")</A>";
    closehtml();
    openhtml();
}

sub dprint {
    my ($f1, $f2, $str) = @_;
    
    if (!$iframes) {
        print $f1 $str;
    }
    print $f2 $str;
}

sub runjobs {
    my ($cmd, $cmd2, $html, $html2, $pre1, $pre2, $post) = @_;
    my $ret, $ret2, $pid;
    
    if ($parallel) {
        FORK: {
            if ($pid = fork) {
                $ret  = system($cmd);
                waitpid($pid, 0);
                $ret2 = $?;
            } elsif (defined $pid) {
                exec($cmd2);
            } elsif ($! == EAGAIN) {
                sleep 5;
                redo FORK;
            } else {
                die "Can't fork!: $!\n";
            }
        }
    } else {
        $ret  = system($cmd);
        $ret2 = system($cmd2);
    }
        
    if ($ret != 0)
    {
        print $pre1." ".$post." failed with exit code ".$ret."\n";
        print "Command was: ".$cmd."\n";
        dprint($html,$html2,"<P>".$pre1." ".$post." failed with exit code ");
        dprint($html,$html2,$ret."<br>Command was: ".$cmd."</P>\n");
        next;
    }
    if ($ret2 != 0)
    {
        print $pre2." ".$post." failed with exit code ".$ret2."\n";
        print "Command was: ".$cmd2."\n";
        dprint($html,$html2,"<P>Ref bitmap generation failed with exit code ");
        dprint($html,$html2,$ret2."<br>Command was: ".$cmd2."</P>\n");
        next;
    }
    
    return (($ret | $ret2) != 0);
}

sub runjobs3 {
    my ($cmd, $cmd2, $cmd3, $html, $html2) = @_;
    my $ret, $ret2, $ret3, $pid;

#print STDERR "runjobs3: $cmd $cmd2 $cmd3\n";
    
    if ($parallel) {
        FORK: {
            if ($pid = fork) {
                $ret  = system($cmd);
                waitpid($pid, 0);
                $ret2 = $?;
            } elsif (defined $pid) {
                FORK2 : {
                    if ($pid = fork) {
                        $ret2 = system($cmd2);
                        waitpid($pid, 0);
                        $ret3 = $?;
                        if ($ret2 = 0) {
                            $ret2 = $ret3;
                        }
                        exit($ret2);
                    } elsif (defined $pid) {
                        exec($cmd3);
                    } elsif ($! == EAGAIN) {
                        sleep 5;
                        redo FORK2;
                    } else {
                        die "Can't fork!: $!\n";
                    }
                }
            } elsif ($! == EAGAIN) {
                sleep 5;
                redo FORK;
            } else {
                die "Can't fork!: $!\n";
            }
        }
    } else {
        $ret  = system($cmd);
        $ret2 = system($cmd2);
        $ret3 = system($cmd3);
    }
        
    if ($ret != 0)
    {
        print "Bitmap conversion failed with exit code ".$ret."\n";
        print "Command was: ".$cmd."\n";
        dprint($html,$html2,"<P>Bitmap conversion failed with exit code ");
        dprint($html,$html2,$ret."<br>Command was: ".$cmd."</P>\n");
        next;
    }
    if ($ret2 != 0)
    {
        print "Bitmap conversion failed with exit code ".$ret2."\n";
        print "Command was: ".$cmd2." or ".$cmd3."\n";
        dprint($html,$html2,"<P>Bitmap conversion failed with exit code ");
        dprint($html,$html2,$ret2."<br>Command was: ".$cmd2." or ".$cmd3."</P>\n");
        next;
    }
}

# END FUNCTIONS
########################################################################

########################################################################
# Here follows todays lesson. Abandon hope all who enter here. Etc. Etc.
$indir    = $ARGV[0];
shift @ARGV;
$baseoutdir = $ARGV[0];
shift @ARGV;
$filenum  = 0;
$framenum = 0;

# Create the output dir/html file
mkdir $baseoutdir;
openhtml();

# Open the index
open(INDEX, "ls $indir/*.meta.gz| sed s/\.\[0-9]\*\.meta.gz// | sort -u |");

# Now run through the list of files
while (<INDEX>)
{
    chomp;

    # Keep everything after the last
    ($path,$_) = $_ =~ m/(.*)\/([^\/]+)/;

    # Put the filename into infile
    $infile = $_;
    ($res,$band) = $_ =~ m/.*\.(\d+)\.(\d+)/;
    $file = $infile;
    $file =~ s/__/\//g;
    
    # Start a new file if required
    if ($setsthisfile >= $maxsets)
    {
        nexthtml();
    }
    
    # Open the iframe
    openiframe();

    # Output the title
    dprint($html,$iframe,"<H1>".$framenum.": ".$file." (".$res."dpi)</H1></BR>\n");
    print "Processing: $framenum: $file ($res)";

    # Add the files to the HTML, converting to PNG if required.
    my $imageCount = 1;
    $images=sprintf "%05d",$imageCount;
    my $image0=sprintf "%05d",$imageCount+0;
    my $image1=sprintf "%05d",$imageCount+1;
    my $image2=sprintf "%05d",$imageCount+2;
#print STDERR "$indir/$infile.$images.bmp.gz\n";
    while (stat("$indir/$infile.$images.bmp.gz"))
    {
	print ".";
        if ($pngize)
        {
            $cmd   = "zcat ".$indir."/".$infile.".".$images.".bmp.gz| ";
            $cmd  .= $convertexe." - ";
            $cmd  .= $outdir."/out.".$images.".png";
            $cmd2  = "zcat ".$indir."/".$infile.".".($image1).".bmp.gz| ";
            $cmd2 .= $convertexe." - ";
            $cmd2 .= $outdir."/out.".($image1).".png";
            $cmd3  = "zcat ".$indir."/".$infile.".".($image2).".bmp.gz| ";
            $cmd3 .= $convertexe." - ";
            $cmd3 .= $outdir."/out.".($image2).".png";
            runjobs3($cmd, $cmd2, $cmd3, $html, $iframe, "convert");
            #unlink $outdir."/out.".$images.".bmp";
            #unlink $outdir."/out.".($image1).".bmp";
            #unlink $outdir."/out.".($image2).".bmp";
            $suffix = ".png";
        }
        else
        {
            # Uncompress
            $cmd   = "zcat $indir/$infile.$images.bmp.gz > ";
            $cmd  .= "$outdir/out.$images.bmp";
            $cmd2  = "zcat $indir/$infile.".($image1).".bmp.gz> ";
            $cmd2 .= "$outdir/out.".($image1).".bmp";
            $cmd3  = "zcat $indir/$infile.".($image2).".bmp.gz> ";
            $cmd3 .= "$outdir/out.".($image2).".bmp";
            runjobs3($cmd, $cmd2, $cmd3, $html, $iframe, "convert");
            $suffix = ".bmp";
        }
            
        $metafile = "$indir/$infile.$images.meta.gz";
        $meta{"X"}    = 0;
        $meta{"Y"}    = 0;
        $meta{"PW"}   = 0;
        $meta{"PH"}   = 0;
        $meta{"W"}    = 0;
        $meta{"H"}    = 0;
        $meta{"PAGE"} = 0;
        if (stat($metafile))
        {
            open(METADATA, "zcat $metafile |");
            while (<METADATA>) {
                chomp;
                s/#.*//;
                s/^\s+//;
                s/\s+$//;
                next unless length;
                my ($var,$value) = split(/\s*=\s*/, $_, 2);
                $meta{$var}=$value;
            }
            close METADATA;
        }
            
        $page = $meta{"PAGE"};
        $mousemove = "onmousemove=\"coord(event,this,'$images',".$meta{"X"}.",".$meta{"Y"}.")\"";
            
        print $iframe "<TABLE><TR><TD><IMG SRC=\"out.$images$suffix\" onMouseOver=\"swap($images)\" onMouseOut=\"swap(".($image1).")\" NAME=\"compare$images\" BORDER=1 TITLE=\"Candidate<->Reference: $file page=$page res=$res\" $mousemove></TD>\n";
        print $iframe "<TD><IMG SRC=\"out.".($image1)."$suffix\" NAME=\"compare".($image1)."\" BORDER=1 TITLE=\"Reference: $file page=$page res=$res\" $mousemove.></TD>\n";
        print $iframe "<TD><IMG SRC=\"out.".($image2)."$suffix\" BORDER=1 TITLE=\"Diff: $file page=$page res=$res\" $mousemove></TD></TR>\n";
        print $iframe "<TR><TD COLSPAN=3><FORM name=\"Coord$images\"><LABEL for=\"X\">Page=$page PageSize=".$meta{"PW"}."x".$meta{"PH"}." Res=$res TopLeft=(".$meta{"X"}.",".$meta{"Y"}.") W=".$meta{"W"}." H=".$meta{"H"}." </LABEL><INPUT type=\"text\" name=\"X\" value=0 size=3>X<INPUT type=\"text\" name=\"Y\" value=0 size=3>Y</FORM></TD></TR></TABLE><BR>\n";

        if (!$iframes) {
            print $html "<TABLE><TR><TD><IMG SRC=\"$framedir/out.$images$suffix\" onMouseOver=\"swap($images)\" onMouseOut=\"swap(".($image1).")\" NAME=\"compare$images\" BORDER=1 TITLE=\"Candidate<->Reference: $file page=$page res=$res\" $mousemove></TD>\n";
            print $html "<TD><IMG SRC=\"$framedir/out.".($image1)."$suffix\" NAME=\"compare".($image1)."\" BORDER=1 TITLE=\"Reference: $file page=$page res=$res\" $mousemove></TD>\n";
            print $html "<TD><IMG SRC=\"$framedir/out.".($image2)."$suffix\" BORDER=1 TITLE=\"Diff: $file page=$page res=$res\" $mousemove></TD></TR>\n";
            print $html "<TR><TD COLSPAN=3><FORM name=\"Coord$images\"><LABEL for=\"X\">Page=$page PageSize=".$meta{"PW"}."x".$meta{"PH"}." Res=$res TopLeft=(".$meta{"X"}.",".$meta{"Y"}.") W=".$meta{"W"}." H=".$meta{"H"}." </LABEL><INPUT type=\"text\" name=\"X\" value=0 size=3>X<INPUT type=\"text\" name=\"Y\" value=0 size=3>Y</FORM></TD></TR></TABLE><BR>\n";
	}
        $imageCount += 3;
        $images=sprintf "%05d",$imageCount;
        $image0=sprintf "%05d",$imageCount+0;
        $image1=sprintf "%05d",$imageCount+1;
        $image2=sprintf "%05d",$imageCount+1;
        $diffs++;
        $setsthisfile++;
    }
    print "\n";
    
    closeiframe();
}

close INDEX;

# List the errored files. If no stdout files this prints and error, but seems
# to continue.
open(INDEX, "ls $indir/*.stdout.gz | sort -u |");

# Now run through the list of files
print $html "<H1>Files that produced errors</H1></BR><DL>\n";
while (<INDEX>)
{
    chomp;

    # Keep everything between the last / and .stdout.gz
    ($path,$_) = $_ =~ m/(.*)\/([^\/]+).stdout.gz/;

    # Put the filename into infile
    $infile = $_;
    ($res,$band) = $_ =~ m/.*\.(\d+)\.(\d+)/;
    $file = $infile;
    $file =~ s/__/\//g;

    $framedir = sprintf("%05d",$framenum);
    $outdir   = $baseoutdir."/".$framedir;
    mkdir $outdir;

    # Uncompress stdout/stderr - FIXME: Maybe should rewrite to html?
    $cmd   = "zcat $indir/$infile.stdout.gz > $outdir/stdout.txt";
    $cmd2  = "zcat $indir/$infile.stderr.gz > $outdir/stderr.txt";
    runjobs($cmd, $cmd2, $html, $undef, "stdout", "stderr", "decompression");

    # Output HTML fragment
    print $html "<DT>$framenum: $file ($res dpi)</DT>\n";
    print $html "<DD><A href=\"$framedir/stdout.txt\">stdout</A>\n";
    print $html "<A href=\"$framedir/stderr.txt\">stderr</A></DT>\n";
    $framenum++;
}
print $html "</DL>";

close INDEX;

# Finish off the HTML file
closehtml();
