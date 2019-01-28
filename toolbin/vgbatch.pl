#!/usr/bin/perl

# #include <disclaimer.h>
# If you speak perl, and are offended by the code herein, I apologise.
# Please feel free to tidy it up.

# Syntax: vgbatch.pl < <input list>

# Setup steps:
# 1) Build your binaries ("make vg" or "make debugvg" are good ideas)
# 2) Edit the paths/options here as appropriate.
# 3) Save the list of changed files from the local cluster regression email
# here (e.g. as list.txt).
# 4) Invoke this script. (e.g. "toolbin/vgbatch.pl < list.txt > vg.out 2>&1 &")
# 5) While that runs, you can see how far it's gone using: "tail -f vg.out"
# 6) Make tea. Drink tea.
#
# list.txt should look like:
#
# tests_private/pdf/PDF_2.0_FTS/fts_01_0108.pdf.ppmraw.300.0 gs ...
# tests_private/pdf/PDF_2.0_FTS/fts_02_0230.pdf.ppmraw.300.0 gs ...
# ...
#
# Basically this mirrors the list of failed jobs given in a cluster test
# email.


########################################################################
# SETUP SECTION

# The path to the executables.
#$gsexe     = "gs/bin/gswin32c.exe";
$gsexe     = "bin/gs";
$pclexe    = "bin/gpcl6";
$xpsexe    = "bin/gxps";
$gpdlexe   = "bin/gpdl";

# Set the following if you want to override all the tests to use a
# particular output format/device.
#$format_override="bmp";
$format_override="";

# The path from your ghostpdl directory to where the test files can be
# found
$fileadjust = "/home/marcos/cluster/";

# END SETUP SECTION
########################################################################

########################################################################
# EXTERNAL USES
use Errno qw(EAGAIN);

########################################################################

########################################################################
# FUNCTIONS


# END FUNCTIONS
########################################################################

########################################################################
# Here follows todays lesson. Abandon hope all who enter here. Etc. Etc.
$basedir      = $ARGV[0];
$ARGV         = shift @ARGV;

# Now run through the list of files
$images = 0;
while (<>)
{
    ($path,$exe) = /^(\S+)\s+(\S+)/;
    ($file,$fmt,$res,$band) = ($path =~ /(\S+)\.(\S+)\.(\d+)\.(\d+)$/);

    $file =~ s/__/\//g;
    
    if ($file eq "") {
        next;
    }

    # Adjust for the local fs layout
    $file = $fileadjust.$file;

    # Check the file exists
    $file2 = "";
    if (!stat($file))
    {
        # Before we give up, consider the possibility that we might need to
        # pdfwrite it.
        # Someone who speaks perl can do this more nicely.
        ($file2) = ($file =~ /(\S+).pdf$/);
        if (stat($file2))
        {
            $exe = "pdfw".$exe;
	}
        else
        {
            ($file2) = ($file =~ /(\S+).ps$/);
            if (!stat($file2))
            {
                print "Unknown file: ".$file." (".$exe.")\n";
                next;
            }
            $exe = "psw".$exe;
	}
    }

    # Avoid doing the same thing twice
    if ($done{"$file:fmt:$res:$band:$exe"})
    {
	print "Repeated test: $file:$fmt:$res:$bad:$exe\n";
        next;
    }
    $done{"$file:$fmt:$res:$band:$exe"} = 1;
    
    # Map format to device
    if ($fmt eq "ppmraw") {
        $devargs="-sDEVICE=ppmraw";
        $suffix="ppm";
    } elsif ($fmt eq "pbmraw") {
        $devargs="-sDEVICE=pbmraw";
        $suffix="pbm";
    } elsif ($fmt eq "pam") {
        $devargs="-sDEVICE=pam";
        $suffix="pam";
    } elsif ($fmt eq "pgmraw") {
        $devargs="-sDEVICE=pgmraw";
        $suffix="pgm";
    } elsif ($fmt eq "pnmcmyk") {
        $devargs="-sDEVICE=pnmcmyk";
        $suffix="pnm";
    } elsif ($fmt eq "pkmraw") {
        $devargs="-sDEVICE=pkmraw";
        $suffix="pkm";
    } elsif ($fmt eq "bmp") {
        $devargs="-sDEVICE=bmp16m";
        $suffix="bmp";
    } elsif ($fmt eq "png") {
        $devargs="-sDEVICE=png16m";
        $suffix="png";
    } elsif ($fmt eq "tiffscaled") {
        $devargs="-sDEVICE=tiffscaled";
        $suffix="tif";
    } elsif ($fmt eq "bitrgb") {
        $devargs="-sDEVICE=bitrgb";
        $suffix="bit";
    } elsif ($fmt eq "bitrgbtags") {
        $devargs="-sDEVICE=bitrgbtags";
        $suffix="bit";
    } elsif ($fmt eq "cups") {
        $devargs="-sDEVICE=cups";
        $suffix="cups";
    } elsif ($fmt eq "plank") {
        $devargs="-sDEVICE=plank";
        $suffix="plank";
    } elsif ($fmt eq "psdcmyk") {
        $devargs="-sDEVICE=psdcmyk";
        $suffix="psd";
    } elsif ($fmt eq "psdcmykog") {
        $devargs="-sDEVICE=psdcmykog";
        $suffix="psd";
    } else {
        print "Unsupported format $fmt - skipping\n";
        next;
    }
    
    # Output the title
    print "=====$path:$exe=====\n";

    my $resargs = " -r$res";
    my $bandargs = " -dMaxBitmap=400000000";
    if ($band == 1) {
	$bandargs = " -dMaxBitmap=1000";
    }
    my $randargs = " -Z: -dNOPAUSE -dBATCH -K2000000 -dClusterJob";

    my $binargs;
    my $psargs = "";
    if ($file =~ m/\.PS$/) { $psargs = " -dCETMODE"; };
    if ($exe =~ m/gs/)
    {
        $binargs = $gsexe;
    }
    elsif ($exe =~ m/pcl/)
    {
        $binargs = $pclexe;
    }
    elsif ($exe =~ m/xps/)
    {
        $binargs = $xpsexe;
    }
    elsif ($exe =~ m/gpdl/)
    {
        $binargs = $gpdlexe;
    }
    else
    {
	die "$bin not matched; dying";
    }

    my $dev1args = $devargs;
    my $out1args = "/dev/null";
    my $two_stage = 0;
    if ($exe =~ m/pdfw/)
    {
	$dev1args = "-sDEVICE=pdfwrite";
	$out1args = "out.pdf";
	$two_stage = 1;
    }
    elsif ($exe =~ m/psw/)
    {
	$dev1args = "-sDEVICE=ps2write";
	$out1args = "out.ps";
	$two_stage = 1;
    }

    $cmd = "valgrind --track-origins=yes $binargs $psargs -sOutputFile=$out1args $bandargs $dev1args $resargs $randargs $file2";
    #system("echo $cmd > out.vg");
    #system("$cmd >>& out.vg");
    system("echo $cmd");
    system("$cmd");
    my $ret = $?;
    #system("echo return $ret >> out.vg");
    system("echo return $ret");

    if ($ret == 0 && $two_stage != 0) {
	#system("echo ----- >> out.vg");
	system("echo -----");
        if ($exe =~ m/pdfw/ ||
            $exe =~ m/psw/)
        {
            $binargs = $gsexe;
        }
	else
	{
	    die "$exe second stage not matchined; dying";
	}

	$cmd = "valgrind --track-origins=yes $binargs $psargs -sOutputFile=/dev/null $bandargs $devargs $resargs $randargs $out1args";
	#system("echo $cmd >> out.vg");
	#system("$cmd >>& out.vg");
	system("echo $cmd");
	system("$cmd");
	$ret = $?;
        #system("echo return $ret >> out.vg");
        system("echo return $ret");
    }
}

print "TESTING COMPLETED!\n";
