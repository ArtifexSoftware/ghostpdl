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

# First we output the headers, our javascript code, and an empty table.

print "<html>\n<head>\n<title>Memory squeeze results</title>\n";
print "<script>\n";
print "var count=0;\n";
print "var maxpoint=0;\n";
print "function hide(n) {\n";
print "  tag = document.getElementById('point'+n);\n";
print "  if (tag) {\n";
print "    tag.setAttribute('style', 'display : none');\n";
print "  }\n";
print "}\n";
print "function show(n) {\n";
print "  for (i = 0; i <= n; i++) {\n";
print "    tag = document.getElementById('point'+(2*i-1));\n";
print "    if (tag) {\n";
print "      tag.setAttribute('style', 'display : block');\n";
print "    }\n";
print "    tag = document.getElementById('point'+(2*i));\n";
print "    if (tag) {\n";
print "      tag.setAttribute('style', 'display : none');\n";
print "    }\n";
print "  }\n";
print "  tag = document.getElementById('point'+(2*n));\n";
print "  if (tag) {\n";
print "    tag.setAttribute('style', 'display : block');\n";
print "  }\n";
print "  for (i = 2*n+1; i <= 2*maxpoint; i++) {\n";
print "    tag = document.getElementById('point'+i);\n";
print "    if (tag) {\n";
print "      tag.setAttribute('style', 'display : none');\n";
print "    }\n";
print "  }\n";
print "}\n";
print "function addCell(n, i) {\n";
print "  var row;\n";
print "  rownum = count & ~31;\n";
print "  cell = document.createElement('td');\n";
print "  cell.setAttribute('style', 'background-color: '+(i == 0 ? '#0f0' : (i == 1 ? '#ff0' : '#f00')));\n";
print "  a = document.createElement('a');\n";
print "  a.appendChild(document.createTextNode(n));\n";
print "  a.href='javascript:show('+n+')';\n";
print "  cell.appendChild(a);\n";
print "  if ((count & 31) == 0) {\n";
print "    row = document.createElement('tr');\n";
print "    row.setAttribute('id', 'row'+rownum);\n";
print "  } else {\n";
print "    row = document.getElementById('row'+rownum);\n";
print "  }\n";
print "  row.appendChild(cell);\n";
print "  if ((count & 31) == 0) {\n";
print "    table = document.getElementById('points');\n";
print "    table.appendChild(row);\n";
print "  }\n";
print "  count++;\n";
print "  if (n > maxpoint) {\n";
print "    maxpoint = n;\n";
print "  }\n";
print "  hide(2*n);\n";
print "  hide(2*n+1);\n";
print "}\n";
print "function r(n) { addCell(n>>1, 2); }\n";
print "function y(n) { addCell(n>>1, 1); }\n";
print "function g(n) { addCell(n>>1, 0); }\n";
print "</script>\n";
print "</head>\n";
print "<body><table id=\"points\">";
print "</table>";

# Now we process the input.

my $at = 1;
my $i;
my $text = "";
my $status;
my $started;

LINE: while (my $line = <>)
{
    chomp $line;
    if (($i) = $line =~ m/SEGV after Memory squeezing @ (\d+)/)
    {
	$status |= 8;
    }
    elsif (($i) = $line =~ m/Memory squeezing @ (\d+) complete/)
    {
	$status |= 2;
	if ($started) {
            if (($quiet == 0) || (($at & 1) == 1) || (($status & 12) != 0)) {
                print "$text</pre></div>";
	    }
  	    if ($at & 1) {
	    } elsif ($status & 8) {
		print "<script>r($at)</script>";
	    } elsif ($status & 4) {
		print "<script>y($at)</script>";
	    } elsif ($quiet == 0) {
		print "<script>g($at)</script>";
	    }
	}
	$started = 0;
	$text = "";
        $at |= 1;
	next LINE;
    }
    elsif (($i) = $line =~ m/Memory squeezing @ (\d+)/)
    {
	if ($started) {
            if (($quiet == 0) || (($at & 1) == 1) || (($status & 12) != 0)) {
                print "$text</pre></div>";
            }
	    if ($at & 1) {
	    } elsif ($status & 8) {
		print "<script>r($at)</script>";
	    } elsif ($status & 4) {
		print "<script>y($at)</script>";
	    } elsif ($quiet == 0) {
		print "<script>g($at)</script>";
	    }
	}
	$at = $i * 2;
	$started = 0;
	$status = 1;
	next LINE;
    }
    elsif ($line =~ m/Allocated blocks:/)
    {
	$status |= 4;
    }
    if ($started == 0) {
        $text = "<div id='point$at'><pre>$line";
	$started = 1;
    } else {
        $text = $text."\n".$line;
    }
}
if ($started) {
  if (($quiet == 0) || (($at & 1) == 1) || (($status & 12) != 0)) {
    print "$text</pre></div>";
  }
  if ($at & 1) {
  } elsif ($status & 8) {
    print "<script>r($at)</script>";
  } elsif ($status & 4) {
    print "<script>y($at)</script>";
  } elsif ($quiet == 0) {
    print "<script>g($at)</script>";
  }
}

print "</body></html>";
