#!/usr/bin/perl

# #include <disclaimer.h>
# If you speak perl, and are offended by the code herein, I apologise.
# Please feel free to tidy it up.

# Syntax: htmldiff.pl <outputdir> <input list>

# Setup steps:
# 1) Place this script in your ghostpdl directory.
# 2) Edit the paths/options here as appropriate.
# 3) Save the list of changed files from the local cluster regression email
# here (e.g. as list.txt).
# 4) Invoke this script. (e.g. diffrun diffout list.txt)
# 5) Make tea. Drink tea.

########################################################################
# SETUP SECTION

# The path to the executables.
#$gsexe     = "gs/bin/gswin32c.exe";
$gsexe     = "gs\\bin\\gswin32c.exe";
$pclexe    = "main\\obj\\pcl6.exe";
$xpsexe    = "xps\\obj\\gxps.exe";
$svgexe    = "svg\\obj\\gsvg.exe";
$bmpcmpexe = "..\\bmpcmp\\bmpcmp\\Release\\bmpcmp.exe";
$convertexe= "convert.exe"; # ImageMagick

# The args fed to the different exes. Probably won't need to play with these.
$gsargs    = "-sDEVICE=bmp16m -dNOPAUSE -dBATCH -q -sDEFAULTPAPERSIZE=letter -dNOOUTERSAVE -dJOBSERVER -c false 0 startjob pop -f";
$gsargsPS  = " %rom%Resource/Init/gs_cet.ps";
$pclargs   = "-sDEVICE=bmp16m -dNOPAUSE";
$xpsargs   = "-sDEVICE=bmp16m -dNOPAUSE";
$svgargs   = "-sDEVICE=bmp16m -dNOPAUSE";
$pwgsargs  = "-sDEVICE=pdfwrite -dNOPAUSE -dBATCH -q -sDEFAULTPAPERSIZE=letter -dNOOUTERSAVE -dJOBSERVER -c false 0 startjob pop -f";
$pwpclargs = "-sDEVICE=pdfwrite -dNOPAUSE";
$pwxpsargs = "-sDEVICE=pdfwrite -dNOPAUSE";
$pwsvgargs = "-sDEVICE=pdfwrite -dNOPAUSE";

# Set the following to the maximum number (approx) of bitmap sets that
# should be put into a single HTML file.
$maxsets = 100;

# Set the following to 1 to convert bmps to pngs (useful to save space
# if you want to show people this across the web).
$pngize = 1;

# The path from your ghostpdl directory to where the test files can be
# found
$fileadjust = "../ghostpcl/";

# The path to prepend to each of the above exe's to get the reference
# version.
$reference = "..\\ghostpdlREF\\";

# I thought we'd need to use redirection, but this gives problems on
# windows at least. Non windows users may want to use the following:
# $redir = " - < ";
$redir = " ";

# Set the following to 1 if you want to use parallel dispatch of jobs
$parallel = 1;

# If set, use iframes rather than copying the content of the frames into
# the top html file.
$iframes = 0;

# Finally, allow a user to override any of these by having their own
# config file.
#
# The config file is expected to live in ~/.htmldiff/htmldiff.cfg
# on a unix based system, and just as htmldiff.cfg in a users home
# directory on windows.
#
# The config file should contain perl commands to override any of the
# above variables. Most probably just $fileadjust and $reference will
# need to be set.
do $ENV{'HOME'}."/.htmldiff/htmldiff.cfg";
do $ENV{'HOMEPATH'}.$ENV{'HOMEDRIVE'}."htmldiff.cfg";

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
    $outdir   = $basedir."/".$framedir;

    if ($iframes)
    {
        print $html "<IFRAME src=\"".$framedir."/frame.html\" WIDTH=\"100%\" FRAMEBORDER=\"0\" id=\"iframe".$framedir."\" scrolling=\"off\"></IFRAME><BR>";
    }

    mkdir $outdir;
    open($iframe, ">", $outdir."/frame.html");
        
    print $iframe "<HTML><HEAD><TITLE>Bitmap Comparison</TITLE>";
    print $iframe "<SCRIPT LANGUAGE=\"JavaScript\">";
    print $iframe "function swap(n){";
    print $iframe   "var x = document.images['compare'+3*Math.floor(n/3)].src;";
    print $iframe   "document.images['compare'+3*Math.floor(n/3)].src=document.images['compare'+(3*Math.floor(n/3)+1)].src;";
    print $iframe   "document.images['compare'+(3*Math.floor(n/3)+1)].src = x;";
    print $iframe "}";
    print $iframe "var undef;";
    print $iframe "function findPosX(obj){";
    print $iframe   "var curLeft = 0;";
    print $iframe   "if (obj.offsetParent){";
    print $iframe     "while(1) {";
    print $iframe       "curLeft += obj.offsetLeft;";
    print $iframe       "if (!obj.offsetParent)";
    print $iframe         "break;";
    print $iframe       "obj = obj.offsetParent;";
    print $iframe     "}";
    print $iframe   "} else if (obj.x)";
    print $iframe     "curLeft += obj.x;";
    print $iframe   "return curLeft;";
    print $iframe "}";
    print $iframe "function findPosY(obj){";
    print $iframe   "var curTop = 0;";
    print $iframe   "if (obj.offsetParent){";
    print $iframe     "while(1) {";
    print $iframe       "curTop += obj.offsetTop;";
    print $iframe       "if (!obj.offsetParent)";
    print $iframe         "break;";
    print $iframe       "obj = obj.offsetParent;";
    print $iframe     "}";
    print $iframe   "} else if (obj.x)";
    print $iframe     "curTop += obj.x;";
    print $iframe   "return curTop;";
    print $iframe "}";
    print $iframe "function coord(event,obj,n,x,y){";
    print $iframe   "if (event.offsetX == undef) {";
    print $iframe     "x += event.pageX-findPosX(obj)-1;";
    print $iframe     "y += event.pageY-findPosY(obj)-1;";
    print $iframe   "} else {";
    print $iframe     "x += event.offsetX;";
    print $iframe     "y += event.offsetY;";
    print $iframe   "}";
    print $iframe   "document['Coord'+n].X.value = x;";
    print $iframe   "document['Coord'+n].Y.value = y;";
    print $iframe "}</SCRIPT></HEAD><BODY onLoad=\"parent.document.getElementById('iframe".$framedir."').style.height=document.getElementById('content').offsetHeight;parent.document.getElementById('iframe".$framedir."').style.width=document.getElementById('content').offsetWidth;\">";
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
    open($html, ">", $basedir."/".getfilename($filenum));

    print $html "<HTML><HEAD><TITLE>Bitmap Comparison</TITLE></HEAD>";
    print $html "<BODY>";
    
    if ($filenum > 0) {
        print $html "<P>";
        if ($num > 1)
        {
            print $html "<A href=\"".getfilename(0)."\">First</A>&nbsp;&nbsp;";
        }
        print $html "<A href=\"".getfilename($filenum-1)."\">Previous(".($filenum-1).")</A>";
        print $html "</P>";
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
$basedir      = $ARGV[0];
$ARGV         = shift @ARGV;
$filenum      = 0;
$framenum     = 0;

# Create the output dir/html file
mkdir $basedir;
openhtml();

# Keep a list of files we've done, so we don't repeat
%done = ();

# Keep a list of files we fail to find any changes in, so we can list them
# at the end.
%nodiffs = ();

# Now run through the list of files
$images = 0;
while (<>)
{
    ($path,$exe,$m1,$m2) = /(\S+)\s+(\S+)\s+(\S+)\s+(\S+)/;
    ($file,$fmt,$res,$band) = ($path =~ /(\S+)\.(\S+)\.(\d+)\.(\d+)$/);

    # Adjust for the local fs layout
    $file = $fileadjust.$file;

    # Check the file exists
    $file2 = "";
    if (!stat($file))
    {
        # Before we give up, consider the possibility that we might need to
        # pdfwrite it.
        # Someone who speaks perl can do this more nicely.
        $file2 = $file;
        ($file2) = ($file2 =~ /(\S+).pdf$/);
        if (!stat($file2))
        {
            print "Unknown file: ".$file." (".$exe.")\n";
            next;
        }
        $exe = "pw".$exe;
    }

    # Avoid doing the same thing twice
    if ($done{$file.":".$exe.":".$res})
    {
        next;
    }
    $done{$file.":".$exe.":".$res} = 1;
    
    # Start a new file if required
    if ($setsthisfile >= $maxsets)
    {
        nexthtml();
    }
    
    # Open the iframe
    openiframe();

    # Output the title
    dprint($html,$iframe,"<H1>".$framenum.": ".$file." (".$res."dpi)</H1></BR>");
    
    # Generate the appropriate bmps to diff
    print $exe.": ".$file."\n";
    if ($exe eq "gs")
    {
        $cmd  =     $gsexe;
        $cmd .= " -r".$res;
        $cmd .= " -sOutputFile=".$outdir."/tmp1_%d.bmp";
        $cmd .= " ".$gsargs;
        if ($file =~ m/\.PS$/) { $cmd .= " ".$gsargsPS; };
        $cmd .= $redir.$file;
        $cmd2  = $reference.$gsexe;
        $cmd2 .= " -r".$res;
        $cmd2 .= " -sOutputFile=".$outdir."/tmp2_%d.bmp";
        $cmd2 .= " ".$gsargs;
        if ($file =~ m/\.PS$/) { $cmd2 .= " ".$gsargsPS; }
        $cmd2 .= $redir.$file;
        if (runjobs($cmd, $cmd2, $html, $iframe,
                    "New", "Ref", "bitmap generation")) {
            next;
        }
    }
    elsif ($exe eq "pcl")
    {
        $cmd   =     $pclexe;
        $cmd  .= " ".$pclargs;
        $cmd  .= " -r".$res;
        $cmd  .= " -sOutputFile=".$outdir."/tmp1_%d.bmp";
        $cmd  .= " ".$file;
        $cmd2  = $reference.$pclexe;
        $cmd2 .= " ".$pclargs;
        $cmd2 .= " -r".$res;
        $cmd2 .= " -sOutputFile=".$outdir."/tmp2_%d.bmp";
        $cmd2 .= " ".$file;
        if (runjobs($cmd, $cmd2, $html, $iframe,
                    "New", "Ref", "bitmap generation")) {
            next;
        }
    }
    elsif ($exe eq "xps")
    {
        $cmd   =     $xpsexe;
        $cmd  .= " ".$xpsargs;
        $cmd  .= " -r".$res;
        $cmd  .= " -sOutputFile=".$outdir."/tmp1_%d.bmp";
        $cmd  .= " ".$file;
        $cmd2  = $reference.$xpsexe;
        $cmd2 .= " ".$xpsargs;
        $cmd2 .= " -r".$res;
        $cmd2 .= " -sOutputFile=".$outdir."/tmp2_%d.bmp";
        $cmd2 .= " ".$file;
        if (runjobs($cmd, $cmd2, $html, $iframe,
                    "New", "Ref", "bitmap generation")) {
            next;
        }
    }
    elsif ($exe eq "svg")
    {
        $cmd   =     $svgexe;
        $cmd  .= " ".$svgargs;
        $cmd  .= " -r".$res;
        $cmd  .= " -sOutputFile=".$outdir."/tmp1_%d.bmp";
        $cmd  .= " ".$file;
        $cmd2  = $reference.$svgexe;
        $cmd2 .= " ".$svgargs;
        $cmd2 .= " -r".$res;
        $cmd2 .= " -sOutputFile=".$outdir."/tmp2_%d.bmp";
        $cmd2 .= " ".$file;
        if (runjobs($cmd, $cmd2, $html, $iframe,
                    "New", "Ref", "bitmap generation")) {
            next;
        }
    }
    elsif ($exe eq "pwgs")
    {
        $cmd   =     $gsexe;
        $cmd  .= " -r".$res;
        $cmd  .= " -sOutputFile=".$outdir."/tmp1.pdf";
        $cmd  .= " ".$pwgsargs;
        if ($file2 =~ m/\.PS$/) { $cmd .= " ".$gsargsPS; }
        $cmd  .= $redir.$file2;
        $cmd2  = $reference.$gsexe;
        $cmd2 .= " -r".$res;
        $cmd2 .= " -sOutputFile=".$outdir."/tmp2.pdf";
        $cmd2 .= " ".$pwgsargs;
        if ($file2 =~ m/\.PS$/) { $cmd2 .= " ".$gsargsPS; }
        $cmd2 .= $redir.$file2;
        if (runjobs($cmd, $cmd2, $html, $iframe,
                    "New", "Ref", "pdf generation")) {
            next;
        }

        $cmd   =     $gsexe;
        $cmd  .= " -r".$res;
        $cmd  .= " -sOutputFile=".$outdir."/tmp1_%d.bmp";
        $cmd  .= " ".$gsargs;
        $cmd  .= $redir.$outdir."/tmp1.pdf";
        $cmd2  =     $gsexe;
        $cmd2 .= " -r".$res;
        $cmd2 .= " -sOutputFile=".$outdir."/tmp2_%d.bmp";
        $cmd2 .= " ".$gsargs;
        $cmd2 .= $redir.$outdir."/tmp2.pdf";
        if (runjobs($cmd, $cmd2, $html, $iframe,
                    "New", "Ref", "bitmap generation")) {
            next;
        }
        unlink $outdir."/tmp1.pdf";
        unlink $outdir."/tmp2.pdf";
    }
    elsif ($exe eq "pwpcl")
    {
        $cmd   =     $pclexe;
        $cmd  .= " ".$pwpclargs;
        $cmd  .= " -r".$res;
        $cmd  .= " -sOutputFile=".$outdir."/tmp1.pdf";
        $cmd  .= " ".$file2;
        $cmd2  = $reference.$pclexe;
        $cmd2 .= " ".$pwpclargs;
        $cmd2 .= " -r".$res;
        $cmd2 .= " -sOutputFile=".$outdir."/tmp2.pdf";
        $cmd2 .= " ".$file2;
        if (runjobs($cmd, $cmd2, $html, $iframe,
                    "New", "Ref", "pdf generation")) {
            next;
        }

        $cmd   =     $gsexe;
        $cmd  .= " -r".$res;
        $cmd  .= " -sOutputFile=".$outdir."/tmp1_%d.bmp";
        $cmd  .= " ".$gsargs;
        $cmd  .= $redir.$outdir."/tmp1.pdf";
        $cmd2  = $reference.$gsexe;
        $cmd2 .= " -r".$res;
        $cmd2 .= " -sOutputFile=".$outdir."/tmp2_%d.bmp";
        $cmd2 .= " ".$gsargs;
        $cmd2 .= $redir.$outdir."/tmp2.pdf";
        if (runjobs($cmd, $cmd2, $html, $iframe,
                    "New", "Ref", "bitmap generation")) {
            next;
        }
        unlink $outdir."/tmp1.pdf";
        unlink $outdir."/tmp2.pdf";
    }
    elsif ($exe eq "pwxps")
    {
        $cmd   =     $xpsexe;
        $cmd  .= " ".$pwxpsargs;
        $cmd  .= " -r".$res;
        $cmd  .= " -sOutputFile=".$outdir."/tmp1.pdf";
        $cmd  .= " ".$file2;
        $cmd2  = $reference.$xpsexe;
        $cmd2 .= " ".$pwxpsargs;
        $cmd2 .= " -r".$res;
        $cmd2 .= " -sOutputFile=".$outdir."/tmp2.pdf";
        $cmd2 .= " ".$file2;
        if (runjobs($cmd, $cmd2, $html, $iframe,
                    "New", "Ref", "pdf generation")) {
            next;
        }

        $cmd   =     $gsexe;
        $cmd  .= " -r".$res;
        $cmd  .= " -sOutputFile=".$outdir."/tmp1_%d.bmp";
        $cmd  .= " ".$gsargs;
        $cmd  .= $redir.$outdir."/tmp1.pdf";
        $cmd2  = $reference.$gsexe;
        $cmd2 .= " -r".$res;
        $cmd2 .= " -sOutputFile=".$outdir."/tmp2_%d.bmp";
        $cmd2 .= " ".$gsargs;
        $cmd2 .= $redir.$outdir."/tmp2.pdf";
        if (runjobs($cmd, $cmd2, $html, $iframe,
                    "New", "Ref", "bitmap generation")) {
            next;
        }
        unlink $outdir."/tmp1.pdf";
        unlink $outdir."/tmp2.pdf";
    }
    elsif ($exe eq "pwsvg")
    {
        $cmd   =     $svgexe;
        $cmd  .= " ".$pwsvgargs;
        $cmd  .= " -r".$res;
        $cmd  .= " -sOutputFile=".$outdir."/tmp1.pdf";
        $cmd  .= " ".$file2;
        $cmd2  = $reference.$svgexe;
        $cmd2 .= " ".$pwsvgargs;
        $cmd2 .= " -r".$res;
        $cmd2 .= " -sOutputFile=".$outdir."/tmp2.pdf";
        $cmd2 .= " ".$file2;
        if (runjobs($cmd, $cmd2, $html, $iframe,
                    "New", "Ref", "pdf generation")) {
            next;
        }

        $cmd   =     $gsexe;
        $cmd  .= " -r".$res;
        $cmd  .= " -sOutputFile=".$outdir."/tmp1_%d.bmp";
        $cmd  .= " ".$gsargs;
        $cmd  .= $redir.$outdir."/tmp1.pdf";
        $cmd2  = $reference.$gsexe;
        $cmd2 .= " -r".$res;
        $cmd2 .= " -sOutputFile=".$outdir."/tmp2_%d.bmp";
        $cmd2 .= " ".$gsargs;
        $cmd2 .= $redir.$outdir."/tmp2.pdf";
        if (runjobs($cmd, $cmd2, $html, $iframe,
                    "New", "Ref", "bitmap generation")) {
            next;
        }
        unlink $outdir."/tmp1.pdf";
        unlink $outdir."/tmp2.pdf";
    }
    else
    {
        print "Unknown program: ".$exe."\n";
        next;
    }

    # Now diff those things
    $page = 1;
    $diffs = 0;
    while (stat($tmp1 = $outdir."/tmp1_".$page.".bmp") &&
           stat($tmp2 = $outdir."/tmp2_".$page.".bmp"))
    {
        $cmd  = $bmpcmpexe;
        $cmd .= " ".$tmp1;
        $cmd .= " ".$tmp2;
        $cmd .= " ".$outdir."/out";
        $cmd .= " ".$images;
        $ret = system($cmd);
        if ($ret != 0)
        {
            print "Image differ failed!\n";
            dprint($html,$html2,"<p>Image differ failed!</p>\n");
        }
        # Delete the temporary files
        unlink $tmp1;
        unlink $tmp2;

        # Add the files to the HTML, converting to PNG if required.
        while (stat($outdir."/out.".$images.".bmp"))
        {
            $suffix = ".bmp";
            if ($pngize)
            {
                $cmd   = $convertexe." ";
                $cmd  .= $outdir."/out.".$images.".bmp ";
                $cmd  .= $outdir."/out.".$images.".png";
                $cmd2  = $convertexe." ";
                $cmd2 .= $outdir."/out.".($images+1).".bmp ";
                $cmd2 .= $outdir."/out.".($images+1).".png";
                $cmd3  = $convertexe." ";
                $cmd3 .= $outdir."/out.".($images+2).".bmp ";
                $cmd3 .= $outdir."/out.".($images+2).".png";
                runjobs3($cmd, $cmd2, $cmd3, $html, $iframe, "convert");
                unlink $outdir."/out.".$images.".bmp";
                unlink $outdir."/out.".($images+1).".bmp";
                unlink $outdir."/out.".($images+2).".bmp";
                $suffix = ".png";
            }
            
            $metafile = $outdir."/out.".$images.".meta";
            $meta{"X"}    = 0;
            $meta{"Y"}    = 0;
            $meta{"PW"}   = 0;
            $meta{"PH"}   = 0;
            $meta{"W"}    = 0;
            $meta{"H"}    = 0;
            if (stat($metafile))
            {
                open(METADATA, $metafile);
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
                unlink $metafile;
            }
            
            $mousemove = "onmousemove=\"coord(event,this,".$images.",".$meta{"X"}.",".$meta{"Y"}.")\"";
            
            dprint($html,$iframe,"<TABLE><TR><TD><IMG SRC=\"out.".$images.$suffix."\" onMouseOver=\"swap(".$images.")\" onMouseOut=\"swap(".($images+1).")\" NAME=\"compare".$images."\" BORDER=1 TITLE=\"Candidate<->Reference: ".$file." page=".$page." res=".$res."\" ".$mousemove."></TD>");
           dprint($html,$iframe,"<TD><IMG SRC=\"out.".($images+1).$suffix."\" NAME=\"compare".($images+1)."\" BORDER=1 TITLE=\"Reference: ".$file." page=".$page." res=".$res."\" ".$mousemove."></TD>");
           dprint($html,$iframe,"<TD><IMG SRC=\"out.".($images+2).$suffix."\" BORDER=1 TITLE=\"Diff: ".$file." page=".$page." res=".$res."\" ".$mousemove."></TD></TR>");
           dprint($html,$iframe,"<TR><TD COLSPAN=3><FORM name=\"Coord".$images."\"><LABEL for=\"X\">Page=".$page." PageSize=".$meta{"PW"}."x".$meta{"PH"}." Res=".$res." TopLeft=(".$meta{"X"}.",".$meta{"Y"}.") W=".$meta{"W"}." H=".$meta{"H"}." </LABEL><INPUT type=\"text\" name=\"X\" value=0 size=3>X<INPUT type=\"text\" name=\"Y\" value=0 size=3>Y</FORM></TD></TR></TABLE><BR>");
           $images += 3;
           $diffs++;
           $setsthisfile++;
        }

        $page++;
    }
    
    closeiframe();
    
    if ($diffs == 0)
    {
        print "Failed to find any differences on any page!\n";
        push @nodiffs, $file.":".$exe.":".$res;
    }
}

# Finish off the HTML file
closehtml();

# List the files that we expected to find differences in, but failed to:
if (scalar(@nodiffs) != 0)
{
  print "Failed to find expected differences in the following:\n";
  
  foreach $file (@nodiffs)
  {
      print "  ".$file."\n";
  }
}
