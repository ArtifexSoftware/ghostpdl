#!/usr/bin/perl

use strict;
use warnings;
use File::Find;

my @testdirs=("/home/marcos/cluster/tests","/home/marcos/cluster/tests_private");
my @devices = ("pdfwrite", "ps2write", "ppmraw");
my @testfiles = ();
sub process_file {
    if (!-f $_) {
	return;
    }
    if ($_ =~ m/\.pdf$/) {
	push @testfiles, $_;
    }
    if ($_ =~ m/\.ps$/) {
	push @testfiles, $_;
    }
    if ($_ =~ m/\.PS$/) {
	push @testfiles, $_;
    }
}

find({ wanted=>\&process_file, no_chdir => 1}, @testdirs);

sub system_bash($) {
    my $cmd = shift;
    my @args = ( "bash", "-c", $cmd);
    my $rc = system(@args);

    if ($rc == -1) {
	print "Failed to execute: $!\n";
    }
    elsif ($rc & 127) {
	if ($rc == 2) {
	    die "You keel me!";
	}
	printf "child died with signal %d, %s coredump\n", ($? & 127), ($? & 128) ? 'with' : 'without';
    }
    return $rc;
}

foreach my $testfile (@testfiles) {
    foreach my $dev (@devices) {

	printf("$testfile to $dev\n");
	`rm -f outfile.* stdout.* stderr.*`;

	my $rc = system_bash("./bin/apitest -sDEVICE=$dev -o outfile.%d. -r72 $testfile");
	if ($rc) {
	    printf("Failed with return code $rc\n");
	    next;
	}

        my $grep = "-av BOGOSITY10000";
	if ($testfile =~ m/.pdf$/) {
	    $grep = '-av "\(/ID\|uuid\|CreationDate\|ModDate\|CreateDate\)"';
	}
	if ($testfile =~ m/.ps$/) {
	    $grep = "-av CreationDate";
	}
	if ($testfile =~ m/.PS$/) {
	    $grep = "-av CreationDate";
	}

	my $fail = 0;
	for (my $page=1; -e "outfile.$page.0"; $page++) {
	    my $diffcount=0;
            for (my $thrd=1; -e "outfile.$page.$thrd"; $thrd++) {
		my $cmd = "diff -q <( grep $grep outfile.$page.0 ) <( grep $grep outfile.$page.$thrd )";
                my $diff1=system_bash($cmd);
		if ($diff1) {
		    $diffcount++;
		}
	    }
	    if ($diffcount) {
		printf("Page $page differs $diffcount times\n");
		$fail = 1;
	    }
	}
	if ($fail) {
	    next;
	}

	my $diffcount=0;
        for (my $thrd=1; -e "stdout.$thrd"; $thrd++) {
            my $cmd = "diff -q <( grep -av Loading stdout.0) <( grep -av Loading stdout.1)";
            my $diff1=system_bash($cmd);
            if ($diff1) {
                $diffcount++;
            }
	}
	if ($diffcount) {
	    printf("Stdout differs $diffcount times\n");
	    next;
	}

	$diffcount=0;
        for (my $thrd=1; -e "stderr.$thrd"; $thrd++) {
            my $cmd = "diff -q stderr.0 stderr.1";
            my $diff1=system_bash($cmd);
            if ($diff1) {
                $diffcount++;
            }
	}
	if ($diffcount) {
	    printf("Stderr differs $diffcount times\n");
	    next;
	}
    }
}

