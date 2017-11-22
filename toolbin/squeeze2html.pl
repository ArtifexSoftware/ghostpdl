#!/usr/bin/perl

# #include <disclaimer.h>
# If you speak perl, and are offended by the code herein, I apologise.
# Please feel free to tidy it up.

# Syntax: squeeze2html.pl < in > out.html
# or (more usefully):
#         squeeze2html.pl -q < in > out.html
# (to hide the ones that passed)

# Setup steps:
# 1) make debug XCFLAGS="-DMEMENTO"
# 2) MEMENTO_SQUEEZEAT=1 gs -sDEVICE=png16m -o /dev/null 2>&1 > log

# Options
my $quiet = 0;

# Handle options:
use Getopt::Std;

my %opts;
getopts('q', \%opts);
if ($opts{q} == 1) {
    $quiet = 1;
}

sub html_escape($)
{
    my $line = shift;

    $line =~ s/&/&amp;/g;
    $line =~ s/\</&lt;/g;
    $line =~ s/\>/&gt;/g;

    return $line;
}

# First we output the headers, our javascript code, and an empty table.
print "<html><head><title>Memory squeezer results</title>";
print "<script>\n";
print "function show(id) {\n";
print "  tag = document.getElementById('txt'+id);\n";
print "  if (tag == null) {return;}\n";
print "  if (tag.hiddenFlag == 0) {\n";
print "    tag.setAttribute('style', 'display:none')\n";
print "    tag.hiddenFlag = 1;\n";
print "  } else {\n";
print "    tag.setAttribute('style', 'display:block')\n";
print "    tag.hiddenFlag = 0;\n";
print "  }\n";
print "}\n";
print "function segv(id) {\n";
print "  tag = document.getElementById('but'+id);\n";
print "  if (tag == null) {return;}\n";
print "  tag.setAttribute('style','background-color:#f88');";
print "}\n";
print "function leak(id) {\n";
print "  tag = document.getElementById('but'+id);\n";
print "  if (tag == null) {return;}\n";
print "  tag.setAttribute('style','background-color:#fc8');";
print "}\n";
print "function ok(id) {\n";
print "  tag = document.getElementById('but'+id);\n";
print "  if (tag == null) {return;}\n";
print "  tag.setAttribute('style','background-color:#8f8');";
print "}\n";
print "</script>";
print "</head><body>";

my $segv = 0;
my $leak = 0;
my $in = 0;
my $buffer = "";
my $inbuffer = "";

sub start_run($) {
    my $in = shift;
    $segv = 0;
    $leak = 0;
}

sub end_run($) {
    my $in = shift;

    if ($quiet == 0 || $segv != 0 || $leak != 0) {
        print "<span id=\"but$in\"><a href=\"javascript:show($in);\">$in</a></span>&nbsp;<div id=\"txt$in\" style=\"display: none\"><pre>";
        print html_escape($inbuffer);
        print "</pre></div>\n";
        if ($segv == 1) {
            print "<script>segv($in);</script>";
        } elsif ($leak == 1) {
            print "<script>leak($in);</script>";
        } else {
            print "<script>ok($in);</script>";
        }
    }
    $inbuffer="";
}

while (<>)
{
    my $line = $_;
    chomp $line;

    if ($line =~ m/^Memory squeezing @ (.*) complete/) {
        end_run($in);
	$in = 0;
    } elsif  ($line =~ m/^Memory squeezing @ (.*)$/) {
        $in = $1;
        if ($buffer ne "") {
            print "<!-- ".html_escape($buffer)." -->\n";
            $buffer="";
        }
        start_run($in);
    } elsif ($in) {
        if ($line =~ m/^SEGV at:/) {
            $segv = 1;
        } elsif ($line =~ m/^Allocated blocks/) {
            $leak = 1;
        }
	$inbuffer = $inbuffer.$line."\n";
    } else {
        $buffer = $buffer."\n".$line;
    }
}

start_run("final");
print html_escape("$buffer\n");
end_run("final");
