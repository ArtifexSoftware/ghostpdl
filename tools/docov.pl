#!/usr/bin/perl -w

# script to see if new test files increase code coverage.

# compile code with gcc options -fprofile-arcs -ftest-coverage.
# place baseline and candidate test in the relative subdirectories oldtests
# and newtests repectively.
# run this script.

use FILE::Find;
use strict;

# old and new test file locations (relative, recursive, can be sym
# links).
my $baseline_tests='oldtests';
my $testcase_candidates='newtests';

# subdir of all covered files (recursive)
my $cfiles="../../";

# hash - key is c file covered, value percent coverage.
my %baseline_coverage=();
my %new_tests_plus_baseline_coverage=();

# file to hold %baseline_coverage.
my $base="baseline.txt";

# commands to run on test files.
my @args= ( ["./pcl6", "-n", "-sDEVICE=ppmraw", "-sOutputFile=/dev/null", "-r300", "-dNOPAUSE" ],
            ["./pcl6", "-n", "-sDEVICE=pbmraw", "-sOutputFile=/dev/null", "-r300", "-dNOPAUSE" ], );


# parse gcov output
sub get_coverage_for_a_source_file  {
    my $test = shift(@_);
    my $source_file; my $percent;
    open (PIPE, "gcov $test 2>&1|") || die "gcov pipe failed $!";
    while (<PIPE>) {
        if (/^File \'(.*)\'/) { $source_file = $1; }
        if (/^Lines executed:(\d+\.\d+)\%/) { $percent = $1; }
    }
    return ($source_file, $percent);
}

sub clear_coverage {
    # yikes
    unlink glob("*.gcov");
    unlink glob("*.gcda");
}

sub print_coverage_increase {
    my $test_file = shift(@_);
    my $total = 0;
    while (my($f, $p) = each(%baseline_coverage)) {
        exists $new_tests_plus_baseline_coverage{$f} ||
            die "internal error: regenerate baseline with recompiled code\n";
        my $np = $new_tests_plus_baseline_coverage{$f};
        if ( $np > $p ) {
            print "test file: $test_file caused coverage increase for $f increased from $p to $np\n";
            $total++;
        }
    }
    print "test file: $test_file changed coverage for $total files\n";
}
    
sub proc_new_coverage {
   if (/^.*\.c\z/s) {
       my $cf = $File::Find::name;
       print "$cf\r";
       my($f, $p) = get_coverage_for_a_source_file $cf;
       $new_tests_plus_baseline_coverage{$f} = $p if (defined($f) && defined($p));
   }
}

sub proc_run_tests {
    if (-f $_) {
        my $f = $File::Find::name;
        map(print("@$_ $f\n"), @args);
        map(system(@$_, $f), @args);
    }
}

sub proc_run_new_tests {
    if (-f $_) {
        my $f = $File::Find::name;
        clear_coverage;
        map(print("@$_ $f\n"), @args);
        map(system(@$_, $f), @args);
        File::Find::find({ wanted => \&proc_new_coverage, no_chdir => 1 }, $cfiles);
        print_coverage_increase $f;
        
    }
}

sub proc_baseline_coverage {
    if (/^.*\.c\z/s) {
        print "$File::Find::name\r";
        my($s, $p) = get_coverage_for_a_source_file $File::Find::name;
        print(BASE "$s $p\n") if (defined($s) && defined($p));
    }
}
 

sub build_baseline {
    open(BASE, ">>$base");
    File::Find::find({ wanted => \&proc_run_tests, no_chdir => 1 }, $baseline_tests);
    File::Find::find({ wanted => \&proc_baseline_coverage, no_chdir => 1 }, $cfiles);
    close(BASE);
}

sub do_new_tests {
    File::Find::find({ wanted => \&proc_run_new_tests, no_chdir => 1 }, $testcase_candidates);
}

build_baseline unless (-e $base);

open(BASE, $base) || die "open (BASELINE, $base)";

# read the baseline
%baseline_coverage=();
while (<BASE>) {
    chomp;
    my ($source_file, $percent) = split(/ /);
    $baseline_coverage{$source_file} = $percent;
}

close(BASE);

do_new_tests;
