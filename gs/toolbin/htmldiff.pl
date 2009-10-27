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


# The path to the executables.
#$gsexe     = "gs/bin/gswin32c.exe";
$gsexe     = "gs\\debugbin\\gswin32c.exe";
$pclexe    = "main\\obj\\pcl6.exe";
$xpsexe    = "xps\\obj\\gxps.exe";
$bmpcmpexe = "..\\bmpcmp\\bmpcmp\\Debug\\bmpcmp.exe";
$convertexe= "convert.exe"; # ImageMagick

# The args fed to the different exes. Probably won't need to play with these.
$gsargs    = "-sDEVICE=bmp16m -dNOPAUSE -dBATCH -q";
$pclargs   = "-sDEVICE=bmp16m -dNOPAUSE";
$xpsargs   = "-sDEVICE=bmp16m -dNOPAUSE";
$pwgsargs  = "-sDEVICE=pdfwrite -dNOPAUSE -dBATCH -q";
$pwpclargs = "-sDEVICE=pdfwrite -dNOPAUSE";
$pwxpsargs = "-sDEVICE=pdfwrite -dNOPAUSE";

# Set the following to true to convert bmps to pngs (useful to save space
# if you want to show people this across the web).
$pngize = 1;

# The path from your ghostpdl directory to where the test files can be
# found
$fileadjust = "../ghostpcl/";

# The path to prepend to each of the above exe's to get the reference
# version.
$reference = "..\\ghostpdlREF\\";


# Here follows todays lesson. Abandon hope all who enter here. Etc. Etc.
$outdir = $ARGV[0];
$ARGV = shift @ARGV;

# Create the output dir/html file
mkdir $outdir;
open($html, ">", $outdir."/compare.html");

print $html "<HTML><HEAD><TITLE>Bitmap Comparison</TITLE></HEAD>";
print $html "<SCRIPT LANGUAGE=\"JavaScript\">";
print $html "function swap(n){";
print $html   "var x = document.images['compare'+3*Math.floor(n/3)].src;";
print $html   "document.images['compare'+3*Math.floor(n/3)].src=document.images['compare'+(3*Math.floor(n/3)+1)].src;";
print $html   "document.images['compare'+(3*Math.floor(n/3)+1)].src = x;";
print $html "}</SCRIPT><BODY>";

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
    
    # Generate the appropriate bmps to diff
    print $exe.": ".$file."\n";
    if ($exe eq "gs")
    {
        $cmd  =     $gsexe;
        $cmd .= " ".$gsargs;
        $cmd .= " -r".$res;
        $cmd .= " -sOutputFile=".$outdir."/tmp1_%d.bmp";
        $cmd .= " ".$file;
        $ret = system($cmd);
        if ($ret != 0)
        {
            print "New bitmap generation failed with exit code ".$ret."\n";
            print "Command was: ".$cmd;
            next;
        }
        $cmd  = $reference.$gsexe;
        $cmd .= " ".$gsargs;
        $cmd .= " -r".$res;
        $cmd .= " -sOutputFile=".$outdir."/tmp2_%d.bmp";
        $cmd .= " ".$file;
        $ret = system($cmd);
        if ($ret != 0)
        {
            print "Ref bitmap generation failed with exit code ".$ret."\n";
            print "Command was: ".$cmd;
            next;
        }
    }
    elsif ($exe eq "pcl")
    {
        $cmd  =     $pclexe;
        $cmd .= " ".$pclargs;
        $cmd .= " -r".$res;
        $cmd .= " -sOutputFile=".$outdir."/tmp1_%d.bmp";
        $cmd .= " ".$file;
        $ret = system($cmd);
        if ($ret != 0)
        {
            print "New bitmap generation failed with exit code ".$ret."\n";
            print "Command was: ".$cmd;
            next;
        }
        $cmd  = $reference.$pclexe;
        $cmd .= " ".$pclargs;
        $cmd .= " -r".$res;
        $cmd .= " -sOutputFile=".$outdir."/tmp2_%d.bmp";
        $cmd .= " ".$file;
        $ret = system($cmd);
        if ($ret != 0)
        {
            print "Ref bitmap generation failed with exit code ".$ret."\n";
            print "Command was: ".$cmd;
            next;
        }
    }
    elsif ($exe eq "xps")
    {
        $cmd  =     $xpsexe;
        $cmd .= " ".$xpsargs;
        $cmd .= " -r".$res;
        $cmd .= " -sOutputFile=".$outdir."/tmp1_%d.bmp";
        $cmd .= " ".$file;
        $ret = system($cmd);
        if ($ret != 0)
        {
            print "New bitmap generation failed with exit code ".$ret."\n";
            print "Command was: ".$cmd;
            next;
        }
        $cmd  = $reference.$xpsexe;
        $cmd .= " ".$xpsargs;
        $cmd .= " -r".$res;
        $cmd .= " -sOutputFile=".$outdir."/tmp2_%d.bmp";
        $cmd .= " ".$file;
        $ret = system($cmd);
        if ($ret != 0)
        {
            print "Ref bitmap generation failed with exit code ".$ret."\n";
            print "Command was: ".$cmd;
            next;
        }
    }
    elsif ($exe eq "pwgs")
    {
        $cmd  =     $gsexe;
        $cmd .= " ".$pwgsargs;
        $cmd .= " -r".$res;
        $cmd .= " -sOutputFile=".$outdir."/tmp1.pdf";
        $cmd .= " ".$file2;
        $ret = system($cmd);
        if ($ret != 0)
        {
            print "New pdf generation failed with exit code ".$ret."\n";
            print "Command was: ".$cmd;
            next;
        }
        $cmd  = $reference.$gsexe;
        $cmd .= " ".$pwgsargs;
        $cmd .= " -r".$res;
        $cmd .= " -sOutputFile=".$outdir."/tmp2.pdf";
        $cmd .= " ".$file2;
        $ret = system($cmd);
        if ($ret != 0)
        {
            print "Ref pdf generation failed with exit code ".$ret."\n";
            print "Command was: ".$cmd;
            next;
        }
        $cmd  =     $gsexe;
        $cmd .= " ".$gsargs;
        $cmd .= " -r".$res;
        $cmd .= " -sOutputFile=".$outdir."/tmp1_%d.bmp";
        $cmd .= " ".$outdir."/tmp1.pdf";
        $ret = system($cmd);
        if ($ret != 0)
        {
            print "New bitmap generation failed with exit code ".$ret."\n";
            print "Command was: ".$cmd;
            next;
        }
        $cmd  =     $gsexe;
        $cmd .= " ".$gsargs;
        $cmd .= " -r".$res;
        $cmd .= " -sOutputFile=".$outdir."/tmp2_%d.bmp";
        $cmd .= " ".$outdir."/tmp2.pdf";
        $ret = system($cmd);
        if ($ret != 0)
        {
            print "Ref bitmap generation failed with exit code ".$ret."\n";
            print "Command was: ".$cmd;
            next;
        }
        unlink $outdir."/tmp1.pdf";
        unlink $outdir."/tmp2.pdf";
    }
    elsif ($exe eq "pwpcl")
    {
        $cmd  =     $pclexe;
        $cmd .= " ".$pwpclargs;
        $cmd .= " -r".$res;
        $cmd .= " -sOutputFile=".$outdir."/tmp1.pdf";
        $cmd .= " ".$file2;
        $ret = system($cmd);
        if ($ret != 0)
        {
            print "New pdf generation failed with exit code ".$ret."\n";
            print "Command was: ".$cmd;
            next;
        }
        $cmd  = $reference.$pclexe;
        $cmd .= " ".$pwpclargs;
        $cmd .= " -r".$res;
        $cmd .= " -sOutputFile=".$outdir."/tmp2.pdf";
        $cmd .= " ".$file2;
        $ret = system($cmd);
        if ($ret != 0)
        {
            print "Ref pdf generation failed with exit code ".$ret."\n";
            print "Command was: ".$cmd;
            next;
        }
        $cmd  =     $gsexe;
        $cmd .= " ".$gsargs;
        $cmd .= " -r".$res;
        $cmd .= " -sOutputFile=".$outdir."/tmp1_%d.bmp";
        $cmd .= " ".$outdir."/tmp1.pdf";
        $ret = system($cmd);
        if ($ret != 0)
        {
            print "New bitmap generation failed with exit code ".$ret."\n";
            print "Command was: ".$cmd;
            next;
        }
        $cmd  = $reference.$gsexe;
        $cmd .= " ".$gsargs;
        $cmd .= " -r".$res;
        $cmd .= " -sOutputFile=".$outdir."/tmp2_%d.bmp";
        $cmd .= " ".$outdir."/tmp2.pdf";
        $ret = system($cmd);
        if ($ret != 0)
        {
            print "Ref bitmap generation failed with exit code ".$ret."\n";
            print "Command was: ".$cmd;
            next;
        }
        unlink $outdir."/tmp1.pdf";
        unlink $outdir."/tmp2.pdf";
    }
    elsif ($exe eq "pwxps")
    {
        $cmd  =     $xpsexe;
        $cmd .= " ".$pwxpsargs;
        $cmd .= " -r".$res;
        $cmd .= " -sOutputFile=".$outdir."/tmp1.pdf";
        $cmd .= " ".$file2;
        $ret = system($cmd);
        if ($ret != 0)
        {
            print "New bitmap generation failed with exit code ".$ret."\n";
            print "Command was: ".$cmd;
            next;
        }
        $cmd  = $reference.$xpsexe;
        $cmd .= " ".$pwxpsargs;
        $cmd .= " -r".$res;
        $cmd .= " -sOutputFile=".$outdir."/tmp2.pdf";
        $cmd .= " ".$file2;
        $ret = system($cmd);
        if ($ret != 0)
        {
            print "Ref bitmap generation failed with exit code ".$ret."\n";
            print "Command was: ".$cmd;
            next;
        }
        $cmd  =     $gsexe;
        $cmd .= " ".$gsargs;
        $cmd .= " -r".$res;
        $cmd .= " -sOutputFile=".$outdir."/tmp1_%d.bmp";
        $cmd .= " ".$outdir."/tmp1.pdf";
        $ret = system($cmd);
        if ($ret != 0)
        {
            print "New bitmap generation failed with exit code ".$ret."\n";
            print "Command was: ".$cmd;
            next;
        }
        $cmd  = $reference.$gsexe;
        $cmd .= " ".$gsargs;
        $cmd .= " -r".$res;
        $cmd .= " -sOutputFile=".$outdir."/tmp2_%d.bmp";
        $cmd .= " ".$outdir."/tmp2.pdf";
        $ret = system($cmd);
        if ($ret != 0)
        {
            print "Ref bitmap generation failed with exit code ".$ret."\n";
            print "Command was: ".$cmd;
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

    # Output the title
    print $html "<H1>".$file." (".$res."dpi)</H1></BR>";
    
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
                $cmd  = $convertexe." ";
                $cmd .= $outdir."/out.".$images.".bmp ";
                $cmd .= $outdir."/out.".$images.".png";
                $ret = system($cmd);
                if ($ret != 0)
                {
                    print "Conversion to png failed!\n";
                }
                unlink $outdir."/out.".$images.".bmp";
                $cmd  = $convertexe." ";
                $cmd .= $outdir."/out.".($images+1).".bmp ";
                $cmd .= $outdir."/out.".($images+1).".png";
                $ret = system($cmd);
                if ($ret != 0)
                {
                    print "Conversion to png failed!\n";
                }
                unlink $outdir."/out.".($images+1).".bmp";
                $cmd  = $convertexe." ";
                $cmd .= $outdir."/out.".($images+2).".bmp ";
                $cmd .= $outdir."/out.".($images+2).".png";
                $ret = system($cmd);
                if ($ret != 0)
                {
                    print "Conversion to png failed!\n";
                }
                unlink $outdir."/out.".($images+2).".bmp";
                $suffix = ".png";
            }
            print $html "<TABLE><TR><TD><IMG SRC=\"out.".$images.$suffix."\" onMouseOver=\"swap(".$images.")\" onMouseOut=\"swap(".($images+1).")\" NAME=\"compare".$images."\" BORDER=1 TITLE=\"Candidate: ".$file." page=".$page." res=".$res."\"></TD>";
           print $html "<TD><IMG SRC=\"out.".($images+1).$suffix."\" NAME=\"compare".($images+1)."\" BORDER=1 TITLE=\"Reference: ".$file." page=".$page." res=".$res."\"></TD>";
           print $html "<TD><IMG SRC=\"out.".($images+2).$suffix."\" BORDER=1 TITLE=\"Diff: ".$file." page=".$page." res=".$res."\"></TD></TR></TABLE><BR>";
           $images += 3;
           $diffs++;
        }

        $page++;
    }
    
    if ($diffs == 0)
    {
        print "Failed to find any differences on any page!\n";
        push @nodiffs, $file.":".$exe.":".$res;
    }
}

# Finish off the HTML file
print $html "</BODY>";
close $html;

# List the files that we expected to find differences in, but failed to:
if (scalar(@nodiffs) != 0)
{
  print "Failed to find expected differences in the following:\n";
  
  foreach $file (@nodiffs)
  {
      print "  ".$file."\n";
  }
}
