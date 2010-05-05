#!/usr/bin/perl

# Generate a table showing the history of files that have changed between
# given revisions. This script must be run on the cluster master machine
# (current casper).
#
#   --branch=<branch>
#   -b=<branch>
#        e.g. icc_work or mupdf. If not specified taken to be trunk.
#
#   --revision=<revision>
#   -r=<revision>
#        Final revision. (Initial revision is taken to be the one before)
#   --revision=<revision>:<revision>
#   -r=<revision>:<revision>
#        Initial/Final revision.
#   If -r is not specified -r is taken to be the most recent revision
#
#   --numrevs=<num>
#   -n=<num>
#        NumRevs. Maximum number of revisions to include.
#   If unspecified taken to be 20.
#
#   --to=<revision>
#   -t=<revision>
#        To revision. Latest revision to include in the table.
#   If omitted taken to be the most recent revision in this branch
#
#   --from=<revision>
#   -f=<revision>
#        From revision. Earliest revision to include in the table.
#   If omitted taken to be NumRevs before that given by -t.
#
# The table is generated from 'From' revision to 'To' revision,
# to a maximum of 'NumRevisions'. Lines are only included in the output
# table if the results differ between 'Initial' and 'Final' revisions.
#
# Probably best to explain with reference to some examples.
#
# 1) To show all the files that revision 1234 caused to change:
#   showtestchanges.pl -r=1234
#
# 2) To show all the files that changed between revision 1234 and 1240:
#   showtestchanges.pl -r1234:1240
#
# 3) To show all the files that changes on the icc branch between
# revision 1234 and 1240 with some specific context:
#   showtestchanges.pl -r1234:1240 -f=1200 -t=1260 -b=icc_work

use strict;
use warnings;
use POSIX;
use Getopt::Long;

# Set defaults
my $numrevs = 20; # How many revisions of history to display?
my $final;
my $initial;
my $from;
my $to;
my $branch;

# Set to 1 to take input from 'testfile' in PWD rather than to generate from
# scratch.
my $test = 0;

# You won't need to change this.
my $clusteroot = '/home/regression/cluster';

# Let's parse some command line parameters
GetOptions("b|branch=s"   => \$branch,
           "r|revision=s" => \$final,
           "n|numrevs=i"  => \$numrevs,
           "f|from=i"     => \$from,
           "t|to=i"       => \$to);

# Interpret the options
if ((!defined($branch)) || ($branch eq "trunk")) {
    $branch = "";
} elsif (substr($branch,-1,1) ne '-') {
    $branch .= "-";
}

# Get the possible list of versions for this branch, sorted by time
my @versions = <$clusteroot/archive/*.tab>;
@versions = map { m/(.*\/$branch\d+.tab)/ } @versions;
my @sorted = map {$_->[0]} sort {$b->[1]<=>$a->[1]} map {[$_, -M]} @versions;
my @rawsorted = map { m/.*\/$branch(\d+).tab/ } @sorted;

if (!defined($final)) {
    # None specified
    $final   = $rawsorted[-1];
    $initial = $rawsorted[-2];
    $final   =~ s/.*\/$branch(\d+).tab/$1/;
    $initial =~ s/.*\/$branch(\d+).tab/$1/;
} elsif ($final =~ m/(\d+):(\d+)/) {
    # Initial:Final
    $initial = $1;
    $final   = $2;
} elsif ($final =~ m/(\d+)/) {
    # Final
    $final = $1;
    ($initial) = grep { $rawsorted[$_] eq $final } 0..$#rawsorted;
    $initial = $rawsorted[$initial-1];
}

# Make $to and $from be the index rather than the revision
if (defined($to)) {
    ($to) = grep { $rawsorted[$_] eq $to } 0..$#rawsorted;
}
if (!defined($from)) {
    $from = 0;
} else {
    ($from) = grep { $rawsorted[$_] eq $from } 0..$#rawsorted;
    if (defined($to)) {
        $numrevs = $to-$from;
    }
}
if (!defined($to)) {
    $to = $#rawsorted;
}

# Constrain to $numrevs
if ($to-$from > $numrevs) {
    $from = $to-$numrevs+1;
}

#print STDERR "Branch='$branch'\n";
#print STDERR "Initial:Final = $initial:$final\n";
#print STDERR "From $from To $to\n";
#print STDERR "$rawsorted[$from] to $rawsorted[$to]\n";

@rawsorted = splice(@rawsorted,$from,$to-$from+1);
@sorted    = splice(@sorted,   $from,$to-$from+1);
@versions  = @rawsorted;

# Make final and initial indexes
($final)   = grep { $rawsorted[$_] eq $final   } 0..$#rawsorted;
($initial) = grep { $rawsorted[$_] eq $initial } 0..$#rawsorted;

#print STDERR "revision range $initial $final:\n";
#foreach my $v (@rawsorted) {
#    print STDERR "$v\n";
#}

my $mainfile = tmpnam();

if ($test != 0)
{
    $mainfile = "testfile";
} else {
    print STDERR "Processing $sorted[0]\n";
    system("cut -f1,2,7 ".shift(@sorted)." | sort -k1b,1 > $mainfile");

    foreach my $version (@sorted) {
        print STDERR "Processing $version\n";
        my $tempfile1 = tmpnam();
        my $tempfile2 = tmpnam();
        system("cut -f1,2,7 $version | sort -k1b,1 > $tempfile1");
        system("join --nocheck-order -j1 $mainfile $tempfile1 > $tempfile2");
        unlink($tempfile1);
        unlink($mainfile);
        $mainfile = $tempfile2;
    }
}

print "<HTML><HEAD><TITLE>Test results by revision</TITLE></HEAD><BODY>\n";
print "<H1>Files that changed between revisions $branch$rawsorted[$initial]";
print " and $branch$rawsorted[$final].</H1>\n";
print "<TABLE>";

my $headers = 0;
my $errors_new  = 0;
my $errors_old  = 0;
my $hashchanges = 0;

open(FH,"<$mainfile");
while (<FH>)
{
    my @fields = split;
    my $line = "<tr align=center><td align=right>";
    my $file;
    my $dev;
    my $res;
    my $band;
    
    $_ = shift(@fields);
    ($file,$dev, $res, $band) = m/(.*)\.(.*)\.(\d*)\.(\d)/;

    $line .= "$file<td>$dev<td>$res<td>$band";

    my $dull = 1;

    my $oldhash  = undef;
    my %hashes   = ();
    my $change   = 0;
    my $acc      = "";
    my $accnum   = 0;
    my $acccols  = 0;
    my $olderr   = undef;
    my $fieldnum = 0;
    my $initial_err;
    my $initial_hash;
    my $final_err;
    my $final_hash;

    while (@fields)
    {
        my $err  = shift(@fields);
        my $hash = shift(@fields);

        if ($err) {
            $hash = 0;
        }
        if ($fieldnum == $initial) {
            $initial_err  = $err;
            $initial_hash = $hash;
        } elsif ($fieldnum == $final) {
            $final_err  = $err;
            $final_hash = $hash;
        }

        if ($err == 0) {
            if (defined($oldhash) && $oldhash eq $hash && !defined($olderr)) {
                $acccols++;
            } else {
                if ($acccols != 0) {
                    $line .= $acc;
                    if ($acccols != 1) {
                        $line .= " colspan=$acccols";
                    }
                    $line .= ">$accnum";
                }
                $acccols = 1;
                if (!defined($hashes{$hash})) {
                    $hashes{$hash} = $change++;
                }
                if ($hashes{$hash} == 0) {
                    $acc = "<td bgcolor=#80ff80"; # Green
                } else {
                    $acc = "<td bgcolor=#ff8040"; # Orange
                    $dull = 0;
                }
                $accnum = $hashes{$hash};
            }
            $oldhash = $hash;
            $olderr = undef;
        } else {
            if (defined($olderr) && $olderr == $err) {
                $acccols++;
            } else {
                if ($acccols != 0) {
                    $line .= $acc;
                    if ($acccols != 1) {
                        $line .= " colspan=$acccols";
                    }
                    $line .= ">$accnum";
                }
                $acc = "<td bgcolor=#ff4040"; #Red
                $acccols = 1;
                $accnum = $err;
                $dull = 0;
            }
            $olderr = $err;
        }
        $fieldnum++;
    }
    if ($acccols != 0) {
        $line .= $acc;
        if ($acccols != 1) {
            $line .= " colspan=$acccols";
        }
        $line .= ">$accnum";
    }
    $line .= "\n";
    if (($initial_err == 0) && ($final_err != 0)) {
        $errors_new++;
    } elsif (($initial_err != 0) && ($final_err == 0)) {
        $errors_old++;
    } elsif ($initial_hash ne $final_hash) {
        $hashchanges++;
    }
    if (($initial_err == $final_err) && ($initial_hash eq $final_hash)) {
#        print STDERR "Dull\n";
        $dull = 1;
    }
    if (!$dull) {
#        print STDERR "I:$initial_err $initial_hash\n";
#        print STDERR "F:$final_err $final_hash\n";
        if ($headers-- == 0)
        {
            print "<TR bgcolor=#8080ff><TH>File<TH>Dev<TH>Res<TH>Band";
            $fieldnum = 0;
            foreach my $version (@versions)
            {
                print "<TH";
                if (($fieldnum == $initial) || ($fieldnum == $final)) {
                    print " bgcolor=#ff8080";
                }
                print ">$version";
                $fieldnum++;
            }
            print "\n";
           $headers += 50;
        }

        print $line;
    }
}
close(FH);
if ($test == 0) {
    unlink($mainfile);
    #print STDERR "$mainfile\n";
}

print "</TABLE>\n";
print "<P>$errors_new new errors.</P>\n";
print "<P>$errors_old fixed errors.</P>\n";
print "<P>$hashchanges changed images.</P>\n";
print "</BODY>";
